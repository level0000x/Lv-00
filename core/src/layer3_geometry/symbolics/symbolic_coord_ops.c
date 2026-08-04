/**
 * @file symbolic_coord_ops.c
 * @brief SymbolicCoord 算术运算操作：加法/减法/乘法/除法
 *
 * @details 实现 RATIONAL / QUADRATIC / ALGEBRAIC / TRANSCENDENTAL
 *          四种符号坐标类型间的混合运算。核心操作：
 *
 *          - symbolic_coord_add / sub / mul / div: 类型分派加法/减法/乘法/除法
 *
 *          类型提升规则（结果类型选择）：
 *          RATIONAL * RATIONAL → RATIONAL（若分母不溢出）
 *          RATIONAL * QUADRATIC → QUADRATIC
 *          RATIONAL * ALGEBRAIC → ALGEBRAIC
 *          任何类型 * TRANSCENDENTAL → TRANSCENDENTAL
 *          QUADRATIC ± QUADRATIC → QUADRATIC（√n 相同则合并，否则回退为 double）
 *
 *          位电路熔断：当位宽超过 BIT_CUTOFF_THRESHOLD 时触发熔断信号，
 *          交由 overflow_context 处理（忽略/回滚/降级）。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv/lv_platform.h"
#include "lv/lv_mempool_utils.h"

#include <float.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/bit_burning.h"
#include "lv/constraint_graph.h"
#include "lv/symbolic_coord.h"

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "mpz_poly.h"

#include "lv/coeff_pool.h"

/* ── 多项式系数内存池（拥有权：本文件定义，其余文件共享，实现见 lv/coeff_pool.h）── */
lvMemPool *g_coeff_pool = NULL;

#define SYM_COORD_DYNAMIC_ARRAY_INIT_CAP 16
#define SYM_COORD_SIGFIGS_MIN_SAFE 6
#define SYM_COORD_SIGFIGS_APPROX 4
#define SYM_COORD_MAX_REFINE 15
#define SYM_COORD_AMB_MIN_SIGFIGS 3
#define COORD_SEVEN_OVER_FIVE_N 32

/* ── 前向声明 ── */
double algebraic_to_double(const Algebraic *a);
double quadratic_to_double(const Quadratic *q);
double transcendental_to_double(const Transcendental *t);
bool is_rational_zero(const Rational *r);
void refine_algebraic_bounds(Algebraic *a, int iterations);
char *algebraic_serialize(const Algebraic *a);
char *quadratic_serialize(const Quadratic *q);
int remove_square_factors(int n);

/* 降级检查函数（定义于 symbolic_coord_trust.c） */
SymbolicCoord *_symbolic_coord_degrade_check_algebraic(SymbolicCoord *result);

/* 前向声明：定义于本文件稍后位置的 static 辅助函数 */
static size_t rational_total_bits(const Rational *r);
static void bit_burning_check_result(SymbolicCoord *result, const char *operation);
static SymbolicCoord *quadratic_to_algebraic(const SymbolicCoord *q);

/* ── 外部溢出上下文 ── */
extern lv_THREAD_LOCAL struct OverflowContext g_overflow_context;

/* ── 共享内部头文件：CoordOpsVTable 定义 ── */
#include "symbolic_coord_internal.h"

/* ── Per-type same-operation handlers for vtable dispatch ── */

/* RATIONAL handlers */
static SymbolicCoord *same_type_add_rational(const SymbolicCoord *a, const SymbolicCoord *b) {
    Rational *r = rational_add(a->data.rational, b->data.rational);
    if (!r)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "symbolic_coord_add: rational_add failed");
    SymbolicCoord *result = symbolic_coord_create_rational(0, 1);
    if (!result) {
        rational_destroy(r);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "symbolic_coord_add: result allocation failed");
    }
    rational_destroy(result->data.rational);
    result->data.rational = r;
    result->trust = (a->trust < b->trust) ? a->trust : b->trust;
    bit_burning_check_result(result, "add");
    return result;
}

static SymbolicCoord *same_type_subtract_rational(const SymbolicCoord *a, const SymbolicCoord *b) {
    Rational *r = rational_subtract(a->data.rational, b->data.rational);
    if (!r)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "symbolic_coord_subtract: rational_subtract failed");
    SymbolicCoord *result = symbolic_coord_create_rational(0, 1);
    if (!result) {
        rational_destroy(r);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "symbolic_coord_subtract: result allocation failed");
    }
    rational_destroy(result->data.rational);
    result->data.rational = r;
    result->trust = (a->trust < b->trust) ? a->trust : b->trust;
    bit_burning_check_result(result, "subtract");
    return result;
}

static SymbolicCoord *same_type_multiply_rational(const SymbolicCoord *a, const SymbolicCoord *b) {
    Rational *r = rational_multiply(a->data.rational, b->data.rational);
    if (!r)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "symbolic_coord_multiply: rational_multiply failed");
    SymbolicCoord *result = symbolic_coord_create_rational(0, 1);
    if (!result) {
        rational_destroy(r);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "symbolic_coord_multiply: result allocation failed");
    }
    rational_destroy(result->data.rational);
    result->data.rational = r;
    result->trust = (a->trust < b->trust) ? a->trust : b->trust;
    bit_burning_check_result(result, "multiply");
    return result;
}

static SymbolicCoord *same_type_divide_rational(const SymbolicCoord *a, const SymbolicCoord *b) {
    Rational *r = rational_divide(a->data.rational, b->data.rational);
    if (!r)
        return NULL;
    SymbolicCoord *result = symbolic_coord_create_rational(0, 1);
    if (!result) {
        rational_destroy(r);
        return NULL;
    }
    rational_destroy(result->data.rational);
    result->data.rational = r;
    result->trust = (a->trust < b->trust) ? a->trust : b->trust;
    bit_burning_check_result(result, "divide");
    return result;
}

static size_t bit_burning_bits_rational(const SymbolicCoord *c) {
    if (!c->data.rational)
        return 0;
    size_t total = rational_total_bits(c->data.rational);
    if (total == SIZE_MAX)
        return SIZE_MAX;
    return total;
}

/* ALGEBRAIC handlers */
static SymbolicCoord *same_type_add_algebraic(const SymbolicCoord *a, const SymbolicCoord *b) {
    Algebraic *alg = algebraic_add(a->data.algebraic, b->data.algebraic);
    if (!alg)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "symbolic_coord_add: algebraic_add failed");
    SymbolicCoord *result =
        symbolic_coord_create_algebraic(&alg->minimal_poly, alg->left_bound, alg->right_bound);
    algebraic_destroy(alg);
    if (result)
        result->trust = (a->trust < b->trust) ? a->trust : b->trust;
    return _symbolic_coord_degrade_check_algebraic(result);
}

static SymbolicCoord *same_type_subtract_algebraic(const SymbolicCoord *a, const SymbolicCoord *b) {
    Algebraic *alg = algebraic_subtract(a->data.algebraic, b->data.algebraic);
    if (!alg)
        return NULL;
    SymbolicCoord *result =
        symbolic_coord_create_algebraic(&alg->minimal_poly, alg->left_bound, alg->right_bound);
    algebraic_destroy(alg);
    if (result)
        result->trust = (a->trust < b->trust) ? a->trust : b->trust;
    return _symbolic_coord_degrade_check_algebraic(result);
}

static SymbolicCoord *same_type_multiply_algebraic(const SymbolicCoord *a, const SymbolicCoord *b) {
    Algebraic *alg = algebraic_multiply(a->data.algebraic, b->data.algebraic);
    if (!alg)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "symbolic_coord_multiply: algebraic_multiply failed");
    SymbolicCoord *result =
        symbolic_coord_create_algebraic(&alg->minimal_poly, alg->left_bound, alg->right_bound);
    algebraic_destroy(alg);
    if (result)
        result->trust = (a->trust < b->trust) ? a->trust : b->trust;
    return _symbolic_coord_degrade_check_algebraic(result);
}

static SymbolicCoord *same_type_divide_algebraic(const SymbolicCoord *a, const SymbolicCoord *b) {
    Algebraic *alg = algebraic_divide(a->data.algebraic, b->data.algebraic);
    if (!alg)
        return NULL;
    SymbolicCoord *result =
        symbolic_coord_create_algebraic(&alg->minimal_poly, alg->left_bound, alg->right_bound);
    algebraic_destroy(alg);
    if (result)
        result->trust = (a->trust < b->trust) ? a->trust : b->trust;
    return _symbolic_coord_degrade_check_algebraic(result);
}

static size_t bit_burning_bits_algebraic(const SymbolicCoord *c) {
    if (!c->data.algebraic)
        return 0;
    size_t total = 0;
    if (c->data.algebraic->cached_rational) {
        size_t rb = rational_total_bits(c->data.algebraic->cached_rational);
        if (rb == SIZE_MAX)
            return SIZE_MAX;
        total += rb;
    }
    for (int i = 0; i <= c->data.algebraic->minimal_poly.degree; i++) {
        size_t cb = (size_t) mpz_sizeinbase(c->data.algebraic->minimal_poly.coeffs[i], 2);
        if (cb > (size_t) BIT_CUTOFF_THRESHOLD)
            return SIZE_MAX;
        total += cb;
    }
    return total;
}

/* QUADRATIC handlers */
static SymbolicCoord *same_type_add_quadratic(const SymbolicCoord *a, const SymbolicCoord *b) {
    Quadratic *q = quadratic_add(a->data.quadratic, b->data.quadratic);
    if (!q) {
        SymbolicCoord *a_alg = quadratic_to_algebraic(a);
        SymbolicCoord *b_alg = quadratic_to_algebraic(b);
        if (!a_alg || !b_alg) {
            if (a_alg)
                symbolic_coord_destroy(a_alg);
            if (b_alg)
                symbolic_coord_destroy(b_alg);
            return NULL;
        }
        SymbolicCoord *result = symbolic_coord_add(a_alg, b_alg);
        symbolic_coord_destroy(a_alg);
        symbolic_coord_destroy(b_alg);
        return result;
    }
    SymbolicCoord *result = symbolic_coord_create_quadratic(q->a, q->b, q->n);
    lv_free((void **) &q);
    if (result)
        result->trust = (a->trust < b->trust) ? a->trust : b->trust;
    return result;
}

static SymbolicCoord *same_type_subtract_quadratic(const SymbolicCoord *a, const SymbolicCoord *b) {
    Quadratic *q = quadratic_subtract(a->data.quadratic, b->data.quadratic);
    if (!q) {
        SymbolicCoord *a_alg = quadratic_to_algebraic(a);
        SymbolicCoord *b_alg = quadratic_to_algebraic(b);
        if (!a_alg || !b_alg) {
            if (a_alg)
                symbolic_coord_destroy(a_alg);
            if (b_alg)
                symbolic_coord_destroy(b_alg);
            return NULL;
        }
        SymbolicCoord *result = symbolic_coord_subtract(a_alg, b_alg);
        symbolic_coord_destroy(a_alg);
        symbolic_coord_destroy(b_alg);
        return result;
    }
    SymbolicCoord *result = symbolic_coord_create_quadratic(q->a, q->b, q->n);
    lv_free((void **) &q);
    if (result)
        result->trust = (a->trust < b->trust) ? a->trust : b->trust;
    return result;
}

static SymbolicCoord *same_type_multiply_quadratic(const SymbolicCoord *a, const SymbolicCoord *b) {
    Quadratic *q = quadratic_multiply(a->data.quadratic, b->data.quadratic);
    if (!q) {
        SymbolicCoord *a_alg = quadratic_to_algebraic(a);
        SymbolicCoord *b_alg = quadratic_to_algebraic(b);
        if (!a_alg || !b_alg) {
            if (a_alg)
                symbolic_coord_destroy(a_alg);
            if (b_alg)
                symbolic_coord_destroy(b_alg);
            return NULL;
        }
        SymbolicCoord *result = symbolic_coord_multiply(a_alg, b_alg);
        symbolic_coord_destroy(a_alg);
        symbolic_coord_destroy(b_alg);
        return result;
    }
    SymbolicCoord *result = symbolic_coord_create_quadratic(q->a, q->b, q->n);
    lv_free((void **) &q);
    if (result)
        result->trust = (a->trust < b->trust) ? a->trust : b->trust;
    return result;
}

static SymbolicCoord *same_type_divide_quadratic(const SymbolicCoord *a, const SymbolicCoord *b) {
    Quadratic *q = quadratic_divide(a->data.quadratic, b->data.quadratic);
    if (!q) {
        SymbolicCoord *a_alg = quadratic_to_algebraic(a);
        SymbolicCoord *b_alg = quadratic_to_algebraic(b);
        if (!a_alg || !b_alg) {
            if (a_alg)
                symbolic_coord_destroy(a_alg);
            if (b_alg)
                symbolic_coord_destroy(b_alg);
            return NULL;
        }
        SymbolicCoord *result = symbolic_coord_divide(a_alg, b_alg);
        symbolic_coord_destroy(a_alg);
        symbolic_coord_destroy(b_alg);
        return result;
    }
    SymbolicCoord *result = symbolic_coord_create_quadratic(q->a, q->b, q->n);
    lv_free((void **) &q);
    if (result)
        result->trust = (a->trust < b->trust) ? a->trust : b->trust;
    return result;
}

static size_t bit_burning_bits_quadratic(const SymbolicCoord *c) {
    if (!c->data.quadratic)
        return 0;
    size_t rb_a = rational_total_bits(c->data.quadratic->a);
    size_t rb_b = rational_total_bits(c->data.quadratic->b);
    if (rb_a == SIZE_MAX || rb_b == SIZE_MAX)
        return SIZE_MAX;
    return rb_a + rb_b;
}

/* ── Per-type compare handlers ── */
static int same_type_compare_rational(const SymbolicCoord *a, const SymbolicCoord *b) {
    return rational_compare(a->data.rational, b->data.rational);
}
static int same_type_compare_algebraic(const SymbolicCoord *a, const SymbolicCoord *b) {
    return algebraic_compare(a->data.algebraic, b->data.algebraic);
}
static int same_type_compare_quadratic(const SymbolicCoord *a, const SymbolicCoord *b) {
    return quadratic_compare(a->data.quadratic, b->data.quadratic);
}
static int same_type_compare_transcendental(const SymbolicCoord *a, const SymbolicCoord *b) {
    return transcendental_compare(a->data.transcendental, b->data.transcendental);
}

/* ── Per-type to_double handlers ── */
static double to_double_rational(const SymbolicCoord *c) {
    return rational_to_double(c->data.rational);
}
static double to_double_algebraic(const SymbolicCoord *c) {
    return algebraic_to_double(c->data.algebraic);
}
static double to_double_quadratic(const SymbolicCoord *c) {
    return quadratic_to_double(c->data.quadratic);
}
static double to_double_transcendental(const SymbolicCoord *c) {
    return transcendental_to_double(c->data.transcendental);
}

/* ── vtable 数组（extern 声明于 symbolic_coord_internal.h） ── */
const CoordOpsVTable kCoordOpsVTable[] = {
    [RATIONAL] = {
        .add = same_type_add_rational,
        .subtract = same_type_subtract_rational,
        .multiply = same_type_multiply_rational,
        .divide = same_type_divide_rational,
        .compare = same_type_compare_rational,
        .to_double = to_double_rational,
        .bit_burning_bits = bit_burning_bits_rational,
        .destroy = destroy_rational,
        .serialize = serialize_rational,
        .copy_data = copy_data_rational,
        .copy_check = copy_check_rational,
        .is_zero = is_zero_rational,
    },
    [ALGEBRAIC] = {
        .add = same_type_add_algebraic,
        .subtract = same_type_subtract_algebraic,
        .multiply = same_type_multiply_algebraic,
        .divide = same_type_divide_algebraic,
        .compare = same_type_compare_algebraic,
        .to_double = to_double_algebraic,
        .bit_burning_bits = bit_burning_bits_algebraic,
        .destroy = destroy_algebraic,
        .serialize = serialize_algebraic,
        .copy_data = copy_data_algebraic,
        .copy_check = copy_check_algebraic,
        .is_zero = is_zero_algebraic,
    },
    [QUADRATIC] = {
        .add = same_type_add_quadratic,
        .subtract = same_type_subtract_quadratic,
        .multiply = same_type_multiply_quadratic,
        .divide = same_type_divide_quadratic,
        .compare = same_type_compare_quadratic,
        .to_double = to_double_quadratic,
        .bit_burning_bits = bit_burning_bits_quadratic,
        .destroy = destroy_quadratic,
        .serialize = serialize_quadratic,
        .copy_data = copy_data_quadratic,
        .copy_check = copy_check_quadratic,
        .is_zero = is_zero_quadratic,
    },
    [TRANSCENDENTAL] = {
        .add = NULL,
        .subtract = NULL,
        .multiply = NULL,
        .divide = NULL,
        .compare = same_type_compare_transcendental,
        .to_double = to_double_transcendental,
        .bit_burning_bits = NULL,
        .destroy = destroy_transcendental,
        .serialize = serialize_transcendental,
        .copy_data = copy_data_transcendental,
        .copy_check = copy_check_transcendental,
        .is_zero = is_zero_transcendental,
    },
};

/* ── 位数熔断辅助函数 ── */

/**
 * @brief 获取有理数总位数（分子+分母），含溢出保护
 */
static size_t rational_total_bits(const Rational *r) {
    if (!r)
        return 0;
    size_t num = mpz_sizeinbase(mpq_numref(r->value), 2);
    size_t den = mpz_sizeinbase(mpq_denref(r->value), 2);
    if (num > SIZE_MAX - den)
        return SIZE_MAX;
    return num + den;
}

/**
 * @brief 检查 SymbolicCoord 结果的位数是否超过熔断阈值。
 */
static void bit_burning_check_result(SymbolicCoord *result, const char *operation) {
    if (!result)
        return;

    (void) operation;

    if (result->type == TRANSCENDENTAL)
        return;

    const CoordOpsVTable *vt = &kCoordOpsVTable[result->type];
    size_t total_bits = vt->bit_burning_bits(result);
    if (total_bits == SIZE_MAX || total_bits > (size_t) BIT_CUTOFF_THRESHOLD) {
        BitBurningState *state = bit_burning_get_global_state();
        bit_burning_check(total_bits > (size_t) BIT_CUTOFF_THRESHOLD ? total_bits : BIT_CUTOFF_THRESHOLD, state);
        result->trust = TRUST_AMBER;
    }
}

/* ── 安全解析辅助 ── */

/**
 * @brief 安全地将字符串解析为 int64_t。
 */
static int64_t safe_atol(const char *str) {
    if (!str || !*str)
        return 0;
    char *end = NULL;
    errno = 0;
    long val = strtol(str, &end, 10);
    if (errno != 0 || end == str)
        return 0;
    return (int64_t) val;
}

/* ── 超越数转 double 辅助 ── */

/* ================================================================
 * 表达式类型 -> 运算语义 静态查找表（数据表化，替代 switch）
 * ================================================================ */

/** @brief 超越数表达式运算语义 */
typedef enum {
    TRANS_OP_UNKNOWN = 0, /**< 未知语义（沿用名称解析回退路径） */
    TRANS_OP_ADD,         /**< 加法：base + 有理数 */
    TRANS_OP_MUL,         /**< 乘法：base * 有理数 */
} TransOpKind;

/** @brief 表达式类型 -> 运算语义 查找表 */
static const TransOpKind s_trans_expr_op_kind[] = {
    [TRANS_EXPR_ADD_RATIONAL] = TRANS_OP_ADD,
    [TRANS_EXPR_MUL_RATIONAL] = TRANS_OP_MUL,
    [TRANS_EXPR_ADD_ALGEBRAIC] = TRANS_OP_UNKNOWN,
    [TRANS_EXPR_MUL_ALGEBRAIC] = TRANS_OP_UNKNOWN,
};

/**
 * 将超越数转换为 double 近似值。
 */
static double transcendental_expr_to_double(const Transcendental *t) {
    if (!t)
        return 0.0;

    const char *base = t->expr ? t->expr->base_name : t->name;
    double base_val;
    if (strcmp(base, "pi") == 0) {
        base_val = M_PI;
    } else if (strcmp(base, "e") == 0) {
        base_val = M_E;
    } else {
        return 0.0;
    }

    if (!t->expr) {
        return base_val;
    }

    if (t->expr->rational_operand) {
        double rat_val = rational_to_double(t->expr->rational_operand);
        /* 查表获取运算语义；仅 ADD/MUL_RATIONAL 有定义，其余继续走回退路径（原 default break） */
        TransOpKind op = TRANS_OP_UNKNOWN;
        if ((unsigned) t->expr->expr_type < lv_ARRAY_SIZE(s_trans_expr_op_kind))
            op = s_trans_expr_op_kind[t->expr->expr_type];
        if (op == TRANS_OP_MUL)
            return base_val * rat_val;
        if (op == TRANS_OP_ADD)
            return base_val + rat_val;
    }

    if (t->expr->expr_type == TRANS_EXPR_ADD_ALGEBRAIC || t->expr->expr_type == TRANS_EXPR_MUL_ALGEBRAIC) {
        return base_val;
    }

    {
        const char *name = t->name;
        char *star_pos = strstr(name, "*pi");
        if (star_pos) {
            int64_t coeff_num = 1;
            int64_t coeff_den = 1;
            if (star_pos == name + 1 && name[0] != '-') {
                coeff_num = safe_atol(name);
            } else if (star_pos == name + 2 && name[0] == '-') {
                coeff_num = safe_atol(name);
            }
            const char *after = star_pos + 3;
            if (*after == '/') {
                coeff_den = safe_atol(after + 1);
            }
            if (coeff_den > 0) {
                return M_PI * (double) coeff_num / (double) coeff_den;
            }
        }

        if (strncmp(name, "pi/", 3) == 0) {
            int64_t den = safe_atol(name + 3);
            if (den > 0)
                return M_PI / (double) den;
        }

        if (strncmp(name, "-pi/", 4) == 0) {
            int64_t den = safe_atol(name + 4);
            if (den > 0)
                return -M_PI / (double) den;
        }

        if (name[0] == '-' && strstr(name, "*pi")) {
            char *sp = strstr(name, "*pi");
            int64_t coeff_num = safe_atol(name);
            int64_t coeff_den = 1;
            const char *after = sp + 3;
            if (*after == '/') {
                coeff_den = safe_atol(after + 1);
            }
            if (coeff_den > 0) {
                return M_PI * (double) coeff_num / (double) coeff_den;
            }
        }
    }

    return 0.0;
}

/* ============================================================
 * Type Promotion Functions
 * ============================================================ */

/**
 * 将有理数类型的符号坐标提升为代数数。
 */
static SymbolicCoord *rational_to_algebraic(const SymbolicCoord *r) {
    Algebraic *alg = algebraic_from_rational(r->data.rational);
    if (!alg)
        return NULL;

    SymbolicCoord *result = lv_calloc(1, sizeof(SymbolicCoord));
    if (!result) {
        algebraic_destroy(alg);
        return NULL;
    }
    result->type = ALGEBRAIC;
    result->trust = r->trust;
    result->data.algebraic = alg;
    return result;
}

/**
 * 将有理数提升为指定 n 值的二次根式。
 */
static SymbolicCoord *rational_to_quadratic_with_n(const SymbolicCoord *r, unsigned int n) {
    Rational *a = rational_copy(r->data.rational);
    Rational *b = rational_create(0, 1);

    SymbolicCoord *result = symbolic_coord_create_quadratic(a, b, n);
    if (result) {
        result->trust = r->trust;
    } else {
        rational_destroy(a);
        rational_destroy(b);
    }
    return result;
}

static SymbolicCoord *rational_to_quadratic(const SymbolicCoord *r) {
    return rational_to_quadratic_with_n(r, 1);
}

/* Promote Quadratic to Algebraic */
static SymbolicCoord *quadratic_to_algebraic(const SymbolicCoord *q) {
    Algebraic *alg = algebraic_from_quadratic(q->data.quadratic);
    if (!alg)
        return NULL;

    SymbolicCoord *result = lv_calloc(1, sizeof(SymbolicCoord));
    if (!result) {
        algebraic_destroy(alg);
        return NULL;
    }
    result->type = ALGEBRAIC;
    result->trust = q->trust;
    result->data.algebraic = alg;
    return result;
}

/* ============================================================
 * Cross-type 类型提升查找表（数据表化，替代逐对 if 分支）
 * ============================================================ */

/** @brief 类型提升等级：等级越高表达能力越强，cross-type 运算提升低等级操作数 */
static const int kCoordPromotionRank[] = {
    [RATIONAL] = 0,
    [QUADRATIC] = 1,
    [ALGEBRAIC] = 2,
};

/** @brief 提升函数签名：将 c 提升为高等级类型；quad_n 仅 RATIONAL→QUADRATIC 提升时使用 */
typedef SymbolicCoord *(*CoordPromoteFn)(const SymbolicCoord *c, unsigned int quad_n);

static SymbolicCoord *promote_rational_to_quadratic(const SymbolicCoord *c, unsigned int quad_n) {
    return rational_to_quadratic_with_n(c, quad_n);
}

static SymbolicCoord *promote_rational_to_algebraic(const SymbolicCoord *c, unsigned int quad_n) {
    (void) quad_n;
    return rational_to_algebraic(c);
}

static SymbolicCoord *promote_quadratic_to_algebraic(const SymbolicCoord *c, unsigned int quad_n) {
    (void) quad_n;
    return quadratic_to_algebraic(c);
}

/** @brief 类型提升函数表 [来源类型][目标类型] → 提升函数；不可提升组合为 NULL */
static const CoordPromoteFn kCoordPromoteFn[4][4] = {
    [RATIONAL][QUADRATIC] = promote_rational_to_quadratic,
    [RATIONAL][ALGEBRAIC] = promote_rational_to_algebraic,
    [QUADRATIC][ALGEBRAIC] = promote_quadratic_to_algebraic,
};

/**
 * @brief 统一 cross-type 提升分派：将低等级操作数提升为高等级类型后递归执行同类型运算
 *
 * 仅处理 RATIONAL / QUADRATIC / ALGEBRAIC 之间等级不同的两两组合；
 * 同类型、含 TRANSCENDENTAL 或不可提升组合返回 NULL。
 *
 * @param op 递归入口（symbolic_coord_add / subtract / multiply / divide）
 */
static SymbolicCoord *promote_cross_type_dispatch(const SymbolicCoord *a, const SymbolicCoord *b,
                                                  SymbolicCoord *(*op)(const SymbolicCoord *, const SymbolicCoord *)) {
    int rank_a = ((unsigned) a->type < lv_ARRAY_SIZE(kCoordPromotionRank)) ? kCoordPromotionRank[a->type] : -1;
    int rank_b = ((unsigned) b->type < lv_ARRAY_SIZE(kCoordPromotionRank)) ? kCoordPromotionRank[b->type] : -1;
    if (rank_a < 0 || rank_b < 0 || rank_a == rank_b)
        return NULL; /* 同类型或含 TRANSCENDENTAL：无需提升 */
    const SymbolicCoord *low = (rank_a < rank_b) ? a : b;
    const SymbolicCoord *high = (rank_a < rank_b) ? b : a;
    /* 查表获取提升函数（带边界检查） */
    CoordPromoteFn fn = NULL;
    if ((unsigned) low->type < lv_ARRAY_SIZE(kCoordPromoteFn) &&
        (unsigned) high->type < lv_ARRAY_SIZE(kCoordPromoteFn[0]))
        fn = kCoordPromoteFn[low->type][high->type];
    if (!fn)
        return NULL;
    unsigned int quad_n = (high->type == QUADRATIC) ? high->data.quadratic->n : 0;
    SymbolicCoord *promoted = fn(low, quad_n);
    if (!promoted)
        return NULL;
    SymbolicCoord *result = (low == a) ? op(promoted, b) : op(a, promoted);
    symbolic_coord_destroy(promoted);
    return result;
}

/* ============================================================
 * Cross-type Arithmetic Operations
 * ============================================================ */

/**
 * 符号坐标加法：计算 a + b。
 */
SymbolicCoord *symbolic_coord_add(const SymbolicCoord *a, const SymbolicCoord *b) {
    if (!a || !b)
        return NULL;
    g_overflow_context.left_type = a->type;
    g_overflow_context.right_type = b->type;
    g_overflow_context.last_operation = "add";

    /* Transcendental + anything */
    if (a->type == TRANSCENDENTAL || b->type == TRANSCENDENTAL) {
        const SymbolicCoord *trans_coord = (a->type == TRANSCENDENTAL) ? a : b;
        const SymbolicCoord *other_coord = (a->type == TRANSCENDENTAL) ? b : a;

        if (other_coord->type == RATIONAL) {
            Transcendental *t = transcendental_create(trans_coord->data.transcendental->name);
            if (!t)
                lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "symbolic_coord_add: transcendental_create failed");

            TranscendentalExpr *expr = lv_calloc(1, sizeof(TranscendentalExpr));
            if (!expr) {
                transcendental_destroy(t);
                return NULL;
            }
            expr->expr_type = TRANS_EXPR_ADD_RATIONAL;
            lv_strlcpy(expr->base_name, trans_coord->data.transcendental->name, sizeof(expr->base_name));
            expr->rational_operand = rational_copy(other_coord->data.rational);
            expr->out_of_scope = false;

            t->expr = expr;

            SymbolicCoord *result = lv_calloc(1, sizeof(SymbolicCoord));
            if (!result) {
                transcendental_destroy(t);
                return NULL;
            }
            result->type = TRANSCENDENTAL;
            result->trust = trans_coord->trust;
            result->data.transcendental = t;
            return result;
        }

        if (other_coord->type == ALGEBRAIC || other_coord->type == QUADRATIC) {
            Transcendental *t = transcendental_create(trans_coord->data.transcendental->name);
            if (!t)
                return NULL;

            TranscendentalExpr *expr = lv_calloc(1, sizeof(TranscendentalExpr));
            if (!expr) {
                transcendental_destroy(t);
                return NULL;
            }
            expr->expr_type = TRANS_EXPR_ADD_ALGEBRAIC;
            lv_strlcpy(expr->base_name, trans_coord->data.transcendental->name, sizeof(expr->base_name));
            expr->rational_operand = NULL;
            expr->out_of_scope = true;

            t->expr = expr;

            SymbolicCoord *result = lv_calloc(1, sizeof(SymbolicCoord));
            if (!result) {
                transcendental_destroy(t);
                return NULL;
            }
            result->type = TRANSCENDENTAL;
            result->trust = TRUST_AMBER;
            result->data.transcendental = t;
            return result;
        }

        {
            const Transcendental *ta = a->data.transcendental;
            const Transcendental *tb = b->data.transcendental;
            const char *base_a = ta->expr ? ta->expr->base_name : ta->name;
            const char *base_b = tb->expr ? tb->expr->base_name : tb->name;

            Transcendental *t = transcendental_create(base_a);
            if (!t)
                return NULL;

            TranscendentalExpr *expr = lv_calloc(1, sizeof(TranscendentalExpr));
            if (!expr) {
                transcendental_destroy(t);
                return NULL;
            }
            expr->out_of_scope = false;
            lv_strlcpy(expr->base_name, base_a, sizeof(expr->base_name));
            if (strcmp(base_a, base_b) == 0) {
                bool a_is_mul = !ta->expr || ta->expr->expr_type == TRANS_EXPR_MUL_RATIONAL;
                bool b_is_mul = !tb->expr || tb->expr->expr_type == TRANS_EXPR_MUL_RATIONAL;

                if (a_is_mul && b_is_mul) {
                    Rational *rat_a =
                        (ta->expr && ta->expr->rational_operand) ? ta->expr->rational_operand : rational_create(1, 1);
                    Rational *rat_b =
                        (tb->expr && tb->expr->rational_operand) ? tb->expr->rational_operand : rational_create(1, 1);
                    Rational *own_a = (!ta->expr) ? rat_a : NULL;
                    Rational *own_b = (!tb->expr) ? rat_b : NULL;

                    expr->expr_type = TRANS_EXPR_MUL_RATIONAL;
                    expr->rational_operand = rational_add(rat_a, rat_b);

                    if (own_a)
                        rational_destroy(own_a);
                    if (own_b)
                        rational_destroy(own_b);

                    if (!expr->rational_operand) {
                        lv_free((void **) &expr);
                        transcendental_destroy(t);
                        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "symbolic_coord_add: rational_operand allocation failed");
                    }
                } else {
                    expr->expr_type = TRANS_EXPR_ADD_ALGEBRAIC;
                    expr->rational_operand = NULL;
                    expr->out_of_scope = true;
                }
            } else {
                expr->expr_type = TRANS_EXPR_ADD_ALGEBRAIC;
                expr->rational_operand = NULL;
                expr->out_of_scope = true;
            }

            t->expr = expr;

            SymbolicCoord *result = lv_calloc(1, sizeof(SymbolicCoord));
            if (!result) {
                transcendental_destroy(t);
                return NULL;
            }
            result->type = TRANSCENDENTAL;
            result->trust = expr->out_of_scope ? TRUST_AMBER : TRUST_BLUE_UNEXPLORED;
            result->data.transcendental = t;
            return result;
        }
    }

    /* Same type operations: vtable dispatch */
    if (a->type == b->type) {
        return kCoordOpsVTable[a->type].add(a, b);
    }

    /* Cross-type operations：统一提升分派（先提升低 rank 操作数，再递归同类型运算） */
    return promote_cross_type_dispatch(a, b, symbolic_coord_add);
}

SymbolicCoord *symbolic_coord_subtract(const SymbolicCoord *a, const SymbolicCoord *b) {
    if (!a || !b)
        return NULL;
    g_overflow_context.left_type = a->type;
    g_overflow_context.right_type = b->type;
    g_overflow_context.last_operation = "subtract";

    /* Transcendental - anything */
    if (a->type == TRANSCENDENTAL || b->type == TRANSCENDENTAL) {
        const SymbolicCoord *trans_coord = (a->type == TRANSCENDENTAL) ? a : b;
        const SymbolicCoord *other_coord = (a->type == TRANSCENDENTAL) ? b : a;
        bool inverted = (b->type == TRANSCENDENTAL);

        if (other_coord->type == RATIONAL) {
            if (inverted) {
                Transcendental *t = transcendental_create(trans_coord->data.transcendental->name);
                if (!t)
                    return NULL;

                TranscendentalExpr *expr = lv_calloc(1, sizeof(TranscendentalExpr));
                if (!expr) {
                    transcendental_destroy(t);
                    return NULL;
                }
                expr->expr_type = TRANS_EXPR_ADD_ALGEBRAIC;
                lv_strlcpy(expr->base_name, trans_coord->data.transcendental->name, sizeof(expr->base_name));
                expr->rational_operand = NULL;
                expr->out_of_scope = true;

                t->expr = expr;

                SymbolicCoord *result = lv_calloc(1, sizeof(SymbolicCoord));
                if (!result) {
                    transcendental_destroy(t);
                    return NULL;
                }
                result->type = TRANSCENDENTAL;
                result->trust = TRUST_AMBER;
                result->data.transcendental = t;
                return result;
            }

            Transcendental *t = transcendental_create(trans_coord->data.transcendental->name);
            if (!t)
                return NULL;

            TranscendentalExpr *expr = lv_calloc(1, sizeof(TranscendentalExpr));
            if (!expr) {
                transcendental_destroy(t);
                return NULL;
            }
            expr->expr_type = TRANS_EXPR_ADD_RATIONAL;
            lv_strlcpy(expr->base_name, trans_coord->data.transcendental->name, sizeof(expr->base_name));
            Rational *neg_r = rational_create(0, 1);
            mpq_neg(neg_r->value, other_coord->data.rational->value);
            expr->rational_operand = neg_r;
            expr->out_of_scope = false;

            t->expr = expr;

            SymbolicCoord *result = lv_calloc(1, sizeof(SymbolicCoord));
            if (!result) {
                transcendental_destroy(t);
                return NULL;
            }
            result->type = TRANSCENDENTAL;
            result->trust = trans_coord->trust;
            result->data.transcendental = t;
            return result;
        }

        if (other_coord->type == ALGEBRAIC || other_coord->type == QUADRATIC) {
            Transcendental *t = transcendental_create(trans_coord->data.transcendental->name);
            if (!t)
                return NULL;

            TranscendentalExpr *expr = lv_calloc(1, sizeof(TranscendentalExpr));
            if (!expr) {
                transcendental_destroy(t);
                return NULL;
            }
            expr->expr_type = TRANS_EXPR_ADD_ALGEBRAIC;
            lv_strlcpy(expr->base_name, trans_coord->data.transcendental->name, sizeof(expr->base_name));
            expr->rational_operand = NULL;
            expr->out_of_scope = true;

            t->expr = expr;

            SymbolicCoord *result = lv_calloc(1, sizeof(SymbolicCoord));
            if (!result) {
                transcendental_destroy(t);
                return NULL;
            }
            result->type = TRANSCENDENTAL;
            result->trust = TRUST_AMBER;
            result->data.transcendental = t;
            return result;
        }

        {
            const Transcendental *ta = a->data.transcendental;
            const Transcendental *tb = b->data.transcendental;
            const char *base_a = ta->expr ? ta->expr->base_name : ta->name;
            const char *base_b = tb->expr ? tb->expr->base_name : tb->name;

            Transcendental *t = transcendental_create(base_a);
            if (!t)
                return NULL;

            TranscendentalExpr *expr = lv_calloc(1, sizeof(TranscendentalExpr));
            if (!expr) {
                transcendental_destroy(t);
                return NULL;
            }
            expr->out_of_scope = false;
            lv_strlcpy(expr->base_name, base_a, sizeof(expr->base_name));
            if (strcmp(base_a, base_b) == 0) {
                bool a_is_mul = !ta->expr || ta->expr->expr_type == TRANS_EXPR_MUL_RATIONAL;
                bool b_is_mul = !tb->expr || tb->expr->expr_type == TRANS_EXPR_MUL_RATIONAL;

                if (a_is_mul && b_is_mul) {
                    Rational *rat_a =
                        (ta->expr && ta->expr->rational_operand) ? ta->expr->rational_operand : rational_create(1, 1);
                    Rational *rat_b =
                        (tb->expr && tb->expr->rational_operand) ? tb->expr->rational_operand : rational_create(1, 1);
                    Rational *own_a = (!ta->expr) ? rat_a : NULL;
                    Rational *own_b = (!tb->expr) ? rat_b : NULL;

                    expr->expr_type = TRANS_EXPR_MUL_RATIONAL;
                    expr->rational_operand = rational_subtract(rat_a, rat_b);

                    if (own_a)
                        rational_destroy(own_a);
                    if (own_b)
                        rational_destroy(own_b);

                    if (!expr->rational_operand) {
                        lv_free((void **) &expr);
                        transcendental_destroy(t);
                        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "symbolic_coord_add: rational_operand allocation failed");
                    }
                } else {
                    expr->expr_type = TRANS_EXPR_ADD_ALGEBRAIC;
                    expr->rational_operand = NULL;
                    expr->out_of_scope = true;
                }
            } else {
                expr->expr_type = TRANS_EXPR_ADD_ALGEBRAIC;
                expr->rational_operand = NULL;
                expr->out_of_scope = true;
            }

            t->expr = expr;

            SymbolicCoord *result = lv_calloc(1, sizeof(SymbolicCoord));
            if (!result) {
                transcendental_destroy(t);
                return NULL;
            }
            result->type = TRANSCENDENTAL;
            result->trust = expr->out_of_scope ? TRUST_AMBER : TRUST_BLUE_UNEXPLORED;
            result->data.transcendental = t;
            return result;
        }
    }

    /* Same type operations: vtable dispatch */
    if (a->type == b->type) {
        return kCoordOpsVTable[a->type].subtract(a, b);
    }

    /* Cross-type operations：统一提升分派（先提升低 rank 操作数，再递归同类型运算） */
    return promote_cross_type_dispatch(a, b, symbolic_coord_subtract);
}

SymbolicCoord *symbolic_coord_multiply(const SymbolicCoord *a, const SymbolicCoord *b) {
    if (!a || !b)
        return NULL;
    g_overflow_context.left_type = a->type;
    g_overflow_context.right_type = b->type;
    g_overflow_context.last_operation = "multiply";

    /* Transcendental * anything */
    if (a->type == TRANSCENDENTAL || b->type == TRANSCENDENTAL) {
        const SymbolicCoord *trans_coord = (a->type == TRANSCENDENTAL) ? a : b;
        const SymbolicCoord *other_coord = (a->type == TRANSCENDENTAL) ? b : a;

        if (other_coord->type == RATIONAL) {
            Transcendental *t = transcendental_create(trans_coord->data.transcendental->name);
            if (!t)
                return NULL;

            TranscendentalExpr *expr = lv_calloc(1, sizeof(TranscendentalExpr));
            if (!expr) {
                transcendental_destroy(t);
                return NULL;
            }
            expr->expr_type = TRANS_EXPR_MUL_RATIONAL;
            lv_strlcpy(expr->base_name, trans_coord->data.transcendental->name, sizeof(expr->base_name));
            expr->rational_operand = rational_copy(other_coord->data.rational);
            expr->out_of_scope = false;

            t->expr = expr;

            SymbolicCoord *result = lv_calloc(1, sizeof(SymbolicCoord));
            if (!result) {
                transcendental_destroy(t);
                return NULL;
            }
            result->type = TRANSCENDENTAL;
            result->trust = trans_coord->trust;
            result->data.transcendental = t;
            return result;
        }

        if (other_coord->type == ALGEBRAIC || other_coord->type == QUADRATIC) {
            Transcendental *t = transcendental_create(trans_coord->data.transcendental->name);
            if (!t)
                return NULL;

            TranscendentalExpr *expr = lv_calloc(1, sizeof(TranscendentalExpr));
            if (!expr) {
                transcendental_destroy(t);
                return NULL;
            }
            expr->expr_type = TRANS_EXPR_MUL_ALGEBRAIC;
            lv_strlcpy(expr->base_name, trans_coord->data.transcendental->name, sizeof(expr->base_name));
            expr->rational_operand = NULL;
            expr->out_of_scope = true;

            t->expr = expr;

            SymbolicCoord *result = lv_calloc(1, sizeof(SymbolicCoord));
            if (!result) {
                transcendental_destroy(t);
                return NULL;
            }
            result->type = TRANSCENDENTAL;
            result->trust = TRUST_AMBER;
            result->data.transcendental = t;
            return result;
        }

        {
            const Transcendental *ta = a->data.transcendental;
            const Transcendental *tb = b->data.transcendental;
            const char *base_a = ta->expr ? ta->expr->base_name : ta->name;
            const char *base_b = tb->expr ? tb->expr->base_name : tb->name;

            Transcendental *t = transcendental_create(base_a);
            if (!t)
                return NULL;

            TranscendentalExpr *expr = lv_calloc(1, sizeof(TranscendentalExpr));
            if (!expr) {
                transcendental_destroy(t);
                return NULL;
            }
            expr->expr_type = TRANS_EXPR_MUL_ALGEBRAIC;
            lv_strlcpy(expr->base_name, base_a, sizeof(expr->base_name));
            expr->rational_operand = NULL;
            expr->out_of_scope = true;

            t->expr = expr;

            SymbolicCoord *result = lv_calloc(1, sizeof(SymbolicCoord));
            if (!result) {
                transcendental_destroy(t);
                return NULL;
            }
            result->type = TRANSCENDENTAL;
            result->trust = TRUST_AMBER;
            result->data.transcendental = t;
            return result;
        }
    }

    /* Same type operations: vtable dispatch */
    if (a->type == b->type) {
        return kCoordOpsVTable[a->type].multiply(a, b);
    }

    /* Cross-type operations */

    /* RATIONAL * QUADRATIC = QUADRATIC：直接系数运算（非提升路径，保持原语义） */
    if ((a->type == RATIONAL && b->type == QUADRATIC) ||
        (a->type == QUADRATIC && b->type == RATIONAL)) {
        if (a->type == RATIONAL) {
            Rational *new_a = rational_multiply(a->data.rational, b->data.quadratic->a);
            Rational *new_b = rational_multiply(a->data.rational, b->data.quadratic->b);
            SymbolicCoord *result = symbolic_coord_create_quadratic(new_a, new_b, b->data.quadratic->n);
            if (result)
                result->trust = (a->trust < b->trust) ? a->trust : b->trust;
            return result;
        }
        Rational *new_a = rational_multiply(a->data.quadratic->a, b->data.rational);
        Rational *new_b = rational_multiply(a->data.quadratic->b, b->data.rational);
        SymbolicCoord *result = symbolic_coord_create_quadratic(new_a, new_b, a->data.quadratic->n);
        if (result)
            result->trust = (a->trust < b->trust) ? a->trust : b->trust;
        return result;
    }

    /* 其余组合（RATIONAL↔ALGEBRAIC、QUADRATIC↔ALGEBRAIC）：统一提升分派 */
    return promote_cross_type_dispatch(a, b, symbolic_coord_multiply);
}

/**
 * 符号坐标除法：计算 a / b。
 */
SymbolicCoord *symbolic_coord_divide(const SymbolicCoord *a, const SymbolicCoord *b) {
    if (!a || !b)
        return NULL;
    g_overflow_context.left_type = a->type;
    g_overflow_context.right_type = b->type;
    g_overflow_context.last_operation = "divide";

    if (symbolic_coord_is_zero(b)) {
        return NULL;
    }

    /* Transcendental / anything */
    if (a->type == TRANSCENDENTAL || b->type == TRANSCENDENTAL) {
        const SymbolicCoord *trans_coord = (a->type == TRANSCENDENTAL) ? a : b;
        const SymbolicCoord *other_coord = (a->type == TRANSCENDENTAL) ? b : a;
        bool inverted = (b->type == TRANSCENDENTAL);

        if (other_coord->type == RATIONAL) {
            Transcendental *t = transcendental_create(trans_coord->data.transcendental->name);
            if (!t)
                return NULL;

            TranscendentalExpr *expr = lv_calloc(1, sizeof(TranscendentalExpr));
            if (!expr) {
                transcendental_destroy(t);
                return NULL;
            }
            expr->expr_type = TRANS_EXPR_MUL_RATIONAL;
            lv_strlcpy(expr->base_name, trans_coord->data.transcendental->name, sizeof(expr->base_name));
            if (inverted) {
                transcendental_destroy(t);
                lv_free((void **) &expr);
                return NULL;
            } else {
                expr->rational_operand = rational_divide(rational_create(1, 1), other_coord->data.rational);
                if (!expr->rational_operand) {
                    transcendental_destroy(t);
                    lv_free((void **) &expr);
                    return NULL;
                }
            }
            expr->out_of_scope = false;

            t->expr = expr;

            SymbolicCoord *result = lv_calloc(1, sizeof(SymbolicCoord));
            if (!result) {
                transcendental_destroy(t);
                return NULL;
            }
            result->type = TRANSCENDENTAL;
            result->trust = trans_coord->trust;
            result->data.transcendental = t;
            return result;
        }

        if (other_coord->type == ALGEBRAIC || other_coord->type == QUADRATIC) {
            Transcendental *t = transcendental_create(trans_coord->data.transcendental->name);
            if (!t)
                return NULL;

            TranscendentalExpr *expr = lv_calloc(1, sizeof(TranscendentalExpr));
            if (!expr) {
                transcendental_destroy(t);
                return NULL;
            }
            expr->expr_type = TRANS_EXPR_MUL_ALGEBRAIC;
            lv_strlcpy(expr->base_name, trans_coord->data.transcendental->name, sizeof(expr->base_name));
            expr->rational_operand = NULL;
            expr->out_of_scope = true;

            t->expr = expr;

            SymbolicCoord *result = lv_calloc(1, sizeof(SymbolicCoord));
            if (!result) {
                transcendental_destroy(t);
                return NULL;
            }
            result->type = TRANSCENDENTAL;
            result->trust = TRUST_AMBER;
            result->data.transcendental = t;
            return result;
        }

        {
            const Transcendental *ta = a->data.transcendental;
            const Transcendental *tb = b->data.transcendental;
            const char *base_a = ta->expr ? ta->expr->base_name : ta->name;
            const char *base_b = tb->expr ? tb->expr->base_name : tb->name;

            if (strcmp(base_a, base_b) == 0) {
                bool a_is_mul = !ta->expr || ta->expr->expr_type == TRANS_EXPR_MUL_RATIONAL;
                bool b_is_mul = !tb->expr || tb->expr->expr_type == TRANS_EXPR_MUL_RATIONAL;

                if (a_is_mul && b_is_mul) {
                    Rational *rat_a =
                        (ta->expr && ta->expr->rational_operand) ? ta->expr->rational_operand : rational_create(1, 1);
                    Rational *rat_b =
                        (tb->expr && tb->expr->rational_operand) ? tb->expr->rational_operand : rational_create(1, 1);
                    Rational *own_a = (!ta->expr) ? rat_a : NULL;
                    Rational *own_b = (!tb->expr) ? rat_b : NULL;

                    Rational *result_rat = rational_divide(rat_a, rat_b);

                    if (own_a)
                        rational_destroy(own_a);
                    if (own_b)
                        rational_destroy(own_b);

                    if (!result_rat)
                        return NULL;

                    SymbolicCoord *result = symbolic_coord_create_rational(0, 1);
                    if (!result) {
                        rational_destroy(result_rat);
                        return NULL;
                    }
                    rational_destroy(result->data.rational);
                    result->data.rational = result_rat;
                    result->trust = TRUST_GREEN;
                    return result;
                }
            }

            Transcendental *t = transcendental_create(base_a);
            if (!t)
                return NULL;

            TranscendentalExpr *expr = lv_calloc(1, sizeof(TranscendentalExpr));
            if (!expr) {
                transcendental_destroy(t);
                return NULL;
            }
            expr->expr_type = TRANS_EXPR_MUL_ALGEBRAIC;
            lv_strlcpy(expr->base_name, base_a, sizeof(expr->base_name));
            expr->rational_operand = NULL;
            expr->out_of_scope = true;

            t->expr = expr;

            SymbolicCoord *result = lv_calloc(1, sizeof(SymbolicCoord));
            if (!result) {
                transcendental_destroy(t);
                return NULL;
            }
            result->type = TRANSCENDENTAL;
            result->trust = TRUST_AMBER;
            result->data.transcendental = t;
            return result;
        }
    }

    /* Same type operations: vtable dispatch */
    if (a->type == b->type) {
        return kCoordOpsVTable[a->type].divide(a, b);
    }

    /* Cross-type operations */

    /* QUADRATIC / RATIONAL = QUADRATIC：直接系数运算（非提升路径，保持原语义） */
    if (a->type == QUADRATIC && b->type == RATIONAL) {
        Rational *new_a = rational_divide(a->data.quadratic->a, b->data.rational);
        Rational *new_b = rational_divide(a->data.quadratic->b, b->data.rational);
        if (!new_a || !new_b) {
            if (new_a)
                rational_destroy(new_a);
            if (new_b)
                rational_destroy(new_b);
            return NULL;
        }
        SymbolicCoord *result = symbolic_coord_create_quadratic(new_a, new_b, a->data.quadratic->n);
        if (result)
            result->trust = (a->trust < b->trust) ? a->trust : b->trust;
        return result;
    }

    /* 其余组合（RATIONAL÷QUADRATIC 提升、RATIONAL↔ALGEBRAIC、QUADRATIC↔ALGEBRAIC）：统一提升分派 */
    return promote_cross_type_dispatch(a, b, symbolic_coord_divide);
}

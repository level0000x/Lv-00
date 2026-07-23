/**
 * @file symbolic_coord_ops.c
 * @brief SymbolicCoord 四类型算术运算与比较操作
 *
 * @details 实现 RATIONAL / QUADRATIC / ALGEBRAIC / TRANSCENDENTAL
 *          四种符号坐标类型间的混合运算与比较。核心操作：
 *
 *          算术：
 *          - symbolic_coord_add / sub / mul / div: 类型分派加法/减法/乘法/除法
 *          - symbolic_coord_pow: 正整数幂（快速幂算法）
 *          - symbolic_coord_negate / abs / reciprocal: 取负/绝对值/倒数
 *
 *          比较：
 *          - symbolic_coord_compare: 全序比较（-1/0/1）
 *          - symbolic_coord_approx_eq: 近似相等（容差 lv_EPSILON_DOUBLE）
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

#include "lv/symbolic_coord.h"
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lv/constraint_graph.h"
#include "lv/bit_burning.h"
#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "mpz_poly.h"

#define SYM_COORD_DYNAMIC_ARRAY_INIT_CAP 16
#define SYM_COORD_SIGFIGS_MIN_SAFE 6
#define SYM_COORD_SIGFIGS_APPROX 4
#define SYM_COORD_EPS 1e-8
#define SYM_COORD_MAX_REFINE 15
#define SYM_COORD_AMB_MIN_SIGFIGS 3
#define COORD_SEVEN_OVER_FIVE_N 32
/* ── 前向声明 ── */
mpz_t *mpz_perfect_sqrt(mpz_t n);
static Rational *algebraic_continued_fraction_approx(const Algebraic *a, double precision);
double algebraic_to_double(const Algebraic *a);
double quadratic_to_double(const Quadratic *q);
double transcendental_to_double(const Transcendental *t);
bool is_rational_zero(const Rational *r);
void refine_algebraic_bounds(Algebraic *a, int iterations);
char *algebraic_serialize(const Algebraic *a);
char *quadratic_serialize(const Quadratic *q);
int remove_square_factors(int n);

/* ── 外部溢出上下文 ── */
extern lv_THREAD_LOCAL struct OverflowContext g_overflow_context;

/* ── Plan Manager 线程局部状态 ── */
#define SYM_COORD_DEGRADE_THRESHOLD  3
#define SYM_COORD_ALGEBRAIC_BIT_LIMIT_FACTOR 10  /* lv_BIT_CUTOFF_THRESHOLD / 10 */

static lv_THREAD_LOCAL int g_degrade_fail_count = 0;
static lv_THREAD_LOCAL int g_degrade_total = 0;

/* ── 前向声明: 降级辅助函数 ── */
static SymbolicCoord *_symbolic_coord_degrade_check_algebraic(SymbolicCoord *result);

/* ── 位数熔断辅助函数 ── */
static void bit_burning_check_result(SymbolicCoord *result, const char *operation);

/**
 * @brief 检查 SymbolicCoord 结果的位数是否超过熔断阈值。
 *
 * 当 Rational 类型结果的分子或分母位数超过 BIT_CUTOFF_THRESHOLD 时，
 * 触发熔断状态更新。如果连续触发次数达到 MAX_CONSECUTIVE_TRIPS，
 * 自动将结果降级为 TRUST_AMBER。
 *
 * @param result   运算结果（可为 NULL）
 * @param operation 操作名称（如 "add", "multiply" 等）
 */
static void bit_burning_check_result(SymbolicCoord *result, const char *operation) {
    if (!result || result->type != RATIONAL)
        return;

    (void)operation; /* 保留参数供未来扩展使用 */

    size_t num_bits = mpz_sizeinbase(mpq_numref(result->data.rational->value), 2);
    size_t den_bits = mpz_sizeinbase(mpq_denref(result->data.rational->value), 2);

    /* 溢出保护 */
    if (num_bits > SIZE_MAX - den_bits) {
        /* 位数溢出，严重情况，直接标记为 AMBER */
        result->trust = TRUST_AMBER;
        return;
    }

    size_t total_bits = num_bits + den_bits;
    if (total_bits <= BIT_CUTOFF_THRESHOLD)
        return;

    BitBurningState *state = bit_burning_get_global_state();
    if (bit_burning_check(total_bits, state)) {
        /* 连续的位数熔断触发，根据连续次数执行不同策略 */
        if (state->consecutive_trips >= MAX_CONSECUTIVE_TRIPS) {
            /* 逃逸出口：连续 3 次触发，永久降级为数值假设 */
            result->trust = TRUST_AMBER;
        } else {
            /* 第 1-2 次：标记为 AMBER，但保留完整构造信息 */
            result->trust = TRUST_AMBER;
        }
    }
}

/* ── SymbolicCoord 操作 ── */

/**
 * 将超越数转换为 double 近似值。
 *
 * @param t 超越数对象
 * @return double 近似值
 */
static double transcendental_expr_to_double(const Transcendental *t) {
    if (!t)
        return 0.0;

    /* Get base constant value */
    const char *base = t->expr ? t->expr->base_name : t->name;
    double base_val;
    if (strcmp(base, "pi") == 0) {
        base_val = M_PI;
    } else if (strcmp(base, "e") == 0) {
        base_val = M_E;
    } else {
        return 0.0;
    }

    /* Bare constant (no expr) */
    if (!t->expr) {
        return base_val;
    }

    /* Handle expression types */
    if (t->expr->rational_operand) {
        double rat_val = rational_to_double(t->expr->rational_operand);
        switch (t->expr->expr_type) {
            case TRANS_EXPR_MUL_RATIONAL:
                return base_val * rat_val;
            case TRANS_EXPR_ADD_RATIONAL:
                return base_val + rat_val;
            default:
                break;
        }
    }

    /* Handle ADD_ALGEBRAIC / MUL_ALGEBRAIC: these are out_of_scope expressions
     * (e.g., pi + sqrt(2), e * sqrt(3)). We cannot compute the exact value
     * since the algebraic operand is not stored in the TranscendentalExpr.
     * Return the base constant value as an approximation, with the understanding
     * that this is only the base component of the expression.
     * Note: for expressions like "e + pi", the name may not match any known
     * pattern, so the fallback below would return 0.0. Returning base_val here
     * is a better approximation. */
    if (t->expr->expr_type == TRANS_EXPR_ADD_ALGEBRAIC || t->expr->expr_type == TRANS_EXPR_MUL_ALGEBRAIC) {
        /* Approximate: return the base constant value.
         * This is an approximation for out_of_scope expressions. */
        return base_val;
    }

    /* Fallback: try to parse the name for compound forms like "pi/2", "3*pi/4" */
    {
        const char *name = t->name;
        /* Try N*pi/M form */
        char *star_pos = strstr(name, "*pi");
        if (star_pos) {
            int64_t coeff_num = 1;
            int64_t coeff_den = 1;
            if (star_pos == name + 1 && name[0] != '-') {
                coeff_num = atol(name);
            } else if (star_pos == name + 2 && name[0] == '-') {
                coeff_num = atol(name);
            }
            const char *after = star_pos + 3;
            if (*after == '/') {
                coeff_den = atol(after + 1);
            }
            if (coeff_den > 0) {
                return M_PI * (double) coeff_num / (double) coeff_den;
            }
        }

        /* Try pi/N form */
        if (strncmp(name, "pi/", 3) == 0) {
            int64_t den = atol(name + 3);
            if (den > 0)
                return M_PI / (double) den;
        }

        /* Try -pi/N form */
        if (strncmp(name, "-pi/", 4) == 0) {
            int64_t den = atol(name + 4);
            if (den > 0)
                return -M_PI / (double) den;
        }

        /* Try -N*pi/M form */
        if (name[0] == '-' && strstr(name, "*pi")) {
            char *sp = strstr(name, "*pi");
            int64_t coeff_num = atol(name);
            int64_t coeff_den = 1;
            const char *after = sp + 3;
            if (*after == '/') {
                coeff_den = atol(after + 1);
            }
            if (coeff_den > 0) {
                return M_PI * (double) coeff_num / (double) coeff_den;
            }
        }
    }

    return 0.0;
}

/* ============================================================
 * SymbolicCoord Implementation
 * ============================================================ */

SymbolicCoord *symbolic_coord_create_rational(int64_t num, uint64_t denom) {
    SymbolicCoord *coord = lv_calloc(1, sizeof(SymbolicCoord));
    if (!coord)
        return NULL;
    coord->type = RATIONAL;
    coord->trust = TRUST_GREEN;
    coord->cache_valid = false;
    coord->cached_value = 0.0;
    coord->data.rational = rational_create(num, denom);
    if (!coord->data.rational) {
        lv_free((void**)&coord);  /* lv_malloc分配 */
        return NULL;
    }
    return coord;
}

/**
 * 创建代数数类型的符号坐标。
 *
 * @param poly  极小多项式
 * @param left  隔离区间左边界
 * @param right 隔离区间右边界
 * @return 新创建的符号坐标对象，失败时返回 NULL；调用者需负责释放
 */
SymbolicCoord *symbolic_coord_create_algebraic(mpz_poly_t *poly, double left, double right) {
    SymbolicCoord *coord = lv_calloc(1, sizeof(SymbolicCoord));
    if (!coord)
        return NULL;
    coord->type = ALGEBRAIC;
    coord->trust = TRUST_GREEN;
    coord->cache_valid = false;
    coord->cached_value = 0.0;
    coord->data.algebraic = algebraic_create(poly, left, right);
    if (!coord->data.algebraic) {
        lv_free((void**)&coord);  /* lv_malloc分配 */
        return NULL;
    }
    return coord;
}

/**
 * 创建二次根式类型的符号坐标。
 *
 * @param a 二次项的系数有理数
 * @param b 根号项的系数有理数
 * @param n 根号内的整数
 * @return 新创建的符号坐标对象，失败时返回 NULL；调用者需负责释放
 */
SymbolicCoord *symbolic_coord_create_quadratic(Rational *a, Rational *b, unsigned int n) {
    SymbolicCoord *coord = lv_calloc(1, sizeof(SymbolicCoord));
    if (!coord)
        return NULL;
    coord->type = QUADRATIC;
    coord->trust = TRUST_GREEN;
    coord->cache_valid = false;
    coord->cached_value = 0.0;
    coord->data.quadratic = quadratic_create(a, b, n);
    if (!coord->data.quadratic) {
        lv_free((void**)&coord);  /* lv_malloc分配 */
        return NULL;
    }
    return coord;
}

SymbolicCoord *symbolic_coord_create_transcendental(const char *name) {
    SymbolicCoord *coord = lv_calloc(1, sizeof(SymbolicCoord));
    if (!coord)
        return NULL;
    coord->type = TRANSCENDENTAL;
    coord->trust = TRUST_BLUE_UNEXPLORED;
    coord->cache_valid = false;
    coord->cached_value = 0.0;
    coord->data.transcendental = transcendental_create(name);
    if (!coord->data.transcendental) {
        lv_free((void**)&coord);  /* lv_malloc分配 */
        return NULL;
    }
    return coord;
}

/**
 * 销毁符号坐标对象并释放内存。
 *
 * 销毁操作包括：
 * 1. 根据坐标类型调用对应的类型销毁函数，释放底层 GMP 变量和动态内存
 * 2. 递归清理嵌套数据结构（如 algebraic 的 cached_rational、
 *    quadratic 的子有理数、transcendental 的表达式树等）
 * 3. 使数值缓存失效，防止悬空引用
 * 4. 将所有指针置 NULL，防止悬空指针
 *
 * @param coord 符号坐标对象，可为 NULL（空操作）
 */
void symbolic_coord_destroy(SymbolicCoord *coord) {
    if (!coord)
        return;

    /* 使数值缓存失效 */
    coord->cache_valid = false;
    coord->cached_value = 0.0;

    switch (coord->type) {
        case RATIONAL:
            rational_destroy(coord->data.rational);
            coord->data.rational = NULL;
            break;
        case ALGEBRAIC:
            algebraic_destroy(coord->data.algebraic);
            coord->data.algebraic = NULL;
            break;
        case QUADRATIC:
            quadratic_destroy(coord->data.quadratic);
            coord->data.quadratic = NULL;
            break;
        case TRANSCENDENTAL:
            transcendental_destroy(coord->data.transcendental);
            coord->data.transcendental = NULL;
            break;
        default:
            break; /* 未知类型无需释放 */
    }

    /* 将 trust 颜色重置为安全默认值 */
    coord->trust = TRUST_GREEN;
    coord->type = RATIONAL;

    lv_free((void**)&coord);  /* lv_malloc分配 */
}

/**
 * 获取任意 SymbolicCoord 类型的数值近似。
 *
 * 先检查缓存：若 cache_valid 为 true，则直接返回 cached_value，
 * 避免重复的 GMP 转换开销。缓存失效时重新计算并更新缓存。
 * 当坐标被修改时需调用 symbolic_coord_invalidate_cache() 使缓存失效。
 *
 * 根据 coord 的类型调用相应的转换函数：
 * - RATIONAL: rational_to_double
 * - ALGEBRAIC: algebraic_to_double
 * - QUADRATIC: quadratic_to_double
 * - TRANSCENDENTAL: transcendental_to_double
 *
 * @param coord SymbolicCoord 对象（不能为 NULL）
 * @return 转换后的双精度浮点数值
 */
double symbolic_coord_to_double(const SymbolicCoord *coord) {
    if (!coord)
        return 0.0;

    /* 缓存命中：直接返回已缓存值，避免重复计算 */
    /* 注意：为保持 const 语义，将 const 转换为非 const 以便写入缓存。
     * 这是安全的，因为缓存是透明的性能优化，不影响逻辑语义。 */
    if (coord->cache_valid) {
        return coord->cached_value;
    }

    double val = 0.0;
    switch (coord->type) {
        case RATIONAL:
            val = rational_to_double(coord->data.rational);
            break;
        case ALGEBRAIC:
            val = algebraic_to_double(coord->data.algebraic);
            break;
        case QUADRATIC:
            val = quadratic_to_double(coord->data.quadratic);
            break;
        case TRANSCENDENTAL:
            val = transcendental_to_double(coord->data.transcendental);
            break;
        default:
            val = 0.0;
            break;
    }

    /* 更新缓存（const 转换为非 const：缓存是性能优化，不改变逻辑语义） */
    ((SymbolicCoord *)coord)->cached_value = val;
    ((SymbolicCoord *)coord)->cache_valid = true;

    return val;
}

/**
 * 使符号坐标的数值缓存失效。
 *
 * 当坐标被修改（如算术运算、信任颜色变更、类型转换）时，
 * 调用此函数标记缓存为无效，确保后续调用
 * symbolic_coord_to_double() 时重新计算精确数值。
 * 提供的指针将被设置为 NULL。
 *
 * @param coord 符号坐标（可为 NULL，空操作）
 */
void symbolic_coord_invalidate_cache(SymbolicCoord *coord) {
    if (!coord)
        return;
    coord->cache_valid = false;
    coord->cached_value = 0.0;
}

/* Cross-type comparison */
int symbolic_coord_compare(const SymbolicCoord *a, const SymbolicCoord *b) {
    if (!a || !b) return 0;
    /* Same type: use type-specific comparison */
    if (a->type == b->type) {
        switch (a->type) {
            case RATIONAL:
                return rational_compare(a->data.rational, b->data.rational);
            case ALGEBRAIC:
                return algebraic_compare(a->data.algebraic, b->data.algebraic);
            case QUADRATIC:
                return quadratic_compare(a->data.quadratic, b->data.quadratic);
            case TRANSCENDENTAL:
                return transcendental_compare(a->data.transcendental, b->data.transcendental);
            default:
                return 0;
        }
    }

    /* 跨类型比较（提高精度）*/

    /* 策略1：如果一方是有理数而另一方是代数数，
     * 尝试将有理化代数数以进行精确比较。 */
    if (a->type == RATIONAL && b->type == ALGEBRAIC) {
        Algebraic *alg = b->data.algebraic;
        /* Check cached rational first */
        if (alg->cached_rational) {
            return rational_compare(a->data.rational, alg->cached_rational);
        }
        /* Try rationalization */
        algebraic_try_rationalize(alg);
        if (alg->cached_rational) {
            return rational_compare(a->data.rational, alg->cached_rational);
        }
    }
    if (b->type == RATIONAL && a->type == ALGEBRAIC) {
        Algebraic *alg = a->data.algebraic;
        if (alg->cached_rational) {
            return rational_compare(alg->cached_rational, b->data.rational);
        }
        algebraic_try_rationalize(alg);
        if (alg->cached_rational) {
            return rational_compare(alg->cached_rational, b->data.rational);
        }
    }

    /* 策略2：如果一方是有理数而另一方是二次根式，
     * 检查二次根式是否实际上是有理数（b == 0）。 */
    if (a->type == RATIONAL && b->type == QUADRATIC) {
        const Quadratic *q = b->data.quadratic;
        if (is_rational_zero(q->b)) {
            return rational_compare(a->data.rational, q->a);
        }
    }
    if (b->type == RATIONAL && a->type == QUADRATIC) {
        const Quadratic *q = a->data.quadratic;
        if (is_rational_zero(q->b)) {
            return rational_compare(q->a, b->data.rational);
        }
    }

    /* 策略3：代数数与其他类型的隔离区间交叉检查。
     * 如果操作数之一是代数数，使用其隔离区间来确定
     * 差值的符号，而无需进行完整的 double 计算。 */
    if (a->type == ALGEBRAIC) {
        const Algebraic *alg = a->data.algebraic;
        double b_val = symbolic_coord_to_double(b);
        /* If the entire isolation interval of a is strictly on one side of b_val */
        if (alg->right_bound < b_val - lv_EPSILON_NUMERIC_COMPARE)
            return -1;
        if (alg->left_bound > b_val + lv_EPSILON_NUMERIC_COMPARE)
            return 1;
        /* Intervals overlap: refine and retry */
        refine_algebraic_bounds(alg, 5);
        if (alg->right_bound < b_val - lv_EPSILON_NUMERIC_COMPARE)
            return -1;
        if (alg->left_bound > b_val + lv_EPSILON_NUMERIC_COMPARE)
            return 1;
    }
    if (b->type == ALGEBRAIC) {
        const Algebraic *alg = b->data.algebraic;
        double a_val = symbolic_coord_to_double(a);
        if (alg->right_bound < a_val - lv_EPSILON_NUMERIC_COMPARE)
            return 1;
        if (alg->left_bound > a_val + lv_EPSILON_NUMERIC_COMPARE)
            return -1;
        refine_algebraic_bounds(alg, 5);
        if (alg->right_bound < a_val - lv_EPSILON_NUMERIC_COMPARE)
            return 1;
        if (alg->left_bound > a_val + lv_EPSILON_NUMERIC_COMPARE)
            return -1;
    }

    /* 策略4：对于二次根式 vs 有理数（b != 0 的情况），
     * 使用连分数近似以获得更高精度。 */
    if ((a->type == RATIONAL && b->type == QUADRATIC) || (b->type == RATIONAL && a->type == QUADRATIC)) {
        const SymbolicCoord *rat_coord = (a->type == RATIONAL) ? a : b;
        const SymbolicCoord *quad_coord = (a->type == QUADRATIC) ? a : b;
        const Quadratic *q = quad_coord->data.quadratic;
        double q_val = rational_to_double(q->a) + rational_to_double(q->b) * sqrt((double) q->n);
        double r_val = rational_to_double(rat_coord->data.rational);

        /* Use tighter tolerance for cross-type comparison */
        double tight_eps = lv_EPSILON_NUMERIC_COMPARE * 0.01;
        if (q_val < r_val - tight_eps)
            return (a->type == QUADRATIC) ? -1 : 1;
        if (q_val > r_val + tight_eps)
            return (a->type == QUADRATIC) ? 1 : -1;
        /* Within tight epsilon: values are effectively equal */
        return 0;
    }

    /* 策略5：最后手段——使用更严格容差的 double 近似比较 */
    double a_val = symbolic_coord_to_double(a);
    double b_val = symbolic_coord_to_double(b);

    /* Use stricter tolerance: 1/100 of the normal epsilon */
    double strict_eps = lv_EPSILON_NUMERIC_COMPARE * 0.01;

    if (a_val < b_val - strict_eps)
        return -1;
    if (a_val > b_val + strict_eps)
        return 1;
    return 0;
}

/* ============================================================
 * Type Promotion Functions
 * ============================================================ */

/**
 * 将有理数类型的符号坐标提升为代数数。
 *
 * @param r 有理数类型的符号坐标（不能为 NULL）
 * @return 新创建的代数数类型的符号坐标，失败时返回 NULL
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
 *
 * @param r 有理数类型的符号坐标（不能为 NULL）
 * @param n 二次根式的 n 值
 * @return 新创建的二次根式类型的符号坐标，失败时返回 NULL
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
 * Cross-type Arithmetic Operations
 * ============================================================ */

/**
 * 符号坐标加法：计算 a + b。
 *
 * 支持所有类型的跨类型运算：
 * - 同类型运算使用类型特定的精确算法
 * - 跨类型运算自动进行类型提升
 * - 超越数与其他类型运算结果标记为 AMBER（信任级别降低）
 *
 * @param a 被加数（不能为 NULL）
 * @param b 加数（不能为 NULL）
 * @return 新的符号坐标对象，失败时返回 NULL；调用者需负责释放
 */
SymbolicCoord *symbolic_coord_add(const SymbolicCoord *a, const SymbolicCoord *b) {
    /* Update overflow context */
    g_overflow_context.left_type = a->type;
    g_overflow_context.right_type = b->type;
    g_overflow_context.last_operation = "add";

    /* Transcendental + anything */
    if (a->type == TRANSCENDENTAL || b->type == TRANSCENDENTAL) {
        const SymbolicCoord *trans_coord = (a->type == TRANSCENDENTAL) ? a : b;
        const SymbolicCoord *other_coord = (a->type == TRANSCENDENTAL) ? b : a;

        if (other_coord->type == RATIONAL) {
            /* Transcendental + Rational -> TRANS_EXPR_ADD_RATIONAL */
            Transcendental *t = transcendental_create(trans_coord->data.transcendental->name);
            if (!t)
                return NULL;

            TranscendentalExpr *expr = lv_calloc(1, sizeof(TranscendentalExpr));
            if (!expr) {
                transcendental_destroy(t);
                return NULL;
            }
            expr->expr_type = TRANS_EXPR_ADD_RATIONAL;
            /* 使用 lv_strlcpy 替代不安全的 strncpy（自动保证零终止） */
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
            /* Transcendental + Algebraic/Quadratic -> out_of_scope */
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

        /* Transcendental + Transcendental: merge if same base, else out_of_scope */
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
                /* 相同底数：检查两者是否都是纯乘法形式（MUL 或裸常量）*/
                bool a_is_mul = !ta->expr || ta->expr->expr_type == TRANS_EXPR_MUL_RATIONAL;
                bool b_is_mul = !tb->expr || tb->expr->expr_type == TRANS_EXPR_MUL_RATIONAL;

                if (a_is_mul && b_is_mul) {
                    /* 两者都是系数*底数形式：(ca*base) + (cb*base) = (ca+cb)*base */
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
                        return NULL;
                    }
                } else {
                    /* 至少有一个是 ADD 形式：无法简化，超出作用域 */
                    expr->expr_type = TRANS_EXPR_ADD_ALGEBRAIC;
                    expr->rational_operand = NULL;
                    expr->out_of_scope = true;
                }
            } else {
                /* 不同底数（如 pi + e）：超出作用域 */
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

    /* Same type operations */
    if (a->type == b->type) {
        switch (a->type) {
            case RATIONAL: {
                Rational *r = rational_add(a->data.rational, b->data.rational);
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

                /* 位数熔断检查（含溢出检测 + 信任降级） */
                bit_burning_check_result(result, "add");
                return result;
            }
            case ALGEBRAIC: {
                Algebraic *alg = algebraic_add(a->data.algebraic, b->data.algebraic);
                if (!alg)
                    return NULL;
                SymbolicCoord *result =
                    symbolic_coord_create_algebraic(&alg->minimal_poly, alg->left_bound, alg->right_bound);
                algebraic_destroy(alg);
                if (result)
                    result->trust = (a->trust < b->trust) ? a->trust : b->trust;
                return _symbolic_coord_degrade_check_algebraic(result);
            }
            case QUADRATIC: {
                Quadratic *q = quadratic_add(a->data.quadratic, b->data.quadratic);
                if (!q) {
                    /* Different sqrt(n) - promote to algebraic */
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
                lv_free((void**)&q); /* quadratic_create copies the rationals */
                if (result)
                    result->trust = (a->trust < b->trust) ? a->trust : b->trust;
                return result;
            }
            default:
                return NULL;
        }
    }

    /* Cross-type operations */

    /* Rational + Algebraic = Algebraic */
    if (a->type == RATIONAL && b->type == ALGEBRAIC) {
        SymbolicCoord *a_alg = rational_to_algebraic(a);
        if (!a_alg)
            return NULL;
        SymbolicCoord *result = symbolic_coord_add(a_alg, b);
        symbolic_coord_destroy(a_alg);
        return result;
    }
    if (a->type == ALGEBRAIC && b->type == RATIONAL) {
        SymbolicCoord *b_alg = rational_to_algebraic(b);
        if (!b_alg)
            return NULL;
        SymbolicCoord *result = symbolic_coord_add(a, b_alg);
        symbolic_coord_destroy(b_alg);
        return result;
    }

    /* Rational + Quadratic = Quadratic */
    if (a->type == RATIONAL && b->type == QUADRATIC) {
        /* Convert rational to quadratic with same n as b */
        SymbolicCoord *a_quad = rational_to_quadratic_with_n(a, b->data.quadratic->n);
        if (!a_quad)
            return NULL;
        SymbolicCoord *result = symbolic_coord_add(a_quad, b);
        symbolic_coord_destroy(a_quad);
        return result;
    }
    if (a->type == QUADRATIC && b->type == RATIONAL) {
        /* Convert rational to quadratic with same n as a */
        SymbolicCoord *b_quad = rational_to_quadratic_with_n(b, a->data.quadratic->n);
        if (!b_quad)
            return NULL;
        SymbolicCoord *result = symbolic_coord_add(a, b_quad);
        symbolic_coord_destroy(b_quad);
        return result;
    }

    /* Quadratic + Algebraic = Algebraic (promote Quadratic) */
    if (a->type == QUADRATIC && b->type == ALGEBRAIC) {
        SymbolicCoord *a_alg = quadratic_to_algebraic(a);
        if (!a_alg)
            return NULL;
        SymbolicCoord *result = symbolic_coord_add(a_alg, b);
        symbolic_coord_destroy(a_alg);
        return result;
    }
    if (a->type == ALGEBRAIC && b->type == QUADRATIC) {
        SymbolicCoord *b_alg = quadratic_to_algebraic(b);
        if (!b_alg)
            return NULL;
        SymbolicCoord *result = symbolic_coord_add(a, b_alg);
        symbolic_coord_destroy(b_alg);
        return result;
    }

    return NULL;
}

SymbolicCoord *symbolic_coord_subtract(const SymbolicCoord *a, const SymbolicCoord *b) {
    /* Update overflow context */
    g_overflow_context.left_type = a->type;
    g_overflow_context.right_type = b->type;
    g_overflow_context.last_operation = "subtract";

    /* Transcendental - anything */
    if (a->type == TRANSCENDENTAL || b->type == TRANSCENDENTAL) {
        const SymbolicCoord *trans_coord = (a->type == TRANSCENDENTAL) ? a : b;
        const SymbolicCoord *other_coord = (a->type == TRANSCENDENTAL) ? b : a;
        bool inverted = (b->type == TRANSCENDENTAL); /* true: a - transcendental */

        if (other_coord->type == RATIONAL) {
            if (inverted) {
                /* rational - transcendental: 无法精确表示，标记为 out_of_scope */
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

            /* Transcendental - Rational -> TRANS_EXPR_ADD_RATIONAL with negated rational */
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
            /* Transcendental - Algebraic/Quadratic -> out_of_scope */
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

        /* Transcendental - Transcendental: merge if same base, else out_of_scope */
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
                /* Same base: check if both are pure multiplicative (MUL or bare) */
                bool a_is_mul = !ta->expr || ta->expr->expr_type == TRANS_EXPR_MUL_RATIONAL;
                bool b_is_mul = !tb->expr || tb->expr->expr_type == TRANS_EXPR_MUL_RATIONAL;

                if (a_is_mul && b_is_mul) {
                    /* Both are coeff*base form: (ca*base) - (cb*base) = (ca-cb)*base */
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
                        return NULL;
                    }
                } else {
                    /* At least one has ADD form: cannot simplify, out_of_scope */
                    expr->expr_type = TRANS_EXPR_ADD_ALGEBRAIC;
                    expr->rational_operand = NULL;
                    expr->out_of_scope = true;
                }
            } else {
                /* Different base (e.g. pi - e): out_of_scope */
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

    /* Same type operations */
    if (a->type == b->type) {
        switch (a->type) {
            case RATIONAL: {
                Rational *r = rational_subtract(a->data.rational, b->data.rational);
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

                /* 位数熔断检查（含溢出检测 + 信任降级） */
                bit_burning_check_result(result, "subtract");
                return result;
            }
            case ALGEBRAIC: {
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
            case QUADRATIC: {
                Quadratic *q = quadratic_subtract(a->data.quadratic, b->data.quadratic);
                if (!q) {
                    /* Different sqrt(n) - promote to algebraic */
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
                lv_free((void**)&q);  /* lv_malloc分配 */
                if (result)
                    result->trust = (a->trust < b->trust) ? a->trust : b->trust;
                return result;
            }
            default:
                return NULL;
        }
    }

    /* Cross-type operations */

    /* Rational - Algebraic = Algebraic */
    if (a->type == RATIONAL && b->type == ALGEBRAIC) {
        SymbolicCoord *a_alg = rational_to_algebraic(a);
        if (!a_alg)
            return NULL;
        SymbolicCoord *result = symbolic_coord_subtract(a_alg, b);
        symbolic_coord_destroy(a_alg);
        return result;
    }
    if (a->type == ALGEBRAIC && b->type == RATIONAL) {
        SymbolicCoord *b_alg = rational_to_algebraic(b);
        if (!b_alg)
            return NULL;
        SymbolicCoord *result = symbolic_coord_subtract(a, b_alg);
        symbolic_coord_destroy(b_alg);
        return result;
    }

    /* Rational - Quadratic = Quadratic */
    if (a->type == RATIONAL && b->type == QUADRATIC) {
        SymbolicCoord *a_quad = rational_to_quadratic_with_n(a, b->data.quadratic->n);
        if (!a_quad)
            return NULL;
        SymbolicCoord *result = symbolic_coord_subtract(a_quad, b);
        symbolic_coord_destroy(a_quad);
        return result;
    }
    if (a->type == QUADRATIC && b->type == RATIONAL) {
        SymbolicCoord *b_quad = rational_to_quadratic_with_n(b, a->data.quadratic->n);
        if (!b_quad)
            return NULL;
        SymbolicCoord *result = symbolic_coord_subtract(a, b_quad);
        symbolic_coord_destroy(b_quad);
        return result;
    }

    /* Quadratic - Algebraic = Algebraic */
    if (a->type == QUADRATIC && b->type == ALGEBRAIC) {
        SymbolicCoord *a_alg = quadratic_to_algebraic(a);
        if (!a_alg)
            return NULL;
        SymbolicCoord *result = symbolic_coord_subtract(a_alg, b);
        symbolic_coord_destroy(a_alg);
        return result;
    }
    if (a->type == ALGEBRAIC && b->type == QUADRATIC) {
        SymbolicCoord *b_alg = quadratic_to_algebraic(b);
        if (!b_alg)
            return NULL;
        SymbolicCoord *result = symbolic_coord_subtract(a, b_alg);
        symbolic_coord_destroy(b_alg);
        return result;
    }

    return NULL;
}

SymbolicCoord *symbolic_coord_multiply(const SymbolicCoord *a, const SymbolicCoord *b) {
    /* Update overflow context */
    g_overflow_context.left_type = a->type;
    g_overflow_context.right_type = b->type;
    g_overflow_context.last_operation = "multiply";

    /* Transcendental * anything */
    if (a->type == TRANSCENDENTAL || b->type == TRANSCENDENTAL) {
        const SymbolicCoord *trans_coord = (a->type == TRANSCENDENTAL) ? a : b;
        const SymbolicCoord *other_coord = (a->type == TRANSCENDENTAL) ? b : a;

        if (other_coord->type == RATIONAL) {
            /* Transcendental * Rational -> TRANS_EXPR_MUL_RATIONAL */
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
            /* Transcendental * Algebraic/Quadratic -> out_of_scope */
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

        /* Transcendental * Transcendental: out_of_scope in general */
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

    /* Same type operations */
    if (a->type == b->type) {
        switch (a->type) {
            case RATIONAL: {
                Rational *r = rational_multiply(a->data.rational, b->data.rational);
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

                /* 位数熔断检查（含溢出检测 + 信任降级） */
                bit_burning_check_result(result, "multiply");
                return result;
            }
            case ALGEBRAIC: {
                Algebraic *alg = algebraic_multiply(a->data.algebraic, b->data.algebraic);
                if (!alg)
                    return NULL;
                SymbolicCoord *result =
                    symbolic_coord_create_algebraic(&alg->minimal_poly, alg->left_bound, alg->right_bound);
                algebraic_destroy(alg);
                if (result)
                    result->trust = (a->trust < b->trust) ? a->trust : b->trust;
                return _symbolic_coord_degrade_check_algebraic(result);
            }
            case QUADRATIC: {
                Quadratic *q = quadratic_multiply(a->data.quadratic, b->data.quadratic);
                if (!q) {
                    /* Different sqrt(n) - promote to algebraic */
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
                lv_free((void**)&q);  /* lv_malloc分配 */
                if (result)
                    result->trust = (a->trust < b->trust) ? a->trust : b->trust;
                return result;
            }
            default:
                return NULL;
        }
    }

    /* Cross-type operations */

    /* Rational * Algebraic = Algebraic */
    if (a->type == RATIONAL && b->type == ALGEBRAIC) {
        SymbolicCoord *a_alg = rational_to_algebraic(a);
        if (!a_alg)
            return NULL;
        SymbolicCoord *result = symbolic_coord_multiply(a_alg, b);
        symbolic_coord_destroy(a_alg);
        return result;
    }
    if (a->type == ALGEBRAIC && b->type == RATIONAL) {
        SymbolicCoord *b_alg = rational_to_algebraic(b);
        if (!b_alg)
            return NULL;
        SymbolicCoord *result = symbolic_coord_multiply(a, b_alg);
        symbolic_coord_destroy(b_alg);
        return result;
    }

    /* Rational * Quadratic = Quadratic */
    if (a->type == RATIONAL && b->type == QUADRATIC) {
        /* r * (a + b*sqrt(n)) = r*a + r*b*sqrt(n) */
        Rational *new_a = rational_multiply(a->data.rational, b->data.quadratic->a);
        Rational *new_b = rational_multiply(a->data.rational, b->data.quadratic->b);
        SymbolicCoord *result = symbolic_coord_create_quadratic(new_a, new_b, b->data.quadratic->n);
        if (result)
            result->trust = (a->trust < b->trust) ? a->trust : b->trust;
        return result;
    }
    if (a->type == QUADRATIC && b->type == RATIONAL) {
        Rational *new_a = rational_multiply(a->data.quadratic->a, b->data.rational);
        Rational *new_b = rational_multiply(a->data.quadratic->b, b->data.rational);
        SymbolicCoord *result = symbolic_coord_create_quadratic(new_a, new_b, a->data.quadratic->n);
        if (result)
            result->trust = (a->trust < b->trust) ? a->trust : b->trust;
        return result;
    }

    /* Quadratic * Algebraic = Algebraic */
    if (a->type == QUADRATIC && b->type == ALGEBRAIC) {
        SymbolicCoord *a_alg = quadratic_to_algebraic(a);
        if (!a_alg)
            return NULL;
        SymbolicCoord *result = symbolic_coord_multiply(a_alg, b);
        symbolic_coord_destroy(a_alg);
        return result;
    }
    if (a->type == ALGEBRAIC && b->type == QUADRATIC) {
        SymbolicCoord *b_alg = quadratic_to_algebraic(b);
        if (!b_alg)
            return NULL;
        SymbolicCoord *result = symbolic_coord_multiply(a, b_alg);
        symbolic_coord_destroy(b_alg);
        return result;
    }

    return NULL;
}

/**
 * 符号坐标除法：计算 a / b。
 *
 * 支持所有类型的跨类型运算。
 *
 * @param a 被除数（不能为 NULL）
 * @param b 除数（不能为 NULL，且不能为零）
 * @return 新的符号坐标对象，失败时返回 NULL；调用者需负责释放
 */
SymbolicCoord *symbolic_coord_divide(const SymbolicCoord *a, const SymbolicCoord *b) {
    /* Update overflow context */
    g_overflow_context.left_type = a->type;
    g_overflow_context.right_type = b->type;
    g_overflow_context.last_operation = "divide";

    /* Transcendental / anything */
    if (a->type == TRANSCENDENTAL || b->type == TRANSCENDENTAL) {
        const SymbolicCoord *trans_coord = (a->type == TRANSCENDENTAL) ? a : b;
        const SymbolicCoord *other_coord = (a->type == TRANSCENDENTAL) ? b : a;
        bool inverted = (b->type == TRANSCENDENTAL);

        if (other_coord->type == RATIONAL) {
            /* Transcendental / Rational -> TRANS_EXPR_MUL_RATIONAL with inverted rational */
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
                /* b is transcendental, a is rational: a / t = a * (1/t), not representable */
                transcendental_destroy(t);
                lv_free((void **) &expr);
                return NULL;
            } else {
                /* a is transcendental, b is rational: t / r = t * (1/r) */
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
            /* Transcendental / Algebraic/Quadratic -> out_of_scope */
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

        /* Transcendental / Transcendental: same base -> rational, else out_of_scope */
        {
            const Transcendental *ta = a->data.transcendental;
            const Transcendental *tb = b->data.transcendental;
            const char *base_a = ta->expr ? ta->expr->base_name : ta->name;
            const char *base_b = tb->expr ? tb->expr->base_name : tb->name;

            if (strcmp(base_a, base_b) == 0) {
                /* Same base: (ca*base) / (cb*base) = ca/cb (rational) */
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

            /* Different base or non-mul form: out_of_scope */
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

    /* Same type operations */
    if (a->type == b->type) {
        switch (a->type) {
            case RATIONAL: {
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

                /* 位数熔断检查（含溢出检测 + 信任降级） */
                bit_burning_check_result(result, "divide");
                return result;
            }
            case ALGEBRAIC: {
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
            case QUADRATIC: {
                Quadratic *q = quadratic_divide(a->data.quadratic, b->data.quadratic);
                if (!q) {
                    /* Different sqrt(n) or division by zero - promote to algebraic */
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
                lv_free((void**)&q);  /* lv_malloc分配 */
                if (result)
                    result->trust = (a->trust < b->trust) ? a->trust : b->trust;
                return result;
            }
            default:
                return NULL;
        }
    }

    /* Cross-type operations */

    /* Rational / Algebraic = Algebraic */
    if (a->type == RATIONAL && b->type == ALGEBRAIC) {
        SymbolicCoord *a_alg = rational_to_algebraic(a);
        if (!a_alg)
            return NULL;
        SymbolicCoord *result = symbolic_coord_divide(a_alg, b);
        symbolic_coord_destroy(a_alg);
        return result;
    }
    if (a->type == ALGEBRAIC && b->type == RATIONAL) {
        SymbolicCoord *b_alg = rational_to_algebraic(b);
        if (!b_alg)
            return NULL;
        SymbolicCoord *result = symbolic_coord_divide(a, b_alg);
        symbolic_coord_destroy(b_alg);
        return result;
    }

    /* Rational / Quadratic = Quadratic */
    if (a->type == RATIONAL && b->type == QUADRATIC) {
        /* r / (a + b*sqrt(n)) = r * (a - b*sqrt(n)) / (a^2 - b^2*n) */
        SymbolicCoord *r_quad = rational_to_quadratic_with_n(a, b->data.quadratic->n);
        if (!r_quad)
            return NULL;
        SymbolicCoord *result = symbolic_coord_divide(r_quad, b);
        symbolic_coord_destroy(r_quad);
        return result;
    }
    if (a->type == QUADRATIC && b->type == RATIONAL) {
        /* (a + b*sqrt(n)) / r = a/r + (b/r)*sqrt(n) */
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

    /* Quadratic / Algebraic = Algebraic */
    if (a->type == QUADRATIC && b->type == ALGEBRAIC) {
        SymbolicCoord *a_alg = quadratic_to_algebraic(a);
        if (!a_alg)
            return NULL;
        SymbolicCoord *result = symbolic_coord_divide(a_alg, b);
        symbolic_coord_destroy(a_alg);
        return result;
    }
    if (a->type == ALGEBRAIC && b->type == QUADRATIC) {
        SymbolicCoord *b_alg = quadratic_to_algebraic(b);
        if (!b_alg)
            return NULL;
        SymbolicCoord *result = symbolic_coord_divide(a, b_alg);
        symbolic_coord_destroy(b_alg);
        return result;
    }

    return NULL;
}

/**
 * 将符号坐标序列化为字符串。
 *
 * @param coord 符号坐标对象（不能为 NULL）
 * @return 新分配的字符串，失败时返回 NULL
 */
char *symbolic_coord_serialize(const SymbolicCoord *coord) {
    switch (coord->type) {
        case RATIONAL:
            return rational_serialize(coord->data.rational);
        case ALGEBRAIC:
            return algebraic_serialize(coord->data.algebraic);
        case QUADRATIC:
            return quadratic_serialize(coord->data.quadratic);
        case TRANSCENDENTAL:
            return transcendental_serialize(coord->data.transcendental);
        default:
            return NULL; /* 未知类型无法序列化 */
    }
    return NULL;
}

/* ============================================================
 * Additional Utility Functions
 * ============================================================ */

/* Create a deep copy of a SymbolicCoord */
SymbolicCoord *symbolic_coord_copy(const SymbolicCoord *src) {
    if (!src)
        return NULL;

    SymbolicCoord *dst = lv_calloc(1, sizeof(SymbolicCoord));
    if (!dst)
        return NULL;

    dst->type = src->type;
    dst->trust = src->trust;
    dst->cache_valid = false;  /* 复制品缓存初始无效，首次访问时重新计算 */
    dst->cached_value = 0.0;

    switch (src->type) {
        case RATIONAL:
            dst->data.rational = rational_copy(src->data.rational);
            break;
        case ALGEBRAIC: {
            dst->data.algebraic = algebraic_create((mpz_poly_t *) &src->data.algebraic->minimal_poly,
                                                   src->data.algebraic->left_bound, src->data.algebraic->right_bound);
            if (dst->data.algebraic && src->data.algebraic->cached_rational) {
                dst->data.algebraic->cached_rational = rational_copy(src->data.algebraic->cached_rational);
            }
            break;
        }
        case QUADRATIC: {
            Rational *a = rational_copy(src->data.quadratic->a);
            Rational *b = rational_copy(src->data.quadratic->b);
            dst->data.quadratic = quadratic_create(a, b, src->data.quadratic->n);
            break;
        }
        case TRANSCENDENTAL: {
            dst->data.transcendental = transcendental_create(src->data.transcendental->name);
            /* Deep copy the expression tree if present */
            if (dst->data.transcendental && src->data.transcendental->expr) {
                TranscendentalExpr *src_expr = src->data.transcendental->expr;
                TranscendentalExpr *dst_expr = lv_calloc(1, sizeof(TranscendentalExpr));
                if (dst_expr) {
                    dst_expr->expr_type = src_expr->expr_type;
                    /* 使用 lv_strlcpy 替代不安全的 strncpy（自动保证零终止） */
                    lv_strlcpy(dst_expr->base_name, src_expr->base_name, sizeof(dst_expr->base_name));
                    dst_expr->rational_operand =
                        src_expr->rational_operand ? rational_copy(src_expr->rational_operand) : NULL;
                    dst_expr->out_of_scope = src_expr->out_of_scope;
                    dst->data.transcendental->expr = dst_expr;
                }
            }
            break;
        }
    }

    bool copy_ok = false;
    switch (src->type) {
        case RATIONAL:
            copy_ok = (dst->data.rational != NULL);
            break;
        case ALGEBRAIC:
            copy_ok = (dst->data.algebraic != NULL);
            break;
        case QUADRATIC:
            copy_ok = (dst->data.quadratic != NULL);
            break;
        case TRANSCENDENTAL:
            copy_ok = (dst->data.transcendental != NULL);
            break;
    }
    if (!copy_ok) {
        symbolic_coord_destroy(dst);
        return NULL;
    }

    return dst;
}

/**
 * 检查符号坐标是否为零。
 *
 * @param coord 符号坐标对象（可为 NULL，NULL 视为零）
 * @return true 表示为零，false 表示非零
 */
bool symbolic_coord_is_zero(const SymbolicCoord *coord) {
    if (!coord)
        return true;
    switch (coord->type) {
        case RATIONAL:
            return mpq_cmp_ui(coord->data.rational->value, 0, 1) == 0;
        case ALGEBRAIC: {
            Algebraic *a = coord->data.algebraic;
            if (a->cached_rational) {
                return mpq_cmp_ui(a->cached_rational->value, 0, 1) == 0;
            }
            /* 检查区间是否包含零 */
            return (a->left_bound <= 0 && a->right_bound >= 0);
        }
        case QUADRATIC: {
            Quadratic *q = coord->data.quadratic;
            return is_rational_zero(q->a) && is_rational_zero(q->b);
        }
        case TRANSCENDENTAL:
            return false; /* 超越数永远不为零 */
    }
    return false;
}

/**
 * 检查 SymbolicCoord 是否为正数。
 *
 * @param coord SymbolicCoord 对象
 * @return 如果值为正返回 true，否则返回 false
 */
bool symbolic_coord_is_positive(const SymbolicCoord *coord) {
    return symbolic_coord_to_double(coord) > 0;
}

/**
 * 检查 SymbolicCoord 是否为负数。
 *
 * @param coord SymbolicCoord 对象
 * @return 如果值为负返回 true，否则返回 false
 */
bool symbolic_coord_is_negative(const SymbolicCoord *coord) {
    return symbolic_coord_to_double(coord) < 0;
}

/* ============================================================
 * Numerical Downgrade to AMBER (Permanent Downgrade)
 * 
 * According to design_v2.9.md Section 1.5:
 * When user chooses "永久降级为数值假设" after 3+ circuit trips,
 * the coordinate is permanently marked as AMBER with numerical approximation.
 * ============================================================ */

/**
 * 将符号坐标降级为 AMBER 信任级别。
 *
 * 当用户选择"永久降级为数值假设"后（连续 3+ 次熔断触发），
 * 符号坐标被永久标记为 AMBER，并使用数值近似。
 *
 * @param coord       原符号坐标（不能为 NULL）
 * @param precision   精度阈值
 * @param declaration 声明信息（可为 NULL）
 * @return 降级后的新符号坐标对象，失败时返回 NULL；调用者需负责释放
 */
SymbolicCoord *symbolic_coord_downgrade_to_amber(const SymbolicCoord *coord, double precision,
                                                 const char *declaration) {
    if (!coord)
        return NULL;

    /* Create a new coordinate with AMBER trust level */
    SymbolicCoord *result = symbolic_coord_copy(coord);
    if (!result)
        return NULL;

    /* Mark as AMBER (numerical assumption) */
    result->trust = TRUST_AMBER;

    /* Store the precision threshold */
    /* Note: In a full implementation, this would be stored in a dedicated field */
    /* For now, we log it */
    fprintf(stderr, "[AMBER DOWNGRADE] Precision: %.15g, Declaration: %s\n", precision,
            declaration ? declaration : "(none)");

    /* The numerical value is accessible via symbolic_coord_to_double() */
    double numerical_value = symbolic_coord_to_double(result);
    fprintf(stderr, "[AMBER DOWNGRADE] Numerical value: %.15g\n", numerical_value);

    return result;
}

/**
 * 检查符号坐标是否标记为 AMBER（数值假设）。
 *
 * @param coord 符号坐标对象（可为 NULL）
 * @return true 表示为 AMBER，false 表示其他
 */
bool symbolic_coord_is_amber(const SymbolicCoord *coord) {
    if (!coord)
        return false;
    return coord->trust == TRUST_AMBER;
}

/*
 * Get the trust color of a coordinate
 */
TrustColor symbolic_coord_get_trust(const SymbolicCoord *coord) {
    if (!coord)
        return TRUST_GREEN;
    return coord->trust;
}

/**
 * 设置符号坐标的信任颜色。
 *
 * @param coord 符号坐标对象（可为 NULL，无操作）
 * @param trust 信任颜色
 */
void symbolic_coord_set_trust(SymbolicCoord *coord, TrustColor trust) {
    if (coord) {
        coord->trust = trust;
        /* 信任颜色变更可能影响数值含义，使缓存失效 */
        symbolic_coord_invalidate_cache(coord);
    }
}

/* ============================================================
 * Unary Negation
 * ============================================================ */

SymbolicCoord *symbolic_coord_negate(const SymbolicCoord *coord) {
    if (!coord)
        return NULL;

    switch (coord->type) {
        case RATIONAL: {
            Rational *neg = rational_create(0, 1);
            mpq_neg(neg->value, coord->data.rational->value);
            SymbolicCoord *result = symbolic_coord_create_rational(0, 1);
            if (result) {
                rational_destroy(result->data.rational);
                result->data.rational = neg;
                result->trust = coord->trust;
            } else {
                rational_destroy(neg);
            }
            return result;
        }
        case ALGEBRAIC: {
            /* Negate algebraic: replace poly P(x) with P(-x), swap bounds */
            Algebraic *a = coord->data.algebraic;
            mpz_poly_t neg_poly;
            mpz_poly_init(&neg_poly);

            /* P(-x): negate odd-degree coefficients */
            if (a->minimal_poly.degree >= 0) {
                neg_poly.degree = a->minimal_poly.degree;
                neg_poly.coeffs = malloc((neg_poly.degree + 1) * sizeof(mpz_t));
                if (!neg_poly.coeffs) {
                    mpz_poly_clear(&neg_poly);
                    return NULL;
                }
                for (int i = 0; i <= neg_poly.degree; i++) {
                    mpz_init(neg_poly.coeffs[i]);
                    if (i % 2 == 1) {
                        mpz_neg(neg_poly.coeffs[i], a->minimal_poly.coeffs[i]);
                    } else {
                        mpz_set(neg_poly.coeffs[i], a->minimal_poly.coeffs[i]);
                    }
                }
            }

            double new_left = -a->right_bound;
            double new_right = -a->left_bound;

            SymbolicCoord *result = symbolic_coord_create_algebraic(&neg_poly, new_left, new_right);
            mpz_poly_clear(&neg_poly);
            if (result)
                result->trust = coord->trust;
            return result;
        }
        case QUADRATIC: {
            Quadratic *q = coord->data.quadratic;
            Rational *neg_a = rational_create(0, 1);
            mpq_neg(neg_a->value, q->a->value);
            Rational *neg_b = rational_create(0, 1);
            mpq_neg(neg_b->value, q->b->value);
            SymbolicCoord *result = symbolic_coord_create_quadratic(neg_a, neg_b, q->n);
            if (result)
                result->trust = coord->trust;
            return result;
        }
        case TRANSCENDENTAL: {
            /* Negate transcendental: flip the sign of rational_operand */
            const Transcendental *src_t = coord->data.transcendental;
            const char *base = src_t->expr ? src_t->expr->base_name : src_t->name;

            Transcendental *t = transcendental_create(base);
            if (!t)
                return NULL;

            TranscendentalExpr *expr = lv_calloc(1, sizeof(TranscendentalExpr));
            if (!expr) {
                transcendental_destroy(t);
                return NULL;
            }
            expr->out_of_scope = src_t->expr ? src_t->expr->out_of_scope : false;
            /* 使用 lv_strlcpy 替代不安全的 strncpy（自动保证零终止） */
            lv_strlcpy(expr->base_name, base, sizeof(expr->base_name));
            if (src_t->expr && src_t->expr->rational_operand) {
                /* Negate the existing rational operand */
                expr->expr_type = src_t->expr->expr_type;
                Rational *neg_r = rational_create(0, 1);
                if (!neg_r) {
                    lv_free((void **) &expr);
                    transcendental_destroy(t);
                    return NULL;
                }
                mpq_neg(neg_r->value, src_t->expr->rational_operand->value);
                expr->rational_operand = neg_r;
            } else {
                /* Bare constant: -pi = -1*pi */
                expr->expr_type = TRANS_EXPR_MUL_RATIONAL;
                expr->rational_operand = rational_create(-1, 1);
            }

            if (!expr->rational_operand) {
                lv_free((void **) &expr);
                transcendental_destroy(t);
                return NULL;
            }

            t->expr = expr;

            SymbolicCoord *result = lv_calloc(1, sizeof(SymbolicCoord));
            if (!result) {
                transcendental_destroy(t);
                return NULL;
            }
            result->type = TRANSCENDENTAL;
            result->trust = coord->trust;
            result->data.transcendental = t;
            return result;
        }
    }
    return NULL;
}

/* ============================================================
 * Power and Square Root Operations
 * ============================================================ */

/* Forward declaration: mpz_perfect_sqrt is defined below */
mpz_t *mpz_perfect_sqrt(mpz_t n);

/**
 * @brief 计算大整数的精确平方根（如果它是完全平方数）
 *
 * 使用 GMP 的 mpz_perfect_square_p() 检查 n 是否为完全平方数，
 * 如果是则分配一个新的 mpz_t 并计算其整数平方根。
 *
 * @param n 输入大整数
 * @return 如果 n 是完全平方数，返回新分配的 mpz_t*（调用者负责释放）；
 *         否则返回 NULL
 */
mpz_t *mpz_perfect_sqrt(mpz_t n) {
    if (mpz_sgn(n) < 0)
        return NULL;
    if (!mpz_perfect_square_p(n))
        return NULL;
    mpz_t *result = (mpz_t *)lv_calloc(1, sizeof(mpz_t));
    if (!result)
        return NULL;
    mpz_init(*result);
    mpz_sqrt(*result, n);
    return result;
}

/*
 * Compute base^exponent where exponent is a non-negative integer.
 *
 * RATIONAL:   Uses GMP mpz_pow_ui for exact computation.
 * QUADRATIC:  (a+b*sqrt(n))^k via repeated squaring.
 * ALGEBRAIC:  Uses Newton evaluation + resultant for minimal poly,
 *             or falls back to double approximation with algebraic creation.
 * TRANSCENDENTAL: Marked as out_of_scope, returns NULL.
 */
/* 指数上限：防止极大指数导致内存耗尽或计算时间过长 */
#define SYMBOLIC_COORD_POW_MAX_EXPONENT 1000

SymbolicCoord *symbolic_coord_pow(const SymbolicCoord *base, unsigned int exponent) {
    if (!base)
        return NULL;

    /* 指数上限检查：防止 DoS */
    if (exponent > SYMBOLIC_COORD_POW_MAX_EXPONENT) {
        return NULL;
    }

    /* base^0 = 1 for any type */
    if (exponent == 0) {
        return symbolic_coord_create_rational(1, 1);
    }

    /* base^1 = base (copy) */
    if (exponent == 1) {
        return symbolic_coord_copy(base);
    }

    switch (base->type) {
        case RATIONAL: {
            /* Exact rational power using GMP */
            const Rational *r = base->data.rational;
            mpz_t num_pow, den_pow;
            mpz_init(num_pow);
            mpz_init(den_pow);

            mpz_pow_ui(num_pow, mpq_numref(r->value), exponent);
            mpz_pow_ui(den_pow, mpq_denref(r->value), exponent);

            Rational *result_r = rational_create_from_mpz(num_pow, den_pow);
            mpz_clear(num_pow);
            mpz_clear(den_pow);

            if (!result_r)
                return NULL;

            SymbolicCoord *result = symbolic_coord_create_rational(0, 1);
            if (result) {
                rational_destroy(result->data.rational);
                result->data.rational = result_r;
                result->trust = base->trust;
            } else {
                rational_destroy(result_r);
            }
            return result;
        }

        case QUADRATIC: {
            /* (a + b*sqrt(n))^k via repeated squaring */
            const Quadratic *q = base->data.quadratic;

            /* Start with identity: 1 + 0*sqrt(n) */
            Rational *res_a = rational_create(1, 1);
            Rational *res_b = rational_create(0, 1);
            unsigned int res_n = q->n;

            /* Current power: base */
            Rational *cur_a = rational_copy(q->a);
            Rational *cur_b = rational_copy(q->b);

            unsigned int k = exponent;
            while (k > 0) {
                if (k & 1) {
                    /* result *= current */
                    /* (res_a + res_b*sqrt(n)) * (cur_a + cur_b*sqrt(n))
                     * = (res_a*cur_a + res_b*cur_b*n) + (res_a*cur_b + res_b*cur_a)*sqrt(n) */
                    Rational *t1 = rational_multiply(res_a, cur_a);
                    Rational *b1b2 = rational_multiply(res_b, cur_b);
                    Rational *n_rat = rational_create(res_n, 1);
                    Rational *t2 = rational_multiply(b1b2, n_rat);
                    Rational *new_a = rational_add(t1, t2);

                    Rational *a1b2 = rational_multiply(res_a, cur_b);
                    Rational *a2b1 = rational_multiply(res_b, cur_a);
                    Rational *new_b = rational_add(a1b2, a2b1);

                    rational_destroy(res_a);
                    rational_destroy(res_b);
                    res_a = new_a;
                    res_b = new_b;

                    rational_destroy(t1);
                    rational_destroy(t2);
                    rational_destroy(b1b2);
                    rational_destroy(n_rat);
                    rational_destroy(a1b2);
                    rational_destroy(a2b1);
                }
                k >>= 1;
                if (k > 0) {
                    /* current = current^2 */
                    /* (cur_a + cur_b*sqrt(n))^2
                     * = (cur_a^2 + cur_b^2*n) + 2*cur_a*cur_b*sqrt(n) */
                    Rational *a_sq = rational_multiply(cur_a, cur_a);
                    Rational *b_sq = rational_multiply(cur_b, cur_b);
                    Rational *n_rat = rational_create(res_n, 1);
                    Rational *b_sq_n = rational_multiply(b_sq, n_rat);
                    Rational *new_cur_a = rational_add(a_sq, b_sq_n);

                    Rational *two = rational_create(2, 1);
                    Rational *ab = rational_multiply(cur_a, cur_b);
                    Rational *new_cur_b = rational_multiply(two, ab);

                    rational_destroy(cur_a);
                    rational_destroy(cur_b);
                    cur_a = new_cur_a;
                    cur_b = new_cur_b;

                    rational_destroy(a_sq);
                    rational_destroy(b_sq);
                    rational_destroy(n_rat);
                    rational_destroy(b_sq_n);
                    rational_destroy(two);
                    rational_destroy(ab);
                }
            }

            rational_destroy(cur_a);
            rational_destroy(cur_b);

            SymbolicCoord *result = symbolic_coord_create_quadratic(res_a, res_b, res_n);
            if (result)
                result->trust = base->trust;
            else {
                rational_destroy(res_a);
                rational_destroy(res_b);
            }
            return result;
        }

        case ALGEBRAIC: {
            /* For algebraic numbers, compute exact power when possible.
             *
             * Strategy:
             * - exponent 0: already handled above (returns 1)
             * - exponent 1: already handled above (returns copy)
             * - exponent 2, degree <= 2: use analytic formula for quadratic surds
             * - higher: numerical approach with rationalization attempt,
             *   then construct new algebraic number from numerical value
             *
             * For the minimal polynomial of alpha^k given minpoly P(alpha):
             * Use resultant_y(P(y), y^k - x) which eliminates y. */
            Algebraic *a = base->data.algebraic;

            /* If the algebraic number is actually a cached rational, use rational path */
            if (a->cached_rational) {
                SymbolicCoord *rat_coord = symbolic_coord_create_rational(0, 1);
                if (rat_coord) {
                    rational_destroy(rat_coord->data.rational);
                    rat_coord->data.rational = rational_copy(a->cached_rational);
                    rat_coord->trust = base->trust;
                    SymbolicCoord *result = symbolic_coord_pow(rat_coord, exponent);
                    symbolic_coord_destroy(rat_coord);
                    return result;
                }
            }

            /* Try rationalization first */
            algebraic_try_rationalize(a);
            if (a->cached_rational) {
                SymbolicCoord *rat_coord = symbolic_coord_create_rational(0, 1);
                if (rat_coord) {
                    rational_destroy(rat_coord->data.rational);
                    rat_coord->data.rational = rational_copy(a->cached_rational);
                    rat_coord->trust = base->trust;
                    SymbolicCoord *result = symbolic_coord_pow(rat_coord, exponent);
                    symbolic_coord_destroy(rat_coord);
                    return result;
                }
            }

            /* Refine bounds for better precision */
            refine_algebraic_bounds(a, 20);
            double val = (a->left_bound + a->right_bound) / 2.0;

            /* For exponent 2 with degree <= 2: use analytic formula */
            if (exponent == 2 && a->minimal_poly.degree <= 2) {
                if (a->minimal_poly.degree == 1) {
                    /* Degree 1: effectively rational, already handled above via
                     * rationalization. But as a fallback, compute directly. */
                    /* P(x) = c1*x + c0 => root = -c0/c1 */
                    mpz_t neg_c0;
                    mpz_init(neg_c0);
                    mpz_neg(neg_c0, a->minimal_poly.coeffs[0]);
                    mpq_t root;
                    mpq_init(root);
                    mpq_set_num(root, neg_c0);
                    mpq_set_den(root, a->minimal_poly.coeffs[1]);
                    mpq_canonicalize(root);
                    mpz_clear(neg_c0);

                    /* Square the rational: (num/den)^2 = num^2/den^2 */
                    mpz_t num_sq, den_sq;
                    mpz_init(num_sq);
                    mpz_init(den_sq);
                    mpz_pow_ui(num_sq, mpq_numref(root), 2);
                    mpz_pow_ui(den_sq, mpq_denref(root), 2);
                    mpq_clear(root);

                    Rational *result_r = rational_create_from_mpz(num_sq, den_sq);
                    mpz_clear(num_sq);
                    mpz_clear(den_sq);

                    if (result_r) {
                        SymbolicCoord *result = symbolic_coord_create_rational(0, 1);
                        if (result) {
                            rational_destroy(result->data.rational);
                            result->data.rational = result_r;
                            result->trust = base->trust;
                        } else {
                            rational_destroy(result_r);
                        }
                        return result;
                    }
                } else if (a->minimal_poly.degree == 2) {
                    /* Degree 2: construct minimal polynomial of alpha^2 via resultant.
                     *
                     * For P(y) = c2*y^2 + c1*y + c0, the minimal polynomial of alpha^2 is:
                     * resultant_y(P(y), y^2 - x) = c2^2 * x^2 + (c1^2 - 2*c0*c2) * x + c0^2
                     * (Sylvester matrix determinant)
                     *
                     * This handles cases like sqrt(2)^2 = 2 exactly: the resultant
                     * polynomial will have 2 as a root, and algebraic_try_rationalize
                     * will detect it. */
                    mpz_t *c2_ptr = &a->minimal_poly.coeffs[2];
                    mpz_t *c1_ptr = &a->minimal_poly.coeffs[1];
                    mpz_t *c0_ptr = &a->minimal_poly.coeffs[0];

                    mpz_t c2_sq, c1_sq, c0_sq, term_mid;
                    mpz_init(c2_sq);
                    mpz_init(c1_sq);
                    mpz_init(c0_sq);
                    mpz_init(term_mid);
                    mpz_mul(c2_sq, *c2_ptr, *c2_ptr);
                    mpz_mul(c1_sq, *c1_ptr, *c1_ptr);
                    mpz_mul(c0_sq, *c0_ptr, *c0_ptr);
                    mpz_mul(term_mid, *c0_ptr, *c2_ptr);
                    mpz_mul_si(term_mid, term_mid, 2);
                    mpz_sub(term_mid, c1_sq, term_mid);

                    mpz_poly_t sq_poly;
                    mpz_poly_init(&sq_poly);
                    sq_poly.degree = 2;
                    sq_poly.coeffs = malloc(3 * sizeof(mpz_t));
                    if (sq_poly.coeffs) {
                        mpz_init(sq_poly.coeffs[0]); /* c0^2 */
                        mpz_init(sq_poly.coeffs[1]); /* c1^2 - 2*c0*c2 */
                        mpz_init(sq_poly.coeffs[2]); /* c2^2 */
                        mpz_set(sq_poly.coeffs[0], c0_sq);
                        mpz_set(sq_poly.coeffs[1], term_mid);
                        mpz_set(sq_poly.coeffs[2], c2_sq);

                        double result_val = val * val;
                        double margin = fabs(result_val) * lv_EPSILON_NEWTON * 100.0;
                        if (margin < lv_EPSILON_NEWTON)
                            margin = lv_EPSILON_NEWTON;

                        SymbolicCoord *result =
                            symbolic_coord_create_algebraic(&sq_poly, result_val - margin, result_val + margin);
                        mpz_poly_clear(&sq_poly);
                        if (result) {
                            result->trust = base->trust;
                            /* Try to rationalize: sqrt(2)^2 = 2 should be detected */
                            algebraic_try_rationalize(result->data.algebraic);
                            if (result->data.algebraic->cached_rational) {
                                SymbolicCoord *rat_result = symbolic_coord_create_rational(0, 1);
                                if (rat_result) {
                                    rational_destroy(rat_result->data.rational);
                                    rat_result->data.rational = rational_copy(result->data.algebraic->cached_rational);
                                    rat_result->trust = result->trust;
                                    symbolic_coord_destroy(result);
                                    mpz_clear(c2_sq);
                                    mpz_clear(c1_sq);
                                    mpz_clear(c0_sq);
                                    mpz_clear(term_mid);
                                    return rat_result;
                                }
                            }
                        }
                    } else {
                        mpz_poly_clear(&sq_poly);
                    }
                    mpz_clear(c2_sq);
                    mpz_clear(c1_sq);
                    mpz_clear(c0_sq);
                    mpz_clear(term_mid);
                    /* Fall through to numerical approach if resultant method failed */
                }
            }

            /* General case: numerical approach with rationalization attempt */
            double result_val = pow(val, (double) exponent);

            /* Try rationalization: check if the result is actually a rational number */
            {
                /* Use continued fraction on the numerical value to find a candidate */
                double margin_cf = fabs(result_val) * lv_EPSILON_NUMERIC_COMPARE;
                if (margin_cf < lv_EPSILON_SUPERTINY)
                    margin_cf = lv_EPSILON_SUPERTINY;

                /* Try mpq_set_d as a quick rationality check */
                mpq_t approx;
                mpq_init(approx);
                mpq_set_d(approx, result_val);
                mpq_canonicalize(approx);

                /* Verify: check if the rational approximation is close enough */
                double approx_val = mpq_get_d(approx);
                if (fabs(approx_val - result_val) <
                    lv_EPSILON_NUMERIC_COMPARE * fabs(result_val) + lv_EPSILON_NUMERIC_COMPARE) {
                    /* Create an algebraic number with this rational value and try rationalization */
                    mpz_poly_t check_poly;
                    mpz_poly_init(&check_poly);
                    check_poly.degree = 1;
                    check_poly.coeffs = malloc(2 * sizeof(mpz_t));
                    if (check_poly.coeffs) {
                        mpz_init(check_poly.coeffs[0]);
                        mpz_init(check_poly.coeffs[1]);
                        mpz_neg(check_poly.coeffs[0], mpq_numref(approx));
                        mpz_set(check_poly.coeffs[1], mpq_denref(approx));

                        double tight_margin = fabs(result_val) * lv_EPSILON_NEWTON;
                        if (tight_margin < lv_EPSILON_NEWTON)
                            tight_margin = lv_EPSILON_NEWTON;

                        SymbolicCoord *result = symbolic_coord_create_algebraic(&check_poly, result_val - tight_margin,
                                                                                result_val + tight_margin);
                        mpz_poly_clear(&check_poly);
                        if (result) {
                            result->trust = base->trust;
                            /* Try to rationalize */
                            algebraic_try_rationalize(result->data.algebraic);
                            if (result->data.algebraic->cached_rational) {
                                /* Successfully rationalized: return as rational */
                                SymbolicCoord *rat_result = symbolic_coord_create_rational(0, 1);
                                if (rat_result) {
                                    rational_destroy(rat_result->data.rational);
                                    rat_result->data.rational = rational_copy(result->data.algebraic->cached_rational);
                                    rat_result->trust = result->trust;
                                    symbolic_coord_destroy(result);
                                    mpq_clear(approx);
                                    return rat_result;
                                }
                            }
                            mpq_clear(approx);
                            return result;
                        }
                    }
                    mpz_poly_clear(&check_poly);
                }
                mpq_clear(approx);
            }

            /* Final fallback: create algebraic number from numerical value */
            double margin = fabs(result_val) * lv_EPSILON_NUMERIC_COMPARE;
            if (margin < lv_EPSILON_NUMERIC_COMPARE)
                margin = lv_EPSILON_NUMERIC_COMPARE;

            mpz_poly_t poly;
            mpz_poly_init(&poly);
            poly.degree = 1;
            poly.coeffs = malloc(2 * sizeof(mpz_t));
            if (!poly.coeffs) {
                mpz_poly_clear(&poly);
                return NULL;
            }
            mpz_init(poly.coeffs[0]);
            mpz_init(poly.coeffs[1]);

            mpq_t approx;
            mpq_init(approx);
            mpq_set_d(approx, result_val);
            mpz_neg(poly.coeffs[0], mpq_numref(approx));
            mpz_set(poly.coeffs[1], mpq_denref(approx));
            mpq_clear(approx);

            SymbolicCoord *result = symbolic_coord_create_algebraic(&poly, result_val - margin, result_val + margin);
            mpz_poly_clear(&poly);
            if (result)
                result->trust = base->trust;
            return result;
        }

        case TRANSCENDENTAL: {
            /* Transcendental power: out_of_scope, but provide double approximation.
             * exponent 0 and 1 are already handled above.
             * For other exponents, create an ALGEBRAIC result with the numerical
             * value pow(transcendental_to_double(t), exponent). */
            {
                double base_val = transcendental_to_double(base->data.transcendental);
                double result_val = pow(base_val, (double) exponent);

                /* Create an algebraic number from the numerical value */
                mpz_poly_t poly;
                mpz_poly_init(&poly);
                poly.degree = 1;
                poly.coeffs = malloc(2 * sizeof(mpz_t));
                if (!poly.coeffs) {
                    mpz_poly_clear(&poly);
                    return NULL;
                }
                mpz_init(poly.coeffs[0]);
                mpz_init(poly.coeffs[1]);

                mpq_t approx;
                mpq_init(approx);
                mpq_set_d(approx, result_val);
                mpz_neg(poly.coeffs[0], mpq_numref(approx));
                mpz_set(poly.coeffs[1], mpq_denref(approx));
                mpq_clear(approx);

                double margin = fabs(result_val) * lv_EPSILON_NEWTON * 100.0;
                if (margin < lv_EPSILON_NEWTON)
                    margin = lv_EPSILON_NEWTON;

                SymbolicCoord *result =
                    symbolic_coord_create_algebraic(&poly, result_val - margin, result_val + margin);
                mpz_poly_clear(&poly);
                if (result)
                    result->trust = TRUST_AMBER;
                return result;
            }
        }
    }

    return NULL;
}

/*
 * Compute sqrt(coord).
 *
 * RATIONAL:   If numerator and denominator are both perfect squares,
 *             return exact rational. Otherwise, try to represent as
 *             quadratic a + b*sqrt(n).
 * QUADRATIC:  sqrt(a + b*sqrt(n)): try nested expansion if
 *             discriminant a^2 - b^2*n is a perfect square.
 * ALGEBRAIC:  Construct sqrt's minimal polynomial via resultant:
 *             resultant_y(P(y), x^2 - y) = P(x^2).
 * TRANSCENDENTAL: Marked as out_of_scope, returns NULL.
 */
SymbolicCoord *symbolic_coord_sqrt(const SymbolicCoord *coord) {
    if (!coord)
        return NULL;

    switch (coord->type) {
        case RATIONAL: {
            const Rational *r = coord->data.rational;
            mpz_t num, den;
            mpz_init_set(num, mpq_numref(r->value));
            mpz_init_set(den, mpq_denref(r->value));

            /* 检查分子和分母是否都是完全平方数 */
            int num_sign = mpz_sgn(num);
            if (num_sign < 0) {
                /* 负有理数：sqrt 不是实数 */
                mpz_clear(num);
                mpz_clear(den);
                return NULL;
            }

            mpz_t *num_sqrt = mpz_perfect_sqrt(num);
            mpz_t *den_sqrt = mpz_perfect_sqrt(den);

            if (num_sqrt && den_sqrt) {
                /* Both are perfect squares: return exact rational */
                Rational *result_r = rational_create_from_mpz(*num_sqrt, *den_sqrt);
                mpz_clear(*num_sqrt);
                lv_free((void **) &num_sqrt);
                mpz_clear(*den_sqrt);
                lv_free((void **) &den_sqrt);

                if (!result_r) {
                    mpz_clear(num);
                    mpz_clear(den);
                    return NULL;
                }

                SymbolicCoord *result = symbolic_coord_create_rational(0, 1);
                if (result) {
                    rational_destroy(result->data.rational);
                    result->data.rational = result_r;
                    result->trust = coord->trust;
                } else {
                    rational_destroy(result_r);
                }
                return result;
            }

            /* 清理任何非 NULL 的结果 */
            if (num_sqrt) {
                mpz_clear(*num_sqrt);
                lv_free((void **) &num_sqrt);
            }
            if (den_sqrt) {
                mpz_clear(*den_sqrt);
                lv_free((void **) &den_sqrt);
            }

            /* 不是完全平方数：尝试表示为二次根式 a + b*sqrt(n)
             * sqrt(p/q) = sqrt(p*q) / q
             * 如果 p*q 有平方因子，提取它们。
             */
            mpz_t product;
            mpz_init(product);
            mpz_mul(product, num, den);

            /* 从乘积中提取完全平方因子 */
            mpz_t remaining;
            mpz_init_set(remaining, product);
            mpz_t square_part;
            mpz_init_set_ui(square_part, 1);

            /* 尝试小素因子 */
            for (unsigned int i = 2; i * i <= 10000; i++) {
                while (mpz_divisible_ui_p(remaining, i * i)) {
                    mpz_mul_ui(square_part, square_part, i);
                    mpz_divexact_ui(remaining, remaining, i * i);
                }
            }

            /* 检查 remaining 是否为 1（上面已处理完全平方情况）
             * 或者如果 remaining > 1，则有 sqrt(product) = square_part * sqrt(remaining) */
            if (mpz_cmp_si(remaining, 1) == 0) {
                /* Should have been caught above, but handle gracefully */
                mpz_clear(product);
                mpz_clear(remaining);
                mpz_clear(square_part);
                return NULL;
            }

            /* sqrt(p/q) = square_part * sqrt(remaining) / q
             * = (square_part / q) * sqrt(remaining)
             * = 0 + (square_part/q) * sqrt(remaining) */

            /* Check if remaining fits in unsigned int */
            if (!mpz_fits_uint_p(remaining)) {
                mpz_clear(product);
                mpz_clear(remaining);
                mpz_clear(square_part);
                return NULL;
            }

            unsigned int n = mpz_get_ui(remaining);
            n = remove_square_factors(n);

            /* Result: 0 + (square_part / den) * sqrt(n) */
            Rational *a = rational_create(0, 1);
            Rational *b = rational_create_from_mpz(square_part, den);

            mpz_clear(product);
            mpz_clear(remaining);
            mpz_clear(square_part);

            SymbolicCoord *result = symbolic_coord_create_quadratic(a, b, n);
            if (result)
                result->trust = coord->trust;
            else {
                rational_destroy(a);
                rational_destroy(b);
            }
            return result;
        }

        case QUADRATIC: {
            /* sqrt(a + b*sqrt(n))
             * Try to expand to c + d*sqrt(m) form.
             *
             * If (a + b*sqrt(n)) = (c + d*sqrt(m))^2
             * = c^2 + d^2*m + 2cd*sqrt(m)
             *
             * For m = n: c^2 + d^2*n = a, 2cd = b
             * => d = b/(2c), substitute: c^2 + b^2*n/(4c^2) = a
             * => 4c^4 - 4a*c^2 + b^2*n = 0
             * => c^2 = (4a +/- sqrt(16a^2 - 16*b^2*n)) / 8
             *        = (a +/- sqrt(a^2 - b^2*n)) / 2
             *
             * So we need a^2 - b^2*n to be a perfect square (as a rational).
             */
            const Quadratic *q = coord->data.quadratic;

            /* Check if b = 0: sqrt(a) where a is rational */
            if (is_rational_zero(q->b)) {
                /* Delegate to rational sqrt */
                SymbolicCoord *rat_coord = symbolic_coord_create_rational(0, 1);
                if (rat_coord) {
                    rational_destroy(rat_coord->data.rational);
                    rat_coord->data.rational = rational_copy(q->a);
                    rat_coord->trust = coord->trust;
                    SymbolicCoord *result = symbolic_coord_sqrt(rat_coord);
                    symbolic_coord_destroy(rat_coord);
                    return result;
                }
                return NULL;
            }

            /* Compute discriminant: a^2 - b^2*n (as rational) */
            mpq_t a_sq, b_sq, b_sq_n, disc;
            mpq_init(a_sq);
            mpq_init(b_sq);
            mpq_init(b_sq_n);
            mpq_init(disc);

            mpq_mul(a_sq, q->a->value, q->a->value);
            mpq_mul(b_sq, q->b->value, q->b->value);
            mpq_set_ui(b_sq_n, q->n, 1);
            mpq_mul(b_sq_n, b_sq_n, b_sq);
            mpq_sub(disc, a_sq, b_sq_n);

            /* disc = a^2 - b^2*n must be non-negative for real sqrt */
            if (mpq_sgn(disc) < 0) {
                mpq_clear(a_sq);
                mpq_clear(b_sq);
                mpq_clear(b_sq_n);
                mpq_clear(disc);
                return NULL;
            }

            /* Check if disc is a perfect square of a rational */
            /* disc = p/q, need sqrt(p/q) = sqrt(p*q)/q to be rational */
            mpz_t disc_num_sq, disc_product;
            mpz_init(disc_num_sq);
            mpz_init(disc_product);
            mpz_mul(disc_product, mpq_numref(disc), mpq_denref(disc));

            mpz_t *disc_sqrt = mpz_perfect_sqrt(disc_product);

            if (disc_sqrt) {
                /* disc is a perfect square rational: disc_sqrt / den(disc) */
                /* k = disc_sqrt / den(disc) */
                mpq_t k;
                mpq_init(k);
                mpz_set(mpq_numref(k), *disc_sqrt);
                mpz_set(mpq_denref(k), mpq_denref(disc));
                mpq_canonicalize(k);

                /* c^2 = (a + k) / 2 */
                mpq_t c_sq, c_sq_neg;
                mpq_init(c_sq);
                mpq_init(c_sq_neg);
                mpq_add(c_sq, q->a->value, k);
                mpq_div_2exp(c_sq, c_sq, 1); /* divide by 2 */

                /* Also try (a - k) / 2 */
                mpq_sub(c_sq_neg, q->a->value, k);
                mpq_div_2exp(c_sq_neg, c_sq_neg, 1);

                /* Try c_sq first: check if it's a perfect square rational */
                mpz_t csq_product;
                mpz_init(csq_product);
                mpz_mul(csq_product, mpq_numref(c_sq), mpq_denref(c_sq));
                mpz_t *c_sqrt = mpz_perfect_sqrt(csq_product);

                Rational *c_rat = NULL;
                Rational *d_rat = NULL;

                if (c_sqrt && mpq_sgn(c_sq) >= 0) {
                    /* c = c_sqrt / den(c_sq) */
                    c_rat = rational_create_from_mpz(*c_sqrt, mpq_denref(c_sq));
                    /* d = b / (2c) */
                    Rational *two_c = rational_create_from_mpz(*c_sqrt, mpq_denref(c_sq));
                    mpq_mul_2exp(two_c->value, two_c->value, 1); /* 2c */
                    d_rat = rational_divide(q->b, two_c);
                    rational_destroy(two_c);
                    mpz_clear(*c_sqrt);
                    lv_free((void **) &c_sqrt);
                } else {
                    if (c_sqrt) {
                        mpz_clear(*c_sqrt);
                        lv_free((void **) &c_sqrt);
                    }

                    /* Try c_sq_neg */
                    mpz_mul(csq_product, mpq_numref(c_sq_neg), mpq_denref(c_sq_neg));
                    c_sqrt = mpz_perfect_sqrt(csq_product);

                    if (c_sqrt && mpq_sgn(c_sq_neg) >= 0) {
                        c_rat = rational_create_from_mpz(*c_sqrt, mpq_denref(c_sq_neg));
                        Rational *two_c = rational_create_from_mpz(*c_sqrt, mpq_denref(c_sq_neg));
                        mpq_mul_2exp(two_c->value, two_c->value, 1);
                        d_rat = rational_divide(q->b, two_c);
                        rational_destroy(two_c);
                        mpz_clear(*c_sqrt);
                        lv_free((void **) &c_sqrt);
                    } else {
                        if (c_sqrt) {
                            mpz_clear(*c_sqrt);
                            lv_free((void **) &c_sqrt);
                        }
                    }
                }

                mpz_clear(csq_product);
                mpq_clear(c_sq);
                mpq_clear(c_sq_neg);
                mpq_clear(k);

                if (c_rat && d_rat) {
                    /* Result: c + d*sqrt(n) */
                    SymbolicCoord *result = symbolic_coord_create_quadratic(c_rat, d_rat, q->n);
                    if (result)
                        result->trust = coord->trust;
                    else {
                        rational_destroy(c_rat);
                        rational_destroy(d_rat);
                    }
                    mpz_clear(*disc_sqrt);
                    lv_free((void **) &disc_sqrt);
                    mpq_clear(a_sq);
                    mpq_clear(b_sq);
                    mpq_clear(b_sq_n);
                    mpq_clear(disc);
                    mpz_clear(disc_num_sq);
                    mpz_clear(disc_product);
                    return result;
                }

                if (c_rat)
                    rational_destroy(c_rat);
                if (d_rat)
                    rational_destroy(d_rat);
                mpz_clear(*disc_sqrt);
                lv_free((void **) &disc_sqrt);
            } else {
                mpz_clear(disc_num_sq);
                mpz_clear(disc_product);
            }

            mpq_clear(a_sq);
            mpq_clear(b_sq);
            mpq_clear(b_sq_n);
            mpq_clear(disc);

            /* Cannot expand exactly: fall back to algebraic representation
             * sqrt(a + b*sqrt(n)) has minimal polynomial obtained by
             * substituting x^2 for x in the minimal poly of (a + b*sqrt(n)).
             *
             * The minimal poly of a + b*sqrt(n) is:
             * x^2 - 2a*x + (a^2 - b^2*n) = 0
             *
             * So sqrt(alpha) satisfies:
             * (x^2)^2 - 2a*(x^2) + (a^2 - b^2*n) = 0
             * x^4 - 2a*x^2 + (a^2 - b^2*n) = 0
             */
            {
                double a_val = rational_to_double(q->a);
                double b_val = rational_to_double(q->b);
                double sqrt_n = sqrt((double) q->n);
                double inner = a_val + b_val * sqrt_n;

                if (inner < 0)
                    return NULL;

                double result_val = sqrt(inner);

                /* Construct minimal poly: x^4 - 2a*x^2 + (a^2 - b^2*n) = 0 */
                mpz_poly_t poly;
                mpz_poly_init(&poly);
                poly.degree = 4;
                poly.coeffs = malloc(5 * sizeof(mpz_t));
                if (!poly.coeffs) {
                    mpz_poly_clear(&poly);
                    return NULL;
                }

                /* coeffs[0] = a^2 - b^2*n (constant term) */
                mpz_init(poly.coeffs[0]);
                mpz_mul(poly.coeffs[0], mpq_numref(q->a->value), mpq_numref(q->a->value));
                mpz_t den_a_sq;
                mpz_init(den_a_sq);
                mpz_mul(den_a_sq, mpq_denref(q->a->value), mpq_denref(q->a->value));

                mpz_t b_sq_n_num, b_sq_n_den;
                mpz_init(b_sq_n_num);
                mpz_init(b_sq_n_den);
                mpz_mul(b_sq_n_num, mpq_numref(q->b->value), mpq_numref(q->b->value));
                mpz_mul_ui(b_sq_n_num, b_sq_n_num, q->n);
                mpz_mul(b_sq_n_den, mpq_denref(q->b->value), mpq_denref(q->b->value));

                /* coeffs[0] = (a_num^2 * b_sq_n_den - b_sq_n_num * den_a^2) / (den_a^2 * b_sq_n_den) */
                /* Store as integer polynomial by multiplying through by LCD */
                mpz_t lcd;
                mpz_init(lcd);
                mpz_mul(lcd, den_a_sq, b_sq_n_den);

                mpz_t term1, term2;
                mpz_init(term1);
                mpz_init(term2);
                mpz_mul(term1, poly.coeffs[0], b_sq_n_den);
                mpz_mul(term2, b_sq_n_num, den_a_sq);
                mpz_sub(poly.coeffs[0], term1, term2);

                /* Scale all coefficients by LCD to get integer polynomial */
                mpz_set(poly.coeffs[0], poly.coeffs[0]); /* already scaled */

                /* coeffs[1] = 0 (no x term) */
                mpz_init_set_ui(poly.coeffs[1], 0);

                /* coeffs[2] = -2a (x^2 term) */
                mpz_init(poly.coeffs[2]);
                mpz_mul_ui(poly.coeffs[2], mpq_numref(q->a->value), 2);
                mpz_neg(poly.coeffs[2], poly.coeffs[2]);
                mpz_mul(poly.coeffs[2], poly.coeffs[2], b_sq_n_den);

                /* coeffs[3] = 0 (no x^3 term) */
                mpz_init_set_ui(poly.coeffs[3], 0);

                /* coeffs[4] = den_a^2 * b_sq_n_den (leading coefficient for x^4) */
                mpz_init_set(poly.coeffs[4], lcd);

                mpz_clear(den_a_sq);
                mpz_clear(b_sq_n_num);
                mpz_clear(b_sq_n_den);
                mpz_clear(lcd);
                mpz_clear(term1);
                mpz_clear(term2);

                double margin = fabs(result_val) * lv_EPSILON_NEWTON * 10.0;
                if (margin < lv_EPSILON_NEWTON)
                    margin = lv_EPSILON_NEWTON;

                SymbolicCoord *result =
                    symbolic_coord_create_algebraic(&poly, result_val - margin, result_val + margin);
                mpz_poly_clear(&poly);
                if (result)
                    result->trust = coord->trust;
                return result;
            }
        }

        case ALGEBRAIC: {
            /* sqrt of algebraic number alpha:
             * If alpha has minpoly P(x), then sqrt(alpha) has minpoly P(x^2).
             *
             * P(x^2) is obtained by replacing x^k with x^{2k} in P(x).
             */
            Algebraic *a = coord->data.algebraic;

            /* Check if the algebraic number is negative (no real sqrt) */
            double mid = (a->left_bound + a->right_bound) / 2.0;
            if (mid < -lv_EPSILON_NUMERIC_COMPARE)
                return NULL;

            /* Try rationalization first: if the algebraic number is actually rational */
            if (a->cached_rational) {
                SymbolicCoord *rat_coord = symbolic_coord_create_rational(0, 1);
                if (rat_coord) {
                    rational_destroy(rat_coord->data.rational);
                    rat_coord->data.rational = rational_copy(a->cached_rational);
                    rat_coord->trust = coord->trust;
                    SymbolicCoord *result = symbolic_coord_sqrt(rat_coord);
                    symbolic_coord_destroy(rat_coord);
                    return result;
                }
            }

            /* Try to rationalize */
            algebraic_try_rationalize(a);
            if (a->cached_rational) {
                SymbolicCoord *rat_coord = symbolic_coord_create_rational(0, 1);
                if (rat_coord) {
                    rational_destroy(rat_coord->data.rational);
                    rat_coord->data.rational = rational_copy(a->cached_rational);
                    rat_coord->trust = coord->trust;
                    SymbolicCoord *result = symbolic_coord_sqrt(rat_coord);
                    symbolic_coord_destroy(rat_coord);
                    return result;
                }
            }

            /* Construct P(x^2) from P(x) */
            int deg = a->minimal_poly.degree;
            int new_deg = 2 * deg;

            mpz_poly_t sqrt_poly;
            mpz_poly_init(&sqrt_poly);
            sqrt_poly.degree = new_deg;
            sqrt_poly.coeffs = malloc((new_deg + 1) * sizeof(mpz_t));
            if (!sqrt_poly.coeffs) {
                mpz_poly_clear(&sqrt_poly);
                return NULL;
            }

            for (int i = 0; i <= new_deg; i++) {
                mpz_init(sqrt_poly.coeffs[i]);
                if (i % 2 == 0) {
                    /* Even power: comes from P(x^{i/2}) */
                    int src_idx = i / 2;
                    if (src_idx <= deg) {
                        mpz_set(sqrt_poly.coeffs[i], a->minimal_poly.coeffs[src_idx]);
                    }
                }
                /* Odd powers: coefficient is 0 (already initialized) */
            }

            /* Compute numerical value for isolation bounds */
            refine_algebraic_bounds(a, 20);
            double val = (a->left_bound + a->right_bound) / 2.0;
            double sqrt_val = sqrt(fabs(val));

            double margin = fabs(sqrt_val) * lv_EPSILON_NEWTON * 10.0;
            if (margin < lv_EPSILON_NEWTON)
                margin = lv_EPSILON_NEWTON;

            SymbolicCoord *result = symbolic_coord_create_algebraic(&sqrt_poly, sqrt_val - margin, sqrt_val + margin);
            mpz_poly_clear(&sqrt_poly);
            if (result)
                result->trust = coord->trust;
            return result;
        }

        case TRANSCENDENTAL: {
            /* Transcendental sqrt: out_of_scope, but provide double approximation.
             * Create an ALGEBRAIC result with the numerical value
             * sqrt(transcendental_to_double(t)). */
            {
                double base_val = transcendental_to_double(coord->data.transcendental);
                if (base_val < 0.0)
                    return NULL; /* sqrt of negative transcendental is not real */

                double sqrt_val = sqrt(base_val);

                /* Create an algebraic number from the numerical value */
                mpz_poly_t poly;
                mpz_poly_init(&poly);
                poly.degree = 1;
                poly.coeffs = malloc(2 * sizeof(mpz_t));
                if (!poly.coeffs) {
                    mpz_poly_clear(&poly);
                    return NULL;
                }
                mpz_init(poly.coeffs[0]);
                mpz_init(poly.coeffs[1]);

                mpq_t approx;
                mpq_init(approx);
                mpq_set_d(approx, sqrt_val);
                mpz_neg(poly.coeffs[0], mpq_numref(approx));
                mpz_set(poly.coeffs[1], mpq_denref(approx));
                mpq_clear(approx);

                double margin = fabs(sqrt_val) * lv_EPSILON_NEWTON * 100.0;
                if (margin < lv_EPSILON_NEWTON)
                    margin = lv_EPSILON_NEWTON;

                SymbolicCoord *result = symbolic_coord_create_algebraic(&poly, sqrt_val - margin, sqrt_val + margin);
                mpz_poly_clear(&poly);
                if (result)
                    result->trust = TRUST_AMBER;
                return result;
            }
        }
    }

    return NULL;
}

/* ============================================================
 * Hash Function for Normalization Grouping
 * 
 * According to design_v2.9.md Section 4.2:
 * "遍历图中所有 POINT 节点，按符号坐标的哈希值分组。
 *  哈希基于符号坐标的规范序列化计算。"
 * ============================================================ */

/* FNV-1a hash constants - 使用 lv_internal.h 中统一定义的 lv_FNV64_PRIME / lv_FNV64_OFFSET_BASIS */

static uint64_t fnv1a_update(uint64_t hash, const char *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        hash ^= (uint64_t) (unsigned char) data[i];
        hash *= lv_FNV64_PRIME;
    }
    return hash;
}

uint64_t symbolic_coord_hash(const SymbolicCoord *coord) {
    if (!coord)
        return 0;

    uint64_t hash = lv_FNV64_OFFSET_BASIS;

    /* Hash the type */
    hash = fnv1a_update(hash, (const char *) &coord->type, sizeof(coord->type));

    switch (coord->type) {
        case RATIONAL: {
            /* Hash based on canonical serialization */
            char *ser = rational_serialize(coord->data.rational);
            if (ser) {
                hash = fnv1a_update(hash, ser, strlen(ser));
                lv_free((void**)&ser); /* lv_malloc分配 */
            }
            break;
        }
        case ALGEBRAIC: {
            Algebraic *a = coord->data.algebraic;
            /* Hash minimal polynomial coefficients */
            for (int i = 0; i <= a->minimal_poly.degree; i++) {
                char *coeff_str = mpz_get_str(NULL, 16, a->minimal_poly.coeffs[i]);
                if (coeff_str) {
                    hash = fnv1a_update(hash, coeff_str, strlen(coeff_str));
                    lv_free_external((void **) &coeff_str);
                }
            }
            /* Hash bounds for distinguishing different roots */
            hash = fnv1a_update(hash, (const char *) &a->left_bound, sizeof(double));
            hash = fnv1a_update(hash, (const char *) &a->right_bound, sizeof(double));
            break;
        }
        case QUADRATIC: {
            Quadratic *q = coord->data.quadratic;
            char *a_ser = rational_serialize(q->a);
            char *b_ser = rational_serialize(q->b);
            if (a_ser) {
                hash = fnv1a_update(hash, a_ser, strlen(a_ser));
                lv_free((void**)&a_ser); /* lv_malloc分配 */
            }
            if (b_ser) {
                hash = fnv1a_update(hash, b_ser, strlen(b_ser));
                lv_free((void**)&b_ser); /* lv_malloc分配 */
            }
            hash = fnv1a_update(hash, (const char *) &q->n, sizeof(q->n));
            break;
        }
        case TRANSCENDENTAL: {
            hash = fnv1a_update(hash, coord->data.transcendental->name, strlen(coord->data.transcendental->name));
            break;
        }
    }

    return hash;
}

/* ============================================================
 * Enhanced Priority Rationalization via Continued Fractions
 * 
 * According to design_v2.9.md Section 1.2:
 * "每次算术运算后自动运行。对结果代数数计算连分式逼近
 *  （精度取当前隔离区间宽度的 1/4），产生一个有理数候选值 r_approx。
 *  在符号层将 r_approx 代入极小多项式精确求值。
 *  若多项式值为零，则该代数数等价于 r_approx"
 * ============================================================ */

/*
 * Compute continued fraction convergent at specified precision.
 * Returns a Rational approximation of the algebraic number.
 * precision: target precision (width of isolation interval / 4)
 */
static Rational *algebraic_continued_fraction_approx(const Algebraic *a, double precision) {
    double val = algebraic_to_double(a);
    if (precision <= 0)
        precision = 1e-15;

    /* Use GMP mpq for exact continued fraction computation */
    mpq_t approx;
    mpq_init(approx);

    /* Simple continued fraction: repeatedly take integer part and invert remainder */
    double x = val;
    mpq_t result;
    mpq_init(result);
    mpq_set_ui(result, 0, 1);

    mpq_t term;
    mpq_init(term);

    /* Build continued fraction from bottom up */
    /* We collect terms and then evaluate from the deepest */
    int terms[100];
    int n_terms = 0;

    double remaining = val;
    for (int i = 0; i < 100 && n_terms < 100; i++) {
        if (remaining < 1e-300 || remaining > 1e300)
            break;

        int64_t int_part = (int64_t) remaining;
        if (n_terms >= 100)
            break;
        terms[n_terms++] = (int) int_part;
        remaining = remaining - (double) int_part;

        if (remaining < precision / 2.0)
            break;
        if (remaining < 1e-300)
            break;

        remaining = 1.0 / remaining;
    }

    /* Evaluate continued fraction from deepest term */
    if (n_terms > 0) {
        mpq_set_si(result, terms[n_terms - 1], 1);
        for (int i = n_terms - 2; i >= 0; i--) {
            mpq_set_si(term, terms[i], 1);
            /* result = term + 1/result */
            mpq_inv(result, result);
            mpq_add(result, term, result);
        }
    }

    mpq_canonicalize(result);

    Rational *r = lv_calloc(1, sizeof(Rational));
    if (r) {
        mpq_init(r->value);
        mpq_set(r->value, result);
    }

    mpq_clear(approx);
    mpq_clear(result);
    mpq_clear(term);

    return r;
}

/*
 * Enhanced priority rationalization using continued fractions.
 * 
 * Algorithm:
 * 1. Compute precision = (right_bound - left_bound) / 4
 * 2. Generate continued fraction approximation at that precision
 * 3. Evaluate minimal polynomial at the rational candidate exactly
 * 4. If polynomial value is zero, cache the rational and return true
 * 
 * Returns: true if rationalization succeeded, false otherwise
 */
bool algebraic_try_rationalize(Algebraic *a) {
    if (!a)
        return false;

    /* 计算精度 = (right_bound - left_bound) / 4 */
    double span = a->right_bound - a->left_bound;
    if (span <= 0.0)
        return false;

    double precision = span / 4.0;

    /* 生成连分数近似 */
    Rational *candidate = algebraic_continued_fraction_approx(a, precision);
    if (!candidate)
        return false;

    /* 在有理候选上精确计算最小多项式值 */
    int deg = a->minimal_poly.degree;

    /* 准备分母 = 1 */
    mpz_t mpz_one;
    mpz_init_set_ui(mpz_one, 1);

    /* result = coeff[0] 的 Rational 表示 */
    Rational *result = rational_create_from_mpz(a->minimal_poly.coeffs[0], mpz_one);
    if (!result) {
        mpz_clear(mpz_one);
        rational_destroy(candidate);
        return false;
    }

    /* power = candidate^0 = 1 */
    Rational *power = rational_create(1, 1);
    if (!power) {
        mpz_clear(mpz_one);
        rational_destroy(result);
        rational_destroy(candidate);
        return false;
    }

    for (int i = 1; i <= deg; i++) {
        /* power *= candidate */
        Rational *new_power = rational_multiply(power, candidate);
        rational_destroy(power);
        power = new_power;

        /* term = coeff[i] * power */
        Rational *coeff_r = rational_create_from_mpz(a->minimal_poly.coeffs[i], mpz_one);
        Rational *term = rational_multiply(coeff_r, power);
        rational_destroy(coeff_r);

        /* result += term */
        Rational *new_result = rational_add(result, term);
        rational_destroy(result);
        rational_destroy(term);
        result = new_result;
    }

    mpz_clear(mpz_one);
    rational_destroy(power);

    /* 检查多项式值是否为零 */
    if (is_rational_zero(result)) {
        /* 成功有理化：缓存结果 */
        if (a->cached_rational)
            rational_destroy(a->cached_rational);
        a->cached_rational = candidate;
        rational_destroy(result);
        return true;
    }

    rational_destroy(result);
    rational_destroy(candidate);
    return false;
}


/* Trust color combination */
TrustColor trust_color_combine(TrustColor a, TrustColor b) {
    /* Rule: non-constructive(LO) + numeric-assumption(AMBER) = DEEP_ORANGE */
    /* Otherwise take the higher value (lower trust) */
    int a_val = (int)a;
    int b_val = (int)b;

    /* Check for light-orange + amber superposition */
    int is_a_lo = (a_val == TRUST_LIGHT_ORANGE_ORACLE || a_val == TRUST_LIGHT_ORANGE_EXPLOSION);
    int is_b_lo = (b_val == TRUST_LIGHT_ORANGE_ORACLE || b_val == TRUST_LIGHT_ORANGE_EXPLOSION);

    if ((is_a_lo && b_val == TRUST_AMBER) || (is_b_lo && a_val == TRUST_AMBER)) {
        return TRUST_DEEP_ORANGE;
    }

    /* Otherwise: higher value = lower trust */
    return (a_val > b_val) ? a : b;
}

/* ============================================================
 * A/B 自动降级系统实现
 *
 * 根据设计文档 3.3 节，Lv-00 的符号坐标系统采用 A/B 双轨策略：
 * - A 计划（主轨道）：基于 GMP + 代数数
 * - B 计划（降级轨道）：二次根式（a + b√n 规范形式）
 * - C 计划：仅有理数
 *
 * 降级规则：
 * - 连续失败次数达到 SYM_COORD_DEGRADE_THRESHOLD (3) 次后自动降级
 * - 降级不可逆（B→A 不自动恢复）
 * - 使用 lv_THREAD_LOCAL 保证线程安全
 * ============================================================ */

/**
 * @brief 降级辅助函数：检查代数数运算结果是否需要降级
 *
 * 当 A 计划运算产生的结果 degree > 2 或系数位数超过阈值时：
 * 1. 尝试有理化（如果结果是实际上的有理数）
 * 2. 尝试检测是否为二次根式形式（degree == 2 且可化简为 a+b√n）
 * 3. 如果无法化简，调用 symbolic_coord_auto_degrade() 触发降级
 *
 * @param result 代数数运算结果（ALGEBRAIC 类型），函数接管所有权
 * @return 处理后的 SymbolicCoord（可能类型已转换）
 */
static SymbolicCoord *_symbolic_coord_degrade_check_algebraic(SymbolicCoord *result) {
    if (!result || result->type != ALGEBRAIC)
        return result;

    /* 仅当当前计划为 PLAN_A 时执行降级检查 */
    if (algebraic_get_plan() != PLAN_A_FULL_ALGEBRAIC)
        return result;

    Algebraic *a = result->data.algebraic;
    if (!a)
        return result;

    int deg = a->minimal_poly.degree;

    /* 检查是否需要降级：degree > 2 或系数位数超过阈值 */
    bool needs_check = false;
    if (deg > 2) {
        needs_check = true;
    } else {
        size_t max_bits = 0;
        for (int i = 0; i <= deg; i++) {
            size_t bits = mpz_sizeinbase(a->minimal_poly.coeffs[i], 2);
            if (bits > max_bits)
                max_bits = bits;
        }
        if (max_bits > (size_t)(lv_BIT_CUTOFF_THRESHOLD / SYM_COORD_ALGEBRAIC_BIT_LIMIT_FACTOR)) {
            needs_check = true;
        }
    }

    if (!needs_check)
        return result;

    /* 1. 尝试有理化 */
    algebraic_try_rationalize(a);
    if (a->cached_rational) {
        SymbolicCoord *new_result = symbolic_coord_create_rational(0, 1);
        if (new_result) {
            rational_destroy(new_result->data.rational);
            new_result->data.rational = rational_copy(a->cached_rational);
            new_result->trust = result->trust;
            symbolic_coord_destroy(result);
            return new_result;
        }
    }

    /* 2. 如果 degree == 2，尝试转换为二次根式形式 */
    if (deg == 2) {
        /* 检查是否为 x^2 - n = 0 的形式 => sqrt(n) */
        mpz_t *c0 = &a->minimal_poly.coeffs[0];
        mpz_t *c1 = &a->minimal_poly.coeffs[1];
        mpz_t *c2 = &a->minimal_poly.coeffs[2];

        if (mpz_cmp_si(*c1, 0) == 0 && mpz_sgn(*c0) < 0 && mpz_sgn(*c2) > 0) {
            /* 形如 c2*x^2 + c0 = 0, i.e. x^2 = -c0/c2 */
            mpz_t n_num;
            mpz_init(n_num);
            mpz_neg(n_num, *c0);
            /* 检查是否 -c0/c2 为正整数 */
            if (mpz_divisible_p(n_num, *c2) && mpz_sgn(n_num) > 0) {
                mpz_divexact(n_num, n_num, *c2);
                if (mpz_fits_uint_p(n_num)) {
                    unsigned long n_val = mpz_get_ui(n_num);
                    if (n_val > 0) {
                        double a_val = algebraic_to_double(a);
                        Rational *zero = rational_create(0, 1);
                        Rational *one = rational_create(1, 1);
                        unsigned int n_ui = (unsigned int)n_val;

                        SymbolicCoord *new_result = symbolic_coord_create_quadratic(zero, one, n_ui);
                        if (new_result) {
                            /* 确定符号 */
                            if (a_val < 0) {
                                /* negate */
                                Rational *neg_one = rational_create(-1, 1);
                                rational_destroy(new_result->data.quadratic->b);
                                new_result->data.quadratic->b = neg_one;
                            }
                            new_result->trust = result->trust;
                            symbolic_coord_destroy(result);
                            mpz_clear(n_num);
                            return new_result;
                        }
                        rational_destroy(zero);
                        rational_destroy(one);
                    }
                }
            }
            mpz_clear(n_num);
        }

        /* 一般二次形式：a*x^2 + b*x + c = 0
         * 解为 x = (-b ± sqrt(b^2 - 4ac)) / (2a)
         * 这是 a + b*sqrt(n) 形式的推广，其中 a = -b/(2a), b = 1/(2a), n = b^2 - 4ac */
        {
            /* 对于无法直接识别的二次形式，保留为 ALGEBRAIC
             * 但计数一次近阈值事件（不强制降级） */
            g_degrade_fail_count++;
        }
        return result;
    }

    /* 3. 对于 degree > 2：无法化简，触发自动降级 */
    symbolic_coord_auto_degrade("algebraic result degree > 2, exceeds A-plan threshold");

    return result;
}

/**
 * @brief 设置坐标系统当前代数计划
 *
 * 运行时切换代数计划。切换后，新创建的坐标将使用新计划。
 * 已有坐标不受影响。
 *
 * @param plan 目标代数计划
 */
void symbolic_coord_set_plan(AlgebraicPlan plan) {
    algebraic_set_plan(plan);
    /* 切换计划时重置失败计数 */
    g_degrade_fail_count = 0;
}

/**
 * @brief 获取当前代数计划
 *
 * @return 当前代数计划
 */
AlgebraicPlan symbolic_coord_get_plan(void) {
    return algebraic_get_plan();
}

/**
 * @brief 自动降级决策
 *
 * 当 A 计划操作失败时调用此函数，决定是否降级。
 * 决策依据：连续失败次数（达到 3 次后自动降级）。
 * 降级路径：PLAN_A → PLAN_B → PLAN_C（单向不可逆）。
 *
 * @param reason 失败原因描述（如 "precision degraded", "timeout"），仅用于日志
 * @return true 已降级到更低计划，false 保持当前计划
 */
bool symbolic_coord_auto_degrade(const char *reason) {
    AlgebraicPlan current = algebraic_get_plan();
    (void)reason; /* 日志预留 */

    /* 若已在最低计划，不再降级 */
    if (current == PLAN_C_RATIONAL_ONLY) {
        g_degrade_fail_count = 0;
        return false;
    }

    g_degrade_fail_count++;

    if (g_degrade_fail_count >= SYM_COORD_DEGRADE_THRESHOLD) {
        /* 降级到下一级计划 */
        AlgebraicPlan new_plan;
        switch (current) {
            case PLAN_A_FULL_ALGEBRAIC:
                new_plan = PLAN_B_QUADRATIC_ONLY;
                break;
            case PLAN_B_QUADRATIC_ONLY:
                new_plan = PLAN_C_RATIONAL_ONLY;
                break;
            default:
                new_plan = current;
                break;
        }

        if (new_plan != current) {
            algebraic_set_plan(new_plan);
            g_degrade_total++;
        }

        g_degrade_fail_count = 0;
        return true;
    }

    return false;
}

/**
 * @brief 创建符号坐标（带计划感知）
 *
 * 根据当前计划选择创建方式：
 * - PLAN_A_FULL_ALGEBRAIC: 创建 ALGEBRAIC 类型，失败后自动降级
 * - PLAN_B_QUADRATIC_ONLY: 仅使用二次根式
 * - PLAN_C_RATIONAL_ONLY: 仅使用有理数
 *
 * @param num 分子
 * @param den 分母
 * @return 新创建的 SymbolicCoord
 */
SymbolicCoord *symbolic_coord_create_with_plan(long num, long den) {
    AlgebraicPlan plan = algebraic_get_plan();

    switch (plan) {
        case PLAN_A_FULL_ALGEBRAIC: {
            /* 尝试创建完整代数数 */
            Rational *r = rational_create((int64_t)num, (uint64_t)den);
            if (!r)
                return NULL;

            Algebraic *alg = algebraic_from_rational(r);
            rational_destroy(r);

            if (alg) {
                SymbolicCoord *result = lv_calloc(1, sizeof(SymbolicCoord));
                if (!result) {
                    algebraic_destroy(alg);
                    return NULL;
                }
                result->type = ALGEBRAIC;
                result->trust = TRUST_GREEN;
                result->cache_valid = false;
                result->cached_value = 0.0;
                result->algebraic_info = NULL;
                result->data.algebraic = alg;
                return result;
            }

            /* 失败后降级并回退 */
            symbolic_coord_auto_degrade("algebraic_from_rational failed");
            /* fall through to PLAN_B */
        }
        case PLAN_B_QUADRATIC_ONLY: {
            /* 创建二次根式 (a + 0*sqrt(1)) */
            Rational *a = rational_create((int64_t)num, (uint64_t)den);
            Rational *b = rational_create(0, 1);
            SymbolicCoord *result = symbolic_coord_create_quadratic(a, b, 1);
            if (!result) {
                rational_destroy(a);
                rational_destroy(b);
            }
            return result;
        }
        case PLAN_C_RATIONAL_ONLY:
        default:
            return symbolic_coord_create_rational((int64_t)num, (uint64_t)den);
    }
}

/**
 * @brief 检测代数表达式是否为二次根式形式
 *
 * 检查代数表达式（字符串格式）是否可以化简为 a + b√n 的形式。
 * 这是 A→B 降级的前置条件。
 *
 * @param expr 代数表达式（字符串格式）
 * @return true 可以化简为二次根式
 */
bool symbolic_coord_is_quadratic_form(const char *expr) {
    if (!expr || expr[0] == '\0')
        return false;

    /* 检查表达式中是否包含 sqrt (或 √) 关键字 */
    bool has_sqrt = (strstr(expr, "sqrt") != NULL);
    bool has_radical = (strstr(expr, "√") != NULL);

    if (!has_sqrt && !has_radical)
        return false;

    /* 检查是否只包含一个 sqrt 项（非嵌套） */
    const char *first;
    if (has_sqrt) {
        first = strstr(expr, "sqrt");
        if (first) {
            const char *second = strstr(first + 4, "sqrt");
            if (second)
                return false; /* 嵌套或复合 sqrt，不是简单二次根式 */
        }
    } else {
        first = strstr(expr, "√");
        if (first) {
            const char *second = strstr(first + 3, "√");
            if (second)
                return false;
        }
    }

    /* 检查是否包含幂运算（如 ^3, 立方等），这些会产生高次代数数 */
    if (strstr(expr, "^3") || strstr(expr, "^4") || strstr(expr, "cbrt") || strstr(expr, "cubic"))
        return false;

    return true;
}

/**
 * @brief 获取降级统计信息
 *
 * @param out_total 输出：总降级次数（可为 NULL）
 * @param out_current 输出：当前计划（可为 NULL）
 */
void symbolic_coord_plan_stats(int *out_total, AlgebraicPlan *out_current) {
    if (out_total)
        *out_total = g_degrade_total;
    if (out_current)
        *out_current = algebraic_get_plan();
}

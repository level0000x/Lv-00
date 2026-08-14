/**
 * @file rational.c
 * @brief Rational 公共 API（薄转发层）+ 位数电路/代数辅助函数
 *
 * @details 本文件原有的独立 Rational 实现与
 *          layer4_reasoning/expr/rational.c 的 lvRational 统一原语
 *          同构且语义相同（均为 mpq_t 单成员封装），已合并为单一实现体：
 *          实现体保留在 layer4_reasoning/expr/rational.c（约 640 行），
 *          本文件的 13 个 public API（rational_*，声明于 symbolic_coord.h）
 *          保持签名不变，薄转发到 lv_rational_*，调用点零改动。
 *
 *          行为保持说明：
 *          - rational_serialize 保留 "分子/分母" 恒定斜杠格式（与
 *            lv_rational_to_string 的"整数无斜杠"格式语义不同，按
 *            被调用最多侧（R1）的语义统一，测试守护不变）；
 *          - rational_to_double 直接 mpq_get_d（Inf/NaN 场景与原行为一致）；
 *          - rational_compare 对 NULL 参数保留原 ±1 语义；
 *          - rational_parse 转发 lv_rational_from_string（同基于
 *            mpq_set_str，失败返回 NULL，语义等价）。
 *
 *          本文件其余部分为位数电路（check_digit_circuit / circuit_*）
 *          与代数数辅助函数（evaluate_poly_at_double 等），与有理数
 *          实现无关，原样保留。
 *
 * @author Lv-00 Project
 * @version 3.4.0 (merged)
 */

#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/lv.h"
#include "lv/rational.h"
#include "lv/symbolic_coord.h"

#include "lv/debug.h"
#include "lv/lv_log.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/mpz_poly.h"

/* ── 外部溢出上下文 ── */
extern lv_THREAD_LOCAL struct OverflowContext g_overflow_context;

#define SYM_COORD_DYNAMIC_ARRAY_INIT_CAP 16
#define SYM_COORD_SIGFIGS_MIN_SAFE 6
#define SYM_COORD_SIGFIGS_APPROX 4
#define SYM_COORD_MAX_REFINE 15
#define SYM_COORD_AMB_MIN_SIGFIGS 3
#define COORD_SEVEN_OVER_FIVE_N 32
/* ── Rational type ── */

/* ────────────────────────────────────────────────────────────────
 * Rational 公共 API —— 薄转发到统一原语实现（lvRational, rational.h）
 * ──────────────────────────────────────────────────────────────── */

Rational *rational_create(int64_t numerator, uint64_t denominator) {
    return (Rational *) lv_rational_create_from_i64(numerator, denominator);
}

Rational *rational_create_from_mpz(const mpz_t numerator, const mpz_t denominator) {
    /* 保持原实现的分母为零 / 指针无效检查语义 */
    if (!numerator || !denominator)
        return NULL;
    return (Rational *) lv_rational_create_from_mpz(numerator, denominator);
}

Rational *rational_copy(const Rational *src) {
    return (Rational *) lv_rational_clone((const lvRational *) src);
}

void rational_destroy(Rational *r) {
    if (r) {
        lvRational *p = (lvRational *) r;
        lv_rational_destroy(&p);
    }
}

int rational_compare(const Rational *a, const Rational *b) {
    if (!a || !b) {
        /* 保持原语义：一空一非空时返回 ±1，均空返回 0 */
        return (a != b) ? ((a ? 1 : -1)) : 0;
    }
    return lv_rational_cmp((const lvRational *) a, (const lvRational *) b);
}

Rational *rational_add(const Rational *a, const Rational *b) {
    return (Rational *) lv_rational_add((const lvRational *) a, (const lvRational *) b);
}

Rational *rational_subtract(const Rational *a, const Rational *b) {
    return (Rational *) lv_rational_sub((const lvRational *) a, (const lvRational *) b);
}

Rational *rational_multiply(const Rational *a, const Rational *b) {
    return (Rational *) lv_rational_mul((const lvRational *) a, (const lvRational *) b);
}

Rational *rational_divide(const Rational *a, const Rational *b) {
    /* lv_rational_div 与原始实现一致：b 为零时返回 NULL */
    return (Rational *) lv_rational_div((const lvRational *) a, (const lvRational *) b);
}

Rational *rational_negate(const Rational *a) {
    return (Rational *) lv_rational_neg((const lvRational *) a);
}

/**
 * 将有理数序列化为 "分子/分母" 格式的字符串。
 *
 * 缓冲区大小 = 分子十进制位数 + 分母十进制位数 + 4（符号、斜杠、空终止符）
 * 额外多分配 4 个字节作为安全余量，防止 mpz_sizeinbase 估算值偏小导致缓冲区溢出。
 *
 * @param r  有理数对象
 * @return  新分配的字符串，调用者需负责释放；失败时返回 NULL
 */
char *rational_serialize(const Rational *r) {
    if (!r)
        return NULL;
    return lv_mpq_to_string(r->value, false);
}

Rational *rational_parse(const char *str) {
    /* 转发到统一实现（同基于 mpq_set_str，失败返回 NULL，语义等价） */
    return (Rational *) lv_rational_from_string(str);
}

/**
 * 将有理数转换为双精度浮点数。
 *
 * 直接使用 GMP 的 mpq_get_d，保持与原实现完全一致
 * （含极大值溢出为 Inf 的场景，与 lv_rational_to_double 的
 * 非有限值返回 false 语义不同）。
 *
 * @param r 有理数对象（不能为 NULL）
 * @return 有理数的双精度浮点数近似值
 */
double rational_to_double(const Rational *r) {
    return mpq_get_d(r->value);
}

/**
 * 检查有理数是否超过位数阈值。
 *
 * 计算分子和分母的比特位数总和，若超过 lv_BIT_CUTOFF_THRESHOLD 则触发熔断。
 *
 * @param r 有理数对象（不能为 NULL）
 * @return CIRCUIT_STATUS_OK 表示正常，CIRCUIT_STATUS_TRIPPED 表示超过阈值
 */
static CircuitStatus check_rational_circuit(const Rational *r) {
    if (!r)
        return CIRCUIT_STATUS_OK;
    size_t num_bits = mpz_sizeinbase(mpq_numref(r->value), 2);
    size_t den_bits = mpz_sizeinbase(mpq_denref(r->value), 2);
    /* 溢出保护：防止 num_bits + den_bits 在 size_t 范围内溢出 */
    if (num_bits > SIZE_MAX - den_bits) {
        return CIRCUIT_STATUS_TRIPPED;
    }
    if (num_bits + den_bits > lv_BIT_CUTOFF_THRESHOLD) {
        return CIRCUIT_STATUS_TRIPPED;
    }
    return CIRCUIT_STATUS_OK;
}

/**
 * 检查代数数是否超过位数阈值。
 *
 * 同时检查缓存的有理数和多项式系数的比特位数。
 *
 * @param a 代数数对象（不能为 NULL）
 * @return CIRCUIT_STATUS_OK 表示正常，CIRCUIT_STATUS_TRIPPED 表示超过阈值
 */
static CircuitStatus check_algebraic_circuit(const Algebraic *a) {
    if (!a)
        return CIRCUIT_STATUS_OK;

    /* 检查缓存的有理数（如果存在） */
    if (a->cached_rational) {
        CircuitStatus status = check_rational_circuit(a->cached_rational);
        if (status == CIRCUIT_STATUS_TRIPPED)
            return CIRCUIT_STATUS_TRIPPED;
    }

    /* 检查多项式系数 */
    for (int i = 0; i <= a->minimal_poly.degree; i++) {
        size_t coeff_bits = mpz_sizeinbase(a->minimal_poly.coeffs[i], 2);
        if (coeff_bits > lv_BIT_CUTOFF_THRESHOLD) {
            return CIRCUIT_STATUS_TRIPPED;
        }
    }

    return CIRCUIT_STATUS_OK;
}

/**
 * 检查二次根式是否超过位数阈值。
 *
 * @param q 二次根式对象（不能为 NULL）
 * @return CIRCUIT_STATUS_OK 表示正常，CIRCUIT_STATUS_TRIPPED 表示超过阈值
 */
static CircuitStatus check_quadratic_circuit(const Quadratic *q) {
    if (!q)
        return CIRCUIT_STATUS_OK;
    CircuitStatus status_a = check_rational_circuit(q->a);
    CircuitStatus status_b = check_rational_circuit(q->b);
    if (status_a == CIRCUIT_STATUS_TRIPPED || status_b == CIRCUIT_STATUS_TRIPPED) {
        return CIRCUIT_STATUS_TRIPPED;
    }
    return CIRCUIT_STATUS_OK;
}

/* ================================================================
 * 坐标类型 -> 位数电路检查函数 静态查找表（数据表化，替代 switch）
 * ================================================================ */

/** @brief 坐标位数电路检查函数指针类型 */
typedef CircuitStatus (*CoordCircuitCheckFn)(const SymbolicCoord *coord);

/** @brief RATIONAL 类型检查：委托 check_rational_circuit */
static CircuitStatus circuit_check_rational(const SymbolicCoord *coord) {
    return check_rational_circuit(coord->data.rational);
}

/** @brief ALGEBRAIC 类型检查：委托 check_algebraic_circuit */
static CircuitStatus circuit_check_algebraic(const SymbolicCoord *coord) {
    return check_algebraic_circuit(coord->data.algebraic);
}

/** @brief QUADRATIC 类型检查：委托 check_quadratic_circuit */
static CircuitStatus circuit_check_quadratic(const SymbolicCoord *coord) {
    return check_quadratic_circuit(coord->data.quadratic);
}

/** @brief TRANSCENDENTAL 类型检查：超越数没有比特位需要检查 */
static CircuitStatus circuit_check_transcendental(const SymbolicCoord *coord) {
    (void) coord;
    return CIRCUIT_STATUS_OK;
}

/** @brief 坐标类型 -> 检查函数 查找表（CoordType 枚举 0~3 连续，风格同 kCoordOpsVTable） */
static const CoordCircuitCheckFn s_coord_circuit_checkers[] = {
    [RATIONAL] = circuit_check_rational,
    [ALGEBRAIC] = circuit_check_algebraic,
    [QUADRATIC] = circuit_check_quadratic,
    [TRANSCENDENTAL] = circuit_check_transcendental,
};

/**
 * 检查 SymbolicCoord 是否超过位数阈值。
 *
 * 根据 design_v2.9.md Section 1.5：
 * "计算有理数分子和分母的比特位数总和，以及多项式的每个系数的比特位数。
 * 若任何值超过 10^6 比特，则触发位数熔断信号"
 *
 * @param coord SymbolicCoord 对象（不能为 NULL）
 * @return CIRCUIT_STATUS_OK 表示正常，CIRCUIT_STATUS_TRIPPED 表示超过阈值
 */
CircuitStatus check_digit_circuit(const SymbolicCoord *coord) {
    if (!coord)
        return CIRCUIT_STATUS_OK;

    /* 查表获取类型对应的检查函数；未知类型视为安全（原 default 分支） */
    CoordCircuitCheckFn checker = NULL;
    if ((unsigned) coord->type < lv_ARRAY_SIZE(s_coord_circuit_checkers)) {
        checker = s_coord_circuit_checkers[coord->type];
    }
    if (checker)
        return checker(coord);
    return CIRCUIT_STATUS_OK;
}

/**
 * 处理位数溢出事件。
 *
 * 根据 design_v2.9.md Section 1.5，用户选项：
 * 1. 忽略 (Ignore): 接受为"数值辅助"，标记为 AMBER
 * 2. 回退 (Rollback): 恢复到冻结点，撤销操作
 * 3. 永久降级 (Permanent Downgrade): 连续3次触发后，降级为数值近似
 */
void circuit_handle_overflow(void) {
    g_overflow_context.overflow_count++;

    /* 记录溢出事件 */
    LOG_WARN("[BIT CIRCUIT TRIPPED] Operation: %s, Count: %d\n",
             g_overflow_context.last_operation ? g_overflow_context.last_operation : "unknown",
             g_overflow_context.overflow_count);

    /* 连续超过阈值后，建议永久降级 */
    int threshold = lv_config_get_int(LV_CFG_CIRCUIT_OVERFLOW_THRESHOLD, 3);
    if (g_overflow_context.overflow_count >= threshold) {
        lv_log(lv_LOG_WARN, "[BIT CIRCUIT] Suggesting permanent downgrade to numerical approximation (AMBER)");
        /* 实际降级由调用者根据用户选择处理 */
    }
}

/**
 * 重置溢出上下文。
 *
 * 清除最近一次结果和操作记录，重置溢出计数器。
 * 注意：保留 frozen_point 以便回退时使用。
 */
void circuit_reset_context(void) {
    g_overflow_context.last_result = NULL;
    g_overflow_context.last_operation = NULL;
    g_overflow_context.overflow_count = 0;
}

/**
 * 设置回滚的冻结点快照。
 *
 * @param snapshot 指向要保存的快照的指针
 */
void circuit_set_frozen_point(void *snapshot) {
    g_overflow_context.frozen_point = snapshot;
    g_overflow_context.has_frozen_point = (snapshot != NULL);
}

/**
 * 获取当前溢出计数。
 *
 * @return 累计溢出次数
 */
int circuit_get_overflow_count(void) {
    return g_overflow_context.overflow_count;
}

/* ============================================================
 * Algebraic Number Implementation
 *
 * 注：原本文件维护的 evaluate_poly_at_double / refine_algebraic_bounds /
 * evaluate_algebraic_at_rational 三个 static 副本已删除——它们与本文件
 * 内无任何调用点（死代码），且与 algebraic.c 的唯一实现存在漂移
 * （缺少 NaN/Inf 检查与相对 epsilon 缩放）。如需使用，统一调用
 * algebraic.c 导出的 sym_evaluate_poly_double / sym_evaluate_algebraic_at_rational
 * （声明于 symbolic_coord_internal.h）；refine_algebraic_bounds 亦由
 * algebraic.c 提供全局版本。
 * ============================================================ */

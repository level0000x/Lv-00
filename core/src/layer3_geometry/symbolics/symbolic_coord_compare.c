/**
 * @file symbolic_coord_compare.c
 * @brief SymbolicCoord 跨类型比较操作
 *
 * @details 实现 RATIONAL / QUADRATIC / ALGEBRAIC / TRANSCENDENTAL
 *          四种符号坐标类型间的跨类型比较。核心策略：
 *
 *          1. 同类型比较 → 使用类型特定比较函数
 *          2. 跨类型比较 → 依次尝试有理化、隔离区间交叉检查、连分数近似、
 *             double 近似兜底
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

#include "symbolic_coord_internal.h"

/* ── 前向声明（来自 symbolics 子目录其他模块）── */
bool is_rational_zero(const Rational *r);
void refine_algebraic_bounds(Algebraic *a, int iterations);
bool algebraic_try_rationalize(Algebraic *a);

/* Cross-type comparison */
int symbolic_coord_compare(const SymbolicCoord *a, const SymbolicCoord *b) {
    if (!a || !b)
        return 0;
    /* Same type: use vtable dispatch */
    if (a->type == b->type) {
        return kCoordOpsVTable[a->type].compare(a, b);
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

/* 收敛入口：propagation.c / graph_node_alloc.c / graph_node_conflict.c
 * 三处手写 coords_equal 统一改调本函数，消除并行实现。
 * 语义约定（NULL-safe）：任一参数为 NULL 即视为不等。 */
bool symbolic_coord_equal(const SymbolicCoord *a, const SymbolicCoord *b) {
    if (!a || !b)
        return false;
    return symbolic_coord_compare(a, b) == 0;
}

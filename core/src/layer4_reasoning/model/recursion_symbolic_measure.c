/**
 * @file recursion_symbolic_measure.c
 * @brief pure symbolic measure computation
 * @details Split from recursion.c
 */

#include "lv/lv_platform.h"
#include "lv/recursion.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_internal.h"
#include "lv/lv_lifecycle.h"
#include "lv/lv_xmacro.h"
#include "lv/lv_utils.h"
#include "lv/stream.h"
#include "lv/stream_context_util.h"
#include "recursion_internal.h"

/* ============== 修改4：纯符号面积计算 ============== */

/**
 * @brief 纯符号测度值计算
 *
 * 使用纯符号坐标的代数运算计算测度值（如鞋带公式计算面积）。
 *
 * @param m     测度指针
 * @param node  几何节点指针
 * @param graph 约束图指针
 * @return 计算结果的符号坐标，失败返回 NULL
 */
/* 测度种类 → 纯符号计算函数（函数指针表分发；CUSTOM 未列出 → 走 default 返回 NULL） */
typedef SymbolicCoord *(*MeasureValueFn)(Measure *m, GeomNode *node, ConstraintGraph *graph);

static SymbolicCoord *measure_symbolic_area(Measure *m, GeomNode *node, ConstraintGraph *graph) {
    (void) m;
    (void) graph;
    /* 使用纯符号坐标的代数运算计算面积（鞋带公式） */
    if (node->type == GEOM_REGION && node->data.region.boundary_segments) {
        int seg_count = node->data.region.segment_count;
        if (seg_count < 3)
            return NULL; /* 至少需要3条边才能构成面积 */

        /*
         * 鞋带公式（符号版本）：
         * area = |sum_{i=0}^{n-1} (x_i * y_{i+1} - x_{i+1} * y_i)| / 2
         *
         * 对于多边形，每条线段提供两个端点。
         * 我们提取每个顶点坐标，然后应用鞋带公式。
         * 相邻线段共享端点，因此只需取每条线段的起点。
         */

        /* 提取顶点坐标数组 */
        /* 每条线段 seg[i] 的起点为 (x_i, y_i)，
         * 下一条线段 seg[(i+1)%n] 的起点为 (x_{i+1}, y_{i+1}) */
        SymbolicCoord *sum = symbolic_coord_create_rational(0, 1);
        if (!sum)
            return NULL;

        for (int i = 0; i < seg_count; i++) {
            GeomNode *seg_i = node->data.region.boundary_segments[i];
            if (!seg_i || seg_i->type != GEOM_LINE_SEGMENT || seg_i->coord_count < 4) {
                symbolic_coord_destroy(sum);
                return NULL;
            }

            /* 下一条线段（环形） */
            int next_i = (i + 1) % seg_count;
            GeomNode *seg_next = node->data.region.boundary_segments[next_i];
            if (!seg_next || seg_next->type != GEOM_LINE_SEGMENT || seg_next->coord_count < 4) {
                symbolic_coord_destroy(sum);
                return NULL;
            }

            /* 当前线段起点：(x_i, y_i) */
            SymbolicCoord *xi = seg_i->symbolic_coords[0];
            SymbolicCoord *yi = seg_i->symbolic_coords[1];

            /* 下一条线段起点：(x_{i+1}, y_{i+1}) */
            SymbolicCoord *xi1 = seg_next->symbolic_coords[0];
            SymbolicCoord *yi1 = seg_next->symbolic_coords[1];

            /* 计算叉积项：x_i * y_{i+1} - x_{i+1} * y_i */
            SymbolicCoord *term1 = symbolic_coord_multiply(xi, yi1);
            SymbolicCoord *term2 = symbolic_coord_multiply(xi1, yi);

            if (!term1 || !term2) {
                symbolic_coord_destroy(term1);
                symbolic_coord_destroy(term2);
                symbolic_coord_destroy(sum);
                return NULL;
            }

            SymbolicCoord *cross = symbolic_coord_subtract(term1, term2);
            symbolic_coord_destroy(term1);
            symbolic_coord_destroy(term2);

            if (!cross) {
                symbolic_coord_destroy(sum);
                return NULL;
            }

            /* 累加到总和 */
            SymbolicCoord *new_sum = symbolic_coord_add(sum, cross);
            symbolic_coord_destroy(sum);
            symbolic_coord_destroy(cross);

            if (!new_sum)
                return NULL;
            sum = new_sum;
        }

        /* 除以2：area = |sum| / 2 */
        SymbolicCoord *two = symbolic_coord_create_rational(2, 1);
        if (!two) {
            symbolic_coord_destroy(sum);
            return NULL;
        }

        SymbolicCoord *half_sum = symbolic_coord_divide(sum, two);
        symbolic_coord_destroy(sum);
        symbolic_coord_destroy(two);

        if (!half_sum)
            return NULL;

        /* 取绝对值：检查是否为负数，如果是则取反 */
        bool is_neg = symbolic_coord_is_negative(half_sum);
        if (is_neg) {
            SymbolicCoord *zero = symbolic_coord_create_rational(0, 1);
            SymbolicCoord *neg_half = symbolic_coord_subtract(zero, half_sum);
            symbolic_coord_destroy(zero);
            symbolic_coord_destroy(half_sum);
            return neg_half;
        }

        return half_sum;
    }
    return NULL;
}

/** @brief 作用域守卫清理回调：销毁 SymbolicCoord 指针变量（配合 lv_DEFER 使用） */
static void defer_symbolic_coord_destroy(void *arg) {
    symbolic_coord_destroy(*(SymbolicCoord **) arg);
}

static SymbolicCoord *measure_symbolic_length(Measure *m, GeomNode *node, ConstraintGraph *graph) {
    (void) m;
    (void) graph;
    /* 线段长度的纯符号计算（返回平方距离）
     *
     * 计算过程：dx = x2 - x1, dy = y2 - y1
     *           result = dx^2 + dy^2
     *
     * 中间 SymbolicCoord 对象在创建前即注册 lv_DEFER 作用域守卫，
     * 任意一步失败直接 return，出口按注册逆序自动销毁，无需手写
     * goto cleanup 标签。
     */
    if (!node->symbolic_coords)
        return NULL;
    if (node->type == GEOM_LINE_SEGMENT && node->coord_count >= 4) {
        SymbolicCoord *x1 = node->symbolic_coords[0];
        SymbolicCoord *y1 = node->symbolic_coords[1];
        SymbolicCoord *x2 = node->symbolic_coords[2];
        SymbolicCoord *y2 = node->symbolic_coords[3];

        SymbolicCoord *dx = NULL;
        SymbolicCoord *dy = NULL;
        SymbolicCoord *dx2 = NULL;
        SymbolicCoord *dy2 = NULL;
        lv_DEFER(defer_symbolic_coord_destroy, &dx);
        lv_DEFER(defer_symbolic_coord_destroy, &dy);
        lv_DEFER(defer_symbolic_coord_destroy, &dx2);
        lv_DEFER(defer_symbolic_coord_destroy, &dy2);

        dx = symbolic_coord_subtract(x2, x1);
        dy = symbolic_coord_subtract(y2, y1);
        if (!dx || !dy)
            return NULL;

        dx2 = symbolic_coord_multiply(dx, dx);
        dy2 = symbolic_coord_multiply(dy, dy);
        if (!dx2 || !dy2)
            return NULL;

        SymbolicCoord *sum = symbolic_coord_add(dx2, dy2);
        if (!sum)
            return NULL;

        /* 计算成功：dx/dy/dx2/dy2 由作用域守卫自动释放，sum 返回给调用者 */
        return sum;
    }
    return NULL;
}

static SymbolicCoord *measure_symbolic_depth(Measure *m, GeomNode *node, ConstraintGraph *graph) {
    (void) m;
    (void) graph;
    return symbolic_coord_create_rational(node->namespace_depth, 1);
}

static SymbolicCoord *measure_symbolic_angle(Measure *m, GeomNode *node, ConstraintGraph *graph) {
    (void) m;
    (void) graph;
    /*
     * 角度的纯符号计算
     * 需要三个点 A, B, C，计算角 ABC（B为顶点）
     * 支持两种输入格式：
     * 1. 节点包含三个点的坐标（6个坐标值）：(ax, ay, bx, by, cx, cy)
     * 2. 节点是一个点（顶点B），reference_node_id 指向另一个点C
     *
     * 使用向量点积和叉积计算角度的 cos 和 sin：
     * cos(theta) = (v1 . v2) / (|v1| * |v2|)
     * sin(theta) = |v1 x v2| / (|v1| * |v2|)
     *
     * 对于可判定的特殊角度（0, 30, 45, 60, 90, 120, 135, 150, 180度），
     * 返回精确的二次根式表示。
     */
    if (node->type == GEOM_POINT && node->coord_count >= 6 && node->symbolic_coords) {
        SymbolicCoord *ax = node->symbolic_coords[0];
        SymbolicCoord *ay = node->symbolic_coords[1];
        SymbolicCoord *bx = node->symbolic_coords[2];
        SymbolicCoord *by = node->symbolic_coords[3];
        SymbolicCoord *cx = node->symbolic_coords[4];
        SymbolicCoord *cy = node->symbolic_coords[5];

        /* 计算方向向量 BA = A - B 和 BC = C - B */
        SymbolicCoord *ba_x = symbolic_coord_subtract(ax, bx);
        SymbolicCoord *ba_y = symbolic_coord_subtract(ay, by);
        SymbolicCoord *bc_x = symbolic_coord_subtract(cx, bx);
        SymbolicCoord *bc_y = symbolic_coord_subtract(cy, by);

        if (!ba_x || !ba_y || !bc_x || !bc_y) {
            symbolic_coord_destroy(ba_x);
            symbolic_coord_destroy(ba_y);
            symbolic_coord_destroy(bc_x);
            symbolic_coord_destroy(bc_y);
            return NULL;
        }

        /* 计算点积：dot = BA . BC = ba_x * bc_x + ba_y * bc_y */
        SymbolicCoord *dot1 = symbolic_coord_multiply(ba_x, bc_x);
        SymbolicCoord *dot2 = symbolic_coord_multiply(ba_y, bc_y);
        SymbolicCoord *dot = symbolic_coord_add(dot1, dot2);

        /* 计算叉积（标量）：cross = ba_x * bc_y - ba_y * bc_x */
        SymbolicCoord *cross1 = symbolic_coord_multiply(ba_x, bc_y);
        SymbolicCoord *cross2 = symbolic_coord_multiply(ba_y, bc_x);
        SymbolicCoord *cross = symbolic_coord_subtract(cross1, cross2);

        /* 计算向量长度的平方 */
        SymbolicCoord *ba_x2 = symbolic_coord_multiply(ba_x, ba_x);
        SymbolicCoord *ba_y2 = symbolic_coord_multiply(ba_y, ba_y);
        SymbolicCoord *ba_len2 = symbolic_coord_add(ba_x2, ba_y2);
        SymbolicCoord *bc_x2 = symbolic_coord_multiply(bc_x, bc_x);
        SymbolicCoord *bc_y2 = symbolic_coord_multiply(bc_y, bc_y);
        SymbolicCoord *bc_len2 = symbolic_coord_add(bc_x2, bc_y2);

        /* 清理中间变量 */
        symbolic_coord_destroy(ba_x);
        symbolic_coord_destroy(ba_y);
        symbolic_coord_destroy(bc_x);
        symbolic_coord_destroy(bc_y);
        symbolic_coord_destroy(dot1);
        symbolic_coord_destroy(dot2);
        symbolic_coord_destroy(cross1);
        symbolic_coord_destroy(cross2);
        symbolic_coord_destroy(ba_x2);
        symbolic_coord_destroy(ba_y2);
        symbolic_coord_destroy(bc_x2);
        symbolic_coord_destroy(bc_y2);

        if (!dot || !cross || !ba_len2 || !bc_len2) {
            symbolic_coord_destroy(dot);
            symbolic_coord_destroy(cross);
            symbolic_coord_destroy(ba_len2);
            symbolic_coord_destroy(bc_len2);
            return NULL;
        }

        /* 检查退化情况（零向量） */
        SymbolicCoord *zero = symbolic_coord_create_rational(0, 1);
        int ba_zero = symbolic_coord_compare(ba_len2, zero);
        int bc_zero = symbolic_coord_compare(bc_len2, zero);
        symbolic_coord_destroy(zero);
        if (ba_zero == 0 || bc_zero == 0) {
            symbolic_coord_destroy(dot);
            symbolic_coord_destroy(cross);
            symbolic_coord_destroy(ba_len2);
            symbolic_coord_destroy(bc_len2);
            return NULL;
        }

        /*
         * 特殊角度检测：
         * 使用 cos^2(theta) = dot^2 / (|BA|^2 * |BC|^2) 和
         * sin^2(theta) = cross^2 / (|BA|^2 * |BC|^2) 来判定特殊角度。
         *
         * 特殊角度的 cos^2 值：
         *   0度:   cos^2 = 1,    sin^2 = 0
         *   30度:  cos^2 = 3/4,  sin^2 = 1/4
         *   45度:  cos^2 = 1/2,  sin^2 = 1/2
         *   60度:  cos^2 = 1/4,  sin^2 = 3/4
         *   90度:  cos^2 = 0,    sin^2 = 1
         *   120度: cos^2 = 1/4,  sin^2 = 3/4 (cos为负)
         *   135度: cos^2 = 1/2,  sin^2 = 1/2 (cos为负)
         *   150度: cos^2 = 3/4,  sin^2 = 1/4 (cos为负)
         *   180度: cos^2 = 1,    sin^2 = 0 (cos为负)
         */

        SymbolicCoord *dot_sq = symbolic_coord_multiply(dot, dot);
        SymbolicCoord *cross_sq = symbolic_coord_multiply(cross, cross);
        SymbolicCoord *len_product = symbolic_coord_multiply(ba_len2, bc_len2);

        /*
         * 在销毁 dot 和 cross 之前，先保存它们的符号。
         * 这两个符号的组合可以唯一确定角度所在的象限：
         *
         *   cross > 0 且 dot > 0: 第一象限，锐角 (0-90°)
         *   cross > 0 且 dot < 0: 第二象限，钝角 (90-180°)
         *   cross < 0 且 dot < 0: 第三象限，负钝角 (-180°--90°)
         *   cross < 0 且 dot > 0: 第四象限，负锐角 (-90°-0°)
         *
         * 对于互补角度的区分（cos^2 相同的角度对）：
         *   30° vs 150°: cos^2 = 3/4, 用 dot 符号区分
         *   45° vs 135°: cos^2 = 1/2, 用 dot 符号区分
         *   60° vs 120°: cos^2 = 1/4, 用 dot 符号区分
         *   0° vs 180°:  cross = 0, 用 dot 符号区分
         */
        SymbolicCoord *sign_zero = symbolic_coord_create_rational(0, 1);
        int cross_sign = symbolic_coord_compare(cross, sign_zero);
        int dot_sign = symbolic_coord_compare(dot, sign_zero);
        symbolic_coord_destroy(sign_zero);

        symbolic_coord_destroy(dot);
        symbolic_coord_destroy(cross);
        symbolic_coord_destroy(ba_len2);
        symbolic_coord_destroy(bc_len2);

        if (!dot_sq || !cross_sq || !len_product) {
            symbolic_coord_destroy(dot_sq);
            symbolic_coord_destroy(cross_sq);
            symbolic_coord_destroy(len_product);
            return NULL;
        }

        /* 计算 cos^2(theta) = dot_sq / len_product */
        SymbolicCoord *cos_sq = symbolic_coord_divide(dot_sq, len_product);
        symbolic_coord_destroy(dot_sq);
        symbolic_coord_destroy(len_product);

        if (!cos_sq) {
            symbolic_coord_destroy(cross_sq);
            return NULL;
        }

        /* 检测直角：dot == 0 */
        SymbolicCoord *zero2 = symbolic_coord_create_rational(0, 1);
        int dot_is_zero = symbolic_coord_compare(cos_sq, zero2);
        symbolic_coord_destroy(zero2);

        if (dot_is_zero == 0) {
            /* 直角 (90度) */
            symbolic_coord_destroy(cos_sq);
            symbolic_coord_destroy(cross_sq);
            /* 返回 pi/2 的符号表示 */
            return symbolic_coord_create_transcendental("pi/2");
        }

        /* 检测 0度 或 180度：cross == 0 */
        SymbolicCoord *zero3 = symbolic_coord_create_rational(0, 1);
        int cross_is_zero = symbolic_coord_compare(cross_sq, zero3);
        symbolic_coord_destroy(zero3);

        if (cross_is_zero == 0) {
            /* 0度 或 180度：用 dot 的符号区分 */
            symbolic_coord_destroy(cross_sq);
            SymbolicCoord *one = symbolic_coord_create_rational(1, 1);
            int cos_is_one = symbolic_coord_compare(cos_sq, one);
            symbolic_coord_destroy(one);

            if (cos_is_one == 0) {
                symbolic_coord_destroy(cos_sq);
                if (dot_sign > 0) {
                    /* dot > 0: 0度（同方向） */
                    return symbolic_coord_create_rational(0, 1);
                } else if (dot_sign < 0) {
                    /* dot < 0: 180度（反方向） */
                    return symbolic_coord_create_transcendental("pi");
                } else {
                    /* dot == 0 且 cross == 0: 退化情况，返回 0 */
                    return symbolic_coord_create_rational(0, 1);
                }
            }
            symbolic_coord_destroy(cos_sq);
            return NULL;
        }

        /* 检测 45度 和 135度：cos^2 = 1/2 */
        {
            SymbolicCoord *half = symbolic_coord_create_rational(1, 2);
            int is_45 = symbolic_coord_compare(cos_sq, half);
            symbolic_coord_destroy(half);

            if (is_45 == 0) {
                symbolic_coord_destroy(cos_sq);
                symbolic_coord_destroy(cross_sq);
                /*
                 * 45度 或 135度：用 dot 和 cross 的符号区分。
                 * cross > 0 且 dot > 0: 45度 (第一象限)
                 * cross > 0 且 dot < 0: 135度 (第二象限)
                 * cross < 0 且 dot < 0: -135度 (第三象限)
                 * cross < 0 且 dot > 0: -45度 (第四象限)
                 */
                if (cross_sign > 0 && dot_sign > 0) {
                    return symbolic_coord_create_transcendental("pi/4");
                } else if (cross_sign > 0 && dot_sign < 0) {
                    return symbolic_coord_create_transcendental("3*pi/4");
                } else if (cross_sign < 0 && dot_sign < 0) {
                    return symbolic_coord_create_transcendental("-3*pi/4");
                } else if (cross_sign < 0 && dot_sign > 0) {
                    return symbolic_coord_create_transcendental("-pi/4");
                } else {
                    /* cross_sign == 0 或 dot_sign == 0: 退化情况 */
                    return symbolic_coord_create_transcendental("pi/4");
                }
            }
        }

        /* 检测 30度 和 150度：cos^2 = 3/4 */
        {
            SymbolicCoord *three_quarters = symbolic_coord_create_rational(3, 4);
            int is_30 = symbolic_coord_compare(cos_sq, three_quarters);
            symbolic_coord_destroy(three_quarters);

            if (is_30 == 0) {
                symbolic_coord_destroy(cos_sq);
                symbolic_coord_destroy(cross_sq);
                /*
                 * 30度 或 150度：用 dot 和 cross 的符号区分。
                 * cross > 0 且 dot > 0: 30度 (第一象限)
                 * cross > 0 且 dot < 0: 150度 (第二象限)
                 * cross < 0 且 dot < 0: -150度 (第三象限)
                 * cross < 0 且 dot > 0: -30度 (第四象限)
                 */
                if (cross_sign > 0 && dot_sign > 0) {
                    return symbolic_coord_create_transcendental("pi/6");
                } else if (cross_sign > 0 && dot_sign < 0) {
                    return symbolic_coord_create_transcendental("5*pi/6");
                } else if (cross_sign < 0 && dot_sign < 0) {
                    return symbolic_coord_create_transcendental("-5*pi/6");
                } else if (cross_sign < 0 && dot_sign > 0) {
                    return symbolic_coord_create_transcendental("-pi/6");
                } else {
                    return symbolic_coord_create_transcendental("pi/6");
                }
            }
        }

        /* 检测 60度 和 120度：cos^2 = 1/4 */
        {
            SymbolicCoord *quarter = symbolic_coord_create_rational(1, 4);
            int is_60 = symbolic_coord_compare(cos_sq, quarter);
            symbolic_coord_destroy(quarter);

            if (is_60 == 0) {
                symbolic_coord_destroy(cos_sq);
                symbolic_coord_destroy(cross_sq);
                /*
                 * 60度 或 120度：用 dot 和 cross 的符号区分。
                 * cross > 0 且 dot > 0: 60度 (第一象限)
                 * cross > 0 且 dot < 0: 120度 (第二象限)
                 * cross < 0 且 dot < 0: -120度 (第三象限)
                 * cross < 0 且 dot > 0: -60度 (第四象限)
                 */
                if (cross_sign > 0 && dot_sign > 0) {
                    return symbolic_coord_create_transcendental("pi/3");
                } else if (cross_sign > 0 && dot_sign < 0) {
                    return symbolic_coord_create_transcendental("2*pi/3");
                } else if (cross_sign < 0 && dot_sign < 0) {
                    return symbolic_coord_create_transcendental("-2*pi/3");
                } else if (cross_sign < 0 && dot_sign > 0) {
                    return symbolic_coord_create_transcendental("-pi/3");
                } else {
                    return symbolic_coord_create_transcendental("pi/3");
                }
            }
        }

        /*
         * 非特殊角度：返回 cos^2(theta) 作为有理近似
         * 用于后续的角度大小比较（cos^2 越大，角度越小）
         */
        symbolic_coord_destroy(cross_sq);
        return cos_sq;
    }
    return NULL;
}

static const MeasureValueFn kMeasureValueFns[] = {
    [MEASURE_KIND_AREA]   = measure_symbolic_area,
    [MEASURE_KIND_LENGTH] = measure_symbolic_length,
    [MEASURE_KIND_DEPTH]  = measure_symbolic_depth,
    [MEASURE_KIND_ANGLE]  = measure_symbolic_angle,
};

SymbolicCoord *measure_compute_value_symbolic(Measure *m, GeomNode *node, ConstraintGraph *graph) {
    if (!m || !node)
        return NULL;

    if (m->type != MEASURE_SYMBOLIC) {
        return NULL;
    }

    /* 测度种类 → 计算函数表分发（CUSTOM/越界走 default → NULL） */
    if ((unsigned) m->kind < sizeof(kMeasureValueFns) / sizeof(kMeasureValueFns[0]) &&
        kMeasureValueFns[m->kind]) {
        return kMeasureValueFns[m->kind](m, node, graph);
    }
    return NULL;
}

/**
 * @brief 比较两个测度值
 *
 * 符号测度使用代数比较，非符号测度返回 MEASURE_UNKNOWN。
 *
 * @param m 测度指针
 * @param a 第一个测度值
 * @param b 第二个测度值
 * @return 比较结果（LESS/EQUAL/GREATER/UNKNOWN/ERROR）
 */
MeasureCompareResult measure_compare(Measure *m, SymbolicCoord *a, SymbolicCoord *b) {
    if (!m || !a || !b)
        return MEASURE_ERROR;

    /* 符号测度使用代数比较 */
    if (m->type == MEASURE_SYMBOLIC) {
        int cmp = symbolic_coord_compare(a, b);
        if (cmp < 0)
            return MEASURE_LESS;
        if (cmp > 0)
            return MEASURE_GREATER;
        return MEASURE_EQUAL;
    }

    /* 非符号测度需要自定义比较函数 */
    return MEASURE_UNKNOWN;
}

/**
 * @brief 比较两个节点的测度值
 *
 * 先计算两个节点的测度值，再进行比较。
 *
 * @param m     测度指针
 * @param a     第一个几何节点
 * @param b     第二个几何节点
 * @param graph 约束图指针
 * @return 比较结果
 */
MeasureCompareResult measure_compare_nodes(Measure *m, GeomNode *a, GeomNode *b, const ConstraintGraph *graph) {
    if (!m || !a || !b)
        return MEASURE_ERROR;

    if (m->type == MEASURE_SYMBOLIC) {
        SymbolicCoord *val_a = measure_compute_value(m, a, graph);
        SymbolicCoord *val_b = measure_compute_value(m, b, graph);

        if (!val_a || !val_b) {
            symbolic_coord_destroy(val_a);
            symbolic_coord_destroy(val_b);
            return MEASURE_UNKNOWN;
        }

        MeasureCompareResult result = measure_compare(m, val_a, val_b);

        symbolic_coord_destroy(val_a);
        symbolic_coord_destroy(val_b);

        return result;
    }

    /* 非符号测度使用自定义比较函数 */
    if (m->compare_func) {
        int cmp = m->compare_func(a, b, m->user_data);
        if (cmp < 0)
            return MEASURE_LESS;
        if (cmp > 0)
            return MEASURE_GREATER;
        return MEASURE_EQUAL;
    }

    return MEASURE_UNKNOWN;
}

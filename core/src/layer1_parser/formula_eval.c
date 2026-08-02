/**
 * @file formula_eval.c
 * @brief 公式节点数值求值实现（从 formula_converter.c 拆分）
 *
 * @details 将 FormulaNode 抽象语法树在指定点 (x, y) 处数值求值，
 *          支持数字/变量/二元运算/一元函数/几何节点（点线圆三角形多边形区域圆弧）。
 */

#include "formula_converter_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/lv_numeric.h"
#include "lv_utils.h"

double eval_node(const FormulaNode *node, double x, double y);
typedef double (*EvalNodeFunc)(const FormulaNode *node, double x, double y);
static double eval_number(const FormulaNode *node, double x, double y) {
            if (node->data.number.is_integer) {
                return (double) node->data.number.numerator;
            } else {
                if (node->data.number.denominator == 0)
                    return 0.0;
                return (double) node->data.number.numerator / (double) node->data.number.denominator;
            }


}
static double eval_variable(const FormulaNode *node, double x, double y) {
            /* 变量名可能是 'x' 或 'y' */
            if (node->data.variable.name) {
                if (strcmp(node->data.variable.name, "x") == 0) {
                    return x;
                } else if (strcmp(node->data.variable.name, "y") == 0) {
                    return y;
                }
            }
            return 0.0;
        }

static double eval_identifier(const FormulaNode *node, double x, double y) {
            /* 标识符作为变量处理 */
            if (node->data.identifier.name) {
                if (strcmp(node->data.identifier.name, "x") == 0) {
                    return x;
                } else if (strcmp(node->data.identifier.name, "y") == 0) {
                    return y;
                }
            }
            return 0.0;
        }

static double eval_b_add(const FormulaNode *node, double x, double y) {
            double l = eval_node(node->data.binary_op.left, x, y);
            double r = eval_node(node->data.binary_op.right, x, y);
            return l + r;
        }

static double eval_b_sub(const FormulaNode *node, double x, double y) {
            double l = eval_node(node->data.binary_op.left, x, y);
            double r = eval_node(node->data.binary_op.right, x, y);
            return l - r;
        }

static double eval_b_mul(const FormulaNode *node, double x, double y) {
            double l = eval_node(node->data.binary_op.left, x, y);
            double r = eval_node(node->data.binary_op.right, x, y);
            return l * r;
        }

static double eval_b_div(const FormulaNode *node, double x, double y) {
            double l = eval_node(node->data.binary_op.left, x, y);
            double r = eval_node(node->data.binary_op.right, x, y);
            /* 使用容差检查代替精确零比较，防止次正规数除法溢出 */
            return (fabs(r) > 1e-15) ? l / r : 0.0;
        }

static double eval_b_pow(const FormulaNode *node, double x, double y) {
            double l = eval_node(node->data.binary_op.left, x, y);
            double r = eval_node(node->data.binary_op.right, x, y);
            /* Guard: pow(negative, non-integer) is undefined in reals.
             * Return 0.0 for consistency with the SQRT handling below. */
            if (l < 0.0 && fabs(r - round(r)) > 1e-12)
                return 0.0;
            return pow(l, r);
        }

static double eval_u_neg(const FormulaNode *node, double x, double y) {
            double v = eval_node(node->data.unary_op.operand, x, y);
            return -v;
        }

static double eval_u_sqrt(const FormulaNode *node, double x, double y) {
            double v = eval_node(node->data.unary_op.operand, x, y);
            return (v >= 0) ? sqrt(v) : 0.0;
        }

static double eval_u_sin(const FormulaNode *node, double x, double y) {
            double v = eval_node(node->data.unary_op.operand, x, y);
            return sin(v);
        }

static double eval_u_cos(const FormulaNode *node, double x, double y) {
            double v = eval_node(node->data.unary_op.operand, x, y);
            return cos(v);
        }

static double eval_u_tan(const FormulaNode *node, double x, double y) {
            double v = eval_node(node->data.unary_op.operand, x, y);
            /* tan(x) 在 x ≈ π/2 + nπ 处发散为 HUGE_VAL，使用容差避开奇点 */
            double rem = fmod(v + M_PI_2, M_PI);
            if (lv_is_zero(rem, lv_EPSILON_DOUBLE) || lv_is_equal(rem, M_PI, lv_EPSILON_DOUBLE)) {
                return 0.0;
            }
            return tan(v);
        }

static double eval_u_abs(const FormulaNode *node, double x, double y) {
            double v = eval_node(node->data.unary_op.operand, x, y);
            return fabs(v);
        }

static double eval_u_ln(const FormulaNode *node, double x, double y) {
            double v = eval_node(node->data.unary_op.operand, x, y);
            return (v > 0) ? log(v) : 0.0;
        }

static double eval_u_log(const FormulaNode *node, double x, double y) {
            double v = eval_node(node->data.unary_op.operand, x, y);
            return (v > 0) ? log10(v) : 0.0;
        }

static double eval_equation(const FormulaNode *node, double x, double y) {
            /* 方程: 返回 lhs - rhs 的值 (零值表示在曲线上) */
            double l = eval_node(node->data.equation.lhs, x, y);
            double r = eval_node(node->data.equation.rhs, x, y);
            return l - r;
        }

/** 从点节点提取坐标；若节点不是合法点或坐标不足，则输出保持 0.0 */
static void eval_point_xy(const FormulaNode *point, double x, double y, double *ox, double *oy) {
            *ox = 0.0;
            *oy = 0.0;
            if (point && point->type == NODE_GEOM_POINT) {
                const FormulaNode *coords = point->data.geom_point.coords;
                if (coords && coords->type == NODE_COORDINATE_LIST && coords->data.coord_list.coord_count >= 2) {
                    *ox = eval_node(coords->data.coord_list.coords[0], x, y);
                    *oy = eval_node(coords->data.coord_list.coords[1], x, y);
                }
            }
        }

static double eval_g_point(const FormulaNode *node, double x, double y) {
            /* 点: 返回坐标值的组合 (x + y) */
            double px, py;
            eval_point_xy(node, x, y, &px, &py);
            return px + py;
        }

static double eval_g_segment(const FormulaNode *node, double x, double y) {
            /* 线段: 返回长度 */
            double x1, y1, x2, y2;
            eval_point_xy(node->data.geom_segment.endpoint1, x, y, &x1, &y1);
            eval_point_xy(node->data.geom_segment.endpoint2, x, y, &x2, &y2);
            return sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
        }

static double eval_g_circle(const FormulaNode *node, double x, double y) {
            /* 圆: 返回半径 */
            if (node->data.geom_circle.radius) {
                return eval_node(node->data.geom_circle.radius, x, y);
            }
            return 0.0;
        }

static double eval_g_triangle(const FormulaNode *node, double x, double y) {
            /* 三角形: 返回面积 (使用海伦公式) */
            double x1, y1, x2, y2, x3, y3;
            eval_point_xy(node->data.geom_triangle.vertex1, x, y, &x1, &y1);
            eval_point_xy(node->data.geom_triangle.vertex2, x, y, &x2, &y2);
            eval_point_xy(node->data.geom_triangle.vertex3, x, y, &x3, &y3);
            /* 使用叉积公式计算面积: 0.5 * |x1(y2-y3) + x2(y3-y1) + x3(y1-y2)| */
            return 0.5 * fabs(x1 * (y2 - y3) + x2 * (y3 - y1) + x3 * (y1 - y2));
        }

static double eval_g_polygon(const FormulaNode *node, double x, double y) {
            /* 多边形: 返回面积 (使用鞋带公式) */
            double area = 0.0;
            int n = node->data.geom_polygon.vertex_count;
            if (n < 3)
                return 0.0;
            for (int i = 0; i < n; i++) {
                FormulaNode *vi = node->data.geom_polygon.vertices[i];
                FormulaNode *vj = node->data.geom_polygon.vertices[(i + 1) % n];
                double xi, yi, xj, yj;
                eval_point_xy(vi, x, y, &xi, &yi);
                eval_point_xy(vj, x, y, &xj, &yj);
                area += xi * yj - xj * yi;
            }
            return 0.5 * fabs(area);
        }

static double eval_g_region(const FormulaNode *node, double x, double y) {
            /* 区域: 返回边界总长度 */
            double perimeter = 0.0;
            int n = node->data.geom_region.segment_count;
            for (int i = 0; i < n; i++) {
                FormulaNode *seg = node->data.geom_region.boundary_segments[i];
                if (seg) {
                    perimeter += eval_node(seg, x, y);
                }
            }
            return perimeter;
        }

static double eval_g_arc(const FormulaNode *node, double x, double y) {
            /* 弧: 返回弧长 = r * |theta2 - theta1| */
            double r = 0.0, theta1 = 0.0, theta2 = 0.0;
            if (node->data.geom_arc.radius) {
                r = eval_node(node->data.geom_arc.radius, x, y);
            }
            if (node->data.geom_arc.start_angle) {
                theta1 = eval_node(node->data.geom_arc.start_angle, x, y);
            }
            if (node->data.geom_arc.end_angle) {
                theta2 = eval_node(node->data.geom_arc.end_angle, x, y);
            }
            return r * fabs(theta2 - theta1);
        }


/**
 * 评估公式节点在特定点的值
 */
double eval_node(const FormulaNode *node, double x, double y) {
    if (!node)
        return 0.0;

    static const EvalNodeFunc s_funcs[] = {
        [NODE_NUMBER] = eval_number,
        [NODE_VARIABLE] = eval_variable,
        [NODE_IDENTIFIER] = eval_identifier,
        [NODE_BINARY_OP_ADD] = eval_b_add,
        [NODE_BINARY_OP_SUB] = eval_b_sub,
        [NODE_BINARY_OP_MUL] = eval_b_mul,
        [NODE_BINARY_OP_DIV] = eval_b_div,
        [NODE_BINARY_OP_POW] = eval_b_pow,
        [NODE_UNARY_OP_NEG] = eval_u_neg,
        [NODE_UNARY_OP_SQRT] = eval_u_sqrt,
        [NODE_UNARY_OP_SIN] = eval_u_sin,
        [NODE_UNARY_OP_COS] = eval_u_cos,
        [NODE_UNARY_OP_TAN] = eval_u_tan,
        [NODE_UNARY_OP_ABS] = eval_u_abs,
        [NODE_UNARY_OP_LN] = eval_u_ln,
        [NODE_UNARY_OP_LOG] = eval_u_log,
        [NODE_EQUATION] = eval_equation,
        [NODE_GEOM_POINT] = eval_g_point,
        [NODE_GEOM_SEGMENT] = eval_g_segment,
        [NODE_GEOM_CIRCLE] = eval_g_circle,
        [NODE_GEOM_TRIANGLE] = eval_g_triangle,
        [NODE_GEOM_POLYGON] = eval_g_polygon,
        [NODE_GEOM_REGION] = eval_g_region,
        [NODE_GEOM_ARC] = eval_g_arc,
    };
    if ((unsigned)node->type < sizeof(s_funcs)/sizeof(s_funcs[0]) && s_funcs[node->type]) {
        return s_funcs[node->type](node, x, y);
    }
    return 0.0;
}

/**
 * @file formula_string.c
 * @brief 公式节点字符串渲染实现（从 formula_converter.c 拆分）
 *
 * @details 将 FormulaNode 抽象语法树渲染为可读字符串，
 *          支持数字/变量/二元运算/方程/几何节点。
 */

#include "formula_converter_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv_utils.h"

/**
 * 将公式节点渲染为字符串（简化版）
 */
void node_to_string(const FormulaNode *node, char *buf, size_t buf_size);
typedef void (*NodeToStringFunc)(const FormulaNode *node, char *buf, size_t buf_size);
static void str_number(const FormulaNode *node, char *buf, size_t buf_size) {
            if (node->data.number.is_integer) {
                snprintf(buf, buf_size, "%lld", (long long) node->data.number.numerator);
            } else {
                snprintf(buf, buf_size, "%lld/%llu", (long long) node->data.number.numerator,
                         (unsigned long long) node->data.number.denominator);
            }


}
static void str_variable(const FormulaNode *node, char *buf, size_t buf_size) {
            if (node->data.variable.name) {
                /* 使用 lv_strlcpy 替代不安全的 strncpy */
                lv_strlcpy(buf, node->data.variable.name, buf_size);
            }


}
static void str_identifier(const FormulaNode *node, char *buf, size_t buf_size) {
            if (node->data.identifier.name) {
                /* 使用 lv_strlcpy 替代不安全的 strncpy */
                lv_strlcpy(buf, node->data.identifier.name, buf_size);
            }


}
static void str_b_add(const FormulaNode *node, char *buf, size_t buf_size) {
            char left[FORMULA_EXPR_BUF_SIZE], right[FORMULA_EXPR_BUF_SIZE];
            node_to_string(node->data.binary_op.left, left, sizeof(left));
            node_to_string(node->data.binary_op.right, right, sizeof(right));
            int n = snprintf(buf, buf_size, "(%s + %s)", left, right);
            if (n < 0 || (size_t) n >= buf_size) {
                /* 缓冲区不足时使用安全截断标记 */
                lv_strlcpy(buf, "(... + ...)", buf_size);
            }
        }

static void str_b_sub(const FormulaNode *node, char *buf, size_t buf_size) {
            char left[FORMULA_EXPR_BUF_SIZE], right[FORMULA_EXPR_BUF_SIZE];
            node_to_string(node->data.binary_op.left, left, sizeof(left));
            node_to_string(node->data.binary_op.right, right, sizeof(right));
            int n = snprintf(buf, buf_size, "(%s - %s)", left, right);
            if (n < 0 || (size_t) n >= buf_size) {
                lv_strlcpy(buf, "(... - ...)", buf_size);
            }
        }

static void str_b_mul(const FormulaNode *node, char *buf, size_t buf_size) {
            char left[FORMULA_EXPR_BUF_SIZE], right[FORMULA_EXPR_BUF_SIZE];
            node_to_string(node->data.binary_op.left, left, sizeof(left));
            node_to_string(node->data.binary_op.right, right, sizeof(right));
            int n = snprintf(buf, buf_size, "(%s * %s)", left, right);
            if (n < 0 || (size_t) n >= buf_size) {
                lv_strlcpy(buf, "(... * ...)", buf_size);
            }
        }

static void str_b_div(const FormulaNode *node, char *buf, size_t buf_size) {
            char left[FORMULA_EXPR_BUF_SIZE], right[FORMULA_EXPR_BUF_SIZE];
            node_to_string(node->data.binary_op.left, left, sizeof(left));
            node_to_string(node->data.binary_op.right, right, sizeof(right));
            int n = snprintf(buf, buf_size, "(%s / %s)", left, right);
            if (n < 0 || (size_t) n >= buf_size) {
                lv_strlcpy(buf, "(... / ...)", buf_size);
            }
        }

static void str_b_pow(const FormulaNode *node, char *buf, size_t buf_size) {
            char left[FORMULA_EXPR_BUF_SIZE], right[FORMULA_EXPR_BUF_SIZE];
            node_to_string(node->data.binary_op.left, left, sizeof(left));
            node_to_string(node->data.binary_op.right, right, sizeof(right));
            int n = snprintf(buf, buf_size, "(%s ^ %s)", left, right);
            if (n < 0 || (size_t) n >= buf_size) {
                lv_strlcpy(buf, "(... ^ ...)", buf_size);
            }
        }

static void str_equation(const FormulaNode *node, char *buf, size_t buf_size) {
            char left[FORMULA_EXPR_BUF_SIZE], right[FORMULA_EXPR_BUF_SIZE];
            node_to_string(node->data.equation.lhs, left, sizeof(left));
            node_to_string(node->data.equation.rhs, right, sizeof(right));
            int n = snprintf(buf, buf_size, "(%s = %s)", left, right);
            if (n < 0 || (size_t) n >= buf_size) {
                lv_strlcpy(buf, "(... = ...)", buf_size);
            }
        }

static void str_g_point(const FormulaNode *node, char *buf, size_t buf_size) {
            const char *name = node->data.geom_point.name ? node->data.geom_point.name : "?";
            int n = snprintf(buf, buf_size, "point(%s)", name);
            if (n < 0 || (size_t) n >= buf_size) {
                lv_strlcpy(buf, "point(...)", buf_size);
            }
        }

static void str_g_segment(const FormulaNode *node, char *buf, size_t buf_size) {
            const char *name = node->data.geom_segment.name ? node->data.geom_segment.name : "?";
            int n = snprintf(buf, buf_size, "segment(%s)", name);
            if (n < 0 || (size_t) n >= buf_size) {
                lv_strlcpy(buf, "segment(...)", buf_size);
            }
        }

static void str_g_circle(const FormulaNode *node, char *buf, size_t buf_size) {
            const char *name = node->data.geom_circle.name ? node->data.geom_circle.name : "?";
            char r[FORMULA_RESULT_BUF_SIZE] = "?";
            if (node->data.geom_circle.radius) {
                node_to_string(node->data.geom_circle.radius, r, sizeof(r));
            }
            int n = snprintf(buf, buf_size, "circle(%s, r=%s)", name, r);
            if (n < 0 || (size_t) n >= buf_size) {
                lv_strlcpy(buf, "circle(...)", buf_size);
            }
        }

static void str_g_triangle(const FormulaNode *node, char *buf, size_t buf_size) {
            const char *name = node->data.geom_triangle.name ? node->data.geom_triangle.name : "?";
            int n = snprintf(buf, buf_size, "triangle(%s)", name);
            if (n < 0 || (size_t) n >= buf_size) {
                lv_strlcpy(buf, "triangle(...)", buf_size);
            }
        }

static void str_g_polygon(const FormulaNode *node, char *buf, size_t buf_size) {
            const char *name = node->data.geom_polygon.name ? node->data.geom_polygon.name : "?";
            int n = snprintf(buf, buf_size, "polygon(%s, %d vertices)", name, node->data.geom_polygon.vertex_count);
            if (n < 0 || (size_t) n >= buf_size) {
                lv_strlcpy(buf, "polygon(...)", buf_size);
            }
        }

static void str_g_region(const FormulaNode *node, char *buf, size_t buf_size) {
            const char *name = node->data.geom_region.name ? node->data.geom_region.name : "?";
            int n = snprintf(buf, buf_size, "region(%s, %d segments)", name, node->data.geom_region.segment_count);
            if (n < 0 || (size_t) n >= buf_size) {
                lv_strlcpy(buf, "region(...)", buf_size);
            }
        }

static void str_g_arc(const FormulaNode *node, char *buf, size_t buf_size) {
            const char *name = node->data.geom_arc.name ? node->data.geom_arc.name : "?";
            char r[FORMULA_RESULT_BUF_SIZE] = "?", t1[FORMULA_RESULT_BUF_SIZE] = "?", t2[FORMULA_RESULT_BUF_SIZE] = "?";
            if (node->data.geom_arc.radius) {
                node_to_string(node->data.geom_arc.radius, r, sizeof(r));
            }
            if (node->data.geom_arc.start_angle) {
                node_to_string(node->data.geom_arc.start_angle, t1, sizeof(t1));
            }
            if (node->data.geom_arc.end_angle) {
                node_to_string(node->data.geom_arc.end_angle, t2, sizeof(t2));
            }
            int n = snprintf(buf, buf_size, "arc(%s, r=%s, %s, %s)", name, r, t1, t2);
            if (n < 0 || (size_t) n >= buf_size) {
                lv_strlcpy(buf, "arc(...)", buf_size);
            }
        }

static void str_default(const FormulaNode *node, char *buf, size_t buf_size) {
            /* 使用 lv_strlcpy 替代不安全的 strncpy */
            lv_strlcpy(buf, "?", buf_size);

}
void node_to_string(const FormulaNode *node, char *buf, size_t buf_size) {
    if (!node || !buf || buf_size == 0)
        return;

    buf[0] = '\0';

    static const NodeToStringFunc s_funcs[] = {
        [NODE_NUMBER] = str_number,
        [NODE_VARIABLE] = str_variable,
        [NODE_IDENTIFIER] = str_identifier,
        [NODE_BINARY_OP_ADD] = str_b_add,
        [NODE_BINARY_OP_SUB] = str_b_sub,
        [NODE_BINARY_OP_MUL] = str_b_mul,
        [NODE_BINARY_OP_DIV] = str_b_div,
        [NODE_BINARY_OP_POW] = str_b_pow,
        [NODE_EQUATION] = str_equation,
        [NODE_GEOM_POINT] = str_g_point,
        [NODE_GEOM_SEGMENT] = str_g_segment,
        [NODE_GEOM_CIRCLE] = str_g_circle,
        [NODE_GEOM_TRIANGLE] = str_g_triangle,
        [NODE_GEOM_POLYGON] = str_g_polygon,
        [NODE_GEOM_REGION] = str_g_region,
        [NODE_GEOM_ARC] = str_g_arc,
    };
    if ((unsigned)node->type < sizeof(s_funcs)/sizeof(s_funcs[0]) && s_funcs[node->type]) {
        s_funcs[node->type](node, buf, buf_size);
    } else {
        str_default(node, buf, buf_size);
    }
}

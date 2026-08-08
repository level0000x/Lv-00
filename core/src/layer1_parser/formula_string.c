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
#include "lv/lv_str_utils.h"
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

/* ---------- 一元运算（统一前缀/后缀模板） ----------
 * 格式设计（与 DSL 渲染后端一致的小写函数名风格）：
 *   NEG  -> "-(x)"，其余 -> "sqrt(x)"、"sin(x)"、"cos(x)"、"tan(x)"、
 *           "abs(x)"、"ln(x)"、"log(x)"。
 * 此前这些类型输出 "?"，本格式为首实现，无字节兼容负担。 */
static void str_unary_fmt(const FormulaNode *node, const char *prefix, const char *suffix, char *buf,
                          size_t buf_size) {
    char operand[FORMULA_EXPR_BUF_SIZE];
    node_to_string(node->data.unary_op.operand, operand, sizeof(operand));
    int n = snprintf(buf, buf_size, "%s%s%s", prefix, operand, suffix);
    if (n < 0 || (size_t) n >= buf_size) {
        lv_strlcpy(buf, "(...)", buf_size);
    }
}

static void str_unary_neg(const FormulaNode *node, char *buf, size_t buf_size) {
    str_unary_fmt(node, "-(", ")", buf, buf_size);
}

static void str_unary_sqrt(const FormulaNode *node, char *buf, size_t buf_size) {
    str_unary_fmt(node, "sqrt(", ")", buf, buf_size);
}

static void str_unary_sin(const FormulaNode *node, char *buf, size_t buf_size) {
    str_unary_fmt(node, "sin(", ")", buf, buf_size);
}

static void str_unary_cos(const FormulaNode *node, char *buf, size_t buf_size) {
    str_unary_fmt(node, "cos(", ")", buf, buf_size);
}

static void str_unary_tan(const FormulaNode *node, char *buf, size_t buf_size) {
    str_unary_fmt(node, "tan(", ")", buf, buf_size);
}

static void str_unary_abs(const FormulaNode *node, char *buf, size_t buf_size) {
    str_unary_fmt(node, "abs(", ")", buf, buf_size);
}

static void str_unary_ln(const FormulaNode *node, char *buf, size_t buf_size) {
    str_unary_fmt(node, "ln(", ")", buf, buf_size);
}

static void str_unary_log(const FormulaNode *node, char *buf, size_t buf_size) {
    str_unary_fmt(node, "log(", ")", buf, buf_size);
}

/* ---------- 坐标列表 ----------
 * 格式："(x, y, z)"（逗号分隔，与渲染后端坐标列表风格一致）。 */
static void str_coord_list(const FormulaNode *node, char *buf, size_t buf_size) {
    size_t pos = 0;
    int n = snprintf(buf, buf_size, "(");
    if (n < 0 || (size_t) n >= buf_size) {
        lv_strlcpy(buf, "(...)", buf_size);
        return;
    }
    pos = (size_t) n;
    for (int i = 0; i < node->data.coord_list.coord_count; i++) {
        char coord[FORMULA_EXPR_BUF_SIZE];
        node_to_string(node->data.coord_list.coords[i], coord, sizeof(coord));
        if (i > 0) {
            if (pos + 2 >= buf_size)
                break;
            buf[pos++] = ',';
            buf[pos++] = ' ';
        }
        size_t len = strlen(coord);
        if (pos + len + 1 > buf_size)
            break;
        memcpy(buf + pos, coord, len);
        pos += len;
    }
    if (pos + 1 < buf_size) {
        buf[pos] = ')';
        buf[pos + 1] = '\0';
    }
}

/* ---------- 几何直线/向量 ----------
 * 格式：参照 str_g_arc 的"名称 + 参与对象"风格：
 *   line(l, p1, p2) / vector(v, start, end)。 */
static void str_g_line(const FormulaNode *node, char *buf, size_t buf_size) {
    const char *name = node->data.geom_line.name ? node->data.geom_line.name : "?";
    char p1[FORMULA_RESULT_BUF_SIZE] = "?", p2[FORMULA_RESULT_BUF_SIZE] = "?";
    if (node->data.geom_line.point1) {
        node_to_string(node->data.geom_line.point1, p1, sizeof(p1));
    }
    if (node->data.geom_line.point2) {
        node_to_string(node->data.geom_line.point2, p2, sizeof(p2));
    }
    int n = snprintf(buf, buf_size, "line(%s, %s, %s)", name, p1, p2);
    if (n < 0 || (size_t) n >= buf_size) {
        lv_strlcpy(buf, "line(...)", buf_size);
    }
}

static void str_g_vector(const FormulaNode *node, char *buf, size_t buf_size) {
    const char *name = node->data.geom_vector.name ? node->data.geom_vector.name : "?";
    char start[FORMULA_RESULT_BUF_SIZE] = "?", end[FORMULA_RESULT_BUF_SIZE] = "?";
    if (node->data.geom_vector.start) {
        node_to_string(node->data.geom_vector.start, start, sizeof(start));
    }
    if (node->data.geom_vector.end) {
        node_to_string(node->data.geom_vector.end, end, sizeof(end));
    }
    int n = snprintf(buf, buf_size, "vector(%s, %s, %s)", name, start, end);
    if (n < 0 || (size_t) n >= buf_size) {
        lv_strlcpy(buf, "vector(...)", buf_size);
    }
}

/* ---------- 几何约束（统一 participants 遍历模板） ----------
 * 格式：小写约束名 + 括号逗号分隔参与者，与 DSL 渲染后端一致：
 *   perpendicular(a, b, c) / parallel(l1, l2) / midpoint(m, a, b) /
 *   bisector(a, b, c) / collinear(a, b, c) / tangent(l, c) /
 *   congruent(s1, s2) / angle(a, b, c)。 */
static void str_constraint(const FormulaNode *node, const char *kind, char *buf, size_t buf_size) {
    size_t pos = 0;
    int n = snprintf(buf, buf_size, "%s(", kind);
    if (n < 0 || (size_t) n >= buf_size) {
        lv_strlcpy(buf, "(...)", buf_size);
        return;
    }
    pos = (size_t) n;
    for (int i = 0; i < node->data.constraint.participant_count; i++) {
        char p[FORMULA_EXPR_BUF_SIZE];
        node_to_string(node->data.constraint.participants[i], p, sizeof(p));
        if (i > 0) {
            if (pos + 2 >= buf_size)
                break;
            buf[pos++] = ',';
            buf[pos++] = ' ';
        }
        size_t len = strlen(p);
        if (pos + len + 1 > buf_size)
            break;
        memcpy(buf + pos, p, len);
        pos += len;
    }
    if (pos + 1 < buf_size) {
        buf[pos] = ')';
        buf[pos + 1] = '\0';
    }
}

static void str_constraint_perpendicular(const FormulaNode *node, char *buf, size_t buf_size) {
    str_constraint(node, "perpendicular", buf, buf_size);
}

static void str_constraint_parallel(const FormulaNode *node, char *buf, size_t buf_size) {
    str_constraint(node, "parallel", buf, buf_size);
}

static void str_constraint_midpoint(const FormulaNode *node, char *buf, size_t buf_size) {
    str_constraint(node, "midpoint", buf, buf_size);
}

static void str_constraint_bisector(const FormulaNode *node, char *buf, size_t buf_size) {
    str_constraint(node, "bisector", buf, buf_size);
}

static void str_constraint_collinear(const FormulaNode *node, char *buf, size_t buf_size) {
    str_constraint(node, "collinear", buf, buf_size);
}

static void str_constraint_tangent(const FormulaNode *node, char *buf, size_t buf_size) {
    str_constraint(node, "tangent", buf, buf_size);
}

static void str_constraint_congruent(const FormulaNode *node, char *buf, size_t buf_size) {
    str_constraint(node, "congruent", buf, buf_size);
}

static void str_constraint_angle(const FormulaNode *node, char *buf, size_t buf_size) {
    str_constraint(node, "angle", buf, buf_size);
}

/* ---------- 复合语句 ----------
 * 格式：语句以 "; " 分隔（与 DSL 渲染后端一致）。 */
static void str_compound(const FormulaNode *node, char *buf, size_t buf_size) {
    size_t pos = 0;
    buf[0] = '\0';
    for (int i = 0; i < node->data.compound.statement_count; i++) {
        char stmt[FORMULA_LARGE_BUF_SIZE];
        node_to_string(node->data.compound.statements[i], stmt, sizeof(stmt));
        if (!lv_str_append_sep(buf, buf_size, &pos, "; ", stmt))
            break;
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
        [NODE_UNARY_OP_NEG] = str_unary_neg,
        [NODE_UNARY_OP_SQRT] = str_unary_sqrt,
        [NODE_UNARY_OP_SIN] = str_unary_sin,
        [NODE_UNARY_OP_COS] = str_unary_cos,
        [NODE_UNARY_OP_TAN] = str_unary_tan,
        [NODE_UNARY_OP_ABS] = str_unary_abs,
        [NODE_UNARY_OP_LN] = str_unary_ln,
        [NODE_UNARY_OP_LOG] = str_unary_log,
        [NODE_EQUATION] = str_equation,
        [NODE_COORDINATE_LIST] = str_coord_list,
        [NODE_GEOM_POINT] = str_g_point,
        [NODE_GEOM_SEGMENT] = str_g_segment,
        [NODE_GEOM_LINE] = str_g_line,
        [NODE_GEOM_CIRCLE] = str_g_circle,
        [NODE_GEOM_TRIANGLE] = str_g_triangle,
        [NODE_GEOM_POLYGON] = str_g_polygon,
        [NODE_GEOM_REGION] = str_g_region,
        [NODE_GEOM_ARC] = str_g_arc,
        [NODE_GEOM_VECTOR] = str_g_vector,
        [NODE_CONSTRAINT_PERPENDICULAR] = str_constraint_perpendicular,
        [NODE_CONSTRAINT_PARALLEL] = str_constraint_parallel,
        [NODE_CONSTRAINT_MIDPOINT] = str_constraint_midpoint,
        [NODE_CONSTRAINT_BISECTOR] = str_constraint_bisector,
        [NODE_CONSTRAINT_COLLINEAR] = str_constraint_collinear,
        [NODE_CONSTRAINT_TANGENT] = str_constraint_tangent,
        [NODE_CONSTRAINT_CONGRUENT] = str_constraint_congruent,
        [NODE_CONSTRAINT_ANGLE] = str_constraint_angle,
        [NODE_COMPOUND] = str_compound,
    };
    if ((unsigned)node->type < sizeof(s_funcs)/sizeof(s_funcs[0]) && s_funcs[node->type]) {
        s_funcs[node->type](node, buf, buf_size);
    } else {
        str_default(node, buf, buf_size);
    }
}

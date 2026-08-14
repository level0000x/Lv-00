/**
 * @file formula_string.c
 * @brief 公式节点字符串渲染实现（从 formula_converter.c 拆分）
 *
 * @details 将 FormulaNode 抽象语法树渲染为可读字符串，
 *          支持数字/变量/二元运算/方程/几何节点。
 */

#include "formula_converter_internal.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_utils.h"

/**
 * 将公式节点渲染为字符串（简化版）
 */
void node_to_string(const FormulaNode *node, char *buf, size_t buf_size);
typedef void (*NodeToStringFunc)(const FormulaNode *node, char *buf, size_t buf_size);

/* 判据 L2：snprintf 定长写入 + 截断防御收敛。
 * 语义：将 fmt/args 格式化为定长缓冲 buf（容量 buf_size）；若截断
 * （n >= buf_size）或出错（n < 0），则用 fallback 兜底串覆盖 buf，
 * 保证 NUL 终止。返回 snprintf 的返回值：截断时 >= buf_size、出错时 <0，
 * 供调用点（str_coord_list / str_constraint 的游标续写）判定是否提前返回。
 * 前置：buf 非空且 buf_size > 0；fallback/fmt 非空。 */
static int str_snprintf_fallback(char *buf, size_t buf_size, const char *fallback, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, buf_size, fmt, ap);
    va_end(ap);
    if (n < 0 || (size_t) n >= buf_size) {
        lv_strlcpy(buf, fallback, buf_size);
    }
    return n;
}

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
            str_snprintf_fallback(buf, buf_size, "(... + ...)", "(%s + %s)", left, right);
        }

static void str_b_sub(const FormulaNode *node, char *buf, size_t buf_size) {
            char left[FORMULA_EXPR_BUF_SIZE], right[FORMULA_EXPR_BUF_SIZE];
            node_to_string(node->data.binary_op.left, left, sizeof(left));
            node_to_string(node->data.binary_op.right, right, sizeof(right));
            str_snprintf_fallback(buf, buf_size, "(... - ...)", "(%s - %s)", left, right);
        }

static void str_b_mul(const FormulaNode *node, char *buf, size_t buf_size) {
            char left[FORMULA_EXPR_BUF_SIZE], right[FORMULA_EXPR_BUF_SIZE];
            node_to_string(node->data.binary_op.left, left, sizeof(left));
            node_to_string(node->data.binary_op.right, right, sizeof(right));
            str_snprintf_fallback(buf, buf_size, "(... * ...)", "(%s * %s)", left, right);
        }

static void str_b_div(const FormulaNode *node, char *buf, size_t buf_size) {
            char left[FORMULA_EXPR_BUF_SIZE], right[FORMULA_EXPR_BUF_SIZE];
            node_to_string(node->data.binary_op.left, left, sizeof(left));
            node_to_string(node->data.binary_op.right, right, sizeof(right));
            str_snprintf_fallback(buf, buf_size, "(... / ...)", "(%s / %s)", left, right);
        }

static void str_b_pow(const FormulaNode *node, char *buf, size_t buf_size) {
            char left[FORMULA_EXPR_BUF_SIZE], right[FORMULA_EXPR_BUF_SIZE];
            node_to_string(node->data.binary_op.left, left, sizeof(left));
            node_to_string(node->data.binary_op.right, right, sizeof(right));
            str_snprintf_fallback(buf, buf_size, "(... ^ ...)", "(%s ^ %s)", left, right);
        }

static void str_equation(const FormulaNode *node, char *buf, size_t buf_size) {
            char left[FORMULA_EXPR_BUF_SIZE], right[FORMULA_EXPR_BUF_SIZE];
            node_to_string(node->data.equation.lhs, left, sizeof(left));
            node_to_string(node->data.equation.rhs, right, sizeof(right));
            str_snprintf_fallback(buf, buf_size, "(... = ...)", "(%s = %s)", left, right);
        }

static void str_g_point(const FormulaNode *node, char *buf, size_t buf_size) {
            const char *name = node->data.geom_point.name ? node->data.geom_point.name : "?";
            str_snprintf_fallback(buf, buf_size, "point(...)", "point(%s)", name);
        }

static void str_g_segment(const FormulaNode *node, char *buf, size_t buf_size) {
            const char *name = node->data.geom_segment.name ? node->data.geom_segment.name : "?";
            str_snprintf_fallback(buf, buf_size, "segment(...)", "segment(%s)", name);
        }

static void str_g_circle(const FormulaNode *node, char *buf, size_t buf_size) {
            const char *name = node->data.geom_circle.name ? node->data.geom_circle.name : "?";
            char r[FORMULA_RESULT_BUF_SIZE] = "?";
            if (node->data.geom_circle.radius) {
                node_to_string(node->data.geom_circle.radius, r, sizeof(r));
            }
            str_snprintf_fallback(buf, buf_size, "circle(...)", "circle(%s, r=%s)", name, r);
        }

static void str_g_triangle(const FormulaNode *node, char *buf, size_t buf_size) {
            const char *name = node->data.geom_triangle.name ? node->data.geom_triangle.name : "?";
            str_snprintf_fallback(buf, buf_size, "triangle(...)", "triangle(%s)", name);
        }

static void str_g_polygon(const FormulaNode *node, char *buf, size_t buf_size) {
            const char *name = node->data.geom_polygon.name ? node->data.geom_polygon.name : "?";
            str_snprintf_fallback(buf, buf_size, "polygon(...)", "polygon(%s, %d vertices)", name,
                                  node->data.geom_polygon.vertex_count);
        }

static void str_g_region(const FormulaNode *node, char *buf, size_t buf_size) {
            const char *name = node->data.geom_region.name ? node->data.geom_region.name : "?";
            str_snprintf_fallback(buf, buf_size, "region(...)", "region(%s, %d segments)", name,
                                  node->data.geom_region.segment_count);
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
            str_snprintf_fallback(buf, buf_size, "arc(...)", "arc(%s, r=%s, %s, %s)", name, r, t1, t2);
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
    str_snprintf_fallback(buf, buf_size, "(...)", "%s%s%s", prefix, operand, suffix);
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
    int n = str_snprintf_fallback(buf, buf_size, "(...)", "(");
    if (n < 0 || (size_t) n >= buf_size)
        return;
    pos = (size_t) n;
    for (int i = 0; i < node->data.coord_list.coord_count; i++) {
        char coord[FORMULA_EXPR_BUF_SIZE];
        node_to_string(node->data.coord_list.coords[i], coord, sizeof(coord));
        /* 统一走 lv_str_append_sep：分隔符 + 元素原子性追加（放不下即截断返回） */
        if (!lv_str_append_sep(buf, buf_size, &pos, ", ", coord))
            break;
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
    str_snprintf_fallback(buf, buf_size, "line(...)", "line(%s, %s, %s)", name, p1, p2);
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
    str_snprintf_fallback(buf, buf_size, "vector(...)", "vector(%s, %s, %s)", name, start, end);
}

/* ---------- 几何约束（统一 participants 遍历模板） ----------
 * 格式：小写约束名 + 括号逗号分隔参与者，与 DSL 渲染后端一致：
 *   perpendicular(a, b, c) / parallel(l1, l2) / midpoint(m, a, b) /
 *   bisector(a, b, c) / collinear(a, b, c) / tangent(l, c) /
 *   congruent(s1, s2) / angle(a, b, c)。 */
static void str_constraint(const FormulaNode *node, const char *kind, char *buf, size_t buf_size) {
    size_t pos = 0;
    int n = str_snprintf_fallback(buf, buf_size, "(...)", "%s(", kind);
    if (n < 0 || (size_t) n >= buf_size)
        return;
    pos = (size_t) n;
    for (int i = 0; i < node->data.constraint.participant_count; i++) {
        char p[FORMULA_EXPR_BUF_SIZE];
        node_to_string(node->data.constraint.participants[i], p, sizeof(p));
        /* 统一走 lv_str_append_sep：分隔符 + 元素原子性追加（放不下即截断返回） */
        if (!lv_str_append_sep(buf, buf_size, &pos, ", ", p))
            break;
    }
    if (pos + 1 < buf_size) {
        buf[pos] = ')';
        buf[pos + 1] = '\0';
    }
}

/* 8 个约束名称包装器由 LV_CONSTRAINT_NAME_X 派生（判据 D 单源） */
#define LV_STR_CONSTRAINT_WRAPPER(ENUM, NAME, IDENT) \
    static void str_constraint_##IDENT(const FormulaNode *node, char *buf, size_t buf_size) { \
        str_constraint(node, NAME, buf, buf_size); \
    }
LV_CONSTRAINT_NAME_X(LV_STR_CONSTRAINT_WRAPPER)
#undef LV_STR_CONSTRAINT_WRAPPER

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
#define LV_STR_CONSTRAINT_DISPATCH(ENUM, NAME, IDENT) [ENUM] = str_constraint_##IDENT,
        LV_CONSTRAINT_NAME_X(LV_STR_CONSTRAINT_DISPATCH)
#undef LV_STR_CONSTRAINT_DISPATCH
        [NODE_COMPOUND] = str_compound,
    };
    if ((unsigned)node->type < sizeof(s_funcs)/sizeof(s_funcs[0]) && s_funcs[node->type]) {
        s_funcs[node->type](node, buf, buf_size);
    } else {
        str_default(node, buf, buf_size);
    }
}

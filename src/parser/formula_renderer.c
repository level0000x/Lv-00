/**
 * @file formula_renderer.c
 * @brief 公式渲染器实现
 *
 * @details 将 AST 渲染为 LaTeX、Python 或 DSL 格式的字符串。
 *          支持自定义精度和格式选项。
 *
 * @author Lv-00 Project
 * @version 3.2.0
 *
 * @dependencies
 *   - formula_renderer.h : 渲染器公共接口定义
 *   - lv00_internal.h    : 内部数据结构和常量
 *   - lv00_utils.h       : 统一内存分配器（lv00_malloc/lv00_free）
 *   - error_codes.h      : 错误码定义
 */

#include "formula_renderer.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error_codes.h"
#include "lv00_internal.h"
#include "lv00_utils.h" /* lv00_malloc / lv00_realloc / lv00_free —— 统一内存分配器 */

/* ============================================================
 * 内部常量和宏
 * ============================================================ */

#define MAX_RENDER_BUFFER 16384     /**< 渲染输出缓冲区大小 */
#define MAX_ERROR_MESSAGE 256       /**< 错误消息缓冲区大小 */
#define POINT_LATEX_BUF_SIZE 256    /**< 点坐标 LaTeX 渲染缓冲区大小 */
#define SEGMENT_LATEX_BUF_SIZE 128  /**< 线段 LaTeX 渲染缓冲区大小 */
#define CIRCLE_LATEX_BUF_SIZE 512   /**< 圆 LaTeX 渲染缓冲区大小 */
#define FRACTION_LATEX_BUF_SIZE 128 /**< 分数 LaTeX 渲染缓冲区大小 */
#define SUB_RENDER_BUF_SIZE 1024    /**< 子表达式渲染临时缓冲区大小 */
#define GEOM_SHORT_BUF_SIZE 64      /**< 几何元素短标签缓冲区大小（如点名"P1"） */
#define GEOM_LONG_BUF_SIZE 256      /**< 几何元素长值缓冲区大小（如坐标字符串） */

/* 希腊字母映射表 */
typedef struct {
    const char *name;
    const char *latex;
} GreekLetterMapping;

static const GreekLetterMapping greek_letters[] = {{"alpha", "\\alpha"},
                                                   {"beta", "\\beta"},
                                                   {"gamma", "\\gamma"},
                                                   {"delta", "\\delta"},
                                                   {"epsilon", "\\epsilon"},
                                                   {"zeta", "\\zeta"},
                                                   {"eta", "\\eta"},
                                                   {"theta", "\\theta"},
                                                   {"iota", "\\iota"},
                                                   {"kappa", "\\kappa"},
                                                   {"lambda", "\\lambda"},
                                                   {"mu", "\\mu"},
                                                   {"nu", "\\nu"},
                                                   {"xi", "\\xi"},
                                                   {"omicron", "\\omicron"},
                                                   {"pi", "\\pi"},
                                                   {"rho", "\\rho"},
                                                   {"sigma", "\\sigma"},
                                                   {"tau", "\\tau"},
                                                   {"upsilon", "\\upsilon"},
                                                   {"phi", "\\phi"},
                                                   {"chi", "\\chi"},
                                                   {"psi", "\\psi"},
                                                   {"omega", "\\omega"},
                                                   {"Alpha", "A"},
                                                   {"Beta", "B"},
                                                   {"Gamma", "\\Gamma"},
                                                   {"Delta", "\\Delta"},
                                                   {"Epsilon", "E"},
                                                   {"Zeta", "Z"},
                                                   {"Eta", "E"},
                                                   {"Theta", "\\Theta"},
                                                   {"Iota", "I"},
                                                   {"Kappa", "K"},
                                                   {"Lambda", "\\Lambda"},
                                                   {"Mu", "M"},
                                                   {"Nu", "N"},
                                                   {"Xi", "\\Xi"},
                                                   {"Omicron", "O"},
                                                   {"Pi", "\\Pi"},
                                                   {"Rho", "P"},
                                                   {"Sigma", "\\Sigma"},
                                                   {"Tau", "T"},
                                                   {"Upsilon", "\\Upsilon"},
                                                   {"Phi", "\\Phi"},
                                                   {"Chi", "X"},
                                                   {"Psi", "\\Psi"},
                                                   {"Omega", "\\Omega"},
                                                   {NULL, NULL}};

/* 三角函数名映射表 */
typedef struct {
    const char *name;
    const char *latex;
} TrigFunctionMapping;

static const TrigFunctionMapping trig_functions[] = {
    {"sin", "\\sin"},       {"cos", "\\cos"},   {"tan", "\\tan"},       {"cot", "\\cot"},
    {"sec", "\\sec"},       {"csc", "\\csc"},   {"arcsin", "\\arcsin"}, {"arccos", "\\arccos"},
    {"arctan", "\\arctan"}, {"sinh", "\\sinh"}, {"cosh", "\\cosh"},     {"tanh", "\\tanh"},
    {"ln", "\\ln"},         {"log", "\\log"},   {"exp", "\\exp"},       {NULL, NULL}};

/* ============================================================
 * 错误处理
 * ============================================================ */

const char *formula_render_get_last_error(void) {
    return lv00_get_last_error_message();
}

/* ============================================================
 * 辅助函数
 * ============================================================ */

const char *formula_latex_greek_name(const char *name) {
    if (!name)
        return name;

    for (int i = 0; greek_letters[i].name != NULL; i++) {
        if (strcmp(name, greek_letters[i].name) == 0) {
            return greek_letters[i].latex;
        }
    }
    return name;
}

static const char *get_trig_latex(const char *name) {
    if (!name)
        return name;

    for (int i = 0; trig_functions[i].name != NULL; i++) {
        if (strcmp(name, trig_functions[i].name) == 0) {
            return trig_functions[i].latex;
        }
    }
    return name;
}

static bool is_greek_letter(const char *name) {
    if (!name)
        return false;

    for (int i = 0; greek_letters[i].name != NULL; i++) {
        if (strcmp(name, greek_letters[i].name) == 0) {
            return true;
        }
    }
    return false;
}

static bool needs_parentheses(const FormulaNode *node, NodeType parent_op, bool is_right) {
    if (!node)
        return false;

    /* 数字和变量不需要括号 */
    if (node->type == NODE_NUMBER || node->type == NODE_VARIABLE || node->type == NODE_IDENTIFIER) {
        return false;
    }

    /* 一元运算不需要括号（除了负号在某些情况下） */
    if (node->type >= NODE_UNARY_OP_NEG && node->type <= NODE_UNARY_OP_LOG) {
        if (parent_op == NODE_BINARY_OP_MUL || parent_op == NODE_BINARY_OP_DIV || parent_op == NODE_BINARY_OP_POW) {
            return true;
        }
        return false;
    }

    /* 几何对象不需要括号 */
    if (node->type >= NODE_GEOM_POINT && node->type <= NODE_GEOM_VECTOR) {
        return false;
    }

    /* 方程需要括号 */
    if (node->type == NODE_EQUATION) {
        return true;
    }

    /* 二元运算符优先级比较 */
    int node_prec = 0;
    int parent_prec = 0;

    switch (node->type) {
        case NODE_BINARY_OP_ADD:
            node_prec = 1;
            break;
        case NODE_BINARY_OP_SUB:
            node_prec = 1;
            break;
        case NODE_BINARY_OP_MUL:
            node_prec = 2;
            break;
        case NODE_BINARY_OP_DIV:
            node_prec = 3;
            break; /* 分数形式 */
        case NODE_BINARY_OP_POW:
            node_prec = 4;
            break;
        default:
            return false;
    }

    switch (parent_op) {
        case NODE_BINARY_OP_ADD:
            parent_prec = 1;
            break;
        case NODE_BINARY_OP_SUB:
            parent_prec = 1;
            break;
        case NODE_BINARY_OP_MUL:
            parent_prec = 2;
            break;
        case NODE_BINARY_OP_DIV:
            parent_prec = 3;
            break;
        case NODE_BINARY_OP_POW:
            parent_prec = 4;
            break;
        default:
            return false;
    }

    /* 子节点优先级更低时需要括号 */
    if (node_prec < parent_prec) {
        return true;
    }

    /* 同优先级时，右侧减法需要括号 */
    if (node_prec == parent_prec && is_right && (parent_op == NODE_BINARY_OP_SUB || parent_op == NODE_BINARY_OP_DIV)) {
        return node->type == NODE_BINARY_OP_ADD || node->type == NODE_BINARY_OP_SUB;
    }

    return false;
}

/* ============================================================
 * LaTeX 渲染
 * ============================================================ */

static int render_latex_internal(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options) {
    if (!node) {
        return -1;
    }

    int written = 0;

    switch (node->type) {
        case NODE_NUMBER:
            if (node->data.number.is_integer) {
                written = snprintf(buffer, size, "%lld", (long long) node->data.number.numerator);
            } else {
                /* 分数渲染为 \frac{a}{b} */
                written = snprintf(buffer, size, "\\frac{%lld}{%llu}", (long long) node->data.number.numerator,
                                   (unsigned long long) node->data.number.denominator);
            }
            break;

        case NODE_VARIABLE:
            if (is_greek_letter(node->data.variable.name)) {
                written = snprintf(buffer, size, "%s", formula_latex_greek_name(node->data.variable.name));
            } else {
                written = snprintf(buffer, size, "%s", node->data.variable.name);
            }
            break;

        case NODE_IDENTIFIER:
            written = snprintf(buffer, size, "%s", node->data.identifier.name);
            break;

        case NODE_BINARY_OP_ADD: {
            char left_buf[SUB_RENDER_BUF_SIZE] = {0};
            char right_buf[SUB_RENDER_BUF_SIZE] = {0};

            render_latex_internal(node->data.binary_op.left, left_buf, sizeof(left_buf), options);
            render_latex_internal(node->data.binary_op.right, right_buf, sizeof(right_buf), options);

            written = snprintf(buffer, size, "%s + %s", left_buf, right_buf);
        } break;

        case NODE_BINARY_OP_SUB: {
            char left_buf[1024] = {0};
            char right_buf[1024] = {0};

            render_latex_internal(node->data.binary_op.left, left_buf, sizeof(left_buf), options);
            render_latex_internal(node->data.binary_op.right, right_buf, sizeof(right_buf), options);

            /* 检查右侧是否需要括号 */
            bool need_paren = needs_parentheses(node->data.binary_op.right, NODE_BINARY_OP_SUB, true);

            if (need_paren) {
                written = snprintf(buffer, size, "%s - \\left(%s\\right)", left_buf, right_buf);
            } else {
                written = snprintf(buffer, size, "%s - %s", left_buf, right_buf);
            }
        } break;

        case NODE_BINARY_OP_MUL: {
            char left_buf[1024] = {0};
            char right_buf[1024] = {0};

            render_latex_internal(node->data.binary_op.left, left_buf, sizeof(left_buf), options);
            render_latex_internal(node->data.binary_op.right, right_buf, sizeof(right_buf), options);

            if (options && options->implicit_multiplication) {
                /* 隐式乘法: ab */
                written = snprintf(buffer, size, "%s %s", left_buf, right_buf);
            } else {
                /* 显式乘法: a \cdot b */
                written = snprintf(buffer, size, "%s \\cdot %s", left_buf, right_buf);
            }
        } break;

        case NODE_BINARY_OP_DIV: {
            char left_buf[1024] = {0};
            char right_buf[1024] = {0};

            render_latex_internal(node->data.binary_op.left, left_buf, sizeof(left_buf), options);
            render_latex_internal(node->data.binary_op.right, right_buf, sizeof(right_buf), options);

            /* 分数形式 */
            written = snprintf(buffer, size, "\\frac{%s}{%s}", left_buf, right_buf);
        } break;

        case NODE_BINARY_OP_POW: {
            char left_buf[1024] = {0};
            char right_buf[1024] = {0};

            render_latex_internal(node->data.binary_op.left, left_buf, sizeof(left_buf), options);
            render_latex_internal(node->data.binary_op.right, right_buf, sizeof(right_buf), options);

            /* 检查底数是否需要括号 */
            bool need_paren = needs_parentheses(node->data.binary_op.left, NODE_BINARY_OP_POW, false);

            if (need_paren) {
                written = snprintf(buffer, size, "\\left(%s\\right)^{%s}", left_buf, right_buf);
            } else {
                written = snprintf(buffer, size, "%s^{%s}", left_buf, right_buf);
            }
        } break;

        case NODE_UNARY_OP_NEG: {
            char operand_buf[SUB_RENDER_BUF_SIZE] = {0};
            render_latex_internal(node->data.unary_op.operand, operand_buf, sizeof(operand_buf), options);

            bool need_paren = needs_parentheses(node->data.unary_op.operand, NODE_UNARY_OP_NEG, false);
            if (need_paren) {
                written = snprintf(buffer, size, "-\\left(%s\\right)", operand_buf);
            } else {
                written = snprintf(buffer, size, "-%s", operand_buf);
            }
        } break;

        case NODE_UNARY_OP_SQRT: {
            char operand_buf[1024] = {0};
            render_latex_internal(node->data.unary_op.operand, operand_buf, sizeof(operand_buf), options);
            written = snprintf(buffer, size, "\\sqrt{%s}", operand_buf);
        } break;

        case NODE_UNARY_OP_SIN:
        case NODE_UNARY_OP_COS:
        case NODE_UNARY_OP_TAN: {
            const char *func_names[] = {[NODE_UNARY_OP_SIN - NODE_UNARY_OP_NEG] = "\\sin",
                                        [NODE_UNARY_OP_COS - NODE_UNARY_OP_NEG] = "\\cos",
                                        [NODE_UNARY_OP_TAN - NODE_UNARY_OP_NEG] = "\\tan"};
            int idx = node->type - NODE_UNARY_OP_NEG;

            char operand_buf[1024] = {0};
            render_latex_internal(node->data.unary_op.operand, operand_buf, sizeof(operand_buf), options);
            written = snprintf(buffer, size, "%s\\left(%s\\right)", func_names[idx], operand_buf);
        } break;

        case NODE_UNARY_OP_ABS: {
            char operand_buf[1024] = {0};
            render_latex_internal(node->data.unary_op.operand, operand_buf, sizeof(operand_buf), options);
            written = snprintf(buffer, size, "\\left|%s\\right|", operand_buf);
        } break;

        case NODE_UNARY_OP_LN: {
            char operand_buf[1024] = {0};
            render_latex_internal(node->data.unary_op.operand, operand_buf, sizeof(operand_buf), options);
            written = snprintf(buffer, size, "\\ln\\left(%s\\right)", operand_buf);
        } break;

        case NODE_UNARY_OP_LOG: {
            char operand_buf[1024] = {0};
            render_latex_internal(node->data.unary_op.operand, operand_buf, sizeof(operand_buf), options);
            written = snprintf(buffer, size, "\\log\\left(%s\\right)", operand_buf);
        } break;

        case NODE_EQUATION: {
            char lhs_buf[SUB_RENDER_BUF_SIZE] = {0};
            char rhs_buf[SUB_RENDER_BUF_SIZE] = {0};

            render_latex_internal(node->data.equation.lhs, lhs_buf, sizeof(lhs_buf), options);
            render_latex_internal(node->data.equation.rhs, rhs_buf, sizeof(rhs_buf), options);

            written = snprintf(buffer, size, "%s = %s", lhs_buf, rhs_buf);
        } break;

        case NODE_GEOM_POINT: {
            char coords_buf[2048] = {0};
            if (node->data.geom_point.coords) {
                render_latex_internal(node->data.geom_point.coords, coords_buf, sizeof(coords_buf), options);
            }

            if (node->data.geom_point.name) {
                written = snprintf(buffer, size, "%s = \\left(%s\\right)", node->data.geom_point.name, coords_buf);
            } else {
                written = snprintf(buffer, size, "\\left(%s\\right)", coords_buf);
            }
        } break;

        case NODE_GEOM_SEGMENT: {
            if (node->data.geom_segment.name) {
                written = snprintf(buffer, size, "\\overline{%s}", node->data.geom_segment.name);
            } else {
                char ep1_buf[64] = {0};
                char ep2_buf[64] = {0};
                if (node->data.geom_segment.endpoint1) {
                    render_latex_internal(node->data.geom_segment.endpoint1, ep1_buf, sizeof(ep1_buf), options);
                }
                if (node->data.geom_segment.endpoint2) {
                    render_latex_internal(node->data.geom_segment.endpoint2, ep2_buf, sizeof(ep2_buf), options);
                }
                written = snprintf(buffer, size, "\\overline{%s%s}", ep1_buf, ep2_buf);
            }
        } break;

        case NODE_GEOM_CIRCLE: {
            char center_buf[64] = {0};
            char radius_buf[256] = {0};

            if (node->data.geom_circle.center) {
                if (node->data.geom_circle.center->type == NODE_IDENTIFIER) {
                    snprintf(center_buf, sizeof(center_buf), "%s", node->data.geom_circle.center->data.identifier.name);
                } else {
                    render_latex_internal(node->data.geom_circle.center, center_buf, sizeof(center_buf), options);
                }
            }

            if (node->data.geom_circle.radius) {
                render_latex_internal(node->data.geom_circle.radius, radius_buf, sizeof(radius_buf), options);
            }

            written = snprintf(buffer, size, "\\text{circle } %s \\text{ with center } %s \\text{ and radius } %s",
                               node->data.geom_circle.name ? node->data.geom_circle.name : "O", center_buf, radius_buf);
        } break;

        case NODE_GEOM_TRIANGLE: {
            if (node->data.geom_triangle.name) {
                written = snprintf(buffer, size, "\\triangle %s", node->data.geom_triangle.name);
            } else {
                written = snprintf(buffer, size, "\\triangle");
            }
        } break;

        case NODE_COORDINATE_LIST: {
            char *ptr = buffer;
            size_t remaining = size;
            int total = 0;

            for (int i = 0; i < node->data.coord_list.coord_count; i++) {
                char coord_buf[256] = {0};
                render_latex_internal(node->data.coord_list.coords[i], coord_buf, sizeof(coord_buf), options);

                int w = snprintf(ptr, remaining, "%s%s", (i > 0) ? ", " : "", coord_buf);
                if (w < 0 || (size_t) w >= remaining)
                    break;
                ptr += w;
                remaining -= w;
                total += w;
            }
            written = total;
        } break;

        case NODE_CONSTRAINT_PERPENDICULAR: {
            char p1_buf[64] = {0}, p2_buf[64] = {0}, p3_buf[64] = {0};
            if (node->data.constraint.participant_count >= 3) {
                render_latex_internal(node->data.constraint.participants[0], p1_buf, sizeof(p1_buf), options);
                render_latex_internal(node->data.constraint.participants[1], p2_buf, sizeof(p2_buf), options);
                render_latex_internal(node->data.constraint.participants[2], p3_buf, sizeof(p3_buf), options);
                written = snprintf(buffer, size, "%s \\perp %s%s", p1_buf, p2_buf, p3_buf);
            }
        } break;

        case NODE_CONSTRAINT_PARALLEL: {
            char l1_buf[64] = {0}, l2_buf[64] = {0};
            if (node->data.constraint.participant_count >= 2) {
                render_latex_internal(node->data.constraint.participants[0], l1_buf, sizeof(l1_buf), options);
                render_latex_internal(node->data.constraint.participants[1], l2_buf, sizeof(l2_buf), options);
                written = snprintf(buffer, size, "%s \\parallel %s", l1_buf, l2_buf);
            }
        } break;

        case NODE_CONSTRAINT_MIDPOINT: {
            char m_buf[64] = {0}, a_buf[64] = {0}, b_buf[64] = {0};
            if (node->data.constraint.participant_count >= 3) {
                render_latex_internal(node->data.constraint.participants[0], m_buf, sizeof(m_buf), options);
                render_latex_internal(node->data.constraint.participants[1], a_buf, sizeof(a_buf), options);
                render_latex_internal(node->data.constraint.participants[2], b_buf, sizeof(b_buf), options);
                written = snprintf(buffer, size, "%s = \\text{midpoint}(%s, %s)", m_buf, a_buf, b_buf);
            }
        } break;

        /* 新增：NODE_GEOM_REGION 区域渲染 */
        case NODE_GEOM_REGION: {
            const char *name = node->data.geom_region.name ? node->data.geom_region.name : "R";
            written = snprintf(buffer, size, "\\text{region } %s", name);
        } break;

        /* 新增：NODE_GEOM_ARC 弧渲染 */
        case NODE_GEOM_ARC: {
            char center_buf[64] = {0}, radius_buf[256] = {0};
            char start_buf[64] = {0}, end_buf[64] = {0};

            if (node->data.geom_arc.center) {
                render_latex_internal(node->data.geom_arc.center, center_buf, sizeof(center_buf), options);
            }
            if (node->data.geom_arc.radius) {
                render_latex_internal(node->data.geom_arc.radius, radius_buf, sizeof(radius_buf), options);
            }
            if (node->data.geom_arc.start_angle) {
                render_latex_internal(node->data.geom_arc.start_angle, start_buf, sizeof(start_buf), options);
            }
            if (node->data.geom_arc.end_angle) {
                render_latex_internal(node->data.geom_arc.end_angle, end_buf, sizeof(end_buf), options);
            }

            written = snprintf(buffer, size,
                               "\\overset{\\frown}{%s} \\text{ with center } %s, \\text{ radius } %s, \\text{ from } "
                               "%s \\text{ to } %s",
                               node->data.geom_arc.name ? node->data.geom_arc.name : "AB", center_buf, radius_buf,
                               start_buf, end_buf);
        } break;

        /* 新增：NODE_CONSTRAINT_ANGLE 角度约束渲染 */
        case NODE_CONSTRAINT_ANGLE: {
            char p1_buf[64] = {0}, p2_buf[64] = {0}, p3_buf[64] = {0};
            if (node->data.constraint.participant_count >= 3) {
                render_latex_internal(node->data.constraint.participants[0], p1_buf, sizeof(p1_buf), options);
                render_latex_internal(node->data.constraint.participants[1], p2_buf, sizeof(p2_buf), options);
                render_latex_internal(node->data.constraint.participants[2], p3_buf, sizeof(p3_buf), options);
                written = snprintf(buffer, size, "\\angle %s %s %s", p1_buf, p2_buf, p3_buf);
            }
        } break;

        case NODE_COMPOUND: {
            char *ptr = buffer;
            size_t remaining = size;
            int total = 0;

            for (int i = 0; i < node->data.compound.statement_count; i++) {
                char stmt_buf[1024] = {0};
                render_latex_internal(node->data.compound.statements[i], stmt_buf, sizeof(stmt_buf), options);

                int w = snprintf(ptr, remaining, "%s%s\\\\\n", stmt_buf,
                                 (i < node->data.compound.statement_count - 1) ? "" : "");
                if (w < 0 || (size_t) w >= remaining)
                    break;
                ptr += w;
                remaining -= w;
                total += w;
            }
            written = total;
        } break;

        default:
            written = snprintf(buffer, size, "\\text{<unknown>}");
            break;
    }

    return written;
}

/* ============================================================
 * Python 渲染
 * ============================================================ */

static int render_python_internal(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options) {
    if (!node) {
        return -1;
    }

    int written = 0;

    switch (node->type) {
        case NODE_NUMBER:
            if (node->data.number.is_integer) {
                written = snprintf(buffer, size, "%lld", (long long) node->data.number.numerator);
            } else {
                if (options && options->fraction_mode) {
                    written = snprintf(buffer, size, "Fraction(%lld, %llu)", (long long) node->data.number.numerator,
                                       (unsigned long long) node->data.number.denominator);
                } else {
                    double val = (double) node->data.number.numerator / (double) node->data.number.denominator;
                    written = snprintf(buffer, size, "%.*f", options ? options->precision : 6, val);
                }
            }
            break;

        case NODE_VARIABLE:
            written = snprintf(buffer, size, "%s", node->data.variable.name);
            break;

        case NODE_IDENTIFIER:
            written = snprintf(buffer, size, "%s", node->data.identifier.name);
            break;

        case NODE_BINARY_OP_ADD: {
            char left_buf[1024] = {0};
            char right_buf[1024] = {0};

            render_python_internal(node->data.binary_op.left, left_buf, sizeof(left_buf), options);
            render_python_internal(node->data.binary_op.right, right_buf, sizeof(right_buf), options);

            written = snprintf(buffer, size, "(%s + %s)", left_buf, right_buf);
        } break;

        case NODE_BINARY_OP_SUB: {
            char left_buf[1024] = {0};
            char right_buf[1024] = {0};

            render_python_internal(node->data.binary_op.left, left_buf, sizeof(left_buf), options);
            render_python_internal(node->data.binary_op.right, right_buf, sizeof(right_buf), options);

            written = snprintf(buffer, size, "(%s - %s)", left_buf, right_buf);
        } break;

        case NODE_BINARY_OP_MUL: {
            char left_buf[1024] = {0};
            char right_buf[1024] = {0};

            render_python_internal(node->data.binary_op.left, left_buf, sizeof(left_buf), options);
            render_python_internal(node->data.binary_op.right, right_buf, sizeof(right_buf), options);

            written = snprintf(buffer, size, "(%s * %s)", left_buf, right_buf);
        } break;

        case NODE_BINARY_OP_DIV: {
            char left_buf[1024] = {0};
            char right_buf[1024] = {0};

            render_python_internal(node->data.binary_op.left, left_buf, sizeof(left_buf), options);
            render_python_internal(node->data.binary_op.right, right_buf, sizeof(right_buf), options);

            written = snprintf(buffer, size, "(%s / %s)", left_buf, right_buf);
        } break;

        case NODE_BINARY_OP_POW: {
            char left_buf[1024] = {0};
            char right_buf[1024] = {0};

            render_python_internal(node->data.binary_op.left, left_buf, sizeof(left_buf), options);
            render_python_internal(node->data.binary_op.right, right_buf, sizeof(right_buf), options);

            written = snprintf(buffer, size, "(%s ** %s)", left_buf, right_buf);
        } break;

        case NODE_UNARY_OP_NEG: {
            char operand_buf[1024] = {0};
            render_python_internal(node->data.unary_op.operand, operand_buf, sizeof(operand_buf), options);
            written = snprintf(buffer, size, "(-%s)", operand_buf);
        } break;

        case NODE_UNARY_OP_SQRT: {
            char operand_buf[1024] = {0};
            render_python_internal(node->data.unary_op.operand, operand_buf, sizeof(operand_buf), options);
            written = snprintf(buffer, size, "sqrt(%s)", operand_buf);
        } break;

        case NODE_UNARY_OP_SIN:
        case NODE_UNARY_OP_COS:
        case NODE_UNARY_OP_TAN: {
            const char *func_names[] = {[NODE_UNARY_OP_SIN - NODE_UNARY_OP_NEG] = "sin",
                                        [NODE_UNARY_OP_COS - NODE_UNARY_OP_NEG] = "cos",
                                        [NODE_UNARY_OP_TAN - NODE_UNARY_OP_NEG] = "tan"};
            int idx = node->type - NODE_UNARY_OP_NEG;

            char operand_buf[1024] = {0};
            render_python_internal(node->data.unary_op.operand, operand_buf, sizeof(operand_buf), options);
            written = snprintf(buffer, size, "%s(%s)", func_names[idx], operand_buf);
        } break;

        case NODE_UNARY_OP_ABS: {
            char operand_buf[1024] = {0};
            render_python_internal(node->data.unary_op.operand, operand_buf, sizeof(operand_buf), options);
            written = snprintf(buffer, size, "abs(%s)", operand_buf);
        } break;

        case NODE_UNARY_OP_LN: {
            char operand_buf[1024] = {0};
            render_python_internal(node->data.unary_op.operand, operand_buf, sizeof(operand_buf), options);
            written = snprintf(buffer, size, "log(%s)", operand_buf);
        } break;

        case NODE_UNARY_OP_LOG: {
            char operand_buf[1024] = {0};
            render_python_internal(node->data.unary_op.operand, operand_buf, sizeof(operand_buf), options);
            written = snprintf(buffer, size, "log10(%s)", operand_buf);
        } break;

        case NODE_EQUATION: {
            char lhs_buf[1024] = {0};
            char rhs_buf[1024] = {0};

            render_python_internal(node->data.equation.lhs, lhs_buf, sizeof(lhs_buf), options);
            render_python_internal(node->data.equation.rhs, rhs_buf, sizeof(rhs_buf), options);

            /* 方程转换为比较表达式 */
            written = snprintf(buffer, size, "(%s == %s)", lhs_buf, rhs_buf);
        } break;

        case NODE_GEOM_POINT: {
            char coords_buf[2048] = {0};
            if (node->data.geom_point.coords) {
                render_python_internal(node->data.geom_point.coords, coords_buf, sizeof(coords_buf), options);
            }

            if (node->data.geom_point.name) {
                written = snprintf(buffer, size, "%s = Point(%s)", node->data.geom_point.name, coords_buf);
            } else {
                written = snprintf(buffer, size, "Point(%s)", coords_buf);
            }
        } break;

        case NODE_GEOM_SEGMENT: {
            char ep1_buf[64] = {0};
            char ep2_buf[64] = {0};

            if (node->data.geom_segment.endpoint1) {
                render_python_internal(node->data.geom_segment.endpoint1, ep1_buf, sizeof(ep1_buf), options);
            }
            if (node->data.geom_segment.endpoint2) {
                render_python_internal(node->data.geom_segment.endpoint2, ep2_buf, sizeof(ep2_buf), options);
            }

            written = snprintf(buffer, size, "Segment(%s, %s)", ep1_buf, ep2_buf);
        } break;

        case NODE_GEOM_CIRCLE: {
            char center_buf[64] = {0};
            char radius_buf[256] = {0};

            if (node->data.geom_circle.center) {
                render_python_internal(node->data.geom_circle.center, center_buf, sizeof(center_buf), options);
            }
            if (node->data.geom_circle.radius) {
                render_python_internal(node->data.geom_circle.radius, radius_buf, sizeof(radius_buf), options);
            }

            written = snprintf(buffer, size, "Circle(%s, %s)", center_buf, radius_buf);
        } break;

        case NODE_GEOM_TRIANGLE: {
            char v1_buf[64] = {0}, v2_buf[64] = {0}, v3_buf[64] = {0};

            if (node->data.geom_triangle.vertex1) {
                render_python_internal(node->data.geom_triangle.vertex1, v1_buf, sizeof(v1_buf), options);
            }
            if (node->data.geom_triangle.vertex2) {
                render_python_internal(node->data.geom_triangle.vertex2, v2_buf, sizeof(v2_buf), options);
            }
            if (node->data.geom_triangle.vertex3) {
                render_python_internal(node->data.geom_triangle.vertex3, v3_buf, sizeof(v3_buf), options);
            }

            written = snprintf(buffer, size, "Triangle(%s, %s, %s)", v1_buf, v2_buf, v3_buf);
        } break;

        case NODE_COORDINATE_LIST: {
            char *ptr = buffer;
            size_t remaining = size;
            int total = 0;

            for (int i = 0; i < node->data.coord_list.coord_count; i++) {
                char coord_buf[256] = {0};
                render_python_internal(node->data.coord_list.coords[i], coord_buf, sizeof(coord_buf), options);

                int w = snprintf(ptr, remaining, "%s%s", (i > 0) ? ", " : "", coord_buf);
                if (w < 0 || (size_t) w >= remaining)
                    break;
                ptr += w;
                remaining -= w;
                total += w;
            }
            written = total;
        } break;

        case NODE_CONSTRAINT_PERPENDICULAR: {
            char p1_buf[64] = {0}, p2_buf[64] = {0}, p3_buf[64] = {0};
            if (node->data.constraint.participant_count >= 3) {
                render_python_internal(node->data.constraint.participants[0], p1_buf, sizeof(p1_buf), options);
                render_python_internal(node->data.constraint.participants[1], p2_buf, sizeof(p2_buf), options);
                render_python_internal(node->data.constraint.participants[2], p3_buf, sizeof(p3_buf), options);
                written = snprintf(buffer, size, "perpendicular(%s, %s, %s)", p1_buf, p2_buf, p3_buf);
            }
        } break;

        case NODE_CONSTRAINT_PARALLEL: {
            char l1_buf[64] = {0}, l2_buf[64] = {0};
            if (node->data.constraint.participant_count >= 2) {
                render_python_internal(node->data.constraint.participants[0], l1_buf, sizeof(l1_buf), options);
                render_python_internal(node->data.constraint.participants[1], l2_buf, sizeof(l2_buf), options);
                written = snprintf(buffer, size, "parallel(%s, %s)", l1_buf, l2_buf);
            }
        } break;

        case NODE_CONSTRAINT_MIDPOINT: {
            char m_buf[64] = {0}, a_buf[64] = {0}, b_buf[64] = {0};
            if (node->data.constraint.participant_count >= 3) {
                render_python_internal(node->data.constraint.participants[0], m_buf, sizeof(m_buf), options);
                render_python_internal(node->data.constraint.participants[1], a_buf, sizeof(a_buf), options);
                render_python_internal(node->data.constraint.participants[2], b_buf, sizeof(b_buf), options);
                written = snprintf(buffer, size, "%s = midpoint(%s, %s)", m_buf, a_buf, b_buf);
            }
        } break;

        /* 新增：NODE_GEOM_REGION 区域渲染 */
        case NODE_GEOM_REGION: {
            const char *name = node->data.geom_region.name ? node->data.geom_region.name : "R";
            written = snprintf(buffer, size, "Region('%s')", name);
        } break;

        /* 新增：NODE_GEOM_ARC 弧渲染 */
        case NODE_GEOM_ARC: {
            char center_buf[64] = {0}, radius_buf[256] = {0};
            char start_buf[64] = {0}, end_buf[64] = {0};

            if (node->data.geom_arc.center) {
                render_python_internal(node->data.geom_arc.center, center_buf, sizeof(center_buf), options);
            }
            if (node->data.geom_arc.radius) {
                render_python_internal(node->data.geom_arc.radius, radius_buf, sizeof(radius_buf), options);
            }
            if (node->data.geom_arc.start_angle) {
                render_python_internal(node->data.geom_arc.start_angle, start_buf, sizeof(start_buf), options);
            }
            if (node->data.geom_arc.end_angle) {
                render_python_internal(node->data.geom_arc.end_angle, end_buf, sizeof(end_buf), options);
            }

            written = snprintf(buffer, size, "Arc('%s', center=%s, radius=%s, start_angle=%s, end_angle=%s)",
                               node->data.geom_arc.name ? node->data.geom_arc.name : "AB", center_buf, radius_buf,
                               start_buf, end_buf);
        } break;

        /* 新增：NODE_CONSTRAINT_ANGLE 角度约束渲染 */
        case NODE_CONSTRAINT_ANGLE: {
            char p1_buf[64] = {0}, p2_buf[64] = {0}, p3_buf[64] = {0};
            if (node->data.constraint.participant_count >= 3) {
                render_python_internal(node->data.constraint.participants[0], p1_buf, sizeof(p1_buf), options);
                render_python_internal(node->data.constraint.participants[1], p2_buf, sizeof(p2_buf), options);
                render_python_internal(node->data.constraint.participants[2], p3_buf, sizeof(p3_buf), options);
                written = snprintf(buffer, size, "angle(%s, %s, %s)", p1_buf, p2_buf, p3_buf);
            }
        } break;

        case NODE_COMPOUND: {
            char *ptr = buffer;
            size_t remaining = size;
            int total = 0;

            for (int i = 0; i < node->data.compound.statement_count; i++) {
                char stmt_buf[1024] = {0};
                render_python_internal(node->data.compound.statements[i], stmt_buf, sizeof(stmt_buf), options);

                int w = snprintf(ptr, remaining, "%s\n", stmt_buf);
                if (w < 0 || (size_t) w >= remaining)
                    break;
                ptr += w;
                remaining -= w;
                total += w;
            }
            written = total;
        } break;

        default:
            written = snprintf(buffer, size, "# <unknown>");
            break;
    }

    return written;
}

/* ============================================================
 * DSL 渲染
 * ============================================================ */

static int render_dsl_internal(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options) {
    if (!node) {
        return -1;
    }

    int written = 0;

    switch (node->type) {
        case NODE_NUMBER:
            if (node->data.number.is_integer) {
                written = snprintf(buffer, size, "%lld", (long long) node->data.number.numerator);
            } else {
                written = snprintf(buffer, size, "%lld/%llu", (long long) node->data.number.numerator,
                                   (unsigned long long) node->data.number.denominator);
            }
            break;

        case NODE_VARIABLE:
            written = snprintf(buffer, size, "%s", node->data.variable.name);
            break;

        case NODE_IDENTIFIER:
            written = snprintf(buffer, size, "%s", node->data.identifier.name);
            break;

        case NODE_BINARY_OP_ADD: {
            char left_buf[1024] = {0};
            char right_buf[1024] = {0};

            render_dsl_internal(node->data.binary_op.left, left_buf, sizeof(left_buf), options);
            render_dsl_internal(node->data.binary_op.right, right_buf, sizeof(right_buf), options);

            written = snprintf(buffer, size, "%s + %s", left_buf, right_buf);
        } break;

        case NODE_BINARY_OP_SUB: {
            char left_buf[1024] = {0};
            char right_buf[1024] = {0};

            render_dsl_internal(node->data.binary_op.left, left_buf, sizeof(left_buf), options);
            render_dsl_internal(node->data.binary_op.right, right_buf, sizeof(right_buf), options);

            written = snprintf(buffer, size, "%s - %s", left_buf, right_buf);
        } break;

        case NODE_BINARY_OP_MUL: {
            char left_buf[1024] = {0};
            char right_buf[1024] = {0};

            render_dsl_internal(node->data.binary_op.left, left_buf, sizeof(left_buf), options);
            render_dsl_internal(node->data.binary_op.right, right_buf, sizeof(right_buf), options);

            written = snprintf(buffer, size, "%s * %s", left_buf, right_buf);
        } break;

        case NODE_BINARY_OP_DIV: {
            char left_buf[1024] = {0};
            char right_buf[1024] = {0};

            render_dsl_internal(node->data.binary_op.left, left_buf, sizeof(left_buf), options);
            render_dsl_internal(node->data.binary_op.right, right_buf, sizeof(right_buf), options);

            written = snprintf(buffer, size, "%s / %s", left_buf, right_buf);
        } break;

        case NODE_BINARY_OP_POW: {
            char left_buf[1024] = {0};
            char right_buf[1024] = {0};

            render_dsl_internal(node->data.binary_op.left, left_buf, sizeof(left_buf), options);
            render_dsl_internal(node->data.binary_op.right, right_buf, sizeof(right_buf), options);

            written = snprintf(buffer, size, "%s ^ %s", left_buf, right_buf);
        } break;

        case NODE_UNARY_OP_NEG: {
            char operand_buf[1024] = {0};
            render_dsl_internal(node->data.unary_op.operand, operand_buf, sizeof(operand_buf), options);
            written = snprintf(buffer, size, "-%s", operand_buf);
        } break;

        case NODE_UNARY_OP_SQRT: {
            char operand_buf[1024] = {0};
            render_dsl_internal(node->data.unary_op.operand, operand_buf, sizeof(operand_buf), options);
            written = snprintf(buffer, size, "sqrt(%s)", operand_buf);
        } break;

        case NODE_UNARY_OP_SIN:
        case NODE_UNARY_OP_COS:
        case NODE_UNARY_OP_TAN: {
            const char *func_names[] = {[NODE_UNARY_OP_SIN - NODE_UNARY_OP_NEG] = "sin",
                                        [NODE_UNARY_OP_COS - NODE_UNARY_OP_NEG] = "cos",
                                        [NODE_UNARY_OP_TAN - NODE_UNARY_OP_NEG] = "tan"};
            int idx = node->type - NODE_UNARY_OP_NEG;

            char operand_buf[1024] = {0};
            render_dsl_internal(node->data.unary_op.operand, operand_buf, sizeof(operand_buf), options);
            written = snprintf(buffer, size, "%s(%s)", func_names[idx], operand_buf);
        } break;

        case NODE_UNARY_OP_ABS: {
            char operand_buf[1024] = {0};
            render_dsl_internal(node->data.unary_op.operand, operand_buf, sizeof(operand_buf), options);
            written = snprintf(buffer, size, "abs(%s)", operand_buf);
        } break;

        case NODE_EQUATION: {
            char lhs_buf[1024] = {0};
            char rhs_buf[1024] = {0};

            render_dsl_internal(node->data.equation.lhs, lhs_buf, sizeof(lhs_buf), options);
            render_dsl_internal(node->data.equation.rhs, rhs_buf, sizeof(rhs_buf), options);

            written = snprintf(buffer, size, "%s = %s", lhs_buf, rhs_buf);
        } break;

        case NODE_GEOM_POINT: {
            char coords_buf[2048] = {0};
            if (node->data.geom_point.coords) {
                render_dsl_internal(node->data.geom_point.coords, coords_buf, sizeof(coords_buf), options);
            }

            written = snprintf(buffer, size, "point %s(%s)",
                               node->data.geom_point.name ? node->data.geom_point.name : "P", coords_buf);
        } break;

        case NODE_GEOM_SEGMENT: {
            char ep1_buf[64] = {0};
            char ep2_buf[64] = {0};

            if (node->data.geom_segment.endpoint1) {
                render_dsl_internal(node->data.geom_segment.endpoint1, ep1_buf, sizeof(ep1_buf), options);
            }
            if (node->data.geom_segment.endpoint2) {
                render_dsl_internal(node->data.geom_segment.endpoint2, ep2_buf, sizeof(ep2_buf), options);
            }

            written = snprintf(buffer, size, "segment %s(%s, %s)",
                               node->data.geom_segment.name ? node->data.geom_segment.name : "AB", ep1_buf, ep2_buf);
        } break;

        case NODE_GEOM_CIRCLE: {
            char center_buf[64] = {0};
            char radius_buf[256] = {0};

            if (node->data.geom_circle.center) {
                render_dsl_internal(node->data.geom_circle.center, center_buf, sizeof(center_buf), options);
            }
            if (node->data.geom_circle.radius) {
                render_dsl_internal(node->data.geom_circle.radius, radius_buf, sizeof(radius_buf), options);
            }

            written = snprintf(buffer, size, "circle %s(%s, %s)",
                               node->data.geom_circle.name ? node->data.geom_circle.name : "O", center_buf, radius_buf);
        } break;

        case NODE_GEOM_TRIANGLE: {
            char v1_buf[64] = {0}, v2_buf[64] = {0}, v3_buf[64] = {0};

            if (node->data.geom_triangle.vertex1) {
                render_dsl_internal(node->data.geom_triangle.vertex1, v1_buf, sizeof(v1_buf), options);
            }
            if (node->data.geom_triangle.vertex2) {
                render_dsl_internal(node->data.geom_triangle.vertex2, v2_buf, sizeof(v2_buf), options);
            }
            if (node->data.geom_triangle.vertex3) {
                render_dsl_internal(node->data.geom_triangle.vertex3, v3_buf, sizeof(v3_buf), options);
            }

            written =
                snprintf(buffer, size, "triangle %s(%s, %s, %s)",
                         node->data.geom_triangle.name ? node->data.geom_triangle.name : "ABC", v1_buf, v2_buf, v3_buf);
        } break;

        case NODE_COORDINATE_LIST: {
            char *ptr = buffer;
            size_t remaining = size;
            int total = 0;

            for (int i = 0; i < node->data.coord_list.coord_count; i++) {
                char coord_buf[256] = {0};
                render_dsl_internal(node->data.coord_list.coords[i], coord_buf, sizeof(coord_buf), options);

                int w = snprintf(ptr, remaining, "%s%s", (i > 0) ? ", " : "", coord_buf);
                if (w < 0 || (size_t) w >= remaining)
                    break;
                ptr += w;
                remaining -= w;
                total += w;
            }
            written = total;
        } break;

        case NODE_CONSTRAINT_PERPENDICULAR: {
            char p1_buf[64] = {0}, p2_buf[64] = {0}, p3_buf[64] = {0};
            if (node->data.constraint.participant_count >= 3) {
                render_dsl_internal(node->data.constraint.participants[0], p1_buf, sizeof(p1_buf), options);
                render_dsl_internal(node->data.constraint.participants[1], p2_buf, sizeof(p2_buf), options);
                render_dsl_internal(node->data.constraint.participants[2], p3_buf, sizeof(p3_buf), options);
                written = snprintf(buffer, size, "perpendicular(%s, %s, %s)", p1_buf, p2_buf, p3_buf);
            }
        } break;

        case NODE_CONSTRAINT_PARALLEL: {
            char l1_buf[64] = {0}, l2_buf[64] = {0};
            if (node->data.constraint.participant_count >= 2) {
                render_dsl_internal(node->data.constraint.participants[0], l1_buf, sizeof(l1_buf), options);
                render_dsl_internal(node->data.constraint.participants[1], l2_buf, sizeof(l2_buf), options);
                written = snprintf(buffer, size, "parallel(%s, %s)", l1_buf, l2_buf);
            }
        } break;

        case NODE_CONSTRAINT_MIDPOINT: {
            char m_buf[64] = {0}, a_buf[64] = {0}, b_buf[64] = {0};
            if (node->data.constraint.participant_count >= 3) {
                render_dsl_internal(node->data.constraint.participants[0], m_buf, sizeof(m_buf), options);
                render_dsl_internal(node->data.constraint.participants[1], a_buf, sizeof(a_buf), options);
                render_dsl_internal(node->data.constraint.participants[2], b_buf, sizeof(b_buf), options);
                written = snprintf(buffer, size, "midpoint(%s, %s, %s)", m_buf, a_buf, b_buf);
            }
        } break;

        /* 新增：NODE_GEOM_REGION 区域渲染 */
        case NODE_GEOM_REGION: {
            const char *name = node->data.geom_region.name ? node->data.geom_region.name : "R";
            written = snprintf(buffer, size, "region %s", name);
        } break;

        /* 新增：NODE_GEOM_ARC 弧渲染 */
        case NODE_GEOM_ARC: {
            char center_buf[64] = {0}, radius_buf[256] = {0};
            char start_buf[64] = {0}, end_buf[64] = {0};

            if (node->data.geom_arc.center) {
                render_dsl_internal(node->data.geom_arc.center, center_buf, sizeof(center_buf), options);
            }
            if (node->data.geom_arc.radius) {
                render_dsl_internal(node->data.geom_arc.radius, radius_buf, sizeof(radius_buf), options);
            }
            if (node->data.geom_arc.start_angle) {
                render_dsl_internal(node->data.geom_arc.start_angle, start_buf, sizeof(start_buf), options);
            }
            if (node->data.geom_arc.end_angle) {
                render_dsl_internal(node->data.geom_arc.end_angle, end_buf, sizeof(end_buf), options);
            }

            written = snprintf(buffer, size, "arc %s(%s, %s, %s, %s)",
                               node->data.geom_arc.name ? node->data.geom_arc.name : "AB", center_buf, radius_buf,
                               start_buf, end_buf);
        } break;

        /* 新增：NODE_CONSTRAINT_ANGLE 角度约束渲染 */
        case NODE_CONSTRAINT_ANGLE: {
            char p1_buf[64] = {0}, p2_buf[64] = {0}, p3_buf[64] = {0};
            if (node->data.constraint.participant_count >= 3) {
                render_dsl_internal(node->data.constraint.participants[0], p1_buf, sizeof(p1_buf), options);
                render_dsl_internal(node->data.constraint.participants[1], p2_buf, sizeof(p2_buf), options);
                render_dsl_internal(node->data.constraint.participants[2], p3_buf, sizeof(p3_buf), options);
                written = snprintf(buffer, size, "angle(%s, %s, %s)", p1_buf, p2_buf, p3_buf);
            }
        } break;

        case NODE_COMPOUND: {
            char *ptr = buffer;
            size_t remaining = size;
            int total = 0;

            for (int i = 0; i < node->data.compound.statement_count; i++) {
                char stmt_buf[1024] = {0};
                render_dsl_internal(node->data.compound.statements[i], stmt_buf, sizeof(stmt_buf), options);

                int w = snprintf(ptr, remaining, "%s; ", stmt_buf);
                if (w < 0 || (size_t) w >= remaining)
                    break;
                ptr += w;
                remaining -= w;
                total += w;
            }
            written = total;
        } break;

        default:
            written = snprintf(buffer, size, "<unknown>");
            break;
    }

    return written;
}

/* ============================================================
 * 公共 API 实现
 * ============================================================ */

/**
 * @brief 渲染公式 AST 为指定格式的字符串（简化版）
 *
 * 使用默认渲染选项将 AST 渲染为字符串。
 *
 * @param node   AST 根节点
 * @param format 输出格式（LaTeX/Python/DSL）
 * @return 新分配的字符串指针，失败返回 NULL
 */
char *formula_render(const FormulaNode *node, OutputFormat format) {
    RenderOptions options = RENDER_OPTIONS_DEFAULT;
    return formula_render_ex(node, format, &options);
}

/**
 * @brief 渲染公式 AST 为指定格式的字符串（扩展版）
 *
 * 使用自定义渲染选项将 AST 渲染为字符串。
 *
 * @param node    AST 根节点
 * @param format  输出格式（LaTeX/Python/DSL）
 * @param options 渲染选项指针
 * @return 新分配的字符串指针，失败返回 NULL
 */
char *formula_render_ex(const FormulaNode *node, OutputFormat format, const RenderOptions *options) {
    if (!node) {
        lv00_set_error(LV00_ERROR_INTERNAL, "NULL node");
        return NULL;
    }

    char *buffer = (char *) lv00_malloc(MAX_RENDER_BUFFER);
    if (!buffer) {
        lv00_set_error(LV00_ERROR_OUT_OF_MEMORY, "Memory allocation failed");
        return NULL;
    }

    int written = 0;

    switch (format) {
        case OUTPUT_LATEX:
            written = render_latex_internal(node, buffer, MAX_RENDER_BUFFER, options);
            break;
        case OUTPUT_PYTHON:
            written = render_python_internal(node, buffer, MAX_RENDER_BUFFER, options);
            break;
        case OUTPUT_DSL:
            written = render_dsl_internal(node, buffer, MAX_RENDER_BUFFER, options);
            break;
        default:
            lv00_set_error(LV00_ERROR_UNSUPPORTED, "Unknown output format");
            lv00_free((void **) &buffer);
            return NULL;
    }

    if (written < 0) {
        lv00_set_error(LV00_ERROR_INTERNAL, "Render failed");
        lv00_free((void **) &buffer);
        return NULL;
    }

    /* 重新分配到实际大小 */
    char *result = (char *) lv00_realloc(buffer, written + 1);
    return result ? result : buffer;
}

/**
 * @brief 渲染公式 AST 到缓冲区（简化版）
 *
 * @param node   AST 根节点
 * @param format 输出格式
 * @param buffer 输出缓冲区
 * @param size   缓冲区大小
 * @return 写入的字节数（不含终止符），失败返回负值
 */
int formula_render_to_buffer(const FormulaNode *node, OutputFormat format, char *buffer, size_t size) {
    RenderOptions options = RENDER_OPTIONS_DEFAULT;
    return formula_render_to_buffer_ex(node, format, &options, buffer, size);
}

/**
 * @brief 渲染公式 AST 到缓冲区（扩展版）
 *
 * @param node    AST 根节点
 * @param format  输出格式
 * @param options 渲染选项指针
 * @param buffer  输出缓冲区
 * @param size    缓冲区大小
 * @return 写入的字节数（不含终止符），失败返回负值
 */
int formula_render_to_buffer_ex(const FormulaNode *node, OutputFormat format, const RenderOptions *options,
                                char *buffer, size_t size) {
    if (!node || !buffer || size == 0) {
        return -1;
    }

    int written = 0;

    switch (format) {
        case OUTPUT_LATEX:
            written = render_latex_internal(node, buffer, size, options);
            break;
        case OUTPUT_PYTHON:
            written = render_python_internal(node, buffer, size, options);
            break;
        case OUTPUT_DSL:
            written = render_dsl_internal(node, buffer, size, options);
            break;
        default:
            return -1;
    }

    return written;
}

/**
 * @brief 渲染公式 AST 为 LaTeX 字符串（便捷函数）
 *
 * @param node AST 根节点
 * @return 新分配的 LaTeX 字符串指针，失败返回 NULL
 */
char *formula_render_latex(const FormulaNode *node) {
    return formula_render(node, OUTPUT_LATEX);
}

/**
 * @brief 渲染公式 AST 为 Python 字符串（便捷函数）
 *
 * @param node AST 根节点
 * @return 新分配的 Python 字符串指针，失败返回 NULL
 */
char *formula_render_python(const FormulaNode *node) {
    return formula_render(node, OUTPUT_PYTHON);
}

/**
 * @brief 渲染公式 AST 为 DSL 字符串（便捷函数）
 *
 * @param node AST 根节点
 * @return 新分配的 DSL 字符串指针，失败返回 NULL
 */
char *formula_render_dsl(const FormulaNode *node) {
    return formula_render(node, OUTPUT_DSL);
}

/**
 * @brief 渲染点坐标为 LaTeX 字符串
 *
 * @param name        点名称
 * @param coords      坐标 AST 节点数组
 * @param coord_count 坐标数量
 * @return 新分配的 LaTeX 字符串指针，失败返回 NULL
 */
char *formula_render_point_latex(const char *name, const FormulaNode **coords, int coord_count) {
    if (!name || !coords || coord_count == 0) {
        return NULL;
    }

    char coords_buf[1024] = {0};
    char *ptr = coords_buf;
    size_t remaining = sizeof(coords_buf);

    for (int i = 0; i < coord_count; i++) {
        char coord_buf[256] = {0};
        formula_render_to_buffer(coords[i], OUTPUT_LATEX, coord_buf, sizeof(coord_buf));

        int w = snprintf(ptr, remaining, "%s%s", (i > 0) ? ", " : "", coord_buf);
        if (w < 0 || (size_t) w >= remaining)
            break;
        ptr += w;
        remaining -= w;
    }

    char *result = (char *) lv00_malloc(POINT_LATEX_BUF_SIZE);
    if (result) {
        snprintf(result, POINT_LATEX_BUF_SIZE, "%s = \\left(%s\\right)", name, coords_buf);
    }
    return result;
}

/**
 * @brief 渲染线段名称为 LaTeX 字符串
 *
 * @param name 线段名称
 * @return 新分配的 LaTeX 字符串指针，失败返回 NULL
 */
char *formula_render_segment_latex(const char *name) {
    if (!name)
        return NULL;

    char *result = (char *) lv00_malloc(SEGMENT_LATEX_BUF_SIZE);
    if (result) {
        snprintf(result, SEGMENT_LATEX_BUF_SIZE, "\\overline{%s}", name);
    }
    return result;
}

/**
 * @brief 渲染圆为 LaTeX 字符串
 *
 * @param name   圆名称
 * @param center 圆心名称
 * @param radius 半径 AST 节点
 * @return 新分配的 LaTeX 字符串指针，失败返回 NULL
 */
char *formula_render_circle_latex(const char *name, const char *center, const FormulaNode *radius) {
    if (!name || !center || !radius)
        return NULL;

    char radius_buf[256] = {0};
    formula_render_to_buffer(radius, OUTPUT_LATEX, radius_buf, sizeof(radius_buf));

    char *result = (char *) lv00_malloc(CIRCLE_LATEX_BUF_SIZE);
    if (result) {
        snprintf(result, CIRCLE_LATEX_BUF_SIZE, "\\text{circle } %s \\text{ with center } %s \\text{ and radius } %s",
                 name, center, radius_buf);
    }
    return result;
}

/**
 * @brief 渲染分数为 LaTeX 字符串
 *
 * @param numerator   分子
 * @param denominator 分母
 * @return 新分配的 LaTeX 字符串指针，失败返回 NULL
 */
char *formula_render_fraction_latex(int64_t numerator, uint64_t denominator) {
    char *result = (char *) lv00_malloc(FRACTION_LATEX_BUF_SIZE);
    if (result) {
        snprintf(result, FRACTION_LATEX_BUF_SIZE, "\\frac{%lld}{%llu}", (long long) numerator,
                 (unsigned long long) denominator);
    }
    return result;
}

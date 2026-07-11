/**
 * @file formula_dsl.c
 * @brief DSL 语法解析器
 *
 * @details 拆分子模块（Lv-00 v3.3.0+）。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lv00/formula_parser.h"
#include "debug.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

FormulaNode *formula_node_copy(const FormulaNode *node) {
    if (!node)
        return NULL;

    FormulaNode *copy = lv00_calloc(1, sizeof(FormulaNode));
    if (!copy)
        return NULL;

    copy->type = node->type;
    copy->line = node->line;
    copy->column = node->column;
    copy->refcount = 1;

    switch (node->type) {
        case NODE_NUMBER:
            copy->data.number = node->data.number;
            break;

        case NODE_VARIABLE:
            copy->data.variable.name = lv00_strdup_safe(node->data.variable.name);
            if (!copy->data.variable.name) {
                lv00_free((void **) &copy);
                return NULL;
            }
            break;

        case NODE_IDENTIFIER:
            copy->data.identifier.name = lv00_strdup_safe(node->data.identifier.name);
            if (!copy->data.identifier.name) {
                lv00_free((void **) &copy);
                return NULL;
            }
            break;

        case NODE_BINARY_OP_ADD:
        case NODE_BINARY_OP_SUB:
        case NODE_BINARY_OP_MUL:
        case NODE_BINARY_OP_DIV:
        case NODE_BINARY_OP_POW:
            copy->data.binary_op.left = formula_node_copy(node->data.binary_op.left);
            if (!copy->data.binary_op.left) { formula_node_destroy(copy); return NULL; }
            copy->data.binary_op.right = formula_node_copy(node->data.binary_op.right);
            if (!copy->data.binary_op.right) { formula_node_destroy(copy); return NULL; }
            break;

        case NODE_UNARY_OP_NEG:
        case NODE_UNARY_OP_SQRT:
        case NODE_UNARY_OP_SIN:
        case NODE_UNARY_OP_COS:
        case NODE_UNARY_OP_TAN:
        case NODE_UNARY_OP_ABS:
        case NODE_UNARY_OP_LN:
        case NODE_UNARY_OP_LOG:
            copy->data.unary_op.operand = formula_node_copy(node->data.unary_op.operand);
            if (!copy->data.unary_op.operand) { formula_node_destroy(copy); return NULL; }
            break;

        case NODE_EQUATION:
            copy->data.equation.lhs = formula_node_copy(node->data.equation.lhs);
            if (!copy->data.equation.lhs) { formula_node_destroy(copy); return NULL; }
            copy->data.equation.rhs = formula_node_copy(node->data.equation.rhs);
            if (!copy->data.equation.rhs) { formula_node_destroy(copy); return NULL; }
            break;

        case NODE_COORDINATE_LIST: {
            copy->data.coord_list.coord_count = node->data.coord_list.coord_count;
            if (node->data.coord_list.coord_count > 0) {
                copy->data.coord_list.coords =
                    lv00_calloc((size_t) node->data.coord_list.coord_count, sizeof(FormulaNode *));
                if (!copy->data.coord_list.coords) {
                    lv00_free((void **) &copy);
                    return NULL;
                }
                for (int i = 0; i < node->data.coord_list.coord_count; i++) {
                    copy->data.coord_list.coords[i] = formula_node_copy(node->data.coord_list.coords[i]);
                    if (!copy->data.coord_list.coords[i]) {
                        for (int j = 0; j < i; j++)
                            formula_node_destroy(copy->data.coord_list.coords[j]);
                        lv00_free((void **) &copy->data.coord_list.coords);
                        lv00_free((void **) &copy);
                        return NULL;
                    }
                }
            }
            break;
        }

        case NODE_GEOM_POINT:
            copy->data.geom_point.name =
                node->data.geom_point.name ? lv00_strdup_safe(node->data.geom_point.name) : NULL;
            copy->data.geom_point.coords = formula_node_copy(node->data.geom_point.coords);
            if (!copy->data.geom_point.coords) {
                lv00_free((void **) &copy->data.geom_point.name);
                lv00_free((void **) &copy);
                return NULL;
            }
            break;

        case NODE_GEOM_SEGMENT:
            copy->data.geom_segment.name =
                node->data.geom_segment.name ? lv00_strdup_safe(node->data.geom_segment.name) : NULL;
            copy->data.geom_segment.endpoint1 = formula_node_copy(node->data.geom_segment.endpoint1);
            if (!copy->data.geom_segment.endpoint1) {
                lv00_free((void **) &copy->data.geom_segment.name);
                lv00_free((void **) &copy);
                return NULL;
            }
            copy->data.geom_segment.endpoint2 = formula_node_copy(node->data.geom_segment.endpoint2);
            if (!copy->data.geom_segment.endpoint2) { formula_node_destroy(copy); return NULL; }
            break;

        case NODE_GEOM_LINE:
            copy->data.geom_line.name = node->data.geom_line.name ? lv00_strdup_safe(node->data.geom_line.name) : NULL;
            copy->data.geom_line.point1 = formula_node_copy(node->data.geom_line.point1);
            if (!copy->data.geom_line.point1) {
                lv00_free((void **) &copy->data.geom_line.name);
                lv00_free((void **) &copy);
                return NULL;
            }
            copy->data.geom_line.point2 = formula_node_copy(node->data.geom_line.point2);
            if (!copy->data.geom_line.point2) { formula_node_destroy(copy); return NULL; }
            copy->data.geom_line.equation = formula_node_copy(node->data.geom_line.equation);
            if (!copy->data.geom_line.equation) { formula_node_destroy(copy); return NULL; }
            break;

        case NODE_GEOM_CIRCLE:
            copy->data.geom_circle.name =
                node->data.geom_circle.name ? lv00_strdup_safe(node->data.geom_circle.name) : NULL;
            copy->data.geom_circle.center = formula_node_copy(node->data.geom_circle.center);
            if (!copy->data.geom_circle.center) {
                lv00_free((void **) &copy->data.geom_circle.name);
                lv00_free((void **) &copy);
                return NULL;
            }
            copy->data.geom_circle.radius = formula_node_copy(node->data.geom_circle.radius);
            if (!copy->data.geom_circle.radius) { formula_node_destroy(copy); return NULL; }
            copy->data.geom_circle.equation = formula_node_copy(node->data.geom_circle.equation);
            if (!copy->data.geom_circle.equation) { formula_node_destroy(copy); return NULL; }
            break;

        case NODE_GEOM_TRIANGLE:
            copy->data.geom_triangle.name =
                node->data.geom_triangle.name ? lv00_strdup_safe(node->data.geom_triangle.name) : NULL;
            copy->data.geom_triangle.vertex1 = formula_node_copy(node->data.geom_triangle.vertex1);
            if (!copy->data.geom_triangle.vertex1) {
                lv00_free((void **) &copy->data.geom_triangle.name);
                lv00_free((void **) &copy);
                return NULL;
            }
            copy->data.geom_triangle.vertex2 = formula_node_copy(node->data.geom_triangle.vertex2);
            if (!copy->data.geom_triangle.vertex2) { formula_node_destroy(copy); return NULL; }
            copy->data.geom_triangle.vertex3 = formula_node_copy(node->data.geom_triangle.vertex3);
            if (!copy->data.geom_triangle.vertex3) { formula_node_destroy(copy); return NULL; }
            break;

        case NODE_GEOM_POLYGON: {
            copy->data.geom_polygon.name =
                node->data.geom_polygon.name ? lv00_strdup_safe(node->data.geom_polygon.name) : NULL;
            copy->data.geom_polygon.vertex_count = node->data.geom_polygon.vertex_count;
            if (node->data.geom_polygon.vertex_count > 0) {
                copy->data.geom_polygon.vertices =
                    lv00_calloc((size_t) node->data.geom_polygon.vertex_count, sizeof(FormulaNode *));
                if (!copy->data.geom_polygon.vertices) {
                    lv00_free((void **) &copy->data.geom_polygon.name);
                    lv00_free((void **) &copy);
                    return NULL;
                }
                for (int i = 0; i < node->data.geom_polygon.vertex_count; i++) {
                    copy->data.geom_polygon.vertices[i] = formula_node_copy(node->data.geom_polygon.vertices[i]);
                    if (!copy->data.geom_polygon.vertices[i]) {
                        for (int j = 0; j < i; j++)
                            formula_node_destroy(copy->data.geom_polygon.vertices[j]);
                        lv00_free((void **) &copy->data.geom_polygon.vertices);
                        lv00_free((void **) &copy->data.geom_polygon.name);
                        lv00_free((void **) &copy);
                        return NULL;
                    }
                }
            }
            break;
        }

        case NODE_GEOM_REGION: {
            copy->data.geom_region.name =
                node->data.geom_region.name ? lv00_strdup_safe(node->data.geom_region.name) : NULL;
            copy->data.geom_region.segment_count = node->data.geom_region.segment_count;
            if (node->data.geom_region.segment_count > 0) {
                copy->data.geom_region.boundary_segments =
                    lv00_calloc((size_t) node->data.geom_region.segment_count, sizeof(FormulaNode *));
                if (!copy->data.geom_region.boundary_segments) {
                    lv00_free((void **) &copy->data.geom_region.name);
                    lv00_free((void **) &copy);
                    return NULL;
                }
                for (int i = 0; i < node->data.geom_region.segment_count; i++) {
                    copy->data.geom_region.boundary_segments[i] =
                        formula_node_copy(node->data.geom_region.boundary_segments[i]);
                    if (!copy->data.geom_region.boundary_segments[i]) {
                        for (int j = 0; j < i; j++)
                            formula_node_destroy(copy->data.geom_region.boundary_segments[j]);
                        lv00_free((void **) &copy->data.geom_region.boundary_segments);
                        lv00_free((void **) &copy->data.geom_region.name);
                        lv00_free((void **) &copy);
                        return NULL;
                    }
                }
            }
            break;
        }

        case NODE_GEOM_ARC:
            copy->data.geom_arc.name = node->data.geom_arc.name ? lv00_strdup_safe(node->data.geom_arc.name) : NULL;
            copy->data.geom_arc.center = formula_node_copy(node->data.geom_arc.center);
            if (!copy->data.geom_arc.center) {
                lv00_free((void **) &copy->data.geom_arc.name);
                lv00_free((void **) &copy);
                return NULL;
            }
            copy->data.geom_arc.radius = formula_node_copy(node->data.geom_arc.radius);
            if (!copy->data.geom_arc.radius) { formula_node_destroy(copy); return NULL; }
            copy->data.geom_arc.start_angle = formula_node_copy(node->data.geom_arc.start_angle);
            if (!copy->data.geom_arc.start_angle) { formula_node_destroy(copy); return NULL; }
            copy->data.geom_arc.end_angle = formula_node_copy(node->data.geom_arc.end_angle);
            if (!copy->data.geom_arc.end_angle) { formula_node_destroy(copy); return NULL; }
            break;

        case NODE_GEOM_VECTOR:
            copy->data.geom_vector.name =
                node->data.geom_vector.name ? lv00_strdup_safe(node->data.geom_vector.name) : NULL;
            copy->data.geom_vector.start = formula_node_copy(node->data.geom_vector.start);
            if (!copy->data.geom_vector.start) {
                lv00_free((void **) &copy->data.geom_vector.name);
                lv00_free((void **) &copy);
                return NULL;
            }
            copy->data.geom_vector.end = formula_node_copy(node->data.geom_vector.end);
            if (!copy->data.geom_vector.end) { formula_node_destroy(copy); return NULL; }
            break;

        case NODE_CONSTRAINT_PERPENDICULAR:
        case NODE_CONSTRAINT_PARALLEL:
        case NODE_CONSTRAINT_MIDPOINT:
        case NODE_CONSTRAINT_BISECTOR:
        case NODE_CONSTRAINT_COLLINEAR:
        case NODE_CONSTRAINT_TANGENT:
        case NODE_CONSTRAINT_CONGRUENT:
        case NODE_CONSTRAINT_ANGLE: {
            copy->data.constraint.participant_count = node->data.constraint.participant_count;
            if (node->data.constraint.participant_count > 0) {
                copy->data.constraint.participants =
                    lv00_calloc((size_t) node->data.constraint.participant_count, sizeof(FormulaNode *));
                if (!copy->data.constraint.participants) {
                    lv00_free((void **) &copy);
                    return NULL;
                }
                for (int i = 0; i < node->data.constraint.participant_count; i++) {
                    copy->data.constraint.participants[i] = formula_node_copy(node->data.constraint.participants[i]);
                    if (!copy->data.constraint.participants[i]) {
                        for (int j = 0; j < i; j++)
                            formula_node_destroy(copy->data.constraint.participants[j]);
                        lv00_free((void **) &copy->data.constraint.participants);
                        lv00_free((void **) &copy);
                        return NULL;
                    }
                }
            }
            break;
        }

        case NODE_COMPOUND: {
            copy->data.compound.statement_count = node->data.compound.statement_count;
            if (node->data.compound.statement_count > 0) {
                copy->data.compound.statements =
                    lv00_calloc((size_t) node->data.compound.statement_count, sizeof(FormulaNode *));
                if (!copy->data.compound.statements) {
                    lv00_free((void **) &copy);
                    return NULL;
                }
                for (int i = 0; i < node->data.compound.statement_count; i++) {
                    copy->data.compound.statements[i] = formula_node_copy(node->data.compound.statements[i]);
                    if (!copy->data.compound.statements[i]) {
                        for (int j = 0; j < i; j++)
                            formula_node_destroy(copy->data.compound.statements[j]);
                        lv00_free((void **) &copy->data.compound.statements);
                        lv00_free((void **) &copy);
                        return NULL;
                    }
                }
            }
            break;
        }

        default:
            /*
             * 前向兼容安全兜底：对新增加的未知节点类型，
             * 通过 memcpy 整体拷贝 data 联合体，确保数据不会静默丢失。
             * 注意：此拷贝为浅拷贝，若 data 中包含指针，后续使用时需注意
             * 所有权语义。新增节点类型时应优先在上方添加专用的深拷贝分支。
             */
            memcpy(&copy->data, &node->data, sizeof(copy->data));
            lv00_set_error(LV00_ERROR_UNSUPPORTED,
                           "formula_node_copy: 未实现的节点类型 %d，已通过 memcpy 兜底拷贝 data",
                           (int) node->type);
            break;
    }

    return copy;
}

/* ============================================================
 * 安全性辅助函数
 * ============================================================ */

/**
 * @brief 追踪AST节点创建并检查安全限制
 *
 * 每次创建AST节点时调用，递增节点计数器并检查是否超过
 * LV00_MAX_AST_NODES 上限。超限时设置错误状态并返回false。
 *
 * @param[in,out] ctx 解析器上下文
 * @param[in]     node 新创建的AST节点
 * @return 新创建的节点（如果超限则释放并返回NULL）
 */
static FormulaNode *track_node(ParserContext *ctx, FormulaNode *node) {
    if (!node) return NULL;
    ctx->node_count++;
    if (ctx->node_count > LV00_MAX_AST_NODES) {
        set_error(ctx, "AST节点数超过安全上限");
        formula_node_destroy(node);
        return NULL;
    }
    return node;
}

/* ============================================================
 * 解析数字
 * ============================================================ */

/**
 * @brief 解析数字字面量
 *
 * 解析输入中的数字，支持整数、浮点数和科学计数法表示。
 * 解析策略：
 * 1. 首先扫描数字的字符表示（整数部分、可选小数部分、可选指数部分）
 * 2. 对于浮点数和科学计数法，使用精确分数算法将数值转换为有理数表示
 * 3. 返回分数形式的 NUMBER 节点，整数则转换为 denominator=1 的分数
 *
 * 浮点数转换算法：
 * - 将浮点数表示为整数/分母的分数形式
 * - 例如 3.14 -> (314/100) -> (157/50)（约分后）
 * - 使用欧几里得算法计算 GCD 进行约分
 *
 * @param ctx 解析器上下文指针
 * @return FormulaNode* 解析出的数字节点，失败返回 NULL
 */
static FormulaNode *parse_number(ParserContext *ctx) {
    size_t start = ctx->pos;
    bool has_dot = false;
    bool has_exponent = false;

    /* 整数部分 */
    while (is_digit(peek(ctx))) {
        consume(ctx);
    }

    /* 小数部分 */
    if (peek(ctx) == '.' && is_digit(peek_next(ctx))) {
        has_dot = true;
        consume(ctx); /* 消费 '.' */
        while (is_digit(peek(ctx))) {
            consume(ctx);
        }
    }

    /* 科学计数法 */
    if (peek(ctx) == 'e' || peek(ctx) == 'E') {
        has_exponent = true;
        consume(ctx);
        if (peek(ctx) == '+' || peek(ctx) == '-') {
            consume(ctx);
        }
        if (!is_digit(peek(ctx))) {
            set_error(ctx, "Expected digit after exponent");
            return NULL;
        }
        while (is_digit(peek(ctx))) {
            consume(ctx);
        }
    }

    /* 提取数字字符串 */
    size_t len = ctx->pos - start;
    char *num_str = lv00_malloc(len + 1);
    if (!num_str) {
        set_error(ctx, "Memory allocation failed");
        return NULL;
    }
    /* 使用 memcpy 进行精确长度复制（已分配 len+1 字节，手动零终止更安全） */
    memcpy(num_str, ctx->input + start, len);
    num_str[len] = '\0';

    /* 转换为数值 */
    double value = strtod(num_str, NULL);
    lv00_free((void **) &num_str);

    /* 创建节点 */
    FormulaNode *node = NULL;

    if (has_dot || has_exponent) {
        /**
         * 浮点数精确转换算法：
         * 1. 将浮点数表示为分数形式（整数部分 + 小数部分/分母）
         * 2. 例如 3.14 -> 整数部分 3, 小数 "14" -> 14/100
         * 3. 合并：(3 * 100 + 14) / 100 = 314/100
         * 4. 使用 GCD 约分以获得最简分数：314/100 -> 157/50
         */
        /* 重新提取小数字符串以获得精确的小数位数 */
        int64_t int_part = 0;
        int64_t frac_part = 0;
        int64_t frac_denom = 1;
        bool negative = false;

        /* 从原始输入重新解析 */
        const char *p = ctx->input + start;
        const char *end = ctx->input + ctx->pos;

        if (*p == '-') {
            negative = true;
            p++;
        } else if (*p == '+') {
            p++;
        }

        /* 步骤1：解析符号后的整数部分 */
        while (p < end && is_digit((unsigned char) *p)) {
            /* 溢出保护：int_part * 10 + digit 不得超过 INT64_MAX */
            if (int_part > (INT64_MAX - (*p - '0')) / 10) {
                set_error(ctx, "整数部分溢出");
                return NULL;
            }
            int_part = int_part * 10 + (*p - '0');
            p++;
        }

        /* 步骤2：解析小数点后数字并计算分母 */
        if (p < end && *p == '.') {
            p++;
            while (p < end && is_digit((unsigned char) *p)) {
                /* 溢出保护：frac_part * 10 + digit 不得超过 INT64_MAX */
                if (frac_part > (INT64_MAX - (*p - '0')) / 10) {
                    set_error(ctx, "小数部分溢出");
                    return NULL;
                }
                frac_part = frac_part * 10 + (*p - '0');
                /* 溢出保护：frac_denom * 10 不得超过 INT64_MAX */
                if (frac_denom > INT64_MAX / 10) {
                    set_error(ctx, "小数分母溢出");
                    return NULL;
                }
                frac_denom *= 10; /* 每位小数使分母乘以10 */
                p++;
            }
        }

        /* 步骤3：处理科学计数法指数 */
        if (p < end && (*p == 'e' || *p == 'E')) {
            p++;
            int exp_sign = 1;
            int exp_val = 0;
            if (p < end && *p == '-') {
                exp_sign = -1;
                p++;
            } else if (p < end && *p == '+') {
                p++;
            }
            while (p < end && is_digit((unsigned char) *p)) {
                exp_val = exp_val * 10 + (*p - '0');
                p++;
            }
            /**
             * 步骤4：将指数应用到分母
             * - 正指数：分母缩小（分子乘以10^exp）
             * - 负指数：分母增大
             */
            if (exp_sign > 0) {
                /* 正指数：分母缩小，即分子乘以 10^exp */
                for (int i = 0; i < exp_val; i++) {
                    frac_denom /= 10;
                    if (frac_denom == 0) {
                        frac_denom = 1;
                        break;
                    }
                }
            } else {
                /* 负指数：分母增大 */
                for (int i = 0; i < -exp_sign * exp_val; i++) {
                    frac_denom *= 10;
                }
            }
        }

        /**
         * 步骤5：合并整数和小数部分
         * value = int_part + frac_part/frac_denom
         *        = (int_part * frac_denom + frac_part) / frac_denom
         */
        /* 溢出检查：int_part * frac_denom 可能超出 int64_t 范围 */
        int64_t numerator;
        if (int_part != 0 && frac_denom != 0) {
            if (int_part > 0) {
                if (frac_denom > INT64_MAX / int_part) {
                    /* 乘法溢出，钳位到 INT64_MAX */
                    numerator = INT64_MAX;
                } else {
                    numerator = int_part * frac_denom + frac_part;
                }
            } else {
                /* int_part < 0 */
                if (frac_denom > INT64_MAX / (-int_part)) {
                    /* 乘法溢出，钳位到 INT64_MIN */
                    numerator = INT64_MIN;
                } else {
                    numerator = int_part * frac_denom + frac_part;
                }
            }
        } else {
            numerator = frac_part;
        }
        int64_t denominator = frac_denom;

        if (negative) {
            numerator = -numerator;
        }

        /**
         * 步骤6：使用欧几里得算法计算 GCD 进行约分
         * GCD(a, b) = GCD(b, a % b)，直到 b = 0
         */
        if (numerator != 0 && denominator > 0) {
            int64_t a = numerator < 0 ? -numerator : numerator;
            int64_t b = denominator;
            while (b != 0) {
                int64_t t = b;
                b = a % b;
                a = t;
            }
            numerator /= a;
            denominator /= a;
        }

        node = formula_create_number(numerator, (uint64_t) denominator);
        if (node) {
            node->line = ctx->line;
            node->column = ctx->column;
            node->data.number.is_integer = (denominator == 1);
        }
    } else {
        /* 纯整数 */
        node = formula_create_number((int64_t) value, 1);
        if (node) {
            node->line = ctx->line;
            node->column = ctx->column;
        }
    }
    return track_node(ctx, node);
}

/* ============================================================
 * 解析标识符
 * ============================================================ */

/**
 * @brief 解析标识符字符串
 *
 * 从当前位置开始解析一个标识符，标识符由字母、数字和下划线组成，
 * 必须以字母或下划线开头。解析完成后指针会移动到标识符之后的位置。
 *
 * 算法步骤：
 * 1. 首先检查当前字符是否为有效的标识符起始字符（is_alpha）
 * 2. 记录起始位置，然后连续消费所有字母数字字符
 * 3. 计算标识符长度并分配内存
 * 4. 使用 memcpy 复制标识符内容并添加 null 终止符
 *
 * @param ctx 解析器上下文指针
 * @return char* 解析出的标识符字符串（需调用者释放），失败返回 NULL
 * @retval NULL 解析失败，错误信息已设置到上下文中
 */
static char *parse_identifier_str(ParserContext *ctx) {
    if (!is_alpha(peek(ctx))) {
        set_error(ctx, "Expected identifier");
        return NULL;
    }

    size_t start = ctx->pos;
    while (is_alnum(peek(ctx))) {
        consume(ctx);
    }

    size_t len = ctx->pos - start;

    /* 安全加固：检查token长度限制 */
    if (len > LV00_MAX_TOKEN_LENGTH) {
        set_error(ctx, "Identifier too long");
        return NULL;
    }

    char *ident = lv00_malloc(len + 1);
    if (!ident) {
        set_error(ctx, "Memory allocation failed");
        return NULL;
    }
    /* 使用 memcpy 进行精确长度复制（已分配 len+1 字节，手动零终止更安全） */
    memcpy(ident, ctx->input + start, len);
    ident[len] = '\0';
    return ident;
}

/**
 * @brief 检查字符串是否为 DSL 关键字
 *
 * 在 DSL_KEYWORDS 表中进行线性查找，判断给定字符串是否为
 * 有效的 DSL 关键字（如 point, segment, circle 等几何元素
 * 或 perpendicular, parallel 等约束关键字）。
 *
 * @param str 要检查的字符串
 * @return true 字符串是 DSL 关键字
 * @return false 字符串不是 DSL 关键字
 */
static bool is_dsl_keyword(const char *str) {
    for (int i = 0; DSL_KEYWORDS[i] != NULL; i++) {
        if (strcmp(str, DSL_KEYWORDS[i]) == 0) {
            return true;
        }
    }
    return false;
}

/* ============================================================
 * 语法检测
 * ============================================================ */

/**
 * @brief 自动检测输入公式的语法类型
 *
 * 通过检查输入字符串中的特征关键字来判断语法类型：
 *   - 包含 LaTeX 命令（如 \\frac, \\sqrt）-> "latex"
 *   - 包含 Python 特征（如 **, ==, sqrt(）-> "python"
 *   - 包含 DSL 关键字（如 point, segment）-> "dsl"
 *   - 无法识别 -> "unknown"
 *
 * @param input 输入公式字符串
 * @return 语法类型字符串（"latex"/"python"/"dsl"/"unknown"）
 */
const char *formula_detect_syntax(const char *input) {
    if (!input || !*input) {
        return "unknown";
    }

    /* 检测 LaTeX 命令 */
    for (int i = 0; LATEX_COMMANDS[i] != NULL; i++) {
        if (strstr(input, LATEX_COMMANDS[i]) != NULL) {
            return "latex";
        }
    }

    /* 检测 DSL 关键字 */
    const char *p = input;
    while (*p) {
        /* 跳过空白 */
        while (*p && isspace((unsigned char) *p))
            p++;
        if (!*p)
            break;

        /* 检查是否为关键字 */
        for (int i = 0; DSL_KEYWORDS[i] != NULL; i++) {
            size_t kwlen = strlen(DSL_KEYWORDS[i]);
            if (strncmp(p, DSL_KEYWORDS[i], kwlen) == 0) {
                /* 确保关键字后是空白或分隔符 */
                char next = p[kwlen];
                if (next == '\0' || isspace((unsigned char) next) || next == '(' || next == '{') {
                    return "dsl";
                }
            }
        }

        /* 移动到下一个单词 */
        while (*p && !isspace((unsigned char) *p))
            p++;
    }

    /* 检测 Python 特征 */
    for (int i = 0; PYTHON_FEATURES[i] != NULL; i++) {
        if (strstr(input, PYTHON_FEATURES[i]) != NULL) {
            return "python";
        }
    }

    /* 默认返回 DSL */
    return "dsl";
}

/* ============================================================
 * DSL 解析器
 * ============================================================ */

/* 前向声明 */
static FormulaNode *parse_dsl_expression(ParserContext *ctx);
static FormulaNode *parse_dsl_term(ParserContext *ctx);
static FormulaNode *parse_dsl_factor(ParserContext *ctx);
static FormulaNode *parse_dsl_atom(ParserContext *ctx);
static FormulaNode *parse_dsl_statement(ParserContext *ctx);

/**
 * @brief 解析 DSL 点定义
 *
 * 解析 DSL 语法中的点定义，格式为：point Name(x, y, ...)
 * 支持任意维度的坐标点。
 *
 * 语法：
 * @code
 * point A(1, 2)        // 2D 点
 * point B(1, 2, 3)     // 3D 点
 * @endcode
 *
 * @param ctx 解析器上下文指针
 * @return FormulaNode* 解析出的几何点节点，失败返回 NULL
 */
static FormulaNode *parse_dsl_point(ParserContext *ctx) {
    skip_whitespace(ctx);

    /* 解析点名称 */
    char *name = parse_identifier_str(ctx);
    if (!name) {
        set_error(ctx, "Expected point name");
        return NULL;
    }

    skip_whitespace(ctx);

    /* 期望 '(' */
    if (!expect_char(ctx, '(')) {
        lv00_free((void **) &name);
        return NULL;
    }

    skip_whitespace(ctx);

    /* 解析坐标列表 */
    FormulaNode *coords[LV00_MAX_COORDINATES] = {NULL};
    int coord_count = 0;

    while (!is_at_end(ctx) && peek(ctx) != ')') {
        skip_whitespace(ctx);

        if (coord_count >= LV00_MAX_COORDINATES) {
            set_error(ctx, "Too many coordinates");
            lv00_free((void **) &name);
            for (int i = 0; i < coord_count; i++)
                formula_node_destroy(coords[i]);
            return NULL;
        }

        coords[coord_count] = parse_dsl_expression(ctx);
        if (!coords[coord_count]) {
            lv00_free((void **) &name);
            for (int i = 0; i < coord_count; i++)
                formula_node_destroy(coords[i]);
            return NULL;
        }
        coord_count++;

        skip_whitespace(ctx);

        if (peek(ctx) == ',') {
            consume(ctx);
        } else if (peek(ctx) != ')') {
            set_error(ctx, "Expected ',' or ')'");
            lv00_free((void **) &name);
            for (int i = 0; i < coord_count; i++)
                formula_node_destroy(coords[i]);
            return NULL;
        }
    }

    /* 期望 ')' */
    if (!expect_char(ctx, ')')) {
        lv00_free((void **) &name);
        for (int i = 0; i < coord_count; i++)
            formula_node_destroy(coords[i]);
        return NULL;
    }

    FormulaNode *coord_list = formula_create_coord_list(coords, coord_count);
    for (int i = 0; i < coord_count; i++)
        formula_node_destroy(coords[i]);

    FormulaNode *node = formula_create_geom_point(name, coord_list);
    lv00_free((void **) &name);
    return node;
}

/**
 * @brief 解析 DSL 线段定义
 *
 * 解析 DSL 语法中的线段定义，格式为：segment Name(P1, P2)
 * 其中 P1 和 P2 是已定义的点标识符。
 *
 * 语法：
 * @code
 * segment AB(A, B)
 * @endcode
 *
 * @param ctx 解析器上下文指针
 * @return FormulaNode* 解析出的几何线段节点，失败返回 NULL
 */
static FormulaNode *parse_dsl_segment(ParserContext *ctx) {
    skip_whitespace(ctx);

    /* 解析线段名称 */
    char *name = parse_identifier_str(ctx);
    if (!name) {
        set_error(ctx, "Expected segment name");
        return NULL;
    }

    skip_whitespace(ctx);

    /* 期望 '(' */
    if (!expect_char(ctx, '(')) {
        lv00_free((void **) &name);
        return NULL;
    }

    skip_whitespace(ctx);

    /* 解析起点 */
    FormulaNode *ep1 = parse_dsl_atom(ctx);
    if (!ep1) {
        lv00_free((void **) &name);
        return NULL;
    }

    skip_whitespace(ctx);

    /* 期望 ',' */
    if (!expect_char(ctx, ',')) {
        lv00_free((void **) &name);
        formula_node_destroy(ep1);
        return NULL;
    }

    skip_whitespace(ctx);

    /* 解析终点 */
    FormulaNode *ep2 = parse_dsl_atom(ctx);
    if (!ep2) {
        lv00_free((void **) &name);
        formula_node_destroy(ep1);
        return NULL;
    }

    skip_whitespace(ctx);

    /* 期望 ')' */
    if (!expect_char(ctx, ')')) {
        lv00_free((void **) &name);
        formula_node_destroy(ep1);
        formula_node_destroy(ep2);
        return NULL;
    }

    FormulaNode *node = formula_create_geom_segment(name, ep1, ep2);
    lv00_free((void **) &name);
    return node;
}

/**
 * @brief 解析 DSL 圆定义
 *
 * 解析 DSL 语法中的圆定义，格式为：circle Name(Center, Radius)
 * 其中 Center 是圆心点标识符，Radius 是半径表达式。
 *
 * 语法：
 * @code
 * circle O(center, 5)
 * circle C(center, r)
 * @endcode
 *
 * @param ctx 解析器上下文指针
 * @return FormulaNode* 解析出的几何圆节点，失败返回 NULL
 */
static FormulaNode *parse_dsl_circle(ParserContext *ctx) {
    skip_whitespace(ctx);

    /* 解析圆名称 */
    char *name = parse_identifier_str(ctx);
    if (!name) {
        set_error(ctx, "Expected circle name");
        return NULL;
    }

    skip_whitespace(ctx);

    /* 期望 '(' */
    if (!expect_char(ctx, '(')) {
        lv00_free((void **) &name);
        return NULL;
    }

    skip_whitespace(ctx);

    /* 解析圆心 */
    FormulaNode *center = parse_dsl_atom(ctx);
    if (!center) {
        lv00_free((void **) &name);
        return NULL;
    }

    skip_whitespace(ctx);

    /* 期望 ',' */
    if (!expect_char(ctx, ',')) {
        lv00_free((void **) &name);
        formula_node_destroy(center);
        return NULL;
    }

    skip_whitespace(ctx);

    /* 解析半径 */
    FormulaNode *radius = parse_dsl_expression(ctx);
    if (!radius) {
        lv00_free((void **) &name);
        formula_node_destroy(center);
        return NULL;
    }

    skip_whitespace(ctx);

    /* 期望 ')' */
    if (!expect_char(ctx, ')')) {
        lv00_free((void **) &name);
        formula_node_destroy(center);
        formula_node_destroy(radius);
        return NULL;
    }

    FormulaNode *node = formula_create_geom_circle(name, center, radius);
    lv00_free((void **) &name);
    return node;
}

/**
 * @brief 解析 DSL 三角形定义
 *
 * 解析 DSL 语法中的三角形定义，格式为：triangle Name(V1, V2, V3)
 * 其中 V1、V2、V3 是三角形的三个顶点标识符。
 *
 * 语法：
 * @code
 * triangle ABC(A, B, C)
 * @endcode
 *
 * @param ctx 解析器上下文指针
 * @return FormulaNode* 解析出的几何三角形节点，失败返回 NULL
 */
static FormulaNode *parse_dsl_triangle(ParserContext *ctx) {
    skip_whitespace(ctx);

    /* 解析三角形名称 */
    char *name = parse_identifier_str(ctx);
    if (!name) {
        set_error(ctx, "Expected triangle name");
        return NULL;
    }

    skip_whitespace(ctx);

    /* 期望 '(' */
    if (!expect_char(ctx, '(')) {
        lv00_free((void **) &name);
        return NULL;
    }

    skip_whitespace(ctx);

    /* 解析三个顶点 */
    FormulaNode *vertices[3] = {NULL, NULL, NULL};
    for (int i = 0; i < 3; i++) {
        vertices[i] = parse_dsl_atom(ctx);
        if (!vertices[i]) {
            lv00_free((void **) &name);
            for (int j = 0; j < i; j++)
                formula_node_destroy(vertices[j]);
            set_error(ctx, "Expected vertex");
            return NULL;
        }

        skip_whitespace(ctx);

        if (i < 2) {
            if (!expect_char(ctx, ',')) {
                lv00_free((void **) &name);
                for (int j = 0; j <= i; j++)
                    formula_node_destroy(vertices[j]);
                return NULL;
            }
            skip_whitespace(ctx);
        }
    }

    /* 期望 ')' */
    if (!expect_char(ctx, ')')) {
        lv00_free((void **) &name);
        for (int i = 0; i < 3; i++)
            formula_node_destroy(vertices[i]);
        return NULL;
    }

    FormulaNode *node = formula_create_geom_triangle(name, vertices[0], vertices[1], vertices[2]);
    lv00_free((void **) &name);
    for (int i = 0; i < 3; i++)
        formula_node_destroy(vertices[i]);
    return node;
}

/**
 * @brief 解析 DSL 弧定义
 *
 * 解析 DSL 语法中的弧定义，格式为：arc Name(Center, Radius, StartAngle, EndAngle)
 * 用于定义一段圆弧，包含起始和结束角度。
 *
 * 语法：
 * @code
 * arc A(center, 5, 0, pi/2)
 * @endcode
 *
 * @param ctx 解析器上下文指针
 * @return FormulaNode* 解析出的几何弧节点，失败返回 NULL
 */
static FormulaNode *parse_dsl_arc(ParserContext *ctx) {
    skip_whitespace(ctx);

    /* 解析弧名称 */
    char *name = parse_identifier_str(ctx);
    if (!name) {
        set_error(ctx, "Expected arc name");
        return NULL;
    }

    skip_whitespace(ctx);

    /* 期望 '(' */
    if (!expect_char(ctx, '(')) {
        lv00_free((void **) &name);
        return NULL;
    }

    skip_whitespace(ctx);

    /* 解析圆心 */
    FormulaNode *center = parse_dsl_atom(ctx);
    if (!center) {
        lv00_free((void **) &name);
        return NULL;
    }

    skip_whitespace(ctx);

    /* 期望 ',' */
    if (!expect_char(ctx, ',')) {
        lv00_free((void **) &name);
        formula_node_destroy(center);
        return NULL;
    }

    skip_whitespace(ctx);

    /* 解析半径 */
    FormulaNode *radius = parse_dsl_expression(ctx);
    if (!radius) {
        lv00_free((void **) &name);
        formula_node_destroy(center);
        return NULL;
    }

    skip_whitespace(ctx);

    /* 期望 ',' */
    if (!expect_char(ctx, ',')) {
        lv00_free((void **) &name);
        formula_node_destroy(center);
        formula_node_destroy(radius);
        return NULL;
    }

    skip_whitespace(ctx);

    /* 解析起始角度 */
    FormulaNode *start_angle = parse_dsl_expression(ctx);
    if (!start_angle) {
        lv00_free((void **) &name);
        formula_node_destroy(center);
        formula_node_destroy(radius);
        return NULL;
    }

    skip_whitespace(ctx);

    /* 期望 ',' */
    if (!expect_char(ctx, ',')) {
        lv00_free((void **) &name);
        formula_node_destroy(center);
        formula_node_destroy(radius);
        formula_node_destroy(start_angle);
        return NULL;
    }

    skip_whitespace(ctx);

    /* 解析结束角度 */
    FormulaNode *end_angle = parse_dsl_expression(ctx);
    if (!end_angle) {
        lv00_free((void **) &name);
        formula_node_destroy(center);
        formula_node_destroy(radius);
        formula_node_destroy(start_angle);
        return NULL;
    }

    skip_whitespace(ctx);

    /* 期望 ')' */
    if (!expect_char(ctx, ')')) {
        lv00_free((void **) &name);
        formula_node_destroy(center);
        formula_node_destroy(radius);
        formula_node_destroy(start_angle);
        formula_node_destroy(end_angle);
        return NULL;
    }

    FormulaNode *node = formula_create_geom_arc(name, center, radius, start_angle, end_angle);
    lv00_free((void **) &name);
    formula_node_destroy(center);
    formula_node_destroy(radius);
    formula_node_destroy(start_angle);
    formula_node_destroy(end_angle);
    return node;
}

/**
 * @brief 解析 DSL 多边形定义
 *
 * 解析 DSL 语法中的多边形定义，格式为：polygon Name([V1, V2, V3, ...])
 * 支持任意顶点数量的多边形。
 *
 * 语法：
 * @code
 * polygon P([A, B, C, D])  // 四边形
 * @endcode
 *
 * @param ctx 解析器上下文指针
 * @return FormulaNode* 解析出的几何多边形节点，失败返回 NULL
 */
static FormulaNode *parse_dsl_polygon(ParserContext *ctx) {
    skip_whitespace(ctx);

    /* 解析多边形名称 */
    char *name = parse_identifier_str(ctx);
    if (!name) {
        set_error(ctx, "Expected polygon name");
        return NULL;
    }

    skip_whitespace(ctx);

    /* 期望 '(' */
    if (!expect_char(ctx, '(')) {
        lv00_free((void **) &name);
        return NULL;
    }

    skip_whitespace(ctx);

    /* 期望 '[' */
    if (!expect_char(ctx, '[')) {
        lv00_free((void **) &name);
        return NULL;
    }

    skip_whitespace(ctx);

    /* 解析顶点列表 */
    FormulaNode *vertices[LV00_MAX_POLYGON_VERTICES] = {NULL};
    int vertex_count = 0;

    while (!is_at_end(ctx) && peek(ctx) != ']') {
        if (vertex_count >= LV00_MAX_POLYGON_VERTICES) {
            set_error(ctx, "Too many vertices in polygon");
            lv00_free((void **) &name);
            for (int i = 0; i < vertex_count; i++)
                formula_node_destroy(vertices[i]);
            return NULL;
        }

        vertices[vertex_count] = parse_dsl_atom(ctx);
        if (!vertices[vertex_count]) {
            lv00_free((void **) &name);
            for (int i = 0; i < vertex_count; i++)
                formula_node_destroy(vertices[i]);
            return NULL;
        }
        vertex_count++;

        skip_whitespace(ctx);

        if (peek(ctx) == ',') {
            consume(ctx);
            skip_whitespace(ctx);
        } else if (peek(ctx) != ']') {
            set_error(ctx, "Expected ',' or ']'");
            lv00_free((void **) &name);
            for (int i = 0; i < vertex_count; i++)
                formula_node_destroy(vertices[i]);
            return NULL;
        }
    }

    /* 期望 ']' */
    if (!expect_char(ctx, ']')) {
        lv00_free((void **) &name);
        for (int i = 0; i < vertex_count; i++)
            formula_node_destroy(vertices[i]);
        return NULL;
    }

    skip_whitespace(ctx);

    /* 期望 ')' */
    if (!expect_char(ctx, ')')) {
        lv00_free((void **) &name);
        for (int i = 0; i < vertex_count; i++)
            formula_node_destroy(vertices[i]);
        return NULL;
    }

    FormulaNode *node = formula_create_geom_polygon(name, vertices, vertex_count);
    lv00_free((void **) &name);
    for (int i = 0; i < vertex_count; i++)
        formula_node_destroy(vertices[i]);
    return node;
}

/**
 * @brief 解析 DSL 区域定义
 *
 * 解析 DSL 语法中的区域定义，格式为：region Name([seg1, seg2, ...])
 * 区域由边界线段列表定义。
 *
 * 语法：
 * @code
 * region R([AB, BC, CD, DA])
 * @endcode
 *
 * @param ctx 解析器上下文指针
 * @return FormulaNode* 解析出的几何区域节点，失败返回 NULL
 */
static FormulaNode *parse_dsl_region(ParserContext *ctx) {
    skip_whitespace(ctx);

    /* 解析区域名称 */
    char *name = parse_identifier_str(ctx);
    if (!name) {
        set_error(ctx, "Expected region name");
        return NULL;
    }

    skip_whitespace(ctx);

    /* 期望 '(' */
    if (!expect_char(ctx, '(')) {
        lv00_free((void **) &name);
        return NULL;
    }

    skip_whitespace(ctx);

    /* 期望 '[' */
    if (!expect_char(ctx, '[')) {
        lv00_free((void **) &name);
        return NULL;
    }

    skip_whitespace(ctx);

    /* 解析边界线段列表 */
    FormulaNode *segments[LV00_MAX_POLYGON_VERTICES] = {NULL};
    int segment_count = 0;

    while (!is_at_end(ctx) && peek(ctx) != ']') {
        if (segment_count >= LV00_MAX_POLYGON_VERTICES) {
            set_error(ctx, "Too many segments in region");
            lv00_free((void **) &name);
            for (int i = 0; i < segment_count; i++)
                formula_node_destroy(segments[i]);
            return NULL;
        }

        segments[segment_count] = parse_dsl_atom(ctx);
        if (!segments[segment_count]) {
            lv00_free((void **) &name);
            for (int i = 0; i < segment_count; i++)
                formula_node_destroy(segments[i]);
            return NULL;
        }
        segment_count++;

        skip_whitespace(ctx);

        if (peek(ctx) == ',') {
            consume(ctx);
            skip_whitespace(ctx);
        } else if (peek(ctx) != ']') {
            set_error(ctx, "Expected ',' or ']'");
            lv00_free((void **) &name);
            for (int i = 0; i < segment_count; i++)
                formula_node_destroy(segments[i]);
            return NULL;
        }
    }

    /* 期望 ']' */
    if (!expect_char(ctx, ']')) {
        lv00_free((void **) &name);
        for (int i = 0; i < segment_count; i++)
            formula_node_destroy(segments[i]);
        return NULL;
    }

    skip_whitespace(ctx);

    /* 期望 ')' */
    if (!expect_char(ctx, ')')) {
        lv00_free((void **) &name);
        for (int i = 0; i < segment_count; i++)
            formula_node_destroy(segments[i]);
        return NULL;
    }

    FormulaNode *node = formula_create_geom_region(name, segments, segment_count);
    lv00_free((void **) &name);
    for (int i = 0; i < segment_count; i++)
        formula_node_destroy(segments[i]);
    return node;
}

/**
 * @brief 根据名称获取几何约束类型
 *
 * 将 DSL 约束名称字符串转换为对应的 AST 节点类型。
 * 支持的约束类型包括：perpendicular（垂直）、parallel（平行）、
 * midpoint（中点）、bisector（角平分线）、collinear（共线）、
 * tangent（相切）、congruent（全等）。
 *
 * @param name 约束名称字符串
 * @return NodeType 对应的节点类型，如果未知则返回 (NodeType)-1
 * @note 返回 (NodeType)-1 时表示未知约束类型，调用方应处理此情况
 */
static NodeType get_constraint_type(const char *name) {
    if (strcmp(name, "perpendicular") == 0)
        return NODE_CONSTRAINT_PERPENDICULAR;
    if (strcmp(name, "parallel") == 0)
        return NODE_CONSTRAINT_PARALLEL;
    if (strcmp(name, "midpoint") == 0)
        return NODE_CONSTRAINT_MIDPOINT;
    if (strcmp(name, "bisector") == 0)
        return NODE_CONSTRAINT_BISECTOR;
    if (strcmp(name, "collinear") == 0)
        return NODE_CONSTRAINT_COLLINEAR;
    if (strcmp(name, "tangent") == 0)
        return NODE_CONSTRAINT_TANGENT;
    if (strcmp(name, "congruent") == 0)
        return NODE_CONSTRAINT_CONGRUENT;
    return (NodeType) -1; /* 未知约束类型，由调用方处理 */
}

/**
 * @brief 解析 DSL 几何约束
 *
 * 解析 DSL 语法中的几何约束定义，格式为：ConstraintName(P1, P2, ...)
 *
 * 支持的约束类型：
 * - perpendicular(AB, CD): AB 垂直于 CD
 * - parallel(AB, CD): AB 平行于 CD
 * - midpoint(M, AB): M 是线段 AB 的中点
 * - bisector(L, A, B, C): L 是角 ABC 的角平分线
 * - collinear(A, B, C): 点 A、B、C 共线
 * - tangent(C, L): 圆 C 与直线 L 相切
 * - congruent(AB, CD): 线段 AB 与 CD 全等
 *
 * 语法：
 * @code
 * perpendicular(AB, CD)
 * midpoint(M, AB)
 * @endcode
 *
 * @param ctx 解析器上下文指针
 * @return FormulaNode* 解析出的约束节点，失败返回 NULL
 */
static FormulaNode *parse_dsl_constraint(ParserContext *ctx) {
    skip_whitespace(ctx);

    /* 解析约束类型 */
    char *constraint_name = parse_identifier_str(ctx);
    if (!constraint_name) {
        set_error(ctx, "Expected constraint type");
        return NULL;
    }

    NodeType constraint_type = get_constraint_type(constraint_name);
    if ((int) constraint_type < 0) {
        char err_buf[LV00_MAX_TEMP_MSG_SIZE];
        snprintf(err_buf, sizeof(err_buf), "未知的约束类型: %s", constraint_name);
        set_error(ctx, err_buf);
        lv00_free((void **) &constraint_name);
        return NULL;
    }
    lv00_free((void **) &constraint_name);

    skip_whitespace(ctx);

    /* 期望 '(' */
    if (!expect_char(ctx, '(')) {
        return NULL;
    }

    skip_whitespace(ctx);

    /* 解析参数列表 */
    FormulaNode *participants[LV00_MAX_PARTICIPANTS] = {NULL};
    int participant_count = 0;

    while (!is_at_end(ctx) && peek(ctx) != ')') {
        skip_whitespace(ctx);

        if (participant_count >= LV00_MAX_PARTICIPANTS) {
            set_error(ctx, "Too many participants");
            for (int i = 0; i < participant_count; i++)
                formula_node_destroy(participants[i]);
            return NULL;
        }

        participants[participant_count] = parse_dsl_atom(ctx);
        if (!participants[participant_count]) {
            for (int i = 0; i < participant_count; i++)
                formula_node_destroy(participants[i]);
            return NULL;
        }
        participant_count++;

        skip_whitespace(ctx);

        if (peek(ctx) == ',') {
            consume(ctx);
        } else if (peek(ctx) != ')') {
            set_error(ctx, "Expected ',' or ')'");
            for (int i = 0; i < participant_count; i++)
                formula_node_destroy(participants[i]);
            return NULL;
        }
    }

    /* 期望 ')' */
    if (!expect_char(ctx, ')')) {
        for (int i = 0; i < participant_count; i++)
            formula_node_destroy(participants[i]);
        return NULL;
    }

    FormulaNode *node = formula_create_constraint(constraint_type, participants, participant_count);
    for (int i = 0; i < participant_count; i++)
        formula_node_destroy(participants[i]);
    return node;
}

/**
 * @brief 解析 DSL 原子表达式
 *
 * 解析 DSL 语法中的最小语法单元（原子），包括：
 * - 数字字面量
 * - 括号表达式
 * - 一元负号/正号
 * - 几何元素定义（point, segment, circle, triangle, arc, polygon, region）
 * - 几何约束（perpendicular, parallel 等）
 * - 函数调用
 * - 变量/标识符引用
 *
 * @param ctx 解析器上下文指针
 * @return FormulaNode* 解析出的原子节点，失败返回 NULL
 */
static FormulaNode *parse_dsl_atom(ParserContext *ctx) {
    skip_whitespace(ctx);

    char c = peek(ctx);

    /* 数字 */
    if (is_digit(c) || (c == '.' && is_digit(peek_next(ctx)))) {
        return parse_number(ctx);
    }

    /* 括号表达式 */
    if (c == '(') {
        consume(ctx);
        FormulaNode *expr = parse_dsl_expression(ctx);
        if (!expr)
            return NULL;
        skip_whitespace(ctx);
        if (!expect_char(ctx, ')')) {
            formula_node_destroy(expr);
            return NULL;
        }
        return expr;
    }

    /* 负号 */
    if (c == '-') {
        consume(ctx);
        FormulaNode *operand = parse_dsl_factor(ctx);
        if (!operand)
            return NULL;
        return track_node(ctx, formula_create_unary_op(NODE_UNARY_OP_NEG, operand));
    }

    /* 正号 */
    if (c == '+') {
        consume(ctx);
        return parse_dsl_factor(ctx);
    }

    /* 标识符或关键字 */
    if (is_alpha(c)) {
        size_t start = ctx->pos;

        /* 读取标识符 */
        char *ident = parse_identifier_str(ctx);
        if (!ident)
            return NULL;

        skip_whitespace(ctx);

        /* 检查是否为关键字 */
        if (strcmp(ident, "point") == 0) {
            lv00_free((void **) &ident);
            return parse_dsl_point(ctx);
        }
        if (strcmp(ident, "segment") == 0) {
            lv00_free((void **) &ident);
            return parse_dsl_segment(ctx);
        }
        if (strcmp(ident, "circle") == 0) {
            lv00_free((void **) &ident);
            return parse_dsl_circle(ctx);
        }
        if (strcmp(ident, "triangle") == 0) {
            lv00_free((void **) &ident);
            return parse_dsl_triangle(ctx);
        }
        if (strcmp(ident, "arc") == 0) {
            lv00_free((void **) &ident);
            return parse_dsl_arc(ctx);
        }
        if (strcmp(ident, "polygon") == 0) {
            lv00_free((void **) &ident);
            return parse_dsl_polygon(ctx);
        }
        if (strcmp(ident, "region") == 0) {
            lv00_free((void **) &ident);
            return parse_dsl_region(ctx);
        }
        if (is_dsl_keyword(ident)) {
            /* 其他约束关键字 */
            ctx->pos = start; /* 回退 */
            lv00_free((void **) &ident);
            return parse_dsl_constraint(ctx);
        }

        /* 函数调用 */
        if (peek(ctx) == '(') {
            consume(ctx);
            skip_whitespace(ctx);

            /* 解析参数 */
            FormulaNode *args[LV00_MAX_ARGUMENTS] = {NULL};
            int arg_count = 0;
            while (!is_at_end(ctx) && peek(ctx) != ')') {
                if (arg_count >= LV00_MAX_ARGUMENTS) {
                    set_error(ctx, "Too many arguments");
                    lv00_free((void **) &ident);
                    for (int i = 0; i < arg_count; i++)
                        formula_node_destroy(args[i]);
                    return NULL;
                }

                args[arg_count] = parse_dsl_expression(ctx);
                if (!args[arg_count]) {
                    lv00_free((void **) &ident);
                    for (int i = 0; i < arg_count; i++)
                        formula_node_destroy(args[i]);
                    return NULL;
                }
                arg_count++;

                skip_whitespace(ctx);

                if (peek(ctx) == ',') {
                    consume(ctx);
                    skip_whitespace(ctx);
                }
            }

            if (!expect_char(ctx, ')')) {
                lv00_free((void **) &ident);
                for (int i = 0; i < arg_count; i++)
                    formula_node_destroy(args[i]);
                return NULL;
            }

            /* 根据函数名创建对应节点 */
            FormulaNode *node = NULL;
            if (strcmp(ident, "sqrt") == 0 && arg_count == 1) {
                node = formula_create_unary_op(NODE_UNARY_OP_SQRT, args[0]);
            } else if (strcmp(ident, "sin") == 0 && arg_count == 1) {
                node = formula_create_unary_op(NODE_UNARY_OP_SIN, args[0]);
            } else if (strcmp(ident, "cos") == 0 && arg_count == 1) {
                node = formula_create_unary_op(NODE_UNARY_OP_COS, args[0]);
            } else if (strcmp(ident, "tan") == 0 && arg_count == 1) {
                node = formula_create_unary_op(NODE_UNARY_OP_TAN, args[0]);
            } else if (strcmp(ident, "abs") == 0 && arg_count == 1) {
                node = formula_create_unary_op(NODE_UNARY_OP_ABS, args[0]);
            } else if (strcmp(ident, "ln") == 0 && arg_count == 1) {
                node = formula_create_unary_op(NODE_UNARY_OP_LN, args[0]);
            } else if (strcmp(ident, "log") == 0 && arg_count == 1) {
                node = formula_create_unary_op(NODE_UNARY_OP_LOG, args[0]);
            } else {
                /* 未知函数，作为标识符返回 */
                node = formula_create_identifier(ident);
            }

            lv00_free((void **) &ident);
            for (int i = 0; i < arg_count; i++)
                formula_node_destroy(args[i]);
            return node;
        }

        /* 普通标识符 */
        FormulaNode *node = formula_create_identifier(ident);
        lv00_free((void **) &ident);
        return node;
    }

    set_error(ctx, "Unexpected character");
    return NULL;
}

/**
 * @brief 解析 DSL 因子（处理幂运算）
 *
 * 在原子表达式基础上处理幂运算（^ 或 **），采用右结合递归解析。
 * 例如：a^b^c 解析为 a^(b^c)。
 *
 * @param ctx 解析器上下文指针
 * @return FormulaNode* 解析出的因子节点，失败返回 NULL
 */
static FormulaNode *parse_dsl_factor(ParserContext *ctx) {
    FormulaNode *left = parse_dsl_atom(ctx);
    if (!left)
        return NULL;

    skip_whitespace(ctx);

    /* 处理幂运算 */
    if (peek(ctx) == '^' || match_string(ctx, "**")) {
        if (match_string(ctx, "**")) {
            consume(ctx);
            consume(ctx);
        } else {
            consume(ctx);
        }
        skip_whitespace(ctx);
        FormulaNode *right = parse_dsl_factor(ctx);
        if (!right) {
            formula_node_destroy(left);
            return NULL;
        }
        return track_node(ctx, formula_create_binary_op(NODE_BINARY_OP_POW, left, right));
    }

    return left;
}

/**
 * @brief 解析 DSL 项（处理乘除）
 *
 * 在因子基础上处理乘法（*）和除法（/）运算，采用左结合循环解析。
 * 注意：** 被识别为幂运算符，在此层停止解析。
 *
 * @param ctx 解析器上下文指针
 * @return FormulaNode* 解析出的项节点，失败返回 NULL
 */
static FormulaNode *parse_dsl_term(ParserContext *ctx) {
    FormulaNode *left = parse_dsl_factor(ctx);
    if (!left)
        return NULL;

    while (true) {
        skip_whitespace(ctx);
        char c = peek(ctx);

        NodeType op_type;
        bool should_continue = false;

        if (c == '*') {
            if (peek_next(ctx) == '*')
                break; /* 幂运算 */
            consume(ctx);
            op_type = NODE_BINARY_OP_MUL;
            should_continue = true;
        } else if (c == '/') {
            consume(ctx);
            op_type = NODE_BINARY_OP_DIV;
            should_continue = true;
        }

        if (!should_continue)
            break;

        skip_whitespace(ctx);
        FormulaNode *right = parse_dsl_factor(ctx);
        if (!right) {
            formula_node_destroy(left);
            return NULL;
        }

        left = track_node(ctx, formula_create_binary_op(op_type, left, right));
        if (!left)
            return NULL;
    }

    return left;
}

/**
 * @brief 解析 DSL 表达式（处理加减）
 *
 * 在项基础上处理加法（+）和减法（-）运算，采用左结合循环解析。
 * 这是 DSL 表达式解析的最高优先级层。
 *
 * @param ctx 解析器上下文指针
 * @return FormulaNode* 解析出的表达式节点，失败返回 NULL
 */
static FormulaNode *parse_dsl_expression(ParserContext *ctx) {
    FormulaNode *left = parse_dsl_term(ctx);
    if (!left)
        return NULL;

    while (true) {
        skip_whitespace(ctx);
        char c = peek(ctx);

        NodeType op_type;
        bool should_continue = false;

        if (c == '+') {
            consume(ctx);
            op_type = NODE_BINARY_OP_ADD;
            should_continue = true;
        } else if (c == '-') {
            consume(ctx);
            op_type = NODE_BINARY_OP_SUB;
            should_continue = true;
        }

        if (!should_continue)
            break;

        skip_whitespace(ctx);
        FormulaNode *right = parse_dsl_term(ctx);
        if (!right) {
            formula_node_destroy(left);
            return NULL;
        }

        left = track_node(ctx, formula_create_binary_op(op_type, left, right));
        if (!left)
            return NULL;
    }

    return left;
}

/**
 * @brief 解析 DSL 语句
 *
 * 解析单个 DSL 语句，即一个表达式或等式（lhs = rhs）。
 * 等式使用单等号 =（双等号 == 不视为等式）。
 *
 * @param ctx 解析器上下文指针
 * @return FormulaNode* 解析出的语句节点，失败或到达末尾返回 NULL
 */
static FormulaNode *parse_dsl_statement(ParserContext *ctx) {
    skip_whitespace(ctx);

    if (is_at_end(ctx))
        return NULL;

    FormulaNode *left = parse_dsl_expression(ctx);
    if (!left)
        return NULL;

    skip_whitespace(ctx);

    /* 检查等式 */
    if (peek(ctx) == '=' && peek_next(ctx) != '=') {
        consume(ctx);
        skip_whitespace(ctx);
        FormulaNode *right = parse_dsl_expression(ctx);
        if (!right) {
            formula_node_destroy(left);
            return NULL;
        }
        return track_node(ctx, formula_create_equation(left, right));
    }

    return left;
}

/**
 * @brief 解析 DSL 复合语句
 *
 * 持续解析 DSL 语句直到输入结束，支持分号和换行作为语句分隔符。
 * 如果只有一个语句则直接返回该节点，多个语句则包装为 COMPOUND 节点。
 *
 * @param ctx 解析器上下文指针
 * @return FormulaNode* 解析出的复合节点或单个语句节点，无语句返回 NULL
 */
static FormulaNode *parse_dsl_compound(ParserContext *ctx) {
    FormulaNode *statements[LV00_MAX_STATEMENTS] = {NULL};
    int statement_count = 0;

    while (!is_at_end(ctx)) {
        skip_whitespace(ctx);
        if (is_at_end(ctx))
            break;

        if (statement_count >= LV00_MAX_STATEMENTS) {
            set_error(ctx, "Too many statements");
            for (int i = 0; i < statement_count; i++)
                formula_node_destroy(statements[i]);
            return NULL;
        }

        FormulaNode *stmt = parse_dsl_statement(ctx);
        if (!stmt) {
            if (ctx->has_error) {
                for (int i = 0; i < statement_count; i++)
                    formula_node_destroy(statements[i]);
                return NULL;
            }
            break;
        }

        statements[statement_count++] = stmt;

        skip_whitespace(ctx);

        /* 语句分隔符 */
        if (peek(ctx) == ';' || peek(ctx) == '\n') {
            consume(ctx);
        }
    }

    if (statement_count == 0) {
        return NULL;
    }

    if (statement_count == 1) {
        return statements[0];
    }

    return formula_create_compound(statements, statement_count);
}

/* ============================================================
 * LaTeX 解析器
 * ============================================================ */

static FormulaNode *parse_latex_expression(ParserContext *ctx);

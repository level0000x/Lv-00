/**
 * @file formula_parser.c
 * @brief 公式解析器实现
 *
 * @details 支持 LaTeX、Python 和 DSL 三种语法的公式解析。
 *          生成抽象语法树（AST），支持错误恢复和位置追踪。
 *
 * @author Lv-00 Project
 * @version 3.0.1
 *
 * @dependencies
 *   - formula_parser.h : 解析器公共接口定义
 *   - lv00_internal.h  : 内部数据结构和常量
 *   - lv00_utils.h     : 统一内存分配器和工具函数
 *   - lv00.h           : 核心类型定义
 */

/* ============================================================
 * 魔法数字常量定义
 * ============================================================ */

#define MAX_COORDINATES       16  /**< 坐标列表最大元素数量 */
#define MAX_VERTICES          32  /**< 顶点列表最大元素数量 */
#define MAX_POLYGON_VERTICES  32  /**< 多边形顶点最大数量 */
#define MAX_STATEMENTS        64  /**< 复合语句最大子语句数量 */
#define MAX_ARGUMENTS         16  /**< 函数参数列表最大元素数量 */
#define MAX_PARTICIPANTS      16  /**< 约束参与者最大数量 */
#define MAX_BUFFER_SIZE       256 /**< 错误消息缓冲区大小 */
#define MAX_TEMP_MSG_SIZE      128 /**< 临时错误消息/诊断缓冲区大小 */

#include "formula_parser.h"
#include "lv00_internal.h"
#include "lv00_utils.h"
#include "lv00.h"
#include "error_codes.h"
#include "stream.h"
#include "stream_context_util.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

/* ============================================================
 * 解析器上下文结构
 * ============================================================ */

LV00_DECLARE_STREAM_CTX(formula_parser)

const char *formula_parser_get_last_error(void) {
    return lv00_get_last_error_message();
}

typedef struct {
    const char *input;              /* 输入字符串 */
    size_t pos;                     /* 当前位置 */
    size_t length;                  /* 输入长度 */
    char error_message[MAX_BUFFER_SIZE]; /* 错误消息缓冲区 */
    bool has_error;                 /* 是否有错误 */
    int line;                       /* 当前行号 */
    int column;                     /* 当前列号 */
} ParserContext;

/* ============================================================
 * DSL 关键字表
 * ============================================================ */

static const char *DSL_KEYWORDS[] = {
    "point", "segment", "circle", "triangle", "line", "region",
    "perpendicular", "parallel", "midpoint", "angle", "distance",
    "area", "perimeter", "tangent", "intersect", "equal", "collinear",
    "bisector", "congruent", "polygon", "vector",
    NULL
};

/* ============================================================
 * LaTeX 命令表
 * ============================================================ */

static const char *LATEX_COMMANDS[] = {
    "\\frac", "\\sqrt", "\\sin", "\\cos", "\\tan", "\\cot",
    "\\pi", "\\theta", "\\alpha", "\\beta", "\\gamma", "\\delta",
    "\\epsilon", "\\lambda", "\\mu", "\\sigma", "\\omega",
    "\\leq", "\\geq", "\\neq", "\\approx", "\\equiv",
    "\\cdot", "\\times", "\\div",
    "\\left", "\\right", "\\langle", "\\rangle",
    "\\begin", "\\end", "\\text", "\\mathrm", "\\ln", "\\log",
    NULL
};

/* ============================================================
 * Python 特征表
 * ============================================================ */

static const char *PYTHON_FEATURES[] = {
    "**", "==", "!=", "<=", ">=", "and ", "or ", "not ",
    "sqrt(", "sin(", "cos(", "tan(", "abs(", "pow(",
    "True", "False", "None", "pi", "e)",
    "import ", "from ", "def ", "if ", "else ", "elif ",
    "for ", "while ", "return ", "lambda ",
    NULL
};

/* ============================================================
 * 希腊字母表
 * ============================================================ */

static const char *GREEK_LETTERS[] = {
    "alpha", "beta", "gamma", "delta", "epsilon", "zeta", "eta",
    "theta", "iota", "kappa", "lambda", "mu", "nu", "xi",
    "omicron", "pi", "rho", "sigma", "tau", "upsilon", "phi",
    "chi", "psi", "omega",
    "Alpha", "Beta", "Gamma", "Delta", "Epsilon", "Zeta", "Eta",
    "Theta", "Iota", "Kappa", "Lambda", "Mu", "Nu", "Xi",
    "Omicron", "Pi", "Rho", "Sigma", "Tau", "Upsilon", "Phi",
    "Chi", "Psi", "Omega",
    NULL
};

/* ============================================================
 * 辅助函数实现
 * ============================================================ */

/**
 * @brief 跳过空白字符和注释
 *
 * 在输入流中跳过所有空白字符（空格、制表符、回车符、换行符）
 * 以及以 # 开头的注释行。处理完成后指针定位到第一个非空白字符处。
 *
 * @param ctx 解析器上下文指针
 */
static void skip_whitespace(ParserContext *ctx) {
    while (ctx->pos < ctx->length) {
        char c = ctx->input[ctx->pos];
        if (c == ' ' || c == '\t' || c == '\r') {
            ctx->pos++;
            ctx->column++;
        } else if (c == '\n') {
            ctx->pos++;
            ctx->line++;
            ctx->column = 1;
        } else if (c == '#') {
            /* 跳过注释直到行尾 */
            while (ctx->pos < ctx->length && ctx->input[ctx->pos] != '\n') {
                ctx->pos++;
            }
        } else {
            break;
        }
    }
}

/**
 * @brief 查看当前字符（不消费）
 *
 * 返回当前位置的字符，但不移动解析位置。
 *
 * @param ctx 解析器上下文指针
 * @return char 当前字符，如果已到达末尾则返回 '\0'
 */
static char peek(ParserContext *ctx) {
    if (ctx->pos >= ctx->length) {
        return '\0';
    }
    return ctx->input[ctx->pos];
}

/**
 * @brief 查看下一个字符（不消费）
 *
 * 返回下一个位置的字符，但不移动解析位置。
 * 用于向前查看一个字符以辅助决策（如判断是否为小数点后的数字）。
 *
 * @param ctx 解析器上下文指针
 * @return char 下一个字符，如果接近末尾则返回 '\0'
 */
static char peek_next(ParserContext *ctx) {
    if (ctx->pos + 1 >= ctx->length) {
        return '\0';
    }
    return ctx->input[ctx->pos + 1];
}

/**
 * @brief 消费当前字符并前进到下一个位置
 *
 * 返回当前位置的字符并将解析位置向前移动一个字符。
 * 同时更新行号和列号追踪，遇到换行符时重置列号并增加行号。
 *
 * @param ctx 解析器上下文指针
 * @return char 被消费的字符，如果已到达末尾则返回 '\0'
 */
static char consume(ParserContext *ctx) {
    if (ctx->pos >= ctx->length) {
        return '\0';
    }
    char c = ctx->input[ctx->pos];
    ctx->pos++;
    if (c == '\n') {
        ctx->line++;
        ctx->column = 1;
    } else {
        ctx->column++;
    }
    return c;
}

/**
 * @brief 检查当前位置是否匹配指定字符串（不消费）
 *
 * 在不移动解析位置的情况下，检查从当前位置开始是否匹配给定字符串。
 * 用于向前查看以决定下一步解析动作。
 *
 * @param ctx 解析器上下文指针
 * @param str 要匹配的字符串
 * @return true 当前位置匹配给定字符串
 * @return false 当前位置不匹配或输入长度不足
 */
static bool match_string(ParserContext *ctx, const char *str) {
    size_t len = strlen(str);
    if (ctx->pos + len > ctx->length) {
        return false;
    }
    return strncmp(ctx->input + ctx->pos, str, len) == 0;
}

/**
 * @brief 匹配并消费字符串
 *
 * 检查当前位置是否匹配给定字符串，如果匹配则消费整个字符串
 * 并将解析位置向前移动。
 *
 * @param ctx 解析器上下文指针
 * @param str 要匹配并消费的字符串
 * @return true 成功匹配并消费字符串
 * @return false 匹配失败，解析位置不变
 */
static bool match_and_consume(ParserContext *ctx, const char *str) {
    if (!match_string(ctx, str)) {
        return false;
    }
    size_t len = strlen(str);
    for (size_t i = 0; i < len; i++) {
        consume(ctx);
    }
    return true;
}

/**
 * @brief 期望并消费指定字符
 *
 * 检查当前字符是否匹配预期字符，如果匹配则消费该字符；
 * 如果不匹配则设置错误信息并标记解析器为错误状态。
 *
 * @param ctx 解析器上下文指针
 * @param c 期望的字符
 * @return true 成功匹配并消费字符
 * @return false 字符不匹配或已到达末尾，错误状态已设置
 */
static bool expect_char(ParserContext *ctx, char c) {
    if (peek(ctx) != c) {
        char msg[MAX_TEMP_MSG_SIZE];
        snprintf(msg, sizeof(msg), "Expected '%c' but got '%s'", c, peek(ctx) ? "unexpected char" : "EOF");
        /* 使用 lv00_strlcpy 替代不安全的 strncpy */
        lv00_strlcpy(ctx->error_message, msg, sizeof(ctx->error_message));
        ctx->has_error = true;
        return false;
    }
    consume(ctx);
    return true;
}

/**
 * @brief 设置解析错误信息
 *
 * 在解析上下文中设置错误消息和错误状态，同时将错误信息
 * 同步到全局错误缓冲区。只有在尚未出错时才会设置新的错误，
 * 以保留第一个错误的信息。
 *
 * @param ctx 解析器上下文指针
 * @param msg 错误消息字符串
 */
static void set_error(ParserContext *ctx, const char *msg) {
    if (!ctx->has_error) {
        snprintf(ctx->error_message, sizeof(ctx->error_message),
                 "Error at line %d, column %d: %s", ctx->line, ctx->column, msg);
        ctx->has_error = true;
        lv00_set_error(LV00_ERROR_PARSE, "%s", ctx->error_message);
    }
}

/**
 * @brief 检查是否到达输入末尾
 *
 * @param ctx 解析器上下文指针
 * @return true 已到达输入末尾
 * @return false 尚未到达输入末尾
 */
static bool is_at_end(ParserContext *ctx) {
    return ctx->pos >= ctx->length;
}

/**
 * @brief 检查字符是否为字母
 *
 * 判断字符是否为英文字母（a-z, A-Z）或下划线（_）。
 *
 * @param c 要检查的字符
 * @return true 字符是字母或下划线
 * @return false 字符不是字母或下划线
 */
static bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

/**
 * @brief 检查字符是否为字母或数字
 *
 * 判断字符是否为英文字母（a-z, A-Z）、数字（0-9）或下划线（_）。
 * 实质上等价于 is_alpha() 与数字检查的组合。
 *
 * @param c 要检查的字符
 * @return true 字符是字母、数字或下划线
 * @return false 字符不是上述字符
 */
static bool is_alnum(char c) {
    return is_alpha(c) || (c >= '0' && c <= '9');
}

/**
 * @brief 检查字符是否为数字
 *
 * 判断字符是否为十进制数字（0-9）。
 *
 * @param c 要检查的字符
 * @return true 字符是数字
 * @return false 字符不是数字
 */
static bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

/* ============================================================
 * AST 节点管理
 * ============================================================ */

/**
 * @brief 创建数值 AST 节点
 *
 * @param numerator   分子
 * @param denominator 分母（不能为 0）
 * @return 新分配的 AST 节点指针，失败返回 NULL
 */
FormulaNode *formula_create_number(int64_t numerator, uint64_t denominator) {
    if (denominator == 0) {
        lv00_set_error(LV00_ERROR_INVALID_PARAM, "formula_create_number: denominator must not be zero");
        return NULL;
    }
    FormulaNode *node = lv00_calloc(1, sizeof(FormulaNode));
    if (!node) return NULL;
    node->type = NODE_NUMBER;
    node->line = 1;
    node->column = 1;
    node->data.number.numerator = numerator;
    node->data.number.denominator = denominator;
    node->data.number.is_integer = (denominator == 1);
    return node;
}

/**
 * @brief 创建变量 AST 节点
 *
 * @param name 变量名称
 * @return 新分配的 AST 节点指针，失败返回 NULL
 */
FormulaNode *formula_create_variable(const char *name) {
    if (!name) return NULL;
    FormulaNode *node = lv00_calloc(1, sizeof(FormulaNode));
    if (!node) return NULL;
    node->type = NODE_VARIABLE;
    node->line = 1;
    node->column = 1;
    node->data.variable.name = lv00_strdup_safe(name);
    if (!node->data.variable.name) {
        lv00_free((void**)&node);
        return NULL;
    }
    return node;
}

/**
 * @brief 创建标识符 AST 节点
 *
 * @param name 标识符名称
 * @return 新分配的 AST 节点指针，失败返回 NULL
 */
FormulaNode *formula_create_identifier(const char *name) {
    if (!name) return NULL;
    FormulaNode *node = lv00_calloc(1, sizeof(FormulaNode));
    if (!node) return NULL;
    node->type = NODE_IDENTIFIER;
    node->line = 1;
    node->column = 1;
    node->data.identifier.name = lv00_strdup_safe(name);
    if (!node->data.identifier.name) {
        lv00_free((void**)&node);
        return NULL;
    }
    return node;
}

/**
 * @brief 创建二元运算 AST 节点
 *
 * @param op_type 运算符类型
 * @param left    左操作数
 * @param right   右操作数
 * @return 新分配的 AST 节点指针，失败返回 NULL
 */
FormulaNode *formula_create_binary_op(NodeType op_type, FormulaNode *left, FormulaNode *right) {
    FormulaNode *node = lv00_calloc(1, sizeof(FormulaNode));
    if (!node) return NULL;
    node->type = op_type;
    node->line = 1;
    node->column = 1;
    node->data.binary_op.left = left;
    node->data.binary_op.right = right;
    return node;
}

/**
 * @brief 创建一元运算 AST 节点
 *
 * @param op_type 运算符类型
 * @param operand 操作数
 * @return 新分配的 AST 节点指针，失败返回 NULL
 */
FormulaNode *formula_create_unary_op(NodeType op_type, FormulaNode *operand) {
    FormulaNode *node = lv00_calloc(1, sizeof(FormulaNode));
    if (!node) return NULL;
    node->type = op_type;
    node->line = 1;
    node->column = 1;
    node->data.unary_op.operand = operand;
    return node;
}

/**
 * @brief 创建等式 AST 节点
 *
 * @param lhs 左侧表达式
 * @param rhs 右侧表达式
 * @return 新分配的 AST 节点指针，失败返回 NULL
 */
FormulaNode *formula_create_equation(FormulaNode *lhs, FormulaNode *rhs) {
    FormulaNode *node = lv00_calloc(1, sizeof(FormulaNode));
    if (!node) return NULL;
    node->type = NODE_EQUATION;
    node->line = 1;
    node->column = 1;
    node->data.equation.lhs = lhs;
    node->data.equation.rhs = rhs;
    return node;
}

FormulaNode *formula_create_coord_list(FormulaNode **coords, int count) {
    FormulaNode *node = lv00_calloc(1, sizeof(FormulaNode));
    if (!node) return NULL;
    node->type = NODE_COORDINATE_LIST;
    node->line = 1;
    node->column = 1;
    if (count > 0 && coords) {
        node->data.coord_list.coords = lv00_malloc(sizeof(FormulaNode*) * count);
        if (!node->data.coord_list.coords) {
            lv00_free((void**)&node);
            return NULL;
        }
        memcpy(node->data.coord_list.coords, coords, sizeof(FormulaNode*) * count);
        node->data.coord_list.coord_count = count;
    }
    return node;
}

FormulaNode *formula_create_geom_point(const char *name, FormulaNode *coords) {
    if (!name) return NULL;
    FormulaNode *node = lv00_calloc(1, sizeof(FormulaNode));
    if (!node) return NULL;
    node->type = NODE_GEOM_POINT;
    node->line = 1;
    node->column = 1;
    node->data.geom_point.name = lv00_strdup_safe(name);
    if (!node->data.geom_point.name) {
        lv00_free((void**)&node);
        return NULL;
    }
    node->data.geom_point.coords = coords;
    return node;
}

FormulaNode *formula_create_geom_segment(const char *name, FormulaNode *ep1, FormulaNode *ep2) {
    if (!name) return NULL;
    FormulaNode *node = lv00_calloc(1, sizeof(FormulaNode));
    if (!node) return NULL;
    node->type = NODE_GEOM_SEGMENT;
    node->line = 1;
    node->column = 1;
    node->data.geom_segment.name = lv00_strdup_safe(name);
    if (!node->data.geom_segment.name) {
        lv00_free((void**)&node);
        return NULL;
    }
    node->data.geom_segment.endpoint1 = ep1;
    node->data.geom_segment.endpoint2 = ep2;
    return node;
}

FormulaNode *formula_create_geom_circle(const char *name, FormulaNode *center, FormulaNode *radius) {
    if (!name) return NULL;
    FormulaNode *node = lv00_calloc(1, sizeof(FormulaNode));
    if (!node) return NULL;
    node->type = NODE_GEOM_CIRCLE;
    node->line = 1;
    node->column = 1;
    node->data.geom_circle.name = lv00_strdup_safe(name);
    if (!node->data.geom_circle.name) {
        lv00_free((void**)&node);
        return NULL;
    }
    node->data.geom_circle.center = center;
    node->data.geom_circle.radius = radius;
    return node;
}

FormulaNode *formula_create_geom_triangle(const char *name, FormulaNode *v1, FormulaNode *v2, FormulaNode *v3) {
    if (!name) return NULL;
    FormulaNode *node = lv00_calloc(1, sizeof(FormulaNode));
    if (!node) return NULL;
    node->type = NODE_GEOM_TRIANGLE;
    node->line = 1;
    node->column = 1;
    node->data.geom_triangle.name = lv00_strdup_safe(name);
    if (!node->data.geom_triangle.name) {
        lv00_free((void**)&node);
        return NULL;
    }
    node->data.geom_triangle.vertex1 = v1;
    node->data.geom_triangle.vertex2 = v2;
    node->data.geom_triangle.vertex3 = v3;
    return node;
}

FormulaNode *formula_create_geom_polygon(const char *name, FormulaNode **vertices, int vertex_count) {
    if (!name) return NULL;
    FormulaNode *node = lv00_calloc(1, sizeof(FormulaNode));
    if (!node) return NULL;
    node->type = NODE_GEOM_POLYGON;
    node->line = 1;
    node->column = 1;
    node->data.geom_polygon.name = lv00_strdup_safe(name);
    if (!node->data.geom_polygon.name) {
        lv00_free((void**)&node);
        return NULL;
    }
    if (vertex_count > 0 && vertices) {
        node->data.geom_polygon.vertices = lv00_malloc(sizeof(FormulaNode*) * vertex_count);
        if (!node->data.geom_polygon.vertices) {
            lv00_free((void**)&node->data.geom_polygon.name);
            lv00_free((void**)&node);
            return NULL;
        }
        memcpy(node->data.geom_polygon.vertices, vertices, sizeof(FormulaNode*) * vertex_count);
        node->data.geom_polygon.vertex_count = vertex_count;
    }
    return node;
}

FormulaNode *formula_create_geom_region(const char *name, FormulaNode **segments, int segment_count) {
    if (!name) return NULL;
    FormulaNode *node = lv00_calloc(1, sizeof(FormulaNode));
    if (!node) return NULL;
    node->type = NODE_GEOM_REGION;
    node->line = 1;
    node->column = 1;
    node->data.geom_region.name = lv00_strdup_safe(name);
    if (!node->data.geom_region.name) {
        lv00_free((void**)&node);
        return NULL;
    }
    if (segment_count > 0 && segments) {
        node->data.geom_region.boundary_segments = lv00_malloc(sizeof(FormulaNode*) * segment_count);
        if (!node->data.geom_region.boundary_segments) {
            lv00_free((void**)&node->data.geom_region.name);
            lv00_free((void**)&node);
            return NULL;
        }
        memcpy(node->data.geom_region.boundary_segments, segments, sizeof(FormulaNode*) * segment_count);
        node->data.geom_region.segment_count = segment_count;
    }
    return node;
}

FormulaNode *formula_create_geom_arc(const char *name, FormulaNode *center, FormulaNode *radius, FormulaNode *start_angle, FormulaNode *end_angle) {
    if (!name) return NULL;
    FormulaNode *node = lv00_calloc(1, sizeof(FormulaNode));
    if (!node) return NULL;
    node->type = NODE_GEOM_ARC;
    node->line = 1;
    node->column = 1;
    node->data.geom_arc.name = lv00_strdup_safe(name);
    if (!node->data.geom_arc.name) {
        lv00_free((void**)&node);
        return NULL;
    }
    node->data.geom_arc.center = center;
    node->data.geom_arc.radius = radius;
    node->data.geom_arc.start_angle = start_angle;
    node->data.geom_arc.end_angle = end_angle;
    return node;
}

FormulaNode *formula_create_constraint(NodeType constraint_type, FormulaNode **participants, int count) {
    FormulaNode *node = lv00_calloc(1, sizeof(FormulaNode));
    if (!node) return NULL;
    node->type = constraint_type;
    node->line = 1;
    node->column = 1;
    if (count > 0 && participants) {
        node->data.constraint.participants = lv00_malloc(sizeof(FormulaNode*) * count);
        if (!node->data.constraint.participants) {
            lv00_free((void**)&node);
            return NULL;
        }
        memcpy(node->data.constraint.participants, participants, sizeof(FormulaNode*) * count);
        node->data.constraint.participant_count = count;
    }
    return node;
}

FormulaNode *formula_create_compound(FormulaNode **statements, int count) {
    FormulaNode *node = lv00_calloc(1, sizeof(FormulaNode));
    if (!node) return NULL;
    node->type = NODE_COMPOUND;
    node->line = 1;
    node->column = 1;
    if (count > 0 && statements) {
        node->data.compound.statements = lv00_malloc(sizeof(FormulaNode*) * count);
        if (!node->data.compound.statements) {
            lv00_free((void**)&node);
            return NULL;
        }
        memcpy(node->data.compound.statements, statements, sizeof(FormulaNode*) * count);
        node->data.compound.statement_count = count;
    }
    return node;
}

int formula_compound_add_statement(FormulaNode *compound, FormulaNode *statement) {
    if (!compound || compound->type != NODE_COMPOUND || !statement) return -1;
    
    int new_count = compound->data.compound.statement_count + 1;
    FormulaNode **new_statements = lv00_realloc(compound->data.compound.statements,
                                            sizeof(FormulaNode*) * new_count);
    if (!new_statements) return -1;
    
    compound->data.compound.statements = new_statements;
    compound->data.compound.statements[compound->data.compound.statement_count++] = statement;
    return 0;
}

void formula_node_destroy(FormulaNode *node) {
    if (!node) return;

    switch (node->type) {
        case NODE_NUMBER:
            /* 无需释放 */
            break;

        case NODE_VARIABLE:
            lv00_free((void**)&node->data.variable.name);
            break;

        case NODE_IDENTIFIER:
            lv00_free((void**)&node->data.identifier.name);
            break;

        case NODE_BINARY_OP_ADD:
        case NODE_BINARY_OP_SUB:
        case NODE_BINARY_OP_MUL:
        case NODE_BINARY_OP_DIV:
        case NODE_BINARY_OP_POW:
            formula_node_destroy(node->data.binary_op.left);
            formula_node_destroy(node->data.binary_op.right);
            break;

        case NODE_UNARY_OP_NEG:
        case NODE_UNARY_OP_SQRT:
        case NODE_UNARY_OP_SIN:
        case NODE_UNARY_OP_COS:
        case NODE_UNARY_OP_TAN:
        case NODE_UNARY_OP_ABS:
        case NODE_UNARY_OP_LN:
        case NODE_UNARY_OP_LOG:
            formula_node_destroy(node->data.unary_op.operand);
            break;

        case NODE_EQUATION:
            formula_node_destroy(node->data.equation.lhs);
            formula_node_destroy(node->data.equation.rhs);
            break;

        case NODE_COORDINATE_LIST:
            for (int i = 0; i < node->data.coord_list.coord_count; i++) {
                formula_node_destroy(node->data.coord_list.coords[i]);
            }
            lv00_free((void**)&node->data.coord_list.coords);
            break;

        case NODE_GEOM_POINT:
            lv00_free((void**)&node->data.geom_point.name);
            formula_node_destroy(node->data.geom_point.coords);
            break;

        case NODE_GEOM_SEGMENT:
            lv00_free((void**)&node->data.geom_segment.name);
            formula_node_destroy(node->data.geom_segment.endpoint1);
            formula_node_destroy(node->data.geom_segment.endpoint2);
            break;

        case NODE_GEOM_LINE:
            lv00_free((void**)&node->data.geom_line.name);
            formula_node_destroy(node->data.geom_line.point1);
            formula_node_destroy(node->data.geom_line.point2);
            formula_node_destroy(node->data.geom_line.equation);
            break;

        case NODE_GEOM_CIRCLE:
            lv00_free((void**)&node->data.geom_circle.name);
            formula_node_destroy(node->data.geom_circle.center);
            formula_node_destroy(node->data.geom_circle.radius);
            formula_node_destroy(node->data.geom_circle.equation);
            break;

        case NODE_GEOM_TRIANGLE:
            lv00_free((void**)&node->data.geom_triangle.name);
            formula_node_destroy(node->data.geom_triangle.vertex1);
            formula_node_destroy(node->data.geom_triangle.vertex2);
            formula_node_destroy(node->data.geom_triangle.vertex3);
            break;

        case NODE_GEOM_POLYGON:
            lv00_free((void**)&node->data.geom_polygon.name);
            for (int i = 0; i < node->data.geom_polygon.vertex_count; i++) {
                formula_node_destroy(node->data.geom_polygon.vertices[i]);
            }
            lv00_free((void**)&node->data.geom_polygon.vertices);
            break;

        case NODE_GEOM_REGION:
            lv00_free((void**)&node->data.geom_region.name);
            for (int i = 0; i < node->data.geom_region.segment_count; i++) {
                formula_node_destroy(node->data.geom_region.boundary_segments[i]);
            }
            lv00_free((void**)&node->data.geom_region.boundary_segments);
            break;

        case NODE_GEOM_ARC:
            lv00_free((void**)&node->data.geom_arc.name);
            formula_node_destroy(node->data.geom_arc.center);
            formula_node_destroy(node->data.geom_arc.radius);
            formula_node_destroy(node->data.geom_arc.start_angle);
            formula_node_destroy(node->data.geom_arc.end_angle);
            break;

        case NODE_GEOM_VECTOR:
            lv00_free((void**)&node->data.geom_vector.name);
            formula_node_destroy(node->data.geom_vector.start);
            formula_node_destroy(node->data.geom_vector.end);
            break;

        case NODE_CONSTRAINT_PERPENDICULAR:
        case NODE_CONSTRAINT_PARALLEL:
        case NODE_CONSTRAINT_MIDPOINT:
        case NODE_CONSTRAINT_BISECTOR:
        case NODE_CONSTRAINT_COLLINEAR:
        case NODE_CONSTRAINT_TANGENT:
        case NODE_CONSTRAINT_CONGRUENT:
            for (int i = 0; i < node->data.constraint.participant_count; i++) {
                formula_node_destroy(node->data.constraint.participants[i]);
            }
            lv00_free((void**)&node->data.constraint.participants);
            break;

        case NODE_COMPOUND:
            for (int i = 0; i < node->data.compound.statement_count; i++) {
                formula_node_destroy(node->data.compound.statements[i]);
            }
            lv00_free((void**)&node->data.compound.statements);
            break;
    }

    lv00_free((void**)&node);
}

FormulaNode *formula_node_copy(const FormulaNode *node) {
    if (!node) return NULL;

    FormulaNode *copy = lv00_calloc(1, sizeof(FormulaNode));
    if (!copy) return NULL;

    copy->type = node->type;
    copy->line = node->line;
    copy->column = node->column;

    switch (node->type) {
        case NODE_NUMBER:
            copy->data.number = node->data.number;
            break;

        case NODE_VARIABLE:
            copy->data.variable.name = lv00_strdup_safe(node->data.variable.name);
            if (!copy->data.variable.name) {
                lv00_free((void**)&copy);
                return NULL;
            }
            break;

        case NODE_IDENTIFIER:
            copy->data.identifier.name = lv00_strdup_safe(node->data.identifier.name);
            if (!copy->data.identifier.name) {
                lv00_free((void**)&copy);
                return NULL;
            }
            break;

        case NODE_BINARY_OP_ADD:
        case NODE_BINARY_OP_SUB:
        case NODE_BINARY_OP_MUL:
        case NODE_BINARY_OP_DIV:
        case NODE_BINARY_OP_POW:
            copy->data.binary_op.left = formula_node_copy(node->data.binary_op.left);
            copy->data.binary_op.right = formula_node_copy(node->data.binary_op.right);
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
            break;

        case NODE_EQUATION:
            copy->data.equation.lhs = formula_node_copy(node->data.equation.lhs);
            copy->data.equation.rhs = formula_node_copy(node->data.equation.rhs);
            break;

        case NODE_COORDINATE_LIST: {
            copy->data.coord_list.coord_count = node->data.coord_list.coord_count;
            if (node->data.coord_list.coord_count > 0) {
                copy->data.coord_list.coords = lv00_calloc((size_t)node->data.coord_list.coord_count, sizeof(FormulaNode *));
                if (!copy->data.coord_list.coords) { lv00_free((void**)&copy); return NULL; }
                for (int i = 0; i < node->data.coord_list.coord_count; i++) {
                    copy->data.coord_list.coords[i] = formula_node_copy(node->data.coord_list.coords[i]);
                    if (!copy->data.coord_list.coords[i]) {
                        for (int j = 0; j < i; j++) formula_node_destroy(copy->data.coord_list.coords[j]);
                        lv00_free((void**)&copy->data.coord_list.coords);
                        lv00_free((void**)&copy);
                        return NULL;
                    }
                }
            }
            break;
        }

        case NODE_GEOM_POINT:
            copy->data.geom_point.name = node->data.geom_point.name ? lv00_strdup_safe(node->data.geom_point.name) : NULL;
            copy->data.geom_point.coords = formula_node_copy(node->data.geom_point.coords);
            break;

        case NODE_GEOM_SEGMENT:
            copy->data.geom_segment.name = node->data.geom_segment.name ? lv00_strdup_safe(node->data.geom_segment.name) : NULL;
            copy->data.geom_segment.endpoint1 = formula_node_copy(node->data.geom_segment.endpoint1);
            copy->data.geom_segment.endpoint2 = formula_node_copy(node->data.geom_segment.endpoint2);
            break;

        case NODE_GEOM_LINE:
            copy->data.geom_line.name = node->data.geom_line.name ? lv00_strdup_safe(node->data.geom_line.name) : NULL;
            copy->data.geom_line.point1 = formula_node_copy(node->data.geom_line.point1);
            copy->data.geom_line.point2 = formula_node_copy(node->data.geom_line.point2);
            copy->data.geom_line.equation = formula_node_copy(node->data.geom_line.equation);
            break;

        case NODE_GEOM_CIRCLE:
            copy->data.geom_circle.name = node->data.geom_circle.name ? lv00_strdup_safe(node->data.geom_circle.name) : NULL;
            copy->data.geom_circle.center = formula_node_copy(node->data.geom_circle.center);
            copy->data.geom_circle.radius = formula_node_copy(node->data.geom_circle.radius);
            copy->data.geom_circle.equation = formula_node_copy(node->data.geom_circle.equation);
            break;

        case NODE_GEOM_TRIANGLE:
            copy->data.geom_triangle.name = node->data.geom_triangle.name ? lv00_strdup_safe(node->data.geom_triangle.name) : NULL;
            copy->data.geom_triangle.vertex1 = formula_node_copy(node->data.geom_triangle.vertex1);
            copy->data.geom_triangle.vertex2 = formula_node_copy(node->data.geom_triangle.vertex2);
            copy->data.geom_triangle.vertex3 = formula_node_copy(node->data.geom_triangle.vertex3);
            break;

        case NODE_GEOM_POLYGON: {
            copy->data.geom_polygon.name = node->data.geom_polygon.name ? lv00_strdup_safe(node->data.geom_polygon.name) : NULL;
            copy->data.geom_polygon.vertex_count = node->data.geom_polygon.vertex_count;
            if (node->data.geom_polygon.vertex_count > 0) {
                copy->data.geom_polygon.vertices = lv00_calloc((size_t)node->data.geom_polygon.vertex_count, sizeof(FormulaNode *));
                if (!copy->data.geom_polygon.vertices) {
                    lv00_free((void**)&copy->data.geom_polygon.name);
                    lv00_free((void**)&copy);
                    return NULL;
                }
                for (int i = 0; i < node->data.geom_polygon.vertex_count; i++) {
                    copy->data.geom_polygon.vertices[i] = formula_node_copy(node->data.geom_polygon.vertices[i]);
                    if (!copy->data.geom_polygon.vertices[i]) {
                        for (int j = 0; j < i; j++) formula_node_destroy(copy->data.geom_polygon.vertices[j]);
                        lv00_free((void**)&copy->data.geom_polygon.vertices);
                        lv00_free((void**)&copy->data.geom_polygon.name);
                        lv00_free((void**)&copy);
                        return NULL;
                    }
                }
            }
            break;
        }

        case NODE_GEOM_REGION: {
            copy->data.geom_region.name = node->data.geom_region.name ? lv00_strdup_safe(node->data.geom_region.name) : NULL;
            copy->data.geom_region.segment_count = node->data.geom_region.segment_count;
            if (node->data.geom_region.segment_count > 0) {
                copy->data.geom_region.boundary_segments = lv00_calloc((size_t)node->data.geom_region.segment_count, sizeof(FormulaNode *));
                if (!copy->data.geom_region.boundary_segments) {
                    lv00_free((void**)&copy->data.geom_region.name);
                    lv00_free((void**)&copy);
                    return NULL;
                }
                for (int i = 0; i < node->data.geom_region.segment_count; i++) {
                    copy->data.geom_region.boundary_segments[i] = formula_node_copy(node->data.geom_region.boundary_segments[i]);
                    if (!copy->data.geom_region.boundary_segments[i]) {
                        for (int j = 0; j < i; j++) formula_node_destroy(copy->data.geom_region.boundary_segments[j]);
                        lv00_free((void**)&copy->data.geom_region.boundary_segments);
                        lv00_free((void**)&copy->data.geom_region.name);
                        lv00_free((void**)&copy);
                        return NULL;
                    }
                }
            }
            break;
        }

        case NODE_GEOM_ARC:
            copy->data.geom_arc.name = node->data.geom_arc.name ? lv00_strdup_safe(node->data.geom_arc.name) : NULL;
            copy->data.geom_arc.center = formula_node_copy(node->data.geom_arc.center);
            copy->data.geom_arc.radius = formula_node_copy(node->data.geom_arc.radius);
            copy->data.geom_arc.start_angle = formula_node_copy(node->data.geom_arc.start_angle);
            copy->data.geom_arc.end_angle = formula_node_copy(node->data.geom_arc.end_angle);
            break;

        case NODE_GEOM_VECTOR:
            copy->data.geom_vector.name = node->data.geom_vector.name ? lv00_strdup_safe(node->data.geom_vector.name) : NULL;
            copy->data.geom_vector.start = formula_node_copy(node->data.geom_vector.start);
            copy->data.geom_vector.end = formula_node_copy(node->data.geom_vector.end);
            break;

        case NODE_CONSTRAINT_PERPENDICULAR:
        case NODE_CONSTRAINT_PARALLEL:
        case NODE_CONSTRAINT_MIDPOINT:
        case NODE_CONSTRAINT_BISECTOR:
        case NODE_CONSTRAINT_COLLINEAR:
        case NODE_CONSTRAINT_TANGENT:
        case NODE_CONSTRAINT_CONGRUENT: {
            copy->data.constraint.participant_count = node->data.constraint.participant_count;
            if (node->data.constraint.participant_count > 0) {
                copy->data.constraint.participants = lv00_calloc((size_t)node->data.constraint.participant_count, sizeof(FormulaNode *));
                if (!copy->data.constraint.participants) { lv00_free((void**)&copy); return NULL; }
                for (int i = 0; i < node->data.constraint.participant_count; i++) {
                    copy->data.constraint.participants[i] = formula_node_copy(node->data.constraint.participants[i]);
                    if (!copy->data.constraint.participants[i]) {
                        for (int j = 0; j < i; j++) formula_node_destroy(copy->data.constraint.participants[j]);
                        lv00_free((void**)&copy->data.constraint.participants);
                        lv00_free((void**)&copy);
                        return NULL;
                    }
                }
            }
            break;
        }

        case NODE_COMPOUND: {
            copy->data.compound.statement_count = node->data.compound.statement_count;
            if (node->data.compound.statement_count > 0) {
                copy->data.compound.statements = lv00_calloc((size_t)node->data.compound.statement_count, sizeof(FormulaNode *));
                if (!copy->data.compound.statements) { lv00_free((void**)&copy); return NULL; }
                for (int i = 0; i < node->data.compound.statement_count; i++) {
                    copy->data.compound.statements[i] = formula_node_copy(node->data.compound.statements[i]);
                    if (!copy->data.compound.statements[i]) {
                        for (int j = 0; j < i; j++) formula_node_destroy(copy->data.compound.statements[j]);
                        lv00_free((void**)&copy->data.compound.statements);
                        lv00_free((void**)&copy);
                        return NULL;
                    }
                }
            }
            break;
        }

        default:
            /* 未知节点类型，仅拷贝基本字段（type/line/column 已拷贝） */
            lv00_set_error(LV00_ERROR_UNSUPPORTED,
                "formula_node_copy: 未实现的节点类型 %d，仅拷贝基本字段", (int)node->type);
            break;
    }

    return copy;
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
        consume(ctx);  /* 消费 '.' */
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
    lv00_free((void**)&num_str);

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

        if (*p == '-') { negative = true; p++; }
        else if (*p == '+') { p++; }

        /* 步骤1：解析符号后的整数部分 */
        while (p < end && is_digit((unsigned char)*p)) {
            int_part = int_part * 10 + (*p - '0');
            p++;
        }

        /* 步骤2：解析小数点后数字并计算分母 */
        if (p < end && *p == '.') {
            p++;
            while (p < end && is_digit((unsigned char)*p)) {
                frac_part = frac_part * 10 + (*p - '0');
                frac_denom *= 10;  /* 每位小数使分母乘以10 */
                p++;
            }
        }

        /* 步骤3：处理科学计数法指数 */
        if (p < end && (*p == 'e' || *p == 'E')) {
            p++;
            int exp_sign = 1;
            int exp_val = 0;
            if (p < end && *p == '-') { exp_sign = -1; p++; }
            else if (p < end && *p == '+') { p++; }
            while (p < end && is_digit((unsigned char)*p)) {
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
                    if (frac_denom == 0) { frac_denom = 1; break; }
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
        int64_t numerator = int_part * frac_denom + frac_part;
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

        node = formula_create_number(numerator, (uint64_t)denominator);
        if (node) {
            node->line = ctx->line;
            node->column = ctx->column;
            node->data.number.is_integer = (denominator == 1);
        }
    } else {
        /* 纯整数 */
        node = formula_create_number((int64_t)value, 1);
        if (node) {
            node->line = ctx->line;
            node->column = ctx->column;
        }
    }
    return node;
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
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;

        /* 检查是否为关键字 */
        for (int i = 0; DSL_KEYWORDS[i] != NULL; i++) {
            size_t kwlen = strlen(DSL_KEYWORDS[i]);
            if (strncmp(p, DSL_KEYWORDS[i], kwlen) == 0) {
                /* 确保关键字后是空白或分隔符 */
                char next = p[kwlen];
                if (next == '\0' || isspace((unsigned char)next) || next == '(' || next == '{') {
                    return "dsl";
                }
            }
        }

        /* 移动到下一个单词 */
        while (*p && !isspace((unsigned char)*p)) p++;
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
        lv00_free((void**)&name);
        return NULL;
    }

    skip_whitespace(ctx);

    /* 解析坐标列表 */
    FormulaNode *coords[MAX_COORDINATES] = {NULL};
    int coord_count = 0;

    while (!is_at_end(ctx) && peek(ctx) != ')') {
        skip_whitespace(ctx);

        if (coord_count >= MAX_COORDINATES) {
            set_error(ctx, "Too many coordinates");
            lv00_free((void**)&name);
            for (int i = 0; i < coord_count; i++) formula_node_destroy(coords[i]);
            return NULL;
        }

        coords[coord_count] = parse_dsl_expression(ctx);
        if (!coords[coord_count]) {
            lv00_free((void**)&name);
            for (int i = 0; i < coord_count; i++) formula_node_destroy(coords[i]);
            return NULL;
        }
        coord_count++;

        skip_whitespace(ctx);

        if (peek(ctx) == ',') {
            consume(ctx);
        } else if (peek(ctx) != ')') {
            set_error(ctx, "Expected ',' or ')'");
            lv00_free((void**)&name);
            for (int i = 0; i < coord_count; i++) formula_node_destroy(coords[i]);
            return NULL;
        }
    }

    /* 期望 ')' */
    if (!expect_char(ctx, ')')) {
        lv00_free((void**)&name);
        for (int i = 0; i < coord_count; i++) formula_node_destroy(coords[i]);
        return NULL;
    }

    FormulaNode *coord_list = formula_create_coord_list(coords, coord_count);
    for (int i = 0; i < coord_count; i++) formula_node_destroy(coords[i]);

    FormulaNode *node = formula_create_geom_point(name, coord_list);
    lv00_free((void**)&name);
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
        lv00_free((void**)&name);
        return NULL;
    }

    skip_whitespace(ctx);

    /* 解析起点 */
    FormulaNode *ep1 = parse_dsl_atom(ctx);
    if (!ep1) {
        lv00_free((void**)&name);
        return NULL;
    }

    skip_whitespace(ctx);

    /* 期望 ',' */
    if (!expect_char(ctx, ',')) {
        lv00_free((void**)&name);
        formula_node_destroy(ep1);
        return NULL;
    }

    skip_whitespace(ctx);

    /* 解析终点 */
    FormulaNode *ep2 = parse_dsl_atom(ctx);
    if (!ep2) {
        lv00_free((void**)&name);
        formula_node_destroy(ep1);
        return NULL;
    }

    skip_whitespace(ctx);

    /* 期望 ')' */
    if (!expect_char(ctx, ')')) {
        lv00_free((void**)&name);
        formula_node_destroy(ep1);
        formula_node_destroy(ep2);
        return NULL;
    }

    FormulaNode *node = formula_create_geom_segment(name, ep1, ep2);
    lv00_free((void**)&name);
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
        lv00_free((void**)&name);
        return NULL;
    }

    skip_whitespace(ctx);

    /* 解析圆心 */
    FormulaNode *center = parse_dsl_atom(ctx);
    if (!center) {
        lv00_free((void**)&name);
        return NULL;
    }

    skip_whitespace(ctx);

    /* 期望 ',' */
    if (!expect_char(ctx, ',')) {
        lv00_free((void**)&name);
        formula_node_destroy(center);
        return NULL;
    }

    skip_whitespace(ctx);

    /* 解析半径 */
    FormulaNode *radius = parse_dsl_expression(ctx);
    if (!radius) {
        lv00_free((void**)&name);
        formula_node_destroy(center);
        return NULL;
    }

    skip_whitespace(ctx);

    /* 期望 ')' */
    if (!expect_char(ctx, ')')) {
        lv00_free((void**)&name);
        formula_node_destroy(center);
        formula_node_destroy(radius);
        return NULL;
    }

    FormulaNode *node = formula_create_geom_circle(name, center, radius);
    lv00_free((void**)&name);
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
        lv00_free((void**)&name);
        return NULL;
    }

    skip_whitespace(ctx);

    /* 解析三个顶点 */
    FormulaNode *vertices[3] = {NULL, NULL, NULL};
    for (int i = 0; i < 3; i++) {
        vertices[i] = parse_dsl_atom(ctx);
        if (!vertices[i]) {
            lv00_free((void**)&name);
            for (int j = 0; j < i; j++) formula_node_destroy(vertices[j]);
            set_error(ctx, "Expected vertex");
            return NULL;
        }

        skip_whitespace(ctx);

        if (i < 2) {
            if (!expect_char(ctx, ',')) {
                lv00_free((void**)&name);
                for (int j = 0; j <= i; j++) formula_node_destroy(vertices[j]);
                return NULL;
            }
            skip_whitespace(ctx);
        }
    }

    /* 期望 ')' */
    if (!expect_char(ctx, ')')) {
        lv00_free((void**)&name);
        for (int i = 0; i < 3; i++) formula_node_destroy(vertices[i]);
        return NULL;
    }

    FormulaNode *node = formula_create_geom_triangle(name, vertices[0], vertices[1], vertices[2]);
    lv00_free((void**)&name);
    for (int i = 0; i < 3; i++) formula_node_destroy(vertices[i]);
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
        lv00_free((void**)&name);
        return NULL;
    }

    skip_whitespace(ctx);

    /* 解析圆心 */
    FormulaNode *center = parse_dsl_atom(ctx);
    if (!center) {
        lv00_free((void**)&name);
        return NULL;
    }

    skip_whitespace(ctx);

    /* 期望 ',' */
    if (!expect_char(ctx, ',')) {
        lv00_free((void**)&name);
        formula_node_destroy(center);
        return NULL;
    }

    skip_whitespace(ctx);

    /* 解析半径 */
    FormulaNode *radius = parse_dsl_expression(ctx);
    if (!radius) {
        lv00_free((void**)&name);
        formula_node_destroy(center);
        return NULL;
    }

    skip_whitespace(ctx);

    /* 期望 ',' */
    if (!expect_char(ctx, ',')) {
        lv00_free((void**)&name);
        formula_node_destroy(center);
        formula_node_destroy(radius);
        return NULL;
    }

    skip_whitespace(ctx);

    /* 解析起始角度 */
    FormulaNode *start_angle = parse_dsl_expression(ctx);
    if (!start_angle) {
        lv00_free((void**)&name);
        formula_node_destroy(center);
        formula_node_destroy(radius);
        return NULL;
    }

    skip_whitespace(ctx);

    /* 期望 ',' */
    if (!expect_char(ctx, ',')) {
        lv00_free((void**)&name);
        formula_node_destroy(center);
        formula_node_destroy(radius);
        formula_node_destroy(start_angle);
        return NULL;
    }

    skip_whitespace(ctx);

    /* 解析结束角度 */
    FormulaNode *end_angle = parse_dsl_expression(ctx);
    if (!end_angle) {
        lv00_free((void**)&name);
        formula_node_destroy(center);
        formula_node_destroy(radius);
        formula_node_destroy(start_angle);
        return NULL;
    }

    skip_whitespace(ctx);

    /* 期望 ')' */
    if (!expect_char(ctx, ')')) {
        lv00_free((void**)&name);
        formula_node_destroy(center);
        formula_node_destroy(radius);
        formula_node_destroy(start_angle);
        formula_node_destroy(end_angle);
        return NULL;
    }

    FormulaNode *node = formula_create_geom_arc(name, center, radius, start_angle, end_angle);
    lv00_free((void**)&name);
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
        lv00_free((void**)&name);
        return NULL;
    }

    skip_whitespace(ctx);

    /* 期望 '[' */
    if (!expect_char(ctx, '[')) {
        lv00_free((void**)&name);
        return NULL;
    }

    skip_whitespace(ctx);

    /* 解析顶点列表 */
    FormulaNode *vertices[MAX_POLYGON_VERTICES] = {NULL};
    int vertex_count = 0;

    while (!is_at_end(ctx) && peek(ctx) != ']') {
        if (vertex_count >= MAX_POLYGON_VERTICES) {
            set_error(ctx, "Too many vertices in polygon");
            lv00_free((void**)&name);
            for (int i = 0; i < vertex_count; i++) formula_node_destroy(vertices[i]);
            return NULL;
        }

        vertices[vertex_count] = parse_dsl_atom(ctx);
        if (!vertices[vertex_count]) {
            lv00_free((void**)&name);
            for (int i = 0; i < vertex_count; i++) formula_node_destroy(vertices[i]);
            return NULL;
        }
        vertex_count++;

        skip_whitespace(ctx);

        if (peek(ctx) == ',') {
            consume(ctx);
            skip_whitespace(ctx);
        } else if (peek(ctx) != ']') {
            set_error(ctx, "Expected ',' or ']'");
            lv00_free((void**)&name);
            for (int i = 0; i < vertex_count; i++) formula_node_destroy(vertices[i]);
            return NULL;
        }
    }

    /* 期望 ']' */
    if (!expect_char(ctx, ']')) {
        lv00_free((void**)&name);
        for (int i = 0; i < vertex_count; i++) formula_node_destroy(vertices[i]);
        return NULL;
    }

    skip_whitespace(ctx);

    /* 期望 ')' */
    if (!expect_char(ctx, ')')) {
        lv00_free((void**)&name);
        for (int i = 0; i < vertex_count; i++) formula_node_destroy(vertices[i]);
        return NULL;
    }

    FormulaNode *node = formula_create_geom_polygon(name, vertices, vertex_count);
    lv00_free((void**)&name);
    for (int i = 0; i < vertex_count; i++) formula_node_destroy(vertices[i]);
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
        lv00_free((void**)&name);
        return NULL;
    }

    skip_whitespace(ctx);

    /* 期望 '[' */
    if (!expect_char(ctx, '[')) {
        lv00_free((void**)&name);
        return NULL;
    }

    skip_whitespace(ctx);

    /* 解析边界线段列表 */
    FormulaNode *segments[MAX_POLYGON_VERTICES] = {NULL};
    int segment_count = 0;

    while (!is_at_end(ctx) && peek(ctx) != ']') {
        if (segment_count >= MAX_POLYGON_VERTICES) {
            set_error(ctx, "Too many segments in region");
            lv00_free((void**)&name);
            for (int i = 0; i < segment_count; i++) formula_node_destroy(segments[i]);
            return NULL;
        }

        segments[segment_count] = parse_dsl_atom(ctx);
        if (!segments[segment_count]) {
            lv00_free((void**)&name);
            for (int i = 0; i < segment_count; i++) formula_node_destroy(segments[i]);
            return NULL;
        }
        segment_count++;

        skip_whitespace(ctx);

        if (peek(ctx) == ',') {
            consume(ctx);
            skip_whitespace(ctx);
        } else if (peek(ctx) != ']') {
            set_error(ctx, "Expected ',' or ']'");
            lv00_free((void**)&name);
            for (int i = 0; i < segment_count; i++) formula_node_destroy(segments[i]);
            return NULL;
        }
    }

    /* 期望 ']' */
    if (!expect_char(ctx, ']')) {
        lv00_free((void**)&name);
        for (int i = 0; i < segment_count; i++) formula_node_destroy(segments[i]);
        return NULL;
    }

    skip_whitespace(ctx);

    /* 期望 ')' */
    if (!expect_char(ctx, ')')) {
        lv00_free((void**)&name);
        for (int i = 0; i < segment_count; i++) formula_node_destroy(segments[i]);
        return NULL;
    }

    FormulaNode *node = formula_create_geom_region(name, segments, segment_count);
    lv00_free((void**)&name);
    for (int i = 0; i < segment_count; i++) formula_node_destroy(segments[i]);
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
    if (strcmp(name, "perpendicular") == 0) return NODE_CONSTRAINT_PERPENDICULAR;
    if (strcmp(name, "parallel") == 0) return NODE_CONSTRAINT_PARALLEL;
    if (strcmp(name, "midpoint") == 0) return NODE_CONSTRAINT_MIDPOINT;
    if (strcmp(name, "bisector") == 0) return NODE_CONSTRAINT_BISECTOR;
    if (strcmp(name, "collinear") == 0) return NODE_CONSTRAINT_COLLINEAR;
    if (strcmp(name, "tangent") == 0) return NODE_CONSTRAINT_TANGENT;
    if (strcmp(name, "congruent") == 0) return NODE_CONSTRAINT_CONGRUENT;
    return (NodeType)-1;  /* 未知约束类型，由调用方处理 */
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
    if ((int)constraint_type < 0) {
        char err_buf[MAX_TEMP_MSG_SIZE];
        snprintf(err_buf, sizeof(err_buf), "未知的约束类型: %s", constraint_name);
        set_error(ctx, err_buf);
        lv00_free((void**)&constraint_name);
        return NULL;
    }
    lv00_free((void**)&constraint_name);

    skip_whitespace(ctx);

    /* 期望 '(' */
    if (!expect_char(ctx, '(')) {
        return NULL;
    }

    skip_whitespace(ctx);

    /* 解析参数列表 */
    FormulaNode *participants[MAX_PARTICIPANTS] = {NULL};
    int participant_count = 0;

    while (!is_at_end(ctx) && peek(ctx) != ')') {
        skip_whitespace(ctx);

        if (participant_count >= MAX_PARTICIPANTS) {
            set_error(ctx, "Too many participants");
            for (int i = 0; i < participant_count; i++) formula_node_destroy(participants[i]);
            return NULL;
        }

        participants[participant_count] = parse_dsl_atom(ctx);
        if (!participants[participant_count]) {
            for (int i = 0; i < participant_count; i++) formula_node_destroy(participants[i]);
            return NULL;
        }
        participant_count++;

        skip_whitespace(ctx);

        if (peek(ctx) == ',') {
            consume(ctx);
        } else if (peek(ctx) != ')') {
            set_error(ctx, "Expected ',' or ')'");
            for (int i = 0; i < participant_count; i++) formula_node_destroy(participants[i]);
            return NULL;
        }
    }

    /* 期望 ')' */
    if (!expect_char(ctx, ')')) {
        for (int i = 0; i < participant_count; i++) formula_node_destroy(participants[i]);
        return NULL;
    }

    FormulaNode *node = formula_create_constraint(constraint_type, participants, participant_count);
    for (int i = 0; i < participant_count; i++) formula_node_destroy(participants[i]);
    return node;
}

/**
 * @brief 解析 DSL 原子表达式
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
        if (!expr) return NULL;
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
        if (!operand) return NULL;
        return formula_create_unary_op(NODE_UNARY_OP_NEG, operand);
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
        if (!ident) return NULL;

        skip_whitespace(ctx);

        /* 检查是否为关键字 */
        if (strcmp(ident, "point") == 0) {
            lv00_free((void**)&ident);
            return parse_dsl_point(ctx);
        }
        if (strcmp(ident, "segment") == 0) {
            lv00_free((void**)&ident);
            return parse_dsl_segment(ctx);
        }
        if (strcmp(ident, "circle") == 0) {
            lv00_free((void**)&ident);
            return parse_dsl_circle(ctx);
        }
        if (strcmp(ident, "triangle") == 0) {
            lv00_free((void**)&ident);
            return parse_dsl_triangle(ctx);
        }
        if (strcmp(ident, "arc") == 0) {
            lv00_free((void**)&ident);
            return parse_dsl_arc(ctx);
        }
        if (strcmp(ident, "polygon") == 0) {
            lv00_free((void**)&ident);
            return parse_dsl_polygon(ctx);
        }
        if (strcmp(ident, "region") == 0) {
            lv00_free((void**)&ident);
            return parse_dsl_region(ctx);
        }
        if (is_dsl_keyword(ident)) {
            /* 其他约束关键字 */
            ctx->pos = start;  /* 回退 */
            lv00_free((void**)&ident);
            return parse_dsl_constraint(ctx);
        }

        /* 函数调用 */
        if (peek(ctx) == '(') {
            consume(ctx);
            skip_whitespace(ctx);

            /* 解析参数 */
            FormulaNode *args[MAX_ARGUMENTS] = {NULL};
            int arg_count = 0;

            while (!is_at_end(ctx) && peek(ctx) != ')') {
                if (arg_count >= MAX_ARGUMENTS) {
                    set_error(ctx, "Too many arguments");
                    lv00_free((void**)&ident);
                    for (int i = 0; i < arg_count; i++) formula_node_destroy(args[i]);
                    return NULL;
                }

                args[arg_count] = parse_dsl_expression(ctx);
                if (!args[arg_count]) {
                    lv00_free((void**)&ident);
                    for (int i = 0; i < arg_count; i++) formula_node_destroy(args[i]);
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
                lv00_free((void**)&ident);
                for (int i = 0; i < arg_count; i++) formula_node_destroy(args[i]);
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

            lv00_free((void**)&ident);
            for (int i = 0; i < arg_count; i++) formula_node_destroy(args[i]);
            return node;
        }

        /* 普通标识符 */
        FormulaNode *node = formula_create_identifier(ident);
        lv00_free((void**)&ident);
        return node;
    }

    set_error(ctx, "Unexpected character");
    return NULL;
}

/**
 * @brief 解析 DSL 因子（处理幂运算）
 */
static FormulaNode *parse_dsl_factor(ParserContext *ctx) {
    FormulaNode *left = parse_dsl_atom(ctx);
    if (!left) return NULL;

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
        return formula_create_binary_op(NODE_BINARY_OP_POW, left, right);
    }

    return left;
}

/**
 * @brief 解析 DSL 项（处理乘除）
 */
static FormulaNode *parse_dsl_term(ParserContext *ctx) {
    FormulaNode *left = parse_dsl_factor(ctx);
    if (!left) return NULL;

    while (true) {
        skip_whitespace(ctx);
        char c = peek(ctx);

        NodeType op_type;
        bool should_continue = false;

        if (c == '*') {
            if (peek_next(ctx) == '*') break;  /* 幂运算 */
            consume(ctx);
            op_type = NODE_BINARY_OP_MUL;
            should_continue = true;
        } else if (c == '/') {
            consume(ctx);
            op_type = NODE_BINARY_OP_DIV;
            should_continue = true;
        }

        if (!should_continue) break;

        skip_whitespace(ctx);
        FormulaNode *right = parse_dsl_factor(ctx);
        if (!right) {
            formula_node_destroy(left);
            return NULL;
        }

        left = formula_create_binary_op(op_type, left, right);
        if (!left) return NULL;
    }

    return left;
}

/**
 * @brief 解析 DSL 表达式（处理加减）
 */
static FormulaNode *parse_dsl_expression(ParserContext *ctx) {
    FormulaNode *left = parse_dsl_term(ctx);
    if (!left) return NULL;

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

        if (!should_continue) break;

        skip_whitespace(ctx);
        FormulaNode *right = parse_dsl_term(ctx);
        if (!right) {
            formula_node_destroy(left);
            return NULL;
        }

        left = formula_create_binary_op(op_type, left, right);
        if (!left) return NULL;
    }

    return left;
}

/**
 * @brief 解析 DSL 语句
 */
static FormulaNode *parse_dsl_statement(ParserContext *ctx) {
    skip_whitespace(ctx);

    if (is_at_end(ctx)) return NULL;

    FormulaNode *left = parse_dsl_expression(ctx);
    if (!left) return NULL;

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
        return formula_create_equation(left, right);
    }

    return left;
}

/**
 * @brief 解析 DSL 复合语句
 */
static FormulaNode *parse_dsl_compound(ParserContext *ctx) {
    FormulaNode *statements[MAX_STATEMENTS] = {NULL};
    int statement_count = 0;

    while (!is_at_end(ctx)) {
        skip_whitespace(ctx);
        if (is_at_end(ctx)) break;

        if (statement_count >= MAX_STATEMENTS) {
            set_error(ctx, "Too many statements");
            for (int i = 0; i < statement_count; i++) formula_node_destroy(statements[i]);
            return NULL;
        }

        FormulaNode *stmt = parse_dsl_statement(ctx);
        if (!stmt) {
            if (ctx->has_error) {
                for (int i = 0; i < statement_count; i++) formula_node_destroy(statements[i]);
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
static FormulaNode *parse_latex_term(ParserContext *ctx);
static FormulaNode *parse_latex_factor(ParserContext *ctx);
static FormulaNode *parse_latex_atom(ParserContext *ctx);

/**
 * @brief 解析 LaTeX 分数 \frac{a}{b}
 */
static FormulaNode *parse_latex_frac(ParserContext *ctx) {
    skip_whitespace(ctx);

    /* 期望 '{' */
    if (!expect_char(ctx, '{')) {
        return NULL;
    }

    skip_whitespace(ctx);
    FormulaNode *numerator = parse_latex_expression(ctx);
    if (!numerator) return NULL;
    skip_whitespace(ctx);

    if (!expect_char(ctx, '}')) {
        formula_node_destroy(numerator);
        return NULL;
    }

    skip_whitespace(ctx);
    if (!expect_char(ctx, '{')) {
        formula_node_destroy(numerator);
        return NULL;
    }

    skip_whitespace(ctx);
    FormulaNode *denominator = parse_latex_expression(ctx);
    if (!denominator) {
        formula_node_destroy(numerator);
        return NULL;
    }
    skip_whitespace(ctx);

    if (!expect_char(ctx, '}')) {
        formula_node_destroy(numerator);
        formula_node_destroy(denominator);
        return NULL;
    }

    return formula_create_binary_op(NODE_BINARY_OP_DIV, numerator, denominator);
}

/**
 * @brief 解析 LaTeX 根号 \sqrt{x}
 */
static FormulaNode *parse_latex_sqrt(ParserContext *ctx) {
    skip_whitespace(ctx);

    if (peek(ctx) == '{') {
        consume(ctx);
        skip_whitespace(ctx);
        FormulaNode *operand = parse_latex_expression(ctx);
        if (!operand) return NULL;
        skip_whitespace(ctx);
        if (!expect_char(ctx, '}')) {
            formula_node_destroy(operand);
            return NULL;
        }
        return formula_create_unary_op(NODE_UNARY_OP_SQRT, operand);
    }

    FormulaNode *operand = parse_latex_atom(ctx);
    if (!operand) return NULL;
    return formula_create_unary_op(NODE_UNARY_OP_SQRT, operand);
}

/**
 * @brief 解析 LaTeX 命令
 */
static FormulaNode *parse_latex_command(ParserContext *ctx) {
    /* 已经匹配了 '\' */
    char *cmd = parse_identifier_str(ctx);
    if (!cmd) {
        set_error(ctx, "Expected LaTeX command after '\\'");
        return NULL;
    }

    FormulaNode *result = NULL;

    if (strcmp(cmd, "frac") == 0) {
        lv00_free((void**)&cmd);
        return parse_latex_frac(ctx);
    }
    if (strcmp(cmd, "sqrt") == 0) {
        lv00_free((void**)&cmd);
        return parse_latex_sqrt(ctx);
    }
    if (strcmp(cmd, "sin") == 0) {
        lv00_free((void**)&cmd);
        skip_whitespace(ctx);
        if (peek(ctx) == '{') {
            consume(ctx);
            skip_whitespace(ctx);
            FormulaNode *arg = parse_latex_expression(ctx);
            if (arg) {
                skip_whitespace(ctx);
                if (expect_char(ctx, '}')) {
                    result = formula_create_unary_op(NODE_UNARY_OP_SIN, arg);
                } else {
                    formula_node_destroy(arg);
                }
            }
        } else {
            FormulaNode *arg = parse_latex_atom(ctx);
            if (arg) result = formula_create_unary_op(NODE_UNARY_OP_SIN, arg);
        }
        return result;
    }
    if (strcmp(cmd, "cos") == 0) {
        lv00_free((void**)&cmd);
        skip_whitespace(ctx);
        if (peek(ctx) == '{') {
            consume(ctx);
            skip_whitespace(ctx);
            FormulaNode *arg = parse_latex_expression(ctx);
            if (arg) {
                skip_whitespace(ctx);
                if (expect_char(ctx, '}')) {
                    result = formula_create_unary_op(NODE_UNARY_OP_COS, arg);
                } else {
                    formula_node_destroy(arg);
                }
            }
        } else {
            FormulaNode *arg = parse_latex_atom(ctx);
            if (arg) result = formula_create_unary_op(NODE_UNARY_OP_COS, arg);
        }
        return result;
    }
    if (strcmp(cmd, "tan") == 0) {
        lv00_free((void**)&cmd);
        skip_whitespace(ctx);
        if (peek(ctx) == '{') {
            consume(ctx);
            skip_whitespace(ctx);
            FormulaNode *arg = parse_latex_expression(ctx);
            if (arg) {
                skip_whitespace(ctx);
                if (expect_char(ctx, '}')) {
                    result = formula_create_unary_op(NODE_UNARY_OP_TAN, arg);
                } else {
                    formula_node_destroy(arg);
                }
            }
        } else {
            FormulaNode *arg = parse_latex_atom(ctx);
            if (arg) result = formula_create_unary_op(NODE_UNARY_OP_TAN, arg);
        }
        return result;
    }
    if (strcmp(cmd, "pi") == 0) {
        lv00_free((void**)&cmd);
        return formula_create_variable("pi");
    }

    /* 其他命令作为变量 */
    result = formula_create_variable(cmd);
    lv00_free((void**)&cmd);
    return result;
}

/**
 * @brief 解析 LaTeX 原子
 */
static FormulaNode *parse_latex_atom(ParserContext *ctx) {
    skip_whitespace(ctx);

    char c = peek(ctx);

    /* 数字 */
    if (is_digit(c) || (c == '.' && is_digit(peek_next(ctx)))) {
        return parse_number(ctx);
    }

    /* 括号表达式 */
    if (c == '(') {
        consume(ctx);
        FormulaNode *expr = parse_latex_expression(ctx);
        if (!expr) return NULL;
        skip_whitespace(ctx);
        if (!expect_char(ctx, ')')) {
            formula_node_destroy(expr);
            return NULL;
        }
        return expr;
    }

    /* LaTeX 命令 */
    if (c == '\\') {
        consume(ctx);
        return parse_latex_command(ctx);
    }

    /* 花括号分组 */
    if (c == '{') {
        consume(ctx);
        skip_whitespace(ctx);
        FormulaNode *expr = parse_latex_expression(ctx);
        if (!expr) return NULL;
        skip_whitespace(ctx);
        if (!expect_char(ctx, '}')) {
            formula_node_destroy(expr);
            return NULL;
        }
        return expr;
    }

    /* 负号 */
    if (c == '-') {
        consume(ctx);
        FormulaNode *operand = parse_latex_factor(ctx);
        if (!operand) return NULL;
        return formula_create_unary_op(NODE_UNARY_OP_NEG, operand);
    }

    /* 正号 */
    if (c == '+') {
        consume(ctx);
        return parse_latex_factor(ctx);
    }

    /* 标识符 */
    if (is_alpha(c)) {
        char *ident = parse_identifier_str(ctx);
        if (!ident) return NULL;

        FormulaNode *node = formula_create_variable(ident);
        lv00_free((void**)&ident);
        return node;
    }

    set_error(ctx, "Unexpected character in LaTeX expression");
    return NULL;
}

/**
 * @brief 解析 LaTeX 因子
 */
static FormulaNode *parse_latex_factor(ParserContext *ctx) {
    FormulaNode *left = parse_latex_atom(ctx);
    if (!left) return NULL;

    skip_whitespace(ctx);

    /* 处理上标 */
    if (peek(ctx) == '^') {
        consume(ctx);
        skip_whitespace(ctx);
        FormulaNode *exponent = NULL;
        if (peek(ctx) == '{') {
            consume(ctx);
            skip_whitespace(ctx);
            exponent = parse_latex_expression(ctx);
            if (exponent) {
                skip_whitespace(ctx);
                if (!expect_char(ctx, '}')) {
                    formula_node_destroy(left);
                    formula_node_destroy(exponent);
                    return NULL;
                }
            }
        } else {
            exponent = parse_latex_atom(ctx);
        }
        if (!exponent) {
            formula_node_destroy(left);
            return NULL;
        }
        return formula_create_binary_op(NODE_BINARY_OP_POW, left, exponent);
    }

    return left;
}

/**
 * @brief 解析 LaTeX 项
 */
static FormulaNode *parse_latex_term(ParserContext *ctx) {
    FormulaNode *left = parse_latex_factor(ctx);
    if (!left) return NULL;

    while (true) {
        skip_whitespace(ctx);
        char c = peek(ctx);

        NodeType op_type = NODE_BINARY_OP_MUL;
        bool should_continue = false;

        if (c == '*') {
            consume(ctx);
            should_continue = true;
        } else if (c == '/') {
            consume(ctx);
            op_type = NODE_BINARY_OP_DIV;
            should_continue = true;
        } else if (match_and_consume(ctx, "\\cdot")) {
            should_continue = true;
        } else if (match_and_consume(ctx, "\\times")) {
            should_continue = true;
        } else if (match_and_consume(ctx, "\\div")) {
            op_type = NODE_BINARY_OP_DIV;
            should_continue = true;
        }

        if (!should_continue) break;

        skip_whitespace(ctx);
        FormulaNode *right = parse_latex_factor(ctx);
        if (!right) {
            formula_node_destroy(left);
            return NULL;
        }

        left = formula_create_binary_op(op_type, left, right);
        if (!left) return NULL;
    }

    return left;
}

/**
 * @brief 解析 LaTeX 表达式
 */
static FormulaNode *parse_latex_expression(ParserContext *ctx) {
    FormulaNode *left = parse_latex_term(ctx);
    if (!left) return NULL;

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

        if (!should_continue) break;

        skip_whitespace(ctx);
        FormulaNode *right = parse_latex_term(ctx);
        if (!right) {
            formula_node_destroy(left);
            return NULL;
        }

        left = formula_create_binary_op(op_type, left, right);
        if (!left) return NULL;
    }

    /* 检查等式 */
    skip_whitespace(ctx);
    if (peek(ctx) == '=' && peek_next(ctx) != '=') {
        consume(ctx);
        skip_whitespace(ctx);
        FormulaNode *right = parse_latex_expression(ctx);
        if (!right) {
            formula_node_destroy(left);
            return NULL;
        }
        return formula_create_equation(left, right);
    }

    return left;
}

/* ============================================================
 * Python 解析器
 * ============================================================ */

static FormulaNode *parse_python_expression(ParserContext *ctx);
static FormulaNode *parse_python_term(ParserContext *ctx);
static FormulaNode *parse_python_factor(ParserContext *ctx);
static FormulaNode *parse_python_atom(ParserContext *ctx);

/**
 * @brief 解析 Python 原子
 */
static FormulaNode *parse_python_atom(ParserContext *ctx) {
    skip_whitespace(ctx);

    char c = peek(ctx);

    /* 数字 */
    if (is_digit(c) || (c == '.' && is_digit(peek_next(ctx)))) {
        return parse_number(ctx);
    }

    /* 括号表达式 */
    if (c == '(') {
        consume(ctx);
        skip_whitespace(ctx);
        FormulaNode *expr = parse_python_expression(ctx);
        if (!expr) return NULL;
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
        FormulaNode *operand = parse_python_factor(ctx);
        if (!operand) return NULL;
        return formula_create_unary_op(NODE_UNARY_OP_NEG, operand);
    }

    /* 正号 */
    if (c == '+') {
        consume(ctx);
        return parse_python_factor(ctx);
    }

    /* 标识符 */
    if (is_alpha(c)) {
        char *ident = parse_identifier_str(ctx);
        if (!ident) return NULL;

        skip_whitespace(ctx);

        /* 检查布尔值 */
        if (strcmp(ident, "True") == 0 || strcmp(ident, "False") == 0) {
            int val = (strcmp(ident, "True") == 0) ? 1 : 0;
            lv00_free((void**)&ident);
            return formula_create_number(val, 1);
        }
        if (strcmp(ident, "None") == 0) {
            lv00_free((void**)&ident);
            return formula_create_variable("None");
        }
        if (strcmp(ident, "pi") == 0) {
            lv00_free((void**)&ident);
            return formula_create_variable("pi");
        }

        /* 函数调用 */
        if (peek(ctx) == '(') {
            consume(ctx);
            skip_whitespace(ctx);

            /* 解析参数 */
            FormulaNode *args[MAX_ARGUMENTS] = {NULL};
            int arg_count = 0;

            while (!is_at_end(ctx) && peek(ctx) != ')') {
                if (arg_count >= MAX_ARGUMENTS) {
                    set_error(ctx, "Too many arguments");
                    lv00_free((void**)&ident);
                    for (int i = 0; i < arg_count; i++) formula_node_destroy(args[i]);
                    return NULL;
                }

                args[arg_count] = parse_python_expression(ctx);
                if (!args[arg_count]) {
                    lv00_free((void**)&ident);
                    for (int i = 0; i < arg_count; i++) formula_node_destroy(args[i]);
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
                lv00_free((void**)&ident);
                for (int i = 0; i < arg_count; i++) formula_node_destroy(args[i]);
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
            } else if (strcmp(ident, "pow") == 0 && arg_count == 2) {
                node = formula_create_binary_op(NODE_BINARY_OP_POW, args[0], args[1]);
            } else {
                node = formula_create_variable(ident);
            }

            lv00_free((void**)&ident);
            for (int i = 0; i < arg_count; i++) formula_node_destroy(args[i]);
            return node;
        }

        FormulaNode *node = formula_create_variable(ident);
        lv00_free((void**)&ident);
        return node;
    }

    set_error(ctx, "Unexpected character in Python expression");
    return NULL;
}

/**
 * @brief 解析 Python 幂运算
 */
static FormulaNode *parse_python_power(ParserContext *ctx) {
    FormulaNode *left = parse_python_atom(ctx);
    if (!left) return NULL;

    skip_whitespace(ctx);

    /* 处理幂运算 ** */
    if (match_string(ctx, "**")) {
        consume(ctx);
        consume(ctx);
        skip_whitespace(ctx);
        FormulaNode *right = parse_python_factor(ctx);
        if (!right) {
            formula_node_destroy(left);
            return NULL;
        }
        return formula_create_binary_op(NODE_BINARY_OP_POW, left, right);
    }

    return left;
}

/**
 * @brief 解析 Python 因子
 */
static FormulaNode *parse_python_factor(ParserContext *ctx) {
    return parse_python_power(ctx);
}

/**
 * @brief 解析 Python 项
 */
static FormulaNode *parse_python_term(ParserContext *ctx) {
    FormulaNode *left = parse_python_factor(ctx);
    if (!left) return NULL;

    while (true) {
        skip_whitespace(ctx);
        char c = peek(ctx);

        NodeType op_type = NODE_BINARY_OP_MUL;
        bool should_continue = false;

        if (c == '*') {
            if (peek_next(ctx) == '*') break;  /* 幂运算 */
            consume(ctx);
            should_continue = true;
        } else if (c == '/') {
            consume(ctx);
            op_type = NODE_BINARY_OP_DIV;
            should_continue = true;
        } else if (c == '%') {
            consume(ctx);
            should_continue = true;
        }

        if (!should_continue) break;

        skip_whitespace(ctx);
        FormulaNode *right = parse_python_factor(ctx);
        if (!right) {
            formula_node_destroy(left);
            return NULL;
        }

        left = formula_create_binary_op(op_type, left, right);
        if (!left) return NULL;
    }

    return left;
}

/**
 * @brief 解析 Python 表达式
 */
static FormulaNode *parse_python_expression(ParserContext *ctx) {
    FormulaNode *left = parse_python_term(ctx);
    if (!left) return NULL;

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

        if (!should_continue) break;

        skip_whitespace(ctx);
        FormulaNode *right = parse_python_term(ctx);
        if (!right) {
            formula_node_destroy(left);
            return NULL;
        }

        left = formula_create_binary_op(op_type, left, right);
        if (!left) return NULL;
    }

    /* 检查等式 */
    skip_whitespace(ctx);
    if (match_and_consume(ctx, "==")) {
        skip_whitespace(ctx);
        FormulaNode *right = parse_python_expression(ctx);
        if (!right) {
            formula_node_destroy(left);
            return NULL;
        }
        return formula_create_equation(left, right);
    }

    return left;
}

/* ============================================================
 * 主解析函数
 * ============================================================ */

FormulaNode *formula_parse(const char *input, const char *syntax) {
    if (!input) {
        lv00_set_error(LV00_ERROR_NULL_POINTER, "Input is NULL");
        return NULL;
    }

    lv00_clear_error();

    if (formula_parser_stream_ctx) {
        stream_emit_info(formula_parser_stream_ctx, "公式解析开始", 0);
    }

    /* 初始化解析上下文 */
    ParserContext ctx = {0};
    ctx.input = input;
    ctx.pos = 0;
    ctx.length = strlen(input);
    ctx.line = 1;
    ctx.column = 1;
    ctx.has_error = false;

    FormulaNode *ast = NULL;

    /* 根据语法类型选择解析器 */
    if (syntax == NULL || strcmp(syntax, "auto") == 0) {
        syntax = formula_detect_syntax(input);
    }

    if (strcmp(syntax, "dsl") == 0) {
        ast = parse_dsl_compound(&ctx);
    } else if (strcmp(syntax, "latex") == 0) {
        ast = parse_latex_expression(&ctx);
    } else if (strcmp(syntax, "python") == 0) {
        ast = parse_python_expression(&ctx);
    } else {
        /* 默认尝试 DSL */
        ast = parse_dsl_compound(&ctx);
    }

    if (ctx.has_error) {
        lv00_set_error(LV00_ERROR_PARSE, "%s", ctx.error_message);
        if (formula_parser_stream_ctx) {
            stream_emit_error(formula_parser_stream_ctx, "公式解析错误", 0);
        }
    } else {
        if (formula_parser_stream_ctx) {
            stream_emit_progress(formula_parser_stream_ctx, 1.0, "公式解析完成", 1, 1);
        }
    }

    return ast;
}

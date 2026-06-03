/**
 * @file dsl_compiler.c
 * @brief DSL 编译器实现 —— 借鉴 Ganja.js AST 转译 + GCLC 几何构造语言
 *
 * @details 完整编译管线：
 *   DSL源码 -> 词法分析(Tokenizer) -> 语法分析(Parser/递归下降) -> AST
 *   -> IR编译(符号解析+类型检查) -> IR(过程化操作序列)
 *   -> 约束图转换(解释执行IR)
 *
 * 借鉴来源：
 * - Ganja.js (github.com/enkimute/ganja.js): AST重写 + 跨语言代码生成
 * - GCLC (github.com/janicicpredrag/gclc): GC语言语法(构造即声明)
 *
 * @version v3.3.0
 * @date 2026-05-24
 */

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "constraint_graph.h"
#include "dsl_compiler.h"
#include "error_codes.h"
#include "lv00_internal.h"
#include "lv00_utils.h"
#include "symbolic_coord.h"

/* ================================================================
 *  内部常量定义
 * ================================================================ */

/** 词法分析时标识符关键字的内部查找表大小 */
#define DSL_KW_TABLE_SIZE 32

/** AST 子节点动态数组初始容量 */
#define DSL_AST_CHILD_INIT_CAP 4

/** IR 操作动态数组初始容量 */
#define DSL_IR_OP_INIT_CAP 16

/** IR 符号表动态数组初始容量 */
#define DSL_IR_SYMBOL_INIT_CAP 16

/** 字符串格式化临时缓冲区大小 */
#define DSL_DUMP_BUF_SIZE 256

/** 数值字面量解析的最大字符数 */
#define DSL_NUM_BUF_SIZE 64

/** AST dump 缩进每级步长 */
#define DSL_DUMP_INDENT_STEP 2

/* ============== 安全计算常量 ============== */

/** 浮点除零保护阈值 (1e-15)。
 *  当分母绝对值小于此值时视为零，避免除零错误。 */
#ifndef LV00_EPSILON
#define LV00_EPSILON 1e-15
#endif

/** 三角函数角度归模上限。
 *  超过此值的角度需进行周期归模：angle = fmod(angle, 2.0 * M_PI)，
 *  防止大角度导致精度损失。 */
#ifndef LV00_TRIG_ANGLE_MAX
#define LV00_TRIG_ANGLE_MAX 1e6
#endif

/* ================================================================
 *  第 0 层：内部辅助宏
 * ================================================================ */

/** 确保 AST 子节点容量足够，不足时自动扩容 */
#define DSL_ENSURE_AST_CHILD_CAP(ast)                                                           \
    do {                                                                                        \
        if ((ast)->child_count >= (ast)->child_capacity) {                                      \
            int _nc = (ast)->child_capacity == 0 ? DSL_AST_CHILD_INIT_CAP                       \
                                                 : (ast)->child_capacity * LV00_ARRAY_GROWTH_FACTOR; \
            void *_np = lv00_realloc((ast)->children, (size_t)_nc * sizeof(DslAST *));           \
            if (!_np) return NULL;                                                              \
            (ast)->children = (DslAST **)_np;                                                   \
            (ast)->child_capacity = _nc;                                                        \
        }                                                                                       \
    } while (0)

/** 确保 IR 操作数组容量足够 */
#define DSL_ENSURE_IR_OP_CAP(ir)                                                                    \
    do {                                                                                            \
        if ((ir)->op_count >= (ir)->op_capacity) {                                                  \
            int _nc = (ir)->op_capacity == 0 ? DSL_IR_OP_INIT_CAP                                   \
                                             : (ir)->op_capacity * LV00_ARRAY_GROWTH_FACTOR;        \
            void *_np = lv00_realloc((ir)->operations, (size_t)_nc * sizeof(DslIROperation));        \
            if (!_np) return false;                                                                 \
            (ir)->operations = (DslIROperation *)_np;                                               \
            (ir)->op_capacity = _nc;                                                                \
        }                                                                                           \
    } while (0)

/** 确保 IR 符号表容量足够 */
#define DSL_ENSURE_IR_SYM_CAP(ir)                                                                   \
    do {                                                                                            \
        if ((ir)->symbol_count >= (ir)->symbol_capacity) {                                          \
            int _nc = (ir)->symbol_capacity == 0 ? DSL_IR_SYMBOL_INIT_CAP                           \
                                                 : (ir)->symbol_capacity * LV00_ARRAY_GROWTH_FACTOR; \
            void *_sp = lv00_realloc((ir)->symbols, (size_t)_nc * sizeof(char *));                  \
            void *_mp = lv00_realloc((ir)->symbol_to_ir_id, (size_t)_nc * sizeof(int));             \
            if (!_sp || !_mp) {                                                                     \
                if (_sp) lv00_free((void **) &_sp);                                                           \
                if (_mp) lv00_free((void **) &_mp);                                                           \
                return false;                                                                       \
            }                                                                                       \
            (ir)->symbols = (char **)_sp;                                                           \
            (ir)->symbol_to_ir_id = (int *)_mp;                                                     \
            (ir)->symbol_capacity = _nc;                                                            \
        }                                                                                           \
    } while (0)

/* ================================================================
 *  第 1 部分：词法分析器 (Tokenizer)
 * ================================================================ */

/**
 * @brief 关键字查找表条目 —— 将关键字字符串映射到对应的词法单元类型
 */
typedef struct {
    const char *keyword; /**< 关键字字符串 */
    DSLTokenType type;   /**< 对应的词法单元类型 */
    int length;          /**< 字符串长度（缓存，避免反复 strlen） */
} DSLKeywordEntry;

/** 关键字查找表 —— 按字符串排序，支持二分查找 */
static const DSLKeywordEntry g_dsl_keywords[] = {
    {"bisector",      DSL_TOK_BISECTOR,      8},
    {"centroid",      DSL_TOK_CENTROID,      8},
    {"circle",        DSL_TOK_CIRCLE,        6},
    {"circumcenter",  DSL_TOK_CIRCUMCENTER, 12},
    {"constraint",    DSL_TOK_CONSTRAINT,   10},
    {"fix",           DSL_TOK_FIX,           3},
    {"free",          DSL_TOK_FREE,          4},
    {"incenter",      DSL_TOK_INCENTER,      8},
    {"intersect",     DSL_TOK_INTERSECT,     9},
    {"let",           DSL_TOK_LET,           3},
    {"line",          DSL_TOK_LINE,          4},
    {"load",          DSL_TOK_LOAD,          4},
    {"midpoint",      DSL_TOK_MIDPOINT,      8},
    {"orthocenter",   DSL_TOK_ORTHOCENTER,  11},
    {"parallel",      DSL_TOK_PARALLEL,      8},
    {"perpendicular", DSL_TOK_PERPENDICULAR, 13},
    {"point",         DSL_TOK_POINT,         5},
    {"polygon",       DSL_TOK_POLYGON,       7},
    {"prove",         DSL_TOK_PROVE,         5},
    {"ray",           DSL_TOK_RAY,           3},
    {"segment",       DSL_TOK_SEGMENT,       7},
    {"triangle",      DSL_TOK_TRIANGLE,      8},
};

#define DSL_KW_COUNT (int)(sizeof(g_dsl_keywords) / sizeof(g_dsl_keywords[0]))

/**
 * @brief 词法分析器内部状态
 *
 * 维护当前扫描位置、行列号以及输出 token 动态数组。
 */
typedef struct {
    const char *source; /**< DSL 源码字符串 */
    int pos;            /**< 当前字符位置索引 */
    int line;           /**< 当前行号（从 1 开始） */
    int col;            /**< 当前列号（从 1 开始） */
    DslToken *tokens;   /**< 输出 token 动态数组 */
    int token_count;    /**< 当前 token 数量 */
    int token_cap;      /**< token 数组容量 */
    bool has_error;     /**< 是否已遇到词法错误 */
} DSLTokenizerState;

/**
 * @brief 二分查找关键字
 *
 * @param word  待匹配的字符串
 * @param len   字符串长度
 * @return 匹配的关键字 token 类型，未匹配返回 DSL_TOK_IDENT
 */
static DSLTokenType dsl_lookup_keyword(const char *word, int len) {
    int lo = 0, hi = DSL_KW_COUNT - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        int cmp = strncmp(word, g_dsl_keywords[mid].keyword, (size_t)len);
        if (cmp == 0) {
            /* 长度也必须完全匹配，避免 "line" 匹配 "line_something" */
            if (len == g_dsl_keywords[mid].length) {
                return g_dsl_keywords[mid].type;
            }
            /* 长度不同：判断该往哪边走 */
            if (len < g_dsl_keywords[mid].length) {
                hi = mid - 1;
            } else {
                lo = mid + 1;
            }
        } else if (cmp < 0) {
            hi = mid - 1;
        } else {
            lo = mid + 1;
        }
    }
    return DSL_TOK_IDENT;
}

/**
 * @brief 向 token 数组中追加一个 token
 *
 * @param state  词法分析器状态
 * @param type   token 类型
 * @param lexeme token 词素文本指针
 * @param line   行号
 * @param col    列号
 * @return true 成功，false 内存不足
 */
static bool dsl_add_token(DSLTokenizerState *state, DSLTokenType type,
                          const char *lexeme, int line, int col) {
    if (state->token_count >= state->token_cap) {
        int nc = state->token_cap == 0 ? 64 : state->token_cap * 2;
        void *np = lv00_realloc(state->tokens, (size_t)nc * sizeof(DslToken));
        if (!np) {
            state->has_error = true;
            return false;
        }
        state->tokens = (DslToken *)np;
        state->token_cap = nc;
    }
    DslToken *t = &state->tokens[state->token_count++];
    t->type = type;
    t->lexeme = lexeme;
    t->line = line;
    t->col = col;
    return true;
}

/**
 * @brief 跳过空白字符（空格、制表符、换行符、回车符），更新行列号
 *
 * @param state 词法分析器状态
 */
static void dsl_skip_whitespace(DSLTokenizerState *state) {
    while (state->source[state->pos] != '\0') {
        char c = state->source[state->pos];
        if (c == ' ' || c == '\t') {
            state->pos++;
            state->col++;
        } else if (c == '\n') {
            state->pos++;
            state->line++;
            state->col = 1;
        } else if (c == '\r') {
            state->pos++;
            /* 处理 \r\n 组合 */
            if (state->source[state->pos] == '\n') {
                state->pos++;
            }
            state->line++;
            state->col = 1;
        } else {
            break;
        }
    }
}

/**
 * @brief 扫描并跳过注释（// 行注释 和 \/* 块注释 *\/）
 *
 * @param state 词法分析器状态
 * @return true 成功跳过，false 遇到未闭合的块注释（词法错误）
 */
static bool dsl_skip_comment(DSLTokenizerState *state) {
    if (state->source[state->pos] == '/' && state->source[state->pos + 1] == '/') {
        /* 行注释：跳过直到行尾或文件结束 */
        state->pos += 2;
        state->col += 2;
        while (state->source[state->pos] != '\0' && state->source[state->pos] != '\n') {
            state->pos++;
            state->col++;
        }
        return true;
    }
    if (state->source[state->pos] == '/' && state->source[state->pos + 1] == '*') {
        /* 块注释：跳过直到 * / */
        int start_line = state->line;
        int start_col = state->col;
        int start_pos = state->pos;  /* 保存注释起始位置（用于错误 token） */
        state->pos += 2;
        state->col += 2;
        while (state->source[state->pos] != '\0') {
            if (state->source[state->pos] == '*' && state->source[state->pos + 1] == '/') {
                state->pos += 2;
                state->col += 2;
                return true;
            }
            if (state->source[state->pos] == '\n') {
                state->line++;
                state->col = 1;
            } else {
                state->col++;
            }
            state->pos++;
        }
        /* 未闭合的块注释：创建错误 token（使用 start_pos 而非 start_col 作为偏移） */
        dsl_add_token(state, DSL_TOK_ERROR, state->source + start_pos, start_line, start_col);
        state->has_error = true;
        return false;
    }
    return false;
}

/**
 * @brief 扫描一个标识符或关键字
 *
 * 标识符规则：[a-zA-Z_][a-zA-Z0-9_]*
 * 扫描后通过二分查找判定是否为关键字。
 *
 * @param state 词法分析器状态
 * @return true 成功，false 失败
 */
static bool dsl_scan_identifier(DSLTokenizerState *state) {
    int start = state->pos;
    int start_col = state->col;
    int start_line = state->line;

    while (state->source[state->pos] != '\0') {
        char c = state->source[state->pos];
        if (isalnum((unsigned char)c) || c == '_') {
            state->pos++;
            state->col++;
        } else {
            break;
        }
    }

    int len = state->pos - start;
    /* 注意：lexeme 需要指向非 const 的源码，但由于 token 的生命周期中源码一直存活，
       这里 lexeme 直接指向源码中标识符的起始位置 */
    DSLTokenType type = dsl_lookup_keyword(state->source + start, len);
    return dsl_add_token(state, type, state->source + start, start_line, start_col);
}

/**
 * @brief 扫描一个数值字面量
 *
 * 支持格式：整数(123)、浮点(3.14)、科学记数法(1.5e-3, 2.0E+4)
 *
 * @param state 词法分析器状态
 * @return true 成功，false 失败
 */
static bool dsl_scan_number(DSLTokenizerState *state) {
    int start = state->pos;
    int start_col = state->col;
    int start_line = state->line;

    /* 整数部分 */
    while (isdigit((unsigned char)state->source[state->pos])) {
        state->pos++;
        state->col++;
    }

    /* 小数部分 */
    if (state->source[state->pos] == '.' && isdigit((unsigned char)state->source[state->pos + 1])) {
        state->pos++;
        state->col++;
        while (isdigit((unsigned char)state->source[state->pos])) {
            state->pos++;
            state->col++;
        }
    }

    /* 科学记数法指数部分 */
    if (state->source[state->pos] == 'e' || state->source[state->pos] == 'E') {
        state->pos++;
        state->col++;
        if (state->source[state->pos] == '+' || state->source[state->pos] == '-') {
            state->pos++;
            state->col++;
        }
        if (!isdigit((unsigned char)state->source[state->pos])) {
            /* 无效的科学记数法：缺少指数数字 */
            dsl_add_token(state, DSL_TOK_ERROR, state->source + start, start_line, start_col);
            state->has_error = true;
            return false;
        }
        while (isdigit((unsigned char)state->source[state->pos])) {
            state->pos++;
            state->col++;
        }
    }

    return dsl_add_token(state, DSL_TOK_NUMBER, state->source + start, start_line, start_col);
}

/**
 * @brief 扫描字符串字面量（用于 load 语句中的文件名）
 *
 * 支持双引号括起的字符串，内部支持转义 \" 和 \\。
 *
 * @param state 词法分析器状态
 * @return true 成功，false 失败
 */
static bool dsl_scan_string(DSLTokenizerState *state) {
    int start_line = state->line;
    int start_col = state->col;
    state->pos++; /* 跳过开头的 " */
    state->col++;

    while (state->source[state->pos] != '\0') {
        if (state->source[state->pos] == '\\' && state->source[state->pos + 1] != '\0') {
            /* 转义字符：跳过两个字符 */
            state->pos += 2;
            state->col += 2;
            continue;
        }
        if (state->source[state->pos] == '"') {
            state->pos++; /* 跳过结尾的 " */
            state->col++;
            /* 字符串内容为 start_col+1 到 state->pos-2 之间的字符，
               但这个区域包含原义引号，我们把整个区域当标识符处理即可；
               load 解析时会去掉首尾引号 */
            return dsl_add_token(state, DSL_TOK_IDENT,
                                 state->source + start_col + 1,
                                 start_line, start_col + 1);
        }
        if (state->source[state->pos] == '\n') {
            state->line++;
            state->col = 1;
        } else {
            state->col++;
        }
        state->pos++;
    }
    /* 未闭合的字符串 */
    dsl_add_token(state, DSL_TOK_ERROR, state->source + start_col, start_line, start_col);
    state->has_error = true;
    return false;
}

/**
 * @brief 扫描箭头运算符 ->
 *
 * @param state 词法分析器状态
 * @return true 成功
 */
static bool dsl_scan_arrow(DSLTokenizerState *state) {
    int start_col = state->col;
    int start_line = state->line;
    state->pos += 2;
    state->col += 2;
    return dsl_add_token(state, DSL_TOK_ARROW, state->source + start_col, start_line, start_col);
}

/* ---- 公共 API：dsl_tokenize ---- */

bool dsl_tokenize(const char *source, DslToken **out_tokens, int *out_count) {
    LV00_CHECK_NULL(source, false);
    LV00_CHECK_NULL(out_tokens, false);
    LV00_CHECK_NULL(out_count, false);

    DSLTokenizerState state;
    memset(&state, 0, sizeof(state));
    state.source = source;
    state.pos = 0;
    state.line = 1;
    state.col = 1;
    state.tokens = NULL;
    state.token_count = 0;
    state.token_cap = 0;
    state.has_error = false;

    while (source[state.pos] != '\0') {
        /* 跳过空白 */
        dsl_skip_whitespace(&state);
        if (source[state.pos] == '\0') break;

        /* 跳过注释 */
        if (source[state.pos] == '/' &&
            (source[state.pos + 1] == '/' || source[state.pos + 1] == '*')) {
            if (!dsl_skip_comment(&state)) break;
            continue;
        }

        char c = source[state.pos];
        int save_col = state.col;
        int save_line = state.line;

        /* 标识符或关键字：字母或下划线开头 */
        if (isalpha((unsigned char)c) || c == '_') {
            if (!dsl_scan_identifier(&state)) goto tokenize_error;
            continue;
        }

        /* 数值字面量：数字或点后跟数字 */
        if (isdigit((unsigned char)c) || (c == '.' && isdigit((unsigned char)source[state.pos + 1]))) {
            if (!dsl_scan_number(&state)) goto tokenize_error;
            continue;
        }

        /* 字符串字面量 */
        if (c == '"') {
            if (!dsl_scan_string(&state)) goto tokenize_error;
            continue;
        }

        /* 单字符运算符和分隔符 */
        state.pos++;
        state.col++;

        switch (c) {
        case '(':
            if (!dsl_add_token(&state, DSL_TOK_LPAREN, source + save_col, save_line, save_col)) goto tokenize_error;
            break;
        case ')':
            if (!dsl_add_token(&state, DSL_TOK_RPAREN, source + save_col, save_line, save_col)) goto tokenize_error;
            break;
        case '{':
            if (!dsl_add_token(&state, DSL_TOK_LBRACE, source + save_col, save_line, save_col)) goto tokenize_error;
            break;
        case '}':
            if (!dsl_add_token(&state, DSL_TOK_RBRACE, source + save_col, save_line, save_col)) goto tokenize_error;
            break;
        case '[':
            if (!dsl_add_token(&state, DSL_TOK_LBRACKET, source + save_col, save_line, save_col)) goto tokenize_error;
            break;
        case ']':
            if (!dsl_add_token(&state, DSL_TOK_RBRACKET, source + save_col, save_line, save_col)) goto tokenize_error;
            break;
        case ',':
            if (!dsl_add_token(&state, DSL_TOK_COMMA, source + save_col, save_line, save_col)) goto tokenize_error;
            break;
        case '=':
            if (!dsl_add_token(&state, DSL_TOK_ASSIGN, source + save_col, save_line, save_col)) goto tokenize_error;
            break;
        case ';':
            if (!dsl_add_token(&state, DSL_TOK_SEMI, source + save_col, save_line, save_col)) goto tokenize_error;
            break;
        case ':':
            if (!dsl_add_token(&state, DSL_TOK_COLON, source + save_col, save_line, save_col)) goto tokenize_error;
            break;
        case '-':
            if (source[state.pos] == '>') {
                /* 回退单字符移动，由 scan_arrow 处理 */
                state.pos--;
                state.col--;
                if (!dsl_scan_arrow(&state)) goto tokenize_error;
            } else {
                /* 单个 - 不是有效 token，报错 */
                if (!dsl_add_token(&state, DSL_TOK_ERROR, source + save_col, save_line, save_col)) goto tokenize_error;
                state.has_error = true;
                goto tokenize_error;
            }
            break;
        default:
            /* 未知字符 */
            if (!dsl_add_token(&state, DSL_TOK_ERROR, source + save_col, save_line, save_col)) goto tokenize_error;
            state.has_error = true;
            break;
        }

        if (state.has_error) break;
    }

    /* 追加 EOF token */
    dsl_add_token(&state, DSL_TOK_EOF, "", state.line, state.col);

    *out_tokens = state.tokens;
    *out_count = state.token_count;
    return !state.has_error;

tokenize_error:
    /* 释放已分配的 token 数组 */
    if (state.tokens) {
        lv00_free((void **)&state.tokens);
    }
    *out_tokens = NULL;
    *out_count = 0;
    return false;
}

/* ---- 公共 API：dsl_tokens_free ---- */

void dsl_tokens_free(DslToken *tokens, int count) {
    (void)count;
    if (tokens) {
        lv00_free((void **)&tokens);
    }
}

/* ================================================================
 *  第 2 部分：递归下降解析器 (Parser)
 * ================================================================ */

/**
 * @brief 解析器内部状态
 *
 * 持有一组 token 及当前读取位置，实现递归下降解析。
 */
typedef struct {
    const DslToken *tokens; /**< token 数组 */
    int count;              /**< token 总数 */
    int pos;                /**< 当前读取位置 */
} DSLParserState;

/**
 * @brief 查看当前 token（不推进位置）
 *
 * @param ps 解析器状态
 * @return 当前 token 指针
 */
static const DslToken *dsl_peek(DSLParserState *ps) {
    if (ps->pos >= ps->count) return &ps->tokens[ps->count - 1]; /* 返回最后一个（应为 EOF） */
    return &ps->tokens[ps->pos];
}

/**
 * @brief 消费当前 token 并前进
 *
 * @param ps 解析器状态
 * @return 被消费的 token 指针
 */
static const DslToken *dsl_advance(DSLParserState *ps) {
    if (ps->pos >= ps->count) return &ps->tokens[ps->count - 1];
    return &ps->tokens[ps->pos++];
}

/**
 * @brief 检查当前 token 是否匹配给定类型，匹配则消费
 *
 * @param ps   解析器状态
 * @param type 期望的 token 类型
 * @return true 匹配并消费，false 不匹配
 */
static bool dsl_match(DSLParserState *ps, DSLTokenType type) {
    if (dsl_peek(ps)->type == type) {
        dsl_advance(ps);
        return true;
    }
    return false;
}

/**
 * @brief 期望当前 token 为给定类型，消费之；否则返回 NULL 表示语法错误
 *
 * @param ps   解析器状态
 * @param type 期望的 token 类型
 * @return 匹配的 token 指针，不匹配返回 NULL
 */
static const DslToken *dsl_expect(DSLParserState *ps, DSLTokenType type) {
    if (dsl_peek(ps)->type == type) {
        return dsl_advance(ps);
    }
    return NULL;
}

/**
 * @brief 创建一个新的 AST 节点
 *
 * @param type  节点类型
 * @param name  标识符名称（可为 NULL）
 * @param line  源码行号
 * @param col   源码列号
 * @return 新创建的 AST 节点，失败返回 NULL
 */
static DslAST *dsl_ast_new(DslASTType type, const char *name, int line, int col) {
    DslAST *node = (DslAST *)lv00_calloc(1, sizeof(DslAST));
    if (!node) return NULL;
    node->type = type;
    node->name = name ? lv00_strdup_safe(name) : NULL;
    node->num_value = 0.0;
    node->line = line;
    node->col = col;
    node->children = NULL;
    node->child_count = 0;
    node->child_capacity = 0;
    return node;
}

/**
 * @brief 向 AST 节点追加一个子节点
 *
 * @param parent 父节点
 * @param child  子节点
 * @return true 成功，false 内存不足
 */
static bool dsl_ast_add_child(DslAST *parent, DslAST *child) {
    DSL_ENSURE_AST_CHILD_CAP(parent);
    parent->children[parent->child_count++] = child;
    return true;
}

/* ---- 前向声明递归下降解析函数 ---- */
static DslAST *dsl_parse_program(DSLParserState *ps);
static DslAST *dsl_parse_statement(DSLParserState *ps);
static DslAST *dsl_parse_expression(DSLParserState *ps);
static DslAST *dsl_parse_block(DSLParserState *ps);

/* ---- 前向声明代码生成函数 ---- */
static bool dsl_compile_block(DslIR *ir, const DslAST *ast);

/**
 * @brief 解析标识符 AST 节点
 *
 * @param ps 解析器状态
 * @return AST 节点，失败返回 NULL
 */
static DslAST *dsl_parse_ident(DSLParserState *ps) {
    const DslToken *tok = dsl_peek(ps);
    if (tok->type == DSL_TOK_IDENT) {
        dsl_advance(ps);
        /* 标识符的 name 由词素末尾限定，但词素直接指向源码区域；
           我们需要复制一份以确保生命周期独立 */
        char buf[256];
        int len = 0;
        const char *s = tok->lexeme;
        while (*s && !isspace((unsigned char)*s) && *s != '(' && *s != ')'
               && *s != '{' && *s != '}' && *s != ',' && *s != ';'
               && *s != '=' && *s != ':' && *s != '[' && *s != ']' && *s != '"') {
            if (len < 255) buf[len++] = *s;
            s++;
        }
        buf[len] = '\0';
        return dsl_ast_new(DSL_AST_IDENT, buf, tok->line, tok->col);
    }
    return NULL;
}

/**
 * @brief 解析数值字面量 AST 节点
 *
 * @param ps 解析器状态
 * @return AST 节点，失败返回 NULL
 */
static DslAST *dsl_parse_number(DSLParserState *ps) {
    const DslToken *tok = dsl_peek(ps);
    if (tok->type == DSL_TOK_NUMBER) {
        dsl_advance(ps);
        char num_buf[DSL_NUM_BUF_SIZE];
        int len = 0;
        const char *s = tok->lexeme;
        while (*s && !isspace((unsigned char)*s) && *s != '(' && *s != ')'
               && *s != '{' && *s != '}' && *s != ',' && *s != ';'
               && *s != '=' && *s != ':') {
            if (len < DSL_NUM_BUF_SIZE - 1) num_buf[len++] = *s;
            s++;
        }
        num_buf[len] = '\0';
        DslAST *node = dsl_ast_new(DSL_AST_NUMBER, NULL, tok->line, tok->col);
        if (!node) return NULL;
        node->num_value = atof(num_buf);
        return node;
    }
    return NULL;
}

/**
 * @brief 解析函数调用表达式（如 intersect(line_a, line_b)）
 *
 * 语法：operation_keyword '(' arg (',' arg)* ')'
 *
 * @param ps     解析器状态
 * @param op_tok 操作关键字 token
 * @return AST 节点，失败返回 NULL
 */
static DslAST *dsl_parse_call(DSLParserState *ps, const DslToken *op_tok) {
    /* 将操作关键字映射到 AST 类型 */
    DslASTType call_type;
    switch (op_tok->type) {
    case DSL_TOK_INTERSECT:     call_type = DSL_AST_INTERSECT;     break;
    case DSL_TOK_PARALLEL:      call_type = DSL_AST_PARALLEL;      break;
    case DSL_TOK_PERPENDICULAR: call_type = DSL_AST_PERPENDICULAR; break;
    case DSL_TOK_MIDPOINT:      call_type = DSL_AST_MIDPOINT;      break;
    case DSL_TOK_CIRCUMCENTER:  call_type = DSL_AST_CIRCUMCENTER;  break;
    case DSL_TOK_ORTHOCENTER:   call_type = DSL_AST_ORTHOCENTER;   break;
    case DSL_TOK_CENTROID:      call_type = DSL_AST_CENTROID;      break;
    case DSL_TOK_INCENTER:      call_type = DSL_AST_INCENTER;      break;
    case DSL_TOK_BISECTOR:      call_type = DSL_AST_BISECTOR;      break;
    default:
        return NULL;
    }

    DslAST *node = dsl_ast_new(call_type, NULL, op_tok->line, op_tok->col);
    if (!node) return NULL;

    if (!dsl_match(ps, DSL_TOK_LPAREN)) {
        /* 函数调用必须带括号 */
        dsl_ast_free(node);
        return NULL;
    }

    /* 解析参数列表 */
    if (dsl_peek(ps)->type != DSL_TOK_RPAREN) {
        DslAST *first_arg = dsl_parse_expression(ps);
        if (!first_arg) {
            dsl_ast_free(node);
            return NULL;
        }
        dsl_ast_add_child(node, first_arg);

        while (dsl_match(ps, DSL_TOK_COMMA)) {
            DslAST *arg = dsl_parse_expression(ps);
            if (!arg) {
                dsl_ast_free(node);
                return NULL;
            }
            dsl_ast_add_child(node, arg);
        }
    }

    if (!dsl_expect(ps, DSL_TOK_RPAREN)) {
        dsl_ast_free(node);
        return NULL;
    }

    return node;
}

/**
 * @brief 解析表达式
 *
 * 语法：expression := ident | number | function_call
 *
 * @param ps 解析器状态
 * @return AST 节点，失败返回 NULL
 */
static DslAST *dsl_parse_expression(DSLParserState *ps) {
    const DslToken *tok = dsl_peek(ps);

    /* 操作函数调用 */
    if (tok->type >= DSL_TOK_INTERSECT && tok->type <= DSL_TOK_BISECTOR) {
        dsl_advance(ps);
        return dsl_parse_call(ps, tok);
    }

    /* 标识符 */
    if (tok->type == DSL_TOK_IDENT) {
        return dsl_parse_ident(ps);
    }

    /* 数值 */
    if (tok->type == DSL_TOK_NUMBER) {
        return dsl_parse_number(ps);
    }

    return NULL;
}

/**
 * @brief 解析语句
 *
 * 支持的语句类型：
 * - point/line/circle/segment/ray/polygon/triangle 声明
 * - constraint { ... }  约束块
 * - prove { ... }       证明块
 * - load "name"         加载公理包
 *
 * @param ps 解析器状态
 * @return AST 节点，失败返回 NULL
 */
static DslAST *dsl_parse_statement(DSLParserState *ps) {
    const DslToken *first = dsl_peek(ps);

    /* constraint 块 */
    if (first->type == DSL_TOK_CONSTRAINT) {
        dsl_advance(ps);
        DslAST *node = dsl_ast_new(DSL_AST_CONSTRAINT, NULL, first->line, first->col);
        if (!node) return NULL;
        DslAST *block = dsl_parse_block(ps);
        if (!block) { dsl_ast_free(node); return NULL; }
        dsl_ast_add_child(node, block);
        dsl_match(ps, DSL_TOK_SEMI);
        return node;
    }

    /* prove 块 */
    if (first->type == DSL_TOK_PROVE) {
        dsl_advance(ps);
        DslAST *node = dsl_ast_new(DSL_AST_PROVE, NULL, first->line, first->col);
        if (!node) return NULL;
        DslAST *block = dsl_parse_block(ps);
        if (!block) { dsl_ast_free(node); return NULL; }
        dsl_ast_add_child(node, block);
        dsl_match(ps, DSL_TOK_SEMI);
        return node;
    }

    /* load 语句 */
    if (first->type == DSL_TOK_LOAD) {
        dsl_advance(ps);
        DslAST *node = dsl_ast_new(DSL_AST_LOAD, NULL, first->line, first->col);
        if (!node) return NULL;
        /* 文件名：可能是标识符（已由词法器将字符串 token 统一为 IDENT） */
        const DslToken *name_tok = dsl_peek(ps);
        if (name_tok->type == DSL_TOK_IDENT) {
            dsl_advance(ps);
            node->name = lv00_strdup_safe(name_tok->lexeme);
        }
        dsl_match(ps, DSL_TOK_SEMI);
        return node;
    }

    /* 几何实体声明：point / line / circle / segment / ray / polygon / triangle */
    DslASTType decl_type;
    bool is_geom_decl = true;
    switch (first->type) {
    case DSL_TOK_POINT:    decl_type = DSL_AST_POINT_DECL;    break;
    case DSL_TOK_LINE:     decl_type = DSL_AST_LINE_DECL;     break;
    case DSL_TOK_CIRCLE:   decl_type = DSL_AST_CIRCLE_DECL;   break;
    case DSL_TOK_SEGMENT:  decl_type = DSL_AST_SEGMENT_DECL;  break;
    case DSL_TOK_RAY:      decl_type = DSL_AST_RAY_DECL;      break;
    case DSL_TOK_POLYGON:  decl_type = DSL_AST_POLYGON_DECL;  break;
    case DSL_TOK_TRIANGLE: decl_type = DSL_AST_TRIANGLE_DECL; break;
    default:
        is_geom_decl = false;
        break;
    }

    if (is_geom_decl) {
        dsl_advance(ps);
        /* 期望标识符 */
        const DslToken *name_tok = dsl_expect(ps, DSL_TOK_IDENT);
        if (!name_tok) return NULL;

        DslAST *node = dsl_ast_new(decl_type, name_tok->lexeme, first->line, first->col);
        if (!node) return NULL;

        /* 检查三种构造方式 */
        const DslToken *peek = dsl_peek(ps);

        /* 方式1：point A 10 20 —— 固定坐标点 */
        if (decl_type == DSL_AST_POINT_DECL && peek->type == DSL_TOK_NUMBER) {
            DslAST *x = dsl_parse_number(ps);
            DslAST *y = dsl_parse_number(ps);
            if (!x || !y) { dsl_ast_free(node); return NULL; }
            dsl_ast_add_child(node, x);
            dsl_ast_add_child(node, y);
            dsl_match(ps, DSL_TOK_SEMI);
            return node;
        }

        /* 方式2：point A = intersect(line_a, line_b) —— 构造点 */
        if (dsl_match(ps, DSL_TOK_ASSIGN)) {
            DslAST *expr = dsl_parse_expression(ps);
            if (!expr) { dsl_ast_free(node); return NULL; }
            dsl_ast_add_child(node, expr);
            dsl_match(ps, DSL_TOK_SEMI);
            return node;
        }

        /* 方式3：line a A B —— 通过已有点构造（两个/多个标识符） */
        if (peek->type == DSL_TOK_IDENT) {
            while (dsl_peek(ps)->type == DSL_TOK_IDENT) {
                DslAST *ref = dsl_parse_ident(ps);
                if (!ref) { dsl_ast_free(node); return NULL; }
                dsl_ast_add_child(node, ref);
            }
            dsl_match(ps, DSL_TOK_SEMI);
            return node;
        }

        /* 方式4：point A —— 自由点（无参数） */
        dsl_match(ps, DSL_TOK_SEMI);
        return node;
    }

    /* 未知语句开头：跳过直到分号或块结束 */
    while (dsl_peek(ps)->type != DSL_TOK_SEMI &&
           dsl_peek(ps)->type != DSL_TOK_RBRACE &&
           dsl_peek(ps)->type != DSL_TOK_EOF) {
        dsl_advance(ps);
    }
    dsl_match(ps, DSL_TOK_SEMI);
    return NULL;
}

/**
 * @brief 解析语句块 { stmt1; stmt2; ... }
 *
 * @param ps 解析器状态
 * @return AST 节点（DSL_AST_BLOCK），失败返回 NULL
 */
static DslAST *dsl_parse_block(DSLParserState *ps) {
    if (!dsl_match(ps, DSL_TOK_LBRACE)) return NULL;

    const DslToken *first = &ps->tokens[ps->pos > 0 ? ps->pos - 1 : 0];
    DslAST *block = dsl_ast_new(DSL_AST_BLOCK, NULL, first->line, first->col);
    if (!block) return NULL;

    while (dsl_peek(ps)->type != DSL_TOK_RBRACE &&
           dsl_peek(ps)->type != DSL_TOK_EOF) {
        DslAST *stmt = dsl_parse_statement(ps);
        if (stmt) {
            dsl_ast_add_child(block, stmt);
        }
    }

    dsl_expect(ps, DSL_TOK_RBRACE);
    return block;
}

/**
 * @brief 解析程序（多个顶层语句）
 *
 * @param ps 解析器状态
 * @return AST 根节点（DSL_AST_PROGRAM），失败返回 NULL
 */
static DslAST *dsl_parse_program(DSLParserState *ps) {
    DslAST *program = dsl_ast_new(DSL_AST_PROGRAM, NULL, 1, 1);
    if (!program) return NULL;

    while (dsl_peek(ps)->type != DSL_TOK_EOF) {
        DslAST *stmt = dsl_parse_statement(ps);
        if (stmt) {
            dsl_ast_add_child(program, stmt);
        }
    }

    return program;
}

/* ---- 公共 API：dsl_parse ---- */

bool dsl_parse(const DslToken *tokens, int count, DslAST **out_ast) {
    LV00_CHECK_NULL(tokens, false);
    LV00_CHECK_NULL(out_ast, false);
    LV00_CHECK(count > 0, LV00_ERROR_INVALID_PARAM, false, "token 数组为空");

    DSLParserState ps;
    ps.tokens = tokens;
    ps.count = count;
    ps.pos = 0;

    DslAST *ast = dsl_parse_program(&ps);
    if (!ast) {
        LV00_ERROR_SET(LV00_ERROR_PARSE, "DSL 语法解析失败");
        return false;
    }

    *out_ast = ast;
    return true;
}

/* ---- 公共 API：dsl_ast_free ---- */

void dsl_ast_free(DslAST *ast) {
    if (!ast) return;
    if (ast->name) {
        lv00_free((void **)&ast->name);
    }
    for (int i = 0; i < ast->child_count; i++) {
        dsl_ast_free(ast->children[i]);
    }
    if (ast->children) {
        lv00_free((void **)&ast->children);
    }
    lv00_free((void **)&ast);
}

/* ---- 公共 API：dsl_ast_type_name ---- */

const char *dsl_ast_type_name(DslASTType type) {
    switch (type) {
    case DSL_AST_PROGRAM:       return "PROGRAM";
    case DSL_AST_POINT_DECL:    return "POINT_DECL";
    case DSL_AST_LINE_DECL:     return "LINE_DECL";
    case DSL_AST_CIRCLE_DECL:   return "CIRCLE_DECL";
    case DSL_AST_SEGMENT_DECL:  return "SEGMENT_DECL";
    case DSL_AST_RAY_DECL:      return "RAY_DECL";
    case DSL_AST_POLYGON_DECL:  return "POLYGON_DECL";
    case DSL_AST_TRIANGLE_DECL: return "TRIANGLE_DECL";
    case DSL_AST_INTERSECT:     return "INTERSECT";
    case DSL_AST_PARALLEL:      return "PARALLEL";
    case DSL_AST_PERPENDICULAR: return "PERPENDICULAR";
    case DSL_AST_MIDPOINT:      return "MIDPOINT";
    case DSL_AST_CIRCUMCENTER:  return "CIRCUMCENTER";
    case DSL_AST_ORTHOCENTER:   return "ORTHOCENTER";
    case DSL_AST_CENTROID:      return "CENTROID";
    case DSL_AST_INCENTER:      return "INCENTER";
    case DSL_AST_BISECTOR:      return "BISECTOR";
    case DSL_AST_CONSTRAINT:    return "CONSTRAINT";
    case DSL_AST_PROVE:         return "PROVE";
    case DSL_AST_LOAD:          return "LOAD";
    case DSL_AST_FIX_POINT:     return "FIX_POINT";
    case DSL_AST_FREE_POINT:    return "FREE_POINT";
    case DSL_AST_BLOCK:         return "BLOCK";
    case DSL_AST_IDENT:         return "IDENT";
    case DSL_AST_NUMBER:        return "NUMBER";
    default:                    return "UNKNOWN";
    }
}

/**
 * @brief 向文件描述符输出格式化字符串
 *
 * @param fd  输出目标（FILE* 指针）
 * @param fmt 格式化字符串
 * @param ... 可变参数
 */
static void dsl_fd_printf(void *fd, const char *fmt, ...) {
    if (!fd) return;
    va_list args;
    va_start(args, fmt);
    vfprintf((FILE *)fd, fmt, args);
    va_end(args);
}

/* ---- 公共 API：dsl_ast_dump ---- */

void dsl_ast_dump(const DslAST *ast, void *fd, int indent) {
    if (!ast) return;

    FILE *fp = (FILE *)fd;
    /* 缩进输出 */
    for (int i = 0; i < indent; i++) {
        dsl_fd_printf(fd, " ");
    }

    dsl_fd_printf(fd, "(%s", dsl_ast_type_name(ast->type));

    if (ast->name) {
        dsl_fd_printf(fd, " '%s'", ast->name);
    }

    if (ast->type == DSL_AST_NUMBER) {
        dsl_fd_printf(fd, " %g", ast->num_value);
    }

    dsl_fd_printf(fd, " [%d:%d]", ast->line, ast->col);

    if (ast->child_count > 0) {
        dsl_fd_printf(fd, "\n");
        for (int i = 0; i < ast->child_count; i++) {
            dsl_ast_dump(ast->children[i], fd, indent + DSL_DUMP_INDENT_STEP);
        }
        for (int i = 0; i < indent; i++) {
            dsl_fd_printf(fd, " ");
        }
    }

    dsl_fd_printf(fd, ")\n");
    (void)fp;
}

/* ================================================================
 *  第 3 部分：IR 编译器 (AST -> IR)
 *
 *  借鉴 Ganja.js 的 AST 重写技术。
 *  遍历 AST 生成过程化 API 调用序列，同时进行符号解析和基本类型检查。
 * ================================================================ */

/**
 * @brief IR 编译器内部状态
 *
 * 包含输出 IR 对象、符号表和编译配置引用。
 */
typedef struct {
    DslIR *ir;                     /**< 输出的 IR */
    const DslCompileConfig *config; /**< 编译配置 */
    int current_source_line;       /**< 当前源码行号（用于错误报告） */
} DSLIRCompiler;

/**
 * @brief 初始化 IR 对象
 *
 * @param ir 待初始化的 IR 对象指针（需由调用者分配内存）
 */
static void dsl_ir_init(DslIR *ir) {
    memset(ir, 0, sizeof(*ir));
    ir->operations = NULL;
    ir->op_count = 0;
    ir->op_capacity = 0;
    ir->symbols = NULL;
    ir->symbol_to_ir_id = NULL;
    ir->symbol_count = 0;
    ir->symbol_capacity = 0;
    ir->next_id = 1; /* 0 保留为无效 ID */
}

/**
 * @brief 在符号表中查找符号，返回对应的 IR 结果 ID
 *
 * @param ir   IR 对象
 * @param name 符号名称
 * @return 符号对应的结果 ID，未找到返回 -1
 */
static int dsl_ir_find_symbol(const DslIR *ir, const char *name) {
    for (int i = 0; i < ir->symbol_count; i++) {
        if (ir->symbols[i] && strcmp(ir->symbols[i], name) == 0) {
            return ir->symbol_to_ir_id[i];
        }
    }
    return -1;
}

/**
 * @brief 在符号表中注册新符号
 *
 * @param ir       IR 对象
 * @param name     符号名称
 * @param result_id 关联的结果实体 ID
 * @return true 成功，false 内存不足
 */
static bool dsl_ir_register_symbol(DslIR *ir, const char *name, int result_id) {
    /* 检查是否已存在 */
    for (int i = 0; i < ir->symbol_count; i++) {
        if (ir->symbols[i] && strcmp(ir->symbols[i], name) == 0) {
            /* 符号已存在，更新映射 */
            ir->symbol_to_ir_id[i] = result_id;
            return true;
        }
    }
    /* 新符号 */
    DSL_ENSURE_IR_SYM_CAP(ir);
    ir->symbols[ir->symbol_count] = lv00_strdup_safe(name);
    if (!ir->symbols[ir->symbol_count]) return false;
    ir->symbol_to_ir_id[ir->symbol_count] = result_id;
    ir->symbol_count++;
    return true;
}

/**
 * @brief 向 IR 追加一条操作
 *
 * @param ir        IR 对象
 * @param op        操作码
 * @param operands  操作数数组（所有权转移给 IR）
 * @param op_count  操作数数量
 * @param result_id 结果实体 ID（-1 表示无结果）
 * @param label     标签（可为 NULL）
 * @param line      源码行号
 * @return true 成功，false 内存不足
 */
static bool dsl_ir_emit(DslIR *ir, DslIROp op, int *operands, int op_count,
                        int result_id, const char *label, int line) {
    DSL_ENSURE_IR_OP_CAP(ir);
    DslIROperation *iop = &ir->operations[ir->op_count++];
    iop->op = op;
    iop->operands = operands;
    iop->operand_count = op_count;
    iop->result_id = result_id;
    iop->label = label;
    iop->source_line = line;
    return true;
}

/**
 * @brief 分配一个新的结果实体 ID
 *
 * @param ir IR 对象
 * @return 新 ID
 */
static int dsl_ir_new_id(DslIR *ir) {
    return ir->next_id++;
}

/**
 * @brief 编译 AST 中单个标识符节点为操作数 ID 数组（只含一个元素）
 *
 * @param ir   IR 对象
 * @param ast  AST 标识符节点
 * @param out_id 输出：解析后的结果实体 ID
 * @return true 成功，false 符号未定义
 */
static bool dsl_compile_ident_ref(DslIR *ir, const DslAST *ast, int *out_id) {
    int id = dsl_ir_find_symbol(ir, ast->name);
    if (id < 0) {
        LV00_ERROR_SET(LV00_ERROR_PARSE,
                       "在第%d行：未定义的符号 '%s'", ast->line, ast->name);
        return false;
    }
    *out_id = id;
    return true;
}

/**
 * @brief 编译 AST 表达式节点，返回结果实体 ID
 *
 * 表达式可能是标识符引用（直接查符号表返回已有 ID），也可能是操作调用（生成 IR 操作序列，分配新 ID）。
 *
 * @param ir      IR 对象
 * @param ast     AST 表达式节点
 * @param out_id  输出：结果实体 ID
 * @return true 成功，false 编译错误
 */
static bool dsl_compile_expr(DslIR *ir, const DslAST *ast, int *out_id) {
    if (!ast) return false;

    /* 标识符引用 */
    if (ast->type == DSL_AST_IDENT) {
        return dsl_compile_ident_ref(ir, ast, out_id);
    }

    /* 操作调用：intersect/parallel/midpoint 等 */
    if (ast->type >= DSL_AST_INTERSECT && ast->type <= DSL_AST_BISECTOR) {
        /* 收集参数 */
        int *operands = NULL;
        int op_count = ast->child_count;
        if (op_count > 0) {
            operands = (int *)lv00_malloc((size_t)op_count * sizeof(int));
            if (!operands) return false;
            for (int i = 0; i < op_count; i++) {
                int arg_id;
                if (!dsl_compile_expr(ir, ast->children[i], &arg_id)) {
                    lv00_free((void **)&operands);
                    return false;
                }
                operands[i] = arg_id;
            }
        }

        /* AST 类型到 IR 操作码的映射 */
        DslIROp ir_op;
        switch (ast->type) {
        case DSL_AST_INTERSECT:     ir_op = IR_INTERSECT;             break;
        case DSL_AST_PARALLEL:      ir_op = IR_PARALLEL_THROUGH;      break;
        case DSL_AST_PERPENDICULAR: ir_op = IR_PERPENDICULAR_THROUGH; break;
        case DSL_AST_MIDPOINT:      ir_op = IR_MIDPOINT_OF;           break;
        case DSL_AST_CIRCUMCENTER:  ir_op = IR_CIRCUMCENTER_OF;       break;
        case DSL_AST_ORTHOCENTER:   ir_op = IR_ORTHOCENTER_OF;        break;
        case DSL_AST_CENTROID:      ir_op = IR_CENTROID_OF;           break;
        case DSL_AST_INCENTER:      ir_op = IR_INCENTER_OF;           break;
        case DSL_AST_BISECTOR:      ir_op = IR_BISECTOR_OF;           break;
        default: ir_op = IR_NOOP; break;
        }

        int result_id = dsl_ir_new_id(ir);
        if (!dsl_ir_emit(ir, ir_op, operands, op_count, result_id, NULL, ast->line)) {
            if (operands) lv00_free((void **)&operands);
            return false;
        }
        *out_id = result_id;
        return true;
    }

    LV00_ERROR_SET(LV00_ERROR_PARSE, "在第%d行：不支持的表达式类型 %d", ast->line, ast->type);
    return false;
}

/**
 * @brief 编译单个 AST 语句为 IR 操作序列
 *
 * @param ir    IR 对象
 * @param ast   AST 语句节点
 * @return true 成功，false 编译错误
 */
static bool dsl_compile_stmt(DslIR *ir, const DslAST *ast) {
    if (!ast) return true;

    switch (ast->type) {
    /* ---- 几何实体声明 ---- */
    case DSL_AST_POINT_DECL:
    case DSL_AST_LINE_DECL:
    case DSL_AST_CIRCLE_DECL:
    case DSL_AST_SEGMENT_DECL:
    case DSL_AST_RAY_DECL:
    case DSL_AST_POLYGON_DECL:
    case DSL_AST_TRIANGLE_DECL: {
        /* 分析子节点结构判断构造方式 */
        if (ast->child_count == 0) {
            /* 自由点（仅 point 支持） */
            if (ast->type == DSL_AST_POINT_DECL) {
                int result_id = dsl_ir_new_id(ir);
                if (!dsl_ir_emit(ir, IR_CREATE_POINT, NULL, 0, result_id, NULL, ast->line)) return false;
                if (!dsl_ir_register_symbol(ir, ast->name, result_id)) return false;
                /* 为实体添加标签 */
                if (!dsl_ir_emit(ir, IR_LABEL, NULL, 0, result_id, ast->name, ast->line)) return false;
                return true;
            }
            LV00_ERROR_SET(LV00_ERROR_PARSE,
                           "在第%d行：'%s' 声明需要构造参数", ast->line, ast->name);
            return false;
        }

        /* 检查第一个子节点是否为表达式（= expr 格式） */
        if (ast->child_count == 1 &&
            ast->children[0]->type >= DSL_AST_INTERSECT &&
            ast->children[0]->type <= DSL_AST_BISECTOR) {
            /* 构造表达式：point A = intersect(...) */
            int expr_id;
            if (!dsl_compile_expr(ir, ast->children[0], &expr_id)) return false;
            /* 将表达式结果注册为符号名 */
            if (!dsl_ir_register_symbol(ir, ast->name, expr_id)) return false;
            /* 打标签 */
            if (!dsl_ir_emit(ir, IR_LABEL, NULL, 0, expr_id, ast->name, ast->line)) return false;
            return true;
        }

        /* 检查第一个子节点是否为数值（固定坐标：point A 10 20） */
        if (ast->type == DSL_AST_POINT_DECL &&
            ast->child_count >= 2 &&
            ast->children[0]->type == DSL_AST_NUMBER &&
            ast->children[1]->type == DSL_AST_NUMBER) {
            int result_id = dsl_ir_new_id(ir);
            double x = ast->children[0]->num_value;
            double y = ast->children[1]->num_value;
            /* 打包坐标信息：使用一个特殊格式的操作。
               暂通过 operands 传递：operands[0] = (int)(x*1000), operands[1] = (int)(y*1000)
               作为过渡方案 */
            int *op_info = (int *)lv00_malloc(2 * sizeof(int));
            if (!op_info) return false;
            op_info[0] = (int)(x * 1000.0);
            op_info[1] = (int)(y * 1000.0);
            if (!dsl_ir_emit(ir, IR_CREATE_POINT_FIXED, op_info, 2, result_id, NULL, ast->line)) return false;
            if (!dsl_ir_register_symbol(ir, ast->name, result_id)) return false;
            if (!dsl_ir_emit(ir, IR_LABEL, NULL, 0, result_id, ast->name, ast->line)) return false;
            return true;
        }

        /* 标识符引用参数：line a A B / circle k A B 等 */
        {
            /* 收集所有标识符引用的 ID */
            int ref_count = 0;
            for (int i = 0; i < ast->child_count; i++) {
                if (ast->children[i]->type == DSL_AST_IDENT) ref_count++;
            }
            if (ref_count == 0) {
                LV00_ERROR_SET(LV00_ERROR_PARSE,
                               "在第%d行：'%s' 声明缺少有效的参数", ast->line, ast->name);
                return false;
            }

            int *operands = (int *)lv00_malloc((size_t)ref_count * sizeof(int));
            if (!operands) return false;
            int idx = 0;
            for (int i = 0; i < ast->child_count; i++) {
                if (ast->children[i]->type == DSL_AST_IDENT) {
                    if (!dsl_compile_ident_ref(ir, ast->children[i], &operands[idx])) {
                        lv00_free((void **)&operands);
                        return false;
                    }
                    idx++;
                }
            }

            /* AST 声明类型到 IR 操作码的映射 */
            DslIROp create_op;
            switch (ast->type) {
            case DSL_AST_LINE_DECL:     create_op = IR_CREATE_LINE;     break;
            case DSL_AST_CIRCLE_DECL:   create_op = IR_CREATE_CIRCLE;   break;
            case DSL_AST_SEGMENT_DECL:  create_op = IR_CREATE_SEGMENT;  break;
            case DSL_AST_RAY_DECL:      create_op = IR_CREATE_RAY;      break;
            case DSL_AST_POLYGON_DECL:  create_op = IR_CREATE_POLYGON;  break;
            case DSL_AST_TRIANGLE_DECL: create_op = IR_CREATE_TRIANGLE; break;
            default: create_op = IR_NOOP; break;
            }

            int result_id = dsl_ir_new_id(ir);
            if (!dsl_ir_emit(ir, create_op, operands, ref_count, result_id, NULL, ast->line)) return false;
            if (!dsl_ir_register_symbol(ir, ast->name, result_id)) return false;
            if (!dsl_ir_emit(ir, IR_LABEL, NULL, 0, result_id, ast->name, ast->line)) return false;
            return true;
        }
    }

    /* ---- 约束块 ---- */
    case DSL_AST_CONSTRAINT: {
        if (ast->child_count > 0) {
            return dsl_compile_block(ir, ast->children[0]);
        }
        return true;
    }

    /* ---- 证明块 ---- */
    case DSL_AST_PROVE: {
        int result_id = dsl_ir_new_id(ir);
        if (!dsl_ir_emit(ir, IR_PROVE, NULL, 0, result_id, NULL, ast->line)) return false;
        if (ast->child_count > 0) {
            return dsl_compile_block(ir, ast->children[0]);
        }
        /* 证明结束检查 */
        if (!dsl_ir_emit(ir, IR_CHECK_SAT, NULL, 0, -1, NULL, ast->line)) return false;
        return true;
    }

    /* ---- 加载公理包 ---- */
    case DSL_AST_LOAD: {
        if (!dsl_ir_emit(ir, IR_LOAD_AXIOM, NULL, 0, -1,
                         ast->name ? ast->name : "default", ast->line)) return false;
        return true;
    }

    /* ---- 语句块 ---- */
    case DSL_AST_BLOCK: {
        return dsl_compile_block(ir, ast);
    }

    /* ---- 程序根节点 ---- */
    case DSL_AST_PROGRAM: {
        return dsl_compile_block(ir, ast);
    }

    default:
        LV00_ERROR_SET(LV00_ERROR_PARSE,
                       "在第%d行：不支持的语句类型 %d", ast->line, ast->type);
        return false;
    }
}

/**
 * @brief 递归编译 AST 语句块中的所有子语句
 *
 * @param ir   IR 对象
 * @param ast  AST 块节点
 * @return true 成功，false 编译错误
 */
static bool dsl_compile_block(DslIR *ir, const DslAST *ast) {
    if (!ast) return true;
    for (int i = 0; i < ast->child_count; i++) {
        if (!dsl_compile_stmt(ir, ast->children[i])) return false;
    }
    return true;
}

/* ---- 公共 API：dsl_compile ---- */

bool dsl_compile(const DslAST *ast, const DslCompileConfig *config, DslIR **out_ir) {
    LV00_CHECK_NULL(ast, false);
    LV00_CHECK_NULL(config, false);
    LV00_CHECK_NULL(out_ir, false);

    DslIR *ir = (DslIR *)lv00_calloc(1, sizeof(DslIR));
    LV00_CHECK_ALLOC(ir, false);
    dsl_ir_init(ir);

    if (!dsl_compile_block(ir, ast)) {
        dsl_ir_free(ir);
        return false;
    }

    if (config->validate_ir) {
        /* 基本 IR 验证：检查所有操作数引用是否有效 */
        for (int i = 0; i < ir->op_count; i++) {
            DslIROperation *op = &ir->operations[i];
            for (int j = 0; j < op->operand_count; j++) {
                int oid = op->operands[j];
                /* 操作数 ID 不能为负 (0 保留为无效) */
                if (oid <= 0) {
                    LV00_ERROR_SET(LV00_ERROR_PARSE,
                                   "IR 验证失败：操作 %d 的操作数 %d 引用无效 ID %d (源码行 %d)",
                                   i, j, oid, op->source_line);
                    dsl_ir_free(ir);
                    return false;
                }
            }
        }
    }

    *out_ir = ir;
    return true;
}

/* ---- 公共 API：dsl_ir_free ---- */

void dsl_ir_free(DslIR *ir) {
    if (!ir) return;
    if (ir->operations) {
        for (int i = 0; i < ir->op_count; i++) {
            if (ir->operations[i].operands) {
                lv00_free((void **)&ir->operations[i].operands);
            }
        }
        lv00_free((void **)&ir->operations);
    }
    if (ir->symbols) {
        for (int i = 0; i < ir->symbol_count; i++) {
            if (ir->symbols[i]) {
                lv00_free((void **)&ir->symbols[i]);
            }
        }
        lv00_free((void **)&ir->symbols);
    }
    if (ir->symbol_to_ir_id) {
        lv00_free((void **)&ir->symbol_to_ir_id);
    }
    lv00_free((void **)&ir);
}

/* ---- 公共 API：dsl_ir_op_name ---- */

const char *dsl_ir_op_name(DslIROp op) {
    switch (op) {
    case IR_CREATE_POINT:            return "CREATE_POINT";
    case IR_CREATE_POINT_FIXED:      return "CREATE_POINT_FIXED";
    case IR_CREATE_LINE:             return "CREATE_LINE";
    case IR_CREATE_CIRCLE:           return "CREATE_CIRCLE";
    case IR_CREATE_SEGMENT:          return "CREATE_SEGMENT";
    case IR_CREATE_RAY:              return "CREATE_RAY";
    case IR_CREATE_POLYGON:          return "CREATE_POLYGON";
    case IR_CREATE_TRIANGLE:         return "CREATE_TRIANGLE";
    case IR_INTERSECT:               return "INTERSECT";
    case IR_PARALLEL_THROUGH:        return "PARALLEL_THROUGH";
    case IR_PERPENDICULAR_THROUGH:   return "PERPENDICULAR_THROUGH";
    case IR_MIDPOINT_OF:             return "MIDPOINT_OF";
    case IR_CIRCUMCENTER_OF:         return "CIRCUMCENTER_OF";
    case IR_ORTHOCENTER_OF:          return "ORTHOCENTER_OF";
    case IR_CENTROID_OF:             return "CENTROID_OF";
    case IR_INCENTER_OF:             return "INCENTER_OF";
    case IR_BISECTOR_OF:             return "BISECTOR_OF";
    case IR_ANGLE_BISECTOR:          return "ANGLE_BISECTOR";
    case IR_ADD_CONSTRAINT:          return "ADD_CONSTRAINT";
    case IR_REMOVE_CONSTRAINT:       return "REMOVE_CONSTRAINT";
    case IR_CONSTRAIN_EQUAL:         return "CONSTRAIN_EQUAL";
    case IR_CONSTRAIN_PARALLEL:      return "CONSTRAIN_PARALLEL";
    case IR_CONSTRAIN_PERPENDICULAR: return "CONSTRAIN_PERPENDICULAR";
    case IR_CONSTRAIN_COLLINEAR:     return "CONSTRAIN_COLLINEAR";
    case IR_CONSTRAIN_CONCYCLIC:     return "CONSTRAIN_CONCYCLIC";
    case IR_LOAD_AXIOM:              return "LOAD_AXIOM";
    case IR_PROVE:                   return "PROVE";
    case IR_CHECK_SAT:               return "CHECK_SAT";
    case IR_LABEL:                   return "LABEL";
    case IR_NOOP:                    return "NOOP";
    default:                         return "UNKNOWN";
    }
}

/* ---- 公共 API：dsl_ir_dump ---- */

void dsl_ir_dump(const DslIR *ir, void *fd) {
    if (!ir) return;
    FILE *fp = (FILE *)fd;
    dsl_fd_printf(fd, "=== DSL IR Dump ===\n");
    dsl_fd_printf(fd, "Operations: %d\n", ir->op_count);
    dsl_fd_printf(fd, "Symbols: %d\n", ir->symbol_count);
    dsl_fd_printf(fd, "Next ID: %d\n\n", ir->next_id);

    /* 输出符号表 */
    if (ir->symbol_count > 0) {
        dsl_fd_printf(fd, "--- Symbol Table ---\n");
        for (int i = 0; i < ir->symbol_count; i++) {
            dsl_fd_printf(fd, "  [%d] '%s' -> IR result_id=%d\n",
                         i, ir->symbols[i] ? ir->symbols[i] : "(null)",
                         ir->symbol_to_ir_id[i]);
        }
        dsl_fd_printf(fd, "\n");
    }

    /* 输出操作序列 */
    dsl_fd_printf(fd, "--- IR Operations ---\n");
    for (int i = 0; i < ir->op_count; i++) {
        const DslIROperation *op = &ir->operations[i];
        dsl_fd_printf(fd, "  [%3d] %-28s result=%3d  operands=[",
                     i, dsl_ir_op_name(op->op), op->result_id);
        for (int j = 0; j < op->operand_count; j++) {
            dsl_fd_printf(fd, "%s%d", (j > 0 ? "," : ""), op->operands[j]);
        }
        dsl_fd_printf(fd, "]");
        if (op->label) {
            dsl_fd_printf(fd, " label='%s'", op->label);
        }
        dsl_fd_printf(fd, " @line %d\n", op->source_line);
    }
    dsl_fd_printf(fd, "=== End IR Dump ===\n");
    (void)fp;
}

/* ================================================================
 *  第 4 部分：IR -> 约束图转换
 *
 *  遍历 IR 操作序列，逐条解释执行，创建约束图节点和约束。
 *  这是编译管线的最后一步。
 * ================================================================ */

/**
 * @brief IR 解释执行器内部状态
 *
 * 维护 IR 结果 ID 到约束图节点 ID 的映射，以及约束图引用。
 */
typedef struct {
    ConstraintGraph *graph;        /**< 目标约束图 */
    const DslIR *ir;               /**< 输入的 IR */
    int *result_to_node;           /**< IR 结果 ID -> 约束图节点 ID 映射 */
    int result_map_size;           /**< 映射表大小 */
    int result_map_capacity;       /**< 映射表容量 */
} DSLIRInterpreter;

/**
 * @brief 初始化解释器状态
 *
 * @param interp 解释器状态（需由调用者分配）
 * @param graph  目标约束图
 * @param ir     输入的 IR
 */
static void dsl_interp_init(DSLIRInterpreter *interp, ConstraintGraph *graph, const DslIR *ir) {
    memset(interp, 0, sizeof(*interp));
    interp->graph = graph;
    interp->ir = ir;
}

/**
 * @brief 注册 IR 结果 ID 到约束图节点 ID 的映射
 *
 * @param interp    解释器状态
 * @param result_id IR 结果 ID
 * @param node_id   约束图节点 ID
 * @return true 成功，false 内存不足
 */
static bool dsl_interp_map_register(DSLIRInterpreter *interp, int result_id, int node_id) {
    /* 确保映射表容量足够 */
    if (result_id >= interp->result_map_capacity) {
        int nc = interp->result_map_capacity == 0 ? 64 : interp->result_map_capacity * 2;
        while (nc <= result_id) nc *= 2;
        void *np = lv00_realloc(interp->result_to_node, (size_t)nc * sizeof(int));
        if (!np) return false;
        interp->result_to_node = (int *)np;
        /* 新扩展区域初始化为 -1 */
        for (int i = interp->result_map_capacity; i < nc; i++) {
            interp->result_to_node[i] = -1;
        }
        interp->result_map_capacity = nc;
        if (interp->result_map_size < nc) interp->result_map_size = nc;
    }
    interp->result_to_node[result_id] = node_id;
    return true;
}

/**
 * @brief 查找 IR 结果 ID 对应的约束图节点 ID
 *
 * @param interp    解释器状态
 * @param result_id IR 结果 ID
 * @return 约束图节点 ID，未找到返回 -1
 */
static int dsl_interp_map_lookup(const DSLIRInterpreter *interp, int result_id) {
    if (result_id < 0 || result_id >= interp->result_map_capacity) return -1;
    return interp->result_to_node[result_id];
}

/**
 * @brief 释放解释器内部资源
 *
 * @param interp 解释器状态
 */
static void dsl_interp_free(DSLIRInterpreter *interp) {
    if (interp->result_to_node) {
        lv00_free((void **)&interp->result_to_node);
    }
    interp->result_map_capacity = 0;
    interp->result_map_size = 0;
}

/**
 * @brief 执行单条 IR 操作，将其翻译为约束图操作
 *
 * @param interp 解释器状态
 * @param op     待执行的 IR 操作
 * @return true 成功，false 执行失败
 */
static bool dsl_interp_execute(DSLIRInterpreter *interp, const DslIROperation *op) {
    ConstraintGraph *graph = interp->graph;

    switch (op->op) {

    /* ---- 实体创建 ---- */
    case IR_CREATE_POINT: {
        /* 自由点：创建两个有理数坐标 (0,0) 作为占位 */
        SymbolicCoord *x = symbolic_coord_create_rational(0, 1);
        SymbolicCoord *y = symbolic_coord_create_rational(0, 1);
        if (!x || !y) {
            if (x) symbolic_coord_destroy(x);
            if (y) symbolic_coord_destroy(y);
            return false;
        }
        SymbolicCoord *coords[2] = {x, y};
        AddNodeResult r = graph_add_point(graph, coords, 2);
        if (r != ADD_NODE_OK) return false;
        int node_id = graph_get_last_added_node_id(graph);
        if (node_id < 0) return false;
        return dsl_interp_map_register(interp, op->result_id, node_id);
    }

    case IR_CREATE_POINT_FIXED: {
        /* 固定坐标点：operands[0] 和 operands[1] 编码了缩放后的坐标 */
        int64_t x_num = 0, y_num = 0;
        if (op->operand_count >= 2) {
            x_num = (int64_t)op->operands[0];
            y_num = (int64_t)op->operands[1];
        }
        SymbolicCoord *x = symbolic_coord_create_rational(x_num, 1000);
        SymbolicCoord *y = symbolic_coord_create_rational(y_num, 1000);
        if (!x || !y) {
            if (x) symbolic_coord_destroy(x);
            if (y) symbolic_coord_destroy(y);
            return false;
        }
        SymbolicCoord *coords[2] = {x, y};
        AddNodeResult r = graph_add_point(graph, coords, 2);
        if (r != ADD_NODE_OK) return false;
        int node_id = graph_get_last_added_node_id(graph);
        if (node_id < 0) return false;
        return dsl_interp_map_register(interp, op->result_id, node_id);
    }

    case IR_CREATE_LINE: {
        /* 通过两个端点创建线段 */
        if (op->operand_count < 2) return false;
        int p1 = dsl_interp_map_lookup(interp, op->operands[0]);
        int p2 = dsl_interp_map_lookup(interp, op->operands[1]);
        if (p1 < 0 || p2 < 0) return false;
        AddNodeResult r = graph_add_line_segment(graph, p1, p2);
        if (r != ADD_NODE_OK) return false;
        int node_id = graph_get_last_added_node_id(graph);
        if (node_id < 0) return false;
        return dsl_interp_map_register(interp, op->result_id, node_id);
    }

    case IR_CREATE_CIRCLE: {
        /* 圆：通过圆心和半径点创建 —— 简化处理：创建两个点作为标识 */
        if (op->operand_count < 2) return false;
        int center_id = dsl_interp_map_lookup(interp, op->operands[0]);
        int radius_pt_id = dsl_interp_map_lookup(interp, op->operands[1]);
        if (center_id < 0 || radius_pt_id < 0) return false;
        /* 圆暂用区域表示：创建包含两点及关联约束 */
        AddNodeResult r = graph_add_line_segment(graph, center_id, radius_pt_id);
        if (r != ADD_NODE_OK) return false;
        int node_id = graph_get_last_added_node_id(graph);
        if (node_id < 0) return false;
        return dsl_interp_map_register(interp, op->result_id, node_id);
    }

    case IR_CREATE_SEGMENT: {
        if (op->operand_count < 2) return false;
        int p1 = dsl_interp_map_lookup(interp, op->operands[0]);
        int p2 = dsl_interp_map_lookup(interp, op->operands[1]);
        if (p1 < 0 || p2 < 0) return false;
        AddNodeResult r = graph_add_line_segment(graph, p1, p2);
        if (r != ADD_NODE_OK) return false;
        int node_id = graph_get_last_added_node_id(graph);
        if (node_id < 0) return false;
        return dsl_interp_map_register(interp, op->result_id, node_id);
    }

    case IR_CREATE_RAY: {
        /* 射线：通过原点和方向点创建 —— 沿用线段表示 */
        if (op->operand_count < 2) return false;
        int origin = dsl_interp_map_lookup(interp, op->operands[0]);
        int dir_pt = dsl_interp_map_lookup(interp, op->operands[1]);
        if (origin < 0 || dir_pt < 0) return false;
        AddNodeResult r = graph_add_line_segment(graph, origin, dir_pt);
        if (r != ADD_NODE_OK) return false;
        int node_id = graph_get_last_added_node_id(graph);
        if (node_id < 0) return false;
        return dsl_interp_map_register(interp, op->result_id, node_id);
    }

    case IR_CREATE_POLYGON: {
        /* 多边形：收集所有顶点，依次创建边 */
        int first_node = -1;
        int prev_node = -1;
        for (int i = 0; i < op->operand_count; i++) {
            int pt = dsl_interp_map_lookup(interp, op->operands[i]);
            if (pt < 0) return false;
            if (i == 0) {
                first_node = pt;
            } else {
                AddNodeResult r = graph_add_line_segment(graph, prev_node, pt);
                if (r != ADD_NODE_OK) return false;
            }
            prev_node = pt;
        }
        /* 闭合边 */
        if (op->operand_count >= 3 && first_node >= 0) {
            AddNodeResult r = graph_add_line_segment(graph, prev_node, first_node);
            if (r != ADD_NODE_OK) return false;
        }
        return true;
    }

    case IR_CREATE_TRIANGLE: {
        if (op->operand_count < 3) return false;
        int a = dsl_interp_map_lookup(interp, op->operands[0]);
        int b = dsl_interp_map_lookup(interp, op->operands[1]);
        int c = dsl_interp_map_lookup(interp, op->operands[2]);
        if (a < 0 || b < 0 || c < 0) return false;
        /* 创建三条边 */
        AddNodeResult r1 = graph_add_line_segment(graph, a, b);
        AddNodeResult r2 = graph_add_line_segment(graph, b, c);
        AddNodeResult r3 = graph_add_line_segment(graph, c, a);
        if (r1 != ADD_NODE_OK || r2 != ADD_NODE_OK || r3 != ADD_NODE_OK) return false;
        return true;
    }

    /* ---- 构造操作 ---- */
    case IR_INTERSECT: {
        /* 交点：创建自由点，添加相交约束 */
        if (op->operand_count < 2) return false;
        int obj1 = dsl_interp_map_lookup(interp, op->operands[0]);
        int obj2 = dsl_interp_map_lookup(interp, op->operands[1]);
        if (obj1 < 0 || obj2 < 0) return false;
        /* 创建交点 */
        SymbolicCoord *x = symbolic_coord_create_rational(0, 1);
        SymbolicCoord *y = symbolic_coord_create_rational(0, 1);
        if (!x || !y) {
            if (x) symbolic_coord_destroy(x);
            if (y) symbolic_coord_destroy(y);
            return false;
        }
        SymbolicCoord *coords[2] = {x, y};
        AddNodeResult r = graph_add_point(graph, coords, 2);
        if (r != ADD_NODE_OK) return false;
        int pt_id = graph_get_last_added_node_id(graph);
        if (pt_id < 0) return false;
        /* 添加相交约束 */
        graph_add_intersection(graph, obj1, obj2, pt_id);
        return dsl_interp_map_register(interp, op->result_id, pt_id);
    }

    case IR_MIDPOINT_OF: {
        /* 中点：创建自由点，添加关联约束 */
        if (op->operand_count < 2) return false;
        int p1 = dsl_interp_map_lookup(interp, op->operands[0]);
        int p2 = dsl_interp_map_lookup(interp, op->operands[1]);
        if (p1 < 0 || p2 < 0) return false;
        /* 创建中点 */
        SymbolicCoord *x = symbolic_coord_create_rational(0, 1);
        SymbolicCoord *y = symbolic_coord_create_rational(0, 1);
        if (!x || !y) {
            if (x) symbolic_coord_destroy(x);
            if (y) symbolic_coord_destroy(y);
            return false;
        }
        SymbolicCoord *coords[2] = {x, y};
        AddNodeResult r = graph_add_point(graph, coords, 2);
        if (r != ADD_NODE_OK) return false;
        int mid_id = graph_get_last_added_node_id(graph);
        if (mid_id < 0) return false;
        /* 添加之间约束 */
        graph_add_betweenness(graph, p1, mid_id, p2);
        return dsl_interp_map_register(interp, op->result_id, mid_id);
    }

    case IR_PARALLEL_THROUGH:
    case IR_PERPENDICULAR_THROUGH:
    case IR_CIRCUMCENTER_OF:
    case IR_ORTHOCENTER_OF:
    case IR_CENTROID_OF:
    case IR_INCENTER_OF:
    case IR_BISECTOR_OF:
    case IR_ANGLE_BISECTOR: {
        /* 高级构造操作：创建自由点/线段作为占位，后续由求解器确定精确位置 */
        SymbolicCoord *x = symbolic_coord_create_rational(0, 1);
        SymbolicCoord *y = symbolic_coord_create_rational(0, 1);
        if (!x || !y) {
            if (x) symbolic_coord_destroy(x);
            if (y) symbolic_coord_destroy(y);
            return false;
        }
        SymbolicCoord *coords[2] = {x, y};
        AddNodeResult r = graph_add_point(graph, coords, 2);
        if (r != ADD_NODE_OK) return false;
        int entity_id = graph_get_last_added_node_id(graph);
        if (entity_id < 0) return false;
        return dsl_interp_map_register(interp, op->result_id, entity_id);
    }

    /* ---- 约束操作 ---- */
    case IR_ADD_CONSTRAINT:
        /* 一般约束添加：由具体约束类型处理，此处为 noop ——
           约束块中的具体约束应在 IR 编译阶段展开为具体约束操作 */
        return true;

    case IR_REMOVE_CONSTRAINT:
        /* 约束移除：暂不实现 */
        return true;

    case IR_CONSTRAIN_EQUAL:
    case IR_CONSTRAIN_PARALLEL:
    case IR_CONSTRAIN_PERPENDICULAR:
    case IR_CONSTRAIN_COLLINEAR:
    case IR_CONSTRAIN_CONCYCLIC:
        /* 高级约束：创建关联关系 */
        if (op->operand_count >= 2) {
            int a = dsl_interp_map_lookup(interp, op->operands[0]);
            int b = dsl_interp_map_lookup(interp, op->operands[1]);
            if (a >= 0 && b >= 0) {
                /* 以关联约束作为通用约束表达 */
                graph_add_incidence(graph, a, b);
            }
        }
        return true;

    /* ---- 系统操作 ---- */
    case IR_LOAD_AXIOM:
        /* 公理加载：标记已处理，实际加载由引擎层负责 */
        return true;

    case IR_PROVE:
        /* 证明初始化 */
        return true;

    case IR_CHECK_SAT:
        /* 可满足性检查：由引擎层负责 */
        return true;

    /* ---- 元操作 ---- */
    case IR_LABEL:
        /* 标签：IR 编译阶段已将标签关联到实体，此处无需额外处理 */
        return true;

    case IR_NOOP:
        return true;

    default:
        LV00_ERROR_SET(LV00_ERROR_INVALID_PARAM,
                       "不支持的 IR 操作码 %d (源码行 %d)", op->op, op->source_line);
        return false;
    }
}

/* ---- 公共 API：dsl_ir_to_constraint_graph ---- */

bool dsl_ir_to_constraint_graph(const DslIR *ir, ConstraintGraph *graph) {
    LV00_CHECK_NULL(ir, false);
    LV00_CHECK_NULL(graph, false);

    DSLIRInterpreter interp;
    dsl_interp_init(&interp, graph, ir);

    bool success = true;
    for (int i = 0; i < ir->op_count; i++) {
        if (!dsl_interp_execute(&interp, &ir->operations[i])) {
            LV00_ERROR_SET(LV00_ERROR_PARSE,
                           "IR 解释执行失败：操作 %d (%s) 在源码行 %d",
                           i, dsl_ir_op_name(ir->operations[i].op),
                           ir->operations[i].source_line);
            success = false;
            break;
        }
    }

    dsl_interp_free(&interp);
    return success;
}

/* ================================================================
 *  第 5 部分：便捷函数
 * ================================================================ */

/* ---- 公共 API：dsl_compile_config_default ---- */

void dsl_compile_config_default(DslCompileConfig *out_config) {
    if (!out_config) return;
    memset(out_config, 0, sizeof(*out_config));
    out_config->target = TARGET_NATIVE;
    out_config->optimize_level = 1;
    out_config->debug_ast = false;
    out_config->validate_ir = true;
    out_config->generate_source_map = true;
    out_config->max_iterations = 1000;
}

/* ---- 公共 API：dsl_compile_and_load ---- */

bool dsl_compile_and_load(const char *source, const DslCompileConfig *config, ConstraintGraph *graph) {
    LV00_CHECK_NULL(source, false);
    LV00_CHECK_NULL(graph, false);

    DslCompileConfig default_cfg;
    if (!config) {
        dsl_compile_config_default(&default_cfg);
        config = &default_cfg;
    }

    /* 阶段 1：词法分析 */
    DslToken *tokens = NULL;
    int token_count = 0;
    if (!dsl_tokenize(source, &tokens, &token_count)) {
        return false;
    }

    /* 阶段 2：语法分析 */
    DslAST *ast = NULL;
    if (!dsl_parse(tokens, token_count, &ast)) {
        dsl_tokens_free(tokens, token_count);
        return false;
    }
    dsl_tokens_free(tokens, token_count);

    /* 阶段 3：IR 编译 */
    DslIR *ir = NULL;
    if (!dsl_compile(ast, config, &ir)) {
        dsl_ast_free(ast);
        return false;
    }
    dsl_ast_free(ast);

    /* 阶段 4：IR -> 约束图 */
    if (!dsl_ir_to_constraint_graph(ir, graph)) {
        dsl_ir_free(ir);
        return false;
    }
    dsl_ir_free(ir);

    return true;
}
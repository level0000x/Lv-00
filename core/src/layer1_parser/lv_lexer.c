/**
 * @file lv_lexer.c
 * @brief Lv-00 DSL 词法分析器实现
 *
 * @details 实现 .lv 源文件的词法分析功能，将源代码文本转换为 Token 流。
 *          支持 75 种 Token 类型，包括关键字、运算符、字面量（整数、有理数、
 *          小数、字符串、布尔值）和分隔符。使用 32 Token 前瞻缓冲区实现
 *          lookahead 解析支持（支持嵌套泛型等深度前瞻扫描）。
 *
 *          主要特性：
 *          - 整数、有理数（3/4）、小数（3.14）数字字面量
 *          - 字符串字面量，含转义序列处理
 *          - C 风格单行（//）和块注释（slash-star ... star-slash）
 *          - 关键字查找表
 *          - 多字符运算符（->, ==, !=, <=, >=, =>, |-, |=）
 *
 * @author Lv-00 Project
 */

#include "lv/lv_lexer.h"

#include "lv/lv_xmacro.h"
#include "lv/parser_safety.h" /* lv_check_token_length（F16/G1 token 长度闸门） */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"

/**
 * @brief 词法分析器结构体
 *
 * 管理源代码字符串的扫描状态，包括当前位置、行号、列号
 * 和一个三 Token 的 lookahead 缓冲区。
 */
struct LvLexer {
    const char *source;  /**< 源字符串指针（不拥有所有权） */
    size_t source_len;   /**< 源字符串长度 */
    size_t pos;          /**< 当前扫描位置 */
    int line;            /**< 当前行号（从 1 开始） */
    int column;          /**< 当前列号（从 1 开始） */
    LvToken peek_buf[32]; /**< 前瞻缓冲区（容量支持嵌套泛型等深度前瞻扫描） */
    int peek_count;      /**< 前瞻缓冲区中有效 Token 数量 */
};

/* ── 关键字查找表 ── */

/**
 * @brief 关键字-类型映射条目
 */
typedef struct {
    const char *word; /**< 关键字字符串 */
    LvTokenType type; /**< 对应的 Token 类型 */
} KeywordEntry;

/**
 * @brief 关键字查找表
 *
 * 包含所有 Lv-00 DSL 保留关键字及其对应的 Token 类型。
 * 按字母顺序排列以便于维护。
 */
static const KeywordEntry s_keywords[] = {
    {"Angle", LV_TOKEN_KW_ANGLE},
    {"Assert", LV_TOKEN_KW_ASSERT},
    {"Assume", LV_TOKEN_KW_ASSUME},
    {"Axiom", LV_TOKEN_KW_AXIOM},
    {"Bool", LV_TOKEN_KW_BOOL},
    {"Circle", LV_TOKEN_KW_CIRCLE},
    {"Compute", LV_TOKEN_KW_COMPUTE},
    {"Constraint", LV_TOKEN_KW_CONSTRAINT},
    {"Export", LV_TOKEN_KW_EXPORT},
    {"Let", LV_TOKEN_KW_LET},
    {"Line", LV_TOKEN_KW_LINE},
    {"Normalize", LV_TOKEN_KW_NORMALIZE},
    {"Point", LV_TOKEN_KW_POINT},
    {"Polygon", LV_TOKEN_KW_POLYGON},
    {"Proof", LV_TOKEN_KW_PROOF},
    {"Proposition", LV_TOKEN_KW_PROPOSITION},
    {"Prove", LV_TOKEN_KW_PROVE},
    {"Ray", LV_TOKEN_KW_RAY},
    {"Scalar", LV_TOKEN_KW_SCALAR},
    {"Segment", LV_TOKEN_KW_SEGMENT},
    {"Theorem", LV_TOKEN_KW_THEOREM},
    {"Triangle", LV_TOKEN_KW_TRIANGLE},
    {"and", LV_TOKEN_KW_AND},
    {"area", LV_TOKEN_KW_AREA},
    {"bottom", LV_TOKEN_KW_BOTTOM},
    {"collinear", LV_TOKEN_KW_COLLINEAR},
    {"congruent", LV_TOKEN_KW_CONGRUENT},
    {"distance", LV_TOKEN_KW_DISTANCE},
    {"exists", LV_TOKEN_KW_EXISTS},
    {"false", LV_TOKEN_KW_FALSE},
    {"forall", LV_TOKEN_KW_FORALL},
    {"import", LV_TOKEN_KW_IMPORT},
    {"length", LV_TOKEN_KW_LENGTH},
    {"measure", LV_TOKEN_KW_MEASURE},
    {"module", LV_TOKEN_KW_MODULE},
    {"not", LV_TOKEN_KW_NOT},
    {"or", LV_TOKEN_KW_OR},
    {"parallel", LV_TOKEN_KW_PARALLEL},
    {"perpendicular", LV_TOKEN_KW_PERPENDICULAR},
    {"radius", LV_TOKEN_KW_RADIUS},
    {"tangent", LV_TOKEN_KW_TANGENT},
    {"true", LV_TOKEN_KW_TRUE},
};

/**
 * @brief 在关键字表中查找单词
 *
 * @param word 指向单词起始位置的指针
 * @param len  单词长度
 * @return 匹配的关键字 Token 类型；若未匹配则返回 LV_TOKEN_IDENTIFIER
 */
static LvTokenType lookup_keyword(const char *word, size_t len) {
    for (size_t i = 0; i < sizeof(s_keywords) / sizeof(s_keywords[0]); i++) {
        if (strlen(s_keywords[i].word) == len && strncmp(s_keywords[i].word, word, len) == 0) {
            return s_keywords[i].type;
        }
    }
    return LV_TOKEN_IDENTIFIER;
}

/* ── 共享几何关键词表 ── */

/**
 * @brief 几何关系关键词表（parser/sema 单一事实源）
 *
 * 供 lv_parser.c（is_relation_func）与 lv_sema.c（check_call 关系分支）
 * 线性 strcmp 精确匹配使用；NULL 结尾终止扫描。
 */
const char *const lv_geometry_relation_keywords[] = {
    "collinear", "parallel", "perpendicular", "congruent", "tangent", NULL
};

/**
 * @brief 几何度量关键词表（parser/sema 单一事实源）
 *
 * 供 lv_parser.c（is_measure_func）与 lv_sema.c（check_call 度量分支）
 * 线性 strcmp 精确匹配使用；NULL 结尾终止扫描。
 */
const char *const lv_measurement_keywords[] = {
    "length", "distance", "angle", "measure", "area", "radius", NULL
};

/**
 * @brief 几何对象构造函数关键词表（parser/sema 单一事实源）
 *
 * 供 lv_parser.c（is_geometry_func）线性 strcmp 精确匹配使用；NULL 结尾终止扫描。
 * lv_sema.c 的几何构造 name→handler 查表须与此表保持同名。
 */
const char *const lv_geometry_constructor_keywords[] = {
    "point", "line", "segment", "circle", "ray", "triangle", NULL
};

/* ── 单字符运算符查找表 ── */

/** 单字符运算符映射条目 */
typedef struct {
    unsigned char valid; /**< 1 表示有效映射；0 表示未映射（默认 LV_TOKEN_ERROR） */
    LvTokenType type;    /**< 映射的 Token 类型 */
} CharTokenEntry;

/**
 * @brief 单字符运算符查找表
 *
 * 按 ASCII 下标索引，未映射的字符默认为 LV_TOKEN_ERROR。
 * 用于替代 lex_raw 中的单字符运算符 switch。
 */
static const CharTokenEntry s_char_token[128] = {
    ['('] = {1, LV_TOKEN_LPAREN},
    [')'] = {1, LV_TOKEN_RPAREN},
    ['{'] = {1, LV_TOKEN_LBRACE},
    ['}'] = {1, LV_TOKEN_RBRACE},
    ['['] = {1, LV_TOKEN_LBRACKET},
    [']'] = {1, LV_TOKEN_RBRACKET},
    [';'] = {1, LV_TOKEN_SEMICOLON},
    [','] = {1, LV_TOKEN_COMMA},
    ['.'] = {1, LV_TOKEN_DOT},
    [':'] = {1, LV_TOKEN_COLON},
    ['='] = {1, LV_TOKEN_EQUALS},
    ['+'] = {1, LV_TOKEN_PLUS},
    ['-'] = {1, LV_TOKEN_MINUS},
    ['*'] = {1, LV_TOKEN_STAR},
    ['/'] = {1, LV_TOKEN_SLASH},
    ['^'] = {1, LV_TOKEN_CARET},
    ['<'] = {1, LV_TOKEN_LT},
    ['>'] = {1, LV_TOKEN_GT},
};

/**
 * @brief 构造一个 LvToken 实例
 *
 * 根据当前词法分析器状态和识别出的 Token 信息填充 Token 结构体。
 * 列号在调用前由 lex_raw 通过 start_col 参数传入，确保记录的是
 * Token 起始位置的列号而非处理后的位置。
 *
 * @param lexer     词法分析器指针
 * @param type      Token 类型
 * @param start_pos Token 在源字符串中的起始偏移量
 * @param length    Token 的长度（字节数）
 * @param start_col Token 起始列号（跳过空白和注释后的列号）
 * @return 填充完成的 LvToken
 */
static LvToken make_token_at(LvLexer *lexer, LvTokenType type, size_t start_pos, size_t length, int start_col) {
    LvToken tok;
    tok.type = type;
    tok.loc.offset = start_pos;
    tok.loc.line = lexer->line;
    tok.loc.column = start_col;
    tok.start = lexer->source + start_pos;
    tok.length = length;

    /* F16/G1：token 长度闸门——超长 token（标识符/字符串等）转为 ERROR
     * token，parser 跳过；上限 lvConfig.parser.parser_max_token_length
     * （默认 4096 可配置不硬编码）。EOF/ERROR 本身不检查。 */
    if (length > 0 && type != LV_TOKEN_EOF && type != LV_TOKEN_ERROR) {
        if (lv_check_token_length(length) != lv_OK) {
            tok.type = LV_TOKEN_ERROR;
        }
    }
    return tok;
}

/**
 * @brief 构造一个 LvToken 实例（使用当前列号）
 *
 * 兼容旧接口：使用 lexer->column 作为 Token 起始列号。
 * 适用于 EOF/ERROR 等无需精确列号的 Token。
 */
static LvToken make_token(LvLexer *lexer, LvTokenType type, size_t start_pos, size_t length) {
    return make_token_at(lexer, type, start_pos, length, lexer->column);
}

/**
 * @brief 跳过空白字符和注释
 *
 * 扫描并跳过空格、制表符、换行符、回车符，以及 C 风格的单行（//）
 * 和块注释（slash-star ... star-slash）。跳过自动更新行号和列号。
 *
 * @param lexer 词法分析器指针
 */
static void skip_whitespace_and_comments(LvLexer *lexer) {
    while (lexer->pos < lexer->source_len) {
        char c = lexer->source[lexer->pos];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            if (c == '\n') {
                lexer->line++;
                lexer->column = 1;
            } else {
                lexer->column++;
            }
            lexer->pos++;
        } else if (c == '/' && lexer->pos + 1 < lexer->source_len) {
            char next = lexer->source[lexer->pos + 1];
            if (next == '/') {
                while (lexer->pos < lexer->source_len && lexer->source[lexer->pos] != '\n')
                    lexer->pos++;
            } else if (next == '*') {
                lexer->pos += 2;
                while (lexer->pos + 1 < lexer->source_len) {
                    if (lexer->source[lexer->pos] == '*' && lexer->source[lexer->pos + 1] == '/') {
                        lexer->pos += 2;
                        break;
                    }
                    if (lexer->source[lexer->pos] == '\n') {
                        lexer->line++;
                        lexer->column = 1;
                    }
                    lexer->pos++;
                }
            } else {
                break;
            }
        } else {
            break;
        }
    }
}

/**
 * @brief 扫描并识别的下一个 Token（底层实现）
 *
 * 从当前位置开始识别一个完整的 Token，支持标识符/关键字、数字字面量
 * （整数、有理数、小数）、字符串字面量、多字符运算符和单字符运算符/分隔符。
 * 识别前自动跳过前置空白和注释。
 *
 * @param lexer 词法分析器指针
 * @return 识别出的 LvToken
 */
static LvToken lex_raw(LvLexer *lexer) {
    skip_whitespace_and_comments(lexer);

    if (lexer->pos >= lexer->source_len)
        return make_token(lexer, LV_TOKEN_EOF, lexer->pos, 0);

    size_t start = lexer->pos;
    int start_col = lexer->column; /* 记录 Token 起始列号 */
    char c = lexer->source[lexer->pos];

    /* 标识符 / 关键字 */
    if (isalpha((unsigned char) c) || c == '_') {
        while (lexer->pos < lexer->source_len &&
               (isalnum((unsigned char) lexer->source[lexer->pos]) || lexer->source[lexer->pos] == '_'))
            lexer->pos++;
        size_t len = lexer->pos - start;
        LvTokenType type = lookup_keyword(lexer->source + start, len);
        lexer->column += (int) len;
        return make_token_at(lexer, type, start, len, start_col);
    }

    /* 数字 */
    if (isdigit((unsigned char) c)) {
        while (lexer->pos < lexer->source_len && isdigit((unsigned char) lexer->source[lexer->pos]))
            lexer->pos++;
        /* 有理数: 3/4 */
        if (lexer->pos < lexer->source_len && lexer->source[lexer->pos] == '/') {
            lexer->pos++;
            if (lexer->pos < lexer->source_len && isdigit((unsigned char) lexer->source[lexer->pos])) {
                while (lexer->pos < lexer->source_len && isdigit((unsigned char) lexer->source[lexer->pos]))
                    lexer->pos++;
                size_t len = lexer->pos - start;
                lexer->column += (int) len;
                return make_token_at(lexer, LV_TOKEN_RATIONAL, start, len, start_col);
            }
            /* '/' 后非数字，回退：按整数处理 '/' 之前的部分 */
            lexer->pos = start;
            while (lexer->pos < lexer->source_len && isdigit((unsigned char) lexer->source[lexer->pos]))
                lexer->pos++;
            size_t len = lexer->pos - start;
            lexer->column += (int) len;
            return make_token_at(lexer, LV_TOKEN_INTEGER, start, len, start_col);
        }
        /* 小数: 3.14 */
        if (lexer->pos < lexer->source_len && lexer->source[lexer->pos] == '.') {
            size_t dot_pos = lexer->pos;
            lexer->pos++;
            if (lexer->pos < lexer->source_len && isdigit((unsigned char) lexer->source[lexer->pos])) {
                while (lexer->pos < lexer->source_len && isdigit((unsigned char) lexer->source[lexer->pos]))
                    lexer->pos++;
                size_t len = lexer->pos - start;
                lexer->column += (int) len;
                return make_token_at(lexer, LV_TOKEN_DECIMAL, start, len, start_col);
            }
            /* '.' 后非数字，回退：按整数处理 */
            lexer->pos = dot_pos;
        }
        size_t len = lexer->pos - start;
        lexer->column += (int) len;
        return make_token_at(lexer, LV_TOKEN_INTEGER, start, len, start_col);
    }

    /* 字符串 */
    if (c == '"') {
        lexer->pos++;
        while (lexer->pos < lexer->source_len && lexer->source[lexer->pos] != '"') {
            if (lexer->source[lexer->pos] == '\\')
                lexer->pos++;
            lexer->pos++;
        }
        if (lexer->pos < lexer->source_len)
            lexer->pos++;
        size_t len = lexer->pos - start;
        lexer->column += (int) len;
        return make_token_at(lexer, LV_TOKEN_STRING, start, len, start_col);
    }

    /* 多字符运算符 */
    if (c == '/' && lexer->pos + 1 < lexer->source_len && lexer->source[lexer->pos + 1] == '\\') {
        /* 合取符号 "/\" 等价于关键字 "and"（规格文件惯用写法） */
        lexer->pos += 2;
        lexer->column += 2;
        return make_token_at(lexer, LV_TOKEN_KW_AND, start, 2, start_col);
    }
    if (c == '-' && lexer->pos + 1 < lexer->source_len && lexer->source[lexer->pos + 1] == '>') {
        lexer->pos += 2;
        lexer->column += 2;
        return make_token_at(lexer, LV_TOKEN_ARROW, start, 2, start_col);
    }
    if (c == '=' && lexer->pos + 1 < lexer->source_len && lexer->source[lexer->pos + 1] == '=') {
        lexer->pos += 2;
        lexer->column += 2;
        return make_token_at(lexer, LV_TOKEN_EQEQ, start, 2, start_col);
    }
    if (c == '!' && lexer->pos + 1 < lexer->source_len && lexer->source[lexer->pos + 1] == '=') {
        lexer->pos += 2;
        lexer->column += 2;
        return make_token_at(lexer, LV_TOKEN_NEQ, start, 2, start_col);
    }
    if (c == '<' && lexer->pos + 1 < lexer->source_len && lexer->source[lexer->pos + 1] == '=') {
        lexer->pos += 2;
        lexer->column += 2;
        return make_token_at(lexer, LV_TOKEN_LE, start, 2, start_col);
    }
    if (c == '>' && lexer->pos + 1 < lexer->source_len && lexer->source[lexer->pos + 1] == '=') {
        lexer->pos += 2;
        lexer->column += 2;
        return make_token_at(lexer, LV_TOKEN_GE, start, 2, start_col);
    }
    if (c == '=' && lexer->pos + 1 < lexer->source_len && lexer->source[lexer->pos + 1] == '>') {
        lexer->pos += 2;
        lexer->column += 2;
        return make_token_at(lexer, LV_TOKEN_THEREFORE, start, 2, start_col);
    }
    if (c == '|' && lexer->pos + 1 < lexer->source_len) {
        char n = lexer->source[lexer->pos + 1];
        if (n == '-') {
            lexer->pos += 2;
            lexer->column += 2;
            return make_token_at(lexer, LV_TOKEN_DARROW, start, 2, start_col);
        }
        if (n == '=') {
            lexer->pos += 2;
            lexer->column += 2;
            return make_token_at(lexer, LV_TOKEN_MODELS, start, 2, start_col);
        }
        if (n == '>') {
            /* S2 管道 |> */
            lexer->pos += 2;
            lexer->column += 2;
            return make_token_at(lexer, LV_TOKEN_PIPE_GT, start, 2, start_col);
        }
    }
    if (c == '|') {
        /* 单独 '|'：类型联合分隔符 */
        lexer->pos++;
        lexer->column++;
        return make_token_at(lexer, LV_TOKEN_PIPE, start, 1, start_col);
    }

    /* 单字符运算符 */
    lexer->pos++;
    lexer->column++;
    /* 查表分发：未映射的字符默认为 LV_TOKEN_ERROR */
    LvTokenType type = LV_TOKEN_ERROR;
    if ((unsigned char) c < 128) {
        const CharTokenEntry *entry = &s_char_token[(unsigned char) c];
        if (entry->valid)
            type = entry->type;
    }
    return make_token_at(lexer, type, start, 1, start_col);
}

/* ── 公共 API ── */

/**
 * @brief 创建词法分析器
 *
 * 分配并初始化一个新的词法分析器实例。分析器不拥有 source 字符串的所有权，
 * 调用者需确保 source 在分析器使用期间保持有效。
 *
 * @param source     源字符串指针
 * @param source_len 源字符串长度
 * @return 词法分析器指针，失败返回 NULL
 */
LvLexer *lv_lexer_create(const char *source, size_t source_len) {
    LvLexer *lexer = (LvLexer *) lv_calloc(1, sizeof(LvLexer));
    if (!lexer)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "failed to allocate lexer");
    lexer->source = source;
    lexer->source_len = source_len;
    lexer->pos = 0;
    lexer->line = 1;
    lexer->column = 1;
    lexer->peek_count = 0;
    return lexer;
}

/**
 * @brief 销毁词法分析器
 *
 * 释放词法分析器占用的内存。注意：不释放 source 字符串，因为
 * 词法分析器不拥有 source 的所有权。
 *
 * @param lexer 词法分析器指针
 */
void lv_lexer_destroy(LvLexer *lexer) {
    lv_free((void **) &lexer);
}

/**
 * @brief 获取下一个 Token
 *
 * 从词法分析器获取下一个 Token。如果前瞻缓冲区中有已缓存的 Token，
 * 优先从缓冲区返回。
 *
 * @param lexer 词法分析器指针
 * @return 下一个 LvToken
 */
LvToken lv_lexer_next(LvLexer *lexer) {
    if (lexer->peek_count > 0) {
        LvToken tok = lexer->peek_buf[0];
        lexer->peek_count--;
        for (int i = 0; i < lexer->peek_count; i++)
            lexer->peek_buf[i] = lexer->peek_buf[i + 1];
        return tok;
    }
    return lex_raw(lexer);
}

/**
 * @brief 前瞻获取 Token
 *
 * 返回当前位置之后第 lookahead 个 Token，不消耗任何 Token。
 * 前瞻缓冲区最多缓存 32 个 Token（lookahead 有效范围为 0~31）。
 *
 * @param lexer     词法分析器指针
 * @param lookahead 前瞻偏移量（0 为下一个 Token，最大 31）
 * @return 前瞻位置的 LvToken；若 lookahead 越界则返回 LV_TOKEN_ERROR
 */
LvToken lv_lexer_peek(LvLexer *lexer, int lookahead) {
    /* [安全] 限制 lookahead 最大为 31，防止 peek_buf[32] 越界写入 */
    if (lookahead < 0 || lookahead >= 32) {
        LvToken err_tok;
        memset(&err_tok, 0, sizeof(err_tok));
        err_tok.type = LV_TOKEN_ERROR;
        return err_tok;
    }
    while (lexer->peek_count <= lookahead) {
        lexer->peek_buf[lexer->peek_count++] = lex_raw(lexer);
    }
    return lexer->peek_buf[lookahead];
}

/**
 * @brief 获取词法分析器当前位置
 *
 * @param lexer 词法分析器指针
 * @return 当前源代码位置信息（行号、列号、偏移量）
 */
LvSourceLoc lv_lexer_get_loc(const LvLexer *lexer) {
    LvSourceLoc loc;
    loc.line = lexer->line;
    loc.column = lexer->column;
    loc.offset = lexer->pos;
    return loc;
}

/**
 * @brief 获取 Token 类型的字符串名称
 *
 * @param type Token 类型枚举值
 * @return 类型名称字符串（静态存储，无需释放）
 */
const char *lv_token_type_name(LvTokenType type) {
    static const char *const names[] = {
        lv_XMACRO_TO_NAME_ARRAY(LV_TOKEN_TYPE_X)
    };
    if (type >= 0 && type < LV_TOKEN_COUNT)
        return names[type];
    return "UNKNOWN";
}

/**
 * @brief 提取 Token 的文本内容
 *
 * 将 Token 的源文本复制到用户提供的缓冲区中，并确保 null 终止。
 * 如果 Token 长度超过缓冲区容量，则截断。
 *
 * @param token    Token 指针
 * @param buf      输出缓冲区
 * @param buf_size 输出缓冲区大小
 * @return 实际复制的字符数（不含 null 终止符）
 */
size_t lv_token_text(const LvToken *token, char *buf, size_t buf_size) {
    if (!token || !buf || buf_size == 0)
        return 0;
    /* [安全] 使用 lv_strlcpy，保证 null 终止且处理 NULL src */
    size_t copy_len = token->length < buf_size - 1 ? token->length : buf_size - 1;
    lv_strlcpy(buf, token->start, copy_len + 1);
    return copy_len;
}

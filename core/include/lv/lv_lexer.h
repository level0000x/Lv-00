#ifndef LV_LEXER_H
#define LV_LEXER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Token 类型 ── */
typedef enum {
    /* 字面量 */
    LV_TOKEN_INTEGER,     // 123
    LV_TOKEN_RATIONAL,    // 3/4
    LV_TOKEN_DECIMAL,     // 3.14
    LV_TOKEN_STRING,      // "hello"
    LV_TOKEN_IDENTIFIER,  // foo, PointA

    /* 关键字 */
    LV_TOKEN_KW_ANGLE,          // Angle
    LV_TOKEN_KW_AREA,           // area
    LV_TOKEN_KW_ASSERT,         // Assert
    LV_TOKEN_KW_ASSUME,         // Assume
    LV_TOKEN_KW_AXIOM,          // Axiom
    LV_TOKEN_KW_BOOL,           // Bool
    LV_TOKEN_KW_BOTTOM,         // bottom
    LV_TOKEN_KW_CIRCLE,         // Circle
    LV_TOKEN_KW_COLLINEAR,      // collinear
    LV_TOKEN_KW_COMPUTE,        // Compute
    LV_TOKEN_KW_CONGRUENT,      // congruent
    LV_TOKEN_KW_CONSTRAINT,     // Constraint
    LV_TOKEN_KW_DISTANCE,       // distance
    LV_TOKEN_KW_EXISTS,         // exists
    LV_TOKEN_KW_EXPORT,         // Export
    LV_TOKEN_KW_FALSE,          // false
    LV_TOKEN_KW_FORALL,         // forall
    LV_TOKEN_KW_IMPORT,         // import
    LV_TOKEN_KW_LENGTH,         // length
    LV_TOKEN_KW_LET,            // Let
    LV_TOKEN_KW_LINE,           // Line
    LV_TOKEN_KW_MEASURE,        // measure
    LV_TOKEN_KW_MODULE,         // module
    LV_TOKEN_KW_NORMALIZE,      // Normalize
    LV_TOKEN_KW_NOT,            // not
    LV_TOKEN_KW_OR,             // or
    LV_TOKEN_KW_AND,            // and
    LV_TOKEN_KW_PARALLEL,       // parallel
    LV_TOKEN_KW_PERPENDICULAR,  // perpendicular
    LV_TOKEN_KW_POINT,          // Point
    LV_TOKEN_KW_POLYGON,        // Polygon
    LV_TOKEN_KW_PROOF,          // Proof
    LV_TOKEN_KW_PROPOSITION,    // Proposition
    LV_TOKEN_KW_PROVE,          // Prove
    LV_TOKEN_KW_RADIUS,         // radius
    LV_TOKEN_KW_RAY,            // Ray
    LV_TOKEN_KW_SCALAR,         // Scalar
    LV_TOKEN_KW_SEGMENT,        // Segment
    LV_TOKEN_KW_TANGENT,        // tangent
    LV_TOKEN_KW_THEOREM,        // Theorem
    LV_TOKEN_KW_TRIANGLE,       // Triangle
    LV_TOKEN_KW_TRUE,           // true

    /* 运算符和分隔符 */
    LV_TOKEN_LPAREN,     // (
    LV_TOKEN_RPAREN,     // )
    LV_TOKEN_LBRACE,     // {
    LV_TOKEN_RBRACE,     // }
    LV_TOKEN_LBRACKET,   // [
    LV_TOKEN_RBRACKET,   // ]
    LV_TOKEN_SEMICOLON,  // ;
    LV_TOKEN_COMMA,      // ,
    LV_TOKEN_DOT,        // .
    LV_TOKEN_COLON,      // :
    LV_TOKEN_EQUALS,     // =
    LV_TOKEN_EQEQ,       // ==
    LV_TOKEN_NEQ,        // !=
    LV_TOKEN_LT,         // <
    LV_TOKEN_LE,         // <=
    LV_TOKEN_GT,         // >
    LV_TOKEN_GE,         // >=
    LV_TOKEN_PLUS,       // +
    LV_TOKEN_MINUS,      // -
    LV_TOKEN_STAR,       // *
    LV_TOKEN_SLASH,      // /
    LV_TOKEN_CARET,      // ^
    LV_TOKEN_ARROW,      // ->
    LV_TOKEN_DARROW,     // |-
    LV_TOKEN_MODELS,     // |=
    LV_TOKEN_THEREFORE,  // =>
    LV_TOKEN_PIPE,       // |

    /* 特殊 */
    LV_TOKEN_EOF,
    LV_TOKEN_ERROR,
    LV_TOKEN_COUNT
} LvTokenType;

/* ── 源码位置 ── */
typedef struct {
    int line;
    int column;
    size_t offset;
} LvSourceLoc;

/* ── Token ── */
typedef struct {
    LvTokenType type;
    LvSourceLoc loc;
    const char *start; /* 指向源文本中的起始位置 */
    size_t length;     /* token 文本长度 */
} LvToken;

/* ── Lexer 状态（不透明） ── */
typedef struct LvLexer LvLexer;

/* ── API ── */

/** 创建 lexer */
LvLexer *lv_lexer_create(const char *source, size_t source_len);

/** 销毁 lexer */
void lv_lexer_destroy(LvLexer *lexer);

/** 获取下一个 token */
LvToken lv_lexer_next(LvLexer *lexer);

/** 窥视下一个 token（不消费） */
LvToken lv_lexer_peek(LvLexer *lexer, int lookahead);

/** 获取当前 lexer 位置（用于错误报告） */
LvSourceLoc lv_lexer_get_loc(const LvLexer *lexer);

/** 将 token 类型转为字符串（用于调试/错误消息） */
const char *lv_token_type_name(LvTokenType type);

/** 将 token 的文本提取到缓冲区（安全） */
size_t lv_token_text(const LvToken *token, char *buf, size_t buf_size);

#ifdef __cplusplus
}
#endif

#endif /* LV_LEXER_H */

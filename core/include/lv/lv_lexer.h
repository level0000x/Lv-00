#ifndef LV_LEXER_H
#define LV_LEXER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lv/lv_xmacro.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Token 类型 ── */
/**
 * @brief X-macro 列表：LvTokenType 枚举值与显示字符串（单一事实源）
 *
 * 新增 / 删除 token 只需修改本列表一处；枚举定义、名称表（lv_lexer.c 的
 * lv_token_type_name）与计数 LV_TOKEN_COUNT 均由本列表派生，避免枚举与名称表失步。
 * 显示字符串用于错误消息 / 调试输出，与枚举标识符不一致（如 KW_ANGLE）。
 */
#define LV_TOKEN_TYPE_X(x) \
    /* 字面量 */ \
    x(LV_TOKEN_INTEGER, "INTEGER") /* 123 */ \
    x(LV_TOKEN_RATIONAL, "RATIONAL") /* 3/4 */ \
    x(LV_TOKEN_DECIMAL, "DECIMAL") /* 3.14 */ \
    x(LV_TOKEN_STRING, "STRING") /* "hello" */ \
    x(LV_TOKEN_IDENTIFIER, "IDENTIFIER") /* foo, PointA */ \
    /* 关键字 */ \
    x(LV_TOKEN_KW_ANGLE, "KW_ANGLE") /* Angle */ \
    x(LV_TOKEN_KW_AREA, "KW_AREA") /* area */ \
    x(LV_TOKEN_KW_ASSERT, "KW_ASSERT") /* Assert */ \
    x(LV_TOKEN_KW_ASSUME, "KW_ASSUME") /* Assume */ \
    x(LV_TOKEN_KW_AXIOM, "KW_AXIOM") /* Axiom */ \
    x(LV_TOKEN_KW_BOOL, "KW_BOOL") /* Bool */ \
    x(LV_TOKEN_KW_BOTTOM, "KW_BOTTOM") /* bottom */ \
    x(LV_TOKEN_KW_CIRCLE, "KW_CIRCLE") /* Circle */ \
    x(LV_TOKEN_KW_COLLINEAR, "KW_COLLINEAR") /* collinear */ \
    x(LV_TOKEN_KW_COMPUTE, "KW_COMPUTE") /* Compute */ \
    x(LV_TOKEN_KW_CONGRUENT, "KW_CONGRUENT") /* congruent */ \
    x(LV_TOKEN_KW_CONSTRAINT, "KW_CONSTRAINT") /* Constraint */ \
    x(LV_TOKEN_KW_DISTANCE, "KW_DISTANCE") /* distance */ \
    x(LV_TOKEN_KW_EXISTS, "KW_EXISTS") /* exists */ \
    x(LV_TOKEN_KW_EXPORT, "KW_EXPORT") /* Export */ \
    x(LV_TOKEN_KW_FALSE, "KW_FALSE") /* false */ \
    x(LV_TOKEN_KW_FORALL, "KW_FORALL") /* forall */ \
    x(LV_TOKEN_KW_IMPORT, "KW_IMPORT") /* import */ \
    x(LV_TOKEN_KW_LENGTH, "KW_LENGTH") /* length */ \
    x(LV_TOKEN_KW_LET, "KW_LET") /* Let */ \
    x(LV_TOKEN_KW_LINE, "KW_LINE") /* Line */ \
    x(LV_TOKEN_KW_MEASURE, "KW_MEASURE") /* measure */ \
    x(LV_TOKEN_KW_MODULE, "KW_MODULE") /* module */ \
    x(LV_TOKEN_KW_NORMALIZE, "KW_NORMALIZE") /* Normalize */ \
    x(LV_TOKEN_KW_NOT, "KW_NOT") /* not */ \
    x(LV_TOKEN_KW_OR, "KW_OR") /* or */ \
    x(LV_TOKEN_KW_AND, "KW_AND") /* and */ \
    x(LV_TOKEN_KW_PARALLEL, "KW_PARALLEL") /* parallel */ \
    x(LV_TOKEN_KW_PERPENDICULAR, "KW_PERPENDICULAR") /* perpendicular */ \
    x(LV_TOKEN_KW_POINT, "KW_POINT") /* Point */ \
    x(LV_TOKEN_KW_POLYGON, "KW_POLYGON") /* Polygon */ \
    x(LV_TOKEN_KW_PROOF, "KW_PROOF") /* Proof */ \
    x(LV_TOKEN_KW_PROPOSITION, "KW_PROPOSITION") /* Proposition */ \
    x(LV_TOKEN_KW_PROVE, "KW_PROVE") /* Prove */ \
    x(LV_TOKEN_KW_RADIUS, "KW_RADIUS") /* radius */ \
    x(LV_TOKEN_KW_RAY, "KW_RAY") /* Ray */ \
    x(LV_TOKEN_KW_SCALAR, "KW_SCALAR") /* Scalar */ \
    x(LV_TOKEN_KW_SEGMENT, "KW_SEGMENT") /* Segment */ \
    x(LV_TOKEN_KW_TANGENT, "KW_TANGENT") /* tangent */ \
    x(LV_TOKEN_KW_THEOREM, "KW_THEOREM") /* Theorem */ \
    x(LV_TOKEN_KW_TRIANGLE, "KW_TRIANGLE") /* Triangle */ \
    x(LV_TOKEN_KW_TRUE, "KW_TRUE") /* true */ \
    /* 运算符和分隔符 */ \
    x(LV_TOKEN_LPAREN, "LPAREN") /* ( */ \
    x(LV_TOKEN_RPAREN, "RPAREN") /* ) */ \
    x(LV_TOKEN_LBRACE, "LBRACE") /* { */ \
    x(LV_TOKEN_RBRACE, "RBRACE") /* } */ \
    x(LV_TOKEN_LBRACKET, "LBRACKET") /* [ */ \
    x(LV_TOKEN_RBRACKET, "RBRACKET") /* ] */ \
    x(LV_TOKEN_SEMICOLON, "SEMICOLON") /* ; */ \
    x(LV_TOKEN_COMMA, "COMMA") /* , */ \
    x(LV_TOKEN_DOT, "DOT") /* . */ \
    x(LV_TOKEN_COLON, "COLON") /* : */ \
    x(LV_TOKEN_EQUALS, "EQUALS") /* = */ \
    x(LV_TOKEN_EQEQ, "EQEQ") /* == */ \
    x(LV_TOKEN_NEQ, "NEQ") /* != */ \
    x(LV_TOKEN_LT, "LT") /* < */ \
    x(LV_TOKEN_LE, "LE") /* <= */ \
    x(LV_TOKEN_GT, "GT") /* > */ \
    x(LV_TOKEN_GE, "GE") /* >= */ \
    x(LV_TOKEN_PLUS, "PLUS") /* + */ \
    x(LV_TOKEN_MINUS, "MINUS") /* - */ \
    x(LV_TOKEN_STAR, "STAR") /* * */ \
    x(LV_TOKEN_SLASH, "SLASH") /* / */ \
    x(LV_TOKEN_CARET, "CARET") /* ^ */ \
    x(LV_TOKEN_ARROW, "ARROW") /* -> */ \
    x(LV_TOKEN_DARROW, "DARROW") /* |- */ \
    x(LV_TOKEN_MODELS, "MODELS") /* |= */ \
    x(LV_TOKEN_THEREFORE, "THEREFORE") /* => */ \
    x(LV_TOKEN_PIPE, "PIPE") /* | */ \
    /* 特殊 */ \
    x(LV_TOKEN_EOF, "EOF") \
    x(LV_TOKEN_ERROR, "ERROR")

typedef enum {
    LV_TOKEN_TYPE_X(LV_X_ENUM_ITEM)
} LvTokenType;

/* Token 类型数量：由 LV_TOKEN_TYPE_X 单源计数 */
#define LV_X_TOKEN_COUNT_ITEM(name, str) +1
#define LV_TOKEN_COUNT (0 LV_TOKEN_TYPE_X(LV_X_TOKEN_COUNT_ITEM))

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

/* ── 共享几何关键词表（parser/sema 单一事实源，定义见 lv_lexer.c） ── */

/** 几何关系关键词表（collinear/parallel/perpendicular/congruent/tangent，NULL 结尾） */
extern const char *const lv_geometry_relation_keywords[];

/** 几何度量关键词表（length/distance/angle/measure/area/radius，NULL 结尾） */
extern const char *const lv_measurement_keywords[];

#ifdef __cplusplus
}
#endif

#endif /* LV_LEXER_H */

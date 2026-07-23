#include "lv/lv_lexer.h"
#include "lv_utils.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

struct LvLexer {
    const char *source;
    size_t      source_len;
    size_t      pos;
    int         line;
    int         column;
    LvToken     peek_buf[3];
    int         peek_count;
};

/* ── 关键字查找表 ── */
typedef struct {
    const char *word;
    LvTokenType type;
} KeywordEntry;

static const KeywordEntry s_keywords[] = {
    {"Angle",        LV_TOKEN_KW_ANGLE},
    {"Assert",       LV_TOKEN_KW_ASSERT},
    {"Assume",       LV_TOKEN_KW_ASSUME},
    {"Axiom",        LV_TOKEN_KW_AXIOM},
    {"Bool",         LV_TOKEN_KW_BOOL},
    {"Circle",       LV_TOKEN_KW_CIRCLE},
    {"Compute",      LV_TOKEN_KW_COMPUTE},
    {"Constraint",   LV_TOKEN_KW_CONSTRAINT},
    {"Export",       LV_TOKEN_KW_EXPORT},
    {"Let",          LV_TOKEN_KW_LET},
    {"Line",         LV_TOKEN_KW_LINE},
    {"Normalize",    LV_TOKEN_KW_NORMALIZE},
    {"Point",        LV_TOKEN_KW_POINT},
    {"Polygon",      LV_TOKEN_KW_POLYGON},
    {"Proof",        LV_TOKEN_KW_PROOF},
    {"Proposition",  LV_TOKEN_KW_PROPOSITION},
    {"Prove",        LV_TOKEN_KW_PROVE},
    {"Ray",          LV_TOKEN_KW_RAY},
    {"Scalar",       LV_TOKEN_KW_SCALAR},
    {"Segment",      LV_TOKEN_KW_SEGMENT},
    {"Theorem",      LV_TOKEN_KW_THEOREM},
    {"Triangle",     LV_TOKEN_KW_TRIANGLE},
    {"and",          LV_TOKEN_KW_AND},
    {"area",         LV_TOKEN_KW_AREA},
    {"bottom",       LV_TOKEN_KW_BOTTOM},
    {"collinear",    LV_TOKEN_KW_COLLINEAR},
    {"congruent",    LV_TOKEN_KW_CONGRUENT},
    {"distance",     LV_TOKEN_KW_DISTANCE},
    {"exists",       LV_TOKEN_KW_EXISTS},
    {"false",        LV_TOKEN_KW_FALSE},
    {"forall",       LV_TOKEN_KW_FORALL},
    {"import",       LV_TOKEN_KW_IMPORT},
    {"length",       LV_TOKEN_KW_LENGTH},
    {"measure",      LV_TOKEN_KW_MEASURE},
    {"module",       LV_TOKEN_KW_MODULE},
    {"not",          LV_TOKEN_KW_NOT},
    {"or",           LV_TOKEN_KW_OR},
    {"parallel",     LV_TOKEN_KW_PARALLEL},
    {"perpendicular",LV_TOKEN_KW_PERPENDICULAR},
    {"radius",       LV_TOKEN_KW_RADIUS},
    {"tangent",      LV_TOKEN_KW_TANGENT},
    {"true",         LV_TOKEN_KW_TRUE},
};

static LvTokenType lookup_keyword(const char *word, size_t len) {
    for (size_t i = 0; i < sizeof(s_keywords) / sizeof(s_keywords[0]); i++) {
        if (strlen(s_keywords[i].word) == len &&
            strncmp(s_keywords[i].word, word, len) == 0) {
            return s_keywords[i].type;
        }
    }
    return LV_TOKEN_IDENTIFIER;
}

static LvToken make_token(LvLexer *lexer, LvTokenType type, size_t start_pos, size_t length) {
    LvToken tok;
    tok.type = type;
    tok.loc.offset = start_pos;
    tok.loc.line = lexer->line;
    tok.loc.column = lexer->column;
    tok.start = lexer->source + start_pos;
    tok.length = length;
    return tok;
}

static void skip_whitespace_and_comments(LvLexer *lexer) {
    while (lexer->pos < lexer->source_len) {
        char c = lexer->source[lexer->pos];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            if (c == '\n') { lexer->line++; lexer->column = 1; }
            else { lexer->column++; }
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
                    if (lexer->source[lexer->pos] == '\n') { lexer->line++; lexer->column = 1; }
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

static LvToken lex_raw(LvLexer *lexer) {
    skip_whitespace_and_comments(lexer);

    if (lexer->pos >= lexer->source_len)
        return make_token(lexer, LV_TOKEN_EOF, lexer->pos, 0);

    size_t start = lexer->pos;
    char c = lexer->source[lexer->pos];
    lexer->column = (int)(lexer->pos - start) + 1;

    /* 标识符 / 关键字 */
    if (isalpha((unsigned char)c) || c == '_') {
        while (lexer->pos < lexer->source_len &&
               (isalnum((unsigned char)lexer->source[lexer->pos]) || lexer->source[lexer->pos] == '_'))
            lexer->pos++;
        size_t len = lexer->pos - start;
        LvTokenType type = lookup_keyword(lexer->source + start, len);
        lexer->column += (int)len;
        return make_token(lexer, type, start, len);
    }

    /* 数字 */
    if (isdigit((unsigned char)c)) {
        while (lexer->pos < lexer->source_len && isdigit((unsigned char)lexer->source[lexer->pos]))
            lexer->pos++;
        /* 有理数: 3/4 */
        if (lexer->pos < lexer->source_len && lexer->source[lexer->pos] == '/') {
            lexer->pos++;
            if (lexer->pos < lexer->source_len && isdigit((unsigned char)lexer->source[lexer->pos])) {
                while (lexer->pos < lexer->source_len && isdigit((unsigned char)lexer->source[lexer->pos]))
                    lexer->pos++;
                return make_token(lexer, LV_TOKEN_RATIONAL, start, lexer->pos - start);
            }
            lexer->pos = start;
            while (lexer->pos < lexer->source_len && isdigit((unsigned char)lexer->source[lexer->pos]))
                lexer->pos++;
            return make_token(lexer, LV_TOKEN_INTEGER, start, lexer->pos - start);
        }
        /* 小数: 3.14 */
        if (lexer->pos < lexer->source_len && lexer->source[lexer->pos] == '.') {
            size_t dot_pos = lexer->pos;
            lexer->pos++;
            if (lexer->pos < lexer->source_len && isdigit((unsigned char)lexer->source[lexer->pos])) {
                while (lexer->pos < lexer->source_len && isdigit((unsigned char)lexer->source[lexer->pos]))
                    lexer->pos++;
                return make_token(lexer, LV_TOKEN_DECIMAL, start, lexer->pos - start);
            }
            lexer->pos = dot_pos;
        }
        return make_token(lexer, LV_TOKEN_INTEGER, start, lexer->pos - start);
    }

    /* 字符串 */
    if (c == '"') {
        lexer->pos++;
        while (lexer->pos < lexer->source_len && lexer->source[lexer->pos] != '"') {
            if (lexer->source[lexer->pos] == '\\') lexer->pos++;
            lexer->pos++;
        }
        if (lexer->pos < lexer->source_len) lexer->pos++;
        return make_token(lexer, LV_TOKEN_STRING, start, lexer->pos - start);
    }

    /* 多字符运算符 */
    if (c == '-' && lexer->pos + 1 < lexer->source_len && lexer->source[lexer->pos + 1] == '>') {
        lexer->pos += 2;
        return make_token(lexer, LV_TOKEN_ARROW, start, 2);
    }
    if (c == '=' && lexer->pos + 1 < lexer->source_len && lexer->source[lexer->pos + 1] == '=') {
        lexer->pos += 2;
        return make_token(lexer, LV_TOKEN_EQEQ, start, 2);
    }
    if (c == '!' && lexer->pos + 1 < lexer->source_len && lexer->source[lexer->pos + 1] == '=') {
        lexer->pos += 2;
        return make_token(lexer, LV_TOKEN_NEQ, start, 2);
    }
    if (c == '<' && lexer->pos + 1 < lexer->source_len && lexer->source[lexer->pos + 1] == '=') {
        lexer->pos += 2;
        return make_token(lexer, LV_TOKEN_LE, start, 2);
    }
    if (c == '>' && lexer->pos + 1 < lexer->source_len && lexer->source[lexer->pos + 1] == '=') {
        lexer->pos += 2;
        return make_token(lexer, LV_TOKEN_GE, start, 2);
    }
    if (c == '=' && lexer->pos + 1 < lexer->source_len && lexer->source[lexer->pos + 1] == '>') {
        lexer->pos += 2;
        return make_token(lexer, LV_TOKEN_THEREFORE, start, 2);
    }
    if (c == '|' && lexer->pos + 1 < lexer->source_len) {
        char n = lexer->source[lexer->pos + 1];
        if (n == '-') { lexer->pos += 2; return make_token(lexer, LV_TOKEN_DARROW, start, 2); }
        if (n == '=') { lexer->pos += 2; return make_token(lexer, LV_TOKEN_MODELS, start, 2); }
    }

    /* 单字符运算符 */
    lexer->pos++;
    switch (c) {
        case '(': return make_token(lexer, LV_TOKEN_LPAREN, start, 1);
        case ')': return make_token(lexer, LV_TOKEN_RPAREN, start, 1);
        case '{': return make_token(lexer, LV_TOKEN_LBRACE, start, 1);
        case '}': return make_token(lexer, LV_TOKEN_RBRACE, start, 1);
        case '[': return make_token(lexer, LV_TOKEN_LBRACKET, start, 1);
        case ']': return make_token(lexer, LV_TOKEN_RBRACKET, start, 1);
        case ';': return make_token(lexer, LV_TOKEN_SEMICOLON, start, 1);
        case ',': return make_token(lexer, LV_TOKEN_COMMA, start, 1);
        case '.': return make_token(lexer, LV_TOKEN_DOT, start, 1);
        case ':': return make_token(lexer, LV_TOKEN_COLON, start, 1);
        case '=': return make_token(lexer, LV_TOKEN_EQUALS, start, 1);
        case '+': return make_token(lexer, LV_TOKEN_PLUS, start, 1);
        case '-': return make_token(lexer, LV_TOKEN_MINUS, start, 1);
        case '*': return make_token(lexer, LV_TOKEN_STAR, start, 1);
        case '/': return make_token(lexer, LV_TOKEN_SLASH, start, 1);
        case '^': return make_token(lexer, LV_TOKEN_CARET, start, 1);
        case '<': return make_token(lexer, LV_TOKEN_LT, start, 1);
        case '>': return make_token(lexer, LV_TOKEN_GT, start, 1);
        default:
            return make_token(lexer, LV_TOKEN_ERROR, start, 1);
    }
}

/* ── 公共 API ── */

LvLexer *lv_lexer_create(const char *source, size_t source_len) {
    LvLexer *lexer = (LvLexer *)lv_malloc(sizeof(LvLexer));
    if (!lexer) return NULL;
    lexer->source = source;
    lexer->source_len = source_len;
    lexer->pos = 0;
    lexer->line = 1;
    lexer->column = 1;
    lexer->peek_count = 0;
    return lexer;
}

void lv_lexer_destroy(LvLexer *lexer) {
    lv_free((void **)&lexer);
}

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

LvToken lv_lexer_peek(LvLexer *lexer, int lookahead) {
    while (lexer->peek_count <= lookahead) {
        lexer->peek_buf[lexer->peek_count++] = lex_raw(lexer);
    }
    return lexer->peek_buf[lookahead];
}

LvSourceLoc lv_lexer_get_loc(const LvLexer *lexer) {
    LvSourceLoc loc;
    loc.line = lexer->line;
    loc.column = lexer->column;
    loc.offset = lexer->pos;
    return loc;
}

const char *lv_token_type_name(LvTokenType type) {
    static const char *names[] = {
        "INTEGER", "RATIONAL", "DECIMAL", "STRING", "IDENTIFIER",
        "KW_ANGLE", "KW_AREA", "KW_ASSERT", "KW_ASSUME", "KW_AXIOM",
        "KW_BOOL", "KW_BOTTOM", "KW_CIRCLE", "KW_COLLINEAR", "KW_COMPUTE",
        "KW_CONGRUENT", "KW_CONSTRAINT", "KW_DISTANCE", "KW_EXISTS", "KW_EXPORT",
        "KW_FALSE", "KW_FORALL", "KW_IMPORT",
        "KW_LENGTH", "KW_LET", "KW_LINE", "KW_MEASURE", "KW_MODULE",
        "KW_NORMALIZE", "KW_NOT", "KW_OR", "KW_AND",
        "KW_PARALLEL", "KW_PERPENDICULAR", "KW_POINT", "KW_POLYGON",
        "KW_PROOF", "KW_PROPOSITION", "KW_PROVE", "KW_RADIUS", "KW_RAY",
        "KW_SCALAR", "KW_SEGMENT", "KW_TANGENT", "KW_THEOREM", "KW_TRIANGLE",
        "KW_TRUE",
        "LPAREN", "RPAREN", "LBRACE", "RBRACE", "LBRACKET", "RBRACKET",
        "SEMICOLON", "COMMA", "DOT", "COLON",
        "EQUALS", "EQEQ", "NEQ", "LT", "LE", "GT", "GE",
        "PLUS", "MINUS", "STAR", "SLASH", "CARET",
        "ARROW", "DARROW", "MODELS", "THEREFORE",
        "EOF", "ERROR"
    };
    if (type >= 0 && type < LV_TOKEN_COUNT)
        return names[type];
    return "UNKNOWN";
}

size_t lv_token_text(const LvToken *token, char *buf, size_t buf_size) {
    if (!token || !buf || buf_size == 0) return 0;
    size_t copy_len = token->length < buf_size - 1 ? token->length : buf_size - 1;
    strncpy(buf, token->start, copy_len);
    buf[copy_len] = '\0';
    return copy_len;
}

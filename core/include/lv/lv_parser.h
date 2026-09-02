#ifndef LV_PARSER_H
#define LV_PARSER_H

#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */
#include "lv/lv_ast.h"
#include "lv/lv_lexer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LvParser LvParser;

typedef struct {
    LvSourceLoc loc;
    char message[256];
} LvParseError;

typedef struct {
    LvAstNode *ast;
    int error_count;
    LvParseError errors[64];
} LvParseResult;

LvParser *lv_parser_create(LvLexer *lexer);
lv_PUBLIC_API void lv_parser_destroy(LvParser *parser);
LvParseResult lv_parser_parse_program(LvParser *parser);

#ifdef __cplusplus
}
#endif

#endif /* LV_PARSER_H */

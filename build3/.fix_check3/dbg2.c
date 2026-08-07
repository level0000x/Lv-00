#include <stdio.h>
#include <string.h>
#include "lv/lv_parser.h"
#include "lv/lv_lexer.h"
#include "lv/lv_ast.h"

int main(void) {
    const char *src =
        "Point Verdict := Pass(proof: ProofTerm);\n"
        "Point VerifyFn := verify(output: Output) -> Verdict;\n"
        "Point Spec := { nested: List<List<Formula>> };\n";
    LvLexer *lex = lv_lexer_create(src, strlen(src));
    LvParser *parser = lv_parser_create(lex);
    LvParseResult res = lv_parser_parse_program(parser);
    printf("parse errors=%d\n", res.error_count);
    if (res.ast)
        lv_ast_print(res.ast, 0);
    if (res.ast) lv_ast_destroy(res.ast);
    lv_parser_destroy(parser);
    lv_lexer_destroy(lex);
    return 0;
}

#include <stdio.h>
#include <string.h>
#include "lv/lv_parser.h"
#include "lv/lv_lexer.h"
#include "lv/lv_ast.h"
#include "lv/lv_loader.h"

static void verify(const char *label, const char *src) {
    LvLexer *lex = lv_lexer_create(src, strlen(src));
    LvParser *parser = lv_parser_create(lex);
    LvParseResult res = lv_parser_parse_program(parser);
    printf("== %s parse errors=%d\n", label, res.error_count);
    LvProveSummary s;
    memset(&s, 0, sizeof(s));
    if (res.ast && res.error_count == 0) {
        lv_verify_proofs(&res, &s);
        printf("   prove=%d pass=%d fail=%d skip=%d\n", s.prove_count, s.pass_count, s.fail_count, s.skip_count);
        for (int i = 0; i < s.prove_count; i++)
            printf("   rep %d: verdict=%d detail=%s\n", i, (int) s.reports[i].verdict, s.reports[i].detail);
    }
    if (res.ast) lv_ast_destroy(res.ast);
    lv_parser_destroy(parser);
    lv_lexer_destroy(lex);
}

int main(void) {
    verify("P or not P", "Prove P or not P;\n");
    verify("P and not P", "Prove P and not P;\n");
    verify("not (not P) -> P", "Prove not (not P) -> P;\n");
    return 0;
}

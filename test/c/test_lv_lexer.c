#include <stdio.h>
#include <string.h>
#include "lv/lv_lexer.h"

#define TEST(n) printf("  [TEST] %s ... ", n)
#define PASS() do { printf("PASS\n"); P++; } while(0)
#define FAIL(m) do { printf("FAIL: %s\n", m); F++; } while(0)

static int P = 0, F = 0;

static void test_single_token(const char *src, LvTokenType expected) {
    LvLexer *lex = lv_lexer_create(src, strlen(src));
    LvToken tok = lv_lexer_next(lex);
    if (tok.type != expected) {
        printf("  expected %s, got %s\n",
               lv_token_type_name(expected), lv_token_type_name(tok.type));
        lv_lexer_destroy(lex);
        FAIL("类型不匹配");
        return;
    }
    lv_lexer_destroy(lex);
    PASS();
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== lv lexer test ===\n\n");

    /* 关键字 */
    printf("[关键字]\n");
    test_single_token("Point", LV_TOKEN_KW_POINT);
    test_single_token("Constraint", LV_TOKEN_KW_CONSTRAINT);
    test_single_token("Prove", LV_TOKEN_KW_PROVE);
    test_single_token("forall", LV_TOKEN_KW_FORALL);
    test_single_token("exists", LV_TOKEN_KW_EXISTS);
    test_single_token("Let", LV_TOKEN_KW_LET);
    test_single_token("Line", LV_TOKEN_KW_LINE);
    test_single_token("Circle", LV_TOKEN_KW_CIRCLE);
    test_single_token("Segment", LV_TOKEN_KW_SEGMENT);
    test_single_token("Triangle", LV_TOKEN_KW_TRIANGLE);
    test_single_token("Angle", LV_TOKEN_KW_ANGLE);
    test_single_token("Normalize", LV_TOKEN_KW_NORMALIZE);
    test_single_token("Theorem", LV_TOKEN_KW_THEOREM);
    test_single_token("Axiom", LV_TOKEN_KW_AXIOM);
    test_single_token("true", LV_TOKEN_KW_TRUE);
    test_single_token("false", LV_TOKEN_KW_FALSE);
    test_single_token("and", LV_TOKEN_KW_AND);
    test_single_token("or", LV_TOKEN_KW_OR);
    test_single_token("not", LV_TOKEN_KW_NOT);
    test_single_token("collinear", LV_TOKEN_KW_COLLINEAR);
    test_single_token("parallel", LV_TOKEN_KW_PARALLEL);
    test_single_token("perpendicular", LV_TOKEN_KW_PERPENDICULAR);
    test_single_token("congruent", LV_TOKEN_KW_CONGRUENT);
    test_single_token("length", LV_TOKEN_KW_LENGTH);
    test_single_token("distance", LV_TOKEN_KW_DISTANCE);
    test_single_token("area", LV_TOKEN_KW_AREA);
    test_single_token("measure", LV_TOKEN_KW_MEASURE);
    test_single_token("radius", LV_TOKEN_KW_RADIUS);
    test_single_token("tangent", LV_TOKEN_KW_TANGENT);
    test_single_token("module", LV_TOKEN_KW_MODULE);
    test_single_token("import", LV_TOKEN_KW_IMPORT);
    test_single_token("Bool", LV_TOKEN_KW_BOOL);
    test_single_token("Scalar", LV_TOKEN_KW_SCALAR);
    test_single_token("Proof", LV_TOKEN_KW_PROOF);
    test_single_token("Proposition", LV_TOKEN_KW_PROPOSITION);
    test_single_token("Polygon", LV_TOKEN_KW_POLYGON);
    test_single_token("Ray", LV_TOKEN_KW_RAY);
    test_single_token("Compute", LV_TOKEN_KW_COMPUTE);
    test_single_token("Assert", LV_TOKEN_KW_ASSERT);
    test_single_token("Assume", LV_TOKEN_KW_ASSUME);
    test_single_token("Export", LV_TOKEN_KW_EXPORT);
    test_single_token("bottom", LV_TOKEN_KW_BOTTOM);

    /* 标识符 */
    printf("[标识符]\n");
    test_single_token("foo", LV_TOKEN_IDENTIFIER);
    test_single_token("A", LV_TOKEN_IDENTIFIER);
    test_single_token("my_var_1", LV_TOKEN_IDENTIFIER);
    test_single_token("implies", LV_TOKEN_IDENTIFIER);
    test_single_token("iff", LV_TOKEN_IDENTIFIER);

    /* 字面量 */
    printf("[字面量]\n");
    test_single_token("123", LV_TOKEN_INTEGER);
    test_single_token("3/4", LV_TOKEN_RATIONAL);
    test_single_token("3.14", LV_TOKEN_DECIMAL);
    test_single_token("\"hello\"", LV_TOKEN_STRING);

    /* 运算符 */
    printf("[运算符/分隔符]\n");
    test_single_token("(", LV_TOKEN_LPAREN);
    test_single_token(")", LV_TOKEN_RPAREN);
    test_single_token("{", LV_TOKEN_LBRACE);
    test_single_token("}", LV_TOKEN_RBRACE);
    test_single_token("[", LV_TOKEN_LBRACKET);
    test_single_token("]", LV_TOKEN_RBRACKET);
    test_single_token(";", LV_TOKEN_SEMICOLON);
    test_single_token(",", LV_TOKEN_COMMA);
    test_single_token(".", LV_TOKEN_DOT);
    test_single_token(":", LV_TOKEN_COLON);
    test_single_token("=", LV_TOKEN_EQUALS);
    test_single_token("==", LV_TOKEN_EQEQ);
    test_single_token("!=", LV_TOKEN_NEQ);
    test_single_token("<", LV_TOKEN_LT);
    test_single_token("<=", LV_TOKEN_LE);
    test_single_token(">", LV_TOKEN_GT);
    test_single_token(">=", LV_TOKEN_GE);
    test_single_token("+", LV_TOKEN_PLUS);
    test_single_token("-", LV_TOKEN_MINUS);
    test_single_token("*", LV_TOKEN_STAR);
    test_single_token("/", LV_TOKEN_SLASH);
    test_single_token("^", LV_TOKEN_CARET);
    test_single_token("->", LV_TOKEN_ARROW);
    test_single_token("=>", LV_TOKEN_THEREFORE);
    test_single_token("|-", LV_TOKEN_DARROW);
    test_single_token("|=", LV_TOKEN_MODELS);

    /* 注释 */
    printf("[注释]\n");
    {
        const char *s = "// line comment\nPoint";
        LvLexer *lex = lv_lexer_create(s, strlen(s));
        LvToken tok = lv_lexer_next(lex);
        TEST("行注释后");
        if (tok.type == LV_TOKEN_KW_POINT) PASS(); else FAIL("注释未跳过");
        lv_lexer_destroy(lex);
    }
    {
        const char *s = "/* block\ncomment */Point";
        LvLexer *lex = lv_lexer_create(s, strlen(s));
        LvToken tok = lv_lexer_next(lex);
        TEST("块注释后");
        if (tok.type == LV_TOKEN_KW_POINT) PASS(); else FAIL("块注释未跳过");
        lv_lexer_destroy(lex);
    }

    /* 完整语句 */
    {
        printf("[完整语句]\n");
        const char *src = "Point A, B, C;\nConstraint collinear(A, B, C);\nProve true;";
        LvLexer *lex = lv_lexer_create(src, strlen(src));
        LvToken tok;
        int count = 0;
        LvTokenType expected[] = {
            LV_TOKEN_KW_POINT, LV_TOKEN_IDENTIFIER, LV_TOKEN_COMMA,
            LV_TOKEN_IDENTIFIER, LV_TOKEN_COMMA, LV_TOKEN_IDENTIFIER,
            LV_TOKEN_SEMICOLON,
            LV_TOKEN_KW_CONSTRAINT, LV_TOKEN_KW_COLLINEAR,
            LV_TOKEN_LPAREN, LV_TOKEN_IDENTIFIER, LV_TOKEN_COMMA,
            LV_TOKEN_IDENTIFIER, LV_TOKEN_COMMA, LV_TOKEN_IDENTIFIER,
            LV_TOKEN_RPAREN, LV_TOKEN_SEMICOLON,
            LV_TOKEN_KW_PROVE, LV_TOKEN_KW_TRUE, LV_TOKEN_SEMICOLON,
            LV_TOKEN_EOF
        };
        int expected_count = sizeof(expected) / sizeof(expected[0]);
        bool ok = true;
        while (count < expected_count) {
            tok = lv_lexer_next(lex);
            if (tok.type != expected[count]) {
                ok = false;
            }
            count++;
            if (tok.type == LV_TOKEN_EOF) break;
        }
        TEST("完整语句 token 序列");
        if (ok && count == expected_count) PASS(); else FAIL("token 序列不匹配");
        lv_lexer_destroy(lex);
    }

    /* EOF */
    printf("[边界]\n");
    {
        LvLexer *lex = lv_lexer_create("", 0);
        LvToken tok = lv_lexer_next(lex);
        TEST("空输入 EOF");
        if (tok.type == LV_TOKEN_EOF) PASS(); else FAIL("!EOF");
        lv_lexer_destroy(lex);
    }

    printf("\n=== %d passed, %d failed ===\n", P, F);
    return F > 0 ? 1 : 0;
}

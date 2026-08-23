/**
 * @file test_lv_ast_ext.c
 * @brief AST 节点契约测试（批次 C-㊺续27：lv_ast.h 8 个零覆盖 API）
 *
 * 覆盖零覆盖 API：
 *   字面量：create_bool / create_decimal / create_rational / create_string
 *   调用：create_call_typed
 *   逻辑：create_logic_binary
 *   一元：create_unary
 *   打印：print
 *
 * 契约要点（与实现核对）：
 *   - create_rational：num/den 存入 literal.rational_value。
 *   - create_decimal：double 值。
 *   - create_bool：int 值。
 *   - create_string：字符串深拷贝。
 *   - create_call_typed：args 按序链接为链表，child_count=arg_count。
 *   - create_logic_binary：op 复制到 binary.op，left/right。
 *   - create_unary：op 复制到 unary.op，operand。
 *   - print：递归打印不崩溃。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/lv_ast.h"

int g_pass_count = 0;
int g_fail_count = 0;

static LvSourceLoc loc = {1, 2};

/* ============== 测试：字面量 ============== */

static void test_ast_literal_api(void) {
    /* rational：num/den */
    LvAstNode *r = lv_ast_create_rational(loc, 3, 4);
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQ((int) r->type, (int) LV_AST_RATIONAL_LITERAL);
    TEST_ASSERT_EQ(r->data.literal.rational_value.num, 3);
    TEST_ASSERT_EQ(r->data.literal.rational_value.den, 4);
    TEST_ASSERT_EQ(r->loc.line, 1);
    lv_ast_destroy(r);

    /* decimal */
    LvAstNode *d = lv_ast_create_decimal(loc, 2.5);
    TEST_ASSERT_NOT_NULL(d);
    TEST_ASSERT_EQ((int) d->type, (int) LV_AST_DECIMAL_LITERAL);
    TEST_ASSERT_DOUBLE(d->data.literal.decimal_value, 2.5, 1e-12);
    lv_ast_destroy(d);

    /* bool */
    LvAstNode *b = lv_ast_create_bool(loc, 1);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_EQ((int) b->type, (int) LV_AST_BOOL_LITERAL);
    TEST_ASSERT_EQ(b->data.literal.bool_value, 1);
    lv_ast_destroy(b);
    LvAstNode *bf = lv_ast_create_bool(loc, 0);
    TEST_ASSERT_EQ(bf->data.literal.bool_value, 0);
    lv_ast_destroy(bf);

    /* string：深拷贝 */
    LvAstNode *s = lv_ast_create_string(loc, "hello");
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQ((int) s->type, (int) LV_AST_STRING_LITERAL);
    TEST_ASSERT_NOT_NULL(s->data.literal.string_value);
    TEST_ASSERT(strcmp(s->data.literal.string_value, "hello") == 0, "字符串值");
    TEST_ASSERT(s->data.literal.string_value != (void *) "hello", "深拷贝非字面量指针");
    lv_ast_destroy(s);

    printf("  test_ast_literal_api: PASSED\n");
}

/* ============== 测试：调用与逻辑 ============== */

static void test_ast_call_logic_api(void) {
    /* create_call_typed：3 参数 */
    LvAstNode *a1 = lv_ast_create_int(loc, 1);
    LvAstNode *a2 = lv_ast_create_int(loc, 2);
    LvAstNode *a3 = lv_ast_create_int(loc, 3);
    LvAstNode *const args[3] = {a1, a2, a3};

    LvAstNode *call = lv_ast_create_call_typed(LV_AST_FUNCTION_CALL, loc, "f", args, 3);
    TEST_ASSERT_NOT_NULL(call);
    TEST_ASSERT_EQ((int) call->type, (int) LV_AST_FUNCTION_CALL);
    TEST_ASSERT(strcmp(call->data.call.func_name, "f") == 0, "函数名");
    TEST_ASSERT_EQ(call->child_count, 3);
    TEST_ASSERT_NOT_NULL(call->child);
    /* 链表顺序 a1 -> a2 -> a3 */
    TEST_ASSERT_EQ(call->child, a1);
    TEST_ASSERT_EQ(call->child->next, a2);
    TEST_ASSERT_EQ(call->child->next->next, a3);
    TEST_ASSERT_NULL(call->child->next->next->next);
    lv_ast_destroy(call);

    /* 空参数 */
    LvAstNode *call0 = lv_ast_create_call_typed(LV_AST_FUNCTION_CALL, loc, "g", NULL, 0);
    TEST_ASSERT_NOT_NULL(call0);
    TEST_ASSERT_EQ(call0->child_count, 0);
    TEST_ASSERT_NULL(call0->child);
    lv_ast_destroy(call0);

    /* create_logic_binary：AND */
    LvAstNode *l = lv_ast_create_bool(loc, 1);
    LvAstNode *rr = lv_ast_create_bool(loc, 0);
    LvAstNode *lb = lv_ast_create_logic_binary(LV_AST_LOGIC_AND, loc, "and", l, rr);
    TEST_ASSERT_NOT_NULL(lb);
    TEST_ASSERT_EQ((int) lb->type, (int) LV_AST_LOGIC_AND);
    TEST_ASSERT(strcmp(lb->data.binary.op, "and") == 0, "逻辑运算符");
    TEST_ASSERT_EQ(lb->data.binary.left, l);
    TEST_ASSERT_EQ(lb->data.binary.right, rr);
    lv_ast_destroy(lb);

    /* create_unary：NOT */
    LvAstNode *operand = lv_ast_create_bool(loc, 1);
    LvAstNode *un = lv_ast_create_unary(loc, "not", operand);
    TEST_ASSERT_NOT_NULL(un);
    TEST_ASSERT_EQ((int) un->type, (int) LV_AST_UNARY_OP);
    TEST_ASSERT(strcmp(un->data.unary.op, "not") == 0, "一元运算符");
    TEST_ASSERT_EQ(un->data.unary.operand, operand);
    lv_ast_destroy(un);

    printf("  test_ast_call_logic_api: PASSED\n");
}

/* ============== 测试：打印 ============== */

static void test_ast_print_api(void) {
    /* 构造小树：f(a, 3/4) */
    LvAstNode *a1 = lv_ast_create_ident(loc, "a");
    LvAstNode *a2 = lv_ast_create_rational(loc, 3, 4);
    LvAstNode *const args[2] = {a1, a2};
    LvAstNode *call = lv_ast_create_call_typed(LV_AST_FUNCTION_CALL, loc, "f", args, 2);

    /* print 递归输出不崩溃 */
    lv_ast_print(call, 0);

    /* 逻辑树 */
    LvAstNode *l = lv_ast_create_bool(loc, 1);
    LvAstNode *rr = lv_ast_create_bool(loc, 0);
    LvAstNode *lb = lv_ast_create_logic_binary(LV_AST_LOGIC_AND, loc, "and", l, rr);
    lv_ast_print(lb, 2);

    /* NULL 安全 */
    lv_ast_print(NULL, 0);

    lv_ast_destroy(call);
    lv_ast_destroy(lb);
    printf("  test_ast_print_api: PASSED\n");
}

/* ============== 测试入口 ============== */

TEST_MAIN_BEGIN("Lv-00 AST Ext Test Suite")
    printf("=== Lv-00 AST Ext Test Suite (batch C-㊺续27) ===\n\n");
    lv_init();

    TEST_MAIN_RUN(test_ast_literal_api);
    TEST_MAIN_RUN(test_ast_call_logic_api);
    TEST_MAIN_RUN(test_ast_print_api);

    lv_cleanup();
TEST_MAIN_END()

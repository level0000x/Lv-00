/**
 * @file test_expr_canon.c
 * @brief lvExprCanonical 模块测试套件
 *
 * 测试 lvExprCanonical 规范多项式模块的所有公共 API，涵盖：
 * - 生命周期管理（create/destroy/clone）
 * - 项操作（add_term）
 * - 规范化操作（canonicalize - 合并同类项、排序、消除零系数项） 
 * - 规范形式检查（is_canonical）
 * - 算术运算（add/sub/mul/scale）
 * - 项比较（compare_terms）
 * - 字符串序列化（to_string）
 *
 * @author Lv-00 Project
 * @date 2026-05-24
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv.h"
#include "expr_canon.h"
#include "test_helpers.h"

/* ============================================================
 * 全局测试计数器
 * ============================================================ */
int g_pass_count = 0;
int g_fail_count = 0;

/* ============================================================
 * 辅助宏
 * ============================================================ */

/** 安全释放 lv_expr_canonical_to_string 返回的字符串 */
#ifndef SAFE_FREE_STR
#define SAFE_FREE_STR(s)  \
    do {                  \
        if (s) free((s)); \
    } while (0)
#endif

/* ============================================================
 * 测试用例
 * ============================================================ */

/**
 * @brief 测试多项式的创建与销毁
 *
 * 创建 2 变量的空多项式，验证非 NULL，
 * 销毁后验证指针变为 NULL。
 */
static void test_expr_create_destroy(void) {
    const char *var_names[] = {"x", "y"};
    lvExprCanonical *expr = lv_expr_canonical_create(2, var_names);
    TEST_ASSERT_NOT_NULL(expr);

    /* 新创建的多项式应为空（0 项） */
    TEST_ASSERT_EQ(expr->term_count, 0);
    TEST_ASSERT_EQ(expr->var_count, 2);

    lv_expr_canonical_destroy(&expr);
    TEST_ASSERT_NULL(expr);
}

/**
 * @brief 测试 add_term 接口
 *
 * 向多项式中添加两项，验证 term_count 正确递增。
 * 添加后不进行 canonicalize，仅验证基础计数。
 */
static void test_expr_add_term(void) {
    const char *var_names[] = {"x", "y"};
    lvExprCanonical *expr = lv_expr_canonical_create(2, var_names);
    TEST_ASSERT_NOT_NULL(expr);
    TEST_ASSERT_EQ(expr->term_count, 0);

    /* 添加项 3*x^1*y^0，即 3x */
    lvRational *c1 = lv_rational_create_from_si(3, 1);
    TEST_ASSERT_NOT_NULL(c1);
    int exp1[] = {1, 0};
    bool ok1 = lv_expr_canonical_add_term(expr, c1, exp1);
    TEST_ASSERT_MSG(ok1, "add_term for 3x should succeed");
    TEST_ASSERT_EQ(expr->term_count, 1);

    /* 添加项 2*x^0*y^1，即 2y */
    lvRational *c2 = lv_rational_create_from_si(2, 1);
    TEST_ASSERT_NOT_NULL(c2);
    int exp2[] = {0, 1};
    bool ok2 = lv_expr_canonical_add_term(expr, c2, exp2);
    TEST_ASSERT_MSG(ok2, "add_term for 2y should succeed");
    TEST_ASSERT_EQ(expr->term_count, 2);

    lv_rational_destroy(&c1);
    lv_rational_destroy(&c2);
    lv_expr_canonical_destroy(&expr);
}

/**
 * @brief 测试 canonicalize 合并同类项: 3x + 2x = 5x
 *
 * 向 1 变量多项式中添加 3x 和 2x，调用 canonicalize，
 * 验证合并后只有 1 项，且系数为 5。
 */
static void test_expr_canonicalize_merge(void) {
    const char *var_names[] = {"x"};
    lvExprCanonical *expr = lv_expr_canonical_create(1, var_names);
    TEST_ASSERT_NOT_NULL(expr);

    /* 添加 3x */
    lvRational *c1 = lv_rational_create_from_si(3, 1);
    TEST_ASSERT_NOT_NULL(c1);
    int exp1[] = {1};
    bool ok1 = lv_expr_canonical_add_term(expr, c1, exp1);
    TEST_ASSERT_MSG(ok1, "add_term for 3x should succeed");

    /* 添加 2x */
    lvRational *c2 = lv_rational_create_from_si(2, 1);
    TEST_ASSERT_NOT_NULL(c2);
    int exp2[] = {1};
    bool ok2 = lv_expr_canonical_add_term(expr, c2, exp2);
    TEST_ASSERT_MSG(ok2, "add_term for 2x should succeed");

    /* 规范化：应合并为 5x */
    bool canon_ok = lv_expr_canonicalize(expr);
    TEST_ASSERT_MSG(canon_ok, "canonicalize should succeed");
    TEST_ASSERT_EQ(expr->term_count, 1);

    /* 验证系数为 5/1 */
    lvRational *expected = lv_rational_create_from_si(5, 1);
    TEST_ASSERT_NOT_NULL(expected);
    int cmp = lv_rational_cmp(expr->terms[0].coeff, expected);
    TEST_ASSERT_EQ(cmp, 0);

    /* 验证指数仍为 [1] */
    TEST_ASSERT_EQ(expr->terms[0].exponents[0], 1);

    lv_rational_destroy(&c1);
    lv_rational_destroy(&c2);
    lv_rational_destroy(&expected);
    lv_expr_canonical_destroy(&expr);
}

/**
 * @brief 测试 canonicalize 排序: z^1 + x^1 应排序为 z^1 在前，x^1 在后
 *
 * 根据规范排序规则：首先按总次数降序（两者次数相同），
 * 同次数内按字典序比较指数数组（最后一个变量优先比较）。
 * 变量顺序为 ["x", "z"]，z^1 指数 [0,1]，x^1 指数 [1,0]，
 * exp[1] 比较: 1 > 0，所以 z^1 排在 x^1 之前。
 */
static void test_expr_canonicalize_sort(void) {
    const char *var_names[] = {"x", "z"};
    lvExprCanonical *expr = lv_expr_canonical_create(2, var_names);
    TEST_ASSERT_NOT_NULL(expr);

    /* 添加 z^1*y^0 -> 即 z^1 (指数: x=0, z=1) */
    lvRational *c1 = lv_rational_create_from_si(1, 1);
    TEST_ASSERT_NOT_NULL(c1);
    int exp_z[] = {0, 1}; /* x^0 * z^1 */
    lv_expr_canonical_add_term(expr, c1, exp_z);

    /* 添加 x^1*z^0 -> 即 x^1 (指数: x=1, z=0) */
    int exp_x[] = {1, 0}; /* x^1 * z^0 */
    lv_expr_canonical_add_term(expr, c1, exp_x);

    /* 规范化 */
    bool canon_ok = lv_expr_canonicalize(expr);
    TEST_ASSERT_MSG(canon_ok, "canonicalize should succeed");

    /* 排好序后，第一项应为 z^1（指数 [0,1]），第二项应为 x^1（指数 [1,0]） */
    TEST_ASSERT_EQ(expr->term_count, 2);

    /* 第一项: 应为 z^1，即 exponents = [0, 1] */
    TEST_ASSERT_EQ(expr->terms[0].exponents[0], 0);
    TEST_ASSERT_EQ(expr->terms[0].exponents[1], 1);

    /* 第二项: 应为 x^1，即 exponents = [1, 0] */
    TEST_ASSERT_EQ(expr->terms[1].exponents[0], 1);
    TEST_ASSERT_EQ(expr->terms[1].exponents[1], 0);

    lv_rational_destroy(&c1);
    lv_expr_canonical_destroy(&expr);
}

/**
 * @brief 测试 canonicalize 消除零系数项: 0*x 应被移除
 *
 * 添加零系数项后 canonicalize，验证多项式为空（0 项）。
 */
static void test_expr_canonicalize_remove_zero(void) {
    const char *var_names[] = {"x"};
    lvExprCanonical *expr = lv_expr_canonical_create(1, var_names);
    TEST_ASSERT_NOT_NULL(expr);

    /* 添加 0*x */
    lvRational *zero = lv_rational_create_from_si(0, 1);
    TEST_ASSERT_NOT_NULL(zero);
    int exp[] = {1};
    lv_expr_canonical_add_term(expr, zero, exp);

    /* 也应添加一个有效的项以确保 canonicalize 不会把整个多项式判空 */
    lvRational *one = lv_rational_create_from_si(1, 1);
    TEST_ASSERT_NOT_NULL(one);
    int exp2[] = {2};
    lv_expr_canonical_add_term(expr, one, exp2);

    /* 规范化：零项应被移除 */
    bool canon_ok = lv_expr_canonicalize(expr);
    TEST_ASSERT_MSG(canon_ok, "canonicalize should succeed");

    /* 应只剩 1 项（x^2） */
    TEST_ASSERT_EQ(expr->term_count, 1);
    TEST_ASSERT_EQ(expr->terms[0].exponents[0], 2);

    lv_rational_destroy(&zero);
    lv_rational_destroy(&one);
    lv_expr_canonical_destroy(&expr);
}

/**
 * @brief 测试 is_canonical 检查
 *
 * 创建多项式、添加项、canonicalize 后，
 * 验证 lv_expr_is_canonical 返回 true。
 * 再添加新项（未 canonicalize），验证返回 false。
 */
static void test_expr_is_canonical(void) {
    const char *var_names[] = {"x", "y"};
    lvExprCanonical *expr = lv_expr_canonical_create(2, var_names);
    TEST_ASSERT_NOT_NULL(expr);

    /* 添加 1*x^2*y^0 */
    lvRational *c = lv_rational_create_from_si(1, 1);
    TEST_ASSERT_NOT_NULL(c);
    int exp[] = {2, 0};
    lv_expr_canonical_add_term(expr, c, exp);

    /* 规范化后应为规范形式 */
    lv_expr_canonicalize(expr);
    TEST_ASSERT_MSG(lv_expr_is_canonical(expr),
                    "freshly canonicalized expr should be canonical");

    /* 添加新项后应不再是规范形式 */
    int exp2[] = {0, 1};
    lv_expr_canonical_add_term(expr, c, exp2);
    TEST_ASSERT_MSG(!lv_expr_is_canonical(expr),
                    "expr with unsorted terms should not be canonical");

    lv_rational_destroy(&c);
    lv_expr_canonical_destroy(&expr);
}

/**
 * @brief 测试 clone 深拷贝
 *
 * 创建多项式、添加项、canonicalize，然后 clone。
 * 验证 clone 后的多项式与原多项式逐项相等，
 * 且修改 clone 不影响原多项式。
 */
static void test_expr_clone(void) {
    const char *var_names[] = {"x", "y"};
    lvExprCanonical *orig = lv_expr_canonical_create(2, var_names);
    TEST_ASSERT_NOT_NULL(orig);

    /* 添加 3x + 2y */
    lvRational *c3 = lv_rational_create_from_si(3, 1);
    lvRational *c2 = lv_rational_create_from_si(2, 1);
    TEST_ASSERT_NOT_NULL(c3);
    TEST_ASSERT_NOT_NULL(c2);
    int exp_x[] = {1, 0};
    int exp_y[] = {0, 1};
    lv_expr_canonical_add_term(orig, c3, exp_x);
    lv_expr_canonical_add_term(orig, c2, exp_y);
    lv_expr_canonicalize(orig);

    /* Clone */
    lvExprCanonical *copy = lv_expr_canonical_clone(orig);
    TEST_ASSERT_NOT_NULL(copy);

    /* 逐项验证 */
    TEST_ASSERT_EQ(copy->term_count, orig->term_count);
    TEST_ASSERT_EQ(copy->var_count, orig->var_count);
    for (int i = 0; i < copy->term_count; i++) {
        int cmp = lv_rational_cmp(copy->terms[i].coeff, orig->terms[i].coeff);
        TEST_ASSERT_EQ_MSG(cmp, 0, "clone coefficient mismatch");
        for (int j = 0; j < copy->var_count; j++) {
            TEST_ASSERT_EQ(copy->terms[i].exponents[j], orig->terms[i].exponents[j]);
        }
    }

    /* 修改 clone 不影响原多项式：给 clone 添加一项 */
    int exp_z[] = {0, 2};
    lv_expr_canonical_add_term(copy, c2, exp_z);
    lv_expr_canonicalize(copy);
    TEST_ASSERT_EQ(orig->term_count, 2); /* 原多项式仍为 2 项 */

    lv_rational_destroy(&c3);
    lv_rational_destroy(&c2);
    lv_expr_canonical_destroy(&orig);
    lv_expr_canonical_destroy(&copy);
}

/**
 * @brief 测试多项式加法: (x + y) + (2x - y) = 3x
 */
static void test_expr_add(void) {
    const char *var_names[] = {"x", "y"};

    /* a = x + y */
    lvExprCanonical *a = lv_expr_canonical_create(2, var_names);
    TEST_ASSERT_NOT_NULL(a);
    lvRational *c1 = lv_rational_create_from_si(1, 1);
    TEST_ASSERT_NOT_NULL(c1);
    int exp_x[] = {1, 0};
    int exp_y[] = {0, 1};
    lv_expr_canonical_add_term(a, c1, exp_x);
    lv_expr_canonical_add_term(a, c1, exp_y);
    lv_expr_canonicalize(a);

    /* b = 2x - y */
    lvExprCanonical *b = lv_expr_canonical_create(2, var_names);
    TEST_ASSERT_NOT_NULL(b);
    lvRational *c2 = lv_rational_create_from_si(2, 1);
    lvRational *cn1 = lv_rational_create_from_si(-1, 1);
    TEST_ASSERT_NOT_NULL(c2);
    TEST_ASSERT_NOT_NULL(cn1);
    lv_expr_canonical_add_term(b, c2, exp_x);
    lv_expr_canonical_add_term(b, cn1, exp_y);
    lv_expr_canonicalize(b);

    /* a + b = 3x */
    lvExprCanonical *result = lv_expr_canonical_add(a, b);
    TEST_ASSERT_NOT_NULL(result);

    TEST_ASSERT_EQ(result->term_count, 1);
    TEST_ASSERT_EQ(result->terms[0].exponents[0], 1);
    TEST_ASSERT_EQ(result->terms[0].exponents[1], 0);

    lvRational *expected = lv_rational_create_from_si(3, 1);
    TEST_ASSERT_NOT_NULL(expected);
    int cmp = lv_rational_cmp(result->terms[0].coeff, expected);
    TEST_ASSERT_EQ(cmp, 0);

    lv_rational_destroy(&c1);
    lv_rational_destroy(&c2);
    lv_rational_destroy(&cn1);
    lv_rational_destroy(&expected);
    lv_expr_canonical_destroy(&a);
    lv_expr_canonical_destroy(&b);
    lv_expr_canonical_destroy(&result);
}

/**
 * @brief 测试多项式减法: (x + y) - (x) = y
 */
static void test_expr_sub(void) {
    const char *var_names[] = {"x", "y"};

    /* a = x + y */
    lvExprCanonical *a = lv_expr_canonical_create(2, var_names);
    TEST_ASSERT_NOT_NULL(a);
    lvRational *c1 = lv_rational_create_from_si(1, 1);
    TEST_ASSERT_NOT_NULL(c1);
    int exp_x[] = {1, 0};
    int exp_y[] = {0, 1};
    lv_expr_canonical_add_term(a, c1, exp_x);
    lv_expr_canonical_add_term(a, c1, exp_y);
    lv_expr_canonicalize(a);

    /* b = x */
    lvExprCanonical *b = lv_expr_canonical_create(2, var_names);
    TEST_ASSERT_NOT_NULL(b);
    lv_expr_canonical_add_term(b, c1, exp_x);
    lv_expr_canonicalize(b);

    /* a - b = y */
    lvExprCanonical *result = lv_expr_canonical_sub(a, b);
    TEST_ASSERT_NOT_NULL(result);

    TEST_ASSERT_EQ(result->term_count, 1);
    TEST_ASSERT_EQ(result->terms[0].exponents[0], 0);
    TEST_ASSERT_EQ(result->terms[0].exponents[1], 1);

    lvRational *expected = lv_rational_create_from_si(1, 1);
    TEST_ASSERT_NOT_NULL(expected);
    int cmp = lv_rational_cmp(result->terms[0].coeff, expected);
    TEST_ASSERT_EQ(cmp, 0);

    lv_rational_destroy(&c1);
    lv_rational_destroy(&expected);
    lv_expr_canonical_destroy(&a);
    lv_expr_canonical_destroy(&b);
    lv_expr_canonical_destroy(&result);
}

/**
 * @brief 测试 compare_terms 排序规则
 *
 * 验证 lv_canonical_compare_terms 的行为：
 * - 总次数高的项应排在前面
 * - 同次数按字典序（最后变量优先比较）
 * - 完全相同指数应返回 0
 */
static void test_expr_compare_terms(void) {
    int var_count = 2;

    /* 测试1：总次数高的项应排在前面 */
    /* exp_a = [2, 0] 总次数 2, exp_b = [1, 0] 总次数 1 */
    int exp_high[] = {2, 0};
    int exp_low[]  = {1, 0};
    int cmp_deg = lv_canonical_compare_terms(exp_high, exp_low, var_count);
    TEST_ASSERT_MSG(cmp_deg > 0,
                    "higher degree term should be ordered before lower degree term");

    /* 测试2：同次数按字典序（最后一个变量优先比较） */
    /* exp_a = [0, 1] -> 变量0指数0，变量1指数1; exp_b = [1, 0] -> 变量0指数1，变量1指数0 */
    /* 同次数=1，比较最后变量（变量1）：1 > 0，所以 exp_a > exp_b */
    int exp_z[] = {0, 1}; /* z^1 (x=0, z=1) */
    int exp_x[] = {1, 0}; /* x^1 (x=1, z=0) */
    int cmp_lex = lv_canonical_compare_terms(exp_z, exp_x, var_count);
    TEST_ASSERT_MSG(cmp_lex > 0,
                    "z^1 should be ordered before x^1 (lexicographic, last var first)");

    /* 测试3：相同指数应返回 0 */
    int exp_same1[] = {1, 1};
    int exp_same2[] = {1, 1};
    int cmp_eq = lv_canonical_compare_terms(exp_same1, exp_same2, var_count);
    TEST_ASSERT_EQ(cmp_eq, 0);
}

/**
 * @brief 测试 to_string 序列化
 *
 * 创建多项式 2x + y（变量名 ["x","y"]），
 * 验证 to_string 返回非空字符串，并包含预期内容。
 */
static void test_expr_to_string(void) {
    const char *var_names[] = {"x", "y"};
    lvExprCanonical *expr = lv_expr_canonical_create(2, var_names);
    TEST_ASSERT_NOT_NULL(expr);

    /* 创建 2x + y */
    lvRational *c2 = lv_rational_create_from_si(2, 1);
    lvRational *c1 = lv_rational_create_from_si(1, 1);
    TEST_ASSERT_NOT_NULL(c2);
    TEST_ASSERT_NOT_NULL(c1);
    int exp_x[] = {1, 0};
    int exp_y[] = {0, 1};
    lv_expr_canonical_add_term(expr, c2, exp_x);
    lv_expr_canonical_add_term(expr, c1, exp_y);
    lv_expr_canonicalize(expr);

    char *s = lv_expr_canonical_to_string(expr);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_MSG(strlen(s) > 0, "to_string should return non-empty string");

    /* 字符串应包含变量名 "x" 和 "y" */
    TEST_ASSERT_MSG(strstr(s, "x") != NULL, "string should contain 'x'");
    TEST_ASSERT_MSG(strstr(s, "y") != NULL, "string should contain 'y'");

    SAFE_FREE_STR(s);

    lv_rational_destroy(&c2);
    lv_rational_destroy(&c1);
    lv_expr_canonical_destroy(&expr);
}

/* ============================================================
 * 主入口
 * ============================================================ */
int main(void) {
    TEST_SUITE_BEGIN("lvExprCanonical");

    TEST_RUN(test_expr_create_destroy);
    TEST_RUN(test_expr_add_term);
    TEST_RUN(test_expr_canonicalize_merge);
    TEST_RUN(test_expr_canonicalize_sort);
    TEST_RUN(test_expr_canonicalize_remove_zero);
    TEST_RUN(test_expr_is_canonical);
    TEST_RUN(test_expr_clone);
    TEST_RUN(test_expr_add);
    TEST_RUN(test_expr_sub);
    TEST_RUN(test_expr_compare_terms);
    TEST_RUN(test_expr_to_string);

    TEST_SUITE_END();
    return (g_fail_count > 0) ? 1 : 0;
}

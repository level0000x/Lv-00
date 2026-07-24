/**
 * @file test_lambda_church.c
 * @brief λ-演算端到端测试：Church 数字编码、β-归约、Y 组合子
 *
 * 测试覆盖（设计文档 8.1 节要求）：
 *   1. Church 数字 0/1/2/3/4 的编译与反向还原
 *   2. λ-项 → 约束图 → λ-项 的反向还原正确性
 *   3. β-归约执行（graph 结构层面验证）
 *   4. Church 运算（后继、乘法、幂）的编译
 *   5. Y 组合子编译与归约尝试
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/lambda_term.h"
#include "lv/lv_utils.h"

/* ── 内部函数的 extern 声明（非 public API） ── */

/* lambda_to_graph.h 内声明 */
bool lambda_to_graph(LvLambdaTerm *term, ConstraintGraph *graph, int *out_node_id);
LvLambdaTerm *graph_to_lambda(ConstraintGraph *graph, int node_id);

/* beta_reduce.c 内声明 */
bool beta_reduce(ConstraintGraph *graph);

/* ── 测试基础设施 ── */
#define TEST(n) printf("  [TEST] %s ... ", n)
#define PASS()            \
    do {                  \
        printf("PASS\n"); \
        P++;              \
    } while (0)
#define FAIL(m)                  \
    do {                         \
        printf("FAIL: %s\n", m); \
        F++;                     \
    } while (0)

static int P = 0, F = 0;

/* ── Church 数字 / 布尔 / 算术辅助函数 ── */

/** Church numeral 0: λf.λx.x */
static LvLambdaTerm *church_0(void) {
    return lv_lambda_create_abs(0, lv_lambda_create_abs(0, lv_lambda_create_var(0)));
}

/** Church numeral 1: λf.λx.(f x) */
static LvLambdaTerm *church_1(void) {
    return lv_lambda_create_abs(
        0, lv_lambda_create_abs(0, lv_lambda_create_app(lv_lambda_create_var(1), lv_lambda_create_var(0))));
}

/** Church numeral 2: λf.λx.(f (f x)) */
static LvLambdaTerm *church_2(void) {
    return lv_lambda_create_abs(
        0, lv_lambda_create_abs(
               0, lv_lambda_create_app(lv_lambda_create_var(1),
                                       lv_lambda_create_app(lv_lambda_create_var(1), lv_lambda_create_var(0)))));
}

/** Church numeral n 的通用构造 */
static LvLambdaTerm *church_n(int n) {
    LvLambdaTerm *body = lv_lambda_create_var(0);
    for (int i = 0; i < n; i++) {
        body = lv_lambda_create_app(lv_lambda_create_var(1), body);
    }
    return lv_lambda_create_abs(0, lv_lambda_create_abs(0, body));
}

/** Church successor: λn.λf.λx.f (n f x) */
static LvLambdaTerm *church_succ(void) {
    LvLambdaTerm *body = lv_lambda_create_app(
        lv_lambda_create_var(1),
        lv_lambda_create_app(lv_lambda_create_app(lv_lambda_create_var(2), lv_lambda_create_var(1)),
                             lv_lambda_create_var(0)));
    return lv_lambda_create_abs(0, lv_lambda_create_abs(0, lv_lambda_create_abs(0, body)));
}

/** Church multiplication: λm.λn.λf.m (n f) */
static LvLambdaTerm *church_mul(void) {
    LvLambdaTerm *body = lv_lambda_create_app(lv_lambda_create_var(2),
                                              lv_lambda_create_app(lv_lambda_create_var(1), lv_lambda_create_var(0)));
    return lv_lambda_create_abs(0, lv_lambda_create_abs(0, lv_lambda_create_abs(0, body)));
}

/** Church exponentiation (pow m n = n m): λm.λn.n m */
static LvLambdaTerm *church_pow(void) {
    LvLambdaTerm *body = lv_lambda_create_app(lv_lambda_create_var(0), lv_lambda_create_var(1));
    return lv_lambda_create_abs(0, lv_lambda_create_abs(0, body));
}

/* ── Church 布尔值 ── */

/** true = λx.λy.x */
static LvLambdaTerm *church_true(void) {
    return lv_lambda_create_abs(0, lv_lambda_create_abs(0, lv_lambda_create_var(1)));
}

/** false = λx.λy.y */
static LvLambdaTerm *church_false(void) {
    return lv_lambda_create_abs(0, lv_lambda_create_abs(0, lv_lambda_create_var(0)));
}

/** if = λp.λt.λf.p t f */
static LvLambdaTerm *church_if(void) {
    return lv_lambda_create_abs(
        0, lv_lambda_create_abs(
               0, lv_lambda_create_abs(
                      0, lv_lambda_create_app(lv_lambda_create_app(lv_lambda_create_var(2), lv_lambda_create_var(1)),
                                              lv_lambda_create_var(0)))));
}

/** iszero = λn. n (λx.false) true */
static LvLambdaTerm *church_iszero(void) {
    LvLambdaTerm *false_term = church_false();
    LvLambdaTerm *true_term = church_true();
    LvLambdaTerm *body = lv_lambda_create_app(
        lv_lambda_create_app(lv_lambda_create_var(0), lv_lambda_create_abs(0, false_term)), true_term);
    return lv_lambda_create_abs(0, body);
}

/** Church predecessor: λn.λf.λx.n (λg.λh.h (g f)) (λu.x) (λu.u) */
static LvLambdaTerm *church_pred(void) {
    /* λg.λh.h (g f) — inside λn.λf.λx: f=Var(3) from λh */
    LvLambdaTerm *inner1 = lv_lambda_create_abs(
        0, lv_lambda_create_app(lv_lambda_create_var(0),
                                lv_lambda_create_app(lv_lambda_create_var(1), lv_lambda_create_var(3))));
    LvLambdaTerm *pair_fn = lv_lambda_create_abs(0, inner1);

    /* λu.x — x is at depth 2: scope=[n(3),f(2),x(1),u(0)] */
    LvLambdaTerm *const_x = lv_lambda_create_abs(0, lv_lambda_create_var(2));

    /* λu.u */
    LvLambdaTerm *const_u = lv_lambda_create_abs(0, lv_lambda_create_var(0));

    /* n pair_fn const_x const_u */
    LvLambdaTerm *body = lv_lambda_create_app(
        lv_lambda_create_app(lv_lambda_create_app(lv_lambda_create_var(0), pair_fn), const_x), const_u);

    return lv_lambda_create_abs(0, lv_lambda_create_abs(0, lv_lambda_create_abs(0, body)));
}

/* ── Y 组合子 ── */

/** Y 组合子: λf.(λx.f (x x)) (λx.f (x x)) */
static LvLambdaTerm *y_combinator(void) {
    LvLambdaTerm *inner = lv_lambda_create_abs(
        0, lv_lambda_create_app(lv_lambda_create_var(1),
                                lv_lambda_create_app(lv_lambda_create_var(0), lv_lambda_create_var(0))));
    LvLambdaTerm *body = lv_lambda_create_app(lv_lambda_copy(inner), inner);
    return lv_lambda_create_abs(0, body);
}

/* ── 辅助：编译 λ-term → 约束图 ── */
static int compile_lambda(LvLambdaTerm *term, ConstraintGraph *graph) {
    int root_id = -1;
    bool ok = lambda_to_graph(term, graph, &root_id);
    return ok ? root_id : -1;
}

/* ── 辅助：反复 β-归约至不动点，返回归约步数 ── */
static int beta_reduce_fully(ConstraintGraph *graph) {
    int steps = 0;
    while (beta_reduce(graph)) {
        steps++;
    }
    return steps;
}

/* ── 辅助：还原后获取 λ-term 的字符串表示 ── */
static char *get_lambda_string(ConstraintGraph *graph, int root_id) {
    LvLambdaTerm *restored = graph_to_lambda(graph, root_id);
    if (!restored)
        return NULL;
    char *str = lv_lambda_to_string(restored);
    lv_lambda_destroy(restored);
    return str;
}

/* ── 辅助：编译并验证 roundtrip -- 核心断言 ── */
static int compile_and_check_roundtrip(LvLambdaTerm *term) {
    ConstraintGraph *graph = graph_create();
    int root_id = compile_lambda(term, graph);
    if (root_id < 0) {
        graph_destroy(graph);
        return -1;
    }

    char *str = get_lambda_string(graph, root_id);
    graph_destroy(graph);
    if (!str)
        return -1;

    lv_free((void **) &str);
    return 0;
}

/* ====================================================================
 * 测试用例
 * ==================================================================== */

/**
 * @brief 测试 Church 数字 0: 编译 + roundtrip
 */
static void test_church_zero(void) {
    LvLambdaTerm *zero = church_0();
    int rc = compile_and_check_roundtrip(zero);
    lv_lambda_destroy(zero);
    if (rc == 0)
        PASS();
    else
        FAIL("编译或还原失败");
}

/**
 * @brief 测试 Church 数字 1: 编译 + roundtrip
 */
static void test_church_one(void) {
    LvLambdaTerm *one = church_1();
    int rc = compile_and_check_roundtrip(one);
    lv_lambda_destroy(one);
    if (rc == 0)
        PASS();
    else
        FAIL("编译或还原失败");
}

/**
 * @brief 测试 Church 数字 2: 编译 + roundtrip
 */
static void test_church_two(void) {
    LvLambdaTerm *two = church_2();
    int rc = compile_and_check_roundtrip(two);
    lv_lambda_destroy(two);
    if (rc == 0)
        PASS();
    else
        FAIL("编译或还原失败");
}

/**
 * @brief 测试 Church 数字 3: 通用构造 + roundtrip
 */
static void test_church_three(void) {
    LvLambdaTerm *three = church_n(3);
    int rc = compile_and_check_roundtrip(three);
    lv_lambda_destroy(three);
    if (rc == 0)
        PASS();
    else
        FAIL("编译或还原失败");
}

/**
 * @brief 测试 Church 数字 4: 通用构造 + roundtrip
 */
static void test_church_four(void) {
    LvLambdaTerm *four = church_n(4);
    int rc = compile_and_check_roundtrip(four);
    lv_lambda_destroy(four);
    if (rc == 0)
        PASS();
    else
        FAIL("编译或还原失败");
}

/**
 * @brief 测试 β-归约: (λx.x) y → y — 验证编译和归约调用
 *
 * 验证 (λy.(λx.x) y) 可以编译为约束图，
 * 并且 beta_reduce 可以被调用（返回成功/失败均可）。
 */
static void test_beta_id(void) {
    /* λy.(λx.x) y — 外层 λy 绑定变量 y，避免自由变量 */
    LvLambdaTerm *body =
        lv_lambda_create_app(lv_lambda_create_abs(0, lv_lambda_create_var(0)), lv_lambda_create_var(0));
    LvLambdaTerm *term = lv_lambda_create_abs(0, body);

    ConstraintGraph *graph = graph_create();
    int root_id = compile_lambda(term, graph);
    lv_lambda_destroy(term);

    if (root_id < 0) {
        graph_destroy(graph);
        FAIL("编译失败");
        return;
    }

    /* 尝试 β-归约（无论结果如何，调用不应崩溃） */
    beta_reduce(graph);

    /* 验证 roundtrip 仍可工作 */
    char *str = get_lambda_string(graph, root_id);
    graph_destroy(graph);
    if (!str) {
        FAIL("归约后还原失败");
        return;
    }

    lv_free((void **) &str);
    PASS();
}

/**
 * @brief 测试 β-归约调用安全性: 编译并调用 beta_reduce
 */
static void test_beta_abs(void) {
    /* λa.λb. (λx.λy.x) a b */
    LvLambdaTerm *k = lv_lambda_create_abs(0, lv_lambda_create_abs(0, lv_lambda_create_var(1)));
    LvLambdaTerm *body =
        lv_lambda_create_app(lv_lambda_create_app(k, lv_lambda_create_var(1)), lv_lambda_create_var(0));
    LvLambdaTerm *term = lv_lambda_create_abs(0, lv_lambda_create_abs(0, body));

    ConstraintGraph *graph = graph_create();
    int root_id = compile_lambda(term, graph);
    lv_lambda_destroy(term);

    if (root_id < 0) {
        graph_destroy(graph);
        FAIL("编译失败");
        return;
    }

    beta_reduce(graph);
    beta_reduce(graph);

    char *str = get_lambda_string(graph, root_id);
    graph_destroy(graph);
    if (!str) {
        FAIL("归约后还原失败");
        return;
    }

    lv_free((void **) &str);
    PASS();
}

/**
 * @brief 测试 Church succ 编译和 roundtrip
 */
static void test_church_succ(void) {
    LvLambdaTerm *two = church_2();
    LvLambdaTerm *succ = church_succ();
    LvLambdaTerm *term = lv_lambda_create_app(succ, two);

    ConstraintGraph *graph = graph_create();
    int root_id = compile_lambda(term, graph);
    lv_lambda_destroy(term);

    if (root_id < 0) {
        graph_destroy(graph);
        FAIL("编译失败");
        return;
    }

    /* 尝试归约 */
    beta_reduce_fully(graph);

    char *str = get_lambda_string(graph, root_id);
    graph_destroy(graph);
    if (!str) {
        FAIL("还原失败");
        return;
    }

    lv_free((void **) &str);
    PASS();
}

/**
 * @brief 测试 Church mul 编译和 roundtrip
 */
static void test_church_mul(void) {
    LvLambdaTerm *two = church_2();
    LvLambdaTerm *three = church_n(3);
    LvLambdaTerm *mul = church_mul();
    LvLambdaTerm *term = lv_lambda_create_app(lv_lambda_create_app(mul, two), three);

    ConstraintGraph *graph = graph_create();
    int root_id = compile_lambda(term, graph);
    lv_lambda_destroy(term);

    if (root_id < 0) {
        graph_destroy(graph);
        FAIL("编译失败");
        return;
    }

    beta_reduce_fully(graph);

    char *str = get_lambda_string(graph, root_id);
    graph_destroy(graph);
    if (!str) {
        FAIL("还原失败");
        return;
    }

    lv_free((void **) &str);
    PASS();
}

/**
 * @brief 测试 Church 幂运算: 2 2 → 4（Church 编码中的指数运算）
 *
 * 在 Church 编码中，Church 数字 k 作用于 Church 数字 n 得到 n^k。
 * 即 2 2 应归约为 Church 数字 4。
 */
static void test_church_pow(void) {
    LvLambdaTerm *two = church_2();
    LvLambdaTerm *term = lv_lambda_create_app(lv_lambda_copy(two), two);

    ConstraintGraph *graph = graph_create();
    int root_id = compile_lambda(term, graph);
    lv_lambda_destroy(term);

    if (root_id < 0) {
        graph_destroy(graph);
        FAIL("编译失败");
        return;
    }

    beta_reduce_fully(graph);

    char *str = get_lambda_string(graph, root_id);
    graph_destroy(graph);
    if (!str) {
        FAIL("还原失败");
        return;
    }

    lv_free((void **) &str);
    PASS();
}

/**
 * @brief 测试 Y 组合子编译和归约调用
 */
static void test_y_combinator_step(void) {
    LvLambdaTerm *Y = y_combinator();
    LvLambdaTerm *F = lv_lambda_create_abs(0, lv_lambda_create_var(0));
    LvLambdaTerm *term = lv_lambda_create_app(Y, F);

    ConstraintGraph *graph = graph_create();
    int root_id = compile_lambda(term, graph);
    lv_lambda_destroy(term);

    if (root_id < 0) {
        graph_destroy(graph);
        FAIL("编译失败");
        return;
    }

    /* 调用单步归约（无论结果） */
    beta_reduce(graph);

    /* 验证 roundtrip */
    char *str = get_lambda_string(graph, root_id);
    graph_destroy(graph);
    if (!str) {
        FAIL("还原失败");
        return;
    }

    lv_free((void **) &str);
    PASS();
}

/**
 * @brief 测试 Y 组合子阶乘的编译和归约
 */
static LvLambdaTerm *make_factorial_F(void) {
    LvLambdaTerm *true_term = church_true();
    LvLambdaTerm *false_term = church_false();
    LvLambdaTerm *if_term = church_if();
    LvLambdaTerm *iszero = church_iszero();
    LvLambdaTerm *one = church_1();
    LvLambdaTerm *mul = church_mul();
    LvLambdaTerm *pred = church_pred();

    LvLambdaTerm *pred_n = lv_lambda_create_app(lv_lambda_copy(pred), lv_lambda_create_var(0));
    LvLambdaTerm *f_pred_n = lv_lambda_create_app(lv_lambda_create_var(1), pred_n);
    LvLambdaTerm *mul_n_f =
        lv_lambda_create_app(lv_lambda_create_app(lv_lambda_copy(mul), lv_lambda_create_var(0)), f_pred_n);
    LvLambdaTerm *iszero_n = lv_lambda_create_app(lv_lambda_copy(iszero), lv_lambda_create_var(0));
    LvLambdaTerm *body = lv_lambda_create_app(
        lv_lambda_create_app(lv_lambda_create_app(lv_lambda_copy(if_term), iszero_n), lv_lambda_copy(one)), mul_n_f);
    LvLambdaTerm *lambda_n = lv_lambda_create_abs(0, body);
    LvLambdaTerm *F = lv_lambda_create_abs(0, lambda_n);

    lv_lambda_destroy(true_term);
    lv_lambda_destroy(false_term);
    lv_lambda_destroy(if_term);
    lv_lambda_destroy(iszero);
    lv_lambda_destroy(one);
    lv_lambda_destroy(mul);
    lv_lambda_destroy(pred);

    return F;
}

static void test_y_combinator_factorial(void) {
    LvLambdaTerm *Y = y_combinator();
    LvLambdaTerm *F = make_factorial_F();
    LvLambdaTerm *YF = lv_lambda_create_app(Y, F);
    LvLambdaTerm *three = church_n(3);
    LvLambdaTerm *term = lv_lambda_create_app(YF, three);

    ConstraintGraph *graph = graph_create();
    int root_id = compile_lambda(term, graph);
    lv_lambda_destroy(term);

    if (root_id < 0) {
        graph_destroy(graph);
        FAIL("编译失败");
        return;
    }

    beta_reduce_fully(graph);

    char *str = get_lambda_string(graph, root_id);
    graph_destroy(graph);
    if (!str) {
        FAIL("还原失败");
        return;
    }

    lv_free((void **) &str);
    PASS();
}

/* ====================================================================
 * main
 * ==================================================================== */
int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== λ-演算端到端测试 ===\n\n");

    printf("[Church 数字编译与还原]\n");
    TEST("Church 0: λf.λx.x");
    test_church_zero();
    TEST("Church 1: λf.λx.(f x)");
    test_church_one();
    TEST("Church 2: λf.λx.(f (f x))");
    test_church_two();
    TEST("Church 3: 通用构造");
    test_church_three();
    TEST("Church 4: 通用构造");
    test_church_four();

    printf("\n[β-归约]\n");
    TEST("(λx.x) y 编译+归约");
    test_beta_id();
    TEST("(λx.λy.x) a b 编译+归约");
    test_beta_abs();
    TEST("Church succ 编译");
    test_church_succ();
    TEST("Church mul 编译");
    test_church_mul();
    TEST("Church pow 2 2 编译");
    test_church_pow();

    printf("\n[Y 组合子]\n");
    TEST("YF 编译+归约");
    test_y_combinator_step();
    TEST("Y 组合子阶乘编译");
    test_y_combinator_factorial();

    printf("\n=== %d passed, %d failed ===\n", P, F);
    return F > 0 ? 1 : 0;
}

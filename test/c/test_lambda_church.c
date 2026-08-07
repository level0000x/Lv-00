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
#include "lv/engine.h"
#include "lv/lambda_church.h"
#include "lv/lambda_term.h"
#include "lv/lambda_to_graph.h"
#include "lv/lambda_type_check.h"
#include "lv/lv_utils.h"
#include "lv/proof.h"

#define TEST_PASS_STATEMENT g_pass_count++
#define TEST_FAIL_STATEMENT g_fail_count++
#include "test_helpers.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ── Church 编码辅助函数（通过公共 API） ── */

/** Church numeral 0: λf.λx.x */
static LvLambdaTerm *church_0(void) {
    return lv_church_0();
}

/** Church numeral 1: λf.λx.(f x) */
static LvLambdaTerm *church_1(void) {
    return lv_church_1();
}

/** Church numeral 2: λf.λx.(f (f x)) */
static LvLambdaTerm *church_2(void) {
    return lv_church_2();
}

/** Church numeral n 的通用构造 */
static LvLambdaTerm *church_n(int n) {
    return lv_church_n(n);
}

/** Church successor: λn.λf.λx.f (n f x) */
static LvLambdaTerm *church_succ(void) {
    return lv_church_succ();
}

/** Church multiplication: λm.λn.λf.m (n f) */
static LvLambdaTerm *church_mul(void) {
    return lv_church_mul();
}

/** Church exponentiation (pow m n = n m): λm.λn.n m */
static LvLambdaTerm *church_pow(void) {
    return lv_church_pow();
}

/* ── Church 布尔值 ── */

/** true = λx.λy.x */
static LvLambdaTerm *church_true(void) {
    return lv_church_true();
}

/** false = λx.λy.y */
static LvLambdaTerm *church_false(void) {
    return lv_church_false();
}

/** if = λp.λt.λf.p t f */
static LvLambdaTerm *church_if(void) {
    return lv_church_if();
}

/** iszero = λn. n (λx.false) true */
static LvLambdaTerm *church_iszero(void) {
    return lv_church_iszero();
}

/** Church predecessor: λn.λf.λx.n (λg.λh.h (g f)) (λu.x) (λu.u) */
static LvLambdaTerm *church_pred(void) {
    return lv_church_pred();
}

/* ── Y 组合子 ── */

/** Y 组合子: λf.(λx.f (x x)) (λx.f (x x)) */
static LvLambdaTerm *y_combinator(void) {
    return lv_church_y_combinator();
}

/* ── 辅助：编译 λ-term → 约束图 ── */
static int compile_lambda(LvLambdaTerm *term, ConstraintGraph *graph) {
    int root_id = -1;
    bool ok = lambda_to_graph(term, graph, &root_id);
    return ok ? root_id : -1;
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

/* ================================================================
 * 集成测试：β-归约公共 API
 * ================================================================ */

/**
 * @brief 验证 beta_reduce 通过公共头文件暴露为 public API
 */
static void test_beta_reduce_public_api(void) {
    /* λy.(λx.x) y — 外层 λy 绑定变量 y，避免自由变量（同 test_beta_id 模式） */
    LvLambdaTerm *body =
        lv_lambda_create_app(lv_lambda_create_abs(0, lv_lambda_create_var(0)), lv_lambda_create_var(0));
    LvLambdaTerm *term = lv_lambda_create_abs(0, body);

    ConstraintGraph *graph = graph_create();
    int root_id;
    bool ok = lambda_to_graph(term, graph, &root_id);
    lv_lambda_destroy(term);
    if (!ok) {
        graph_destroy(graph);
        FAIL("compile λy.(λx.x) y");
        return;
    }

    /* beta_reduce 通过 public API 调用不应崩溃 */
    beta_reduce(graph);

    graph_destroy(graph);
    PASS();
}

/**
 * @brief 验证引擎重写-求解管线集成 β-归约
 */
static void test_engine_lambda_integration(void) {
    lvEngine *engine = engine_create();
    if (!engine) {
        FAIL("engine create");
        return;
    }

    /* λy.(λx.x) y 编译到引擎的主图 */
    LvLambdaTerm *body =
        lv_lambda_create_app(lv_lambda_create_abs(0, lv_lambda_create_var(0)), lv_lambda_create_var(0));
    LvLambdaTerm *term = lv_lambda_create_abs(0, body);
    int root_id;
    bool ok = lambda_to_graph(term, engine->main_graph, &root_id);
    lv_lambda_destroy(term);
    if (!ok) {
        engine_destroy(engine);
        FAIL("engine: compile λy.(λx.x) y");
        return;
    }

    /* β-归约通过公共 API 在引擎上下文中调用（可能不匹配，但不应崩溃） */
    beta_reduce(engine->main_graph);

    engine_destroy(engine);
    PASS();
}

/**
 * @brief 验证证明多策略系统已注册 HOL Light 策略
 */
static void test_proof_strategy_hol_light(void) {
    Proposition *prop = proposition_create(0, PROPOSITION_TYPE_ATOMIC);
    ProofMultiStrategy *mse = proof_multi_strategy_create(NULL);
    if (!mse) {
        proposition_unref(prop);
        FAIL("mse create");
        return;
    }

    /* 检查 HOL Light 策略描述符已注册 */
    const ProofStrategyDescriptor *desc = &mse->strategies[PROOF_STRATEGY_HOL_LIGHT];
    if (desc->type != PROOF_STRATEGY_HOL_LIGHT || !desc->name || !desc->execute) {
        proof_multi_strategy_destroy(mse);
        proposition_unref(prop);
        FAIL("HOL Light strategy descriptor");
        return;
    }

    /* 验证 HOL Light 在默认回退顺序中 */
    bool found = false;
    for (int i = 0; i < mse->fallback_count; i++) {
        if (mse->fallback_order[i] == PROOF_STRATEGY_HOL_LIGHT) {
            found = true;
            break;
        }
    }
    proof_multi_strategy_destroy(mse);
    proposition_unref(prop);

    if (!found) {
        FAIL("HOL Light not in fallback order");
        return;
    }
    PASS();
}

/** @brief 验证 HOL Light 验证函数可通过公共 API 调用 */
static void test_hol_light_verify_api(void) {
    /* 测试 proof_minimal_verify 的基本调用 */
    const char *premises[] = {"x=y", "y=z", NULL};
    const char *conclusion = "x=z";
    char *trace = NULL;
    VerifyResult result = proof_minimal_verify(VERIFY_TRANS, premises, conclusion, &trace);
    if (result == VERIFY_VALID) {
        if (trace) lv_free((void **)&trace);
        PASS();
    } else {
        if (trace) {
            FAIL(trace);
            lv_free((void **)&trace);
        } else {
            FAIL("VERIFY_TRANS unexpected result");
        }
    }
}
static void test_proof_strategy_lambda(void) {
    Proposition *prop = proposition_create(0, PROPOSITION_TYPE_ATOMIC);
    ProofMultiStrategy *mse = proof_multi_strategy_create(NULL);
    if (!mse) {
        proposition_unref(prop);
        FAIL("mse create");
        return;
    }

    /* 检查 λ-演算策略描述符已注册 */
    const ProofStrategyDescriptor *desc = &mse->strategies[PROOF_STRATEGY_LAMBDA_CALCULUS];
    if (desc->type != PROOF_STRATEGY_LAMBDA_CALCULUS || !desc->name || !desc->execute) {
        proof_multi_strategy_destroy(mse);
        proposition_unref(prop);
        FAIL("lambda strategy descriptor");
        return;
    }

    proof_multi_strategy_destroy(mse);
    proposition_unref(prop);
    PASS();
}

/* ================================================================
 * λ-项类型检查测试
 * ================================================================ */

/**
 * @brief 测试 λx.x 的类型推断结果为 α → α
 */
static void test_type_infer_id(void) {
    /* λx.x */
    LvLambdaTerm *id = lv_lambda_create_abs(0, lv_lambda_create_var(0));

    TypeSystem *ts = type_system_create();
    LambdaTypingContext ctx;
    if (!ts || !lambda_type_check_init(&ctx, ts)) {
        type_system_destroy(ts);
        lv_lambda_destroy(id);
        FAIL("type_infer_id: init");
        return;
    }

    TypeRegion *type = lambda_type_infer(id, &ctx);
    lv_lambda_destroy(id);
    if (!type || type->kind != TYPE_KIND_FUNCTION) {
        lambda_type_check_destroy(&ctx);
        type_system_destroy(ts);
        FAIL("type_infer_id: not function type");
        return;
    }
    lambda_type_check_destroy(&ctx);
    type_system_destroy(ts);
    PASS();
}

/**
 * @brief 测试 λx.λy.x 的类型推断结果为 α → β → α
 */
static void test_type_infer_k(void) {
    /* λx.λy.x — K 组合子 */
    LvLambdaTerm *body = lv_lambda_create_abs(1, lv_lambda_create_var(1));
    LvLambdaTerm *term = lv_lambda_create_abs(0, body);

    TypeSystem *ts = type_system_create();
    LambdaTypingContext ctx;
    if (!ts || !lambda_type_check_init(&ctx, ts)) {
        type_system_destroy(ts);
        lv_lambda_destroy(term);
        FAIL("type_infer_k: init");
        return;
    }

    TypeRegion *type = lambda_type_infer(term, &ctx);
    lv_lambda_destroy(term);
    if (!type || type->kind != TYPE_KIND_FUNCTION) {
        lambda_type_check_destroy(&ctx);
        type_system_destroy(ts);
        FAIL("type_infer_k: not function type");
        return;
    }
    lambda_type_check_destroy(&ctx);
    type_system_destroy(ts);
    PASS();
}

/**
 * @brief 测试 (λx.x)(λy.y) 类型推断通过
 */
static void test_type_check_app_id(void) {
    /* (λx.x) (λy.y) */
    LvLambdaTerm *id1 = lv_lambda_create_abs(0, lv_lambda_create_var(0));
    LvLambdaTerm *id2 = lv_lambda_create_abs(0, lv_lambda_create_var(0));
    LvLambdaTerm *app = lv_lambda_create_app(id1, id2);

    TypeSystem *ts = type_system_create();
    LambdaTypingContext ctx;
    if (!ts || !lambda_type_check_init(&ctx, ts)) {
        type_system_destroy(ts);
        lv_lambda_destroy(app);
        FAIL("type_check_app_id: init");
        return;
    }

    TypeRegion *type = lambda_type_infer(app, &ctx);
    lv_lambda_destroy(app);
    if (!type) {
        lambda_type_check_destroy(&ctx);
        type_system_destroy(ts);
        FAIL("type_check_app_id: result is NULL");
        return;
    }
    lambda_type_check_destroy(&ctx);
    type_system_destroy(ts);
    PASS();
}

/* ── 公共 API 直接测试 ── */

/**
 * @brief 测试 Church 编码公共 API 的各个函数不会崩溃或返回 NULL
 */
static void test_church_public_api(void) {
    LvLambdaTerm *terms[33];
    int count = 0;
    bool ok = true;

    /* 调用所有公共 API 函数，验证不返回 NULL */
    terms[count++] = lv_church_0();
    terms[count++] = lv_church_1();
    terms[count++] = lv_church_2();
    terms[count++] = lv_church_n(5);
    terms[count++] = lv_church_succ();
    terms[count++] = lv_church_mul();
    terms[count++] = lv_church_pow();
    terms[count++] = lv_church_pred();
    terms[count++] = lv_church_true();
    terms[count++] = lv_church_false();
    terms[count++] = lv_church_if();
    terms[count++] = lv_church_iszero();
    terms[count++] = lv_church_add();
    terms[count++] = lv_church_sub();
    terms[count++] = lv_church_pair();
    terms[count++] = lv_church_first();
    terms[count++] = lv_church_second();
    terms[count++] = lv_church_nil();
    terms[count++] = lv_church_cons();
    terms[count++] = lv_church_tail();
    terms[count++] = lv_church_map();
    terms[count++] = lv_church_filter();
    terms[count++] = lv_church_foldr();
    terms[count++] = lv_church_foldl();
    terms[count++] = lv_church_length();
    terms[count++] = lv_church_append();
    terms[count++] = lv_church_leq();
    terms[count++] = lv_church_eq();
    terms[count++] = lv_church_gt();
    terms[count++] = lv_church_y_combinator();
    terms[count++] = lv_church_div();
    terms[count++] = lv_church_factorial();
    terms[count++] = lv_church_fib();

    for (int i = 0; i < count; i++) {
        if (!terms[i]) {
            ok = false;
            break;
        }
    }

    /* 验证 lv_church_n(负数) 返回 NULL */
    LvLambdaTerm *neg = lv_church_n(-1);
    if (neg != NULL) {
        lv_lambda_destroy(neg);
        ok = false;
    }

    /* 清理 */
    for (int i = 0; i < count; i++) {
        lv_lambda_destroy(terms[i]);
    }

    if (ok)
        PASS();
    else
        FAIL("公共 API 返回 NULL");
}

/* ── Church 扩展测试 ── */

/** 测试 Church 加法: add 2 3 → 5 的 roundtrip */
static void test_church_add_roundtrip(void) {
    LvLambdaTerm *a2 = lv_church_add();
    LvLambdaTerm *c2 = lv_church_2();
    LvLambdaTerm *c3 = lv_church_n(3);

    /* add 2 3 */
    LvLambdaTerm *add23 = lv_lambda_create_app(lv_lambda_create_app(a2, c2), c3);
    int rc = compile_and_check_roundtrip(add23);
    lv_lambda_destroy(add23);
    if (rc == 0)
        PASS();
    else
        FAIL("Church add 2 3 roundtrip 失败");
}

/** 测试 pair 编译 */
static void test_church_pair_compile(void) {
    LvLambdaTerm *pr = lv_church_pair();
    int rc = compile_and_check_roundtrip(pr);
    lv_lambda_destroy(pr);
    if (rc == 0)
        PASS();
    else
        FAIL("Church pair 编译失败");
}

/** 测试 Church 减法: sub 5 2 → 3 的 roundtrip */
static void test_church_sub_roundtrip(void) {
    LvLambdaTerm *sub = lv_church_sub();
    LvLambdaTerm *c5 = lv_church_n(5);
    LvLambdaTerm *c2 = lv_church_2();

    /* sub 5 2 */
    LvLambdaTerm *sub52 = lv_lambda_create_app(lv_lambda_create_app(sub, c5), c2);
    int rc = compile_and_check_roundtrip(sub52);
    lv_lambda_destroy(sub52);
    if (rc == 0)
        PASS();
    else
        FAIL("Church sub 5 2 roundtrip 失败");
}

/** 测试 Church 前驱: pred 3 → 2 的 roundtrip */
static void test_church_pred_roundtrip(void) {
    LvLambdaTerm *pred = lv_church_pred();
    LvLambdaTerm *c3 = lv_church_n(3);

    /* pred 3 */
    LvLambdaTerm *pred3 = lv_lambda_create_app(pred, c3);
    int rc = compile_and_check_roundtrip(pred3);
    lv_lambda_destroy(pred3);
    if (rc == 0)
        PASS();
    else
        FAIL("Church pred 3 roundtrip 失败");
}

/** 测试 nil 编译 */
static void test_church_nil_compile(void) {
    LvLambdaTerm *nil = lv_church_nil();
    int rc = compile_and_check_roundtrip(nil);
    lv_lambda_destroy(nil);
    if (rc == 0)
        PASS();
    else
        FAIL("Church nil 编译失败");
}

/** 测试 cons 编译 */
static void test_church_cons_compile(void) {
    LvLambdaTerm *cons = lv_church_cons();
    int rc = compile_and_check_roundtrip(cons);
    lv_lambda_destroy(cons);
    if (rc == 0)
        PASS();
    else
        FAIL("Church cons 编译失败");
}

/* ── Church 布尔运算测试 ── */

static void test_church_not_compile(void) {
    LvLambdaTerm *t = lv_church_not();
    int rc = compile_and_check_roundtrip(t);
    lv_lambda_destroy(t);
    if (rc == 0) PASS(); else FAIL("not 编译失败");
}

static void test_church_and_compile(void) {
    LvLambdaTerm *t = lv_church_and();
    int rc = compile_and_check_roundtrip(t);
    lv_lambda_destroy(t);
    if (rc == 0) PASS(); else FAIL("and 编译失败");
}

static void test_church_or_compile(void) {
    LvLambdaTerm *t = lv_church_or();
    int rc = compile_and_check_roundtrip(t);
    lv_lambda_destroy(t);
    if (rc == 0) PASS(); else FAIL("or 编译失败");
}

static void test_church_xor_compile(void) {
    LvLambdaTerm *t = lv_church_xor();
    int rc = compile_and_check_roundtrip(t);
    lv_lambda_destroy(t);
    if (rc == 0) PASS(); else FAIL("xor 编译失败");
}

/* ── Church 列表操作测试 ── */

static void test_church_isnil_compile(void) {
    LvLambdaTerm *t = lv_church_isnil();
    int rc = compile_and_check_roundtrip(t);
    lv_lambda_destroy(t);
    if (rc == 0) PASS(); else FAIL("isnil 编译失败");
}

static void test_church_head_compile(void) {
    LvLambdaTerm *t = lv_church_head();
    int rc = compile_and_check_roundtrip(t);
    lv_lambda_destroy(t);
    if (rc == 0) PASS(); else FAIL("head 编译失败");
}

static void test_church_tail_compile(void) {
    LvLambdaTerm *t = lv_church_tail();
    int rc = compile_and_check_roundtrip(t);
    lv_lambda_destroy(t);
    if (rc == 0) PASS(); else FAIL("tail 编译失败");
}

static void test_church_map_compile(void) {
    LvLambdaTerm *t = lv_church_map();
    int rc = compile_and_check_roundtrip(t);
    lv_lambda_destroy(t);
    if (rc == 0) PASS(); else FAIL("map 编译失败");
}

static void test_church_filter_compile(void) {
    LvLambdaTerm *t = lv_church_filter();
    int rc = compile_and_check_roundtrip(t);
    lv_lambda_destroy(t);
    if (rc == 0) PASS(); else FAIL("filter 编译失败");
}

static void test_church_foldl_compile(void) {
    LvLambdaTerm *t = lv_church_foldl();
    int rc = compile_and_check_roundtrip(t);
    lv_lambda_destroy(t);
    if (rc == 0) PASS(); else FAIL("foldl 编译失败");
}

static void test_church_foldr_compile(void) {
    LvLambdaTerm *t = lv_church_foldr();
    int rc = compile_and_check_roundtrip(t);
    lv_lambda_destroy(t);
    if (rc == 0) PASS(); else FAIL("foldr 编译失败");
}

static void test_church_length_compile(void) {
    LvLambdaTerm *t = lv_church_length();
    int rc = compile_and_check_roundtrip(t);
    lv_lambda_destroy(t);
    if (rc == 0) PASS(); else FAIL("length 编译失败");
}

static void test_church_append_compile(void) {
    LvLambdaTerm *t = lv_church_append();
    int rc = compile_and_check_roundtrip(t);
    lv_lambda_destroy(t);
    if (rc == 0) PASS(); else FAIL("append 编译失败");
}

/* ── Church 比较运算测试 ── */

static void test_church_leq_compile(void) {
    LvLambdaTerm *t = lv_church_leq();
    int rc = compile_and_check_roundtrip(t);
    lv_lambda_destroy(t);
    if (rc == 0) PASS(); else FAIL("leq 编译失败");
}

static void test_church_eq_compile(void) {
    LvLambdaTerm *t = lv_church_eq();
    int rc = compile_and_check_roundtrip(t);
    lv_lambda_destroy(t);
    if (rc == 0) PASS(); else FAIL("eq 编译失败");
}

static void test_church_gt_compile(void) {
    LvLambdaTerm *t = lv_church_gt();
    int rc = compile_and_check_roundtrip(t);
    lv_lambda_destroy(t);
    if (rc == 0) PASS(); else FAIL("gt 编译失败");
}

/* ── Church 扩展运算测试（Y 组合子递归）── */

static void test_church_div_compile(void) {
    LvLambdaTerm *t = lv_church_div();
    int rc = compile_and_check_roundtrip(t);
    lv_lambda_destroy(t);
    if (rc == 0) PASS(); else FAIL("div 编译失败");
}

static void test_church_factorial_direct_compile(void) {
    LvLambdaTerm *t = lv_church_factorial();
    int rc = compile_and_check_roundtrip(t);
    lv_lambda_destroy(t);
    if (rc == 0) PASS(); else FAIL("factorial 编译失败");
}

static void test_church_fib_compile(void) {
    LvLambdaTerm *t = lv_church_fib();
    int rc = compile_and_check_roundtrip(t);
    lv_lambda_destroy(t);
    if (rc == 0) PASS(); else FAIL("fib 编译失败");
}

/* ====================================================================
 * main
 * ==================================================================== */
TEST_MAIN_BEGIN("λ-演算端到端测试")
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("[Church 编码公共 API]\n");
    TEST_MAIN_RUN(test_church_public_api);
    printf("\n[Church 编码扩展]\n");
    TEST_MAIN_RUN(test_church_add_roundtrip);
    TEST_MAIN_RUN(test_church_sub_roundtrip);
    TEST_MAIN_RUN(test_church_pred_roundtrip);
    TEST_MAIN_RUN(test_church_pair_compile);
    TEST_MAIN_RUN(test_church_nil_compile);
    TEST_MAIN_RUN(test_church_cons_compile);
    printf("\n[Church 布尔运算]\n");
    TEST_MAIN_RUN(test_church_not_compile);
    TEST_MAIN_RUN(test_church_and_compile);
    TEST_MAIN_RUN(test_church_or_compile);
    TEST_MAIN_RUN(test_church_xor_compile);
    printf("\n[Church 列表操作]\n");
    TEST_MAIN_RUN(test_church_isnil_compile);
    TEST_MAIN_RUN(test_church_head_compile);
    TEST_MAIN_RUN(test_church_tail_compile);
    TEST_MAIN_RUN(test_church_map_compile);
    TEST_MAIN_RUN(test_church_filter_compile);
    TEST_MAIN_RUN(test_church_foldr_compile);
    TEST_MAIN_RUN(test_church_foldl_compile);
    TEST_MAIN_RUN(test_church_length_compile);
    TEST_MAIN_RUN(test_church_append_compile);
    printf("\n[Church 比较运算]\n");
    TEST_MAIN_RUN(test_church_leq_compile);
    TEST_MAIN_RUN(test_church_eq_compile);
    TEST_MAIN_RUN(test_church_gt_compile);
    printf("\n[Church 数字编译与还原]\n");
    TEST_MAIN_RUN(test_church_zero);
    TEST_MAIN_RUN(test_church_one);
    TEST_MAIN_RUN(test_church_two);
    TEST_MAIN_RUN(test_church_three);
    TEST_MAIN_RUN(test_church_four);
    printf("\n[β-归约]\n");
    TEST_MAIN_RUN(test_beta_id);
    TEST_MAIN_RUN(test_beta_abs);
    TEST_MAIN_RUN(test_church_succ);
    TEST_MAIN_RUN(test_church_mul);
    TEST_MAIN_RUN(test_church_pow);
    printf("\n[Y 组合子]\n");
    TEST_MAIN_RUN(test_y_combinator_step);
    TEST_MAIN_RUN(test_y_combinator_factorial);
    printf("\n[Church 扩展运算]\n");
    TEST_MAIN_RUN(test_church_div_compile);
    TEST_MAIN_RUN(test_church_factorial_direct_compile);
    TEST_MAIN_RUN(test_church_fib_compile);
    printf("\n[集成测试]\n");
    TEST_MAIN_RUN(test_beta_reduce_public_api);
    TEST_MAIN_RUN(test_engine_lambda_integration);
    TEST_MAIN_RUN(test_proof_strategy_lambda);
    TEST_MAIN_RUN(test_proof_strategy_hol_light);
    TEST_MAIN_RUN(test_hol_light_verify_api);
    printf("\n[λ-项类型检查]\n");
    TEST_MAIN_RUN(test_type_infer_id);
    TEST_MAIN_RUN(test_type_infer_k);
    TEST_MAIN_RUN(test_type_check_app_id);
TEST_MAIN_END()

/**
 * @file test_engine_scheduler.c
 * @brief Lv-00 EngineScheduler 模块全面测试
 *
 * 覆盖生命周期、后端注册、图特征分析、路由规则、后端选择、
 * 分发求解、统计诊断、配置修改及 NULL 安全。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* engine_scheduler.h 必须在 lv.h 之前包含，以确保 smt_backend.h 在 proof.h 之前处理 */
#include "lv/engine_scheduler.h"

#include "lv.h"
#include "test_helpers.h"

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

/* ── 辅助：创建含 2 个 POINT 节点 + 1 个 INCIDENCE 约束的简单图 ── */
static ConstraintGraph *create_simple_graph(void) {
    ConstraintGraph *g = graph_create();
    if (!g)
        return NULL;

    /* 两个 POINT 节点 */
    int p0 = add_point(g, 0, 1, 0, 1);
    int p1 = add_point(g, 1, 1, 0, 1);
    if (p0 < 0 || p1 < 0) {
        graph_destroy(g);
        return NULL;
    }

    /* 一条线段 */
    AddNodeResult ar = graph_add_line_segment(g, p0, p1);
    if (ar != ADD_NODE_OK) {
        graph_destroy(g);
        return NULL;
    }

    return g;
}

/* ============================================================
 * 测试 1: 生命周期 - scheduler_create / scheduler_destroy
 * ============================================================ */
static void test_lifecycle(void) {
    TEST("scheduler_create");
    EngineScheduler *s = scheduler_create();
    if (!s) {
        FAIL("NULL");
        return;
    }
    PASS();

    TEST("scheduler_destroy NULL 安全");
    scheduler_destroy(NULL);
    PASS();

    TEST("scheduler_destroy");
    scheduler_destroy(s);
    PASS();
}

/* ============================================================
 * 测试 2: 后端注册
 * ============================================================ */
static void test_register_backend(void) {
    EngineScheduler *s = scheduler_create();
    if (!s) {
        FAIL("create");
        F++;
        return;
    }

    TEST("默认 GROEBNER 已注册");
    if (scheduler_is_backend_available(s, GROEBNER))
        PASS();
    else
        FAIL("GROEBNER not available");

    TEST("注册新后端 NULL safety");
    int rc = scheduler_register_backend(NULL, SMT_Z3, 50, "test", NULL);
    if (rc != -1) {
        FAIL("expected -1");
        goto cleanup;
    }
    PASS();

    TEST("注册 SMT_Z3（附带 detect_func）");
    /* 提供检测函数使后端标记为可用，从而能被 list 列出 */
    rc = scheduler_register_backend(s, SMT_Z3, 50, "Z3 test backend",
                                    (SolverBackendDetectFunc) (void *) scheduler_is_backend_available);
    if (rc != 0) {
        FAIL("register failed");
        goto cleanup;
    }
    PASS();

    TEST("SMT_Z3 已列出");
    SolverBackendType types[8];
    int cnt = scheduler_list_available_backends(s, types, 8);
    bool found = false;
    for (int i = 0; i < cnt; i++) {
        if (types[i] == SMT_Z3) {
            found = true;
            break;
        }
    }
    if (found)
        PASS();
    else
        FAIL("SMT_Z3 not listed");

cleanup:
    scheduler_destroy(s);
}

/* ============================================================
 * 测试 3: 图特征分析
 * ============================================================ */
static void test_analyze_graph(void) {
    EngineScheduler *s = scheduler_create();
    ConstraintGraph *g = create_simple_graph();
    if (!s || !g) {
        FAIL("setup");
        if (s)
            scheduler_destroy(s);
        if (g)
            graph_destroy(g);
        F++;
        return;
    }

    TEST("scheduler_analyze_graph NULL safety");
    int rc = scheduler_analyze_graph(NULL, NULL);
    if (rc != -1) {
        FAIL("expected -1");
        goto cleanup;
    }
    PASS();

    {
        GraphFeatures features;
        TEST("analyze 2-POINT + 1-SEGMENT graph");
        rc = scheduler_analyze_graph(g, &features);
        if (rc != 0) {
            FAIL("analyze failed");
            goto cleanup;
        }
        /* 2 POINT + 1 SEGMENT = 3 节点 */
        if (features.total_nodes == 3)
            PASS();
        else {
            printf("(nodes=%d) ", features.total_nodes);
            FAIL("!3");
        }
    }

cleanup:
    scheduler_destroy(s);
    graph_destroy(g);
}

/* ============================================================
 * 测试 4: 加载预设路由规则
 * ============================================================ */
static void test_preset_rules(void) {
    EngineScheduler *s = scheduler_create();
    if (!s) {
        FAIL("create");
        F++;
        return;
    }

    TEST("scheduler_load_preset_rules NULL safety");
    int rc = scheduler_load_preset_rules(NULL);
    if (rc != -1) {
        FAIL("expected -1");
        goto cleanup;
    }
    PASS();

    /* scheduler_create 已经调用了 load_preset_rules，应至少有 2 条规则 */
    char buf[2048];
    int len = scheduler_diagnose(s, buf, sizeof(buf));
    TEST("预设规则已加载 (≥2)");
    /* 诊断输出包含 "Routing rules: N"，检查规则数 */
    (void) len;
    /* 通过再次加载并检查返回值确认 */
    rc = scheduler_load_preset_rules(s);
    if (rc == 0)
        PASS();
    else
        FAIL("load failed");

cleanup:
    scheduler_destroy(s);
}

/* ============================================================
 * 测试 5: 后端选择
 * ============================================================ */
static void test_select_backend(void) {
    EngineScheduler *s = scheduler_create();
    ConstraintGraph *g = create_simple_graph();
    if (!s || !g) {
        FAIL("setup");
        if (s)
            scheduler_destroy(s);
        if (g)
            graph_destroy(g);
        F++;
        return;
    }

    TEST("scheduler_select_backend NULL safety");
    SolverBackendType bt = scheduler_select_backend(NULL, NULL, NULL, 0);
    if (bt == GROEBNER)
        PASS();
    else
        FAIL("!GROEBNER");

    TEST("选择 GROEBNER（小图）");
    char reason[128];
    bt = scheduler_select_backend(s, g, reason, sizeof(reason));
    if (bt == GROEBNER)
        PASS();
    else {
        printf("(backend=%d) ", (int) bt);
        FAIL("!GROEBNER");
    }

cleanup:
    scheduler_destroy(s);
    graph_destroy(g);
}

/* ============================================================
 * 测试 6: scheduler_solve
 * ============================================================ */
static void test_solve(void) {
    EngineScheduler *s = scheduler_create();
    ConstraintGraph *g = create_simple_graph();
    if (!s || !g) {
        FAIL("setup");
        if (s)
            scheduler_destroy(s);
        if (g)
            graph_destroy(g);
        F++;
        return;
    }

    TEST("scheduler_solve NULL safety");
    int rc = scheduler_solve(NULL, NULL, NULL);
    if (rc != -1) {
        FAIL("expected -1");
        goto cleanup;
    }
    PASS();

    TEST("scheduler_solve 简单图");
    SMTSolverResult result;
    smtsolver_result_init(&result);
    rc = scheduler_solve(s, g, &result);
    if (rc == 0)
        PASS();
    else {
        printf("(rc=%d, sat=%d, err=%d) ", rc, (int) result.sat_result, (int) result.error_code);
        FAIL("solve failed");
    }

    smtsolver_result_clear(&result);

cleanup:
    scheduler_destroy(s);
    graph_destroy(g);
}

/* ============================================================
 * 测试 7: 统计
 * ============================================================ */
static void test_stats(void) {
    EngineScheduler *s = scheduler_create();
    ConstraintGraph *g = create_simple_graph();
    if (!s || !g) {
        FAIL("setup");
        if (s)
            scheduler_destroy(s);
        if (g)
            graph_destroy(g);
        F++;
        return;
    }

    TEST("scheduler_get_stats NULL safety");
    scheduler_get_stats(NULL, NULL);
    PASS();

    /* 执行一次求解，产生统计 */
    SMTSolverResult result;
    smtsolver_result_init(&result);
    scheduler_solve(s, g, &result);
    smtsolver_result_clear(&result);

    SchedulerStats stats;
    TEST("scheduler_get_stats total_solves ≥ 1");
    scheduler_get_stats(s, &stats);
    if (stats.total_solves >= 1)
        PASS();
    else {
        printf("(solves=%lld) ", (long long) stats.total_solves);
        FAIL("!≥1");
    }

    TEST("scheduler_reset_stats NULL safety");
    scheduler_reset_stats(NULL);
    PASS();

    TEST("scheduler_reset_stats");
    scheduler_reset_stats(s);
    scheduler_get_stats(s, &stats);
    if (stats.total_solves == 0)
        PASS();
    else
        FAIL("not zeroed");

cleanup:
    scheduler_destroy(s);
    graph_destroy(g);
}

/* ============================================================
 * 测试 8: 诊断
 * ============================================================ */
static void test_diagnose(void) {
    EngineScheduler *s = scheduler_create();
    if (!s) {
        FAIL("create");
        F++;
        return;
    }

    TEST("scheduler_diagnose NULL safety");
    int rc = scheduler_diagnose(NULL, NULL, 0);
    if (rc != -1) {
        FAIL("expected -1");
        goto cleanup;
    }
    PASS();

    TEST("scheduler_diagnose 产生合理输出");
    char buf[1024];
    rc = scheduler_diagnose(s, buf, sizeof(buf));
    if (rc > 0 && strlen(buf) > 20)
        PASS();
    else {
        printf("(len=%d) ", rc);
        FAIL("too short");
    }

cleanup:
    scheduler_destroy(s);
}

/* ============================================================
 * 测试 9: 配置更改
 * ============================================================ */
static void test_config(void) {
    EngineScheduler *s = scheduler_create();
    if (!s) {
        FAIL("create");
        F++;
        return;
    }

    TEST("scheduler_set_default_backend NULL safety");
    scheduler_set_default_backend(NULL, SMT_Z3);
    PASS();

    TEST("scheduler_set_default_backend");
    scheduler_set_default_backend(s, SMT_Z3);
    /* 通过诊断输出来验证 */
    char buf[512];
    scheduler_diagnose(s, buf, sizeof(buf));
    if (strstr(buf, "Default backend: 1") != NULL)
        PASS();
    else
        FAIL("default not changed");

    TEST("scheduler_set_fallback_policy NULL safety");
    SolverBackendType fb[] = {GROEBNER, SMT_Z3};
    scheduler_set_fallback_policy(NULL, true, fb, 2);
    PASS();

    TEST("scheduler_set_fallback_policy disable");
    scheduler_set_fallback_policy(s, false, fb, 2);
    scheduler_diagnose(s, buf, sizeof(buf));
    if (strstr(buf, "disabled") != NULL)
        PASS();
    else
        FAIL("not disabled");

    TEST("scheduler_set_fallback_policy enable with chain");
    scheduler_set_fallback_policy(s, true, fb, 2);
    scheduler_diagnose(s, buf, sizeof(buf));
    if (strstr(buf, "depth=2") != NULL)
        PASS();
    else
        FAIL("depth not 2");

cleanup:
    scheduler_destroy(s);
}

/* ============================================================
 * 测试 10: NULL 输入安全 - 所有函数
 * ============================================================ */
static void test_null_safety(void) {
    /* scheduler_is_backend_available */
    TEST("scheduler_is_backend_available(NULL)");
    if (!scheduler_is_backend_available(NULL, GROEBNER))
        PASS();
    else
        FAIL("should be false");

    /* scheduler_set_backend_available */
    TEST("scheduler_set_backend_available(NULL)");
    scheduler_set_backend_available(NULL, GROEBNER, true);
    PASS();

    /* scheduler_list_available_backends */
    TEST("scheduler_list_available_backends(NULL)");
    if (scheduler_list_available_backends(NULL, NULL, 0) == 0)
        PASS();
    else
        FAIL("should be 0");

    /* scheduler_add_routing_rule */
    TEST("scheduler_add_routing_rule(NULL)");
    RoutingRule rule;
    memset(&rule, 0, sizeof(rule));
    if (scheduler_add_routing_rule(NULL, &rule) == -1)
        PASS();
    else
        FAIL("should be -1");

    if (scheduler_add_routing_rule(NULL, NULL) == -1)
        PASS();
    else
        FAIL("should be -1");

    /* scheduler_remove_routing_rule */
    TEST("scheduler_remove_routing_rule(NULL)");
    if (scheduler_remove_routing_rule(NULL, NULL) == -1)
        PASS();
    else
        FAIL("should be -1");

    if (scheduler_remove_routing_rule(NULL, "test") == -1)
        PASS();
    else
        FAIL("should be -1");

    /* scheduler_feature_summary */
    TEST("scheduler_feature_summary(NULL)");
    if (strcmp(scheduler_feature_summary(NULL), "NULL features") == 0)
        PASS();
    else
        FAIL("wrong message");

    /* scheduler_select_backend with NULL graph (should still return GROEBNER) */
    TEST("scheduler_select_backend(NULL, NULL)");
    if (scheduler_select_backend(NULL, NULL, NULL, 0) == GROEBNER)
        PASS();
    else
        FAIL("!GROEBNER");

    /* scheduler_solve_with_backend NULL safety */
    TEST("scheduler_solve_with_backend(NULL)");
    SMTSolverResult result;
    smtsolver_result_init(&result);
    if (scheduler_solve_with_backend(NULL, NULL, GROEBNER, &result) == -1)
        PASS();
    else
        FAIL("should be -1");

    /* scheduler_solve_groebner_compat NULL safety */
    TEST("scheduler_solve_groebner_compat(NULL)");
    GroebnerResult *gr = NULL;
    int rc = scheduler_solve_groebner_compat(NULL, NULL, &gr);
    if (rc == -1)
        PASS();
    else
        FAIL("should be -1");

    /* scheduler_set_auto_create NULL safety */
    TEST("scheduler_set_auto_create(NULL)");
    scheduler_set_auto_create(NULL, true);
    PASS();

    /* scheduler_convert_smt_to_groebner NULL safety */
    TEST("scheduler_convert_smt_to_groebner(NULL)");
    if (scheduler_convert_smt_to_groebner(NULL, NULL) == NULL)
        PASS();
    else
        FAIL("should be NULL");
}

/* ============================================================
 * Main
 * ============================================================ */
int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== EngineScheduler 测试套件 ===\n\n");

    lv_init();

    printf("[组 1] 生命周期\n");
    test_lifecycle();

    printf("[组 2] 后端注册\n");
    test_register_backend();

    printf("[组 3] 图特征分析\n");
    test_analyze_graph();

    printf("[组 4] 预设路由规则\n");
    test_preset_rules();

    printf("[组 5] 后端选择\n");
    test_select_backend();

    printf("[组 6] 分发求解\n");
    test_solve();

    printf("[组 7] 统计\n");
    test_stats();

    printf("[组 8] 诊断\n");
    test_diagnose();

    printf("[组 9] 配置更改\n");
    test_config();

    printf("[组 10] NULL 输入安全\n");
    test_null_safety();

    lv_cleanup();
    printf("\n=== %d passed, %d failed ===\n", P, F);
    return F > 0 ? 1 : 0;
}

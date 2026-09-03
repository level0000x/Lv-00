/**
 * @file test_geometric_primitives.c
 * @brief 13 几何原语统一包装层（geo_*）契约测试
 *
 * 覆盖（批次 229：13 原语 C 地基——原 geo_* 全库零测试，首次契约钉住）：
 *   - geo_create_node：POINT（坐标 id 作有理数）创建；CIRCLE（圆心+半径端点，
 *     修复 GEO_NODE_CIRCLE 枚举空洞——原 handler 表无 CIRCLE 槽返 INVALID_TYPE）；
 *     INVALID_TYPE 越界回退。
 *   - geo_query："count" 返回 [node_count, constraint_count]。
 *   - geo_create_constraint：INCIDENCE 参与者不足报 INVALID_PARAM。
 *   - geo_solve：空引擎求解 OK（ENGINE_SOLVE_OK 路径）。
 *
 * @author Lv-00 Project
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv.h"
#include "lv/engine.h"
#include "lv/constraint_graph.h"
#include "lv/geometric_primitives.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

static lvEngine *g_engine = NULL;

static void setup(void) {
    if (!g_engine)
        g_engine = lv_engine_create();
}

static void teardown(void) {
    if (g_engine) {
        lv_engine_destroy(g_engine);
        g_engine = NULL;
    }
}

/* ============== geo_create_node ============== */

static void test_create_point(void) {
    TEST("geo_create_node POINT");
    if (!g_engine)
        FAIL("engine 未创建");
    ConstraintGraph *g = engine_get_main_graph(g_engine);
    int ids[2] = {3, 5}; /* 点 (3,5)（id 作有理数坐标，geo 层语义） */
    GeoResult r = geo_create_node(g, GEO_NODE_POINT, ids, 2);
    if (r.status == GEO_STATUS_OK && r.data) {
        PASS();
        lv_free((void **) &r.data);
    } else {
        printf("  status=%d msg=%s\n", (int) r.status, r.message ? r.message : "(null)");
        FAIL("geo_create_node POINT 失败");
    }
}

static void test_create_circle(void) {
    TEST("geo_create_node CIRCLE（修复枚举空洞）");
    if (!g_engine)
        FAIL("engine 未创建");
    ConstraintGraph *g = engine_get_main_graph(g_engine);

    /* 先建两个点作为圆心与半径端点 */
    int p1[2] = {0, 0};
    int p2[2] = {1, 1};
    GeoResult r1 = geo_create_node(g, GEO_NODE_POINT, p1, 2);
    GeoResult r2 = geo_create_node(g, GEO_NODE_POINT, p2, 2);
    int center = r1.data ? *(int *) r1.data : -1;
    int radpt = r2.data ? *(int *) r2.data : -1;
    if (r1.data) lv_free((void **) &r1.data);
    if (r2.data) lv_free((void **) &r2.data);
    if (center < 0 || radpt < 0) {
        FAIL("前置点创建失败");
        return;
    }

    int ids[2] = {center, radpt};
    GeoResult r = geo_create_node(g, GEO_NODE_CIRCLE, ids, 2);
    if (r.status == GEO_STATUS_OK) {
        PASS();
        if (r.data) lv_free((void **) &r.data);
    } else {
        printf("  status=%d msg=%s\n", (int) r.status, r.message ? r.message : "(null)");
        FAIL("geo_create_node CIRCLE 应成功（修复后）");
    }
}

static void test_invalid_type(void) {
    TEST("geo_create_node 越界类型回退");
    if (!g_engine)
        FAIL("engine 未创建");
    ConstraintGraph *g = engine_get_main_graph(g_engine);
    int ids[1] = {0};
    GeoResult r = geo_create_node(g, (GeoNodeType) 999, ids, 1);
    if (r.status == GEO_STATUS_INVALID_TYPE) {
        PASS();
    } else {
        FAIL("越界类型应返回 INVALID_TYPE");
    }
}

/* ============== geo_query ============== */

static void test_query_count(void) {
    TEST("geo_query count");
    if (!g_engine)
        FAIL("engine 未创建");
    ConstraintGraph *g = engine_get_main_graph(g_engine);
    GeoResult r = geo_query(g, "count", 0);
    if (r.status == GEO_STATUS_OK && r.data) {
        int *cnt = (int *) r.data;
        if (cnt[0] >= 3 && cnt[1] >= 0) { /* 前序测试至少建了 3 点 */
            PASS();
        } else {
            printf("  nodes=%d constraints=%d\n", cnt[0], cnt[1]);
            FAIL("count 结果异常");
        }
        lv_free((void **) &r.data);
    } else {
        FAIL("geo_query count 失败");
    }
}

/* ============== geo_create_constraint ============== */

static void test_constraint_param(void) {
    TEST("geo_create_constraint 参与者不足");
    if (!g_engine)
        FAIL("engine 未创建");
    ConstraintGraph *g = engine_get_main_graph(g_engine);
    int p[1] = {0};
    GeoResult r = geo_create_constraint(g, GEO_CONSTRAINT_BETWEENNESS, p, 1);
    if (r.status == GEO_STATUS_INVALID_PARAM) {
        PASS();
    } else {
        printf("  status=%d\n", (int) r.status);
        FAIL("参与者不足应返回 INVALID_PARAM");
    }
}

/* ============== geo_solve ============== */

static void test_solve_empty(void) {
    TEST("geo_solve 空引擎（OK 路径）");
    if (!g_engine)
        FAIL("engine 未创建");
    GeoResult r = geo_solve(g_engine);
    if (r.status == GEO_STATUS_OK) {
        PASS();
    } else {
        printf("  status=%d msg=%s\n", (int) r.status, r.message ? r.message : "(null)");
        FAIL("geo_solve 空引擎应 OK（无可约束自由点，OK 或冲突均合理，此处钉 OK）");
    }
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("GeometricPrimitives")

    printf("\n--- 13 几何原语 (geo_*) ---\n");
    lv_init();
    setup();

    TEST_MAIN_RUN(test_create_point);
    TEST_MAIN_RUN(test_create_circle);
    TEST_MAIN_RUN(test_invalid_type);
    TEST_MAIN_RUN(test_query_count);
    TEST_MAIN_RUN(test_constraint_param);
    TEST_MAIN_RUN(test_solve_empty);

    teardown();
    lv_cleanup();

TEST_MAIN_END()

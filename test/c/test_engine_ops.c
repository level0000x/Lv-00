/**
 * @file test_engine_ops.c
 * @brief 引擎核心操作测试
 *
 * 覆盖引擎生命周期、几何构造、归一化、求解、状态管理、流式输出。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lv.h"
#include "engine.h"

#define TEST(n) printf("  [TEST] %s ... ", n)
#define PASS()  do { printf("PASS\n"); P++; } while(0)
#define FAIL(m) do { printf("FAIL: %s\n", m); F++; } while(0)

static int P = 0, F = 0;

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== engine 核心操作测试 ===\n\n");

    lv_init();

    /* ── 组 1: 引擎生命周期 ── */
    printf("[组 1] 引擎生命周期\n");
    {
        TEST("engine_create"); lvEngine *e = lv_engine_create();
        if (e) PASS(); else FAIL("NULL");

        TEST("engine_destroy NULL安全"); lv_engine_destroy(NULL); PASS();

        TEST("engine_destroy"); lv_engine_destroy(e); PASS();
    }

    /* ── 组 2: 状态管理 ── */
    printf("[组 2] 状态管理\n");
    {
        lvEngine *e = lv_engine_create();
        if (!e) { FAIL("create"); F = 99; goto g2_end; }

        TEST("初始状态=IDLE");
        if (engine_get_state(e) == ENGINE_STATE_IDLE) PASS(); else FAIL("!IDLE");

        TEST("is_busy=false");
        if (!engine_is_busy(e)) PASS(); else FAIL("busy");

        TEST("state_name非NULL");
        if (engine_state_name(ENGINE_STATE_IDLE)) PASS(); else FAIL("NULL");

        TEST("valid_transition IDLE→PARSING");
        if (engine_is_valid_transition(ENGINE_STATE_IDLE, ENGINE_STATE_PARSING)) PASS(); else FAIL("!valid");

        TEST("invalid_transition ERROR→PARSING");
        if (!engine_is_valid_transition(ENGINE_STATE_ERROR, ENGINE_STATE_PARSING)) PASS(); else FAIL("valid");

        TEST("transition OK");
        if (lv_engine_transition_state(e, ENGINE_STATE_PARSING) == ENGINE_STATUS_OK) PASS(); else FAIL("FAIL");

        TEST("transition后状态=PARSING");
        if (engine_get_state(e) == ENGINE_STATE_PARSING) PASS(); else FAIL("!PARSING");

        /* 回到 IDLE */
        lv_engine_transition_state(e, ENGINE_STATE_IDLE);

        TEST("last_status_OK");
        if (engine_get_last_status(e) == ENGINE_STATUS_OK) PASS(); else FAIL("!OK");

        lv_engine_destroy(e);
g2_end: ;
    }

    /* ── 组 3: 基本几何构造 ── */
    printf("[组 3] 基本几何构造\n");
    {
        lvEngine *e = lv_engine_create();
        if (!e) { FAIL("create"); F = 99; goto g3_end; }

        TEST("add_point 原点");
        int p0 = lv_add_point(e, 0, 1, 0, 1);
        if (p0 >= 0) PASS(); else FAIL("FAIL");

        TEST("add_point (1,0)");
        int p1 = lv_add_point(e, 1, 1, 0, 1);
        if (p1 > p0) PASS(); else FAIL("!递增");

        TEST("add_point (0,1)");
        int p2 = lv_add_point(e, 0, 1, 1, 1);
        if (p2 > p1) PASS(); else FAIL("!递增");

        TEST("add_line_segment");
        int s1 = lv_add_line_segment(e, p0, p1);
        if (s1 >= 0) PASS(); else FAIL("FAIL");

        TEST("add_line_segment 2");
        int s2 = lv_add_line_segment(e, p1, p2);
        if (s2 >= 0) PASS(); else FAIL("FAIL");

        TEST("add_line_segment 3");
        int s3 = lv_add_line_segment(e, p2, p0);
        if (s3 >= 0) PASS(); else FAIL("FAIL");

        lv_engine_destroy(e);
g3_end: ;
    }

    /* ── 组 4: 流式输出 ── */
    printf("[组 4] 流式输出\n");
    {
        lvEngine *e = lv_engine_create();
        if (!e) { FAIL("create"); F = 99; goto g4_end; }

        TEST("stream enabled default");
        StreamContext *sc = engine_get_stream_context(e);
        if (sc != NULL) PASS(); else FAIL("NULL");

        TEST("disable streaming");
        engine_set_streaming_enabled(e, false);
        if (!engine_is_streaming_enabled(e)) PASS(); else FAIL("still enabled");

        TEST("re-enable streaming");
        engine_set_streaming_enabled(e, true);
        if (engine_is_streaming_enabled(e)) PASS(); else FAIL("disabled");

        lv_engine_destroy(e);
g4_end: ;
    }

    /* ── 组 5: 错误处理 ── */
    printf("[组 5] 错误处理\n");
    {
        TEST("status_to_string 非NULL");
        if (engine_status_to_string(ENGINE_STATUS_OK)) PASS(); else FAIL("NULL");

        TEST("status_to_identifier 非NULL");
        if (engine_status_to_identifier(ENGINE_STATUS_OK)) PASS(); else FAIL("NULL");

        TEST("status_get_description 非NULL");
        if (engine_status_get_description(ENGINE_STATUS_OK)) PASS(); else FAIL("NULL");

        TEST("engine_solve(NULL)安全");
        EngineSolveResult sr = engine_solve(NULL);
        if (sr == ENGINE_SOLVE_ERROR) PASS(); else FAIL("!ERROR");

        TEST("engine_get_last_status(NULL)=OK");
        if (engine_get_last_status(NULL) == ENGINE_STATUS_OK) PASS(); else FAIL("!OK");

        TEST("engine_get_state(NULL)=IDLE");
        if (engine_get_state(NULL) == ENGINE_STATE_IDLE) PASS(); else FAIL("!IDLE");
    }

    lv_cleanup();
    printf("\n=== %d passed, %d failed ===\n", P, F);
    return F > 0 ? 1 : 0;
}

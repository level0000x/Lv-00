/**
 * @file test_engine_ops.c
 * @brief 引擎核心操作测试
 *
 * 覆盖引擎生命周期、几何构造、归一化、求解、状态管理、流式输出。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine.h"
#include "lv.h"

/* 使用共享 TEST/PASS/FAIL 宏；计数挂钩保持原有 P/F 计数行为 */
#define TEST_PASS_STATEMENT P++
#define TEST_FAIL_STATEMENT F++

#include "test_helpers.h"

static int P = 0, F = 0;

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== engine 核心操作测试 ===\n\n");

    lv_init();

    /* ── 组 1: 引擎生命周期 ── */
    printf("[组 1] 引擎生命周期\n");
    {
        TEST("engine_create");
        lvEngine *e = lv_engine_create();
        if (e)
            PASS();
        else
            FAIL("NULL");

        TEST("engine_destroy NULL安全");
        lv_engine_destroy(NULL);
        PASS();

        TEST("engine_destroy");
        lv_engine_destroy(e);
        PASS();
    }

    /* ── 组 2: 状态管理 ── */
    printf("[组 2] 状态管理\n");
    {
        lvEngine *e = lv_engine_create();
        if (!e) {
            FAIL("create");
            F = 99;
            goto g2_end;
        }

        TEST("初始状态=IDLE");
        if (engine_get_state(e) == ENGINE_STATE_IDLE)
            PASS();
        else
            FAIL("!IDLE");

        TEST("is_busy=false");
        if (!engine_is_busy(e))
            PASS();
        else
            FAIL("busy");

        TEST("state_name非NULL");
        if (engine_state_name(ENGINE_STATE_IDLE))
            PASS();
        else
            FAIL("NULL");

        TEST("valid_transition IDLE→PARSING");
        if (engine_is_valid_transition(ENGINE_STATE_IDLE, ENGINE_STATE_PARSING))
            PASS();
        else
            FAIL("!valid");

        TEST("invalid_transition ERROR→PARSING");
        if (!engine_is_valid_transition(ENGINE_STATE_ERROR, ENGINE_STATE_PARSING))
            PASS();
        else
            FAIL("valid");

        TEST("transition OK");
        if (lv_engine_transition_state(e, ENGINE_STATE_PARSING) == ENGINE_STATUS_OK)
            PASS();
        else
            FAIL("FAIL");

        TEST("transition后状态=PARSING");
        if (engine_get_state(e) == ENGINE_STATE_PARSING)
            PASS();
        else
            FAIL("!PARSING");

        /* 回到 IDLE */
        lv_engine_transition_state(e, ENGINE_STATE_IDLE);

        TEST("last_status_OK");
        if (engine_get_last_status(e) == ENGINE_STATUS_OK)
            PASS();
        else
            FAIL("!OK");

        lv_engine_destroy(e);
    g2_end:;
    }

    /* ── 组 3: 基本几何构造 ── */
    printf("[组 3] 基本几何构造\n");
    {
        lvEngine *e = lv_engine_create();
        if (!e) {
            FAIL("create");
            F = 99;
            goto g3_end;
        }

        TEST("add_point 原点");
        int p0 = lv_add_point(e, 0, 1, 0, 1);
        if (p0 >= 0)
            PASS();
        else
            FAIL("FAIL");

        TEST("add_point (1,0)");
        int p1 = lv_add_point(e, 1, 1, 0, 1);
        if (p1 > p0)
            PASS();
        else
            FAIL("!递增");

        TEST("add_point (0,1)");
        int p2 = lv_add_point(e, 0, 1, 1, 1);
        if (p2 > p1)
            PASS();
        else
            FAIL("!递增");

        TEST("add_line_segment");
        int s1 = lv_add_line_segment(e, p0, p1);
        if (s1 >= 0)
            PASS();
        else
            FAIL("FAIL");

        TEST("add_line_segment 2");
        int s2 = lv_add_line_segment(e, p1, p2);
        if (s2 >= 0)
            PASS();
        else
            FAIL("FAIL");

        TEST("add_line_segment 3");
        int s3 = lv_add_line_segment(e, p2, p0);
        if (s3 >= 0)
            PASS();
        else
            FAIL("FAIL");

        lv_engine_destroy(e);
    g3_end:;
    }

    /* ── 组 4: 流式输出 ── */
    printf("[组 4] 流式输出\n");
    {
        lvEngine *e = lv_engine_create();
        if (!e) {
            FAIL("create");
            F = 99;
            goto g4_end;
        }

        TEST("stream enabled default");
        StreamContext *sc = engine_get_stream_context(e);
        if (sc != NULL)
            PASS();
        else
            FAIL("NULL");

        TEST("disable streaming");
        engine_set_streaming_enabled(e, false);
        if (!engine_is_streaming_enabled(e))
            PASS();
        else
            FAIL("still enabled");

        TEST("re-enable streaming");
        engine_set_streaming_enabled(e, true);
        if (engine_is_streaming_enabled(e))
            PASS();
        else
            FAIL("disabled");

        lv_engine_destroy(e);
    g4_end:;
    }

    /* ── 组 5: 错误处理 ── */
    printf("[组 5] 错误处理\n");
    {
        TEST("status_to_string 非NULL");
        if (engine_status_to_string(ENGINE_STATUS_OK))
            PASS();
        else
            FAIL("NULL");

        TEST("status_to_identifier 非NULL");
        if (engine_status_to_identifier(ENGINE_STATUS_OK))
            PASS();
        else
            FAIL("NULL");

        TEST("status_get_description 非NULL");
        if (engine_status_get_description(ENGINE_STATUS_OK))
            PASS();
        else
            FAIL("NULL");

        TEST("engine_solve(NULL)安全");
        EngineSolveResult sr = engine_solve(NULL);
        if (sr == ENGINE_SOLVE_ERROR)
            PASS();
        else
            FAIL("!ERROR");

        TEST("engine_get_last_status(NULL)=OK");
        if (engine_get_last_status(NULL) == ENGINE_STATUS_OK)
            PASS();
        else
            FAIL("!OK");

        TEST("engine_get_state(NULL)=IDLE");
        if (engine_get_state(NULL) == ENGINE_STATE_IDLE)
            PASS();
        else
            FAIL("!IDLE");
    }

    /* ── 组 6: 基本求解 ── */
    printf("[组 6] 基本求解\n");
    {
        lvEngine *e = lv_engine_create();
        if (!e) {
            FAIL("create");
            F = 99;
            goto g6_end;
        }

        int p0 = lv_add_point(e, 0, 1, 0, 1);
        int p1 = lv_add_point(e, 1, 1, 0, 1);
        int p2 = lv_add_point(e, 0, 1, 1, 1);
        if (p0 < 0 || p1 < 0 || p2 < 0) {
            FAIL("add_point");
            goto g6_cleanup;
        }

        lv_add_line_segment(e, p0, p1);
        lv_add_line_segment(e, p1, p2);
        lv_add_line_segment(e, p2, p0);

        TEST("lv_solve 三角形");
        EngineSolveResult sr = lv_solve(e);
        if (sr == ENGINE_SOLVE_OK)
            PASS();
        else {
            printf("  -> %d", (int) sr);
            FAIL("!OK");
        }

        lv_engine_destroy(e);

        /* 正方形 */
        e = lv_engine_create();
        if (!e) {
            FAIL("create2");
            F = 99;
            goto g6_end;
        }

        p0 = lv_add_point(e, 0, 1, 0, 1);
        p1 = lv_add_point(e, 1, 1, 0, 1);
        p2 = lv_add_point(e, 1, 1, 1, 1);
        int p3 = lv_add_point(e, 0, 1, 1, 1);
        lv_add_line_segment(e, p0, p1);
        lv_add_line_segment(e, p1, p2);
        lv_add_line_segment(e, p2, p3);
        lv_add_line_segment(e, p3, p0);

        TEST("lv_solve 正方形");
        sr = lv_solve(e);
        if (sr == ENGINE_SOLVE_OK)
            PASS();
        else {
            printf("  -> %d", (int) sr);
            FAIL("!OK");
        }

    g6_cleanup:
        lv_engine_destroy(e);
    g6_end:;
    }

    /* ── 组 7: 带 INCIDENCE 约束的求解 ── */
    printf("[组 7] 带 INCIDENCE 约束的求解\n");
    {
        lvEngine *e = lv_engine_create();
        if (!e) {
            FAIL("create");
            F = 99;
            goto g7_end;
        }

        int p0 = lv_add_point(e, 0, 1, 0, 1);
        int p1 = lv_add_point(e, 1, 1, 0, 1);
        int p2 = lv_add_point(e, 0, 1, 1, 1);
        int s1 = lv_add_line_segment(e, p0, p1);
        int s2 = lv_add_line_segment(e, p1, p2);
        int s3 = lv_add_line_segment(e, p2, p0);
        lv_add_constraint_incidence(e, p0, s1);
        lv_add_constraint_incidence(e, p1, s1);
        lv_add_constraint_incidence(e, p1, s2);
        lv_add_constraint_incidence(e, p2, s2);
        lv_add_constraint_incidence(e, p2, s3);
        lv_add_constraint_incidence(e, p0, s3);

        TEST("lv_solve 三角形+incidence");
        EngineSolveResult sr = lv_solve(e);
        if (sr == ENGINE_SOLVE_OK)
            PASS();
        else {
            printf("  -> %d", (int) sr);
            FAIL("!OK");
        }

        lv_engine_destroy(e);

        /* 正方形 + incidence */
        e = lv_engine_create();
        if (!e) {
            FAIL("create2");
            F = 99;
            goto g7_end;
        }

        p0 = lv_add_point(e, 0, 1, 0, 1);
        p1 = lv_add_point(e, 1, 1, 0, 1);
        p2 = lv_add_point(e, 1, 1, 1, 1);
        int p3 = lv_add_point(e, 0, 1, 1, 1);
        s1 = lv_add_line_segment(e, p0, p1);
        s2 = lv_add_line_segment(e, p1, p2);
        s3 = lv_add_line_segment(e, p2, p3);
        int s4 = lv_add_line_segment(e, p3, p0);
        lv_add_constraint_incidence(e, p0, s1);
        lv_add_constraint_incidence(e, p1, s1);
        lv_add_constraint_incidence(e, p1, s2);
        lv_add_constraint_incidence(e, p2, s2);
        lv_add_constraint_incidence(e, p2, s3);
        lv_add_constraint_incidence(e, p3, s3);
        lv_add_constraint_incidence(e, p3, s4);
        lv_add_constraint_incidence(e, p0, s4);

        TEST("lv_solve 正方形+incidence");
        sr = lv_solve(e);
        if (sr == ENGINE_SOLVE_OK)
            PASS();
        else {
            printf("  -> %d", (int) sr);
            FAIL("!OK");
        }

        lv_engine_destroy(e);
    g7_end:;
    }

    /* ── 组 8: 求解边界条件 ── */
    printf("[组 8] 求解边界条件\n");
    {
        /* 空图求解 */
        lvEngine *e = lv_engine_create();
        if (!e) {
            FAIL("create");
            F = 99;
            goto g8_end;
        }

        TEST("lv_solve 空图");
        EngineSolveResult sr = lv_solve(e);
        if (sr == ENGINE_SOLVE_OK)
            PASS();
        else {
            printf("  -> %d", (int) sr);
            FAIL("!OK");
        }

        /* 多次求解 */
        int p0 = lv_add_point(e, 0, 1, 0, 1);
        int p1 = lv_add_point(e, 1, 1, 0, 1);
        int p2 = lv_add_point(e, 0, 1, 1, 1);
        lv_add_line_segment(e, p0, p1);
        lv_add_line_segment(e, p1, p2);
        lv_add_line_segment(e, p2, p0);

        bool all_ok = true;
        for (int i = 0; i < 5; i++) {
            sr = lv_solve(e);
            if (sr != ENGINE_SOLVE_OK) {
                all_ok = false;
                break;
            }
        }
        TEST("lv_solve 连续5次");
        if (all_ok)
            PASS();
        else
            FAIL("某次失败");

        /* 多点无线段 */
        lv_add_point(e, 2, 1, 2, 1); /* 额外点 */
        TEST("lv_solve 4点3线段");
        sr = lv_solve(e);
        if (sr == ENGINE_SOLVE_OK)
            PASS();
        else {
            printf("  -> %d", (int) sr);
            FAIL("!OK");
        }

        lv_engine_destroy(e);
    g8_end:;
    }

    lv_cleanup();
    printf("\n=== %d passed, %d failed ===\n", P, F);
    return F > 0 ? 1 : 0;
}

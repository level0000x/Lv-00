/**
 * @file test_engine_ops.c
 * @brief 引擎核心操作测试
 *
 * 覆盖引擎生命周期、几何构造、归一化、求解、状态管理、流式输出。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* engine_scheduler.h 必须在 lv.h 之前包含，以确保 smt_backend.h 在 proof.h 之前处理 */
#include "lv/engine_scheduler.h"

#include "engine.h"
#include "lv.h"
#include "context.h"
#include "ecosystem.h"
#include "runtime_monitor.h"

/* 使用共享 TEST/PASS/FAIL 宏；计数挂钩保持原有 P/F 计数行为 */
#define TEST_PASS_STATEMENT g_pass_count++
#define TEST_FAIL_STATEMENT g_fail_count++

#include "test_helpers.h"

int g_pass_count = 0;
int g_fail_count = 0;

TEST_MAIN_BEGIN("engine 核心操作测试")
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
            g_fail_count = 99;
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
            g_fail_count = 99;
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
            g_fail_count = 99;
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
            g_fail_count = 99;
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
            g_fail_count = 99;
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
            g_fail_count = 99;
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
            g_fail_count = 99;
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
            g_fail_count = 99;
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
    /* ── 组 9: 引擎状态机边界 ── */
    printf("[组 9] 引擎状态机边界\n");
    {
        lvEngine *e = lv_engine_create();
        if (!e) {
            FAIL("create");
            g_fail_count = 99;
            goto g9_end;
        }
        TEST("engine self-transition 幂等");
        if (lv_engine_transition_state(e, ENGINE_STATE_IDLE) == ENGINE_STATUS_OK &&
            engine_get_state(e) == ENGINE_STATE_IDLE)
            PASS();
        else
            FAIL("!OK");
        TEST("engine 越界目标状态拒绝");
        if (lv_engine_transition_state(e, (EngineState) 99) == ENGINE_STATUS_INVALID_STATE)
            PASS();
        else
            FAIL("!INVALID_STATE");
        TEST("engine_is_valid_transition 上界越界安全");
        if (!engine_is_valid_transition((EngineState) 99, ENGINE_STATE_IDLE) &&
            !engine_is_valid_transition(ENGINE_STATE_IDLE, (EngineState) 99) &&
            !engine_is_valid_transition((EngineState) 99, (EngineState) 99))
            PASS();
        else
            FAIL("越界未拒绝");
        TEST("engine 非法转移 IDLE→COMPLETE");
        if (lv_engine_transition_state(e, ENGINE_STATE_COMPLETE) == ENGINE_STATUS_INVALID_STATE &&
            engine_get_state(e) == ENGINE_STATE_IDLE)
            PASS();
        else
            FAIL("非法转移未被拒绝");
        TEST("engine_state_name 越界回退");
        if (engine_state_name((EngineState) 99) != NULL)
            PASS();
        else
            FAIL("回退为 NULL");
        lv_engine_destroy(e);
    g9_end:;
    }

    /* ── 组 10: 上下文状态机 ── */
    printf("[组 10] 上下文状态机\n");
    {
        lvContext *ctx = lv_context_create();
        if (!ctx) {
            FAIL("create");
            g_fail_count = 99;
            goto g10_end;
        }
        TEST("context 初始状态 IDLE");
        if (lv_context_get_state(ctx) == lv_CONTEXT_IDLE)
            PASS();
        else
            FAIL("!IDLE");
        TEST("context 转移表 IDLE→PARSING");
        if (lv_context_state_transition_valid(lv_CONTEXT_IDLE, lv_CONTEXT_PARSING))
            PASS();
        else
            FAIL("!valid");
        TEST("context 转移表 ERROR→PARSING 非法");
        if (!lv_context_state_transition_valid(lv_CONTEXT_ERROR, lv_CONTEXT_PARSING))
            PASS();
        else
            FAIL("越权转移被放行");
        TEST("context 转移表越界安全");
        if (!lv_context_state_transition_valid((lvContextState) 99, lv_CONTEXT_IDLE) &&
            !lv_context_state_transition_valid(lv_CONTEXT_IDLE, (lvContextState) 99))
            PASS();
        else
            FAIL("越界未拒绝");
        TEST("context 合法转移 PARSING");
        if (lv_context_set_state(ctx, lv_CONTEXT_PARSING) == lv_OK &&
            lv_context_get_state(ctx) == lv_CONTEXT_PARSING)
            PASS();
        else
            FAIL("转移失败");
        TEST("context 非法转移 IDLE→COMPLETE 拒绝");
        if (lv_context_set_state(ctx, lv_CONTEXT_IDLE) != lv_OK) {
            FAIL("回退失败");
        } else if (lv_context_set_state(ctx, lv_CONTEXT_COMPLETE) == lv_ERROR_INVALID_STATE &&
                   lv_context_get_state(ctx) == lv_CONTEXT_IDLE) {
            PASS();
        } else {
            FAIL("非法转移未被拒绝");
        }
        TEST("context NULL 参数");
        if (lv_context_set_state(NULL, lv_CONTEXT_IDLE) == lv_ERROR_NULL_POINTER &&
            lv_context_get_state(NULL) == lv_CONTEXT_IDLE)
            PASS();
        else
            FAIL("NULL 处理异常");
        TEST("context_state_name 越界回退");
        if (lv_context_state_name((lvContextState) 99) != NULL)
            PASS();
        else
            FAIL("回退为 NULL");
        TEST("context_destroy NULL 安全");
        lv_context_destroy(NULL);
        PASS();
        lv_context_destroy(ctx);
    g10_end:;
    }

    /* ── 组 11: 状态机合流终审一交叉一致性 ── */
    printf("[组 11] 状态机合流终审一交叉一致性\n");
    {
        /* 两侧 5×5 转移矩阵逐格一致（终审一判定：矩阵同构、协议异构，
         * 此处钉住同构面：0-4 枚举序下任意 (from,to) 的合法性两侧一致） */
        int equiv = 1;
        for (int f = 0; f < 5 && equiv; f++) {
            for (int t = 0; t < 5; t++) {
                if (engine_is_valid_transition((EngineState) f, (EngineState) t) !=
                    lv_context_state_transition_valid((lvContextState) f, (lvContextState) t)) {
                    equiv = 0;
                    break;
                }
            }
        }
        TEST("engine/context 转移矩阵逐格一致");
        if (equiv)
            PASS();
        else
            FAIL("矩阵不一致");

        /* 引擎完整合法路径 IDLE→PARSING→REASONING→COMPLETE→IDLE */
        lvEngine *e = lv_engine_create();
        if (!e) {
            FAIL("create");
            g_fail_count = 99;
            goto g11_end;
        }
        int ok = 1;
        ok = ok && lv_engine_transition_state(e, ENGINE_STATE_PARSING) == ENGINE_STATUS_OK;
        ok = ok && engine_get_state(e) == ENGINE_STATE_PARSING;
        ok = ok && lv_engine_transition_state(e, ENGINE_STATE_REASONING) == ENGINE_STATUS_OK;
        ok = ok && engine_get_state(e) == ENGINE_STATE_REASONING;
        ok = ok && lv_engine_transition_state(e, ENGINE_STATE_COMPLETE) == ENGINE_STATUS_OK;
        ok = ok && engine_get_state(e) == ENGINE_STATE_COMPLETE;
        ok = ok && lv_engine_transition_state(e, ENGINE_STATE_IDLE) == ENGINE_STATUS_OK;
        ok = ok && engine_get_state(e) == ENGINE_STATE_IDLE;
        TEST("engine 完整五态路径");
        if (ok)
            PASS();
        else
            FAIL("路径失败");
        TEST("engine ERROR→IDLE 复位");
        ok = lv_engine_transition_state(e, ENGINE_STATE_ERROR) == ENGINE_STATUS_OK &&
             engine_get_state(e) == ENGINE_STATE_ERROR;
        ok = ok && lv_engine_transition_state(e, ENGINE_STATE_IDLE) == ENGINE_STATUS_OK &&
              engine_get_state(e) == ENGINE_STATE_IDLE;
        if (ok)
            PASS();
        else
            FAIL("ERROR→IDLE 失败");
        lv_engine_destroy(e);

        /* 上下文完整合法路径 IDLE→PARSING→REASONING→COMPLETE→IDLE */
        lvContext *ctx = lv_context_create();
        if (!ctx) {
            FAIL("create");
            g_fail_count = 99;
            goto g11_end;
        }
        ok = 1;
        ok = ok && lv_context_set_state(ctx, lv_CONTEXT_PARSING) == lv_OK;
        ok = ok && lv_context_get_state(ctx) == lv_CONTEXT_PARSING;
        ok = ok && lv_context_set_state(ctx, lv_CONTEXT_REASONING) == lv_OK;
        ok = ok && lv_context_get_state(ctx) == lv_CONTEXT_REASONING;
        ok = ok && lv_context_set_state(ctx, lv_CONTEXT_COMPLETE) == lv_OK;
        ok = ok && lv_context_get_state(ctx) == lv_CONTEXT_COMPLETE;
        ok = ok && lv_context_set_state(ctx, lv_CONTEXT_IDLE) == lv_OK;
        ok = ok && lv_context_get_state(ctx) == lv_CONTEXT_IDLE;
        TEST("context 完整五态路径");
        if (ok)
            PASS();
        else
            FAIL("路径失败");
        TEST("context ERROR→IDLE 复位");
        ok = lv_context_set_state(ctx, lv_CONTEXT_ERROR) == lv_OK &&
             lv_context_get_state(ctx) == lv_CONTEXT_ERROR;
        ok = ok && lv_context_set_state(ctx, lv_CONTEXT_IDLE) == lv_OK &&
              lv_context_get_state(ctx) == lv_CONTEXT_IDLE;
        if (ok)
            PASS();
        else
            FAIL("ERROR→IDLE 失败");
        lv_context_destroy(ctx);
    g11_end:;
    }

    /* ── 组 12: lv_index_in_range 收敛行为 ── */
    printf("[组 12] lv_index_in_range 收敛行为\n");
    {
        /* ecosystem 越界索引 → NULL（-1 / count / 远界，均拒绝） */
        TEST("ecosystem module_name 越界 → NULL");
        int n = lv_ecosystem_module_count();
        if (lv_ecosystem_module_name(-1) == NULL &&
            lv_ecosystem_module_name(n) == NULL &&
            lv_ecosystem_module_name(n + 1000) == NULL)
            PASS();
        else
            FAIL("越界未拒绝");

        /* event trace：lv_event_trace_end 越界 id 安全 no-op */
        TEST("event_trace_init");
        if (lv_event_trace_init(16))
            PASS();
        else
            FAIL("!init");
        int eid = lv_event_trace_begin(EVENT_TYPE_SOLVE_START, "K2_test");
        TEST("event_trace_begin");
        if (eid >= 0)
            PASS();
        else
            FAIL("!begin");
        TEST("event_trace_end 越界/负 id 安全");
        lv_event_trace_end(-1, NULL);
        lv_event_trace_end(999999, NULL);
        lv_event_trace_end(eid, "done");
        PASS();
        lv_event_trace_shutdown();
    }

    /* ── 组 13: K2 状态机边界与游标收敛 ── */
    printf("[组 13] K2 状态机边界与游标收敛\n");
    {
        /* 状态机越界输入（负值/超界）双侧安全拒绝（lv_index_in_range 收敛后覆盖负值下界） */
        TEST("engine_is_valid_transition 越界/负值 → false");
        if (!engine_is_valid_transition((EngineState) -1, ENGINE_STATE_PARSING) &&
            !engine_is_valid_transition(ENGINE_STATE_IDLE, (EngineState) -1) &&
            !engine_is_valid_transition((EngineState) 99, ENGINE_STATE_PARSING) &&
            !engine_is_valid_transition(ENGINE_STATE_IDLE, (EngineState) 99))
            PASS();
        else
            FAIL("越界未拒绝");

        TEST("context_state_transition_valid 越界/负值 → false");
        if (!lv_context_state_transition_valid((lvContextState) -1, lv_CONTEXT_PARSING) &&
            !lv_context_state_transition_valid(lv_CONTEXT_IDLE, (lvContextState) -1) &&
            !lv_context_state_transition_valid((lvContextState) 99, lv_CONTEXT_PARSING) &&
            !lv_context_state_transition_valid(lv_CONTEXT_IDLE, (lvContextState) 99))
            PASS();
        else
            FAIL("越界未拒绝");

        /* 转移 API 越界目标返回错误且状态保持 */
        lvEngine *e = lv_engine_create();
        lvContext *ctx = lv_context_create();
        if (!e || !ctx) {
            FAIL("create");
            g_fail_count = 99;
            goto g13_end;
        }
        TEST("engine_transition 越界目标 → INVALID_STATE");
        if (lv_engine_transition_state(e, (EngineState) 99) == ENGINE_STATUS_INVALID_STATE &&
            engine_get_state(e) == ENGINE_STATE_IDLE)
            PASS();
        else
            FAIL("越界未拒绝");
        TEST("context_set_state 越界目标 → INVALID_STATE");
        if (lv_context_set_state(ctx, (lvContextState) 99) == lv_ERROR_INVALID_STATE &&
            lv_context_get_state(ctx) == lv_CONTEXT_IDLE)
            PASS();
        else
            FAIL("越界未拒绝");
        lv_engine_destroy(e);
        lv_context_destroy(ctx);

        /* scheduler_diagnose 盲区6 收敛：lvStrBuf 返回完整应有长度、小缓冲安全截断 */
        EngineScheduler *s = scheduler_create();
        if (!s) {
            FAIL("scheduler_create");
            g_fail_count = 99;
            goto g13_end;
        }
        TEST("scheduler_diagnose NULL/0 缓冲 → 错误");
        if (scheduler_diagnose(NULL, NULL, 0) < 0)
            PASS();
        else
            FAIL("未拒绝");
        char big[1024];
        int full_len = scheduler_diagnose(s, big, sizeof(big));
        TEST("scheduler_diagnose 完整输出");
        if (full_len > 20 && (size_t) full_len < sizeof(big) && strstr(big, "Default backend") != NULL)
            PASS();
        else
            FAIL("输出异常");
        TEST("scheduler_diagnose 小缓冲返回完整长度且截断安全");
        char small[8];
        int small_len = scheduler_diagnose(s, small, sizeof(small));
        if (small_len == full_len && small[7] == '\0' && strncmp(small, big, 7) == 0)
            PASS();
        else
            FAIL("截断语义异常");
        scheduler_destroy(s);
    g13_end:;
    }

    lv_cleanup();
        
TEST_MAIN_END()

/**
 * @file test_solve_debug.c
 * @brief 最小化 lv_solve() 调试测试
 *
 * 逐步建立几何图形并调用 lv_solve()，定位挂起位置。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "lv.h"
#include "engine.h"

static int P = 0, F = 0;

/* 超时信号处理 */
static volatile int timeout_flag = 0;
static void alarm_handler(int sig) {
    (void)sig;
    timeout_flag = 1;
    printf("  *** 超时！lv_solve() 似乎挂起了 ***\n");
}

#define TEST(n) printf("  [TEST] %s ... ", n); fflush(stdout)
#define PASS()  do { printf("PASS\n"); P++; fflush(stdout); } while(0)
#define FAIL(m) do { printf("FAIL: %s\n", m); F++; fflush(stdout); } while(0)

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== lv_solve() 调试测试 ===\n\n");

    lv_init();

    /* ── 测试1: 3点3线三角形（不带约束） ── */
    printf("[测试1] 3点3线三角形（无显式约束）\n");
    {
        lvEngine *e = lv_engine_create();
        if (!e) { FAIL("create"); goto t1_end; }

        int p0 = lv_add_point(e, 0, 1, 0, 1);
        int p1 = lv_add_point(e, 1, 1, 0, 1);
        int p2 = lv_add_point(e, 0, 1, 1, 1);
        if (p0 < 0 || p1 < 0 || p2 < 0) { FAIL("add_point"); goto t1_cleanup; }
        printf("    点: %d, %d, %d\n", p0, p1, p2);

        int s1 = lv_add_line_segment(e, p0, p1);
        int s2 = lv_add_line_segment(e, p1, p2);
        int s3 = lv_add_line_segment(e, p2, p0);
        if (s1 < 0 || s2 < 0 || s3 < 0) { FAIL("add_line_segment"); goto t1_cleanup; }
        printf("    线段: %d, %d, %d\n", s1, s2, s3);

        TEST("lv_solve 三角形");
        signal(SIGABRT, alarm_handler);
        timeout_flag = 0;
        EngineSolveResult r = lv_solve(e);
        if (timeout_flag) {
            FAIL("超时");
        } else {
            printf("  -> solve 返回: %d\n", (int)r);
            PASS();
        }

t1_cleanup:
        lv_engine_destroy(e);
t1_end: ;
    }

    /* ── 测试2: 3点3线三角形 + INCIDENCE 约束 ── */
    printf("[测试2] 3点3线三角形 + INCIDENCE 约束\n");
    {
        lvEngine *e = lv_engine_create();
        if (!e) { FAIL("create"); goto t2_end; }

        int p0 = lv_add_point(e, 0, 1, 0, 1);
        int p1 = lv_add_point(e, 1, 1, 0, 1);
        int p2 = lv_add_point(e, 0, 1, 1, 1);
        if (p0 < 0 || p1 < 0 || p2 < 0) { FAIL("add_point"); goto t2_cleanup; }

        int s1 = lv_add_line_segment(e, p0, p1);
        int s2 = lv_add_line_segment(e, p1, p2);
        int s3 = lv_add_line_segment(e, p2, p0);
        if (s1 < 0 || s2 < 0 || s3 < 0) { FAIL("add_line_segment"); goto t2_cleanup; }

        /* Add incidence constraints: point on segment */
        lv_add_constraint_incidence(e, p0, s1);
        lv_add_constraint_incidence(e, p1, s1);
        lv_add_constraint_incidence(e, p1, s2);
        lv_add_constraint_incidence(e, p2, s2);
        lv_add_constraint_incidence(e, p2, s3);
        lv_add_constraint_incidence(e, p0, s3);
        printf("    已添加 6 个 INCIDENCE 约束\n");

        TEST("lv_solve 三角形+incidence");
        timeout_flag = 0;
        EngineSolveResult r = lv_solve(e);
        if (timeout_flag) {
            FAIL("超时");
        } else {
            printf("  -> solve 返回: %d\n", (int)r);
            PASS();
        }

t2_cleanup:
        lv_engine_destroy(e);
t2_end: ;
    }

    /* ── 测试3: 4点4线正方形（无显式约束） ── */
    printf("[测试3] 4点4线正方形\n");
    {
        lvEngine *e = lv_engine_create();
        if (!e) { FAIL("create"); goto t3_end; }

        int p0 = lv_add_point(e, 0, 1, 0, 1);
        int p1 = lv_add_point(e, 1, 1, 0, 1);
        int p2 = lv_add_point(e, 1, 1, 1, 1);
        int p3 = lv_add_point(e, 0, 1, 1, 1);
        if (p0 < 0 || p1 < 0 || p2 < 0 || p3 < 0) { FAIL("add_point"); goto t3_cleanup; }

        lv_add_line_segment(e, p0, p1);
        lv_add_line_segment(e, p1, p2);
        lv_add_line_segment(e, p2, p3);
        lv_add_line_segment(e, p3, p0);

        TEST("lv_solve 正方形");
        timeout_flag = 0;
        EngineSolveResult r = lv_solve(e);
        if (timeout_flag) {
            FAIL("超时");
        } else {
            printf("  -> solve 返回: %d\n", (int)r);
            PASS();
        }

t3_cleanup:
        lv_engine_destroy(e);
t3_end: ;
    }

    /* ── 测试4: 4点4线正方形 + INCIDENCE ── */
    printf("[测试4] 4点4线正方形 + INCIDENCE 约束\n");
    {
        lvEngine *e = lv_engine_create();
        if (!e) { FAIL("create"); goto t4_end; }

        int p0 = lv_add_point(e, 0, 1, 0, 1);
        int p1 = lv_add_point(e, 1, 1, 0, 1);
        int p2 = lv_add_point(e, 1, 1, 1, 1);
        int p3 = lv_add_point(e, 0, 1, 1, 1);

        int s0 = lv_add_line_segment(e, p0, p1);
        int s1 = lv_add_line_segment(e, p1, p2);
        int s2 = lv_add_line_segment(e, p2, p3);
        int s3 = lv_add_line_segment(e, p3, p0);

        lv_add_constraint_incidence(e, p0, s0);
        lv_add_constraint_incidence(e, p1, s0);
        lv_add_constraint_incidence(e, p1, s1);
        lv_add_constraint_incidence(e, p2, s1);
        lv_add_constraint_incidence(e, p2, s2);
        lv_add_constraint_incidence(e, p3, s2);
        lv_add_constraint_incidence(e, p3, s3);
        lv_add_constraint_incidence(e, p0, s3);
        printf("    已添加 8 个 INCIDENCE 约束\n");

        TEST("lv_solve 正方形+incidence");
        timeout_flag = 0;
        EngineSolveResult r = lv_solve(e);
        if (timeout_flag) {
            FAIL("超时");
        } else {
            printf("  -> solve 返回: %d\n", (int)r);
            PASS();
        }

t4_cleanup:
        lv_engine_destroy(e);
t4_end: ;
    }

    /* ── 测试5: 多次求解调用 ── */
    printf("[测试5] 多次 lv_solve 调用\n");
    {
        lvEngine *e = lv_engine_create();
        if (!e) { FAIL("create"); goto t5_end; }

        int p0 = lv_add_point(e, 0, 1, 0, 1);
        int p1 = lv_add_point(e, 1, 1, 0, 1);
        int p2 = lv_add_point(e, 0, 1, 1, 1);
        lv_add_line_segment(e, p0, p1);
        lv_add_line_segment(e, p1, p2);
        lv_add_line_segment(e, p2, p0);

        bool all_ok = true;
        for (int i = 0; i < 5; i++) {
            EngineSolveResult r = lv_solve(e);
            if (r != ENGINE_SOLVE_OK) {
                printf("    第%d次返回 %d\n", i+1, (int)r);
                all_ok = false;
                break;
            }
        }
        TEST("lv_solve 5次连续调用");
        if (all_ok) PASS(); else FAIL("某次失败");

t5_cleanup:
        lv_engine_destroy(e);
t5_end: ;
    }

    /* ── 测试6: 空图求解 ── */
    printf("[测试6] 空图/边界条件\n");
    {
        TEST("lv_solve(NULL)");
        EngineSolveResult r = lv_solve(NULL);
        if (r == ENGINE_SOLVE_ERROR) PASS(); else FAIL("应返回 ERROR");

        lvEngine *e = lv_engine_create();
        if (!e) { FAIL("create"); goto t6_end; }

        TEST("lv_solve 空图");
        r = lv_solve(e);
        if (r == ENGINE_SOLVE_OK) PASS(); else { printf("  -> %d", (int)r); FAIL("应返回 OK"); }

        lv_engine_destroy(e);
t6_end: ;
    }

    /* ── 测试7: 多点无线段图 ── */
    printf("[测试7] 多点无线段\n");
    {
        lvEngine *e = lv_engine_create();
        if (!e) { FAIL("create"); goto t7_end; }

        lv_add_point(e, 0, 1, 0, 1);
        lv_add_point(e, 1, 1, 0, 1);
        lv_add_point(e, 0, 1, 1, 1);
        lv_add_point(e, 1, 1, 1, 1);

        TEST("lv_solve 4点无线段");
        timeout_flag = 0;
        EngineSolveResult r = lv_solve(e);
        if (timeout_flag) FAIL("超时");
        else { printf("  -> solve 返回: %d\n", (int)r); PASS(); }

        lv_engine_destroy(e);
t7_end: ;
    }

    printf("\n=== 结果: %d PASS, %d FAIL ===\n", P, F);
    lv_cleanup();
    return F > 0 ? 1 : 0;
}

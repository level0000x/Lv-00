/**
 * @file streaming_demo.c
 * @brief Lv-00 流式输出端到端演示
 *
 * 本示例演示 Lv-00 的完整流式输出工作流程：
 * 1. 创建引擎并注册流式回调
 * 2. 构造几何图形（三角形、圆交点）
 * 3. 所有引擎事件通过 stdout 以 JSON-RPC notification 格式实时输出
 *
 * 配合 Python 桥接脚本 (stream_bridge.py) 和 stream-monitor 前端，
 * 可实现 Web 界面的实时可视化监控。
 *
 * 构建: cmake --build build --target example_streaming
 * 运行: build\example_streaming.exe
 *        python stream_bridge.py
 */

#include "lv00.h"
#include "stream.h"
#include "engine.h"
#include "constraint_graph.h"
#include "symbolic_coord.h"
#include "error_codes.h"
#include "interop.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── 流式回调：将所有事件以 JSON-RPC 格式输出到 stdout ── */
static void stream_to_stdout_callback(const StreamEvent *event, void *user_data) {
    if (!event) return;

    char buf[STREAM_JSON_BUFFER_DEFAULT_SIZE + 256];
    int len = stream_event_to_jsonrpc(event, buf, sizeof(buf));
    if (len > 0) {
        printf("%s\n", buf);
        fflush(stdout);
    }
}

/* ── 辅助：添加有理数坐标点 ── */
static int add_rational_point(ConstraintGraph *g, int64_t xn, uint64_t xd,
                                                int64_t yn, uint64_t yd) {
    SymbolicCoord *cx = symbolic_coord_create_rational(xn, xd);
    SymbolicCoord *cy = symbolic_coord_create_rational(yn, yd);
    if (!cx || !cy) return -1;
    SymbolicCoord *coords[] = {cx, cy};
    AddNodeResult res = graph_add_point(g, coords, 2);
    return (res == ADD_NODE_OK) ? (g->next_node_id - 1) : -1;
}

/* ── 演示1：等边三角形构造 ── */
static void demo_triangle(void) {
    fprintf(stderr, "\n========== 演示1: 等边三角形构造 ==========\n");

    LV00Engine *engine = engine_create();
    if (!engine) { fprintf(stderr, "  engine_create 失败\n"); return; }

    /* 注册流式回调 */
    StreamContext *sctx = engine_get_stream_context(engine);
    int cb_id = -1;
    if (sctx) {
        cb_id = stream_register_callback_ex(sctx, stream_to_stdout_callback,
                                             NULL, STREAM_FILTER_ALL);
        fprintf(stderr, "  流式回调已注册 (id=%d)\n", cb_id);
    }

    ConstraintGraph *g = engine->main_graph;

    /* 三角形 A(0,0), B(2,0), C(1,√3) */
    int a = add_rational_point(g, 0, 1, 0, 1);
    int b = add_rational_point(g, 2, 1, 0, 1);

    SymbolicCoord *cx = symbolic_coord_create_rational(1, 1);
    Rational *qa = rational_create(0, 1);
    Rational *qb = rational_create(1, 1);
    SymbolicCoord *cy = symbolic_coord_create_quadratic(qa, qb, 3);
    SymbolicCoord *c_coords[] = {cx, cy};
    graph_add_point(g, c_coords, 2);
    rational_destroy(qa);
    rational_destroy(qb);
    int c = g->next_node_id - 1;

    fprintf(stderr, "  顶点: A=%d, B=%d, C=%d\n", a, b, c);

    /* 三条边 */
    int ab = graph_add_line_segment(g, a, b);
    int bc = graph_add_line_segment(g, b, c);
    int ca = graph_add_line_segment(g, c, a);
    fprintf(stderr, "  边: AB=%d, BC=%d, CA=%d\n", ab, bc, ca);

    /* A 在 AB 上，B 在 AB 上，等等 */
    graph_add_incidence(g, a, ab);
    graph_add_incidence(g, b, ab);
    graph_add_incidence(g, b, bc);
    graph_add_incidence(g, c, bc);
    graph_add_incidence(g, c, ca);
    graph_add_incidence(g, a, ca);

    /* 运行求解 */
    fprintf(stderr, "\n  运行引擎求解...\n");
    EngineSolveResult result = engine_solve(engine);
    fprintf(stderr, "  求解结果: %d\n", result);

    /* 注销回调 */
    if (sctx && cb_id >= 0) {
        stream_unregister_callback_by_id(sctx, cb_id);
    }
    engine_destroy(engine);

    fprintf(stderr, "========== 演示1 完成 ==========\n");
}

/* ── 演示2：圆与线交点 ── */
static void demo_circle_line(void) {
    fprintf(stderr, "\n========== 演示2: 圆与线交点测试 ==========\n");

    LV00Engine *engine = engine_create();
    if (!engine) { fprintf(stderr, "  engine_create 失败\n"); return; }

    StreamContext *sctx = engine_get_stream_context(engine);
    int cb_id = -1;
    if (sctx) {
        cb_id = stream_register_callback_ex(sctx, stream_to_stdout_callback,
                                             NULL, STREAM_FILTER_ALL);
    }

    ConstraintGraph *g = engine->main_graph;

    /* 圆心 O(0,0), 半径点 P(3,0) */
    int o = add_rational_point(g, 0, 1, 0, 1);
    int p = add_rational_point(g, 3, 1, 0, 1);
    /* 线上两点 Q(-1, 4), R(7, -2) */
    int q = add_rational_point(g, -1, 1, 4, 1);
    int r = add_rational_point(g, 7, 1, -2, 1);
    /* 未知交点 X(0,0) */
    int x = add_rational_point(g, 0, 1, 0, 1);

    fprintf(stderr, "  O=%d, P=%d(半径3), Q=%d, R=%d, X=%d(待求)\n", o, p, q, r, x);

    /* 线段 QR 和 OX */
    int qr = graph_add_line_segment(g, q, r);
    int op_seg = graph_add_line_segment(g, o, p);

    /* 关联约束 */
    graph_add_incidence(g, q, qr);
    graph_add_incidence(g, r, qr);
    graph_add_incidence(g, o, op_seg);
    graph_add_incidence(g, p, op_seg);

    /* X 在直线 QR 上 */
    graph_add_incidence(g, x, qr);

    /* 交点约束: 线段 OP 与 QR 相交于点 X */
    graph_add_intersection(g, op_seg, qr, x);

    fprintf(stderr, "\n  运行引擎求解...\n");
    EngineSolveResult result = engine_solve(engine);
    fprintf(stderr, "  求解结果: %d\n", result);

    if (sctx && cb_id >= 0) {
        stream_unregister_callback_by_id(sctx, cb_id);
    }
    engine_destroy(engine);

    fprintf(stderr, "========== 演示2 完成 ==========\n");
}

/* ── 演示3：迭代流式事件统计 ── */
static void demo_stream_stats(void) {
    fprintf(stderr, "\n========== 演示3: 流式事件统计 ==========\n");

    LV00Engine *engine = engine_create();
    if (!engine) return;

    StreamContext *sctx = engine_get_stream_context(engine);
    int cb_id = -1;
    if (sctx) {
        cb_id = stream_register_callback_ex(sctx, stream_to_stdout_callback,
                                             NULL, STREAM_FILTER_ALL);
    }

    ConstraintGraph *g = engine->main_graph;

    /* 构造多个点触发多轮事件 */
    for (int i = 0; i < 5; i++) {
        add_rational_point(g, i * 2, 1, i, 1);
    }

    /* 连接所有点 */
    for (int i = 0; i < 4; i++) {
        int sid = g->next_node_id - 5 + i;
        int tid = sid + 1;
        int seg = graph_add_line_segment(g, sid, tid);
        graph_add_incidence(g, sid, seg);
        graph_add_incidence(g, tid, seg);
    }

    fprintf(stderr, "  已构造 5 个点和 4 条线段\n");

    /* 运行引擎 */
    engine_solve(engine);

    /* 输出统计 */
    if (sctx) {
        fprintf(stderr, "\n  事件统计:\n");
        fprintf(stderr, "    总事件数: %ld\n", stream_get_total_event_count(sctx));
        fprintf(stderr, "    丢弃数:   %ld\n", stream_get_dropped_count(sctx));

        const StreamEventType stats_types[] = {
            STREAM_EVENT_ENGINE_START, STREAM_EVENT_ENGINE_DONE,
            STREAM_EVENT_NODE_ADDED, STREAM_EVENT_CONSTRAINT_ADDED,
            STREAM_EVENT_NORMALIZE_START, STREAM_EVENT_NORMALIZE_DONE,
            STREAM_EVENT_NORMALIZE_MERGE, STREAM_EVENT_SOLVE_START,
            STREAM_EVENT_SOLVE_DONE, STREAM_EVENT_CONFLICT_DETECTED
        };
        for (int i = 0; i < 10; i++) {
            long count = stream_get_event_count(sctx, stats_types[i]);
            if (count > 0) {
                fprintf(stderr, "    %s: %ld\n",
                        stream_event_type_name(stats_types[i]), count);
            }
        }
    }

    if (sctx && cb_id >= 0) {
        stream_unregister_callback_by_id(sctx, cb_id);
    }
    engine_destroy(engine);

    fprintf(stderr, "========== 演示3 完成 ==========\n");
}

/* ── 主程序 ── */
int main(int argc, char *argv[]) {
    fprintf(stderr, "╔════════════════════════════════════════════╗\n");
    fprintf(stderr, "║  Lv-00 流式输出端到端演示 v3.0.1          ║\n");
    fprintf(stderr, "║  JSON-RPC 事件将输出到 stdout             ║\n");
    fprintf(stderr, "╚════════════════════════════════════════════╝\n");

    int demo = 0;
    if (argc > 1) {
        demo = atoi(argv[1]);
    }

    if (demo == 0 || demo == 1) demo_triangle();
    if (demo == 0 || demo == 2) demo_circle_line();
    if (demo == 0 || demo == 3) demo_stream_stats();

    fprintf(stderr, "\n所有演示完成。\n");
    return 0;
}

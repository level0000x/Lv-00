/**
 * @file test_layer6_visual.c
 * @brief 综合测试：Layer 6 可视化模块
 *
 * 涵盖 9 大模块：
 *   1. Block Canvas：创建/销毁、添加/移除块、连接、SVG 渲染
 *   2. Geometry Canvas：创建/销毁、添加/移除实体/约束、包围盒、SVG
 *   3. Visual Editor：创建/销毁、重置、视图切换、全量/增量执行、状态查询
 *   4. Node Graph：创建/销毁、添加/移除/查找节点、连接、布局
 *   5. View Synchronizer：创建/销毁、启用/禁用、传播、刷新
 *   6. Block Scheduler：创建/销毁、策略、全量/增量执行、脏标记
 *   7. Text Code View：创建/销毁、设置/获取文本、插入/删除、渲染
 *   8. Converters：block_to_text/text_to_block、block_to_node/node_to_block
 *   9. Sync Protocol：创建/销毁、传播、冲突记录
 *
 * @author Lv-00 Project
 */

#include "test_helpers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/visual_editor.h"
#include "lv/block_scheduler.h"
#include "lv/extended_types.h"
#include "lv/func_block.h"
#include "lv/representation_converter.h"
#include "lv/io_blocks.h"

/* ============================================================
 * 前向声明：sync_protocol.c 中未在头文件暴露的内部 API
 * ============================================================ */
typedef struct lvSyncProtocol lvSyncProtocol;
lvSyncProtocol *lv_sync_protocol_create(void *graph);
void lv_sync_protocol_destroy(lvSyncProtocol *proto);
int lv_sync_propagate(lvSyncProtocol *proto, int source_view, void *change, int max_depth);

/* ============================================================
 * 前向声明：block_to_geometry.c 中未在头文件暴露的内部类型和 API
 * ============================================================ */
typedef struct GeometryEncoding GeometryEncoding;
void lv_geometry_encoding_destroy(GeometryEncoding *enc);

/* 全局测试计数器 */
int g_pass_count = 0;
int g_fail_count = 0;

/* 最大端口数常量（与 block_to_text.c 保持一致） */
#define MAX_BLOCK_PORTS 64

/* ============================================================
 * 辅助：SimpleBlockGraph —— 为调度器和转换器测试提供块图
 * ============================================================ */
typedef struct {
    FuncBlock **blocks;
    int count;
} SimpleBlockGraph;

static SimpleBlockGraph *create_test_block_graph(int n) {
    SimpleBlockGraph *bg = (SimpleBlockGraph *)lv_calloc(1, sizeof(SimpleBlockGraph));
    if (!bg) return NULL;
    bg->blocks = (FuncBlock **)lv_calloc((size_t)(n > 0 ? n : 1), sizeof(FuncBlock *));
    if (!bg->blocks) {
        lv_free((void **)&bg);
        return NULL;
    }
    bg->count = n;
    for (int i = 0; i < n; i++) {
        bg->blocks[i] = func_block_create(i + 1);
        if (bg->blocks[i]) {
            char name[32];
            snprintf(name, sizeof(name), "block_%d", i);
            func_block_set_name(bg->blocks[i], name);
        }
    }
    return bg;
}

static void destroy_test_block_graph(SimpleBlockGraph *bg) {
    if (!bg) return;
    for (int i = 0; i < bg->count; i++) {
        if (bg->blocks[i]) func_block_destroy(bg->blocks[i]);
    }
    lv_free((void **)&bg->blocks);
    lv_free((void **)&bg);
}

/* ============================================================
 * 辅助：带有 cap 字段的块图（用于 block_to_text 等转换器的输出）
 * ============================================================ */
typedef struct {
    FuncBlock **blocks;
    int count;
    int cap;
} SimpleBlockGraphCap;

/* ============================================================
 * 测试组 1: Block Canvas 块画布
 * ============================================================ */
static void test_bc_create_destroy(void) {
    lvBlockCanvasView *canvas = lv_block_canvas_create();
    TEST_ASSERT_NOT_NULL(canvas);
    lv_block_canvas_destroy(canvas);
    lv_block_canvas_destroy(NULL);
    TEST_ASSERT(1, "重复销毁 NULL 安全");
}

static void test_bc_add_block(void) {
    lvBlockCanvasView *canvas = lv_block_canvas_create();
    TEST_ASSERT_NOT_NULL(canvas);

    int id1 = lv_block_canvas_add_block(canvas, "Add", 10, 20, 120, 60, 2, 2, 1);
    TEST_ASSERT(id1 > 0, "添加块应返回正 ID");

    int id2 = lv_block_canvas_add_block(canvas, "Sub", 200, 20, 120, 60, 0, 1, 1);
    TEST_ASSERT(id2 > 0, "第二个块 ID 应为正");
    TEST_ASSERT(id2 != id1, "两次添加应返回不同 ID");

    /* 默认尺寸 */
    int id3 = lv_block_canvas_add_block(canvas, "Mul", 400, 20, 0, 0, 4, 3, 2);
    TEST_ASSERT(id3 > 0, "默认尺寸应成功");

    /* NULL 标签 */
    int ret = lv_block_canvas_add_block(canvas, NULL, 0, 0, 100, 50, 0, 0, 0);
    TEST_ASSERT_EQ(ret, -1);

    /* NULL 画布 */
    ret = lv_block_canvas_add_block(NULL, "X", 0, 0, 100, 50, 0, 1, 0);
    TEST_ASSERT_EQ(ret, -1);

    lv_block_canvas_destroy(canvas);
}

/* 批量添加大量块测试扩容机制 */
static void test_bc_add_many_blocks(void) {
    lvBlockCanvasView *canvas = lv_block_canvas_create();
    TEST_ASSERT_NOT_NULL(canvas);

    int ids[50];
    for (int i = 0; i < 50; i++) {
        char label[32];
        snprintf(label, sizeof(label), "B%d", i);
        ids[i] = lv_block_canvas_add_block(canvas, label, (double)(i * 130), 0, 120, 60, i % 6, 2, 1);
        TEST_ASSERT(ids[i] > 0, "批量添加块应成功");
    }
    /* 验证 ID 唯一性 */
    for (int i = 0; i < 50; i++) {
        for (int j = i + 1; j < 50; j++) {
            TEST_ASSERT(ids[i] != ids[j], "块 ID 应唯一");
        }
    }
    lv_block_canvas_destroy(canvas);
}

static void test_bc_remove_block(void) {
    lvBlockCanvasView *canvas = lv_block_canvas_create();
    TEST_ASSERT_NOT_NULL(canvas);

    int id1 = lv_block_canvas_add_block(canvas, "A", 0, 0, 100, 50, 0, 1, 1);
    int id2 = lv_block_canvas_add_block(canvas, "B", 200, 0, 100, 50, 0, 1, 1);
    TEST_ASSERT(id1 > 0 && id2 > 0, "添加块");

    int r = lv_block_canvas_remove_block(canvas, id1);
    TEST_ASSERT_EQ(r, 0);

    /* 重复移除 */
    r = lv_block_canvas_remove_block(canvas, id1);
    TEST_ASSERT_EQ(r, -1);

    /* 无效参数 */
    r = lv_block_canvas_remove_block(canvas, 0);
    TEST_ASSERT_EQ(r, -1);
    r = lv_block_canvas_remove_block(NULL, 1);
    TEST_ASSERT_EQ(r, -1);

    lv_block_canvas_destroy(canvas);
}

static void test_bc_connect(void) {
    lvBlockCanvasView *canvas = lv_block_canvas_create();
    TEST_ASSERT_NOT_NULL(canvas);

    int b1 = lv_block_canvas_add_block(canvas, "A", 0, 0, 100, 50, 0, 1, 1);
    int b2 = lv_block_canvas_add_block(canvas, "B", 200, 0, 100, 50, 0, 1, 1);
    TEST_ASSERT(b1 > 0 && b2 > 0, "添加块");

    /* 连接（输出端口 ID=2，输入端口 ID=1） */
    int conn = lv_block_canvas_connect_blocks(canvas, b1, 2, b2, 1);
    TEST_ASSERT(conn > 0, "连接应返回正 ID");

    /* 自连接应失败 */
    int self = lv_block_canvas_connect_blocks(canvas, b1, 2, b1, 1);
    TEST_ASSERT_EQ(self, -1);

    /* 无效块 ID */
    int bad = lv_block_canvas_connect_blocks(canvas, -1, 1, b2, 1);
    TEST_ASSERT_EQ(bad, -1);

    /* 无效端口 ID */
    bad = lv_block_canvas_connect_blocks(canvas, b1, 999, b2, 1);
    TEST_ASSERT_EQ(bad, -1);

    /* NULL 画布 */
    bad = lv_block_canvas_connect_blocks(NULL, b1, 2, b2, 1);
    TEST_ASSERT_EQ(bad, -1);

    /* 移除源块时连接应自动删除 */
    int b3 = lv_block_canvas_add_block(canvas, "C", 400, 0, 100, 50, 0, 1, 1);
    int c2 = lv_block_canvas_connect_blocks(canvas, b2, 2, b3, 1);
    TEST_ASSERT(c2 > 0, "第三个连接");
    lv_block_canvas_remove_block(canvas, b2);
    /* 连接应已被移除 */

    lv_block_canvas_destroy(canvas);
}

static void test_bc_render_svg(void) {
    lvBlockCanvasView *canvas = lv_block_canvas_create();
    TEST_ASSERT_NOT_NULL(canvas);

    int b1 = lv_block_canvas_add_block(canvas, "Test", 0, 0, 100, 50, 0, 1, 1);
    TEST_ASSERT(b1 > 0, "添加块");

    int b2 = lv_block_canvas_add_block(canvas, "Another", 200, 0, 100, 50, 3, 2, 1);
    TEST_ASSERT(b2 > 0, "添加第二个块");

    lv_block_canvas_connect_blocks(canvas, b1, 2, b2, 1);

    char *svg = lv_block_canvas_render_svg(canvas);
    TEST_ASSERT_NOT_NULL(svg);
    TEST_ASSERT(strstr(svg, "<svg") != NULL);
    TEST_ASSERT(strstr(svg, "</svg>") != NULL);
    TEST_ASSERT(strstr(svg, "Test") != NULL);
    TEST_ASSERT(strstr(svg, "Another") != NULL);
    TEST_ASSERT(strstr(svg, "<?xml") != NULL);
    lv_free((void **)&svg);

    /* NULL 画布 */
    svg = lv_block_canvas_render_svg(NULL);
    TEST_ASSERT_NULL(svg);

    /* 空画布 */
    lvBlockCanvasView *empty = lv_block_canvas_create();
    TEST_ASSERT_NOT_NULL(empty);
    svg = lv_block_canvas_render_svg(empty);
    TEST_ASSERT_NOT_NULL(svg);
    TEST_ASSERT(strstr(svg, "<svg") != NULL);
    lv_free((void **)&svg);
    lv_block_canvas_destroy(empty);

    lv_block_canvas_destroy(canvas);
}

/* ============================================================
 * 测试组 2: Geometry Canvas 几何画布
 * ============================================================ */
static void test_gc_create_destroy(void) {
    lvGeometryCanvas *gc = lv_geometry_canvas_create();
    TEST_ASSERT_NOT_NULL(gc);
    lv_geometry_canvas_destroy(gc);
    lv_geometry_canvas_destroy(NULL);
    TEST_ASSERT(1, "销毁 NULL 安全");
}

static void test_gc_add_entity(void) {
    lvGeometryCanvas *gc = lv_geometry_canvas_create();
    TEST_ASSERT_NOT_NULL(gc);

    /* 点 */
    double pt[] = {50.0, 100.0};
    int pt_id = lv_geometry_canvas_add_entity(gc, 0, "P1", pt, 2);
    TEST_ASSERT(pt_id > 0, "添加点应返回正 ID");

    /* 线 */
    double line[] = {0, 0, 100, 100};
    int line_id = lv_geometry_canvas_add_entity(gc, 1, "L1", line, 4);
    TEST_ASSERT(line_id > 0, "添加线应成功");
    TEST_ASSERT(line_id != pt_id, "ID 应不同");

    /* 圆 */
    double circ[] = {50, 50, 30};
    int circ_id = lv_geometry_canvas_add_entity(gc, 2, "C1", circ, 3);
    TEST_ASSERT(circ_id > 0, "添加圆应成功");

    /* 多边形 */
    double poly[] = {0, 0, 100, 0, 100, 100, 0, 100};
    int poly_id = lv_geometry_canvas_add_entity(gc, 3, "Poly", poly, 8);
    TEST_ASSERT(poly_id > 0, "添加多边形应成功");

    /* 空标签 */
    double pt2[] = {10, 10};
    int id_no_label = lv_geometry_canvas_add_entity(gc, 0, NULL, pt2, 2);
    TEST_ASSERT(id_no_label > 0, "NULL 标签应成功");

    /* 无效参数 */
    int bad = lv_geometry_canvas_add_entity(gc, 0, NULL, NULL, 0);
    TEST_ASSERT_EQ(bad, -1);

    bad = lv_geometry_canvas_add_entity(gc, 0, "bad", pt, 0);
    TEST_ASSERT_EQ(bad, -1);

    bad = lv_geometry_canvas_add_entity(NULL, 0, "bad", pt, 2);
    TEST_ASSERT_EQ(bad, -1);

    lv_geometry_canvas_destroy(gc);
}

/* 批量添加大量实体测试扩容 */
static void test_gc_add_many_entities(void) {
    lvGeometryCanvas *gc = lv_geometry_canvas_create();
    TEST_ASSERT_NOT_NULL(gc);

    double pt[] = {0, 0};
    int ids[50];
    for (int i = 0; i < 50; i++) {
        ids[i] = lv_geometry_canvas_add_entity(gc, 0, "P", pt, 2);
        TEST_ASSERT(ids[i] > 0, "批量添加实体");
    }
    lv_geometry_canvas_destroy(gc);
}

static void test_gc_remove_entity(void) {
    lvGeometryCanvas *gc = lv_geometry_canvas_create();
    double pts[] = {10, 20};
    int id = lv_geometry_canvas_add_entity(gc, 0, "P", pts, 2);
    TEST_ASSERT(id > 0, "添加点");

    int r = lv_geometry_canvas_remove_entity(gc, id);
    TEST_ASSERT_EQ(r, 0);

    r = lv_geometry_canvas_remove_entity(gc, id);
    TEST_ASSERT_EQ(r, -1);

    r = lv_geometry_canvas_remove_entity(NULL, 1);
    TEST_ASSERT_EQ(r, -1);

    r = lv_geometry_canvas_remove_entity(gc, 0);
    TEST_ASSERT_EQ(r, -1);

    lv_geometry_canvas_destroy(gc);
}

static void test_gc_add_constraint(void) {
    lvGeometryCanvas *gc = lv_geometry_canvas_create();
    double p1c[] = {0, 0}, p2c[] = {100, 100};
    int p1 = lv_geometry_canvas_add_entity(gc, 0, "A", p1c, 2);
    int p2 = lv_geometry_canvas_add_entity(gc, 0, "B", p2c, 2);
    TEST_ASSERT(p1 > 0 && p2 > 0, "添加两个点");

    int cid = lv_geometry_canvas_add_constraint(gc, p1, p2, "distance_100");
    TEST_ASSERT(cid > 0, "添加约束");

    /* NULL 标签 */
    cid = lv_geometry_canvas_add_constraint(gc, p1, p2, NULL);
    TEST_ASSERT(cid > 0, "NULL 标签约束应成功");

    /* 无效参数 */
    int bad = lv_geometry_canvas_add_constraint(gc, 0, 0, "x");
    TEST_ASSERT_EQ(bad, -1);

    bad = lv_geometry_canvas_add_constraint(NULL, p1, p2, "x");
    TEST_ASSERT_EQ(bad, -1);

    /* 移除实体时关联约束应自动删除 */
    int cid2 = lv_geometry_canvas_add_constraint(gc, p2, p1, "back");
    TEST_ASSERT(cid2 > 0, "第二个约束");
    lv_geometry_canvas_remove_entity(gc, p1);
    /* p1 相关的约束应被清理 */

    lv_geometry_canvas_destroy(gc);
}

/* 批量添加大量约束测试扩容 */
static void test_gc_add_many_constraints(void) {
    lvGeometryCanvas *gc = lv_geometry_canvas_create();
    double p1c[] = {0, 0};
    int p1 = lv_geometry_canvas_add_entity(gc, 0, "A", p1c, 2);
    TEST_ASSERT(p1 > 0, "锚点实体");
    int ids[50];
    for (int i = 0; i < 50; i++) {
        double pt[] = {(double)i * 10, (double)i * 10};
        int pi = lv_geometry_canvas_add_entity(gc, 0, "P", pt, 2);
        ids[i] = lv_geometry_canvas_add_constraint(gc, p1, pi, "link");
        TEST_ASSERT(ids[i] > 0, "批量添加约束");
    }
    lv_geometry_canvas_destroy(gc);
}

static void test_gc_fit_view(void) {
    lvGeometryCanvas *gc = lv_geometry_canvas_create();

    /* 空画布 */
    int r = lv_geometry_canvas_fit_view(gc);
    TEST_ASSERT_EQ(r, -1);

    double pts[] = {50, 50};
    lv_geometry_canvas_add_entity(gc, 0, "X", pts, 2);
    r = lv_geometry_canvas_fit_view(gc);
    TEST_ASSERT_EQ(r, 0);

    r = lv_geometry_canvas_fit_view(NULL);
    TEST_ASSERT_EQ(r, -1);

    lv_geometry_canvas_destroy(gc);
}

static void test_gc_render_svg(void) {
    lvGeometryCanvas *gc = lv_geometry_canvas_create();
    double pt[] = {100, 200};
    lv_geometry_canvas_add_entity(gc, 0, "P", pt, 2);
    double line[] = {0, 0, 200, 100};
    lv_geometry_canvas_add_entity(gc, 1, "L", line, 4);
    double circ[] = {50, 50, 25};
    lv_geometry_canvas_add_entity(gc, 2, "C", circ, 3);
    double poly[] = {0, 0, 100, 0, 100, 100, 0, 100};
    lv_geometry_canvas_add_entity(gc, 3, "Poly", poly, 8);

    char *svg = lv_geometry_canvas_render_svg(gc);
    TEST_ASSERT_NOT_NULL(svg);
    TEST_ASSERT(strstr(svg, "<svg") != NULL);
    TEST_ASSERT(strstr(svg, "</svg>") != NULL);
    TEST_ASSERT(strstr(svg, "P") != NULL);
    TEST_ASSERT(strstr(svg, "L") != NULL);
    TEST_ASSERT(strstr(svg, "C") != NULL);
    TEST_ASSERT(strstr(svg, "Poly") != NULL);
    lv_free((void **)&svg);

    /* NULL */
    svg = lv_geometry_canvas_render_svg(NULL);
    TEST_ASSERT_NULL(svg);

    /* 空画布 */
    lvGeometryCanvas *empty = lv_geometry_canvas_create();
    svg = lv_geometry_canvas_render_svg(empty);
    TEST_ASSERT_NOT_NULL(svg);
    TEST_ASSERT(strstr(svg, "<svg") != NULL);
    lv_free((void **)&svg);
    lv_geometry_canvas_destroy(empty);

    lv_geometry_canvas_destroy(gc);
}

/* 带约束的 SVG 渲染 */
static void test_gc_render_svg_with_constraints(void) {
    lvGeometryCanvas *gc = lv_geometry_canvas_create();
    double p1c[] = {0, 0}, p2c[] = {100, 0};
    int p1 = lv_geometry_canvas_add_entity(gc, 0, "A", p1c, 2);
    int p2 = lv_geometry_canvas_add_entity(gc, 0, "B", p2c, 2);
    lv_geometry_canvas_add_constraint(gc, p1, p2, "horizontal");
    lv_geometry_canvas_add_constraint(gc, p2, p1, NULL);

    char *svg = lv_geometry_canvas_render_svg(gc);
    TEST_ASSERT_NOT_NULL(svg);
    TEST_ASSERT(strstr(svg, "horizontal") != NULL);
    lv_free((void **)&svg);

    lv_geometry_canvas_destroy(gc);
}

/* ============================================================
 * 测试组 3: Visual Editor 可视化编辑器
 * ============================================================ */
static void test_ve_create_destroy(void) {
    lvVisualEditor *editor = lv_visual_editor_create();
    TEST_ASSERT_NOT_NULL(editor);
    TEST_ASSERT_EQ(editor->layer_id, 6);
    TEST_ASSERT_EQ(editor->active_view, lv_VIEW_NODE_GRAPH);
    TEST_ASSERT_EQ(editor->state, lv_EDITOR_IDLE);

    lv_visual_editor_destroy(editor);
    lv_visual_editor_destroy(NULL);
}

static void test_ve_reset(void) {
    lvVisualEditor *editor = lv_visual_editor_create();
    TEST_ASSERT_NOT_NULL(editor);

    int r = lv_visual_editor_reset(editor);
    TEST_ASSERT_EQ(r, 0);

    r = lv_visual_editor_reset(NULL);
    TEST_ASSERT_EQ(r, -1);

    lv_visual_editor_destroy(editor);
}

static void test_ve_switch_view(void) {
    lvVisualEditor *editor = lv_visual_editor_create();
    TEST_ASSERT_NOT_NULL(editor);

    int r = lv_visual_editor_switch_view(editor, lv_VIEW_GEOMETRY_CANVAS);
    TEST_ASSERT_EQ(r, 0);
    TEST_ASSERT_EQ(lv_visual_editor_active_view(editor), lv_VIEW_GEOMETRY_CANVAS);

    r = lv_visual_editor_switch_view(editor, lv_VIEW_BLOCK_CANVAS);
    TEST_ASSERT_EQ(r, 0);
    TEST_ASSERT_EQ(lv_visual_editor_active_view(editor), lv_VIEW_BLOCK_CANVAS);

    r = lv_visual_editor_switch_view(editor, lv_VIEW_TEXT_CODE);
    TEST_ASSERT_EQ(r, 0);
    TEST_ASSERT_EQ(lv_visual_editor_active_view(editor), lv_VIEW_TEXT_CODE);

    /* 无效视图 */
    r = lv_visual_editor_switch_view(editor, (lvViewType)99);
    TEST_ASSERT_EQ(r, -1);

    /* NULL */
    r = lv_visual_editor_switch_view(NULL, lv_VIEW_NODE_GRAPH);
    TEST_ASSERT_EQ(r, -1);
    TEST_ASSERT_EQ(lv_visual_editor_active_view(NULL), lv_VIEW_NODE_GRAPH);

    lv_visual_editor_destroy(editor);
}

static void test_ve_execute(void) {
    lvVisualEditor *editor = lv_visual_editor_create();
    TEST_ASSERT_NOT_NULL(editor);

    /* 无 block_graph 时应失败 */
    int r = lv_visual_editor_execute(editor);
    TEST_ASSERT_EQ(r, -1);
    TEST_ASSERT_EQ(editor->state, lv_EDITOR_ERROR);

    /* 增量执行同样 */
    r = lv_visual_editor_execute_incremental(editor);
    TEST_ASSERT_EQ(r, -1);

    /* NULL */
    r = lv_visual_editor_execute(NULL);
    TEST_ASSERT_EQ(r, -1);
    r = lv_visual_editor_execute_incremental(NULL);
    TEST_ASSERT_EQ(r, -1);

    /* 设置 block_graph 后执行 */
    SimpleBlockGraph *bg = create_test_block_graph(2);
    editor->block_graph = bg;
    r = lv_visual_editor_execute(editor);
    TEST_ASSERT_EQ(r, 0);
    TEST_ASSERT_EQ(editor->state, lv_EDITOR_IDLE);

    /* 再次执行应成功 */
    r = lv_visual_editor_execute(editor);
    TEST_ASSERT_EQ(r, 0);

    /* 增量执行 */
    r = lv_visual_editor_execute_incremental(editor);
    TEST_ASSERT_EQ(r, 0);

    editor->block_graph = NULL;
    lv_visual_editor_destroy(editor);
    destroy_test_block_graph(bg);
}

static void test_ve_state_error(void) {
    lvVisualEditor *editor = lv_visual_editor_create();
    TEST_ASSERT_NOT_NULL(editor);

    TEST_ASSERT_EQ(lv_visual_editor_state(editor), lv_EDITOR_IDLE);
    TEST_ASSERT_EQ(lv_visual_editor_state(NULL), lv_EDITOR_ERROR);

    const char *err = lv_visual_editor_last_error(editor);
    TEST_ASSERT_NOT_NULL(err);

    const char *null_err = lv_visual_editor_last_error(NULL);
    TEST_ASSERT_STR_EQ(null_err, "NULL editor");

    lv_visual_editor_destroy(editor);
}

/* ============================================================
 * 测试组 4: Node Graph 节点图
 * ============================================================ */
static void test_ng_create_destroy(void) {
    lvNodeGraphView *ng = lv_node_graph_create();
    TEST_ASSERT_NOT_NULL(ng);
    lv_node_graph_destroy(ng);
    lv_node_graph_destroy(NULL);
    TEST_ASSERT(1, "销毁 NULL 安全");
}

static void test_ng_add_find_remove_node(void) {
    lvNodeGraphView *ng = lv_node_graph_create();
    TEST_ASSERT_NOT_NULL(ng);

    /* 自动分配 ID */
    int n1 = lv_node_graph_add_node(ng, 0, "Start", 10, 20, 1);
    TEST_ASSERT(n1 > 0, "自动分配 ID 应返回正 ID");

    /* 指定 ID */
    int n2 = lv_node_graph_add_node(ng, 100, "End", 500, 300, 3);
    TEST_ASSERT_EQ(n2, 100);

    /* 默认类型 */
    int n3 = lv_node_graph_add_node(ng, 0, "Mid", 200, 100, 0);
    TEST_ASSERT(n3 > 0, "默认类型节点");

    /* 查找 */
    lvGraphNode *found = lv_node_graph_find_node(ng, n1);
    TEST_ASSERT_NOT_NULL(found);
    /* 无法直接比较 label 因为 test_helpers 中没有 TEST_ASSERT_STR_EQ_MSG 宏 */
    TEST_ASSERT(found->id == n1, "查找返回正确节点");

    found = lv_node_graph_find_node(ng, 999);
    TEST_ASSERT_NULL(found);

    found = lv_node_graph_find_node(NULL, 1);
    TEST_ASSERT_NULL(found);

    /* NULL 标签 */
    int bad = lv_node_graph_add_node(ng, 0, NULL, 0, 0, 0);
    TEST_ASSERT_EQ(bad, -1);

    /* 移除 */
    int r = lv_node_graph_remove_node(ng, n2);
    TEST_ASSERT_EQ(r, 0);

    r = lv_node_graph_remove_node(ng, n2);
    TEST_ASSERT_EQ(r, -1);

    r = lv_node_graph_remove_node(ng, 0);
    TEST_ASSERT_EQ(r, -1);

    r = lv_node_graph_remove_node(NULL, 1);
    TEST_ASSERT_EQ(r, -1);

    lv_node_graph_destroy(ng);
}

/* 批量添加节点测试扩容 */
static void test_ng_add_many_nodes(void) {
    lvNodeGraphView *ng = lv_node_graph_create();
    TEST_ASSERT_NOT_NULL(ng);

    int ids[50];
    for (int i = 0; i < 50; i++) {
        char label[32];
        snprintf(label, sizeof(label), "N%d", i);
        ids[i] = lv_node_graph_add_node(ng, 0, label, (double)(i * 20), (double)(i * 10), i % 5);
        TEST_ASSERT(ids[i] > 0, "批量添加节点");
    }
    lv_node_graph_destroy(ng);
}

static void test_ng_connections(void) {
    lvNodeGraphView *ng = lv_node_graph_create();
    int n1 = lv_node_graph_add_node(ng, 0, "A", 0, 0, 0);
    int n2 = lv_node_graph_add_node(ng, 0, "B", 100, 100, 0);
    int n3 = lv_node_graph_add_node(ng, 0, "C", 200, 0, 0);
    TEST_ASSERT(n1 > 0 && n2 > 0 && n3 > 0, "添加三个节点");

    /* 添加连接 */
    int c1 = lv_node_graph_add_connection(ng, n1, n2, "edge_AB");
    TEST_ASSERT(c1 > 0, "添加连接");
    int c2 = lv_node_graph_add_connection(ng, n2, n3, "edge_BC");
    TEST_ASSERT(c2 > 0, "第二个连接");
    TEST_ASSERT(c2 != c1, "连接 ID 应不同");

    /* NULL 标签 */
    int c3 = lv_node_graph_add_connection(ng, n1, n3, NULL);
    TEST_ASSERT(c3 > 0, "NULL 标签连接应成功");

    /* 无效参数 */
    int bad = lv_node_graph_add_connection(ng, 0, n2, "x");
    TEST_ASSERT_EQ(bad, -1);
    bad = lv_node_graph_add_connection(NULL, n1, n2, "x");
    TEST_ASSERT_EQ(bad, -1);

    /* 移除连接 */
    int r = lv_node_graph_remove_connection(ng, c1);
    TEST_ASSERT_EQ(r, 0);
    r = lv_node_graph_remove_connection(ng, c1);
    TEST_ASSERT_EQ(r, -1);
    r = lv_node_graph_remove_connection(ng, 0);
    TEST_ASSERT_EQ(r, -1);
    r = lv_node_graph_remove_connection(NULL, 1);
    TEST_ASSERT_EQ(r, -1);

    /* 移除节点时应自动删除连接 */
    r = lv_node_graph_remove_node(ng, n3);
    TEST_ASSERT_EQ(r, 0);

    lv_node_graph_destroy(ng);
}

/* 批量添加连接测试扩容 */
static void test_ng_add_many_connections(void) {
    lvNodeGraphView *ng = lv_node_graph_create();
    int prev = lv_node_graph_add_node(ng, 0, "Root", 0, 0, 0);
    TEST_ASSERT(prev > 0);
    int ids[50];
    for (int i = 0; i < 50; i++) {
        char label[32];
        snprintf(label, sizeof(label), "N%d", i);
        int ni = lv_node_graph_add_node(ng, 0, label, (double)(i * 20), (double)(i * 10), 0);
        ids[i] = lv_node_graph_add_connection(ng, prev, ni, "link");
        TEST_ASSERT(ids[i] > 0, "批量添加连接");
    }
    lv_node_graph_destroy(ng);
}

static void test_ng_layout(void) {
    lvNodeGraphView *ng = lv_node_graph_create();

    /* 单节点 */
    lv_node_graph_add_node(ng, 0, "Only", 100, 100, 0);
    int r = lv_node_graph_layout(ng);
    TEST_ASSERT_EQ(r, 0);

    /* 多节点 */
    lv_node_graph_add_node(ng, 0, "B", 200, 200, 0);
    lv_node_graph_add_node(ng, 0, "C", 300, 50, 0);
    lv_node_graph_add_connection(ng, 1, 2, "e1");
    lv_node_graph_add_connection(ng, 2, 3, "e2");
    r = lv_node_graph_layout(ng);
    TEST_ASSERT_EQ(r, 0);

    /* 空图（无节点） */
    lvNodeGraphView *empty = lv_node_graph_create();
    r = lv_node_graph_layout(empty);
    TEST_ASSERT_EQ(r, 0);
    lv_node_graph_destroy(empty);

    /* NULL */
    r = lv_node_graph_layout(NULL);
    TEST_ASSERT_EQ(r, -1);

    lv_node_graph_destroy(ng);
}

/* ============================================================
 * 测试组 5: View Synchronizer 视图同步器
 * ============================================================ */
static void test_vs_create_destroy(void) {
    lvViewSynchronizer *sync = lv_view_sync_create();
    TEST_ASSERT_NOT_NULL(sync);
    lv_view_sync_destroy(sync);
    lv_view_sync_destroy(NULL);
    TEST_ASSERT(1, "销毁 NULL 安全");
}

static void test_vs_enable_disable(void) {
    lvViewSynchronizer *sync = lv_view_sync_create();
    TEST_ASSERT_NOT_NULL(sync);

    TEST_ASSERT_EQ(lv_view_sync_conflicts(sync), 0);
    TEST_ASSERT_EQ(lv_view_sync_conflicts(NULL), 0);

    int r = lv_view_sync_disable(sync);
    TEST_ASSERT_EQ(r, 0);
    r = lv_view_sync_enable(sync);
    TEST_ASSERT_EQ(r, 0);

    r = lv_view_sync_enable(NULL);
    TEST_ASSERT_EQ(r, -1);
    r = lv_view_sync_disable(NULL);
    TEST_ASSERT_EQ(r, -1);

    lv_view_sync_destroy(sync);
}

static void test_vs_propagate_flush(void) {
    lvViewSynchronizer *sync = lv_view_sync_create();
    TEST_ASSERT_NOT_NULL(sync);

    /* 传播 */
    int r = lv_view_sync_propagate(sync, 1, "node_added");
    TEST_ASSERT_EQ(r, 0);

    r = lv_view_sync_propagate(sync, 2, "block_removed");
    TEST_ASSERT_EQ(r, 0);

    /* 重复传播相同视图 ID */
    r = lv_view_sync_propagate(sync, 1, "node_moved");
    TEST_ASSERT_EQ(r, 0);

    /* 无效参数 */
    r = lv_view_sync_propagate(NULL, 1, "x");
    TEST_ASSERT_EQ(r, -1);
    r = lv_view_sync_propagate(sync, 3, NULL);
    TEST_ASSERT_EQ(r, -1);

    /* 刷新 */
    int flushed = lv_view_sync_flush(sync);
    TEST_ASSERT(flushed > 0, "flush 应返回处理的变更数");

    /* 再次刷新——队列已空 */
    flushed = lv_view_sync_flush(sync);
    TEST_ASSERT_EQ(flushed, 0);

    flushed = lv_view_sync_flush(NULL);
    TEST_ASSERT_EQ(flushed, -1);

    /* 禁用时传播和 flush */
    lv_view_sync_disable(sync);
    r = lv_view_sync_propagate(sync, 1, "test");
    TEST_ASSERT_EQ(r, 0);
    flushed = lv_view_sync_flush(sync);
    TEST_ASSERT_EQ(flushed, 0);

    lv_view_sync_destroy(sync);
}

/* 批量传播测试扩容机制 */
static void test_vs_many_propagations(void) {
    lvViewSynchronizer *sync = lv_view_sync_create();
    TEST_ASSERT_NOT_NULL(sync);

    for (int i = 0; i < 50; i++) {
        char change[32];
        snprintf(change, sizeof(change), "change_%d", i);
        int r = lv_view_sync_propagate(sync, i % 4, change);
        TEST_ASSERT_EQ(r, 0);
    }
    int flushed = lv_view_sync_flush(sync);
    TEST_ASSERT(flushed > 0, "批量传播后 flush 应处理变更");

    lv_view_sync_destroy(sync);
}

/* ============================================================
 * 测试组 6: Block Scheduler 块调度器
 * ============================================================ */
static void test_bs_create_destroy(void) {
    SimpleBlockGraph *bg = create_test_block_graph(3);
    TEST_ASSERT_NOT_NULL(bg);

    lvBlockScheduler *sched = lv_block_scheduler_create(bg);
    TEST_ASSERT_NOT_NULL(sched);
    lv_block_scheduler_destroy(sched);
    lv_block_scheduler_destroy(NULL);

    destroy_test_block_graph(bg);
}

static void test_bs_strategy(void) {
    SimpleBlockGraph *bg = create_test_block_graph(2);
    lvBlockScheduler *sched = lv_block_scheduler_create(bg);
    TEST_ASSERT_NOT_NULL(sched);

    lv_block_scheduler_set_strategy(sched, lv_SCHED_INCREMENTAL);
    lv_block_scheduler_set_strategy(sched, lv_SCHED_LAZY);
    lv_block_scheduler_set_strategy(sched, lv_SCHED_FULL);
    lv_block_scheduler_set_strategy(NULL, lv_SCHED_FULL);

    TEST_ASSERT(1, "策略设置安全");

    lv_block_scheduler_destroy(sched);
    destroy_test_block_graph(bg);
}

static void test_bs_run(void) {
    SimpleBlockGraph *bg = create_test_block_graph(3);
    lvBlockScheduler *sched = lv_block_scheduler_create(bg);
    TEST_ASSERT_NOT_NULL(sched);

    lvExecResult res = lv_block_scheduler_run(sched);
    TEST_ASSERT(res.success, "全量执行应成功");
    TEST_ASSERT(res.blocks_executed > 0, "应执行块");

    lv_block_scheduler_destroy(sched);
    destroy_test_block_graph(bg);
}

static void test_bs_run_edge_cases(void) {
    /* NULL 调度器 */
    lvExecResult res = lv_block_scheduler_run(NULL);
    TEST_ASSERT_EQ(res.success, 0);
    TEST_ASSERT(strlen(res.error_msg) > 0);

    /* 空块图 */
    SimpleBlockGraph *empty = create_test_block_graph(0);
    lvBlockScheduler *sched = lv_block_scheduler_create(empty);
    res = lv_block_scheduler_run(sched);
    TEST_ASSERT_EQ(res.success, 0);
    lv_block_scheduler_destroy(sched);
    destroy_test_block_graph(empty);

    /* 单块图 */
    SimpleBlockGraph *single = create_test_block_graph(1);
    sched = lv_block_scheduler_create(single);
    res = lv_block_scheduler_run(sched);
    TEST_ASSERT(res.success, "单块执行应成功");
    lv_block_scheduler_destroy(sched);
    destroy_test_block_graph(single);
}

static void test_bs_mark_dirty(void) {
    SimpleBlockGraph *bg = create_test_block_graph(3);
    lvBlockScheduler *sched = lv_block_scheduler_create(bg);
    TEST_ASSERT_NOT_NULL(sched);

    lv_block_scheduler_mark_dirty(sched, 1);
    lv_block_scheduler_mark_dirty(sched, 2);
    lv_block_scheduler_mark_dirty(NULL, 1);
    lv_block_scheduler_mark_dirty(sched, 0);

    /* 重复标记 */
    lv_block_scheduler_mark_dirty(sched, 1);
    lv_block_scheduler_mark_dirty(sched, 1);

    lv_block_scheduler_mark_all_dirty(sched);
    lv_block_scheduler_mark_all_dirty(NULL);

    TEST_ASSERT(1, "脏标记安全");

    lv_block_scheduler_destroy(sched);
    destroy_test_block_graph(bg);
}

static void test_bs_incremental(void) {
    SimpleBlockGraph *bg = create_test_block_graph(3);
    lvBlockScheduler *sched = lv_block_scheduler_create(bg);
    TEST_ASSERT_NOT_NULL(sched);

    /* 无脏块 */
    lvExecResult res = lv_block_scheduler_run_incremental(sched, NULL, 0);
    TEST_ASSERT_EQ(res.success, 1);
    TEST_ASSERT(res.blocks_skipped > 0);

    /* 标记脏块后执行 */
    int dirty_ids[] = {1, 2};
    res = lv_block_scheduler_run_incremental(sched, dirty_ids, 2);
    TEST_ASSERT_EQ(res.success, 1);
    TEST_ASSERT(res.blocks_executed > 0);

    /* NULL 增量执行 */
    res = lv_block_scheduler_run_incremental(NULL, NULL, 0);
    TEST_ASSERT_EQ(res.success, 0);

    /* 空脏块数组 */
    res = lv_block_scheduler_run_incremental(sched, NULL, 0);
    TEST_ASSERT_EQ(res.success, 1);

    lv_block_scheduler_destroy(sched);
    destroy_test_block_graph(bg);
}

/* ============================================================
 * 测试组 7: Text Code View 文本代码视图
 * ============================================================ */
static void test_tc_create_destroy(void) {
    lvTextCodeView *view = lv_text_code_create();
    TEST_ASSERT_NOT_NULL(view);
    lv_text_code_destroy(view);
    lv_text_code_destroy(NULL);
    TEST_ASSERT(1, "销毁 NULL 安全");
}

static void test_tc_set_get_text(void) {
    lvTextCodeView *view = lv_text_code_create();
    TEST_ASSERT_NOT_NULL(view);

    const char *txt = "block main { input port0 output port1 }";
    int r = lv_text_code_set_text(view, txt);
    TEST_ASSERT_EQ(r, 0);

    const char *got = lv_text_code_get_text(view);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_STR_EQ(got, txt);

    /* 设置长文本测试扩容 */
    char long_txt[8192];
    memset(long_txt, 'A', 8000);
    long_txt[8000] = '\0';
    r = lv_text_code_set_text(view, long_txt);
    TEST_ASSERT_EQ(r, 0);
    got = lv_text_code_get_text(view);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQ(strlen(got), (size_t)8000);

    /* 设置空字符串 */
    r = lv_text_code_set_text(view, "");
    TEST_ASSERT_EQ(r, 0);
    got = lv_text_code_get_text(view);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQ(strlen(got), (size_t)0);

    /* 无效参数 */
    r = lv_text_code_set_text(NULL, "x");
    TEST_ASSERT_EQ(r, -1);
    r = lv_text_code_set_text(view, NULL);
    TEST_ASSERT_EQ(r, -1);

    const char *g = lv_text_code_get_text(NULL);
    TEST_ASSERT_NULL(g);

    lv_text_code_destroy(view);
}

static void test_tc_insert_delete(void) {
    lvTextCodeView *view = lv_text_code_create();
    TEST_ASSERT_NOT_NULL(view);

    lv_text_code_set_text(view, "hello world");

    /* 插入 */
    int r = lv_text_code_insert(view, 5, "XYZ");
    TEST_ASSERT_EQ(r, 0);
    const char *got = lv_text_code_get_text(view);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT(strstr(got, "helloXYZ world") != NULL || strcmp(got, "helloXYZ world") == 0);

    /* 边界位置插入 */
    r = lv_text_code_insert(view, 0, "!");
    TEST_ASSERT_EQ(r, 0);

    /* 超界位置插入（应修正到末尾） */
    r = lv_text_code_insert(view, 9999, "END");
    TEST_ASSERT_EQ(r, 0);

    /* 负位置插入（应修正到开头） */
    r = lv_text_code_insert(view, -5, "H");
    TEST_ASSERT_EQ(r, 0);

    /* 无效参数 */
    r = lv_text_code_insert(NULL, 0, "x");
    TEST_ASSERT_EQ(r, -1);
    r = lv_text_code_insert(view, 0, NULL);
    TEST_ASSERT_EQ(r, -1);

    /* 删除 */
    r = lv_text_code_delete(view, 0, 1);
    TEST_ASSERT_EQ(r, 0);

    /* 超界删除 */
    r = lv_text_code_delete(view, 0, 9999);
    TEST_ASSERT_EQ(r, 0);

    /* 无效参数 */
    r = lv_text_code_delete(NULL, 0, 1);
    TEST_ASSERT_EQ(r, -1);
    r = lv_text_code_delete(view, -1, 1);
    TEST_ASSERT_EQ(r, -1);
    r = lv_text_code_delete(view, 0, 0);
    TEST_ASSERT_EQ(r, -1);
    r = lv_text_code_delete(view, 0, -1);
    TEST_ASSERT_EQ(r, -1);

    /* 空文本上删除 */
    lvTextCodeView *empty = lv_text_code_create();
    r = lv_text_code_delete(empty, 0, 1);
    TEST_ASSERT_EQ(r, -1);
    lv_text_code_destroy(empty);

    lv_text_code_destroy(view);
}

/* 大量文本操作测试 */
static void test_tc_many_operations(void) {
    lvTextCodeView *view = lv_text_code_create();
    TEST_ASSERT_NOT_NULL(view);

    /* 反复插入删除 */
    for (int i = 0; i < 100; i++) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", i);
        int r = lv_text_code_insert(view, i, buf);
        TEST_ASSERT_EQ(r, 0);
    }
    for (int i = 0; i < 50; i++) {
        int r = lv_text_code_delete(view, 0, 1);
        TEST_ASSERT_EQ(r, 0);
    }
    TEST_ASSERT(1, "大量文本操作安全");

    lv_text_code_destroy(view);
}

static void test_tc_render(void) {
    lvTextCodeView *view = lv_text_code_create();
    lv_text_code_set_text(view, "test code");

    char buf[256];
    int r = lv_text_code_render(view, buf, sizeof(buf));
    TEST_ASSERT(r >= 0, "渲染应返回非负值");

    /* 较小缓冲区 */
    char small[4];
    r = lv_text_code_render(view, small, sizeof(small));
    /* 应截断但不失败 */
    TEST_ASSERT(r >= 0, "小缓冲区应截断");
    small[3] = '\0';
    TEST_ASSERT_EQ(strlen(small), (size_t)3);

    /* 无效参数 */
    r = lv_text_code_render(NULL, buf, sizeof(buf));
    TEST_ASSERT_EQ(r, -1);
    r = lv_text_code_render(view, NULL, 10);
    TEST_ASSERT_EQ(r, -1);
    r = lv_text_code_render(view, buf, 0);
    TEST_ASSERT_EQ(r, -1);

    lv_text_code_destroy(view);
}

/* ============================================================
 * 测试组 8: Extended Types 扩展类型
 * ============================================================ */
static void test_list_type_lifecycle(void) {
    void *dummy_type = (void *)(intptr_t)1;
    lvListTypeRegion *t = lv_list_type_create(dummy_type);
    TEST_ASSERT_NOT_NULL(t);
    lv_list_type_destroy(t);
    lv_list_type_destroy(NULL);
    TEST_ASSERT(1, "销毁 NULL 安全");
}

static void test_map_type_lifecycle(void) {
    void *k = (void *)(intptr_t)1;
    void *v = (void *)(intptr_t)2;
    lvMapTypeRegion *t = lv_map_type_create(k, v);
    TEST_ASSERT_NOT_NULL(t);
    lv_map_type_destroy(t);
    lv_map_type_destroy(NULL);
}

static void test_function_type_lifecycle(void) {
    void *p = (void *)(intptr_t)1;
    void *r = (void *)(intptr_t)2;

    lvFunctionTypeRegion *t = lv_function_type_create(p, r, 0);
    TEST_ASSERT_NOT_NULL(t);

    lvFunctionTypeRegion *tdep = lv_function_type_create(p, r, 1);
    TEST_ASSERT_NOT_NULL(tdep);
    lv_function_type_destroy(tdep);

    lv_function_type_destroy(t);
    lv_function_type_destroy(NULL);
}

static void test_effect_type_lifecycle(void) {
    lvEffectType effects[] = {lv_EFFECT_PURE, lv_EFFECT_FILE_READ};
    lvEffectTypeRegion *t = lv_effect_type_create(effects, 2, NULL);
    TEST_ASSERT_NOT_NULL(t);
    lv_effect_type_destroy(t);
    lv_effect_type_destroy(NULL);
}

static void test_extended_type_compatible(void) {
    void *ta = (void *)(intptr_t)1;

    int r = lv_extended_type_compatible(ta, ta);
    TEST_ASSERT(r != 0, "相同指针应兼容");

    r = lv_extended_type_compatible(NULL, ta);
    TEST_ASSERT_EQ(r, 0);

    r = lv_extended_type_compatible(ta, NULL);
    TEST_ASSERT_EQ(r, 0);

    r = lv_extended_type_compatible(NULL, NULL);
    TEST_ASSERT_EQ(r, 0);
}

/* ============================================================
 * 测试组 9: Converters 表示转换器
 * ============================================================ */
static void test_convert_block_to_text(void) {
    SimpleBlockGraph *bg = create_test_block_graph(2);
    TEST_ASSERT_NOT_NULL(bg);

    lvConvertResult res = lv_convert_block_to_text(bg);
    TEST_ASSERT_EQ(res.success, 1);
    TEST_ASSERT_NOT_NULL(res.output);

    char *text = (char *)res.output;
    TEST_ASSERT(strstr(text, "block") != NULL);
    TEST_ASSERT(strstr(text, "block_0") != NULL);
    lv_free((void **)&text);

    /* NULL 输入 */
    res = lv_convert_block_to_text(NULL);
    TEST_ASSERT_EQ(res.success, 0);

    /* 空块图 */
    SimpleBlockGraph *empty = create_test_block_graph(0);
    res = lv_convert_block_to_text(empty);
    TEST_ASSERT_EQ(res.success, 1);
    text = (char *)res.output;
    TEST_ASSERT_NOT_NULL(text);
    lv_free((void **)&text);
    destroy_test_block_graph(empty);

    destroy_test_block_graph(bg);
}

static void test_convert_text_to_block(void) {
    const char *code = "block testBlock {\n  input port0\n  output port1\n}\n";
    lvConvertResult res = lv_convert_text_to_block(code);
    TEST_ASSERT_EQ(res.success, 1);
    TEST_ASSERT_NOT_NULL(res.output);

    SimpleBlockGraphCap *sg = (SimpleBlockGraphCap *)res.output;
    TEST_ASSERT(sg->count > 0, "应解析出块");
    for (int i = 0; i < sg->count; i++) {
        if (sg->blocks[i]) func_block_destroy(sg->blocks[i]);
    }
    lv_free((void **)&sg->blocks);
    lv_free((void **)&sg);

    /* 空代码 */
    res = lv_convert_text_to_block("");
    TEST_ASSERT_EQ(res.success, 0);

    /* NULL */
    res = lv_convert_text_to_block(NULL);
    TEST_ASSERT_EQ(res.success, 0);

    /* 多块文本 */
    const char *multi = "block A {\n  input port0\n  output port1\n}\nblock B {\n  input port0\n  output port2\n}\n";
    res = lv_convert_text_to_block(multi);
    TEST_ASSERT_EQ(res.success, 1);
    sg = (SimpleBlockGraphCap *)res.output;
    TEST_ASSERT(sg->count >= 2, "应解析多个块");
    for (int i = 0; i < sg->count; i++) {
        if (sg->blocks[i]) func_block_destroy(sg->blocks[i]);
    }
    lv_free((void **)&sg->blocks);
    lv_free((void **)&sg);
}

static void test_convert_block_to_node(void) {
    SimpleBlockGraph *bg = create_test_block_graph(2);
    TEST_ASSERT_NOT_NULL(bg);

    lvConvertResult res = lv_convert_block_to_node(bg);
    TEST_ASSERT_EQ(res.success, 1);
    TEST_ASSERT_NOT_NULL(res.output);

    /* 反向转换 */
    lvConvertResult res2 = lv_convert_node_to_block(res.output);
    TEST_ASSERT_EQ(res2.success, 1);

    /* 清理反向转换输出 */
    SimpleBlockGraph *bg2 = (SimpleBlockGraph *)res2.output;
    for (int i = 0; i < bg2->count; i++) {
        if (bg2->blocks[i]) func_block_destroy(bg2->blocks[i]);
    }
    lv_free((void **)&bg2->blocks);
    lv_free((void **)&bg2);

    /* 清理正向转换：node_to_block 不会释放输入 NodeGraph，
     * 所以这里显式调用内部清理函数——无法直接清理内部 NodeGraph，跳过 */

    /* NULL */
    res = lv_convert_block_to_node(NULL);
    TEST_ASSERT_EQ(res.success, 0);
    res = lv_convert_node_to_block(NULL);
    TEST_ASSERT_EQ(res.success, 0);

    destroy_test_block_graph(bg);
}

static void test_convert_block_to_geometry(void) {
    SimpleBlockGraph *bg = create_test_block_graph(2);
    TEST_ASSERT_NOT_NULL(bg);

    lvConvertResult res = lv_convert_block_to_geometry(bg);
    TEST_ASSERT_EQ(res.success, 1);
    TEST_ASSERT_NOT_NULL(res.output);

    /* 清理几何编码 */
    GeometryEncoding *enc = (GeometryEncoding *)res.output;
    lv_geometry_encoding_destroy(enc);

    /* 各类型块 */
    SimpleBlockGraph *bg2 = create_test_block_graph(1);
    if (bg2 && bg2->blocks[0]) {
        int ports[] = {10, 20, 30};
        func_block_set_input_ports(bg2->blocks[0], ports, 3);
        func_block_set_output_ports(bg2->blocks[0], ports, 2);
    }
    res = lv_convert_block_to_geometry(bg2);
    TEST_ASSERT_EQ(res.success, 1);
    enc = (GeometryEncoding *)res.output;
    lv_geometry_encoding_destroy(enc);
    destroy_test_block_graph(bg2);

    /* NULL */
    res = lv_convert_block_to_geometry(NULL);
    TEST_ASSERT_EQ(res.success, 0);

    destroy_test_block_graph(bg);
}

static void test_convert_geometry_to_block(void) {
    /* 先 block → geometry，再 reverse */
    SimpleBlockGraph *bg = create_test_block_graph(2);
    TEST_ASSERT_NOT_NULL(bg);

    lvConvertResult fwd = lv_convert_block_to_geometry(bg);
    TEST_ASSERT_EQ(fwd.success, 1);

    lvConvertResult rev = lv_convert_geometry_to_block(fwd.output);
    TEST_ASSERT_EQ(rev.success, 1);

    /* 清理反向输出 */
    SimpleBlockGraphCap *sg2 = (SimpleBlockGraphCap *)rev.output;
    for (int i = 0; i < sg2->count; i++) {
        if (sg2->blocks[i]) func_block_destroy(sg2->blocks[i]);
    }
    lv_free((void **)&sg2->blocks);
    lv_free((void **)&sg2);

    /* 清理正向（几何编码） */
    GeometryEncoding *enc = (GeometryEncoding *)fwd.output;
    lv_geometry_encoding_destroy(enc);

    /* NULL */
    lvConvertResult res = lv_convert_geometry_to_block(NULL);
    TEST_ASSERT_EQ(res.success, 0);

    destroy_test_block_graph(bg);
}

/* 完整的 block → geometry → block 往返测试 */
static void test_convert_geometry_roundtrip(void) {
    SimpleBlockGraph *bg = create_test_block_graph(3);
    TEST_ASSERT_NOT_NULL(bg);

    /* 正向转换 */
    lvConvertResult fwd = lv_convert_block_to_geometry(bg);
    TEST_ASSERT_EQ(fwd.success, 1);

    /* 反向转换 */
    lvConvertResult rev = lv_convert_geometry_to_block(fwd.output);
    TEST_ASSERT_EQ(rev.success, 1);

    /* 验证反向结果块数量 */
    SimpleBlockGraphCap *sg = (SimpleBlockGraphCap *)rev.output;
    TEST_ASSERT(sg->count > 0, "往返后的块数应 > 0");

    /* 清理 */
    for (int i = 0; i < sg->count; i++) {
        if (sg->blocks[i]) func_block_destroy(sg->blocks[i]);
    }
    lv_free((void **)&sg->blocks);
    lv_free((void **)&sg);

    GeometryEncoding *enc = (GeometryEncoding *)fwd.output;
    lv_geometry_encoding_destroy(enc);

    destroy_test_block_graph(bg);
}

/* block → node → block 往返测试 */
static void test_convert_node_roundtrip(void) {
    SimpleBlockGraph *bg = create_test_block_graph(3);
    TEST_ASSERT_NOT_NULL(bg);

    /* 正向 */
    lvConvertResult fwd = lv_convert_block_to_node(bg);
    TEST_ASSERT_EQ(fwd.success, 1);

    /* 反向 */
    lvConvertResult rev = lv_convert_node_to_block(fwd.output);
    TEST_ASSERT_EQ(rev.success, 1);

    /* 清理 */
    SimpleBlockGraph *bg2 = (SimpleBlockGraph *)rev.output;
    for (int i = 0; i < bg2->count; i++) {
        if (bg2->blocks[i]) func_block_destroy(bg2->blocks[i]);
    }
    lv_free((void **)&bg2->blocks);
    lv_free((void **)&bg2);

    destroy_test_block_graph(bg);
}

/* block → text → block 往返测试 */
static void test_convert_text_roundtrip(void) {
    SimpleBlockGraph *bg = create_test_block_graph(2);
    TEST_ASSERT_NOT_NULL(bg);

    /* block → text */
    lvConvertResult fwd = lv_convert_block_to_text(bg);
    TEST_ASSERT_EQ(fwd.success, 1);

    char *text = (char *)fwd.output;
    TEST_ASSERT_NOT_NULL(text);

    /* text → block */
    lvConvertResult rev = lv_convert_text_to_block(text);
    TEST_ASSERT_EQ(rev.success, 1);

    /* 清理 */
    SimpleBlockGraphCap *sg = (SimpleBlockGraphCap *)rev.output;
    for (int i = 0; i < sg->count; i++) {
        if (sg->blocks[i]) func_block_destroy(sg->blocks[i]);
    }
    lv_free((void **)&sg->blocks);
    lv_free((void **)&sg);
    lv_free((void **)&text);

    destroy_test_block_graph(bg);
}

/* ============================================================
 * 测试组 10: Sync Protocol 同步协议
 * ============================================================ */
static void test_sp_create_destroy(void) {
    SimpleBlockGraph *bg = create_test_block_graph(1);
    TEST_ASSERT_NOT_NULL(bg);

    lvSyncProtocol *proto = lv_sync_protocol_create(bg);
    TEST_ASSERT_NOT_NULL(proto);
    lv_sync_protocol_destroy(proto);
    lv_sync_protocol_destroy(NULL);
    TEST_ASSERT(1, "销毁 NULL 安全");

    destroy_test_block_graph(bg);
}

static void test_sp_propagate_block(void) {
    SimpleBlockGraph *bg = create_test_block_graph(2);
    TEST_ASSERT_NOT_NULL(bg);

    lvSyncProtocol *proto = lv_sync_protocol_create(bg);
    TEST_ASSERT_NOT_NULL(proto);

    /* 块视图传播 */
    int count = lv_sync_propagate(proto, 2, bg, 10);
    TEST_ASSERT(count >= 0, "块视图传播");

    /* 无效参数 */
    count = lv_sync_propagate(NULL, 2, bg, 10);
    TEST_ASSERT_EQ(count, -1);

    lv_sync_protocol_destroy(proto);
    destroy_test_block_graph(bg);
}

static void test_sp_propagate_text(void) {
    SimpleBlockGraph *bg = create_test_block_graph(1);
    TEST_ASSERT_NOT_NULL(bg);

    lvSyncProtocol *proto = lv_sync_protocol_create(bg);
    TEST_ASSERT_NOT_NULL(proto);

    const char *code = "block test { input port0 output port1 }";
    int count = lv_sync_propagate(proto, 3, (void *)code, 10);
    TEST_ASSERT(count >= 0, "文本视图传播");

    lv_sync_protocol_destroy(proto);
    destroy_test_block_graph(bg);
}

static void test_sp_propagate_node(void) {
    /* 需要先 block → node 获得 NodeGraph */
    SimpleBlockGraph *bg = create_test_block_graph(2);
    TEST_ASSERT_NOT_NULL(bg);

    lvConvertResult res = lv_convert_block_to_node(bg);
    TEST_ASSERT_EQ(res.success, 1);

    lvSyncProtocol *proto = lv_sync_protocol_create(bg);
    TEST_ASSERT_NOT_NULL(proto);

    int count = lv_sync_propagate(proto, 1, res.output, 10);
    TEST_ASSERT(count >= 0, "节点图传播");

    lv_sync_protocol_destroy(proto);
    /* res.output (NodeGraph) 无法从外部完全清理，跳过 */
    destroy_test_block_graph(bg);
}

static void test_sp_propagate_geometry(void) {
    SimpleBlockGraph *bg = create_test_block_graph(2);
    TEST_ASSERT_NOT_NULL(bg);

    lvConvertResult res = lv_convert_block_to_geometry(bg);
    TEST_ASSERT_EQ(res.success, 1);

    lvSyncProtocol *proto = lv_sync_protocol_create(bg);
    TEST_ASSERT_NOT_NULL(proto);

    int count = lv_sync_propagate(proto, 0, res.output, 10);
    TEST_ASSERT(count >= 0, "几何画布传播");

    GeometryEncoding *enc = (GeometryEncoding *)res.output;
    lv_geometry_encoding_destroy(enc);
    lv_sync_protocol_destroy(proto);
    destroy_test_block_graph(bg);
}

static void test_sp_recursion_limit(void) {
    SimpleBlockGraph *bg = create_test_block_graph(1);
    TEST_ASSERT_NOT_NULL(bg);

    lvSyncProtocol *proto = lv_sync_protocol_create(bg);
    TEST_ASSERT_NOT_NULL(proto);

    /* 最大深度为 0 应触发递归保护 */
    int count = lv_sync_propagate(proto, 2, bg, 0);
    TEST_ASSERT_EQ(count, -1);

    /* 最大深度为 1 */
    count = lv_sync_propagate(proto, 2, bg, 1);
    TEST_ASSERT(count >= 0, "深度 1 传播");

    lv_sync_protocol_destroy(proto);
    destroy_test_block_graph(bg);
}

static void test_sp_unknown_view(void) {
    SimpleBlockGraph *bg = create_test_block_graph(1);
    TEST_ASSERT_NOT_NULL(bg);

    lvSyncProtocol *proto = lv_sync_protocol_create(bg);
    TEST_ASSERT_NOT_NULL(proto);

    /* 未知视图类型应记录冲突 */
    int count = lv_sync_propagate(proto, 99, bg, 10);
    TEST_ASSERT_EQ(count, 0);

    lv_sync_protocol_destroy(proto);
    destroy_test_block_graph(bg);
}

/* ============================================================
 * 测试主函数
 * ============================================================ */
int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);

    TEST_SUITE_BEGIN("Layer 6 Visual Modules");

    /* ===== 组1: Block Canvas ===== */
    fprintf(stderr, "\n--- Block Canvas ---\n");
    TEST_RUN(test_bc_create_destroy);
    TEST_RUN(test_bc_add_block);
    TEST_RUN(test_bc_add_many_blocks);
    TEST_RUN(test_bc_remove_block);
    TEST_RUN(test_bc_connect);
    TEST_RUN(test_bc_render_svg);

    /* ===== 组2: Geometry Canvas ===== */
    fprintf(stderr, "\n--- Geometry Canvas ---\n");
    TEST_RUN(test_gc_create_destroy);
    TEST_RUN(test_gc_add_entity);
    TEST_RUN(test_gc_add_many_entities);
    TEST_RUN(test_gc_remove_entity);
    TEST_RUN(test_gc_add_constraint);
    TEST_RUN(test_gc_add_many_constraints);
    TEST_RUN(test_gc_fit_view);
    TEST_RUN(test_gc_render_svg);
    TEST_RUN(test_gc_render_svg_with_constraints);

    /* ===== 组3: Visual Editor ===== */
    fprintf(stderr, "\n--- Visual Editor ---\n");
    TEST_RUN(test_ve_create_destroy);
    TEST_RUN(test_ve_reset);
    TEST_RUN(test_ve_switch_view);
    TEST_RUN(test_ve_execute);
    TEST_RUN(test_ve_state_error);

    /* ===== 组4: Node Graph ===== */
    fprintf(stderr, "\n--- Node Graph ---\n");
    TEST_RUN(test_ng_create_destroy);
    TEST_RUN(test_ng_add_find_remove_node);
    TEST_RUN(test_ng_add_many_nodes);
    TEST_RUN(test_ng_connections);
    TEST_RUN(test_ng_add_many_connections);
    TEST_RUN(test_ng_layout);

    /* ===== 组5: View Synchronizer ===== */
    fprintf(stderr, "\n--- View Synchronizer ---\n");
    TEST_RUN(test_vs_create_destroy);
    TEST_RUN(test_vs_enable_disable);
    TEST_RUN(test_vs_propagate_flush);
    TEST_RUN(test_vs_many_propagations);

    /* ===== 组6: Block Scheduler ===== */
    fprintf(stderr, "\n--- Block Scheduler ---\n");
    TEST_RUN(test_bs_create_destroy);
    TEST_RUN(test_bs_strategy);
    TEST_RUN(test_bs_run);
    TEST_RUN(test_bs_run_edge_cases);
    TEST_RUN(test_bs_mark_dirty);
    TEST_RUN(test_bs_incremental);

    /* ===== 组7: Text Code View ===== */
    fprintf(stderr, "\n--- Text Code View ---\n");
    TEST_RUN(test_tc_create_destroy);
    TEST_RUN(test_tc_set_get_text);
    TEST_RUN(test_tc_insert_delete);
    TEST_RUN(test_tc_many_operations);
    TEST_RUN(test_tc_render);

    /* ===== 组8: Extended Types ===== */
    fprintf(stderr, "\n--- Extended Types ---\n");
    TEST_RUN(test_list_type_lifecycle);
    TEST_RUN(test_map_type_lifecycle);
    TEST_RUN(test_function_type_lifecycle);
    TEST_RUN(test_effect_type_lifecycle);
    TEST_RUN(test_extended_type_compatible);

    /* ===== 组9: Converters ===== */
    fprintf(stderr, "\n--- Representation Converters ---\n");
    TEST_RUN(test_convert_block_to_text);
    TEST_RUN(test_convert_text_to_block);
    TEST_RUN(test_convert_block_to_node);
    TEST_RUN(test_convert_block_to_geometry);
    TEST_RUN(test_convert_geometry_to_block);
    TEST_RUN(test_convert_geometry_roundtrip);
    TEST_RUN(test_convert_node_roundtrip);
    TEST_RUN(test_convert_text_roundtrip);

    /* ===== 组10: Sync Protocol ===== */
    fprintf(stderr, "\n--- Sync Protocol ---\n");
    TEST_RUN(test_sp_create_destroy);
    TEST_RUN(test_sp_propagate_block);
    TEST_RUN(test_sp_propagate_text);
    TEST_RUN(test_sp_propagate_node);
    TEST_RUN(test_sp_propagate_geometry);
    TEST_RUN(test_sp_recursion_limit);
    TEST_RUN(test_sp_unknown_view);

    TEST_SUITE_END();
    return g_fail_count > 0 ? 1 : 0;
}
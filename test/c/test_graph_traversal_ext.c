/**
 * @file test_graph_traversal_ext.c
 * @brief 图遍历抽象层契约测试（批次 C-㊺：lv_graph_traversal.h 12 个零覆盖 API）
 *
 * 覆盖 12 个 ctest 零覆盖 API：
 *   - 约束图便利族：lv_graph_count_nodes / lv_graph_has_cycle /
 *     lv_graph_topological_sort / lv_graph_traverse /
 *     lv_graph_traverse_from / lv_graph_traverse_neighbors
 *   - 通用算法族：lv_bfs_run / lv_topo_run / lv_cycle_detect
 *   - 树族：lv_tree_traverse（lv_tree_release_recursive 为 inline）
 *   - 字符串族：lv_traversal_order_to_string / lv_traversal_result_to_string
 *
 * 契约要点（与实现核对）：
 *   - lv_bfs_run：NULL spec/neighbors 或 node_count<=0 → -1。
 *   - lv_graph_topological_sort：NULL 参数 → -1；空图 → lv_OK + count 0；
 *     有环 → -1。
 *   - lv_graph_count_nodes：NULL → 0（活跃节点计数）。
 *   - 字符串族未知枚举 → "UNKNOWN"。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/lv_graph_traversal.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* 4 节点 DAG：0→1, 0→2, 1→3 */
static int dag_neighbors(void *ctx, int node_id, int batch, int *out, void **infos, int max) {
    (void)ctx;
    (void)infos;
    if (batch != 0 || !out || max <= 0)
        return 0;
    switch (node_id) {
    case 0:
        if (max < 2)
            return 2;
        out[0] = 1;
        out[1] = 2;
        return 2;
    case 1:
        if (max < 1)
            return 1;
        out[0] = 3;
        return 1;
    default:
        return 0;
    }
}

/* 有环图：2→0 加回边（0→1→2→0 环） */
static int cycle_neighbors(void *ctx, int node_id, int batch, int *out, void **infos, int max) {
    (void)ctx;
    (void)infos;
    if (batch != 0 || !out || max <= 0)
        return 0;
    switch (node_id) {
    case 0:
        out[0] = 1;
        return 1;
    case 1:
        out[0] = 2;
        return 1;
    case 2:
        out[0] = 0; /* 回边成环 */
        return 1;
    default:
        return 0;
    }
}

/* BFS 访问统计 */
typedef struct {
    int visits[8];
    int count;
} VisitStat;

static lvTraversalResult bfs_visit(void *ctx, int node_id) {
    VisitStat *st = (VisitStat *)ctx;
    if (st->count < 8)
        st->visits[st->count++] = node_id;
    return lv_TRAVERSAL_CONTINUE;
}

/* ============== 测试：便利函数 ============== */

static void test_util_api(void) {
    /* count_nodes：NULL → 0；空图 → 0；加节点后计数 */
    TEST_ASSERT_EQ(lv_graph_count_nodes(NULL), 0);
    ConstraintGraph *g = graph_create();
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_EQ(lv_graph_count_nodes(g), 0);
    add_point(g, 0, 1, 0, 1);
    add_point(g, 1, 1, 0, 1);
    TEST_ASSERT_EQ(lv_graph_count_nodes(g), 2);

    /* has_cycle：NULL/空 → false；简单图（无约束）→ false */
    TEST_ASSERT(!lv_graph_has_cycle(NULL), "NULL 无环");
    TEST_ASSERT(!lv_graph_has_cycle(g), "无约束图无环");

    /* topological_sort：NULL 参数 → -1；无约束图 → OK + count */
    int *nodes = NULL;
    int count = -1;
    TEST_ASSERT_EQ(lv_graph_topological_sort(NULL, &nodes, &count), -1);
    TEST_ASSERT_EQ(lv_graph_topological_sort(g, NULL, &count), -1);
    TEST_ASSERT_EQ(lv_graph_topological_sort(g, &nodes, NULL), -1);
    TEST_ASSERT_EQ(lv_graph_topological_sort(g, &nodes, &count), lv_OK);
    TEST_ASSERT_EQ(count, 2);
    TEST_ASSERT_NOT_NULL(nodes);
    lv_free((void **)&nodes);

    graph_destroy(g);
    printf("  test_util_api: PASSED\n");
}

/* ============== 测试：字符串映射 ============== */

static void test_string_api(void) {
    /* order：5 枚举 + 未知 */
    TEST_ASSERT_STR_EQ(lv_traversal_order_to_string(lv_TRAVERSAL_DFS_PRE), "DFS_PRE");
    TEST_ASSERT_STR_EQ(lv_traversal_order_to_string(lv_TRAVERSAL_DFS_POST), "DFS_POST");
    TEST_ASSERT_STR_EQ(lv_traversal_order_to_string(lv_TRAVERSAL_BFS), "BFS");
    TEST_ASSERT_STR_EQ(lv_traversal_order_to_string(lv_TRAVERSAL_TOPOLOGICAL), "TOPOLOGICAL");
    TEST_ASSERT_STR_EQ(lv_traversal_order_to_string(lv_TRAVERSAL_REVERSE_TOPOLOGICAL), "REVERSE_TOPOLOGICAL");
    TEST_ASSERT_STR_EQ(lv_traversal_order_to_string((lvTraversalOrder)99), "UNKNOWN");

    /* result：3 枚举 + 未知 */
    TEST_ASSERT_STR_EQ(lv_traversal_result_to_string(lv_TRAVERSAL_CONTINUE), "CONTINUE");
    TEST_ASSERT_STR_EQ(lv_traversal_result_to_string(lv_TRAVERSAL_SKIP_CHILDREN), "SKIP_CHILDREN");
    TEST_ASSERT_STR_EQ(lv_traversal_result_to_string(lv_TRAVERSAL_STOP), "STOP");
    TEST_ASSERT_STR_EQ(lv_traversal_result_to_string((lvTraversalResult)99), "UNKNOWN");

    printf("  test_string_api: PASSED\n");
}

/* ============== 测试：通用 BFS ============== */

static void test_bfs_api(void) {
    /* NULL 契约 → -1 */
    TEST_ASSERT_EQ(lv_bfs_run(NULL), -1);
    lvBfsSpec bad = {0};
    TEST_ASSERT_EQ(lv_bfs_run(&bad), -1); /* node_count=0 */

    /* 正路径：DAG 从 0 开始 BFS → 访问 0,1,2,3 */
    int seeds[1] = {0};
    VisitStat st;
    memset(&st, 0, sizeof(st));
    lvBfsSpec spec = {
        .node_count = 4,
        .seeds = seeds,
        .seed_count = 1,
        .visited = NULL,
        .mark_on_enqueue = true,
        .max_queue = 0,
        .neighbors = dag_neighbors,
        .visit = bfs_visit,
        .ctx = &st,
    };
    TEST_ASSERT_EQ(lv_bfs_run(&spec), 4); /* 出队 4 节点 */
    TEST_ASSERT_EQ(st.count, 4);
    TEST_ASSERT_EQ(st.visits[0], 0);

    /* visit NULL：仅遍历不回调 */
    spec.visit = NULL;
    TEST_ASSERT_EQ(lv_bfs_run(&spec), 4);

    printf("  test_bfs_api: PASSED\n");
}

/* ============== 测试：通用拓扑排序 ============== */

static void test_topo_run_api(void) {
    /* NULL 契约 → -1 */
    TEST_ASSERT_EQ(lv_topo_run(NULL), -1);
    lvTopoSpec bad = {0};
    TEST_ASSERT_EQ(lv_topo_run(&bad), -1); /* node_count=0 */

    /* DAG 拓扑：0→1→3 与 0→2，序中 0 先于 1/2，1 先于 3 */
    int order[4];
    lvTopoSpec spec = {
        .node_count = 4,
        .nodes = NULL,
        .nodes_count = 0,
        .out_order = order,
        .successors = dag_neighbors,
        .ctx = NULL,
    };
    int n = lv_topo_run(&spec);
    TEST_ASSERT_EQ(n, 4); /* 全部排序 */
    /* 位置校验：0 在 1/2 前、1 在 3 前 */
    int pos[4];
    for (int i = 0; i < n; i++)
        pos[order[i]] = i;
    TEST_ASSERT(pos[0] < pos[1] && pos[0] < pos[2], "0 先于 1/2");
    TEST_ASSERT(pos[1] < pos[3], "1 先于 3");

    /* 有环：仅输出无环部分（n < 待排序数） */
    int order2[3];
    lvTopoSpec cs = {
        .node_count = 3,
        .nodes = NULL,
        .nodes_count = 0,
        .out_order = order2,
        .successors = cycle_neighbors,
        .ctx = NULL,
    };
    TEST_ASSERT(lv_topo_run(&cs) < 3, "环检测：排序数小于节点数");

    printf("  test_topo_run_api: PASSED\n");
}

/* ============== 测试：通用环检测 ============== */

static void test_cycle_detect_api(void) {
    /* NULL 契约 → false */
    TEST_ASSERT(!lv_cycle_detect(NULL), "NULL spec 无环");

    /* DAG 无环 */
    int seeds[4] = {0, 1, 2, 3};
    lvCycleDetectSpec dag = {
        .node_count = 4,
        .seeds = seeds,
        .seed_count = 4,
        .neighbors = dag_neighbors,
        .on_cycle = NULL,
        .ctx = NULL,
    };
    TEST_ASSERT(!lv_cycle_detect(&dag), "DAG 无环");

    /* 回边图有环（seeds 须在 0..node_count-1 范围内） */
    int cyc_seeds[3] = {0, 1, 2};
    lvCycleDetectSpec cyc = {
        .node_count = 3,
        .seeds = cyc_seeds,
        .seed_count = 3,
        .neighbors = cycle_neighbors,
        .on_cycle = NULL,
        .ctx = NULL,
    };
    TEST_ASSERT(lv_cycle_detect(&cyc), "回边图有环");

    printf("  test_cycle_detect_api: PASSED\n");
}

/* ============== 测试：树遍历 ============== */

/* 简单树节点（堆分配，children 数组） */
typedef struct TNode {
    int id;
    struct TNode **children;
    int child_count;
} TNode;

/* lv_tree_traverse 契约：get_children 返回的数组由遍历方 lv_free 释放
 * （每次调用须返回新分配的堆数组），此处复制子指针到堆 */
static int tree_get_children(void *node, void ***out_children) {
    TNode *n = (TNode *)node;
    *out_children = NULL;
    if (n->child_count == 0)
        return 0;
    void **arr = lv_malloc((size_t)n->child_count * sizeof(void *));
    for (int i = 0; i < n->child_count; i++)
        arr[i] = n->children[i];
    *out_children = arr;
    return n->child_count;
}

/* lv_tree_release_recursive 专用回调（lvTreeGetChildrenFn：void** + int*） */
static void **tree_rel_get_children(void *node, int *out_count) {
    TNode *n = (TNode *)node;
    *out_count = n->child_count;
    return (void **)n->children;
}

static void tree_release_cleanup(void *node) {
    TNode *n = (TNode *)node;
    lv_free((void **)&n->children);
}

typedef struct {
    int order[8];
    int count;
} TreeStat;

static lvTraversalResult tree_visit(void *node, int depth, void *user_data) {
    (void)depth;
    TreeStat *st = (TreeStat *)user_data;
    TNode *n = (TNode *)node;
    if (st->count < 8)
        st->order[st->count++] = n->id;
    return lv_TRAVERSAL_CONTINUE;
}

static void test_tree_api(void) {
    /* 构造树：root(0) → [1, 2]，1 → [3] */
    TNode n3 = {3, NULL, 0};
    TNode *c1kids[1] = {&n3};
    TNode n1 = {1, c1kids, 1};
    TNode n2 = {2, NULL, 0};
    TNode *root_kids[2] = {&n1, &n2};
    TNode root = {0, root_kids, 2};

    /* 前序遍历：0,1,3,2 */
    TreeStat st;
    memset(&st, 0, sizeof(st));
    lvTreeTraversalConfig cfg = lv_TREE_TRAVERSAL_DEFAULT_CONFIG;
    TEST_ASSERT_EQ(lv_tree_traverse(&root, tree_visit, &st, tree_get_children, &cfg), lv_OK);
    TEST_ASSERT_EQ(st.count, 4);
    TEST_ASSERT_EQ(st.order[0], 0);
    TEST_ASSERT_EQ(st.order[1], 1);
    TEST_ASSERT_EQ(st.order[2], 3);
    TEST_ASSERT_EQ(st.order[3], 2);

    /* NULL 契约：root NULL / get_children NULL → -1 */
    TEST_ASSERT_EQ(lv_tree_traverse(NULL, tree_visit, &st, tree_get_children, &cfg), -1);
    TEST_ASSERT_EQ(lv_tree_traverse(&n2, tree_visit, &st, NULL, &cfg), -1);

    /* inline 递归释放（后序）：先子后父，cleanup 释放 children */
    TNode *heap_root = lv_calloc(1, sizeof(TNode));
    heap_root->id = 10;
    TNode *h1 = lv_calloc(1, sizeof(TNode));
    h1->id = 11;
    TNode **kids = lv_calloc(1, sizeof(TNode *));
    kids[0] = h1;
    heap_root->children = kids;
    heap_root->child_count = 1;
    lv_tree_release_recursive(heap_root, tree_rel_get_children, tree_release_cleanup);

    printf("  test_tree_api: PASSED\n");
}

/* ========== 蓝图图 API（TEN_LAYER_OPTIMIZED_PLAN §12.4/§15.2.2/§15.4，批次 G1c） ========== */

/* 变更回调记录 */
static int g_cb_events = 0;
static int g_cb_last_node = -1;
static int g_cb_last_type = -1;
static void test_change_callback(int graph_id, int node_id, int change_type, void *user_data) {
    (void) graph_id;
    (void) user_data;
    g_cb_events++;
    g_cb_last_node = node_id;
    g_cb_last_type = change_type;
}

static void test_blueprint_graph_api(void) {
    ConstraintGraph *g = graph_create();
    TEST_ASSERT_NOT_NULL(g);

    /* lv_graph_add_point：double 坐标建点 */
    int p0 = lv_graph_add_point(g, 0.0, 0.0);
    int p1 = lv_graph_add_point(g, 3.0, 4.0);
    int p2 = lv_graph_add_point(g, 1.0, 1.0);
    int p3 = lv_graph_add_point(g, 5.0, 2.0);
    TEST_ASSERT(p0 >= 0 && p1 >= 0 && p2 >= 0 && p3 >= 0, "add_point 均成功");
    TEST_ASSERT_EQ(p1, p0 + 1);

    /* 两条线段（ID 4,5）+ 平行约束连接 0-1 与 2-3 */
    TEST_ASSERT_EQ(graph_add_line_segment(g, p0, p1), ADD_NODE_OK);
    TEST_ASSERT_EQ(graph_add_line_segment(g, p2, p3), ADD_NODE_OK);
    TEST_ASSERT_EQ(graph_add_parallel(g, 4, 5), ADD_CONSTRAINT_OK);

    /* lv_graph_get_nodes_by_type：POINT 4 个、LINE_SEGMENT 2 个 */
    int *ids = NULL;
    int cnt = 0;
    TEST_ASSERT(lv_graph_get_nodes_by_type(g, GEOM_POINT, &ids, &cnt), "POINT 查询成功");
    TEST_ASSERT_EQ(cnt, 4);
    lv_free((void **) &ids);
    TEST_ASSERT(lv_graph_get_nodes_by_type(g, GEOM_LINE_SEGMENT, &ids, &cnt), "SEGMENT 查询成功");
    TEST_ASSERT_EQ(cnt, 2);
    TEST_ASSERT(ids[0] == 4 && ids[1] == 5, "线段 ID 正确");
    lv_free((void **) &ids);
    TEST_ASSERT(lv_graph_get_nodes_by_type(g, GEOM_CIRCLE, &ids, &cnt), "CIRCLE 空查询成功");
    TEST_ASSERT_EQ(cnt, 0);
    lv_free((void **) &ids);
    TEST_ASSERT(!lv_graph_get_nodes_by_type(g, GEOM_POINT, NULL, &cnt), "NULL out 拒绝");
    TEST_ASSERT(!lv_graph_get_nodes_by_type(NULL, GEOM_POINT, &ids, &cnt), "NULL graph 拒绝");

    /* lv_graph_get_dependents：节点 4（线段）参与的平行约束另一参与者为 5 */
    int *deps = lv_graph_get_dependents(g, 4);
    TEST_ASSERT_NOT_NULL(deps);
    int found_5 = 0;
    for (int i = 0; deps[i] >= 0; i++) {
        if (deps[i] == 5) found_5 = 1;
    }
    TEST_ASSERT(found_5, "依赖含 5");
    lv_free((void **) &deps);

    /* 节点 5 的依赖为 4 */
    deps = lv_graph_get_dependents(g, 5);
    TEST_ASSERT_NOT_NULL(deps);
    int found_4 = 0;
    for (int i = 0; deps[i] >= 0; i++) {
        if (deps[i] == 4) found_4 = 1;
    }
    TEST_ASSERT(found_4, "依赖含 4");
    lv_free((void **) &deps);

    /* 无依赖节点 → 仅 -1 结尾 */
    deps = lv_graph_get_dependents(g, 0);
    TEST_ASSERT_NOT_NULL(deps);
    TEST_ASSERT_EQ(deps[0], -1);
    lv_free((void **) &deps);
    TEST_ASSERT_NULL(lv_graph_get_dependents(NULL, 0));
    TEST_ASSERT_NULL(lv_graph_get_dependents(g, -1));

    /* 变更回调：注册 + 触发 + 覆盖 + 取消 */
    g_cb_events = 0;
    TEST_ASSERT(lv_graph_register_change_callback(g, test_change_callback, NULL), "注册回调");
    lv_graph_on_node_changed(g, 7, 2);
    TEST_ASSERT_EQ(g_cb_events, 1);
    TEST_ASSERT_EQ(g_cb_last_node, 7);
    TEST_ASSERT_EQ(g_cb_last_type, 2);
    lv_graph_on_node_changed(g, 8, 0);
    TEST_ASSERT_EQ(g_cb_events, 2);
    TEST_ASSERT(lv_graph_register_change_callback(g, NULL, NULL), "取消回调");
    lv_graph_on_node_changed(g, 9, 1);
    TEST_ASSERT_EQ(g_cb_events, 2); /* 取消后不再触发 */
    TEST_ASSERT(!lv_graph_register_change_callback(NULL, test_change_callback, NULL), "NULL graph 拒绝");

    /* lv_graph_decompose：4 点 + 2 线段 + 平行约束 → 单连通分量（所有节点相连） */
    lvSubgraphTask *tasks = NULL;
    int task_count = 0;
    TEST_ASSERT_EQ(lv_graph_decompose(g, &tasks, &task_count), 0);
    TEST_ASSERT_EQ(task_count, 1);
    TEST_ASSERT_EQ(tasks[0].node_count, 6);
    for (int i = 0; i < tasks[0].node_count; i++)
        lv_free((void **) &tasks[0].node_ids);
    lv_free((void **) &tasks);

    /* 分解 NULL 契约 */
    TEST_ASSERT_EQ(lv_graph_decompose(NULL, &tasks, &task_count), -1);
    TEST_ASSERT_EQ(lv_graph_decompose(g, NULL, &task_count), -1);

    /* G5-internal：蓝图内部访问转发（ENGINE_INTERNAL / internal_*） */
    GeomNode *inode = lv_internal_get_node(g, p0);
    TEST_ASSERT_NOT_NULL(inode);
    TEST_ASSERT_EQ(inode->id, p0);
    TEST_ASSERT_NULL(lv_internal_get_node(g, 9999));
    TEST_ASSERT(lv_internal_remove_node(g, 9999) == false, "未知节点移除 false");
    GeomNode *ginfo = lv_internal_get_node(g, 0);
    TEST_ASSERT_NOT_NULL(ginfo);

    graph_destroy(g);
    printf("  test_blueprint_graph_api: PASSED\n");
}

/* ============== 测试入口 ============== */

TEST_MAIN_BEGIN("Lv-00 Graph Traversal Ext Test Suite")
    printf("=== Lv-00 Graph Traversal Ext Test Suite (batch C-㊺) ===\n\n");
    lv_init();

    TEST_MAIN_RUN(test_util_api);
    TEST_MAIN_RUN(test_string_api);
    TEST_MAIN_RUN(test_bfs_api);
    TEST_MAIN_RUN(test_topo_run_api);
    TEST_MAIN_RUN(test_cycle_detect_api);
    TEST_MAIN_RUN(test_tree_api);
    TEST_MAIN_RUN(test_blueprint_graph_api);

    lv_cleanup();
TEST_MAIN_END()

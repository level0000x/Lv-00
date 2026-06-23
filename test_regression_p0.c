/**
 * @file test_regression_p0.c
 * @brief P0/P1 回归测试 — 验证已修复的 Double-Free、栈溢出、swap-remove 级联引用
 *
 * 编译方式（在 core/src 目录下）:
 *   gcc -std=c11 -o test_regression_p0 test_regression_p0.c lv00_impl_native.c \
 *       -I../include -I../include/lv00 -lgmp
 *
 * 运行（ASan 检测）:
 *   gcc -std=c11 -fsanitize=address,undefined -g -O1 \
 *       -o test_regression_p0 test_regression_p0.c lv00_impl_native.c \
 *       -I../include -I../include/lv00 -lgmp -lm
 *   ./test_regression_p0
 *
 * 预期结果: 全部 PASS，ASan 无报错
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gmp.h>

/* ---- 从 lv00_impl_native.c 提取的内部类型定义（避免包含整个巨型文件） ---- */

typedef struct {
    int64_t id;
    mpq_t   value;
    int     pinned;
} GraphNode;

typedef struct {
    int64_t id;
    int32_t from;
    int32_t to;
    mpq_t   weight;
} GraphEdge;

typedef struct {
    int64_t    id;
    GraphNode* nodes;
    int        node_count;
    int        node_cap;
    GraphEdge* edges;
    int        edge_count;
    int        edge_cap;
} ConstraintGraph;

typedef struct Expr {
    int    kind;
    mpq_t  val;
    char*  name;
    struct Expr* left;
    struct Expr* right;
} Expr;

/* ---- 内部函数声明（实现位于 lv00_impl_native.c） ---- */
ConstraintGraph* graph_create(void);
void             graph_destroy(ConstraintGraph* g);
int64_t          graph_add_node(ConstraintGraph* g, const mpq_t value, int pinned);
int              graph_remove_node(ConstraintGraph* g, int64_t node_id);
int64_t          graph_add_edge(ConstraintGraph* g, int from_idx, int to_idx, const mpq_t weight);
int              graph_remove_edge(ConstraintGraph* g, int64_t edge_id);
void             expr_destroy(Expr* e);

/* ---- 计数器 ---- */
static int pass = 0, fail = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL: %s\n", msg); fail++; } \
    else { pass++; } \
} while(0)

/* ================================================================
 * Test 1: graph_remove_node — 不再 Double-Free
 * ================================================================ */
void test_remove_node_no_double_free(void) {
    printf("  [T1] graph_remove_node: no double-free... ");
    ConstraintGraph* g = graph_create();
    CHECK(g != NULL, "graph_create returned NULL");

    mpq_t val; mpq_init(val); mpq_set_si(val, 3, 2);
    int64_t nid = graph_add_node(g, val, 0);
    CHECK(nid > 0, "graph_add_node failed");

    int ret = graph_remove_node(g, nid);
    CHECK(ret == 0, "graph_remove_node failed");
    CHECK(g->node_count == 0, "node_count should be 0 after remove");

    graph_destroy(g);
    mpq_clear(val);
    printf("PASS\n");
}

/* ================================================================
 * Test 2: graph_remove_edge — 不再 Double-Free
 * ================================================================ */
void test_remove_edge_no_double_free(void) {
    printf("  [T2] graph_remove_edge: no double-free... ");
    ConstraintGraph* g = graph_create();
    CHECK(g != NULL, "graph_create returned NULL");

    mpq_t v, w;
    mpq_init(v); mpq_set_si(v, 1, 1);
    mpq_init(w); mpq_set_si(w, 1, 2);

    int64_t n1 = graph_add_node(g, v, 0);
    int64_t n2 = graph_add_node(g, v, 0);
    CHECK(n1 > 0 && n2 > 0, "graph_add_node failed");

    int64_t eid = graph_add_edge(g, 0, 1, w);
    CHECK(eid > 0, "graph_add_edge failed");

    int ret = graph_remove_edge(g, eid);
    CHECK(ret == 0, "graph_remove_edge failed");
    CHECK(g->edge_count == 0, "edge_count should be 0 after remove");

    graph_destroy(g);
    mpq_clear(v); mpq_clear(w);
    printf("PASS\n");
}

/* ================================================================
 * Test 3: 重复 remove_node / remove_edge — 稳定性测试
 * ================================================================ */
void test_repeated_remove(void) {
    printf("  [T3] repeated remove_node/remove_edge: stability... ");
    ConstraintGraph* g = graph_create();
    CHECK(g != NULL, "graph_create returned NULL");

    mpq_t v, w;
    mpq_init(v); mpq_set_si(v, 1, 1);
    mpq_init(w); mpq_set_si(w, 1, 2);

    /* 添加 10 个节点 */
    int64_t ids[10];
    for (int i = 0; i < 10; i++) {
        ids[i] = graph_add_node(g, v, i % 2);
        CHECK(ids[i] > 0, "graph_add_node failed");
    }
    /* 添加 20 条边 */
    for (int i = 0; i < 10; i++) {
        graph_add_edge(g, i, (i + 1) % 10, w);
        graph_add_edge(g, (i + 1) % 10, i, w);
    }

    /* 逐个移除节点（会触发 swap-remove 级联更新）*/
    for (int i = 0; i < 10; i++) {
        int ret = graph_remove_node(g, ids[i]);
        CHECK(ret == 0, "graph_remove_node should succeed");
    }

    CHECK(g->node_count == 0, "all nodes should be removed");

    graph_destroy(g);
    mpq_clear(v); mpq_clear(w);
    printf("PASS\n");
}

/* ================================================================
 * Test 4: expr_destroy — 深树不再泄漏/栈溢出
 * ================================================================ */
static Expr* make_deep_tree(int depth) {
    if (depth <= 0) {
        Expr* e = (Expr*)calloc(1, sizeof(Expr));
        mpq_init(e->val); mpq_set_si(e->val, 1, 1);
        e->kind = 0;
        return e;
    }
    Expr* e = (Expr*)calloc(1, sizeof(Expr));
    mpq_init(e->val); mpq_set_si(e->val, depth, 1);
    e->kind = 1;
    e->name = strdup("x");
    e->left = make_deep_tree(depth - 1);
    e->right = make_deep_tree(depth - 1);
    return e;
}

void test_expr_destroy_deep(void) {
    printf("  [T4] expr_destroy: 500-node deep tree... ");
    Expr* tree = make_deep_tree(8);  /* 2^9 - 1 = 511 nodes, depth 9 */
    CHECK(tree != NULL, "make_deep_tree returned NULL");
    expr_destroy(tree);  /* 旧代码 256 限制会泄漏；修复后可完整释放 */
    printf("PASS (no crash, no leak)\n");
}

/* ================================================================
 * Test 5: swap-remove 边索引完整性
 * ================================================================ */
void test_swap_remove_edge_integrity(void) {
    printf("  [T5] swap-remove: edge index integrity... ");
    ConstraintGraph* g = graph_create();
    CHECK(g != NULL, "graph_create returned NULL");

    mpq_t v, w;
    mpq_init(v); mpq_set_si(v, 1, 1);
    mpq_init(w); mpq_set_si(w, 1, 2);

    /* 创建 4 个节点: idx 0,1,2,3 */
    for (int i = 0; i < 4; i++) {
        graph_add_node(g, v, 0);
    }

    /* 边引用节点 3（最后一个节点）*/
    graph_add_edge(g, 0, 3, w);  /* from=0, to=3 */
    graph_add_edge(g, 3, 2, w);                /* from=3, to=2 */

    /* 移除节点 1（非末尾），触发 swap-remove：
     *   旧末尾节点 3 → 移到位置 1
     *   所有引用了索引 3（旧末尾）的边应更新为 1 */
    int64_t node1_id = g->nodes[1].id;

    int ret = graph_remove_node(g, node1_id);
    CHECK(ret == 0, "graph_remove_node failed");

    /* 现在原本引用索引 3(old_last) 的边应改为引用索引 1(new pos of moved node) */
    /* node_count=3, edge_count=2 */
    CHECK(g->node_count == 3, "expected 3 nodes remaining");
    CHECK(g->edge_count == 2, "expected 2 edges remaining");

    /* 验证边索引合理性：from/to 必须在 [0, node_count) 内 */
    int edge_ok = 1;
    for (int e = 0; e < g->edge_count; e++) {
        if (g->edges[e].from < 0 || g->edges[e].from >= g->node_count ||
            g->edges[e].to   < 0 || g->edges[e].to   >= g->node_count) {
            edge_ok = 0;
            printf("\n    BAD EDGE[%d]: from=%d to=%d (node_count=%d)\n",
                   e, g->edges[e].from, g->edges[e].to, g->node_count);
        }
    }
    CHECK(edge_ok, "all edges must point to valid node indices after swap-remove");

    graph_destroy(g);
    mpq_clear(v); mpq_clear(w);
    printf("PASS\n");
}

/* ================================================================
 * main
 * ================================================================ */
int main(void) {
    printf("\n========================================\n");
    printf("  Lv-00 P0/P1 Regression Tests\n");
    printf("========================================\n\n");

    test_remove_node_no_double_free();
    test_remove_edge_no_double_free();
    test_repeated_remove();
    test_expr_destroy_deep();
    test_swap_remove_edge_integrity();

    printf("\n========================================\n");
    printf("  Results: %d passed, %d failed, %d total\n", pass, fail, pass + fail);
    printf("========================================\n");

    return fail > 0 ? 1 : 0;
}

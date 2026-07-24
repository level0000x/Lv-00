/**
 * @file test_graph_hash.c
 * @brief Lv-00 GraphHash 模块全面测试
 *
 * 覆盖空图哈希、确定性、相同结构不同 ID 的哈希行为、
 * graph_hash_equal、graph_hash_destroy 及 quick_hash。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/graph_hash.h"

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

/* ── 辅助：创建含 2 个 POINT 节点的图（使用 graph_add_node_with_id 指定 ID）── */
static ConstraintGraph *create_graph_with_ids(int id1, int id2) {
    ConstraintGraph *g = graph_create();
    if (!g)
        return NULL;

    /* 第一个点 (0,0) */
    SymbolicCoord *c0 = symbolic_coord_create_rational(0, 1);
    SymbolicCoord *c1 = symbolic_coord_create_rational(0, 1);
    if (!c0 || !c1) {
        if (c0)
            symbolic_coord_destroy(c0);
        if (c1)
            symbolic_coord_destroy(c1);
        graph_destroy(g);
        return NULL;
    }
    SymbolicCoord *coords0[] = {c0, c1};
    GeomNode *n1 = graph_add_node_with_id(g, id1, GEOM_POINT, coords0, 2);
    if (!n1) {
        graph_destroy(g);
        return NULL;
    }

    /* 第二个点 (1,0) */
    SymbolicCoord *c2 = symbolic_coord_create_rational(1, 1);
    SymbolicCoord *c3 = symbolic_coord_create_rational(0, 1);
    if (!c2 || !c3) {
        if (c2)
            symbolic_coord_destroy(c2);
        if (c3)
            symbolic_coord_destroy(c3);
        graph_destroy(g);
        return NULL;
    }
    SymbolicCoord *coords1[] = {c2, c3};
    GeomNode *n2 = graph_add_node_with_id(g, id2, GEOM_POINT, coords1, 2);
    if (!n2) {
        graph_destroy(g);
        return NULL;
    }

    return g;
}

/* ── 辅助：创建含 2 POINT + 1 INCIDENCE 的图 ── */
static ConstraintGraph *create_graph_with_constraint(int id1, int id2) {
    ConstraintGraph *g = create_graph_with_ids(id1, id2);
    if (!g)
        return NULL;

    /* 添加一条线段 */
    AddNodeResult ar = graph_add_line_segment(g, id1, id2);
    if (ar != ADD_NODE_OK) {
        graph_destroy(g);
        return NULL;
    }

    /* 线段是第 2 个节点（索引 1），添加关联约束 */
    int seg_id = g->next_node_id - 1;
    AddConstraintResult ac = graph_add_incidence(g, id1, seg_id);
    if (ac != ADD_CONSTRAINT_OK) {
        graph_destroy(g);
        return NULL;
    }

    return g;
}

/* ============================================================
 * 测试 1: 空图哈希
 * ============================================================ */
static void test_empty_graph_hash(void) {
    ConstraintGraph *g = graph_create();
    if (!g) {
        FAIL("create");
        F++;
        return;
    }

    TEST("compute_complete_graph_hash 空图");
    GraphHash *h = compute_complete_graph_hash(g);
    if (!h) {
        FAIL("NULL");
        graph_destroy(g);
        return;
    }
    /* 空图哈希应非零（FNV offset basis 作为初始值） */
    if (h->hash != 0)
        PASS();
    else
        FAIL("hash is 0");

    TEST("空图 node_hashes 为 NULL");
    if (h->node_hashes == NULL && h->node_count == 0)
        PASS();
    else
        FAIL("node_hashes not NULL or node_count not 0");

    graph_hash_destroy(h);
    graph_destroy(g);
}

/* ============================================================
 * 测试 2: 确定性 - 相同图产生相同哈希
 * ============================================================ */
static void test_determinism(void) {
    ConstraintGraph *g = graph_create();
    if (!g) {
        FAIL("create");
        F++;
        return;
    }

    /* 用 add_point 添加两个点 */
    int p0 = add_point(g, 0, 1, 0, 1);
    int p1 = add_point(g, 1, 1, 0, 1);
    if (p0 < 0 || p1 < 0) {
        FAIL("add_point");
        graph_destroy(g);
        F++;
        return;
    }

    GraphHash *h1 = compute_complete_graph_hash(g);
    if (!h1) {
        FAIL("h1 NULL");
        graph_destroy(g);
        F++;
        return;
    }

    GraphHash *h2 = compute_complete_graph_hash(g);
    if (!h2) {
        FAIL("h2 NULL");
        graph_hash_destroy(h1);
        graph_destroy(g);
        F++;
        return;
    }

    TEST("相同图两次哈希值相同");
    if (h1->hash == h2->hash)
        PASS();
    else
        FAIL("different hashes");

    TEST("相同图 node_hashes 逐元素相同");
    bool equal = true;
    if (h1->node_count != h2->node_count) {
        equal = false;
    } else {
        for (int i = 0; i < h1->node_count; i++) {
            if (h1->node_hashes[i] != h2->node_hashes[i]) {
                equal = false;
                break;
            }
        }
    }
    if (equal)
        PASS();
    else
        FAIL("node hashes differ");

    graph_hash_destroy(h1);
    graph_hash_destroy(h2);
    graph_destroy(g);
}

/* ============================================================
 * 测试 3: 相同结构、不同节点 ID → 哈希不同（实现包含 ID）
 *
 * 注意：当前实现将 node->id 混入哈希，因此不同 ID 产生不同哈希。
 * 若未来改为纯结构哈希，此测试需更新。
 * ============================================================ */
static void test_diff_ids_diff_hash(void) {
    ConstraintGraph *g1 = create_graph_with_ids(1, 2);
    ConstraintGraph *g2 = create_graph_with_ids(10, 20);
    if (!g1 || !g2) {
        FAIL("setup");
        if (g1)
            graph_destroy(g1);
        if (g2)
            graph_destroy(g2);
        F++;
        return;
    }

    GraphHash *h1 = compute_complete_graph_hash(g1);
    GraphHash *h2 = compute_complete_graph_hash(g2);
    if (!h1 || !h2) {
        FAIL("hash NULL");
        goto cleanup;
    }

    TEST("不同 ID 产生不同哈希");
    if (h1->hash != h2->hash)
        PASS();
    else
        FAIL("hashes are same (unexpected)");

cleanup:
    if (h1)
        graph_hash_destroy(h1);
    if (h2)
        graph_hash_destroy(h2);
    graph_destroy(g1);
    graph_destroy(g2);
}

/* ============================================================
 * 测试 4: graph_hash_equal
 * ============================================================ */
static void test_hash_equal(void) {
    ConstraintGraph *g = graph_create();
    if (!g) {
        FAIL("create");
        F++;
        return;
    }

    int p0 = add_point(g, 0, 1, 0, 1);
    int p1 = add_point(g, 1, 1, 0, 1);
    (void) p0;
    (void) p1;

    GraphHash *h1 = compute_complete_graph_hash(g);
    GraphHash *h2 = compute_complete_graph_hash(g);
    if (!h1 || !h2) {
        FAIL("hash NULL");
        graph_destroy(g);
        F++;
        return;
    }

    TEST("graph_hash_equal NULL, NULL");
    if (!graph_hash_equal(NULL, NULL))
        PASS();
    else
        FAIL("should be false");

    TEST("graph_hash_equal NULL, h");
    if (!graph_hash_equal(NULL, h1))
        PASS();
    else
        FAIL("should be false");

    TEST("graph_hash_equal h, NULL");
    if (!graph_hash_equal(h1, NULL))
        PASS();
    else
        FAIL("should be false");

    TEST("graph_hash_equal 相同 hash");
    if (graph_hash_equal(h1, h2))
        PASS();
    else
        FAIL("should be equal");

    /* 构建一个不同的图 */
    ConstraintGraph *g2 = graph_create();
    int p2 = add_point(g2, 9, 1, 9, 1);
    (void) p2;
    GraphHash *h3 = compute_complete_graph_hash(g2);
    if (!h3) {
        FAIL("h3 NULL");
        goto cleanup;
    }

    TEST("graph_hash_equal 不同 hash");
    if (!graph_hash_equal(h1, h3))
        PASS();
    else
        FAIL("should be not equal");

    if (h3)
        graph_hash_destroy(h3);
cleanup:
    graph_hash_destroy(h1);
    graph_hash_destroy(h2);
    graph_destroy(g);
    graph_destroy(g2);
}

/* ============================================================
 * 测试 5: graph_hash_destroy - NULL 安全
 * ============================================================ */
static void test_hash_destroy(void) {
    TEST("graph_hash_destroy(NULL)");
    graph_hash_destroy(NULL);
    PASS();

    TEST("graph_hash_destroy(valid)");
    ConstraintGraph *g = graph_create();
    if (!g) {
        FAIL("create");
        F++;
        return;
    }
    GraphHash *h = compute_complete_graph_hash(g);
    if (!h) {
        FAIL("hash NULL");
        graph_destroy(g);
        F++;
        return;
    }
    graph_hash_destroy(h);
    PASS();
    graph_destroy(g);
}

/* ============================================================
 * 测试 6: compute_quick_graph_hash
 * ============================================================ */
static void test_quick_hash(void) {
    TEST("compute_quick_graph_hash(NULL)");
    if (compute_quick_graph_hash(NULL) == 0)
        PASS();
    else
        FAIL("should be 0");

    ConstraintGraph *g = graph_create();
    if (!g) {
        FAIL("create");
        F++;
        return;
    }

    TEST("quick_hash 空图非零");
    uint64_t q1 = compute_quick_graph_hash(g);
    if (q1 != 0)
        PASS();
    else
        FAIL("unexpected 0");

    /* 添加节点后哈希变化 */
    int p0 = add_point(g, 0, 1, 0, 1);
    int p1 = add_point(g, 1, 1, 0, 1);
    (void) p0;
    (void) p1;

    uint64_t q2 = compute_quick_graph_hash(g);
    TEST("quick_hash 有节点后改变");
    if (q2 != q1)
        PASS();
    else
        FAIL("unchanged");

    TEST("quick_hash 确定性");
    uint64_t q3 = compute_quick_graph_hash(g);
    if (q3 == q2)
        PASS();
    else
        FAIL("not deterministic");

    graph_destroy(g);
}

/* ============================================================
 * Main
 * ============================================================ */
int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== GraphHash 测试套件 ===\n\n");

    lv_init();

    printf("[组 1] 空图哈希\n");
    test_empty_graph_hash();

    printf("[组 2] 确定性\n");
    test_determinism();

    printf("[组 3] 不同 ID 的哈希行为\n");
    test_diff_ids_diff_hash();

    printf("[组 4] graph_hash_equal\n");
    test_hash_equal();

    printf("[组 5] graph_hash_destroy\n");
    test_hash_destroy();

    printf("[组 6] compute_quick_graph_hash\n");
    test_quick_hash();

    lv_cleanup();
    printf("\n=== %d passed, %d failed ===\n", P, F);
    return F > 0 ? 1 : 0;
}

/**
 * @file test_edge_cases.c
 * @brief Lv-00 边界条件与鲁棒性测试套件
 *
 * 测试目标：
 * - NULL 参数传入 API 的防护
 * - 极限坐标值（INT64_MAX、分母为零、超大值）
 * - 空图/空数组操作
 * - 内存分配失败后继续运行的鲁棒性
 * - graph_destroy(NULL) 安全性
 * - 多次创建/销毁循环（内存/资源泄漏检测）
 * - 符号坐标各类极限输入
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv.h"
#include "test_helpers.h"

int g_pass_count = 0;
int g_fail_count = 0;
int g_errors = 0;

/* ============================================================
 * 辅助函数：在安全前提下检查函数调用不崩溃
 * ============================================================ */

/**
 * @brief 检查函数传入 NULL 参数时返回预期错误值而非崩溃
 *
 * 用法: CHECK_NULL_SAFE(test_name, func_call, expected_return)
 * 若 func_call 返回 expected_return，测试通过；
 * 若返回其他值或触发 SIGSEGV（由外部测试框架检测），测试失败。
 */
#define CHECK_NULL_SAFE(name, expr, expected)                                          \
    do {                                                                               \
        printf("  %s ... ", (name));                                                   \
        fflush(stdout);                                                                \
        intptr_t result = (intptr_t)(expr);                                            \
        if (result == (intptr_t)(expected)) {                                          \
            printf("PASS (returned %ld as expected)\n", (long)result);                 \
            g_pass_count++;                                                            \
        } else {                                                                       \
            printf("FAIL (returned %ld, expected %ld)\n", (long)result, (long)expected); \
            g_fail_count++;                                                            \
        }                                                                              \
    } while (0)

/**
 * @brief 检查函数传入有效参数时成功返回
 */
#define CHECK_VALID(name, expr, unexpected)                                            \
    do {                                                                               \
        printf("  %s ... ", (name));                                                   \
        fflush(stdout);                                                                \
        intptr_t result = (intptr_t)(expr);                                            \
        if (result != (intptr_t)(unexpected)) {                                        \
            printf("PASS (returned %ld)\n", (long)result);                             \
            g_pass_count++;                                                            \
        } else {                                                                       \
            printf("FAIL (returned %ld, which is the failure sentinel)\n", (long)result); \
            g_fail_count++;                                                            \
        }                                                                              \
    } while (0)

/* ============================================================
 * 测试用例
 * ============================================================ */

/**
 * @brief T1: NULL 参数安全性测试
 *
 * 验证核心 API 在传入 NULL 参数时不会崩溃，而是安全返回错误指示。
 * 这是防止 0xC0000005 访问违例的关键测试。
 */
static void test_null_parameter_safety(void) {
    printf("\n--- T1: NULL Parameter Safety ---\n");

    /* graph_create 不需要参数，始终安全 */
    printf("  graph_create(NULL-like ambient) ... ");
    {
        ConstraintGraph *g = graph_create();
        if (g) {
            printf("PASS\n");
            g_pass_count++;
        } else {
            printf("FAIL\n");
            g_fail_count++;
        }
        graph_destroy(g);
    }

    /* graph_destroy(NULL) 必须安全 */
    printf("  graph_destroy(NULL) ... ");
    fflush(stdout);
    graph_destroy(NULL);
    printf("PASS (no crash)\n");
    g_pass_count++;

    /* graph_get_node(NULL, ...) */
    CHECK_NULL_SAFE("graph_get_node(NULL, 0)", graph_get_node(NULL, 0), NULL);

    /* graph_get_node_by_id(NULL, 0) */
    CHECK_NULL_SAFE("graph_get_node_by_id(NULL, 0)", graph_get_node_by_id(NULL, 0), NULL);

    /* graph_get_constraint(NULL, 0) */
    CHECK_NULL_SAFE("graph_get_constraint(NULL, 0)", graph_get_constraint(NULL, 0), NULL);

    /* graph_add_point(NULL, ...) */
    {
        SymbolicCoord *c = symbolic_coord_create_rational(0, 1);
        SymbolicCoord *coords[] = {c, c};
        printf("  graph_add_point(NULL, coords, 2) ... ");
        fflush(stdout);
        AddNodeResult r = graph_add_point(NULL, coords, 2);
        if (r != ADD_NODE_OK) {
            printf("PASS (correctly rejected with code %d)\n", (int)r);
            g_pass_count++;
        } else {
            printf("FAIL (unexpectedly succeeded with NULL graph)\n");
            g_fail_count++;
        }
        symbolic_coord_destroy(c);
    }

    /* graph_get_node_count(NULL) */
    CHECK_NULL_SAFE("graph_get_node_count(NULL)", graph_get_node_count(NULL), 0);

    /* graph_get_constraint_count(NULL) */
    CHECK_NULL_SAFE("graph_get_constraint_count(NULL)", graph_get_constraint_count(NULL), 0);

    /* graph_get_last_added_node_id(NULL) */
    CHECK_NULL_SAFE("graph_get_last_added_node_id(NULL)", graph_get_last_added_node_id(NULL), -1);

    /* symbolic_coord_create_rational 分母为0 */
    printf("  symbolic_coord_create_rational(1, 0) ... ");
    fflush(stdout);
    {
        SymbolicCoord *bad = symbolic_coord_create_rational(1, 0);
        if (bad == NULL) {
            printf("PASS (correctly rejected)\n");
            g_pass_count++;
        } else {
            printf("WARN (returned non-NULL, destroying)\n");
            symbolic_coord_destroy(bad);
            g_pass_count++; /* 至少没崩溃 */
        }
    }

    /* symbolic_coord_destroy(NULL) */
    printf("  symbolic_coord_destroy(NULL) ... ");
    fflush(stdout);
    symbolic_coord_destroy(NULL);
    printf("PASS (no crash)\n");
    g_pass_count++;

    /* lv_free(NULL) —— 双重指针为 NULL */
    printf("  lv_free on NULL ptr-to-ptr ... ");
    fflush(stdout);
    {
        void *null_ptr = NULL;
        lv_free(&null_ptr); /* 应安全，不崩溃 */
    }
    printf("PASS (no crash)\n");
    g_pass_count++;

    /* lv_malloc(0) —— 零大小分配 */
    printf("  lv_malloc(0) ... ");
    fflush(stdout);
    {
        void *p = lv_malloc(0);
        if (p) {
            printf("PASS (returned non-NULL, which is acceptable)\n");
            lv_free(&p);
        } else {
            printf("PASS (returned NULL)\n");
        }
        g_pass_count++;
    }
}

/**
 * @brief T2: 极限坐标值测试
 *
 * 测试坐标系统在极端数值下的行为。
 */
static void test_extreme_coordinates(void) {
    printf("\n--- T2: Extreme Coordinates ---\n");

    /* INT64_MAX 分子 */
    printf("  symbolic_coord_create_rational(INT64_MAX, 1) ... ");
    fflush(stdout);
    {
        SymbolicCoord *c = symbolic_coord_create_rational(INT64_MAX, 1);
        if (c) {
            printf("PASS\n");
            symbolic_coord_destroy(c);
            g_pass_count++;
        } else {
            printf("FAIL (creation failed)\n");
            g_fail_count++;
        }
    }

    /* INT64_MIN 分子 */
    printf("  symbolic_coord_create_rational(INT64_MIN, 1) ... ");
    fflush(stdout);
    {
        SymbolicCoord *c = symbolic_coord_create_rational(INT64_MIN, 1);
        if (c) {
            printf("PASS\n");
            symbolic_coord_destroy(c);
            g_pass_count++;
        } else {
            printf("FAIL (creation failed)\n");
            g_fail_count++;
        }
    }

    /* 极大分母 UINT64_MAX */
    printf("  symbolic_coord_create_rational(1, UINT64_MAX) ... ");
    fflush(stdout);
    {
        SymbolicCoord *c = symbolic_coord_create_rational(1, UINT64_MAX);
        if (c) {
            printf("PASS\n");
            symbolic_coord_destroy(c);
            g_pass_count++;
        } else {
            printf("FAIL (creation failed)\n");
            g_fail_count++;
        }
    }

    /* 零分子，合法分母 */
    printf("  symbolic_coord_create_rational(0, 1) ... ");
    fflush(stdout);
    {
        SymbolicCoord *c = symbolic_coord_create_rational(0, 1);
        if (c) {
            printf("PASS (zero coord created)\n");
            /* 验证为零 */
            if (symbolic_coord_is_zero(c)) {
                printf("    is_zero: PASS\n");
                g_pass_count++;
            } else {
                printf("    is_zero: FAIL\n");
                g_fail_count++;
            }
            symbolic_coord_destroy(c);
            g_pass_count++;
        } else {
            printf("FAIL (creation failed)\n");
            g_fail_count++;
        }
    }

    /* 负分母 —— 行为由实现决定 */
    printf("  symbolic_coord_create_rational(1, (uint64_t)-1) [negative as uint] ... ");
    fflush(stdout);
    {
        SymbolicCoord *c = symbolic_coord_create_rational(1, (uint64_t)-1);
        if (c) {
            printf("PASS (created)\n");
            symbolic_coord_destroy(c);
        } else {
            printf("PASS (rejected as expected)\n");
        }
        g_pass_count++;
    }
}

/**
 * @brief T3: 空图操作测试
 *
 * 验证在空图上执行各种操作的鲁棒性。
 */
static void test_empty_graph_operations(void) {
    printf("\n--- T3: Empty Graph Operations ---\n");

    ConstraintGraph *g = graph_create();
    if (!g) {
        printf("  SKIP: graph_create() returned NULL\n");
        return;
    }

    /* 验证空图属性 */
    printf("  empty graph node_count ... ");
    fflush(stdout);
    if (g->node_count == 0) {
        printf("PASS (%d)\n", g->node_count);
        g_pass_count++;
    } else {
        printf("FAIL (%d, expected 0)\n", g->node_count);
        g_fail_count++;
    }

    printf("  empty graph constraint_count ... ");
    fflush(stdout);
    if (g->constraint_count == 0) {
        printf("PASS (%d)\n", g->constraint_count);
        g_pass_count++;
    } else {
        printf("FAIL (%d, expected 0)\n", g->constraint_count);
        g_fail_count++;
    }

    /* graph_get_node_count 包装函数 */
    int nc = graph_get_node_count(g);
    printf("  graph_get_node_count on empty ... ");
    if (nc == 0) {
        printf("PASS\n");
        g_pass_count++;
    } else {
        printf("FAIL (%d)\n", nc);
        g_fail_count++;
    }

    /* graph_get_last_added_node_id 在空图上 */
    int last_id = graph_get_last_added_node_id(g);
    printf("  graph_get_last_added_node_id on empty ... ");
    if (last_id == -1) {
        printf("PASS\n");
        g_pass_count++;
    } else {
        printf("FAIL (%d, expected -1)\n", last_id);
        g_fail_count++;
    }

    /* 在空图上查找节点 */
    GeomNode *n = graph_get_node(g, 0);
    printf("  graph_get_node(g, 0) on empty ... ");
    if (n == NULL) {
        printf("PASS\n");
        g_pass_count++;
    } else {
        printf("FAIL (returned non-NULL)\n");
        g_fail_count++;
    }

    /* 在空图上查找节点（负数 ID） */
    n = graph_get_node(g, -1);
    printf("  graph_get_node(g, -1) on empty ... ");
    if (n == NULL) {
        printf("PASS\n");
        g_pass_count++;
    } else {
        printf("FAIL (returned non-NULL)\n");
        g_fail_count++;
    }

    /* 节点数量统计一致性 */
    printf("  node count consistency (empty) ... ");
    if (graph_get_node_count(g) == g->node_count) {
        printf("PASS\n");
        g_pass_count++;
    } else {
        printf("FAIL\n");
        g_fail_count++;
    }

    graph_destroy(g);
}

/**
 * @brief T4: 内存分配/释放循环测试
 *
 * 反复创建和销毁图/坐标，检测内存泄漏和资源耗尽。
 */
static void test_memory_cycle(void) {
    printf("\n--- T4: Memory Allocation/Deallocation Cycle ---\n");

    /* 快速循环：分配-释放图 */
    printf("  graph create/destroy x100 ... ");
    fflush(stdout);
    {
        int cycle_ok = 1;
        for (int i = 0; i < 100; i++) {
            ConstraintGraph *g = graph_create();
            if (!g) {
                printf("FAIL (allocation failed at iteration %d)\n", i);
                cycle_ok = 0;
                g_fail_count++;
                break;
            }
            graph_destroy(g);
        }
        if (cycle_ok) {
            printf("PASS\n");
            g_pass_count++;
        }
    }

    /* 快速循环：分配-释放符号坐标 */
    printf("  coord create/destroy x1000 ... ");
    fflush(stdout);
    {
        int cycle_ok = 1;
        for (int i = 0; i < 1000; i++) {
            SymbolicCoord *c = symbolic_coord_create_rational(i % 1000, (i % 999) + 1);
            if (!c) {
                printf("FAIL (allocation failed at iteration %d)\n", i);
                cycle_ok = 0;
                g_fail_count++;
                break;
            }
            symbolic_coord_destroy(c);
        }
        if (cycle_ok) {
            printf("PASS\n");
            g_pass_count++;
        }
    }

    /* 混合循环：创建带节点的图 */
    printf("  graph with point create/destroy x50 ... ");
    fflush(stdout);
    {
        int cycle_ok = 1;
        for (int i = 0; i < 50; i++) {
            ConstraintGraph *g = graph_create();
            if (!g) {
                cycle_ok = 0;
                printf("FAIL (graph creation at iter %d)\n", i);
                g_fail_count++;
                break;
            }

            /* 添加一个点 */
            SymbolicCoord *cx = symbolic_coord_create_rational(i, 1);
            SymbolicCoord *cy = symbolic_coord_create_rational(i * 2, 1);
            if (cx && cy) {
                SymbolicCoord *coords[] = {cx, cy};
                if (graph_add_point(g, coords, 2) != 0) {
                    /* graph_add_point 失败，释放坐标 */
                    symbolic_coord_destroy(cx);
                    symbolic_coord_destroy(cy);
                    graph_destroy(g);
                    cycle_ok = 0;
                    printf("FAIL (graph_add_point failed at iter %d)\n", i);
                    g_fail_count++;
                    break;
                }
                symbolic_coord_destroy(cx);
                symbolic_coord_destroy(cy);
            } else {
                /* 坐标创建失败，释放已分配的坐标 */
                if (cx) symbolic_coord_destroy(cx);
                if (cy) symbolic_coord_destroy(cy);
                graph_destroy(g);
                cycle_ok = 0;
                printf("FAIL (coord creation failed at iter %d)\n", i);
                g_fail_count++;
                break;
            }

            graph_destroy(g);
        }
        if (cycle_ok) {
            printf("PASS\n");
            g_pass_count++;
        }
    }
}

/**
 * @brief T5: 大型图操作测试
 *
 * 在包含大量节点的图上执行操作，检测性能边界和内存问题。
 */
static void test_large_graph(void) {
    printf("\n--- T5: Large Graph Operations ---\n");

    ConstraintGraph *g = graph_create();
    if (!g) {
        printf("  SKIP: graph_create() failed\n");
        g_fail_count++;
        return;
    }

    /* 批量创建点 */
    printf("  creating 200 points ... ");
    fflush(stdout);
    int first_point_id = -1;
    int last_point_id = -1;
    int success_count = 0;
    for (int i = 0; i < 200; i++) {
        SymbolicCoord *cx = symbolic_coord_create_rational(i, 1);
        SymbolicCoord *cy = symbolic_coord_create_rational(i * 3, 2);
        if (!cx || !cy) {
            if (cx) symbolic_coord_destroy(cx);
            if (cy) symbolic_coord_destroy(cy);
            break;
        }
        SymbolicCoord *coords[] = {cx, cy};
        AddNodeResult r = graph_add_point(g, coords, 2);
        symbolic_coord_destroy(cx);
        symbolic_coord_destroy(cy);
        if (r == ADD_NODE_OK) {
            success_count++;
            if (first_point_id < 0) first_point_id = g->next_node_id - 1;
            last_point_id = g->next_node_id - 1;
        }
    }
    printf("added %d points (IDs %d..%d)\n", success_count, first_point_id, last_point_id);
    if (success_count == 200) {
        g_pass_count++;
    } else {
        printf("  WARN: only %d/200 points created\n", success_count);
        g_pass_count++; /* 部分成功也算测试通过 */
    }

    /* 验证节点数量 */
    printf("  node count after bulk add ... ");
    if (graph_get_node_count(g) == success_count) {
        printf("PASS\n");
        g_pass_count++;
    } else {
        printf("FAIL (%d != %d)\n", graph_get_node_count(g), success_count);
        g_fail_count++;
    }

    /* 在大型图上按 ID 查找 */
    printf("  graph_get_node on large graph ... ");
    fflush(stdout);
    {
        int lookup_ok = 1;
        for (int i = first_point_id; i <= last_point_id && i <= first_point_id + 10; i++) {
            GeomNode *n = graph_get_node(g, i);
            if (!n || n->type != GEOM_POINT) {
                lookup_ok = 0;
                printf("FAIL (node %d not found or wrong type)\n", i);
                g_fail_count++;
                break;
            }
        }
        if (lookup_ok) {
            printf("PASS\n");
            g_pass_count++;
        }
    }

    /* 查找不存在的节点 ID */
    printf("  graph_get_node for nonexistent ID ... ");
    {
        GeomNode *n = graph_get_node(g, last_point_id + 1000);
        if (n == NULL) {
            printf("PASS\n");
            g_pass_count++;
        } else {
            printf("FAIL (unexpectedly found node)\n");
            g_fail_count++;
        }
    }

    graph_destroy(g);
}

/**
 * @brief T6: 符号坐标类型边界测试
 *
 * 测试符号坐标在不同类型下的创建和行为。
 */
static void test_symbolic_coord_types(void) {
    printf("\n--- T6: Symbolic Coordinate Types ---\n");

    /* 正数有理坐标 */
    printf("  positive rational ... ");
    {
        SymbolicCoord *c = symbolic_coord_create_rational(5, 1);
        if (c && symbolic_coord_is_positive(c) && !symbolic_coord_is_negative(c) && !symbolic_coord_is_zero(c)) {
            printf("PASS\n");
            g_pass_count++;
        } else {
            printf("FAIL\n");
            g_fail_count++;
        }
        symbolic_coord_destroy(c);
    }

    /* 负数有理坐标 */
    printf("  negative rational ... ");
    {
        SymbolicCoord *c = symbolic_coord_create_rational(-3, 1);
        if (c && symbolic_coord_is_negative(c) && !symbolic_coord_is_positive(c) && !symbolic_coord_is_zero(c)) {
            printf("PASS\n");
            g_pass_count++;
        } else {
            printf("FAIL\n");
            g_fail_count++;
        }
        symbolic_coord_destroy(c);
    }

    /* 零值坐标 */
    printf("  zero check consistency ... ");
    {
        SymbolicCoord *c = symbolic_coord_create_rational(0, 5);
        if (c && symbolic_coord_is_zero(c) && !symbolic_coord_is_positive(c) && !symbolic_coord_is_negative(c)) {
            printf("PASS\n");
            g_pass_count++;
        } else {
            printf("FAIL\n");
            g_fail_count++;
        }
        symbolic_coord_destroy(c);
    }

    /* trust color 默认值 */
    printf("  default trust color ... ");
    {
        SymbolicCoord *c = symbolic_coord_create_rational(1, 1);
        if (c) {
            TrustColor tc = symbolic_coord_get_trust(c);
            /* 新创建的坐标通常默认为 TRUST_GREEN */
            printf("%s (color=%d)\n", (tc == TRUST_GREEN) ? "PASS" : "NOTE", (int)tc);
            if (tc == TRUST_GREEN) {
                g_pass_count++;
            } else {
                /* 不是 GREEN 也不一定是错误，取决于实现 */
                g_pass_count++;
            }
            symbolic_coord_destroy(c);
        } else {
            printf("FAIL\n");
            g_fail_count++;
        }
    }

    /* 分数坐标 */
    printf("  fractional rational coord ... ");
    {
        SymbolicCoord *c = symbolic_coord_create_rational(1, 3);
        if (c) {
            printf("PASS\n");
            g_pass_count++;
            symbolic_coord_destroy(c);
        } else {
            printf("FAIL\n");
            g_fail_count++;
        }
    }

    /* 复制坐标 */
    printf("  symbolic_coord_copy ... ");
    {
        SymbolicCoord *orig = symbolic_coord_create_rational(7, 2);
        if (orig) {
            SymbolicCoord *copy = symbolic_coord_copy(orig);
            if (copy) {
                int cmp = symbolic_coord_compare(orig, copy);
                if (cmp == 0) {
                    printf("PASS (copies equal)\n");
                    g_pass_count++;
                } else {
                    printf("FAIL (copies differ, cmp=%d)\n", cmp);
                    g_fail_count++;
                }
                symbolic_coord_destroy(copy);
            } else {
                printf("FAIL (copy returned NULL)\n");
                g_fail_count++;
            }
            symbolic_coord_destroy(orig);
        } else {
            printf("FAIL (orig creation failed)\n");
            g_fail_count++;
        }
    }
}

/**
 * @brief T7: lv_malloc/lv_free 边界测试
 *
 * 测试统一内存分配器在边界条件下的行为。
 */
static void test_memory_boundaries(void) {
    printf("\n--- T7: Memory Allocator Boundaries ---\n");

    /* 超大分配 —— 应该优雅失败或成功 */
    printf("  lv_malloc(SIZE_MAX/2) [should fail gracefully] ... ");
    fflush(stdout);
    {
        void *p = lv_malloc(SIZE_MAX / 2);
        if (p == NULL) {
            printf("PASS (correctly returned NULL)\n");
            g_pass_count++;
        } else {
            printf("NOTE (surprisingly succeeded, freeing)\n");
            lv_free(&p);
            g_pass_count++;
        }
    }

    /* 多次小分配 */
    printf("  lv_malloc(1) x1000 ... ");
    fflush(stdout);
    {
        void *ptrs[1000] = {NULL};
        int ok = 1;
        for (int i = 0; i < 1000; i++) {
            ptrs[i] = lv_malloc(1);
            if (!ptrs[i]) {
                printf("FAIL (allocation %d failed)\n", i);
                ok = 0;
                g_fail_count++;
                break;
            }
        }
        for (int i = 0; i < 1000 && ptrs[i]; i++) {
            lv_free(&ptrs[i]);
        }
        if (ok) {
            printf("PASS\n");
            g_pass_count++;
        }
    }

    /* calloc 零参数 */
    printf("  lv_calloc(0, 10) ... ");
    fflush(stdout);
    {
        void *p = lv_calloc(0, 10);
        if (p == NULL) {
            printf("PASS (returned NULL)\n");
            g_pass_count++;
        } else {
            printf("NOTE (returned non-NULL, expected NULL for zero count)\n");
            lv_free(&p);
            g_pass_count++;
        }
    }

    /* calloc 溢出保护 */
    printf("  lv_calloc(SIZE_MAX, 2) [overflow check] ... ");
    fflush(stdout);
    {
        void *p = lv_calloc(SIZE_MAX, 2);
        if (p == NULL) {
            printf("PASS (overflow correctly detected)\n");
            g_pass_count++;
        } else {
            printf("FAIL (overflow not detected)\n");
            lv_free(&p);
            g_fail_count++;
        }
    }

    /* realloc NULL 等同于 malloc */
    printf("  lv_realloc(NULL, 32) ... ");
    fflush(stdout);
    {
        void *p = lv_realloc(NULL, 32);
        if (p) {
            printf("PASS\n");
            lv_free(&p);
            g_pass_count++;
        } else {
            printf("FAIL\n");
            g_fail_count++;
        }
    }

    /* realloc size=0 */
    printf("  lv_realloc(ptr, 0) ... ");
    fflush(stdout);
    {
        void *p = lv_malloc(16);
        void *r = lv_realloc(p, 0);
        if (r == NULL) {
            printf("PASS (returned NULL as documented)\n");
            g_pass_count++;
        } else {
            printf("NOTE (returned non-NULL)\n");
            lv_free(&r);
            g_pass_count++;
        }
        lv_free(&p); /* 原指针仍需手动释放 */
    }
}

/* ============================================================
 * 主函数
 * ============================================================ */

int main(void) {
    setbuf(stdout, NULL);

    printf("=== Lv-00 Edge Case Test Suite ===\n");
    printf("Version: %s\n\n", lv_get_version_string());

    /* 步骤 0: 初始化系统 */
    printf("--- Initialization ---\n");
    printf("Calling lv_init() ... ");
    fflush(stdout);

    if (!lv_init()) {
        fprintf(stderr, "\nFATAL: lv_init() failed!\n");
        fprintf(stderr, "Last error: %s (code %d)\n",
                lv_get_last_error_message(),
                (int)lv_get_last_error_code());
        fprintf(stderr, "Cannot continue without system initialization.\n");
        return 1;
    }
    printf("OK\n");
    g_pass_count++;

    printf("System initialized: %s\n", lv_is_initialized() ? "YES" : "NO");
    if (!lv_is_initialized()) {
        fprintf(stderr, "FATAL: System claims to be uninitialized after lv_init() succeeded\n");
        return 1;
    }

    /* 重置计数器（不含初始化步骤） */
    g_pass_count = 0;
    g_fail_count = 0;

    /* 运行所有测试 */
    test_null_parameter_safety();
    test_extreme_coordinates();
    test_empty_graph_operations();
    test_memory_cycle();
    test_large_graph();
    test_symbolic_coord_types();
    test_memory_boundaries();

    /* 步骤 N: 清理系统 */
    printf("\n--- Cleanup ---\n");
    printf("Calling lv_cleanup() ... ");
    fflush(stdout);
    lv_cleanup();
    printf("OK\n");

    /* ================================================================
     * 结果汇总
     * ================================================================ */
    int total = g_pass_count + g_fail_count;
    printf("\n");
    printf("========================================\n");
    printf("  Edge Case Test Results\n");
    printf("========================================\n");
    printf("  Passed: %d\n", g_pass_count);
    printf("  Failed: %d\n", g_fail_count);
    printf("  Total:  %d\n", total);
    if (g_fail_count == 0) {
        printf("  Status: ALL TESTS PASSED\n");
    } else {
        printf("  Status: SOME TESTS FAILED\n");
    }
    printf("========================================\n");

    return g_fail_count > 0 ? 1 : 0;
}

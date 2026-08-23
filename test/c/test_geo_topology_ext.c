/**
 * @file test_geo_topology_ext.c
 * @brief 拓扑旧版兼容契约测试（批次 C-㊺续34：geo_topology.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（2 个）：
 *   lv_euler_characteristic / lv_is_simplicial_complex
 *
 * 契约要点（与 geo_topology.c 核对）：
 *   - euler_characteristic：V - E + F；负数计数 / 2E < 3F / 闭曲面 χ>2 → -1。
 *   - is_simplicial_complex：faces NULL 或 n_faces 0 → 0；dim 非 1..3 → 0；
 *     面内重复顶点 / 负顶点 → 0；一致 → 1。
 *
 * @author Lv-00 Project
 */

#include <stdio.h>

#include "lv/geo_topology.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：欧拉示性数 ============== */

static void test_euler_characteristic(void) {
    /* 四面体：V-E+F = 4-6+4 = 2（闭曲面，2E=12=3F） */
    TEST_ASSERT_EQ(lv_euler_characteristic(4, 6, 4), 2);

    /* 立方体：8-12+6 = 2 */
    TEST_ASSERT_EQ(lv_euler_characteristic(8, 12, 6), 2);

    /* 带边界的曲面：2E > 3F，χ = 5-7+3 = 1 */
    TEST_ASSERT_EQ(lv_euler_characteristic(5, 7, 3), 1);

    /* 负数计数：-1 */
    TEST_ASSERT_EQ(lv_euler_characteristic(-1, 0, 0), -1);

    /* 2E < 3F：-1 */
    TEST_ASSERT_EQ(lv_euler_characteristic(0, 1, 1), -1);

    /* 闭曲面 χ > 2：-1（5-6+4=3，2E=12=3F 闭） */
    TEST_ASSERT_EQ(lv_euler_characteristic(5, 6, 4), -1);
}

/* ============== 测试：单纯复形 ============== */

static void test_is_simplicial_complex(void) {
    /* 两个共享边 0-1 的三角形（dim 2）：一致 */
    int faces2[] = {0, 1, 2, 0, 1, 3};
    TEST_ASSERT_EQ(lv_is_simplicial_complex(faces2, 2, 2), 1);

    /* 边列表（dim 1）：{0-1, 1-2} */
    int edges[] = {0, 1, 1, 2};
    TEST_ASSERT_EQ(lv_is_simplicial_complex(edges, 2, 1), 1);

    /* 退化：面内重复顶点（0,0,1）→ 0 */
    int degenerate[] = {0, 0, 1};
    TEST_ASSERT_EQ(lv_is_simplicial_complex(degenerate, 1, 2), 0);

    /* 负顶点 → 0 */
    int neg[] = {-1, 0, 1};
    TEST_ASSERT_EQ(lv_is_simplicial_complex(neg, 1, 2), 0);

    /* NULL / 空 → 0 */
    TEST_ASSERT_EQ(lv_is_simplicial_complex(NULL, 2, 2), 0);
    TEST_ASSERT_EQ(lv_is_simplicial_complex(faces2, 0, 2), 0);

    /* dim 越界 → 0 */
    TEST_ASSERT_EQ(lv_is_simplicial_complex(faces2, 2, 0), 0);
    TEST_ASSERT_EQ(lv_is_simplicial_complex(faces2, 2, 4), 0);

    /* 四面体列表（dim 3）：单面 0-1-2-3 */
    int tetra[] = {0, 1, 2, 3};
    TEST_ASSERT_EQ(lv_is_simplicial_complex(tetra, 1, 3), 1);
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("GeoTopologyExt")

    printf("\n--- geo_topology (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_euler_characteristic);
    TEST_MAIN_RUN(test_is_simplicial_complex);

TEST_MAIN_END()

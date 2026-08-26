/**
 * @file test_geo_halfedge_mesh_ext.c
 * @brief Halfedge 网格契约测试（批次 C-㊺续14：geo_halfedge_mesh.h 30 个零覆盖 API）
 *
 * 覆盖零覆盖 API：
 *   构造族：create_default（inline）/ default_config / compute_normals /
 *     compute_curvature
 *   计数族：edge_count / face_count / vertex_count
 *   有效性族：is_valid / validate
 *   面查询族：edge_halfedge / face_halfedge / halfedge_angle /
 *     halfedge_corner_angle / halfedge_next / vertex_angle /
 *     vertex_curvature / vertex_normal / vertex_out_halfedge
 *   迭代器族：face_iter_begin/get/valid/next / vertex_iter_begin/get/
 *     valid/next / mesh_vertex_iter_begin/next /
 *     mesh_vertex_out_iter_begin/next / mesh_face_iter_begin/next
 *   Legacy 族：halfedge_mesh_add_vertex / halfedge_mesh_add_face
 *   面构造族：add_face（通用多边形）
 *
 * 修复点（M5 补齐）：
 *   - compute_normals / compute_curvature / edge_count / face_count /
 *     vertex_count / is_valid / halfedge_mesh_add_vertex /
 *     halfedge_mesh_add_face 原无实现，按头契约补齐。
 *   - add_face 通用多边形构造（3..16 边形，扇形三角化）。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/geo_halfedge_mesh.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：构造与计数 ============== */

static void test_he_construct_api(void) {
    /* default_config */
    lvHeMeshConfig cfg = lv_he_mesh_default_config();
    TEST_ASSERT(cfg.initial_capacity > 0, "初始容量为正");
    TEST_ASSERT(cfg.max_faces_per_edge >= 1, "每边面数上限");
    TEST_ASSERT(cfg.maintain_normals, "默认维护法线");

    /* create_default（inline，等价 create(NULL)） */
    lvHeMesh *m = lv_he_mesh_create_default();
    TEST_ASSERT_NOT_NULL(m);
    TEST_ASSERT_EQ(m->config.initial_capacity, cfg.initial_capacity);

    /* 空网格计数为 0 */
    TEST_ASSERT_EQ(lv_he_mesh_vertex_count(m), 0);
    TEST_ASSERT_EQ(lv_he_mesh_edge_count(m), 0);
    TEST_ASSERT_EQ(lv_he_mesh_face_count(m), 0);

    /* NULL 契约 */
    TEST_ASSERT_EQ(lv_he_mesh_vertex_count(NULL), 0);
    TEST_ASSERT_EQ(lv_he_mesh_edge_count(NULL), 0);
    TEST_ASSERT_EQ(lv_he_mesh_face_count(NULL), 0);

    /* 加顶点后计数 */
    lvVertex v0 = lv_he_mesh_add_vertex(m, 0, 0, 0);
    lvVertex v1 = lv_he_mesh_add_vertex(m, 1, 0, 0);
    lvVertex v2 = lv_he_mesh_add_vertex(m, 0, 1, 0);
    TEST_ASSERT(v0 >= 0 && v1 >= 0 && v2 >= 0, "顶点索引有效");
    TEST_ASSERT_EQ(lv_he_mesh_vertex_count(m), 3);

    lv_he_mesh_destroy(m);
    printf("  test_he_construct_api: PASSED\n");
}

/* ============== 测试：有效性 ============== */

static void test_he_valid_api(void) {
    lvHeMesh *m = lv_he_mesh_create(NULL);
    TEST_ASSERT_NOT_NULL(m);

    /* 空网格验证 */
    TEST_ASSERT(lv_he_mesh_validate(m), "空网格有效");
    TEST_ASSERT(lv_he_mesh_is_valid(m), "is_valid 委托 validate");

    /* 加顶点与三角面后仍有效 */
    lvVertex v0 = lv_he_mesh_add_vertex(m, 0, 0, 0);
    lvVertex v1 = lv_he_mesh_add_vertex(m, 1, 0, 0);
    lvVertex v2 = lv_he_mesh_add_vertex(m, 0, 1, 0);
    lvFace f = lv_he_mesh_add_face_triangle(m, v0, v1, v2);
    TEST_ASSERT(f >= 0, "三角面创建成功");
    TEST_ASSERT(lv_he_mesh_validate(m), "含面网格有效");
    TEST_ASSERT(lv_he_mesh_is_valid(m), "is_valid 一致");

    /* NULL 契约 */
    TEST_ASSERT(!lv_he_mesh_validate(NULL), "NULL 验证失败");
    TEST_ASSERT(!lv_he_mesh_is_valid(NULL), "NULL is_valid 失败");

    lv_he_mesh_destroy(m);
    printf("  test_he_valid_api: PASSED\n");
}

/* ============== 测试：面查询 ============== */

static void test_he_face_query_api(void) {
    lvHeMesh *m = lv_he_mesh_create(NULL);
    /* 单位正方形对角线三角化：两个三角形 */
    lvVertex v0 = lv_he_mesh_add_vertex(m, 0, 0, 0);
    lvVertex v1 = lv_he_mesh_add_vertex(m, 1, 0, 0);
    lvVertex v2 = lv_he_mesh_add_vertex(m, 1, 1, 0);
    lvVertex v3 = lv_he_mesh_add_vertex(m, 0, 1, 0);
    lvFace f0 = lv_he_mesh_add_face_triangle(m, v0, v1, v2);
    lvFace f1 = lv_he_mesh_add_face_triangle(m, v0, v2, v3);
    TEST_ASSERT(f0 >= 0 && f1 >= 0, "两三角面创建成功");

    /* 面计数 */
    TEST_ASSERT_EQ(lv_he_mesh_face_count(m), 2);

    /* face_halfedge：返回面首半边 */
    lvHalfedge he = lv_he_mesh_face_halfedge(m, f0);
    TEST_ASSERT(he >= 0, "面半边有效");
    TEST_ASSERT_EQ(lv_he_mesh_halfedge_face(m, he), f0);

    /* halfedge_next 环绕：3 条边回到起点 */
    lvHalfedge he2 = lv_he_mesh_halfedge_next(m, he);
    lvHalfedge he3 = lv_he_mesh_halfedge_next(m, he2);
    lvHalfedge he4 = lv_he_mesh_halfedge_next(m, he3);
    TEST_ASSERT_EQ(he4, he);

    /* edge_halfedge：边存在 */
    lvEdge e = lv_he_mesh_find_edge(m, v0, v1);
    TEST_ASSERT(e >= 0, "边找到");
    lvHalfedge edge_he = lv_he_mesh_edge_halfedge(m, e);
    TEST_ASSERT(edge_he >= 0, "边半边有效");

    /* face_area：两三角形各 0.5 */
    TEST_ASSERT_DOUBLE(lv_he_mesh_face_area(m, f0), 0.5, 1e-9);
    TEST_ASSERT_DOUBLE(lv_he_mesh_face_area(m, f1), 0.5, 1e-9);

    /* face_valence：3 */
    TEST_ASSERT_EQ(lv_he_mesh_face_valence(m, f0), 3);

    /* halfedge_angle / corner_angle：正方形角为 90° */
    /* 面 f0 顶点 v1 处的角：he 为 v0->v1，corner = angle(prev(he), he) */
    lvHalfedge he_prev = lv_he_mesh_halfedge_next(m, lv_he_mesh_halfedge_next(m, he)); /* 环回 v2->v0 */
    double ang = lv_he_mesh_halfedge_corner_angle(m, he);
    TEST_ASSERT(ang > 0 && ang < lv_TWO_PI, "角在合法范围");
    (void) he_prev;

    /* NULL 契约 */
    TEST_ASSERT_EQ(lv_he_mesh_edge_halfedge(NULL, 0), lv_HE_INVALID);
    TEST_ASSERT_EQ(lv_he_mesh_face_halfedge(NULL, 0), lv_HE_INVALID);
    TEST_ASSERT_EQ(lv_he_mesh_halfedge_next(NULL, 0), lv_HE_INVALID);
    TEST_ASSERT_DOUBLE(lv_he_mesh_halfedge_angle(NULL, 0, 1), 0, 1e-12);
    TEST_ASSERT_DOUBLE(lv_he_mesh_halfedge_corner_angle(NULL, 0), 0, 1e-12);
    TEST_ASSERT_DOUBLE(lv_he_mesh_face_area(NULL, 0), 0, 1e-12);
    TEST_ASSERT_EQ(lv_he_mesh_face_valence(NULL, 0), 0);

    lv_he_mesh_destroy(m);
    printf("  test_he_face_query_api: PASSED\n");
}

/* ============== 测试：统计（max_vertex_valence 完整实现） ============== */

static void test_he_stats_api(void) {
    lvHeMesh *m = lv_he_mesh_create(NULL);
    TEST_ASSERT_NOT_NULL(m);

    /* 空网格：val=0 */
    lvHeMeshStats st;
    lv_he_mesh_get_stats(m, &st);
    TEST_ASSERT_EQ(st.max_vertex_valence, 0);
    TEST_ASSERT_EQ(st.vertex_count, 0);

    /* 正方形对角线三角化：中心顶点 v2 度数 4（出半边 4），其余顶点度数 2 */
    lvVertex v0 = lv_he_mesh_add_vertex(m, 0, 0, 0);
    lvVertex v1 = lv_he_mesh_add_vertex(m, 1, 0, 0);
    lvVertex v2 = lv_he_mesh_add_vertex(m, 0, 1, 0);
    lvVertex v3 = lv_he_mesh_add_vertex(m, 1, 1, 0);
    lv_he_mesh_add_face_triangle(m, v0, v1, v2);
    lv_he_mesh_add_face_triangle(m, v0, v2, v3);
    lv_he_mesh_add_face_triangle(m, v1, v3, v2);

    lv_he_mesh_get_stats(m, &st);
    TEST_ASSERT_EQ(st.vertex_count, 4);
    TEST_ASSERT(st.max_vertex_valence >= 3, "三角剖分网格顶点度数应 >= 3");

    /* NULL 契约 */
    lv_he_mesh_get_stats(NULL, &st); /* 不崩溃 */
    lv_he_mesh_get_stats(m, NULL);   /* 不崩溃 */

    lv_he_mesh_destroy(m);
    printf("  test_he_stats_api: PASSED\n");
}

/* ============== 测试：顶点查询 ============== */

static void test_he_vertex_query_api(void) {
    lvHeMesh *m = lv_he_mesh_create(NULL);
    lvVertex v0 = lv_he_mesh_add_vertex(m, 0, 0, 0);
    lvVertex v1 = lv_he_mesh_add_vertex(m, 1, 0, 0);
    lvVertex v2 = lv_he_mesh_add_vertex(m, 1, 1, 0);
    lvVertex v3 = lv_he_mesh_add_vertex(m, 0, 1, 0);
    lvFace f0 = lv_he_mesh_add_face_triangle(m, v0, v1, v2);
    lvFace f1 = lv_he_mesh_add_face_triangle(m, v0, v2, v3);
    (void) f0;
    (void) f1;

    /* vertex_out_halfedge */
    lvHalfedge out_he = lv_he_mesh_vertex_out_halfedge(m, v0);
    TEST_ASSERT(out_he >= 0, "顶点出半边有效");

    /* halfedge_vertex */
    TEST_ASSERT_EQ(lv_he_mesh_halfedge_vertex(m, out_he), v0);

    /* halfedge_twin 反向 */
    lvHalfedge twin = lv_he_mesh_halfedge_twin(m, out_he);
    TEST_ASSERT(twin >= 0, "孪生半边有效");

    /* vertex_angle：正方形角顶点 v0 总角 90°（两个 45° 三角形角） */
    double ang = lv_he_mesh_vertex_angle(m, v0);
    TEST_ASSERT(ang > 0, "顶点角为正");
    TEST_ASSERT(ang < lv_TWO_PI, "顶点角小于 2π");
    TEST_ASSERT_DOUBLE(ang, lv_TWO_PI / 4, 1e-9); /* 90° */

    /* vertex_curvature：离散曲率 = 2π - 邻接角和（边界顶点非零） */
    double curv = lv_he_mesh_vertex_curvature(m, v0);
    TEST_ASSERT_DOUBLE(curv, lv_TWO_PI - ang, 1e-9);

    /* vertex_normal：平面网格法线为 z 轴 */
    lvPoint3D n = lv_he_mesh_vertex_normal(m, v0);
    TEST_ASSERT_DOUBLE(n.x, 0, 1e-9);
    TEST_ASSERT_DOUBLE(n.y, 0, 1e-9);
    TEST_ASSERT_DOUBLE(n.z, 1, 1e-9);

    /* compute_normals / compute_curvature（M5 补齐）：可调用且不崩溃 */
    lv_he_mesh_compute_normals(m);
    lv_he_mesh_compute_curvature(m);
    curv = lv_he_mesh_vertex_curvature(m, v0);
    TEST_ASSERT_DOUBLE(curv, lv_TWO_PI - ang, 1e-9);

    /* NULL 契约 */
    TEST_ASSERT_EQ(lv_he_mesh_vertex_out_halfedge(NULL, 0), lv_HE_INVALID);
    TEST_ASSERT_EQ(lv_he_mesh_halfedge_vertex(NULL, 0), lv_HE_INVALID);
    TEST_ASSERT_EQ(lv_he_mesh_halfedge_twin(NULL, 0), lv_HE_INVALID);
    TEST_ASSERT_DOUBLE(lv_he_mesh_vertex_angle(NULL, 0), 0, 1e-12);
    TEST_ASSERT_DOUBLE(lv_he_mesh_vertex_curvature(NULL, 0), 0, 1e-12);
    lv_he_mesh_compute_normals(NULL);
    lv_he_mesh_compute_curvature(NULL);

    lv_he_mesh_destroy(m);
    printf("  test_he_vertex_query_api: PASSED\n");
}

/* ============== 测试：迭代器 ============== */

static void test_he_iterator_api(void) {
    lvHeMesh *m = lv_he_mesh_create(NULL);
    lvVertex v0 = lv_he_mesh_add_vertex(m, 0, 0, 0);
    lvVertex v1 = lv_he_mesh_add_vertex(m, 1, 0, 0);
    lvVertex v2 = lv_he_mesh_add_vertex(m, 1, 1, 0);
    lvVertex v3 = lv_he_mesh_add_vertex(m, 0, 1, 0);
    lvFace f0 = lv_he_mesh_add_face_triangle(m, v0, v1, v2);
    lvFace f1 = lv_he_mesh_add_face_triangle(m, v0, v2, v3);
    (void) f0;
    (void) f1;

    /* 短名顶点迭代器：环绕 v0 的邻接半边 */
    lvHeVertexIterator vit = lv_he_vertex_iter_begin(m, v0);
    TEST_ASSERT(lv_he_vertex_iter_valid(&vit), "顶点迭代器有效");
    int steps = 0;
    while (lv_he_vertex_iter_valid(&vit)) {
        lvHalfedge h = lv_he_vertex_iter_get(&vit);
        TEST_ASSERT(h >= 0, "迭代器取出半边有效");
        lv_he_vertex_iter_next(&vit);
        steps++;
    }
    TEST_ASSERT(steps >= 2, "顶点迭代器至少遍历 2 条邻接边");

    /* _mesh_ 前缀顶点迭代器（out_iter 围绕 v 的出半边） */
    lvHeVertexIterator oit = lv_he_mesh_vertex_out_iter_begin(m, v0);
    int osteps = 0;
    lvHalfedge oh = lv_he_mesh_vertex_out_iter_next(&oit);
    while (oh != lv_HE_INVALID) {
        osteps++;
        oh = lv_he_mesh_vertex_out_iter_next(&oit);
    }
    TEST_ASSERT(osteps >= 2, "out_iter 遍历完成");

    /* mesh_vertex_iter_begin/next：网格级遍历全部顶点（修复点：原实现
     * 把 flags 当索引 0 硬编码，只迭代顶点 0） */
    lvHeVertexIterator mit = lv_he_mesh_vertex_iter_begin(m, 0);
    int msteps = (mit.current != lv_HE_INVALID) ? 1 : 0;
    while (lv_he_mesh_vertex_iter_next(&mit)) {
        msteps++;
    }
    TEST_ASSERT_EQ(msteps, 4); /* 4 个顶点 */

    /* 面迭代器 */
    lvHeFaceIterator fit = lv_he_face_iter_begin(m, f0);
    TEST_ASSERT(lv_he_face_iter_valid(&fit), "面迭代器有效");
    int fsteps = 0;
    while (lv_he_face_iter_valid(&fit)) {
        lvHalfedge h = lv_he_face_iter_get(&fit);
        TEST_ASSERT(h >= 0, "面迭代器取出半边");
        lv_he_face_iter_next(&fit);
        fsteps++;
    }
    TEST_ASSERT_EQ(fsteps, 3);

    /* mesh_face_iter_begin/next：网格级遍历全部面（修复点：原实现硬编码面 0） */
    lvHeFaceIterator mfit = lv_he_mesh_face_iter_begin(m, 0);
    int mfsteps = (mfit.current != lv_HE_INVALID) ? 1 : 0;
    while (lv_he_mesh_face_iter_next(&mfit)) {
        mfsteps++;
    }
    TEST_ASSERT_EQ(mfsteps, 2); /* 2 个面 */

    /* NULL 契约 */
    lvHeVertexIterator null_it = lv_he_vertex_iter_begin(NULL, 0);
    TEST_ASSERT(!lv_he_vertex_iter_valid(&null_it), "NULL 顶点迭代器无效");
    lvHeFaceIterator null_fit = lv_he_face_iter_begin(NULL, 0);
    TEST_ASSERT(!lv_he_face_iter_valid(&null_fit), "NULL 面迭代器无效");
    TEST_ASSERT_EQ(lv_he_vertex_iter_get(NULL), lv_HE_INVALID);
    TEST_ASSERT_EQ(lv_he_face_iter_get(NULL), lv_HE_INVALID);
    lv_he_vertex_iter_next(NULL);
    lv_he_face_iter_next(NULL);

    lv_he_mesh_destroy(m);
    printf("  test_he_iterator_api: PASSED\n");
}

/* ============== 测试：通用面构造与 Legacy ============== */

static void test_he_addface_legacy_api(void) {
    /* add_face 通用多边形（M5 补齐）：五边形 */
    lvHeMesh *m = lv_he_mesh_create(NULL);
    lvVertex v[5];
    double coords[5][3] = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0.5, 1.5, 0}, {0, 1, 0}};
    for (int i = 0; i < 5; i++) {
        v[i] = lv_he_mesh_add_vertex(m, coords[i][0], coords[i][1], coords[i][2]);
    }
    const int idx[5] = {v[0], v[1], v[2], v[3], v[4]};
    lvFace f = lv_he_mesh_add_face(m, idx, 5);
    TEST_ASSERT(f >= 0, "五边形面创建成功");
    TEST_ASSERT_EQ(lv_he_mesh_face_count(m), 1);
    TEST_ASSERT_EQ(lv_he_mesh_face_valence(m, f), 5);
    TEST_ASSERT(lv_he_mesh_face_area(m, f) > 1.0, "五边形面积为正");
    TEST_ASSERT(lv_he_mesh_validate(m), "五边形网格有效");

    /* 退化面拒绝：count<3 */
    TEST_ASSERT_EQ(lv_he_mesh_add_face(m, idx, 2), lv_HE_INVALID);
    /* 相邻重复拒绝 */
    const int dup[3] = {v[0], v[0], v[1]};
    TEST_ASSERT_EQ(lv_he_mesh_add_face(m, dup, 3), lv_HE_INVALID);
    /* 非法顶点拒绝 */
    const int bad[3] = {v[0], v[1], 99};
    TEST_ASSERT_EQ(lv_he_mesh_add_face(m, bad, 3), lv_HE_INVALID);
    /* NULL 契约 */
    TEST_ASSERT_EQ(lv_he_mesh_add_face(NULL, idx, 3), lv_HE_INVALID);
    TEST_ASSERT_EQ(lv_he_mesh_add_face(m, NULL, 3), lv_HE_INVALID);
    lv_he_mesh_destroy(m);

    /* Legacy：halfedge_mesh_add_vertex / add_face（M5 补齐） */
    lvHalfedgeMesh *lm = lv_he_mesh_create(NULL);
    int a = lv_halfedge_mesh_add_vertex(lm, 0, 0, 0);
    int b = lv_halfedge_mesh_add_vertex(lm, 1, 0, 0);
    int c = lv_halfedge_mesh_add_vertex(lm, 0, 1, 0);
    TEST_ASSERT(a >= 0 && b >= 0 && c >= 0, "legacy 顶点创建");
    const int li[3] = {a, b, c};
    int lf = lv_halfedge_mesh_add_face(lm, li, 3);
    TEST_ASSERT(lf >= 0, "legacy 面创建");
    TEST_ASSERT_EQ(lv_he_mesh_face_count(lm), 1);
    /* legacy NULL 契约 */
    TEST_ASSERT_EQ(lv_halfedge_mesh_add_vertex(NULL, 0, 0, 0), lv_HE_INVALID);
    TEST_ASSERT_EQ(lv_halfedge_mesh_add_face(NULL, li, 3), lv_HE_INVALID);
    TEST_ASSERT_EQ(lv_halfedge_mesh_add_face(lm, NULL, 3), lv_HE_INVALID);
    TEST_ASSERT_EQ(lv_halfedge_mesh_add_face(lm, li, 2), lv_HE_INVALID);
    lv_he_mesh_destroy(lm);

    printf("  test_he_addface_legacy_api: PASSED\n");
}

/* ============== 测试入口 ============== */

TEST_MAIN_BEGIN("Lv-00 Halfedge Mesh Ext Test Suite")
    printf("=== Lv-00 Halfedge Mesh Ext Test Suite (batch C-㊺续14) ===\n\n");
    lv_init();

    TEST_MAIN_RUN(test_he_construct_api);
    TEST_MAIN_RUN(test_he_valid_api);
    TEST_MAIN_RUN(test_he_face_query_api);
    TEST_MAIN_RUN(test_he_stats_api);
    TEST_MAIN_RUN(test_he_vertex_query_api);
    TEST_MAIN_RUN(test_he_iterator_api);
    TEST_MAIN_RUN(test_he_addface_legacy_api);

    lv_cleanup();
TEST_MAIN_END()

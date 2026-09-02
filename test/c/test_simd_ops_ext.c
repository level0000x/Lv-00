/**
 * @file test_simd_ops_ext.c
 * @brief SIMD 运算库契约测试（批次 C-㊺续16：simd_ops.h 60 个零覆盖 API）
 *
 * 覆盖零覆盖 API：
 *   能力族：detect_capabilities / has_capability / capability_name /
 *     get_stats / reset_stats
 *   Vec4d 族：zero/one/set1/set/load/loadu/store/storeu/add/sub/mul/div/
 *     neg/sqrt/abs/max/min/fmadd/cmpeq/cmplt/cmple/cmpgt/cmpge/select/
 *     hsum/hmax/hmin/dot/norm/normalize/cross
 *   Vec4f 族：zero/set1/load/store/add/sub/mul/div/sqrt/hsum/dot
 *   Vec8f 族：zero/set1/load/store/add/sub/mul/div/hsum
 *   矩阵/批量：mat4x4_vec4_mul / dot_product_array / norm_array / scale_array
 *
 * 契约要点：
 *   - 比较/select 的掩码语义：true=-1.0（全 1 位），false=0.0。
 *   - normalize：norm <= 阈值时原样返回。
 *   - mat4x4_vec4_mul：列主序矩阵 × 向量。
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/simd_ops.h"

int g_pass_count = 0;
int g_fail_count = 0;

#define ASSERT_VEC4D(vecval, xv, yv, zv, wv, msg)               \
    do {                                                         \
        TEST_ASSERT_DOUBLE((vecval).v[0], (xv), 1e-9);           \
        TEST_ASSERT_DOUBLE((vecval).v[1], (yv), 1e-9);           \
        TEST_ASSERT_DOUBLE((vecval).v[2], (zv), 1e-9);           \
        TEST_ASSERT_DOUBLE((vecval).v[3], (wv), 1e-9);           \
        (void) (msg);                                            \
    } while (0)

/* ============== 测试：能力检测 ============== */

static void test_simd_capability_api(void) {
    /* detect_capabilities：返回位掩码，包含 NONE 或已知能力 */
    uint32_t caps = lv_simd_detect_capabilities();
    TEST_ASSERT(caps >= 0, "能力掩码非负");

    /* has_capability：与 detect 一致 */
    bool has_none = lv_simd_has_capability(lv_SIMD_NONE);
    (void) has_none;
    TEST_ASSERT(!lv_simd_has_capability((lvSimdCapability) 0x40000000), "未知能力为 false");

    /* capability_name：各能力名非空 */
    TEST_ASSERT_NOT_NULL(lv_simd_capability_name(lv_SIMD_NONE));
    TEST_ASSERT_NOT_NULL(lv_simd_capability_name(lv_SIMD_SSE2));
    TEST_ASSERT_NOT_NULL(lv_simd_capability_name(lv_SIMD_AVX512F));
    TEST_ASSERT(strlen(lv_simd_capability_name(lv_SIMD_SSE2)) > 0, "能力名非空");
    /* 已知能力返回具体名称 */
    const char *n = lv_simd_capability_name(lv_SIMD_NEON);
    TEST_ASSERT(strcmp(n, "Unknown") != 0, "NEON 有名");

    /* get_stats / reset_stats */
    lvSimdStats stats;
    lv_simd_get_stats(&stats);
    TEST_ASSERT(stats.vec4_ops >= 0, "统计非负");
    lv_simd_reset_stats();
    lv_simd_get_stats(&stats);
    TEST_ASSERT_EQ(stats.vec4_ops, 0);
    TEST_ASSERT_EQ(stats.vec8_ops, 0);
    TEST_ASSERT_EQ(stats.array_ops, 0);
    TEST_ASSERT_EQ(stats.elements_processed, 0);
    TEST_ASSERT_EQ(stats.scalar_fallbacks, 0);
    /* NULL 契约 */
    lv_simd_get_stats(NULL);

    printf("  test_simd_capability_api: PASSED\n");
}

/* ============== 测试：Vec4d 构造 ============== */

static void test_vec4d_construct_api(void) {
    lvVec4d z = lv_vec4d_zero();
    ASSERT_VEC4D(z, 0, 0, 0, 0, "zero");

    lvVec4d o = lv_vec4d_one();
    ASSERT_VEC4D(o, 1, 1, 1, 1, "one");

    lvVec4d s = lv_vec4d_set1(3.5);
    ASSERT_VEC4D(s, 3.5, 3.5, 3.5, 3.5, "set1");

    lvVec4d st = lv_vec4d_set(1, 2, 3, 4);
    ASSERT_VEC4D(st, 1, 2, 3, 4, "set");

    /* load/store */
    double buf[4] = {1.5, 2.5, 3.5, 4.5};
    lvVec4d ld = lv_vec4d_load(buf);
    ASSERT_VEC4D(ld, 1.5, 2.5, 3.5, 4.5, "load");
    lvVec4d ldu = lv_vec4d_loadu(buf);
    ASSERT_VEC4D(ldu, 1.5, 2.5, 3.5, 4.5, "loadu");

    double out[4] = {0, 0, 0, 0};
    lv_vec4d_store(out, ld);
    TEST_ASSERT_DOUBLE(out[0], 1.5, 1e-12);
    TEST_ASSERT_DOUBLE(out[3], 4.5, 1e-12);
    memset(out, 0, sizeof(out));
    lv_vec4d_storeu(out, ld);
    TEST_ASSERT_DOUBLE(out[0], 1.5, 1e-12);
    TEST_ASSERT_DOUBLE(out[3], 4.5, 1e-12);

    printf("  test_vec4d_construct_api: PASSED\n");
}

/* ============== 测试：Vec4d 算术 ============== */

static void test_vec4d_arith_api(void) {
    lvVec4d a = lv_vec4d_set(1, 2, 3, 4);
    lvVec4d b = lv_vec4d_set(5, 6, 7, 8);

    lvVec4d r = lv_vec4d_add(a, b);
    ASSERT_VEC4D(r, 6, 8, 10, 12, "add");
    r = lv_vec4d_sub(a, b);
    ASSERT_VEC4D(r, -4, -4, -4, -4, "sub");
    r = lv_vec4d_mul(a, b);
    ASSERT_VEC4D(r, 5, 12, 21, 32, "mul");
    r = lv_vec4d_div(b, a);
    ASSERT_VEC4D(r, 5, 3, 7.0 / 3.0, 2, "div");
    r = lv_vec4d_neg(a);
    ASSERT_VEC4D(r, -1, -2, -3, -4, "neg");
    r = lv_vec4d_sqrt(lv_vec4d_set1(4.0));
    ASSERT_VEC4D(r, 2, 2, 2, 2, "sqrt");
    r = lv_vec4d_abs(lv_vec4d_set(-1, -2, 3, -4));
    ASSERT_VEC4D(r, 1, 2, 3, 4, "abs");
    r = lv_vec4d_max(a, b);
    ASSERT_VEC4D(r, 5, 6, 7, 8, "max");
    r = lv_vec4d_min(a, b);
    ASSERT_VEC4D(r, 1, 2, 3, 4, "min");

    /* fmadd：a*x+y = (1,2,3,4)*(5,6,7,8)+(1,1,1,1) */
    lvVec4d y = lv_vec4d_one();
    r = lv_vec4d_fmadd(a, b, y);
    ASSERT_VEC4D(r, 6, 13, 22, 33, "fmadd");

    printf("  test_vec4d_arith_api: PASSED\n");
}

/* ============== 测试：Vec4d 比较/选择 ============== */

static void test_vec4d_cmp_api(void) {
    lvVec4d a = lv_vec4d_set(1, 2, 3, 4);
    lvVec4d b = lv_vec4d_set(1, 6, 3, 8);
    lvVec4d r;

    r = lv_vec4d_cmpeq(a, b);
    ASSERT_VEC4D(r, -1.0, 0.0, -1.0, 0.0, "cmpeq");
    r = lv_vec4d_cmplt(a, b);
    ASSERT_VEC4D(r, 0.0, -1.0, 0.0, -1.0, "cmplt");
    r = lv_vec4d_cmple(a, b);
    ASSERT_VEC4D(r, -1.0, -1.0, -1.0, -1.0, "cmple");
    r = lv_vec4d_cmpgt(a, b);
    ASSERT_VEC4D(r, 0.0, 0.0, 0.0, 0.0, "cmpgt");
    r = lv_vec4d_cmpge(a, b);
    ASSERT_VEC4D(r, -1.0, 0.0, -1.0, 0.0, "cmpge");

    /* select：mask 为 -1 处取 a，0 处取 b */
    lvVec4d mask = lv_vec4d_set(-1.0, 0.0, -1.0, 0.0);
    lvVec4d x = lv_vec4d_set(10, 20, 30, 40);
    lvVec4d y = lv_vec4d_set(1, 2, 3, 4);
    r = lv_vec4d_select(mask, x, y);
    ASSERT_VEC4D(r, 10, 2, 30, 4, "select");

    printf("  test_vec4d_cmp_api: PASSED\n");
}

/* ============== 测试：Vec4d 归约/几何 ============== */

static void test_vec4d_reduce_api(void) {
    lvVec4d a = lv_vec4d_set(1, 2, 3, 4);

    TEST_ASSERT_DOUBLE(lv_vec4d_hsum(a), 10, 1e-9);
    TEST_ASSERT_DOUBLE(lv_vec4d_hmax(a), 4, 1e-9);
    TEST_ASSERT_DOUBLE(lv_vec4d_hmin(a), 1, 1e-9);

    lvVec4d b = lv_vec4d_set(2, 3, 4, 5);
    /* dot = 2+6+12+20 = 40 */
    TEST_ASSERT_DOUBLE(lv_vec4d_dot(a, b), 40, 1e-9);
    /* norm = sqrt(1+4+9+16) = sqrt(30) */
    TEST_ASSERT_DOUBLE(lv_vec4d_norm(a), sqrt(30.0), 1e-9);

    /* normalize：单位向量 */
    lvVec4d u = lv_vec4d_normalize(lv_vec4d_set(3, 0, 4, 0));
    TEST_ASSERT_DOUBLE(lv_vec4d_norm(u), 1, 1e-9);
    TEST_ASSERT_DOUBLE(u.v[0], 0.6, 1e-9);
    TEST_ASSERT_DOUBLE(u.v[2], 0.8, 1e-9);

    /* normalize 零向量：原样返回（阈值保护） */
    lvVec4d z = lv_vec4d_normalize(lv_vec4d_zero());
    ASSERT_VEC4D(z, 0, 0, 0, 0, "zero normalize");

    /* cross：3D 叉积，w=0 */
    lvVec4d p = lv_vec4d_set(1, 0, 0, 0);
    lvVec4d q = lv_vec4d_set(0, 1, 0, 0);
    lvVec4d c = lv_vec4d_cross(p, q);
    ASSERT_VEC4D(c, 0, 0, 1, 0, "cross");

    printf("  test_vec4d_reduce_api: PASSED\n");
}

/* ============== 测试：Vec4f ============== */

static void test_vec4f_api(void) {
    lvVec4f z = lv_vec4f_zero();
    TEST_ASSERT_DOUBLE(z.v[0], 0, 1e-6);
    TEST_ASSERT_DOUBLE(z.v[3], 0, 1e-6);

    lvVec4f s = lv_vec4f_set1(2.0f);
    TEST_ASSERT_DOUBLE(s.v[1], 2, 1e-6);

    float buf[4] = {1, 2, 3, 4};
    lvVec4f l = lv_vec4f_load(buf);
    TEST_ASSERT_DOUBLE(l.v[0], 1, 1e-6);
    TEST_ASSERT_DOUBLE(l.v[3], 4, 1e-6);

    float out[4] = {0, 0, 0, 0};
    lv_vec4f_store(out, l);
    TEST_ASSERT_DOUBLE(out[0], 1, 1e-6);
    TEST_ASSERT_DOUBLE(out[2], 3, 1e-6);

    lvVec4f a = lv_vec4f_set1(1.0f);
    lvVec4f b = lv_vec4f_set1(2.0f);
    lvVec4f r = lv_vec4f_add(a, b);
    TEST_ASSERT_DOUBLE(r.v[0], 3, 1e-6);
    r = lv_vec4f_sub(b, a);
    TEST_ASSERT_DOUBLE(r.v[0], 1, 1e-6);
    r = lv_vec4f_mul(a, b);
    TEST_ASSERT_DOUBLE(r.v[0], 2, 1e-6);
    r = lv_vec4f_div(b, a);
    TEST_ASSERT_DOUBLE(r.v[0], 2, 1e-6);
    r = lv_vec4f_sqrt(lv_vec4f_set1(16.0f));
    TEST_ASSERT_DOUBLE(r.v[0], 4, 1e-6);

    /* hsum / dot */
    lvVec4f v = lv_vec4f_load(buf); /* 1,2,3,4 */
    TEST_ASSERT_DOUBLE(lv_vec4f_hsum(v), 10, 1e-4);
    lvVec4f w = lv_vec4f_set1(2.0f);
    TEST_ASSERT_DOUBLE(lv_vec4f_dot(v, w), 20, 1e-3);

    printf("  test_vec4f_api: PASSED\n");
}

/* ============== 测试：Vec8f ============== */

static void test_vec8f_api(void) {
    lvVec8f z = lv_vec8f_zero();
    TEST_ASSERT_DOUBLE(z.v[0], 0, 1e-6);
    TEST_ASSERT_DOUBLE(z.v[7], 0, 1e-6);

    lvVec8f s = lv_vec8f_set1(3.0f);
    TEST_ASSERT_DOUBLE(s.v[3], 3, 1e-6);
    TEST_ASSERT_DOUBLE(s.v[7], 3, 1e-6);

    float buf[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    lvVec8f l = lv_vec8f_load(buf);
    TEST_ASSERT_DOUBLE(l.v[0], 1, 1e-6);
    TEST_ASSERT_DOUBLE(l.v[7], 8, 1e-6);

    float out[8] = {0};
    lv_vec8f_store(out, l);
    TEST_ASSERT_DOUBLE(out[0], 1, 1e-6);
    TEST_ASSERT_DOUBLE(out[7], 8, 1e-6);

    lvVec8f a = lv_vec8f_set1(1.0f);
    lvVec8f b = lv_vec8f_set1(2.0f);
    lvVec8f r = lv_vec8f_add(a, b);
    TEST_ASSERT_DOUBLE(r.v[0], 3, 1e-6);
    r = lv_vec8f_sub(b, a);
    TEST_ASSERT_DOUBLE(r.v[7], 1, 1e-6);
    r = lv_vec8f_mul(a, b);
    TEST_ASSERT_DOUBLE(r.v[0], 2, 1e-6);
    r = lv_vec8f_div(b, a);
    TEST_ASSERT_DOUBLE(r.v[0], 2, 1e-6);

    /* hsum = 1+2+...+8 = 36 */
    TEST_ASSERT_DOUBLE(lv_vec8f_hsum(l), 36, 1e-3);

    printf("  test_vec8f_api: PASSED\n");
}

/* ============== 测试：矩阵与批量 ============== */

static void test_simd_matrix_batch_api(void) {
    /* mat4x4_vec4_mul：列主序矩阵（单位阵） */
    double ident[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    lvVec4d v = lv_vec4d_set(2, 3, 4, 1);
    lvVec4d r = lv_simd_mat4x4_vec4_mul(ident, v);
    ASSERT_VEC4D(r, 2, 3, 4, 1, "identity");

    /* 缩放矩阵：diag(2,2,2,1) */
    double scale[16] = {2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 1};
    r = lv_simd_mat4x4_vec4_mul(scale, v);
    ASSERT_VEC4D(r, 4, 6, 8, 1, "scale");

    /* dot_product_array */
    double a[4] = {1, 2, 3, 4};
    double b[4] = {5, 6, 7, 8};
    TEST_ASSERT_DOUBLE(lv_simd_dot_product_array(a, b, 4), 70, 1e-9);
    TEST_ASSERT_DOUBLE(lv_simd_dot_product_array(a, b, 0), 0, 1e-9);

    /* norm_array：count 个 4D 向量各自求范数（每组 4 元素）
     * 向量1=(3,4,0,0) → 5；向量2=(5,12,0,0) → 13 */
    double in[8] = {3, 4, 0, 0, 5, 12, 0, 0};
    double nout[4] = {0, 0, 0, 0};
    lv_simd_norm_array(in, nout, 2);
    TEST_ASSERT_DOUBLE(nout[0], 5, 1e-9);
    TEST_ASSERT_DOUBLE(nout[1], 13, 1e-9);

    /* scale_array */
    double sout[4] = {0, 0, 0, 0};
    lv_simd_scale_array(a, 2.0, sout, 4);
    TEST_ASSERT_DOUBLE(sout[0], 2, 1e-9);
    TEST_ASSERT_DOUBLE(sout[3], 8, 1e-9);

    printf("  test_simd_matrix_batch_api: PASSED\n");
}

/* ============== 蓝图批量原语导出（PERFORMANCE_OPTIMIZATION.md §2.3，批次 SIMD-export） ============== */

static void test_blueprint_simd_batch_export(void) {
    /* add_array_d */
    double a[5] = {1, 2, 3, 4, 5};
    double b[5] = {10, 20, 30, 40, 50};
    double o[5] = {0, 0, 0, 0, 0};
    lv_simd_add_array_d(a, b, o, 5);
    for (int i = 0; i < 5; i++)
        TEST_ASSERT_DOUBLE(o[i], a[i] + b[i], 1e-9);
    lv_simd_add_array_d(a, b, o, 0); /* 空 count 安全 */

    /* sum_array_d */
    TEST_ASSERT_DOUBLE(lv_simd_sum_array_d(a, 5), 15, 1e-9);
    TEST_ASSERT_DOUBLE(lv_simd_sum_array_d(a, 0), 0, 1e-9);
    double neg[3] = {-1.5, 2.5, -1.0};
    TEST_ASSERT_DOUBLE(lv_simd_sum_array_d(neg, 3), 0, 1e-9);

    /* distance_array：2D 逐对欧氏距离 */
    double x1[3] = {0, 3, 1};
    double y1[3] = {0, 4, 1};
    double x2[3] = {0, 0, 4};
    double y2[3] = {0, 0, 5};
    double dout[3] = {0, 0, 0};
    lv_simd_distance_array(x1, y1, x2, y2, dout, 3);
    TEST_ASSERT_DOUBLE(dout[0], 0, 1e-9);   /* (0,0)-(0,0) */
    TEST_ASSERT_DOUBLE(dout[1], 5, 1e-9);   /* (3,4)-(0,0) */
    TEST_ASSERT_DOUBLE(dout[2], 5, 1e-9);   /* (1,1)-(4,5) */

    /* cross2d_array：叉积 z 分量 */
    double ax[2] = {1, 3};
    double ay[2] = {0, 4};
    double bx[2] = {0, 5};
    double by[2] = {2, 6};
    double cout[2] = {0, 0};
    lv_simd_cross2d_array(ax, ay, bx, by, cout, 2);
    TEST_ASSERT_DOUBLE(cout[0], 1 * 2 - 0 * 0, 1e-9); /* 2 */
    TEST_ASSERT_DOUBLE(cout[1], 3 * 6 - 4 * 5, 1e-9); /* -2 */

    printf("  test_blueprint_simd_batch_export: PASSED\n");
}

/* ============== 测试入口 ============== */

TEST_MAIN_BEGIN("Lv-00 SIMD Ops Ext Test Suite")
    printf("=== Lv-00 SIMD Ops Ext Test Suite (batch C-㊺续16) ===\n\n");
    lv_init();

    TEST_MAIN_RUN(test_simd_capability_api);
    TEST_MAIN_RUN(test_vec4d_construct_api);
    TEST_MAIN_RUN(test_vec4d_arith_api);
    TEST_MAIN_RUN(test_vec4d_cmp_api);
    TEST_MAIN_RUN(test_vec4d_reduce_api);
    TEST_MAIN_RUN(test_vec4f_api);
    TEST_MAIN_RUN(test_vec8f_api);
    TEST_MAIN_RUN(test_simd_matrix_batch_api);
    TEST_MAIN_RUN(test_blueprint_simd_batch_export);

    lv_cleanup();
TEST_MAIN_END()

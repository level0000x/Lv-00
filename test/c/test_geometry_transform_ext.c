/**
 * @file test_geometry_transform_ext.c
 * @brief 几何变换契约测试（批次 C-㊺续9：geometry_transform.h 17 个零覆盖 API）
 *
 * 覆盖 17 个 ctest 零覆盖 API：
 *   - 引用计数族：lv_transform_ref / lv_transform_unref
 *   - 应用族：lv_transform_apply_double / apply_mpq / apply_double4x4
 *   - 矩阵族：lv_transform_get_matrix / lv_transform_identity_double /
 *     lv_transform_translate_double / lv_transform_rotate_double /
 *     lv_transform_scale_double
 *   - 序列/群族：lv_transform_sequence_add / sequence_destroy /
 *     lv_transform_group_add_generator / group_destroy
 *   - 分析族：lv_transform_order / lv_transform_identify_symmetries /
 *     lv_reflect_point
 *
 * 契约要点（与头注释核对）：
 *   - ref/unref：NULL 安全；unref 归零释放。
 *   - order：恒等 → 1；无限阶（平移）→ 0；NULL → -1。
 *   - reflect_point：x 轴反射 (2,3) → (2,-3)。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "test_unified.h"
#include "lv/geometry_transform.h"

int g_pass_count = 0;
int g_fail_count = 0;

static void set_mpq(mpq_t q, long num) {
    mpq_set_si(q, num, 1);
}

/* ============== 测试：引用计数 ============== */

static void test_refcount_api(void) {
    /* NULL 安全 */
    lv_transform_ref(NULL);
    lv_transform_unref(NULL);

    lvTransform *t = lv_transform_identity();
    TEST_ASSERT_NOT_NULL(t);
    TEST_ASSERT_EQ(t->ref_count, 1);

    lv_transform_ref(t);
    TEST_ASSERT_EQ(t->ref_count, 2);
    lv_transform_ref(t);
    TEST_ASSERT_EQ(t->ref_count, 3);

    lv_transform_unref(t);
    TEST_ASSERT_EQ(t->ref_count, 2);
    lv_transform_unref(t);
    TEST_ASSERT_EQ(t->ref_count, 1);
    lv_transform_unref(t); /* 归零释放 */

    printf("  test_refcount_api: PASSED\n");
}

/* ============== 测试：点应用 ============== */

static void test_apply_api(void) {
    /* apply_double：平移 (2,3) → 点 (1,1) → (3,4) */
    mpq_t dx;
    mpq_init(dx);
    set_mpq(dx, 2);
    mpq_t dy;
    mpq_init(dy);
    set_mpq(dy, 3);
    lvTransform *tr = lv_transform_translation(dx, dy);
    TEST_ASSERT_NOT_NULL(tr);
    double ox = 0, oy = 0;
    lv_transform_apply_double(tr, 1.0, 1.0, &ox, &oy);
    TEST_ASSERT_EQ(ox, 3.0);
    TEST_ASSERT_EQ(oy, 4.0);

    /* apply_mpq：同上 */
    mpq_t sx;
    mpq_init(sx);
    set_mpq(sx, 1);
    mpq_t sy;
    mpq_init(sy);
    set_mpq(sy, 1);
    mpq_t tx, ty;
    mpq_init(tx);
    mpq_init(ty);
    lv_transform_apply_mpq(tr, sx, sy, tx, ty);
    TEST_ASSERT_EQ(mpq_get_d(tx), 3.0);
    TEST_ASSERT_EQ(mpq_get_d(ty), 4.0);
    mpq_clear(tx);
    mpq_clear(ty);
    mpq_clear(sx);
    mpq_clear(sy);
    mpq_clear(dx);
    mpq_clear(dy);
    lv_transform_destroy(tr);

    /* apply_double4x4：恒等 4x4 → 顶点不变 */
    double ident[16];
    lv_transform_identity_double(ident);
    double in[6] = {1, 2, 3, 4, 5, 6};
    double out[6] = {0, 0, 0, 0, 0, 0};
    lv_transform_apply_double4x4(ident, in, out, 2);
    TEST_ASSERT_EQ(out[0], 1.0);
    TEST_ASSERT_EQ(out[1], 2.0);
    TEST_ASSERT_EQ(out[3], 4.0);

    printf("  test_apply_api: PASSED\n");
}

/* ============== 测试：矩阵获取与 double 矩阵 ============== */

static void test_matrix_api(void) {
    /* get_matrix：恒等 → a=1, d=1, tx=0, ty=0 */
    lvTransform *t = lv_transform_identity();
    TEST_ASSERT_NOT_NULL(t);
    lvTransformMatrix m;
    TEST_ASSERT(lv_transform_get_matrix(t, &m), "获取矩阵");
    TEST_ASSERT_EQ(mpq_get_d(m.a), 1.0);
    TEST_ASSERT_EQ(mpq_get_d(m.d), 1.0);
    TEST_ASSERT_EQ(mpq_get_d(m.tx), 0.0);
    TEST_ASSERT_EQ(mpq_get_d(m.ty), 0.0);
    mpq_clear(m.a);
    mpq_clear(m.b);
    mpq_clear(m.tx);
    mpq_clear(m.c);
    mpq_clear(m.d);
    mpq_clear(m.ty);
    lv_transform_destroy(t);

    /* 平移：tx = dx */
    mpq_t dx;
    mpq_init(dx);
    set_mpq(dx, 5);
    mpq_t dy;
    mpq_init(dy);
    set_mpq(dy, 0);
    lvTransform *tr = lv_transform_translation(dx, dy);
    TEST_ASSERT_NOT_NULL(tr);
    lvTransformMatrix m2;
    TEST_ASSERT(lv_transform_get_matrix(tr, &m2), "平移矩阵获取");
    TEST_ASSERT_EQ(mpq_get_d(m2.tx), 5.0);
    mpq_clear(m2.a);
    mpq_clear(m2.b);
    mpq_clear(m2.tx);
    mpq_clear(m2.c);
    mpq_clear(m2.d);
    mpq_clear(m2.ty);
    mpq_clear(dx);
    mpq_clear(dy);
    lv_transform_destroy(tr);

    /* identity_double：主对角 1 */
    double id[16];
    lv_transform_identity_double(id);
    TEST_ASSERT_EQ(id[0], 1.0);
    TEST_ASSERT_EQ(id[5], 1.0);
    TEST_ASSERT_EQ(id[10], 1.0);
    TEST_ASSERT_EQ(id[15], 1.0);
    TEST_ASSERT_EQ(id[1], 0.0);

    /* translate_double：tx/ty/tz 元素 */
    double mtx[16];
    lv_transform_translate_double(mtx, 2, 3, 4);
    TEST_ASSERT_EQ(mtx[12], 2.0);
    TEST_ASSERT_EQ(mtx[13], 3.0);
    TEST_ASSERT_EQ(mtx[14], 4.0);

    /* rotate_double：绕 z 轴 90° → 列主序旋转矩阵 out[1]=-sin、out[4]=+sin */
    double rot[16];
    lv_transform_rotate_double(rot, M_PI / 2, 0, 0, 1);
    TEST_ASSERT(fabs(rot[0]) < 1e-9, "cos90 ≈ 0");
    TEST_ASSERT(fabs(rot[1] + 1.0) < 1e-9, "sin90 负号位置");
    TEST_ASSERT(fabs(rot[4] - 1.0) < 1e-9, "sin90 正号位置");

    /* scale_double：sx/sy/sz 元素 */
    double scl[16];
    lv_transform_scale_double(scl, 2, 3, 4);
    TEST_ASSERT_EQ(scl[0], 2.0);
    TEST_ASSERT_EQ(scl[5], 3.0);
    TEST_ASSERT_EQ(scl[10], 4.0);

    printf("  test_matrix_api: PASSED\n");
}

/* ============== 测试：序列与群 ============== */

static void test_sequence_group_api(void) {
    /* sequence_add：NULL 契约 + 正常 */
    lvTransformSequence *seq = lv_transform_sequence_create();
    TEST_ASSERT_NOT_NULL(seq);
    TEST_ASSERT(!lv_transform_sequence_add(NULL, NULL), "NULL seq");
    TEST_ASSERT(!lv_transform_sequence_add(seq, NULL), "NULL transform");
    mpq_t dx;
    mpq_init(dx);
    set_mpq(dx, 1);
    mpq_t dy;
    mpq_init(dy);
    set_mpq(dy, 0);
    lvTransform *tr = lv_transform_translation(dx, dy);
    TEST_ASSERT_NOT_NULL(tr);
    TEST_ASSERT(lv_transform_sequence_add(seq, tr), "添加平移");
    TEST_ASSERT_EQ(seq->transforms_da.count, 1);

    /* compose_all：序列合成（平移） */
    lvTransform *comp = lv_transform_sequence_compose_all(seq);
    TEST_ASSERT_NOT_NULL(comp);
    double ox = 0, oy = 0;
    lv_transform_apply_double(comp, 0.0, 0.0, &ox, &oy);
    TEST_ASSERT_EQ(ox, 1.0);
    lv_transform_destroy(comp);

    lv_transform_sequence_destroy(seq);
    mpq_clear(dx);
    mpq_clear(dy);

    /* group_add_generator：NULL 契约 + 正常 */
    lvTransformGroup *grp = lv_transform_group_create("test_group");
    TEST_ASSERT_NOT_NULL(grp);
    TEST_ASSERT(!lv_transform_group_add_generator(NULL, NULL), "NULL group");
    TEST_ASSERT(!lv_transform_group_add_generator(grp, NULL), "NULL generator");
    lvTransform *id = lv_transform_identity();
    TEST_ASSERT_NOT_NULL(id);
    TEST_ASSERT(lv_transform_group_add_generator(grp, id), "添加生成元");
    TEST_ASSERT_EQ(grp->generator_count, 1);
    lv_transform_group_destroy(grp);
    lv_transform_group_destroy(NULL); /* NULL 安全 */

    printf("  test_sequence_group_api: PASSED\n");
}

/* ============== 测试：阶与反射 ============== */

static void test_order_reflect_api(void) {
    /* order：恒等 → 1；平移（无限）→ 0；NULL → -1 */
    TEST_ASSERT_EQ(lv_transform_order(NULL), -1);
    lvTransform *id = lv_transform_identity();
    TEST_ASSERT_NOT_NULL(id);
    TEST_ASSERT_EQ(lv_transform_order(id), 1);
    lv_transform_destroy(id);
    mpq_t dx;
    mpq_init(dx);
    set_mpq(dx, 1);
    mpq_t dy;
    mpq_init(dy);
    set_mpq(dy, 0);
    lvTransform *tr = lv_transform_translation(dx, dy);
    TEST_ASSERT_NOT_NULL(tr);
    TEST_ASSERT_EQ(lv_transform_order(tr), 0); /* 平移无限阶 */
    lv_transform_destroy(tr);
    mpq_clear(dx);
    mpq_clear(dy);

    /* reflect_point：x 轴反射 (2,3) → (2,-3)
     * 参数顺序（C-㊺续9 修复后统一）：点在前 (px,py,ax,ay,bx,by) */
    mpq_t px;
    mpq_init(px);
    set_mpq(px, 2);
    mpq_t py;
    mpq_init(py);
    set_mpq(py, 3);
    mpq_t ax;
    mpq_init(ax);
    set_mpq(ax, 0);
    mpq_t ay;
    mpq_init(ay);
    set_mpq(ay, 0);
    mpq_t bx;
    mpq_init(bx);
    set_mpq(bx, 1);
    mpq_t by;
    mpq_init(by);
    set_mpq(by, 0);
    mpq_t rx, ry;
    mpq_init(rx);
    mpq_init(ry);
    TEST_ASSERT(lv_reflect_point(px, py, ax, ay, bx, by, rx, ry), "点反射");
    TEST_ASSERT_EQ(mpq_get_d(rx), 2.0);
    TEST_ASSERT_EQ(mpq_get_d(ry), -3.0);
    mpq_clear(ax);
    mpq_clear(ay);
    mpq_clear(bx);
    mpq_clear(by);
    mpq_clear(px);
    mpq_clear(py);
    mpq_clear(rx);
    mpq_clear(ry);

    /* identify_symmetries：NULL 图 → 0 */
    TEST_ASSERT_EQ(lv_transform_identify_symmetries(NULL, NULL, 0), 0);

    printf("  test_order_reflect_api: PASSED\n");
}

/* ============================================================
 * 批次 C-㊺续32：追加零覆盖 API（6 个）
 *   lv_transform_group_create_preset / reflection_line / rotation_arbitrary
 *   / rotation_double / scale / type_name
 * ============================================================ */

static void test_type_name_api(void) {
    TEST_ASSERT_STR_EQ(lv_transform_type_name(TRANSFORM_IDENTITY), "identity");
    TEST_ASSERT_STR_EQ(lv_transform_type_name(TRANSFORM_TRANSLATION), "translation");
    TEST_ASSERT_STR_EQ(lv_transform_type_name(TRANSFORM_ROTATION), "rotation");
    TEST_ASSERT_STR_EQ(lv_transform_type_name(TRANSFORM_SCALE), "scale");
    TEST_ASSERT_STR_EQ(lv_transform_type_name(TRANSFORM_SHEAR), "shear");
    TEST_ASSERT_STR_EQ(lv_transform_type_name(TRANSFORM_REFLECTION), "reflection");
    TEST_ASSERT_STR_EQ(lv_transform_type_name(TRANSFORM_SCALING), "scaling");
    TEST_ASSERT_STR_EQ(lv_transform_type_name(TRANSFORM_AFFINE), "affine");
    TEST_ASSERT_STR_EQ(lv_transform_type_name(TRANSFORM_PROJECTIVE), "projective");
    TEST_ASSERT_STR_EQ(lv_transform_type_name(TRANSFORM_GLUING), "gluing");
    TEST_ASSERT_STR_EQ(lv_transform_type_name(TRANSFORM_COMPOSITE), "composite");
    TEST_ASSERT_STR_EQ(lv_transform_type_name((lvTransformType) 99), "unknown");
    printf("  test_type_name_api: PASSED\n");
}

static void test_scale_api(void) {
    mpq_t sx, sy;
    mpq_init(sx);
    mpq_init(sy);
    mpq_set_si(sx, 2, 1);
    mpq_set_si(sy, 3, 1);

    lvTransform *t = lv_transform_scale(sx, sy);
    TEST_ASSERT_NOT_NULL(t);
    TEST_ASSERT_EQ((int) t->type, (int) TRANSFORM_SCALE);
    TEST_ASSERT(!t->is_isometry, "scale(2,3) not isometry");
    TEST_ASSERT(t->is_orientation_preserving, "scale(2,3) preserves orientation");

    double dx = 0.0, dy = 0.0;
    lv_transform_apply_double(t, 1.0, 1.0, &dx, &dy);
    TEST_ASSERT_DOUBLE(dx, 2.0, 1e-9);
    TEST_ASSERT_DOUBLE(dy, 3.0, 1e-9);
    lv_transform_destroy(t);

    /* 单位缩放：isometry */
    mpq_set_si(sx, 1, 1);
    mpq_set_si(sy, 1, 1);
    t = lv_transform_scale(sx, sy);
    TEST_ASSERT_NOT_NULL(t);
    TEST_ASSERT(t->is_isometry, "scale(1,1) is isometry");
    lv_transform_destroy(t);

    mpq_clear(sx);
    mpq_clear(sy);
    printf("  test_scale_api: PASSED\n");
}

static void test_rotation_double_api(void) {
    /* 绕原点旋转 90°：逆时针 (1,0) -> (0,1)（C-㊺续32 修复方向） */
    lvTransform *t = lv_transform_rotation_double(0.0, 0.0, 3.14159265358979323846 / 2.0);
    TEST_ASSERT_NOT_NULL(t);
    TEST_ASSERT_EQ((int) t->type, (int) TRANSFORM_ROTATION);
    TEST_ASSERT(t->is_isometry, "rotation is isometry");

    double dx = 0.0, dy = 0.0;
    lv_transform_apply_double(t, 1.0, 0.0, &dx, &dy);
    TEST_ASSERT_DOUBLE(dx, 0.0, 1e-9);
    TEST_ASSERT_DOUBLE(dy, 1.0, 1e-9);
    lv_transform_destroy(t);

    /* 绕中心 (1,0) 旋转 180°：(2,0) 关于中心对称到 (0,0) */
    t = lv_transform_rotation_double(1.0, 0.0, 3.14159265358979323846);
    TEST_ASSERT_NOT_NULL(t);
    lv_transform_apply_double(t, 2.0, 0.0, &dx, &dy);
    TEST_ASSERT_DOUBLE(dx, 0.0, 1e-9);
    TEST_ASSERT_DOUBLE(dy, 0.0, 1e-9);
    lv_transform_destroy(t);

    printf("  test_rotation_double_api: PASSED\n");
}

static void test_rotation_arbitrary_api(void) {
    mpq_t cx, cy, cos_a, sin_a;
    mpq_init(cx);
    mpq_init(cy);
    mpq_init(cos_a);
    mpq_init(sin_a);
    mpq_set_si(cx, 0, 1);
    mpq_set_si(cy, 0, 1);
    mpq_set_si(cos_a, 0, 1); /* cos 90° = 0 */
    mpq_set_si(sin_a, 1, 1); /* sin 90° = 1 */

    lvTransform *t = lv_transform_rotation_arbitrary(cx, cy, cos_a, sin_a);
    TEST_ASSERT_NOT_NULL(t);
    TEST_ASSERT_EQ((int) t->type, (int) TRANSFORM_ROTATION);

    /* 90° 逆时针：(1,0) -> (0,1)（C-㊺续32 修复方向，与特殊角版一致） */
    double dx = 0.0, dy = 0.0;
    lv_transform_apply_double(t, 1.0, 0.0, &dx, &dy);
    TEST_ASSERT_DOUBLE(dx, 0.0, 1e-12);
    TEST_ASSERT_DOUBLE(dy, 1.0, 1e-12);
    lv_transform_destroy(t);

    mpq_clear(cx);
    mpq_clear(cy);
    mpq_clear(cos_a);
    mpq_clear(sin_a);
    printf("  test_rotation_arbitrary_api: PASSED\n");
}

static void test_reflection_line_api(void) {
    mpq_t a, b, c;
    mpq_init(a);
    mpq_init(b);
    mpq_init(c);

    /* 直线 x = 0（a=1, b=0, c=0）：(1,1) -> (-1,1) */
    mpq_set_si(a, 1, 1);
    mpq_set_si(b, 0, 1);
    mpq_set_si(c, 0, 1);
    lvTransform *t = lv_transform_reflection_line(a, b, c);
    TEST_ASSERT_NOT_NULL(t);
    TEST_ASSERT_EQ((int) t->type, (int) TRANSFORM_REFLECTION);
    TEST_ASSERT(!t->is_orientation_preserving, "reflection flips orientation");

    double dx = 0.0, dy = 0.0;
    lv_transform_apply_double(t, 1.0, 1.0, &dx, &dy);
    TEST_ASSERT_DOUBLE(dx, -1.0, 1e-12);
    TEST_ASSERT_DOUBLE(dy, 1.0, 1e-12);
    lv_transform_destroy(t);

    /* 直线 y = x（a=1, b=-1, c=0）：(1,2) -> (2,1) */
    mpq_set_si(a, 1, 1);
    mpq_set_si(b, -1, 1);
    mpq_set_si(c, 0, 1);
    t = lv_transform_reflection_line(a, b, c);
    TEST_ASSERT_NOT_NULL(t);
    lv_transform_apply_double(t, 1.0, 2.0, &dx, &dy);
    TEST_ASSERT_DOUBLE(dx, 2.0, 1e-12);
    TEST_ASSERT_DOUBLE(dy, 1.0, 1e-12);
    lv_transform_destroy(t);

    mpq_clear(a);
    mpq_clear(b);
    mpq_clear(c);
    printf("  test_reflection_line_api: PASSED\n");
}

static void test_group_create_preset_api(void) {
    /* D2（Klein 四元群）：2 个反射生成元 */
    lvTransformGroup *g = lv_transform_group_create_preset("D2");
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_EQ(g->generator_count, 2);
    TEST_ASSERT_EQ(g->order, 4);
    lv_transform_group_destroy(g);

    /* C2：1 个生成元 */
    g = lv_transform_group_create_preset("C2");
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_EQ(g->generator_count, 1);
    lv_transform_group_destroy(g);

    /* 未知类型：返回空群（不崩溃） */
    g = lv_transform_group_create_preset("nope");
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_EQ(g->generator_count, 0);
    lv_transform_group_destroy(g);

    /* NULL 契约 */
    TEST_ASSERT_NULL(lv_transform_group_create_preset(NULL));
    printf("  test_group_create_preset_api: PASSED\n");
}

/* ============== 测试入口 ============== */

TEST_MAIN_BEGIN("Lv-00 Geometry Transform Ext Test Suite")
    printf("=== Lv-00 Geometry Transform Ext Test Suite (batch C-㊺续9) ===\n\n");
    lv_init();

    TEST_MAIN_RUN(test_refcount_api);
    TEST_MAIN_RUN(test_apply_api);
    TEST_MAIN_RUN(test_matrix_api);
    TEST_MAIN_RUN(test_sequence_group_api);
    TEST_MAIN_RUN(test_order_reflect_api);
    TEST_MAIN_RUN(test_type_name_api);
    TEST_MAIN_RUN(test_scale_api);
    TEST_MAIN_RUN(test_rotation_double_api);
    TEST_MAIN_RUN(test_rotation_arbitrary_api);
    TEST_MAIN_RUN(test_reflection_line_api);
    TEST_MAIN_RUN(test_group_create_preset_api);

    lv_cleanup();
TEST_MAIN_END()

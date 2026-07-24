/**
 * @file test_geom_transform_presets.c
 * @brief 几何变换预设函数测试
 *
 * 测试 lv_impl_upper.c 中实现的几何变换预设函数：
 * - preset_translate:      平移变换
 * - preset_scale:          缩放变换
 * - preset_affine:         仿射变换
 * - preset_reflect_point:  关于点的反射
 * - preset_identity_transform: 恒等变换
 * - preset_dilate:         位似变换
 * - preset_rotation_about: 绕指定点旋转
 * - preset_rotate:         绕原点旋转
 * - preset_shear_x:        X 方向剪切
 * - preset_shear_y:        Y 方向剪切
 * - preset_glide_reflect:  滑移反射
 *
 * 这些函数在 lv_impl_upper.c 中以外部链接实现（非 static），
 * 使用 symbolic_coord_* API 进行符号坐标计算。
 */

#include <stdio.h>
#include <stdlib.h>

#include "lv.h"

/* ============================================================
 * 前向声明 —— lv_impl_upper.c 中实现的预设变换函数
 * ============================================================ */
int64_t preset_translate(lvEngine *ctx, int64_t obj_id, int64_t dx, int64_t dy);
int64_t preset_scale(lvEngine *ctx, int64_t obj_id, int64_t sx, int64_t sy, int64_t denom);
int64_t preset_affine(lvEngine *ctx, int64_t obj_id, int64_t a11, int64_t a12, int64_t a21, int64_t a22, int64_t tx,
                      int64_t ty, int64_t denom);
int64_t preset_reflect_point(lvEngine *ctx, int64_t obj_id, int64_t center_id);
int64_t preset_identity_transform(lvEngine *ctx);
int64_t preset_dilate(lvEngine *ctx, int64_t obj_id, int64_t center_id, int64_t ratio_num, int64_t ratio_den);
int64_t preset_rotation_about(lvEngine *ctx, int64_t obj_id, int64_t center_id, int64_t angle_mrad);
int64_t preset_rotate(lvEngine *ctx, int64_t obj_id, int64_t angle_mrad);
int64_t preset_shear_x(lvEngine *ctx, int64_t obj_id, int64_t factor, int64_t denom);
int64_t preset_shear_y(lvEngine *ctx, int64_t obj_id, int64_t factor, int64_t denom);
int64_t preset_glide_reflect(lvEngine *ctx, int64_t obj_id, int64_t line_id, int64_t dx, int64_t dy);

/* ============================================================
 * 测试计数器与宏
 * ============================================================ */
static int g_pass = 0, g_fail = 0;

#define TEST(n) printf("  [TEST] %s ... ", n)
#define PASS()            \
    do {                  \
        printf("PASS\n"); \
        g_pass++;         \
    } while (0)
#define FAIL(m)                  \
    do {                         \
        printf("FAIL: %s\n", m); \
        g_fail++;                \
    } while (0)

/* ============================================================
 * 测试 1: preset_translate —— 平移变换
 *
 * 创建点 (0,0)，平移 (dx=3, dy=4)，
 * 验证返回的新点 ID 有效且与原 ID 不同。
 * ============================================================ */
static void test_preset_translate(void) {
    lvEngine *engine = lv_engine_create();
    if (!engine) {
        FAIL("引擎创建失败");
        return;
    }

    int p0 = lv_add_point_i(engine, 0, 0);
    if (p0 < 0) {
        FAIL("添加点失败");
        lv_engine_destroy(engine);
        return;
    }

    int64_t result = preset_translate(engine, (int64_t) p0, 3, 4);
    if (result <= 0) {
        FAIL("preset_translate 返回 ID 应 > 0");
    } else if ((int) result == p0) {
        FAIL("平移结果 ID 应与原 ID 不同");
    } else {
        PASS();
    }

    lv_engine_destroy(engine);
}

/* ============================================================
 * 测试 2: preset_scale —— 缩放变换
 *
 * 创建点 (2,3)，缩放 (sx=3, sy=2, denom=1)，
 * 验证返回的新点 ID 有效。
 * ============================================================ */
static void test_preset_scale(void) {
    lvEngine *engine = lv_engine_create();
    if (!engine) {
        FAIL("引擎创建失败");
        return;
    }

    int p0 = lv_add_point_i(engine, 2, 3);
    if (p0 < 0) {
        FAIL("添加点失败");
        lv_engine_destroy(engine);
        return;
    }

    int64_t result = preset_scale(engine, (int64_t) p0, 3, 2, 1);
    if (result > 0) {
        PASS();
    } else {
        FAIL("preset_scale 返回 ID 应 > 0");
    }

    lv_engine_destroy(engine);
}

/* ============================================================
 * 测试 3: preset_affine —— 仿射变换（90度旋转矩阵）
 *
 * 创建点 (1,0)，应用 90度旋转矩阵：
 *   a11=0, a12=-1, a21=1, a22=0, tx=0, ty=0, denom=1
 *   x' = (0*1 + (-1)*0 + 0) / 1 = 0
 *   y' = (1*1 + 0*0 + 0) / 1 = 1
 * 验证返回的新点 ID 有效。
 * ============================================================ */
static void test_preset_affine(void) {
    lvEngine *engine = lv_engine_create();
    if (!engine) {
        FAIL("引擎创建失败");
        return;
    }

    int p0 = lv_add_point_i(engine, 1, 0);
    if (p0 < 0) {
        FAIL("添加点失败");
        lv_engine_destroy(engine);
        return;
    }

    int64_t result = preset_affine(engine, (int64_t) p0, 0, -1, 1, 0, 0, 0, 1);
    if (result > 0) {
        PASS();
    } else {
        FAIL("preset_affine 返回 ID 应 > 0");
    }

    lv_engine_destroy(engine);
}

/* ============================================================
 * 测试 4: preset_reflect_point —— 关于点的反射
 *
 * 创建点 (2,2) 和中点 (0,0)，
 * 验证反射结果 ID 有效。
 * ============================================================ */
static void test_preset_reflect_point(void) {
    lvEngine *engine = lv_engine_create();
    if (!engine) {
        FAIL("引擎创建失败");
        return;
    }

    int p0 = lv_add_point_i(engine, 2, 2);
    if (p0 < 0) {
        FAIL("添加点失败");
        lv_engine_destroy(engine);
        return;
    }
    int center = lv_add_point_i(engine, 0, 0);
    if (center < 0) {
        FAIL("添加中心点失败");
        lv_engine_destroy(engine);
        return;
    }

    int64_t result = preset_reflect_point(engine, (int64_t) p0, (int64_t) center);
    if (result > 0) {
        PASS();
    } else {
        FAIL("preset_reflect_point 返回 ID 应 > 0");
    }

    lv_engine_destroy(engine);
}

/* ============================================================
 * 测试 5: preset_identity_transform —— 恒等变换
 *
 * 调用 preset_identity_transform(ctx)，
 * 验证返回 ID 有效。
 * ============================================================ */
static void test_preset_identity(void) {
    lvEngine *engine = lv_engine_create();
    if (!engine) {
        FAIL("引擎创建失败");
        return;
    }

    int64_t result = preset_identity_transform(engine);
    if (result > 0) {
        PASS();
    } else {
        FAIL("preset_identity_transform 返回 ID 应 > 0");
    }

    lv_engine_destroy(engine);
}

/* ============================================================
 * 测试 6: preset_dilate —— 位似变换
 *
 * 创建点 (3,4) 和中心 (0,0)，比例 2:1，
 * 验证结果 ID 有效。
 * ============================================================ */
static void test_preset_dilate(void) {
    lvEngine *engine = lv_engine_create();
    if (!engine) {
        FAIL("引擎创建失败");
        return;
    }

    int p0 = lv_add_point_i(engine, 3, 4);
    if (p0 < 0) {
        FAIL("添加点失败");
        lv_engine_destroy(engine);
        return;
    }
    int center = lv_add_point_i(engine, 0, 0);
    if (center < 0) {
        FAIL("添加中心点失败");
        lv_engine_destroy(engine);
        return;
    }

    int64_t result = preset_dilate(engine, (int64_t) p0, (int64_t) center, 2, 1);
    if (result > 0) {
        PASS();
    } else {
        FAIL("preset_dilate 返回 ID 应 > 0");
    }

    lv_engine_destroy(engine);
}

/* ============================================================
 * 测试 7: preset_rotation_about —— 绕指定点旋转
 *
 * 创建点 (1,0) 和中心 (0,0)，旋转 90 度，
 * 验证结果 ID 有效。
 * ============================================================ */
static void test_preset_rotation_about(void) {
    lvEngine *engine = lv_engine_create();
    if (!engine) {
        FAIL("引擎创建失败");
        return;
    }

    int p0 = lv_add_point_i(engine, 1, 0);
    if (p0 < 0) {
        FAIL("添加点失败");
        lv_engine_destroy(engine);
        return;
    }
    int center = lv_add_point_i(engine, 0, 0);
    if (center < 0) {
        FAIL("添加中心点失败");
        lv_engine_destroy(engine);
        return;
    }

    /* 90度 = pi/2 rad ≈ 1570796 mrad（毫弧度） */
    int64_t result = preset_rotation_about(engine, (int64_t) p0, (int64_t) center, 1570796);
    if (result > 0) {
        PASS();
    } else {
        FAIL("preset_rotation_about 返回 ID 应 > 0");
    }

    lv_engine_destroy(engine);
}

/* ============================================================
 * 测试 8: preset_rotate —— 绕原点旋转
 *
 * 创建点 (1,0)，旋转 90 度（约 1570796 mrad），
 * 验证结果 ID 有效。
 * ============================================================ */
static void test_preset_rotate(void) {
    lvEngine *engine = lv_engine_create();
    if (!engine) {
        FAIL("引擎创建失败");
        return;
    }

    int p0 = lv_add_point_i(engine, 1, 0);
    if (p0 < 0) {
        FAIL("添加点失败");
        lv_engine_destroy(engine);
        return;
    }

    int64_t result = preset_rotate(engine, (int64_t) p0, 1570796);
    if (result > 0) {
        PASS();
    } else {
        FAIL("preset_rotate 返回 ID 应 > 0");
    }

    lv_engine_destroy(engine);
}

/* ============================================================
 * 测试 9: preset_shear_x —— X 方向剪切变换
 *
 * 创建点 (2,3)，剪切因子 2，分母 1，
 * 验证结果 ID 有效。
 * ============================================================ */
static void test_preset_shear_x(void) {
    lvEngine *engine = lv_engine_create();
    if (!engine) {
        FAIL("引擎创建失败");
        return;
    }

    int p0 = lv_add_point_i(engine, 2, 3);
    if (p0 < 0) {
        FAIL("添加点失败");
        lv_engine_destroy(engine);
        return;
    }

    int64_t result = preset_shear_x(engine, (int64_t) p0, 2, 1);
    if (result > 0) {
        PASS();
    } else {
        FAIL("preset_shear_x 返回 ID 应 > 0");
    }

    lv_engine_destroy(engine);
}

/* ============================================================
 * 测试 10: preset_shear_y —— Y 方向剪切变换
 *
 * 创建点 (2,3)，剪切因子 2，分母 1，
 * 验证结果 ID 有效。
 * ============================================================ */
static void test_preset_shear_y(void) {
    lvEngine *engine = lv_engine_create();
    if (!engine) {
        FAIL("引擎创建失败");
        return;
    }

    int p0 = lv_add_point_i(engine, 2, 3);
    if (p0 < 0) {
        FAIL("添加点失败");
        lv_engine_destroy(engine);
        return;
    }

    int64_t result = preset_shear_y(engine, (int64_t) p0, 2, 1);
    if (result > 0) {
        PASS();
    } else {
        FAIL("preset_shear_y 返回 ID 应 > 0");
    }

    lv_engine_destroy(engine);
}

/* ============================================================
 * 测试 11: preset_glide_reflect —— 滑移反射
 *
 * 创建点 (1,0) 和线段（两个端点），
 * 滑移反射验证结果 ID 有效。
 * ============================================================ */
static void test_preset_glide_reflect(void) {
    lvEngine *engine = lv_engine_create();
    if (!engine) {
        FAIL("引擎创建失败");
        return;
    }

    int p0 = lv_add_point_i(engine, 1, 0);
    if (p0 < 0) {
        FAIL("添加点失败");
        lv_engine_destroy(engine);
        return;
    }

    /* 创建一条线段（作为反射轴） */
    int l0 = lv_add_point_i(engine, 0, 0);
    if (l0 < 0) {
        FAIL("添加线段端点失败");
        lv_engine_destroy(engine);
        return;
    }
    int l1 = lv_add_point_i(engine, 1, 1);
    if (l1 < 0) {
        FAIL("添加线段端点失败");
        lv_engine_destroy(engine);
        return;
    }

    if (lv_add_line_segment(engine, l0, l1) < 0) {
        FAIL("添加线段失败");
        lv_engine_destroy(engine);
        return;
    }

    int64_t result = preset_glide_reflect(engine, (int64_t) p0, (int64_t) l0, 2, 3);
    if (result > 0) {
        PASS();
    } else {
        FAIL("preset_glide_reflect 返回 ID 应 > 0");
    }

    lv_engine_destroy(engine);
}

/* ============================================================
 * Main
 * ============================================================ */
int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== 几何变换预设函数测试 ===\n\n");

    if (!lv_init()) {
        fprintf(stderr, "lv_init 失败\n");
        return 1;
    }

    printf("[组 1] 平移变换\n");
    test_preset_translate();

    printf("[组 2] 缩放变换\n");
    test_preset_scale();

    printf("[组 3] 仿射变换（90度旋转）\n");
    test_preset_affine();

    printf("[组 4] 关于点的反射\n");
    test_preset_reflect_point();

    printf("[组 5] 恒等变换\n");
    test_preset_identity();

    printf("[组 6] 位似变换\n");
    test_preset_dilate();

    printf("[组 7] 绕指定点旋转\n");
    test_preset_rotation_about();

    printf("[组 8] 绕原点旋转\n");
    test_preset_rotate();

    printf("[组 9] X 方向剪切\n");
    test_preset_shear_x();

    printf("[组 10] Y 方向剪切\n");
    test_preset_shear_y();

    printf("[组 11] 滑移反射\n");
    test_preset_glide_reflect();

    lv_cleanup();
    printf("\n=== %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}

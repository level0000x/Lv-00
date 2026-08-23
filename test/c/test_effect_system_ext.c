/**
 * @file test_effect_system_ext.c
 * @brief 副作用系统契约测试（批次 C-㊺续25：effect_system.h 10 个零覆盖 API）
 *
 * 覆盖零覆盖 API：
 *   追踪器：tracker_create / destroy / reset / record / has_effect /
 *     is_pure / current
 *   组合：compose / annotation_destroy
 *   检查规则：check_geometry_pure
 *
 * 契约要点（与实现核对）：
 *   - create：分配追踪器（entries reserve 64）。
 *   - record：追加日志条目（desc 截断到 256）。
 *   - has_effect：按类型存在查询。
 *   - is_pure：entries 空 → 1；NULL → 1。
 *   - current：current_effect 初始 NULL；tracker 层不自动维护
 *     （compose 结果需手动关联）。
 *   - compose：a/b 均 NULL → NULL；合并效果数组（拷贝拼接）；总 count 0
 *     → NULL。
 *   - check_geometry_pure：所有 entry 为 PURE → 1，否则 0。
 *   - destroy 均 NULL 安全。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/effect_system.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：追踪器生命周期 ============== */

static void test_effect_tracker_lifecycle_api(void) {
    lvEffectTracker *t = lv_effect_tracker_create();
    TEST_ASSERT_NOT_NULL(t);

    /* 初始纯净 */
    TEST_ASSERT(lv_effect_tracker_is_pure(t), "初始纯净");
    TEST_ASSERT_NULL(lv_effect_tracker_current(t));
    TEST_ASSERT(!lv_effect_tracker_has_effect(t, lv_EFFECT_FILE_READ), "初始无副作用");

    /* record 后不再纯净 */
    lv_effect_tracker_record(t, lv_EFFECT_FILE_READ, 42, "read config");
    TEST_ASSERT(!lv_effect_tracker_is_pure(t), "记录后非纯净");
    TEST_ASSERT(lv_effect_tracker_has_effect(t, lv_EFFECT_FILE_READ), "含 FILE_READ");
    TEST_ASSERT(!lv_effect_tracker_has_effect(t, lv_EFFECT_NETWORK), "不含 NETWORK");

    /* 记录多种副作用 */
    lv_effect_tracker_record(t, lv_EFFECT_NETWORK, 43, "send");
    lv_effect_tracker_record(t, lv_EFFECT_RANDOM, 44, NULL); /* desc NULL 安全 */
    TEST_ASSERT(lv_effect_tracker_has_effect(t, lv_EFFECT_NETWORK), "含 NETWORK");
    TEST_ASSERT(lv_effect_tracker_has_effect(t, lv_EFFECT_RANDOM), "含 RANDOM");

    /* reset 清空 */
    lv_effect_tracker_reset(t);
    TEST_ASSERT(lv_effect_tracker_is_pure(t), "reset 后纯净");
    TEST_ASSERT(!lv_effect_tracker_has_effect(t, lv_EFFECT_NETWORK), "reset 后无副作用");

    /* NULL 契约 */
    lv_effect_tracker_record(NULL, lv_EFFECT_TIME, 1, "x");
    lv_effect_tracker_reset(NULL);
    lv_effect_tracker_destroy(NULL);
    TEST_ASSERT(lv_effect_tracker_is_pure(NULL), "NULL 视为纯净");
    TEST_ASSERT(!lv_effect_tracker_has_effect(NULL, lv_EFFECT_TIME), "NULL 无副作用");
    TEST_ASSERT_NULL(lv_effect_tracker_current(NULL));

    lv_effect_tracker_destroy(t);
    printf("  test_effect_tracker_lifecycle_api: PASSED\n");
}

/* ============== 测试：组合 ============== */

static void test_effect_compose_api(void) {
    /* 构造两个注解 */
    lvEffectType types_a[] = {lv_EFFECT_FILE_READ, lv_EFFECT_NETWORK};
    lvEffectAnnotation a = {types_a, 2};

    lvEffectType types_b[] = {lv_EFFECT_RANDOM};
    lvEffectAnnotation b = {types_b, 1};

    /* compose：合并 3 个 */
    lvEffectAnnotation *c = lv_effect_compose(&a, &b);
    TEST_ASSERT_NOT_NULL(c);
    TEST_ASSERT_EQ(c->effect_count, 3);
    TEST_ASSERT_EQ((int) c->effects[0], (int) lv_EFFECT_FILE_READ);
    TEST_ASSERT_EQ((int) c->effects[1], (int) lv_EFFECT_NETWORK);
    TEST_ASSERT_EQ((int) c->effects[2], (int) lv_EFFECT_RANDOM);
    lv_effect_annotation_destroy(c);

    /* 单侧 NULL */
    c = lv_effect_compose(&a, NULL);
    TEST_ASSERT_NOT_NULL(c);
    TEST_ASSERT_EQ(c->effect_count, 2);
    lv_effect_annotation_destroy(c);
    c = lv_effect_compose(NULL, &b);
    TEST_ASSERT_NOT_NULL(c);
    TEST_ASSERT_EQ(c->effect_count, 1);
    lv_effect_annotation_destroy(c);

    /* 双 NULL → NULL */
    TEST_ASSERT_NULL(lv_effect_compose(NULL, NULL));

    /* 空注解组合：一侧空 + 一侧非空 → 非 NULL（count 总和 > 0） */
    lvEffectAnnotation empty = {NULL, 0};
    c = lv_effect_compose(&empty, &a);
    TEST_ASSERT_NOT_NULL(c);
    TEST_ASSERT_EQ(c->effect_count, 2);
    lv_effect_annotation_destroy(c);

    /* 双空注解 → NULL */
    lvEffectAnnotation empty2 = {NULL, 0};
    TEST_ASSERT_NULL(lv_effect_compose(&empty, &empty2));

    /* annotation_destroy NULL 安全 */
    lv_effect_annotation_destroy(NULL);
    printf("  test_effect_compose_api: PASSED\n");
}

/* ============== 测试：几何纯净检查 ============== */

static void test_effect_geometry_pure_api(void) {
    lvEffectTracker *t = lv_effect_tracker_create();

    /* 空追踪器 → 纯净 */
    TEST_ASSERT(lv_effect_check_geometry_pure(t), "空追踪器纯净");
    TEST_ASSERT(lv_effect_check_geometry_pure(NULL), "NULL 纯净");

    /* 纯副作用记录 → 纯净 */
    lv_effect_tracker_record(t, lv_EFFECT_PURE, 1, "pure op");
    TEST_ASSERT(lv_effect_check_geometry_pure(t), "PURE 记录仍纯净");

    /* 非纯副作用 → 非纯净 */
    lv_effect_tracker_record(t, lv_EFFECT_UI_RENDER, 2, "render");
    TEST_ASSERT(!lv_effect_check_geometry_pure(t), "UI_RENDER 非纯净");

    /* reset 恢复纯净 */
    lv_effect_tracker_reset(t);
    TEST_ASSERT(lv_effect_check_geometry_pure(t), "reset 恢复纯净");

    lv_effect_tracker_destroy(t);
    printf("  test_effect_geometry_pure_api: PASSED\n");
}

/* ============== 测试入口 ============== */

TEST_MAIN_BEGIN("Lv-00 Effect System Ext Test Suite")
    printf("=== Lv-00 Effect System Ext Test Suite (batch C-㊺续25) ===\n\n");
    lv_init();

    TEST_MAIN_RUN(test_effect_tracker_lifecycle_api);
    TEST_MAIN_RUN(test_effect_compose_api);
    TEST_MAIN_RUN(test_effect_geometry_pure_api);

    lv_cleanup();
TEST_MAIN_END()

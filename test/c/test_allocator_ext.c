/**
 * @file test_allocator_ext.c
 * @brief 可替换分配器契约测试（批次 C-㊺续34：allocator.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（5 个）：
 *   lv_allocator_debug / get / raw / reset / set
 *
 * 契约要点（与 allocator.c 核对）：
 *   - get：当前分配器（初始为调试分配器），永不 NULL。
 *   - raw/debug：返回静态实例（name 分别为 "raw"/"debug"）。
 *   - set：返回切换前的分配器；ops NULL 或 alloc/free 缺失时拒绝切换；
 *     切换后 get 返回新分配器。
 *   - reset：切回调试分配器。
 *
 * 注意：测试全程不做内存分配（避免在非调试分配器下产生混合分配），
 * 且末尾保证恢复到调试分配器。
 *
 * @author Lv-00 Project
 */

#include <stdio.h>
#include <string.h>

#include "lv/allocator.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：获取分配器 ============== */

static void test_get_raw_debug(void) {
    /* get 永不 NULL */
    const AllocatorOps *cur = lv_allocator_get();
    TEST_ASSERT_NOT_NULL(cur);

    /* raw/debug 实例 */
    const AllocatorOps *raw = lv_allocator_raw();
    const AllocatorOps *dbg = lv_allocator_debug();
    TEST_ASSERT_NOT_NULL(raw);
    TEST_ASSERT_NOT_NULL(dbg);
    TEST_ASSERT_STR_EQ(raw->name, "raw");
    TEST_ASSERT_STR_EQ(dbg->name, "debug");
    TEST_ASSERT_NOT_NULL(raw->alloc);
    TEST_ASSERT_NOT_NULL(raw->free);
    TEST_ASSERT_NOT_NULL(dbg->alloc);
    TEST_ASSERT_NOT_NULL(dbg->free);
}

/* ============== 测试：切换与恢复 ============== */

static void test_set_reset(void) {
    const AllocatorOps *orig = lv_allocator_get();

    /* set(NULL)：仅返回当前，不切换 */
    TEST_ASSERT(lv_allocator_set(NULL) == orig, "set(NULL) returns current");
    TEST_ASSERT(lv_allocator_get() == orig, "no switch on NULL");

    /* 切换到 raw */
    const AllocatorOps *prev = lv_allocator_set(lv_allocator_raw());
    TEST_ASSERT(prev == orig, "set returns previous");
    TEST_ASSERT(lv_allocator_get() == lv_allocator_raw(), "switched to raw");

    /* 切回 debug */
    prev = lv_allocator_set(lv_allocator_debug());
    TEST_ASSERT(prev == lv_allocator_raw(), "set returns previous 2");
    TEST_ASSERT(lv_allocator_get() == lv_allocator_debug(), "switched to debug");

    /* reset 切回调试 */
    lv_allocator_reset();
    TEST_ASSERT(lv_allocator_get() == lv_allocator_debug(), "reset to debug");
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("AllocatorExt")

    printf("\n--- allocator (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_get_raw_debug);
    TEST_MAIN_RUN(test_set_reset);

TEST_MAIN_END()

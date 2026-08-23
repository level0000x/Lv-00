/**
 * @file test_lv_stream_context_ext.c
 * @brief 流式输出上下文契约测试（批次 C-㊺续31：lv_stream_context.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（3 个）：
 *   lv_context_get_stream / set_streaming_enabled / is_streaming_enabled
 *
 * 契约要点（与 lv_stream_context.h / lv_stream_context.c 核对）：
 *   - get_stream：NULL 返回 NULL；首次调用惰性创建 StreamContext，后续返回同一实例。
 *   - set_streaming_enabled：NULL 安全；true 时未创建则创建；false 时销毁并置 NULL。
 *   - is_streaming_enabled：NULL 返回 false；以 stream_ctx 非 NULL 判定。
 *
 * 说明：lvContext 为公开结构（context.h），栈上零初始化即可模拟
 * stream_ctx == NULL 的前置状态，不依赖 lv_context_create 的副作用。
 *
 * @author Lv-00 Project
 */

#include <stdio.h>
#include <string.h>

#include "lv/context.h"
#include "lv/lv_stream_context.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：NULL 契约 ============== */

static void test_null_contract(void) {
    TEST_ASSERT_NULL(lv_context_get_stream(NULL));
    TEST_ASSERT(!lv_context_is_streaming_enabled(NULL), "NULL ctx disabled");
    lv_context_set_streaming_enabled(NULL, true);  /* 不崩 */
    lv_context_set_streaming_enabled(NULL, false); /* 不崩 */
}

/* ============== 测试：惰性创建与同实例 ============== */

static void test_lazy_create_same_instance(void) {
    lvContext ctx;
    memset(&ctx, 0, sizeof(ctx));

    /* 初始未启用 */
    TEST_ASSERT(!lv_context_is_streaming_enabled(&ctx), "initially disabled");

    /* 首次 get_stream 惰性创建；再次调用返回同一实例 */
    struct StreamContext *s1 = lv_context_get_stream(&ctx);
    TEST_ASSERT_NOT_NULL(s1);
    struct StreamContext *s2 = lv_context_get_stream(&ctx);
    TEST_ASSERT(s1 == s2, "same instance on repeated get");
    TEST_ASSERT(lv_context_is_streaming_enabled(&ctx), "enabled after lazy create");

    /* 清理：禁用销毁 stream_ctx */
    lv_context_set_streaming_enabled(&ctx, false);
    TEST_ASSERT(!lv_context_is_streaming_enabled(&ctx), "disabled after cleanup");
}

/* ============== 测试：启用/禁用往返 ============== */

static void test_enable_disable_roundtrip(void) {
    lvContext ctx;
    memset(&ctx, 0, sizeof(ctx));

    /* 显式启用创建 */
    lv_context_set_streaming_enabled(&ctx, true);
    TEST_ASSERT(lv_context_is_streaming_enabled(&ctx), "enabled after explicit enable");
    TEST_ASSERT_NOT_NULL(lv_context_get_stream(&ctx));

    /* 禁用销毁 */
    lv_context_set_streaming_enabled(&ctx, false);
    TEST_ASSERT(!lv_context_is_streaming_enabled(&ctx), "disabled");

    /* 再次启用重新创建 */
    lv_context_set_streaming_enabled(&ctx, true);
    TEST_ASSERT(lv_context_is_streaming_enabled(&ctx), "re-enabled");
    TEST_ASSERT_NOT_NULL(lv_context_get_stream(&ctx));

    /* 最终清理 */
    lv_context_set_streaming_enabled(&ctx, false);
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("StreamContextExt")

    printf("\n--- lv_stream_context (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_null_contract);
    TEST_MAIN_RUN(test_lazy_create_same_instance);
    TEST_MAIN_RUN(test_enable_disable_roundtrip);

TEST_MAIN_END()

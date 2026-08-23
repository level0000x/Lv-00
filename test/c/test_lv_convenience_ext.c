/**
 * @file test_lv_convenience_ext.c
 * @brief 高层便捷 API 契约测试（批次 C-㊺续34：lv_convenience.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（4 个）：
 *   lv_prove / lv_preset_load / lv_preset_unload / lv_preset_apply
 *
 * 契约要点（与 lv_convenience.c 核对）：
 *   - 参数无效（ctx/name NULL、空名）→ -1。
 *   - prove：上下文状态非 IDLE/COMPLETE → -2。
 *   - preset_load/apply/unload：未加载/不存在 → -3（apply 状态不合法 → -2）。
 *
 * @author Lv-00 Project
 */

#include <stdio.h>
#include <string.h>

#include "lv/context.h"
#include "lv/lv_convenience.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：参数契约 ============== */

static void test_param_contract(void) {
    /* ctx NULL */
    TEST_ASSERT_EQ(lv_prove(NULL, "x"), -1);
    TEST_ASSERT_EQ(lv_preset_load(NULL, "m"), -1);
    TEST_ASSERT_EQ(lv_preset_unload(NULL, "m"), -1);
    TEST_ASSERT_EQ(lv_preset_apply(NULL, "m"), -1);

    lvContext ctx;
    memset(&ctx, 0, sizeof(ctx));

    /* name NULL / 空 */
    TEST_ASSERT_EQ(lv_preset_load(&ctx, NULL), -1);
    TEST_ASSERT_EQ(lv_preset_load(&ctx, ""), -1);
    TEST_ASSERT_EQ(lv_preset_unload(&ctx, NULL), -1);
    TEST_ASSERT_EQ(lv_preset_unload(&ctx, ""), -1);
    TEST_ASSERT_EQ(lv_preset_apply(&ctx, NULL), -1);
    TEST_ASSERT_EQ(lv_preset_apply(&ctx, ""), -1);
}

/* ============== 测试：未加载/状态契约 ============== */

static void test_unloaded_and_state(void) {
    lvContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.state = lv_CONTEXT_IDLE;

    /* 未加载的预设：apply → -3、unload → -3 */
    TEST_ASSERT_EQ(lv_preset_apply(&ctx, "no-such-preset"), -3);
    TEST_ASSERT_EQ(lv_preset_unload(&ctx, "no-such-preset"), -3);

    /* prove：非 IDLE/COMPLETE 状态 → -2 */
    ctx.state = lv_CONTEXT_REASONING;
    TEST_ASSERT_EQ(lv_prove(&ctx, "x"), -2);
}

/* ============== 测试：prove 正常路径（不崩溃） ============== */

static void test_prove_runs(void) {
    lvContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.state = lv_CONTEXT_IDLE;

    /* goal 文本解析："x" 不是合法 DSL → -3 解析失败；或空图证明成功。
     * 无论结果如何，调用必须不崩溃且返回值落在 [-4, 0]。 */
    int rc = lv_prove(&ctx, "x");
    TEST_ASSERT(rc >= -4 && rc <= 0, "prove returns documented range");

    /* goal NULL：使用上下文空图证明（不崩溃） */
    ctx.state = lv_CONTEXT_IDLE;
    rc = lv_prove(&ctx, NULL);
    TEST_ASSERT(rc >= -4 && rc <= 0, "prove NULL goal runs");
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("ConvenienceExt")

    printf("\n--- lv_convenience (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_param_contract);
    TEST_MAIN_RUN(test_unloaded_and_state);
    TEST_MAIN_RUN(test_prove_runs);

TEST_MAIN_END()

/**
 * @file test_ecosystem_ext.c
 * @brief 插件生态系统契约测试（批次 C-㊺续33：ecosystem.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（3 个）：
 *   lv_ecosystem_init / register_module / shutdown
 *
 * 契约要点（与 ecosystem.c 核对）：
 *   - init：幂等（重复调用返回 0）。
 *   - register_module：未 init / name NULL / layer 越界（<1 或 >10）/
 *     重复名称 → -1；成功 0。
 *   - shutdown：清空注册表；之后 register 被拒（未初始化）。
 *
 * 说明：本测试进程内自包含，末尾 shutdown 恢复全局状态。
 *
 * @author Lv-00 Project
 */

#include <stdio.h>

#include "lv/ecosystem.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：init/register/shutdown 生命周期 ============== */

static void test_lifecycle(void) {
    /* init 幂等 */
    TEST_ASSERT_EQ(lv_ecosystem_init(), 0);
    TEST_ASSERT_EQ(lv_ecosystem_init(), 0);

    /* 注册 */
    TEST_ASSERT_EQ(lv_ecosystem_register_module("m1", 1), 0);
    TEST_ASSERT_EQ(lv_ecosystem_register_module("m2", 10), 0);

    /* 重复名称拒绝 */
    TEST_ASSERT_EQ(lv_ecosystem_register_module("m1", 2), -1);

    /* 参数契约 */
    TEST_ASSERT_EQ(lv_ecosystem_register_module(NULL, 1), -1);
    TEST_ASSERT_EQ(lv_ecosystem_register_module("m3", 0), -1);
    TEST_ASSERT_EQ(lv_ecosystem_register_module("m4", 11), -1);

    /* shutdown 清空 */
    lv_ecosystem_shutdown();
    TEST_ASSERT_EQ(lv_ecosystem_module_count(), 0);

    /* shutdown 后未初始化：register 被拒 */
    TEST_ASSERT_EQ(lv_ecosystem_register_module("m5", 1), -1);

    /* 可重新 init */
    TEST_ASSERT_EQ(lv_ecosystem_init(), 0);
    TEST_ASSERT_EQ(lv_ecosystem_register_module("m6", 5), 0);
    lv_ecosystem_shutdown();
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("EcosystemExt")

    printf("\n--- ecosystem (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_lifecycle);

TEST_MAIN_END()

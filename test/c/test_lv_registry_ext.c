/**
 * @file test_lv_registry_ext.c
 * @brief 注册表清空与模块生命周期契约测试（批次 C-㊺续30：lv_registry.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（4 个）：
 *   lv_registry_clear
 *   lv_module_init_all / cleanup_all / count
 *
 * 契约要点（与 lv_registry.h / lv_registry.c 核对）：
 *   - clear：调用各条目 destroy 回调并释放 name，但保留 entries 数组与互斥锁，
 *     可幂等多次调用，清空后仍可继续 put（无需重新 init）；与 destroy 的区别是
 *     不销毁互斥锁。
 *   - 模块注册表为进程级单例：lv_init() 内部注册 context_resources/interop_plugins，
 *     并调用 lv_module_init_all()。本测试不调用 lv_init()，因此注册表仅包含本测试
 *     注册的模块，init_all/cleanup_all 的调用与断言不受真实模块干扰。
 *   - init_all：按优先级升序（CORE < RESOURCE < OUTPUT）调用各模块 init；
 *     任一 init 返回 false 则整体失败。
 *   - cleanup_all：按优先级降序（反向）调用 cleanup。
 *   - count：返回已注册模块数，不受 init_all/cleanup_all 影响。
 *
 * @author Lv-00 Project
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "lv/lv_registry.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============================================================
 * 注册表 clear 测试
 * ============================================================ */

static int g_destroy_count = 0;

static void test_destroy_fn(void *value) {
    (void) value;
    g_destroy_count++;
}

static void test_registry_clear_api(void) {
    lvRegistry reg;
    lv_registry_init(&reg, 2);

    g_destroy_count = 0;
    TEST_ASSERT(lv_registry_put_ex(&reg, "k1", (void *) (intptr_t) 0x11, test_destroy_fn), "put k1");
    TEST_ASSERT(lv_registry_put_ex(&reg, "k2", (void *) (intptr_t) 0x22, test_destroy_fn), "put k2");
    TEST_ASSERT(lv_registry_put(&reg, "k3", (void *) (intptr_t) 0x33), "put k3 (no destroy)");
    TEST_ASSERT_EQ(lv_registry_count(&reg), 3);

    /* clear：释放条目与 name，调用 destroy，保留数组与锁 */
    lv_registry_clear(&reg);
    TEST_ASSERT_EQ(lv_registry_count(&reg), 0);
    TEST_ASSERT_EQ(g_destroy_count, 2);
    TEST_ASSERT_NULL(lv_registry_get(&reg, "k1"));
    TEST_ASSERT_NULL(lv_registry_get(&reg, "k3"));

    /* 幂等：再次 clear 不崩溃，计数不变 */
    lv_registry_clear(&reg);
    TEST_ASSERT_EQ(lv_registry_count(&reg), 0);
    TEST_ASSERT_EQ(g_destroy_count, 2);

    /* 清空后可直接 put（数组与锁保留），无需重新 init */
    TEST_ASSERT(lv_registry_put(&reg, "re1", (void *) (intptr_t) 0x55), "put after clear");
    TEST_ASSERT_EQ(lv_registry_count(&reg), 1);
    TEST_ASSERT_EQ((intptr_t) lv_registry_get(&reg, "re1"), (intptr_t) 0x55);

    /* NULL 安全 */
    lv_registry_clear(NULL);

    lv_registry_destroy(&reg);
}

/* ============================================================
 * 模块生命周期测试（不调用 lv_init：注册表仅含测试模块）
 * ============================================================ */

/* 调用顺序记录：写入优先级值（0=CORE, 1=RESOURCE, 4=OUTPUT） */
static int g_init_events[8];
static int g_init_n = 0;
static int g_cleanup_events[8];
static int g_cleanup_n = 0;

static bool mod_init_core(void) {
    g_init_events[g_init_n++] = lv_MODULE_PRIO_CORE;
    return true;
}
static void mod_cleanup_core(void) {
    g_cleanup_events[g_cleanup_n++] = lv_MODULE_PRIO_CORE;
}

static bool mod_init_res(void) {
    g_init_events[g_init_n++] = lv_MODULE_PRIO_RESOURCE;
    return true;
}
static void mod_cleanup_res(void) {
    g_cleanup_events[g_cleanup_n++] = lv_MODULE_PRIO_RESOURCE;
}

static bool mod_init_out(void) {
    g_init_events[g_init_n++] = lv_MODULE_PRIO_OUTPUT;
    return true;
}
static void mod_cleanup_out(void) {
    g_cleanup_events[g_cleanup_n++] = lv_MODULE_PRIO_OUTPUT;
}

/* 失败模块：init 返回 false */
static bool mod_init_fail(void) {
    return false;
}

static void test_module_lifecycle_api(void) {
    /* 注册 3 个不同优先级的模块 */
    TEST_ASSERT(lv_module_register("t_core", mod_init_core, mod_cleanup_core, lv_MODULE_PRIO_CORE),
                "register t_core");
    TEST_ASSERT(lv_module_register("t_res", mod_init_res, mod_cleanup_res, lv_MODULE_PRIO_RESOURCE),
                "register t_res");
    TEST_ASSERT(lv_module_register("t_out", mod_init_out, mod_cleanup_out, lv_MODULE_PRIO_OUTPUT),
                "register t_out");

    /* J1/F28：重复名称改为幂等 upsert（更新条目而非拒绝——
     * 修复 init/cleanup 循环后"重复注册被吞"）；NULL 名称仍拒绝 */
    TEST_ASSERT(lv_module_register("t_core", mod_init_core, mod_cleanup_core, lv_MODULE_PRIO_CORE),
                "duplicate upsert succeeds");
    TEST_ASSERT(!lv_module_register(NULL, mod_init_core, mod_cleanup_core, lv_MODULE_PRIO_CORE),
                "NULL name rejected");

    /* 计数 = 3（本进程未 lv_init，注册表仅测试模块） */
    TEST_ASSERT_EQ(lv_module_count(), 3);

    /* init_all：按优先级升序调用 init（CORE -> RESOURCE -> OUTPUT），返回 true */
    g_init_n = 0;
    TEST_ASSERT(lv_module_init_all(), "init all true");
    TEST_ASSERT_EQ(g_init_n, 3);
    TEST_ASSERT_EQ(g_init_events[0], (int) lv_MODULE_PRIO_CORE);
    TEST_ASSERT_EQ(g_init_events[1], (int) lv_MODULE_PRIO_RESOURCE);
    TEST_ASSERT_EQ(g_init_events[2], (int) lv_MODULE_PRIO_OUTPUT);

    /* cleanup_all：按反向优先级清理（OUTPUT -> RESOURCE -> CORE） */
    g_cleanup_n = 0;
    lv_module_cleanup_all();
    TEST_ASSERT_EQ(g_cleanup_n, 3);
    TEST_ASSERT_EQ(g_cleanup_events[0], (int) lv_MODULE_PRIO_OUTPUT);
    TEST_ASSERT_EQ(g_cleanup_events[1], (int) lv_MODULE_PRIO_RESOURCE);
    TEST_ASSERT_EQ(g_cleanup_events[2], (int) lv_MODULE_PRIO_CORE);

    /* init_all/cleanup_all 不影响注册计数 */
    TEST_ASSERT_EQ(lv_module_count(), 3);

    /* J1/F28：cleanup_all 后 registry_reset 清空 count，可重新注册 */
    lv_module_registry_reset();
    TEST_ASSERT_EQ(lv_module_count(), 0);
    TEST_ASSERT(lv_module_register("t_after_reset", mod_init_core, mod_cleanup_core, lv_MODULE_PRIO_CORE),
                "register after reset");
    TEST_ASSERT_EQ(lv_module_count(), 1);

    /* init 失败 -> init_all 返回 false */
    TEST_ASSERT(lv_module_register("t_fail", mod_init_fail, NULL, lv_MODULE_PRIO_LATE), "register t_fail");
    TEST_ASSERT_EQ(lv_module_count(), 2);
    TEST_ASSERT(!lv_module_init_all(), "init_all false on failing module");
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("RegistryExt")

    printf("\n--- lv_registry clear ---\n");
    TEST_MAIN_RUN(test_registry_clear_api);

    printf("\n--- module lifecycle ---\n");
    TEST_MAIN_RUN(test_module_lifecycle_api);

TEST_MAIN_END()

/**
 * @file test_lv_backend_plugin_ext.c
 * @brief 后端插件注册表契约测试（批次 C-㊺续35：lv_backend_plugin.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（7 个）：
 *   lv_backend_plugin_registry_init / cleanup
 *   lv_backend_plugin_register / unregister / find
 *   lv_backend_plugin_init_all / cleanup_all
 *
 * 契约要点（与 lv_backend_plugin.c 核对）：
 *   - register：NULL 参数/重复名 → false；成功 true。
 *   - unregister：NULL → false；找到 → true（末元素前移）。
 *   - find：NULL → NULL；按名查找。
 *   - init_all/cleanup_all：按优先级调用回调；NULL 安全。
 *   - cleanup：NULL 安全。
 *
 * @author Lv-00 Project
 */

#include <stdio.h>
#include <string.h>

#include "lv/lv_backend_plugin.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试辅助：生命周期回调计数 ============== */

static int g_init_count = 0;
static int g_cleanup_count = 0;

static bool fake_init(void) {
    g_init_count++;
    return true;
}
static void fake_cleanup(void) {
    g_cleanup_count++;
}

static void make_plugin(lvBackendPlugin *p, const char *name, int priority) {
    memset(p, 0, sizeof(*p));
    p->name = name;
    p->version = "1.0";
    p->type = lv_PLUGIN_TYPE_SMT;
    p->priority = priority;
    p->init = fake_init;
    p->cleanup = fake_cleanup;
}

/* ============== 测试：注册/查找/注销 ============== */

static void test_register_find_unregister(void) {
    lvBackendPluginRegistry reg;
    lv_backend_plugin_registry_init(&reg);
    TEST_ASSERT_EQ(lv_backend_plugin_count(&reg), 0);

    lvBackendPlugin p1, p2;
    make_plugin(&p1, "plugin-a", 1);
    make_plugin(&p2, "plugin-b", 2);

    /* 注册 */
    TEST_ASSERT(lv_backend_plugin_register(&reg, &p1), "register a");
    TEST_ASSERT(lv_backend_plugin_register(&reg, &p2), "register b");
    TEST_ASSERT_EQ(lv_backend_plugin_count(&reg), 2);

    /* 重复名拒绝 */
    TEST_ASSERT(!lv_backend_plugin_register(&reg, &p1), "duplicate rejected");

    /* 查找 */
    TEST_ASSERT(lv_backend_plugin_find(&reg, "plugin-a") == &p1, "find a");
    TEST_ASSERT(lv_backend_plugin_find(&reg, "plugin-b") == &p2, "find b");
    TEST_ASSERT_NULL(lv_backend_plugin_find(&reg, "nope"));

    /* 注销 */
    TEST_ASSERT(lv_backend_plugin_unregister(&reg, "plugin-a"), "unregister a");
    TEST_ASSERT(!lv_backend_plugin_unregister(&reg, "plugin-a"), "unregister a again");
    TEST_ASSERT_EQ(lv_backend_plugin_count(&reg), 1);
    TEST_ASSERT_NULL(lv_backend_plugin_find(&reg, "plugin-a"));

    /* NULL 契约 */
    TEST_ASSERT(!lv_backend_plugin_register(NULL, &p1), "register NULL reg");
    TEST_ASSERT(!lv_backend_plugin_register(&reg, NULL), "register NULL plugin");
    TEST_ASSERT(!lv_backend_plugin_unregister(NULL, "x"), "unregister NULL reg");
    TEST_ASSERT(!lv_backend_plugin_unregister(&reg, NULL), "unregister NULL name");
    TEST_ASSERT_NULL(lv_backend_plugin_find(NULL, "x"));
    TEST_ASSERT_NULL(lv_backend_plugin_find(&reg, NULL));
    TEST_ASSERT_EQ(lv_backend_plugin_count(NULL), 0);

    lv_backend_plugin_registry_cleanup(&reg);
    lv_backend_plugin_registry_cleanup(NULL);
    lv_backend_plugin_registry_init(NULL);
}

/* ============== 测试：批量生命周期 ============== */

static void test_init_cleanup_all(void) {
    lvBackendPluginRegistry reg;
    lv_backend_plugin_registry_init(&reg);

    g_init_count = 0;
    g_cleanup_count = 0;

    lvBackendPlugin p1, p2;
    make_plugin(&p1, "init-a", 1);
    make_plugin(&p2, "init-b", 2);
    TEST_ASSERT(lv_backend_plugin_register(&reg, &p1), "register a");
    TEST_ASSERT(lv_backend_plugin_register(&reg, &p2), "register b");

    /* init_all：两个回调都被调用 */
    lv_backend_plugin_init_all(&reg);
    TEST_ASSERT_EQ(g_init_count, 2);

    /* cleanup_all：反向清理 */
    lv_backend_plugin_cleanup_all(&reg);
    TEST_ASSERT_EQ(g_cleanup_count, 2);

    /* NULL 安全 */
    lv_backend_plugin_init_all(NULL);
    lv_backend_plugin_cleanup_all(NULL);

    lv_backend_plugin_registry_cleanup(&reg);
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("BackendPluginExt")

    printf("\n--- lv_backend_plugin (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_register_find_unregister);
    TEST_MAIN_RUN(test_init_cleanup_all);

TEST_MAIN_END()

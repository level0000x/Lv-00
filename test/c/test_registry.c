/**
 * @file test_registry.c
 * @brief 泛型注册表设施（lv_registry）单元测试
 *
 * 覆盖：
 * - put/get/remove 生命周期与遍历
 * - 重复注册行为（与旧语义一致：重复返回 false）
 * - destroy 回调在 remove / destroy 时被调用
 * - 现有 name→factory 兼容 API（register/create/find）仍工作
 * - 迁移后插件接口注册/注销行为（lv_plugin_register_interface 等）
 * - 迁移后插件配置 set/get 行为（lv_plugin_config_set/get）
 * - 迁移后几何事件注册行为（geo_event_register）
 *
 * @author Lv-00 Project
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/geo_event_detect.h"
#include "lv/lv_registry.h"
#include "lv/plugin_system.h"

#include "lv.h"
#include "test_helpers.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============================================================
 * 测试辅助
 * ============================================================ */

/** @brief destroy 回调调用计数 */
static int g_destroy_count = 0;

/** @brief 测试用 destroy 回调 */
static void test_destroy_fn(void *value) {
    (void) value;
    g_destroy_count++;
}

/** @brief 测试用工厂函数（name→factory 形态） */
static void *test_factory_a(void) {
    return (void *) (intptr_t) 0xA11CE;
}

/* ============================================================
 * put/get/remove 生命周期
 * ============================================================ */

static void test_put_get_remove_lifecycle(void) {
    lvRegistry reg;
    lv_registry_init(&reg, 2);

    /* 空表 */
    TEST_ASSERT_EQ(lv_registry_count(&reg), 0);
    TEST_ASSERT_NULL(lv_registry_get(&reg, "k1"));
    TEST_ASSERT_EQ(lv_registry_find(&reg, "k1"), -1);

    /* 追加（含自动扩容：初始容量 2，写入 3 个） */
    TEST_ASSERT(lv_registry_put(&reg, "k1", (void *) (intptr_t) 0x1111), "put k1");
    TEST_ASSERT(lv_registry_put(&reg, "k2", (void *) (intptr_t) 0x2222), "put k2");
    TEST_ASSERT(lv_registry_put(&reg, "k3", (void *) (intptr_t) 0x3333), "put k3 (grow)");
    TEST_ASSERT_EQ(lv_registry_count(&reg), 3);

    /* 取值与遍历 */
    TEST_ASSERT_EQ((intptr_t) lv_registry_get(&reg, "k1"), (intptr_t) 0x1111);
    TEST_ASSERT_EQ((intptr_t) lv_registry_get(&reg, "k3"), (intptr_t) 0x3333);
    TEST_ASSERT_NULL(lv_registry_get(&reg, "missing"));

    const char *name0 = NULL;
    void *value0 = NULL;
    TEST_ASSERT(lv_registry_get_at(&reg, 0, &name0, &value0), "get_at 0");
    TEST_ASSERT_NOT_NULL(name0);
    TEST_ASSERT_STR_EQ(name0, "k1");
    TEST_ASSERT_EQ((intptr_t) value0, (intptr_t) 0x1111);
    TEST_ASSERT(!lv_registry_get_at(&reg, 99, NULL, NULL), "get_at out of range");
    TEST_ASSERT(!lv_registry_get_at(&reg, -1, NULL, NULL), "get_at negative");

    /* 删除（前移紧凑，保持顺序） */
    TEST_ASSERT(lv_registry_remove(&reg, "k2"), "remove k2");
    TEST_ASSERT_NULL(lv_registry_get(&reg, "k2"));
    TEST_ASSERT_EQ(lv_registry_count(&reg), 2);
    TEST_ASSERT(!lv_registry_remove(&reg, "k2"), "remove k2 again");
    TEST_ASSERT(!lv_registry_remove(&reg, "missing"), "remove missing");

    /* 剩余条目顺序保持 */
    const char *n1 = NULL, *n2 = NULL;
    TEST_ASSERT(lv_registry_get_at(&reg, 0, &n1, NULL), "get_at after remove 0");
    TEST_ASSERT(lv_registry_get_at(&reg, 1, &n2, NULL), "get_at after remove 1");
    TEST_ASSERT_STR_EQ(n1, "k1");
    TEST_ASSERT_STR_EQ(n2, "k3");

    /* NULL 安全 */
    TEST_ASSERT(!lv_registry_put(NULL, "x", NULL), "put NULL reg");
    TEST_ASSERT(!lv_registry_put(&reg, NULL, NULL), "put NULL name");
    TEST_ASSERT_NULL(lv_registry_get(NULL, "x"));
    TEST_ASSERT(!lv_registry_remove(NULL, "x"), "remove NULL reg");
    TEST_ASSERT_EQ(lv_registry_count(NULL), 0);
    TEST_ASSERT(!lv_registry_get_at(NULL, 0, NULL, NULL), "get_at NULL reg");

    lv_registry_destroy(&reg);
    lv_registry_destroy(NULL);
}

/* ============================================================
 * 重复注册行为
 * ============================================================ */

static void test_duplicate_register(void) {
    lvRegistry reg;
    lv_registry_init(&reg, 2);

    /* put 重复：第二次被拒绝且不覆盖原值 */
    TEST_ASSERT(lv_registry_put(&reg, "dup", (void *) (intptr_t) 1), "first put");
    TEST_ASSERT(!lv_registry_put(&reg, "dup", (void *) (intptr_t) 2), "duplicate put rejected");
    TEST_ASSERT_EQ((intptr_t) lv_registry_get(&reg, "dup"), (intptr_t) 1);

    /* register 重复（name→factory）：第二次被拒绝 */
    TEST_ASSERT(lv_registry_register(&reg, "alpha", test_factory_a), "register alpha");
    TEST_ASSERT(!lv_registry_register(&reg, "alpha", test_factory_a), "duplicate register rejected");
    TEST_ASSERT(!lv_registry_register(&reg, "dup", test_factory_a), "register over put name rejected");

    /* 不同名称共存 */
    TEST_ASSERT(lv_registry_put(&reg, "beta", (void *) (intptr_t) 7), "put beta");
    TEST_ASSERT_EQ(lv_registry_count(&reg), 3);

    lv_registry_destroy(&reg);
}

/* ============================================================
 * destroy 回调
 * ============================================================ */

static void test_destroy_callback(void) {
    lvRegistry reg;
    lv_registry_init(&reg, 2);

    g_destroy_count = 0;
    TEST_ASSERT(lv_registry_put_ex(&reg, "owned", (void *) (intptr_t) 0xABC, test_destroy_fn), "put with destroy");
    TEST_ASSERT_EQ(g_destroy_count, 0);

    /* remove 时调用 destroy */
    TEST_ASSERT(lv_registry_remove(&reg, "owned"), "remove owned");
    TEST_ASSERT_EQ(g_destroy_count, 1);

    /* destroy 注册表时调用 destroy */
    TEST_ASSERT(lv_registry_put_ex(&reg, "owned2", (void *) (intptr_t) 0xDEF, test_destroy_fn), "put owned2");
    TEST_ASSERT(lv_registry_put(&reg, "plain", (void *) (intptr_t) 0x1), "put plain (no destroy)");
    lv_registry_destroy(&reg);
    TEST_ASSERT_EQ(g_destroy_count, 2);

    /* 空条目（value == NULL 且带 destroy）：不调用 destroy（无值可析构） */
    g_destroy_count = 0;
    lv_registry_init(&reg, 2);
    TEST_ASSERT(lv_registry_put_ex(&reg, "nullv", NULL, test_destroy_fn), "put NULL value");
    TEST_ASSERT(lv_registry_remove(&reg, "nullv"), "remove NULL value");
    TEST_ASSERT_EQ(g_destroy_count, 0);
    lv_registry_destroy(&reg);
}

/* ============================================================
 * name→factory 兼容 API
 * ============================================================ */

static void test_factory_compat(void) {
    lvRegistry reg;
    lv_registry_init(&reg, 2);

    TEST_ASSERT(lv_registry_register(&reg, "alpha", test_factory_a), "register alpha");
    TEST_ASSERT_EQ(lv_registry_find(&reg, "alpha"), 0);
    TEST_ASSERT_EQ(lv_registry_find(&reg, "missing"), -1);
    TEST_ASSERT_NOT_NULL(lv_registry_create(&reg, "alpha"));
    TEST_ASSERT_NULL(lv_registry_create(&reg, "missing"));

    /* 工厂形态与泛型形态共存于同一注册表 */
    TEST_ASSERT(lv_registry_put(&reg, "beta", (void *) (intptr_t) 0x1234), "put beta");
    TEST_ASSERT_EQ(lv_registry_count(&reg), 2);
    TEST_ASSERT_EQ((intptr_t) lv_registry_get(&reg, "beta"), (intptr_t) 0x1234);
    TEST_ASSERT_NOT_NULL(lv_registry_create(&reg, "alpha"));

    /* 泛型条目被 remove 后工厂条目不受影响 */
    TEST_ASSERT(lv_registry_remove(&reg, "beta"), "remove beta");
    TEST_ASSERT_EQ(lv_registry_count(&reg), 1);
    TEST_ASSERT_NOT_NULL(lv_registry_create(&reg, "alpha"));

    lv_registry_destroy(&reg);
}

/* ============================================================
 * 迁移后：插件接口注册/注销
 * ============================================================ */

static void test_plugin_interface_registry(void) {
    lvPluginSystem *sys = lv_plugin_system_create(NULL);
    TEST_ASSERT_NOT_NULL(sys);
    TEST_ASSERT_EQ(lv_plugin_system_init(sys), 0);

    lvPluginContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.system = sys;

    lvPlugin plugin;
    memset(&plugin, 0, sizeof(plugin));
    plugin.context = &ctx;

    lvPluginInterface iface;
    memset(&iface, 0, sizeof(iface));
    strncpy(iface.name, "test.iface", sizeof(iface.name) - 1);
    iface.version = 1;

    /* 注册 */
    TEST_ASSERT_EQ(lv_plugin_register_interface(&plugin, &iface), 0);

    /* 查询（名称+版本精确匹配） */
    TEST_ASSERT_EQ(lv_plugin_query_interface(sys, "test.iface", 1), &iface);
    TEST_ASSERT_NULL(lv_plugin_query_interface(sys, "test.iface", 2));
    TEST_ASSERT_NULL(lv_plugin_query_interface(sys, "missing", 1));

    /* 通配符查询 */
    size_t cnt = 0;
    lvPluginInterface **found = lv_plugin_query_interfaces(sys, "test.*", &cnt);
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_EQ(cnt, (size_t) 1);
    if (found) {
        TEST_ASSERT_EQ(found[0], &iface);
        lv_free((void **) &found);
    }

    /* 重复注册：同一插件同名接口被拒绝 */
    TEST_ASSERT_EQ(lv_plugin_register_interface(&plugin, &iface), -1);

    /* 注销后查询为 NULL */
    TEST_ASSERT_EQ(lv_plugin_unregister_interface(&plugin, "test.iface"), 0);
    TEST_ASSERT_NULL(lv_plugin_query_interface(sys, "test.iface", 1));

    /* 再次注销：未找到 */
    TEST_ASSERT_EQ(lv_plugin_unregister_interface(&plugin, "test.iface"), -1);

    /* 两个插件可注册同名接口（per-plugin 查重） */
    lvPlugin plugin2;
    memset(&plugin2, 0, sizeof(plugin2));
    plugin2.context = &ctx;

    lvPluginInterface iface2;
    memset(&iface2, 0, sizeof(iface2));
    strncpy(iface2.name, "shared.iface", sizeof(iface2.name) - 1);
    iface2.version = 1;

    lvPluginInterface iface3;
    memset(&iface3, 0, sizeof(iface3));
    strncpy(iface3.name, "shared.iface", sizeof(iface3.name) - 1);
    iface3.version = 2;

    TEST_ASSERT_EQ(lv_plugin_register_interface(&plugin, &iface2), 0);
    TEST_ASSERT_EQ(lv_plugin_register_interface(&plugin2, &iface3), 0);
    TEST_ASSERT_EQ(lv_plugin_query_interface(sys, "shared.iface", 1), &iface2);
    TEST_ASSERT_EQ(lv_plugin_query_interface(sys, "shared.iface", 2), &iface3);

    /* 按插件精确注销：不影响另一个插件的同名接口 */
    TEST_ASSERT_EQ(lv_plugin_unregister_interface(&plugin, "shared.iface"), 0);
    TEST_ASSERT_NULL(lv_plugin_query_interface(sys, "shared.iface", 1));
    TEST_ASSERT_EQ(lv_plugin_query_interface(sys, "shared.iface", 2), &iface3);

    TEST_ASSERT_EQ(lv_plugin_unregister_interface(&plugin2, "shared.iface"), 0);
    TEST_ASSERT_NULL(lv_plugin_query_interface(sys, "shared.iface", 2));

    /* 清理影子数组（register 内部分配，测试手动释放） */
    if (plugin.registered_interfaces) {
        lv_free((void **) &plugin.registered_interfaces);
    }
    if (plugin2.registered_interfaces) {
        lv_free((void **) &plugin2.registered_interfaces);
    }

    lv_plugin_system_cleanup(sys);
    lv_plugin_system_destroy(sys);
}

/* ============================================================
 * 迁移后：插件配置 set/get
 * ============================================================ */

static void test_config_registry(void) {
    lvPluginConfig *cfg = lv_plugin_config_create();
    TEST_ASSERT_NOT_NULL(cfg);

    TEST_ASSERT_EQ(lv_plugin_config_set(cfg, "key1", "value1", 0), 0);
    TEST_ASSERT_EQ(lv_plugin_config_set(cfg, "key2", "42", 1), 0);
    TEST_ASSERT_STR_EQ(lv_plugin_config_get(cfg, "key1", "default"), "value1");
    TEST_ASSERT_STR_EQ(lv_plugin_config_get(cfg, "key2", "default"), "42");
    TEST_ASSERT_STR_EQ(lv_plugin_config_get(cfg, "missing", "default"), "default");

    /* 覆盖语义：已存在的 key 更新 value/type */
    TEST_ASSERT_EQ(lv_plugin_config_set(cfg, "key1", "value2", 2), 0);
    TEST_ASSERT_STR_EQ(lv_plugin_config_get(cfg, "key1", "default"), "value2");

    /* 多实例隔离（复合 key per-instance 查重） */
    lvPluginConfig *cfg2 = lv_plugin_config_create();
    TEST_ASSERT_NOT_NULL(cfg2);
    TEST_ASSERT_EQ(lv_plugin_config_set(cfg2, "key1", "other", 0), 0);
    TEST_ASSERT_STR_EQ(lv_plugin_config_get(cfg, "key1", "default"), "value2");
    TEST_ASSERT_STR_EQ(lv_plugin_config_get(cfg2, "key1", "default"), "other");

    lv_plugin_config_destroy(cfg2);
    lv_plugin_config_destroy(cfg);

    /* NULL 安全 */
    lv_plugin_config_destroy(NULL);
    TEST_ASSERT_NULL(lv_plugin_config_get(NULL, "key", "def"));
}

/* ============================================================
 * 迁移后：几何事件注册
 * ============================================================ */

static void test_geo_event_registry(void) {
    lvEventDetector *detector = geo_event_detector_create();
    TEST_ASSERT_NOT_NULL(detector);

    /* 注册（默认函数分发表） */
    TEST_ASSERT_EQ(geo_event_register(detector, 1, lv_EVENT_CROSSING, NULL, 0, true, NULL), 0);
    TEST_ASSERT_EQ(geo_event_register(detector, 2, lv_EVENT_THRESHOLD, NULL, 0, false, NULL), 0);
    TEST_ASSERT_EQ(detector->num_events, 2);

    /* 重复 ID 注册失败（失败码不变：对外 -1） */
    TEST_ASSERT_EQ(geo_event_register(detector, 1, lv_EVENT_CROSSING, NULL, 0, true, NULL), -1);

    /* 满容量：注册 GEO_EVENT_MAX_EVENTS 个后继续注册失败 */
    lvEventDetector *big = geo_event_detector_create();
    TEST_ASSERT_NOT_NULL(big);
    int rc = 0;
    for (int i = 0; i < GEO_EVENT_MAX_EVENTS; i++) {
        rc = geo_event_register(big, i + 1, lv_EVENT_THRESHOLD, NULL, 0, false, NULL);
        if (rc != 0) {
            break;
        }
    }
    TEST_ASSERT_EQ(rc, 0);
    TEST_ASSERT_EQ(big->num_events, GEO_EVENT_MAX_EVENTS);
    TEST_ASSERT_EQ(geo_event_register(big, 10000, lv_EVENT_THRESHOLD, NULL, 0, false, NULL), -1);

    /* 销毁 big 后其注册表条目被清理，不影响 detector */
    geo_event_detector_destroy(big);
    TEST_ASSERT_EQ(detector->num_events, 2);
    TEST_ASSERT_EQ(geo_event_register(detector, 1, lv_EVENT_CROSSING, NULL, 0, true, NULL), -1);

    geo_event_detector_destroy(detector);
    geo_event_detector_destroy(NULL);
}

/* ============================================================
 * Main
 * ============================================================ */

TEST_MAIN_BEGIN("Registry")

    printf("\n--- Generic Registry ---\n");
    TEST_MAIN_RUN(test_put_get_remove_lifecycle);
    TEST_MAIN_RUN(test_duplicate_register);
    TEST_MAIN_RUN(test_destroy_callback);
    TEST_MAIN_RUN(test_factory_compat);

    printf("\n--- Migrated Registries ---\n");
    TEST_MAIN_RUN(test_plugin_interface_registry);
    TEST_MAIN_RUN(test_config_registry);
    TEST_MAIN_RUN(test_geo_event_registry);

TEST_MAIN_END()
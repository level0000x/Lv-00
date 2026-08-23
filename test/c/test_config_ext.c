/**
 * @file test_config_ext.c
 * @brief 运行时配置系统契约测试（批次 C-㊺续22：config.h 12 个零覆盖 API）
 *
 * 覆盖零覆盖 API：
 *   生命周期：config_default / config_current / config_apply / config_reset
 *   单键访问：config_set_int / config_set_double / config_get_int /
 *     config_get_bool / config_get_double / config_get_string
 *   JSON：config_load_json / config_to_json
 *
 * 契约要点（与实现核对）：
 *   - default：静态默认值（X-macro 四元组生成）。
 *   - current：首次调用从默认初始化，返回全局单例。
 *   - apply：NULL → -1；应用后立即生效并同步几何配置。
 *   - set_int/set_double：字符串键匹配，未知键/NULL → false。
 *   - get_int/get_bool/get_double/get_string：NULL 键返回默认值。
 *   - to_json：输出合法 JSON 含键；load_json：文件缺失 → -1，正常文件
 *     解析应用。
 *   - 测试需保护全局配置状态：结束后 reset 恢复默认。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/config.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：生命周期 ============== */

static void test_config_lifecycle_api(void) {
    /* default：静态默认值 */
    const lvConfig *d = lv_config_default();
    TEST_ASSERT_NOT_NULL(d);
    TEST_ASSERT_EQ(d->engine.max_module_depth, 32);
    TEST_ASSERT_EQ(d->solver.solver_max_iterations, 10000);
    TEST_ASSERT_DOUBLE(d->geometry.geo_min_zoom, 0.01, 1e-12);
    TEST_ASSERT_DOUBLE(d->geometry.geo_max_zoom, 100.0, 1e-12);

    /* current：首次从默认初始化 */
    const lvConfig *c = lv_config_current();
    TEST_ASSERT_NOT_NULL(c);
    TEST_ASSERT_EQ(c->engine.max_module_depth, d->engine.max_module_depth);
    TEST_ASSERT(c == lv_config_current(), "current 返回同一实例");

    /* apply：应用修改的副本 */
    lvConfig cfg = *c;
    cfg.engine.max_module_depth = 99;
    cfg.geometry.geo_min_zoom = 0.5;
    TEST_ASSERT_EQ(lv_config_apply(&cfg), 0);
    TEST_ASSERT_EQ(lv_config_current()->engine.max_module_depth, 99);
    TEST_ASSERT_DOUBLE(lv_config_current()->geometry.geo_min_zoom, 0.5, 1e-12);

    /* apply NULL → -1 */
    TEST_ASSERT_EQ(lv_config_apply(NULL), -1);

    /* reset：恢复默认 */
    lv_config_reset();
    TEST_ASSERT_EQ(lv_config_current()->engine.max_module_depth, 32);
    TEST_ASSERT_DOUBLE(lv_config_current()->geometry.geo_min_zoom, 0.01, 1e-12);

    printf("  test_config_lifecycle_api: PASSED\n");
}

/* ============== 测试：单键访问 ============== */

static void test_config_key_api(void) {
    /* set_int：匹配键 */
    TEST_ASSERT(lv_config_set_int("max_module_depth", 55), "设置 int 键");
    TEST_ASSERT_EQ(lv_config_get_int("max_module_depth", -1), 55);
    /* 未知键 → false，get 返回默认 */
    TEST_ASSERT(!lv_config_set_int("no_such_key", 1), "未知键失败");
    TEST_ASSERT_EQ(lv_config_get_int("no_such_key", 42), 42);
    TEST_ASSERT(!lv_config_set_int(NULL, 1), "NULL 键失败");
    TEST_ASSERT_EQ(lv_config_get_int(NULL, 42), 42);

    /* set_double */
    TEST_ASSERT(lv_config_set_double("geo_min_zoom", 0.25), "设置 double 键");
    TEST_ASSERT_DOUBLE(lv_config_get_double("geo_min_zoom", -1.0), 0.25, 1e-12);
    TEST_ASSERT(!lv_config_set_double("no_such_key", 1.0), "未知 double 键失败");
    TEST_ASSERT_DOUBLE(lv_config_get_double("no_such_key", 1.5), 1.5, 1e-12);
    TEST_ASSERT_DOUBLE(lv_config_get_double(NULL, 1.5), 1.5, 1e-12);

    /* get_bool：int 键非零 → true */
    TEST_ASSERT(lv_config_set_int("max_module_depth", 1), "bool 测试设 1");
    TEST_ASSERT(lv_config_get_bool("max_module_depth", false), "非零为 true");
    TEST_ASSERT(lv_config_set_int("max_module_depth", 0), "bool 测试设 0");
    TEST_ASSERT(!lv_config_get_bool("max_module_depth", true), "零为 false");
    TEST_ASSERT(lv_config_get_bool("no_such_key", true), "未知键返回默认 true");
    TEST_ASSERT(!lv_config_get_bool(NULL, false), "NULL 键返回默认 false");

    /* get_string：A 注册表无字符串，命中返回 default；未知键回落 B 或默认 */
    const char *s = lv_config_get_string("max_module_depth", "dflt");
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT(strcmp(s, "dflt") == 0, "A 命中键返回 default");
    s = lv_config_get_string("no_such_key", "fallback");
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT(strcmp(s, "fallback") == 0, "未知键返回默认");
    TEST_ASSERT(strcmp(lv_config_get_string(NULL, "n"), "n") == 0, "NULL 键返回默认");

    /* 类型安全 getter（X-macro 生成，顺带验证 setter 联动） */
    lv_config_set_max_module_depth(77);
    TEST_ASSERT_EQ(lv_config_get_max_module_depth(), 77);
    lv_config_set_geo_min_zoom(0.125);
    TEST_ASSERT_DOUBLE(lv_config_get_geo_min_zoom(), 0.125, 1e-12);

    lv_config_reset();
    printf("  test_config_key_api: PASSED\n");
}

/* ============== 测试：JSON ============== */

static void test_config_json_api(void) {
    /* to_json：合法 JSON 含键 */
    char buf[16384];
    int n = lv_config_to_json(buf, sizeof(buf));
    TEST_ASSERT(n > 0, "to_json 正长度");
    TEST_ASSERT(strstr(buf, "\"max_module_depth\"") != NULL, "含 int 键");
    TEST_ASSERT(strstr(buf, "\"geo_min_zoom\"") != NULL, "含 double 键");
    TEST_ASSERT(buf[0] == '{', "JSON 对象开头");
    TEST_ASSERT(strstr(buf, "}") != NULL, "JSON 对象结尾");

    /* to_json NULL/过小 */
    TEST_ASSERT_EQ(lv_config_to_json(NULL, sizeof(buf)), -1);
    TEST_ASSERT_EQ(lv_config_to_json(buf, 32), -1);

    /* load_json：文件不存在 → -1 */
    TEST_ASSERT_EQ(lv_config_load_json("nonexistent_config_file.json"), -1);
    TEST_ASSERT_EQ(lv_config_load_json(NULL), -1);

    /* load_json：临时配置文件往返 */
    const char *path = "lv_test_config.json";
    FILE *f = fopen(path, "w");
    TEST_ASSERT_NOT_NULL(f);
    fprintf(f, "{\"max_module_depth\": 123, \"geo_min_zoom\": 0.75}");
    fclose(f);

    TEST_ASSERT_EQ(lv_config_load_json(path), 0);
    TEST_ASSERT_EQ(lv_config_current()->engine.max_module_depth, 123);
    TEST_ASSERT_DOUBLE(lv_config_current()->geometry.geo_min_zoom, 0.75, 1e-12);

    /* 未知键被忽略：load_json 以默认配置为基准全量替换（未在 JSON 的键
     * 恢复默认），未知键本身不影响已定义键 */
    f = fopen(path, "w");
    fprintf(f, "{\"unknown_key\": 999}");
    fclose(f);
    TEST_ASSERT_EQ(lv_config_load_json(path), 0);
    TEST_ASSERT_EQ(lv_config_current()->engine.max_module_depth, 32); /* 未在 JSON → 默认 */

    remove(path);
    lv_config_reset();
    printf("  test_config_json_api: PASSED\n");
}

/* ============== 测试入口 ============== */

TEST_MAIN_BEGIN("Lv-00 Config Ext Test Suite")
    printf("=== Lv-00 Config Ext Test Suite (batch C-㊺续22) ===\n\n");
    lv_init();

    TEST_MAIN_RUN(test_config_lifecycle_api);
    TEST_MAIN_RUN(test_config_key_api);
    TEST_MAIN_RUN(test_config_json_api);

    lv_cleanup();
TEST_MAIN_END()

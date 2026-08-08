/**
 * @file test_module_serialize_unify.c
 * @brief 模块序列化统一（X-macro 单一事实源）测试
 *
 * 覆盖：
 *  - JSON：字节格式（紧凑、字段顺序）与 round-trip
 *    （注：依赖数组/图的反序列化存在历史局限，见 test_json_roundtrip 注释）
 *  - MessagePack：字节格式（5 键 fixmap）与 round-trip
 *  - MessagePack：未知键 skip（mp_decoder_skip_value 跳转表覆盖嵌套容器）
 *  - 版本哈希：确定性 + 字段变化敏感性 + graph 不参与（历史行为）
 *  - 三色 DFS 环检测：环路径输出、无环、>MAX_MODULE_DEPTH 模块数（动态路径栈）
 *
 * 注意：msgpack 二进制格式按历史约定不含 graph，round-trip 后 graph 为 NULL。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv.h"
#include "lv/module_internal.h" /* 访问 mod->dependencies 以设置 dep->module（内部测试） */
#include "test_helpers.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== JSON round-trip + 字节格式 ============== */

static void test_json_roundtrip(void) {
    printf("Test: JSON round-trip + byte format...\n");

    Module *mod = module_create("TestModule", "1.0.0");
    lv_ASSERT_NOT_NULL(mod);
    module_add_dependency(mod, "Base", ">=1.0.0");
    module_add_dependency(mod, "MathLib", "^2.0.0");
    module_export_function_block(mod, 1001);
    module_export_function_block(mod, 1002);
    module_export_type_region(mod, 2001);
    AxiomPackage *pkg = axiom_package_create("Euclid", "1.0");
    lv_ASSERT_NOT_NULL(pkg);
    module_add_axiom_package(mod, pkg);

    /* graph：一个点 */
    ConstraintGraph *g = graph_create();
    SymbolicCoord *coords[2] = {symbolic_coord_create_rational(5, 1), symbolic_coord_create_rational(6, 1)};
    graph_add_point(g, coords, 2);
    module_set_graph(mod, g);
    lv_ASSERT(graph_get_node_count(module_get_graph(mod)) == 1);

    char *json = module_serialize_to_json(mod);
    lv_ASSERT_NOT_NULL(json);

    /* 字节格式：紧凑无空白、字段顺序 name/version/dependencies/exports/axiom_packages/graph */
    lv_ASSERT(json[0] == '{');
    lv_ASSERT(json[1] == '"'); /* 无空白 */
    lv_ASSERT(strstr(json, "\"name\":\"TestModule\"") != NULL);
    lv_ASSERT(strstr(json, "\"version\":\"1.0.0\"") != NULL);
    lv_ASSERT(strstr(json, "\"dependencies\":[{\"name\":\"Base\",\"version_constraint\":\">=1.0.0\"}") != NULL);
    lv_ASSERT(strstr(json, "\"exports\":{\"function_blocks\":[1001,1002],\"type_regions\":[2001]}") != NULL);
    lv_ASSERT(strstr(json, "\"axiom_packages\":[\"Euclid\"]") != NULL);
    lv_ASSERT(strstr(json, "\"graph\":{") != NULL);
    printf("  JSON 字节: %s\n", json);

    /* round-trip：依赖/导出字段循环逗号 bug 已修复（与 graph_serialize.c 同源），
     * 依赖数组完整恢复、后续键（exports/axiom_packages/graph）不再丢失 */
    Module *restored = NULL;
    ModuleLoadStatus status = module_deserialize_from_json(json, &restored);
    lv_ASSERT(status == MODULE_LOAD_OK);
    lv_ASSERT_NOT_NULL(restored);
    lv_ASSERT_STR_EQ(module_get_name(restored), "TestModule");
    lv_ASSERT_STR_EQ(module_get_version(restored), "1.0.0");
    lv_ASSERT(module_get_dependency_count(restored) == 2);   /* 两个依赖均恢复 */
    lv_ASSERT(module_get_axiom_package_count(restored) == 1); /* 后续键不再丢失 */
    lv_ASSERT_NOT_NULL(module_get_graph(restored));            /* graph 已恢复 */
    lv_ASSERT(graph_get_node_count(module_get_graph(restored)) == 1);

    lv_free_ptr(json);
    module_destroy(restored);
    module_destroy(mod);
    printf("  PASSED\n");

}

/* ============== MessagePack round-trip + 字节格式 ============== */

static void test_msgpack_roundtrip(void) {
    printf("Test: MessagePack round-trip + byte format...\n");

    Module *mod = module_create("TestModule", "1.0.0");
    lv_ASSERT_NOT_NULL(mod);
    module_add_dependency(mod, "Base", ">=1.0.0");
    module_export_function_block(mod, 7);
    module_export_type_region(mod, 8);
    AxiomPackage *pkg = axiom_package_create("Euclid", "1.0");
    lv_ASSERT_NOT_NULL(pkg);
    module_add_axiom_package(mod, pkg);

    uint8_t *data = NULL;
    size_t size = 0;
    ModuleSaveStatus sstatus = module_save_to_binary(mod, &data, &size);
    lv_ASSERT(sstatus == MODULE_SAVE_OK);
    lv_ASSERT(data != NULL && size > 0);

    /* 字节格式：顶层 fixmap 5 键 + "name" fixstr + "TestModule" fixstr(10) */
    lv_ASSERT(size >= 12);
    lv_ASSERT(data[0] == 0x85); /* fixmap 5（msgpack 不含 graph，与历史一致） */
    lv_ASSERT(memcmp(data + 1, "\xa4name", 5) == 0);
    lv_ASSERT(data[6] == 0xaa); /* fixstr 10 = "TestModule" */
    lv_ASSERT(memcmp(data + 7, "TestModule", 10) == 0);

    /* round-trip */
    Module *restored = NULL;
    ModuleLoadStatus status = module_load_from_binary(data, size, &restored);
    lv_ASSERT(status == MODULE_LOAD_OK);
    lv_ASSERT_NOT_NULL(restored);
    lv_ASSERT_STR_EQ(module_get_name(restored), "TestModule");
    lv_ASSERT_STR_EQ(module_get_version(restored), "1.0.0");
    lv_ASSERT(module_get_dependency_count(restored) == 1);
    lv_ASSERT(module_get_axiom_package_count(restored) == 1);
    lv_ASSERT(module_get_graph(restored) == NULL); /* msgpack 格式不含 graph */

    lv_free_ptr(data);
    module_destroy(restored);
    module_destroy(mod);
    printf("  PASSED\n");

}

/* ============== MessagePack 未知键 skip（跳转表覆盖） ==============
 *
 * {"name":"M", "extra":{...嵌套容器/标量混合...}, "version":"1.0"}
 * "extra" 是未知键，其值包含 fixmap/fixarray/uint8/int64/float64/bin8/
 * str16/array16/map16 多种类型，验证 mp_decoder_skip_value 跳转表能完整跳过。
 */
static void test_msgpack_unknown_key_skip(void) {
    printf("Test: MessagePack unknown-key skip (skip table)...\n");

    static const uint8_t data[] = {
        0x83, /* fixmap 3 */
        0xa4, 'n', 'a', 'm', 'e', 0xa1, 'M', /* "name": "M" */
        /* "extra"（未知键）：值 = fixmap 9，覆盖跳转表各容器/标量分支 */
        0xa5, 'e', 'x', 't', 'r', 'a',
        0x89,
        0xa2, 'u', '8', 0xcc, 0xfe,                          /* "u8": uint8 0xfe */
        0xa3, 'n', 'e', 'g', 0xd3, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* "neg": int64 -1 */
        0xa3, 'f', '6', '4', 0xcb, 0x3f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* "f64": float64 1.0 */
        0xa3, 'b', 'i', 'n', 0xc4, 0x02, 0xaa, 0xbb,         /* "bin": bin8 [0xaa,0xbb] */
        0xa5, 's', 't', 'r', '1', '6', 0xda, 0x00, 0x02, 'h', 'i', /* "str16": "hi" */
        0xa5, 'a', 'r', 'r', '1', '6', 0xdc, 0x00, 0x02, 0x01, 0x02, /* "arr16": [1,2] */
        0xa2, 'f', 'a', 0x93, 0x01, 0x02, 0xa1, 's',          /* "fa": fixarray [1,2,"s"] */
        0xa5, 'm', 'a', 'p', '1', '6', 0xde, 0x00, 0x01, 0xa1, 'k', 0xa1, 'v', /* "map16": {"k":"v"} */
        0xa6, 'n', 'e', 's', 't', 'e', 'd', 0x81, 0xa1, 'y', 0x92, 0x01, 0xa1, 'z', /* "nested": {"y":[1,"z"]} */
        0xa7, 'v', 'e', 'r', 's', 'i', 'o', 'n', 0xa3, '1', '.', '0', /* "version": "1.0" */
    };

    Module *mod = NULL;
    ModuleLoadStatus status = module_load_from_binary(data, sizeof(data), &mod);
    lv_ASSERT(status == MODULE_LOAD_OK);
    lv_ASSERT_NOT_NULL(mod);
    lv_ASSERT_STR_EQ(module_get_name(mod), "M");
    lv_ASSERT_STR_EQ(module_get_version(mod), "1.0");

    module_destroy(mod);
    printf("  PASSED\n");

}

/* ============== 版本哈希 ============== */

static void test_version_hash(void) {
    printf("Test: version hash determinism + sensitivity...\n");

    Module *mod = module_create("HashModule", "1.0.0");
    lv_ASSERT_NOT_NULL(mod);
    module_add_dependency(mod, "Base", ">=1.0.0");

    char *h1 = module_compute_version_hash(mod);
    lv_ASSERT_NOT_NULL(h1);
    char *h2 = module_compute_version_hash(mod);
    lv_ASSERT_NOT_NULL(h2);
    lv_ASSERT_STR_EQ(h1, h2); /* 确定性 */

    /* 版本变化 → 哈希变化 */
    Module *mod2 = module_create("HashModule", "2.0.0");
    lv_ASSERT_NOT_NULL(mod2);
    module_add_dependency(mod2, "Base", ">=1.0.0");
    char *h3 = module_compute_version_hash(mod2);
    lv_ASSERT_NOT_NULL(h3);
    lv_ASSERT(strcmp(h1, h3) != 0);

    /* 依赖变化 → 哈希变化 */
    Module *mod3 = module_create("HashModule", "1.0.0");
    lv_ASSERT_NOT_NULL(mod3);
    module_add_dependency(mod3, "Other", ">=1.0.0");
    char *h4 = module_compute_version_hash(mod3);
    lv_ASSERT_NOT_NULL(h4);
    lv_ASSERT(strcmp(h1, h4) != 0);

    /* graph 不参与版本哈希（历史行为：hash_field_graph 为空 handler） */
    module_set_graph(mod, graph_create());
    char *h5 = module_compute_version_hash(mod);
    lv_ASSERT_NOT_NULL(h5);
    lv_ASSERT_STR_EQ(h1, h5);

    lv_free_ptr(h1);
    lv_free_ptr(h2);
    lv_free_ptr(h3);
    lv_free_ptr(h4);
    lv_free_ptr(h5);
    module_destroy(mod);
    module_destroy(mod2);
    module_destroy(mod3);
    printf("  PASSED\n");

}

/* ============== 三色 DFS 环检测 ============== */

/* 为模块 mod 的依赖 dep_name 设置已解析的模块指针（测试辅助） */
static void link_dependency(Module *mod, const char *dep_name, Module *dep) {
    for (int i = 0; i < mod->dependencies.count; i++) {
        ModuleDependency *d = ((ModuleDependency *) mod->dependencies.data) + i;
        if (strcmp(d->name, dep_name) == 0) {
            d->module = dep;
            return;
        }
    }
    lv_ASSERT(!"link_dependency: 依赖未找到");
}

static void test_cycle_detect(void) {
    printf("Test: three-color DFS cycle detect + path output...\n");

    /* 环 A→B→C→A */
    Module *ma = module_create("A", "1.0");
    Module *mb = module_create("B", "1.0");
    Module *mc = module_create("C", "1.0");
    lv_ASSERT(ma && mb && mc);
    module_add_dependency(ma, "B", "1.0");
    module_add_dependency(mb, "C", "1.0");
    module_add_dependency(mc, "A", "1.0");
    link_dependency(ma, "B", mb);
    link_dependency(mb, "C", mc);
    link_dependency(mc, "A", ma);

    Module *cyclic[] = {ma, mb, mc};
    int *path = NULL;
    int path_len = 0;
    lv_ASSERT(module_full_cycle_detect(cyclic, 3, &path, &path_len));
    lv_ASSERT_NOT_NULL(path);
    lv_ASSERT(path_len == 3);
    lv_ASSERT(path[0] == 0 && path[1] == 1 && path[2] == 2); /* A→B→C→A */
    lv_free_ptr(path);
    module_destroy(ma);
    module_destroy(mb);
    module_destroy(mc);

    /* 无环 A→B→C */
    Module *na = module_create("A", "1.0");
    Module *nb = module_create("B", "1.0");
    Module *nc = module_create("C", "1.0");
    lv_ASSERT(na && nb && nc);
    module_add_dependency(na, "B", "1.0");
    module_add_dependency(nb, "C", "1.0");
    link_dependency(na, "B", nb);
    link_dependency(nb, "C", nc);

    Module *acyclic[] = {na, nb, nc};
    path = NULL;
    path_len = 0;
    lv_ASSERT(!module_full_cycle_detect(acyclic, 3, &path, &path_len));
    lv_ASSERT(path == NULL);
    module_destroy(na);
    module_destroy(nb);
    module_destroy(nc);

    /* > MAX_MODULE_DEPTH(32) 线性链：验证动态路径栈消除固定栈上限 */
    enum { CHAIN_N = 40 };
    Module **chain = (Module **) calloc((size_t) CHAIN_N, sizeof(Module *));
    lv_ASSERT_NOT_NULL(chain);
    for (int i = 0; i < CHAIN_N; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "M%d", i);
        chain[i] = module_create(buf, "1.0");
        lv_ASSERT_NOT_NULL(chain[i]);
        if (i > 0) {
            char dep_name[32];
            snprintf(dep_name, sizeof(dep_name), "M%d", i - 1);
            module_add_dependency(chain[i], dep_name, "1.0");
            link_dependency(chain[i], dep_name, chain[i - 1]);
        }
    }
    path = NULL;
    path_len = 0;
    lv_ASSERT(!module_full_cycle_detect(chain, CHAIN_N, &path, &path_len)); /* 无环不崩溃 */
    lv_ASSERT(path == NULL);
    for (int i = 0; i < CHAIN_N; i++)
        module_destroy(chain[i]);
    free(chain);

    printf("  PASSED\n");

}

/* ============== 主函数 ============== */

TEST_MAIN_BEGIN("Lv-00 Module Serialization Unify Test Suite")
    printf("=== Lv-00 Module Serialization Unify Test Suite ===\n\n");
    TEST_MAIN_RUN(test_json_roundtrip);
    TEST_MAIN_RUN(test_msgpack_roundtrip);
    TEST_MAIN_RUN(test_msgpack_unknown_key_skip);
    TEST_MAIN_RUN(test_version_hash);
    TEST_MAIN_RUN(test_cycle_detect);
    printf("\n=== All module serialization unify tests PASSED! ===\n");
TEST_MAIN_END()

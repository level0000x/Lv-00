/**
 * @file test_serialize_registry.c
 * @brief 测试序列化注册表（lv_serialize_register_format 系列 API）、
 *        graph JSON 试点接入与统一 round-trip 验证
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

#include "lv/lv_storage.h"
#include "lv/lv_serialize_adapters.h"
#include "lv/meta_repr.h"

/* ── 假类型：验证默认格式（旧 API 兼容）与多格式共存 ── */
typedef struct {
    int value;
} DummyObj;

static bool dummy_ser(const void *obj, lvStorage *storage) {
    if (!obj || !storage) return false;
    const DummyObj *d = (const DummyObj *) obj;
    char buf[32];
    int n = snprintf(buf, sizeof(buf), "%d", d->value);
    return lv_storage_write_all(storage, buf, n);
}

static bool dummy_deser(void *obj, lvStorage *storage) {
    if (!obj || !storage) return false;
    int64_t size = 0;
    char *buf = lv_storage_read_all(storage, &size);
    if (!buf) return false;
    DummyObj *d = (DummyObj *) lv_malloc(sizeof(DummyObj));
    if (!d) {
        lv_free((void **) &buf);
        return false;
    }
    d->value = atoi(buf);
    lv_free((void **) &buf);
    *(DummyObj **) obj = d;
    return true;
}

static bool graph_compare_cb(const void *a, const void *b) {
    return meta_repr_graph_equivalent((const ConstraintGraph *) a, (const ConstraintGraph *) b);
}

/** @brief 构建一个包含点/线段/约束的测试图 */
static ConstraintGraph *build_test_graph(void) {
    ConstraintGraph *graph = graph_create();
    if (!graph) return NULL;
    add_point(graph, 1, 1, 2, 1);
    add_point(graph, 3, 1, 4, 1);
    if (graph_add_line_segment(graph, 0, 1) != ADD_NODE_OK) {
        graph_destroy(graph);
        return NULL;
    }
    if (graph_add_incidence(graph, 0, 2) != ADD_CONSTRAINT_OK) {
        graph_destroy(graph);
        return NULL;
    }
    return graph;
}

/* 1. 默认格式（旧 API）注册与往返 */
void test_default_format_compat(void) {
    printf("=== 测试 默认格式注册（旧 API 兼容） ===\n");

    /* 旧 API 注册 → key "DummyType:default" */
    lv_ASSERT(lv_serialize_register("DummyType", dummy_ser, dummy_deser));

    DummyObj d;
    d.value = 42;

    lvStorage *s = lv_storage_open("mem://dummy_default",
        lv_STORAGE_WRITE | lv_STORAGE_CREATE | lv_STORAGE_TRUNCATE | lv_STORAGE_BINARY);
    lv_ASSERT_NOT_NULL(s);
    /* 旧 API 序列化 → 默认格式 */
    lv_ASSERT(lv_serialize_to_storage("DummyType", &d, s));
    lv_ASSERT(lv_storage_seek(s, 0, lv_STORAGE_SEEK_SET) >= 0);
    DummyObj *out = NULL;
    /* 旧 API 反序列化 → 默认格式 */
    lv_ASSERT(lv_deserialize_from_storage("DummyType", &out, s));
    lv_ASSERT_NOT_NULL(out);
    lv_ASSERT(out->value == 42);
    lv_free((void **) &out);
    lv_storage_close(s);

    printf("默认格式兼容测试通过!\n\n");
}

/* 2. format 维度：多格式共存与隔离 */
void test_format_dimension(void) {
    printf("=== 测试 format 维度（多格式共存） ===\n");

    /* 同类型注册两种显式格式 */
    lv_ASSERT(lv_serialize_register_format("DummyType", "json", dummy_ser, dummy_deser));
    lv_ASSERT(lv_serialize_register_format("DummyType", "custom", dummy_ser, dummy_deser));

    DummyObj d;
    d.value = 7;

    /* 显式格式序列化/反序列化 */
    lvStorage *s = lv_storage_open("mem://dummy_multi",
        lv_STORAGE_WRITE | lv_STORAGE_CREATE | lv_STORAGE_TRUNCATE | lv_STORAGE_BINARY);
    lv_ASSERT_NOT_NULL(s);
    lv_ASSERT(lv_serialize_to_storage_format("DummyType", "custom", &d, s));
    lv_ASSERT(lv_storage_seek(s, 0, lv_STORAGE_SEEK_SET) >= 0);
    DummyObj *out = NULL;
    lv_ASSERT(lv_deserialize_from_storage_format("DummyType", "custom", &out, s));
    lv_ASSERT_NOT_NULL(out);
    lv_ASSERT(out->value == 7);
    lv_free((void **) &out);
    lv_storage_close(s);

    /* 未注册格式 → 失败 */
    lvStorage *s2 = lv_storage_open("mem://dummy_badfmt",
        lv_STORAGE_WRITE | lv_STORAGE_CREATE | lv_STORAGE_TRUNCATE | lv_STORAGE_BINARY);
    lv_ASSERT_NOT_NULL(s2);
    lv_ASSERT(lv_serialize_to_storage_format("DummyType", "bin", &d, s2) == false);
    lv_storage_close(s2);

    /* 未注册类型 → 失败 */
    lvStorage *s3 = lv_storage_open("mem://dummy_notype",
        lv_STORAGE_WRITE | lv_STORAGE_CREATE | lv_STORAGE_TRUNCATE | lv_STORAGE_BINARY);
    lv_ASSERT_NOT_NULL(s3);
    lv_ASSERT(lv_serialize_to_storage("NoSuchType", &d, s3) == false);
    lv_storage_close(s3);

    printf("format 维度测试通过!\n\n");
}

/* 3. graph "ConstraintGraph:json" 文件 round-trip */
void test_graph_json_file_roundtrip(void) {
    printf("=== 测试 ConstraintGraph:json 文件 round-trip ===\n");

    /* 幂等注册（重复调用应仍返回 true） */
    lv_ASSERT(lv_serialize_register_graph_adapters());
    lv_ASSERT(lv_serialize_register_graph_adapters());

    ConstraintGraph *graph = build_test_graph();
    lv_ASSERT_NOT_NULL(graph);

    const char *filepath = "test_serialize_registry_tmp.json";
    remove(filepath); /* 清理可能残留的文件 */

    lv_ASSERT(lv_serialize_to_file_format("ConstraintGraph", "json", graph, filepath));

    ConstraintGraph *restored = NULL;
    lv_ASSERT(lv_deserialize_from_file_format("ConstraintGraph", "json", &restored, filepath));
    lv_ASSERT_NOT_NULL(restored);

    /* 语义等价（graph 现有比较） */
    lv_ASSERT(meta_repr_graph_equivalent(graph, restored));
    lv_ASSERT(graph_get_node_count(restored) == 3);         /* 2 点 + 1 线段 */
    lv_ASSERT(graph_get_constraint_count(restored) == 1);

    /* 与 graph_serialize_to_json 直接结果字节一致 */
    char *expected = graph_serialize_to_json(graph);
    lv_ASSERT_NOT_NULL(expected);
    lvStorage *rs = lv_storage_open(filepath, lv_STORAGE_READ | lv_STORAGE_BINARY);
    lv_ASSERT_NOT_NULL(rs);
    int64_t fsz = 0;
    char *content = lv_storage_read_all(rs, &fsz);
    lv_storage_close(rs);
    lv_ASSERT_NOT_NULL(content);
    lv_ASSERT(strcmp(expected, content) == 0);
    printf("文件内容与 graph_serialize_to_json 直接结果一致 (%lld 字节)\n", (long long) fsz);
    lv_free_ptr(expected);
    lv_free_ptr(content);

    remove(filepath);
    graph_destroy(restored);
    graph_destroy(graph);

    printf("graph JSON 文件 round-trip 测试通过!\n\n");
}

/* 4. 统一 round-trip 验证 API */
void test_roundtrip_verify(void) {
    printf("=== 测试 lv_roundtrip_verify 统一验证 ===\n");

    lv_ASSERT(lv_serialize_register_graph_adapters());

    ConstraintGraph *graph = build_test_graph();
    lv_ASSERT_NOT_NULL(graph);

    /* 内置分派（compare=NULL → "ConstraintGraph" → meta_repr_graph_equivalent） */
    lv_ASSERT(lv_roundtrip_verify("ConstraintGraph", "json", graph, NULL));

    /* 显式 compare 回调 */
    lv_ASSERT(lv_roundtrip_verify("ConstraintGraph", "json", graph, graph_compare_cb));

    /* 未注册类型/格式 → false */
    lv_ASSERT(lv_roundtrip_verify("NoSuchType", "json", graph, NULL) == false);
    lv_ASSERT(lv_roundtrip_verify("ConstraintGraph", "bin", graph, NULL) == false);

    /* 参数校验 */
    lv_ASSERT(lv_roundtrip_verify(NULL, "json", graph, NULL) == false);
    lv_ASSERT(lv_roundtrip_verify("ConstraintGraph", "json", NULL, NULL) == false);

    graph_destroy(graph);

    printf("round-trip 验证测试通过!\n\n");
}

/* 5. 坐标保持守护（round-trip 后坐标不丢失） */
void test_roundtrip_verify_coords(void) {
    printf("=== 测试 round-trip 坐标保持 ===\n");

    lv_ASSERT(lv_serialize_register_graph_adapters());

    ConstraintGraph *graph = graph_create();
    lv_ASSERT_NOT_NULL(graph);
    add_point(graph, 5, 2, 9, 3); /* 有理数坐标 */

    lv_ASSERT(lv_roundtrip_verify("ConstraintGraph", "json", graph, NULL));

    graph_destroy(graph);
    printf("坐标 round-trip 测试通过!\n\n");
}

TEST_MAIN_BEGIN("序列化注册表测试")
    printf("========================================\n");
    printf("序列化注册表测试\n");
    printf("========================================\n\n");
    TEST_MAIN_RUN(test_default_format_compat);
    TEST_MAIN_RUN(test_format_dimension);
    TEST_MAIN_RUN(test_graph_json_file_roundtrip);
    TEST_MAIN_RUN(test_roundtrip_verify);
    TEST_MAIN_RUN(test_roundtrip_verify_coords);
    printf("========================================\n");
    printf("所有序列化注册表测试通过!\n");
    printf("========================================\n");
TEST_MAIN_END()

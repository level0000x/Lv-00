/**
 * @file test_storage_ext.c
 * @brief 统一存储抽象契约测试（批次 C-㊺续11：lv_storage.h 13 个零覆盖 API）
 *
 * 覆盖 13 个 ctest 零覆盖 API：
 *   - 存储 I/O 族：lv_storage_read / write / tell / size / flush / eof /
 *     is_open
 *   - 文件族：lv_serialize_to_file / lv_deserialize_from_file
 *   - 后端族：lv_storage_register_backend
 *   - 系统族：lv_storage_system_init / system_cleanup
 *   - 验证族：lv_storage_register_verify
 *
 * 契约要点（与头注释核对）：
 *   - mem:// 内存缓冲内置后端：write 后回绕读回。
 *   - 序列化注册表：type_name → ser/deser 回调；文件 round-trip。
 *   - register_backend：scheme 重复 → false。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/lv_storage.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* 自定义序列化类型 */
typedef struct {
    int value;
} TestObj;

static bool test_ser(const void *obj, lvStorage *s) {
    const TestObj *o = (const TestObj *)obj;
    return lv_storage_write(s, &o->value, (int64_t)sizeof(int)) == (int64_t)sizeof(int);
}

static bool test_deser(void *obj, lvStorage *s) {
    TestObj **o = (TestObj **)obj;
    *o = (TestObj *)lv_malloc(sizeof(TestObj));
    if (!*o)
        return false;
    return lv_storage_read(s, &(*o)->value, (int64_t)sizeof(int)) == (int64_t)sizeof(int);
}

/* ============== 测试：存储 I/O（mem:// 后端） ============== */

static void test_storage_io_api(void) {
    lv_storage_system_init();

    /* open mem:// 写模式 */
    lvStorage *s = lv_storage_open("mem://test", lv_STORAGE_WRITE | lv_STORAGE_CREATE);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT(lv_storage_is_open(s), "存储已打开");

    /* write + tell + size */
    const char *data = "hello";
    TEST_ASSERT_EQ(lv_storage_write(s, data, 5), 5);
    TEST_ASSERT_EQ(lv_storage_tell(s), 5);
    TEST_ASSERT_EQ(lv_storage_size(s), 5);
    TEST_ASSERT(lv_storage_flush(s), "刷新成功");

    /* seek 回绕 + read + eof */
    TEST_ASSERT_EQ(lv_storage_seek(s, 0, lv_STORAGE_SEEK_SET), 0);
    char buf[8] = {0};
    TEST_ASSERT_EQ(lv_storage_read(s, buf, 5), 5);
    TEST_ASSERT_STR_EQ(buf, "hello");
    TEST_ASSERT(lv_storage_eof(s), "读完后到达末尾");
    lv_storage_close(s);

    /* NULL 契约 */
    TEST_ASSERT(!lv_storage_is_open(NULL), "NULL 未打开");
    TEST_ASSERT_EQ(lv_storage_read(NULL, buf, 5), -1);
    TEST_ASSERT_EQ(lv_storage_write(NULL, data, 5), -1);
    TEST_ASSERT_EQ(lv_storage_tell(NULL), -1);
    TEST_ASSERT_EQ(lv_storage_size(NULL), -1);
    TEST_ASSERT(!lv_storage_flush(NULL), "NULL 刷新失败");
    TEST_ASSERT(lv_storage_eof(NULL), "NULL 视为末尾");
    lv_storage_close(NULL); /* NULL 安全 */

    lv_storage_system_cleanup();
    printf("  test_storage_io_api: PASSED\n");
}

/* ============== 测试：序列化文件 round-trip ============== */

static void test_serialize_file_api(void) {
    lv_storage_system_init();

    /* 注册类型 */
    TEST_ASSERT(lv_serialize_register("TestObj", test_ser, test_deser), "注册序列化器");

    /* serialize_to_file → 临时文件 */
    TestObj obj;
    obj.value = 42;
    const char *path = "./_tmp_c47_storage.bin";
    remove(path);
    TEST_ASSERT(lv_serialize_to_file("TestObj", &obj, path), "序列化到文件");

    /* deserialize_from_file → 值一致 */
    TestObj *out = NULL;
    TEST_ASSERT(lv_deserialize_from_file("TestObj", &out, path), "从文件反序列化");
    TEST_ASSERT_NOT_NULL(out);
    TEST_ASSERT_EQ(out->value, 42);
    lv_free((void **)&out);
    remove(path);

    /* NULL 契约 */
    TEST_ASSERT(!lv_serialize_to_file(NULL, &obj, path), "NULL type");
    TEST_ASSERT(!lv_serialize_to_file("TestObj", NULL, path), "NULL obj");
    TEST_ASSERT(!lv_serialize_to_file("TestObj", &obj, NULL), "NULL path");

    lv_storage_system_cleanup();
    printf("  test_serialize_file_api: PASSED\n");
}

/* ============== 测试：后端注册 ============== */

static void test_backend_api(void) {
    /* 内置 file/mem 已在 system_init 注册；重复注册 file → false */
    lv_storage_system_init();
    lvStorageBackendInfo info;
    memset(&info, 0, sizeof(info));
    info.scheme = "file";
    info.ops = NULL;
    TEST_ASSERT(!lv_storage_register_backend(&info), "重复 scheme 注册失败");

    /* NULL 契约 */
    TEST_ASSERT(!lv_storage_register_backend(NULL), "NULL info");

    /* 自定义 scheme 注册（ops NULL 也会注册？——契约：scheme+ops 非 NULL） */
    info.scheme = "testx";
    TEST_ASSERT(!lv_storage_register_backend(&info), "ops NULL 注册失败");

    lv_storage_system_cleanup();
    printf("  test_backend_api: PASSED\n");
}

/* ============== 测试：验证注册 ============== */

static bool obj_compare(const void *a, const void *b) {
    const TestObj *oa = (const TestObj *)a;
    const TestObj *ob = (const TestObj *)b;
    return oa->value == ob->value;
}

static void obj_free(void *o) {
    lv_free((void **)&o);
}

static void test_verify_api(void) {
    /* register_verify：NULL 契约 + 正常（不崩溃） */
    lv_storage_register_verify(NULL, NULL, NULL); /* 不崩溃即通过 */
    lv_storage_register_verify("TestObj", obj_compare, obj_free);

    /* 通过注册的序列化器做 round-trip（mem:// 内存往返） */
    lv_storage_system_init();
    TEST_ASSERT(lv_serialize_register("TestObj", test_ser, test_deser), "注册序列化器");
    TestObj obj;
    obj.value = 7;
    TEST_ASSERT(lv_roundtrip_verify("TestObj", NULL, &obj, obj_compare), "round-trip 一致");

    lv_storage_system_cleanup();
    printf("  test_verify_api: PASSED\n");
}

/* ============== 测试入口 ============== */

TEST_MAIN_BEGIN("Lv-00 Storage Ext Test Suite")
    printf("=== Lv-00 Storage Ext Test Suite (batch C-㊺续11) ===\n\n");
    lv_init();

    TEST_MAIN_RUN(test_storage_io_api);
    TEST_MAIN_RUN(test_serialize_file_api);
    TEST_MAIN_RUN(test_backend_api);
    TEST_MAIN_RUN(test_verify_api);

    lv_cleanup();
TEST_MAIN_END()

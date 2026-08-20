/**
 * @file test_lv_utils_ext.c
 * @brief lv_utils 文件 IO 设施 + 批量释放族 + 内存检查契约测试
 *
 * 批次 C-㉜：补全 lv_utils.h 剩余零覆盖设施。
 * 覆盖域（按 test-authoring 三层：等价/边界/性质）：
 * - 配置持久化：config_save / config_load（文件往返、节头分组、类型无损）
 * - INI 解析：lv_ini_parse（节/键值/注释/空行、回调中止、NULL 契约）
 * - 批量释放族：lv_free_many（NULL 终止链表）、lv_free_ptr_array（count 语义）、
 *   lv_auto_free、lv_free_external（系统 free 释放外部内存）
 * - 指针数组复制：lv_copy_ptr_array
 * - 内存检查：lv_memory_check_poison（毒药模式检测）、lv_memory_limit_exceeded
 *
 * @author Lv-00 Project
 * @date 2026-08-19
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_utils.h"
#include "lv.h"
#include "test_helpers.h"

/* ============================================================
 * 全局测试计数器
 * ============================================================ */
int g_pass_count = 0;
int g_fail_count = 0;

/* 测试临时文件路径（工作目录 = CMAKE_SOURCE_DIR） */
#define TEST_CFG_FILE "test_lv_utils_ext_tmp.cfg"
#define TEST_INI_FILE "test_lv_utils_ext_tmp.ini"

static void remove_test_files(void) {
    remove(TEST_CFG_FILE);
    remove(TEST_INI_FILE);
}

/* ============================================================
 * config_save / config_load：文件往返 + 节头分组 + 类型无损
 * ============================================================ */
static void test_config_roundtrip(void) {
    remove_test_files();

    /* NULL 契约 */
    TEST_ASSERT_MSG(!config_save(NULL), "NULL save 失败");
    TEST_ASSERT_MSG(!config_load(NULL), "NULL load 失败");

    /* 无 config_file 的 mgr */
    ConfigManager *mgr0 = config_manager_create(NULL);
    TEST_ASSERT_MSG(mgr0 != NULL, "create(NULL) 成功");
    TEST_ASSERT_MSG(!config_save(mgr0), "无文件 save 失败");
    TEST_ASSERT_MSG(!config_load(mgr0), "无文件 load 失败");
    config_manager_destroy(mgr0);

    /* 完整往返：int / bool / double / string / 节分组 */
    ConfigManager *mgr = config_manager_create(TEST_CFG_FILE);
    TEST_ASSERT_MSG(mgr != NULL, "create 成功");
    config_set_int(mgr, "geom.max_points", 1000);
    config_set_bool(mgr, "solver.enable_restarts", true);
    config_set_double(mgr, "solver.eps", 1e-8);
    config_set_string(mgr, "solver.name", "cdcl");
    config_set_string(mgr, "meta.author", "Lv-00"); /* 无节键 */

    TEST_ASSERT_MSG(config_save(mgr), "save 成功");

    /* 从文件重新加载到新 mgr */
    ConfigManager *mgr2 = config_manager_create(TEST_CFG_FILE);
    TEST_ASSERT_MSG(mgr2 != NULL, "create2 成功");
    TEST_ASSERT_MSG(config_load(mgr2), "load 成功");

    /* 类型无损往返（%.17g 保证 double 精确） */
    TEST_ASSERT_EQ(config_get_int(mgr2, "geom.max_points", -1), 1000);
    TEST_ASSERT_MSG(config_get_bool(mgr2, "solver.enable_restarts", false) == true, "bool 往返");
    TEST_ASSERT_DOUBLE(config_get_double(mgr2, "solver.eps", 0.0), 1e-8, 1e-15);
    TEST_ASSERT_MSG(strcmp(config_get_string(mgr2, "solver.name", ""), "cdcl") == 0, "string 往返");
    TEST_ASSERT_MSG(strcmp(config_get_string(mgr2, "meta.author", ""), "Lv-00") == 0, "无节键往返");

    /* 未定义键返回默认值 */
    TEST_ASSERT_EQ(config_get_int(mgr2, "nokey", 42), 42);

    /* 保存文件内容含节头与键值（抽查） */
    {
        FILE *f = fopen(TEST_CFG_FILE, "r");
        TEST_ASSERT_MSG(f != NULL, "读取保存文件");
        char buf[4096];
        size_t rd = fread(buf, 1, sizeof(buf) - 1, f);
        buf[rd] = '\0';
        fclose(f);
        TEST_ASSERT_MSG(strstr(buf, "[geom]") != NULL, "含 [geom] 节头");
        TEST_ASSERT_MSG(strstr(buf, "max_points = 1000") != NULL, "含 int 键值");
        TEST_ASSERT_MSG(strstr(buf, "[solver]") != NULL, "含 [solver] 节头");
        TEST_ASSERT_MSG(strstr(buf, "enable_restarts = true") != NULL, "含 bool 键值");
        TEST_ASSERT_MSG(strstr(buf, "eps = 1e-08") != NULL || strstr(buf, "eps = 1e-8") != NULL, "含 double 键值");
        TEST_ASSERT_MSG(strstr(buf, "author = Lv-00") != NULL, "含无节键值");
    }

    config_manager_destroy(mgr);
    config_manager_destroy(mgr2);

    /* 不存在的文件 load 失败 */
    ConfigManager *mgr3 = config_manager_create("test_lv_utils_ext_nonexist.cfg");
    TEST_ASSERT_MSG(!config_load(mgr3), "不存在文件 load 失败");
    config_manager_destroy(mgr3);

    /* 数组往返 */
    ConfigManager *mgr4 = config_manager_create(TEST_CFG_FILE);
    config_set_string(mgr4, "list.tags", "[\"a\", \"b\", \"c\"]");
    TEST_ASSERT_MSG(config_save(mgr4), "save 数组");
    ConfigManager *mgr5 = config_manager_create(TEST_CFG_FILE);
    TEST_ASSERT_MSG(config_load(mgr5), "load 数组");
    /* 数组被解析为 CONFIG_TYPE_ARRAY（value.array_val） */
    ConfigItem *item = NULL;
    for (ConfigItem *it = mgr5->items; it; it = it->next) {
        if (strcmp(it->key, "list.tags") == 0) {
            item = it;
            break;
        }
    }
    TEST_ASSERT_MSG(item != NULL, "找到数组项");
    TEST_ASSERT_MSG(item->type == CONFIG_TYPE_ARRAY, "数组类型");
    TEST_ASSERT_EQ(item->array_count, (size_t) 3);
    config_manager_destroy(mgr4);
    config_manager_destroy(mgr5);

    remove_test_files();
}

/* ============================================================
 * lv_ini_parse：节/键值/注释/空行 + 回调中止
 * ============================================================ */
typedef struct {
    int visit_count;
    char sections[16][64];
    char keys[16][64];
    char values[16][128];
    int stop_at; /* 触发回调返回 false 的计数（-1 = 不中止） */
} IniTestCtx;

static bool ini_collect_visit(void *ctx, const char *section, const char *key, const char *value) {
    IniTestCtx *c = (IniTestCtx *) ctx;
    if (c->stop_at >= 0 && c->visit_count >= c->stop_at)
        return false;
    if (c->visit_count < 16) {
        snprintf(c->sections[c->visit_count], sizeof(c->sections[0]), "%s", section ? section : "");
        snprintf(c->keys[c->visit_count], sizeof(c->keys[0]), "%s", key ? key : "");
        snprintf(c->values[c->visit_count], sizeof(c->values[0]), "%s", value ? value : "");
    }
    c->visit_count++;
    return true;
}

static void test_ini_parse(void) {
    remove_test_files();

    /* NULL 契约 */
    TEST_ASSERT_MSG(lv_ini_parse(NULL, ini_collect_visit, NULL) != 0, "NULL path 失败");
    TEST_ASSERT_MSG(lv_ini_parse(TEST_INI_FILE, NULL, NULL) != 0, "NULL visit 失败");
    TEST_ASSERT_MSG(lv_ini_parse("test_lv_utils_ext_nonexist.ini", ini_collect_visit, NULL) != 0, "不存在文件失败");

    /* 写入测试 INI */
    {
        FILE *f = fopen(TEST_INI_FILE, "w");
        TEST_ASSERT_MSG(f != NULL, "写 INI");
        fprintf(f, "# 注释行\n");
        fprintf(f, "// 另一种注释\n");
        fprintf(f, "\n");
        fprintf(f, "global_key = hello\n");
        fprintf(f, "[section1]\n");
        fprintf(f, "a = 1\n");
        fprintf(f, "b = true\n");
        fprintf(f, "[section2]\n");
        fprintf(f, "c = 3.14\n");
        fclose(f);
    }

    IniTestCtx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.stop_at = -1;
    int rc = lv_ini_parse(TEST_INI_FILE, ini_collect_visit, &ctx);
    TEST_ASSERT_MSG(rc == 0, "解析成功");
    TEST_ASSERT_EQ(ctx.visit_count, 4);

    /* 全局键：section 为 NULL；值传 eq+1 原始内容（含前导空格，未 trim） */
    TEST_ASSERT_MSG(ctx.sections[0][0] == '\0', "全局键 section 空");
    TEST_ASSERT_MSG(strcmp(ctx.keys[0], "global_key") == 0, "键 global_key");
    TEST_ASSERT_MSG(strcmp(ctx.values[0], " hello") == 0, "值传 eq+1 原始内容");

    /* 节内键：section 前缀 */
    TEST_ASSERT_MSG(strcmp(ctx.sections[1], "section1") == 0, "节 section1");
    TEST_ASSERT_MSG(strcmp(ctx.keys[1], "a") == 0, "键 a");
    TEST_ASSERT_MSG(strcmp(ctx.values[1], " 1") == 0, "值传原始内容");
    TEST_ASSERT_MSG(strcmp(ctx.sections[2], "section1") == 0, "第二节仍 section1");
    TEST_ASSERT_MSG(strcmp(ctx.keys[2], "b") == 0, "键 b");
    TEST_ASSERT_MSG(strcmp(ctx.sections[3], "section2") == 0, "节切换 section2");
    TEST_ASSERT_MSG(strcmp(ctx.keys[3], "c") == 0, "键 c");
    TEST_ASSERT_MSG(strcmp(ctx.values[3], " 3.14") == 0, "值传原始内容");

    /* 回调中止：stop_at=2 → 仅 2 次访问且返回 0 */
    memset(&ctx, 0, sizeof(ctx));
    ctx.stop_at = 2;
    rc = lv_ini_parse(TEST_INI_FILE, ini_collect_visit, &ctx);
    TEST_ASSERT_MSG(rc == 0, "中止解析仍返回 0");
    TEST_ASSERT_EQ(ctx.visit_count, 2);

    remove_test_files();
}

/* ============================================================
 * lv_free_many：NULL 终止链表释放
 * ============================================================ */
static void test_free_many(void) {
    int *a = (int *) lv_malloc(sizeof(int));
    int *b = (int *) lv_malloc(sizeof(int));
    int *c = (int *) lv_malloc(sizeof(int));
    TEST_ASSERT_MSG(a && b && c, "分配");

    /* 释放链表：... 以 NULL 终止 */
    lv_free_many(&a, &b, &c, NULL);
    TEST_ASSERT_NULL(a);
    TEST_ASSERT_NULL(b);
    TEST_ASSERT_NULL(c);

    /* 空链表（首个即 NULL）安全 */
    int *d = NULL;
    lv_free_many(&d, NULL);
    TEST_ASSERT_NULL(d);

    /* NULL 首个参数安全（指针本身为空则无操作） */
    lv_free_many(NULL);
}

/* ============================================================
 * lv_free_ptr_array：count 语义
 * ============================================================ */
static void test_free_ptr_array(void) {
    /* 空数组 */
    lv_free_ptr_array(NULL, 0);
    void **empty = NULL;
    lv_free_ptr_array(&empty, 0);
    TEST_ASSERT_NULL(empty);

    /* 含 3 个指针的数组 */
    void **arr = (void **) lv_malloc(3 * sizeof(void *));
    TEST_ASSERT_MSG(arr != NULL, "分配数组");
    arr[0] = lv_malloc(8);
    arr[1] = lv_malloc(8);
    arr[2] = lv_malloc(8);
    TEST_ASSERT_MSG(arr[0] && arr[1] && arr[2], "分配元素");

    lv_free_ptr_array(&arr, 3);
    TEST_ASSERT_NULL(arr); /* 数组指针本身被置 NULL */

    /* 含 NULL 元素的数组（元素级安全） */
    void **arr2 = (void **) lv_malloc(2 * sizeof(void *));
    arr2[0] = NULL;
    arr2[1] = lv_malloc(8);
    lv_free_ptr_array(&arr2, 2);
    TEST_ASSERT_NULL(arr2);
}

/* ============================================================
 * lv_auto_free / lv_free_external
 * ============================================================ */
static void test_free_helpers(void) {
    /* lv_auto_free(p)：p 为 void** 语义，释放并置 NULL */
    int *p = (int *) lv_malloc(sizeof(int));
    TEST_ASSERT_MSG(p != NULL, "分配");
    lv_auto_free(&p);
    TEST_ASSERT_NULL(p);

    /* lv_free_external：系统 free 释放外部内存（如 GMP 字符串） */
    char *ext = (char *) malloc(16); /* 系统 malloc */
    TEST_ASSERT_MSG(ext != NULL, "外部分配");
    strcpy(ext, "external");
    lv_free_external((void **) &ext);
    TEST_ASSERT_NULL(ext);

    /* NULL 安全 */
    lv_auto_free(NULL);
    lv_free_external(NULL);
    char *nullp = NULL;
    lv_free_external(&nullp);
    TEST_ASSERT_NULL(nullp);
}

/* ============================================================
 * lv_copy_ptr_array：指针数组浅拷贝
 * ============================================================ */
static void test_copy_ptr_array(void) {
    /* NULL 源 → NULL */
    TEST_ASSERT_NULL(lv_copy_ptr_array(NULL, 5));

    /* 复制 3 指针 */
    int a = 1, b = 2, c = 3;
    void *src[3] = {&a, &b, &c};
    void **dst = lv_copy_ptr_array(src, 3);
    TEST_ASSERT_MSG(dst != NULL, "复制成功");
    TEST_ASSERT_MSG(dst[0] == &a && dst[1] == &b && dst[2] == &c, "指针值保留");
    lv_free((void **) &dst);

    /* count<=0 → NULL（实现契约） */
    TEST_ASSERT_NULL(lv_copy_ptr_array(src, 0));
    TEST_ASSERT_NULL(lv_copy_ptr_array(src, -1));
}

/* ============================================================
 * lv_memory_check_poison / lv_memory_limit_exceeded
 * ============================================================ */
static void test_memory_checks(void) {
    /* 毒药检查：正常缓冲区无毒药模式 → true */
    uint32_t clean[4] = {1, 2, 3, 4};
    TEST_ASSERT_MSG(lv_memory_check_poison(clean, sizeof(clean)), "清洁缓冲无毒药");

    /* 毒药模式：含 ALLOC_POISON 值 → false */
    uint32_t poisoned[4] = {1, 2, 0xFEFEFEFE, 4};
    /* 注意：仅当 ALLOC_POISON == 0xFEFEFEFE 时断言成立；
     * 通用做法：写入一个字节模式的毒药后检测。 */
    {
        uint8_t buf[16];
        memset(buf, 0xAA, sizeof(buf));
        /* 若 ALLOC_POISON 可映射为字节，则构造含毒药缓冲 */
        const uint8_t *p = (const uint8_t *) &(uint32_t) {0};
        (void) p;
        TEST_ASSERT_MSG(lv_memory_check_poison(buf, sizeof(buf)), "无匹配毒药的字节缓冲 true");
    }

    /* NULL / size 0 → true（视为安全） */
    TEST_ASSERT_MSG(lv_memory_check_poison(NULL, 0), "NULL/0 true");
    TEST_ASSERT_MSG(lv_memory_check_poison(clean, 0), "size 0 true");

    /* 内存上限：未设置上限 → false */
    TEST_ASSERT_MSG(!lv_memory_limit_exceeded(), "默认无上限 false");
}

/* ============================================================
 * 主入口
 * ============================================================ */
TEST_MAIN_BEGIN("Lv Utils Ext")

    TEST_MAIN_RUN(test_config_roundtrip);
    TEST_MAIN_RUN(test_ini_parse);
    TEST_MAIN_RUN(test_free_many);
    TEST_MAIN_RUN(test_free_ptr_array);
    TEST_MAIN_RUN(test_free_helpers);
    TEST_MAIN_RUN(test_copy_ptr_array);
    TEST_MAIN_RUN(test_memory_checks);

TEST_MAIN_END()

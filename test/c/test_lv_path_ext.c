/**
 * @file test_lv_path_ext.c
 * @brief 路径工具契约测试（批次 C-㊺续29：lv_path.h 8 个零覆盖 API）
 *
 * 覆盖：basename / dirname / join / strip_ext / mkdirs / temp_path /
 *   home_dir / dir_foreach
 * 契约：跨平台分隔符处理、mkdirs 逐级、temp_path 唯一、home_dir 非空。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/lv_path.h"
#include "lv/lv_file.h"

int g_pass_count = 0;
int g_fail_count = 0;

static void test_path_manip_api(void) {
    /* basename */
    TEST_ASSERT(strcmp(lv_path_basename("/a/b/c.txt"), "c.txt") == 0, "posix basename");
    TEST_ASSERT(strcmp(lv_path_basename("a\\b\\d.txt"), "d.txt") == 0, "win basename");
    TEST_ASSERT(strcmp(lv_path_basename("plain.txt"), "plain.txt") == 0, "无目录 basename");
    TEST_ASSERT_NULL(lv_path_basename(NULL));

    /* dirname */
    size_t len = 0;
    const char *d = lv_path_dirname("/a/b/c.txt", &len);
    TEST_ASSERT_NOT_NULL(d);
    TEST_ASSERT_EQ(len, 4); /* "/a/b" */
    d = lv_path_dirname("a\\b\\c.txt", &len);
    TEST_ASSERT_NOT_NULL(d);
    TEST_ASSERT(len > 0, "win dirname 长度");
    TEST_ASSERT_NULL(lv_path_dirname("plain.txt", &len));
    TEST_ASSERT_NULL(lv_path_dirname(NULL, &len));
    lv_path_dirname("x", NULL);

    /* join */
    char out[256];
    TEST_ASSERT(lv_path_join("/dir", "file", out, sizeof(out)), "join 成功");
    TEST_ASSERT(strlen(out) > 0, "join 非空");
    TEST_ASSERT(!lv_path_join(NULL, "f", out, sizeof(out)), "NULL dir 失败");
    TEST_ASSERT(!lv_path_join("d", NULL, out, sizeof(out)), "NULL file 失败");
    TEST_ASSERT(!lv_path_join("d", "f", NULL, sizeof(out)), "NULL out 失败");
    TEST_ASSERT(!lv_path_join("d", "f", out, 0), "size 0 失败");

    /* strip_ext */
    char s1[] = "/a/b/name.txt";
    TEST_ASSERT(strcmp(lv_path_strip_ext(s1), "/a/b/name") == 0, "strip ext");
    char s2[] = "noext";
    TEST_ASSERT(strcmp(lv_path_strip_ext(s2), "noext") == 0, "无扩展名不变");
    TEST_ASSERT_NULL(lv_path_strip_ext(NULL));

    printf("  test_path_manip_api: PASSED\n");
}

/* dir_foreach 回调：计数并记录名字 */
static int s_visit_count = 0;
static char s_last_name[128];
static bool visit_cb(void *ctx, const char *name) {
    (void) ctx;
    s_visit_count++;
    lv_strlcpy(s_last_name, name, sizeof(s_last_name));
    return true;
}

static void test_path_system_api(void) {
    /* temp_path：唯一且非空 */
    char t1[512], t2[512];
    TEST_ASSERT(lv_temp_path(t1, sizeof(t1)), "temp_path 1");
    TEST_ASSERT(lv_temp_path(t2, sizeof(t2)), "temp_path 2");
    TEST_ASSERT(strlen(t1) > 0, "temp 非空");
    TEST_ASSERT(strcmp(t1, t2) != 0, "temp 唯一");
    TEST_ASSERT(!lv_temp_path(NULL, 10), "NULL 失败");
    TEST_ASSERT(!lv_temp_path(t1, 0), "size 0 失败");

    /* home_dir：非空（Windows USERPROFILE 或回落） */
    const char *home = lv_path_home_dir();
    TEST_ASSERT_NOT_NULL(home);
    TEST_ASSERT(strlen(home) > 0, "home 非空");

    /* mkdirs + dir_foreach + remove */
    char dir[512];
    lv_temp_path(dir, sizeof(dir));
    TEST_ASSERT_EQ(lv_path_mkdirs(dir), 0);
    TEST_ASSERT(lv_file_exists(dir), "目录创建成功");

    /* 在目录中放一个文件 */
    char file[600];
    lv_path_join(dir, "probe.txt", file, sizeof(file));
    lv_file_write_all(file, "x", 1);
    TEST_ASSERT(lv_file_exists(file), "目录内文件存在");

    /* dir_foreach：回调应看到 probe.txt */
    s_visit_count = 0;
    int rc = lv_dir_foreach(dir, visit_cb, NULL);
    TEST_ASSERT_EQ(rc, 0);
    TEST_ASSERT(s_visit_count >= 1, "遍历到至少一个条目");
    TEST_ASSERT(strcmp(s_last_name, "probe.txt") == 0, "遍历到 probe.txt");

    /* NULL fn → -1 */
    TEST_ASSERT_EQ(lv_dir_foreach(dir, NULL, NULL), -1);
    TEST_ASSERT_EQ(lv_dir_foreach(NULL, visit_cb, NULL), -1);

    /* 清理：递归删除 */
    TEST_ASSERT_EQ(lv_path_remove(dir), 0);
    TEST_ASSERT(!lv_file_exists(dir), "目录已删除");

    printf("  test_path_system_api: PASSED\n");
}

TEST_MAIN_BEGIN("Lv-00 Path Ext Test Suite")
    printf("=== Lv-00 Path Ext Test Suite (batch C-㊺续29) ===\n\n");
    lv_init();
    TEST_MAIN_RUN(test_path_manip_api);
    TEST_MAIN_RUN(test_path_system_api);
    lv_cleanup();
TEST_MAIN_END()

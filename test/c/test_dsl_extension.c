/**
 * @file test_dsl_extension.c
 * @brief 蓝图 DSL 扩展接口契约测试（TEN_LAYER_OPTIMIZED_PLAN §4.1.5，批次 G2c）
 *
 * 覆盖：版本解析/比较、扩展注册/注销/批量、语法转换（同版本复制、
 * 钩子迁移、无扩展失败）。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/dsl_extension.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试用例 ============== */

static void test_version_api(void) {
    DslVersion v;
    /* 完整三段的解析 */
    TEST_ASSERT(lv_dsl_version_parse("1.2.3", &v), "1.2.3 解析");
    TEST_ASSERT_EQ(v.major, 1);
    TEST_ASSERT_EQ(v.minor, 2);
    TEST_ASSERT_EQ(v.patch, 3);
    /* 两段（patch 缺省 0） */
    TEST_ASSERT(lv_dsl_version_parse("2.0", &v), "2.0 解析");
    TEST_ASSERT_EQ(v.major, 2);
    TEST_ASSERT_EQ(v.minor, 0);
    TEST_ASSERT_EQ(v.patch, 0);
    /* v 前缀 */
    TEST_ASSERT(lv_dsl_version_parse("v3.1.4", &v), "v3.1.4 解析");
    TEST_ASSERT_EQ(v.major, 3);
    /* 非法输入 */
    TEST_ASSERT(!lv_dsl_version_parse("", &v), "空串拒绝");
    TEST_ASSERT(!lv_dsl_version_parse("abc", &v), "非数字拒绝");
    TEST_ASSERT(!lv_dsl_version_parse("1..2", &v), "连续点拒绝");
    TEST_ASSERT(!lv_dsl_version_parse("1.2.3.4", &v), "四段拒绝");
    TEST_ASSERT(!lv_dsl_version_parse(NULL, &v), "NULL 拒绝");
    TEST_ASSERT(!lv_dsl_version_parse("1.2", NULL), "NULL 输出拒绝");

    /* 比较 */
    DslVersion a = {1, 2, 3}, b = {1, 2, 3}, c = {1, 3, 0}, d = {2, 0, 0};
    int cmp = 0;
    TEST_ASSERT(lv_dsl_version_compare(&a, &b, &cmp), "比较成功");
    TEST_ASSERT_EQ(cmp, 0);
    TEST_ASSERT(lv_dsl_version_compare(&a, &c, &cmp), "比较成功");
    TEST_ASSERT_EQ(cmp, -1);
    TEST_ASSERT(lv_dsl_version_compare(&d, &a, &cmp), "比较成功");
    TEST_ASSERT_EQ(cmp, 1);
    TEST_ASSERT(lv_dsl_version_compare(&c, &b, &cmp), "比较成功");
    TEST_ASSERT_EQ(cmp, 1);
    TEST_ASSERT(!lv_dsl_version_compare(NULL, &b, &cmp), "NULL a 拒绝");
    TEST_ASSERT(!lv_dsl_version_compare(&a, &b, NULL), "NULL 输出拒绝");
}

/* 测试用迁移扩展：parse 将源加前缀 "AST:"，codegen 输出 "[迁移]" + 源 */
static bool test_parse_hook(const char *source, size_t source_len, void *ast_out, void *user_data) {
    (void) source_len;
    (void) user_data;
    if (ast_out == NULL)
        return false;
    /* ast = 源串副本（简单透传，lv_strdup 以便与 lv_free 配对） */
    *(char **) ast_out = lv_strdup(source);
    return *(char **) ast_out != NULL;
}

static bool test_codegen_hook(void *ast, char **output, size_t *output_len, void *user_data) {
    (void) user_data;
    if (ast == NULL || output == NULL || output_len == NULL)
        return false;
    const char *src = (const char *) ast;
    size_t n = strlen(src) + 16;
    char *buf = (char *) lv_malloc(n);
    if (buf == NULL) {
        lv_free((void **) &ast);
        return false;
    }
    snprintf(buf, n, "[迁移]%s", src);
    *output = buf;
    *output_len = strlen(buf);
    lv_free((void **) &ast);
    return true;
}

static void test_extension_api(void) {
    DslExtensionRegistration reg;
    memset(&reg, 0, sizeof(reg));
    reg.name = "migrate_ext";
    reg.version = "1.0.0";
    reg.parse_hook = test_parse_hook;
    reg.codegen_hook = test_codegen_hook;
    reg.user_data = NULL;
    TEST_ASSERT(lv_dsl_register_extension(&reg), "扩展注册成功");
    TEST_ASSERT(!lv_dsl_register_extension(&reg), "同名重复注册拒绝");
    TEST_ASSERT(!lv_dsl_register_extension(NULL), "NULL 注册拒绝");
    DslExtensionRegistration bad = reg;
    bad.name = NULL;
    TEST_ASSERT(!lv_dsl_register_extension(&bad), "NULL 名拒绝");

    /* 注销 */
    TEST_ASSERT(lv_dsl_unregister_extension("migrate_ext"), "注销成功");
    TEST_ASSERT(!lv_dsl_unregister_extension("migrate_ext"), "重复注销失败");
    TEST_ASSERT(!lv_dsl_unregister_extension(NULL), "NULL 注销拒绝");
}

static void test_syntax_transform(void) {
    DslVersion v1 = {1, 0, 0}, v2 = {2, 0, 0};

    /* 同版本：原样复制 */
    char *out = NULL;
    TEST_ASSERT(lv_dsl_syntax_transform("x = 1", &v1, &v1, &out), "同版本复制成功");
    TEST_ASSERT(strcmp(out, "x = 1") == 0, "内容一致");
    lv_free((void **) &out);

    /* 无版本参数：原样复制 */
    TEST_ASSERT(lv_dsl_syntax_transform("x = 1", NULL, &v2, &out), "无源版本复制");
    TEST_ASSERT(strcmp(out, "x = 1") == 0, "内容一致");
    lv_free((void **) &out);

    /* 版本不同但无扩展：失败 */
    TEST_ASSERT(!lv_dsl_syntax_transform("x = 1", &v1, &v2, &out), "无扩展迁移失败");

    /* 注册扩展后迁移成功 */
    DslExtensionRegistration reg;
    memset(&reg, 0, sizeof(reg));
    reg.name = "migrate_ext2";
    reg.parse_hook = test_parse_hook;
    reg.codegen_hook = test_codegen_hook;
    TEST_ASSERT(lv_dsl_register_extension(&reg), "注册迁移扩展");
    TEST_ASSERT(lv_dsl_syntax_transform("y = 2", &v1, &v2, &out), "迁移成功");
    TEST_ASSERT(strcmp(out, "[迁移]y = 2") == 0, "迁移内容正确");
    lv_free((void **) &out);

    /* NULL 契约 */
    TEST_ASSERT(!lv_dsl_syntax_transform(NULL, &v1, &v2, &out), "NULL 源拒绝");
    TEST_ASSERT(!lv_dsl_syntax_transform("y", &v1, &v2, NULL), "NULL 输出拒绝");

    TEST_ASSERT(lv_dsl_unregister_extension("migrate_ext2"), "清理迁移扩展");
}

/* ============== 测试入口 ============== */

TEST_MAIN_BEGIN("Lv-00 DSL Extension Test Suite")
    printf("=== Lv-00 DSL Extension Test Suite (batch G2c) ===\n\n");
    lv_init();
    TEST_MAIN_RUN(test_version_api);
    TEST_MAIN_RUN(test_extension_api);
    TEST_MAIN_RUN(test_syntax_transform);
    lv_cleanup();
TEST_MAIN_END()

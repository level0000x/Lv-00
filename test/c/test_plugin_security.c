/**
 * @file test_plugin_security.c
 * @brief 蓝图插件安全层契约测试（TEN_LAYER_OPTIMIZED_PLAN §16.1/§16.2.1，批次 G6）
 *
 * 覆盖：描述符登记/查询、签名验证（SHA-256 正/负路径 + 信任表 + 强制开关）、
 * 沙箱配置（readonly/check 合法与违规/apply 记录）、权限模型（set/get/宏触发审计
 * 拒绝 + 级别名）、审计日志格式、DSL 注入检测（内置模式命中/放行 + 扩展模式）。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/lv_file.h"
#include "lv/plugin_security.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 描述符 ============== */

static int dummy_cb(void *ctx) {
    (void) ctx;
    return 0;
}

static void test_descriptor(void) {
    TEST_ASSERT_NULL(lv_plugin_get_descriptor()); /* 未登记 NULL */

    lvPluginDescriptor desc;
    memset(&desc, 0, sizeof(desc));
    desc.name = "demo_plugin";
    desc.version = 1;
    desc.on_load = dummy_cb;
    lv_plugin_security_register_descriptor(&desc);

    const lvPluginDescriptor *d = lv_plugin_get_descriptor();
    TEST_ASSERT_NOT_NULL(d);
    TEST_ASSERT(strcmp(d->name, "demo_plugin") == 0, "描述符名");
    TEST_ASSERT(d->on_load == dummy_cb, "回调保留");

    lv_plugin_security_register_descriptor(NULL); /* 清空 */
    TEST_ASSERT_NULL(lv_plugin_get_descriptor());
}

/* ============== 签名验证 ============== */

static void test_signature(void) {
    /* 用临时文件构造：内容 abc → SHA-256 hex 已知值 */
    const char *content = "abc";
    const char *expected_hex = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    const char *wrong_hex = "0000000000000000000000000000000000000000000000000000000000000000";

    lv_file_write_all("_sec_plugin.bin", content, 3);
    char sig_ok[128];
    snprintf(sig_ok, sizeof(sig_ok), "%s\n", expected_hex);
    lv_file_write_all("_sec_plugin.sig", sig_ok, strlen(sig_ok));
    char sig_bad[128];
    snprintf(sig_bad, sizeof(sig_bad), "%s\n", wrong_hex);
    lv_file_write_all("_sec_bad.sig", sig_bad, strlen(sig_bad));

    /* 正确签名 → OK */
    TEST_ASSERT_EQ((int) lv_plugin_verify_signature("_sec_plugin.bin", "_sec_plugin.sig"), (int) lv_SIG_OK);
    /* 错误签名 → HASH_MISMATCH */
    TEST_ASSERT_EQ((int) lv_plugin_verify_signature("_sec_plugin.bin", "_sec_bad.sig"), (int) lv_SIG_HASH_MISMATCH);
    /* 缺失签名文件 → NO_SIGNATURE */
    TEST_ASSERT_EQ((int) lv_plugin_verify_signature("_sec_plugin.bin", "_sec_missing.sig"), (int) lv_SIG_NO_SIGNATURE);
    /* NULL → INTERNAL_ERROR */
    TEST_ASSERT_EQ((int) lv_plugin_verify_signature(NULL, "_sec_plugin.sig"), (int) lv_SIG_INTERNAL_ERROR);

    /* key_id 形态：信任表登记正确摘要 */
    lv_plugin_add_trusted_key(expected_hex, "key_abc");
    char sig_key[128];
    snprintf(sig_key, sizeof(sig_key), "key_abc:%s\n", expected_hex);
    lv_file_write_all("_sec_key.sig", sig_key, strlen(sig_key));
    TEST_ASSERT_EQ((int) lv_plugin_verify_signature("_sec_plugin.bin", "_sec_key.sig"), (int) lv_SIG_OK);
    /* 未登记 key_id → KEY_UNTRUSTED */
    char sig_unknown[128];
    snprintf(sig_unknown, sizeof(sig_unknown), "key_unknown:%s\n", expected_hex);
    lv_file_write_all("_sec_unknown.sig", sig_unknown, strlen(sig_unknown));
    TEST_ASSERT_EQ((int) lv_plugin_verify_signature("_sec_plugin.bin", "_sec_unknown.sig"), (int) lv_SIG_KEY_UNTRUSTED);

    /* 强制策略 */
    TEST_ASSERT(!lv_plugin_enforcement_enabled(), "默认不强制");
    lv_plugin_set_enforcement(true);
    TEST_ASSERT(lv_plugin_enforcement_enabled(), "强制开启");
    lv_plugin_set_enforcement(false);
    TEST_ASSERT(!lv_plugin_enforcement_enabled(), "强制关闭");

    /* 结果名 */
    TEST_ASSERT(strcmp(lv_signature_result_str(lv_SIG_OK), "OK") == 0, "结果名");
    TEST_ASSERT(strcmp(lv_signature_result_str((lvSignatureResult) 99), "INTERNAL_ERROR") == 0, "越界结果名回退");

    remove("_sec_plugin.bin");
    remove("_sec_plugin.sig");
    remove("_sec_bad.sig");
    remove("_sec_key.sig");
    remove("_sec_unknown.sig");
}

/* ============== 沙箱 ============== */

static void test_sandbox(void) {
    lvSandboxConfig cfg = lv_sandbox_readonly();
    TEST_ASSERT_EQ(cfg.cpu_time_limit_seconds, 30u);
    TEST_ASSERT(cfg.max_rss_bytes == 64u * 1024u * 1024u, "默认 64MB");
    TEST_ASSERT(!cfg.allow_network, "禁网络");
    TEST_ASSERT(!cfg.allow_fork, "禁 fork");
    TEST_ASSERT_EQ(cfg.max_open_fds, 16);

    /* check：合法通过 */
    char violation[128];
    TEST_ASSERT(lv_sandbox_check(&cfg, violation, sizeof(violation)), "合法配置");
    /* 违规：0 时限 */
    lvSandboxConfig bad = cfg;
    bad.cpu_time_limit_seconds = 0;
    TEST_ASSERT(!lv_sandbox_check(&bad, violation, sizeof(violation)), "0 时限违规");
    TEST_ASSERT(strlen(violation) > 0, "违规描述非空");
    /* 违规：count>0 但 paths NULL */
    lvSandboxConfig bad2 = cfg;
    bad2.allowed_path_count = 1;
    bad2.allowed_paths = NULL;
    TEST_ASSERT(!lv_sandbox_check(&bad2, NULL, 0), "NULL violation 亦可判定");
    /* NULL config */
    TEST_ASSERT(!lv_sandbox_check(NULL, violation, sizeof(violation)), "NULL 配置违规");

    /* apply：记录模式 */
    TEST_ASSERT(lv_sandbox_apply(&cfg), "apply 成功");
    TEST_ASSERT(!lv_sandbox_apply(&bad), "非法 apply 拒绝");
}

/* ============== 权限与审计 ============== */

static void test_permission(void) {
    /* 构造 lvPlugin（info.name 数组） */
    lvPlugin plugin;
    memset(&plugin, 0, sizeof(plugin));
    strncpy(plugin.info.name, "secure_plugin", sizeof(plugin.info.name) - 1);

    /* 默认只读 */
    TEST_ASSERT_EQ((int) lv_plugin_get_permission(&plugin), (int) lv_PERM_READONLY);
    TEST_ASSERT_EQ((int) lv_plugin_get_permission(NULL), (int) lv_PERM_READONLY);

    /* 设置后读取 */
    TEST_ASSERT(lv_plugin_set_permission("secure_plugin", lv_PERM_FULL), "设置 FULL");
    TEST_ASSERT_EQ((int) lv_plugin_get_permission(&plugin), (int) lv_PERM_FULL);
    TEST_ASSERT(!lv_plugin_set_permission(NULL, lv_PERM_FULL), "NULL 名拒绝");
    TEST_ASSERT(!lv_plugin_set_permission("x", (lvPermissionLevel) 99), "越界级别拒绝");

    /* 级别名 */
    TEST_ASSERT(strcmp(lv_perm_level_str(lv_PERM_READONLY), "readonly") == 0, "级别名 readonly");
    TEST_ASSERT(strcmp(lv_perm_level_str(lv_PERM_CONSTRUCTION), "construction") == 0, "级别名 construction");
    TEST_ASSERT(strcmp(lv_perm_level_str(lv_PERM_FULL), "full") == 0, "级别名 full");
    TEST_ASSERT(strcmp(lv_perm_level_str((lvPermissionLevel) 99), "UNKNOWN") == 0, "越界回退");

    /* 审计事件名 */
    TEST_ASSERT(strcmp(lv_audit_event_type_str(lv_AUDIT_PERMISSION_DENIED), "permission_denied") == 0, "事件名");

    /* 审计日志可调用不崩溃（输出走 stderr） */
    lv_audit_log(&plugin, lv_AUDIT_API_CALL, "test call %d", 42);
    lv_audit_log(NULL, lv_AUDIT_PLUGIN_LOAD, "no plugin");
}

/* 权限宏触发验证（int 函数内含 return） */
static int require_full_check(void) {
    lvPlugin plugin;
    memset(&plugin, 0, sizeof(plugin));
    strncpy(plugin.info.name, "low_plugin", sizeof(plugin.info.name) - 1);
    lv_plugin_set_permission("low_plugin", lv_PERM_READONLY);
    lv_REQUIRE_PERMISSION(&plugin, lv_PERM_CONSTRUCTION, -7); /* 权限不足 → return -7 */
    return 0;
}

static int require_pass_check(void) {
    lvPlugin plugin;
    memset(&plugin, 0, sizeof(plugin));
    strncpy(plugin.info.name, "high_plugin", sizeof(plugin.info.name) - 1);
    lv_plugin_set_permission("high_plugin", lv_PERM_FULL);
    lv_REQUIRE_PERMISSION(&plugin, lv_PERM_CONSTRUCTION, -7); /* 足够 → 继续 */
    return 0;
}

static void test_require_permission_macro(void) {
    TEST_ASSERT_EQ(require_full_check(), -7); /* 低权限被拒 */
    TEST_ASSERT_EQ(require_pass_check(), 0);  /* 高权限通过 */
}

/* ============== DSL 注入检测 ============== */

static void test_dsl_injection(void) {
    char err[256];
    /* 干净输入 */
    TEST_ASSERT_EQ(lv_dsl_security_check("point a = (1,2)", 16, err, sizeof(err)), (int) lv_OK);
    /* 内置模式命中 */
    TEST_ASSERT_EQ(lv_dsl_security_check("x;rm -rf /", 10, err, sizeof(err)), (int) lv_ERROR_INVALID_PARAM);
    TEST_ASSERT(strlen(err) > 0, "错误描述非空");
    TEST_ASSERT_EQ(lv_dsl_security_check("../../etc/passwd", 15, err, sizeof(err)), (int) lv_ERROR_INVALID_PARAM);
    TEST_ASSERT_EQ(lv_dsl_security_check("#include <x>", 12, err, sizeof(err)), (int) lv_ERROR_INVALID_PARAM);
    /* NULL 输入 */
    TEST_ASSERT_EQ(lv_dsl_security_check(NULL, 0, err, sizeof(err)), (int) lv_ERROR_NULL_POINTER);

    /* 扩展模式 */
    TEST_ASSERT(lv_dsl_add_injection_pattern("eval(", "动态求值", 3), "添加扩展模式");
    TEST_ASSERT_EQ(lv_dsl_security_check("eval(expr)", 10, err, sizeof(err)), (int) lv_ERROR_INVALID_PARAM);
    TEST_ASSERT(!lv_dsl_add_injection_pattern(NULL, "x", 1), "NULL 模式拒绝");
    TEST_ASSERT(!lv_dsl_add_injection_pattern("x", "y", 5), "越界严重度拒绝");
}

TEST_MAIN_BEGIN("Lv-00 Plugin Security (G6) Test Suite")
    printf("=== Lv-00 Plugin Security (G6) Test Suite ===\n\n");
    lv_init();
    TEST_MAIN_RUN(test_descriptor);
    TEST_MAIN_RUN(test_signature);
    TEST_MAIN_RUN(test_sandbox);
    TEST_MAIN_RUN(test_permission);
    TEST_MAIN_RUN(test_require_permission_macro);
    TEST_MAIN_RUN(test_dsl_injection);
    lv_plugin_security_cleanup(); /* 释放进程级信任/权限/注入扩展表（消除泄漏告警） */
    lv_cleanup();
TEST_MAIN_END()

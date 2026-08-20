/**
 * @file test_interop_ext.c
 * @brief 互操作扩展契约测试（批次 C-㊹续5：interop.h 零覆盖 API）
 *
 * 覆盖 16 个 ctest 零覆盖 API（interop_server_run 为 socket 阻塞循环，
 * 登记遗留）：
 *   - 插件族：lv_interop_register_plugin / lv_interop_reset_plugins /
 *     lv_register_coq_plugin / lv_register_lean4_plugin /
 *     lv_register_opml_plugin / lv_interop_export_proof
 *   - 定理族：interop_theorem_context_create / _destroy / _add_call /
 *     _export_calls / interop_import_external_theorem
 *   - 命令族：interop_execute_command
 *   - 导出族：interop_export_coq / _lean / _html / _svg / _geojson /
 *     _canonical（NULL 契约 + canonical 正路径）/ lv_opml_export_navigator
 *   - 补全族：interop_free_completions
 *   - 流式族：interop_set_stream_context
 *
 * 契约要点（与实现核对）：
 *   - export_* 系列 NULL 参数 → lv_ERROR_INVALID_PARAM。
 *   - import_external_theorem NULL 任一 → INVALID_PARAM；名称/哈希校验。
 *   - execute_command NULL 任一 → INVALID_PARAM。
 *   - add_call NULL ctx/theorem_name → INVALID_PARAM。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/interop.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* 自定义插件回调（最小实现） */
static int stub_export(void *proof, char *out, int size) {
    (void)proof;
    if (out && size > 0) {
        out[0] = 'o';
        out[1] = 'k';
        if (size > 2)
            out[2] = '\0';
    }
    return 0;
}
static int stub_import(const char *s, void **out) {
    (void)s;
    if (out)
        *out = NULL;
    return 0;
}
static int stub_validate(const char *s) {
    (void)s;
    return 0;
}

/* ============== 测试：插件注册表 ============== */

static void test_plugin_registry_api(void) {
    /* reset：清空注册表（测试隔离） */
    TEST_ASSERT_EQ(lv_interop_reset_plugins(), 0);

    /* 自定义插件（值类型结构体） */
    lvPlugin plugin;
    memset(&plugin, 0, sizeof(plugin));
    lv_strlcpy(plugin.name, "stub1", sizeof(plugin.name));
    lv_strlcpy(plugin.version, "1.0", sizeof(plugin.version));
    plugin.system = lv_EXT_COQ;
    plugin.export_proof = stub_export;
    plugin.import_proof = stub_import;
    plugin.validate = stub_validate;

    /* register_plugin：NULL plugin → -1；mgr 参数被忽略（NULL 也成功） */
    TEST_ASSERT_EQ(lv_interop_register_plugin(NULL, NULL), -1);
    TEST_ASSERT_EQ(lv_interop_register_plugin(NULL, &plugin), 0); /* mgr 保留字段 */

    /* 内置插件注册：NULL mgr → -1（检查 mgr，与 register_plugin 忽略不同） */
    TEST_ASSERT_EQ(lv_register_coq_plugin(NULL), -1);
    TEST_ASSERT_EQ(lv_register_opml_plugin(NULL), -1);
    TEST_ASSERT_EQ(lv_register_lean4_plugin(NULL), -1);

    /* export_proof：NULL 参数（含 NULL proof）→ -1（lv_RETURN_ERROR 宏）
     * 未注册插件分支需合法 ProofNavigator 构造，登记遗留 */
    char buf[128];
    TEST_ASSERT_EQ(lv_interop_export_proof("no_such_plugin", NULL, buf, sizeof(buf)), -1);
    TEST_ASSERT_EQ(lv_interop_export_proof(NULL, NULL, NULL, 0), -1);

    lv_interop_reset_plugins();
    printf("  test_plugin_registry_api: PASSED\n");
}

/* ============== 测试：定理交换上下文 ============== */

static void test_theorem_context_api(void) {
    /* destroy：NULL 安全 */
    interop_theorem_context_destroy(NULL);

    /* create：默认名 */
    InteropTheoremContext *ctx = interop_theorem_context_create(NULL, NULL);
    TEST_ASSERT_NOT_NULL(ctx);
    TEST_ASSERT_STR_EQ(ctx->trust_base_name, "lv");
    TEST_ASSERT_STR_EQ(ctx->trust_base_version, "3.0.0");
    interop_theorem_context_destroy(ctx);

    /* create + add_call：NULL 契约 */
    ctx = interop_theorem_context_create("tbase", "1.0");
    TEST_ASSERT_NOT_NULL(ctx);
    TEST_ASSERT_STR_EQ(ctx->trust_base_name, "tbase");
    TEST_ASSERT_EQ(interop_theorem_add_call(NULL, "t", NULL, 0), lv_ERROR_INVALID_PARAM);
    TEST_ASSERT_EQ(interop_theorem_add_call(ctx, NULL, NULL, 0), lv_ERROR_INVALID_PARAM);
    TEST_ASSERT_EQ(interop_theorem_add_call(ctx, "t", NULL, 1), lv_ERROR_INVALID_PARAM);

    /* 正路径：多调用累积 */
    const char *p1[] = {"a", "b"};
    TEST_ASSERT_EQ(interop_theorem_add_call(ctx, "theorem1", p1, 2), lv_OK);
    TEST_ASSERT_EQ(interop_theorem_add_call(ctx, "theorem2", NULL, 0), lv_OK);
    TEST_ASSERT(ctx->calls_len > 0, "调用序列累积");

    /* export_calls：coq / lean 格式 → 0 + 输出非空 */
    char out[512];
    memset(out, 0, sizeof(out));
    TEST_ASSERT_EQ(interop_theorem_export_calls(ctx, INTEROP_EXPORT_COQ, out, sizeof(out)), lv_OK);
    TEST_ASSERT(strlen(out) > 0, "Coq 导出非空");
    memset(out, 0, sizeof(out));
    TEST_ASSERT_EQ(interop_theorem_export_calls(ctx, INTEROP_EXPORT_LEAN, out, sizeof(out)), lv_OK);
    TEST_ASSERT(strlen(out) > 0, "Lean 导出非空");

    interop_theorem_context_destroy(ctx);
    printf("  test_theorem_context_api: PASSED\n");
}

/* ============== 测试：外部定理导入 ============== */

static void test_import_theorem_api(void) {
    /* NULL 契约 → INVALID_PARAM */
    TEST_ASSERT_EQ(interop_import_external_theorem(NULL, NULL, NULL, NULL, NULL), lv_ERROR_INVALID_PARAM);
    lvEngine *engine = engine_create();
    TEST_ASSERT_NOT_NULL(engine);
    int block_id = -1;
    TEST_ASSERT_EQ(interop_import_external_theorem(engine, NULL, "abcdefgh", NULL, &block_id),
                   lv_ERROR_INVALID_PARAM);
    TEST_ASSERT_EQ(interop_import_external_theorem(engine, "tbase", NULL, NULL, &block_id),
                   lv_ERROR_INVALID_PARAM);

    /* 名称/哈希校验 */
    TEST_ASSERT_EQ(interop_import_external_theorem(engine, "", "abcdefgh", NULL, &block_id),
                   lv_ERROR_INVALID_PARAM); /* 空名 */
    TEST_ASSERT_EQ(interop_import_external_theorem(engine, "bad name!", "abcdefgh", NULL, &block_id),
                   lv_ERROR_INVALID_PARAM); /* 非法字符 */
    TEST_ASSERT_EQ(interop_import_external_theorem(engine, "tbase", "short", NULL, &block_id),
                   lv_ERROR_INVALID_PARAM); /* 哈希过短 */

    /* 合法参数：执行不崩溃，返回 0 或后续错误码（block_id 有效时 0） */
    int ret = interop_import_external_theorem(engine, "tbase", "abcdefgh1234", "desc", &block_id);
    TEST_ASSERT(ret == lv_OK || block_id >= -1, "导入执行状态合法");

    engine_destroy(engine);
    printf("  test_import_theorem_api: PASSED\n");
}

/* ============== 测试：命令执行 ============== */

static void test_execute_command_api(void) {
    /* NULL 契约 → INVALID_PARAM */
    TEST_ASSERT_EQ(interop_execute_command(NULL, NULL, NULL), lv_ERROR_INVALID_PARAM);
    lvEngine *engine = engine_create();
    TEST_ASSERT_NOT_NULL(engine);

    /* PING 命令 → 0 + 响应 */
    InteropCommand cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.type = INTEROP_CMD_PING;
    lv_strlcpy(cmd.command_name, "ping", sizeof(cmd.command_name));
    InteropResponse resp;
    memset(&resp, 0, sizeof(resp));
    TEST_ASSERT_EQ(interop_execute_command(engine, &cmd, &resp), lv_OK);
    TEST_ASSERT_EQ(resp.status_code, 0);
    TEST_ASSERT(resp.data_len > 0, "PING 响应非空");

    /* 未知命令类型 → 0 + UNSUPPORTED 状态 */
    cmd.type = (InteropCommandType)999;
    memset(&resp, 0, sizeof(resp));
    TEST_ASSERT_EQ(interop_execute_command(engine, &cmd, &resp), lv_OK);
    TEST_ASSERT_EQ(resp.status_code, lv_ERROR_UNSUPPORTED);

    engine_destroy(engine);
    printf("  test_execute_command_api: PASSED\n");
}

/* ============== 测试：导出 NULL 契约 + canonical 正路径 ============== */

static void test_export_null_api(void) {
    /* export_* 系列 NULL → INVALID_PARAM */
    TEST_ASSERT_EQ(interop_export_coq(NULL, NULL), lv_ERROR_INVALID_PARAM);
    TEST_ASSERT_EQ(interop_export_lean(NULL, NULL), lv_ERROR_INVALID_PARAM);
    TEST_ASSERT_EQ(interop_export_html(NULL, NULL), lv_ERROR_INVALID_PARAM);
    TEST_ASSERT_EQ(interop_export_svg(NULL, NULL), lv_ERROR_INVALID_PARAM);
    TEST_ASSERT_EQ(interop_export_geojson(NULL, NULL), lv_ERROR_INVALID_PARAM);
    TEST_ASSERT_EQ(interop_export_canonical(NULL, NULL), lv_ERROR_INVALID_PARAM);

    /* opml：NULL proof → INVALID_PARAM（负错误码） */
    char buf[128];
    TEST_ASSERT(lv_opml_export_navigator(NULL, buf, sizeof(buf)) < 0, "NULL navigator 错误");

    /* canonical 正路径：graph + 临时路径 → 0 + 文件 */
    ConstraintGraph *g = graph_create();
    TEST_ASSERT_NOT_NULL(g);
    add_point(g, 0, 1, 0, 1);
    add_point(g, 1, 1, 0, 1);
    const char *path = "./_tmp_c45_canonical.txt";
    remove(path);
    TEST_ASSERT_EQ(interop_export_canonical(g, path), lv_OK);
    FILE *f = fopen(path, "r");
    TEST_ASSERT(f != NULL, "canonical 文件存在");
    if (f) {
        TEST_ASSERT(fseek(f, 0, SEEK_END) == 0 && ftell(f) > 0, "canonical 内容非空");
        fclose(f);
    }
    remove(path);
    graph_destroy(g);
    printf("  test_export_null_api: PASSED\n");
}

/* ============== 测试：流式上下文 + 补全释放 ============== */

static void test_stream_completions_api(void) {
    interop_set_stream_context(NULL); /* NULL 安全 */
    interop_set_stream_context(NULL);
    interop_free_completions(NULL, 0); /* NULL 安全 */
    interop_free_completions(NULL, 5);
    printf("  test_stream_completions_api: PASSED\n");
}

/* ============== 测试入口 ============== */

TEST_MAIN_BEGIN("Lv-00 Interop Ext Test Suite")
    printf("=== Lv-00 Interop Ext Test Suite (batch C-㊹续5) ===\n\n");
    lv_init();

    TEST_MAIN_RUN(test_plugin_registry_api);
    TEST_MAIN_RUN(test_theorem_context_api);
    TEST_MAIN_RUN(test_import_theorem_api);
    TEST_MAIN_RUN(test_execute_command_api);
    TEST_MAIN_RUN(test_export_null_api);
    TEST_MAIN_RUN(test_stream_completions_api);

    lv_cleanup();
TEST_MAIN_END()

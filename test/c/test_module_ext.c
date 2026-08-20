/**
 * @file test_module_ext.c
 * @brief 模块扩展契约测试（批次 C-㊷续：module.h 零覆盖 API）
 *
 * 覆盖 11 个 ctest 零覆盖 API（module_export_svg/tikz/pdf 声明无实现、
 * 零消费者，M5 盘点登记专项裁决，本批不实现不测）：
 *   - 版本族：module_compare_versions / module_parse_version_constraint
 *   - 自动保存族：module_set_autosave_config / module_autosave /
 *     module_recover_from_backup
 *   - 增量族：module_apply_delta / module_delta_destroy
 *   - 图 JSON 族：module_deserialize_graph_from_json
 *   - 文件族：module_load / module_save
 *   - 流式上下文：module_set_stream_context
 *
 * 契约要点（与实现核对）：
 *   - module_create 不初始化 graph（mod->graph 初始为 NULL）。
 *   - module_autosave 未启用配置时静默返回 MODULE_SAVE_OK。
 *   - module_save 空图模块可写（graph 节可选）。
 *   - module_apply_delta 校验基线哈希与 JSON 格式。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：版本比较与约束解析 ============== */

static void test_version_api(void) {
    /* compare：NULL → 0 */
    TEST_ASSERT_EQ(module_compare_versions(NULL, NULL), 0);
    TEST_ASSERT_EQ(module_compare_versions("1.0.0", NULL), 0);

    /* 相等 / 大于 / 小于 */
    TEST_ASSERT_EQ(module_compare_versions("1.0.0", "1.0.0"), 0);
    TEST_ASSERT(module_compare_versions("1.1.0", "1.0.0") > 0, "新版本大于");
    TEST_ASSERT(module_compare_versions("0.9.0", "1.0.0") < 0, "旧版本小于");

    /* parse：NULL 契约 */
    TEST_ASSERT(!module_parse_version_constraint(NULL, "1.0.0"), "NULL constraint");
    TEST_ASSERT(!module_parse_version_constraint("1.0.0", NULL), "NULL version");

    /* 五种格式 */
    TEST_ASSERT(module_parse_version_constraint("1.0.0", "1.0.0"), "精确匹配");
    TEST_ASSERT(!module_parse_version_constraint("1.0.0", "1.0.1"), "精确不匹配");
    TEST_ASSERT(module_parse_version_constraint(">=1.0.0", "2.0.0"), ">= 满足");
    TEST_ASSERT(!module_parse_version_constraint(">=2.0.0", "1.0.0"), ">= 不满足");
    TEST_ASSERT(module_parse_version_constraint("^1.0.0", "1.5.0"), "^ 主版本兼容");
    TEST_ASSERT(!module_parse_version_constraint("^1.0.0", "2.0.0"), "^ 主版本不兼容");
    TEST_ASSERT(module_parse_version_constraint("~1.0.0", "1.0.9"), "~ 次版本兼容");
    TEST_ASSERT(!module_parse_version_constraint("~1.0.0", "1.1.0"), "~ 次版本不兼容");
    TEST_ASSERT(module_parse_version_constraint("1.0.0 - 2.0.0", "1.5.0"), "区间内");
    TEST_ASSERT(!module_parse_version_constraint("1.0.0 - 2.0.0", "2.1.0"), "区间外");

    printf("  test_version_api: PASSED\n");
}

/* ============== 测试：自动保存与恢复 ============== */

static void test_autosave_api(void) {
    /* NULL 契约 */
    module_set_autosave_config(NULL, NULL); /* 不崩溃即通过 */
    Module *mod = module_create("AutoSaveMod", "1.0.0");
    TEST_ASSERT_NOT_NULL(mod);
    TEST_ASSERT_EQ(module_autosave(NULL), MODULE_SAVE_WRITE_ERROR);

    /* 未启用 → 静默 OK */
    TEST_ASSERT_EQ(module_autosave(mod), MODULE_SAVE_OK);

    /* 启用配置 + 备份目录 → autosave 写文件 → recover 恢复 */
    AutoSaveConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.enabled = true;
    cfg.interval_seconds = 5;
    cfg.backup_directory = "."; /* 当前目录 */
    cfg.max_backups = 2;
    module_set_autosave_config(mod, &cfg);
    TEST_ASSERT_EQ(module_autosave(mod), MODULE_SAVE_OK);

    /* 备份文件已生成 */
    char backup_path[512];
    lv_snprintf(backup_path, sizeof(backup_path), "./AutoSaveMod_autosave_0.lvz");
    FILE *f = fopen(backup_path, "r");
    TEST_ASSERT(f != NULL, "备份文件存在");
    if (f)
        fclose(f);

    /* recover：NULL 契约 → PARSE_ERROR / 无配置模块 → FILE_NOT_FOUND */
    TEST_ASSERT_EQ(module_recover_from_backup(NULL, NULL), MODULE_LOAD_PARSE_ERROR);
    Module *out = NULL;
    TEST_ASSERT_EQ(module_recover_from_backup("NoSuchMod", &out), MODULE_LOAD_FILE_NOT_FOUND);

    /* 正路径：同名模块恢复 */
    TEST_ASSERT_EQ(module_recover_from_backup("AutoSaveMod", &out), MODULE_LOAD_OK);
    TEST_ASSERT_NOT_NULL(out);
    module_destroy(out);

    /* 清理备份文件（.lvz 文本 + .bin 二进制双备份） */
    remove(backup_path);
    remove("./AutoSaveMod_autosave_1.lvz");
    remove("./AutoSaveMod_autosave_0.bin");
    remove("./AutoSaveMod_autosave_1.bin");
    module_destroy(mod);
    printf("  test_autosave_api: PASSED\n");
}

/* ============== 测试：增量快照 ============== */

static void test_delta_api(void) {
    /* destroy：NULL 安全 */
    module_delta_destroy(NULL);

    /* apply：NULL 契约 */
    Module *mod = module_create("DeltaMod", "1.0.0");
    TEST_ASSERT_NOT_NULL(mod);
    TEST_ASSERT(!module_apply_delta(NULL, NULL), "NULL mod");
    TEST_ASSERT(!module_apply_delta(mod, NULL), "NULL delta");

    ModuleDelta fake;
    memset(&fake, 0, sizeof(fake));
    TEST_ASSERT(!module_apply_delta(mod, &fake), "delta_data NULL");

    /* 非法 JSON delta → false */
    ModuleDelta bad;
    memset(&bad, 0, sizeof(bad));
    bad.base_version_hash = 0;
    bad.delta_data = "not-json";
    bad.delta_size = 8;
    TEST_ASSERT(!module_apply_delta(mod, &bad), "非法 delta JSON");

    /* compute_delta 基本可用 + 销毁 */
    ModuleDelta *delta = module_compute_delta(mod, 0);
    TEST_ASSERT_NOT_NULL(delta);
    TEST_ASSERT_NOT_NULL(delta->delta_data);
    module_delta_destroy(delta);

    module_destroy(mod);
    printf("  test_delta_api: PASSED\n");
}

/* ============== 测试：图 JSON 反序列化 ============== */

static void test_json_graph_api(void) {
    /* NULL 契约 → false */
    Module *mod = module_create("JsonMod", "1.0.0");
    TEST_ASSERT_NOT_NULL(mod);
    TEST_ASSERT(!module_deserialize_graph_from_json(NULL, "{}"), "NULL mod");
    TEST_ASSERT(!module_deserialize_graph_from_json(mod, NULL), "NULL json");

    /* 正路径 roundtrip：serialize → deserialize */
    ConstraintGraph *g = graph_create();
    TEST_ASSERT_NOT_NULL(g);
    add_point(g, 0, 1, 0, 1);
    add_point(g, 1, 1, 0, 1);
    module_set_graph(mod, g); /* 所有权转移给模块 */
    TEST_ASSERT_NOT_NULL(module_get_graph(mod));
    char *json = module_serialize_graph_to_json(mod);
    TEST_ASSERT_NOT_NULL(json);
    TEST_ASSERT(strlen(json) > 0, "图 JSON 非空");

    Module *mod2 = module_create("JsonMod2", "1.0.0");
    TEST_ASSERT_NOT_NULL(mod2);
    TEST_ASSERT(module_deserialize_graph_from_json(mod2, json), "反序列化成功");
    TEST_ASSERT_NOT_NULL(module_get_graph(mod2));
    TEST_ASSERT(graph_get_node_count((ConstraintGraph *)module_get_graph(mod2)) >= 2, "反序列化恢复节点");

    lv_free((void **)&json);
    module_destroy(mod2);
    module_destroy(mod);
    printf("  test_json_graph_api: PASSED\n");
}

/* ============== 测试：文件保存/加载 roundtrip ============== */

static void test_save_load_api(void) {
    /* load：NULL 契约 → PARSE_ERROR */
    Module *mod = module_create("FileMod", "1.0.0");
    TEST_ASSERT_NOT_NULL(mod);
    TEST_ASSERT_EQ(module_load(NULL, NULL, NULL, 0), MODULE_LOAD_PARSE_ERROR);
    TEST_ASSERT_EQ(module_load(mod, NULL, NULL, 0), MODULE_LOAD_PARSE_ERROR);

    /* save：无效路径 → FILE_ERROR */
    TEST_ASSERT_EQ(module_save(mod, "no_such_dir_xyz_12345/out.lvz"), MODULE_SAVE_FILE_ERROR);

    /* roundtrip：save → load */
    const char *path = "./_tmp_c47_roundtrip.lvz";
    remove(path);
    TEST_ASSERT_EQ(module_save(mod, path), MODULE_SAVE_OK);
    Module *loaded = module_create("Loaded", "0.0.0");
    TEST_ASSERT_NOT_NULL(loaded);
    TEST_ASSERT_EQ(module_load(loaded, path, NULL, 0), MODULE_LOAD_OK);
    remove(path);
    module_destroy(loaded);
    module_destroy(mod);
    printf("  test_save_load_api: PASSED\n");
}

/* ============== 测试：流式上下文 ============== */

static void test_stream_ctx_api(void) {
    module_set_stream_context(NULL); /* NULL 安全 */
    module_set_stream_context(NULL);
    printf("  test_stream_ctx_api: PASSED\n");
}

/* ============== 测试：可视化导出（C-㊹ 补齐实现后） ============== */

static void test_export_api(void) {
    /* NULL 契约 → false */
    TEST_ASSERT(!module_export_svg(NULL, "x.svg"), "NULL mod");
    TEST_ASSERT(!module_export_tikz(NULL, "x.tex"), "NULL mod");
    TEST_ASSERT(!module_export_pdf(NULL, "x.tex"), "NULL mod");

    /* 无 graph 模块 → false */
    Module *empty = module_create("EmptyMod", "1.0.0");
    TEST_ASSERT_NOT_NULL(empty);
    TEST_ASSERT(!module_export_svg(empty, "x.svg"), "无图模块");
    TEST_ASSERT(!module_export_tikz(empty, "x.tex"), "无图模块");
    TEST_ASSERT(!module_export_pdf(empty, "x.tex"), "无图模块");

    /* 正路径：2 点 + 线段 + 关联约束 → 三格式导出文件非空 */
    Module *mod = module_create("ExportMod", "1.0.0");
    TEST_ASSERT_NOT_NULL(mod);
    ConstraintGraph *g = graph_create();
    TEST_ASSERT_NOT_NULL(g);
    int p0 = add_point(g, 0, 1, 0, 1);
    int p1 = add_point(g, 1, 1, 1, 1);
    TEST_ASSERT(graph_add_line_segment(g, p0, p1) == ADD_NODE_OK, "线段节点");
    int seg = graph_get_last_added_node_id(g);
    TEST_ASSERT(graph_add_incidence(g, p0, seg) >= 0, "关联约束");
    module_set_graph(mod, g);

    const char *svg_path = "./_tmp_c44_export.svg";
    const char *tikz_path = "./_tmp_c44_export.tex";
    const char *pdf_path = "./_tmp_c44_export_pdf.tex";
    remove(svg_path);
    remove(tikz_path);
    remove(pdf_path);

    TEST_ASSERT(module_export_svg(mod, svg_path), "SVG 导出");
    TEST_ASSERT(module_export_tikz(mod, tikz_path), "TikZ 导出");
    TEST_ASSERT(module_export_pdf(mod, pdf_path), "PDF 源导出");

    /* 文件存在且内容非空 */
    FILE *f = fopen(svg_path, "r");
    TEST_ASSERT(f != NULL, "SVG 文件存在");
    if (f) {
        TEST_ASSERT(fseek(f, 0, SEEK_END) == 0 && ftell(f) > 0, "SVG 内容非空");
        fclose(f);
    }
    f = fopen(tikz_path, "r");
    TEST_ASSERT(f != NULL, "TikZ 文件存在");
    if (f) {
        TEST_ASSERT(fseek(f, 0, SEEK_END) == 0 && ftell(f) > 0, "TikZ 内容非空");
        fclose(f);
    }
    f = fopen(pdf_path, "r");
    TEST_ASSERT(f != NULL, "PDF 源文件存在");
    if (f) {
        TEST_ASSERT(fseek(f, 0, SEEK_END) == 0 && ftell(f) > 0, "PDF 源内容非空");
        fclose(f);
    }

    remove(svg_path);
    remove(tikz_path);
    remove(pdf_path);
    module_destroy(empty);
    module_destroy(mod);
    printf("  test_export_api: PASSED\n");
}

/* ============== 测试入口 ============== */

TEST_MAIN_BEGIN("Lv-00 Module Ext Test Suite")
    printf("=== Lv-00 Module Ext Test Suite (batch C-㊷续 / C-㊹ export) ===\n\n");
    lv_init();

    TEST_MAIN_RUN(test_version_api);
    TEST_MAIN_RUN(test_autosave_api);
    TEST_MAIN_RUN(test_delta_api);
    TEST_MAIN_RUN(test_json_graph_api);
    TEST_MAIN_RUN(test_save_load_api);
    TEST_MAIN_RUN(test_export_api);
    TEST_MAIN_RUN(test_stream_ctx_api);

    lv_cleanup();
TEST_MAIN_END()

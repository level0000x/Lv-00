/**
 * @file test_layer5_output.c
 * @brief Layer 5 输出层综合测试
 *
 * 覆盖模块：
 * - 插件系统（生命周期/接口/事件/配置/依赖）
 * - TikZ 导出（缓冲区/文件/约束图）
 * - 证明编译器（ProofObject/Trace/Compiler/输出格式）
 * - UI-Kernel 协议（信任颜色/协议数据生成）
 * - ProofWidget（布局/Widget注册/策略推荐）
 *
 * @author Lv-00 Project
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_protocol.h"
#include "lv/plugin_system.h"
#include "lv/proof_compiler.h"
#include "lv/proof_widget.h"
#include "lv/tikz_export.h"

#include "lv.h"
#include "test_helpers.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============================================================
 * 插件系统测试
 * ============================================================ */

static void test_plugin_system_lifecycle(void) {
    /* 创建/销毁 */
    lvPluginSystem *sys = lv_plugin_system_create(NULL);
    TEST_ASSERT_NOT_NULL(sys);
    TEST_ASSERT_EQ(lv_plugin_system_init(sys), 0);
    lv_plugin_system_cleanup(sys);
    lv_plugin_system_destroy(sys);

    /* NULL 安全 */
    lv_plugin_system_destroy(NULL);
    lv_plugin_system_init(NULL);
    lv_plugin_system_cleanup(NULL);
}

static void test_plugin_config(void) {
    lvPluginConfig *cfg = lv_plugin_config_create();
    TEST_ASSERT_NOT_NULL(cfg);

    /* 设置/获取 */
    TEST_ASSERT_EQ(lv_plugin_config_set(cfg, "key1", "value1", 0), 0);
    TEST_ASSERT_EQ(lv_plugin_config_set(cfg, "key2", "42", 1), 0);

    const char *v = lv_plugin_config_get(cfg, "key1", "default");
    TEST_ASSERT_STR_EQ(v, "value1");

    v = lv_plugin_config_get(cfg, "nonexistent", "default_val");
    TEST_ASSERT_STR_EQ(v, "default_val");

    /* NULL 安全 */
    lv_plugin_config_destroy(NULL);
    TEST_ASSERT_NULL(lv_plugin_config_get(NULL, "key", "def"));

    lv_plugin_config_destroy(cfg);
}

static void test_plugin_queries(void) {
    /* NULL 传入 */
    size_t cnt = 0;
    TEST_ASSERT_NULL(lv_plugin_find(NULL, "test"));
    TEST_ASSERT_NULL(lv_plugin_get_all(NULL, &cnt));
    TEST_ASSERT_EQ(cnt, (size_t) 0);

    cnt = 0;
    TEST_ASSERT_NULL(lv_plugin_get_by_type(NULL, lv_PLUGIN_TYPE_NATIVE, &cnt));
    TEST_ASSERT_EQ(cnt, (size_t) 0);

    cnt = 0;
    TEST_ASSERT_NULL(lv_plugin_get_by_state(NULL, lv_PLUGIN_STATE_LOADED, &cnt));
    TEST_ASSERT_EQ(cnt, (size_t) 0);

    /* is_active / get_state */
    TEST_ASSERT(!lv_plugin_is_active(NULL), "lv plugin is active should fail for invalid input");
    TEST_ASSERT_EQ(lv_plugin_get_state(NULL), (lvPluginState) 0);
}

static void test_plugin_interfaces(void) {
    /* NULL 安全 */
    TEST_ASSERT_EQ(lv_plugin_register_interface(NULL, NULL), -1);
    TEST_ASSERT_EQ(lv_plugin_unregister_interface(NULL, "test"), -1);
    TEST_ASSERT_NULL(lv_plugin_query_interface(NULL, "test", 0));

    size_t cnt = 0;
    lvPluginInterface **ifaces = lv_plugin_query_interfaces(NULL, NULL, &cnt);
    TEST_ASSERT_NULL(ifaces);
    TEST_ASSERT_EQ(cnt, (size_t) 0);
}

static void test_plugin_events(void) {
    TEST_ASSERT_EQ(lv_plugin_send_event(NULL, 0, NULL, (size_t) 0), -1);
    TEST_ASSERT_EQ(lv_plugin_broadcast_event(NULL, 0, NULL, (size_t) 0), -1);
    lv_plugin_set_event_handler(NULL, NULL); /* 不应崩溃 */
}

static void test_plugin_dependencies(void) {
    TEST_ASSERT_EQ(lv_plugin_resolve_dependencies(NULL, NULL), -1);
    TEST_ASSERT_EQ(lv_plugin_check_dependencies(NULL), 0); /* 无依赖视为满足 */

    size_t cnt = 0;
    lvPlugin **deps = lv_plugin_get_dependents(NULL, NULL, &cnt);
    TEST_ASSERT_NULL(deps);
    TEST_ASSERT_EQ(cnt, (size_t) 0);
}

static void test_plugin_search_path(void) {
    lvPluginSystem *sys = lv_plugin_system_create(NULL);
    TEST_ASSERT_NOT_NULL(sys);

    TEST_ASSERT_EQ(lv_plugin_system_add_search_path(sys, "/some/path"), 0);
    TEST_ASSERT_EQ(lv_plugin_system_add_search_path(sys, "/another/path"), 0);

    size_t cnt = 0;
    char **paths = lv_plugin_system_get_search_paths(sys, &cnt);
    /* 可能返回 NULL 或有效值，取决于实现 */
    if (paths) {
        for (size_t i = 0; i < cnt; i++)
            lv_free((void **)&paths[i]);
        lv_free((void **)&paths);
    }

    TEST_ASSERT_EQ(lv_plugin_system_remove_search_path(sys, "/some/path"), 0);
    lv_plugin_system_destroy(sys);
}

static void test_plugin_version(void) {
    TEST_ASSERT_EQ(lv_plugin_check_version(">=1.0.0", "1.0.0"), 0);
    /* NULL 安全 */
    TEST_ASSERT_EQ(lv_plugin_check_api_compatibility(1, 1), 1);
}

static void test_plugin_error(void) {
    TEST_ASSERT_NULL(lv_plugin_get_last_error(NULL));
    TEST_ASSERT_NULL(lv_plugin_system_get_last_error(NULL));
    lv_plugin_clear_error(NULL);
    lv_plugin_system_clear_error(NULL);
}

static void test_plugin_info_json(void) {
    /* NULL 安全 */
    char *json = lv_plugin_get_info_json(NULL);
    TEST_ASSERT_NULL(json);

    json = lv_plugin_system_get_info_json(NULL);
    TEST_ASSERT_NULL(json);
}

/* ============================================================
 * TikZ 导出测试
 * ============================================================ */

static void test_tikz_export_basic(void) {
    /* 准备约束图 */
    lv_init();
    lvEngine *e = lv_engine_create();
    TEST_ASSERT_NOT_NULL(e);

    lv_add_point(e, 0, 1, 0, 1);
    lv_add_point(e, 1, 1, 0, 1);
    lv_add_point(e, 0, 1, 1, 1);
    lv_add_line_segment(e, 0, 1);
    lv_add_line_segment(e, 1, 2);

    /* 缓冲区导出 */
    char buf[4096];
    int n = lv_tikz_export((void *) e->main_graph, buf, sizeof(buf));
    TEST_ASSERT(n > 0, "tikz export to buffer");
    TEST_ASSERT(strstr(buf, "tikzpicture") != NULL, "has tikzpicture env");
    TEST_ASSERT(strstr(buf, "\\fill") != NULL || strstr(buf, "\\draw") != NULL, "has tikz commands");

    /* 空图 */
    lvEngine *empty = lv_engine_create();
    n = lv_tikz_export((void *) empty->main_graph, buf, sizeof(buf));
    TEST_ASSERT(n > 0, "empty graph export");
    lv_engine_destroy(empty);

    /* NULL 安全 */
    TEST_ASSERT_EQ(lv_tikz_export(NULL, NULL, 0), -1);
    TEST_ASSERT_EQ(lv_tikz_export(NULL, buf, sizeof(buf)), -1);
    TEST_ASSERT_EQ(lv_tikz_export((void *) e->main_graph, NULL, sizeof(buf)), -1);
    TEST_ASSERT_EQ(lv_tikz_export((void *) e->main_graph, buf, 0), -1);

    lv_engine_destroy(e);
    lv_cleanup();
}

static void test_tikz_export_file(void) {
    lv_init();
    lvEngine *e = lv_engine_create();
    TEST_ASSERT_NOT_NULL(e);

    lv_add_point(e, 0, 1, 0, 1);
    lv_add_point(e, 1, 1, 0, 1);

    int n = lv_tikz_export_file((void *) e->main_graph, "test_output.tex");
    TEST_ASSERT(n > 0, "tikz file export");

    /* NULL 安全 */
    TEST_ASSERT_EQ(lv_tikz_export_file(NULL, "test.tex"), -1);
    TEST_ASSERT_EQ(lv_tikz_export_file((void *) e->main_graph, NULL), -1);

    lv_engine_destroy(e);
    lv_cleanup();
}

/* ============================================================
 * 证明编译器测试
 * ============================================================ */

static void test_proof_object(void) {
    lvProofObject *obj = lv_proof_object_create();
    TEST_ASSERT_NOT_NULL(obj);
    TEST_ASSERT_EQ(lv_proof_object_get_step_count(obj), 0);
    TEST_ASSERT(!lv_proof_object_is_valid(obj), "lv proof object is valid should fail for invalid input");

    /* 添加步骤 */
    lvProofStepRecord *step = lv_proof_step_record_create();
    TEST_ASSERT_NOT_NULL(step);
    step->type = (ProofStepType) 0;
    step->depth = 0;
    int id = lv_proof_object_add_step(obj, step);
    TEST_ASSERT(id >= 0, "add step to proof object");
    TEST_ASSERT_EQ(lv_proof_object_get_step_count(obj), 1);
    /* 仅添加步骤未设置 is_proved → isValid 为 false */
    TEST_ASSERT(!lv_proof_object_is_valid(obj), "not valid without is_proved");

    /* 添加公理/假设 */
    TEST_ASSERT(lv_proof_object_add_axiom(obj, 100), "add axiom");
    TEST_ASSERT(lv_proof_object_add_assumption(obj, 200), "add assumption");

    /* 添加 NULL 步骤 */
    TEST_ASSERT_EQ(lv_proof_object_add_step(obj, NULL), -1);

    lv_proof_object_destroy(obj);

    /* NULL 安全 */
    lv_proof_object_destroy(NULL);
    TEST_ASSERT_EQ(lv_proof_object_get_step_count(NULL), 0);
    TEST_ASSERT(!lv_proof_object_is_valid(NULL), "lv proof object is valid should fail for invalid input");

    /* 验证 */
    obj = lv_proof_object_create();
    TEST_ASSERT(!lv_proof_object_verify(obj), "empty proof not verifiable");
    lv_proof_object_destroy(obj);
}

static void test_proof_step_record(void) {
    lvProofStepRecord *rec = lv_proof_step_record_create();
    TEST_ASSERT_NOT_NULL(rec);
    TEST_ASSERT_EQ(rec->step_id, 0);
    TEST_ASSERT_EQ(rec->premise_count, 0);

    lv_proof_step_record_destroy(rec);
    lv_proof_step_record_destroy(NULL);
}

static void test_proof_trace(void) {
    lvProofTrace *trace = lv_proof_trace_create();
    TEST_ASSERT_NOT_NULL(trace);
    TEST_ASSERT_EQ(trace->event_count, 0);

    /* 跟踪生命周期 */
    lv_proof_trace_start(trace, 1);
    lv_proof_trace_step(trace, 0, "start", 1);
    lv_proof_trace_backtrack(trace, 0, 1);
    lv_proof_trace_branch(trace, "branch_a", 1, 2);
    lv_proof_trace_lemma(trace, 42, "useful_lemma");
    lv_proof_trace_contradiction(trace, 0, 3);
    lv_proof_trace_complete(trace, true);

    TEST_ASSERT(trace->event_count >= 3, "trace has events");

    /* 添加事件 */
    lvTraceEvent *ev = lv_trace_event_create(TRACE_EVENT_STEP);
    TEST_ASSERT_NOT_NULL(ev);
    TEST_ASSERT_EQ(ev->type, TRACE_EVENT_STEP);
    TEST_ASSERT_EQ(lv_proof_trace_add_event(trace, ev), 0);

    lv_proof_trace_destroy(trace);
    lv_proof_trace_destroy(NULL);
}

static void test_proof_compiler(void) {
    lvCompilerConfig cfg = lv_compiler_config_default();
    TEST_ASSERT_EQ(cfg.format, OUTPUT_FORMAT_TEXT);
    TEST_ASSERT(cfg.include_metadata, "default include metadata");

    lvProofCompiler *compiler = lv_proof_compiler_create(&cfg);
    TEST_ASSERT_NOT_NULL(compiler);

    /* 修改配置 */
    cfg.format = OUTPUT_FORMAT_JSON;
    cfg.include_metadata = false;
    lv_proof_compiler_set_config(compiler, &cfg);

    /* 编译空证明 */
    lvProofObject *obj = lv_proof_object_create();
    char *result = lv_proof_compiler_compile(compiler, obj, NULL);
    TEST_ASSERT_NOT_NULL(result);
    lv_free((void **)&result);
    lv_proof_object_destroy(obj);

    /* 各格式输出 */
    obj = lv_proof_object_create();

    result = lv_proof_compiler_to_json(obj, NULL);
    TEST_ASSERT_NOT_NULL(result);
    lv_free((void **)&result);

    result = lv_proof_compiler_to_text(obj, "zh");
    TEST_ASSERT_NOT_NULL(result);
    lv_free((void **)&result);

    result = lv_proof_compiler_to_latex(obj, "zh");
    TEST_ASSERT_NOT_NULL(result);
    lv_free((void **)&result);

    result = lv_proof_compiler_to_tikz(obj);
    TEST_ASSERT_NOT_NULL(result);
    lv_free((void **)&result);

    result = lv_proof_compiler_to_graphviz(obj, NULL);
    TEST_ASSERT_NOT_NULL(result);
    lv_free((void **)&result);

    lv_proof_object_destroy(obj);

    /* NULL 安全 */
    lv_proof_compiler_destroy(NULL);
    result = lv_proof_compiler_to_json(NULL, NULL);
    TEST_ASSERT_NULL(result);

    lv_proof_compiler_destroy(compiler);
}

/* ============================================================
 * UI-Kernel 协议测试
 * ============================================================ */

static void test_trust_color(void) {
    /* 名称 */
    TEST_ASSERT_STR_EQ(lv_trust_color_name(lv_COLOR_GREEN), "Green");
    TEST_ASSERT_STR_EQ(lv_trust_color_name(lv_COLOR_RED), "Red");
    TEST_ASSERT_STR_EQ(lv_trust_color_name((lvTrustColor) 99), "Unknown");

    /* RGBA */
    TEST_ASSERT_EQ(lv_trust_color_rgba(lv_COLOR_GREEN), 0xFF3fb950);
    TEST_ASSERT_EQ(lv_trust_color_rgba(lv_COLOR_RED), 0xFFf85149);
    TEST_ASSERT_EQ(lv_trust_color_rgba((lvTrustColor) 99), 0xFF888888);

    /* SVG */
    TEST_ASSERT_STR_EQ(lv_trust_color_svg(lv_COLOR_GREEN), "#3fb950");
    TEST_ASSERT_STR_EQ(lv_trust_color_svg((lvTrustColor) 99), "#888888");

    /* TikZ */
    TEST_ASSERT_NOT_NULL(lv_trust_color_tikz(lv_COLOR_GREEN));

    /* 颜色映射 */
    lvTrustColor lv = trust_color_to_lv_protocol(TRUST_GREEN);
    TEST_ASSERT_EQ(lv, lv_COLOR_GREEN);

    TrustColor tc = lv_protocol_to_trust_color(lv_COLOR_BLUE);
    TEST_ASSERT_EQ(tc, TRUST_BLUE_UNEXPLORED);
}

/* ============================================================
 * ProofWidget 测试
 * ============================================================ */

static void test_proof_widget_lifecycle(void) {
    lvWidgetLayout *layout = proof_widget_init(4);
    TEST_ASSERT_NOT_NULL(layout);

    /* 注册 Widget */
    int id1 = proof_widget_register(layout, WIDGET_GOAL_DISPLAY, "Goal", 0);
    TEST_ASSERT(id1 >= 0, "id1 >= 0");
    int id2 = proof_widget_register(layout, WIDGET_HYPOTHESIS_PANEL, "Hypotheses", 1);
    TEST_ASSERT(id2 >= 0, "id2 >= 0");
    int id3 = proof_widget_register(layout, WIDGET_STEP_NAVIGATOR, "Steps", 2);
    TEST_ASSERT(id3 >= 0, "id3 >= 0");

    /* 更新 */
    TEST_ASSERT_EQ(proof_widget_update(layout, id1, true, true, "Active Goal", 0, NULL), 0);
    TEST_ASSERT_EQ(proof_widget_update(layout, id2, false, true, "Disabled Hypos", 1, "{\"data\":1}"), 0);

    /* 越界更新 */
    TEST_ASSERT_EQ(proof_widget_update(layout, 999, false, false, NULL, 0, NULL), -1);
    TEST_ASSERT_EQ(proof_widget_update(NULL, 0, false, false, NULL, 0, NULL), -1);

    /* NULL 注册 */
    TEST_ASSERT_EQ(proof_widget_register(NULL, WIDGET_GOAL_DISPLAY, "x", 0), -1);

    /* 布局管理 */
    proof_widget_set_layout_type(layout, LAYOUT_HORIZONTAL, 3, 1);
    proof_widget_set_persistence_key(layout, "test_key");
    int order[] = {2, 1, 0};
    proof_widget_set_order(layout, order, 3);

    /* 布局导出 */
    char *json = proof_widget_export_layout(layout);
    TEST_ASSERT_NOT_NULL(json);
    TEST_ASSERT(strstr(json, "widgets") != NULL, "strstr should succeed");
    lv_free((void **)&json);

    /* 持久化键设为 NULL */
    proof_widget_set_persistence_key(layout, NULL);

    proof_widget_destroy(layout);
    proof_widget_destroy(NULL);
}

static void test_proof_widget_goal(void) {
    lvGoalDisplay goal;
    memset(&goal, 0, sizeof(goal));

    /* NULL 安全 */
    TEST_ASSERT_EQ(proof_widget_get_goal(NULL, &goal), -1);
    TEST_ASSERT_EQ(proof_widget_get_goal((ProofNavigator *) 0x1, NULL), -1);
}

static void test_proof_widget_suggest(void) {
    char *suggestions[5];
    double confidences[5];

    int rc = proof_widget_suggest_tactic(NULL, suggestions, confidences, 5);
    TEST_ASSERT_EQ(rc, -1);

    rc = proof_widget_suggest_tactic((ProofNavigator *) 0x1, suggestions, confidences, 0);
    TEST_ASSERT_EQ(rc, -1);

    /* 清理 */
    for (int i = 0; i < 5; i++)
        suggestions[i] = NULL;
}

static void test_proof_widget_search_tree(void) {
    char *tree = proof_widget_get_search_tree(NULL);
    TEST_ASSERT_NULL(tree);

    tree = proof_widget_get_search_tree((ProofNavigator *) 0x1);
    TEST_ASSERT_NOT_NULL(tree);
    lv_free((void **)&tree);
}

static void test_proof_widget_dependency(void) {
    char *dep = proof_widget_get_dependency_graph(NULL);
    TEST_ASSERT_NULL(dep);

    dep = proof_widget_get_dependency_graph((ProofNavigator *) 0x1);
    TEST_ASSERT_NOT_NULL(dep);
    lv_free((void **)&dep);
}

/* ============================================================
 * Main
 * ============================================================ */

TEST_MAIN_BEGIN("Layer5 Output")

    /* ── 插件系统 ── */
    printf("\n--- Plugin System ---\n");
    TEST_MAIN_RUN(test_plugin_system_lifecycle);
    TEST_MAIN_RUN(test_plugin_config);
    TEST_MAIN_RUN(test_plugin_queries);
    TEST_MAIN_RUN(test_plugin_interfaces);
    TEST_MAIN_RUN(test_plugin_events);
    TEST_MAIN_RUN(test_plugin_dependencies);
    TEST_MAIN_RUN(test_plugin_search_path);
    TEST_MAIN_RUN(test_plugin_version);
    TEST_MAIN_RUN(test_plugin_error);
    TEST_MAIN_RUN(test_plugin_info_json);

    /* ── TikZ 导出 ── */
    printf("\n--- TikZ Export ---\n");
    TEST_MAIN_RUN(test_tikz_export_basic);
    TEST_MAIN_RUN(test_tikz_export_file);

    /* ── 证明编译器 ── */
    printf("\n--- Proof Compiler ---\n");
    TEST_MAIN_RUN(test_proof_object);
    TEST_MAIN_RUN(test_proof_step_record);
    TEST_MAIN_RUN(test_proof_trace);
    TEST_MAIN_RUN(test_proof_compiler);

    /* ── UI-Kernel 协议 ── */
    printf("\n--- UI-Kernel Protocol ---\n");
    TEST_MAIN_RUN(test_trust_color);

    /* ── Proof Widget ── */
    printf("\n--- Proof Widget ---\n");
    TEST_MAIN_RUN(test_proof_widget_lifecycle);
    TEST_MAIN_RUN(test_proof_widget_goal);
    TEST_MAIN_RUN(test_proof_widget_suggest);
    TEST_MAIN_RUN(test_proof_widget_search_tree);
    TEST_MAIN_RUN(test_proof_widget_dependency);

TEST_MAIN_END()

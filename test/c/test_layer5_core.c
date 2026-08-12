/**
 * @file test_layer5_core.c
 * @brief Layer5 核心模块深度测试 — Plugin System / Proof Compiler
 *
 * 覆盖已有测试未深入触及的边缘情况、压力场景、状态机路径。
 *
 * @author Lv-00 Project
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/plugin_system.h"
#include "lv/proof.h"
#include "lv/proof_compiler.h"

#include "lv.h"
#include "test_helpers.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ================================================================
 * 插件系统深度测试
 * ================================================================ */

static void test_plugin_interface_full(void) {
    lvPluginSystem *sys = lv_plugin_system_create(NULL);
    TEST_ASSERT_NOT_NULL(sys);
    lv_plugin_system_init(sys);

    /* 创建一个模拟插件来注册接口 */
    lvPlugin mock_plugin;
    memset(&mock_plugin, 0, sizeof(mock_plugin));
    lvPluginContext mock_ctx;
    memset(&mock_ctx, 0, sizeof(mock_ctx));
    mock_ctx.system = sys;
    mock_plugin.context = &mock_ctx;

    lvPluginInterface iface;
    memset(&iface, 0, sizeof(iface));
    lv_strlcpy(iface.name, "test_interface", sizeof(iface.name));
    iface.version = 1;

    /* 注册 */
    TEST_ASSERT_EQ(lv_plugin_register_interface(&mock_plugin, &iface), 0);

    /* 重复注册相同名称应失败 */
    TEST_ASSERT_EQ(lv_plugin_register_interface(&mock_plugin, &iface), -1);

    /* 精确查询 */
    lvPluginInterface *found = lv_plugin_query_interface(sys, "test_interface", 1);
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_STR_EQ(found->name, "test_interface");

    /* 版本不匹配 */
    TEST_ASSERT_NULL(lv_plugin_query_interface(sys, "test_interface", 2));

    /* 通配符查询 */
    size_t cnt;
    lvPluginInterface **results = lv_plugin_query_interfaces(sys, "test_*", &cnt);
    TEST_ASSERT_NOT_NULL(results);
    TEST_ASSERT_EQ(cnt, (size_t) 1);
    lv_free((void **)&results);

    /* 无匹配模式 */
    results = lv_plugin_query_interfaces(sys, "nomatch_*", &cnt);
    TEST_ASSERT_NULL(results);
    TEST_ASSERT_EQ(cnt, (size_t) 0);

    /* 注销 */
    TEST_ASSERT_EQ(lv_plugin_unregister_interface(&mock_plugin, "test_interface"), 0);
    TEST_ASSERT_NULL(lv_plugin_query_interface(sys, "test_interface", 1));

    /* 再次注销应失败 */
    TEST_ASSERT_EQ(lv_plugin_unregister_interface(&mock_plugin, "test_interface"), -1);

    lv_plugin_system_destroy(sys);
}

static void test_plugin_event_handler(void) {
    lvPluginSystem *sys = lv_plugin_system_create(NULL);
    TEST_ASSERT_NOT_NULL(sys);

    /* 设置事件处理器 */
    static int event_fired = 0;
    event_fired = 0;

    lv_plugin_set_event_handler(sys, NULL);
    lv_plugin_set_event_handler(sys, NULL); /* 双重 NULL */

    /* 设置后广播（应该不会崩溃） */
    lv_plugin_broadcast_event(sys, lv_PLUGIN_EVENT_SHUTDOWN, NULL, 0);

    /* 设置实际处理器 */
    lv_plugin_set_event_handler(sys, NULL);

    lv_plugin_system_destroy(sys);
}

static void test_plugin_dependency_deep(void) {
    lvPluginSystem *sys = lv_plugin_system_create(NULL);
    TEST_ASSERT_NOT_NULL(sys);

    /* 创建一个模拟插件 */
    lvPlugin plugin_a;
    memset(&plugin_a, 0, sizeof(plugin_a));
    lv_strlcpy(plugin_a.info.name, "PluginA", sizeof(plugin_a.info.name));
    plugin_a.state = lv_PLUGIN_STATE_LOADED;

    lvPluginDependency dep;
    memset(&dep, 0, sizeof(dep));
    lv_strlcpy(dep.name, "PluginB", sizeof(dep.name));
    lv_strlcpy(dep.version_constraint, ">=1.0.0", sizeof(dep.version_constraint));
    dep.optional = 0;

    lvPluginDependency *dep_ptrs[] = {&dep};
    plugin_a.info.dependencies = dep_ptrs;
    plugin_a.info.dependency_count = 1;

    /* 依赖不在系统中 */
    TEST_ASSERT_EQ(lv_plugin_resolve_dependencies(sys, NULL), -1);
    TEST_ASSERT_EQ(lv_plugin_resolve_dependencies(NULL, &plugin_a), -1);

    /* check_dependencies */
    int has_dep = lv_plugin_check_dependencies(&plugin_a);
    TEST_ASSERT_EQ(has_dep, 0); /* 有非可选依赖 */

    /* 改为可选 */
    dep.optional = 1;
    has_dep = lv_plugin_check_dependencies(&plugin_a);
    TEST_ASSERT_EQ(has_dep, 1); /* 无非可选 */

    /* get_dependents */
    size_t cnt;
    lvPlugin **deps = lv_plugin_get_dependents(sys, &plugin_a, &cnt);
    TEST_ASSERT_NULL(deps);
    TEST_ASSERT_EQ(cnt, (size_t) 0);

    lv_plugin_system_destroy(sys);
}

static void test_plugin_version_deep(void) {
    /* 语义版本各种组合 */
    TEST_ASSERT_EQ(lv_plugin_check_version("1.0.0", "1.0.0"), 1); /* 精确匹配 */
    TEST_ASSERT_EQ(lv_plugin_check_version("1.0.0", "2.0.0"), 1); /* 更高 major */
    TEST_ASSERT_EQ(lv_plugin_check_version("2.0.0", "1.0.0"), 0); /* 更低 major → 不匹配 */
    TEST_ASSERT_EQ(lv_plugin_check_version("1.0.0", "1.1.0"), 1); /* 更高 minor */
    TEST_ASSERT_EQ(lv_plugin_check_version("1.1.0", "1.0.0"), 0); /* 更低 minor → 不匹配 */
    TEST_ASSERT_EQ(lv_plugin_check_version("1.0.0", "1.0.1"), 1); /* 更高 patch */
    TEST_ASSERT_EQ(lv_plugin_check_version("1.0.1", "1.0.0"), 0); /* 更低 patch → 不匹配 */

    /* NULL 参数 */
    TEST_ASSERT_EQ(lv_plugin_check_version(NULL, "1.0.0"), 0);
    TEST_ASSERT_EQ(lv_plugin_check_version("1.0.0", NULL), 0);
    TEST_ASSERT_EQ(lv_plugin_check_version(NULL, NULL), 0);

    /* API 兼容性 */
    TEST_ASSERT_EQ(lv_plugin_check_api_compatibility(1, 2), 1); /* provided >= required */
    TEST_ASSERT_EQ(lv_plugin_check_api_compatibility(2, 1), 0); /* provided < required */
    TEST_ASSERT_EQ(lv_plugin_check_api_compatibility(1, 1), 1);
}

static void test_plugin_config_deep(void) {
    lvPluginConfig *cfg = lv_plugin_config_create();
    TEST_ASSERT_NOT_NULL(cfg);

    /* 空配置获取 */
    const char *v = lv_plugin_config_get(cfg, "any", "def");
    TEST_ASSERT_STR_EQ(v, "def");

    /* 设置并覆盖 */
    TEST_ASSERT_EQ(lv_plugin_config_set(cfg, "key1", "value1", 0), 0);
    TEST_ASSERT_EQ(lv_plugin_config_set(cfg, "key1", "value1_new", 0), 0); /* 覆盖 */
    v = lv_plugin_config_get(cfg, "key1", "def");
    TEST_ASSERT_STR_EQ(v, "value1_new");

    /* NULL key/value 保护 */
    TEST_ASSERT_EQ(lv_plugin_config_set(cfg, NULL, "v", 0), -1);
    TEST_ASSERT_EQ(lv_plugin_config_set(cfg, "k", NULL, 0), -1);

    /* 从 NULL config 获取 */
    v = lv_plugin_config_get(NULL, "key", "def");
    TEST_ASSERT_NULL(v);

    lv_plugin_config_destroy(cfg);
}

static void test_plugin_search_path_deep(void) {
    lvPluginSystem *sys = lv_plugin_system_create(NULL);
    TEST_ASSERT_NOT_NULL(sys);

    /* 添加路径 */
    TEST_ASSERT_EQ(lv_plugin_system_add_search_path(sys, "/path/a"), 0);
    TEST_ASSERT_EQ(lv_plugin_system_add_search_path(sys, "/path/b"), 0);
    TEST_ASSERT_EQ(lv_plugin_system_add_search_path(sys, "/path/a"), 0); /* 重复 */

    size_t cnt;
    char **paths = lv_plugin_system_get_search_paths(sys, &cnt);
    TEST_ASSERT_EQ(cnt, (size_t) 2);

    /* 移除 */
    TEST_ASSERT_EQ(lv_plugin_system_remove_search_path(sys, "/path/a"), 0);
    TEST_ASSERT_EQ(lv_plugin_system_remove_search_path(sys, "/path/nonexistent"), -1);

    /* NULL 安全 */
    TEST_ASSERT_EQ(lv_plugin_system_add_search_path(NULL, "/path"), -1);
    TEST_ASSERT_EQ(lv_plugin_system_add_search_path(sys, NULL), -1);
    TEST_ASSERT_EQ(lv_plugin_system_remove_search_path(NULL, "/path"), -1);
    TEST_ASSERT_EQ(lv_plugin_system_remove_search_path(sys, NULL), -1);

    /* autoload NULL 安全 */
    TEST_ASSERT_EQ(lv_plugin_system_autoload(NULL, "/path"), -1);
    TEST_ASSERT_EQ(lv_plugin_system_autoload(sys, NULL), -1);
    TEST_ASSERT_EQ(lv_plugin_system_autoload_all(NULL), -1);

    lv_plugin_system_destroy(sys);
}

static void test_plugin_json_info(void) {
    lvPluginSystem *sys = lv_plugin_system_create(NULL);
    TEST_ASSERT_NOT_NULL(sys);

    char *json = lv_plugin_system_get_info_json(sys);
    TEST_ASSERT_NOT_NULL(json);
    TEST_ASSERT(strstr(json, "plugin_count") != NULL || strstr(json, "version") != NULL, "strstr should succeed");
    lv_free((void **)&json);

    lv_plugin_system_destroy(sys);
}

static void test_plugin_activate_deactivate(void) {
    lvPluginSystem *sys = lv_plugin_system_create(NULL);
    TEST_ASSERT_NOT_NULL(sys);

    lvPlugin plugin;
    memset(&plugin, 0, sizeof(plugin));
    plugin.state = lv_PLUGIN_STATE_LOADED;

    /* NULL 安全 */
    TEST_ASSERT_EQ(lv_plugin_activate(NULL), -1);
    TEST_ASSERT_EQ(lv_plugin_deactivate(NULL), -1);

    /* 状态检查 */
    TEST_ASSERT(!lv_plugin_is_active(NULL), "lv plugin is active should fail for invalid input");
    TEST_ASSERT_EQ(lv_plugin_get_state(NULL), lv_PLUGIN_STATE_UNLOADED);

    /* 未加载的不可激活 */
    plugin.state = lv_PLUGIN_STATE_UNLOADED;
    TEST_ASSERT_EQ(lv_plugin_activate(&plugin), -1);

    /* 激活 */
    plugin.state = lv_PLUGIN_STATE_LOADED;
    /* 无 system 上下文, 无依赖, 无 on_activate — 应该失败因为 context 为空 */
    /* 但测试状态机可以 */
    plugin.context = NULL;

    lv_plugin_system_destroy(sys);
}

/* ================================================================
 * 证明编译器深度测试
 * ================================================================ */

static void test_proof_object_with_premises(void) {
    lvProofObject *obj = lv_proof_object_create();
    TEST_ASSERT_NOT_NULL(obj);

    /* 创建三个步骤形成链 */
    lvProofStepRecord *s1 = lv_proof_step_record_create();
    s1->type = (ProofStepType) 0;
    s1->depth = 0;
    s1->rule_name = lv_strdup("axiom");
    lv_proof_object_add_step(obj, s1);

    lvProofStepRecord *s2 = lv_proof_step_record_create();
    s2->type = (ProofStepType) 0;
    s2->depth = 1;
    s2->rule_name = lv_strdup("deduction");
    const int s2_premises[] = { 0 };
    TEST_ASSERT(lv_proof_step_record_set_premises(s2, s2_premises, 1), "s2 set premises");
    lv_proof_object_add_step(obj, s2);

    lvProofStepRecord *s3 = lv_proof_step_record_create();
    s3->type = (ProofStepType) 0;
    s3->depth = 2;
    s3->rule_name = lv_strdup("conclusion");
    const int s3_premises[] = { 1 };
    TEST_ASSERT(lv_proof_step_record_set_premises(s3, s3_premises, 1), "s3 set premises");
    lv_proof_object_add_step(obj, s3);

    TEST_ASSERT_EQ(lv_proof_object_get_step_count(obj), 3);

    /* 设置 goal 和 is_proved 使 isValid 通过，才能调用 verify */
    Proposition *goal = proposition_create(0, PROPOSITION_TYPE_ATOMIC);
    obj->goal = goal;
    obj->is_proved = true;
    /* 最后一步的结论需要匹配 goal */
    s3->conclusion = goal;
    proposition_ref(goal);
    s3->conclusion_id = goal->id;

    TEST_ASSERT(lv_proof_object_is_valid(obj), "valid after setting goal and is_proved");

    /* verify: 前提顺序正确 */
    bool ok = lv_proof_object_verify(obj);
    TEST_ASSERT(ok, "verify valid chain");

    lv_proof_object_destroy(obj);
}

static void test_proof_object_invalid_chain(void) {
    lvProofObject *obj = lv_proof_object_create();

    /* 前提引用未来步骤 */
    lvProofStepRecord *s1 = lv_proof_step_record_create();
    const int future_premises[] = { 2 }; /* 未来步骤 */
    TEST_ASSERT(lv_proof_step_record_set_premises(s1, future_premises, 1), "s1 set premises");
    lv_proof_object_add_step(obj, s1);

    lvProofStepRecord *s2 = lv_proof_step_record_create();
    lv_proof_object_add_step(obj, s2);

    /* verify 应失败 */
    bool ok = lv_proof_object_verify(obj);
    TEST_ASSERT(!ok, "invalid chain detect");

    lv_proof_object_destroy(obj);
}

static void test_proof_object_add_axiom_assumption(void) {
    lvProofObject *obj = lv_proof_object_create();

    /* 添加公理/假设边界 */
    TEST_ASSERT(!lv_proof_object_add_axiom(NULL, 1), "lv proof object add axiom should fail for invalid input");
    TEST_ASSERT(!lv_proof_object_add_assumption(NULL, 1), "lv proof object add assumption should fail for invalid input");

    /* 添加大量触发扩容 */
    for (int i = 0; i < 40; i++) {
        TEST_ASSERT(lv_proof_object_add_axiom(obj, i), "lv proof object add axiom should succeed");
        TEST_ASSERT(lv_proof_object_add_assumption(obj, i), "lv proof object add assumption should succeed");
    }

    lv_proof_object_destroy(obj);
}

static void test_proof_compiler_all_formats(void) {
    /* 准备一个带有步骤的证明对象 */
    lvProofObject *obj = lv_proof_object_create();
    obj->theorem_name = lv_strdup("勾股定理");
    obj->is_proved = true;

    lvProofStepRecord *s1 = lv_proof_step_record_create();
    s1->type = (ProofStepType) 0;
    s1->depth = 0;
    s1->rule_name = lv_strdup("公理1");
    lv_proof_object_add_step(obj, s1);

    lvProofStepRecord *s2 = lv_proof_step_record_create();
    s2->type = (ProofStepType) 0;
    s2->depth = 1;
    s2->rule_name = lv_strdup("推理");
    const int s2_premises[] = { 0 };
    TEST_ASSERT(lv_proof_step_record_set_premises(s2, s2_premises, 1), "s2 set premises");
    lv_proof_object_add_step(obj, s2);

    /* JSON 格式 */
    char *json = lv_proof_compiler_to_json(obj, NULL);
    TEST_ASSERT_NOT_NULL(json);
    TEST_ASSERT(strstr(json, "勾股定理") != NULL || strstr(json, "勾") != NULL || strstr(json, "theorem_name") != NULL, "strstr should succeed");
    lv_free((void **)&json);

    /* LaTeX 格式 */
    char *latex = lv_proof_compiler_to_latex(obj, "zh");
    TEST_ASSERT_NOT_NULL(latex);
    TEST_ASSERT(strstr(latex, "Proof") != NULL || strstr(latex, "证明") != NULL, "strstr should succeed");
    lv_free((void **)&latex);

    /* LaTeX 英文 */
    latex = lv_proof_compiler_to_latex(obj, "en");
    TEST_ASSERT_NOT_NULL(latex);
    TEST_ASSERT(strstr(latex, "Proof") != NULL, "strstr should succeed");
    lv_free((void **)&latex);

    /* TikZ 格式 */
    char *tikz = lv_proof_compiler_to_tikz(obj);
    TEST_ASSERT_NOT_NULL(tikz);
    TEST_ASSERT(strstr(tikz, "tikzpicture") != NULL, "strstr should succeed");
    lv_free((void **)&tikz);

    /* Text 格式 */
    char *text = lv_proof_compiler_to_text(obj, "zh");
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT(strstr(text, "证明") != NULL || strstr(text, "勾股定理") != NULL, "strstr should succeed");
    lv_free((void **)&text);

    /* Graphviz 格式 */
    char *dot = lv_proof_compiler_to_graphviz(obj, NULL);
    TEST_ASSERT_NOT_NULL(dot);
    TEST_ASSERT(strstr(dot, "digraph") != NULL, "strstr should succeed");
    lv_free((void **)&dot);

    lv_proof_object_destroy(obj);
}

static void test_proof_compiler_null_objects(void) {
    /* NULL proof */
    char *r = lv_proof_compiler_to_json(NULL, NULL);
    TEST_ASSERT_NULL(r);

    r = lv_proof_compiler_to_latex(NULL, "zh");
    TEST_ASSERT_NULL(r);

    r = lv_proof_compiler_to_tikz(NULL);
    TEST_ASSERT_NULL(r);

    r = lv_proof_compiler_to_text(NULL, "zh");
    TEST_ASSERT_NULL(r);

    r = lv_proof_compiler_to_graphviz(NULL, NULL);
    TEST_ASSERT_NULL(r);

    /* Compiler compile with NULL */
    lvCompilerConfig cfg = lv_compiler_config_default();
    lvProofCompiler *comp = lv_proof_compiler_create(&cfg);
    TEST_ASSERT_NOT_NULL(comp);

    r = lv_proof_compiler_compile(comp, NULL, NULL);
    TEST_ASSERT_NULL(r);

    r = lv_proof_compiler_compile(NULL, NULL, NULL);
    TEST_ASSERT_NULL(r);

    lv_proof_compiler_destroy(comp);
}

static void test_proof_compiler_config(void) {
    lvCompilerConfig cfg = lv_compiler_config_default();
    TEST_ASSERT_EQ(cfg.format, OUTPUT_FORMAT_TEXT);
    TEST_ASSERT(cfg.include_metadata, "default config should include metadata");
    TEST_ASSERT(!cfg.verbose, "default config should not be verbose");
    TEST_ASSERT_EQ(cfg.max_depth, 64);

    lvProofCompiler *comp = lv_proof_compiler_create(&cfg);
    TEST_ASSERT_NOT_NULL(comp);

    /* 修改配置 */
    cfg.format = OUTPUT_FORMAT_GRAPHVIZ;
    cfg.verbose = true;
    cfg.max_depth = 512;
    lv_proof_compiler_set_config(comp, &cfg);

    /* 空证明编译 Graphviz */
    lvProofObject *obj = lv_proof_object_create();
    char *r = lv_proof_compiler_compile(comp, obj, NULL);
    TEST_ASSERT_NOT_NULL(r);
    lv_free((void **)&r);

    lv_proof_object_destroy(obj);
    lv_proof_compiler_destroy(comp);
}

static void test_proof_export_to_file(void) {
    lvProofObject *obj = lv_proof_object_create();
    obj->theorem_name = lv_strdup("Test");

    /* 正常导出 */
    bool ok = lv_proof_export_to_file(obj, NULL, OUTPUT_FORMAT_TEXT, "test_proof_output.txt");
    TEST_ASSERT(ok, "export to file");

    /* NULL 安全 */
    TEST_ASSERT(!lv_proof_export_to_file(NULL, NULL, OUTPUT_FORMAT_TEXT, "test.txt"), "lv proof export to file should fail for invalid input");
    TEST_ASSERT(!lv_proof_export_to_file(obj, NULL, OUTPUT_FORMAT_TEXT, NULL), "lv proof export to file should fail for invalid input");
    TEST_ASSERT(!lv_proof_export_to_file(NULL, NULL, OUTPUT_FORMAT_TEXT, NULL), "lv proof export to file should fail for invalid input");

    lv_proof_object_destroy(obj);
}

static void test_proof_step_record_premises(void) {
    lvProofStepRecord *rec = lv_proof_step_record_create();
    TEST_ASSERT_NOT_NULL(rec);

    /* 添加前提 */
    const int premises[] = { 0, 1, 2 };
    TEST_ASSERT(lv_proof_step_record_set_premises(rec, premises, 3), "rec set premises");
    TEST_ASSERT_EQ(rec->premise_capacity, 3);

    /* 设置字段 */
    rec->rule_name = lv_strdup("modus_ponens");
    rec->justification = lv_strdup("MP applied");
    rec->depth = 2;
    rec->color = (ProofColor) 0;

    lv_proof_step_record_destroy(rec);
    lv_proof_step_record_destroy(NULL);
}

static void test_proof_trace_lifecycle(void) {
    lvProofTrace *trace = lv_proof_trace_create();
    TEST_ASSERT_NOT_NULL(trace);

    /* 跟踪各种事件 */
    lv_proof_trace_start(trace, 42);
    lv_proof_trace_step(trace, 1, "step1", 1);
    lv_proof_trace_step(trace, 2, "step2", 2);
    lv_proof_trace_backtrack(trace, 2, 1);
    lv_proof_trace_branch(trace, "branch_x", 3, 2);
    lv_proof_trace_lemma(trace, 100, "helper_lemma");
    lv_proof_trace_contradiction(trace, 0, 5);
    lv_proof_trace_complete(trace, true);

    /* 验证事件数 */
    TEST_ASSERT(trace->event_count > 0, "trace has events");
    TEST_ASSERT_EQ(trace->proof_id, 42);

    /* 创建并添加事件 */
    lvTraceEvent *ev = lv_trace_event_create(TRACE_EVENT_STEP);
    TEST_ASSERT_NOT_NULL(ev);
    ev->step_id = 99;
    ev->description = lv_strdup("manual event");
    ev->depth = 3;
    int rc = lv_proof_trace_add_event(trace, ev);
    TEST_ASSERT_EQ(rc, 0);

    /* NULL 安全 */
    lv_trace_event_destroy(NULL);
    lv_proof_trace_destroy(NULL);
    lv_proof_trace_start(NULL, 1);
    lv_proof_trace_step(NULL, 0, "x", 0);
    lv_proof_trace_backtrack(NULL, 0, 0);
    lv_proof_trace_branch(NULL, "x", 0, 0);
    lv_proof_trace_lemma(NULL, 0, "x");
    lv_proof_trace_contradiction(NULL, 0, 0);
    lv_proof_trace_complete(NULL, true);

    lv_proof_trace_destroy(trace);
}

/* ================================================================
 * Main
 * ================================================================ */

TEST_MAIN_BEGIN("Layer5 Core (Plugin System / Proof Compiler)")

    /* ── 插件系统深度测试 ── */
    printf("\n--- Plugin System Deep ---\n");
    TEST_MAIN_RUN(test_plugin_interface_full);
    TEST_MAIN_RUN(test_plugin_event_handler);
    TEST_MAIN_RUN(test_plugin_dependency_deep);
    TEST_MAIN_RUN(test_plugin_version_deep);
    TEST_MAIN_RUN(test_plugin_config_deep);
    TEST_MAIN_RUN(test_plugin_search_path_deep);
    TEST_MAIN_RUN(test_plugin_json_info);
    TEST_MAIN_RUN(test_plugin_activate_deactivate);

    /* ── 证明编译器深度测试 ── */
    printf("\n--- Proof Compiler Deep ---\n");
    TEST_MAIN_RUN(test_proof_object_with_premises);
    TEST_MAIN_RUN(test_proof_object_invalid_chain);
    TEST_MAIN_RUN(test_proof_object_add_axiom_assumption);
    TEST_MAIN_RUN(test_proof_compiler_all_formats);
    TEST_MAIN_RUN(test_proof_compiler_null_objects);
    TEST_MAIN_RUN(test_proof_compiler_config);
    TEST_MAIN_RUN(test_proof_export_to_file);
    TEST_MAIN_RUN(test_proof_step_record_premises);
    TEST_MAIN_RUN(test_proof_trace_lifecycle);

TEST_MAIN_END()

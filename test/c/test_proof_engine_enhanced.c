/**
 * @file test_proof_engine_enhanced.c
 * @brief 增强证明引擎（proof_engine_enhanced.h）零覆盖 API 契约测试
 *
 * 批次 C-㊱：补全 proof_engine_enhanced.h 中 31 个此前零测试覆盖的 API。
 * 覆盖域（按 test-authoring 三层：等价/边界/性质）：
 * - 溯源树：lv_trace_tree_create / destroy / lv_trace_node_create / destroy /
 *   add_child / set_status / compute_color / find_path / export_dot / to_json
 * - 反证法：lv_contradiction_path_create / destroy / add_node /
 *   lv_detect_contradiction / lv_contradiction_path_validate /
 *   lv_engine_proof_by_contradiction（NULL 契约）
 * - 引擎生命周期：lv_proof_engine_create / destroy / set_rule_library /
 *   register_strategy / get_stats
 * - 证明验证：lv_verify_proof / lv_verify_proof_step
 * - 证明优化：lv_optimize_proof / lv_compute_proof_complexity / lv_simplify_proof
 * - 证明导出：lv_proof_to_natural_language / latex / coq / isar
 * - 证明执行：lv_proof_engine_prove / auto_prove / prove_with_strategy（NULL 契约）
 *
 * @author Lv-00 Project
 * @date 2026-08-20
 */

#include <stdio.h>
#include <string.h>

#include "lv/proof_engine_enhanced.h"
#include "lv/proof.h"
#include "lv.h"
#include "test_helpers.h"

/* ============================================================
 * 全局测试计数器
 * ============================================================ */
int g_pass_count = 0;
int g_fail_count = 0;

/* 测试导出文件 */
#define TEST_DOT_FILE "test_proof_engine_tmp.dot"

static void remove_dot_file(void) {
    remove(TEST_DOT_FILE);
}

/* ============================================================
 * 溯源树：节点创建/树结构/颜色/路径/导出
 * ============================================================ */
static void test_trace_tree(void) {
    /* 树创建：NULL 根命题 → 空目标节点 */
    lvProofTraceTree *tree = lv_trace_tree_create(NULL);
    TEST_ASSERT_MSG(tree != NULL, "树创建");
    TEST_ASSERT_MSG(tree->root != NULL, "根节点非空");
    TEST_ASSERT_MSG(tree->root->type == TRACE_NODE_GOAL, "根类型 GOAL");
    TEST_ASSERT_EQ(tree->all_nodes.count, 1);
    TEST_ASSERT_MSG(!tree->is_complete, "初始未完成");

    /* 节点创建 */
    lvProofTraceNode *axiom = lv_trace_node_create(TRACE_NODE_AXIOM, "公理A");
    lvProofTraceNode *deriv = lv_trace_node_create(TRACE_NODE_DERIVATION, "推导1");
    lvProofTraceNode *deriv2 = lv_trace_node_create(TRACE_NODE_DERIVATION, "推导2");
    TEST_ASSERT_MSG(axiom && deriv && deriv2, "节点创建");
    TEST_ASSERT_MSG(axiom->id > 0, "节点 ID 正数");
    TEST_ASSERT_MSG(axiom->id != deriv->id, "节点 ID 唯一");
    TEST_ASSERT_MSG(axiom->status == TRACE_STATUS_UNEXPLORED, "初始未探索");
    TEST_ASSERT_MSG(strcmp(axiom->label, "公理A") == 0, "标签复制");

    /* 添加子节点：根 → deriv → axiom, deriv2 */
    TEST_ASSERT_MSG(lv_trace_node_add_child(tree->root, deriv), "根加子节点");
    TEST_ASSERT_MSG(lv_trace_node_add_child(deriv, axiom), "deriv 加子节点");
    TEST_ASSERT_MSG(lv_trace_node_add_child(deriv, deriv2), "deriv 加第二个子节点");
    TEST_ASSERT_MSG(deriv->parent == tree->root, "父指针");
    TEST_ASSERT_MSG(deriv->depth == 1, "深度 1");
    TEST_ASSERT_MSG(axiom->depth == 2, "深度 2");
    TEST_ASSERT_EQ(deriv->children.count, 2);

    /* NULL 契约 */
    TEST_ASSERT_MSG(!lv_trace_node_add_child(NULL, axiom), "NULL parent 失败");
    TEST_ASSERT_MSG(!lv_trace_node_add_child(deriv, NULL), "NULL child 失败");
    TEST_ASSERT_MSG(!lv_trace_node_add_child(NULL, NULL), "NULL both 失败");

    /* 设置状态：终态记录完成时间 */
    lv_trace_node_set_status(axiom, TRACE_STATUS_PROVED);
    TEST_ASSERT_MSG(axiom->status == TRACE_STATUS_PROVED, "状态更新");
    TEST_ASSERT_MSG(axiom->complete_time_ns > 0, "终态记录完成时间");
    lv_trace_node_set_status(NULL, TRACE_STATUS_PROVED); /* NULL 安全 */

    /* 颜色计算：公理 GREEN；推导取子节点最小颜色 */
    TrustColor axiom_color = lv_trace_node_compute_color(axiom);
    TEST_ASSERT_MSG(axiom_color == TRUST_GREEN, "公理绿色");
    TrustColor deriv_color = lv_trace_node_compute_color(deriv);
    TEST_ASSERT_MSG(deriv_color == TRUST_GREEN, "推导取子节点最小颜色（均绿）");

    /* 路径查找：根到 axiom */
    lvProofTraceNode *path[8];
    uint32_t plen = lv_trace_tree_find_path(tree, tree->root->id, axiom->id, path, 8);
    TEST_ASSERT_MSG(plen >= 2, "路径长度 >= 2");
    TEST_ASSERT_MSG(plen <= 8, "路径在容量内");
    TEST_ASSERT_MSG(path[plen - 1] == axiom, "路径终点为 axiom");

    /* DOT 导出 */
    remove_dot_file();
    TEST_ASSERT_MSG(lv_trace_tree_export_dot(tree, TEST_DOT_FILE), "DOT 导出");
    {
        FILE *f = fopen(TEST_DOT_FILE, "r");
        TEST_ASSERT_MSG(f != NULL, "DOT 文件存在");
        char buf[512];
        size_t n = fread(buf, 1, sizeof(buf) - 1, f);
        buf[n] = '\0';
        fclose(f);
        TEST_ASSERT_MSG(strstr(buf, "digraph") != NULL, "DOT 含 digraph");
    }
    remove_dot_file();
    TEST_ASSERT_MSG(!lv_trace_tree_export_dot(NULL, TEST_DOT_FILE), "NULL tree 导出失败");
    TEST_ASSERT_MSG(!lv_trace_tree_export_dot(tree, NULL), "NULL path 导出失败");

    /* JSON 导出 */
    char *json = lv_trace_tree_to_json(tree);
    TEST_ASSERT_MSG(json != NULL, "JSON 导出非空");
    TEST_ASSERT_MSG(strstr(json, "root") != NULL || strstr(json, "\"id\"") != NULL, "JSON 含结构");
    lv_free((void **) &json);
    TEST_ASSERT_NULL(lv_trace_tree_to_json(NULL));

    /* 树销毁（递归释放节点） */
    lv_trace_tree_destroy(tree);
    lv_trace_tree_destroy(NULL); /* NULL 安全 */

    /* 节点独立销毁（未入树的节点） */
    lvProofTraceNode *orphan = lv_trace_node_create(TRACE_NODE_LEMMA, "孤儿");
    lv_trace_node_destroy(orphan);
    lv_trace_node_destroy(NULL);
}

/* ============================================================
 * 反证法：矛盾路径 / 矛盾检测 / 验证
 * ============================================================ */
static void test_contradiction(void) {
    /* 路径创建/节点添加/销毁 */
    lvContradictionPath *path = lv_contradiction_path_create();
    TEST_ASSERT_MSG(path != NULL, "路径创建");
    TEST_ASSERT_EQ(path->nodes.count, 0);

    uint32_t id0 = lv_contradiction_path_add_node(path, "P", "假设", true);
    TEST_ASSERT_EQ(id0, (uint32_t) 0);
    uint32_t id1 = lv_contradiction_path_add_node(path, "¬P", "推导", false);
    TEST_ASSERT_EQ(id1, (uint32_t) 1);
    TEST_ASSERT_EQ(path->nodes.count, 2);

    /* 节点内容 */
    lvContradictionPathNode *n0 = (lvContradictionPathNode *)lv_darray_get(&path->nodes, 0);
    TEST_ASSERT_MSG(strcmp(n0->statement, "P") == 0, "陈述");
    TEST_ASSERT_MSG(n0->is_assumption == true, "假设标记");
    lvContradictionPathNode *n1 = (lvContradictionPathNode *)lv_darray_get(&path->nodes, 1);
    TEST_ASSERT_MSG(n1->is_assumption == false, "非假设标记");

    /* NULL 契约 */
    TEST_ASSERT_EQ(lv_contradiction_path_add_node(NULL, "x", "y", false), (uint32_t) -1);
    TEST_ASSERT_EQ(lv_contradiction_path_add_node(path, NULL, "y", false), (uint32_t) -1);
    lv_contradiction_path_destroy(NULL);

    lv_contradiction_path_destroy(path);

    /* 矛盾检测：NULL 契约 */
    lvContradictionType ctype;
    char cdesc[512];
    TEST_ASSERT_MSG(!lv_detect_contradiction(NULL, NULL, &ctype, cdesc), "NULL graph/nav 无矛盾");
    TEST_ASSERT_MSG(!lv_detect_contradiction(NULL, NULL, NULL, cdesc), "NULL out_type 失败");
    TEST_ASSERT_MSG(!lv_detect_contradiction(NULL, NULL, &ctype, NULL), "NULL out_desc 失败");

    /* 空 nav + 空图：无矛盾 */
    Proposition *prop = proposition_create(100, PROPOSITION_TYPE_ATOMIC);
    ProofNavigator *nav = proof_navigator_create(prop, NULL);
    TEST_ASSERT_MSG(nav != NULL, "nav 创建");
    ConstraintGraph *graph = graph_create();
    TEST_ASSERT_MSG(graph != NULL, "graph 创建");
    memset(cdesc, 0, sizeof(cdesc));
    TEST_ASSERT_MSG(!lv_detect_contradiction(graph, nav, &ctype, cdesc), "空结构无矛盾");

    /* 反证法证明：NULL 契约 */
    lvContradictionPath *out_path = NULL;
    lvProofEngineConfig cfg = {0};
    lvProofEngine *engine = lv_proof_engine_create(&cfg);
    TEST_ASSERT_MSG(engine != NULL, "引擎创建");
    TEST_ASSERT_MSG(!lv_engine_proof_by_contradiction(NULL, prop, 10, &out_path), "NULL engine 失败");
    TEST_ASSERT_MSG(!lv_engine_proof_by_contradiction(engine, NULL, 10, &out_path), "NULL goal 失败");
    /* 空图反证：可能不成功（无约束），但不崩溃 */
    bool br = lv_engine_proof_by_contradiction(engine, prop, 10, &out_path);
    TEST_ASSERT_MSG(br == false || out_path != NULL, "反证法执行不崩溃");

    graph_destroy(graph);
    lv_proof_engine_destroy(engine);
    proof_navigator_destroy(nav);
    proposition_unref(prop);
}

/* ============================================================
 * 引擎生命周期：create / destroy / set_rule_library / register / get_stats
 * ============================================================ */
static void test_engine_lifecycle(void) {
    /* NULL 配置 → 默认值 */
    lvProofEngine *engine = lv_proof_engine_create(NULL);
    TEST_ASSERT_MSG(engine != NULL, "默认配置创建");
    TEST_ASSERT_MSG(engine->config.max_depth == 50, "默认深度 50");
    TEST_ASSERT_MSG(engine->config.verify_proofs == true, "默认验证开启");
    TEST_ASSERT_EQ(engine->strategy_count, 0);

    /* 指定配置 */
    lvProofEngineConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.max_depth = 10;
    cfg.max_branches = 4;
    cfg.timeout_ms = 100;
    lvProofEngine *e2 = lv_proof_engine_create(&cfg);
    TEST_ASSERT_MSG(e2 != NULL, "指定配置创建");
    TEST_ASSERT_MSG(e2->config.max_depth == 10, "配置复制");

    /* set_rule_library：NULL 与任意指针（规则库不 deref） */
    lv_proof_engine_set_rule_library(e2, NULL);
    lv_proof_engine_set_rule_library(e2, (lvRuleLibrary *) 1);
    lv_proof_engine_set_rule_library(NULL, NULL); /* NULL 安全 */

    /* register_strategy */
    lvProofStrategy strategy;
    memset(&strategy, 0, sizeof(strategy));
    strategy.type = STRATEGY_DIRECT;
    snprintf(strategy.name, sizeof(strategy.name), "直接证明");
    strategy.priority = 1.0;
    TEST_ASSERT_MSG(lv_proof_engine_register_strategy(e2, &strategy), "注册策略");
    TEST_ASSERT_EQ(e2->strategy_count, 1);

    lvProofStrategy strategy2;
    memset(&strategy2, 0, sizeof(strategy2));
    strategy2.type = STRATEGY_CONTRADICTION;
    strategy2.priority = 5.0; /* 高优先级插入到前 */
    TEST_ASSERT_MSG(lv_proof_engine_register_strategy(e2, &strategy2), "注册策略2");
    TEST_ASSERT_EQ(e2->strategy_count, 2);
    TEST_ASSERT_MSG(e2->strategies[0].priority == 5.0, "按优先级排序");

    /* NULL 契约 */
    TEST_ASSERT_MSG(!lv_proof_engine_register_strategy(e2, NULL), "NULL strategy 失败");
    TEST_ASSERT_MSG(!lv_proof_engine_register_strategy(NULL, &strategy), "NULL engine 失败");

    /* get_stats */
    uint64_t total = 99, success = 99;
    double avg = -1.0;
    lv_proof_engine_get_stats(e2, &total, &success, &avg);
    TEST_ASSERT_EQ(total, (uint64_t) 0);
    TEST_ASSERT_EQ(success, (uint64_t) 0);
    TEST_ASSERT_DOUBLE(avg, 0.0, 1e-12);
    lv_proof_engine_get_stats(e2, NULL, NULL, NULL); /* NULL 输出安全 */
    lv_proof_engine_get_stats(NULL, &total, &success, &avg); /* NULL engine 安全 */

    /* prove 家族：NULL 契约 */
    Proposition *prop = proposition_create(101, PROPOSITION_TYPE_ATOMIC);
    ConstraintGraph *graph = graph_create();
    lvProofTraceTree *trace = NULL;
    lvStrategyType stype;
    TEST_ASSERT_MSG(!lv_proof_engine_prove(NULL, prop, graph, &trace), "NULL engine prove 失败");
    TEST_ASSERT_MSG(!lv_proof_engine_prove(e2, NULL, graph, &trace), "NULL goal prove 失败");
    TEST_ASSERT_MSG(!lv_proof_engine_auto_prove(NULL, prop, graph, &trace, &stype), "NULL auto_prove 失败");
    TEST_ASSERT_MSG(!lv_proof_engine_prove_with_strategy(NULL, prop, graph, STRATEGY_DIRECT, &trace),
                    "NULL prove_with_strategy 失败");

    graph_destroy(graph);
    lv_proof_engine_destroy(e2);
    lv_proof_engine_destroy(engine);
    lv_proof_engine_destroy(NULL);
    proposition_unref(prop);
}

/* ============================================================
 * 证明验证：lv_verify_proof / lv_verify_proof_step
 * ============================================================ */
static void test_verify(void) {
    char err[512];

    /* NULL 契约 */
    TEST_ASSERT_MSG(lv_verify_proof(NULL, err) == lv_VERIFY_ERROR, "NULL trace ERROR");
    TEST_ASSERT_MSG(lv_verify_proof(NULL, NULL) == lv_VERIFY_ERROR, "NULL trace + NULL err ERROR");

    /* 空树（无根）→ ERROR */
    lvProofTraceTree *tree = lv_trace_tree_create(NULL);
    TEST_ASSERT_MSG(tree != NULL, "树创建");
    tree->root = NULL; /* 模拟无根 */
    TEST_ASSERT_MSG(lv_verify_proof(tree, err) == lv_VERIFY_ERROR, "无根 ERROR");
    lv_trace_tree_destroy(tree);

    /* 正常树：根未证明 → INCOMPLETE */
    lvProofTraceTree *tree2 = lv_trace_tree_create(NULL);
    TEST_ASSERT_MSG(lv_verify_proof(tree2, err) == lv_VERIFY_INCOMPLETE, "根未证明 INCOMPLETE");

    /* 根标记证明 → VALID */
    lv_trace_node_set_status(tree2->root, TRACE_STATUS_PROVED);
    tree2->is_complete = true;
    tree2->final_color = lv_trace_node_compute_color(tree2->root);
    TEST_ASSERT_MSG(lv_verify_proof(tree2, err) == lv_VERIFY_VALID, "完整树 VALID");

    /* 无子节点的推导节点 → INVALID */
    lvProofTraceTree *tree3 = lv_trace_tree_create(NULL);
    lvProofTraceNode *deriv = lv_trace_node_create(TRACE_NODE_DERIVATION, "空推导");
    lv_trace_node_add_child(tree3->root, deriv);
    lv_trace_node_set_status(tree3->root, TRACE_STATUS_PROVED);
    TEST_ASSERT_MSG(lv_verify_proof(tree3, err) == lv_VERIFY_INVALID, "空推导节点 INVALID");

    lv_trace_tree_destroy(tree2);
    lv_trace_tree_destroy(tree3);

    /* 步骤验证 */
    TEST_ASSERT_MSG(lv_verify_proof_step(NULL, NULL, err) == lv_VERIFY_ERROR, "NULL step ERROR");
    ProofStep *step = proof_step_create(PROOF_STEP_ADD_NODE);
    step->id = 1;
    TEST_ASSERT_MSG(lv_verify_proof_step(step, NULL, err) == lv_VERIFY_INCOMPLETE, "未完成步骤 INCOMPLETE");
    step->is_completed = true;
    TEST_ASSERT_MSG(lv_verify_proof_step(step, NULL, err) == lv_VERIFY_VALID, "完成步骤 VALID");
    proof_step_destroy(step);
}

/* ============================================================
 * 证明优化：复杂度 / 简化 / 优化
 * ============================================================ */
static void test_optimize(void) {
    /* NULL 契约 */
    TEST_ASSERT_EQ(lv_compute_proof_complexity(NULL), (uint32_t) 0);
    TEST_ASSERT_EQ(lv_simplify_proof(NULL), (uint32_t) 0);
    lvProofTraceTree *out = NULL;
    TEST_ASSERT_MSG(!lv_optimize_proof(NULL, &out), "NULL trace 失败");
    TEST_ASSERT_MSG(!lv_optimize_proof(NULL, NULL), "NULL out 失败");

    /* 构造树：根(GOAL) → deriv → axiom */
    lvProofTraceTree *tree = lv_trace_tree_create(NULL);
    lvProofTraceNode *deriv = lv_trace_node_create(TRACE_NODE_DERIVATION, "推导");
    lvProofTraceNode *axiom = lv_trace_node_create(TRACE_NODE_AXIOM, "公理");
    lv_trace_node_add_child(tree->root, deriv);
    lv_trace_node_add_child(deriv, axiom);

    /* 复杂度：非零 */
    uint32_t cplx = lv_compute_proof_complexity(tree);
    TEST_ASSERT_MSG(cplx > 0, "复杂度非零");

    /* 简化：无冗余（deriv 有子节点）→ 步数不变 */
    uint32_t steps = lv_simplify_proof(tree);
    TEST_ASSERT_EQ(steps, (uint32_t) tree->all_nodes.count);

    /* 优化：复制树（无冗余被删） */
    lvProofTraceTree *optimized = NULL;
    TEST_ASSERT_MSG(lv_optimize_proof(tree, &optimized), "优化成功");
    TEST_ASSERT_MSG(optimized != NULL, "优化树非空");
    TEST_ASSERT_MSG(optimized->root != NULL, "优化树有根");
    TEST_ASSERT_MSG(optimized->all_nodes.count >= 1, "优化树节点数 >= 1");
    lv_trace_tree_destroy(optimized);

    /* 冗余节点：空推导（无子节点）→ 优化时被移除 */
    lvProofTraceTree *tree2 = lv_trace_tree_create(NULL);
    lvProofTraceNode *empty_deriv = lv_trace_node_create(TRACE_NODE_DERIVATION, "空推导");
    lv_trace_node_add_child(tree2->root, empty_deriv); /* 无子节点的推导 = 冗余 */
    lvProofTraceTree *opt2 = NULL;
    TEST_ASSERT_MSG(lv_optimize_proof(tree2, &opt2), "优化冗余树");
    /* 冗余节点被跳过：优化树根无子节点（空推导未复制） */
    TEST_ASSERT_MSG(opt2->root != NULL, "优化树有根");
    TEST_ASSERT_EQ(opt2->root->children.count, 0);

    lv_trace_tree_destroy(tree2);
    lv_trace_tree_destroy(opt2);
    lv_trace_tree_destroy(tree);
}

/* ============================================================
 * 证明导出：NL / latex / coq / isar
 * ============================================================ */
static void test_export_formats(void) {
    /* NULL 契约 */
    TEST_ASSERT_NULL(lv_proof_to_natural_language(NULL, PROOF_NL_LANG_ZH_CN));
    TEST_ASSERT_NULL(lv_proof_to_latex(NULL));
    TEST_ASSERT_NULL(lv_proof_to_coq(NULL));
    TEST_ASSERT_NULL(lv_proof_to_isar(NULL));

    /* 构造树并导出 */
    lvProofTraceTree *tree = lv_trace_tree_create(NULL);
    lvProofTraceNode *axiom = lv_trace_node_create(TRACE_NODE_AXIOM, "axiom_a");
    lv_trace_node_add_child(tree->root, axiom);
    lv_trace_node_set_status(tree->root, TRACE_STATUS_PROVED);
    lv_trace_node_set_status(axiom, TRACE_STATUS_PROVED);

    char *nl = lv_proof_to_natural_language(tree, PROOF_NL_LANG_ZH_CN);
    TEST_ASSERT_MSG(nl != NULL, "NL 导出非空");
    TEST_ASSERT_MSG(strlen(nl) > 0, "NL 非空串");
    lv_free((void **) &nl);

    char *nl_en = lv_proof_to_natural_language(tree, PROOF_NL_LANG_EN_US);
    TEST_ASSERT_MSG(nl_en != NULL, "NL EN 导出非空");
    lv_free((void **) &nl_en);

    char *latex = lv_proof_to_latex(tree);
    TEST_ASSERT_MSG(latex != NULL, "LaTeX 导出非空");
    TEST_ASSERT_MSG(strstr(latex, "documentclass") != NULL || strlen(latex) > 0, "LaTeX 内容");
    lv_free((void **) &latex);

    /* K35：LaTeX 转义回归——lv_str_latex_escape_alloc 对用户可控字符串
     * 含 `_ % \` 正确转义（lv_proof_to_latex 内部对 label/description/
     * rule name 使用同一转义，见 proof_export.c PROOF_LATEX_APPEND） */
    {
        char *e1 = lv_str_latex_escape_alloc("a_b%c\\d");
        TEST_ASSERT_MSG(e1 != NULL, "转义分配非空");
        if (e1) {
            TEST_ASSERT_MSG(strstr(e1, "a\\_b\\%c") != NULL, "label 特殊字符已转义");
            TEST_ASSERT_MSG(strstr(e1, "a_b%c") == NULL, "未保留未转义形式");
            lv_free((void **) &e1);
        }
        /* 空串与普通串安全 */
        char *e2 = lv_str_latex_escape_alloc(NULL);
        TEST_ASSERT_MSG(e2 != NULL && e2[0] == '\0', "NULL 输入按空串处理");
        lv_free((void **) &e2);
        char *e3 = lv_str_latex_escape_alloc("plain");
        TEST_ASSERT_MSG(e3 != NULL && strcmp(e3, "plain") == 0, "普通串原样");
        lv_free((void **) &e3);
    }

    char *coq = lv_proof_to_coq(tree);
    TEST_ASSERT_MSG(coq != NULL, "Coq 导出非空");
    lv_free((void **) &coq);

    char *isar = lv_proof_to_isar(tree);
    TEST_ASSERT_MSG(isar != NULL, "Isar 导出非空");
    lv_free((void **) &isar);

    lv_trace_tree_destroy(tree);
}

/* ============================================================
 * 主入口
 * ============================================================ */
TEST_MAIN_BEGIN("Proof Engine Enhanced")

    TEST_MAIN_RUN(test_trace_tree);
    TEST_MAIN_RUN(test_contradiction);
    TEST_MAIN_RUN(test_engine_lifecycle);
    TEST_MAIN_RUN(test_verify);
    TEST_MAIN_RUN(test_optimize);
    TEST_MAIN_RUN(test_export_formats);

TEST_MAIN_END()

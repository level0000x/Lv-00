/**
 * @file test_output_export.c
 * @brief Layer5 输出/导出模块深度测试 — TikZ Export / ProofWidget / LV Protocol
 *
 * 覆盖已有测试未深入触及的颜色系统全覆盖、协议数据生成全函数、
 * 资源释放安全、Widget 策略推荐全路径、TikZ 导出边界情况。
 *
 * @author Lv-00 Project
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/formula_converter.h"
#include "lv/formula_parser.h"
#include "lv/formula_renderer.h"
#include "lv/lv_protocol.h"
#include "lv/proof.h"
#include "lv/proof_widget.h"
#include "lv/tikz_export.h"

#include "lv.h"
#include "test_helpers.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ================================================================
 * 颜色系统全覆盖测试
 * ================================================================ */

#define COLOR_COUNT 12

static void test_color_all_named(void) {
    /* 所有 12 种颜色的名称 */
    const char *expected_names[] = {"Green",  "Blue",       "BlueRange", "Yellow", "Amber",  "LightOrange",
                                    "Orange", "DarkOrange", "Red",       "Grey",   "Purple", "Cyan"};
    const uint32_t expected_rgba[] = {0xFF3fb950, 0xFF58a6ff, 0xFF90caf9, 0xFFd29922, 0xFFffc107, 0xFFff9800,
                                      0xFFf0883e, 0xFFdb6d28, 0xFFf85149, 0xFF8b949e, 0xFFbc8cff, 0xFF39c5cf};
    const char *expected_svg[] = {"#3fb950", "#58a6ff", "#90caf9", "#d29922", "#ffc107", "#ff9800",
                                  "#f0883e", "#db6d28", "#f85149", "#8b949e", "#bc8cff", "#39c5cf"};

    for (int i = 0; i < COLOR_COUNT; i++) {
        lvTrustColor c = (lvTrustColor) i;
        TEST_ASSERT_STR_EQ(lv_trust_color_name(c), expected_names[i]);
        TEST_ASSERT_EQ(lv_trust_color_rgba(c), expected_rgba[i]);
        TEST_ASSERT_STR_EQ(lv_trust_color_svg(c), expected_svg[i]);

        const char *tikz = lv_trust_color_tikz(c);
        TEST_ASSERT_NOT_NULL(tikz);
        TEST_ASSERT(strstr(tikz, "HTML") != NULL, "tikz color has HTML");

        /* 双向映射 */
        TrustColor tc_back = lv_protocol_to_trust_color(c);
        lvTrustColor lv_back = trust_color_to_lv_protocol(tc_back);
        /* 仅验证映射不崩溃, 一些映射可能不是恒等 */
    }

    /* 越界 */
    TEST_ASSERT_STR_EQ(lv_trust_color_name((lvTrustColor) 99), "Unknown");
    TEST_ASSERT_EQ(lv_trust_color_rgba((lvTrustColor) 99), 0xFF888888);
    TEST_ASSERT_STR_EQ(lv_trust_color_svg((lvTrustColor) 99), "#888888");
    TEST_ASSERT_STR_EQ(lv_trust_color_tikz((lvTrustColor) 99), "{HTML}{888888}");
}

static void test_color_mapping_all(void) {
    /* trust_color_to_lv_protocol 所有分支 */
    TEST_ASSERT_EQ(trust_color_to_lv_protocol(TRUST_GREEN), lv_COLOR_GREEN);
    TEST_ASSERT_EQ(trust_color_to_lv_protocol(TRUST_BLUE_UNEXPLORED), lv_COLOR_BLUE);
    TEST_ASSERT_EQ(trust_color_to_lv_protocol(TRUST_BLUE_EXCEEDED), lv_COLOR_BLUE);
    TEST_ASSERT_EQ(trust_color_to_lv_protocol(TRUST_BLUE_OUT_OF_SCOPE), lv_COLOR_BLUE_RANGE);
    TEST_ASSERT_EQ(trust_color_to_lv_protocol(TRUST_YELLOW), lv_COLOR_YELLOW);
    TEST_ASSERT_EQ(trust_color_to_lv_protocol(TRUST_LIGHT_ORANGE_ORACLE), lv_COLOR_LIGHT_ORANGE);
    TEST_ASSERT_EQ(trust_color_to_lv_protocol(TRUST_LIGHT_ORANGE_EXPLOSION), lv_COLOR_ORANGE);
    TEST_ASSERT_EQ(trust_color_to_lv_protocol(TRUST_AMBER), lv_COLOR_AMBER);
    TEST_ASSERT_EQ(trust_color_to_lv_protocol(TRUST_DEEP_ORANGE), lv_COLOR_DARK_ORANGE);
    TEST_ASSERT_EQ(trust_color_to_lv_protocol(TRUST_RED), lv_COLOR_RED);
    TEST_ASSERT_EQ(trust_color_to_lv_protocol((TrustColor) 99), lv_COLOR_GREY);

    /* lv_protocol_to_trust_color 所有分支 */
    TEST_ASSERT_EQ(lv_protocol_to_trust_color(lv_COLOR_GREEN), TRUST_GREEN);
    TEST_ASSERT_EQ(lv_protocol_to_trust_color(lv_COLOR_BLUE), TRUST_BLUE_UNEXPLORED);
    TEST_ASSERT_EQ(lv_protocol_to_trust_color(lv_COLOR_BLUE_RANGE), TRUST_BLUE_OUT_OF_SCOPE);
    TEST_ASSERT_EQ(lv_protocol_to_trust_color(lv_COLOR_YELLOW), TRUST_YELLOW);
    TEST_ASSERT_EQ(lv_protocol_to_trust_color(lv_COLOR_AMBER), TRUST_AMBER);
    TEST_ASSERT_EQ(lv_protocol_to_trust_color(lv_COLOR_LIGHT_ORANGE), TRUST_LIGHT_ORANGE_ORACLE);
    TEST_ASSERT_EQ(lv_protocol_to_trust_color(lv_COLOR_ORANGE), TRUST_LIGHT_ORANGE_EXPLOSION);
    TEST_ASSERT_EQ(lv_protocol_to_trust_color(lv_COLOR_DARK_ORANGE), TRUST_DEEP_ORANGE);
    TEST_ASSERT_EQ(lv_protocol_to_trust_color(lv_COLOR_RED), TRUST_RED);
    TEST_ASSERT_EQ(lv_protocol_to_trust_color(lv_COLOR_GREY), TRUST_BLUE_UNEXPLORED);
    TEST_ASSERT_EQ(lv_protocol_to_trust_color(lv_COLOR_PURPLE), TRUST_GREEN);
    TEST_ASSERT_EQ(lv_protocol_to_trust_color(lv_COLOR_CYAN), TRUST_BLUE_UNEXPLORED);
    TEST_ASSERT_EQ(lv_protocol_to_trust_color((lvTrustColor) 99), TRUST_BLUE_UNEXPLORED);
}

/* ================================================================
 * 协议数据生成测试
 * ================================================================ */

static void test_proto_draw_commands(void) {
    lv_init();

    lvDrawCmdList list;
    memset(&list, 0, sizeof(list));

    /* NULL 安全 */
    TEST_ASSERT_EQ(lv_proto_draw_commands(NULL, 0, 0, 1.0, 800, 600, NULL), -1);

    /* 正常调用 */
    int rc = lv_proto_draw_commands(NULL, 10.0, 20.0, 1.5, 800, 600, &list);
    TEST_ASSERT_EQ(rc, 0);
    TEST_ASSERT(list.count > 0, "has draw commands");
    TEST_ASSERT_NOT_NULL(list.cmds);
    TEST_ASSERT(list.capacity >= list.count, "capacity should be valid");

    /* 验证第一个命令 */
    TEST_ASSERT(list.cmds[0].type == lv_DRAW_TEXT, "first cmd is text");
    TEST_ASSERT(list.cmds[0].x1 == 10.0, "x1 should equal 10.0");
    TEST_ASSERT(list.cmds[0].y1 == 20.0, "y1 should equal 20.0");

    /* 视口元数据 */
    TEST_ASSERT_EQ(list.viewport_offset_x, 10.0);
    TEST_ASSERT_EQ(list.viewport_offset_y, 20.0);
    TEST_ASSERT_EQ(list.viewport_scale, 1.5);
    TEST_ASSERT_EQ(list.canvas_width, 800);
    TEST_ASSERT_EQ(list.canvas_height, 600);

    lv_proto_free_draw_commands(&list);

    /* 释放后应为零 */
    TEST_ASSERT_NULL(list.cmds);
    TEST_ASSERT_EQ(list.count, 0);

    /* 双重释放安全 */
    lv_proto_free_draw_commands(&list);
    lv_proto_free_draw_commands(NULL);

    lv_cleanup();
}

static void test_proto_table_rows(void) {
    lv_init();

    lvTableRowList list;
    memset(&list, 0, sizeof(list));

    /* NULL 安全 */
    TEST_ASSERT_EQ(lv_proto_table_rows(NULL, NULL), -1);

    int rc = lv_proto_table_rows(NULL, &list);
    TEST_ASSERT_EQ(rc, 0);
    TEST_ASSERT(list.count > 0, "has table rows");
    TEST_ASSERT_NOT_NULL(list.rows);

    /* 验证行数据 */
    TEST_ASSERT(list.rows[0].id == 0, "first row id=0");
    TEST_ASSERT(strlen(list.rows[0].name) > 0, "row has name");

    lv_proto_free_table_rows(&list);
    lv_proto_free_table_rows(NULL);

    lv_cleanup();
}

static void test_proto_dsl_text(void) {
    lv_init();

    char buf[4096];
    memset(buf, 0, sizeof(buf));

    /* NULL 安全 */
    TEST_ASSERT_EQ(lv_proto_dsl_text(NULL, NULL, 0), -1);
    TEST_ASSERT_EQ(lv_proto_dsl_text(NULL, buf, 0), -1);

    int rc = lv_proto_dsl_text(NULL, buf, sizeof(buf));
    TEST_ASSERT_EQ(rc, 0);
    TEST_ASSERT(strlen(buf) > 0, "dsl text non-empty");
    TEST_ASSERT(strstr(buf, "%%") != NULL || strstr(buf, "Lv-00") != NULL, "has DSL comment markers");

    /* 极小缓冲区 */
    char tiny[4];
    rc = lv_proto_dsl_text(NULL, tiny, sizeof(tiny));
    TEST_ASSERT_EQ(rc, 0);

    lv_cleanup();
}

static void test_proto_tree(void) {
    lv_init();

    lvTreeNode *root = NULL;

    /* NULL 安全 */
    TEST_ASSERT_EQ(lv_proto_tree(NULL, NULL), -1);

    int rc = lv_proto_tree(NULL, &root);
    TEST_ASSERT_EQ(rc, 0);
    TEST_ASSERT_NOT_NULL(root);

    /* 根节点验证 */
    TEST_ASSERT(strlen(root->id) > 0, "root has id");
    TEST_ASSERT(strlen(root->label) > 0, "root has label");
    TEST_ASSERT(root->status == lv_TREE_ROOT, "root status");

    /* 子节点 */
    TEST_ASSERT(root->child_count > 0, "has children");
    TEST_ASSERT_NOT_NULL(root->children);

    for (int i = 0; i < root->child_count && i < 3; i++) {
        TEST_ASSERT_NOT_NULL(root->children[i]);
        TEST_ASSERT(strlen(root->children[i]->id) > 0, "child id should be non-empty");
    }

    lv_proto_free_tree(root);
    lv_proto_free_tree(NULL);

    lv_cleanup();
}

static void test_proto_topology(void) {
    lv_init();

    lvTopoGraph graph;
    memset(&graph, 0, sizeof(graph));

    /* NULL 安全 */
    TEST_ASSERT_EQ(lv_proto_topology(NULL, NULL), -1);

    int rc = lv_proto_topology(NULL, &graph);
    TEST_ASSERT_EQ(rc, 0);

    /* 验证拓扑结构：3 blocks, 2 edges */
    TEST_ASSERT(graph.block_count > 0, "has topology blocks");
    TEST_ASSERT(graph.edge_count > 0, "has topology edges");
    TEST_ASSERT_NOT_NULL(graph.blocks);
    TEST_ASSERT_NOT_NULL(graph.edges);

    /* 验证块数据 */
    TEST_ASSERT(strlen(graph.blocks[0].name) > 0, "block has name");
    TEST_ASSERT(graph.blocks[0].layout_x >= 0, "layout_x should be valid");

    /* 资源释放 */
    lv_proto_free_topology(&graph);
    lv_proto_free_topology(NULL);

    lv_cleanup();
}

static void test_proto_proof_navigator(void) {
    lv_init();

    lvProofNavigator nav;
    memset(&nav, 0, sizeof(nav));

    /* NULL 安全 */
    TEST_ASSERT_EQ(lv_proto_proof_navigator(NULL, NULL), -1);

    int rc = lv_proto_proof_navigator(NULL, &nav);
    TEST_ASSERT_EQ(rc, 0);

    /* 验证证明步骤 */
    TEST_ASSERT(nav.step_count > 0, "has proof steps");
    TEST_ASSERT_NOT_NULL(nav.steps);
    TEST_ASSERT(nav.total_steps > 0, "total steps > 0");
    TEST_ASSERT(nav.is_complete, "navigator is complete");

    /* 验证步骤数据 */
    TEST_ASSERT(nav.steps[0].step_id >= 0, "step_id should be valid");
    TEST_ASSERT(strlen(nav.steps[0].label) > 0, "step label should be non-empty");

    /* 策略和摘要 */
    TEST_ASSERT(strlen(nav.strategy_label) > 0, "strategy label should be non-empty");
    TEST_ASSERT(strlen(nav.nl_summary) > 0, "nl summary should be non-empty");

    lv_proto_free_proof(&nav);
    lv_proto_free_proof(NULL);

    lv_cleanup();
}

static void test_proto_engine_status(void) {
    lv_init();

    lvEngineStatus status;
    memset(&status, 0, sizeof(status));

    /* NULL 安全 */
    TEST_ASSERT_EQ(lv_proto_engine_status(NULL, NULL), -1);

    int rc = lv_proto_engine_status(NULL, &status);
    TEST_ASSERT_EQ(rc, 0);

    /* 验证状态字段 */
    TEST_ASSERT(status.node_count >= 0, "node_count should be valid");
    TEST_ASSERT(status.constraint_count >= 0, "constraint_count should be valid");
    TEST_ASSERT(status.memory_usage_mb >= 0.0, "memory_usage_mb should be valid");
    TEST_ASSERT(strlen(status.engine_state) > 0, "engine state non-empty");
    TEST_ASSERT(strlen(status.backend_info) > 0, "backend info non-empty");

    lv_cleanup();
}

static void test_proto_completions(void) {
    lv_init();

    lvCompletionList list;
    memset(&list, 0, sizeof(list));

    /* NULL 安全 */
    int rc = lv_proto_completions(NULL, NULL, NULL);
    TEST_ASSERT_EQ(rc, -1);

    /* 空前缀：应匹配所有 */
    rc = lv_proto_completions(NULL, "", &list);
    TEST_ASSERT_EQ(rc, 0);
    TEST_ASSERT(list.count > 0, "empty prefix matches all");
    lv_proto_free_completions(&list);

    /* 特定前缀匹配 */
    rc = lv_proto_completions(NULL, "add", &list);
    TEST_ASSERT_EQ(rc, 0);
    TEST_ASSERT(list.count > 0, "prefix 'add' matches");
    if (list.count > 0 && list.items) {
        TEST_ASSERT_NOT_NULL(list.items[0].text);
        TEST_ASSERT(strstr(list.items[0].text, "add") != NULL, "completion starts with 'add'");
    }
    lv_proto_free_completions(&list);

    /* 无匹配前缀 */
    rc = lv_proto_completions(NULL, "zzz_nonexistent_prefix_xyz", &list);
    TEST_ASSERT_EQ(rc, 0);
    TEST_ASSERT_EQ(list.count, 0);

    /* NULL prefix 等价于空字符串 */
    rc = lv_proto_completions(NULL, NULL, &list);
    TEST_ASSERT_EQ(rc, 0);
    TEST_ASSERT(list.count > 0, "NULL prefix matches all");
    lv_proto_free_completions(&list);

    /* 双层释放安全 */
    lv_proto_free_completions(NULL);

    lv_cleanup();
}

static void test_proto_terminal_exec(void) {
    lv_init();

    lvTerminalResponse resp;
    memset(&resp, 0, sizeof(resp));

    /* NULL 安全 */
    TEST_ASSERT_EQ(lv_proto_terminal_exec(NULL, NULL, NULL), -1);
    TEST_ASSERT_EQ(lv_proto_terminal_exec(NULL, NULL, &resp), 0);

    /* NULL command → error */
    int rc = lv_proto_terminal_exec(NULL, NULL, &resp);
    TEST_ASSERT_EQ(rc, 0);
    TEST_ASSERT(!resp.success, "response should indicate failure");
    TEST_ASSERT_EQ(resp.error_code, -1);

    /* 有效命令 */
    rc = lv_proto_terminal_exec((void *) 0x1, "solve", &resp);
    TEST_ASSERT_EQ(rc, 0);
    TEST_ASSERT(resp.success, "response should indicate success");
    TEST_ASSERT(strlen(resp.output) > 0, "has output");

    lv_cleanup();
}

/* ================================================================
 * TikZ 导出深度测试
 * ================================================================ */

static void test_tikz_export_points_only(void) {
    lv_init();
    lvEngine *e = lv_engine_create();
    TEST_ASSERT_NOT_NULL(e);

    /* 添加多个点但不连线段 */
    lv_add_point(e, 0, 1, 0, 1);
    lv_add_point(e, 1, 1, 1, 1);
    lv_add_point(e, 2, 1, 0, 1);

    char buf[4096];
    int n = lv_tikz_export((void *) e->main_graph, buf, sizeof(buf));
    TEST_ASSERT(n > 0, "points-only export");
    TEST_ASSERT(strstr(buf, "\\fill") != NULL, "has point commands");
    TEST_ASSERT(strstr(buf, "\\draw") == NULL, "no line commands (only points)");

    lv_engine_destroy(e);
    lv_cleanup();
}

static void test_tikz_export_segments_only(void) {
    lv_init();
    lvEngine *e = lv_engine_create();
    TEST_ASSERT_NOT_NULL(e);

    lv_add_point(e, 0, 1, 0, 1);
    lv_add_point(e, 1, 1, 0, 1);
    lv_add_point(e, 0, 1, 1, 1);
    lv_add_line_segment(e, 0, 1);
    lv_add_line_segment(e, 1, 2);

    char buf[4096];
    int n = lv_tikz_export((void *) e->main_graph, buf, sizeof(buf));
    TEST_ASSERT(n > 0, "segments export");
    TEST_ASSERT(strstr(buf, "\\fill") != NULL, "has point commands");
    TEST_ASSERT(strstr(buf, "\\draw") != NULL, "has line commands");

    lv_engine_destroy(e);
    lv_cleanup();
}

static void test_tikz_export_file_safety(void) {
    lv_init();
    lvEngine *e = lv_engine_create();
    TEST_ASSERT_NOT_NULL(e);

    lv_add_point(e, 0, 1, 0, 1);

    /* 导出到临时文件 */
    int n = lv_tikz_export_file((void *) e->main_graph, "test_tikz_temp.tex");
    TEST_ASSERT(n > 0, "file export");

    /* 读取验证 */
    FILE *fp = fopen("test_tikz_temp.tex", "r");
    TEST_ASSERT_NOT_NULL(fp);
    char check[256] = {0};
    TEST_ASSERT_NOT_NULL(fgets(check, sizeof(check), fp));
    TEST_ASSERT(strstr(check, "tikzpicture") != NULL || strstr(check, "Lv-00") != NULL, "file contains tikz content");
    fclose(fp);

    lv_engine_destroy(e);
    lv_cleanup();
}

static void test_tikz_export_empty_graph(void) {
    lv_init();
    lvEngine *e = lv_engine_create();
    TEST_ASSERT_NOT_NULL(e);

    char buf[128];
    int n = lv_tikz_export((void *) e->main_graph, buf, sizeof(buf));
    TEST_ASSERT(n > 0, "empty graph still produces tikzpicture env");
    TEST_ASSERT(strstr(buf, "tikzpicture") != NULL, "has tikzpicture even empty");

    lv_engine_destroy(e);
    lv_cleanup();
}

/* ================================================================
 * Proof Widget 深度测试
 * ================================================================ */

static void test_widget_layout_negative_capacity(void) {
    /* 容量 <= 0 时使用默认值 8 */
    lvWidgetLayout *layout = proof_widget_init(0);
    TEST_ASSERT_NOT_NULL(layout);
    proof_widget_destroy(layout);

    layout = proof_widget_init(-5);
    TEST_ASSERT_NOT_NULL(layout);
    proof_widget_destroy(layout);
}

static void test_widget_register_all_types(void) {
    lvWidgetLayout *layout = proof_widget_init(10);
    TEST_ASSERT_NOT_NULL(layout);

    ProofWidgetType all_types[] = {WIDGET_GOAL_DISPLAY,     WIDGET_HYPOTHESIS_PANEL, WIDGET_APPLY_BUTTON,
                                   WIDGET_STEP_NAVIGATOR,   WIDGET_SEARCH_TREE,      WIDGET_TIMELINE,
                                   WIDGET_DEPENDENCY_GRAPH, WIDGET_TACTIC_HISTORY};
    int n_types = sizeof(all_types) / sizeof(all_types[0]);

    for (int i = 0; i < n_types; i++) {
        int id = proof_widget_register(layout, all_types[i], NULL, i);
        TEST_ASSERT(id >= 0, "widget registration should return non-negative id");
    }
    TEST_ASSERT_EQ(layout->widget_count, n_types);

    proof_widget_destroy(layout);
}

static void test_widget_update_full(void) {
    lvWidgetLayout *layout = proof_widget_init(4);
    int id = proof_widget_register(layout, WIDGET_GOAL_DISPLAY, "Original", 0);
    TEST_ASSERT(id >= 0, "id >= 0");

    /* 更新所有字段 */
    TEST_ASSERT_EQ(proof_widget_update(layout, id, true, false, "Updated Label", 5, "{\"key\":\"val\"}"), 0);

    /* 验证（通过导出 JSON 检查） */
    char *json = proof_widget_export_layout(layout);
    TEST_ASSERT_NOT_NULL(json);
    TEST_ASSERT(strstr(json, "Updated Label") != NULL, "label updated");
    TEST_ASSERT(strstr(json, "false") != NULL || strstr(json, "true") != NULL, "has boolean flags");
    lv_free((void **)&json);

    /* 更新仅部分字段 */
    TEST_ASSERT_EQ(proof_widget_update(layout, id, true, true, NULL, 3, NULL), 0);

    proof_widget_destroy(layout);
}

static void test_widget_suggest_tactic(void) {
    char *suggestions[5];
    double confidences[5];
    memset(suggestions, 0, sizeof(suggestions));
    memset(confidences, 0, sizeof(confidences));

    /* NULL navigator */
    int rc = proof_widget_suggest_tactic(NULL, suggestions, confidences, 3);
    TEST_ASSERT_EQ(rc, -1);

    /* max_count = 0 */
    rc = proof_widget_suggest_tactic((ProofNavigator *) 0x1, suggestions, confidences, 0);
    TEST_ASSERT_EQ(rc, -1);

    /* 正常调用（真实导航器，含等号目标 → reflexivity/rewrite/auto 按置信度排序） */
    Proposition *target = proposition_create(1, PROPOSITION_TYPE_ATOMIC);
    TEST_ASSERT_NOT_NULL(target);
    target->name = lv_strdup("AB = BA");
    ProofNavigator *nav = proof_navigator_create(target, NULL);
    TEST_ASSERT_NOT_NULL(nav);

    rc = proof_widget_suggest_tactic(nav, suggestions, confidences, 3);
    TEST_ASSERT_EQ(rc, 3);
    TEST_ASSERT_NOT_NULL(suggestions[0]);
    TEST_ASSERT_NOT_NULL(suggestions[1]);
    TEST_ASSERT_NOT_NULL(suggestions[2]);
    TEST_ASSERT(confidences[0] > confidences[1], "sorted by confidence");
    TEST_ASSERT(confidences[1] > confidences[2], "sorted by confidence");

    /* 清理 */
    for (int i = 0; i < 3; i++) {
        if (suggestions[i])
            lv_free((void **)&suggestions[i]);
    }
    proof_navigator_destroy(nav);
    proposition_destroy(target);
}

static void test_widget_get_step_highlights(void) {
    lvProofStepHighlight highlights[5];
    memset(highlights, 0, sizeof(highlights));

    /* NULL 安全 */
    int rc = proof_widget_get_step_highlights(NULL, highlights, 3);
    TEST_ASSERT_EQ(rc, -1);

    rc = proof_widget_get_step_highlights((ProofNavigator *) 0x1, NULL, 3);
    TEST_ASSERT_EQ(rc, -1);

    rc = proof_widget_get_step_highlights((ProofNavigator *) 0x1, highlights, 0);
    TEST_ASSERT_EQ(rc, -1);

    /* 正常调用 */
    rc = proof_widget_get_step_highlights((ProofNavigator *) 0x1, highlights, 3);
    TEST_ASSERT_EQ(rc, 0);
    TEST_ASSERT_EQ(highlights[0].step_id, 0);
    TEST_ASSERT_EQ(highlights[1].step_id, 1);
    TEST_ASSERT_EQ(highlights[2].step_id, 2);
    TEST_ASSERT(highlights[0].color == HIGHLIGHT_NORMAL, "default normal");
    TEST_ASSERT(!highlights[0].is_animated, "default should not be animated");
}

static void test_widget_get_goal_hypotheses(void) {
    lvGoalDisplay goal;
    memset(&goal, 0, sizeof(goal));

    /* NULL 安全 */
    TEST_ASSERT_EQ(proof_widget_get_goal(NULL, &goal), -1);
    TEST_ASSERT_EQ(proof_widget_get_goal((ProofNavigator *) 0x1, NULL), -1);

    /* 正常调用（真实导航器） */
    Proposition *target = proposition_create(1, PROPOSITION_TYPE_ATOMIC);
    TEST_ASSERT_NOT_NULL(target);
    target->name = lv_strdup("AB = BA");
    ProofNavigator *nav = proof_navigator_create(target, NULL);
    TEST_ASSERT_NOT_NULL(nav);

    int rc = proof_widget_get_goal(nav, &goal);
    TEST_ASSERT_EQ(rc, 0);
    TEST_ASSERT_NOT_NULL(goal.goal_text);
    TEST_ASSERT(!goal.is_solved, "goal should not be solved initially");

    /* 释放 */
    goal_display_free(&goal);
    goal_display_free(NULL);

    /* hypotheses */
    lvHypothesisEntry hypos[3];
    memset(hypos, 0, sizeof(hypos));

    rc = proof_widget_get_hypotheses(NULL, hypos, 3);
    TEST_ASSERT_EQ(rc, -1);

    rc = proof_widget_get_hypotheses((ProofNavigator *) 0x1, NULL, 3);
    TEST_ASSERT_EQ(rc, -1);

    rc = proof_widget_get_hypotheses((ProofNavigator *) 0x1, hypos, 0);
    TEST_ASSERT_EQ(rc, -1);

    rc = proof_widget_get_hypotheses(nav, hypos, 3);
    TEST_ASSERT_EQ(rc, 0);
    TEST_ASSERT_EQ(hypos[0].hyp_id, 0);

    /* 清理 */
    proof_navigator_destroy(nav);
    proposition_destroy(target);
}

static void test_widget_search_tree_dep_graph(void) {
    /* Search tree */
    char *tree = proof_widget_get_search_tree(NULL);
    TEST_ASSERT_NULL(tree);

    tree = proof_widget_get_search_tree((ProofNavigator *) 0x1);
    TEST_ASSERT_NOT_NULL(tree);
    TEST_ASSERT(strstr(tree, "search_tree") != NULL || strstr(tree, "type") != NULL, "strstr should succeed");
    lv_free((void **)&tree);

    /* Dependency graph */
    char *dep = proof_widget_get_dependency_graph(NULL);
    TEST_ASSERT_NULL(dep);

    dep = proof_widget_get_dependency_graph((ProofNavigator *) 0x1);
    TEST_ASSERT_NOT_NULL(dep);
    TEST_ASSERT(strstr(dep, "dependency_graph") != NULL || strstr(dep, "type") != NULL, "strstr should succeed");
    lv_free((void **)&dep);
}

static void test_widget_apply_tactic(void) {
    bool success;
    char *feedback = NULL;

    /* NULL 安全 */
    int rc = proof_widget_apply_tactic(NULL, "intro", NULL, &success, &feedback);
    TEST_ASSERT_EQ(rc, -1);

    rc = proof_widget_apply_tactic((ProofNavigator *) 0x1, NULL, NULL, &success, &feedback);
    TEST_ASSERT_EQ(rc, -1);

    /* 但 apply_tactic 会试图解引用 navigator，所以只能测 NULL 情况 */
    /* 不过我们可以测试返回码 */
    TEST_ASSERT_EQ(proof_widget_apply_tactic(NULL, NULL, NULL, NULL, NULL), -1);
}

static void test_widget_export_layout_null(void) {
    char *json = proof_widget_export_layout(NULL);
    TEST_ASSERT_NULL(json);
}

static void test_widget_set_order_edge(void) {
    lvWidgetLayout *layout = proof_widget_init(4);
    TEST_ASSERT_NOT_NULL(layout);

    /* NULL 安全 */
    proof_widget_set_order(NULL, NULL, 0);
    proof_widget_set_order(layout, NULL, 0);
    proof_widget_set_order(layout, NULL, -1);

    /* 正常设置 */
    int order[] = {2, 1, 0, 3};
    proof_widget_set_order(layout, order, 4);

    proof_widget_destroy(layout);
}

static void test_widget_layout_type_null(void) {
    /* NULL 安全 */
    proof_widget_set_layout_type(NULL, LAYOUT_GRID, 2, 2);
    proof_widget_set_persistence_key(NULL, "key");
    proof_widget_set_persistence_key(NULL, NULL);

    lvWidgetLayout *layout = proof_widget_init(4);
    proof_widget_set_layout_type(layout, LAYOUT_TABBED, 0, 0);     /* 0 → 默认 2 */
    proof_widget_set_layout_type(layout, LAYOUT_VERTICAL, -1, -1); /* 负数 → 默认 */
    proof_widget_destroy(layout);
}

/* ================================================================
 * 公式渲染器表驱动收敛测试（盲区1 / 盲区2）：输出逐字节一致
 * ================================================================ */

static void test_formula_render_byte_identical(void) {
    /* 构造 1 + 2 二元运算 AST */
    FormulaNode *n1 = formula_create_number(1, 1);
    FormulaNode *n2 = formula_create_number(2, 1);
    TEST_ASSERT_NOT_NULL(n1);
    TEST_ASSERT_NOT_NULL(n2);
    FormulaNode *add = formula_create_binary_op(NODE_BINARY_OP_ADD, n1, n2);
    TEST_ASSERT_NOT_NULL(add);

    char buf[256];
    int n;

    /* 四个已收敛后端（dsl/ascii/latex/python）输出与历史格式串逐字节一致 */
    n = formula_render_to_buffer(add, OUTPUT_ASCII, buf, sizeof(buf));
    TEST_ASSERT(n > 0, "ascii render n>0");
    TEST_ASSERT_STR_EQ(buf, "(1 + 2)");

    n = formula_render_to_buffer(add, OUTPUT_PYTHON, buf, sizeof(buf));
    TEST_ASSERT(n > 0, "python render n>0");
    TEST_ASSERT_STR_EQ(buf, "(1 + 2)");

    n = formula_render_to_buffer(add, OUTPUT_DSL, buf, sizeof(buf));
    TEST_ASSERT(n > 0, "dsl render n>0");
    TEST_ASSERT_STR_EQ(buf, "1 + 2");

    n = formula_render_to_buffer(add, OUTPUT_LATEX, buf, sizeof(buf));
    TEST_ASSERT(n > 0, "latex render n>0");
    TEST_ASSERT_STR_EQ(buf, "1 + 2");

    /* 一元运算：sqrt(4) */
    FormulaNode *n4 = formula_create_number(4, 1);
    TEST_ASSERT_NOT_NULL(n4);
    FormulaNode *sqrt_node = formula_create_unary_op(NODE_UNARY_OP_SQRT, n4);
    TEST_ASSERT_NOT_NULL(sqrt_node);

    n = formula_render_to_buffer(sqrt_node, OUTPUT_ASCII, buf, sizeof(buf));
    TEST_ASSERT(n > 0, "ascii sqrt n>0");
    TEST_ASSERT_STR_EQ(buf, "sqrt(4)");

    n = formula_render_to_buffer(sqrt_node, OUTPUT_PYTHON, buf, sizeof(buf));
    TEST_ASSERT(n > 0, "python sqrt n>0");
    TEST_ASSERT_STR_EQ(buf, "sqrt(4)");

    n = formula_render_to_buffer(sqrt_node, OUTPUT_DSL, buf, sizeof(buf));
    TEST_ASSERT(n > 0, "dsl sqrt n>0");
    TEST_ASSERT_STR_EQ(buf, "sqrt(4)");

    n = formula_render_to_buffer(sqrt_node, OUTPUT_LATEX, buf, sizeof(buf));
    TEST_ASSERT(n > 0, "latex sqrt n>0");
    TEST_ASSERT_STR_EQ(buf, "\\sqrt{4}");

    /* 一元负号：-x */
    FormulaNode *vx = formula_create_variable("x");
    TEST_ASSERT_NOT_NULL(vx);
    FormulaNode *neg = formula_create_unary_op(NODE_UNARY_OP_NEG, vx);
    TEST_ASSERT_NOT_NULL(neg);

    n = formula_render_to_buffer(neg, OUTPUT_ASCII, buf, sizeof(buf));
    TEST_ASSERT(n > 0, "ascii neg n>0");
    TEST_ASSERT_STR_EQ(buf, "-(x)");

    n = formula_render_to_buffer(neg, OUTPUT_PYTHON, buf, sizeof(buf));
    TEST_ASSERT(n > 0, "python neg n>0");
    TEST_ASSERT_STR_EQ(buf, "(-x)");

    n = formula_render_to_buffer(neg, OUTPUT_DSL, buf, sizeof(buf));
    TEST_ASSERT(n > 0, "dsl neg n>0");
    TEST_ASSERT_STR_EQ(buf, "-x");

    n = formula_render_to_buffer(neg, OUTPUT_LATEX, buf, sizeof(buf));
    TEST_ASSERT(n > 0, "latex neg n>0");
    TEST_ASSERT_STR_EQ(buf, "-x");

    /* 释放整棵树（父节点持有子节点引用，需分别释放） */
    formula_node_destroy(add);
    formula_node_destroy(n1);
    formula_node_destroy(n2);
    formula_node_destroy(sqrt_node);
    formula_node_destroy(n4);
    formula_node_destroy(neg);
    formula_node_destroy(vx);
}

static void test_graph_to_formula_render_append(void) {
    /* 盲区2：RenderTarget + render_append 收敛后的小图逐字节输出契约 */
    lv_init();
    lvEngine *e = lv_engine_create();
    TEST_ASSERT_NOT_NULL(e);

    int p0 = lv_add_point(e, 0, 1, 1, 1); /* 点 (0, 1) */
    TEST_ASSERT(p0 >= 0, "add point (0,1)");

    char pname[16];
    snprintf(pname, sizeof(pname), "P%d", p0);
    char expected_latex[128];
    char expected_python[128];
    char expected_dsl[128];
    snprintf(expected_latex, sizeof(expected_latex), "%s = \\left(0.00, 1.00\\right)\\\\\n", pname);
    snprintf(expected_python, sizeof(expected_python), "%s = Point(0.00, 1.00)\n", pname);
    snprintf(expected_dsl, sizeof(expected_dsl), "point %s(0.00, 1.00); ", pname);

    GraphToFormulaResult *r = graph_to_formula(e->main_graph);
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT(r->success, "graph_to_formula success");

    TEST_ASSERT_STR_EQ(r->latex_output, expected_latex);
    TEST_ASSERT_STR_EQ(r->python_output, expected_python);
    TEST_ASSERT_STR_EQ(r->dsl_output, expected_dsl);

    graph_to_formula_result_destroy(r);
    lv_engine_destroy(e);
    lv_cleanup();
}

static void test_graph_to_formula_render_append_truncation(void) {
    /* 盲区2：render_append 在缓冲写满时静默跳过整次追加，不越界、不产生部分拷贝 */
    lv_init();
    lvEngine *e = lv_engine_create();
    TEST_ASSERT_NOT_NULL(e);

    int first_id = -1, last_id = -1;
    for (int i = 0; i < 300; i++) {
        int p = lv_add_point(e, i, 1, 0, 1); /* 点 (i, 0) */
        TEST_ASSERT(p >= 0, "add point");
        if (first_id < 0)
            first_id = p;
        last_id = p;
    }
    TEST_ASSERT(first_id >= 0, "first point id");

    GraphToFormulaResult *r = graph_to_formula(e->main_graph);
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT(r->success, "graph_to_formula success");
    TEST_ASSERT_NOT_NULL(r->latex_output);
    TEST_ASSERT_NOT_NULL(r->python_output);
    TEST_ASSERT_NOT_NULL(r->dsl_output);

    /* 三个输出均在 4096 容量内 NUL 终止（FORMULA_EXPORT_BUF_SIZE），无越界写 */
    TEST_ASSERT(strlen(r->latex_output) < 4096, "latex truncated within capacity");
    TEST_ASSERT(strlen(r->python_output) < 4096, "python truncated within capacity");
    TEST_ASSERT(strlen(r->dsl_output) < 4096, "dsl truncated within capacity");

    /* 截断语义：头部内容完整保留，尾部（最后一个节点）被跳过 */
    char first_name[16], last_name[16];
    snprintf(first_name, sizeof(first_name), "P%d", first_id);
    snprintf(last_name, sizeof(last_name), "P%d", last_id);
    TEST_ASSERT(strstr(r->latex_output, first_name) != NULL, "latex keeps head content");
    TEST_ASSERT(strstr(r->latex_output, last_name) == NULL, "latex truncation skips tail");

    graph_to_formula_result_destroy(r);
    lv_engine_destroy(e);
    lv_cleanup();
}

/* ================================================================
 * Main
 * ================================================================ */

TEST_MAIN_BEGIN("Output Export (TikZ / Proof Widget / Protocol)")

    /* ── 颜色系统 ── */
    printf("\n--- Color System ---\n");
    TEST_MAIN_RUN(test_color_all_named);
    TEST_MAIN_RUN(test_color_mapping_all);

    /* ── 协议数据生成 ── */
    printf("\n--- Protocol (lv_proto_*) ---\n");
    TEST_MAIN_RUN(test_proto_draw_commands);
    TEST_MAIN_RUN(test_proto_table_rows);
    TEST_MAIN_RUN(test_proto_dsl_text);
    TEST_MAIN_RUN(test_proto_tree);
    TEST_MAIN_RUN(test_proto_topology);
    TEST_MAIN_RUN(test_proto_proof_navigator);
    TEST_MAIN_RUN(test_proto_engine_status);
    TEST_MAIN_RUN(test_proto_completions);
    TEST_MAIN_RUN(test_proto_terminal_exec);

    /* ── TikZ 导出 ── */
    printf("\n--- TikZ Export ---\n");
    TEST_MAIN_RUN(test_tikz_export_points_only);
    TEST_MAIN_RUN(test_tikz_export_segments_only);
    TEST_MAIN_RUN(test_tikz_export_file_safety);
    TEST_MAIN_RUN(test_tikz_export_empty_graph);

    /* ── Proof Widget ── */
    printf("\n--- Proof Widget ---\n");
    TEST_MAIN_RUN(test_widget_layout_negative_capacity);
    TEST_MAIN_RUN(test_widget_register_all_types);
    TEST_MAIN_RUN(test_widget_update_full);
    TEST_MAIN_RUN(test_widget_suggest_tactic);
    TEST_MAIN_RUN(test_widget_get_step_highlights);
    TEST_MAIN_RUN(test_widget_get_goal_hypotheses);
    TEST_MAIN_RUN(test_widget_search_tree_dep_graph);
    TEST_MAIN_RUN(test_widget_apply_tactic);
    TEST_MAIN_RUN(test_widget_export_layout_null);
    TEST_MAIN_RUN(test_widget_set_order_edge);
    TEST_MAIN_RUN(test_widget_layout_type_null);

    /* ── 公式渲染 / 转换（K6 收敛回归） ── */
    printf("\n--- Formula Render / Convert (K6) ---\n");
    TEST_MAIN_RUN(test_formula_render_byte_identical);
    TEST_MAIN_RUN(test_graph_to_formula_render_append);
    TEST_MAIN_RUN(test_graph_to_formula_render_append_truncation);

TEST_MAIN_END()

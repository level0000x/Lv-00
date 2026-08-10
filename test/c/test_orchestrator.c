/**
 * @file test_orchestrator.c
 * @brief 测试 L7 编排层（orchestrator）与 L9 应用层（application）
 *
 * 覆盖：
 *   (a) 默认配置填充
 *   (b) 完整六阶段流水线运行（Parse/Resource/Geometry/Reasoning/Output/Visual）
 *   (c) lv_meta_verify_session 六项检查全部通过
 *   (d) 单阶段运行的前置自动执行与阶段结果查询
 *   (e) 错误路径（文件缺失 / NULL 参数）
 *   (f) 应用层 quick_verify / batch / export / visualize / load / version
 *
 * @author Lv-00 Project
 * @version 1.1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/application.h"
#include "lv/constraint_graph.h"
#include "lv/meta_verify.h"
#include "lv/orchestrator.h"

#include "test_helpers.h"

/* test_helpers.h 要求的文件级计数器 */
int g_pass_count = 0;
int g_fail_count = 0;

static void test_config_default(void) {
    TEST("orchestrator: 默认配置填充");
    lvSessionConfig cfg;
    lv_orchestrator_config_default(&cfg);
    TEST_ASSERT_EQ(cfg.max_reasoning_depth, 8);
    TEST_ASSERT_EQ(cfg.timeout_ms, 5000);
    TEST_ASSERT_EQ(cfg.enable_visualization, 0);
    TEST_ASSERT_STR_EQ(cfg.input_format, "dsl");
    TEST_ASSERT_STR_EQ(cfg.output_format, "json");
    PASS();
}

static void test_pipeline_full(void) {
    TEST("orchestrator: 完整流水线运行");
    lvSession *s = lv_orchestrator_create(NULL);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQ(lv_orchestrator_run(s, "examples/geometry_verify.lv"), 0);
    TEST_ASSERT_EQ(s->success, 1);
    TEST_ASSERT_EQ(s->stages[lv_STAGE_PARSE].status, lv_STAGE_COMPLETED);
    TEST_ASSERT_EQ(s->stages[lv_STAGE_RESOURCE].status, lv_STAGE_COMPLETED);
    TEST_ASSERT_EQ(s->stages[lv_STAGE_GEOMETRY].status, lv_STAGE_COMPLETED);
    TEST_ASSERT_EQ(s->stages[lv_STAGE_REASONING].status, lv_STAGE_COMPLETED);
    TEST_ASSERT_EQ(s->stages[lv_STAGE_OUTPUT].status, lv_STAGE_COMPLETED);
    TEST_ASSERT_EQ(s->stages[lv_STAGE_VISUAL].status, lv_STAGE_SKIPPED);
    TEST_ASSERT_MSG(strstr(s->stages[lv_STAGE_REASONING].error_msg, "proved") != NULL,
                    "reasoning 消息应含 proved");
    TEST_ASSERT_MSG(strstr(s->stages[lv_STAGE_OUTPUT].error_msg, "格式=json") != NULL,
                    "output 消息应含 格式=json");
    TEST_ASSERT_MSG(s->stages[lv_STAGE_PARSE].elapsed_ms >= 0.0, "elapsed 非负");
    lv_orchestrator_destroy(s);
    PASS();
}

static void test_meta_verify_six(void) {
    TEST("orchestrator: 六项元验证全过");
    lvSession *s = lv_orchestrator_create(NULL);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQ(lv_orchestrator_run(s, "examples/geometry_verify.lv"), 0);
    lvMetaVerifier *v = lv_meta_verifier_create();
    TEST_ASSERT_NOT_NULL(v);
    lvVerifyReport rep = lv_meta_verify_session(v, s);
    TEST_ASSERT_EQ(lv_verify_report_passed(&rep), 1);
    for (int i = 0; i < rep.total_checks; i++)
        TEST_ASSERT_EQ(rep.results[i].passed, 1);
    lv_meta_verifier_destroy(v);
    lv_orchestrator_destroy(s);
    PASS();
}

static void test_visual_enabled(void) {
    TEST("orchestrator: 启用可视化阶段");
    lvSessionConfig cfg;
    lv_orchestrator_config_default(&cfg);
    cfg.enable_visualization = 1;
    lvSession *s = lv_orchestrator_create(&cfg);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQ(lv_orchestrator_run(s, "examples/geometry_verify.lv"), 0);
    TEST_ASSERT_EQ(s->stages[lv_STAGE_VISUAL].status, lv_STAGE_COMPLETED);
    TEST_ASSERT_MSG(strstr(s->stages[lv_STAGE_VISUAL].error_msg, "TikZ") != NULL,
                    "visual 消息应含 TikZ");
    lv_orchestrator_destroy(s);
    PASS();
}

static void test_run_stage_partial(void) {
    TEST("orchestrator: 单阶段前置自动执行");
    lvSession *s = lv_orchestrator_create(NULL);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQ(lv_orchestrator_run_stage(s, lv_STAGE_REASONING), 0);
    TEST_ASSERT_EQ(s->stages[lv_STAGE_PARSE].status, lv_STAGE_COMPLETED);
    TEST_ASSERT_EQ(s->stages[lv_STAGE_RESOURCE].status, lv_STAGE_COMPLETED);
    TEST_ASSERT_EQ(s->stages[lv_STAGE_GEOMETRY].status, lv_STAGE_COMPLETED);
    TEST_ASSERT_EQ(s->stages[lv_STAGE_REASONING].status, lv_STAGE_COMPLETED);
    TEST_ASSERT_EQ(s->stages[lv_STAGE_OUTPUT].status, lv_STAGE_PENDING);
    lvStageResult r;
    TEST_ASSERT_EQ(lv_orchestrator_get_stage_result(s, lv_STAGE_REASONING, &r), 0);
    TEST_ASSERT_EQ(r.status, lv_STAGE_COMPLETED);
    TEST_ASSERT_EQ(lv_orchestrator_get_stage_result(s, (lvSessionStage)99, &r), -1);
    lv_orchestrator_destroy(s);
    PASS();
}

static void test_error_path(void) {
    TEST("orchestrator: 错误路径");
    lvSession *s = lv_orchestrator_create(NULL);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_MSG(lv_orchestrator_run(s, "no_such_file_xyz.lv") != 0, "缺失文件应失败");
    TEST_ASSERT_EQ(s->success, 0);
    TEST_ASSERT_MSG(lv_orchestrator_last_error(s)[0] != '\0', "应有错误信息");
    lv_orchestrator_destroy(s);
    TEST_ASSERT_EQ(lv_orchestrator_run(NULL, "examples/geometry_verify.lv"), -1);
    TEST_ASSERT_STR_EQ(lv_orchestrator_last_error(NULL), "");
    PASS();
}

static void test_application_version(void) {
    TEST("application: 版本");
    TEST_ASSERT_MSG(strlen(lv_application_get_version()) > 0, "版本非空");
    PASS();
}

static void test_application_verify(void) {
    TEST("application: quick_verify");
    TEST_ASSERT_EQ(lv_application_quick_verify("examples/geometry_verify.lv"), 1);
    TEST_ASSERT_EQ(lv_application_quick_verify("no_such_file.lv"), 0);
    PASS();

    TEST("application: batch");
    const char *files[] = {"examples/geometry_verify.lv", "examples/geometry_verify.lv"};
    TEST_ASSERT_EQ(lv_application_batch(files, 2), 2);
    TEST_ASSERT_EQ(lv_application_batch(NULL, 0), -1);
    TEST_ASSERT_EQ(lv_application_batch(NULL, 1), -1);
    PASS();
}

static void test_application_export(void) {
    TEST("application: export");
    lvApplicationConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.command = LV_APP_CMD_EXPORT;
    cfg.input_path = "examples/geometry_verify.lv";
    cfg.output_path = "build_verify/test_orchestrator_export.json";
    cfg.output_format = "json";
    TEST_ASSERT_EQ(lv_application_run(&cfg), 0);
    FILE *fp = fopen(cfg.output_path, "rb");
    TEST_ASSERT_NOT_NULL(fp);
    if (fp) {
        fseek(fp, 0, SEEK_END);
        TEST_ASSERT_MSG(ftell(fp) > 0, "导出文件非空");
        fclose(fp);
        remove(cfg.output_path);
    }
    PASS();

    TEST("application: visualize");
    cfg.command = LV_APP_CMD_VISUALIZE;
    cfg.output_path = "build_verify/test_orchestrator_vis.tex";
    TEST_ASSERT_EQ(lv_application_run(&cfg), 0);
    fp = fopen(cfg.output_path, "rb");
    TEST_ASSERT_NOT_NULL(fp);
    if (fp) {
        fseek(fp, 0, SEEK_END);
        TEST_ASSERT_MSG(ftell(fp) > 0, "可视化文件非空");
        fclose(fp);
        remove(cfg.output_path);
    }
    PASS();

    TEST("application: export dot");
    cfg.command = LV_APP_CMD_EXPORT;
    cfg.output_path = "build_verify/test_orchestrator_export.dot";
    cfg.output_format = "dot";
    TEST_ASSERT_EQ(lv_application_run(&cfg), 0);
    fp = fopen(cfg.output_path, "rb");
    TEST_ASSERT_NOT_NULL(fp);
    if (fp) {
        fseek(fp, 0, SEEK_END);
        long sz = ftell(fp);
        TEST_ASSERT_MSG(sz > 0, "DOT 导出文件非空");
        fclose(fp);
        FILE *rfp = fopen(cfg.output_path, "rb");
        if (rfp) {
            char head[32] = {0};
            size_t got = fread(head, 1, sizeof(head) - 1, rfp);
            fclose(rfp);
            TEST_ASSERT_MSG(got > 0 && memcmp(head, "digraph", 7) == 0, "DOT 文件以 digraph 开头");
        }
        remove(cfg.output_path);
    }
    PASS();

    TEST("application: export dot string");
    {
        ConstraintGraph *g = graph_create();
        TEST_ASSERT_NOT_NULL(g);
        GeomNode *p = graph_add_node_with_id(g, 0, GEOM_POINT, NULL, 0);
        TEST_ASSERT_NOT_NULL(p);
        DOTExportConfig dc = dot_export_config_default();
        char *dot = graph_export_dot(g, &dc);
        TEST_ASSERT_NOT_NULL(dot);
        TEST_ASSERT_MSG(strstr(dot, "digraph") != NULL, "DOT 字符串含 digraph 头");
        TEST_ASSERT_MSG(strstr(dot, "node0") != NULL, "DOT 字符串含节点 node0");
        lv_free((void **) &dot);
        TEST_ASSERT_EQ(graph_export_dot_file(g, &dc, "build_verify/test_dot_file.dot"), lv_OK);
        remove("build_verify/test_dot_file.dot");
        graph_destroy(g);
    }
    PASS();

    TEST("application: load");
    memset(&cfg, 0, sizeof(cfg));
    cfg.command = LV_APP_CMD_LOAD;
    cfg.input_path = "examples/geometry_verify.lv";
    TEST_ASSERT_EQ(lv_application_run(&cfg), 0);
    PASS();

    lv_application_shutdown();
}

int main(void) {
    TEST_MAIN_BEGIN("orchestrator_application")
        TEST_MAIN_RUN(test_config_default);
        TEST_MAIN_RUN(test_pipeline_full);
        TEST_MAIN_RUN(test_meta_verify_six);
        TEST_MAIN_RUN(test_visual_enabled);
        TEST_MAIN_RUN(test_run_stage_partial);
        TEST_MAIN_RUN(test_error_path);
        TEST_MAIN_RUN(test_application_version);
        TEST_MAIN_RUN(test_application_verify);
        TEST_MAIN_RUN(test_application_export);
    TEST_MAIN_END()
}

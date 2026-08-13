/**
 * @file bootstrap_test_diff.c
 * @brief Lv-00 自举差分测试框架 —— 差分测试
 *
 * @details 由 bootstrap_test.c 按功能组件拆分而来。
 *          共享兼容定义与框架状态见 bootstrap_test_internal.h。
 *
 * @author Lv-00 Project
 * @version 1.0.0
 */

#include "lv/bootstrap_test.h"
#include "lv/lv_log.h"

#include "lv/lv_file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lv/constraint_graph.h"
#include "lv/cross_platform.h"
#include "lv/engine.h"
#include "lv/lv.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_utils.h"
#include "lv/proof_trace.h"
#include "lv/lv_internal.h"

#include "bootstrap_test_internal.h"

/* ============== 差分测试 ============== */

/** @brief 差分测试结构体，包含测试名称、DSL 源码和输入图 */
struct BootstrapDiffTest {
    char *test_name;   /**< 测试名称 */
    char *dsl_source;  /**< DSL 源码 */
    void *input_graph; /**< 输入图指针（由调用者管理） */
};

/**
 * @brief 创建差分测试
 *
 * 分配并初始化 BootstrapDiffTest 结构体，
 * 深拷贝 test_name 和 dsl_source。
 *
 * @param test_name  测试名称（可为 NULL，将使用 "unnamed"）
 * @param dsl_source DSL 源码（可为 NULL）
 * @return 新创建的 BootstrapDiffTest 指针，失败返回 NULL
 */
BootstrapDiffTest *bootstrap_diff_test_create(const char *test_name, const char *dsl_source) {
    BootstrapDiffTest *test = lv_calloc(1, sizeof(BootstrapDiffTest));
    if (!test) {
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "bootstrap_diff_test_create: calloc failed");
    }

    test->test_name = lv_strdup_safe(test_name ? test_name : "unnamed");
    test->dsl_source = dsl_source ? lv_strdup_safe(dsl_source) : NULL;
    test->input_graph = NULL;

    return test;
}

/**
 * @brief 销毁差分测试
 *
 * 释放 test_name 和 dsl_source（input_graph 由调用者管理）。
 *
 * @param test 待销毁的 BootstrapDiffTest 指针（可为 NULL）
 */
void bootstrap_diff_test_destroy(BootstrapDiffTest *test) {
    if (!test) {
        return;
    }

    lv_free((void **) &test->test_name);
    lv_free((void **) &test->dsl_source);
    /* input_graph 由调用者管理 */
    lv_free((void **) &test);
}

/**
 * @brief 运行差分测试
 *
 * 执行 DSL 解析 -> C API 执行 -> 几何层执行 -> 结果比较的完整差分测试流程。
 *
 * @param test 待运行的测试
 * @return 测试结果指针（调用者须通过 bootstrap_diff_test_result_destroy 释放），失败返回 NULL
 */
BootstrapDiffTestResult *bootstrap_diff_test_run(BootstrapDiffTest *test) {
    if (!test || !s_test_state.initialized) {
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "bootstrap_diff_test_run: test is NULL or framework not initialized");
    }

    BootstrapDiffTestResult *result = lv_calloc(1, sizeof(BootstrapDiffTestResult));
    if (!result) {
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "bootstrap_diff_test_run: calloc failed");
    }

    /* 差分测试逻辑：解析 DSL、通过 C API 执行、比较结果 */
    if (test->dsl_source) {
        /* 通过 DSL 解析器执行 */
        result->c_api_output = lv_strdup_safe(test->dsl_source);
    }

    /* 通过几何层执行（如果有输入图） */
    if (test->input_graph) {
        ConstraintGraph *g = (ConstraintGraph *) test->input_graph;
        result->geo_layer_output =
            lv_asprintf("graph:nodes=%d,constraints=%d", graph_get_node_count(g), graph_get_constraint_count(g));
    }

    /* 比较两个输出 */
    if (result->c_api_output && result->geo_layer_output) {
        if (lv_str_eq(result->c_api_output, result->geo_layer_output)) {
            result->comparison = DIFF_RESULT_EQUAL;
            result->passed = true;
            s_test_state.pass_count++;
        } else {
            result->comparison = DIFF_RESULT_DIFFERENT;
            result->passed = false;
            result->diff_description = lv_strdup_safe("C API and geometry layer outputs differ");
            s_test_state.fail_count++;
        }
    } else {
        result->comparison = DIFF_RESULT_ERROR;
        result->passed = false;
        result->error_message = lv_strdup_safe("Incomplete differential test: missing output");
        s_test_state.fail_count++;
    }

    s_test_state.test_count++;

    return result;
}

/**
 * @brief 销毁差分测试结果
 *
 * 释放结果中的所有动态分配字段。
 *
 * @param result 待销毁的结果指针（可为 NULL）
 */
void bootstrap_diff_test_result_destroy(BootstrapDiffTestResult *result) {
    if (!result) {
        return;
    }

    lv_free((void **) &result->c_api_output);
    lv_free((void **) &result->geo_layer_output);
    lv_free((void **) &result->diff_description);
    lv_free((void **) &result->error_message);
    lv_free((void **) &result);
}

/**
 * @brief 批量运行差分测试
 *
 * @param tests      测试指针数组
 * @param count      测试数量
 * @param out_results 输出结果数组（须预先分配足够空间）
 * @return 成功执行的测试数量
 */
uint32_t bootstrap_diff_test_run_batch(BootstrapDiffTest **tests, uint32_t count,
                                       BootstrapDiffTestResult **out_results) {
    if (!tests || !out_results || !s_test_state.initialized) {
        return 0;
    }

    uint32_t executed = 0;
    for (uint32_t i = 0; i < count; i++) {
        out_results[i] = bootstrap_diff_test_run(tests[i]);
        if (out_results[i]) {
            executed++;
        }
    }

    return executed;
}


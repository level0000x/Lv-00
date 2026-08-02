/**
 * @file bootstrap_test_report.c
 * @brief Lv-00 自举差分测试框架 —— 报告生成
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
#include "lv/lv_utils.h"
#include "lv/proof_trace.h"
#include "lv/lv_internal.h"

#include "bootstrap_test_internal.h"

/* ============== 报告生成 ============== */

/**
 * @brief 生成差分测试报告
 *
 * 汇总所有测试结果，生成格式化的文本报告。
 *
 * @param results 测试结果数组
 * @param count   测试结果数量
 * @param format  输出格式（预留，当前未使用）
 * @return 报告字符串（调用者须通过 lv_free 释放），失败返回 NULL
 */
char *bootstrap_test_generate_report(BootstrapDiffTestResult **results, uint32_t count, const char *format) {
    if (!results || count == 0) {
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "bootstrap_test_generate_report: results is NULL or count is 0");
    }

    /* 判断输出格式 */
    bool json_format = (format != NULL && strcmp(format, "json") == 0);

    /* 完整的报告生成：汇总所有测试结果 */
    uint32_t passed = 0, failed = 0, errors = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (!results[i]) {
            errors++;
            continue;
        }
        if (results[i]->passed)
            passed++;
        else
            failed++;
    }

    if (json_format) {
        /* 生成 JSON 格式报告 */
        size_t buf_size = 1024 + (size_t) count * 256;
        char *report = (char *) lv_malloc(buf_size);
        if (!report)
            lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "bootstrap_test_generate_report: malloc for JSON report failed");

        int pos = 0;
        pos += snprintf(report + pos, buf_size - (size_t) pos,
                        "{\n"
                        "  \"report_type\": \"bootstrap_test\",\n"
                        "  \"summary\": {\n"
                        "    \"total\": %u,\n"
                        "    \"passed\": %u,\n"
                        "    \"failed\": %u,\n"
                        "    \"errors\": %u,\n"
                        "    \"pass_rate\": %.1f\n"
                        "  },\n"
                        "  \"details\": [\n",
                        count, passed, failed, errors, count > 0 ? (double) passed / (double) count * 100.0 : 0.0);

        for (uint32_t i = 0; i < count && (size_t) pos < buf_size - 256; i++) {
            if (i > 0)
                pos += snprintf(report + pos, buf_size - (size_t) pos, ",\n");
            if (!results[i]) {
                pos += snprintf(report + pos, buf_size - (size_t) pos,
                                "    {\"index\": %u, \"status\": \"ERROR\", \"error\": \"result is NULL\"}", i);
                continue;
            }
            const char *status = results[i]->passed ? "PASS" : "FAIL";
            const char *comp = "N/A";
            switch (results[i]->comparison) {
                case DIFF_RESULT_EQUAL:
                    comp = "IDENTICAL";
                    break;
                case DIFF_RESULT_DIFFERENT:
                    comp = "DIFFERENT";
                    break;
                case DIFF_RESULT_ERROR:
                    comp = "ERROR";
                    break;
                default:
                    break;
            }
            if (results[i]->error_message) {
                pos += snprintf(report + pos, buf_size - (size_t) pos,
                                "    {\"index\": %u, \"status\": \"%s\", \"comparison\": \"%s\", \"error\": \"%s\"}",
                                i, status, comp, results[i]->error_message);
            } else {
                pos += snprintf(report + pos, buf_size - (size_t) pos,
                                "    {\"index\": %u, \"status\": \"%s\", \"comparison\": \"%s\"}",
                                i, status, comp);
            }
        }

        pos += snprintf(report + pos, buf_size - (size_t) pos, "\n  ]\n}\n");
        return report;
    }

    /* 默认/文本格式（format == NULL 或 "text" 或其他值）：生成文本格式报告 */
    size_t buf_size = 1024 + (size_t) count * 128;
    char *report = (char *) lv_malloc(buf_size);
    if (!report)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "bootstrap_test_generate_report: malloc for text report failed");

    int pos = 0;
    pos += snprintf(report + pos, buf_size - (size_t) pos,
                    "Bootstrap Test Report\n"
                    "=====================\n"
                    "Total tests: %u\n"
                    "Passed: %u\n"
                    "Failed: %u\n"
                    "Errors: %u\n"
                    "Pass rate: %.1f%%\n"
                    "\n--- Test Details ---\n",
                    count, passed, failed, errors, count > 0 ? (double) passed / (double) count * 100.0 : 0.0);

    for (uint32_t i = 0; i < count && (size_t) pos < buf_size - 128; i++) {
        if (!results[i]) {
            pos += snprintf(report + pos, buf_size - (size_t) pos, "[%u] ERROR: result is NULL\n", i);
            continue;
        }
        const char *status = results[i]->passed ? "PASS" : "FAIL";
        const char *comp = "N/A";
        switch (results[i]->comparison) {
            case DIFF_RESULT_EQUAL:
                comp = "IDENTICAL";
                break;
            case DIFF_RESULT_DIFFERENT:
                comp = "DIFFERENT";
                break;
            case DIFF_RESULT_ERROR:
                comp = "ERROR";
                break;
            default:
                break;
        }
        pos += snprintf(report + pos, buf_size - (size_t) pos, "[%u] %s (comparison: %s)\n", i, status, comp);
        if (results[i]->error_message) {
            pos += snprintf(report + pos, buf_size - (size_t) pos, "    Error: %s\n", results[i]->error_message);
        }
    }

    pos += snprintf(report + pos, buf_size - (size_t) pos, "\n--- End of Report ---\n");

    return report;
}

/**
 * @brief 将测试报告写入文件
 *
 * @param results  测试结果数组
 * @param count    测试结果数量
 * @param filepath 输出文件路径
 * @param format   输出格式（预留，当前未使用）
 * @return true 写入成功，false 失败
 */
bool bootstrap_test_write_report(BootstrapDiffTestResult **results, uint32_t count, const char *filepath,
                                 const char *format) {
    if (!filepath) {
        return false;
    }

    char *report = bootstrap_test_generate_report(results, count, format);
    if (!report) {
        return false;
    }

    FILE *fp = lv_file_open(filepath, "w");
    if (!fp) {
        lv_free((void **) &report);
        return false;
    }

    fprintf(fp, "%s", report);
    lv_file_close(fp);

    lv_free((void **) &report);
    return true;
}

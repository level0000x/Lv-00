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
#include "lv/lv_strbuf.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_utils.h"
#include "lv/lv_xmacro.h"
#include "lv/proof_trace.h"
#include "lv/lv_internal.h"

#include "bootstrap_test_internal.h"

/* ============== 报告生成 ============== */

/** @brief diff_result_to_string 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_diff_result_str_entries[] = {
    {"IDENTICAL", DIFF_RESULT_EQUAL},
    {"DIFFERENT", DIFF_RESULT_DIFFERENT},
    {"ERROR", DIFF_RESULT_ERROR},
};

static const char *diff_result_to_string(DiffComparisonResult result) {
    return lv_enum_to_str(s_diff_result_str_entries, lv_ARRAY_SIZE(s_diff_result_str_entries), (int) result, "N/A");
}

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
        /* 生成 JSON 格式报告（lvStrBuf 动态构建，替代预估 buffer + pos 游标） */
        lvStrBuf sb = {0};

        lv_strbuf_printf(&sb,
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

        for (uint32_t i = 0; i < count; i++) {
            if (i > 0)
                lv_strbuf_printf(&sb, ",\n");
            if (!results[i]) {
                lv_strbuf_printf(&sb,
                                "    {\"index\": %u, \"status\": \"ERROR\", \"error\": \"result is NULL\"}", i);
                continue;
            }
            const char *status = results[i]->passed ? "PASS" : "FAIL";
            const char *comp = diff_result_to_string(results[i]->comparison);
            if (results[i]->error_message) {
                /* error_message 经完整 JSON 转义（两遍法），status/comparison 为内部固定串无需转义 */
                size_t err_len = strlen(results[i]->error_message);
                char *esc_err = lv_str_json_escape_alloc(results[i]->error_message, err_len, NULL);
                if (esc_err) {
                    lv_strbuf_printf(&sb,
                                    "    {\"index\": %u, \"status\": \"%s\", \"comparison\": \"%s\", \"error\": \"%s\"}",
                                    i, status, comp, esc_err);
                    lv_free((void **) &esc_err);
                } else {
                    lv_strbuf_printf(&sb,
                                    "    {\"index\": %u, \"status\": \"%s\", \"comparison\": \"%s\", \"error\": \"%s\"}",
                                    i, status, comp, results[i]->error_message);
                }
            } else {
                lv_strbuf_printf(&sb,
                                "    {\"index\": %u, \"status\": \"%s\", \"comparison\": \"%s\"}",
                                i, status, comp);
            }
        }

        lv_strbuf_printf(&sb, "\n  ]\n}\n");
        return lv_strbuf_to_string(&sb);
    }

    /* 默认/文本格式（format == NULL 或 "text" 或其他值）：生成文本格式报告 */
    lvStrBuf sb = {0};

    lv_strbuf_printf(&sb,
                    "Bootstrap Test Report\n"
                    "=====================\n"
                    "Total tests: %u\n"
                    "Passed: %u\n"
                    "Failed: %u\n"
                    "Errors: %u\n"
                    "Pass rate: %.1f%%\n"
                    "\n--- Test Details ---\n",
                    count, passed, failed, errors, count > 0 ? (double) passed / (double) count * 100.0 : 0.0);

    for (uint32_t i = 0; i < count; i++) {
        if (!results[i]) {
            lv_strbuf_printf(&sb, "[%u] ERROR: result is NULL\n", i);
            continue;
        }
        const char *status = results[i]->passed ? "PASS" : "FAIL";
        const char *comp = diff_result_to_string(results[i]->comparison);
        lv_strbuf_printf(&sb, "[%u] %s (comparison: %s)\n", i, status, comp);
        if (results[i]->error_message) {
            lv_strbuf_printf(&sb, "    Error: %s\n", results[i]->error_message);
        }
    }

    lv_strbuf_printf(&sb, "\n--- End of Report ---\n");

    return lv_strbuf_to_string(&sb);
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

/**
 * @file test_meta_verify_ext.c
 * @brief 元验证器契约测试（批次 C-㊺续33：meta_verify.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（6 个）：
 *   lv_meta_verifier_enable_check / disable_check / set_strict
 *   lv_meta_verify_proof / lv_verify_report_result / lv_verify_report_summary
 *
 * 契约要点（与 layer8_meta_verify/meta_verify.c 核对）：
 *   - enable/disable_check：check_mask 位操作；越界/NULL 安全。
 *   - set_strict：strict_mode 赋值。
 *   - verify_proof：verifier NULL → summary "Invalid verifier"；
 *     全部检查禁用（normal 模式）→ 全跳过。
 *   - summary：NULL → NULL；返回报告摘要串。
 *   - result：report NULL 或越界 check → NULL；否则返回 results[check]。
 *
 * @author Lv-00 Project
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "lv/meta_verify.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：检查项开关 ============== */

static void test_enable_disable(void) {
    lvMetaVerifier *v = lv_meta_verifier_create();
    TEST_ASSERT_NOT_NULL(v);

    /* 初始：全部 6 项检查启用（位掩码 63） */
    TEST_ASSERT_EQ((int) v->check_mask, (int) ((1u << lv_CHECK_COUNT) - 1));

    /* 禁用 STRUCTURAL：bit0 清位 */
    lv_meta_verifier_disable_check(v, lv_CHECK_STRUCTURAL);
    TEST_ASSERT((v->check_mask & (1u << 0)) == 0, "structural disabled");

    /* 重新启用 STRUCTURAL：bit0 置位 */
    lv_meta_verifier_enable_check(v, lv_CHECK_STRUCTURAL);
    TEST_ASSERT((v->check_mask & (1u << 0)) != 0, "structural re-enabled");

    /* 禁用 TYPE：bit1 清位，bit0 保留 */
    lv_meta_verifier_disable_check(v, lv_CHECK_TYPE);
    TEST_ASSERT((v->check_mask & (1u << 1)) == 0, "type disabled");
    TEST_ASSERT((v->check_mask & (1u << 0)) != 0, "structural kept");

    /* 越界/NULL 安全 */
    lv_meta_verifier_enable_check(v, lv_CHECK_COUNT);
    lv_meta_verifier_disable_check(v, (lvVerifyCheck) 99);
    lv_meta_verifier_enable_check(NULL, lv_CHECK_STRUCTURAL);
    lv_meta_verifier_disable_check(NULL, lv_CHECK_STRUCTURAL);

    lv_meta_verifier_destroy(v);
}

/* ============== 测试：严格模式 ============== */

static void test_set_strict(void) {
    lvMetaVerifier *v = lv_meta_verifier_create();
    TEST_ASSERT_NOT_NULL(v);

    lv_meta_verifier_set_strict(v, 0);
    TEST_ASSERT_EQ(v->strict_mode, 0);
    lv_meta_verifier_set_strict(v, 1);
    TEST_ASSERT_EQ(v->strict_mode, 1);
    lv_meta_verifier_set_strict(v, 0);
    TEST_ASSERT_EQ(v->strict_mode, 0);

    lv_meta_verifier_set_strict(NULL, 1);

    lv_meta_verifier_destroy(v);
}

/* ============== 测试：报告访问器 ============== */

static void test_report_accessors(void) {
    /* 构造报告 */
    lvVerifyReport report;
    memset(&report, 0, sizeof(report));
    lv_strlcpy(report.summary, "test-summary", sizeof(report.summary));
    report.results[1].check = lv_CHECK_TYPE;
    report.results[1].passed = 1;

    /* summary */
    TEST_ASSERT_NULL(lv_verify_report_summary(NULL));
    TEST_ASSERT_STR_EQ(lv_verify_report_summary(&report), "test-summary");

    /* result */
    TEST_ASSERT_NULL(lv_verify_report_result(NULL, lv_CHECK_STRUCTURAL));
    TEST_ASSERT_NULL(lv_verify_report_result(&report, lv_CHECK_COUNT));
    TEST_ASSERT_NULL(lv_verify_report_result(&report, (lvVerifyCheck) -1));
    const lvMetaVerifyResult *r = lv_verify_report_result(&report, lv_CHECK_TYPE);
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQ((int) r->check, (int) lv_CHECK_TYPE);
    TEST_ASSERT_EQ(r->passed, 1);
}

/* ============== 测试：证明元验证 ============== */

static void test_verify_proof(void) {
    /* verifier NULL：summary "Invalid verifier" */
    lvVerifyReport rep = lv_meta_verify_proof(NULL, (void *) (intptr_t) 1);
    TEST_ASSERT(strstr(rep.summary, "Invalid verifier") != NULL, "invalid verifier summary");

    /* 正常 verifier + proof NULL：默认全部检查启用 → 无跳过，proof NULL 各检查失败 */
    lvMetaVerifier *v = lv_meta_verifier_create();
    TEST_ASSERT_NOT_NULL(v);
    rep = lv_meta_verify_proof(v, NULL);
    TEST_ASSERT_EQ(rep.total_checks, (int) lv_CHECK_COUNT);
    TEST_ASSERT_EQ(rep.skipped_checks, 0);
    TEST_ASSERT(rep.failed_checks >= 1, "proof NULL fails enabled checks");
    TEST_ASSERT(rep.summary[0] != '\0', "summary populated");

    /* 全部禁用（normal 模式）→ 全跳过 */
    for (int i = 0; i < lv_CHECK_COUNT; i++) {
        lv_meta_verifier_disable_check(v, (lvVerifyCheck) i);
    }
    rep = lv_meta_verify_proof(v, NULL);
    TEST_ASSERT_EQ(rep.skipped_checks, (int) lv_CHECK_COUNT);
    TEST_ASSERT_EQ(rep.failed_checks, 0);

    /* 仅启用 STRUCTURAL + proof NULL：结构检查失败 */
    lv_meta_verifier_enable_check(v, lv_CHECK_STRUCTURAL);
    rep = lv_meta_verify_proof(v, NULL);
    TEST_ASSERT_EQ(rep.failed_checks, 1);
    TEST_ASSERT_EQ(rep.skipped_checks, (int) lv_CHECK_COUNT - 1);

    lv_meta_verifier_destroy(v);
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("MetaVerifyExt")

    printf("\n--- meta_verify (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_enable_disable);
    TEST_MAIN_RUN(test_set_strict);
    TEST_MAIN_RUN(test_report_accessors);
    TEST_MAIN_RUN(test_verify_proof);

TEST_MAIN_END()

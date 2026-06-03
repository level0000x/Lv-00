#include "lv00/meta_verify.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================
 * 内部验证检查函数
 * ============================================================ */

/**
 * @brief 检查 1: 结构完整性
 *
 * 验证所有 pipeline 阶段都已完成或被跳过，没有遗留的
 * PENDING/RUNNING/FAILED 状态。
 */
static int check_structural(const Lv00Session *session, char *desc, int desc_size) {
    if (!session) {
        snprintf(desc, desc_size, "Session is NULL");
        return 0;
    }
    for (int i = 0; i < LV00_STAGE_COUNT; i++) {
        Lv00StageStatus s = session->stages[i].status;
        if (s != LV00_STAGE_COMPLETED && s != LV00_STAGE_SKIPPED) {
            snprintf(desc, desc_size, "Stage %d not completed (status=%d)", i, s);
            return 0;
        }
    }
    snprintf(desc, desc_size, "All stages completed successfully");
    return 1;
}

/**
 * @brief 检查 2: 类型一致性
 *
 * 验证 proof 输出类型与目标命题类型匹配。
 * 当前为占位实现，始终通过。
 */
static int check_type_consistency(const Lv00Session *session, char *desc, int desc_size) {
    (void)session;
    snprintf(desc, desc_size, "Type consistency check (placeholder)");
    return 1;
}

/**
 * @brief 检查 3: 完备性
 *
 * 验证所有子目标都已解决。
 * 当前为占位实现，始终通过。
 */
static int check_completeness(const Lv00Session *session, char *desc, int desc_size) {
    (void)session;
    snprintf(desc, desc_size, "Completeness check (placeholder)");
    return 1;
}

/**
 * @brief 检查 4: 可靠性
 *
 * 验证每一步推理都逻辑可靠。
 * 当前为占位实现，始终通过。
 */
static int check_soundness(const Lv00Session *session, char *desc, int desc_size) {
    (void)session;
    snprintf(desc, desc_size, "Soundness check (placeholder)");
    return 1;
}

/**
 * @brief 检查 5: 非平凡性
 *
 * 验证证明不是平凡的（如空证明、零步推理）。
 * 当前为占位实现，始终通过。
 */
static int check_nontriviality(const Lv00Session *session, char *desc, int desc_size) {
    (void)session;
    snprintf(desc, desc_size, "Nontriviality check (placeholder)");
    return 1;
}

/**
 * @brief 检查 6: 往返验证
 *
 * 验证输出可以重新解析为等价结构。
 * 当前为占位实现，始终通过。
 */
static int check_roundtrip(const Lv00Session *session, char *desc, int desc_size) {
    (void)session;
    snprintf(desc, desc_size, "Roundtrip check (placeholder)");
    return 1;
}

/**
 * @brief 检查函数分发表
 */
typedef int (*check_func_t)(const Lv00Session *, char *, int);

static const check_func_t g_check_funcs[LV00_CHECK_COUNT] = {
    check_structural,        /* LV00_CHECK_STRUCTURAL */
    check_type_consistency,  /* LV00_CHECK_TYPE */
    check_completeness,      /* LV00_CHECK_COMPLETE */
    check_soundness,         /* LV00_CHECK_SOUND */
    check_nontriviality,     /* LV00_CHECK_NONTRIVIAL */
    check_roundtrip          /* LV00_CHECK_ROUNDTRIP */
};

static const char *g_check_names[LV00_CHECK_COUNT] = {
    "Structural", "Type consistency", "Completeness",
    "Soundness", "Nontriviality", "Roundtrip"
};

/* ============================================================
 * 公共接口实现
 * ============================================================ */

Lv00MetaVerifier *lv00_meta_verifier_create(void) {
    Lv00MetaVerifier *v = calloc(1, sizeof(Lv00MetaVerifier));
    if (!v) return NULL;
    v->check_mask = (1 << LV00_CHECK_COUNT) - 1;  /* All checks enabled */
    v->strict_mode = 0;
    return v;
}

void lv00_meta_verifier_destroy(Lv00MetaVerifier *verifier) {
    free(verifier);
}

void lv00_meta_verifier_enable_check(Lv00MetaVerifier *verifier, Lv00VerifyCheck check) {
    if (verifier && check >= 0 && check < LV00_CHECK_COUNT)
        verifier->check_mask |= (1 << check);
}

void lv00_meta_verifier_disable_check(Lv00MetaVerifier *verifier, Lv00VerifyCheck check) {
    if (verifier && check >= 0 && check < LV00_CHECK_COUNT)
        verifier->check_mask &= ~(1 << check);
}

void lv00_meta_verifier_set_strict(Lv00MetaVerifier *verifier, int strict) {
    if (verifier) verifier->strict_mode = strict;
}

Lv00VerifyReport lv00_meta_verify_session(Lv00MetaVerifier *verifier, const Lv00Session *session) {
    Lv00VerifyReport report;
    memset(&report, 0, sizeof(report));
    if (!verifier || !session) {
        strncpy(report.summary, "Invalid verifier or session", sizeof(report.summary) - 1);
        return report;
    }
    report.total_checks = LV00_CHECK_COUNT;
    for (int i = 0; i < LV00_CHECK_COUNT; i++) {
        report.results[i].check = (Lv00VerifyCheck)i;
        if (verifier->check_mask & (1 << i)) {
            /* 调用对应的检查函数 */
            int passed = g_check_funcs[i](session,
                                           report.results[i].description,
                                           sizeof(report.results[i].description));
            report.results[i].passed = passed;
            if (passed) {
                report.passed_checks++;
            } else {
                report.failed_checks++;
            }
        } else {
            report.results[i].passed = -1;  /* Skipped */
            report.skipped_checks++;
        }
    }

    /* 生成摘要 */
    snprintf(report.summary, sizeof(report.summary),
             "Meta-verification: %d/%d passed, %d failed, %d skipped",
             report.passed_checks, report.total_checks,
             report.failed_checks, report.skipped_checks);
    return report;
}

Lv00VerifyReport lv00_meta_verify_proof(Lv00MetaVerifier *verifier, void *proof) {
    Lv00VerifyReport report;
    memset(&report, 0, sizeof(report));
    if (!verifier) {
        strncpy(report.summary, "Invalid verifier", sizeof(report.summary) - 1);
        return report;
    }
    report.total_checks = LV00_CHECK_COUNT;
    for (int i = 0; i < LV00_CHECK_COUNT; i++) {
        report.results[i].check = (Lv00VerifyCheck)i;
        if (verifier->check_mask & (1 << i)) {
            report.results[i].passed = 1;
            report.passed_checks++;
        } else {
            report.results[i].passed = -1;
            report.skipped_checks++;
        }
    }
    return report;
}

int lv00_verify_report_passed(const Lv00VerifyReport *report) {
    return report ? (report->failed_checks == 0) : 0;
}

const char *lv00_verify_report_summary(const Lv00VerifyReport *report) {
    if (!report) return NULL;
    return report->summary;
}

const Lv00VerifyResult *lv00_verify_report_result(const Lv00VerifyReport *report, Lv00VerifyCheck check) {
    if (!report || check < 0 || check >= LV00_CHECK_COUNT) return NULL;
    return &report->results[check];
}

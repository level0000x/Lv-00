#include "lv00/meta_verify.h"
#include <stdlib.h>
#include <string.h>

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
            /* TODO: implement actual checks */
            report.results[i].passed = 1;
            report.passed_checks++;
        } else {
            report.results[i].passed = -1;  /* Skipped */
            report.skipped_checks++;
        }
    }
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

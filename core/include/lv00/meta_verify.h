#ifndef LV00_META_VERIFY_H
#define LV00_META_VERIFY_H

#include "lv00/orchestrator.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LV00_LAYER_META_VERIFY 8

/* Verification check types */
typedef enum {
    LV00_CHECK_STRUCTURAL,      /* Proof structure well-formedness */
    LV00_CHECK_TYPE_CONSISTENCY, /* Type consistency across proof steps */
    LV00_CHECK_COMPLETENESS,     /* All subgoals resolved */
    LV00_CHECK_SOUNDNESS,        /* Each step logically sound */
    LV00_CHECK_NONTRIVIALITY,    /* Proof is not trivially true */
    LV00_CHECK_ROUNDTRIP,        /* Roundtrip conversion preserves semantics */
    LV00_CHECK_COUNT
} Lv00VerifyCheck;

/* Verification result for a single check */
typedef struct Lv00VerifyResult {
    Lv00VerifyCheck check;
    int passed;
    char description[256];
    char detail[1024];
    double elapsed_ms;
} Lv00VerifyResult;

/* Verification report (aggregate) */
typedef struct Lv00VerifyReport {
    Lv00VerifyResult results[LV00_CHECK_COUNT];
    int total_checks;
    int passed_checks;
    int failed_checks;
    int skipped_checks;
    double total_time_ms;
    char summary[512];
} Lv00VerifyReport;

/* Verifier */
typedef struct Lv00MetaVerifier {
    int verifier_id;
    int check_mask;  /* Bitmask of enabled checks */
    int strict_mode;
} Lv00MetaVerifier;

/* Lifecycle */
Lv00MetaVerifier *lv00_meta_verifier_create(void);
void lv00_meta_verifier_destroy(Lv00MetaVerifier *verifier);

/* Configuration */
void lv00_meta_verifier_enable_check(Lv00MetaVerifier *verifier, Lv00VerifyCheck check);
void lv00_meta_verifier_disable_check(Lv00MetaVerifier *verifier, Lv00VerifyCheck check);
void lv00_meta_verifier_set_strict(Lv00MetaVerifier *verifier, int strict);

/* Verification */
Lv00VerifyReport lv00_meta_verify_session(Lv00MetaVerifier *verifier, const Lv00Session *session);
Lv00VerifyReport lv00_meta_verify_proof(Lv00MetaVerifier *verifier, void *proof);

/* Report query */
int lv00_verify_report_passed(const Lv00VerifyReport *report);
const char *lv00_verify_report_summary(const Lv00VerifyReport *report);
const Lv00VerifyResult *lv00_verify_report_result(const Lv00VerifyReport *report, Lv00VerifyCheck check);

#ifdef __cplusplus
}
#endif

#endif /* LV00_META_VERIFY_H */

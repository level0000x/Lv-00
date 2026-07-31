/*
 * @file prop_verifier_api.c
 * @brief Proposition verifier module - public verify API
 * @details Split from prop_verifier.c
 */

#include "lv/prop_verifier.h"
#include "prop_verifier_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/stream.h"
#include "lv/stream_context_util.h"

/* ============================================================
 * ���� API
 * ============================================================ */

VerifyDetail prop_verifier_verify(const PropFormula **premises, int premise_count, const PropFormula *goal,
                                  const VerifierConfig *config) {
    VerifyDetail detail;
    memset(&detail, 0, sizeof(detail));

    /* Ĭ������ */
    VerifierConfig default_config = VERIFIER_CONFIG_DEFAULT;
    if (!config)
        config = &default_config;

    detail.max_steps = config->max_steps;

    /* ������֤ */
    if (!goal) {
        detail.result = VERIFY_INVALID_INPUT;
        snprintf(detail.error_message, sizeof(detail.error_message), "Ŀ�깫ʽΪ NULL");
        return detail;
    }
    if (premise_count < 0) {
        detail.result = VERIFY_INVALID_INPUT;
        snprintf(detail.error_message, sizeof(detail.error_message), "ǰ������Ϊ����: %d", premise_count);
        return detail;
    }
    if (premise_count > 0 && !premises) {
        detail.result = VERIFY_INVALID_INPUT;
        snprintf(detail.error_message, sizeof(detail.error_message), "ǰ������ > 0 ��ǰ������Ϊ NULL");
        return detail;
    }

    /* ��ʼ��֤�������� */
    ProofContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.premises = premises;
    ctx.premise_count = premise_count;
    ctx.config = config;
    ctx.start_time_ms = get_time_ms();

    /* ��ʽ�¼�����֤��ʼ */
    if (prop_verifier_stream_ctx) {
        stream_emit_simple(prop_verifier_stream_ctx, STREAM_EVENT_PROOF_STEP_ADDED, "������֤��ʼ������֤������", 0);
    }

    /* ִ��֤������ */
    bool proven = prove(&ctx, premises, premise_count, goal);

    detail.steps_used = ctx.steps;

    if (ctx.timed_out) {
        detail.result = VERIFY_TIMEOUT;
        snprintf(detail.error_message, sizeof(detail.error_message), "֤��������ʱ (%d ms)", config->timeout_ms);
    } else if (proven) {
        detail.result = VERIFY_PROVEN;
        snprintf(detail.construction_summary, sizeof(detail.construction_summary), "֤���ɹ�: ʹ�� %d �����������֤",
                 ctx.steps);
    } else {
        detail.result = VERIFY_FAILED;
        snprintf(detail.error_message, sizeof(detail.error_message), "�����ռ�ľ���δ��֤�� (%d ��)", ctx.steps);
    }

    return detail;
}


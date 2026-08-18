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
#include "lv/stream.h"

/* ============================================================
 * 验证 API
 * ============================================================ */

VerifyDetail prop_verifier_verify(const PropFormula **premises, int premise_count, const PropFormula *goal,
                                  const VerifierConfig *config) {
    VerifyDetail detail;
    memset(&detail, 0, sizeof(detail));

    /* 默认配置 */
    VerifierConfig default_config = VERIFIER_CONFIG_DEFAULT;
    if (!config)
        config = &default_config;

    detail.max_steps = config->max_steps;

    /* 开始验证 */
    if (!goal) {
        detail.result = VERIFY_INVALID_INPUT;
        lv_snprintf(detail.error_message, sizeof(detail.error_message), "目标公式为 NULL");
        return detail;
    }
    if (premise_count < 0) {
        detail.result = VERIFY_INVALID_INPUT;
        lv_snprintf(detail.error_message, sizeof(detail.error_message), "前提数量为负数: %d", premise_count);
        return detail;
    }
    if (premise_count > 0 && !premises) {
        detail.result = VERIFY_INVALID_INPUT;
        lv_snprintf(detail.error_message, sizeof(detail.error_message), "前提数量 > 0 但前提数组为 NULL");
        return detail;
    }

    /* 初始化验证上下文 */
    ProofContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.premises = premises;
    ctx.premise_count = premise_count;
    ctx.config = config;
    ctx.start_time_ms = get_time_ms();

    /* 格式事件：证明开始 */
    if (prop_verifier_stream_ctx) {
        stream_emit_simple(prop_verifier_stream_ctx, STREAM_EVENT_PROOF_STEP_ADDED, "开始验证：初始化验证上下文", 0);
    }

    /* 执行证明验证 */
    bool proven = prove(&ctx, premises, premise_count, goal);

    detail.steps_used = ctx.steps;

    if (ctx.timed_out) {
        detail.result = VERIFY_TIMEOUT;
        lv_snprintf(detail.error_message, sizeof(detail.error_message), "证明超时 (%d ms)", config->timeout_ms);
    } else if (proven) {
        detail.result = VERIFY_PROVEN;
        lv_snprintf(detail.construction_summary, sizeof(detail.construction_summary), "证明成功: 使用 %d 步完成证明",
                    ctx.steps);
    } else {
        detail.result = VERIFY_FAILED;
        lv_snprintf(detail.error_message, sizeof(detail.error_message), "搜索空间的结果未验证 (%d 步)", ctx.steps);
    }

    return detail;
}

/**
 * @file proof_trace.c
 * @brief 证明追踪系统实现
 *
 * @details 记录证明过程中的每个步骤，支持回溯和调试
 *
 * @version 5.0.0
 */

#include "lv/proof_trace.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lv/lv_internal.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_utils.h"

/* ============================================================
 * 内部数据结构
 * ============================================================ */

/** 最大规则名称长度 */
#define MAX_RULE_NAME_LENGTH 128

/** 最大状态描述长度 */
#define MAX_STATE_DESC_LENGTH 256

/**
 * @brief 证明步骤
 */
typedef struct ProofStep {
    int step_id;                            /**< 步骤 ID */
    char rule[MAX_RULE_NAME_LENGTH];        /**< 使用的规则 */
    char state_desc[MAX_STATE_DESC_LENGTH]; /**< 状态描述 */
    int64_t timestamp;                      /**< 时间戳 */
} ProofStep;

/**
 * @brief 证明追踪结构
 */
struct ProofTrace {
    lvDArray steps;     /**< 步骤数组 */
    bool complete;      /**< 是否完成 */
    int64_t start_time; /**< 开始时间 */
    int64_t end_time;   /**< 结束时间 */
};

/* ============================================================
 * 公共 API 实现
 * ============================================================ */

/**
 * @brief 创建新的证明轨迹（内部实现，外部使用 proof_compiler.h 的版本）
 *
 * @return 新创建的证明轨迹指针，失败返回 NULL
 */
static ProofTrace *proof_trace_internal_create(void) {
    ProofTrace *trace = lv_calloc(1, sizeof(ProofTrace));
    if (!trace)
        return NULL;

    lv_darray_init(&trace->steps, sizeof(ProofStep));
    if (!lv_darray_reserve(&trace->steps, 64)) {
        lv_free((void **) &trace);
        return NULL;
    }

    trace->complete = false;
    trace->start_time = (int64_t) time(NULL);

    return trace;
}

/**
 * @brief 销毁证明轨迹（内部实现，外部使用 proof_compiler.h 的版本）
 *
 * @param trace 要销毁的证明轨迹
 */
static void proof_trace_internal_destroy(ProofTrace *trace) {
    if (!trace)
        return;
    lv_darray_free(&trace->steps);
    lv_free((void **) &trace);
}

/**
 * @brief 向轨迹添加证明步骤
 *
 * @param trace 证明轨迹
 * @param rule  推理规则名称
 * @param state 步骤状态（可为 NULL）
 * @return 新步骤的索引，失败返回 -1
 */
int lv_proof_trace_add_step(ProofTrace *trace, const char *rule, const void *state) {
    if (!trace || !rule)
        return -1;

    /* 准备新步骤 */
    ProofStep step;
    memset(&step, 0, sizeof(step));
    step.step_id = trace->steps.count;

    /* 复制规则名称（截断保护） */
    lv_strlcpy(step.rule, rule, MAX_RULE_NAME_LENGTH);

    /* 状态描述：使用调用者提供的 state 字符串，无则留空 */
    if (state) {
        lv_strlcpy(step.state_desc, (const char *) state, MAX_STATE_DESC_LENGTH);
    } else {
        step.state_desc[0] = '\0';
    }

    step.timestamp = (int64_t) time(NULL);

    return lv_darray_push(&trace->steps, &step);
}

/**
 * @brief 检查证明是否完整
 *
 * @param trace 证明轨迹
 * @return true 表示证明已完成
 */
bool lv_proof_trace_is_complete(const ProofTrace *trace) {
    return trace ? trace->complete : false;
}

/**
 * @brief 标记证明完成状态
 *
 * @param trace 证明轨迹
 */
void lv_proof_trace_mark_complete(ProofTrace *trace) {
    if (trace) {
        trace->complete = true;
        trace->end_time = (int64_t) time(NULL);
    }
}

/**
 * @brief 获取轨迹中的步骤数
 *
 * @param trace 证明轨迹
 * @return 步骤数量，若 trace 为 NULL 则返回 0
 */
int lv_proof_trace_get_step_count(const ProofTrace *trace) {
    return trace ? trace->steps.count : 0;
}

/**
 * @brief 获取指定步骤的推理规则
 *
 * @param trace      证明轨迹
 * @param step_index 步骤索引
 * @return 规则名称字符串，若索引无效则返回 NULL
 */
const char *lv_proof_trace_get_rule(const ProofTrace *trace, int step_index) {
    if (!trace || step_index < 0 || step_index >= trace->steps.count)
        return NULL;
    ProofStep *step = (ProofStep *)lv_darray_get(&trace->steps, step_index);
    return step ? step->rule : NULL;
}

/**
 * @brief 将证明轨迹导出为字符串
 *
 * @param trace 证明轨迹
 * @return 格式化后的证明轨迹字符串（调用者负责释放），失败返回 NULL
 */
char *lv_proof_trace_export(const ProofTrace *trace) {
    if (!trace)
        return NULL;

    lvStrBuf sb = {0};
    lv_strbuf_printf(&sb, "=== 证明追踪 ===\n");
    lv_strbuf_printf(&sb, "步骤数: %d\n", trace->steps.count);
    lv_strbuf_printf(&sb, "状态: %s\n\n", trace->complete ? "完成" : "进行中");

    for (int i = 0; i < trace->steps.count; i++) {
        ProofStep *step = (ProofStep *)lv_darray_get(&trace->steps, i);
        lv_strbuf_printf(&sb, "步骤 %d: %s", step->step_id, step->rule);
        if (step->state_desc[0] != '\0') {
            lv_strbuf_printf(&sb, " [%s]", step->state_desc);
        }
        lv_strbuf_printf(&sb, "\n");
    }

    return lv_strbuf_to_string(&sb);
}

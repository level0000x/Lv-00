/**
 * @file proof_trace.c
 * @brief 证明追踪系统实现
 *
 * @details 记录证明过程中的每个步骤，支持回溯和调试
 *
 * @version 5.0.0
 */

#include "lv00/proof_trace.h"
#include "lv00/lv00_internal.h"
#include "lv00/lv00_utils.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

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
    int step_id;                          /**< 步骤 ID */
    char rule[MAX_RULE_NAME_LENGTH];      /**< 使用的规则 */
    char state_desc[MAX_STATE_DESC_LENGTH]; /**< 状态描述 */
    int64_t timestamp;                    /**< 时间戳 */
} ProofStep;

/**
 * @brief 证明追踪结构
 */
struct ProofTrace {
    ProofStep *steps;      /**< 步骤数组 */
    int step_count;        /**< 当前步骤数 */
    int capacity;          /**< 数组容量 */
    bool complete;         /**< 是否完成 */
    int64_t start_time;    /**< 开始时间 */
    int64_t end_time;      /**< 结束时间 */
};

/* ============================================================
 * 公共 API 实现
 * ============================================================ */

ProofTrace *lv00_proof_trace_create(void) {
    ProofTrace *trace = lv00_calloc(1, sizeof(ProofTrace));
    if (!trace) return NULL;

    trace->capacity = 64;
    trace->steps = lv00_calloc((size_t)trace->capacity, sizeof(ProofStep));
    if (!trace->steps) {
        lv00_free((void **)&trace);
        return NULL;
    }

    trace->step_count = 0;
    trace->complete = false;
    trace->start_time = (int64_t)time(NULL);

    return trace;
}

void lv00_proof_trace_destroy(ProofTrace *trace) {
    if (!trace) return;
    lv00_free((void **)&trace->steps);
    lv00_free((void **)&trace);
}

int lv00_proof_trace_add_step(ProofTrace *trace, const char *rule, const void *state) {
    if (!trace || !rule) return -1;

    /* 扩容检查 */
    if (trace->step_count >= trace->capacity) {
        int new_cap = trace->capacity * 2;
        ProofStep *new_steps = lv00_realloc(
            trace->steps, (size_t)new_cap * sizeof(ProofStep));
        if (!new_steps) return -1;
        trace->steps = new_steps;
        trace->capacity = new_cap;
    }

    /* 添加步骤 */
    ProofStep *step = &trace->steps[trace->step_count];
    step->step_id = trace->step_count;

    /* 复制规则名称 */
    strncpy(step->rule, rule, MAX_RULE_NAME_LENGTH - 1);
    step->rule[MAX_RULE_NAME_LENGTH - 1] = '\0';

    /* 状态描述（如果有） */
    if (state) {
        snprintf(step->state_desc, MAX_STATE_DESC_LENGTH,
                 "State at step %d", trace->step_count);
    } else {
        step->state_desc[0] = '\0';
    }

    step->timestamp = (int64_t)time(NULL);

    return trace->step_count++;
}

bool lv00_proof_trace_is_complete(const ProofTrace *trace) {
    return trace ? trace->complete : false;
}

void lv00_proof_trace_mark_complete(ProofTrace *trace) {
    if (trace) {
        trace->complete = true;
        trace->end_time = (int64_t)time(NULL);
    }
}

int lv00_proof_trace_get_step_count(const ProofTrace *trace) {
    return trace ? trace->step_count : 0;
}

const char *lv00_proof_trace_get_rule(const ProofTrace *trace, int step_index) {
    if (!trace || step_index < 0 || step_index >= trace->step_count) {
        return NULL;
    }
    return trace->steps[step_index].rule;
}

char *lv00_proof_trace_export(const ProofTrace *trace) {
    if (!trace) return NULL;

    /* 分配输出缓冲区 */
    size_t buf_size = (size_t)(trace->step_count * 256 + 1024);
    char *buf = lv00_malloc(buf_size);
    if (!buf) return NULL;

    int pos = 0;
    pos += snprintf(buf + pos, buf_size - (size_t)pos,
                    "=== 证明追踪 ===\n");
    pos += snprintf(buf + pos, buf_size - (size_t)pos,
                    "步骤数: %d\n", trace->step_count);
    pos += snprintf(buf + pos, buf_size - (size_t)pos,
                    "状态: %s\n\n", trace->complete ? "完成" : "进行中");

    for (int i = 0; i < trace->step_count; i++) {
        ProofStep *step = &trace->steps[i];
        pos += snprintf(buf + pos, buf_size - (size_t)pos,
                        "步骤 %d: %s", step->step_id, step->rule);
        if (step->state_desc[0] != '\0') {
            pos += snprintf(buf + pos, buf_size - (size_t)pos,
                            " [%s]", step->state_desc);
        }
        pos += snprintf(buf + pos, buf_size - (size_t)pos, "\n");
    }

    return buf;
}

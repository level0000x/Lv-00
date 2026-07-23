/**
 * @file proof_trace.c
 * @brief 证明追踪系统实现
 *
 * @details 记录证明过程中的每个步骤，支持回溯和调试
 *
 * @version 5.0.0
 */

#include "lv/proof_trace.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

/**
 * @brief 创建新的证明轨迹
 *
 * @return 新创建的证明轨迹指针，失败返回 NULL
 */
ProofTrace *lv_proof_trace_create(void) {
    ProofTrace *trace = lv_calloc(1, sizeof(ProofTrace));
    if (!trace) return NULL;

    trace->capacity = 64;
    trace->steps = lv_calloc((size_t)trace->capacity, sizeof(ProofStep));
    if (!trace->steps) {
        lv_free((void **)&trace);
        return NULL;
    }

    trace->step_count = 0;
    trace->complete = false;
    trace->start_time = (int64_t)time(NULL);

    return trace;
}

/**
 * @brief 销毁证明轨迹
 *
 * @param trace 要销毁的证明轨迹
 */
void lv_proof_trace_destroy(ProofTrace *trace) {
    if (!trace) return;
    lv_free((void **)&trace->steps);
    lv_free((void **)&trace);
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
    if (!trace || !rule) return -1;

    /* 扩容检查 */
    if (trace->step_count >= trace->capacity) {
        int new_cap = trace->capacity * 2;
        ProofStep *new_steps = lv_realloc(
            trace->steps, (size_t)new_cap * sizeof(ProofStep));
        if (!new_steps) return -1;
        trace->steps = new_steps;
        trace->capacity = new_cap;
    }

    /* 添加步骤 */
    ProofStep *step = &trace->steps[trace->step_count];
    step->step_id = trace->step_count;

    /* 复制规则名称（截断保护） */
    strncpy(step->rule, rule, MAX_RULE_NAME_LENGTH - 1);
    step->rule[MAX_RULE_NAME_LENGTH - 1] = '\0';

    /* 状态描述：使用调用者提供的 state 字符串，无则留空 */
    if (state) {
        strncpy(step->state_desc, (const char *)state, MAX_STATE_DESC_LENGTH - 1);
        step->state_desc[MAX_STATE_DESC_LENGTH - 1] = '\0';
    } else {
        step->state_desc[0] = '\0';
    }

    step->timestamp = (int64_t)time(NULL);

    return trace->step_count++;
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
        trace->end_time = (int64_t)time(NULL);
    }
}

/**
 * @brief 获取轨迹中的步骤数
 *
 * @param trace 证明轨迹
 * @return 步骤数量，若 trace 为 NULL 则返回 0
 */
int lv_proof_trace_get_step_count(const ProofTrace *trace) {
    return trace ? trace->step_count : 0;
}

/**
 * @brief 获取指定步骤的推理规则
 *
 * @param trace      证明轨迹
 * @param step_index 步骤索引
 * @return 规则名称字符串，若索引无效则返回 NULL
 */
const char *lv_proof_trace_get_rule(const ProofTrace *trace, int step_index) {
    if (!trace || step_index < 0 || step_index >= trace->step_count) {
        return NULL;
    }
    return trace->steps[step_index].rule;
}

/**
 * @brief 将证明轨迹导出为字符串
 *
 * @param trace 证明轨迹
 * @return 格式化后的证明轨迹字符串（调用者负责释放），失败返回 NULL
 */
char *lv_proof_trace_export(const ProofTrace *trace) {
    if (!trace) return NULL;

    /* 分配输出缓冲区 */
    size_t buf_size = (size_t)trace->step_count * 256 + 1024;
    char *buf = lv_malloc(buf_size);
    if (!buf) return NULL;

    int pos = 0;
    /* 写入前计算剩余空间，防止 pos 超过 buf_size 导致回绕 */
    #define TRACE_WRITE(...) do { \
        if ((size_t)pos < buf_size) { \
            int n = snprintf(buf + pos, buf_size - (size_t)pos, __VA_ARGS__); \
            pos += (n > 0 ? n : 0); \
            if ((size_t)pos > buf_size) pos = (int)buf_size; \
        } \
    } while (0)

    TRACE_WRITE("=== 证明追踪 ===\n");
    TRACE_WRITE("步骤数: %d\n", trace->step_count);
    TRACE_WRITE("状态: %s\n\n", trace->complete ? "完成" : "进行中");

    for (int i = 0; i < trace->step_count; i++) {
        ProofStep *step = &trace->steps[i];
        TRACE_WRITE("步骤 %d: %s", step->step_id, step->rule);
        if (step->state_desc[0] != '\0') {
            TRACE_WRITE(" [%s]", step->state_desc);
        }
        TRACE_WRITE("\n");
    }
    #undef TRACE_WRITE

    return buf;
}

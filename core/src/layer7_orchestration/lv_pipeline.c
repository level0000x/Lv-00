/**
 * @file lv_pipeline.c
 * @brief 流水线抽象层实现
 *
 * @details 提供标准化的流水线处理框架，支持阶段管理、顺序执行、
 *          进度报告、取消操作和事件总线集成。
 */

#include "lv/lv_pipeline.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "lv/lv_internal.h"
#include "lv/lv_check.h"
#include "lv/lv_utils.h"

/* 流水线进度事件类型 */
#define lv_PIPELINE_EVENT_PROGRESS 1000
#define lv_PIPELINE_EVENT_STAGE_BEGIN 1001
#define lv_PIPELINE_EVENT_STAGE_END 1002
#define lv_PIPELINE_EVENT_COMPLETE 1003
#define lv_PIPELINE_EVENT_CANCELLED 1004

/* 进度事件数据结构 */
typedef struct {
    int stage_index;
    int total_stages;
    const char *stage_name;
    int progress_pct;
    const char *message;
} lvPipelineProgressEvent;

/* ============================================================
 * 内部辅助函数
 * ============================================================ */

/**
 * @brief 发送进度事件到事件总线
 */
static void lv_pipeline_emit_progress(lvPipeline *pipeline, int stage_index,
                                       int total_stages, const char *stage_name,
                                       int progress_pct, const char *message) {
    if (!pipeline->event_bus || !pipeline->config.enable_progress_events)
        return;

    lvPipelineProgressEvent evt_data;
    evt_data.stage_index = stage_index;
    evt_data.total_stages = total_stages;
    evt_data.stage_name = stage_name;
    evt_data.progress_pct = progress_pct;
    evt_data.message = message;

    lv_event_emit(pipeline->event_bus, lv_PIPELINE_EVENT_PROGRESS, (void *)&evt_data);
}

/**
 * @brief 获取当前微秒时间戳
 */
static int64_t lv_pipeline_now_us(void) {
    return (int64_t)((double)clock() / CLOCKS_PER_SEC * 1000000.0);
}

/**
 * @brief 计算阶段级进度百分比（0-100）
 */
static int lv_pipeline_calc_stage_progress(int stage_index, int total_stages) {
    if (total_stages <= 0) return 100;
    /* 每个阶段贡献均匀的进度份额 */
    return (stage_index * 100) / total_stages;
}

/* ============================================================
 * 状态转字符串
 * ============================================================ */

const char *lv_pipeline_stage_status_to_string(lvPipelineStageStatus status) {
    switch (status) {
        case lv_PIPELINE_STAGE_IDLE:      return "IDLE";
        case lv_PIPELINE_STAGE_RUNNING:   return "RUNNING";
        case lv_PIPELINE_STAGE_COMPLETED: return "COMPLETED";
        case lv_PIPELINE_STAGE_FAILED:    return "FAILED";
        case lv_PIPELINE_STAGE_SKIPPED:   return "SKIPPED";
        case lv_PIPELINE_STAGE_CANCELLED: return "CANCELLED";
        default:                          return "UNKNOWN";
    }
}

/* ============================================================
 * 初始化/清理
 * ============================================================ */

void lv_pipeline_init(lvPipeline *pipeline, const lvPipelineConfig *config) {
    lv_CHECK_NOT_NULL_VOID(pipeline);

    memset(pipeline, 0, sizeof(*pipeline));

    if (config) {
        pipeline->config = *config;
    } else {
        pipeline->config = (lvPipelineConfig)lv_PIPELINE_DEFAULT_CONFIG;
    }

    /* 预分配阶段数组 */
    int capacity = pipeline->config.max_stages > 0 ? pipeline->config.max_stages : 32;
    pipeline->stages = (lvPipelineStage *)lv_calloc((size_t)capacity, sizeof(lvPipelineStage));
    if (pipeline->stages) {
        pipeline->stage_capacity = capacity;
    } else {
        pipeline->stage_capacity = 0;
    }

    pipeline->stage_count = 0;
    pipeline->cancelled = false;
    pipeline->error_message[0] = '\0';
}

void lv_pipeline_cleanup(lvPipeline *pipeline) {
    if (!pipeline) return;
    lv_free((void **)&pipeline->stages);
    memset(pipeline, 0, sizeof(*pipeline));
}

/* ============================================================
 * 阶段管理
 * ============================================================ */

int lv_pipeline_add_stage(lvPipeline *pipeline, const char *name,
                           const char *description, lvPipelineProcessFn process,
                           void *stage_ctx) {
    lv_CHECK_NOT_NULL(pipeline);
    lv_CHECK_ARG(name != NULL, lv_ERROR_INVALID_PARAM, "pipeline add_stage: name is NULL");
    lv_CHECK_ARG(process != NULL, lv_ERROR_INVALID_PARAM, "pipeline add_stage: process is NULL");

    if (pipeline->stage_count >= pipeline->config.max_stages) {
        lv_RETURN_ERROR(lv_ERROR_INVALID_STATE, "pipeline add_stage: max stages reached (%d)",
                        pipeline->config.max_stages);
    }

    /* 扩容 */
    if (pipeline->stage_count >= pipeline->stage_capacity) {
        int new_capacity = pipeline->stage_capacity > 0
                           ? pipeline->stage_capacity * 2
                           : 8;
        if (!lv_ensure_capacity((void **)&pipeline->stages, pipeline->stage_count,
                                 &pipeline->stage_capacity, sizeof(lvPipelineStage), new_capacity)) {
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "pipeline add_stage: realloc failed");
        }
    }

    lvPipelineStage *stage = &pipeline->stages[pipeline->stage_count];
    memset(stage, 0, sizeof(*stage));
    stage->name = name;
    stage->description = description;
    stage->process = process;
    stage->stage_ctx = stage_ctx;
    stage->status = lv_PIPELINE_STAGE_IDLE;
    stage->start_time_us = 0;
    stage->duration_us = 0;

    return pipeline->stage_count++;
}

bool lv_pipeline_remove_stage(lvPipeline *pipeline, int stage_index) {
    if (!pipeline || stage_index < 0 || stage_index >= pipeline->stage_count)
        return false;

    /* 向前移动后续元素 */
    int move_count = pipeline->stage_count - stage_index - 1;
    if (move_count > 0) {
        memmove(&pipeline->stages[stage_index], &pipeline->stages[stage_index + 1],
                (size_t)move_count * sizeof(lvPipelineStage));
    }
    pipeline->stage_count--;

    return true;
}

bool lv_pipeline_clear_stages(lvPipeline *pipeline) {
    if (!pipeline) return false;
    for (int i = 0; i < pipeline->stage_count; i++) {
        memset(&pipeline->stages[i], 0, sizeof(lvPipelineStage));
    }
    pipeline->stage_count = 0;
    pipeline->error_message[0] = '\0';
    return true;
}

int lv_pipeline_stage_count(const lvPipeline *pipeline) {
    return pipeline ? pipeline->stage_count : 0;
}

lvPipelineStage *lv_pipeline_get_stage(lvPipeline *pipeline, int stage_index) {
    if (!pipeline || stage_index < 0 || stage_index >= pipeline->stage_count)
        return NULL;
    return &pipeline->stages[stage_index];
}

/* ============================================================
 * 执行
 * ============================================================ */

bool lv_pipeline_execute(lvPipeline *pipeline, void *pipeline_data) {
    lv_CHECK_NOT_NULL(pipeline);
    lv_CHECK_ARG(pipeline->stage_count > 0, lv_ERROR_INVALID_STATE,
                 "pipeline execute: no stages added");

    pipeline->cancelled = false;
    pipeline->error_message[0] = '\0';

    int total_stages = pipeline->stage_count;
    int last_reported_pct = -1;

    for (int i = 0; i < total_stages; i++) {
        /* 检查取消标志 */
        if (pipeline->cancelled) {
            /* 标记所有未执行的阶段为已取消 */
            for (int j = i; j < total_stages; j++) {
                if (pipeline->stages[j].status == lv_PIPELINE_STAGE_IDLE) {
                    pipeline->stages[j].status = lv_PIPELINE_STAGE_CANCELLED;
                }
            }
            snprintf(pipeline->error_message, sizeof(pipeline->error_message),
                     "Pipeline cancelled at stage %d (%s)", i, pipeline->stages[i].name);
            if (pipeline->event_bus) {
                lv_event_emit(pipeline->event_bus, lv_PIPELINE_EVENT_CANCELLED, pipeline);
            }
            return false;
        }

        lvPipelineStage *stage = &pipeline->stages[i];

        /* 标记为运行中 */
        stage->status = lv_PIPELINE_STAGE_RUNNING;
        stage->start_time_us = lv_pipeline_now_us();

        /* 发送阶段开始事件 */
        if (pipeline->event_bus && pipeline->config.enable_progress_events) {
            lvPipelineProgressEvent begin_evt;
            begin_evt.stage_index = i;
            begin_evt.total_stages = total_stages;
            begin_evt.stage_name = stage->name;
            begin_evt.progress_pct = lv_pipeline_calc_stage_progress(i, total_stages);
            begin_evt.message = "stage begin";
            lv_event_emit(pipeline->event_bus, lv_PIPELINE_EVENT_STAGE_BEGIN, (void *)&begin_evt);
        }

        /* 调用进度回调 */
        if (pipeline->progress_cb) {
            int base_pct = lv_pipeline_calc_stage_progress(i, total_stages);
            pipeline->progress_cb(i, total_stages, stage->name, base_pct, pipeline->progress_user_data);
        }

        /* 执行阶段处理函数 */
        char error_buf[256] = {0};
        bool success = stage->process(stage->stage_ctx, pipeline_data,
                                       pipeline->event_bus, error_buf, (int)sizeof(error_buf));

        /* 记录耗时 */
        stage->duration_us = lv_pipeline_now_us() - stage->start_time_us;

        if (success) {
            stage->status = lv_PIPELINE_STAGE_COMPLETED;
        } else {
            stage->status = lv_PIPELINE_STAGE_FAILED;
            /* 复制错误信息 */
            if (error_buf[0] != '\0') {
                strncpy(pipeline->error_message, error_buf, sizeof(pipeline->error_message) - 1);
                pipeline->error_message[sizeof(pipeline->error_message) - 1] = '\0';
            } else {
                snprintf(pipeline->error_message, sizeof(pipeline->error_message),
                         "Stage %d (%s) failed", i, stage->name);
            }
        }

        /* 发送阶段结束事件 */
        if (pipeline->event_bus && pipeline->config.enable_progress_events) {
            lvPipelineProgressEvent end_evt;
            end_evt.stage_index = i;
            end_evt.total_stages = total_stages;
            end_evt.stage_name = stage->name;
            end_evt.progress_pct = lv_pipeline_calc_stage_progress(i + 1, total_stages);
            end_evt.message = success ? "stage completed" : "stage failed";
            lv_event_emit(pipeline->event_bus, lv_PIPELINE_EVENT_STAGE_END, (void *)&end_evt);
        }

        /* 发送进度事件 */
        int current_pct = lv_pipeline_calc_stage_progress(i + 1, total_stages);
        if (pipeline->config.enable_progress_events &&
            (current_pct - last_reported_pct >= pipeline->config.progress_interval_pct ||
             i == total_stages - 1)) {
            lv_pipeline_emit_progress(pipeline, i, total_stages, stage->name,
                                       current_pct, success ? "completed" : "failed");
            last_reported_pct = current_pct;
        }

        /* 通知进度回调（阶段完成） */
        if (pipeline->progress_cb) {
            int complete_pct = lv_pipeline_calc_stage_progress(i + 1, total_stages);
            pipeline->progress_cb(i, total_stages, stage->name, complete_pct, pipeline->progress_user_data);
        }

        /* 失败处理 */
        if (!success) {
            if (pipeline->config.stop_on_failure) {
                /* 标记所有后续阶段为已跳过 */
                for (int j = i + 1; j < total_stages; j++) {
                    if (pipeline->stages[j].status == lv_PIPELINE_STAGE_IDLE) {
                        pipeline->stages[j].status = lv_PIPELINE_STAGE_SKIPPED;
                    }
                }
                return false;
            }
            /* 不停止时继续执行下一阶段 */
        }
    }

    /* 发送完成事件 */
    if (pipeline->event_bus && pipeline->config.enable_progress_events) {
        lvPipelineProgressEvent complete_evt;
        complete_evt.stage_index = total_stages;
        complete_evt.total_stages = total_stages;
        complete_evt.stage_name = NULL;
        complete_evt.progress_pct = 100;
        complete_evt.message = "pipeline complete";
        lv_event_emit(pipeline->event_bus, lv_PIPELINE_EVENT_COMPLETE, (void *)&complete_evt);
    }

    return true;
}

void lv_pipeline_cancel(lvPipeline *pipeline) {
    if (pipeline) {
        pipeline->cancelled = true;
    }
}

/* ============================================================
 * 进度回调
 * ============================================================ */

void lv_pipeline_set_progress_callback(lvPipeline *pipeline,
                                        lvPipelineProgressFn cb, void *user_data) {
    if (!pipeline) return;
    pipeline->progress_cb = cb;
    pipeline->progress_user_data = user_data;
}

/* ============================================================
 * 事件总线关联
 * ============================================================ */

void lv_pipeline_set_event_bus(lvPipeline *pipeline, lvEventBus *bus) {
    if (!pipeline) return;
    pipeline->event_bus = bus;
}

lvEventBus *lv_pipeline_get_event_bus(const lvPipeline *pipeline) {
    return pipeline ? pipeline->event_bus : NULL;
}

/* ============================================================
 * 状态查询
 * ============================================================ */

lvPipelineStageStatus lv_pipeline_get_stage_status(const lvPipeline *pipeline, int stage_index) {
    if (!pipeline || stage_index < 0 || stage_index >= pipeline->stage_count)
        return lv_PIPELINE_STAGE_IDLE;
    return pipeline->stages[stage_index].status;
}

bool lv_pipeline_is_running(const lvPipeline *pipeline) {
    if (!pipeline) return false;
    for (int i = 0; i < pipeline->stage_count; i++) {
        if (pipeline->stages[i].status == lv_PIPELINE_STAGE_RUNNING)
            return true;
    }
    return false;
}

bool lv_pipeline_is_cancelled(const lvPipeline *pipeline) {
    return pipeline ? pipeline->cancelled : false;
}

const char *lv_pipeline_get_error(const lvPipeline *pipeline) {
    if (!pipeline) return NULL;
    return pipeline->error_message[0] != '\0' ? pipeline->error_message : NULL;
}
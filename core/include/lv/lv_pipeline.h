#ifndef lv_PIPELINE_H
#define lv_PIPELINE_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "lv/lv_event_bus.h"

// 流水线阶段状态
typedef enum {
    lv_PIPELINE_STAGE_IDLE = 0,
    lv_PIPELINE_STAGE_RUNNING,
    lv_PIPELINE_STAGE_COMPLETED,
    lv_PIPELINE_STAGE_FAILED,
    lv_PIPELINE_STAGE_SKIPPED,
    lv_PIPELINE_STAGE_CANCELLED
} lvPipelineStageStatus;

// 流水线阶段进度回调
typedef void (*lvPipelineProgressFn)(int stage_index, int total_stages,
                                      const char *stage_name, int progress_pct,
                                      void *user_data);

// 流水线阶段处理函数
typedef bool (*lvPipelineProcessFn)(void *stage_ctx, void *pipeline_data,
                                     lvEventBus *event_bus, char *error_buf, int error_buf_size);

// 流水线阶段定义
typedef struct lvPipelineStage {
    const char *name;                      // 阶段名称
    const char *description;               // 阶段描述
    lvPipelineProcessFn process;           // 处理函数
    void *stage_ctx;                       // 阶段上下文
    lvPipelineStageStatus status;          // 当前状态
    int64_t start_time_us;                 // 开始时间戳
    int64_t duration_us;                   // 执行耗时
} lvPipelineStage;

// 流水线配置
typedef struct {
    int max_stages;                        // 最大阶段数
    bool stop_on_failure;                  // 失败时停止
    bool enable_progress_events;           // 是否发送进度事件
    int progress_interval_pct;             // 进度事件间隔百分比
} lvPipelineConfig;

#define lv_PIPELINE_DEFAULT_CONFIG { 32, true, true, 5 }

// 流水线
typedef struct lvPipeline {
    lvPipelineStage *stages;               // 阶段数组
    int stage_count;
    int stage_capacity;
    lvPipelineConfig config;
    lvPipelineProgressFn progress_cb;      // 进度回调
    void *progress_user_data;
    lvEventBus *event_bus;                 // 关联的事件总线
    bool cancelled;                        // 取消标志
    char error_message[256];               // 错误信息
} lvPipeline;

// ---- API ----

// 初始化/清理
void lv_pipeline_init(lvPipeline *pipeline, const lvPipelineConfig *config);
void lv_pipeline_cleanup(lvPipeline *pipeline);

// 阶段管理
int lv_pipeline_add_stage(lvPipeline *pipeline, const char *name,
                           const char *description, lvPipelineProcessFn process,
                           void *stage_ctx);
bool lv_pipeline_remove_stage(lvPipeline *pipeline, int stage_index);
bool lv_pipeline_clear_stages(lvPipeline *pipeline);
int lv_pipeline_stage_count(const lvPipeline *pipeline);
lvPipelineStage *lv_pipeline_get_stage(lvPipeline *pipeline, int stage_index);

// 执行
bool lv_pipeline_execute(lvPipeline *pipeline, void *pipeline_data);
void lv_pipeline_cancel(lvPipeline *pipeline);

// 进度
void lv_pipeline_set_progress_callback(lvPipeline *pipeline,
                                        lvPipelineProgressFn cb, void *user_data);

// 事件总线关联
void lv_pipeline_set_event_bus(lvPipeline *pipeline, lvEventBus *bus);
lvEventBus *lv_pipeline_get_event_bus(const lvPipeline *pipeline);

// 状态查询
lvPipelineStageStatus lv_pipeline_get_stage_status(const lvPipeline *pipeline, int stage_index);
bool lv_pipeline_is_running(const lvPipeline *pipeline);
bool lv_pipeline_is_cancelled(const lvPipeline *pipeline);
const char *lv_pipeline_get_error(const lvPipeline *pipeline);

// 阶段状态转字符串
const char *lv_pipeline_stage_status_to_string(lvPipelineStageStatus status);

#endif
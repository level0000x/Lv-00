/**
 * @file orchestrator.h
 * @brief Layer 7 编排调度器 —— 多阶段 pipeline 会话管理
 *
 * @details 本模块提供 Lv-00 系统的顶层编排接口，管理从解析到可视化
 *          的完整 pipeline（Stage 0-5）。每个阶段独立运行，支持
 *          从任意阶段启动、单阶段执行和全 pipeline 执行。
 *
 * @author Lv-00 Project
 * @version 1.1.0
 */

#ifndef LV00_ORCHESTRATOR_H
#define LV00_ORCHESTRATOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/* ============================================================
 * 枚举定义
 * ============================================================ */

/**
 * @brief Pipeline 阶段枚举
 */
typedef enum {
    LV00_STAGE_PARSE = 0,     /**< Stage 0: 输入解析 */
    LV00_STAGE_RESOURCE,      /**< Stage 1: 资源加载 */
    LV00_STAGE_GEOMETRY,      /**< Stage 2: 几何构造 */
    LV00_STAGE_REASONING,     /**< Stage 3: 推理求解 */
    LV00_STAGE_OUTPUT,        /**< Stage 4: 输出生成 */
    LV00_STAGE_VISUAL,        /**< Stage 5: 可视化（可选） */
    LV00_STAGE_COUNT          /**< 阶段总数 */
} Lv00PipelineStage;

/**
 * @brief 阶段运行状态
 */
typedef enum {
    LV00_STAGE_PENDING = 0,   /**< 未开始 */
    LV00_STAGE_RUNNING,       /**< 运行中 */
    LV00_STAGE_COMPLETED,     /**< 已完成 */
    LV00_STAGE_FAILED,        /**< 失败 */
    LV00_STAGE_SKIPPED        /**< 已跳过 */
} Lv00StageStatus;

/* ============================================================
 * 结构体定义
 * ============================================================ */

/**
 * @brief 单个阶段的运行结果
 */
typedef struct {
    Lv00PipelineStage stage;       /**< 阶段标识 */
    Lv00StageStatus status;       /**< 运行状态 */
    double elapsed_ms;            /**< 耗时（毫秒） */
    char error_msg[256];           /**< 错误信息 */
} Lv00StageResult;

/**
 * @brief 会话配置
 */
typedef struct {
    int max_reasoning_depth;       /**< 最大推理深度 */
    int timeout_ms;                /**< 超时（毫秒） */
    int enable_visualization;      /**< 是否启用可视化 */
    char input_format[64];         /**< 输入格式 */
    char output_format[64];        /**< 输出格式 */
} Lv00SessionConfig;

/**
 * @brief 编排会话
 */
typedef struct Lv00Session {
    int session_id;                /**< 会话 ID */
    char session_name[128];         /**< 会话名称 */
    Lv00SessionConfig config;       /**< 会话配置 */
    Lv00StageResult stages[LV00_STAGE_COUNT]; /**< 各阶段结果 */
    int success;                    /**< 整体是否成功 */
    char final_error[512];          /**< 最终错误信息 */
} Lv00Session;

/* ============================================================
 * 公共接口
 * ============================================================ */

/**
 * @brief 创建默认会话配置
 * @return 返回默认配置结构体
 */
Lv00SessionConfig lv00_default_session_config(void);

/**
 * @brief 创建会话
 * @param name 会话名称
 * @return 成功返回会话指针，失败返回 NULL
 */
Lv00Session *lv00_session_create(const char *name);

/**
 * @brief 销毁会话
 * @param session 会话指针
 */
void lv00_session_destroy(Lv00Session *session);

/**
 * @brief 配置会话
 * @param session 会话指针
 * @param config 配置参数
 * @return 成功返回 0，失败返回非零
 */
int lv00_session_configure(Lv00Session *session, const Lv00SessionConfig *config);

/**
 * @brief 运行完整 pipeline
 * @param session 会话指针
 * @param input 输入字符串
 * @return 成功返回 0，失败返回非零
 */
int lv00_session_run(Lv00Session *session, const char *input);

/**
 * @brief 运行单个阶段
 * @param session 会话指针
 * @param stage 要运行的阶段
 * @return 成功返回 0，失败返回非零
 */
int lv00_session_run_stage(Lv00Session *session, Lv00PipelineStage stage);

/**
 * @brief 从指定阶段开始运行
 * @param session 会话指针
 * @param from_stage 起始阶段
 * @return 成功返回 0，失败返回非零
 */
int lv00_session_run_from(Lv00Session *session, Lv00PipelineStage from_stage);

/**
 * @brief 获取阶段结果
 * @param session 会话指针
 * @param stage 阶段标识
 * @return 返回对应阶段的结果指针
 */
const Lv00StageResult *lv00_session_stage_result(const Lv00Session *session, Lv00PipelineStage stage);

/**
 * @brief 查询整体是否成功
 * @param session 会话指针
 * @return 成功返回 1，失败返回 0
 */
int lv00_session_success(const Lv00Session *session);

/**
 * @brief 获取最终错误信息
 * @param session 会话指针
 * @return 返回错误信息字符串
 */
const char *lv00_session_error(const Lv00Session *session);

/**
 * @brief 计算总耗时
 * @param session 会话指针
 * @return 返回总耗时（毫秒）
 */
double lv00_session_total_time(const Lv00Session *session);

#ifdef __cplusplus
}
#endif

#endif /* LV00_ORCHESTRATOR_H */

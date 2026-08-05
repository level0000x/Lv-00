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

#ifndef lv_ORCHESTRATOR_H
#define lv_ORCHESTRATOR_H

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
    lv_STAGE_PARSE = 0, /**< Stage 0: 输入解析 */
    lv_STAGE_RESOURCE,  /**< Stage 1: 资源加载 */
    lv_STAGE_GEOMETRY,  /**< Stage 2: 几何构造 */
    lv_STAGE_REASONING, /**< Stage 3: 推理求解 */
    lv_STAGE_OUTPUT,    /**< Stage 4: 输出生成 */
    lv_STAGE_VISUAL,    /**< Stage 5: 可视化（可选） */
    lv_STAGE_COUNT      /**< 阶段总数 */
} lvSessionStage;

/**
 * @brief 阶段运行状态
 */
typedef enum {
    lv_STAGE_PENDING = 0, /**< 未开始 */
    lv_STAGE_RUNNING,     /**< 运行中 */
    lv_STAGE_COMPLETED,   /**< 已完成 */
    lv_STAGE_FAILED,      /**< 失败 */
    lv_STAGE_SKIPPED      /**< 已跳过 */
} lvStageStatus;

/* ============================================================
 * 结构体定义
 * ============================================================ */

/**
 * @brief 单个阶段的运行结果
 */
typedef struct {
    lvSessionStage stage; /**< 阶段标识 */
    lvStageStatus status;  /**< 运行状态 */
    double elapsed_ms;     /**< 耗时（毫秒） */
    char error_msg[256];   /**< 错误信息 */
} lvStageResult;

/**
 * @brief 会话配置
 */
typedef struct {
    int max_reasoning_depth;  /**< 最大推理深度 */
    int timeout_ms;           /**< 超时（毫秒） */
    int enable_visualization; /**< 是否启用可视化 */
    char input_format[64];    /**< 输入格式 */
    char output_format[64];   /**< 输出格式 */
} lvSessionConfig;

/**
 * @brief 编排会话
 */
typedef struct lvSession {
    int session_id;                       /**< 会话 ID */
    char session_name[128];               /**< 会话名称 */
    lvSessionConfig config;               /**< 会话配置 */
    lvStageResult stages[lv_STAGE_COUNT]; /**< 各阶段结果 */
    int success;                          /**< 整体是否成功 */
    char final_error[512];                /**< 最终错误信息 */
} lvSession;

#ifdef __cplusplus
}
#endif

#endif /* lv_ORCHESTRATOR_H */

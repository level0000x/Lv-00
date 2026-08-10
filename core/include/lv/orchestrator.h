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

#ifndef lv_PUBLIC_API
#define lv_PUBLIC_API
#endif

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
    void *internal;                       /**< 编排器内部状态（lv_impl_upper_orchestrator.c 私有） */
} lvSession;

/* ============================================================
 * 编排器 API（实现于 core/src/lv_impl_upper_orchestrator.c）
 * ============================================================ */

/**
 * @brief 填充会话默认配置
 * @param out 输出配置结构（非空）
 */
lv_PUBLIC_API void lv_orchestrator_config_default(lvSessionConfig *out);

/**
 * @brief 创建编排会话
 * @param config 会话配置（可传 NULL 使用默认值）
 * @return 新建会话指针；失败返回 NULL
 */
lv_PUBLIC_API lvSession *lv_orchestrator_create(const lvSessionConfig *config);

/**
 * @brief 销毁编排会话并释放全部内部资源
 * @param session 会话指针（可为 NULL）
 */
lv_PUBLIC_API void lv_orchestrator_destroy(lvSession *session);

/**
 * @brief 运行完整 pipeline（Parse→Resource→Geometry→Reasoning→Output→Visual）
 *
 * 任一阶段失败时，后续阶段自动置为 lv_STAGE_SKIPPED，session->success 置 0。
 *
 * @param session    会话（非空）
 * @param input_path 输入文件路径（.lv 或 .ggb/.svg 文本源；为空则使用已有输入）
 * @return 0 表示全部阶段成功，非 0 表示流水线失败
 */
lv_PUBLIC_API int lv_orchestrator_run(lvSession *session, const char *input_path);

/**
 * @brief 运行单个阶段（前置未运行阶段将按顺序自动执行）
 * @param session 会话（非空）
 * @param stage   目标阶段
 * @return 0 表示该阶段成功，非 0 表示失败
 */
lv_PUBLIC_API int lv_orchestrator_run_stage(lvSession *session, lvSessionStage stage);

/**
 * @brief 查询阶段运行结果
 * @param session 会话（非空）
 * @param stage   目标阶段
 * @param out     输出阶段结果（非空）
 * @return 0 表示成功，非 0 表示参数无效
 */
lv_PUBLIC_API int lv_orchestrator_get_stage_result(const lvSession *session, lvSessionStage stage, lvStageResult *out);

/**
 * @brief 获取最近一次错误信息
 * @param session 会话（可为 NULL）
 * @return 错误字符串（失败时非空；成功时为空串）
 */
lv_PUBLIC_API const char *lv_orchestrator_last_error(const lvSession *session);

#ifdef __cplusplus
}
#endif

#endif /* lv_ORCHESTRATOR_H */

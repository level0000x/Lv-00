/**
 * @file application.h
 * @brief Layer 9 应用入口层 —— 顶层 CLI/批量工作流接口
 *
 * @details 本模块提供 Lv-00 系统的应用级入口，封装编排会话（L7）与
 *          元验证（L8），支持 Load / Verify / Batch / Export / Visualize
 *          五种命令。实现于 core/src/lv_impl_upper_app.c。
 *
 * @author Lv-00 Project
 * @version 1.1.0
 */

#ifndef lv_APPLICATION_H
#define lv_APPLICATION_H

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
 * @brief 应用命令
 */
typedef enum {
    LV_APP_CMD_LOAD = 0, /**< 加载输入并运行完整流水线 */
    LV_APP_CMD_VERIFY,   /**< 运行流水线并执行元验证（六项检查） */
    LV_APP_CMD_BATCH,    /**< 批量验证多个输入文件 */
    LV_APP_CMD_EXPORT,   /**< 运行流水线并导出结果到文件 */
    LV_APP_CMD_VISUALIZE /**< 运行流水线并生成可视化输出 */
} lvAppCommand;

/* ============================================================
 * 结构体定义
 * ============================================================ */

/**
 * @brief 应用运行配置
 */
typedef struct {
    lvAppCommand command;                 /**< 要执行的命令 */
    const char *input_path;               /**< 输入文件路径（.lv/.dsl 源） */
    const char *output_path;              /**< 导出输出路径（EXPORT/VISUALIZE） */
    const char *output_format;            /**< 导出格式：json/canonical/dot */
    const char *const *batch_inputs;      /**< BATCH 输入文件列表 */
    int batch_count;                      /**< BATCH 输入数量 */
    int timeout_ms;                       /**< 推理超时（毫秒，0 用默认） */
    int max_reasoning_depth;              /**< 最大推理深度（0 用默认） */
    int enable_visualization;             /**< 是否启用可视化阶段 */
} lvApplicationConfig;

/* ============================================================
 * 应用 API（实现于 core/src/lv_impl_upper_app.c）
 * ============================================================ */

/**
 * @brief 运行应用命令
 *
 * @param config 应用配置（非空）
 * @return 成功返回 0；LOAD/EXPORT/VISUALIZE 失败返回非 0；
 *         VERIFY/BATCH 返回通过检查的文件数（0 表示全部失败）
 */
lv_PUBLIC_API int lv_application_run(const lvApplicationConfig *config);

/**
 * @brief 获取应用版本字符串
 * @return 形如 "1.1.0" 的版本字符串（静态存储）
 */
lv_PUBLIC_API const char *lv_application_get_version(void);

/**
 * @brief 单文件快速验证（Load + Verify）
 * @param filepath 输入文件路径
 * @return 六项检查全部通过返回 1，否则返回 0
 */
lv_PUBLIC_API int lv_application_quick_verify(const char *filepath);

/**
 * @brief 批量验证多个文件
 * @param filepaths 输入文件路径数组
 * @param count     文件数量
 * @return 通过的检查总数（每文件最多 6 项）；参数无效返回 -1
 */
lv_PUBLIC_API int lv_application_batch(const char *const *filepaths, int count);

/**
 * @brief 释放应用层全局资源（无全局状态时为空操作）
 */
lv_PUBLIC_API void lv_application_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* lv_APPLICATION_H */

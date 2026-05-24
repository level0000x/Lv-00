/**
 * @file config.h
 * @brief Lv-00 集中化配置 —— 所有硬编码常量的单一事实来源
 *
 * 此文件收集项目中散布在各源文件中的魔数、阈值、默认缓冲区大小
 * 和定时参数，统一以 LV00_CONFIG_ 前缀命名。
 *
 * 使用方式:
 *   #include "lv00/config.h"
 *   engine->rewrite_step_limit = LV00_CONFIG_DEFAULT_REWRITE_LIMIT;
 *
 * 来源文件参考:
 *   - solver.h: SOLVER_MAX_VAR_ID
 *   - stream.h: STREAM_ASYNC_QUEUE_DEFAULT_CAPACITY, STREAM_JSON_BUFFER_DEFAULT_SIZE
 *   - rewrite.h: WL_ITERATIONS, WL_HISTORY_SIZE
 *   - symbolic_coord.h: MAX_MODULE_DEPTH, BIT_CUTOFF_THRESHOLD, MAX_PRECISION_BITS
 *   - engine.h: 默认重写步数上限
 *   - mini_kernel.h: 默认 max_statements, max_proof_depth, verification_timeout_ms
 *   - constraint_graph.h: 错误缓冲区大小
 *   - 各 .c 文件中的魔数
 *
 * @version 1.0.0
 * @date 2026-05-24
 */

#ifndef LV00_CONFIG_H
#define LV00_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* ====================================================================
 * 约束图与求解器限制
 * ==================================================================== */

/** @brief 求解器中变量 ID 的最大值（防止稀疏 ID 导致 OOM） */
#define LV00_CONFIG_SOLVER_MAX_VAR_ID             100000

/** @brief 模块最大嵌套深度 */
#define LV00_CONFIG_MAX_MODULE_DEPTH              32

/** @brief 约束图错误缓冲区大小（字节） */
#define LV00_CONFIG_GRAPH_ERROR_BUFFER_SIZE       256

/** @brief 约束图序列化错误缓冲区大小（字节） */
#define LV00_CONFIG_GRAPH_SERIALIZE_BUFFER_SIZE   256

/** @brief 引擎错误信息缓冲区大小（字节） */
#define LV00_CONFIG_ENGINE_ERROR_BUFFER_SIZE      256

/** @brief 动态数组初始容量（指数增长前） */
#define LV00_CONFIG_INITIAL_ARRAY_CAPACITY        8

/** @brief 哈希索引表初始容量（必须为 2 的幂） */
#define LV00_CONFIG_INITIAL_HASH_INDEX_CAPACITY   64

/* ====================================================================
 * 重写引擎阈值
 * ==================================================================== */

/** @brief 默认重写步数上限 */
#define LV00_CONFIG_DEFAULT_REWRITE_LIMIT         1000

/** @brief WL (Weisfeiler-Lehman) 图核迭代次数 */
#define LV00_CONFIG_WL_ITERATIONS                 3

/** @brief WL 图哈希历史环形缓冲区大小 */
#define LV00_CONFIG_WL_HISTORY_SIZE               64

/* ====================================================================
 * 流式输出阈值
 * ==================================================================== */

/** @brief 异步事件队列默认容量 */
#define LV00_CONFIG_STREAM_ASYNC_QUEUE_CAPACITY   1024

/** @brief JSON 序列化缓冲区默认大小（字节） */
#define LV00_CONFIG_STREAM_JSON_BUFFER_SIZE       4096

/** @brief 默认节流间隔（毫秒） */
#define LV00_CONFIG_STREAM_DEFAULT_THROTTLE_MS    50

/* ====================================================================
 * 数值精度与代数计算
 * ==================================================================== */

/** @brief 位数熔断阈值（触发 A→B 计划切换） */
#define LV00_CONFIG_BIT_CUTOFF_THRESHOLD          1000000

/** @brief 代数数默认精度位数 */
#define LV00_CONFIG_MAX_PRECISION_BITS            100

/** @brief 压力测试默认链长度 */
#define LV00_CONFIG_STRESS_TEST_DEFAULT_CHAIN     100

/** @brief 压力测试默认最大多项式度数 */
#define LV00_CONFIG_STRESS_TEST_MAX_POLY_DEGREE   4

/* ====================================================================
 * 极简验证内核 (MiniKernel) 默认参数
 * ==================================================================== */

/** @brief 最大语句数量（0 = 不限制） */
#define LV00_CONFIG_MINI_KERNEL_MAX_STATEMENTS    10000

/** @brief 最大证明深度（防止无限递归，0 = 不限制） */
#define LV00_CONFIG_MINI_KERNEL_MAX_PROOF_DEPTH   1000

/** @brief 验证超时（毫秒，0 = 无超时） */
#define LV00_CONFIG_MINI_KERNEL_VERIFY_TIMEOUT_MS 30000

/* ====================================================================
 * 解析器安全限制
 * ==================================================================== */

/** @brief 最大输入长度（字符数） */
#define LV00_CONFIG_PARSER_MAX_INPUT_LENGTH       1048576  /* 1 MiB */

/** @brief 最大 token 数量 */
#define LV00_CONFIG_PARSER_MAX_TOKENS             100000

/** @brief 最大 AST 深度 */
#define LV00_CONFIG_PARSER_MAX_AST_DEPTH          256

/** @brief 最大 AST 节点数 */
#define LV00_CONFIG_PARSER_MAX_AST_NODES          500000

/** @brief 最大 token 长度（字符数） */
#define LV00_CONFIG_PARSER_MAX_TOKEN_LENGTH       4096

/* ====================================================================
 * 运行时防护 (Runtime Guard) 阈值
 * ==================================================================== */

/** @brief 运行时防护最大递归深度 */
#define LV00_CONFIG_RUNTIME_GUARD_MAX_RECURSE     128

/** @brief 自旋尝试次数 */
#define LV00_CONFIG_RUNTIME_GUARD_SPIN_ATTEMPTS   1024

/** @brief 写操作警告阈值（微秒） */
#define LV00_CONFIG_RUNTIME_GUARD_WRITE_WARN_US   10000

/* ====================================================================
 * 通用缓冲区大小
 * ==================================================================== */

/** @brief 通用标签/名称最大长度 */
#define LV00_CONFIG_MAX_LABEL_LENGTH              256

/** @brief 通用公式文本最大长度 */
#define LV00_CONFIG_MAX_FORMULA_LENGTH            1024

/** @brief 通用变量名最大长度 */
#define LV00_CONFIG_MAX_VARIABLE_NAME_LENGTH      128

/** @brief 通用替换项最大长度 */
#define LV00_CONFIG_MAX_REPLACEMENT_TERM_LENGTH   512

/** @brief 证明引用列表最大长度 */
#define LV00_CONFIG_MAX_PROOF_REFS                64

/* ====================================================================
 * 兼容层 —— 保持与现有宏的向后兼容
 *
 * 以下定义确保在逐步迁移到 config.h 期间，现有代码不中断。
 * 迁移完成后可移除此节。
 * ==================================================================== */

/* solver.h 兼容 */
#ifndef SOLVER_MAX_VAR_ID
#define SOLVER_MAX_VAR_ID LV00_CONFIG_SOLVER_MAX_VAR_ID
#endif

/* symbolic_coord.h 兼容 */
#ifndef MAX_MODULE_DEPTH
#define MAX_MODULE_DEPTH LV00_CONFIG_MAX_MODULE_DEPTH
#endif
#ifndef BIT_CUTOFF_THRESHOLD
#define BIT_CUTOFF_THRESHOLD LV00_CONFIG_BIT_CUTOFF_THRESHOLD
#endif
#ifndef MAX_PRECISION_BITS
#define MAX_PRECISION_BITS LV00_CONFIG_MAX_PRECISION_BITS
#endif

/* rewrite.h 兼容 */
#ifndef WL_ITERATIONS
#define WL_ITERATIONS LV00_CONFIG_WL_ITERATIONS
#endif
#ifndef WL_HISTORY_SIZE
#define WL_HISTORY_SIZE LV00_CONFIG_WL_HISTORY_SIZE
#endif

/* stream.h 兼容 */
#ifndef STREAM_ASYNC_QUEUE_DEFAULT_CAPACITY
#define STREAM_ASYNC_QUEUE_DEFAULT_CAPACITY LV00_CONFIG_STREAM_ASYNC_QUEUE_CAPACITY
#endif
#ifndef STREAM_JSON_BUFFER_DEFAULT_SIZE
#define STREAM_JSON_BUFFER_DEFAULT_SIZE LV00_CONFIG_STREAM_JSON_BUFFER_SIZE
#endif

#ifdef __cplusplus
}
#endif

#endif /* LV00_CONFIG_H */

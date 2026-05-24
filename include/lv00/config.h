/**
 * @file config.h
 * @brief Lv-00 集中化配置 —— 所有硬编码常量的单一事实来源
 *
 * 此文件收集项目中散布在各源文件中的魔数、阈值、默认缓冲区大小
 * 和定时参数，统一以 LV00_CONFIG_ 前缀命名。
 *
 * 【中文模块说明】
 * config.h 是 Lv-00 系统的全局配置中心，集中管理所有可调参数。
 * 所有硬编码常量均在此定义，避免魔数散布在源代码各处。
 * 配置分为以下几大类：
 * - 约束图与求解器限制（变量ID上限、模块嵌套深度、缓冲区大小）
 * - 重写引擎阈值（默认重写步数上限、WL图核迭代次数）
 * - 流式输出阈值（异步队列容量、JSON缓冲区大小、节流间隔）
 * - 数值精度与代数计算（位数熔断阈值、代数数精度位数）
 * - 极简验证内核参数（最大语句数、证明深度、验证超时）
 * - 解析器安全限制（最大输入长度、token数、AST深度/节点数）
 * - 运行时防护阈值（递归深度、自旋次数、写操作警告阈值）
 * - 通用缓冲区大小（标签、公式、变量名、替换项、证明引用）
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
 * @version 3.3.0
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
 * 集中化配置常量（从各模块提取）
 *
 * 以下常量从各模块中提取集中管理，避免魔数分散。
 * 来源模块：constraint_graph, stream, formula_parser, memory_pool 等
 * ==================================================================== */

/* 约束图配置 */
/** @brief 每个节点的最大邻接数（来源：constraint_graph.c） */
#define LV00_CONFIG_GRAPH_ADJ_MAX_PER_NODE    256

/* 流式系统配置 */
/** @brief 流式回调初始容量（来源：stream.c） */
#define LV00_CONFIG_STREAM_INITIAL_CALLBACKS  16

/** @brief 流式回调硬上限（来源：stream.c） */
#define LV00_CONFIG_STREAM_MAX_CALLBACKS      64

/* 内存池配置 */
/** @brief 约束节点池对象大小（来源：memory_pool.c） */
#define LV00_CONFIG_POOL_CONSTRAINT_NODE_SIZE 128

/** @brief 约束池对象大小（来源：memory_pool.c） */
#define LV00_CONFIG_POOL_CONSTRAINT_SIZE      96

/** @brief 符号坐标池对象大小（来源：memory_pool.c） */
#define LV00_CONFIG_POOL_SYMBOLIC_COORD_SIZE  64

/** @brief 证明步骤池对象大小（来源：memory_pool.c） */
#define LV00_CONFIG_POOL_PROOF_STEP_SIZE      128

/* 解析器配置 */
/** @brief 最大坐标数（来源：formula_parser.c） */
#define LV00_CONFIG_PARSER_MAX_COORDINATES    16

/** @brief 最大顶点数（来源：formula_parser.c） */
#define LV00_CONFIG_PARSER_MAX_VERTICES       32

/** @brief 多边形最大顶点数（来源：formula_parser.c） */
#define LV00_CONFIG_PARSER_MAX_POLYGON_VERTICES 32

/** @brief 最大语句数（来源：formula_parser.c） */
#define LV00_CONFIG_PARSER_MAX_STATEMENTS     64

/** @brief 最大参数数（来源：formula_parser.c） */
#define LV00_CONFIG_PARSER_MAX_ARGUMENTS      16

/** @brief 最大参与者数（来源：formula_parser.c） */
#define LV00_CONFIG_PARSER_MAX_PARTICIPANTS   16

/** @brief 解析器缓冲区大小（来源：formula_parser.c） */
#define LV00_CONFIG_PARSER_MAX_BUFFER_SIZE    256

/** @brief 临时消息大小（来源：formula_parser.c） */
#define LV00_CONFIG_PARSER_MAX_TEMP_MSG_SIZE  128

/* ====================================================================
 * 兼容层 —— 保持与现有宏的向后兼容
 *
 * 以下定义确保在逐步迁移到 config.h 期间，现有代码不中断。
 * 迁移完成后可移除此节。
 *
 * 【设计风险说明】
 * 此兼容层使用 #ifndef 守卫定义旧宏名，当旧宏已在其他头文件中
 * 被定义时，本节不会覆盖该定义。这种设计可能导致以下问题：
 *   1. 若某模块在包含 config.h 之前自行定义了同名宏（值不同），
 *      该模块将使用自己的值而非集中配置值，产生不一致。
 *   2. 宏定义顺序依赖：先包含的模块的定义会"胜出"，后包含的
 *      config.h 兼容层静默跳过，不发出任何警告。
 * 缓解措施：建议各模块统一通过 lv00.h（已包含 config.h）引入
 * 配置，避免在模块内部自行定义 LV00_CONFIG_ 前缀以外的同名常量。
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

/*
 * ========================================================================
 * 配置集中化迁移计划
 * ========================================================================
 *
 * 以下分散在各模块中的本地 #define 应逐步迁移到本文件：
 *
 * | 模块              | 当前常量                          | 建议名称                    |
 * |-------------------|-----------------------------------|-----------------------------|
 * | constraint_graph  | ADJ_MAX_PER_NODE (256)            | LV00_CONFIG_ADJ_MAX_PER_NODE|
 * | constraint_graph  | POINT_CONSTRAINT_ARRAY_SIZE (64)  | LV00_CONFIG_CONSTRAINT_ARRAY|
 * | normalization     | NORM_MAX_ID (1000000)             | LV00_CONFIG_NORM_MAX_ID     |
 * | normalization     | NORM_DEFAULT_CAPACITY (16)        | LV00_CONFIG_NORM_DEFAULT_CAP|
 * | solver_core       | DEFAULT_CLAUSE_CAPACITY (256)     | LV00_CONFIG_CLAUSE_CAP      |
 * | solver_core       | DEFAULT_VAR_CAPACITY (128)        | LV00_CONFIG_VAR_CAP         |
 * | rewrite           | REWRITE_VF2_MAX_DEPTH (100)       | LV00_CONFIG_REWRITE_MAX_DEPTH|
 * | memory_pool       | LV00_DEFAULT_ALIGNMENT (16)       | LV00_CONFIG_MEM_ALIGNMENT   |
 * | unify             | MAX_EQUIVALENCES (256)            | LV00_CONFIG_MAX_EQUIVALENCES|
 *
 * 迁移原则：
 * 1. 新常量使用 LV00_CONFIG_ 前缀
 * 2. 原模块保留 #define 作为别名（#define OLD_NAME LV00_CONFIG_NEW_NAME）
 * 3. 逐步替换各模块中的直接引用
 * ========================================================================
 */

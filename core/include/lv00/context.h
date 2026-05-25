/**
 * @file context.h
 * @brief Lv-00 隔离上下文系统 —— 统一状态容器、分支推理与熔断机制
 *
 * @details Lv00Context 是 Lv-00 项目的最高优先级架构修复。它将当前分散在
 *          全局/线程局部变量和 LV00Engine 中的状态统一收容到一个隔离的
 *          上下文容器中，为以下关键需求提供基础设施：
 *
 *          1. **分支证明**：前向证明、反证法、假设引入等推理分支各自拥有
 *             独立的上下文快照，可自由切换/回滚。
 *          2. **并发安全**：每个线程可持有独立的 Lv00Context 实例，
 *             消除全局状态竞争。
 *          3. **资源隔离**：每个几何问题的求解过程完全隔离，
 *             不会相互污染状态。
 *          4. **熔断保护**：内建超时、递归深度、内存上限等多维熔断器，
 *             防止失控计算耗尽系统资源。
 *          5. **状态机追踪**：完整的生命周期状态机，便于调试和监控。
 *
 * 架构位置：
 *   本文件是 Lv-00 从"全局引擎模式"向"隔离上下文模式"迁移的枢纽。
 *   短期内与 LV00Engine 并存（context 持有 engine 引用），
 *   长期逐步将引擎逻辑迁移到 context 中。
 *
 * 借鉴来源：
 *   - Why3 (why3_multi_prover_dispatch.md)：多证明器调度的会话隔离
 *   - Coq/Lean：证明状态快照与回滚 (proof state snapshot)
 *   - Rosette (rosette_symbolic_vm.md)：符号执行的状态分支
 *   - Souffle (souffle_datalog_engine.md)：Datalog 引擎的增量计算缓存
 *
 * @author Lv-00 Project
 * @version 3.3.0
 * @date   2026-05-24
 */

#ifndef LV00_CONTEXT_H
#define LV00_CONTEXT_H

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 依赖头文件
 * ============================================================ */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

/* 错误码系统 —— 所有上下文操作返回 Lv00ErrorCode */
#include "error_codes.h"

/* 工具函数 —— MemoryStats 类型定义在此 */
#include "lv00_utils.h"

/* 前向声明 —— 避免循环依赖，具体类型在各模块头文件中定义 */
struct ConstraintGraph;     /* constraint_graph.h */
struct StreamContext;       /* stream.h */
struct NormalizationResult; /* normalization.h */
struct RewriteRule;         /* rewrite.h */
struct FuncBlock;           /* func_block.h */

/* ============================================================
 * 第一部分：上下文状态机枚举
 *
 * 上下文从创建到销毁经历这些状态的有序转移：
 *
 *   IDLE ──→ PARSING ──→ REASONING ──→ COMPLETE ──→ (reset) ──→ IDLE
 *     │                      │
 *     └──────────────────────┼──→ ERROR ──→ (reset) ──→ IDLE
 *                            │
 *                            └──→ IDLE (取消/中断)
 *
 * 状态转移规则（严格）：
 *   - IDLE         可转到 PARSING               (开始输入)
 *   - IDLE         可转到 ERROR                 (初始化失败)
 *   - PARSING      可转到 REASONING             (解析完成，开始推理)
 *   - PARSING      可转到 ERROR                 (解析失败)
 *   - PARSING      可转到 IDLE                  (取消/中断)
 *   - REASONING    可转到 COMPLETE             (推理成功完成)
 *   - REASONING    可转到 ERROR                 (推理失败/熔断)
 *   - REASONING    可转到 IDLE                  (取消/中断)
 *   - COMPLETE     可转到 IDLE (通过 reset)     (准备新问题)
 *   - ERROR        可转到 IDLE (通过 reset)     (清理后重试)
 * ============================================================ */

/**
 * @brief 上下文状态机枚举
 *
 * 追踪 Lv00Context 从创建到销毁的完整生命周期。
 * 每个状态决定了当前允许执行的操作集合。
 */
typedef enum {
    /** 空闲状态 —— 上下文已创建但尚未开始处理任何问题。
     *  在此状态下可接收输入、设置参数、加载模块/公理。 */
    LV00_CONTEXT_IDLE = 0,

    /** 解析状态 —— 正在将输入文本/DSL 解析为内部约束图结构。
     *  在此状态下可逐步添加几何对象和约束。 */
    LV00_CONTEXT_PARSING,

    /** 推理状态 —— 正在执行重写、求解、合一、证明等推理操作。
     *  在此状态下禁止修改约束图拓扑（仅允许变量绑定和代数计算）。 */
    LV00_CONTEXT_REASONING,

    /** 错误状态 —— 遇到不可恢复的错误（解析失败、约束冲突、资源耗尽等）。
     *  必须通过 lv00_context_reset() 清理后重新开始。 */
    LV00_CONTEXT_ERROR,

    /** 完成状态 —— 当前问题的求解/证明已成功完成。
     *  可查询结果，或通过 lv00_context_reset() 开始新问题。 */
    LV00_CONTEXT_COMPLETE
} Lv00ContextState;

/**
 * @brief 获取状态机的可读字符串名称
 * @param state 上下文状态枚举值
 * @return 状态的中文名称字符串（静态存储，无需释放）
 */
const char *lv00_context_state_name(Lv00ContextState state);

/**
 * @brief 检查状态转移是否合法
 * @param from 当前状态
 * @param to   目标状态
 * @return true 如果转移合法，false 如果不允许
 */
bool lv00_context_state_transition_valid(Lv00ContextState from, Lv00ContextState to);

/* ============================================================
 * 第二部分：推理分支栈
 *
 * 推理过程中，前向证明、反证法、假设引入等操作会创建推理分支。
 * 每个分支保存当前推理的快照，以便在分支失败时回滚。
 *
 * 分支栈支持以下推理策略：
 *   - 前向证明 (forward proof)：    从已知前提出发，逐步推导结论。
 *                                    每个推导步骤记录在栈帧中。
 *   - 反证法 (proof by contradiction)：假设命题为假，推导矛盾。
 *                                    假设状态保存在栈帧中。
 *   - 假设引入 (hypothesis introduction)：引入临时假设进行推理。
 *                                    若假设不成立则回滚。
 *
 * 分支栈的每个帧 (ReasoningFrame) 记录了进入该分支时的完整状态快照。
 * 当分支推理失败时，通过 lv00_context_rollback() 回滚到上一个栈帧。
 * ============================================================ */

/**
 * @brief 推理分支类型
 */
typedef enum {
    REASONING_BRANCH_NONE,         /**< 无分支（主推理线） */
    REASONING_BRANCH_FORWARD,      /**< 前向证明分支 */
    REASONING_BRANCH_CONTRADICTION, /**< 反证法分支：假设否定命题成立 */
    REASONING_BRANCH_HYPOTHESIS    /**< 假设引入分支：引入临时假设 */
} ReasoningBranchType;

/**
 * @brief 推理分支状态
 */
typedef enum {
    BRANCH_ACTIVE,    /**< 分支仍在推理中 */
    BRANCH_CLOSED,    /**< 分支已闭合（到达目标） */
    BRANCH_FAILED,    /**< 分支推理失败 */
    BRANCH_ABANDONED  /**< 分支被放弃（超时/熔断） */
} ReasoningBranchStatus;

/**
 * @brief 推理栈帧 —— 记录进入一个推理分支时的状态
 *
 * 每个分支帧包含：
 * - 分支类型和状态，用于追踪推理路径
 * - 分支入口处约束图的深拷贝（用于回滚）
 * - 推理步骤计数器，防止无限深度
 * - 该分支特定的假设列表和目标列表
 */
typedef struct ReasoningFrame {
    /** 分支类型：前向证明 / 反证法 / 假设引入 */
    ReasoningBranchType branch_type;

    /** 分支当前状态 */
    ReasoningBranchStatus status;

    /** 该分支的推理深度（相对于根上下文） */
    int depth;

    /** 该分支内执行的推理步骤计数 */
    int step_count;

    /** 分支入口处约束图的深拷贝快照（回滚目标） */
    struct ConstraintGraph *graph_snapshot;

    /** 该分支引入的假设节点 ID 数组（以 -1 结尾） */
    int *assumption_node_ids;

    /** 假设数量 */
    int assumption_count;

    /** 该分支的目标节点 ID 数组（以 -1 结尾） */
    int *target_node_ids;

    /** 目标数量 */
    int target_count;

    /** 该分支的 AST 语法树根节点浅拷贝（指向上下文 AST 的引用） */
    void *ast_root_ref;

    /** 分支创建时间戳（微秒，用于超时判定） */
    uint64_t created_at_us;

    /** 分支的自行超时时间（毫秒，0 = 继承父上下文超时） */
    uint64_t timeout_ms;

    /** 用户自留的数据指针（用于扩展） */
    void *user_data;
} ReasoningFrame;

/**
 * @brief 推理分支栈 —— 管理推理路径的栈结构
 *
 * 最大深度由 LV00_CONTEXT_MAX_REASONING_DEPTH 限制。
 * 当栈满时，新的分支创建会失败并返回错误码。
 */
typedef struct ReasoningStack {
    /** 栈帧数组（动态增长，但不超过 max_depth） */
    ReasoningFrame *frames;

    /** 当前栈顶索引（-1 表示空栈，0 表示根帧） */
    int top;

    /** 栈当前容量 */
    int capacity;

    /** 栈绝对最大深度（在上下文创建时设定） */
    int max_depth;
} ReasoningStack;

/* ============================================================
 * 第三部分：熔断器（Circuit Breaker）
 *
 * 借鉴分布式系统中的熔断器模式，防止单次计算失控：
 *
 *   关闭态 (CLOSED) ── 错误次数超阈值 ──→ 打开态 (OPEN)
 *       ↑                                      │
 *       │                                      │ 冷却时间过后
 *       │                                      ↓
 *       └────────── 尝试调用成功 ←── 半开态 (HALF_OPEN)
 *
 * 在 Lv-00 中，熔断器监控以下维度：
 *   - 时间熔断：单次操作超时
 *   - 深度熔断：递归/重写深度超限
 *   - 次数熔断：推理步数超限
 *   - 内存熔断：内存使用超限
 *   - 错误熔断：连续错误次数超限
 *
 * 任一维度触发，上下文转入 CONTEXT_ERROR 状态。
 * ============================================================ */

/**
 * @brief 熔断器状态
 */
typedef enum {
    CIRCUIT_BREAKER_CLOSED,      /**< 关闭态：正常工作 */
    CIRCUIT_BREAKER_HALF_OPEN,   /**< 半开态：冷却后试探性恢复 */
    CIRCUIT_BREAKER_OPEN         /**< 打开态：熔断，拒绝执行 */
} CircuitBreakerState;

/**
 * @brief 熔断器 —— 多维熔断保护
 *
 * 熔断器在每个 Lv00Context 中内建，监控关键资源指标。
 * 当任一指标超过阈值时，熔断器打开，上下文转入 ERROR 状态。
 * 熔断后需通过 lv00_context_reset() 重置。
 */
typedef struct CircuitBreaker {
    /** 熔断器当前状态 */
    CircuitBreakerState state;

    /* ── 时间熔断 ── */

    /** 单次操作超时时间（毫秒）。0 表示不限制。 */
    uint64_t timeout_ms;

    /** 上下文创建至今的总运行时间（毫秒）。超时后熔断。 */
    uint64_t total_timeout_ms;

    /** 不可取消区域计数（> 0 表示在关键路径中，超时不触发熔断） */
    int uncancellable_refcount;

    /* ── 深度熔断 ── */

    /** 当前递归/重写/推理的嵌套深度 */
    int current_depth;

    /** 最大允许深度（默认 LV00_CONTEXT_DEFAULT_MAX_DEPTH） */
    int max_depth;

    /* ── 次数熔断 ── */

    /** 已执行的推理步骤总数 */
    int64_t total_steps;

    /** 最大推理步骤数（0 表示不限制） */
    int64_t max_steps;

    /* ── 错误熔断 ── */

    /** 连续错误计数（一次成功操作会清零） */
    int consecutive_errors;

    /** 连续错误上限（超过则熔断） */
    int max_consecutive_errors;

    /* ── 内存熔断 ── */

    /** 内存使用上限（字节，0 表示不限制） */
    size_t max_memory_bytes;

    /* ── 时间测量 ── */

    /** 上下文创建/最近一次 reset 的时间戳（微秒） */
    uint64_t start_time_us;

    /** 当前操作的开始时间（微秒，用于单次超时判断） */
    uint64_t operation_start_us;

    /** 冷却时间（毫秒，熔断后必须等待的最小时间） */
    uint64_t cooldown_ms;

    /** 熔断发生的时间戳（微秒） */
    uint64_t tripped_at_us;

    /** 上次熔断的触发原因（可读字符串，静态存储或动态分配） */
    char *trip_reason;

    /** 熔断次数（生命周期内累计） */
    int trip_count;
} CircuitBreaker;

/* ============================================================
 * 第四部分：上下文主结构 —— Lv00Context
 *
 * Lv00Context 是 Lv-00 项目状态管理的中心容器。
 * 每个独立的几何问题求解任务应拥有自己的上下文实例。
 *
 * 字段分类：
 *   1. 几何对象容器 —— 点、线段、区域、约束等的统一存储
 *   2. AST 语法树 —— 已解析问题的抽象语法表示
 *   3. 代数计算缓存 —— 中间代数计算结果的缓存
 *   4. 推理分支栈 —— 多路径推理的状态管理
 *   5. 运行时参数 —— 配置、超时、资源限制
 *   6. 状态机 —— 生命周期的可追踪状态
 *   7. 熔断器 —— 多维资源保护
 *   8. 快照/回滚 —— 分支证明的支持机制
 *   9. 线程安全 —— 可选的并发访问保护
 * ============================================================ */

/**
 * @brief Lv-00 隔离上下文
 *
 * 这是 Lv-00 从全局引擎模式迁移到隔离上下文模式的核心数据结构。
 *
 * 使用模式：
 *
 * @code
 *   // 1. 创建上下文
 *   Lv00Context *ctx = lv00_context_create();
 *
 *   // 2. 配置参数
 *   lv00_context_set_timeout(ctx, 30000);  // 30 秒超时
 *   lv00_context_set_max_depth(ctx, 50);   // 最大推理深度 50
 *
 *   // 3. 进入解析状态
 *   lv00_context_set_state(ctx, LV00_CONTEXT_PARSING);
 *   // ... 添加几何对象和约束到 ctx->main_graph ...
 *
 *   // 4. 进入推理状态
 *   lv00_context_set_state(ctx, LV00_CONTEXT_REASONING);
 *
 *   // 5. 分支推理（如反证法）
 *   Lv00Context *snapshot = lv00_context_snapshot(ctx);
 *   lv00_context_push_reasoning(ctx, REASONING_BRANCH_CONTRADICTION, ...);
 *   // ... 尝试推导矛盾 ...
 *   bool proved = ...;
 *   if (!proved) {
 *       lv00_context_rollback(ctx, snapshot);  // 回滚到假设前
 *   }
 *
 *   // 6. 完成
 *   lv00_context_set_state(ctx, LV00_CONTEXT_COMPLETE);
 *
 *   // 7. 为下一个问题重置
 *   lv00_context_reset(ctx);
 *
 *   // 8. 销毁
 *   lv00_context_destroy(ctx);
 * @endcode
 *
 * @note 上下文不拥有其引用的外部资源（如模块注册表、公理系统）。
 *       这些共享资源由 lv00_init() / lv00_cleanup() 管理。
 */
typedef struct Lv00Context {
    /* ==================================================================
     * 1. 几何对象容器
     *
     * main_graph 是约束图的核心容器，存放所有几何对象：
     *   - 点（GeomNode with GEOM_POINT）：零维，用符号坐标表示
     *   - 线段（GeomNode with GEOM_LINE_SEGMENT）：一维
     *   - 区域（GeomNode with GEOM_REGION）：二维
     *   - 端口（GeomNode with GEOM_PORT）：函数块接口
     *   - 函数块（GeomNode with GEOM_FUNCTION_BLOCK）：封装构造
     *   - 约束（Constraint）：关联、之间、相交、包含、连接
     *
     * 此容器在上下文创建时分配，在销毁时释放。
     * ================================================================== */
    struct ConstraintGraph *main_graph;

    /* ==================================================================
     * 2. AST 语法树根节点
     *
     * 存储已解析输入的抽象语法树（Abstract Syntax Tree）。
     * 在 PARSING 状态中构建，在 REASONING 状态中遍历和求值。
     *
     * 当前阶段（v3.3.0）为不透明指针，实际类型取决于 DSL 编译器实现。
     * 未来可能为具体的 AST 节点类型。
     * ================================================================== */
    void *ast_root;

    /* ==================================================================
     * 3. 代数计算缓存
     *
     * 缓存中间代数计算结果：
     *   - Groebner 基计算结果（多项式理想基）
     *   - 符号化简结果（代数数、二次根式化简）
     *   - 数值近似表（用于快速数值验证）
     *   - 合一（unification）绑定表
     *
     * 缓存通过 lv00_context_invalidate_cache() 标记为脏，
     * 在下次需要时重新计算。这样可以避免约束图修改后使用过期结果。
     * ================================================================== */

    /** Groebner 基计算结果的缓存（不透明指针，由 solver 模块管理） */
    void *groebner_cache;

    /** 符号化简单布尔/代数结果缓存（不透明指针） */
    void *symbolic_cache;

    /** 数值近似值缓存表（不透明指针） */
    void *numeric_cache;

    /** 合一绑定表缓存（不透明指针，由 unify 模块管理） */
    void *unification_cache;

    /** 缓存是否有效（false = 约束图已修改，需要重新计算） */
    bool cache_valid;

    /** 缓存命中次数（用于统计和调优） */
    int64_t cache_hits;

    /** 缓存未命中次数 */
    int64_t cache_misses;

    /* ==================================================================
     * 4. 推理分支栈
     *
     * 管理多路径推理的状态切换。
     * 当前活跃的推理路径由栈顶帧定义。
     *
     * 栈的使用流程：
     *   1. push: 创建分支快照并入栈（如假设 ¬P 开始反证法）
     *   2. pop:  分支闭合（如找到矛盾，假设 ¬P 被推翻）
     *   3. rollback: 分支失败，恢复到 push 前的状态
     *
     * 每个分支帧包含其独立的约束图快照和推理状态。
     * ================================================================== */
    ReasoningStack reasoning_stack;

    /* ==================================================================
     * 5. 运行时参数
     *
     * 控制上下文行为的可配置参数。
     * 可在 IDLE 状态中随时修改，进入 PARSING/REASONING 后部分冻结。
     * ================================================================== */

    /** 上下文名称（用于调试和日志标识，可为 NULL） */
    char *name;

    /** 当前错误码（线程安全：每个上下文独立） */
    Lv00ErrorCode error_code;

    /** 错误描述文本（定长缓冲区，防止动态分配导致的 OOM 错误循环） */
    char error_message[512];

    /** 最后操作的状态码 */
    int last_status;

    /* ==================================================================
     * 6. 状态机
     *
     * 追踪上下文从创建到销毁的完整生命周期。
     * 状态转移严格受限，非法转移返回错误。
     * ================================================================== */

    /** 当前状态机状态 */
    Lv00ContextState state;

    /** 上一个状态（用于调试和审计） */
    Lv00ContextState previous_state;

    /** 状态转移次数（用于检测异常状态循环） */
    int64_t state_transition_count;

    /* ==================================================================
     * 7. 熔断器（Circuit Breaker）
     *
     * 内建的多维资源保护机制。
     * 任何维度超限都会触发熔断，将上下文转入 ERROR 状态。
     * ================================================================== */
    CircuitBreaker circuit_breaker;

    /* ==================================================================
     * 8. 递归深度追踪
     *
     * 独立于推理栈的递归深度计数器，用于：
     *   - 防止无限递归导致的栈溢出
     *   - 在 DSL 函数调用、合一嵌套、重写嵌套中追踪深度
     *   - 与熔断器的深度熔断联动
     * ================================================================== */

    /** 当前递归深度（函数调用/合一/重写嵌套层数） */
    int recursion_depth;

    /** 递归深度硬上限（默认 LV00_CONTEXT_MAX_RECURSION_DEPTH） */
    int max_recursion_depth;

    /** 递归深度超过安全阈值时的处理策略 */
    enum {
        LV00_RECURSION_POLICY_ERROR,    /**< 直接报错，终止当前操作 */
        LV00_RECURSION_POLICY_DOWNGRADE /**< 尝试降级处理（如用数值近似） */
    } recursion_policy;

    /* ==================================================================
     * 9. 流式输出上下文
     *
     * 用于向前端/日志系统发射实时事件。
     * 为 NULL 时禁用流式输出。
     * ================================================================== */
    struct StreamContext *stream_ctx;

    /* ==================================================================
     * 10. 规范化结果缓存
     *
     * 最近一次图归一化操作的结果。
     * 为 NULL 表示尚未执行或结果已过期。
     * ================================================================== */
    struct NormalizationResult *last_normalization;

    /* ==================================================================
     * 11. 内存池引用
     *
     * 内存池（Memory Pool）用于高效的小对象分配。
     * 当前阶段为不透明指针，指向上下文私有的内存管理结构。
     *
     * 设计意图：
     *   - 减少 malloc/free 的系统调用开销
     *   - 上下文销毁时，可一次性释放整个内存池
     *   - 快照/回滚时，可通过 arena 分配器避免深拷贝
     * ================================================================== */
    void *memory_pool;

    /** 内存统计（当前上下文级别，而非全局级别） */
    MemoryStats mem_stats;

    /* ==================================================================
     * 12. 公理与规则引用
     *
     * 指向共享的模块/公理/规则注册表的引用（不拥有所有权）。
     * 由 lv00_init() 全局初始化，多个上下文共享。
     *
     * 仅在需要隔离的公理环境时（如不同数学理论的证明），
     * 才为上下文创建私有副本。
     * ================================================================== */

    /** 已加载的模块引用（不拥有所有权，指向全局模块注册表） */
    void **module_refs;

    /** 模块引用数量 */
    int module_ref_count;

    /** 已加载的公理包引用（不拥有所有权） */
    void **axiom_pkg_refs;

    /** 公理包引用数量 */
    int axiom_pkg_ref_count;

    /** 重写规则引用（不拥有所有权） */
    struct RewriteRule **rewrite_rule_refs;

    /** 重写规则引用数量 */
    int rewrite_rule_ref_count;

    /** 重写步数上限（默认: 1000） */
    int rewrite_step_limit;

    /* ==================================================================
     * 13. 快照/回滚支持
     *
     * Lv00Context 自身就是可快照/可回滚的。
     *
     * snapshot_stack 存储 lv00_context_snapshot() 创建的上下文快照链。
     * 与 reasoning_stack 不同，snapshot_stack 是通用的状态保存/恢复机制，
     * 不限于推理分支，也可用于：
     *   - "试验性"操作前的保存点
     *   - 并发的 explore/exploit 策略
     *   - 外部工具的 undo/redo 支持
     *
     * 快照是上下文完整的深拷贝（或写时复制引用）。
     * ================================================================== */

    /** 快照引用计数：当前有多少活跃快照指向此上下文的子资源 */
    int snapshot_refcount;

    /** 当前上下文的父快照（回滚目标，NULL 表示无父快照） */
    struct Lv00Context *parent_snapshot;

    /** 快照深度（快照链的嵌套层数） */
    int snapshot_depth;

    /* ==================================================================
     * 14. 线程安全（可选，编译期开关）
     *
     * 通过编译宏 LV00_CONTEXT_THREAD_SAFE 控制是否启用线程安全。
     * 启用后，上下文的关键操作会加锁，允许在多线程环境中安全使用。
     *
     * 默认不启用，因为单线程推理是主要使用场景，
     * 线程安全锁会带来性能开销。
     * ================================================================== */
#ifdef LV00_CONTEXT_THREAD_SAFE

    /** 上下文级别的互斥锁（保护所有可变字段） */
    void *mutex; /* 实际类型为平台相关的互斥锁，使用 void* 避免引入平台头文件 */

    /** 约束图访问锁（细粒度锁，允许并行读取约束图） */
    void *graph_rwlock; /* 实际类型为读写锁 */

    /** 缓存访问锁 */
    void *cache_lock;

#endif /* LV00_CONTEXT_THREAD_SAFE */

    /* ==================================================================
     * 15. 上下文 ID 与统计
     * ================================================================== */

    /** 上下文唯一标识符（全局自增，用于日志追踪） */
    uint64_t context_id;

    /** 上下文创建时间戳（微秒） */
    uint64_t created_at_us;

    /** 当前上下文已处理的问题数量（每次 reset 后 +1 如果前一个问题被标记为 complete） */
    int problems_processed;

    /** 用户扩展数据指针（供外部框架使用，上下文不管理其生命周期） */
    void *user_extension;
} Lv00Context;

/* ============================================================
 * 第五部分：默认常量
 *
 * 上下文创建和操作的默认参数。
 * 这些常量可在上下文创建后通过 setter 函数覆盖。
 * ============================================================ */

/**
 * @brief 默认熔断超时（毫秒）
 *
 * 单次操作超过此时限仍未完成，触发时间熔断。
 * 默认 30 秒，可根据问题复杂度调整。
 */
#define LV00_CONTEXT_DEFAULT_TIMEOUT_MS 30000

/**
 * @brief 默认递归/推理深度上限
 *
 * 防止无限递归导致的栈溢出。
 * 默认 100，对于极深的问题可提高到 1000。
 */
#define LV00_CONTEXT_DEFAULT_MAX_DEPTH 100

/**
 * @brief 递归深度绝对硬上限
 *
 * 这是无论如何不能被超越的深度硬限制，
 * 由系统栈大小和内存约束决定。
 *
 * @note 设为 10000 而非更大的值，是因为 C 语言默认栈大小
 *       通常为 1-8 MB，每层递归帧可能消耗数百字节到数 KB，
 *       过深的递归会导致栈溢出（Stack Overflow），引发未定义行为。
 */
#define LV00_CONTEXT_MAX_RECURSION_DEPTH 10000

/**
 * @brief 默认最大推理步骤数
 *
 * 超过此步数仍未完成推理，触发次数熔断。
 * 0 表示不限制。
 */
#define LV00_CONTEXT_DEFAULT_MAX_STEPS 1000000

/**
 * @brief 默认连续错误上限
 *
 * 连续发生此数量的错误后触发错误熔断。
 */
#define LV00_CONTEXT_DEFAULT_MAX_CONSECUTIVE_ERRORS 10

/**
 * @brief 熔断器默认冷却时间（毫秒）
 *
 * 熔断器打开后必须等待此时长才能进入半开态。
 */
#define LV00_CONTEXT_DEFAULT_COOLDOWN_MS 5000

/**
 * @brief 推理栈默认初始容量
 */
#define LV00_CONTEXT_REASONING_STACK_DEFAULT_CAPACITY 8

/**
 * @brief 推理栈最大深度上限
 */
#define LV00_CONTEXT_REASONING_STACK_MAX_DEPTH 1000

/* ============================================================
 * 第六部分：生命周期管理 API
 *
 * 上下文从创建到销毁的完整生命周期：
 *
 *   1. lv00_context_create()      —— 分配并初始化上下文
 *   2. lv00_context_reset()       —— 清空状态，准备下一个问题
 *   3. lv00_context_destroy()     —— 释放所有资源
 *
 * 快照/回滚辅助：
 *   4. lv00_context_snapshot()    —— 保存当前状态
 *   5. lv00_context_rollback()    —— 恢复到快照状态
 * ============================================================ */

/**
 * @brief 创建并初始化一个新的隔离上下文
 *
 * 分配 Lv00Context 结构体并设置默认值：
 * - 状态机初始化为 LV00_CONTEXT_IDLE
 * - 分配空的约束图 (ConstraintGraph)
 * - 初始化熔断器（30s 超时，深度上限 100）
 * - 初始化推理栈（空栈，容量 8）
 * - 分配流式输出上下文 (StreamContext)
 * - 分配全局唯一 ID
 *
 * @return 新上下文的指针。如果内存分配失败，返回 NULL 并通过
 *         lv00_get_last_error_code() 设置错误码 LV00_ERROR_OUT_OF_MEMORY。
 *
 * @note 调用者负责最终调用 lv00_context_destroy() 释放。不使用时
 *       必须销毁，否则会泄漏约束图、缓存和栈帧所占用的内存。
 *
 * @note 上下文创建是线程安全的：每个线程可以独立创建自己的上下文实例。
 */
Lv00Context *lv00_context_create(void);

/**
 * @brief 销毁上下文，释放所有关联资源
 *
 * 释放的资源包括（按顺序）：
 * 1. 约束图 (ConstraintGraph)
 * 2. 推理栈中的所有帧（包括每帧的约束图快照）
 * 3. 代数计算缓存（Groebner 基、符号化简、数值近似、合一表）
 * 4. 流式输出上下文
 * 5. 规范化结果
 * 6. 名称字符串
 * 7. 熔断器错误原因字符串
 * 8. 线程安全锁（如启用）
 * 9. 上下文结构体本身
 *
 * @param ctx 要销毁的上下文指针（可为 NULL，此时函数为空操作）。
 *
 * @note 销毁不释放上下文引用的共享资源（全局模块注册表、公理系统）。
 *
 * @note 如果有活跃的子快照仍引用此上下文（snapshot_refcount > 0），
 *       销毁会触发断言或警告，因为这意味着存在悬空指针。建议先
 *       销毁所有子快照或通过 rollback 整合后再销毁父上下文。
 */
void lv00_context_destroy(Lv00Context *ctx);

/**
 * @brief 创建当前上下文的完整快照（深拷贝）
 *
 * 快照包含当前上下文的完整状态副本：
 * - 约束图的深拷贝（包括所有节点和约束）
 * - 推理栈的深拷贝（包括每帧的约束图快照）
 * - 熔断器状态的拷贝
 * - 状态机状态的拷贝
 * - 缓存状态标记（但不深拷贝缓存内容）
 *
 * 快照的典型用途：
 * 1. 分支推理前保存状态，分支失败时回滚
 * 2. 试验性操作前的保存点
 * 3. Undo/Redo 栈的构造
 *
 * @param ctx 源上下文（不能为 NULL）
 * @return 新分配的不可变快照（Lv00Context*，与源上下文类型相同）。
 *         失败返回 NULL，错误码通过 lv00_get_last_error_code() 获取。
 *
 * @note 快照也是完整的 Lv00Context 结构体，可以通过
 *       lv00_context_rollback() 恢复到快照状态。
 *       快照的 snapshot_refcount 和 parent_snapshot 会自动设置。
 *
 * @warning 快照是深拷贝操作，对于大规模约束图可能开销较大。
 *          未来版本可能实现写时复制（Copy-on-Write）优化。
 */
Lv00Context *lv00_context_snapshot(Lv00Context *ctx);

/**
 * @brief 将上下文状态回滚到指定的快照
 *
 * 用快照中的状态替换上下文的当前状态：
 * - 释放当前上下文的约束图，替换为快照的约束图副本
 * - 释放当前推理栈，替换为快照的推理栈副本
 * - 恢复熔断器状态（但熔断计数保留）
 * - 恢复状态机状态
 * - 缓存失效标记设置为脏
 *
 * @param ctx      要回滚的上下文（非 NULL）
 * @param snapshot 之前通过 lv00_context_snapshot() 创建的快照（非 NULL）
 * @return true 成功，false 失败（参数为空或回滚过程中资源分配失败）。
 *         失败时上下文保持原状态不变，错误码已设置。
 *
 * @note 回滚成功后，snapshot 不会被自动释放。调用者如果需要释放快照，
 *       应调用 lv00_context_destroy(snapshot)。
 *
 * @note 回滚操作保留以下字段不受影响：
 *       - context_id（保持唯一性）
 *       - problems_processed（不减少）
 *       - trip_count（熔断次数累计保留，不过度乐观）
 *
 * @warning 如果 ctx 和 snapshot 不是从同一快照链派生的，
 *          回滚可能导致不一致状态。调用者应确保 snapshot 是
 *          ctx 或 ctx 祖先的直接快照。
 */
bool lv00_context_rollback(Lv00Context *ctx, Lv00Context *snapshot);

/**
 * @brief 重置上下文，清除所有问题特定状态
 *
 * 将上下文恢复到刚创建时的"干净"状态，准备处理下一个问题：
 * - 状态机恢复到 LV00_CONTEXT_IDLE
 * - 销毁并重建约束图（空图）
 * - 清空推理栈
 * - 清空代数计算缓存
 * - 重置熔断器（但保留 trip_count 用于统计）
 * - 递归深度归零
 * - 清除错误状态
 * - 释放规范化结果
 *
 * @param ctx 要重置的上下文（非 NULL）
 *
 * @note 重置不改变以下字段：
 *       - context_id（保持唯一性）
 *       - 模块/公理/规则引用（保持加载状态，除非在 reset 时主动卸载）
 *       - 流式上下文（保持回调注册）
 *       - 用户扩展指针
 *       - 熔断器 trip_count（保留历史统计）
 *
 * @note 这是推荐的在问题之间切换的方式，比销毁再创建更高效。
 */
void lv00_context_reset(Lv00Context *ctx);

/* ============================================================
 * 第七部分：状态机管理 API
 * ============================================================ */

/**
 * @brief 获取上下文当前状态
 * @param ctx 上下文（可为 NULL，返回 LV00_CONTEXT_IDLE）
 * @return 当前状态机状态
 */
Lv00ContextState lv00_context_get_state(const Lv00Context *ctx);

/**
 * @brief 尝试将上下文转移到指定状态
 *
 * 此函数会验证状态转移的合法性。
 * 非法转移将返回错误，不改变上下文状态。
 *
 * @param ctx      上下文（非 NULL）
 * @param new_state 目标状态
 * @return LV00_OK 成功，LV00_ERROR_INVALID_STATE 非法转移
 */
Lv00ErrorCode lv00_context_set_state(Lv00Context *ctx, Lv00ContextState new_state);

/* ============================================================
 * 第八部分：推理栈 API
 * ============================================================ */

/**
 * @brief 在当前推理栈上压入一个新分支帧
 *
 * 压入分支帧前会自动创建当前约束图的快照并保存在帧中。
 * 如果启用了熔断器且深度超限，此函数将失败。
 *
 * @param ctx         上下文（非 NULL）
 * @param branch_type 分支类型（前向证明 / 反证法 / 假设引入）
 * @param timeout_ms  该分支的独立超时（0 = 继承父上下文超时）
 * @return LV00_OK 成功，其他错误码表示失败
 */
Lv00ErrorCode lv00_context_push_reasoning(Lv00Context *ctx, ReasoningBranchType branch_type, uint64_t timeout_ms);

/**
 * @brief 从推理栈弹出栈顶帧（分支闭合）
 *
 * 如果分支成功闭合（status = BRANCH_CLOSED），弹出并释放该帧。
 * 如果分支失败（status = BRANCH_FAILED），调用者应先回滚再弹出。
 *
 * @param ctx 上下文（非 NULL）
 * @return LV00_OK 成功，LV00_ERROR_INVALID_STATE 栈为空时失败
 */
Lv00ErrorCode lv00_context_pop_reasoning(Lv00Context *ctx);

/**
 * @brief 获取当前推理栈深度
 * @param ctx 上下文（可为 NULL，返回 0）
 * @return 栈中帧的数量（0 = 主推理线，>= 1 = 至少一个分支）
 */
int lv00_context_get_reasoning_depth(const Lv00Context *ctx);

/**
 * @brief 获取当前活跃的推理分支帧（栈顶）
 * @param ctx 上下文（非 NULL）
 * @return 栈顶帧指针（栈为空时返回 NULL）
 */
ReasoningFrame *lv00_context_get_current_reasoning_frame(Lv00Context *ctx);

/* ============================================================
 * 第九部分：熔断器 API
 * ============================================================ */

/**
 * @brief 检查熔断器是否已触发
 *
 * 此函数应在每个推理步骤之前调用。
 * 如果熔断器打开，上下文应停止当前操作并转入 ERROR 状态。
 *
 * @param ctx 上下文（非 NULL）
 * @return true 熔断器打开（应停止操作），false 正常工作
 */
bool lv00_context_is_circuit_open(const Lv00Context *ctx);

/**
 * @brief 开始一次可熔断操作（设置操作开始时间）
 *
 * 应在每个可能耗时较长的操作开始前调用。
 * 与 lv00_context_check_timeout() 配对使用。
 *
 * @param ctx 上下文（非 NULL）
 */
void lv00_context_begin_operation(Lv00Context *ctx);

/**
 * @brief 检查当前操作是否超时
 *
 * 比较当前时间与操作开始时间。超时则触发熔断器。
 *
 * @param ctx 上下文（非 NULL）
 * @return true 已超时（熔断器已打开），false 正常
 */
bool lv00_context_check_timeout(Lv00Context *ctx);

/**
 * @brief 进入不可取消区域（uncancellable section）
 *
 * 在关键路径（如内存释放、状态提交）中调用此函数，
 * 暂时阻止超时熔断。必须与 lv00_context_leave_uncancellable() 配对。
 *
 * @param ctx 上下文（非 NULL）
 */
void lv00_context_enter_uncancellable(Lv00Context *ctx);

/**
 * @brief 离开不可取消区域
 * @param ctx 上下文（非 NULL）
 */
void lv00_context_leave_uncancellable(Lv00Context *ctx);

/**
 * @brief 记录一次推理步骤
 *
 * 递增步骤计数，如果超过上限则触发熔断。
 *
 * @param ctx 上下文（非 NULL）
 * @return true 步骤在限制内，false 超限触发熔断
 */
bool lv00_context_record_step(Lv00Context *ctx);

/**
 * @brief 记录一次成功操作（重置连续错误计数）
 * @param ctx 上下文（非 NULL）
 */
void lv00_context_record_success(Lv00Context *ctx);

/**
 * @brief 记录一次错误操作（递增连续错误计数）
 *
 * 如果连续错误超过上限，触发熔断。
 *
 * @param ctx 上下文（非 NULL）
 * @return true 正常，false 连续错误超限触发熔断
 */
bool lv00_context_record_error(Lv00Context *ctx);

/* ============================================================
 * 第十部分：参数配置 API
 * ============================================================ */

/**
 * @brief 设置上下文超时时间
 * @param ctx        上下文（非 NULL）
 * @param timeout_ms 超时时间（毫秒），0 = 不限制
 */
void lv00_context_set_timeout(Lv00Context *ctx, uint64_t timeout_ms);

/**
 * @brief 获取上下文超时时间
 * @param ctx 上下文（可为 NULL，返回 0）
 * @return 超时时间（毫秒）
 */
uint64_t lv00_context_get_timeout(const Lv00Context *ctx);

/**
 * @brief 设置递归/推理深度上限
 * @param ctx      上下文（非 NULL）
 * @param max_depth 最大深度（必须 >= 1，<= LV00_CONTEXT_MAX_RECURSION_DEPTH）
 */
void lv00_context_set_max_depth(Lv00Context *ctx, int max_depth);

/**
 * @brief 获取递归/推理深度上限
 * @param ctx 上下文（可为 NULL，返回默认值）
 * @return 最大深度
 */
int lv00_context_get_max_depth(const Lv00Context *ctx);

/**
 * @brief 设置最大推理步骤数
 * @param ctx       上下文（非 NULL）
 * @param max_steps 最大步骤数，0 = 不限制
 */
void lv00_context_set_max_steps(Lv00Context *ctx, int64_t max_steps);

/**
 * @brief 获取最大推理步骤数
 * @param ctx 上下文（可为 NULL，返回 0）
 * @return 最大步骤数
 */
int64_t lv00_context_get_max_steps(const Lv00Context *ctx);

/**
 * @brief 设置上下文名称（用于日志标识）
 * @param ctx  上下文（非 NULL）
 * @param name 名称字符串（内部复制）
 */
void lv00_context_set_name(Lv00Context *ctx, const char *name);

/**
 * @brief 获取上下文名称
 * @param ctx 上下文（可为 NULL，返回 "null"）
 * @return 名称字符串（内部存储，勿释放）
 */
const char *lv00_context_get_name(const Lv00Context *ctx);

/**
 * @brief 获取上下文 ID
 * @param ctx 上下文（可为 NULL，返回 0）
 * @return 上下文唯一 ID
 */
uint64_t lv00_context_get_id(const Lv00Context *ctx);

/* ============================================================
 * 第十一部分：缓存管理 API
 * ============================================================ */

/**
 * @brief 标记所有缓存为无效
 *
 * 当约束图被修改后调用此函数，
 * 确保下次查询时重新计算而非使用过期结果。
 *
 * @param ctx 上下文（非 NULL）
 */
void lv00_context_invalidate_cache(Lv00Context *ctx);

/**
 * @brief 检查缓存是否有效
 * @param ctx 上下文（非 NULL）
 * @return true 缓存有效可用，false 需要重新计算
 */
bool lv00_context_is_cache_valid(const Lv00Context *ctx);

/**
 * @brief 清除所有缓存内容（释放内存但保留缓存结构）
 * @param ctx 上下文（非 NULL）
 */
void lv00_context_clear_cache(Lv00Context *ctx);

/* ============================================================
 * 第十二部分：错误管理 API
 * ============================================================ */

/**
 * @brief 设置上下文的错误状态
 *
 * 设置错误码和描述消息，同时触发状态机转入 ERROR 状态。
 *
 * @param ctx   上下文（非 NULL）
 * @param code  错误码
 * @param fmt   错误描述格式字符串
 * @param ...   格式参数
 */
void lv00_context_set_error(Lv00Context *ctx, Lv00ErrorCode code, const char *fmt, ...);

/**
 * @brief 清除上下文的错误状态
 *
 * 将错误码重置为 LV00_OK，清空错误消息。
 * 不改变状态机（调用者应同时调用 lv00_context_set_state 转回 IDLE）。
 *
 * @param ctx 上下文（非 NULL）
 */
void lv00_context_clear_error(Lv00Context *ctx);

/**
 * @brief 获取上下文的错误码
 * @param ctx 上下文（可为 NULL）
 * @return 错误码（ctx 为 NULL 时返回 LV00_ERROR_NULL_POINTER）
 */
Lv00ErrorCode lv00_context_get_error_code(const Lv00Context *ctx);

/**
 * @brief 获取上下文的错误消息
 * @param ctx 上下文（可为 NULL）
 * @return 错误消息字符串（内部存储，勿释放。ctx 为 NULL 时返回 "null context"）
 */
const char *lv00_context_get_error_message(const Lv00Context *ctx);

/* ============================================================
 * 第十三部分：流式输出 API
 * ============================================================ */

/**
 * @brief 获取上下文的流式输出上下文
 * @param ctx 上下文（非 NULL）
 * @return 流式上下文指针
 */
struct StreamContext *lv00_context_get_stream(Lv00Context *ctx);

/**
 * @brief 设置流式输出的启用状态
 * @param ctx     上下文（非 NULL）
 * @param enabled true 启用，false 禁用
 */
void lv00_context_set_streaming_enabled(Lv00Context *ctx, bool enabled);

/**
 * @brief 检查流式输出是否启用
 * @param ctx 上下文（可为 NULL，返回 false）
 * @return true 启用
 */
bool lv00_context_is_streaming_enabled(const Lv00Context *ctx);

/* ============================================================
 * 第十四部分：统计与调试 API
 * ============================================================ */

/**
 * @brief 获取上下文统计信息的可读摘要
 *
 * 将上下文的关键统计指标格式化为可读字符串：
 * - 上下文 ID 和名称
 * - 当前状态
 * - 推理栈深度
 * - 步骤总数
 * - 缓存命中率
 * - 熔断状态
 * - 已处理问题数
 * - 运行时间
 *
 * @param ctx      上下文（非 NULL）
 * @param buf      输出缓冲区
 * @param buf_size 缓冲区大小（建议至少 1024 字节）
 * @return 实际写入的字符数（不含终止符）
 */
int lv00_context_get_stats(const Lv00Context *ctx, char *buf, size_t buf_size);

/**
 * @brief 获取上下文的运行时间（微秒）
 * @param ctx 上下文（非 NULL）
 * @return 自创建或最近一次 reset 以来的微秒数
 */
uint64_t lv00_context_get_uptime_us(const Lv00Context *ctx);

#ifdef __cplusplus
}
#endif

#endif /* LV00_CONTEXT_H */

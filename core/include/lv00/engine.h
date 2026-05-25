/**
 * @file engine.h
 * @brief Lv-00 主引擎 —— 工作流编排、模块/公理加载、重写与求解
 *
 * 提供引擎的创建/销毁、模块与公理包加载、函数打包与实例化、
 * 重写-求解协作流程、位电路跳闸处理以及冻结点快照回滚机制。
 *
 * 【中文模块说明】
 * engine.h 是 Lv-00 系统的核心调度模块，负责协调各子系统的工作流程。
 * 主要功能包括：
 * - 引擎生命周期管理（创建、销毁、状态机控制）
 * - 模块与公理包的动态加载
 * - 重写规则注册与管理
 * - 重写-求解协作流水线（先重写简化约束，再调用求解器求解）
 * - 位电路跳闸处理（当符号计算精度超限时触发熔断器）
 * - 冻结点快照与回滚（支持撤销到之前的约束图状态）
 * - 五状态机形式化管理（IDLE → PARSING → REASONING → COMPLETE/ERROR）
 * - 流式输出集成（通过 StreamContext 实时推送引擎事件）
 * - 五层架构层级验证（编译时和运行时检查跨层调用合法性）
 */

#ifndef LV00_ENGINE_H
#define LV00_ENGINE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "config.h"
#include "axiom_pkg.h"
#include "constraint_graph.h"
#include "func_block.h"
#include "module.h"
#include "normalization.h"
#include "rewrite.h"
#include "solver.h"
#include "stream.h"
#include "unify.h"

/* ============================================================
 * 五层架构层级标识（v3.3）
 * ============================================================
 * 用于编译时层级边界检查和运行时诊断。
 * 详见 docs/ARCHITECTURE_v3.3.md
 *
 * 使用方式：
 *   在每个 .c 文件开头（#include 之前），通过 CMake 的
 *   target_compile_definitions 自动设置 LV00_CURRENT_LAYER。
 *   无需手动定义。
 * ============================================================ */

/** @brief Layer 1: 输入解析层 — 词法分析、公式解析、DSL 编译 */
#define LV00_LAYER_PARSER    1

/** @brief Layer 2: 资源管理层 — 内存分配、错误码、调试、工具函数 */
#define LV00_LAYER_RESOURCE  2

/** @brief Layer 3: 几何拓扑层 — 约束图、符号坐标、几何原语 */
#define LV00_LAYER_GEOMETRY  3

/** @brief Layer 4: 公理推理层 — 引擎、求解器、证明、重写、合一 */
#define LV00_LAYER_REASONING 4

/** @brief Layer 5: 结果输出层 — 流式输出、TikZ 导出、互操作 */
#define LV00_LAYER_OUTPUT    5

/* ── 层级验证开关 ──
 * 通过 CMake 选项 ENABLE_LAYER_VALIDATION 控制。
 * 启用后，LV00_ENABLE_LAYER_VALIDATION 和 LV00_CURRENT_LAYER
 * 会被自动定义在每个层的编译单元中。
 */
#ifdef LV00_ENABLE_LAYER_VALIDATION

/* 确保 LV00_CURRENT_LAYER 已被 CMake 定义 */
#ifndef LV00_CURRENT_LAYER
#error "LV00_CURRENT_LAYER must be defined when LV00_ENABLE_LAYER_VALIDATION is enabled. \
Check that the source file belongs to a CMake layer target (lv00_layerN_*)."
#endif

/**
 * @brief 编译时层级边界断言
 *
 * 在源文件头部使用此宏声明当前编译单元允许调用的最低层级。
 * 例如，Layer 4 的代码可以使用 LV00_ALLOW_LAYER(2) 来声明
 * 它可以调用 Layer 2 及以上的代码。
 *
 * @param min_layer 允许的最低层级编号（1-5，数字越小层级越低）
 *
 * 使用示例：
 *   // 在 Layer 4 (reasoning) 的源文件中：
 *   LV00_ALLOW_LAYER(LV00_LAYER_RESOURCE);  // 允许调用 Layer 2+
 *   LV00_ALLOW_LAYER(LV00_LAYER_GEOMETRY);  // 允许调用 Layer 3+
 *
 * @note 此宏在编译时通过 _Static_assert 检查，不产生运行时代码。
 * @note 当 LV00_ENABLE_LAYER_VALIDATION 未定义时，此宏为空操作。
 */
#define LV00_ALLOW_LAYER(min_layer) \
    _Static_assert(LV00_CURRENT_LAYER >= (min_layer), \
        "LV00 layer boundary violation: layer " #LV00_CURRENT_LAYER \
        " may not call functions from layer " #min_layer \
        " (only upper layers may call lower layers)." \
        " See docs/ARCHITECTURE_v3.3.md")

/**
 * @brief 编译时断言：当前层可以直接调用目标层
 *
 * 更严格的检查：要求当前层必须高于目标层至少 1 级
 * （即禁止同层调用，允许跨层向下调用）。
 *
 * @param target_layer 目标层级编号
 */
#define LV00_REQUIRE_STRICTLY_ABOVE(target_layer) \
    _Static_assert(LV00_CURRENT_LAYER > (target_layer), \
        "LV00 layer boundary violation: layer " #LV00_CURRENT_LAYER \
        " must be strictly above layer " #target_layer)

#else
/* 未启用层级验证时，所有检查宏均为空操作 */
#define LV00_ALLOW_LAYER(min_layer)                 ((void)0)
#define LV00_REQUIRE_STRICTLY_ABOVE(target_layer)   ((void)0)
#endif /* LV00_ENABLE_LAYER_VALIDATION */

/**
 * @brief 层级验证标志
 *
 * 引擎实例可以通过此标志决定是否在运行时执行层级边界检查。
 * 默认关闭（仅影响运行时诊断，不影响编译时 _Static_assert）。
 *
 * 启用后，跨层函数调用会检查调用栈是否合规，
 * 违规时通过 engine 的错误报告机制发出警告。
 */
#define LV00_LAYER_VALIDATION_FLAG_NONE     0x00  /**< 不执行层级验证 */
#define LV00_LAYER_VALIDATION_FLAG_RUNTIME  0x01  /**< 运行时调用栈检查 */
#define LV00_LAYER_VALIDATION_FLAG_STRICT   0x02  /**< 严格模式：违规即中止 */

/* 前向声明 —— Lv00Context 定义在 context.h 中，避免循环依赖 */
struct Lv00Context;

/* ── 引擎状态码（必须在 LV00Engine 结构体之前定义）──
 * 【枚举值命名规范】所有枚举值使用 UPPER_SNAKE_CASE
 */
typedef enum {
    ENGINE_STATUS_OK,                 /**< 操作成功完成 */
    ENGINE_STATUS_OUT_OF_MEMORY,      /**< 内存分配失败 */
    ENGINE_STATUS_INVALID_STATE,      /**< 引擎处于无效状态（如未初始化即调用） */
    ENGINE_STATUS_INVALID_ARGUMENT,   /**< 传入参数无效（空指针、越界等） */
    ENGINE_STATUS_CONSTRAINT_CONFLICT,/**< 约束冲突：无法满足的约束条件 */
    ENGINE_STATUS_MODULE_ERROR,       /**< 模块加载/执行错误 */
    ENGINE_STATUS_ERROR_INTERNAL      /**< 内部错误 */
} EngineStatus;

/* ============================================================
 * 五状态引擎状态机（v3.3.0 形式化）
 *
 * 引擎从创建到销毁经历以下状态的严格转移：
 *
 *                   ┌──────────────┐
 *                   │    IDLE      │  <── 初始状态（刚创建/重置后）
 *                   └──────┬───────┘
 *                          │ 开始解析输入
 *                          ▼
 *                   ┌──────────────┐
 *              ┌─── │   PARSING    │ ── 解析失败 ──→ ERROR
 *              │    └──────┬───────┘
 *              │           │ 解析完成，开始推理
 *              │           ▼
 *              │    ┌──────────────┐
 *              │    │  REASONING   │ ── 矛盾/超时 ──→ ERROR
 *              │    └──────┬───────┘
 *              │           │ 证明成功
 *              │           ▼
 *              │    ┌──────────────┐
 *              └───→│  COMPLETE    │ ── 重置 ──→ IDLE
 *                   └──────────────┘
 *                        ↑
 *                   ┌──────────────┐
 *                   │    ERROR     │ ── 重置 ──→ IDLE
 *                   └──────────────┘
 *
 * 状态转移规则（严格）：
 *   IDLE      → PARSING   (开始接收新输入)
 *   IDLE      → ERROR     (初始化失败)
 *   PARSING   → REASONING (解析成功完成)
 *   PARSING   → ERROR     (解析失败)
 *   PARSING   → IDLE      (取消/中断)
 *   REASONING → COMPLETE  (证明/求解成功)
 *   REASONING → ERROR     (矛盾/超时/资源耗尽)
 *   REASONING → IDLE      (取消/中断)
 *   COMPLETE  → IDLE      (重置，准备新问题)
 *   ERROR     → IDLE      (重置，清理错误状态)
 * ============================================================ */

/**
 * @brief 引擎状态机枚举 —— 与 context.h 中 Lv00ContextState 语义对齐
 *
 * 引擎在其生命周期内严格遵循这些状态的转移规则。
 * 任何非法转移都将被拒绝并返回错误码。
 *
 * 【枚举值命名规范】所有枚举值使用 UPPER_SNAKE_CASE
 */
typedef enum {
    /** 空闲状态 —— 引擎已创建但尚未开始处理任何问题。
     *  可接收输入、加载模块/公理、设置参数。 */
    ENGINE_STATE_IDLE = 0,

    /** 解析状态 —— 正在将输入文本/DSL 解析为内部约束图结构。
     *  可逐步添加几何对象和约束。 */
    ENGINE_STATE_PARSING,

    /** 推理状态 —— 正在执行重写、求解、合一、证明等推理操作。
     *  禁止修改约束图拓扑（仅允许变量绑定和代数计算）。 */
    ENGINE_STATE_REASONING,

    /** 错误状态 —— 遇到不可恢复的错误（解析失败、约束冲突、资源耗尽等）。
     *  必须通过 engine_reset() 清理后重新开始。 */
    ENGINE_STATE_ERROR,

    /** 完成状态 —— 当前问题的求解/证明已成功完成。
     *  可查询结果，或通过 engine_reset() 开始新问题。 */
    ENGINE_STATE_COMPLETE
} EngineState;

/**
 * @struct LV00Engine
 * @brief 引擎核心状态 —— Lv-00 系统的中央调度器
 *
 * 【封装性说明 —— 内部实现细节警告】
 *   LV00Engine 结构体的字段定义属于引擎的内部实现细节。
 *   外部代码（非 engine.c）不应直接访问或修改这些字段，否则可能
 *   破坏引擎内部不变量，导致未定义行为。
 *
 *   推荐做法：
 *   - 使用 engine.h 中声明的公共 API 函数操作引擎实例
 *   - 通过 engine_get_state()、engine_get_last_status() 等访问器获取状态
 *   - 不要直接读写 engine->state、engine->last_status 等字段
 *
 *   当前保留结构体字段在头文件中可见是为了：
 *   1. 避免破坏现有 ABI（已发布的二进制兼容性）
 *   2. 允许内联性能关键路径中的快速状态检查（需谨慎使用）
 *   3. 为未来迁移到不透明指针（opaque pointer）模式预留过渡期
 *
 *   长期计划：当条件成熟时，将结构体定义迁移到 engine.c 内部，
 *   头文件仅保留前向声明（typedef struct LV00Engine LV00Engine;）。
 *
 * LV00Engine 是整个 Lv-00 系统的核心数据结构，持有约束图、模块、公理包、
 * 重写规则等所有子系统资源，并协调它们之间的工作流程。
 *
 * 【字段分组说明】
 *   1. 核心数据容器组：
 *      - main_graph: 主约束图，所有几何元素与约束的统一容器
 *      - loaded_modules / axiom_packages / rewrite_rules: 动态资源数组
 *      - rewrite_step_limit: 可配置的重写步数上限
 *
 *   2. 状态与快照组：
 *      - frozen_point: 位电路跳闸回滚用的冻结点快照（引擎持有所有权）
 *      - last_unify_status: 上一次合一操作的结果状态码
 *
 *   3. 错误报告组：
 *      - last_status: 最近一次操作的 EngineStatus 错误码
 *      - last_error: 最近一次操作的错误描述文本
 *
 *   4. 流式输出组：
 *      - stream_ctx: 可选的流式输出上下文（为 NULL 时不发射事件）
 *
 *   5. 上下文与架构组：
 *      - context: 关联的 Lv00Context 实例指针（v3.3.0 过渡期共存设计）
 *      - layer_validation_flags: 五层架构运行时层级验证标志
 *
 *   6. 状态机组：
 *      - state / previous_state / state_transition_count: 五状态机字段
 *
 * 【线程安全性】
 *   LV00Engine 本身 **不是线程安全的**。所有对引擎实例的调用必须在同一线程中执行。
 *   如果需要在多线程环境中使用，调用者负责通过外部同步机制（如互斥锁）保护引擎实例。
 *   引擎内部的动态数组（模块、公理包、重写规则）在单次 API 调用内是安全的，
 *   但跨调用的并发访问会导致未定义行为。
 *
 * 【生命周期管理】
 *   创建：通过 engine_create() 分配并初始化，初始状态为 ENGINE_STATE_IDLE。
 *   使用：通过 engine_load_module() / engine_load_axiom_package() / engine_add_rewrite_rule()
 *         加载资源，通过 engine_solve() / engine_rewrite_and_solve() 执行求解。
 *   销毁：通过 engine_destroy() 释放所有关联资源（约束图、模块、公理包、重写规则、
 *         冻结点快照、流式上下文），最后释放引擎结构体本身。
 *   注意：engine_destroy() 接受 NULL 参数（空操作），因此无需检查 NULL 后再调用。
 *         销毁后，引擎指针不可再被使用（悬垂指针）。
 */
typedef struct LV00Engine {
    ConstraintGraph *main_graph;    /**< 主约束图指针 —— 引擎的核心数据结构，所有几何元素与约束的容器 */
    Module **loaded_modules;        /**< 已加载模块的动态数组（指针数组） */
    int module_count;               /**< 已加载模块数量 */
    int module_capacity;            /**< 模块数组当前容量（指数增长，初始 LV00_INITIAL_ARRAY_CAPACITY） */
    AxiomPackage **axiom_packages;  /**< 已加载公理包的动态数组（指针数组） */
    int axiom_package_count;        /**< 已加载公理包数量 */
    int axiom_package_capacity;     /**< 公理包数组当前容量（指数增长） */
    RewriteRule **rewrite_rules;    /**< 重写规则的动态数组（指针数组） */
    int rewrite_rule_count;         /**< 已注册重写规则数量 */
    int rewrite_rule_capacity;      /**< 重写规则数组当前容量（指数增长） */

    /* 可配置的重写步数上限（默认: 1000） */
    int rewrite_step_limit;

    /* 位电路跳闸回滚的冻结点快照（由引擎持有所有权） */
    void *frozen_point;

    /* 上一次合一操作的状态码 */
    int last_unify_status;

    /* ── 引擎级别的错误状态（每个引擎实例独立隔离）── */
    EngineStatus last_status; /* 最近一次操作的状态码 */
    char last_error[LV00_CONFIG_ENGINE_ERROR_BUFFER_SIZE]; /**< 最近一次操作的错误描述文本（大小由 config.h 控制） */

    /* 流式输出上下文（可选，为 NULL 时不发射事件） */
    StreamContext *stream_ctx;

    /*
     * ── 隔离上下文指针（v3.3.0 新增）──
     *
     * 指向该引擎关联的 Lv00Context 实例。在"引擎-上下文共存"过渡期内，
     * 引擎通过此指针访问上下文的熔断器、推理栈、缓存等高级功能。
     *
     * 迁移计划（见 engine.c 顶部注释）：
     *   短期：LV00Engine.context 与 Lv00Context 中的 main_graph 共存。
     *   长期：引擎逻辑逐步迁移到 Lv00Context，LV00Engine 降级为薄封装层。
     *
     * 为 NULL 时，引擎工作在传统模式（不启用上下文高级特性）。
     */
    struct Lv00Context *context;

    /*
     * ── 五层架构层级验证标志（v3.3 新增）──
     *
     * 控制运行时层级边界检查的行为。
     * 默认值：LV00_LAYER_VALIDATION_FLAG_NONE（不检查）。
     *
     * 可用值（可位或组合）：
     *   LV00_LAYER_VALIDATION_FLAG_RUNTIME — 启用运行时调用栈检查
     *   LV00_LAYER_VALIDATION_FLAG_STRICT  — 违规时立即 abort()
     *
     * 注意：此标志仅影响运行时检查。编译时检查由
     * LV00_ENABLE_LAYER_VALIDATION / LV00_ALLOW_LAYER 宏控制。
     */
    int layer_validation_flags;

    /*
     * ── 五状态机字段（v3.3.0 形式化）──
     *
     * 引擎在其生命周期内严格遵循状态机转移规则。
     * 所有状态变更必须通过 lv00_engine_transition_state() 执行，
     * 该函数验证转移合法性。非法转移将被拒绝并返回错误码。
     *
     * state:                当前状态（初始为 ENGINE_STATE_IDLE）
     * previous_state:       上一个状态（用于调试和审计追踪）
     * state_transition_count: 状态转移总次数（检测异常状态循环）
     */
    EngineState state;
    EngineState previous_state;
    int state_transition_count;
} LV00Engine;

/**
 * @brief 创建并初始化一个 Lv-00 引擎实例。
 *
 * 【创建流程】
 *   1. 分配 LV00Engine 结构体内存（堆分配）
 *   2. 创建并初始化主约束图（ConstraintGraph）
 *   3. 初始化模块/公理包/重写规则动态数组（初始容量由 LV00_INITIAL_ARRAY_CAPACITY 控制）
 *   4. 设置默认重写步数上限（1000 步）
 *   5. 创建流式输出上下文（StreamContext）
 *   6. 将引擎状态设为 ENGINE_STATE_IDLE
 *   7. 清零错误状态和冻结点指针
 *
 * @return 指向新引擎的指针；内存分配失败时返回 NULL。
 * @note 调用者拥有返回指针的所有权，最终须通过 engine_destroy() 释放。
 */
LV00Engine *engine_create(void);

/**
 * @brief 销毁引擎实例，释放所有关联资源。
 *
 * 【销毁流程与资源释放顺序】
 *   1. 销毁主约束图（constraint_graph_destroy）—— 释放所有节点、约束和符号坐标
 *   2. 逐个销毁已加载模块（module_destroy）—— 释放模块内部资源
 *   3. 逐个销毁已加载公理包（axiom_package_destroy）—— 释放公理定义和规则
 *   4. 逐个销毁重写规则（rewrite_rule_destroy）—— 释放规则的模式和替换图
 *   5. 销毁冻结点快照（如果 frozen_point 非 NULL）—— 释放深拷贝的约束图
 *   6. 销毁流式输出上下文（stream_destroy）—— 释放回调注册表和事件队列
 *   7. 释放模块/公理包/重写规则动态数组本身（非数组内元素，元素已在上面的步骤中释放）
 *   8. 释放 LV00Engine 结构体本身
 *
 * @param engine 要销毁的引擎指针（可为 NULL，此时为空操作）。
 * @note 销毁后引擎指针不可再被使用。引擎不持有 context（Lv00Context）的所有权，
 *       因此 context 的生命周期由外部管理，不会被此函数释放。
 * @warning 如果引擎处于 ENGINE_STATE_REASONING 状态，销毁操作将强制中断推理流程。
 *          建议在销毁前确保引擎处于 IDLE 或 COMPLETE 状态。
 */
void engine_destroy(LV00Engine *engine);

/**
 * @brief 向引擎注册一条重写规则。
 *
 * 将规则加入引擎的重写规则数组，后续调用 engine_solve() 时会按注册顺序
 * 依次尝试匹配并应用这些规则。
 *
 * @param engine 引擎实例。
 * @param rule   要注册的重写规则指针（所有权转移给引擎，调用者不应再释放）。
 * @return true 成功，false 失败（内存不足或参数无效）。
 */
bool engine_add_rewrite_rule(LV00Engine *engine, const RewriteRule *rule);

/**
 * @brief 从指定文件路径加载一个模块到引擎。
 *
 * 解析模块文件（.lvmod 格式），将其中定义的几何元素、约束和规则
 * 加载到引擎的约束图中。
 *
 * @param engine   引擎实例。
 * @param filepath 模块文件的绝对或相对路径。
 * @return ModuleLoadStatus 枚举值，指示加载结果。
 */
ModuleLoadStatus engine_load_module(LV00Engine *engine, const char *filepath);

/**
 * @brief 从指定文件路径加载一个公理包到引擎。
 *
 * 解析公理包文件（.lvax 格式），将公理系统中定义的定理和推导规则
 * 注册到引擎，供后续求解过程引用。
 *
 * @param engine   引擎实例。
 * @param filepath 公理包文件的绝对或相对路径。
 * @return AxiomLoadStatus 枚举值，指示加载结果。
 */
AxiomLoadStatus engine_load_axiom_package(LV00Engine *engine, const char *filepath);

/**
 * @brief 将一组内部节点打包为函数块
 *
 * 将引擎约束图中的若干内部节点封装为一个可复用的函数块。
 * 打包后，调用者可通过返回的 func_block_id 在后续操作中引用该函数块。
 *
 * @param[in]  engine            引擎实例
 * @param[in]  internal_node_ids 内部节点 ID 数组（将被打包的节点）
 * @param[in]  internal_count    内部节点数量
 * @param[in]  input_port_ids    输入端口节点 ID 数组（定义函数块的输入接口）
 * @param[in]  input_count       输入端口数量
 * @param[in]  output_port_ids   输出端口节点 ID 数组（定义函数块的输出接口）
 * @param[in]  output_count      输出端口数量
 * @param[out] out_func_block_id 输出：新创建的函数块 ID
 * @return true 成功，false 失败（参数无效或内存不足）
 */
bool engine_pack_function(LV00Engine *engine, const int *internal_node_ids, int internal_count, const int *input_port_ids,
                          int input_count, const int *output_port_ids, int output_count, int *out_func_block_id);

/**
 * @brief 实例化一个已打包的函数块
 *
 * 根据给定的参数映射，在引擎约束图中创建指定函数块的一个实例。
 * 实例化会复制函数块内部的节点和约束结构，并将输入/输出端口
 * 绑定到映射表中指定的实际节点。
 *
 * @param[in]  engine          引擎实例
 * @param[in]  func_block_id   要实例化的函数块 ID（由 engine_pack_function 创建）
 * @param[in]  arg_mappings    参数映射数组：arg_mappings[i] 表示函数块的第 i 个输入端口
 *                             映射到的实际节点 ID
 * @param[in]  arg_count       参数映射数量（应与函数块的输入端口数量一致）
 * @param[out] out_result_count 输出：实例化产生的结果（输出端口）数量
 * @return 新创建的输出端口节点 ID 数组（调用者负责 free），失败返回 NULL
 */
int *engine_instantiate_function(LV00Engine *engine, int func_block_id, const int *arg_mappings, int arg_count,
                                 int *out_result_count);

UnifyStatus engine_unify(LV00Engine *engine, ConstraintGraph *construction, ConstraintGraph *proposition);

typedef enum { ENGINE_SOLVE_OK, ENGINE_SOLVE_CONFLICT, ENGINE_SOLVE_TIMEOUT, ENGINE_SOLVE_ERROR } EngineSolveResult;

typedef enum {
    ENGINE_CIRCUIT_IGNORE,
    ENGINE_CIRCUIT_ROLLBACK,
    ENGINE_CIRCUIT_DOWNGRADE,
    ENGINE_CIRCUIT_ERROR
} EngineCircuitResult;

/** @brief 位电路跳闸时的用户动作
 * 【枚举值命名规范】所有枚举值使用 UPPER_SNAKE_CASE
 */
typedef enum {
    ENGINE_CIRCUIT_ACTION_IGNORE,   /**< 忽略，接受当前结果 */
    ENGINE_CIRCUIT_ACTION_ROLLBACK, /**< 回滚到冻结点快照 */
    ENGINE_CIRCUIT_ACTION_DOWNGRADE /**< 永久降级为 AMBER */
} EngineCircuitAction;

EngineStatus engine_get_last_status(const LV00Engine *engine);
/**
 * @brief 获取引擎最近一次错误的描述字符串
 *
 * @param[in] engine 引擎实例（当前未使用，可为 NULL）
 * @return 内部静态错误字符串指针。调用者不得 free。
 *         在下一次可能修改错误状态的操作前有效。
 *         如无错误，返回空字符串。
 */
const char *engine_get_last_error(const LV00Engine *engine);

/**
 * @brief 将引擎状态码转换为可读字符串（v3.4.1 新增）
 *
 * 提供所有 EngineStatus 枚举值的人类可读描述，支持国际化。
 *
 * @param status 引擎状态码
 * @return 状态描述字符串（静态常量，无需释放）
 *
 * @note 返回的字符串为中文描述，用于日志记录和用户界面显示。
 *       如果状态码未知，返回 "未知状态"。
 *
 * 示例:
 * @code
 *   EngineStatus status = engine_get_last_status(engine);
 *   printf("操作结果: %s\n", engine_status_to_string(status));
 * @endcode
 */
const char *engine_status_to_string(EngineStatus status);

/**
 * @brief 将引擎状态码转换为英文标识符字符串（v3.4.1 新增）
 *
 * 返回状态码的英文标识符，适合用于日志、配置文件和程序逻辑判断。
 *
 * @param status 引擎状态码
 * @return 英文标识符字符串（静态常量，无需释放）
 *
 * @note 返回值格式为 ENGINE_STATUS_XXX，如 "ENGINE_STATUS_OK"。
 *       如果状态码未知，返回 "ENGINE_STATUS_UNKNOWN"。
 */
const char *engine_status_to_identifier(EngineStatus status);

/**
 * @brief 获取引擎状态的详细描述（v3.4.1 新增）
 *
 * 返回包含状态码、描述和建议操作的完整信息。
 *
 * @param status 引擎状态码
 * @return 详细描述字符串（静态常量，无需释放）
 */
const char *engine_status_get_description(EngineStatus status);

/* ---- 工作流编排 ---- */

/* 完整求解流水线：重写 -> 求解器 -> 冲突检查 -> 自由度更新。
 * 成功返回 ENGINE_SOLVE_OK，冲突返回 ENGINE_SOLVE_CONFLICT，
 * 超时返回 ENGINE_SOLVE_TIMEOUT。 */
EngineSolveResult engine_solve(LV00Engine *engine);

/* 重写-求解工作流，实现协作协议：
 * 先重写 -> 遇停顿则求解 -> 冲突暴露。
 * 返回总执行步数，出错返回负值。 */
int engine_rewrite_and_solve(LV00Engine *engine, int max_rewrite_steps, int max_solve_steps);

/* 位电路跳闸处理器，用于 symbolic_coord 溢出事件。
 * 返回 ENGINE_CIRCUIT_IGNORE 表示已处理，ENGINE_CIRCUIT_ROLLBACK 表示需要回滚，
 * ENGINE_CIRCUIT_DOWNGRADE 表示建议降级。 */
EngineCircuitResult engine_handle_circuit_trip(LV00Engine *engine);

/* 带显式用户动作的电路跳闸处理。
 * action: EngineCircuitAction 枚举值之一。
 * 成功返回 ENGINE_CIRCUIT_IGNORE，回滚返回 ENGINE_CIRCUIT_ROLLBACK，
 * 降级返回 ENGINE_CIRCUIT_DOWNGRADE，错误返回 ENGINE_CIRCUIT_ERROR。 */
EngineCircuitResult engine_handle_circuit_trip_with_action(LV00Engine *engine, EngineCircuitAction action);

/* ---- 重写步数上限配置 ---- */

/**
 * @brief 设置引擎的重写步数上限。
 *
 * 该上限由 engine_solve() 和 engine_rewrite_and_solve() 使用，
 * 用于限制每次迭代中的重写步数。
 *
 * @param engine  引擎实例。
 * @param limit   最大重写步数（必须 > 0；默认为 1000）。
 */
void engine_set_rewrite_step_limit(LV00Engine *engine, int limit);

/**
 * @brief 获取当前重写步数上限。
 *
 * @param engine  引擎实例。
 * @return 当前重写步数上限（默认 1000）。
 */
int engine_get_rewrite_step_limit(const LV00Engine *engine);

/* ---- 冻结点快照机制 ---- */

/**
 * @brief 创建当前引擎状态的冻结点快照。
 *
 * 深拷贝约束图，用于位电路跳闸后的回滚。
 * 调用者拥有返回的快照所有权，最终须调用 engine_destroy_frozen_point() 释放。
 *
 * @param engine  引擎实例。
 * @return 指向快照的不透明指针，失败返回 NULL。
 */
void *engine_create_frozen_point(LV00Engine *engine);

/**
 * @brief 将引擎恢复到先前创建的冻结点。
 *
 * 用快照状态替换引擎的约束图。
 * 成功恢复后，快照已被消耗，不应再传递给 engine_destroy_frozen_point()。
 *
 * @param engine        引擎实例。
 * @param frozen_point  由 engine_create_frozen_point() 返回的快照。
 * @return true 成功，false 失败。
 */
bool engine_restore_frozen_point(LV00Engine *engine, void *frozen_point);

/**
 * @brief 销毁冻结点快照，释放其内存。
 *
 * @param frozen_point  要销毁的快照（可为 NULL）。
 */
void engine_destroy_frozen_point(void *frozen_point);

/* ---- 流式输出 API ---- */

/**
 * @brief 获取引擎的流式上下文
 *
 * 引擎创建时自动创建流式上下文。可通过 stream_register_callback()
 * 注册回调以接收实时事件。
 *
 * @param engine  引擎实例
 * @return 流式上下文指针，或 NULL（引擎为 NULL 时）
 */
StreamContext *engine_get_stream_context(const LV00Engine *engine);

/**
 * @brief 设置引擎的流式输出开关
 *
 * 启用后，引擎在执行 solve/rewrite/normalize 等操作时会持续
 * 发射流式事件。默认启用。
 *
 * @param engine   引擎实例
 * @param enabled  true 启用，false 禁用
 */
void engine_set_streaming_enabled(LV00Engine *engine, bool enabled);

/**
 * @brief 获取流式输出是否启用
 *
 * @param engine  引擎实例
 * @return true 启用，false 禁用或 engine 为 NULL
 */
bool engine_is_streaming_enabled(const LV00Engine *engine);

/**
 * @brief 发射引擎流式事件（便捷函数）
 *
 * 填充 StreamEvent 的通用字段后调用 stream_emit()。
 * 如果 engine->stream_ctx 为 NULL，则为空操作。
 *
 * @param engine       引擎实例
 * @param type         事件类型
 * @param description  描述文本
 * @param step_number  步骤编号
 * @param node_id      相关节点 ID（-1 表示无）
 * @param constraint_id 相关约束 ID（-1 表示无）
 */
void engine_emit_stream_event(LV00Engine *engine, StreamEventType type, const char *description, int step_number,
                              int node_id, int constraint_id);

/* ---- 五状态机 API（v3.3.0 形式化）---- */

/**
 * @brief 尝试将引擎转移到指定状态
 *
 * 验证状态转移的合法性。非法转移将返回错误，不改变引擎状态。
 * 合法的转移记录在转移表中（见 engine.c）。
 *
 * @param engine    引擎实例（非 NULL）
 * @param new_state 目标状态（ENGINE_STATE_IDLE / PARSING / REASONING / ERROR / COMPLETE）
 * @return ENGINE_OK 成功，ENGINE_INVALID_STATE 非法转移
 */
EngineStatus lv00_engine_transition_state(LV00Engine *engine, EngineState new_state);

/**
 * @brief 获取引擎当前状态
 * @param engine 引擎实例（可为 NULL，返回 ENGINE_STATE_IDLE）
 * @return 当前状态枚举值
 */
EngineState engine_get_state(const LV00Engine *engine);

/**
 * @brief 检查引擎是否正在忙于推理计算
 *
 * 引擎在 REASONING 状态下无法接受新的求解请求、修改约束图拓扑
 * 或执行销毁操作。此函数提供一个便捷的忙状态查询接口。
 *
 * @param engine 引擎实例（可为 NULL，返回 false）
 * @return true 表示引擎繁忙（处于 REASONING 状态），false 表示空闲可接受请求
 *
 * @note 等价于 engine_get_state(engine) == ENGINE_STATE_REASONING
 * @see engine_get_state()
 */
bool engine_is_busy(const LV00Engine *engine);

/**
 * @brief 获取状态的可读名称
 * @param state 状态枚举值
 * @return 状态的中文名称字符串（静态存储，无需释放）
 */
const char *engine_state_name(EngineState state);

/**
 * @brief 检查从当前状态到目标状态的转移是否合法
 * @param from 当前状态
 * @param to   目标状态
 * @return true 合法，false 不合法
 */
bool engine_is_valid_transition(EngineState from, EngineState to);

#ifdef __cplusplus
}
#endif

#endif /* LV00_ENGINE_H */
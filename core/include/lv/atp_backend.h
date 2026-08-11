/**
 * @file atp_backend.h
 * @brief 一阶逻辑自动定理证明器（FOL ATP）后端抽象层
 *
 * 借鉴 Vampire、E Prover、iProver 的 FOL ATP 架构，
 * 为 Lv-00 提供纯一阶逻辑证明后端，与 SMT 后端
 * (smt_backend.h) 互补。SMT 处理算术/非线性约束，
 * ATP 处理纯逻辑推导和一阶量词推理。
 *
 * 新增证明策略类型：PROOF_STRATEGY_SUPERPOSITION
 * （映射到 proof.h 的 ProofStrategyType 扩展）。
 *
 * 设计借鉴：
 * - Vampire (github.com/vprover/vampire) — superposition calculus + strategy scheduling
 * - E Prover (github.com/eprover/eprover) — clause evaluation heuristics
 * - iProver (github.com/iprover/iprover) — Inst-Gen instantiation for quantifier-heavy problems
 *
 * @version 1.1.0
 * @date 2026-05-24
 */
#ifndef lv_ATP_BACKEND_H
#define lv_ATP_BACKEND_H
#include "constraint_graph.h"
#include "proof.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ========================================================================
 * ATP 后端类型枚举
 * ======================================================================== */
/** ATP 求解器后端类型 */
typedef enum {
    ATP_BACKEND_VAMPIRE = 0, /**< Vampire — superposition calculus，CASC 冠军 */
    ATP_BACKEND_EPROVER = 1, /**< E Prover — 高性能模块化 ATP */
    ATP_BACKEND_IPROVER = 2, /**< iProver — Inst-Gen，量词友好 */
    ATP_BACKEND_CUSTOM = 3,  /**< 自定义后端 */
    ATP_BACKEND_COUNT        /**< 后端总数 */
} ATPBackendType;
/**
 * @brief ATPBackendType 条目宏（枚举↔显示名映射的单一事实来源）
 * atp_backend.c 的 atp_backend_type_name 表由本宏生成；"e"/"eprover" 缩写
 * 在 atp_backend_type_from_name 内独立处理（非表项）。
 */
#define LV_ATP_BACKEND_ENTRY(x) \
    x(ATP_BACKEND_VAMPIRE, "Vampire") \
    x(ATP_BACKEND_EPROVER, "E Prover") \
    x(ATP_BACKEND_IPROVER, "iProver") \
    x(ATP_BACKEND_CUSTOM, "Custom")
/** ATP 输入格式 */
typedef enum {
    ATP_FORMAT_TPTP_FOF = 0, /**< TPTP FOF（一阶公式）— 最通用 */
    ATP_FORMAT_TPTP_CNF = 1, /**< TPTP CNF（子句范式）— 适合 superposition */
    ATP_FORMAT_TPTP_TFF = 2, /**< TPTP TFF（带类型的一阶公式） */
    ATP_FORMAT_SMTLIB2 = 3,  /**< SMT-LIB2 — 与 SMT 后端共用格式 */
} ATPInputFormat;

/**
 * @brief ATPInputFormat 全字段条目宏（枚举↔字符串映射的单一事实来源）
 *
 * 每行携带 4 列：ENUM（枚举值）、LANG（TPTP 语言标识符，TPTP 编码头部使用；
 * SMT-LIB2 无 TPTP 语言标识符，为 NULL）、MODE（命令行模式参数，Vampire/E
 * 等求解器使用；SMT-LIB2 为 NULL）、DISPLAY（对外显示名，atp_format_name 输出）。
 * atp_backend.c 的 lang/mode/display 三张表统一由本宏生成，禁止在其他文件重复定义。
 */
#define LV_ATP_FORMAT_ENTRY(x) \
    x(ATP_FORMAT_TPTP_FOF, "fof", "--fof", "TPTP FOF") \
    x(ATP_FORMAT_TPTP_CNF, "cnf", "--cnf", "TPTP CNF") \
    x(ATP_FORMAT_TPTP_TFF, "tff", "--tff", "TPTP TFF") \
    x(ATP_FORMAT_SMTLIB2, NULL, NULL, "SMT-LIB2")
/** ATP 求解结果 */
typedef enum {
    ATP_RESULT_SAT = 0,     /**< 可满足 */
    ATP_RESULT_UNSAT = 1,   /**< 不可满足（对应"证明成功"） */
    ATP_RESULT_UNKNOWN = 2, /**< 未知（超时/资源耗尽） */
    ATP_RESULT_ERROR = 3,   /**< 错误 */
} ATPResult;
/* ========================================================================
 * ATP 配置与结果
 * ======================================================================== */
/** ATP 求解器配置 */
typedef struct {
    /** 基本配置 */
    ATPInputFormat input_format; /**< 输入编码格式 */
    double timeout_seconds;      /**< 求解超时（秒），0 = 无限制 */
    int memory_limit_mb;         /**< 内存限制（MB），0 = 无限制 */
    /** 策略配置（借鉴 Vampire strategy scheduling） */
    bool auto_strategy;        /**< 自动策略选择（推荐） */
    const char *strategy_name; /**< 手动指定策略名（NULL = 默认） */
    /** 输出配置 */
    bool produce_proof;      /**< 输出证明（TSTP 格式） */
    bool produce_unsat_core; /**< 输出 unsat core */
    /** 高级配置 */
    bool use_avatar;            /**< Vampire AVATAR 模式（SAT+superposition） */
    int clause_weight_limit;    /**< 子句权重上限（0 = 默认） */
    const char *custom_options; /**< 自定义命令行选项（NULL = 无） */
    /** 调试 */
    int verbosity;        /**< 详细级别（0-3） */
    const char *log_file; /**< 日志文件路径（NULL = 无） */
} ATPConfig;
/** ATP 子句分配（变量绑定） */
typedef struct {
    int variable_id;     /**< 变量序号 */
    char *variable_name; /**< 变量名（如 "X", "Y"） */
    char *term;          /**< 绑定的项（TPTP 语法） */
} ATPBinding;
/** ATP 证明步骤 */
typedef struct {
    int step_id;          /**< 步骤序号 */
    char *clause;         /**< 子句（TPTP 语法） */
    char *inference_rule; /**< 推理规则名（resolution/superposition/...） */
    char *justification;  /**< 引用（parent step id） */
    bool is_axiom;        /**< 是否为公理步骤 */
    bool is_goal;         /**< 是否为结论步骤 */
} ATPProofStep;
/** ATP 求解结果 */
typedef struct {
    ATPResult result;          /**< 求解结果 */
    ATPBackendType backend;    /**< 使用的后端 */
    double solve_time_seconds; /**< 求解耗时（秒） */
    int generated_clauses;     /**< 生成的子句数 */
    int processed_clauses;     /**< 处理的子句数 */
    int kept_clauses;          /**< 保留的子句数 */
    /** 证明（UNSAT 时有意义） */
    ATPProofStep *proof_steps; /**< 证明步骤数组 */
    int proof_step_count;      /**< 证明步骤数 */
    /** Unsat Core（如果 produce_unsat_core=true） */
    int *unsat_core_clause_ids; /**< Unsat core 子句 ID 数组 */
    int unsat_core_count;       /**< Unsat core 大小 */
    /** 错误信息 */
    int error_code;          /**< 错误码 */
    char error_message[512]; /**< 错误消息 */
    /** 原始输出（调试用） */
    char *raw_output;      /**< 求解器原始 stdout */
    int raw_output_length; /**< 原始输出长度 */
} ATPResultInfo;
/* ========================================================================
 * ATP 编码：约束图 → TPTP
 *
 * 将 Lv-00 约束图编码为 TPTP FOF（一阶公式）格式，
 * 供 Vampire/E Prover/iProver 求解。
 *
 * 编码映射：
 * - 节点 → 常量符号（point_node_id, line_node_id, ...）
 * - GeomType → 一元谓词（is_point(X), is_line_segment(X), ...）
 * - 约束 → 二元/三元谓词（incident(P, L), between(A, B, C), ...）
 * - 公理 → TPTP axiom 子句（如"两点确定一条直线"）
 *
 * 几何公理自动从 axiom_packages/ 的当前加载状态中提取。
 * ======================================================================== */
/**
 * @brief 将约束图编码为 TPTP FOF 格式字符串
 *
 * @param[in] graph        约束图（非 NULL）
 * @param[in] format       ATP 输入格式（TPTP_FOF/CNF/TFF）
 * @param[in] problem_name TPTP 问题名（如 "lv_geometry_1"）
 * @param[in] include_proof_goal  是否包含待证明目标
 * @param[in] target_prop 待证明的命题（NULL = 跳过）
 * @return TPTP 格式字符串（调用者负责 free），失败返回 NULL
 */
char *atp_encode_constraint_graph(const ConstraintGraph *graph, ATPInputFormat format, const char *problem_name,
                                  bool include_proof_goal, const Proposition *target_prop);
/* ========================================================================
 * ATP 求解器生命周期
 *
 * 设计模式：工厂 + 注册表（与 smt_backend.h 一致）
 * ======================================================================== */
/** ATP 求解器句柄（opaque） */
typedef struct ATPBackendSolver ATPBackendSolver;
/** 求解器工厂函数指针 */
typedef ATPBackendSolver *(*ATPBackendCreateFunc)(const ATPConfig *config);
/** 后端注册条目 */
typedef struct {
    ATPBackendType type;         /**< 后端类型 */
    bool available;              /**< 系统上是否可用 */
    ATPBackendCreateFunc create; /**< 创建函数 */
    int priority;                /**< 默认优先级（低=优先） */
    const char *description;     /**< 描述 */
} ATPBackendEntry;
/** ATP 后端注册表 */
typedef struct {
    ATPBackendEntry entries[ATP_BACKEND_COUNT]; /**< 注册条目数组 */
    int count;                                  /**< 已注册数 */
} ATPBackendRegistry;
/**
 * @brief 创建默认 ATP 配置
 * @return 默认配置（TPTP_FOF, 30s 超时, auto_strategy, produce_proof）
 */
ATPConfig atp_config_default(void);
/**
 * @brief 创建 ATP 求解器
 * @param type   后端类型
 * @param config 配置
 * @return 求解器句柄，失败返回 NULL
 */
ATPBackendSolver *atp_solver_create(ATPBackendType type, const ATPConfig *config);
/**
 * @brief 销毁 ATP 求解器
 * @param solver 求解器句柄
 */
void atp_solver_destroy(ATPBackendSolver *solver);
/**
 * @brief 获取求解器后端类型
 */
ATPBackendType atp_solver_get_type(const ATPBackendSolver *solver);
/* ========================================================================
 * ATP 求解操作
 * ======================================================================== */
/**
 * @brief 将 TPTP 编码加载到求解器
 * @param solver    求解器
 * @param tptp_text TPTP 格式文本
 * @return lv_OK 成功
 */
int atp_solver_load(ATPBackendSolver *solver, const char *tptp_text);
/**
 * @brief 执行求解
 * @param solver 求解器
 * @param result 输出结果（调用者用 atp_result_destroy 释放）
 * @return lv_OK 成功
 */
int atp_solver_solve(ATPBackendSolver *solver, ATPResultInfo *result);
/**
 * @brief 便捷函数：编码 + 加载 + 求解
 *
 * 等价于：
 *   tptp = atp_encode_constraint_graph(graph, format, name, goal, prop);
 *   atp_solver_load(solver, tptp);
 *   atp_solver_solve(solver, result);
 *
 * @param solver      求解器
 * @param graph       约束图
 * @param format      ATP 格式
 * @param problem_name 问题名
 * @param include_goal 是否包含证明目标
 * @param target_prop  目标命题
 * @param result       输出结果
 * @return lv_OK 成功
 */
int atp_solver_solve_graph(ATPBackendSolver *solver, const ConstraintGraph *graph, ATPInputFormat format,
                           const char *problem_name, bool include_goal, const Proposition *target_prop,
                           ATPResultInfo *result);
/* ========================================================================
 * 结果处理与转换
 * ======================================================================== */
/**
 * @brief 释放 ATP 求解结果
 */
void atp_result_destroy(ATPResultInfo *result);
/**
 * @brief 初始化 ATP 求解结果
 */
void atp_result_init(ATPResultInfo *result);
/**
 * @brief 将 ATP 证明转换为 Lv-00 ProofNavigator 步骤
 *
 * 解析 TSTP 格式的 ATP 证明输出，转换为 Lv-00 的
 * ProofStep 数组，可直接 append 到 ProofNavigator。
 *
 * @param[in]  result       ATP 求解结果（ATP_RESULT_UNSAT）
 * @param[out] proof        Lv-00 证明导航器
 * @param[out] step_count   转换的步骤数
 * @return lv_OK 成功
 *
 * @note 当前仅支持 TSTP（Vampire/E Prover 输出格式）
 */
int atp_proof_to_lv(const ATPResultInfo *result, Proof *proof, int *step_count);
/* ========================================================================
 * 后端注册与发现
 *
 * 与 smt_backend.h 的注册表模式完全一致，
 * 便于 engine_scheduler.h 统一调度 ATP 和 SMT 后端。
 * ======================================================================== */
/**
 * @brief 获取全局 ATP 后端注册表
 */
const ATPBackendRegistry *atp_get_registry(void);
/**
 * @brief 注册自定义 ATP 后端
 * @return lv_OK 成功，lv_ERROR_ALREADY_EXISTS 已存在
 */
int atp_register_backend(const ATPBackendEntry *entry);
/**
 * @brief 检查后端在系统上是否可用
 *
 * 通过检查可执行文件是否在 PATH 中：
 * - ATP_BACKEND_VAMPIRE → `vampire --version`
 * - ATP_BACKEND_EPROVER → `eprover --version`
 * - ATP_BACKEND_IPROVER → `iprover --version`
 */
bool atp_is_backend_available(ATPBackendType type);
/**
 * @brief 查找后端条目
 * @return 找到返回条目指针，否则 NULL
 */
const ATPBackendEntry *atp_find_backend(ATPBackendType type);
/**
 * @brief 获取后端类型名称（如 "Vampire"）
 */
const char *atp_backend_type_name(ATPBackendType type);
/**
 * @brief 将所有 ATP 后端注册到全局后端插件注册表
 *
 * 创建 lvBackendPlugin 包装器，使 ATP 后端可通过统一的
 * lv_backend_plugin_find() / lv_backend_plugin_find_by_type() 查找。
 * 可安全地多次调用（仅首次生效）。
 */
void atp_register_all_plugins(void);
/**
 * @brief 从名称字符串解析后端类型
 * @return 成功返回 true，失败返回 false
 */
bool atp_backend_type_from_name(const char *name, ATPBackendType *out_type);
/* ========================================================================
 * 引擎调度器集成
 *
 * 以下函数可将 ATP 后端注册到 EngineScheduler，
 * 使其参与自动后端选择。
 * ======================================================================== */
/**
 * @brief 将所有可用 ATP 后端注册到引擎调度器
 *
 * 对每个系统上可用的 ATP 后端，在 EngineScheduler 中
 * 注册对应的后端条目，包括路由规则：
 * - 含有量化公式 + Vampire/E 可用 → ATP 优先
 * - 纯逻辑约束（无非线性算术）→ ATP 优先于 SMT
 *
 * @return 注册的后端数量
 */
int atp_register_all_to_scheduler(void);
/**
 * @brief 自动选择最优后端并求解（ATP vs SMT）
 *
 * 决策逻辑：
 * 1. 如果约束图仅含逻辑约束（INCIDENCE, BETWEENNESS），优先 ATP
 * 2. 如果约束含非线性算术，优先 SMT
 * 3. 如果混合，同时尝试 ATP 和 SMT，返回最先成功的结果
 *
 * @param graph  约束图
 * @param config ATP 配置
 * @param result 输出结果
 * @return lv_OK 成功
 */
int atp_auto_solve(const ConstraintGraph *graph, const ATPConfig *config, ATPResultInfo *result);
/* ========================================================================
 * 字符串工具
 * ======================================================================== */
/** 获取结果类型名称 */
const char *atp_result_name(ATPResult result);
/** 获取输入格式名称 */
const char *atp_format_name(ATPInputFormat format);
#ifdef __cplusplus
}
#endif
#endif /* lv_ATP_BACKEND_H */

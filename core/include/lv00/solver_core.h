/**
 * @file solver_core.h
 * @brief 求解器内核 —— 借鉴 CaDiCaL 的 CDCL 极简内核
 *
 * 设计借鉴来源：
 * - CaDiCaL (github.com/arminbiere/cadical) — Armin Biere 的 CDCL SAT 求解器
 *   · 冲突驱动子句学习（CDCL）核心循环
 *   · 增量求解接口（add/assume/solve/get）
 *   · 极简设计：约 15k 行 C++，聚焦核心正确性
 *   · 双监视文字（two-watched-literals）快速单元传播
 *   · 几何约束的代数量化解特定适配
 *
 * 核心设计理念：
 * 将 CaDiCaL 的 CDCL 引擎适配为 Lv-00 几何约束求解的核心后端：
 *   - 增量构建约束集
 *   - 冲突分析输出矛盾子句
 *   - 支持假设（under-assumptions）模式
 *   - 与 Groebner 基代数求解器协作
 *
 * @version v3.3.0
 * @date 2026-05-24
 */

#ifndef LV00_SOLVER_CORE_H
#define LV00_SOLVER_CORE_H

#include "lv00.h"
#include "symbolic_coord.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── 前向声明 ── */

/**
 * @brief 求解器句柄（不透明指针）
 *
 * 借鉴 CaDiCaL 的 Solver 类设计，外部代码仅持有不透明句柄，
 * 内部实现细节完全隐藏。所有操作通过 API 函数进行。
 */
typedef struct Lv00Solver Lv00Solver;

/* ── 求解结果 ── */

/**
 * @brief 求解器返回结果
 *
 * 借鉴 CaDiCaL 的三值语义，对应 MiniSat 的 l_True/l_False/l_Undef。
 */
typedef enum {
    LV00_SOLVER_SAT = 10,   /**< 可满足：已找到一组赋值 */
    LV00_SOLVER_UNSAT = 20, /**< 不可满足：问题无解 */
    LV00_SOLVER_UNKNOWN = 0 /**< 未知：资源耗尽（超时/内存） */
} Lv00SolverResult;

/**
 * @brief 求解结果转字符串
 *
 * @param result  求解结果
 * @return 人类可读的字符串
 */
static inline const char *lv00_solver_result_string(Lv00SolverResult result) {
    switch (result) {
        case LV00_SOLVER_SAT:
            return "SAT";
        case LV00_SOLVER_UNSAT:
            return "UNSAT";
        case LV00_SOLVER_UNKNOWN:
            return "UNKNOWN";
        default:
            return "?";
    }
}

/* ── 约束 ID ── */

/**
 * @brief 约束标识符
 *
 * 每个通过 add_constraint 添加的约束都获得一个唯一 ID，
 * 支持后续的 remove_constraint 操作。
 */
typedef int Lv00ConstraintId;

/** @brief 无效约束 ID 标记 */
#define LV00_CONSTRAINT_ID_INVALID (-1)

/* ── 变量类型 ── */

/**
 * @brief 求解器变量
 *
 * 内部 SAT 变量的外部标识符，从 1 开始编号。
 */
typedef int Lv00SolverVar;

/* ── 文字 ── */

/**
 * @brief 求解器文字
 *
 * 对变量和极性的编码：正值为 v（v 赋真），负值为 -v（v 赋假）。
 * 借鉴 CaDiCaL 的 int-based literal 编码。
 */
typedef int Lv00SolverLit;

/**
 * @brief 从变量和极性构造文字
 *
 * @param var   变量（>= 1）
 * @param sign  极性（true = 正，false = 负）
 * @return 文字
 */
static inline Lv00SolverLit lv00_make_lit(Lv00SolverVar var, bool sign) {
    return sign ? var : -var;
}

/**
 * @brief 从文字取反
 *
 * @param lit  文字
 * @return lit 的否定
 */
static inline Lv00SolverLit lv00_lit_negate(Lv00SolverLit lit) {
    return -lit;
}

/**
 * @brief 从文字获取变量
 *
 * @param lit  文字
 * @return 变量 ID（>= 1）
 */
static inline Lv00SolverVar lv00_lit_var(Lv00SolverLit lit) {
    return (lit < 0) ? -lit : lit;
}

/**
 * @brief 判断文字的正负极性
 *
 * @param lit  文字
 * @return true = 正文字，false = 负文字
 */
static inline bool lv00_lit_sign(Lv00SolverLit lit) {
    return lit > 0;
}

/* ── CDCL 状态机 ── */

/**
 * @brief CDCL 求解器内部状态
 *
 * 借鉴 CaDiCaL 的搜索状态机，每个状态对应求解流程的一个阶段。
 */
typedef enum {
    CDCL_IDLE = 0,        /**< 空闲：等待 solve() 调用 */
    CDCL_PROPAGATING = 1, /**< 传播：执行单元传播（BCP） */
    CDCL_CONFLICT = 2,    /**< 冲突：检测到冲突子句 */
    CDCL_ANALYZING = 3,   /**< 分析：分析冲突生成学习子句 */
    CDCL_BACKJUMPING = 4, /**< 回跳：非时序回溯到决策层 */
    CDCL_LEARNING = 5,    /**< 学习：将学习子句加入子句库 */
    CDCL_DECIDING = 6,    /**< 决策：做出新的变量赋值决策 */
    CDCL_RESTARTING = 7,  /**< 重启：放弃当前决策栈重启搜索 */
    CDCL_SATISFIED = 8,   /**< 满足：所有变量已赋值且无冲突 */
    CDCL_UNSAT = 9        /**< 不可满足：推导出空子句 */
} CDCLState;

/**
 * @brief CDCL 求解上下文
 *
 * 封装 CDCL 搜索的所有运行时状态：
 * - 蕴含图：记录文字被赋值的因果关系
 * - 决策层级：每个赋值的决策深度
 * - 冲突分析：构建蕴含图到冲突的解析
 * - 学习子句库：存储冲突分析产生的 nogood 子句
 */
typedef struct CDCLContext {
    /** 当前 CDCL 状态 */
    CDCLState state;

    /* ── 变量赋值 ── */
    /** 变量赋值数组：0 = 未赋值，ABS(lit) 对应变量 ID */
    int *assigns;     /**< 赋值数组（0 = 未赋值） */
    int *levels;      /**< 每个变量的决策层级 */
    int *reasons;     /**< 每个赋值的归因子句索引（-1 = 决策） */
    int var_count;    /**< 变量数量 */
    int var_capacity; /**< 赋值数组容量 */

    /* ── 决策栈 ── */
    int *trail;         /**< 赋值路径（文字序列） */
    int *trail_lim;     /**< 每个决策层级在trail中的起始位置 */
    int trail_size;     /**< trail 当前大小 */
    int trail_capacity; /**< trail 容量 */
    int decision_level; /**< 当前决策层级 */

    /* ── 子句数据库 ── */
    int **clauses;          /**< 原始子句 + 学习子句 */
    int *clause_sizes;      /**< 每个子句的大小 */
    int orig_clause_count;  /**< 原始子句数量 */
    int learn_clause_count; /**< 学习子句数量 */
    int clause_capacity;    /**< 子句数组容量 */

    /* ── 监视文字 ── */
    int **watches;         /**< 每个文字的监视列表 */
    int *watch_sizes;      /**< 每个文字的监视列表大小 */
    int *watch_capacities; /**< 每个文字的监视列表容量 */

    /* ── 冲突分析 ── */
    int *conflict_clause;  /**< 当前冲突子句 */
    int conflict_size;     /**< 冲突子句大小 */
    int conflict_capacity; /**< 冲突子句容量 */
    int backtrack_level;   /**< 回跳目标层级 */

    /* ── 统计 ── */
    int64_t propagations;     /**< 单元传播次数 */
    int64_t conflicts;        /**< 冲突次数 */
    int64_t decisions;        /**< 决策次数 */
    int64_t restarts;         /**< 重启次数 */
    int64_t learned_literals; /**< 学习到的文字总数 */
    double time_ms;           /**< 累计耗时（毫秒） */
} CDCLContext;

/* ── 配置 ── */

/**
 * @brief 求解器配置
 *
 * 借鉴 CaDiCaL 的 Options 系统，可配置求解行为。
 */
typedef struct Lv00SolverConfig {
    /** CDCL 核心参数 */
    bool enable_cdcl;          /**< 是否启用 CDCL（默认 true） */
    bool enable_restarts;      /**< 是否启用重启（默认 true） */
    bool enable_phase_saving;  /**< 是否保存阶段值（默认 true） */
    bool enable_luby_restarts; /**< 使用 Luby 序列重启（默认 true） */
    int restart_interval;      /**< 初始重启间隔（冲突数，默认 100） */
    double restart_multiplier; /**< 重启间隔倍增因子（默认 1.5） */
    double clause_decay;       /**< 子句活性衰减因子（默认 0.999） */
    double var_decay;          /**< 变量活性衰减因子（默认 0.95） */

    /** 资源限制 */
    int64_t max_conflicts; /**< 最大冲突数限制（0 = 无限） */
    int64_t max_decisions; /**< 最大决策数限制（0 = 无限） */
    double max_time_sec;   /**< 最大求解时间限制（0 = 无限） */
    int64_t max_memory_mb; /**< 最大内存限制（0 = 无限） */

    /** 代数求解器协同 */
    bool enable_groebner_fallback; /**< SAT 无解时回退 Groebner 基（默认 true） */
    bool enable_smt_combination;   /**< SMT 组合求解（默认 false） */
} Lv00SolverConfig;

/**
 * @brief 创建默认求解器配置
 *
 * 返回一个所有字段设置为合理默认值的配置。
 *
 * @return 默认配置
 */
static inline Lv00SolverConfig lv00_solver_config_default(void) {
    Lv00SolverConfig cfg;
    cfg.enable_cdcl = true;
    cfg.enable_restarts = true;
    cfg.enable_phase_saving = true;
    cfg.enable_luby_restarts = true;
    cfg.restart_interval = 100;
    cfg.restart_multiplier = 1.5;
    cfg.clause_decay = 0.999;
    cfg.var_decay = 0.95;
    cfg.max_conflicts = 0;
    cfg.max_decisions = 0;
    cfg.max_time_sec = 0.0;
    cfg.max_memory_mb = 0;
    cfg.enable_groebner_fallback = true;
    cfg.enable_smt_combination = false;
    return cfg;
}

/* ── 生命周期 API ── */

/**
 * @brief 创建求解器实例
 *
 * 分配并初始化一个 Lv00Solver，采用默认配置。
 *
 * @return 求解器句柄，失败返回 NULL
 */
Lv00Solver *lv00_solver_create(void);

/**
 * @brief 使用指定配置创建求解器实例
 *
 * @param config  配置（按值复制）
 * @return 求解器句柄，失败返回 NULL
 */
Lv00Solver *lv00_solver_create_with_config(const Lv00SolverConfig *config);

/**
 * @brief 销毁求解器实例
 *
 * 释放求解器的所有内部资源（子句库、变量表、CDCL 上下文等）。
 *
 * @param solver  求解器句柄（可为 NULL）
 */
void lv00_solver_destroy(Lv00Solver *solver);

/* ── 变量管理 API ── */

/**
 * @brief 为新变量分配外部标识符
 *
 * 借鉴 CaDiCaL 的 new_var() 接口，每次调用返回递增的变量编号。
 *
 * @param solver  求解器句柄
 * @return 新变量标识符（>= 1），失败返回 -1
 */
Lv00SolverVar lv00_solver_new_var(Lv00Solver *solver);

/**
 * @brief 批量分配多个变量
 *
 * @param solver  求解器句柄
 * @param count   需要分配的变量数量
 * @return 第一个新变量的标识符，失败返回 -1
 */
Lv00SolverVar lv00_solver_new_vars(Lv00Solver *solver, int count);

/**
 * @brief 获取当前变量总数
 *
 * @param solver  求解器句柄
 * @return 变量总数
 */
int lv00_solver_var_count(const Lv00Solver *solver);

/* ── 约束管理 API（增量求解） ── */

/**
 * @brief 添加约束子句
 *
 * 向求解器添加一个 CNF 子句（文字的析取式）。
 * 返回约束 ID 用于后续的 remove() 或 failed() 查询。
 *
 * @param solver    求解器句柄
 * @param literals  文字数组
 * @param count     文字数量
 * @return 约束 ID（>= 0），失败返回 LV00_CONSTRAINT_ID_INVALID
 *
 * @note 借鉴 CaDiCaL 的 add() 接口：零元子句表示矛盾，
 *       单元子句强制该文字为真。
 */
Lv00ConstraintId lv00_solver_add_constraint(Lv00Solver *solver, const Lv00SolverLit *literals, int count);

/**
 * @brief 移除之前添加的约束
 *
 * 借鉴 CaDiCaL 的增量求解支持，允许回收不再需要的约束。
 *
 * @param solver         求解器句柄
 * @param constraint_id  要移除的约束 ID
 * @return true 成功，false 约束不存在
 */
bool lv00_solver_remove_constraint(Lv00Solver *solver, Lv00ConstraintId constraint_id);

/* ── 求解 API ── */

/**
 * @brief 求解器求解
 *
 * 执行 CDCL 搜索，返回 SAT / UNSAT / UNKNOWN。
 *
 * @param solver  求解器句柄
 * @return 求解结果
 */
Lv00SolverResult lv00_solver_solve(Lv00Solver *solver);

/**
 * @brief 带假设求解
 *
 * 借鉴 CaDiCaL 的 solve(assumptions) 模定式：在给定假设文字下求解。
 * 假设文字会被临时强制为真，若 UNSAT 则可通过 failed() 查询矛盾假设。
 *
 * @param solver         求解器句柄
 * @param assumptions    假设文字数组
 * @param count          假设数量
 * @return 求解结果
 */
Lv00SolverResult lv00_solver_solve_under_assumptions(Lv00Solver *solver, const Lv00SolverLit *assumptions, int count);

/* ── 冲突追踪 API ── */

/**
 * @brief 查询失败原因
 *
 * 在 solve_under_assumptions 返回 UNSAT 后，
 * 查询给定约束是否是导致不可满足的原因之一。
 *
 * @param solver         求解器句柄
 * @param constraint_id  约束 ID
 * @return true 该约束出现在最终冲突中
 */
bool lv00_solver_failed_constraint(const Lv00Solver *solver, Lv00ConstraintId constraint_id);

/**
 * @brief 查询失败假设
 *
 * 在 solve_under_assumptions 返回 UNSAT 后，
 * 查询给定假设文字是否是导致不可满足的原因之一。
 *
 * @param solver      求解器句柄
 * @param assumption  假设文字
 * @return true 该假设出现在最终冲突中
 *
 * @note 借鉴 CaDiCaL 的 failed(assumption) 接口。
 */
bool lv00_solver_failed_assumption(const Lv00Solver *solver, Lv00SolverLit assumption);

/**
 * @brief 获取冲突集
 *
 * 返回当前矛盾的核心假设文字子集（UNSAT core）。
 *
 * @param solver     求解器句柄
 * @param out_count  输出：冲突集大小
 * @return 假设文字数组（调用者负责 free），失败返回 NULL
 */
Lv00SolverLit *lv00_solver_conflict_set(const Lv00Solver *solver, int *out_count);

/* ── 查询赋值 API ── */

/**
 * @brief 查询变量当前赋值
 *
 * 在 solve() 返回 SAT 后查询变量被赋予的真值。
 *
 * @param solver  求解器句柄
 * @param var     变量 ID
 * @return 正数 = 真，负数 = 假，0 = 未赋值
 */
int lv00_solver_get_value(const Lv00Solver *solver, Lv00SolverVar var);

/**
 * @brief 查询几何坐标值
 *
 * 将 SAT 赋值解码回符号坐标值。仅在求解返回 SAT 后调用。
 *
 * @param solver   求解器句柄
 * @param var_base 坐标编码的起始变量 ID
 * @param coord    输出：解码后的符号坐标
 * @return true 解码成功，false 变量未赋值或失败
 */
bool lv00_solver_get_coord(const Lv00Solver *solver, Lv00SolverVar var_base, SymbolicCoord *coord);

/* ── CDCL 状态机访问 API ── */

/**
 * @brief 获取 CDCL 当前状态
 *
 * @param solver  求解器句柄
 * @return 当前 CDCL 状态
 */
CDCLState lv00_solver_cdcl_state(const Lv00Solver *solver);

/**
 * @brief 获取 CDCL 统计信息
 *
 * @param solver           求解器句柄
 * @param out_conflicts    输出：冲突次数
 * @param out_decisions    输出：决策次数
 * @param out_propagations 输出：传播次数
 * @param out_restarts     输出：重启次数
 */
void lv00_solver_cdcl_stats(const Lv00Solver *solver, int64_t *out_conflicts, int64_t *out_decisions,
                            int64_t *out_propagations, int64_t *out_restarts);

/**
 * @brief 获取 CDCL 上下文（用于外部诊断）
 *
 * @param solver  求解器句柄
 * @return 只读的 CDCL 上下文指针，求解器未初始化时返回 NULL
 *
 * @note 返回的指针属于求解器，调用者不应修改或释放。
 */
const CDCLContext *lv00_solver_cdcl_context(const Lv00Solver *solver);

/* ── 协同求解 API ──
 *
 * Lv-00 求解器与 Groebner 基代数求解器的协同组合。
 */

/**
 * @brief 代数协同求解
 *
 * 当 SAT 求解达到资源限制仍未决时，
 * 切换到 Groebner 基代数方法尝试求解。
 *
 * @param solver  求解器句柄
 * @return 协同求解结果
 */
Lv00SolverResult lv00_solver_solve_algebraic(Lv00Solver *solver);

/**
 * @brief 设置约束图协同引用
 *
 * 将约束图关联到求解器，使得 SAT 求解器可以使用图的语义信息。
 *
 * @param solver  求解器句柄
 * @param graph   约束图（只读引用）
 */
void lv00_solver_set_constraint_graph(Lv00Solver *solver, const struct ConstraintGraph *graph);

/* ── 导入/导出 API ── */

/**
 * @brief 导出求解器状态为简化的求解器（clone）
 *
 * 创建一个新求解器，包含所有当前子句和变量状态。
 *
 * @param solver  原求解器
 * @return 克隆的求解器，失败返回 NULL
 */
Lv00Solver *lv00_solver_clone(const Lv00Solver *solver);

/**
 * @brief 重置求解器
 *
 * 清除所有变量和约束，恢复为初始化状态。
 *
 * @param solver  求解器句柄
 */
void lv00_solver_reset(Lv00Solver *solver);

#ifdef __cplusplus
}
#endif

#endif /* LV00_SOLVER_CORE_H */

/**
 * @file solver_core.c
 * @brief CDCL SAT 求解器核心实现 —— 借鉴 CaDiCaL 的 CDCL 极简内核
 *
 * 实现 CDCL（冲突驱动子句学习）SAT 求解器的核心框架：
 * 不透明句柄 Lv00Solver 的生命周期管理、10 状态 CDCL 状态机（传播/冲突分析/回溯/
 * 重启/满足/不可满足）、双监视文字的单元传播、变量管理和约束管理。
 *
 * 当前为桩实现框架，求解器求解返回 UNKNOWN，外部求解器集成待后续实现。
 *
 * @version v3.3.0
 * @date 2026-05-24
 */

#include "solver_core.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "lv00_internal.h"
#include "lv00_utils.h"
#include "error_codes.h"
#include "constraint_graph.h"

/** CDCL 求解器最大决策次数，超过此限制强制终止以避免无限循环 */
#define CDCL_MAX_DECISIONS     1000
/** CDCL 求解器主循环最大步数 */
#define CDCL_MAX_STEPS         1000
/** CDCL 求解器最大重启次数 */
#define CDCL_MAX_RESTARTS      10

/* ========================================================================
 * 内部常量
 * ======================================================================== */

/** 默认子句库容量 */
#define DEFAULT_CLAUSE_CAPACITY 256
/** 默认变量容量 */
#define DEFAULT_VAR_CAPACITY 128
/** 决策路径初始容量 */
#define DEFAULT_TRAIL_CAPACITY 256
/** 冲突子句初始容量 */
#define DEFAULT_CONFLICT_CAP 64
/** Luby 序列默认因子 */
#define DEFAULT_LUBY_FACTOR 100
/** 监视文字初始化容量 */
#define DEFAULT_WATCH_CAPACITY 64

/* ========================================================================
 * 求解器内部结构定义（不透明句柄的实现）
 * ======================================================================== */

struct Lv00Solver {
    /* 配置 */
    Lv00SolverConfig config;

    /* 变量管理 */
    int *values;        /**< 变量赋值数组：0=未赋值, >0=真, <0=假 */
    int var_count;      /**< 已分配变量数 */
    int var_capacity;   /**< values 数组容量 */
    int next_var_id;    /**< 下一个待分配变量 ID */

    /* 约束管理 */
    int **clauses;        /**< 子句数组（文字序列） */
    int *clause_sizes;    /**< 每子句的文字数 */
    int clause_count;     /**< 子句总数 */
    int clause_capacity;  /**< 子句数组容量 */
    int next_constraint_id; /**< 下一个约束 ID */

    /* 失败标记（用于 failed_constraint / failed_assumption） */
    bool *constraint_failed; /**< 约束失败标记，索引=constraint_id */
    int constraint_failed_cap; /**< 失败标记数组容量 */
    int *assumption_lits;     /**< 最近求解的假设文字 */
    int assumption_count;     /**< 假设数量 */
    bool *assumption_failed;  /**< 假设失败标记 */
    int assumption_failed_cap; /**< 假设失败标记数组容量 */

    /* CDCL 上下文 */
    CDCLContext cdcl;

    /* 约束图关联 */
    const ConstraintGraph *graph;
};

/* ========================================================================
 * 内部辅助 —— 确保容量
 * ======================================================================== */

/**
 * @brief 通用数组容量确保函数（委托给统一的 lv00_ensure_capacity）
 *
 * @param arr        指向数组指针的指针（二级指针，用于更新调用者的指针）
 * @param capacity   指向当前容量的指针（扩容成功后会被更新）
 * @param required   需要的最小元素个数
 * @param elem_size  每个元素的字节大小
 *
 * @return 1 表示容量充足或扩容成功，0 表示扩容失败（内存不足或溢出）
 *
 * @note 内部委托给 lv00_ensure_capacity，最小增长量为 1
 */
static int ensure_array_cap(void **arr, int *capacity, int required, size_t elem_size) {
    return lv00_ensure_capacity(arr, required, capacity, elem_size, 1) ? 1 : 0;
}

static bool ensure_clause_cap(Lv00Solver *s) {
    /* 注：ensure_clause_cap 需要同时扩容 clauses 和 clause_sizes 两个数组，
     * 无法直接使用 ensure_array_cap（通用函数只处理单数组）。
     * 但 realloc 已使用临时变量模式，失败时不会丢失原指针。 */
    if (s->clause_count >= s->clause_capacity) {
        /* 检查容量扩大的乘法是否会导致整数溢出 */
        if (s->clause_capacity > INT_MAX / LV00_ARRAY_GROWTH_FACTOR) return false;
        int new_cap = (s->clause_capacity == 0) ? DEFAULT_CLAUSE_CAPACITY
                                                : s->clause_capacity * LV00_ARRAY_GROWTH_FACTOR;
        int **new_c = (int **)lv00_realloc(s->clauses, (size_t)new_cap * sizeof(int *));
        int *new_s = (int *)lv00_realloc(s->clause_sizes, (size_t)new_cap * sizeof(int));
        if (!new_c || !new_s) {
            if (new_c) lv00_free((void **)&new_c);
            if (new_s) lv00_free((void **)&new_s);
            return false;
        }
        s->clauses = new_c;
        s->clause_sizes = new_s;
        s->clause_capacity = new_cap;
    }
    return true;
}

static bool ensure_var_cap(Lv00Solver *s) {
    return ensure_array_cap((void **)&s->values, &s->var_capacity,
                            s->var_count + 1, sizeof(int));
}

/**
 * @brief 初始化 CDCL 上下文
 */
static void cdcl_context_init(CDCLContext *ctx) {
    memset(ctx, 0, sizeof(CDCLContext));
    ctx->state = CDCL_IDLE;
    ctx->decision_level = 0;
}

/**
 * @brief 释放 CDCL 上下文中的动态数组
 */
static void cdcl_context_destroy(CDCLContext *ctx) {
    LV00_CHECK_NULL_VOID(ctx);
    lv00_free((void **)&ctx->assigns);
    lv00_free((void **)&ctx->levels);
    lv00_free((void **)&ctx->reasons);
    lv00_free((void **)&ctx->trail);
    lv00_free((void **)&ctx->trail_lim);
    lv00_free((void **)&ctx->conflict_clause);

    if (ctx->clauses) {
        for (int i = 0; i < ctx->orig_clause_count + ctx->learn_clause_count; i++) {
            lv00_free((void **)&ctx->clauses[i]);
        }
        lv00_free((void **)&ctx->clauses);
    }
    lv00_free((void **)&ctx->clause_sizes);

    if (ctx->watches) {
        for (int i = 1; i <= ctx->var_count; i++) {
            lv00_free((void **)&ctx->watches[i]);
        }
        lv00_free((void **)&ctx->watches);
    }
    lv00_free((void **)&ctx->watch_sizes);
    lv00_free((void **)&ctx->watch_capacities);

    memset(ctx, 0, sizeof(CDCLContext));
}

/* ========================================================================
 * 生命周期 API
 * ======================================================================== */

Lv00Solver *lv00_solver_create(void) {
    Lv00SolverConfig default_cfg = lv00_solver_config_default();
    return lv00_solver_create_with_config(&default_cfg);
}

Lv00Solver *lv00_solver_create_with_config(const Lv00SolverConfig *config) {
    LV00_CHECK_NULL(config, NULL);

    Lv00Solver *s = (Lv00Solver *)lv00_malloc(sizeof(Lv00Solver));
    LV00_CHECK_ALLOC(s, NULL);
    memset(s, 0, sizeof(Lv00Solver));

    /* 复制配置 */
    s->config = *config;

    /* 初始化变量管理 */
    s->values = (int *)lv00_malloc((size_t)DEFAULT_VAR_CAPACITY * sizeof(int));
    if (!s->values) {
        lv00_free((void **)&s);
        return NULL;
    }
    memset(s->values, 0, (size_t)DEFAULT_VAR_CAPACITY * sizeof(int));
    s->var_capacity = DEFAULT_VAR_CAPACITY;
    s->var_count = 0;
    s->next_var_id = 1;

    /* 初始化约束管理 */
    s->clauses = (int **)lv00_malloc((size_t)DEFAULT_CLAUSE_CAPACITY * sizeof(int *));
    s->clause_sizes = (int *)lv00_malloc((size_t)DEFAULT_CLAUSE_CAPACITY * sizeof(int));
    if (!s->clauses || !s->clause_sizes) {
        lv00_free((void **)&s->values);
        if (s->clauses) lv00_free((void **)&s->clauses);
        if (s->clause_sizes) lv00_free((void **)&s->clause_sizes);
        lv00_free((void **)&s);
        return NULL;
    }
    s->clause_capacity = DEFAULT_CLAUSE_CAPACITY;
    s->clause_count = 0;
    s->next_constraint_id = 0;

    /* 初始化 CDCL 上下文 */
    cdcl_context_init(&s->cdcl);

    /* 初始化失败标记 */
    s->constraint_failed = NULL;
    s->constraint_failed_cap = 0;
    s->assumption_lits = NULL;
    s->assumption_count = 0;
    s->assumption_failed = NULL;
    s->assumption_failed_cap = 0;

    s->graph = NULL;

    return s;
}

void lv00_solver_destroy(Lv00Solver *solver) {
    LV00_CHECK_NULL_VOID(solver);

    /* 释放变量 */
    lv00_free((void **)&solver->values);

    /* 释放子句 */
    for (int i = 0; i < solver->clause_count; i++) {
        lv00_free((void **)&solver->clauses[i]);
    }
    lv00_free((void **)&solver->clauses);
    lv00_free((void **)&solver->clause_sizes);

    /* 释放失败标记 */
    lv00_free((void **)&solver->constraint_failed);
    lv00_free((void **)&solver->assumption_lits);
    lv00_free((void **)&solver->assumption_failed);

    /* 释放 CDCL 上下文 */
    cdcl_context_destroy(&solver->cdcl);

    lv00_free((void **)&solver);
}

/* ========================================================================
 * 变量管理 API
 * ======================================================================== */

Lv00SolverVar lv00_solver_new_var(Lv00Solver *solver) {
    LV00_CHECK_NULL(solver, -1);
    return lv00_solver_new_vars(solver, 1);
}

Lv00SolverVar lv00_solver_new_vars(Lv00Solver *solver, int count) {
    LV00_CHECK_NULL(solver, -1);
    if (count <= 0) {
        lv00_set_error_ctx(LV00_ERROR_INVALID_PARAM, __FILE__, __LINE__, __func__,
                           "变量数量必须 >= 1, 实际=%d", count);
        return -1;
    }

    int first_id = solver->next_var_id;

    /* 扩容变量数组 */
    while (solver->var_count + count > solver->var_capacity) {
        /* 整数溢出检查：确保扩容不会超过 INT_MAX */
        if (solver->var_capacity > INT_MAX / LV00_ARRAY_GROWTH_FACTOR) {
            lv00_set_error_ctx(LV00_ERROR_OVERFLOW, __FILE__, __LINE__, __func__,
                               "变量容量溢出: current=%d", solver->var_capacity);
            return -1;
        }
        int new_cap = solver->var_capacity * LV00_ARRAY_GROWTH_FACTOR;
        int *new_v = (int *)lv00_realloc(solver->values, (size_t)new_cap * sizeof(int));
        if (!new_v) return -1;
        memset(new_v + solver->var_capacity, 0,
               (size_t)(new_cap - solver->var_capacity) * sizeof(int));
        solver->values = new_v;
        solver->var_capacity = new_cap;
    }

    solver->var_count += count;
    solver->next_var_id += count;
    solver->cdcl.var_count = solver->var_count;

    return first_id;
}

int lv00_solver_var_count(const Lv00Solver *solver) {
    LV00_CHECK_NULL(solver, 0);
    return solver->var_count;
}

/* ========================================================================
 * 约束管理 API
 * ======================================================================== */

Lv00ConstraintId lv00_solver_add_constraint(Lv00Solver *solver,
                                             const Lv00SolverLit *literals, int count) {
    LV00_CHECK_NULL(solver, LV00_CONSTRAINT_ID_INVALID);
    LV00_CHECK_NULL(literals, LV00_CONSTRAINT_ID_INVALID);
    if (count < 0) {
        lv00_set_error_ctx(LV00_ERROR_INVALID_PARAM, __FILE__, __LINE__, __func__,
                           "子句文字数不能为负: %d", count);
        return LV00_CONSTRAINT_ID_INVALID;
    }

    /* 零文字子句 = 矛盾 */
    if (count == 0) {
        lv00_set_error_ctx(LV00_ERROR_CONSTRAINT_CONFLICT, __FILE__, __LINE__, __func__,
                           "零文字子句表示矛盾");
        return LV00_CONSTRAINT_ID_INVALID;
    }

    if (!ensure_clause_cap(solver)) return LV00_CONSTRAINT_ID_INVALID;

    Lv00ConstraintId cid = solver->next_constraint_id;

    /* 扩展 constraint_failed 数组（在添加子句之前，避免扩容失败时子句已添加但标记数组不完整） */
    if (cid >= solver->constraint_failed_cap) {
        /* 整数溢出检查 */
        if (solver->constraint_failed_cap > INT_MAX / LV00_ARRAY_GROWTH_FACTOR) {
            lv00_set_error_ctx(LV00_ERROR_OVERFLOW, __FILE__, __LINE__, __func__,
                               "约束失败标记数组容量溢出: current=%d", solver->constraint_failed_cap);
            return LV00_CONSTRAINT_ID_INVALID;
        }
        int new_cap = (solver->constraint_failed_cap == 0) ? DEFAULT_CLAUSE_CAPACITY
                                                           : solver->constraint_failed_cap * LV00_ARRAY_GROWTH_FACTOR;
        bool *new_fail = (bool *)lv00_realloc(solver->constraint_failed, (size_t)new_cap * sizeof(bool));
        if (!new_fail) return LV00_CONSTRAINT_ID_INVALID;
        memset(new_fail + solver->constraint_failed_cap, 0,
               (size_t)(new_cap - solver->constraint_failed_cap) * sizeof(bool));
        solver->constraint_failed = new_fail;
        solver->constraint_failed_cap = new_cap;
    }

    /* 分配子句存储 */
    int *clause = (int *)lv00_malloc((size_t)(count + 1) * sizeof(int));
    LV00_CHECK_ALLOC(clause, LV00_CONSTRAINT_ID_INVALID);
    memcpy(clause, literals, (size_t)count * sizeof(int));
    clause[count] = 0; /* 0 终止标记 */

    int idx = solver->clause_count;
    solver->clauses[idx] = clause;
    solver->clause_sizes[idx] = count;
    solver->clause_count++;

    /* 更新 CDCL 子句库 */
    solver->cdcl.orig_clause_count++;

    solver->next_constraint_id++;
    solver->constraint_failed[cid] = false;

    return cid;
}

bool lv00_solver_remove_constraint(Lv00Solver *solver, Lv00ConstraintId constraint_id) {
    LV00_CHECK_NULL(solver, false);
    if (constraint_id < 0 || constraint_id >= solver->next_constraint_id) {
        lv00_set_error_ctx(LV00_ERROR_NOT_FOUND, __FILE__, __LINE__, __func__,
                           "约束 ID=%d 不在有效范围 [0, %d)", constraint_id, solver->next_constraint_id);
        return false;
    }

    /* 桩实现：标记约束为已移除。
     * 注意：当前实现不回收子句内存（增量求解需保留引用）。
     * 后续完整实现应支持子句回收和监视文字更新。 */
    if (constraint_id < solver->constraint_failed_cap) {
        solver->constraint_failed[constraint_id] = true;
    }

    /* 更新 CDCL 子句计数 */
    if (solver->cdcl.orig_clause_count > 0) {
        solver->cdcl.orig_clause_count--;
    }

    return true;
}

/* ========================================================================
 * CDCL 状态机 —— 10 状态实现
 * ======================================================================== */

/**
 * @brief 单元传播（BCP —— Boolean Constraint Propagation）
 *
 * 借鉴 CaDiCaL 的双监视文字算法：当子句中除一个文字外均为假时，
 * 强制传播该文字为真。
 *
 * @return CDCL 下一状态
 */
static CDCLState cdcl_step_propagate(CDCLContext *ctx) {
    /* 桩：无监视文字实现，直接进入下一阶段 */
    /* 此处框架预留：遍历监视列表，检测单元子句 */
    LV00_UNUSED(ctx);

    if (ctx->propagations < LV00_DEFAULT_MAX_ITERATIONS) {
        ctx->propagations++;
        return CDCL_PROPAGATING;
    }

    /* 传播收敛，转入决策 */
    return CDCL_DECIDING;
}

/**
 * @brief 冲突检测
 *
 * 检查是否存在全体文字均被否定赋值的冲突子句。
 *
 * @return CDCL 下一状态
 */
static CDCLState cdcl_step_conflict(CDCLContext *ctx) {
    /* 桩：无实际冲突检测，默认无冲突 */
    LV00_UNUSED(ctx);

    /* 框架预留：遍历子句，检测全体否定 */
    return CDCL_DECIDING;
}

/**
 * @brief 冲突分析
 *
 * 从冲突子句出发，沿蕴含图反向解析，生成学习子句。
 *
 * @return CDCL 下一状态
 */
static CDCLState cdcl_step_analyze(CDCLContext *ctx) {
    /* 桩：无冲突分析实现 */
    LV00_UNUSED(ctx);
    ctx->backtrack_level = 0;
    return CDCL_BACKJUMPING;
}

/**
 * @brief 非时序回溯
 *
 * 回跳至学习子句中最高的非冲突决策层级。
 *
 * @return CDCL 下一状态
 */
static CDCLState cdcl_step_backjump(CDCLContext *ctx) {
    /* 桩：实际回溯需展开决策栈 */
    LV00_UNUSED(ctx);
    return CDCL_LEARNING;
}

/**
 * @brief 子句学习
 *
 * 将冲突分析产生的学习子句加入子句库。
 *
 * @return CDCL 下一状态
 */
static CDCLState cdcl_step_learn(CDCLContext *ctx) {
    /* 桩 */
    LV00_UNUSED(ctx);
    ctx->conflicts++;
    return CDCL_DECIDING;
}

/**
 * @brief 变量决策
 *
 * 选择一个未赋值变量，尝试赋值。
 *
 * @return CDCL 下一状态
 */
static CDCLState cdcl_step_decide(CDCLContext *ctx) {
    /* 桩：不实际做决策 */
    LV00_UNUSED(ctx);
    ctx->decisions++;
    ctx->restarts++;

    /* 资源耗尽检查：桩实现无法确定可满足性，返回 UNKNOWN（IDLE） */
    if (ctx->decisions > CDCL_MAX_DECISIONS) {
        return CDCL_IDLE; /* 桩实现：无法确定，返回空闲状态 */
    }
    return CDCL_IDLE; /* 模拟：返回空闲状态 */
}

/**
 * @brief 重启
 *
 * 保留学习子句，清零决策栈，重新开始搜索。
 *
 * @return CDCL 下一状态
 */
static CDCLState cdcl_step_restart(CDCLContext *ctx) {
    /* 桩：重置决策层 */
    LV00_UNUSED(ctx);
    ctx->decision_level = 0;
    return CDCL_PROPAGATING;
}

/**
 * @brief CDCL 状态机主循环
 *
 * 执行 CDCL 搜索直到达到终止状态（SAT / UNSAT / 资源耗尽）。
 *
 * @param ctx CDCL 上下文
 * @return 终止状态
 */
static CDCLState cdcl_run(Lv00Solver *solver) {
    CDCLContext *ctx = &solver->cdcl;
    int max_steps = CDCL_MAX_STEPS;
    int step = 0;

    while (step < max_steps) {
        step++;

        switch (ctx->state) {
            case CDCL_IDLE:
                ctx->state = CDCL_PROPAGATING;
                break;

            case CDCL_PROPAGATING:
                ctx->state = cdcl_step_propagate(ctx);
                break;

            case CDCL_CONFLICT:
                ctx->state = cdcl_step_conflict(ctx);
                if (ctx->state == CDCL_CONFLICT) {
                    ctx->state = CDCL_ANALYZING;
                }
                break;

            case CDCL_ANALYZING:
                ctx->state = cdcl_step_analyze(ctx);
                break;

            case CDCL_BACKJUMPING:
                ctx->state = cdcl_step_backjump(ctx);
                break;

            case CDCL_LEARNING:
                ctx->state = cdcl_step_learn(ctx);
                /* 检查是否需要重启 */
                if (ctx->state == CDCL_DECIDING && solver->config.enable_restarts) {
                    if (ctx->restarts < CDCL_MAX_RESTARTS) {
                        ctx->state = CDCL_RESTARTING;
                    }
                }
                break;

            case CDCL_DECIDING:
                ctx->state = cdcl_step_decide(ctx);
                break;

            case CDCL_RESTARTING:
                ctx->state = cdcl_step_restart(ctx);
                break;

            case CDCL_SATISFIED:
            case CDCL_UNSAT:
                return ctx->state;

            default:
                return CDCL_IDLE;
        }
    }

    /* 步数耗尽：桩实现无法确定可满足性，返回 UNKNOWN */
    ctx->state = CDCL_IDLE;
    return CDCL_IDLE;
}

/* ========================================================================
 * 求解 API
 * ======================================================================== */

Lv00SolverResult lv00_solver_solve(Lv00Solver *solver) {
    LV00_CHECK_NULL(solver, LV00_SOLVER_UNKNOWN);

    /* 资源限制检查 */
    if (solver->config.max_time_sec > 0.0 && solver->cdcl.time_ms / 1000.0 > solver->config.max_time_sec) {
        return LV00_SOLVER_UNKNOWN;
    }

    /* 桩：运行 CDCL 状态机 */
    CDCLState final_state = cdcl_run(solver);

    switch (final_state) {
        case CDCL_SATISFIED:
            return LV00_SOLVER_SAT;
        case CDCL_UNSAT:
            return LV00_SOLVER_UNSAT;
        default:
            return LV00_SOLVER_UNKNOWN;
    }
}

Lv00SolverResult lv00_solver_solve_under_assumptions(Lv00Solver *solver,
                                                      const Lv00SolverLit *assumptions, int count) {
    LV00_CHECK_NULL(solver, LV00_SOLVER_UNKNOWN);

    /* 记录假设 */
    lv00_free((void **)&solver->assumption_lits);
    solver->assumption_lits = NULL;
    solver->assumption_count = 0;

    if (count > 0 && assumptions) {
        solver->assumption_lits = (int *)lv00_malloc((size_t)count * sizeof(int));
        if (!solver->assumption_lits) return LV00_SOLVER_UNKNOWN;
        memcpy(solver->assumption_lits, assumptions, (size_t)count * sizeof(int));
        solver->assumption_count = count;

        /* 扩展 assumption_failed 数组 */
        if (count > solver->assumption_failed_cap) {
            lv00_free((void **)&solver->assumption_failed);
            solver->assumption_failed = (bool *)lv00_malloc((size_t)count * sizeof(bool));
            if (solver->assumption_failed) {
                memset(solver->assumption_failed, 0, (size_t)count * sizeof(bool));
                solver->assumption_failed_cap = count;
            } else {
                solver->assumption_failed_cap = 0;
            }
        } else if (solver->assumption_failed) {
            memset(solver->assumption_failed, 0, (size_t)count * sizeof(bool));
        }
    }

    return lv00_solver_solve(solver);
}

/* ========================================================================
 * 冲突追踪 API
 * ======================================================================== */

bool lv00_solver_failed_constraint(const Lv00Solver *solver, Lv00ConstraintId constraint_id) {
    LV00_CHECK_NULL(solver, false);
    if (constraint_id < 0 || constraint_id >= solver->constraint_failed_cap) return false;
    return solver->constraint_failed[constraint_id];
}

bool lv00_solver_failed_assumption(const Lv00Solver *solver, Lv00SolverLit assumption) {
    LV00_CHECK_NULL(solver, false);
    if (!solver->assumption_failed || solver->assumption_count == 0) return false;

    for (int i = 0; i < solver->assumption_count; i++) {
        if (solver->assumption_lits[i] == assumption) {
            return solver->assumption_failed[i];
        }
    }
    return false;
}

Lv00SolverLit *lv00_solver_conflict_set(const Lv00Solver *solver, int *out_count) {
    LV00_CHECK_NULL(solver, NULL);
    LV00_CHECK_NULL(out_count, NULL);

    /* 桩：返回空冲突集 */
    *out_count = 0;
    int *set = (int *)lv00_malloc(sizeof(int));
    if (set) set[0] = 0;
    return set;
}

/* ========================================================================
 * 查询赋值 API
 * ======================================================================== */

int lv00_solver_get_value(const Lv00Solver *solver, Lv00SolverVar var) {
    LV00_CHECK_NULL(solver, 0);
    if (var < 1 || var > solver->var_count) {
        return 0; /* 无效变量 */
    }
    return solver->values[var - 1];
}

bool lv00_solver_get_coord(const Lv00Solver *solver, Lv00SolverVar var_base,
                            SymbolicCoord *coord) {
    LV00_CHECK_NULL(solver, false);
    LV00_CHECK_NULL(coord, false);

    /* 桩：不解码坐标 */
    LV00_UNUSED(var_base);
    memset(coord, 0, sizeof(SymbolicCoord));
    return false;
}

/* ========================================================================
 * CDCL 状态机访问 API
 * ======================================================================== */

CDCLState lv00_solver_cdcl_state(const Lv00Solver *solver) {
    LV00_CHECK_NULL(solver, CDCL_IDLE);
    return solver->cdcl.state;
}

void lv00_solver_cdcl_stats(const Lv00Solver *solver, int64_t *out_conflicts,
                             int64_t *out_decisions, int64_t *out_propagations,
                             int64_t *out_restarts) {
    if (!solver) {
        if (out_conflicts) *out_conflicts = 0;
        if (out_decisions) *out_decisions = 0;
        if (out_propagations) *out_propagations = 0;
        if (out_restarts) *out_restarts = 0;
        return;
    }
    if (out_conflicts) *out_conflicts = solver->cdcl.conflicts;
    if (out_decisions) *out_decisions = solver->cdcl.decisions;
    if (out_propagations) *out_propagations = solver->cdcl.propagations;
    if (out_restarts) *out_restarts = solver->cdcl.restarts;
}

const CDCLContext *lv00_solver_cdcl_context(const Lv00Solver *solver) {
    LV00_CHECK_NULL(solver, NULL);
    return &solver->cdcl;
}

/* ========================================================================
 * 协同求解 API
 * ======================================================================== */

Lv00SolverResult lv00_solver_solve_algebraic(Lv00Solver *solver) {
    LV00_CHECK_NULL(solver, LV00_SOLVER_UNKNOWN);

    /* 桩：Groebner 基求解未集成 */
    LV00_UNUSED(solver);
    return LV00_SOLVER_UNKNOWN;
}

void lv00_solver_set_constraint_graph(Lv00Solver *solver,
                                       const struct ConstraintGraph *graph) {
    LV00_CHECK_NULL_VOID(solver);
    solver->graph = graph;
}

/* ========================================================================
 * 导入/导出 API
 * ======================================================================== */

Lv00Solver *lv00_solver_clone(const Lv00Solver *solver) {
    LV00_CHECK_NULL(solver, NULL);

    Lv00Solver *clone = lv00_solver_create_with_config(&solver->config);
    if (!clone) return NULL;

    /* 复制变量状态 */
    if (clone->var_capacity >= solver->var_count) {
        memcpy(clone->values, solver->values, (size_t)solver->var_count * sizeof(int));
    }
    clone->var_count = solver->var_count;
    clone->next_var_id = solver->next_var_id;

    /* 复制子句 */
    clone->clause_count = solver->clause_count;
    for (int i = 0; i < solver->clause_count; i++) {
        int size = solver->clause_sizes[i];
        if (!ensure_clause_cap(clone)) {
            lv00_solver_destroy(clone);
            return NULL;
        }
        clone->clauses[i] = (int *)lv00_malloc((size_t)(size + 1) * sizeof(int));
        if (!clone->clauses[i]) {
            lv00_solver_destroy(clone);
            return NULL;
        }
        memcpy(clone->clauses[i], solver->clauses[i], (size_t)(size + 1) * sizeof(int));
        clone->clause_sizes[i] = size;
    }
    clone->next_constraint_id = solver->next_constraint_id;

    /* 复制图引用 */
    clone->graph = solver->graph;

    return clone;
}

void lv00_solver_reset(Lv00Solver *solver) {
    LV00_CHECK_NULL_VOID(solver);

    /* 清除变量赋值 */
    memset(solver->values, 0, (size_t)solver->var_capacity * sizeof(int));
    solver->var_count = 0;
    solver->next_var_id = 1;

    /* 清除子句 */
    for (int i = 0; i < solver->clause_count; i++) {
        lv00_free((void **)&solver->clauses[i]);
    }
    solver->clause_count = 0;
    solver->next_constraint_id = 0;

    /* 清除失败标记 */
    if (solver->constraint_failed) {
        memset(solver->constraint_failed, 0, (size_t)solver->constraint_failed_cap * sizeof(bool));
    }
    lv00_free((void **)&solver->assumption_lits);
    solver->assumption_lits = NULL;
    solver->assumption_count = 0;
    if (solver->assumption_failed) {
        memset(solver->assumption_failed, 0, (size_t)solver->assumption_failed_cap * sizeof(bool));
    }

    /* 重置 CDCL 上下文 */
    cdcl_context_destroy(&solver->cdcl);
    cdcl_context_init(&solver->cdcl);

    solver->graph = NULL;
}

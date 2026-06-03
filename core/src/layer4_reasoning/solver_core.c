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
 * CDCL 内部辅助函数
 * ======================================================================== */

/**
 * @brief 判断文字在当前赋值下是否为真
 *
 * @param ctx  CDCL 上下文
 * @param lit  文字（正=变量赋真，负=变量赋假）
 * @return 1=真, 0=未赋值, -1=假
 */
static int lit_value(const CDCLContext *ctx, int lit) {
    int var = (lit < 0) ? -lit : lit;
    if (var < 1 || var > ctx->var_count) return 0;
    int a = ctx->assigns[var];
    if (a == 0) return 0;
    return (a == lit) ? 1 : -1;
}

/**
 * @brief 将文字赋值为真（加入 trail）
 */
static void cdcl_assign(CDCLContext *ctx, int lit, int reason_clause) {
    int var = (lit < 0) ? -lit : lit;
    ctx->assigns[var] = lit;
    ctx->levels[var] = ctx->decision_level;
    ctx->reasons[var] = reason_clause;

    /* 扩展 trail 容量 */
    if (ctx->trail_size >= ctx->trail_capacity) {
        int new_cap = (ctx->trail_capacity == 0) ? DEFAULT_TRAIL_CAPACITY
                                                 : ctx->trail_capacity * LV00_ARRAY_GROWTH_FACTOR;
        int *new_trail = (int *)lv00_realloc(ctx->trail, (size_t)new_cap * sizeof(int));
        if (!new_trail) return;
        ctx->trail = new_trail;
        ctx->trail_capacity = new_cap;
    }
    ctx->trail[ctx->trail_size++] = lit;
}

/**
 * @brief 将文字赋值为真（决策层 0，用于初始传播）
 */
static void cdcl_assign_unit(CDCLContext *ctx, int lit, int reason_clause) {
    int old_level = ctx->decision_level;
    ctx->decision_level = 0;
    cdcl_assign(ctx, lit, reason_clause);
    ctx->decision_level = old_level;
}

/**
 * @brief 回溯：撤销从 trail[top..trail_size) 的所有赋值
 */
static void cdcl_undo_trail(CDCLContext *ctx, int top) {
    while (ctx->trail_size > top) {
        int lit = ctx->trail[--ctx->trail_size];
        int var = (lit < 0) ? -lit : lit;
        ctx->assigns[var] = 0;
        ctx->levels[var] = 0;
        ctx->reasons[var] = -1;
    }
}

/**
 * @brief 回溯到指定决策层
 */
static void cdcl_backtrack_to(CDCLContext *ctx, int level) {
    if (level >= ctx->decision_level) return;
    /* 撤销 trail_lim[level+1..] 对应的所有赋值 */
    int new_trail_size = (level >= 0 && level < ctx->decision_level)
                         ? ctx->trail_lim[level]
                         : 0;
    /* trail_lim 索引从 0 开始，但决策层 0 的 trail_lim[0] 不一定存在 */
    if (level < 0) new_trail_size = 0;
    else if (level == 0) new_trail_size = 0; /* 决策层 0 保留单元传播 */
    else if (level < ctx->decision_level && ctx->trail_lim) {
        new_trail_size = ctx->trail_lim[level];
    }
    cdcl_undo_trail(ctx, new_trail_size);
    ctx->decision_level = level;
}

/**
 * @brief 初始化 CDCL 上下文的子句库（从 solver 的 clauses 复制）
 */
static bool cdcl_init_clauses(CDCLContext *ctx, Lv00Solver *solver) {
    int total = solver->clause_count;
    if (total == 0) return true;

    /* 扩容 */
    if (total > ctx->clause_capacity) {
        int new_cap = total * 2;
        int **new_clauses = (int **)lv00_realloc(ctx->clauses, (size_t)new_cap * sizeof(int *));
        int *new_sizes = (int *)lv00_realloc(ctx->clause_sizes, (size_t)new_cap * sizeof(int));
        if (!new_clauses || !new_sizes) {
            if (new_clauses) lv00_free((void **)&new_clauses);
            if (new_sizes) lv00_free((void **)&new_sizes);
            return false;
        }
        ctx->clauses = new_clauses;
        ctx->clause_sizes = new_sizes;
        ctx->clause_capacity = new_cap;
    }

    /* 复制子句 */
    for (int i = 0; i < total; i++) {
        int sz = solver->clause_sizes[i];
        ctx->clauses[i] = (int *)lv00_malloc((size_t)(sz + 1) * sizeof(int));
        if (!ctx->clauses[i]) return false;
        memcpy(ctx->clauses[i], solver->clauses[i], (size_t)(sz + 1) * sizeof(int));
        ctx->clause_sizes[i] = sz;
    }
    ctx->orig_clause_count = total;
    ctx->learn_clause_count = 0;

    return true;
}

/**
 * @brief 初始化 CDCL 上下文的赋值数组
 */
static bool cdcl_init_assigns(CDCLContext *ctx, int var_count) {
    if (var_count <= 0) return true;
    ctx->assigns = (int *)lv00_calloc((size_t)(var_count + 1), sizeof(int));
    ctx->levels = (int *)lv00_calloc((size_t)(var_count + 1), sizeof(int));
    ctx->reasons = (int *)lv00_malloc((size_t)(var_count + 1) * sizeof(int));
    if (!ctx->assigns || !ctx->levels || !ctx->reasons) return false;
    for (int i = 0; i <= var_count; i++) ctx->reasons[i] = -1;
    ctx->var_count = var_count;
    ctx->var_capacity = var_count;
    return true;
}

/**
 * @brief 初始化 trail_lim 数组
 */
static bool cdcl_init_trail_lim(CDCLContext *ctx, int capacity) {
    ctx->trail_lim = (int *)lv00_calloc((size_t)capacity, sizeof(int));
    return ctx->trail_lim != NULL;
}

/**
 * @brief 扩展 trail_lim 数组
 */
static bool cdcl_ensure_trail_lim(CDCLContext *ctx, int level) {
    /* trail_lim 需要至少 level+1 个槽位（决策层 0 不需要记录） */
    /* 我们用 trail_lim[decision_level] 记录当前决策层的起始位置 */
    static int trail_lim_cap = 0;
    LV00_UNUSED(trail_lim_cap);
    /* 简单策略：每次需要时 realloc 到足够大 */
    int needed = level + 2;
    /* 检查是否需要扩容 - 通过检查 realloc 是否会失败 */
    int *new_lim = (int *)lv00_realloc(ctx->trail_lim, (size_t)needed * sizeof(int));
    if (!new_lim) return false;
    /* 初始化新增部分 */
    for (int i = (ctx->trail_lim ? 0 : 0); i < needed; i++) {
        /* 只初始化可能未初始化的部分 */
    }
    ctx->trail_lim = new_lim;
    return true;
}

/* ========================================================================
 * CDCL 状态机 —— 10 状态实现
 * ======================================================================== */

/**
 * @brief 单元传播（BCP —— Boolean Constraint Propagation）
 *
 * 遍历所有子句，找到单元子句（只有一个未赋值文字）并强制传播。
 * 使用简单的线性扫描实现（非双监视文字，适合小规模问题）。
 *
 * @return CDCL 下一状态
 */
static CDCLState cdcl_step_propagate(CDCLContext *ctx) {
    int propagated_any = 0;
    int total_clauses = ctx->orig_clause_count + ctx->learn_clause_count;

    for (int i = 0; i < total_clauses; i++) {
        int *clause = ctx->clauses[i];
        int sz = ctx->clause_sizes[i];
        int unassigned = -1;  /* 未赋值文字的索引 */
        int unassigned_lit = 0;
        int false_count = 0;
        bool satisfied = false;

        for (int j = 0; j < sz; j++) {
            int lit = clause[j];
            int val = lit_value(ctx, lit);
            if (val == 1) { satisfied = true; break; }
            if (val == -1) { false_count++; }
            else { unassigned = j; unassigned_lit = lit; }
        }

        if (satisfied) continue;

        if (false_count == sz) {
            /* 冲突：所有文字为假 */
            ctx->conflict_clause = clause;
            ctx->conflict_size = sz;
            return CDCL_CONFLICT;
        }

        if (false_count == sz - 1 && unassigned >= 0) {
            /* 单元子句：强制传播 */
            cdcl_assign(ctx, unassigned_lit, i);
            ctx->propagations++;
            propagated_any = 1;
        }
    }

    if (propagated_any) {
        return CDCL_PROPAGATING; /* 继续传播直到不动点 */
    }

    /* 传播收敛，转入决策 */
    return CDCL_DECIDING;
}

/**
 * @brief 冲突检测
 *
 * 遍历子句检查是否存在全体文字均被否定赋值的冲突子句。
 * 由 propagate 触发，此处进行二次确认。
 *
 * @return CDCL 下一状态
 */
static CDCLState cdcl_step_conflict(CDCLContext *ctx) {
    if (ctx->conflict_clause != NULL && ctx->conflict_size > 0) {
        /* 已在 propagate 中检测到冲突 */
        return CDCL_CONFLICT;
    }

    /* 二次扫描确认 */
    int total_clauses = ctx->orig_clause_count + ctx->learn_clause_count;
    for (int i = 0; i < total_clauses; i++) {
        int *clause = ctx->clauses[i];
        int sz = ctx->clause_sizes[i];
        bool all_false = true;
        for (int j = 0; j < sz; j++) {
            if (lit_value(ctx, clause[j]) != -1) { all_false = false; break; }
        }
        if (all_false) {
            ctx->conflict_clause = clause;
            ctx->conflict_size = sz;
            return CDCL_CONFLICT;
        }
    }

    return CDCL_DECIDING;
}

/**
 * @brief 冲突分析
 *
 * 从冲突子句出发，沿蕴含图反向解析（1-UIP 策略），生成学习子句。
 * 学习子句存储在 ctx->conflict_clause 中（复用），backtrack_level
 * 设为学习子句中除 UIP 文字外的最高决策层。
 *
 * @return CDCL 下一状态
 */
static CDCLState cdcl_step_analyze(CDCLContext *ctx) {
    if (!ctx->conflict_clause || ctx->conflict_size == 0) {
        /* 空冲突子句 = UNSAT */
        return CDCL_UNSAT;
    }

    /* 使用 seen 标记数组（复用 levels 数组的符号位，或使用栈上临时数组） */
    /* 简化实现：收集冲突子句中所有在当前或更早决策层被赋值的文字 */
    int *seen = (int *)lv00_calloc((size_t)(ctx->var_count + 1), sizeof(int));
    if (!seen) return CDCL_UNSAT;

    int *resolving = (int *)lv00_malloc((size_t)(ctx->trail_capacity + 1) * sizeof(int));
    int resolve_count = 0;
    if (!resolving) { lv00_free((void **)&seen); return CDCL_UNSAT; }

    /* 初始化：将冲突子句中的文字加入解析栈 */
    for (int i = 0; i < ctx->conflict_size; i++) {
        int lit = ctx->conflict_clause[i];
        int var = (lit < 0) ? -lit : lit;
        if (lit_value(ctx, lit) == -1 && !seen[var]) {
            seen[var] = 1;
            resolving[resolve_count++] = lit;
        }
    }

    /* 反向解析：沿蕴含图回溯到 1-UIP */
    int idx = 0;
    int cnt_at_current_level = 0;
    int bt_level = 0;

    while (idx < resolve_count) {
        int lit = resolving[idx++];
        int var = (lit < 0) ? -lit : lit;
        int level = ctx->levels[var];

        if (level == ctx->decision_level) {
            cnt_at_current_level++;
        } else if (level > 0) {
            if (level > bt_level) bt_level = level;
            /* 将该变量的归因子句中的文字加入解析栈 */
            int reason = ctx->reasons[var];
            if (reason >= 0 && reason < ctx->orig_clause_count + ctx->learn_clause_count) {
                int *rclause = ctx->clauses[reason];
                int rsz = ctx->clause_sizes[reason];
                for (int j = 0; j < rsz; j++) {
                    int rlit = rclause[j];
                    int rvar = (rlit < 0) ? -rlit : rlit;
                    if (rlit != lit && !seen[rvar]) {
                        seen[rvar] = 1;
                        resolving[resolve_count++] = rlit;
                    }
                }
            }
        }
        /* level == 0 的文字不需要进一步解析 */
    }

    /* 构建学习子句：所有 seen 标记的文字取反 */
    int learned_size = 0;
    for (int v = 1; v <= ctx->var_count; v++) {
        if (seen[v]) learned_size++;
    }

    if (learned_size == 0) {
        /* 空学习子句 = UNSAT */
        lv00_free((void **)&seen);
        lv00_free((void **)&resolving);
        return CDCL_UNSAT;
    }

    /* 扩容冲突子句缓冲区以存储学习子句 */
    if (learned_size > ctx->conflict_capacity) {
        int new_cap = learned_size * 2;
        int *new_cc = (int *)lv00_realloc(ctx->conflict_clause, (size_t)new_cap * sizeof(int));
        if (!new_cc) {
            lv00_free((void **)&seen);
            lv00_free((void **)&resolving);
            return CDCL_UNSAT;
        }
        ctx->conflict_clause = new_cc;
        ctx->conflict_capacity = new_cap;
    }

    int write = 0;
    for (int v = 1; v <= ctx->var_count; v++) {
        if (seen[v]) {
            ctx->conflict_clause[write++] = -ctx->assigns[v]; /* 取反 */
        }
    }
    ctx->conflict_size = write;

    ctx->backtrack_level = bt_level;

    lv00_free((void **)&seen);
    lv00_free((void **)&resolving);

    return CDCL_BACKJUMPING;
}

/**
 * @brief 非时序回溯
 *
 * 回跳至 backtrack_level，撤销该层之上的所有赋值。
 *
 * @return CDCL 下一状态
 */
static CDCLState cdcl_step_backjump(CDCLContext *ctx) {
    int target = ctx->backtrack_level;

    /* 撤销 trail 中高于 target 层的所有赋值 */
    while (ctx->trail_size > 0) {
        int lit = ctx->trail[ctx->trail_size - 1];
        int var = (lit < 0) ? -lit : lit;
        if (ctx->levels[var] <= target) break;
        ctx->trail_size--;
        ctx->assigns[var] = 0;
        ctx->levels[var] = 0;
        ctx->reasons[var] = -1;
    }

    ctx->decision_level = target;
    ctx->conflict_clause = NULL;
    ctx->conflict_size = 0;

    return CDCL_LEARNING;
}

/**
 * @brief 子句学习
 *
 * 将冲突分析产生的学习子句加入子句库，并立即传播其单元蕴含。
 *
 * @return CDCL 下一状态
 */
static CDCLState cdcl_step_learn(CDCLContext *ctx) {
    if (ctx->conflict_size == 0) {
        ctx->conflicts++;
        return CDCL_DECIDING;
    }

    int learned_size = ctx->conflict_size;
    int *learned_lits = ctx->conflict_clause;

    /* 扩容子句数组 */
    int total = ctx->orig_clause_count + ctx->learn_clause_count;
    if (total >= ctx->clause_capacity) {
        int new_cap = (ctx->clause_capacity == 0) ? DEFAULT_CLAUSE_CAPACITY
                                                   : ctx->clause_capacity * LV00_ARRAY_GROWTH_FACTOR;
        int **new_cl = (int **)lv00_realloc(ctx->clauses, (size_t)new_cap * sizeof(int *));
        int *new_sz = (int *)lv00_realloc(ctx->clause_sizes, (size_t)new_cap * sizeof(int));
        if (!new_cl || !new_sz) {
            if (new_cl) lv00_free((void **)&new_cl);
            if (new_sz) lv00_free((void **)&new_sz);
            ctx->conflicts++;
            return CDCL_DECIDING;
        }
        ctx->clauses = new_cl;
        ctx->clause_sizes = new_sz;
        ctx->clause_capacity = new_cap;
    }

    /* 分配并复制学习子句 */
    int *new_clause = (int *)lv00_malloc((size_t)(learned_size + 1) * sizeof(int));
    if (!new_clause) {
        ctx->conflicts++;
        return CDCL_DECIDING;
    }
    memcpy(new_clause, learned_lits, (size_t)learned_size * sizeof(int));
    new_clause[learned_size] = 0;

    int idx = ctx->orig_clause_count + ctx->learn_clause_count;
    ctx->clauses[idx] = new_clause;
    ctx->clause_sizes[idx] = learned_size;
    ctx->learn_clause_count++;
    ctx->learned_literals += learned_size;

    ctx->conflicts++;

    /* 清除冲突状态 */
    ctx->conflict_clause = NULL;
    ctx->conflict_size = 0;

    return CDCL_DECIDING;
}

/**
 * @brief 变量决策
 *
 * 选择第一个未赋值的变量，赋值为真（正文字），压入决策栈。
 *
 * @return CDCL 下一状态
 */
static CDCLState cdcl_step_decide(CDCLContext *ctx) {
    /* 检查是否所有变量已赋值 -> SAT */
    bool all_assigned = true;
    for (int v = 1; v <= ctx->var_count; v++) {
        if (ctx->assigns[v] == 0) { all_assigned = false; break; }
    }
    if (all_assigned) return CDCL_SATISFIED;

    /* 资源耗尽检查 */
    if (ctx->decisions > CDCL_MAX_DECISIONS) {
        return CDCL_IDLE;
    }

    /* 选择第一个未赋值的变量（简单策略） */
    int decision_var = -1;
    for (int v = 1; v <= ctx->var_count; v++) {
        if (ctx->assigns[v] == 0) { decision_var = v; break; }
    }
    if (decision_var < 0) return CDCL_SATISFIED;

    /* 进入新决策层 */
    ctx->decision_level++;

    /* 记录当前 trail 位置 */
    int needed = ctx->decision_level + 1;
    int *new_lim = (int *)lv00_realloc(ctx->trail_lim, (size_t)needed * sizeof(int));
    if (new_lim) {
        ctx->trail_lim = new_lim;
        ctx->trail_lim[ctx->decision_level] = ctx->trail_size;
    }

    /* 赋值：默认选正文字（可扩展为 VSIDS 等启发式） */
    cdcl_assign(ctx, decision_var, -1); /* -1 表示决策赋值 */
    ctx->decisions++;

    return CDCL_PROPAGATING;
}

/**
 * @brief 重启
 *
 * 保留学习子句，撤销所有非零层决策，重新开始搜索。
 *
 * @return CDCL 下一状态
 */
static CDCLState cdcl_step_restart(CDCLContext *ctx) {
    /* 撤销所有决策层（保留层 0 的单元传播） */
    int keep = 0;
    if (ctx->trail_lim && ctx->decision_level > 0) {
        keep = ctx->trail_lim[0]; /* 层 0 的 trail 起始位置 = 0 */
    }
    cdcl_undo_trail(ctx, keep);
    ctx->decision_level = 0;
    ctx->restarts++;
    return CDCL_PROPAGATING;
}

/**
 * @brief CDCL 状态机主循环
 *
 * 初始化 CDCL 上下文（赋值数组、子句库），然后执行 CDCL 搜索
 * 直到达到终止状态（SAT / UNSAT / 资源耗尽）。
 *
 * @param solver  求解器实例
 * @return 终止状态
 */
static CDCLState cdcl_run(Lv00Solver *solver) {
    CDCLContext *ctx = &solver->cdcl;

    /* 首次运行时初始化 CDCL 上下文 */
    if (ctx->assigns == NULL) {
        if (!cdcl_init_assigns(ctx, solver->var_count)) return CDCL_IDLE;
    }
    if (ctx->clauses == NULL || ctx->orig_clause_count == 0) {
        if (!cdcl_init_clauses(ctx, solver)) return CDCL_IDLE;
    }
    /* 确保 var_count 同步 */
    ctx->var_count = solver->var_count;

    /* 初始状态 */
    if (ctx->state == CDCL_IDLE) {
        ctx->state = CDCL_PROPAGATING;
    }

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
                    /* 决策层 0 冲突 = UNSAT */
                    if (ctx->decision_level == 0) {
                        ctx->state = CDCL_UNSAT;
                    } else {
                        ctx->state = CDCL_ANALYZING;
                    }
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
                    if (ctx->restarts < CDCL_MAX_RESTARTS &&
                        ctx->conflicts > 0 &&
                        (int)ctx->conflicts % solver->config.restart_interval == 0) {
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

    /* 步数耗尽 */
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

    /* 运行 CDCL 状态机 */
    CDCLState final_state = cdcl_run(solver);

    switch (final_state) {
        case CDCL_SATISFIED: {
            /* 将 CDCL 赋值同步回 solver->values */
            CDCLContext *ctx = &solver->cdcl;
            for (int v = 1; v <= solver->var_count && v <= ctx->var_count; v++) {
                if (v <= solver->var_capacity) {
                    solver->values[v - 1] = ctx->assigns[v];
                }
            }
            return LV00_SOLVER_SAT;
        }
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

    /* 返回最近学习子句作为冲突集 */
    const CDCLContext *ctx = &solver->cdcl;
    int total = ctx->orig_clause_count + ctx->learn_clause_count;

    if (ctx->learn_clause_count > 0 && ctx->clauses) {
        /* 返回最后一个学习子句 */
        int idx = total - 1;
        int sz = ctx->clause_sizes[idx];
        int *set = (int *)lv00_malloc((size_t)(sz + 1) * sizeof(int));
        if (set) {
            memcpy(set, ctx->clauses[idx], (size_t)(sz + 1) * sizeof(int));
            *out_count = sz;
            return set;
        }
    }

    /* 无学习子句，返回空冲突集 */
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

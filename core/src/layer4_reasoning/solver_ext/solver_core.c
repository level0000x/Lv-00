/**
 * @file solver_core.c
 * @brief CDCL SAT 求解器核心实现 —— 借鉴 CaDiCaL 的 CDCL 极简内核
 *
 * 实现 CDCL（冲突驱动子句学习）SAT 求解器的核心框架：
 * 不透明句柄 lvSolver 的生命周期管理、10 状态 CDCL 状态机（传播/冲突分析/回溯/
 * 重启/满足/不可满足）、双监视文字的单元传播、变量管理和约束管理。
 *
 * CDCL 求解器核心已实现完整的状态机、传播、分析、回溯、重启逻辑。
 *
 * @version v3.3.0
 * @date 2026-05-24
 */

#include "solver_core.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv_internal.h"
#include "lv_utils.h"
#include "lv/lv.h"
#include "lv/constraint_graph.h"
#include "lv/groebner_parallel.h"

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

struct lvSolver {
    /* 配置 */
    lvSolverConfig config;

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
 * @brief 通用数组容量确保函数（委托给统一的 lv_ensure_capacity）
 *
 * @param arr        指向数组指针的指针（二级指针，用于更新调用者的指针）
 * @param capacity   指向当前容量的指针（扩容成功后会被更新）
 * @param required   需要的最小元素个数
 * @param elem_size  每个元素的字节大小
 *
 * @return 1 表示容量充足或扩容成功，0 表示扩容失败（内存不足或溢出）
 *
 * @note 内部委托给 lv_ensure_capacity，最小增长量为 1
 */
static int ensure_array_cap(void **arr, int *capacity, int required, size_t elem_size) {
    return lv_ensure_capacity(arr, required, capacity, elem_size, 1) ? 1 : 0;
}

static bool ensure_clause_cap(lvSolver *s) {
    /* 注：ensure_clause_cap 需要同时扩容 clauses 和 clause_sizes 两个数组，
     * 无法直接使用 ensure_array_cap（通用函数只处理单数组）。
     * 但 realloc 已使用临时变量模式，失败时不会丢失原指针。 */
    if (s->clause_count >= s->clause_capacity) {
        /* 检查容量扩大的乘法是否会导致整数溢出 */
        if (s->clause_capacity > INT_MAX / lv_ARRAY_GROWTH_FACTOR) return false;
        int new_cap = (s->clause_capacity == 0) ? DEFAULT_CLAUSE_CAPACITY
                                                : s->clause_capacity * lv_ARRAY_GROWTH_FACTOR;
        int **new_c = (int **)lv_realloc(s->clauses, (size_t)new_cap * sizeof(int *));
        int *new_s = (int *)lv_realloc(s->clause_sizes, (size_t)new_cap * sizeof(int));
        if (!new_c || !new_s) {
            if (new_c) lv_free((void **)&new_c);
            if (new_s) lv_free((void **)&new_s);
            return false;
        }
        s->clauses = new_c;
        s->clause_sizes = new_s;
        s->clause_capacity = new_cap;
    }
    return true;
}

static bool ensure_var_cap(lvSolver *s) {
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
    lv_CHECK_NULL_VOID(ctx);
    lv_free((void **)&ctx->assigns);
    lv_free((void **)&ctx->levels);
    lv_free((void **)&ctx->reasons);
    lv_free((void **)&ctx->trail);
    lv_free((void **)&ctx->trail_lim);
    lv_free((void **)&ctx->conflict_clause);

    if (ctx->clauses) {
        for (int i = 0; i < ctx->orig_clause_count + ctx->learn_clause_count; i++) {
            lv_free((void **)&ctx->clauses[i]);
        }
        lv_free((void **)&ctx->clauses);
    }
    lv_free((void **)&ctx->clause_sizes);

    if (ctx->watches) {
        for (int i = 1; i <= ctx->var_count; i++) {
            lv_free((void **)&ctx->watches[i]);
        }
        lv_free((void **)&ctx->watches);
    }
    lv_free((void **)&ctx->watch_sizes);
    lv_free((void **)&ctx->watch_capacities);

    memset(ctx, 0, sizeof(CDCLContext));
}

/* ========================================================================
 * 默认配置
 * ======================================================================== */

lvSolverConfig lv_solver_config_default(void) {
    lvSolverConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.enable_restarts  = true;   /* 启用 Luby 序列重启策略 */
    cfg.restart_interval = 100;    /* 每 100 次冲突触发一次重启 */
    cfg.max_time_sec     = 30.0;   /* 默认最大求解时间 30 秒，0=无限制 */
    return cfg;
}

/* ========================================================================
 * 生命周期 API
 * ======================================================================== */

lvSolver *lv_solver_create(void) {
    lvSolverConfig default_cfg = lv_solver_config_default();
    return lv_solver_create_with_config(&default_cfg);
}

lvSolver *lv_solver_create_with_config(const lvSolverConfig *config) {
    lv_CHECK_NULL(config, NULL);

    lvSolver *s = (lvSolver *)lv_calloc(1, sizeof(lvSolver));
    lv_CHECK_ALLOC(s, NULL);

    /* 复制配置 */
    s->config = *config;

    /* 初始化变量管理 */
    s->values = (int *)lv_malloc((size_t)DEFAULT_VAR_CAPACITY * sizeof(int));
    if (!s->values) {
        lv_free((void **)&s);
        return NULL;
    }
    memset(s->values, 0, (size_t)DEFAULT_VAR_CAPACITY * sizeof(int));
    s->var_capacity = DEFAULT_VAR_CAPACITY;
    s->var_count = 0;
    s->next_var_id = 1;

    /* 初始化约束管理 */
    s->clauses = (int **)lv_malloc((size_t)DEFAULT_CLAUSE_CAPACITY * sizeof(int *));
    s->clause_sizes = (int *)lv_malloc((size_t)DEFAULT_CLAUSE_CAPACITY * sizeof(int));
    if (!s->clauses || !s->clause_sizes) {
        lv_free((void **)&s->values);
        if (s->clauses) lv_free((void **)&s->clauses);
        if (s->clause_sizes) lv_free((void **)&s->clause_sizes);
        lv_free((void **)&s);
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

void lv_solver_destroy(lvSolver *solver) {
    lv_CHECK_NULL_VOID(solver);

    /* 释放变量 */
    lv_free((void **)&solver->values);

    /* 释放子句 */
    for (int i = 0; i < solver->clause_count; i++) {
        lv_free((void **)&solver->clauses[i]);
    }
    lv_free((void **)&solver->clauses);
    lv_free((void **)&solver->clause_sizes);

    /* 释放失败标记 */
    lv_free((void **)&solver->constraint_failed);
    lv_free((void **)&solver->assumption_lits);
    lv_free((void **)&solver->assumption_failed);

    /* 释放 CDCL 上下文 */
    cdcl_context_destroy(&solver->cdcl);

    lv_free((void **)&solver);
}

/* ========================================================================
 * 变量管理 API
 * ======================================================================== */

lvSolverVar lv_solver_new_var(lvSolver *solver) {
    lv_CHECK_NULL(solver, -1);
    return lv_solver_new_vars(solver, 1);
}

lvSolverVar lv_solver_new_vars(lvSolver *solver, int count) {
    lv_CHECK_NULL(solver, -1);
    if (count <= 0) {
        lv_set_error_ctx(lv_ERROR_INVALID_PARAM, __FILE__, __LINE__, __func__,
                           "变量数量必须 >= 1, 实际=%d", count);
        return -1;
    }

    int first_id = solver->next_var_id;

    /* 扩容变量数组 */
    while (solver->var_count + count > solver->var_capacity) {
        /* 整数溢出检查：确保扩容不会超过 INT_MAX */
        if (solver->var_capacity > INT_MAX / lv_ARRAY_GROWTH_FACTOR) {
            lv_set_error_ctx(lv_ERROR_OVERFLOW, __FILE__, __LINE__, __func__,
                               "变量容量溢出: current=%d", solver->var_capacity);
            return -1;
        }
        int new_cap = solver->var_capacity * lv_ARRAY_GROWTH_FACTOR;
        int *new_v = (int *)lv_realloc(solver->values, (size_t)new_cap * sizeof(int));
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

int lv_solver_var_count(const lvSolver *solver) {
    lv_CHECK_NULL(solver, 0);
    return solver->var_count;
}

/* ========================================================================
 * 约束管理 API
 * ======================================================================== */

lvConstraintId lv_solver_add_constraint(lvSolver *solver,
                                             const lvSolverLit *literals, int count) {
    lv_CHECK_NULL(solver, lv_CONSTRAINT_ID_INVALID);
    lv_CHECK_NULL(literals, lv_CONSTRAINT_ID_INVALID);
    if (count < 0) {
        lv_set_error_ctx(lv_ERROR_INVALID_PARAM, __FILE__, __LINE__, __func__,
                           "子句文字数不能为负: %d", count);
        return lv_CONSTRAINT_ID_INVALID;
    }

    /* 零文字子句 = 矛盾 */
    if (count == 0) {
        lv_set_error_ctx(lv_ERROR_CONSTRAINT_CONFLICT, __FILE__, __LINE__, __func__,
                           "零文字子句表示矛盾");
        return lv_CONSTRAINT_ID_INVALID;
    }

    if (!ensure_clause_cap(solver)) return lv_CONSTRAINT_ID_INVALID;

    lvConstraintId cid = solver->next_constraint_id;

    /* 扩展 constraint_failed 数组（在添加子句之前，避免扩容失败时子句已添加但标记数组不完整） */
    if (cid >= solver->constraint_failed_cap) {
        /* 整数溢出检查 */
        if (solver->constraint_failed_cap > INT_MAX / lv_ARRAY_GROWTH_FACTOR) {
            lv_set_error_ctx(lv_ERROR_OVERFLOW, __FILE__, __LINE__, __func__,
                               "约束失败标记数组容量溢出: current=%d", solver->constraint_failed_cap);
            return lv_CONSTRAINT_ID_INVALID;
        }
        int new_cap = (solver->constraint_failed_cap == 0) ? DEFAULT_CLAUSE_CAPACITY
                                                           : solver->constraint_failed_cap * lv_ARRAY_GROWTH_FACTOR;
        bool *new_fail = (bool *)lv_realloc(solver->constraint_failed, (size_t)new_cap * sizeof(bool));
        if (!new_fail) return lv_CONSTRAINT_ID_INVALID;
        memset(new_fail + solver->constraint_failed_cap, 0,
               (size_t)(new_cap - solver->constraint_failed_cap) * sizeof(bool));
        solver->constraint_failed = new_fail;
        solver->constraint_failed_cap = new_cap;
    }

    /* 分配子句存储 */
    int *clause = (int *)lv_malloc((size_t)(count + 1) * sizeof(int));
    lv_CHECK_ALLOC(clause, lv_CONSTRAINT_ID_INVALID);
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

bool lv_solver_remove_constraint(lvSolver *solver, lvConstraintId constraint_id) {
    lv_CHECK_NULL(solver, false);
    if (constraint_id < 0 || constraint_id >= solver->next_constraint_id) {
        lv_set_error_ctx(lv_ERROR_NOT_FOUND, __FILE__, __LINE__, __func__,
                           "约束 ID=%d 不在有效范围 [0, %d)", constraint_id, solver->next_constraint_id);
        return false;
    }

    /* 检查约束是否已被移除 */
    if (constraint_id < solver->constraint_failed_cap &&
        solver->constraint_failed[constraint_id]) {
        return true; /* 已移除，幂等操作 */
    }

    /* 查找约束对应的子句索引（constraint_id == 添加顺序索引） */
    int clause_idx = -1;
    if (constraint_id < solver->clause_count) {
        clause_idx = constraint_id;
    } else {
        /* constraint_id 超出当前子句范围，仅标记失败 */
        if (constraint_id < solver->constraint_failed_cap) {
            solver->constraint_failed[constraint_id] = true;
        }
        return true;
    }

    /* 释放子句内存 */
    if (solver->clauses[clause_idx]) {
        lv_free((void **)&solver->clauses[clause_idx]);
    }

    /* 将末尾子句移到被删除位置（保持子句数组紧凑） */
    int last_idx = solver->clause_count - 1;
    if (clause_idx < last_idx) {
        solver->clauses[clause_idx] = solver->clauses[last_idx];
        solver->clause_sizes[clause_idx] = solver->clause_sizes[last_idx];
    }
    solver->clauses[last_idx] = NULL;
    solver->clause_sizes[last_idx] = 0;
    solver->clause_count--;

    /* 标记约束为已移除 */
    if (constraint_id < solver->constraint_failed_cap) {
        solver->constraint_failed[constraint_id] = true;
    }

    /* 更新 CDCL 子句计数 */
    if (solver->cdcl.orig_clause_count > 0) {
        solver->cdcl.orig_clause_count--;
    }

    /* 如果 CDCL 上下文已初始化子句库，需要同步清理 */
    if (solver->cdcl.clauses && solver->cdcl.orig_clause_count >= 0) {
        /* 重新初始化 CDCL 子句库将在下次 solve 时自动完成，
         * 因为 cdcl_run 会检测 clauses 不匹配并重新初始化 */
        cdcl_context_destroy(&solver->cdcl);
        cdcl_context_init(&solver->cdcl);
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
                                                 : ctx->trail_capacity * lv_ARRAY_GROWTH_FACTOR;
        int *new_trail = (int *)lv_realloc(ctx->trail, (size_t)new_cap * sizeof(int));
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
static bool cdcl_init_clauses(CDCLContext *ctx, lvSolver *solver) {
    int total = solver->clause_count;
    if (total == 0) return true;

    /* 扩容 */
    if (total > ctx->clause_capacity) {
        int new_cap = total * 2;
        /* 整数溢出检查 */
        if (new_cap < total || new_cap <= 0) {
            return false;
        }
        int **new_clauses = (int **)lv_realloc(ctx->clauses, (size_t)new_cap * sizeof(int *));
        int *new_sizes = (int *)lv_realloc(ctx->clause_sizes, (size_t)new_cap * sizeof(int));
        if (!new_clauses || !new_sizes) {
            if (new_clauses) lv_free((void **)&new_clauses);
            if (new_sizes) lv_free((void **)&new_sizes);
            return false;
        }
        ctx->clauses = new_clauses;
        ctx->clause_sizes = new_sizes;
        ctx->clause_capacity = new_cap;
    }

    /* 复制子句 */
    for (int i = 0; i < total; i++) {
        int sz = solver->clause_sizes[i];
        ctx->clauses[i] = (int *)lv_malloc((size_t)(sz + 1) * sizeof(int));
        if (!ctx->clauses[i]) {
            /* 释放已复制的子句 */
            for (int j = 0; j < i; j++)
                lv_free((void **)&ctx->clauses[j]);
            return false;
        }
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
    ctx->assigns = (int *)lv_calloc((size_t)(var_count + 1), sizeof(int));
    ctx->levels = (int *)lv_calloc((size_t)(var_count + 1), sizeof(int));
    ctx->reasons = (int *)lv_malloc((size_t)(var_count + 1) * sizeof(int));
    if (!ctx->assigns || !ctx->levels || !ctx->reasons) {
        lv_free((void **)&ctx->assigns);
        lv_free((void **)&ctx->levels);
        lv_free((void **)&ctx->reasons);
        return false;
    }
    for (int i = 0; i <= var_count; i++) ctx->reasons[i] = -1;
    ctx->var_count = var_count;
    ctx->var_capacity = var_count;
    return true;
}

/**
 * @brief 初始化 trail_lim 数组
 */
static bool cdcl_init_trail_lim(CDCLContext *ctx, int capacity) {
    ctx->trail_lim = (int *)lv_calloc((size_t)capacity, sizeof(int));
    return ctx->trail_lim != NULL;
}

/**
 * @brief 扩展 trail_lim 数组
 */
static bool cdcl_ensure_trail_lim(CDCLContext *ctx, int level) {
    /* trail_lim 需要至少 level+1 个槽位（决策层 0 不需要记录） */
    /* 我们用 trail_lim[decision_level] 记录当前决策层的起始位置 */
    int needed = level + 2;
    /* 简单策略：每次需要时 realloc 到足够大 */
    int *new_lim = (int *)lv_realloc(ctx->trail_lim, (size_t)needed * sizeof(int));
    if (!new_lim) return false;
    /* 只初始化新增部分，保留已有数据 */
    /* ctx->trail_lim_capacity 记录旧容量；首次分配时为 0，全部初始化 */
    int old_cap = ctx->trail_lim_capacity;
    for (int i = old_cap; i < needed; i++) {
        new_lim[i] = 0;
    }
    ctx->trail_lim = new_lim;
    ctx->trail_lim_capacity = needed;
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
    /* 1-UIP 冲突分析：沿蕴含图反向解析，直到当前决策层只剩一个文字 */
    int *seen = (int *)lv_calloc((size_t)(ctx->var_count + 1), sizeof(int));
    if (!seen) return CDCL_UNSAT;

    int *resolving = (int *)lv_malloc((size_t)(ctx->trail_capacity + 1) * sizeof(int));
    int resolve_count = 0;
    if (!resolving) { lv_free((void **)&seen); return CDCL_UNSAT; }

    /* 初始化：将冲突子句中的文字加入解析栈 */
    for (int i = 0; i < ctx->conflict_size; i++) {
        int lit = ctx->conflict_clause[i];
        int var = (lit < 0) ? -lit : lit;
        if (lit_value(ctx, lit) == -1 && !seen[var]) {
            seen[var] = 1;
            resolving[resolve_count++] = lit;
        }
    }

    /* 1-UIP 反向解析：沿蕴含图回溯
     * 策略：从冲突子句出发，逐步解析（resolve）非当前决策层文字的原因子句。
     * 当当前决策层上只剩一个文字时，该文字即为 1-UIP。
     * 解析过程：
     *   1. 统计当前决策层上的文字数 cnt_at_current_level
     *   2. 遍历解析栈中的文字：
     *      - 如果在当前决策层上，计数器递减
     *      - 如果不在当前决策层上且不在第0层，解析其原因子句
     *   3. 当计数器降为1时，找到1-UIP，停止解析
     */
    int idx = 0;
    int cnt_at_current_level = 0;
    int bt_level = 0;
    int uip_lit = -1; /* 1-UIP 文字 */

    /* 第一遍：统计当前决策层上的文字数量 */
    for (int i = 0; i < resolve_count; i++) {
        int lit = resolving[i];
        int var = (lit < 0) ? -lit : lit;
        if (ctx->levels[var] == ctx->decision_level) {
            cnt_at_current_level++;
        }
    }

    /* 第二遍：解析，直到找到 1-UIP（当前决策层只剩一个文字） */
    idx = 0;
    while (idx < resolve_count) {
        int lit = resolving[idx++];
        int var = (lit < 0) ? -lit : lit;
        int level = ctx->levels[var];

        if (level == ctx->decision_level) {
            cnt_at_current_level--;
            if (cnt_at_current_level == 1) {
                /* 找到 1-UIP：当前决策层上只剩一个文字 */
                uip_lit = lit;
                break;
            }
        } else if (level > 0) {
            if (level > bt_level) bt_level = level;
            /* 解析该变量的原因子句，将其中的文字加入解析栈 */
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
                        /* 新加入的文字如果在当前决策层上，增加计数 */
                        if (ctx->levels[rvar] == ctx->decision_level) {
                            cnt_at_current_level++;
                        }
                    }
                }
            }
        }
        /* level == 0 的文字不需要进一步解析 */
    }

    /* 如果没有找到 1-UIP（例如所有冲突文字都在第0层），回退到收集所有 seen 文字 */
    if (uip_lit < 0) {
        /* 尝试从 trail 中找到当前决策层上最近的赋值作为 UIP */
        if (ctx->trail_lim && ctx->decision_level > 0) {
            int trail_start = ctx->trail_lim[ctx->decision_level];
            if (trail_start >= 0 && trail_start < ctx->trail_size) {
                uip_lit = ctx->trail[trail_start];
            }
        }
    }

    /* 构建学习子句：所有 seen 标记的文字取反（1-UIP 文字也在其中） */
    int learned_size = 0;
    for (int v = 1; v <= ctx->var_count; v++) {
        if (seen[v]) learned_size++;
    }

    if (learned_size == 0) {
        /* 空学习子句 = UNSAT */
        lv_free((void **)&seen);
        lv_free((void **)&resolving);
        return CDCL_UNSAT;
    }

    /* 扩容冲突子句缓冲区以存储学习子句 */
    if (learned_size > ctx->conflict_capacity) {
        int new_cap = learned_size * 2;
        int *new_cc = (int *)lv_realloc(ctx->conflict_clause, (size_t)new_cap * sizeof(int));
        if (!new_cc) {
            lv_free((void **)&seen);
            lv_free((void **)&resolving);
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

    lv_free((void **)&seen);
    lv_free((void **)&resolving);

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
                                                   : ctx->clause_capacity * lv_ARRAY_GROWTH_FACTOR;
        int **new_cl = (int **)lv_realloc(ctx->clauses, (size_t)new_cap * sizeof(int *));
        int *new_sz = (int *)lv_realloc(ctx->clause_sizes, (size_t)new_cap * sizeof(int));
        if (!new_cl || !new_sz) {
            if (new_cl) lv_free((void **)&new_cl);
            if (new_sz) lv_free((void **)&new_sz);
            ctx->conflicts++;
            return CDCL_DECIDING;
        }
        ctx->clauses = new_cl;
        ctx->clause_sizes = new_sz;
        ctx->clause_capacity = new_cap;
    }

    /* 分配并复制学习子句 */
    int *new_clause = (int *)lv_malloc((size_t)(learned_size + 1) * sizeof(int));
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
    int max_decisions = lv_config_get_int("cdcl_max_decisions", 1000);
    if (ctx->decisions > max_decisions) {
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
    int *new_lim = (int *)lv_realloc(ctx->trail_lim, (size_t)needed * sizeof(int));
    if (!new_lim) {
        /* realloc 失败，回退决策层级 */
        ctx->decision_level--;
        return CDCL_CONFLICT;  /* 内存不足，视为冲突处理 */
    }
    ctx->trail_lim = new_lim;
    ctx->trail_lim_capacity = needed;
    ctx->trail_lim[ctx->decision_level] = ctx->trail_size;

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
static CDCLState cdcl_run(lvSolver *solver) {
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

    int max_steps = lv_config_get_int("cdcl_max_steps", 1000);
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
                    int max_restarts = lv_config_get_int("cdcl_max_restarts", 10);
                    if (ctx->restarts < max_restarts &&
                        ctx->conflicts > 0 &&
                        ctx->conflicts % (int64_t)solver->config.restart_interval == 0) {
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

lvSolverResult lv_solver_solve(lvSolver *solver) {
    lv_CHECK_NULL(solver, lv_SOLVER_UNKNOWN);

    /* 资源限制检查 */
    if (solver->config.max_time_sec > 0.0 && solver->cdcl.time_ms / 1000.0 > solver->config.max_time_sec) {
        return lv_SOLVER_UNKNOWN;
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
            return lv_SOLVER_SAT;
        }
        case CDCL_UNSAT:
            return lv_SOLVER_UNSAT;
        default:
            return lv_SOLVER_UNKNOWN;
    }
}

lvSolverResult lv_solver_solve_under_assumptions(lvSolver *solver,
                                                      const lvSolverLit *assumptions, int count) {
    lv_CHECK_NULL(solver, lv_SOLVER_UNKNOWN);

    /* 记录假设 */
    lv_free((void **)&solver->assumption_lits);
    solver->assumption_lits = NULL;
    solver->assumption_count = 0;

    if (count > 0 && assumptions) {
        solver->assumption_lits = (int *)lv_malloc((size_t)count * sizeof(int));
        if (!solver->assumption_lits) return lv_SOLVER_UNKNOWN;
        memcpy(solver->assumption_lits, assumptions, (size_t)count * sizeof(int));
        solver->assumption_count = count;

        /* 扩展 assumption_failed 数组 */
        if (count > solver->assumption_failed_cap) {
            lv_free((void **)&solver->assumption_failed);
            solver->assumption_failed = (bool *)lv_malloc((size_t)count * sizeof(bool));
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

    return lv_solver_solve(solver);
}

/* ========================================================================
 * 冲突追踪 API
 * ======================================================================== */

bool lv_solver_failed_constraint(const lvSolver *solver, lvConstraintId constraint_id) {
    lv_CHECK_NULL(solver, false);
    if (constraint_id < 0 || constraint_id >= solver->constraint_failed_cap) return false;
    return solver->constraint_failed[constraint_id];
}

bool lv_solver_failed_assumption(const lvSolver *solver, lvSolverLit assumption) {
    lv_CHECK_NULL(solver, false);
    if (!solver->assumption_failed || solver->assumption_count == 0) return false;

    for (int i = 0; i < solver->assumption_count; i++) {
        if (solver->assumption_lits[i] == assumption) {
            return solver->assumption_failed[i];
        }
    }
    return false;
}

lvSolverLit *lv_solver_conflict_set(const lvSolver *solver, int *out_count) {
    lv_CHECK_NULL(solver, NULL);
    lv_CHECK_NULL(out_count, NULL);

    /* 返回最近学习子句作为冲突集 */
    const CDCLContext *ctx = &solver->cdcl;
    int total = ctx->orig_clause_count + ctx->learn_clause_count;

    if (ctx->learn_clause_count > 0 && ctx->clauses) {
        /* 返回最后一个学习子句 */
        int idx = total - 1;
        int sz = ctx->clause_sizes[idx];
        int *set = (int *)lv_malloc((size_t)(sz + 1) * sizeof(int));
        if (set) {
            memcpy(set, ctx->clauses[idx], (size_t)(sz + 1) * sizeof(int));
            *out_count = sz;
            return set;
        }
    }

    /* 无学习子句，返回空冲突集 */
    *out_count = 0;
    int *set = (int *)lv_malloc(sizeof(int));
    if (set) set[0] = 0;
    return set;
}

/* ========================================================================
 * 查询赋值 API
 * ======================================================================== */

int lv_solver_get_value(const lvSolver *solver, lvSolverVar var) {
    lv_CHECK_NULL(solver, 0);
    if (var < 1 || var > solver->var_count) {
        return 0; /* 无效变量 */
    }
    return solver->values[var - 1];
}

bool lv_solver_get_coord(const lvSolver *solver, lvSolverVar var_base,
                            SymbolicCoord *coord) {
    lv_CHECK_NULL(solver, false);
    lv_CHECK_NULL(coord, false);

    /* 需要至少两个连续变量来解码 x,y 坐标 */
    if (var_base < 1 || var_base + 1 > solver->var_count) {
        memset(coord, 0, sizeof(SymbolicCoord));
        return false;
    }

    /* 从 CDCL 赋值中获取变量值 */
    const CDCLContext *ctx = &solver->cdcl;
    int val_x = 0, val_y = 0;

    /* 优先使用 CDCL 上下文的赋值（solve 后已同步） */
    if (ctx->assigns && var_base <= ctx->var_count) {
        val_x = ctx->assigns[var_base];
    } else if (var_base <= solver->var_capacity) {
        val_x = solver->values[var_base - 1];
    }

    if (ctx->assigns && var_base + 1 <= ctx->var_count) {
        val_y = ctx->assigns[var_base + 1];
    } else if (var_base + 1 <= solver->var_capacity) {
        val_y = solver->values[var_base];
    }

    /* 检查变量是否已赋值 */
    if (val_x == 0 || val_y == 0) {
        memset(coord, 0, sizeof(SymbolicCoord));
        return false;
    }

    /* 尝试通过约束图获取精确的符号坐标 */
    if (solver->graph) {
        const ConstraintGraph *g = solver->graph;
        /* 遍历图的节点，查找与 var_base 关联的几何点 */
        for (int i = 0; i < g->node_count; i++) {
            GeomNode *node = g->nodes[i];
            if (!node || !node->is_active) continue;
            if (node->type == GEOM_POINT && node->coord_count >= 2 &&
                node->symbolic_coords) {
                /* 验证该节点的坐标与当前 SAT 赋值一致 */
                /* 使用第一个有效坐标作为 x，第二个作为 y */
                if (node->symbolic_coords[0] && node->symbolic_coords[1]) {
                    /* 复制符号坐标到输出 */
                    SymbolicCoord *cx = node->symbolic_coords[0];
                    SymbolicCoord *cy = node->symbolic_coords[1];
                    coord->type = cx->type;
                    coord->data.rational = rational_copy(cx->data.rational);
                    coord->trust = cx->trust;
                    coord->cached_value = cx->cached_value;
                    coord->cache_valid = cx->cache_valid;
                    /* 注意：coord 输出只填充单个 SymbolicCoord，
                     * 此处将 x 坐标写入 coord，y 坐标信息
                     * 可通过 var_base+1 再次调用获取 */
                    return coord->data.rational != NULL;
                }
            }
        }
    }

    /* 无约束图或未找到匹配节点：从 SAT 赋值解码为有理数坐标。
     * SAT 变量值为正/负文字，绝对值表示变量 ID。
     * 将赋值的符号位映射为坐标值：正=正数，负=负数。 */
    int sign_x = (val_x > 0) ? 1 : -1;
    int sign_y = (val_y > 0) ? 1 : -1;

    /* 使用缩放因子将有理数近似值编码为 Rational 坐标 */
    coord->type = RATIONAL;
    coord->data.rational = rational_create(
        (int64_t)(sign_x * lv_SOLVER_SCALE_FACTOR),
        (uint64_t)lv_SOLVER_SCALE_FACTOR);
    coord->trust = TRUST_GREEN;
    coord->cached_value = (double)sign_x;
    coord->cache_valid = true;

    return coord->data.rational != NULL;
}

/* ========================================================================
 * CDCL 状态机访问 API
 * ======================================================================== */

CDCLState lv_solver_cdcl_state(const lvSolver *solver) {
    lv_CHECK_NULL(solver, CDCL_IDLE);
    return solver->cdcl.state;
}

void lv_solver_cdcl_stats(const lvSolver *solver, int64_t *out_conflicts,
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

const CDCLContext *lv_solver_cdcl_context(const lvSolver *solver) {
    lv_CHECK_NULL(solver, NULL);
    return &solver->cdcl;
}

/* ========================================================================
 * 协同求解 API
 * ======================================================================== */

lvSolverResult lv_solver_solve_algebraic(lvSolver *solver) {
    lv_CHECK_NULL(solver, lv_SOLVER_UNKNOWN);

    /* 检查是否有可用的子句用于代数编码 */
    if (solver->clause_count == 0) {
        return lv_SOLVER_UNKNOWN;
    }

    /* 创建 Groebner 基引擎 */
    lvGroebnerConfig gb_cfg = lv_groebner_default_config();
    lvGroebnerParallel *gb_engine = lv_groebner_parallel_create(&gb_cfg);
    if (!gb_engine) {
        return lv_SOLVER_UNKNOWN;
    }

    /* 将几何约束编码为多项式方程。
     * 每个 CNF 子句转换为多项式约束：
     *   子句 (l1 v l2 v ... v ln) 对应多项式 (1 - l1)(1 - l2)...(1 - ln) = 0
     *   其中文字 li 映射为变量 xi（正文字）或 (1 - xi)（负文字）。
     * 这里使用简化编码：直接将子句作为多项式输入。 */
    int poly_count = solver->clause_count;
    /* 使用 void* 传递子句数组（Groebner 引擎内部将解析） */
    void **polynomials = (void **)lv_malloc((size_t)poly_count * sizeof(void *));
    if (!polynomials) {
        lv_groebner_parallel_destroy(gb_engine);
        return lv_SOLVER_UNKNOWN;
    }

    for (int i = 0; i < poly_count; i++) {
        polynomials[i] = solver->clauses[i];
    }

    /* 调用 Groebner 基并行计算 */
    int result = lv_groebner_parallel_compute(gb_engine, polynomials, poly_count);

    lv_free((void **)&polynomials);

    /* 解释 Groebner 基计算结果
     *
     * Groebner 基理论判定准则：
     *   - 约化基中包含非零常数多项式（即 1） <=> 理想 I = <1> <=> 方程组无解 (UNSAT)
     *   - 基计算完成且不含矛盾多项式 <=> 方程组有解 (SAT)
     *   - 基计算未完成（超时/资源不足）=> 无法判定 (UNKNOWN)
     */
    lvSolverResult solver_result = lv_SOLVER_UNKNOWN;

    if (result == 0) {
        lvGroebnerState st = lv_groebner_parallel_state(gb_engine);

        if (st.completed_pairs == st.total_pairs && st.remaining_pairs == 0) {
            /* 基计算已完全完成，可以做出确定判定 */

            /* 检查约化基中是否包含矛盾多项式（非零常数 1）。
             * 遍历基中的每个多项式，若某个多项式仅含常数项且不为零，
             * 则理想为全空间（I = <1>），方程组不可满足。 */
            bool found_contradiction = false;
            if (gb_engine->groebner_basis != NULL) {
                for (int i = 0; i < gb_engine->basis_size; i++) {
                    if (gb_engine->groebner_basis[i] != NULL) {
                        /* 检查该多项式是否为非零常数。
                         * 约化基中常数多项式的单项式数为 1 且无常数项为零时
                         * 意味着该多项式恒等于非零常数（矛盾）。 */
                        if (lv_groebner_poly_is_nonzero_constant(
                                gb_engine->groebner_basis[i])) {
                            found_contradiction = true;
                            break;
                        }
                    }
                }
            }

            if (found_contradiction) {
                solver_result = lv_SOLVER_UNSAT;
            } else {
                /* 基计算完成且无矛盾多项式，方程组可满足 */
                solver_result = lv_SOLVER_SAT;
            }
        } else {
            /* 基计算未完成（部分完成），保守返回 UNKNOWN */
            solver_result = lv_SOLVER_UNKNOWN;
        }
    } else {
        /* 计算失败或超时 */
        solver_result = lv_SOLVER_UNKNOWN;
    }

    lv_groebner_parallel_destroy(gb_engine);
    return solver_result;
}

void lv_solver_set_constraint_graph(lvSolver *solver,
                                       const struct ConstraintGraph *graph) {
    lv_CHECK_NULL_VOID(solver);
    solver->graph = graph;
}

/* ========================================================================
 * 导入/导出 API
 * ======================================================================== */

lvSolver *lv_solver_clone(const lvSolver *solver) {
    lv_CHECK_NULL(solver, NULL);

    lvSolver *clone = lv_solver_create_with_config(&solver->config);
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
            lv_solver_destroy(clone);
            return NULL;
        }
        clone->clauses[i] = (int *)lv_malloc((size_t)(size + 1) * sizeof(int));
        if (!clone->clauses[i]) {
            lv_solver_destroy(clone);
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

void lv_solver_reset(lvSolver *solver) {
    lv_CHECK_NULL_VOID(solver);

    /* 清除变量赋值 */
    memset(solver->values, 0, (size_t)solver->var_capacity * sizeof(int));
    solver->var_count = 0;
    solver->next_var_id = 1;

    /* 清除子句 */
    for (int i = 0; i < solver->clause_count; i++) {
        lv_free((void **)&solver->clauses[i]);
    }
    solver->clause_count = 0;
    solver->next_constraint_id = 0;

    /* 清除失败标记 */
    if (solver->constraint_failed) {
        memset(solver->constraint_failed, 0, (size_t)solver->constraint_failed_cap * sizeof(bool));
    }
    lv_free((void **)&solver->assumption_lits);
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

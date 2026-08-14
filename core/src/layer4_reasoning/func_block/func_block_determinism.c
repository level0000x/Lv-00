/**
 * @file func_block_determinism.c
 * @brief 函数块确定性检查模块
 * @details 实现函数块的静态/动态确定性检查、确定性验证流水线。
 *          包含约束统计、自由度分析、Groebner 基求解等核心逻辑。
 *
 * INTERNAL NOTE: 本文件使用 goto 清理路径模式（24 处）。
 *   对于错误清理场景这是 C 语言惯用法，不做修改。
 *   若新增代码应考虑拆分超过 200 行的函数。
 *
 * @author Lv-00 Project
 * @version 3.0.1
 */

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/solver.h"

#include "lv/func_block.h"
#include "lv/func_block_internal.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/stream.h"
#include "lv/stream_context_util.h"

/* ==================== 命名常量 ==================== */

/** 二次约束解数量的最大迭代次数 */
#define MAX_QUADRATIC_SOLUTION_ITERATIONS 30

/* ================================================================
 * 查找表：ConstraintType → 计数器递增函数
 * ================================================================ */

/** @brief 约束类型计数器函数类型 */
typedef void (*ConstraintCounterFunc)(DeterminismStaticStats *stats);

static void count_linear(DeterminismStaticStats *stats) { stats->linear_count++; }
static void count_quadratic(DeterminismStaticStats *stats) { stats->quadratic_count++; }
static void count_connection(DeterminismStaticStats *stats) { stats->connection_count++; }

/**
 * @brief 约束类型计数器查找表（按枚举值升序）
 *
 * 索引：INCIDENCE=0, BETWEENNESS=1, INTERSECTION=2,
 *       CONTAINMENT=3, CONNECTION=4, ANGLE=5
 */
static const ConstraintCounterFunc s_constraint_counters[] = {
    count_linear,     /* INCIDENCE */
    count_linear,     /* BETWEENNESS */
    count_quadratic,  /* INTERSECTION */
    count_linear,     /* CONTAINMENT */
    count_connection, /* CONNECTION */
    count_linear,     /* ANGLE */
};
#define lv_CONSTRAINT_COUNTER_COUNT lv_ARRAY_SIZE(s_constraint_counters)

/* ================================================================
 * 查找表：dof_result → 结果映射（DeterminismCheckResult 版本）
 * ================================================================ */

/** @brief dof_result 映射到 (DeterminismState, DeterminismCheckResult) */
typedef struct {
    DeterminismState fb_det;
    DeterminismCheckResult ret;
} DofResultCheckEntry;

/**
 * @brief dof_result 映射表（索引 = dof_result + 1）
 *
 * dof_result = -1 → 索引 0：过约束
 * dof_result =  0 → 索引 1：恰好约束
 * dof_result =  1 → 索引 2：欠约束
 *
 * 【关联说明】本表与下方 s_dof_result_status_map 高度同构（同一
 * dof_result → (fb_det, 返回码) 映射），仅返回码类型不同：
 *   本表返回 DeterminismCheckResult，供 func_block_check_determinism_static()
 *   使用；s_dof_result_status_map 返回 DeterminismStatus，供
 *   func_block_determinism_check_static() 使用。
 * 修改时需保持两表同步（fb_det 字段语义一致）。
 */
static const DofResultCheckEntry s_dof_result_check_map[] = {
    {DETERMINISM_NON_DETERMINISTIC, DETERMINISM_CHECK_NO_SOLUTION},  /* dof_result = -1 */
    {DETERMINISM_VERIFIED, DETERMINISM_CHECK_UNIQUE},                /* dof_result =  0 */
    {DETERMINISM_PARTIALLY_VERIFIED, DETERMINISM_CHECK_MULTIPLE},    /* dof_result =  1 */
};

/* ================================================================
 * 查找表：dof_result → 结果映射（DeterminismStatus 版本）
 * ================================================================ */

/** @brief dof_result 映射到 (DeterminismState, DeterminismStatus) */
typedef struct {
    DeterminismState fb_det;
    DeterminismStatus ret;
} DofResultStatusEntry;

/**
 * @brief dof_result 映射表（索引 = dof_result + 1）
 *
 * dof_result = -1 → 索引 0：过约束
 * dof_result =  0 → 索引 1：恰好约束
 * dof_result =  1 → 索引 2：欠约束
 *
 * 【关联说明】本表与上方 s_dof_result_check_map 高度同构（同一
 * dof_result → (fb_det, 返回码) 映射），仅返回码类型不同：
 *   本表返回 DeterminismStatus，供 func_block_determinism_check_static()
 *   使用；s_dof_result_check_map 返回 DeterminismCheckResult，供
 *   func_block_check_determinism_static() 使用。
 * 修改时需保持两表同步（fb_det 字段语义一致）。
 */
static const DofResultStatusEntry s_dof_result_status_map[] = {
    {DETERMINISM_NON_DETERMINISTIC, DETERMINISM_NON_DETERMINISTIC},  /* dof_result = -1 */
    {DETERMINISM_VERIFIED, DETERMINISM_VERIFIED},                    /* dof_result =  0 */
    {DETERMINISM_PARTIALLY_VERIFIED, DETERMINISM_PARTIALLY_VERIFIED},/* dof_result =  1 */
};

/* ================================================================
 * 查找表：SolverStatus → 静态确定性结果映射
 * ================================================================ */

/** @brief SolverStatus 映射到静态确定性检查结果 */
typedef struct {
    DeterminismState fb_det;
    DeterminismStatus static_res;
} SolverStatusStaticEntry;

/**
 * @brief SolverStatus 静态结果映射表
 *
 * 索引：SOLVER_UNIQUE=1 → 索引 0, SOLVER_MULTIPLE=2 → 1,
 *       SOLVER_NO_SOLUTION=3 → 2, SOLVER_OVERCONSTRAINED=4 → 3
 * 其他状态（SOLVER_OUT_OF_SCOPE, SOLVER_TIMEOUT 等）走默认路径
 */
static const SolverStatusStaticEntry s_solver_status_static_map[] = {
    {DETERMINISM_VERIFIED, DETERMINISM_VERIFIED},                     /* SOLVER_UNIQUE */
    {DETERMINISM_NON_DETERMINISTIC, DETERMINISM_NON_DETERMINISTIC},   /* SOLVER_MULTIPLE */
    {DETERMINISM_NON_DETERMINISTIC, DETERMINISM_NON_DETERMINISTIC},   /* SOLVER_NO_SOLUTION */
    {DETERMINISM_NON_DETERMINISTIC, DETERMINISM_NON_DETERMINISTIC},   /* SOLVER_OVERCONSTRAINED */
};
#define lv_SOLVER_STATUS_STATIC_MAP_COUNT lv_ARRAY_SIZE(s_solver_status_static_map)

/* ================================================================
 * 查找表：SolverStatus → 动态确定性结果映射
 * ================================================================ */

/** @brief SolverStatus 映射到动态确定性检查结果 */
typedef struct {
    DeterminismState fb_det;
    DeterminismStatus dynamic_res;
    bool is_handled;  /* true = goto dynamic_done; false = 走启发式路径 */
} SolverStatusDynamicEntry;

/**
 * @brief SolverStatus 动态结果映射表
 *
 * 索引：SOLVER_UNIQUE=1 → 索引 0, SOLVER_MULTIPLE=2 → 1,
 *       SOLVER_NO_SOLUTION=3 → 2, SOLVER_OVERCONSTRAINED=4 → 3,
 *       SOLVER_OUT_OF_SCOPE=5 → 4, SOLVER_TIMEOUT=6 → 5
 */
static const SolverStatusDynamicEntry s_solver_status_dynamic_map[] = {
    {DETERMINISM_VERIFIED, DETERMINISM_VERIFIED, true},               /* SOLVER_UNIQUE */
    {DETERMINISM_NON_DETERMINISTIC, DETERMINISM_NON_DETERMINISTIC, true}, /* SOLVER_MULTIPLE */
    {DETERMINISM_NON_DETERMINISTIC, DETERMINISM_NON_DETERMINISTIC, true}, /* SOLVER_NO_SOLUTION */
    {DETERMINISM_NON_DETERMINISTIC, DETERMINISM_NON_DETERMINISTIC, true}, /* SOLVER_OVERCONSTRAINED */
    {DETERMINISM_PARTIALLY_VERIFIED, DETERMINISM_PARTIALLY_VERIFIED, false}, /* SOLVER_OUT_OF_SCOPE */
    {DETERMINISM_PARTIALLY_VERIFIED, DETERMINISM_PARTIALLY_VERIFIED, false}, /* SOLVER_TIMEOUT */
};
#define lv_SOLVER_STATUS_DYNAMIC_MAP_COUNT lv_ARRAY_SIZE(s_solver_status_dynamic_map)

/* ================================================================
 * 查找表：DeterminismCheckResult → DeterminismState
 * ================================================================ */

/**
 * @brief DeterminismCheckResult → DeterminismState 映射表
 *
 * 索引：DETERMINISM_CHECK_RESULT_UNIQUE=0, _MULTIPLE=1,
 *       _NO_SOLUTION=2, _TIMEOUT=3, _OUT_OF_RANGE=4
 */
static const DeterminismState s_check_result_to_state[] = {
    DETERMINISM_VERIFIED,           /* DETERMINISM_CHECK_RESULT_UNIQUE */
    DETERMINISM_PARTIALLY_VERIFIED, /* DETERMINISM_CHECK_RESULT_MULTIPLE */
    DETERMINISM_NON_DETERMINISTIC,  /* DETERMINISM_CHECK_RESULT_NO_SOLUTION */
    DETERMINISM_PARTIALLY_VERIFIED, /* DETERMINISM_CHECK_RESULT_TIMEOUT */
    DETERMINISM_PARTIALLY_VERIFIED, /* DETERMINISM_CHECK_RESULT_OUT_OF_RANGE */
};
#define lv_CHECK_RESULT_TO_STATE_COUNT lv_ARRAY_SIZE(s_check_result_to_state)

/* ============== 确定性检查 ============== */

/**
 * @brief 收集节点ID列表并统计涉及内部节点的约束类型
 *
 * 这是两个静态确定性检查函数的共享核心逻辑：
 * 1. 收集内部节点和端口的ID列表
 * 2. 遍历约束图，分类统计涉及内部节点的约束
 * 3. 对于纯线性系统，计算自由度
 *
 * @param fb           函数块（提供内部节点/端口ID）
 * @param graph        约束图
 * @param step_limit   步数上限（0 表示不限制）
 * @param stats        输出参数：填充统计结果
 * @return 分配的 all_ids 数组（调用者负责释放），失败返回 NULL
 */
int *determinism_collect_constraint_stats(const FuncBlock *fb, const ConstraintGraph *graph, int step_limit,
                                          DeterminismStaticStats *stats) {
    if (!fb || !graph || !stats)
        return NULL;

    memset(stats, 0, sizeof(*stats));

    /* 第1步：使用共享辅助函数收集所有内部相关节点ID */
    int *all_ids = NULL;
    int total_count = 0;
    if (!collect_all_block_ids(fb, &all_ids, &total_count)) {
        return NULL;
    }

    /* 第2步：统计约束类型 */
    for (int i = 0; i < graph->constraint_count; i++) {
        if (step_limit > 0 && stats->steps >= step_limit)
            break;
        stats->steps++;

        Constraint *c = graph->constraints[i];

        /* 检查约束是否涉及内部节点 */
        bool involves_internal = false;
        for (int j = 0; j < c->participant_count; j++) {
            if (is_id_in_array(c->participants[j], all_ids, total_count)) {
                involves_internal = true;
                break;
            }
        }
        if (!involves_internal)
            continue;

        if ((unsigned) c->type < lv_CONSTRAINT_COUNTER_COUNT) {
            s_constraint_counters[c->type](stats);
        }
    }

    /* 第3步：对于纯线性约束系统，计算自由度 */
    if (stats->quadratic_count == 0) {
        stats->total_dof = 0;
        for (int i = 0; i < fb->internal_node_count; i++) {
            /* 安全性说明：graph_get_node 仅读取图内容，不修改图结构。
             * 此处 graph 参数来源为 const ConstraintGraph *，
             * 强制转换是安全的，因为被调用函数不会产生副作用。 */
            GeomNode *n = graph_get_node((ConstraintGraph *) graph, fb->internal_node_ids[i]);
            /* 修复：增强 graph_get_node 返回值检查，
             * 对 NULL 返回值记录警告日志，便于排查节点丢失问题 */
            if (!n) {
                lv_LOG_WARNING(
                    "determinism_collect_constraint_stats: "
                    "内部节点 id=%d 在图中不存在，跳过自由度计算",
                    fb->internal_node_ids[i]);
                continue;
            }
            if (n->type == GEOM_POINT || n->type == GEOM_PORT) {
                stats->total_dof += 2;
            }
        }
        stats->free_dof = stats->total_dof - stats->linear_count;
    }

    return all_ids;
}

/**
 * @brief 根据线性系统的自由度分析结果判定确定性
 *
 * @param free_dof 自由度数
 * @return -1 过约束（无解），0 恰好约束（唯一解），1 欠约束（多解）
 */
int determinism_evaluate_linear_dof(int free_dof) {
    if (free_dof < 0)
        return -1; /* 过约束：无解 */
    if (free_dof == 0)
        return 0; /* 恰好约束：唯一解 */
    return 1;     /* 欠约束：多解 */
}

/**
 * @brief 清理求解器结果（GroebnerResult）
 *
 * 两个静态检查函数都需要在调用求解器后清理结果，
 * 提取为公共函数以避免重复的释放逻辑。
 *
 * 释放步骤：
 * 1. 遍历所有解，逐个调用 symbolic_coord_destroy 销毁符号坐标
 * 2. 释放解数组指针
 * 3. 释放 GroebnerResult 结构体本身
 */
void determinism_cleanup_groebner(void *gresult) {
    if (!gresult)
        return;
    GroebnerResult *gr = (GroebnerResult *) gresult;
    /* 逐个销毁符号坐标解 */
    for (int i = 0; i < gr->solution_count; i++) {
        symbolic_coord_destroy(gr->solutions[i]);
    }
    /* 释放解数组和结构体 */
    lv_free((void **) &gr->solutions);
    lv_free((void **) &gr);
}

/**
 * @brief 静态确定性检查（旧版，已弃用，仅供 func_block_verify_determinism 内部使用）
 *
 * @deprecated 本函数为早期实现，已由新版 func_block_determinism_check_static()
 *             取代（依据设计文档 8.2 节，返回 DeterminismStatus，且不做图内
 *             方程提取分析）。两版算法存在实质差异：
 *             - 旧版含"方程提取增强逻辑"（高次方程检测 → OUT_OF_RANGE；
 *               按全图变量数-方程数计算精确自由度），新版不含；
 *             - 旧版二次约束路径在求解器返回 MULTIPLE/NO_SOLUTION 时仍会
 *               继续走 quadratic_count <= input_count 判断（可能回退为
 *               VERIFIED），新版则直接映射为 NON_DETERMINISTIC。
 *             为避免改变 func_block_verify_determinism 的既有确定性结果，
 *             保守保留本实现及其专属映射表，仅标记弃用关系。
 *             新增代码请使用 func_block_determinism_check_static()。
 *
 * @param fb         函数块
 * @param graph      约束图
 * @param step_limit 步数上限
 * @return 确定性检查结果（DeterminismCheckResult 详细结果枚举）
 */
DeterminismCheckResult func_block_check_determinism_static(FuncBlock *fb, ConstraintGraph *graph, int step_limit) {
    if (!fb || !graph)
        return DETERMINISM_CHECK_NO_SOLUTION;

    /* 使用共享核心：收集约束统计 */
    DeterminismStaticStats stats;
    int *all_ids = determinism_collect_constraint_stats(fb, graph, step_limit, &stats);
    if (!all_ids) {
        /* total_count == 0：无内部节点，一定是确定性 */
        if (fb->internal_node_count + fb->input_count + fb->output_count == 0)
            return DETERMINISM_CHECK_UNIQUE;
        return DETERMINISM_CHECK_NO_SOLUTION;
    }
    lv_free((void **) &all_ids);

    /* 超时检查 */
    if (step_limit > 0 && stats.steps >= step_limit) {
        fb->determinism = DETERMINISM_PARTIALLY_VERIFIED;
        return DETERMINISM_CHECK_TIMEOUT;
    }

    /* ======== 方程提取与分析（废弃版本独有的增强逻辑） ======== */
    /*
     * 根据 design_v2.9.md Section 8.2，从约束图中提取实际方程进行精确分析。
     *
     * 分析流程：
     * 1. 创建方程系统并提取约束图中的所有方程
     * 2. 扫描方程的多项式次数：
     *    - 如果存在超过2次的高次方程，标记为部分验证（PARTIALLY_VERIFIED）
     * 3. 统计独立变量数量（两遍扫描策略）：
     *    - 第一遍：找到最大变量 ID
     *    - 第二遍：使用布尔数组去重，统计实际独立变量数
     * 4. 计算自由度 = 独立变量数 - 方程数：
     *    - free_dof < 0：过约束（无解） -> NON_DETERMINISTIC
     *    - free_dof == 0 且 max_degree <= 2：恰好约束（唯一解） -> VERIFIED
     *    - free_dof > 0：欠约束（多解） -> 继续后续分析
     */
    {
        EquationSystem *eq_sys = equation_system_create();
        if (eq_sys) {
            int eq_count = solver_extract_equations_full(graph, eq_sys);
            if (eq_count > 0) {
                int total_eqs = equation_system_count(eq_sys);
                int max_degree = 0;
                int high_degree_count = 0;
                for (int i = 0; i < total_eqs; i++) {
                    const mpz_poly_t *poly = equation_system_get_poly(eq_sys, i);
                    if (poly && poly->degree > max_degree)
                        max_degree = poly->degree;
                    if (poly && poly->degree > 2)
                        high_degree_count++;
                }

                if (high_degree_count > 0) {
                    fb->determinism = DETERMINISM_PARTIALLY_VERIFIED;
                    equation_system_destroy(eq_sys);
                    return DETERMINISM_CHECK_OUT_OF_RANGE;
                }

                /* 两遍扫描策略找最大变量ID */
                int max_var_id = -1;
                for (int i = 0; i < total_eqs; i++) {
                    int vid = equation_system_get_var_id(eq_sys, i);
                    if (vid > max_var_id)
                        max_var_id = vid;
                }
                int var_count = 0;
                bool *seen_vars = NULL;
                if (max_var_id >= 0) {
                    seen_vars = lv_calloc((size_t) (max_var_id + 1), sizeof(bool));
                    if (!seen_vars) {
                        /* 修复：seen_vars 分配失败时，直接销毁 eq_sys 并返回，
                         * seen_vars 为 NULL 无需释放 */
                        equation_system_destroy(eq_sys);
                        return DETERMINISM_CHECK_NO_SOLUTION;
                    }
                }
                for (int i = 0; i < total_eqs; i++) {
                    int vid = equation_system_get_var_id(eq_sys, i);
                    if (vid >= 0 && seen_vars && vid <= max_var_id && !seen_vars[vid]) {
                        seen_vars[vid] = true;
                        var_count++;
                    }
                }
                /* 修复：seen_vars 使用完毕后立即释放，确保后续所有 return 路径
                 * 都不会遗漏释放（防御性编程，避免未来代码变更引入泄漏） */
                lv_free((void **) &seen_vars);

                int actual_free_dof = var_count - total_eqs;
                if (actual_free_dof < 0) {
                    fb->determinism = DETERMINISM_NON_DETERMINISTIC;
                    equation_system_destroy(eq_sys);
                    return DETERMINISM_CHECK_NO_SOLUTION;
                } else if (actual_free_dof == 0 && max_degree <= 2) {
                    fb->determinism = DETERMINISM_VERIFIED;
                    equation_system_destroy(eq_sys);
                    return DETERMINISM_CHECK_UNIQUE;
                }
            }
            equation_system_destroy(eq_sys);
        }
    }
    /* ======== 方程提取结束 ======== */

    /* 使用共享核心：纯线性约束系统的自由度分析 */
    if (stats.quadratic_count == 0) {
        int dof_result = determinism_evaluate_linear_dof(stats.free_dof);
        int dof_idx = dof_result + 1;
        if (dof_idx >= 0 && dof_idx < (int) lv_ARRAY_SIZE(s_dof_result_check_map)) {
            fb->determinism = s_dof_result_check_map[dof_idx].fb_det;
            return s_dof_result_check_map[dof_idx].ret;
        }
        fb->determinism = DETERMINISM_PARTIALLY_VERIFIED;
        return DETERMINISM_CHECK_MULTIPLE;
    }

    /* 含二次约束：尝试消元法 */
    {
        int expected_solution_count = 1;
        for (int i = 0; i < stats.quadratic_count && i < 30; i++) {
            if (expected_solution_count > INT_MAX / 2) {
                expected_solution_count = INT_MAX;
                break;
            }
            expected_solution_count *= 2;
        }

        if (graph && expected_solution_count <= 2) {
            GroebnerResult *gresult = NULL;
            SolverStatus status = solve_algebraic_system(graph, NULL, 0, &gresult);
            if (status == SOLVER_UNIQUE) {
                fb->determinism = DETERMINISM_VERIFIED;
                determinism_cleanup_groebner(gresult);
                return DETERMINISM_CHECK_UNIQUE;
            }
            determinism_cleanup_groebner(gresult);
        }
    }

    if (stats.quadratic_count <= fb->input_count) {
        fb->determinism = DETERMINISM_VERIFIED;
        return DETERMINISM_CHECK_UNIQUE;
    }

    fb->determinism = DETERMINISM_PARTIALLY_VERIFIED;
    return DETERMINISM_CHECK_MULTIPLE;
}

/* ============== 确定性检查（设计文档 8.2 节增强版） ============== */

/**
 * @brief 启发式估算二次约束（INTERSECTION）对解数量的影响
 *
 * 遍历涉及内部节点的约束，每遇到一个 INTERSECTION 约束将
 * expected_solutions 翻倍（上限 INT_MAX），并记录是否存在二次约束。
 *
 * @param graph                  约束图
 * @param all_ids                内部相关节点 ID 列表
 * @param all_count              all_ids 长度
 * @param out_expected_solutions 输出：估算的解数量（无二次约束时为 1）
 * @param out_has_quadratic     输出：是否存在二次约束
 */
static void determinism_estimate_intersection_solutions(const ConstraintGraph *graph, const int *all_ids, int all_count,
                                                        int *out_expected_solutions, bool *out_has_quadratic) {
    int expected_solutions = 1;
    bool has_quadratic = false;
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        bool involves_internal = false;
        for (int j = 0; j < c->participant_count; j++) {
            if (is_id_in_array(c->participants[j], all_ids, all_count)) {
                involves_internal = true;
                break;
            }
        }
        if (!involves_internal)
            continue;
        if (c->type == INTERSECTION) {
            has_quadratic = true;
            /* 两条线段相交：最多1个交点，但可能有0个（不相交） */
            /* 每个二次约束可能使解的数量翻倍 */
            if (expected_solutions <= INT_MAX / 2) {
                expected_solutions *= 2;
            }
        }
    }
    *out_expected_solutions = expected_solutions;
    *out_has_quadratic = has_quadratic;
}

/**
 * @brief 静态确定性检查（增强版）
 *
 * 根据设计文档 8.2 节，使用默认步数上限执行静态分析：
 * 1. 收集内部节点和端口的 ID 列表
 * 2. 统计涉及内部节点的约束类型（线性/二次/连接）
 * 3. 对纯线性系统进行自由度分析
 * 4. 含二次约束时调用 Groebner 基求解器尝试消元
 *
 * @param fb    函数块
 * @param graph 约束图
 * @return 确定性状态（VERIFIED / NON_DETERMINISTIC / PARTIALLY_VERIFIED）
 */
DeterminismStatus func_block_determinism_check_static(FuncBlock *fb, const ConstraintGraph *graph) {
    if (!fb || !graph) {
        /* 流式事件：静态确定性检查完成 */
        if (func_block_stream_ctx) {
            stream_emit_simple(func_block_stream_ctx, STREAM_EVENT_FUNC_BLOCK_DETERMINISM_CHECK,
                               "静态确定性检查完成: 非确定性", 1);
        }
        return DETERMINISM_NON_DETERMINISTIC;
    }

    DeterminismStatus static_result = DETERMINISM_NON_DETERMINISTIC;

    /* 流式事件：静态确定性检查开始 */
    if (func_block_stream_ctx) {
        stream_emit_simple(func_block_stream_ctx, STREAM_EVENT_FUNC_BLOCK_DETERMINISM_CHECK, "静态确定性检查开始", 0);
    }

    /* 使用共享核心：收集约束统计 */
    DeterminismStaticStats stats;
    int *all_ids = determinism_collect_constraint_stats(fb, graph, lv_DEFAULT_DETERMINISM_STEP_LIMIT, &stats);
    if (!all_ids) {
        /* total_count == 0：无内部节点，直接标记为已验证 */
        if (fb->internal_node_count + fb->input_count + fb->output_count == 0) {
            fb->determinism = DETERMINISM_VERIFIED;
            static_result = DETERMINISM_VERIFIED;
        } else {
            static_result = DETERMINISM_NON_DETERMINISTIC;
        }
        goto static_done;
    }
    lv_free((void **) &all_ids);

    /* 超时检查：分析未完成但未发现冲突 */
    if (stats.steps >= lv_DEFAULT_DETERMINISM_STEP_LIMIT) {
        fb->determinism = DETERMINISM_PARTIALLY_VERIFIED;
        static_result = DETERMINISM_PARTIALLY_VERIFIED;
        goto static_done;
    }

    /* 使用共享核心：纯线性约束系统的自由度分析 */
    if (stats.quadratic_count == 0) {
        int dof_result = determinism_evaluate_linear_dof(stats.free_dof);
        int dof_idx = dof_result + 1;
        if (dof_idx >= 0 && dof_idx < (int) lv_ARRAY_SIZE(s_dof_result_status_map)) {
            fb->determinism = s_dof_result_status_map[dof_idx].fb_det;
            static_result = s_dof_result_status_map[dof_idx].ret;
            goto static_done;
        }
        fb->determinism = DETERMINISM_PARTIALLY_VERIFIED;
        static_result = DETERMINISM_PARTIALLY_VERIFIED;
        goto static_done;
    }

    /* 含二次约束：使用求解器尝试消元 */
    {
        GroebnerResult *gresult = NULL;
        /* 安全性说明：solve_algebraic_system 仅从图中提取方程，不修改图结构。
         * 此处 graph 参数来源为 const ConstraintGraph *，
         * 强制转换是安全的，因为被调用函数不会产生副作用。 */
        SolverStatus status = solve_algebraic_system((ConstraintGraph *) graph, NULL, 0, &gresult);

        {
            int status_idx = (int) status - 1;
            if (status_idx >= 0 && status_idx < (int) lv_SOLVER_STATUS_STATIC_MAP_COUNT) {
                fb->determinism = s_solver_status_static_map[status_idx].fb_det;
                static_result = s_solver_status_static_map[status_idx].static_res;
                determinism_cleanup_groebner(gresult);
                goto static_done;
            }
            determinism_cleanup_groebner(gresult);
        }
    }

    /* 求解器未能给出明确结论：检查二次约束数量是否可控 */
    if (stats.quadratic_count <= fb->input_count) {
        fb->determinism = DETERMINISM_VERIFIED;
        static_result = DETERMINISM_VERIFIED;
        goto static_done;
    }

    /* 无法静态确定，但未发现冲突 */
    fb->determinism = DETERMINISM_PARTIALLY_VERIFIED;
    static_result = DETERMINISM_PARTIALLY_VERIFIED;

static_done:
    /* 流式事件：静态确定性检查完成 */
    if (func_block_stream_ctx) {
        const char *desc = (static_result == DETERMINISM_VERIFIED)             ? "静态确定性检查完成: 已验证"
                           : (static_result == DETERMINISM_PARTIALLY_VERIFIED) ? "静态确定性检查完成: 部分验证"
                                                                               : "静态确定性检查完成: 非确定性";
        stream_emit_simple(func_block_stream_ctx, STREAM_EVENT_FUNC_BLOCK_DETERMINISM_CHECK, desc, 1);
    }
    return static_result;
}

/**
 * @brief 动态确定性检查（增强版）
 *
 * 在给定实参值下，将输入绑定到端口节点后运行求解器，
 * 根据求解结果判断确定性。
 *
 * 【线程安全限制】此函数使用 save/restore 模式临时修改图中节点的坐标，
 * 调用者必须确保在调用期间对图中涉及的输入端口节点具有独占访问权。
 *
 * @param fb           函数块
 * @param graph        约束图
 * @param input_values 输入值数组
 * @param n_inputs     输入值数量
 * @return 确定性状态（VERIFIED / NON_DETERMINISTIC / PARTIALLY_VERIFIED）
 */
DeterminismStatus func_block_determinism_check_dynamic(FuncBlock *fb, ConstraintGraph *graph,
                                                       const SymbolicCoord **input_values, int n_inputs) {
    if (!fb || !graph) {
        /* 流式事件：动态确定性检查完成 */
        if (func_block_stream_ctx) {
            stream_emit_simple(func_block_stream_ctx, STREAM_EVENT_FUNC_BLOCK_DETERMINISM_CHECK,
                               "动态确定性检查完成: 非确定性", 1);
        }
        return DETERMINISM_NON_DETERMINISTIC;
    }

    DeterminismStatus dynamic_result = DETERMINISM_NON_DETERMINISTIC;

    /* 流式事件：动态确定性检查开始 */
    if (func_block_stream_ctx) {
        stream_emit_simple(func_block_stream_ctx, STREAM_EVENT_FUNC_BLOCK_DETERMINISM_CHECK, "动态确定性检查开始", 0);
    }

    /* 收集所有内部相关节点ID（使用共享辅助函数） */
    int *all_ids = NULL;
    int all_count = 0;
    if (!collect_all_block_ids(fb, &all_ids, &all_count)) {
        fb->determinism = DETERMINISM_VERIFIED;
        dynamic_result = DETERMINISM_VERIFIED;
        goto dynamic_done;
    }

    /* 将实参值绑定到输入端口节点（临时设置坐标用于求解器）
     *
     * 【线程安全限制】此函数直接修改图中节点的 symbolic_coords 字段，
     * 然后在求解完成后恢复原始值。这种 save/restore 模式在多线程环境下
     * 是不安全的：如果另一个线程同时读取或修改同一节点的坐标，将导致
     * 数据竞争（data race）。调用者必须确保在调用此函数期间，对 graph
     * 中涉及的所有输入端口节点具有独占访问权。
     *
     * 绑定流程（save/restore 模式）：
     * 1. 分配保存数组 saved_coords[] 和 saved_coord_counts[]
     * 2. 对每个输入端口：
     *    a. 保存原始 symbolic_coords 指针和 coord_count
     *    b. 将 input_values[i] 深拷贝后绑定到端口节点
     * 3. 求解完成后，恢复所有端口节点的原始坐标并释放临时副本
     *
     * 恢复逻辑需处理以下边界情况：
     *   1. port_node 为 NULL（节点已被删除）
     *   2. malloc 分配失败（saved_coords/saved_coord_counts 为 NULL）
     *   3. symbolic_coord_copy 失败（port_node->symbolic_coords 保持原值）
     *   4. n_inputs > fb->input_count（多余输入被忽略）
     */
    SymbolicCoord ***saved_coords = NULL;
    int *saved_coord_counts = NULL;
    if (n_inputs > 0 && input_values != NULL) {
        saved_coords = lv_malloc((size_t) n_inputs * sizeof(SymbolicCoord **));
        saved_coord_counts = lv_malloc((size_t) n_inputs * sizeof(int));
        if (saved_coords && saved_coord_counts) {
            /* 初始化 saved_coords 数组，确保恢复时能正确判断哪些槽位有效 */
            for (int i = 0; i < n_inputs; i++) {
                saved_coords[i] = NULL;
                saved_coord_counts[i] = 0;
            }
            for (int i = 0; i < n_inputs && i < fb->input_count; i++) {
                GeomNode *port_node = graph_get_node(graph, fb->input_port_ids[i]);
                if (port_node) {
                    /* 保存原始坐标 */
                    saved_coords[i] = port_node->symbolic_coords;
                    saved_coord_counts[i] = port_node->coord_count;
                    /* 绑定实参坐标（创建副本以避免修改原始值） */
                    if (input_values[i]) {
                        port_node->symbolic_coords = lv_malloc(sizeof(SymbolicCoord *));
                        if (port_node->symbolic_coords) {
                            port_node->symbolic_coords[0] = symbolic_coord_copy(input_values[i]);
                            if (port_node->symbolic_coords[0]) {
                                port_node->coord_count = 1;
                            } else {
                                /* symbolic_coord_copy 失败：恢复原始坐标 */
                                lv_free((void **) &port_node->symbolic_coords);
                                port_node->symbolic_coords = saved_coords[i];
                                port_node->coord_count = saved_coord_counts[i];
                                saved_coords[i] = NULL; /* 标记为已恢复，避免 double-free */
                            }
                        } else {
                            /* malloc 失败：恢复原始坐标 */
                            port_node->symbolic_coords = saved_coords[i];
                            port_node->coord_count = saved_coord_counts[i];
                            saved_coords[i] = NULL; /* 标记为已恢复，避免 double-free */
                        }
                    }
                }
            }
        } else {
            /* saved_coords 或 saved_coord_counts 分配失败：无法安全地保存/恢复坐标，
             * 跳过参数绑定，直接使用当前图中的坐标值进行求解 */
            lv_free((void **) &saved_coords);
            lv_free((void **) &saved_coord_counts);
            saved_coords = NULL;
            saved_coord_counts = NULL;
        }
    }

    /* 运行求解器 */
    GroebnerResult *gresult = NULL;
    SolverStatus status = solve_algebraic_system(graph, NULL, 0, &gresult);

    /* 恢复输入端口节点的原始坐标
     * 注意：saved_coords[i] 为 NULL 表示该槽位从未成功保存或已提前恢复，
     * 此时跳过恢复以避免 double-free */
    if (saved_coords && saved_coord_counts) {
        for (int i = 0; i < n_inputs && i < fb->input_count; i++) {
            if (!saved_coords[i])
                continue; /* 跳过未保存或已提前恢复的槽位 */
            GeomNode *port_node = graph_get_node(graph, fb->input_port_ids[i]);
            if (port_node) {
                /* 释放临时绑定的坐标 */
                if (port_node->symbolic_coords && port_node->coord_count == 1) {
                    symbolic_coord_destroy(port_node->symbolic_coords[0]);
                    lv_free((void **) &port_node->symbolic_coords);
                }
                /* 恢复原始坐标 */
                port_node->symbolic_coords = saved_coords[i];
                port_node->coord_count = saved_coord_counts[i];
            }
        }
        lv_free((void **) &saved_coords);
        lv_free((void **) &saved_coord_counts);
    }

    /* 根据求解器结果判断确定性 */
    {
        int status_idx = (int) status - 1;
        if (status_idx >= 0 && status_idx < (int) lv_SOLVER_STATUS_DYNAMIC_MAP_COUNT) {
            const SolverStatusDynamicEntry *entry = &s_solver_status_dynamic_map[status_idx];
            if (entry->is_handled) {
                fb->determinism = entry->fb_det;
                determinism_cleanup_groebner(gresult);
                dynamic_result = entry->dynamic_res;
                goto dynamic_done;
            }
            /* 未处理状态（SOLVER_OUT_OF_SCOPE, SOLVER_TIMEOUT）: 继续启发式分析 */
            determinism_cleanup_groebner(gresult);
        } else {
            determinism_cleanup_groebner(gresult);
        }
    }
    /* 回退到启发式分析：估算二次约束（INTERSECTION）对解数量的影响 */
    {
        int expected_solutions = 0;
        bool has_quadratic = false;
        determinism_estimate_intersection_solutions(graph, all_ids, all_count, &expected_solutions, &has_quadratic);
        if (expected_solutions > 1 && has_quadratic) {
            fb->determinism = DETERMINISM_NON_DETERMINISTIC;
            dynamic_result = DETERMINISM_NON_DETERMINISTIC;
            goto dynamic_done;
        }
    }
    fb->determinism = DETERMINISM_PARTIALLY_VERIFIED;
    dynamic_result = DETERMINISM_PARTIALLY_VERIFIED;
    goto dynamic_done;

dynamic_done:
    /* 修复：统一在此处释放 all_ids，确保所有路径都不会遗漏释放 */
    lv_free((void **) &all_ids);
    /* 流式事件：动态确定性检查完成 */
    if (func_block_stream_ctx) {
        const char *desc = (dynamic_result == DETERMINISM_VERIFIED)             ? "动态确定性检查完成: 已验证"
                           : (dynamic_result == DETERMINISM_PARTIALLY_VERIFIED) ? "动态确定性检查完成: 部分验证"
                                                                                : "动态确定性检查完成: 非确定性";
        stream_emit_simple(func_block_stream_ctx, STREAM_EVENT_FUNC_BLOCK_DETERMINISM_CHECK, desc, 1);
    }
    return dynamic_result;
}

/* ------------------------------------------------------------------ */
/*  func_block_verify_determinism                                     */
/* ------------------------------------------------------------------ */

/**
 * @brief 完整的确定性验证流水线。
 *
 * 根据 design_v2.9.md 第8.2节：
 * 1. 先运行静态分析。
 * 2. 如果结果为 VERIFIED，立即返回。
 * 3. 如果结果为 PARTIALLY_VERIFIED，调用方应运行动态检查。
 *
 * @param fb         函数块
 * @param graph      约束图
 * @param step_limit 静态分析的最大步数
 * @return 最终确定性状态（VERIFIED / NON_DETERMINISTIC / PARTIALLY_VERIFIED / UNVERIFIED）
 */
DeterminismState func_block_verify_determinism(FuncBlock *fb, ConstraintGraph *graph, int step_limit) {
    if (!fb || !graph)
        return DETERMINISM_UNVERIFIED;

    /* 流式事件：确定性验证入口 */
    if (func_block_stream_ctx) {
        stream_emit_simple(func_block_stream_ctx, STREAM_EVENT_FUNC_BLOCK_DETERMINISM_CHECK, "函数块确定性验证开始",
                           -1);
    }

    /* 第1步：静态分析
     *
     * 【有意使用旧版】此处保留调用旧版 func_block_check_determinism_static()：
     * 该函数含方程提取增强逻辑（高次方程检测、按全图变量数-方程数计算精确
     * 自由度），且二次约束路径在求解器返回 MULTIPLE/NO_SOLUTION 时仍会走
     * quadratic_count <= input_count 回退判断，与新版
     * func_block_determinism_check_static() 结果存在实质差异。
     * 为不改变本流水线既有确定性结果，保守保留旧版调用；
     * 若需对齐新版语义，请评估结果变化后再切换。 */
    DeterminismCheckResult static_result = func_block_check_determinism_static(fb, graph, step_limit);

    if ((unsigned) static_result < lv_CHECK_RESULT_TO_STATE_COUNT) {
        DeterminismState state = s_check_result_to_state[static_result];
        if (state == DETERMINISM_VERIFIED && func_block_stream_ctx) {
            stream_emit_simple(func_block_stream_ctx, STREAM_EVENT_FUNC_BLOCK_DETERMINISM_CHECK, "确定性验证通过", -1);
        } else if (state == DETERMINISM_NON_DETERMINISTIC && func_block_stream_ctx) {
            stream_emit_simple(func_block_stream_ctx, STREAM_EVENT_WARNING, "检测到非确定性", -1);
        }
        return state;
    }
    return DETERMINISM_UNVERIFIED;
}

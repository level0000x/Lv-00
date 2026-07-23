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

#include "func_block.h"
#include "func_block_internal.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "stream.h"
#include "stream_context_util.h"

/* ==================== 命名常量 ==================== */

/** 二次约束解数量的最大迭代次数 */
#define MAX_QUADRATIC_SOLUTION_ITERATIONS 30

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

        switch (c->type) {
            case INCIDENCE:
            case BETWEENNESS:
            case CONTAINMENT:
                stats->linear_count++;
                break;
            case INTERSECTION:
                stats->quadratic_count++;
                break;
            case CONNECTION:
                stats->connection_count++;
                break;
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
 * @brief 静态确定性检查
 *
 * @param fb         函数块
 * @param graph      约束图
 * @param step_limit 步数上限
 * @return 确定性检查结果
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
        switch (dof_result) {
            case -1:
                fb->determinism = DETERMINISM_NON_DETERMINISTIC;
                return DETERMINISM_CHECK_NO_SOLUTION;
            case 0:
                fb->determinism = DETERMINISM_VERIFIED;
                return DETERMINISM_CHECK_UNIQUE;
            default:
                fb->determinism = DETERMINISM_PARTIALLY_VERIFIED;
                return DETERMINISM_CHECK_MULTIPLE;
        }
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

/**
 * @brief 动态确定性检查（废弃版本）
 *
 * @deprecated 本函数自 design_v2.9.md (Section 8.2) 生效后废弃。
 *             请使用 func_block_determinism_check_dynamic() 替代。
 *
 * 废弃原因：
 * 1. 返回值类型（DeterminismCheckResult）与增强版返回的 DeterminismStatus
 *    不兼容，导致调用方需要额外的类型转换。
 * 2. 缺少线程安全的 save/restore 坐标绑定机制。增强版使用
 *    saved_coords/saved_coord_counts 数组在求解前后保存/恢复节点的
 *    symbolic_coords，而本版本直接修改图中节点坐标，在多线程环境下
 *    存在数据竞争风险。
 * 3. 增强版增加了流式事件通知（stream_emit_simple）支持，便于上层框架
 *    监控确定性检查的进度和结果。
 * 4. 增强版有更完善的错误处理路径（统一在 dynamic_done 标签处释放
 *    all_ids），避免了本版本中存在的多 return 路径资源泄漏风险。
 *
 * 替代方案：
 *   func_block_determinism_check_dynamic(fb, graph, input_values, n_inputs)
 *
 * @param fb              函数块
 * @param graph           约束图
 * @param arg_values      实参符号坐标数组
 * @param arg_count       实参数量
 * @param out_solutions   输出候选解数组（调用者负责释放）
 * @param out_solution_count 输出候选解数量
 * @return 确定性检查结果
 */
#if defined(__GNUC__) || defined(__clang__)
__attribute__((deprecated("use func_block_determinism_check_dynamic instead")))
#elif defined(_MSC_VER)
__declspec(deprecated("use func_block_determinism_check_dynamic instead"))
#endif
DeterminismCheckResult func_block_check_determinism_dynamic(FuncBlock *fb, ConstraintGraph *graph,
                                                            SymbolicCoord **arg_values, int arg_count,
                                                            GeomNode ***out_solutions, int *out_solution_count) {
    if (!fb || !graph || !out_solutions || !out_solution_count) {
        return DETERMINISM_CHECK_NO_SOLUTION;
    }

    *out_solutions = NULL;
    *out_solution_count = 0;

    /* 修复：使用统一的 cleanup 标签管理 all_ids 释放，
     * 确保所有 return 路径都经过资源释放点。
     * 原代码在中间某处直接 lv_free 后仍有多个 return 路径，
     * 若未来代码添加新的返回路径，可能遗漏资源释放。 */
    DeterminismCheckResult retval = DETERMINISM_CHECK_UNIQUE;

    /* 收集所有内部相关节点ID（使用共享辅助函数） */
    int *all_ids = NULL;
    int all_count = 0;
    if (!collect_all_block_ids(fb, &all_ids, &all_count)) {
        fb->determinism = DETERMINISM_VERIFIED;
        retval = DETERMINISM_CHECK_UNIQUE;
        goto dynamic_cleanup;
    }

    /* 动态分析：在给定实参值下估算解的数量 */
    int expected_solution_count = 1;
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
            if (expected_solution_count <= INT_MAX / 2) {
                expected_solution_count *= 2;
            }
        }
    }

    /*
     * 说明：all_ids 在约束统计循环中已使用完毕。
     * 此处不再使用 all_ids，后续求解器和启发式代码也无需 all_ids。
     * all_ids 将由统一的 dynamic_cleanup 标签释放。
     */

    /* 根据 design_v2.9.md Section 8.2：实际求解代数系统，
     * 而不仅仅是从约束类型进行估算。 */
    {
        GroebnerResult *gresult = NULL;
        SolverStatus status = solve_algebraic_system(graph, NULL, 0, &gresult);

        if (status == SOLVER_UNIQUE) {
            fb->determinism = DETERMINISM_VERIFIED;
            determinism_cleanup_groebner(gresult);
            retval = DETERMINISM_CHECK_UNIQUE;
            goto dynamic_cleanup;
        } else if (status == SOLVER_NO_SOLUTION) {
            fb->determinism = DETERMINISM_NON_DETERMINISTIC;
            determinism_cleanup_groebner(gresult);
            retval = DETERMINISM_CHECK_NO_SOLUTION;
            goto dynamic_cleanup;
        } else if (status == SOLVER_MULTIPLE) {
            /* 如果函数块有选择器，可能会过滤为唯一解 */
            if (fb->selector) {
                fb->determinism = DETERMINISM_PARTIALLY_VERIFIED;
                determinism_cleanup_groebner(gresult);
                retval = DETERMINISM_CHECK_MULTIPLE;
                goto dynamic_cleanup;
            }
            fb->determinism = DETERMINISM_NON_DETERMINISTIC;
            determinism_cleanup_groebner(gresult);
            retval = DETERMINISM_CHECK_MULTIPLE;
            goto dynamic_cleanup;
        } else if (status == SOLVER_OUT_OF_SCOPE) {
            fb->determinism = DETERMINISM_PARTIALLY_VERIFIED;
            determinism_cleanup_groebner(gresult);
            /* 继续执行下面的启发式检查 */
        } else {
            determinism_cleanup_groebner(gresult);
            /* 继续执行下面的启发式检查 */
        }
    }

    /* 启发式检查：利用之前计算的 expected_solution_count 和 has_quadratic */
    if (expected_solution_count == 0) {
        fb->determinism = DETERMINISM_NON_DETERMINISTIC;
        retval = DETERMINISM_CHECK_NO_SOLUTION;
        goto dynamic_cleanup;
    }

    if (expected_solution_count > 1 && has_quadratic) {
        fb->determinism = DETERMINISM_NON_DETERMINISTIC;
        *out_solution_count = expected_solution_count;

        /* 如果有选择器，报告需要选择器 */
        if (!fb->selector) {
            retval = DETERMINISM_CHECK_MULTIPLE;
            goto dynamic_cleanup;
        }
        /* 有选择器时，仍然返回 MULTIPLE，调用者负责使用选择器 */
        retval = DETERMINISM_CHECK_MULTIPLE;
        goto dynamic_cleanup;
    }

    fb->determinism = DETERMINISM_VERIFIED;
    retval = DETERMINISM_CHECK_UNIQUE;

dynamic_cleanup:
    /* 修复：统一在此释放 all_ids，确保所有路径都不会遗漏释放。
     * 即使 all_ids 为 NULL，lv_free 也能安全处理。 */
    lv_free((void **) &all_ids);
    return retval;
}

/* ============== 确定性检查（设计文档 8.2 节增强版） ============== */

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
        switch (dof_result) {
            case -1:
                fb->determinism = DETERMINISM_NON_DETERMINISTIC;
                static_result = DETERMINISM_NON_DETERMINISTIC;
                goto static_done;
            case 0:
                fb->determinism = DETERMINISM_VERIFIED;
                static_result = DETERMINISM_VERIFIED;
                goto static_done;
            default:
                fb->determinism = DETERMINISM_PARTIALLY_VERIFIED;
                static_result = DETERMINISM_PARTIALLY_VERIFIED;
                goto static_done;
        }
    }

    /* 含二次约束：使用求解器尝试消元 */
    {
        GroebnerResult *gresult = NULL;
        /* 安全性说明：solve_algebraic_system 仅从图中提取方程，不修改图结构。
         * 此处 graph 参数来源为 const ConstraintGraph *，
         * 强制转换是安全的，因为被调用函数不会产生副作用。 */
        SolverStatus status = solve_algebraic_system((ConstraintGraph *) graph, NULL, 0, &gresult);

        switch (status) {
            case SOLVER_UNIQUE:
                fb->determinism = DETERMINISM_VERIFIED;
                determinism_cleanup_groebner(gresult);
                static_result = DETERMINISM_VERIFIED;
                goto static_done;
            case SOLVER_NO_SOLUTION:
            case SOLVER_OVERCONSTRAINED:
                fb->determinism = DETERMINISM_NON_DETERMINISTIC;
                determinism_cleanup_groebner(gresult);
                static_result = DETERMINISM_NON_DETERMINISTIC;
                goto static_done;
            case SOLVER_MULTIPLE:
                fb->determinism = DETERMINISM_NON_DETERMINISTIC;
                determinism_cleanup_groebner(gresult);
                static_result = DETERMINISM_NON_DETERMINISTIC;
                goto static_done;
            default:
                determinism_cleanup_groebner(gresult);
                break;
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
    switch (status) {
        case SOLVER_UNIQUE:
            /* 唯一解：确认确定性 */
            fb->determinism = DETERMINISM_VERIFIED;
            determinism_cleanup_groebner(gresult);
            /* 修复：all_ids 统一在 dynamic_done 处释放，避免遗漏 */
            dynamic_result = DETERMINISM_VERIFIED;
            goto dynamic_done;

        case SOLVER_MULTIPLE:
            /* 多解：降级为非确定性 */
            fb->determinism = DETERMINISM_NON_DETERMINISTIC;
            determinism_cleanup_groebner(gresult);
            /* 修复：all_ids 统一在 dynamic_done 处释放，避免遗漏 */
            dynamic_result = DETERMINISM_NON_DETERMINISTIC;
            goto dynamic_done;

        case SOLVER_NO_SOLUTION:
            /* 无解：输入不满足前置条件 */
            fb->determinism = DETERMINISM_NON_DETERMINISTIC;
            determinism_cleanup_groebner(gresult);
            /* 修复：all_ids 统一在 dynamic_done 处释放，避免遗漏 */
            dynamic_result = DETERMINISM_NON_DETERMINISTIC;
            goto dynamic_done;

        case SOLVER_OVERCONSTRAINED:
            /* 过约束：存在冲突 */
            fb->determinism = DETERMINISM_NON_DETERMINISTIC;
            determinism_cleanup_groebner(gresult);
            /* 修复：all_ids 统一在 dynamic_done 处释放，避免遗漏 */
            dynamic_result = DETERMINISM_NON_DETERMINISTIC;
            goto dynamic_done;

        case SOLVER_TIMEOUT:
        case SOLVER_OUT_OF_SCOPE:
        default:
            /* 求解器超时或超出范围：无法确认，但无冲突 */
            determinism_cleanup_groebner(gresult);
            /* 回退到启发式分析 */
            {
                int expected_solutions = 1;
                bool has_quadratic = false;
                for (int i = 0; i < graph->constraint_count; i++) {
                    Constraint *c = graph->constraints[i];
                    bool involves = false;
                    for (int j = 0; j < c->participant_count; j++) {
                        if (is_id_in_array(c->participants[j], all_ids, all_count)) {
                            involves = true;
                            break;
                        }
                    }
                    if (!involves)
                        continue;
                    if (c->type == INTERSECTION) {
                        has_quadratic = true;
                        if (expected_solutions <= INT_MAX / 2) {
                            expected_solutions *= 2;
                        }
                    }
                }
                /* 修复：all_ids 统一在 dynamic_done 处释放，此处不再释放 */
                if (expected_solutions > 1 && has_quadratic) {
                    fb->determinism = DETERMINISM_NON_DETERMINISTIC;
                    dynamic_result = DETERMINISM_NON_DETERMINISTIC;
                    goto dynamic_done;
                }
            }
            fb->determinism = DETERMINISM_PARTIALLY_VERIFIED;
            dynamic_result = DETERMINISM_PARTIALLY_VERIFIED;
            goto dynamic_done;
    }

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

    /* 第1步：静态分析 */
    DeterminismCheckResult static_result = func_block_check_determinism_static(fb, graph, step_limit);

    switch (static_result) {
        case DETERMINISM_CHECK_UNIQUE:
            if (func_block_stream_ctx) {
                stream_emit_simple(func_block_stream_ctx, STREAM_EVENT_FUNC_BLOCK_DETERMINISM_CHECK, "确定性验证通过",
                                   -1);
            }
            return DETERMINISM_VERIFIED;
        case DETERMINISM_CHECK_NO_SOLUTION:
            if (func_block_stream_ctx) {
                stream_emit_simple(func_block_stream_ctx, STREAM_EVENT_WARNING, "检测到非确定性", -1);
            }
            return DETERMINISM_NON_DETERMINISTIC;
        case DETERMINISM_CHECK_TIMEOUT:
        case DETERMINISM_CHECK_MULTIPLE:
        case DETERMINISM_CHECK_OUT_OF_RANGE:
            /* 部分验证：需要动态检查 */
            return DETERMINISM_PARTIALLY_VERIFIED;
        default:
            return DETERMINISM_UNVERIFIED;
    }
}

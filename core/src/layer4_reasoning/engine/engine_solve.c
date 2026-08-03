/**
 * @file engine_solve.c
 * @brief 引擎求解流水线（从 engine.c 拆分）
 *
 * @details 实现完整求解流水线与重写-求解协作工作流。
 *          协调执行：重写（有限步数）-> 求解器（处理剩余约束）
 *          -> 冲突检查 -> 自由度更新。
 *          支持位电路熔断检测与信任颜色传播。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv/engine.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "lv/bit_burning.h"
#include "lv/constraint_graph.h"
#include "lv/lambda_to_graph.h"
#include "lv/lv.h"
#include "lv/lv_config.h"
#include "lv/normalization.h"
#include "lv/rewrite.h"
#include "lv/solver.h"
#include "lv/stream.h"
#include "lv/trust_color.h"

#include "engine_internal.h"

/** @brief 重写-求解协作最大迭代次数（已迁移至 lvConfig 运行时配置） */

/**
 * @brief 检查约束图中的冲突
 *
 * @param engine   引擎实例
 * @param context  上下文描述（用于错误消息）
 * @return 0 无冲突，-1 检测到冲突（并设置 last_status）
 */
static int check_and_report_conflicts(lvEngine *engine, const char *context) {
    int conflict_count = 0;
    int **conflicts = graph_detect_conflicts(engine->main_graph, &conflict_count, NULL);
    if (conflicts) {
        for (int i = 0; i < conflict_count; i++) {
            lv_free((void **) &conflicts[i]);
        }
        lv_free((void **) &conflicts);
    }
    if (conflict_count > 0) {
        engine_set_error(engine, ENGINE_STATUS_CONSTRAINT_CONFLICT, "%s: 检测到 %d 个冲突", context, conflict_count);
        lv_RETURN_ERROR(lv_ERROR_CONSTRAINT_CONFLICT, "check_and_report_conflicts: %d conflicts detected", conflict_count);
    }
    return 0;
}

/**
 * @brief 在约束图上运行求解器
 *
 * @param engine   引擎实例
 * @param context  上下文描述（用于错误消息）
 * @return ENGINE_SOLVE_OK 成功，ENGINE_SOLVE_CONFLICT 冲突，ENGINE_SOLVE_TIMEOUT 超时
 */
static EngineSolveResult run_solver_on_graph(lvEngine *engine, const char *context) {
    int *dirty_ids = NULL;
    int free_count = count_degrees_of_freedom(engine->main_graph, &dirty_ids);
    if (free_count < 0) {
        /* count_degrees_of_freedom 返回 -1 表示内部错误（如内存分配失败） */
        engine_set_error(engine, ENGINE_STATUS_ERROR_INTERNAL, "%s: 计算自由度失败", context);
        lv_free((void **) &dirty_ids);
        return ENGINE_SOLVE_CONFLICT;
    }
    if (free_count > 0 && dirty_ids) {
        GroebnerResult *result = NULL;
        SolverStatus sstatus = solve_algebraic_system(engine->main_graph, dirty_ids, free_count, &result);
        if (result)
            groebner_result_destroy(result);
        lv_free((void **) &dirty_ids);

        if (sstatus == SOLVER_STATUS_NO_SOLUTION || sstatus == SOLVER_STATUS_OVERCONSTRAINED) {
            engine->last_status = ENGINE_STATUS_CONSTRAINT_CONFLICT;
            snprintf(engine->last_error, sizeof(engine->last_error), "%s: 求解器检测到冲突", context);
            return ENGINE_SOLVE_CONFLICT;
        }
        if (sstatus == SOLVER_STATUS_TIMEOUT) {
            engine->last_status = ENGINE_STATUS_CONSTRAINT_CONFLICT;
            snprintf(engine->last_error, sizeof(engine->last_error), "%s: 求解器超时", context);
            return ENGINE_SOLVE_TIMEOUT;
        }
        return ENGINE_SOLVE_OK;
    }
    lv_free((void **) &dirty_ids);
    return ENGINE_SOLVE_OK;
}

/**
 * @brief 完整求解流水线
 *
 * 协调执行：重写（有限步数）-> 求解器（处理剩余约束）
 *         -> 冲突检查 -> 自由度更新
 *
 * @param engine 引擎实例
 * @return ENGINE_SOLVE_OK 成功，ENGINE_SOLVE_CONFLICT 冲突，
 *         ENGINE_SOLVE_TIMEOUT 超时，ENGINE_SOLVE_ERROR 错误
 */
EngineSolveResult engine_solve(lvEngine *engine) {
    /* P2修复: 迁移到 engine 实例变量，移除全局 TLS 状态依赖 */
    if (!engine)
        return ENGINE_SOLVE_ERROR;
    if (!engine->main_graph) {
        engine->last_status = ENGINE_STATUS_INVALID_STATE;
        snprintf(engine->last_error, sizeof(engine->last_error), "求解失败: 约束图为空");
        return ENGINE_SOLVE_ERROR;
    }

    /* 流式事件: 引擎开始 */
    engine_emit_stream_event(engine, STREAM_EVENT_ENGINE_START, "求解流程启动", 0, -1, -1);

    /* 步骤0：设置位数熔断检查点 */
    {
        BitBurningState *bb_state = bit_burning_get_global_state();
        bit_burning_set_checkpoint(engine->main_graph, bb_state);
    }

    /* 步骤0：归一化约束图
     * 根据 design_v2.9.md Section 18.1：求解前进行归一化可消除冗余节点并规范化图结构。
     * 归一化失败时记录警告，但不中断求解流程（归一化是优化步骤，
     * 即使失败，求解器仍可尝试求解原始图）。 */
    {
        engine_emit_stream_event(engine, STREAM_EVENT_NORMALIZE_START, "开始图规范化", 0, -1, -1);

        NormalizationResult *norm = graph_normalize(engine->main_graph, false);
        if (!norm) {
            /* 归一化失败（可能内存不足或图状态异常），记录警告继续执行。
             * 求解器将在未归一化的图上运行，结果可能不如预期。 */
            snprintf(engine->last_error, sizeof(engine->last_error),
                     "engine_solve: graph_normalize 返回 NULL，将继续在未规范化的图上求解");
            engine_emit_stream_event(engine, STREAM_EVENT_WARNING, "图规范化失败，将继续在未规范化的图上求解", 0, -1,
                                     -1);
        } else {
            /* 流式事件: 归一化完成（含合并节点数） */
            {
                StreamEvent ev;
                memset(&ev, 0, sizeof(ev));
                ev.type = STREAM_EVENT_NORMALIZE_DONE;
                ev.timestamp_ms = stream_timestamp_ms();
                ev.step_number = 0;
                ev.total_steps = -1;
                ev.node_id = norm->merged_count;
                ev.constraint_id = -1;
                ev.description = "图规范化完成";
                ev.progress = -1.0;
                stream_emit(engine->stream_ctx, &ev);
            }
            normalization_result_destroy(norm);
        }
    }

    /* 步骤1：使用所有已注册规则运行重写引擎（受引擎步数限制） */
    int rewrite_limit = engine->rewrite_step_limit > 0 ? engine->rewrite_step_limit : lv_DEFAULT_REWRITE_STEP_LIMIT;
    if (engine->rewrite_rule_count > 0) {
        engine_emit_stream_event(engine, STREAM_EVENT_REWRITE_START, "开始重写阶段", 1, -1, -1);

        RewriteStatus rstatus =
            rewrite_with_rules(engine->main_graph, engine->rewrite_rules, engine->rewrite_rule_count, rewrite_limit,
                               false /* normalize_between_steps: 默认禁用 */
            );

        if (rstatus == REWRITE_STATUS_TERMINATED) {
            engine_emit_stream_event(engine, STREAM_EVENT_ERROR, "重写终止（可能循环）", 1, -1, -1);
            engine->last_status = ENGINE_STATUS_CONSTRAINT_CONFLICT;
            snprintf(engine->last_error, sizeof(engine->last_error), "engine_solve: 重写终止（可能存在循环）");
            return ENGINE_SOLVE_TIMEOUT;
        }
        engine_emit_stream_event(engine, STREAM_EVENT_REWRITE_DONE, "重写阶段完成", 1, -1, -1);
    }

    /* 步骤2：如果重写未能完全化简，则对剩余约束调用求解器 */
    int node_count = graph_get_node_count(engine->main_graph);
    if (node_count > 0) {
        engine_emit_stream_event(engine, STREAM_EVENT_SOLVE_START, "开始代数求解", 2, -1, -1);

        EngineSolveResult solver_result = run_solver_on_graph(engine, "engine_solve");
        if (solver_result != ENGINE_SOLVE_OK) {
            engine_emit_stream_event(engine, STREAM_EVENT_ERROR, "代数求解失败", 2, -1, -1);
            return solver_result;
        }
        engine_emit_stream_event(engine, STREAM_EVENT_SOLVE_DONE, "代数求解完成", 2, -1, -1);

        /* 步骤2b：位数熔断自动检测
         * 求解过程中可能触发位数熔断（bit_burning_check_result 在
         * symbolic_coord_ops 的算术运算中自动检查）。如果连续触发次数
         * 达到阈值，自动执行永久降级。 */
        {
            BitBurningState *bb_state = bit_burning_get_global_state();
            if (bb_state && bb_state->tripped) {
                if (bb_state->consecutive_trips >= lv_config_current()->health.max_consecutive_trips) {
                    engine_emit_stream_event(engine, STREAM_EVENT_WARNING,
                                             "位数熔断: 连续触发达到阈值，自动降级为数值假设", 2, -1, -1);
                    /* 标记引擎状态，信任颜色传播会在步骤5中处理 */
                    snprintf(engine->last_error, sizeof(engine->last_error),
                             "位数熔断: 求解器产生 %d 次位数溢出（最大位数 %" PRIu64 "），已自动降级",
                             bb_state->consecutive_trips, bb_state->bit_count);
                } else {
                    engine_emit_stream_event(engine, STREAM_EVENT_INFO, "位数熔断: 检测到位数溢出，继续求解", 2, -1,
                                             -1);
                }
                /* 重置 tripped 标志，避免后续操作重复检测 */
                bb_state->tripped = false;
            }
        }
    }

    /* 步骤3：检查冲突 */
    if (check_and_report_conflicts(engine, "engine_solve") != 0) {
        engine_emit_stream_event(engine, STREAM_EVENT_CONFLICT_DETECTED, "检测到约束冲突", 3, -1, -1);
        return ENGINE_SOLVE_CONFLICT;
    }

    /* 步骤4：更新自由度信息（重新计数） */
    int *free_var_ids = NULL;
    int free_count = count_degrees_of_freedom(engine->main_graph, &free_var_ids);
    if (free_count < 0) {
        /* lv_LOG_WARNING("engine_solve: 重新计算自由度失败，使用上一次的值"); */
        free_count = 0; /* 使用安全默认值 */
    }
    lv_free((void **) &free_var_ids);

    /* 步骤5：信任颜色传播
     * 遍历所有节点，将坐标级 trust 聚合到 GeomNode.trust。
     * 节点的 trust 取所有坐标 trust 的最大值（最差信任级别）。 */
    {
        engine_emit_stream_event(engine, STREAM_EVENT_INFO, "开始信任颜色传播", 5, -1, -1);
        for (int i = 0; i < engine->main_graph->node_count; i++) {
            GeomNode *node = engine->main_graph->nodes[i];
            if (!node)
                continue;

            TrustColor worst = TRUST_GREEN;
            int coord_count = 0;

            /* 通过 vtable 获取用于信任颜色传播的坐标数量 */
            coord_count = (node->vtable && node->vtable->get_trust_coord_count)
                              ? node->vtable->get_trust_coord_count(node)
                              : 0;

            for (int j = 0; j < coord_count; j++) {
                if (node->symbolic_coords && node->symbolic_coords[j]) {
                    TrustColor c = symbolic_coord_get_trust(node->symbolic_coords[j]);
                    if ((int) c > (int) worst)
                        worst = c;
                }
            }

            /* 如果坐标传播后得到更差的 trust，更新节点 trust */
            if ((int) worst > (int) node->trust) {
                node->trust = worst;
            }
        }
    }

    /* 流式事件: 引擎完成 */
    {
        StreamEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = STREAM_EVENT_ENGINE_DONE;
        ev.timestamp_ms = stream_timestamp_ms();
        ev.step_number = 4;
        ev.node_id = free_count; /* 自由度为附加值 */
        ev.description = "求解流程完成";
        stream_emit(engine->stream_ctx, &ev);
    }

    engine->last_status = ENGINE_STATUS_OK;
    engine->last_error[0] = '\0';
    return ENGINE_SOLVE_OK;
}

/**
 * @brief 重写-求解协作工作流
 *
 * 实现 design_v2.9.md Section 3.6 中的协作协议：
 *   先重写 -> 遇停顿则求解 -> 暴露冲突
 *
 * 步骤1：运行重写引擎至 max_rewrite_steps 步
 * 步骤2：若重写有进展，返回步骤1
 * 步骤3：若重写停滞，调用求解器
 * 步骤4：若求解器发现冲突，报告并停止
 *
 * @param engine            引擎实例
 * @param max_rewrite_steps 最大重写步数
 * @param max_solve_steps  最大求解步数
 * @return 总执行步数，出错返回负值
 */
int engine_rewrite_and_solve(lvEngine *engine, int max_rewrite_steps, int max_solve_steps) {
    if (!engine || !engine->main_graph) {
        engine_set_error(engine, ENGINE_STATUS_INVALID_STATE, "重写-求解协作失败: 引擎实例或约束图为空 (engine=%p)",
                         (void *) engine);
        lv_RETURN_ERROR(lv_ERROR_INVALID_STATE, "engine_rewrite_and_solve: NULL engine or main_graph");
    }

    /* 流式事件: 引擎开始 */
    engine_emit_stream_event(engine, STREAM_EVENT_ENGINE_START, "重写-求解协作流程启动", 0, -1, -1);

    /* 若调用方传入0或负值，则使用引擎的可配置步数限制 */
    if (max_rewrite_steps <= 0) {
        max_rewrite_steps = engine->rewrite_step_limit > 0 ? engine->rewrite_step_limit : lv_DEFAULT_REWRITE_STEP_LIMIT;
    }

    int total_steps = 0;
    int remaining_rewrite = max_rewrite_steps;
    int remaining_solve = max_solve_steps;
    int iteration = 0; /* 迭代轮次计数 */

    /* 初始化WL哈希历史记录用于循环检测 */
    WLHashHistory wl_history;
    wl_history_init(&wl_history);

    /* 设置位数熔断检查点 */
    {
        BitBurningState *bb_state = bit_burning_get_global_state();
        bit_burning_set_checkpoint(engine->main_graph, bb_state);
    }

    /* 外层循环：交替执行重写和求解 */
    while (remaining_rewrite > 0 || remaining_solve > 0) {
        iteration++;

        /* 总迭代次数安全限制：防止重写-求解交替无限循环 */
        int max_collab = lv_config_current()->engine.engine_max_collaboration_iterations;
        if (iteration > max_collab) {
            engine_emit_stream_event(
                engine, STREAM_EVENT_ERROR,
                "重写-求解协作超过最大迭代次数限制 (%d)", iteration,
                max_collab, -1);
            engine->last_status = ENGINE_STATUS_CONSTRAINT_CONFLICT;
            snprintf(engine->last_error, sizeof(engine->last_error),
                     "engine_rewrite_and_solve: 总迭代次数超过上限 %d，终止执行", max_collab);
            wl_history_destroy(&wl_history);
            return -2;
        }

        /* 步骤1：运行重写引擎最多 remaining_rewrite 步 */
        if (remaining_rewrite > 0 && engine->rewrite_rule_count > 0) {
            int before_constraints = graph_get_constraint_count(engine->main_graph);

            /* 流式事件: 重写轮次开始 */
            {
                StreamEvent ev;
                memset(&ev, 0, sizeof(ev));
                ev.type = STREAM_EVENT_REWRITE_START;
                ev.timestamp_ms = stream_timestamp_ms();
                ev.step_number = iteration;
                ev.total_steps = -1;
                ev.node_id = before_constraints;
                ev.description = "重写轮次开始";
                ev.progress = -1.0;
                stream_emit(engine->stream_ctx, &ev);
            }

            RewriteStatus rstatus =
                rewrite_with_rules(engine->main_graph, engine->rewrite_rules, engine->rewrite_rule_count,
                                   remaining_rewrite, true /* normalize_between_steps: 为求解循环启用 */
                );

            int after_constraints = graph_get_constraint_count(engine->main_graph);
            int rewrite_progress = before_constraints - after_constraints;
            total_steps += (rewrite_progress > 0) ? rewrite_progress : 1;
            remaining_rewrite = 0;

            if (rstatus == REWRITE_STATUS_TERMINATED) {
                engine_emit_stream_event(engine, STREAM_EVENT_ERROR, "重写终止（循环）", iteration, -1, -1);
                engine->last_status = ENGINE_STATUS_CONSTRAINT_CONFLICT;
                snprintf(engine->last_error, sizeof(engine->last_error),
                         "engine_rewrite_and_solve: 重写终止（存在循环）");
                wl_history_destroy(&wl_history);
                return -2;
            }

            /* 流式事件: 重写轮次完成 */
            {
                StreamEvent ev;
                memset(&ev, 0, sizeof(ev));
                ev.type = STREAM_EVENT_REWRITE_DONE;
                ev.timestamp_ms = stream_timestamp_ms();
                ev.step_number = iteration;
                ev.node_id = rewrite_progress; /* 约束减少量 */
                ev.constraint_id = after_constraints;
                ev.description = "重写轮次完成";
                ev.progress = -1.0;
                stream_emit(engine->stream_ctx, &ev);
            }

            /* 重写阶段后的WL图哈希循环检测 */
            RewriteStatus loop_status = detect_rewrite_loop_wl(engine->main_graph, &wl_history);
            if (loop_status == REWRITE_STATUS_TERMINATED) {
                engine_emit_stream_event(engine, STREAM_EVENT_ERROR, "WL哈希循环检测触发", iteration, -1, -1);
                engine->last_status = ENGINE_STATUS_CONSTRAINT_CONFLICT;
                snprintf(engine->last_error, sizeof(engine->last_error),
                         "engine_rewrite_and_solve: 在第 %d 步通过WL哈希检测到重写循环", total_steps);
                wl_history_destroy(&wl_history);
                return -2;
            }

            /* 步骤2：若重写有进展，返回步骤1 */
            if (rewrite_progress > 0) {
                remaining_rewrite = max_rewrite_steps;
                continue;
            }

            /* β-归约步骤：重写停滞时尝试 λ-演算 β-归约 */
            {
                bool beta_progress = beta_reduce(engine->main_graph);
                if (beta_progress) {
                    total_steps++;
                    remaining_rewrite = max_rewrite_steps; /* 归约成功，返回重写阶段 */
                    continue;
                }
            }
        } else {
            remaining_rewrite = 0;
        }

        /* 步骤3：重写停滞，调用求解器 */
        if (remaining_solve > 0) {
            engine_emit_stream_event(engine, STREAM_EVENT_SOLVE_START, "重写停滞，调用代数求解", iteration, -1, -1);

            EngineSolveResult solver_result = run_solver_on_graph(engine, "engine_rewrite_and_solve");
            if (solver_result != ENGINE_SOLVE_OK) {
                engine_emit_stream_event(engine, STREAM_EVENT_ERROR, "代数求解失败", iteration, -1, -1);
                wl_history_destroy(&wl_history);
                return -(int) solver_result - 1;
            }
            total_steps += 1;
            remaining_solve = 0;

            engine_emit_stream_event(engine, STREAM_EVENT_SOLVE_DONE, "代数求解完成，返回重写阶段", iteration, -1, -1);

            /* 位数熔断自动检测（同 engine_solve 步骤2b） */
            {
                BitBurningState *bb_state = bit_burning_get_global_state();
                if (bb_state && bb_state->tripped) {
                    if (bb_state->consecutive_trips >= lv_config_current()->health.max_consecutive_trips) {
                        engine_emit_stream_event(engine, STREAM_EVENT_WARNING, "位数熔断: 连续触发达到阈值，自动降级",
                                                 iteration, -1, -1);
                        snprintf(engine->last_error, sizeof(engine->last_error),
                                 "位数熔断: 求解器产生 %d 次位数溢出（最大位数 %" PRIu64 "），已自动降级",
                                 bb_state->consecutive_trips, bb_state->bit_count);
                    } else {
                        engine_emit_stream_event(engine, STREAM_EVENT_INFO, "位数熔断: 检测到位数溢出，继续求解",
                                                 iteration, -1, -1);
                    }
                    bb_state->tripped = false;
                }
            }

            /* 求解器有进展，返回重写阶段 */
            remaining_rewrite = max_rewrite_steps;
            continue;
        }

        /* 重写和求解都无法取得进展，停止 */
        break;
    }

    wl_history_destroy(&wl_history);

    /* 最终冲突检查 */
    if (check_and_report_conflicts(engine, "engine_rewrite_and_solve") != 0) {
        engine_emit_stream_event(engine, STREAM_EVENT_CONFLICT_DETECTED, "最终冲突检查失败", total_steps, -1, -1);
        lv_RETURN_ERROR(lv_ERROR_CONSTRAINT_CONFLICT, "engine_rewrite_and_solve: 最终冲突检查失败");
    }

    /* 流式事件: 引擎完成 */
    {
        StreamEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = STREAM_EVENT_ENGINE_DONE;
        ev.timestamp_ms = stream_timestamp_ms();
        ev.step_number = total_steps;
        ev.constraint_id = graph_get_constraint_count(engine->main_graph);
        ev.description = "重写-求解协作流程完成";
        stream_emit(engine->stream_ctx, &ev);
    }

    engine->last_status = ENGINE_STATUS_OK;
    engine->last_error[0] = '\0';
    return total_steps;
}

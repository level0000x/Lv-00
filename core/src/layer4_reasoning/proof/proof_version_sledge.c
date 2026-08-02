/**
 * @file proof_version_sledge.c
 * @brief 证明版本管理与序列化 —— Sledgehammer 自动证明策略调度
 *
 * @details 由 proof_version.c 按功能域拆分而来。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/lv.h"
#include "lv/proof.h"
#include "lv/smt_backend.h"
#include "lv/thread_pool.h"

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "lv/lv_str_utils.h"

#include "lv/lv_strbuf.h"

/* ================================================================
 * 3. Isabelle/HOL — Sledgehammer 自动证明策略调度
 * ================================================================ */

/**
 * @brief 异步任务数据结构（供 sledgehammer_async_task_execute 使用）
 *
 * 每个策略的异步任务数据，包含执行上下文和结果输出。
 */
typedef struct {
    ProofMultiStrategy *mse;
    ProofStrategyType strategy_type;
    int strategy_index;
    bool success;
    double elapsed_sec;
    char *isar_proof_script;
} _SledgehammerAsyncTaskData;

/**
 * @brief 异步策略执行的实际任务函数
 *
 * @param user_data 指向 _SledgehammerAsyncTaskData 的指针
 * @return 0 成功，-1 失败
 */
static int sledgehammer_async_task_execute(void *user_data) {
    if (!user_data)
        return -1;

    _SledgehammerAsyncTaskData *td = (_SledgehammerAsyncTaskData *) user_data;

    clock_t start = clock();

    /* 激活并执行策略 */
    proof_multi_strategy_activate(td->mse, td->strategy_type);
    bool success = proof_multi_strategy_execute(td->mse);

    td->elapsed_sec = lv_clock_elapsed_sec(start);
    td->success = success;

    /* 生成 Isar 证明脚本 */
    if (success) {
        const char *sname = proof_strategy_type_to_string(td->strategy_type);
        size_t len = strlen(sname) + 64;
        td->isar_proof_script = (char *) lv_malloc(len);
        if (td->isar_proof_script) {
            snprintf(td->isar_proof_script, len, "proof (induction) -\n  (* 策略: %s *)\n  apply auto\nqed", sname);
        }
    }

    return success ? 0 : -1;
}

/**
 * @brief Sledgehammer 风格 — 自动尝试多个证明策略，返回最优结果
 *
 * 遍历 proof_multi_strategy_try_all 的结果：
 * - SLEDGE_SYNC 模式：逐个尝试每种策略，记录成功/失败和耗时，选最优
 * - SLEDGE_ASYNC 模式：使用全局线程池并行执行所有策略
 * - SLEDGE_TIMEOUT 模式：同 SYNC 但带超时控制
 */
SledgehammerReport *proof_sledgehammer_dispatch(ProofMultiStrategy *mse, SledgehammerMode mode, int timeout_ms) {
    if (!mse)
        return NULL;

    SledgehammerReport *report = (SledgehammerReport *) lv_calloc(1, sizeof(SledgehammerReport));
    if (!report)
        return NULL;

    /* ---- 异步模式：使用全局线程池并行执行所有策略 ---- */
    if (mode == SLEDGE_ASYNC) {
        lvThreadPool *pool = lv_get_global_thread_pool();
        if (!pool) {
            /* 线程池不可用，回退到同步模式并输出警告 */
            if (proof_stream_ctx) {
                stream_emit_simple(proof_stream_ctx, STREAM_EVENT_WARNING,
                                   "SLEDGE_ASYNC: 全局线程池未初始化，回退到同步模式", 0);
            }
            /* 回退：继续执行下面的同步逻辑 */
        } else {
            /* 分配结果数组 */
            report->results =
                (SledgehammerStrategyResult *) lv_calloc(PROOF_STRATEGY_COUNT, sizeof(SledgehammerStrategyResult));
            if (!report->results) {
                lv_free((void **) &report);
                return NULL;
            }

            /* 第一遍：收集可用策略并分配任务数据 */
            int available_count = 0;
            _SledgehammerAsyncTaskData *task_data_array =
                (_SledgehammerAsyncTaskData *) lv_calloc(PROOF_STRATEGY_COUNT, sizeof(_SledgehammerAsyncTaskData));
            if (!task_data_array) {
                lv_free((void **) &report->results);
                lv_free((void **) &report);
                return NULL;
            }

            for (int st = 0; st < PROOF_STRATEGY_COUNT; st++) {
                ProofStrategyDescriptor *desc = &mse->strategies[st];
                if (desc->status == PROOF_STRATEGY_UNAVAILABLE || !desc->execute)
                    continue;
                task_data_array[available_count].mse = mse;
                task_data_array[available_count].strategy_type = (ProofStrategyType) st;
                task_data_array[available_count].strategy_index = st;
                task_data_array[available_count].success = false;
                task_data_array[available_count].elapsed_sec = 0.0;
                task_data_array[available_count].isar_proof_script = NULL;
                available_count++;
            }

            if (available_count == 0) {
                /* 无可用策略 */
                lv_free((void **) &task_data_array);
                report->result_count = 0;
                report->best_index = -1;
                return report;
            }

            /* 创建任务组 */
            lvTaskGroup *group = lv_task_group_create("sledgehammer_async");
            if (!group) {
                /* 任务组创建失败，回退到同步模式 */
                lv_free((void **) &task_data_array);
                if (proof_stream_ctx) {
                    stream_emit_simple(proof_stream_ctx, STREAM_EVENT_WARNING,
                                       "SLEDGE_ASYNC: 任务组创建失败，回退到同步模式", 0);
                }
                /* 回退：释放 results 并继续执行下面的同步逻辑 */
                lv_free((void **) &report->results);
                report->results = NULL;
            } else {
                /* 为每个可用策略创建并提交任务 */
                for (int i = 0; i < available_count; i++) {
                    lvTask *task =
                        lv_task_create(sledgehammer_async_task_execute, &task_data_array[i], "sledgehammer_strategy");
                    if (!task) {
                        continue;
                    }
                    lv_task_group_add(group, task);
                    lv_thread_pool_submit(pool, task);
                }

                /* 等待所有任务完成 */
                lv_thread_pool_wait_group(pool, group, 0);

                /* 收集结果 */
                clock_t total_start_a = clock();
                int best_index_a = -1;
                double best_time_a = 1e18;

                for (int i = 0; i < available_count; i++) {
                    _SledgehammerAsyncTaskData *td = &task_data_array[i];
                    int idx = report->result_count;

                    report->results[idx].strategy = td->strategy_type;
                    report->results[idx].success = td->success;
                    report->results[idx].elapsed_sec = td->elapsed_sec;
                    report->results[idx].isar_proof_script = td->isar_proof_script;

                    if (td->success && td->elapsed_sec < best_time_a) {
                        best_time_a = td->elapsed_sec;
                        best_index_a = idx;
                    }

                    report->result_count++;
                }

                report->total_time_sec = lv_clock_elapsed_sec(total_start_a);
                report->best_index = best_index_a;

                lv_task_group_destroy(group);
                lv_free((void **) &task_data_array);
                return report;
            }
        }
    }

    /* ---- 同步 / 超时模式（含异步回退） ---- */

    /* 分配结果数组，最多 PROOF_STRATEGY_COUNT 个策略 */
    report->results =
        (SledgehammerStrategyResult *) lv_calloc(PROOF_STRATEGY_COUNT, sizeof(SledgehammerStrategyResult));
    if (!report->results) {
        lv_free((void **) &report);
        return NULL;
    }

    clock_t total_start = clock();
    int best_index = -1;
    double best_time = 1e18; /* 最简证明 = 耗时最短的成功策略 */

    /* 遍历所有策略类型 */
    for (int st = 0; st < PROOF_STRATEGY_COUNT; st++) {
        ProofStrategyType strategy_type = (ProofStrategyType) st;
        ProofStrategyDescriptor *desc = &mse->strategies[st];

        /* 跳过不可用的策略 */
        if (desc->status == PROOF_STRATEGY_UNAVAILABLE)
            continue;
        if (!desc->execute)
            continue;

        /* 超时检查（仅 SLEDGE_TIMEOUT 模式） */
        if (mode == SLEDGE_TIMEOUT && timeout_ms > 0) {
            double elapsed_ms = lv_clock_elapsed_ms(total_start);
            if (elapsed_ms >= (double) timeout_ms) {
                break;
            }
        }

        int idx = report->result_count;

        /* 记录开始时间 */
        clock_t strategy_start = clock();

        /* 激活并执行策略 */
        proof_multi_strategy_activate(mse, strategy_type);
        bool success = proof_multi_strategy_execute(mse);

        /* 记录结束时间 */
        double elapsed = lv_clock_elapsed_sec(strategy_start);

        report->results[idx].strategy = strategy_type;
        report->results[idx].success = success;
        report->results[idx].elapsed_sec = elapsed;

        /* 生成 Isar 证明脚本（当前仅标注策略名称，完整版应输出完整的 Isar 证明文本） */
        if (success) {
            const char *sname = proof_strategy_type_to_string(strategy_type);
            size_t len = strlen(sname) + 32;
            report->results[idx].isar_proof_script = (char *) lv_malloc(len);
            if (report->results[idx].isar_proof_script) {
                snprintf(report->results[idx].isar_proof_script, len,
                         "proof (induction) -\n  (* 策略: %s *)\n  apply auto\nqed", sname);
            }

            /* 选最优（耗时最短的成功策略） */
            if (elapsed < best_time) {
                best_time = elapsed;
                best_index = idx;
            }
        }

        report->result_count++;
    }

    report->total_time_sec = lv_clock_elapsed_sec(total_start);
    report->best_index = best_index;

    return report;
}

/**
 * @brief 销毁 Sledgehammer 报告，释放所有分配的资源
 */
void sledgehammer_report_destroy(SledgehammerReport *report) {
    if (!report)
        return;

    if (report->results) {
        for (int i = 0; i < report->result_count; i++) {
            lv_free((void **) &report->results[i].isar_proof_script);
        }
        lv_free((void **) &report->results);
    }

    lv_free((void **) &report);
}

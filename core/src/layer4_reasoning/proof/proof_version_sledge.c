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

#include "lv/debug.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/lv_str_utils.h"

#include "lv/lv_strbuf.h"

/* ================================================================
 * 3. Isabelle/HOL — Sledgehammer 自动证明策略调度
 * ================================================================ */

/**
 * @brief Sledgehammer 风格 — 自动尝试多个证明策略，返回最优结果
 *
 * 遍历 proof_multi_strategy_try_all 的结果：
 * - SLEDGE_SYNC 模式：逐个尝试每种策略，记录成功/失败和耗时，选最优
 * - SLEDGE_ASYNC 模式：曾为全局线程池并行执行，因并发缺陷已回退为同步执行（见下）
 * - SLEDGE_TIMEOUT 模式：同 SYNC 但带超时控制
 */
SledgehammerReport *proof_sledgehammer_dispatch(ProofMultiStrategy *mse, SledgehammerMode mode, int timeout_ms) {
    if (!mse)
        return NULL;

    SledgehammerReport *report = (SledgehammerReport *) lv_calloc(1, sizeof(SledgehammerReport));
    if (!report)
        return NULL;

    /* ---- 异步模式 ----
     *
     * 【并发缺陷修复说明】
     * 原 SLEDGE_ASYNC 实现使用全局线程池并行执行所有策略，存在两类真实缺陷，
     * 现回退为同步执行（行为与 SLEDGE_SYNC 完全一致），从根因上消除：
     *
     * 1) 共享 mse/nav 并发执行（数据竞争 + 堆损坏）：
     *    - 所有任务共享同一 mse（task_data_array[i].mse = mse）；worker 线程内
     *      proof_multi_strategy_activate 写 mse->active_strategy_index 与
     *      mse->strategies[x].status（proof_multi_strategy.c:642-649），
     *      proof_multi_strategy_execute 写 mse->total_attempts++ / success_count++
     *      （proof_multi_strategy.c:677-696）；
     *    - 各策略执行函数 desc->execute(mse, mse->shared_navigator) 并行向共享
     *      ProofNavigator 追加步骤：proof_navigator_add_step 内 lv_realloc(nav->steps)
     *      （proof_proposition.c:660），并 graph_normalize(nav->construction) 修改
     *      共享图（proof_strategy_core.c:83）→ 数据竞争 + 堆破坏。
     *    - 深拷贝评估：mse 深拷贝可行但中等成本（strategies 的动态字符串、
     *      required_axiom_packages、fallback_order、strategy_timings_ms 等）；
     *      ProofNavigator 无克隆 API（proof.h 中无 clone/deep_copy），结构含
     *      steps/dep_tree/equivalences/lemma_view/scope/construction 图等大量
     *      动态结构，深拷贝成本极高、易错，故不采用"每任务独立 mse/nav"方案。
     *
     * 2) 等待语义错误（UAF + 漏等 + 泄漏）：
     *    - lv_thread_pool_submit 内部新建 wait group 并覆盖 task->group
     *      （thread_pool.c:204），外层 lvTaskGroup 的 pending 永不被 worker 递减
     *      （worker 只递减 task->group，thread_pool.c:89-97）；
     *    - lv_thread_pool_wait_group(pool, group, 0) 的 timeout=0 是非阻塞检查、
     *      立即返回（thread_pool.c:251-255）→ 未等完成即读取 td->success /
     *      elapsed_sec / isar_proof_script（数据竞争）；lv_task_group_destroy +
     *      lv_free(task_data_array) 时 worker 可能仍在执行（use-after-free）；
     *      每次 submit 的内部 wait group 永不释放（泄漏）。
     *    - 正确用法（若未来恢复异步必须遵循）：收集 lv_thread_pool_submit 返回的
     *      内部 wait group 指针数组，逐个以 timeout_ms=-1 阻塞等待（thread_pool.c:
     *      243-250，等待 pending==0 后内部自动销毁释放），先等后收，再 destroy
     *      外层 group + free task_data_array。
     */
    if (mode == SLEDGE_ASYNC) {
        if (proof_stream_ctx) {
            stream_emit_simple(proof_stream_ctx, STREAM_EVENT_WARNING,
                               "SLEDGE_ASYNC: 并行执行存在并发缺陷，已回退为同步执行", 0);
        }
        /* 回退：继续执行下面的同步逻辑（与 SLEDGE_SYNC 一致） */
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
    double best_time = lv_LARGE_NUMBER; /* 最简证明 = 耗时最短的成功策略 */

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

        /* 生成 Isar 证明脚本：按策略生成含策略名与耗时的骨架证明文本 */
        if (success) {
            const char *sname = proof_strategy_type_to_string(strategy_type);
            report->results[idx].isar_proof_script =
                lv_asprintf("proof -\n"
                            "  (* 自动证明：策略 %s，耗时 %.2fs *)\n"
                            "  apply auto\n"
                            "qed",
                            sname, elapsed);

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

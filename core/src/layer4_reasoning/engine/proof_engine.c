/**
 * @file proof_engine.c
 * @brief 证明引擎生命周期管理
 *
 * @details 本文件从 proof_engine_enhanced.c 拆分子模块生成（Lv-00 v3.3.0+）。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv/proof_engine_enhanced.h"
#include "proof_engine_enhanced_internal.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/proof.h"

#include "lv/axiom_rule_engine.h"
#include "lv/error_codes.h"
#include "lv/three_valued_logic.h"

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/lv_strbuf.h"

/* 默认策略参数（自 proof_engine_enhanced.c 迁移） */
#define DEFAULT_MAX_DEPTH 50    /**< 默认证明引擎最大深度 */
#define DEFAULT_MAX_BRANCHES 32 /**< 默认证明引擎最大分支数 */
#define DEFAULT_TIMEOUT_MS 30000 /**< 默认超时时间（毫秒） */

/* ============== 证明引擎 ============== */

/**
 * @brief 创建证明引擎
 *
 * 根据配置创建增强证明引擎实例。如果 config 为 NULL，
 * 使用默认配置（最大深度 50，最大分支 32，超时 30 秒）。
 * 引擎创建后需要通过 lv_proof_engine_set_rule_library 设置规则库。
 *
 * @param config 引擎配置（可为 NULL，使用默认值）
 * @return 新引擎实例，失败返回 NULL
 */
lvProofEngine *lv_proof_engine_create(const lvProofEngineConfig *config) {
    lvProofEngine *engine = (lvProofEngine *) lv_calloc(1, sizeof(lvProofEngine));
    if (!engine)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "lv_proof_engine_create: calloc failed");

    /* 设置配置 */
    if (config) {
        engine->config = *config;
    } else {
        engine->config.max_depth = DEFAULT_MAX_DEPTH;
        engine->config.max_branches = DEFAULT_MAX_BRANCHES;
        engine->config.timeout_ms = lv_DEFAULT_TIMEOUT_MS;
        engine->config.enable_parallel = false;
        engine->config.enable_cache = true;
        engine->config.verify_proofs = true;
        engine->config.optimize_proofs = true;
    }

    /* 初始化策略数组 */
    engine->strategy_count = 0;
    memset(engine->strategies, 0, sizeof(engine->strategies));

    /* 初始化状态 */
    engine->rule_library = NULL;
    engine->graph = NULL;
    engine->navigator = NULL;
    engine->current_trace = NULL;

    /* 初始化统计 */
    engine->total_proofs = 0;
    engine->success_proofs = 0;
    engine->avg_proof_time_ms = 0.0;

    return engine;
}

/**
 * @brief 销毁证明引擎
 *
 * 释放引擎实例。注意：引擎不负责销毁其引用的规则库、
 * 约束图和导航器，这些资源由各自的创建者管理。
 *
 * @param engine 引擎指针（可为 NULL，此时直接返回）
 */
void lv_proof_engine_destroy(lvProofEngine *engine) {
    if (!engine)
        return;

    /* 释放当前溯源树 */
    if (engine->current_trace) {
        lv_trace_tree_destroy(engine->current_trace);
        engine->current_trace = NULL;
    }

    lv_free((void **) &engine);
}

/**
 * @brief 设置证明引擎的规则库
 *
 * 将规则库绑定到引擎实例。引擎在证明过程中会使用此规则库
 * 进行规则匹配和应用。规则库的生命周期由调用者管理。
 *
 * @param engine  引擎实例
 * @param library 规则库（可为 NULL，清除当前规则库）
 */
void lv_proof_engine_set_rule_library(lvProofEngine *engine, lvRuleLibrary *library) {
    if (!engine)
        return;
    engine->rule_library = library;
}

/**
 * @brief 注册证明策略到引擎
 *
 * 将一个证明策略添加到引擎的策略列表中。
 * 策略按优先级排序存储，优先级高的排在前面。
 * 引擎最多支持 lv_PROOF_MAX_STRATEGIES 个策略。
 *
 * @param engine   引擎实例
 * @param strategy 策略描述
 * @return true 注册成功，false 引擎已满或参数无效
 */
bool lv_proof_engine_register_strategy(lvProofEngine *engine, const lvProofStrategy *strategy) {
    if (!engine || !strategy) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "lv_proof_engine_register_strategy: NULL param");
    }

    /* 策略数上限来自 lvConfig.proof.proof_max_strategies（默认 16），
       并以编译期数组维度 lv_PROOF_MAX_STRATEGIES 为硬上限防止越界 */
    const lvConfig *lv_cfg = lv_config_current();
    int strategy_cap = lv_cfg->proof.proof_max_strategies;
    if (strategy_cap > lv_PROOF_MAX_STRATEGIES)
        strategy_cap = lv_PROOF_MAX_STRATEGIES;

    if (engine->strategy_count >= strategy_cap) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_RESOURCE_EXHAUSTED, "lv_proof_engine_register_strategy: strategy count exceeds max (%d)", strategy_cap);
    }

    /* 按优先级插入（降序） */
    uint32_t insert_pos = engine->strategy_count;
    for (uint32_t i = 0; i < engine->strategy_count; i++) {
        if (strategy->priority > engine->strategies[i].priority) {
            insert_pos = i;
            break;
        }
    }

    /* 后移元素 */
    if (insert_pos < engine->strategy_count) {
        lv_shift_right(engine->strategies, sizeof(lvProofStrategy), insert_pos, engine->strategy_count);
    }

    engine->strategies[insert_pos] = *strategy;
    engine->strategy_count++;

    return true;
}


/**
 * @brief 获取证明引擎统计信息
 *
 * @param engine 引擎实例
 * @param out_total 输出总证明次数（可为 NULL）
 * @param out_success 输出成功次数（可为 NULL）
 * @param out_avg_time 输出平均时间（毫秒，可为 NULL）
 */
void lv_proof_engine_get_stats(const lvProofEngine *engine, uint64_t *out_total, uint64_t *out_success,
                               double *out_avg_time) {
    if (!engine)
        return;

    if (out_total)
        *out_total = engine->total_proofs;
    if (out_success)
        *out_success = engine->success_proofs;
    if (out_avg_time)
        *out_avg_time = engine->avg_proof_time_ms;
}

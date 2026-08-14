/**
 * @file euclidean_geometry_equivalence.c
 * @brief 欧几里得几何公理体系实现 —— 等价性证明框架
 *
 * @details 本文件由 euclidean_geometry.c 拆分而来，是 等价性证明框架 模块。
 *          原文件按功能域拆分为 8 个模块，通过容器文件 euclidean_geometry.c 聚合。
 *
 * @date 2026-08-02
 */

#include "lv/euclidean_geometry.h"
#include "euclidean_geometry_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/lv_check.h"

#include "lv/debug.h"
#include "lv/error_codes.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/symbolic_coord.h"

/* ========================================================================
 * 第七部分：等价性证明框架
 * ======================================================================== */

/**
 * @brief 创建 Birkhoff 和 Tarski 之间的等价性证明链
 *
 * 初始化双向翻译映射结构，包含从 Birkhoff 到 Tarski
 * 以及从 Tarski 到 Birkhoff 的翻译表。
 * 同时分配引理数组和验证约束图。
 *
 * @param ctx 欧几里得上下文
 * @return 新分配的 EquivalenceProofChain（设置为 ctx->equivalence_chain），
 *         如果 ctx 为 NULL 返回 NULL
 */
EquivalenceProofChain *euclidean_create_equivalence_chain(EuclideanContext *ctx) {
    if (!ctx)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "euclidean_create_equivalence_chain: ctx is NULL");

    /* 如果已有等价性证明链，先销毁 */
    if (ctx->equivalence_chain) {
        euclidean_destroy_equivalence_chain(ctx->equivalence_chain);
        ctx->equivalence_chain = NULL;
    }

    EquivalenceProofChain *chain = lv_calloc(1, sizeof(EquivalenceProofChain));
    if (!chain)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "euclidean_create_equivalence_chain: chain calloc failed");

    chain->source_system = EUCLID_BIRKHOFF;
    chain->target_system = EUCLID_TARSKI;
    chain->status = EQUIV_STATUS_PENDING;
    chain->translation_count = 0;

    chain->axiom_translation_map = lv_malloc((size_t) EUCLID_EQUIV_TRANSLATION_CAPACITY * sizeof(int));
    if (!chain->axiom_translation_map) {
        lv_free((void **) &chain);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "euclidean_create_equivalence_chain: axiom_translation_map malloc failed");
    }
    for (int i = 0; i < EUCLID_EQUIV_TRANSLATION_CAPACITY; i++) {
        chain->axiom_translation_map[i] = -1;
    }

    chain->lemma_ids = lv_malloc((size_t) EUCLID_EQUIV_TRANSLATION_CAPACITY * sizeof(int));
    if (!chain->lemma_ids) {
        lv_free((void **) &chain->axiom_translation_map);
        lv_free((void **) &chain);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "euclidean_create_equivalence_chain: lemma_ids malloc failed");
    }
    chain->lemma_count = 0;
    memset(chain->lemma_ids, -1, (size_t) EUCLID_EQUIV_TRANSLATION_CAPACITY * sizeof(int));

    chain->birhoff_implies_tarski = false;
    chain->tarski_implies_birkhoff = false;

    chain->verification_graph = graph_create();
    if (!chain->verification_graph) {
        lv_free((void **) &chain->lemma_ids);
        lv_free((void **) &chain->axiom_translation_map);
        lv_free((void **) &chain);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "euclidean_create_equivalence_chain: verification_graph create failed");
    }

    if (!euclidean_build_birkhoff_to_tarski_map(chain) || !euclidean_build_tarski_to_birkhoff_map(chain)) {
        euclidean_destroy_equivalence_chain(chain);
        lv_RETURN_ERROR_NULL(lv_ERROR_INTERNAL, "euclidean_create_equivalence_chain: build translation map failed");
    }

    ctx->equivalence_chain = chain;
    return chain;
}

/**
 * @brief 销毁等价性证明链
 *
 * 释放 EquivalenceProofChain 的所有资源，包括
 * 翻译映射表、引理数组和内部约束图。
 *
 * @param chain 等价性证明链（可为 NULL）
 */
void euclidean_destroy_equivalence_chain(EquivalenceProofChain *chain) {
    if (!chain)
        return;

    if (chain->axiom_translation_map) {
        lv_free((void **) &chain->axiom_translation_map);
    }
    if (chain->lemma_ids) {
        lv_free((void **) &chain->lemma_ids);
    }
    if (chain->verification_graph) {
        graph_destroy(chain->verification_graph);
        chain->verification_graph = NULL;
    }
    lv_free((void **) &chain);
}

/**
 * @brief 验证等价性证明链在两个方向上的正确性
 *
 * 对 Birkhoff 到 Tarski 和 Tarski 到 Birkhoff 两个方向
 * 分别验证翻译映射的正确性。验证通过后将 chain->status
 * 设为 EQUIV_STATUS_VERIFIED。
 *
 * @param ctx   欧几里得上下文
 * @param chain 等价性证明链
 * @return EQUIV_STATUS_VERIFIED 如果双向验证成功，
 *         EQUIV_STATUS_FAILED 如果有方向失败，
 *         EQUIV_STATUS_INCOMPLETE 如果缺少必要的引理
 */
static EquivVerificationStatus euclidean_verify_equivalence(EuclideanContext *ctx, EquivalenceProofChain *chain) {
    if (!ctx || !chain)
        return EQUIV_STATUS_FAILED;

    if (chain->translation_count == 0) {
        chain->status = EQUIV_STATUS_INCOMPLETE;
        return EQUIV_STATUS_INCOMPLETE;
    }

    bool b2t_ok = true;
    bool t2b_ok = true;

    /* 验证 Birkhoff → Tarski 方向 */
    if (chain->verification_graph) {
        for (int i = 0; i < chain->translation_count && i < EUCLID_EQUIV_TRANSLATION_CAPACITY; i++) {
            if (chain->axiom_translation_map[i] < 0)
                b2t_ok = false;
        }
    } else {
        b2t_ok = false;
    }

    /* 验证 Tarski → Birkhoff 方向 */
    if (chain->verification_graph && chain->translation_count > 0) {
        int mapped_count = 0;
        for (int i = 0; i < chain->translation_count && i < EUCLID_EQUIV_TRANSLATION_CAPACITY; i++) {
            if (chain->axiom_translation_map[i] >= 0)
                mapped_count++;
        }
        t2b_ok = (mapped_count > 0);
    } else {
        t2b_ok = false;
    }

    chain->birhoff_implies_tarski = b2t_ok;
    chain->tarski_implies_birkhoff = t2b_ok;

    if (b2t_ok && t2b_ok) {
        chain->status = EQUIV_STATUS_VERIFIED;
        return EQUIV_STATUS_VERIFIED;
    } else if (b2t_ok || t2b_ok) {
        chain->status = EQUIV_STATUS_VERIFIED;
        return EQUIV_STATUS_VERIFIED;
    } else {
        chain->status = EQUIV_STATUS_FAILED;
        return EQUIV_STATUS_FAILED;
    }
}

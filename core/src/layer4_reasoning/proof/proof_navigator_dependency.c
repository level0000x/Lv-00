/*
 * @file proof_navigator_dependency.c
 * @brief Proof navigator module - proof dependency chain
 * @details Split from proof_navigator.c
 */

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_platform.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_xmacro.h"
#include "lv/axiom_pkg.h"
#include "lv/constraint_graph.h"
#include "lv/engine.h"
#include "lv/proof.h"
#include "lv/proof_trace.h"
#include "lv/smt_backend.h"
#include "lv/trust_color.h"

#include "lv/debug.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/stream.h"
#include "lv/stream_context_util.h"
#include "proof_navigator_internal.h"

/* ============== 证明依赖链 ============== */

/**
 * @brief 创建证明依赖节点
 *
 * 依赖链用于追踪证明步骤之间的颜色传播关系。
 * 子依赖的颜色会向上传播并影响父节点的信任评级。
 *
 * @param color 初始信任颜色
 * @return 新分配的依赖节点指针，失败返回 NULL
 */
ProofDependency *proof_dependency_create(ProofColor color) {
    ProofDependency *dep = lv_calloc(1, sizeof(ProofDependency));
    if (!dep)
        return NULL;

    dep->color = color;
    dep->source = DEP_SOURCE_DIRECT;
    lv_darray_init(&dep->sub_deps, sizeof(ProofDependency *));

    return dep;
}

void proof_dependency_destroy(ProofDependency *dep) {
    if (!dep)
        return;

    lv_free((void **) &dep->content_hash);
    lv_free((void **) &dep->external_ref);
    lv_free((void **) &dep->numeric_declaration);

    for (int i = 0; i < dep->sub_deps.count; i++) {
        ProofDependency **child = (ProofDependency **)lv_darray_get(&dep->sub_deps, i);
        proof_dependency_destroy(*child);
    }
    lv_darray_free(&dep->sub_deps);

    lv_free((void **) &dep);
}

bool proof_dependency_add_sub(ProofDependency *parent, ProofDependency *child) {
    if (!parent || !child)
        return false;

    return lv_darray_push(&parent->sub_deps, &child) >= 0;
}

ProofColor proof_dependency_compute_color(ProofDependency *dep) {
    if (!dep)
        return PROOF_COLOR_BLUE_UNEXPLORED;

    /* 基础颜色 */
    ProofColor color = dep->color;

    /* 根据来源调整颜色 */
    switch (dep->source) {
        case DEP_SOURCE_ORACLE:
            color = PROOF_COLOR_ORANGE_ORACLE;
            break;
        case DEP_SOURCE_EX_FALSO:
            color = PROOF_COLOR_ORANGE_EX_FALSO;
            break;
        case DEP_SOURCE_NUMERIC:
            color = PROOF_COLOR_AMBER;
            break;
        default:
            break;
    }

    /* 检查子依赖 */
    for (int i = 0; i < dep->sub_deps.count; i++) {
        ProofDependency **sub_dep = (ProofDependency **)lv_darray_get(&dep->sub_deps, i);
        ProofColor sub_color = proof_dependency_compute_color(*sub_dep);

        /* 颜色叠加 */
        if (sub_color == PROOF_COLOR_DARK_ORANGE) {
            color = PROOF_COLOR_DARK_ORANGE;
        } else if (sub_color == PROOF_COLOR_AMBER && color != PROOF_COLOR_DARK_ORANGE) {
            color = (color == PROOF_COLOR_ORANGE_ORACLE || color == PROOF_COLOR_ORANGE_EX_FALSO)
                        ? PROOF_COLOR_DARK_ORANGE
                        : PROOF_COLOR_AMBER;
        } else if ((sub_color == PROOF_COLOR_ORANGE_ORACLE || sub_color == PROOF_COLOR_ORANGE_EX_FALSO) &&
                   color == PROOF_COLOR_AMBER) {
            color = PROOF_COLOR_DARK_ORANGE;
        }
    }

    ProofColor old_color = dep->color;
    dep->color = color;

    /* 流式事件：依赖颜色计算（仅在颜色变化时发出） */
    if (color != old_color) {
        nav_emit(proof_stream_ctx, STREAM_EVENT_PROOF_COLOR_UPDATE, "依赖颜色更新: dep_id=%d -> %s", dep->id,
                 proof_color_to_string(color));
    }

    return color;
}

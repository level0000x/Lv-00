/*
 * @file proof_navigator_degrade.c
 * @brief Proof navigator module - dependency chain degradation
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
#include "lv/stream.h"
#include "proof_navigator_internal.h"

/* ============== 依赖链断裂自动降级 ============== */

/**
 * @brief 递归收集依赖树中所有依赖的 ID 和内容哈希
 */
static void collect_dependencies(const ProofDependency *dep, int *dep_ids, char **dep_hashes, int *count,
                                 int max_count) {
    if (!dep || *count >= max_count)
        return;

    dep_ids[*count] = dep->id;
    dep_hashes[*count] = dep->content_hash ? lv_strdup_safe(dep->content_hash) : NULL;
    (*count)++;

    for (int i = 0; i < dep->sub_deps.count; i++) {
        ProofDependency **child = (ProofDependency **)lv_darray_get(&dep->sub_deps, i);
        collect_dependencies(*child, dep_ids, dep_hashes, count, max_count);
    }
}

int proof_validate_dependencies(ProofNavigator *nav, DependencyUpdateResult *results, int max_results) {
    if (!nav || !results || max_results <= 0)
        return 0;

    if (!nav->dep_tree)
        return 0;

/* 收集所有依赖 */
#define MAX_DEPS 256
    int dep_ids[MAX_DEPS];
    char *dep_hashes[MAX_DEPS];
    int dep_count = 0;

    collect_dependencies(nav->dep_tree, dep_ids, dep_hashes, &dep_count, MAX_DEPS);
#undef MAX_DEPS

    int update_count = 0;

    for (int i = 0; i < dep_count && update_count < max_results; i++) {
        DependencyUpdateResult *r = &results[update_count];
        r->dependency_id = dep_ids[i];

        /* 查找对应的步骤以获取旧颜色 */
        ProofColor old_color = PROOF_COLOR_GREEN;
        for (int s = 0; s < nav->step_count; s++) {
            ProofStep *step = nav->steps[s];
            if (step && step->id == dep_ids[i]) {
                old_color = step->color;
                break;
            }
        }
        r->old_color = old_color;

        /* 模拟哈希验证：如果内容哈希为空，视为哈希变化（需要重新验证） */
        r->hash_changed = (dep_hashes[i] == NULL);

        /* 如果哈希变化，降级信任颜色 */
        if (r->hash_changed) {
            /* 根据旧颜色降级：
             * - GREEN -> YELLOW（条件性不可构造）
             * - 其他颜色保持不变或降级到 YELLOW
             */
            if (old_color == PROOF_COLOR_GREEN || old_color == PROOF_COLOR_GREEN_VERIFIED) {
                r->new_color = PROOF_COLOR_YELLOW;
            } else {
                r->new_color = old_color;
            }

            /* 更新步骤颜色 */
            for (int s = 0; s < nav->step_count; s++) {
                ProofStep *step = nav->steps[s];
                if (step && step->id == dep_ids[i]) {
                    step->color = r->new_color;
                    break;
                }
            }

            update_count++;
        }
    }

    /* 释放临时哈希字符串 */
    for (int i = 0; i < dep_count; i++) {
        lv_free((void **) &dep_hashes[i]);
    }

    /* 重新计算最终颜色 */
    if (update_count > 0) {
        proof_navigator_compute_final_color(nav);
    }

    /* 流式事件：依赖验证结果 */
    if (update_count > 0) {
        nav_emit(proof_stream_ctx, STREAM_EVENT_PROOF_DEPENDENCY_CHANGE, "依赖验证完成: %d 个依赖需要更新",
                 update_count);
    }

    return update_count;
}

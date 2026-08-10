/**
 * @file proof_navigator.c
 * @brief ProofNavigator 证明导航
 *
 * @details 拆分子模块（Lv-00 v3.3.0+）。
 * @author Lv-00 Project
 * @version 3.3.0
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

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "stream.h"
#include "stream_context_util.h"
#include "proof_navigator_internal.h"

/* 流式上下文声明 */
/* lv_DEFAULT_MAX_STEPS 已迁移至 config.h */

/* lv_proof_tree_* 函数实现在 proof_tree.c 中，通过链接解析 */

/**
 * @brief 计算证明导航器的最终颜色
 *
 * 遍历所有证明步骤，根据颜色优先级计算最终信任颜色。
 * 颜色优先级：深橙色 > 橙黄色 > 浅橙色 > 黄色 > 蓝色 > 绿色
 */
ProofColor proof_navigator_compute_final_color(ProofNavigator *nav) {
    if (!nav)
        return PROOF_COLOR_GREEN;

    ProofColor final_color = PROOF_COLOR_GREEN;

    for (int i = 0; i < nav->step_count; i++) {
        ProofStep *step = nav->steps[i];
        if (!step)
            continue;

        /* 颜色优先级：深橙色 > 橙黄色 > 浅橙色 > 黄色 > 蓝色 > 绿色 */
        if (step->color == PROOF_COLOR_DARK_ORANGE) {
            final_color = PROOF_COLOR_DARK_ORANGE;
        } else if (step->color == PROOF_COLOR_AMBER && final_color != PROOF_COLOR_DARK_ORANGE) {
            final_color = PROOF_COLOR_AMBER;
        } else if ((step->color == PROOF_COLOR_ORANGE_ORACLE || step->color == PROOF_COLOR_ORANGE_EX_FALSO) &&
                   final_color != PROOF_COLOR_DARK_ORANGE && final_color != PROOF_COLOR_AMBER) {
            /* 如果同时存在 ORACLE 和 EX_FALSO 两种橙色，升级为深橙色 */
            if ((final_color == PROOF_COLOR_ORANGE_ORACLE && step->color == PROOF_COLOR_ORANGE_EX_FALSO) ||
                (final_color == PROOF_COLOR_ORANGE_EX_FALSO && step->color == PROOF_COLOR_ORANGE_ORACLE)) {
                final_color = PROOF_COLOR_DARK_ORANGE;
            } else {
                final_color = step->color;
            }
        } else if (step->color == PROOF_COLOR_YELLOW && final_color == PROOF_COLOR_GREEN) {
            final_color = PROOF_COLOR_YELLOW;
        } else if (step->color >= PROOF_COLOR_BLUE_UNEXPLORED && step->color <= PROOF_COLOR_BLUE_OUT_OF_RANGE &&
                   final_color == PROOF_COLOR_GREEN) {
            final_color = step->color;
        }
    }

    nav->final_color = final_color;

    /* 从约束图节点信任颜色整合 */
    if (nav->construction) {
        ProofColor graph_color = PROOF_COLOR_GREEN;
        for (int i = 0; i < nav->construction->node_count; i++) {
            GeomNode *node = nav->construction->nodes[i];
            if (!node)
                continue;
            /* 将 GeomNode.trust (TrustColor) 转换为 ProofColor 后合并 */
            ProofColor node_color = trust_color_to_proof(node->trust);
            if (node_color != PROOF_COLOR_GREEN) {
                if (graph_color == PROOF_COLOR_GREEN) {
                    graph_color = node_color;
                } else {
                    /* 多颜色叠加 */
                    bool is_lo = (node_color == PROOF_COLOR_ORANGE_ORACLE || node_color == PROOF_COLOR_ORANGE_EX_FALSO);
                    bool is_graph_lo =
                        (graph_color == PROOF_COLOR_ORANGE_ORACLE || graph_color == PROOF_COLOR_ORANGE_EX_FALSO);
                    if ((is_lo && graph_color == PROOF_COLOR_AMBER) ||
                        (is_graph_lo && node_color == PROOF_COLOR_AMBER)) {
                        graph_color = PROOF_COLOR_DARK_ORANGE;
                    } else if ((int) node_color > (int) graph_color) {
                        graph_color = node_color;
                    }
                }
            }
        }
        /* 整合图的信任颜色到最终颜色 */
        if (graph_color != PROOF_COLOR_GREEN) {
            if (final_color == PROOF_COLOR_GREEN) {
                final_color = graph_color;
            } else {
                final_color = proof_color_combine(final_color, graph_color);
            }
        }
    }

    nav->final_color = final_color;

    /* 流式输出：最终颜色计算 */
    nav_emit(proof_stream_ctx, STREAM_EVENT_PROOF_COLOR_UPDATE, "最终颜色计算: %s",
             proof_color_to_string(final_color));

    return final_color;
}

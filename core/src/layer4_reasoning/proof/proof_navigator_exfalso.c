/*
 * @file proof_navigator_exfalso.c
 * @brief Proof navigator module - ex falso / explosion principle
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

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "stream.h"
#include "stream_context_util.h"
#include "proof_navigator_internal.h"

/* ============== 爆炸原理 ============== */

/**
 * @brief 创建爆炸原理（ex falso quodlibet）函数块
 *
 * 在约束图中构造一个特殊的函数块，实现"从矛盾推出任意命题"的逻辑原理。
 * 输入端口接受 bottom 证物，输出端口设置为多态类型以适配任意目标命题。
 *
 * @param graph         约束图指针
 * @param out_block_id  [输出] 新创建的函数块节点 ID
 * @return true 创建成功，false 参数无效或图操作失败
 */
bool proof_create_ex_falso_block(ConstraintGraph *graph, int *out_block_id) {
    if (!graph || !out_block_id)
        return false;

    /* 创建一个特殊的函数块 */
    /* 输入端口：接受 ⊥ 证物 */
    /* 输出端口：输出任意 P 证物（多态类型） */

    /* 创建输入端口 */
    AddNodeResult result = graph_add_port(graph, PORT_INPUT, 0, -1);
    if (result < 0)
        return false;
    int input_port_id = graph->next_node_id - 1;

    /* 创建输出端口 - 标记为多态类型 */
    result = graph_add_port(graph, PORT_OUTPUT, 0, -1);
    if (result < 0) {
        /* 输出端口创建失败，移除已创建的输入端口 */
        graph_remove_node(graph, input_port_id);
        return false;
    }
    int output_port_id = graph->next_node_id - 1;

    /* 标记输出端口为多态 */
    GeomNode *out_port_node = graph_get_node(graph, output_port_id);
    if (out_port_node && out_port_node->type == GEOM_PORT && out_port_node->data.port) {
        out_port_node->data.port->is_polymorphic = true;
    }

    /* 创建函数块 */
    int internal_nodes[] = {input_port_id, output_port_id};
    int input_ports[] = {input_port_id};
    int output_ports[] = {output_port_id};

    result = graph_add_function_block(graph, internal_nodes, 2, input_ports, 1, output_ports, 1);
    if (result != ADD_NODE_OK) {
        /* 函数块创建失败，移除已创建的端口 */
        graph_remove_node(graph, input_port_id);
        graph_remove_node(graph, output_port_id);
        return false;
    }

    *out_block_id = graph->next_node_id - 1;

    /* 标记为爆炸原理块 */
    GeomNode *fb = graph_get_node(graph, *out_block_id);
    if (fb && fb->type == GEOM_FUNCTION_BLOCK) {
        /* 设置特殊标记 - 使用 LIGHT_ORANGE_EXPLOSION 表示爆炸原理 */
        fb->trust = TRUST_LIGHT_ORANGE_EXPLOSION;
        fb->lo_subtype = LO_EXPLOSION;
    }

    /* 流式事件：爆炸原理块创建 */
    if (proof_stream_ctx != NULL) {
        lvStrBuf sb_3 = {0};
        lv_strbuf_printf(&sb_3, "爆炸原理函数块创建: block_id=%d", *out_block_id);
        stream_emit_simple(proof_stream_ctx, STREAM_EVENT_INFO, sb_3.data, 0);
        lv_strbuf_destroy(&sb_3);
    }

    return true;
}

bool proof_apply_ex_falso(ProofNavigator *nav, ConstraintGraph *bottom_proof, Proposition *target_prop) {
    if (!nav || !bottom_proof || !target_prop)
        return false;

    /* 创建爆炸原理步骤 */
    ProofStep *step = proof_step_create(PROOF_STEP_EX_FALSO);
    if (!step)
        return false;

    step->color = PROOF_COLOR_ORANGE_EX_FALSO;

    /* 根据目标命题类型设置输出端口类型（实例化多态） */
    /* 在 bottom_proof 图中查找爆炸原理块，将其输出端口的多态标记解除，
       并设置为目标命题的类型 */
    for (int i = 0; i < bottom_proof->node_count; i++) {
        GeomNode *n = bottom_proof->nodes[i];
        if (n && n->type == GEOM_FUNCTION_BLOCK && n->trust == TRUST_LIGHT_ORANGE_EXPLOSION &&
            n->lo_subtype == LO_EXPLOSION) {
            /* 找到爆炸原理块，更新其输出端口类型 */
            if (n->data.func_block.output_port_ids && n->data.func_block.output_count > 0) {
                int out_port_id = n->data.func_block.output_port_ids[0];
                GeomNode *out_port = graph_get_node(bottom_proof, out_port_id);
                if (out_port && out_port->type == GEOM_PORT && out_port->data.port) {
                    /* 实例化多态：解除多态标记，设置为目标命题类型 */
                    out_port->data.port->is_polymorphic = false;
                    /* 将目标命题的类型信息记录在端口上 */
                    if (target_prop->prop_type) {
                        out_port->data.port->type_region = target_prop->prop_type;
                    }
                }
            }
            break; /* 只处理第一个爆炸原理块 */
        }
    }

    /* 添加到导航器 */
    if (!proof_navigator_add_step(nav, step)) {
        proof_step_destroy(step);
        return false;
    }

    /* 更新目标命题颜色 */
    target_prop->color = PROOF_COLOR_ORANGE_EX_FALSO;

    /* 流式事件：爆炸原理应用 */
    if (proof_stream_ctx != NULL) {
        stream_emit_simple(proof_stream_ctx, STREAM_EVENT_PROOF_STEP_APPLIED, "爆炸原理 (ex falso) 已应用", 0);
    }

    return true;
}

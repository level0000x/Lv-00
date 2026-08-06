/**
 * @file rewrite_hash.c
 * @brief 重写规则：图结构哈希与 WL 哈希
 *
 * 从 rewrite_match.c 拆分的模块之一（拆分清单见 rewrite_binding.c）。
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
#include "lv/rewrite.h"

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "mpz_poly.h"

#include "lv/symbolic_coord.h"

extern uint64_t compute_wl_graph_hash(ConstraintGraph *graph);
/* ---------------------------------------------------------------------------
 * compute_graph_hash  (改进的结构哈希)
 * ------------------------------------------------------------------------- */

/**
 * @brief 计算约束图的结构哈希值（改进版）
 *
 * 基于 FNV-1a 算法计算约束图的完整结构哈希：
 * - 对每个节点，哈希其 ID 和类型
 * - 对 POINT 节点额外哈希其符号坐标的序列化值
 * - 对每个约束，哈希其类型和参与节点 ID 列表
 *
 * @param graph 约束图指针
 * @return 32位结构哈希值
 */
uint32_t compute_graph_hash(ConstraintGraph *graph) {
    uint64_t h = lv_FNV64_OFFSET_BASIS;

    /* 哈希节点类型和 POINT 节点的符号坐标 */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *n = graph->nodes[i];
        h = lv_fnv1a_update(h, &n->id, sizeof(n->id));
        int type_val = (int) n->type;
        h = lv_fnv1a_update(h, &type_val, sizeof(type_val));

        if (n->type == GEOM_POINT && n->coord_count > 0 && n->symbolic_coords) {
            for (int c = 0; c < n->coord_count; c++) {
                if (n->symbolic_coords[c]) {
                    char *ser = symbolic_coord_serialize(n->symbolic_coords[c]);
                    if (ser) {
                        h = lv_fnv1a_update(h, ser, strlen(ser));
                        lv_free((void **) &ser);
                    }
                }
            }
        }
    }

    /* 哈希约束类型及其参与者列表 */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        int type_val = (int) c->type;
        h = lv_fnv1a_update(h, &type_val, sizeof(type_val));
        h = lv_fnv1a_update(h, c->participants, c->participant_count * sizeof(int));
    }

    return (int) h;
}

/**
 * @brief 检测重写循环：判断当前图哈希是否在历史中出现过
 *
 * 计算当前约束图的结构哈希值，与历史哈希记录逐一比对。
 * 若匹配则说明图状态已出现过，形成重写循环。
 *
 * @param graph          当前约束图指针
 * @param history_hashes 历史哈希值数组
 * @param history_count  历史记录数量
 * @return true 表示检测到循环，false 表示未检测到
 */
bool detect_rewrite_loop(ConstraintGraph *graph, const int *history_hashes, int history_count) {
    uint32_t current_hash = compute_graph_hash(graph);
    for (int i = 0; i < history_count; i++) {
        if (history_hashes[i] == current_hash) {
            return true;
        }
    }
    return false;
}

/* ===========================================================================
 * WL 图核哈希（公开接口）
 *
 * 封装内部的 compute_wl_graph_hash 函数，提供公开的 API。
 * 允许外部模块（如 solver、unify）获取图的拓扑哈希用于去重或比较。
 * ===========================================================================
 */

/**
 * @brief 计算图的 Weisfeiler-Lehman 图核哈希值
 *
 * 封装内部的 compute_wl_graph_hash 函数，提供公开的 API。
 * 允许外部模块（如 solver、unify）获取图的拓扑哈希用于去重或比较。
 *
 * @param graph 约束图指针
 * @return 64位 WL 哈希值
 */
uint64_t rewrite_compute_wl_hash(const ConstraintGraph *graph) {
    return compute_wl_graph_hash((ConstraintGraph *) graph);
}

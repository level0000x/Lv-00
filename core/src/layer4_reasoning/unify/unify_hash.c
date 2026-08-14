/**
 * @file unify_hash.c
 * @brief hash-filtered unification
 * @details Split from unify.c
 */

#include "lv/unify.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/geometric_primitives.h"

#include "lv/debug.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/normalization.h"
#include "lv/stream.h"
#include "lv/stream.h"
#include "lv/type_system.h"
#include "lv/lv_strbuf.h"
#include "unify_internal.h"

/* ---------------------------------------------------------------------------
 * Task 1b: 带哈希预过滤的合一
 *
 * 在进行详细的约束匹配之前，先使用 symbolic_coord_hash() 对节点
 * 按坐标哈希分组。只有哈希相同的节点组之间才进行比较，大幅减少
 * 不必要的约束匹配次数。
 * ------------------------------------------------------------------------- */

UnifyStatus unify_construction_with_proposition_hash_filtered(const ConstraintGraph *construction,
                                                              const ConstraintGraph *proposition) {
    /* 合一前执行图规范化遍（设计文档 3.8 节） */
    if (construction) {
        geo_normalize((ConstraintGraph *) construction, true);
    }
    if (proposition) {
        geo_normalize((ConstraintGraph *) proposition, true);
    }
    NormalizationResult *nc = graph_normalize((ConstraintGraph *) construction, true);
    NormalizationResult *np = graph_normalize((ConstraintGraph *) proposition, true);
    if (!nc || !np) {
        if (nc)
            normalization_result_destroy(nc);
        if (np)
            normalization_result_destroy(np);
        return UNIFY_STATUS_FAILED;
    }
    if (nc->merged_count != np->merged_count) {
        normalization_result_destroy(nc);
        normalization_result_destroy(np);
        return UNIFY_STATUS_COORD_MISMATCH;
    }

    /* 计算命题图中所有节点的坐标哈希 */
    uint64_t *prop_hashes = lv_calloc((size_t) proposition->node_count, sizeof(uint64_t));
    if (!prop_hashes) {
        normalization_result_destroy(nc);
        normalization_result_destroy(np);
        return UNIFY_STATUS_FAILED;
    }
    for (int i = 0; i < proposition->node_count; i++) {
        prop_hashes[i] = compute_node_coord_hash(proposition->nodes[i]);
    }

    /* 计算构造图中所有节点的坐标哈希 */
    uint64_t *con_hashes = lv_calloc((size_t) construction->node_count, sizeof(uint64_t));
    if (!con_hashes) {
        lv_free((void **) &prop_hashes);
        normalization_result_destroy(nc);
        normalization_result_destroy(np);
        return UNIFY_STATUS_FAILED;
    }
    for (int i = 0; i < construction->node_count; i++) {
        con_hashes[i] = compute_node_coord_hash(construction->nodes[i]);
    }

    /* 防止多个命题端口匹配到同一个构造端口 */
    bool *used_construction_ports = lv_calloc((size_t) construction->node_count, sizeof(bool));
    if (!used_construction_ports && construction->node_count > 0) {
        lv_free((void **) &prop_hashes);
        lv_free((void **) &con_hashes);
        normalization_result_destroy(nc);
        normalization_result_destroy(np);
        return UNIFY_STATUS_FAILED;
    }

    /* 创建类型系统，用于端口类型等价检查 */
    TypeSystem *ts = type_system_create();

    /* 端口类型匹配（使用哈希预过滤：只比较相同哈希组的端口） */
    for (int i = 0; i < proposition->node_count; i++) {
        GeomNode *pn = proposition->nodes[i];
        if (pn->type != GEOM_PORT)
            continue;
        Port *pp = pn->data.port;
        bool found_match = false;
        for (int j = 0; j < construction->node_count; j++) {
            GeomNode *cn = construction->nodes[j];
            if (cn->type != GEOM_PORT)
                continue;

            /* 跳过已被其他命题端口匹配的构造端口，防止多对一 */
            if (used_construction_ports[j])
                continue;

            Port *cp = cn->data.port;

            /* 哈希预过滤：使用端口类型哈希进行快速排除。
             * 端口节点的 coord_count=0，所以我们哈希端口属性
             *（类型 + namespace_depth）而不是坐标。 */
            {
                uint64_t p_port_hash = ((uint64_t) pp->type << 32) | ((uint64_t) pp->namespace_depth << 16);
                uint64_t c_port_hash = ((uint64_t) cp->type << 32) | ((uint64_t) cp->namespace_depth << 16);
                if (p_port_hash != c_port_hash)
                    continue;
            }
            if (pp->type != cp->type)
                continue;
            if (pp->namespace_depth != cp->namespace_depth)
                continue;
            if (pp->parent_block_id != cp->parent_block_id)
                continue;
            if (pp->is_formal_param != cp->is_formal_param)
                continue;

            /* 类型等价检查（TypeSystem） */
            if (pp->type_region && cp->type_region && ts) {
                TypeEquivResult equiv = type_check_equivalence(ts, pp->type_region, cp->type_region, false);
                if (equiv == TYPE_EQUIV_NOT_EQUIV)
                    continue;
            }

            /* 匹配成功，标记该构造端口为已使用 */
            used_construction_ports[j] = true;
            found_match = true;
            break;
        }
        if (!found_match) {
            lv_free((void **) &used_construction_ports);
            if (ts)
                type_system_destroy(ts);
            lv_free((void **) &prop_hashes);
            lv_free((void **) &con_hashes);
            normalization_result_destroy(nc);
            normalization_result_destroy(np);
            return UNIFY_STATUS_PORT_TYPE_MISMATCH;
        }
    }
    lv_free((void **) &used_construction_ports);
    if (ts)
        type_system_destroy(ts);

    /* 约束匹配：使用哈希预过滤加速 */
    for (int i = 0; i < proposition->constraint_count; i++) {
        Constraint *pc = proposition->constraints[i];
        bool found_match = false;

        /* 计算此命题约束所有参与者的坐标哈希签名 */
        uint64_t p_sig = 0;
        for (int k = 0; k < pc->participant_count; k++) {
            GeomNode *p_node = graph_get_node(proposition, pc->participants[k]);
            if (p_node) {
                p_sig ^= compute_node_coord_hash(p_node);
            }
        }

        for (int j = 0; j < construction->constraint_count; j++) {
            Constraint *cc = construction->constraints[j];
            if (pc->type != cc->type)
                continue;
            if (pc->participant_count != cc->participant_count)
                continue;

            /* 哈希预过滤：计算构造约束的哈希签名，
             * 如果签名不同则跳过详细比较 */
            uint64_t c_sig = 0;
            for (int k = 0; k < cc->participant_count; k++) {
                GeomNode *c_node = graph_get_node(construction, cc->participants[k]);
                if (c_node) {
                    c_sig ^= compute_node_coord_hash(c_node);
                }
            }
            if (p_sig != c_sig)
                continue;

            /* 详细匹配：检查参与者 ID */
            bool same = true;
            for (int k = 0; k < pc->participant_count; k++) {
                if (pc->participants[k] != cc->participants[k]) {
                    same = false;
                    break;
                }
            }
            if (same) {
                found_match = true;
                break;
            }
        }
        if (!found_match) {
            lv_free((void **) &prop_hashes);
            lv_free((void **) &con_hashes);
            normalization_result_destroy(nc);
            normalization_result_destroy(np);
            return UNIFY_STATUS_CONSTRAINT_MISMATCH;
        }
    }

    lv_free((void **) &prop_hashes);
    lv_free((void **) &con_hashes);
    normalization_result_destroy(nc);
    normalization_result_destroy(np);
    return UNIFY_STATUS_OK;
}

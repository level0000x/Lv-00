/**
 * @file unify_fine.c
 * @brief fine-grained matching functions
 * @details Split from unify.c
 */

#include "unify.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/geometric_primitives.h"
#include "lv/proof.h"

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "normalization.h"
#include "stream.h"
#include "stream_context_util.h"
#include "type_system.h"
#include "lv/lv_strbuf.h"
#include "unify_internal.h"

/* ===========================================================================
 * 精细化匹配函数 —— 将合一流程中的各阶段抽取为可独立调用的函数
 *
 * 这些函数将完整的合一检查分解为端口匹配、约束匹配、坐标判等三个
 * 独立阶段，允许外部调用者进行更精细的控制和调试。
 *
 * 所有函数均集成流式事件输出。
 * ===========================================================================
 */

/**
 * @brief 单独执行端口类型匹配
 *
 * 遍历命题图的所有端口节点，在构造图中查找类型、命名空间深度、
 * 类型区域都等价的端口。一个构造端口最多匹配一个命题端口。
 *
 * 流式输出: 匹配每对端口时发出 PROOF_UNIFY 事件，
 * 包含端口类型和命名空间深度的 JSON 详细信息。
 */
int unify_match_ports(const ConstraintGraph *construction, const ConstraintGraph *proposition, int *out_port_bindings,
                      int max_bindings) {
    if (!construction || !proposition)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "unify_match_ports: NULL construction or proposition");

    if (unify_stream_ctx) {
        stream_emit_simple(unify_stream_ctx, STREAM_EVENT_PROOF_UNIFY, "精细端口匹配开始", 0);
    }

    /* 统计命题端口节点数量 */
    int prop_port_count = 0;
    for (int i = 0; i < proposition->node_count; i++) {
        if (proposition->nodes[i]->type == GEOM_PORT)
            prop_port_count++;
    }

    /* 跟踪已匹配的构造端口 */
    bool *used = lv_calloc((size_t) construction->node_count, sizeof(bool));
    if (!used && construction->node_count > 0)
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "unify_match_ports: calloc used failed");

    /* 创建 TypeSystem 用于端口类型等价检查 */
    TypeSystem *ts = type_system_create();

    int match_count = 0;

    for (int i = 0; i < proposition->node_count; i++) {
        GeomNode *pn = proposition->nodes[i];
        if (pn->type != GEOM_PORT)
            continue;
        Port *pp = pn->data.port;
        if (!pp)
            continue;

        bool found = false;
        for (int j = 0; j < construction->node_count; j++) {
            GeomNode *cn = construction->nodes[j];
            if (cn->type != GEOM_PORT)
                continue;
            if (used[j])
                continue;
            Port *cp = cn->data.port;
            if (!cp)
                continue;

            /* 类型和命名空间深度匹配 */
            if (pp->type != cp->type)
                continue;
            if (pp->namespace_depth != cp->namespace_depth)
                continue;
            if (pp->parent_block_id != cp->parent_block_id)
                continue;
            if (pp->is_formal_param != cp->is_formal_param)
                continue;

            /* TypeSystem 等价检查 */
            if (pp->type_region && cp->type_region && ts) {
                TypeEquivResult equiv = type_check_equivalence(ts, pp->type_region, cp->type_region, false);
                if (equiv == TYPE_EQUIV_NOT_EQUIV)
                    continue;
            }

            /* 找到匹配 */
            used[j] = true;
            found = true;

            if (out_port_bindings && match_count < max_bindings) {
                out_port_bindings[match_count * 2] = pn->id;
                out_port_bindings[match_count * 2 + 1] = cn->id;
            }
            match_count++;

            if (unify_stream_ctx) {
                StreamEvent ev;
                memset(&ev, 0, sizeof(ev));
                ev.type = STREAM_EVENT_PROOF_UNIFY;
                ev.timestamp_ms = stream_timestamp_ms();
                ev.node_id = cn->id;
                ev.step_number = match_count;
                ev.var_id = pn->id;
                ev.description = "端口匹配成功";
                lvStrBuf sb = {0};
                lv_strbuf_printf(&sb,
                         "{\"prop_port_id\":%d,\"const_port_id\":%d,"
                         "\"port_type\":%d,\"namespace_depth\":%d}",
                         pn->id, cn->id, (int) pp->type, pp->namespace_depth);
                ev.detail_json = sb.data;
                stream_emit(unify_stream_ctx, &ev);
                lv_strbuf_destroy(&sb);
            }
            break;
        }

        if (!found) {
            /* 此命题端口没有匹配的 construction 端口 */
            lv_free((void **) &used);
            if (ts)
                type_system_destroy(ts);
            if (unify_stream_ctx) {
                lvStrBuf sb_2 = {0};
                lv_strbuf_printf(&sb_2, "端口匹配失败: 命题端口 %d (type=%d) 无对应构造端口", pn->id,
                         pp ? (int) pp->type : -1);
                stream_emit_simple(unify_stream_ctx, STREAM_EVENT_PROOF_UNIFY, sb_2.data, match_count);
                lv_strbuf_destroy(&sb_2);
            }
            return -1;
        }
    }

    lv_free((void **) &used);
    if (ts)
        type_system_destroy(ts);

    if (unify_stream_ctx) {
        lvStrBuf sb_3 = {0};
        lv_strbuf_printf(&sb_3, "精细端口匹配完成: %d 对端口匹配成功", match_count);
        stream_emit_simple(unify_stream_ctx, STREAM_EVENT_PROOF_UNIFY, sb_3.data, match_count);
        lv_strbuf_destroy(&sb_3);
    }

    return match_count;
}

/**
 * @brief 单独执行约束匹配
 *
 * 遍历命题图的所有约束，在构造图中查找类型一致且参与者数量
 * 一致的对应约束。仅检查约束拓扑结构，不涉及坐标判等。
 *
 * 流式输出: 匹配每对约束时发出 PROOF_UNIFY 事件。
 */
int unify_match_constraints(const ConstraintGraph *construction, const ConstraintGraph *proposition,
                            int *out_constraint_bindings) {
    if (!construction || !proposition)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "unify_match_constraints: NULL construction or proposition");

    if (unify_stream_ctx) {
        stream_emit_simple(unify_stream_ctx, STREAM_EVENT_PROOF_UNIFY, "精细约束匹配开始", 0);
    }

    int match_count = 0;

    /* 跟踪已匹配的构造约束 */
    bool *used = lv_calloc((size_t) construction->constraint_count, sizeof(bool));
    if (!used && construction->constraint_count > 0)
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "unify_match_constraints: calloc used failed");

    for (int i = 0; i < proposition->constraint_count; i++) {
        const Constraint *pc = proposition->constraints[i];
        if (!pc)
            continue;

        bool found = false;
        for (int j = 0; j < construction->constraint_count; j++) {
            const Constraint *cc = construction->constraints[j];
            if (!cc || used[j])
                continue;

            /* 约束类型必须匹配 */
            if (pc->type != cc->type)
                continue;

            /* 参与者数量必须匹配 */
            if (pc->participant_count != cc->participant_count)
                continue;

            /* 检查参与者 ID */
            bool same = true;
            for (int k = 0; k < pc->participant_count; k++) {
                if (pc->participants[k] != cc->participants[k]) {
                    same = false;
                    break;
                }
            }

            if (same) {
                used[j] = true;
                found = true;

                if (out_constraint_bindings) {
                    out_constraint_bindings[match_count * 2] = pc->id;
                    out_constraint_bindings[match_count * 2 + 1] = cc->id;
                }
                match_count++;

                if (unify_stream_ctx) {
                    StreamEvent ev;
                    memset(&ev, 0, sizeof(ev));
                    ev.type = STREAM_EVENT_PROOF_UNIFY;
                    ev.timestamp_ms = stream_timestamp_ms();
                    ev.constraint_id = cc->id;
                    ev.step_number = match_count;
                    ev.description = "约束匹配成功";
                    lvStrBuf sb_4 = {0};
                    lv_strbuf_printf(&sb_4,
                             "{\"prop_constraint_id\":%d,\"const_constraint_id\":%d,"
                             "\"type\":%d,\"participants\":%d}",
                             pc->id, cc->id, (int) pc->type, pc->participant_count);
                    ev.detail_json = sb_4.data;
                    stream_emit(unify_stream_ctx, &ev);
                    lv_strbuf_destroy(&sb_4);
                }
                break;
            }
        }

        if (!found) {
            lv_free((void **) &used);
            if (unify_stream_ctx) {
                lvStrBuf sb_5 = {0};
                lv_strbuf_printf(&sb_5, "约束匹配失败: 命题约束 %d (type=%d) 无对应构造约束", pc->id,
                         (int) pc->type);
                stream_emit_simple(unify_stream_ctx, STREAM_EVENT_PROOF_UNIFY, sb_5.data, match_count);
                lv_strbuf_destroy(&sb_5);
            }
            return -1;
        }
    }

    lv_free((void **) &used); /* 使用 lv_calloc/lv_free 统一内存管理 */

    if (unify_stream_ctx) {
        lvStrBuf sb_6 = {0};
        lv_strbuf_printf(&sb_6, "精细约束匹配完成: %d 对约束匹配成功", match_count);
        stream_emit_simple(unify_stream_ctx, STREAM_EVENT_PROOF_UNIFY, sb_6.data, match_count);
        lv_strbuf_destroy(&sb_6);
    }

    return match_count;
}

/**
 * @brief 单独执行符号坐标判等
 *
 * 使用 symbolic_coord_compare 比较两个坐标，返回比较结果。
 * 0 表示完全相等，非 0 表示不相等。
 * 此函数也为未来扩展坐标等价的更多语义（如归一化后的等价、
 * 模变换后的等价等）预留了扩展点。
 *
 * 流式输出: 仅在结果不相等时发出 PROOF_UNIFY 事件（含详细差异信息）。
 */
int unify_match_coords(const SymbolicCoord *c1, const SymbolicCoord *c2) {
    if (!c1 && !c2)
        return 0; /* 两者均为 NULL 视为相等 */
    if (!c1 || !c2)
        return -1; /* 仅一个为 NULL：不相等 */

    int result = symbolic_coord_compare(c1, c2);

    if (result != 0 && unify_stream_ctx) {
        char *s1 = symbolic_coord_serialize(c1);
        char *s2 = symbolic_coord_serialize(c2);
        lvStrBuf sb_7 = {0};
        lv_strbuf_printf(&sb_7, "坐标不相等: \"%s\" vs \"%s\"", s1 ? s1 : "(null)", s2 ? s2 : "(null)");
        stream_emit_simple(unify_stream_ctx, STREAM_EVENT_PROOF_UNIFY, sb_7.data, 0);
        lv_free((void **) &s1);
        lv_free((void **) &s2);
        lv_strbuf_destroy(&sb_7);
    }

    return result;
}

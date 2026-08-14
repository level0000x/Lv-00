/**
 * @file unify_report.c
 * @brief detailed failure reporting
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
 * 不匹配位置的具体报告
 * ------------------------------------------------------------------------- */

void unify_failure_info_destroy(UnifyFailureInfo *info) {
    if (info) {
        lv_free((void **) &info->description);
    }
}

/**
 * @brief 初始化失败信息结构体
 *
 * @param info 失败信息结构体指针
 */
static void failure_info_init(UnifyFailureInfo *info) {
    if (info) {
        info->status = UNIFY_STATUS_OK;
        info->failed_constraint_id = -1;
        info->failed_node_id = -1;
        info->failed_port_index = -1;
        info->description = NULL;
    }
}

/**
 * @brief 设置失败信息结构体的值
 *
 * @param info           失败信息结构体指针
 * @param status         合一状态码
 * @param constraint_id  失败的约束 ID
 * @param node_id        失败的节点 ID
 * @param port_index     失败的端口索引
 * @param fmt            格式字符串
 * @param ...            可变参数
 */
static void failure_info_set(UnifyFailureInfo *info, UnifyStatus status, int constraint_id, int node_id, int port_index,
                             const char *fmt, ...) {
    if (!info)
        return;
    info->status = status;
    info->failed_constraint_id = constraint_id;
    info->failed_node_id = node_id;
    info->failed_port_index = port_index;
    if (info->description) {
        lv_free((void **) &info->description);
    }
    if (fmt) {
        /* 用 lvStrBuf 一次性格式化，替代两遍 vsnprintf */
        lvStrBuf sb = {0};
        va_list args;
        va_start(args, fmt);
        lv_strbuf_vprintf(&sb, fmt, args);
        va_end(args);
        if (sb.len > 0)
            info->description = lv_strbuf_to_string(&sb);
        else
            lv_strbuf_destroy(&sb);
    }
}

UnifyStatus unify_construction_with_proposition_detailed(const ConstraintGraph *construction,
                                                         const ConstraintGraph *pattern,
                                                         UnifyFailureInfo *out_failure) {
    if (out_failure)
        failure_info_init(out_failure);

    if (unify_stream_ctx) {
        stream_emit_simple(unify_stream_ctx, STREAM_EVENT_PROOF_UNIFY, "详细合一检查开始", 0);
    }

    if (!construction || !pattern) {
        failure_info_set(out_failure, UNIFY_STATUS_FAILED, -1, -1, -1, "NULL graph argument");
        if (unify_stream_ctx) {
            stream_emit_simple(unify_stream_ctx, STREAM_EVENT_PROOF_UNIFY, "详细合一检查失败：图参数为空", 0);
        }
        return UNIFY_STATUS_FAILED;
    }

    NormalizationResult *nc = graph_normalize(construction, true);
    NormalizationResult *np = graph_normalize(pattern, true);
    if (!nc || !np) {
        if (nc)
            normalization_result_destroy(nc);
        if (np)
            normalization_result_destroy(np);
        failure_info_set(out_failure, UNIFY_STATUS_FAILED, -1, -1, -1, "Normalization failed");
        if (unify_stream_ctx) {
            stream_emit_simple(unify_stream_ctx, STREAM_EVENT_PROOF_UNIFY, "详细合一检查失败：归一化失败", 0);
        }
        return UNIFY_STATUS_FAILED;
    }

    if (nc->merged_count != np->merged_count) {
        normalization_result_destroy(nc);
        normalization_result_destroy(np);
        failure_info_set(out_failure, UNIFY_STATUS_COORD_MISMATCH, -1, -1, -1,
                         "Merged node count mismatch: construction has %d, pattern has %d", nc->merged_count,
                         np->merged_count);
        if (unify_stream_ctx) {
            stream_emit_simple(unify_stream_ctx, STREAM_EVENT_PROOF_UNIFY, "详细合一检查失败：合并节点数量不匹配", 0);
        }
        return UNIFY_STATUS_COORD_MISMATCH;
    }

    /* 跟踪已匹配的 construction 端口 */
    int construction_port_count = 0;
    for (int j = 0; j < construction->node_count; j++) {
        if (construction->nodes[j]->type == GEOM_PORT)
            construction_port_count++;
    }
    int *used_construction_ports =
        lv_calloc(construction_port_count > 0 ? (size_t) construction_port_count : 1, sizeof(int));

    TypeSystem *ts = type_system_create();

    /* 端口类型匹配（带详细失败报告） */
    int prop_port_index = 0;
    for (int i = 0; i < pattern->node_count; i++) {
        GeomNode *pn = pattern->nodes[i];
        if (pn->type != GEOM_PORT)
            continue;
        Port *pp = pn->data.port;
        bool found_match = false;
        int cidx = 0;
        for (int j = 0; j < construction->node_count; j++) {
            GeomNode *cn = construction->nodes[j];
            if (cn->type != GEOM_PORT)
                continue;
            if (used_construction_ports[cidx]) {
                cidx++;
                continue;
            }
            Port *cp = cn->data.port;
            if (pp->type != cp->type) {
                cidx++;
                continue;
            }
            if (pp->namespace_depth != cp->namespace_depth) {
                cidx++;
                continue;
            }
            if (pp->parent_block_id != cp->parent_block_id) {
                cidx++;
                continue;
            }
            if (pp->is_formal_param != cp->is_formal_param) {
                cidx++;
                continue;
            }
            if (pp->type_region && cp->type_region && ts) {
                TypeEquivResult equiv = type_check_equivalence(ts, pp->type_region, cp->type_region, false);
                if (equiv == TYPE_EQUIV_NOT_EQUIV) {
                    cidx++;
                    continue;
                }
            }
            used_construction_ports[cidx] = 1;
            found_match = true;
            break;
        }
        if (!found_match) {
            lv_free((void **) &used_construction_ports);
            if (ts)
                type_system_destroy(ts);
            normalization_result_destroy(nc);
            normalization_result_destroy(np);
            failure_info_set(out_failure, UNIFY_STATUS_PORT_TYPE_MISMATCH, -1, pn->id, prop_port_index,
                             "No matching port in construction for pattern port %d "
                             "(type=%d, namespace_depth=%d)",
                             pn->id, (int) pp->type, pp->namespace_depth);
            if (unify_stream_ctx) {
                stream_emit_simple(unify_stream_ctx, STREAM_EVENT_PROOF_UNIFY, "详细合一检查失败：端口类型不匹配", 0);
            }
            return UNIFY_STATUS_PORT_TYPE_MISMATCH;
        }
        prop_port_index++;
    }
    lv_free((void **) &used_construction_ports);
    if (ts)
        type_system_destroy(ts);

    /* 约束匹配（带详细失败报告） */
    for (int i = 0; i < pattern->constraint_count; i++) {
        Constraint *pc = pattern->constraints[i];
        bool found_match = false;
        for (int j = 0; j < construction->constraint_count; j++) {
            Constraint *cc = construction->constraints[j];
            if (pc->type != cc->type)
                continue;
            if (pc->participant_count != cc->participant_count)
                continue;
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
            normalization_result_destroy(nc);
            normalization_result_destroy(np);
            failure_info_set(out_failure, UNIFY_STATUS_CONSTRAINT_MISMATCH, pc->id, -1, -1,
                             "No matching constraint in construction for pattern "
                             "constraint %d (type=%d, participants=%d)",
                             pc->id, (int) pc->type, pc->participant_count);
            if (unify_stream_ctx) {
                stream_emit_simple(unify_stream_ctx, STREAM_EVENT_PROOF_UNIFY, "详细合一检查失败：约束不匹配", 0);
            }
            return UNIFY_STATUS_CONSTRAINT_MISMATCH;
        }
    }

    normalization_result_destroy(nc);
    normalization_result_destroy(np);
    if (unify_stream_ctx) {
        stream_emit_simple(unify_stream_ctx, STREAM_EVENT_PROOF_UNIFY, "详细合一检查成功", 0);
    }
    return UNIFY_STATUS_OK;
}

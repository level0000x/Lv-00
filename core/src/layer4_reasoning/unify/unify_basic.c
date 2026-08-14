/**
 * @file unify_basic.c
 * @brief basic and coordinate-aware unification
 * @details Split from unify.c
 */

#include "lv/unify.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/geometric_primitives.h"
#include "lv/proof.h"

#include "lv/debug.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/normalization.h"
#include "lv/stream.h"
#include "lv/stream.h"
#include "lv/type_system.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_xmacro.h"
#include "unify_internal.h"

/* ---------------------------------------------------------------------------
 * 合一失败状态中文文案（统一来源）
 * ------------------------------------------------------------------------- */

/** @brief UnifyStatus → 中文原因 查找表（按枚举值升序，文案语义取更完整版本） */
#define LV_UNIFY_STATUS_REASON_X(x) \
    x(UNIFY_STATUS_PORT_TYPE_MISMATCH, "端口类型不匹配") \
    x(UNIFY_STATUS_CONSTRAINT_MISMATCH, "约束类型不匹配") \
    x(UNIFY_STATUS_COORD_MISMATCH, "符号坐标不匹配") \
    x(UNIFY_STATUS_STRUCTURE_MISMATCH, "图结构不匹配") \
    x(UNIFY_STATUS_SCOPE_MISMATCH, "作用域不匹配") \
    x(UNIFY_STATUS_FAILED, "合一检查系统内部错误")
static const char *const s_unify_status_reasons_zh[] = {
    lv_XMACRO_TO_NAME_ARRAY(LV_UNIFY_STATUS_REASON_X)
};
#undef LV_UNIFY_STATUS_REASON_X

const char *unify_status_reason_zh(UnifyStatus status) {
    if ((int) status >= 0 && (size_t) status < lv_ARRAY_SIZE(s_unify_status_reasons_zh) &&
        s_unify_status_reasons_zh[(int) status]) {
        return s_unify_status_reasons_zh[(int) status];
    }
    return "未知错误";
}

/* ---------------------------------------------------------------------------
 * 基础合一
 * ------------------------------------------------------------------------- */

UnifyStatus unify_construction_with_proposition(const ConstraintGraph *construction,
                                                const ConstraintGraph *proposition) {
    /* 合一前执行图规范化遍（设计文档 3.8 节） */
    if (construction) {
        geo_normalize((ConstraintGraph *) construction, true);
    }
    if (proposition) {
        geo_normalize((ConstraintGraph *) proposition, true);
    }
    if (unify_stream_ctx) {
        stream_emit_simple(unify_stream_ctx, STREAM_EVENT_PROOF_UNIFY, "合一检查开始", 0);
    }
    NormalizationResult *nc = graph_normalize(construction, true);
    NormalizationResult *np = graph_normalize(proposition, true);
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
    /* 跟踪已匹配的构造端口，防止多对一匹配 */
    int construction_port_count = 0;
    for (int j = 0; j < construction->node_count; j++) {
        if (construction->nodes[j]->type == GEOM_PORT)
            construction_port_count++;
    }
    /* 【安全性修复】避免 lv_calloc(0, sizeof(int)) 的实现定义行为。
     * C标准规定 calloc(0, N) 可能返回 NULL 或一个不可解引用的非NULL指针。
     * 当 construction_port_count == 0 时：
     *   - 若返回 NULL：used_construction_ports 为 NULL，后续依赖其非NULL的代码存在隐患
     *   - 若返回非NULL：该指针在函数退出时未被 lv_free()，造成内存泄漏
     * 修复方式：当 port_count 为 0 时分配最小单元（1个元素），确保行为统一且无泄漏。
     * 该额外分配的1个元素在后续循环中不会被使用（循环条件跳过无端口的图）。 */
    int alloc_count = construction_port_count > 0 ? construction_port_count : 1;
    int *used_construction_ports = lv_calloc(alloc_count, sizeof(int));
    if (!used_construction_ports) {
        normalization_result_destroy(nc);
        normalization_result_destroy(np);
        return UNIFY_STATUS_FAILED;
    }

    /* 在循环外创建 TypeSystem 以提高性能 */
    TypeSystem *ts = type_system_create();

    if (!match_ports(construction, proposition, used_construction_ports, ts)) {
        lv_free((void **) &used_construction_ports);
        if (ts)
            type_system_destroy(ts);
        normalization_result_destroy(nc);
        normalization_result_destroy(np);
        if (unify_stream_ctx) {
            stream_emit_simple(unify_stream_ctx, STREAM_EVENT_PROOF_UNIFY, "合一检查失败：端口类型不匹配", 0);
        }
        return UNIFY_STATUS_PORT_TYPE_MISMATCH;
    }
    lv_free((void **) &used_construction_ports);
    if (ts)
        type_system_destroy(ts);
    for (int i = 0; i < proposition->constraint_count; i++) {
        Constraint *pc = proposition->constraints[i];
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
            if (unify_stream_ctx) {
                stream_emit_simple(unify_stream_ctx, STREAM_EVENT_PROOF_UNIFY, "合一检查失败：约束不匹配", 0);
            }
            return UNIFY_STATUS_CONSTRAINT_MISMATCH;
        }
    }
    normalization_result_destroy(nc);
    normalization_result_destroy(np);
    if (unify_stream_ctx) {
        stream_emit_simple(unify_stream_ctx, STREAM_EVENT_PROOF_UNIFY, "合一检查成功", 0);
    }
    return UNIFY_STATUS_OK;
}

/* ---------------------------------------------------------------------------
 * Task 1a: 带坐标级别相等检查的合一
 *
 * 在约束匹配阶段，除了检查约束类型和参与者 ID 之外，
 * 还验证对应参与者的符号坐标是否相等。
 * 这确保了深层子图同构不仅匹配拓扑结构，还匹配几何语义。
 * ------------------------------------------------------------------------- */

UnifyStatus unify_construction_with_proposition_coord(const ConstraintGraph *construction,
                                                      const ConstraintGraph *proposition) {
    /*
     * 执行带坐标级判等的合一检查。流程分为四个阶段：
     *
     * 【阶段A - 归一化】对构造图和命题图分别进行归一化（含代数化简），
     *   比较合并节点数。若数量不等，直接返回 COORD_MISMATCH。
     *
     * 【阶段B - 端口类型匹配】遍历命题图的所有端口节点，在构造图中
     *   查找类型、命名空间深度、类型区域均匹配的端口。每个构造端口
     *   最多匹配一个命题端口（通过 used_construction_ports 数组防多对一）。
     *
     * 【阶段C - 约束匹配 + 坐标判等】在归一化约束匹配的基础上，
     *   对每一对匹配的约束进一步验证所有参与者的符号坐标是否相等。
     *   坐标检查仅对 GEOM_POINT 类型且有坐标的节点生效；
     *   非 POINT 或无坐标的节点视为自动通过。
     *
     * 【阶段D - 结果返回】所有端口和约束均匹配成功返回 OK，
     *   否则返回对应的错误状态码。
     */

    NormalizationResult *nc = graph_normalize(construction, true);
    NormalizationResult *np = graph_normalize(proposition, true);
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

    /* 跟踪已匹配的构造端口（防止多对一匹配） */
    int construction_port_count = 0;
    for (int j = 0; j < construction->node_count; j++) {
        if (construction->nodes[j]->type == GEOM_PORT)
            construction_port_count++;
    }
    int *used_construction_ports = lv_calloc(construction_port_count > 0 ? construction_port_count : 1, sizeof(int));

    /* 创建 TypeSystem 用于端口类型等价检查 */
    TypeSystem *ts = type_system_create();

    /* 阶段B：端口类型匹配 —— 调用公共 match_ports() 辅助函数 */
    if (!match_ports(construction, proposition, used_construction_ports, ts)) {
        lv_free((void **) &used_construction_ports);
        if (ts)
            type_system_destroy(ts);
        normalization_result_destroy(nc);
        normalization_result_destroy(np);
        return UNIFY_STATUS_PORT_TYPE_MISMATCH;
    }
    lv_free((void **) &used_construction_ports);
    if (ts)
        type_system_destroy(ts);

    /* 阶段C：约束匹配 + 坐标级别判等
     * 在归一化约束匹配成功后，验证所有参与节点的符号坐标相等 */
    for (int i = 0; i < proposition->constraint_count; i++) {
        Constraint *pc = proposition->constraints[i];
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
            if (!same)
                continue;

            /* 坐标级别相等检查：验证对应参与者的符号坐标 */
            bool coords_ok = true;
            for (int k = 0; k < pc->participant_count; k++) {
                GeomNode *p_node = graph_get_node(proposition, pc->participants[k]);
                GeomNode *c_node = graph_get_node(construction, cc->participants[k]);
                if (!nodes_coords_equal(p_node, c_node)) {
                    /* 如果两个节点都不是 POINT 类型或都没有坐标，
                     * 则跳过坐标检查（不视为不匹配） */
                    if (p_node && c_node && p_node->type == GEOM_POINT && c_node->type == GEOM_POINT &&
                        p_node->coord_count > 0 && c_node->coord_count > 0) {
                        coords_ok = false;
                        break;
                    }
                }
            }
            if (!coords_ok)
                continue;

            found_match = true;
            break;
        }
        if (!found_match) {
            normalization_result_destroy(nc);
            normalization_result_destroy(np);
            return UNIFY_STATUS_COORD_MISMATCH;
        }
    }

    normalization_result_destroy(nc);
    normalization_result_destroy(np);
    return UNIFY_STATUS_OK;
}

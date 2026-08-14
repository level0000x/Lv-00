/**
 * @file debug_normalize_assert.c
 * @brief normalization invariant assertions
 * @details Split from debug.c
 */

#include "lv/lv_file.h"
#include "lv/lv_platform.h"
#include "lv/lv_thread.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "lv/engine.h"
#include "lv/lv_json.h"

#include "lv/context.h"
#include "lv/debug.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/stream.h"
#include "lv/lv_xmacro.h"
#include "lv/lv_strbuf.h"
#include "layer2_resource/debug_internal.h"

/* ------------------------------------------------------------------ */
/*  debug_assert_normalization_invariants                              */
/* ------------------------------------------------------------------ */

/**
 * @brief 对引擎的约束图断言归一化不变量。
 *
 * 根据 design_v2.9.md 第3.6节：
 * 1. 同一作用域内不存在未合并的同坐标 POINT 节点
 * 2. 不存在未合并的同端点 LINE_SEGMENT 节点
 * 3. 所有约束的参与者引用有效节点
 * 4. 约束参与者列表按 ID 排序（稳定化）
 *
 * @param engine 引擎实例
 * @param ctx    调试上下文
 * @return 违规数量（0 = 全部通过）
 */
int debug_assert_normalization_invariants(const lvEngine *engine, DebugContext *ctx) {
    if (!ctx || !engine || !engine->main_graph)
        return 0;

    int violations = 0;
    ConstraintGraph *graph = engine->main_graph;

    /* 不变量 1：同一作用域内不存在未合并的同坐标 POINT 节点 */ /* [修复] 英文注释改为中文 */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *a = graph->nodes[i];
        if (a->type != GEOM_POINT || a->coord_count < 2)
            continue;
        for (int j = i + 1; j < graph->node_count; j++) {
            GeomNode *b = graph->nodes[j];
            if (b->type != GEOM_POINT || b->coord_count < 2)
                continue;
            /* 同一作用域检查 */
            if (a->namespace_depth != b->namespace_depth)
                continue;
            if (a->parent_block_id != b->parent_block_id)
                continue;
            /* 坐标相等性检查 */ /* [修复] 英文注释改为中文 */
            bool same = true;
            int min_coords = a->coord_count < b->coord_count ? a->coord_count : b->coord_count;
            for (int k = 0; k < min_coords && same; k++) {
                if (symbolic_coord_compare(a->symbolic_coords[k], b->symbolic_coords[k]) != 0) {
                    same = false;
                }
            }
            if (same) {
                debug_log(LOG_LEVEL_ERROR, "normalization",
                          "Invariant violation: nodes %d and %d have same coords "
                          "but were not merged",
                          a->id, b->id);
                violations++;
            }
        }
    }

    /* 不变量 2：不存在未合并的同端点 LINE_SEGMENT 节点 */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *a = graph->nodes[i];
        if (a->type != GEOM_LINE_SEGMENT || a->coord_count < 2)
            continue;
        for (int j = i + 1; j < graph->node_count; j++) {
            GeomNode *b = graph->nodes[j];
            if (b->type != GEOM_LINE_SEGMENT || b->coord_count < 2)
                continue;
            bool fwd = (symbolic_coord_compare(a->symbolic_coords[0], b->symbolic_coords[0]) == 0 &&
                        symbolic_coord_compare(a->symbolic_coords[1], b->symbolic_coords[1]) == 0);
            bool rev = (symbolic_coord_compare(a->symbolic_coords[0], b->symbolic_coords[1]) == 0 &&
                        symbolic_coord_compare(a->symbolic_coords[1], b->symbolic_coords[0]) == 0);
            if (fwd || rev) {
                debug_log(LOG_LEVEL_ERROR, "normalization",
                          "Invariant violation: segments %d and %d have same endpoints "
                          "but were not merged",
                          a->id, b->id);
                violations++;
            }
        }
    }

    /* 不变量 3：所有约束的参与者引用有效节点 */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        for (int k = 0; k < c->participant_count; k++) {
            if (!graph_get_node(graph, c->participants[k])) {
                debug_log(LOG_LEVEL_ERROR, "normalization",
                          "Invariant violation: constraint %d references "
                          "non-existent node %d",
                          c->id, c->participants[k]);
                violations++;
            }
        }
    }

    /* 不变量 4：参与者列表按 ID 排序 */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        for (int k = 1; k < c->participant_count; k++) {
            if (c->participants[k - 1] > c->participants[k]) {
                debug_log(LOG_LEVEL_ERROR, "normalization",
                          "Invariant violation: constraint %d participants not sorted", c->id);
                violations++;
                break;
            }
        }
    }

    if (ctx->abort_on_violation && violations > 0) {
        abort();
    }

    ctx->violation_count += violations;
    return violations;
}

/* ------------------------------------------------------------------ */
/*  debug_assert_port_invariants（自 debug_state.c 迁入：引擎状态断言归同域） */
/* ------------------------------------------------------------------ */

/**
 * @brief 对引擎的约束图断言端口不变量。
 *
 * 检查：
 * 1. formal 参数端口必须有合法 parent_block_id
 * 2. 端口 namespace_depth 非负
 * 3. 函数块输入/输出端口与父块关联一致（input 端口 is_formal_param=true，
 *    output 端口 is_formal_param=false）
 *
 * @param engine 引擎实例
 * @param ctx    调试上下文（port_invariant_checks 须为 true 才执行）
 * @return 违规数量（0 = 全部通过）
 */
int debug_assert_port_invariants(const lvEngine *engine, DebugContext *ctx) {
    if (!ctx || !ctx->port_invariant_checks)
        return 0;
    if (!engine || !engine->main_graph)
        return 0;

    int violations = 0;
    ConstraintGraph *graph = engine->main_graph;

    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (node->type == GEOM_PORT) {
            Port *port = node->data.port;
            if (port->is_formal_param && port->parent_block_id < 0) {
                LOG_ERROR("port", "Node %d: Port is marked as formal param but has invalid parent_block_id (%d)",
                          node->id, port->parent_block_id);
                lv_set_error(lv_ERROR_INVALID_STATE,
                             "[PORT INVARIANT VIOLATION] Node %d: Port is marked as formal param but has invalid "
                             "parent_block_id (%d)",
                             node->id, port->parent_block_id);
                violations++;
                /* 在 release 构建中不应该 abort，而是使用 lv_set_error
                 * 记录错误后返回当前的 violations 计数。 */
                if (ctx->abort_on_violation) {
                    return violations;
                }
            }
            if (port->namespace_depth < 0) {
                LOG_ERROR("port", "Node %d: Port has negative namespace_depth (%d)", node->id, port->namespace_depth);
                lv_set_error(lv_ERROR_INVALID_STATE,
                             "[PORT INVARIANT VIOLATION] Node %d: Port has negative namespace_depth (%d)", node->id,
                             port->namespace_depth);
                violations++;
                /* 在 release 构建中不应该 abort，而是使用 lv_set_error
                 * 记录错误后返回当前的 violations 计数。 */
                if (ctx->abort_on_violation) {
                    return violations;
                }
            }
        }
        if (node->type == GEOM_FUNCTION_BLOCK) {
            for (int j = 0; j < node->data.func_block.input_count; j++) {
                int port_id = node->data.func_block.input_port_ids[j];
                GeomNode *port_node = graph_get_node(graph, port_id);
                if (port_node && port_node->type == GEOM_PORT) {
                    Port *p = port_node->data.port;
                    if (p->parent_block_id != node->id || !p->is_formal_param) {
                        LOG_ERROR("port",
                                  "Function block %d input port %d: parent_block_id mismatch (expected %d, got %d) or "
                                  "is_formal_param is false",
                                  node->id, port_id, node->id, p->parent_block_id);
                        lv_set_error(lv_ERROR_INVALID_STATE,
                                     "[PORT INVARIANT VIOLATION] Function block %d input port %d: parent_block_id "
                                     "mismatch (expected %d, got %d) or is_formal_param is false",
                                     node->id, port_id, node->id, p->parent_block_id);
                        violations++;
                        /* 在 release 构建中不应该 abort，而是使用 lv_set_error
                         * 记录错误后返回当前的 violations 计数。 */
                        if (ctx->abort_on_violation) {
                            return violations;
                        }
                    }
                }
            }
            for (int j = 0; j < node->data.func_block.output_count; j++) {
                int port_id = node->data.func_block.output_port_ids[j];
                GeomNode *port_node = graph_get_node(graph, port_id);
                if (port_node && port_node->type == GEOM_PORT) {
                    Port *p = port_node->data.port;
                    if (p->parent_block_id != node->id || p->is_formal_param) {
                        LOG_ERROR("port",
                                  "Function block %d output port %d: parent_block_id mismatch (expected %d, got %d) or "
                                  "is_formal_param is true",
                                  node->id, port_id, node->id, p->parent_block_id);
                        lv_set_error(lv_ERROR_INVALID_STATE,
                                     "[PORT INVARIANT VIOLATION] Function block %d output port %d: parent_block_id "
                                     "mismatch (expected %d, got %d) or is_formal_param is true",
                                     node->id, port_id, node->id, p->parent_block_id);
                        violations++;
                        /* 在 release 构建中不应该 abort，而是使用 lv_set_error
                         * 记录错误后返回当前的 violations 计数。 */
                        if (ctx->abort_on_violation) {
                            return violations;
                        }
                    }
                }
            }
        }
    }

    ctx->violation_count += violations;
    return violations;
}

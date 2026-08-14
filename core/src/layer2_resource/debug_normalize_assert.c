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
#include "lv/stream_context_util.h"
#include "lv/type_system.h"
#include "lv/lv_xmacro.h"
#include "lv/lv_strbuf.h"
#include "debug_internal.h"

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

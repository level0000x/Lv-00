/**
 * @file solver_snapshot.c
 * @brief 求解回滚快照（保存/恢复/释放）
 *
 * @details 从 solver.c 拆分出的子模块（Lv-00 项目 v3.3.0+）。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv00/solver.h"
#include "../solver_snapshot.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv00/constraint_graph.h"
#include "debug.h"
#include "lv00_internal.h"
#include "lv00_utils.h"
#include "mpz_poly.h"
#include "lv00/stream.h"
#include "stream_context_util.h"

/* --- 共享宏 --- */
#define LV00_SOLVER_DYNARRAY_INIT_CAP 16
#define LV00_SOLVER_LINEAR_COEFF_COUNT 2
#define LV00_SOLVER_QUADRATIC_COEFF_COUNT 3
#define LV00_ZERO_EPSILON 1e-12
#define SOLVER_DETAIL_BUF_SIZE 512
#define EQUATION_PUSH_OR_GOTO(sys, poly, vid, ci, label) \
    do { \
        if (equation_system_push((sys), (poly), (vid), (ci)) != 0) { \
            lv00_set_error(LV00_ERROR_OUT_OF_MEMORY, "push failed (OOM)"); \
            goto label; \
        } \
    } while (0)

/* ── SolverSnapshot ── */

typedef struct SolverSnapshot {
    int *node_ids;
    SymbolicCoord **copies;
    int node_count;
    int coord_count;
} SolverSnapshot;

static void solver_snapshot_free(SolverSnapshot *snapshot);

static bool solver_snapshot_save(const ConstraintGraph *graph, SolverSnapshot *snapshot) {
    if (!graph || !snapshot)
        return false;

    memset(snapshot, 0, sizeof(SolverSnapshot));
    snapshot->node_count = graph->node_count;
    if (snapshot->node_count <= 0)
        return true;

    snapshot->coord_count = snapshot->node_count * 2;
    snapshot->node_ids = lv00_malloc((size_t) snapshot->node_count * sizeof(int));
    snapshot->copies = lv00_malloc((size_t) snapshot->coord_count * sizeof(SymbolicCoord *));

    if (!snapshot->node_ids || !snapshot->copies) {
        solver_snapshot_free(snapshot);
        return false;
    }

    for (int i = 0; i < snapshot->node_count; i++) {
        snapshot->node_ids[i] = graph->nodes[i]->id;
        GeomNode *node = graph->nodes[i];

        if (node && node->symbolic_coords) {
            snapshot->copies[i * 2 + 0] =
                node->symbolic_coords[0] ? symbolic_coord_copy(node->symbolic_coords[0]) : NULL;
            snapshot->copies[i * 2 + 1] =
                (node->coord_count >= 2 && node->symbolic_coords[1])
                    ? symbolic_coord_copy(node->symbolic_coords[1])
                    : NULL;
        } else {
            snapshot->copies[i * 2 + 0] = NULL;
            snapshot->copies[i * 2 + 1] = NULL;
        }
    }

    return true;
}

/**
 * @brief 回滚——将节点坐标恢复到快照状态
 *
 * 在求解失败（SOLVER_STATUS_TIMEOUT / SOLVER_STATUS_NO_SOLUTION 等）时调用，
 * 将受影响的节点坐标替换为快照中保存的原始值。
 *
 * 使用场景：求解过程中部分方程已被求解并回代，导致某些节点坐标
 * 被修改，但后续发现系统无解或超时——此时需要回滚所有修改。
 *
 * @param graph    约束图指针
 * @param snapshot 先前通过 solver_snapshot_save 保存的快照
 */
static void solver_snapshot_restore(ConstraintGraph *graph, const SolverSnapshot *snapshot) {
    if (!graph || !snapshot || snapshot->node_count <= 0)
        return;

    for (int i = 0; i < snapshot->node_count; i++) {
        /* 按节点 ID 查找当前图中的节点（节点顺序可能已变化） */
        GeomNode *node = graph_get_node(graph, snapshot->node_ids[i]);
        if (!node || !node->symbolic_coords)
            continue;

        if (snapshot->copies[i * 2 + 0]) {
            symbolic_coord_destroy(node->symbolic_coords[0]);
            node->symbolic_coords[0] = symbolic_coord_copy(snapshot->copies[i * 2 + 0]);
        }
        if (node->coord_count >= 2 && node->symbolic_coords[1] && snapshot->copies[i * 2 + 1]) {
            symbolic_coord_destroy(node->symbolic_coords[1]);
            node->symbolic_coords[1] = symbolic_coord_copy(snapshot->copies[i * 2 + 1]);
        }
    }
}

/**
 * @brief 释放快照——求解成功后释放所有保存的坐标副本
 *
 * 快照不再需要时必须调用，否则会泄漏 SymbolicCoord 内存。
 * 此函数是幂等的：多次调用或对零值快照调用是安全的。
 *
 * @param snapshot 待释放的快照（调用后置零）
 */
static void solver_snapshot_free(SolverSnapshot *snapshot) {
    if (!snapshot)
        return;

    if (snapshot->copies) {
        for (int i = 0; i < snapshot->coord_count; i++) {
            if (snapshot->copies[i]) {
                symbolic_coord_destroy(snapshot->copies[i]);
                snapshot->copies[i] = NULL;
            }
        }
        lv00_free((void **) &snapshot->copies);
    }

    lv00_free((void **) &snapshot->node_ids);
    snapshot->node_count = 0;
    snapshot->coord_count = 0;
}


/**
 * @file solver_snapshot.c
 * @brief 求解快照/回滚实现 — 从 solver.c 拆分
 *
 * 原位置: solver.c L108-L222
 */

#include "../solver_snapshot.h"
#include "lv00/lv00.h"
#include "lv00/constraint_graph.h"
#include <string.h>

bool solver_snapshot_save(const ConstraintGraph *graph, SolverSnapshot *snapshot) {
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

void solver_snapshot_restore(ConstraintGraph *graph, const SolverSnapshot *snapshot) {
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

void solver_snapshot_free(SolverSnapshot *snapshot) {
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
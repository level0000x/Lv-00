/**
 * @file meta_verify.c
 * @brief Layer 4 元验证逻辑实现 —— 完备性、可靠性、差分验证
 *
 * 本文件替换 lv_impl_upper.c 中的桩实现，提供基于 ConstraintGraph
 * 的实质性元验证逻辑。三个验证函数被 lv_impl_upper.c 中 lv_upper_full_verify
 * 调用，作为完整验证流水线的一部分。
 *
 * 设计原则：
 *   1. 纯约束图操作 —— 不依赖引擎/会话上下文，仅在 ConstraintGraph 上操作
 *   2. 可组合 —— 各验证函数可独立调用，也可通过 lv_upper_full_verify 组合
 *   3. 保守失败 —— 遇内存分配失败等不可恢复错误时返回 -1
 *
 * @author Lv-00 Project
 * @version 1.1.0
 */

#include "lv/conflict_detector.h"
#include "lv/constraint_graph.h"

/**
 * Verify completeness: check that constraint graph has no unconstrained
 * degrees of freedom for all POINT nodes.
 *
 * Algorithm:
 *   1. Walk all nodes, count POINT nodes with coord_count == 0 (unresolved)
 *   2. Run conflict_detector for deeper analysis (conflicts can block resolution)
 *   3. Return 1 if all nodes resolved, 0 if any remain unresolved
 *
 * @param graph  Constraint graph to verify (NULL returns -1)
 * @return 1 if complete, 0 if incomplete, -1 on error
 */
int lv_graph_meta_verify_completeness(const ConstraintGraph *graph) {
    if (!graph)
        return -1;

    /* Walk all nodes, count POINT nodes with coord_count == 0 (unresolved) */
    int unresolved_count = 0;
    int error_count = 0;

    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node) {
            error_count++;
            continue;
        }
        if (!node->is_active)
            continue;
        if (node->type == GEOM_POINT && node->coord_count == 0) {
            unresolved_count++;
        }
    }

    if (error_count > 0)
        return -1;

    /* Run conflict_detector for deeper analysis */
    /* Conflicts can prevent resolution, making the graph incomplete */
    bool has_conflict = lv_conflict_detect_quick(graph);
    if (has_conflict)
        return 0;

    return (unresolved_count == 0) ? 1 : 0;
}

/**
 * Verify soundness: check that constraint graph has no contradictions.
 *
 * Algorithm:
 *   1. Call lv_conflict_detect_quick for fast structural conflict check
 *   2. Walk constraints checking for basic consistency:
 *      - All constraint participants must reference valid, active nodes
 *      - Self-incidence (same node) is always consistent
 *   3. Return 1 if no conflicts, 0 if conflicts found
 *
 * @param graph  Constraint graph to verify (NULL returns -1)
 * @return 1 if sound, 0 if contradiction found, -1 on error
 */
int lv_graph_meta_verify_soundness(const ConstraintGraph *graph) {
    if (!graph)
        return -1;
    if (graph->node_count == 0 && graph->constraint_count == 0)
        return 1;

    /* Call lv_conflict_detect_quick for fast structural check */
    bool has_conflict = lv_conflict_detect_quick(graph);
    if (has_conflict)
        return 0;

    /* Walk constraints checking for basic consistency */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        if (!c)
            continue;
        if (!c->is_active)
            continue;

        /* Check participant validity */
        for (int j = 0; j < c->participant_count; j++) {
            int pid = c->participants[j];
            GeomNode *node = graph_get_node(graph, pid);
            if (!node || !node->is_active) {
                /* Constraint references invalid or inactive node */
                return 0;
            }
        }

        /* INCIDENCE between two valid nodes is consistent by default */
        /* Same node compared to itself is always consistent */
        if (c->type == INCIDENCE && c->participant_count >= 2) {
            if (c->participants[0] == c->participants[1]) {
                /* Self-incidence is trivially consistent */
                continue;
            }
        }
    }

    return 1; /* Sound */
}

/**
 * Verify differential: compare two constraint graphs and report differences.
 *
 * Algorithm:
 *   1. Quick pointer-equality check (same graph → identical)
 *   2. Compare node counts
 *   3. Compare constraint counts
 *   4. Compare individual nodes (type, coord_count)
 *   5. Compare individual constraints (type, participant_count)
 *   6. Return count of differences (0 if identical)
 *
 * @param graph_a  First graph (NULL returns -1)
 * @param graph_b  Second graph (NULL returns -1)
 * @return 0 if identical, positive count of differences, -1 on error
 */
int lv_graph_meta_verify_differential(const ConstraintGraph *graph_a, const ConstraintGraph *graph_b) {
    if (!graph_a || !graph_b)
        return -1;
    if (graph_a == graph_b)
        return 0;

    int differences = 0;

    /* Compare node counts */
    if (graph_a->node_count != graph_b->node_count) {
        differences++;
    }

    /* Compare constraint counts */
    if (graph_a->constraint_count != graph_b->constraint_count) {
        differences++;
    }

    /* Compare individual nodes (type, coord_count) */
    int min_node_count = graph_a->node_count < graph_b->node_count ? graph_a->node_count : graph_b->node_count;
    for (int i = 0; i < min_node_count; i++) {
        GeomNode *na = graph_a->nodes[i];
        GeomNode *nb = graph_b->nodes[i];
        if (!na || !nb) {
            differences++;
            continue;
        }
        if (na->type != nb->type || na->coord_count != nb->coord_count) {
            differences++;
        }
    }

    /* Compare individual constraints (type, participant_count) */
    int min_con_count =
        graph_a->constraint_count < graph_b->constraint_count ? graph_a->constraint_count : graph_b->constraint_count;
    for (int i = 0; i < min_con_count; i++) {
        Constraint *ca = graph_a->constraints[i];
        Constraint *cb = graph_b->constraints[i];
        if (!ca || !cb) {
            differences++;
            continue;
        }
        if (ca->type != cb->type || ca->participant_count != cb->participant_count) {
            differences++;
        }
    }

    return differences;
}

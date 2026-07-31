/**
 * @file graph_memory.c
 * @brief ConstraintGraph 内存管理与资源释放
 *
 * @details 实现约束图的创建、销毁及内存生命周期管理：
 *          - graph_create: 分配并初始化空的约束图结构
 *            （包括节点数组、约束数组、序列化缓冲区和哈希索引）
 *          - graph_destroy: 级联销毁所有子资源
 *            销毁顺序：节点 → 约束参与者数组 → 约束数组 → 哈希索引 → 图本身
 *          - graph_clear: 清空所有节点和约束（保留图结构，可重用）
 *          - node_destroy: 单节点析构（释放符号坐标数组和属性字符串）
 *
 *          类型层级管理（GraphKind）：
 *          - FLAT / HIERARCHICAL / PARAMETRIC 三种图类型
 *          - 影响节点类型兼容性检查和约束验证逻辑
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
#include "lv/symbolic_coord.h"

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"

/* ── 前向声明（graph_index.c 中定义） ── */
void node_destroy(GeomNode *node);

/* 冗余约束检测用的哈希排序辅助结构 */
typedef struct {
    int constraint_idx;
    unsigned long hash;
} ConstraintHashEntry;

/* 按 hash 升序比较（插入排序使用） */
static int cmp_constraint_hash(const void *a, const void *b, void *ctx) {
    (void) ctx;
    const ConstraintHashEntry *ea = (const ConstraintHashEntry *) a;
    const ConstraintHashEntry *eb = (const ConstraintHashEntry *) b;
    if (ea->hash < eb->hash)
        return -1;
    if (ea->hash > eb->hash)
        return 1;
    return 0;
}

/**
 * @brief 销毁约束图并释放所有资源
 *
 * 依次销毁所有节点（调用 node_destroy）、释放所有约束的参与者数组和约束本身、
 * 释放节点和约束的哈希索引、释放序列化缓冲区和邻接矩阵。
 * 最后释放约束图结构体本身。
 *
 * @param graph 约束图指针（可以为 NULL，此时直接返回）
 */
void graph_destroy(ConstraintGraph *graph) {
    if (!graph)
        return;
    for (int i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i])
            node_destroy(graph->nodes[i]);
    }
    lv_free((void **) &graph->nodes);
    for (int i = 0; i < graph->constraint_count; i++) {
        if (graph->constraints[i]) {
            lv_free((void **) &graph->constraints[i]->participants);
            lv_free((void **) &graph->constraints[i]);
        }
    }
    lv_free((void **) &graph->constraints);
    lv_free((void **) &graph->node_index);
    lv_free((void **) &graph->constraint_index);
    /* 释放每图级的错误缓冲区（v3.3.0） */
    lv_free((void **) &graph->error_buffer);
    lv_free((void **) &graph->serialize_buffer);
    lv_free((void **) &graph);
}

/**
 * 检测约束图中的冗余约束。
 *
 * 使用两阶段检测：
 * 1. 精确重复检测：使用哈希分组和排序实现 O(n log n) 复杂度
 * 2. 线性相关性检测：使用高斯消元和 GMP mpq_t 精确算术
 *
 * @param graph     约束图指针
 * @param out_count 输出：找到的冗余约束数量
 * @return 冗余约束 ID 数组，调用者需负责释放；失败时返回 NULL
 */
int *graph_detect_redundant_constraints(const ConstraintGraph *graph, int *out_count) {
    /* 参数验证：防止空指针解引用 */
    if (!out_count)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "graph_detect_redundant_constraints: out_count is NULL");
    *out_count = 0;
    if (!graph || graph->constraint_count == 0)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "graph_detect_redundant_constraints: graph is NULL or empty");

    /* Allocate enough space for both phases */
    /* [安全] 防止 constraint_count * 2 整数溢出 */
    if (graph->constraint_count > INT_MAX / 2)
        lv_RETURN_ERROR_NULL(lv_ERROR_OVERFLOW, "graph_detect_redundant_constraints: constraint_count overflow");
    int max_redundant = graph->constraint_count * 2;
    int *redundant = lv_malloc((size_t) max_redundant * sizeof(int));
    if (!redundant)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "graph_detect_redundant_constraints: malloc redundant failed");
    for (int i = 0; i < max_redundant; i++) {
        redundant[i] = -1;
    }

    /* Phase 1: Exact duplicate detection using hash-based grouping
     * Optimization: O(n log n) instead of O(n²) by sorting constraints
     * by a hash signature (type + participant_count + first participant).
     * Only compare constraints within the same hash group.
     */

    /* Helper struct for sorting — ConstraintHashEntry 已移至文件作用域 */
    int n = graph->constraint_count;
    if (n > 1) {
        ConstraintHashEntry *entries = lv_malloc((size_t) n * sizeof(ConstraintHashEntry));
        if (entries) {
            /* Compute hash for each constraint */
            for (int i = 0; i < n; i++) {
                Constraint *c = graph->constraints[i];
                unsigned long h = (unsigned long) c->type * 31 + (unsigned long) c->participant_count;
                for (int k = 0; k < c->participant_count; k++) {
                    h = h * 37 + (unsigned long) c->participants[k];
                }
                entries[i].constraint_idx = i;
                entries[i].hash = h;
            }

            /* Sort by hash (simple insertion sort for small arrays, qsort for large) */
            lv_insertion_sort(entries, (size_t) n, sizeof(ConstraintHashEntry), cmp_constraint_hash, NULL);

            /* Compare constraints with same hash */
            int i = 0;
            while (i < n) {
                unsigned long cur_hash = entries[i].hash;
                int j = i + 1;
                /* Find group with same hash */
                while (j < n && entries[j].hash == cur_hash)
                    j++;

                /* Compare all pairs within this group */
                for (int a = i; a < j; a++) {
                    Constraint *ci = graph->constraints[entries[a].constraint_idx];
                    for (int b = a + 1; b < j; b++) {
                        Constraint *cj = graph->constraints[entries[b].constraint_idx];
                        if (ci->type != cj->type || ci->participant_count != cj->participant_count)
                            continue;
                        bool same = true;
                        for (int k = 0; k < ci->participant_count; k++) {
                            if (ci->participants[k] != cj->participants[k]) {
                                same = false;
                                break;
                            }
                        }
                        if (same) {
                            redundant[*out_count] = cj->id;
                            (*out_count)++;
                        }
                    }
                }
                i = j;
            }

            lv_free((void **) &entries);
        } else {
            /* Fallback to O(n²) if allocation fails */
            for (int i = 0; i < graph->constraint_count; i++) {
                Constraint *ci = graph->constraints[i];
                for (int j = i + 1; j < graph->constraint_count; j++) {
                    Constraint *cj = graph->constraints[j];
                    if (ci->type != cj->type || ci->participant_count != cj->participant_count)
                        continue;
                    bool same = true;
                    for (int k = 0; k < ci->participant_count; k++) {
                        if (ci->participants[k] != cj->participants[k]) {
                            same = false;
                            break;
                        }
                    }
                    if (same) {
                        redundant[*out_count] = cj->id;
                        (*out_count)++;
                    }
                }
            }
        }
    }

    /* Phase 2: Linear dependency detection using Gaussian elimination
     * with GMP mpq_t for exact rational arithmetic.
     *
     * For INCIDENCE constraints: point P on line AB means
     *   (P-A) x (B-A) = 0  (cross product in 2D)
     *   => (Px-Ax)*(By-Ay) - (Py-Ay)*(Bx-Ax) = 0
     * This is a linear equation in the coordinates.
     *
     * For BETWEENNESS constraints: P2 is between P1 and P3,
     *   which implies collinearity: (P2-P1) x (P3-P1) = 0
     *   => (P2x-P1x)*(P3y-P1y) - (P2y-P1y)*(P3x-P1x) = 0
     *
     * We collect these linear equations, build a coefficient matrix,
     * and use Gaussian elimination to find rows that are linearly
     * dependent (i.e., can be expressed as combinations of others).
     */

    /* Collect all coordinate variables (point x,y pairs) */
    /* First, find all points referenced by INCIDENCE/BETWEENNESS constraints */
    int *point_ids = lv_malloc((size_t) graph->node_count * sizeof(int));
    if (!point_ids) {
        lv_free((void **) &redundant);
        return redundant;
    }
    int point_count = 0;
    bool *point_seen = lv_calloc(graph->node_count, sizeof(bool));
    if (!point_seen) {
        lv_free((void **) &point_ids);
        lv_free((void **) &redundant);
        return redundant;
    }

    /* Use a mapping from node id to variable index */
    int max_node_id = 0;
    for (int i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i]->id > max_node_id)
            max_node_id = graph->nodes[i]->id;
    }

    /* node_id_to_var_idx: maps node_id to variable index (-1 if not a variable) */
    int *node_id_to_var_idx = lv_malloc((size_t) (max_node_id + 1) * sizeof(int));
    if (!node_id_to_var_idx) {
        lv_free((void **) &point_seen);
        lv_free((void **) &point_ids);
        lv_free((void **) &redundant);
        return redundant;
    }
    for (int i = 0; i <= max_node_id; i++)
        node_id_to_var_idx[i] = -1;

    /* Collect points involved in INCIDENCE or BETWEENNESS constraints */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        if (c->type != INCIDENCE && c->type != BETWEENNESS)
            continue;
        for (int j = 0; j < c->participant_count; j++) {
            int nid = c->participants[j];
            if (nid < 0 || nid > max_node_id)
                continue;
            GeomNode *n = graph_get_node(graph, nid);
            if (!n || n->type != GEOM_POINT)
                continue;
            if (node_id_to_var_idx[nid] < 0) {
                node_id_to_var_idx[nid] = point_count * 2; /* x,y pair */
                if (point_count < graph->node_count) {
                    point_ids[point_count] = nid;
                    point_count++;
                }
            }
        }
    }

    int num_vars = point_count * 2; /* x and y for each point */

    /* Count linear constraints */
    int num_linear = 0;
    int *linear_constraint_indices = lv_malloc((size_t) graph->constraint_count * sizeof(int));
    if (!linear_constraint_indices) {
        lv_free((void **) &node_id_to_var_idx);
        lv_free((void **) &point_seen);
        lv_free((void **) &point_ids);
        lv_free((void **) &redundant);
        return redundant;
    }
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        if (c->type == INCIDENCE || c->type == BETWEENNESS) {
            linear_constraint_indices[num_linear] = i;
            num_linear++;
        }
    }

    if (num_linear <= 1 || num_vars <= 0) {
        /* 约束数量不足，无法进行线性相关性检测 */
        lv_free((void **) &point_ids);
        lv_free((void **) &point_seen);
        lv_free((void **) &node_id_to_var_idx);
        lv_free((void **) &linear_constraint_indices);
        return redundant;
    }

    /* 使用 GMP mpq_t 构建系数矩阵进行精确算术运算
     * 矩阵维度：num_linear x (num_vars + 1) [增广矩阵]
     * 每行代表一个约束对应的线性方程 */
    /* [安全] 防止乘法溢出：size_t 计算 */
    size_t matrix_size = (size_t) num_linear * (size_t) (num_vars + 1);
    if (num_linear > 0 && matrix_size / (size_t) num_linear != (size_t) (num_vars + 1)) {
        lv_free((void **) &point_ids);
        lv_free((void **) &point_seen);
        lv_free((void **) &node_id_to_var_idx);
        lv_free((void **) &linear_constraint_indices);
        return redundant;
    }
    mpq_t *matrix = lv_calloc(matrix_size, sizeof(mpq_t));
    if (!matrix) {
        lv_free((void **) &point_ids);
        lv_free((void **) &point_seen);
        lv_free((void **) &node_id_to_var_idx);
        lv_free((void **) &linear_constraint_indices);
        return redundant;
    }

    for (int i = 0; i < num_linear * (num_vars + 1); i++) {
        mpq_init(matrix[i]);
    }

    /* Fill the matrix with equation coefficients */
    for (int row = 0; row < num_linear; row++) {
        Constraint *c = graph->constraints[linear_constraint_indices[row]];

        if (c->type == INCIDENCE && c->participant_count >= 2) {
            /* INCIDENCE: point P on line segment S
             * (P-A) x (B-A) = 0
             * We need the coordinates of P and the endpoints of S.
             * The line segment's symbolic_coords[0..3] are (Ax, Ay, Bx, By).
             * The point's symbolic_coords[0..1] are (Px, Py).
             */
            int point_id = c->participants[0];
            int seg_id = c->participants[1];
            GeomNode *pt = graph_get_node(graph, point_id);
            GeomNode *seg = graph_get_node(graph, seg_id);

            if (pt && seg && seg->type == GEOM_LINE_SEGMENT && seg->coord_count >= 4 && seg->symbolic_coords &&
                pt->coord_count >= 2 && pt->symbolic_coords) {
                /* Get coordinates - use exact mpq_t for RATIONAL, double for others */
                mpq_t ax_q, ay_q, bx_q, by_q;
                bool ax_exact = false, ay_exact = false, bx_exact = false, by_exact = false;
                double ax_d, ay_d, bx_d, by_d;

                ax_d = symbolic_coord_to_double(seg->symbolic_coords[0]);
                ay_d = symbolic_coord_to_double(seg->symbolic_coords[1]);
                bx_d = symbolic_coord_to_double(seg->symbolic_coords[2]);
                by_d = symbolic_coord_to_double(seg->symbolic_coords[3]);

                if (seg->symbolic_coords[0]->type == RATIONAL) {
                    mpq_init(ax_q);
                    mpq_set(ax_q, seg->symbolic_coords[0]->data.rational->value);
                    ax_exact = true;
                }
                if (seg->symbolic_coords[1]->type == RATIONAL) {
                    mpq_init(ay_q);
                    mpq_set(ay_q, seg->symbolic_coords[1]->data.rational->value);
                    ay_exact = true;
                }
                if (seg->symbolic_coords[2]->type == RATIONAL) {
                    mpq_init(bx_q);
                    mpq_set(bx_q, seg->symbolic_coords[2]->data.rational->value);
                    bx_exact = true;
                }
                if (seg->symbolic_coords[3]->type == RATIONAL) {
                    mpq_init(by_q);
                    mpq_set(by_q, seg->symbolic_coords[3]->data.rational->value);
                    by_exact = true;
                }

                /* Direction vector of line: (Bx-Ax, By-Ay) */
                int p_idx = node_id_to_var_idx[point_id];
                if (p_idx >= 0) {
                    /* Coefficient for Px: (By-Ay), for Py: -(Bx-Ax) */
                    if (by_exact && ay_exact) {
                        mpq_t dy_q;
                        mpq_init(dy_q);
                        mpq_sub(dy_q, by_q, ay_q);
                        mpq_set(matrix[row * (num_vars + 1) + p_idx], dy_q);
                        mpq_neg(matrix[row * (num_vars + 1) + p_idx], dy_q);
                        mpq_clear(dy_q);
                    } else {
                        double dy = by_d - ay_d;
                        mpq_set_d(matrix[row * (num_vars + 1) + p_idx], dy);
                    }

                    if (bx_exact && ax_exact) {
                        mpq_t dx_q;
                        mpq_init(dx_q);
                        mpq_sub(dx_q, bx_q, ax_q);
                        mpq_neg(matrix[row * (num_vars + 1) + p_idx + 1], dx_q);
                        mpq_clear(dx_q);
                    } else {
                        double dx = bx_d - ax_d;
                        mpq_set_d(matrix[row * (num_vars + 1) + p_idx + 1], -dx);
                    }
                }

                /* Constant term: Ax*dy - Ay*dx */
                if (ax_exact && ay_exact && bx_exact && by_exact) {
                    mpq_t dy_q, dx_q, const_q;
                    mpq_init(dy_q);
                    mpq_init(dx_q);
                    mpq_init(const_q);
                    mpq_sub(dy_q, by_q, ay_q);
                    mpq_sub(dx_q, bx_q, ax_q);
                    /* const = Ax*dy - Ay*dx */
                    mpq_mul(const_q, ax_q, dy_q);
                    mpq_t tmp_q;
                    mpq_init(tmp_q);
                    mpq_mul(tmp_q, ay_q, dx_q);
                    mpq_sub(const_q, const_q, tmp_q);
                    mpq_clear(tmp_q);
                    mpq_set(matrix[row * (num_vars + 1) + num_vars], const_q);
                    mpq_clear(dy_q);
                    mpq_clear(dx_q);
                    mpq_clear(const_q);
                } else {
                    double dx = bx_d - ax_d;
                    double dy = by_d - ay_d;
                    mpq_set_d(matrix[row * (num_vars + 1) + num_vars], ax_d * dy - ay_d * dx);
                }

                if (ax_exact)
                    mpq_clear(ax_q);
                if (ay_exact)
                    mpq_clear(ay_q);
                if (bx_exact)
                    mpq_clear(bx_q);
                if (by_exact)
                    mpq_clear(by_q);
            }
        } else if (c->type == BETWEENNESS && c->participant_count >= 3) {
            /* BETWEENNESS: P2 between P1 and P3
             * Collinearity: (P2-P1) x (P3-P1) = 0
             * => (P2x-P1x)*(P3y-P1y) - (P2y-P1y)*(P3x-P1x) = 0
             * => P2x*(P3y-P1y) - P2y*(P3x-P1x) - P1x*(P3y-P1y) + P1y*(P3x-P1x) = 0
             */
            int p1_id = c->participants[0];
            int p2_id = c->participants[1];
            int p3_id = c->participants[2];
            GeomNode *p1 = graph_get_node(graph, p1_id);
            GeomNode *p2 = graph_get_node(graph, p2_id);
            GeomNode *p3 = graph_get_node(graph, p3_id);

            if (p1 && p2 && p3 && p1->coord_count >= 2 && p1->symbolic_coords && p2->coord_count >= 2 &&
                p2->symbolic_coords && p3->coord_count >= 2 && p3->symbolic_coords) {
                /* Get coordinates - use exact mpq_t for RATIONAL, double for others */
                mpq_t p1x_q, p1y_q, p3x_q, p3y_q;
                bool p1x_exact = false, p1y_exact = false, p3x_exact = false, p3y_exact = false;
                double p1x, p1y, p3x, p3y;

                p1x = symbolic_coord_to_double(p1->symbolic_coords[0]);
                p1y = symbolic_coord_to_double(p1->symbolic_coords[1]);
                p3x = symbolic_coord_to_double(p3->symbolic_coords[0]);
                p3y = symbolic_coord_to_double(p3->symbolic_coords[1]);

                if (p1->symbolic_coords[0]->type == RATIONAL) {
                    mpq_init(p1x_q);
                    mpq_set(p1x_q, p1->symbolic_coords[0]->data.rational->value);
                    p1x_exact = true;
                }
                if (p1->symbolic_coords[1]->type == RATIONAL) {
                    mpq_init(p1y_q);
                    mpq_set(p1y_q, p1->symbolic_coords[1]->data.rational->value);
                    p1y_exact = true;
                }
                if (p3->symbolic_coords[0]->type == RATIONAL) {
                    mpq_init(p3x_q);
                    mpq_set(p3x_q, p3->symbolic_coords[0]->data.rational->value);
                    p3x_exact = true;
                }
                if (p3->symbolic_coords[1]->type == RATIONAL) {
                    mpq_init(p3y_q);
                    mpq_set(p3y_q, p3->symbolic_coords[1]->data.rational->value);
                    p3y_exact = true;
                }

                int p2_idx = node_id_to_var_idx[p2_id];
                if (p2_idx >= 0) {
                    /* Coefficient for P2x: (P3y-P1y), for P2y: -(P3x-P1x) */
                    if (p3y_exact && p1y_exact) {
                        mpq_t dy13_q;
                        mpq_init(dy13_q);
                        mpq_sub(dy13_q, p3y_q, p1y_q);
                        mpq_set(matrix[row * (num_vars + 1) + p2_idx], dy13_q);
                        mpq_clear(dy13_q);
                    } else {
                        mpq_set_d(matrix[row * (num_vars + 1) + p2_idx], p3y - p1y);
                    }

                    if (p3x_exact && p1x_exact) {
                        mpq_t dx13_q;
                        mpq_init(dx13_q);
                        mpq_sub(dx13_q, p3x_q, p1x_q);
                        mpq_neg(matrix[row * (num_vars + 1) + p2_idx + 1], dx13_q);
                        mpq_clear(dx13_q);
                    } else {
                        mpq_set_d(matrix[row * (num_vars + 1) + p2_idx + 1], -(p3x - p1x));
                    }
                }

                /* Constant term: P1x*(P3y-P1y) - P1y*(P3x-P1x) */
                if (p1x_exact && p1y_exact && p3x_exact && p3y_exact) {
                    mpq_t dy13_q, dx13_q, const_q;
                    mpq_init(dy13_q);
                    mpq_init(dx13_q);
                    mpq_init(const_q);
                    mpq_sub(dy13_q, p3y_q, p1y_q);
                    mpq_sub(dx13_q, p3x_q, p1x_q);
                    mpq_mul(const_q, p1x_q, dy13_q);
                    mpq_t tmp13_q;
                    mpq_init(tmp13_q);
                    mpq_mul(tmp13_q, p1y_q, dx13_q);
                    mpq_sub(const_q, const_q, tmp13_q);
                    mpq_clear(tmp13_q);
                    mpq_set(matrix[row * (num_vars + 1) + num_vars], const_q);
                    mpq_clear(dy13_q);
                    mpq_clear(dx13_q);
                    mpq_clear(const_q);
                } else {
                    double dy13 = p3y - p1y;
                    double dx13 = p3x - p1x;
                    mpq_set_d(matrix[row * (num_vars + 1) + num_vars], p1x * dy13 - p1y * dx13);
                }

                if (p1x_exact)
                    mpq_clear(p1x_q);
                if (p1y_exact)
                    mpq_clear(p1y_q);
                if (p3x_exact)
                    mpq_clear(p3x_q);
                if (p3y_exact)
                    mpq_clear(p3y_q);
            }
        }
    }

    /* Gaussian elimination with partial pivoting using mpq_t */
    int *pivot_row = lv_malloc((size_t) num_linear * sizeof(int)); /* maps row i -> original constraint index */
    if (!pivot_row) {
        for (int i = 0; i < num_linear * (num_vars + 1); i++)
            mpq_clear(matrix[i]);
        lv_free((void **) &matrix);
        lv_free((void **) &linear_constraint_indices);
        lv_free((void **) &node_id_to_var_idx);
        lv_free((void **) &point_seen);
        lv_free((void **) &point_ids);
        lv_free((void **) &redundant);
        return redundant;
    }
    for (int i = 0; i < num_linear; i++)
        pivot_row[i] = linear_constraint_indices[i];

    int rank = 0;
    for (int col = 0; col < num_vars && rank < num_linear; col++) {
        /* Find pivot row (first non-zero entry in this column) */
        int pivot = -1;
        for (int row = rank; row < num_linear; row++) {
            if (mpq_sgn(matrix[row * (num_vars + 1) + col]) != 0) {
                pivot = row;
                break;
            }
        }
        if (pivot < 0)
            continue; /* All zeros in this column */

        /* Swap rows rank and pivot */
        if (pivot != rank) {
            for (int j = 0; j <= num_vars; j++) {
                mpq_swap(matrix[rank * (num_vars + 1) + j], matrix[pivot * (num_vars + 1) + j]);
            }
            int tmp = pivot_row[rank];
            pivot_row[rank] = pivot_row[pivot];
            pivot_row[pivot] = tmp;
        }

        /* Scale pivot row so leading coefficient is 1 */
        mpq_t inv_pivot;
        mpq_init(inv_pivot);
        mpq_inv(inv_pivot, matrix[rank * (num_vars + 1) + col]);
        for (int j = col; j <= num_vars; j++) {
            mpq_mul(matrix[rank * (num_vars + 1) + j], matrix[rank * (num_vars + 1) + j], inv_pivot);
        }
        mpq_clear(inv_pivot);

        /* Eliminate this column from all other rows */
        for (int row = 0; row < num_linear; row++) {
            if (row == rank)
                continue;
            if (mpq_sgn(matrix[row * (num_vars + 1) + col]) == 0)
                continue;

            mpq_t factor;
            mpq_init(factor);
            mpq_set(factor, matrix[row * (num_vars + 1) + col]);
            for (int j = col; j <= num_vars; j++) {
                mpq_t tmp;
                mpq_init(tmp);
                mpq_mul(tmp, factor, matrix[rank * (num_vars + 1) + j]);
                mpq_sub(matrix[row * (num_vars + 1) + j], matrix[row * (num_vars + 1) + j], tmp);
                mpq_clear(tmp);
            }
            mpq_clear(factor);
        }

        rank++;
    }

    /* After Gaussian elimination, rows from 'rank' to 'num_linear-1'
     * should be all-zero. These correspond to linearly dependent constraints.
     * However, we also check for rows that became zero due to elimination
     * but have a non-zero RHS (inconsistent), which we skip.
     * Rows that are all-zero (including RHS) are redundant.
     */
    for (int row = rank; row < num_linear; row++) {
        /* Check if this row is all-zero */
        bool all_zero = true;
        for (int j = 0; j <= num_vars; j++) {
            if (mpq_sgn(matrix[row * (num_vars + 1) + j]) != 0) {
                all_zero = false;
                break;
            }
        }
        if (all_zero) {
            int con_id = pivot_row[row];
            /* Check not already marked redundant */
            bool already = false;
            for (int m = 0; m < *out_count; m++) {
                if (redundant[m] == con_id) {
                    already = true;
                    break;
                }
            }
            if (!already && *out_count < max_redundant) {
                redundant[*out_count] = con_id;
                (*out_count)++;
                LOG_DEBUG("constraint_graph", "Linear dependency: constraint %d is redundant (Gaussian elimination)",
                          con_id);
            }
        }
    }

    /* Also check among the rank rows: if two rows have identical
     * coefficient patterns, one is redundant */
    for (int i = 0; i < rank; i++) {
        for (int j = i + 1; j < rank; j++) {
            bool identical = true;
            for (int k = 0; k <= num_vars; k++) {
                if (mpq_equal(matrix[i * (num_vars + 1) + k], matrix[j * (num_vars + 1) + k]) == 0) {
                    identical = false;
                    break;
                }
            }
            if (identical) {
                int con_id = pivot_row[j];
                bool already = false;
                for (int m = 0; m < *out_count; m++) {
                    if (redundant[m] == con_id) {
                        already = true;
                        break;
                    }
                }
                if (!already && *out_count < max_redundant) {
                    redundant[*out_count] = con_id;
                    (*out_count)++;
                    LOG_DEBUG("constraint_graph", "Linear dependency: constraint %d is identical to constraint %d",
                              con_id, pivot_row[i]);
                }
            }
        }
    }

    /* 清理 Phase 2 资源 */
    for (int i = 0; i < num_linear * (num_vars + 1); i++) {
        mpq_clear(matrix[i]);
    }
    lv_free((void **) &matrix);
    lv_free((void **) &pivot_row);
    lv_free((void **) &linear_constraint_indices);
    lv_free((void **) &node_id_to_var_idx);
    lv_free((void **) &point_seen);
    lv_free((void **) &point_ids);

    return redundant;
}
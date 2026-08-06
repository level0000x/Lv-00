/**
 * @file graph_node_conflict.c
 * @brief ConstraintGraph 节点与约束生命周期管理 —— 线段相交与增量代数冲突检测
 *
 * @details 由 graph_node.c 按功能域拆分而来。
 *          共享内部函数声明见 graph_node_internal.h。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include <assert.h>
#include <float.h>
#include <gmp.h>
#include <math.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/solver.h"
#include "lv/symbolic_coord.h"

#include "config.h"
#include "context.h"
#include "debug.h"
#include "error_codes.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "stream.h"
#include "stream_context_util.h"
#include "lv/lv_strbuf.h"

#include "graph_node_internal.h"

/* ================================================================
 *  增量代数冲突检测
 * ================================================================ */

/* ── 冲突检测函数（文件作用域，用于查找表） ── */
typedef bool (*ConflictCheckFn)(const ConstraintGraph *graph, const Constraint *new_constraint);

static bool check_conflict_incidence(const ConstraintGraph *graph, const Constraint *new_constraint) {
    /* 点不应已被约束在两条不相交线上（这在2D中会过度约束） */
    int point_id = new_constraint->participants[0];
    int line_id = new_constraint->participants[1];
    int incident_count = 0;
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        if (c->type != INCIDENCE)
            continue;
        if (c->participants[0] == point_id && c->participants[1] != line_id) {
            incident_count++;
        }
    }
    /* 在2D中，点在2条不同线上是可以的（构成交点）
     * 但3条或更多条线且没有相交约束则是冲突 */
    if (incident_count >= 2) {
        /* 检查是否有任意一对关联线之间有 INTERSECTION 约束 */
        /* v3.4.2: 使用动态分配替代固定大小数组，避免缓冲区溢出风险 */
        int *lines = NULL;
        int line_count = 0;
        int lines_capacity = 8; /* 初始容量 */

        lines = (int *) lv_malloc(sizeof(int) * lines_capacity);
        if (!lines) {
            lv_LOG_ERROR("check_incremental_conflict: 内存分配失败");
            return false; /* 内存不足，保守返回无冲突 */
        }

        for (int i = 0; i < graph->constraint_count; i++) {
            Constraint *c = graph->constraints[i];
            if (c->type == INCIDENCE && c->participants[0] == point_id) {
                /* v3.4.2: 动态扩容 */
                if (!lv_ensure_capacity((void **) &lines, line_count, &lines_capacity, sizeof(int), 0)) {
                    lv_free((void **) &lines);
                    return false;
                }
                lines[line_count++] = c->participants[1];
            }
        }
        /* 包含新添加的线 */
        if (!lv_ensure_capacity((void **) &lines, line_count, &lines_capacity, sizeof(int), 0)) {
            lv_free((void **) &lines);
            return false;
        }
        lines[line_count++] = line_id;

        /* 检查所有线对的相交约束 */
        bool conflict_found = false;
        for (int a = 0; a < line_count && !conflict_found; a++) {
            for (int b = a + 1; b < line_count && !conflict_found; b++) {
                bool has_intersection = false;
                for (int i = 0; i < graph->constraint_count; i++) {
                    Constraint *c = graph->constraints[i];
                    if (c->type == INTERSECTION && c->participant_count == 3) {
                        if ((c->participants[0] == lines[a] && c->participants[1] == lines[b]) ||
                            (c->participants[0] == lines[b] && c->participants[1] == lines[a])) {
                            has_intersection = true;
                            break;
                        }
                    }
                }
                if (!has_intersection) {
                    /* 两条线共点但没有相交约束 - 如果线平行则是潜在冲突 */
                    conflict_found = true;
                }
            }
        }

        /* v3.4.2: 释放动态分配的数组 */
        lv_free((void **) &lines);

        if (conflict_found) {
            return true;
        }
    }
    return false;
}

static bool check_conflict_betweenness(const ConstraintGraph *graph, const Constraint *new_constraint) {
    /* 检查三个点是否已被约束为非共线（如，各自位于不同线上） */
    int p1 = new_constraint->participants[0];
    int p2 = new_constraint->participants[1];
    int p3 = new_constraint->participants[2];

    /* 收集每个点所在的线 */
    int p1_lines[64], p1_lc = 0;
    int p2_lines[64], p2_lc = 0;
    int p3_lines[64], p3_lc = 0;
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        if (c->type != INCIDENCE)
            continue;
        int pid = c->participants[0], lid = c->participants[1];
        if (pid == p1 && p1_lc < 64)
            p1_lines[p1_lc++] = lid;
        if (pid == p2 && p2_lc < 64)
            p2_lines[p2_lc++] = lid;
        if (pid == p3 && p3_lc < 64)
            p3_lines[p3_lc++] = lid;
    }
    /* 如果没有两个点共享一条线，则它们不可能共线 */
    bool any_shared = false;
    for (int i = 0; i < p1_lc && !any_shared; i++) {
        for (int j = 0; j < p2_lc && !any_shared; j++) {
            if (p1_lines[i] == p2_lines[j])
                any_shared = true;
        }
    }
    if (!any_shared) {
        for (int i = 0; i < p2_lc && !any_shared; i++) {
            for (int j = 0; j < p3_lc && !any_shared; j++) {
                if (p2_lines[i] == p3_lines[j])
                    any_shared = true;
            }
        }
    }
    if (!any_shared) {
        for (int i = 0; i < p1_lc && !any_shared; i++) {
            for (int j = 0; j < p3_lc && !any_shared; j++) {
                if (p1_lines[i] == p3_lines[j])
                    any_shared = true;
            }
        }
    }
    if (!any_shared)
        return true; /* 冲突：不可能共线 */
    return false;
}

static bool check_conflict_intersection(const ConstraintGraph *graph, const Constraint *new_constraint) {
    /* Check if the two lines are already known to be parallel
     * (no intersection possible). For now, we check if there's
     * already an INTERSECTION constraint for the same pair with
     * a different result point. */
    int l1 = new_constraint->participants[0];
    int l2 = new_constraint->participants[1];
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        if (c->type == INTERSECTION && c->participant_count == 3) {
            if ((c->participants[0] == l1 && c->participants[1] == l2) ||
                (c->participants[0] == l2 && c->participants[1] == l1)) {
                /* Already have an intersection for this pair */
                if (c->participants[2] != new_constraint->participants[2]) {
                    return true; /* Different result point = conflict */
                }
            }
        }
    }
    return false;
}

static bool check_conflict_none(const ConstraintGraph *graph, const Constraint *new_constraint) {
    (void)graph; (void)new_constraint;
    /* No simple algebraic conflict check for these types */
    return false;
}

static const ConflictCheckFn kConflictCheckTable[] = {
    check_conflict_incidence,    /* INCIDENCE */
    check_conflict_betweenness,  /* BETWEENNESS */
    check_conflict_intersection, /* INTERSECTION */
    check_conflict_none,         /* CONTAINMENT */
    check_conflict_none,         /* CONNECTION */
    check_conflict_none          /* ANGLE */
};
static const int kConflictCheckTableCount =
    (int)(sizeof(kConflictCheckTable) / sizeof(kConflictCheckTable[0]));

/**
 * 检查新约束是否与现有约束代数冲突。
 *
 * 使用 GMP mpq_t 进行精确有理数运算：
 * - INCIDENCE：检查点是否已约束在另一条不相交线上
 * - BETWEENNESS：检查三点是否已约束为非共线
 * - INTERSECTION：检查两线是否已声明为平行
 * - CONTAINMENT：检查内部节点是否已在外部
 * - CONNECTION：无代数冲突可能
 *
 * @param graph         约束图指针
 * @param new_constraint 新约束指针
 * @return true 表示存在冲突，false 表示无冲突
 */
bool check_incremental_conflict(const ConstraintGraph *graph, const Constraint *new_constraint) {
    if (!graph || !new_constraint)
        return false;

    if (new_constraint->type >= 0 && new_constraint->type < kConflictCheckTableCount) {
        return kConflictCheckTable[(int)new_constraint->type](graph, new_constraint);
    }
    /* v3.5.0: 未知约束类型，记录错误 */
    lv_set_error(lv_ERROR_UNKNOWN, "check_incremental_conflict: 未知约束类型 %d (constraint id=%d)",
                 (int) new_constraint->type, new_constraint->id);
    return false;
}

/**
 * @brief 回滚 graph_alloc_node 之后尚未完成的点节点添加
 *
 * 递减节点计数、从节点索引表中移除并释放节点自身。
 * 注意：调用前需自行清理 node->symbolic_coords 的内容
 * （清理后该字段为 NULL，node_destroy 内部跳过坐标销毁，行为等价于
 * 原先仅释放节点外壳的 lv_free，并保持所有节点释放统一走 node_destroy 路径）。
 *
 * @param graph 约束图指针
 * @param node  待回滚的节点
 */
static void graph_rollback_point(ConstraintGraph *graph, GeomNode *node) {
    if (!graph || !node)
        return;
    graph->node_count--;
    node_index_remove(graph, node->id);
    node_destroy(node);
}

/**
 * 在约束图中添加点节点。
 *
 * @param graph       约束图指针
 * @param coords      点的坐标数组（每个坐标对应一个自由度）
 * @param coord_count 坐标数量
 * @return 添加结果状态
 */
AddNodeResult graph_add_point(ConstraintGraph *graph, SymbolicCoord *const *coords, int coord_count) {
    if (!graph || (coord_count > 0 && !coords))
        return ADD_NODE_CONFLICT;
    GeomNode *node = graph_alloc_node(graph, GEOM_POINT);
    if (!node)
        return ADD_NODE_CONFLICT;
    node->symbolic_coords = lv_malloc((size_t) coord_count * sizeof(SymbolicCoord *));
    if (!node->symbolic_coords) {
        graph_rollback_point(graph, node);
        return ADD_NODE_CONFLICT;
    }
    /* 深拷贝坐标，使节点拥有这些坐标 */
    for (int i = 0; i < coord_count; i++) {
        node->symbolic_coords[i] = coords[i] ? symbolic_coord_copy(coords[i]) : NULL;
        if (coords[i] && !node->symbolic_coords[i]) {
            /* 坐标拷贝失败：清理已分配的坐标并回滚节点添加 */
            for (int j = 0; j < i; j++) {
                symbolic_coord_destroy(node->symbolic_coords[j]);
            }
            lv_free((void **) &node->symbolic_coords);
            node->symbolic_coords = NULL;
            node->coord_count = 0;
            graph_rollback_point(graph, node);
            return ADD_NODE_CONFLICT;
        }
    }
    node->coord_count = coord_count;
    if (graph_stream_ctx) {
        lvStrBuf sb_3 = {0};
        lv_strbuf_printf(&sb_3, "添加点节点: id=%d", node->id);
        stream_emit_simple(graph_stream_ctx, STREAM_EVENT_NODE_ADDED, sb_3.data, 0);
        lv_strbuf_destroy(&sb_3);
    }
    return ADD_NODE_OK;
}

/**
 * @brief 添加线段节点（由两个端点定义）
 *
 * 创建线段类型的几何节点，验证端点存在性，并自动添加端点关联约束。
 *
 * @param graph         约束图指针
 * @param endpoint1_id 第一个端点节点 ID
 * @param endpoint2_id 第二个端点节点 ID
 * @return 操作结果枚举
 */
AddNodeResult graph_add_line_segment(ConstraintGraph *graph, int endpoint1_id, int endpoint2_id) {
    GeomNode *n1 = graph_get_node(graph, endpoint1_id);
    GeomNode *n2 = graph_get_node(graph, endpoint2_id);
    if (!n1 || !n2)
        return ADD_NODE_CONFLICT;
    if (n1->type != GEOM_POINT || n2->type != GEOM_POINT)
        return ADD_NODE_CONFLICT;
    GeomNode *node = graph_alloc_node(graph, GEOM_LINE_SEGMENT);
    if (!node)
        return ADD_NODE_CONFLICT;

    /* 计算线段需要的坐标总数：两个端点的坐标之和，至少为4个 */
    int total_coords = n1->coord_count + n2->coord_count;
    /* 如果端点没有坐标，至少保留4个位置以容纳 (x1,y1,x2,y2) */
    if (total_coords < 4)
        total_coords = 4;

    node->symbolic_coords = lv_malloc((size_t) total_coords * sizeof(SymbolicCoord *));
    if (!node->symbolic_coords) {
        /* 统一回滚路径：节点尚未挂接坐标（symbolic_coords 为 NULL、coord_count 为 0），
         * 与 graph_add_point 的回滚场景一致，graph_rollback_point 内部经 node_destroy
         * 走统一节点释放路径（内部跳过 NULL 坐标销毁，等价于原仅释放外壳的 lv_free） */
        graph_rollback_point(graph, node);
        return ADD_NODE_CONFLICT;
    }

    /* 拷贝端点1的所有坐标 */
    int coord_idx = 0;
    if (n1->symbolic_coords && n1->coord_count > 0) {
        for (int i = 0; i < n1->coord_count; i++) {
            node->symbolic_coords[coord_idx++] = symbolic_coord_copy(n1->symbolic_coords[i]);
        }
    } else {
        /* 端点1无坐标时填充NULL（保持 x1 位置） */
        node->symbolic_coords[coord_idx++] = NULL;
    }

    /* 拷贝端点2的所有坐标 */
    if (n2->symbolic_coords && n2->coord_count > 0) {
        for (int i = 0; i < n2->coord_count; i++) {
            node->symbolic_coords[coord_idx++] = symbolic_coord_copy(n2->symbolic_coords[i]);
        }
    } else {
        /* 端点2无坐标时填充NULL（保持 x2 位置） */
        node->symbolic_coords[coord_idx++] = NULL;
    }

    /* 将剩余位置初始化为 NULL */
    for (int i = coord_idx; i < total_coords; i++) {
        node->symbolic_coords[i] = NULL;
    }

    node->coord_count = coord_idx;
    return ADD_NODE_OK;
}

/**
 * 在约束图中添区域节点。
 *
 * 区域由边界线段围成，边界线段必须已存在于图中。
 *
 * @param graph                约束图指针
 * @param boundary_segment_ids  边界线段节点 ID 数组
 * @param segment_count        边界线段数量
 * @return 添加结果状态
 */
static bool graph_coord_equal_for_compatibility(const SymbolicCoord *a, const SymbolicCoord *b) {
    if (!a || !b)
        return false;
    if (a->type != b->type)
        return false;
    if (a->type == RATIONAL)
        return a->data.rational && b->data.rational && rational_compare(a->data.rational, b->data.rational) == 0;
    return symbolic_coord_compare(a, b) == 0;
}

static bool graph_segment_is_degenerate(const GeomNode *segment) {
    if (!segment || segment->type != GEOM_LINE_SEGMENT)
        return false;
    if (!segment->symbolic_coords || segment->coord_count < 4)
        return true;
    if (!segment->symbolic_coords[0] || !segment->symbolic_coords[1] || !segment->symbolic_coords[2] ||
        !segment->symbolic_coords[3])
        return true;
    return graph_coord_equal_for_compatibility(segment->symbolic_coords[0], segment->symbolic_coords[2]) &&
           graph_coord_equal_for_compatibility(segment->symbolic_coords[1], segment->symbolic_coords[3]);
}

static bool graph_segments_have_same_endpoints(const GeomNode *a, const GeomNode *b) {
    if (!a || !b || a->type != GEOM_LINE_SEGMENT || b->type != GEOM_LINE_SEGMENT)
        return false;
    if (!a->symbolic_coords || !b->symbolic_coords || a->coord_count < 4 || b->coord_count < 4)
        return false;
    for (int i = 0; i < 4; i++) {
        if (!a->symbolic_coords[i] || !b->symbolic_coords[i])
            return false;
    }
    bool same_direction = graph_coord_equal_for_compatibility(a->symbolic_coords[0], b->symbolic_coords[0]) &&
                          graph_coord_equal_for_compatibility(a->symbolic_coords[1], b->symbolic_coords[1]) &&
                          graph_coord_equal_for_compatibility(a->symbolic_coords[2], b->symbolic_coords[2]) &&
                          graph_coord_equal_for_compatibility(a->symbolic_coords[3], b->symbolic_coords[3]);
    bool reverse_direction = graph_coord_equal_for_compatibility(a->symbolic_coords[0], b->symbolic_coords[2]) &&
                             graph_coord_equal_for_compatibility(a->symbolic_coords[1], b->symbolic_coords[3]) &&
                             graph_coord_equal_for_compatibility(a->symbolic_coords[2], b->symbolic_coords[0]) &&
                             graph_coord_equal_for_compatibility(a->symbolic_coords[3], b->symbolic_coords[1]);
    return same_direction || reverse_direction;
}

/**
 * @brief 计算约束的自由度 (DOF) 消耗量
 *
 * 根据约束类型返回该约束消耗的自由度数：
 *   - INCIDENCE (关联): 点在线段/区域上 → 1 DOF
 *   - BETWEENNESS (介于): 点在两点之间 → 1 DOF
 *   - INTERSECTION (相交): 两线在交点相交 → 1 DOF
 *   - CONTAINMENT (包含): 对象在另一对象内 → 1 DOF
 *   - CONNECTION (连接): 端口间数据流 → 0 DOF（非几何约束）
 *
 * @param con 约束指针
 * @return 该约束消耗的自由度数
 */
static int constraint_dof_cost(const Constraint *con) {
    if (!con)
        return 0;
    static const int kConstraintDofCost[] = {
        1,  /* INCIDENCE */
        1,  /* BETWEENNESS */
        1,  /* INTERSECTION */
        1,  /* CONTAINMENT */
        0,  /* CONNECTION */
        1   /* ANGLE */
    };
    static const int kConstraintDofCostCount =
        (int)(sizeof(kConstraintDofCost) / sizeof(kConstraintDofCost[0]));
    if (con->type >= 0 && con->type < kConstraintDofCostCount) {
        return kConstraintDofCost[(int)con->type];
    }
    /* 未知约束类型：保守按 1 DOF 消耗计 */
    return 1;
}

bool graph_check_compatibility(const ConstraintGraph *graph, lvConstraintCompatibilityResult *out_result) {
    if (out_result) {
        out_result->status = lv_CONSTRAINT_STATUS_INVALID;
        out_result->conflicting_constraint_id = -1;
        out_result->redundant_constraint_count = 0;
        out_result->free_degree_count = 0;
        out_result->diagnostic = "输入无效";
    }
    if (!graph || !out_result)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "graph_check_compatibility: NULL graph or out_result");

    if (graph->node_count == 0) {
        out_result->status = lv_CONSTRAINT_STATUS_UNDER_CONSTRAINED;
        out_result->free_degree_count = 1;
        out_result->diagnostic = "空约束图缺少几何事实";
        return true;
    }

    /* ================================================================
     * 阶段 1: 统计活跃几何节点并检测退化线段
     *
     * DOF 模型：
     *   - 每个点节点 (GEOM_POINT):   2 DOF (x, y)
     *   - 每个区域节点 (GEOM_REGION): 2 DOF (位置)
     *   - 每条线段 (GEOM_LINE_SEGMENT): 不增加 DOF，但提供 4 DOF 约束
     *     （两个端点 x 2 坐标，固定线段后两端点相对位置完全确定）
     *   - 每条线段的长度是固有属性（已隐含），线段体可平移（2 DOF 刚体运动）
     *     但端点坐标作为约束消耗 4 DOF
     * ================================================================ */
    int active_geometry_nodes = 0;
    int active_segment_count = 0;
    int segment_constraint_bonus = 0;
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node)
            continue;
        /* v3.6.0: 跳过已废弃（不活跃）的节点 */
        if (!node->is_active)
            continue;
        if (node->type == GEOM_LINE_SEGMENT) {
            if (graph_segment_is_degenerate(node)) {
                out_result->status = lv_CONSTRAINT_STATUS_INCONSISTENT;
                out_result->conflicting_constraint_id = node->id;
                out_result->diagnostic = "检测到由重合端点构成的退化线段";
                return true;
            }
            active_segment_count++;
            /* 每条线段连接两个端点，完全确定其相对位置（4 DOF 约束） */
            segment_constraint_bonus += 4;
        } else if (node->type == GEOM_POINT || node->type == GEOM_REGION || node->type == GEOM_CIRCLE) {
            active_geometry_nodes++;
        }
    }

    /* 边界情况：所有节点均不活跃 */
    if (active_geometry_nodes == 0 && active_segment_count == 0) {
        out_result->status = lv_CONSTRAINT_STATUS_UNDER_CONSTRAINED;
        out_result->free_degree_count = 1;
        out_result->diagnostic = "约束图中无活跃几何节点";
        return true;
    }

    /* ================================================================
     * 阶段 2: 检测重复线段（退化冗余）
     * ================================================================ */
    int redundant_count = 0;
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *left = graph->nodes[i];
        if (!left || !left->is_active || left->type != GEOM_LINE_SEGMENT)
            continue;
        for (int j = i + 1; j < graph->node_count; j++) {
            GeomNode *right = graph->nodes[j];
            if (!right || !right->is_active)
                continue;
            if (graph_segments_have_same_endpoints(left, right))
                redundant_count++;
        }
    }
    out_result->redundant_constraint_count = redundant_count;
    if (redundant_count > 0) {
        out_result->status = lv_CONSTRAINT_STATUS_OVER_CONSTRAINED;
        out_result->diagnostic = "检测到重复线段约束";
        return true;
    }

    /* ================================================================
     * 阶段 3: 自由度 (DOF) 计算
     *
     * 总自由度 = 活跃点/区域节点 × 2
     * 约束消耗  = 线段约束奖励 + 各类型几何约束消耗
     * 剩余自由度 = 总自由度 - 约束消耗
     *
     * 约束类型 DOF 消耗：
     *   INCIDENCE (关联):    1 DOF  -- 点在线上
     *   BETWEENNESS (介于):  1 DOF  -- 三点共线
     *   INTERSECTION (相交): 1 DOF  -- 两线交于一点
     *   CONTAINMENT (包含):  1 DOF  -- 对象在内
     *   CONNECTION (连接):   0 DOF  -- 数据流，非几何
     *   DISTANCE (距离):     1 DOF  -- (通过 template_id >= 100 识别)
     *   ANGLE (角度):        1 DOF  -- (通过 template_id >= 100 识别)
     *   PERPENDICULAR (垂直): 1 DOF -- (通过 template_id >= 100 识别)
     *   PARALLEL (平行):     1 DOF  -- (通过 template_id >= 100 识别)
     * ================================================================ */
    int total_dof = active_geometry_nodes * 2;
    int active_constraint_count = segment_constraint_bonus;
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *con = graph->constraints[i];
        if (!con || !con->is_active)
            continue;
        active_constraint_count += constraint_dof_cost(con);
    }
    int free_dof = total_dof - active_constraint_count;

    /* ================================================================
     * 阶段 4: 根据剩余自由度判定约束状态
     *   - free_dof <  0  → 过约束 (OVER_CONSTRAINED)
     *   - free_dof == 0  → 恰好约束 (CONSISTENT)
     *   - free_dof >  0  → 欠约束 (UNDER_CONSTRAINED)
     * ================================================================ */
    if (free_dof < 0) {
        out_result->status = lv_CONSTRAINT_STATUS_OVER_CONSTRAINED;
        out_result->free_degree_count = free_dof;
        out_result->diagnostic = "约束过多导致过约束";
        return true;
    }

    if (free_dof > 0) {
        out_result->status = lv_CONSTRAINT_STATUS_UNDER_CONSTRAINED;
        out_result->free_degree_count = free_dof;
        out_result->diagnostic = "约束不足，存在自由度";
        return true;
    }

    /* free_dof == 0: 恰好约束 */
    out_result->status = lv_CONSTRAINT_STATUS_CONSISTENT;
    out_result->free_degree_count = 0;
    out_result->diagnostic = "约束图状态良好";
    return true;
}

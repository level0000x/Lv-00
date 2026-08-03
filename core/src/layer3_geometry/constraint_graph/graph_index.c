/**
 * @file graph_index.c
 * @brief GIndex 空间索引与哈希查找实现
 *
 * @details 实现约束图的多维度索引系统：
 *          - 节点哈希索引：O(1) 按 node_id 查找 GeomNode
 *          - 约束哈希索引：O(1) 按 constraint_id 查找 Constraint
 *          - 邻接矩阵：节点 → 关联约束列表的快速遍历
 *          - 函数块边界验证：检查跨命名空间约束引用的合法性
 *          - 增量冲突检查：新约束添加时的实时冲突检测
 *
 *          哈希策略采用开放地址法的线性探测（linear probing），
 *          capacity 按 2 的幂扩容以优化取模运算。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include <float.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/context.h"
#include "lv/stream.h"
#include "lv/symbolic_coord.h"

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "type_system.h"
#include "lv/lv_strbuf.h"

/* ── 流上下文声明 ── */
/* ── 前向声明（graph_node.c 中定义） ── */
bool constraint_exists(const ConstraintGraph *graph, ConstraintType type, const int *participants, int count);
Constraint *graph_alloc_constraint(ConstraintGraph *graph, ConstraintType type);
void constraint_index_remove(ConstraintGraph *graph, int constraint_id);
void node_index_remove(ConstraintGraph *graph, int node_id);
bool check_incremental_conflict(const ConstraintGraph *graph, const Constraint *new_constraint);
unsigned node_id_hash(int id, int capacity);
unsigned constraint_id_hash(int id, int capacity);

/**
 * 验证函数块的跨边界约束引用是否合法。
 * 检查命名空间深度和父块关系。
 *
 * @param func_block         函数块节点
 * @param internal_ids       内部节点 ID 数组
 * @param internal_count     内部节点数量
 * @param external_namespace 外部引用的命名空间深度
 * @param external_parent    外部引用的父块 ID
 * @param has_internal       是否有内部节点
 * @param has_external       是否有外部引用
 * @return true 表示引用合法，false 表示违反作用域规则
 */
static bool validate_cross_boundary_refs(GeomNode *func_block, int *internal_ids, int internal_count,
                                         int external_namespace, int external_parent, bool has_internal,
                                         bool has_external) {
    (void) internal_count; /* 未使用参数 */

    if (has_internal && has_external) {
        /* 检查作用域规则：
             * - 子块可以引用父块的公共节点
             * - 兄弟块不能相互引用私有节点
             * - namespace_depth 差异应最多为1才是有效引用
             */
        int block_namespace = func_block->namespace_depth;
        int block_parent = func_block->parent_block_id;

        /* 引用兄弟块内部节点是无效的 */
        if (external_namespace == block_namespace && external_parent != block_parent) {
            /* 相同深度但不同父块 - 兄弟块引用 */
            lv_free((void **) &internal_ids);
            lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_STATE, "Cross-boundary constraint references sibling block's private node");
        }

        /* 引用更深命名空间的节点是无效的 */
        if (external_namespace > block_namespace) {
            lv_free((void **) &internal_ids);
            lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_STATE, "Cross-boundary constraint references node from deeper namespace");
        }

        /* 命名空间深度差异 > 1 是无效的 */
        if (block_namespace - external_namespace > 1) {
            lv_free((void **) &internal_ids);
            lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_STATE, "Cross-boundary constraint spans more than one namespace level");
        }
    }

    lv_free((void **) &internal_ids);
    return true;
}

/**
 * 内部：为已分配的约束填充参与者数组（malloc + 复制）。
 *
 * 供 typed add_* 的共享分配器与反序列化（graph_add_constraint_with_id）复用。
 * 失败时返回 false 且 con->participants 保持 NULL，由调用方负责回滚约束本体
 * （typed 路径需同时回滚图计数与索引，with-id 路径仅释放约束）。
 *
 * @param con          已分配的约束指针（不得为 NULL）
 * @param participants 参与者节点 ID 数组
 * @param count        参与者数量
 * @return true 成功，false 分配失败
 */
bool graph_constraint_assign_participants(Constraint *con, const int *participants, int count) {
    con->participants = lv_malloc((size_t) count * sizeof(int));
    if (!con->participants)
        return false;
    memcpy(con->participants, participants, (size_t) count * sizeof(int));
    con->participant_count = count;
    return true;
}

/**
 * 内部：按类型分配约束并填充参与者（含查重与失败回滚），供 typed add_* 复用。
 *
 * 流程：constraint_exists 查重 → graph_alloc_constraint 分配 →
 * 参与者数组 malloc；分配参与者失败时回滚约束（撤销计数与索引注册）并返回冲突。
 *
 * @param graph        约束图指针
 * @param type         约束类型
 * @param participants 参与者节点 ID 数组
 * @param count        参与者数量
 * @param out_con      成功时输出新分配的约束指针，失败时置 NULL
 * @return 添加结果状态码（ADD_CONSTRAINT_OK / DUPLICATE / CONFLICT）
 */
static AddConstraintResult graph_add_constraint_typed(ConstraintGraph *graph, ConstraintType type,
                                                      const int *participants, int count, Constraint **out_con) {
    *out_con = NULL;
    if (constraint_exists(graph, type, participants, count))
        return ADD_CONSTRAINT_DUPLICATE;
    Constraint *con = graph_alloc_constraint(graph, type);
    if (!con)
        return ADD_CONSTRAINT_CONFLICT;
    if (!graph_constraint_assign_participants(con, participants, count)) {
        graph->constraint_count--;
        constraint_index_remove(graph, con->id);
        lv_free((void **) &con);
        return ADD_CONSTRAINT_CONFLICT;
    }
    *out_con = con;
    return ADD_CONSTRAINT_OK;
}

/**
 * 在约束图中添加关联约束（点在线/区域上）。
 *
 * @param graph           约束图指针
 * @param point_id       点节点 ID
 * @param line_or_region_id 线段或区域节点 ID
 * @return 添加结果状态
 */
AddConstraintResult graph_add_incidence(ConstraintGraph *graph, int point_id, int line_or_region_id) {
    GeomNode *point = graph_get_node(graph, point_id);
    GeomNode *target = graph_get_node(graph, line_or_region_id);
    if (!point || !target)
        return ADD_CONSTRAINT_CONFLICT;
    if (point->type != GEOM_POINT)
        return ADD_CONSTRAINT_CONFLICT;
    if (target->type != GEOM_LINE_SEGMENT && target->type != GEOM_REGION && target->type != GEOM_CIRCLE)
        return ADD_CONSTRAINT_CONFLICT;
    int participants[2] = {point_id, line_or_region_id};
    Constraint *con = NULL;
    AddConstraintResult result = graph_add_constraint_typed(graph, INCIDENCE, participants, 2, &con);
    if (result != ADD_CONSTRAINT_OK)
        return result;
    /* 增量代数冲突检查 */
    if (check_incremental_conflict(graph, con)) {
        LOG_WARN("constraint_graph",
                 "检测到 INCIDENCE 约束 %d 的增量冲突 "
                 "(点 %d 在线/区域 %d 上)",
                 con->id, point_id, line_or_region_id);
    }
    if (graph_stream_ctx) {
        lvStrBuf sb = {0};
        lv_strbuf_printf(&sb, "添加关联约束: id=%d, point=%d, target=%d", con->id, point_id, line_or_region_id);
        stream_emit_simple(graph_stream_ctx, STREAM_EVENT_CONSTRAINT_ADDED, sb.data, 0);
        lv_strbuf_destroy(&sb);
    }
    graph->dirty = true; /* v3.5.0: 约束被添加，标记脏状态 */
    return ADD_CONSTRAINT_OK;
}

/**
 * @brief 添加角度约束（两条线段之间的夹角）
 *
 * 声明两条线段在指定角度处相交。
 *
 * @param graph         约束图指针
 * @param line1_id      第一条线段 ID
 * @param line2_id      第二条线段 ID
 * @param angle_degrees 角度值（度）
 * @return 操作结果枚举
 */
AddConstraintResult graph_add_angle(ConstraintGraph *graph, int line1_id, int line2_id, double angle_degrees) {
    GeomNode *l1 = graph_get_node(graph, line1_id);
    GeomNode *l2 = graph_get_node(graph, line2_id);
    if (!l1 || !l2)
        return ADD_CONSTRAINT_CONFLICT;
    if (l1->type != GEOM_LINE_SEGMENT || l2->type != GEOM_LINE_SEGMENT)
        return ADD_CONSTRAINT_CONFLICT;
    int participants[2] = {line1_id, line2_id};
    Constraint *con = NULL;
    AddConstraintResult result = graph_add_constraint_typed(graph, ANGLE, participants, 2, &con);
    if (result != ADD_CONSTRAINT_OK)
        return result;
    con->numeric_value = angle_degrees;
    graph->dirty = true;
    return ADD_CONSTRAINT_OK;
}

/**
 * @brief 添加介于约束（B-A-C 共线有序）
 *
 * 声明点 B 在点 A 和点 C 之间，三点共线且有序。
 *
 * @param graph 约束图指针
 * @param p1_id 第一个端点 ID
 * @param p2_id 中间点 ID
 * @param p3_id 第二个端点 ID
 * @return 操作结果枚举
 */
AddConstraintResult graph_add_betweenness(ConstraintGraph *graph, int p1_id, int p2_id, int p3_id) {
    GeomNode *p1 = graph_get_node(graph, p1_id);
    GeomNode *p2 = graph_get_node(graph, p2_id);
    GeomNode *p3 = graph_get_node(graph, p3_id);
    if (!p1 || !p2 || !p3)
        return ADD_CONSTRAINT_CONFLICT;
    if (p1->type != GEOM_POINT || p2->type != GEOM_POINT || p3->type != GEOM_POINT) {
        return ADD_CONSTRAINT_CONFLICT;
    }
    int participants[3] = {p1_id, p2_id, p3_id};
    Constraint *con = NULL;
    AddConstraintResult result = graph_add_constraint_typed(graph, BETWEENNESS, participants, 3, &con);
    if (result != ADD_CONSTRAINT_OK)
        return result;
    /* 增量代数冲突检查 */
    if (check_incremental_conflict(graph, con)) {
        LOG_WARN("constraint_graph",
                 "检测到 BETWEENNESS 约束 %d 的增量冲突 "
                 "(点 %d-%d-%d 不共线)",
                 con->id, p1_id, p2_id, p3_id);
    }
    graph->dirty = true; /* v3.5.0: 约束被添加，标记脏状态 */
    return ADD_CONSTRAINT_OK;
}

/**
 * @brief 添加相交约束
 *
 * 声明两条线在指定交点处真交。
 *
 * @param graph           约束图指针
 * @param line1_id        第一条线 ID
 * @param line2_id        第二条线 ID
 * @param result_point_id 交点 ID
 * @return 操作结果枚举
 */
AddConstraintResult graph_add_intersection(ConstraintGraph *graph, int line1_id, int line2_id, int result_point_id) {
    GeomNode *l1 = graph_get_node(graph, line1_id);
    GeomNode *l2 = graph_get_node(graph, line2_id);
    GeomNode *pt = graph_get_node(graph, result_point_id);
    if (!l1 || !l2 || !pt)
        return ADD_CONSTRAINT_CONFLICT;
    if (l1->type != GEOM_LINE_SEGMENT || l2->type != GEOM_LINE_SEGMENT)
        return ADD_CONSTRAINT_CONFLICT;
    if (pt->type != GEOM_POINT)
        return ADD_CONSTRAINT_CONFLICT;
    int participants[3] = {line1_id, line2_id, result_point_id};
    Constraint *con = NULL;
    AddConstraintResult result = graph_add_constraint_typed(graph, INTERSECTION, participants, 3, &con);
    if (result != ADD_CONSTRAINT_OK)
        return result;
    /* 增量代数冲突检查 */
    if (check_incremental_conflict(graph, con)) {
        LOG_WARN("constraint_graph",
                 "检测到 INTERSECTION 约束 %d 的增量冲突 "
                 "(线 %d 和 %d 已在不同点相交)",
                 con->id, line1_id, line2_id);
    }
    graph->dirty = true; /* v3.5.0: 约束被添加，标记脏状态 */
    return ADD_CONSTRAINT_OK;
}

/**
 * @brief 添加包含约束
 *
 * 声明内部节点被包含在外部节点（区域）中。
 *
 * @param graph   约束图指针
 * @param inner_id 内部节点 ID
 * @param outer_id 外部节点 ID
 * @return 操作结果枚举
 */
AddConstraintResult graph_add_containment(ConstraintGraph *graph, int inner_id, int outer_id) {
    GeomNode *inner = graph_get_node(graph, inner_id);
    GeomNode *outer = graph_get_node(graph, outer_id);
    if (!inner || !outer)
        return ADD_CONSTRAINT_CONFLICT;
    if (inner->type != GEOM_POINT && inner->type != GEOM_REGION)
        return ADD_CONSTRAINT_CONFLICT;
    if (outer->type != GEOM_REGION && outer->type != GEOM_CIRCLE)
        return ADD_CONSTRAINT_CONFLICT;

    /* 添加包含约束时检查宇宙层级 */
    if (!type_check_universe_constraint(outer, inner)) {
        UniverseLevel outer_level = type_get_universe_level(outer);
        UniverseLevel inner_level = type_get_universe_level(inner);
        graph_set_error(graph, "违反宇宙层级约束: 区域层级 %d 必须高于内容层级 %d", (int) outer_level,
                        (int) inner_level);
        return ADD_CONSTRAINT_CONFLICT;
    }

    int participants[2] = {inner_id, outer_id};
    Constraint *con = NULL;
    AddConstraintResult result = graph_add_constraint_typed(graph, CONTAINMENT, participants, 2, &con);
    if (result != ADD_CONSTRAINT_OK)
        return result;
    graph->dirty = true; /* v3.5.0: 约束被添加，标记脏状态 */
    return ADD_CONSTRAINT_OK;
}

/**
 * 在约束图中添加连接约束（端口连接）。
 *
 * 连接约束必须是输出端口到输入端口，且命名空间深度相同或相差 1。
 *
 * @param graph        约束图指针
 * @param src_port_id 源端口节点 ID（必须是输出端口）
 * @param dst_port_id 目标端口节点 ID（必须是输入端口）
 * @return 添加结果状态
 */
AddConstraintResult graph_add_connection(ConstraintGraph *graph, int src_port_id, int dst_port_id) {
    GeomNode *src = graph_get_node(graph, src_port_id);
    GeomNode *dst = graph_get_node(graph, dst_port_id);
    if (!src || !dst)
        return ADD_CONSTRAINT_CONFLICT;
    if (src->type != GEOM_PORT || dst->type != GEOM_PORT)
        return ADD_CONSTRAINT_CONFLICT;
    Port *src_port = src->data.port;
    Port *dst_port = dst->data.port;
    if (src_port->type != PORT_OUTPUT || dst_port->type != PORT_INPUT) {
        return ADD_CONSTRAINT_CONFLICT;
    }
    if (src_port->namespace_depth != dst_port->namespace_depth &&
        abs(src_port->namespace_depth - dst_port->namespace_depth) != 1) {
        return ADD_CONSTRAINT_CONFLICT;
    }
    int participants[2] = {src_port_id, dst_port_id};
    Constraint *con = NULL;
    AddConstraintResult result = graph_add_constraint_typed(graph, CONNECTION, participants, 2, &con);
    if (result != ADD_CONSTRAINT_OK)
        return result;
    /* 建立双向连接关系 */
    dst_port->connected_to = src;
    src_port->connected_to = dst;
    graph->dirty = true; /* v3.5.0: 约束被添加，标记脏状态 */
    return ADD_CONSTRAINT_OK;
}

/**
 * @brief 从所有约束的参与者列表中移除对指定节点的引用
 *
 * 遍历约束图中的所有约束，将参与者列表中匹配 node_id 的条目
 * 移除，并相应减少 participant_count。此函数在删除节点时调用，
 * 确保约束数据与图的节点集合保持一致。
 *
 * @param graph   约束图指针
 * @param node_id 要移除引用的节点 ID
 */
static void remove_references_to_node(ConstraintGraph *graph, int node_id) {
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *con = graph->constraints[i];
        for (int j = 0; j < con->participant_count; j++) {
            if (con->participants[j] == node_id) {
                for (int k = j; k < con->participant_count - 1; k++) {
                    con->participants[k] = con->participants[k + 1];
                }
                con->participant_count--;
                j--;
            }
        }
    }
}

/* 前向声明 */
void node_destroy(GeomNode *node);

/**
 * @brief 检查节点是否属于某个区域的边界
 * @param graph 约束图指针
 * @param node_id 节点ID
 * @return 是返回 true，否则返回 false
 */
static bool node_in_region_boundary(const ConstraintGraph *graph, int node_id) {
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *n = graph->nodes[i];
        if (n->type == GEOM_REGION) {
            for (int j = 0; j < n->data.region.segment_count; j++) {
                if (n->data.region.boundary_segments[j] && n->data.region.boundary_segments[j]->id == node_id) {
                    return true;
                }
            }
        }
        if (n->type == GEOM_CIRCLE) {
            if (n->data.circle.center_node_id == node_id || n->data.circle.radius_node_id == node_id) {
                return true;
            }
        }
    }
    return false;
}

/**
 * @brief 从约束图中移除节点
 *
 * 移除指定节点及其所有关联约束，清理邻接表和哈希索引。
 *
 * @param graph   约束图指针
 * @param node_id 要移除的节点 ID
 * @return 操作结果枚举
 */
RemoveNodeResult graph_remove_node(ConstraintGraph *graph, int node_id) {
    GeomNode *node = graph_get_node(graph, node_id);
    if (!node)
        return REMOVE_NODE_NOT_FOUND;

    /* 检查节点是否属于某个区域的边界 */
    if (node_in_region_boundary(graph, node_id)) {
        return REMOVE_NODE_ERROR;
    }

    /* 清理交叉引用：将其他 PORT 的 connected_to 指向此节点的置为 NULL */
    if (node->type == GEOM_PORT) {
        for (int i = 0; i < graph->node_count; i++) {
            GeomNode *other = graph->nodes[i];
            if (other->type == GEOM_PORT && other->data.port && other->data.port->connected_to &&
                other->data.port->connected_to->id == node_id) {
                other->data.port->connected_to = NULL;
            }
        }
    }

    /* 清理交叉引用：将此节点从所有 FUNCTION_BLOCK 的 internal_nodes 中移除 */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *fb = graph->nodes[i];
        if (fb->type == GEOM_FUNCTION_BLOCK && fb->data.func_block.internal_nodes) {
            for (int j = 0; j < fb->data.func_block.internal_node_count; j++) {
                if (fb->data.func_block.internal_nodes[j] && fb->data.func_block.internal_nodes[j]->id == node_id) {
                    fb->data.func_block.internal_nodes[j] = NULL;
                }
            }
        }
    }

    /* 移除所有引用此节点的约束 */
    for (int i = graph->constraint_count - 1; i >= 0; i--) {
        Constraint *con = graph->constraints[i];
        bool references_node = false;
        for (int j = 0; j < con->participant_count; j++) {
            if (con->participants[j] == node_id) {
                references_node = true;
                break;
            }
        }
        if (references_node) {
            int cid = con->id;
            lv_free((void **) &con->participants);
            lv_free((void **) &con);
            constraint_index_remove(graph, cid);
            for (int k = i; k < graph->constraint_count - 1; k++) {
                graph->constraints[k] = graph->constraints[k + 1];
            }
            graph->constraint_count--;
            graph->dirty = true; /* v3.5.0: 约束被移除，标记脏状态 */
        }
    }

    /* 从数组中移除节点（压缩） */
    for (int i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i]->id == node_id) {
            /* 在销毁节点之前先从哈希索引中移除，
             * 因为 node_index_remove 会访问其他条目的节点指针 */
            node_index_remove(graph, node_id);
            node_destroy(graph->nodes[i]);
            for (int j = i; j < graph->node_count - 1; j++) {
                graph->nodes[j] = graph->nodes[j + 1];
            }
            graph->node_count--;
            if (graph_stream_ctx) {
                lvStrBuf sb_2 = {0};
                lv_strbuf_printf(&sb_2, "移除节点: id=%d", node_id);
                stream_emit_node_event(graph_stream_ctx, STREAM_EVENT_INFO, node_id, sb_2.data, 0);
                lv_strbuf_destroy(&sb_2);
            }
            return REMOVE_NODE_OK;
        }
    }
    return REMOVE_NODE_NOT_FOUND;
}

/**
 * @brief 从约束图中移除指定索引处的约束
 *
 * @param graph           约束图指针
 * @param constraint_index 约束在数组中的索引
 * @return 操作结果枚举
 */
RemoveConstraintResult graph_remove_constraint(ConstraintGraph *graph, int constraint_index) {
    if (constraint_index < 0 || constraint_index >= graph->constraint_count) {
        return REMOVE_CONSTRAINT_NOT_FOUND;
    }
    Constraint *con = graph->constraints[constraint_index];
    int cid = con->id;

    /* 清理交叉引用：如果是 CONNECTION 约束，清理 dst_port 的 connected_to */
    if (con->type == CONNECTION && con->participant_count == 2) {
        GeomNode *dst = graph_get_node(graph, con->participants[1]);
        if (dst && dst->type == GEOM_PORT && dst->data.port) {
            dst->data.port->connected_to = NULL;
        }
    }

    /* 先从哈希索引中移除，再释放约束内存（避免 use-after-free） */
    constraint_index_remove(graph, cid);
    lv_free((void **) &con->participants);
    lv_free((void **) &con);
    for (int i = constraint_index; i < graph->constraint_count - 1; i++) {
        graph->constraints[i] = graph->constraints[i + 1];
    }
    graph->constraint_count--;
    graph->dirty = true; /* v3.5.0: 约束被移除，标记脏状态 */
    if (graph_stream_ctx) {
        lvStrBuf sb_3 = {0};
        lv_strbuf_printf(&sb_3, "移除约束: id=%d", cid);
        stream_emit_constraint_event(graph_stream_ctx, STREAM_EVENT_INFO, cid, sb_3.data, 0);
        lv_strbuf_destroy(&sb_3);
    }
    return REMOVE_CONSTRAINT_OK;
}

/* ============================================================
 * v3.5.0: 脏标记传播与约束生命周期管理
 * ============================================================ */

/**
 * @brief 标记约束图为脏状态
 *
 * 约束被修改（添加/删除/废弃）时调用，设置 dirty 标记。
 * 后续 graph_sync_nodes() 会遍历受影响节点并刷新属性。
 *
 * @param graph 约束图指针
 */
void graph_mark_dirty(ConstraintGraph *graph) {
    if (!graph)
        return;
    graph->dirty = true;
}

/**
 * @brief 同步约束图中所有受影响节点的属性
 *
 * 遍历所有节点，刷新受约束影响的属性（如 trust 等级、
 * 数值精度等）。同步完成后重置 dirty 标记。
 *
 * @param graph 约束图指针
 */
void graph_sync_nodes(ConstraintGraph *graph) {
    if (!graph)
        return;
    if (!graph->dirty)
        return; /* 无变更，无需同步 */

    /* 遍历所有活跃约束，传播约束信息到受影响节点 */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        if (!c || !c->is_active)
            continue;

        /* 刷新每个参与节点：根据约束类型调整 trust 等级 */
        for (int j = 0; j < c->participant_count; j++) {
            GeomNode *node = graph_get_node(graph, c->participants[j]);
            if (!node)
                continue;

            /* 基于约束类型调整信任等级 */
            switch (c->type) {
                case INCIDENCE:
                case BETWEENNESS:
                case INTERSECTION:
                    /* 几何约束对精度要求高，保持或提升 trust */
                    if (node->trust > TRUST_GREEN)
                        node->trust = TRUST_GREEN;
                    break;
                case CONTAINMENT:
                case CONNECTION:
                case ANGLE:
                    /* 拓扑/数值约束允许较低的 trust */
                    break;
                default:
                    lv_LOG_ERROR("graph_sync_nodes: 未知约束类型 %d (id=%d)", (int) c->type, c->id);
                    break;
            }
        }
    }

    graph->dirty = false;
}

/**
 * @brief 废弃约束（惰性删除）
 *
 * 将约束标记为不活跃（is_active = false），从活跃约束索引中移除，
 * 但保留其数据以便审计跟踪。活跃约束迭代时自动跳过不活跃约束。
 *
 * @param graph         约束图指针
 * @param constraint_id 要废弃的约束 ID
 * @return lv_OK 成功，其他错误码表示失败
 */
int graph_deactivate_constraint(ConstraintGraph *graph, int constraint_id) {
    if (!graph)
        return lv_ERROR_INVALID_PARAM;

    Constraint *con = graph_get_constraint(graph, constraint_id);
    if (!con) {
        lv_set_error(lv_ERROR_NOT_FOUND, "graph_deactivate_constraint: 约束 #%d 未找到", constraint_id);
        return lv_ERROR_NOT_FOUND;
    }
    if (!con->is_active) {
        lv_set_error(lv_ERROR_UNKNOWN, "graph_deactivate_constraint: 约束 #%d 已经是不活跃状态", constraint_id);
        return lv_ERROR_UNKNOWN;
    }

    /* 标记为不活跃 */
    con->is_active = false;

    /* 从哈希索引中移除（保留约束数据用于审计） */
    constraint_index_remove(graph, constraint_id);

    /* 标记图为脏状态，需要同步 */
    graph_mark_dirty(graph);

    LOG_INFO("constraint_graph", "约束 #%d (类型=%d) 已废弃，保留数据用于审计跟踪", constraint_id, (int) con->type);

    if (graph_stream_ctx) {
        lvStrBuf sb_4 = {0};
        lv_strbuf_printf(&sb_4, "废弃约束: id=%d (已停用，保留审计数据)", constraint_id);
        stream_emit_constraint_event(graph_stream_ctx, STREAM_EVENT_INFO, constraint_id, sb_4.data, 0);
        lv_strbuf_destroy(&sb_4);
    }

    return lv_OK;
}

/**
 * 查找涉及指定节点的所有约束。
 *
 * @param graph        约束图指针
 * @param node_id      节点 ID
 * @param out_indices 输出：约束索引数组
 * @param max_results 数组最大容量
 * @return 找到的约束数量
 */
int graph_find_constraints_involving(const ConstraintGraph *graph, int node_id, int *out_indices, int max_results) {
    if (!graph || !out_indices || max_results <= 0)
        return 0;
    int count = 0;
    for (int i = 0; i < graph->constraint_count && count < max_results; i++) {
        Constraint *c = graph->constraints[i];
        if (!c->is_active) /* v3.5.0: 跳过不活跃约束 */
            continue;
        for (int j = 0; j < c->participant_count; j++) {
            if (c->participants[j] == node_id) {
                out_indices[count++] = i;
                break;
            }
        }
    }
    return count;
}

/**
 * 检测约束是否冗余。
 *
 * @param graph       约束图指针
 * @param type        约束类型
 * @param participants 参与者节点 ID 数组
 * @param n_parts    参与者数量
 * @return 1 表示冗余，0 表示不冗余，-1 表示错误
 */
int graph_detect_redundancy(const ConstraintGraph *graph, ConstraintType type, const int *participants, int n_parts) {
    if (!graph || !participants || n_parts <= 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "graph_detect_redundancy: invalid params (graph=%p, participants=%p, n_parts=%d)",
                        (const void *)graph, (const void *)participants, n_parts);
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        if (!c->is_active) /* v3.5.0: 跳过不活跃约束 */
            continue;
        if (c->type != type || c->participant_count != n_parts)
            continue;
        bool same = true;
        for (int j = 0; j < n_parts; j++) {
            if (c->participants[j] != participants[j]) {
                same = false;
                break;
            }
        }
        if (same)
            return 1;
    }
    return 0;
}

/**
 * 获取约束图中的节点数量。
 *
 * @param graph 约束图指针
 * @return 节点数量
 */
int graph_get_node_count(const ConstraintGraph *graph) {
    if (!graph)
        return 0;
    return graph->node_count;
}

/**
 * @brief 获取约束图中的约束数量
 *
 * @param graph 约束图指针
 * @return 约束数量
 */
int graph_get_constraint_count(const ConstraintGraph *graph) {
    if (!graph)
        return 0;
    return graph->constraint_count;
}

/**
 * 通过节点 ID 获取节点（线性扫描版本）。
 *
 * @param graph   约束图指针
 * @param node_id 节点 ID
 * @return 节点指针，不存在时返回 NULL
 */
GeomNode *graph_get_node_by_id(const ConstraintGraph *graph, int node_id) {
    if (!graph)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "graph_get_node_by_id: graph is NULL");
    for (int i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i]->id == node_id)
            return graph->nodes[i];
    }
    return NULL;
}

GeomNode *graph_get_node(const ConstraintGraph *graph, int node_id) {
    if (!graph)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "graph_get_node: graph is NULL");
    if (graph->node_index) {
        unsigned idx = node_id_hash(node_id, graph->node_index_capacity);
        while (graph->node_index[idx] != NULL) {
            if (graph->node_index[idx]->id == node_id)
                return graph->node_index[idx];
            idx = (idx + 1) & (unsigned) (graph->node_index_capacity - 1);
        }
        return NULL;
    }
    /* Fallback to linear scan if index not built */
    for (int i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i]->id == node_id)
            return graph->nodes[i];
    }
    return NULL;
}

Constraint *graph_get_constraint(const ConstraintGraph *graph, int constraint_id) {
    if (!graph)
        return NULL;
    if (graph->constraint_index) {
        unsigned idx = constraint_id_hash(constraint_id, graph->constraint_index_capacity);
        while (graph->constraint_index[idx] != NULL) {
            if (graph->constraint_index[idx]->id == constraint_id)
                return graph->constraint_index[idx];
            idx = (idx + 1) & (unsigned) (graph->constraint_index_capacity - 1);
        }
        return NULL;
    }
    /* Fallback to linear scan if index not built */
    for (int i = 0; i < graph->constraint_count; i++) {
        if (graph->constraints[i]->id == constraint_id)
            return graph->constraints[i];
    }
    return NULL;
}

/**
 * 创建新的约束图。
 *
 * @return 新创建的约束图，失败时返回 NULL；调用者需负责释放
 */
ConstraintGraph *graph_create(void) {
    ConstraintGraph *graph = lv_calloc(1, sizeof(ConstraintGraph));
    if (!graph)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "graph_create: calloc failed");
    graph->next_node_id = 0;
    graph->next_constraint_id = 0;
    graph->dirty = false; /* v3.5.0: 脏标记初始化为 false */

    /* ============================================================================
     * 遗留缓冲区说明 (v3.4.0 计划清理)
     * ============================================================================
     * error_buffer 和 serialize_buffer 是 v3.3.0 引入的每图级错误缓冲区，
     * 用于替代旧版静态全局变量，提升并发安全性。
     *
     * 迁移计划 (v3.4.0):
     * - 当 lvContext 统一错误系统完全就绪后，这些缓冲区将逐步迁移
     *   到 context->error_message[] 数组中统一管理。
     * - graph_set_error() / graph_get_error() 已优先使用 context 错误存储，
     *   这些缓冲区仅作为 fallback 保留。
     * - 当前保留是为了向后兼容，避免破坏已有调用代码。
     *
     * 风险评估: 无运行时风险，仅为架构演进预留的占位代码。
     * ============================================================================ */
    graph->error_buffer = lv_malloc(256);
    graph->serialize_buffer = lv_malloc(256);
    if (!graph->error_buffer || !graph->serialize_buffer) {
        lv_free((void **) &graph->error_buffer);
        lv_free((void **) &graph->serialize_buffer);
        lv_free((void **) &graph);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "graph_create: buffer allocation failed");
    }
    graph->error_buffer[0] = '\0';
    graph->serialize_buffer[0] = '\0';

    return graph;
}

/* ============================================================
 * 错误码转换函数
 * ============================================================ */

lvErrorCode lv_add_node_result_to_error(AddNodeResult result) {
    switch (result) {
        case ADD_NODE_OK:
            return lv_OK;
        case ADD_NODE_CONFLICT:
            return lv_ERROR_NODE_CONFLICT;
        case ADD_NODE_INVALID_REGION:
            return lv_ERROR_INVALID_REGION;
        default:
            return lv_ERROR_UNKNOWN;
    }
}

/**
 * 将添加约束结果转换为错误码。
 *
 * @param result 添加约束结果
 * @return 对应的错误码
 */
lvErrorCode lv_add_constraint_result_to_error(AddConstraintResult result) {
    switch (result) {
        case ADD_CONSTRAINT_OK:
            return lv_OK;
        case ADD_CONSTRAINT_DUPLICATE:
            return lv_ERROR_CONSTRAINT_DUPLICATE;
        case ADD_CONSTRAINT_CONFLICT:
            return lv_ERROR_CONSTRAINT_CONFLICT;
        default:
            return lv_ERROR_UNKNOWN;
    }
}

lvErrorCode lv_remove_node_result_to_error(RemoveNodeResult result) {
    switch (result) {
        case REMOVE_NODE_OK:
            return lv_OK;
        case REMOVE_NODE_NOT_FOUND:
            return lv_ERROR_NODE_NOT_FOUND;
        case REMOVE_NODE_ERROR:
            return lv_ERROR_GRAPH_CORRUPTED;
        default:
            return lv_ERROR_UNKNOWN;
    }
}

/* ============================================================
 * 统一错误系统实现 (v3.4.0: 迁移到 lvContext)
 *
 * 优先使用 graph->context->error_message，fallback 到
 * graph->error_buffer。
 * ============================================================ */

/**
 * @brief 设置约束图的错误信息 (v3.4.0: 支持 lvContext)
 *
 * 优先将错误信息存储到 graph->context->error_message 中
 * (如果有 context)，fallback 到 graph->error_buffer。
 *
 * @param graph 约束图（可以为 NULL，但错误信息不会被存储）
 * @param fmt   printf 风格的格式字符串
 * @param ...   可变参数列表
 */
void graph_set_error(ConstraintGraph *graph, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    if (graph) {
        /* 优先使用 context 错误存储 */
        if (graph->context) {
            vsnprintf(graph->context->error_message, sizeof(graph->context->error_message), fmt, args);
        } else if (graph->error_buffer) {
            /* Fallback 到 error_buffer */
            vsnprintf(graph->error_buffer, 256, fmt, args);
        } else {
            /* 两者都不可用，记录到全局错误 API */
            char fallback[256];
            vsnprintf(fallback, sizeof(fallback), fmt, args);
            lv_set_error(lv_ERROR_UNKNOWN, "%s", fallback);
        }
    }

    va_end(args);
}

/**
 * @brief 获取约束图的错误信息 (v3.4.0: 支持 lvContext)
 *
 * 优先从 graph->context->error_message 读取错误信息
 * (如果有 context 且有错误)，fallback 到 graph->error_buffer。
 *
 * @param graph 约束图（可以为 NULL，返回 "NULL graph"）
 * @return 错误信息字符串（内部存储，勿 free）
 */
const char *graph_get_error(const ConstraintGraph *graph) {
    if (!graph) {
        return "NULL graph";
    }

    /* 优先从 context 读取错误信息 */
    if (graph->context && graph->context->error_message[0]) {
        return graph->context->error_message;
    }

    /* Fallback 到 error_buffer */
    if (graph->error_buffer && graph->error_buffer[0]) {
        return graph->error_buffer;
    }

    return "";
}

/**
 * @brief 销毁几何节点并释放其所有资源
 *
 * 根据节点类型释放对应的内部数据：
 * - GEOM_FUNCTION_BLOCK：释放内部节点数组、输入/输出端口 ID 数组
 * - 所有类型：释放符号坐标数组和数值假设声明字符串
 * 最后释放节点结构体本身。
 *
 * @param node 要销毁的几何节点指针
 */
void node_destroy(GeomNode *node) {
    if (!node)
        return;
    if (node->symbolic_coords) {
        for (int i = 0; i < node->coord_count; i++) {
            symbolic_coord_destroy(node->symbolic_coords[i]);
        }
        lv_free((void **) &node->symbolic_coords);
    }
    if (node->numeric_assumption_declaration) {
        lv_free((void **) &node->numeric_assumption_declaration);
        node->numeric_assumption_declaration = NULL;
    }
    /* 通过 vtable 释放类型特定的数据 */
    if (node->vtable && node->vtable->free) {
        node->vtable->free(node);
    }
    lv_free((void **) &node);
}

/**
 * @brief 获取约束图中最后添加的节点 ID
 *
 * 返回节点数组中最后一个节点的索引（即 node_count - 1）。
 * 注意：此 ID 是数组索引而非节点的逻辑 ID。
 *
 * @param graph 约束图指针
 * @return 最后节点的索引，图为空时返回 -1
 */
int graph_get_last_node_id(const ConstraintGraph *graph) {
    if (!graph || graph->node_count == 0)
        return -1;
    return graph->node_count - 1;
}

/**
 * @file beta_reduce.c
 * @brief β-归约端口继承规则核心实现
 *
 * 实现 λ-演算 β-归约对应的约束图变换操作，使用三字段端口继承规则
 * （parent_block_id + is_formal_param + namespace_depth）处理变量捕获。
 *
 * 端口继承规则：
 * - 情况 A（形式参数引用）：parent_block_id == func_block_id && is_formal_param == true
 *   → 重定向到实参输出端口
 * - 情况 B（自由变量引用）：parent_block_id != func_block_id
 *   → 保持原目标不变
 * - 情况 C（内部局部引用）：parent_block_id == func_block_id && is_formal_param == false
 *   → 重映射到复制件对应新端口
 */

#include "lv/constraint_graph.h"
#include "lv/func_block.h"
#include "lv/lambda_term.h"
#include "lv_internal.h"
#include "debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===========================================================================
 * 内部辅助：获取约束数组中某约束的索引位置
 * =========================================================================== */

/**
 * @brief 在约束图中查找约束 ID 对应的数组索引
 *
 * @param graph        约束图
 * @param constraint_id 约束 ID
 * @return 约束数组索引，未找到返回 -1
 */
static int constraint_index_by_id(const ConstraintGraph *graph, int constraint_id) {
    for (int i = 0; i < graph->constraint_count; i++) {
        if (graph->constraints[i] && graph->constraints[i]->id == constraint_id)
            return i;
    }
    return -1;
}

/**
 * @brief 检查节点是否为 func_block 的形式参数端口
 *
 * @param node           节点
 * @param func_block_id  函数块 ID
 * @return true 如果该节点是 func_block 的形式参数端口
 */
static bool is_formal_param_port(const GeomNode *node, int func_block_id) {
    if (!node) return false;
    if (node->type != GEOM_PORT) return false;
    if (!node->data.port) return false;
    return (node->parent_block_id == func_block_id && node->data.port->is_formal_param);
}

/**
 * @brief 检查节点是否是 func_block 的内部局部节点
 *
 * @param node           节点
 * @param func_block_id  函数块 ID
 * @return true 如果该节点是 func_block 的内部局部节点
 */
static bool is_internal_local_node(const GeomNode *node, int func_block_id) {
    if (!node) return false;
    /* 父块是 func_block 且不是形式参数（包括端口和几何节点） */
    if (node->parent_block_id != func_block_id) return false;
    if (node->type == GEOM_PORT && node->data.port && node->data.port->is_formal_param)
        return false;
    return true;
}

/**
 * @brief 检查节点是否为自由变量引用（父块不是 func_block）
 *
 * @param node           节点
 * @param func_block_id  函数块 ID
 * @return true 如果该节点是自由变量（parent_block_id != func_block_id）
 */
static bool is_free_variable_ref(const GeomNode *node, int func_block_id) {
    if (!node) return false;
    return (node->parent_block_id != func_block_id);
}

/* ===========================================================================
 * 内部辅助：创建节点的深拷贝（适配 graph_add_* 公共 API）
 * =========================================================================== */

/**
 * @brief 创建内部节点的复制件到目标图
 *
 * 根据原节点类型创建对应类型的新节点，复制坐标等公共数据。
 * 对于 PORT 节点，同时复制 is_formal_param 等端口属性。
 *
 * @param graph  目标约束图
 * @param source 原节点（只读）
 * @return 新节点的 ID，失败返回 -1
 */
static int duplicate_node(ConstraintGraph *graph, const GeomNode *source) {
    if (!graph || !source) return -1;

    switch (source->type) {
    case GEOM_POINT: {
        /* 复制 POINT 节点 */
        SymbolicCoord **coords = NULL;
        if (source->coord_count > 0 && source->symbolic_coords) {
            coords = lv_calloc((size_t)source->coord_count, sizeof(SymbolicCoord *));
            if (!coords) return -1;
            for (int c = 0; c < source->coord_count; c++) {
                coords[c] = symbolic_coord_copy(source->symbolic_coords[c]);
                if (!coords[c]) {
                    for (int j = 0; j < c; j++)
                        symbolic_coord_destroy(coords[j]);
                    lv_free((void**)&coords);
                    return -1;
                }
            }
        }
        AddNodeResult nr = graph_add_point(graph,
            (SymbolicCoord *const *)coords, source->coord_count);
        /* 不论成功失败，释放坐标副本（graph_add_point 内部会做拷贝） */
        if (coords) {
            for (int c = 0; c < source->coord_count; c++)
                symbolic_coord_destroy(coords[c]);
            lv_free((void**)&coords);
        }
        if (nr != ADD_NODE_OK) return -1;
        int new_id = graph_get_last_added_node_id(graph);
        /* 复制 namespace_depth 和 parent_block_id */
        GeomNode *new_node = graph_get_node(graph, new_id);
        if (new_node) {
            new_node->namespace_depth = source->namespace_depth;
            new_node->parent_block_id = source->parent_block_id;
        }
        return new_id;
    }

    case GEOM_PORT: {
        /* 复制 PORT 节点 */
        if (!source->data.port) return -1;
        PortType pt = source->data.port->type;
        int depth = source->namespace_depth;
        int parent = source->parent_block_id;
        AddNodeResult nr = graph_add_port(graph, pt, depth, parent);
        if (nr != ADD_NODE_OK) return -1;
        int new_id = graph_get_last_added_node_id(graph);
        GeomNode *new_node = graph_get_node(graph, new_id);
        if (new_node && new_node->data.port) {
            /* 复制端口属性 */
            new_node->data.port->is_formal_param = source->data.port->is_formal_param;
            new_node->data.port->is_polymorphic = source->data.port->is_polymorphic;
            /* 不复制 connected_to（后续通过约束重建） */
            new_node->data.port->connected_to = NULL;
        }
        return new_id;
    }

    case GEOM_LINE_SEGMENT: {
        /* 线段节点的复制需要端点信息。
         * 由于端点也是内部节点，会在后续约束重建时处理。
         * 此处创建一个占位线段（使用临时端点），
         * 后续通过约束拷贝建立连接。 */
        /* 创建临时端点 */
        SymbolicCoord *zc = symbolic_coord_create_rational(0, 1);
        SymbolicCoord *tmp_coords[] = {zc};
        AddNodeResult nr = graph_add_point(graph, tmp_coords, 1);
        symbolic_coord_destroy(zc);
        if (nr != ADD_NODE_OK) return -1;
        int ep1_id = graph_get_last_added_node_id(graph);

        zc = symbolic_coord_create_rational(0, 1);
        SymbolicCoord *tmp_coords2[] = {zc};
        nr = graph_add_point(graph, tmp_coords2, 1);
        symbolic_coord_destroy(zc);
        if (nr != ADD_NODE_OK) return -1;
        int ep2_id = graph_get_last_added_node_id(graph);

        nr = graph_add_line_segment(graph, ep1_id, ep2_id);
        if (nr != ADD_NODE_OK) return -1;
        int new_id = graph_get_last_added_node_id(graph);
        GeomNode *new_node = graph_get_node(graph, new_id);
        if (new_node) {
            new_node->namespace_depth = source->namespace_depth;
            new_node->parent_block_id = source->parent_block_id;
        }
        return new_id;
    }

    case GEOM_REGION: {
        /* 区域节点：创建空区域，边界通过后续约束重建 */
        int empty_segs[] = {0};
        AddNodeResult nr = graph_add_region(graph, empty_segs, 0);
        if (nr != ADD_NODE_OK) return -1;
        int new_id = graph_get_last_added_node_id(graph);
        GeomNode *new_node = graph_get_node(graph, new_id);
        if (new_node) {
            new_node->namespace_depth = source->namespace_depth;
            new_node->parent_block_id = source->parent_block_id;
        }
        return new_id;
    }

    default:
        LOG_WARN("beta_reduce", "duplicate_node: 不支持的节点类型 %d", (int)source->type);
        return -1;
    }
}

/* ===========================================================================
 * 内部辅助：为内部节点 IDs 创建 ID 映射表
 * =========================================================================== */

/**
 * @brief 为内部节点分配新的 ID 映射
 *
 * 根据三字段规则确定每个内部节点的目标映射：
 * - Case A：形式参数 → 映射到实参 ID
 * - Case B：自由变量 → 映射到自身（保持原目标）
 * - Case C：内部局部 → 创建副本并映射到新 ID
 *
 * @param graph            约束图（用于创建副本）
 * @param func_block_id    函数块 ID
 * @param internal_ids     内部节点 ID 数组
 * @param internal_count   内部节点数量
 * @param arg_node_id      实参节点 ID
 * @param out_id_map       输出：ID 映射表（由调用者分配，大小至少为 graph->next_node_id）
 *                         初始化为 -1；调用者需保证 out_id_map 在调用前已 memset -1
 * @return 新创建的内部局部节点副本数量（Case C 的数量），失败返回 -1
 */
static int build_id_mapping(ConstraintGraph *graph, int func_block_id,
                             const int *internal_ids, int internal_count,
                             int arg_node_id, int *out_id_map)
{
    if (!graph || !internal_ids || !out_id_map) return -1;

    int copy_count = 0;
    for (int i = 0; i < internal_count; i++) {
        int old_id = internal_ids[i];
        if (old_id < 0) continue;

        GeomNode *node = graph_get_node(graph, old_id);
        if (!node) continue;

        if (node->parent_block_id == func_block_id) {
            if (node->type == GEOM_PORT && node->data.port &&
                node->data.port->is_formal_param) {
                /* 情况 A：形式参数 → 重定向到实参 */
                out_id_map[old_id] = arg_node_id;
                LOG_DEBUG("beta_reduce", "  [A] 形式参数 port=%d → 重定向到实参 %d",
                          old_id, arg_node_id);
            } else {
                /* 情况 C：内部局部 → 创建副本 */
                int new_id = duplicate_node(graph, node);
                if (new_id < 0) {
                    LOG_ERROR("beta_reduce", "复制内部节点 %d 失败", old_id);
                    return -1;
                }
                out_id_map[old_id] = new_id;
                copy_count++;
                LOG_DEBUG("beta_reduce", "  [C] 内部局部 %d (type=%d) → 副本 %d",
                          old_id, (int)node->type, new_id);
            }
        } else {
            /* 情况 B：自由变量 → 保持原目标 */
            out_id_map[old_id] = old_id;
            LOG_DEBUG("beta_reduce", "  [B] 自由变量 %d → 保持原目标 %d",
                      old_id, old_id);
        }
    }

    return copy_count;
}

/* ===========================================================================
 * 内部辅助：重建/重定向内部约束
 * =========================================================================== */

/**
 * @brief 查找并重建涉及内部节点的所有约束
 *
 * 遍历图中所有约束，对每个参与者包含内部节点的约束：
 * - 使用 ID 映射表将内部节点 ID 替换为新 ID
 * - 重新创建约束
 *
 * @param graph          约束图
 * @param internal_ids   内部节点 ID 数组
 * @param internal_count 内部节点数量
 * @param id_map         ID 映射表（old_id → new_id）
 * @return true 全部成功，false 部分失败
 */
static bool remap_internal_constraints(ConstraintGraph *graph,
                                        const int *internal_ids, int internal_count,
                                        const int *id_map)
{
    if (!graph || !internal_ids || !id_map) return false;

    /* 先找出涉及内部节点的约束 */
    int max_constraints = graph->constraint_count;
    int *affected_target_ids = NULL;
    int affected_count = 0;

    /* 标记哪些旧 ID 需要映射 */
    bool *needs_remap = lv_calloc((size_t)graph->next_node_id, sizeof(bool));
    if (!needs_remap) return false;
    memset(needs_remap, 0, (size_t)graph->next_node_id * sizeof(bool));

    for (int i = 0; i < internal_count; i++) {
        int old_id = internal_ids[i];
        if (old_id >= 0 && old_id < graph->next_node_id) {
            needs_remap[old_id] = true;
        }
    }

    /* 收集并重建约束 */
    for (int ci = 0; ci < graph->constraint_count; ci++) {
        Constraint *con = graph->constraints[ci];
        if (!con || !con->is_active) continue;

        /* 检查是否涉及任何内部节点 */
        bool involves_internal = false;
        for (int p = 0; p < con->participant_count; p++) {
            int pid = con->participants[p];
            if (pid >= 0 && pid < graph->next_node_id && needs_remap[pid]) {
                involves_internal = true;
                break;
            }
        }

        if (!involves_internal) continue;

        /* 重建约束：解析参与者 ID */
        int resolved_participants[8];
        if (con->participant_count > 8) {
            LOG_WARN("beta_reduce", "约束 %d 参与者数量 %d 超过上限 8",
                     con->id, con->participant_count);
            continue;
        }

        bool skip_constraint = false;
        for (int p = 0; p < con->participant_count; p++) {
            int pid = con->participants[p];
            /* 查找映射后的 ID */
            int mapped_id = (pid >= 0 && pid < graph->next_node_id) ? id_map[pid] : pid;
            if (mapped_id < 0) {
                /* 参与者无映射（理论上不会发生，因为涉及内部节点的约束
                 * 其所有内部参与者都应被映射） */
                LOG_WARN("beta_reduce", "约束 %d 参与者 %d 未映射，跳过", con->id, pid);
                skip_constraint = true;
                break;
            }
            resolved_participants[p] = mapped_id;
        }

        if (skip_constraint) continue;

        /* 添加重建后的约束 */
        AddConstraintResult ar = ADD_CONSTRAINT_OK;
        switch (con->type) {
        case INCIDENCE:
            if (con->participant_count == 2)
                ar = graph_add_incidence(graph, resolved_participants[0], resolved_participants[1]);
            break;
        case BETWEENNESS:
            if (con->participant_count == 3)
                ar = graph_add_betweenness(graph, resolved_participants[0],
                                            resolved_participants[1], resolved_participants[2]);
            break;
        case INTERSECTION:
            if (con->participant_count == 3)
                ar = graph_add_intersection(graph, resolved_participants[0],
                                             resolved_participants[1], resolved_participants[2]);
            break;
        case CONTAINMENT:
            if (con->participant_count == 2)
                ar = graph_add_containment(graph, resolved_participants[0], resolved_participants[1]);
            break;
        case CONNECTION:
            if (con->participant_count == 2)
                ar = graph_add_connection(graph, resolved_participants[0], resolved_participants[1]);
            break;
        default:
            LOG_WARN("beta_reduce", "未知约束类型 %d", (int)con->type);
            continue;
        }

        if (ar != ADD_CONSTRAINT_OK && ar != ADD_CONSTRAINT_DUPLICATE) {
            LOG_WARN("beta_reduce", "重建约束 %d 失败 (type=%d, result=%d)",
                     con->id, (int)con->type, (int)ar);
        }

        /* 记录受影响的约束 ID，以便后续移除旧约束 */
        int *tmp = lv_realloc(affected_target_ids,
                               (size_t)(affected_count + 1) * sizeof(int));
        if (tmp) {
            affected_target_ids = tmp;
            affected_target_ids[affected_count++] = ci;
        }
    }

    lv_free((void**)&needs_remap);

    /* 移除已重建的旧约束（倒序删除避免索引漂移） */
    for (int i = affected_count - 1; i >= 0; i--) {
        int idx = affected_target_ids[i];
        Constraint *old_con = graph->constraints[idx];
        if (old_con) {
            /* 使用 graph_deactivate_constraint 惰性删除 */
            graph_deactivate_constraint(graph, old_con->id);
        }
    }
    lv_free((void**)&affected_target_ids);

    return true;
}

/* ===========================================================================
 * beta_reduce_match：匹配 β-归约模式
 * =========================================================================== */

bool beta_reduce_match(ConstraintGraph *graph, int *out_func_block_id,
                        int *out_arg_node_id, int *out_output_port_id)
{
    if (!graph || !out_func_block_id || !out_arg_node_id || !out_output_port_id) {
        return false;
    }

    *out_func_block_id = -1;
    *out_arg_node_id = -1;
    *out_output_port_id = -1;

    /* 遍历所有节点，查找函数块 */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || !node->is_active) continue;
        if (node->type != GEOM_FUNCTION_BLOCK) continue;

        int fb_id = node->id;
        int input_port_count = node->data.func_block.input_count;
        int output_port_count = node->data.func_block.output_count;
        int *input_port_ids = node->data.func_block.input_port_ids;
        int *output_port_ids = node->data.func_block.output_port_ids;

        if (input_port_count <= 0 || output_port_count <= 0) continue;

        /* 查找输入端口到外部实参的连接 */
        int found_arg_node_id = -1;
        int found_input_port_id = -1;

        for (int ip = 0; ip < input_port_count && found_arg_node_id < 0; ip++) {
            int port_id = input_port_ids[ip];
            GeomNode *port_node = graph_get_node(graph, port_id);
            if (!port_node || port_node->type != GEOM_PORT) continue;
            if (!port_node->data.port) continue;
            if (port_node->data.port->type != PORT_INPUT) continue;

            /* 查找该端口的 CONNECTION 约束 */
            int con_indices[32];
            int con_count = graph_find_constraints_involving(graph, port_id,
                                                              con_indices, 32);

            for (int c = 0; c < con_count; c++) {
                Constraint *con = graph_get_constraint(graph, con_indices[c]);
                if (!con || !con->is_active) continue;
                if (con->type != CONNECTION) continue;
                if (con->participant_count != 2) continue;

                /* CONNECTION(src_output, dst_input)：实参输出 → 函数块输入 */
                int src_id = con->participants[0];
                int dst_id = con->participants[1];

                if (dst_id == port_id) {
                    /* src_id 是外部实参（不在 func_block 内部） */
                    GeomNode *src_node = graph_get_node(graph, src_id);
                    if (src_node && src_node->parent_block_id != fb_id) {
                        found_arg_node_id = src_id;
                        found_input_port_id = port_id;
                        break;
                    }
                }
            }
        }

        if (found_arg_node_id < 0) continue;

        /* 查找输出端口到外部消费端的连接 */
        int found_output_port_id = -1;
        for (int op = 0; op < output_port_count && found_output_port_id < 0; op++) {
            int port_id = output_port_ids[op];
            GeomNode *port_node = graph_get_node(graph, port_id);
            if (!port_node || port_node->type != GEOM_PORT) continue;
            if (!port_node->data.port) continue;
            if (port_node->data.port->type != PORT_OUTPUT) continue;

            /* 查找该端口的 CONNECTION 约束 */
            int con_indices[32];
            int con_count = graph_find_constraints_involving(graph, port_id,
                                                              con_indices, 32);

            for (int c = 0; c < con_count; c++) {
                Constraint *con = graph_get_constraint(graph, con_indices[c]);
                if (!con || !con->is_active) continue;
                if (con->type != CONNECTION) continue;
                if (con->participant_count != 2) continue;

                int src_id = con->participants[0];
                int dst_id = con->participants[1];

                /* 输出端口作为 src → 外部 dst */
                if (src_id == port_id) {
                    GeomNode *dst_node = graph_get_node(graph, dst_id);
                    if (dst_node && dst_node->parent_block_id != fb_id) {
                        found_output_port_id = port_id;
                        break;
                    }
                }

                /* 外部 src → 输出端口作为 dst */
                if (dst_id == port_id) {
                    GeomNode *src_node = graph_get_node(graph, src_id);
                    if (src_node && src_node->parent_block_id != fb_id) {
                        found_output_port_id = port_id;
                        break;
                    }
                }
            }
        }

        if (found_output_port_id < 0) continue;

        /* 匹配成功 */
        *out_func_block_id = fb_id;
        *out_arg_node_id = found_arg_node_id;
        *out_output_port_id = found_output_port_id;

        LOG_DEBUG("beta_reduce", "匹配成功: func_block=%d, arg=%d, out_port=%d",
                  fb_id, found_arg_node_id, found_output_port_id);
        return true;
    }

    return false;
}

/* ===========================================================================
 * beta_reduce_apply：执行 β-归约应用
 * =========================================================================== */

bool beta_reduce_apply(ConstraintGraph *graph, int func_block_id,
                        int arg_node_id, int output_port_id)
{
    if (!graph) return false;

    GeomNode *func_node = graph_get_node(graph, func_block_id);
    if (!func_node || func_node->type != GEOM_FUNCTION_BLOCK) {
        LOG_WARN("beta_reduce", "函数块 %d 不存在或类型不正确", func_block_id);
        return false;
    }

    /* 检查实参节点 */
    GeomNode *arg_node = graph_get_node(graph, arg_node_id);
    if (!arg_node) {
        LOG_WARN("beta_reduce", "实参节点 %d 不存在", arg_node_id);
        return false;
    }

    /* 获取内部节点信息 */
    int internal_count = func_node->data.func_block.internal_node_count;
    GeomNode **internal_nodes = func_node->data.func_block.internal_nodes;

    if (internal_count <= 0 || !internal_nodes) {
        LOG_WARN("beta_reduce", "函数块 %d 无内部节点", func_block_id);
        return false;
    }

    LOG_DEBUG("beta_reduce", "β-归约应用: func_block=%d, arg=%d, internal_count=%d",
              func_block_id, arg_node_id, internal_count);

    /* Step 1: 构建内部节点 ID 数组 */
    int *internal_ids = lv_calloc((size_t)internal_count, sizeof(int));
    if (!internal_ids) return false;
    for (int i = 0; i < internal_count; i++) {
        internal_ids[i] = internal_nodes[i] ? internal_nodes[i]->id : -1;
    }

    /* Step 2: 分配 ID 映射表。
     * 使用 next_node_id + internal_count 作为安全大小。 */
    int map_size = graph->next_node_id + internal_count + 64;
    int *id_map = lv_calloc((size_t)map_size, sizeof(int));
    if (!id_map) {
        lv_free((void**)&internal_ids);
        return false;
    }
    memset(id_map, -1, (size_t)map_size * sizeof(int));

    /* Step 3: 构建 ID 映射（创建内部局部节点的副本） */
    int copy_count = build_id_mapping(graph, func_block_id,
                                       internal_ids, internal_count,
                                       arg_node_id, id_map);
    if (copy_count < 0) {
        lv_free((void**)&internal_ids);
        lv_free((void**)&id_map);
        return false;
    }

    /* Step 4: 重建涉及内部节点的约束（使用 ID 映射） */
    if (!remap_internal_constraints(graph, internal_ids, internal_count, id_map)) {
        LOG_WARN("beta_reduce", "重建内部约束失败");
        lv_free((void**)&internal_ids);
        lv_free((void**)&id_map);
        return false;
    }

    /* Step 5: 将输出端口的外部消费者重连到复制的内部输出端口副本。
     * 查找原输出端口 id_map 中的映射来找到对应的新 PORT */
    int new_output_port_id = (output_port_id >= 0 && output_port_id < map_size)
                              ? id_map[output_port_id] : -1;

    if (new_output_port_id >= 0) {
        /* 找到原输出端口的外部 CONNECTION 约束 */
        int con_indices[32];
        int con_count = graph_find_constraints_involving(graph, output_port_id,
                                                          con_indices, 32);

        for (int c = 0; c < con_count; c++) {
            Constraint *con = graph_get_constraint(graph, con_indices[c]);
            if (!con || !con->is_active) continue;
            if (con->type != CONNECTION) continue;
            if (con->participant_count != 2) continue;

            int src_id = con->participants[0];
            int dst_id = con->participants[1];

            /* 函数块输出端口 → 外部消费者 */
            if (src_id == output_port_id) {
                /* 建立新连接：新输出端口 → 外部消费者 */
                if (dst_id >= 0) {
                    graph_add_connection(graph, new_output_port_id, dst_id);
                    LOG_DEBUG("beta_reduce", "重连输出: 新端口 %d → 消费者 %d",
                              new_output_port_id, dst_id);
                }
            }
            /* 外部 → 函数块输出端口（不太常见，但处理一下） */
            if (dst_id == output_port_id) {
                if (src_id >= 0) {
                    graph_add_connection(graph, src_id, new_output_port_id);
                    LOG_DEBUG("beta_reduce", "重连输出: 来源 %d → 新端口 %d",
                              src_id, new_output_port_id);
                }
            }
        }
    }

    /* Step 6: 移除函数块节点（这会自动移除相关的 CONNECTION 约束和边界端口） */
    RemoveNodeResult rr = graph_remove_node(graph, func_block_id);
    if (rr != REMOVE_NODE_OK) {
        LOG_WARN("beta_reduce", "移除函数块节点 %d 失败", func_block_id);
    }

    /* Step 7: 标记图为脏状态 */
    graph_mark_dirty(graph);

    LOG_DEBUG("beta_reduce", "β-归约成功: func_block=%d 已内联, %d 个内部局部节点已复制",
              func_block_id, copy_count);

    lv_free((void**)&internal_ids);
    lv_free((void**)&id_map);

    return true;
}

/* ===========================================================================
 * beta_reduce：顶层 β-归约函数
 * =========================================================================== */

bool beta_reduce(ConstraintGraph *graph) {
    if (!graph) return false;

    int func_block_id = -1, arg_node_id = -1, output_port_id = -1;

    if (!beta_reduce_match(graph, &func_block_id, &arg_node_id, &output_port_id)) {
        LOG_DEBUG("beta_reduce", "无 β-归约匹配模式");
        return false;
    }

    return beta_reduce_apply(graph, func_block_id, arg_node_id, output_port_id);
}

/**
 * @file func_block_instantiate.c
 * @brief 函数块例化与捕获避免模块
 * @details 实现函数块的例化（beta-归约）、部分应用（柯里化），
 *          以及捕获避免替换（Capture-Avoiding Substitution）和 Alpha-重命名。
 *
 * @author Lv-00 Project
 * @version 3.2.0
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "func_block.h"
#include "func_block_internal.h"
#include "lv00_internal.h"
#include "lv00_utils.h"
#include "stream.h"
#include "stream_context_util.h"

/* ============== 例化操作 ============== */

/**
 * @brief 例化时的 beta-归约辅助
 *
 * 对内部节点进行分类（设计文档 3.3）：
 *   - 形式参数（is_formal_param=true, parent_block_id==fb->id）-> 替换为实参
 *   - 自由变量（parent_block_id != fb->id）-> 保持原引用
 *   - 内部局部变量（parent_block_id==fb->id, 非形式参数）-> 创建深拷贝
 *
 * @param fb               函数块
 * @param graph            约束图
 * @param arg_mappings     实参映射数组
 * @param arg_count       实参数量
 * @param id_map          ID 映射表
 * @param max_id          最大 ID
 * @param out_new_node_ids 输出新节点 ID 数组
 * @param out_new_node_count 输出新节点数量
 * @return 例化结果
 */
static InstantiateResult instantiate_copy_internal_nodes(
    FuncBlock *fb,
    ConstraintGraph *graph,
    int *arg_mappings,
    int arg_count,
    int *id_map,
    int max_id,
    int **out_new_node_ids,
    int *out_new_node_count)
{
    /* 使用安全加法宏防止 capacity 计算整数溢出 */
    int capacity = LV00_SAFE_ADD(fb->internal_node_count, fb->output_count, INT_MAX);
    if (capacity == INT_MAX) {
        return INSTANTIATE_OUT_OF_MEMORY;
    }
    int *new_node_ids = lv00_malloc((size_t)capacity * sizeof(int));
    if (!new_node_ids) return INSTANTIATE_OUT_OF_MEMORY;
    int new_count = 0;

    /* Beta-归约第一步：输入端口（形式参数）映射到实参节点 */
    for (int i = 0; i < fb->input_count && i < arg_count; i++) {
        int port_id = fb->input_port_ids[i];
        int arg_id = arg_mappings[i];
        GeomNode *arg_node = graph_get_node(graph, arg_id);
        if (!arg_node) {
            lv00_free((void **)&new_node_ids);
            return INSTANTIATE_NO_SOLUTION;
        }
        /* O(1) 映射：形式参数 -> 实参 */
        if (port_id >= 0 && port_id <= max_id) {
            id_map[port_id] = arg_id;
        }
    }

    /* Beta-归约第二步：遍历内部节点，按三种情况处理 */
    for (int i = 0; i < fb->internal_node_count; i++) {
        int old_id = fb->internal_node_ids[i];

        /* 跳过输入端口（已在上面替换） */
        if (is_id_in_array(old_id, fb->input_port_ids, fb->input_count)) {
            continue;
        }

        GeomNode *orig = graph_get_node(graph, old_id);
        if (!orig) continue;

        /* O(1) parent_block_id 检查：三种情况 */
        /* 情况1: 形式参数 - 已通过 arg_mappings 替换 */
        if (orig->type == GEOM_PORT && orig->data.port &&
            orig->data.port->is_formal_param &&
            orig->parent_block_id == fb->id) {
            continue;
        }

        /* 情况2: 自由变量 - 保持原引用，不创建副本 */
        /* parent_block_id != fb->id 的都应保持原引用，包括 -1 的全局节点 */
        if (orig->parent_block_id != fb->id) {
            if (old_id >= 0 && old_id <= max_id) {
                id_map[old_id] = old_id;
            }
            continue;
        }

        /* 情况3: 内部局部变量 - 创建深拷贝 */
        GeomNode *copy = lv00_malloc(sizeof(GeomNode));
        if (!copy) {
            lv00_free((void **)&new_node_ids);
            return INSTANTIATE_OUT_OF_MEMORY;
        }
        memcpy(copy, orig, sizeof(GeomNode));
        memset(&copy->data, 0, sizeof(copy->data));

        /* 分配新ID */
        copy->id = graph->next_node_id++;
        /* namespace_depth 恢复到外层 */
        copy->namespace_depth = orig->namespace_depth > 0 ? orig->namespace_depth - 1 : 0;
        copy->parent_block_id = -1;

        /*
         * 深拷贝 symbolic_coords
         *
         * 注意：memcpy 浅拷贝后，copy->symbolic_coords 指向 orig 的数组，
         * 如果后续深拷贝分配失败，必须将 copy->symbolic_coords 置为 NULL，
         * 否则后续释放 copy 时会 double-free orig 的坐标数组
         */
        if (orig->symbolic_coords && orig->coord_count > 0) {
            copy->symbolic_coords = lv00_malloc((size_t)orig->coord_count * sizeof(SymbolicCoord *));
            if (!copy->symbolic_coords) {
                /* 深拷贝分配失败：将 symbolic_coords 置为 NULL，避免 double-free */
                copy->symbolic_coords = NULL;
                copy->coord_count = 0;
                lv00_free((void **)&copy);
                lv00_free((void **)&new_node_ids);
                return INSTANTIATE_OUT_OF_MEMORY;
            }
            for (int j = 0; j < orig->coord_count; j++) {
                copy->symbolic_coords[j] = orig->symbolic_coords[j]
                    ? symbolic_coord_copy(orig->symbolic_coords[j]) : NULL;
                /* 如果单个坐标拷贝失败，释放已拷贝的部分并回退 */
                if (orig->symbolic_coords[j] && !copy->symbolic_coords[j]) {
                    for (int k = 0; k < j; k++) {
                        if (copy->symbolic_coords[k])
                            symbolic_coord_destroy(copy->symbolic_coords[k]);
                    }
                    lv00_free((void **)&copy->symbolic_coords);
                    copy->symbolic_coords = NULL;
                    copy->coord_count = 0;
                    lv00_free((void **)&copy);
                    lv00_free((void **)&new_node_ids);
                    return INSTANTIATE_OUT_OF_MEMORY;
                }
            }
        }

        /* 深拷贝 numeric_assumption_declaration */
        if (orig->numeric_assumption_declaration) {
            copy->numeric_assumption_declaration = lv00_strdup(orig->numeric_assumption_declaration);
        } else {
            copy->numeric_assumption_declaration = NULL;
        }

        /* 处理类型特定数据 */
        switch (orig->type) {
            case GEOM_PORT: {
                /* 防止空指针解引用：确保 orig->data.port 不为 NULL */
                if (!orig->data.port) {
                    lv00_free((void **)&copy->symbolic_coords);
                    lv00_free((void **)&copy->numeric_assumption_declaration);
                    lv00_free((void **)&copy);
                    lv00_free((void **)&new_node_ids);
                    return INSTANTIATE_NO_SOLUTION;
                }
                Port *port_copy = lv00_malloc(sizeof(Port));
                if (!port_copy) {
                    lv00_free((void **)&copy->symbolic_coords);
                    lv00_free((void **)&copy->numeric_assumption_declaration);
                    lv00_free((void **)&copy);
                    lv00_free((void **)&new_node_ids);
                    return INSTANTIATE_OUT_OF_MEMORY;
                }
                memcpy(port_copy, orig->data.port, sizeof(Port));
                port_copy->id = copy->id;
                port_copy->connected_to = NULL;
                port_copy->namespace_depth = copy->namespace_depth;
                port_copy->parent_block_id = -1;
                copy->data.port = port_copy;
                break;
            }
            case GEOM_REGION: {
                if (orig->data.region.boundary_segments && orig->data.region.segment_count > 0) {
                    copy->data.region.boundary_segments = lv00_malloc(
                        (size_t)orig->data.region.segment_count * sizeof(GeomNode *));
                    if (!copy->data.region.boundary_segments) {
                        lv00_free((void **)&copy->symbolic_coords);
                        lv00_free((void **)&copy->numeric_assumption_declaration);
                        lv00_free((void **)&copy);
                        lv00_free((void **)&new_node_ids);
                        return INSTANTIATE_OUT_OF_MEMORY;
                    }
                    memcpy(copy->data.region.boundary_segments,
                           orig->data.region.boundary_segments,
                           (size_t)orig->data.region.segment_count * sizeof(GeomNode *));
                }
                break;
            }
            case GEOM_FUNCTION_BLOCK: {
                if (orig->data.func_block.internal_nodes && orig->data.func_block.internal_node_count > 0) {
                    copy->data.func_block.internal_nodes = lv00_malloc(
                        (size_t)orig->data.func_block.internal_node_count * sizeof(GeomNode *));
                    if (!copy->data.func_block.internal_nodes) {
                        lv00_free((void **)&copy->symbolic_coords);
                        lv00_free((void **)&copy->numeric_assumption_declaration);
                        lv00_free((void **)&copy);
                        lv00_free((void **)&new_node_ids);
                        return INSTANTIATE_OUT_OF_MEMORY;
                    }
                    memcpy(copy->data.func_block.internal_nodes,
                           orig->data.func_block.internal_nodes,
                           (size_t)orig->data.func_block.internal_node_count * sizeof(GeomNode *));
                }
                if (orig->data.func_block.input_port_ids && orig->data.func_block.input_count > 0) {
                    copy->data.func_block.input_port_ids = lv00_malloc(
                        (size_t)orig->data.func_block.input_count * sizeof(int));
                    if (!copy->data.func_block.input_port_ids) {
                        lv00_free((void **)&copy->data.func_block.internal_nodes);
                        lv00_free((void **)&copy->symbolic_coords);
                        lv00_free((void **)&copy->numeric_assumption_declaration);
                        lv00_free((void **)&copy);
                        lv00_free((void **)&new_node_ids);
                        return INSTANTIATE_OUT_OF_MEMORY;
                    }
                    memcpy(copy->data.func_block.input_port_ids,
                           orig->data.func_block.input_port_ids,
                           (size_t)orig->data.func_block.input_count * sizeof(int));
                }
                if (orig->data.func_block.output_port_ids && orig->data.func_block.output_count > 0) {
                    copy->data.func_block.output_port_ids = lv00_malloc(
                        (size_t)orig->data.func_block.output_count * sizeof(int));
                    if (!copy->data.func_block.output_port_ids) {
                        lv00_free((void **)&copy->data.func_block.input_port_ids);
                        lv00_free((void **)&copy->data.func_block.internal_nodes);
                        lv00_free((void **)&copy->symbolic_coords);
                        lv00_free((void **)&copy->numeric_assumption_declaration);
                        lv00_free((void **)&copy);
                        lv00_free((void **)&new_node_ids);
                        return INSTANTIATE_OUT_OF_MEMORY;
                    }
                    memcpy(copy->data.func_block.output_port_ids,
                           orig->data.func_block.output_port_ids,
                           (size_t)orig->data.func_block.output_count * sizeof(int));
                }
                break;
            }
            default:
                /* GEOM_POINT, GEOM_LINE_SEGMENT 无额外数据 */
                break;
        }

        /* 注册 ID 映射 */
        if (old_id >= 0 && old_id <= max_id) {
            id_map[old_id] = copy->id;
        }

        /* 使用 lv00_realloc 统一内存管理，确保内存追踪系统可以追踪此分配 */
        GeomNode **new_nodes = lv00_realloc(graph->nodes,
            (size_t)(graph->node_count + 1) * sizeof(GeomNode *));
        if (!new_nodes) {
            if (copy->type == GEOM_PORT) lv00_free((void **)&copy->data.port);
            if (copy->type == GEOM_REGION) lv00_free((void **)&copy->data.region.boundary_segments);
            if (copy->type == GEOM_FUNCTION_BLOCK) {
                lv00_free((void **)&copy->data.func_block.internal_nodes);
                lv00_free((void **)&copy->data.func_block.input_port_ids);
                lv00_free((void **)&copy->data.func_block.output_port_ids);
            }
            lv00_free((void **)&copy->symbolic_coords);
            lv00_free((void **)&copy->numeric_assumption_declaration);
            lv00_free((void **)&copy);
            lv00_free((void **)&new_node_ids);
            return INSTANTIATE_OUT_OF_MEMORY;
        }
        graph->nodes = new_nodes;
        graph->nodes[graph->node_count++] = copy;
        /* 将新节点注册到哈希索引，确保 graph_get_node 能通过 ID 查找到该节点 */
        graph_node_index_insert(graph, copy);

        if (new_count >= capacity) {
            /* 修复：防止 capacity *= 2 导致有符号整数溢出。
             * 当 capacity > INT_MAX / 2 时翻倍会溢出，此时应终止操作。 */
            if (capacity > INT_MAX / 2) {
                lv00_free((void **)&new_node_ids);
                return INSTANTIATE_OUT_OF_MEMORY;
            }
            capacity *= 2;
            int *tmp = lv00_realloc(new_node_ids, (size_t)capacity * sizeof(int));
            if (!tmp) {
                lv00_free((void **)&new_node_ids);
                return INSTANTIATE_OUT_OF_MEMORY;
            }
            new_node_ids = tmp;
        }
        new_node_ids[new_count++] = copy->id;
    }

    /* 流式事件：函数块例化完成 */
    if (func_block_stream_ctx) {
        stream_emit_simple(func_block_stream_ctx, STREAM_EVENT_FUNC_BLOCK_INSTANTIATE_DONE,
            "函数块例化完成", 0);
    }
    *out_new_node_ids = new_node_ids;
    *out_new_node_count = new_count;
    return INSTANTIATE_OK;
}

/**
 * @brief 例化后更新复制节点中的引用
 *
 * 使用 ID 映射表更新复制节点中的引用，包括：
 * - GEOM_REGION：更新边界线段引用
 * - GEOM_FUNCTION_BLOCK：更新内部节点、输入/输出端口 ID
 * - GEOM_PORT：更新连接目标引用
 *
 * @param graph       约束图
 * @param new_node_ids 新节点 ID 数组
 * @param new_count   新节点数量
 * @param id_map     ID 映射表
 * @param max_id     最大 ID
 */
static void instantiate_update_references(
    ConstraintGraph *graph,
    int *new_node_ids,
    int new_count,
    int *id_map,
    int max_id)
{
    for (int i = 0; i < new_count; i++) {
        GeomNode *copy = graph_get_node(graph, new_node_ids[i]);
        if (!copy) continue;

        switch (copy->type) {
            case GEOM_REGION:
                for (int j = 0; j < copy->data.region.segment_count; j++) {
                    if (copy->data.region.boundary_segments[j]) {
                        int old_seg_id = copy->data.region.boundary_segments[j]->id;
                        if (old_seg_id >= 0 && old_seg_id <= max_id && id_map[old_seg_id] >= 0) {
                            copy->data.region.boundary_segments[j] =
                                graph_get_node(graph, id_map[old_seg_id]);
                        }
                    }
                }
                break;

            case GEOM_FUNCTION_BLOCK:
                for (int j = 0; j < copy->data.func_block.internal_node_count; j++) {
                    if (copy->data.func_block.internal_nodes[j]) {
                        int old_nid = copy->data.func_block.internal_nodes[j]->id;
                        if (old_nid >= 0 && old_nid <= max_id && id_map[old_nid] >= 0) {
                            copy->data.func_block.internal_nodes[j] =
                                graph_get_node(graph, id_map[old_nid]);
                        }
                    }
                }
                for (int j = 0; j < copy->data.func_block.input_count; j++) {
                    int old_pid = copy->data.func_block.input_port_ids[j];
                    if (old_pid >= 0 && old_pid <= max_id && id_map[old_pid] >= 0) {
                        copy->data.func_block.input_port_ids[j] = id_map[old_pid];
                    }
                }
                for (int j = 0; j < copy->data.func_block.output_count; j++) {
                    int old_pid = copy->data.func_block.output_port_ids[j];
                    if (old_pid >= 0 && old_pid <= max_id && id_map[old_pid] >= 0) {
                        copy->data.func_block.output_port_ids[j] = id_map[old_pid];
                    }
                }
                break;

            case GEOM_PORT:
                if (copy->data.port && copy->data.port->connected_to) {
                    int old_cid = copy->data.port->connected_to->id;
                    if (old_cid >= 0 && old_cid <= max_id && id_map[old_cid] >= 0) {
                        copy->data.port->connected_to =
                            graph_get_node(graph, id_map[old_cid]);
                    }
                }
                break;

            default:
                break;
        }
    }
}

/**
 * @brief 例化时复制涉及内部节点的约束
 *
 * 非 CONNECTION 约束：直接复制并替换参与者ID。
 * 仅复制完全涉及内部节点且至少有一个参与者被映射的约束。
 *
 * @param fb      函数块
 * @param graph   约束图
 * @param id_map ID 映射表
 * @param max_id 最大 ID
 */
static void instantiate_copy_constraints(
    FuncBlock *fb,
    ConstraintGraph *graph,
    int *id_map,
    int max_id)
{
    /* 收集所有内部相关ID（使用共享辅助函数） */
    int *all_ids = NULL;
    int all_count = 0;
    if (!collect_all_block_ids(fb, &all_ids, &all_count)) return;

    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        if (c->type == CONNECTION) continue;

        /* 检查约束是否完全涉及内部节点 */
        bool all_internal = true;
        bool any_mapped = false;
        for (int j = 0; j < c->participant_count; j++) {
            int pid = c->participants[j];
            if (is_id_in_array(pid, all_ids, all_count)) {
                /* 是内部节点 */
                if (pid >= 0 && pid <= max_id && id_map[pid] >= 0 && id_map[pid] != pid) {
                    any_mapped = true;
                }
            } else {
                all_internal = false;
            }
        }

        if (!all_internal || !any_mapped) continue;

        /* 创建新约束，替换参与者ID */
        Constraint *new_c = lv00_malloc(sizeof(Constraint));
        if (!new_c) continue;
        memset(new_c, 0, sizeof(Constraint));
        new_c->id = graph->next_constraint_id++;
        new_c->type = c->type;
        new_c->template_id = c->template_id;
        new_c->participant_count = c->participant_count;
        new_c->participants = lv00_malloc((size_t)c->participant_count * sizeof(int));
        if (!new_c->participants) {
            lv00_free((void **)&new_c);
            continue;
        }
        for (int j = 0; j < c->participant_count; j++) {
            int pid = c->participants[j];
            if (pid >= 0 && pid <= max_id && id_map[pid] >= 0) {
                new_c->participants[j] = id_map[pid];
            } else {
                new_c->participants[j] = pid;
            }
        }

        /* 添加到图 */
        /* 使用 lv00_realloc 统一内存管理 */
        Constraint **new_constraints = lv00_realloc(graph->constraints,
            (size_t)(graph->constraint_count + 1) * sizeof(Constraint *));
        if (!new_constraints) {
            lv00_free((void **)&new_c->participants);
            lv00_free((void **)&new_c);
            continue;
        }
        graph->constraints = new_constraints;
        graph->constraints[graph->constraint_count++] = new_c;
        /* 将新约束注册到哈希索引，确保 graph_get_constraint 能通过 ID 查找到该约束 */
        graph_constraint_index_insert(graph, new_c);
    }

    lv00_free((void **)&all_ids);
}

/**
 * @brief 例化时复制 CONNECTION 约束
 *
 * 应用设计文档 3.3 节的三种情况 beta-归约：
 *
 * CONNECTION 约束连接两个端口（src_port -> dst_port）。
 * 对于每个参与者端口 p，执行 O(1) 判定：
 *   情况 A（形式参数引用）：
 *     p.parent_block_id == fb->id AND p.is_formal_param == true
 *     -> 重定向到对应实参端口（通过 id_map 查找）
 *
 *   情况 B（自由变量引用）：
 *     p.parent_block_id != fb->id
 *     -> 保持原连接目标不变（引用外部变量，不被 beta-归约改变）
 *
 *   情况 C（内部局部引用）：
 *     p.parent_block_id == fb->id AND p.is_formal_param == false
 *     -> 重映射到复制件中对应的新内部节点（通过 id_map 查找）
 *
 * @param fb      函数块
 * @param graph   约束图
 * @param id_map ID 映射表（旧 ID -> 新 ID）
 * @param max_id 最大 ID（用于边界检查）
 */
static void instantiate_copy_connection_constraints(
    FuncBlock *fb,
    ConstraintGraph *graph,
    int *id_map,
    int max_id)
{
    /* 收集所有内部相关ID（使用共享辅助函数） */
    int *all_ids = NULL;
    int all_count = 0;
    if (!collect_all_block_ids(fb, &all_ids, &all_count)) return;

    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        if (c->type != CONNECTION) continue;

        /* CONNECTION 约束有两个参与者：src_port 和 dst_port */
        if (c->participant_count != 2) continue;

        int src_id = c->participants[0];
        int dst_id = c->participants[1];

        /* 检查约束是否涉及内部节点 */
        bool src_internal = is_id_in_array(src_id, all_ids, all_count);
        bool dst_internal = is_id_in_array(dst_id, all_ids, all_count);

        /* 两端都是外部节点：不涉及此函数块，跳过 */
        if (!src_internal && !dst_internal) continue;

        /* 对每个参与者端口应用三种情况判定 */
        int new_src_id = src_id;
        int new_dst_id = dst_id;
        bool any_changed = false;

        /* 处理源端口 */
        if (src_internal) {
            GeomNode *src_node = graph_get_node(graph, src_id);
            if (src_node && src_node->type == GEOM_PORT && src_node->data.port) {
                Port *p = src_node->data.port;
                if (p->parent_block_id == fb->id && p->is_formal_param) {
                    /* 情况 A：形式参数引用 -> 重定向到实参 */
                    if (src_id >= 0 && src_id <= max_id && id_map[src_id] >= 0) {
                        new_src_id = id_map[src_id];
                        any_changed = true;
                    }
                } else if (p->parent_block_id != fb->id) {
                    /* 情况 B：自由变量引用 -> 保持原目标不变 */
                    new_src_id = src_id;
                } else {
                    /* 情况 C：内部局部引用 -> 重映射到复制件 */
                    if (src_id >= 0 && src_id <= max_id && id_map[src_id] >= 0) {
                        new_src_id = id_map[src_id];
                        any_changed = true;
                    }
                }
            } else if (src_id >= 0 && src_id <= max_id && id_map[src_id] >= 0) {
                /* 非 PORT 类型但属于内部节点：直接重映射 */
                new_src_id = id_map[src_id];
                any_changed = true;
            }
        }

        /* 处理目标端口 */
        if (dst_internal) {
            GeomNode *dst_node = graph_get_node(graph, dst_id);
            if (dst_node && dst_node->type == GEOM_PORT && dst_node->data.port) {
                Port *p = dst_node->data.port;
                if (p->parent_block_id == fb->id && p->is_formal_param) {
                    /* 情况 A：形式参数引用 -> 重定向到实参 */
                    if (dst_id >= 0 && dst_id <= max_id && id_map[dst_id] >= 0) {
                        new_dst_id = id_map[dst_id];
                        any_changed = true;
                    }
                } else if (p->parent_block_id != fb->id) {
                    /* 情况 B：自由变量引用 -> 保持原目标不变 */
                    new_dst_id = dst_id;
                } else {
                    /* 情况 C：内部局部引用 -> 重映射到复制件 */
                    if (dst_id >= 0 && dst_id <= max_id && id_map[dst_id] >= 0) {
                        new_dst_id = id_map[dst_id];
                        any_changed = true;
                    }
                }
            } else if (dst_id >= 0 && dst_id <= max_id && id_map[dst_id] >= 0) {
                /* 非 PORT 类型但属于内部节点：直接重映射 */
                new_dst_id = id_map[dst_id];
                any_changed = true;
            }
        }

        /* 如果没有任何变化，不需要创建新约束 */
        if (!any_changed) continue;

        /* 创建新的 CONNECTION 约束 */
        Constraint *new_c = lv00_malloc(sizeof(Constraint));
        if (!new_c) continue;
        memset(new_c, 0, sizeof(Constraint));
        new_c->id = graph->next_constraint_id++;
        new_c->type = CONNECTION;
        new_c->template_id = c->template_id;
        new_c->participant_count = 2;
        new_c->participants = lv00_malloc(2 * sizeof(int));
        if (!new_c->participants) {
            lv00_free((void **)&new_c);
            continue;
        }
        new_c->participants[0] = new_src_id;
        new_c->participants[1] = new_dst_id;

        /* 添加到图 */
        /* 使用 lv00_realloc 统一内存管理 */
        Constraint **new_constraints = lv00_realloc(graph->constraints,
            (size_t)(graph->constraint_count + 1) * sizeof(Constraint *));
        if (!new_constraints) {
            lv00_free((void **)&new_c->participants);
            lv00_free((void **)&new_c);
            continue;
        }
        graph->constraints = new_constraints;
        graph->constraints[graph->constraint_count++] = new_c;
        /* 将新 CONNECTION 约束注册到哈希索引 */
        graph_constraint_index_insert(graph, new_c);
    }

    lv00_free((void **)&all_ids);
}

/**
 * @brief 例化函数块
 *
 * @param fb               函数块
 * @param graph           约束图
 * @param arg_mappings    实参映射数组
 * @param arg_count      实参数量
 * @param out_new_node_ids 输出新节点 ID 数组
 * @param out_new_node_count 输出新节点数量
 * @return 例化结果
 */
InstantiateResult func_block_instantiate(
    FuncBlock *fb,
    ConstraintGraph *graph,
    int *arg_mappings,
    int arg_count,
    int **out_new_node_ids,
    int *out_new_node_count)
{
    if (!fb || !graph || !out_new_node_ids || !out_new_node_count) {
        return INSTANTIATE_NO_SOLUTION;
    }

    *out_new_node_ids = NULL;
    *out_new_node_count = 0;

    /* 流式事件：函数块例化开始 */
    if (func_block_stream_ctx) {
        stream_emit_simple(func_block_stream_ctx, STREAM_EVENT_FUNC_BLOCK_INSTANTIATE_START,
            "函数块例化开始", 0);
    }

    /* 检查前置条件 */
    for (int i = 0; i < fb->precondition_count; i++) {
        GeomNode *region = graph_get_node(graph, fb->precondition_region_ids[i]);
        if (!region || region->type != GEOM_REGION) {
            /* 流式事件：函数块例化失败 */
            if (func_block_stream_ctx) {
                stream_emit_simple(func_block_stream_ctx, STREAM_EVENT_FUNC_BLOCK_INSTANTIATE_DONE,
                    "函数块例化失败", 1);
            }
            return INSTANTIATE_PRECONDITION_FAILED;
        }
        if (!graph_validate_region_closure(graph, region->id)) {
            /* 流式事件：函数块例化失败 */
            if (func_block_stream_ctx) {
                stream_emit_simple(func_block_stream_ctx, STREAM_EVENT_FUNC_BLOCK_INSTANTIATE_DONE,
                    "函数块例化失败", 1);
            }
            return INSTANTIATE_PRECONDITION_FAILED;
        }
    }

    /* 检查实参数量 */
    if (arg_count < fb->input_count) {
        /* 流式事件：函数块例化失败 */
        if (func_block_stream_ctx) {
            stream_emit_simple(func_block_stream_ctx, STREAM_EVENT_FUNC_BLOCK_INSTANTIATE_DONE,
                "函数块例化失败", 1);
        }
        return INSTANTIATE_NO_SOLUTION;
    }

    /* 确定 ID 映射表大小 */
    int max_id = 0;
    for (int i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i]->id > max_id) max_id = graph->nodes[i]->id;
    }
    for (int i = 0; i < fb->internal_node_count; i++) {
        if (fb->internal_node_ids[i] > max_id) max_id = fb->internal_node_ids[i];
    }
    for (int i = 0; i < fb->input_count; i++) {
        if (fb->input_port_ids[i] > max_id) max_id = fb->input_port_ids[i];
    }
    for (int i = 0; i < fb->output_count; i++) {
        if (fb->output_port_ids[i] > max_id) max_id = fb->output_port_ids[i];
    }

    /* 整数溢出检查：确保 max_id + 2 不超过 INT_MAX */
    if (max_id > INT_MAX - 2) {
        LOG_ERROR("func_block", "实例化失败：节点ID过大，无法分配ID映射表");
        return INSTANTIATE_OUT_OF_MEMORY;
    }

    int *id_map = lv00_malloc((size_t)(max_id + 2) * sizeof(int));
    if (!id_map) {
        /* 流式事件：函数块例化失败 */
        if (func_block_stream_ctx) {
            stream_emit_simple(func_block_stream_ctx, STREAM_EVENT_FUNC_BLOCK_INSTANTIATE_DONE,
                "函数块例化失败", 1);
        }
        return INSTANTIATE_OUT_OF_MEMORY;
    }
    for (int i = 0; i <= max_id + 1; i++) {
        id_map[i] = -1;
    }

    /* Beta-归约 + 复制内部节点 */
    int *new_node_ids = NULL;
    int new_count = 0;
    InstantiateResult result = instantiate_copy_internal_nodes(
        fb, graph, arg_mappings, arg_count, id_map, max_id,
        &new_node_ids, &new_count);

    if (result != INSTANTIATE_OK) {
        lv00_free((void **)&id_map);
        return result;
    }

    /* 第二遍：更新复制节点中的引用 */
    instantiate_update_references(graph, new_node_ids, new_count, id_map, max_id);

    /* 复制约束（非 CONNECTION） */
    instantiate_copy_constraints(fb, graph, id_map, max_id);

    /* 复制 CONNECTION 约束（应用设计文档 3.3 节三情况 beta-归约） */
    instantiate_copy_connection_constraints(fb, graph, id_map, max_id);

    /* 流式事件：函数块例化完成 */
    if (func_block_stream_ctx) {
        stream_emit_simple(func_block_stream_ctx, STREAM_EVENT_FUNC_BLOCK_INSTANTIATE_DONE,
            result == INSTANTIATE_OK ? "函数块例化完成" : "函数块例化失败", 1);
    }

    lv00_free((void **)&id_map);
    *out_new_node_ids = new_node_ids;
    *out_new_node_count = new_count;
    return INSTANTIATE_OK;
}

/* ============== 部分应用（柯里化） ============== */

/**
 * @brief 部分应用函数块（柯里化）
 *
 * @param fb                函数块
 * @param graph            约束图
 * @param fixed_arg_mappings 固定的实参映射数组
 * @param fixed_count     固定实参数量
 * @param out_new_fb      输出新函数块
 * @return true  成功，false 失败
 */
bool func_block_partial_apply(
    FuncBlock *fb,
    ConstraintGraph *graph,
    int *fixed_arg_mappings,
    int fixed_count,
    FuncBlock **out_new_fb)
{
    if (!fb || !graph || !out_new_fb) return false;
    if (fixed_count < 0 || fixed_count > fb->input_count) return false;

    /* 流式事件：部分应用入口 */
    if (func_block_stream_ctx) {
        stream_emit_simple(func_block_stream_ctx,
                           STREAM_EVENT_FUNC_BLOCK_PARTIAL_APPLY,
                           "函数块部分应用",
                           -1);
    }

    /* 创建新函数块 */
    FuncBlock *new_fb = func_block_create(fb->id + LV00_FUNC_BLOCK_ID_OFFSET);
    if (!new_fb) return false;

    /* 复制内部节点 */
    if (!func_block_set_internal_nodes(new_fb, fb->internal_node_ids, fb->internal_node_count)) {
        func_block_destroy(new_fb);
        return false;
    }

    /* 剩余的输入端口（未被固定的部分） */
    int remaining_count = fb->input_count - fixed_count;
    if (remaining_count > 0) {
        int *remaining_ports = lv00_malloc((size_t)remaining_count * sizeof(int));
        if (!remaining_ports) {
            func_block_destroy(new_fb);
            return false;
        }
        for (int i = fixed_count, j = 0; i < fb->input_count; i++, j++) {
            remaining_ports[j] = fb->input_port_ids[i];
        }
        bool ok = func_block_set_input_ports(new_fb, remaining_ports, remaining_count);
        lv00_free((void **)&remaining_ports);
        if (!ok) {
            func_block_destroy(new_fb);
            return false;
        }
    }

    /* 复制输出端口 */
    if (!func_block_set_output_ports(new_fb, fb->output_port_ids, fb->output_count)) {
        func_block_destroy(new_fb);
        return false;
    }

    /* 复制选择器 */
    if (fb->selector) {
        SolutionSelector *new_sel = selector_create(fb->selector->type);
        if (!new_sel) {
            func_block_destroy(new_fb);
            return false;
        }
        new_sel->reference_node_id = fb->selector->reference_node_id;
        new_sel->custom_func = fb->selector->custom_func;
        new_sel->user_data = fb->selector->user_data;
        func_block_set_selector(new_fb, new_sel);
    }

    /* 复制前置条件 */
    if (fb->precondition_count > 0) {
        if (!func_block_set_preconditions(new_fb, fb->precondition_region_ids, fb->precondition_count)) {
            func_block_destroy(new_fb);
            return false;
        }
    }

    /* 复制端口依赖 */
    for (int i = 0; i < fb->port_dep_count; i++) {
        if (!func_block_add_port_dependency(new_fb, &fb->port_deps[i])) {
            func_block_destroy(new_fb);
            return false;
        }
    }

    /* 记录固定的参数映射为端口依赖 */
    for (int i = 0; i < fixed_count; i++) {
        PortDependency dep;
        memset(&dep, 0, sizeof(PortDependency));
        dep.type = PORT_DEP_INCIDENCE;
        dep.port_id = fb->input_port_ids[i];
        dep.external_node_id = fixed_arg_mappings[i];
        dep.internal_node_id = fb->input_port_ids[i];
        dep.constraint_data = NULL;
        func_block_add_port_dependency(new_fb, &dep);
    }

    /* 复制名称和描述 */
    if (fb->name) {
        new_fb->name = lv00_strdup(fb->name);
    }
    if (fb->description) {
        new_fb->description = lv00_strdup(fb->description);
    }

    new_fb->determinism = fb->determinism;
    new_fb->has_measure = fb->has_measure;
    new_fb->measure_node_id = fb->measure_node_id;
    new_fb->measure_compare = fb->measure_compare;
    new_fb->view_state = fb->view_state;

    /* 流式事件：部分应用完成 */
    if (func_block_stream_ctx) {
        stream_emit_simple(func_block_stream_ctx,
                           STREAM_EVENT_FUNC_BLOCK_PARTIAL_APPLY,
                           "部分应用完成",
                           -1);
    }

    *out_new_fb = new_fb;
    return true;
}

/* ============== 捕获避免替换（Capture-Avoiding Substitution） ============== */

/**
 * @brief 收集一个节点在约束图中引用的所有自由变量 ID
 *
 * "自由变量"是指该节点通过 CONNECTION 约束引用的、不属于同一函数块的外部节点。
 * 对于 PORT 类型节点，检查其 connected_to 目标是否属于不同块；
 * 对于非 PORT 类型节点，如果它不属于任何块，则自身就是自由变量。
 *
 * @param graph       约束图
 * @param node_id     起始节点 ID
 * @param block_id    所属函数块 ID（-1 表示不属于任何块）
 * @param out_ids     输出数组（调用者负责 free）
 * @param out_count   输出数量
 * @return true 成功，false 失败
 */
static bool collect_free_vars_for_node(
    ConstraintGraph *graph,
    int node_id,
    int block_id,
    int **out_ids,
    int *out_count)
{
    if (!graph || !out_ids || !out_count) return false;
    *out_ids = NULL;
    *out_count = 0;

    GeomNode *node = graph_get_node(graph, node_id);
    if (!node) return true; /* 节点不存在，无自由变量 */

    /* 对于 PORT 类型节点，检查其 connected_to */
    if (node->type == GEOM_PORT && node->data.port) {
        Port *p = node->data.port;
        if (p->connected_to) {
            int target_id = p->connected_to->id;
            /* 自由变量：连接目标不属于同一块 */
            if (p->parent_block_id != block_id || block_id < 0) {
                *out_ids = lv00_malloc(sizeof(int));
                if (!*out_ids) return false;
                (*out_ids)[0] = target_id;
                *out_count = 1;
            }
        }
        return true;
    }

    /* 对于非 PORT 节点，如果它不属于任何块，它自身就是自由变量 */
    if (block_id < 0 || node->parent_block_id != block_id) {
        *out_ids = lv00_malloc(sizeof(int));
        if (!*out_ids) return false;
        (*out_ids)[0] = node_id;
        *out_count = 1;
    }

    return true;
}

/**
 * @brief 收集函数块体中所有绑定变量（形式参数）的 ID
 *
 * 绑定变量 = 输入端口中 is_formal_param=true 的节点 ID。
 * 如果提供了 graph 参数，会验证端口节点确实存在且类型为 GEOM_PORT。
 *
 * @param block       函数块
 * @param graph       约束图（用于验证端口节点，可为 NULL）
 * @param out_ids     输出数组（调用者负责 free）
 * @param out_count   输出数量
 * @return true 成功，false 失败
 */
static bool collect_bound_vars_in_block(
    const FuncBlock *block,
    ConstraintGraph *graph,
    int **out_ids,
    int *out_count)
{
    if (!block || !out_ids || !out_count) return false;
    *out_ids = NULL;
    *out_count = 0;

    if (block->input_count == 0) return true;

    int *bound = lv00_malloc((size_t)block->input_count * sizeof(int));
    if (!bound) return false;

    int count = 0;
    for (int i = 0; i < block->input_count; i++) {
        int port_id = block->input_port_ids[i];
        GeomNode *n = graph ? graph_get_node(graph, port_id) : NULL;
        if (n && n->type == GEOM_PORT && n->data.port &&
            n->data.port->is_formal_param) {
            bound[count++] = port_id;
        }
    }

    if (count == 0) {
        lv00_free((void **)&bound);
        *out_ids = NULL;
        *out_count = 0;
    } else {
        *out_ids = bound;
        *out_count = count;
    }
    return true;
}

/**
 * @brief 检测变量捕获
 *
 * 检查将 actual_arg_nodes 代入函数块体是否会捕获任何自由变量。
 * 具体做法：
 *   1. 收集实参节点引用的所有自由变量 ID
 *   2. 收集函数块体中所有绑定变量（形式参数）ID
 *   3. 检查是否存在交集（自由变量 ID == 绑定变量 ID）
 *   4. 如果存在，返回这些被捕获的变量 ID
 *
 * 注意：此函数在例化之前调用，此时函数块的内部节点和端口
 * 仍然存在于约束图中。绑定变量直接使用 block->input_port_ids。
 *
 * @param block            函数块
 * @param actual_arg_nodes 实参节点 ID 数组
 * @param arg_count        实参数量
 * @param captured_vars    输出：被捕获的变量 ID 数组（调用者负责 free）
 * @param captured_count   输出：被捕获的变量数量
 * @return true 如果会发生变量捕获，false 否则
 */
static bool detect_variable_capture(
    FuncBlock *block,
    const int *actual_arg_nodes,
    int arg_count,
    int **captured_vars,
    int *captured_count)
{
    if (!block || !captured_vars || !captured_count) return false;
    *captured_vars = NULL;
    *captured_count = 0;

    if (arg_count == 0 || !actual_arg_nodes) return false;

    /*
     * 注意：此函数在例化之前调用，此时函数块的内部节点和端口
     * 仍然存在于约束图中。我们直接使用 block 中的 ID 信息。
     *
     * 绑定变量 = 输入端口的 ID（形式参数）。
     * 自由变量 = 实参节点中引用的外部变量。
     *
     * 由于函数块打包后，形式参数的 ID 是固定的。
     * 如果实参节点自身或其连接目标的 ID 与某个形式参数 ID 相同，
     * 则会发生变量捕获。
     */

    /* 收集绑定变量（形式参数 ID） */
    int *bound_ids = NULL;
    int bound_count = 0;
    /* 不传 graph，直接使用 block 的 input_port_ids 作为绑定变量集合 */
    bound_ids = lv00_malloc((size_t)block->input_count * sizeof(int));
    if (!bound_ids) return false;
    for (int i = 0; i < block->input_count; i++) {
        bound_ids[i] = block->input_port_ids[i];
    }
    bound_count = block->input_count;

    /* 收集实参中的自由变量 ID */
    /* 自由变量 = 实参节点 ID 本身（因为它们来自外部作用域） */
    int free_capacity = arg_count * 2;
    int *free_ids = lv00_malloc((size_t)free_capacity * sizeof(int));
    if (!free_ids) {
        lv00_free((void **)&bound_ids);
        return false;
    }
    int free_count = 0;

    for (int i = 0; i < arg_count; i++) {
        int arg_id = actual_arg_nodes[i];

        /* 实参节点自身是一个自由变量引用 */
        bool already = is_id_in_array(arg_id, free_ids, free_count);
        if (!already) {
            if (free_count >= free_capacity) {
                free_capacity *= 2;
                int *tmp = lv00_realloc(free_ids, (size_t)free_capacity * sizeof(int));
                if (!tmp) {
                    lv00_free((void **)&free_ids);
                    lv00_free((void **)&bound_ids);
                    return false;
                }
                free_ids = tmp;
            }
            free_ids[free_count++] = arg_id;
        }
    }

    /* 检查交集：自由变量中哪些 ID 也出现在绑定变量中 */
    int cap_capacity = 8;
    int *captured = lv00_malloc((size_t)cap_capacity * sizeof(int));
    if (!captured) {
        lv00_free((void **)&free_ids);
        lv00_free((void **)&bound_ids);
        return false;
    }
    int cap_count = 0;

    for (int i = 0; i < free_count; i++) {
        if (is_id_in_array(free_ids[i], bound_ids, bound_count)) {
            if (cap_count >= cap_capacity) {
                cap_capacity *= 2;
                int *tmp = lv00_realloc(captured, (size_t)cap_capacity * sizeof(int));
                if (!tmp) {
                    lv00_free((void **)&captured);
                    lv00_free((void **)&free_ids);
                    lv00_free((void **)&bound_ids);
                    return false;
                }
                captured = tmp;
            }
            captured[cap_count++] = free_ids[i];
        }
    }

    lv00_free((void **)&free_ids);
    lv00_free((void **)&bound_ids);

    *captured_vars = captured;
    *captured_count = cap_count;
    return cap_count > 0;
}

/**
 * @brief 在函数块内执行 alpha-重命名
 *
 * 将函数块中所有对 old_id 的引用替换为 new_id。
 * 扫描范围包括：
 *   - internal_node_ids 数组
 *   - input_port_ids 数组
 *   - output_port_ids 数组
 *   - port_deps 数组中的 external_node_id 和 internal_node_id
 *   - precondition_region_ids 数组
 *
 * 注意：此函数修改的是 FuncBlock 结构体中的 ID 记录，
 * 不修改约束图中的节点。约束图中的节点修改在例化时通过
 * id_map 间接完成。
 *
 * @param block   函数块
 * @param old_id  原始节点 ID
 * @param new_id  新节点 ID
 * @return 0 成功，-1 失败
 */
static int alpha_rename_in_block(
    FuncBlock *block,
    int old_id,
    int new_id)
{
    if (!block || old_id < 0 || new_id < 0) return -1;
    if (old_id == new_id) return 0; /* 无需重命名 */

    /* 替换 internal_node_ids 中的引用 */
    for (int i = 0; i < block->internal_node_count; i++) {
        if (block->internal_node_ids[i] == old_id) {
            block->internal_node_ids[i] = new_id;
        }
    }

    /* 替换 input_port_ids 中的引用 */
    for (int i = 0; i < block->input_count; i++) {
        if (block->input_port_ids[i] == old_id) {
            block->input_port_ids[i] = new_id;
        }
    }

    /* 替换 output_port_ids 中的引用 */
    for (int i = 0; i < block->output_count; i++) {
        if (block->output_port_ids[i] == old_id) {
            block->output_port_ids[i] = new_id;
        }
    }

    /* 替换 port_deps 中的引用 */
    for (int i = 0; i < block->port_dep_count; i++) {
        if (block->port_deps[i].port_id == old_id) {
            block->port_deps[i].port_id = new_id;
        }
        if (block->port_deps[i].external_node_id == old_id) {
            block->port_deps[i].external_node_id = new_id;
        }
        if (block->port_deps[i].internal_node_id == old_id) {
            block->port_deps[i].internal_node_id = new_id;
        }
    }

    /* 替换 precondition_region_ids 中的引用 */
    for (int i = 0; i < block->precondition_count; i++) {
        if (block->precondition_region_ids[i] == old_id) {
            block->precondition_region_ids[i] = new_id;
        }
    }

    return 0;
}

/**
 * @brief 生成一个不与给定 ID 集合冲突的新鲜 ID
 *
 * 从 base 开始向上搜索，找到第一个不在 occupied_ids 中的 ID。
 * 采用线性扫描策略，适用于 ID 空间较稀疏的场景。
 *
 * @param base          起始 ID
 * @param occupied_ids  已占用的 ID 数组
 * @param occupied_count 已占用 ID 数量
 * @return 新鲜 ID
 */
static int generate_fresh_id(int base, const int *occupied_ids, int occupied_count)
{
    int candidate = base;
    while (is_id_in_array(candidate, occupied_ids, occupied_count)) {
        candidate++;
    }
    return candidate;
}

/**
 * @brief 收集约束图中所有已使用的节点 ID
 *
 * 遍历约束图的节点数组，提取所有节点的 ID。
 * 用于在 alpha-重命名时确定已占用的 ID 空间。
 *
 * @param graph  约束图
 * @param out_ids    输出数组（调用者负责 free）
 * @param out_count  输出数量
 * @return true 成功，false 失败
 */
static bool collect_all_graph_node_ids(
    const ConstraintGraph *graph,
    int **out_ids,
    int *out_count)
{
    if (!graph || !out_ids || !out_count) return false;
    *out_count = graph->node_count;
    if (graph->node_count == 0) {
        *out_ids = NULL;
        return true;
    }
    *out_ids = lv00_malloc((size_t)graph->node_count * sizeof(int));
    if (!*out_ids) return false;
    for (int i = 0; i < graph->node_count; i++) {
        (*out_ids)[i] = graph->nodes[i]->id;
    }
    return true;
}

/**
 * @brief 执行捕获避免的例化
 *
 * 这是 func_block_instantiate() 的增强版本，在例化之前执行：
 *   1. 捕获检测：检查实参中的自由变量是否会与函数块的形式参数冲突
 *   2. Alpha-重命名：如果检测到捕获，对函数块中的绑定变量进行重命名
 *   3. 例化：调用现有的 func_block_instantiate() 完成实际的例化操作
 *
 * 设计文档 3.3 节要求的三种情况（A/B/C）由底层
 * func_block_instantiate() 处理，本函数在此基础上增加了
 * 捕获避免的安全性保证。
 *
 * @param block             函数块
 * @param actual_arg_nodes  实参节点 ID 数组
 * @param arg_count         实参数量
 * @param target_graph      目标约束图
 * @param output_node_ids   输出：内部节点到外部节点 ID 的映射
 * @param output_node_count 输出：映射数量
 * @return INSTANTIATE_OK 成功，其他值表示失败
 */
InstantiateResult func_block_instantiate_capture_avoiding(
    FuncBlock *block,
    const int *actual_arg_nodes,
    int arg_count,
    ConstraintGraph *target_graph,
    int **output_node_ids,
    int *output_node_count)
{
    if (!block || !target_graph || !output_node_ids || !output_node_count) {
        return INSTANTIATE_NO_SOLUTION;
    }

    *output_node_ids = NULL;
    *output_node_count = 0;

    /* 流式事件：捕获避免例化开始 */
    if (func_block_stream_ctx) {
        stream_emit_simple(func_block_stream_ctx, STREAM_EVENT_FUNC_BLOCK_CAPTURE_AVOID,
            "捕获避免例化开始", 0);
    }

    /* 步骤 1：检测变量捕获 */
    int *captured_vars = NULL;
    int captured_count = 0;
    bool has_capture = detect_variable_capture(
        block, actual_arg_nodes, arg_count,
        &captured_vars, &captured_count);

    if (has_capture) {
        /*
         * 步骤 2：对每个被捕获的绑定变量执行 alpha-重命名
         *
         * 为每个被捕获的形式参数生成一个新鲜 ID，
         * 然后在函数块结构体中将旧 ID 替换为新 ID。
         *
         * 注意：这只修改 FuncBlock 结构体中的 ID 记录。
         * 约束图中对应的节点不会被修改（因为打包后的节点
         * 在图中是只读的模板）。例化时 instantiate_copy_internal_nodes
         * 会根据 FuncBlock 中的 ID 信息创建新的副本。
         */

        /* 收集所有已占用的 ID，用于生成新鲜 ID */
        int *occupied_ids = NULL;
        int occupied_count = 0;
        collect_all_graph_node_ids(target_graph, &occupied_ids, &occupied_count);

        /* 同时将函数块自身的内部节点 ID 也加入占用集合 */
        int total_occupied = occupied_count + block->internal_node_count
                           + block->input_count + block->output_count;
        int *all_occupied = lv00_malloc((size_t)total_occupied * sizeof(int));
        if (all_occupied) {
            int idx = 0;
            for (int i = 0; i < occupied_count; i++)
                all_occupied[idx++] = occupied_ids[i];
            for (int i = 0; i < block->internal_node_count; i++)
                all_occupied[idx++] = block->internal_node_ids[i];
            for (int i = 0; i < block->input_count; i++)
                all_occupied[idx++] = block->input_port_ids[i];
            for (int i = 0; i < block->output_count; i++)
                all_occupied[idx++] = block->output_port_ids[i];
            total_occupied = idx;

            /* 以 target_graph->next_node_id 为基数生成新鲜 ID */
            int base = target_graph->next_node_id;

            for (int i = 0; i < captured_count; i++) {
                int old_id = captured_vars[i];
                int fresh_id = generate_fresh_id(base, all_occupied, total_occupied);
                if (fresh_id != old_id) {
                    alpha_rename_in_block(block, old_id, fresh_id);
                    /* 将新 ID 加入占用集合，避免后续冲突 */
                    if (idx < total_occupied) {
                        all_occupied[idx++] = fresh_id;
                        total_occupied = idx;
                    }
                    base = fresh_id + 1;
                }
            }
            lv00_free((void **)&all_occupied);
        }
        lv00_free((void **)&occupied_ids);
        lv00_free((void **)&captured_vars);
    }

    /*
     * 步骤 3：调用现有的例化逻辑
     *
     * actual_arg_nodes 作为 arg_mappings 传入。
     * 注意：func_block_instantiate 期望 arg_mappings 是可修改的 int*，
     * 而 actual_arg_nodes 是 const int*，因此需要复制。
     */
    int *arg_mappings = NULL;
    if (arg_count > 0 && actual_arg_nodes) {
        arg_mappings = lv00_malloc((size_t)arg_count * sizeof(int));
        if (!arg_mappings) return INSTANTIATE_OUT_OF_MEMORY;
        memcpy(arg_mappings, actual_arg_nodes, (size_t)arg_count * sizeof(int));
    }

    InstantiateResult result = func_block_instantiate(
        block, target_graph, arg_mappings, arg_count,
        output_node_ids, output_node_count);

    /* 流式事件：捕获避免例化完成 */
    if (func_block_stream_ctx) {
        stream_emit_simple(func_block_stream_ctx, STREAM_EVENT_FUNC_BLOCK_INSTANTIATE_DONE,
            result == INSTANTIATE_OK ? "捕获避免例化完成" : "捕获避免例化失败", 1);
    }

    lv00_free((void **)&arg_mappings);
    return result;
}

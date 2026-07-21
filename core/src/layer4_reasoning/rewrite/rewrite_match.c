/**
 * @file rewrite_match.c
 * @brief 匹配查找（WL散列 + 图快照）
 *
 * @details 拆分子模块（Lv-00 v3.3.0+）。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lv00/rewrite.h"
#include "lv00/constraint_graph.h"
#include "debug.h"
#include "lv00_internal.h"
#include "lv00_utils.h"
#include "mpz_poly.h"

extern uint64_t compute_wl_graph_hash(ConstraintGraph *graph);
uint32_t compute_graph_hash(ConstraintGraph *graph);

/* ---------------------------------------------------------------------------
 * 内部辅助函数
 * ------------------------------------------------------------------------- */

/* 前向声明 */
extern bool evaluate_precondition(ConstraintGraph *graph,
                                   RewriteRule *rule,
                                   RewriteMatch *match);

/**
 * @brief FNV-1a 64位哈希混合（与项目统一的 64 位哈希体系一致）
 *
 * 对输入数据逐字节进行 FNV-1a 哈希混合：hash ^= byte; hash *= FNV_prime。
 * 使用 64 位哈希替代原先的 32 位版本，降低碰撞概率。
 *
 * @param hash 当前哈希值（64位）
 * @param data 待混合的数据指针
 * @param len  数据长度（字节数）
 * @return 混合后的新哈希值（64位）
 */
static uint64_t fnv1a_mix(uint64_t hash, const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;
    for (size_t i = 0; i < len; i++) {
        hash ^= p[i];
        hash *= LV00_FNV64_PRIME;  /* 使用 64 位 FNV 质数，与项目统一 */
    }
    return hash;
}

/**
 * @brief 从匹配绑定表中解析模式变量对应的实际图节点 ID
 *
 * 模式变量（负 ID）通过匹配绑定表映射到约束图中的实际节点。
 * 遍历绑定表查找匹配项，未找到返回 -1。
 *
 * @param bindings      节点绑定数组（[pattern_id, actual_id] 交错）
 * @param binding_count 绑定对数量
 * @param pattern_var_id 待解析的模式变量 ID（负值）
 * @return 对应的实际图节点 ID，未找到返回 -1
 */
int resolve_binding(const int *bindings, int binding_count, int pattern_var_id) {
    for (int i = 0; i < binding_count; i++) {
        if (bindings[i * 2] == pattern_var_id) {
            return bindings[i * 2 + 1];
        }
    }
    return -1;
}

/**
 * @brief 检查模式变量是否在替换约束中被引用
 *
 * 遍历替换约束数组中的每个约束及其参与者，
 * 判断给定的模式变量 ID 是否出现在替换中。
 *
 * @param repl            替换规则描述
 * @param pattern_var_id  待检查的模式变量 ID
 * @return true 如果该变量在替换约束中被引用，否则 false
 */
bool pattern_var_used_in_replacement(const RewriteReplacement *repl, int pattern_var_id) {
    for (int c = 0; c < repl->replacement_constraint_count; c++) {
        Constraint *rc = repl->replacement_constraints[c];
        for (int p = 0; p < rc->participant_count; p++) {
            if (rc->participants[p] == pattern_var_id) {
                return true;
            }
        }
    }
    return false;
}

/**
 * @brief 检查模式变量是否出现在替换的节点绑定表中
 *
 * 在替换的 node_bindings 表中搜索给定的模式变量 ID。
 * node_bindings 定义了替换后如何重新映射模式变量到新节点。
 *
 * @param repl            替换规则描述
 * @param pattern_var_id  待检查的模式变量 ID
 * @return true 如果该变量在绑定表中，否则 false
 */
bool pattern_var_in_replacement_bindings(const RewriteReplacement *repl, int pattern_var_id) {
    for (int b = 0; b < repl->binding_count; b++) {
        if (repl->node_bindings[b][0] == pattern_var_id) {
            return true;
        }
    }
    return false;
}

/**
 * @brief 解析替换约束中的参与者 ID 为实际图节点 ID
 *
 * 根据参与者 ID 的类型进行不同处理：
 * - 负值（模式变量）：在匹配绑定表中查找
 * - 正值但在 new_nodes 列表中：使用新创建节点的映射
 * - 正值且不在模式变量中：外部/边界节点，保持不变
 *
 * @param participant_id    参与者 ID
 * @param match_bindings    匹配绑定数组
 * @param match_binding_count 绑定数量
 * @param new_node_map      新节点映射数组
 * @param new_node_map_count 映射数量
 * @param new_nodes         新节点 ID 数组
 * @param new_node_count    新节点数量
 * @return 解析后的实际图节点 ID，失败返回 -1
 */
int resolve_replacement_participant(
    int participant_id,
    const int *match_bindings,
    int match_binding_count,
    const int *new_node_map,   /* 将替换 new_node 索引映射到实际图节点 ID */
    int new_node_map_count,
    const int *new_nodes,      /* replacement->new_nodes 数组 */
    int new_node_count)
{
    if (participant_id < 0) {
        /* 模式变量：在匹配绑定表中查找 */
        return resolve_binding(match_bindings, match_binding_count, participant_id);
    }

    /* 检查是否是替换中的新节点引用。
       替换中的新节点通过 new_nodes 数组中的位置标识，
       替换约束通过 new_nodes[i] 中存储的相同值来引用它们。 */
    for (int i = 0; i < new_node_count; i++) {
        if (new_nodes[i] == participant_id) {
                    /* 映射到新创建的图节点 ID */
            if (i < new_node_map_count) {
                return new_node_map[i];
            }
            return -1;
        }
    }

    /* 外部/边界节点：保持 ID 不变 */
    return participant_id;
}

/**
 * @brief 向约束图中添加通用约束
 *
 * 根据约束类型和已解析的参与者 ID 数组，
 * 调用对应的图添加函数。支持关联、中间、交点、包含、连接五种约束类型。
 *
 * @param graph             目标约束图
 * @param type              约束类型
 * @param participants      参与者 ID 数组（已解析到实际图节点）
 * @param participant_count 参与者数量
 * @return true 添加成功，false 失败（类型不支持或参数不匹配）
 */
bool add_constraint_generic(ConstraintGraph *graph,
                                   ConstraintType type,
                                   const int *participants,
                                   int participant_count)
{
    switch (type) {
        case INCIDENCE:
            if (participant_count == 2)
                return graph_add_incidence(graph, participants[0], participants[1]) == ADD_CONSTRAINT_OK;
            break;
        case BETWEENNESS:
            if (participant_count == 3)
                return graph_add_betweenness(graph, participants[0], participants[1], participants[2]) == ADD_CONSTRAINT_OK;
            break;
        case INTERSECTION:
            if (participant_count == 3)
                return graph_add_intersection(graph, participants[0], participants[1], participants[2]) == ADD_CONSTRAINT_OK;
            break;
        case CONTAINMENT:
            if (participant_count == 2)
                return graph_add_containment(graph, participants[0], participants[1]) == ADD_CONSTRAINT_OK;
            break;
        case CONNECTION:
            if (participant_count == 2)
                return graph_add_connection(graph, participants[0], participants[1]) == ADD_CONSTRAINT_OK;
            break;
    }
    return false;
}

/**
 * @brief 检查约束 ID 是否在匹配的约束绑定列表中
 *
 * 遍历匹配对象中的 constraint_bindings 数组，
 * 判断给定约束是否已被匹配覆盖。
 *
 * @param match         重写匹配对象
 * @param constraint_id 待检查的约束 ID
 * @return true 如果该约束已被匹配，否则 false
 */
/**
 * @brief 检查节点 ID 是否已绑定到匹配中的某个模式变量
 *
 * 在匹配的 node_bindings 中查找该节点 ID。node_bindings
 * 以 [pattern_id, actual_id] 交错存储，此处匹配第二个元素。
 *
 * @param match   重写匹配对象
 * @param node_id 待检查的实际图节点 ID
 * @return true 如果该节点已被模式变量绑定，否则 false
 */
static bool is_pattern_bound_node(const RewriteMatch *match, int node_id) {
    for (int i = 0; i < match->binding_count; i++) {
        if (match->node_bindings[i * 2 + 1] == node_id) {
            return true;
        }
    }
    return false;
}

/**
 * @brief 检查约束 ID 是否在匹配的约束绑定列表中
 *
 * 遍历匹配对象中的约束绑定数组，判断给定约束是否已被匹配覆盖。
 *
 * @param match         重写匹配对象
 * @param constraint_id 待检查的约束 ID
 * @return true 如果该约束已被匹配，否则 false
 */
bool is_matched_constraint(const RewriteMatch *match, int constraint_id) {
    for (int i = 0; i < match->binding_count; i++) {
        if (match->constraint_bindings[i] == constraint_id) {
            return true;
        }
    }
    return false;
}

/**
 * @brief 快速一致性检查：在重写后检测明显的冲突
 *
 * 调用 graph_detect_conflicts 检测约束图中的冲突。
 *
 * @param graph 约束图指针
 * @return true 表示图看起来一致（无冲突），false 表示检测到冲突
 * @note graph_detect_conflicts 返回的 conflicts 数组和 conflict_sizes 数组
 *       需要分别释放；两者可能为 NULL（无冲突或分配失败时）。
 *       此函数不区分"无冲突"和"分配失败"两种情况，均视为通过。
 */
bool check_graph_consistency(ConstraintGraph *graph) {
    int conflict_count = 0;
    int *conflict_sizes = NULL;
    int **conflicts = graph_detect_conflicts(graph, &conflict_count, &conflict_sizes);
    if (conflicts && conflict_count > 0) {
        /* 存在冲突：释放冲突数组并返回 false */
        if (conflict_sizes) lv00_free((void**)&conflict_sizes);
        if (conflicts) lv00_free((void**)&conflicts);
        return false;
    }
    /* 无冲突或 conflicts 为 NULL（分配失败）：释放资源并返回 true */
    if (conflict_sizes) lv00_free((void**)&conflict_sizes);
    if (conflicts) lv00_free((void**)&conflicts);
    return true;
}

/* ---------------------------------------------------------------------------
 * Graph Snapshot — 用于重写替换操作的事务性回滚
 * ------------------------------------------------------------------------- */

/* 深拷贝单个 GeomNode
 *
 * 【内存管理策略】此函数对所有动态分配的字段执行深拷贝：
 *   - symbolic_coords: 对每个坐标调用 symbolic_coord_copy()（堆分配独立副本）
 *   - numeric_assumption_declaration: 通过 lv00_strdup_safe() 复制字符串（堆分配独立副本）
 *   - data.port / data.region / data.func_block: 分配独立副本，但内部指针
 *     （如 connected_to、boundary_segments、internal_nodes）在拷贝时置为 NULL，
 *     需要在图快照恢复阶段通过 ID 映射重新绑定
 *
 * 所有权模型：返回的 GeomNode 由调用者拥有，需通过 free_geomnodes_and_data()
 * 或 graph_snapshot_destroy() 释放。可被部分失败的分配可通过返回前回滚已分配
 * 资源来保持无泄漏。
 *
 * @param src 源节点（不修改）
 * @return 深拷贝的节点，失败返回 NULL（已分配资源已回滚）
 */
static GeomNode *graph_node_deep_copy(const GeomNode *src) {
    if (!src) return NULL;
    GeomNode *dst = lv00_malloc(sizeof(GeomNode));
    if (!dst) return NULL;
    memcpy(dst, src, sizeof(GeomNode));
    /* 清零 union data，避免 GEOM_POINT 等类型继承源节点的悬垂指针 */
    memset(&dst->data, 0, sizeof(dst->data));

    /* 深拷贝符号坐标 */
    dst->symbolic_coords = NULL;
    if (src->coord_count > 0 && src->symbolic_coords) {
        dst->symbolic_coords = lv00_malloc((size_t)src->coord_count * sizeof(SymbolicCoord *));
        if (dst->symbolic_coords) {
            for (int c = 0; c < src->coord_count; c++) {
                dst->symbolic_coords[c] = symbolic_coord_copy(src->symbolic_coords[c]);
                if (!dst->symbolic_coords[c]) {
                    for (int j = 0; j < c; j++) symbolic_coord_destroy(dst->symbolic_coords[j]);
                    lv00_free((void**)&dst->symbolic_coords);
                    dst->symbolic_coords = NULL;
                    dst->coord_count = 0;
                    lv00_free((void**)&dst);
                    return NULL;
                }
            }
        }
    }

    /* 深拷贝 numeric_assumption_declaration
     * 【内存管理策略】strdup 在堆上分配独立副本，所有权转移给新节点 dst。
     * 调用者无需关心源字符串的生命周期。若分配失败（返回 NULL），
     * 整个深拷贝操作视为失败，需回滚已分配的所有资源。 */
    dst->numeric_assumption_declaration = NULL;
    if (src->numeric_assumption_declaration) {
        dst->numeric_assumption_declaration = lv00_strdup_safe(src->numeric_assumption_declaration);
        if (!dst->numeric_assumption_declaration) {
            /* strdup 分配失败：回滚已分配的符号坐标资源 */
            if (dst->symbolic_coords) {
                for (int j = 0; j < src->coord_count; j++) {
                    if (dst->symbolic_coords[j])
                        symbolic_coord_destroy(dst->symbolic_coords[j]);
                }
                lv00_free((void**)&dst->symbolic_coords);
            }
            lv00_free((void**)&dst);
            return NULL;
        }
    }

    /* 深拷贝类型特定数据 */
    switch (src->type) {
        case GEOM_PORT: {
            if (src->data.port) {
                dst->data.port = lv00_malloc(sizeof(Port));
                if (dst->data.port) {
                    memcpy(dst->data.port, src->data.port, sizeof(Port));
                    dst->data.port->connected_to = NULL; /* 指针在恢复后需要重建 */
                }
            }
            break;
        }
        case GEOM_REGION: {
            dst->data.region.boundary_segments = NULL;
            dst->data.region.segment_count = 0;
            if (src->data.region.segment_count > 0 && src->data.region.boundary_segments) {
                dst->data.region.boundary_segments = lv00_malloc(
                    (size_t)src->data.region.segment_count * sizeof(GeomNode *));
                if (dst->data.region.boundary_segments) {
                    dst->data.region.segment_count = src->data.region.segment_count;
                    /* 指针置空，恢复时根据 ID 重新绑定 */
                    memset(dst->data.region.boundary_segments, 0,
                           (size_t)src->data.region.segment_count * sizeof(GeomNode *));
                }
            }
            break;
        }
        case GEOM_FUNCTION_BLOCK: {
            dst->data.func_block.internal_nodes = NULL;
            dst->data.func_block.input_port_ids = NULL;
            dst->data.func_block.output_port_ids = NULL;
            dst->data.func_block.internal_node_count = 0;
            dst->data.func_block.input_count = 0;
            dst->data.func_block.output_count = 0;
            dst->data.func_block.determinism_state = src->data.func_block.determinism_state;

            if (src->data.func_block.internal_node_count > 0 && src->data.func_block.internal_nodes) {
                dst->data.func_block.internal_nodes = lv00_malloc(
                    (size_t)src->data.func_block.internal_node_count * sizeof(GeomNode *));
                if (dst->data.func_block.internal_nodes) {
                    dst->data.func_block.internal_node_count = src->data.func_block.internal_node_count;
                    memset(dst->data.func_block.internal_nodes, 0,
                           (size_t)src->data.func_block.internal_node_count * sizeof(GeomNode *));
                }
            }
            if (src->data.func_block.input_count > 0 && src->data.func_block.input_port_ids) {
                dst->data.func_block.input_port_ids = lv00_malloc(
                    (size_t)src->data.func_block.input_count * sizeof(int));
                if (dst->data.func_block.input_port_ids) {
                    memcpy(dst->data.func_block.input_port_ids, src->data.func_block.input_port_ids,
                           (size_t)src->data.func_block.input_count * sizeof(int));
                    dst->data.func_block.input_count = src->data.func_block.input_count;
                }
            }
            if (src->data.func_block.output_count > 0 && src->data.func_block.output_port_ids) {
                dst->data.func_block.output_port_ids = lv00_malloc(
                    (size_t)src->data.func_block.output_count * sizeof(int));
                if (dst->data.func_block.output_port_ids) {
                    memcpy(dst->data.func_block.output_port_ids, src->data.func_block.output_port_ids,
                           (size_t)src->data.func_block.output_count * sizeof(int));
                    dst->data.func_block.output_count = src->data.func_block.output_count;
                }
            }
            break;
        }
        default:
            break;
    }

    return dst;
}

/**
 * @brief 销毁快照中的单个节点
 *
 * @param node 待销毁的节点指针
 */
static void snapshot_node_destroy(GeomNode *node) {
    if (!node) return;
    if (node->symbolic_coords) {
        for (int c = 0; c < node->coord_count; c++) {
            symbolic_coord_destroy(node->symbolic_coords[c]);
        }
        lv00_free((void**)&node->symbolic_coords);
    }
    lv00_free((void**)&node->numeric_assumption_declaration);
    switch (node->type) {
        case GEOM_PORT:
            lv00_free((void**)&node->data.port);
            break;
        case GEOM_REGION:
            lv00_free((void**)&node->data.region.boundary_segments);
            break;
        case GEOM_FUNCTION_BLOCK:
            lv00_free((void**)&node->data.func_block.internal_nodes);
            lv00_free((void**)&node->data.func_block.input_port_ids);
            lv00_free((void**)&node->data.func_block.output_port_ids);
            break;
        default:
            break;
    }
    lv00_free((void**)&node);
}

/**
 * @brief 创建约束图的快照
 *
 * 用于重写替换操作的事务性回滚。对图中所有节点和约束进行深拷贝，
 * 并收集交叉引用信息（端口连接、区域边界、功能块内部节点）用于恢复。
 *
 * @param graph 源约束图指针
 * @return 新分配的图快照，失败返回 NULL
 */
GraphSnapshot *graph_snapshot_create(const ConstraintGraph *graph) {
    if (!graph) return NULL;

    GraphSnapshot *snap = lv00_malloc(sizeof(GraphSnapshot));
    if (!snap) return NULL;
    memset(snap, 0, sizeof(GraphSnapshot));

    snap->node_count = graph->node_count;
    snap->node_capacity = graph->node_count > 0 ? graph->node_count : 1;
    snap->constraint_count = graph->constraint_count;
    snap->constraint_capacity = graph->constraint_count > 0 ? graph->constraint_count : 1;
    snap->next_node_id = graph->next_node_id;
    snap->next_constraint_id = graph->next_constraint_id;

    /* 收集交叉引用信息（在深拷贝之前，因为深拷贝会清零指针） */
    /* 第一遍：计数 */
    int port_ref_count = 0, region_ref_count = 0, fb_ref_count = 0;
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *n = graph->nodes[i];
        if (n->type == GEOM_PORT && n->data.port) port_ref_count++;
        if (n->type == GEOM_REGION && n->data.region.segment_count > 0) region_ref_count++;
        if (n->type == GEOM_FUNCTION_BLOCK && n->data.func_block.internal_node_count > 0) fb_ref_count++;
    }

    /* 分配并填充 port_refs */
    snap->port_ref_count = port_ref_count;
    snap->port_refs = NULL;
    if (port_ref_count > 0) {
        snap->port_refs = lv00_malloc((size_t)port_ref_count * sizeof(PortRef));
        if (snap->port_refs) {
            int idx = 0;
            for (int i = 0; i < graph->node_count; i++) {
                GeomNode *n = graph->nodes[i];
                if (n->type == GEOM_PORT && n->data.port) {
                    snap->port_refs[idx].port_node_index = i;
                    snap->port_refs[idx].connected_to_id =
                        n->data.port->connected_to ? n->data.port->connected_to->id : -1;
                    idx++;
                }
            }
        } else {
            /* port_refs 分配失败：将 port_ref_count 重置为 0，
             * 确保后续恢复时不会访问无效的 port_refs 指针。
             * 此时快照中缺少端口连接信息，恢复后端口连接将丢失。 */
            snap->port_ref_count = 0;
            LOG_WARN("rewrite", "graph_snapshot_create: port_refs 分配失败 (count=%d)", port_ref_count);
        }
    }

    /* 分配并填充 region_refs */
    snap->region_ref_count = region_ref_count;
    snap->region_refs = NULL;
    if (region_ref_count > 0) {
        snap->region_refs = lv00_malloc((size_t)region_ref_count * sizeof(RegionRef));
        if (snap->region_refs) {
            int idx = 0;
            for (int i = 0; i < graph->node_count; i++) {
                GeomNode *n = graph->nodes[i];
                if (n->type == GEOM_REGION && n->data.region.segment_count > 0 &&
                    n->data.region.boundary_segments) {
                    snap->region_refs[idx].region_node_index = i;
                    snap->region_refs[idx].segment_count = n->data.region.segment_count;
                    snap->region_refs[idx].segment_ids = lv00_malloc(
                        (size_t)n->data.region.segment_count * sizeof(int));
                    if (snap->region_refs[idx].segment_ids) {
                        for (int k = 0; k < n->data.region.segment_count; k++) {
                            snap->region_refs[idx].segment_ids[k] =
                                n->data.region.boundary_segments[k] ?
                                n->data.region.boundary_segments[k]->id : -1;
                        }
                    }
                    idx++;
                }
            }
        } else {
            snap->region_ref_count = 0;
        }
    }

    /* 分配并填充 fb_refs */
    snap->fb_ref_count = fb_ref_count;
    snap->fb_refs = NULL;
    if (fb_ref_count > 0) {
        snap->fb_refs = lv00_malloc((size_t)fb_ref_count * sizeof(FBRef));
        if (snap->fb_refs) {
            int idx = 0;
            for (int i = 0; i < graph->node_count; i++) {
                GeomNode *n = graph->nodes[i];
                if (n->type == GEOM_FUNCTION_BLOCK &&
                    n->data.func_block.internal_node_count > 0 &&
                    n->data.func_block.internal_nodes) {
                    snap->fb_refs[idx].fb_node_index = i;
                    snap->fb_refs[idx].internal_node_count = n->data.func_block.internal_node_count;
                    snap->fb_refs[idx].internal_node_ids = lv00_malloc(
                        (size_t)n->data.func_block.internal_node_count * sizeof(int));
                    if (snap->fb_refs[idx].internal_node_ids) {
                        for (int k = 0; k < n->data.func_block.internal_node_count; k++) {
                            snap->fb_refs[idx].internal_node_ids[k] =
                                n->data.func_block.internal_nodes[k] ?
                                n->data.func_block.internal_nodes[k]->id : -1;
                        }
                    }
                    idx++;
                }
            }
        } else {
            snap->fb_ref_count = 0;
        }
    }

    /* 深拷贝节点数组 */
    snap->nodes = lv00_malloc((size_t)snap->node_capacity * sizeof(GeomNode *));
    if (!snap->nodes) {
        /* cleanup refs */
        lv00_free((void**)&snap->port_refs);
        for (int i = 0; i < snap->region_ref_count; i++) lv00_free((void**)&snap->region_refs[i].segment_ids);
        lv00_free((void**)&snap->region_refs);
        for (int i = 0; i < snap->fb_ref_count; i++) lv00_free((void**)&snap->fb_refs[i].internal_node_ids);
        lv00_free((void**)&snap->fb_refs);
        lv00_free((void**)&snap);
        return NULL;
    }
    for (int i = 0; i < graph->node_count; i++) {
        snap->nodes[i] = graph_node_deep_copy(graph->nodes[i]);
        if (!snap->nodes[i]) {
            /* 回滚已分配的节点 */
            for (int j = 0; j < i; j++) snapshot_node_destroy(snap->nodes[j]);
            lv00_free((void**)&snap->nodes);
            lv00_free((void**)&snap);
            return NULL;
        }
    }

    /* 深拷贝约束数组 */
    snap->constraints = lv00_malloc((size_t)snap->constraint_capacity * sizeof(Constraint *));
    if (!snap->constraints) {
        for (int i = 0; i < snap->node_count; i++) snapshot_node_destroy(snap->nodes[i]);
        lv00_free((void**)&snap->nodes);
        /* cleanup refs */
        lv00_free((void**)&snap->port_refs);
        for (int i = 0; i < snap->region_ref_count; i++) lv00_free((void**)&snap->region_refs[i].segment_ids);
        lv00_free((void**)&snap->region_refs);
        for (int i = 0; i < snap->fb_ref_count; i++) lv00_free((void**)&snap->fb_refs[i].internal_node_ids);
        lv00_free((void**)&snap->fb_refs);
        lv00_free((void**)&snap);
        return NULL;
    }
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *src = graph->constraints[i];
        Constraint *dst = lv00_malloc(sizeof(Constraint));
        if (!dst) {
            for (int j = 0; j < i; j++) {
                lv00_free((void**)&snap->constraints[j]->participants);
                lv00_free((void**)&snap->constraints[j]);
            }
            for (int j = 0; j < snap->node_count; j++) snapshot_node_destroy(snap->nodes[j]);
            lv00_free((void**)&snap->constraints);
            lv00_free((void**)&snap->nodes);
            lv00_free((void**)&snap);
            return NULL;
        }
        dst->id = src->id;
        dst->type = src->type;
        dst->template_id = src->template_id;
        dst->participant_count = src->participant_count;
        dst->participants = NULL;
        if (src->participant_count > 0 && src->participants) {
            dst->participants = lv00_malloc((size_t)src->participant_count * sizeof(int));
            if (dst->participants) {
                memcpy(dst->participants, src->participants,
                       (size_t)src->participant_count * sizeof(int));
            }
        }
        snap->constraints[i] = dst;
    }

    return snap;
}

/* 从快照恢复约束图。
 *
 * 【重要风险说明】
 * 此函数首先销毁当前图中的所有节点和约束，然后从快照重建。
 * 如果在销毁之后的重建过程中发生内存分配失败，图将被重置为空图状态
 * （所有指针置 NULL，计数归零），而非停留在半销毁的不一致状态。
 * 调用者应检查返回值：返回 false 表示恢复失败，图已被重置为空图。
 *
 * 参数：
 *   snapshot - 之前通过 graph_snapshot_create 创建的快照
 *   graph    - 要恢复的目标约束图
 * 返回：
 *   true  - 恢复成功
 *   false - 恢复失败（内存不足），图已被重置为空图
 */
bool graph_snapshot_restore(GraphSnapshot *snapshot, ConstraintGraph *graph) {
    if (!snapshot || !graph) return false;

    /* 1. 销毁当前图中的所有节点和约束 */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (node->symbolic_coords) {
            for (int c = 0; c < node->coord_count; c++) {
                symbolic_coord_destroy(node->symbolic_coords[c]);
            }
            lv00_free((void**)&node->symbolic_coords);
        }
        lv00_free((void**)&node->numeric_assumption_declaration);
        switch (node->type) {
            case GEOM_PORT:
                lv00_free((void**)&node->data.port);
                break;
            case GEOM_REGION:
                lv00_free((void**)&node->data.region.boundary_segments);
                break;
            case GEOM_FUNCTION_BLOCK:
                lv00_free((void**)&node->data.func_block.internal_nodes);
                lv00_free((void**)&node->data.func_block.input_port_ids);
                lv00_free((void**)&node->data.func_block.output_port_ids);
                break;
            default:
                break;
        }
        lv00_free((void**)&node);
    }
    for (int i = 0; i < graph->constraint_count; i++) {
        lv00_free((void**)&graph->constraints[i]->participants);
        lv00_free((void**)&graph->constraints[i]);
    }
    lv00_free((void**)&graph->nodes);
    lv00_free((void**)&graph->constraints);
    lv00_free((void**)&graph->node_index);
    lv00_free((void**)&graph->constraint_index);

    /* 2. 从快照恢复所有节点和约束（深拷贝） */
    graph->node_count = snapshot->node_count;
    graph->node_capacity = snapshot->node_capacity;
    graph->constraint_count = snapshot->constraint_count;
    graph->constraint_capacity = snapshot->constraint_capacity;
    graph->next_node_id = snapshot->next_node_id;
    graph->next_constraint_id = snapshot->next_constraint_id;

    graph->nodes = lv00_malloc((size_t)graph->node_capacity * sizeof(GeomNode *));
    if (!graph->nodes) {
        /* 将图重置为空图状态，避免半销毁 */
        graph->nodes = NULL;
        graph->node_count = 0;
        graph->node_capacity = 0;
        graph->constraints = NULL;
        graph->constraint_count = 0;
        graph->constraint_capacity = 0;
        graph->node_index = NULL;
        graph->node_index_capacity = 0;
        graph->constraint_index = NULL;
        graph->constraint_index_capacity = 0;
        return false;
    }
    for (int i = 0; i < snapshot->node_count; i++) {
        graph->nodes[i] = graph_node_deep_copy(snapshot->nodes[i]);
        if (!graph->nodes[i]) {
            /* 清理已分配的部分节点数据 */
            for (int j = 0; j < i; j++) {
                snapshot_node_destroy(graph->nodes[j]);
            }
            lv00_free((void**)&graph->nodes);
            graph->nodes = NULL;
            graph->node_count = 0;
            graph->node_capacity = 0;
            graph->constraints = NULL;
            graph->constraint_count = 0;
            graph->constraint_capacity = 0;
            graph->node_index = NULL;
            graph->node_index_capacity = 0;
            graph->constraint_index = NULL;
            graph->constraint_index_capacity = 0;
            return false;
        }
    }

    graph->constraints = lv00_malloc((size_t)graph->constraint_capacity * sizeof(Constraint *));
    if (!graph->constraints) {
        /* 清理已恢复的节点数据，将图重置为空图状态 */
        for (int i = 0; i < graph->node_count; i++) {
            snapshot_node_destroy(graph->nodes[i]);
        }
        lv00_free((void**)&graph->nodes);
        graph->nodes = NULL;
        graph->node_count = 0;
        graph->node_capacity = 0;
        graph->constraints = NULL;
        graph->constraint_count = 0;
        graph->constraint_capacity = 0;
        graph->node_index = NULL;
        graph->node_index_capacity = 0;
        graph->constraint_index = NULL;
        graph->constraint_index_capacity = 0;
        return false;
    }
    for (int i = 0; i < snapshot->constraint_count; i++) {
        Constraint *src = snapshot->constraints[i];
        Constraint *dst = lv00_malloc(sizeof(Constraint));
        if (!dst) {
            /* 清理已分配的部分约束数据 */
            for (int j = 0; j < i; j++) {
                lv00_free((void**)&graph->constraints[j]->participants);
                lv00_free((void**)&graph->constraints[j]);
            }
            lv00_free((void**)&graph->constraints);
            for (int j = 0; j < graph->node_count; j++) {
                snapshot_node_destroy(graph->nodes[j]);
            }
            lv00_free((void**)&graph->nodes);
            graph->nodes = NULL;
            graph->node_count = 0;
            graph->node_capacity = 0;
            graph->constraints = NULL;
            graph->constraint_count = 0;
            graph->constraint_capacity = 0;
            graph->node_index = NULL;
            graph->node_index_capacity = 0;
            graph->constraint_index = NULL;
            graph->constraint_index_capacity = 0;
            return false;
        }
        dst->id = src->id;
        dst->type = src->type;
        dst->template_id = src->template_id;
        dst->participant_count = src->participant_count;
        dst->participants = NULL;
        if (src->participant_count > 0 && src->participants) {
            dst->participants = lv00_malloc((size_t)src->participant_count * sizeof(int));
            if (dst->participants) {
                memcpy(dst->participants, src->participants,
                       (size_t)src->participant_count * sizeof(int));
            }
        }
        graph->constraints[i] = dst;
    }

    /* 2.5 重建交叉引用（PORT.connected_to, REGION.boundary_segments,
     *     FUNCTION_BLOCK.internal_nodes） */
    {
        /* 构建 id_map: 节点 ID -> 新图中节点指针 */
        /* 使用简单的线性搜索（节点数通常不大）；如果需要可改为哈希表 */
        for (int r = 0; r < snapshot->port_ref_count; r++) {
            PortRef *ref = &snapshot->port_refs[r];
            if (ref->connected_to_id < 0) continue;
            if (ref->port_node_index >= graph->node_count) continue;
            GeomNode *port_node = graph->nodes[ref->port_node_index];
            if (!port_node || port_node->type != GEOM_PORT || !port_node->data.port) continue;
            /* 在新图中查找 connected_to_id 对应的节点 */
            for (int i = 0; i < graph->node_count; i++) {
                if (graph->nodes[i]->id == ref->connected_to_id) {
                    port_node->data.port->connected_to = graph->nodes[i];
                    break;
                }
            }
        }

        for (int r = 0; r < snapshot->region_ref_count; r++) {
            RegionRef *ref = &snapshot->region_refs[r];
            if (ref->region_node_index >= graph->node_count) continue;
            GeomNode *region_node = graph->nodes[ref->region_node_index];
            if (!region_node || region_node->type != GEOM_REGION) continue;
            if (region_node->data.region.boundary_segments && ref->segment_ids) {
                for (int k = 0; k < ref->segment_count && k < region_node->data.region.segment_count; k++) {
                    if (ref->segment_ids[k] < 0) continue;
                    for (int i = 0; i < graph->node_count; i++) {
                        if (graph->nodes[i]->id == ref->segment_ids[k]) {
                            region_node->data.region.boundary_segments[k] = graph->nodes[i];
                            break;
                        }
                    }
                }
            }
        }

        for (int r = 0; r < snapshot->fb_ref_count; r++) {
            FBRef *ref = &snapshot->fb_refs[r];
            if (ref->fb_node_index >= graph->node_count) continue;
            GeomNode *fb_node = graph->nodes[ref->fb_node_index];
            if (!fb_node || fb_node->type != GEOM_FUNCTION_BLOCK) continue;
            if (fb_node->data.func_block.internal_nodes && ref->internal_node_ids) {
                for (int k = 0; k < ref->internal_node_count && k < fb_node->data.func_block.internal_node_count; k++) {
                    if (ref->internal_node_ids[k] < 0) continue;
                    for (int i = 0; i < graph->node_count; i++) {
                        if (graph->nodes[i]->id == ref->internal_node_ids[k]) {
                            fb_node->data.func_block.internal_nodes[k] = graph->nodes[i];
                            break;
                        }
                    }
                }
            }
        }
    }

    /* 3. 重建哈希索引 */
    /* 重建节点哈希索引 */
    graph->node_index = NULL;
    graph->node_index_capacity = 0;
    if (graph->node_count > 0) {
        /* 计算合适的哈希表大小（至少是节点数的 2 倍，且为 2 的幂） */
        int cap = 4;
        while (cap < graph->node_count * 2) cap *= 2;
        graph->node_index = lv00_malloc((size_t)cap * sizeof(GeomNode *));
        if (graph->node_index) {
            memset(graph->node_index, 0, (size_t)cap * sizeof(GeomNode *));
            graph->node_index_capacity = cap;
            for (int i = 0; i < graph->node_count; i++) {
                GeomNode *node = graph->nodes[i];
                unsigned idx = (unsigned)node->id * 2654435769u & (unsigned)(cap - 1);
                while (graph->node_index[idx] != NULL) {
                    idx = (idx + 1) & (unsigned)(cap - 1);
                }
                graph->node_index[idx] = node;
            }
        } else {
            /* calloc 失败：节点索引不可用，但图数据已恢复，仍视为成功。
             * 后续按 ID 查找节点将退化为线性搜索。 */
            LOG_WARN("rewrite", "graph_snapshot_restore: 节点哈希索引分配失败 (cap=%d)", cap);
        }
    }

    /* 重建约束哈希索引 */
    graph->constraint_index = NULL;
    graph->constraint_index_capacity = 0;
    if (graph->constraint_count > 0) {
        int cap = 4;
        while (cap < graph->constraint_count * 2) cap *= 2;
        graph->constraint_index = lv00_malloc((size_t)cap * sizeof(Constraint *));
        if (graph->constraint_index) {
            memset(graph->constraint_index, 0, (size_t)cap * sizeof(Constraint *));
            graph->constraint_index_capacity = cap;
            for (int i = 0; i < graph->constraint_count; i++) {
                Constraint *con = graph->constraints[i];
                unsigned idx = (unsigned)con->id * 2654435769u & (unsigned)(cap - 1);
                while (graph->constraint_index[idx] != NULL) {
                    idx = (idx + 1) & (unsigned)(cap - 1);
                }
                graph->constraint_index[idx] = con;
            }
        } else {
            /* calloc 失败：约束索引不可用，但图数据已恢复，仍视为成功。
             * 后续按 ID 查找约束将退化为线性搜索。 */
            LOG_WARN("rewrite", "graph_snapshot_restore: 约束哈希索引分配失败 (cap=%d)", cap);
        }
    }

    return true;
}

void graph_snapshot_destroy(GraphSnapshot *snapshot) {
    if (!snapshot) return;
    for (int i = 0; i < snapshot->node_count; i++) {
        snapshot_node_destroy(snapshot->nodes[i]);
    }
    lv00_free((void**)&snapshot->nodes);
    for (int i = 0; i < snapshot->constraint_count; i++) {
        lv00_free((void**)&snapshot->constraints[i]->participants);
        lv00_free((void**)&snapshot->constraints[i]);
    }
    lv00_free((void**)&snapshot->constraints);
    /* 释放交叉引用信息 */
    for (int i = 0; i < snapshot->region_ref_count; i++) {
        lv00_free((void**)&snapshot->region_refs[i].segment_ids);
    }
    lv00_free((void**)&snapshot->region_refs);
    for (int i = 0; i < snapshot->fb_ref_count; i++) {
        lv00_free((void**)&snapshot->fb_refs[i].internal_node_ids);
    }
    lv00_free((void**)&snapshot->fb_refs);
    lv00_free((void**)&snapshot->port_refs);
    lv00_free((void**)&snapshot);
}

/* ---------------------------------------------------------------------------
 * compute_graph_hash  (改进的结构哈希)
 * ------------------------------------------------------------------------- */

/**
 * @brief 计算约束图的结构哈希值（改进版）
 *
 * 基于 FNV-1a 算法计算约束图的完整结构哈希：
 * - 对每个节点，哈希其 ID 和类型
 * - 对 POINT 节点额外哈希其符号坐标的序列化值
 * - 对每个约束，哈希其类型和参与节点 ID 列表
 *
 * @param graph 约束图指针
 * @return 32位结构哈希值
 */
uint32_t compute_graph_hash(ConstraintGraph *graph) {
    uint64_t h = LV00_FNV64_OFFSET_BASIS;

    /* 哈希节点类型和 POINT 节点的符号坐标 */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *n = graph->nodes[i];
        h = fnv1a_mix(h, &n->id, sizeof(n->id));
        int type_val = (int)n->type;
        h = fnv1a_mix(h, &type_val, sizeof(type_val));

        if (n->type == GEOM_POINT && n->coord_count > 0 && n->symbolic_coords) {
            for (int c = 0; c < n->coord_count; c++) {
                if (n->symbolic_coords[c]) {
                    char *ser = symbolic_coord_serialize(n->symbolic_coords[c]);
                    if (ser) {
                        h = fnv1a_mix(h, ser, strlen(ser));
                        lv00_free((void**)&ser);
                    }
                }
            }
        }
    }

    /* 哈希约束类型及其参与者列表 */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        int type_val = (int)c->type;
        h = fnv1a_mix(h, &type_val, sizeof(type_val));
        h = fnv1a_mix(h, c->participants, c->participant_count * sizeof(int));
    }

    return (int)h;
}

/* ---------------------------------------------------------------------------
 * 公共 API
 * ------------------------------------------------------------------------- */

RewriteRule *rewrite_rule_create(const char *name, RewritePattern *pattern,
                                 RewriteReplacement *replacement, int measure)
{
    RewriteRule *rule = lv00_malloc(sizeof(RewriteRule));
    if (!rule) return NULL;
    /* 【内存管理策略】strdup 为 rule->name 分配独立副本。
     * 若分配失败，需回滚已分配的 rule 结构体。
     * 注意：pattern 和 replacement 的所有权不属于 rule，
     * 由调用者管理，无需在此处释放。 */
    rule->name = lv00_strdup_safe(name);
    if (!rule->name) {
        lv00_free((void**)&rule);
        return NULL;
    }
    rule->pattern = pattern;
    rule->replacement = replacement;
    rule->reduction_measure = measure;
    rule->condition_func = NULL;
    rule->condition_data = NULL;
    return rule;
}

/* 销毁重写规则，释放其持有的资源。
 *
 * 【所有权说明】
 * 此函数仅释放 rule 本身及其 name 字符串。
 * rule->pattern 和 rule->replacement 的所有权不属于 rule 对象，
 * 它们由规则文件解析器（parse_lvz_file）统一管理，在解析器销毁时
 * 通过 parsed_rule_destroy 释放。因此此处不释放 pattern 和 replacement，
 * 避免双重释放。
 * 如果需要在其他场景下独立销毁 rule 及其子对象，应先调用相应的
 * pattern/replacement 销毁函数，再调用此函数。
 */
void rewrite_rule_destroy(RewriteRule *rule) {
    if (rule) {
        lv00_free((void**)&rule->name);
        /* 注意：不释放 rule->pattern 和 rule->replacement，所有权不属于此对象 */
        lv00_free((void**)&rule);
    }
}

/* ---- 模式匹配辅助函数 ---- */

static bool pattern_var_matches_node(int pattern_var_id, GeomNode *graph_node,
                                     const int *bindings, int binding_count)
{
    if (pattern_var_id >= 0) {
        return pattern_var_id == graph_node->id;
    }
    for (int i = 0; i < binding_count; i++) {
        if (bindings[i * 2] == pattern_var_id) {
            return bindings[i * 2 + 1] == graph_node->id;
        }
    }
    return false;
}

/**
 * @brief 检查模式约束是否与图约束匹配
 *
 * 检查类型、参与者数量和参与者 ID 是否一致。
 *
 * @param pattern         模式约束
 * @param graph_con       图约束
 * @param bindings        绑定数组
 * @param binding_count   绑定数量
 * @return true 表示匹配成功
 */
static bool pattern_constraint_matches(Constraint *pattern, Constraint *graph_con,
                                       const int *bindings, int binding_count)
{
    if (pattern->type != graph_con->type) return false;
    if (pattern->participant_count != graph_con->participant_count) return false;
    for (int i = 0; i < pattern->participant_count; i++) {
        int pid = pattern->participants[i];
        int gid = graph_con->participants[i];
        if (pid < 0) {
            bool found = false;
            for (int j = 0; j < binding_count; j++) {
                if (bindings[j * 2] == pid) {
                    found = (bindings[j * 2 + 1] == gid);
                    break;
                }
            }
            if (!found) return false;
        } else {
            if (pid != gid) return false;
        }
    }
    return true;
}

/**
 * @brief 查找与重写规则模式匹配的单个匹配
 *
 * 在约束图中查找与给定重写规则模式匹配的模式变量绑定。
 * 支持 local_equivalence_tolerant 模式，该模式下 POINT 节点
 * 可以通过符号坐标而非节点 ID 进行匹配。
 *
 * @param graph                      约束图指针
 * @param rule                       重写规则指针
 * @param local_equivalence_tolerant 是否启用局部等价容忍模式
 * @return 新分配的匹配结果，失败返回 NULL
 */
RewriteMatch *find_rewrite_match(ConstraintGraph *graph, RewriteRule *rule,
                                 bool local_equivalence_tolerant)
{
    RewritePattern *pat = rule->pattern;
    RewriteMatch *match = lv00_malloc(sizeof(RewriteMatch));
    if (!match) return NULL;
    match->node_bindings = lv00_malloc(pat->var_count * 2 * sizeof(int));
    if (!match->node_bindings) { lv00_free((void**)&match); return NULL; }
    match->constraint_bindings = lv00_malloc(pat->pattern_constraint_count * sizeof(int));
    if (!match->constraint_bindings) { lv00_free((void**)&match->node_bindings); lv00_free((void**)&match); return NULL; }
    match->binding_count = 0;
    int binding_count = 0;

    /* --- Phase 1: bind pattern variables to graph nodes --- */
    for (int j = 0; j < pat->var_count; j++) {
        int pattern_var_id = pat->variable_node_ids[j];
        bool bound = false;

        for (int i = 0; i < graph->node_count && !bound; i++) {
            GeomNode *gn = graph->nodes[i];

            /* 跳过已被其他模式变量绑定的图节点（每个图节点
               在单个匹配中只能满足一个模式变量） */
            bool already_used = false;
            for (int k = 0; k < binding_count; k++) {
                if (match->node_bindings[k * 2 + 1] == gn->id) {
                    already_used = true;
                    break;
                }
            }
            if (already_used) continue;

            /* 标准ID匹配 */
            if (pattern_var_id >= 0) {
                if (pattern_var_id == gn->id) {
                    match->node_bindings[binding_count * 2] = pattern_var_id;
                    match->node_bindings[binding_count * 2 + 1] = gn->id;
                    binding_count++;
                    bound = true;
                }
                continue;
            }

            /* 模式变量（负值）：先检查是否已绑定 */
            bool was_bound = false;
            for (int k = 0; k < binding_count; k++) {
                if (match->node_bindings[k * 2] == pattern_var_id) {
                    was_bound = true;
                    break;
                }
            }
            if (was_bound) continue;

            /* 在 local_equivalence_tolerant 模式下，对于 POINT 节点，
               即使ID不同，也接受具有相同符号坐标的节点。
               这允许跨结构等价但独立构造的子图进行匹配。 */
            if (local_equivalence_tolerant && gn->type == GEOM_POINT) {
                /* 尝试查找已绑定节点中是否有相同坐标的节点 */
                bool coord_match = false;
                int existing_bind = -1;
                for (int k = 0; k < binding_count; k++) {
                    if (match->node_bindings[k * 2] == pattern_var_id) {
                        existing_bind = match->node_bindings[k * 2 + 1];
                        break;
                    }
                }
                if (existing_bind >= 0) {
                    GeomNode *existing = graph_get_node(graph, existing_bind);
                    if (existing && existing->type == GEOM_POINT &&
                        existing->coord_count == gn->coord_count) {
                        coord_match = true;
                        for (int c = 0; c < gn->coord_count; c++) {
                            if (symbolic_coord_compare(existing->symbolic_coords[c],
                                                      gn->symbolic_coords[c]) != 0) {
                                coord_match = false;
                                break;
                            }
                        }
                    }
                    if (coord_match) {
                        bound = true;
                    }
                } else {
                    /* 首次出现此变量：绑定它 */
                    match->node_bindings[binding_count * 2] = pattern_var_id;
                    match->node_bindings[binding_count * 2 + 1] = gn->id;
                    binding_count++;
                    bound = true;
                }
            } else {
                /* 标准模式：绑定第一个未绑定节点 */
                match->node_bindings[binding_count * 2] = pattern_var_id;
                match->node_bindings[binding_count * 2 + 1] = gn->id;
                binding_count++;
                bound = true;
            }
        }

        if (!bound) {
            /* 无法绑定此模式变量 */
            lv00_free((void**)&match->node_bindings);
            lv00_free((void**)&match->constraint_bindings);
            lv00_free((void**)&match);
            return NULL;
        }
    }

    /* --- Phase 2: match pattern constraints against graph constraints --- */
    int constraint_match_count = 0;
    bool *pattern_con_matched = lv00_malloc((size_t)pat->pattern_constraint_count * sizeof(bool));
    if (pattern_con_matched) memset(pattern_con_matched, 0,
                                     (size_t)pat->pattern_constraint_count * sizeof(bool));

    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *gc = graph->constraints[i];
        for (int j = 0; j < pat->pattern_constraint_count; j++) {
            if (pattern_con_matched[j]) continue;
            if (pattern_constraint_matches(pat->pattern_constraints[j], gc,
                                           match->node_bindings, binding_count)) {
                match->constraint_bindings[j] = gc->id;
                pattern_con_matched[j] = true;
                constraint_match_count++;
                break;
            }
        }
    }

    lv00_free((void**)&pattern_con_matched);

    if (constraint_match_count != pat->pattern_constraint_count) {
        lv00_free((void**)&match->node_bindings);
        lv00_free((void**)&match->constraint_bindings);
        lv00_free((void**)&match);
        return NULL;
    }

    /* binding_count 记录的是实际绑定的节点数量（Phase 1 中递增的计数器），
     * 而非 constraint_match_count（约束匹配数量）。两者含义不同：
     *   - binding_count: 模式变量到图节点的绑定对数
     *   - constraint_match_count: 模式约束到图约束的匹配数
     * 此处应使用 binding_count，因为后续代码依赖 binding_count 来遍历
     * node_bindings 数组中的绑定对。 */
    match->binding_count = binding_count;
    return match;
}

/* ===========================================================================
 * WL 图核哈希（公开接口）
 *
 * 封装内部的 compute_wl_graph_hash 函数，提供公开的 API。
 * 允许外部模块（如 solver、unify）获取图的拓扑哈希用于去重或比较。
 * ===========================================================================
 */

/**
 * @brief 计算图的 Weisfeiler-Lehman 图核哈希值
 *
 * 封装内部的 compute_wl_graph_hash 函数，提供公开的 API。
 * 允许外部模块（如 solver、unify）获取图的拓扑哈希用于去重或比较。
 *
 * @param graph 约束图指针
 * @return 64位 WL 哈希值
 */
uint64_t rewrite_compute_wl_hash(const ConstraintGraph *graph) {
    return compute_wl_graph_hash((ConstraintGraph *)graph);
}

/* ===========================================================================
 * 多非重叠匹配查找
 *
 * 在约束图中查找所有与给定重写规则模式匹配的非重叠子图同构。
 * 每次找到一个匹配后，将其匹配的节点标记为已使用，继续搜索直到
 * 无法找到新的匹配。最终返回按匹配质量（匹配节点数降序）排序的
 * 匹配数组。
 *
 * 设计规范参考：design_v2.9.md Section 6.4
 * ===========================================================================
 */

/**
 * @brief 匹配质量比较函数：按匹配节点数降序排序
 *
 * 用作 qsort 的比较函数。以 binding_count（匹配的约束数量）作为
 * 匹配质量的代理指标，数量多的排前面。
 *
 * @param a 指向第一个 RewriteMatch 指针的指针
 * @param b 指向第二个 RewriteMatch 指针的指针
 * @return >0 表示 a 优于 b，<0 表示 b 优于 a
 */
static int match_quality_cmp(const void *a, const void *b) {
    const RewriteMatch *ma = *(const RewriteMatch **)a;
    const RewriteMatch *mb = *(const RewriteMatch **)b;
    /* binding_count 是匹配的约束数量，作为匹配质量的代理指标 */
    if (ma->binding_count != mb->binding_count) {
        return (mb->binding_count > ma->binding_count) ? 1 : -1;
    }
    return true;
}

    /* 检查匹配是否与已使用的节点集合重叠。
     * 返回 true 如果存在重叠（即匹配中有节点在 used_ids 中）。 */
/**
 * @brief 检查匹配是否与已使用的节点集合重叠
 *
 * @param match                   匹配指针
 * @param used_ids               已使用节点 ID 数组
 * @param used_count             已使用节点数量
 * @param node_binding_pair_count 节点绑定对数量
 * @return true 如果存在重叠
 */
static bool match_overlaps_used(const RewriteMatch *match,
                                const int *used_ids, int used_count,
                                int node_binding_pair_count)
{
    for (int i = 0; i < node_binding_pair_count; i++) {
        int graph_node_id = match->node_bindings[i * 2 + 1];
        if (graph_node_id < 0) continue;
        for (int u = 0; u < used_count; u++) {
            if (graph_node_id == used_ids[u]) {
                return true;
            }
        }
    }
    return false;
}

/**
 * @brief 将匹配中的所有图节点 ID 添加到已使用集合中
 *
 * @param match                   匹配指针
 * @param used_ids               已使用节点 ID 数组指针
 * @param used_count             已使用节点数量指针
 * @param used_capacity          容量指针
 * @param node_binding_pair_count 节点绑定对数量
 */
static void add_match_to_used(const RewriteMatch *match,
                              int **used_ids, int *used_count, int *used_capacity,
                              int node_binding_pair_count)
{
    for (int i = 0; i < node_binding_pair_count; i++) {
        int graph_node_id = match->node_bindings[i * 2 + 1];
        if (graph_node_id < 0) continue;

        /* 检查是否已在集合中 */
        bool already = false;
        for (int u = 0; u < *used_count; u++) {
            if ((*used_ids)[u] == graph_node_id) {
                already = true;
                break;
            }
        }
        if (already) continue;

        /* 扩容 */
        if (*used_count >= *used_capacity) {
            int new_cap = *used_capacity > 0 ? *used_capacity * 2 : 16;
            int *new_arr = lv00_realloc(*used_ids, (size_t)new_cap * sizeof(int));
            if (!new_arr) {
                debug_log_rewrite("内存分配失败：无法扩展 used_ids 数组");
                return;
            }
            *used_ids = new_arr;
            *used_capacity = new_cap;
        }
        (*used_ids)[(*used_count)++] = graph_node_id;
    }
}

int find_all_non_overlapping_matches(
    ConstraintGraph *graph,
    RewriteRule *rule,
    const int *used_node_ids, int used_count,
    RewriteMatch ***out_matches, int *out_match_count)
{
    if (!graph || !rule || !rule->pattern || !out_matches || !out_match_count)
        return -1;

    *out_matches = NULL;
    *out_match_count = 0;

    RewritePattern *pat = rule->pattern;
    if (pat->var_count == 0)
        return 0;

    /* 初始化本地已使用节点集合（合并外部传入的已使用节点） */
    int local_used_capacity = used_count > 0 ? used_count + 16 : 16;
    int *local_used = lv00_malloc((size_t)local_used_capacity * sizeof(int));
    if (!local_used) return -1;
    int local_used_count = 0;

    /* 复制外部传入的已使用节点 */
    for (int i = 0; i < used_count; i++) {
        if (local_used_count >= local_used_capacity) {
            int new_cap = local_used_capacity * 2;
            int *new_arr = lv00_realloc(local_used, (size_t)new_cap * sizeof(int));
            if (!new_arr) { lv00_free((void**)&local_used); return -1; }
            local_used = new_arr;
            local_used_capacity = new_cap;
        }
        local_used[local_used_count++] = used_node_ids[i];
    }

    /* 创建图快照，以便在搜索过程中临时移除已匹配节点 */
    GraphSnapshot *snapshot = graph_snapshot_create(graph);
    if (!snapshot) {
        lv00_free((void**)&local_used);
        return -1;
    }

    /* 匹配结果数组 */
    int match_capacity = 8;
    RewriteMatch **matches = lv00_malloc((size_t)match_capacity * sizeof(RewriteMatch *));
    if (!matches) {
        graph_snapshot_destroy(snapshot);
        lv00_free((void**)&local_used);
        return -1;
    }
    int match_count = 0;

    /* 计算节点绑定对的数量：pattern 的 var_count */
    int node_binding_pairs = pat->var_count;

    /* 反复搜索，直到找不到新的非重叠匹配 */
    int max_iterations = graph->node_count + 1; /* 防止无限循环 */
    int iteration = 0;

    while (iteration < max_iterations) {
        iteration++;

        /* 使用 VF2 在当前图状态中查找一个匹配 */
        RewriteMatch *match = vf2_find_match(graph, pat, false);
        if (!match) break;

        /* 检查前置条件 */
        if (!evaluate_precondition(graph, rule, match)) {
            lv00_free((void**)&match->node_bindings);
            lv00_free((void**)&match->constraint_bindings);
            lv00_free((void**)&match);
            break; /* 前置条件失败，停止搜索 */
        }

        /* 检查匹配是否与已使用节点重叠 */
        if (match_overlaps_used(match, local_used, local_used_count,
                                node_binding_pairs)) {
            /* 匹配与已使用节点重叠 -- 需要移除已使用的节点后重新搜索。
             * 从图中移除已使用的节点，然后继续循环。 */
            for (int u = 0; u < local_used_count; u++) {
                graph_remove_node(graph, local_used[u]);
            }
            /* 清空本地已使用集合（已从图中移除） */
            local_used_count = 0;

            lv00_free((void**)&match->node_bindings);
            lv00_free((void**)&match->constraint_bindings);
            lv00_free((void**)&match);
            continue;
        }

        /* 找到一个有效的非重叠匹配 -- 保存它 */
        if (match_count >= match_capacity) {
            int new_cap = match_capacity * 2;
            RewriteMatch **new_arr = lv00_realloc(matches,
                (size_t)new_cap * sizeof(RewriteMatch *));
            if (!new_arr) {
                lv00_free((void**)&match->node_bindings);
                lv00_free((void**)&match->constraint_bindings);
                lv00_free((void**)&match);
                break;
            }
            matches = new_arr;
            match_capacity = new_cap;
        }
        matches[match_count++] = match;

        /* 将此匹配的节点添加到已使用集合 */
        add_match_to_used(match, &local_used, &local_used_count,
                          &local_used_capacity, node_binding_pairs);

        /* 从图中移除已匹配的节点，以便下次搜索不会找到重叠匹配 */
        for (int i = 0; i < node_binding_pairs; i++) {
            int graph_node_id = match->node_bindings[i * 2 + 1];
            if (graph_node_id >= 0) {
                graph_remove_node(graph, graph_node_id);
            }
        }
    }

    /* 从快照恢复原始图 */
    if (!graph_snapshot_restore(snapshot, graph)) {
        /* 恢复失败：图已被重置为空图状态，这是一个严重错误。
         * 释放所有已找到的匹配结果并返回错误。 */
        LOG_ERROR("rewrite", "find_all_non_overlapping_matches: 图快照恢复失败，图已被重置为空图");
        graph_snapshot_destroy(snapshot);
        for (int i = 0; i < match_count; i++) {
            lv00_free((void**)&matches[i]->node_bindings);
            lv00_free((void**)&matches[i]->constraint_bindings);
            lv00_free((void**)&matches[i]);
        }
        lv00_free((void**)&matches);
        lv00_free((void**)&local_used);
        *out_matches = NULL;
        *out_match_count = 0;
        return -1;
    }
    graph_snapshot_destroy(snapshot);

    /* 按匹配质量排序（匹配约束数降序） */
    if (match_count > 1) {
        qsort(matches, (size_t)match_count, sizeof(RewriteMatch *),
              match_quality_cmp);
    }

    lv00_free((void**)&local_used);

    *out_matches = matches;
    *out_match_count = match_count;
    return 0;
}

/* ===========================================================================
 * 批量应用非重叠匹配
 *
 * 对一组非重叠匹配依次应用重写规则。对每个匹配创建图快照，
 * 尝试应用替换。如果替换产生冲突或失败，回滚到快照状态并跳过。
 * 返回成功应用的替换数量。
 *
 * 设计规范参考：design_v2.9.md Section 6.4
 * ===========================================================================
 */

int rewrite_apply_all_matches(
    ConstraintGraph *graph,
    RewriteRule *rule,
    RewriteMatch *matches, int match_count,
    int *out_applied_count)
{
    if (!graph || !rule || !matches || match_count <= 0 || !out_applied_count)
        return -1;

    *out_applied_count = 0;

    /* 记录已被前序替换修改过的节点 ID，用于冲突检测 */
    int modified_capacity = 64;
    int *modified_node_ids = lv00_malloc((size_t)modified_capacity * sizeof(int));
    if (!modified_node_ids) return -1;
    int modified_count = 0;

    int applied = 0;

    for (int m = 0; m < match_count; m++) {
        RewriteMatch *match = &matches[m];

        /* 检查此匹配的节点是否已被前序替换修改过 */
        bool conflict = false;
        int node_binding_pairs = rule->pattern ? rule->pattern->var_count : 0;
        for (int i = 0; i < node_binding_pairs; i++) {
            int graph_node_id = match->node_bindings[i * 2 + 1];
            if (graph_node_id < 0) continue;

            /* 检查节点是否仍然存在于图中 */
            if (!graph_get_node(graph, graph_node_id)) {
                conflict = true;
                break;
            }

            /* 检查节点是否已被修改 */
            for (int k = 0; k < modified_count; k++) {
                if (modified_node_ids[k] == graph_node_id) {
                    conflict = true;
                    break;
                }
            }
            if (conflict) break;
        }

        if (conflict) {
            /* 跳过此匹配 -- 与前序替换冲突 */
            continue;
        }

        /* 创建图快照（用于回滚） */
        GraphSnapshot *snap = graph_snapshot_create(graph);
        if (!snap) {
            /* 快照创建失败，停止后续处理 */
            break;
        }

        /* 尝试应用替换 */
        RewriteStatus status = apply_rewrite(graph, rule, match);

        if (status == REWRITE_APPLIED) {
            /* 应用成功 -- 记录被修改的节点 */
            for (int i = 0; i < node_binding_pairs; i++) {
                int graph_node_id = match->node_bindings[i * 2 + 1];
                if (graph_node_id < 0) continue;

                if (modified_count >= modified_capacity) {
                    int new_cap = modified_capacity * 2;
                    int *new_arr = lv00_realloc(modified_node_ids,
                        (size_t)new_cap * sizeof(int));
                    if (!new_arr) {
                        debug_log_rewrite("内存分配失败：无法扩展 modified_node_ids 数组");
                        break;
                    }
                    modified_node_ids = new_arr;
                    modified_capacity = new_cap;
                }
                modified_node_ids[modified_count++] = graph_node_id;
            }
            applied++;
        } else {
            /* 应用失败 -- apply_rewrite 内部已通过快照回滚，
             * 无需额外恢复操作 */
            (void)snap;
        }

        /* 注意：apply_rewrite 内部会创建并销毁自己的快照。
         * 此处的快照用于检测 apply_rewrite 是否真正修改了图。
         * 由于 apply_rewrite 在失败时已回滚，我们只需销毁此快照。 */
        graph_snapshot_destroy(snap);
    }

    lv00_free((void**)&modified_node_ids);
    *out_applied_count = applied;
    return 0;
}

/* ===========================================================================
 * 规则热加载/卸载
 *
 * 支持从 .lvz 格式文件动态加载重写规则，以及按名称卸载指定规则。
 * =========================================================================== */

/* .lvz 文件解析辅助结构 */
typedef struct {
    char name[256];
    int priority;
    /* 模式变量节点 ID 列表 */
    int *pattern_var_ids;
    int pattern_var_count;
    /* 模式约束：每条约束由 type + participant_count + participants 组成 */
    struct {
        ConstraintType type;
        int participant_count;
        int participants[8]; /* 最多 8 个参与者（BETWEENNESS/INTERSECTION 为 3） */
    } *pattern_constraints;
    int pattern_constraint_count;
    /* 替换约束 */
    struct {
        ConstraintType type;
        int participant_count;
        int participants[8];
    } *replacement_constraints;
    int replacement_constraint_count;
    /* 替换节点绑定 */
    struct {
        int pattern_var_id;
        int target_id;
    } *node_bindings;
    int node_binding_count;
    /* 新节点 */
    int *new_nodes;
    int new_node_count;
} LvzRewriteRule;
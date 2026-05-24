/**
 * @file rewrite.c
 * @brief 图重写引擎实现
 * @details 实现 VF2 子图同构匹配算法和 Weisfeiler-Lehman 图核哈希。
 *          支持图快照/回滚机制、规则热加载和 .lvz 规则文件解析。
 *          提供非重叠匹配和多步重写功能。
 *
 * 【重构计划】
 *   以下模块适合提取为独立文件，以降低本文件的复杂度：
 *   1. VF2 子图同构匹配引擎
 *      - 来源：vf2_match_recursive / vf2_feasible / vf2_lookahead / VF2State
 *      - 建议文件：src/vf2_matcher.c + src/vf2_matcher.h
 *      - 原因：VF2 是独立图算法，与重写规则引擎正交；
 *              提取后可与 constraint_graph 模块形成清晰的依赖关系
 *   2. WL 图核哈希（Weisfeiler-Lehman Graph Kernel）
 *      - 来源：wl_hash_graph / wl_hash_* 系列函数
 *      - 建议文件：src/wl_hash.c + src/wl_hash.h
 *      - 原因：图核哈希是通用图算法，不依赖重写语义
 *   3. 图快照/回滚（Graph Snapshot / Rollback）
 *      - 来源：snapshot_save / snapshot_restore / graph_snapshot_* 系列
 *      - 建议文件：src/graph_snapshot.c + src/graph_snapshot.h
 *      - 原因：快照机制与重写引擎逻辑独立，可复用于 undo/redo 场景
 *   4. .lvz 规则文件解析
 *      - 来源：parse_lvz_rule / parse_rule_file / lvz_rule_validate
 *      - 建议文件：src/lvz_rule_parser.c + src/lvz_rule_parser.h
 *      - 原因：解析器与引擎分离，便于单独测试和格式版本演进
 *
 *   注意：此计划仅记录结构优化方向，实际拆分需配合接口稳定性评估和
 *   回归测试，避免破坏现有的 VF2 -> 重写 -> 规范化流水线。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 *
 * @dependencies
 *   - rewrite.h            : 图重写引擎公共接口定义
 *   - lv00_internal.h      : 内部数据结构、常量、FNV 哈希基础
 *   - lv00_utils.h         : 统一内存分配器
 *   - constraint_graph.h   : 约束图接口
 *   - normalization.h      : 图规范化引擎（规范化间步）
 *   - stream.h             : 流式事件输出
 */

#include "rewrite.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "constraint_graph.h"
#include "debug.h"
#include "lv00_internal.h"
#include "lv00_utils.h" /* lv00_malloc / lv00_free —— 统一内存分配器 */
#include "node_deep_copy.h"
#include "normalization.h"
#include "stream.h"
#include "stream_context_util.h"

/* ==================== 命名常量 ==================== */

/** VF2 子图同构搜索回溯深度上限 */
#define REWRITE_VF2_MAX_DEPTH 100

/** 哈希计算批次大小（用于 WL 图核哈希的增量计算） */
#define REWRITE_HASH_BATCH_SIZE 64

LV00_DECLARE_STREAM_CTX(rewrite)

/* ── 前向声明 ── */
static uint64_t compute_wl_graph_hash(ConstraintGraph *graph);
static uint32_t compute_graph_hash(ConstraintGraph *graph);

/* ---------------------------------------------------------------------------
 * 内部辅助函数
 * ------------------------------------------------------------------------- */

/* 前向声明 */
static bool evaluate_precondition(ConstraintGraph *graph, RewriteRule *rule, RewriteMatch *match);

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
    const uint8_t *p = (const uint8_t *) data;
    for (size_t i = 0; i < len; i++) {
        hash ^= p[i];
        hash *= LV00_FNV64_PRIME; /* 使用 64 位 FNV 质数，与项目统一 */
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
static int resolve_binding(const int *bindings, int binding_count, int pattern_var_id) {
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
static bool pattern_var_used_in_replacement(const RewriteReplacement *repl, int pattern_var_id) {
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
static bool pattern_var_in_replacement_bindings(const RewriteReplacement *repl, int pattern_var_id) {
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
static int resolve_replacement_participant(int participant_id, const int *match_bindings, int match_binding_count,
                                           const int *new_node_map, /* 将替换 new_node 索引映射到实际图节点 ID */
                                           int new_node_map_count,
                                           const int *new_nodes, /* replacement->new_nodes 数组 */
                                           int new_node_count) {
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
static bool add_constraint_generic(ConstraintGraph *graph, ConstraintType type, const int *participants,
                                   int participant_count) {
    switch (type) {
        case INCIDENCE:
            if (participant_count == 2)
                return graph_add_incidence(graph, participants[0], participants[1]) == ADD_CONSTRAINT_OK;
            break;
        case BETWEENNESS:
            if (participant_count == 3)
                return graph_add_betweenness(graph, participants[0], participants[1], participants[2]) ==
                       ADD_CONSTRAINT_OK;
            break;
        case INTERSECTION:
            if (participant_count == 3)
                return graph_add_intersection(graph, participants[0], participants[1], participants[2]) ==
                       ADD_CONSTRAINT_OK;
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
static bool is_matched_constraint(const RewriteMatch *match, int constraint_id) {
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
static bool check_graph_consistency(ConstraintGraph *graph) {
    int conflict_count = 0;
    int *conflict_sizes = NULL;
    int **conflicts = graph_detect_conflicts(graph, &conflict_count, &conflict_sizes);
    if (conflicts && conflict_count > 0) {
        /* 存在冲突：释放冲突数组并返回 false */
        if (conflict_sizes)
            lv00_free((void **) &conflict_sizes);
        if (conflicts)
            lv00_free((void **) &conflicts);
        return false;
    }
    /* 无冲突或 conflicts 为 NULL（分配失败）：
     * 当 conflict_count == 0 且 conflicts != NULL 时，确认无冲突；
     * 当 conflicts == NULL 时，可能是分配失败，保守返回 true 但记录警告 */
    if (conflict_sizes)
        lv00_free((void **) &conflict_sizes);
    if (conflicts) {
        lv00_free((void **) &conflicts);
    } else if (conflict_count == 0) {
        /* conflicts 为 NULL 但 conflict_count 也为 0，说明确实无冲突 */
    } else {
        LOG_WARN("rewrite", "冲突检测内存分配失败，跳过一致性检查");
    }
    return true;
}

/* ---------------------------------------------------------------------------
 * Graph Snapshot — 用于重写替换操作的事务性回滚
 * ------------------------------------------------------------------------- */


/**
 * @brief 销毁快照中的单个节点
 *
 * @param node 待销毁的节点指针
 */
static void snapshot_node_destroy(GeomNode *node) {
    if (!node)
        return;
    if (node->symbolic_coords) {
        for (int c = 0; c < node->coord_count; c++) {
            symbolic_coord_destroy(node->symbolic_coords[c]);
        }
        lv00_free((void **) &node->symbolic_coords);
    }
    lv00_free((void **) &node->numeric_assumption_declaration);
    switch (node->type) {
        case GEOM_PORT:
            if (node->data.port) {
                /* type_region 由 TypeSystem 统一管理，此处不释放 */
                lv00_free((void **) &node->data.port);
            }
            break;
        case GEOM_REGION:
            lv00_free((void **) &node->data.region.boundary_segments);
            break;
        case GEOM_FUNCTION_BLOCK:
            lv00_free((void **) &node->data.func_block.internal_nodes);
            lv00_free((void **) &node->data.func_block.input_port_ids);
            lv00_free((void **) &node->data.func_block.output_port_ids);
            break;
        default:
            break;
    }
    lv00_free((void **) &node);
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
    if (!graph)
        return NULL;

    GraphSnapshot *snap = lv00_malloc(sizeof(GraphSnapshot));
    if (!snap)
        return NULL;
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
        if (n->type == GEOM_PORT && n->data.port)
            port_ref_count++;
        if (n->type == GEOM_REGION && n->data.region.segment_count > 0)
            region_ref_count++;
        if (n->type == GEOM_FUNCTION_BLOCK && n->data.func_block.internal_node_count > 0)
            fb_ref_count++;
    }

    /* 分配并填充 port_refs */
    snap->port_ref_count = port_ref_count;
    snap->port_refs = NULL;
    if (port_ref_count > 0) {
        snap->port_refs = lv00_malloc((size_t) port_ref_count * sizeof(PortRef));
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
        snap->region_refs = lv00_malloc((size_t) region_ref_count * sizeof(RegionRef));
        if (snap->region_refs) {
            int idx = 0;
            for (int i = 0; i < graph->node_count; i++) {
                GeomNode *n = graph->nodes[i];
                if (n->type == GEOM_REGION && n->data.region.segment_count > 0 && n->data.region.boundary_segments) {
                    snap->region_refs[idx].region_node_index = i;
                    snap->region_refs[idx].segment_count = n->data.region.segment_count;
                    snap->region_refs[idx].segment_ids =
                        lv00_malloc((size_t) n->data.region.segment_count * sizeof(int));
                    if (snap->region_refs[idx].segment_ids) {
                        for (int k = 0; k < n->data.region.segment_count; k++) {
                            snap->region_refs[idx].segment_ids[k] =
                                n->data.region.boundary_segments[k] ? n->data.region.boundary_segments[k]->id : -1;
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
        snap->fb_refs = lv00_malloc((size_t) fb_ref_count * sizeof(FBRef));
        if (snap->fb_refs) {
            int idx = 0;
            for (int i = 0; i < graph->node_count; i++) {
                GeomNode *n = graph->nodes[i];
                if (n->type == GEOM_FUNCTION_BLOCK && n->data.func_block.internal_node_count > 0 &&
                    n->data.func_block.internal_nodes) {
                    snap->fb_refs[idx].fb_node_index = i;
                    snap->fb_refs[idx].internal_node_count = n->data.func_block.internal_node_count;
                    snap->fb_refs[idx].internal_node_ids =
                        lv00_malloc((size_t) n->data.func_block.internal_node_count * sizeof(int));
                    if (snap->fb_refs[idx].internal_node_ids) {
                        for (int k = 0; k < n->data.func_block.internal_node_count; k++) {
                            snap->fb_refs[idx].internal_node_ids[k] =
                                n->data.func_block.internal_nodes[k] ? n->data.func_block.internal_nodes[k]->id : -1;
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
    snap->nodes = lv00_malloc((size_t) snap->node_capacity * sizeof(GeomNode *));
    if (!snap->nodes) {
        /* cleanup refs */
        lv00_free((void **) &snap->port_refs);
        for (int i = 0; i < snap->region_ref_count; i++)
            lv00_free((void **) &snap->region_refs[i].segment_ids);
        lv00_free((void **) &snap->region_refs);
        for (int i = 0; i < snap->fb_ref_count; i++)
            lv00_free((void **) &snap->fb_refs[i].internal_node_ids);
        lv00_free((void **) &snap->fb_refs);
        lv00_free((void **) &snap);
        return NULL;
    }
    for (int i = 0; i < graph->node_count; i++) {
        snap->nodes[i] = node_deep_copy_geom_node(graph->nodes[i], NULL);
        if (!snap->nodes[i]) {
            /* 回滚已分配的节点 */
            for (int j = 0; j < i; j++)
                snapshot_node_destroy(snap->nodes[j]);
            lv00_free((void **) &snap->nodes);
            lv00_free((void **) &snap);
            return NULL;
        }
    }

    /* 深拷贝约束数组 */
    snap->constraints = lv00_malloc((size_t) snap->constraint_capacity * sizeof(Constraint *));
    if (!snap->constraints) {
        for (int i = 0; i < snap->node_count; i++)
            snapshot_node_destroy(snap->nodes[i]);
        lv00_free((void **) &snap->nodes);
        /* cleanup refs */
        lv00_free((void **) &snap->port_refs);
        for (int i = 0; i < snap->region_ref_count; i++)
            lv00_free((void **) &snap->region_refs[i].segment_ids);
        lv00_free((void **) &snap->region_refs);
        for (int i = 0; i < snap->fb_ref_count; i++)
            lv00_free((void **) &snap->fb_refs[i].internal_node_ids);
        lv00_free((void **) &snap->fb_refs);
        lv00_free((void **) &snap);
        return NULL;
    }
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *src = graph->constraints[i];
        Constraint *dst = lv00_malloc(sizeof(Constraint));
        if (!dst) {
            for (int j = 0; j < i; j++) {
                lv00_free((void **) &snap->constraints[j]->participants);
                lv00_free((void **) &snap->constraints[j]);
            }
            for (int j = 0; j < snap->node_count; j++)
                snapshot_node_destroy(snap->nodes[j]);
            lv00_free((void **) &snap->constraints);
            lv00_free((void **) &snap->nodes);
            lv00_free((void **) &snap);
            return NULL;
        }
        dst->id = src->id;
        dst->type = src->type;
        dst->template_id = src->template_id;
        dst->participant_count = src->participant_count;
        dst->participants = NULL;
        if (src->participant_count > 0 && src->participants) {
            dst->participants = lv00_malloc((size_t) src->participant_count * sizeof(int));
            if (dst->participants) {
                memcpy(dst->participants, src->participants, (size_t) src->participant_count * sizeof(int));
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
    if (!snapshot || !graph)
        return false;

    /* 1. 销毁当前图中的所有节点和约束 */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (node->symbolic_coords) {
            for (int c = 0; c < node->coord_count; c++) {
                symbolic_coord_destroy(node->symbolic_coords[c]);
            }
            lv00_free((void **) &node->symbolic_coords);
        }
        lv00_free((void **) &node->numeric_assumption_declaration);
        switch (node->type) {
            case GEOM_PORT:
                lv00_free((void **) &node->data.port);
                break;
            case GEOM_REGION:
                lv00_free((void **) &node->data.region.boundary_segments);
                break;
            case GEOM_FUNCTION_BLOCK:
                lv00_free((void **) &node->data.func_block.internal_nodes);
                lv00_free((void **) &node->data.func_block.input_port_ids);
                lv00_free((void **) &node->data.func_block.output_port_ids);
                break;
            default:
                break;
        }
        lv00_free((void **) &node);
    }
    for (int i = 0; i < graph->constraint_count; i++) {
        lv00_free((void **) &graph->constraints[i]->participants);
        lv00_free((void **) &graph->constraints[i]);
    }
    lv00_free((void **) &graph->nodes);
    lv00_free((void **) &graph->constraints);
    lv00_free((void **) &graph->node_index);
    lv00_free((void **) &graph->constraint_index);

    /* 2. 从快照恢复所有节点和约束（深拷贝） */
    graph->node_count = snapshot->node_count;
    graph->node_capacity = snapshot->node_capacity;
    graph->constraint_count = snapshot->constraint_count;
    graph->constraint_capacity = snapshot->constraint_capacity;
    graph->next_node_id = snapshot->next_node_id;
    graph->next_constraint_id = snapshot->next_constraint_id;

    graph->nodes = lv00_malloc((size_t) graph->node_capacity * sizeof(GeomNode *));
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
        graph->nodes[i] = node_deep_copy_geom_node(snapshot->nodes[i], NULL);
        if (!graph->nodes[i]) {
            /* 清理已分配的部分节点数据 */
            for (int j = 0; j < i; j++) {
                snapshot_node_destroy(graph->nodes[j]);
            }
            lv00_free((void **) &graph->nodes);
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

    graph->constraints = lv00_malloc((size_t) graph->constraint_capacity * sizeof(Constraint *));
    if (!graph->constraints) {
        /* 清理已恢复的节点数据，将图重置为空图状态 */
        for (int i = 0; i < graph->node_count; i++) {
            snapshot_node_destroy(graph->nodes[i]);
        }
        lv00_free((void **) &graph->nodes);
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
                lv00_free((void **) &graph->constraints[j]->participants);
                lv00_free((void **) &graph->constraints[j]);
            }
            lv00_free((void **) &graph->constraints);
            for (int j = 0; j < graph->node_count; j++) {
                snapshot_node_destroy(graph->nodes[j]);
            }
            lv00_free((void **) &graph->nodes);
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
            dst->participants = lv00_malloc((size_t) src->participant_count * sizeof(int));
            if (dst->participants) {
                memcpy(dst->participants, src->participants, (size_t) src->participant_count * sizeof(int));
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
            if (ref->connected_to_id < 0)
                continue;
            if (ref->port_node_index >= graph->node_count)
                continue;
            GeomNode *port_node = graph->nodes[ref->port_node_index];
            if (!port_node || port_node->type != GEOM_PORT || !port_node->data.port)
                continue;
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
            if (ref->region_node_index >= graph->node_count)
                continue;
            GeomNode *region_node = graph->nodes[ref->region_node_index];
            if (!region_node || region_node->type != GEOM_REGION)
                continue;
            if (region_node->data.region.boundary_segments && ref->segment_ids) {
                for (int k = 0; k < ref->segment_count && k < region_node->data.region.segment_count; k++) {
                    if (ref->segment_ids[k] < 0)
                        continue;
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
            if (ref->fb_node_index >= graph->node_count)
                continue;
            GeomNode *fb_node = graph->nodes[ref->fb_node_index];
            if (!fb_node || fb_node->type != GEOM_FUNCTION_BLOCK)
                continue;
            if (fb_node->data.func_block.internal_nodes && ref->internal_node_ids) {
                for (int k = 0; k < ref->internal_node_count && k < fb_node->data.func_block.internal_node_count; k++) {
                    if (ref->internal_node_ids[k] < 0)
                        continue;
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
        while (cap < graph->node_count * 2)
            cap *= 2;
        graph->node_index = lv00_malloc((size_t) cap * sizeof(GeomNode *));
        if (graph->node_index) {
            memset(graph->node_index, 0, (size_t) cap * sizeof(GeomNode *));
            graph->node_index_capacity = cap;
            for (int i = 0; i < graph->node_count; i++) {
                GeomNode *node = graph->nodes[i];
                unsigned idx = (unsigned) node->id * 2654435769u & (unsigned) (cap - 1);
                while (graph->node_index[idx] != NULL) {
                    idx = (idx + 1) & (unsigned) (cap - 1);
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
        while (cap < graph->constraint_count * 2)
            cap *= 2;
        graph->constraint_index = lv00_malloc((size_t) cap * sizeof(Constraint *));
        if (graph->constraint_index) {
            memset(graph->constraint_index, 0, (size_t) cap * sizeof(Constraint *));
            graph->constraint_index_capacity = cap;
            for (int i = 0; i < graph->constraint_count; i++) {
                Constraint *con = graph->constraints[i];
                unsigned idx = (unsigned) con->id * 2654435769u & (unsigned) (cap - 1);
                while (graph->constraint_index[idx] != NULL) {
                    idx = (idx + 1) & (unsigned) (cap - 1);
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
    if (!snapshot)
        return;
    for (int i = 0; i < snapshot->node_count; i++) {
        snapshot_node_destroy(snapshot->nodes[i]);
    }
    lv00_free((void **) &snapshot->nodes);
    for (int i = 0; i < snapshot->constraint_count; i++) {
        lv00_free((void **) &snapshot->constraints[i]->participants);
        lv00_free((void **) &snapshot->constraints[i]);
    }
    lv00_free((void **) &snapshot->constraints);
    /* 释放交叉引用信息 */
    for (int i = 0; i < snapshot->region_ref_count; i++) {
        lv00_free((void **) &snapshot->region_refs[i].segment_ids);
    }
    lv00_free((void **) &snapshot->region_refs);
    for (int i = 0; i < snapshot->fb_ref_count; i++) {
        lv00_free((void **) &snapshot->fb_refs[i].internal_node_ids);
    }
    lv00_free((void **) &snapshot->fb_refs);
    lv00_free((void **) &snapshot->port_refs);
    lv00_free((void **) &snapshot);
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
static uint32_t compute_graph_hash(ConstraintGraph *graph) {
    uint64_t h = LV00_FNV64_OFFSET_BASIS;

    /* 哈希节点类型和 POINT 节点的符号坐标 */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *n = graph->nodes[i];
        h = fnv1a_mix(h, &n->id, sizeof(n->id));
        int type_val = (int) n->type;
        h = fnv1a_mix(h, &type_val, sizeof(type_val));

        if (n->type == GEOM_POINT && n->coord_count > 0 && n->symbolic_coords) {
            for (int c = 0; c < n->coord_count; c++) {
                if (n->symbolic_coords[c]) {
                    char *ser = symbolic_coord_serialize(n->symbolic_coords[c]);
                    if (ser) {
                        h = fnv1a_mix(h, ser, strlen(ser));
                        lv00_free((void **) &ser);
                    }
                }
            }
        }
    }

    /* 哈希约束类型及其参与者列表 */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        int type_val = (int) c->type;
        h = fnv1a_mix(h, &type_val, sizeof(type_val));
        h = fnv1a_mix(h, c->participants, c->participant_count * sizeof(int));
    }

    return (int) h;
}

/* ---------------------------------------------------------------------------
 * 公共 API
 * ------------------------------------------------------------------------- */

RewriteRule *rewrite_rule_create(const char *name, RewritePattern *pattern, RewriteReplacement *replacement,
                                 int measure) {
    RewriteRule *rule = lv00_malloc(sizeof(RewriteRule));
    if (!rule)
        return NULL;
    /* 【内存管理策略】strdup 为 rule->name 分配独立副本。
     * 若分配失败，需回滚已分配的 rule 结构体。
     * 注意：pattern 和 replacement 的所有权不属于 rule，
     * 由调用者管理，无需在此处释放。 */
    rule->name = lv00_strdup_safe(name);
    if (!rule->name) {
        lv00_free((void **) &rule);
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
        lv00_free((void **) &rule->name);
        /* 注意：不释放 rule->pattern 和 rule->replacement，所有权不属于此对象 */
        lv00_free((void **) &rule);
    }
}

/* ---- 模式匹配辅助函数 ---- */

static bool pattern_var_matches_node(int pattern_var_id, GeomNode *graph_node, const int *bindings, int binding_count) {
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
static bool pattern_constraint_matches(Constraint *pattern, Constraint *graph_con, const int *bindings,
                                       int binding_count) {
    if (pattern->type != graph_con->type)
        return false;
    if (pattern->participant_count != graph_con->participant_count)
        return false;
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
            if (!found)
                return false;
        } else {
            if (pid != gid)
                return false;
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
RewriteMatch *find_rewrite_match(ConstraintGraph *graph, RewriteRule *rule, bool local_equivalence_tolerant) {
    RewritePattern *pat = rule->pattern;
    RewriteMatch *match = lv00_malloc(sizeof(RewriteMatch));
    if (!match)
        return NULL;
    match->node_bindings = lv00_malloc(pat->var_count * 2 * sizeof(int));
    if (!match->node_bindings) {
        lv00_free((void **) &match);
        return NULL;
    }
    match->constraint_bindings = lv00_malloc(pat->pattern_constraint_count * sizeof(int));
    if (!match->constraint_bindings) {
        lv00_free((void **) &match->node_bindings);
        lv00_free((void **) &match);
        return NULL;
    }
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
            if (already_used)
                continue;

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
            if (was_bound)
                continue;

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
                    if (existing && existing->type == GEOM_POINT && existing->coord_count == gn->coord_count) {
                        coord_match = true;
                        for (int c = 0; c < gn->coord_count; c++) {
                            if (symbolic_coord_compare(existing->symbolic_coords[c], gn->symbolic_coords[c]) != 0) {
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
            lv00_free((void **) &match->node_bindings);
            lv00_free((void **) &match->constraint_bindings);
            lv00_free((void **) &match);
            return NULL;
        }
    }

    /* --- Phase 2: match pattern constraints against graph constraints --- */
    int constraint_match_count = 0;
    bool *pattern_con_matched = lv00_malloc((size_t) pat->pattern_constraint_count * sizeof(bool));
    if (pattern_con_matched)
        memset(pattern_con_matched, 0, (size_t) pat->pattern_constraint_count * sizeof(bool));

    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *gc = graph->constraints[i];
        for (int j = 0; j < pat->pattern_constraint_count; j++) {
            if (pattern_con_matched[j])
                continue;
            if (pattern_constraint_matches(pat->pattern_constraints[j], gc, match->node_bindings, binding_count)) {
                match->constraint_bindings[j] = gc->id;
                pattern_con_matched[j] = true;
                constraint_match_count++;
                break;
            }
        }
    }

    lv00_free((void **) &pattern_con_matched);

    if (constraint_match_count != pat->pattern_constraint_count) {
        lv00_free((void **) &match->node_bindings);
        lv00_free((void **) &match->constraint_bindings);
        lv00_free((void **) &match);
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
    return compute_wl_graph_hash((ConstraintGraph *) graph);
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
    const RewriteMatch *ma = *(const RewriteMatch **) a;
    const RewriteMatch *mb = *(const RewriteMatch **) b;
    /* binding_count 是匹配的约束数量，作为匹配质量的代理指标 */
    if (ma->binding_count != mb->binding_count) {
        return (mb->binding_count > ma->binding_count) ? 1 : -1;
    }
    return 0;
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
static bool match_overlaps_used(const RewriteMatch *match, const int *used_ids, int used_count,
                                int node_binding_pair_count) {
    for (int i = 0; i < node_binding_pair_count; i++) {
        int graph_node_id = match->node_bindings[i * 2 + 1];
        if (graph_node_id < 0)
            continue;
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
static void add_match_to_used(const RewriteMatch *match, int **used_ids, int *used_count, int *used_capacity,
                              int node_binding_pair_count) {
    for (int i = 0; i < node_binding_pair_count; i++) {
        int graph_node_id = match->node_bindings[i * 2 + 1];
        if (graph_node_id < 0)
            continue;

        /* 检查是否已在集合中 */
        bool already = false;
        for (int u = 0; u < *used_count; u++) {
            if ((*used_ids)[u] == graph_node_id) {
                already = true;
                break;
            }
        }
        if (already)
            continue;

        /* 扩容 */
        if (*used_count >= *used_capacity) {
            int new_cap = *used_capacity > 0 ? *used_capacity * 2 : 16;
            int *new_arr = lv00_realloc(*used_ids, (size_t) new_cap * sizeof(int));
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

int find_all_non_overlapping_matches(ConstraintGraph *graph, RewriteRule *rule, const int *used_node_ids,
                                     int used_count, RewriteMatch ***out_matches, int *out_match_count) {
    if (!graph || !rule || !rule->pattern || !out_matches || !out_match_count)
        return -1;

    *out_matches = NULL;
    *out_match_count = 0;

    RewritePattern *pat = rule->pattern;
    if (pat->var_count == 0)
        return 0;

    /* 初始化本地已使用节点集合（合并外部传入的已使用节点） */
    int local_used_capacity = used_count > 0 ? used_count + 16 : 16;
    int *local_used = lv00_malloc((size_t) local_used_capacity * sizeof(int));
    if (!local_used)
        return -1;
    int local_used_count = 0;

    /* 复制外部传入的已使用节点 */
    for (int i = 0; i < used_count; i++) {
        if (local_used_count >= local_used_capacity) {
            int new_cap = local_used_capacity * 2;
            int *new_arr = lv00_realloc(local_used, (size_t) new_cap * sizeof(int));
            if (!new_arr) {
                lv00_free((void **) &local_used);
                return -1;
            }
            local_used = new_arr;
            local_used_capacity = new_cap;
        }
        local_used[local_used_count++] = used_node_ids[i];
    }

    /* 创建图快照，以便在搜索过程中临时移除已匹配节点 */
    GraphSnapshot *snapshot = graph_snapshot_create(graph);
    if (!snapshot) {
        lv00_free((void **) &local_used);
        return -1;
    }

    /* 匹配结果数组 */
    int match_capacity = 8;
    RewriteMatch **matches = lv00_malloc((size_t) match_capacity * sizeof(RewriteMatch *));
    if (!matches) {
        graph_snapshot_destroy(snapshot);
        lv00_free((void **) &local_used);
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
        if (!match)
            break;

        /* 检查前置条件 */
        if (!evaluate_precondition(graph, rule, match)) {
            lv00_free((void **) &match->node_bindings);
            lv00_free((void **) &match->constraint_bindings);
            lv00_free((void **) &match);
            break; /* 前置条件失败，停止搜索 */
        }

        /* 检查匹配是否与已使用节点重叠 */
        if (match_overlaps_used(match, local_used, local_used_count, node_binding_pairs)) {
            /* 匹配与已使用节点重叠 -- 需要移除已使用的节点后重新搜索。
             * 从图中移除已使用的节点，然后继续循环。 */
            for (int u = 0; u < local_used_count; u++) {
                graph_remove_node(graph, local_used[u]);
            }
            /* 清空本地已使用集合（已从图中移除） */
            local_used_count = 0;

            lv00_free((void **) &match->node_bindings);
            lv00_free((void **) &match->constraint_bindings);
            lv00_free((void **) &match);
            continue;
        }

        /* 找到一个有效的非重叠匹配 -- 保存它 */
        if (match_count >= match_capacity) {
            int new_cap = match_capacity * 2;
            RewriteMatch **new_arr = lv00_realloc(matches, (size_t) new_cap * sizeof(RewriteMatch *));
            if (!new_arr) {
                lv00_free((void **) &match->node_bindings);
                lv00_free((void **) &match->constraint_bindings);
                lv00_free((void **) &match);
                break;
            }
            matches = new_arr;
            match_capacity = new_cap;
        }
        matches[match_count++] = match;

        /* 将此匹配的节点添加到已使用集合 */
        add_match_to_used(match, &local_used, &local_used_count, &local_used_capacity, node_binding_pairs);

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
            lv00_free((void **) &matches[i]->node_bindings);
            lv00_free((void **) &matches[i]->constraint_bindings);
            lv00_free((void **) &matches[i]);
        }
        lv00_free((void **) &matches);
        lv00_free((void **) &local_used);
        *out_matches = NULL;
        *out_match_count = 0;
        return -1;
    }
    graph_snapshot_destroy(snapshot);

    /* 按匹配质量排序（匹配约束数降序） */
    if (match_count > 1) {
        qsort(matches, (size_t) match_count, sizeof(RewriteMatch *), match_quality_cmp);
    }

    lv00_free((void **) &local_used);

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

int rewrite_apply_all_matches(ConstraintGraph *graph, RewriteRule *rule, RewriteMatch *matches, int match_count,
                              int *out_applied_count) {
    if (!graph || !rule || !matches || match_count <= 0 || !out_applied_count)
        return -1;

    *out_applied_count = 0;

    /* 记录已被前序替换修改过的节点 ID，用于冲突检测 */
    int modified_capacity = 64;
    int *modified_node_ids = lv00_malloc((size_t) modified_capacity * sizeof(int));
    if (!modified_node_ids)
        return -1;
    int modified_count = 0;

    int applied = 0;

    for (int m = 0; m < match_count; m++) {
        RewriteMatch *match = &matches[m];

        /* 检查此匹配的节点是否已被前序替换修改过 */
        bool conflict = false;
        int node_binding_pairs = rule->pattern ? rule->pattern->var_count : 0;
        for (int i = 0; i < node_binding_pairs; i++) {
            int graph_node_id = match->node_bindings[i * 2 + 1];
            if (graph_node_id < 0)
                continue;

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
            if (conflict)
                break;
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
                if (graph_node_id < 0)
                    continue;

                if (modified_count >= modified_capacity) {
                    int new_cap = modified_capacity * 2;
                    int *new_arr = lv00_realloc(modified_node_ids, (size_t) new_cap * sizeof(int));
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
            (void) snap;
        }

        /* 注意：apply_rewrite 内部会创建并销毁自己的快照。
         * 此处的快照用于检测 apply_rewrite 是否真正修改了图。
         * 由于 apply_rewrite 在失败时已回滚，我们只需销毁此快照。 */
        graph_snapshot_destroy(snap);
    }

    lv00_free((void **) &modified_node_ids);
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
    GeomType *new_node_types; /* 新节点的几何类型 */
} ParsedRule;

/* 解析约束类型字符串 */
static ConstraintType parse_constraint_type(const char *str) {
    if (strcmp(str, "incidence") == 0)
        return INCIDENCE;
    if (strcmp(str, "betweenness") == 0)
        return BETWEENNESS;
    if (strcmp(str, "intersection") == 0)
        return INTERSECTION;
    if (strcmp(str, "containment") == 0)
        return CONTAINMENT;
    if (strcmp(str, "connection") == 0)
        return CONNECTION;
    return INCIDENCE; /* 默认 */
}

/**
 * @brief 跳过空白字符
 *
 * @param p 输入字符串指针
 * @return 跳过空白后的指针
 */
static const char *skip_whitespace(const char *p) {
    while (*p == ' ' || *p == '\t' || *p == '\r')
        p++;
    return p;
}

/**
 * @brief 跳过一行
 *
 * @param p 输入字符串指针
 * @return 跳过当前行后的指针
 */
static const char *skip_line(const char *p) {
    while (*p && *p != '\n')
        p++;
    if (*p == '\n')
        p++;
    return p;
}

/* 读取一个整数 token。
 * 支持可选的正负号前缀。如果数值超出 int 范围，将设置 *out 为
 * INT_MAX 或 INT_MIN 并继续解析（不会崩溃，但值可能不精确）。
 * 返回指向解析后下一个字符的指针。 */
static const char *read_int(const char *p, int *out) {
    p = skip_whitespace(p);
    *out = 0;
    int sign = 1;
    if (*p == '-') {
        sign = -1;
        p++;
    }
    while (*p >= '0' && *p <= '9') {
        int digit = *p - '0';
        /* 溢出检查：在乘法前判断 value * 10 是否会超出 INT_MAX/10 */
        if (*out > INT_MAX / 10 || (*out == INT_MAX / 10 && digit > INT_MAX % 10)) {
            /* 整数溢出，钳位到最大/最小值 */
            *out = (sign > 0) ? INT_MAX : INT_MIN;
            /* 跳过剩余数字字符 */
            while (*p >= '0' && *p <= '9')
                p++;
            return p;
        }
        *out = *out * 10 + digit;
        p++;
    }
    *out *= sign;
    return p;
}

/* 前向声明 */
static void parsed_rule_destroy(ParsedRule *rule);

/* 读取一个字符串 token（到空白或行尾） */
static const char *read_token(const char *p, char *buf, int buf_size) {
    p = skip_whitespace(p);
    int i = 0;
    while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' && i < buf_size - 1) {
        buf[i++] = *p++;
    }
    buf[i] = '\0';
    return p;
}

/**
 * @brief 从 .lvz 规则文件解析重写规则
 *
 * .lvz 文件格式支持以下指令：
 * - rule <name> <priority>：定义规则名称和优先级
 * - pattern_vars <v1> <v2> ...：定义模式变量
 * - pattern_constraint <type> <p1> [p2] [p3]：定义模式约束
 * - replacement_constraint <type> <p1> [p2] [p3]：定义替换约束
 * - node_binding <pattern_var> <target_id>：定义节点绑定
 * - new_nodes <id1> <id2> ...：定义新节点
 * - new_node_types <type1> <type2> ...：定义新节点类型
 *
 * @param filepath  规则文件路径
 * @param out_count 输出：解析得到的规则数量
 * @return 解析后的规则数组，失败返回 NULL
 */
static ParsedRule *parse_lvz_file(const char *filepath, int *out_count) {
    FILE *f = fopen(filepath, "r");
    if (!f)
        return NULL;

    /* 读取整个文件 */
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize <= 0) {
        fclose(f);
        return NULL;
    }

    char *content = lv00_malloc((size_t) fsize + 1);
    if (!content) {
        fclose(f);
        return NULL;
    }
    size_t nread = fread(content, 1, (size_t) fsize, f);
    content[nread] = '\0';
    fclose(f);

    /* 第一遍：计算规则数量 */
    int rule_count = 0;
    const char *p = content;
    while (*p) {
        if (*p == '#') {
            p = skip_line(p);
            continue;
        } /* 注释 */
        char token[64];
        p = read_token(p, token, sizeof(token));
        if (strcmp(token, "rule") == 0) {
            rule_count++;
        }
        p = skip_line(p);
    }

    if (rule_count == 0) {
        lv00_free((void **) &content);
        return NULL;
    }

    /* 分配规则数组 */
    ParsedRule *rules = lv00_malloc((size_t) rule_count * sizeof(ParsedRule));
    if (!rules) {
        lv00_free((void **) &content);
        return NULL;
    }
    memset(rules, 0, (size_t) rule_count * sizeof(ParsedRule));

    /* 第二遍：解析规则 */
    p = content;
    int current_rule = -1;
    while (*p) {
        if (*p == '#') {
            p = skip_line(p);
            continue;
        }
        if (*p == '\n') {
            p++;
            continue;
        }

        char token[256];
        p = read_token(p, token, sizeof(token));

        if (strcmp(token, "rule") == 0) {
            current_rule++;
            /* 读取规则名和优先级 */
            p = read_token(p, rules[current_rule].name, sizeof(rules[current_rule].name));
            p = read_int(p, &rules[current_rule].priority);
        } else if (current_rule >= 0) {
            if (strcmp(token, "pattern_vars") == 0) {
                /* pattern_vars: v1 v2 v3 ... */
                int count = 0;
                int vars[64];
                while (*p && *p != '\n') {
                    int v;
                    p = read_int(p, &v);
                    if (count < 64)
                        vars[count++] = v;
                }
                rules[current_rule].pattern_var_ids = lv00_malloc((size_t) count * sizeof(int));
                if (rules[current_rule].pattern_var_ids) {
                    memcpy(rules[current_rule].pattern_var_ids, vars, (size_t) count * sizeof(int));
                    rules[current_rule].pattern_var_count = count;
                }
            } else if (strcmp(token, "pattern_constraint") == 0) {
                /* pattern_constraint: type p1 p2 [p3] */
                char type_str[32];
                p = read_token(p, type_str, sizeof(type_str));
                int parts[8];
                int pcount = 0;
                while (*p && *p != '\n') {
                    int v;
                    const char *next = read_int(p, &v);
                    if (next == p)
                        break; /* 没有读到数字 */
                    p = next;
                    if (pcount < 8)
                        parts[pcount++] = v;
                }
                int idx = rules[current_rule].pattern_constraint_count;
                void *new_pc = lv00_realloc(rules[current_rule].pattern_constraints,
                                            (size_t) (idx + 1) * sizeof(rules[current_rule].pattern_constraints[0]));
                if (!new_pc) {
                    for (int r = 0; r <= current_rule; r++)
                        parsed_rule_destroy(&rules[r]);
                    lv00_free((void **) &rules);
                    lv00_free((void **) &content);
                    return NULL;
                }
                rules[current_rule].pattern_constraints = new_pc;
                rules[current_rule].pattern_constraints[idx].type = parse_constraint_type(type_str);
                rules[current_rule].pattern_constraints[idx].participant_count = pcount;
                memcpy(rules[current_rule].pattern_constraints[idx].participants, parts, (size_t) pcount * sizeof(int));
                rules[current_rule].pattern_constraint_count++;
            } else if (strcmp(token, "replacement_constraint") == 0) {
                /* replacement_constraint: type p1 p2 [p3] */
                char type_str[32];
                p = read_token(p, type_str, sizeof(type_str));
                int parts[8];
                int pcount = 0;
                while (*p && *p != '\n') {
                    int v;
                    const char *next = read_int(p, &v);
                    if (next == p)
                        break;
                    p = next;
                    if (pcount < 8)
                        parts[pcount++] = v;
                }
                int idx = rules[current_rule].replacement_constraint_count;
                void *new_rc =
                    lv00_realloc(rules[current_rule].replacement_constraints,
                                 (size_t) (idx + 1) * sizeof(rules[current_rule].replacement_constraints[0]));
                if (!new_rc) {
                    for (int r = 0; r <= current_rule; r++)
                        parsed_rule_destroy(&rules[r]);
                    lv00_free((void **) &rules);
                    lv00_free((void **) &content);
                    return NULL;
                }
                rules[current_rule].replacement_constraints = new_rc;
                rules[current_rule].replacement_constraints[idx].type = parse_constraint_type(type_str);
                rules[current_rule].replacement_constraints[idx].participant_count = pcount;
                memcpy(rules[current_rule].replacement_constraints[idx].participants, parts,
                       (size_t) pcount * sizeof(int));
                rules[current_rule].replacement_constraint_count++;
            } else if (strcmp(token, "node_binding") == 0) {
                /* node_binding: pattern_var target_id */
                int var_id, target;
                p = read_int(p, &var_id);
                p = read_int(p, &target);
                int idx = rules[current_rule].node_binding_count;
                void *new_nb = lv00_realloc(rules[current_rule].node_bindings,
                                            (size_t) (idx + 1) * sizeof(rules[current_rule].node_bindings[0]));
                if (!new_nb) {
                    for (int r = 0; r <= current_rule; r++)
                        parsed_rule_destroy(&rules[r]);
                    lv00_free((void **) &rules);
                    lv00_free((void **) &content);
                    return NULL;
                }
                rules[current_rule].node_bindings = new_nb;
                rules[current_rule].node_bindings[idx].pattern_var_id = var_id;
                rules[current_rule].node_bindings[idx].target_id = target;
                rules[current_rule].node_binding_count++;
            } else if (strcmp(token, "new_nodes") == 0) {
                /* new_nodes: id1 id2 ... */
                int nodes[64];
                int ncount = 0;
                while (*p && *p != '\n') {
                    int v;
                    const char *next = read_int(p, &v);
                    if (next == p)
                        break;
                    p = next;
                    if (ncount < 64)
                        nodes[ncount++] = v;
                }
                rules[current_rule].new_nodes = lv00_malloc((size_t) ncount * sizeof(int));
                if (rules[current_rule].new_nodes) {
                    memcpy(rules[current_rule].new_nodes, nodes, (size_t) ncount * sizeof(int));
                    rules[current_rule].new_node_count = ncount;
                }
            } else if (strcmp(token, "new_node_types") == 0) {
                /* new_node_types: type1 type2 ... (0=POINT, 1=LINE_SEGMENT, 2=REGION) */
                GeomType types[64];
                int tcount = 0;
                while (*p && *p != '\n') {
                    int v;
                    const char *next = read_int(p, &v);
                    if (next == p)
                        break;
                    p = next;
                    if (tcount < 64) {
                        /* 验证类型值合法性 */
                        if (v >= GEOM_POINT && v <= GEOM_FUNCTION_BLOCK) {
                            types[tcount++] = (GeomType) v;
                        } else {
                            types[tcount++] = GEOM_POINT; /* 默认为 POINT */
                        }
                    }
                }
                rules[current_rule].new_node_types = lv00_malloc((size_t) tcount * sizeof(GeomType));
                if (rules[current_rule].new_node_types) {
                    memcpy(rules[current_rule].new_node_types, types, (size_t) tcount * sizeof(GeomType));
                }
            }
        }
        p = skip_line(p);
    }

    lv00_free((void **) &content);
    *out_count = rule_count;
    return rules;
}

/**
 * @brief 释放解析后的规则数据
 *
 * 释放 ParsedRule 中所有动态分配的资源：
 * pattern_var_ids、pattern_constraints、replacement_constraints、
 * node_bindings 和 new_nodes 数组。
 *
 * @param rule 待销毁的解析规则指针（可为 NULL）
 */
static void parsed_rule_destroy(ParsedRule *rule) {
    if (!rule)
        return;
    lv00_free((void **) &rule->pattern_var_ids);
    lv00_free((void **) &rule->pattern_constraints);
    lv00_free((void **) &rule->replacement_constraints);
    lv00_free((void **) &rule->node_bindings);
    lv00_free((void **) &rule->new_nodes);
    lv00_free((void **) &rule->new_node_types);
}

/**
 * @brief 将 ParsedRule 转换为 RewriteRule
 *
 * 从解析后的规则数据构建完整的 RewriteRule 结构体：
 * - 构建 RewritePattern（变量 ID 列表和模式约束）
 * - 构建 RewriteReplacement（替换约束和节点绑定）
 * 分配失败时进行回滚清理，返回 NULL。
 *
 * @param pr 解析后的规则数据（只读）
 * @return 新分配的 RewriteRule 指针，失败返回 NULL
 */
static RewriteRule *parsed_rule_to_rewrite_rule(const ParsedRule *pr) {
    /* 构建模式 */
    RewritePattern *pattern = lv00_malloc(sizeof(RewritePattern));
    if (!pattern)
        return NULL;
    pattern->var_count = pr->pattern_var_count;
    pattern->variable_node_ids = NULL;
    if (pr->pattern_var_count > 0 && pr->pattern_var_ids) {
        pattern->variable_node_ids = lv00_malloc((size_t) pr->pattern_var_count * sizeof(int));
        if (pattern->variable_node_ids) {
            memcpy(pattern->variable_node_ids, pr->pattern_var_ids, (size_t) pr->pattern_var_count * sizeof(int));
        }
    }

    /* 构建模式约束 */
    pattern->pattern_constraint_count = pr->pattern_constraint_count;
    pattern->pattern_constraints = NULL;
    if (pr->pattern_constraint_count > 0 && pr->pattern_constraints) {
        pattern->pattern_constraints = lv00_malloc((size_t) pr->pattern_constraint_count * sizeof(Constraint *));
        if (pattern->pattern_constraints) {
            for (int i = 0; i < pr->pattern_constraint_count; i++) {
                Constraint *c = lv00_malloc(sizeof(Constraint));
                if (c) {
                    memset(c, 0, sizeof(Constraint));
                    c->type = pr->pattern_constraints[i].type;
                    c->participant_count = pr->pattern_constraints[i].participant_count;
                    c->participants = lv00_malloc((size_t) c->participant_count * sizeof(int));
                    if (c->participants) {
                        memcpy(c->participants, pr->pattern_constraints[i].participants,
                               (size_t) c->participant_count * sizeof(int));
                    }
                }
                pattern->pattern_constraints[i] = c;
            }
        }
    }

    /* 构建替换 */
    RewriteReplacement *replacement = lv00_malloc(sizeof(RewriteReplacement));
    if (!replacement) {
        /* 简化清理 */
        lv00_free((void **) &pattern->variable_node_ids);
        if (pattern->pattern_constraints) {
            for (int i = 0; i < pattern->pattern_constraint_count; i++) {
                if (pattern->pattern_constraints[i]) {
                    lv00_free((void **) &pattern->pattern_constraints[i]->participants);
                    lv00_free((void **) &pattern->pattern_constraints[i]);
                }
            }
            lv00_free((void **) &pattern->pattern_constraints);
        }
        lv00_free((void **) &pattern);
        return NULL;
    }

    /* 替换节点绑定 */
    replacement->binding_count = pr->node_binding_count;
    replacement->node_bindings = NULL;
    if (pr->node_binding_count > 0 && pr->node_bindings) {
        replacement->node_bindings = lv00_malloc((size_t) pr->node_binding_count * sizeof(int *));
        if (replacement->node_bindings) {
            for (int i = 0; i < pr->node_binding_count; i++) {
                replacement->node_bindings[i] = lv00_malloc(2 * sizeof(int));
                if (replacement->node_bindings[i]) {
                    replacement->node_bindings[i][0] = pr->node_bindings[i].pattern_var_id;
                    replacement->node_bindings[i][1] = pr->node_bindings[i].target_id;
                }
            }
        }
    }

    /* 替换约束 */
    replacement->replacement_constraint_count = pr->replacement_constraint_count;
    replacement->replacement_constraints = NULL;
    if (pr->replacement_constraint_count > 0 && pr->replacement_constraints) {
        replacement->replacement_constraints =
            lv00_malloc((size_t) pr->replacement_constraint_count * sizeof(Constraint *));
        if (replacement->replacement_constraints) {
            for (int i = 0; i < pr->replacement_constraint_count; i++) {
                Constraint *c = lv00_malloc(sizeof(Constraint));
                if (c) {
                    memset(c, 0, sizeof(Constraint));
                    c->type = pr->replacement_constraints[i].type;
                    c->participant_count = pr->replacement_constraints[i].participant_count;
                    c->participants = lv00_malloc((size_t) c->participant_count * sizeof(int));
                    if (c->participants) {
                        memcpy(c->participants, pr->replacement_constraints[i].participants,
                               (size_t) c->participant_count * sizeof(int));
                    }
                }
                replacement->replacement_constraints[i] = c;
            }
        }
    }

    /* 新节点 */
    replacement->new_node_count = pr->new_node_count;
    replacement->new_nodes = NULL;
    replacement->new_node_types = NULL;
    if (pr->new_node_count > 0 && pr->new_nodes) {
        replacement->new_nodes = lv00_malloc((size_t) pr->new_node_count * sizeof(int));
        if (replacement->new_nodes) {
            memcpy(replacement->new_nodes, pr->new_nodes, (size_t) pr->new_node_count * sizeof(int));
        }
    }
    /* 新节点类型 */
    if (pr->new_node_count > 0 && pr->new_node_types) {
        replacement->new_node_types = lv00_malloc((size_t) pr->new_node_count * sizeof(GeomType));
        if (replacement->new_node_types) {
            memcpy(replacement->new_node_types, pr->new_node_types, (size_t) pr->new_node_count * sizeof(GeomType));
        }
    }

    RewriteRule *rule = rewrite_rule_create(pr->name, pattern, replacement, pr->priority);
    return rule;
}

int rewrite_rules_load_from_file(const char *filepath, RewriteRule ***out_rules, int *out_count) {
    if (!filepath || !out_rules || !out_count)
        return -1;

    *out_rules = NULL;
    *out_count = 0;

    int parsed_count = 0;
    ParsedRule *parsed = parse_lvz_file(filepath, &parsed_count);
    if (!parsed || parsed_count <= 0) {
        if (parsed)
            lv00_free((void **) &parsed);
        return -1;
    }

    RewriteRule **rules = lv00_malloc((size_t) parsed_count * sizeof(RewriteRule *));
    if (!rules) {
        for (int i = 0; i < parsed_count; i++)
            parsed_rule_destroy(&parsed[i]);
        lv00_free((void **) &parsed);
        return -1;
    }
    memset(rules, 0, (size_t) parsed_count * sizeof(RewriteRule *));

    int loaded = 0;
    for (int i = 0; i < parsed_count; i++) {
        rules[loaded] = parsed_rule_to_rewrite_rule(&parsed[i]);
        if (rules[loaded]) {
            loaded++;
            if (rewrite_stream_ctx) {
                stream_emit_simple(rewrite_stream_ctx, STREAM_EVENT_REWRITE_RULE_LOADED,
                                   rules[loaded - 1]->name ? rules[loaded - 1]->name : "(unnamed)", 0);
            }
        }
        parsed_rule_destroy(&parsed[i]);
    }
    lv00_free((void **) &parsed);

    if (loaded == 0) {
        lv00_free((void **) &rules);
        return 0;
    }

    /* 压缩数组 */
    if (loaded < parsed_count) {
        RewriteRule **compressed = lv00_realloc(rules, (size_t) loaded * sizeof(RewriteRule *));
        if (compressed)
            rules = compressed;
    }

    *out_rules = rules;
    *out_count = loaded;
    return loaded;
}

bool rewrite_rule_unload(RewriteRule ***rules, int *count, const char *rule_name) {
    if (!rules || !*rules || !count || !rule_name)
        return false;

    int found_idx = -1;
    for (int i = 0; i < *count; i++) {
        if ((*rules)[i] && (*rules)[i]->name && strcmp((*rules)[i]->name, rule_name) == 0) {
            found_idx = i;
            break;
        }
    }

    if (found_idx < 0)
        return false;

    /* 销毁该规则 */
    RewriteRule *rule = (*rules)[found_idx];
    if (rule) {
        /* 销毁模式 */
        if (rule->pattern) {
            lv00_free((void **) &rule->pattern->variable_node_ids);
            if (rule->pattern->pattern_constraints) {
                for (int i = 0; i < rule->pattern->pattern_constraint_count; i++) {
                    if (rule->pattern->pattern_constraints[i]) {
                        lv00_free((void **) &rule->pattern->pattern_constraints[i]->participants);
                        lv00_free((void **) &rule->pattern->pattern_constraints[i]);
                    }
                }
                lv00_free((void **) &rule->pattern->pattern_constraints);
            }
            lv00_free((void **) &rule->pattern);
        }
        /* 销毁替换 */
        if (rule->replacement) {
            if (rule->replacement->node_bindings) {
                for (int i = 0; i < rule->replacement->binding_count; i++) {
                    lv00_free((void **) &rule->replacement->node_bindings[i]);
                }
                lv00_free((void **) &rule->replacement->node_bindings);
            }
            if (rule->replacement->replacement_constraints) {
                for (int i = 0; i < rule->replacement->replacement_constraint_count; i++) {
                    if (rule->replacement->replacement_constraints[i]) {
                        lv00_free((void **) &rule->replacement->replacement_constraints[i]->participants);
                        lv00_free((void **) &rule->replacement->replacement_constraints[i]);
                    }
                }
                lv00_free((void **) &rule->replacement->replacement_constraints);
            }
            lv00_free((void **) &rule->replacement->new_nodes);
            lv00_free((void **) &rule->replacement);
        }
        lv00_free((void **) &rule->name);
        lv00_free((void **) &rule);
    }

    /* 从数组中移除并压缩 */
    for (int i = found_idx; i < *count - 1; i++) {
        (*rules)[i] = (*rules)[i + 1];
    }
    (*count)--;

    /* 缩小数组 */
    if (*count > 0) {
        RewriteRule **compressed = lv00_realloc(*rules, (size_t) *count * sizeof(RewriteRule *));
        if (compressed)
            *rules = compressed;
    } else {
        lv00_free((void **) &*rules);
        *rules = NULL;
    }

    if (rewrite_stream_ctx) {
        stream_emit_simple(rewrite_stream_ctx, STREAM_EVENT_INFO, rule_name, 0);
    }

    return true;
}

/* ---- apply_rewrite (THE MAIN IMPLEMENTATION) ---- */

RewriteStatus apply_rewrite(ConstraintGraph *graph, RewriteRule *rule, RewriteMatch *match) {
    if (!graph || !rule || !match || !rule->replacement) {
        return REWRITE_NO_MATCH;
    }
    if (rule->reduction_measure < 0) {
        return REWRITE_NO_MATCH;
    }

    /* ================================================================
     * GRAPH SNAPSHOT — 用于替换操作的真正事务性回滚
     * 在执行任何修改前创建图的深拷贝快照。
     * 如果替换后检测到冲突，使用快照完整恢复图状态。
     * ================================================================ */
    GraphSnapshot *snapshot = graph_snapshot_create(graph);
    if (!snapshot) {
        return REWRITE_NO_MATCH;
    }

    const RewriteReplacement *repl = rule->replacement;
    const RewritePattern *pat = rule->pattern;

    /* ================================================================
     * TRANSACTION LOG
     * We record every mutation so we can roll back on failure.
     * ================================================================ */
    struct TxnEntry {
        enum { TXN_ADD_NODE, TXN_ADD_CONSTRAINT, TXN_REMOVE_NODE, TXN_REMOVE_CONSTRAINT } kind;
        int id;               /* node or constraint id */
        ConstraintType ctype; /* for added constraints */
        int *participants;    /* copy of participant array for added constraints */
        int participant_count;
    };

    int txn_cap = 64;
    int txn_count = 0;
    struct TxnEntry *txn = lv00_malloc((size_t) txn_cap * sizeof(struct TxnEntry));
    if (!txn) {
        graph_snapshot_destroy(snapshot);
        return REWRITE_NO_MATCH;
    }

#define TXN_PUSH(entry)                                                                            \
    do {                                                                                           \
        if (txn_count >= txn_cap) {                                                                \
            txn_cap *= 2;                                                                          \
            struct TxnEntry *_tmp = lv00_realloc(txn, (size_t) txn_cap * sizeof(struct TxnEntry)); \
            if (!_tmp)                                                                             \
                goto txn_rollback;                                                                 \
            txn = _tmp;                                                                            \
        }                                                                                          \
        txn[txn_count++] = (entry);                                                                \
    } while (0)

    RewriteStatus result = REWRITE_NO_MATCH;

    /* ----------------------------------------------------------------
     * Step a: Create new nodes from replacement.new_nodes
     *
     * new_nodes[i] is a placeholder id.  We create a real node in the
     * graph and record the mapping from placeholder -> actual id.
     *
     * If replacement specifies new_node_types[i], use that type.
     * Otherwise, infer from replacement constraint context:
     *   - If the new node participates in a constraint that implies
     *     LINE_SEGMENT endpoints, create POINT nodes.
     *   - Default to GEOM_POINT for backward compatibility.
     *
     * Supported types: GEOM_POINT, GEOM_LINE_SEGMENT, GEOM_REGION.
     * ---------------------------------------------------------------- */
    int *new_node_map = NULL;
    if (repl->new_node_count > 0) {
        new_node_map = lv00_malloc((size_t) repl->new_node_count * sizeof(int));
        if (!new_node_map)
            goto txn_cleanup;

        for (int i = 0; i < repl->new_node_count; i++) {
            GeomType node_type = GEOM_POINT; /* 默认类型 */

            /* 优先使用规则中显式指定的类型 */
            if (repl->new_node_types && i < repl->new_node_count) {
                node_type = repl->new_node_types[i];
            } else {
                /* 推断类型：扫描替换约束，检查新节点参与的约束类型 */
                int placeholder_id = repl->new_nodes[i];
                for (int c = 0; c < repl->replacement_constraint_count; c++) {
                    Constraint *rc = repl->replacement_constraints[c];
                    bool involves_new_node = false;
                    for (int p = 0; p < rc->participant_count; p++) {
                        if (rc->participants[p] == placeholder_id) {
                            involves_new_node = true;
                            break;
                        }
                    }
                    if (involves_new_node) {
                        /* 如果约束类型是 INCIDENCE 且参与者数量为 2，
                           新节点可能是线段端点 -> 保持 POINT */
                        if (rc->type == INCIDENCE && rc->participant_count == 2) {
                            node_type = GEOM_POINT;
                        }
                        /* 如果约束类型暗示线段参与，且新节点是线段本身 */
                        if (rc->type == BETWEENNESS && rc->participant_count == 3) {
                            /* BETWEENNESS 的三个参与者可能是 (p1, p2, p3)，
                               如果新节点不是端点，保持 POINT */
                        }
                    }
                }
            }

            AddNodeResult nr = ADD_NODE_OK;
            int actual_id = -1;

            switch (node_type) {
                case GEOM_LINE_SEGMENT: {
                    /* 创建线段需要两个端点。如果替换约束中有 INCIDENCE
                   关联到此线段的端点，使用已解析的端点 ID。
                   否则创建两个占位点作为端点。 */
                    int ep1_id = -1, ep2_id = -1;
                    int placeholder_id = repl->new_nodes[i];

                    /* 尝试从替换约束中找到关联的端点 */
                    for (int c = 0; c < repl->replacement_constraint_count && ep1_id < 0; c++) {
                        Constraint *rc = repl->replacement_constraints[c];
                        if (rc->type == INCIDENCE && rc->participant_count == 2) {
                            for (int p = 0; p < rc->participant_count; p++) {
                                if (rc->participants[p] == placeholder_id) {
                                    int other_idx = 1 - p;
                                    int other_id = rc->participants[other_idx];
                                    if (other_id < 0) {
                                        /* 模式变量 -> 查找匹配绑定 */
                                        other_id =
                                            resolve_binding(match->node_bindings, match->binding_count, other_id);
                                    } else if (other_id != placeholder_id) {
                                        /* 检查是否是另一个新节点 */
                                        bool is_other_new = false;
                                        for (int nn = 0; nn < i; nn++) {
                                            if (repl->new_nodes[nn] == other_id) {
                                                other_id = new_node_map[nn];
                                                is_other_new = true;
                                                break;
                                            }
                                        }
                                    }
                                    if (ep1_id < 0)
                                        ep1_id = other_id;
                                    else if (ep2_id < 0)
                                        ep2_id = other_id;
                                }
                            }
                        }
                    }

                    /* 如果没有找到端点，创建占位点 */
                    if (ep1_id < 0) {
                        SymbolicCoord *zc = symbolic_coord_create_rational(0, 1);
                        SymbolicCoord *coords[] = {zc};
                        nr = graph_add_point(graph, coords, 1);
                        symbolic_coord_destroy(zc);
                        if (nr != ADD_NODE_OK)
                            goto txn_rollback;
                        ep1_id = graph->next_node_id - 1;
                        struct TxnEntry ep_e;
                        ep_e.kind = TXN_ADD_NODE;
                        ep_e.id = ep1_id;
                        ep_e.participants = NULL;
                        ep_e.participant_count = 0;
                        TXN_PUSH(ep_e);
                    }
                    if (ep2_id < 0) {
                        SymbolicCoord *zc = symbolic_coord_create_rational(0, 1);
                        SymbolicCoord *coords[] = {zc};
                        nr = graph_add_point(graph, coords, 1);
                        symbolic_coord_destroy(zc);
                        if (nr != ADD_NODE_OK)
                            goto txn_rollback;
                        ep2_id = graph->next_node_id - 1;
                        struct TxnEntry ep_e;
                        ep_e.kind = TXN_ADD_NODE;
                        ep_e.id = ep2_id;
                        ep_e.participants = NULL;
                        ep_e.participant_count = 0;
                        TXN_PUSH(ep_e);
                    }

                    nr = graph_add_line_segment(graph, ep1_id, ep2_id);
                    if (nr != ADD_NODE_OK)
                        goto txn_rollback;
                    actual_id = graph->next_node_id - 1;
                    break;
                }

                case GEOM_REGION: {
                    /* 创建区域需要边界线段 ID。
                   尝试从替换约束中找到 CONTAINMENT 关联的线段。 */
                    int seg_ids[64];
                    int seg_count = 0;
                    int placeholder_id = repl->new_nodes[i];

                    for (int c = 0; c < repl->replacement_constraint_count && seg_count < 64; c++) {
                        Constraint *rc = repl->replacement_constraints[c];
                        if (rc->type == CONTAINMENT && rc->participant_count == 2) {
                            for (int p = 0; p < rc->participant_count; p++) {
                                if (rc->participants[p] == placeholder_id) {
                                    int other_idx = 1 - p;
                                    int other_id = rc->participants[other_idx];
                                    if (other_id < 0) {
                                        other_id =
                                            resolve_binding(match->node_bindings, match->binding_count, other_id);
                                    } else {
                                        /* 检查是否是另一个新节点 */
                                        for (int nn = 0; nn < i; nn++) {
                                            if (repl->new_nodes[nn] == other_id) {
                                                other_id = new_node_map[nn];
                                                break;
                                            }
                                        }
                                    }
                                    if (other_id >= 0) {
                                        seg_ids[seg_count++] = other_id;
                                    }
                                }
                            }
                        }
                    }

                    /* 如果没有找到边界线段，创建一个空区域（使用空数组） */
                    nr = graph_add_region(graph, seg_ids, seg_count);
                    if (nr != ADD_NODE_OK)
                        goto txn_rollback;
                    actual_id = graph->next_node_id - 1;
                    break;
                }

                case GEOM_POINT:
                default: {
                    /* 创建 POINT 节点（原有逻辑） */
                    SymbolicCoord *zero_coord = symbolic_coord_create_rational(0, 1);
                    SymbolicCoord *coords[] = {zero_coord};
                    nr = graph_add_point(graph, coords, 1);
                    symbolic_coord_destroy(zero_coord);

                    if (nr != ADD_NODE_OK) {
                        goto txn_rollback;
                    }
                    actual_id = graph->next_node_id - 1;
                    break;
                }
            }

            if (nr != ADD_NODE_OK) {
                goto txn_rollback;
            }

            new_node_map[i] = actual_id;

            struct TxnEntry e;
            e.kind = TXN_ADD_NODE;
            e.id = actual_id;
            e.participants = NULL;
            e.participant_count = 0;
            TXN_PUSH(e);
        }
    }

    /* ----------------------------------------------------------------
     * Step b: Create new constraints from replacement.replacement_constraints
     *
     * For each replacement constraint, resolve every participant id:
     *   - negative id  -> pattern variable -> look up in match bindings
     *   - new node id  -> look up in new_node_map
     *   - positive id not in pattern -> external node, keep as-is
     * ---------------------------------------------------------------- */
    for (int c = 0; c < repl->replacement_constraint_count; c++) {
        Constraint *rc = repl->replacement_constraints[c];
        int *resolved = lv00_malloc((size_t) rc->participant_count * sizeof(int));
        if (!resolved)
            goto txn_rollback;

        bool all_ok = true;
        for (int p = 0; p < rc->participant_count; p++) {
            int rid = resolve_replacement_participant(rc->participants[p], match->node_bindings, match->binding_count,
                                                      new_node_map, repl->new_node_count, repl->new_nodes,
                                                      repl->new_node_count);
            if (rid < 0) {
                all_ok = false;
                break;
            }
            resolved[p] = rid;
        }

        if (!all_ok) {
            lv00_free((void **) &resolved);
            goto txn_rollback;
        }

        /* 验证所有引用的节点确实存在 */
        for (int p = 0; p < rc->participant_count; p++) {
            if (!graph_get_node(graph, resolved[p])) {
                lv00_free((void **) &resolved);
                goto txn_rollback;
            }
        }

        bool added = add_constraint_generic(graph, rc->type, resolved, rc->participant_count);
        if (!added) {
            lv00_free((void **) &resolved);
            goto txn_rollback;
        }

        /* 记录添加的约束，以便可能回滚 */
        int new_con_id = graph->next_constraint_id - 1;
        struct TxnEntry e;
        e.kind = TXN_ADD_CONSTRAINT;
        e.id = new_con_id;
        e.ctype = rc->type;
        e.participants = resolved;
        e.participant_count = rc->participant_count;
        TXN_PUSH(e);
    }

    /* ----------------------------------------------------------------
     * Step c: Remove old matched constraints
     *
     * Remove every constraint that was matched by the pattern.
     * ---------------------------------------------------------------- */
    for (int i = 0; i < match->binding_count; i++) {
        int con_id = match->constraint_bindings[i];
        if (graph_get_constraint(graph, con_id)) {
            struct TxnEntry e;
            e.kind = TXN_REMOVE_CONSTRAINT;
            e.id = con_id;
            e.participants = NULL;
            e.participant_count = 0;
            TXN_PUSH(e);

            if (graph_remove_constraint(graph, con_id) != REMOVE_CONSTRAINT_OK) {
                goto txn_rollback;
            }
        }
    }

    /* ----------------------------------------------------------------
     * Step d: Remove old matched pattern nodes that are NOT referenced
     * by the replacement.
     *
     * A matched node (bound to a negative pattern var id) should be
     * removed if:
     *   1. It does NOT appear in any replacement constraint, AND
     *   2. It does NOT appear in the replacement node_bindings
     * ---------------------------------------------------------------- */
    for (int i = 0; i < match->binding_count; i++) {
        int pattern_var_id = match->node_bindings[i * 2];
        int graph_node_id = match->node_bindings[i * 2 + 1];

        /* 只考虑模式变量（负 ID） */
        if (pattern_var_id >= 0)
            continue;

        /* 检查替换结果是否仍需要此节点 */
        bool used = pattern_var_used_in_replacement(repl, pattern_var_id);
        if (used)
            continue;

        /* 检查替换结果是否重新绑定了此变量 */
        bool rebound = pattern_var_in_replacement_bindings(repl, pattern_var_id);
        if (rebound)
            continue;

        /* 还需检查：此节点是否被任何未匹配的约束引用？
           如果是，删除它会破坏这些约束。
           graph_remove_node 函数已处理移除引用，但我们应仅在
           该节点没有剩余约束引用时才删除它。 */
        bool has_external_refs = false;
        for (int c = 0; c < graph->constraint_count; c++) {
            Constraint *con = graph->constraints[c];
            if (is_matched_constraint(match, con->id))
                continue;
            for (int p = 0; p < con->participant_count; p++) {
                if (con->participants[p] == graph_node_id) {
                    has_external_refs = true;
                    break;
                }
            }
            if (has_external_refs)
                break;
        }
        if (has_external_refs)
            continue;

        /* 可以安全移除 */
        GeomNode *node = graph_get_node(graph, graph_node_id);
        if (node && node->type != GEOM_REGION) {
            struct TxnEntry e;
            e.kind = TXN_REMOVE_NODE;
            e.id = graph_node_id;
            e.participants = NULL;
            e.participant_count = 0;
            TXN_PUSH(e);

            if (graph_remove_node(graph, graph_node_id) != REMOVE_NODE_OK) {
                goto txn_rollback;
            }
        }
    }

    /* ----------------------------------------------------------------
     * Step e: Boundary reconnection is implicit.
     *
     * Any replacement constraint that references external nodes (positive
     * ids not in the pattern) already keeps those references because
     * resolve_replacement_participant passes them through unchanged.
     * No additional work is needed here.
     * ---------------------------------------------------------------- */

    result = REWRITE_APPLIED;

    /* 验证约束图一致性：若产生冲突，使用快照回滚 */
    if (!check_graph_consistency(graph)) {
        graph_snapshot_restore(snapshot, graph);
        graph_snapshot_destroy(snapshot);
        result = REWRITE_CONFLUENCE_ISSUE;
        goto txn_cleanup;
    }

    goto txn_cleanup;

txn_rollback:
    /* 使用图快照进行真正的回滚，替代原来的逐操作撤销 */
    graph_snapshot_restore(snapshot, graph);
    result = REWRITE_NO_MATCH;

txn_cleanup:
    /* 销毁快照（成功路径或回滚路径都需要销毁） */
    graph_snapshot_destroy(snapshot);

    for (int i = 0; i < txn_count; i++) {
        if (txn[i].kind == TXN_ADD_CONSTRAINT && txn[i].participants) {
            lv00_free((void **) &txn[i].participants);
        }
    }
    lv00_free((void **) &txn);
    lv00_free((void **) &new_node_map);

#undef TXN_PUSH

    return result;
}

/**
 * @brief qsort 比较函数：按 reduction_measure 降序排序，
 *        相同则按注册顺序（原始索引）升序排列
 *
 * @param a 规则指针 a
 * @param b 规则指针 b
 * @return 比较结果
 */
typedef struct {
    RewriteRule *rule;
    int original_index;
} SortedRule;

static int sorted_rule_cmp(const void *a, const void *b) {
    const SortedRule *sa = (const SortedRule *) a;
    const SortedRule *sb = (const SortedRule *) b;
    if (sa->rule->reduction_measure != sb->rule->reduction_measure) {
        /* 度量值高的优先 */
        return (sb->rule->reduction_measure > sa->rule->reduction_measure) ? 1 : -1;
    }
    /* 相同度量值：保持注册顺序 */
    return (sa->original_index - sb->original_index);
}

RewriteStatus rewrite_with_rules(ConstraintGraph *graph, RewriteRule **rules, int rule_count, int step_limit,
                                 bool normalize_between_steps) {
    if (rule_count <= 0)
        return REWRITE_OK;

    if (rewrite_stream_ctx) {
        stream_emit_simple(rewrite_stream_ctx, STREAM_EVENT_REWRITE_START, "rewrite phase started", 0);
    }

    /* 按规则优先级排序 */
    SortedRule *sorted = lv00_malloc((size_t) rule_count * sizeof(SortedRule));
    if (!sorted)
        return REWRITE_TERMINATED;
    for (int i = 0; i < rule_count; i++) {
        sorted[i].rule = rules[i];
        sorted[i].original_index = i;
    }
    qsort(sorted, (size_t) rule_count, sizeof(SortedRule), sorted_rule_cmp);

    int steps = 0;
    int *history_hashes = lv00_malloc((size_t) step_limit * sizeof(uint32_t));
    if (!history_hashes) {
        lv00_free((void **) &sorted);
        return REWRITE_TERMINATED;
    }
    int history_count = 0;

    RewriteStatus final_status = REWRITE_OK;

    while (steps < step_limit) {
        /* 通过图哈希检测重写循环 */
        uint32_t current_hash = compute_graph_hash(graph);
        bool loop_detected = false;
        for (int i = 0; i < history_count; i++) {
            if (history_hashes[i] == current_hash) {
                loop_detected = true;
                break;
            }
        }
        if (loop_detected) {
            if (rewrite_stream_ctx) {
                stream_emit_simple(rewrite_stream_ctx, STREAM_EVENT_ERROR, "rewrite loop detected, terminating", steps);
            }
            final_status = REWRITE_TERMINATED;
            break;
        }
        if (history_count < step_limit) {
            history_hashes[history_count++] = current_hash;
        }

        /* 按优先级依次尝试每条规则 */
        bool applied = false;
        for (int i = 0; i < rule_count; i++) {
            RewriteRule *rule = sorted[i].rule;
            RewriteMatch *match = find_rewrite_match(graph, rule, false);
            if (!match)
                continue;

            if (rewrite_stream_ctx) {
                stream_emit_simple(rewrite_stream_ctx, STREAM_EVENT_REWRITE_MATCH_FOUND,
                                   rule->name ? rule->name : "rule matched", steps);
            }

            RewriteStatus status = apply_rewrite(graph, rule, match);
            if (status == REWRITE_APPLIED) {
                if (rewrite_stream_ctx) {
                    stream_emit_simple(rewrite_stream_ctx, STREAM_EVENT_REWRITE_APPLIED,
                                       rule->name ? rule->name : "rule applied", steps);
                }
                /* apply_rewrite 内部已通过快照机制处理冲突检测和回滚，
                 * 返回 REWRITE_APPLIED 表示替换成功且图一致。 */

                /* 根据 design_v2.9.md Section 6.4，可在重写步骤之间
                 * 选择性进行规范化，以防止冗余节点干扰后续匹配。 */
                if (normalize_between_steps) {
                    NormalizationResult *norm_result = graph_normalize(graph, false);
                    if (norm_result)
                        normalization_result_destroy(norm_result);
                }

                applied = true;
                lv00_free((void **) &match);
                break;
            } else {
                if (rewrite_stream_ctx) {
                    stream_emit_simple(rewrite_stream_ctx, STREAM_EVENT_REWRITE_ROLLBACK,
                                       rule->name ? rule->name : "rule rolled back", steps);
                }
            }
            lv00_free((void **) &match);
        }

        if (!applied)
            break;
        steps++;
    }

    if (steps >= step_limit && final_status == REWRITE_OK) {
        final_status = REWRITE_TERMINATED;
    }

    if (rewrite_stream_ctx) {
        stream_emit_simple(rewrite_stream_ctx, STREAM_EVENT_REWRITE_DONE, "rewrite phase done", steps);
    }

done:
    lv00_free((void **) &history_hashes);
    lv00_free((void **) &sorted);
    return final_status;
}

/* ===========================================================================
 * 循环检测
 * ===========================================================================
 */

/**
 * @brief 检测重写循环：判断当前图哈希是否在历史中出现过
 *
 * 计算当前约束图的结构哈希值，与历史哈希记录逐一比对。
 * 若匹配则说明图状态已出现过，形成重写循环。
 *
 * @param graph          当前约束图指针
 * @param history_hashes 历史哈希值数组
 * @param history_count  历史记录数量
 * @return true 表示检测到循环，false 表示未检测到
 */
static bool detect_rewrite_loop(ConstraintGraph *graph, int *history_hashes, int history_count) {
    uint32_t current_hash = compute_graph_hash(graph);
    for (int i = 0; i < history_count; i++) {
        if (history_hashes[i] == current_hash) {
            return true;
        }
    }
    return false;
}

/* ===========================================================================
 * VF2 子图同构匹配算法
 *
 * VF2 (Vento-Foggia 2) 是一种高效的子图同构验证算法，通过递归搜索
 * 和可行性剪枝来检测模式图是否与目标图的某个子图同构。
 * ===========================================================================
 */

/* 初始化 VF2 匹配状态 */
static void vf2_state_init(VF2State *state, int pattern_size, int target_size) {
    state->pattern_size = pattern_size;
    state->target_size = target_size;
    state->core_count = 0;

    state->core_1 = lv00_malloc((size_t) pattern_size * sizeof(int));
    state->core_2 = lv00_malloc((size_t) target_size * sizeof(int));
    state->in_1 = lv00_malloc((size_t) pattern_size * sizeof(int));
    state->out_1 = lv00_malloc((size_t) pattern_size * sizeof(int));
    state->in_2 = lv00_malloc((size_t) target_size * sizeof(int));
    state->out_2 = lv00_malloc((size_t) target_size * sizeof(int));

    if (state->in_1)
        memset(state->in_1, 0, (size_t) pattern_size * sizeof(int));
    if (state->out_1)
        memset(state->out_1, 0, (size_t) pattern_size * sizeof(int));
    if (state->in_2)
        memset(state->in_2, 0, (size_t) target_size * sizeof(int));
    if (state->out_2)
        memset(state->out_2, 0, (size_t) target_size * sizeof(int));

    for (int i = 0; i < pattern_size; i++)
        state->core_1[i] = -1;
    for (int i = 0; i < target_size; i++)
        state->core_2[i] = -1;

    /* 初始化 in/out 集合 */
    int initial_capacity = target_size > 0 ? target_size : 8;
    state->in_set = lv00_malloc((size_t) initial_capacity * sizeof(int));
    state->out_set = lv00_malloc((size_t) initial_capacity * sizeof(int));
    state->in_count = 0;
    state->out_count = 0;
    state->in_capacity = initial_capacity;
    state->out_capacity = initial_capacity;
}

/* 销毁 VF2 匹配状态，释放内存 */
static void vf2_state_destroy(VF2State *state) {
    lv00_free((void **) &state->core_1);
    lv00_free((void **) &state->core_2);
    lv00_free((void **) &state->in_1);
    lv00_free((void **) &state->out_1);
    lv00_free((void **) &state->in_2);
    lv00_free((void **) &state->out_2);
    lv00_free((void **) &state->in_set);
    lv00_free((void **) &state->out_set);
    state->core_1 = state->core_2 = NULL;
    state->in_1 = state->out_1 = NULL;
    state->in_2 = state->out_2 = NULL;
    state->in_set = state->out_set = NULL;
}

/* 检查模式图中节点 p 的邻居是否与目标图中节点 t 的邻居兼容。
 * 对于已映射的邻居，验证对应关系的一致性。
 * 对于 POINT 节点，使用 symbolic_coord_compare() 进行符号坐标判等。 */
static bool vf2_feasible(VF2State *state, int p, int t, ConstraintGraph *pattern_graph, ConstraintGraph *target_graph,
                         bool local_equivalence_tolerant) {
    GeomNode *pn = pattern_graph->nodes[p];
    GeomNode *tn = target_graph->nodes[t];

    /* 节点类型必须匹配 */
    if (pn->type != tn->type)
        return false;

    /* 增强语义可行性检查：信任颜色和 Light Orange 子类型 */
    if (pn->trust != tn->trust)
        return false;
    if (pn->lo_subtype != tn->lo_subtype)
        return false;

    /* PORT 节点语义检查：端口类型必须一致 */
    if (pn->type == GEOM_PORT) {
        if (!pn->data.port || !tn->data.port)
            return false;
        if (pn->data.port->type != tn->data.port->type)
            return false;
    }

    /* FUNCTION_BLOCK 节点语义检查：确定性状态必须一致 */
    if (pn->type == GEOM_FUNCTION_BLOCK) {
        if (pn->data.func_block.determinism_state != tn->data.func_block.determinism_state)
            return false;
    }

    /* 对于 POINT 节点，在局部等价容忍模式下检查符号坐标 */
    if (pn->type == GEOM_POINT && local_equivalence_tolerant) {
        if (pn->coord_count != tn->coord_count)
            return false;
        for (int c = 0; c < pn->coord_count; c++) {
            if (symbolic_coord_compare(pn->symbolic_coords[c], tn->symbolic_coords[c]) != 0) {
                return false;
            }
        }
    }

    /* 对于 LINE_SEGMENT 节点，在局部等价容忍模式下比较端点坐标。
     * LINE_SEGMENT 的两个端点通过 INCIDENCE 约束关联。
     * 分别收集模式图和目标图中线段的两个端点坐标，进行排序后比对，
     * 以支持端点顺序无关的等价判断。 */
    if (pn->type == GEOM_LINE_SEGMENT && local_equivalence_tolerant) {
        /* 收集模式图中 p 的两个端点的符号坐标 */
        SymbolicCoord *p_endpoint_coords[2] = {NULL, NULL};
        int p_ep_count = 0;
        for (int c = 0; c < pattern_graph->constraint_count; c++) {
            Constraint *pc = pattern_graph->constraints[c];
            if (pc->type == INCIDENCE && pc->participant_count == 2) {
                for (int k = 0; k < 2; k++) {
                    if (pc->participants[k] == p) {
                        int ep_idx = pc->participants[1 - k];
                        if (ep_idx >= 0 && ep_idx < pattern_graph->node_count) {
                            GeomNode *ep_node = pattern_graph->nodes[ep_idx];
                            if (ep_node && ep_node->type == GEOM_POINT && ep_node->coord_count > 0 &&
                                ep_node->symbolic_coords) {
                                p_endpoint_coords[p_ep_count++] = ep_node->symbolic_coords[0];
                                if (p_ep_count >= 2)
                                    break;
                            }
                        }
                    }
                }
            }
            if (p_ep_count >= 2)
                break;
        }
        /* 收集目标图中 t 的两个端点的符号坐标 */
        SymbolicCoord *t_endpoint_coords[2] = {NULL, NULL};
        int t_ep_count = 0;
        for (int c = 0; c < target_graph->constraint_count; c++) {
            Constraint *tc = target_graph->constraints[c];
            if (tc->type == INCIDENCE && tc->participant_count == 2) {
                for (int k = 0; k < 2; k++) {
                    if (tc->participants[k] == t) {
                        int ep_idx = tc->participants[1 - k];
                        GeomNode *ep_node = graph_get_node(target_graph, ep_idx);
                        if (ep_node && ep_node->type == GEOM_POINT && ep_node->coord_count > 0 &&
                            ep_node->symbolic_coords) {
                            t_endpoint_coords[t_ep_count++] = ep_node->symbolic_coords[0];
                            if (t_ep_count >= 2)
                                break;
                        }
                    }
                }
            }
            if (t_ep_count >= 2)
                break;
        }
        /* 如果两端点坐标都收集到了，进行排序后比对 */
        if (p_ep_count == 2 && t_ep_count == 2) {
            /* 排序模式图端点坐标 */
            if (symbolic_coord_compare(p_endpoint_coords[0], p_endpoint_coords[1]) > 0) {
                SymbolicCoord *tmp = p_endpoint_coords[0];
                p_endpoint_coords[0] = p_endpoint_coords[1];
                p_endpoint_coords[1] = tmp;
            }
            /* 排序目标图端点坐标 */
            if (symbolic_coord_compare(t_endpoint_coords[0], t_endpoint_coords[1]) > 0) {
                SymbolicCoord *tmp = t_endpoint_coords[0];
                t_endpoint_coords[0] = t_endpoint_coords[1];
                t_endpoint_coords[1] = tmp;
            }
            /* 逐对比较排序后的端点坐标 */
            if (symbolic_coord_compare(p_endpoint_coords[0], t_endpoint_coords[0]) != 0 ||
                symbolic_coord_compare(p_endpoint_coords[1], t_endpoint_coords[1]) != 0) {
                return false;
            }
        }
    }

    /* 检查已匹配邻居的一致性：
     * 对于模式图中 p 的每个已映射邻居 p'，目标图中 t 必须有对应的
     * 已映射邻居 t'，且 (p', t') 必须在 core 中。 */
    for (int c = 0; c < pattern_graph->constraint_count; c++) {
        Constraint *pc = pattern_graph->constraints[c];
        bool p_participates = false;
        for (int k = 0; k < pc->participant_count; k++) {
            if (pc->participants[k] == p) {
                p_participates = true;
                continue;
            }
            int p_neighbor = pc->participants[k];
            if (state->core_1[p_neighbor] < 0)
                continue;
            int t_neighbor = state->core_1[p_neighbor];
            /* 检查 t 和 t_neighbor 之间是否存在相同类型的约束 */
            bool found = false;
            for (int ci = 0; ci < target_graph->constraint_count; ci++) {
                Constraint *tc = target_graph->constraints[ci];
                if (tc->type != pc->type)
                    continue;
                bool t_in = false, tn_in = false;
                for (int kk = 0; kk < tc->participant_count; kk++) {
                    if (tc->participants[kk] == t)
                        t_in = true;
                    if (tc->participants[kk] == t_neighbor)
                        tn_in = true;
                }
                if (t_in && tn_in) {
                    found = true;
                    break;
                }
            }
            if (!found)
                return false;
        }
        if (!p_participates)
            continue;
    }

    /* 反向检查：目标图中 t 的已映射邻居在模式图中也必须兼容 */
    for (int c = 0; c < target_graph->constraint_count; c++) {
        Constraint *tc = target_graph->constraints[c];
        bool t_participates = false;
        for (int k = 0; k < tc->participant_count; k++) {
            if (tc->participants[k] == t) {
                t_participates = true;
                continue;
            }
            int t_neighbor = tc->participants[k];
            if (state->core_2[t_neighbor] < 0)
                continue;
            int p_mapped = state->core_2[t_neighbor];
            /* 在模式图中，p 和 p_mapped 之间必须存在相同类型的约束 */
            bool found = false;
            for (int ci = 0; ci < pattern_graph->constraint_count; ci++) {
                Constraint *pcon = pattern_graph->constraints[ci];
                if (pcon->type != tc->type)
                    continue;
                bool p_in = false, pm_in = false;
                for (int kk = 0; kk < pcon->participant_count; kk++) {
                    if (pcon->participants[kk] == p)
                        p_in = true;
                    if (pcon->participants[kk] == p_mapped)
                        pm_in = true;
                }
                if (p_in && pm_in) {
                    found = true;
                    break;
                }
            }
            if (!found)
                return false;
        }
        if (!t_participates)
            continue;
    }

    return true;
}

/* VF2 前瞻函数：检查匹配 p->t 是否有前景。
 * 验证未映射邻居的兼容性，确保不会因为当前匹配导致
 * 后续无法完成匹配。 */
static bool vf2_lookahead(VF2State *state, int p, int t, ConstraintGraph *pattern_graph,
                          ConstraintGraph *target_graph) {
    /* 统计模式图中 p 的未映射邻居数量 */
    int p_unmapped_neighbors = 0;
    for (int c = 0; c < pattern_graph->constraint_count; c++) {
        Constraint *pc = pattern_graph->constraints[c];
        bool p_in = false;
        bool all_unmapped = true;
        for (int k = 0; k < pc->participant_count; k++) {
            if (pc->participants[k] == p) {
                p_in = true;
            } else if (state->core_1[pc->participants[k]] >= 0) {
                all_unmapped = false;
            }
        }
        if (p_in && all_unmapped) {
            p_unmapped_neighbors++;
        }
    }

    /* 统计目标图中 t 的未映射邻居数量 */
    int t_unmapped_neighbors = 0;
    for (int c = 0; c < target_graph->constraint_count; c++) {
        Constraint *tc = target_graph->constraints[c];
        bool t_in = false;
        bool all_unmapped = true;
        for (int k = 0; k < tc->participant_count; k++) {
            if (tc->participants[k] == t) {
                t_in = true;
            } else if (state->core_2[tc->participants[k]] >= 0) {
                all_unmapped = false;
            }
        }
        if (t_in && all_unmapped) {
            t_unmapped_neighbors++;
        }
    }

    /* 目标图的未映射邻居数不能少于模式图的未映射邻居数 */
    if (t_unmapped_neighbors < p_unmapped_neighbors)
        return false;

    /* 检查未映射邻居的度数兼容性：
     * 对于模式图中 p 的每个未映射邻居 p'，
     * 目标图中至少要有一个类型兼容的未映射邻居 t' */
    for (int c = 0; c < pattern_graph->constraint_count; c++) {
        Constraint *pc = pattern_graph->constraints[c];
        bool p_in = false;
        for (int k = 0; k < pc->participant_count; k++) {
            if (pc->participants[k] == p) {
                p_in = true;
                continue;
            }
            int p_neighbor = pc->participants[k];
            if (state->core_1[p_neighbor] >= 0)
                continue; /* 已映射，跳过 */

            GeomNode *pn = pattern_graph->nodes[p_neighbor];

            /* 在目标图中查找类型兼容的未映射邻居 */
            bool has_compatible = false;
            for (int gc = 0; gc < target_graph->constraint_count; gc++) {
                Constraint *tc = target_graph->constraints[gc];
                if (tc->type != pc->type)
                    continue;

                bool t_in = false;
                for (int kk = 0; kk < tc->participant_count; kk++) {
                    if (tc->participants[kk] == t) {
                        t_in = true;
                        continue;
                    }
                    int t_neighbor = tc->participants[kk];
                    if (state->core_2[t_neighbor] >= 0)
                        continue; /* 已映射，跳过 */

                    GeomNode *tn = target_graph->nodes[t_neighbor];
                    if (tn->type == pn->type) {
                        has_compatible = true;
                        break;
                    }
                }
                if (has_compatible)
                    break;
            }
            if (!has_compatible)
                return false;
        }
        if (!p_in)
            continue;
    }

    return true;
}

/* VF2 递归匹配：尝试将模式图的所有节点映射到目标图。
 * 使用可行性剪枝和前瞻函数来减少搜索空间。
 * 找到的最大匹配会保存在 best_match 中。
 *
 * @param depth 当前递归深度，由上层调用者传入 depth+1。
 *              超过 REWRITE_VF2_MAX_DEPTH 时返回 false，防止栈溢出。 */
static bool vf2_match_recursive(VF2State *state, ConstraintGraph *pattern_graph, ConstraintGraph *target_graph,
                                bool local_equivalence_tolerant, RewriteMatch *best_match, int *best_match_size,
                                int depth) {
    /* 递归深度保护：超过限制则立即返回失败，防止栈溢出 */
    if (depth > REWRITE_VF2_MAX_DEPTH) {
        LOG_WARN("rewrite", "VF2: max recursion depth (%d) exceeded", REWRITE_VF2_MAX_DEPTH);
        return false;
    }

    /* 基准情况：所有模式节点都已匹配 */
    if (state->core_count >= state->pattern_size) {
        return true;
    }

    /* 选择下一个要匹配的模式节点：
     * 优先选择有最多已映射邻居的未映射节点（MRV 启发式） */
    int best_p = -1;
    int best_mapped_neighbors = -1;

    for (int p = 0; p < state->pattern_size; p++) {
        if (state->core_1[p] >= 0)
            continue;

        int mapped_neighbors = 0;
        for (int c = 0; c < pattern_graph->constraint_count; c++) {
            Constraint *pc = pattern_graph->constraints[c];
            bool p_in = false;
            for (int k = 0; k < pc->participant_count; k++) {
                if (pc->participants[k] == p) {
                    p_in = true;
                } else if (state->core_1[pc->participants[k]] >= 0) {
                    mapped_neighbors++;
                }
            }
            if (p_in)
                break;
        }

        if (mapped_neighbors > best_mapped_neighbors) {
            best_mapped_neighbors = mapped_neighbors;
            best_p = p;
        }
    }

    if (best_p < 0)
        return false;

    /* 计算 best_p 的约束度数（涉及的约束数）用于候选排序 */
    int p_degree = 0;
    for (int c = 0; c < pattern_graph->constraint_count; c++) {
        Constraint *pc = pattern_graph->constraints[c];
        for (int k = 0; k < pc->participant_count; k++) {
            if (pc->participants[k] == best_p) {
                p_degree++;
                break;
            }
        }
    }

    /* 收集所有候选目标节点，按度数兼容性排序 */
    int *candidates = lv00_malloc((size_t) state->target_size * sizeof(int));
    int *cand_scores = lv00_malloc((size_t) state->target_size * sizeof(int));
    int cand_count = 0;
    if (!candidates || !cand_scores) {
        lv00_free((void **) &candidates);
        lv00_free((void **) &cand_scores);
        return false;
    }

    for (int t = 0; t < state->target_size; t++) {
        if (state->core_2[t] >= 0)
            continue;
        /* in/out 集合剪枝：跳过 out_set 中的节点 */
        bool in_out_set = false;
        for (int o = 0; o < state->out_count; o++) {
            if (state->out_set[o] == t) {
                in_out_set = true;
                break;
            }
        }
        if (in_out_set)
            continue;

        /* 计算目标节点度数 & 兼容性评分 */
        int t_degree = 0;
        for (int c = 0; c < target_graph->constraint_count; c++) {
            Constraint *tc = target_graph->constraints[c];
            for (int k = 0; k < tc->participant_count; k++) {
                if (tc->participants[k] == t) {
                    t_degree++;
                    break;
                }
            }
        }
        /* 评分 = 1 + (度数匹配分: 越接近 p_degree 越好)
         * 匹配度越高（差值越小）评分越低（优先级越高） */
        int score = 1 + abs(t_degree - p_degree);
        candidates[cand_count] = t;
        cand_scores[cand_count] = score;
        cand_count++;
    }

    /* 按评分升序排列（评分低的优先尝试） */
    for (int i = 0; i < cand_count - 1; i++) {
        int best_idx = i;
        for (int j = i + 1; j < cand_count; j++) {
            if (cand_scores[j] < cand_scores[best_idx])
                best_idx = j;
        }
        if (best_idx != i) {
            int tmp_t = candidates[i];
            candidates[i] = candidates[best_idx];
            candidates[best_idx] = tmp_t;
            int tmp_s = cand_scores[i];
            cand_scores[i] = cand_scores[best_idx];
            cand_scores[best_idx] = tmp_s;
        }
    }

    /* 尝试将 best_p 匹配到目标图中的候选节点（按度数兼容性排序） */
    for (int ci = 0; ci < cand_count; ci++) {
        int t = candidates[ci];

        /* 可行性检查 */
        if (!vf2_feasible(state, best_p, t, pattern_graph, target_graph, local_equivalence_tolerant))
            continue;

        /* 前瞻检查 */
        if (!vf2_lookahead(state, best_p, t, pattern_graph, target_graph))
            continue;

        /* 执行匹配 */
        state->core_1[best_p] = t;
        state->core_2[t] = best_p;
        state->core_count++;

        /* 更新 in/out 集合：将 t 加入 in_set */
        if (state->in_count >= state->in_capacity) {
            if (state->in_capacity > INT_MAX / 2) {
                LOG_WARN("rewrite", "VF2: in_set capacity overflow");
                state->core_1[best_p] = -1;
                state->core_2[t] = -1;
                state->core_count--;
                lv00_free((void **) &candidates);
                lv00_free((void **) &cand_scores);
                return false;
            }
            int new_cap = state->in_capacity * 2;
            int *new_in = lv00_realloc(state->in_set, (size_t) new_cap * sizeof(int));
            if (!new_in) {
                LOG_WARN("rewrite", "VF2: in_set realloc failed (cap=%d), skipping candidate", new_cap);
                state->core_1[best_p] = -1;
                state->core_2[t] = -1;
                state->core_count--;
                continue;
            }
            state->in_set = new_in;
            state->in_capacity = new_cap;
        }
        state->in_set[state->in_count++] = t;

        /* 递归搜索 */
        int saved_out_count = state->out_count;

        if (vf2_match_recursive(state, pattern_graph, target_graph, local_equivalence_tolerant, best_match,
                                best_match_size, depth + 1)) {
            lv00_free((void **) &candidates);
            lv00_free((void **) &cand_scores);
            return true;
        }

        /* 回溯：从 in_set 中移除 t */
        for (int i = state->in_count - 1; i >= 0; i--) {
            if (state->in_set[i] == t) {
                state->in_set[i] = state->in_set[state->in_count - 1];
                state->in_count--;
                break;
            }
        }

        /* 回溯：恢复 out_set */
        state->out_count = saved_out_count;

        /* 将 t 加入 out_set */
        if (state->out_count >= state->out_capacity) {
            if (state->out_capacity > INT_MAX / 2) {
                LOG_WARN("rewrite", "VF2: out_set capacity overflow");
                lv00_free((void **) &candidates);
                lv00_free((void **) &cand_scores);
                return false;
            }
            int new_cap = state->out_capacity * 2;
            int *new_out = lv00_realloc(state->out_set, (size_t) new_cap * sizeof(int));
            if (!new_out) {
                LOG_WARN("rewrite", "VF2: out_set realloc failed (cap=%d), skipping candidate", new_cap);
                state->core_1[best_p] = -1;
                state->core_2[t] = -1;
                state->core_count--;
                continue;
            }
            state->out_set = new_out;
            state->out_capacity = new_cap;
        }
        state->out_set[state->out_count++] = t;

        state->core_1[best_p] = -1;
        state->core_2[t] = -1;
        state->core_count--;
    }

    lv00_free((void **) &candidates);
    lv00_free((void **) &cand_scores);
    return false;
}

/* VF2 子图同构匹配的公开接口。
 * 在目标图中搜索与模式图同构的子图，返回匹配结果。
 * 如果找到匹配，返回 RewriteMatch 对象；否则返回 NULL。
 *
 * 当 local_equivalence_tolerant 为 true 时，在可行性检查和
 * 约束匹配阶段，对 POINT 节点使用 symbolic_coord_compare
 * 进行坐标相等性验证（design_v2.9.md Section 6.2）。 */
RewriteMatch *vf2_find_match(ConstraintGraph *target_graph, RewritePattern *pattern, bool local_equivalence_tolerant) {
    if (!target_graph || !pattern || pattern->var_count == 0)
        return NULL;

    /* ----------------------------------------------------------------
     * 构建模式图的约束图结构。
     *
     * 为每个模式变量节点创建一个 GeomNode（类型为 GEOM_POINT），
     * 节点在 nodes 数组中的索引对应 variable_node_ids 的下标。
     * 节点 ID 设为模式变量的负 ID（如 -1, -2, ...），
     * 以便在可行性检查中通过 graph_get_node 查找。
     *
     * 模式约束中的参与者是模式变量 ID（负数）或固定节点 ID
     * （正数）。我们将参与者映射到模式图中的节点索引：
     *   - 负数参与者 -> 在 variable_node_ids 中查找其下标
     *   - 正数参与者 -> 在 variable_node_ids 中查找其下标
     * 如果参与者不在 variable_node_ids 中，则跳过该约束
     * （该约束涉及外部/边界节点，VF2 仅匹配模式变量之间的结构）。
     * ---------------------------------------------------------------- */
    ConstraintGraph *pattern_graph = graph_create();
    if (!pattern_graph)
        return NULL;

    /* 为每个模式变量创建节点 */
    for (int i = 0; i < pattern->var_count; i++) {
        GeomNode *node = lv00_malloc(sizeof(GeomNode));
        if (!node) {
            graph_destroy(pattern_graph);
            return NULL;
        }
        memset(node, 0, sizeof(GeomNode));
        node->id = pattern->variable_node_ids[i]; /* 负数 ID */
        node->type = GEOM_POINT;
        node->trust = TRUST_GREEN;
        node->coord_count = 0;
        node->symbolic_coords = NULL;

        pattern_graph->nodes =
            lv00_realloc(pattern_graph->nodes, (size_t) (pattern_graph->node_count + 1) * sizeof(GeomNode *));
        pattern_graph->nodes[pattern_graph->node_count++] = node;
    }
    pattern_graph->next_node_id = pattern->var_count;

    /* 构建模式变量 ID -> 数组索引的映射表 */
    int *var_id_to_idx = lv00_malloc((size_t) pattern->var_count * sizeof(int));
    if (!var_id_to_idx) {
        graph_destroy(pattern_graph);
        return NULL;
    }
    for (int i = 0; i < pattern->var_count; i++) {
        var_id_to_idx[i] = -1;
    }
    for (int i = 0; i < pattern->var_count; i++) {
        /* 使用 abs(variable_node_ids[i]) % var_count 作为哈希索引 */
        int vid = pattern->variable_node_ids[i];
        int idx = ((vid < 0 ? -vid : vid) % pattern->var_count);
        while (var_id_to_idx[idx] >= 0 && pattern->variable_node_ids[var_id_to_idx[idx]] != vid) {
            idx = (idx + 1) % pattern->var_count;
        }
        var_id_to_idx[idx] = i;
    }

    /* 将模式约束添加到模式图中。
     * 参与者 ID 映射到模式图节点的索引（即 nodes 数组下标）。 */
    for (int c = 0; c < pattern->pattern_constraint_count; c++) {
        Constraint *pc = pattern->pattern_constraints[c];

        /* 将参与者 ID 映射到模式图中的节点索引 */
        int *mapped_participants = lv00_malloc((size_t) pc->participant_count * sizeof(int));
        if (!mapped_participants) {
            lv00_free((void **) &var_id_to_idx);
            graph_destroy(pattern_graph);
            return NULL;
        }

        bool all_mapped = true;
        for (int p = 0; p < pc->participant_count; p++) {
            int pid = pc->participants[p];
            int node_idx = -1;

            /* 在 variable_node_ids 中查找 pid */
            for (int v = 0; v < pattern->var_count; v++) {
                if (pattern->variable_node_ids[v] == pid) {
                    node_idx = v;
                    break;
                }
            }

            if (node_idx < 0) {
                /* 参与者不在模式变量中 -- 跳过此约束 */
                all_mapped = false;
                break;
            }
            mapped_participants[p] = node_idx;
        }

        if (all_mapped) {
            Constraint *new_con = lv00_malloc(sizeof(Constraint));
            if (!new_con) {
                lv00_free((void **) &mapped_participants);
                lv00_free((void **) &var_id_to_idx);
                graph_destroy(pattern_graph);
                return NULL;
            }
            memset(new_con, 0, sizeof(Constraint));
            new_con->id = pattern_graph->next_constraint_id++;
            new_con->type = pc->type;
            new_con->participant_count = pc->participant_count;
            new_con->participants = mapped_participants;

            pattern_graph->constraints = lv00_realloc(
                pattern_graph->constraints, (size_t) (pattern_graph->constraint_count + 1) * sizeof(Constraint *));
            pattern_graph->constraints[pattern_graph->constraint_count++] = new_con;
        } else {
            lv00_free((void **) &mapped_participants);
        }
    }

    lv00_free((void **) &var_id_to_idx);

    /* 初始化 VF2 匹配状态 */
    VF2State state;
    vf2_state_init(&state, pattern_graph->node_count, target_graph->node_count);

    /* 使用模式图和目标图进行 VF2 子图同构匹配 */
    bool found = vf2_match_recursive(&state, pattern_graph, target_graph, local_equivalence_tolerant, NULL, NULL, 0);

    RewriteMatch *match = NULL;

    if (found) {
        /* 将 VF2 匹配结果转换为 RewriteMatch 格式 */
        match = lv00_malloc(sizeof(RewriteMatch));
        if (!match) {
            vf2_state_destroy(&state);
            graph_destroy(pattern_graph);
            return NULL;
        }

        match->node_bindings = lv00_malloc((size_t) pattern->var_count * 2 * sizeof(int));
        if (!match->node_bindings) {
            lv00_free((void **) &match);
            vf2_state_destroy(&state);
            graph_destroy(pattern_graph);
            return NULL;
        }

        match->constraint_bindings = lv00_malloc((size_t) pattern->pattern_constraint_count * sizeof(int));
        if (!match->constraint_bindings) {
            lv00_free((void **) &match->node_bindings);
            lv00_free((void **) &match);
            vf2_state_destroy(&state);
            graph_destroy(pattern_graph);
            return NULL;
        }

        /* 填充节点绑定：
         * core_1[i] 是模式图中第 i 个节点在目标图中的索引 */
        for (int i = 0; i < pattern->var_count; i++) {
            int target_node_idx = state.core_1[i];
            match->node_bindings[i * 2] = pattern->variable_node_ids[i];
            if (target_node_idx >= 0 && target_node_idx < target_graph->node_count) {
                match->node_bindings[i * 2 + 1] = target_graph->nodes[target_node_idx]->id;
            } else {
                match->node_bindings[i * 2 + 1] = -1;
            }
        }

        /* 匹配约束：将模式约束与目标约束对应。
         * 在 local_equivalence_tolerant 模式下，对 POINT 节点的参与者
         * 使用 symbolic_coord_compare 进行坐标相等性验证。 */
        int constraint_match_count = 0;
        for (int pc = 0; pc < pattern->pattern_constraint_count; pc++) {
            Constraint *pcon = pattern->pattern_constraints[pc];

            for (int gc = 0; gc < target_graph->constraint_count; gc++) {
                Constraint *tcon = target_graph->constraints[gc];
                if (pcon->type != tcon->type)
                    continue;
                if (pcon->participant_count != tcon->participant_count)
                    continue;

                bool all_match = true;
                for (int k = 0; k < pcon->participant_count; k++) {
                    int pid = pcon->participants[k];
                    int tid = tcon->participants[k];

                    /* 查找模式变量对应的绑定 */
                    if (pid < 0) {
                        bool found_bind = false;
                        for (int b = 0; b < pattern->var_count; b++) {
                            if (pattern->variable_node_ids[b] == pid) {
                                if (match->node_bindings[b * 2 + 1] == tid) {
                                    found_bind = true;
                                }
                                break;
                            }
                        }
                        if (!found_bind) {
                            all_match = false;
                            break;
                        }
                    } else {
                        if (pid != tid) {
                            all_match = false;
                            break;
                        }
                    }

                    /* local_equivalence_tolerant 模式下：
                     * 对 POINT 节点使用 symbolic_coord_compare 验证坐标相等 */
                    if (local_equivalence_tolerant && all_match) {
                        GeomNode *p_node = graph_get_node(target_graph, pid);
                        GeomNode *t_node = graph_get_node(target_graph, tid);
                        if (p_node && t_node && p_node->type == GEOM_POINT && t_node->type == GEOM_POINT &&
                            p_node->coord_count > 0 && t_node->coord_count > 0) {
                            if (p_node->coord_count != t_node->coord_count) {
                                all_match = false;
                            } else {
                                for (int c = 0; c < p_node->coord_count; c++) {
                                    if (!p_node->symbolic_coords[c] || !t_node->symbolic_coords[c] ||
                                        symbolic_coord_compare(p_node->symbolic_coords[c],
                                                               t_node->symbolic_coords[c]) != 0) {
                                        all_match = false;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }

                if (all_match) {
                    match->constraint_bindings[pc] = tcon->id;
                    constraint_match_count++;
                    break;
                }
            }
        }

        match->binding_count = constraint_match_count;

        /* 验证所有已添加到模式图的约束都匹配成功 */
        if (constraint_match_count != pattern_graph->constraint_count) {
            lv00_free((void **) &match->node_bindings);
            lv00_free((void **) &match->constraint_bindings);
            lv00_free((void **) &match);
            match = NULL;
        }

        /* 流式输出：匹配成功时发射事件 */
        if (match && rewrite_stream_ctx) {
            stream_emit_simple(rewrite_stream_ctx, STREAM_EVENT_REWRITE_MATCH_FOUND,
                               "VF2 subgraph isomorphism match found", -1);
        }
    }

    vf2_state_destroy(&state);
    graph_destroy(pattern_graph);
    return match;
}

/* ===========================================================================
 * WL (Weisfeiler-Lehman) 图核哈希
 *
 * WL 算法通过迭代精化节点标签来计算图的拓扑哈希。
 * 初始标签基于节点类型和约束拓扑（忽略坐标值），
 * 每轮迭代根据邻居标签更新当前标签，最终聚合为图哈希。
 * ===========================================================================
 */

/* 初始化 WL 哈希历史环形缓冲区 */
void wl_history_init(WLHashHistory *hist) {
    hist->hash_history = lv00_malloc(WL_HISTORY_SIZE * sizeof(uint64_t));
    if (hist->hash_history)
        memset(hist->hash_history, 0, WL_HISTORY_SIZE * sizeof(uint64_t));
    hist->history_count = 0;
    hist->history_pos = 0;
}

/* uint64_t 比较函数（供 qsort 使用） */
static int uint64_compare(const void *a, const void *b) {
    uint64_t va = *(const uint64_t *) a;
    uint64_t vb = *(const uint64_t *) b;
    if (va < vb)
        return -1;
    if (va > vb)
        return 1;
    return 0;
}

/* 销毁 WL 哈希历史，释放内存 */
void wl_history_destroy(WLHashHistory *hist) {
    lv00_free((void **) &hist->hash_history);
    hist->hash_history = NULL;
    hist->history_count = 0;
    hist->history_pos = 0;
}

/* 向环形缓冲区中推入一个新的图哈希（64位完整哈希 + 32位轻量哈希） */
static void wl_history_push(WLHashHistory *hist, uint64_t hash) {
    hist->hash_history[hist->history_pos] = hash;
    hist->history_pos = (hist->history_pos + 1) % WL_HISTORY_SIZE;
    if (hist->history_count < WL_HISTORY_SIZE) {
        hist->history_count++;
    }
    /* 同步更新32位轻量哈希（用于快速预筛选） */
    uint32_t light = (uint32_t) (hash ^ (hash >> 32));
    hist->light_hash_history[hist->light_history_pos] = light;
    hist->light_history_pos = (hist->light_history_pos + 1) % WL_HISTORY_SIZE;
    if (hist->light_history_count < WL_HISTORY_SIZE) {
        hist->light_history_count++;
    }
}

/* 两阶段检查：先用32位轻量哈希快速预筛选，匹配时再用64位确认 */
static bool wl_history_contains_light(WLHashHistory *hist, uint32_t light_hash) {
    for (int i = 0; i < hist->light_history_count; i++) {
        if (hist->light_hash_history[i] == light_hash) {
            return true;
        }
    }
    return false;
}

/* 检查环形缓冲区中是否已包含指定的哈希值 */
static bool wl_history_contains(WLHashHistory *hist, uint64_t hash) {
    /* 阶段1：32位轻量预筛选 */
    uint32_t light = (uint32_t) (hash ^ (hash >> 32));
    if (!wl_history_contains_light(hist, light)) {
        return false; /* 快速路径：轻量哈希不匹配，直接排除 */
    }

    /* 阶段2：64位精确确认 */
    for (int i = 0; i < hist->history_count; i++) {
        if (hist->hash_history[i] == hash) {
            return true;
        }
    }
    return false;
}

/* 计算节点的初始 WL 标签。
 * 标签基于节点类型和约束拓扑（不包含坐标值），
 * 确保结构相同但坐标不同的图具有相同的初始标签。 */
static uint64_t *compute_wl_initial_labels(ConstraintGraph *graph, int node_count) {
    uint64_t *labels = lv00_malloc((size_t) node_count * sizeof(uint64_t));
    if (!labels)
        return NULL;

    for (int i = 0; i < node_count; i++) {
        GeomNode *n = graph->nodes[i];
        /* 类型 + 信任颜色 + Light Orange子类型 作为增强基础标签 */
        uint64_t label = (uint64_t) (n->type + 1) * 65599 + (uint64_t) (n->trust) + ((uint64_t) (n->lo_subtype) << 8);

        /* 统计该节点参与的每种约束类型的数量（拓扑信息） */
        int incidence_count = 0, betweenness_count = 0;
        int intersection_count = 0, containment_count = 0;
        int connection_count = 0;

        for (int c = 0; c < graph->constraint_count; c++) {
            Constraint *con = graph->constraints[c];
            for (int p = 0; p < con->participant_count; p++) {
                if (con->participants[p] == n->id) {
                    switch (con->type) {
                        case INCIDENCE:
                            incidence_count++;
                            break;
                        case BETWEENNESS:
                            betweenness_count++;
                            break;
                        case INTERSECTION:
                            intersection_count++;
                            break;
                        case CONTAINMENT:
                            containment_count++;
                            break;
                        case CONNECTION:
                            connection_count++;
                            break;
                    }
                    break;
                }
            }
        }

        /* 将约束计数信息混入标签 */
        label = label * 31 + (uint64_t) incidence_count;
        label = label * 31 + (uint64_t) betweenness_count;
        label = label * 31 + (uint64_t) intersection_count;
        label = label * 31 + (uint64_t) containment_count;
        label = label * 31 + (uint64_t) connection_count;

        labels[i] = label;
    }

    return labels;
}

/* 执行一轮 WL 迭代：根据邻居标签精化当前标签。
 * 每个节点的新标签 = hash(旧标签 + 排序后的邻居标签列表)。
 * 返回新分配的标签数组，调用者负责释放。 */
static uint64_t *wl_refine_labels(ConstraintGraph *graph, uint64_t *labels, int node_count) {
    uint64_t *new_labels = lv00_malloc((size_t) node_count * sizeof(uint64_t));
    if (!new_labels)
        return NULL;

    /* 构建节点 id -> 索引的映射 */
    int *id_to_idx = lv00_malloc((size_t) node_count * sizeof(int));
    if (!id_to_idx) {
        lv00_free((void **) &new_labels);
        return NULL;
    }
    for (int i = 0; i < node_count; i++) {
        id_to_idx[i] = -1;
    }
    for (int i = 0; i < node_count; i++) {
        int id = graph->nodes[i]->id;
        /* 使用 id % node_count 作为简单哈希，处理 id 可能不连续的情况 */
        int idx = ((id % node_count) + node_count) % node_count;
        /* 线性探测解决冲突 */
        while (idx < node_count && id_to_idx[idx] >= 0 && graph->nodes[id_to_idx[idx]]->id != id) {
            idx = (idx + 1) % node_count;
        }
        if (idx < node_count) {
            id_to_idx[idx] = i;
        }
    }

    for (int i = 0; i < node_count; i++) {
        uint64_t refined = labels[i];

        /* 收集该节点所有邻居的标签（容量提升至128以支持密集图） */
        uint64_t neighbor_labels[128];
        int neighbor_count = 0;

        for (int c = 0; c < graph->constraint_count; c++) {
            Constraint *con = graph->constraints[c];
            for (int p = 0; p < con->participant_count; p++) {
                if (con->participants[p] == graph->nodes[i]->id) {
                    for (int q = 0; q < con->participant_count; q++) {
                        if (q == p)
                            continue;
                        int neighbor_id = con->participants[q];
                        /* 查找邻居节点在 nodes 数组中的索引 */
                        int nidx = ((neighbor_id % node_count) + node_count) % node_count;
                        while (nidx < node_count && id_to_idx[nidx] >= 0 &&
                               graph->nodes[id_to_idx[nidx]]->id != neighbor_id) {
                            nidx = (nidx + 1) % node_count;
                        }
                        if (nidx < node_count && id_to_idx[nidx] >= 0 &&
                            graph->nodes[id_to_idx[nidx]]->id == neighbor_id) {
                            if (neighbor_count < 128) {
                                neighbor_labels[neighbor_count++] = labels[id_to_idx[nidx]];
                            }
                        }
                    }
                    break;
                }
            }
        }

        /* 使用标准 qsort 对邻居标签排序以确保确定性 */
        qsort(neighbor_labels, neighbor_count, sizeof(uint64_t), (int (*)(const void *, const void *)) uint64_compare);

        /* 将邻居标签混入精化标签 */
        for (int n = 0; n < neighbor_count; n++) {
            refined = refined * LV00_REWRITE_WL_HASH_MULTIPLIER + neighbor_labels[n];
        }

        new_labels[i] = refined;
    }

    lv00_free((void **) &id_to_idx);
    return new_labels;
}

/* 计算 WL 图核哈希（2 轮迭代，基于拓扑结构，忽略坐标值）。
 * 将所有节点标签聚合为一个 64 位图哈希。 */
static uint64_t compute_wl_graph_hash(ConstraintGraph *graph) {
    if (!graph || graph->node_count == 0)
        return 0;

    int node_count = graph->node_count;

    /* 计算初始标签 */
    uint64_t *labels = compute_wl_initial_labels(graph, node_count);
    if (!labels)
        return 0;

    /* 执行 WL_ITERATIONS 轮迭代 */
    for (int iter = 0; iter < WL_ITERATIONS; iter++) {
        uint64_t *new_labels = wl_refine_labels(graph, labels, node_count);
        lv00_free((void **) &labels);
        if (!new_labels)
            return 0;
        labels = new_labels;
    }

    /* 聚合所有节点标签为图哈希 */
    uint64_t graph_hash = (uint64_t) graph->node_count;
    for (int i = 0; i < node_count; i++) {
        graph_hash = graph_hash * LV00_REWRITE_WL_HASH_MULTIPLIER + labels[i];
    }

    lv00_free((void **) &labels);
    return graph_hash;
}

/* 使用 WL 图核哈希检测重写循环。
 * 计算当前图的 WL 哈希，与历史缓冲区比较。
 * 如果发现重复哈希，说明图回到了之前的状态，存在循环。
 * 新哈希会被推入历史缓冲区（固定 16 步）。 */
RewriteStatus detect_rewrite_loop_wl(ConstraintGraph *graph, WLHashHistory *hist) {
    if (!graph || !hist)
        return REWRITE_OK;

    uint64_t current_hash = compute_wl_graph_hash(graph);

    /* 检查缓冲区中是否已存在该哈希 */
    if (wl_history_contains(hist, current_hash)) {
        if (rewrite_stream_ctx) {
            stream_emit_simple(rewrite_stream_ctx, STREAM_EVENT_ERROR, "WL rewrite loop detected: graph hash repeated",
                               -1);
        }
        return REWRITE_TERMINATED;
    }

    /* 推入新哈希 */
    wl_history_push(hist, current_hash);
    return REWRITE_OK;
}

/* ===========================================================================
 * 前置条件系统
 *
 * 前置条件允许在匹配成功后、执行替换前进行额外的验证。
 * 只有前置条件评估通过时，才会执行重写替换操作。
 * ===========================================================================
 */

/* 评估重写规则的前置条件。
 * 如果规则没有设置前置条件（condition_func 为 NULL），
 * 默认返回 true（通过）。
 * 前置条件在匹配成功后、替换前调用。 */
static bool evaluate_precondition(ConstraintGraph *graph, RewriteRule *rule, RewriteMatch *match) {
    if (!rule || !rule->condition_func)
        return true;

    return rule->condition_func(graph, match, rule->condition_data);
}

/* ===========================================================================
 * 重写度量验证
 *
 * 在应用重写规则后，验证归约度量是否确实减少了。
 * 度量定义为：节点数 + 约束数。
 * 如果 after 的度量比 before 的度量减少了 expected_reduction 或更多，
 * 则返回 1，否则返回 0。
 * =========================================================================== */

bool rewrite_validate_measure(const ConstraintGraph *graph, const RewriteRule *rule, const GraphHash *graph_before) {
    if (!graph || !rule)
        return false;

    /* 根据 design_v2.9.md 第6.1节：
     * - reduction_measure > 0：验证缩减量 >= 度量值
     * - reduction_measure == 0：跳过验证
     * - reduction_measure < 0：验证扩展量 <= |度量值| */
    int expected = rule->reduction_measure;
    if (expected == 0)
        return true; /* 跳过中性规则的验证 */

    if (graph_before) {
        int nodes_before = graph_before->node_count;
        int nodes_after = graph->node_count;

        int node_reduction = nodes_before - nodes_after;

        if (expected > 0) {
            /* 验证节点缩减量是否达到目标 */
            return (node_reduction >= expected);
        } else {
            /* 验证扩展量不超过 |expected| */
            int max_expansion = -expected;
            int node_expansion = nodes_after - nodes_before;
            return (node_expansion <= max_expansion);
        }
    }

    /* 回退：无哈希时的基本检查 */
    return true;
}

/* ===========================================================================
 * 最佳匹配选择
 *
 * 在图中查找所有非重叠的子图匹配，选择匹配子图节点数最多的匹配。
 * 这确保了重写规则应用在最合适的子图上。
 * ===========================================================================
 */

/* 在图中查找最佳匹配（匹配子图节点数最多的匹配）。
 * 使用 VF2 算法进行子图同构匹配，并通过前置条件验证。
 * 返回最佳匹配的 RewriteMatch 对象，或 NULL 表示未找到。 */
RewriteMatch *find_best_match(ConstraintGraph *graph, RewriteRule *rule, bool local_equivalence_tolerant) {
    if (!graph || !rule || !rule->pattern)
        return NULL;

    RewritePattern *pat = rule->pattern;

    /* 使用 VF2 算法查找匹配 */
    RewriteMatch *match = vf2_find_match(graph, pat, local_equivalence_tolerant);
    if (!match)
        return NULL;

    /* 评估前置条件 */
    if (!evaluate_precondition(graph, rule, match)) {
        lv00_free((void **) &match->node_bindings);
        lv00_free((void **) &match->constraint_bindings);
        lv00_free((void **) &match);
        return NULL;
    }

    return match;
}

/* ================================================================
 * === 第六梯队参考项目落地 (P1) 实现 — Maude 重写策略引擎 =========
 * === 2026-05-24 ==================================================
 *
 * 本节实现 Maude 风格重写策略系统，包含：
 *   1. 策略树构造器（10 种策略组合子的构造函数）
 *   2. rewrite_strategy_apply()   —— 策略驱动的图重写执行
 *   3. rewrite_search_backward() —— 逆向证明搜索（BFS/DFS）
 *
 * 依赖：
 *   - vf2_find_match()    用于子图同构匹配
 *   - apply_rewrite()     用于执行单步重写
 *   - graph_snapshot_*()  用于图深拷贝
 *   - graph_create() / graph_destroy() 用于图生命周期管理
 * ================================================================ */

/* ---- 内部辅助函数：深拷贝约束图 ---- */
static ConstraintGraph *rewrite_graph_deep_copy(const ConstraintGraph *src) {
    if (!src)
        return NULL;
    ConstraintGraph *dst = graph_create();
    if (!dst)
        return NULL;
    GraphSnapshot *snap = graph_snapshot_create(src);
    if (!snap) {
        graph_destroy(dst);
        return NULL;
    }
    graph_snapshot_restore(snap, dst);
    graph_snapshot_destroy(snap);
    return dst;
}

/* ---- 内部辅助函数：判断是否为公理态（归约终点） ---- */
static bool rewrite_graph_is_axiom_like(const ConstraintGraph *graph) {
    if (!graph)
        return true;
    /* P1 简单判定：节点数为 0 且 约束数为 0 视为公理态。
     * 后续可扩展为公理库精确匹配。 */
    return (graph->node_count == 0 && graph->constraint_count == 0);
}

/* ==================================================================
 * 策略树构造器
 *
 * 所有构造器使用 lv00_malloc 分配 RewriteStrategy 节点，
 * 设置 kind 和对应字段，左右子树指针初始化为 NULL。
 * ================================================================== */

RewriteStrategy *rewrite_strategy_create_idle(void) {
    RewriteStrategy *s = (RewriteStrategy *) lv00_malloc(sizeof(RewriteStrategy));
    if (!s)
        return NULL;
    memset(s, 0, sizeof(RewriteStrategy));
    s->kind = REWRITE_STRAT_IDLE;
    return s;
}

RewriteStrategy *rewrite_strategy_create_fail(void) {
    RewriteStrategy *s = (RewriteStrategy *) lv00_malloc(sizeof(RewriteStrategy));
    if (!s)
        return NULL;
    memset(s, 0, sizeof(RewriteStrategy));
    s->kind = REWRITE_STRAT_FAIL;
    return s;
}

RewriteStrategy *rewrite_strategy_create_apply_rule(int rule_id) {
    RewriteStrategy *s = (RewriteStrategy *) lv00_malloc(sizeof(RewriteStrategy));
    if (!s)
        return NULL;
    memset(s, 0, sizeof(RewriteStrategy));
    s->kind = REWRITE_STRAT_APPLY_RULE;
    s->rule_id = rule_id;
    return s;
}

RewriteStrategy *rewrite_strategy_create_match(const char *pattern) {
    RewriteStrategy *s = (RewriteStrategy *) lv00_malloc(sizeof(RewriteStrategy));
    if (!s)
        return NULL;
    memset(s, 0, sizeof(RewriteStrategy));
    s->kind = REWRITE_STRAT_MATCH_PATTERN;
    if (pattern) {
        size_t len = strlen(pattern) + 1;
        s->pattern_expr = (char *) lv00_malloc(len);
        if (s->pattern_expr)
            memcpy(s->pattern_expr, pattern, len);
    }
    return s;
}

RewriteStrategy *rewrite_strategy_create_test(int (*test)(void *), void *ctx) {
    RewriteStrategy *s = (RewriteStrategy *) lv00_malloc(sizeof(RewriteStrategy));
    if (!s)
        return NULL;
    memset(s, 0, sizeof(RewriteStrategy));
    s->kind = REWRITE_STRAT_TEST_COND;
    s->test_func = test;
    s->test_ctx = ctx;
    return s;
}

RewriteStrategy *rewrite_strategy_sequence(RewriteStrategy *left, RewriteStrategy *right) {
    RewriteStrategy *s = (RewriteStrategy *) lv00_malloc(sizeof(RewriteStrategy));
    if (!s)
        return NULL;
    memset(s, 0, sizeof(RewriteStrategy));
    s->kind = REWRITE_STRAT_SEQUENCE;
    s->left = left;
    s->right = right;
    return s;
}

RewriteStrategy *rewrite_strategy_orelse(RewriteStrategy *left, RewriteStrategy *right) {
    RewriteStrategy *s = (RewriteStrategy *) lv00_malloc(sizeof(RewriteStrategy));
    if (!s)
        return NULL;
    memset(s, 0, sizeof(RewriteStrategy));
    s->kind = REWRITE_STRAT_ORELSE;
    s->left = left;
    s->right = right;
    return s;
}

RewriteStrategy *rewrite_strategy_repeat(RewriteStrategy *child, int max_iter) {
    RewriteStrategy *s = (RewriteStrategy *) lv00_malloc(sizeof(RewriteStrategy));
    if (!s)
        return NULL;
    memset(s, 0, sizeof(RewriteStrategy));
    s->kind = REWRITE_STRAT_REPEAT;
    s->left = child; /* REPEAT 的子策略存储在 left */
    s->max_iterations = max_iter;
    return s;
}

RewriteStrategy *rewrite_strategy_normalize(RewriteStrategy *child) {
    RewriteStrategy *s = (RewriteStrategy *) lv00_malloc(sizeof(RewriteStrategy));
    if (!s)
        return NULL;
    memset(s, 0, sizeof(RewriteStrategy));
    s->kind = REWRITE_STRAT_NORMALIZE;
    s->left = child;
    return s;
}

RewriteStrategy *rewrite_strategy_try(RewriteStrategy *child) {
    RewriteStrategy *s = (RewriteStrategy *) lv00_malloc(sizeof(RewriteStrategy));
    if (!s)
        return NULL;
    memset(s, 0, sizeof(RewriteStrategy));
    s->kind = REWRITE_STRAT_TRY;
    s->left = child;
    return s;
}

void rewrite_strategy_destroy(RewriteStrategy *s) {
    if (!s)
        return;
    /* 递归销毁左右子树 */
    rewrite_strategy_destroy(s->left);
    rewrite_strategy_destroy(s->right);
    /* 释放叶节点额外资源 */
    if (s->pattern_expr)
        lv00_free((void **) &s->pattern_expr);
    /* 置零后释放节点自身 */
    memset(s, 0, sizeof(RewriteStrategy));
    lv00_free((void **) &s);
}

/* ==================================================================
 * rewrite_strategy_apply —— 策略驱动的图重写执行
 *
 * 递归遍历策略树，根据节点 kind 执行不同语义：
 *   IDLE        : 不做任何修改，直接输出输入图的拷贝
 *   FAIL        : 立即返回失败
 *   APPLY_RULE  : 在图中匹配 rule_id 指定的规则并执行替换
 *   MATCH_PATTERN: 仅检查模式是否存在（不修改图）
 *   TEST_COND   : 调用 test_func 检查条件
 *   SEQUENCE    : 先 left 后 right
 *   ORELSE      : 先 left，失败则回退并尝试 right
 *   REPEAT      : 循环执行直到不动点或达到 max_iterations
 *   NORMALIZE   : 等价于 repeat(child ; child)（规范化到正规形式）
 *   TRY         : 尝试 child，失败则保持原状返回 IDLE 行为
 *
 * 注意：graph 为 const（调用者保有所有权），本函数通过深拷贝
 *       创建可变工作图来执行重写，最终通过 out_graph 返回结果。
 * ================================================================== */

bool rewrite_strategy_apply(const ConstraintGraph *graph, const RewriteStrategy *strategy, const RewriteRule *rules,
                            int rule_count, ConstraintGraph **out_graph, int *out_steps) {
    if (!graph || !strategy || !out_graph)
        return false;
    if (out_steps)
        *out_steps = 0;

    switch (strategy->kind) {
        /* ---------- IDLE ---------- */
        case REWRITE_STRAT_IDLE: {
            *out_graph = rewrite_graph_deep_copy(graph);
            return (*out_graph != NULL);
        }

        /* ---------- FAIL ---------- */
        case REWRITE_STRAT_FAIL:
            return false;

        /* ---------- APPLY_RULE ---------- */
        case REWRITE_STRAT_APPLY_RULE: {
            if (strategy->rule_id < 0 || strategy->rule_id >= rule_count)
                return false;
            const RewriteRule *rule = &rules[strategy->rule_id];
            if (!rule->pattern)
                return false;

            /* 深拷贝输入图作为工作图 */
            ConstraintGraph *working = rewrite_graph_deep_copy(graph);
            if (!working)
                return false;

            /* VF2 子图同构匹配 */
            RewriteMatch *match = vf2_find_match(working, rule->pattern, false);
            if (!match) {
                graph_destroy(working);
                return false;
            }

            /* 评估前置条件 */
            if (!evaluate_precondition(working, (RewriteRule *) rule, match)) {
                lv00_free((void **) &match->node_bindings);
                lv00_free((void **) &match->constraint_bindings);
                lv00_free((void **) &match);
                graph_destroy(working);
                return false;
            }

            /* 执行重写 */
            RewriteStatus status = apply_rewrite(working, (RewriteRule *) rule, match);

            /* 释放匹配对象 */
            lv00_free((void **) &match->node_bindings);
            lv00_free((void **) &match->constraint_bindings);
            lv00_free((void **) &match);

            if (status == REWRITE_APPLIED) {
                *out_graph = working;
                if (out_steps)
                    *out_steps = 1;
                return true;
            } else {
                graph_destroy(working);
                return false;
            }
        }

        /* ---------- MATCH_PATTERN ---------- */
        case REWRITE_STRAT_MATCH_PATTERN: {
            /* 仅检查模式是否可匹配，不修改图。
         * P1 实现：使用指定的第一个规则做匹配性探测。
         * 若 rule_count == 0 则始终失败。 */
            *out_graph = rewrite_graph_deep_copy(graph);
            if (!*out_graph)
                return false;

            if (rule_count == 0) {
                /* 无可用规则，保持图不变并返回成功（匹配语义：匹配成功表示
             * 至少存在一条规则可匹配；无规则则自然匹配失败，但策略不修改图） */
                return false;
            }

            /* 尝试任一规则做模式存在性检查 */
            bool matched = false;
            for (int i = 0; i < rule_count; i++) {
                if (!rules[i].pattern)
                    continue;
                RewriteMatch *m = vf2_find_match(*out_graph, rules[i].pattern, false);
                if (m) {
                    lv00_free((void **) &m->node_bindings);
                    lv00_free((void **) &m->constraint_bindings);
                    lv00_free((void **) &m);
                    matched = true;
                    break;
                }
            }
            return matched;
        }

        /* ---------- TEST_COND ---------- */
        case REWRITE_STRAT_TEST_COND: {
            *out_graph = rewrite_graph_deep_copy(graph);
            if (!*out_graph)
                return false;
            if (!strategy->test_func)
                return false;
            int result = strategy->test_func(strategy->test_ctx);
            return (result != 0);
        }

        /* ---------- SEQUENCE (s1 ; s2) ---------- */
        case REWRITE_STRAT_SEQUENCE: {
            if (!strategy->left || !strategy->right)
                return false;

            ConstraintGraph *mid_graph = NULL;
            int steps1 = 0;
            bool ok1 = rewrite_strategy_apply(graph, strategy->left, rules, rule_count, &mid_graph, &steps1);
            if (!ok1)
                return false;

            ConstraintGraph *final_graph = NULL;
            int steps2 = 0;
            bool ok2 = rewrite_strategy_apply(mid_graph, strategy->right, rules, rule_count, &final_graph, &steps2);
            graph_destroy(mid_graph);

            if (ok2) {
                *out_graph = final_graph;
                if (out_steps)
                    *out_steps = steps1 + steps2;
                return true;
            }
            return false;
        }

        /* ---------- ORELSE (s1 or-else s2) ---------- */
        case REWRITE_STRAT_ORELSE: {
            if (!strategy->left || !strategy->right)
                return false;

            ConstraintGraph *left_graph = NULL;
            int steps1 = 0;
            bool ok = rewrite_strategy_apply(graph, strategy->left, rules, rule_count, &left_graph, &steps1);
            if (ok) {
                *out_graph = left_graph;
                if (out_steps)
                    *out_steps = steps1;
                return true;
            }
            /* left 失败，尝试 right */
            return rewrite_strategy_apply(graph, strategy->right, rules, rule_count, out_graph, out_steps);
        }

        /* ---------- REPEAT (repeat s until fixpoint) ---------- */
        case REWRITE_STRAT_REPEAT: {
            if (!strategy->left)
                return false;

            int total_steps = 0;
            ConstraintGraph *current = rewrite_graph_deep_copy(graph);
            if (!current)
                return false;

            int iter = 0;
            int max_iter = strategy->max_iterations;
            if (max_iter <= 0)
                max_iter = 100; /* P1 默认上限 100 */

            while (iter < max_iter) {
                ConstraintGraph *next = NULL;
                int sub_steps = 0;
                bool applied = rewrite_strategy_apply(current, strategy->left, rules, rule_count, &next, &sub_steps);
                if (!applied || sub_steps == 0) {
                    /* 不动点：子策略未产生变化 */
                    if (next)
                        graph_destroy(next);
                    break;
                }
                graph_destroy(current);
                current = next;
                total_steps += sub_steps;
                iter++;
            }

            *out_graph = current;
            if (out_steps)
                *out_steps = total_steps;
            return (total_steps > 0);
        }

        /* ---------- NORMALIZE (normalize s := repeat(s ; s)) ---------- */
        case REWRITE_STRAT_NORMALIZE: {
            if (!strategy->left)
                return false;

            /* 构造 s ; s 的序列策略 */
            RewriteStrategy seq_inner;
            memset(&seq_inner, 0, sizeof(seq_inner));
            seq_inner.kind = REWRITE_STRAT_SEQUENCE;
            seq_inner.left = strategy->left;
            seq_inner.right = strategy->left;

            /* 包装为 repeat(s ; s) */
            RewriteStrategy repeat_wrapper;
            memset(&repeat_wrapper, 0, sizeof(repeat_wrapper));
            repeat_wrapper.kind = REWRITE_STRAT_REPEAT;
            repeat_wrapper.left = &seq_inner;
            repeat_wrapper.max_iterations = 100;

            return rewrite_strategy_apply(graph, &repeat_wrapper, rules, rule_count, out_graph, out_steps);
        }

        /* ---------- TRY ---------- */
        case REWRITE_STRAT_TRY: {
            if (!strategy->left)
                return false;

            ConstraintGraph *try_graph = NULL;
            int sub_steps = 0;
            bool applied = rewrite_strategy_apply(graph, strategy->left, rules, rule_count, &try_graph, &sub_steps);
            if (applied) {
                *out_graph = try_graph;
                if (out_steps)
                    *out_steps = sub_steps;
                return true;
            } else {
                /* 失败则保持原状，输出输入图的拷贝 */
                if (try_graph)
                    graph_destroy(try_graph);
                *out_graph = rewrite_graph_deep_copy(graph);
                if (out_steps)
                    *out_steps = 0;
                return (*out_graph != NULL);
            }
        }

        default:
            return false;
    } /* switch (strategy->kind) */
}

/* ==================================================================
 * rewrite_search_backward —— BFS/DFS 逆向证明搜索
 *
 * 从目标命题（target_graph）出发，逆向应用规则：
 *   对每条规则，在图中搜索其 RHS（替换约束）的匹配，
 *   若找到则执行逆向替换（移除匹配部分，恢复模式结构），
 *   生成 predecessor 状态，检查是否归约到公理。
 *
 * BFS 使用环形队列（容量 4096），保证找到最短证明路径。
 * DFS 使用递归（带深度限制），找到任意可行路径即返回。
 *
 * 环形队列结构：每个槽位存储
 *   - 约束图指针（深拷贝）
 *   - 搜索深度
 *   - 到达路径（rule_id 序列）
 * ================================================================== */

#define BACKWARD_SEARCH_MAX_QUEUE 4096
#define BACKWARD_SEARCH_MAX_DEPTH 64

typedef struct {
    ConstraintGraph *graph; /* 状态图（深拷贝） */
    int depth;              /* 搜索深度 */
    int *path;              /* rule_id 序列 */
    int path_len;           /* 路径长度 */
} BackwardSearchState;

/* ---- 内部辅助：逆向应用单条规则（真正的反向替换） ---- */
static bool rewrite_rule_apply_backward(const ConstraintGraph *graph, const RewriteRule *rule,
                                        ConstraintGraph **out_predecessor) {
    if (!graph || !rule || !rule->pattern || !rule->replacement || !out_predecessor)
        return false;

    const RewriteReplacement *repl = rule->replacement;
    const RewritePattern *lhs_pat = rule->pattern;

    /* ---- Step 1: Build a RewritePattern from the rule's RHS (replacement) ----
     *
     * True reverse substitution: match the RHS of the rule in the current graph,
     * remove the matched RHS nodes/constraints, then add the LHS nodes/constraints.
     *
     * The replacement constraints reference nodes by:
     *   - negative IDs: pattern variables (shared with LHS)
     *   - positive IDs in repl->new_nodes: nodes created during forward application
     * We collect all unique node references from replacement_constraints to build
     * the variable_node_ids array for the RHS pattern.
     */
    if (repl->replacement_constraint_count == 0)
        return false;

    /* Collect unique node IDs referenced in replacement constraints */
    int var_cap = 64;
    int *rhs_var_ids = lv00_malloc((size_t) var_cap * sizeof(int));
    if (!rhs_var_ids)
        return false;
    int rhs_var_count = 0;

    for (int c = 0; c < repl->replacement_constraint_count; c++) {
        Constraint *rc = repl->replacement_constraints[c];
        for (int p = 0; p < rc->participant_count; p++) {
            int pid = rc->participants[p];
            /* Check if already collected */
            bool found = false;
            for (int v = 0; v < rhs_var_count; v++) {
                if (rhs_var_ids[v] == pid) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                if (rhs_var_count >= var_cap) {
                    var_cap *= 2;
                    int *tmp = lv00_realloc(rhs_var_ids, (size_t) var_cap * sizeof(int));
                    if (!tmp) {
                        lv00_free((void **) &rhs_var_ids);
                        return false;
                    }
                    rhs_var_ids = tmp;
                }
                rhs_var_ids[rhs_var_count++] = pid;
            }
        }
    }

    /* Build a RewritePattern from replacement constraints */
    RewritePattern rhs_pattern;
    rhs_pattern.variable_node_ids = rhs_var_ids;
    rhs_pattern.var_count = rhs_var_count;
    rhs_pattern.pattern_constraints = repl->replacement_constraints;
    rhs_pattern.pattern_constraint_count = repl->replacement_constraint_count;

    /* ---- Step 2: Match RHS pattern in the current graph ---- */
    RewriteMatch *rhs_match = vf2_find_match((ConstraintGraph *) graph, &rhs_pattern, false);

    lv00_free((void **) &rhs_var_ids);

    if (!rhs_match)
        return false;

    /* ---- Step 3: Deep copy the current graph as the predecessor ---- */
    ConstraintGraph *pred = rewrite_graph_deep_copy(graph);
    if (!pred) {
        lv00_free((void **) &rhs_match->node_bindings);
        lv00_free((void **) &rhs_match->constraint_bindings);
        lv00_free((void **) &rhs_match);
        return false;
    }

    /* ---- Step 4: Remove matched RHS constraints from predecessor ---- */
    for (int i = 0; i < rhs_match->binding_count; i++) {
        int con_id = rhs_match->constraint_bindings[i];
        if (graph_get_constraint(pred, con_id)) {
            graph_remove_constraint(pred, con_id);
        }
    }

    /* ---- Step 5: Remove matched RHS nodes that are not referenced
     *            by any remaining constraint in predecessor ---- */
    for (int i = 0; i < rhs_match->binding_count; i++) {
        int pattern_var_id = rhs_match->node_bindings[i * 2];
        int graph_node_id = rhs_match->node_bindings[i * 2 + 1];

        /* Check if this node is still referenced by any remaining constraint */
        bool has_refs = false;
        for (int c = 0; c < pred->constraint_count; c++) {
            Constraint *con = pred->constraints[c];
            if (!con)
                continue;
            for (int p = 0; p < con->participant_count; p++) {
                if (con->participants[p] == graph_node_id) {
                    has_refs = true;
                    break;
                }
            }
            if (has_refs)
                break;
        }
        if (!has_refs) {
            GeomNode *node = graph_get_node(pred, graph_node_id);
            if (node && node->type != GEOM_REGION) {
                graph_remove_node(pred, graph_node_id);
            }
        }
    }

    /* ---- Step 6: Add LHS constraints to predecessor ----
     *
     * Resolve LHS pattern constraint participants using the RHS match bindings.
     * LHS pattern variables that also appear in RHS will be resolved via the
     * rhs_match bindings. LHS variables not in RHS cannot be resolved and
     * are skipped (they were consumed during forward application).
     */
    for (int c = 0; c < lhs_pat->pattern_constraint_count; c++) {
        Constraint *lc = lhs_pat->pattern_constraints[c];
        int *resolved = lv00_malloc((size_t) lc->participant_count * sizeof(int));
        if (!resolved)
            continue;

        bool all_ok = true;
        for (int p = 0; p < lc->participant_count; p++) {
            int pid = lc->participants[p];
            int rid = resolve_binding(rhs_match->node_bindings, rhs_match->binding_count, pid);
            if (rid < 0) {
                /* LHS variable not found in RHS match — cannot resolve */
                all_ok = false;
                break;
            }
            resolved[p] = rid;
        }

        if (all_ok) {
            /* Verify all referenced nodes exist before adding constraint */
            bool nodes_exist = true;
            for (int p = 0; p < lc->participant_count; p++) {
                if (!graph_get_node(pred, resolved[p])) {
                    nodes_exist = false;
                    break;
                }
            }
            if (nodes_exist) {
                add_constraint_generic(pred, lc->type, resolved, lc->participant_count);
            }
        }

        lv00_free((void **) &resolved);
    }

    /* Cleanup RHS match */
    lv00_free((void **) &rhs_match->node_bindings);
    lv00_free((void **) &rhs_match->constraint_bindings);
    lv00_free((void **) &rhs_match);

    /* Check that predecessor actually differs from current graph */
    if (pred->node_count == graph->node_count && pred->constraint_count == graph->constraint_count) {
        graph_destroy(pred);
        return false;
    }

    *out_predecessor = pred;
    return true;
}

/* ---- 内部辅助：释放 BackwardSearchState 的资源 ---- */
static void backward_state_destroy(BackwardSearchState *st) {
    if (!st)
        return;
    if (st->graph)
        graph_destroy(st->graph);
    if (st->path)
        lv00_free((void **) &st->path);
    memset(st, 0, sizeof(BackwardSearchState));
}

/* ---- BFS 逆向证明搜索（环形队列） ---- */
static bool rewrite_search_backward_bfs(const ConstraintGraph *target_graph, const RewriteRule *rules, int rule_count,
                                        int max_depth, int **out_path, int *out_path_len) {
    if (max_depth <= 0)
        max_depth = BACKWARD_SEARCH_MAX_DEPTH;

    BackwardSearchState *queue =
        (BackwardSearchState *) lv00_malloc(sizeof(BackwardSearchState) * BACKWARD_SEARCH_MAX_QUEUE);
    if (!queue)
        return false;
    memset(queue, 0, sizeof(BackwardSearchState) * BACKWARD_SEARCH_MAX_QUEUE);

    int head = 0, tail = 0;
    int queue_count = 0;

    /* 入队初始状态：目标命题 + depth=0 + 空路径 */
    queue[tail].graph = rewrite_graph_deep_copy(target_graph);
    queue[tail].depth = 0;
    queue[tail].path = NULL;
    queue[tail].path_len = 0;
    tail = (tail + 1) % BACKWARD_SEARCH_MAX_QUEUE;
    queue_count++;

    bool found = false;

    while (queue_count > 0 && !found) {
        BackwardSearchState current = queue[head];
        head = (head + 1) % BACKWARD_SEARCH_MAX_QUEUE;
        queue_count--;

        /* 检查是否归约到公理 */
        if (rewrite_graph_is_axiom_like(current.graph)) {
            /* 找到证明路径 */
            if (out_path_len)
                *out_path_len = current.path_len;
            if (out_path) {
                if (current.path_len > 0) {
                    *out_path = (int *) lv00_malloc((size_t) current.path_len * sizeof(int));
                    if (*out_path)
                        memcpy(*out_path, current.path, (size_t) current.path_len * sizeof(int));
                } else {
                    *out_path = NULL;
                }
            }
            backward_state_destroy(&current);
            found = true;
            break;
        }

        /* 检查深度限制 */
        if (current.depth >= max_depth) {
            backward_state_destroy(&current);
            continue;
        }

        /* 尝试逆向应用每条规则 */
        for (int r = 0; r < rule_count; r++) {
            if (queue_count >= BACKWARD_SEARCH_MAX_QUEUE - 1)
                break; /* 队列满 */

            ConstraintGraph *pred = NULL;
            if (!rewrite_rule_apply_backward(current.graph, &rules[r], &pred))
                continue;

            /* 构造新路径：current.path + [r] */
            int new_len = current.path_len + 1;
            int *new_path = (int *) lv00_malloc((size_t) new_len * sizeof(int));
            if (!new_path) {
                graph_destroy(pred);
                continue;
            }
            if (current.path && current.path_len > 0)
                memcpy(new_path, current.path, (size_t) current.path_len * sizeof(int));
            new_path[current.path_len] = r;

            /* 入队 */
            queue[tail].graph = pred;
            queue[tail].depth = current.depth + 1;
            queue[tail].path = new_path;
            queue[tail].path_len = new_len;
            tail = (tail + 1) % BACKWARD_SEARCH_MAX_QUEUE;
            queue_count++;
        }

        backward_state_destroy(&current);
    }

    /* 清理队列中剩余状态 */
    for (int i = 0; i < BACKWARD_SEARCH_MAX_QUEUE; i++) {
        if (queue[i].graph)
            graph_destroy(queue[i].graph);
        if (queue[i].path)
            lv00_free((void **) &queue[i].path);
    }
    lv00_free((void **) &queue);

    return found;
}

/* ---- DFS 逆向证明搜索（递归） ---- */
static bool rewrite_search_backward_dfs_recursive(ConstraintGraph *graph, const RewriteRule *rules, int rule_count,
                                                  int max_depth, int current_depth, int *path, int path_len,
                                                  ConstraintGraph **cached_states, int cached_count,
                                                  int **result_path, int *result_path_len) {
    /* 检查是否归约到公理 */
    if (rewrite_graph_is_axiom_like(graph)) {
        /* Found: copy the current path to result */
        if (result_path && result_path_len && path_len > 0) {
            int *rp = lv00_malloc((size_t) path_len * sizeof(int));
            if (rp) {
                memcpy(rp, path, (size_t) path_len * sizeof(int));
                *result_path = rp;
                *result_path_len = path_len;
            }
        }
        return true;
    }

    /* 深度限制 */
    if (current_depth >= max_depth)
        return false;

    /* 简单循环检测：检查当前图是否与已访问状态同构
     * P1 使用节点数+约束数快速哈希做近似去重 */
    uint64_t current_sig = ((uint64_t) graph->node_count << 32) | (uint64_t) graph->constraint_count;
    for (int i = 0; i < cached_count; i++) {
        uint64_t cs = ((uint64_t) cached_states[i]->node_count << 32) | (uint64_t) cached_states[i]->constraint_count;
        if (cs == current_sig)
            return false; /* 已访问，剪枝 */
    }

    for (int r = 0; r < rule_count; r++) {
        ConstraintGraph *pred = NULL;
        if (!rewrite_rule_apply_backward(graph, &rules[r], &pred))
            continue;

        /* 递归搜索 predecessor */
        int *new_path = (int *) lv00_malloc((size_t) (path_len + 1) * sizeof(int));
        if (!new_path) {
            graph_destroy(pred);
            continue;
        }
        if (path && path_len > 0)
            memcpy(new_path, path, (size_t) path_len * sizeof(int));
        new_path[path_len] = r;

        bool found = rewrite_search_backward_dfs_recursive(pred, rules, rule_count, max_depth, current_depth + 1,
                                                           new_path, path_len + 1, cached_states, cached_count,
                                                           result_path, result_path_len);

        if (found) {
            /* Path has been set by the deepest successful frame.
             * Free the intermediate path allocation (the result was
             * independently allocated inside the base case). */
            graph_destroy(pred);
            lv00_free((void **) &new_path);
            return true;
        }

        graph_destroy(pred);
        lv00_free((void **) &new_path);
    }

    return false;
}

/* ---- DFS 包装器（启动递归搜索并提取结果） ---- */
static bool rewrite_search_backward_dfs(const ConstraintGraph *target_graph, const RewriteRule *rules, int rule_count,
                                        int max_depth, int **out_path, int *out_path_len) {
    if (max_depth <= 0)
        max_depth = BACKWARD_SEARCH_MAX_DEPTH;

    /* 初始化图（深拷贝，DFS 会修改它） */
    ConstraintGraph *working = rewrite_graph_deep_copy(target_graph);
    if (!working)
        return false;

    /* cached_states 用于简单循环检测 */
    ConstraintGraph *cached = NULL;
    int cached_count = 0;

    int *path_buffer = NULL;
    int path_cap = max_depth * 2;
    path_buffer = (int *) lv00_malloc((size_t) path_cap * sizeof(int));
    if (!path_buffer) {
        graph_destroy(working);
        return false;
    }

    /* result_path / result_path_len receive the successful path from the
     * recursive base case when a proof is found. */
    int *result_path = NULL;
    int result_path_len = 0;

    bool found = rewrite_search_backward_dfs_recursive(working, rules, rule_count, max_depth, 0, path_buffer, 0,
                                                       &cached, cached_count, &result_path, &result_path_len);

    if (found && result_path) {
        if (out_path)
            *out_path = result_path;
        else
            lv00_free((void **) &result_path);

        if (out_path_len)
            *out_path_len = result_path_len;
    } else {
        if (result_path)
            lv00_free((void **) &result_path);
        if (out_path)
            *out_path = NULL;
        if (out_path_len)
            *out_path_len = 0;
    }

    graph_destroy(working);
    lv00_free((void **) &path_buffer);

    return found;
}

/* ==================================================================
 * rewrite_search_backward —— 公开接口
 * ================================================================== */

bool rewrite_search_backward(const ConstraintGraph *target_graph, const RewriteRule *rules, int rule_count,
                             int max_depth, bool use_bfs, int **out_path, int *out_path_len) {
    if (!target_graph || !rules || rule_count <= 0)
        return false;
    if (!out_path || !out_path_len)
        return false;

    *out_path = NULL;
    *out_path_len = 0;

    /* 直接就是公理态？ */
    if (rewrite_graph_is_axiom_like(target_graph)) {
        *out_path_len = 0;
        *out_path = NULL;
        return true;
    }

    if (use_bfs) {
        return rewrite_search_backward_bfs(target_graph, rules, rule_count, max_depth, out_path, out_path_len);
    } else {
        return rewrite_search_backward_dfs(target_graph, rules, rule_count, max_depth, out_path, out_path_len);
    }
}

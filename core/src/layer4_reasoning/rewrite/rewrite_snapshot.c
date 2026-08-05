/**
 * @file rewrite_snapshot.c
 * @brief 重写规则：图快照（事务回滚）
 *
 * 从 rewrite_match.c 拆分的模块之一（拆分清单见 rewrite_binding.c）。
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
#include "lv/node_deep_copy.h"
#include "lv/rewrite.h"

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "mpz_poly.h"
/* ---------------------------------------------------------------------------
 * Graph Snapshot — 用于重写替换操作的事务性回滚
 * ------------------------------------------------------------------------- */

/* ========================================================================
 * H4: Snapshot 操作 VTable — 替代 GeomType switch 语句
 * ======================================================================== */
typedef void (*SnapshotNodeDestroyFn)(GeomNode *node);
typedef void (*SnapshotNodeCleanupFn)(GeomNode *node);

typedef struct {
    SnapshotNodeDestroyFn destroy_data;
    SnapshotNodeCleanupFn cleanup_data;
} SnapshotNodeOps;

/* ---- 类型特定 handler 函数（节点深拷贝统一走 node_deep_copy_geom_node，
 *     原 copy_port_data/copy_region_data/copy_func_block_data 三套 handler
 *     与其 DataCopyHandler 完全同构，已收敛删除） ---- */

static void destroy_port_data(GeomNode *node) {
    lv_free((void **) &node->data.port);
}
static void cleanup_port_data(GeomNode *node) {
    lv_free((void **) &node->data.port);
}

static void destroy_region_data(GeomNode *node) {
    lv_free((void **) &node->data.region.boundary_segments);
}
static void cleanup_region_data(GeomNode *node) {
    lv_free((void **) &node->data.region.boundary_segments);
}

static void destroy_func_block_data(GeomNode *node) {
    lv_free((void **) &node->data.func_block.internal_nodes);
    lv_free((void **) &node->data.func_block.input_port_ids);
    lv_free((void **) &node->data.func_block.output_port_ids);
}
static void cleanup_func_block_data(GeomNode *node) {
    lv_free((void **) &node->data.func_block.internal_nodes);
    lv_free((void **) &node->data.func_block.input_port_ids);
    lv_free((void **) &node->data.func_block.output_port_ids);
}

/* ---- VTable 实例表 ---- */
static const SnapshotNodeOps kPointSnapshotOps =           {NULL, NULL};
static const SnapshotNodeOps kLineSegmentSnapshotOps =     {NULL, NULL};
static const SnapshotNodeOps kRegionSnapshotOps =           {destroy_region_data, cleanup_region_data};
static const SnapshotNodeOps kCircleSnapshotOps =           {NULL, NULL};
static const SnapshotNodeOps kPortSnapshotOps =             {destroy_port_data, cleanup_port_data};
static const SnapshotNodeOps kFuncBlockSnapshotOps =        {destroy_func_block_data, cleanup_func_block_data};

static const SnapshotNodeOps *kSnapshotNodeOpsTable[] = {
    [GEOM_POINT]          = &kPointSnapshotOps,
    [GEOM_LINE_SEGMENT]   = &kLineSegmentSnapshotOps,
    [GEOM_REGION]         = &kRegionSnapshotOps,
    [GEOM_CIRCLE]         = &kCircleSnapshotOps,
    [GEOM_PORT]           = &kPortSnapshotOps,
    [GEOM_FUNCTION_BLOCK] = &kFuncBlockSnapshotOps,
};

static const SnapshotNodeOps *get_snapshot_ops(GeomType type) {
    if (type >= 0 && (size_t)type < sizeof(kSnapshotNodeOpsTable)/sizeof(kSnapshotNodeOpsTable[0])) {
        return kSnapshotNodeOpsTable[type];
    }
    return NULL;
}

/* 深拷贝单个 GeomNode
 *
 * 统一委托给 node_deep_copy_geom_node()（原 SnapshotNodeOps 三套
 * copy_*_data handler 与其 DataCopyHandler 完全同构，已收敛删除）。
 * node_deep_copy 对坐标/字符串/类型特定数据执行深拷贝；端口 connected_to
 * 与区域/函数块内部引用置空/拷贝源引用后，在图快照恢复阶段通过 ID 映射重绑定。
 *
 * @param src 源节点（不修改）
 * @return 深拷贝的节点，失败返回 NULL（已分配资源已回滚）
 */
static GeomNode *graph_node_deep_copy(const GeomNode *src) {
    return node_deep_copy_geom_node(src, NULL);
}

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
        lv_free((void **) &node->symbolic_coords);
    }
    lv_free((void **) &node->numeric_assumption_declaration);
    /* 通过 VTable 分发类型特定数据销毁 */
    const SnapshotNodeOps *ops = get_snapshot_ops(node->type);
    if (ops && ops->destroy_data) {
        ops->destroy_data(node);
    }
    lv_free((void **) &node);
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

    GraphSnapshot *snap = lv_calloc(1, sizeof(GraphSnapshot));
    if (!snap)
        return NULL;

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
        snap->port_refs = lv_calloc((size_t) port_ref_count, sizeof(PortRef));
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
        snap->region_refs = lv_calloc((size_t) region_ref_count, sizeof(RegionRef));
        if (snap->region_refs) {
            int idx = 0;
            for (int i = 0; i < graph->node_count; i++) {
                GeomNode *n = graph->nodes[i];
                if (n->type == GEOM_REGION && n->data.region.segment_count > 0 && n->data.region.boundary_segments) {
                    snap->region_refs[idx].region_node_index = i;
                    snap->region_refs[idx].segment_count = n->data.region.segment_count;
                    snap->region_refs[idx].segment_ids = lv_malloc((size_t) n->data.region.segment_count * sizeof(int));
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
        snap->fb_refs = lv_calloc((size_t) fb_ref_count, sizeof(FBRef));
        if (snap->fb_refs) {
            int idx = 0;
            for (int i = 0; i < graph->node_count; i++) {
                GeomNode *n = graph->nodes[i];
                if (n->type == GEOM_FUNCTION_BLOCK && n->data.func_block.internal_node_count > 0 &&
                    n->data.func_block.internal_nodes) {
                    snap->fb_refs[idx].fb_node_index = i;
                    snap->fb_refs[idx].internal_node_count = n->data.func_block.internal_node_count;
                    snap->fb_refs[idx].internal_node_ids =
                        lv_malloc((size_t) n->data.func_block.internal_node_count * sizeof(int));
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
    snap->nodes = lv_malloc((size_t) snap->node_capacity * sizeof(GeomNode *));
    if (!snap->nodes) {
        /* cleanup refs */
        lv_free((void **) &snap->port_refs);
        for (int i = 0; i < snap->region_ref_count; i++)
            lv_free((void **) &snap->region_refs[i].segment_ids);
        lv_free((void **) &snap->region_refs);
        for (int i = 0; i < snap->fb_ref_count; i++)
            lv_free((void **) &snap->fb_refs[i].internal_node_ids);
        lv_free((void **) &snap->fb_refs);
        lv_free((void **) &snap);
        return NULL;
    }
    for (int i = 0; i < graph->node_count; i++) {
        snap->nodes[i] = graph_node_deep_copy(graph->nodes[i]);
        if (!snap->nodes[i]) {
            /* 回滚已分配的节点 */
            for (int j = 0; j < i; j++)
                snapshot_node_destroy(snap->nodes[j]);
            lv_free((void **) &snap->nodes);
            lv_free((void **) &snap);
            return NULL;
        }
    }

    /* 深拷贝约束数组 */
    snap->constraints = lv_malloc((size_t) snap->constraint_capacity * sizeof(Constraint *));
    if (!snap->constraints) {
        for (int i = 0; i < snap->node_count; i++)
            snapshot_node_destroy(snap->nodes[i]);
        lv_free((void **) &snap->nodes);
        /* cleanup refs */
        lv_free((void **) &snap->port_refs);
        for (int i = 0; i < snap->region_ref_count; i++)
            lv_free((void **) &snap->region_refs[i].segment_ids);
        lv_free((void **) &snap->region_refs);
        for (int i = 0; i < snap->fb_ref_count; i++)
            lv_free((void **) &snap->fb_refs[i].internal_node_ids);
        lv_free((void **) &snap->fb_refs);
        lv_free((void **) &snap);
        return NULL;
    }
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *src = graph->constraints[i];
        Constraint *dst = lv_calloc(1, sizeof(Constraint));
        if (!dst) {
            for (int j = 0; j < i; j++) {
                lv_free((void **) &snap->constraints[j]->participants);
                lv_free((void **) &snap->constraints[j]);
            }
            for (int j = 0; j < snap->node_count; j++)
                snapshot_node_destroy(snap->nodes[j]);
            lv_free((void **) &snap->constraints);
            lv_free((void **) &snap->nodes);
            lv_free((void **) &snap);
            return NULL;
        }
        dst->id = src->id;
        dst->type = src->type;
        dst->template_id = src->template_id;
        dst->participant_count = src->participant_count;
        dst->participants = NULL;
        if (src->participant_count > 0 && src->participants) {
            dst->participants = lv_malloc((size_t) src->participant_count * sizeof(int));
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
            lv_free((void **) &node->symbolic_coords);
        }
        lv_free((void **) &node->numeric_assumption_declaration);
        /* 通过 VTable 分发类型特定数据清理 */
        const SnapshotNodeOps *ops = get_snapshot_ops(node->type);
        if (ops && ops->cleanup_data) {
            ops->cleanup_data(node);
        }
        lv_free((void **) &node);
    }
    for (int i = 0; i < graph->constraint_count; i++) {
        lv_free((void **) &graph->constraints[i]->participants);
        lv_free((void **) &graph->constraints[i]);
    }
    lv_free((void **) &graph->nodes);
    lv_free((void **) &graph->constraints);
    lv_free((void **) &graph->node_index);
    lv_free((void **) &graph->constraint_index);

    /* 2. 从快照恢复所有节点和约束（深拷贝） */
    graph->node_count = snapshot->node_count;
    graph->node_capacity = snapshot->node_capacity;
    graph->constraint_count = snapshot->constraint_count;
    graph->constraint_capacity = snapshot->constraint_capacity;
    graph->next_node_id = snapshot->next_node_id;
    graph->next_constraint_id = snapshot->next_constraint_id;

    graph->nodes = lv_malloc((size_t) graph->node_capacity * sizeof(GeomNode *));
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
            lv_free((void **) &graph->nodes);
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

    graph->constraints = lv_malloc((size_t) graph->constraint_capacity * sizeof(Constraint *));
    if (!graph->constraints) {
        /* 清理已恢复的节点数据，将图重置为空图状态 */
        for (int i = 0; i < graph->node_count; i++) {
            snapshot_node_destroy(graph->nodes[i]);
        }
        lv_free((void **) &graph->nodes);
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
        Constraint *dst = lv_calloc(1, sizeof(Constraint));
        if (!dst) {
            /* 清理已分配的部分约束数据 */
            for (int j = 0; j < i; j++) {
                lv_free((void **) &graph->constraints[j]->participants);
                lv_free((void **) &graph->constraints[j]);
            }
            lv_free((void **) &graph->constraints);
            for (int j = 0; j < graph->node_count; j++) {
                snapshot_node_destroy(graph->nodes[j]);
            }
            lv_free((void **) &graph->nodes);
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
            dst->participants = lv_malloc((size_t) src->participant_count * sizeof(int));
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
            /* 先置空，再在新图中查找 connected_to_id 对应的节点。
             * node_deep_copy_geom_node 复制时保留源引用，置空可避免
             * 找不到目标时残留指向源图的悬垂指针。 */
            port_node->data.port->connected_to = NULL;
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
                /* 先清零（node_deep_copy 复制时保留源引用），再按 segment_ids 重绑定 */
                for (int s = 0; s < region_node->data.region.segment_count; s++)
                    region_node->data.region.boundary_segments[s] = NULL;
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
                /* 先清零（node_deep_copy 复制时保留源引用），再按 internal_node_ids 重绑定 */
                for (int s = 0; s < fb_node->data.func_block.internal_node_count; s++)
                    fb_node->data.func_block.internal_nodes[s] = NULL;
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
        graph->node_index = lv_malloc((size_t) cap * sizeof(GeomNode *));
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
        graph->constraint_index = lv_malloc((size_t) cap * sizeof(Constraint *));
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
    lv_free((void **) &snapshot->nodes);
    for (int i = 0; i < snapshot->constraint_count; i++) {
        lv_free((void **) &snapshot->constraints[i]->participants);
        lv_free((void **) &snapshot->constraints[i]);
    }
    lv_free((void **) &snapshot->constraints);
    /* 释放交叉引用信息 */
    for (int i = 0; i < snapshot->region_ref_count; i++) {
        lv_free((void **) &snapshot->region_refs[i].segment_ids);
    }
    lv_free((void **) &snapshot->region_refs);
    for (int i = 0; i < snapshot->fb_ref_count; i++) {
        lv_free((void **) &snapshot->fb_refs[i].internal_node_ids);
    }
    lv_free((void **) &snapshot->fb_refs);
    lv_free((void **) &snapshot->port_refs);
    lv_free((void **) &snapshot);
}

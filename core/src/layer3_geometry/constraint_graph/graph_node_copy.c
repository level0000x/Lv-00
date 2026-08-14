/**
 * @file graph_node_copy.c
 * @brief 约束图深拷贝（由 graph_node_alloc.c 拆分子模块）
 *
 * @details graph_copy（唯一公共图级复制入口）与游离节点深拷贝
 *          graph_node_deep_copy_detached。
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
#include "lv/determinism_state.h"
#include "lv/memory_pool.h"
#include "lv/symbolic_coord.h"
#include "lv/geo_utils.h" /* geo_point_on_segment / geo_segments_intersect / GEO_EPSILON */

#include "lv/config.h"
#include "lv/context.h"
#include "lv/debug.h"
#include "lv/error_codes.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/stream.h"
#include "lv/lv_json.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_xmacro.h"

#include "graph_node_internal.h"

/* ============== graph_copy：约束图深拷贝（唯一公共图级复制入口） ============== */

/**
 * @brief 深拷贝约束图
 *
 * 遍历源图中的所有节点和约束，在新图中创建完全独立的副本。
 * 高级类型（Region/Circle/Port/FunctionBlock）的类型特定数据
 * （boundary_segments、center/radius_node_id、data.port、
 * internal_nodes/input/output_port_ids）通过 vtable->clone 深拷贝，
 * 内部指针引用通过 vtable->fixup_refs 重映射到新图
 * （graph_add_node_with_id 保证新图节点 ID 与源图一致，故使用恒等 id_map）。
 *
 * 该实现是 ConstraintGraph 的唯一公共深拷贝入口。engine_frozen 的引擎
 * 冻结点与 critical_pair 的工作图复制此前各有独立的深拷贝实现（新 ID
 * 方案 / GraphSnapshot 序列化-恢复方案），均已收敛至此函数。
 */
ConstraintGraph *graph_copy(const ConstraintGraph *graph) {
    if (!graph)
        return NULL;

    ConstraintGraph *new_graph = graph_create();
    if (!new_graph)
        return NULL;

    int max_id = -1; /* 源图最大节点 ID，用于构建恒等 id_map */

    /* 复制所有节点 */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *src = graph->nodes[i];
        if (!src)
            continue;

        /* 使用带ID接口添加节点，保持ID一致 */
        GeomNode *dst = graph_add_node_with_id(new_graph, src->id, src->type, src->symbolic_coords, src->coord_count);
        if (!dst) {
            graph_destroy(new_graph);
            return NULL;
        }

        /* 复制增强字段 */
        dst->trust = src->trust;
        dst->is_active = src->is_active;
        dst->lo_subtype = src->lo_subtype;
        dst->namespace_depth = src->namespace_depth;
        dst->parent_block_id = src->parent_block_id;
        dst->numeric_precision = src->numeric_precision;

        /* 深拷贝 numeric_assumption_declaration */
        if (src->numeric_assumption_declaration) {
            dst->numeric_assumption_declaration = lv_strdup_safe(src->numeric_assumption_declaration);
        }

        /* 高级类型：通过 vtable->clone 深拷贝类型特定数据（union data）到 dst，
         * 修复 graph_copy 之前丢失 Region/Circle/Port/FunctionBlock 类型数据的缺陷 */
        if (src->vtable && src->vtable->clone) {
            if (!src->vtable->clone(src, new_graph)) {
                graph_destroy(new_graph);
                return NULL;
            }
        }

        if (src->id > max_id)
            max_id = src->id;
    }

    /* 第二遍：修复类型特定数据中的交叉引用（此时所有节点均已就绪）。
     * 由于 graph_add_node_with_id 保证新图节点 ID 与源图一致，
     * id_map 为恒等映射（old_id -> 同一 ID）。 */
    if (max_id >= 0) {
        int *id_map = (int *) lv_calloc((size_t) (max_id + 1), sizeof(int));
        if (!id_map) {
            graph_destroy(new_graph);
            return NULL;
        }
        for (int i = 0; i <= max_id; i++) {
            id_map[i] = i;
        }
        for (int i = 0; i < graph->node_count; i++) {
            GeomNode *src = graph->nodes[i];
            if (!src)
                continue;
            if (src->vtable && src->vtable->fixup_refs) {
                GeomNode *dst = graph_get_node(new_graph, src->id);
                if (dst) {
                    src->vtable->fixup_refs(dst, id_map, max_id, new_graph);
                }
            }
        }
        lv_free((void **) &id_map);
    }

    /* 复制所有约束 */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *src = graph->constraints[i];
        if (!src)
            continue;

        Constraint *dst =
            graph_add_constraint_with_id(new_graph, src->id, src->type, src->participants, src->participant_count);
        if (!dst) {
            graph_destroy(new_graph);
            return NULL;
        }

        /* 复制增强字段 */
        dst->template_id = src->template_id;
        dst->is_active = src->is_active;
        dst->numeric_value = src->numeric_value;
        dst->satisfaction = src->satisfaction;
    }

    /* 复制高级图属性 */
    new_graph->dirty = graph->dirty;

    return new_graph;
}

/**
 * @brief 从零创建源节点的深拷贝游离节点（不挂入常驻图）
 *
 * GeomNodeVTable::clone 的契约要求"目标节点已存在于 dst_graph 且 ID 与源节点
 * 相同"（如 graph_copy 通过 graph_add_node_with_id 先建目标节点再 clone）。
 * 本函数为无目标图的从零创建场景（node_deep_copy_geom_node）提供契约适配：
 * 1. 创建临时图；
 * 2. 通过统一分配路径 node_alloc_internal（with_id，不发射流事件）创建外壳
 *    并深拷贝 symbolic_coords；
 * 3. 拷贝增强字段（trust/is_active/lo_subtype/numeric_precision/
 *    namespace_depth/parent_block_id/numeric_assumption_declaration），
 *    字段集与 node_deep_copy_geom_node 原实现（node_deep_copy.c）一致；
 * 4. 调用 src->vtable->clone 深拷贝 union data —— node_deep_copy.c 原
 *    kDataCopyHandlers 分发表（copy_func_block/copy_port/copy_region/
 *    copy_circle）已收敛至各类型 vtable clone 合并实现；
 * 5. 从临时图摘除副本并销毁临时图，返回游离节点。
 *
 * 指针类字段（region.boundary_segments / func_block.internal_nodes /
 * port.connected_to）与 clone 契约一致保留源图引用，由调用方通过 ID 映射重建。
 *
 * @param src    源节点（不修改）
 * @param new_id 副本节点 ID。clone 契约要求目标 ID 与源节点一致（内部以
 *               src->id 创建），故 ID 重映射在 clone 完成后进行；Port.id 与
 *               func_block 端口 ID 数组保持源值不参与重映射（与原语义一致）
 * @return 深拷贝的游离节点，失败返回 NULL（已分配资源已回滚）
 */
GeomNode *graph_node_deep_copy_detached(const GeomNode *src, int new_id) {
    if (!src)
        return NULL;

    ConstraintGraph *tmp = graph_create();
    if (!tmp)
        return NULL;

    /* clone 契约：目标节点 ID 与源节点一致（clone 内部 graph_get_node(dst_graph,
     * node->id)）；ID 重映射在 clone 完成后进行 */
    GeomNode *dst = node_alloc_internal(tmp, src->type, src->id, true, src->symbolic_coords, src->coord_count);
    if (!dst) {
        graph_destroy(tmp);
        return NULL;
    }

    /* 拷贝增强字段（与 graph_copy 深拷贝路径字段集一致） */
    dst->trust = src->trust;
    dst->is_active = src->is_active;
    dst->lo_subtype = src->lo_subtype;
    dst->namespace_depth = src->namespace_depth;
    dst->parent_block_id = src->parent_block_id;
    dst->numeric_precision = src->numeric_precision;
    if (src->numeric_assumption_declaration) {
        dst->numeric_assumption_declaration = lv_strdup_safe(src->numeric_assumption_declaration);
        if (!dst->numeric_assumption_declaration) {
            graph_destroy(tmp);
            return NULL;
        }
    }

    /* union data 深拷贝走 vtable->clone（node_deep_copy.c 原 kDataCopyHandlers
     * 分发表四类 handler 的合并实现：func_block_clone / port_clone /
     * region_clone / circle_clone；point/line_segment 无类型特定数据） */
    if (!src->vtable || !src->vtable->clone) {
        graph_destroy(tmp);
        return NULL;
    }
    if (!src->vtable->clone(src, tmp)) {
        graph_destroy(tmp);
        return NULL;
    }

    /* 从临时图摘除副本（nodes 末尾 + 节点索引），销毁临时图 */
    if (tmp->node_count > 0 && tmp->nodes[tmp->node_count - 1] == dst) {
        tmp->node_count--;
        tmp->nodes[tmp->node_count] = NULL;
    }
    node_index_remove(tmp, src->id);
    graph_destroy(tmp);

    /* clone 完成后重设副本 ID（id_map 重映射语义与 node_deep_copy_geom_node 一致） */
    dst->id = new_id;
    return dst;
}


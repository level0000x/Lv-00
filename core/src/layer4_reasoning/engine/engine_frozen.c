/**
 * @file engine_frozen.c
 * @brief 引擎冻结点快照机制（从 engine.c 拆分）
 *
 * @details 冻结点是对引擎约束图的深拷贝，在执行有风险的符号操作前创建。
 *          如果位电路跳闸，引擎可以回滚到这个快照。
 *          实现完整的图深拷贝（节点/约束/哈希索引重建）与快照管理。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv/engine.h"

#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/lv.h"
#include "lv/lv_config.h"
#include "lv/node_deep_copy.h"
#include "lv/symbolic_coord.h"

/**
 * @brief 深拷贝整个约束图
 *
 * 返回一个新分配的 ConstraintGraph，它是源图的结构克隆。
 * 调用方拥有返回的图的所有权，必须调用 graph_destroy() 释放。
 *
 * @param src 源约束图
 * @return 深拷贝后的新图，失败返回 NULL
 */
static ConstraintGraph *graph_deep_copy(const ConstraintGraph *src) {
    if (!src)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "graph_deep_copy: src is NULL");

    ConstraintGraph *dst = graph_create();
    if (!dst)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "graph_deep_copy: graph_create failed");

    /* 构建ID映射表：old_id -> new_id */
    int max_id = 0;
    for (int i = 0; i < src->node_count; i++) {
        if (src->nodes[i]->id > max_id)
            max_id = src->nodes[i]->id;
    }
    int *id_map = NULL;
    if (max_id > 0) {
        id_map = lv_calloc((size_t) (max_id + 1), sizeof(int));
        if (!id_map) {
            graph_destroy(dst);
            lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "graph_deep_copy: calloc id_map failed");
        }
        for (int i = 0; i <= max_id; i++)
            id_map[i] = -1;
    }

    /* 第一遍：深拷贝所有节点 */
    /* 预分配足够容量以容纳所有源节点 */
    if (src->node_count > 0) {
        dst->nodes = lv_calloc((size_t) src->node_count, sizeof(GeomNode *));
        if (!dst->nodes) {
            graph_destroy(dst);
            lv_free((void **) &id_map);
            lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "graph_deep_copy: calloc dst->nodes failed");
        }
    }
    for (int i = 0; i < src->node_count; i++) {
        GeomNode *orig = src->nodes[i];
        GeomNode *copy = node_deep_copy_geom_node(orig, NULL);
        if (!copy) {
            /* 失败时清理 */
            graph_destroy(dst);
            lv_free((void **) &id_map);
            lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "graph_deep_copy: node_deep_copy_geom_node failed");
        }

        /* 分配新ID并记录映射 */
        int new_id = dst->next_node_id++;
        copy->id = new_id;
        if (id_map && orig->id >= 0 && orig->id <= max_id) {
            id_map[orig->id] = new_id;
        }

        /* 添加到目标图（无需realloc，已预分配） */
        dst->nodes[dst->node_count++] = copy;
    }

    /* 第二遍：通过 vtable 更新拷贝节点中的交叉引用 */
    for (int i = 0; i < dst->node_count; i++) {
        GeomNode *copy = dst->nodes[i];
        if (copy->vtable && copy->vtable->fixup_refs) {
            copy->vtable->fixup_refs(copy, id_map, max_id, dst);
        }
    }

    /* 复制约束(Constraint)数组。
     * 约束是图完整性的关键部分，如果约束复制失败（内存不足），
     * 深拷贝的结果将是不完整的，可能导致后续求解产生错误结果。
     * 因此，约束分配失败时释放已分配的所有资源并返回 NULL。 */
    for (int i = 0; i < src->constraint_count; i++) {
        Constraint *orig_c = src->constraints[i];
        Constraint *copy_c = lv_calloc(1, sizeof(Constraint));
        if (!copy_c) {
            /* 约束分配失败：释放已复制的所有约束和整个目标图，返回 NULL */
            for (int k = 0; k < dst->constraint_count; k++) {
                lv_free((void **) &dst->constraints[k]->participants);
                lv_free((void **) &dst->constraints[k]);
            }
            lv_free((void **) &dst->constraints);
            graph_destroy(dst);
            lv_free((void **) &id_map);
            lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "graph_deep_copy: calloc copy_c failed");
        }

        copy_c->id = dst->next_constraint_id++;
        copy_c->type = orig_c->type;
        copy_c->template_id = orig_c->template_id;
        copy_c->participant_count = orig_c->participant_count;

        if (orig_c->participant_count > 0) {
            copy_c->participants = lv_calloc((size_t) orig_c->participant_count, sizeof(int));
            if (copy_c->participants) {
                for (int j = 0; j < orig_c->participant_count; j++) {
                    int old_pid = orig_c->participants[j];
                    if (id_map && old_pid >= 0 && old_pid <= max_id && id_map[old_pid] >= 0) {
                        copy_c->participants[j] = id_map[old_pid];
                    } else {
                        copy_c->participants[j] = old_pid;
                    }
                }
            } else {
                copy_c->participant_count = 0;
            }
        } else {
            copy_c->participants = NULL;
        }

        Constraint **tmp_cons;
        if (dst->constraint_count == 0) {
            tmp_cons = lv_calloc(1, sizeof(Constraint *));
        } else {
            tmp_cons = lv_realloc(dst->constraints, (size_t) (dst->constraint_count + 1) * sizeof(Constraint *));
        }
        if (tmp_cons) {
            dst->constraints = tmp_cons;
            dst->constraints[dst->constraint_count++] = copy_c;
        } else {
            /* realloc 失败：释放当前约束、已复制的所有约束、整个目标图和 id_map */
            lv_free((void **) &copy_c->participants);
            lv_free((void **) &copy_c);
            for (int k = 0; k < dst->constraint_count; k++) {
                lv_free((void **) &dst->constraints[k]->participants);
                lv_free((void **) &dst->constraints[k]);
            }
            lv_free((void **) &dst->constraints);
            graph_destroy(dst);
            lv_free((void **) &id_map);
            lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "graph_deep_copy: realloc constraints failed");
        }
    }

    lv_free((void **) &id_map);

    /* ---- 重建哈希索引 ----
     * graph_create() 初始化空的哈希表（node_index, constraint_index），
     * 但上面的深拷贝绕过了 graph_alloc_node/graph_alloc_constraint，
     * 所以哈希表仍然为空。使用与 constraint_graph.c 相同的 FNV-1a
     * 开放寻址方案重建它们。
     */
    {
        /* --- 重建 node_index --- */
        if (dst->node_count > 0) {
            int ni_cap = lv_NODE_INDEX_INITIAL_SIZE;
            while (ni_cap < dst->node_count * lv_ARRAY_GROWTH_FACTOR)
                ni_cap *= lv_ARRAY_GROWTH_FACTOR; /* 保持负载率 < 0.5 */

            lv_free((void **) &dst->node_index); /* 释放 graph_create 创建的空表 */
            dst->node_index = lv_calloc((size_t) ni_cap, sizeof(GeomNode *));
            if (!dst->node_index) {
                graph_destroy(dst);
                lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "graph_deep_copy: calloc node_index failed");
            }
            dst->node_index_capacity = ni_cap;
            for (int i = 0; i < dst->node_count; i++) {
                unsigned idx = (unsigned) dst->nodes[i]->id * lv_FNV_HASH_MULTIPLIER & (unsigned) (ni_cap - 1);
                while (dst->node_index[idx] != NULL) {
                    idx = (idx + 1) & (unsigned) (ni_cap - 1);
                }
                dst->node_index[idx] = dst->nodes[i];
            }
        }

        /* --- 重建 constraint_index --- */
        if (dst->constraint_count > 0) {
            int ci_cap = lv_CONSTRAINT_INDEX_INITIAL_SIZE;
            while (ci_cap < dst->constraint_count * lv_ARRAY_GROWTH_FACTOR)
                ci_cap *= lv_ARRAY_GROWTH_FACTOR;

            lv_free((void **) &dst->constraint_index);
            dst->constraint_index = lv_calloc((size_t) ci_cap, sizeof(Constraint *));
            if (!dst->constraint_index) {
                graph_destroy(dst);
                lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "graph_deep_copy: calloc constraint_index failed");
            }
            dst->constraint_index_capacity = ci_cap;
            for (int i = 0; i < dst->constraint_count; i++) {
                unsigned idx = (unsigned) dst->constraints[i]->id * lv_FNV_HASH_MULTIPLIER & (unsigned) (ci_cap - 1);
                while (dst->constraint_index[idx] != NULL) {
                    idx = (idx + 1) & (unsigned) (ci_cap - 1);
                }
                dst->constraint_index[idx] = dst->constraints[i];
            }
        }
    }

    return dst;
}

/** @brief 创建引擎状态冻结点 @details 保存当前引擎状态，用于后续回滚。 @param engine 引擎实例 @return 冻结点句柄，失败返回 NULL */
void *engine_create_frozen_point(lvEngine *engine) {
    if (!engine || !engine->main_graph)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "engine_create_frozen_point: NULL engine or main_graph");

    ConstraintGraph *snapshot = graph_deep_copy(engine->main_graph);
    return (void *) snapshot;
}

/** @brief 恢复引擎状态到指定冻结点 @param engine 引擎实例 @param frozen_point 冻结点句柄 @return true 成功 */
bool engine_restore_frozen_point(lvEngine *engine, void *frozen_point) {
    if (!engine || !frozen_point)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "engine_restore_frozen_point: NULL engine or frozen_point");

    ConstraintGraph *snapshot = (ConstraintGraph *) frozen_point;

    /* 销毁当前图 */
    if (engine->main_graph) {
        graph_destroy(engine->main_graph);
    }

    /* 用快照替换（所有权转移给引擎） */
    engine->main_graph = snapshot;

    /* 同时更新电路系统的冻结点状态 */
    circuit_set_frozen_point(NULL);
    engine->frozen_point = NULL;

    return true;
}

/** @brief 销毁冻结点并释放关联资源 @param frozen_point 冻结点句柄 */
void engine_destroy_frozen_point(void *frozen_point) {
    if (!frozen_point)
        return;
    ConstraintGraph *snapshot = (ConstraintGraph *) frozen_point;
    graph_destroy(snapshot);
}

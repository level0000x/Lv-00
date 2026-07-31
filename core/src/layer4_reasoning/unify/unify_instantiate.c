/**
 * @file unify_instantiate.c
 * @brief proposition instantiation
 * @details Split from unify.c
 */

#include "unify.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/geometric_primitives.h"
#include "lv/proof.h"

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "normalization.h"
#include "stream.h"
#include "stream_context_util.h"
#include "type_system.h"
#include "lv/lv_strbuf.h"
#include "unify_internal.h"

/* ---------------------------------------------------------------------------
 * 命题的实例化
 * ------------------------------------------------------------------------- */

/**
 * @brief ID映射表条目结构
 *
 * 用于在深拷贝过程中建立旧ID到新ID的映射关系
 */
typedef struct {
    int old_id; /**< 源图中的节点ID */
    int new_id; /**< 目标图中的节点ID */
} IdMappingEntry;

/**
 * @brief ID映射表结构
 */
typedef struct {
    IdMappingEntry *entries; /**< 映射条目数组 */
    int count;               /**< 当前条目数量 */
    int capacity;            /**< 数组容量 */
} IdMappingTable;

/**
 * @brief 初始化ID映射表
 * @param table 映射表指针
 * @param initial_capacity 初始容量
 */
/**
 * @brief 初始化ID映射表
 *
 * 分配映射条目数组，若分配失败则将 entries 设为 NULL 并将 capacity 设为 0，
 * 后续 id_mapping_add 会安全地检测到此状态并返回失败。
 *
 * @param table            映射表指针
 * @param initial_capacity 初始容量（必须 > 0）
 * @return true 初始化成功，false 内存分配失败
 */
static bool id_mapping_init(IdMappingTable *table, int initial_capacity) {
    table->entries = (IdMappingEntry *) lv_calloc((size_t) initial_capacity, sizeof(IdMappingEntry));
    if (!table->entries) {
        table->capacity = 0;
        table->count = 0;
        lv_RETURN_ERROR_BOOL(lv_ERROR_OUT_OF_MEMORY, "id_mapping_init: calloc entries failed");
    }
    table->count = 0;
    table->capacity = initial_capacity;
    return true;
}

/**
 * @brief 销毁ID映射表
 * @param table 映射表指针
 */
static void id_mapping_destroy(IdMappingTable *table) {
    if (table->entries) {
        lv_free((void **) &table->entries);
        table->entries = NULL;
    }
    table->count = 0;
    table->capacity = 0;
}

/**
 * @brief 添加ID映射
 * @param table 映射表指针
 * @param old_id 旧ID
 * @param new_id 新ID
 * @return 成功返回true，失败返回false（内存不足或表未初始化）
 */
static bool id_mapping_add(IdMappingTable *table, int old_id, int new_id) {
    /* 检查表是否已初始化（init 失败时 entries 为 NULL） */
    if (!table->entries)
        lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_STATE, "id_mapping_add: table not initialized");
    if (!lv_ensure_capacity((void **)&table->entries, table->count,
                            &table->capacity, sizeof(IdMappingEntry), 1))
        return false;

    table->entries[table->count].old_id = old_id;
    table->entries[table->count].new_id = new_id;
    table->count++;
    return true;
}

/**
 * @brief 查找旧ID对应的新ID
 * @param table 映射表指针
 * @param old_id 旧ID
 * @return 找到返回新ID，未找到返回-1
 */
static int id_mapping_find(const IdMappingTable *table, int old_id) {
    for (int i = 0; i < table->count; i++) {
        if (table->entries[i].old_id == old_id) {
            return table->entries[i].new_id;
        }
    }
    return -1;
}

/**
 * @brief 辅助函数：深拷贝约束图（带完整ID映射）
 *
 * 创建约束图的深拷贝，包括所有节点和约束。
 * 使用ID映射表确保LINE_SEGMENT端点和REGION边界正确更新。
 *
 * 【拷贝流程 —— 五阶段策略】
 * 由于不同节点类型之间存在拓扑依赖关系（例如 REGION 引用 LINE_SEGMENT），
 * 深拷贝必须按依赖顺序分阶段进行：
 *
 *   阶段0 - 预初始化：创建空目标图，初始化 ID 映射表（预分配 node_count+16 项）
 *   阶段1 - 拷贝无依赖节点（POINT + PORT）：这些节点不引用其他节点，
 *           可在无映射表的情况下直接拷贝，完成后填充映射表
 *   阶段2 - 拷贝 LINE_SEGMENT：需要查找端点ID在映射表中的新ID，
 *           通过符号坐标哈希值查找端点映射
 *   阶段3 - 拷贝 REGION：需要查找边界线段ID在映射表中的新ID
 *   阶段4 - 拷贝 FUNCTION_BLOCK：需要查找内部节点、输入输出端口ID的新ID
 *   阶段5 - 拷贝约束：使用ID映射表转换约束中的所有参与者引用
 *
 * 【错误处理】任何阶段的分配失败或添加失败均通过 goto fail 跳转，
 * 在 fail 标签处统一销毁 ID 映射表和已构建的目标图。
 *
 * @param src 源约束图
 * @return 深拷贝后的约束图，失败返回NULL
 */

/* --- 约束拷贝：函数指针表 --- */
typedef AddConstraintResult (*ConstraintCopyFunc)(ConstraintGraph *dst, int *participants, int count);

static AddConstraintResult copy_incidence(ConstraintGraph *dst, int *p, int n) {
    if (n >= 2)
        return graph_add_incidence(dst, p[0], p[1]);
    return ADD_CONSTRAINT_CONFLICT;
}

static AddConstraintResult copy_betweenness(ConstraintGraph *dst, int *p, int n) {
    if (n >= 3)
        return graph_add_betweenness(dst, p[0], p[1], p[2]);
    return ADD_CONSTRAINT_CONFLICT;
}

static AddConstraintResult copy_intersection(ConstraintGraph *dst, int *p, int n) {
    if (n >= 3)
        return graph_add_intersection(dst, p[0], p[1], p[2]);
    return ADD_CONSTRAINT_CONFLICT;
}

static AddConstraintResult copy_containment(ConstraintGraph *dst, int *p, int n) {
    if (n >= 2)
        return graph_add_containment(dst, p[0], p[1]);
    return ADD_CONSTRAINT_CONFLICT;
}

static AddConstraintResult copy_angle(ConstraintGraph *dst, int *p, int n) {
    if (n >= 2)
        return graph_add_angle(dst, p[0], p[1], 0.0);
    return ADD_CONSTRAINT_CONFLICT;
}

static AddConstraintResult copy_connection(ConstraintGraph *dst, int *p, int n) {
    if (n >= 2)
        return graph_add_connection(dst, p[0], p[1]);
    return ADD_CONSTRAINT_CONFLICT;
}

static ConstraintCopyFunc s_constraint_copy_funcs[] = {
    [INCIDENCE] = copy_incidence,
    [BETWEENNESS] = copy_betweenness,
    [INTERSECTION] = copy_intersection,
    [CONTAINMENT] = copy_containment,
    [CONNECTION] = copy_connection,
    [ANGLE] = copy_angle,
};
static const int s_constraint_copy_func_count = (int)(sizeof(s_constraint_copy_funcs) / sizeof(s_constraint_copy_funcs[0]));

static ConstraintGraph *deep_copy_graph(const ConstraintGraph *src) {
    if (!src)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "deep_copy_graph: src is NULL");

    ConstraintGraph *dst = graph_create();
    if (!dst)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "deep_copy_graph: graph_create failed");

    /* 创建ID映射表 */
    IdMappingTable id_map;
    /* 初始化 ID 映射表，用于记录源图节点 ID 到目标图节点 ID 的对应关系。
     * 若映射表初始化失败，释放已分配的目标图和 ID 映射表并返回 NULL。 */
    id_mapping_init(&id_map, src->node_count + 16);
    if (!id_map.entries) {
        graph_destroy(dst);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "deep_copy_graph: id_mapping_init failed");
    }

    /*
     * 阶段1：拷贝无依赖节点（POINT 和 PORT）
     *
     * 这些节点类型不引用任何尚未创建的节点，可以在映射表
     * 尚未完全填充的情况下安全地逐个拷贝。拷贝后立即将
     * (old_id, new_id) 写入映射表，供后续阶段使用。
     *
     * LINE_SEGMENT、REGION、FUNCTION_BLOCK 在此阶段跳过，
     * 等待其依赖的节点先完成拷贝。
     */

    /* 先拷贝POINT和PORT节点 */
    for (int i = 0; i < src->node_count; i++) {
        GeomNode *src_node = src->nodes[i];
        int old_id = src_node->id;
        int new_id = -1;

        switch (src_node->type) {
            case GEOM_POINT: {
                AddNodeResult r = graph_add_point(dst, src_node->symbolic_coords, src_node->coord_count);
                if (r != ADD_NODE_OK)
                    goto fail;
                new_id = dst->nodes[dst->node_count - 1]->id;
                break;
            }
            case GEOM_PORT: {
                PortType pt = PORT_INPUT;
                int ns_depth = 0;
                int parent_id = -1;
                if (src_node->data.port) {
                    pt = src_node->data.port->type;
                    ns_depth = src_node->data.port->namespace_depth;
                    parent_id = src_node->data.port->parent_block_id;
                }
                AddNodeResult r = graph_add_port(dst, pt, ns_depth, parent_id);
                if (r != ADD_NODE_OK)
                    goto fail;
                new_id = dst->nodes[dst->node_count - 1]->id;

                /* 复制端口属性到新节点 */
                GeomNode *new_node = dst->nodes[dst->node_count - 1];
                if (new_node && new_node->data.port && src_node->data.port) {
                    new_node->data.port->is_formal_param = src_node->data.port->is_formal_param;
                    new_node->data.port->is_polymorphic = src_node->data.port->is_polymorphic;
                    new_node->data.port->type_region = src_node->data.port->type_region;
                    /* 注意：type_region 不做深拷贝，共享引用 */
                }
                break;
            }
            case GEOM_LINE_SEGMENT:
            case GEOM_REGION:
            case GEOM_CIRCLE:
            case GEOM_FUNCTION_BLOCK:
                /* 这些类型在第二阶段处理 */
                continue;
        }

        /* 记录ID映射 */
        if (new_id >= 0) {
            if (!id_mapping_add(&id_map, old_id, new_id))
                goto fail;
        }
    }

    /*
     * 阶段2：拷贝 LINE_SEGMENT 节点
     *
     * LINE_SEGMENT 依赖两个端点（POINT 节点），这些端点在阶段1中
     * 已完成拷贝并写入映射表。由于 graph_add_line_segment 将端点坐标
     * 拷贝到 LINE_SEGMENT 的 symbolic_coords 数组中（而非通过图约束
     * 存储端点 ID），此处需要通过坐标匹配在源图中查找端点：
     *
     *   1. 取 LINE_SEGMENT 的 symbolic_coords[0] 和 symbolic_coords[1]
     *      （分别对应端点1和端点2的第一个坐标维度）
     *   2. 遍历源图的 POINT 节点，比较 symbolic_coords[0] 是否匹配
     *   3. 将找到的端点旧 ID 通过映射表转换为新 ID
     *   4. 调用 graph_add_line_segment 创建新线段
     *
     * 若无法确定端点（无坐标或找不到匹配 POINT），使用 -1 占位符。
     */
    for (int i = 0; i < src->node_count; i++) {
        GeomNode *src_node = src->nodes[i];
        if (src_node->type != GEOM_LINE_SEGMENT)
            continue;

        int old_id = src_node->id;

        /*
         * 通过坐标匹配查找端点 POINT 节点。
         * LINE_SEGMENT 的 symbolic_coords 是端点坐标的副本，
         * 因此可以通过比较坐标值找到原始端点。
         */
        int endpoint1_old = -1, endpoint2_old = -1;

        if (src_node->coord_count >= 2 && src_node->symbolic_coords[0] && src_node->symbolic_coords[1]) {
            /*
             * 遍历源图 POINT 节点，找到坐标匹配的端点。
             * 每个 POINT 的 symbolic_coords[0] 与端点坐标比较。
             */
            for (int j = 0; j < src->node_count; j++) {
                GeomNode *candidate = src->nodes[j];
                if (candidate->type != GEOM_POINT)
                    continue;
                if (!candidate->symbolic_coords || !candidate->symbolic_coords[0])
                    continue;

                /* 比较候选 POINT 的坐标是否与端点1坐标匹配 */
                if (endpoint1_old < 0 &&
                    symbolic_coord_compare(src_node->symbolic_coords[0], candidate->symbolic_coords[0]) == 0) {
                    endpoint1_old = candidate->id;
                    continue;
                }

                /* 比较候选 POINT 的坐标是否与端点2坐标匹配 */
                if (endpoint2_old < 0 &&
                    symbolic_coord_compare(src_node->symbolic_coords[1], candidate->symbolic_coords[0]) == 0) {
                    endpoint2_old = candidate->id;
                }
            }
        }

        /* 查找端点的新ID */
        int endpoint1_new = id_mapping_find(&id_map, endpoint1_old);
        int endpoint2_new = id_mapping_find(&id_map, endpoint2_old);

        /* 如果找不到映射，使用-1（占位符） */
        if (endpoint1_new < 0)
            endpoint1_new = -1;
        if (endpoint2_new < 0)
            endpoint2_new = -1;

        AddNodeResult r = graph_add_line_segment(dst, endpoint1_new, endpoint2_new);
        if (r != ADD_NODE_OK)
            goto fail;

        int new_id = dst->nodes[dst->node_count - 1]->id;
        if (!id_mapping_add(&id_map, old_id, new_id))
            goto fail;
    }

    /*
     * 阶段3：拷贝 REGION 节点
     *
     * REGION 依赖其边界线段（LINE_SEGMENT 节点），这些线段在阶段2中
     * 已完成拷贝。此处遍历边界线段数组，通过映射表将旧线段ID转换为
     * 新ID，再调用 graph_add_region 创建新区域节点。
     */
    for (int i = 0; i < src->node_count; i++) {
        GeomNode *src_node = src->nodes[i];
        if (src_node->type != GEOM_REGION)
            continue;

        int old_id = src_node->id;

        /* 转换边界线段ID */
        int *new_boundary_ids = NULL;
        int new_segment_count = 0;

        if (src_node->data.region.boundary_segments && src_node->data.region.segment_count > 0) {
            new_boundary_ids = lv_calloc(src_node->data.region.segment_count, sizeof(int));
            if (!new_boundary_ids)
                goto fail;

            for (int j = 0; j < src_node->data.region.segment_count; j++) {
                int old_seg_id = src_node->data.region.boundary_segments[j]->id;
                int new_seg_id = id_mapping_find(&id_map, old_seg_id);
                if (new_seg_id >= 0) {
                    new_boundary_ids[new_segment_count++] = new_seg_id;
                }
            }
        }

        AddNodeResult r = graph_add_region(dst, new_boundary_ids, new_segment_count);
        if (new_boundary_ids)
            lv_free((void **) &new_boundary_ids);
        if (r != ADD_NODE_OK)
            goto fail;

        int new_id = dst->nodes[dst->node_count - 1]->id;
        if (!id_mapping_add(&id_map, old_id, new_id))
            goto fail;
    }

    /*
     * 阶段4：拷贝 FUNCTION_BLOCK 节点
     *
     * FUNCTION_BLOCK 是三阶段中最复杂的节点类型，需要转换三类引用：
     *   a) 内部节点 (internal_nodes)：在阶段1中作为 POINT/PORT 已拷贝
     *   b) 输入端口 (input_port_ids)：在阶段1中作为 PORT 已拷贝
     *   c) 输出端口 (output_port_ids)：在阶段1中作为 PORT 已拷贝
     * 所有ID通过映射表查找新ID后，调用 graph_add_function_block 创建。
     * 拷贝后同步 determinism_state 确定性状态标志。
     */
    for (int i = 0; i < src->node_count; i++) {
        GeomNode *src_node = src->nodes[i];
        if (src_node->type != GEOM_FUNCTION_BLOCK)
            continue;

        int old_id = src_node->id;

        /* 转换内部节点ID */
        int *new_internal_ids = NULL;
        int new_internal_count = 0;
        if (src_node->data.func_block.internal_nodes && src_node->data.func_block.internal_node_count > 0) {
            new_internal_ids = lv_calloc(src_node->data.func_block.internal_node_count, sizeof(int));
            if (!new_internal_ids)
                goto fail;

            for (int j = 0; j < src_node->data.func_block.internal_node_count; j++) {
                int old_internal_id = src_node->data.func_block.internal_nodes[j]->id;
                int new_internal_id = id_mapping_find(&id_map, old_internal_id);
                if (new_internal_id >= 0) {
                    new_internal_ids[new_internal_count++] = new_internal_id;
                }
            }
        }

        /* 转换端口ID */
        int *new_input_ids = NULL;
        int new_input_count = 0;
        if (src_node->data.func_block.input_port_ids && src_node->data.func_block.input_count > 0) {
            new_input_ids = lv_calloc(src_node->data.func_block.input_count, sizeof(int));
            if (!new_input_ids) {
                lv_free((void **) &new_internal_ids);
                goto fail;
            }
            for (int j = 0; j < src_node->data.func_block.input_count; j++) {
                int old_port_id = src_node->data.func_block.input_port_ids[j];
                int new_port_id = id_mapping_find(&id_map, old_port_id);
                if (new_port_id >= 0) {
                    new_input_ids[new_input_count++] = new_port_id;
                }
            }
        }

        int *new_output_ids = NULL;
        int new_output_count = 0;
        if (src_node->data.func_block.output_port_ids && src_node->data.func_block.output_count > 0) {
            new_output_ids = lv_calloc(src_node->data.func_block.output_count, sizeof(int));
            if (!new_output_ids) {
                lv_free((void **) &new_internal_ids);
                lv_free((void **) &new_input_ids);
                goto fail;
            }
            for (int j = 0; j < src_node->data.func_block.output_count; j++) {
                int old_port_id = src_node->data.func_block.output_port_ids[j];
                int new_port_id = id_mapping_find(&id_map, old_port_id);
                if (new_port_id >= 0) {
                    new_output_ids[new_output_count++] = new_port_id;
                }
            }
        }

        AddNodeResult r = graph_add_function_block(dst, new_internal_ids, new_internal_count, new_input_ids,
                                                   new_input_count, new_output_ids, new_output_count);

        lv_free((void **) &new_internal_ids);
        lv_free((void **) &new_input_ids);
        lv_free((void **) &new_output_ids);

        if (r != ADD_NODE_OK)
            goto fail;

        /* 复制确定性状态 */
        GeomNode *new_node = dst->nodes[dst->node_count - 1];
        if (new_node && new_node->type == GEOM_FUNCTION_BLOCK) {
            new_node->data.func_block.determinism_state = src_node->data.func_block.determinism_state;
        }

        int new_id = new_node->id;
        if (!id_mapping_add(&id_map, old_id, new_id))
            goto fail;
    }

    /*
     * 阶段5：拷贝约束
     *
     * 所有节点在阶段1-4中已完成拷贝，映射表已完全填充。
     * 遍历源图的所有约束，将每个约束的 participant IDs 通过映射表
     * 转换为目标图中的新ID，再调用对应的 graph_add_* 函数添加约束。
     *
     * 找不到映射的参与者ID保留原始值（可能是外部引用），
     * 约束添加失败不视为致命错误（通过 (void)r 丢弃返回值），
     * 因为部分约束可能由于节点映射不完整而无法创建。
     */
    for (int i = 0; i < src->constraint_count; i++) {
        Constraint *sc = src->constraints[i];
        AddConstraintResult r;

        /* 转换约束中的参与者ID */
        int *new_participants = lv_calloc(sc->participant_count, sizeof(int));
        if (!new_participants)
            goto fail;
        int new_participant_count = 0;

        for (int j = 0; j < sc->participant_count; j++) {
            int new_id = id_mapping_find(&id_map, sc->participants[j]);
            if (new_id >= 0) {
                new_participants[new_participant_count++] = new_id;
            } else {
                /* 如果找不到映射，使用原始ID（可能是外部引用） */
                new_participants[new_participant_count++] = sc->participants[j];
            }
        }

        if (sc->type >= 0 && sc->type < s_constraint_copy_func_count && s_constraint_copy_funcs[sc->type]) {
            r = s_constraint_copy_funcs[sc->type](dst, new_participants, new_participant_count);
        } else {
            r = ADD_CONSTRAINT_CONFLICT;
        }
        (void) r; /* 约束添加失败不视为致命错误 */
        lv_free((void **) &new_participants);
    }

    /* 清理ID映射表 */
    id_mapping_destroy(&id_map);

    return dst;

fail:
    id_mapping_destroy(&id_map);
    graph_destroy(dst);
    lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "deep_copy_graph: allocation failed during deep copy");
}

/**
 * @brief 实例化命题图中的多态类型变量
 *
 * @details 对命题图进行深拷贝后，将指定节点（type_var_node_id）的端口类型区域
 *          设置为传入的具体类型 concrete_type。此函数用于将带有多态类型变量的
 *          命题模式实例化为具体类型版本，通常发生在合一检查确定类型绑定之后。
 *
 * 【类型区域生命周期管理规则 —— 调用者必读】
 *
 *   本函数对 concrete_type 使用引用语义（浅拷贝/指针赋值），不创建深拷贝。
 *   这意味着调用者必须承担以下生命周期责任：
 *
 *   1. concrete_type 指向的内存必须在 out_instantiated 指向的图存在期间
 *      始终保持有效。任何在实例化图销毁之前释放 concrete_type 的行为都将
 *      导致 inst 图中端口的 type_region 指针成为悬垂指针。
 *
 *   2. 禁止修改 concrete_type 的子字段（子类型、约束ID等），因为所有通过
 *      此函数实例化的端口共享同一份 concrete_type 引用，修改会导致
 *      不可预期的副作用。
 *
 *   3. 销毁规则：调用者应先销毁 out_instantiated（调用 graph_destroy），
 *      再销毁 concrete_type（调用 type_region_destroy）。反向操作将导致
 *      type_region 悬垂指针。注意 graph_destroy 不会释放 type_region，
 *      因为 type_region 的所有权属于调用者。
 *
 *   4. 如果调用者无法保证上述生命周期覆盖，应在调用本函数之前先通过
 *      其他方式创建 concrete_type 的深拷贝，再将拷贝传入。
 *      type_system 模块中已有 type_region_deep_copy() 静态函数提供此能力，
 *      但尚未导出为公共API。规划中将在后续版本中将其导出为
 *      type_system_deep_copy_type_region() 公共接口。
 *
 * 【内存所有权模型】
 *   - proposition: 输入，本函数不获取其所有权，不修改
 *   - concrete_type: 输入，本函数获取其引用（非所有权），调用者负责释放
 *   - out_instantiated: 输出，调用者获得所有权，使用完毕后需 graph_destroy
 *
 * @param proposition     含多态类型变量的命题图（输入，不修改）
 * @param type_var_node_id 待实例化的类型变量节点ID
 * @param concrete_type   具体类型区域（输入，生命周期由调用者管理）
 * @param out_instantiated 输出：实例化后的命题图（调用者获得所有权）
 * @return true 表示实例化成功，false 表示失败（参数无效或内存不足）
 */
bool unify_instantiate_proposition(ConstraintGraph *proposition, int type_var_node_id, const TypeRegion *concrete_type,
                                   ConstraintGraph **out_instantiated) {
    if (!proposition || !concrete_type || !out_instantiated)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "unify_instantiate_proposition: NULL parameter");

    *out_instantiated = NULL;

    /* 深拷贝命题图 */
    ConstraintGraph *inst = deep_copy_graph(proposition);
    if (!inst)
        lv_RETURN_ERROR_BOOL(lv_ERROR_OUT_OF_MEMORY, "unify_instantiate_proposition: deep_copy_graph failed");

    /* 查找类型变量节点 */
    GeomNode *type_var_node = NULL;
    for (int i = 0; i < inst->node_count; i++) {
        if (inst->nodes[i]->id == type_var_node_id) {
            type_var_node = inst->nodes[i];
            break;
        }
    }

    if (type_var_node && type_var_node->type == GEOM_PORT && type_var_node->data.port) {
        /*
         * 【类型区域赋值 —— 引用语义安全性说明】
         *
         * 此处将 concrete_type 指针直接赋值给 type_region，使用浅拷贝
         * （引用语义），而非深拷贝。这是有意为之的设计决策，原因如下：
         *
         * A. 为什么当前使用浅拷贝：
         *    - type_system 模块中已有 type_region_deep_copy() 静态函数，
         *      但该函数尚未导出为公共API（static 修饰，仅 type_system.c 内可见）。
         *    - 将 type_region_deep_copy 提升为公共API需要重新考虑：
         *      a) 深拷贝后谁负责释放（所有权转移语义）
         *      b) 递归子类型的生命周期管理（first_type/second_type 等）
         *      c) contained_node_ids / constraint_ids 在深拷贝后的有效性
         *    - 当前所有调用 unify_instantiate_proposition 的路径都确保
         *      concrete_type 来自 TypeSystem 的注册表（type_regions 数组），
         *      其生命周期由 TypeSystem 管理，通常覆盖整个引擎生命周期。
         *      因此浅拷贝在当前调用场景下实际上是安全的。
         *
         * B. 浅拷贝的风险（在非标准调用路径中）：
         *    1. 悬垂指针：若调用者传入栈上分配的临时 TypeRegion，或调用后
         *       立即销毁 concrete_type，则实例化图中的指针将失效。
         *    2. 别名修改：多个端口共享同一 concrete_type 时，任何一方通过
         *       指针修改其字段（如约束ID、子类型等）将影响所有引用者。
         *    3. 双重释放：若调用者在 graph_destroy 之后仍尝试 type_region_destroy
         *       concrete_type，因为 graph_destroy 中不会释放 type_region（端口
         *       不拥有 type_region 的所有权），不会双重释放；但若未来修改
         *       graph_destroy 实现，则需注意此问题。
         *
         * C. 规划中的修复方案：
         *    1. 短期：在 debug.h 中添加 TYPE_REGION_LIFECYCLE_CHECK 宏，
         *       在调试模式下对 concrete_type 添加哨兵标记，检测悬垂访问。
         *    2. 长期：将 type_region_deep_copy() 导出为公共API
         *       type_system_deep_copy_type_region()，并在本函数中使用它，
         *       同时明确文档化深拷贝后的所有权转移规则。
         *
         * D. 当前的安全保障（调用约定）：
         *    - 调用者约定：concrete_type 必须在 out_instantiated 的整个
         *      生命周期内有效（详见函数头部的生命周期管理规则文档）。
         *    - 调试模式断言：以下检查在调试模式下警告调用者注意生命周期：
          */
        if (debug_is_debug_mode()) {
            if (!concrete_type->alias_name && !concrete_type->variable_name && concrete_type->kind == 0 &&
                concrete_type->level == 0) {
                /* 如果 concrete_type 的所有可识别字段均为零/空，可能是已被销毁
                  * 或未初始化的对象。仅在调试模式下记录警告，不中断执行，
                  * 因为某些合法的类型区域可能确实全部为零值。 */
                debug_log(LOG_LEVEL_WARN, "unify",
                          "unify_instantiate_proposition: concrete_type at %p appears "
                          "to be zero-initialized or destroyed — possible dangling pointer "
                          "risk for node %d",
                          (const void *) concrete_type, type_var_node_id);
            }
        }
        type_var_node->data.port->type_region = (TypeRegion *) concrete_type;
        type_var_node->data.port->is_polymorphic = false;
    }

    *out_instantiated = inst;
    return true;
}

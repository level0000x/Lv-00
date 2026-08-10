/**
 * @file bit_burning.c
 * @brief 位数熔断系统实现
 *
 * 实现 GMP 计算中间结果位数超过 10^6 比特阈值时的熔断保护机制。
 * 包括传染阻断、冻结点回退和逃逸出口三个核心功能。
 *
 * @author Lv-00 Project
 * @version 1.0.0
 */

#include "lv/bit_burning.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv_internal.h"
#include "lv_utils.h"
/* node_destroy：统一节点释放路径（graph_index.c 实现），含内部字段清理与
 * vtable->free 类型特定 union 数据释放，替代本文件手写的按类型 switch 释放 */
#include "layer3_geometry/constraint_graph/graph_node_internal.h"

/* ── 线程局部全局熔断状态 ── */
static lv_THREAD_LOCAL BitBurningState g_bit_burning_state = {0};
static lv_THREAD_LOCAL char *g_checkpoint_json = NULL; /* 冻结点 JSON 快照 */

/**
 * @brief 检查中间结果是否超过位数阈值
 *
 * @param num_bits 当前位数
 * @param state 输出：熔断状态
 * @return true 超过阈值，需要触发熔断
 */
bool bit_burning_check(size_t num_bits, BitBurningState *state) {
    if (!state)
        return false;

    if (num_bits > BIT_CUTOFF_THRESHOLD) {
        state->tripped = true;
        state->bit_count = (uint64_t) num_bits;
        state->consecutive_trips++;
        snprintf(state->reason, sizeof(state->reason), "中间结果位数 %zu 超过阈值 %d", num_bits, BIT_CUTOFF_THRESHOLD);
        return true;
    }

    return false;
}

/**
 * @brief 设置冻结点（在可能触发熔断的操作前调用）
 *
 * 保存当前约束图的完整状态快照（使用 JSON 序列化）。
 *
 * @param graph 约束图
 * @param state 熔断状态
 */
void bit_burning_set_checkpoint(ConstraintGraph *graph, BitBurningState *state) {
    if (!graph || !state)
        return;

    /* 记录当前节点和约束数量 */
    state->checkpoint_node_count = graph->node_count;
    state->checkpoint_constraint_count = graph->constraint_count;

    /* 通过 JSON 序列化创建完整快照 */
    char *json = graph_serialize_to_json(graph);
    if (json) {
        /* 释放旧的检查点 */
        if (g_checkpoint_json) {
            lv_free((void **) &g_checkpoint_json);
        }
        g_checkpoint_json = json;
    }
}

/**
 * @brief 回退到冻结点
 *
 * 从 JSON 快照反序列化恢复约束图。
 *
 * @param graph 约束图
 * @param state 熔断状态
 * @return true 回退成功
 */
bool bit_burning_rollback(ConstraintGraph *graph, BitBurningState *state) {
    if (!graph || !state)
        return false;

    if (!g_checkpoint_json) {
        /* 没有可用检查点，只能通过删除节点回退 */
        if (state->checkpoint_node_count == 0 && state->checkpoint_constraint_count == 0) {
            return false;
        }

        /* 回退方式：删除冻结点之后添加的节点和约束
         * 注意：这只能处理简单的情况（无交叉引用和复杂依赖） */
        while (graph->constraint_count > state->checkpoint_constraint_count) {
            int idx = graph->constraint_count - 1;
            if (graph->constraints[idx]) {
                graph_remove_constraint(graph, graph->constraints[idx]->id);
            }
        }

        while (graph->node_count > state->checkpoint_node_count) {
            int idx = graph->node_count - 1;
            if (graph->nodes[idx]) {
                graph_remove_node(graph, graph->nodes[idx]->id);
            }
        }

        return true;
    }

    /* 使用 JSON 快照完整恢复
     * 注意：此方法会重建整个图，所有已有的图指针会失效。
     * 调用方需确保外部引用已更新。 */
    ConstraintGraph *restored = graph_deserialize_from_json(g_checkpoint_json);
    if (!restored)
        return false;

    /* 用恢复的图替换当前图 */
    /* 由于 ConstraintGraph 是指针，调用者需要知道图已经替换。
     * 我们在这里就地移植数据，使得外部 graph 指针仍然有效。 */

    /* 1. 释放当前图的内部数据 */
    for (int i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i]) {
            /* 经统一节点释放路径 node_destroy 释放：内部字段（symbolic_coords /
             * numeric_assumption_declaration）+ vtable->free 类型特定 union 数据
             * （port / region.boundary_segments / circle / func_block 各数组）+ 节点外壳；
             * 与原先手写的"清理 coords + 释放 decl + 按 type switch 释放 union"语义逐字等价，
             * 且外壳经 lv_pool_free 对非池分配自动按普通分配释放 */
            node_destroy(graph->nodes[i]);
        }
    }
    lv_free((void **) &graph->nodes);
    lv_free((void **) &graph->node_index);

    for (int i = 0; i < graph->constraint_count; i++) {
        if (graph->constraints[i]) {
            lv_free((void **) &graph->constraints[i]->participants);
            lv_free((void **) &graph->constraints[i]);
        }
    }
    lv_free((void **) &graph->constraints);
    lv_free((void **) &graph->constraint_index);

    /* 2. 从恢复的图中移植数据 */
    graph->nodes = restored->nodes;
    graph->node_count = restored->node_count;
    graph->node_capacity = restored->node_capacity;
    graph->node_index = restored->node_index;
    graph->node_index_capacity = restored->node_index_capacity;
    graph->next_node_id = restored->next_node_id;

    graph->constraints = restored->constraints;
    graph->constraint_count = restored->constraint_count;
    graph->constraint_capacity = restored->constraint_capacity;
    graph->constraint_index = restored->constraint_index;
    graph->constraint_index_capacity = restored->constraint_index_capacity;
    graph->next_constraint_id = restored->next_constraint_id;

    graph->dirty = restored->dirty;

    /* 3. 释放 restored 外壳（不释放内部数据，已移植） */
    lv_free((void **) &restored->error_buffer);
    lv_free((void **) &restored->serialize_buffer);
    lv_free((void **) &restored);

    /* 4. 重置熔断状态 */
    state->tripped = false;
    g_checkpoint_json = NULL;

    return true;
}

/**
 * @brief 永久降级为数值假设
 *
 * 将节点标记为 TRUST_AMBER，存储数值假设声明。
 * 被标记节点的下游节点自动继承 TRUST_AMBER。
 *
 * @param graph 约束图
 * @param node_id 节点 ID
 * @param precision 数值精度阈值
 * @param declaration 声明文本
 * @return true 降级成功
 */
bool bit_burning_downgrade_to_amber(ConstraintGraph *graph, int node_id, double precision, const char *declaration) {
    if (!graph)
        return false;

    GeomNode *node = graph_get_node(graph, node_id);
    if (!node)
        return false;

    /* 设置信任颜色为 TRUST_AMBER */
    node->trust = TRUST_AMBER;

    /* 设置数值精度阈值 */
    node->numeric_precision = precision;

    /* 设置数值假设声明 */
    if (node->numeric_assumption_declaration) {
        lv_free((void **) &node->numeric_assumption_declaration);
    }
    if (declaration) {
        node->numeric_assumption_declaration = lv_strdup(declaration);
    } else {
        node->numeric_assumption_declaration = NULL;
    }

    /* 传播到所有依赖节点 */
    bit_burning_propagate_downgrade(graph, node_id);

    return true;
}

/**
 * @brief 执行熔断操作
 *
 * 根据连续熔断次数和用户选择执行相应操作。
 *
 * @param graph 约束图
 * @param node_id 触发熔断的节点 ID
 * @param state 熔断状态
 * @param action 用户选择的操作
 * @return true 操作成功
 */
bool bit_burning_execute(ConstraintGraph *graph, int node_id, BitBurningState *state, BurningAction action) {
    if (!graph || !state)
        return false;

    switch (action) {
        case BURN_ACTION_IGNORE:
            /* 标记节点为"数值辅助"，但不传播标记
             * 后续通过 bit_burning_is_blocked() 检查是否阻断传播 */
            {
                GeomNode *node = graph_get_node(graph, node_id);
                if (node) {
                    node->trust = TRUST_AMBER;
                }
            }
            state->tripped = false;
            return true;

        case BURN_ACTION_ROLLBACK:
            return bit_burning_rollback(graph, state);

        case BURN_ACTION_DOWNGRADE: {
            char decl[256];
            snprintf(decl, sizeof(decl), "位熔断降级: 连续触发 %d 次, 位数 %" PRIu64, state->consecutive_trips,
                     state->bit_count);
            return bit_burning_downgrade_to_amber(graph, node_id, lv_EPSILON_ULTRA, decl);
        }

        default:
            return false;
    }
}

/**
 * @brief 检查下游传播是否被阻断
 *
 * 当一个"数值辅助"节点被后续构造引用时，
 * 检查是否可以安全传播。
 *
 * @param graph 约束图
 * @param source_node_id 源节点 ID
 * @param target_node_id 目标节点 ID
 * @return true 传播被阻断，false 可以安全传播
 */
bool bit_burning_is_blocked(ConstraintGraph *graph, int source_node_id, int target_node_id) {
    if (!graph)
        return false;

    GeomNode *source = graph_get_node(graph, source_node_id);
    if (!source)
        return false;

    /* 如果源节点不是 TRUST_AMBER，不阻断 */
    if (source->trust != TRUST_AMBER)
        return false;

    GeomNode *target = graph_get_node(graph, target_node_id);
    if (!target)
        return false;

    /* 如果目标节点已经降级，不需要阻断 */
    if (target->trust == TRUST_AMBER)
        return false;

    /* 检查目标节点的类型：纯符号运算不需要数值近似，不阻断 */
    /* 向量/代数结构需要精确表示，如果源是数值假设，则阻断传播 */
    switch (target->type) {
        case GEOM_POINT:
            /* 点节点如果依赖数值假设的坐标，阻断 */
            if (target->coord_count > 0) {
                for (int i = 0; i < target->coord_count; i++) {
                    if (target->symbolic_coords[i] && target->symbolic_coords[i]->trust == TRUST_AMBER) {
                        return true;
                    }
                }
            }
            return false;

        case GEOM_LINE_SEGMENT:
        case GEOM_REGION:
        case GEOM_CIRCLE:
        case GEOM_PORT:
        case GEOM_FUNCTION_BLOCK:
            /* 这些类型可能依赖数值计算，阻断 */
            return true;

        default:
            return false;
    }
}

/**
 * @brief 永久降级的自动传播
 *
 * 当一个节点被降级为 TRUST_AMBER 后，
 * 所有依赖它的下游节点自动继承 TRUST_AMBER。
 *
 * @param graph 约束图
 * @param node_id 已降级节点 ID
 */
void bit_burning_propagate_downgrade(ConstraintGraph *graph, int node_id) {
    if (!graph)
        return;

    /* 遍历所有约束，找到引用 node_id 的约束 */
    /* 通过这些约束找到依赖该节点的下游节点 */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *con = graph->constraints[i];
        if (!con || !con->is_active)
            continue;

        /* 检查该约束是否引用了目标节点 */
        bool references_target = false;
        for (int j = 0; j < con->participant_count; j++) {
            if (con->participants[j] == node_id) {
                references_target = true;
                break;
            }
        }

        if (!references_target)
            continue;

        /* 对约束中的所有参与者，如果它们不是源节点且不是已降级的，递归降级 */
        for (int j = 0; j < con->participant_count; j++) {
            int pid = con->participants[j];
            if (pid == node_id)
                continue;

            GeomNode *pn = graph_get_node(graph, pid);
            if (!pn)
                continue;

            /* 跳过已降级或已标记的节点 */
            if (pn->trust == TRUST_AMBER)
                continue;

            /* 降级此节点 */
            pn->trust = TRUST_AMBER;

            /* 设置默认声明 */
            if (!pn->numeric_assumption_declaration) {
                char decl[128];
                snprintf(decl, sizeof(decl), "自动传播降级: 从节点 %d 继承", node_id);
                pn->numeric_assumption_declaration = lv_strdup(decl);
            }

            /* 递归传播 */
            bit_burning_propagate_downgrade(graph, pid);
        }
    }

    /* 另外：遍历所有节点，检查是否有 symbolic_coords 引用了降级节点的坐标 */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *n = graph->nodes[i];
        if (!n || n->id == node_id || n->trust == TRUST_AMBER)
            continue;

        if (n->coord_count > 0 && n->symbolic_coords) {
            for (int j = 0; j < n->coord_count; j++) {
                if (n->symbolic_coords[j] && n->symbolic_coords[j]->trust == TRUST_AMBER) {
                    n->trust = TRUST_AMBER;

                    if (!n->numeric_assumption_declaration) {
                        char decl[128];
                        snprintf(decl, sizeof(decl), "坐标继承降级: 从节点 %d", node_id);
                        n->numeric_assumption_declaration = lv_strdup(decl);
                    }
                    break;
                }
            }
        }
    }
}

/**
 * @brief 获取线程局部全局熔断状态
 *
 * @return BitBurningState* 指向全局熔断状态的指针
 */
BitBurningState *bit_burning_get_global_state(void) {
    return &g_bit_burning_state;
}

/**
 * @file func_block.c
 * @brief 函数块核心实现
 * @details 实现函数块的创建、销毁、打包、深拷贝等核心管理 API。
 *          确定性检查见 func_block_determinism.c，
 *          例化与捕获避免见 func_block_instantiate.c，
 *          序列化/反序列化见 func_block_serialize.c。
 *
 * @author Lv-00 Project
 * @version 3.2.0
 */

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "func_block.h"
#include "func_block_internal.h"
#include "lv00_internal.h"
#include "lv00_utils.h"
#include "solver.h"
#include "stream.h"
#include "stream_context_util.h"

/* ==================== 命名常量 ==================== */

/** 函数块数组扩容的增长因子（与 LV00_ARRAY_GROWTH_FACTOR 保持一致） */
#define FUNC_BLOCK_ARRAY_GROWTH_FACTOR 2
/** 函数块默认容量 */
#define FUNC_BLOCK_DEFAULT_CAPACITY 8

/* 流式上下文定义（非 static，供其他子模块文件 extern 引用） */
LV00_THREAD_LOCAL StreamContext *func_block_stream_ctx = NULL;

/**
 * @brief 设置函数块模块的流式上下文
 *
 * @param ctx 流式上下文指针（可为 NULL，表示清除上下文）
 */
void func_block_set_stream_context(StreamContext *ctx) {
    func_block_stream_ctx = ctx;
}

/* ============== 函数块管理API ============== */

/**
 * @brief 创建函数块
 *
 * 分配并初始化一个 FuncBlock 结构体。新块的初始状态：
 * - 确定性状态设为 DETERMINISM_UNVERIFIED（未经检验）
 * - 内部节点数组、输入/输出端口数组、端口依赖数组均为空
 * - 选择器、名称、描述、测度比较函数均为 NULL
 * - 视图状态设为 FB_VIEW_EXPANDED（展开）
 *
 * @param id 函数块唯一标识符
 * @return 成功返回 FuncBlock 指针（调用方负责通过 func_block_destroy 释放），
 *         内存不足时返回 NULL
 */
FuncBlock *func_block_create(int id) {
    FuncBlock *fb = lv00_malloc(sizeof(FuncBlock));
    if (!fb) return NULL;
    memset(fb, 0, sizeof(FuncBlock));
    fb->id = id;
    fb->determinism = DETERMINISM_UNVERIFIED;
    fb->selector = NULL;
    fb->name = NULL;
    fb->description = NULL;
    fb->has_measure = false;
    fb->measure_node_id = -1;
    fb->measure_compare = NULL;
    fb->view_state = FB_VIEW_EXPANDED;
    return fb;
}

/**
 * @brief 销毁函数块
 *
 * 释放 FuncBlock 结构体及其所有动态分配的成员：
 * - internal_node_ids（内部节点ID数组）
 * - input_port_ids / output_port_ids（输入/输出端口ID数组）
 * - port_deps（端口依赖数组）
 * - precondition_region_ids（前置条件区域ID数组）
 * - selector（选择器对象，通过 selector_destroy 递归释放）
 * - name / description（名称和描述字符串）
 *
 * 参数为 NULL 时安全返回（no-op），无需调用方判空。
 *
 * @param fb 函数块指针，可为 NULL
 */
void func_block_destroy(FuncBlock *fb) {
    if (!fb) return;
    lv00_free((void **)&fb->internal_node_ids);
    lv00_free((void **)&fb->input_port_ids);
    lv00_free((void **)&fb->output_port_ids);
    lv00_free((void **)&fb->port_deps);
    lv00_free((void **)&fb->precondition_region_ids);
    if (fb->selector) {
        selector_destroy(fb->selector);
        /* 修复：释放后置 NULL，防止悬空指针风险。
         * 虽然 fb 本身即将被释放，但防御性编程可避免未来重构引入 use-after-free */
        fb->selector = NULL;
    }
    lv00_free((void **)&fb->name);
    lv00_free((void **)&fb->description);
    lv00_free((void **)&fb);
}

/**
 * @brief 设置函数块的内部节点列表
 *
 * 内部节点是函数块"封装"的几何节点——这些节点位于块内部，
 * 外部不可见。函数块作为这些节点的抽象边界。
 *
 * 操作逻辑：
 * - 释放旧的 internal_node_ids 数组（如果存在）
 * - 调用 dup_int_array 深拷贝 node_ids 到新数组
 * - 更新 internal_node_count
 *
 * @param fb      函数块指针（不可为 NULL）
 * @param node_ids 内部节点 ID 数组（可为 NULL，当 count=0 时）
 * @param count   节点数量
 * @return true  设置成功
 * @return false fb 为 NULL 或 count<0 或内存分配失败
 */
bool func_block_set_internal_nodes(FuncBlock *fb, const int *node_ids, int count) {
    if (!fb || count < 0) return false;
    lv00_free((void **)&fb->internal_node_ids);
    fb->internal_node_ids = dup_int_array(node_ids, count);
    if (count > 0 && !fb->internal_node_ids) return false;
    fb->internal_node_count = count;
    return true;
}

/**
 * @brief 设置函数块的输入端口列表
 *
 * 输入端口是函数块从外部接收数据/坐标的入口。当函数块被例化时，
 * 外部节点通过绑定到这些端口向块内传递几何信息。
 *
 * 操作逻辑：
 * - 释放旧的 input_port_ids 数组（如果存在）
 * - 调用 dup_int_array 深拷贝 port_ids 到新数组
 * - 更新 input_count
 *
 * @param fb       函数块指针（不可为 NULL）
 * @param port_ids 输入端口 ID 数组（可为 NULL，当 count=0 时）
 * @param count    端口数量
 * @return true  设置成功
 * @return false fb 为 NULL 或 count<0 或内存分配失败
 */
bool func_block_set_input_ports(FuncBlock *fb, const int *port_ids, int count) {
    if (!fb || count < 0) return false;
    lv00_free((void **)&fb->input_port_ids);
    fb->input_port_ids = dup_int_array(port_ids, count);
    if (count > 0 && !fb->input_port_ids) return false;
    fb->input_count = count;
    return true;
}

/**
 * @brief 设置函数块的输出端口列表
 *
 * 输出端口是函数块向外部暴露计算结果的出口。当函数块被执行后，
 * 输出端口上的节点被解析/构造完成，供外部引用。
 *
 * 操作逻辑：
 * - 释放旧的 output_port_ids 数组（如果存在）
 * - 调用 dup_int_array 深拷贝 port_ids 到新数组
 * - 更新 output_count
 *
 * @param fb       函数块指针（不可为 NULL）
 * @param port_ids 输出端口 ID 数组（可为 NULL，当 count=0 时）
 * @param count    端口数量
 * @return true  设置成功
 * @return false fb 为 NULL 或 count<0 或内存分配失败
 */
bool func_block_set_output_ports(FuncBlock *fb, const int *port_ids, int count) {
    if (!fb || count < 0) return false;
    lv00_free((void **)&fb->output_port_ids);
    fb->output_port_ids = dup_int_array(port_ids, count);
    if (count > 0 && !fb->output_port_ids) return false;
    fb->output_count = count;
    return true;
}

/**
 * @brief 设置函数块名称
 *
 * @param fb   函数块指针（不可为 NULL）
 * @param name 名称字符串（可为 NULL，表示清除名称）
 * @return true  设置成功
 * @return false 失败
 */
bool func_block_set_name(FuncBlock *fb, const char *name) {
    if (!fb) return false;
    lv00_free((void **)&fb->name);
    if (name && name[0] != '\0') {
        fb->name = lv00_strdup(name);
        if (!fb->name) return false;
    }
    return true;
}

/**
 * @brief 设置函数块描述
 *
 * @param fb          函数块指针（不可为 NULL）
 * @param description 描述字符串（可为 NULL，表示清除描述）
 * @return true  设置成功
 * @return false 失败
 */
bool func_block_set_description(FuncBlock *fb, const char *description) {
    if (!fb) return false;
    lv00_free((void **)&fb->description);
    if (description && description[0] != '\0') {
        fb->description = lv00_strdup(description);
        if (!fb->description) return false;
    }
    return true;
}

/* ============== Getter 函数实现 ============== */

/**
 * @brief 获取输入端口数量
 *
 * 安全访问函数块的输入端口计数。支持 NULL 安全检查。
 *
 * @param fb 函数块指针（可为 NULL）
 * @return 输入端口数量，fb 为 NULL 时返回 0
 */
int func_block_get_input_count(const FuncBlock *fb) {
    return fb ? fb->input_count : 0;
}

/**
 * @brief 获取输出端口数量
 *
 * 安全访问函数块的输出端口计数。支持 NULL 安全检查。
 *
 * @param fb 函数块指针（可为 NULL）
 * @return 输出端口数量，fb 为 NULL 时返回 0
 */
int func_block_get_output_count(const FuncBlock *fb) {
    return fb ? fb->output_count : 0;
}

/**
 * @brief 获取内部节点数量
 *
 * 安全访问函数块的内部节点计数。支持 NULL 安全检查。
 *
 * @param fb 函数块指针（可为 NULL）
 * @return 内部节点数量，fb 为 NULL 时返回 0
 */
int func_block_get_internal_count(const FuncBlock *fb) {
    return fb ? fb->internal_node_count : 0;
}

/**
 * @brief 获取函数块ID
 *
 * 安全访问函数块的唯一标识符。支持 NULL 安全检查。
 *
 * @param fb 函数块指针（可为 NULL）
 * @return 函数块ID，fb 为 NULL 时返回 -1
 */
int func_block_get_id(const FuncBlock *fb) {
    return fb ? fb->id : -1;
}

/**
 * @brief 获取确定性状态
 *
 * 安全访问函数块的确定性状态。支持 NULL 安全检查。
 * 确定性状态用于判断函数块是否产生唯一解。
 *
 * @param fb 函数块指针（可为 NULL）
 * @return 确定性状态，fb 为 NULL 时返回 DETERMINISM_UNVERIFIED
 */
DeterminismState func_block_get_determinism(const FuncBlock *fb) {
    return fb ? fb->determinism : DETERMINISM_UNVERIFIED;
}

/**
 * @brief 获取函数块名称
 *
 * 安全访问函数块的名称字符串。返回的字符串是只读的，
 * 调用者不应修改或释放。
 *
 * @param fb 函数块指针（可为 NULL）
 * @return 名称字符串（只读），fb 为 NULL 或无名称时返回 NULL
 */
const char *func_block_get_name(const FuncBlock *fb) {
    return fb ? fb->name : NULL;
}

/**
 * @brief 获取函数块描述
 *
 * 安全访问函数块的描述字符串。返回的字符串是只读的，
 * 调用者不应修改或释放。
 *
 * @param fb 函数块指针（可为 NULL）
 * @return 描述字符串（只读），fb 为 NULL 或无描述时返回 NULL
 */
const char *func_block_get_description(const FuncBlock *fb) {
    return fb ? fb->description : NULL;
}

/**
 * @brief 设置函数块的选择器
 *
 * @param fb       函数块指针（不可为 NULL）
 * @param selector 选择器指针（可为 NULL，表示清除选择器）
 * @return true  设置成功
 * @return false 失败
 */
bool func_block_set_selector(FuncBlock *fb, SolutionSelector *selector) {
    if (!fb) return false;
    if (fb->selector) {
        selector_destroy(fb->selector);
    }
    fb->selector = selector;
    return true;
}

/**
 * @brief 添加端口依赖
 *
 * @param fb  函数块指针（不可为 NULL）
 * @param dep 端口依赖指针（不可为 NULL）
 * @return true  添加成功
 * @return false 失败
 */
bool func_block_add_port_dependency(FuncBlock *fb, PortDependency *dep) {
    if (!fb || !dep) return false;
    /* 使用 lv00_realloc 统一内存管理，确保内存追踪系统可以追踪此分配 */
    int new_count = fb->port_dep_count + 1;
    PortDependency *new_deps = lv00_realloc(fb->port_deps, (size_t)new_count * sizeof(PortDependency));
    if (!new_deps) return false;
    fb->port_deps = new_deps;
    fb->port_deps[fb->port_dep_count] = *dep;
    fb->port_dep_count = new_count;
    return true;
}

/**
 * @brief 设置函数块的前置条件
 *
 * @param fb        函数块指针（不可为 NULL）
 * @param region_ids 前置条件区域 ID 数组（可为 NULL，当 count=0 时）
 * @param count     区域数量
 * @return true  设置成功
 * @return false 失败
 */
bool func_block_set_preconditions(FuncBlock *fb, const int *region_ids, int count) {
    if (!fb || count < 0) return false;
    lv00_free((void **)&fb->precondition_region_ids);
    fb->precondition_region_ids = dup_int_array(region_ids, count);
    if (count > 0 && !fb->precondition_region_ids) return false;
    fb->precondition_count = count;
    return true;
}

/* ============== 跨边界检测 ============== */

/**
 * @brief 检测跨边界约束
 *
 * @param graph               约束图
 * @param internal_node_ids   内部节点 ID 数组
 * @param internal_count     内部节点数量
 * @param out_conflicts      输出冲突数组
 * @param out_conflict_count 输出冲突数量
 * @return true  检测到冲突，false 未检测到或参数无效
 */
bool func_block_detect_cross_boundary(
    ConstraintGraph *graph,
    const int *internal_node_ids,
    int internal_count,
    CrossBoundaryConstraint **out_conflicts,
    int *out_conflict_count)
{
    if (!graph || !internal_node_ids || !out_conflicts || !out_conflict_count) {
        if (out_conflict_count) *out_conflict_count = 0;
        if (out_conflicts) *out_conflicts = NULL;
        return false;
    }

    /* 使用 constraint_graph.h 中定义的 find_cross_boundary_constraints 函数 */
    /* 但我们需要合并端口节点到内部节点集合中 */
    CrossBoundaryConstraint *conflicts = find_cross_boundary_constraints(
        graph, internal_node_ids, internal_count, NULL, 0, out_conflict_count);

    *out_conflicts = conflicts;

    if (*out_conflict_count > 0 && func_block_stream_ctx) {
        stream_emit_simple(func_block_stream_ctx,
                           STREAM_EVENT_FUNC_BLOCK_CROSS_BOUNDARY,
                           "检测到跨边界端口依赖",
                           -1);
    }

    return *out_conflict_count > 0;
}

/* ============== 打包操作 ============== */

/**
 * @brief 打包函数块
 *
 * 将内部节点、输入端口和输出端口打包为一个函数块。
 *
 * @param graph               约束图
 * @param internal_node_ids   内部节点 ID 数组
 * @param internal_count      内部节点数量
 * @param input_port_ids      输入端口 ID 数组
 * @param input_count         输入端口数量
 * @param output_port_ids     输出端口 ID 数组
 * @param output_count        输出端口数量
 * @param cross_boundary_actions 跨边界处理动作数组
 * @param cross_boundary_count   跨边界处理动作数量
 * @param out_func_block      输出参数，返回新创建的函数块
 * @return 打包结果状态码
 */
PackResult func_block_pack(
    ConstraintGraph *graph,
    const int *internal_node_ids,
    int internal_count,
    const int *input_port_ids,
    int input_count,
    const int *output_port_ids,
    int output_count,
    CrossBoundaryAction *cross_boundary_actions,
    int cross_boundary_count,
    FuncBlock **out_func_block)
{
    if (!graph || !out_func_block) return PACK_INVALID_NODES;

    /* 流式事件：函数块打包开始 */
    if (func_block_stream_ctx) {
        stream_emit_simple(func_block_stream_ctx, STREAM_EVENT_FUNC_BLOCK_PACK_START,
            "函数块打包开始", 0);
    }

    /* 参数基本验证 */
    if (internal_count > 0 && !internal_node_ids) return PACK_INVALID_NODES;
    if (input_count > 0 && !input_port_ids) return PACK_INVALID_PORTS;
    if (output_count > 0 && !output_port_ids) return PACK_INVALID_PORTS;

    /* 验证所有内部节点存在 */
    for (int i = 0; i < internal_count; i++) {
        GeomNode *n = graph_get_node(graph, internal_node_ids[i]);
        if (!n) return PACK_INVALID_NODES;
    }

    /* 验证输入端口 */
    for (int i = 0; i < input_count; i++) {
        GeomNode *n = graph_get_node(graph, input_port_ids[i]);
        if (!n || n->type != GEOM_PORT) return PACK_INVALID_PORTS;
    }

    /* 验证输出端口 */
    for (int i = 0; i < output_count; i++) {
        GeomNode *n = graph_get_node(graph, output_port_ids[i]);
        if (!n || n->type != GEOM_PORT) return PACK_INVALID_PORTS;
    }

    /* 检测跨边界约束 */
    CrossBoundaryConstraint *conflicts = NULL;
    int conflict_count = 0;

    /* 合并内部节点和端口用于跨边界检测 */
    /* 使用安全加法宏防止 total_bound 计算整数溢出。
     * 注意：嵌套调用 LV00_SAFE_ADD 时，若第一次加法溢出返回 INT_MAX，
     * 第二次再加第三个值会再次溢出仍返回 INT_MAX，因此必须逐级检查。
     * 修复：拆分为两次独立的安全加法，每次都检查溢出结果。 */
    int partial = LV00_SAFE_ADD(internal_count, input_count, INT_MAX);
    if (partial == INT_MAX) return PACK_OUT_OF_MEMORY;
    int total_bound = LV00_SAFE_ADD(partial, output_count, INT_MAX);
    if (total_bound == INT_MAX) return PACK_OUT_OF_MEMORY;
    int *bound_ids = NULL;
    if (total_bound > 0) {
        bound_ids = lv00_malloc((size_t)total_bound * sizeof(int));
        if (!bound_ids) return PACK_OUT_OF_MEMORY;
        int bidx = 0;
        for (int i = 0; i < internal_count; i++)
            bound_ids[bidx++] = internal_node_ids[i];
        for (int i = 0; i < input_count; i++)
            bound_ids[bidx++] = input_port_ids[i];
        for (int i = 0; i < output_count; i++)
            bound_ids[bidx++] = output_port_ids[i];
        total_bound = bidx;
    }

    /* 使用 constraint_graph.h 的函数检测跨边界约束 */
    conflicts = find_cross_boundary_constraints(
        graph, bound_ids ? bound_ids : internal_node_ids,
        bound_ids ? total_bound : internal_count,
        NULL, 0, &conflict_count);

    if (conflict_count > 0) {
        /* 流式事件：打包过程中检测到跨边界约束 */
        if (func_block_stream_ctx) {
            stream_emit_simple(func_block_stream_ctx,
                               STREAM_EVENT_FUNC_BLOCK_CROSS_BOUNDARY,
                               "打包检测到跨边界约束，开始处理",
                               0);
        }

        if (!cross_boundary_actions || cross_boundary_count < conflict_count) {
            /* 存在跨边界约束但未提供足够的处理方式 */
            lv00_free((void **)&conflicts);
            lv00_free((void **)&bound_ids);
            return PACK_CROSS_BOUNDARY_CONFLICT;
        }

        /* 处理每条跨边界约束 */
        for (int i = 0; i < conflict_count; i++) {
            CrossBoundaryAction action = cross_boundary_actions[i];
            switch (action) {
                case CROSS_BOUNDARY_CANCEL:
                    lv00_free((void **)&conflicts);
                    lv00_free((void **)&bound_ids);
                    return PACK_CANCELLED;

                case CROSS_BOUNDARY_DISCONNECT:
                    /* 断开：删除该约束 */
                    for (int ci = 0; ci < graph->constraint_count; ci++) {
                        if (graph->constraints[ci]->id == conflicts[i].constraint_id) {
                            graph_remove_constraint(graph, ci);
                            break;
                        }
                    }
                    break;

                case CROSS_BOUNDARY_PROMOTE:
                    /* 提升：约束保留，记录为端口依赖（后续处理） */
                    break;
            }
        }
        lv00_free((void **)&conflicts);
    }
    lv00_free((void **)&bound_ids);

    /* 在图中创建 GEOM_FUNCTION_BLOCK 节点 */
    AddNodeResult add_result = graph_add_function_block(
        graph, internal_node_ids, internal_count,
        input_port_ids, input_count,
        output_port_ids, output_count);
    if (add_result != ADD_NODE_OK) {
        return PACK_OUT_OF_MEMORY;
    }

    /* 函数块ID = 新创建节点的ID
     *
     * v10.0 修复：使用 graph_get_last_added_node_id() 公共接口替代脆弱的
     * graph->next_node_id - 1 内部实现假设。该接口由 constraint_graph.c 提供，
     * 确保无论内部实现如何变化都能正确获取最后添加的节点 ID。
     */
    int fb_id = graph_get_last_added_node_id(graph);
    if (fb_id < 0) {
        LV00_LOG_ERROR("func_block_pack: graph_get_last_added_node_id() 返回 %d，无法推断函数块ID",
                       fb_id);
        return PACK_OUT_OF_MEMORY;
    }

    /* 创建 FuncBlock 结构 */
    FuncBlock *fb = func_block_create(fb_id);
    if (!fb) {
        /* func_block_create 失败时，需要从图中移除已添加的函数块节点，
         * 避免资源泄漏（图中残留无主节点） */
        /* 修复：检查 graph_remove_node 返回值，确保节点确实被移除 */
        if (graph_remove_node(graph, fb_id) != REMOVE_NODE_OK) {
            LV00_LOG_WARNING("func_block_pack: graph_remove_node(%d) 失败，图中可能残留无主节点", fb_id);
        }
        return PACK_OUT_OF_MEMORY;
    }

    if (!func_block_set_internal_nodes(fb, internal_node_ids, internal_count)) {
        /* 修复：检查 graph_remove_node 返回值，确保节点确实被移除 */
        if (graph_remove_node(graph, fb_id) != REMOVE_NODE_OK) {
            LV00_LOG_WARNING("func_block_pack: graph_remove_node(%d) 失败，图中可能残留无主节点", fb_id);
        }
        func_block_destroy(fb);
        return PACK_OUT_OF_MEMORY;
    }
    if (!func_block_set_input_ports(fb, input_port_ids, input_count)) {
        /* 修复：检查 graph_remove_node 返回值，确保节点确实被移除 */
        if (graph_remove_node(graph, fb_id) != REMOVE_NODE_OK) {
            LV00_LOG_WARNING("func_block_pack: graph_remove_node(%d) 失败，图中可能残留无主节点", fb_id);
        }
        func_block_destroy(fb);
        return PACK_OUT_OF_MEMORY;
    }
    if (!func_block_set_output_ports(fb, output_port_ids, output_count)) {
        /* 修复：检查 graph_remove_node 返回值，确保节点确实被移除 */
        if (graph_remove_node(graph, fb_id) != REMOVE_NODE_OK) {
            LV00_LOG_WARNING("func_block_pack: graph_remove_node(%d) 失败，图中可能残留无主节点", fb_id);
        }
        func_block_destroy(fb);
        return PACK_OUT_OF_MEMORY;
    }

    /* ---- 设计文档 3.2: 更新内部节点 ---- */
    /* namespace_depth 重计算：新深度 = 原深度 - 原上下文深度 + 1 */
    /* 原上下文深度取第一个内部节点的 namespace_depth 作为基准 */
    /* 如果内部节点有不同的上下文深度，以最小值为准 */
    int context_depth = 0;
    if (internal_count > 0) {
        GeomNode *first_node = graph_get_node(graph, internal_node_ids[0]);
        if (first_node) {
            context_depth = first_node->namespace_depth;
            /* 取所有内部节点的最小 namespace_depth 作为上下文深度 */
            for (int i = 1; i < internal_count; i++) {
                GeomNode *n = graph_get_node(graph, internal_node_ids[i]);
                if (n && n->namespace_depth < context_depth) {
                    context_depth = n->namespace_depth;
                }
            }
        }
    }

    for (int i = 0; i < internal_count; i++) {
        GeomNode *n = graph_get_node(graph, internal_node_ids[i]);
        if (n) {
            n->namespace_depth = n->namespace_depth - context_depth + 1;
            n->parent_block_id = fb_id;
        }
    }

    /* 输入端口：标记 is_formal_param=true，更新 namespace_depth 和 parent_block_id */
    for (int i = 0; i < input_count; i++) {
        GeomNode *n = graph_get_node(graph, input_port_ids[i]);
        if (n && n->type == GEOM_PORT && n->data.port) {
            n->data.port->parent_block_id = fb_id;
            n->data.port->is_formal_param = true;
            n->data.port->namespace_depth = n->data.port->namespace_depth - context_depth + 1;
            n->parent_block_id = fb_id;
            n->namespace_depth = n->namespace_depth - context_depth + 1;
        }
    }

    /* 输出端口：is_formal_param=false（输出不是形式参数），更新归属 */
    for (int i = 0; i < output_count; i++) {
        GeomNode *n = graph_get_node(graph, output_port_ids[i]);
        if (n && n->type == GEOM_PORT && n->data.port) {
            n->data.port->parent_block_id = fb_id;
            n->data.port->is_formal_param = false;
            n->data.port->namespace_depth = n->data.port->namespace_depth - context_depth + 1;
            n->parent_block_id = fb_id;
            n->namespace_depth = n->namespace_depth - context_depth + 1;
        }
    }

    /* 流式事件：函数块打包完成 */
    if (func_block_stream_ctx) {
        stream_emit_simple(func_block_stream_ctx, STREAM_EVENT_FUNC_BLOCK_PACK_DONE,
            "函数块打包完成", 0);
    }
    *out_func_block = fb;
    return PACK_OK;
}

/* ============== 辅助函数 ============== */

/**
 * @brief 将确定性状态转换为字符串
 *
 * @param state 确定性状态枚举值
 * @return 对应的字符串表示
 */
const char *determinism_state_to_string(DeterminismState state) {
    switch (state) {
        case DETERMINISM_UNVERIFIED:         return "UNVERIFIED";
        case DETERMINISM_VERIFIED:           return "VERIFIED";
        case DETERMINISM_NON_DETERMINISTIC:  return "NON_DETERMINISTIC";
        case DETERMINISM_PARTIALLY_VERIFIED: return "PARTIALLY_VERIFIED";
        default:                             return "UNKNOWN";
    }
}

/**
 * @brief 将打包结果转换为字符串
 *
 * @param result 打包结果枚举值
 * @return 对应的字符串表示
 */
const char *pack_result_to_string(PackResult result) {
    switch (result) {
        case PACK_OK:                      return "OK";
        case PACK_CROSS_BOUNDARY_CONFLICT: return "CROSS_BOUNDARY_CONFLICT";
        case PACK_INVALID_NODES:           return "INVALID_NODES";
        case PACK_INVALID_PORTS:           return "INVALID_PORTS";
        case PACK_OUT_OF_MEMORY:           return "OUT_OF_MEMORY";
        case PACK_CANCELLED:               return "CANCELLED";
        default:                           return "UNKNOWN";
    }
}

/**
 * @brief 将例化结果转换为字符串
 *
 * @param result 例化结果枚举值
 * @return 对应的字符串表示
 */
const char *instantiate_result_to_string(InstantiateResult result) {
    switch (result) {
        case INSTANTIATE_OK:                  return "OK";
        case INSTANTIATE_NO_SOLUTION:         return "NO_SOLUTION";
        case INSTANTIATE_MULTIPLE_SOLUTIONS:  return "MULTIPLE_SOLUTIONS";
        case INSTANTIATE_SELECTOR_NEEDED:     return "SELECTOR_NEEDED";
        case INSTANTIATE_PRECONDITION_FAILED: return "PRECONDITION_FAILED";
        case INSTANTIATE_OUT_OF_MEMORY:       return "OUT_OF_MEMORY";
        default:                              return "UNKNOWN";
    }
}

/* ============== 视图折叠/展开 ============== */

/**
 * @brief 设置函数块的视图状态
 *
 * @param fb    函数块
 * @param state 视图状态
 */
void func_block_set_view_state(FuncBlock *fb, FuncBlockViewState state) {
    if (fb) {
        fb->view_state = state;
    }
}

/**
 * @brief 获取函数块的视图状态
 *
 * @param fb 函数块
 * @return 视图状态
 */
FuncBlockViewState func_block_get_view_state(const FuncBlock *fb) {
    if (!fb) return FB_VIEW_EXPANDED;
    return fb->view_state;
}

/* ============== 简化版打包API ============== */

/**
 * @brief 执行打包操作（简化版API）
 *
 * 使用 PackConfig 结构体简化参数传递，避免传递过多的独立参数。
 * 这是 func_block_pack 的包装函数，提供更友好的API接口。
 *
 * @param graph 约束图
 * @param config 打包配置
 * @param out_func_block 输出的函数块
 * @return 打包结果
 */
PackResult func_block_pack_ex(
    ConstraintGraph *graph,
    const PackConfig *config,
    FuncBlock **out_func_block)
{
    /* 参数验证 */
    if (!graph || !config || !out_func_block) {
        LV00_ERROR_RETURN(LV00_ERROR_INVALID_PARAM, PACK_INVALID_NODES,
            "无效参数: graph=%p, config=%p, out_func_block=%p",
            (void*)graph, (void*)config, (void*)out_func_block);
    }
    
    /* 验证必需参数 */
    if (!config->internal_node_ids || config->internal_count <= 0) {
        LV00_ERROR_RETURN(LV00_ERROR_INVALID_PARAM, PACK_INVALID_NODES,
            "无效的内部节点: ids=%p, count=%d",
            (void*)config->internal_node_ids, config->internal_count);
    }
    
    if (!config->input_port_ids || config->input_count < 0) {
        LV00_ERROR_RETURN(LV00_ERROR_INVALID_PARAM, PACK_INVALID_PORTS,
            "无效的输入端口: ids=%p, count=%d",
            (void*)config->input_port_ids, config->input_count);
    }
    
    if (!config->output_port_ids || config->output_count < 0) {
        LV00_ERROR_RETURN(LV00_ERROR_INVALID_PARAM, PACK_INVALID_PORTS,
            "无效的输出端口: ids=%p, count=%d",
            (void*)config->output_port_ids, config->output_count);
    }
    
    /* 调用传统API */
    PackResult result = func_block_pack(
        graph,
        config->internal_node_ids,
        config->internal_count,
        config->input_port_ids,
        config->input_count,
        config->output_port_ids,
        config->output_count,
        (CrossBoundaryAction*)config->cross_boundary_actions,
        config->cross_boundary_count,
        out_func_block
    );
    
    /* 设置名称和描述（如果提供了） */
    if (result == PACK_OK && *out_func_block) {
        if (config->name && config->name[0] != '\0') {
            func_block_set_name(*out_func_block, config->name);
        }
        if (config->description && config->description[0] != '\0') {
            func_block_set_description(*out_func_block, config->description);
        }
    }
    
    return result;
}

/* ============== 打包冲突对话框（API层） ============== */

/** 跨边界回调上下文结构体：封装回调和用户数据，确保线程安全 */
typedef struct {
    CrossBoundaryCallback callback;   /**< 回调函数指针 */
    void *user_data;                  /**< 回调用户数据 */
} CrossBoundaryCallbackContext;

static LV00_THREAD_LOCAL CrossBoundaryCallbackContext g_cross_boundary_ctx = {NULL, NULL};

/**
 * @brief 设置跨边界回调
 *
 * @param cb        回调函数
 * @param user_data 用户数据
 */
void func_block_set_cross_boundary_callback(CrossBoundaryCallback cb, void *user_data) {
    g_cross_boundary_ctx.callback = cb;
    g_cross_boundary_ctx.user_data = user_data;
}

/* ============== 深拷贝 ============== */

/**
 * @brief 深拷贝函数块
 *
 * 创建一个函数块的完整深拷贝，包括所有动态分配的成员。
 * 拷贝后的函数块与原始函数块完全独立，修改其中一个不会影响另一个。
 *
 * 深拷贝内容包括：
 * - internal_node_ids（深拷贝数组）
 * - input_port_ids / output_port_ids（深拷贝数组）
 * - port_deps（深拷贝数组）
 * - selector（创建新的选择器并复制字段）
 * - name / description（lv00_strdup）
 * - precondition_region_ids（深拷贝数组）
 * - has_measure / measure_node_id / measure_compare（直接复制）
 * - view_state（直接复制）
 * - determinism（直接复制）
 *
 * @param src 源函数块
 * @return 新创建的函数块副本，失败返回 NULL
 */
FuncBlock *func_block_copy(const FuncBlock *src)
{
    if (!src) return NULL;

    /* 创建新函数块并复制基本字段 */
    FuncBlock *dst = func_block_create(src->id);
    if (!dst) return NULL;

    /* 深拷贝内部节点 ID 数组 */
    if (src->internal_node_count > 0 && src->internal_node_ids) {
        dst->internal_node_ids = dup_int_array(src->internal_node_ids,
                                                src->internal_node_count);
        if (!dst->internal_node_ids) goto fail;
    }
    dst->internal_node_count = src->internal_node_count;

    /* 深拷贝输入端口 ID 数组 */
    if (src->input_count > 0 && src->input_port_ids) {
        dst->input_port_ids = dup_int_array(src->input_port_ids,
                                             src->input_count);
        if (!dst->input_port_ids) goto fail;
    }
    dst->input_count = src->input_count;

    /* 深拷贝输出端口 ID 数组 */
    if (src->output_count > 0 && src->output_port_ids) {
        dst->output_port_ids = dup_int_array(src->output_port_ids,
                                              src->output_count);
        if (!dst->output_port_ids) goto fail;
    }
    dst->output_count = src->output_count;

    /* 深拷贝端口依赖数组 */
    if (src->port_dep_count > 0 && src->port_deps) {
        dst->port_deps = lv00_malloc(
            (size_t)src->port_dep_count * sizeof(PortDependency));
        if (!dst->port_deps) goto fail;
        memcpy(dst->port_deps, src->port_deps,
               (size_t)src->port_dep_count * sizeof(PortDependency));
    }
    dst->port_dep_count = src->port_dep_count;

    /* 深拷贝选择器 */
    if (src->selector) {
        dst->selector = lv00_malloc(sizeof(SolutionSelector));
        if (!dst->selector) goto fail;
        memcpy(dst->selector, src->selector, sizeof(SolutionSelector));
        /* 选择器中的函数指针和 user_data 直接复制（浅拷贝），
         * 因为 user_data 的生命周期由外部管理 */
    }

    /* 深拷贝名称字符串 */
    if (src->name) {
        dst->name = lv00_strdup(src->name);
        if (!dst->name) goto fail;
    }

    /* 深拷贝描述字符串 */
    if (src->description) {
        dst->description = lv00_strdup(src->description);
        if (!dst->description) goto fail;
    }

    /* 深拷贝前置条件区域 ID 数组 */
    if (src->precondition_count > 0 && src->precondition_region_ids) {
        dst->precondition_region_ids = dup_int_array(
            src->precondition_region_ids, src->precondition_count);
        if (!dst->precondition_region_ids) goto fail;
    }
    dst->precondition_count = src->precondition_count;

    /* 直接复制值类型字段 */
    dst->determinism      = src->determinism;
    dst->has_measure      = src->has_measure;
    dst->measure_node_id  = src->measure_node_id;
    dst->measure_compare  = src->measure_compare;
    dst->view_state       = src->view_state;

    return dst;

fail:
    /* 清理已分配的部分资源 */
    func_block_destroy(dst);
    return NULL;
}

/* ==================== 内部共享函数 ==================== */

bool collect_all_block_ids(
    const FuncBlock *fb,
    int **out_ids,
    int *out_count)
{
    if (!fb || !out_ids || !out_count) return false;

    /* 计算总数：内部节点 + 输入端口 + 输出端口 */
    int total = fb->internal_node_count + fb->input_count + fb->output_count;
    if (total <= 0) return false;

    /* 检查整数溢出 */
    if (total > INT_MAX / (int)sizeof(int)) return false;

    int *ids = lv00_malloc((size_t)total * sizeof(int));
    if (!ids) return false;

    int count = 0;

    /* 收集内部节点ID */
    for (int i = 0; i < fb->internal_node_count; i++) {
        ids[count++] = fb->internal_node_ids[i];
    }

    /* 收集输入端口ID */
    for (int i = 0; i < fb->input_count; i++) {
        ids[count++] = fb->input_port_ids[i];
    }

    /* 收集输出端口ID */
    for (int i = 0; i < fb->output_count; i++) {
        ids[count++] = fb->output_port_ids[i];
    }

    *out_ids = ids;
    *out_count = count;
    return true;
}

/**
 * @file func_block_compose.c
 * @brief 函数块组合子实现
 * @details 提供函数块的组合操作，包括顺序组合（g o f）和并行乘积（f x g）。
 *          使用 graph->next_node_id 生成唯一 ID 以避免冲突。
 *
 * @author Lv-00 Project
 * @version 3.0.1
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/error_codes.h"
#include "lv/func_block.h"
#include "lv/lv_lifecycle.h"
#include "lv/lv_utils.h"

/* ============== 函数块组合子 ============== */

/** @brief 作用域守卫清理回调：销毁 FuncBlock 指针变量（配合 lv_DEFER 使用） */
static void defer_func_block_destroy(void *arg) {
    func_block_destroy(*(FuncBlock **) arg);
}

/**
 * @brief 顺序组合两个函数块：g o f
 *
 * 将函数块 f 和 g 进行顺序组合，其中 f 的输出端口连接到 g 的输入端口。
 * 组合后的新函数块具有：
 *   - 输入端口 = f 的输入端口
 *   - 输出端口 = g 的输出端口
 *   - 内部节点 = f 的内部节点 ∪ g 的内部节点 ∪ f 的输出端口 ∪ g 的输入端口
 *   - 端口依赖 = f 的每个输出端口到 g 的对应输入端口的关联约束
 *
 * 前置条件：f 的输出端口数必须等于 g 的输入端口数。
 *
 * @param f           第一个函数块（先执行）
 * @param g           第二个函数块（后执行）
 * @param graph       约束图，用于分配全局唯一的函数块 ID
 * @param out_composed 输出参数，返回新创建的组合函数块
 * @return true  组合成功
 * @return false 组合失败（参数无效、端口数不匹配或内存不足）
 */
bool func_block_compose(FuncBlock *f, FuncBlock *g, ConstraintGraph *graph, FuncBlock **out_composed) {
    if (!f || !g || !graph || !out_composed)
        return false;
    if (f->output_count != g->input_count)
        return false;

    /* 使用 graph->next_node_id 生成全局唯一 ID，避免 ID 冲突 */
    int composed_id = graph->next_node_id++;
    bool success = false;

    FuncBlock *composed = func_block_create(composed_id);
    if (!composed) {
        graph->next_node_id--; /* 回滚 ID */
        return false;
    }
    /* composed 创建后立即注册作用域守卫：失败路径直接 return，
     * 出口自动销毁 composed，无需手写 compose_cleanup 标签。 */
    lv_DEFER(defer_func_block_destroy, &composed);

    /*
     * 内部节点 = f 的内部节点 ∪ g 的内部节点 ∪ f 的输出端口 ∪ g 的输入端口
     *
     * f 的输出端口和 g 的输入端口在组合后成为内部连接点，
     * 不再暴露为外部端口，因此需要纳入内部节点集合。
     */
    int total_internal = f->internal_node_count + g->internal_node_count + f->output_count + g->input_count;
    int *internal_ids = lv_malloc((size_t) total_internal * sizeof(int));
    if (!internal_ids) {
        graph->next_node_id--; /* 回滚 ID */
        return false;
    }
    lv_DEFER_FREE(internal_ids);
    int idx = 0;
    for (int i = 0; i < f->internal_node_count; i++)
        internal_ids[idx++] = f->internal_node_ids[i];
    for (int i = 0; i < g->internal_node_count; i++)
        internal_ids[idx++] = g->internal_node_ids[i];
    for (int i = 0; i < f->output_count; i++)
        internal_ids[idx++] = f->output_port_ids[i];
    for (int i = 0; i < g->input_count; i++)
        internal_ids[idx++] = g->input_port_ids[i];

    if (!func_block_set_internal_nodes(composed, internal_ids, idx)) {
        graph->next_node_id--; /* 回滚 ID */
        return false;
    }

    /* 输入端口 = f 的输入端口（组合后的外部输入来自 f） */
    if (!func_block_set_input_ports(composed, f->input_port_ids, f->input_count)) {
        graph->next_node_id--; /* 回滚 ID */
        return false;
    }

    /* 输出端口 = g 的输出端口（组合后的外部输出来自 g） */
    if (!func_block_set_output_ports(composed, g->output_port_ids, g->output_count)) {
        graph->next_node_id--; /* 回滚 ID */
        return false;
    }

    /*
     * 添加连接依赖：f 的输出 -> g 的输入
     *
     * 对每一对对应的端口创建关联约束（PORT_DEP_INCIDENCE），
     * 表示 f 的第 i 个输出端口在约束上关联到 g 的第 i 个输入端口。
     */
    for (int i = 0; i < f->output_count && i < g->input_count; i++) {
        PortDependency dep;
        memset(&dep, 0, sizeof(PortDependency));
        dep.type = PORT_DEP_INCIDENCE;
        dep.port_id = f->output_port_ids[i];
        dep.external_node_id = g->input_port_ids[i];
        dep.internal_node_id = f->output_port_ids[i];
        dep.constraint_data = NULL;
        func_block_add_port_dependency(composed, &dep);
    }

    /* 设置组合函数块的名称，格式为 "(g_name >> f_name)" */
    if (f->name && g->name) {
        /* lv_asprintf 精确分配，消除固定余量估算与截断分支 */
        composed->name = lv_asprintf("(%s >> %s)", g->name, f->name);
    }

    composed->determinism = DETERMINISM_UNVERIFIED;
    /* 成功路径：composed 交由调用者接管，置 NULL 使作用域守卫不再销毁 */
    *out_composed = composed;
    composed = NULL;
    success = true;
    return success;
}

/**
 * @brief 并行乘积两个函数块：f x g
 *
 * 将函数块 f 和 g 进行并行乘积组合，两个函数块独立执行。
 * 乘积后的新函数块具有：
 *   - 输入端口 = f 的输入端口 ∪ g 的输入端口
 *   - 输出端口 = f 的输出端口 ∪ g 的输出端口
 *   - 内部节点 = f 的内部节点 ∪ g 的内部节点（去重合并）
 *
 * @param f           第一个函数块
 * @param g           第二个函数块
 * @param graph       约束图，用于分配全局唯一的函数块 ID
 * @param out_product 输出参数，返回新创建的乘积函数块
 * @return true  乘积成功
 * @return false 乘积失败（参数无效或内存不足）
 */
bool func_block_product(FuncBlock *f, FuncBlock *g, ConstraintGraph *graph, FuncBlock **out_product) {
    if (!f || !g || !graph || !out_product)
        return false;

    /* 使用 graph->next_node_id 生成全局唯一 ID */
    int product_id = graph->next_node_id++;
    bool success = false;

    FuncBlock *product = func_block_create(product_id);
    if (!product) {
        graph->next_node_id--; /* 回滚 ID */
        return false;
    }
    /* product 创建后立即注册作用域守卫：失败路径直接 return，
     * 出口自动销毁 product，无需手写 product_cleanup 标签。 */
    lv_DEFER(defer_func_block_destroy, &product);

    /*
     * 内部节点 = f 的内部节点 ∪ g 的内部节点
     *
     * 使用 merge_int_arrays 合并两个内部节点数组。
     * 需要检查返回值是否为 NULL，防止内存分配失败。
     */
    int total_internal = 0;
    int *internal_ids = merge_int_arrays(f->internal_node_ids, f->internal_node_count, g->internal_node_ids,
                                         g->internal_node_count, &total_internal);
    lv_DEFER_FREE(internal_ids); /* NULL 也安全，作用域守卫自动释放 */

    if (total_internal > 0) {
        if (!internal_ids || !func_block_set_internal_nodes(product, internal_ids, total_internal)) {
            graph->next_node_id--; /* 回滚 ID */
            return false;
        }
    }

    /* 输入端口 = f 的输入端口 ∪ g 的输入端口 */
    int total_input = 0;
    int *input_ids =
        merge_int_arrays(f->input_port_ids, f->input_count, g->input_port_ids, g->input_count, &total_input);
    lv_DEFER_FREE(input_ids);

    if (total_input > 0) {
        if (!input_ids || !func_block_set_input_ports(product, input_ids, total_input)) {
            graph->next_node_id--; /* 回滚 ID */
            return false;
        }
    }

    /* 输出端口 = f 的输出端口 ∪ g 的输出端口 */
    int total_output = 0;
    int *output_ids =
        merge_int_arrays(f->output_port_ids, f->output_count, g->output_port_ids, g->output_count, &total_output);
    lv_DEFER_FREE(output_ids);

    if (total_output > 0) {
        if (!output_ids || !func_block_set_output_ports(product, output_ids, total_output)) {
            graph->next_node_id--; /* 回滚 ID */
            return false;
        }
    }

    /* 设置乘积函数块的名称，格式为 "(f_name * g_name)" */
    if (f->name && g->name) {
        /* lv_asprintf 精确分配，消除固定余量估算与截断分支 */
        product->name = lv_asprintf("(%s * %s)", f->name, g->name);
    }

    product->determinism = DETERMINISM_UNVERIFIED;
    /* 成功路径：product 交由调用者接管，置 NULL 使作用域守卫不再销毁 */
    *out_product = product;
    product = NULL;
    success = true;
    return success;
}

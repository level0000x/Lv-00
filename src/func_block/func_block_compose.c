/**
 * @file func_block_compose.c
 * @brief 函数块组合子实现
 * @details 提供函数块的组合操作，包括顺序组合（g o f）和并行乘积（f x g）。
 *          使用 graph->next_node_id 生成唯一 ID 以避免冲突。
 *
 * @author Lv-00 Project
 * @version 3.2.0
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error_codes.h"
#include "func_block.h"
#include "lv00_utils.h"

/* ==================== 命名常量 ==================== */

/** 组合函数块名称格式中额外字符数（括号、空格、运算符） */
#define COMPOSE_NAME_EXTRA_CHARS 8
/** 乘积函数块名称格式中额外字符数（括号、空格、运算符） */
#define PRODUCT_NAME_EXTRA_CHARS 6

/* ============== 函数块组合子 ============== */

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
    if (!composed)
        goto compose_cleanup;

    /*
     * 内部节点 = f 的内部节点 ∪ g 的内部节点 ∪ f 的输出端口 ∪ g 的输入端口
     *
     * f 的输出端口和 g 的输入端口在组合后成为内部连接点，
     * 不再暴露为外部端口，因此需要纳入内部节点集合。
     */
    /* 整数溢出检查 */
    if (f->internal_node_count > INT_MAX - g->internal_node_count - f->output_count - g->input_count) {
        fprintf(stderr, "[ERROR] 函数块组合失败：内部节点总数溢出\n");
        return NULL;
    }
    int total_internal = f->internal_node_count + g->internal_node_count + f->output_count + g->input_count;
    int *internal_ids = lv00_malloc((size_t) total_internal * sizeof(int));
    if (!internal_ids)
        goto compose_cleanup;
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
        lv00_free((void **) &internal_ids);
        goto compose_cleanup;
    }
    lv00_free((void **) &internal_ids);

    /* 输入端口 = f 的输入端口（组合后的外部输入来自 f） */
    if (!func_block_set_input_ports(composed, f->input_port_ids, f->input_count)) {
        goto compose_cleanup;
    }

    /* 输出端口 = g 的输出端口（组合后的外部输出来自 g） */
    if (!func_block_set_output_ports(composed, g->output_port_ids, g->output_count)) {
        goto compose_cleanup;
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
        size_t name_len = strlen(f->name) + strlen(g->name) + COMPOSE_NAME_EXTRA_CHARS;
        composed->name = lv00_malloc(name_len);
        if (composed->name) {
            int written = snprintf(composed->name, name_len, "(%s >> %s)", g->name, f->name);
            if (written < 0 || (size_t) written >= name_len) {
                lv00_set_error(LV00_ERROR_BUFFER_TOO_SMALL,
                               "func_block_compose: 组合函数块名称截断（需要%zu字节，已分配%zu字节）",
                               strlen(f->name) + strlen(g->name) + COMPOSE_NAME_EXTRA_CHARS, name_len);
            }
        }
    }

    composed->determinism = DETERMINISM_UNVERIFIED;
    success = true;

compose_cleanup:
    if (!success) {
        graph->next_node_id--; /* 回滚 ID */
        if (composed)
            func_block_destroy(composed);
    }
    if (success) {
        *out_composed = composed;
    }
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
    if (!product)
        goto product_cleanup;

    /*
     * 内部节点 = f 的内部节点 ∪ g 的内部节点
     *
     * 使用 merge_int_arrays 合并两个内部节点数组。
     * 需要检查返回值是否为 NULL，防止内存分配失败。
     */
    int total_internal = 0;
    int *internal_ids = merge_int_arrays(f->internal_node_ids, f->internal_node_count, g->internal_node_ids,
                                         g->internal_node_count, &total_internal);

    if (total_internal > 0) {
        if (!internal_ids || !func_block_set_internal_nodes(product, internal_ids, total_internal)) {
            lv00_free((void **) &internal_ids);
            goto product_cleanup;
        }
    }
    lv00_free((void **) &internal_ids);

    /* 输入端口 = f 的输入端口 ∪ g 的输入端口 */
    int total_input = 0;
    int *input_ids =
        merge_int_arrays(f->input_port_ids, f->input_count, g->input_port_ids, g->input_count, &total_input);

    if (total_input > 0) {
        if (!input_ids || !func_block_set_input_ports(product, input_ids, total_input)) {
            lv00_free((void **) &input_ids);
            goto product_cleanup;
        }
    }
    lv00_free((void **) &input_ids);

    /* 输出端口 = f 的输出端口 ∪ g 的输出端口 */
    int total_output = 0;
    int *output_ids =
        merge_int_arrays(f->output_port_ids, f->output_count, g->output_port_ids, g->output_count, &total_output);

    if (total_output > 0) {
        if (!output_ids || !func_block_set_output_ports(product, output_ids, total_output)) {
            lv00_free((void **) &output_ids);
            goto product_cleanup;
        }
    }
    lv00_free((void **) &output_ids);

    /* 设置乘积函数块的名称，格式为 "(f_name * g_name)" */
    if (f->name && g->name) {
        size_t name_len = strlen(f->name) + strlen(g->name) + PRODUCT_NAME_EXTRA_CHARS;
        product->name = lv00_malloc(name_len);
        if (product->name) {
            int written = snprintf(product->name, name_len, "(%s * %s)", f->name, g->name);
            if (written < 0 || (size_t) written >= name_len) {
                lv00_set_error(LV00_ERROR_BUFFER_TOO_SMALL,
                               "func_block_product: 乘积函数块名称截断（需要%zu字节，已分配%zu字节）",
                               strlen(f->name) + strlen(g->name) + PRODUCT_NAME_EXTRA_CHARS, name_len);
            }
        }
    }

    product->determinism = DETERMINISM_UNVERIFIED;
    success = true;

product_cleanup:
    if (!success) {
        graph->next_node_id--; /* 回滚 ID */
        if (product)
            func_block_destroy(product);
    }
    if (success) {
        *out_product = product;
    }
    return success;
}

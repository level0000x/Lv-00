/**
 * @file func_block_preset_ops.c
 * @brief 预设函数块操作接口实现
 *
 * 实现预设函数块的高级操作，包括链式调用、批量操作、验证测试等。
 * 所有操作都遵循函数式设计原则，不修改原始预设。
 *
 * 内存管理：
 * - 使用 lv_malloc / lv_free 进行内存管理
 * - 所有输出参数由调用者负责释放
 * - 错误时自动清理已分配的资源
 */

#include "func_block_preset_ops.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "error_codes.h"
#include "lv/constraint_graph.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

/* ================================================================
 * 命名常量
 * ================================================================ */

/** 预设链初始容量 */
#define PRESET_CHAIN_INITIAL_CAPACITY 8

/** 预设链扩容因子 */
#define PRESET_CHAIN_GROWTH_FACTOR 2

/** 最大预设名称长度 */
#define MAX_PRESET_NAME_LENGTH 256

/** 组合预设名称前缀 */
#define COMPOSED_PRESET_PREFIX "composed_"

/* ================================================================
 * 【修复】模块级原子计数器，确保跨函数的线程安全唯一ID生成
 *
 * 将原本分散在 preset_partial_bind、preset_compose、preset_make_recursive
 * 三个函数中的局部 static _Atomic 变量统一提升到模块层。
 * 这样可以：
 *   - 避免编译器对块作用域 _Atomic static 变量的不可移植行为
 *   - 确保所有计数器在程序启动时即完成零初始化
 *   - 让所有调用点共享同一计数器，避免命名冲突
 * ================================================================ */
static _Atomic int g_bind_counter = 0;         /**< partial_bind 操作计数器 */
static _Atomic int g_compose_counter = 0;      /**< compose 操作计数器 */
static _Atomic int g_recursive_counter = 0;    /**< 递归预设计数器 */

/* ================================================================
 * 预设链式调用实现
 * ================================================================ */

/**
 * @brief 链式调用节点
 *
 * 表示预设链中的一个步骤，包含预设名称和输入映射关系。
 * 输入映射指定当前步骤的每个输入端口应从上一步骤的哪个输出获取数据。
 */
typedef struct ChainNode {
    char *preset_name;                    /**< 预设名称（动态分配，由 preset_chain_destroy 释放） */
    int *input_mapping;                   /**< 输入映射数组：input_mapping[j] 表示第 j 个输入来自上一步的第 input_mapping[j] 个输出 */
    int mapping_count;                    /**< 映射数组长度（通常等于预设的输入端口数量） */
} ChainNode;

/**
 * @brief 预设链结构
 *
 * 有序的预设执行序列。每个节点依次执行，前一步的输出作为后一步的输入。
 * 支持通过 input_mapping 自定义数据流连接。
 */
struct PresetChain {
    ChainNode *nodes;                     /**< 节点数组（动态分配，按执行顺序排列） */
    int count;                            /**< 当前节点数量（已添加的预设步骤数） */
    int capacity;                         /**< 数组容量（>= count，用于扩容判断） */
};

PresetChain *preset_chain_create(void)
{
    /* 分配链结构体 */
    PresetChain *chain = lv_malloc(sizeof(PresetChain));
    if (!chain) return NULL;

    /* 分配初始容量的节点数组 */
    chain->nodes = lv_malloc(PRESET_CHAIN_INITIAL_CAPACITY * sizeof(ChainNode));
    if (!chain->nodes) {
        lv_free((void **)&chain);
        return NULL;
    }

    chain->count = 0;
    chain->capacity = PRESET_CHAIN_INITIAL_CAPACITY;

    /* 初始化所有节点槽位为空状态，防止野指针 */
    for (int i = 0; i < chain->capacity; i++) {
        chain->nodes[i].preset_name = NULL;
        chain->nodes[i].input_mapping = NULL;
        chain->nodes[i].mapping_count = 0;
    }

    return chain;
}

void preset_chain_destroy(PresetChain *chain)
{
    if (!chain) return;

    /* 释放每个节点的资源 */
    for (int i = 0; i < chain->count; i++) {
        lv_free((void **)&chain->nodes[i].preset_name);
        lv_free((void **)&chain->nodes[i].input_mapping);
    }

    lv_free((void **)&chain->nodes);
    lv_free((void **)&chain);
}

/**
 * @brief 确保链有足够的容量
 *
 * 当 chain->count >= chain->capacity 时，以 PRESET_CHAIN_GROWTH_FACTOR 倍率扩容。
 * 新分配的节点槽位会被初始化为空状态。
 *
 * @param chain 预设链指针
 * @return true 扩容成功或无需扩容，false 参数无效或内存不足
 */
static bool ensure_chain_capacity(PresetChain *chain)
{
    if (!chain) return false;
    if (chain->count < chain->capacity) return true;

    /* 【修复】检查 capacity * GROWTH_FACTOR 是否溢出 int 范围 */
    if (chain->capacity > INT_MAX / PRESET_CHAIN_GROWTH_FACTOR) {
        return false;  /* 溢出，无法继续扩容 */
    }
    int new_capacity = chain->capacity * PRESET_CHAIN_GROWTH_FACTOR;

    /* 【修复】检查 new_capacity * sizeof(ChainNode) 是否溢出 size_t */
    if ((size_t)new_capacity > SIZE_MAX / sizeof(ChainNode)) {
        return false;  /* size_t 乘法将溢出，拒绝分配 */
    }

    ChainNode *new_nodes = lv_realloc(chain->nodes,
                                         (size_t)new_capacity * sizeof(ChainNode));
    if (!new_nodes) return false;

    /* 初始化新节点 */
    for (int i = chain->capacity; i < new_capacity; i++) {
        new_nodes[i].preset_name = NULL;
        new_nodes[i].input_mapping = NULL;
        new_nodes[i].mapping_count = 0;
    }

    chain->nodes = new_nodes;
    chain->capacity = new_capacity;
    return true;
}

bool preset_chain_add(PresetChain *chain,
                       const char *preset_name,
                       const int *input_mapping)
{
    if (!chain || !preset_name) return false;

    /* 验证预设存在 */
    PresetBlockMetadata *meta = preset_blocks_get_metadata(preset_name);
    if (!meta) {
        /* 使用 lv_ERROR_SET 宏记录错误信息（宏定义在 error_codes.h 中） */
        lv_ERROR_SET(lv_ERROR_INVALID_PARAM,
                       "预设 '%s' 不存在", preset_name);
        return false;
    }

    if (!ensure_chain_capacity(chain)) {
        { void *tmp = meta; lv_free(&tmp); }
        return false;
    }

    ChainNode *node = &chain->nodes[chain->count];

    /* 复制预设名称 */
    node->preset_name = lv_strdup(preset_name);
    if (!node->preset_name) {
        { void *tmp = meta; lv_free(&tmp); }
        return false;
    }

    /* 复制输入映射 */
    if (input_mapping && meta->input_count > 0) {
        node->input_mapping = lv_malloc((size_t)meta->input_count * sizeof(int));
        if (!node->input_mapping) {
            lv_free((void **)&node->preset_name);
            { void *tmp = meta; lv_free(&tmp); }
            return false;
        }
        memcpy(node->input_mapping, input_mapping,
               (size_t)meta->input_count * sizeof(int));
        node->mapping_count = meta->input_count;
    } else {
        node->input_mapping = NULL;
        node->mapping_count = 0;
    }

    chain->count++;
    { void *tmp = meta; lv_free(&tmp); }
    return true;
}

/**
 * @brief 顺序执行预设链中的所有预设
 *
 * 从初始参数开始，依次对链中的每个预设执行实例化操作。
 * 前一步的输出作为后一步的输入，支持通过 input_mapping 自定义数据流。
 *
 * 执行流程：
 * 1. 复制初始参数作为当前输出集
 * 2. 对每个链节点：
 *    a. 从注册表查找预设模板
 *    b. 根据 input_mapping 构建参数映射（默认顺序映射）
 *    c. 调用 func_block_instantiate 执行实例化
 *    d. 将实例化输出更新为当前输出集
 * 3. 返回最终输出集
 *
 * @param chain            预设链
 * @param graph            约束图（实例化操作的目标图）
 * @param initial_args     初始参数数组（链的第一个预设的输入）
 * @param arg_count        初始参数数量
 * @param out_final_outputs 输出：最终输出节点 ID 数组（调用者负责释放）
 * @param out_output_count 输出：最终输出节点数量
 * @return true 执行成功，false 参数无效或任一步骤实例化失败
 */
bool preset_chain_execute(PresetChain *chain,
                           ConstraintGraph *graph,
                           const int *initial_args,
                           int arg_count,
                           int **out_final_outputs,
                           int *out_output_count)
{
    if (!chain || !graph || !out_final_outputs || !out_output_count) {
        return false;
    }

    *out_final_outputs = NULL;
    *out_output_count = 0;

    if (chain->count == 0) return false;

    /* 当前可用的输出节点ID */
    int *current_outputs = NULL;
    int current_output_count = 0;

    /* 复制初始参数 */
    if (initial_args && arg_count > 0) {
        current_outputs = lv_malloc((size_t)arg_count * sizeof(int));
        if (!current_outputs) return false;
        memcpy(current_outputs, initial_args, (size_t)arg_count * sizeof(int));
        current_output_count = arg_count;
    }

    /* 依次执行链中的每个预设 */
    for (int i = 0; i < chain->count; i++) {
        ChainNode *node = &chain->nodes[i];

        /* 查找预设 */
        FuncBlock *fb = func_block_registry_lookup(node->preset_name);
        if (!fb) {
            lv_free((void **)&current_outputs);
            return false;
        }

        /* 【修复】验证查找返回的函数块状态完整性：input_count 必须 >= 0 或 == -1（可变） */
        if (fb->input_count < -1) {
            /* 输入端口计数异常，视为无效函数块 */
            func_block_destroy(fb);
            lv_free((void **)&current_outputs);
            return false;
        }

        /* 准备参数映射 */
        int *arg_mappings = NULL;
        int mapping_count = fb->input_count;

        if (mapping_count > 0) {
            arg_mappings = lv_malloc((size_t)mapping_count * sizeof(int));
            if (!arg_mappings) {
                func_block_destroy(fb);
                lv_free((void **)&current_outputs);
                return false;
            }

            /* 应用输入映射 */
            for (int j = 0; j < mapping_count; j++) {
                if (node->input_mapping && j < node->mapping_count) {
                    int map_idx = node->input_mapping[j];
                    if (map_idx >= 0 && map_idx < current_output_count) {
                        arg_mappings[j] = current_outputs[map_idx];
                    } else {
                        arg_mappings[j] = -1;  /* 无效映射 */
                    }
                } else if (j < current_output_count) {
                    /* 默认顺序映射 */
                    arg_mappings[j] = current_outputs[j];
                } else {
                    arg_mappings[j] = -1;
                }
            }
        }

        /* 实例化预设 */
        int *new_outputs = NULL;
        int new_output_count = 0;

        InstantiateResult result = func_block_instantiate(
            fb, graph, arg_mappings, mapping_count,
            &new_outputs, &new_output_count);

        func_block_destroy(fb);
        lv_free((void **)&arg_mappings);

        if (result != INSTANTIATE_OK) {
            /* 修复内存泄漏：错误路径下释放可能已分配的 new_outputs */
            lv_free((void **)&new_outputs);
            lv_free((void **)&current_outputs);
            return false;
        }

        /* 更新当前输出 */
        lv_free((void **)&current_outputs);
        current_outputs = new_outputs;
        current_output_count = new_output_count;
    }

    *out_final_outputs = current_outputs;
    *out_output_count = current_output_count;
    return true;
}

/* ================================================================
 * 预设批量操作实现
 * ================================================================ */

bool preset_batch_instantiate(const char *preset_name,
                               ConstraintGraph *graph,
                               const int **arg_sets,
                               int set_count,
                               int args_per_set,
                               int ***out_results)
{
    if (!preset_name || !graph || !arg_sets || !out_results || set_count <= 0) {
        return false;
    }

    /* 查找预设 */
    FuncBlock *template_fb = func_block_registry_lookup(preset_name);
    if (!template_fb) {
        lv_ERROR_SET(lv_ERROR_INVALID_PARAM,
                       "预设 '%s' 不存在", preset_name);
        return false;
    }

    /* 验证参数数量 */
    if (template_fb->input_count != args_per_set && template_fb->input_count != -1) {
        func_block_destroy(template_fb);
        lv_ERROR_SET(lv_ERROR_INVALID_PARAM,
                       "参数数量不匹配：预设需要 %d 个参数，提供了 %d 个",
                       template_fb->input_count, args_per_set);
        return false;
    }

    func_block_destroy(template_fb);

    /* 分配结果数组 */
    int **results = lv_malloc((size_t)set_count * sizeof(int *));
    if (!results) return false;

    /* 批量实例化 */
    for (int i = 0; i < set_count; i++) {
        results[i] = NULL;

        FuncBlock *fb = func_block_registry_lookup(preset_name);
        if (!fb) {
            /* 清理已分配的结果 */
            for (int j = 0; j < i; j++) {
                lv_free((void **)&results[j]);
            }
            lv_free((void **)&results);
            return false;
        }

        int *outputs = NULL;
        int output_count = 0;

        /* 复制参数（因为实例化可能修改参数数组） */
        int *args_copy = lv_malloc((size_t)args_per_set * sizeof(int));
        if (!args_copy) {
            func_block_destroy(fb);
            for (int j = 0; j < i; j++) {
                lv_free((void **)&results[j]);
            }
            lv_free((void **)&results);
            return false;
        }
        memcpy(args_copy, arg_sets[i], (size_t)args_per_set * sizeof(int));

        InstantiateResult result = func_block_instantiate(
            fb, graph, args_copy, args_per_set,
            &outputs, &output_count);

        func_block_destroy(fb);
        lv_free((void **)&args_copy);

        if (result != INSTANTIATE_OK) {
            /* 【修复】错误路径：释放可能已被 func_block_instantiate 分配的 outputs，防止内存泄漏 */
            lv_free((void **)&outputs);
            /* 清理已分配的结果 */
            for (int j = 0; j < i; j++) {
                lv_free((void **)&results[j]);
            }
            lv_free((void **)&results);
            return false;
        }

        /* 简化：只保存第一个输出节点ID */
        if (output_count > 0) {
            results[i] = lv_malloc(sizeof(int));
            if (results[i]) {
                results[i][0] = outputs[0];
            }
            lv_free((void **)&outputs);
        } else {
            results[i] = NULL;
        }
    }

    *out_results = results;
    return true;
}

bool preset_batch_apply(const char *preset_name,
                         ConstraintGraph *graph,
                         const int *target_nodes,
                         int node_count,
                         int ***out_results)
{
    if (!preset_name || !graph || !target_nodes || !out_results || node_count <= 0) {
        return false;
    }

    /* 查找预设 */
    const PresetBlockMetadata *meta = preset_blocks_get_metadata(preset_name);
    if (!meta) {
        lv_ERROR_SET(lv_ERROR_INVALID_PARAM,
                       "预设 '%s' 不存在", preset_name);
        return false;
    }

    int input_count = meta->input_count;
    if (input_count <= 0) {
        lv_ERROR_SET(lv_ERROR_INVALID_PARAM,
                       "预设 '%s' 不支持批量应用", preset_name);
        return false;
    }

    /* 计算可以应用多少次 */
    int apply_count = node_count / input_count;
    if (apply_count == 0) {
        lv_ERROR_SET(lv_ERROR_INVALID_PARAM,
                       "节点数量不足：需要 %d 个节点，只有 %d 个",
                       input_count, node_count);
        return false;
    }

    /* 分配结果数组 */
    int **results = lv_malloc((size_t)apply_count * sizeof(int *));
    if (!results) return false;

    /* 批量应用 */
    for (int i = 0; i < apply_count; i++) {
        results[i] = NULL;

        FuncBlock *fb = func_block_registry_lookup(preset_name);
        if (!fb) {
            for (int j = 0; j < i; j++) {
                lv_free((void **)&results[j]);
            }
            lv_free((void **)&results);
            return false;
        }

        /* 准备参数 */
        int *args = lv_malloc((size_t)input_count * sizeof(int));
        if (!args) {
            func_block_destroy(fb);
            for (int j = 0; j < i; j++) {
                lv_free((void **)&results[j]);
            }
            lv_free((void **)&results);
            return false;
        }

        for (int j = 0; j < input_count; j++) {
            args[j] = target_nodes[i * input_count + j];
        }

        int *outputs = NULL;
        int output_count = 0;

        InstantiateResult result = func_block_instantiate(
            fb, graph, args, input_count,
            &outputs, &output_count);

        func_block_destroy(fb);
        lv_free((void **)&args);

        if (result != INSTANTIATE_OK) {
            /* 【修复】错误路径：释放可能已被 func_block_instantiate 分配的 outputs，防止内存泄漏 */
            lv_free((void **)&outputs);
            for (int j = 0; j < i; j++) {
                lv_free((void **)&results[j]);
            }
            lv_free((void **)&results);
            return false;
        }

        /* 保存输出 */
        if (output_count > 0) {
            results[i] = lv_malloc((size_t)output_count * sizeof(int));
            if (results[i]) {
                memcpy(results[i], outputs, (size_t)output_count * sizeof(int));
            }
        }
        lv_free((void **)&outputs);
    }

    *out_results = results;
    return true;
}

/* ================================================================
 * 预设验证与测试实现
 * ================================================================ */

/**
 * @brief 验证预设的完整性和确定性
 *
 * 对指定预设执行以下验证：
 * 1. 名称有效性检查（非空、存在于注册表）
 * 2. 元数据完整性检查（输入/输出数量有效）
 * 3. 模板函数块可加载性检查
 * 4. 确定性状态检查（是否已通过静态验证）
 *
 * @param preset_name 预设名称
 * @return 验证结果结构体（包含各检查项的状态和错误信息），
 *         调用者需通过 preset_validation_result_destroy 释放 error_message
 */
PresetValidationResult preset_validate(const char *preset_name)
{
    PresetValidationResult result = {
        .is_valid = false,
        .has_valid_inputs = false,
        .has_valid_outputs = false,
        .determinism_check_passed = false,
        .error_message = NULL
    };

    if (!preset_name) {
        result.error_message = lv_strdup("预设名称为空");
        return result;
    }

    /* 获取元数据 */
    const PresetBlockMetadata *meta = preset_blocks_get_metadata(preset_name);
    if (!meta) {
        result.error_message = lv_strdup("预设不存在");
        return result;
    }

    /* 验证输入 */
    if (meta->input_count >= 0) {
        result.has_valid_inputs = true;
    }

    /* 验证输出 */
    if (meta->output_count >= 0) {
        result.has_valid_outputs = true;
    }

    /* 查找函数块模板 */
    FuncBlock *fb = func_block_registry_lookup(preset_name);
    if (!fb) {
        result.error_message = lv_strdup("无法加载预设模板");
        return result;
    }

    /* 检查确定性 */
    if (fb->determinism == DETERMINISM_VERIFIED ||
        fb->determinism == DETERMINISM_PARTIALLY_VERIFIED) {
        result.determinism_check_passed = true;
    }

    func_block_destroy(fb);

    /* 综合判断 */
    result.is_valid = result.has_valid_inputs &&
                      result.has_valid_outputs;

    if (!result.is_valid && !result.error_message) {
        result.error_message = lv_strdup("预设定义不完整");
    }

    return result;
}

void preset_validation_result_destroy(PresetValidationResult *result)
{
    if (!result) return;
    lv_free((void **)&result->error_message);
}

bool preset_test_instantiation(const char *preset_name,
                                ConstraintGraph *graph,
                                const int *test_args,
                                int arg_count)
{
    if (!preset_name || !graph) return false;

    /* 查找预设 */
    FuncBlock *fb = func_block_registry_lookup(preset_name);
    if (!fb) return false;

    /* 验证参数数量 */
    if (fb->input_count != arg_count && fb->input_count != -1) {
        func_block_destroy(fb);
        return false;
    }

    /* 复制参数 */
    int *args_copy = NULL;
    if (arg_count > 0 && test_args) {
        args_copy = lv_malloc((size_t)arg_count * sizeof(int));
        if (!args_copy) {
            func_block_destroy(fb);
            return false;
        }
        memcpy(args_copy, test_args, (size_t)arg_count * sizeof(int));
    }

    /* 尝试实例化 */
    int *outputs = NULL;
    int output_count = 0;

    InstantiateResult result = func_block_instantiate(
        fb, graph, args_copy, arg_count,
        &outputs, &output_count);

    func_block_destroy(fb);
    lv_free((void **)&args_copy);

    if (result == INSTANTIATE_OK && outputs) {
        lv_free((void **)&outputs);
        return true;
    }

    return false;
}

/* ================================================================
 * 预设参数操作实现
 * ================================================================ */

bool preset_create_bindings(const char *preset_name,
                             PresetParamBinding **out_bindings,
                             int *out_count)
{
    if (!preset_name || !out_bindings || !out_count) return false;

    /* 获取元数据 */
    const PresetBlockMetadata *meta = preset_blocks_get_metadata(preset_name);
    if (!meta) return false;

    int count = meta->input_count;
    if (count <= 0) {
        *out_bindings = NULL;
        *out_count = 0;
        return true;
    }

    PresetParamBinding *bindings = lv_malloc((size_t)count * sizeof(PresetParamBinding));
    if (!bindings) return false;

    for (int i = 0; i < count; i++) {
        bindings[i].param_index = i;
        bindings[i].bound_node_id = -1;
        bindings[i].is_bound = false;
    }

    *out_bindings = bindings;
    *out_count = count;
    return true;
}

void preset_bindings_destroy(PresetParamBinding *bindings)
{
    lv_free((void **)&bindings);
}

bool preset_partial_bind(const char *preset_name,
                          const PresetParamBinding *bindings,
                          int binding_count,
                          char **out_new_preset_name)
{
    if (!preset_name || !bindings || !out_new_preset_name || binding_count <= 0) {
        return false;
    }

    /* 查找原预设 */
    FuncBlock *fb = func_block_registry_lookup(preset_name);
    if (!fb) return false;

    /* 【修复】验证查找返回的函数块状态完整性 */
    if (fb->input_count < -1 || fb->output_count < -1) {
        func_block_destroy(fb);
        return false;
    }

    /* 生成新预设名称 */
    char new_name[MAX_PRESET_NAME_LENGTH];
    snprintf(new_name, sizeof(new_name), "%s_bound_%d",
             preset_name, atomic_fetch_add(&g_bind_counter, 1));

    /* 创建新函数块 */
    FuncBlock *new_fb = func_block_create(fb->id + 10000);
    if (!new_fb) {
        func_block_destroy(fb);
        return false;
    }

    /* 复制基本信息 */
    if (fb->name) {
        new_fb->name = lv_strdup(new_name);
    }
    if (fb->description) {
        char desc_buf[512];
        snprintf(desc_buf, sizeof(desc_buf), "%s（部分绑定）", fb->description);
        new_fb->description = lv_strdup(desc_buf);
    }

    /* 计算剩余参数 */
    int remaining_count = 0;
    for (int i = 0; i < binding_count; i++) {
        if (!bindings[i].is_bound) {
            remaining_count++;
        }
    }

    /* 设置输入端口（只包含未绑定的参数） */
    if (remaining_count > 0) {
        int *remaining_ports = lv_malloc((size_t)remaining_count * sizeof(int));
        if (!remaining_ports) {
            func_block_destroy(fb);
            func_block_destroy(new_fb);
            return false;
        }

        int idx = 0;
        for (int i = 0; i < binding_count; i++) {
            if (!bindings[i].is_bound && idx < remaining_count) {
                remaining_ports[idx++] = i;
            }
        }

        func_block_set_input_ports(new_fb, remaining_ports, remaining_count);
        lv_free((void **)&remaining_ports);
    }

    /* 复制输出端口 */
    func_block_set_output_ports(new_fb, fb->output_port_ids, fb->output_count);

    /* 复制内部节点 */
    func_block_set_internal_nodes(new_fb, fb->internal_node_ids, fb->internal_node_count);

    /* 注册新预设 */
    bool registered = func_block_register(new_name, new_fb->description,
                                           PRESET_CATEGORY_CONSTRUCTION, new_fb);

    func_block_destroy(fb);
    func_block_destroy(new_fb);

    if (registered) {
        *out_new_preset_name = lv_strdup(new_name);
        return true;
    }

    return false;
}

/* ================================================================
 * 预设搜索与推荐实现
 * ================================================================ */

int preset_search_by_signature(int input_count,
                                int output_count,
                                PresetSearchResult *out_result)
{
    if (!out_result) return 0;

    out_result->names = NULL;
    out_result->relevance_scores = NULL;
    out_result->count = 0;

    /* 临时存储匹配结果 */
    char **names = lv_malloc(64 * sizeof(char *));
    double *scores = lv_malloc(64 * sizeof(double));
    if (!names || !scores) {
        lv_free((void **)&names);
        lv_free((void **)&scores);
        return 0;
    }

    int count = 0;
    int capacity = 64;

    /* 遍历所有预设 */
    const char *all_names[256];
    int total = preset_blocks_get_all_names(all_names, 256);

    for (int i = 0; i < total; i++) {
        const PresetBlockMetadata *meta = preset_blocks_get_metadata(all_names[i]);
        if (!meta) continue;

        bool match = true;
        double score = 1.0;

        /* 检查输入数量 */
        if (input_count >= 0) {
            if (meta->input_count == input_count) {
                score += 1.0;
            } else if (meta->input_count != -1) {
                match = false;
            }
        }

        /* 检查输出数量 */
        if (output_count >= 0) {
            if (meta->output_count == output_count) {
                score += 1.0;
            } else if (meta->output_count != -1) {
                match = false;
            }
        }

        if (match && count < capacity) {
            names[count] = lv_strdup(meta->name);
            scores[count] = score;
            count++;
        }
    }

    if (count > 0) {
        out_result->names = names;
        out_result->relevance_scores = scores;
        out_result->count = count;
    } else {
        lv_free((void **)&names);
        lv_free((void **)&scores);
    }

    return count;
}

int preset_recommend_related(const char *preset_name,
                              PresetSearchResult *out_result)
{
    if (!preset_name || !out_result) return 0;

    out_result->names = NULL;
    out_result->relevance_scores = NULL;
    out_result->count = 0;

    /* 获取参考预设 */
    const PresetBlockMetadata *ref_meta = preset_blocks_get_metadata(preset_name);
    if (!ref_meta) return 0;

    /* 临时存储 */
    char **names = lv_malloc(64 * sizeof(char *));
    double *scores = lv_malloc(64 * sizeof(double));
    if (!names || !scores) {
        lv_free((void **)&names);
        lv_free((void **)&scores);
        return 0;
    }

    int count = 0;
    int capacity = 64;

    /* 遍历所有预设 */
    const char *all_names[256];
    int total = preset_blocks_get_all_names(all_names, 256);

    for (int i = 0; i < total; i++) {
        if (strcmp(all_names[i], preset_name) == 0) continue;

        const PresetBlockMetadata *meta = preset_blocks_get_metadata(all_names[i]);
        if (!meta) continue;

        double score = 0.0;

        /* 同类别加分 */
        if (meta->category == ref_meta->category) {
            score += 2.0;
        }

        /* 相同输入输出数量加分 */
        if (meta->input_count == ref_meta->input_count) {
            score += 0.5;
        }
        if (meta->output_count == ref_meta->output_count) {
            score += 0.5;
        }

        /* 名称相似性 */
        if (strstr(meta->name, preset_name) ||
            strstr(preset_name, meta->name)) {
            score += 1.0;
        }

        if (score > 1.0 && count < capacity) {
            names[count] = lv_strdup(meta->name);
            scores[count] = score;
            count++;
        }
    }

    if (count > 0) {
        out_result->names = names;
        out_result->relevance_scores = scores;
        out_result->count = count;
    } else {
        lv_free((void **)&names);
        lv_free((void **)&scores);
    }

    return count;
}

void preset_search_result_destroy(PresetSearchResult *result)
{
    if (!result) return;

    if (result->names) {
        for (int i = 0; i < result->count; i++) {
            lv_free((void **)&result->names[i]);
        }
        lv_free((void **)&result->names);
    }

    lv_free((void **)&result->relevance_scores);
    result->count = 0;
}

/* ================================================================
 * 预设组合操作实现
 * ================================================================ */

static bool preset_compose(const char *preset_a,
                     const char *preset_b,
                     PresetComposeMode mode,
                     char **out_composed_name)
{
    if (!preset_a || !preset_b || !out_composed_name) return false;

    /* 查找预设 */
    FuncBlock *fb_a = func_block_registry_lookup(preset_a);
    FuncBlock *fb_b = func_block_registry_lookup(preset_b);

    if (!fb_a || !fb_b) {
        if (fb_a) func_block_destroy(fb_a);
        if (fb_b) func_block_destroy(fb_b);
        return false;
    }

    /* 生成新名称 */
    char new_name[MAX_PRESET_NAME_LENGTH];

    switch (mode) {
        case PRESET_COMPOSE_SEQUENCE:
            snprintf(new_name, sizeof(new_name), "composed_seq_%d", atomic_fetch_add(&g_compose_counter, 1));
            break;
        case PRESET_COMPOSE_PARALLEL:
            snprintf(new_name, sizeof(new_name), "composed_par_%d", atomic_fetch_add(&g_compose_counter, 1));
            break;
        case PRESET_COMPOSE_FEEDBACK:
            snprintf(new_name, sizeof(new_name), "composed_fb_%d", atomic_fetch_add(&g_compose_counter, 1));
            break;
        case PRESET_COMPOSE_BRANCH:
            snprintf(new_name, sizeof(new_name), "composed_br_%d", atomic_fetch_add(&g_compose_counter, 1));
            break;
        default:
            func_block_destroy(fb_a);
            func_block_destroy(fb_b);
            return false;
    }

    /* 使用现有的组合函数 */
    FuncBlock *composed = NULL;
    bool success = false;

    /*
     * 创建临时约束图用于组合操作。
     *
     * func_block_compose / func_block_product 需要 graph 参数来分配全局唯一 ID
     *（通过 graph->next_node_id++），传入 NULL 会导致函数直接返回 false，
     * 使组合操作永远失败。此处创建临时图仅为获取 ID 分配能力，
     * 组合结果不会被添加到此图中。
     */
    ConstraintGraph *temp_graph = graph_create();
    if (!temp_graph) {
        func_block_destroy(fb_a);
        func_block_destroy(fb_b);
        return false;
    }

    switch (mode) {
        case PRESET_COMPOSE_SEQUENCE:
            /* 顺序组合：a -> b，需要 a 的输出数 = b 的输入数 */
            if (fb_a->output_count == fb_b->input_count) {
                /* 使用 func_block_compose（注意顺序是 g ∘ f） */
                success = func_block_compose(fb_a, fb_b, temp_graph, &composed);
            }
            break;

        case PRESET_COMPOSE_PARALLEL:
            /* 并行组合 */
            success = func_block_product(fb_a, fb_b, temp_graph, &composed);
            break;

        default:
            /* 其他模式暂不支持 */
            break;
    }

    /* 销毁临时约束图（组合结果已独立持有，不依赖此图） */
    graph_destroy(temp_graph);

    func_block_destroy(fb_a);
    func_block_destroy(fb_b);

    if (success && composed) {
        /* 设置名称 */
        if (composed->name) lv_free((void **)&composed->name);
        composed->name = lv_strdup(new_name);

        /* 注册新预设 */
        bool registered = func_block_register(new_name, composed->description,
                                               PRESET_CATEGORY_CONSTRUCTION, composed);
        func_block_destroy(composed);

        if (registered) {
            *out_composed_name = lv_strdup(new_name);
            return true;
        }
    }

    return false;
}

bool preset_make_recursive(const char *base_preset,
                            int max_iterations,
                            char **out_recursive_name)
{
    if (!base_preset || !out_recursive_name || max_iterations <= 0) {
        return false;
    }

    /* 查找基础预设 */
    FuncBlock *fb = func_block_registry_lookup(base_preset);
    if (!fb) return false;

    /* 验证预设可以递归（输入输出兼容） */
    if (fb->output_count != fb->input_count) {
        func_block_destroy(fb);
        lv_ERROR_SET(lv_ERROR_INVALID_PARAM,
                       "预设 '%s' 的输入输出数量不匹配，无法递归",
                       base_preset);
        return false;
    }

    /* 生成递归预设名称 */
    char new_name[MAX_PRESET_NAME_LENGTH];
    snprintf(new_name, sizeof(new_name), "recursive_%s_%d",
             base_preset, atomic_fetch_add(&g_recursive_counter, 1));

    /* 创建递归预设（简化实现：复制原预设并添加递归标记） */
    FuncBlock *recursive_fb = func_block_copy(fb);
    func_block_destroy(fb);

    if (!recursive_fb) return false;

    /* 更新名称和描述 */
    if (recursive_fb->name) {
        lv_free((void **)&recursive_fb->name);
    }
    recursive_fb->name = lv_strdup(new_name);

    if (recursive_fb->description) {
        char desc_buf[512];
        snprintf(desc_buf, sizeof(desc_buf),
                 "%s（递归版本，最大迭代%d次）",
                 recursive_fb->description, max_iterations);
        lv_free((void **)&recursive_fb->description);
        recursive_fb->description = lv_strdup(desc_buf);
    }

    /* 注册递归预设 */
    bool registered = func_block_register(new_name, recursive_fb->description,
                                           PRESET_CATEGORY_CONSTRUCTION, recursive_fb);
    func_block_destroy(recursive_fb);

    if (registered) {
        *out_recursive_name = lv_strdup(new_name);
        return true;
    }

    return false;
}

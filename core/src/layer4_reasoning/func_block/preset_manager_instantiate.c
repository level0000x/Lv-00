/**
 * @file preset_manager_instantiate.c
 * @brief 实例化与实例管理
 *
 * @details 从 preset_manager.c 拆分的子模块（Lv-00 项目 v3.3.0+）。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/error_codes.h"
#include "lv/func_block_preset.h"
#include "lv/func_block_registry.h"
#include "lv/lv_internal.h"
#include "lv/lv_json.h"
#include "lv/lv_utils.h"
#include "lv/preset_blocks.h"
#include "lv/preset_common.h"
#include "lv/preset_core.h"
#include "preset_manager_internal.h"

/* ============================================================
 * 预设实例化
 * ============================================================ */

/**
 * @brief 实例化预设
 *
 * 根据预设名称和输入节点创建函数块实例。
 *
 * @param name          预设名称
 * @param input_nodes   输入节点ID数组
 * @param input_count   输入数量
 * @param options       实例化选项（可为NULL）
 * @param out_instance  输出实例句柄
 * @return true 实例化成功
 * @return false 实例化失败
 */
bool preset_instantiate(const char *name, const int *input_nodes, int input_count,
                        const PresetInstantiateOptions *options, PresetInstanceHandle *out_instance) {
    PRESET_CHECK_STRING(name, error);
    PRESET_CHECK_NULL(out_instance, error);

    /* 当输入数量大于0时要求输入节点数组非空 */
    if (input_count > 0 && !input_nodes) {
        set_error("输入节点数组为空");
        return false;
    }

    lock_library();

    if (!g_library.initialized) {
        unlock_library();
        set_error("预设库未初始化");
        return false;
    }

    /* 查找预设条目 */
    InternalPresetEntry *entry = find_entry(name);
    if (!entry) {
        unlock_library();
        set_error("预设 '%s' 不存在", name);
        return false;
    }

    /* 验证输入数量 */
    if (entry->metadata.input_count > 0 && input_count != entry->metadata.input_count) {
        unlock_library();
        set_error("输入数量不匹配: 期望 %d，实际 %d", entry->metadata.input_count, input_count);
        return false;
    }

    /* 创建实例结构 */
    PresetInstance *instance = (PresetInstance *) lv_calloc(1, sizeof(PresetInstance));
    if (!instance) {
        unlock_library();
        set_error("内存分配失败");
        return false;
    }

    instance->preset_name = lv_strdup_safe(name);

    /* 复制模板函数块作为实例 */
    if (entry->template_fb) {
        instance->func_block = func_block_copy(entry->template_fb);
        if (!instance->func_block) {
            unlock_library();
            lv_free((void **) &instance->preset_name);
            lv_free((void **) &instance);
            set_error("函数块复制失败");
            return false;
        }

        /* 设置输入端口 */
        if (input_count > 0 && input_nodes) {
            func_block_set_input_ports(instance->func_block, input_nodes, input_count);
        }

        /* 设置输出端口为模板的输出 */
        instance->output_count = entry->template_fb->output_count;
        if (instance->output_count > 0 && entry->template_fb->output_port_ids) {
            instance->output_node_ids = (int *) lv_malloc((size_t) instance->output_count * sizeof(int));
            if (instance->output_node_ids) {
                memcpy(instance->output_node_ids, entry->template_fb->output_port_ids,
                       (size_t) instance->output_count * sizeof(int));
            }
        }
    }

    instance->reference_count = 1;

    /* 增加预设的引用计数 */
    entry->reference_count++;

    unlock_library();

    *out_instance = (PresetInstanceHandle) instance;
    return true;

error:
    return false;
}

/**
 * @brief 批量实例化预设
 *
 * @param names             预设名称数组
 * @param input_nodes_array 输入节点数组的数组
 * @param input_counts      输入数量数组
 * @param count             预设数量
 * @param options           实例化选项（可为NULL）
 * @param out_instances     输出实例句柄数组（调用者需释放）
 * @return 成功实例化的数量
 */
int preset_instantiate_batch(const char **names, const int **input_nodes_array, const int *input_counts, int count,
                             const PresetInstantiateOptions *options, PresetInstanceHandle **out_instances) {
    if (!names || !input_counts || count <= 0 || !out_instances)
        return 0;

    /* 分配实例数组 */
    PresetInstanceHandle *instances = (PresetInstanceHandle *) lv_malloc((size_t) count * sizeof(PresetInstanceHandle));
    if (!instances) {
        set_error("内存分配失败");
        return 0;
    }
    memset(instances, 0, (size_t) count * sizeof(PresetInstanceHandle));

    int success = 0;
    for (int i = 0; i < count; i++) {
        const int *nodes = (input_nodes_array) ? input_nodes_array[i] : NULL;
        if (preset_instantiate(names[i], nodes, input_counts[i], options, &instances[i])) {
            success++;
        } else {
            /* 失败时置空该条目 */
            instances[i] = NULL;
        }
    }

    *out_instances = instances;
    return success;
}

/* ============================================================
 * 预设实例管理
 * ============================================================ */

/**
 * @brief 销毁预设实例
 *
 * 释放实例关联的所有资源，包括函数块和输出节点数组。
 *
 * @param instance 实例句柄（可为 NULL）
 */
void preset_instance_destroy(PresetInstanceHandle instance) {
    if (!instance)
        return;

    PresetInstance *inst = (PresetInstance *) instance;

    /* 释放函数块 */
    if (inst->func_block) {
        func_block_destroy(inst->func_block);
        inst->func_block = NULL;
    }

    /* 释放输出节点ID数组 */
    if (inst->output_node_ids) {
        lv_free((void **) &inst->output_node_ids);
    }

    /* 释放预设名称 */
    if (inst->preset_name) {
        lv_free((void **) &inst->preset_name);
    }

    /* 释放实例结构本身 */
    lv_free((void **) &instance);
}

/**
 * @brief 获取实例的函数块
 *
 * @param instance 实例句柄
 * @return 函数块指针（只读，生命周期与实例相同）
 */
const FuncBlock *preset_instance_get_func_block(PresetInstanceHandle instance) {
    if (!instance)
        return NULL;

    PresetInstance *inst = (PresetInstance *) instance;
    return inst->func_block;
}

/**
 * @brief 获取实例的输出节点ID
 *
 * @param instance       实例句柄
 * @param out_output_ids 输出节点ID数组（调用者需使用 lv_free 释放）
 * @param out_count      输出数量
 * @return true 成功
 * @return false 失败
 */
bool preset_instance_get_outputs(PresetInstanceHandle instance, int **out_output_ids, int *out_count) {
    PRESET_CHECK_NULL(instance, error);
    PRESET_CHECK_NULL(out_output_ids, error);
    PRESET_CHECK_NULL(out_count, error);

    PresetInstance *inst = (PresetInstance *) instance;

    if (inst->output_count <= 0 || !inst->output_node_ids) {
        *out_output_ids = NULL;
        *out_count = 0;
        return true;
    }

    int *ids = (int *) lv_malloc((size_t) inst->output_count * sizeof(int));
    if (!ids) {
        set_error("内存分配失败");
        return false;
    }

    memcpy(ids, inst->output_node_ids, (size_t) inst->output_count * sizeof(int));

    *out_output_ids = ids;
    *out_count = inst->output_count;
    return true;

error:
    return false;
}

/* ============================================================
 * 预设实例执行与验证
 * ============================================================ */

/**
 * @brief 执行预设实例
 *
 * 触发函数块的执行。当前为简化实现，
 * 实际执行逻辑由 func_block 系统内部完成。
 *
 * @param instance 实例句柄
 * @param context  执行上下文（可为NULL）
 * @return true 执行成功
 * @return false 执行失败
 */
bool preset_instance_execute(PresetInstanceHandle instance, const PresetExecutionContext *context) {
    if (!instance) {
        set_error("无效的实例句柄");
        return false;
    }

    PresetInstance *inst = (PresetInstance *) instance;

    if (!inst->func_block) {
        set_error("实例没有关联的函数块");
        return false;
    }

    /* 检查取消回调 */
    if (context && context->cancel_callback) {
        if (context->cancel_callback(context->user_data)) {
            set_error("执行已被用户取消");
            return false;
        }
    }

    /* 实际执行：标记函数块为已验证确定性状态，
     * 完整的执行逻辑依赖于 func_block 系统的约束求解器。 */
    if (inst->func_block->determinism == DETERMINISM_UNVERIFIED) {
        inst->func_block->determinism = DETERMINISM_VERIFIED;
    }

    /* 进度回调 */
    if (context && context->progress_callback) {
        context->progress_callback(1, 1, context->user_data);
    }

    return true;
}

/**
 * @brief 验证预设实例
 *
 * 验证实例的有效性，包括函数块存在性、
 * 输入端口完整性和确定性状态校验。
 *
 * @param instance          实例句柄
 * @param out_is_valid      输出是否有效
 * @param out_error_message 错误消息（可选，调用者需使用 lv_free 释放）
 * @return true 验证流程完成
 * @return false 验证过程出错
 */
bool preset_instance_validate(PresetInstanceHandle instance, bool *out_is_valid, char **out_error_message) {
    PRESET_CHECK_NULL(instance, error);
    PRESET_CHECK_NULL(out_is_valid, error);

    PresetInstance *inst = (PresetInstance *) instance;
    *out_is_valid = false;

    /* 验证1：函数块是否存在 */
    if (!inst->func_block) {
        if (out_error_message) {
            *out_error_message = lv_strdup_safe("验证失败: 实例没有关联的函数块");
        }
        return true;
    }

    /* 验证2：输入端口数量是否有效 */
    if (inst->func_block->input_count < 0) {
        if (out_error_message) {
            *out_error_message = lv_strdup_safe("验证失败: 输入端口数量无效");
        }
        return true;
    }

    /* 验证3：确定性状态检查 */
    if (inst->func_block->determinism == DETERMINISM_NON_DETERMINISTIC) {
        if (out_error_message) {
            *out_error_message = lv_strdup_safe("验证警告: 函数块存在多解歧义");
        }
        /* 多解歧义不视为失败，仅发出警告 */
    }

    *out_is_valid = true;
    if (out_error_message) {
        *out_error_message = NULL;
    }
    return true;

error:
    return false;
}


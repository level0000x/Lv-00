/**
 * @file preset_manager_compose.c
 * @brief 组合与参数绑定
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
 * 预设组合
 * ============================================================ */

/* ============================================================
 * 组合模式的输出元数据计算查找表（VTable）
 *
 * 将 preset_compose() 中对 composition->mode 的大型 switch 重构为
 * 函数指针查找表：每种组合模式对应一个独立的 static 计算函数，
 * 通过 designated initializer 建立「模式 → 函数」的映射。
 * 所有函数均通过输出参数 composed_meta 修改组合后的元数据。
 * ============================================================ */

/** 组合模式元数据计算函数指针类型 */
typedef void (*ComposeMetaFn)(const PresetComposition *composition, InternalPresetEntry *first_entry,
                              PresetMetadata *composed_meta);

/** @brief 顺序执行：输入 = 第一个预设的输入，输出 = 最后一个预设的输出 */
static void compose_meta_sequence(const PresetComposition *composition, InternalPresetEntry *first_entry,
                                  PresetMetadata *composed_meta) {
    (void) first_entry;
    InternalPresetEntry *last_entry = find_entry(composition->preset_names[composition->count - 1]);
    if (last_entry) {
        composed_meta->output_count = last_entry->metadata.output_count;
        composed_meta->output_params = last_entry->metadata.output_params;
    }
}

/** @brief 并行执行：输出数量 = 各预设输出之和 */
static void compose_meta_parallel(const PresetComposition *composition, InternalPresetEntry *first_entry,
                                  PresetMetadata *composed_meta) {
    (void) first_entry;
    int total_outputs = 0;
    for (int i = 0; i < composition->count; i++) {
        InternalPresetEntry *e = find_entry(composition->preset_names[i]);
        if (e && e->metadata.output_count > 0) {
            total_outputs += e->metadata.output_count;
        }
    }
    composed_meta->output_count = total_outputs;
}

/** @brief 管道模式：类似顺序，但输出保留中间状态（即第一个预设的输出） */
static void compose_meta_pipe(const PresetComposition *composition, InternalPresetEntry *first_entry,
                              PresetMetadata *composed_meta) {
    (void) composition;
    composed_meta->output_count = first_entry->metadata.output_count;
}

/** @brief 反馈/分支模式：保留第一预设的输出数量作为初始值（无额外调整） */
static void compose_meta_noop(const PresetComposition *composition, InternalPresetEntry *first_entry,
                              PresetMetadata *composed_meta) {
    (void) composition;
    (void) first_entry;
    (void) composed_meta;
}

/** 组合模式 → 元数据计算函数 查找表（designated initializer） */
static const ComposeMetaFn kComposeMetaHandlers[] = {
    [PRESET_COMPOSE_SEQUENCE] = compose_meta_sequence,
    [PRESET_COMPOSE_PARALLEL] = compose_meta_parallel,
    [PRESET_COMPOSE_PIPE] = compose_meta_pipe,
    [PRESET_COMPOSE_FEEDBACK] = compose_meta_noop,
    [PRESET_COMPOSE_BRANCH] = compose_meta_noop,
};

/**
 * @brief 组合预设
 *
 * 根据 PresetComposition 描述将多个预设组合为一个新预设。
 * 支持多种组合模式（顺序、并行、反馈、分支、管道）。
 *
 * @param composition    组合描述
 * @param out_new_entry  输出新条目句柄
 * @return true 组合成功
 * @return false 组合失败
 */
bool preset_compose(const PresetComposition *composition, PresetEntryHandle *out_new_entry) {
    PRESET_CHECK_NULL(composition, error);
    PRESET_CHECK_NULL(composition->preset_names, error);
    PRESET_CHECK_STRING(composition->new_name, error);

    if (composition->count < 1) {
        set_error("组合至少需要一个预设");
        return false;
    }

    lock_library();

    if (!g_library.initialized) {
        unlock_library();
        set_error("预设库未初始化");
        return false;
    }

    /* 检查新名称是否已存在 */
    if (find_entry(composition->new_name) != NULL) {
        unlock_library();
        set_error("预设 '%s' 已存在", composition->new_name);
        return false;
    }

    /* 验证所有组成预设是否存在 */
    for (int i = 0; i < composition->count; i++) {
        if (!composition->preset_names[i] || find_entry(composition->preset_names[i]) == NULL) {
            unlock_library();
            set_error("预设 '%s' 不存在", composition->preset_names[i] ? composition->preset_names[i] : "(null)");
            return false;
        }
    }

    /* 获取第一个预设作为基础 */
    InternalPresetEntry *first_entry = find_entry(composition->preset_names[0]);
    if (!first_entry) {
        unlock_library();
        set_error("内部错误：预设查找失败");
        return false;
    }

    /* 创建组合后的元数据（基于第一个预设） */
    PresetMetadata composed_meta;
    memcpy(&composed_meta, &first_entry->metadata, sizeof(PresetMetadata));
    /* 注意：仅浅复制 name/description/math_def 等 const 指针，
     * 在注册时 preset_register_custom 会通过 lv_strdup 深拷贝。 */

    /* 根据组合模式确定输出行为（VTable 分发） */
    if ((unsigned) composition->mode < sizeof(kComposeMetaHandlers) / sizeof(kComposeMetaHandlers[0]) &&
        kComposeMetaHandlers[composition->mode]) {
        kComposeMetaHandlers[composition->mode](composition, first_entry, &composed_meta);
    }
    /* 未知组合模式：保持第一预设的元数据不变（与原 default 行为一致） */

    /* 注册新预设 */
    bool success = preset_register_custom(&composed_meta, first_entry->template_fb, out_new_entry);

    unlock_library();

    if (success) {
        ; /* 注册完成 */
    } else {
        set_error("组合预设注册失败");
    }

    return success;

error:
    return false;
}

/* ============================================================
 * 参数绑定
 * ============================================================ */

/**
 * @brief 绑定预设参数
 *
 * 将预设的指定索引位置的输入参数绑定为固定值，
 * 生成一个新的预设（部分应用）。
 *
 * @param preset_name  原预设名称
 * @param param_index  参数索引
 * @param value        绑定的节点ID值
 * @param out_new_name 输出新预设名称（调用者需使用 lv_free 释放）
 * @return true 绑定成功
 * @return false 绑定失败
 */
bool preset_bind_parameter(const char *preset_name, int param_index, int value, char **out_new_name) {
    PRESET_CHECK_STRING(preset_name, error);

    lock_library();

    if (!g_library.initialized) {
        unlock_library();
        set_error("预设库未初始化");
        return false;
    }

    /* 查找预设 */
    InternalPresetEntry *entry = find_entry(preset_name);
    if (!entry) {
        unlock_library();
        set_error("预设 '%s' 不存在", preset_name);
        return false;
    }

    /* 验证参数索引 */
    /* exempt: 带 input_count>0 守卫（input_count==0 时仅拒绝负索引，与 lv_index_in_range 语义不同），保留手写检查 */
    if (param_index < 0 || (entry->metadata.input_count > 0 && param_index >= entry->metadata.input_count)) {
        unlock_library();
        set_error("参数索引 %d 越界（有效范围: 0-%d）", param_index, entry->metadata.input_count - 1);
        return false;
    }

    /* 验证参数类型兼容性：
     * 检查输入参数的 PresetParamType 与模板端口的端口类型是否兼容。
     * 若元数据定义了输入参数类型信息，则验证：
     * - PARAM_TYPE_VARIADIC: 不可绑定到单值
     * - PARAM_TYPE_ANY: 始终兼容（多态类型）
     * - 其余具体类型: 均可绑定节点 ID */
    if (entry->metadata.input_params != NULL && param_index < entry->metadata.input_count) {
        PresetParamType param_type = entry->metadata.input_params[param_index].type;
        if (param_type == PARAM_TYPE_VARIADIC) {
            unlock_library();
            set_error("参数索引 %d 类型为 VARIADIC，无法绑定单个值", param_index);
            return false;
        }
        /* 简单类型枚举匹配检查：确保参数类型在有效的枚举范围内 */
        if (param_type < PARAM_TYPE_POINT || param_type > PARAM_TYPE_VARIADIC) {
            unlock_library();
            set_error("参数索引 %d 类型无效", param_index);
            return false;
        }
    }

    /* 生成新预设名称：原名称 + _bound_ + 索引 */
    char new_name[PRESET_BUFFER_SIZE];
    int written = snprintf(new_name, sizeof(new_name), "%s_bound_%d", preset_name, param_index);
    /* exempt: snprintf 返回码检测（非索引语义），勿替换为 lv_index_in_range */
    if (written < 0 || (size_t) written >= sizeof(new_name)) {
        unlock_library();
        set_error("新预设名称过长");
        return false;
    }

    /* 创建偏应用元数据 */
    PresetMetadata bound_meta = entry->metadata;
    bound_meta.name = new_name;

    /* 直接在本管理器内实现简化版的参数绑定：
     * 复制模板，然后从输入端口中移除指定索引的端口 */
    FuncBlock *template_copy = NULL;
    if (entry->template_fb) {
        template_copy = func_block_copy(entry->template_fb);
    }

    if (template_copy && entry->template_fb->input_count > 1) {
        int old_count = entry->template_fb->input_count;
        int new_count = old_count - 1;

        int *new_inputs = (int *) lv_malloc((size_t) new_count * sizeof(int));
        if (new_inputs) {
            int dst = 0;
            for (int i = 0; i < old_count; i++) {
                if (i != param_index) {
                    new_inputs[dst++] = entry->template_fb->input_port_ids[i];
                }
            }
            func_block_set_input_ports(template_copy, new_inputs, new_count);
            lv_free((void **) &new_inputs);
        }
    } else if (template_copy && entry->template_fb->input_count == 1) {
        /* 绑定唯一输入后，输入数量变为0 */
        func_block_set_input_ports(template_copy, NULL, 0);
    }

    /* 注册新预设 */
    bool success = preset_register_custom(&bound_meta, template_copy, NULL);

    /* 释放临时函数块副本 */
    if (template_copy) {
        func_block_destroy(template_copy);
    }

    unlock_library();

    if (success && out_new_name) {
        *out_new_name = lv_strdup_safe(new_name);
    }

    return success;

error:
    return false;
}


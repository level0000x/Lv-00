/**
 * @file func_block_preset_internal.c
 * @brief 库生命周期、实例化与内部辅助函数
 *
 * 从 func_block_preset.c 拆分的模块之一。
 *
 * @version v5.0.0
 */

#include "func_block_preset_internal.h"
#include "lv/lv_xmacro.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error_codes.h"
#include "lv_internal.h"
#include "lv_utils.h"

/* ============================================================
 * 内部辅助函数
 * ============================================================ */

/**
 * @brief 查找预设条目索引
 *
 * 在 g_preset_library.entries 数组中线性搜索指定名称的预设。
 * 仅搜索已激活（is_active == true）的条目。
 *
 * @param name 预设名称（区分大小写）
 * @return 找到时返回数组索引，未找到或参数无效返回 -1
 */
int find_preset_index(const char *name) {
    if (!name)
        return -1;

    for (int i = 0; i < g_preset_library.count; i++) {
        if (g_preset_library.entries[i].is_active && g_preset_library.entries[i].metadata.name &&
            strcmp(g_preset_library.entries[i].metadata.name, name) == 0) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief 创建预设模板函数块
 *
 * @param metadata 元数据
 * @return 模板函数块，失败返回NULL
 */
static FuncBlock *create_preset_template(const PresetMetadata *metadata) {
    if (!metadata)
        return NULL;

    int id = g_preset_library.next_preset_id++;
    FuncBlock *fb = func_block_create(id);
    if (!fb)
        return NULL;

    /* 设置名称和描述 */
    if (metadata->name) {
        fb->name = lv_strdup(metadata->name);
    }
    if (metadata->description) {
        fb->description = lv_strdup(metadata->description);
    }

    /* 设置确定性状态 */
    if (metadata->properties & PRESET_PROPERTY_DETERMINISTIC) {
        fb->determinism = DETERMINISM_VERIFIED;
    } else {
        fb->determinism = DETERMINISM_NON_DETERMINISTIC;
    }

    /* 设置输入输出数量（实际端口在实例化时创建） */
    fb->input_count = metadata->input_count;
    fb->output_count = metadata->output_count;

    return fb;
}

/**
 * @brief 注册单个内置预设
 *
 * @param metadata 元数据
 * @return true 成功，false 失败
 */
static bool register_builtin_preset(const PresetMetadata *metadata) {
    if (!metadata || g_preset_library.count >= MAX_PRESETS)
        return false;

    /* 检查是否已存在 */
    if (find_preset_index(metadata->name) >= 0)
        return false;

    /* 创建模板 */
    FuncBlock *template = create_preset_template(metadata);
    if (!template)
        return false;

    /* 添加到库 */
    int idx = g_preset_library.count++;
    g_preset_library.entries[idx].metadata = *metadata;
    g_preset_library.entries[idx].template_fb = template;
    g_preset_library.entries[idx].is_builtin = true;
    g_preset_library.entries[idx].is_active = true;

    return true;
}

/**
 * @brief 初始化所有内置预设
 *
 * @return true 成功，false 失败
 */
static bool init_builtin_presets(void) {
    for (int i = 0; i < g_builtin_count; i++) {
        if (!register_builtin_preset(&g_builtin_metadata[i])) {
            lv_LOG_WARNING("注册内置预设失败: %s", g_builtin_metadata[i].name);
            /* 继续注册其他预设 */
        }
    }
    return true;
}

/* ============================================================
 * 公共 API 实现
 * ============================================================ */

bool func_block_preset_library_init(void) {
    /* 幂等操作 */
    /* exempt: g_preset_library.initialized 带 reinit（cleanup 置 false 后
     * 可再次 init），lv_once 不可重置，故保留幂等守卫。 */
    if (g_preset_library.initialized) {
        return true;
    }

    /* 清空状态 */
    memset(&g_preset_library, 0, sizeof(g_preset_library));
    g_preset_library.next_preset_id = lv_PRESET_ID_OFFSET;

    /* 初始化内置预设 */
    if (!init_builtin_presets()) {
        return false;
    }

    g_preset_library.initialized = true;
    return true;
}

void func_block_preset_library_cleanup(void) {
    /* 释放所有模板函数块 */
    for (int i = 0; i < g_preset_library.count; i++) {
        if (g_preset_library.entries[i].template_fb) {
            func_block_destroy(g_preset_library.entries[i].template_fb);
            g_preset_library.entries[i].template_fb = NULL;
        }
    }

    /* 重置状态 */
    memset(&g_preset_library, 0, sizeof(g_preset_library));
}

const PresetMetadata *func_block_preset_get_metadata(const char *preset_name) {
    if (!preset_name)
        return NULL;

    int idx = find_preset_index(preset_name);
    if (idx < 0)
        return NULL;

    return &g_preset_library.entries[idx].metadata;
}

InstantiateResult func_block_preset_instantiate(const char *preset_name, const int *input_node_ids, int input_count,
                                                ConstraintGraph *graph, FuncBlock **out_func_block) {
    InstantiateOptions opts = {.auto_resolve_ambiguity = true,
                               .validate_constraints = true,
                               .add_to_graph = true,
                               .max_solutions = 1,
                               .default_selector = NULL};

    InstantiateDetails details;
    memset(&details, 0, sizeof(details));

    InstantiateResult result =
        func_block_preset_instantiate_ex(preset_name, input_node_ids, input_count, graph, &opts, &details);

    if (out_func_block) {
        *out_func_block = details.func_block;
    }

    /* 释放警告信息 */
    if (details.warnings) {
        for (int i = 0; i < details.warning_count; i++) {
            lv_free((void **) &details.warnings[i]);
        }
        lv_free((void **) &details.warnings);
    }
    lv_free((void **) &details.error_detail);

    return result;
}

InstantiateResult func_block_preset_instantiate_ex(const char *preset_name, const int *input_node_ids, int input_count,
                                                   ConstraintGraph *graph, const InstantiateOptions *options,
                                                   InstantiateDetails *out_details) {
    /* 参数检查 */
    if (!preset_name || !graph || !out_details) {
        if (out_details)
            out_details->result = INSTANTIATE_OUT_OF_MEMORY;
        return INSTANTIATE_OUT_OF_MEMORY;
    }

    memset(out_details, 0, sizeof(InstantiateDetails));

    /* 查找预设 */
    int idx = find_preset_index(preset_name);
    if (idx < 0) {
        out_details->result = INSTANTIATE_NO_SOLUTION;
        out_details->error_detail = lv_strdup("预设不存在");
        return INSTANTIATE_NO_SOLUTION;
    }

    const PresetMetadata *metadata = &g_preset_library.entries[idx].metadata;

    /* 验证输入数量 */
    if (metadata->input_count > 0 && input_count != metadata->input_count) {
        out_details->result = INSTANTIATE_PRECONDITION_FAILED;
        out_details->error_detail =
            lv_asprintf("输入参数数量不匹配: 需要%d个，提供%d个", metadata->input_count, input_count);
        return INSTANTIATE_PRECONDITION_FAILED;
    }

    /* 验证输入节点 */
    if (input_count > 0 && !input_node_ids) {
        out_details->result = INSTANTIATE_PRECONDITION_FAILED;
        out_details->error_detail = lv_strdup("输入节点ID为空");
        return INSTANTIATE_PRECONDITION_FAILED;
    }

    for (int i = 0; i < input_count; i++) {
        GeomNode *node = graph_get_node(graph, input_node_ids[i]);
        if (!node) {
            out_details->result = INSTANTIATE_PRECONDITION_FAILED;
            out_details->error_detail = lv_asprintf("输入节点%d不存在", input_node_ids[i]);
            return INSTANTIATE_PRECONDITION_FAILED;
        }
    }

    /* 创建函数块副本 */
    FuncBlock *fb = func_block_copy(g_preset_library.entries[idx].template_fb);
    if (!fb) {
        out_details->result = INSTANTIATE_OUT_OF_MEMORY;
        return INSTANTIATE_OUT_OF_MEMORY;
    }

    /* 设置输入端口 - 使用 func_block_set_input_ports 自动释放 func_block_copy 拷贝来的旧值，避免内存泄漏 */
    if (input_count > 0) {
        if (!func_block_set_input_ports(fb, input_node_ids, input_count)) {
            func_block_destroy(fb);
            out_details->result = INSTANTIATE_OUT_OF_MEMORY;
            return INSTANTIATE_OUT_OF_MEMORY;
        }
    } else {
        /* input_count == 0 时也需清空拷贝来的旧 input_port_ids，避免内存泄漏 */
        func_block_set_input_ports(fb, NULL, 0);
    }

    /* 添加到约束图（如果需要） */
    if (options && options->add_to_graph) {
        /* 这里应该创建实际的约束节点 */
        /* 简化实现：仅标记为已实例化 */
    }

    out_details->result = INSTANTIATE_OK;
    out_details->func_block = fb;

    return INSTANTIATE_OK;
}

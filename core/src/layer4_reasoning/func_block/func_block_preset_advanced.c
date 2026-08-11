/**
 * @file func_block_preset_advanced.c
 * @brief 高级预设操作：组合、偏应用、逆与注册
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
#include "lv/lv_numeric.h" /* lv_index_in_range */

/* ============================================================
 * 高级预设操作
 * ============================================================ */

bool func_block_preset_compose(const char *f_name, const char *g_name, const char *new_preset_name) {
    /* ── 第一步：参数验证 ── */
    if (!f_name || !g_name || !new_preset_name)
        return false;

    /* ── 第二步：检查两个输入预设是否存在 ── */
    if (!func_block_preset_exists(f_name) || !func_block_preset_exists(g_name)) {
        return false;
    }

    /* 检查同名预设是否已存在 */
    if (func_block_preset_exists(new_preset_name)) {
        return false;
    }

    /* ── 第三步：获取两个预设的元数据 ── */
    const PresetMetadata *f_meta = func_block_preset_get_metadata(f_name);
    const PresetMetadata *g_meta = func_block_preset_get_metadata(g_name);
    if (!f_meta || !g_meta)
        return false;

    /* ── 第四步：类型兼容性验证 ──
     * 组合 g(f(x)) 要求 f 的输出类型与 g 的输入类型兼容
     * 简化处理：若定义了 input_params/output_params 则检查；
     * 否则仅依赖输出/输入数量匹配。
     */
    if (f_meta->output_params != NULL && g_meta->input_params != NULL) {
        int min_count = (f_meta->output_count < g_meta->input_count) ? f_meta->output_count : g_meta->input_count;
        for (int i = 0; i < min_count; i++) {
            PresetParamType f_out_type = f_meta->output_params[i].type;
            PresetParamType g_in_type = g_meta->input_params[i].type;

            /* 任意类型兼容所有类型 */
            if (f_out_type == PARAM_TYPE_ANY || g_in_type == PARAM_TYPE_ANY)
                continue;

            /* 类型必须精确匹配或满足子类型兼容规则 */
            if (f_out_type != g_in_type) {
                bool compatible = false;
                /* 线段/射线 → 直线 */
                if (g_in_type == PARAM_TYPE_LINE &&
                    (f_out_type == PARAM_TYPE_SEGMENT || f_out_type == PARAM_TYPE_RAY)) {
                    compatible = true;
                }
                /* 圆弧 → 圆 */
                if (g_in_type == PARAM_TYPE_CIRCLE && f_out_type == PARAM_TYPE_ARC) {
                    compatible = true;
                }
                if (!compatible)
                    return false;
            }
        }
    }

    /* ── 第五步：创建组合预设的元数据 ──
     * 组合预设 g(f(x)) 的输入 = f 的输入，输出 = g 的输出
     */
    PresetMetadata composed_meta = *g_meta; /* 以 g 为基础复制 */
    composed_meta.name = new_preset_name;   /* 覆盖名称 */
    composed_meta.input_params = f_meta->input_params;
    composed_meta.input_count = f_meta->input_count;
    /* 组合复杂度取较大者 */
    if (f_meta->complexity > composed_meta.complexity) {
        composed_meta.complexity = f_meta->complexity;
    }
    /* 合并数学性质：组合保留两者的交集性质 */
    composed_meta.properties = f_meta->properties & g_meta->properties;

    /* ── 第六步：创建组合预设的模板函数块 ──
     * 使用 f 的模板作为基础，将 g 的输出端口信息写入
     */
    int f_idx = find_preset_index(f_name);
    int g_idx = find_preset_index(g_name);
    if (f_idx < 0 || g_idx < 0)
        return false;

    FuncBlock *f_template = g_preset_library.entries[f_idx].template_fb;
    FuncBlock *g_template = g_preset_library.entries[g_idx].template_fb;

    /* 创建组合模板：深拷贝 f 的模板，然后设置输出为 g 的输出 */
    FuncBlock *composed_template = func_block_copy(f_template);
    if (!composed_template)
        return false;

    /* 设置组合后的输出端口为 g 的输出端口 */
    if (g_template->output_count > 0 && g_template->output_port_ids) {
        func_block_set_output_ports(composed_template, g_template->output_port_ids, g_template->output_count);
    }

    /* 设置组合名称 */
    func_block_set_name(composed_template, composed_meta.name);

    /* ── 第七步：注册新预设到库中 ── */
    return func_block_preset_register_custom(&composed_meta, composed_template);
}

bool func_block_preset_partial(const char *preset_name, const int *fixed_param_indices, int fixed_count,
                               const char *new_preset_name) {
    /* ── 第一步：参数验证 ── */
    if (!preset_name || !new_preset_name)
        return false;
    if (fixed_count > 0 && !fixed_param_indices)
        return false;

    /* ── 第二步：检查原预设是否存在 ── */
    if (!func_block_preset_exists(preset_name))
        return false;

    /* 检查同名预设是否已存在 */
    if (func_block_preset_exists(new_preset_name))
        return false;

    /* ── 第三步：获取原预设的元数据和模板 ── */
    const PresetMetadata *meta = func_block_preset_get_metadata(preset_name);
    if (!meta)
        return false;

    int idx = find_preset_index(preset_name);
    if (idx < 0)
        return false;

    /* ── 第四步：验证待固定参数的索引合法性 ── */
    for (int i = 0; i < fixed_count; i++) {
        int param_idx = fixed_param_indices[i];
        if (!lv_index_in_range(param_idx, meta->input_count)) {
            /* 参数索引越界 */
            return false;
        }
        /* 检查是否有重复的固定索引 */
        for (int j = i + 1; j < fixed_count; j++) {
            if (fixed_param_indices[j] == param_idx) {
                /* 重复指定同一参数 */
                return false;
            }
        }
    }

    /* ── 第五步：创建偏应用后的预设元数据 ──
     * 偏应用预设的输入数量 = 原输入数量 - 已固定的参数数量
     */
    PresetMetadata partial_meta = *meta;
    partial_meta.name = new_preset_name;

    /* 若原输入数量有效（非可变），计算新的输入数量 */
    if (meta->input_count > 0) {
        partial_meta.input_count = meta->input_count - fixed_count;
        if (partial_meta.input_count < 0) {
            /* 固定参数数量超过原输入数量 */
            return false;
        }
    }

    /* ── 第六步：创建偏应用后的模板函数块 ──
     * 基于原模板深拷贝，然后从中移除已固定的输入端口
     */
    FuncBlock *template = g_preset_library.entries[idx].template_fb;
    FuncBlock *partial_template = func_block_copy(template);
    if (!partial_template)
        return false;

    /* 构建新的输入端口数组（跳过已固定的端口） */
    int new_input_count = partial_meta.input_count;
    if (new_input_count > 0 && template->input_count > 0) {
        int *new_input_ports = (int *) lv_malloc((size_t) new_input_count * sizeof(int));
        if (!new_input_ports) {
            func_block_destroy(partial_template);
            return false;
        }

        int src_idx = 0;
        for (int i = 0; i < template->input_count && src_idx < new_input_count; i++) {
            /* 检查当前索引是否需要被固定（跳过） */
            bool is_fixed = false;
            for (int j = 0; j < fixed_count; j++) {
                if (fixed_param_indices[j] == i) {
                    is_fixed = true;
                    break;
                }
            }
            if (!is_fixed) {
                new_input_ports[src_idx] = template->input_port_ids[i];
                src_idx++;
            }
        }

        /* 设置新的输入端口（func_block_set_input_ports 会自动释放旧值） */
        func_block_set_input_ports(partial_template, new_input_ports, new_input_count);
        lv_free((void **) &new_input_ports);
    }

    /* 设置新名称 */
    func_block_set_name(partial_template, partial_meta.name);

    /* ── 第七步：注册新预设到库中 ── */
    return func_block_preset_register_custom(&partial_meta, partial_template);
}

const char *func_block_preset_get_inverse(const char *preset_name) {
    /* 防止空指针解引用：preset_name 为 NULL 时直接返回 NULL */
    if (!preset_name)
        return NULL;

    /* 简化实现：返回常见逆操作 */
    if (strcmp(preset_name, "translation") == 0)
        return "translation";
    if (strcmp(preset_name, "rotation") == 0)
        return "rotation";
    if (strcmp(preset_name, "scaling") == 0)
        return "scaling";
    if (strcmp(preset_name, "inversion") == 0)
        return "inversion";
    if (strcmp(preset_name, "reflection_point") == 0)
        return "reflection_point";
    return NULL;
}

bool func_block_preset_register_custom(const PresetMetadata *metadata, const FuncBlock *template_fb) {
    if (!metadata || !template_fb)
        return false;
    if (g_preset_library.count >= MAX_PRESETS)
        return false;
    if (find_preset_index(metadata->name) >= 0)
        return false;

    FuncBlock *copy = func_block_copy(template_fb);
    if (!copy)
        return false;

    int idx = g_preset_library.count++;
    g_preset_library.entries[idx].metadata = *metadata;
    g_preset_library.entries[idx].template_fb = copy;
    g_preset_library.entries[idx].is_builtin = false;
    g_preset_library.entries[idx].is_active = true;

    /* 维护哈希索引（惰性创建；插入失败不影响正确性，回退线性） */
    if (!g_preset_library.preset_index)
        g_preset_library.preset_index = lv_hashtable_str_create(64);
    if (g_preset_library.preset_index)
        lv_hashtable_str_insert(g_preset_library.preset_index, metadata->name, (void *) (intptr_t) (idx + 1));

    return true;
}

int func_block_preset_count(void) {
    return func_block_registry_get_count();
}

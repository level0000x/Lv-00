/**
 * @file func_block_preset_query.c
 * @brief 预设查询、类型验证与枚举映射
 *
 * 从 func_block_preset.c 拆分的模块之一。
 *
 * @version v5.0.0
 */

#include "func_block_preset_internal.h"
#include "lv/lv_xmacro.h"
#include "lv/preset_category.h" /* LV_PRESET_CATEGORY_ENTRY 单一事实来源 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/error_codes.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"

void func_block_preset_free_details(InstantiateDetails *details) {
    if (!details)
        return;

    if (details->warnings) {
        for (int i = 0; i < details->warning_count; i++) {
            lv_free((void **) &details->warnings[i]);
        }
        lv_free((void **) &details->warnings);
    }

    lv_free((void **) &details->error_detail);
    lv_free((void **) &details->output_node_ids);

    if (details->func_block) {
        func_block_destroy(details->func_block);
        details->func_block = NULL;
    }
}

/* ================================================================
 * 节点类型 -> 预设参数类型 映射表（数据表化，替代 switch）
 * ================================================================ */

/* GeomNode 类型 → PresetParamType 映射表。
 * NODE_TYPE_* 取值为 -5..2（负值为 func_block 内部扩展类型，未在
 * GeomType 枚举中定义），统一加 NODE_TYPE_PARAM_SHIFT 偏移映射到表索引 0..7。 */
#define NODE_TYPE_PARAM_SHIFT 5

static const PresetParamType s_node_type_to_param_types[] = {
    [NODE_TYPE_VECTOR + NODE_TYPE_PARAM_SHIFT]  = PARAM_TYPE_VECTOR,
    [NODE_TYPE_SCALAR + NODE_TYPE_PARAM_SHIFT]  = PARAM_TYPE_SCALAR,
    [NODE_TYPE_POLYGON + NODE_TYPE_PARAM_SHIFT] = PARAM_TYPE_POLYGON,
    [NODE_TYPE_ARC + NODE_TYPE_PARAM_SHIFT]     = PARAM_TYPE_ARC,
    [NODE_TYPE_CIRCLE + NODE_TYPE_PARAM_SHIFT]  = PARAM_TYPE_CIRCLE,
    [NODE_TYPE_POINT + NODE_TYPE_PARAM_SHIFT]   = PARAM_TYPE_POINT,
    [NODE_TYPE_LINE + NODE_TYPE_PARAM_SHIFT]    = PARAM_TYPE_LINE,
    [NODE_TYPE_REGION + NODE_TYPE_PARAM_SHIFT]  = PARAM_TYPE_REGION,
};

bool func_block_preset_validate_types(const char *preset_name, GeomNode **input_nodes, int input_count,
                                      int *out_mismatch_index) {
    if (!preset_name)
        return false;

    /* 当 input_count == 0 时，input_nodes 为 NULL 是合理的，不应视为错误 */
    if (input_count > 0 && !input_nodes)
        return false;

    const PresetMetadata *metadata = func_block_preset_get_metadata(preset_name);
    if (!metadata)
        return false;

    /* ── 第一步：验证输入参数个数 ──
     * 可变输入（input_count == -1）跳过数量检查
     */
    if (metadata->input_count > 0 && input_count != metadata->input_count) {
        if (out_mismatch_index)
            *out_mismatch_index = 0;
        return false;
    }

    /* ── 第二步：逐个验证输入节点类型与预设期望类型的兼容性 ──
     * 若预设定义了 input_params 数组，则逐项对比；
     * 若 input_params 为空（简化的预设元数据），则仅依赖数量检查结果。
     */
    if (metadata->input_params != NULL && input_nodes != NULL) {
        int check_count = (input_count < metadata->input_count) ? input_count : metadata->input_count;

        for (int i = 0; i < check_count; i++) {
            /* 节点不存在视为类型不匹配 */
            if (input_nodes[i] == NULL) {
                if (out_mismatch_index)
                    *out_mismatch_index = i;
                return false;
            }

            /* 获取节点类型（通过 GeomNode 的类型字段） */
            const PresetParamType expected = metadata->input_params[i].type;

            /* 期望类型为任意类型（PARAM_TYPE_ANY）时跳过类型检查 */
            if (expected == PARAM_TYPE_ANY) {
                continue;
            }

            /* 获取节点实际类型映射（负值扩展类型经偏移映射到查找表） */
            PresetParamType actual = PARAM_TYPE_ANY;
            int type_idx = (int) input_nodes[i]->type + NODE_TYPE_PARAM_SHIFT;
            if (type_idx >= 0 && type_idx < (int) lv_ARRAY_SIZE(s_node_type_to_param_types))
                actual = s_node_type_to_param_types[type_idx];

            /* 类型兼容性检查：
             *   - 精确匹配：通过
             *   - 线段/射线均兼容直线类型
             *   - 圆弧兼容圆类型
             *   - 其他情况不匹配
             */
            bool compatible = (actual == expected);
            if (!compatible) {
                /* 线段/射线 → 直线兼容 */
                if (expected == PARAM_TYPE_LINE && (actual == PARAM_TYPE_SEGMENT || actual == PARAM_TYPE_RAY)) {
                    compatible = true;
                }
                /* 圆弧 → 圆兼容 */
                if (expected == PARAM_TYPE_CIRCLE && actual == PARAM_TYPE_ARC) {
                    compatible = true;
                }
            }

            if (!compatible) {
                if (out_mismatch_index)
                    *out_mismatch_index = i;
                return false;
            }
        }
    }

    /* ── 第三步：验证输出类型（若定义了输出参数） ──
     * 注：目前只做存在性检查，实际输出类型的验证
     * 需在实例化后根据生成的 GeomNode 类型进行。
     */
    if (metadata->output_params == NULL && metadata->output_count < 0) {
        /* 可变输出数量，不做静态验证 */
    }

    return true;
}

bool func_block_preset_validate_constraints(const char *preset_name, ConstraintGraph *graph, const int *input_node_ids,
                                            int input_count, const char **out_violated_constraint) {
    if (!preset_name || !graph)
        return false;

    const PresetMetadata *metadata = func_block_preset_get_metadata(preset_name);
    if (!metadata)
        return false;

    /* 简化实现：仅检查节点存在性 */
    for (int i = 0; i < input_count; i++) {
        if (!graph_get_node(graph, input_node_ids[i])) {
            if (out_violated_constraint) {
                *out_violated_constraint = "输入节点不存在";
            }
            return false;
        }
    }

    return true;
}

int func_block_preset_get_input_count(const char *preset_name) {
    const PresetMetadata *metadata = func_block_preset_get_metadata(preset_name);
    if (!metadata)
        return -1;
    return metadata->input_count;
}

int func_block_preset_get_output_count(const char *preset_name) {
    const PresetMetadata *metadata = func_block_preset_get_metadata(preset_name);
    if (!metadata)
        return -1;
    return metadata->output_count;
}

int func_block_preset_list(const char **out_names, int max_count, PresetCategory category) {
    if (!out_names || max_count <= 0)
        return 0;

    int count = 0;
    for (int i = 0; i < g_preset_library.count && count < max_count; i++) {
        if (!g_preset_library.entries[i].is_active)
            continue;

        if ((int) category < 0 || g_preset_library.entries[i].metadata.category == category) {
            out_names[count++] = g_preset_library.entries[i].metadata.name;
        }
    }

    return count;
}

bool func_block_preset_exists(const char *preset_name) {
    return find_preset_index(preset_name) >= 0;
}

/* ================================================================
 * 枚举 -> 名称 映射表（数据表化，替代 switch）
 * ================================================================ */

/** @brief func_block_preset_category_string 名称表（按枚举值升序，
 *  由共享条目宏 LV_PRESET_CATEGORY_ENTRY 生成，中文名以本查询侧为 UI 显示源） */
#define LV_PRESET_CATEGORY_ROW_ZH(ENUM, EN_KEY, ZH_NAME) { ZH_NAME, ENUM },
static const lvStrToEnumEntry s_func_block_preset_category_string_entries[] = {
    LV_PRESET_CATEGORY_ENTRY(LV_PRESET_CATEGORY_ROW_ZH)
};
#undef LV_PRESET_CATEGORY_ROW_ZH

const char *func_block_preset_category_string(PresetCategory category) {
    return lv_enum_to_str(s_func_block_preset_category_string_entries, lv_ARRAY_SIZE(s_func_block_preset_category_string_entries), (int) category, "未知类别");
}

/** @brief func_block_preset_param_type_string 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_func_block_preset_param_type_string_entries[] = {
    {"点", PARAM_TYPE_POINT},
    {"直线", PARAM_TYPE_LINE},
    {"线段", PARAM_TYPE_SEGMENT},
    {"射线", PARAM_TYPE_RAY},
    {"圆", PARAM_TYPE_CIRCLE},
    {"圆弧", PARAM_TYPE_ARC},
    {"多边形", PARAM_TYPE_POLYGON},
    {"区域", PARAM_TYPE_REGION},
    {"角度", PARAM_TYPE_ANGLE},
    {"向量", PARAM_TYPE_VECTOR},
    {"标量", PARAM_TYPE_SCALAR},
    {"布尔值", PARAM_TYPE_BOOLEAN},
    {"曲线", PARAM_TYPE_CURVE},
    {"曲面", PARAM_TYPE_SURFACE},
    {"任意类型", PARAM_TYPE_ANY},
    {"可变参数", PARAM_TYPE_VARIADIC},
};

const char *func_block_preset_param_type_string(PresetParamType type) {
    return lv_enum_to_str(s_func_block_preset_param_type_string_entries, lv_ARRAY_SIZE(s_func_block_preset_param_type_string_entries), (int) type, "未知类型");
}

/** @brief func_block_preset_complexity_string 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_func_block_preset_complexity_string_entries[] = {
    {"O(1) - 常数时间", COMPLEXITY_O1},
    {"O(log n) - 对数时间", COMPLEXITY_OLOGN},
    {"O(n) - 线性时间", COMPLEXITY_ON},
    {"O(n log n) - 线性对数", COMPLEXITY_ONLOGN},
    {"O(n²) - 平方时间", COMPLEXITY_ON2},
    {"O(n³) - 立方时间", COMPLEXITY_ON3},
    {"未知", COMPLEXITY_UNKNOWN},
};

const char *func_block_preset_complexity_string(PresetComplexity complexity) {
    return lv_enum_to_str(s_func_block_preset_complexity_string_entries, lv_ARRAY_SIZE(s_func_block_preset_complexity_string_entries), (int) complexity, "未知");
}

int func_block_preset_properties_string(PresetProperty properties, char *out_buffer, size_t buffer_size) {
    if (!out_buffer || buffer_size == 0)
        return 0;

    const char *props[] = {properties & PRESET_PROPERTY_IDEMPOTENT ? "幂等" : NULL,
                           properties & PRESET_PROPERTY_INVOLUTIVE ? "对合" : NULL,
                           properties & PRESET_PROPERTY_COMMUTATIVE ? "交换" : NULL,
                           properties & PRESET_PROPERTY_ASSOCIATIVE ? "结合" : NULL,
                           properties & PRESET_PROPERTY_LINEAR ? "线性" : NULL,
                           properties & PRESET_PROPERTY_CONTINUOUS ? "连续" : NULL,
                           properties & PRESET_PROPERTY_DETERMINISTIC ? "确定" : NULL,
                           properties & PRESET_PROPERTY_CONSTRUCTIVE ? "构造" : NULL,
                           properties & PRESET_PROPERTY_REVERSIBLE ? "可逆" : NULL};

    int written = 0;
    bool first = true;

    for (size_t i = 0; i < sizeof(props) / sizeof(props[0]); i++) {
        if (props[i]) {
            if (!first && written < (int) buffer_size - 1) {
                out_buffer[written++] = ',';
            }
            int len = (int) strlen(props[i]);
            if (written + len < (int) buffer_size - 1) {
                memcpy(out_buffer + written, props[i], len);
                written += len;
                first = false;
            }
        }
    }

    out_buffer[written] = '\0';
    return written;
}

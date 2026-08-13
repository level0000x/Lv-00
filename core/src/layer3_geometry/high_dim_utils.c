/*
 * @file high_dim_utils.c
 * @brief High-dim module - utility functions
 * @details Split from high_dim.c
 */

#include "high_dim.h"
#include "high_dim_internal.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "lv/config.h"
#include "lv/lv_json.h"
#include "lv/lv_parse_utils.h"

#include "debug.h"
#include "error_codes.h"
#include "lv_internal.h"
#include "lv/lv_str_utils.h"
#include "lv_utils.h"
#include "stream.h"
#include "stream_context_util.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_xmacro.h"

/* ==================== 工具函数 ==================== */

int high_dim_validate_mapping(int dimension_count, const HighDimAxisMapping *mappings, int mapping_count) {
    if (dimension_count < 4 || dimension_count > HIGH_DIM_MAX_DIMENSIONS) {
        return 0;
    }

    if (!mappings || mapping_count < 1 || mapping_count > dimension_count) {
        return 0;
    }

    /* 检查是否至少有一个维度映射到X和Y */
    int has_x = 0, has_y = 0;
    for (int i = 0; i < mapping_count; i++) {
        if (mappings[i].axis_index < 0 || mappings[i].axis_index >= dimension_count) {
            return 0;
        }
        if (mappings[i].mapping_type == HIGH_DIM_MAP_TO_X)
            has_x = 1;
        if (mappings[i].mapping_type == HIGH_DIM_MAP_TO_Y)
            has_y = 1;
    }

    return (has_x && has_y) ? 1 : 0;
}

/* ================================================================
 * 枚举 -> 名称 映射表（数据表化，替代 switch）
 * ================================================================ */

/** @brief high_dim_mapping_type_to_string 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_high_dim_mapping_type_to_string_entries[] = {
    {"x", HIGH_DIM_MAP_TO_X},
    {"y", HIGH_DIM_MAP_TO_Y},
    {"fold", HIGH_DIM_MAP_FOLD},
    {"discard", HIGH_DIM_MAP_DISCARD},
};

const char *high_dim_mapping_type_to_string(HighDimMappingType mapping_type) {
    return lv_enum_to_str(s_high_dim_mapping_type_to_string_entries, lv_ARRAY_SIZE(s_high_dim_mapping_type_to_string_entries), (int) mapping_type, "unknown");
}

HighDimMappingType high_dim_mapping_type_from_string(const char *str) {
    if (!str)
        return (HighDimMappingType) -1;

    if (lv_str_eq(str, "x"))
        return HIGH_DIM_MAP_TO_X;
    if (lv_str_eq(str, "y"))
        return HIGH_DIM_MAP_TO_Y;
    if (lv_str_eq(str, "fold"))
        return HIGH_DIM_MAP_FOLD;
    if (lv_str_eq(str, "discard"))
        return HIGH_DIM_MAP_DISCARD;

    return (HighDimMappingType) -1;
}

int high_dim_get_folded_dimensions_info(const HighDimProjectionPreset *preset, char *buffer, size_t buffer_size) {
    if (!preset || !buffer || buffer_size == 0) {
        return lv_ERROR_INVALID_PARAM;
    }

    char folded_list[256] = "";
    int folded_count = 0;

    for (int i = 0; i < preset->mapping_count; i++) {
        if (preset->mappings[i].mapping_type == HIGH_DIM_MAP_FOLD ||
            preset->mappings[i].mapping_type == HIGH_DIM_MAP_DISCARD) {
            if (folded_count > 0) {
                lv_strlcat(folded_list, ", ", sizeof(folded_list));
            }
            char dim_str[16];
            lv_snprintf(dim_str, sizeof(dim_str), "%d", preset->mappings[i].axis_index);
            lv_strlcat(folded_list, dim_str, sizeof(folded_list));
            folded_count++;
        }
    }

    if (folded_count > 0) {
        lv_snprintf(buffer, buffer_size, "折叠维度: %s", folded_list);
    } else {
        lv_strlcpy(buffer, "无折叠维度", buffer_size);
    }

    return lv_OK;
}

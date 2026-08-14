/*
 * @file high_dim_preset.c
 * @brief High-dim module - projection preset management
 * @details Split from high_dim.c
 */

#include "lv/high_dim.h"
#include "high_dim_internal.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "lv/config.h"
#include "lv/lv_json.h"
#include "lv/lv_parse_utils.h"

#include "lv/debug.h"
#include "lv/error_codes.h"
#include "lv/lv_internal.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_utils.h"
#include "lv/stream.h"
#include "lv/stream_context_util.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_xmacro.h"

/* ==================== 投影预设管理 ==================== */

/**
 * @brief 添加投影预设
 *
 * 向指定高维块添加一个新的投影预设。
 *
 * @param manager 管理器指针
 * @param block_id 块 ID
 * @param preset  投影预设指针
 * @return lv_OK 成功，错误码表示失败原因
 */
int high_dim_add_projection_preset(HighDimManager *manager, int block_id, const HighDimProjectionPreset *preset) {
    if (!manager || !preset)
        return lv_ERROR_INVALID_PARAM;

    HighDimAbstractBlock *block = high_dim_get_block(manager, block_id);
    if (!block) {
        return lv_ERROR_NOT_FOUND;
    }

    if (block->preset_count >= HIGH_DIM_MAX_PROJECTION_PRESETS) {
        return lv_ERROR_RESOURCE_EXHAUSTED;
    }

    /* 验证预设 */
    if (!high_dim_validate_mapping(preset->dimension_count, preset->mappings, preset->mapping_count)) {
        return lv_ERROR_INVALID_PARAM;
    }

    memcpy(&block->presets[block->preset_count], preset, sizeof(HighDimProjectionPreset));
    block->preset_count++;

    if (high_dim_stream_ctx) {
        stream_emit_info(high_dim_stream_ctx, "投影预设创建", block->preset_count - 1);
    }

    return block->preset_count - 1;
}

/**
 * @brief 移除投影预设
 *
 * 从指定高维块中移除指定索引的投影预设。
 *
 * @param manager      管理器指针
 * @param block_id     块 ID
 * @param preset_index 预设索引
 * @return lv_OK 成功，错误码表示失败原因
 */
int high_dim_remove_projection_preset(HighDimManager *manager, int block_id, int preset_index) {
    if (!manager)
        return lv_ERROR_INVALID_PARAM;

    HighDimAbstractBlock *block = high_dim_get_block(manager, block_id);
    if (!block) {
        return lv_ERROR_NOT_FOUND;
    }

    if (preset_index < 0 || preset_index >= block->preset_count) {
        return lv_ERROR_INVALID_PARAM;
    }

    /* 不能删除最后一个预设 */
    if (block->preset_count <= 1) {
        return lv_ERROR_UNSUPPORTED;
    }

    /* 移动后续预设（lv_shift_left 内部 memmove 整体前移，源和目标区域可能重叠） */
    lv_shift_left(block->presets, sizeof(HighDimProjectionPreset), (size_t) preset_index, (size_t) block->preset_count);

    block->preset_count--;

    /* 调整当前预设索引 */
    if (block->current_preset_index >= preset_index) {
        block->current_preset_index--;
    }
    if (block->current_preset_index < 0) {
        block->current_preset_index = 0;
    }

    return lv_OK;
}

/**
 * @brief 设置当前投影预设
 *
 * @param manager      管理器指针
 * @param block_id     块 ID
 * @param preset_index 预设索引
 * @return lv_OK 成功，错误码表示失败原因
 */
int high_dim_set_current_preset(HighDimManager *manager, int block_id, int preset_index) {
    if (!manager)
        return lv_ERROR_INVALID_PARAM;

    HighDimAbstractBlock *block = high_dim_get_block(manager, block_id);
    if (!block) {
        return lv_ERROR_NOT_FOUND;
    }

    if (preset_index < 0 || preset_index >= block->preset_count) {
        return lv_ERROR_INVALID_PARAM;
    }

    block->current_preset_index = preset_index;

    if (high_dim_stream_ctx) {
        stream_emit_info(high_dim_stream_ctx, "视图切换：投影预设已更新", preset_index);
    }

    return lv_OK;
}

const HighDimProjectionPreset *high_dim_get_current_preset(const HighDimManager *manager, int block_id) {
    if (!manager)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "high_dim_get_current_preset: manager is NULL");

    /* 注意: 此处将 const HighDimManager* 转换为非 const 是因为
     * high_dim_get_block() 缺少 const 版本的API，但该函数不会修改图结构 */
    const HighDimAbstractBlock *block = high_dim_get_block((HighDimManager *) manager, block_id);
    if (!block || block->current_preset_index < 0) {
        return NULL;
    }

    return &block->presets[block->current_preset_index];
}

/**
 * @brief 创建默认投影预设
 *
 * 根据维度数量生成默认的投影映射（前两个维度映射到 x/y）。
 *
 * @param dimension_count 维度数量
 * @param preset          输出参数，接收预设数据
 * @return lv_OK 成功，错误码表示失败原因
 */
int high_dim_create_default_preset(int dimension_count, HighDimProjectionPreset *preset) {
    if (!preset || dimension_count < 4 || dimension_count > HIGH_DIM_MAX_DIMENSIONS) {
        return lv_ERROR_INVALID_PARAM;
    }

    memset(preset, 0, sizeof(HighDimProjectionPreset));

    lv_strlcpy(preset->name, "Default", HIGH_DIM_PROJECTION_NAME_MAX);
    preset->dimension_count = dimension_count;
    preset->mapping_count = dimension_count;
    preset->is_default = true;

    /* 默认映射：前两个维度映射到X和Y，其余折叠 */
    for (int i = 0; i < dimension_count; i++) {
        preset->mappings[i].axis_index = i;
        preset->mappings[i].scale = 1.0;
        preset->mappings[i].offset = 0.0;

        if (i == 0) {
            preset->mappings[i].mapping_type = HIGH_DIM_MAP_TO_X;
        } else if (i == 1) {
            preset->mappings[i].mapping_type = HIGH_DIM_MAP_TO_Y;
        } else {
            preset->mappings[i].mapping_type = HIGH_DIM_MAP_FOLD;
        }
    }

    /* 单位变换矩阵 */
    preset->transform.m[0][0] = 1.0;
    preset->transform.m[0][1] = 0.0;
    preset->transform.m[1][0] = 0.0;
    preset->transform.m[1][1] = 1.0;

    return lv_OK;
}


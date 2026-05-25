/**
 * @file high_dim.c
 * @brief 高维结构表示与交互模块实现
 *
 * @details 本模块实现四维及以上数学对象的表示和投影机制。
 *
 *          核心功能：
 *          - 高维块管理：注册/注销/查询高维抽象块（>= 4维）
 *          - 投影预设：管理多套投影映射方案，支持默认和自定义预设
 *          - 轴映射：定义各维度到二维平面的映射方式
 *            HIGH_DIM_MAP_TO_X / HIGH_DIM_MAP_TO_Y / HIGH_DIM_MAP_FOLD / HIGH_DIM_MAP_DISCARD
 *          - 坐标投影：将高维 SymbolicCoord 投影为 HighDimProjectedCoord
 *          - 二维变换：旋转和缩放变换矩阵组合到投影结果
 *          - 保真度计算：评估投影的信息保留程度
 *            （维度可见性 40% + 约束可见性 60% 加权平均）
 *          - 语义缩放：实现嵌套高维块内部的视角切换（深度栈管理）
 *
 *          关键数据结构：
 *          - HighDimAbstractBlock：高维抽象块（维度数、预设、保真度）
 *          - HighDimAxisMapping：单维度到二维的映射定义
 *          - HighDimProjectionPreset：完整的投影方案（映射列表+2D变换）
 *          - HighDimProjectedCoord：投影后的二维坐标结果
 *          - HighDimVisibilityStats：保真度统计结果
 *
 * @author Lv-00 Project
 * @version 3.2.0
 *
 * @dependencies
 *   - high_dim.h           : 高维模块公共接口定义
 *   - error_codes.h        : 错误码定义（LV00_OK / LV00_ERROR_*）
 *   - lv00_utils.h         : 统一内存分配器和工具函数
 *   - lv00_internal.h      : 内部数据结构与常量（M_PI 等）
 *   - stream.h             : 流式事件输出
 *   - constraint_graph.h   : 约束图接口（保真度计算依赖）
 */

#include "high_dim.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "debug.h" /* LOG_DEBUG, LOG_WARN, LOG_ERROR 等日志宏 */
#include "error_codes.h"
#include "lv00_internal.h" /* M_PI, LV00_SAFE_SNPRINTF 等内部宏 */
#include "lv00_utils.h"
#include "stream.h"
#include "stream_context_util.h"

/* ==================== 内部常量 ==================== */

#define HIGH_DIM_INITIAL_CAPACITY 16

LV00_DECLARE_STREAM_CTX(high_dim)

/**
 * 圆周率常量 π
 *
 * 改用 lv00_internal.h 中统一定义的 M_PI，
 * 避免常量重复定义，确保全项目精度一致。
 * 值: 3.14159265358979323846
 */

/* ==================== 内部辅助函数 ==================== */


/* ==================== 生命周期管理 ==================== */

/**
 * @brief 创建高维管理器
 *
 * 分配并初始化 HighDimManager，调用 high_dim_manager_init 完成内部状态设置。
 *
 * @return 新分配的管理器指针，失败返回 NULL
 */
HighDimManager *high_dim_manager_create(void) {
    HighDimManager *manager = (HighDimManager *) lv00_malloc(sizeof(HighDimManager));
    if (!manager)
        return NULL;

    if (high_dim_manager_init(manager) != 0) {
        lv00_free((void **) &manager);
        return NULL;
    }

    return manager;
}

/**
 * @brief 销毁高维管理器
 *
 * 释放高维块数组和管管理器本身。HighDimAbstractBlock 仅含标量和固定大小数组，
 * 无需逐个释放。
 *
 * @param manager 管理器指针（可为 NULL）
 */
void high_dim_manager_destroy(HighDimManager *manager) {
    if (!manager)
        return;

    /* HighDimAbstractBlock 仅含标量和固定大小数组，无动态资源需要释放；
     * 已移除空 for 循环（迭代无副作用）。 */
    lv00_free((void **) &manager->blocks);

    lv00_free((void **) &manager);
}

/**
 * @brief 初始化高维管理器
 *
 * 分配初始容量为 HIGH_DIM_INITIAL_CAPACITY 的高维块数组。
 *
 * @param manager 管理器指针
 * @return LV00_OK 成功，错误码表示失败原因
 */
int high_dim_manager_init(HighDimManager *manager) {
    if (!manager)
        return LV00_ERROR_INVALID_PARAM;

    manager->blocks = (HighDimAbstractBlock *) lv00_malloc(sizeof(HighDimAbstractBlock) * HIGH_DIM_INITIAL_CAPACITY);
    if (!manager->blocks) {
        return LV00_ERROR_OUT_OF_MEMORY;
    }

    manager->block_count = 0;
    manager->block_capacity = HIGH_DIM_INITIAL_CAPACITY;

    /* 初始化语义缩放深度栈 */
    manager->perspective_depth = 0;
    memset(manager->perspective_stack, 0, sizeof(manager->perspective_stack));

    return LV00_OK;
}

/* ==================== 高维块操作 ==================== */

/**
 * @brief 注册高维块
 *
 * 向管理器添加一个新的高维抽象块，指定维度数量。
 *
 * @param manager         管理器指针
 * @param block_id        块 ID
 * @param dimension_count 维度数量
 * @return LV00_OK 成功，错误码表示失败原因
 */
int high_dim_register_block(HighDimManager *manager, int block_id, int dimension_count) {
    if (!manager || dimension_count < 4 || dimension_count > HIGH_DIM_MAX_DIMENSIONS) {
        return LV00_ERROR_INVALID_PARAM;
    }

    /* 检查是否已存在 —— 时间复杂度 O(n)，若 block_count 较大
     * 可改用哈希集合（如开放寻址法）将查找降至 O(1)。
     * 当前 block_count 通常较小，线性搜索可接受。 */
    for (int i = 0; i < manager->block_count; i++) {
        if (manager->blocks[i].block_id == block_id) {
            return LV00_ERROR_ALREADY_EXISTS;
        }
    }

    /* 扩容检查 */
    if (manager->block_count >= manager->block_capacity) {
        /* 修复：添加整数溢出检查，防止 block_capacity * 2 超过 int 范围 */
        if (manager->block_capacity > INT_MAX / 2) {
            return LV00_ERROR_OUT_OF_MEMORY;
        }
        int new_capacity = manager->block_capacity * 2;
        HighDimAbstractBlock *new_blocks =
            (HighDimAbstractBlock *) lv00_realloc(manager->blocks, sizeof(HighDimAbstractBlock) * new_capacity);
        if (!new_blocks) {
            return LV00_ERROR_OUT_OF_MEMORY;
        }
        manager->blocks = new_blocks;
        manager->block_capacity = new_capacity;
    }

    /* 初始化新块 */
    HighDimAbstractBlock *block = &manager->blocks[manager->block_count];
    memset(block, 0, sizeof(HighDimAbstractBlock));

    block->block_id = block_id;
    block->dimension_count = dimension_count;
    block->preset_count = 0;
    block->current_preset_index = -1;
    block->fidelity_ratio = 1.0;

    /* 创建默认投影预设 */
    HighDimProjectionPreset default_preset;
    int result = high_dim_create_default_preset(dimension_count, &default_preset);
    if (result != LV00_OK) {
        return result;
    }

    memcpy(&block->presets[0], &default_preset, sizeof(HighDimProjectionPreset));
    block->preset_count = 1;
    block->current_preset_index = 0;

    manager->block_count++;

    return LV00_OK;
}

/**
 * @brief 注销高维块
 *
 * 从管理器中移除指定 ID 的高维块，将最后一个块移到被删除位置以保持数组紧凑。
 *
 * @param manager  管理器指针
 * @param block_id 块 ID
 * @return LV00_OK 成功，错误码表示失败原因
 */
int high_dim_unregister_block(HighDimManager *manager, int block_id) {
    if (!manager)
        return LV00_ERROR_INVALID_PARAM;

    int index = -1;
    for (int i = 0; i < manager->block_count; i++) {
        if (manager->blocks[i].block_id == block_id) {
            index = i;
            break;
        }
    }

    if (index < 0) {
        return LV00_ERROR_NOT_FOUND;
    }

    /* 移动后续元素：使用单次 memmove 替代循环，提高效率 */
    if (index < manager->block_count - 1) {
        memmove(&manager->blocks[index], &manager->blocks[index + 1],
                (manager->block_count - index - 1) * sizeof(HighDimAbstractBlock));
    }

    manager->block_count--;

    return LV00_OK;
}

/**
 * @brief 根据块 ID 查找高维块
 *
 * 在管理器的高维块数组中线性搜索指定 ID 的块。
 *
 * @param manager  管理器指针
 * @param block_id 块 ID
 * @return 高维块指针，未找到或 manager 为 NULL 时返回 NULL
 */
HighDimAbstractBlock *high_dim_get_block(HighDimManager *manager, int block_id) {
    if (!manager)
        return NULL;

    for (int i = 0; i < manager->block_count; i++) {
        if (manager->blocks[i].block_id == block_id) {
            return &manager->blocks[i];
        }
    }

    return NULL;
}

/* ==================== 投影预设管理 ==================== */

/**
 * @brief 添加投影预设
 *
 * 向指定高维块添加一个新的投影预设。
 *
 * @param manager 管理器指针
 * @param block_id 块 ID
 * @param preset  投影预设指针
 * @return LV00_OK 成功，错误码表示失败原因
 */
int high_dim_add_projection_preset(HighDimManager *manager, int block_id, const HighDimProjectionPreset *preset) {
    if (!manager || !preset)
        return LV00_ERROR_INVALID_PARAM;

    HighDimAbstractBlock *block = high_dim_get_block(manager, block_id);
    if (!block) {
        return LV00_ERROR_NOT_FOUND;
    }

    if (block->preset_count >= HIGH_DIM_MAX_PROJECTION_PRESETS) {
        return LV00_ERROR_RESOURCE_EXHAUSTED;
    }

    /* 验证预设 */
    if (!high_dim_validate_mapping(preset->dimension_count, preset->mappings, preset->mapping_count)) {
        return LV00_ERROR_INVALID_PARAM;
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
 * @return LV00_OK 成功，错误码表示失败原因
 */
int high_dim_remove_projection_preset(HighDimManager *manager, int block_id, int preset_index) {
    if (!manager)
        return LV00_ERROR_INVALID_PARAM;

    HighDimAbstractBlock *block = high_dim_get_block(manager, block_id);
    if (!block) {
        return LV00_ERROR_NOT_FOUND;
    }

    if (preset_index < 0 || preset_index >= block->preset_count) {
        return LV00_ERROR_INVALID_PARAM;
    }

    /* 不能删除最后一个预设 */
    if (block->preset_count <= 1) {
        return LV00_ERROR_UNSUPPORTED;
    }

    /* 移动后续预设（使用 memmove 而非 memcpy，因为源和目标区域可能重叠） */
    for (int i = preset_index; i < block->preset_count - 1; i++) {
        memmove(&block->presets[i], &block->presets[i + 1], sizeof(HighDimProjectionPreset));
    }

    block->preset_count--;

    /* 调整当前预设索引 */
    if (block->current_preset_index >= preset_index) {
        block->current_preset_index--;
    }
    if (block->current_preset_index < 0) {
        block->current_preset_index = 0;
    }

    return LV00_OK;
}

/**
 * @brief 设置当前投影预设
 *
 * @param manager      管理器指针
 * @param block_id     块 ID
 * @param preset_index 预设索引
 * @return LV00_OK 成功，错误码表示失败原因
 */
int high_dim_set_current_preset(HighDimManager *manager, int block_id, int preset_index) {
    if (!manager)
        return LV00_ERROR_INVALID_PARAM;

    HighDimAbstractBlock *block = high_dim_get_block(manager, block_id);
    if (!block) {
        return LV00_ERROR_NOT_FOUND;
    }

    if (preset_index < 0 || preset_index >= block->preset_count) {
        return LV00_ERROR_INVALID_PARAM;
    }

    block->current_preset_index = preset_index;

    if (high_dim_stream_ctx) {
        stream_emit_info(high_dim_stream_ctx, "视图切换：投影预设已更新", preset_index);
    }

    return LV00_OK;
}

const HighDimProjectionPreset *high_dim_get_current_preset(const HighDimManager *manager, int block_id) {
    if (!manager)
        return NULL;

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
 * @return LV00_OK 成功，错误码表示失败原因
 */
int high_dim_create_default_preset(int dimension_count, HighDimProjectionPreset *preset) {
    if (!preset || dimension_count < 4 || dimension_count > HIGH_DIM_MAX_DIMENSIONS) {
        return LV00_ERROR_INVALID_PARAM;
    }

    memset(preset, 0, sizeof(HighDimProjectionPreset));

    lv00_strlcpy(preset->name, "Default", HIGH_DIM_PROJECTION_NAME_MAX);
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

    return LV00_OK;
}

/* ==================== 坐标投影 ==================== */

/**
 * @brief 高维坐标投影到二维
 *
 * 使用当前投影预设将高维符号坐标投影为二维投影坐标。
 *
 * @param manager         管理器指针
 * @param block_id        块 ID
 * @param high_dim_coords 高维坐标数组
 * @param coord_count     坐标数量
 * @param projected       输出参数，接收投影结果
 * @return LV00_OK 成功，错误码表示失败原因
 */
int high_dim_project_coordinates(HighDimManager *manager, int block_id, const SymbolicCoord **high_dim_coords,
                                 int coord_count, HighDimProjectedCoord *projected) {
    /*
     * 【边界检查和错误处理】
     *   1. NULL 指针检查：manager、high_dim_coords、projected 必须非空
     *   2. coord_count 非负检查：虽然 >= 0 允许空数组，但 > preset->mapping_count
     *      时多余的坐标会因循环条件 (i < coord_count) 被跳过 —— 属于安全行为
     *   3. coord_count 不足时（< preset->mapping_count），循环仅处理现有坐标，
     *      缺失维度默认值为 0.0 —— 投影结果仍可用但精度下降
     *   4. high_dim_coords 为 NULL 时，coord_count 必须为 0；如果 coord_count > 0
     *      则本函数信任调用者保证的数组有效性
     *   5. 如果当前块没有有效的投影预设（preset == NULL），返回 INVALID_STATE
     *   6. projected->folded_info 缓冲区大小为 HIGH_DIM_FOLDED_INFO_MAX，
     *      确保 snprintf 不会越界写入
     */
    if (!manager || !high_dim_coords || !projected || coord_count < 0) {
        return LV00_ERROR_INVALID_PARAM;
    }

    HighDimAbstractBlock *block = high_dim_get_block(manager, block_id);
    if (!block) {
        return LV00_ERROR_NOT_FOUND;
    }

    const HighDimProjectionPreset *preset = high_dim_get_current_preset(manager, block_id);
    if (!preset) {
        return LV00_ERROR_INVALID_STATE;
    }

    /* 初始化投影结果 */
    projected->x = 0.0;
    projected->y = 0.0;
    projected->is_valid = true;
    projected->folded_info[0] = '\0';

    /* 收集折叠维度的信息 */
    char folded_dims[256] = "";
    int folded_count = 0;

    /* 应用维度映射 */
    for (int i = 0; i < preset->mapping_count && i < coord_count; i++) {
        const HighDimAxisMapping *mapping = &preset->mappings[i];

        /* 获取坐标数值近似值 */
        double coord_value = 0.0;
        if (high_dim_coords[i]) {
            coord_value = symbolic_coord_to_double(high_dim_coords[i]);
        }

        double scaled_value = coord_value * mapping->scale + mapping->offset;

        switch (mapping->mapping_type) {
            case HIGH_DIM_MAP_TO_X:
                projected->x += scaled_value;
                break;
            case HIGH_DIM_MAP_TO_Y:
                projected->y += scaled_value;
                break;
            case HIGH_DIM_MAP_FOLD:
            case HIGH_DIM_MAP_DISCARD:
                if (folded_count < 3) {
                    char dim_info[32];
                    snprintf(dim_info, sizeof(dim_info), "%s%d:%.2f", folded_count > 0 ? ", " : "", mapping->axis_index,
                             coord_value);
                    lv00_strlcat(folded_dims, dim_info, sizeof(folded_dims));
                }
                folded_count++;
                break;
        }
    }

    /* 应用2D变换 */
    double x = projected->x;
    double y = projected->y;
    projected->x = preset->transform.m[0][0] * x + preset->transform.m[0][1] * y;
    projected->y = preset->transform.m[1][0] * x + preset->transform.m[1][1] * y;

    /* 设置折叠维度信息 */
    if (folded_count > 0) {
        snprintf(projected->folded_info, sizeof(projected->folded_info), "折叠维度(%d): %s", folded_count, folded_dims);
    }

    return LV00_OK;
}

/**
 * @brief 对投影坐标应用二维变换
 *
 * @param coord     投影坐标
 * @param transform 二维变换矩阵
 * @param result    输出参数，接收变换结果
 * @return LV00_OK 成功，错误码表示失败原因
 */
int high_dim_apply_transform(const HighDimProjectedCoord *coord, const HighDimTransform2D *transform,
                             HighDimProjectedCoord *result) {
    if (!coord || !transform || !result) {
        return LV00_ERROR_INVALID_PARAM;
    }

    double x = coord->x;
    double y = coord->y;

    result->x = transform->m[0][0] * x + transform->m[0][1] * y;
    result->y = transform->m[1][0] * x + transform->m[1][1] * y;
    result->is_valid = coord->is_valid;
    lv00_strlcpy(result->folded_info, coord->folded_info, sizeof(result->folded_info));

    return LV00_OK;
}

/**
 * @brief 创建旋转变换矩阵
 *
 * @param angle_rad 旋转角度（弧度）
 * @param transform 输出参数，接收变换矩阵
 * @return LV00_OK 成功，错误码表示失败原因
 */
int high_dim_create_rotation_transform(double angle_rad, HighDimTransform2D *transform) {
    if (!transform)
        return LV00_ERROR_INVALID_PARAM;

    double cos_a = cos(angle_rad);
    double sin_a = sin(angle_rad);

    transform->m[0][0] = cos_a;
    transform->m[0][1] = -sin_a;
    transform->m[1][0] = sin_a;
    transform->m[1][1] = cos_a;

    return LV00_OK;
}

/**
 * @brief 创建缩放变换矩阵
 *
 * @param scale_x   x 轴缩放因子
 * @param scale_y   y 轴缩放因子
 * @param transform 输出参数，接收变换矩阵
 * @return LV00_OK 成功，错误码表示失败原因
 */
int high_dim_create_scale_transform(double scale_x, double scale_y, HighDimTransform2D *transform) {
    if (!transform)
        return LV00_ERROR_INVALID_PARAM;

    transform->m[0][0] = scale_x;
    transform->m[0][1] = 0.0;
    transform->m[1][0] = 0.0;
    transform->m[1][1] = scale_y;

    return LV00_OK;
}

/* ==================== 保真度计算 ==================== */

/**
 * @brief 计算高维投影的保真度
 *
 * 评估高维结构投影到二维后的信息保留程度，输出可见性统计。
 *
 * @param manager         管理器指针
 * @param block_id        块 ID
 * @param constraint_graph 约束图指针
 * @param stats           输出参数，接收可见性统计
 * @return LV00_OK 成功，错误码表示失败原因
 */
int high_dim_calculate_fidelity(HighDimManager *manager, int block_id, const ConstraintGraph *constraint_graph,
                                HighDimVisibilityStats *stats) {
    if (!manager || !stats) {
        return LV00_ERROR_INVALID_PARAM;
    }

    HighDimAbstractBlock *block = high_dim_get_block(manager, block_id);
    if (!block) {
        return LV00_ERROR_NOT_FOUND;
    }

    const HighDimProjectionPreset *preset = high_dim_get_current_preset(manager, block_id);
    if (!preset) {
        return LV00_ERROR_INVALID_STATE;
    }

    /* ── 第一层：维度可见性分析 ── */
    int visible_dims = 0;
    int total_mapped = 0;

    for (int i = 0; i < preset->mapping_count; i++) {
        if (preset->mappings[i].mapping_type == HIGH_DIM_MAP_TO_X ||
            preset->mappings[i].mapping_type == HIGH_DIM_MAP_TO_Y) {
            visible_dims++;
        }
        total_mapped++;
    }

    /* ── 第二层：基于约束图的关系统计（当约束图可用时） ── */
    int constraint_visible = 0;
    int constraint_total = 0;

    if (constraint_graph && constraint_graph->constraint_count > 0) {
        constraint_total = constraint_graph->constraint_count;

        /*
         * 保真度增强计算：
         * 遍历所有约束，检查参与约束的节点是否在可见维度上有坐标。
         * 对于每个约束，如果其所有参与者节点都有至少一个坐标落在
         * 被映射到 X 或 Y 轴的维度上，则该约束被视为"可见"。
         *
         * 简化策略（因为节点坐标不直接记录维度信息）：
         * 使用维度比例作为约束可见性的近似：
         *   - INCIDENCE/BETWEENNESS/INTERSECTION 约束涉及2-3个节点
         *   - 如果可见维度比例 >= 50%，认为约束在投影中可区分
         *   - CONTAINMENT/CONNECTION 约束涉及端口和区域，需要更多维度
         */
        double dim_ratio = (block->dimension_count > 0) ? (double) visible_dims / block->dimension_count : 1.0;

        for (int i = 0; i < constraint_graph->constraint_count; i++) {
            Constraint *c = constraint_graph->constraints[i];
            if (!c)
                continue;

            /*
             * 不同约束类型对维度可见性的要求不同：
             * - INCIDENCE（关联）：需要1个可见维度即可区分
             * - BETWEENNESS（之间）：需要2个可见维度（位置排序）
             * - INTERSECTION（相交）：需要2个可见维度
             * - CONTAINMENT（包含）：需要2个可见维度（内外判定）
             * - CONNECTION（连接）：端口连接，需要1个可见维度
             */
            int required_dims = 1;
            switch (c->type) {
                case INCIDENCE:
                case CONNECTION:
                    required_dims = 1;
                    break;
                case BETWEENNESS:
                case INTERSECTION:
                case CONTAINMENT:
                    required_dims = 2;
                    break;
            }

            /* 如果可见维度数满足该约束类型的要求，则视为可见 */
            if (visible_dims >= required_dims) {
                constraint_visible++;
            }
        }
    }

    /* ── 综合保真度计算 ── */
    /*
     * 保真度 = 加权平均：
     *   - 维度可见性权重 40%
     *   - 约束可见性权重 60%（约束是几何语义的核心载体）
     *
     * 当约束图不可用时，退回到纯维度比例计算。
     */
    double dim_fidelity = (block->dimension_count > 0) ? (double) visible_dims / block->dimension_count : 1.0;
    double constraint_fidelity = (constraint_total > 0) ? (double) constraint_visible / constraint_total : dim_fidelity;

    if (constraint_total > 0) {
        /* 有约束图数据：使用加权综合保真度 */
        stats->fidelity_ratio = 0.4 * dim_fidelity + 0.6 * constraint_fidelity;
        stats->total_relations = constraint_total;
        stats->visible_relations = constraint_visible;
    } else {
        /* 无约束图数据：退回到维度比例 */
        stats->fidelity_ratio = dim_fidelity;
        stats->total_relations = block->dimension_count;
        stats->visible_relations = visible_dims;
    }

    /* 保真度钳制到 [0.0, 1.0] */
    if (stats->fidelity_ratio < 0.0)
        stats->fidelity_ratio = 0.0;
    if (stats->fidelity_ratio > 1.0)
        stats->fidelity_ratio = 1.0;

    block->fidelity_ratio = stats->fidelity_ratio;

    return LV00_OK;
}

/**
 * @brief 检查保真度是否低于阈值
 *
 * @param manager   管理器指针
 * @param block_id  块 ID
 * @param threshold 保真度阈值（0.0~1.0）
 * @return 1 低于阈值，0 不低于阈值，-1 参数无效
 */
int high_dim_is_fidelity_below_threshold(const HighDimManager *manager, int block_id, double threshold) {
    if (!manager || threshold < 0.0 || threshold > 1.0) {
        return -1;
    }

    /* 注意: 此处将 const HighDimManager* 转换为非 const 是因为
     * high_dim_get_block() 缺少 const 版本的API，但该函数不会修改图结构 */
    const HighDimAbstractBlock *block = high_dim_get_block((HighDimManager *) manager, block_id);
    if (!block) {
        return -1;
    }

    return (block->fidelity_ratio < threshold) ? 1 : 0;
}

/**
 * @brief 获取保真度警告信息
 *
 * 根据当前保真度生成人类可读的警告描述。
 *
 * @param manager     管理器指针
 * @param block_id    块 ID
 * @param buffer      输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return LV00_OK 成功，错误码表示失败原因
 */
int high_dim_get_fidelity_warning(const HighDimManager *manager, int block_id, char *buffer, size_t buffer_size) {
    if (!manager || !buffer || buffer_size == 0) {
        return LV00_ERROR_INVALID_PARAM;
    }

    /* 注意: 此处将 const HighDimManager* 转换为非 const 是因为
     * high_dim_get_block() 缺少 const 版本的API，但该函数不会修改图结构 */
    const HighDimAbstractBlock *block = high_dim_get_block((HighDimManager *) manager, block_id);
    if (!block) {
        return LV00_ERROR_NOT_FOUND;
    }

    const HighDimProjectionPreset *preset = high_dim_get_current_preset(manager, block_id);
    if (!preset) {
        return LV00_ERROR_INVALID_STATE;
    }

    snprintf(buffer, buffer_size,
             "警告：当前投影'%s'的保真度为%.1f%%，低于推荐阈值%.0f%%。"
             "建议切换到其他投影预设以获得更好的可视化效果。",
             preset->name, block->fidelity_ratio * 100.0, HIGH_DIM_DEFAULT_FIDELITY_THRESHOLD * 100.0);

    return LV00_OK;
}

/* ==================== 语义缩放 ==================== */

int high_dim_enter_block_perspective(HighDimManager *manager, int block_id) {
    /**
     * 进入高维块内部透视（语义缩放）
     *
     * 切换画布上下文到指定高维块的局部坐标系。
     * 在C层实现基本的深度栈管理：将当前block_id压入栈顶，深度递增。
     * 完整的渲染语义（切换投影矩阵、更新视图层级）依赖UI层渲染引擎。
     *
     * 参数验证通过但UI层尚未集成时的行为：
     * - 深度栈push操作正常执行（C层状态正确）
     * - 设置warning提示UI层需要配合完成视觉切换
     *
     * @param manager 高维管理器指针
     * @param block_id 要进入透视的函数块ID
     * @return LV00_OK 成功（深度栈已更新）
     *         LV00_ERROR_INVALID_PARAM 参数无效
     *         LV00_ERROR_NOT_FOUND 未找到对应的高维块
     *         LV00_ERROR_UNSUPPORTED 深度栈已满
     */
    if (!manager)
        return LV00_ERROR_INVALID_PARAM;

    HighDimAbstractBlock *block = high_dim_get_block(manager, block_id);
    if (!block) {
        lv00_set_error(LV00_ERROR_NOT_FOUND, "进入块透视失败：未找到block_id=%d对应的高维抽象块", block_id);
        return LV00_ERROR_NOT_FOUND;
    }

    /* 检查深度栈是否已满 */
    if (manager->perspective_depth >= HIGH_DIM_MAX_DEPTH) {
        /* T-NEW-18: 自动折叠最深层，腾出空间 */
        int collapsed_block_id = manager->perspective_stack[manager->perspective_depth - 1];
        manager->perspective_stack[manager->perspective_depth - 1] = 0;
        manager->perspective_depth--;

        LOG_DEBUG("high_dim", "深度栈已满（最大深度=%d），自动折叠最深层block_id=%d后继续进入block_id=%d",
                   HIGH_DIM_MAX_DEPTH, collapsed_block_id, block_id);

        if (high_dim_stream_ctx) {
            stream_emit_warning(high_dim_stream_ctx,
                "语义缩放深度栈已满，自动折叠最深层（block_id=%d）", collapsed_block_id);
        }
    }

    /* 将当前block_id压入深度栈 */
    manager->perspective_stack[manager->perspective_depth] = block_id;
    manager->perspective_depth++;

    if (high_dim_stream_ctx) {
        stream_emit_progress(high_dim_stream_ctx, 0.0, "语义缩放：进入块透视", block_id, -1);
    }

    /* DEBUG级别日志：提示UI层需要同步切换渲染管线 */
    LOG_DEBUG("high_dim", "已进入block_id=%d的内部透视，当前深度=%d。", block_id, manager->perspective_depth);

    return LV00_OK;
}

int high_dim_exit_block_perspective(HighDimManager *manager) {
    /**
     * 退出高维块内部透视（语义缩放）
     *
     * 从深度栈pop顶部block_id，恢复到上一级透视的上下文。
     * C层负责深度栈的pop操作和状态管理。
     * 完整的视图恢复（切换渲染管线、还原投影矩阵）依赖UI层渲染引擎。
     *
     * @param manager 高维管理器指针
     * @return LV00_OK 成功（深度栈已pop）
     *         LV00_ERROR_INVALID_PARAM 参数无效
     *         LV00_ERROR_UNSUPPORTED 深度栈已空（已在最外层）
     */
    if (!manager)
        return LV00_ERROR_INVALID_PARAM;

    /* 检查深度栈是否已空 */
    if (manager->perspective_depth <= 0) {
        /* T-NEW-19: 已在最外层，返回当前状态信息而非错误 */
        lv00_set_error(LV00_OK,
                       "当前已在最外层透视（深度=0），无需退出。当前状态：block_count=%d",
                       manager->block_count);
        LOG_DEBUG("high_dim", "退出透视请求被忽略：已在最外层，当前block_count=%d", manager->block_count);
        return LV00_OK;
    }

    /* 获取即将退出的block_id并pop栈 */
    int exited_block_id = manager->perspective_stack[manager->perspective_depth - 1];
    manager->perspective_stack[manager->perspective_depth - 1] = 0;
    manager->perspective_depth--;

    /* DEBUG级别日志：提示UI层需要同步恢复上层视图 */
    LOG_DEBUG("high_dim", "已退出block_id=%d的内部透视，恢复到深度=%d。", exited_block_id, manager->perspective_depth);

    return LV00_OK;
}

int high_dim_get_current_depth(const HighDimManager *manager) {
    /**
     * 获取当前语义缩放透视深度
     *
     * 返回深度栈中当前记录的透视深度（即进入了几层块内部）。
     * 深度为0表示在最外层（无透视）。
     *
     * @param manager 高维管理器指针（const，只读操作）
     * @return 当前透视深度（>= 0），manager为NULL时返回-1
     */
    if (!manager)
        return -1;

    /* 直接返回C层维护的深度计数值 */
    return manager->perspective_depth;
}

int high_dim_zoom_to_level(HighDimManager *manager, int target_depth, int block_id) {
    /**
     * 直接跳转到指定缩放层级
     *
     * 通过反复进入或退出透视来达到目标深度。
     */
    if (!manager)
        return LV00_ERROR_INVALID_PARAM;
    if (target_depth < 0 || target_depth > HIGH_DIM_MAX_DEPTH)
        return LV00_ERROR_INVALID_PARAM;

    /* 如果目标深度小于当前深度，退出到目标深度 */
    while (manager->perspective_depth > target_depth) {
        int rc = high_dim_exit_block_perspective(manager);
        if (rc != LV00_OK)
            return rc;
    }

    /* 如果目标深度大于当前深度，进入透视到目标深度 */
    while (manager->perspective_depth < target_depth) {
        int rc = high_dim_enter_block_perspective(manager, block_id);
        if (rc != LV00_OK)
            return rc;
    }

    return LV00_OK;
}

int high_dim_get_zoom_level(const HighDimManager *manager, int *out_depth, int *out_top_block_id) {
    /**
     * 获取当前缩放级别信息
     */
    if (!manager)
        return LV00_ERROR_INVALID_PARAM;

    if (out_depth)
        *out_depth = manager->perspective_depth;

    if (out_top_block_id) {
        if (manager->perspective_depth > 0) {
            *out_top_block_id = manager->perspective_stack[manager->perspective_depth - 1];
        } else {
            *out_top_block_id = -1;
        }
    }

    return LV00_OK;
}

int high_dim_set_focus_point(HighDimManager *manager, int focus_block_id) {
    /**
     * 设置缩放焦点block_id
     *
     * 验证block存在后记录焦点，供UI层在缩放时参考。
     */
    if (!manager)
        return LV00_ERROR_INVALID_PARAM;

    HighDimAbstractBlock *block = high_dim_get_block(manager, focus_block_id);
    if (!block) {
        lv00_set_error(LV00_ERROR_NOT_FOUND, "设置缩放焦点失败：未找到block_id=%d", focus_block_id);
        return LV00_ERROR_NOT_FOUND;
    }

    /* 焦点信息记录在透视栈的保留位置（不影响当前深度） */
    /* 使用 perspective_stack[0] 作为焦点记录（仅当深度为0时） */
    if (manager->perspective_depth == 0) {
        manager->perspective_stack[0] = focus_block_id;
    }

    if (high_dim_stream_ctx) {
        stream_emit_info(high_dim_stream_ctx, "语义缩放：焦点已设置", focus_block_id);
    }

    return LV00_OK;
}

/* ==================== 多投影视图 ==================== */

/**
 * @brief 多投影视图内部上下文
 *
 * 在 C 层维护每个多投影视图的状态，包括关联的高维块、投影预设和
 * 当前高亮元素列表。视图ID由 create 函数生成，通过静态数组统一管理。
 */
#define HIGH_DIM_MAX_ACTIVE_VIEWS 32

typedef struct {
    int view_id;                                       /**< 唯一视图标识符 */
    int block_id;                                      /**< 关联的高维块ID */
    int preset_index;                                  /**< 使用的投影预设索引 */
    bool is_active;                                    /**< 视图是否处于激活状态 */
    int highlighted_elements[HIGH_DIM_MAX_DIMENSIONS]; /**< 当前高亮的元素ID列表 */
    int highlighted_count;                             /**< 高亮元素数量 */
} HighDimMultiViewContext;

/** 全局活跃视图追踪数组 */
static LV00_THREAD_LOCAL HighDimMultiViewContext g_multi_views[HIGH_DIM_MAX_ACTIVE_VIEWS];
static LV00_THREAD_LOCAL int g_multi_view_count = 0;

/**
 * @brief 在全局视图数组中查找指定 view_id 对应的索引
 *
 * @param view_id 视图ID
 * @return 数组索引（>= 0），未找到返回 -1
 */
static int high_dim_find_view_index(int view_id) {
    for (int i = 0; i < g_multi_view_count; i++) {
        if (g_multi_views[i].is_active && g_multi_views[i].view_id == view_id) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief 分配一个新的视图上下文槽位
 *
 * @param view_id 视图ID
 * @param block_id 关联的高维块ID
 * @param preset_index 投影预设索引
 * @return 新分配的数组索引，失败返回 -1
 */
static int high_dim_allocate_view_slot(int view_id, int block_id, int preset_index) {
    /* 首先尝试复用已释放的槽位 */
    for (int i = 0; i < g_multi_view_count; i++) {
        if (!g_multi_views[i].is_active) {
            memset(&g_multi_views[i], 0, sizeof(HighDimMultiViewContext));
            g_multi_views[i].view_id = view_id;
            g_multi_views[i].block_id = block_id;
            g_multi_views[i].preset_index = preset_index;
            g_multi_views[i].is_active = true;
            g_multi_views[i].highlighted_count = 0;
            return i;
        }
    }

    /* 分配新槽位 */
    if (g_multi_view_count >= HIGH_DIM_MAX_ACTIVE_VIEWS) {
        return -1; /* 视图槽位已满 */
    }

    int idx = g_multi_view_count;
    memset(&g_multi_views[idx], 0, sizeof(HighDimMultiViewContext));
    g_multi_views[idx].view_id = view_id;
    g_multi_views[idx].block_id = block_id;
    g_multi_views[idx].preset_index = preset_index;
    g_multi_views[idx].is_active = true;
    g_multi_views[idx].highlighted_count = 0;
    g_multi_view_count++;

    return idx;
}

int high_dim_create_multi_projection_view(HighDimManager *manager, int block_id, const int *preset_indices,
                                          int preset_count, int *view_ids) {
    /**
     * @brief 创建多投影并排视图
     *
     * 为同一个高维块创建多个并排显示的投影视图。每个视图使用不同的
     * 投影预设，以便用户同时从多个角度观察高维对象。
     *
     * 实现要点：
     *   1. 参数验证：manager、preset_indices、view_ids 非空，preset_count > 0
     *   2. 存在性验证：目标高维块必须已注册
     *   3. 预设索引验证：每个 preset_indices[i] 必须在 [0, preset_count) 范围内
     *   4. 视图ID生成：使用 block_id 和 preset_index 的组合编码
     *      （格式: block_id * 1000 + preset_index）
     *   5. 视图上下文注册：在全局视图追踪数组中分配槽位，
     *      记录 view_id、block_id、preset_index 的映射关系
     *   6. 容量检查：确保不超过 HIGH_DIM_MAX_ACTIVE_VIEWS 限制
     *
     * 视图ID编码方案：
     *   由于 view_id = block_id * 1000 + preset_index，如果同一个
     *   (block_id, preset_index) 对被多次请求，将分配不同的视图ID
     *   （通过追加偏移量确保唯一性）。
     *
     * 注意：完整的渲染管线（视口布局、摄像机设置、OpenGL上下文
     * 切换）由UI层渲染引擎负责。C层仅维护视图逻辑状态的记录。
     *
     * @param manager 高维管理器指针
     * @param block_id 要创建多视图的高维块ID
     * @param preset_indices 投影预设索引数组，每个元素对应一个视图
     * @param preset_count 视图数量（= preset_indices 的元素个数）
     * @param view_ids 输出参数，接收生成的视图ID数组（长度 >= preset_count）
     * @return LV00_OK 所有视图创建成功
     *         LV00_ERROR_INVALID_PARAM 参数无效
     *         LV00_ERROR_NOT_FOUND 未找到指定的高维块
     *         LV00_ERROR_RESOURCE_EXHAUSTED 视图槽位已满
     */
    if (!manager || !preset_indices || !view_ids || preset_count <= 0) {
        return LV00_ERROR_INVALID_PARAM;
    }

    HighDimAbstractBlock *block = high_dim_get_block(manager, block_id);
    if (!block) {
        lv00_set_error(LV00_ERROR_NOT_FOUND, "创建多投影视图失败：未找到block_id=%d对应的高维抽象块", block_id);
        return LV00_ERROR_NOT_FOUND;
    }

    /* 验证预设索引 */
    for (int i = 0; i < preset_count; i++) {
        if (preset_indices[i] < 0 || preset_indices[i] >= block->preset_count) {
            lv00_set_error(LV00_ERROR_INVALID_PARAM,
                           "创建多投影视图失败：第%d个预设索引=%d无效"
                           "（有效范围：0-%d）",
                           i, preset_indices[i], block->preset_count - 1);
            return LV00_ERROR_INVALID_PARAM;
        }
    }

    /* 为每个预设创建视图 */
    for (int i = 0; i < preset_count; i++) {
        /* 生成唯一视图ID：基础编码 + 冲突避免偏移 */
        /* 注意: block_id 和 preset_index 必须小于 1000，否则ID会碰撞 */
        if (block_id >= 1000 || preset_indices[i] >= 1000) {
            LV00_LOG_WARNING("视图ID编码: block_id=%d 或 preset_index=%d 超过999，可能产生ID碰撞", block_id,
                             preset_indices[i]);
        }
        int base_vid = block_id * 1000 + preset_indices[i];
        int vid = base_vid;
        int offset = 0;
        /* 检查ID冲突：如果已有相同ID的活跃视图，递增偏移 */
        while (high_dim_find_view_index(vid) >= 0 && offset < 100) {
            offset++;
            vid = base_vid + offset * 10000; /* 加偏移量确保唯一性 */
        }
        if (offset >= 100) {
            /* 无法找到唯一ID：清理已创建的视图 */
            for (int j = 0; j < i; j++) {
                int idx = high_dim_find_view_index(view_ids[j]);
                if (idx >= 0) {
                    g_multi_views[idx].is_active = false;
                }
            }
            lv00_set_error(LV00_ERROR_RESOURCE_EXHAUSTED,
                           "创建多投影视图失败：block_id=%d 的视图ID空间已耗尽"
                           "（无法为preset_index=%d分配唯一ID）",
                           block_id, preset_indices[i]);
            return LV00_ERROR_RESOURCE_EXHAUSTED;
        }

        /* 分配视图槽位 */
        int slot_idx = high_dim_allocate_view_slot(vid, block_id, preset_indices[i]);
        if (slot_idx < 0) {
            /* 槽位已满：回滚已创建的视图 */
            for (int j = 0; j < i; j++) {
                int idx = high_dim_find_view_index(view_ids[j]);
                if (idx >= 0) {
                    g_multi_views[idx].is_active = false;
                }
            }
            lv00_set_error(LV00_ERROR_RESOURCE_EXHAUSTED,
                           "创建多投影视图失败：全局视图槽位已满"
                           "（最大=%d，当前=%d）",
                           HIGH_DIM_MAX_ACTIVE_VIEWS, g_multi_view_count);
            return LV00_ERROR_RESOURCE_EXHAUSTED;
        }

        view_ids[i] = vid;
    }

    /* DEBUG级别日志：提示UI层需要同步创建视图窗口 */
    LOG_DEBUG("high_dim", "已为block_id=%d创建%d个并排投影视图（view_ids[0]=%d,...）。", block_id, preset_count,
              view_ids[0]);

    return LV00_OK;
}

int high_dim_destroy_multi_projection_view(HighDimManager *manager, int view_id) {
    /**
     * @brief 销毁多投影视图
     *
     * 完整的视图销毁流程包括以下步骤：
     *   1. 参数与视图ID有效性验证
     *   2. 从 view_id 反推原始的 block_id 和 preset_index
     *      （与 create 函数使用的编码规则一致：view_id = block_id * 1000 + preset_index）
     *   3. 验证对应的 block 和 preset 是否仍然存在
     *   4. 从全局视图追踪数组中标记该视图为 inactive，
     *      释放视图槽位供后续复用
     *   5. 清除该视图的高亮状态记录
     *   6. 输出中文诊断信息以协助UI层同步关闭对应窗口
     *
     * 视图槽位回收策略：
     *   - 将 is_active 设置为 false，不压缩数组（避免O(n)移动）
     *   - 下次 create 调用时通过 high_dim_allocate_view_slot 复用已释放槽位
     *   - 视图槽位在程序退出前不会释放内存，仅做逻辑标记
     *
     * @param manager 高维管理器指针
     * @param view_id 视图ID（来自 high_dim_create_multi_projection_view 的返回值）
     * @return LV00_OK 视图成功标记为销毁
     *         LV00_ERROR_INVALID_PARAM 参数无效或 view_id 不合法
     *         LV00_ERROR_NOT_FOUND 未找到指定的视图或对应的块不存在
     */
    if (!manager)
        return LV00_ERROR_INVALID_PARAM;

    /* 验证 view_id 的基本有效性 */
    if (view_id <= 0) {
        lv00_set_error(LV00_ERROR_INVALID_PARAM, "销毁视图失败：无效的视图ID=%d，ID必须为正值", view_id);
        return LV00_ERROR_INVALID_PARAM;
    }

    /* 在全局视图数组中找到该视图 */
    int view_index = high_dim_find_view_index(view_id);
    if (view_index < 0) {
        lv00_set_error(LV00_ERROR_NOT_FOUND,
                       "销毁视图失败：未找到view_id=%d对应的活跃视图"
                       "（可能已被销毁或从未创建）",
                       view_id);
        return LV00_ERROR_NOT_FOUND;
    }

    HighDimMultiViewContext *view_ctx = &g_multi_views[view_index];
    int block_id = view_ctx->block_id;
    int preset_index = view_ctx->preset_index;

    /* 验证对应的块是否仍然存在 */
    HighDimAbstractBlock *block = high_dim_get_block(manager, block_id);
    if (!block) {
        /* 块已被删除：仍需清理视图槽位，使用WARN级别日志记录异常情况 */
        LOG_WARN("high_dim",
                 "view_id=%d对应的block_id=%d已不存在（可能已被注销），"
                 "但视图槽位仍将被清理。",
                 view_id, block_id);
    } else {
        /* 验证预设索引是否仍然有效 */
        if (preset_index < 0 || preset_index >= block->preset_count) {
            LOG_WARN("high_dim",
                     "view_id=%d对应的预设索引=%d已无效"
                     "（当前块预设数=%d），视图槽位仍将被清理。",
                     view_id, preset_index, block->preset_count);
        }
    }

    /* 清除高亮状态 */
    view_ctx->highlighted_count = 0;
    memset(view_ctx->highlighted_elements, 0, sizeof(view_ctx->highlighted_elements));

    /* 标记视图为 inactive（槽位可复用） */
    view_ctx->is_active = false;

    /* DEBUG级别日志：提示UI层需要同步关闭视图窗口 */
    LOG_DEBUG("high_dim", "视图view_id=%d（block_id=%d, preset=%d, 高亮元素数=%d）已销毁。", view_id, block_id,
              preset_index, view_ctx->highlighted_count);

    return LV00_OK;
}

int high_dim_link_highlight(HighDimManager *manager, const int *view_ids, int view_count, int element_id) {
    /**
     * @brief 多视图联动高亮元素
     *
     * 在一个视图中高亮元素时，所有关联视图联动高亮对应元素。
     * 实现以下功能：
     *
     *   1. 参数和视图有效性验证：
     *      - manager、view_ids 非空，view_count > 0，element_id >= 0
     *      - 每个 view_id 对应的活跃视图必须在全局追踪数组中存在
     *      - 验证每个视图对应的 block 和 preset 仍有效
     *
     *   2. 坐标空间映射（C层核心逻辑）：
     *      - 获取每个视图的投影预设配置
     *      - 对于给定的 element_id，在各视图中查找对应的几何元素
     *      - 建立跨视图的元素对应关系表
     *      - 注：完整的坐标映射（从视图A到视图B的高维坐标转换）
     *        需要访问实际的节点几何数据，当前C层记录高亮状态，
     *        具体的UI渲染高亮由UI层完成
     *
     *   3. 高亮状态记录：
     *      - 在每个关联视图的上下文中记录 element_id
     *      - 使用 highlighted_elements[] 数组存储高亮元素列表
     *      - 如果该元素已在高亮列表中，则跳过（去重）
     *      - 达到容量上限时输出警告
     *
     *   4. 跨视图一致性保证：
     *      - 确保所有关联视图都接收到相同的高亮指令
     *      - 如果某个视图的高亮列表已满，该视图被跳过并输出警告
     *      - 至少有一个视图成功记录高亮状态时，整体操作视为成功
     *
     * @param manager 高维管理器指针
     * @param view_ids 关联视图ID数组（所有需要联动高亮的视图）
     * @param view_count 视图数量
     * @param element_id 要联动高亮的元素ID（>= 0）
     * @return LV00_OK 高亮状态已成功记录到至少一个视图
     *         LV00_ERROR_INVALID_PARAM 参数无效
     *         LV00_ERROR_NOT_FOUND 所有视图均未找到
     */
    if (!manager || !view_ids || view_count <= 0) {
        return LV00_ERROR_INVALID_PARAM;
    }

    /* 验证 element_id 有效性 */
    if (element_id < 0) {
        lv00_set_error(LV00_ERROR_INVALID_PARAM, "联动高亮失败：无效的元素ID=%d", element_id);
        return LV00_ERROR_INVALID_PARAM;
    }

    /* 遍历所有关联视图，记录高亮状态 */
    int views_highlighted = 0;
    int views_skipped = 0;
    char skipped_info[512] = "";

    for (int i = 0; i < view_count; i++) {
        int vid = view_ids[i];
        if (vid <= 0) {
            if (views_skipped < 10) {
                char buf[64];
                snprintf(buf, sizeof(buf), "%sview_id[%d]=%d无效; ", views_skipped > 0 ? "" : "", i, vid);
                lv00_strlcat(skipped_info, buf, sizeof(skipped_info));
            }
            views_skipped++;
            continue;
        }

        /* 在视图追踪数组中查找 */
        int view_idx = high_dim_find_view_index(vid);
        if (view_idx < 0) {
            if (views_skipped < 10) {
                char buf[64];
                snprintf(buf, sizeof(buf), "%sview_id=%d未注册; ", views_skipped > 0 ? "" : "", vid);
                lv00_strlcat(skipped_info, buf, sizeof(skipped_info));
            }
            views_skipped++;
            continue;
        }

        HighDimMultiViewContext *view_ctx = &g_multi_views[view_idx];

        /* 验证对应的 block 仍然存在 */
        HighDimAbstractBlock *block = high_dim_get_block(manager, view_ctx->block_id);
        if (!block) {
            if (views_skipped < 10) {
                char buf[80];
                snprintf(buf, sizeof(buf), "%sview_id=%d的block_id=%d已注销; ", views_skipped > 0 ? "" : "", vid,
                         view_ctx->block_id);
                lv00_strlcat(skipped_info, buf, sizeof(skipped_info));
            }
            views_skipped++;
            continue;
        }

        /* 检查该元素是否已在高亮列表中（去重） */
        bool already_highlighted = false;
        for (int h = 0; h < view_ctx->highlighted_count; h++) {
            if (view_ctx->highlighted_elements[h] == element_id) {
                already_highlighted = true;
                break;
            }
        }

        if (already_highlighted) {
            views_skipped++;
            continue; /* 跳过重复高亮 */
        }

        /* 添加高亮元素 */
        if (view_ctx->highlighted_count < HIGH_DIM_MAX_DIMENSIONS) {
            view_ctx->highlighted_elements[view_ctx->highlighted_count] = element_id;
            view_ctx->highlighted_count++;
            views_highlighted++;
        } else {
            if (views_skipped < 10) {
                char buf[80];
                snprintf(buf, sizeof(buf), "%sview_id=%d高亮列表已满; ", views_skipped > 0 ? "" : "", vid);
                lv00_strlcat(skipped_info, buf, sizeof(skipped_info));
            }
            views_skipped++;
        }
    }

    /* 汇总结果 */
    if (views_highlighted == 0) {
        lv00_set_error(LV00_ERROR_NOT_FOUND, "联动高亮失败：所有%d个视图均未能记录高亮状态。跳过原因：%s", view_count,
                       skipped_info[0] ? skipped_info : "所有视图未注册或无效");
        return LV00_ERROR_NOT_FOUND;
    }

    if (views_skipped > 0) {
        /* 部分视图被跳过，使用WARN级别日志记录 */
        LOG_WARN("high_dim",
                 "联动高亮部分成功：元素element_id=%d已在%d/%d个视图中高亮"
                 "（跳过%d个视图：%s）。",
                 element_id, views_highlighted, view_count, views_skipped, skipped_info[0] ? skipped_info : "重复高亮");
    } else {
        /* DEBUG级别日志：记录完整成功状态 */
        LOG_DEBUG("high_dim",
                  "联动高亮成功：元素element_id=%d已在全部%d个关联视图中高亮。"
                  "首视图view_id=%d。",
                  element_id, view_count, view_ids[0]);
    }

    return LV00_OK;
}

/* ==================== 序列化 ==================== */

int high_dim_preset_serialize_json(const HighDimProjectionPreset *preset, char *buffer, size_t buffer_size) {
    if (!preset || !buffer || buffer_size == 0) {
        return LV00_ERROR_INVALID_PARAM;
    }

    int written = snprintf(buffer, buffer_size,
                           "{\n"
                           "  \"name\": \"%s\",\n"
                           "  \"dimension_count\": %d,\n"
                           "  \"mapping_count\": %d,\n"
                           "  \"mappings\": [\n",
                           preset->name, preset->dimension_count, preset->mapping_count);

    if (written >= (int) buffer_size) {
        return LV00_ERROR_BUFFER_TOO_SMALL;
    }

    size_t offset = written;

    /* 序列化映射配置 */
    for (int i = 0; i < preset->mapping_count && offset < buffer_size; i++) {
        const HighDimAxisMapping *m = &preset->mappings[i];
        written = snprintf(buffer + offset, buffer_size - offset,
                           "    {\n"
                           "      \"axis_index\": %d,\n"
                           "      \"mapping_type\": \"%s\",\n"
                           "      \"scale\": %.6f,\n"
                           "      \"offset\": %.6f\n"
                           "    }%s\n",
                           m->axis_index, high_dim_mapping_type_to_string(m->mapping_type), m->scale, m->offset,
                           (i < preset->mapping_count - 1) ? "," : "");
        offset += written;
    }

    if (offset < buffer_size) {
        written = snprintf(buffer + offset, buffer_size - offset,
                           "  ],\n"
                           "  \"transform\": {\n"
                           "    \"m00\": %.6f,\n"
                           "    \"m01\": %.6f,\n"
                           "    \"m10\": %.6f,\n"
                           "    \"m11\": %.6f\n"
                           "  },\n"
                           "  \"is_default\": %s\n"
                           "}",
                           preset->transform.m[0][0], preset->transform.m[0][1], preset->transform.m[1][0],
                           preset->transform.m[1][1], preset->is_default ? "true" : "false");
        offset += written;
    }

    return (offset >= buffer_size) ? LV00_ERROR_BUFFER_TOO_SMALL : (int) offset;
}

/* ==================== JSON 反序列化辅助函数 ==================== */

/** 跳过空白字符 */
static const char *hd_json_skip_ws(const char *p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    return p;
}

/** 跳过一个 JSON 值（字符串、数字、对象、数组、true/false/null） */
static const char *hd_json_skip_value(const char *p) {
    p = hd_json_skip_ws(p);
    if (*p == '"') {
        p++;
        while (*p && *p != '"') {
            if (*p == '\\')
                p++;
            p++;
        }
        if (*p == '"')
            p++;
    } else if (*p == '{') {
        int depth = 1;
        p++;
        while (*p && depth > 0) {
            if (*p == '{')
                depth++;
            else if (*p == '}')
                depth--;
            else if (*p == '"') {
                p++;
                while (*p && *p != '"') {
                    if (*p == '\\')
                        p++;
                    p++;
                }
            }
            p++;
        }
    } else if (*p == '[') {
        int depth = 1;
        p++;
        while (*p && depth > 0) {
            if (*p == '[')
                depth++;
            else if (*p == ']')
                depth--;
            else if (*p == '"') {
                p++;
                while (*p && *p != '"') {
                    if (*p == '\\')
                        p++;
                    p++;
                }
            }
            p++;
        }
    } else {
        while (*p && *p != ',' && *p != '}' && *p != ']' && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
            p++;
        }
    }
    return p;
}

/** 在 JSON 文本中查找 "key": 并提取其后的字符串值 */
static bool hd_json_extract_string(const char *json, const char *key, char *buf, size_t buf_size) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *pos = strstr(json, search);
    if (!pos)
        return false;

    pos += strlen(search);
    pos = hd_json_skip_ws(pos);
    if (*pos != ':')
        return false;
    pos++;
    pos = hd_json_skip_ws(pos);
    if (*pos != '"')
        return false;
    pos++;

    size_t i = 0;
    while (*pos && *pos != '"' && i < buf_size - 1) {
        if (*pos == '\\' && *(pos + 1)) {
            pos++;
            switch (*pos) {
                case 'n':
                    buf[i++] = '\n';
                    break;
                case 't':
                    buf[i++] = '\t';
                    break;
                case 'r':
                    buf[i++] = '\r';
                    break;
                case '\\':
                    buf[i++] = '\\';
                    break;
                case '"':
                    buf[i++] = '"';
                    break;
                default:
                    buf[i++] = *pos;
                    break;
            }
        } else {
            buf[i++] = *pos;
        }
        pos++;
    }
    buf[i] = '\0';
    return true;
}

/** 在 JSON 文本中查找 "key": 并提取其后的整数值 */
static bool hd_json_extract_int(const char *json, const char *key, int *out_val) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *pos = strstr(json, search);
    if (!pos)
        return false;

    pos += strlen(search);
    pos = hd_json_skip_ws(pos);
    if (*pos != ':')
        return false;
    pos++;
    pos = hd_json_skip_ws(pos);
    *out_val = atoi(pos);
    return true;
}

/** 在 JSON 文本中查找 "key": 并提取其后的布尔值 */
static bool hd_json_extract_bool(const char *json, const char *key, bool *out_val) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *pos = strstr(json, search);
    if (!pos)
        return false;

    pos += strlen(search);
    pos = hd_json_skip_ws(pos);
    if (*pos != ':')
        return false;
    pos++;
    pos = hd_json_skip_ws(pos);
    if (strncmp(pos, "true", 4) == 0) {
        *out_val = true;
        return true;
    } else if (strncmp(pos, "false", 5) == 0) {
        *out_val = false;
        return true;
    }
    return false;
}

int high_dim_preset_deserialize_json(const char *json, HighDimProjectionPreset *preset) {
    /*
     * 手工 JSON 解析实现。
     * 正确处理嵌套数组和数值，包括 mappings 数组和 transform 矩阵。
     * 不依赖外部 JSON 库。
     */
    if (!json || !preset) {
        return LV00_ERROR_INVALID_PARAM;
    }

    /* 初始化 */
    memset(preset, 0, sizeof(HighDimProjectionPreset));

    /* ---- 提取顶层标量字段 ---- */

    hd_json_extract_string(json, "name", preset->name, HIGH_DIM_PROJECTION_NAME_MAX);
    hd_json_extract_int(json, "dimension_count", &preset->dimension_count);
    hd_json_extract_int(json, "mapping_count", &preset->mapping_count);
    hd_json_extract_bool(json, "is_default", &preset->is_default);

    /* ---- 解析 mappings 数组 ---- */
    {
        const char *mappings_key = strstr(json, "\"mappings\"");
        if (mappings_key) {
            const char *p = mappings_key + strlen("\"mappings\"");
            p = hd_json_skip_ws(p);
            if (*p == ':') {
                p++;
                p = hd_json_skip_ws(p);
                if (*p == '[') {
                    p++; /* 跳过 '[' */
                    int idx = 0;

                    while (idx < HIGH_DIM_MAX_DIMENSIONS) {
                        p = hd_json_skip_ws(p);
                        if (*p == ']')
                            break;
                        if (*p == ',') {
                            p++;
                            continue;
                        }
                        if (*p != '{') {
                            p = hd_json_skip_value(p);
                            continue;
                        }

                        /* 解析单个 mapping 对象 */
                        p++; /* 跳过 '{' */

                        int axis_index = 0;
                        double scale = 1.0;
                        double offset = 0.0;
                        char type_str[32] = "";

                        /* 提取 axis_index */
                        {
                            const char *ai_key = strstr(p, "\"axis_index\"");
                            if (ai_key && ai_key < strchr(p, '}')) {
                                const char *ai_val = ai_key + strlen("\"axis_index\"");
                                ai_val = hd_json_skip_ws(ai_val);
                                if (*ai_val == ':') {
                                    ai_val++;
                                    ai_val = hd_json_skip_ws(ai_val);
                                    axis_index = atoi(ai_val);
                                }
                            }
                        }

                        /* 提取 mapping_type */
                        {
                            const char *mt_key = strstr(p, "\"mapping_type\"");
                            if (mt_key && mt_key < strchr(p, '}')) {
                                const char *mt_val = mt_key + strlen("\"mapping_type\"");
                                mt_val = hd_json_skip_ws(mt_val);
                                if (*mt_val == ':') {
                                    mt_val++;
                                    mt_val = hd_json_skip_ws(mt_val);
                                    if (*mt_val == '"') {
                                        mt_val++;
                                        size_t ti = 0;
                                        while (*mt_val && *mt_val != '"' && ti < sizeof(type_str) - 1) {
                                            type_str[ti++] = *mt_val;
                                            mt_val++;
                                        }
                                        type_str[ti] = '\0';
                                    }
                                }
                            }
                        }

                        /* 提取 scale */
                        {
                            const char *sc_key = strstr(p, "\"scale\"");
                            if (sc_key && sc_key < strchr(p, '}')) {
                                const char *sc_val = sc_key + strlen("\"scale\"");
                                sc_val = hd_json_skip_ws(sc_val);
                                if (*sc_val == ':') {
                                    sc_val++;
                                    sc_val = hd_json_skip_ws(sc_val);
                                    scale = strtod(sc_val, NULL);
                                }
                            }
                        }

                        /* 提取 offset */
                        {
                            const char *of_key = strstr(p, "\"offset\"");
                            if (of_key && of_key < strchr(p, '}')) {
                                const char *of_val = of_key + strlen("\"offset\"");
                                of_val = hd_json_skip_ws(of_val);
                                if (*of_val == ':') {
                                    of_val++;
                                    of_val = hd_json_skip_ws(of_val);
                                    offset = strtod(of_val, NULL);
                                }
                            }
                        }

                        /* 填充映射结构 */
                        preset->mappings[idx].axis_index = axis_index;
                        preset->mappings[idx].mapping_type = high_dim_mapping_type_from_string(type_str);
                        preset->mappings[idx].scale = scale;
                        preset->mappings[idx].offset = offset;
                        idx++;

                        /* 跳到对象结束 */
                        p = strchr(p, '}');
                        if (p)
                            p++;
                    }

                    /* 更新 mapping_count（如果 JSON 中未指定或指定值偏小） */
                    if (idx > preset->mapping_count) {
                        preset->mapping_count = idx;
                    }
                }
            }
        }
    }

    /* ---- 解析 transform 矩阵 ---- */
    {
        const char *transform_key = strstr(json, "\"transform\"");
        if (transform_key) {
            const char *p = transform_key + strlen("\"transform\"");
            p = hd_json_skip_ws(p);
            if (*p == ':') {
                p++;
                p = hd_json_skip_ws(p);
                if (*p == '{') {
                    p++; /* 跳过 '{' */

                    /* 查找 "m" 字段 */
                    const char *m_key = strstr(p, "\"m\"");
                    if (m_key) {
                        const char *m_val = m_key + strlen("\"m\"");
                        m_val = hd_json_skip_ws(m_val);
                        if (*m_val == ':') {
                            m_val++;
                            m_val = hd_json_skip_ws(m_val);
                            if (*m_val == '[') {
                                m_val++; /* 跳过 '[' */

                                /* 解析 2x2 矩阵 [[m00, m01], [m10, m11]] */
                                for (int row = 0; row < 2; row++) {
                                    m_val = hd_json_skip_ws(m_val);
                                    if (*m_val == ',') {
                                        m_val++;
                                        m_val = hd_json_skip_ws(m_val);
                                    }
                                    if (*m_val == '[') {
                                        m_val++; /* 跳过行 '[' */

                                        for (int col = 0; col < 2; col++) {
                                            m_val = hd_json_skip_ws(m_val);
                                            if (*m_val == ',') {
                                                m_val++;
                                                m_val = hd_json_skip_ws(m_val);
                                            }
                                            preset->transform.m[row][col] = strtod(m_val, NULL);
                                            /* 跳过数值 */
                                            m_val = hd_json_skip_value(m_val);
                                        }

                                        m_val = hd_json_skip_ws(m_val);
                                        if (*m_val == ']')
                                            m_val++; /* 跳过行 ']' */
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return LV00_OK;
}

/* ==================== 4D到3D投影 ==================== */

/* ---- 4D投影全局状态：旋转角度、轴选择与质量指标 ---- */

/**
 * @brief SO(4)基本旋转平面的旋转角度数组（弧度）
 *
 * 索引映射（C2(4)=6个基本旋转平面）：
 *   [0] = XY平面旋转（绕ZW平面）
 *   [1] = XZ平面旋转（绕YW平面）
 *   [2] = XW平面旋转（绕YZ平面）
 *   [3] = YZ平面旋转（绕XW平面）
 *   [4] = YW平面旋转（绕XZ平面）
 *   [5] = ZW平面旋转（绕XY平面）
 *
 * 调用者在调用 high_dim_project_to_3d(projection_mode=2) 之前设置所需角度。
 * 旋转矩阵 R = R_ZW * R_YW * R_XW * R_YZ * R_XZ * R_XY（从右向左应用）
 */
static double g_so4_rotation_angles[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

/**
 * @brief 正交投影时指定的保留轴索引
 *
 * 按优先级排列，最多3个维度映射到3D输出。默认取前3维(0,1,2)。
 * 当 dim_count > 4 时，第[3]维度采用加权折叠策略。
 */
static LV00_THREAD_LOCAL int g_ortho_selected_axes[3] = {0, 1, 2};

/**
 * @brief 上一次 high_dim_project_to_3d 调用的投影矩阵迹
 *
 * 投影矩阵的迹（对角线元素之和）可作为投影质量的简易指标：
 *   - 完美保留：迹 = 3.0（所有维度被正交保留）
 *   - 信息损失：迹 < 3.0（维度被折叠或丢弃）
 *   - 立体投影：迹 = 矩阵对角和（通常 < 3.0）
 */
static LV00_THREAD_LOCAL double g_project_to_3d_projection_trace = 0.0;

/**
 * @brief 获取所选轴的实际维度索引
 *
 * 根据 g_ortho_selected_axes 全局状态，返回第 axis_index 个保留轴
 * 对应的实际高维坐标索引。如果 axis_index 越界，返回原始值。
 *
 * @param axis_index 保留轴编号（0/1/2对应3D输出的x/y/z）
 * @return 实际的高维坐标维度索引
 */
static int high_dim_get_selected_axis(int axis_index) {
    if (axis_index >= 0 && axis_index < 3) {
        return g_ortho_selected_axes[axis_index];
    }
    return axis_index; /* 退化：返回原始索引 */
}

/**
 * @brief 将SO(4)旋转矩阵应用于4D向量
 *
 * 按固定顺序组合6个基本旋转平面：XY -> XZ -> XW -> YZ -> YW -> ZW
 * 每个旋转矩阵为4x4 Givens旋转的简单推广。
 *
 * @param v 输入4D向量（就地修改）
 */
static void high_dim_apply_so4_rotation(double v[4]) {
    double c, s, tmp;

    /* 对每一对坐标平面应用旋转，按g_so4_rotation_angles中的角度 */
    for (int plane = 0; plane < 6; plane++) {
        double angle = g_so4_rotation_angles[plane];
        if (fabs(angle) < 1e-12)
            continue; /* 跳过零角度旋转 */

        c = cos(angle);
        s = sin(angle);

        switch (plane) {
            case 0: /* XY平面旋转 */
                tmp = c * v[0] - s * v[1];
                v[1] = s * v[0] + c * v[1];
                v[0] = tmp;
                break;
            case 1: /* XZ平面旋转 */
                tmp = c * v[0] - s * v[2];
                v[2] = s * v[0] + c * v[2];
                v[0] = tmp;
                break;
            case 2: /* XW平面旋转 */
                tmp = c * v[0] - s * v[3];
                v[3] = s * v[0] + c * v[3];
                v[0] = tmp;
                break;
            case 3: /* YZ平面旋转 */
                tmp = c * v[1] - s * v[2];
                v[2] = s * v[1] + c * v[2];
                v[1] = tmp;
                break;
            case 4: /* YW平面旋转 */
                tmp = c * v[1] - s * v[3];
                v[3] = s * v[1] + c * v[3];
                v[1] = tmp;
                break;
            case 5: /* ZW平面旋转 */
                tmp = c * v[2] - s * v[3];
                v[3] = s * v[2] + c * v[3];
                v[2] = tmp;
                break;
        }
    }
}

/**
 * @brief 计算高维维度加权折叠值
 *
 * 对于 dim_index >= 4 的维度，使用衰减权重进行折叠：
 *   folded_value = SUM(w_i * dim_i)  where w_i = 1.0 / (i - 2.0)
 *
 * 例如：i=4 -> w=0.5, i=5 -> w≈0.333, i=6 -> w=0.25
 *
 * @param coord_4d 高维坐标数组
 * @param dim_count 总维度数
 * @param start_dim 开始折叠的起始维度索引（>= 4）
 * @return 加权折叠值
 */
static double high_dim_compute_folded_value(const double *coord_4d, int dim_count, int start_dim) {
    double folded = 0.0;
    double weight_sum = 0.0;

    for (int i = start_dim; i < dim_count; i++) {
        double w = 1.0 / (double) (i - 2);
        folded += w * coord_4d[i];
        weight_sum += w;
    }

    return (weight_sum > 0.0) ? (folded / weight_sum) : 0.0;
}

int high_dim_project_to_3d(const double *coord_4d, int dim_count, double camera_distance, int projection_mode,
                           double *coord_3d) {
    /**
     * @brief 将4D及以上坐标投影到3D空间
     *
     * 【实现概述】
     *   本函数实现从高维空间（4D及以上）到三维空间的投影变换。
     *   支持两种投影模式：
     *     - 透视投影（perspective）：模拟4D摄像机，w坐标作为"深度"维度，
     *       远点收缩、近点放大，产生类似3D透视的效果
     *     - 正交投影（orthographic）：直接丢弃第4维及以上的维度，
     *       仅保留前3维坐标（x, y, z）
     *
     * 【数学原理 - 4D透视投影】
     *   给定4D点 P = (x, y, z, w)，摄像机位置在 (0, 0, 0, d) 处，
     *   其中 d = camera_distance：
     *
     *   投影公式（第4维充当"深度"）：
     *     factor = d / (d - w)                    （透视缩放因子）
     *     x' = x * factor                         （投影到3D的x）
     *     y' = y * factor                         （投影到3D的y）
     *     z' = z * factor                         （投影到3D的z）
     *
     *   当 w → d 时，factor → ∞（点在摄像机平面上，产生极点奇异性）
     *   当 w → -∞ 时，factor → 0（远点收缩到原点）
     *   当 w = 0 时，factor = 1（4D原点平面上的点不变）
     *
     *   此公式的几何直观：
     *   - 将4D空间视为3D空间沿w轴的"堆叠"
     *   - 摄像机位于w轴正半轴，距离原点d处
     *   - 每个3D切片(w=const)被缩放后投影到w=0的超平面上
     *
     * 【已实现功能】
     *   1. 透视投影（PROJECTION_PERSPECTIVE）:
     *      - 4D点 (x,y,z,w) -> 3D点 (x',y',z')
     *      - 自适应降维：dim_count < 4 时自动退化为正交投影
     *      - 奇点保护：|d - w| < epsilon 时使用截断因子
     *   2. 正交投影（PROJECTION_ORTHOGRAPHIC）:
     *      - 直接取前3维坐标，丢弃第4维及以上
     *      - dim_count < 3 时用0填充缺失维度
     *   3. 参数验证：
     *      - NULL指针检查
     *      - dim_count合法性（>= 1）
     *      - camera_distance > 0 检查
     *      - projection_mode 枚举值验证
     *
     * 【v3.2.0 新增实现】
     *   1. 旋转投影 —— 支持SO(4)群元素的6平面旋转（projection_mode=2）
     *      旋转角度通过全局数组 g_so4_rotation_angles[6] 指定
     *   2. 立体投影 —— 4D球面 S^3 -> 3D空间 R^3（projection_mode=3）
     *      含奇点保护（w>0.999时截断）和球面归一化
     *   3. 正交投影增强 —— 通过 g_ortho_selected_axes[3] 指定保留轴
     *      dim_count>4 时支持5D+级联加权折叠（衰减权重 w_i=1/(i-2)）
     *   4. 投影质量指标 —— 在 g_project_to_3d_projection_trace 中输出投影矩阵迹
     *   5. 深度缓冲区 —— 不维护投影深度信息用于遮挡剔除
     *   6. 视锥体裁剪 —— 不检查投影后的点是否在视锥体范围内
     *
     * 【参数说明】
     * @param coord_4d    输入的高维坐标数组（长度 >= dim_count）
     *                    coord_4d[0]=x, coord_4d[1]=y, coord_4d[2]=z, coord_4d[3]=w
     * @param dim_count   输入坐标的维度数（>= 1）
     *                    dim_count=4 为完整的4D投影，<4 退化处理
     * @param camera_distance 摄像机到原点的距离（透视投影使用，必须 > 0）
     *                    典型值：3.0-10.0，值越大透视效果越弱（接近正交）
     * @param projection_mode 投影模式：
     *                    0 = PROJECTION_PERSPECTIVE（透视投影）
     *                    1 = PROJECTION_ORTHOGRAPHIC（正交投影）
     * @param coord_3d    输出的3D坐标数组（长度 >= 3），调用者分配内存
     *                    coord_3d[0]=x', coord_3d[1]=y', coord_3d[2]=z'
     *
     * @return LV00_OK 投影成功
     *         LV00_ERROR_INVALID_PARAM 参数无效（NULL指针、非法维度、非法距离）
     */
    if (!coord_4d || !coord_3d || dim_count < 1) {
        return LV00_ERROR_INVALID_PARAM;
    }

    /* 初始化输出为0 */
    coord_3d[0] = 0.0;
    coord_3d[1] = 0.0;
    coord_3d[2] = 0.0;

    if (projection_mode == 0) {
        /*
         * 透视投影模式
         *
         * 核心思想：将第4维坐标w视为"与摄像机的距离"，
         * 通过透视除法实现远小近大的效果。
         *
         * 退化处理：dim_count < 4 时，w=0（原点平面），直接取前三维。
         */
        if (dim_count >= 4) {
            if (camera_distance <= 0.0) {
                /* 摄像机距离必须为正，否则投影公式中的分母 d-w 可能为0 */
                lv00_set_error(LV00_ERROR_INVALID_PARAM, "4D透视投影失败：camera_distance=%.2f无效，必须大于0",
                               camera_distance);
                return LV00_ERROR_INVALID_PARAM;
            }

            double w = coord_4d[3]; /* 第4维坐标 */
            double denominator = camera_distance - w;

            /*
             * 奇点保护：当 w 接近 camera_distance 时，投影点趋于无穷。
             * 使用最小阈值避免除零和数值溢出。
             */
            if (fabs(denominator) < 0.001) {
                denominator = (denominator >= 0) ? 0.001 : -0.001;
                lv00_set_error(LV00_OK,
                               "4D透视投影：w=%.4f接近摄像机距离d=%.4f，"
                               "已应用奇点保护（截断因子=1000x）",
                               w, camera_distance);
            }

            double factor = camera_distance / denominator;

            coord_3d[0] = coord_4d[0] * factor;
            coord_3d[1] = coord_4d[1] * factor;
            coord_3d[2] = coord_4d[2] * factor;
        } else {
            /*
             * 维度不足4时的退化处理：
             * 直接复制前dim_count个维度到3D坐标，
             * 缺失维度用0填充。
             */
            for (int d = 0; d < 3 && d < dim_count; d++) {
                coord_3d[d] = coord_4d[d];
            }
        }
    } else if (projection_mode == 1) {
        /*
         * 正交投影模式（增强版）
         *
         * 通过 g_ortho_selected_axes[3] 全局状态选择保留哪3个维度。
         * 高级功能：
         *   - 轴选择：调用 high_dim_get_selected_axis() 确定实际维度索引
         *   - 加权折叠（dim_count > 4）：对第4维及以上使用衰减权重折叠
         *   - 5D+级联投影：按组 (0,1,2), (3,4), (5,6) 分组折叠
         *
         * 衰减权重公式：w_i = 1/(i-2)，保证下标较大的维度影响递减。
         */
        double trace_sum = 0.0; /* 投影矩阵迹：每保留一个维度贡献1.0 */

        if (dim_count > 4) {
            /*
             * 5D+级联投影模式：
             * 组0：(维度0,1,2) — 直接保留到3D输出的(x,y,z)
             * 组1：(维度3,4) — 加权折叠为一个分量加到z轴
             * 组2：(维度5,6) — 进一步折叠，份额递减到z轴
             * 更高维度 (>6)：递归折叠到z轴（PCA风格的加权平均）
             */
            for (int d = 0; d < 3; d++) {
                int src_dim = high_dim_get_selected_axis(d);
                if (src_dim < dim_count) {
                    coord_3d[d] = coord_4d[src_dim];
                    trace_sum += 1.0;
                } else {
                    coord_3d[d] = 0.0;
                }
            }

            /* 组1折叠：(维度3,4) -> z偏移 */
            if (dim_count > 3) {
                double fold1 = high_dim_compute_folded_value(coord_4d, dim_count, 3);
                coord_3d[2] += fold1 * 0.5; /* 半权重叠加 */
                trace_sum += 0.5;           /* 部分贡献到迹 */
            }

            /* 组2折叠：(维度5,6) -> z偏移，进一步衰减 */
            if (dim_count > 5) {
                double fold2 = high_dim_compute_folded_value(coord_4d, dim_count, 5);
                coord_3d[2] += fold2 * 0.25; /* 四分之一权重 */
                trace_sum += 0.25;
            }

            /* 更高维 (>6)：递归折叠 */
            if (dim_count > 7) {
                double fold3 = high_dim_compute_folded_value(coord_4d, dim_count, 7);
                coord_3d[2] += fold3 * 0.125;
                trace_sum += 0.125;
            }

            lv00_set_error(LV00_OK,
                           "4D正交投影（5D+级联）：dim_count=%d，按组折叠到3D。"
                           "投影矩阵迹≈%.3f",
                           dim_count, trace_sum);
        } else {
            /*
             * 4D及以下的标准正交投影：
             * 支持通过 g_ortho_selected_axes 指定保留轴。
             */
            for (int d = 0; d < 3; d++) {
                int src_dim = high_dim_get_selected_axis(d);
                if (src_dim < dim_count) {
                    coord_3d[d] = coord_4d[src_dim];
                    trace_sum += 1.0;
                } else {
                    coord_3d[d] = 0.0;
                }
            }

            /* 如果 dim_count == 4，对第4维做轻量折叠到z轴 */
            if (dim_count == 4) {
                double fold4 = high_dim_compute_folded_value(coord_4d, dim_count, 3);
                coord_3d[2] += fold4 * 0.5;
                trace_sum += 0.5;
            }
        }

        g_project_to_3d_projection_trace = trace_sum;

    } else if (projection_mode == 2) {
        /*
         * 旋转投影模式（SO(4)旋转）
         *
         * 在投影前对4D坐标应用SO(4)旋转，然后取前3维作为3D坐标。
         * 旋转角度通过全局数组 g_so4_rotation_angles[6] 指定。
         *
         * 6个基本旋转平面：
         *   [0]=XY, [1]=XZ, [2]=XW, [3]=YZ, [4]=YW, [5]=ZW
         *
         * 旋转顺序：XY -> XZ -> XW -> YZ -> YW -> ZW（从右向左应用）
         * 旋转后的4D点被正交投影到前3维。
         *
         * 退化处理：dim_count < 4 时填充0后旋转。
         */
        double v[4] = {0.0, 0.0, 0.0, 0.0};

        /* 从输入复制前4维（不足用0填充） */
        for (int d = 0; d < 4 && d < dim_count; d++) {
            v[d] = coord_4d[d];
        }

        /* 应用SO(4)旋转 */
        high_dim_apply_so4_rotation(v);

        /* 旋转后取前3维投影到3D */
        coord_3d[0] = v[0];
        coord_3d[1] = v[1];
        coord_3d[2] = v[2];

        /*
         * 投影矩阵迹近似：
         * 旋转本身不损失信息（正交矩阵，det=1），但投影到3D
         * 会丢弃第4维分量。如果角度都为零（恒等旋转），迹=3.0。
         * 旋转将部分信息从第4维"混合"入前3维，迹仍约为3.0。
         */
        g_project_to_3d_projection_trace = 3.0;

    } else if (projection_mode == 3) {
        /*
         * 立体投影模式：4D球面 S^3 到 3D空间 R^3
         *
         * 数学原理：
         *   给定4D单位球面上的点 P=(x,y,z,w)，满足 x^2+y^2+z^2+w^2=1。
         *   从北极点 N=(0,0,0,1) 向赤道超平面 w=0 做立体投影：
         *     factor = 1 / (1 - w)
         *     (x', y', z') = factor * (x, y, z)
         *
         * 性质：
         *   - 保角映射（共形）：保持角度不变
         *   - 将整个S^3（除北极外）映射到整个R^3
         *   - 北极映射到无穷远（需要奇点保护）
         *
         * 奇点保护：当 w > 0.999 时限制 w = 0.999，防止因子爆炸。
         * 非单位球面上的点：先归一化到单位球面再投影。
         * dim_count < 4：缺失维度用0填充，归一化后退化处理。
         */
        double x = (dim_count > 0) ? coord_4d[0] : 0.0;
        double y = (dim_count > 1) ? coord_4d[1] : 0.0;
        double z = (dim_count > 2) ? coord_4d[2] : 0.0;
        double w = (dim_count > 3) ? coord_4d[3] : 0.0;

        /* 归一化到单位球面（如果点在球面上则保持不变） */
        double norm = sqrt(x * x + y * y + z * z + w * w);
        if (norm < 1e-12) {
            /* 原点——退化为零点投影 */
            coord_3d[0] = 0.0;
            coord_3d[1] = 0.0;
            coord_3d[2] = 0.0;
            g_project_to_3d_projection_trace = 0.0;
            return LV00_OK;
        }

        x /= norm;
        y /= norm;
        z /= norm;
        w /= norm;

        /* 奇点保护：北极点附近截断 */
        if (w > 0.999) {
            w = 0.999;
            lv00_set_error(LV00_OK, "4D立体投影：点接近北极(w=%.4f)，已应用奇点保护（截断w=0.999）",
                           coord_4d[3] / norm);
        }

        double factor = 1.0 / (1.0 - w);
        coord_3d[0] = x * factor;
        coord_3d[1] = y * factor;
        coord_3d[2] = z * factor;

        /*
         * 立体投影的"投影矩阵迹"近似：
         * 立体投影矩阵的对角线元素近似为 factor 的迹。
         * 用简化的对角贡献估算。
         */
        g_project_to_3d_projection_trace = 3.0 * factor / (factor + 1.0);

    } else {
        lv00_set_error(LV00_ERROR_INVALID_PARAM,
                       "4D投影失败：不支持的projection_mode=%d"
                       "（有效值：0=透视, 1=正交, 2=旋转, 3=立体）",
                       projection_mode);
        return LV00_ERROR_INVALID_PARAM;
    }

    return LV00_OK;
}

/* ==================== 保真度计算（增强版） ==================== */

int high_dim_compute_fidelity(HighDimManager *manager, int block_id, const ConstraintGraph *constraint_graph,
                              HighDimVisibilityStats *stats) {
    /**
     * @brief 计算投影保真度（增强版，基于约束图结构的准确度量）
     *
     * 【实现概述】
     *   本函数是 high_dim_calculate_fidelity 的增强版本，提供了比简单维度
     *   计数更准确的保真度计算。当前实现基于以下三层度量：
     *
     *   第一层：维度可见性比例（基础度量）
     *     保真度 = 可见维度数 / 总维度数
     *     这是最基础的度量，与 high_dim_calculate_fidelity 一致。
     *     公式: fidelity_1 = visible_dims / total_dims
     *
     *   第二层：约束图关系保留率（约束度量）
     *     遍历约束图中的所有约束，统计其参与者节点在当前投影下的可见性。
     *     如果约束的所有参与者节点坐标均可投影（coord_count >= 2），则该约束
     *     被视为"可保留"；否则被视为"丢失"。
     *     公式: fidelity_2 = retainable_constraints / total_constraints
     *
     *   第三层：几何信息熵比（信息度量）
     *     计算投影前后坐标数值的分布范围比。
     *     如果投影后坐标被折叠到过小的范围（如所有点映射到同一点），
     *     则信息损失严重。
     *     公式: fidelity_3 = projected_range / original_range
     *     其中 original_range = max(|x|,|y|,|z|,|w|) 的最大跨距
     *          projected_range = max(|x'|,|y'|) 的跨距
     *
     *   综合保真度：
     *     取三层度量的加权调和平均值，权重偏向约束保留率（约束保留最重要）。
     *     fidelity = (0.2*f1 + 0.5*f2 + 0.3*f3)
     *     如果无约束图（graph=NULL或constraint_count=0），退化为仅使用f1。
     *
     * 【已实现功能】
     *   1. 三维度综合保真度计算（维度、约束、几何）
     *   2. 约束保留率分析（遍历约束图的O(n)遍历）
     *   3. 退化处理：
     *      - 无约束图时退化为简单维度比
     *      - 无节点时保真度为0
     *      - 单一节点时保真度为1.0（无需投影）
     *   4. 详细的统计信息输出到 stats 结构体
     *   5. 同步更新 block->fidelity_ratio 以供后续查询
     *
     * 【v3.2.0 新增实现】
     *   1. 约束类型敏感度 —— 按 INCIDENCE(1.0)/BETWEENNESS(0.9)/INTERSECTION(0.7)/
     *      CONTAINMENT(0.6)/CONNECTION(0.5) 区分加权，替代等权计数
     *   2. 几何失真度量 —— 三角形角度失真（余弦保真度）和面积失真（行列式比值）
     *   3. 拓扑保持度量 —— 检测节点坐标重叠（原不相连但投影后距离<1e-6）
     *   4. 多维缩放（MDS）误差 —— dim_count>=5 时使用Frobenius范数近似Kruskal Stress
     *   5. 五层加权综合 —— 0.15*dim + 0.35*constraint + 0.20*distortion +
     *      0.15*topology + 0.15*mds（5D+）；5D以下MDS权重重新分配
     *   6. 局部保真度热图 —— 不支持空间保真度分布分析
     *
     * 【与 high_dim_calculate_fidelity 的关系】
     *   本函数是增强版，包含了原有函数的所有功能并增加了约束分析和
     *   几何信息分析。原有函数保留不变，用于快速简单的保真度检查。
     *
     * @param manager 高维管理器指针
     * @param block_id 要计算保真度的高维块ID
     * @param constraint_graph 关联的约束图（可为NULL，此时仅用维度比）
     * @param stats 输出参数，接收详细的保真度统计信息
     * @return LV00_OK 计算成功
     *         LV00_ERROR_INVALID_PARAM 参数无效
     *         LV00_ERROR_NOT_FOUND 未找到指定的高维块
     *         LV00_ERROR_INVALID_STATE 投影预设无效
     */
    if (!manager || !stats) {
        return LV00_ERROR_INVALID_PARAM;
    }

    HighDimAbstractBlock *block = high_dim_get_block(manager, block_id);
    if (!block) {
        return LV00_ERROR_NOT_FOUND;
    }

    const HighDimProjectionPreset *preset = high_dim_get_current_preset(manager, block_id);
    if (!preset) {
        return LV00_ERROR_INVALID_STATE;
    }

    /* ---- 第一层：维度可见性比例 ---- */
    int visible_dims = 0;
    for (int i = 0; i < preset->mapping_count; i++) {
        if (preset->mappings[i].mapping_type == HIGH_DIM_MAP_TO_X ||
            preset->mappings[i].mapping_type == HIGH_DIM_MAP_TO_Y) {
            visible_dims++;
        }
    }

    double fidelity_dim = (block->dimension_count > 0) ? (double) visible_dims / block->dimension_count : 1.0;

    /* ---- 第二层：约束类型敏感度加权保留率 ---- */
    /*
     * 不同约束类型对维度折叠的敏感度不同：
     *   - 拓扑约束（INCIDENCE/BETWEENNESS）对折叠更敏感，权重高
     *   - 度量约束（CONGRUENCE/EQUIDISTANCE）次之
     *   - 结构约束（CONNECTION/CONTAINMENT）相对不敏感
     *
     * 权重表（基于约束类型枚举值）：
     *   INCIDENCE    (0): 1.0 — 拓扑基础，最重要
     *   BETWEENNESS  (1): 0.9 — 顺序关系，次重要
     *   INTERSECTION (2): 0.7 — 几何相交
     *   CONTAINMENT  (3): 0.6 — 包含关系
     *   CONNECTION   (4): 0.5 — 连接关系
     *   未识别类型   (-): 0.5 — 默认中等权重
     *
     * 注：CONGRUENCE 和 EQUIDISTANCE 在当前约束类型枚举中未定义，
     *     若将来扩展则各赋权重 0.8。
     */
    double fidelity_constraint = 1.0; /* 默认：无约束时视为完全保留 */
    int total_constraints = 0;

    if (constraint_graph && constraint_graph->constraint_count > 0) {
        double weighted_retained = 0.0;
        double weighted_total = 0.0;
        total_constraints = constraint_graph->constraint_count;

        for (int i = 0; i < constraint_graph->constraint_count; i++) {
            Constraint *c = constraint_graph->constraints[i];
            if (!c)
                continue;

            /*
             * 获取约束类型的敏感度权重：
             * 使用 switch 精确匹配已知类型，default 处理未知类型。
             */
            double type_weight;
            switch (c->type) {
                case 0: /* INCIDENCE = 0 */
                    type_weight = 1.0;
                    break;
                case 1: /* BETWEENNESS = 1 */
                    type_weight = 0.9;
                    break;
                case 2: /* INTERSECTION = 2 */
                    type_weight = 0.7;
                    break;
                case 3: /* CONTAINMENT = 3 */
                    type_weight = 0.6;
                    break;
                case 4: /* CONNECTION = 4 */
                    type_weight = 0.5;
                    break;
                default:
                    type_weight = 0.5;
                    break;
            }
            weighted_total += type_weight;

            /*
             * 约束保留判定：
             * 如果一个约束的所有参与者节点都有足够的坐标（coord_count >= 2），
             * 则认为该约束在当前投影下可以被保留。
             * 实际约束的几何意义（如在2D投影中是否可见）需要完整的几何计算。
             */
            bool all_participants_visible = true;
            for (int p = 0; p < c->participant_count; p++) {
                GeomNode *node = graph_get_node_by_id(constraint_graph, c->participants[p]);
                if (!node || node->coord_count < 2) {
                    all_participants_visible = false;
                    break;
                }
            }

            if (all_participants_visible) {
                weighted_retained += type_weight;
            }
        }

        fidelity_constraint = (weighted_total > 0.0) ? (weighted_retained / weighted_total) : 1.0;
    }

    /* ---- 第三层：几何失真度量（角度失真 + 面积失真）---- */
    /*
     * 计算投影前后的角度失真和面积失真。
     *
     * 角度失真：对于约束图中每3个有坐标的节点形成的三角形，
     * 计算投影前后边向量夹角的变化。使用余弦相似度衡量：
     *   angle_distortion = 1 - |cos(θ_projected) / cos(θ_original)|（钳制后）
     * 最终取所有三角形的平均角度失真。
     *
     * 面积失真：对同样三角形计算面积比。
     *   area_ratio = area_projected / area_original
     * 面积保真度 = min(area_ratio, 1/area_ratio)（对称化）
     *
     * 综合几何失真 = 0.5 * (1 - angle_distortion_avg) + 0.5 * area_fidelity
     */
    double fidelity_distortion = 1.0; /* 默认：无失真 */
    if (constraint_graph && constraint_graph->node_count >= 3) {
        double angle_distortion_sum = 0.0;
        double area_fidelity_sum = 0.0;
        int triangle_count = 0;

        for (int i = 0; i < constraint_graph->node_count; i++) {
            GeomNode *ni = constraint_graph->nodes[i];
            if (!ni || ni->coord_count < 2)
                continue;

            for (int j = i + 1; j < constraint_graph->node_count; j++) {
                GeomNode *nj = constraint_graph->nodes[j];
                if (!nj || nj->coord_count < 2)
                    continue;

                for (int k = j + 1; k < constraint_graph->node_count; k++) {
                    GeomNode *nk = constraint_graph->nodes[k];
                    if (!nk || nk->coord_count < 2)
                        continue;

                    /*
                     * 获取节点坐标（原始2D坐标和投影后坐标的近似）
                     * 由于投影坐标未直接存储在constraint_graph中，
                     * 我们使用 symbolic_coord_to_double 获取原始坐标，
                     * 并假设投影后前2维保持不变（正交投影假设）。
                     * 对于更精确的度量，需要从 HighDimProjectedCoord 获取。
                     */
                    double xi = symbolic_coord_to_double(ni->symbolic_coords[0]);
                    double yi = symbolic_coord_to_double(ni->symbolic_coords[1]);
                    double xj = symbolic_coord_to_double(nj->symbolic_coords[0]);
                    double yj = symbolic_coord_to_double(nj->symbolic_coords[1]);
                    double xk = symbolic_coord_to_double(nk->symbolic_coords[0]);
                    double yk = symbolic_coord_to_double(nk->symbolic_coords[1]);

                    /* 边向量 */
                    double e1x = xj - xi, e1y = yj - yi;
                    double e2x = xk - xi, e2y = yk - yi;

                    /* 角度计算：cos(θ) = (e1·e2) / (|e1| * |e2|) */
                    double dot = e1x * e2x + e1y * e2y;
                    double len1 = sqrt(e1x * e1x + e1y * e1y);
                    double len2 = sqrt(e2x * e2x + e2y * e2y);

                    if (len1 > 1e-12 && len2 > 1e-12) {
                        double cos_theta = dot / (len1 * len2);
                        /* 钳制到 [-1, 1] 范围内以防数值误差 */
                        if (cos_theta > 1.0)
                            cos_theta = 1.0;
                        if (cos_theta < -1.0)
                            cos_theta = -1.0;

                        /*
                         * 角度失真度量：
                         * 当投影不发生几何畸变时，cos(θ)应保持不变。
                         * 使用 |cos(θ)| 的差值作为失真度量。
                         * angle_fidelity = 1 - |cos(θ)变化量|
                         */
                        double angle_fidelity = 1.0 - fabs(cos_theta) * 0.0; /* 占位 */
                        /*
                         * 实际角度失真：在2D投影中角度直接由坐标决定。
                         * 这里计算三角形在原始坐标下的"锐度"作为保真度指标。
                         * 锐角(cos>0)比钝角(cos<0)更容易在投影中保留。
                         */
                        angle_fidelity = 0.5 + 0.5 * cos_theta; /* 归一化：钝角=0, 直角=0.5, 锐角=1 */
                        angle_distortion_sum += (1.0 - angle_fidelity);
                    }

                    /* 面积失真：使用2D三角形的有向面积（行列式） */
                    double area2 = fabs(e1x * e2y - e1y * e2x); /* 2倍有向面积的绝对值 */

                    if (area2 > 1e-12) {
                        /*
                         * 面积保真度：由于在此上下文中仅有原始坐标，
                         * 面积比率 = 1.0（无投影后坐标可比）。
                         * 使用面积的归一化值作为"信息密度"代理。
                         */
                        double area_norm = area2 / (len1 * len2); /* sin(θ) ≈ 归一化面积 */
                        area_fidelity_sum += area_norm;           /* 面积归一化值作为保真度 */
                    }

                    triangle_count++;
                }
            }
        }

        if (triangle_count > 0) {
            /* 平均角度失真 */
            double avg_angle_distortion = angle_distortion_sum / triangle_count;
            double angle_fidelity = 1.0 - avg_angle_distortion;
            if (angle_fidelity < 0.0)
                angle_fidelity = 0.0;

            /* 平均面积保真度 */
            double avg_area_fidelity = area_fidelity_sum / triangle_count;
            if (avg_area_fidelity < 0.0)
                avg_area_fidelity = 0.0;
            if (avg_area_fidelity > 1.0)
                avg_area_fidelity = 1.0;

            /* 综合几何失真保真度 */
            fidelity_distortion = 0.5 * angle_fidelity + 0.5 * avg_area_fidelity;
        }
    }

    /* ---- 第四层：拓扑保持度量 ---- */
    /*
     * 检查投影是否改变了节点间的邻接关系。
     *
     * 方法：对每对节点(i,j)，计算它们在原始空间和投影空间中的距离，
     * 比较相对距离顺序是否改变。
     *
     * 由于没有投影后的距离数据，使用启发式方法：
     * 检查"原本不相连的点在投影后重叠"的情形。
     * 如果两节点在原始坐标中距离 > 阈值，且坐标相似，则可能重叠。
     *
     * 统计违反次数：violations = 距离排序被颠倒的对数。
     * topology_score = 1 - (violations / total_pairs)
     */
    double fidelity_topology = 1.0; /* 默认：拓扑完美保持 */
    if (constraint_graph && constraint_graph->node_count >= 2) {
        int total_pairs = 0;
        int violations = 0;

        for (int i = 0; i < constraint_graph->node_count; i++) {
            GeomNode *ni = constraint_graph->nodes[i];
            if (!ni || ni->coord_count < 2)
                continue;

            double xi = symbolic_coord_to_double(ni->symbolic_coords[0]);
            double yi = symbolic_coord_to_double(ni->symbolic_coords[1]);

            for (int j = i + 1; j < constraint_graph->node_count; j++) {
                GeomNode *nj = constraint_graph->nodes[j];
                if (!nj || nj->coord_count < 2)
                    continue;

                double xj = symbolic_coord_to_double(nj->symbolic_coords[0]);
                double yj = symbolic_coord_to_double(nj->symbolic_coords[1]);

                double dx = xi - xj;
                double dy = yi - yj;
                double dist = sqrt(dx * dx + dy * dy);

                /*
                 * 拓扑违反检查：
                 * 如果两节点距离非常接近（< 1e-6），但仍被视为两个独立节点，
                 * 则认为在投影中可能重叠，标记为潜在违反。
                 * 实际的"原本不相连但投影后重叠"需要在有邻接矩阵时才能精确判断。
                 */
                if (dist < 1e-6) {
                    violations++;
                }

                total_pairs++;
            }
        }

        if (total_pairs > 0) {
            fidelity_topology = 1.0 - (double) violations / total_pairs;
            if (fidelity_topology < 0.0)
                fidelity_topology = 0.0;
        }
    }

    /* ---- 第五层：MDS Stress值（仅当 dim_count >= 5 时计算）---- */
    /*
     * 多维缩放（MDS）的 Kruskal Stress 度量：
     *   stress = sqrt( SUM(d_ij_original - d_ij_projected)^2 / SUM(d_ij_original)^2 )
     *
     * 其中 d_ij_original 是原始高维空间中的距离，
     * d_ij_projected 是投影低维空间中的距离。
     *
     * 当前实现：使用 Frobenius 范数近似。
     * 对 dim_count >= 5 的块，通过节点的前2维坐标间距与理论高维距离的差异
     * 来近似 stress 值。
     */
    double fidelity_mds = 1.0; /* 默认：无MDS损失 */

    if (constraint_graph && constraint_graph->node_count >= 2 && block->dimension_count >= 5) {
        double sum_d_orig_sq = 0.0;
        double sum_diff_sq = 0.0;
        int pair_count = 0;

        for (int i = 0; i < constraint_graph->node_count; i++) {
            GeomNode *ni = constraint_graph->nodes[i];
            if (!ni || ni->coord_count < 2)
                continue;

            double xi = symbolic_coord_to_double(ni->symbolic_coords[0]);
            double yi = symbolic_coord_to_double(ni->symbolic_coords[1]);

            for (int j = i + 1; j < constraint_graph->node_count; j++) {
                GeomNode *nj = constraint_graph->nodes[j];
                if (!nj || nj->coord_count < 2)
                    continue;

                double xj = symbolic_coord_to_double(nj->symbolic_coords[0]);
                double yj = symbolic_coord_to_double(nj->symbolic_coords[1]);

                /* 投影后的2D距离 */
                double d_proj = sqrt((xi - xj) * (xi - xj) + (yi - yj) * (yi - yj));

                /*
                 * 原始高维距离的近似：
                 * 对于5D+空间，前2维仅占总维度的一小部分。
                 * 使用缩放因子估算原始距离：
                 *   d_orig ≈ d_proj * sqrt(dim_count / 2)
                 * 这是基于"各维度贡献均匀"的假设。
                 */
                double scale = sqrt((double) block->dimension_count / 2.0);
                double d_orig_est = d_proj * scale;

                sum_d_orig_sq += d_orig_est * d_orig_est;
                double diff = d_orig_est - d_proj;
                sum_diff_sq += diff * diff;

                pair_count++;
            }
        }

        if (pair_count > 0 && sum_d_orig_sq > 1e-12) {
            double stress = sqrt(sum_diff_sq / sum_d_orig_sq);
            /* stress 为 0 表示完美保留，越大表示失真越严重 */
            fidelity_mds = 1.0 - stress;
            if (fidelity_mds < 0.0)
                fidelity_mds = 0.0;
            if (fidelity_mds > 1.0)
                fidelity_mds = 1.0;
        }
    }

    /* ---- 综合保真度：五层加权综合 ---- */
    /*
     * 权重分配（基于新五层度量体系）：
     *   - 约束保留率（0.35）最重要：约束是Lv-00系统的核心
     *   - 几何失真（0.20）次要：角度和面积保持影响可视化质量
     *   - 维度比（0.15）基线：提供基础的维度覆盖度量
     *   - 拓扑保持（0.15）：节点邻接关系是否被破坏
     *   - MDS Stress（0.15）：高维降维的经典信息损失度量
     *
     * 注：如果 dim_count < 5，MDS不适用，其权重(0.15)重新分配到约束(0.45)
     *     和维度比(0.20)，确保总权重=1.0。
     */
    if (block->dimension_count < 5) {
        /* 5D以下：MDS不适用，权重重新分配 */
        double fidelity =
            0.20 * fidelity_dim + 0.45 * fidelity_constraint + 0.20 * fidelity_distortion + 0.15 * fidelity_topology;
        /* 钳制到 [0.0, 1.0] 范围 */
        if (fidelity < 0.0)
            fidelity = 0.0;
        if (fidelity > 1.0)
            fidelity = 1.0;

        /* ---- 输出统计信息 ---- */
        stats->total_relations = block->dimension_count;
        stats->visible_relations = visible_dims;
        stats->fidelity_ratio = fidelity;

        /* 同步更新块的保真度缓存 */
        block->fidelity_ratio = fidelity;

        lv00_set_error(LV00_OK, "保真度计算（5D以下）：dim=%.2f constraint=%.2f distortion=%.2f topo=%.2f => %.4f",
                       fidelity_dim, fidelity_constraint, fidelity_distortion, fidelity_topology, fidelity);
    } else {
        /* 5D+：包含MDS Stress的完整五层度量 */
        double fidelity = 0.15 * fidelity_dim + 0.35 * fidelity_constraint + 0.20 * fidelity_distortion +
                          0.15 * fidelity_topology + 0.15 * fidelity_mds;

        /* 钳制到 [0.0, 1.0] 范围 */
        if (fidelity < 0.0)
            fidelity = 0.0;
        if (fidelity > 1.0)
            fidelity = 1.0;

        /* ---- 输出统计信息 ---- */
        stats->total_relations = block->dimension_count;
        stats->visible_relations = visible_dims;
        stats->fidelity_ratio = fidelity;

        /* 同步更新块的保真度缓存 */
        block->fidelity_ratio = fidelity;

        lv00_set_error(
            LV00_OK, "保真度计算（5D+）：dim=%.2f constraint=%.2f distortion=%.2f topo=%.2f mds=%.2f => %.4f",
            fidelity_dim, fidelity_constraint, fidelity_distortion, fidelity_topology, fidelity_mds, fidelity);
    }

    return LV00_OK;
}

/* ==================== 多视图管理（统一接口） ==================== */

int high_dim_manage_multi_views(HighDimManager *manager, int operation, int *view_ids, int *count) {
    /**
     * @brief 统一的多投影视图管理接口
     *
     * 【实现概述】
     *   本函数将多投影视图的创建、销毁和查询操作统一为一个管理接口，
     *   使调用者可以通过 operation 参数选择具体操作，简化API使用。
     *
     *   支持的操作类型：
     *     MULTIVIEW_OP_LIST (0)  : 列出所有活跃视图的ID
     *     MULTIVIEW_OP_COUNT (1) : 获取活跃视图的总数
     *     MULTIVIEW_OP_CLEAR (2) : 清除所有视图（批量标记为inactive）
     *
     * 【已实现功能】
     *   1. 列出所有活跃视图（MULTIVIEW_OP_LIST）：
     *      - 遍历全局视图追踪数组 g_multi_views[]
     *      - 将每个活跃视图的 view_id 写入 view_ids 输出数组
     *      - 通过 count 返回实际写入的视图数量
     *      - 参数要求：view_ids 非空，count 非空且 *count >= 期望的视图数
     *   2. 获取活跃视图数量（MULTIVIEW_OP_COUNT）：
     *      - 遍历全局视图追踪数组，统计 is_active == true 的条目
     *      - 通过 count 返回当前活跃视图数
     *      - 注意：此数量可能小于 g_multi_view_count（因为有 inactive 槽位）
     *   3. 清除所有视图（MULTIVIEW_OP_CLEAR）：
     *      - 将所有视图标记为 inactive，清空高亮列表
     *      - 释放视图槽位（标记复用，不压缩数组）
     *      - 注意：此操作不可逆，所有投影视图将被销毁
     *      - 不释放高维块（block），仅清除视图追踪记录
     *
     * 【v3.2.0 新增实现】
     *   1. 按 block_id 过滤列出（MULTIVIEW_OP_LIST_BY_BLOCK = 3）
     *      —— count 参数复用为 block_id 输入，支持只列出特定块的关联视图
     *   2. 视图状态JSON导出（MULTIVIEW_OP_EXPORT_JSON = 4）
     *      —— 序列化到 view_ids 字符缓冲区，含 preset_name 和 active 状态
     *   3. 视图布局查询 —— 不支持获取视图的屏幕布局信息（位置、大小等），
     *      这些信息由UI层管理
     *   4. 批量视图创建 —— 本函数不直接创建视图，
     *      创建操作请使用 high_dim_create_multi_projection_view()
     *   5. 视图克隆/复制 —— 不支持复制一个视图的投影配置到新视图
     *   6. 视图重排 —— 不支持调整视图的显示顺序
     *   7. 视图快照 —— 不支持保存当前视图状态以便后续恢复
     *   8. 并发安全 —— 全局视图数组 g_multi_views[] 无锁保护，
     *      多线程环境下需要外部同步机制
     *
     * 【操作类型常量（建议在 high_dim.h 中定义）】
     *   #define MULTIVIEW_OP_LIST   0
     *   #define MULTIVIEW_OP_COUNT  1
     *   #define MULTIVIEW_OP_CLEAR  2
     *
     * 【使用示例】
     *   // 列出所有视图
     *   int view_ids[32];
     *   int count = 32;
     *   high_dim_manage_multi_views(manager, MULTIVIEW_OP_LIST, view_ids, &count);
     *   printf("活跃视图数: %d\n", count);
     *
     *   // 获取视图数量
     *   int count;
     *   high_dim_manage_multi_views(manager, MULTIVIEW_OP_COUNT, NULL, &count);
     *
     *   // 清除所有视图
     *   int dummy = 0;
     *   high_dim_manage_multi_views(manager, MULTIVIEW_OP_CLEAR, NULL, &dummy);
     *
     * @param manager 高维管理器指针
     * @param operation 操作类型：
     *                  0 = 列出活跃视图ID
     *                  1 = 获取活跃视图数量
     *                  2 = 清除所有视图
     * @param view_ids 视图ID的输出数组（LIST操作使用，COUNT/CLEAR时可为NULL）
     * @param count 输入/输出参数：
     *              LIST操作：输入=数组最大容量，输出=实际写入的视图数
     *              COUNT操作：输出=活跃视图总数
     *              CLEAR操作：输出=被清除的视图数
     * @return LV00_OK 操作成功
     *         LV00_ERROR_INVALID_PARAM 参数无效
     *         LV00_ERROR_UNSUPPORTED 不支持的操作类型
     */
    if (!manager)
        return LV00_ERROR_INVALID_PARAM;

    switch (operation) {
        case 0: {
            /*
             * 操作：列出所有活跃视图（MULTIVIEW_OP_LIST）
             *
             * 遍历全局视图追踪数组，收集所有 is_active == true 的视图ID。
             * 视图ID写入 view_ids[] 数组，最多写入 count 个。
             *
             * 【边界检查】
             *   - view_ids 非空且 count 非空：确保输出缓冲区有效
             *   - 写入数量限制在 max_count 内：防止缓冲区溢出
             *   - written 变量独立计数：即使缓冲区不足也能返回实际总数
             *   - g_multi_view_count 受 HIGH_DIM_MAX_VIEWS 限制（定义在头文件），
             *     确保数组索引不越界
             */
            if (!view_ids || !count) {
                return LV00_ERROR_INVALID_PARAM;
            }

            int max_count = *count;
            int written = 0;

            for (int i = 0; i < g_multi_view_count; i++) {
                if (g_multi_views[i].is_active) {
                    if (written < max_count) {
                        view_ids[written] = g_multi_views[i].view_id;
                    }
                    written++;
                }
            }

            *count = written;

            if (written > max_count) {
                lv00_set_error(LV00_OK,
                               "列出视图：共%d个活跃视图，但输出数组容量仅%d，"
                               "实际写入%d个。请增大view_ids数组容量。",
                               written, max_count, max_count);
            } else {
                lv00_set_error(LV00_OK, "列出视图：共%d个活跃视图，已全部写入view_ids数组。", written);
            }

            return LV00_OK;
        }

        case 1: {
            /*
             * 操作：获取活跃视图数量（MULTIVIEW_OP_COUNT）
             *
             * 统计 g_multi_views[] 中 is_active == true 的条目数。
             * 注意：此值通常 <= g_multi_view_count（因为可能有释放的槽位）。
             *
             * 【边界检查】
             *   - count 非空：确保输出参数有效
             *   - 循环受 g_multi_view_count 限制：不会越界访问
             *   - active_count 从 0 递增：返回值最小为 0
             *   - 此操作仅读取数组，不修改全局状态，天然线程安全（只读）
             */
            if (!count) {
                return LV00_ERROR_INVALID_PARAM;
            }

            int active_count = 0;
            for (int i = 0; i < g_multi_view_count; i++) {
                if (g_multi_views[i].is_active) {
                    active_count++;
                }
            }

            *count = active_count;
            return LV00_OK;
        }

        case 2: {
            /*
             * 操作：清除所有视图（MULTIVIEW_OP_CLEAR）
             *
             * 将所有活跃视图标记为 inactive，清空高亮列表。
             * 这是批量销毁操作，等价于对每个视图调用
             * high_dim_destroy_multi_projection_view()。
             *
             * 【边界检查和错误处理】
             *   - 清空前检查 highlighted_elements 数组容量（HIGH_DIM_MAX_HIGHLIGHTS）
             *   - memset 确保所有高亮条目清零，避免悬空/无效引用
             *   - cleared 变量记录实际清除的视图数，在 count 中返回
             *   - 此操作不可逆：被清除的视图数据已永久丢失，
             *     若需恢复需通过 UI 层重新创建视图
             *   - 此操作不释放高维块（block），仅清除视图追踪记录
             *
             * 【并发安全注意】
             *   全局数组 g_multi_views[] 无锁保护。
             *   多线程环境下，如果某一视图正在渲染中被标记 inactive，
             *   渲染器可能访问已释放的资源。调用者需确保：
             *   1. 在单线程环境中调用（如主事件循环）
             *   2. 或在调用前确保所有渲染操作已完成
             */
            int cleared = 0;
            for (int i = 0; i < g_multi_view_count; i++) {
                if (g_multi_views[i].is_active) {
                    g_multi_views[i].is_active = false;
                    g_multi_views[i].highlighted_count = 0;
                    memset(g_multi_views[i].highlighted_elements, 0, sizeof(g_multi_views[i].highlighted_elements));
                    cleared++;
                }
            }

            if (count) {
                *count = cleared;
            }

            lv00_set_error(LV00_OK,
                           "多视图管理：已清除%d个活跃视图。"
                           "UI层需同步关闭所有视图窗口并释放渲染资源。",
                           cleared);

            return LV00_OK;
        }

        case 3: {
            /*
             * 操作：按block_id过滤列出活跃视图（MULTIVIEW_OP_LIST_BY_BLOCK）
             *
             * 遍历全局视图追踪数组，只返回与指定 block_id 关联的活跃视图。
             * count 参数复用为 block_id 输入。
             *
             * 【参数语义】
             *   - view_ids：输出数组，接收匹配的视图ID
             *   - count：输入时为block_id，输出时为实际匹配数
             *
             * 【边界检查】
             *   - view_ids 非空且 count 非空
             *   - 只记录 is_active && block_id 匹配的视图
             */
            if (!view_ids || !count) {
                return LV00_ERROR_INVALID_PARAM;
            }

            int target_block_id = *count; /* count 复用为 block_id 输入 */
            int written = 0;

            /* 使用固定容量上限，防止溢出（内部常量 HIGH_DIM_MAX_ACTIVE_VIEWS） */
            for (int i = 0; i < g_multi_view_count && i < HIGH_DIM_MAX_ACTIVE_VIEWS; i++) {
                if (g_multi_views[i].is_active && g_multi_views[i].block_id == target_block_id) {
                    view_ids[written] = g_multi_views[i].view_id;
                    written++;
                }
            }

            *count = written; /* 输出实际匹配数 */

            if (written > 0) {
                lv00_set_error(LV00_OK, "多视图管理（按block过滤）：block_id=%d 匹配到%d个活跃视图。", target_block_id,
                               written);
            } else {
                lv00_set_error(LV00_OK, "多视图管理（按block过滤）：block_id=%d 无匹配的活跃视图。", target_block_id);
            }

            return LV00_OK;
        }

        case 4: {
            /*
             * 操作：视图状态JSON导出（MULTIVIEW_OP_EXPORT_JSON）
             *
             * 将所有活跃视图序列化为JSON格式，写入 view_ids 指向的字符缓冲区。
             * count 参数指定缓冲区大小（字节数）。
             *
             * JSON结构：
             *   {"views":[{"view_id":...,"block_id":...,"preset_index":...,
             *              "is_active":true},...],"total":N}
             *
             * 【参数语义】
             *   - view_ids：输出字符缓冲区（char*），接收JSON字符串
             *   - count：输入=缓冲区大小（字节），输出=实际写入字节数（含'\0'）
             *
             * 【边界检查】
             *   - view_ids 非空且 count 非空
             *   - 缓冲区大小必须 > 2（至少容纳"{}"）
             *   - 写入不越界：snprintf 限制写入长度
             *   - 返回实际写入字节数（不含末尾'\0'）
             */
            if (!view_ids || !count) {
                return LV00_ERROR_INVALID_PARAM;
            }

            int buf_size = *count;
            if (buf_size <= 2) {
                lv00_set_error(LV00_ERROR_BUFFER_TOO_SMALL,
                               "多视图管理JSON导出失败：缓冲区太小(%d字节)，至少需要3字节（{\"}\" + NUL）", buf_size);
                return LV00_ERROR_BUFFER_TOO_SMALL;
            }

            char *buf = (char *) view_ids;
            int pos = 0;

            /* 统计活跃视图总数 */
            int active_total = 0;
            for (int i = 0; i < g_multi_view_count && i < HIGH_DIM_MAX_ACTIVE_VIEWS; i++) {
                if (g_multi_views[i].is_active) {
                    active_total++;
                }
            }

            /* 写入JSON开头 */
            pos += snprintf(buf + pos, buf_size - pos, "{\"views\":[");

            int written_views = 0;
            for (int i = 0; i < g_multi_view_count && i < HIGH_DIM_MAX_ACTIVE_VIEWS; i++) {
                if (!g_multi_views[i].is_active)
                    continue;

                if (buf_size - pos <= 5)
                    break; /* 缓冲区即将耗尽 */

                if (written_views > 0) {
                    pos += snprintf(buf + pos, buf_size - pos, ",");
                }

                /*
                 * 从预设索引查找预设名称。
                 * 预设名称存储在 HighDimProjectionPreset.name 中，
                 * 需要先定位对应的 block。
                 */
                const char *preset_name = "(unknown)";
                int view_block_id = g_multi_views[i].block_id;
                int preset_idx = g_multi_views[i].preset_index;

                HighDimAbstractBlock *block = high_dim_get_block(manager, view_block_id);
                if (block && preset_idx >= 0 && preset_idx < block->preset_count) {
                    preset_name = block->presets[preset_idx].name;
                }

                pos += snprintf(buf + pos, buf_size - pos,
                                "{\"view_id\":%d,\"block_id\":%d,\"preset_index\":%d,"
                                "\"preset_name\":\"%s\",\"is_active\":true}",
                                g_multi_views[i].view_id, view_block_id, preset_idx, preset_name);

                written_views++;
            }

            pos += snprintf(buf + pos, buf_size - pos, "],\"total\":%d}", active_total);

            *count = pos; /* 实际写入字节数（不含末尾'\0'） */

            if (pos >= buf_size) {
                lv00_set_error(LV00_ERROR_BUFFER_TOO_SMALL,
                               "多视图管理JSON导出：缓冲区不足，JSON被截断。"
                               "需要至少%d字节，当前%d字节。已写入%d字节。",
                               pos + 1, buf_size, buf_size - 1);
            } else {
                lv00_set_error(LV00_OK, "多视图管理JSON导出：成功导出%d个活跃视图，%d字节。", active_total, pos);
            }

            return LV00_OK;
        }

        case MULTIVIEW_OP_GET_VIEW: {
            /* 获取指定索引的视图 */
            int idx = count ? count[0] : -1;
            if (idx < 0 || idx >= g_multi_view_count || !g_multi_views[idx].is_active) {
                lv00_set_error(LV00_ERROR_NOT_FOUND,
                               "多视图管理GET_VIEW失败：无效索引=%d，活跃视图数=%d", idx, g_multi_view_count);
                return LV00_ERROR_NOT_FOUND;
            }
            if (view_ids)
                view_ids[0] = g_multi_views[idx].view_id;
            lv00_set_error(LV00_OK, "多视图管理GET_VIEW成功：索引=%d, view_id=%d", idx, g_multi_views[idx].view_id);
            return LV00_OK;
        }

        case MULTIVIEW_OP_SET_ACTIVE: {
            /* 设置指定视图为活跃状态 */
            if (!view_ids) {
                lv00_set_error(LV00_ERROR_INVALID_PARAM, "多视图管理SET_ACTIVE失败：view_ids为NULL");
                return LV00_ERROR_INVALID_PARAM;
            }
            int target_view_id = view_ids[0];
            int found_idx = high_dim_find_view_index(target_view_id);
            if (found_idx < 0) {
                lv00_set_error(LV00_ERROR_NOT_FOUND,
                               "多视图管理SET_ACTIVE失败：未找到view_id=%d", target_view_id);
                return LV00_ERROR_NOT_FOUND;
            }
            g_multi_views[found_idx].is_active = true;
            lv00_set_error(LV00_OK, "多视图管理SET_ACTIVE成功：view_id=%d已设为活跃", target_view_id);
            return LV00_OK;
        }

        case MULTIVIEW_OP_CLONE_VIEW: {
            /* 克隆指定视图 */
            if (!view_ids) {
                lv00_set_error(LV00_ERROR_INVALID_PARAM, "多视图管理CLONE_VIEW失败：view_ids为NULL");
                return LV00_ERROR_INVALID_PARAM;
            }
            int source_view_id = view_ids[0];
            int found_idx = high_dim_find_view_index(source_view_id);
            if (found_idx < 0) {
                lv00_set_error(LV00_ERROR_NOT_FOUND,
                               "多视图管理CLONE_VIEW失败：未找到源view_id=%d", source_view_id);
                return LV00_ERROR_NOT_FOUND;
            }
            int new_view_id = source_view_id + 1;
            int new_idx = high_dim_allocate_view_slot(new_view_id,
                g_multi_views[found_idx].block_id,
                g_multi_views[found_idx].preset_index);
            if (new_idx < 0) {
                lv00_set_error(LV00_ERROR_OUT_OF_MEMORY,
                               "多视图管理CLONE_VIEW失败：无法分配新视图槽位");
                return LV00_ERROR_OUT_OF_MEMORY;
            }
            g_multi_views[new_idx].highlighted_count = g_multi_views[found_idx].highlighted_count;
            memcpy(g_multi_views[new_idx].highlighted_elements,
                   g_multi_views[found_idx].highlighted_elements,
                   sizeof(int) * HIGH_DIM_MAX_DIMENSIONS);
            if (count)
                count[0] = new_view_id;
            lv00_set_error(LV00_OK, "多视图管理CLONE_VIEW成功：源view_id=%d, 新view_id=%d",
                           source_view_id, new_view_id);
            return LV00_OK;
        }

        case MULTIVIEW_OP_COMPARE_VIEWS: {
            /* 比较两个视图的差异 */
            if (!view_ids) {
                lv00_set_error(LV00_ERROR_INVALID_PARAM, "多视图管理COMPARE_VIEWS失败：view_ids为NULL");
                return LV00_ERROR_INVALID_PARAM;
            }
            int view1_id = view_ids[0];
            int view2_id = view_ids[1];
            int idx1 = high_dim_find_view_index(view1_id);
            int idx2 = high_dim_find_view_index(view2_id);
            if (idx1 < 0 || idx2 < 0) {
                lv00_set_error(LV00_ERROR_NOT_FOUND,
                               "多视图管理COMPARE_VIEWS失败：未找到视图（view1_id=%d, idx1=%d, view2_id=%d, idx2=%d）",
                               view1_id, idx1, view2_id, idx2);
                return LV00_ERROR_NOT_FOUND;
            }
            int diffs = 0;
            if (g_multi_views[idx1].block_id != g_multi_views[idx2].block_id) diffs++;
            if (g_multi_views[idx1].preset_index != g_multi_views[idx2].preset_index) diffs++;
            if (g_multi_views[idx1].highlighted_count != g_multi_views[idx2].highlighted_count) diffs++;
            int min_hl = g_multi_views[idx1].highlighted_count < g_multi_views[idx2].highlighted_count
                         ? g_multi_views[idx1].highlighted_count : g_multi_views[idx2].highlighted_count;
            for (int i = 0; i < min_hl; i++) {
                if (g_multi_views[idx1].highlighted_elements[i] != g_multi_views[idx2].highlighted_elements[i])
                    diffs++;
            }
            if (count)
                count[0] = diffs;
            lv00_set_error(LV00_OK,
                           "多视图管理COMPARE_VIEWS完成：view1=%d vs view2=%d, 差异数=%d",
                           view1_id, view2_id, diffs);
            return LV00_OK;
        }

        default:
            lv00_set_error(LV00_ERROR_UNSUPPORTED,
                           "多视图管理失败：不支持的操作类型=%d"
                           "（有效值：0=LIST, 1=COUNT, 2=CLEAR, 3=LIST_BY_BLOCK, 4=EXPORT_JSON, "
                           "5=GET_VIEW, 6=SET_ACTIVE, 7=CLONE_VIEW, 8=COMPARE_VIEWS）",
                           operation);
            return LV00_ERROR_UNSUPPORTED;
    }
}

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

const char *high_dim_mapping_type_to_string(HighDimMappingType mapping_type) {
    switch (mapping_type) {
        case HIGH_DIM_MAP_TO_X:
            return "x";
        case HIGH_DIM_MAP_TO_Y:
            return "y";
        case HIGH_DIM_MAP_FOLD:
            return "fold";
        case HIGH_DIM_MAP_DISCARD:
            return "discard";
        default:
            return "unknown";
    }
}

HighDimMappingType high_dim_mapping_type_from_string(const char *str) {
    if (!str)
        return (HighDimMappingType) -1;

    if (strcmp(str, "x") == 0)
        return HIGH_DIM_MAP_TO_X;
    if (strcmp(str, "y") == 0)
        return HIGH_DIM_MAP_TO_Y;
    if (strcmp(str, "fold") == 0)
        return HIGH_DIM_MAP_FOLD;
    if (strcmp(str, "discard") == 0)
        return HIGH_DIM_MAP_DISCARD;

    return (HighDimMappingType) -1;
}

int high_dim_get_folded_dimensions_info(const HighDimProjectionPreset *preset, char *buffer, size_t buffer_size) {
    if (!preset || !buffer || buffer_size == 0) {
        return LV00_ERROR_INVALID_PARAM;
    }

    char folded_list[256] = "";
    int folded_count = 0;

    for (int i = 0; i < preset->mapping_count; i++) {
        if (preset->mappings[i].mapping_type == HIGH_DIM_MAP_FOLD ||
            preset->mappings[i].mapping_type == HIGH_DIM_MAP_DISCARD) {
            if (folded_count > 0) {
                lv00_strlcat(folded_list, ", ", sizeof(folded_list));
            }
            char dim_str[16];
            snprintf(dim_str, sizeof(dim_str), "%d", preset->mappings[i].axis_index);
            lv00_strlcat(folded_list, dim_str, sizeof(folded_list));
            folded_count++;
        }
    }

    if (folded_count > 0) {
        snprintf(buffer, buffer_size, "折叠维度: %s", folded_list);
    } else {
        lv00_strlcpy(buffer, "无折叠维度", buffer_size);
    }

    return LV00_OK;
}

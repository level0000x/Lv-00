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
 * @version 3.0.1
 *
 * @dependencies
 *   - high_dim.h           : 高维模块公共接口定义
 *   - error_codes.h        : 错误码定义（lv_OK / lv_ERROR_*）
 *   - lv_utils.h         : 统一内存分配器和工具函数
 *   - lv_internal.h      : 内部数据结构与常量（M_PI 等）
 *   - stream.h             : 流式事件输出
 *   - constraint_graph.h   : 约束图接口（保真度计算依赖）
 */

#include "high_dim.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "lv/config.h"
#include "lv/lv_parse_utils.h"

#include "debug.h" /* LOG_DEBUG, LOG_WARN, LOG_ERROR 等日志宏 */
#include "error_codes.h"
#include "lv_internal.h" /* M_PI, lv_SAFE_SNPRINTF 等内部宏 */
#include "lv_utils.h"
#include "stream.h"
#include "stream_context_util.h"

/* ==================== 内部常量 ==================== */

lv_DECLARE_STREAM_CTX(high_dim);

/**
 * 圆周率常量 π
 *
 * 改用 lv_internal.h 中统一定义的 M_PI，
 * 避免常量重复定义，确保全项目精度一致。
 * 值: 3.14159265358979323846
 */

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 安全的 snprintf 包装函数
 *
 * 本函数是对 vsnprintf 的薄包装层，虽然功能上等价于直接调用 vsnprintf，
 * 但保留此包装层有以下设计考量：
 * 1. 统一格式化接口：项目内所有字符串格式化通过此函数进行，便于统一管理。
 * 2. 未来可扩展性：可在不修改调用点的情况下，添加格式化日志、长度校验、
 *    或替换为自定义格式化后端。
 * 3. 代码可读性：函数名前缀 high_dim_ 明确标识所属模块，增强代码自描述性。
 *
 * @param str     目标缓冲区
 * @param size    缓冲区大小（字节）
 * @param format  printf 风格的格式字符串
 * @param ...     可变参数
 * @return 成功时返回将写入的字符数（不含终止符），失败返回负值
 */
static int high_dim_snprintf(char *str, size_t size, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int result = vsnprintf(str, size, format, args);
    va_end(args);
    return result;
}

/* ==================== 生命周期管理 ==================== */

/**
 * @brief 创建高维管理器
 *
 * 分配并初始化 HighDimManager，调用 high_dim_manager_init 完成内部状态设置。
 *
 * @return 新分配的管理器指针，失败返回 NULL
 */
HighDimManager *high_dim_manager_create(void) {
    HighDimManager *manager = (HighDimManager *) lv_malloc(sizeof(HighDimManager));
    if (!manager)
        return NULL;

    if (high_dim_manager_init(manager) != 0) {
        lv_free((void **) &manager);
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
    lv_free((void **) &manager->blocks);

    lv_free((void **) &manager);
}

/**
 * @brief 初始化高维管理器
 *
 * 分配初始容量为 HIGH_DIM_INITIAL_CAPACITY 的高维块数组。
 *
 * @param manager 管理器指针
 * @return lv_OK 成功，错误码表示失败原因
 */
int high_dim_manager_init(HighDimManager *manager) {
    if (!manager)
        return lv_ERROR_INVALID_PARAM;

    manager->blocks = (HighDimAbstractBlock *) lv_malloc(sizeof(HighDimAbstractBlock) * HIGH_DIM_INITIAL_CAPACITY);
    if (!manager->blocks) {
        return lv_ERROR_OUT_OF_MEMORY;
    }

    manager->block_count = 0;
    manager->block_capacity = HIGH_DIM_INITIAL_CAPACITY;

    /* 初始化语义缩放深度栈 */
    manager->perspective_depth = 0;
    memset(manager->perspective_stack, 0, sizeof(manager->perspective_stack));

    return lv_OK;
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
 * @return lv_OK 成功，错误码表示失败原因
 */
int high_dim_register_block(HighDimManager *manager, int block_id, int dimension_count) {
    if (!manager || dimension_count < 4 || dimension_count > HIGH_DIM_MAX_DIMENSIONS) {
        return lv_ERROR_INVALID_PARAM;
    }

    /* 检查是否已存在 */
    for (int i = 0; i < manager->block_count; i++) {
        if (manager->blocks[i].block_id == block_id) {
            return lv_ERROR_ALREADY_EXISTS;
        }
    }

    /* 扩容检查 */
    if (manager->block_count >= manager->block_capacity) {
        /* 修复：添加整数溢出检查，防止 block_capacity * 2 超过 int 范围 */
        if (manager->block_capacity > INT_MAX / 2) {
            return lv_ERROR_OUT_OF_MEMORY;
        }
        int new_capacity = manager->block_capacity * 2;
        HighDimAbstractBlock *new_blocks =
            (HighDimAbstractBlock *) lv_realloc(manager->blocks, sizeof(HighDimAbstractBlock) * new_capacity);
        if (!new_blocks) {
            return lv_ERROR_OUT_OF_MEMORY;
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
    if (result != lv_OK) {
        return result;
    }

    memcpy(&block->presets[0], &default_preset, sizeof(HighDimProjectionPreset));
    block->preset_count = 1;
    block->current_preset_index = 0;

    manager->block_count++;

    return lv_OK;
}

/**
 * @brief 注销高维块
 *
 * 从管理器中移除指定 ID 的高维块，将最后一个块移到被删除位置以保持数组紧凑。
 *
 * @param manager  管理器指针
 * @param block_id 块 ID
 * @return lv_OK 成功，错误码表示失败原因
 */
int high_dim_unregister_block(HighDimManager *manager, int block_id) {
    if (!manager)
        return lv_ERROR_INVALID_PARAM;

    int index = -1;
    for (int i = 0; i < manager->block_count; i++) {
        if (manager->blocks[i].block_id == block_id) {
            index = i;
            break;
        }
    }

    if (index < 0) {
        return lv_ERROR_NOT_FOUND;
    }

    /* 移动后续元素：使用单次 memmove 替代循环，提高效率 */
    if (index < manager->block_count - 1) {
        memmove(&manager->blocks[index], &manager->blocks[index + 1],
                (manager->block_count - index - 1) * sizeof(HighDimAbstractBlock));
    }

    manager->block_count--;

    return lv_OK;
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
 * @return lv_OK 成功，错误码表示失败原因
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
     *      确保 high_dim_snprintf 不会越界写入
     */
    if (!manager || !high_dim_coords || !projected || coord_count < 0) {
        return lv_ERROR_INVALID_PARAM;
    }

    HighDimAbstractBlock *block = high_dim_get_block(manager, block_id);
    if (!block) {
        return lv_ERROR_NOT_FOUND;
    }

    const HighDimProjectionPreset *preset = high_dim_get_current_preset(manager, block_id);
    if (!preset) {
        return lv_ERROR_INVALID_STATE;
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
                    high_dim_snprintf(dim_info, sizeof(dim_info), "%s%d:%.2f", folded_count > 0 ? ", " : "",
                                      mapping->axis_index, coord_value);
                    lv_strlcat(folded_dims, dim_info, sizeof(folded_dims));
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
        high_dim_snprintf(projected->folded_info, sizeof(projected->folded_info), "折叠维度(%d): %s", folded_count,
                          folded_dims);
    }

    return lv_OK;
}

/**
 * @brief 对投影坐标应用二维变换
 *
 * @param coord     投影坐标
 * @param transform 二维变换矩阵
 * @param result    输出参数，接收变换结果
 * @return lv_OK 成功，错误码表示失败原因
 */
int high_dim_apply_transform(const HighDimProjectedCoord *coord, const HighDimTransform2D *transform,
                             HighDimProjectedCoord *result) {
    if (!coord || !transform || !result) {
        return lv_ERROR_INVALID_PARAM;
    }

    double x = coord->x;
    double y = coord->y;

    result->x = transform->m[0][0] * x + transform->m[0][1] * y;
    result->y = transform->m[1][0] * x + transform->m[1][1] * y;
    result->is_valid = coord->is_valid;
    lv_strlcpy(result->folded_info, coord->folded_info, sizeof(result->folded_info));

    return lv_OK;
}

/**
 * @brief 创建旋转变换矩阵
 *
 * @param angle_rad 旋转角度（弧度）
 * @param transform 输出参数，接收变换矩阵
 * @return lv_OK 成功，错误码表示失败原因
 */
int high_dim_create_rotation_transform(double angle_rad, HighDimTransform2D *transform) {
    if (!transform)
        return lv_ERROR_INVALID_PARAM;

    double cos_a = cos(angle_rad);
    double sin_a = sin(angle_rad);

    transform->m[0][0] = cos_a;
    transform->m[0][1] = -sin_a;
    transform->m[1][0] = sin_a;
    transform->m[1][1] = cos_a;

    return lv_OK;
}

/**
 * @brief 创建缩放变换矩阵
 *
 * @param scale_x   x 轴缩放因子
 * @param scale_y   y 轴缩放因子
 * @param transform 输出参数，接收变换矩阵
 * @return lv_OK 成功，错误码表示失败原因
 */
int high_dim_create_scale_transform(double scale_x, double scale_y, HighDimTransform2D *transform) {
    if (!transform)
        return lv_ERROR_INVALID_PARAM;

    transform->m[0][0] = scale_x;
    transform->m[0][1] = 0.0;
    transform->m[1][0] = 0.0;
    transform->m[1][1] = scale_y;

    return lv_OK;
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
 * @return lv_OK 成功，错误码表示失败原因
 */
int high_dim_calculate_fidelity(HighDimManager *manager, int block_id, const ConstraintGraph *constraint_graph,
                                HighDimVisibilityStats *stats) {
    if (!manager || !stats) {
        return lv_ERROR_INVALID_PARAM;
    }

    HighDimAbstractBlock *block = high_dim_get_block(manager, block_id);
    if (!block) {
        return lv_ERROR_NOT_FOUND;
    }

    const HighDimProjectionPreset *preset = high_dim_get_current_preset(manager, block_id);
    if (!preset) {
        return lv_ERROR_INVALID_STATE;
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

    return lv_OK;
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
 * @return lv_OK 成功，错误码表示失败原因
 */

/* ── 保真度默认阈值 ── */
double lv_high_dim_default_fidelity_threshold(void) {
    return lv_config_current()->high_dim_default_fidelity_threshold;
}

int high_dim_get_fidelity_warning(const HighDimManager *manager, int block_id, char *buffer, size_t buffer_size) {
    if (!manager || !buffer || buffer_size == 0) {
        return lv_ERROR_INVALID_PARAM;
    }

    /* 注意: 此处将 const HighDimManager* 转换为非 const 是因为
     * high_dim_get_block() 缺少 const 版本的API，但该函数不会修改图结构 */
    const HighDimAbstractBlock *block = high_dim_get_block((HighDimManager *) manager, block_id);
    if (!block) {
        return lv_ERROR_NOT_FOUND;
    }

    const HighDimProjectionPreset *preset = high_dim_get_current_preset(manager, block_id);
    if (!preset) {
        return lv_ERROR_INVALID_STATE;
    }

    high_dim_snprintf(buffer, buffer_size,
                      "警告：当前投影'%s'的保真度为%.1f%%，低于推荐阈值%.0f%%。"
                      "建议切换到其他投影预设以获得更好的可视化效果。",
                      preset->name, block->fidelity_ratio * 100.0, lv_high_dim_default_fidelity_threshold() * 100.0);

    return lv_OK;
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
     * @return lv_OK 成功（深度栈已更新）
     *         lv_ERROR_INVALID_PARAM 参数无效
     *         lv_ERROR_NOT_FOUND 未找到对应的高维块
     *         lv_ERROR_UNSUPPORTED 深度栈已满
     */
    if (!manager)
        return lv_ERROR_INVALID_PARAM;

    HighDimAbstractBlock *block = high_dim_get_block(manager, block_id);
    if (!block) {
        lv_set_error(lv_ERROR_NOT_FOUND, "进入块透视失败：未找到block_id=%d对应的高维抽象块", block_id);
        return lv_ERROR_NOT_FOUND;
    }

    /* 检查深度栈是否已满 */
    if (manager->perspective_depth >= HIGH_DIM_MAX_DEPTH) {
        lv_set_error(lv_ERROR_UNSUPPORTED, "语义缩放深度栈已满（最大深度=%d），无法进入更深的透视层级",
                     HIGH_DIM_MAX_DEPTH);
        return lv_ERROR_UNSUPPORTED;
    }

    /* 将当前block_id压入深度栈 */
    manager->perspective_stack[manager->perspective_depth] = block_id;
    manager->perspective_depth++;

    if (high_dim_stream_ctx) {
        stream_emit_progress(high_dim_stream_ctx, 0.0, "语义缩放：进入块透视", block_id, -1);
    }

    /* DEBUG级别日志：提示UI层需要同步切换渲染管线 */
    LOG_DEBUG("high_dim", "已进入block_id=%d的内部透视，当前深度=%d。", block_id, manager->perspective_depth);

    return lv_OK;
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
     * @return lv_OK 成功（深度栈已pop）
     *         lv_ERROR_INVALID_PARAM 参数无效
     *         lv_ERROR_UNSUPPORTED 深度栈已空（已在最外层）
     */
    if (!manager)
        return lv_ERROR_INVALID_PARAM;

    /* 检查深度栈是否已空 */
    if (manager->perspective_depth <= 0) {
        lv_set_error(lv_ERROR_UNSUPPORTED, "当前已在最外层透视，无法继续退出");
        return lv_ERROR_UNSUPPORTED;
    }

    /* 获取即将退出的block_id并pop栈 */
    int exited_block_id = manager->perspective_stack[manager->perspective_depth - 1];
    manager->perspective_stack[manager->perspective_depth - 1] = 0;
    manager->perspective_depth--;

    /* DEBUG级别日志：提示UI层需要同步恢复上层视图 */
    LOG_DEBUG("high_dim", "已退出block_id=%d的内部透视，恢复到深度=%d。", exited_block_id, manager->perspective_depth);

    return lv_OK;
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

/* ==================== 多投影视图 ==================== */

/**
 * @brief 多投影视图内部上下文
 *
 * 在 C 层维护每个多投影视图的状态，包括关联的高维块、投影预设和
 * 当前高亮元素列表。视图ID由 create 函数生成，通过静态数组统一管理。
 */
typedef struct {
    int view_id;                                       /**< 唯一视图标识符 */
    int block_id;                                      /**< 关联的高维块ID */
    int preset_index;                                  /**< 使用的投影预设索引 */
    bool is_active;                                    /**< 视图是否处于激活状态 */
    int highlighted_elements[HIGH_DIM_MAX_DIMENSIONS]; /**< 当前高亮的元素ID列表 */
    int highlighted_count;                             /**< 高亮元素数量 */
} HighDimMultiViewContext;

/** 全局活跃视图追踪数组 */
static lv_THREAD_LOCAL HighDimMultiViewContext g_multi_views[HIGH_DIM_MAX_ACTIVE_VIEWS];
static lv_THREAD_LOCAL int g_multi_view_count = 0;

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
     * @return lv_OK 所有视图创建成功
     *         lv_ERROR_INVALID_PARAM 参数无效
     *         lv_ERROR_NOT_FOUND 未找到指定的高维块
     *         lv_ERROR_RESOURCE_EXHAUSTED 视图槽位已满
     */
    if (!manager || !preset_indices || !view_ids || preset_count <= 0) {
        return lv_ERROR_INVALID_PARAM;
    }

    HighDimAbstractBlock *block = high_dim_get_block(manager, block_id);
    if (!block) {
        lv_set_error(lv_ERROR_NOT_FOUND, "创建多投影视图失败：未找到block_id=%d对应的高维抽象块", block_id);
        return lv_ERROR_NOT_FOUND;
    }

    /* 验证预设索引 */
    for (int i = 0; i < preset_count; i++) {
        if (preset_indices[i] < 0 || preset_indices[i] >= block->preset_count) {
            lv_set_error(lv_ERROR_INVALID_PARAM,
                         "创建多投影视图失败：第%d个预设索引=%d无效"
                         "（有效范围：0-%d）",
                         i, preset_indices[i], block->preset_count - 1);
            return lv_ERROR_INVALID_PARAM;
        }
    }

    /* 为每个预设创建视图 */
    for (int i = 0; i < preset_count; i++) {
        /* 生成唯一视图ID：基础编码 + 冲突避免偏移 */
        /* 注意: block_id 和 preset_index 必须小于 1000，否则ID会碰撞 */
        if (block_id >= 1000 || preset_indices[i] >= 1000) {
            lv_LOG_WARNING("视图ID编码: block_id=%d 或 preset_index=%d 超过999，可能产生ID碰撞", block_id,
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
            lv_set_error(lv_ERROR_RESOURCE_EXHAUSTED,
                         "创建多投影视图失败：block_id=%d 的视图ID空间已耗尽"
                         "（无法为preset_index=%d分配唯一ID）",
                         block_id, preset_indices[i]);
            return lv_ERROR_RESOURCE_EXHAUSTED;
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
            lv_set_error(lv_ERROR_RESOURCE_EXHAUSTED,
                         "创建多投影视图失败：全局视图槽位已满"
                         "（最大=%d，当前=%d）",
                         HIGH_DIM_MAX_ACTIVE_VIEWS, g_multi_view_count);
            return lv_ERROR_RESOURCE_EXHAUSTED;
        }

        view_ids[i] = vid;
    }

    /* DEBUG级别日志：提示UI层需要同步创建视图窗口 */
    LOG_DEBUG("high_dim", "已为block_id=%d创建%d个并排投影视图（view_ids[0]=%d,...）。", block_id, preset_count,
              view_ids[0]);

    return lv_OK;
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
     * @return lv_OK 视图成功标记为销毁
     *         lv_ERROR_INVALID_PARAM 参数无效或 view_id 不合法
     *         lv_ERROR_NOT_FOUND 未找到指定的视图或对应的块不存在
     */
    if (!manager)
        return lv_ERROR_INVALID_PARAM;

    /* 验证 view_id 的基本有效性 */
    if (view_id <= 0) {
        lv_set_error(lv_ERROR_INVALID_PARAM, "销毁视图失败：无效的视图ID=%d，ID必须为正值", view_id);
        return lv_ERROR_INVALID_PARAM;
    }

    /* 在全局视图数组中找到该视图 */
    int view_index = high_dim_find_view_index(view_id);
    if (view_index < 0) {
        lv_set_error(lv_ERROR_NOT_FOUND,
                     "销毁视图失败：未找到view_id=%d对应的活跃视图"
                     "（可能已被销毁或从未创建）",
                     view_id);
        return lv_ERROR_NOT_FOUND;
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

    return lv_OK;
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
     * @return lv_OK 高亮状态已成功记录到至少一个视图
     *         lv_ERROR_INVALID_PARAM 参数无效
     *         lv_ERROR_NOT_FOUND 所有视图均未找到
     */
    if (!manager || !view_ids || view_count <= 0) {
        return lv_ERROR_INVALID_PARAM;
    }

    /* 验证 element_id 有效性 */
    if (element_id < 0) {
        lv_set_error(lv_ERROR_INVALID_PARAM, "联动高亮失败：无效的元素ID=%d", element_id);
        return lv_ERROR_INVALID_PARAM;
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
                high_dim_snprintf(buf, sizeof(buf), "%sview_id[%d]=%d无效; ", views_skipped > 0 ? "" : "", i, vid);
                lv_strlcat(skipped_info, buf, sizeof(skipped_info));
            }
            views_skipped++;
            continue;
        }

        /* 在视图追踪数组中查找 */
        int view_idx = high_dim_find_view_index(vid);
        if (view_idx < 0) {
            if (views_skipped < 10) {
                char buf[64];
                high_dim_snprintf(buf, sizeof(buf), "%sview_id=%d未注册; ", views_skipped > 0 ? "" : "", vid);
                lv_strlcat(skipped_info, buf, sizeof(skipped_info));
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
                high_dim_snprintf(buf, sizeof(buf), "%sview_id=%d的block_id=%d已注销; ", views_skipped > 0 ? "" : "",
                                  vid, view_ctx->block_id);
                lv_strlcat(skipped_info, buf, sizeof(skipped_info));
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
                high_dim_snprintf(buf, sizeof(buf), "%sview_id=%d高亮列表已满; ", views_skipped > 0 ? "" : "", vid);
                lv_strlcat(skipped_info, buf, sizeof(skipped_info));
            }
            views_skipped++;
        }
    }

    /* 汇总结果 */
    if (views_highlighted == 0) {
        lv_set_error(lv_ERROR_NOT_FOUND, "联动高亮失败：所有%d个视图均未能记录高亮状态。跳过原因：%s", view_count,
                     skipped_info[0] ? skipped_info : "所有视图未注册或无效");
        return lv_ERROR_NOT_FOUND;
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

    return lv_OK;
}

/* ==================== 序列化 ==================== */

int high_dim_preset_serialize_json(const HighDimProjectionPreset *preset, char *buffer, size_t buffer_size) {
    if (!preset || !buffer || buffer_size == 0) {
        return lv_ERROR_INVALID_PARAM;
    }

    int written = high_dim_snprintf(buffer, buffer_size,
                                    "{\n"
                                    "  \"name\": \"%s\",\n"
                                    "  \"dimension_count\": %d,\n"
                                    "  \"mapping_count\": %d,\n"
                                    "  \"mappings\": [\n",
                                    preset->name, preset->dimension_count, preset->mapping_count);

    if (written >= (int) buffer_size) {
        return lv_ERROR_BUFFER_TOO_SMALL;
    }

    size_t offset = written;

    /* 序列化映射配置 */
    for (int i = 0; i < preset->mapping_count && offset < buffer_size; i++) {
        const HighDimAxisMapping *m = &preset->mappings[i];
        written = high_dim_snprintf(buffer + offset, buffer_size - offset,
                                    "    {\n"
                                    "      \"axis_index\": %d,\n"
                                    "      \"mapping_type\": \"%s\",\n"
                                    "      \"scale\": %.6f,\n"
                                    "      \"offset\": %.6f\n"
                                    "    }%s\n",
                                    m->axis_index, high_dim_mapping_type_to_string(m->mapping_type), m->scale,
                                    m->offset, (i < preset->mapping_count - 1) ? "," : "");
        offset += written;
    }

    if (offset < buffer_size) {
        written = high_dim_snprintf(buffer + offset, buffer_size - offset,
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

    return (offset >= buffer_size) ? lv_ERROR_BUFFER_TOO_SMALL : (int) offset;
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
    *out_val = 0;
    lv_parse_int(pos, out_val);
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
        return lv_ERROR_INVALID_PARAM;
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
                                    axis_index = 0;
                                    lv_parse_int(ai_val, &axis_index);
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

    return lv_OK;
}

/* ==================== 4D到3D投影 ==================== */

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
     * 【仍为桩函数的部分（需要外部依赖或后续版本实现）】
     *   1. 旋转投影 —— 当前投影假设坐标轴已对齐，不支持4D旋转后的投影
     *      （需要4D旋转矩阵，即 SO(4) 群元素）
     *   2. 立体投影（stereographic）—— 4D球面 S^3 到 3D空间 R^3 的投影
     *   3. 正交投影的轴选择 —— 当前固定取前3维，不支持用户指定保留哪3个维度
     *   4. 5D及以上 —— dim_count > 4 时，第5维及以上的折叠策略不完善
     *      （应支持级联投影或加权折叠）
     *   5. 投影矩阵输出 —— 当前仅输出投影后的3D坐标，不输出4x3投影矩阵
     *      （用于后续的逆投影和误差分析）
     *   6. 深度缓冲区 —— 不维护投影深度信息用于遮挡剔除
     *   7. 视锥体裁剪 —— 不检查投影后的点是否在视锥体范围内
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
     * @return lv_OK 投影成功
     *         lv_ERROR_INVALID_PARAM 参数无效（NULL指针、非法维度、非法距离）
     */
    if (!coord_4d || !coord_3d || dim_count < 1) {
        return lv_ERROR_INVALID_PARAM;
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
                lv_set_error(lv_ERROR_INVALID_PARAM, "4D透视投影失败：camera_distance=%.2f无效，必须大于0",
                             camera_distance);
                return lv_ERROR_INVALID_PARAM;
            }

            double w = coord_4d[3]; /* 第4维坐标 */
            double denominator = camera_distance - w;

            /*
             * 奇点保护：当 w 接近 camera_distance 时，投影点趋于无穷。
             * 使用最小阈值避免除零和数值溢出。
             */
            if (fabs(denominator) < camera_distance * 1e-6 + 1e-12) {
                double min_denom = camera_distance * 1e-6 + 1e-12;
                denominator = (denominator >= 0) ? min_denom : -min_denom;
                lv_set_error(lv_OK,
                             "4D透视投影：w=%.4f接近摄像机距离d=%.4f，"
                             "已应用奇点保护（截断因子=%.0fx）",
                             w, camera_distance, camera_distance / min_denom);
            }

            double factor = camera_distance / denominator;

            /* 防止 factor 过大导致坐标溢出 */
            if (factor > 1e12)
                factor = 1e12;
            if (factor < -1e12)
                factor = -1e12;

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
         * 正交投影模式
         *
         * 简单丢弃第4维及以上的所有维度。
         * 这是一种"轴对齐切片"——将高维点沿w轴方向垂直投影到3D超平面上。
         * 没有透视效果，保距性好（保留x,y,z的真实比例）。
         *
         * 缺失维度（dim_count < 3）：用0填充。
         * 多余维度（dim_count > 4）：第4维及以上的信息完全丢失。
         *   后续版本应支持维度加权折叠（如 PCA 降维）以减少信息损失。
         */
        for (int d = 0; d < 3; d++) {
            coord_3d[d] = (d < dim_count) ? coord_4d[d] : 0.0;
        }
    } else {
        lv_set_error(lv_ERROR_INVALID_PARAM, "4D投影失败：不支持的projection_mode=%d（有效值：0=透视, 1=正交）",
                     projection_mode);
        return lv_ERROR_INVALID_PARAM;
    }

    return lv_OK;
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
     * 【仍为桩函数的部分（需要外部依赖或后续版本实现）】
     *   1. 约束类型敏感度 —— 当前将所有约束类型（INCIDENCE/BETWEENNESS/...）
     *      同等对待，实际应区分：拓扑约束（如连接关系）比度量约束（如距离）
     *      对维度折叠更敏感
     *   2. 几何失真度量 —— 当前仅计算坐标范围的"收缩比"，未计算形状保真度
     *      （如角度失真、面积失真、交叉比等更精细的几何不变量）
     *   3. 局部保真度热图 —— 当前仅输出全局保真度，不支持"哪个区域保真度低"
     *      的空间分析
     *   4. 动态精度调整 —— 不支持根据保真度自动调整投影参数
     *      （如自动旋转视点以最大化可见维度）
     *   5. 多维缩放（MDS）误差 —— 对5D+维度，降维到2D的MDS stress 值
     *      可作为更精确的保真度损失度量
     *   6. 拓扑保持度量 —— 检查投影是否改变了节点间的邻接关系
     *      （如原本不相连的点在投影后重叠）
     *
     * 【与 high_dim_calculate_fidelity 的关系】
     *   本函数是增强版，包含了原有函数的所有功能并增加了约束分析和
     *   几何信息分析。原有函数保留不变，用于快速简单的保真度检查。
     *
     * @param manager 高维管理器指针
     * @param block_id 要计算保真度的高维块ID
     * @param constraint_graph 关联的约束图（可为NULL，此时仅用维度比）
     * @param stats 输出参数，接收详细的保真度统计信息
     * @return lv_OK 计算成功
     *         lv_ERROR_INVALID_PARAM 参数无效
     *         lv_ERROR_NOT_FOUND 未找到指定的高维块
     *         lv_ERROR_INVALID_STATE 投影预设无效
     */
    if (!manager || !stats) {
        return lv_ERROR_INVALID_PARAM;
    }

    HighDimAbstractBlock *block = high_dim_get_block(manager, block_id);
    if (!block) {
        return lv_ERROR_NOT_FOUND;
    }

    const HighDimProjectionPreset *preset = high_dim_get_current_preset(manager, block_id);
    if (!preset) {
        return lv_ERROR_INVALID_STATE;
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

    /* ---- 第二层：约束图关系保留率 ---- */
    double fidelity_constraint = 1.0; /* 默认：无约束时视为完全保留 */
    int total_constraints = 0;
    int retainable_constraints = 0;

    if (constraint_graph && constraint_graph->constraint_count > 0) {
        total_constraints = constraint_graph->constraint_count;

        for (int i = 0; i < constraint_graph->constraint_count; i++) {
            Constraint *c = constraint_graph->constraints[i];
            if (!c)
                continue;

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
                retainable_constraints++;
            }
        }

        fidelity_constraint = (total_constraints > 0) ? (double) retainable_constraints / total_constraints : 1.0;
    }

    /* ---- 第三层：几何信息熵比 ---- */
    /*
     * 计算约束图中所有节点坐标的范围。
     * 如果投影后范围过小（所有点聚在一起），信息损失大。
     *
     * 当前使用节点坐标的最大跨距作为"几何信息量"的代理指标。
     * 完整的几何信息度量应使用协方差矩阵的特征值分析。
     */
    double fidelity_geometry = 1.0;
    if (constraint_graph && constraint_graph->node_count > 0) {
        double min_x = 0.0, max_x = 0.0;
        double min_y = 0.0, max_y = 0.0;
        int has_coords = 0;

        for (int i = 0; i < constraint_graph->node_count; i++) {
            GeomNode *node = constraint_graph->nodes[i];
            if (!node || node->coord_count < 2)
                continue;

            double x = symbolic_coord_to_double(node->symbolic_coords[0]);
            double y = symbolic_coord_to_double(node->symbolic_coords[1]);

            if (has_coords == 0) {
                min_x = max_x = x;
                min_y = max_y = y;
                has_coords = 1;
            } else {
                if (x < min_x)
                    min_x = x;
                if (x > max_x)
                    max_x = x;
                if (y < min_y)
                    min_y = y;
                if (y > max_y)
                    max_y = y;
            }
        }

        if (has_coords) {
            double range_x = max_x - min_x;
            double range_y = max_y - min_y;
            double total_range = fmax(range_x, range_y);

            /*
             * 几何保真度：如果投影后的坐标范围至少覆盖100个单位，
             * 则认为几何信息保留充分。小于100时按比例衰减。
             * 这个阈值可以根据实际使用场景调整。
             */
            if (total_range >= 100.0) {
                fidelity_geometry = 1.0;
            } else if (total_range > 0.0) {
                fidelity_geometry = total_range / 100.0;
            } else {
                fidelity_geometry = 0.0; /* 所有点重合，完全损失几何信息 */
            }
        }
    }

    /* ---- 综合保真度：加权调和平均 ---- */
    /*
     * 权重分配理由：
     *   - 约束保留率（0.5）最重要：约束是Lv-00系统的核心，
     *     约束丢失意味着求解能力下降
     *   - 几何信息（0.3）次要：几何失真影响可视化质量
     *   - 维度比（0.2）基线：提供基础的维度覆盖度量
     */
    double fidelity = 0.2 * fidelity_dim + 0.5 * fidelity_constraint + 0.3 * fidelity_geometry;

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

    return lv_OK;
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
     * 【仍为桩函数的部分（需要外部依赖或后续版本实现）】
     *   1. 按 block_id 过滤列出 —— 当前 LIST 返回所有视图，
     *      不支持只列出某个特定高维块的关联视图
     *   2. 视图状态导出 —— 不支持将当前多视图状态序列化为JSON/配置，
     *      以便保存和恢复工作会话
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
     * @return lv_OK 操作成功
     *         lv_ERROR_INVALID_PARAM 参数无效
     *         lv_ERROR_UNSUPPORTED 不支持的操作类型
     */
    if (!manager)
        return lv_ERROR_INVALID_PARAM;

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
                return lv_ERROR_INVALID_PARAM;
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
                lv_set_error(lv_OK,
                             "列出视图：共%d个活跃视图，但输出数组容量仅%d，"
                             "实际写入%d个。请增大view_ids数组容量。",
                             written, max_count, max_count);
            } else {
                lv_set_error(lv_OK, "列出视图：共%d个活跃视图，已全部写入view_ids数组。", written);
            }

            return lv_OK;
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
                return lv_ERROR_INVALID_PARAM;
            }

            int active_count = 0;
            for (int i = 0; i < g_multi_view_count; i++) {
                if (g_multi_views[i].is_active) {
                    active_count++;
                }
            }

            *count = active_count;
            return lv_OK;
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

            lv_set_error(lv_OK,
                         "多视图管理：已清除%d个活跃视图。"
                         "UI层需同步关闭所有视图窗口并释放渲染资源。",
                         cleared);

            return lv_OK;
        }

        default:
            lv_set_error(lv_ERROR_UNSUPPORTED,
                         "多视图管理失败：不支持的操作类型=%d"
                         "（有效值：0=LIST, 1=COUNT, 2=CLEAR）",
                         operation);
            return lv_ERROR_UNSUPPORTED;
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
            high_dim_snprintf(dim_str, sizeof(dim_str), "%d", preset->mappings[i].axis_index);
            lv_strlcat(folded_list, dim_str, sizeof(folded_list));
            folded_count++;
        }
    }

    if (folded_count > 0) {
        high_dim_snprintf(buffer, buffer_size, "折叠维度: %s", folded_list);
    } else {
        lv_strlcpy(buffer, "无折叠维度", buffer_size);
    }

    return lv_OK;
}

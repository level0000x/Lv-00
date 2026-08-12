/**
 * @file preset_topology.c
 * @brief 拓扑学预设函数块 - 实现
 *
 * H1 评估结论（2026-08-07）：注册样板已由 LV_PRESET_REGISTER 宏抽象（每条目 1 行），无需代码生成；运行时注册由 module/presets/topology.lvz 数据驱动（convert_presets.py 生成）。
 *
 * 实现理论数学研究中常用的拓扑学运算预设函数块。
 * 涵盖拓扑空间基础、连续映射、分离公理、紧致性、连通性及基本群。
 *
 * @module Topology
 * @category PRESET_CATEGORY_TOPOLOGY
 * @version 5.0.0
 */

/*
 * ============================================================
 * 头文件包含说明
 * ============================================================
 * preset_topology.h -> preset_blocks.h -> func_block_registry.h
 *   -> 提供 PresetType 枚举、preset_blocks_register_simple() 声明
 *   -> 提供 PresetCategory 枚举（PRESET_CATEGORY_TOPOLOGY 等）
 * preset_common.h
 *   -> 提供 PRESET_REGISTER 等宏、preset_register_common() 内联函数
 *   -> 提供 PRESET_SAFE_MALLOC 等安全内存操作宏
 * lv_internal.h / lv_utils.h
 *   -> 提供 lv_malloc、lv_free、lv_strdup、lv_log_* 等
 * ============================================================
 */
#include "preset_topology.h"

#include <string.h>

#include "lv_internal.h"
#include "lv_utils.h"
#include "preset_blocks.h"
#include "preset_common.h" /* 预设公共宏与辅助函数（PRESET_ERROR_LOG 等） */

/* ==================== 预设函数块数量 ==================== */

/** 拓扑学模块预设函数块总数：49（与头文件中 TOPOLOGY_PRESET_COUNT 一致） */


/**
 * @brief 获取拓扑学预设函数块数量
 */
int preset_topology_count(void) {
    return TOPOLOGY_PRESET_COUNT;
}

/**
 * @brief 获取拓扑学预设名称列表
 */
bool preset_topology_get_names(char ***out_names, int *out_count) {
    static const char *const preset_names[] = {
        PRESET_TOPOLOGY_TEST,
        PRESET_OPEN_SET_TEST,
        PRESET_CLOSED_SET_TEST,
        PRESET_CLOSURE,
        PRESET_INTERIOR,
        PRESET_BOUNDARY,
        PRESET_NEIGHBORHOOD_TEST,
        PRESET_NEIGHBORHOOD_SYSTEM,
        PRESET_BASE_TEST,
        PRESET_TOPOLOGY_FROM_BASE,
        PRESET_CONTINUOUS_MAP_TEST,
        PRESET_HOMEOMORPHISM_TEST,
        PRESET_QUOTIENT_TOPOLOGY,
        PRESET_PRODUCT_TOPOLOGY,
        PRESET_SUBSPACE_TOPOLOGY,
        PRESET_T0_SPACE_TEST,
        PRESET_T1_SPACE_TEST,
        PRESET_T2_SPACE_TEST,
        PRESET_T3_SPACE_TEST,
        PRESET_T4_SPACE_TEST,
        PRESET_COMPACT_SPACE_TEST,
        PRESET_SEQUENTIALLY_COMPACT,
        PRESET_LOCALLY_COMPACT_TEST,
        PRESET_ONE_POINT_COMPACTIFICATION,
        PRESET_CONNECTED_SPACE_TEST,
        PRESET_PATH_CONNECTED_TEST,
        PRESET_CONNECTED_COMPONENT,
        PRESET_PATH_COMPONENT,
        PRESET_LOCALLY_CONNECTED_TEST,
        PRESET_TOTALLY_DISCONNECTED,
        PRESET_HOMOTOPY_TEST,
        PRESET_PATH_HOMOTOPY_TEST,
        PRESET_FUNDAMENTAL_GROUP,
        PRESET_PATH_CLASS_MULTIPLY,
        PRESET_SIMPLY_CONNECTED_TEST,
        PRESET_COVERING_SPACE,
        PRESET_DISCRETE_TOPOLOGY,
        PRESET_TRIVIAL_TOPOLOGY,
        PRESET_METRIC_TOPOLOGY,
        PRESET_ORDER_TOPOLOGY,
        PRESET_OPEN_MAP_TEST,
        PRESET_CLOSED_MAP_TEST,
        PRESET_EMBEDDING_TEST,
        PRESET_SUBBASE_TEST,
        PRESET_OPEN_COVER,
        PRESET_SEPARATION_AXIOMS,
        PRESET_COMPACTIFICATION,
        PRESET_FINITE_SUBCOVER,
        PRESET_LIFTING_EXISTENCE,
    };

    return preset_module_get_names(preset_names,
        (int) (sizeof(preset_names) / sizeof(preset_names[0])), out_names, out_count);
}

PresetCategory preset_topology_category(void) {
    return PRESET_CATEGORY_TOPOLOGY;
}

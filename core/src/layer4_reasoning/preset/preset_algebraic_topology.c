/**
 * @file preset_algebraic_topology.c
 * @brief 代数拓扑预设函数块 - 实现
 *
 * 实现理论数学研究中常用的代数拓扑运算预设函数块。
 * 涵盖同调论、上同调论、基本群推广和单纯复形四大领域。
 * 共23个预设函数块，均遵循模块化、确定性原则。
 *
 * 采用统一的 preset_blocks_register_simple 注册接口。
 *
 * @module AlgebraicTopology
 * @category PRESET_CATEGORY_TOPOLOGY
 * @version 5.0.0
 * @author Lv-00 开发团队
 */

#include "lv/preset_algebraic_topology.h"

#include <string.h>

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/preset_blocks.h"
#include "lv/preset_common.h"

/* ==================== 预设函数块数量 ==================== */

/** 代数拓扑模块预设函数块总数（与头文件中 ALGEBRAIC_TOPOLOGY_PRESET_COUNT 一致） */
#define AT_PRESET_COUNT ALGEBRAIC_TOPOLOGY_PRESET_COUNT

/**
 * @brief 获取代数拓扑预设函数块数量
 *
 * @return int 代数拓扑模块预设函数块总数（23）
 */
int preset_algebraic_topology_count(void) {
    return AT_PRESET_COUNT;
}

/**
 * @brief 获取代数拓扑预设的类别
 *
 * @return PresetCategory 预设类别（PRESET_CATEGORY_TOPOLOGY）
 */
PresetCategory preset_algebraic_topology_category(void) {
    return PRESET_CATEGORY_TOPOLOGY;
}

/**
 * @brief 获取代数拓扑预设名称列表
 *
 * @param out_names 输出名称数组（调用者需释放每个元素和数组本身）
 * @param out_count 输出数量
 * @return true 成功
 * @return false 失败
 */
bool preset_algebraic_topology_get_names(char ***out_names, int *out_count) {
    static const char *const preset_names[] = {
        /* 同调论 */
        PRESET_AT_SIMPLICIAL_HOMOLOGY,
        PRESET_AT_SINGULAR_HOMOLOGY,
        PRESET_AT_RELATIVE_HOMOLOGY,
        PRESET_AT_MAYER_VIETORIS,
        PRESET_AT_EXCISION_THEOREM,
        PRESET_AT_CELLULAR_HOMOLOGY,
        PRESET_AT_BETTI_NUMBERS,
        PRESET_AT_HOMOLOGY_EXACT_SEQUENCE,
        /* 上同调论 */
        PRESET_AT_SINGULAR_COHOMOLOGY,
        PRESET_AT_CUP_PRODUCT,
        PRESET_AT_DE_RHAM_COHOMOLOGY,
        PRESET_AT_CAP_PRODUCT,
        PRESET_AT_COHOMOLOGY_RING,
        /* 基本群推广（高阶同伦） */
        PRESET_AT_HIGHER_HOMOTOPY_GROUPS,
        PRESET_AT_RELATIVE_HOMOTOPY,
        PRESET_AT_HUREWICZ_HOMOMORPHISM,
        PRESET_AT_HOMOTOPY_EXACT_SEQUENCE,
        PRESET_AT_WHITEHEAD_THEOREM,
        /* 单纯复形 */
        PRESET_AT_SIMPLICIAL_COMPLEX,
        PRESET_AT_TRIANGULATION,
        PRESET_AT_EULER_CHARACTERISTIC,
        PRESET_AT_BARYCENTRIC_SUBDIVISION,
        PRESET_AT_SIMPLICIAL_APPROX,
    };

    return preset_module_get_names(preset_names,
        (int) (sizeof(preset_names) / sizeof(preset_names[0])), out_names, out_count);
}

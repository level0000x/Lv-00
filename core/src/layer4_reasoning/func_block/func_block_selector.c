/**
 * @file func_block_selector.c
 * @brief 多解选择器实现
 * @details 提供从多个候选解中选择特定解的策略，包括正根/负根选择、
 *          区域内选择（射线法）、最近点选择和自定义选择函数。
 *
 * @author Lv-00 Project
 * @version 3.0.1
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "lv/func_block.h"
#include "lv/geo_utils.h"
#include "lv/lv_numeric.h"
#include "lv/lv_xmacro.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"

/* ============== 内部几何辅助函数 ============== */

/**
 * @brief 判断点是否在区域内（射线法）
 *
 * 从候选点向右发射水平射线，统计与区域边界的交点数。
 * 交点数为奇数则在区域内，偶数则在区域外。
 *
 * @param point   候选点
 * @param region  区域节点
 * @param graph   约束图
 * @return true  点在区域内
 * @return false 点不在区域内或参数无效
 */
static bool point_in_region(GeomNode *point, GeomNode *region, ConstraintGraph *graph) {
    (void) graph;
    if (!point || !region || region->type != GEOM_REGION)
        return false;
    if (point->coord_count < 2 || !point->symbolic_coords)
        return false;
    if (!point->symbolic_coords[0] || !point->symbolic_coords[1])
        return false;
    if (!region->data.region.boundary_segments)
        return false;

    /* 将候选点的符号坐标转换为浮点数用于射线法计算 */
    double px = symbolic_coord_to_double(point->symbolic_coords[0]);
    double py = symbolic_coord_to_double(point->symbolic_coords[1]);

    /* 收敛说明（批次 T F-2）：射线法实现已收敛到共享设施
     * geo_point_in_region_segments（geo_utils），采用 lv_is_zero 防卫跳过
     * 近水平退化边；此处仅保留 GeomNode 输入校验与坐标提取。 */
    return geo_point_in_region_segments(px, py, region->data.region.boundary_segments,
                                        region->data.region.segment_count);
}

/**
 * @brief 计算两个点之间的欧几里得距离（平方）
 *
 * 使用符号坐标的 double 值计算距离平方，避免 sqrt 开销。
 *
 * @param a 第一个点
 * @param b 第二个点
 * @return 距离平方，无法计算时返回 lv_DEFAULT_DISTANCE_SQUARED
 */
static double point_distance(GeomNode *a, GeomNode *b) {
    if (!a || !b || a->coord_count < 2 || b->coord_count < 2)
        return lv_DEFAULT_DISTANCE_SQUARED;
    if (!a->symbolic_coords || !b->symbolic_coords)
        return lv_DEFAULT_DISTANCE_SQUARED;

    /* 提取两个点的二维符号坐标并转换为浮点数 */
    double ax = symbolic_coord_to_double(a->symbolic_coords[0]);
    double ay = symbolic_coord_to_double(a->symbolic_coords[1]);
    double bx = symbolic_coord_to_double(b->symbolic_coords[0]);
    double by = symbolic_coord_to_double(b->symbolic_coords[1]);

    /* 检查坐标转换结果是否有效，纯符号坐标可能返回 NaN */
    if (isnan(ax) || isnan(ay) || isnan(bx) || isnan(by)) {
        return lv_DEFAULT_DISTANCE_SQUARED;
    }

    /* 计算距离平方（避免 sqrt 开销，用于比较大小足够） */
    double dx = ax - bx;
    double dy = ay - by;
    return geo_norm_sq_2d(dx, dy);
}

/* ============== 选择器 API ============== */

/**
 * @brief 创建多解选择器
 *
 * @param type 选择器类型
 * @return 新创建的选择器指针，失败返回 NULL
 */
SolutionSelector *selector_create(SelectorType type) {
    SolutionSelector *sel = lv_calloc(1, sizeof(SolutionSelector));
    if (!sel)
        return NULL;
    sel->type = type;
    sel->reference_node_id = -1;
    sel->custom_func = NULL;
    sel->user_data = NULL;
    sel->graph = NULL;
    return sel;
}

/**
 * @brief 创建带参考节点的选择器
 *
 * @param type              选择器类型
 * @param reference_node_id 参考节点 ID
 * @return 新创建的选择器指针，失败返回 NULL
 */
SolutionSelector *selector_create_with_reference(SelectorType type, int reference_node_id) {
    SolutionSelector *sel = selector_create(type);
    if (!sel)
        return NULL;
    sel->reference_node_id = reference_node_id;
    return sel;
}

/**
 * @brief 创建自定义选择器
 *
 * @param func     自定义选择函数
 * @param user_data 用户数据指针
 * @return 新创建的选择器指针，失败返回 NULL
 */
SolutionSelector *selector_create_custom(SelectorFunction func, void *user_data) {
    if (!func)
        return NULL;
    /* 复用 selector_create 避免初始化代码重复 */
    SolutionSelector *sel = selector_create(SELECTOR_CUSTOM);
    if (!sel)
        return NULL;
    sel->custom_func = func;
    sel->user_data = user_data;
    return sel;
}

/**
 * @brief 销毁选择器
 *
 * 释放选择器内部拥有的 name / solution_values 子字段以及结构体本身。
 *
 * 注意：本函数不会释放 user_data 所指向的内存（含 func_block_copy
 * 经 copy_user_data 深拷贝的副本，按既有约定由调用者管理其生命周期）。
 * 这一约定避免了选择器与调用者之间的内存所有权歧义。
 *
 * 【2026-08 收敛】此前仅释放外壳，func_block_copy 深拷贝的 name /
 * solution_values 无释放路径（泄漏）；现统一由本函数释放，使
 * func_block_copy 的失败路径可统一 goto fail 走 func_block_destroy 清理。
 *
 * @param selector 选择器指针
 */
void selector_destroy(SolutionSelector *selector) {
    if (!selector)
        return;
    lv_free((void **) &selector->name);
    lv_free((void **) &selector->solution_values);
    lv_free((void **) &selector);
}

/**
 * @brief 设置选择器关联的约束图
 *
 * 显式设置 ConstraintGraph 引用，用于区域选择和最近点选择。
 *
 * @param selector 选择器指针
 * @param graph    约束图指针
 */
void selector_set_graph(SolutionSelector *selector, ConstraintGraph *graph) {
    if (!selector)
        return;
    selector->graph = graph;
}

/**
 * @brief 应用选择器从候选解中选择特定解
 *
 * @param selector           选择器
 * @param candidates         候选解数组
 * @param count              候选解数量
 * @param out_selected_index 输出的选中索引
 * @return true  选择成功
 * @return false 选择失败（参数无效或无法找到有效解）
 */

/* ============================================================
 * 选择器类型分发：type → 选择策略函数指针表
 * ============================================================ */

/** 选择策略处理函数指针类型 */
typedef bool (*SelectorApplyFn)(SolutionSelector *selector, GeomNode **candidates, int count,
                                int *out_selected_index);

/* 策略1：选择正根 - 遍历候选解，取第一个 x 坐标大于 0 的解 */
static bool selector_positive_root(SolutionSelector *selector, GeomNode **candidates, int count,
                                   int *out_selected_index) {
    (void) selector;
    for (int i = 0; i < count; i++) {
        if (candidates[i] && candidates[i]->coord_count > 0 && candidates[i]->symbolic_coords &&
            candidates[i]->symbolic_coords[0]) {
            double val = symbolic_coord_to_double(candidates[i]->symbolic_coords[0]);
            /* 检查返回值有效性：纯符号坐标可能无法转换为数值（返回 NaN） */
            if (!isnan(val) && val > 0.0) {
                *out_selected_index = i;
                return true;
            }
        }
    }
    /* 未找到正根，选择失败 */
    return false;
}

/* 策略2：选择负根 - 遍历候选解，取第一个 x 坐标小于 0 的解 */
static bool selector_negative_root(SolutionSelector *selector, GeomNode **candidates, int count,
                                   int *out_selected_index) {
    (void) selector;
    for (int i = 0; i < count; i++) {
        if (candidates[i] && candidates[i]->coord_count > 0 && candidates[i]->symbolic_coords &&
            candidates[i]->symbolic_coords[0]) {
            double val = symbolic_coord_to_double(candidates[i]->symbolic_coords[0]);
            /* 检查返回值有效性：纯符号坐标可能无法转换为数值（返回 NaN） */
            if (!isnan(val) && val < 0.0) {
                *out_selected_index = i;
                return true;
            }
        }
    }
    /* 未找到负根，选择失败 */
    return false;
}

/* 策略3：选择区域内的解 - 使用射线法判断候选点是否在指定区域内 */
static bool selector_in_region(SolutionSelector *selector, GeomNode **candidates, int count,
                               int *out_selected_index) {
    ConstraintGraph *sel_graph = selector->graph;
    GeomNode *region = NULL;
    if (!sel_graph) {
        /* 约束图未设置，配置错误 */
        return false;
    }
    region = graph_get_node(sel_graph, selector->reference_node_id);
    if (!region || region->type != GEOM_REGION) {
        /* 参考节点无效或非区域类型 */
        return false;
    }
    for (int i = 0; i < count; i++) {
        if (candidates[i] && point_in_region(candidates[i], region, sel_graph)) {
            *out_selected_index = i;
            return true;
        }
    }
    /* 未找到区域内的点 */
    return false;
}

/* 策略4：选择最近点 - 遍历候选解，取距离参考点欧几里得距离最小的解 */
static bool selector_nearest_to_point(SolutionSelector *selector, GeomNode **candidates, int count,
                                      int *out_selected_index) {
    ConstraintGraph *sel_graph = selector->graph;
    GeomNode *ref_point = NULL;
    if (!sel_graph) {
        /* 约束图未设置，配置错误 */
        return false;
    }
    ref_point = graph_get_node(sel_graph, selector->reference_node_id);
    if (!ref_point) {
        /* 参考点无效 */
        return false;
    }
    int best_idx = -1;
    double best_dist = lv_DEFAULT_DISTANCE_SQUARED;
    for (int i = 0; i < count; i++) {
        if (!candidates[i])
            continue;
        double dist = point_distance(candidates[i], ref_point);
        if (dist < best_dist) {
            best_dist = dist;
            best_idx = i;
        }
    }
    if (best_idx >= 0) {
        *out_selected_index = best_idx;
        return true;
    }
    /* 无有效候选解 */
    return false;
}

/* 策略5：自定义选择 - 委托给用户提供的回调函数 */
static bool selector_custom(SolutionSelector *selector, GeomNode **candidates, int count,
                            int *out_selected_index) {
    if (selector->custom_func) {
        return selector->custom_func(candidates, count, out_selected_index, selector->user_data);
    }
    return false;
}

/** 选择器类型 → 选择策略函数查找表 */
static const SelectorApplyFn kSelectorApplyHandlers[] = {
    [SELECTOR_POSITIVE_ROOT] = selector_positive_root,
    [SELECTOR_NEGATIVE_ROOT] = selector_negative_root,
    [SELECTOR_IN_REGION] = selector_in_region,
    [SELECTOR_NEAREST_TO_POINT] = selector_nearest_to_point,
    [SELECTOR_CUSTOM] = selector_custom,
};

bool selector_apply(SolutionSelector *selector, GeomNode **candidates, int count, int *out_selected_index) {
    if (!selector || !candidates || count <= 0 || !out_selected_index) {
        return false;
    }

    if (count == 1) {
        *out_selected_index = 0;
        return true;
    }

    /* 按选择器类型分派到对应的选择策略 */
    return LV_DISPATCH(kSelectorApplyHandlers, selector->type, false, selector, candidates, count, out_selected_index);
}

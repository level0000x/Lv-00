/**
 * @file aabb_common.c
 * @brief AABB 树查询结果管理与内部公共工具
 *
 * 从 geo_aabb_tree.c 拆分的模块之一：
 *   - aabb_box.c     包围盒基础操作
 *   - aabb_common.c  查询结果管理与内部公共工具
 *   - aabb_tree_2d.c 2D AABB 树构建与查询
 *   - aabb_tree_3d.c 3D AABB 树构建与查询
 *
 * @version v3.6.0
 */

#include "lv/lv.h"
#include "lv/geo_aabb_tree.h"
#include "aabb_internal.h"

#include <stdlib.h>

#include "lv_utils.h"
/* ========================================================================
 * 第二部分：查询结果管理
 * ======================================================================== */

/**
 * @brief 初始化查询结果
 *
 * 将 ids 置 NULL，count 和 capacity 置 0。
 */
lv_PUBLIC_API void lv_aabb_query_result_init(lvAABBQueryResult *result) {
    if (!result)
        return;
    result->ids = NULL;
    result->count = 0;
    result->capacity = 0;
}

/**
 * @brief 释放查询结果
 *
 * 释放 ids 数组并将结构体重置为初始状态。
 */
lv_PUBLIC_API void lv_aabb_query_result_free(lvAABBQueryResult *result) {
    if (!result)
        return;
    lv_free((void **) &(result->ids));
    result->ids = NULL;
    result->count = 0;
    result->capacity = 0;
}

/**
 * @brief 获取默认 AABB 树配置
 */
lv_PUBLIC_API lvAABBTreeConfig lv_aabb_tree_default_config(void) {
    lvAABBTreeConfig cfg;
    cfg.max_leaf_size = lv_config_get_int(LV_CFG_AABB_DEFAULT_MAX_LEAF_SIZE, AABB_DEFAULT_MAX_LEAF_SIZE);
    cfg.max_depth = lv_config_get_int(LV_CFG_AABB_DEFAULT_MAX_DEPTH, AABB_DEFAULT_MAX_DEPTH);
    cfg.use_sah = (bool)lv_config_get_int(LV_CFG_AABB_DEFAULT_USE_SAH, 1);
    return cfg;
}


/**
 * @brief 向查询结果中添加一个 ID（自动扩容）
 *
 * 使用 2 倍扩容策略，初始容量为 16。
 */
void result_push_back(lvAABBQueryResult *result, int id) {
    if (result->count >= result->capacity) {
        int new_cap = (result->capacity > 0) ? result->capacity * 2 : 16;
        int *new_ids = (int *) lv_realloc(result->ids, (size_t) new_cap * sizeof(int));
        if (!new_ids)
            return;
        result->ids = new_ids;
        result->capacity = new_cap;
    }
    result->ids[result->count++] = id;
}

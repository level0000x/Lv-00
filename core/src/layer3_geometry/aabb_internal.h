#ifndef LV_AABB_INTERNAL_H
#define LV_AABB_INTERNAL_H

#include "lv/geo_aabb_tree.h"

/* ========================================================================
 * AABB 内部共享宏
 * ======================================================================== */

#define AABB_INITIAL_CAPACITY 64
#define AABB_DEFAULT_MAX_LEAF_SIZE 4
#define AABB_DEFAULT_MAX_DEPTH 64
#define AABB_INVALID_NODE (-1)
#define AABB_DEFAULT_USE_SAH true

/* ========================================================================
 * 内部共享函数（aabb_common.c 提供）
 * ======================================================================== */

/** @brief 向查询结果中添加一个 ID（自动扩容） */
void result_push_back(lvAABBQueryResult *result, int id);

#endif /* LV_AABB_INTERNAL_H */

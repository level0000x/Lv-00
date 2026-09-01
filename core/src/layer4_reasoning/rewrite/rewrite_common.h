#ifndef REWRITE_COMMON_H
#define REWRITE_COMMON_H

/**
 * @file rewrite_common.h
 * @brief 重写子模块共享内部头：模式变量类型掩码推导
 *
 * 统一标准匹配器（rewrite_match_search.c）与 VF2 匹配器（rewrite_vf2.c）
 * 中逐字重复的模式变量类型推导逻辑，保持两处类型门控约定一致。
 */

#include "lv/constraint_graph.h"
#include "lv/rewrite.h"
#include "lv/lv_utils.h" /* lv_mask_all（K63/F89 掩码设施） */

/* 几何节点类型数量（GeomType 枚举成员数），用于构造全类型掩码 */
#define REWRITE_GEOM_TYPE_COUNT 6
/* K63/F89：手写 (1u<<N)-1 改调权威 lv_mask_all（nbits<32 时等价） */
#define REWRITE_ALL_GEOM_TYPES_MASK lv_mask_all(REWRITE_GEOM_TYPE_COUNT)

/* 按约束类型与参与者槽位查类型掩码表；越界或零掩码回退全类型 */
unsigned rewrite_participant_type_mask(ConstraintType type, int position);

/* 依据模式约束参与者槽位推导每个模式变量可绑定的 GeomType 集合 */
void rewrite_pattern_var_type_masks(const RewritePattern *pat, unsigned *masks, int var_count);

#endif /* REWRITE_COMMON_H */

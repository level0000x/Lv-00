/**
 * @file euclidean_geometry_internal.h
 * @brief 欧几里得几何拆分模块内部共享声明
 *
 * @details 本文件包含 euclidean_geometry.c 拆分为多个模块后，
 *          需要在各模块间共享的内部函数声明。
 *          这些函数在 euclidean_geometry_helpers.c 中实现，
 *          被其他模块调用时不再使用 static 限定。
 */
#ifndef EUCLIDEAN_GEOMETRY_INTERNAL_H
#define EUCLIDEAN_GEOMETRY_INTERNAL_H

#include "euclidean_geometry.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 模块级常量（各拆分模块共享）
 * ======================================================================== */

/** @brief 点/线/圆注册数组的初始容量 */
#define EUCLID_INITIAL_CAPACITY 8

/** @brief 等价性证明链的默认翻译映射容量 */
#define EUCLID_EQUIV_TRANSLATION_CAPACITY 32

/** @brief 共线性验证的默认浮点容差 */
#define EUCLID_COLLINEARITY_EPSILON 1e-10

/** @brief 线段全等验证的默认百分比容差 */
#define EUCLID_CONGRUENCE_TOLERANCE 1e-8

/**
 * @brief 公理位掩码的分组偏移量
 *
 *   bits 0-7:   IncidenceAxiom   (8 条)
 *   bits 8-11:  OrderAxiom       (4 条)
 *   bits 12-16: CongruenceAxiom  (5 条)
 *   bits 17-19: ParallelAxiom    (3 条)
 *   bits 20-21: ContinuityAxiom  (2 条)
 */
#define EUCLID_INCIDENCE_OFFSET 0
#define EUCLID_ORDER_OFFSET 8
#define EUCLID_CONGRUENCE_OFFSET 12
#define EUCLID_PARALLEL_OFFSET 17
#define EUCLID_CONTINUITY_OFFSET 20

/** @brief 公理启用掩码的默认值 —— 启用全部五大公理组的所有公理 */
#define EUCLID_DEFAULT_AXIOM_MASK 0x003FFFFF

/* -----------------------------------------------------------------------
 * 位掩码与公理辅助
 * ----------------------------------------------------------------------- */

/**
 * @brief 将公理组别和索引转换为位掩码偏移量
 * @param group    公理组别 (0-4)
 * @param axiom_id 公理在组内的索引
 * @return 位掩码偏移量 (0-31)，参数无效返回 -1
 */
int euclidean_axiom_mask_offset(int group, int axiom_id);

/* -----------------------------------------------------------------------
 * 实体注册查询
 * ----------------------------------------------------------------------- */

bool euclidean_point_is_registered(const EuclideanContext *ctx, int point_id);
bool euclidean_line_is_registered(const EuclideanContext *ctx, int line_id);
bool euclidean_circle_is_registered(const EuclideanContext *ctx, int circle_id);

bool euclidean_register_point_id(EuclideanContext *ctx, int point_id);
bool euclidean_register_line_id(EuclideanContext *ctx, int line_id);
bool euclidean_register_circle_id(EuclideanContext *ctx, int circle_id);

/* -----------------------------------------------------------------------
 * 一致性 / 不一致状态管理
 * ----------------------------------------------------------------------- */

bool euclidean_verify_axiom_inconsistency(EuclideanContext *ctx);
void euclidean_set_inconsistency(EuclideanContext *ctx, int source_id, const char *message);
void euclidean_clear_inconsistency(EuclideanContext *ctx);

/* -----------------------------------------------------------------------
 * 等价性证明链翻译映射构建
 * ----------------------------------------------------------------------- */

bool euclidean_build_birkhoff_to_tarski_map(EquivalenceProofChain *chain);
bool euclidean_build_tarski_to_birkhoff_map(EquivalenceProofChain *chain);

#ifdef __cplusplus
}
#endif

#endif /* EUCLIDEAN_GEOMETRY_INTERNAL_H */
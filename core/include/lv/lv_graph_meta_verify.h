/**
 * @file lv_graph_meta_verify.h
 * @brief 图级元验证 API（M7 裁决：L4 版契约头）
 *
 * 图级元验证（completeness/soundness/differential）基于 ConstraintGraph
 * 做三检查，实现于 layer4_reasoning/proof/meta_verify.c。
 *
 * 【M7 分层说明】本模块与 layer8_meta_verify/meta_verify.c（会话级
 * lv_meta_verifier_*，基于 lvSession/lvProofObject 六检查）**语义不同层、
 * 非重复实现**：本模块是约束图级完整性/可靠性检查，L8 是证明会话级
 * 验证器。二者命名均含 meta_verify 属历史命名重叠，函数前缀
 * lv_graph_meta_verify_*（图级）与 lv_meta_verifier_*（会话级）区分，
 * 登记豁免（M7 裁决，见 standard-unification-design.md）。
 */

#ifndef lv_LV_GRAPH_META_VERIFY_H
#define lv_LV_GRAPH_META_VERIFY_H

#include "constraint_graph.h"
#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 验证图完整性：所有 POINT 节点已解析（coord_count > 0）
 *
 * @param graph 约束图（NULL 返回 -1）
 * @return 1 完整；0 存在未解析点；-1 参数/内部错误
 */
lv_PUBLIC_API int lv_graph_meta_verify_completeness(const ConstraintGraph *graph);

/**
 * @brief 验证图可靠性：无冲突/矛盾约束
 *
 * @param graph 约束图（NULL 返回 -1）
 * @return 1 可靠；0 存在冲突；-1 参数/内部错误
 */
lv_PUBLIC_API int lv_graph_meta_verify_soundness(const ConstraintGraph *graph);

/**
 * @brief 差分验证：两图结构一致性
 *
 * @param graph_a 图 A（NULL 返回 -1）
 * @param graph_b 图 B（NULL 返回 -1）
 * @return 1 一致；0 存在差异；-1 参数/内部错误
 */
lv_PUBLIC_API int lv_graph_meta_verify_differential(const ConstraintGraph *graph_a, const ConstraintGraph *graph_b);

#ifdef __cplusplus
}
#endif

#endif /* lv_LV_GRAPH_META_VERIFY_H */

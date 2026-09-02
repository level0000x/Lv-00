#ifndef lv_PROOF_VERSION_INTERNAL_H
#define lv_PROOF_VERSION_INTERNAL_H

/* 向后兼容委派头：公共 API 定义已上移至 proof_version.h。
 * 保留本头文件以便直接包含 proof_version_internal.h 的旧代码继续工作。 */

#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */
#include "proof_version.h"

/* ================================================================
 * 内部辅助：ghost 标记表绑定的证明导航器
 * （由 proof_navigator_create / proof_navigator_destroy 维护，
 *   供 proof_check_ghost_conflicts 进行依赖链冲突检查使用）
 * ================================================================ */

struct ProofNavigator;

/** @brief 绑定当前用于 ghost 依赖链检查的证明导航器（内部辅助） */
lv_PUBLIC_API void proof_ghost_set_navigator(struct ProofNavigator *nav);

/** @brief 若指定导航器正是当前绑定的 ghost 检查导航器，则解除绑定（内部辅助） */
lv_PUBLIC_API void proof_ghost_clear_navigator(struct ProofNavigator *nav);

#endif /* lv_PROOF_VERSION_INTERNAL_H */

/**
 * @file lv_constraint_guard.h
 * @brief 约束参与数守卫 —— 收敛后端编码族（smtlib2 / bdd / groebner）的参与数前置守卫样板
 *
 * 判据：H（维度展开重复项 / 泛化）——「!c || c->participant_count < N」与
 *       「c->participant_count >= N」两类守卫在 3 个后端编码族中出现 25+ 处，
 *       收敛为单一内联判定，消除手写比较的语义歧义（>= 与 == 混用）。
 *
 * 契约卡（ABSTRACTION_SPEC §4.4）：
 * - 语义契约：判定约束非 NULL 且参与者数 >= min；不访问 participants 内容，不做类型判定。
 * - 前置条件：c 可为 NULL（NULL 视为不满足）；min 为任意 int。
 * - 失败/截断语义：纯查询，无失败态，不分配资源，不修改约束。
 * - 边界行为：c == NULL 恒返回 false；min <= 0 且 c 非 NULL 恒返回 true
 *             （participant_count >= 0 恒成立）；参与者数恰为 min 时返回 true（>= 语义）。
 * - 扩展点：无（若未来需要「恰好 N 个参与者」，另立 lv_constraint_has_exactly_participants）。
 */
#ifndef lv_CONSTRAINT_GUARD_H
#define lv_CONSTRAINT_GUARD_H

#include <stdbool.h>

#include "lv/constraint_graph.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 判定约束非 NULL 且参与者数不少于 min */
static inline bool lv_constraint_has_participants(const Constraint *c, int min) {
    return c && c->participant_count >= min;
}

#ifdef __cplusplus
}
#endif

#endif /* lv_CONSTRAINT_GUARD_H */

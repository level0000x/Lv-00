/**
 * @file groebner_engine_guard.h
 * @brief Groebner 引擎句柄/上下文有效性守卫（K4 收敛：判据 A）
 *
 * @details 收敛 groebner_engine*.c 中逐字同构的「句柄/上下文有效性守卫」
 *          样板（registry/ring 上下文有效性 + poly/ideal/variety id 范围检查）。
 *          所有谓词均为纯函数：不设置错误上下文、不加锁、不写寄存器。
 *
 * 契约卡：
 * - 语义契约：groebner_id_in_range 判定 0 <= id < count（闭半开区间，语义对齐
 *   共享设施 lv_index_in_range）；groebner_registry_has_ring 判定 registry 有效
 *   持有 ring_id（registry 非 NULL 且 ring_id 在注册表 ring_count 范围内）。
 * - 前置条件：count 必须非负；registry 可为 NULL（此时判 false，不访问成员）。
 * - 失败/截断语义：无失败路径（谓词恒返回 bool，不设错误）。
 * - 边界行为：id < 0 或 id >= count → false；registry == NULL → false；
 *   空注册表（ring_count == 0）→ 任何 ring_id 均 false。
 * - 扩展点：若共享设施 lv_index_in_range 落地，groebner_id_in_range 可替换为其
 *   inline 等价语义（本头为唯一替换点）。
 */
#ifndef lv_GROEBNER_ENGINE_GUARD_H
#define lv_GROEBNER_ENGINE_GUARD_H

#include <stdbool.h>

#include "lv/groebner_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 谓词：0 <= id < count（等语义于 lv_index_in_range(id, count)） */
static inline bool groebner_id_in_range(int id, int count) {
    return id >= 0 && id < count;
}

/* 谓词：registry 非 NULL 且 ring_id 在注册表 ring_count 范围内 */
static inline bool groebner_registry_has_ring(const lvRingRegistry *registry, int ring_id) {
    return registry != NULL && groebner_id_in_range(ring_id, registry->ring_count);
}

#ifdef __cplusplus
}
#endif

#endif /* lv_GROEBNER_ENGINE_GUARD_H */

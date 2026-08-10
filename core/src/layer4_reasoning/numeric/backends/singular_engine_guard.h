/**
 * @file singular_engine_guard.h
 * @brief Singular 后端注册表/句柄有效性守卫（K4 收敛：判据 A）
 *
 * @details 收敛 singular_backend.c 中散落的「注册表/句柄有效性守卫」
 *          样板（registry 有效性 + ideal id 范围检查）。
 *          所有谓词均为纯函数：不设置错误上下文、不加锁、不写寄存器。
 *
 * 契约卡：
 * - 语义契约：singular_registry_has 判定 registry 非 NULL。
 * - 前置条件：reg 可为 NULL（此时判 false，不访问成员）。
 * - 失败/截断语义：无失败路径（谓词恒返回 bool，不设错误）。
 * - 边界行为：reg == NULL → false。
 * - 扩展点：id 范围检查已收敛至共享设施 lv_index_in_range（lv_numeric.h），
 *   singular_id_in_range 别名已删除，范围守卫直接使用 lv_index_in_range。
 */
#ifndef lv_SINGULAR_ENGINE_GUARD_H
#define lv_SINGULAR_ENGINE_GUARD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 谓词：registry 非 NULL */
static inline bool singular_registry_has(const void *reg) {
    return reg != NULL;
}

#ifdef __cplusplus
}
#endif

#endif /* lv_SINGULAR_ENGINE_GUARD_H */

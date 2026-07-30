/**
 * @file lv_mempool_utils.h
 * @brief 内存池静态单例工具 —— 延迟初始化 + 自动置空清理
 *
 * @details 提供将 lvMemPool 包装为模块内静态单例的辅助函数，
 *          消除各模块中重复的 "static lvMemPool *pool + init_if_null" 模式。
 *
 * @author Lv-00 Project
 */

#ifndef lv_MEMPOOL_UTILS_H
#define lv_MEMPOOL_UTILS_H

#include "lv_mempool.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 获取或创建静态内存池（延迟初始化）
 *
 * 首次调用时通过 lv_mempool_create 创建内存池，后续返回同一实例。
 * 调用者应在模块不再需要池时调用 lv_mempool_static_destroy 释放。
 *
 * @param pool       静态 lvMemPool 指针的指针（必须非 NULL）
 * @param block_size 每个块的大小（字节）
 * @param initial_count 初始预分配块数量
 * @return lvMemPool 指针，失败返回 NULL
 */
lvMemPool *lv_mempool_static_init(lvMemPool **pool, size_t block_size, int initial_count);

/**
 * @brief 销毁静态内存池并置空指针
 *
 * 调用 lv_mempool_destroy 后，将 *pool 置为 NULL，防止悬空引用。
 *
 * @param pool 静态 lvMemPool 指针的指针（NULL 安全）
 */
void lv_mempool_static_destroy(lvMemPool **pool);

#ifdef __cplusplus
}
#endif

#endif /* lv_MEMPOOL_UTILS_H */

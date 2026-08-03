/**
 * @file allocator.h
 * @brief 可替换的内存分配器策略接口
 *
 * 定义 AllocatorOps vtable，支持在运行时切换内存分配策略：
 *   - 原始分配器（标准 malloc/free）
 *   - 调试分配器（带魔数检测和追踪，即当前 lv_malloc 的默认行为）
 *   - 用户自定义分配器
 *
 * 切换分配器是线程安全的（通过互斥锁保护）。
 * 注意：切换分配器时，所有已分配的内存必须使用原分配器释放。
 *
 * @author Lv-00 Project
 * @version 1.0.0
 */

#ifndef lv_LV_ALLOCATOR_H
#define lv_LV_ALLOCATOR_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 函数指针类型：查询给定指针的底层分配实际可用大小
 *
 * 对应平台 API：
 *   - Windows : _msize
 *   - macOS   : malloc_size
 *   - Linux   : malloc_usable_size
 *
 * @param ptr 已分配的内存指针
 * @return 该指针指向的底层分配的实际可用字节数，失败时返回 0
 */
typedef size_t (*AllocatorSizeQuery)(void *ptr);

/**
 * @brief 可替换的内存分配器虚表
 *
 * 每个函数指针对应一个分配/释放操作。
 * calloc 和 realloc 可为 NULL（此时 lv_calloc/lv_realloc 会使用
 * alloc + memset 或 alloc + memcpy 作为回退）。
 *
 * @note alloc 和 free 必须非 NULL，否则切换分配器时会被拒绝。
 */
typedef struct {
    void *(*alloc)(size_t size);
    void *(*calloc)(size_t count, size_t size);
    void *(*realloc)(void *ptr, size_t new_size);
    void  (*free)(void *ptr);
    const char *name;
    AllocatorSizeQuery size_query;  /**< 查询底层分配的实际可用大小（可为 NULL） */
} AllocatorOps;

/**
 * @brief 设置全局分配器
 * @param ops 分配器虚表指针（alloc 和 free 必须非空，NULL 表示仅获取当前值）
 * @return 切换前的分配器指针（不会返回 NULL）
 *
 * @note 线程安全：内部使用互斥锁保护写操作。
 * @warning 切换分配器时，所有已分配的内存必须使用原分配器释放。
 *          混合使用不同分配器的 alloc/free 会导致未定义行为。
 */
const AllocatorOps *lv_allocator_set(const AllocatorOps *ops);

/**
 * @brief 获取当前分配器
 * @return 当前分配器指针（永远不会返回 NULL）
 *
 * @note 此函数频繁在 lv_malloc/lv_free 等热路径上调用，出于性能考虑
 *       不进行互斥锁保护。由于指针写入仅发生在切换时（罕见操作），
 *      且指针读取在目标平台上是对齐的原子操作，此设计是安全的。
 */
const AllocatorOps *lv_allocator_get(void);

/**
 * @brief 重置为默认分配器（调试分配器，带魔数检测和追踪）
 *
 * @note 线程安全：内部使用互斥锁保护写操作。
 */
void lv_allocator_reset(void);

/**
 * @brief 获取原始分配器（标准 malloc/free）
 * @return 原始分配器实例指针
 *
 * 原始分配器直接透传标准库的 malloc/free/calloc/realloc，
 * 不附加任何魔数检测、内存追踪或毒模式填充。
 */
const AllocatorOps *lv_allocator_raw(void);

/**
 * @brief 获取调试分配器（带魔数检测和追踪）
 * @return 调试分配器实例指针
 *
 * 调试分配器保持当前 lv_malloc/lv_free 的完整行为：
 *   - 魔数头部/尾部（缓冲区溢出检测）
 *   - 分配追踪链表（泄漏检测）
 *   - 毒模式填充（use-after-free 检测）
 *   - 内存统计（MemoryStats）
 *   - 内存限制检查
 */
const AllocatorOps *lv_allocator_debug(void);

#ifdef __cplusplus
}
#endif

#endif /* lv_LV_ALLOCATOR_H */
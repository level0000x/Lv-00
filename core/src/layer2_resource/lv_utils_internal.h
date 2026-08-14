/**
 * @file lv_utils_internal.h
 * @brief lv_utils 内部共享类型/宏/辅助函数声明（从 lv_utils.c 拆分）
 *
 * @details 由 lv_utils.c 与其拆分文件（lv_utils_misc.c 等）共享的
 *          内存分配头结构、模块状态与内存追踪辅助函数。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#ifndef lv_LV_UTILS_INTERNAL_H
#define lv_LV_UTILS_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lv/lv_utils.h"
#include "lv/cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 前向声明 —— AllocHeader 定义于本文件 */
struct AllocHeader;

/**
 * @brief Lv Utils 模块全局状态（线程局部）
 */
typedef struct LvUtilsState {
    MemoryStats memory_stats;
    size_t memory_limit;
    bool poison_enabled;
    struct AllocHeader *tracked_allocs;
    uint64_t random_state;
} LvUtilsState;

/**
 * @brief 内部分配头 —— 存储每次分配的元数据
 *
 * 每个通过 lv_malloc / lv_calloc 分配的内存块前附加此头部，
 * 使得 lv_free 和 lv_realloc 可以精确获取原始分配大小。
 * 内存布局：[AllocHeader | 用户数据 (size 字节) | 尾部魔数 (4 字节)]
 */
typedef struct AllocHeader {
    uint32_t head_magic;            /**< 头部魔数，检测内存损坏和 double-free */
    uint32_t tail_offset;           /**< 尾部魔数相对于 data 的偏移（字节）= size */
    size_t size;                    /**< 用户请求的分配大小（字节） */
    const char *file;               /**< 分配源文件名（NULL 表示未记录） */
    int line;                       /**< 分配源行号（0 表示未记录） */
    struct AllocHeader *track_next; /**< 全局追踪链表中的下一个节点 */
    char data[];                    /**< 柔性数组成员：实际数据起始位置 */
} AllocHeader;

#define ALLOC_HEAD_MAGIC 0xADBEEF01                   /**< 头部魔数（存活标记） */
#define ALLOC_TAIL_MAGIC 0xADBEEF02                   /**< 尾部魔数（缓冲区溢出检测） */
#define ALLOC_POISON 0xDEADBEEF                       /**< 毒模式（use-after-free 检测） */
#define ALLOC_MAGIC_FREED 0xDEADDEAD                  /**< 已释放标记（double-free 检测） */
#define ALLOC_HEADER_SIZE offsetof(AllocHeader, data) /**< 头部大小（不含 data） */
#define ALLOC_TAIL_MAGIC_SIZE sizeof(uint32_t) /**< 尾部魔数大小 */

/** 模块级唯一状态实例（线程局部，在 lv_utils.c 定义） */
extern lv_THREAD_LOCAL LvUtilsState s_utils_state;

/* 内存追踪辅助（在 lv_utils.c 实现，lv_utils_misc.c 共享） */
AllocHeader *get_header(void *ptr);
void track_allocation(AllocHeader *hdr);
bool untrack_allocation(AllocHeader *hdr);
void fill_poison(void *data, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* lv_LV_UTILS_INTERNAL_H */

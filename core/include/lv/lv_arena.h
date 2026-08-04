/**
 * @file lv_arena.h
 * @brief 竞技场分配器 —— 批量分配一次性释放，减少内存碎片化
 *
 * @details 提供竞技场（arena）分配器抽象层，支持批量分配一次性释放的
 *          生命周期管理。适合临时对象的批量分配场景，大幅减少内存碎片。
 *
 *          核心特性：
 *          - 块管理：自动增长块大小（翻倍策略）
 *          - Checkpoint/Rollback：支持标记和回滚
 *          - 线程安全：可选互斥锁保护
 *          - 临时竞技场：线程局部存储的快速分配
 *
 * @author Lv-00 Project
 * @version 1.0.0
 */

#ifndef lv_ARENA_H
#define lv_ARENA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "lv_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

// 竞技场分配器
// 支持批量分配一次性释放，减少内存碎片化
// 线程安全版本和非线程安全版本

typedef struct lvArenaBlock {
    struct lvArenaBlock *next;   // 下一个块
    size_t capacity;             // 块总容量
    size_t used;                 // 已用字节数
    // 数据区域紧随其后
} lvArenaBlock;

// 竞技场标记（用于 checkpoint/rollback）
typedef struct lvArenaMark {
    lvArenaBlock *block;         // 当前块
    size_t offset;               // 当前块中的偏移
} lvArenaMark;

typedef struct lvArena {
    lvArenaBlock *head;          // 当前活跃块
    lvArenaBlock *blocks;        // 所有块链表
    size_t block_size;           // 默认块大小
    size_t total_allocated;      // 总分配字节数
    size_t total_used;           // 总使用字节数
    bool thread_safe;            // 是否线程安全
    lvMutex mutex;               // 互斥锁（仅 thread_safe=true 时使用）
} lvArena;

// ---- API ----

// 创建竞技场
// block_size: 默认块大小（0 = 使用默认 64KB）
// thread_safe: 是否线程安全
lvArena *lv_arena_create(size_t block_size, bool thread_safe);

// 销毁竞技场（释放所有块）
void lv_arena_destroy(lvArena *arena);

// 从竞技场分配内存
// 返回对齐到 8 字节的内存指针
void *lv_arena_alloc(lvArena *arena, size_t size);

// 从竞技场分配并清零内存
void *lv_arena_calloc(lvArena *arena, size_t size);

// 复制字符串到竞技场
char *lv_arena_strdup(lvArena *arena, const char *str);

// 重置竞技场（释放所有块，回到初始状态）
void lv_arena_reset(lvArena *arena);

// 获取当前标记（用于 checkpoint）
lvArenaMark lv_arena_mark(lvArena *arena);

// 回滚到标记（释放标记后分配的所有内存）
void lv_arena_reset_to_mark(lvArena *arena, lvArenaMark mark);

// 查询统计
size_t lv_arena_total_allocated(const lvArena *arena);
size_t lv_arena_total_used(const lvArena *arena);
int lv_arena_block_count(const lvArena *arena);

// 线程安全包装
void lv_arena_lock(lvArena *arena);
void lv_arena_unlock(lvArena *arena);

// 临时竞技场（线程局部，用于快速临时分配）
lvArena *lv_arena_tmp(void);

#ifdef __cplusplus
}
#endif

#endif /* lv_ARENA_H */
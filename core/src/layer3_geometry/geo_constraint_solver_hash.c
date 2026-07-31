/**
 * @file geo_constraint_solver_hash.c
 * @brief 几何约束求解器 —— 哈希表实现（O(1) ID 到索引映射）
 */

#include "geo_constraint_solver_internal.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* 前向声明（内部使用） */
static int id_hash_find(const IdHashTable *table, int id);

/* ========================================================================
 * 第一部分：哈希表实现（O(1) ID 到索引映射）
 * ======================================================================== */

/**
 * @brief 计算 ID 的哈希值
 * @param id 实体/约束 ID
 * @return 哈希值（0 到 ID_HASH_TABLE_SIZE-1）
 */
static inline uint32_t id_hash(int id) {
    /* 使用简单的整数哈希：
     * 对于正 ID：直接取模
     * 对于负 ID：先取绝对值再取模
     * 使用位运算 (&) 代替取模 (%)，要求表大小为 2 的幂次 */
    uint32_t hash = (uint32_t) (id >= 0 ? id : -id);
    /* 混合高位和低位，提高分布均匀性 */
    hash = (hash ^ (hash >> 16)) & (ID_HASH_TABLE_SIZE - 1);
    return hash;
}

/**
 * @brief 初始化哈希表
 * @param table 哈希表指针
 */
void id_hash_init(IdHashTable *table) {
    if (table == NULL)
        return;

    table->count = 0;
    for (int i = 0; i < ID_HASH_TABLE_SIZE; i++) {
        table->entries[i].occupied = false;
        table->entries[i].id = 0;
        table->entries[i].index = -1;
    }
}

/**
 * @brief 向哈希表插入 ID-索引映射
 * @param table 哈希表指针
 * @param id 实体/约束 ID
 * @param index 在数组中的索引
 * @return true 插入成功，false 表已满或 ID 已存在
 */
bool id_hash_insert(IdHashTable *table, int id, int index) {
    if (table == NULL || index < 0)
        return false;

    /* 检查负载因子 */
    if (table->count >= (int) (ID_HASH_TABLE_SIZE * HASH_LOAD_FACTOR_MAX)) {
        return false; /* 哈希表过满 */
    }

    /* 检查 ID 是否已存在 */
    if (id_hash_find(table, id) >= 0) {
        return false; /* ID 已存在 */
    }

    uint32_t hash = id_hash(id);

    /* 线性探测解决冲突 */
    for (int probe = 0; probe < ID_HASH_TABLE_SIZE; probe++) {
        uint32_t idx = (hash + probe) & (ID_HASH_TABLE_SIZE - 1);

        if (!table->entries[idx].occupied) {
            table->entries[idx].id = id;
            table->entries[idx].index = index;
            table->entries[idx].occupied = true;
            table->count++;
            return true;
        }
    }

    return false; /* 哈希表已满（理论上不会发生，因为有负载因子检查） */
}

/**
 * @brief 在哈希表中查找 ID 对应的索引
 *
 * 使用线性探测处理哈希冲突。
 * 重要：当遇到 tombstone（已删除标记）时继续探测，
 * 只有遇到"从未使用"的空槽（id=0 且 index=-1）才返回 -1。
 *
 * @param table 哈希表指针
 * @param id 实体/约束 ID
 * @return 索引（-1 表示未找到）
 */
static int id_hash_find(const IdHashTable *table, int id) {
    if (table == NULL)
        return -1;

    uint32_t hash = id_hash(id);

    /* 线性探测查找 */
    for (int probe = 0; probe < ID_HASH_TABLE_SIZE; probe++) {
        uint32_t idx = (hash + probe) & (ID_HASH_TABLE_SIZE - 1);
        const HashEntry *entry = &table->entries[idx];

        if (!entry->occupied) {
            /* 遇到未占用槽：区分 tombstone 和从未使用的空槽 */
            /* tombstone: id=0 且 index=-1 但 occupied=false */
            /* 从未使用的空槽: id=0 且 index=-1 且 occupied=false */
            /* 由于初始化时所有条目都是 id=0, index=-1, occupied=false， */
            /* tombstone 是从 occupied=true 变为 false 的，所以需要额外判断 */
            /* 简化判断：如果 id=0 且 index=-1，认为是从未使用的空槽 */
            if (entry->id == 0 && entry->index == -1) {
                return -1; /* 从未使用的空槽，说明不存在 */
            }
            /* 否则是 tombstone，继续探测 */
            continue;
        }

        if (entry->id == id) {
            return entry->index;
        }
    }

    return -1; /* 未找到 */
}

/**
 * @brief 从哈希表中删除 ID 映射
 * @param table 哈希表指针
 * @param id 实体/约束 ID
 * @return true 删除成功，false 未找到
 */
bool id_hash_remove(IdHashTable *table, int id) {
    if (table == NULL)
        return false;

    uint32_t hash = id_hash(id);

    /* 线性探测查找并删除 */
    for (int probe = 0; probe < ID_HASH_TABLE_SIZE; probe++) {
        uint32_t idx = (hash + probe) & (ID_HASH_TABLE_SIZE - 1);

        if (!table->entries[idx].occupied) {
            return false; /* 未找到 */
        }

        if (table->entries[idx].id == id) {
            /* 标记为删除（不实际删除，保持探测链完整） */
            table->entries[idx].occupied = false;
            table->entries[idx].id = 0;
            table->entries[idx].index = -1;
            table->count--;
            return true;
        }
    }

    return false;
}

/**
 * @brief 快速查找实体索引（使用哈希表）
 * @param sys_ex 扩展求解器系统
 * @param id 实体 ID
 * @return 索引（-1 表示未找到）
 */
int find_entity_index_fast(const lvSolverSystemEx *sys_ex, int id) {
    if (sys_ex == NULL)
        return -1;
    return id_hash_find(&sys_ex->entity_hash, id);
}

/**
 * @brief 快速查找约束索引（使用哈希表）
 * @param sys_ex 扩展求解器系统
 * @param id 约束 ID
 * @return 索引（-1 表示未找到）
 */
int find_constraint_index_fast(const lvSolverSystemEx *sys_ex, int id) {
    if (sys_ex == NULL)
        return -1;
    return id_hash_find(&sys_ex->constraint_hash, id);
}


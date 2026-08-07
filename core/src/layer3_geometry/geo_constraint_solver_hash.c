/**
 * @file geo_constraint_solver_hash.c
 * @brief 几何约束求解器 —— 哈希表实现（O(1) ID 到索引映射）
 *
 * 收敛说明（lv_hashtable）：IdHashTable 内嵌固定数组（ID_HASH_TABLE_SIZE=512）于
 * lvSolverSystemEx（geo_constraint_solver_internal.h 不可改），无句柄字段且无销毁入口
 * （lv_geo_solver_destroy 直接释放整个结构），无法持有 lvHashtable 句柄，故复用
 * lv_hashtable 的 int 键哈希函数（id_hash 委托 lv_hashtable_int_hash，表内一致）与
 * 负载因子策略（HASH_LOAD_FACTOR_MAX=0.75）。删除语义按 lv_hashtable 的 tombstone
 * 语义修正（见 id_hash_remove 注释）。
 */

#include "geo_constraint_solver_internal.h"

#include "lv/lv_hashtable.h"

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
    /* 统一委托 lv_hashtable 的 int 键哈希（FNV-1a 单步，512 为 2 的幂走掩码） */
    return lv_hashtable_int_hash(id, ID_HASH_TABLE_SIZE);
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
            /* 空槽（index == -1，从未使用）：探测链结束，说明不存在 */
            if (entry->index == -1) {
                return -1;
            }
            /* index == -2：tombstone（已删除槽位），继续探测保持链完整 */
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
            /* tombstone 删除：置 DELETED 标记（index == -2），保持探测链完整。
             * 原实现删除后复位为空槽（index == -1），导致同探测链后续条目在删除后
             * 查找时提前终止而不可见（链断裂缺陷）；lv_hashtable 的 tombstone 语义
             * 修正为：删除槽继续参与探测，同链条目仍可查找到，墓碑槽可被后续插入复用。 */
            table->entries[idx].occupied = false;
            table->entries[idx].id = 0;
            table->entries[idx].index = -2;
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


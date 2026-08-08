/**
 * @file geo_constraint_solver_hash.c
 * @brief 几何约束求解器 —— 哈希表实现（O(1) ID 到索引映射）
 *
 * 收敛说明（lv_hashtable）：IdHashTable 内部持有 lv_hashtable_int 句柄
 * （自动扩容 + tombstone 删除语义），由 lv_geo_solver_create/destroy 通过
 * id_hash_init/id_hash_destroy 创建/释放。API 签名保持不变，仅内部存储
 * 从固定数组（ID_HASH_TABLE_SIZE=512，count>=384 时插入失败回退线性扫描）
 * 收敛为自动扩容哈希表，消除大规模实体/约束下的插入静默失败。
 */

#include "geo_constraint_solver_internal.h"

#include "lv/lv_hashtable.h"

#include <stdint.h>

/* ========================================================================
 * 第一部分：哈希表实现（O(1) ID 到索引映射）
 * ======================================================================== */

/**
 * @brief 初始化哈希表
 * @param table 哈希表指针
 */
void id_hash_init(IdHashTable *table) {
    if (table == NULL)
        return;
    table->ht = lv_hashtable_int_create(ID_HASH_TABLE_SIZE);
}

/**
 * @brief 销毁哈希表并释放内部存储
 * @param table 哈希表指针
 */
void id_hash_destroy(IdHashTable *table) {
    if (table == NULL)
        return;
    lv_hashtable_int_destroy(table->ht);
    table->ht = NULL;
}

/**
 * @brief 向哈希表插入 ID-索引映射
 * @param table 哈希表指针
 * @param id 实体/约束 ID
 * @param index 在数组中的索引
 * @return true 插入成功，false ID 已存在
 */
bool id_hash_insert(IdHashTable *table, int id, int index) {
    if (table == NULL || index < 0)
        return false;
    /* 值存下标+1 避开 NULL（值为 NULL 视同键不存在） */
    return lv_hashtable_int_insert(table->ht, id, (void *) (intptr_t) (index + 1));
}

/**
 * @brief 在哈希表中查找 ID 对应的索引
 * @param table 哈希表指针
 * @param id 实体/约束 ID
 * @return 索引（-1 表示未找到）
 */
static int id_hash_find(const IdHashTable *table, int id) {
    if (table == NULL)
        return -1;
    void *v = lv_hashtable_int_get(table->ht, id);
    if (v == NULL)
        return -1;
    return (int) (intptr_t) v - 1;
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
    return lv_hashtable_int_remove(table->ht, id);
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

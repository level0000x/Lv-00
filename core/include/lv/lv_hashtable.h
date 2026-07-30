/**
 * @file lv_hashtable.h
 * @brief 泛型哈希表抽象层
 *
 * 基于链式哈希的泛型哈希表，支持任意键类型和值类型。
 * 键通过哈希函数和相等比较函数进行管理。
 *
 * @note 哈希函数 lv_hash_bytes 和 lv_hash_string 已在 lv/lv_utils.h 中声明，
 *       本模块直接复用。
 */
#ifndef lv_HASHTABLE_H
#define lv_HASHTABLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lv/lv_utils.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 哈希表节点（内部链表节点） */
typedef struct lvHashNode {
    struct lvHashNode *next;
    uint64_t hash;          /* 完整哈希值 */
    char data[];            /* 键 + 值连续存储 */
} lvHashNode;

/** @brief 哈希函数类型 */
typedef uint64_t (*lvHashFunc)(const void *key, size_t key_len);

/** @brief 键比较函数类型 */
typedef bool (*lvHashEqualFunc)(const void *key_a, size_t len_a,
                                const void *key_b, size_t len_b);

/** @brief 值销毁回调 */
typedef void (*lvHashValueDestroyFunc)(void *value);

/** @brief 哈希表 */
typedef struct {
    lvHashNode **buckets;            /* 桶数组 */
    size_t bucket_count;             /* 桶数量 */
    size_t count;                    /* 元素数量 */
    size_t key_size;                 /* 键大小 */
    size_t value_size;               /* 值大小 */
    lvHashFunc hash_func;            /* 哈希函数 */
    lvHashEqualFunc equal_func;      /* 相等比较函数 */
    lvHashValueDestroyFunc destroy_value; /* 值销毁回调（可选） */
} lvHashTable;

/**
 * @brief 初始化哈希表
 * @param ht          哈希表指针
 * @param key_size    键大小（字节）
 * @param value_size  值大小（字节）
 * @param bucket_count 初始桶数量（0 则使用默认 64）
 * @param hash_func   哈希函数（NULL 则使用默认 lv_hash_bytes）
 * @param equal_func  相等比较函数（NULL 则使用 memcmp）
 * @param destroy     值销毁回调（可选）
 * @return true 成功，false 内存分配失败
 */
bool lv_hashtable_init(lvHashTable *ht, size_t key_size, size_t value_size,
                       size_t bucket_count, lvHashFunc hash_func,
                       lvHashEqualFunc equal_func,
                       lvHashValueDestroyFunc destroy);

/**
 * @brief 销毁哈希表，释放所有资源
 */
void lv_hashtable_destroy(lvHashTable *ht);

/**
 * @brief 插入键值对（若键已存在则覆盖值）
 * @return true 成功，false 内存分配失败
 */
bool lv_hashtable_insert(lvHashTable *ht, const void *key, const void *value);

/**
 * @brief 查找键对应的值
 * @param[out] out_value 输出值缓冲区（可为 NULL，仅用于检查存在性）
 * @return true 找到，false 未找到
 */
bool lv_hashtable_find(const lvHashTable *ht, const void *key,
                       void *out_value);

/**
 * @brief 删除键对应的条目
 * @return true 条目存在并被删除，false 键不存在
 */
bool lv_hashtable_erase(lvHashTable *ht, const void *key);

/**
 * @brief 清空哈希表
 */
void lv_hashtable_clear(lvHashTable *ht);

/**
 * @brief 获取元素数量
 */
size_t lv_hashtable_size(const lvHashTable *ht);

/**
 * @brief 获取桶数组大小
 */
size_t lv_hashtable_bucket_count(const lvHashTable *ht);

#ifdef __cplusplus
}
#endif

#endif /* lv_HASHTABLE_H */

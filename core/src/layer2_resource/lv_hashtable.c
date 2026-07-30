/**
 * @file lv_hashtable.c
 * @brief 泛型哈希表实现
 *
 * 基于链式哈希的泛型哈希表，使用 lv_malloc/lv_free 管理内存。
 * 默认哈希函数为 lv_hash_bytes（FNV-1a），默认比较函数为 memcmp。
 */
#include <string.h>

#include "lv/lv_hashtable.h"

/** @brief 默认桶数量 */
#define lv_DEFAULT_BUCKET_COUNT 64

/**
 * @brief 默认键相等比较函数（memcmp）
 */
static bool default_equal(const void *key_a, size_t len_a,
                          const void *key_b, size_t len_b) {
    if (len_a != len_b)
        return false;
    return memcmp(key_a, key_b, len_a) == 0;
}

/**
 * @brief 计算桶索引
 */
static inline size_t bucket_index(uint64_t hash, size_t bucket_count) {
    return (size_t)(hash % (uint64_t)bucket_count);
}

bool lv_hashtable_init(lvHashTable *ht, size_t key_size, size_t value_size,
                       size_t bucket_count, lvHashFunc hash_func,
                       lvHashEqualFunc equal_func,
                       lvHashValueDestroyFunc destroy) {
    if (!ht || key_size == 0)
        return false;

    ht->key_size = key_size;
    ht->value_size = value_size;
    ht->hash_func = hash_func ? hash_func : lv_hash_bytes;
    ht->equal_func = equal_func ? equal_func : default_equal;
    ht->destroy_value = destroy;
    ht->count = 0;

    if (bucket_count == 0)
        bucket_count = lv_DEFAULT_BUCKET_COUNT;
    ht->bucket_count = bucket_count;

    ht->buckets = (lvHashNode **)lv_calloc(bucket_count, sizeof(lvHashNode *));
    if (!ht->buckets)
        return false;

    return true;
}

void lv_hashtable_destroy(lvHashTable *ht) {
    if (!ht)
        return;

    lv_hashtable_clear(ht);
    lv_free((void **)&ht->buckets);
    memset(ht, 0, sizeof(*ht));
}

bool lv_hashtable_insert(lvHashTable *ht, const void *key, const void *value) {
    if (!ht || !key || !value)
        return false;

    uint64_t hash = ht->hash_func(key, ht->key_size);
    size_t idx = bucket_index(hash, ht->bucket_count);

    /* 查找是否已存在 */
    lvHashNode *node = ht->buckets[idx];
    while (node) {
        if (node->hash == hash &&
            ht->equal_func(key, ht->key_size, node->data, ht->key_size)) {
            /* 键已存在，覆盖值 */
            memcpy(node->data + ht->key_size, value, ht->value_size);
            return true;
        }
        node = node->next;
    }

    /* 创建新节点：data[] 中连续存储 key + value */
    size_t node_size = sizeof(lvHashNode) + ht->key_size + ht->value_size;
    node = (lvHashNode *)lv_malloc(node_size);
    if (!node)
        return false;

    node->hash = hash;
    memcpy(node->data, key, ht->key_size);
    memcpy(node->data + ht->key_size, value, ht->value_size);

    /* 头插法 */
    node->next = ht->buckets[idx];
    ht->buckets[idx] = node;
    ht->count++;

    return true;
}

bool lv_hashtable_find(const lvHashTable *ht, const void *key,
                       void *out_value) {
    if (!ht || !key)
        return false;

    uint64_t hash = ht->hash_func(key, ht->key_size);
    size_t idx = bucket_index(hash, ht->bucket_count);

    lvHashNode *node = ht->buckets[idx];
    while (node) {
        if (node->hash == hash &&
            ht->equal_func(key, ht->key_size, node->data, ht->key_size)) {
            if (out_value)
                memcpy(out_value, node->data + ht->key_size, ht->value_size);
            return true;
        }
        node = node->next;
    }

    return false;
}

bool lv_hashtable_erase(lvHashTable *ht, const void *key) {
    if (!ht || !key)
        return false;

    uint64_t hash = ht->hash_func(key, ht->key_size);
    size_t idx = bucket_index(hash, ht->bucket_count);

    lvHashNode **pp = &ht->buckets[idx];
    while (*pp) {
        if ((*pp)->hash == hash &&
            ht->equal_func(key, ht->key_size, (*pp)->data, ht->key_size)) {
            lvHashNode *victim = *pp;
            *pp = victim->next;

            /* 调用值销毁回调 */
            if (ht->destroy_value)
                ht->destroy_value(victim->data + ht->key_size);

            lv_free((void **)&victim);
            ht->count--;
            return true;
        }
        pp = &(*pp)->next;
    }

    return false;
}

void lv_hashtable_clear(lvHashTable *ht) {
    if (!ht)
        return;

    for (size_t i = 0; i < ht->bucket_count; i++) {
        lvHashNode *node = ht->buckets[i];
        while (node) {
            lvHashNode *next = node->next;

            /* 调用值销毁回调 */
            if (ht->destroy_value)
                ht->destroy_value(node->data + ht->key_size);

            lv_free((void **)&node);
            node = next;
        }
        ht->buckets[i] = NULL;
    }
    ht->count = 0;
}

size_t lv_hashtable_size(const lvHashTable *ht) {
    return ht ? ht->count : 0;
}

size_t lv_hashtable_bucket_count(const lvHashTable *ht) {
    return ht ? ht->bucket_count : 0;
}

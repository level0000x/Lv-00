/**
 * @file lv_bitset.h
 * @brief Lv-00 唯一位图容器 —— uint64 词数组 set/test/clear
 *
 * @details K63/F89 收敛：全库唯一真位图（relation_model.c:549-578 手写 uint8
 *          位图去重）收编到本容器。uint64 词数组按最大元素 ID 动态分配，
 *          无独立上限闸门（容量仅受内存约束）。
 *
 *          所有函数为 static inline（纯工具，无 .c，无外部链接符号），
 *          依赖 lv_calloc/lv_free（lv_utils.h，L2 资源层）。
 *
 *          使用模式（relation_model 语义）：
 *            1. lv_bitset_init(&bs)
 *            2. lv_bitset_reserve(&bs, max_elem)   // 按最大 ID 分配
 *            3. lv_bitset_set(&bs, id) / lv_bitset_test(&bs, id)
 *            4. lv_bitset_destroy(&bs)
 *
 * @version 1.0.0
 */

#ifndef lv_BITSET_H
#define lv_BITSET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "lv/lv_utils.h" /* lv_calloc / lv_free */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 位图容器：uint64 词数组 */
typedef struct lvBitset {
    uint64_t *words;      /**< 词数组（按位索引 id/64） */
    size_t word_count;    /**< 已分配词数 */
} lvBitset;

/** @brief 初始化为空位图（未分配） */
static inline void lv_bitset_init(lvBitset *bs) {
    if (!bs)
        return;
    bs->words = NULL;
    bs->word_count = 0;
}

/**
 * @brief 按最大元素 ID 预留容量（可多次调用扩容，保留已置位）
 *
 * @param bs      位图指针（非 NULL）
 * @param max_id  最大元素 ID（>= 0；分配词数 = max_id/64 + 1）
 * @return true 成功；false 分配失败或 bs 为 NULL
 */
static inline bool lv_bitset_reserve(lvBitset *bs, size_t max_id) {
    if (!bs)
        return false;
    size_t need = max_id / 64 + 1;
    if (bs->word_count >= need && bs->words)
        return true;
    uint64_t *w = (uint64_t *) lv_calloc(need, sizeof(uint64_t));
    if (!w)
        return false;
    if (bs->words && bs->word_count > 0) {
        memcpy(w, bs->words, bs->word_count * sizeof(uint64_t));
        lv_free((void **) &bs->words);
    }
    bs->words = w;
    bs->word_count = need;
    return true;
}

/** @brief 置位 id（越界静默忽略，调用方保证 id 在预留范围内） */
static inline void lv_bitset_set(lvBitset *bs, size_t id) {
    if (!bs || !bs->words || id / 64 >= bs->word_count)
        return;
    bs->words[id / 64] |= (1ULL << (id % 64));
}

/** @brief 测试 id 是否置位（越界返回 false） */
static inline bool lv_bitset_test(const lvBitset *bs, size_t id) {
    if (!bs || !bs->words || id / 64 >= bs->word_count)
        return false;
    return (bs->words[id / 64] & (1ULL << (id % 64))) != 0;
}

/** @brief 清除 id（越界静默忽略） */
static inline void lv_bitset_clear(lvBitset *bs, size_t id) {
    if (!bs || !bs->words || id / 64 >= bs->word_count)
        return;
    bs->words[id / 64] &= ~(1ULL << (id % 64));
}

/** @brief 全部清零（保留容量） */
static inline void lv_bitset_clear_all(lvBitset *bs) {
    if (!bs || !bs->words)
        return;
    memset(bs->words, 0, bs->word_count * sizeof(uint64_t));
}

/** @brief 释放位图（重置为空） */
static inline void lv_bitset_destroy(lvBitset *bs) {
    if (!bs)
        return;
    if (bs->words)
        lv_free((void **) &bs->words);
    bs->words = NULL;
    bs->word_count = 0;
}

#ifdef __cplusplus
}
#endif

#endif /* lv_BITSET_H */

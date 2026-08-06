/**
 * @file hash_history.h
 * @brief 通用哈希历史（uint64 环形缓冲 + 可选 32 位轻量预筛）
 *
 * @details 收敛 normalization.c 的 RewriteHistory 与 rewrite/rewrite_wl.c 的
 *          WLHashHistory 两套同构实现的公共核心：
 *          - 存储：uint64 完整哈希环形缓冲（保留最近 capacity 个哈希）；
 *          - 预筛：可选 32 位轻量哈希环形缓冲（contains 两阶段检查）；
 *          - contains：light 预筛（启用时）→ full 精确确认；
 *          - add：满容量时环形覆盖最旧条目。
 *
 *          布局注意：HashHistory 的字段类型与顺序与 lv/rewrite.h 的
 *          WLHashHistory 逐字段一致（uint64_t* + int + int + uint32_t* +
 *          int + int），因此 rewrite_wl.c 的 wl_history_* 包装层可以直接
 *          将其强转为 HashHistory 复用；环形容量通过 add/init 参数传入，
 *          WL 侧使用 WL_HISTORY_SIZE，RewriteHistory 侧使用其 capacity。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#ifndef lv_HASH_HISTORY_H
#define lv_HASH_HISTORY_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 哈希历史结构（布局与 WLHashHistory 兼容，见文件头注释） */
typedef struct HashHistory {
    uint64_t *full_history;   /**< 完整 64 位哈希环形缓冲 */
    int full_count;           /**< 完整缓冲中的元素数（<= capacity） */
    int full_pos;             /**< 完整缓冲环形写入位置 */
    uint32_t *light_history;  /**< 32 位轻量哈希环形缓冲（NULL 表示禁用预筛） */
    int light_count;          /**< 轻量缓冲中的元素数 */
    int light_pos;            /**< 轻量缓冲环形写入位置 */
} HashHistory;

/**
 * @brief 初始化哈希历史
 *
 * @param hh        哈希历史结构指针（不能为 NULL）
 * @param capacity  环形容量
 * @param use_light 是否启用 32 位轻量预筛缓冲
 */
void hash_history_init(HashHistory *hh, int capacity, bool use_light);

/**
 * @brief 销毁哈希历史，释放缓冲区
 *
 * @param hh 哈希历史结构指针
 */
void hash_history_destroy(HashHistory *hh);

/**
 * @brief 向环形缓冲区推入一个新的 64 位哈希（满容量时覆盖最旧条目）
 *
 * @param hh        哈希历史结构指针
 * @param capacity  环形容量（与 init 传入的一致）
 * @param hash      64 位哈希值
 */
void hash_history_add(HashHistory *hh, int capacity, uint64_t hash);

/**
 * @brief 检查哈希是否已存在于历史中（两阶段：light 预筛 → full 确认）
 *
 * @param hh   哈希历史结构指针
 * @param hash 64 位哈希值
 * @return true 已存在，false 不存在
 */
bool hash_history_contains(const HashHistory *hh, uint64_t hash);

/**
 * @brief 返回历史中的元素数量
 *
 * @param hh 哈希历史结构指针
 * @return 元素数量
 */
int hash_history_count(const HashHistory *hh);

#ifdef __cplusplus
}
#endif

#endif /* lv_HASH_HISTORY_H */

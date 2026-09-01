/**
 * @file lv_hashtable.h
 * @brief 统一哈希表设施（int 键 / int64 键 / string 键 三形态）
 *
 * @details 收敛项目内多处手写哈希（ConstraintGraph 节点/约束索引、
 *          几何约束求解器 IdHashTable、预设库名称哈希、快速空间索引、
 *          表达式规范化合并去重桶），提供三种键形态：
 *            - int 键（lv_hashtable_int_*）：开放寻址（线性探测）+
 *              自动扩容重哈希 + tombstone（墓碑）删除
 *            - int64 键（lv_hashtable_i64_*）：与 int 形态同一套
 *              开放寻址实现（由宏模板实例化），键类型 int64_t，
 *              哈希为 FNV-1a 64 位混入键的 8 字节
 *            - string 键（lv_hashtable_str_*）：分离链式（头插）+
 *              自动扩容重哈希；键副本由表内部持有，值所有权归调用方
 *
 * 所有内存通过 lv_malloc / lv_calloc / lv_free 分配。
 * 值（value）不应为 NULL —— 值为 NULL 视同该键不存在。
 *
 * @author Lv-00 Project
 * @version 1.0.0
 */

#ifndef lv_LV_HASHTABLE_H
#define lv_LV_HASHTABLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */

#ifdef __cplusplus
extern "C" {
#endif

/** 哈希表句柄（不透明结构，定义见 lv_hashtable.c） */
typedef struct lvHashtable lvHashtable;

/** int 键遍历回调 */
typedef void (*lvHashtableIntVisitor)(int key, void *value, void *ctx);
/** int64 键遍历回调 */
typedef void (*lvHashtableI64Visitor)(int64_t key, void *value, void *ctx);
/** string 键遍历回调 */
typedef void (*lvHashtableStrVisitor)(const char *key, void *value, void *ctx);

/* ========================================================================
 * int 键形态
 *
 * 开放寻址（线性探测）+ 自动扩容重哈希：
 *   - 容量恒为 2 的幂，负载因子 0.75，达到阈值后翻倍扩容并重哈希
 *   - 删除采用 tombstone（墓碑）标记：被删槽位标记为 DELETED，
 *     探测链保持完整，删除后同链条目仍可查找到；
 *     墓碑槽位在后续插入中可复用，扩容重哈希时丢弃
 * ======================================================================== */

/** @brief 创建 int 键哈希表
 *  @param initial_capacity 初始容量（<=0 时使用默认值；自动向上取 2 的幂）
 *  @return 哈希表句柄，失败返回 NULL */
lvHashtable *lv_hashtable_int_create(int initial_capacity);

/** @brief 销毁 int 键哈希表并释放内部存储（不释放 value） */
lv_PUBLIC_API void lv_hashtable_int_destroy(lvHashtable *ht);

/** @brief 插入键值对；键已存在时返回 false 不覆盖 */
lv_PUBLIC_API bool lv_hashtable_int_insert(lvHashtable *ht, int key, void *value);

/** @brief 按键查找；未找到返回 NULL */
lv_PUBLIC_API void *lv_hashtable_int_get(const lvHashtable *ht, int key);

/** @brief 键是否存在 */
lv_PUBLIC_API bool lv_hashtable_int_contains(const lvHashtable *ht, int key);

/** @brief 删除键值对；键不存在返回 false */
lv_PUBLIC_API bool lv_hashtable_int_remove(lvHashtable *ht, int key);

/** @brief 当前条目数 */
lv_PUBLIC_API int lv_hashtable_int_count(const lvHashtable *ht);

/** @brief 遍历所有条目（顺序不定）；回调中删除当前条目安全 */
lv_PUBLIC_API void lv_hashtable_int_foreach(lvHashtable *ht, lvHashtableIntVisitor visitor, void *ctx);

/** @brief int 键哈希（FNV-1a 单步：key * lv_FNV_HASH_MULTIPLIER；
 *  容量为 2 的幂时走位掩码，否则取模）。
 *  供依赖固定数组布局、无法持有句柄的调用方（如 ConstraintGraph 内嵌
 *  索引数组）与 lv_hashtable 共享完全一致的哈希。 */
lv_PUBLIC_API unsigned lv_hashtable_int_hash(int key, int capacity);

/* ========================================================================
 * int64（i64）键形态
 *
 * 与 int 形态完全相同的开放寻址（线性探测）+ tombstone 实现
 * （同一宏模板实例化，扩容/重哈希/删除语义严格一致），仅键类型与
 * 哈希函数不同：
 *   - 键类型 int64_t（覆盖 64 位整数键；uint64_t 键可按位模式转换）
 *   - 哈希为 FNV-1a 64 位混入键的 8 字节（lv_fnv1a_hash_int），
 *     连续/相近整数键分布均匀
 * ======================================================================== */

/** @brief 创建 int64 键哈希表
 *  @param initial_capacity 初始容量（<=0 时使用默认值；自动向上取 2 的幂）
 *  @return 哈希表句柄，失败返回 NULL */
lvHashtable *lv_hashtable_i64_create(int initial_capacity);

/** @brief 销毁 int64 键哈希表并释放内部存储（不释放 value） */
lv_PUBLIC_API void lv_hashtable_i64_destroy(lvHashtable *ht);

/** @brief 插入键值对；键已存在时返回 false 不覆盖 */
lv_PUBLIC_API bool lv_hashtable_i64_insert(lvHashtable *ht, int64_t key, void *value);

/** @brief 按键查找；未找到返回 NULL */
lv_PUBLIC_API void *lv_hashtable_i64_get(const lvHashtable *ht, int64_t key);

/** @brief 键是否存在 */
lv_PUBLIC_API bool lv_hashtable_i64_contains(const lvHashtable *ht, int64_t key);

/** @brief 删除键值对；键不存在返回 false */
lv_PUBLIC_API bool lv_hashtable_i64_remove(lvHashtable *ht, int64_t key);

/** @brief 当前条目数 */
lv_PUBLIC_API int lv_hashtable_i64_count(const lvHashtable *ht);

/** @brief 遍历所有条目（顺序不定）；回调中删除当前条目安全 */
lv_PUBLIC_API void lv_hashtable_i64_foreach(lvHashtable *ht, lvHashtableI64Visitor visitor, void *ctx);

/** @brief int64 键哈希（FNV-1a 64 位：以 lv_FNV64_OFFSET_BASIS 为初值混入
 *  键的 8 字节；容量为 2 的幂时走位掩码，否则取模）。
 *  供依赖固定数组布局、无法持有句柄的调用方与 lv_hashtable 共享
 *  完全一致的哈希。 */
lv_PUBLIC_API unsigned lv_hashtable_i64_hash(int64_t key, int capacity);

/* ========================================================================
 * string 键形态
 *
 * 分离链式（头插）+ 自动扩容重哈希：
 *   - 桶数恒为 2 的幂，负载因子 0.75，达到阈值后翻倍并重哈希
 *   - 键副本由表内部持有（调用方传入的 key 会被复制）；
 *     值的所有权始终归调用方，销毁表不会释放 value
 * ======================================================================== */

/** @brief 创建 string 键哈希表
 *  @param initial_bucket_count 初始桶数（<=0 时使用默认值；自动向上取 2 的幂）
 *  @return 哈希表句柄，失败返回 NULL */
lvHashtable *lv_hashtable_str_create(int initial_bucket_count);

/** @brief 销毁 string 键哈希表：释放键副本与内部节点，不释放 value */
lv_PUBLIC_API void lv_hashtable_str_destroy(lvHashtable *ht);

/** @brief 插入键值对（内部复制 key）；键已存在时返回 false 不覆盖 */
lv_PUBLIC_API bool lv_hashtable_str_insert(lvHashtable *ht, const char *key, void *value);

/** @brief 按键查找；未找到返回 NULL */
lv_PUBLIC_API void *lv_hashtable_str_get(const lvHashtable *ht, const char *key);

/** @brief 键是否存在 */
lv_PUBLIC_API bool lv_hashtable_str_contains(const lvHashtable *ht, const char *key);

/** @brief 删除键值对（释放键副本）；键不存在返回 false */
lv_PUBLIC_API bool lv_hashtable_str_remove(lvHashtable *ht, const char *key);

/** @brief 当前条目数 */
lv_PUBLIC_API int lv_hashtable_str_count(const lvHashtable *ht);

/** @brief 遍历所有条目（顺序不定）；
 *  注意：回调中不得删除当前节点（如需删除，先收集后统一处理） */
lv_PUBLIC_API void lv_hashtable_str_foreach(lvHashtable *ht, lvHashtableStrVisitor visitor, void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* lv_LV_HASHTABLE_H */

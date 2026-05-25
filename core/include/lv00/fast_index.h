/**
 * @file fast_index.h
 * @brief 高效索引与检索系统
 *
 * @details 提供高性能数据结构与索引实现：
 *   1. 哈希表（开放寻址 + Robin Hood 哈希）
 *   2. 布隆过滤器（快速存在性检测）
 *   3. 跳表（有序数据快速查找）
 *   4. LRU缓存（热点数据缓存）
 *   5. 空间索引（R树变体，用于几何查询）
 *
 * 设计目标：
 *   - O(1) 平均查找时间
 *   - 低内存碎片
 *   - 高缓存命中率
 *   - 线程安全选项
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#ifndef LV00_FAST_INDEX_H
#define LV00_FAST_INDEX_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ============== 配置常量 ============== */

/** 哈希表默认初始容量 */
#define LV00_HASH_DEFAULT_CAPACITY 64

/** 哈希表最大负载因子（百分比） */
#define LV00_HASH_MAX_LOAD_FACTOR 75

/** 布隆过滤器默认误判率 */
#define LV00_BLOOM_DEFAULT_ERROR_RATE 0.01

/** 跳表最大层数 */
#define LV00_SKIPLIST_MAX_LEVEL 32

/** 跳表默认概率因子 */
#define LV00_SKIPLIST_P 0.25

/** LRU缓存默认容量 */
#define LV00_LRU_DEFAULT_CAPACITY 256

/** R树最大分支数 */
#define LV00_RTREE_MAX_ENTRIES 16

/* ============== 哈希表 ============== */

/**
 * @brief 哈希表条目
 */
typedef struct Lv00HashEntry {
    uint64_t key;           /**< 键 */
    void *value;            /**< 值 */
    uint8_t hash;           /**< 哈希值低8位（用于Robin Hood） */
    bool occupied;          /**< 是否被占用 */
} Lv00HashEntry;

/**
 * @brief 哈希表
 */
typedef struct {
    Lv00HashEntry *entries; /**< 条目数组 */
    size_t capacity;        /**< 容量 */
    size_t count;           /**< 已用条目数 */
    size_t tombstones;      /**< 墓碑数量 */
    bool thread_safe;       /**< 是否线程安全 */
    void *mutex;            /**< 互斥锁（内部使用） */
} Lv00HashTable;

/**
 * @brief 创建哈希表
 */
Lv00HashTable *lv00_hash_create(size_t initial_capacity, bool thread_safe);

/**
 * @brief 销毁哈希表
 */
void lv00_hash_destroy(Lv00HashTable *ht);

/**
 * @brief 插入键值对
 *
 * @return 旧值（如果键已存在），否则返回NULL
 */
void *lv00_hash_insert(Lv00HashTable *ht, uint64_t key, void *value);

/**
 * @brief 查找值
 */
void *lv00_hash_find(const Lv00HashTable *ht, uint64_t key);

/**
 * @brief 删除键
 *
 * @return 被删除的值，如果不存在返回NULL
 */
void *lv00_hash_remove(Lv00HashTable *ht, uint64_t key);

/**
 * @brief 检查键是否存在
 */
bool lv00_hash_contains(const Lv00HashTable *ht, uint64_t key);

/**
 * @brief 获取条目数量
 */
size_t lv00_hash_size(const Lv00HashTable *ht);

/**
 * @brief 清空哈希表
 */
void lv00_hash_clear(Lv00HashTable *ht);

/**
 * @brief 迭代回调
 */
typedef void (*Lv00HashIterFunc)(uint64_t key, void *value, void *user_data);

/**
 * @brief 遍历哈希表
 */
void lv00_hash_foreach(const Lv00HashTable *ht, Lv00HashIterFunc func, void *user_data);

/* ============== 布隆过滤器 ============== */

/**
 * @brief 布隆过滤器
 */
typedef struct {
    uint8_t *bits;          /**< 位数组 */
    size_t num_bits;        /**< 位数 */
    size_t num_hashes;      /**< 哈希函数数量 */
    size_t count;           /**< 已添加元素数 */
} Lv00BloomFilter;

/**
 * @brief 创建布隆过滤器
 *
 * @param expected_items 预期元素数量
 * @param error_rate 误判率
 */
Lv00BloomFilter *lv00_bloom_create(size_t expected_items, double error_rate);

/**
 * @brief 销毁布隆过滤器
 */
void lv00_bloom_destroy(Lv00BloomFilter *bf);

/**
 * @brief 添加元素
 */
void lv00_bloom_add(Lv00BloomFilter *bf, const void *data, size_t len);

/**
 * @brief 添加字符串
 */
void lv00_bloom_add_str(Lv00BloomFilter *bf, const char *str);

/**
 * @brief 添加整数
 */
void lv00_bloom_add_int(Lv00BloomFilter *bf, int64_t value);

/**
 * @brief 检查元素是否可能存在
 *
 * @return true 表示可能存在（可能有假阳性）
 *         false 表示一定不存在
 */
bool lv00_bloom_might_contain(const Lv00BloomFilter *bf, const void *data, size_t len);

/**
 * @brief 检查字符串是否可能存在
 */
bool lv00_bloom_might_contain_str(const Lv00BloomFilter *bf, const char *str);

/**
 * @brief 获取当前假阳性率估计
 */
double lv00_bloom_estimate_fp_rate(const Lv00BloomFilter *bf);

/**
 * @brief 清空布隆过滤器
 */
void lv00_bloom_clear(Lv00BloomFilter *bf);

/* ============== 跳表 ============== */

/**
 * @brief 跳表节点
 */
typedef struct Lv00SkipNode {
    int64_t key;                    /**< 键 */
    void *value;                    /**< 值 */
    int level;                      /**< 层数 */
    struct Lv00SkipNode **forward;  /**< 前向指针数组 */
} Lv00SkipNode;

/**
 * @brief 跳表
 */
typedef struct {
    Lv00SkipNode *header;           /**< 头节点 */
    int level;                      /**< 当前最大层数 */
    size_t count;                   /**< 元素数量 */
    bool thread_safe;               /**< 是否线程安全 */
    void *mutex;                    /**< 互斥锁 */
} Lv00SkipList;

/**
 * @brief 创建跳表
 */
Lv00SkipList *lv00_skiplist_create(bool thread_safe);

/**
 * @brief 销毁跳表
 */
void lv00_skiplist_destroy(Lv00SkipList *sl);

/**
 * @brief 插入键值对
 */
void lv00_skiplist_insert(Lv00SkipList *sl, int64_t key, void *value);

/**
 * @brief 查找值
 */
void *lv00_skiplist_find(const Lv00SkipList *sl, int64_t key);

/**
 * @brief 删除键
 *
 * @return 被删除的值
 */
void *lv00_skiplist_remove(Lv00SkipList *sl, int64_t key);

/**
 * @brief 查找第一个 >= key 的节点
 */
Lv00SkipNode *lv00_skiplist_lower_bound(const Lv00SkipList *sl, int64_t key);

/**
 * @brief 获取最小键的节点
 */
Lv00SkipNode *lv00_skiplist_first(const Lv00SkipList *sl);

/**
 * @brief 获取最大键的节点
 */
Lv00SkipNode *lv00_skiplist_last(const Lv00SkipList *sl);

/**
 * @brief 获取元素数量
 */
size_t lv00_skiplist_size(const Lv00SkipList *sl);

/* ============== 高性能LRU缓存 ============== */

/**
 * @brief LRU缓存节点
 */
typedef struct Lv00LRUNode {
    uint64_t key;                   /**< 键 */
    void *value;                    /**< 值 */
    struct Lv00LRUNode *prev;       /**< 前向指针 */
    struct Lv00LRUNode *next;       /**< 后向指针 */
    struct Lv00LRUNode *hash_next;  /**< 哈希链 */
} Lv00LRUNode;

/**
 * @brief LRU缓存
 */
typedef struct {
    Lv00LRUNode **hash_table;       /**< 哈希表 */
    size_t hash_capacity;           /**< 哈希表容量 */
    Lv00LRUNode *head;              /**< 最近使用 */
    Lv00LRUNode *tail;              /**< 最少使用 */
    size_t capacity;                /**< 最大容量 */
    size_t count;                   /**< 当前数量 */
    bool thread_safe;               /**< 是否线程安全 */
    void *mutex;                    /**< 互斥锁 */

    /* 统计 */
    uint64_t hits;
    uint64_t misses;
} Lv00LRUCache;

/**
 * @brief 值销毁回调
 */
typedef void (*Lv00LRUDestroyFunc)(void *value);

/**
 * @brief 创建LRU缓存
 */
Lv00LRUCache *lv00_lru_create(size_t capacity, bool thread_safe);

/**
 * @brief 销毁LRU缓存
 */
void lv00_lru_destroy(Lv00LRUCache *lru, Lv00LRUDestroyFunc destroy_func);

/**
 * @brief 获取值（命中则移到前面）
 */
void *lv00_lru_get(Lv00LRUCache *lru, uint64_t key);

/**
 * @brief 插入值（超出容量则淘汰最少使用）
 *
 * @return 被淘汰的值（如果有）
 */
void *lv00_lru_put(Lv00LRUCache *lru, uint64_t key, void *value);

/**
 * @brief 删除键
 */
void *lv00_lru_remove(Lv00LRUCache *lru, uint64_t key);

/**
 * @brief 检查键是否存在
 */
bool lv00_lru_contains(const Lv00LRUCache *lru, uint64_t key);

/**
 * @brief 清空缓存
 */
void lv00_lru_clear(Lv00LRUCache *lru, Lv00LRUDestroyFunc destroy_func);

/**
 * @brief 获取命中率
 */
double lv00_lru_hit_rate(const Lv00LRUCache *lru);

/**
 * @brief 获取统计信息
 */
void lv00_lru_get_stats(const Lv00LRUCache *lru, uint64_t *hits, uint64_t *misses, size_t *count);

/* ============== 空间索引（R树变体） ============== */

/**
 * @brief 二维边界框
 */
typedef struct {
    double min_x, min_y;
    double max_x, max_y;
} Lv00BBox2D;

/**
 * @brief 空间索引条目
 */
typedef struct Lv00SpatialEntry {
    Lv00BBox2D bbox;                /**< 边界框 */
    void *data;                     /**< 用户数据 */
    uint64_t id;                    /**< 条目ID */
} Lv00SpatialEntry;

/**
 * @brief R树节点
 */
typedef struct Lv00RTreeNode {
    Lv00BBox2D bbox;                        /**< 节点边界框 */
    int count;                              /**< 子节点/条目数量 */
    bool is_leaf;                           /**< 是否为叶节点 */
    union {
        struct Lv00RTreeNode **children;    /**< 子节点 */
        Lv00SpatialEntry *entries;          /**< 条目数组 */
    };
} Lv00RTreeNode;

/**
 * @brief R树
 */
typedef struct {
    Lv00RTreeNode *root;            /**< 根节点 */
    int max_entries;                /**< 每节点最大条目数 */
    int min_entries;                /**< 每节点最小条目数 */
    size_t count;                   /**< 总条目数 */
    bool thread_safe;               /**< 是否线程安全 */
    void *mutex;                    /**< 互斥锁 */
} Lv00RTree;

/**
 * @brief 创建R树
 */
Lv00RTree *lv00_rtree_create(int max_entries, bool thread_safe);

/**
 * @brief 销毁R树
 */
void lv00_rtree_destroy(Lv00RTree *rtree);

/**
 * @brief 插入条目
 *
 * @param rtree R树
 * @param bbox 边界框
 * @param data 用户数据
 * @return 条目ID
 */
uint64_t lv00_rtree_insert(Lv00RTree *rtree, const Lv00BBox2D *bbox, void *data);

/**
 * @brief 删除条目
 */
bool lv00_rtree_remove(Lv00RTree *rtree, uint64_t id);

/**
 * @brief 查询与边界框相交的所有条目
 */
typedef void (*Lv00SpatialQueryFunc)(const Lv00SpatialEntry *entry, void *user_data);

void lv00_rtree_query(const Lv00RTree *rtree, const Lv00BBox2D *bbox,
                      Lv00SpatialQueryFunc callback, void *user_data);

/**
 * @brief 查询包含点的所有条目
 */
void lv00_rtree_query_point(const Lv00RTree *rtree, double x, double y,
                            Lv00SpatialQueryFunc callback, void *user_data);

/**
 * @brief 查询与边界框相交的条目数量
 */
size_t lv00_rtree_count_intersect(const Lv00RTree *rtree, const Lv00BBox2D *bbox);

/**
 * @brief 查找最近的k个条目
 */
void lv00_rtree_nearest(const Lv00RTree *rtree, double x, double y, int k,
                        Lv00SpatialQueryFunc callback, void *user_data);

/**
 * @brief 获取条目总数
 */
size_t lv00_rtree_size(const Lv00RTree *rtree);

/**
 * @brief 获取R树高度
 */
int lv00_rtree_height(const Lv00RTree *rtree);

/* ============== 辅助函数 ============== */

/**
 * @brief 计算边界框面积
 */
static inline double lv00_bbox_area(const Lv00BBox2D *bbox) {
    return (bbox->max_x - bbox->min_x) * (bbox->max_y - bbox->min_y);
}

/**
 * @brief 计算两个边界框的合并面积
 */
Lv00BBox2D lv00_bbox_merge(const Lv00BBox2D *a, const Lv00BBox2D *b);

/**
 * @brief 计算两个边界框的交集面积
 */
double lv00_bbox_intersection_area(const Lv00BBox2D *a, const Lv00BBox2D *b);

/**
 * @brief 检查两个边界框是否相交
 */
bool lv00_bbox_intersects(const Lv00BBox2D *a, const Lv00BBox2D *b);

/**
 * @brief 检查点是否在边界框内
 */
bool lv00_bbox_contains_point(const Lv00BBox2D *bbox, double x, double y);

/**
 * @brief 计算点到边界框的最小距离
 */
double lv00_bbox_distance_to_point(const Lv00BBox2D *bbox, double x, double y);

/* ============== 性能统计 ============== */

/**
 * @brief 索引统计信息
 */
typedef struct {
    uint64_t lookups;       /**< 查找次数 */
    uint64_t inserts;       /**< 插入次数 */
    uint64_t deletes;       /**< 删除次数 */
    uint64_t cache_hits;    /**< 缓存命中次数 */
    uint64_t cache_misses;  /**< 缓存未命中次数 */
    uint64_t rebalances;    /**< 重平衡次数 */
    uint64_t total_time_us; /**< 总耗时 */
} Lv00IndexStats;

/**
 * @brief 打印索引诊断信息
 */
void lv00_index_print_diag(void *stream);

#ifdef __cplusplus
}
#endif

#endif /* LV00_FAST_INDEX_H */

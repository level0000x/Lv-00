/**
 * @file lv_lru_cache.c
 * @brief LRU 缓存设施实现（PERFORMANCE_OPTIMIZATION.md §2.3 蓝图落地）
 *
 * @details
 *  结构：lv_hashtable_int_*（key → lvLruNode*，O(1) 查找）
 *      + 双向链表记录访问顺序（MRU 在头，LRU 在尾）。
 *  get 命中 → 节点移至头部（刷新）；put 新键 → 插头；
 *  put 满且新键 → 淘汰尾部节点；put 已存在 → 更新值并移至头部。
 *
 *  值所有权：value 为 [borrow]，调用方持有；缓存不拷贝、不释放，
 *  淘汰/销毁仅丢弃引用。所有内存经 lv_malloc/lv_calloc/lv_free 分配。
 *
 *  线程安全：thread_safe=true 的缓存共享文件级惰性互斥锁 g_lru_lock
 *  （仿 dsl_extension.c 的 lv_LAZY_LOCK_DEFINE 用法；锁进程生命周期内
 *  不销毁）；thread_safe=false 时不加锁，由调用方保证串行访问。
 *
 * @author Lv-00 Project
 * @version 1.0.0
 */

#include "lv/lv_lru_cache.h"

#include <stddef.h>

#include "lv/lv_hashtable.h"
#include "lv/lv_thread.h"  /* lv_LAZY_LOCK_DEFINE / lv_lazy_lock_lock / unlock */
#include "lv/lv_utils.h"   /* lv_malloc, lv_calloc, lv_free, lv_FREE_AND_NULL */

/* 默认容量：capacity<=0 时使用 */
#define lv_LRU_DEFAULT_CAPACITY 64

/* ---- 内部链表节点 ---- */

typedef struct lvLruNode {
    int key;                /**< 条目键 */
    void *value;            /**< 条目值（[borrow]：所有权归调用方，缓存不释放） */
    struct lvLruNode *prev; /**< 前驱（更近 MRU 侧） */
    struct lvLruNode *next; /**< 后继（更近 LRU 侧） */
} lvLruNode;

/* ---- 缓存结构 ---- */

struct lvLRUCache {
    int capacity;       /**< 容量上限（创建时确定，>=1） */
    int count;          /**< 当前条目数 */
    bool thread_safe;   /**< 是否启用内部互斥保护 */
    lvHashtable *table; /**< key → lvLruNode* */
    lvLruNode *head;    /**< 链表头 = 最新使用（MRU） */
    lvLruNode *tail;    /**< 链表尾 = 最久未用（LRU） */
};

/* ---- 线程安全：文件级惰性互斥锁（仿 dsl_extension.c） ---- */

lv_LAZY_LOCK_DEFINE(g_lru_lock);
#define LRU_LOCK()   lv_lazy_lock_lock(&g_lru_lock, g_lru_lock_init_once)
#define LRU_UNLOCK() lv_lazy_lock_unlock(&g_lru_lock)

/** @brief 仅对 thread_safe 缓存加锁 */
static void lru_lock(lvLRUCache *cache) {
    if (cache->thread_safe)
        LRU_LOCK();
}

/** @brief 仅对 thread_safe 缓存解锁 */
static void lru_unlock(lvLRUCache *cache) {
    if (cache->thread_safe)
        LRU_UNLOCK();
}

/* ============================================================
 * 内部链表辅助（调用方须已持有锁）
 * ============================================================ */

/** @brief 将节点从链表摘除并修正头/尾指针 */
static void lru_detach(lvLRUCache *cache, lvLruNode *node) {
    if (node->prev)
        node->prev->next = node->next;
    else
        cache->head = node->next;
    if (node->next)
        node->next->prev = node->prev;
    else
        cache->tail = node->prev;
    node->prev = NULL;
    node->next = NULL;
}

/** @brief 将节点插入链表头部（MRU）；节点须不在链表中 */
static void lru_push_front(lvLRUCache *cache, lvLruNode *node) {
    node->prev = NULL;
    node->next = cache->head;
    if (cache->head)
        cache->head->prev = node;
    else
        cache->tail = node; /* 空链表：头尾同指 */
    cache->head = node;
}

/** @brief 淘汰链表尾部（LRU）节点：摘除 + 哈希表删键 + 释放节点（不释放 value） */
static void lru_evict_lru(lvLRUCache *cache) {
    lvLruNode *victim = cache->tail;
    if (!victim)
        return;
    lru_detach(cache, victim);
    lv_hashtable_int_remove(cache->table, victim->key);
    /* value 所有权归调用方：仅丢弃引用，不释放 */
    lv_FREE_AND_NULL(victim);
    cache->count--;
}

/* ============================================================
 * 公共接口
 * ============================================================ */

lvLRUCache *lv_lru_create(int capacity, bool thread_safe) {
    if (capacity <= 0)
        capacity = lv_LRU_DEFAULT_CAPACITY;
    lvLRUCache *cache = (lvLRUCache *) lv_calloc(1, sizeof(lvLRUCache));
    if (!cache)
        return NULL;
    cache->capacity = capacity;
    cache->thread_safe = thread_safe;
    /* 初始槽位按容量申请即可：负载因子 0.75 达到后哈希表自动扩容 */
    cache->table = lv_hashtable_int_create(capacity);
    if (!cache->table) {
        lv_FREE_AND_NULL(cache);
        return NULL;
    }
    return cache;
}

void lv_lru_destroy(lvLRUCache *cache) {
    if (!cache)
        return;
    lru_lock(cache);
    lvLruNode *node = cache->head;
    while (node) {
        lvLruNode *next = node->next;
        lv_FREE_AND_NULL(node); /* value 所有权归调用方：不释放 */
        node = next;
    }
    cache->head = NULL;
    cache->tail = NULL;
    cache->count = 0;
    if (cache->table) {
        lv_hashtable_int_destroy(cache->table);
        cache->table = NULL;
    }
    lru_unlock(cache);
    lv_FREE_AND_NULL(cache);
}

bool lv_lru_put(lvLRUCache *cache, int key, void *value) {
    if (!cache || !value)
        return false; /* NULL value 与「未命中返回 NULL」语义冲突 */
    lru_lock(cache);

    /* 键已存在：更新值并提升为最新使用 */
    lvLruNode *node = (lvLruNode *) lv_hashtable_int_get(cache->table, key);
    if (node) {
        node->value = value;
        if (cache->head != node) {
            lru_detach(cache, node);
            lru_push_front(cache, node);
        }
        lru_unlock(cache);
        return true;
    }

    /* 缓存已满且为新增键：先淘汰最久未用条目 */
    if (cache->count >= cache->capacity)
        lru_evict_lru(cache);

    /* 插入新节点：链表头 + 哈希表注册 */
    node = (lvLruNode *) lv_calloc(1, sizeof(lvLruNode));
    if (!node) {
        lru_unlock(cache);
        return false;
    }
    node->key = key;
    node->value = value;
    if (!lv_hashtable_int_insert(cache->table, key, node)) {
        /* 已加锁下仅可能为哈希表扩容 OOM；释放节点并报失败 */
        lv_FREE_AND_NULL(node);
        lru_unlock(cache);
        return false;
    }
    lru_push_front(cache, node);
    cache->count++;
    lru_unlock(cache);
    return true;
}

void *lv_lru_get(lvLRUCache *cache, int key) {
    if (!cache)
        return NULL;
    lru_lock(cache);
    lvLruNode *node = (lvLruNode *) lv_hashtable_int_get(cache->table, key);
    void *result = NULL;
    if (node) {
        /* 命中：刷新访问时间（移至头部） */
        if (cache->head != node) {
            lru_detach(cache, node);
            lru_push_front(cache, node);
        }
        result = node->value;
    }
    lru_unlock(cache);
    return result;
}

int lv_lru_count(const lvLRUCache *cache) {
    if (!cache)
        return 0;
    /* 读操作同样受锁保护；const 语义下加锁属逻辑操作，非数据修改 */
    lvLRUCache *c = (lvLRUCache *) cache;
    lru_lock(c);
    int count = c->count;
    lru_unlock(c);
    return count;
}

int lv_lru_capacity(const lvLRUCache *cache) {
    if (!cache)
        return 0;
    lvLRUCache *c = (lvLRUCache *) cache;
    lru_lock(c);
    int capacity = c->capacity;
    lru_unlock(c);
    return capacity;
}

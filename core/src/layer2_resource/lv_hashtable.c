/**
 * @file lv_hashtable.c
 * @brief 统一哈希表设施实现（int 键 / string 键 双形态）
 *
 * @details
 *  int 形态：开放寻址（线性探测）+ 自动扩容重哈希 + tombstone 删除。
 *    槽位三态：EMPTY（从未使用）/ OCCUPIED（占用）/ DELETED（墓碑）。
 *    删除只置 DELETED 标记，探测链保持完整；墓碑槽可被后续插入复用，
 *    扩容重哈希时自然丢弃。负载因子 0.75，容量恒为 2 的幂。
 *
 *  string 形态：分离链式（同桶头插）+ 自动扩容重哈希。
 *    键副本由表内部持有（lv_malloc + memcpy），值所有权归调用方；
 *    扩容时仅重排节点（键副本复用，不重新复制）。
 *
 * @author Lv-00 Project
 * @version 1.0.0
 */

#include "lv/lv_hashtable.h"

#include "lv/config.h"    /* lv_FNV_HASH_MULTIPLIER */
#include "lv/lv_utils.h"  /* lv_malloc, lv_calloc, lv_free, lv_fnv1a_hash_str */

#include <string.h>

/* ---- 内部常量 ---- */

/** 槽位状态 */
enum {
    LV_HT_EMPTY = 0,
    LV_HT_OCCUPIED = 1,
    LV_HT_DELETED = 2
};

/** 表形态 */
enum {
    LV_HT_KIND_INT = 0,
    LV_HT_KIND_STR = 1
};

/** int 形态默认初始容量 */
#define LV_HT_DEFAULT_CAPACITY 8
/** string 形态默认初始桶数 */
#define LV_HT_DEFAULT_BUCKETS 16
/** 容量上限（2^30，防 int 溢出） */
#define LV_HT_MAX_CAPACITY 0x40000000
/** 负载因子 0.75 = 3/4 */
#define LV_HT_LOAD_NUMERATOR 3
#define LV_HT_LOAD_DENOMINATOR 4

/* ---- 内部结构 ---- */

/** int 形态槽位 */
typedef struct {
    int key;
    void *value;
    uint8_t state; /* LV_HT_EMPTY / LV_HT_OCCUPIED / LV_HT_DELETED */
} lvIntSlot;

/** string 形态链节点 */
typedef struct lvStrNode {
    char *key;
    void *value;
    struct lvStrNode *next;
} lvStrNode;

struct lvHashtable {
    uint8_t kind;    /* LV_HT_KIND_INT / LV_HT_KIND_STR */
    int capacity;    /* int 形态：槽位数；string 形态：桶数（均 2 的幂） */
    int count;       /* 当前条目数 */
    int deleted;     /* int 形态：墓碑数（string 形态未用） */
    void *storage;   /* int：lvIntSlot *；string：lvStrNode ** */
};

/* ---- 内部辅助 ---- */

/** 不小于 min 的 2 的幂（上限 LV_HT_MAX_CAPACITY） */
static int lv_ht_next_pow2(int min) {
    int cap = 1;
    while (cap < min && cap <= LV_HT_MAX_CAPACITY / 2) {
        cap <<= 1;
    }
    return cap;
}

/** 字符串键副本 */
static char *lv_ht_strdup(const char *s) {
    size_t len = strlen(s);
    char *copy = (char *) lv_malloc(len + 1);
    if (copy)
        memcpy(copy, s, len + 1);
    return copy;
}

/* ========================================================================
 * int 键形态
 * ======================================================================== */

unsigned lv_hashtable_int_hash(int key, int capacity) {
    /* FNV-1a 单步：与 lv_FNV_HASH_MULTIPLIER（0x01000193U）一致，
     * 2 的幂容量走位掩码，否则取模（防御非 2 幂容量）。 */
    unsigned h = (unsigned) key * lv_FNV_HASH_MULTIPLIER;
    if (capacity > 0 && (capacity & (capacity - 1)) == 0) {
        return h & (unsigned) (capacity - 1);
    }
    return h % (unsigned) (capacity > 0 ? capacity : 1);
}

lvHashtable *lv_hashtable_int_create(int initial_capacity) {
    if (initial_capacity <= 0)
        initial_capacity = LV_HT_DEFAULT_CAPACITY;
    int cap = lv_ht_next_pow2(initial_capacity);

    lvHashtable *ht = (lvHashtable *) lv_calloc(1, sizeof(lvHashtable));
    if (!ht)
        return NULL;
    ht->kind = LV_HT_KIND_INT;
    ht->capacity = cap;
    ht->storage = lv_calloc((size_t) cap, sizeof(lvIntSlot));
    if (!ht->storage) {
        lv_free((void **) &ht);
        return NULL;
    }
    return ht;
}

void lv_hashtable_int_destroy(lvHashtable *ht) {
    if (!ht)
        return;
    if (ht->kind == LV_HT_KIND_INT) {
        lv_free((void **) &ht->storage);
    }
    lv_free((void **) &ht);
}

void *lv_hashtable_int_get(const lvHashtable *ht, int key) {
    if (!ht || ht->kind != LV_HT_KIND_INT || ht->capacity <= 0)
        return NULL;

    const lvIntSlot *slots = (const lvIntSlot *) ht->storage;
    unsigned idx = lv_hashtable_int_hash(key, ht->capacity);
    while (true) {
        const lvIntSlot *slot = &slots[idx];
        if (slot->state == LV_HT_EMPTY)
            return NULL; /* 从未使用：探测链结束 */
        if (slot->state == LV_HT_OCCUPIED && slot->key == key)
            return slot->value;
        idx = (idx + 1) & (unsigned) (ht->capacity - 1);
    }
}

bool lv_hashtable_int_contains(const lvHashtable *ht, int key) {
    return lv_hashtable_int_get(ht, key) != NULL;
}

/** int 形态扩容：翻倍容量并重哈希（丢弃墓碑） */
static bool lv_ht_int_grow(lvHashtable *ht) {
    if (ht->capacity >= LV_HT_MAX_CAPACITY)
        return false;
    int new_cap = ht->capacity * 2;

    lvIntSlot *new_slots = (lvIntSlot *) lv_calloc((size_t) new_cap, sizeof(lvIntSlot));
    if (!new_slots)
        return false;

    const lvIntSlot *old = (const lvIntSlot *) ht->storage;
    for (int i = 0; i < ht->capacity; i++) {
        if (old[i].state == LV_HT_OCCUPIED) {
            unsigned idx = lv_hashtable_int_hash(old[i].key, new_cap);
            while (new_slots[idx].state == LV_HT_OCCUPIED) {
                idx = (idx + 1) & (unsigned) (new_cap - 1);
            }
            new_slots[idx].key = old[i].key;
            new_slots[idx].value = old[i].value;
            new_slots[idx].state = LV_HT_OCCUPIED;
        }
    }

    lv_free((void **) &ht->storage);
    ht->storage = new_slots;
    ht->capacity = new_cap;
    ht->deleted = 0;
    return true;
}

bool lv_hashtable_int_insert(lvHashtable *ht, int key, void *value) {
    if (!ht || ht->kind != LV_HT_KIND_INT)
        return false;

    /* 达到负载阈值（count + deleted 均占用槽位）时扩容 */
    if (ht->count + ht->deleted >= ht->capacity * LV_HT_LOAD_NUMERATOR / LV_HT_LOAD_DENOMINATOR) {
        if (!lv_ht_int_grow(ht))
            return false;
    }

    lvIntSlot *slots = (lvIntSlot *) ht->storage;
    unsigned idx = lv_hashtable_int_hash(key, ht->capacity);
    int first_deleted = -1;

    while (true) {
        lvIntSlot *slot = &slots[idx];
        if (slot->state == LV_HT_EMPTY) {
            /* 探测链结束：key 不存在，插入（优先复用探测途中遇到的墓碑槽） */
            int target = (first_deleted >= 0) ? first_deleted : (int) idx;
            slots[target].key = key;
            slots[target].value = value;
            slots[target].state = LV_HT_OCCUPIED;
            if (first_deleted >= 0)
                ht->deleted--;
            ht->count++;
            return true;
        }
        if (slot->state == LV_HT_OCCUPIED && slot->key == key)
            return false; /* 已存在 */
        if (slot->state == LV_HT_DELETED && first_deleted < 0)
            first_deleted = (int) idx;
        idx = (idx + 1) & (unsigned) (ht->capacity - 1);
    }
}

bool lv_hashtable_int_remove(lvHashtable *ht, int key) {
    if (!ht || ht->kind != LV_HT_KIND_INT)
        return false;

    lvIntSlot *slots = (lvIntSlot *) ht->storage;
    unsigned idx = lv_hashtable_int_hash(key, ht->capacity);
    while (true) {
        lvIntSlot *slot = &slots[idx];
        if (slot->state == LV_HT_EMPTY)
            return false; /* 未找到 */
        if (slot->state == LV_HT_OCCUPIED && slot->key == key) {
            /* tombstone 删除：保留探测链完整 */
            slot->state = LV_HT_DELETED;
            slot->value = NULL;
            ht->count--;
            ht->deleted++;
            return true;
        }
        idx = (idx + 1) & (unsigned) (ht->capacity - 1);
    }
}

int lv_hashtable_int_count(const lvHashtable *ht) {
    return (ht && ht->kind == LV_HT_KIND_INT) ? ht->count : 0;
}

void lv_hashtable_int_foreach(lvHashtable *ht, lvHashtableIntVisitor visitor, void *ctx) {
    if (!ht || !visitor || ht->kind != LV_HT_KIND_INT)
        return;

    lvIntSlot *slots = (lvIntSlot *) ht->storage;
    for (int i = 0; i < ht->capacity; i++) {
        if (slots[i].state == LV_HT_OCCUPIED) {
            int k = slots[i].key;
            void *v = slots[i].value;
            visitor(k, v, ctx); /* 回调可安全释放 value（值所有权归调用方） */
        }
    }
}

/* ========================================================================
 * string 键形态
 * ======================================================================== */

/** string 形态扩容：翻倍桶数并重哈希（节点复用，仅重排） */
static bool lv_ht_str_grow(lvHashtable *ht) {
    if (ht->capacity >= LV_HT_MAX_CAPACITY)
        return false;
    int new_buckets = ht->capacity * 2;

    lvStrNode **new_buckets_arr = (lvStrNode **) lv_calloc((size_t) new_buckets, sizeof(lvStrNode *));
    if (!new_buckets_arr)
        return false;

    lvStrNode **old = (lvStrNode **) ht->storage;
    for (int i = 0; i < ht->capacity; i++) {
        lvStrNode *node = old[i];
        while (node) {
            lvStrNode *next = node->next;
            unsigned b = (unsigned) (lv_fnv1a_hash_str(node->key) % (uint64_t) new_buckets);
            node->next = new_buckets_arr[b];
            new_buckets_arr[b] = node;
            node = next;
        }
    }

    lv_free((void **) &ht->storage);
    ht->storage = new_buckets_arr;
    ht->capacity = new_buckets;
    return true;
}

lvHashtable *lv_hashtable_str_create(int initial_bucket_count) {
    if (initial_bucket_count <= 0)
        initial_bucket_count = LV_HT_DEFAULT_BUCKETS;
    int buckets = lv_ht_next_pow2(initial_bucket_count);

    lvHashtable *ht = (lvHashtable *) lv_calloc(1, sizeof(lvHashtable));
    if (!ht)
        return NULL;
    ht->kind = LV_HT_KIND_STR;
    ht->capacity = buckets;
    ht->storage = lv_calloc((size_t) buckets, sizeof(lvStrNode *));
    if (!ht->storage) {
        lv_free((void **) &ht);
        return NULL;
    }
    return ht;
}

void lv_hashtable_str_destroy(lvHashtable *ht) {
    if (!ht)
        return;
    if (ht->kind == LV_HT_KIND_STR) {
        lvStrNode **buckets = (lvStrNode **) ht->storage;
        for (int i = 0; i < ht->capacity; i++) {
            lvStrNode *node = buckets[i];
            while (node) {
                lvStrNode *next = node->next;
                lv_free((void **) &node->key);
                lv_free((void **) &node);
                node = next;
            }
        }
        lv_free((void **) &ht->storage);
    }
    lv_free((void **) &ht);
}

void *lv_hashtable_str_get(const lvHashtable *ht, const char *key) {
    if (!ht || !key || ht->kind != LV_HT_KIND_STR)
        return NULL;

    lvStrNode **buckets = (lvStrNode **) ht->storage;
    unsigned b = (unsigned) (lv_fnv1a_hash_str(key) % (uint64_t) ht->capacity);
    for (lvStrNode *node = buckets[b]; node; node = node->next) {
        if (strcmp(node->key, key) == 0)
            return node->value;
    }
    return NULL;
}

bool lv_hashtable_str_contains(const lvHashtable *ht, const char *key) {
    return lv_hashtable_str_get(ht, key) != NULL;
}

bool lv_hashtable_str_insert(lvHashtable *ht, const char *key, void *value) {
    if (!ht || !key || ht->kind != LV_HT_KIND_STR)
        return false;

    if (lv_hashtable_str_get(ht, key))
        return false; /* 已存在 */

    if (ht->count >= ht->capacity * LV_HT_LOAD_NUMERATOR / LV_HT_LOAD_DENOMINATOR) {
        if (!lv_ht_str_grow(ht))
            return false;
    }

    char *key_copy = lv_ht_strdup(key);
    if (!key_copy)
        return false;

    lvStrNode *node = (lvStrNode *) lv_malloc(sizeof(lvStrNode));
    if (!node) {
        lv_free((void **) &key_copy);
        return false;
    }

    lvStrNode **buckets = (lvStrNode **) ht->storage;
    unsigned b = (unsigned) (lv_fnv1a_hash_str(key) % (uint64_t) ht->capacity);
    node->key = key_copy;
    node->value = value;
    node->next = buckets[b]; /* 头插 */
    buckets[b] = node;
    ht->count++;
    return true;
}

bool lv_hashtable_str_remove(lvHashtable *ht, const char *key) {
    if (!ht || !key || ht->kind != LV_HT_KIND_STR)
        return false;

    lvStrNode **buckets = (lvStrNode **) ht->storage;
    unsigned b = (unsigned) (lv_fnv1a_hash_str(key) % (uint64_t) ht->capacity);
    lvStrNode *node = buckets[b];
    lvStrNode *prev = NULL;
    while (node) {
        if (strcmp(node->key, key) == 0) {
            if (prev)
                prev->next = node->next;
            else
                buckets[b] = node->next;
            lv_free((void **) &node->key);
            lv_free((void **) &node);
            ht->count--;
            return true;
        }
        prev = node;
        node = node->next;
    }
    return false;
}

int lv_hashtable_str_count(const lvHashtable *ht) {
    return (ht && ht->kind == LV_HT_KIND_STR) ? ht->count : 0;
}

void lv_hashtable_str_foreach(lvHashtable *ht, lvHashtableStrVisitor visitor, void *ctx) {
    if (!ht || !visitor || ht->kind != LV_HT_KIND_STR)
        return;

    lvStrNode **buckets = (lvStrNode **) ht->storage;
    for (int i = 0; i < ht->capacity; i++) {
        lvStrNode *node = buckets[i];
        while (node) {
            lvStrNode *next = node->next; /* 先保存后继，回调可安全释放当前 value */
            const char *k = node->key;
            void *v = node->value;
            visitor(k, v, ctx);
            node = next;
        }
    }
}
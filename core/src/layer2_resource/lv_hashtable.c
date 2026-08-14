/**
 * @file lv_hashtable.c
 * @brief 统一哈希表设施实现（int 键 / int64 键 / string 键 三形态）
 *
 * @details
 *  int 与 int64（i64）形态：开放寻址（线性探测）+ 自动扩容重哈希 +
 *    tombstone 删除，二者由同一宏模板 LV_HT_DEFINE_OPENADDR_KIND 实例化，
 *    仅键类型与哈希函数不同；扩容/重哈希/插入/查找/删除/遍历共用一份模板实现。
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

#include "lv/config.h"    /* lv_FNV_HASH_MULTIPLIER, lv_FNV64_OFFSET_BASIS */
#include "lv/lv_utils.h"  /* lv_malloc, lv_calloc, lv_free, lv_fnv1a_hash_str, lv_fnv1a_hash_int */
#include "lv/lv_str_utils.h"

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
    LV_HT_KIND_STR = 1,
    LV_HT_KIND_I64 = 2
};

/** 开放寻址形态默认初始容量 */
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

/** int64（i64）形态槽位 */
typedef struct {
    int64_t key;
    void *value;
    uint8_t state; /* LV_HT_EMPTY / LV_HT_OCCUPIED / LV_HT_DELETED */
} lvI64Slot;

/** string 形态链节点 */
typedef struct lvStrNode {
    char *key;
    void *value;
    struct lvStrNode *next;
} lvStrNode;

struct lvHashtable {
    uint8_t kind;    /* LV_HT_KIND_INT / LV_HT_KIND_STR / LV_HT_KIND_I64 */
    int capacity;    /* 开放寻址形态：槽位数；string 形态：桶数（均 2 的幂） */
    int count;       /* 当前条目数 */
    int deleted;     /* 开放寻址形态：墓碑数（string 形态未用） */
    void *storage;   /* int：lvIntSlot *；i64：lvI64Slot *；string：lvStrNode ** */
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


/* ========================================================================
 * 开放寻址（线性探测）键形态公共模板：int / int64
 *
 * 由宏 LV_HT_DEFINE_OPENADDR_KIND 实例化出两个键形态：
 *   - int 形态（lv_hashtable_int_*）：键类型 int，哈希 lv_hashtable_int_hash
 *   - i64 形态（lv_hashtable_i64_*）：键类型 int64_t，哈希 lv_hashtable_i64_hash
 * 扩容/重哈希/插入/查找/删除/遍历逻辑由模板统一维护，两个形态共用一份实现，
 * 保证语义（负载因子、tombstone 删除、扩容重哈希丢弃墓碑）严格一致。
 * ======================================================================== */

/**
 * @def LV_HT_DEFINE_OPENADDR_KIND(FN_SUFFIX, TYPE_SUFFIX, KEY_TYPE, KIND, SLOT_TYPE, HASH_FN, DEFAULT_CAP)
 * 实例化一套开放寻址（线性探测 + tombstone）哈希表形态实现。
 * @param FN_SUFFIX   函数名后缀（小写）：int / i64
 * @param TYPE_SUFFIX Visitor 类型名后缀（首字母大写）：Int / I64
 * @param KEY_TYPE    键 C 类型：int / int64_t
 * @param KIND        形态枚举：LV_HT_KIND_INT / LV_HT_KIND_I64
 * @param SLOT_TYPE   槽位结构类型：lvIntSlot / lvI64Slot
 * @param HASH_FN     哈希函数名，签名 (KEY_TYPE key, int capacity) -> unsigned
 * @param DEFAULT_CAP 默认初始容量宏
 */
#define LV_HT_DEFINE_OPENADDR_KIND(FN_SUFFIX, TYPE_SUFFIX, KEY_TYPE, KIND, SLOT_TYPE, HASH_FN, DEFAULT_CAP) \
                                                                                                     \
lvHashtable *lv_hashtable_##FN_SUFFIX##_create(int initial_capacity) {                               \
    if (initial_capacity <= 0)                                                                       \
        initial_capacity = DEFAULT_CAP;                                                              \
    int cap = lv_ht_next_pow2(initial_capacity);                                                     \
                                                                                                     \
    lvHashtable *ht = (lvHashtable *) lv_calloc(1, sizeof(lvHashtable));                             \
    if (!ht)                                                                                         \
        return NULL;                                                                                 \
    ht->kind = KIND;                                                                                 \
    ht->capacity = cap;                                                                              \
    ht->storage = lv_calloc((size_t) cap, sizeof(SLOT_TYPE));                                        \
    if (!ht->storage) {                                                                              \
        lv_free((void **) &ht);                                                                      \
        return NULL;                                                                                 \
    }                                                                                                \
    return ht;                                                                                       \
}                                                                                                    \
                                                                                                     \
void lv_hashtable_##FN_SUFFIX##_destroy(lvHashtable *ht) {                                           \
    if (!ht)                                                                                         \
        return;                                                                                      \
    if (ht->kind == KIND) {                                                                          \
        lv_free((void **) &ht->storage);                                                             \
    }                                                                                                \
    lv_free((void **) &ht);                                                                          \
}                                                                                                    \
                                                                                                     \
void *lv_hashtable_##FN_SUFFIX##_get(const lvHashtable *ht, KEY_TYPE key) {                          \
    if (!ht || ht->kind != KIND || ht->capacity <= 0)                                                \
        return NULL;                                                                                 \
                                                                                                     \
    const SLOT_TYPE *slots = (const SLOT_TYPE *) ht->storage;                                        \
    unsigned idx = HASH_FN(key, ht->capacity);                                                       \
    while (true) {                                                                                   \
        const SLOT_TYPE *slot = &slots[idx];                                                         \
        if (slot->state == LV_HT_EMPTY)                                                              \
            return NULL; /* 从未使用：探测链结束 */                                                  \
        if (slot->state == LV_HT_OCCUPIED && slot->key == key)                                       \
            return slot->value;                                                                      \
        idx = (idx + 1) & (unsigned) (ht->capacity - 1);                                             \
    }                                                                                                \
}                                                                                                    \
                                                                                                     \
bool lv_hashtable_##FN_SUFFIX##_contains(const lvHashtable *ht, KEY_TYPE key) {                      \
    return lv_hashtable_##FN_SUFFIX##_get(ht, key) != NULL;                                          \
}                                                                                                    \
                                                                                                     \
/* 开放寻址形态扩容：翻倍容量并重哈希（丢弃墓碑） */                                                  \
static bool lv_ht_##FN_SUFFIX##_grow(lvHashtable *ht) {                                              \
    if (ht->capacity >= LV_HT_MAX_CAPACITY)                                                          \
        return false;                                                                                \
    int new_cap = ht->capacity * 2;                                                                  \
                                                                                                     \
    SLOT_TYPE *new_slots = (SLOT_TYPE *) lv_calloc((size_t) new_cap, sizeof(SLOT_TYPE));             \
    if (!new_slots)                                                                                  \
        return false;                                                                                \
                                                                                                     \
    const SLOT_TYPE *old = (const SLOT_TYPE *) ht->storage;                                          \
    for (int i = 0; i < ht->capacity; i++) {                                                         \
        if (old[i].state == LV_HT_OCCUPIED) {                                                        \
            unsigned idx = HASH_FN(old[i].key, new_cap);                                             \
            while (new_slots[idx].state == LV_HT_OCCUPIED) {                                         \
                idx = (idx + 1) & (unsigned) (new_cap - 1);                                          \
            }                                                                                        \
            new_slots[idx].key = old[i].key;                                                         \
            new_slots[idx].value = old[i].value;                                                     \
            new_slots[idx].state = LV_HT_OCCUPIED;                                                   \
        }                                                                                            \
    }                                                                                                \
                                                                                                     \
    lv_free((void **) &ht->storage);                                                                 \
    ht->storage = new_slots;                                                                         \
    ht->capacity = new_cap;                                                                          \
    ht->deleted = 0;                                                                                 \
    return true;                                                                                     \
}                                                                                                    \
                                                                                                     \
bool lv_hashtable_##FN_SUFFIX##_insert(lvHashtable *ht, KEY_TYPE key, void *value) {                 \
    if (!ht || ht->kind != KIND)                                                                     \
        return false;                                                                                \
                                                                                                     \
    /* 达到负载阈值（count + deleted 均占用槽位）时扩容 */                                             \
    if (ht->count + ht->deleted >= ht->capacity * LV_HT_LOAD_NUMERATOR / LV_HT_LOAD_DENOMINATOR) {   \
        if (!lv_ht_##FN_SUFFIX##_grow(ht))                                                           \
            return false;                                                                            \
    }                                                                                                \
                                                                                                     \
    SLOT_TYPE *slots = (SLOT_TYPE *) ht->storage;                                                    \
    unsigned idx = HASH_FN(key, ht->capacity);                                                       \
    int first_deleted = -1;                                                                          \
                                                                                                     \
    while (true) {                                                                                   \
        SLOT_TYPE *slot = &slots[idx];                                                               \
        if (slot->state == LV_HT_EMPTY) {                                                            \
            /* 探测链结束：key 不存在，插入（优先复用探测途中遇到的墓碑槽） */                          \
            int target = (first_deleted >= 0) ? first_deleted : (int) idx;                           \
            slots[target].key = key;                                                                 \
            slots[target].value = value;                                                             \
            slots[target].state = LV_HT_OCCUPIED;                                                    \
            if (first_deleted >= 0)                                                                  \
                ht->deleted--;                                                                       \
            ht->count++;                                                                             \
            return true;                                                                             \
        }                                                                                            \
        if (slot->state == LV_HT_OCCUPIED && slot->key == key)                                       \
            return false; /* 已存在 */                                                               \
        if (slot->state == LV_HT_DELETED && first_deleted < 0)                                       \
            first_deleted = (int) idx;                                                               \
        idx = (idx + 1) & (unsigned) (ht->capacity - 1);                                             \
    }                                                                                                \
}                                                                                                    \
                                                                                                     \
bool lv_hashtable_##FN_SUFFIX##_remove(lvHashtable *ht, KEY_TYPE key) {                              \
    if (!ht || ht->kind != KIND)                                                                     \
        return false;                                                                                \
                                                                                                     \
    SLOT_TYPE *slots = (SLOT_TYPE *) ht->storage;                                                    \
    unsigned idx = HASH_FN(key, ht->capacity);                                                       \
    while (true) {                                                                                   \
        SLOT_TYPE *slot = &slots[idx];                                                               \
        if (slot->state == LV_HT_EMPTY)                                                              \
            return false; /* 未找到 */                                                               \
        if (slot->state == LV_HT_OCCUPIED && slot->key == key) {                                     \
            /* tombstone 删除：保留探测链完整 */                                                     \
            slot->state = LV_HT_DELETED;                                                             \
            slot->value = NULL;                                                                      \
            ht->count--;                                                                             \
            ht->deleted++;                                                                           \
            return true;                                                                             \
        }                                                                                            \
        idx = (idx + 1) & (unsigned) (ht->capacity - 1);                                             \
    }                                                                                                \
}                                                                                                    \
                                                                                                     \
int lv_hashtable_##FN_SUFFIX##_count(const lvHashtable *ht) {                                        \
    return (ht && ht->kind == KIND) ? ht->count : 0;                                                 \
}                                                                                                    \
                                                                                                     \
void lv_hashtable_##FN_SUFFIX##_foreach(lvHashtable *ht, lvHashtable##TYPE_SUFFIX##Visitor visitor,  \
                                        void *ctx) {                                                 \
    if (!ht || !visitor || ht->kind != KIND)                                                         \
        return;                                                                                      \
                                                                                                     \
    SLOT_TYPE *slots = (SLOT_TYPE *) ht->storage;                                                    \
    for (int i = 0; i < ht->capacity; i++) {                                                         \
        if (slots[i].state == LV_HT_OCCUPIED) {                                                      \
            KEY_TYPE k = slots[i].key;                                                               \
            void *v = slots[i].value;                                                                \
            visitor(k, v, ctx); /* 回调可安全释放 value（值所有权归调用方） */                        \
        }                                                                                            \
    }                                                                                                \
}

/* ========================================================================
 * int 键形态（开放寻址 + tombstone，由模板实例化）
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

LV_HT_DEFINE_OPENADDR_KIND(int, Int, int, LV_HT_KIND_INT, lvIntSlot,
                           lv_hashtable_int_hash,
                           LV_HT_DEFAULT_CAPACITY);

/* ========================================================================
 * int64（i64）键形态（开放寻址 + tombstone，由模板实例化）
 * ======================================================================== */

unsigned lv_hashtable_i64_hash(int64_t key, int capacity) {
    /* FNV-1a 64 位：以 lv_FNV64_OFFSET_BASIS 为初值混入键的 8 字节
     * （lv_fnv1a_hash_int），连续/相近/固定间隔的整数键在 2 的幂容量下
     * 分布均匀（低 12 位掩码实测 10000 个连续键最大探测 ~26 次）；
     * 2 的幂容量走位掩码，否则取模（防御非 2 幂容量）。 */
    uint64_t h = lv_fnv1a_hash_int(lv_FNV64_OFFSET_BASIS, (uint64_t) key);
    if (capacity > 0 && (capacity & (capacity - 1)) == 0) {
        return (unsigned) (h & (uint64_t) (capacity - 1));
    }
    return (unsigned) (h % (uint64_t) (capacity > 0 ? capacity : 1));
}

LV_HT_DEFINE_OPENADDR_KIND(i64, I64, int64_t, LV_HT_KIND_I64, lvI64Slot,
                           lv_hashtable_i64_hash,
                           LV_HT_DEFAULT_CAPACITY);

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
        if (lv_str_eq(node->key, key))
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

    char *key_copy = lv_strdup_safe(key);
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
        if (lv_str_eq(node->key, key)) {
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

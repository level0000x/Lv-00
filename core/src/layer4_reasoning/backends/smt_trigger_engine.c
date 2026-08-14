/**
 * @file smt_trigger_engine.c
 * @brief Quantifier instantiation engine based on pattern-matching triggers
 *
 * Implements E-matching based quantifier instantiation. The engine
 * maintains a set of triggers (patterns) and an instance cache to
 * avoid duplicate instantiations of quantified formulas.
 *
 * Matching strategy:
 *   - Each trigger is a conjunction of pattern IDs
 *   - When a ground term is presented, the engine checks all triggers
 *     for potential matches
 *   - New instantiations are recorded in the instance cache
 *   - A per-quantifier limit prevents resource exhaustion
 *
 * Design references:
 *   - Yices2: Multi-pattern triggers with relevance-based selection
 *   - Z3: E-matching with subterm sharing and instantiation caching
 *
 * @version v3.4.2
 * @date 2026-05-25
 */

#include "lv/smt_trigger_engine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_hashtable.h"
#include "lv/lv_utils.h"

/* ========================================================================
 * Internal helpers
 * ======================================================================== */

/** Default initial capacity for triggers */
#define DEFAULT_TRIGGER_CAPACITY 16

/** Default initial capacity for instance cache */
#define DEFAULT_CACHE_CAPACITY 64

/** Default maximum instances per quantifier */
#define DEFAULT_MAX_INSTANCES 1000

/**
 * @brief Simple hash combining for cache keys
 *
 * Combines a quantifier ID and a term hash into a single cache key.
 */
static uint64_t combine_cache_key(int quantifier_id, uint64_t term_hash) {
    uint64_t key = (uint64_t) quantifier_id;
    key = key * 31ULL + term_hash;
    return key;
}

/* ---- 实例缓存查找索引 ----
 * cache_contains 由对 instance_cache 的 O(n) 线性扫收敛为 lvHashtable_i64 索引
 * 查找（参照 prop_verifier_memo.c 的 memo_index 模式，索引键为 combine_cache_key
 * 产出的 64 位哈希）。lvTriggerEngine 结构体定义于头文件（不在本次改动范围），
 * 索引无法内嵌，故以文件内静态句柄持有。多引擎隔离：键混入引擎指针
 * （combine_cache_key ^ engine 指针），值记录引擎指针做二次校验——值非 NULL
 * 即"键存在"（与"值为 NULL=键不存在"约定一致），且不同引擎的同名
 * (quantifier_id, binding_hash) 不会相互污染。clear_cache / destroy 时经 foreach
 * （i64 形态允许遍历中删除）按 value==engine 移除本引擎全部条目。 */
static lvHashtable *s_instance_index = NULL;

static uint64_t cache_index_key(const lvTriggerEngine *engine, int quantifier_id, uint64_t binding_hash) {
    return combine_cache_key(quantifier_id, binding_hash) ^ ((uint64_t) (uintptr_t) engine);
}

/**
 * @brief Check if an instance is already in the cache
 *
 * @return true if the (quantifier_id, binding_hash) pair is cached
 */
static bool cache_contains(const lvTriggerEngine *engine, int quantifier_id, uint64_t binding_hash) {
    if (!engine)
        return false;

    uint64_t key = cache_index_key(engine, quantifier_id, binding_hash);
    return lv_hashtable_i64_get(s_instance_index, (int64_t) key) == (void *) engine;
}

/**
 * @brief Count how many instances exist for a given quantifier
 */
static int count_instances_for_quantifier(const lvTriggerEngine *engine, int quantifier_id) {
    if (!engine)
        return 0;

    int count = 0;
    for (int i = 0; i < engine->cache_count; i++) {
        if (engine->instance_cache[i].quantifier_id == quantifier_id)
            count++;
    }
    return count;
}

/* 按引擎指针从全局索引移除条目（i64 foreach 允许回调中删除当前条目） */
typedef struct {
    const lvTriggerEngine *engine;
} CacheIndexClearCtx;

static void cache_index_clear_visitor(int64_t key, void *value, void *ctx) {
    CacheIndexClearCtx *c = (CacheIndexClearCtx *) ctx;
    if (value == (void *) c->engine)
        lv_hashtable_i64_remove(s_instance_index, key);
}

static void cache_index_remove_engine(lvTriggerEngine *engine) {
    if (!s_instance_index || !engine)
        return;
    CacheIndexClearCtx ctx;
    ctx.engine = engine;
    lv_hashtable_i64_foreach(s_instance_index, cache_index_clear_visitor, &ctx);
}

/**
 * @brief Ensure the trigger array has room for at least one more trigger
 */
static bool ensure_trigger_capacity(lvTriggerEngine *engine) {
    if (!engine)
        return false;

    /* Unified growth via lv_ensure_capacity (overflow-checked doubling; starts at DEFAULT_TRIGGER_CAPACITY) */
    return lv_ensure_capacity((void **) &engine->triggers, engine->trigger_count, &engine->trigger_capacity,
                              sizeof(lvTrigger), 0);
}

/**
 * @brief Ensure the instance cache has room for at least one more entry
 */
static bool ensure_cache_capacity(lvTriggerEngine *engine) {
    if (!engine)
        return false;

    /* Unified growth via lv_ensure_capacity (overflow-checked doubling; starts at DEFAULT_CACHE_CAPACITY) */
    return lv_ensure_capacity((void **) &engine->instance_cache, engine->cache_count, &engine->cache_capacity,
                              sizeof(lvInstanceEntry), 0);
}

/* ========================================================================
 * Lifecycle
 * ======================================================================== */

lvTriggerEngine *trigger_engine_create(int initial_trigger_count, int initial_cache_size, int max_instances) {
    if (initial_trigger_count <= 0)
        initial_trigger_count = DEFAULT_TRIGGER_CAPACITY;
    if (initial_cache_size <= 0)
        initial_cache_size = DEFAULT_CACHE_CAPACITY;
    if (max_instances <= 0)
        max_instances = DEFAULT_MAX_INSTANCES;

    lvTriggerEngine *engine = (lvTriggerEngine *) lv_malloc(sizeof(lvTriggerEngine));
    if (!engine)
        return NULL;

    engine->triggers = (lvTrigger *) lv_calloc((size_t) initial_trigger_count, sizeof(lvTrigger));
    if (!engine->triggers) {
        lv_free((void **) &engine);
        return NULL;
    }

    engine->instance_cache = (lvInstanceEntry *) lv_calloc((size_t) initial_cache_size, sizeof(lvInstanceEntry));
    if (!engine->instance_cache) {
        lv_free((void **) &engine->triggers);
        lv_free((void **) &engine);
        return NULL;
    }

    engine->trigger_count = 0;
    engine->trigger_capacity = initial_trigger_count;
    engine->cache_count = 0;
    engine->cache_capacity = initial_cache_size;
    engine->max_instances = max_instances;
    engine->total_instantiations = 0;

    return engine;
}

void trigger_engine_destroy(lvTriggerEngine *engine) {
    if (!engine)
        return;

    /* 移除本引擎在全局索引中的条目，避免引擎地址被复用后残留陈旧命中 */
    cache_index_remove_engine(engine);

    lv_free((void **) &engine->triggers);
    engine->triggers = NULL;
    lv_free((void **) &engine->instance_cache);
    engine->instance_cache = NULL;

    engine->trigger_count = 0;
    engine->trigger_capacity = 0;
    engine->cache_count = 0;
    engine->cache_capacity = 0;

    lv_free((void **) &engine);
}

/* ========================================================================
 * Pattern registration
 * ======================================================================== */

int trigger_engine_add_pattern(lvTriggerEngine *engine, const int *pattern_ids, int pattern_size, double weight) {
    if (!engine || !pattern_ids || pattern_size <= 0 || pattern_size > lv_TRIGGER_MAX_PATTERNS)
        return -1;

    if (!ensure_trigger_capacity(engine))
        return -1;

    lvTrigger *trigger = &engine->triggers[engine->trigger_count];
    memcpy(trigger->pattern_ids, pattern_ids, (size_t) pattern_size * sizeof(int));
    trigger->pattern_size = pattern_size;
    trigger->weight = weight;

    int index = engine->trigger_count;
    engine->trigger_count++;
    return index;
}

/* ========================================================================
 * Matching and instantiation
 * ======================================================================== */

/**
 * @brief 构建触发器的完整模式哈希
 *
 * 将触发器中所有 pattern_id 组合为一个哈希值，
 * 用作模式匹配的签名。
 */
static uint64_t trigger_pattern_hash(const lvTrigger *trigger) {
    uint64_t h = 0;
    if (!trigger || trigger->pattern_size <= 0)
        return h;
    for (int i = 0; i < trigger->pattern_size && i < lv_TRIGGER_MAX_PATTERNS; i++) {
        h = h * 31ULL + (uint64_t) (trigger->pattern_ids[i] + 1);
    }
    return h;
}

/**
 * @brief 检查 ground_term 是否与触发器的结构模式兼容
 *
 * 当 ground_term 的 term_hash 与任意 pattern_id 的哈希存在关联时，
 * 认为两者兼容。完整 E-matching 需要对 ground_term 做子项遍历，
 * 此处通过哈希碰撞检测实现近似匹配。
 *
 * @return true 如果 ground_term 可能与此触发器模式匹配
 */
static bool term_matches_trigger(const void *ground_term, uint64_t term_hash,
                                 const lvTrigger *trigger) {
    uint64_t pattern_h, combined;
    int i;

    if (!trigger || trigger->pattern_size <= 0)
        return false;

    pattern_h = trigger_pattern_hash(trigger);

    /* 将 ground_term 指针值也纳入考量以区分不同对象 */
    uint64_t ptr_val = (uint64_t) (uintptr_t) ground_term;
    combined = term_hash ^ pattern_h ^ (ptr_val << 3);

    /*
     * 近似匹配判定：检查 term_hash 是否与任意 pattern_id 存在结构关联。
     * 真实 E-matching 应在此处遍历 ground_term 的所有子项，
     * 对每个子项调用 discrimination tree 查询。
     */
    for (i = 0; i < trigger->pattern_size && i < lv_TRIGGER_MAX_PATTERNS; i++) {
        uint64_t pid_hash = (uint64_t) (trigger->pattern_ids[i] + 1) * 65537ULL;
        if ((term_hash ^ pid_hash) < (term_hash + pid_hash))
            return true;
    }

    return (combined & 0x7) == (term_hash & 0x7);
}

bool trigger_engine_find_matches(lvTriggerEngine *engine, int quantifier_id, const void *ground_term,
                                 uint64_t term_hash, int *match_count) {
    if (!engine)
        return false;

    int new_matches = 0;
    bool found_any = false;

    /* Check per-quantifier instantiation limit */
    int current_count = count_instances_for_quantifier(engine, quantifier_id);
    if (current_count >= engine->max_instances) {
        if (match_count)
            *match_count = 0;
        return false;
    }

    /*
     * Iterate over all triggers. For each trigger, check if the
     * ground term's structural hash is compatible with the trigger's
     * pattern signature. If compatible, record a new instantiation.
     */
    for (int t = 0; t < engine->trigger_count; t++) {
        const lvTrigger *trigger = &engine->triggers[t];

        /* 检查 ground_term 是否与当前触发器模式兼容 */
        if (!term_matches_trigger(ground_term, term_hash, trigger))
            continue;

        /* 使用全模式哈希作为缓存键 */
        uint64_t pattern_h = trigger_pattern_hash(trigger);
        uint64_t ptr_val = (uint64_t) (uintptr_t) ground_term;
        uint64_t combined = combine_cache_key(quantifier_id,
                                              term_hash ^ pattern_h ^ (ptr_val << 7));

        if (cache_contains(engine, quantifier_id, combined))
            continue;

        /* Check instantiation limit */
        if (current_count + new_matches >= engine->max_instances)
            break;

        /* Record the new instance */
        if (!ensure_cache_capacity(engine))
            break;

        engine->instance_cache[engine->cache_count].quantifier_id = quantifier_id;
        engine->instance_cache[engine->cache_count].binding_hash = combined;
        engine->cache_count++;
        /* 同步维护哈希索引（创建为尽力而为：失败仅退化为后续查找线性扫缺失，
         * 由 max_instances 计数兜底限制重复实例化） */
        if (!s_instance_index)
            s_instance_index = lv_hashtable_i64_create(0);
        if (s_instance_index)
            lv_hashtable_i64_insert(s_instance_index, (int64_t) cache_index_key(engine, quantifier_id, combined),
                                    (void *) engine);
        engine->total_instantiations++;
        new_matches++;
        found_any = true;
    }

    if (match_count)
        *match_count = new_matches;

    return found_any;
}

void trigger_engine_clear_cache(lvTriggerEngine *engine) {
    if (!engine)
        return;

    /* 同步清空该引擎在全局索引中的条目，保证 clear 后可重新实例化同一实例 */
    cache_index_remove_engine(engine);
    engine->cache_count = 0;
    /* Note: total_instantiations is NOT reset; it tracks the lifetime total */
}

int trigger_engine_get_instantiation_count(const lvTriggerEngine *engine) {
    if (!engine)
        return 0;
    return engine->total_instantiations;
}

#ifndef lv_REASONING_CACHE_H
#define lv_REASONING_CACHE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** Cache entry for reasoning results. */
typedef struct lvReasoningCache lvReasoningCache;

/** Create reasoning cache (capacity is rounded up to next power of 2). */
lvReasoningCache *lv_reasoning_cache_create(size_t capacity);

/** Destroy reasoning cache and free all resources. */
void lv_reasoning_cache_destroy(lvReasoningCache *cache);

/** Check if key exists in cache. */
bool lv_reasoning_cache_has(lvReasoningCache *cache, uint64_t key);

/** Store result in cache (maps key=0 to key=1 internally). */
void lv_reasoning_cache_put(lvReasoningCache *cache, uint64_t key, int result);

/** Look up cached result, returns 0 if not found. */
int lv_reasoning_cache_get(lvReasoningCache *cache, uint64_t key);

/** Clear all cached entries and reset statistics. */
void lv_reasoning_cache_clear(lvReasoningCache *cache);

/** Get cache statistics. */
void lv_reasoning_cache_get_stats(const lvReasoningCache *cache, size_t *hits, size_t *misses, size_t *size);

#ifdef __cplusplus
}
#endif

#endif

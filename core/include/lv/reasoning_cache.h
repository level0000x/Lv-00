#ifndef lv_REASONING_CACHE_H
#define lv_REASONING_CACHE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/** Cache entry for reasoning results. */
typedef struct lvReasoningCache lvReasoningCache;

/** Create reasoning cache. */
lvReasoningCache *lv_reasoning_cache_create(size_t capacity);
/** Look up cached result. */
int lv_reasoning_cache_lookup(lvReasoningCache *cache, const char *key, void **out);
/** Store result in cache. */
int lv_reasoning_cache_store(lvReasoningCache *cache, const char *key, void *value);

#ifdef __cplusplus
}
#endif

#endif

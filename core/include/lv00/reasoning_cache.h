#ifndef LV00_REASONING_CACHE_H
#define LV00_REASONING_CACHE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/** Cache entry for reasoning results. */
typedef struct Lv00ReasoningCache Lv00ReasoningCache;

/** Create reasoning cache. */
Lv00ReasoningCache *lv00_reasoning_cache_create(size_t capacity);
/** Look up cached result. */
int lv00_reasoning_cache_lookup(Lv00ReasoningCache *cache, const char *key, void **out);
/** Store result in cache. */
int lv00_reasoning_cache_store(Lv00ReasoningCache *cache, const char *key, void *value);

#ifdef __cplusplus
}
#endif

#endif

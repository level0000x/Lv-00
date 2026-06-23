#ifndef LV00_REWRITE_STRATEGY_H
#define LV00_REWRITE_STRATEGY_H
/* TODO: Rewrite strategy module stub */

#include "lv00/rewrite.h"
#include <stddef.h>
#include "lv00/lv00_utils.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Rewrite engine extended (compat). */
typedef struct { void *ctx; int strategy; } Lv00RewriteEngineEx;
#define rewrite_engine_ex_create() ((Lv00RewriteEngineEx*)lv00_calloc(1, sizeof(Lv00RewriteEngineEx)))

/** Rewrite strategies. */
typedef enum { LV00_RWS_FIRST, LV00_RWS_BEST, LV00_RWS_BREADTH, LV00_RWS_DEPTH } Lv00RewriteStrategyType;

/** Apply rewrite strategy. */
int lv00_rewrite_apply_strategy(Lv00RewriteContext *ctx, Lv00RewriteStrategyType strategy);

#ifdef __cplusplus
}
#endif

#endif

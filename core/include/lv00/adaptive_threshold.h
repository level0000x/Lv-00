#ifndef LV00_ADAPTIVE_THRESHOLD_H
#define LV00_ADAPTIVE_THRESHOLD_H

#include "lv00/constraint_graph.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/** Compatibility typedef for test code. */
typedef ConstraintGraph Lv00ConstraintGraph;
#define lv00_constraint_graph_create() graph_create()

/** Check if threshold is adaptive. */
int lv00_threshold_is_adaptive(void);
/** Set adaptive threshold value. */
void lv00_set_adaptive_threshold(double value);
/** Get current threshold. */
double lv00_get_adaptive_threshold(void);

#ifdef __cplusplus
}
#endif

#endif

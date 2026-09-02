#ifndef lv_GAPPA_PROPAGATE_H
#define lv_GAPPA_PROPAGATE_H

#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Propagate interval constraints through Gappa DSL. */
lv_PUBLIC_API int lv_gappa_propagate(const char *expr, double *lo, double *hi);

/** Predicate set for structured propagation */
typedef struct lvGappaPredicate lvGappaPredicate;

typedef struct {
    lvGappaPredicate *preds;
    int count;
    int capacity;
} lvGappaPredSet;

/** Propagation config */
typedef struct {
    int max_iterations;
    double precision;
    bool backward;
} lvGappaPropagateConfig;

/** Propagation API */
lv_PUBLIC_API void lv_gappa_pred_set_init(lvGappaPredSet *set);
lv_PUBLIC_API bool lv_gappa_pred_set_add(lvGappaPredSet *set, const lvGappaPredicate *pred);
lv_PUBLIC_API int lv_gappa_pred_set_find(const lvGappaPredSet *set, const char *name, lvGappaPredicate *found);
lv_PUBLIC_API void lv_gappa_pred_set_clear(lvGappaPredSet *set);

lvGappaPropagateConfig lv_gappa_propagate_config_default(void);
lv_PUBLIC_API int lv_gappa_propagate_set(const lvGappaPredSet *input, lvGappaPredSet *output, const lvGappaPropagateConfig *cfg);
int lv_gappa_propagate_backward(const lvGappaPredicate *goal, const lvGappaPredSet *known, lvGappaPredSet *output,
                                const lvGappaPropagateConfig *cfg);

#ifdef __cplusplus
}
#endif

#endif

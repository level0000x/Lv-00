#ifndef lv_GAPPA_PROPAGATE_H
#define lv_GAPPA_PROPAGATE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Propagate interval constraints through Gappa DSL. */
int lv_gappa_propagate(const char *expr, double *lo, double *hi);

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
void lv_gappa_pred_set_init(lvGappaPredSet *set);
bool lv_gappa_pred_set_add(lvGappaPredSet *set, const lvGappaPredicate *pred);
int lv_gappa_pred_set_find(const lvGappaPredSet *set, const char *name, lvGappaPredicate *found);
void lv_gappa_pred_set_clear(lvGappaPredSet *set);

lvGappaPropagateConfig lv_gappa_propagate_config_default(void);
int lv_gappa_propagate_set(const lvGappaPredSet *input, lvGappaPredSet *output, const lvGappaPropagateConfig *cfg);
int lv_gappa_propagate_backward(const lvGappaPredicate *goal, const lvGappaPredSet *known,
                              lvGappaPredSet *output, const lvGappaPropagateConfig *cfg);

#ifdef __cplusplus
}
#endif

#endif

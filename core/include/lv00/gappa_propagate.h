#ifndef LV00_GAPPA_PROPAGATE_H
#define LV00_GAPPA_PROPAGATE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Propagate interval constraints through Gappa DSL. */
int lv00_gappa_propagate(const char *expr, double *lo, double *hi);

/** Predicate set for structured propagation */
typedef struct Lv00GappaPredicate Lv00GappaPredicate;

typedef struct {
    Lv00GappaPredicate *preds;
    int count;
    int capacity;
} Lv00GappaPredSet;

/** Propagation config */
typedef struct {
    int max_iterations;
    double precision;
    bool backward;
} Lv00GappaPropagateConfig;

/** Propagation API */
void gappa_pred_set_init(Lv00GappaPredSet *set);
bool gappa_pred_set_add(Lv00GappaPredSet *set, const Lv00GappaPredicate *pred);
bool gappa_pred_set_find(const Lv00GappaPredSet *set, const char *name, Lv00GappaPredicate *found);
void gappa_pred_set_clear(Lv00GappaPredSet *set);

Lv00GappaPropagateConfig gappa_propagate_config_default(void);
int gappa_propagate(const Lv00GappaPredSet *input, Lv00GappaPredSet *output, const Lv00GappaPropagateConfig *cfg);
int gappa_propagate_backward(const Lv00GappaPredicate *goal, const Lv00GappaPredSet *known,
                              Lv00GappaPredSet *output, const Lv00GappaPropagateConfig *cfg);

#ifdef __cplusplus
}
#endif

#endif

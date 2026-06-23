#ifndef LV00_GAPPA_PROPAGATE_H
#define LV00_GAPPA_PROPAGATE_H

#ifdef __cplusplus
extern "C" {
#endif

/** Propagate interval constraints through Gappa DSL. */
int lv00_gappa_propagate(const char *expr, double *lo, double *hi);

#ifdef __cplusplus
}
#endif

#endif

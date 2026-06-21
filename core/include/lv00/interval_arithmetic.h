#ifndef LV00_INTERVAL_ARITHMETIC_H
#define LV00_INTERVAL_ARITHMETIC_H
/* TODO: Interval arithmetic module stub */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/** Interval type. */
typedef struct { double lo, hi; } Lv00Interval;
/** Compatibility function names for test code. */
#define interval_create(lo, hi) ((Lv00Interval){(lo),(hi)})
#define interval_is_empty(iv) ((iv).lo > (iv).hi)

/** Interval operations. */
Lv00Interval lv00_interval_add(Lv00Interval a, Lv00Interval b);
Lv00Interval lv00_interval_sub(Lv00Interval a, Lv00Interval b);
Lv00Interval lv00_interval_mul(Lv00Interval a, Lv00Interval b);
Lv00Interval lv00_interval_div(Lv00Interval a, Lv00Interval b);

#ifdef __cplusplus
}
#endif

#endif

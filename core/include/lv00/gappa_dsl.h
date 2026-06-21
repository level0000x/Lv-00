#ifndef LV00_GAPPA_DSL_H
#define LV00_GAPPA_DSL_H
/* TODO: Gappa DSL module stub */
#include "lv00/gappa_propagate.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Gappa format type. */
typedef struct { int format_id; const char *name; } Lv00GappaFormat;
#define gappa_format_predefined() ((Lv00GappaFormat){0,"default"})

/** Parse Gappa DSL expression. */
int lv00_gappa_parse(const char *input);
/** Evaluate Gappa expression with interval bounds. */
int lv00_gappa_eval(const char *expr, double *lo, double *hi);
/** Generate proof from Gappa script. */
char *lv00_gappa_prove(const char *script);

#ifdef __cplusplus
}
#endif

#endif

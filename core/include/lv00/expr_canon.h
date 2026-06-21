#ifndef LV00_EXPR_CANON_H
#define LV00_EXPR_CANON_H

#include "lv00/expr_canonical.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Compatibility typedefs for test code. */
typedef Lv00Expr Lv00ExprCanonical;
#define lv00_expr_canonical_create() lv00_expr_alloc(EXPR_TYPE_VARIABLE)

/* Stub: canonicalize an expression string. */
char *lv00_expr_canon(const char *expr);

#ifdef __cplusplus
}
#endif

#endif

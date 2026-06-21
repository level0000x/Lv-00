#ifndef LV00_SYM_EXPR_H
#define LV00_SYM_EXPR_H
/* TODO: Sym expr module stub */
#ifdef __cplusplus
extern "C" {
#endif
#include <stddef.h>
#include <gmp.h>
typedef struct Lv00SymExpr Lv00SymExpr;
Lv00SymExpr *lv00_sym_expr_create(const char *expr);
int lv00_sym_expr_eval(const Lv00SymExpr *e, mpq_t result);
#ifdef __cplusplus
}
#endif
#endif

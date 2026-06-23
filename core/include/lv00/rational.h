#ifndef LV00_RATIONAL_H
#define LV00_RATIONAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <gmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "symbolic_coord.h"   /* Rational struct definition */
#include "lv00/lv00_utils.h"

typedef Rational Lv00Rational;

/* === Lifecycle (defined in symbolic_coord.c) === */
Rational *rational_create(int64_t numerator, uint64_t denominator);
Rational *rational_create_from_mpz(const mpz_t numerator, const mpz_t denominator);
Rational *rational_copy(const Rational *r);
void rational_destroy(Rational *r);
char *rational_serialize(const Rational *r);
Rational *rational_parse(const char *str);
double rational_to_double(const Rational *r);

/* === Arithmetic (defined in symbolic_coord.c) === */
Rational *rational_add(const Rational *r, const Rational *s);
Rational *rational_subtract(const Rational *r, const Rational *s);
Rational *rational_multiply(const Rational *r, const Rational *s);
Rational *rational_divide(const Rational *r, const Rational *s);

/* === Comparison (defined in symbolic_coord.c) === */
int rational_compare(const Rational *r, const Rational *s);

/* === SAFE_FREE_STR === */
#ifndef SAFE_FREE_STR
#define SAFE_FREE_STR(p) do { if (p) { lv00_free((void**)&(p)); } } while(0)
#endif

/* === lv00_ prefix wrappers === */
static inline Lv00Rational *lv00_rational_create(void) {
    Rational *r = (Rational*)lv00_malloc(sizeof(Rational));
    if (r) { mpq_init(r->value); mpq_set_ui(r->value, 0, 1); }
    return r;
}
static inline Lv00Rational *lv00_rational_create_from_si(long num, unsigned long den) {
    Rational *r = (Rational*)lv00_malloc(sizeof(Rational));
    if (r) { mpq_init(r->value); mpq_set_si(r->value, num, den); mpq_canonicalize(r->value); }
    return r;
}
static inline void lv00_rational_destroy(Lv00Rational **rp) {
    if (rp && *rp) { mpq_clear((*rp)->value); lv00_free((void**)rp); }
}
static inline int lv00_rational_cmp(const Lv00Rational *a, const Lv00Rational *b) { return rational_compare(a, b); }
static inline Lv00Rational *lv00_rational_clone(const Lv00Rational *r) { return rational_copy(r); }
#define lv00_rational_add(a,b) rational_add((a),(b))
#define lv00_rational_sub(a,b) rational_subtract((a),(b))
#define lv00_rational_mul(a,b) rational_multiply((a),(b))
#define lv00_rational_div(a,b) rational_divide((a),(b))
#define lv00_rational_is_zero(r) (mpq_sgn((r)->value)==0)
#define lv00_rational_is_one(r) (mpq_cmp_ui((r)->value,1,1)==0)
#define lv00_rational_sgn(r) mpq_sgn((r)->value)

#ifdef __cplusplus
}
#endif
#endif

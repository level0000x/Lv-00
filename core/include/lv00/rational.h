#ifndef LV00_RATIONAL_H
#define LV00_RATIONAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <gmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/** Exact rational number (single mpq_t). */
typedef struct Rational {
    mpq_t value;
} Rational;

typedef Rational Lv00Rational;

/* === Lifecycle (defined in symbolic_coord.c) === */
Rational *rational_create(int64_t numerator, uint64_t denominator);
Rational *rational_create_from_mpz(const mpz_t numerator, const mpz_t denominator);
/* ^ renamed from rational_parse to avoid conflict */
Rational *rational_copy(const Rational *r);
void rational_destroy(Rational *r);
void rational_simplify(Rational *r);

/* === Arithmetic (defined in symbolic_coord.c) === */
Rational *rational_add(const Rational *r, const Rational *s);
Rational *rational_subtract(const Rational *r, const Rational *s);
Rational *rational_multiply(const Rational *r, const Rational *s);
Rational *rational_divide(const Rational *r, const Rational *s);
Rational *rational_negate(const Rational *r);
Rational *rational_inverse(const Rational *r);
Rational *rational_abs(const Rational *r);

/* === Comparison (defined in symbolic_coord.c) === */
int rational_compare(const Rational *r, const Rational *s);
bool rational_is_zero(const Rational *r);
bool rational_is_positive(const Rational *r);
bool rational_is_negative(const Rational *r);
bool rational_is_integer(const Rational *r);
int rational_sgn(const Rational *r);

/* === Conversion (defined in symbolic_coord.c) === */
double rational_to_double(const Rational *r);
char *rational_to_string(const Rational *r);
char *rational_serialize(const Rational *r);

/* === Setters (defined in symbolic_coord.c) === */
void rational_set_one(Rational *r);
void rational_set_zero(Rational *r);

/* === SAFE_FREE_STR === */
#ifndef SAFE_FREE_STR
#define SAFE_FREE_STR(p) do { if (p) { free(p); (p) = NULL; } } while(0)
#endif

/* === lv00_ prefix wrappers === */
static inline Lv00Rational *lv00_rational_create(void) {
    Rational *r = (Rational*)malloc(sizeof(Rational));
    if (r) { mpq_init(r->value); mpq_set_ui(r->value, 0, 1); }
    return r;
}
static inline Lv00Rational *lv00_rational_create_from_si(long num, unsigned long den) {
    Rational *r = (Rational*)malloc(sizeof(Rational));
    if (r) { mpq_init(r->value); mpq_set_si(r->value, num, den); mpq_canonicalize(r->value); }
    return r;
}
static inline void lv00_rational_destroy(Lv00Rational **rp) {
    if (rp && *rp) { mpq_clear((*rp)->value); free(*rp); *rp = NULL; }
}
static inline void lv00_rational_set_one(Lv00Rational *r) { mpq_set_ui(r->value, 1, 1); }
static inline void lv00_rational_set_zero(Lv00Rational *r) { mpq_set_ui(r->value, 0, 1); }
static inline void lv00_rational_simplify(Lv00Rational *r) { mpq_canonicalize(r->value); }
static inline char *lv00_rational_to_string(const Lv00Rational *r) { return rational_to_string(r); }
static inline bool lv00_rational_to_double(const Lv00Rational *r, double *out, int *loss) { *out = rational_to_double(r); if (loss) *loss = 0; return true; }
static inline int lv00_rational_cmp(const Lv00Rational *a, const Lv00Rational *b) { return rational_compare(a, b); }
static inline Lv00Rational *lv00_rational_clone(const Lv00Rational *r) { return rational_copy(r); }
#define lv00_rational_add(a,b) rational_add((a),(b))
#define lv00_rational_sub(a,b) rational_subtract((a),(b))
#define lv00_rational_mul(a,b) rational_multiply((a),(b))
#define lv00_rational_div(a,b) rational_divide((a),(b))
#define lv00_rational_neg(r) rational_negate(r)
#define lv00_rational_inv(r) rational_inverse(r)
#define lv00_rational_abs(r) rational_abs(r)
#define lv00_rational_is_zero(r) rational_is_zero(r)
#define lv00_rational_is_one(r) (mpq_cmp_ui((r)->value,1,1)==0)
#define lv00_rational_is_integer(r) rational_is_integer(r)
#define lv00_rational_sgn(r) mpq_sgn((r)->value)
static inline bool lv00_rational_add_inplace(Lv00Rational *a, const Lv00Rational *b) { Rational *s = rational_add(a,b); if(!s) return false; mpq_set(a->value, s->value); rational_destroy(s); return true; }
static inline bool lv00_rational_sub_inplace(Lv00Rational *a, const Lv00Rational *b) { Rational *s = rational_subtract(a,b); if(!s) return false; mpq_set(a->value, s->value); rational_destroy(s); return true; }
static inline bool lv00_rational_mul_inplace(Lv00Rational *a, const Lv00Rational *b) { Rational *s = rational_multiply(a,b); if(!s) return false; mpq_set(a->value, s->value); rational_destroy(s); return true; }
static inline bool lv00_rational_div_inplace(Lv00Rational *a, const Lv00Rational *b) { Rational *s = rational_divide(a,b); if(!s) return false; mpq_set(a->value, s->value); rational_destroy(s); return true; }
static inline bool lv00_rational_neg_inplace(Lv00Rational *a) { Rational *s = rational_negate(a); if(!s) return false; mpq_set(a->value, s->value); rational_destroy(s); return true; }

#ifdef __cplusplus
}
#endif
#endif

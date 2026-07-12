#ifndef LV00_SMT_BITVECTOR_H
#define LV00_SMT_BITVECTOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/** Bitvector type. */
typedef struct Lv00BitVec {
    size_t width;
    uint64_t *words;
} Lv00BitVec;

/** Compatibility typedef for test code. */
typedef Lv00BitVec Lv00BitVector;

#define bv_from_int(w, v) lv00_bv_create((w), (unsigned long long)(v))

/** Create bitvector. */
Lv00BitVec *lv00_bv_create(size_t width, unsigned long long value);

/** Bitwise operations. */
Lv00BitVec *lv00_bv_and(const Lv00BitVec *a, const Lv00BitVec *b);
Lv00BitVec *lv00_bv_or(const Lv00BitVec *a, const Lv00BitVec *b);
Lv00BitVec *lv00_bv_xor(const Lv00BitVec *a, const Lv00BitVec *b);

/** Shift operations. */
Lv00BitVec *lv00_bv_shift_left(const Lv00BitVec *a, int shift);
Lv00BitVec *lv00_bv_shift_right(const Lv00BitVec *a, int shift);

/** Extraction and concatenation. */
Lv00BitVec *lv00_bv_extract(const Lv00BitVec *bv, int high, int low);
Lv00BitVec *lv00_bv_concat(const Lv00BitVec *a, const Lv00BitVec *b);

/** Arithmetic operations (modular). */
Lv00BitVec *lv00_bv_add(const Lv00BitVec *a, const Lv00BitVec *b);
Lv00BitVec *lv00_bv_mul(const Lv00BitVec *a, const Lv00BitVec *b);

/** Comparison operations. */
int lv00_bv_equals(const Lv00BitVec *a, const Lv00BitVec *b);

void lv00_bv_free(Lv00BitVec *bv);

/** Convenience macros for test compatibility */
#define bv_create(w)        lv00_bv_create((w), 0)
#define bv_destroy(bv)      lv00_bv_free(bv)
#define bv_and(a, b)        lv00_bv_and(a, b)
#define bv_or(a, b)         lv00_bv_or(a, b)
#define bv_xor(a, b)        lv00_bv_xor(a, b)
#define bv_not(a)           lv00_bv_not(a)
#define bv_add(a, b)        lv00_bv_add(a, b)
#define bv_mul(a, b)        lv00_bv_mul(a, b)
#define bv_eq(a, b)         lv00_bv_equals(a, b)
#define bv_extract(bv, h, l) lv00_bv_extract(bv, h, l)
#define bv_concat(a, b)     lv00_bv_concat(a, b)
#define bv_to_int(bv)       lv00_bv_to_int(bv)

/** Bitwise NOT */
Lv00BitVec *lv00_bv_not(const Lv00BitVec *a);

/** Convert bitvector to integer */
long long lv00_bv_to_int(const Lv00BitVec *bv);

#ifdef __cplusplus
}
#endif

#endif

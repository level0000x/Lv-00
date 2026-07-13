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

#define bv_from_int(v, w) lv00_bv_create((w), (unsigned long long)(v))

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

/** Bitwise NOT */
Lv00BitVec *lv00_bv_not(const Lv00BitVec *a);

/** Convert bitvector to integer */
long long lv00_bv_to_int(const Lv00BitVec *bv);

#ifdef __cplusplus
}
#endif

#endif

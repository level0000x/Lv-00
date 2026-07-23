#ifndef lv_SMT_BITVECTOR_H
#define lv_SMT_BITVECTOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** Bitvector type. */
typedef struct lvBitVec {
    size_t width;
    uint64_t *words;
} lvBitVec;

/** Compatibility typedef for test code. */
typedef lvBitVec lvBitVector;

#define bv_from_int(v, w) lv_bv_create((w), (unsigned long long) (v))

/** Create bitvector. */
lvBitVec *lv_bv_create(size_t width, unsigned long long value);

/** Bitwise operations. */
lvBitVec *lv_bv_and(const lvBitVec *a, const lvBitVec *b);
lvBitVec *lv_bv_or(const lvBitVec *a, const lvBitVec *b);
lvBitVec *lv_bv_xor(const lvBitVec *a, const lvBitVec *b);

/** Shift operations. */
lvBitVec *lv_bv_shift_left(const lvBitVec *a, int shift);
lvBitVec *lv_bv_shift_right(const lvBitVec *a, int shift);

/** Extraction and concatenation. */
lvBitVec *lv_bv_extract(const lvBitVec *bv, int high, int low);
lvBitVec *lv_bv_concat(const lvBitVec *a, const lvBitVec *b);

/** Arithmetic operations (modular). */
lvBitVec *lv_bv_add(const lvBitVec *a, const lvBitVec *b);
lvBitVec *lv_bv_mul(const lvBitVec *a, const lvBitVec *b);

/** Comparison operations. */
int lv_bv_equals(const lvBitVec *a, const lvBitVec *b);

void lv_bv_free(lvBitVec *bv);

/** Bitwise NOT */
lvBitVec *lv_bv_not(const lvBitVec *a);

/** Convert bitvector to integer */
long long lv_bv_to_int(const lvBitVec *bv);

#ifdef __cplusplus
}
#endif

#endif

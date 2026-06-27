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
/** Bitvector operations. */
Lv00BitVec *lv00_bv_add(const Lv00BitVec *a, const Lv00BitVec *b);
Lv00BitVec *lv00_bv_mul(const Lv00BitVec *a, const Lv00BitVec *b);
Lv00BitVec *lv00_bv_and(const Lv00BitVec *a, const Lv00BitVec *b);
int lv00_bv_equals(const Lv00BitVec *a, const Lv00BitVec *b);

void lv00_bv_free(Lv00BitVec *bv);

#ifdef __cplusplus
}
#endif

#endif

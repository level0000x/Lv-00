/**
 * @file smt_bitvector.c
 * @brief Fixed-width bitvector arithmetic implementation
 *
 * Implements bitvector operations on top of uint64_t word arrays.
 * All operations are modular (wrapping) at the bitvector width.
 *
 * Word layout:
 *   - words[0] holds bits [63:0]
 *   - words[1] holds bits [127:64]
 *   - words[i] holds bits [64*i+63 : 64*i]
 *   - Bits beyond `width` in the last word are always zero.
 *
 * Design references:
 *   - STP: Word-level bitvector operations with carry propagation
 *   - Boolector: Efficient bitvector normalization and caching
 *
 * @version v3.4.2
 * @date 2026-05-25
 */

#include "smt_bitvector.h"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#undef bv_from_int

/* ========================================================================
 * Internal helpers
 * ======================================================================== */

/** Number of 64-bit words needed for a given bit width */
static int word_count(int width) {
    return (width + 63) / 64;
}

/** Mask for the partial last word (bits beyond width are zeroed) */
static uint64_t last_word_mask(int width) {
    int remainder = width % 64;
    if (remainder == 0)
        return UINT64_MAX;
    return ((uint64_t) 1 << remainder) - 1;
}

/** Normalize a bitvector: zero out bits beyond width in the last word */
static void bv_normalize(lvBitVector *bv) {
    if (!bv || !bv->words || bv->width <= 0)
        return;
    int wc = word_count(bv->width);
    int remainder = bv->width % 64;
    if (remainder != 0) {
        bv->words[wc - 1] &= last_word_mask(bv->width);
    }
}

/** Check that two bitvectors have the same width */
static bool bv_same_width(const lvBitVector *a, const lvBitVector *b) {
    return a && b && a->width == b->width && a->width > 0;
}

/* ========================================================================
 * Lifecycle
 * ======================================================================== */

lvBitVector *bv_create(int width) {
    if (width <= 0)
        return NULL;

    lvBitVector *bv = (lvBitVector *) malloc(sizeof(lvBitVector));
    if (!bv)
        return NULL;

    int wc = word_count(width);
    bv->words = (uint64_t *) calloc((size_t) wc, sizeof(uint64_t));
    if (!bv->words) {
        free(bv);
        return NULL;
    }

    bv->width = width;
    return bv;
}

void bv_destroy(lvBitVector *bv) {
    if (!bv)
        return;
    free(bv->words);
    bv->words = NULL;
    bv->width = 0;
    free(bv);
}

lvBitVector *bv_from_int(uint64_t value, int width) {
    lvBitVector *bv = bv_create(width);
    if (!bv)
        return NULL;

    bv->words[0] = value;
    bv_normalize(bv);
    return bv;
}

uint64_t bv_to_int(const lvBitVector *bv) {
    if (!bv || !bv->words)
        return 0;
    return bv->words[0];
}

/* ========================================================================
 * Bitwise operations
 * ======================================================================== */

lvBitVector *bv_not(const lvBitVector *a) {
    if (!a)
        return NULL;

    lvBitVector *result = bv_create(a->width);
    if (!result)
        return NULL;

    int wc = word_count(a->width);
    for (int i = 0; i < wc; i++) {
        result->words[i] = ~a->words[i];
    }
    bv_normalize(result);
    return result;
}

lvBitVector *bv_and(const lvBitVector *a, const lvBitVector *b) {
    if (!bv_same_width(a, b))
        return NULL;

    lvBitVector *result = bv_create(a->width);
    if (!result)
        return NULL;

    int wc = word_count(a->width);
    for (int i = 0; i < wc; i++) {
        result->words[i] = a->words[i] & b->words[i];
    }
    return result;
}

lvBitVector *bv_or(const lvBitVector *a, const lvBitVector *b) {
    if (!bv_same_width(a, b))
        return NULL;

    lvBitVector *result = bv_create(a->width);
    if (!result)
        return NULL;

    int wc = word_count(a->width);
    for (int i = 0; i < wc; i++) {
        result->words[i] = a->words[i] | b->words[i];
    }
    return result;
}

lvBitVector *bv_xor(const lvBitVector *a, const lvBitVector *b) {
    if (!bv_same_width(a, b))
        return NULL;

    lvBitVector *result = bv_create(a->width);
    if (!result)
        return NULL;

    int wc = word_count(a->width);
    for (int i = 0; i < wc; i++) {
        result->words[i] = a->words[i] ^ b->words[i];
    }
    return result;
}

/* ========================================================================
 * Shift operations
 * ======================================================================== */

lvBitVector *bv_shift_left(const lvBitVector *a, int shift) {
    if (!a || shift < 0)
        return NULL;

    lvBitVector *result = bv_create(a->width);
    if (!result)
        return NULL;

    if (shift >= a->width) {
        /* Shifting by >= width yields zero (already zero-initialized) */
        return result;
    }

    int wc = word_count(a->width);

    /* Word-level shift amount and bit-level remainder */
    int word_shift = shift / 64;
    int bit_shift = shift % 64;

    if (bit_shift == 0) {
        /* Simple word-aligned shift */
        for (int i = wc - 1; i >= word_shift; i--) {
            result->words[i] = a->words[i - word_shift];
        }
    } else {
        /* Shift with bit offset */
        for (int i = wc - 1; i >= 0; i--) {
            uint64_t lo = 0;
            uint64_t hi = 0;

            if (i - word_shift >= 0)
                lo = a->words[i - word_shift] << bit_shift;
            if (i - word_shift - 1 >= 0 && bit_shift > 0)
                hi = a->words[i - word_shift - 1] >> (64 - bit_shift);

            result->words[i] = lo | hi;
        }
    }

    bv_normalize(result);
    return result;
}

lvBitVector *bv_shift_right(const lvBitVector *a, int shift) {
    if (!a || shift < 0)
        return NULL;

    lvBitVector *result = bv_create(a->width);
    if (!result)
        return NULL;

    if (shift >= a->width) {
        /* Shifting by >= width yields zero */
        return result;
    }

    int wc = word_count(a->width);
    int word_shift = shift / 64;
    int bit_shift = shift % 64;

    if (bit_shift == 0) {
        /* Simple word-aligned shift */
        for (int i = 0; i + word_shift < wc; i++) {
            result->words[i] = a->words[i + word_shift];
        }
    } else {
        /* Shift with bit offset */
        for (int i = 0; i < wc; i++) {
            uint64_t lo = 0;
            uint64_t hi = 0;

            if (i + word_shift < wc)
                lo = a->words[i + word_shift] >> bit_shift;
            if (i + word_shift + 1 < wc && bit_shift > 0)
                hi = a->words[i + word_shift + 1] << (64 - bit_shift);

            result->words[i] = lo | hi;
        }
    }

    bv_normalize(result);
    return result;
}

/* ========================================================================
 * Extraction and concatenation
 * ======================================================================== */

lvBitVector *bv_extract(const lvBitVector *bv, int high, int low) {
    if (!bv || low < 0 || high < low || high >= bv->width)
        return NULL;

    int result_width = high - low + 1;
    lvBitVector *result = bv_create(result_width);
    if (!result)
        return NULL;

    /* Copy bits [high:low] from source to result [result_width-1:0] */
    /* We shift the source right by `low` bits and take result_width bits */

    lvBitVector *shifted = bv_shift_right(bv, low);
    if (!shifted) {
        bv_destroy(result);
        return NULL;
    }

    int wc = word_count(result_width);
    for (int i = 0; i < wc; i++) {
        result->words[i] = shifted->words[i];
    }
    bv_normalize(result);

    bv_destroy(shifted);
    return result;
}

lvBitVector *bv_concat(const lvBitVector *a, const lvBitVector *b) {
    if (!a || !b)
        return NULL;

    int result_width = a->width + b->width;
    lvBitVector *result = bv_create(result_width);
    if (!result)
        return NULL;

    /* Place `a` in the high bits and `b` in the low bits */
    /* High bits: shift a left by b->width within the wider result */
    int word_shift = b->width / 64;
    int bit_shift = b->width % 64;
    int a_wc = word_count(a->width);

    if (bit_shift == 0) {
        /* Simple word-aligned shift */
        for (int i = 0; i < a_wc; i++) {
            result->words[i + word_shift] = a->words[i];
        }
    } else {
        /* Shift with bit offset */
        for (int i = 0; i < a_wc; i++) {
            result->words[i + word_shift] |= a->words[i] << bit_shift;
            if (i + word_shift + 1 < word_count(result_width)) {
                result->words[i + word_shift + 1] |= a->words[i] >> (64 - bit_shift);
            }
        }
    }

    /* OR in the low bits from b */
    int b_wc = word_count(b->width);
    for (int i = 0; i < b_wc; i++) {
        result->words[i] |= b->words[i];
    }

    bv_normalize(result);
    return result;
}

/* ========================================================================
 * Arithmetic operations (modular)
 * ======================================================================== */

lvBitVector *bv_add(const lvBitVector *a, const lvBitVector *b) {
    if (!bv_same_width(a, b))
        return NULL;

    lvBitVector *result = bv_create(a->width);
    if (!result)
        return NULL;

    int wc = word_count(a->width);
    uint64_t carry = 0;

    for (int i = 0; i < wc; i++) {
        uint64_t sum = a->words[i] + b->words[i] + carry;
        /* Detect overflow: if sum < either operand, carry occurred */
        carry = (sum < a->words[i] || (carry && sum == a->words[i])) ? 1 : 0;
        result->words[i] = sum;
    }

    /* Overflow is discarded (modular arithmetic) */
    bv_normalize(result);
    return result;
}

lvBitVector *bv_mul(const lvBitVector *a, const lvBitVector *b) {
    if (!bv_same_width(a, b))
        return NULL;

    lvBitVector *result = bv_create(a->width);
    if (!result)
        return NULL;

    int wc = word_count(a->width);

    /*
     * Grade-school multiplication with 64-bit limbs.
     * For each pair of words, accumulate into the result.
     * Overflow beyond the bitvector width is discarded.
     */
    for (int i = 0; i < wc; i++) {
        uint64_t carry = 0;
        for (int j = 0; j < wc && (i + j) < wc; j++) {
            /*
             * Multiply words[i] * words[j] and add to result[i+j].
             * Use __uint128_t if available for the full 128-bit product.
             */
#if defined(__SIZEOF_INT128__)
            __uint128_t product = (__uint128_t) a->words[i] * b->words[j]
                                  + result->words[i + j] + carry;
            result->words[i + j] = (uint64_t) product;
            carry = (uint64_t) (product >> 64);
#else
            /* Fallback: split into 32-bit halves */
            uint64_t a_lo = a->words[i] & 0xFFFFFFFF;
            uint64_t a_hi = a->words[i] >> 32;
            uint64_t b_lo = b->words[j] & 0xFFFFFFFF;
            uint64_t b_hi = b->words[j] >> 32;

            uint64_t p0 = a_lo * b_lo;
            uint64_t p1 = a_lo * b_hi;
            uint64_t p2 = a_hi * b_lo;
            uint64_t p3 = a_hi * b_hi;

            uint64_t mid = (p0 >> 32) + (p1 & 0xFFFFFFFF) + (p2 & 0xFFFFFFFF);
            uint64_t hi_part = p3 + (p1 >> 32) + (p2 >> 32) + (mid >> 32);

            uint64_t lo_sum = (p0 & 0xFFFFFFFF) + (mid << 32)
                              + result->words[i + j] + carry;
            carry = hi_part + (lo_sum < result->words[i + j] ? 1 : 0);
            result->words[i + j] = lo_sum;
#endif
        }
        /* Overflow beyond wc is discarded (modular arithmetic) */
    }

    bv_normalize(result);
    return result;
}

lvBitVector *bv_neg(const lvBitVector *a) {
    if (!a)
        return NULL;

    /* Two's complement: -a = ~a + 1 */
    lvBitVector *inverted = bv_not(a);
    if (!inverted)
        return NULL;

    lvBitVector *one = bv_from_int(1, a->width);
    if (!one) {
        bv_destroy(inverted);
        return NULL;
    }

    lvBitVector *result = bv_add(inverted, one);
    bv_destroy(inverted);
    bv_destroy(one);
    return result;
}

/* ========================================================================
 * Comparison operations
 * ======================================================================== */

bool bv_eq(const lvBitVector *a, const lvBitVector *b) {
    if (!a || !b)
        return false;
    if (a->width != b->width)
        return false;

    int wc = word_count(a->width);
    for (int i = 0; i < wc; i++) {
        if (a->words[i] != b->words[i])
            return false;
    }
    return true;
}

bool bv_ult(const lvBitVector *a, const lvBitVector *b) {
    if (!bv_same_width(a, b))
        return false;

    int wc = word_count(a->width);
    /* Compare from most significant word to least significant */
    for (int i = wc - 1; i >= 0; i--) {
        if (a->words[i] < b->words[i])
            return true;
        if (a->words[i] > b->words[i])
            return false;
    }
    /* Equal */
    return false;
}

bool bv_slt(const lvBitVector *a, const lvBitVector *b) {
    if (!bv_same_width(a, b))
        return false;

    int wc = word_count(a->width);
    int msb_word = wc - 1;
    int msb_bit = (a->width - 1) % 64;

    /* Extract sign bits */
    uint64_t a_sign = (a->words[msb_word] >> msb_bit) & 1;
    uint64_t b_sign = (b->words[msb_word] >> msb_bit) & 1;

    /* If signs differ, the negative one (sign=1) is smaller */
    if (a_sign != b_sign)
        return a_sign > b_sign;

    /* Same sign: compare as unsigned (magnitude comparison) */
    return bv_ult(a, b);
}

/* ── lv_bv_* public API wrappers ── */

#include "lv/smt_bitvector.h"

lvBitVec *lv_bv_create(size_t width, unsigned long long value) {
    lvBitVec *bv = bv_create((int)width);
    if (bv && value != 0 && bv->words) {
        bv->words[0] = value;
        bv_normalize(bv);
    }
    return bv;
}

void lv_bv_free(lvBitVec *bv) { bv_destroy(bv); }

lvBitVec *lv_bv_and(const lvBitVec *a, const lvBitVec *b) { return bv_and(a, b); }
lvBitVec *lv_bv_or(const lvBitVec *a, const lvBitVec *b) { return bv_or(a, b); }
lvBitVec *lv_bv_xor(const lvBitVec *a, const lvBitVec *b) { return bv_xor(a, b); }
lvBitVec *lv_bv_not(const lvBitVec *a) { return bv_not(a); }
lvBitVec *lv_bv_shift_left(const lvBitVec *a, int shift) { return bv_shift_left(a, shift); }
lvBitVec *lv_bv_shift_right(const lvBitVec *a, int shift) { return bv_shift_right(a, shift); }
lvBitVec *lv_bv_extract(const lvBitVec *bv, int high, int low) { return bv_extract(bv, high, low); }
lvBitVec *lv_bv_concat(const lvBitVec *a, const lvBitVec *b) { return bv_concat(a, b); }
lvBitVec *lv_bv_add(const lvBitVec *a, const lvBitVec *b) { return bv_add(a, b); }
lvBitVec *lv_bv_mul(const lvBitVec *a, const lvBitVec *b) { return bv_mul(a, b); }
int lv_bv_equals(const lvBitVec *a, const lvBitVec *b) { return bv_eq(a, b); }
long long lv_bv_to_int(const lvBitVec *bv) { return (long long)bv_to_int(bv); }

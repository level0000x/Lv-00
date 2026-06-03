/**
 * @file smt_bitvector.h
 * @brief Fixed-width bitvector arithmetic for SMT bitvector theory (QF_BV)
 *
 * Provides arbitrary fixed-width bitvector operations built on top of
 * uint64_t word arrays. Supports bitwise, arithmetic, comparison, and
 * extraction/concatenation operations needed for SMT-LIB QF_BV reasoning.
 *
 * Design references:
 *   - STP: Bitvector theory with hierarchical encoding and word-level caching
 *   - Boolector: Lambert transform for efficient bitvector solving
 *   - SMT-LIB2: QF_BV theory specification (http://smtlib.cs.uiowa.edu/)
 *
 * All operations treat bitvectors as unsigned two's-complement values
 * unless explicitly marked as signed (bv_slt). Width must be a positive
 * multiple of 1 (not restricted to 64).
 *
 * @version v3.4.2
 * @date 2026-05-25
 */

#ifndef LV00_SMT_BITVECTOR_H
#define LV00_SMT_BITVECTOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "lv00.h"

/* ========================================================================
 * Bitvector type
 * ======================================================================== */

/**
 * Fixed-width bitvector stored as an array of 64-bit words.
 *
 * Bit layout: words[0] holds bits [63:0], words[1] holds bits [127:64],
 * etc. The most significant bits of the last word beyond `width` are
 * always zero.
 */
typedef struct Lv00BitVector {
    uint64_t *words; /**< Array of 64-bit words holding the bit pattern */
    int width;       /**< Total bit width (must be > 0) */
} Lv00BitVector;

/* ========================================================================
 * Lifecycle
 * ======================================================================== */

/**
 * @brief Create a zero-initialized bitvector of the given width
 *
 * @param[in] width  Bit width (must be > 0)
 * @return New bitvector, or NULL on invalid width or allocation failure
 */
LV00_PUBLIC_API Lv00BitVector *bv_create(int width);

/**
 * @brief Destroy a bitvector and free its memory
 *
 * @param[in,out] bv  Bitvector to destroy (may be NULL)
 */
LV00_PUBLIC_API void bv_destroy(Lv00BitVector *bv);

/**
 * @brief Create a bitvector from an unsigned integer value
 *
 * The value is truncated to fit within the given width.
 *
 * @param[in] value  Unsigned integer value
 * @param[in] width  Bit width (must be > 0)
 * @return New bitvector, or NULL on error
 */
LV00_PUBLIC_API Lv00BitVector *bv_from_int(uint64_t value, int width);

/**
 * @brief Convert a bitvector to an unsigned 64-bit integer
 *
 * If the bitvector is wider than 64 bits, only the low 64 bits are returned.
 *
 * @param[in] bv  Bitvector (non-NULL)
 * @return Unsigned integer representation of the low 64 bits
 */
LV00_PUBLIC_API uint64_t bv_to_int(const Lv00BitVector *bv);

/* ========================================================================
 * Bitwise operations
 * ======================================================================== */

/**
 * @brief Bitwise NOT (complement)
 *
 * @param[in] a  Operand (non-NULL)
 * @return New bitvector with all bits flipped, or NULL on error
 */
LV00_PUBLIC_API Lv00BitVector *bv_not(const Lv00BitVector *a);

/**
 * @brief Bitwise AND
 *
 * Both operands must have the same width.
 *
 * @param[in] a  First operand (non-NULL)
 * @param[in] b  Second operand (non-NULL, same width as a)
 * @return New bitvector, or NULL on error
 */
LV00_PUBLIC_API Lv00BitVector *bv_and(const Lv00BitVector *a, const Lv00BitVector *b);

/**
 * @brief Bitwise OR
 *
 * Both operands must have the same width.
 *
 * @param[in] a  First operand (non-NULL)
 * @param[in] b  Second operand (non-NULL, same width as a)
 * @return New bitvector, or NULL on error
 */
LV00_PUBLIC_API Lv00BitVector *bv_or(const Lv00BitVector *a, const Lv00BitVector *b);

/**
 * @brief Bitwise XOR
 *
 * Both operands must have the same width.
 *
 * @param[in] a  First operand (non-NULL)
 * @param[in] b  Second operand (non-NULL, same width as a)
 * @return New bitvector, or NULL on error
 */
LV00_PUBLIC_API Lv00BitVector *bv_xor(const Lv00BitVector *a, const Lv00BitVector *b);

/* ========================================================================
 * Shift operations
 * ======================================================================== */

/**
 * @brief Logical shift left
 *
 * Vacated low bits are filled with zeros. Shifting by >= width yields zero.
 *
 * @param[in] a      Bitvector to shift (non-NULL)
 * @param[in] shift  Number of bits to shift left (>= 0)
 * @return New bitvector, or NULL on error
 */
LV00_PUBLIC_API Lv00BitVector *bv_shift_left(const Lv00BitVector *a, int shift);

/**
 * @brief Logical shift right
 *
 * Vacated high bits are filled with zeros. Shifting by >= width yields zero.
 *
 * @param[in] a      Bitvector to shift (non-NULL)
 * @param[in] shift  Number of bits to shift right (>= 0)
 * @return New bitvector, or NULL on error
 */
LV00_PUBLIC_API Lv00BitVector *bv_shift_right(const Lv00BitVector *a, int shift);

/* ========================================================================
 * Extraction and concatenation
 * ======================================================================== */

/**
 * @brief Extract a contiguous range of bits
 *
 * Extracts bits [high : low] (inclusive) from the bitvector, producing
 * a new bitvector of width (high - low + 1).
 *
 * Precondition: 0 <= low <= high < bv->width
 *
 * @param[in] bv    Source bitvector (non-NULL)
 * @param[in] high  High bit index (inclusive)
 * @param[in] low   Low bit index (inclusive)
 * @return New bitvector with extracted bits, or NULL on error
 */
LV00_PUBLIC_API Lv00BitVector *bv_extract(const Lv00BitVector *bv, int high, int low);

/**
 * @brief Concatenate two bitvectors
 *
 * Produces a new bitvector of width (a->width + b->width) where the bits
 * of `a` occupy the high positions and the bits of `b` occupy the low
 * positions. In SMT-LIB notation: (concat a b).
 *
 * @param[in] a  High bits (non-NULL)
 * @param[in] b  Low bits (non-NULL)
 * @return New concatenated bitvector, or NULL on error
 */
LV00_PUBLIC_API Lv00BitVector *bv_concat(const Lv00BitVector *a, const Lv00BitVector *b);

/* ========================================================================
 * Arithmetic operations (modular)
 * ======================================================================== */

/**
 * @brief Modular addition (wrapping)
 *
 * Computes (a + b) mod 2^width. Both operands must have the same width.
 *
 * @param[in] a  First operand (non-NULL)
 * @param[in] b  Second operand (non-NULL, same width as a)
 * @return New bitvector, or NULL on error
 */
LV00_PUBLIC_API Lv00BitVector *bv_add(const Lv00BitVector *a, const Lv00BitVector *b);

/**
 * @brief Modular multiplication (wrapping)
 *
 * Computes (a * b) mod 2^width. Both operands must have the same width.
 *
 * @param[in] a  First operand (non-NULL)
 * @param[in] b  Second operand (non-NULL, same width as a)
 * @return New bitvector, or NULL on error
 */
LV00_PUBLIC_API Lv00BitVector *bv_mul(const Lv00BitVector *a, const Lv00BitVector *b);

/**
 * @brief Two's complement negation
 *
 * Computes (-a) mod 2^width, equivalent to bv_not(a) + 1.
 *
 * @param[in] a  Operand (non-NULL)
 * @return New bitvector, or NULL on error
 */
LV00_PUBLIC_API Lv00BitVector *bv_neg(const Lv00BitVector *a);

/* ========================================================================
 * Comparison operations
 * ======================================================================== */

/**
 * @brief Unsigned equality comparison
 *
 * @param[in] a  First operand (non-NULL)
 * @param[in] b  Second operand (non-NULL)
 * @return true if the bitvectors have the same width and bit pattern
 */
LV00_PUBLIC_API bool bv_eq(const Lv00BitVector *a, const Lv00BitVector *b);

/**
 * @brief Unsigned less-than comparison
 *
 * Interprets both bitvectors as unsigned integers and returns true
 * if a < b. Both must have the same width.
 *
 * @param[in] a  First operand (non-NULL)
 * @param[in] b  Second operand (non-NULL, same width as a)
 * @return true if a < b in unsigned interpretation
 */
LV00_PUBLIC_API bool bv_ult(const Lv00BitVector *a, const Lv00BitVector *b);

/**
 * @brief Signed less-than comparison
 *
 * Interprets both bitvectors as two's-complement signed integers and
 * returns true if a < b. Both must have the same width.
 *
 * @param[in] a  First operand (non-NULL)
 * @param[in] b  Second operand (non-NULL, same width as a)
 * @return true if a < b in signed interpretation
 */
LV00_PUBLIC_API bool bv_slt(const Lv00BitVector *a, const Lv00BitVector *b);

#ifdef __cplusplus
}
#endif

#endif /* LV00_SMT_BITVECTOR_H */

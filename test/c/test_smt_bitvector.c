/**
 * @file test_smt_bitvector.c
 * @brief Tests for the SMT bitvector module (smt_bitvector.h)
 *
 * Tests cover:
 * - Bitvector creation and destruction
 * - Bitwise NOT
 * - Bitwise AND
 * - Modular addition
 * - Bit extraction
 * - Bitvector concatenation
 *
 * Uses the Lv-00 test framework macros from test_helpers.h.
 *
 * @version v3.4.2
 * @date 2026-05-25
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv00.h"
#include "smt_bitvector.h"
#include "test_helpers.h"

/* ── Convenience aliases for test readability ── */
#define bv_create(w)         lv00_bv_create((w), 0)
#define bv_destroy(bv)       lv00_bv_free(bv)
#define bv_and(a, b)         lv00_bv_and(a, b)
#define bv_or(a, b)          lv00_bv_or(a, b)
#define bv_xor(a, b)         lv00_bv_xor(a, b)
#define bv_not(a)            lv00_bv_not(a)
#define bv_add(a, b)         lv00_bv_add(a, b)
#define bv_mul(a, b)         lv00_bv_mul(a, b)
#define bv_eq(a, b)          lv00_bv_equals(a, b)
#define bv_extract(bv, h, l) lv00_bv_extract(bv, h, l)
#define bv_concat(a, b)      lv00_bv_concat(a, b)
#define bv_to_int(bv)        lv00_bv_to_int(bv)

/* Global test counters (required by test framework) */
int g_pass_count = 0;
int g_fail_count = 0;

/* ========================================================================
 * Test: bv_create
 * ======================================================================== */

/**
 * @brief Test basic bitvector creation and properties
 *
 * Verifies that bv_create returns a non-NULL pointer for valid widths,
 * that bv_from_int correctly stores values, and that bv_to_int retrieves
 * them. Also tests edge cases (width 0, width 1).
 */
void test_bv_create(void) {
    /* Create a 64-bit bitvector from an integer */
    Lv00BitVector *bv = bv_from_int(42, 64);
    TEST_ASSERT_NOT_NULL(bv);
    TEST_ASSERT_EQ(bv->width, 64);
    TEST_ASSERT_EQ(bv_to_int(bv), 42);
    bv_destroy(bv);

    /* Create a 128-bit bitvector from an integer */
    bv = bv_from_int(0xDEADBEEFCAFEBABEULL, 128);
    TEST_ASSERT_NOT_NULL(bv);
    TEST_ASSERT_EQ(bv->width, 128);
    TEST_ASSERT_EQ(bv_to_int(bv), 0xDEADBEEFCAFEBABEULL);
    bv_destroy(bv);

    /* Create a zero-initialized bitvector */
    bv = bv_create(32);
    TEST_ASSERT_NOT_NULL(bv);
    TEST_ASSERT_EQ(bv->width, 32);
    TEST_ASSERT_EQ(bv_to_int(bv), 0);
    bv_destroy(bv);

    /* Width 1 bitvector */
    bv = bv_from_int(1, 1);
    TEST_ASSERT_NOT_NULL(bv);
    TEST_ASSERT_EQ(bv->width, 1);
    TEST_ASSERT_EQ(bv_to_int(bv), 1);
    bv_destroy(bv);

    /* Invalid width returns NULL */
    bv = bv_create(0);
    TEST_ASSERT_NULL(bv);

    bv = bv_create(-1);
    TEST_ASSERT_NULL(bv);

    /* Value truncation: 256 in 8 bits should be 0 */
    bv = bv_from_int(256, 8);
    TEST_ASSERT_NOT_NULL(bv);
    TEST_ASSERT_EQ(bv_to_int(bv), 0);
    bv_destroy(bv);
}

/* ========================================================================
 * Test: bv_not
 * ======================================================================== */

/**
 * @brief Test bitwise NOT (complement)
 *
 * Verifies that NOT inverts all bits correctly for various widths,
 * including the boundary case where the result wraps within the width.
 */
void test_bv_not(void) {
    /* NOT of 0 should be all 1s */
    Lv00BitVector *zero = bv_from_int(0, 8);
    Lv00BitVector *inv = bv_not(zero);
    TEST_ASSERT_NOT_NULL(inv);
    TEST_ASSERT_EQ(bv_to_int(inv), 0xFF);
    bv_destroy(zero);
    bv_destroy(inv);

    /* NOT of 0xFF should be 0 */
    Lv00BitVector *ff = bv_from_int(0xFF, 8);
    inv = bv_not(ff);
    TEST_ASSERT_NOT_NULL(inv);
    TEST_ASSERT_EQ(bv_to_int(inv), 0);
    bv_destroy(ff);
    bv_destroy(inv);

    /* NOT of 0x0F should be 0xF0 */
    Lv00BitVector *val = bv_from_int(0x0F, 8);
    inv = bv_not(val);
    TEST_ASSERT_NOT_NULL(inv);
    TEST_ASSERT_EQ(bv_to_int(inv), 0xF0);
    bv_destroy(val);
    bv_destroy(inv);

    /* NOT of NULL returns NULL */
    inv = bv_not(NULL);
    TEST_ASSERT_NULL(inv);

    /* Double NOT should be identity */
    Lv00BitVector *orig = bv_from_int(0x12345678, 32);
    Lv00BitVector *not1 = bv_not(orig);
    Lv00BitVector *not2 = bv_not(not1);
    TEST_ASSERT(bv_eq(orig, not2), "double NOT should be identity");
    bv_destroy(orig);
    bv_destroy(not1);
    bv_destroy(not2);
}

/* ========================================================================
 * Test: bv_and
 * ======================================================================== */

/**
 * @brief Test bitwise AND operation
 *
 * Verifies AND with various operand combinations and edge cases.
 */
void test_bv_and(void) {
    /* 0xFF AND 0x0F = 0x0F */
    Lv00BitVector *a = bv_from_int(0xFF, 8);
    Lv00BitVector *b = bv_from_int(0x0F, 8);
    Lv00BitVector *result = bv_and(a, b);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQ(bv_to_int(result), 0x0F);
    bv_destroy(a);
    bv_destroy(b);
    bv_destroy(result);

    /* 0xAA AND 0x55 = 0x00 */
    a = bv_from_int(0xAA, 8);
    b = bv_from_int(0x55, 8);
    result = bv_and(a, b);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQ(bv_to_int(result), 0x00);
    bv_destroy(a);
    bv_destroy(b);
    bv_destroy(result);

    /* x AND 0 = 0 */
    a = bv_from_int(0x12345678, 32);
    b = bv_from_int(0, 32);
    result = bv_and(a, b);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQ(bv_to_int(result), 0);
    bv_destroy(a);
    bv_destroy(b);
    bv_destroy(result);

    /* x AND x = x */
    a = bv_from_int(0xABCD, 16);
    result = bv_and(a, a);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT(bv_eq(a, result), "x AND x should equal x");
    bv_destroy(a);
    bv_destroy(result);

    /* Mismatched width returns NULL */
    a = bv_from_int(1, 8);
    b = bv_from_int(1, 16);
    result = bv_and(a, b);
    TEST_ASSERT_NULL(result);
    bv_destroy(a);
    bv_destroy(b);
}

/* ========================================================================
 * Test: bv_add
 * ======================================================================== */

/**
 * @brief Test modular addition
 *
 * Verifies addition with carry, overflow wrapping, and edge cases.
 */
void test_bv_add(void) {
    /* Simple addition: 3 + 4 = 7 */
    Lv00BitVector *a = bv_from_int(3, 8);
    Lv00BitVector *b = bv_from_int(4, 8);
    Lv00BitVector *result = bv_add(a, b);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQ(bv_to_int(result), 7);
    bv_destroy(a);
    bv_destroy(b);
    bv_destroy(result);

    /* Overflow wrapping: 255 + 1 = 0 (mod 256) */
    a = bv_from_int(255, 8);
    b = bv_from_int(1, 8);
    result = bv_add(a, b);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQ(bv_to_int(result), 0);
    bv_destroy(a);
    bv_destroy(b);
    bv_destroy(result);

    /* Carry propagation: 0xFF + 0x01 = 0x00 (8-bit) */
    a = bv_from_int(0xFF, 8);
    b = bv_from_int(0x01, 8);
    result = bv_add(a, b);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQ(bv_to_int(result), 0);
    bv_destroy(a);
    bv_destroy(b);
    bv_destroy(result);

    /* 64-bit addition without overflow */
    a = bv_from_int(1000000, 64);
    b = bv_from_int(2000000, 64);
    result = bv_add(a, b);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQ(bv_to_int(result), 3000000);
    bv_destroy(a);
    bv_destroy(b);
    bv_destroy(result);

    /* x + 0 = x */
    a = bv_from_int(0x1234, 16);
    b = bv_from_int(0, 16);
    result = bv_add(a, b);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT(bv_eq(a, result), "x + 0 should equal x");
    bv_destroy(a);
    bv_destroy(b);
    bv_destroy(result);
}

/* ========================================================================
 * Test: bv_extract
 * ======================================================================== */

/**
 * @brief Test bit extraction
 *
 * Verifies extraction of bit ranges from bitvectors of various widths.
 */
void test_bv_extract(void) {
    /* Extract low 4 bits of 0xAB (8-bit) -> 0xB (4-bit) */
    Lv00BitVector *bv = bv_from_int(0xAB, 8);
    Lv00BitVector *ext = bv_extract(bv, 3, 0);
    TEST_ASSERT_NOT_NULL(ext);
    TEST_ASSERT_EQ(ext->width, 4);
    TEST_ASSERT_EQ(bv_to_int(ext), 0xB);
    bv_destroy(bv);
    bv_destroy(ext);

    /* Extract high 4 bits of 0xAB (8-bit) -> 0xA (4-bit) */
    bv = bv_from_int(0xAB, 8);
    ext = bv_extract(bv, 7, 4);
    TEST_ASSERT_NOT_NULL(ext);
    TEST_ASSERT_EQ(ext->width, 4);
    TEST_ASSERT_EQ(bv_to_int(ext), 0xA);
    bv_destroy(bv);
    bv_destroy(ext);

    /* Extract single bit: bit 0 of 1 -> 1 */
    bv = bv_from_int(1, 8);
    ext = bv_extract(bv, 0, 0);
    TEST_ASSERT_NOT_NULL(ext);
    TEST_ASSERT_EQ(ext->width, 1);
    TEST_ASSERT_EQ(bv_to_int(ext), 1);
    bv_destroy(bv);
    bv_destroy(ext);

    /* Extract single bit: bit 1 of 1 -> 0 */
    bv = bv_from_int(1, 8);
    ext = bv_extract(bv, 1, 1);
    TEST_ASSERT_NOT_NULL(ext);
    TEST_ASSERT_EQ(ext->width, 1);
    TEST_ASSERT_EQ(bv_to_int(ext), 0);
    bv_destroy(bv);
    bv_destroy(ext);

    /* Extract full range: should be identity */
    bv = bv_from_int(0x1234, 16);
    ext = bv_extract(bv, 15, 0);
    TEST_ASSERT_NOT_NULL(ext);
    TEST_ASSERT_EQ(ext->width, 16);
    TEST_ASSERT(bv_eq(bv, ext), "extracting full range should be identity");
    bv_destroy(bv);
    bv_destroy(ext);

    /* Invalid ranges return NULL */
    bv = bv_from_int(0xFF, 8);
    ext = bv_extract(bv, 7, 8); /* high < low */
    TEST_ASSERT_NULL(ext);
    ext = bv_extract(bv, 8, 0); /* high >= width */
    TEST_ASSERT_NULL(ext);
    ext = bv_extract(bv, -1, 0); /* negative index */
    TEST_ASSERT_NULL(ext);
    bv_destroy(bv);
}

/* ========================================================================
 * Test: bv_concat
 * ======================================================================== */

/**
 * @brief Test bitvector concatenation
 *
 * Verifies that concat places the first operand in high bits and
 * the second in low bits, producing the correct combined width.
 */
void test_bv_concat(void) {
    /* concat(0xA [4-bit], 0xB [4-bit]) = 0xAB [8-bit] */
    Lv00BitVector *hi = bv_from_int(0xA, 4);
    Lv00BitVector *lo = bv_from_int(0xB, 4);
    Lv00BitVector *result = bv_concat(hi, lo);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQ(result->width, 8);
    TEST_ASSERT_EQ(bv_to_int(result), 0xAB);
    bv_destroy(hi);
    bv_destroy(lo);
    bv_destroy(result);

    /* concat(0x12 [8-bit], 0x34 [8-bit]) = 0x1234 [16-bit] */
    hi = bv_from_int(0x12, 8);
    lo = bv_from_int(0x34, 8);
    result = bv_concat(hi, lo);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQ(result->width, 16);
    TEST_ASSERT_EQ(bv_to_int(result), 0x1234);
    bv_destroy(hi);
    bv_destroy(lo);
    bv_destroy(result);

    /* concat with zero low part: concat(0xFF [8-bit], 0 [8-bit]) = 0xFF00 [16-bit] */
    hi = bv_from_int(0xFF, 8);
    lo = bv_from_int(0, 8);
    result = bv_concat(hi, lo);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQ(result->width, 16);
    TEST_ASSERT_EQ(bv_to_int(result), 0xFF00);
    bv_destroy(hi);
    bv_destroy(lo);
    bv_destroy(result);

    /* concat with zero high part: concat(0 [8-bit], 0xAB [8-bit]) = 0x00AB [16-bit] */
    hi = bv_from_int(0, 8);
    lo = bv_from_int(0xAB, 8);
    result = bv_concat(hi, lo);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQ(result->width, 16);
    TEST_ASSERT_EQ(bv_to_int(result), 0xAB);
    bv_destroy(hi);
    bv_destroy(lo);
    bv_destroy(result);

    /* NULL operands return NULL */
    result = bv_concat(NULL, lo);
    TEST_ASSERT_NULL(result);
    result = bv_concat(hi, NULL);
    TEST_ASSERT_NULL(result);
}

/* ========================================================================
 * Main
 * ======================================================================== */

int main(void) {
    TEST_SUITE_BEGIN("SMT Bitvector");

    TEST_RUN(test_bv_create);
    TEST_RUN(test_bv_not);
    TEST_RUN(test_bv_and);
    TEST_RUN(test_bv_add);
    TEST_RUN(test_bv_extract);
    TEST_RUN(test_bv_concat);

    TEST_SUITE_END();

    return g_fail_count > 0 ? 1 : 0;
}

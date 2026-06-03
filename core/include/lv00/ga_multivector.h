/**
 * @file ga_multivector.h
 * @brief PGA Multivector type definitions and operations
 *
 * Merges design patterns from GATr (Geometric Algebra Transformer)
 * and GAALOP (Geometric Algebra ALgebra Oriented Programmer):
 *   - GATr: flat array multivector storage, grade-based operations
 *   - GAALOP: multiplication table driven computation, code generation
 *
 * The default algebra is Cl(3,0,1) -- the projective geometric algebra
 * for 3D Euclidean space with 16 basis blades (grades 0..4).
 *
 * Blade ordering (Cl(3,0,1)):
 *   Grade 0: 1
 *   Grade 1: e1, e2, e3, e0
 *   Grade 2: e12, e13, e14, e23, e24, e34
 *   Grade 3: e123, e124, e134, e234
 *   Grade 4: e1234
 *
 * @version 1.0.0
 */

#ifndef LV00_GA_MULTIVECTOR_H
#define LV00_GA_MULTIVECTOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lv00.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Constants
 * ======================================================================== */

/** @brief Dimension of the multivector component array (2^4 = 16 for Cl(3,0,1)) */
#define GA_MV_DIM 16

/* ========================================================================
 * Blade index constants for Cl(3,0,1)
 *
 * These indices correspond to the canonical ordering of the 16 basis
 * blades in the 4D algebra with signature (3,0,1).
 * ======================================================================== */

#define GA_BLADE_1     0   /**< Scalar (grade 0) */
#define GA_BLADE_E1    1   /**< e1 (grade 1) */
#define GA_BLADE_E2    2   /**< e2 (grade 1) */
#define GA_BLADE_E3    3   /**< e3 (grade 1) */
#define GA_BLADE_E0    4   /**< e0 (grade 1, null basis) */
#define GA_BLADE_E12   5   /**< e12 (grade 2) */
#define GA_BLADE_E13   6   /**< e13 (grade 2) */
#define GA_BLADE_E03   7   /**< e03 = e1*e0 (grade 2) */
#define GA_BLADE_E23   8   /**< e23 (grade 2) */
#define GA_BLADE_E023  9   /**< e023 = e2*e0 (grade 2) */
#define GA_BLADE_E123  10  /**< e123 (grade 3) */
#define GA_BLADE_E0123 11  /**< e0123 = e1*e2*e0 (grade 3) */
#define GA_BLADE_E013  12  /**< e013 = e1*e3*e0 (grade 3) */
#define GA_BLADE_E0234 13  /**< e0234 = e2*e3*e0 (grade 3) */
#define GA_BLADE_E01234 14 /**< e01234 (grade 4) */
#define GA_BLADE_E1234 15  /**< e1234 (grade 4, pseudoscalar) */

/* ========================================================================
 * Structures
 * ======================================================================== */

/**
 * @brief Signature of a geometric algebra Cl(p,q,r)
 *
 * @param p  Number of positive-norm basis vectors
 * @param q  Number of negative-norm basis vectors
 * @param r  Number of null (degenerate) basis vectors
 */
typedef struct GASignature {
    int p; /**< Positive-norm basis vectors */
    int q; /**< Negative-norm basis vectors */
    int r; /**< Null basis vectors */
} GASignature;

/**
 * @brief Single entry in the geometric product multiplication table
 *
 * Each entry describes the result of multiplying two basis blades:
 *   e_i * e_j = sign * e_{result_index}
 *
 * @param result_index  Index of the resulting basis blade (0..GA_MV_DIM-1)
 * @param sign          Sign factor: +1 or -1
 */
typedef struct GAMultEntry {
    int   result_index; /**< Index of the result blade */
    int   sign;         /**< Sign: +1 or -1 */
} GAMultEntry;

/**
 * @brief Full multiplication table for a geometric algebra
 *
 * The table is a flat array of size dim*dim, where table[i*dim + j]
 * gives the GAMultEntry for the product e_i * e_j.
 *
 * @param sig          Algebra signature Cl(p,q,r)
 * @param dim          Dimension of the multivector (number of basis blades)
 * @param table        Flat multiplication table (dim x dim entries)
 * @param blade_names  Human-readable names for each basis blade (dim strings)
 */
typedef struct GAMultTable {
    GASignature sig;          /**< Algebra signature */
    int         dim;          /**< Number of basis blades */
    GAMultEntry *table;       /**< Multiplication table (owned, dim*dim entries) */
    char       **blade_names; /**< Blade name strings (owned, dim entries) */
} GAMultTable;

/**
 * @brief Multivector in a geometric algebra
 *
 * Stores both numeric and optional symbolic components.
 * The component array is indexed by blade index following the
 * canonical ordering of the algebra's basis blades.
 *
 * @param components           Numeric coefficients for each blade
 * @param symbolic_components  Symbolic expression strings (NULL if not symbolic)
 * @param is_symbolic          Whether symbolic components are active
 * @param trust                Trust level for approximate computation (0.0..1.0)
 */
typedef struct Lv00MultiVector {
    double components[GA_MV_DIM];           /**< Numeric blade coefficients */
    char  *symbolic_components[GA_MV_DIM];  /**< Symbolic blade expressions (nullable) */
    int    is_symbolic;                      /**< Non-zero if symbolic mode is active */
    double trust;                            /**< Trust level for results */
} Lv00MultiVector;

/* ========================================================================
 * Predefined Signatures
 * ======================================================================== */

/** @brief Cl(3,0,1) -- Projective GA for 3D Euclidean geometry (PGA) */
static const GASignature GA_CL_3_0_1 = {3, 0, 1};

/** @brief Cl(2,0,1) -- Projective GA for 2D Euclidean geometry */
static const GASignature GA_CL_2_0_1 = {2, 0, 1};

/** @brief Cl(3,0,0) -- Standard 3D Euclidean GA */
static const GASignature GA_CL_3_0_0 = {3, 0, 0};

/* ========================================================================
 * Basic Multivector Operations
 * ======================================================================== */

/**
 * @brief Create a zero multivector
 * @return A new zero multivector (caller owns, free with ga_mv_free)
 */
LV00_PUBLIC_API Lv00MultiVector *ga_mv_zero(void);

/**
 * @brief Create a scalar multivector
 * @param value  The scalar value
 * @return A new scalar multivector (caller owns, free with ga_mv_free)
 */
LV00_PUBLIC_API Lv00MultiVector *ga_mv_scalar(double value);

/**
 * @brief Create a copy of a multivector
 * @param mv  Source multivector (must not be NULL)
 * @return A deep copy (caller owns, free with ga_mv_free)
 */
LV00_PUBLIC_API Lv00MultiVector *ga_mv_copy(const Lv00MultiVector *mv);

/**
 * @brief Free a multivector and its symbolic components
 * @param mv  Multivector to free (may be NULL)
 */
LV00_PUBLIC_API void ga_mv_free(Lv00MultiVector *mv);

/**
 * @brief Project a multivector onto a specific grade
 * @param mv     Source multivector
 * @param grade  Target grade (0..4)
 * @return A new multivector containing only the specified grade components
 */
LV00_PUBLIC_API Lv00MultiVector *ga_mv_grade_projection(const Lv00MultiVector *mv, int grade);

/* ========================================================================
 * Multiplication Table Operations
 * ======================================================================== */

/**
 * @brief Create a multiplication table for the given signature
 * @param sig  Algebra signature
 * @return A new multiplication table (caller owns, free with ga_mult_table_destroy)
 */
LV00_PUBLIC_API GAMultTable *ga_mult_table_create(GASignature sig);

/**
 * @brief Destroy a multiplication table and free its resources
 * @param tbl  Table to destroy (may be NULL)
 */
LV00_PUBLIC_API void ga_mult_table_destroy(GAMultTable *tbl);

/**
 * @brief Get the multiplication table entry for e_i * e_j
 * @param tbl  Multiplication table
 * @param i    First blade index
 * @param j    Second blade index
 * @return The multiplication entry (zero entry if indices out of range)
 */
LV00_PUBLIC_API GAMultEntry ga_mult_table_get_entry(const GAMultTable *tbl, int i, int j);

/* ========================================================================
 * Geometric Algebra Operations
 * ======================================================================== */

/**
 * @brief Compute the geometric product of two multivectors
 *
 * The geometric product is the fundamental product of geometric algebra.
 * For basis blades: e_i * e_j = sign * e_k (looked up from the table).
 *
 * @param a  First operand
 * @param b  Second operand
 * @return A new multivector representing a * b
 */
LV00_PUBLIC_API Lv00MultiVector *ga_geometric_product(const Lv00MultiVector *a,
                                                       const Lv00MultiVector *b);

/**
 * @brief Compute the outer (wedge) product of two multivectors
 *
 * The outer product extracts the highest-grade components of the
 * geometric product. It represents the join of subspaces.
 *
 * @param a  First operand
 * @param b  Second operand
 * @return A new multivector representing a ^ b
 */
LV00_PUBLIC_API Lv00MultiVector *ga_outer_product(const Lv00MultiVector *a,
                                                   const Lv00MultiVector *b);

/**
 * @brief Compute the inner (dot) product of two multivectors
 *
 * The inner product extracts the lowest-grade (non-scalar) components
 * of the geometric product. It represents the meet of subspaces.
 *
 * @param a  First operand
 * @param b  Second operand
 * @return A new multivector representing a . b
 */
LV00_PUBLIC_API Lv00MultiVector *ga_inner_product(const Lv00MultiVector *a,
                                                   const Lv00MultiVector *b);

/**
 * @brief Compute the reverse of a multivector
 *
 * The reverse negates all odd-grade components:
 *   rev(e_i1 ... e_ik) = (-1)^(k*(k-1)/2) * e_i1 ... e_ik
 *
 * @param mv  Source multivector
 * @return A new multivector that is the reverse of mv
 */
LV00_PUBLIC_API Lv00MultiVector *ga_reverse(const Lv00MultiVector *mv);

/**
 * @brief Compute the grade involute of a multivector
 *
 * The grade involute negates all odd-grade components:
 *   G*(mv) = sum_k (-1)^k <mv>_k
 *
 * @param mv  Source multivector
 * @return A new multivector that is the grade involute of mv
 */
LV00_PUBLIC_API Lv00MultiVector *ga_grade_involute(const Lv00MultiVector *mv);

/**
 * @brief Compute the squared norm of a multivector
 *
 * The squared norm is defined as: ||mv||^2 = <mv * ~mv>_0
 * where ~mv is the reverse. For a pure blade b: ||b||^2 = b * ~b.
 *
 * @param mv  Source multivector
 * @return The scalar squared norm
 */
LV00_PUBLIC_API double ga_norm_squared(const Lv00MultiVector *mv);

/* ========================================================================
 * Arithmetic Operations
 * ======================================================================== */

/**
 * @brief Add two multivectors
 * @param a  First operand
 * @param b  Second operand
 * @return A new multivector representing a + b
 */
LV00_PUBLIC_API Lv00MultiVector *ga_mv_add(const Lv00MultiVector *a,
                                             const Lv00MultiVector *b);

/**
 * @brief Subtract two multivectors
 * @param a  First operand (minuend)
 * @param b  Second operand (subtrahend)
 * @return A new multivector representing a - b
 */
LV00_PUBLIC_API Lv00MultiVector *ga_mv_sub(const Lv00MultiVector *a,
                                             const Lv00MultiVector *b);

/**
 * @brief Scale a multivector by a scalar
 * @param mv     Source multivector
 * @param scale  Scalar factor
 * @return A new multivector representing scale * mv
 */
LV00_PUBLIC_API Lv00MultiVector *ga_mv_scale(const Lv00MultiVector *mv, double scale);

#ifdef __cplusplus
}
#endif

#endif /* LV00_GA_MULTIVECTOR_H */

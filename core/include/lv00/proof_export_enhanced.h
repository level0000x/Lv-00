/**
 * @file proof_export_enhanced.h
 * @brief Enhanced proof export module supporting multiple output formats.
 *
 * @details Provides a unified interface for exporting proof data to various
 *          formats including HTML, LaTeX, Coq, Lean 4, JSON, and DOT (Graphviz).
 *          Inspired by Mathport (Lean migration), Why3 (multi-prover dispatch),
 *          and MMT (mathematical knowledge management).
 *
 *          Each format has its own serialization strategy:
 *          - HTML: Human-readable web page with styled proof steps
 *          - LaTeX: Publication-ready typeset proof
 *          - Coq: Machine-checkable proof script
 *          - Lean 4: Machine-checkable proof script
 *          - JSON: Structured data for programmatic consumption
 *          - DOT: Graph visualization of proof dependencies
 *
 * @author Lv-00 Project
 * @version 3.3.0
 * @date   2026-05-25
 */

#ifndef LV00_PROOF_EXPORT_ENHANCED_H
#define LV00_PROOF_EXPORT_ENHANCED_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lv00.h"

/* ============================================================
 * Export format enumeration
 * ============================================================ */

/**
 * @brief Supported export formats for proof data.
 */
typedef enum Lv00ExportFormat {
    EXPORT_HTML   = 0, /**< HTML web page */
    EXPORT_LATEX  = 1, /**< LaTeX document */
    EXPORT_COQ    = 2, /**< Coq proof script */
    EXPORT_LEAN4  = 3, /**< Lean 4 proof script */
    EXPORT_JSON   = 4, /**< JSON structured data */
    EXPORT_DOT    = 5  /**< DOT (Graphviz) graph */
} Lv00ExportFormat;

/* ============================================================
 * Export configuration
 * ============================================================ */

/**
 * @brief Configuration for proof export.
 */
typedef struct Lv00ExportConfig {
    Lv00ExportFormat format;              /**< Target export format */
    bool             include_proof_trace; /**< Include detailed proof trace */
    bool             include_geometry;    /**< Include geometric construction data */
    bool             pretty_print;        /**< Enable pretty-printing / indentation */
} Lv00ExportConfig;

/* ============================================================
 * Export result
 * ============================================================ */

/**
 * @brief Holds the result of a proof export operation.
 *
 * @note The output string is dynamically allocated and must be freed
 *       by the caller using proof_export_result_destroy().
 */
typedef struct Lv00ExportResult {
    char   *output;       /**< Exported content (null-terminated string) */
    size_t  output_size;  /**< Length of output in bytes (excluding null terminator) */
    bool    success;      /**< Whether the export succeeded */
    char   *error_msg;    /**< Error message if success is false (may be NULL) */
} Lv00ExportResult;

/* ============================================================
 * Proof step structure (simplified for export)
 * ============================================================ */

/**
 * @brief A single step in a proof.
 */
typedef struct Lv00ProofStep {
    int         step_id;    /**< Unique step identifier */
    const char *rule;       /**< Applied rule name */
    const char *premise;    /**< Premise description */
    const char *conclusion; /**< Conclusion description */
    int         depth;      /**< Nesting depth in the proof tree */
} Lv00ProofStep;

/**
 * @brief A proof consisting of ordered steps.
 */
typedef struct Lv00Proof {
    Lv00ProofStep *steps;   /**< Array of proof steps */
    size_t         n_steps; /**< Number of steps */
    const char    *theorem; /**< Theorem statement */
} Lv00Proof;

/* ============================================================
 * API: Export
 * ============================================================ */

/**
 * @brief Export a proof to the specified format.
 *
 * @param proof   The proof to export
 * @param config  Export configuration
 * @return Pointer to the export result, or NULL on failure
 */
LV00_PUBLIC_API Lv00ExportResult *proof_export_enhanced(const Lv00Proof       *proof,
                                                        const Lv00ExportConfig *config);

/**
 * @brief Export a proof from a navigator context (simplified interface).
 *
 * This is a convenience function that wraps proof_export_enhanced
 * with common defaults for the given format.
 *
 * @param theorem_name  Name of the theorem
 * @param format        Target export format
 * @return Pointer to the export result, or NULL on failure
 */
LV00_PUBLIC_API Lv00ExportResult *proof_export_from_navigator(const char        *theorem_name,
                                                              Lv00ExportFormat   format);

/* ============================================================
 * API: Destroy
 * ============================================================ */

/**
 * @brief Destroy an export result and free all associated memory.
 *
 * @param result  The export result to destroy (may be NULL)
 */
LV00_PUBLIC_API void proof_export_result_destroy(Lv00ExportResult *result);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PROOF_EXPORT_ENHANCED_H */

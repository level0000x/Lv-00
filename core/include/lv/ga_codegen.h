/**
 * @file ga_codegen.h
 * @brief Geometric Algebra code generator
 *
 * Translates GA expressions into target programming languages.
 * Inspired by GAALOP's approach of compiling symbolic GA expressions
 * into optimized numeric code.
 *
 * Supported targets:
 *   - C:     Standard C code
 *   - CPP:   C++ code
 *   - CUDA:  CUDA kernel code
 *   - LATEX: LaTeX mathematical notation
 *   - PYTHON: Python / NumPy code
 *   - DOT:   Graphviz DOT graph for expression trees
 *
 * @version 1.1.0
 */

#ifndef lv_GA_CODEGEN_H
#define lv_GA_CODEGEN_H

#include "ga_multivector.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Enumerations
 * ======================================================================== */

/**
 * @brief Target language for code generation
 */
typedef enum GACodegenTarget {
    GA_CODEGEN_C,      /**< Standard C code */
    GA_CODEGEN_CPP,    /**< C++ code */
    GA_CODEGEN_CUDA,   /**< CUDA kernel code */
    GA_CODEGEN_LATEX,  /**< LaTeX mathematical notation */
    GA_CODEGEN_PYTHON, /**< Python / NumPy code */
    GA_CODEGEN_DOT     /**< Graphviz DOT graph */
} GACodegenTarget;

/* ========================================================================
 * Structures
 * ======================================================================== */

/**
 * @brief Options for code generation
 *
 * @param target         Target language
 * @param variable_name  Name of the output variable in generated code
 * @param indent         Indentation string (e.g., "  " or "\t")
 * @param include_header Whether to include header comments
 * @param optimize       Whether to apply basic optimizations (constant folding)
 */
typedef struct GACodegenOptions {
    GACodegenTarget target;    /**< Target language */
    const char     *variable_name; /**< Output variable name */
    const char     *indent;        /**< Indentation string */
    int             include_header; /**< Include header comments (non-zero = yes) */
    int             optimize;       /**< Apply optimizations (non-zero = yes) */
} GACodegenOptions;

/**
 * @brief Result of code generation
 *
 * @param code        Generated source code string (caller must free with ga_codegen_result_destroy)
 * @param error_msg   Error message string (NULL if no error)
 * @param target      Target language that was used
 * @param line_count  Number of lines in generated code
 */
typedef struct GACodegenResult {
    char            *code;        /**< Generated code string */
    char            *error_msg;   /**< Error message (NULL if success) */
    GACodegenTarget  target;      /**< Target language used */
    int              line_count;  /**< Number of lines generated */
} GACodegenResult;

/* ========================================================================
 * Code Generation Functions
 * ======================================================================== */

/**
 * @brief Compile a multivector expression into target code
 *
 * Takes a multivector and generates source code that reconstructs
 * the same multivector in the target language.
 *
 * @param mv       Source multivector to compile
 * @param options  Code generation options
 * @return A code generation result (caller owns, free with ga_codegen_result_destroy)
 */
lv_PUBLIC_API GACodegenResult *ga_codegen_compile(const lvMultiVector *mv,
                                                     const GACodegenOptions *options);

/**
 * @brief Destroy a code generation result and free its resources
 * @param result  Result to destroy (may be NULL)
 */
lv_PUBLIC_API void ga_codegen_result_destroy(GACodegenResult *result);

/* ========================================================================
 * Rendering Functions
 * ======================================================================== */

/**
 * @brief Render a multivector as a LaTeX string
 *
 * Generates a human-readable LaTeX representation of the multivector,
 * showing each non-zero blade component with its coefficient.
 *
 * @param mv  Source multivector
 * @return A new LaTeX string (caller owns, free with lv_free_ptr)
 */
lv_PUBLIC_API char *ga_render_latex(const lvMultiVector *mv);

/**
 * @brief Render a multivector as a DOT graph string
 *
 * Generates a Graphviz DOT representation of the multivector's
 * non-zero components as a simple graph.
 *
 * @param mv  Source multivector
 * @return A new DOT string (caller owns, free with lv_free_ptr)
 */
lv_PUBLIC_API char *ga_render_dot(const lvMultiVector *mv);

#ifdef __cplusplus
}
#endif

#endif /* lv_GA_CODEGEN_H */

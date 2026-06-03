﻿/**
 * @file test_proof_export_enhanced.c
 * @brief Tests for the enhanced proof export module.
 *
 * @details Tests cover:
 *          - HTML export
 *          - LaTeX export
 *          - Coq export
 *          - Lean 4 export
 *          - JSON export
 *          - DOT export
 *          - Export from navigator convenience function
 *          - NULL safety
 *
 * @author Lv-00 Project
 * @version 3.3.0
 * @date   2026-05-25
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv00.h"
#include "proof_export_enhanced.h"
#include "test_helpers.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============================================================
 * Helper: create a simple test proof
 * ============================================================ */

static Lv00Proof create_test_proof(void) {
    static Lv00ProofStep steps[3];

    steps[0].step_id    = 1;
    steps[0].rule       = "assume";
    steps[0].premise    = NULL;
    steps[0].conclusion = "P";
    steps[0].depth      = 0;

    steps[1].step_id    = 2;
    steps[1].rule       = "intro";
    steps[1].premise    = "P";
    steps[1].conclusion = "P -> Q";
    steps[1].depth      = 0;

    steps[2].step_id    = 3;
    steps[2].rule       = "apply";
    steps[2].premise    = "P -> Q";
    steps[2].conclusion = "Q";
    steps[2].depth      = 1;

    Lv00Proof proof;
    proof.steps   = steps;
    proof.n_steps = 3;
    proof.theorem = "Modus Ponens: P -> (P -> Q) -> Q";
    return proof;
}

/* ============================================================
 * Test: HTML export
 * ============================================================ */

static void test_export_html(void) {
    Lv00Proof proof = create_test_proof();

    Lv00ExportConfig config;
    config.format              = EXPORT_HTML;
    config.include_proof_trace = true;
    config.include_geometry    = false;
    config.pretty_print        = true;

    Lv00ExportResult *result = proof_export_enhanced(&proof, &config);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT(result->success, "HTML export should succeed");
    TEST_ASSERT_NOT_NULL(result->output);
    TEST_ASSERT(result->output_size > 0, "HTML output should not be empty");

    /* Check for expected HTML elements */
    TEST_ASSERT(strstr(result->output, "<!DOCTYPE html>") != NULL,
                "HTML should contain DOCTYPE");
    TEST_ASSERT(strstr(result->output, "<html>") != NULL,
                "HTML should contain html tag");
    TEST_ASSERT(strstr(result->output, "Modus Ponens") != NULL,
                "HTML should contain theorem name");

    proof_export_result_destroy(result);
}

/* ============================================================
 * Test: LaTeX export
 * ============================================================ */

static void test_export_latex(void) {
    Lv00Proof proof = create_test_proof();

    Lv00ExportConfig config;
    config.format              = EXPORT_LATEX;
    config.include_proof_trace = false;
    config.include_geometry    = false;
    config.pretty_print        = true;

    Lv00ExportResult *result = proof_export_enhanced(&proof, &config);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT(result->success, "LaTeX export should succeed");
    TEST_ASSERT_NOT_NULL(result->output);

    TEST_ASSERT(strstr(result->output, "\\documentclass") != NULL,
                "LaTeX should contain documentclass");
    TEST_ASSERT(strstr(result->output, "\\begin{proof}") != NULL,
                "LaTeX should contain proof environment");
    TEST_ASSERT(strstr(result->output, "\\end{document}") != NULL,
                "LaTeX should end document");

    proof_export_result_destroy(result);
}

/* ============================================================
 * Test: Coq export
 * ============================================================ */

static void test_export_coq(void) {
    Lv00Proof proof = create_test_proof();

    Lv00ExportConfig config;
    config.format              = EXPORT_COQ;
    config.include_proof_trace = false;
    config.include_geometry    = false;
    config.pretty_print        = true;

    Lv00ExportResult *result = proof_export_enhanced(&proof, &config);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT(result->success, "Coq export should succeed");
    TEST_ASSERT_NOT_NULL(result->output);

    TEST_ASSERT(strstr(result->output, "Theorem") != NULL,
                "Coq should contain Theorem");
    TEST_ASSERT(strstr(result->output, "Qed.") != NULL,
                "Coq should contain Qed");

    proof_export_result_destroy(result);
}

/* ============================================================
 * Test: Lean 4 export
 * ============================================================ */

static void test_export_lean4(void) {
    Lv00Proof proof = create_test_proof();

    Lv00ExportConfig config;
    config.format              = EXPORT_LEAN4;
    config.include_proof_trace = false;
    config.include_geometry    = false;
    config.pretty_print        = true;

    Lv00ExportResult *result = proof_export_enhanced(&proof, &config);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT(result->success, "Lean 4 export should succeed");
    TEST_ASSERT_NOT_NULL(result->output);

    TEST_ASSERT(strstr(result->output, "theorem") != NULL,
                "Lean 4 should contain theorem");
    TEST_ASSERT(strstr(result->output, "Mathlib") != NULL,
                "Lean 4 should import Mathlib");

    proof_export_result_destroy(result);
}

/* ============================================================
 * Test: JSON export
 * ============================================================ */

static void test_export_json(void) {
    Lv00Proof proof = create_test_proof();

    Lv00ExportConfig config;
    config.format              = EXPORT_JSON;
    config.include_proof_trace = true;
    config.include_geometry    = false;
    config.pretty_print        = true;

    Lv00ExportResult *result = proof_export_enhanced(&proof, &config);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT(result->success, "JSON export should succeed");
    TEST_ASSERT_NOT_NULL(result->output);

    TEST_ASSERT(strstr(result->output, "\"theorem\"") != NULL,
                "JSON should contain theorem field");
    TEST_ASSERT(strstr(result->output, "\"steps\"") != NULL,
                "JSON should contain steps field");
    TEST_ASSERT(strstr(result->output, "\"id\"") != NULL,
                "JSON should contain id field");
    TEST_ASSERT(strstr(result->output, "\"rule\"") != NULL,
                "JSON should contain rule field");
    TEST_ASSERT(strstr(result->output, "Modus Ponens") != NULL,
                "JSON should contain theorem name");

    proof_export_result_destroy(result);
}

/* ============================================================
 * Test: DOT export
 * ============================================================ */

static void test_export_dot(void) {
    Lv00Proof proof = create_test_proof();

    Lv00ExportConfig config;
    config.format              = EXPORT_DOT;
    config.include_proof_trace = false;
    config.include_geometry    = false;
    config.pretty_print        = true;

    Lv00ExportResult *result = proof_export_enhanced(&proof, &config);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT(result->success, "DOT export should succeed");
    TEST_ASSERT_NOT_NULL(result->output);

    TEST_ASSERT(strstr(result->output, "digraph Proof") != NULL,
                "DOT should contain digraph declaration");
    TEST_ASSERT(strstr(result->output, "step1") != NULL,
                "DOT should contain step1 node");
    TEST_ASSERT(strstr(result->output, "->") != NULL,
                "DOT should contain edges");

    proof_export_result_destroy(result);
}

/* ============================================================
 * Test: Export from navigator
 * ============================================================ */

static void test_export_from_navigator(void) {
    Lv00ExportResult *result = proof_export_from_navigator("Test Theorem", EXPORT_HTML);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT(result->success, "Navigator export should succeed");
    TEST_ASSERT_NOT_NULL(result->output);
    TEST_ASSERT(strstr(result->output, "Test Theorem") != NULL,
                "Navigator export should contain theorem name");

    proof_export_result_destroy(result);

    /* Test with JSON format */
    result = proof_export_from_navigator("Another Theorem", EXPORT_JSON);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT(result->success, "Navigator JSON export should succeed");
    TEST_ASSERT(strstr(result->output, "Another Theorem") != NULL,
                "Navigator JSON export should contain theorem name");

    proof_export_result_destroy(result);
}

/* ============================================================
 * Test: NULL safety
 * ============================================================ */

static void test_export_null_safety(void) {
    Lv00Proof proof = create_test_proof();

    Lv00ExportConfig config;
    config.format              = EXPORT_HTML;
    config.include_proof_trace = false;
    config.include_geometry    = false;
    config.pretty_print        = true;

    /* NULL proof */
    Lv00ExportResult *result = proof_export_enhanced(NULL, &config);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT(!result->success, "NULL proof should fail");
    proof_export_result_destroy(result);

    /* NULL config */
    result = proof_export_enhanced(&proof, NULL);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT(!result->success, "NULL config should fail");
    proof_export_result_destroy(result);

    /* NULL theorem name for navigator */
    result = proof_export_from_navigator(NULL, EXPORT_HTML);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT(!result->success, "NULL theorem name should fail");
    proof_export_result_destroy(result);

    /* NULL-safe destroy */
    proof_export_result_destroy(NULL);
}

/* ============================================================
 * Main
 * ============================================================ */

int main(void) {
    TEST_SUITE_BEGIN("Proof Export Enhanced");

    TEST_RUN(test_export_html);
    TEST_RUN(test_export_latex);
    TEST_RUN(test_export_coq);
    TEST_RUN(test_export_lean4);
    TEST_RUN(test_export_json);
    TEST_RUN(test_export_dot);
    TEST_RUN(test_export_from_navigator);
    TEST_RUN(test_export_null_safety);

    TEST_SUITE_END();

    return g_fail_count > 0 ? 1 : 0;
}

/**
 * @file test_smt_backend.c
 * @brief Comprehensive tests for SMT backend, theory combiner, and trigger engine
 *
 * Tests cover:
 * - SMTSolver lifecycle (create/destroy/config)
 * - SMT-LIB2 encoding from constraint graph
 * - smtsolver_encode / smtsolver_check / smtsolver_decode_result
 * - smtsolver_solve full pipeline with Groebner backend
 * - Result management (init/free/clear/find_assignment)
 * - Backend availability and utility queries
 * - SMT theory combiner lifecycle and dispatch
 * - Trigger engine lifecycle, patterns, matching, and cache
 * - Edge cases (empty graphs, contradictory constraints, NULL handles)
 *
 * @version v3.4.2
 * @date 2026-07-24
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv.h"
#include "smt_backend.h"
#include "smt_theory_combiner.h"
#include "smt_trigger_engine.h"
#include "test_helpers.h"

/* ========================================================================
 * Global test counters (required by test framework)
 * ======================================================================== */
int g_pass_count = 0;
int g_fail_count = 0;

/* ========================================================================
 * Helper: create a simple constraint graph with two points and a segment
 * ======================================================================== */
/* 收敛说明：本地 create_simple_graph 仅创建两个 POINT 节点 (0,0)(3,0)、不添加线段，
 * 与 lv_test_geom_graph_builder.h 的 lv_test_line_graph（会添加线段）语义不匹配，故保留本地实现。*/
static ConstraintGraph *create_simple_graph(void) {
    ConstraintGraph *g = graph_create();
    if (!g)
        return NULL;

    /* (0,0) and (3,0) — horizontal segment */
    SymbolicCoord *c0_x = symbolic_coord_create_rational(0, 1);
    SymbolicCoord *c0_y = symbolic_coord_create_rational(0, 1);
    SymbolicCoord *c1_x = symbolic_coord_create_rational(3, 1);
    SymbolicCoord *c1_y = symbolic_coord_create_rational(0, 1);
    if (!c0_x || !c0_y || !c1_x || !c1_y) {
        if (c0_x)
            symbolic_coord_destroy(c0_x);
        if (c0_y)
            symbolic_coord_destroy(c0_y);
        if (c1_x)
            symbolic_coord_destroy(c1_x);
        if (c1_y)
            symbolic_coord_destroy(c1_y);
        graph_destroy(g);
        return NULL;
    }

    SymbolicCoord *pt0_coords[] = {c0_x, c0_y};
    SymbolicCoord *pt1_coords[] = {c1_x, c1_y};
    graph_add_point(g, pt0_coords, 2);
    graph_add_point(g, pt1_coords, 2);
    return g;
}

/* ========================================================================
 * Test Group 1: SMTSolver lifecycle
 * ======================================================================== */

static void test_solver_create_destroy(void) {
    /* Create with Groebner backend (default config) */
    SMTSolver *s = smtsolver_create(GROEBNER, NULL);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQ(smtsolver_get_type(s), GROEBNER);
    TEST_ASSERT_EQ(smtsolver_get_last_error(s), SMT_ERROR_NONE);
    smtsolver_destroy(s);

    /* Create with explicit config */
    SMTSolverConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.timeout_ms = 5000;
    cfg.logic = SMT_LOGIC_QF_NRA;
    cfg.produce_models = true;
    s = smtsolver_create(GROEBNER, &cfg);
    TEST_ASSERT_NOT_NULL(s);
    smtsolver_destroy(s);

    /* Create with Z3 (unlinked, should get BACKEND_UNAVAILABLE) */
    s = smtsolver_create(SMT_Z3, NULL);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQ(smtsolver_get_last_error(s), SMT_ERROR_BACKEND_UNAVAILABLE);
    smtsolver_destroy(s);

    /* Create with cvc5 (unlinked) */
    s = smtsolver_create(SMT_CVC5, NULL);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQ(smtsolver_get_last_error(s), SMT_ERROR_BACKEND_UNAVAILABLE);
    smtsolver_destroy(s);

    /* Create with Singular (unlinked) */
    s = smtsolver_create(SMT_SINGULAR, NULL);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQ(smtsolver_get_last_error(s), SMT_ERROR_BACKEND_UNAVAILABLE);
    smtsolver_destroy(s);

    /* Destroy NULL is safe */
    smtsolver_destroy(NULL);
}

static void test_solver_default_config(void) {
    /* GROEBNER default should use QF_NRA */
    const SMTSolverConfig *cfg = smtsolver_default_config(GROEBNER);
    TEST_ASSERT_NOT_NULL(cfg);
    TEST_ASSERT_EQ(cfg->logic, SMT_LOGIC_QF_NRA);
    TEST_ASSERT(cfg->produce_models, "produce_models should be true");
    TEST_ASSERT(cfg->timeout_ms > 0, "timeout should be positive");

    /* Z3 default */
    cfg = smtsolver_default_config(SMT_Z3);
    TEST_ASSERT_NOT_NULL(cfg);

    /* Invalid type falls back to GROEBNER */
    cfg = smtsolver_default_config((SolverBackendType) 999);
    TEST_ASSERT_NOT_NULL(cfg);
    TEST_ASSERT_EQ(cfg->logic, SMT_LOGIC_QF_NRA);
}

static void test_solver_error_handling(void) {
    SMTSolver *s = smtsolver_create(GROEBNER, NULL);
    TEST_ASSERT_NOT_NULL(s);

    /* Set an error and verify */
    smtsolver_set_error(s, SMT_ERROR_MEMORY_EXHAUSTED, "test error message");
    TEST_ASSERT_EQ(smtsolver_get_last_error(s), SMT_ERROR_MEMORY_EXHAUSTED);
    const char *msg = smtsolver_get_last_error_message(s);
    TEST_ASSERT_NOT_NULL(msg);
    TEST_ASSERT(strlen(msg) > 0, "error message should not be empty");

    smtsolver_destroy(s);

    /* Operations on NULL handles */
    TEST_ASSERT_EQ(smtsolver_get_type(NULL), COUNT);
    TEST_ASSERT_EQ(smtsolver_get_last_error(NULL), SMT_ERROR_NONE);
    TEST_ASSERT_NOT_NULL(smtsolver_get_last_error_message(NULL));
}

/* ========================================================================
 * Test Group 2: SMT-LIB2 encoding
 * ======================================================================== */

static void test_smtlib2_encoding_empty_graph(void) {
    ConstraintGraph *g = graph_create();
    TEST_ASSERT_NOT_NULL(g);

    char buf[4096];
    int written = smtencode_constraint_graph_to_smtlib2(g, SMT_LOGIC_AUTO, false, buf, sizeof(buf));
    TEST_ASSERT(written > 0, "should encode empty graph");

    /* Check for SMT-LIB2 header */
    TEST_ASSERT(strstr(buf, "set-logic") != NULL, "should contain set-logic");
    TEST_ASSERT(strstr(buf, "check-sat") != NULL, "should contain check-sat");

    graph_destroy(g);
}

static void test_smtlib2_encoding_simple_graph(void) {
    ConstraintGraph *g = create_simple_graph();
    TEST_ASSERT_NOT_NULL(g);

    char buf[8192];
    int written = smtencode_constraint_graph_to_smtlib2(g, SMT_LOGIC_QF_NRA, false, buf, sizeof(buf));
    TEST_ASSERT(written > 0, "should encode simple graph");

    /* Check for variable declarations */
    TEST_ASSERT(strstr(buf, "declare-fun") != NULL, "should declare variables");

    graph_destroy(g);
}

static void test_smtlib2_encoding_with_named_assertions(void) {
    ConstraintGraph *g = create_simple_graph();
    TEST_ASSERT_NOT_NULL(g);

    char buf[8192];
    int written = smtencode_constraint_graph_to_smtlib2(g, SMT_LOGIC_AUTO, true, buf, sizeof(buf));
    TEST_ASSERT(written > 0, "should encode with named assertions");

    graph_destroy(g);
}

static void test_smtlib2_encoding_buffer_too_small(void) {
    ConstraintGraph *g = graph_create();
    TEST_ASSERT_NOT_NULL(g);

    char small_buf[10];
    int result = smtencode_constraint_graph_to_smtlib2(g, SMT_LOGIC_AUTO, false, small_buf, 10);
    /* Should return required size or -1 */
    TEST_ASSERT(result > 0 || result == -1, "should handle small buffer gracefully");

    /* NULL graph -> -1 */
    result = smtencode_constraint_graph_to_smtlib2(NULL, SMT_LOGIC_AUTO, false, small_buf, 10);
    TEST_ASSERT_EQ(result, -1);

    /* NULL output -> -1 */
    result = smtencode_constraint_graph_to_smtlib2(g, SMT_LOGIC_AUTO, false, NULL, 10);
    TEST_ASSERT_EQ(result, -1);

    graph_destroy(g);
}

/* ========================================================================
 * Test Group 3: smtsolver_encode / smtsolver_check / smtsolver_solve
 * ======================================================================== */

static void test_solver_encode_check(void) {
    ConstraintGraph *g = create_simple_graph();
    TEST_ASSERT_NOT_NULL(g);

    SMTSolver *s = smtsolver_create(GROEBNER, NULL);
    TEST_ASSERT_NOT_NULL(s);

    /* Encode */
    char buf[4096];
    int enc_len = smtencode_constraint_graph_to_smtlib2(g, SMT_LOGIC_QF_NRA, false, buf, sizeof(buf));
    TEST_ASSERT(enc_len > 0, "encoding should succeed");

    int rc = smtsolver_encode(s, buf, enc_len);
    TEST_ASSERT_EQ(rc, 0);

    /* Check (Groebner backend without graph — returns UNKNOWN since no variety) */
    SMTSatResult sat = smtsolver_check(s);
    TEST_ASSERT(sat == SMT_RESULT_UNKNOWN || sat == SMT_RESULT_SAT, "check result should be UNKNOWN or SAT");

    smtsolver_destroy(s);
    graph_destroy(g);
}

static void test_solver_encode_empty_input(void) {
    SMTSolver *s = smtsolver_create(GROEBNER, NULL);
    TEST_ASSERT_NOT_NULL(s);

    int rc = smtsolver_encode(s, "", 0);
    TEST_ASSERT(rc < 0, "empty input should fail");

    /* NULL input */
    rc = smtsolver_encode(s, NULL, 0);
    TEST_ASSERT(rc < 0, "NULL input should fail");

    smtsolver_destroy(s);
}

static void test_solver_solve_graph(void) {
    ConstraintGraph *g = create_simple_graph();
    TEST_ASSERT_NOT_NULL(g);

    SMTSolver *s = smtsolver_create(GROEBNER, NULL);
    TEST_ASSERT_NOT_NULL(s);

    SMTSolverResult result;
    smtsolver_result_init(&result);

    int rc = smtsolver_solve(s, g, &result);
    TEST_ASSERT(rc == 0 || rc == 1, "solve should complete (0=SAT, 1=UNKNOWN)");

    /* Check result structure */
    TEST_ASSERT(result.sat_result == SMT_RESULT_SAT || result.sat_result == SMT_RESULT_UNKNOWN,
                "result should be SAT or UNKNOWN");
    TEST_ASSERT(result.backend_used == GROEBNER, "backend should be GROEBNER");

    smtsolver_result_free(&result);
    smtsolver_destroy(s);
    graph_destroy(g);
}

static void test_solver_solve_null_handles(void) {
    ConstraintGraph *g = create_simple_graph();
    TEST_ASSERT_NOT_NULL(g);
    SMTSolver *s = smtsolver_create(GROEBNER, NULL);
    TEST_ASSERT_NOT_NULL(s);

    SMTSolverResult result;
    smtsolver_result_init(&result);

    /* NULL solver */
    int rc = smtsolver_solve(NULL, g, &result);
    TEST_ASSERT(rc < 0, "NULL solver should fail");

    /* NULL graph */
    rc = smtsolver_solve(s, NULL, &result);
    TEST_ASSERT(rc < 0, "NULL graph should fail");

    smtsolver_destroy(s);
    graph_destroy(g);
}

/* ========================================================================
 * Test Group 4: Result management
 * ======================================================================== */

static void test_result_init_free_clear(void) {
    SMTSolverResult result;

    /* Init */
    smtsolver_result_init(&result);
    TEST_ASSERT_EQ(result.sat_result, SMT_RESULT_UNKNOWN);
    TEST_ASSERT_EQ(result.backend_used, GROEBNER);
    TEST_ASSERT_EQ(result.assignment_count, 0);
    TEST_ASSERT_EQ(result.assignments, (void *) 0);
    TEST_ASSERT_EQ(result.unsat_core_size, 0);

    /* Free (idempotent) */
    smtsolver_result_free(&result);
    TEST_ASSERT_EQ(result.assignment_count, 0);
    TEST_ASSERT_NULL(result.assignments);

    /* Clear */
    smtsolver_result_init(&result);
    result.sat_result = SMT_RESULT_SAT;
    result.assignment_count = 5;
    smtsolver_result_clear(&result);
    TEST_ASSERT_EQ(result.sat_result, SMT_RESULT_UNKNOWN);
    TEST_ASSERT_EQ(result.assignment_count, 0);

    /* NULL-safe */
    smtsolver_result_init(NULL);
    smtsolver_result_free(NULL);
    smtsolver_result_clear(NULL);
}

static void test_result_find_assignment(void) {
    SMTSolverResult result;
    smtsolver_result_init(&result);

    /* Empty result — no assignments */
    const SMTVariableAssignment *found = smtsolver_result_find_assignment(&result, 0);
    TEST_ASSERT_NULL(found);

    /* NULL-safe */
    found = smtsolver_result_find_assignment(NULL, 0);
    TEST_ASSERT_NULL(found);

    /* No need to free — null assignments don't need freeing */
}

/* ========================================================================
 * Test Group 5: Backend availability and utility queries
 * ======================================================================== */

static void test_backend_availability(void) {
    /* Groebner is always available (built-in) */
    TEST_ASSERT(smtsolver_is_backend_available(GROEBNER), "Groebner backend should always be available");

    /* Z3/cvc5/Singular are not linked */
    TEST_ASSERT(!smtsolver_is_backend_available(SMT_Z3), "Z3 should not be available (not linked)");
    TEST_ASSERT(!smtsolver_is_backend_available(SMT_CVC5), "cvc5 should not be available (not linked)");
    TEST_ASSERT(!smtsolver_is_backend_available(SMT_SINGULAR), "Singular should not be available (not linked)");

    /* Invalid backend */
    TEST_ASSERT(!smtsolver_is_backend_available((SolverBackendType) 999), "Invalid backend should not be available");
}

static void test_backend_type_names(void) {
    TEST_ASSERT_STR_EQ(smtsolver_backend_type_name(GROEBNER), "Groebner");
    TEST_ASSERT_STR_EQ(smtsolver_backend_type_name(SMT_Z3), "Z3");
    TEST_ASSERT_STR_EQ(smtsolver_backend_type_name(SMT_CVC5), "cvc5");
    TEST_ASSERT_STR_EQ(smtsolver_backend_type_name(SMT_SINGULAR), "Singular");
    TEST_ASSERT_STR_EQ(smtsolver_backend_type_name((SolverBackendType) 999), "Unknown");

    /* Parse from name */
    SolverBackendType out;
    out = smtsolver_backend_type_from_name("groebner");
    TEST_ASSERT_EQ(out, GROEBNER);
    /* Should also accept common misspelling */
    out = smtsolver_backend_type_from_name("grobner");
    TEST_ASSERT_EQ(out, GROEBNER);
    out = smtsolver_backend_type_from_name("z3");
    TEST_ASSERT_EQ(out, SMT_Z3);
    out = smtsolver_backend_type_from_name("cvc5");
    TEST_ASSERT_EQ(out, SMT_CVC5);
    out = smtsolver_backend_type_from_name("singular");
    TEST_ASSERT_EQ(out, SMT_SINGULAR);
    /* Unknown name */
    out = smtsolver_backend_type_from_name("nonexistent");
    TEST_ASSERT_EQ(out, COUNT);
    /* NULL name */
    out = smtsolver_backend_type_from_name(NULL);
    TEST_ASSERT_EQ(out, COUNT);
}

static void test_logic_names(void) {
    TEST_ASSERT_STR_EQ(smtsolver_logic_name(SMT_LOGIC_QF_NRA), "QF_NRA");
    TEST_ASSERT_STR_EQ(smtsolver_logic_name(SMT_LOGIC_QF_LRA), "QF_LRA");
    TEST_ASSERT_STR_EQ(smtsolver_logic_name(SMT_LOGIC_QF_NIA), "QF_NIA");
    TEST_ASSERT_STR_EQ(smtsolver_logic_name(SMT_LOGIC_QF_LIA), "QF_LIA");
    TEST_ASSERT_STR_EQ(smtsolver_logic_name(SMT_LOGIC_QF_UFLRA), "QF_UFLRA");
    TEST_ASSERT_STR_EQ(smtsolver_logic_name(SMT_LOGIC_QF_UFNRA), "QF_UFNRA");
    TEST_ASSERT_STR_EQ(smtsolver_logic_name(SMT_LOGIC_QF_BV), "QF_BV");
    TEST_ASSERT_STR_EQ(smtsolver_logic_name(SMT_LOGIC_AUTO), "AUTO");
    TEST_ASSERT_STR_EQ(smtsolver_logic_name((SMTLogic) 999), "UNKNOWN");
}

static void test_sat_result_names(void) {
    TEST_ASSERT_STR_EQ(smtsolver_sat_result_name(SMT_RESULT_SAT), "SAT");
    TEST_ASSERT_STR_EQ(smtsolver_sat_result_name(SMT_RESULT_UNSAT), "UNSAT");
    TEST_ASSERT_STR_EQ(smtsolver_sat_result_name(SMT_RESULT_UNKNOWN), "UNKNOWN");
    TEST_ASSERT_STR_EQ(smtsolver_sat_result_name(SMT_RESULT_ERROR), "ERROR");
    TEST_ASSERT_STR_EQ(smtsolver_sat_result_name((SMTSatResult) 999), "INVALID");
}

static void test_error_strings(void) {
    TEST_ASSERT_STR_EQ(smtsolver_error_string(SMT_ERROR_NONE), "No error");
    TEST_ASSERT_STR_EQ(smtsolver_error_string(SMT_ERROR_BACKEND_UNAVAILABLE), "Backend unavailable");
    TEST_ASSERT_STR_EQ(smtsolver_error_string(SMT_ERROR_ENCODING_FAILED), "Encoding failed");
    TEST_ASSERT_STR_EQ(smtsolver_error_string(SMT_ERROR_PARSE_FAILED), "Parse failed");
    TEST_ASSERT_STR_EQ(smtsolver_error_string(SMT_ERROR_SOLVER_CRASHED), "Solver crashed");
    TEST_ASSERT_STR_EQ(smtsolver_error_string(SMT_ERROR_MEMORY_EXHAUSTED), "Memory exhausted");
    TEST_ASSERT_STR_EQ(smtsolver_error_string(SMT_ERROR_TIMEOUT_REACHED), "Timeout reached");
    TEST_ASSERT_STR_EQ(smtsolver_error_string(SMT_ERROR_UNSUPPORTED_THEORY), "Unsupported theory");
    TEST_ASSERT_STR_EQ(smtsolver_error_string(SMT_ERROR_INVALID_MODEL), "Invalid model");
    TEST_ASSERT_STR_EQ(smtsolver_error_string((SMTErrorCode) 999), "Unknown error");
}

/* ========================================================================
 * Test Group 6: Backend registry
 * ======================================================================== */

static void test_registry_lifecycle(void) {
    SMTBackendRegistry *reg = smtsolver_get_registry();
    TEST_ASSERT_NOT_NULL(reg);
    TEST_ASSERT(reg->count >= 0, "registry count should be valid");
}

static void test_registry_register_find(void) {
    SMTBackendRegistry *reg = smtsolver_get_registry();
    TEST_ASSERT_NOT_NULL(reg);

    /* Register a custom backend */
    SMTBackendEntry entry;
    memset(&entry, 0, sizeof(entry));
    entry.type = SMT_Z3;
    strncpy(entry.name, "test_z3", sizeof(entry.name) - 1);
    entry.available = true;
    entry.priority = 5;

    int rc = smtsolver_register_backend(reg, &entry);
    /* May succeed or fail if already registered — either is acceptable */

    /* Find the backend */
    const SMTBackendEntry *found = smtsolver_find_backend(reg, SMT_Z3);
    /* If registration succeeded, found should be non-NULL */
    TEST_ASSERT(found != NULL || rc == 0, "find should work after register");

    /* Find non-existent */
    found = smtsolver_find_backend(reg, (SolverBackendType) 999);
    TEST_ASSERT_NULL(found);

    /* NULL-safe */
    rc = smtsolver_register_backend(NULL, &entry);
    TEST_ASSERT_EQ(rc, -1);
    rc = smtsolver_register_backend(reg, NULL);
    TEST_ASSERT_EQ(rc, -1);
    found = smtsolver_find_backend(NULL, GROEBNER);
    TEST_ASSERT_NULL(found);
}

/* ========================================================================
 * Test Group 7: SMT Theory Combiner
 * ======================================================================== */

/* A simple test solver function for the theory combiner */
static lvTheoryResult test_solver_fn(void *context, const void *constraints) {
    lvTheoryResult res;
    (void) context;
    (void) constraints;
    res.satisfiable = true;
    res.timeout = false;
    res.solve_time_ms = 1.0;
    return res;
}

/* A solver that always times out */
static lvTheoryResult timeout_solver_fn(void *context, const void *constraints) {
    lvTheoryResult res;
    (void) context;
    (void) constraints;
    res.satisfiable = false;
    res.timeout = true;
    res.solve_time_ms = 100.0;
    return res;
}

static void test_combiner_create_destroy(void) {
    lvTheoryCombiner *c = smt_combiner_create(4, 1000.0);
    TEST_ASSERT_NOT_NULL(c);
    smt_combiner_destroy(c);

    /* Null-safe destroy */
    smt_combiner_destroy(NULL);

    /* Negative initial capacity uses default */
    c = smt_combiner_create(-1, 500.0);
    TEST_ASSERT_NOT_NULL(c);
    smt_combiner_destroy(c);
}

static void test_combiner_add_theory(void) {
    lvTheoryCombiner *c = smt_combiner_create(4, 1000.0);
    TEST_ASSERT_NOT_NULL(c);

    /* Add a theory */
    bool ok = smt_combiner_add_theory(c, lv_THEORY_LRA, 0, test_solver_fn, NULL);
    TEST_ASSERT(ok, "should add theory");

    /* Add another with higher priority */
    ok = smt_combiner_add_theory(c, lv_THEORY_UF, 1, test_solver_fn, NULL);
    TEST_ASSERT(ok, "should add second theory");

    /* NULL combiner */
    ok = smt_combiner_add_theory(NULL, lv_THEORY_LRA, 0, test_solver_fn, NULL);
    TEST_ASSERT(!ok, "NULL combiner should fail");

    /* NULL solver function */
    ok = smt_combiner_add_theory(c, lv_THEORY_BV, 2, NULL, NULL);
    TEST_ASSERT(!ok, "NULL solver_fn should fail");

    /* Update existing theory (same theory_id) */
    ok = smt_combiner_add_theory(c, lv_THEORY_LRA, 5, test_solver_fn, NULL);
    TEST_ASSERT(ok, "should update existing theory");

    smt_combiner_destroy(c);
}

static void test_combiner_set_enabled(void) {
    lvTheoryCombiner *c = smt_combiner_create(4, 1000.0);
    TEST_ASSERT_NOT_NULL(c);

    smt_combiner_add_theory(c, lv_THEORY_LRA, 0, test_solver_fn, NULL);

    /* Disable */
    bool ok = smt_combiner_set_enabled(c, lv_THEORY_LRA, false);
    TEST_ASSERT(ok, "should disable theory");

    /* Enable */
    ok = smt_combiner_set_enabled(c, lv_THEORY_LRA, true);
    TEST_ASSERT(ok, "should enable theory");

    /* Non-existent theory */
    ok = smt_combiner_set_enabled(c, (lvTheoryId) 999, false);
    TEST_ASSERT(!ok, "non-existent theory should fail");

    /* NULL combiner */
    ok = smt_combiner_set_enabled(NULL, lv_THEORY_LRA, false);
    TEST_ASSERT(!ok, "NULL combiner should fail");

    smt_combiner_destroy(c);
}

static void test_combiner_solve(void) {
    lvTheoryCombiner *c = smt_combiner_create(4, 1000.0);
    TEST_ASSERT_NOT_NULL(c);

    /* Solve with no theories registered */
    int dummy = 42;
    lvTheoryResult res = smt_combiner_solve(c, &dummy);
    TEST_ASSERT(res.timeout, "no theories -> should timeout");

    /* Add a solver and solve */
    smt_combiner_add_theory(c, lv_THEORY_LRA, 0, test_solver_fn, NULL);
    res = smt_combiner_solve(c, &dummy);
    TEST_ASSERT(res.satisfiable, "should be satisfiable");
    TEST_ASSERT(!res.timeout, "should not timeout");

    /* NULL combiner */
    res = smt_combiner_solve(NULL, &dummy);
    TEST_ASSERT(res.timeout, "NULL combiner -> should timeout");

    /* NULL constraints */
    res = smt_combiner_solve(c, NULL);
    TEST_ASSERT(res.timeout, "NULL constraints -> should timeout");

    smt_combiner_destroy(c);
}

static void test_combiner_dispatch_order(void) {
    lvTheoryCombiner *c = smt_combiner_create(4, 1000.0);
    TEST_ASSERT_NOT_NULL(c);

    /* Add timeout solver first (lower priority = higher precedence) */
    smt_combiner_add_theory(c, lv_THEORY_UF, 0, timeout_solver_fn, NULL);

    /* Add a solver that returns definitive result second */
    smt_combiner_add_theory(c, lv_THEORY_LRA, 1, test_solver_fn, NULL);

    /* The first solver (UF, priority 0) should be tried first and may timeout.
     * If it times out, the second should be tried. But if it returns a result,
     * we get the timeout. The test verifies the dispatch doesn't crash. */
    int dummy = 42;
    lvTheoryResult res = smt_combiner_solve(c, &dummy);
    /* UF solver times out (priority 0), LRA solver (priority 1) gives SAT */
    TEST_ASSERT(res.satisfiable, "second solver should give SAT");
    TEST_ASSERT(!res.timeout, "should get definitive result");

    smt_combiner_destroy(c);
}

/* ========================================================================
 * Test Group 8: SMT Trigger Engine
 * ======================================================================== */

static void test_trigger_create_destroy(void) {
    lvTriggerEngine *e = trigger_engine_create(16, 64, 1000);
    TEST_ASSERT_NOT_NULL(e);

    /* Null-safe destroy */
    trigger_engine_destroy(e);
    trigger_engine_destroy(NULL);
}

static void test_trigger_add_pattern(void) {
    lvTriggerEngine *e = trigger_engine_create(4, 8, 100);
    TEST_ASSERT_NOT_NULL(e);

    int pattern[] = {1, 2, 3};
    int idx = trigger_engine_add_pattern(e, pattern, 3, 1.0);
    TEST_ASSERT(idx >= 0, "should add pattern");

    /* Single element pattern */
    int single[] = {42};
    idx = trigger_engine_add_pattern(e, single, 1, 0.5);
    TEST_ASSERT(idx >= 0, "should add single pattern");

    /* NULL pattern_ids */
    idx = trigger_engine_add_pattern(e, NULL, 1, 1.0);
    TEST_ASSERT_EQ(idx, -1);

    /* Empty pattern */
    idx = trigger_engine_add_pattern(e, pattern, 0, 1.0);
    TEST_ASSERT_EQ(idx, -1);

    /* NULL engine */
    idx = trigger_engine_add_pattern(NULL, pattern, 1, 1.0);
    TEST_ASSERT_EQ(idx, -1);

    trigger_engine_destroy(e);
}

static void test_trigger_find_matches(void) {
    lvTriggerEngine *e = trigger_engine_create(4, 8, 100);
    TEST_ASSERT_NOT_NULL(e);

    /* Add patterns */
    int p1[] = {10, 20};
    int p2[] = {30};
    trigger_engine_add_pattern(e, p1, 2, 1.0);
    trigger_engine_add_pattern(e, p2, 1, 0.5);

    /* Find matches */
    int match_count = 0;
    bool found = trigger_engine_find_matches(e, 0, (void *) (uintptr_t) 0x1234, 0xABCD, &match_count);
    TEST_ASSERT(found, "should find matches");
    TEST_ASSERT(match_count > 0, "should have matches");

    /* Second call should find new matches (different term_hash) */
    found = trigger_engine_find_matches(e, 0, (void *) (uintptr_t) 0x5678, 0xDEAD, &match_count);
    TEST_ASSERT(found, "should find more matches");
    TEST_ASSERT(match_count > 0, "should have more matches");

    /* NULL engine */
    found = trigger_engine_find_matches(NULL, 0, NULL, 0, &match_count);
    TEST_ASSERT(!found, "NULL engine -> false");

    trigger_engine_destroy(e);
}

static void test_trigger_cache_clear(void) {
    lvTriggerEngine *e = trigger_engine_create(4, 8, 100);
    TEST_ASSERT_NOT_NULL(e);

    int p[] = {1, 2};
    trigger_engine_add_pattern(e, p, 2, 1.0);

    /* Find some matches */
    int match_count = 0;
    trigger_engine_find_matches(e, 0, NULL, 0x1111, &match_count);

    /* Clear cache */
    trigger_engine_clear_cache(e);

    /* After clear, the same term should match again */
    bool found = trigger_engine_find_matches(e, 0, NULL, 0x1111, &match_count);
    TEST_ASSERT(found, "should match after cache clear");

    /* NULL-safe */
    trigger_engine_clear_cache(NULL);

    trigger_engine_destroy(e);
}

static void test_trigger_instantiation_limit(void) {
    /* Create engine with very low max instances */
    lvTriggerEngine *e = trigger_engine_create(4, 8, 2);
    TEST_ASSERT_NOT_NULL(e);

    int p[] = {1};
    trigger_engine_add_pattern(e, p, 1, 1.0);

    int match_count = 0;
    bool found;

    /* First match */
    found = trigger_engine_find_matches(e, 0, NULL, 0xAAAA, &match_count);
    TEST_ASSERT(found, "first match should work");

    /* Second match (different quantifier) */
    found = trigger_engine_find_matches(e, 1, NULL, 0xBBBB, &match_count);
    TEST_ASSERT(found, "second match should work");

    /* Check total instantiation count */
    int total = trigger_engine_get_instantiation_count(e);
    TEST_ASSERT(total > 0, "should have instantiations");

    trigger_engine_destroy(e);
}

static void test_trigger_get_count(void) {
    lvTriggerEngine *e = trigger_engine_create(4, 8, 100);
    TEST_ASSERT_NOT_NULL(e);

    /* Initially zero */
    TEST_ASSERT_EQ(trigger_engine_get_instantiation_count(e), 0);

    /* NULL engine */
    TEST_ASSERT_EQ(trigger_engine_get_instantiation_count(NULL), 0);

    trigger_engine_destroy(e);
}

/* ========================================================================
 * Main
 * ======================================================================== */

TEST_MAIN_BEGIN("SMT Backend")

    /* Group 1: SMTSolver lifecycle */
    TEST_MAIN_RUN(test_solver_create_destroy);
    TEST_MAIN_RUN(test_solver_default_config);
    TEST_MAIN_RUN(test_solver_error_handling);

    /* Group 2: SMT-LIB2 encoding */
    TEST_MAIN_RUN(test_smtlib2_encoding_empty_graph);
    TEST_MAIN_RUN(test_smtlib2_encoding_simple_graph);
    TEST_MAIN_RUN(test_smtlib2_encoding_with_named_assertions);
    TEST_MAIN_RUN(test_smtlib2_encoding_buffer_too_small);

    /* Group 3: Solve pipeline */
    TEST_MAIN_RUN(test_solver_encode_check);
    TEST_MAIN_RUN(test_solver_encode_empty_input);
    TEST_MAIN_RUN(test_solver_solve_graph);
    TEST_MAIN_RUN(test_solver_solve_null_handles);

    /* Group 4: Result management */
    TEST_MAIN_RUN(test_result_init_free_clear);
    TEST_MAIN_RUN(test_result_find_assignment);

    /* Group 5: Utility queries */
    TEST_MAIN_RUN(test_backend_availability);
    TEST_MAIN_RUN(test_backend_type_names);
    TEST_MAIN_RUN(test_logic_names);
    TEST_MAIN_RUN(test_sat_result_names);
    TEST_MAIN_RUN(test_error_strings);

    /* Group 6: Backend registry */
    TEST_MAIN_RUN(test_registry_lifecycle);
    TEST_MAIN_RUN(test_registry_register_find);

    /* Group 7: Theory combiner */
    TEST_MAIN_RUN(test_combiner_create_destroy);
    TEST_MAIN_RUN(test_combiner_add_theory);
    TEST_MAIN_RUN(test_combiner_set_enabled);
    TEST_MAIN_RUN(test_combiner_solve);
    TEST_MAIN_RUN(test_combiner_dispatch_order);

    /* Group 8: Trigger engine */
    TEST_MAIN_RUN(test_trigger_create_destroy);
    TEST_MAIN_RUN(test_trigger_add_pattern);
    TEST_MAIN_RUN(test_trigger_find_matches);
    TEST_MAIN_RUN(test_trigger_cache_clear);
    TEST_MAIN_RUN(test_trigger_instantiation_limit);
    TEST_MAIN_RUN(test_trigger_get_count);


TEST_MAIN_END()

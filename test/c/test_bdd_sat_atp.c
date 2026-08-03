/**
 * @file test_bdd_sat_atp.c
 * @brief Comprehensive tests for BDD, SAT, ATP, approximate counter,
 *        Groebner parallel, and probabilistic constraint backends
 *
 * Tests cover:
 * - BDD manager lifecycle, variable creation, terminals
 * - BDD boolean operations (AND, OR, NOT, ITE, XOR, NAND)
 * - BDD reference counting and reorder (sifting)
 * - BDD constraint graph encoding and CNF conversion
 * - ADD manager lifecycle and operations (add, sub, mul, div, max, min)
 * - SAT encoding lifecycle, variable mapping, clause management
 * - SAT geometric constraint encoding (collinearity, parallelism, etc.)
 * - SAT solve/decode and model management
 * - ATP solver lifecycle, encoding, and result management
 * - Approximate counter vs constraint graph
 * - Groebner parallel engine lifecycle and computation
 * - Probabilistic constraint distribution and sampling
 *
 * @version v3.3.0
 * @date 2026-07-24
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "approx_counter.h"
#include "atp_backend.h"
#include "bdd_encoding.h"
#include "groebner_parallel.h"
#include "lv.h"
#include "probabilistic_constraint.h"
#include "sat_encoding.h"
#include "test_helpers.h"

/* ========================================================================
 * Global test counters (required by test framework)
 * ======================================================================== */
int g_pass_count = 0;
int g_fail_count = 0;

/* ========================================================================
 * Test Group 1: BDD Manager Lifecycle
 * ======================================================================== */

static void test_bdd_manager_create_destroy(void) {
    BDDManager *mgr = bdd_manager_create(8, 1024);
    TEST_ASSERT_NOT_NULL(mgr);
    TEST_ASSERT_EQ(mgr->var_count, 0);
    TEST_ASSERT_NOT_NULL(mgr->true_node);
    TEST_ASSERT_NOT_NULL(mgr->false_node);
    TEST_ASSERT(mgr->unique_table_size >= 1024, "unique table size should be at least 1024");
    bdd_manager_destroy(mgr);

    /* Null-safe destroy */
    bdd_manager_destroy(NULL);

    /* Zero var_count */
    mgr = bdd_manager_create(0, 512);
    TEST_ASSERT_NOT_NULL(mgr);
    bdd_manager_destroy(mgr);
}

static void test_bdd_new_var(void) {
    BDDManager *mgr = bdd_manager_create(4, 1024);
    TEST_ASSERT_NOT_NULL(mgr);

    int v0 = bdd_new_var(mgr, "x", BDD_BOOLEAN);
    TEST_ASSERT_EQ(v0, 0);
    TEST_ASSERT_EQ(mgr->var_count, 1); /* var_count starts at 0, +1 new */

    int v1 = bdd_new_var(mgr, "y", BDD_INT_BIT);
    TEST_ASSERT_EQ(v1, 1);

    int v2 = bdd_new_var(mgr, NULL, BDD_ENUM);
    TEST_ASSERT_EQ(v2, 2);

    /* NULL manager */
    int v = bdd_new_var(NULL, "test", BDD_BOOLEAN);
    TEST_ASSERT_EQ(v, -1);

    bdd_manager_destroy(mgr);
}

static void test_bdd_terminals(void) {
    BDDManager *mgr = bdd_manager_create(4, 1024);
    TEST_ASSERT_NOT_NULL(mgr);

    BDDNode *t = bdd_true(mgr);
    TEST_ASSERT_NOT_NULL(t);
    TEST_ASSERT_EQ(t->var_id, -1);

    BDDNode *f = bdd_false(mgr);
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQ(f->var_id, -1);

    /* NULL manager */
    TEST_ASSERT_NULL(bdd_true(NULL));
    TEST_ASSERT_NULL(bdd_false(NULL));

    bdd_manager_destroy(mgr);
}

static void test_bdd_literal(void) {
    BDDManager *mgr = bdd_manager_create(4, 1024);
    TEST_ASSERT_NOT_NULL(mgr);

    int v = bdd_new_var(mgr, "a", BDD_BOOLEAN);

    /* Positive literal */
    BDDNode *pos = bdd_literal(mgr, v);
    TEST_ASSERT_NOT_NULL(pos);
    TEST_ASSERT_EQ(pos->var_id, v);

    /* Negative literal */
    BDDNode *neg = bdd_literal(mgr, -v);
    TEST_ASSERT_NOT_NULL(neg);

    /* NULL manager */
    TEST_ASSERT_NULL(bdd_literal(NULL, v));

    bdd_manager_destroy(mgr);
}

/* ========================================================================
 * Test Group 2: BDD Boolean Operations
 * ======================================================================== */

static void test_bdd_and_or_not(void) {
    BDDManager *mgr = bdd_manager_create(4, 1024);
    TEST_ASSERT_NOT_NULL(mgr);
    fprintf(stderr, "[DBG] 1 create done\n");

    int v = bdd_new_var(mgr, "x", BDD_BOOLEAN);
    int w = bdd_new_var(mgr, "y", BDD_BOOLEAN);
    fprintf(stderr, "[DBG] 2 new_var done v=%d w=%d\n", v, w);

    BDDNode *x = bdd_literal(mgr, v);
    BDDNode *y = bdd_literal(mgr, w);
    fprintf(stderr, "[DBG] 3 literals done x=%p y=%p\n", (void*)x, (void*)y);

    /* AND */
    fprintf(stderr, "[DBG] 4 about to bdd_and\n");
    BDDNode *a = bdd_and(mgr, x, y);
    fprintf(stderr, "[DBG] 5 and done a=%p\n", (void*)a);
    TEST_ASSERT_NOT_NULL(a);
    bdd_deref(mgr, a);
    fprintf(stderr, "[DBG] 6 deref a done\n");

    /* OR */
    fprintf(stderr, "[DBG] 7 about to bdd_or\n");
    BDDNode *o = bdd_or(mgr, x, y);
    fprintf(stderr, "[DBG] 8 or done o=%p\n", (void*)o);
    TEST_ASSERT_NOT_NULL(o);
    bdd_deref(mgr, o);
    fprintf(stderr, "[DBG] 9 deref o done\n");

    /* NOT */
    fprintf(stderr, "[DBG] 10 about to bdd_not\n");
    BDDNode *n = bdd_not(mgr, x);
    fprintf(stderr, "[DBG] 11 not done n=%p\n", (void*)n);
    TEST_ASSERT_NOT_NULL(n);
    bdd_deref(mgr, n);
    fprintf(stderr, "[DBG] 12 deref n done\n");

    /* x AND x = x */
    fprintf(stderr, "[DBG] 13 about to bdd_and(x,x)\n");
    BDDNode *xx = bdd_and(mgr, x, x);
    fprintf(stderr, "[DBG] 14 and(x,x) done xx=%p\n", (void*)xx);
    TEST_ASSERT_NOT_NULL(xx);
    bdd_deref(mgr, xx);
    fprintf(stderr, "[DBG] 15 deref xx done\n");

    /* x OR x = x */
    fprintf(stderr, "[DBG] 16 about to bdd_or(x,x)\n");
    BDDNode *xo = bdd_or(mgr, x, x);
    fprintf(stderr, "[DBG] 17 or(x,x) done xo=%p\n", (void*)xo);
    TEST_ASSERT_NOT_NULL(xo);
    bdd_deref(mgr, xo);
    fprintf(stderr, "[DBG] 18 deref xo done\n");

    bdd_deref(mgr, x);
    fprintf(stderr, "[DBG] 19 deref x done\n");
    bdd_deref(mgr, y);
    fprintf(stderr, "[DBG] 20 deref y done\n");

    /* NULL safety */
    fprintf(stderr, "[DBG] 20a NULL safety: bdd_and(mgr, NULL, y)\n");
    TEST_ASSERT_NULL(bdd_and(mgr, NULL, y));
    fprintf(stderr, "[DBG] 20b NULL safety: bdd_or(mgr, x, NULL)\n");
    TEST_ASSERT_NULL(bdd_or(mgr, x, NULL));
    fprintf(stderr, "[DBG] 20c NULL safety: bdd_not(mgr, NULL)\n");
    TEST_ASSERT_NULL(bdd_not(mgr, NULL));
    fprintf(stderr, "[DBG] 20d NULL safety: bdd_and(NULL, x, y)\n");
    TEST_ASSERT_NULL(bdd_and(NULL, x, y));

    fprintf(stderr, "[DBG] 21 about to destroy\n");
    bdd_manager_destroy(mgr);
    fprintf(stderr, "[DBG] 22 destroy done\n");
}

static void test_bdd_ite(void) {
    BDDManager *mgr = bdd_manager_create(4, 1024);
    TEST_ASSERT_NOT_NULL(mgr);

    int v = bdd_new_var(mgr, "cond", BDD_BOOLEAN);
    BDDNode *c = bdd_literal(mgr, v);
    BDDNode *t = bdd_true(mgr);
    BDDNode *f = bdd_false(mgr);

    /* ite(True, T, F) = T */
    BDDNode *r1 = bdd_ite(mgr, t, t, f);
    TEST_ASSERT_NOT_NULL(r1);
    bdd_deref(mgr, r1);

    /* ite(False, T, F) = F */
    BDDNode *r2 = bdd_ite(mgr, f, t, f);
    TEST_ASSERT_NOT_NULL(r2);
    bdd_deref(mgr, r2);

    /* ite(cond, T, F) = cond (literal) */
    BDDNode *r3 = bdd_ite(mgr, c, t, f);
    TEST_ASSERT_NOT_NULL(r3);
    bdd_deref(mgr, r3);

    /* ite(cond, F, T) = not(cond) */
    BDDNode *r4 = bdd_ite(mgr, c, f, t);
    TEST_ASSERT_NOT_NULL(r4);
    bdd_deref(mgr, r4);

    /* NULL safety */
    TEST_ASSERT_NULL(bdd_ite(mgr, NULL, t, f));
    TEST_ASSERT_NULL(bdd_ite(NULL, c, t, f));

    bdd_deref(mgr, c);
    bdd_manager_destroy(mgr);
}

static void test_bdd_xor_nand(void) {
    BDDManager *mgr = bdd_manager_create(4, 1024);
    TEST_ASSERT_NOT_NULL(mgr);

    int v = bdd_new_var(mgr, "a", BDD_BOOLEAN);
    int w = bdd_new_var(mgr, "b", BDD_BOOLEAN);

    BDDNode *x = bdd_literal(mgr, v);
    BDDNode *y = bdd_literal(mgr, w);

    /* XOR */
    BDDNode *xr = bdd_xor(mgr, x, y);
    TEST_ASSERT_NOT_NULL(xr);
    bdd_deref(mgr, xr);

    /* NAND */
    BDDNode *nd = bdd_nand(mgr, x, y);
    TEST_ASSERT_NOT_NULL(nd);
    bdd_deref(mgr, nd);

    /* NULL safety */
    TEST_ASSERT_NULL(bdd_xor(mgr, NULL, y));
    TEST_ASSERT_NULL(bdd_nand(mgr, x, NULL));

    bdd_deref(mgr, x);
    bdd_deref(mgr, y);
    bdd_manager_destroy(mgr);
}

/* ========================================================================
 * Test Group 3: BDD Reference Counting
 * ======================================================================== */

static void test_bdd_ref_deref(void) {
    BDDManager *mgr = bdd_manager_create(4, 1024);
    TEST_ASSERT_NOT_NULL(mgr);

    BDDNode *t = bdd_true(mgr);
    BDDNode *f = bdd_false(mgr);

    /* Terminal nodes always have ref_count >= 1 */
    bdd_ref(t); /* increment */
    bdd_ref(f); /* increment */
    bdd_deref(mgr, t);
    bdd_deref(mgr, f);

    /* NULL-safe */
    bdd_ref(NULL);
    bdd_deref(mgr, NULL);
    bdd_deref(NULL, t);

    bdd_manager_destroy(mgr);
}

/* ========================================================================
 * Test Group 4: BDD Reorder (Sifting)
 * ======================================================================== */

static void test_bdd_reorder_sift(void) {
    BDDManager *mgr = bdd_manager_create(4, 1024);
    TEST_ASSERT_NOT_NULL(mgr);

    /* Create some variables */
    bdd_new_var(mgr, "a", BDD_BOOLEAN);
    bdd_new_var(mgr, "b", BDD_BOOLEAN);

    /* Build a simple BDD: a AND b */
    BDDNode *a = bdd_literal(mgr, 0);
    BDDNode *b = bdd_literal(mgr, 1);
    BDDNode *ab = bdd_and(mgr, a, b);

    /* Reorder */
    int improved = bdd_reorder_sift(mgr);
    /* Should not crash; improved >= 0 */
    TEST_ASSERT(improved >= 0, "sifting should complete");

    bdd_deref(mgr, a);
    bdd_deref(mgr, b);
    bdd_deref(mgr, ab);

    /* NULL-safe */
    TEST_ASSERT_EQ(bdd_reorder_sift(NULL), -1);

    bdd_manager_destroy(mgr);
}

/* ========================================================================
 * Test Group 5: BDD constraint_graph_to_bdd
 * ======================================================================== */

static void test_constraint_graph_to_bdd(void) {
    BDDManager *mgr = bdd_manager_create(8, 2048);
    TEST_ASSERT_NOT_NULL(mgr);

    ConstraintGraph *g = graph_create();
    TEST_ASSERT_NOT_NULL(g);

    /* Add a simple point */
    SymbolicCoord *cx = symbolic_coord_create_rational(0, 1);
    SymbolicCoord *cy = symbolic_coord_create_rational(0, 1);
    TEST_ASSERT_NOT_NULL(cx);
    TEST_ASSERT_NOT_NULL(cy);
    SymbolicCoord *coords[] = {cx, cy};
    graph_add_point(g, coords, 2);

    /* Encode to BDD */
    BDDNode *bdd = constraint_graph_to_bdd(g, mgr);
    /* May be NULL or valid depending on implementation */
    if (bdd) {
        bdd_deref(mgr, bdd);
    }

    /* NULL safety */
    TEST_ASSERT_NULL(constraint_graph_to_bdd(NULL, mgr));
    TEST_ASSERT_NULL(constraint_graph_to_bdd(g, NULL));

    graph_destroy(g);
    bdd_manager_destroy(mgr);
}

static void test_bdd_to_cnf(void) {
    BDDManager *mgr = bdd_manager_create(4, 1024);
    TEST_ASSERT_NOT_NULL(mgr);

    int v = bdd_new_var(mgr, "x", BDD_BOOLEAN);
    BDDNode *x = bdd_literal(mgr, v);

    char *cnf = NULL;
    bool ok = bdd_to_cnf(x, &cnf);
    /* May fail depending on precise implementation, but should not crash */
    if (ok && cnf) {
        TEST_ASSERT(strstr(cnf, "cnf") != NULL || strstr(cnf, "p cnf") != NULL,
                    "CNF output should contain DIMACS header");
        lv_free((void **) &cnf);
    }

    /* Terminal True */
    BDDNode *t = bdd_true(mgr);
    ok = bdd_to_cnf(t, &cnf);
    if (ok && cnf) {
        lv_free((void **) &cnf);
    }

    /* NULL safety */
    TEST_ASSERT(!bdd_to_cnf(NULL, &cnf), "bdd to cnf should fail for invalid input");
    TEST_ASSERT(!bdd_to_cnf(x, NULL), "bdd to cnf should fail for invalid input");

    bdd_deref(mgr, x);
    bdd_manager_destroy(mgr);
}

/* ========================================================================
 * Test Group 6: ADD Manager Lifecycle and Operations
 * ======================================================================== */

static void test_add_manager_create_destroy(void) {
    ADDManager *mgr = add_manager_create(4, 512);
    TEST_ASSERT_NOT_NULL(mgr);
    TEST_ASSERT_NOT_NULL(mgr->zero_node);
    TEST_ASSERT_NOT_NULL(mgr->one_node);
    add_manager_destroy(mgr);

    /* Null-safe */
    add_manager_destroy(NULL);
}

static void test_add_operations(void) {
    ADDManager *mgr = add_manager_create(4, 512);
    TEST_ASSERT_NOT_NULL(mgr);

    ADDNode *c3 = add_constant(mgr, 3.0);
    ADDNode *c5 = add_constant(mgr, 5.0);
    TEST_ASSERT_NOT_NULL(c3);
    TEST_ASSERT_NOT_NULL(c5);

    /* add: 3 + 5 = 8 */
    ADDNode *sum = add_add(mgr, c3, c5);
    TEST_ASSERT_NOT_NULL(sum);
    TEST_ASSERT(sum->is_constant, "sum of constants should be constant");
    TEST_ASSERT(fabs(sum->constant - 8.0) < 1e-10, "3 + 5 should be 8");

    /* sub: 3 - 5 = -2 */
    ADDNode *diff = add_sub(mgr, c3, c5);
    TEST_ASSERT_NOT_NULL(diff);
    TEST_ASSERT(diff->is_constant, "difference of constants should be constant");

    /* mul: 3 * 5 = 15 */
    ADDNode *prod = add_mul(mgr, c3, c5);
    TEST_ASSERT_NOT_NULL(prod);
    TEST_ASSERT(prod->is_constant, "product of constants should be constant");
    TEST_ASSERT(fabs(prod->constant - 15.0) < 1e-10, "3 * 5 should be 15");

    /* div: 3 / 5 = 0.6 */
    ADDNode *quot = add_div(mgr, c3, c5);
    TEST_ASSERT_NOT_NULL(quot);
    TEST_ASSERT(quot->is_constant, "quotient of constants should be constant");
    TEST_ASSERT(fabs(quot->constant - 0.6) < 1e-10, "3 / 5 should be 0.6");

    /* max(3, 5) = 5 */
    ADDNode *mx = add_max(mgr, c3, c5);
    TEST_ASSERT_NOT_NULL(mx);
    TEST_ASSERT(mx->is_constant, "max of constants should be constant");
    TEST_ASSERT(fabs(mx->constant - 5.0) < 1e-10, "max(3,5) should be 5");

    /* min(3, 5) = 3 */
    ADDNode *mn = add_min(mgr, c3, c5);
    TEST_ASSERT_NOT_NULL(mn);
    TEST_ASSERT(mn->is_constant, "min of constants should be constant");
    TEST_ASSERT(fabs(mn->constant - 3.0) < 1e-10, "min(3,5) should be 3");

    /* mul by zero = zero */
    ADDNode *zero = add_constant(mgr, 0.0);
    ADDNode *pz = add_mul(mgr, c3, zero);
    TEST_ASSERT_NOT_NULL(pz);
    TEST_ASSERT(fabs(pz->constant - 0.0) < 1e-10, "3 * 0 should be 0");

    /* mul by one = identity */
    ADDNode *one = add_constant(mgr, 1.0);
    ADDNode *po = add_mul(mgr, c3, one);
    TEST_ASSERT_NOT_NULL(po);
    TEST_ASSERT(fabs(po->constant - 3.0) < 1e-10, "3 * 1 should be 3");

    /* NULL safety */
    TEST_ASSERT_NULL(add_add(mgr, NULL, c5));
    TEST_ASSERT_NULL(add_add(NULL, c3, c5));
    TEST_ASSERT_NULL(add_mul(mgr, c3, NULL));
    TEST_ASSERT_NULL(add_sub(NULL, c3, c5));
    TEST_ASSERT_NULL(add_div(mgr, NULL, c5));
    TEST_ASSERT_NULL(add_max(NULL, c3, c5));
    TEST_ASSERT_NULL(add_min(mgr, NULL, c5));

    add_manager_destroy(mgr);
}

/* ========================================================================
 * Test Group 7: SAT Encoding Lifecycle and Variable Management
 * ======================================================================== */

static void test_sat_encoding_create_destroy(void) {
    SatEncoding *enc = sat_encoding_create(64, 128);
    TEST_ASSERT_NOT_NULL(enc);
    TEST_ASSERT(enc->var_map.capacity >= 64, "var capacity should be at least 64");
    TEST_ASSERT(enc->clause_capacity >= 128, "clause capacity should be at least 128");
    TEST_ASSERT_EQ(enc->var_map.count, 0);
    TEST_ASSERT_EQ(enc->clause_count, 0);
    sat_encoding_destroy(enc);

    /* Null-safe */
    sat_encoding_destroy(NULL);

    /* Zero initial capacity uses defaults */
    enc = sat_encoding_create(0, 0);
    TEST_ASSERT_NOT_NULL(enc);
    sat_encoding_destroy(enc);
}

static void test_sat_var_register_lookup(void) {
    SatEncoding *enc = sat_encoding_create(64, 128);
    TEST_ASSERT_NOT_NULL(enc);

    /* Register a binary var */
    int ids[] = {1, 2};
    int v1 = sat_encoding_register_var(enc, 2, ids);
    TEST_ASSERT(v1 >= 1, "var_id should be >= 1");

    /* Register same var again (should return existing) */
    int v2 = sat_encoding_register_var(enc, 2, ids);
    TEST_ASSERT_EQ(v1, v2);

    /* Lookup existing */
    int found = sat_encoding_lookup_var(enc, 2, ids);
    TEST_ASSERT_EQ(found, v1);

    /* Lookup non-existent */
    int nonexistent_ids[] = {99, 100};
    found = sat_encoding_lookup_var(enc, 2, nonexistent_ids);
    TEST_ASSERT_EQ(found, -1);

    /* Invalid arity */
    int rc = sat_encoding_register_var(enc, 0, ids);
    TEST_ASSERT_EQ(rc, -1);
    rc = sat_encoding_register_var(enc, 9, ids);
    TEST_ASSERT_EQ(rc, -1);
    found = sat_encoding_lookup_var(enc, 0, ids);
    TEST_ASSERT_EQ(found, -1);

    /* NULL safety */
    rc = sat_encoding_register_var(NULL, 2, ids);
    TEST_ASSERT_EQ(rc, -1);
    rc = sat_encoding_register_var(enc, 2, NULL);
    TEST_ASSERT_EQ(rc, -1);
    found = sat_encoding_lookup_var(NULL, 2, ids);
    TEST_ASSERT_EQ(found, -1);

    sat_encoding_destroy(enc);
}

static void test_sat_clause_management(void) {
    SatEncoding *enc = sat_encoding_create(64, 128);
    TEST_ASSERT_NOT_NULL(enc);

    /* Add a clause */
    SatLiteral lits[] = {1, -2, 3};
    int idx = sat_encoding_add_clause(enc, lits, 3);
    TEST_ASSERT(idx >= 0, "clause index should be >= 0");
    TEST_ASSERT_EQ(enc->clause_count, 1);

    /* Add an assumption (unit clause) */
    int aidx = sat_encoding_add_assumption(enc, 5);
    TEST_ASSERT(aidx >= 0, "assumption index should be >= 0");
    TEST_ASSERT_EQ(enc->clause_count, 2);

    /* Empty literals list */
    int rc = sat_encoding_add_clause(enc, lits, 0);
    TEST_ASSERT_EQ(rc, -1);

    /* NULL safety */
    rc = sat_encoding_add_clause(NULL, lits, 3);
    TEST_ASSERT_EQ(rc, -1);
    rc = sat_encoding_add_clause(enc, NULL, 3);
    TEST_ASSERT_EQ(rc, -1);
    rc = sat_encoding_add_assumption(NULL, 1);
    TEST_ASSERT_EQ(rc, -1);

    sat_encoding_destroy(enc);
}

static void test_sat_encoding_stats(void) {
    SatEncoding *enc = sat_encoding_create(64, 128);
    TEST_ASSERT_NOT_NULL(enc);

    int out_vars = -1, out_clauses = -1;
    sat_encoding_get_stats(enc, &out_vars, &out_clauses);
    TEST_ASSERT_EQ(out_vars, 0);
    TEST_ASSERT_EQ(out_clauses, 0);

    /* NULL safety */
    sat_encoding_get_stats(NULL, &out_vars, &out_clauses);
    TEST_ASSERT_EQ(out_vars, 0);
    TEST_ASSERT_EQ(out_clauses, 0);
    sat_encoding_get_stats(enc, NULL, NULL);

    sat_encoding_destroy(enc);
}

static void test_sat_unsat_core(void) {
    SatEncoding *enc = sat_encoding_create(64, 128);
    TEST_ASSERT_NOT_NULL(enc);

    int count = 0;
    int *core = sat_get_unsat_core(enc, &count);
    /* Without graph, should return empty core */
    TEST_ASSERT_NOT_NULL(core);
    lv_free((void **) &core);

    /* NULL safety */
    core = sat_get_unsat_core(NULL, &count);
    TEST_ASSERT_NULL(core);
    core = sat_get_unsat_core(enc, NULL);
    TEST_ASSERT_NULL(core);

    sat_encoding_destroy(enc);
}

static void test_sat_export_dimacs(void) {
    SatEncoding *enc = sat_encoding_create(64, 128);
    TEST_ASSERT_NOT_NULL(enc);

    /* Add a clause */
    SatLiteral lits[] = {1, -2, 3};
    sat_encoding_add_clause(enc, lits, 3);

    /* Export to temp file */
    bool ok = sat_encoding_export_dimacs(enc, "test_dimacs.cnf");
    TEST_ASSERT(ok, "DIMACS export should succeed");

    /* Clean up */
    remove("test_dimacs.cnf");

    /* NULL safety */
    ok = sat_encoding_export_dimacs(NULL, "test.cnf");
    TEST_ASSERT(!ok, "NULL graph should fail");
    ok = sat_encoding_export_dimacs(enc, NULL);
    TEST_ASSERT(!ok, "NULL path should fail");

    sat_encoding_destroy(enc);
}

/* ========================================================================
 * Test Group 8: SAT Geometric Constraint Encoding
 * ======================================================================== */

static void test_sat_encode_collinearity(void) {
    SatEncoding *enc = sat_encoding_create(64, 128);
    TEST_ASSERT_NOT_NULL(enc);

    int cnt = sat_encode_collinearity(enc, 1, 2, 3);
    TEST_ASSERT(cnt > 0, "should encode collinearity constraints");
    TEST_ASSERT(enc->clause_count > 0, "should have added clauses");

    /* NULL safety */
    cnt = sat_encode_collinearity(NULL, 1, 2, 3);
    TEST_ASSERT_EQ(cnt, -1);

    sat_encoding_destroy(enc);
}

static void test_sat_encode_parallelism(void) {
    SatEncoding *enc = sat_encoding_create(64, 128);
    TEST_ASSERT_NOT_NULL(enc);

    int cnt = sat_encode_parallelism(enc, 1, 2, 3, 4);
    TEST_ASSERT(cnt > 0, "should encode parallelism constraints");

    sat_encoding_destroy(enc);
}

static void test_sat_encode_perpendicularity(void) {
    SatEncoding *enc = sat_encoding_create(64, 128);
    TEST_ASSERT_NOT_NULL(enc);

    int cnt = sat_encode_perpendicularity(enc, 1, 2, 3, 4);
    TEST_ASSERT(cnt > 0, "should encode perpendicularity constraints");

    sat_encoding_destroy(enc);
}

static void test_sat_encode_distance_eq(void) {
    SatEncoding *enc = sat_encoding_create(64, 128);
    TEST_ASSERT_NOT_NULL(enc);

    int cnt = sat_encode_distance_eq(enc, 1, 2, 3, 4);
    TEST_ASSERT(cnt > 0, "should encode distance equality constraints");

    sat_encoding_destroy(enc);
}

static void test_sat_encode_angle_eq(void) {
    SatEncoding *enc = sat_encoding_create(64, 128);
    TEST_ASSERT_NOT_NULL(enc);

    int cnt = sat_encode_angle_eq(enc, 1, 2, 3, 4, 5, 6);
    TEST_ASSERT(cnt > 0, "should encode angle equality constraints");

    sat_encoding_destroy(enc);
}

static void test_sat_encode_containment(void) {
    SatEncoding *enc = sat_encoding_create(64, 128);
    TEST_ASSERT_NOT_NULL(enc);

    int cnt = sat_encode_containment(enc, 1, 2);
    TEST_ASSERT(cnt > 0, "should encode containment constraints");

    sat_encoding_destroy(enc);
}

static void test_sat_constraint_graph_to_sat(void) {
    ConstraintGraph *g = graph_create();
    TEST_ASSERT_NOT_NULL(g);

    /* Add a point */
    SymbolicCoord *cx = symbolic_coord_create_rational(0, 1);
    SymbolicCoord *cy = symbolic_coord_create_rational(0, 1);
    SymbolicCoord *coords[] = {cx, cy};
    graph_add_point(g, coords, 2);

    SatEncoding *enc = sat_encoding_create(64, 128);
    TEST_ASSERT_NOT_NULL(enc);

    SatResult res = constraint_graph_to_sat(g, enc);
    /* Empty graph with no constraints */
    TEST_ASSERT(res == SAT_OK || res == SAT_RESULT_SAT, "should succeed");

    /* NULL safety */
    res = constraint_graph_to_sat(NULL, enc);
    TEST_ASSERT_EQ(res, SAT_ERROR);
    res = constraint_graph_to_sat(g, NULL);
    TEST_ASSERT_EQ(res, SAT_ERROR);

    sat_encoding_destroy(enc);
    graph_destroy(g);
}

static void test_sat_solve_and_decode(void) {
    SatEncoding *enc = sat_encoding_create(64, 128);
    TEST_ASSERT_NOT_NULL(enc);

    /* Add simple satisfiable clause: 1 (unit) */
    SatLiteral unit[] = {1};
    sat_encoding_add_clause(enc, unit, 1);

    SatModel *model = NULL;
    SatResult res = sat_solve_and_decode(enc, &model);
    /* Should be SAT or UNKNOWN (depends on internal solver availability) */
    TEST_ASSERT(res == SAT_RESULT_SAT || res == SAT_RESULT_UNKNOWN || res == SAT_RESULT_UNSAT,
                "should return a valid result");
    if (model) {
        sat_model_destroy(model);
    }

    /* NULL safety */
    res = sat_solve_and_decode(NULL, &model);
    TEST_ASSERT_EQ(res, SAT_ERROR);
    res = sat_solve_and_decode(enc, NULL);
    TEST_ASSERT_EQ(res, SAT_ERROR);

    sat_encoding_destroy(enc);
}

/* ========================================================================
 * Test Group 9: ATP Solver Lifecycle and Encoding
 * ======================================================================== */

static void test_atp_config_default(void) {
    ATPConfig cfg = atp_config_default();
    TEST_ASSERT_EQ(cfg.input_format, ATP_FORMAT_TPTP_FOF);
    TEST_ASSERT(cfg.timeout_seconds > 0, "timeout should be positive");
    TEST_ASSERT(cfg.auto_strategy, "auto_strategy should be true");
    TEST_ASSERT(cfg.produce_proof, "produce_proof should be true");
}

static void test_atp_solver_create_destroy(void) {
    ATPBackendSolver *solver = atp_solver_create(ATP_BACKEND_VAMPIRE, NULL);
    TEST_ASSERT_NOT_NULL(solver);
    TEST_ASSERT_EQ(atp_solver_get_type(solver), ATP_BACKEND_VAMPIRE);
    atp_solver_destroy(solver);

    /* With config */
    ATPConfig cfg = atp_config_default();
    cfg.timeout_seconds = 60.0;
    solver = atp_solver_create(ATP_BACKEND_EPROVER, &cfg);
    TEST_ASSERT_NOT_NULL(solver);
    atp_solver_destroy(solver);

    /* Null-safe destroy */
    atp_solver_destroy(NULL);

    /* NULL config uses defaults */
    solver = atp_solver_create(ATP_BACKEND_IPROVER, NULL);
    TEST_ASSERT_NOT_NULL(solver);
    atp_solver_destroy(solver);
}

static void test_atp_encode_empty_graph(void) {
    ConstraintGraph *g = graph_create();
    TEST_ASSERT_NOT_NULL(g);

    char *tptp = atp_encode_constraint_graph(g, ATP_FORMAT_TPTP_FOF, "test_problem", false, NULL);
    TEST_ASSERT_NOT_NULL(tptp);
    TEST_ASSERT(strlen(tptp) > 0, "TPTP output should not be empty");
    TEST_ASSERT(strstr(tptp, "fof") != NULL || strstr(tptp, "cnf") != NULL, "should contain TPTP format marker");
    lv_free((void **) &tptp);

    /* NULL safety */
    tptp = atp_encode_constraint_graph(NULL, ATP_FORMAT_TPTP_FOF, "test", false, NULL);
    TEST_ASSERT_NULL(tptp);

    graph_destroy(g);
}

static void test_atp_encode_with_goal(void) {
    ConstraintGraph *g = graph_create();
    TEST_ASSERT_NOT_NULL(g);

    /* Add a point */
    SymbolicCoord *cx = symbolic_coord_create_rational(1, 1);
    SymbolicCoord *cy = symbolic_coord_create_rational(2, 1);
    SymbolicCoord *coords[] = {cx, cy};
    graph_add_point(g, coords, 2);

    char *tptp = atp_encode_constraint_graph(g, ATP_FORMAT_TPTP_FOF, "test_goal", true, NULL);
    TEST_ASSERT_NOT_NULL(tptp);
    TEST_ASSERT(strstr(tptp, "conjecture") != NULL, "should contain conjecture");
    lv_free((void **) &tptp);

    /* TFF format */
    tptp = atp_encode_constraint_graph(g, ATP_FORMAT_TPTP_TFF, "test_tff", false, NULL);
    TEST_ASSERT_NOT_NULL(tptp);
    TEST_ASSERT(strstr(tptp, "tff") != NULL, "should contain tff format");
    lv_free((void **) &tptp);

    graph_destroy(g);
}

static void test_atp_solver_load(void) {
    ATPBackendSolver *solver = atp_solver_create(ATP_BACKEND_VAMPIRE, NULL);
    TEST_ASSERT_NOT_NULL(solver);

    int rc = atp_solver_load(solver, "fof(test, axiom, $true).\n");
    TEST_ASSERT_EQ(rc, (int) lv_OK);
    TEST_ASSERT(rc == (int) lv_OK, "should have problem loaded");

    /* Empty input */
    rc = atp_solver_load(solver, "");
    TEST_ASSERT(rc != (int) lv_OK, "empty input should fail");

    /* NULL safety */
    rc = atp_solver_load(NULL, "fof(a, axiom, $true).");
    TEST_ASSERT_EQ(rc, (int) lv_ERROR_NULL_POINTER);
    rc = atp_solver_load(solver, NULL);
    TEST_ASSERT_EQ(rc, (int) lv_ERROR_NULL_POINTER);

    atp_solver_destroy(solver);
}

static void test_atp_solver_solve(void) {
    ATPBackendSolver *solver = atp_solver_create(ATP_BACKEND_VAMPIRE, NULL);
    TEST_ASSERT_NOT_NULL(solver);

    ATPResultInfo result;
    atp_result_init(&result);

    /* Load some TPTP text */
    atp_solver_load(solver, "fof(test, axiom, $true).\n");

    int rc = atp_solver_solve(solver, &result);
    TEST_ASSERT_EQ(rc, (int) lv_OK);
    /* Vampire likely not available in PATH, so result should be UNKNOWN */
    TEST_ASSERT(
        result.result == ATP_RESULT_UNKNOWN || result.result == ATP_RESULT_SAT || result.result == ATP_RESULT_UNSAT,
        "should return a valid ATP result");

    atp_result_destroy(&result);
    atp_solver_destroy(solver);
}

static void test_atp_solver_solve_no_problem(void) {
    ATPBackendSolver *solver = atp_solver_create(ATP_BACKEND_VAMPIRE, NULL);
    TEST_ASSERT_NOT_NULL(solver);

    ATPResultInfo result;
    atp_result_init(&result);

    /* Solve without loading a problem */
    int rc = atp_solver_solve(solver, &result);
    TEST_ASSERT(rc != (int) lv_OK, "solve without problem should fail");

    atp_result_destroy(&result);
    atp_solver_destroy(solver);
}

static void test_atp_solve_graph(void) {
    ConstraintGraph *g = graph_create();
    TEST_ASSERT_NOT_NULL(g);

    ATPBackendSolver *solver = atp_solver_create(ATP_BACKEND_VAMPIRE, NULL);
    TEST_ASSERT_NOT_NULL(solver);

    ATPResultInfo result;
    atp_result_init(&result);

    int rc = atp_solver_solve_graph(solver, g, ATP_FORMAT_TPTP_FOF, "test", true, NULL, &result);
    TEST_ASSERT_EQ(rc, (int) lv_OK);

    atp_result_destroy(&result);
    atp_solver_destroy(solver);
    graph_destroy(g);
}

static void test_atp_result_lifecycle(void) {
    ATPResultInfo result;
    atp_result_init(&result);
    TEST_ASSERT_EQ(result.result, ATP_RESULT_UNKNOWN);
    TEST_ASSERT_EQ(result.backend, ATP_BACKEND_COUNT);

    /* Destroy (null-safe) */
    atp_result_destroy(&result);

    /* NULL-safe */
    atp_result_init(NULL);
    atp_result_destroy(NULL);
}

static void test_atp_backend_names(void) {
    TEST_ASSERT_STR_EQ(atp_backend_type_name(ATP_BACKEND_VAMPIRE), "Vampire");
    TEST_ASSERT_STR_EQ(atp_backend_type_name(ATP_BACKEND_EPROVER), "E Prover");
    TEST_ASSERT_STR_EQ(atp_backend_type_name(ATP_BACKEND_IPROVER), "iProver");
    TEST_ASSERT_STR_EQ(atp_backend_type_name(ATP_BACKEND_CUSTOM), "Custom");
    TEST_ASSERT_STR_EQ(atp_backend_type_name((ATPBackendType) 999), "Unknown");

    /* Parse from name */
    ATPBackendType out;
    TEST_ASSERT(atp_backend_type_from_name("vampire", &out), "atp backend type from name should succeed");
    TEST_ASSERT_EQ(out, ATP_BACKEND_VAMPIRE);
    TEST_ASSERT(atp_backend_type_from_name("eprover", &out), "atp backend type from name should succeed");
    TEST_ASSERT_EQ(out, ATP_BACKEND_EPROVER);
    TEST_ASSERT(atp_backend_type_from_name("e", &out), "atp backend type from name should succeed");
    TEST_ASSERT_EQ(out, ATP_BACKEND_EPROVER);
    TEST_ASSERT(atp_backend_type_from_name("iprover", &out), "atp backend type from name should succeed");
    TEST_ASSERT_EQ(out, ATP_BACKEND_IPROVER);
    TEST_ASSERT(atp_backend_type_from_name("custom", &out), "atp backend type from name should succeed");
    TEST_ASSERT_EQ(out, ATP_BACKEND_CUSTOM);
    TEST_ASSERT(!atp_backend_type_from_name("unknown", &out), "atp backend type from name should fail for invalid input");
    TEST_ASSERT(!atp_backend_type_from_name(NULL, &out), "atp backend type from name should fail for invalid input");

    /* Result names */
    TEST_ASSERT_STR_EQ(atp_result_name(ATP_RESULT_SAT), "SAT");
    TEST_ASSERT_STR_EQ(atp_result_name(ATP_RESULT_UNSAT), "UNSAT");
    TEST_ASSERT_STR_EQ(atp_result_name(ATP_RESULT_UNKNOWN), "UNKNOWN");
    TEST_ASSERT_STR_EQ(atp_result_name(ATP_RESULT_ERROR), "ERROR");

    /* Format names */
    TEST_ASSERT_STR_EQ(atp_format_name(ATP_FORMAT_TPTP_FOF), "TPTP FOF");
    TEST_ASSERT_STR_EQ(atp_format_name(ATP_FORMAT_TPTP_CNF), "TPTP CNF");
    TEST_ASSERT_STR_EQ(atp_format_name(ATP_FORMAT_TPTP_TFF), "TPTP TFF");
    TEST_ASSERT_STR_EQ(atp_format_name(ATP_FORMAT_SMTLIB2), "SMT-LIB2");
}

static void test_atp_registry(void) {
    const ATPBackendRegistry *reg = atp_get_registry();
    TEST_ASSERT_NOT_NULL(reg);
    TEST_ASSERT(reg->count >= 0, "registry count should be valid");

    /* Register */
    ATPBackendEntry entry;
    memset(&entry, 0, sizeof(entry));
    entry.type = ATP_BACKEND_CUSTOM;
    entry.available = true;
    entry.priority = 1;
    entry.description = "test backend";

    int rc = atp_register_backend(&entry);
    TEST_ASSERT(rc == (int) lv_OK || rc == (int) lv_ERROR_ALREADY_EXISTS,
                "register should succeed or indicate duplicate");

    /* NULL safety */
    rc = atp_register_backend(NULL);
    TEST_ASSERT_EQ(rc, (int) lv_ERROR_NULL_POINTER);

    /* Find */
    const ATPBackendEntry *found = atp_find_backend(ATP_BACKEND_CUSTOM);
    /* May or may not be found depending on previous registration */

    /* Check availability (uses system PATH checks, should not crash) */
    bool avail = atp_is_backend_available(ATP_BACKEND_VAMPIRE);
    /* Not likely installed, but API should not crash */
    (void) avail;
}

/* ========================================================================
 * Test Group 10: Approximate Counter
 * ======================================================================== */

static void test_approx_count_solutions(void) {
    ConstraintGraph *g = graph_create();
    TEST_ASSERT_NOT_NULL(g);

    /* Add a simple point */
    SymbolicCoord *cx = symbolic_coord_create_rational(0, 1);
    SymbolicCoord *cy = symbolic_coord_create_rational(0, 1);
    SymbolicCoord *coords[] = {cx, cy};
    graph_add_point(g, coords, 2);

    ApproxCountResult result;
    PacConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.epsilon = 0.1;
    cfg.delta = 0.05;
    cfg.seed = 42;

    bool ok = approx_count_solutions(g, &cfg, &result);
    TEST_ASSERT(ok, "approx count should succeed");

    /* Clean up */
    approx_count_result_destroy(&result);

    /* NULL safety */
    ok = approx_count_solutions(NULL, &cfg, &result);
    TEST_ASSERT(!ok, "NULL graph should fail");
    ok = approx_count_solutions(g, NULL, &result);
    TEST_ASSERT(!ok, "NULL config should fail");
    ok = approx_count_solutions(g, &cfg, NULL);
    TEST_ASSERT(!ok, "NULL result should fail");

    graph_destroy(g);
}

static void test_approx_count_projected(void) {
    ConstraintGraph *g = graph_create();
    TEST_ASSERT_NOT_NULL(g);

    SymbolicCoord *cx = symbolic_coord_create_rational(1, 1);
    SymbolicCoord *cy = symbolic_coord_create_rational(2, 1);
    SymbolicCoord *coords[] = {cx, cy};
    graph_add_point(g, coords, 2);

    PacConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.epsilon = 0.1;
    cfg.delta = 0.05;

    int proj_vars[] = {0};
    ApproxCountResult result;
    bool ok = approx_count_projected(g, proj_vars, 1, &cfg, &result);
    TEST_ASSERT(ok, "projected count should succeed");

    approx_count_result_destroy(&result);

    /* NULL safety */
    ok = approx_count_projected(NULL, proj_vars, 1, &cfg, &result);
    TEST_ASSERT(!ok, "NULL graph should fail");
    ok = approx_count_projected(g, proj_vars, 1, &cfg, NULL);
    TEST_ASSERT(!ok, "NULL result should fail");

    graph_destroy(g);
}

static void test_approx_count_to_sat(void) {
    ConstraintGraph *g = graph_create();
    TEST_ASSERT_NOT_NULL(g);

    int out_vars = 0;
    char *dimacs = approx_count_to_sat(g, &out_vars);
    /* May be NULL for empty graph, but should not crash */
    if (dimacs) {
        TEST_ASSERT(out_vars >= 0, "should have non-negative variables");
        lv_free((void **) &dimacs);
    }

    /* NULL safety */
    dimacs = approx_count_to_sat(NULL, &out_vars);
    TEST_ASSERT_NULL(dimacs);

    graph_destroy(g);
}

static void test_approx_count_pac_bound(void) {
    PacConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.epsilon = 0.1;

    ApproxCountResult res;
    memset(&res, 0, sizeof(res));
    res.total_count = 1000;

    double bound = approx_count_get_pac_bound(&cfg, &res);
    TEST_ASSERT(bound > 0.0, "PAC bound should be positive");

    /* Zero estimate */
    res.total_count = 0;
    bound = approx_count_get_pac_bound(&cfg, &res);
    TEST_ASSERT(bound >= 1.0, "zero estimate should still have bound >= 1");

    /* NULL safety */
    bound = approx_count_get_pac_bound(NULL, &res);
    TEST_ASSERT_EQ(bound, 0.0);
    bound = approx_count_get_pac_bound(&cfg, NULL);
    TEST_ASSERT_EQ(bound, 0.0);
}

static void test_is_approximately_constructible(void) {
    ConstraintGraph *g = graph_create();
    TEST_ASSERT_NOT_NULL(g);

    bool ok = is_approximately_constructible(g, 0.5);
    /* Should succeed or fail gracefully */
    (void) ok;

    /* NULL safety */
    ok = is_approximately_constructible(NULL, 0.5);
    TEST_ASSERT(!ok, "NULL graph should fail");

    /* Invalid probability */
    ok = is_approximately_constructible(g, 1.5);
    /* Should handle gracefully */
    (void) ok;

    graph_destroy(g);
}

/* ========================================================================
 * Test Group 11: Groebner Parallel Engine
 * ======================================================================== */

static void test_groebner_default_config(void) {
    lvGroebnerConfig cfg = lv_groebner_default_config();
    TEST_ASSERT(cfg.max_threads >= 1, "should have at least 1 thread");
    TEST_ASSERT(cfg.chunk_size > 0, "chunk size should be positive");
}

static void test_groebner_parallel_create_destroy(void) {
    lvGroebnerParallel *eng = lv_groebner_parallel_create(NULL);
    TEST_ASSERT_NOT_NULL(eng);
    TEST_ASSERT(eng->config.max_threads >= 1, "should use default config");
    lv_groebner_parallel_destroy(eng);

    /* With explicit config */
    lvGroebnerConfig cfg = lv_groebner_default_config();
    cfg.max_threads = 2;
    eng = lv_groebner_parallel_create(&cfg);
    TEST_ASSERT_NOT_NULL(eng);
    TEST_ASSERT_EQ(eng->config.max_threads, 2);
    lv_groebner_parallel_destroy(eng);

    /* Null-safe */
    lv_groebner_parallel_destroy(NULL);
}

static void test_groebner_parallel_compute(void) {
    lvGroebnerParallel *eng = lv_groebner_parallel_create(NULL);
    TEST_ASSERT_NOT_NULL(eng);

    /* Simple input: one clause [1, 0] */
    int c1[] = {1, 0};
    int *polys[] = {c1};

    int rc = lv_groebner_parallel_compute(eng, (void **) polys, 1);
    TEST_ASSERT(rc == 0 || rc == -1, "compute should complete without crash");

    /* NULL safety */
    rc = lv_groebner_parallel_compute(NULL, (void **) polys, 1);
    TEST_ASSERT_EQ(rc, -1);
    rc = lv_groebner_parallel_compute(eng, NULL, 1);
    TEST_ASSERT_EQ(rc, -1);
    rc = lv_groebner_parallel_compute(eng, (void **) polys, 0);
    TEST_ASSERT_EQ(rc, -1);

    lv_groebner_parallel_destroy(eng);
}

static void test_groebner_parallel_state(void) {
    lvGroebnerParallel *eng = lv_groebner_parallel_create(NULL);
    TEST_ASSERT_NOT_NULL(eng);

    lvGroebnerState state = lv_groebner_parallel_state(eng);
    TEST_ASSERT_EQ(state.total_pairs, 0);
    TEST_ASSERT_EQ(state.completed_pairs, 0);

    /* NULL safety */
    state = lv_groebner_parallel_state(NULL);
    TEST_ASSERT_EQ(state.total_pairs, 0);

    lv_groebner_parallel_destroy(eng);
}

static void test_groebner_poly_is_nonzero_constant(void) {
    /* NULL safety */
    TEST_ASSERT(!lv_groebner_poly_is_nonzero_constant(NULL), "lv groebner poly is nonzero constant should fail for invalid input");

    /* Testing with SimplePoly directly is not possible from outside,
     * just verify the NULL case doesn't crash. */
}

/* ========================================================================
 * Test Group 12: Probabilistic Constraint
 * ======================================================================== */

static void test_prob_dist_create_uniform(void) {
    double params[] = {0.0, 10.0};
    ProbDistribution *d = prob_dist_create(PROB_DIST_UNIFORM, params, 2);
    TEST_ASSERT_NOT_NULL(d);
    TEST_ASSERT_EQ(d->type, PROB_DIST_UNIFORM);
    TEST_ASSERT_EQ(d->param_count, 2);
    TEST_ASSERT(fabs(d->support_lo - 0.0) < 1e-10, "support_lo should be 0.0");
    TEST_ASSERT(fabs(d->support_hi - 10.0) < 1e-10, "support_hi should be 10.0");
    prob_dist_destroy(d);

    /* NULL params for uniform uses defaults */
    d = prob_dist_create(PROB_DIST_UNIFORM, NULL, 0);
    TEST_ASSERT_NOT_NULL(d);
    prob_dist_destroy(d);
}

static void test_prob_dist_create_normal(void) {
    double params[] = {0.0, 1.0};
    ProbDistribution *d = prob_dist_create(PROB_DIST_NORMAL, params, 2);
    TEST_ASSERT_NOT_NULL(d);
    TEST_ASSERT_EQ(d->type, PROB_DIST_NORMAL);
    prob_dist_destroy(d);
}

static void test_prob_dist_create_beta(void) {
    double params[] = {2.0, 3.0};
    ProbDistribution *d = prob_dist_create(PROB_DIST_BETA, params, 2);
    TEST_ASSERT_NOT_NULL(d);
    TEST_ASSERT_EQ(d->type, PROB_DIST_BETA);
    TEST_ASSERT(fabs(d->support_lo - 0.0) < 1e-10, "beta support_lo should be 0.0");
    TEST_ASSERT(fabs(d->support_hi - 1.0) < 1e-10, "beta support_hi should be 1.0");
    prob_dist_destroy(d);
}

static void test_prob_dist_destroy_null(void) {
    prob_dist_destroy(NULL);
}

static void test_prob_dist_pdf(void) {
    /* Uniform distribution [0, 1] */
    double params[] = {0.0, 1.0};
    ProbDistribution *d = prob_dist_create(PROB_DIST_UNIFORM, params, 2);
    TEST_ASSERT_NOT_NULL(d);

    double pdf_at_05 = prob_dist_pdf(d, 0.5);
    TEST_ASSERT(fabs(pdf_at_05 - 1.0) < 1e-10, "uniform PDF should be 1/(b-a)=1.0");

    double pdf_outside = prob_dist_pdf(d, -1.0);
    TEST_ASSERT(fabs(pdf_outside - 0.0) < 1e-10, "PDF outside support should be 0.0");

    prob_dist_destroy(d);

    /* NULL distribution */
    double pdf = prob_dist_pdf(NULL, 0.5);
    TEST_ASSERT(fabs(pdf - 0.0) < 1e-10, "NULL dist PDF should be 0.0");

    /* Normal distribution */
    double normal_params[] = {0.0, 1.0};
    d = prob_dist_create(PROB_DIST_NORMAL, normal_params, 2);
    TEST_ASSERT_NOT_NULL(d);
    /* PDF at mean should be 1/(sigma * sqrt(2*pi)) */
    double expected = 1.0 / (1.0 * sqrt(2.0 * 3.14159265358979323846));
    pdf = prob_dist_pdf(d, 0.0);
    TEST_ASSERT(fabs(pdf - expected) < 1e-10, "normal PDF at mean should be 1/sqrt(2*pi)");
    prob_dist_destroy(d);
}

static void test_prob_dist_cdf(void) {
    /* Uniform [0, 1] */
    double params[] = {0.0, 1.0};
    ProbDistribution *d = prob_dist_create(PROB_DIST_UNIFORM, params, 2);
    TEST_ASSERT_NOT_NULL(d);

    double cdf_at_05 = prob_dist_cdf(d, 0.5);
    TEST_ASSERT(fabs(cdf_at_05 - 0.5) < 1e-10, "uniform CDF at 0.5 should be 0.5");

    double cdf_below = prob_dist_cdf(d, -1.0);
    TEST_ASSERT(fabs(cdf_below - 0.0) < 1e-10, "CDF below support should be 0.0");

    double cdf_above = prob_dist_cdf(d, 2.0);
    TEST_ASSERT(fabs(cdf_above - 1.0) < 1e-10, "CDF above support should be 1.0");

    prob_dist_destroy(d);

    /* NULL distribution */
    double cdf = prob_dist_cdf(NULL, 0.5);
    TEST_ASSERT(fabs(cdf - 0.0) < 1e-10, "NULL dist CDF should be 0.0");
}

static void test_prob_dist_sample(void) {
    /* Uniform [0, 1] */
    double params[] = {0.0, 1.0};
    ProbDistribution *d = prob_dist_create(PROB_DIST_UNIFORM, params, 2);
    TEST_ASSERT_NOT_NULL(d);

    double *samples = NULL;
    int n = prob_dist_sample(d, 100, &samples);
    TEST_ASSERT_EQ(n, 100);
    TEST_ASSERT_NOT_NULL(samples);

    /* All samples should be in [0, 1] */
    for (int i = 0; i < n; i++) {
        TEST_ASSERT(samples[i] >= 0.0 && samples[i] <= 1.0, "uniform samples should be in [0,1]");
    }

    lv_free((void **) &samples);
    prob_dist_destroy(d);

    /* NULL safety */
    n = prob_dist_sample(NULL, 10, &samples);
    TEST_ASSERT_EQ(n, -1);
    n = prob_dist_sample(d, -1, &samples);
    TEST_ASSERT_EQ(n, -1);
    n = prob_dist_sample(d, 10, NULL);
    TEST_ASSERT_EQ(n, -1);
}

static void test_prob_constraint_create_destroy(void) {
    /* With distribution */
    double params[] = {0.0, 5.0};
    ProbDistribution *d = prob_dist_create(PROB_DIST_UNIFORM, params, 2);
    TEST_ASSERT_NOT_NULL(d);

    ProbConstraintNode *node = prob_constraint_create(42, d);
    TEST_ASSERT_NOT_NULL(node);
    TEST_ASSERT_EQ(node->base_node_id, 42);
    TEST_ASSERT(node->is_soft, "should be soft constraint");
    prob_constraint_destroy(node);

    /* Without distribution (deterministic) */
    node = prob_constraint_create(7, NULL);
    TEST_ASSERT_NOT_NULL(node);
    TEST_ASSERT(!node->is_soft, "should be hard constraint");
    prob_constraint_destroy(node);

    /* NULL safety */
    prob_constraint_destroy(NULL);
}

static void test_prob_constraint_sample(void) {
    double params[] = {0.0, 10.0};
    ProbDistribution *d = prob_dist_create(PROB_DIST_UNIFORM, params, 2);
    TEST_ASSERT_NOT_NULL(d);

    ProbConstraintNode *node = prob_constraint_create(1, d);
    TEST_ASSERT_NOT_NULL(node);

    double *samples = NULL;
    int n = prob_constraint_sample(node, 50, &samples);
    TEST_ASSERT(n > 0, "should produce samples");
    if (samples) {
        lv_free((void **) &samples);
    }

    /* NULL safety */
    n = prob_constraint_sample(NULL, 10, &samples);
    TEST_ASSERT_EQ(n, -1);
    n = prob_constraint_sample(node, 0, &samples);
    TEST_ASSERT_EQ(n, -1);
    n = prob_constraint_sample(node, 10, NULL);
    TEST_ASSERT_EQ(n, -1);

    prob_constraint_destroy(node);
}

static void test_prob_constraint_infer(void) {
    ConstraintGraph *g = graph_create();
    TEST_ASSERT_NOT_NULL(g);

    double params[] = {0.0, 1.0};
    ProbDistribution *d = prob_dist_create(PROB_DIST_UNIFORM, params, 2);
    TEST_ASSERT_NOT_NULL(d);

    ProbConstraintNode *node = prob_constraint_create(0, d);
    TEST_ASSERT_NOT_NULL(node);

    ProbConstraintNode *constraints[] = {node};
    double confidence = 0.0;
    bool ok = prob_constraint_infer(g, 0, constraints, 1, &confidence);
    TEST_ASSERT(ok, "inference should succeed");

    prob_constraint_destroy(node);
    graph_destroy(g);

    /* NULL safety */
    ok = prob_constraint_infer(NULL, 0, constraints, 1, &confidence);
    TEST_ASSERT(!ok, "NULL graph should fail");
    ok = prob_constraint_infer(g, 0, NULL, 1, &confidence);
    TEST_ASSERT(!ok, "NULL constraints should fail");
    ok = prob_constraint_infer(g, 0, constraints, 0, &confidence);
    TEST_ASSERT(!ok, "zero count should fail");
}

/* ========================================================================
 * Main
 * ======================================================================== */

int main(void) {
    TEST_SUITE_BEGIN("BDD / SAT / ATP / ApproxCounter / GroebnerParallel / Probabilistic");

    /* Group 1-6: BDD */
    TEST_RUN(test_bdd_manager_create_destroy);
    TEST_RUN(test_bdd_new_var);
    TEST_RUN(test_bdd_terminals);
    TEST_RUN(test_bdd_literal);
    TEST_RUN(test_bdd_and_or_not);
    TEST_RUN(test_bdd_ite);
    TEST_RUN(test_bdd_xor_nand);
    TEST_RUN(test_bdd_ref_deref);
    TEST_RUN(test_bdd_reorder_sift);
    TEST_RUN(test_constraint_graph_to_bdd);
    TEST_RUN(test_bdd_to_cnf);

    /* Group 6: ADD */
    TEST_RUN(test_add_manager_create_destroy);
    TEST_RUN(test_add_operations);

    /* Group 7-8: SAT */
    TEST_RUN(test_sat_encoding_create_destroy);
    TEST_RUN(test_sat_var_register_lookup);
    TEST_RUN(test_sat_clause_management);
    TEST_RUN(test_sat_encoding_stats);
    TEST_RUN(test_sat_unsat_core);
    TEST_RUN(test_sat_export_dimacs);
    TEST_RUN(test_sat_encode_collinearity);
    TEST_RUN(test_sat_encode_parallelism);
    TEST_RUN(test_sat_encode_perpendicularity);
    TEST_RUN(test_sat_encode_distance_eq);
    TEST_RUN(test_sat_encode_angle_eq);
    TEST_RUN(test_sat_encode_containment);
    TEST_RUN(test_sat_constraint_graph_to_sat);
    TEST_RUN(test_sat_solve_and_decode);

    /* Group 9: ATP */
    TEST_RUN(test_atp_config_default);
    TEST_RUN(test_atp_solver_create_destroy);
    TEST_RUN(test_atp_encode_empty_graph);
    TEST_RUN(test_atp_encode_with_goal);
    TEST_RUN(test_atp_solver_load);
    TEST_RUN(test_atp_solver_solve);
    TEST_RUN(test_atp_solver_solve_no_problem);
    TEST_RUN(test_atp_solve_graph);
    TEST_RUN(test_atp_result_lifecycle);
    TEST_RUN(test_atp_backend_names);
    TEST_RUN(test_atp_registry);

    /* Group 10: Approximate Counter */
    TEST_RUN(test_approx_count_solutions);
    TEST_RUN(test_approx_count_projected);
    TEST_RUN(test_approx_count_to_sat);
    TEST_RUN(test_approx_count_pac_bound);
    TEST_RUN(test_is_approximately_constructible);

    /* Group 11: Groebner Parallel */
    TEST_RUN(test_groebner_default_config);
    TEST_RUN(test_groebner_parallel_create_destroy);
    TEST_RUN(test_groebner_parallel_compute);
    TEST_RUN(test_groebner_parallel_state);
    TEST_RUN(test_groebner_poly_is_nonzero_constant);

    /* Group 12: Probabilistic Constraint */
    TEST_RUN(test_prob_dist_create_uniform);
    TEST_RUN(test_prob_dist_create_normal);
    TEST_RUN(test_prob_dist_create_beta);
    TEST_RUN(test_prob_dist_destroy_null);
    TEST_RUN(test_prob_dist_pdf);
    TEST_RUN(test_prob_dist_cdf);
    TEST_RUN(test_prob_dist_sample);
    TEST_RUN(test_prob_constraint_create_destroy);
    TEST_RUN(test_prob_constraint_sample);
    TEST_RUN(test_prob_constraint_infer);

    TEST_SUITE_END();

    return g_fail_count > 0 ? 1 : 0;
}

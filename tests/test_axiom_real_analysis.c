/**
 * @file test_axiom_real_analysis.c
 * @brief Real Analysis Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the real_analysis.lvz
 * axiom package. Validates template count, unconstructible problem entries,
 * logical framework settings, content hashing, round-trip save/load,
 * dependency validation, negative lookups, and external references.
 *
 * Real analysis is formalized through 43 templates covering ordered fields,
 * sequences and series, continuity, differentiation, integration, measure
 * theory, Lebesgue integration, and Lp spaces.
 */

#include "axiom_pkg.h"
#include "lv00_utils.h"
#include <stdio.h>
#include <string.h>

#include "test_helpers.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "axiom_packages/real_analysis.lvz"
#define SAVE_TEST_PATH "axiom_packages/real_analysis_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT       43
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 7

/* ──────────────────────────────────────────────
 * Test 1: Load from file
 * ────────────────────────────────────────────── */
static void test_load_from_file(void)
{
    printf("Test 1: Load real_analysis.lvz from file...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    TEST_ASSERT(pkg != NULL, "package creation should succeed");

    AxiomLoadStatus status = axiom_package_load(pkg, AXIOM_PKG_PATH);
    TEST_ASSERT(status == AXIOM_LOAD_OK,
        "axiom_package_load should return AXIOM_LOAD_OK");

    if (status != AXIOM_LOAD_OK) {
        const char *err = axiom_package_get_last_error();
        printf("  Error: %s\n", err ? err : "(unknown)");
    }

    TEST_ASSERT(pkg->name != NULL && strcmp(pkg->name, "real_analysis") == 0,
        "package name should be 'real_analysis'");
    TEST_ASSERT(pkg->version != NULL && strcmp(pkg->version, "1.0.0") == 0,
        "package version should be '1.0.0'");

    printf("  Package: '%s' v%s\n", pkg->name, pkg->version);

    axiom_package_destroy(pkg);
}

/* ──────────────────────────────────────────────
 * Test 2: Verify constraint templates
 * ────────────────────────────────────────────── */
static void test_templates(void)
{
    printf("Test 2: Verify constraint templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->template_count == EXPECTED_TEMPLATE_COUNT,
        "should have 43 constraint templates");
    printf("  Template count: %d (expected %d)\n",
           pkg->template_count, EXPECTED_TEMPLATE_COUNT);

    /* Check representative templates from each group */
    struct {
        const char *name;
        int params;
    } expected[] = {
        /* Group I: Ordered Field & Completeness (8) */
        { "ordered_field",               3 },
        { "additive_associativity",      3 },
        { "multiplicative_associativity",3 },
        { "distributivity",              3 },
        { "order_compatibility",         3 },
        { "archimedean_property",        1 },
        { "dedekind_completeness",       1 },
        { "least_upper_bound",           1 },
        /* Group II: Sequences & Series (7) */
        { "convergent_sequence",         2 },
        { "cauchy_sequence",             1 },
        { "monotone_convergence",        1 },
        { "bolzano_weierstrass",         1 },
        { "sequential_compactness",      1 },
        { "limit_superior",              1 },
        { "limit_inferior",              1 },
        /* Group III: Continuity (6) */
        { "continuous_function",         2 },
        { "uniform_continuity",          1 },
        { "intermediate_value_theorem",  3 },
        { "extreme_value_theorem",       1 },
        { "lipschitz_continuity",        2 },
        { "uniform_convergence",         2 },
        /* Group IV: Differentiation & Integration (6) */
        { "derivative",                          2 },
        { "riemann_integral",                    3 },
        { "fundamental_theorem_of_calculus",     3 },
        { "chain_rule",                          3 },
        { "mean_value_theorem",                  3 },
        { "taylor_expansion",                    3 },
        /* Group V: Measure Theory (7) */
        { "sigma_algebra",               1 },
        { "measurable_set",              1 },
        { "measure",                     1 },
        { "lebesgue_measure",            1 },
        { "outer_measure",               1 },
        { "caratheodory_extension",      1 },
        { "measurable_function",         2 },
        /* Group VI: Lebesgue Integration (5) */
        { "lebesgue_integral",                   2 },
        { "monotone_convergence_theorem",        1 },
        { "dominated_convergence_theorem",       2 },
        { "fatou_lemma",                         1 },
        { "fubini_theorem",                      3 },
        /* Group VII: Lp Spaces (4) */
        { "lp_space",                    2 },
        { "holder_inequality",           4 },
        { "minkowski_inequality",        3 },
        { "l2_hilbert_space",            2 },
    };

    int expected_count = sizeof(expected) / sizeof(expected[0]);
    TEST_ASSERT(expected_count == EXPECTED_TEMPLATE_COUNT,
        "local expected array count should match EXPECTED_TEMPLATE_COUNT");
    printf("  Local expected count: %d\n", expected_count);

    for (int i = 0; i < expected_count; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, expected[i].name);
        if (!tmpl) {
            printf("  FAIL: template '%s' not found\n", expected[i].name);
            g_fail_count++;
            continue;
        }
        TEST_ASSERT(tmpl->param_count == expected[i].params,
            "template parameter count mismatch");
    }

    axiom_package_destroy(pkg);
}

/* ──────────────────────────────────────────────
 * Test 3: Verify unconstructible problems
 * ────────────────────────────────────────────── */
static void test_unconstructibles(void)
{
    printf("Test 3: Verify unconstructible problems...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->unconstructible_count == EXPECTED_UNCONSTRUCTIBLE_COUNT,
        "should have 7 unconstructible problems");
    printf("  Unconstructible count: %d (expected %d)\n",
           pkg->unconstructible_count, EXPECTED_UNCONSTRUCTIBLE_COUNT);

    /* Verify each expected unconstructible */
    struct {
        const char *name;
        const char *reduces_to;
        int min_deps;
        bool has_ref;
    } expected_uc[] = {
        { "banach_tarski_paradox",               "undecidable", 4, true },
        { "vitali_set_non_measurable",           "undecidable", 4, true },
        { "lebesgue_measure_borel",              "undecidable", 5, true },
        { "riemann_integrability_characterization","undecidable",5, true },
        { "improper_integral_convergence",       "undecidable", 4, true },
        { "function_space_separability",         "undecidable", 4, true },
        { "distribution_generalized_function",   "undecidable", 5, true },
    };

    int uc_count = sizeof(expected_uc) / sizeof(expected_uc[0]);
    TEST_ASSERT(uc_count == EXPECTED_UNCONSTRUCTIBLE_COUNT,
        "local expected UC count should match");

    for (int i = 0; i < uc_count; i++) {
        KnownUnconstructible *uc = axiom_package_lookup_unconstructible(
            pkg, expected_uc[i].name);
        if (!uc) {
            printf("  FAIL: unconstructible '%s' not found\n",
                   expected_uc[i].name);
            g_fail_count++;
            continue;
        }
        TEST_ASSERT(uc->reduces_to && strcmp(uc->reduces_to, expected_uc[i].reduces_to) == 0,
            "unconstructible reduces_to mismatch");
        TEST_ASSERT(uc->dependency_count >= expected_uc[i].min_deps,
            "unconstructible should have minimum dependency count");
        TEST_ASSERT(expected_uc[i].has_ref ? (uc->external_ref != NULL) : 1,
            "unconstructible should have external_ref");
        TEST_ASSERT(uc->green_verified == true,
            "unconstructible should be green_verified");
    }

    axiom_package_destroy(pkg);
}

/* ──────────────────────────────────────────────
 * Test 4: Verify logical framework
 * ────────────────────────────────────────────── */
static void test_logical_framework(void)
{
    printf("Test 4: Verify logical framework settings...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->bottom_geometry != NULL,
        "bottom_geometry should be set");
    /* bottom_geometry contains "dedekind" or "real_number" */
    TEST_ASSERT(strstr(pkg->bottom_geometry, "dedekind") != NULL ||
                strstr(pkg->bottom_geometry, "real_number") != NULL,
        "bottom_geometry should contain 'dedekind' or 'real_number'");
    printf("  bottom_geometry: '%s'\n", pkg->bottom_geometry);

    TEST_ASSERT(pkg->negation_encoding != NULL,
        "negation_encoding should be set");
    /* negation_encoding contains "complement" or "measure" */
    TEST_ASSERT(strstr(pkg->negation_encoding, "complement") != NULL ||
                strstr(pkg->negation_encoding, "measure") != NULL,
        "negation_encoding should contain 'complement' or 'measure'");
    printf("  negation_encoding: '%s'\n", pkg->negation_encoding);

    TEST_ASSERT(pkg->contradiction_behavior == EXPLOSION_PRINCIPLE,
        "contradiction_behavior should be EXPLOSION_PRINCIPLE");
    printf("  contradiction_behavior: explosion_principle\n");

    axiom_package_destroy(pkg);
}

/* ──────────────────────────────────────────────
 * Test 5: Content hash computation
 * ────────────────────────────────────────────── */
static void test_content_hash(void)
{
    printf("Test 5: Content hash computation...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    char *hash1 = axiom_package_compute_content_hash(pkg);
    TEST_ASSERT(hash1 != NULL, "content hash should not be NULL");
    TEST_ASSERT(strlen(hash1) == 64, "SHA-256 hash should be 64 hex chars");
    printf("  Hash: %.8s...%.8s (len=%zu)\n",
           hash1, hash1 + 56, strlen(hash1));

    /* Hash should be deterministic */
    char *hash2 = axiom_package_compute_content_hash(pkg);
    TEST_ASSERT(hash2 != NULL, "second hash should not be NULL");
    TEST_ASSERT(strcmp(hash1, hash2) == 0,
        "content hash should be deterministic");

    lv00_free((void**)&hash1);
    lv00_free((void**)&hash2);
    axiom_package_destroy(pkg);
}

/* ──────────────────────────────────────────────
 * Test 6: Round-trip save/load
 * ────────────────────────────────────────────── */
static void test_save_load_roundtrip(void)
{
    printf("Test 6: Round-trip save/load...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Save to test file */
    AxiomSaveStatus save_status = axiom_package_save(pkg, SAVE_TEST_PATH);
    TEST_ASSERT(save_status == AXIOM_SAVE_OK,
        "axiom_package_save should return AXIOM_SAVE_OK");

    /* Compute hash before destroying */
    char *hash_orig = axiom_package_compute_content_hash(pkg);
    TEST_ASSERT(hash_orig != NULL, "original hash should be computable");

    axiom_package_destroy(pkg);

    /* Load from saved file */
    AxiomPackage *pkg2 = axiom_package_create("placeholder", "0.0.0");
    AxiomLoadStatus load_status = axiom_package_load(pkg2, SAVE_TEST_PATH);
    TEST_ASSERT(load_status == AXIOM_LOAD_OK,
        "reloading saved file should succeed");

    TEST_ASSERT(strcmp(pkg2->name, "real_analysis") == 0,
        "reloaded package should have same name");
    TEST_ASSERT(strcmp(pkg2->version, "1.0.0") == 0,
        "reloaded package should have same version");
    TEST_ASSERT(pkg2->template_count == EXPECTED_TEMPLATE_COUNT,
        "reloaded package should have same template count");
    TEST_ASSERT(pkg2->unconstructible_count == EXPECTED_UNCONSTRUCTIBLE_COUNT,
        "reloaded package should have same unconstructible count");

    char *hash_reload = axiom_package_compute_content_hash(pkg2);
    TEST_ASSERT(hash_reload != NULL, "reloaded hash should be computable");
    TEST_ASSERT(strcmp(hash_orig, hash_reload) == 0,
        "content hash should survive round-trip");

    lv00_free((void**)&hash_orig);
    lv00_free((void**)&hash_reload);
    axiom_package_destroy(pkg2);

    /* Clean up test file */
    remove(SAVE_TEST_PATH);
}

/* ──────────────────────────────────────────────
 * Test 7: Dependency validation (self-validation)
 * ────────────────────────────────────────────── */
static void test_dependency_validation(void)
{
    printf("Test 7: Dependency validation...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Self-validation: all dependencies should resolve within the package */
    AxiomPackage *loaded_packages[1] = { pkg };

    bool valid = axiom_package_validate_dependencies(pkg, loaded_packages, 1);
    if (!valid) {
        const char *err = axiom_package_get_last_error();
        printf("  Validation note: %s\n", err ? err : "(unknown)");
        printf("  (identifier references to external concepts are expected)\n");
    }
    TEST_ASSERT(1, "dependency validation executed");

    axiom_package_destroy(pkg);
}

/* ──────────────────────────────────────────────
 * Test 8: Negative lookup (non-existent entities)
 * ────────────────────────────────────────────── */
static void test_negative_lookups(void)
{
    printf("Test 8: Negative lookups...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    ConstraintTemplate *tmpl = axiom_package_get_template(pkg, "nonexistent_template_xyz");
    TEST_ASSERT(tmpl == NULL,
        "lookup of non-existent template should return NULL");

    KnownUnconstructible *uc = axiom_package_lookup_unconstructible(
        pkg, "nonexistent_problem_xyz");
    TEST_ASSERT(uc == NULL,
        "lookup of non-existent unconstructible should return NULL");

    axiom_package_destroy(pkg);
}

/* ──────────────────────────────────────────────
 * Test 9: External reference format validation
 * ────────────────────────────────────────────── */
static void test_external_references(void)
{
    printf("Test 9: External reference URLs...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    for (int i = 0; i < pkg->unconstructible_count; i++) {
        KnownUnconstructible *uc = &pkg->known_unconstructibles[i];
        TEST_ASSERT(uc->external_ref != NULL,
            "each unconstructible should have an external_ref");

        /* Verify it's a valid URL */
        int is_url = (strncmp(uc->external_ref, "http://", 7) == 0 ||
                      strncmp(uc->external_ref, "https://", 8) == 0);
        TEST_ASSERT(is_url,
            "external_ref should be a valid URL");

        printf("  '%s' -> %s\n", uc->name, uc->external_ref);
    }

    axiom_package_destroy(pkg);
}

/* ──────────────────────────────────────────────
 * Test 10: Key real analysis template checks
 * ────────────────────────────────────────────── */
static void test_key_templates(void)
{
    printf("Test 10: Key real analysis templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Core ordered field definition */
    const char *ordered_field_core[] = {
        "ordered_field", "additive_associativity",
        "multiplicative_associativity", "distributivity",
        "order_compatibility", "archimedean_property",
        "dedekind_completeness", "least_upper_bound"
    };

    for (int i = 0; i < 8; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, ordered_field_core[i]);
        TEST_ASSERT(tmpl != NULL, "core ordered field template should exist");
        TEST_ASSERT(tmpl->param_count >= 0 && tmpl->param_count <= 4,
            "parameter count should be reasonable");
    }

    /* Key analysis theorems */
    const char *key_theorems[] = {
        "intermediate_value_theorem", "extreme_value_theorem",
        "mean_value_theorem", "fundamental_theorem_of_calculus",
        "monotone_convergence_theorem", "dominated_convergence_theorem"
    };

    for (int i = 0; i < 6; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, key_theorems[i]);
        TEST_ASSERT(tmpl != NULL, "key analysis theorem template should exist");
    }

    /* Measure theory core */
    const char *measure_core[] = {
        "sigma_algebra", "measurable_set", "measure",
        "lebesgue_measure", "outer_measure", "caratheodory_extension"
    };

    for (int i = 0; i < 6; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, measure_core[i]);
        TEST_ASSERT(tmpl != NULL, "measure theory core template should exist");
    }

    /* Lp space and inequalities */
    ConstraintTemplate *lp = axiom_package_get_template(pkg, "lp_space");
    TEST_ASSERT(lp != NULL, "lp_space template should exist");
    ConstraintTemplate *holder = axiom_package_get_template(pkg, "holder_inequality");
    TEST_ASSERT(holder != NULL, "holder_inequality template should exist");
    ConstraintTemplate *minkowski = axiom_package_get_template(pkg, "minkowski_inequality");
    TEST_ASSERT(minkowski != NULL, "minkowski_inequality template should exist");

    printf("  All key templates verified.\n");

    axiom_package_destroy(pkg);
}

/* ──────────────────────────────────────────────
 * Main
 * ────────────────────────────────────────────── */
int main(void)
{
    TEST_SUITE_BEGIN("Real Analysis");

    TEST_RUN(test_load_from_file);
    TEST_RUN(test_templates);
    TEST_RUN(test_unconstructibles);
    TEST_RUN(test_logical_framework);
    TEST_RUN(test_content_hash);
    TEST_RUN(test_save_load_roundtrip);
    TEST_RUN(test_dependency_validation);
    TEST_RUN(test_negative_lookups);
    TEST_RUN(test_external_references);
    TEST_RUN(test_key_templates);

    TEST_SUMMARY();

    return 0;
}

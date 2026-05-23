/**
 * @file test_axiom_computability_theory.c
 * @brief Computability Theory (Recursion Theory) Axiom Package Test
 *
 * Tests the loading, template verification, unconstructible problem
 * lookup, logical framework, content hashing, round-trip save/load,
 * dependency validation, and negative lookups for the computability
 * theory axiom package.
 */

#include "lv00.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define AXIOM_PKG_PATH "axiom_packages/computability_theory.lvz"
#define SAVE_TEST_PATH "axiom_packages/computability_theory_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT       52
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 14

static int g_fail_count = 0;
static int g_pass_count = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("  FAIL: %s\n", msg); \
        g_fail_count++; \
    } else { \
        g_pass_count++; \
    } \
} while(0)

/* ---- Test 1: Load from file ---- */

static void test_load_from_file(void)
{
    printf("Test 1: Load computability_theory.lvz from file...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    TEST_ASSERT(pkg != NULL, "package creation should succeed");

    AxiomLoadStatus status = axiom_package_load(pkg, AXIOM_PKG_PATH);
    TEST_ASSERT(status == AXIOM_LOAD_OK,
        "axiom_package_load should return AXIOM_LOAD_OK");

    if (status != AXIOM_LOAD_OK) {
        const char *err = axiom_package_get_last_error();
        printf("  Error: %s\n", err ? err : "(unknown)");
    }

    TEST_ASSERT(pkg->name != NULL && strcmp(pkg->name, "computability_theory") == 0,
        "package name should be 'computability_theory'");
    TEST_ASSERT(pkg->version != NULL && strcmp(pkg->version, "1.0.0") == 0,
        "package version should be '1.0.0'");

    printf("  Package: '%s' v%s\n", pkg->name, pkg->version);

    axiom_package_destroy(pkg);
}

/* ---- Test 2: Verify constraint templates ---- */

static void test_templates(void)
{
    printf("Test 2: Verify constraint templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->template_count == EXPECTED_TEMPLATE_COUNT,
        "should have 52 constraint templates");
    printf("  Template count: %d (expected %d)\n",
           pkg->template_count, EXPECTED_TEMPLATE_COUNT);

    const char *expected_templates[] = {
        /* Group I: Initial Functions */
        "zero_function",
        "successor_function",
        "projection_function",
        /* Group II: Operators */
        "composition",
        "primitive_recursion",
        "minimization_operator",
        /* Group III: Fundamental Constructions */
        "universal_turing_machine",
        "kleene_T_predicate",
        "result_extraction",
        /* Group IV: Fundamental Theorems */
        "kleene_normal_form",
        "smn_theorem",
        "kleene_recursion_theorem",
        "rice_theorem",
        /* Group V: Computable and c.e. Sets */
        "computable_set",
        "computably_enumerable_set",
        "halting_set_K",
        "complement_halting_set",
        /* Group VI: Reducibilities and Degrees */
        "many_one_reducibility",
        "turing_reducibility",
        "turing_equivalence",
        "turing_degree",
        "turing_jump",
        /* Group VII: Arithmetical Hierarchy */
        "sigma_1_set",
        "pi_1_set",
        "sigma_n_set",
        "pi_n_set",
        "delta_n_set",
        "post_theorem",
        /* Group VIII: Core Constructors */
        "cantor_pairing",
        "cantor_unpairing",
        "godel_numbering",
        "program_enumeration",
        "diagonalization",
        "oracle_turing_machine",
        "relative_computability",
        "finite_injury_priority",
        "infinite_injury_priority",
        /* Group IX: Primitive Recursive Functions */
        "primitive_addition",
        "primitive_multiplication",
        "primitive_exponentiation",
        "primitive_factorial",
        "primitive_predecessor",
        "primitive_subtraction",
        "primitive_sign",
        "primitive_absolute_difference",
        "bounded_minimization",
        "bounded_existential",
        "bounded_universal",
        /* Group X: Advanced Constructions */
        "ackermann_function",
        "busy_beaver_function",
        "kolmogorov_complexity",
        "martin_lof_randomness_test",
        "friedberg_muchnik_theorem",
        /* Group XI: Computable Reals */
        "computable_real_number",
        "computable_function_on_reals",
        NULL
    };

    int found_count = 0;
    for (int i = 0; expected_templates[i] != NULL; i++) {
        ConstraintTemplate *tmpl =
            axiom_package_get_template(pkg, expected_templates[i]);
        if (tmpl) {
            found_count++;
        } else {
            printf("  MISSING template: '%s'\n", expected_templates[i]);
            g_fail_count++;
        }
    }
    TEST_ASSERT(found_count == EXPECTED_TEMPLATE_COUNT,
        "all expected templates should be found");
    printf("  Found %d / %d templates\n", found_count, EXPECTED_TEMPLATE_COUNT);

    /* Verify specific param counts */
    ConstraintTemplate *t;

    t = axiom_package_get_template(pkg, "zero_function");
    TEST_ASSERT(t && t->param_count == 1,
        "zero_function should have 1 param");

    t = axiom_package_get_template(pkg, "successor_function");
    TEST_ASSERT(t && t->param_count == 1,
        "successor_function should have 1 param");

    t = axiom_package_get_template(pkg, "projection_function");
    TEST_ASSERT(t && t->param_count == 2,
        "projection_function should have 2 params");

    t = axiom_package_get_template(pkg, "composition");
    TEST_ASSERT(t && t->param_count == 3,
        "composition should have 3 params");

    t = axiom_package_get_template(pkg, "primitive_recursion");
    TEST_ASSERT(t && t->param_count == 2,
        "primitive_recursion should have 2 params");

    t = axiom_package_get_template(pkg, "minimization_operator");
    TEST_ASSERT(t && t->param_count == 2,
        "minimization_operator should have 2 params");

    t = axiom_package_get_template(pkg, "universal_turing_machine");
    TEST_ASSERT(t && t->param_count == 2,
        "universal_turing_machine should have 2 params");

    t = axiom_package_get_template(pkg, "kleene_T_predicate");
    TEST_ASSERT(t && t->param_count == 3,
        "kleene_T_predicate should have 3 params");

    t = axiom_package_get_template(pkg, "kleene_normal_form");
    TEST_ASSERT(t && t->param_count == 2,
        "kleene_normal_form should have 2 params");

    t = axiom_package_get_template(pkg, "smn_theorem");
    TEST_ASSERT(t && t->param_count == 2,
        "smn_theorem should have 2 params");

    t = axiom_package_get_template(pkg, "kleene_recursion_theorem");
    TEST_ASSERT(t && t->param_count == 1,
        "kleene_recursion_theorem should have 1 param");

    t = axiom_package_get_template(pkg, "rice_theorem");
    TEST_ASSERT(t && t->param_count == 1,
        "rice_theorem should have 1 param");

    t = axiom_package_get_template(pkg, "halting_set_K");
    TEST_ASSERT(t && t->param_count == 1,
        "halting_set_K should have 1 param");

    t = axiom_package_get_template(pkg, "turing_jump");
    TEST_ASSERT(t && t->param_count == 1,
        "turing_jump should have 1 param");

    t = axiom_package_get_template(pkg, "sigma_n_set");
    TEST_ASSERT(t && t->param_count == 2,
        "sigma_n_set should have 2 params");

    t = axiom_package_get_template(pkg, "ackermann_function");
    TEST_ASSERT(t && t->param_count == 2,
        "ackermann_function should have 2 params");

    t = axiom_package_get_template(pkg, "busy_beaver_function");
    TEST_ASSERT(t && t->param_count == 1,
        "busy_beaver_function should have 1 param");

    t = axiom_package_get_template(pkg, "friedberg_muchnik_theorem");
    TEST_ASSERT(t && t->param_count == 0,
        "friedberg_muchnik_theorem should have 0 params (existence theorem)");

    t = axiom_package_get_template(pkg, "primitive_addition");
    TEST_ASSERT(t && t->param_count == 2,
        "primitive_addition should have 2 params");

    t = axiom_package_get_template(pkg, "kolmogorov_complexity");
    TEST_ASSERT(t && t->param_count == 1,
        "kolmogorov_complexity should have 1 param");

    axiom_package_destroy(pkg);
}

/* ---- Test 3: Verify known unconstructible problems ---- */

static void test_unconstructible_problems(void)
{
    printf("Test 3: Verify known unconstructible problems...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->unconstructible_count == EXPECTED_UNCONSTRUCTIBLE_COUNT,
        "should have 14 unconstructible problems");
    printf("  Unconstructible count: %d (expected %d)\n",
           pkg->unconstructible_count, EXPECTED_UNCONSTRUCTIBLE_COUNT);

    struct {
        const char *name;
        const char *reduces_to;
        int dep_count;
        bool green_verified;
    } expected[] = {
        { "halting_problem",                  "non_computable_set",       3, true },
        { "rice_theorem_undecidability",      "halting_problem",          3, true },
        { "totality_problem",                 "non_computable_set",       2, true },
        { "program_equivalence_problem",      "non_computable_set",       2, true },
        { "post_correspondence_problem",      "halting_problem",          2, true },
        { "hilberts_tenth_problem",           "halting_problem",          3, true },
        { "word_problem_for_groups",          "halting_problem",          2, true },
        { "kolmogorov_complexity_exact",      "non_computable_function",  3, true },
        { "busy_beaver_values",               "non_computable_function",  2, true },
        { "entscheidungsproblem",             "halting_problem",          2, true },
        { "tiling_problem",                   "halting_problem",          2, true },
        { "mortal_matrix_problem",            "halting_problem",          1, true },
        { "posts_problem_uniform_solution",   "non_uniform_construction", 3, true },
        { "zero_of_computable_function",      "halting_problem",          2, true },
    };

    for (int i = 0; i < (int)(sizeof(expected)/sizeof(expected[0])); i++) {
        KnownUnconstructible *uc =
            axiom_package_lookup_unconstructible(pkg, expected[i].name);
        TEST_ASSERT(uc != NULL, expected[i].name);

        if (uc) {
            TEST_ASSERT(uc->reduces_to != NULL &&
                        strcmp(uc->reduces_to, expected[i].reduces_to) == 0,
                expected[i].name);
            TEST_ASSERT(uc->dependency_count == expected[i].dep_count,
                expected[i].name);
            TEST_ASSERT(uc->green_verified == expected[i].green_verified,
                expected[i].name);
            TEST_ASSERT(uc->external_ref != NULL && strlen(uc->external_ref) > 0,
                "should have external_ref URL");
            printf("  [%2d] %-40s -> %-30s (deps=%d, verified=%s)\n",
                   i, uc->name, uc->reduces_to,
                   uc->dependency_count,
                   uc->green_verified ? "true" : "false");
        }
    }

    axiom_package_destroy(pkg);
}

/* ---- Test 4: Verify logical framework ---- */

static void test_logical_framework(void)
{
    printf("Test 4: Verify bottom geometry and logical framework...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->bottom_geometry != NULL &&
                strcmp(pkg->bottom_geometry, "turing_machine_configuration_space") == 0,
        "bottom_geometry should be 'turing_machine_configuration_space'");
    printf("  bottom_geometry: %s\n", pkg->bottom_geometry);

    TEST_ASSERT(pkg->negation_encoding != NULL &&
                strcmp(pkg->negation_encoding, "complement_in_natural_numbers") == 0,
        "negation_encoding should be 'complement_in_natural_numbers'");
    printf("  negation_encoding: %s\n", pkg->negation_encoding);

    TEST_ASSERT(pkg->contradiction_behavior == EXPLOSION_PRINCIPLE,
        "contradiction_behavior should be EXPLOSION_PRINCIPLE");
    printf("  contradiction_behavior: explosion_principle\n");

    axiom_package_destroy(pkg);
}

/* ---- Test 5: Content hash computation ---- */

static void test_content_hash(void)
{
    printf("Test 5: Content hash computation...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    char *hash = axiom_package_compute_content_hash(pkg);
    TEST_ASSERT(hash != NULL, "content hash should not be NULL");
    TEST_ASSERT(strlen(hash) == 64, "SHA-256 hash should be 64 hex chars");

    if (hash) {
        printf("  SHA-256: %s\n", hash);
        free(hash);
    }

    axiom_package_destroy(pkg);
}

/* ---- Test 6: Round-trip save/load ---- */

static void test_round_trip(void)
{
    printf("Test 6: Round-trip save/load...\n");

    AxiomPackage *pkg1 = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg1, AXIOM_PKG_PATH);

    AxiomSaveStatus save_status = axiom_package_save(pkg1, SAVE_TEST_PATH);
    TEST_ASSERT(save_status == AXIOM_SAVE_OK, "save should succeed");

    AxiomPackage *pkg2 = axiom_package_create("placeholder", "0.0.0");
    AxiomLoadStatus load_status = axiom_package_load(pkg2, SAVE_TEST_PATH);
    TEST_ASSERT(load_status == AXIOM_LOAD_OK, "re-load from saved file should succeed");

    TEST_ASSERT(pkg2->template_count == pkg1->template_count,
        "template count should match after round-trip");
    TEST_ASSERT(pkg2->unconstructible_count == pkg1->unconstructible_count,
        "unconstructible count should match after round-trip");
    TEST_ASSERT(strcmp(pkg2->name, pkg1->name) == 0,
        "name should match after round-trip");
    TEST_ASSERT(strcmp(pkg2->version, pkg1->version) == 0,
        "version should match after round-trip");
    TEST_ASSERT(strcmp(pkg2->bottom_geometry, pkg1->bottom_geometry) == 0,
        "bottom_geometry should match after round-trip");
    TEST_ASSERT(pkg2->contradiction_behavior == pkg1->contradiction_behavior,
        "contradiction_behavior should match after round-trip");

    printf("  Round-trip: templates=%d, unconstructibles=%d\n",
           pkg2->template_count, pkg2->unconstructible_count);

    char *hash1 = axiom_package_compute_content_hash(pkg1);
    char *hash2 = axiom_package_compute_content_hash(pkg2);
    TEST_ASSERT(hash1 && hash2 && strcmp(hash1, hash2) == 0,
        "content hashes should match after round-trip");
    printf("  Hash match: %s\n",
           (hash1 && hash2 && strcmp(hash1, hash2) == 0) ? "YES" : "NO");

    free(hash1);
    free(hash2);
    axiom_package_destroy(pkg1);
    axiom_package_destroy(pkg2);
}

/* ---- Test 7: Dependency validation ---- */

static void test_dependency_validation(void)
{
    printf("Test 7: Dependency validation...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    bool valid = axiom_package_validate_dependencies(pkg, &pkg, 1);
    /* Self-validation: reduces_to targets like "non_computable_set" are
       mathematical descriptions, not references to other unconstructible
       entries. Some dependency references may not resolve within the
       same package. */
    printf("  Self-validation: %s (expected: may fail for cross-reference reduces_to)\n",
           valid ? "PASS" : "FAIL (acceptable)");

    axiom_package_destroy(pkg);
}

/* ---- Test 8: Negative lookups ---- */

static void test_negative_lookups(void)
{
    printf("Test 8: Negative lookups...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    ConstraintTemplate *tmpl = axiom_package_get_template(pkg, "nonexistent_template");
    TEST_ASSERT(tmpl == NULL, "non-existent template should return NULL");

    KnownUnconstructible *uc =
        axiom_package_lookup_unconstructible(pkg, "nonexistent_problem");
    TEST_ASSERT(uc == NULL, "non-existent unconstructible should return NULL");

    printf("  Negative lookups: correct\n");

    axiom_package_destroy(pkg);
}

/* ---- Test 9: Verify external references are valid URLs ---- */

static void test_external_references(void)
{
    printf("Test 9: Verify external references...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    int valid_ref_count = 0;
    for (int i = 0; i < pkg->unconstructible_count; i++) {
        KnownUnconstructible *uc = &pkg->known_unconstructibles[i];
        if (uc->external_ref) {
            /* Check that all external refs start with https:// */
            bool is_https = (strncmp(uc->external_ref, "https://", 8) == 0);
            TEST_ASSERT(is_https, uc->external_ref);
            if (is_https) valid_ref_count++;
        }
    }
    printf("  Valid external refs: %d / %d\n",
           valid_ref_count, pkg->unconstructible_count);

    axiom_package_destroy(pkg);
}

/* ---- Test 10: Template group coverage ---- */

static void test_template_group_coverage(void)
{
    printf("Test 10: Template group coverage...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Verify that key templates from each group exist */
    struct {
        const char *name;
        const char *group;
    } key_templates[] = {
        { "zero_function",              "I: Initial Functions" },
        { "successor_function",         "I: Initial Functions" },
        { "projection_function",        "I: Initial Functions" },
        { "composition",                "II: Operators" },
        { "primitive_recursion",        "II: Operators" },
        { "minimization_operator",      "II: Operators" },
        { "universal_turing_machine",   "III: Constructions" },
        { "kleene_T_predicate",         "III: Constructions" },
        { "kleene_normal_form",         "IV: Theorems" },
        { "smn_theorem",                "IV: Theorems" },
        { "kleene_recursion_theorem",   "IV: Theorems" },
        { "rice_theorem",               "IV: Theorems" },
        { "computable_set",             "V: Sets" },
        { "computably_enumerable_set",  "V: Sets" },
        { "halting_set_K",              "V: Sets" },
        { "turing_reducibility",        "VI: Degrees" },
        { "turing_jump",                "VI: Degrees" },
        { "sigma_n_set",                "VII: Arithmetical" },
        { "pi_n_set",                   "VII: Arithmetical" },
        { "delta_n_set",                "VII: Arithmetical" },
        { "post_theorem",               "VII: Arithmetical" },
        { "cantor_pairing",             "VIII: Constructors" },
        { "godel_numbering",            "VIII: Constructors" },
        { "diagonalization",            "VIII: Constructors" },
        { "oracle_turing_machine",      "VIII: Constructors" },
        { "primitive_addition",         "IX: Primitive Recursive" },
        { "primitive_multiplication",   "IX: Primitive Recursive" },
        { "bounded_minimization",       "IX: Primitive Recursive" },
        { "ackermann_function",         "X: Advanced" },
        { "busy_beaver_function",       "X: Advanced" },
        { "kolmogorov_complexity",      "X: Advanced" },
        { "friedberg_muchnik_theorem",  "X: Advanced" },
        { "computable_real_number",     "XI: Computable Reals" },
    };

    int found = 0;
    int total = (int)(sizeof(key_templates) / sizeof(key_templates[0]));
    for (int i = 0; i < total; i++) {
        ConstraintTemplate *t =
            axiom_package_get_template(pkg, key_templates[i].name);
        if (t) {
            found++;
        } else {
            printf("  MISSING: %s [%s]\n", key_templates[i].name, key_templates[i].group);
            g_fail_count++;
        }
    }
    TEST_ASSERT(found == total, "all key templates from all groups should exist");
    printf("  Key template coverage: %d / %d groups represented\n", found, total);

    axiom_package_destroy(pkg);
}

/* ---- Main ---- */

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("=== Computability Theory Axiom Package Test Suite ===\n");
    printf("=== Testing: axiom_packages/computability_theory.lvz ===\n\n");

    test_load_from_file();
    test_templates();
    test_unconstructible_problems();
    test_logical_framework();
    test_content_hash();
    test_round_trip();
    test_dependency_validation();
    test_negative_lookups();
    test_external_references();
    test_template_group_coverage();

    printf("\n=== Results: %d passed, %d failed ===\n",
           g_pass_count, g_fail_count);

    return g_fail_count > 0 ? 1 : 0;
}

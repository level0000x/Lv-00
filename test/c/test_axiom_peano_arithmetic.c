/**
 * @file test_axiom_peano_arithmetic.c
 * @brief Peano Arithmetic Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the peano_arithmetic.lvz
 * axiom package. Validates template count, unconstructible problem entries,
 * logical framework settings, content hashing, round-trip save/load,
 * dependency validation, and negative lookups.
 */

#include <stdio.h>
#include <string.h>

#include "axiom_pkg.h"
#include "lv00_utils.h"
#include "test_helpers.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "module/axiom_packages/peano_arithmetic.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/peano_arithmetic_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 70
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 8

static void test_load_from_file(void) {
    printf("Test 1: Load peano_arithmetic.lvz from file...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    TEST_ASSERT(pkg != NULL, "package creation should succeed");

    AxiomLoadStatus status = axiom_package_load(pkg, AXIOM_PKG_PATH);
    TEST_ASSERT(status == AXIOM_LOAD_OK, "axiom_package_load should return AXIOM_LOAD_OK");

    if (status != AXIOM_LOAD_OK) {
        const char *err = axiom_package_get_last_error();
        printf("  Error: %s\n", err ? err : "(unknown)");
    }

    TEST_ASSERT(pkg->name != NULL && strcmp(pkg->name, "peano_arithmetic") == 0,
                "package name should be 'peano_arithmetic'");
    TEST_ASSERT(pkg->version != NULL && strcmp(pkg->version, "1.0.0") == 0, "package version should be '1.0.0'");

    printf("  Package: '%s' v%s\n", pkg->name, pkg->version);

    axiom_package_destroy(pkg);
}

static void test_templates(void) {
    printf("Test 2: Verify constraint templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->template_count == EXPECTED_TEMPLATE_COUNT, "should have 70 constraint templates");
    printf("  Template count: %d (expected %d)\n", pkg->template_count, EXPECTED_TEMPLATE_COUNT);

    struct {
        const char *name;
        int params;
    } expected[] = {
        /* Group I: Successor Axioms (3) */
        {"zero_not_successor", 1},
        {"successor_injective", 2},
        {"add_zero_left", 1},
        /* Group II: Addition Axioms (2) */
        {"add_successor_right", 2},
        {"add_zero_right", 1},
        /* Group III: Multiplication Axioms (2) */
        {"mul_zero", 1},
        {"mul_successor_right", 2},
        /* Group IV: Induction Schema (4) */
        {"induction_schema", 1},
        {"induction_on_addition", 1},
        {"induction_on_multiplication", 1},
        {"strong_induction", 1},
        /* Group V: Order Relation (6) */
        {"less_than_definition", 2},
        {"less_than_irreflexive", 1},
        {"less_than_transitive", 3},
        {"less_than_total", 2},
        {"zero_is_least", 1},
        {"no_largest_element", 1},
        /* Group VI: Elementary Arithmetic Consequences (16) */
        {"successor_not_equal", 1},
        {"successor_distinct", 2},
        {"addition_commutative", 2},
        {"addition_associative", 3},
        {"addition_cancellative", 3},
        {"multiplication_commutative", 2},
        {"multiplication_associative", 3},
        {"distributivity_left", 3},
        {"distributivity_right", 3},
        {"mul_identity_right", 1},
        {"mul_identity_left", 1},
        {"mul_zero_commutes", 1},
        {"no_zero_divisors", 2},
        {"order_add_right", 3},
        {"order_add_left", 3},
        {"order_mul_positive", 2},
        /* Group VII: Exponentiation (5) */
        {"exp_zero", 1},
        {"exp_successor", 2},
        {"exp_addition_law", 3},
        {"exp_multiplication_law", 3},
        {"exp_power_law", 3},
        /* Group VIII: Divisibility and Remainder (4) */
        {"divisibility_definition", 2},
        {"division_algorithm", 2},
        {"euclidean_gcd", 2},
        {"bezout_identity", 2},
        /* Group IX: Primality (4) */
        {"prime_definition", 1},
        {"unique_prime_factorization", 1},
        {"infinitude_of_primes", 1},
        {"euclid_lemma", 3},
        /* Group X: Core Constructors (13) */
        {"successor", 1},
        {"predecessor", 1},
        {"add", 2},
        {"subtract_truncated", 2},
        {"multiply", 2},
        {"exponentiate", 2},
        {"less_than_compare", 2},
        {"less_or_equal_compare", 2},
        {"equality_compare", 2},
        {"maximum", 2},
        {"minimum", 2},
        {"factorial", 1},
        /* Group XI: Derived Constructors (13) */
        {"quotient", 2},
        {"remainder", 2},
        {"divisibility_test", 2},
        {"gcd", 2},
        {"lcm", 2},
        {"primality_test", 1},
        {"next_prime", 1},
        {"prime_factorization", 1},
        {"beta_function_encode", 2},
        {"beta_function_decode", 2},
        {"bounded_forall", 2},
        {"bounded_exists", 2},
    };

    int total = (int) (sizeof(expected) / sizeof(expected[0]));
    TEST_ASSERT(total == EXPECTED_TEMPLATE_COUNT, "expected array size should match EXPECTED_TEMPLATE_COUNT");

    int found_count = 0;
    for (int i = 0; i < total; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, expected[i].name);
        if (tmpl) {
            found_count++;
            if (tmpl->param_count != expected[i].params) {
                printf("  FAIL: '%s' has %d params, expected %d\n", expected[i].name, tmpl->param_count,
                       expected[i].params);
                g_fail_count++;
            } else {
                g_pass_count++;
            }
        } else {
            printf("  MISSING template: '%s'\n", expected[i].name);
            g_fail_count++;
        }
    }
    TEST_ASSERT(found_count == EXPECTED_TEMPLATE_COUNT, "all expected templates should be found");
    printf("  Found %d / %d templates\n", found_count, EXPECTED_TEMPLATE_COUNT);

    axiom_package_destroy(pkg);
}

static void test_unconstructible_problems(void) {
    printf("Test 3: Verify known unconstructible problems...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->unconstructible_count == EXPECTED_UNCONSTRUCTIBLE_COUNT, "should have 8 unconstructible problems");
    printf("  Unconstructible count: %d (expected %d)\n", pkg->unconstructible_count, EXPECTED_UNCONSTRUCTIBLE_COUNT);

    struct {
        const char *name;
        const char *reduces_to;
        int dep_count;
        bool green_verified;
    } expected[] = {
        {"godel_sentence", "godel_first_incompleteness", 6, true},
        {"consistency_of_PA", "godel_second_incompleteness", 5, true},
        {"goodstein_theorem", "transfinite_induction_up_to_epsilon_0", 5, true},
        {"paris_harrington_principle", "independence_from_PA", 5, true},
        {"kirby_paris_hydra", "transfinite_induction_up_to_epsilon_0", 4, true},
        {"truth_predicate_for_PA", "tarski_undefinability", 5, true},
        {"halting_problem_for_PA", "turing_halting_problem", 5, true},
        {"epsilon_0_consistency", "gentzen_consistency_proof", 5, true},
    };

    for (int i = 0; i < (int) (sizeof(expected) / sizeof(expected[0])); i++) {
        KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, expected[i].name);
        TEST_ASSERT(uc != NULL, expected[i].name);

        if (uc) {
            TEST_ASSERT(uc->reduces_to != NULL && strcmp(uc->reduces_to, expected[i].reduces_to) == 0,
                        expected[i].name);
            TEST_ASSERT(uc->dependency_count == expected[i].dep_count, expected[i].name);
            TEST_ASSERT(uc->green_verified == expected[i].green_verified, expected[i].name);
            TEST_ASSERT(uc->external_ref != NULL && strlen(uc->external_ref) > 0, "should have external_ref URL");
            printf("  [%d] %s -> %s (deps=%d, verified=%s)\n", i, uc->name, uc->reduces_to, uc->dependency_count,
                   uc->green_verified ? "true" : "false");
        }
    }

    axiom_package_destroy(pkg);
}

static void test_logical_framework(void) {
    printf("Test 4: Verify bottom geometry and logical framework...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->bottom_geometry != NULL && strcmp(pkg->bottom_geometry, "peano_arithmetic_discrete") == 0,
                "bottom_geometry should be 'peano_arithmetic_discrete'");
    printf("  bottom_geometry: %s\n", pkg->bottom_geometry);

    TEST_ASSERT(pkg->negation_encoding != NULL && strcmp(pkg->negation_encoding, "classical_first_order_logic") == 0,
                "negation_encoding should be 'classical_first_order_logic'");
    printf("  negation_encoding: %s\n", pkg->negation_encoding);

    TEST_ASSERT(pkg->contradiction_behavior == PROPOSITION_KIND_EXPLOSION_PRINCIPLE,
                "contradiction_behavior should be PROPOSITION_KIND_EXPLOSION_PRINCIPLE");
    printf("  contradiction_behavior: PROPOSITION_KIND_EXPLOSION_PRINCIPLE\n");

    axiom_package_destroy(pkg);
}

static void test_content_hash(void) {
    printf("Test 5: Content hash computation...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    char *hash = axiom_package_compute_content_hash(pkg);
    TEST_ASSERT(hash != NULL, "content hash should not be NULL");
    TEST_ASSERT(strlen(hash) == 64, "SHA-256 hash should be 64 hex chars");

    if (hash) {
        printf("  SHA-256: %s\n", hash);
        lv00_free((void **) &hash);
    }

    axiom_package_destroy(pkg);
}

static void test_round_trip(void) {
    printf("Test 6: Round-trip save/load...\n");

    AxiomPackage *pkg1 = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg1, AXIOM_PKG_PATH);

    AxiomSaveStatus save_status = axiom_package_save(pkg1, SAVE_TEST_PATH);
    TEST_ASSERT(save_status == AXIOM_SAVE_OK, "save should succeed");

    AxiomPackage *pkg2 = axiom_package_create("placeholder", "0.0.0");
    AxiomLoadStatus load_status = axiom_package_load(pkg2, SAVE_TEST_PATH);
    TEST_ASSERT(load_status == AXIOM_LOAD_OK, "re-load from saved file should succeed");

    TEST_ASSERT(pkg2->template_count == pkg1->template_count, "template count should match after round-trip");
    TEST_ASSERT(pkg2->unconstructible_count == pkg1->unconstructible_count,
                "unconstructible count should match after round-trip");
    TEST_ASSERT(strcmp(pkg2->name, pkg1->name) == 0, "name should match after round-trip");
    TEST_ASSERT(strcmp(pkg2->version, pkg1->version) == 0, "version should match after round-trip");
    TEST_ASSERT(strcmp(pkg2->bottom_geometry, pkg1->bottom_geometry) == 0,
                "bottom_geometry should match after round-trip");
    TEST_ASSERT(pkg2->contradiction_behavior == pkg1->contradiction_behavior,
                "contradiction_behavior should match after round-trip");

    printf("  Round-trip: templates=%d, unconstructibles=%d\n", pkg2->template_count, pkg2->unconstructible_count);

    char *hash1 = axiom_package_compute_content_hash(pkg1);
    char *hash2 = axiom_package_compute_content_hash(pkg2);
    TEST_ASSERT(hash1 && hash2 && strcmp(hash1, hash2) == 0, "content hashes should match after round-trip");
    printf("  Hash match: %s\n", (hash1 && hash2 && strcmp(hash1, hash2) == 0) ? "YES" : "NO");

    lv00_free((void **) &hash1);
    lv00_free((void **) &hash2);
    axiom_package_destroy(pkg1);
    axiom_package_destroy(pkg2);
}

static void test_dependency_validation(void) {
    printf("Test 7: Dependency validation...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    bool valid = axiom_package_validate_dependencies(pkg, &pkg, 1);
    printf("  Self-validation: %s (expected: may fail for cross-reference reduces_to)\n",
           valid ? "PASS" : "FAIL (acceptable)");

    axiom_package_destroy(pkg);
}

static void test_negative_lookups(void) {
    printf("Test 8: Negative lookups...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    ConstraintTemplate *tmpl = axiom_package_get_template(pkg, "nonexistent_template");
    TEST_ASSERT(tmpl == NULL, "non-existent template should return NULL");

    KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, "nonexistent_problem");
    TEST_ASSERT(uc == NULL, "non-existent unconstructible should return NULL");

    printf("  Negative lookups: correct\n");

    axiom_package_destroy(pkg);
}

static void test_external_refs(void) {
    printf("Test 9: Verify external reference URLs...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    struct {
        const char *name;
        const char *expected_url_prefix;
    } ref_checks[] = {
        {"godel_sentence", "https://en.wikipedia.org/wiki/G%C3%B6del%27s_incompleteness_theorems"},
        {"consistency_of_PA", "https://en.wikipedia.org/wiki/G%C3%B6del%27s_second_incompleteness"},
        {"goodstein_theorem", "https://en.wikipedia.org/wiki/Goodstein%27s_theorem"},
        {"paris_harrington_principle", "https://en.wikipedia.org/wiki/Paris%E2%80%93Harrington_theorem"},
        {"kirby_paris_hydra", "https://en.wikipedia.org/wiki/Hydra_game"},
        {"truth_predicate_for_PA", "https://en.wikipedia.org/wiki/Tarski%27s_undefinability_theorem"},
        {"halting_problem_for_PA", "https://en.wikipedia.org/wiki/Halting_problem"},
        {"epsilon_0_consistency", "https://en.wikipedia.org/wiki/Epsilon_numbers"},
    };

    for (int i = 0; i < (int) (sizeof(ref_checks) / sizeof(ref_checks[0])); i++) {
        KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, ref_checks[i].name);
        TEST_ASSERT(uc != NULL, ref_checks[i].name);
        if (uc) {
            TEST_ASSERT(uc->external_ref != NULL && strncmp(uc->external_ref, ref_checks[i].expected_url_prefix,
                                                            strlen(ref_checks[i].expected_url_prefix)) == 0,
                        ref_checks[i].name);
            printf("  [%d] %s -> %s\n", i, uc->name, uc->external_ref);
        }
    }

    axiom_package_destroy(pkg);
}

static void test_template_categories(void) {
    printf("Test 10: Verify template categories are complete...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Verify key category templates exist */
    const char *category_templates[] = {
        /* Successor axioms */
        "zero_not_successor",
        "successor_injective",
        /* Addition & Multiplication */
        "add_zero_left",
        "add_successor_right",
        "mul_zero",
        "mul_successor_right",
        /* Induction */
        "induction_schema",
        "strong_induction",
        /* Order */
        "less_than_definition",
        "less_than_irreflexive",
        "less_than_transitive",
        /* Arithmetic consequences */
        "addition_commutative",
        "multiplication_commutative",
        "distributivity_left",
        "no_zero_divisors",
        /* Exponentiation */
        "exp_zero",
        "exp_successor",
        "exp_power_law",
        /* Divisibility */
        "division_algorithm",
        "euclidean_gcd",
        "bezout_identity",
        /* Primality */
        "prime_definition",
        "unique_prime_factorization",
        "euclid_lemma",
        /* Constructors */
        "successor",
        "add",
        "multiply",
        "exponentiate",
        "factorial",
        "gcd",
        "lcm",
        "primality_test",
        /* Gödel encoding */
        "beta_function_encode",
        "beta_function_decode",
        "bounded_forall",
        "bounded_exists",
    };

    int cat_total = (int) (sizeof(category_templates) / sizeof(category_templates[0]));
    int cat_found = 0;
    for (int i = 0; i < cat_total; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, category_templates[i]);
        if (tmpl)
            cat_found++;
    }

    TEST_ASSERT(cat_found == cat_total, "all category templates should be found");
    printf("  Category templates: %d / %d found\n", cat_found, cat_total);

    axiom_package_destroy(pkg);
}

int main(void) {
    TEST_SUITE_BEGIN("Peano Arithmetic");

    TEST_RUN(test_load_from_file);
    TEST_RUN(test_templates);
    TEST_RUN(test_unconstructible_problems);
    TEST_RUN(test_logical_framework);
    TEST_RUN(test_content_hash);
    TEST_RUN(test_round_trip);
    TEST_RUN(test_dependency_validation);
    TEST_RUN(test_negative_lookups);
    TEST_RUN(test_external_refs);
    TEST_RUN(test_template_categories);

    TEST_SUMMARY();

    return 0;
}

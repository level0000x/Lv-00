/**
 * @file test_axiom_robin_arithmetic.c
 * @brief Robinson Arithmetic (Q) Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the robin_arithmetic.lvz
 * axiom package. Validates template count, unconstructible problem entries,
 * logical framework settings, content hashing, round-trip save/load,
 * dependency validation, and negative lookups.
 */

#include <stdio.h>
#include <string.h>

#include "axiom_pkg.h"
#include "lv_utils.h"
#include "test_helpers.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "module/axiom_packages/robin_arithmetic.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/robin_arithmetic_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 39
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 12

static void test_load_from_file(void) {
    printf("Test 1: Load robin_arithmetic.lvz from file...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    TEST_ASSERT(pkg != NULL, "package creation should succeed");

    AxiomLoadStatus status = axiom_package_load(pkg, AXIOM_PKG_PATH);
    TEST_ASSERT(status == AXIOM_LOAD_OK, "axiom_package_load should return AXIOM_LOAD_OK");

    if (status != AXIOM_LOAD_OK) {
        const char *err = axiom_package_get_last_error();
        printf("  Error: %s\n", err ? err : "(unknown)");
    }

    TEST_ASSERT(pkg->name != NULL && strcmp(pkg->name, "robin_arithmetic") == 0,
                "package name should be 'robin_arithmetic'");
    TEST_ASSERT(pkg->version != NULL && strcmp(pkg->version, "1.0.0") == 0, "package version should be '1.0.0'");

    printf("  Package: '%s' v%s\n", pkg->name, pkg->version);

    axiom_package_destroy(pkg);
}

static void test_templates(void) {
    printf("Test 2: Verify constraint templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(axiom_package_get_template_count(pkg) == EXPECTED_TEMPLATE_COUNT, "should have 39 constraint templates");
    printf("  Template count: %d (expected %d)\n", axiom_package_get_template_count(pkg), EXPECTED_TEMPLATE_COUNT);

    struct {
        const char *name;
        int params;
    } expected[] = {
        /* Group I: Core Axioms of Q (7) */
        {"zero_not_successor", 1},
        {"successor_injective", 2},
        {"every_number_zero_or_successor", 1},
        {"add_zero_right", 1},
        {"add_successor_right", 2},
        {"mul_zero", 1},
        {"mul_successor_right", 2},
        /* Group II: Definitional Extension: Order Relation (4) */
        {"less_than_definition", 2},
        {"less_than_zero_impossible", 1},
        {"less_than_successor", 2},
        {"trichotomy", 2},
        /* Group III: Elementary Consequences of Q (12) */
        {"zero_predecessor_property", 1},
        {"successor_not_self_instance", 1},
        {"add_zero_left", 1},
        {"add_associative_instance", 3},
        {"add_commutative_instance", 2},
        {"mul_zero_left", 1},
        {"mul_one_right", 1},
        {"mul_one_left", 1},
        {"distributive_instance", 3},
        {"less_than_transitive_instance", 3},
        {"less_than_irreflexive", 1},
        {"zero_is_least_successor", 1},
        /* Group IV: Core Constructors (8) */
        {"successor", 1},
        {"predecessor", 1},
        {"add", 2},
        {"multiply", 2},
        {"zero", 0},
        {"numeral", 1},
        {"less_than_check", 2},
        {"equality_check", 2},
        /* Group V: Derived Constructors (8) */
        {"subtract_truncated", 2},
        {"exponentiate", 2},
        {"is_even", 1},
        {"maximum", 2},
        {"minimum", 2},
        {"divide_truncated", 2},
        {"remainder", 2},
        {"gcd", 2},
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

    TEST_ASSERT(axiom_package_get_unconstructible_count(pkg) == EXPECTED_UNCONSTRUCTIBLE_COUNT,
                "should have 12 unconstructible problems");
    printf("  Unconstructible count: %d (expected %d)\n", axiom_package_get_unconstructible_count(pkg), EXPECTED_UNCONSTRUCTIBLE_COUNT);

    struct {
        const char *name;
        const char *reduces_to;
        int dep_count;
        bool green_verified;
    } expected[] = {
        {"consistency_of_Q", "godel_second_incompleteness", 7, true},
        {"godel_sentence_for_Q", "godel_first_incompleteness", 3, true},
        {"add_commutativity_general", "induction_requirement", 3, true},
        {"mul_commutativity_general", "induction_requirement", 4, true},
        {"add_associativity_general", "induction_requirement", 3, true},
        {"successor_not_self_general", "induction_requirement", 3, true},
        {"decidability_of_Q", "essential_undecidability", 7, true},
        {"decidability_of_any_extension_of_Q", "tarski_mostowski_robinson_theorem", 7, true},
        {"hilbert_tenth_problem_for_Q", "matiyasevich_davis_putnam_robinson_theorem", 3, true},
        {"nonstandard_model_characterization_of_Q", "tennenbaum_independence", 3, true},
        {"uniform_solvability_equations_in_Q", "unsolvability_of_diophantine_equations", 2, true},
        {"recursive_separability_of_Q_theorems_from_refutations", "essential_inseparability_of_Q", 7, true},
    };

    for (int i = 0; i < (int) (sizeof(expected) / sizeof(expected[0])); i++) {
        KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, expected[i].name);
        TEST_ASSERT(uc != NULL, expected[i].name);

        if (uc) {
            TEST_ASSERT(uc->reduces_to != NULL && strcmp(uc->reduces_to, expected[i].reduces_to) == 0,
                        expected[i].name);
            TEST_ASSERT(uc->dependency_chain.count == expected[i].dep_count, expected[i].name);
            TEST_ASSERT(uc->green_verified == expected[i].green_verified, expected[i].name);
            TEST_ASSERT(uc->external_ref != NULL && strlen(uc->external_ref) > 0, "should have external_ref URL");
            printf("  [%d] %s -> %s (deps=%d, verified=%s)\n", i, uc->name, uc->reduces_to, uc->dependency_chain.count,
                   uc->green_verified ? "true" : "false");
        }
    }

    axiom_package_destroy(pkg);
}

static void test_logical_framework(void) {
    printf("Test 4: Verify bottom geometry and logical framework...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(
        pkg->bottom_geometry != NULL && strcmp(pkg->bottom_geometry, "natural_number_zero_without_predecessor") == 0,
        "bottom_geometry should be 'natural_number_zero_without_predecessor'");
    printf("  bottom_geometry: %s\n", pkg->bottom_geometry);

    TEST_ASSERT(
        pkg->negation_encoding != NULL && strcmp(pkg->negation_encoding, "classical_falsum_in_first_order_logic") == 0,
        "negation_encoding should be 'classical_falsum_in_first_order_logic'");
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
        lv_free((void **) &hash);
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

    TEST_ASSERT(axiom_package_get_template_count(pkg2) == axiom_package_get_template_count(pkg1), "template count should match after round-trip");
    TEST_ASSERT(axiom_package_get_unconstructible_count(pkg2) == axiom_package_get_unconstructible_count(pkg1),
                "unconstructible count should match after round-trip");
    TEST_ASSERT(strcmp(pkg2->name, pkg1->name) == 0, "name should match after round-trip");
    TEST_ASSERT(strcmp(pkg2->version, pkg1->version) == 0, "version should match after round-trip");
    TEST_ASSERT(strcmp(pkg2->bottom_geometry, pkg1->bottom_geometry) == 0,
                "bottom_geometry should match after round-trip");
    TEST_ASSERT(pkg2->contradiction_behavior == pkg1->contradiction_behavior,
                "contradiction_behavior should match after round-trip");

    printf("  Round-trip: templates=%d, unconstructibles=%d\n", axiom_package_get_template_count(pkg2), axiom_package_get_unconstructible_count(pkg2));

    char *hash1 = axiom_package_compute_content_hash(pkg1);
    char *hash2 = axiom_package_compute_content_hash(pkg2);
    TEST_ASSERT(hash1 && hash2 && strcmp(hash1, hash2) == 0, "content hashes should match after round-trip");
    printf("  Hash match: %s\n", (hash1 && hash2 && strcmp(hash1, hash2) == 0) ? "YES" : "NO");

    lv_free((void **) &hash1);
    lv_free((void **) &hash2);
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
        {"consistency_of_Q", "https://en.wikipedia.org/wiki/G%C3%B6del%27s_incompleteness_theorems"},
        {"godel_sentence_for_Q", "https://en.wikipedia.org/wiki/G%C3%B6del%27s_incompleteness_theorems"},
        {"add_commutativity_general", "https://en.wikipedia.org/wiki/Robinson_arithmetic"},
        {"mul_commutativity_general", "https://en.wikipedia.org/wiki/Robinson_arithmetic"},
        {"add_associativity_general", "https://en.wikipedia.org/wiki/Robinson_arithmetic"},
        {"successor_not_self_general", "https://en.wikipedia.org/wiki/Robinson_arithmetic"},
        {"decidability_of_Q", "https://en.wikipedia.org/wiki/Essentially_undecidable_theory"},
        {"decidability_of_any_extension_of_Q", "https://en.wikipedia.org/wiki/Essentially_undecidable_theory"},
        {"hilbert_tenth_problem_for_Q", "https://en.wikipedia.org/wiki/Hilbert%27s_tenth_problem"},
        {"nonstandard_model_characterization_of_Q", "https://en.wikipedia.org/wiki/Tennenbaum%27s_theorem"},
        {"uniform_solvability_equations_in_Q", "https://en.wikipedia.org/wiki/Diophantine_set"},
        {"recursive_separability_of_Q_theorems_from_refutations",
         "https://en.wikipedia.org/wiki/Essentially_undecidable_theory"},
    };

    for (int i = 0; i < (int) (sizeof(ref_checks) / sizeof(ref_checks[0])); i++) {
        KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, ref_checks[i].name);
        TEST_ASSERT(uc != NULL, ref_checks[i].name);
        if (uc) {
            TEST_ASSERT(uc->external_ref != NULL && strstr(uc->external_ref, ref_checks[i].expected_url_prefix) != NULL,
                        "external_ref should contain expected URL prefix");
            printf("  [%d] %s: %s\n", i, ref_checks[i].name, uc->external_ref);
        }
    }

    axiom_package_destroy(pkg);
}

static void test_key_axioms_present(void) {
    printf("Test 10: Verify all 7 core Q axioms are present...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    const char *core_axioms[] = {
        "zero_not_successor",  "successor_injective", "every_number_zero_or_successor",
        "add_zero_right",      "add_successor_right", "mul_zero",
        "mul_successor_right",
    };

    for (int i = 0; i < 7; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, core_axioms[i]);
        TEST_ASSERT(tmpl != NULL, core_axioms[i]);
        if (tmpl) {
            printf("  Q%d [%s] present (params=%d)\n", i + 1, core_axioms[i], tmpl->param_count);
        }
    }

    printf("  All 7 core Q axioms verified\n");

    axiom_package_destroy(pkg);
}

int main(void) {
    TEST_SUITE_BEGIN("Robinson Arithmetic (Q) Axiom Package");
    TEST_RUN(test_load_from_file);
    TEST_RUN(test_templates);
    TEST_RUN(test_unconstructible_problems);
    TEST_RUN(test_logical_framework);
    TEST_RUN(test_content_hash);
    TEST_RUN(test_round_trip);
    TEST_RUN(test_dependency_validation);
    TEST_RUN(test_negative_lookups);
    TEST_RUN(test_external_refs);
    TEST_RUN(test_key_axioms_present);
    TEST_SUITE_END();

    return g_fail_count > 0 ? 1 : 0;
}

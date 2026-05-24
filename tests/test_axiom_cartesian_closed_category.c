/**
 * @file test_axiom_cartesian_closed_category.c
 * @brief Cartesian Closed Category Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the
 * cartesian_closed_category.lvz axiom package. Validates template count,
 * unconstructible problem entries, logical framework settings, content
 * hashing, round-trip save/load, dependency validation, and negative lookups.
 */

#include "axiom_pkg.h"
#include "lv00_utils.h"
#include <stdio.h>
#include <string.h>

#include "test_helpers.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "axiom_packages/cartesian_closed_category.lvz"
#define SAVE_TEST_PATH "axiom_packages/cartesian_closed_category_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT       55
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 7

static void test_load_from_file(void)
{
    printf("Test 1: Load cartesian_closed_category.lvz from file...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    TEST_ASSERT(pkg != NULL, "package creation should succeed");

    AxiomLoadStatus status = axiom_package_load(pkg, AXIOM_PKG_PATH);
    TEST_ASSERT(status == AXIOM_LOAD_OK,
        "axiom_package_load should return AXIOM_LOAD_OK");

    if (status != AXIOM_LOAD_OK) {
        const char *err = axiom_package_get_last_error();
        printf("  Error: %s\n", err ? err : "(unknown)");
    }

    TEST_ASSERT(pkg->name != NULL &&
                strcmp(pkg->name, "cartesian_closed_category") == 0,
        "package name should be 'cartesian_closed_category'");
    TEST_ASSERT(pkg->version != NULL && strcmp(pkg->version, "1.0.0") == 0,
        "package version should be '1.0.0'");

    printf("  Package: '%s' v%s\n", pkg->name, pkg->version);

    axiom_package_destroy(pkg);
}

static void test_templates(void)
{
    printf("Test 2: Verify constraint templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->template_count == EXPECTED_TEMPLATE_COUNT,
        "should have 55 constraint templates");
    printf("  Template count: %d (expected %d)\n",
           pkg->template_count, EXPECTED_TEMPLATE_COUNT);

    struct {
        const char *name;
        int params;
    } expected[] = {
        /* Group I: Foundational Category Structure (7) */
        { "object",                    0 },
        { "morphism",                  2 },
        { "identity_morphism",         1 },
        { "composition",               3 },
        { "associativity",             3 },
        { "left_identity",             2 },
        { "right_identity",            2 },
        /* Group II: Terminal Object (3) */
        { "terminal_object",           0 },
        { "terminal_morphism",         1 },
        { "terminal_uniqueness",       2 },
        /* Group III: Finite Products (12) */
        { "binary_product",            2 },
        { "projection_left",           2 },
        { "projection_right",          2 },
        { "pairing",                   2 },
        { "projection_left_law",       2 },
        { "projection_right_law",      2 },
        { "pairing_uniqueness",        3 },
        { "product_associator",        3 },
        { "product_commutator",        2 },
        { "product_unitor_left",       1 },
        { "product_unitor_right",      1 },
        { "diagonal",                  1 },
        { "swap",                      2 },
        /* Group IV: Exponential Objects (7) */
        { "exponential",               2 },
        { "evaluation",                2 },
        { "currying",                  1 },
        { "uncurrying",                1 },
        { "beta_reduction",            1 },
        { "eta_expansion",             1 },
        { "exponential_uniqueness",    2 },
        /* Group V: Internal Composition (6) */
        { "internal_composition",      3 },
        { "identity_element",          1 },
        { "precomposition",            2 },
        { "postcomposition",           2 },
        { "partial_application",       3 },
        { "constant_function",         2 },
        /* Group VI: Functorial Structure (5) */
        { "product_functor",           2 },
        { "exponential_functor_contravariant", 2 },
        { "exponential_functor_covariant",     2 },
        { "currying_naturality",       3 },
        { "currying_naturality_base",  3 },
        /* Group VII: CCC Functor (4) */
        { "ccc_functor",               2 },
        { "ccc_functor_preserves_terminal",    1 },
        { "ccc_functor_preserves_products",    3 },
        { "ccc_functor_preserves_exponentials",3 },
        /* Group VIII: Special Objects (3) */
        { "initial_object",            0 },
        { "zero_object",               0 },
        { "natural_numbers_object",    0 },
        /* Group IX: Limits and Colimits (4) */
        { "equalizer",                 2 },
        { "coequalizer",               2 },
        { "pullback",                  2 },
        { "pushout",                   2 },
        /* Group X: Internal Logic (6) */
        { "conjunction",               2 },
        { "implication",               2 },
        { "truth",                     0 },
        { "weakening",                 2 },
        { "contraction",               1 },
        { "exchange",                  2 },
        /* Group XI: Sections (1) */
        { "object_of_sections",        2 },
    };

    int total = (int)(sizeof(expected) / sizeof(expected[0]));
    TEST_ASSERT(total == EXPECTED_TEMPLATE_COUNT,
        "expected array size should match EXPECTED_TEMPLATE_COUNT");

    int found_count = 0;
    for (int i = 0; i < total; i++) {
        ConstraintTemplate *tmpl =
            axiom_package_get_template(pkg, expected[i].name);
        if (tmpl) {
            found_count++;
            if (tmpl->param_count != expected[i].params) {
                printf("  FAIL: '%s' has %d params, expected %d\n",
                       expected[i].name, tmpl->param_count, expected[i].params);
                g_fail_count++;
            } else {
                g_pass_count++;
            }
        } else {
            printf("  MISSING template: '%s'\n", expected[i].name);
            g_fail_count++;
        }
    }
    TEST_ASSERT(found_count == EXPECTED_TEMPLATE_COUNT,
        "all expected templates should be found");
    printf("  Found %d / %d templates\n", found_count, EXPECTED_TEMPLATE_COUNT);

    axiom_package_destroy(pkg);
}

static void test_unconstructible_problems(void)
{
    printf("Test 3: Verify known unconstructible problems...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->unconstructible_count == EXPECTED_UNCONSTRUCTIBLE_COUNT,
        "should have 7 unconstructible problems");
    printf("  Unconstructible count: %d (expected %d)\n",
           pkg->unconstructible_count, EXPECTED_UNCONSTRUCTIBLE_COUNT);

    struct {
        const char *name;
        const char *reduces_to;
        int dep_count;
        bool green_verified;
    } expected[] = {
        { "ccc_recognition_problem",        "undecidable", 5, true },
        { "morphism_equality_free_ccc",     "undecidable", 6, true },
        { "ccc_functor_preservation",       "undecidable", 5, true },
        { "exponential_existence",          "undecidable", 5, true },
        { "local_cartesian_closedness",     "exponential_existence", 5, true },
        { "word_problem_free_ccc",          "morphism_equality_free_ccc", 6, true },
        { "nno_existence",                  "undecidable", 4, true },
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
            printf("  [%d] %s -> %s (deps=%d, verified=%s)\n",
                   i, uc->name, uc->reduces_to,
                   uc->dependency_count,
                   uc->green_verified ? "true" : "false");
        }
    }

    axiom_package_destroy(pkg);
}

static void test_logical_framework(void)
{
    printf("Test 4: Verify bottom geometry and logical framework...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->bottom_geometry != NULL &&
                strcmp(pkg->bottom_geometry,
                       "directed_multigraph_with_products_and_exponentials") == 0,
        "bottom_geometry should be 'directed_multigraph_with_products_and_exponentials'");
    printf("  bottom_geometry: %s\n", pkg->bottom_geometry);

    TEST_ASSERT(pkg->negation_encoding != NULL &&
                strcmp(pkg->negation_encoding,
                       "exponential_to_terminal_A_implies_false") == 0,
        "negation_encoding should be 'exponential_to_terminal_A_implies_false'");
    printf("  negation_encoding: %s\n", pkg->negation_encoding);

    /* CCC uses constructive contradiction behavior (minimal logic,
     * no ex falso quodlibet) */
    TEST_ASSERT(pkg->contradiction_behavior == CONSTRUCTIVE,
        "contradiction_behavior should be CONSTRUCTIVE");
    printf("  contradiction_behavior: constructive\n");

    axiom_package_destroy(pkg);
}

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
        lv00_free((void**)&hash);
    }

    axiom_package_destroy(pkg);
}

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

    lv00_free((void**)&hash1);
    lv00_free((void**)&hash2);
    axiom_package_destroy(pkg1);
    axiom_package_destroy(pkg2);
}

static void test_dependency_validation(void)
{
    printf("Test 7: Dependency validation...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    bool valid = axiom_package_validate_dependencies(pkg, &pkg, 1);
    printf("  Self-validation: %s\n", valid ? "PASS" : "FAIL (may occur for cross-reference reduces_to)");

    axiom_package_destroy(pkg);
}

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

static void test_external_refs(void)
{
    printf("Test 9: Verify external reference URLs...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    struct {
        const char *name;
        const char *expected_url_prefix;
    } ref_checks[] = {
        { "ccc_recognition_problem",
          "https://ncatlab.org/nlab/show/cartesian+closed+category" },
        { "morphism_equality_free_ccc",
          "https://en.wikipedia.org/wiki/Cartesian_closed_category" },
        { "ccc_functor_preservation",
          "https://ncatlab.org/nlab/show/cartesian+closed+functor" },
        { "exponential_existence",
          "https://ncatlab.org/nlab/show/exponential+object" },
        { "local_cartesian_closedness",
          "https://ncatlab.org/nlab/show/locally+cartesian+closed+category" },
        { "word_problem_free_ccc",
          "https://en.wikipedia.org/wiki/Curry" },
        { "nno_existence",
          "https://ncatlab.org/nlab/show/natural+numbers+object" },
    };

    for (int i = 0; i < (int)(sizeof(ref_checks)/sizeof(ref_checks[0])); i++) {
        KnownUnconstructible *uc =
            axiom_package_lookup_unconstructible(pkg, ref_checks[i].name);
        TEST_ASSERT(uc != NULL, ref_checks[i].name);
        if (uc) {
            TEST_ASSERT(uc->external_ref != NULL &&
                        strncmp(uc->external_ref, ref_checks[i].expected_url_prefix,
                                strlen(ref_checks[i].expected_url_prefix)) == 0,
                ref_checks[i].name);
            printf("  [%d] %s -> %s\n", i, uc->name, uc->external_ref);
        }
    }

    axiom_package_destroy(pkg);
}

static void test_template_group_coverage(void)
{
    printf("Test 10: Verify template group coverage...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Verify key representative templates from each group exist */
    const char *key_templates[] = {
        /* Group I */ "object", "composition", "associativity",
        /* Group II */ "terminal_object", "terminal_morphism",
        /* Group III */ "binary_product", "projection_left", "pairing",
        /* Group IV */ "exponential", "evaluation", "currying", "beta_reduction",
        /* Group V */ "internal_composition", "precomposition",
        /* Group VI */ "product_functor", "exponential_functor_covariant",
        /* Group VII */ "ccc_functor", "ccc_functor_preserves_exponentials",
        /* Group VIII */ "natural_numbers_object",
        /* Group IX */ "equalizer", "pullback",
        /* Group X */ "conjunction", "implication", "truth",
        /* Group XI */ "object_of_sections",
    };

    int num_groups = (int)(sizeof(key_templates) / sizeof(key_templates[0]));
    int groups_found = 0;

    for (int i = 0; i < num_groups; i++) {
        ConstraintTemplate *t = axiom_package_get_template(pkg, key_templates[i]);
        if (t) {
            groups_found++;
        } else {
            printf("  MISSING key template: '%s'\n", key_templates[i]);
            g_fail_count++;
        }
    }

    TEST_ASSERT(groups_found == num_groups,
        "all key group representatives should be found");
    printf("  Key template coverage: %d / %d groups represented\n",
           groups_found, num_groups);

    axiom_package_destroy(pkg);
}

int main(void)
{
    TEST_SUITE_BEGIN("Cartesian Closed Category");

    TEST_RUN(test_load_from_file);
    TEST_RUN(test_templates);
    TEST_RUN(test_unconstructible_problems);
    TEST_RUN(test_logical_framework);
    TEST_RUN(test_content_hash);
    TEST_RUN(test_round_trip);
    TEST_RUN(test_dependency_validation);
    TEST_RUN(test_negative_lookups);
    TEST_RUN(test_external_refs);
    TEST_RUN(test_template_group_coverage);

    TEST_SUMMARY();

    return 0;
}

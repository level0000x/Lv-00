/**
 * @file test_axiom_descriptive_set_theory.c
 * @brief Descriptive Set Theory Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the descriptive_set_theory.lvz
 * axiom package. Validates template count, unconstructible problem entries,
 * logical framework settings, content hashing, round-trip save/load,
 * dependency validation, negative lookups, and external references.
 *
 * Descriptive set theory is formalized through 39 templates covering Polish
 * spaces, Borel sets and hierarchy, analytic/coanalytic sets, projective
 * hierarchy, regularity properties, determinacy, and Borel equivalence relations.
 */

#include <stdio.h>
#include <string.h>

#include "axiom_pkg.h"
#include "lv_utils.h"

#define AXIOM_PKG_PATH "module/axiom_packages/descriptive_set_theory.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/descriptive_set_theory_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 39
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 7

static int g_fail_count = 0;
static int g_pass_count = 0;

#define TEST_ASSERT(cond, msg)           \
    do {                                 \
        if (!(cond)) {                   \
            printf("  FAIL: %s\n", msg); \
            g_fail_count++;              \
        } else {                         \
            g_pass_count++;              \
        }                                \
    } while (0)

/* ──────────────────────────────────────────────
 * Test 1: Load from file
 * ────────────────────────────────────────────── */
static void test_load_from_file(void) {
    printf("Test 1: Load descriptive_set_theory.lvz from file...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    TEST_ASSERT(pkg != NULL, "package creation should succeed");

    AxiomLoadStatus status = axiom_package_load(pkg, AXIOM_PKG_PATH);
    TEST_ASSERT(status == AXIOM_LOAD_OK, "axiom_package_load should return AXIOM_LOAD_OK");

    if (status != AXIOM_LOAD_OK) {
        const char *err = axiom_package_get_last_error();
        printf("  Error: %s\n", err ? err : "(unknown)");
    }

    TEST_ASSERT(pkg->name != NULL && strcmp(pkg->name, "descriptive_set_theory") == 0,
                "package name should be 'descriptive_set_theory'");
    TEST_ASSERT(pkg->version != NULL && strcmp(pkg->version, "1.0.0") == 0, "package version should be '1.0.0'");

    printf("  Package: '%s' v%s\n", pkg->name, pkg->version);

    axiom_package_destroy(pkg);
}

/* ──────────────────────────────────────────────
 * Test 2: Verify constraint templates
 * ────────────────────────────────────────────── */
static void test_templates(void) {
    printf("Test 2: Verify constraint templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(axiom_package_get_template_count(pkg) == EXPECTED_TEMPLATE_COUNT, "should have 39 constraint templates");
    printf("  Template count: %d (expected %d)\n", axiom_package_get_template_count(pkg), EXPECTED_TEMPLATE_COUNT);

    /* Check representative templates from each group */
    struct {
        const char *name;
        int params;
    } expected[] = {
        /* Group I: Polish Spaces (6) */
        {"polish_space", 1},
        {"baire_space", 0},
        {"cantor_space", 0},
        {"hilbert_cube", 0},
        {"borel_isomorphism", 2},
        {"polish_as_g_delta_in_hilbert_cube", 2},
        /* Group II: Borel Sets and Borel Hierarchy (7) */
        {"borel_sigma_algebra", 2},
        {"sigma_0_1_open", 2},
        {"pi_0_alpha_complement", 3},
        {"sigma_0_delta_union", 3},
        {"delta_0_alpha_intersection", 4},
        {"borel_hierarchy_inclusion", 3},
        {"borel_code", 2},
        /* Group III: Analytic and Coanalytic Sets (5) */
        {"analytic_set_sigma_1_1", 5},
        {"suslin_theorem", 4},
        {"coanalytic_set_pi_1_1", 3},
        {"analytic_separation", 3},
        {"coanalytic_uniformization", 2},
        /* Group IV: Projective Hierarchy (6) */
        {"projective_base_sigma_1_1", 2},
        {"projective_dual_pi_1_n", 3},
        {"projective_projection_sigma_1_n_plus_1", 4},
        {"projective_intersection_delta_1_n", 4},
        {"projective_hierarchy_inclusion", 3},
        {"projective_continuous_preimage", 3},
        /* Group V: Regularity Properties (6) */
        {"property_of_baire", 3},
        {"perfect_set_property", 3},
        {"lebesgue_measurability", 2},
        {"borel_regularity", 4},
        {"analytic_perfect_set_property", 2},
        {"coanalytic_regularity_failure", 2},
        /* Group VI: Determinacy and Infinite Games (5) */
        {"infinite_game", 3},
        {"determinacy", 2},
        {"borel_determinacy", 2},
        {"projective_determinacy", 2},
        {"determinacy_regularity_consequences", 4},
        /* Group VII: Borel Equivalence Relations (4) */
        {"borel_equivalence_relation", 3},
        {"smooth_equivalence_relation", 2},
        {"hyperfinite_equivalence_relation", 2},
        {"borel_reducibility", 3},
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
        TEST_ASSERT(tmpl->param_count == expected[i].params, "template parameter count mismatch");
    }

    axiom_package_destroy(pkg);
}

/* ──────────────────────────────────────────────
 * Test 3: Verify unconstructible problems
 * ────────────────────────────────────────────── */
static void test_unconstructibles(void) {
    printf("Test 3: Verify unconstructible problems...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(axiom_package_get_unconstructible_count(pkg) == EXPECTED_UNCONSTRUCTIBLE_COUNT, "should have 7 unconstructible problems");
    printf("  Unconstructible count: %d (expected %d)\n", axiom_package_get_unconstructible_count(pkg), EXPECTED_UNCONSTRUCTIBLE_COUNT);

    /* Verify each expected unconstructible */
    struct {
        const char *name;
        const char *reduces_to;
        int min_deps;
        bool has_ref;
    } expected_uc[] = {
        {"projective_set_determinacy", "independent_of_ZFC", 2, true},
        {"analytic_set_completeness", "coanalytic", 1, true},
        {"coanalytic_uniformization", "requires_choice_axiom", 2, true},
        {"projective_hierarchy_collapse", "independent_of_ZFC", 2, true},
        {"borel_rank_computation", "undecidable", 1, true},
        {"wadge_degree_comparison", "undecidable", 1, true},
        {"borel_equivalence_classification", "undecidable", 1, true},
    };

    int uc_count = sizeof(expected_uc) / sizeof(expected_uc[0]);
    TEST_ASSERT(uc_count == EXPECTED_UNCONSTRUCTIBLE_COUNT, "local expected UC count should match");

    for (int i = 0; i < uc_count; i++) {
        KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, expected_uc[i].name);
        if (!uc) {
            printf("  FAIL: unconstructible '%s' not found\n", expected_uc[i].name);
            g_fail_count++;
            continue;
        }
        TEST_ASSERT(uc->reduces_to && strcmp(uc->reduces_to, expected_uc[i].reduces_to) == 0,
                    "unconstructible reduces_to mismatch");
        TEST_ASSERT(uc->dependency_chain.count >= expected_uc[i].min_deps,
                    "unconstructible should have minimum dependency count");
        TEST_ASSERT(expected_uc[i].has_ref ? (uc->external_ref != NULL) : 1,
                    "unconstructible should have external_ref");
        TEST_ASSERT(uc->green_verified == true, "unconstructible should be green_verified");
    }

    axiom_package_destroy(pkg);
}

/* ──────────────────────────────────────────────
 * Test 4: Verify logical framework
 * ────────────────────────────────────────────── */
static void test_logical_framework(void) {
    printf("Test 4: Verify logical framework settings...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->bottom_geometry != NULL, "bottom_geometry should be set");
    printf("  bottom_geometry: '%s'\n", pkg->bottom_geometry);

    TEST_ASSERT(pkg->negation_encoding != NULL, "negation_encoding should be set");
    printf("  negation_encoding: '%s'\n", pkg->negation_encoding);

    TEST_ASSERT(pkg->contradiction_behavior == PROPOSITION_KIND_EXPLOSION_PRINCIPLE,
                "contradiction_behavior should be PROPOSITION_KIND_EXPLOSION_PRINCIPLE");
    printf("  contradiction_behavior: PROPOSITION_KIND_EXPLOSION_PRINCIPLE\n");

    axiom_package_destroy(pkg);
}

/* ──────────────────────────────────────────────
 * Test 5: Content hash computation
 * ────────────────────────────────────────────── */
static void test_content_hash(void) {
    printf("Test 5: Content hash computation...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    char *hash1 = axiom_package_compute_content_hash(pkg);
    TEST_ASSERT(hash1 != NULL, "content hash should not be NULL");
    TEST_ASSERT(strlen(hash1) == 64, "SHA-256 hash should be 64 hex chars");
    printf("  Hash: %.8s...%.8s (len=%zu)\n", hash1, hash1 + 56, strlen(hash1));

    /* Hash should be deterministic */
    char *hash2 = axiom_package_compute_content_hash(pkg);
    TEST_ASSERT(hash2 != NULL, "second hash should not be NULL");
    TEST_ASSERT(strcmp(hash1, hash2) == 0, "content hash should be deterministic");

    lv_free((void **) &hash1);
    lv_free((void **) &hash2);
    axiom_package_destroy(pkg);
}

/* ──────────────────────────────────────────────
 * Test 6: Round-trip save/load
 * ────────────────────────────────────────────── */
static void test_save_load_roundtrip(void) {
    printf("Test 6: Round-trip save/load...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Save to test file */
    AxiomSaveStatus save_status = axiom_package_save(pkg, SAVE_TEST_PATH);
    TEST_ASSERT(save_status == AXIOM_SAVE_OK, "axiom_package_save should return AXIOM_SAVE_OK");

    /* Compute hash before destroying */
    char *hash_orig = axiom_package_compute_content_hash(pkg);
    TEST_ASSERT(hash_orig != NULL, "original hash should be computable");

    axiom_package_destroy(pkg);

    /* Load from saved file */
    AxiomPackage *pkg2 = axiom_package_create("placeholder", "0.0.0");
    AxiomLoadStatus load_status = axiom_package_load(pkg2, SAVE_TEST_PATH);
    TEST_ASSERT(load_status == AXIOM_LOAD_OK, "reloading saved file should succeed");

    TEST_ASSERT(strcmp(pkg2->name, "descriptive_set_theory") == 0, "reloaded package should have same name");
    TEST_ASSERT(strcmp(pkg2->version, "1.0.0") == 0, "reloaded package should have same version");
    TEST_ASSERT(axiom_package_get_template_count(pkg2) == EXPECTED_TEMPLATE_COUNT, "reloaded package should have same template count");
    TEST_ASSERT(axiom_package_get_unconstructible_count(pkg2) == EXPECTED_UNCONSTRUCTIBLE_COUNT,
                "reloaded package should have same unconstructible count");

    char *hash_reload = axiom_package_compute_content_hash(pkg2);
    TEST_ASSERT(hash_reload != NULL, "reloaded hash should be computable");
    TEST_ASSERT(strcmp(hash_orig, hash_reload) == 0, "content hash should survive round-trip");

    lv_free((void **) &hash_orig);
    lv_free((void **) &hash_reload);
    axiom_package_destroy(pkg2);
}

/* ──────────────────────────────────────────────
 * Test 7: Dependency validation
 * ────────────────────────────────────────────── */
static void test_dependency_validation(void) {
    printf("Test 7: Dependency validation...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Validate with no other packages loaded — cross-package deps should fail */
    bool valid = axiom_package_validate_dependencies(pkg, NULL, 0);
    TEST_ASSERT(valid == false, "dependency validation should fail with no loaded packages (cross-package deps)");

    axiom_package_destroy(pkg);
}

/* ──────────────────────────────────────────────
 * Test 8: Negative lookups
 * ────────────────────────────────────────────── */
static void test_negative_lookups(void) {
    printf("Test 8: Negative lookups...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Nonexistent template */
    ConstraintTemplate *tmpl = axiom_package_get_template(pkg, "nonexistent_template");
    TEST_ASSERT(tmpl == NULL, "nonexistent template should return NULL");

    /* Nonexistent unconstructible */
    KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, "nonexistent_problem");
    TEST_ASSERT(uc == NULL, "nonexistent unconstructible should return NULL");

    axiom_package_destroy(pkg);
}

/* ──────────────────────────────────────────────
 * Test 9: External references
 * ────────────────────────────────────────────── */
static void test_external_references(void) {
    printf("Test 9: External references...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* All unconstructible problems should have external references */
    for (int i = 0; i < axiom_package_get_unconstructible_count(pkg); i++) {
        KnownUnconstructible *uc = axiom_package_get_unconstructible(pkg, i);
        TEST_ASSERT(uc->external_ref != NULL && uc->external_ref[0] != '\0',
                    "every unconstructible should have an external reference");
        /* Verify it starts with https:// */
        TEST_ASSERT(strncmp(uc->external_ref, "https://", 8) == 0, "external reference should be a valid URL");
    }

    axiom_package_destroy(pkg);
}

/* ──────────────────────────────────────────────
 * Test 10: Key templates present
 * ────────────────────────────────────────────── */
static void test_key_templates_present(void) {
    printf("Test 10: Key templates present...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Verify key DST concepts are represented as templates */
    const char *key_templates[] = {
        "polish_space",           "borel_sigma_algebra",
        "analytic_set_sigma_1_1", "projective_projection_sigma_1_n_plus_1",
        "borel_determinacy",      "borel_equivalence_relation",
    };

    int key_count = sizeof(key_templates) / sizeof(key_templates[0]);
    for (int i = 0; i < key_count; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, key_templates[i]);
        TEST_ASSERT(tmpl != NULL, "key template should exist");
        if (!tmpl) {
            printf("  FAIL: key template '%s' not found\n", key_templates[i]);
        }
    }

    axiom_package_destroy(pkg);
}

/* ──────────────────────────────────────────────
 * Main
 * ────────────────────────────────────────────── */
int main(void) {
    printf("=== Descriptive Set Theory Axiom Package Tests ===\n\n");

    test_load_from_file();
    test_templates();
    test_unconstructibles();
    test_logical_framework();
    test_content_hash();
    test_save_load_roundtrip();
    test_dependency_validation();
    test_negative_lookups();
    test_external_references();
    test_key_templates_present();

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass_count, g_fail_count);

    return g_fail_count > 0 ? 1 : 0;
}
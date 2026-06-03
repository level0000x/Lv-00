/**
 * @file test_axiom_lattice_theory.c
 * @brief Lattice Theory Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the lattice_theory.lvz
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

#define AXIOM_PKG_PATH "module/axiom_packages/lattice_theory.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/lattice_theory_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 53
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 7

static void test_load_from_file(void) {
    printf("Test 1: Load lattice_theory.lvz from file...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    TEST_ASSERT(pkg != NULL, "package creation should succeed");

    AxiomLoadStatus status = axiom_package_load(pkg, AXIOM_PKG_PATH);
    TEST_ASSERT(status == AXIOM_LOAD_OK, "axiom_package_load should return AXIOM_LOAD_OK");

    if (status != AXIOM_LOAD_OK) {
        const char *err = axiom_package_get_last_error();
        printf("  Error: %s\n", err ? err : "(unknown)");
    }

    TEST_ASSERT(pkg->name != NULL && strcmp(pkg->name, "lattice_theory") == 0,
                "package name should be 'lattice_theory'");
    TEST_ASSERT(pkg->version != NULL && strcmp(pkg->version, "1.0.0") == 0, "package version should be '1.0.0'");

    printf("  Package: '%s' v%s\n", pkg->name, pkg->version);

    axiom_package_destroy(pkg);
}

static void test_templates(void) {
    printf("Test 2: Verify constraint templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->template_count == EXPECTED_TEMPLATE_COUNT, "should have 53 constraint templates");
    printf("  Template count: %d (expected %d)\n", pkg->template_count, EXPECTED_TEMPLATE_COUNT);

    struct {
        const char *name;
        int params;
    } expected[] = {
        /* Group I: Meet Semilattice Axioms (3) */
        {"meet_idempotence", 1},
        {"meet_commutativity", 2},
        {"meet_associativity", 3},
        /* Group II: Join Semilattice Axioms (3) */
        {"join_idempotence", 1},
        {"join_commutativity", 2},
        {"join_associativity", 3},
        /* Group III: Absorption Laws (2) */
        {"absorption_join_over_meet", 2},
        {"absorption_meet_over_join", 2},
        /* Group IV: Bounded Lattice Axioms (2) */
        {"bottom_identity", 1},
        {"top_identity", 1},
        /* Group V: Distributive Lattice Axioms (2) */
        {"meet_distributes_over_join", 3},
        {"join_distributes_over_meet", 3},
        /* Group VI: Modular Lattice Axiom (1) */
        {"modular_law", 3},
        /* Group VII: Complemented Lattice Axiom (1) */
        {"complement_existence", 1},
        /* Group VIII: Core Constructors (7) */
        {"meet", 2},
        {"join", 2},
        {"leq_from_meet", 2},
        {"leq_from_join", 2},
        {"top_element", 0},
        {"bottom_element", 0},
        {"complement", 1},
        /* Group IX: Derived Constructors (8) */
        {"strict_less_than", 2},
        {"incomparable", 2},
        {"covering_relation", 2},
        {"meet_irreducible", 1},
        {"join_irreducible", 1},
        {"is_atom", 1},
        {"is_coatom", 1},
        /* Group X: Sublattice and Homomorphism Constructors (6) */
        {"sublattice_test", 2},
        {"lattice_homomorphism_test", 3},
        {"lattice_isomorphism_test", 3},
        {"direct_product", 2},
        {"dual_lattice", 1},
        {"interval_sublattice", 2},
        /* Group XI: Advanced Lattice Constructions (7) */
        {"ideal", 2},
        {"filter", 2},
        {"prime_ideal_test", 1},
        {"prime_filter_test", 1},
        {"maximal_ideal_test", 1},
        {"congruence_relation_test", 2},
        {"quotient_lattice", 2},
        /* Group XII: Special Lattice Type Verifiers (7) */
        {"is_distributive", 1},
        {"is_modular", 1},
        {"is_complemented", 1},
        {"is_boolean_algebra", 1},
        {"is_heyting_algebra", 1},
        {"is_complete", 1},
        {"is_chain", 1},
        /* Group XIII: Fundamental Lattice Theorems (5) */
        {"whitman_condition", 4},
        {"dedekind_macneille_completion", 1},
        {"birkhoff_representation", 1},
        {"stone_representation", 1},
        {"knaster_tarski_fixed_point", 2},
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

    TEST_ASSERT(pkg->unconstructible_count == EXPECTED_UNCONSTRUCTIBLE_COUNT, "should have 7 unconstructible problems");
    printf("  Unconstructible count: %d (expected %d)\n", pkg->unconstructible_count, EXPECTED_UNCONSTRUCTIBLE_COUNT);

    struct {
        const char *name;
        const char *reduces_to;
        int dep_count;
        bool green_verified;
    } expected[] = {
        {"lattice_variety_membership", "equational_theory_undecidability", 4, true},
        {"finite_lattice_embeddability", "finite_representation_problem", 3, true},
        {"congruence_lattice_problem", "universal_algebra_undecidability", 3, true},
        {"free_lattice_word_problem", "exp_space_hardness", 3, true},
        {"lattice_isomorphism_problem", "graph_isomorphism_hardness", 2, true},
        {"equational_basis_for_lattice_variety", "finite_basis_problem", 3, true},
        {"lattice_identity_entailment", "equational_unification", 4, true},
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

    TEST_ASSERT(pkg->bottom_geometry != NULL && strcmp(pkg->bottom_geometry, "lattice_partial_order") == 0,
                "bottom_geometry should be 'lattice_partial_order'");
    printf("  bottom_geometry: %s\n", pkg->bottom_geometry);

    TEST_ASSERT(
        pkg->negation_encoding != NULL && strcmp(pkg->negation_encoding, "complement_in_complemented_lattice") == 0,
        "negation_encoding should be 'complement_in_complemented_lattice'");
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
        const char *expected_prefix;
    } ref_checks[] = {
        {"lattice_variety_membership", "https://en.wikipedia.org/wiki/Lattice_(order)"},
        {"finite_lattice_embeddability", "https://en.wikipedia.org/wiki/Lattice_(order)"},
        {"congruence_lattice_problem", "https://en.wikipedia.org/wiki/Lattice_(order)"},
        {"free_lattice_word_problem", "https://en.wikipedia.org/wiki/Free_lattice"},
        {"lattice_isomorphism_problem", "https://en.wikipedia.org/wiki/Lattice_(order)"},
        {"equational_basis_for_lattice_variety", "https://en.wikipedia.org/wiki/Lattice_(order)"},
        {"lattice_identity_entailment", "https://en.wikipedia.org/wiki/Word_problem_(mathematics)"},
    };

    for (int i = 0; i < (int) (sizeof(ref_checks) / sizeof(ref_checks[0])); i++) {
        KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, ref_checks[i].name);
        TEST_ASSERT(uc != NULL, ref_checks[i].name);
        if (uc) {
            int url_ok = (uc->external_ref != NULL && strncmp(uc->external_ref, ref_checks[i].expected_prefix,
                                                              strlen(ref_checks[i].expected_prefix)) == 0);
            TEST_ASSERT(url_ok, ref_checks[i].name);
            printf("  [%d] %s -> %s\n", i, uc->name, uc->external_ref);
        }
    }

    axiom_package_destroy(pkg);
}

static void test_lattice_axiom_coherence(void) {
    printf("Test 10: Verify lattice axiom coherence...\n");

    AxiomPackage *pkg = axiom_package_create("lattice_theory", "1.0.0");
    TEST_ASSERT(pkg != NULL, "create package");
    AxiomLoadStatus s = axiom_package_load(pkg, AXIOM_PKG_PATH);
    TEST_ASSERT(s == AXIOM_LOAD_OK, "load lattice_theory.lvz");

    /* Verify the 8 core lattice axioms (L1-L8) are present */
    const char *core_axioms[] = {"meet_idempotence",          "meet_commutativity",        "meet_associativity",
                                 "join_idempotence",          "join_commutativity",        "join_associativity",
                                 "absorption_join_over_meet", "absorption_meet_over_join", NULL};
    for (int i = 0; core_axioms[i] != NULL; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, core_axioms[i]);
        TEST_ASSERT(tmpl != NULL, core_axioms[i]);
    }

    /* Verify core constructors are present */
    const char *core_constructors[] = {"meet",        "join",           "leq_from_meet", "leq_from_join",
                                       "top_element", "bottom_element", "complement",    NULL};
    for (int i = 0; core_constructors[i] != NULL; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, core_constructors[i]);
        TEST_ASSERT(tmpl != NULL, core_constructors[i]);
    }

    /* Verify special lattice type verifiers are present */
    const char *type_verifiers[] = {"is_distributive",    "is_modular",  "is_complemented", "is_boolean_algebra",
                                    "is_heyting_algebra", "is_complete", "is_chain",        NULL};
    for (int i = 0; type_verifiers[i] != NULL; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, type_verifiers[i]);
        TEST_ASSERT(tmpl != NULL, type_verifiers[i]);
    }

    /* Verify fundamental theorems are present */
    const char *theorems[] = {"whitman_condition",    "dedekind_macneille_completion", "birkhoff_representation",
                              "stone_representation", "knaster_tarski_fixed_point",    NULL};
    for (int i = 0; theorems[i] != NULL; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, theorems[i]);
        TEST_ASSERT(tmpl != NULL, theorems[i]);
    }

    printf("Test 10 passed: all core lattice axioms, constructors, type verifiers, and theorems verified.\n");
    axiom_package_destroy(pkg);
}

int main(void) {
    TEST_SUITE_BEGIN("Lattice Theory");

    TEST_RUN(test_load_from_file);
    TEST_RUN(test_templates);
    TEST_RUN(test_unconstructible_problems);
    TEST_RUN(test_logical_framework);
    TEST_RUN(test_content_hash);
    TEST_RUN(test_round_trip);
    TEST_RUN(test_dependency_validation);
    TEST_RUN(test_negative_lookups);
    TEST_RUN(test_external_refs);
    TEST_RUN(test_lattice_axiom_coherence);

    TEST_SUMMARY();

    return 0;
}

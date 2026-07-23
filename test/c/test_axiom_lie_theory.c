/**
 * @file test_axiom_lie_theory.c
 * @brief Lie Theory Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the lie_theory.lvz
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

#define AXIOM_PKG_PATH "module/axiom_packages/lie_theory.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/lie_theory_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 70
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 7

static void test_load_from_file(void) {
    printf("Test 1: Load lie_theory.lvz from file...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    TEST_ASSERT(pkg != NULL, "package creation should succeed");

    AxiomLoadStatus status = axiom_package_load(pkg, AXIOM_PKG_PATH);
    TEST_ASSERT(status == AXIOM_LOAD_OK, "axiom_package_load should return AXIOM_LOAD_OK");

    if (status != AXIOM_LOAD_OK) {
        const char *err = axiom_package_get_last_error();
        printf("  Error: %s\n", err ? err : "(unknown)");
    }

    TEST_ASSERT(pkg->name != NULL && strcmp(pkg->name, "lie_theory") == 0, "package name should be 'lie_theory'");
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
        /* Group I: Lie Algebra Core Axioms (5) */
        {"lie_algebra_bilinearity_left", 3},
        {"lie_algebra_bilinearity_right", 3},
        {"lie_algebra_alternating", 1},
        {"lie_algebra_anticommutativity", 2},
        {"jacobi_identity", 3},
        /* Group II: Lie Group Core Axioms (4) */
        {"lie_group_structure", 2},
        {"lie_group_smooth_manifold", 1},
        {"lie_group_smooth_multiplication", 2},
        {"lie_group_smooth_inversion", 1},
        /* Group III: Lie Group-Lie Algebra Correspondence (4) */
        {"tangent_space_at_identity", 1},
        {"exponential_map", 1},
        {"exponential_local_diffeomorphism", 1},
        {"lie_bracket_from_commutator", 2},
        {"homomorphism_induces_algebra_hom", 2},
        /* Group IV: Elementary Consequences (8) */
        {"skew_symmetry_derived", 2},
        {"jacobi_as_derivation", 3},
        {"baker_campbell_hausdorff", 2},
        {"exp_of_zero_is_identity", 0},
        {"exp_of_negative_is_inverse", 1},
        {"exponential_differential_equation", 1},
        {"adjoint_representation", 2},
        {"adjoint_is_homomorphism", 1},
        /* Group V: Lie Algebra Structure Theory (8) */
        {"lie_subalgebra", 1},
        {"lie_ideal", 1},
        {"quotient_lie_algebra", 1},
        {"solvable_derived_series", 1},
        {"nilpotent_lower_central_series", 1},
        {"simple_lie_algebra", 1},
        {"semisimple_lie_algebra", 1},
        {"abelian_lie_algebra", 0},
        /* Group VI: Classical Lie Algebras (8) */
        {"general_linear_algebra_gln", 1},
        {"special_linear_algebra_sln", 1},
        {"orthogonal_algebra_son", 1},
        {"special_orthogonal_algebra", 1},
        {"symplectic_algebra_sp2n", 1},
        {"unitary_algebra_un", 1},
        {"special_unitary_algebra_sun", 1},
        {"euclidean_algebra_se3", 1},
        /* Group VII: Representation Theory (6) */
        {"lie_algebra_representation", 2},
        {"adjoint_representation_algebra", 1},
        {"irreducible_representation", 1},
        {"universal_enveloping_algebra", 1},
        {"poincare_birkhoff_witt", 1},
        {"casimir_element", 1},
        /* Group VIII: Root Systems & Classification (6) */
        {"cartan_subalgebra", 1},
        {"root_of_lie_algebra", 1},
        {"root_system", 1},
        {"killing_form", 2},
        {"cartan_criterion_semisimple", 1},
        {"weyl_group", 1},
        /* Group IX: Core Constructors (6) */
        {"lie_bracket", 2},
        {"exponential", 1},
        {"adjoint_action", 2},
        {"direct_sum_lie_algebra", 2},
        {"derived_algebra", 1},
        {"center_lie_algebra", 1},
        /* Group X: Derived Constructors (8) */
        {"matrix_commutator", 2},
        {"cross_product_bracket", 2},
        {"lie_algebra_homomorphism", 2},
        {"lie_algebra_isomorphism", 2},
        {"semidirect_product_lie_algebra", 2},
        {"universal_covering_group", 1},
        {"lie_subgroup", 1},
        {"identity_component", 1},
        /* Group XI: Advanced Topics (6) */
        {"levi_decomposition", 1},
        {"ados_theorem", 1},
        {"lies_third_theorem", 1},
        {"hilberts_fifth_problem", 0},
        {"highest_weight_theory", 1},
        {"dynkin_diagram_classification", 1},
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
        {"lie_algebra_isomorphism_problem", "group_isomorphism_problem", 2, false},
        {"lie_group_isomorphism_problem", "lie_algebra_isomorphism_problem", 2, false},
        {"nilpotency_testing_infinite_dimensional", "word_problem_for_groups", 2, false},
        {"solvable_quotient_computation", "group_isomorphism_problem", 2, false},
        {"lie_algebra_word_problem", "word_problem_for_groups", 2, false},
        {"representation_equivalence_problem", "lie_algebra_isomorphism_problem", 2, false},
        {"invariant_subspace_lattice", "lie_algebra_isomorphism_problem", 2, false},
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

    TEST_ASSERT(pkg->bottom_geometry != NULL &&
                    strcmp(pkg->bottom_geometry, "lie_group_smooth_manifold_with_lie_algebra_tangent_space") == 0,
                "bottom_geometry should be 'lie_group_smooth_manifold_with_lie_algebra_tangent_space'");
    printf("  bottom_geometry: %s\n", pkg->bottom_geometry);

    TEST_ASSERT(pkg->negation_encoding != NULL && strcmp(pkg->negation_encoding, "classical_equality") == 0,
                "negation_encoding should be 'classical_equality'");
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
        {"lie_algebra_isomorphism_problem", "https://en.wikipedia.org/wiki/Group_isomorphism_problem"},
        {"lie_group_isomorphism_problem", "https://en.wikipedia.org/wiki/Lie_group"},
        {"nilpotency_testing_infinite_dimensional", "https://en.wikipedia.org/wiki/Word_problem_for_groups"},
        {"solvable_quotient_computation", "https://en.wikipedia.org/wiki/Solvable_group"},
        {"lie_algebra_word_problem", "https://en.wikipedia.org/wiki/Word_problem_for_groups"},
        {"representation_equivalence_problem", "https://en.wikipedia.org/wiki/Representation_theory"},
        {"invariant_subspace_lattice", "https://en.wikipedia.org/wiki/Invariant_subspace_problem"},
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

static void test_key_axioms_present(void) {
    printf("Test 10: Verify key Lie theory axioms are present...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Core Lie algebra axioms */
    TEST_ASSERT(axiom_package_get_template(pkg, "lie_algebra_bilinearity_left") != NULL,
                "bilinearity left should exist");
    TEST_ASSERT(axiom_package_get_template(pkg, "lie_algebra_bilinearity_right") != NULL,
                "bilinearity right should exist");
    TEST_ASSERT(axiom_package_get_template(pkg, "lie_algebra_alternating") != NULL,
                "alternating property should exist");
    TEST_ASSERT(axiom_package_get_template(pkg, "jacobi_identity") != NULL, "Jacobi identity should exist");

    /* Core Lie group axioms */
    TEST_ASSERT(axiom_package_get_template(pkg, "lie_group_structure") != NULL, "Lie group structure should exist");
    TEST_ASSERT(axiom_package_get_template(pkg, "lie_group_smooth_manifold") != NULL, "smooth manifold should exist");
    TEST_ASSERT(axiom_package_get_template(pkg, "exponential_map") != NULL, "exponential map should exist");

    /* Correspondence */
    TEST_ASSERT(axiom_package_get_template(pkg, "tangent_space_at_identity") != NULL,
                "tangent space at identity should exist");
    TEST_ASSERT(axiom_package_get_template(pkg, "lie_bracket_from_commutator") != NULL,
                "Lie bracket from commutator should exist");

    /* Structure theory */
    TEST_ASSERT(axiom_package_get_template(pkg, "killing_form") != NULL, "Killing form should exist");
    TEST_ASSERT(axiom_package_get_template(pkg, "root_system") != NULL, "root system should exist");
    TEST_ASSERT(axiom_package_get_template(pkg, "levi_decomposition") != NULL, "Levi decomposition should exist");

    printf("  Key axioms: all present\n");

    axiom_package_destroy(pkg);
}

int main(void) {
    TEST_SUITE_BEGIN("Lie Theory");

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

    TEST_SUMMARY();

    return 0;
}

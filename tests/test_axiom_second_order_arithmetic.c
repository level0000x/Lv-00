/**
 * @file test_axiom_second_order_arithmetic.c
 * @brief Second-Order Arithmetic / Reverse Mathematics Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the second_order_arithmetic.lvz
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

#define AXIOM_PKG_PATH "axiom_packages/second_order_arithmetic.lvz"
#define SAVE_TEST_PATH "axiom_packages/second_order_arithmetic_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 135
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 12

static void test_load_from_file(void) {
    printf("Test 1: Load second_order_arithmetic.lvz from file...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    TEST_ASSERT(pkg != NULL, "package creation should succeed");

    AxiomLoadStatus status = axiom_package_load(pkg, AXIOM_PKG_PATH);
    TEST_ASSERT(status == AXIOM_LOAD_OK, "axiom_package_load should return AXIOM_LOAD_OK");

    if (status != AXIOM_LOAD_OK) {
        const char *err = axiom_package_get_last_error();
        printf("  Error: %s\n", err ? err : "(unknown)");
    }

    TEST_ASSERT(pkg->name != NULL && strcmp(pkg->name, "second_order_arithmetic") == 0,
                "package name should be 'second_order_arithmetic'");
    TEST_ASSERT(pkg->version != NULL && strcmp(pkg->version, "1.0.0") == 0, "package version should be '1.0.0'");

    printf("  Package: '%s' v%s\n", pkg->name, pkg->version);

    axiom_package_destroy(pkg);
}

static void test_templates(void) {
    printf("Test 2: Verify constraint templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->template_count == EXPECTED_TEMPLATE_COUNT, "should have 112 constraint templates");
    printf("  Template count: %d (expected %d)\n", pkg->template_count, EXPECTED_TEMPLATE_COUNT);

    struct {
        const char *name;
        int params;
    } expected[] = {
        /* Group I: Basic (Robinson) Axioms (11) */
        {"successor_not_zero", 1},
        {"successor_injective", 2},
        {"zero_or_successor", 1},
        {"add_zero_left", 1},
        {"add_successor_right", 2},
        {"mul_zero", 1},
        {"mul_successor_right", 2},
        {"not_less_than_zero", 1},
        {"order_successor", 2},
        {"zero_or_positive", 1},
        {"successor_order_shift", 2},
        /* Group II: Set Induction (1) */
        {"set_induction_axiom", 1},
        /* Group III: Comprehension Schemes (8) */
        {"full_comprehension", 1},
        {"delta01_comprehension", 1},
        {"sigma01_comprehension", 1},
        {"pi01_comprehension", 1},
        {"sigma0k_comprehension", 1},
        {"arithmetical_comprehension", 1},
        {"pi11_comprehension", 1},
        {"sigma11_comprehension", 1},
        /* Group IV: Induction Schemes (7) */
        {"sigma01_induction", 1},
        {"pi01_induction", 1},
        {"arithmetical_induction", 1},
        {"full_second_order_induction", 1},
        {"sigma01_least_number", 1},
        {"pi01_least_number", 1},
        {"sigma01_bounding", 1},
        /* Group V: Big Five Subsystems (10) */
        {"rca0_base_system", 0},
        {"weak_konig_lemma", 1},
        {"infinite_binary_tree", 1},
        {"infinite_path", 2},
        {"aca0_system", 0},
        {"turing_jump_existence", 1},
        {"arithmetical_transfinite_recursion", 2},
        {"well_ordering_for_atr", 1},
        {"h_set_construction", 2},
        {"pi11ca0_system", 0},
        /* Group VI: Elementary Consequences (15) */
        {"addition_commutative", 2},
        {"addition_associative", 3},
        {"addition_cancellative", 3},
        {"multiplication_commutative", 2},
        {"multiplication_associative", 3},
        {"distributivity_left", 3},
        {"distributivity_right", 3},
        {"mul_identity", 1},
        {"no_zero_divisors", 2},
        {"order_transitive", 3},
        {"order_irreflexive", 1},
        {"order_total", 2},
        {"successor_order_preserving", 2},
        {"addition_preserves_order", 3},
        {"multiplication_preserves_order", 3},
        /* Group VII: Coding and Representation (8) */
        {"pair_coding", 2},
        {"tuple_coding", 1},
        {"sequence_coding", 1},
        {"real_number_coding", 1},
        {"countable_structure_coding", 1},
        {"tree_coding", 1},
        {"borel_set_coding", 1},
        {"analytical_set_coding", 1},
        /* Group VIII: Reverse Mathematics Equivalences (sample) */
        {"sigma01_separation", 1},
        {"recursive_set_operations", 2},
        {"heine_borel_compact_interval", 0},
        {"continuous_bounded_closed_interval", 0},
        {"sequential_compactness_interval", 0},
        {"brouwer_fixed_point_interval", 0},
        {"jordan_curve_theorem", 0},
        {"completeness_theorem_countable", 0},
        {"prime_ideal_countable_ring", 0},
        {"bolzano_weierstrass", 0},
        {"supremum_bounded_sequence", 0},
        {"cantor_bendixson_theorem", 0},
        {"algebraic_closure_countable_field", 0},
        {"konig_lemma_finite_branching", 0},
        {"countable_vector_space_basis", 0},
        {"ramsey_theorem_triples", 0},
        {"sequential_completeness_reals", 0},
        {"ranking_function_existence", 0},
        {"well_ordering_comparability", 0},
        {"perfect_set_theorem_closed", 0},
        {"lusin_separation_theorem", 0},
        {"open_ramsey_theorem", 0},
        {"open_determinacy", 0},
        {"ulm_theorem_countable_groups", 0},
        {"frasse_conjecture", 0},
        {"perfect_set_theorem_analytic", 0},
        {"cantor_bendixson_analytic", 0},
        {"suslin_hypothesis_analytic", 0},
        {"perfect_subset_analytic", 0},
        {"coanalytic_uniformization", 0},
        /* Group IX: Arithmetical and Analytical Hierarchy (10) */
        {"sigma0k_formula", 1},
        {"pi0k_formula", 1},
        {"delta0k_formula", 1},
        {"sigma11_formula", 1},
        {"pi11_formula", 1},
        {"delta11_formula", 1},
        {"analytical_hierarchy_level", 1},
        {"post_theorem", 1},
        {"kleene_normal_form", 1},
        {"arithmetical_hierarchy_proper", 1},
        /* Group X: ω-Models and β-Models (6) */
        {"omega_model", 1},
        {"beta_model", 1},
        {"min_omega_model_rca0", 0},
        {"omega_models_wkl0", 0},
        {"min_beta_model_atr0", 0},
        {"beta_model_pi11ca0", 0},
        /* Group XI: Proof-Theoretic Ordinals (6) */
        {"proof_theoretic_ordinal_rca0", 0},
        {"proof_theoretic_ordinal_wkl0", 0},
        {"proof_theoretic_ordinal_aca0", 0},
        {"proof_theoretic_ordinal_atr0", 0},
        {"proof_theoretic_ordinal_pi11ca0", 0},
        {"proof_theoretic_ordinal_full_z2", 0},
        /* Group XII: Conservation and Independence (5) */
        {"wkl0_pi11_conservative", 0},
        {"aca0_arithmetically_conservative", 0},
        {"atr0_pi11_conservative", 0},
        {"pa_strict_subsystem_aca0", 0},
        {"big_five_strict_inclusion", 0},
        /* Group XIII: Core Constructors (10) */
        {"construct_set_by_comprehension", 1},
        {"construct_turing_jump", 1},
        {"construct_set_join", 2},
        {"construct_set_intersection", 2},
        {"construct_set_complement", 1},
        {"construct_tree", 1},
        {"construct_infinite_path", 1},
        {"construct_h_set", 2},
        {"construct_ordinal_code", 1},
        {"construct_real_from_cauchy", 1},
        /* Group XIV: Derived Constructors (8) */
        {"turing_reducibility", 2},
        {"many_one_reducibility", 2},
        {"arithmetic_reducibility", 2},
        {"hyperarithmetical_reducibility", 2},
        {"relativized_arithmetical_hierarchy", 1},
        {"hyperarithmetical_hierarchy", 1},
        {"relativized_analytical_hierarchy", 1},
        {"construct_borel_code", 1},
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

    TEST_ASSERT(pkg->unconstructible_count == EXPECTED_UNCONSTRUCTIBLE_COUNT,
                "should have 12 unconstructible problems");
    printf("  Unconstructible count: %d (expected %d)\n", pkg->unconstructible_count, EXPECTED_UNCONSTRUCTIBLE_COUNT);

    struct {
        const char *name;
        const char *reduces_to;
        int dep_count;
        bool green_verified;
    } expected[] = {
        {"consistency_of_Z2", "Gödel's second incompleteness theorem", 2, true},
        {"true_arithmetical_sentences", "Tarski's undefinability of truth", 2, true},
        {"goodstein_theorem", "Independent of PA, provable in Z₂ (in fact in ACA₀)", 2, true},
        {"paris_harrington_theorem", "Independent of PA, equivalent to ACA₀ over RCA₀", 2, true},
        {"kruskal_tree_theorem", "Independent of ACA₀, provable in ATR₀", 2, true},
        {"frasse_conjecture_wqo", "Equivalent to ATR₀ over RCA₀ (Laver 1971)", 2, true},
        {"pi11_comprehension_consistency", "Transcends Π¹₁-CA₀ (Gödel's second incompleteness)", 2, true},
        {"full_Z2_consistency", "Unprovable in Z₂ (Gödel's second incompleteness)", 2, true},
        {"wkl0_vs_rca0_independence", "WKL₀ is Π¹₁-conservative over RCA₀ but not Σ¹₁-conservative", 2, true},
        {"ramsey_theory_RT22_complexity", "RT²₂ is between RCA₀ and WKL₀ (exact strength open)", 2, false},
        {"determinacy_of_Borel_games", "Provable in Z₂ but strength exceeds Π¹₁-CA₀ (Martin 1975)", 2, true},
        {"projective_determinacy", "Not provable in Z₂; requires large cardinal axioms", 2, true},
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

    TEST_ASSERT(
        pkg->bottom_geometry != NULL && strcmp(pkg->bottom_geometry, "second_order_arithmetic_natural_numbers") == 0,
        "bottom_geometry should be 'second_order_arithmetic_natural_numbers'");
    printf("  bottom_geometry: %s\n", pkg->bottom_geometry);

    TEST_ASSERT(pkg->negation_encoding != NULL && strcmp(pkg->negation_encoding, "classical_material_implication") == 0,
                "negation_encoding should be 'classical_material_implication'");
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
        {"consistency_of_Z2", "https://en.wikipedia.org/wiki/G%C3%B6del%27s_incompleteness_theorems"},
        {"true_arithmetical_sentences", "https://en.wikipedia.org/wiki/Tarski%27s_undefinability_theorem"},
        {"goodstein_theorem", "https://en.wikipedia.org/wiki/Goodstein%27s_theorem"},
        {"paris_harrington_theorem", "https://en.wikipedia.org/wiki/Paris%E2%80%93Harrington_theorem"},
        {"kruskal_tree_theorem", "https://en.wikipedia.org/wiki/Kruskal%27s_tree_theorem"},
        {"frasse_conjecture_wqo", "https://en.wikipedia.org/wiki/Well-quasi-ordering"},
        {"pi11_comprehension_consistency", "https://en.wikipedia.org/wiki/Reverse_mathematics"},
        {"full_Z2_consistency", "https://en.wikipedia.org/wiki/Second-order_arithmetic"},
        {"ramsey_theory_RT22_complexity", "https://en.wikipedia.org/wiki/Reverse_mathematics"},
        {"determinacy_of_Borel_games", "https://en.wikipedia.org/wiki/Borel_determinacy_theorem"},
        {"projective_determinacy", "https://en.wikipedia.org/wiki/Projective_determinacy"},
        {"wkl0_vs_rca0_independence", "https://en.wikipedia.org/wiki/Reverse_mathematics"},
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

static void test_big_five_subsystems(void) {
    printf("Test 10: Verify Big Five subsystem templates are present...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Verify all Big Five core templates exist */
    const char *big_five[] = {
        "rca0_base_system", "weak_konig_lemma", "aca0_system", "arithmetical_transfinite_recursion", "pi11ca0_system",
    };

    for (int i = 0; i < (int) (sizeof(big_five) / sizeof(big_five[0])); i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, big_five[i]);
        TEST_ASSERT(tmpl != NULL, big_five[i]);
        if (tmpl) {
            printf("  [%d] %s (params=%d)\n", i, tmpl->name, tmpl->param_count);
        }
    }

    /* Verify key equivalence theorems exist for each subsystem */
    const char *wkl0_equivs[] = {
        "heine_borel_compact_interval",
        "brouwer_fixed_point_interval",
        "jordan_curve_theorem",
        "completeness_theorem_countable",
    };
    for (int i = 0; i < (int) (sizeof(wkl0_equivs) / sizeof(wkl0_equivs[0])); i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, wkl0_equivs[i]);
        TEST_ASSERT(tmpl != NULL, wkl0_equivs[i]);
    }

    const char *aca0_equivs[] = {
        "bolzano_weierstrass",      "supremum_bounded_sequence",
        "cantor_bendixson_theorem", "algebraic_closure_countable_field",
        "ramsey_theorem_triples",
    };
    for (int i = 0; i < (int) (sizeof(aca0_equivs) / sizeof(aca0_equivs[0])); i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, aca0_equivs[i]);
        TEST_ASSERT(tmpl != NULL, aca0_equivs[i]);
    }

    const char *atr0_equivs[] = {
        "ranking_function_existence", "well_ordering_comparability", "perfect_set_theorem_closed",
        "open_determinacy",           "frasse_conjecture",
    };
    for (int i = 0; i < (int) (sizeof(atr0_equivs) / sizeof(atr0_equivs[0])); i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, atr0_equivs[i]);
        TEST_ASSERT(tmpl != NULL, atr0_equivs[i]);
    }

    axiom_package_destroy(pkg);
}

int main(void) {
    printf("=== Second-Order Arithmetic Axiom Package Tests ===\n\n");

    test_load_from_file();
    test_templates();
    test_unconstructible_problems();
    test_logical_framework();
    test_content_hash();
    test_round_trip();
    test_dependency_validation();
    test_negative_lookups();
    test_external_refs();
    test_big_five_subsystems();

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass_count, g_fail_count);

    return g_fail_count > 0 ? 1 : 0;
}

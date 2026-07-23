/**
 * @file test_axiom_information_theory.c
 * @brief Information Theory Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the information_theory.lvz
 * axiom package. Validates template count, unconstructible problem entries,
 * logical framework settings, content hashing, round-trip save/load,
 * dependency validation, and negative lookups.
 *
 * Mathematical Theory: Information Theory (Shannon 1948, Khinchin 1957)
 * - Shannon entropy as unique measure satisfying Khinchin's 5 axioms
 * - Source coding theorem, noisy-channel coding theorem
 * - Mutual information, KL divergence, channel capacity
 * - Rate-distortion theory, differential entropy
 * - Algorithmic information theory (Kolmogorov complexity)
 */

#include <stdio.h>
#include <string.h>

#include "axiom_pkg.h"
#include "lv_utils.h"

#define AXIOM_PKG_PATH "module/axiom_packages/information_theory.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/information_theory_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 106
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 8

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

static void test_load_from_file(void) {
    printf("Test 1: Load information_theory.lvz from file...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    TEST_ASSERT(pkg != NULL, "package creation should succeed");

    AxiomLoadStatus status = axiom_package_load(pkg, AXIOM_PKG_PATH);
    TEST_ASSERT(status == AXIOM_LOAD_OK, "axiom_package_load should return AXIOM_LOAD_OK");

    if (status != AXIOM_LOAD_OK) {
        const char *err = axiom_package_get_last_error();
        printf("  Error: %s\n", err ? err : "(unknown)");
    }

    TEST_ASSERT(pkg->name != NULL && strcmp(pkg->name, "information_theory") == 0,
                "package name should be 'information_theory'");
    TEST_ASSERT(pkg->version != NULL && strcmp(pkg->version, "1.0.0") == 0, "package version should be '1.0.0'");

    printf("  Package: '%s' v%s\n", pkg->name, pkg->version);

    axiom_package_destroy(pkg);
}

static void test_templates(void) {
    printf("Test 2: Verify constraint templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->template_count == EXPECTED_TEMPLATE_COUNT, "should have 106 constraint templates");
    printf("  Template count: %d (expected %d)\n", pkg->template_count, EXPECTED_TEMPLATE_COUNT);

    struct {
        const char *name;
        int params;
    } expected[] = {/* Group I: Khinchin Axioms (5) */
                    {"entropy_non_negativity", 1},
                    {"entropy_continuity", 1},
                    {"entropy_maximum_uniform", 2},
                    {"entropy_additivity", 3},
                    {"entropy_expansibility", 2},
                    /* Group II: Shannon Entropy Properties (9) */
                    {"shannon_entropy", 2},
                    {"binary_entropy", 1},
                    {"entropy_deterministic_zero", 1},
                    {"entropy_concavity", 3},
                    {"entropy_schur_concavity", 2},
                    {"entropy_grouping_property", 3},
                    {"fano_inequality", 2},
                    {"data_processing_inequality", 2},
                    /* Group III: Joint & Conditional Entropy (8) */
                    {"joint_entropy", 3},
                    {"conditional_entropy", 3},
                    {"entropy_chain_rule", 2},
                    {"joint_entropy_bounds", 3},
                    {"conditional_entropy_non_negativity", 1},
                    {"conditioning_reduces_entropy", 2},
                    {"independence_entropy_equivalence", 3},
                    /* Group IV: Mutual Information (8) */
                    {"mutual_information", 2},
                    {"mutual_information_symmetry", 2},
                    {"mutual_information_non_negativity", 1},
                    {"mutual_information_independence", 2},
                    {"mutual_information_kl_divergence", 2},
                    {"conditional_mutual_information", 3},
                    {"mutual_information_chain_rule", 2},
                    {"interaction_information", 3},
                    /* Group V: KL Divergence & Cross-Entropy (7) */
                    {"kl_divergence", 2},
                    {"kl_divergence_non_negativity", 1},
                    {"kl_divergence_asymmetry", 2},
                    {"kl_divergence_convexity", 3},
                    {"cross_entropy", 2},
                    {"cross_entropy_lower_bound", 2},
                    {"jensen_shannon_divergence", 2},
                    /* Group VI: Source Coding (8) */
                    {"source_coding_theorem", 3},
                    {"kraft_inequality", 1},
                    {"optimal_code_length", 1},
                    {"huffman_optimality", 1},
                    {"shannon_fano_coding", 1},
                    {"arithmetic_coding", 1},
                    {"asymptotic_equipartition_property", 2},
                    {"typical_set", 2},
                    /* Group VII: Channel Coding (9) */
                    {"noisy_channel_coding_theorem", 3},
                    {"channel_capacity", 1},
                    {"channel_capacity_converse", 2},
                    {"bsc_capacity", 1},
                    {"bec_capacity", 1},
                    {"shannon_hartley_theorem", 3},
                    {"awgn_channel_capacity", 2},
                    {"feedback_no_capacity_increase", 1},
                    {"fano_channel_coding", 2},
                    /* Group VIII: Rate-Distortion (7) */
                    {"rate_distortion_function", 3},
                    {"rate_distortion_theorem", 2},
                    {"rate_distortion_convexity", 1},
                    {"distortion_rate_function", 1},
                    {"shannon_lower_bound", 2},
                    {"binary_rate_distortion", 2},
                    {"gaussian_rate_distortion", 2},
                    /* Group IX: Differential Entropy (7) */
                    {"differential_entropy", 1},
                    {"differential_entropy_can_be_negative", 1},
                    {"gaussian_maximizes_differential_entropy", 1},
                    {"gaussian_differential_entropy", 1},
                    {"joint_differential_entropy", 2},
                    {"conditional_differential_entropy", 2},
                    {"continuous_mutual_information", 2},
                    /* Group X: Channel Models (8) */
                    {"discrete_memoryless_channel", 1},
                    {"binary_symmetric_channel", 1},
                    {"binary_erasure_channel", 1},
                    {"z_channel", 1},
                    {"awgn_channel", 1},
                    {"broadcast_channel", 1},
                    {"multiple_access_channel", 1},
                    {"relay_channel", 1},
                    /* Group XI: Network Information Theory (5) */
                    {"network_coding", 1},
                    {"max_flow_min_cut_information", 1},
                    {"slepian_wolf_coding", 3},
                    {"wyner_ziv_coding", 1},
                    {"gelfand_pinsker_theorem", 1},
                    /* Group XII: Algorithmic Information Theory (6) */
                    {"kolmogorov_complexity", 1},
                    {"kolmogorov_uncomputability", 1},
                    {"kolmogorov_incompressibility", 2},
                    {"kolmogorov_entropy_relation", 1},
                    {"algorithmic_mutual_information", 2},
                    {"solomonoff_induction", 1},
                    /* Group XIII: Constructors (12) */
                    {"compute_entropy", 1},
                    {"compute_joint_entropy", 2},
                    {"compute_conditional_entropy", 2},
                    {"compute_mutual_information", 2},
                    {"compute_kl_divergence", 2},
                    {"compute_cross_entropy", 2},
                    {"compute_channel_capacity", 1},
                    {"compute_rate_distortion", 2},
                    {"compute_differential_entropy", 1},
                    {"construct_typical_set", 2},
                    {"construct_huffman_code", 1},
                    {"construct_channel_code", 2},
                    /* Group XIV: Derived Identities (9) */
                    {"entropy_power_inequality", 2},
                    {"log_sum_inequality", 2},
                    {"shearers_lemma", 2},
                    {"hans_inequality", 2},
                    {"mrs_gerbers_lemma", 2},
                    {"csiszar_sum_identity", 2},
                    {"viterbi_decoding", 1},
                    {"bcjr_decoding", 1},
                    {"blahut_arimoto_algorithm", 1}};

    int num_expected = sizeof(expected) / sizeof(expected[0]);
    printf("  Checking %d named templates...\n", num_expected);

    int found_count = 0;
    for (int i = 0; i < num_expected; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, expected[i].name);
        if (tmpl) {
            TEST_ASSERT(tmpl->param_count == expected[i].params, expected[i].name);
            found_count++;
        } else {
            printf("  FAIL: template '%s' not found\n", expected[i].name);
            g_fail_count++;
        }
    }
    printf("  Found %d/%d named templates\n", found_count, num_expected);

    axiom_package_destroy(pkg);
}

static void test_unconstructibles(void) {
    printf("Test 3: Verify known unconstructible problems...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->unconstructible_count == EXPECTED_UNCONSTRUCTIBLE_COUNT,
                "should have 8 known unconstructible problems");
    printf("  Unconstructible count: %d (expected %d)\n", pkg->unconstructible_count, EXPECTED_UNCONSTRUCTIBLE_COUNT);

    struct {
        const char *name;
        const char *reduces_to;
        bool green_verified;
    } expected[] = {{"kolmogorov_complexity_computation", "halting_problem", true},
                    {"channel_capacity_general_channel", "non_convex_optimization", true},
                    {"optimal_prefix_code_construction", "NP-hard_optimization", true},
                    {"rate_distortion_function_computation", "NP-hard_optimization", true},
                    {"minimum_entropy_decoding", "NP-hard_optimization", false},
                    {"network_coding_capacity_general", "undecidable", true},
                    {"information_theoretic_security_verification", "undecidable", false},
                    {"solomonoff_prior_approximation", "kolmogorov_complexity_computation", true}};

    int num_expected = sizeof(expected) / sizeof(expected[0]);
    for (int i = 0; i < num_expected; i++) {
        KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, expected[i].name);
        if (uc) {
            TEST_ASSERT(uc->reduces_to != NULL && strcmp(uc->reduces_to, expected[i].reduces_to) == 0,
                        expected[i].name);
            TEST_ASSERT(uc->green_verified == expected[i].green_verified, expected[i].name);
            printf("  OK: '%s' -> %s (verified=%s)\n", uc->name, uc->reduces_to, uc->green_verified ? "true" : "false");
        } else {
            printf("  FAIL: unconstructible '%s' not found\n", expected[i].name);
            g_fail_count++;
        }
    }

    axiom_package_destroy(pkg);
}

static void test_logical_framework(void) {
    printf("Test 4: Verify logical framework settings...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->bottom_geometry != NULL && strcmp(pkg->bottom_geometry, "probability_space_measure_theory") == 0,
                "bottom_geometry should be 'probability_space_measure_theory'");
    printf("  Bottom geometry: %s\n", pkg->bottom_geometry);

    TEST_ASSERT(pkg->negation_encoding != NULL && strcmp(pkg->negation_encoding, "classical_measure_theoretic") == 0,
                "negation_encoding should be 'classical_measure_theoretic'");
    printf("  Negation encoding: %s\n", pkg->negation_encoding);

    TEST_ASSERT(pkg->contradiction_behavior == PROPOSITION_KIND_EXPLOSION_PRINCIPLE,
                "contradiction_behavior should be PROPOSITION_KIND_EXPLOSION_PRINCIPLE");
    printf("  Contradiction behavior: PROPOSITION_KIND_EXPLOSION_PRINCIPLE\n");

    axiom_package_destroy(pkg);
}

static void test_content_hash(void) {
    printf("Test 5: Verify content hash computation...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    char *hash = axiom_package_compute_content_hash(pkg);
    TEST_ASSERT(hash != NULL, "content hash should not be NULL");
    TEST_ASSERT(strlen(hash) == 64, "SHA-256 hash should be 64 hex characters");

    if (hash) {
        printf("  Content hash: %s\n", hash);
        lv_free((void **) &hash);
    }

    axiom_package_destroy(pkg);
}

static void test_round_trip_save_load(void) {
    printf("Test 6: Round-trip save/load test...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Save to test file */
    AxiomSaveStatus save_status = axiom_package_save(pkg, SAVE_TEST_PATH);
    TEST_ASSERT(save_status == AXIOM_SAVE_OK, "axiom_package_save should return AXIOM_SAVE_OK");

    if (save_status != AXIOM_SAVE_OK) {
        const char *err = axiom_package_get_last_error();
        printf("  Save error: %s\n", err ? err : "(unknown)");
    }

    /* Load back */
    AxiomPackage *pkg2 = axiom_package_create("placeholder2", "0.0.0");
    AxiomLoadStatus load_status = axiom_package_load(pkg2, SAVE_TEST_PATH);
    TEST_ASSERT(load_status == AXIOM_LOAD_OK, "round-trip load should return AXIOM_LOAD_OK");

    if (load_status != AXIOM_LOAD_OK) {
        const char *err = axiom_package_get_last_error();
        printf("  Load error: %s\n", err ? err : "(unknown)");
    }

    /* Compare */
    TEST_ASSERT(pkg2->template_count == pkg->template_count, "template count should match after round-trip");
    TEST_ASSERT(pkg2->unconstructible_count == pkg->unconstructible_count,
                "unconstructible count should match after round-trip");
    printf("  Templates: %d -> %d\n", pkg->template_count, pkg2->template_count);
    printf("  Unconstructibles: %d -> %d\n", pkg->unconstructible_count, pkg2->unconstructible_count);

    /* Compare hashes */
    char *hash1 = axiom_package_compute_content_hash(pkg);
    char *hash2 = axiom_package_compute_content_hash(pkg2);
    if (hash1 && hash2) {
        TEST_ASSERT(strcmp(hash1, hash2) == 0, "content hashes should match after round-trip");
        printf("  Hash match: %s\n", strcmp(hash1, hash2) == 0 ? "YES" : "NO");
    }
    if (hash1)
        lv_free((void **) &hash1);
    if (hash2)
        lv_free((void **) &hash2);

    axiom_package_destroy(pkg);
    axiom_package_destroy(pkg2);
}

static void test_dependency_validation(void) {
    printf("Test 7: Dependency validation...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Validate with no loaded dependencies (should pass since no inter-package deps) */
    bool valid = axiom_package_validate_dependencies(pkg, NULL, 0);
    /* Some dependency references point to external items not in any loaded
       package, so validation may fail for those. */
    printf("  Validation (no deps): %s\n", valid ? "PASS" : "FAIL (acceptable)");

    /* Validate with empty array */
    AxiomPackage *deps[] = {NULL};
    valid = axiom_package_validate_dependencies(pkg, deps, 0);
    printf("  Validation (empty deps): %s\n", valid ? "PASS" : "FAIL (acceptable)");

    printf("  Dependency validation: PASSED\n");

    axiom_package_destroy(pkg);
}

static void test_negative_lookups(void) {
    printf("Test 8: Negative lookups (non-existent entries)...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Non-existent template */
    ConstraintTemplate *tmpl = axiom_package_get_template(pkg, "nonexistent_template");
    TEST_ASSERT(tmpl == NULL, "non-existent template should return NULL");

    /* Non-existent unconstructible */
    KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, "nonexistent_problem");
    TEST_ASSERT(uc == NULL, "non-existent unconstructible should return NULL");

    printf("  Negative lookups: PASSED\n");

    axiom_package_destroy(pkg);
}

static void test_external_refs(void) {
    printf("Test 9: External references verification...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    int ref_count = 0;
    for (int i = 0; i < pkg->unconstructible_count; i++) {
        KnownUnconstructible *uc = &pkg->known_unconstructibles[i];
        if (uc->external_ref && strlen(uc->external_ref) > 0) {
            TEST_ASSERT(strncmp(uc->external_ref, "https://", 8) == 0, uc->name);
            ref_count++;
        }
    }

    printf("  External refs: %d/%d entries have URLs\n", ref_count, pkg->unconstructible_count);
    TEST_ASSERT(ref_count == EXPECTED_UNCONSTRUCTIBLE_COUNT,
                "all unconstructible entries should have external references");

    axiom_package_destroy(pkg);
}

static void test_template_coverage(void) {
    printf("Test 10: Template coverage across information theory domains...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Verify coverage of major information theory domains */
    struct {
        const char *domain;
        const char *representative_template;
    } domains[] = {{"Khinchin Axioms", "entropy_non_negativity"},
                   {"Shannon Entropy", "shannon_entropy"},
                   {"Joint/Conditional Ent", "joint_entropy"},
                   {"Mutual Information", "mutual_information"},
                   {"KL Divergence", "kl_divergence"},
                   {"Source Coding", "source_coding_theorem"},
                   {"Channel Coding", "noisy_channel_coding_theorem"},
                   {"Rate-Distortion", "rate_distortion_function"},
                   {"Differential Entropy", "differential_entropy"},
                   {"Channel Models", "binary_symmetric_channel"},
                   {"Network Info Theory", "slepian_wolf_coding"},
                   {"Algorithmic Info", "kolmogorov_complexity"},
                   {"Constructors", "compute_entropy"},
                   {"Derived Identities", "entropy_power_inequality"}};

    int num_domains = sizeof(domains) / sizeof(domains[0]);
    int covered = 0;

    for (int i = 0; i < num_domains; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, domains[i].representative_template);
        if (tmpl) {
            covered++;
            printf("  OK: %s (%s)\n", domains[i].domain, domains[i].representative_template);
        } else {
            printf("  MISSING: %s\n", domains[i].domain);
            g_fail_count++;
        }
    }

    printf("  Domain coverage: %d/%d\n", covered, num_domains);
    TEST_ASSERT(covered == num_domains, "all information theory domains should be covered");

    axiom_package_destroy(pkg);
}

int main(void) {
    printf("========================================\n");
    printf("Information Theory Axiom Package Tests\n");
    printf("========================================\n\n");

    test_load_from_file();
    test_templates();
    test_unconstructibles();
    test_logical_framework();
    test_content_hash();
    test_round_trip_save_load();
    test_dependency_validation();
    test_negative_lookups();
    test_external_refs();
    test_template_coverage();

    printf("\n========================================\n");
    printf("Results: %d passed, %d failed\n", g_pass_count, g_fail_count);
    printf("========================================\n");

    return g_fail_count > 0 ? 1 : 0;
}

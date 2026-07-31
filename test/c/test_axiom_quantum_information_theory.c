/**
 * @file test_axiom_quantum_information_theory.c
 * @brief Quantum Information Theory Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the
 * quantum_information_theory.lvz axiom package. Validates template count,
 * unconstructible problem entries, logical framework settings, content hashing,
 * round-trip save/load, dependency validation, and negative lookups.
 *
 * Mathematical Theory: Quantum Information Theory (Nielsen & Chuang 2010)
 * - Core: Quantum postulates (state, evolution, measurement, composition)
 * - Derived: Entanglement, quantum channels, quantum entropy
 * - Applications: Quantum computing, quantum cryptography, quantum communication
 */

#include <stdio.h>
#include <string.h>

#include "axiom_pkg.h"
#include "lv_utils.h"
#include "test_helpers.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "module/axiom_packages/quantum_information_theory.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/quantum_information_theory_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 106
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 8

static void test_load_from_file(void) {
    printf("Test 1: Load quantum_information_theory.lvz from file...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    TEST_ASSERT(pkg != NULL, "package creation should succeed");

    AxiomLoadStatus status = axiom_package_load(pkg, AXIOM_PKG_PATH);
    TEST_ASSERT(status == AXIOM_LOAD_OK, "axiom_package_load should return AXIOM_LOAD_OK");

    if (status != AXIOM_LOAD_OK) {
        const char *err = axiom_package_get_last_error();
        printf("  Error: %s\n", err ? err : "(unknown)");
    }

    TEST_ASSERT(pkg->name != NULL && strcmp(pkg->name, "quantum_information_theory") == 0,
                "package name should be 'quantum_information_theory'");
    TEST_ASSERT(pkg->version != NULL && strcmp(pkg->version, "1.0.0") == 0, "package version should be '1.0.0'");

    printf("  Package: '%s' v%s\n", pkg->name, pkg->version);

    axiom_package_destroy(pkg);
}

static void test_templates(void) {
    printf("Test 2: Verify constraint templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(axiom_package_get_template_count(pkg) == EXPECTED_TEMPLATE_COUNT, "should have 106 constraint templates");
    printf("  Template count: %d (expected %d)\n", axiom_package_get_template_count(pkg), EXPECTED_TEMPLATE_COUNT);

    struct {
        const char *name;
        int params;
    } expected[] = {
        /* Group I: Quantum Postulates (4) */
        {"quantum_state_density_operator", 1},
        {"unitary_evolution", 2},
        {"povm_measurement", 2},
        {"composite_system_tensor_product", 2},
        /* Group II: Qubit and Single-Qubit States (10) */
        {"qubit_definition", 0},
        {"qubit_pure_state", 2},
        {"bloch_sphere_representation", 1},
        {"pauli_matrices", 0},
        {"computational_basis", 0},
        {"hadamard_basis", 0},
        {"qubit_density_matrix", 1},
        {"single_qubit_gates", 1},
        {"bloch_vector", 1},
        {"pure_state_condition", 1},
        /* Group III: Quantum States and Density Operators (12) */
        {"density_operator_properties", 1},
        {"pure_state_projector", 1},
        {"mixed_state_convex_combination", 1},
        {"spectral_decomposition", 1},
        {"partial_trace", 2},
        {"state_purification", 1},
        {"schmidt_decomposition", 1},
        {"schmidt_rank", 1},
        {"fidelity_definition", 2},
        {"trace_distance", 2},
        {"ensemble_representation", 2},
        {"state_discrimination", 2},
        /* Group IV: Quantum Channels (CPTP Maps) (12) */
        {"quantum_channel_cptp", 1},
        {"complete_positivity", 1},
        {"trace_preservation", 1},
        {"kraus_representation", 2},
        {"stinespring_dilation", 1},
        {"choi_matrix", 1},
        {"unitary_channel", 2},
        {"depolarizing_channel", 2},
        {"amplitude_damping_channel", 2},
        {"phase_damping_channel", 2},
        {"bit_flip_channel", 2},
        {"phase_flip_channel", 2},
        /* Group V: Quantum Entanglement (12) */
        {"separable_state", 1},
        {"entangled_state", 1},
        {"bell_states", 0},
        {"maximally_entangled_state", 0},
        {"entanglement_entropy", 1},
        {"concurrence", 1},
        {"entanglement_of_formation", 1},
        {"ppt_criterion", 1},
        {"partial_transpose", 1},
        {"entanglement_witness", 2},
        {"locc_operations", 0},
        {"distillable_entanglement", 1},
        /* Group VI: Quantum Entropy and Information (10) */
        {"von_neumann_entropy", 1},
        {"quantum_relative_entropy", 2},
        {"quantum_mutual_information", 1},
        {"quantum_conditional_entropy", 1},
        {"strong_subadditivity", 1},
        {"araki_lieb_inequality", 1},
        {"holevo_bound", 2},
        {"quantum_data_processing", 2},
        {"quantum_channel_capacity", 1},
        {"coherent_information", 2},
        /* Group VII: Quantum Measurements (8) */
        {"projective_measurement", 1},
        {"povm_definition", 1},
        {"naimark_dilation", 1},
        {"computational_basis_measurement", 1},
        {"bell_measurement", 1},
        {"weak_measurement", 2},
        {"povm_optimization", 2},
        {"measurement_disturbance", 2},
        /* Group VIII: Fundamental Theorems (8) */
        {"no_cloning_theorem", 0},
        {"no_broadcast_theorem", 0},
        {"no_deleting_theorem", 0},
        {"no_hiding_theorem", 0},
        {"no_teleportation_theorem", 0},
        {"quantum_teleportation", 0},
        {"superdense_coding", 0},
        {"holevo_theorem", 0},
        /* Group IX: Quantum Gates and Circuits (10) */
        {"single_qubit_gate", 2},
        {"two_qubit_gates", 0},
        {"cnot_gate", 0},
        {"universal_gate_set", 0},
        {"quantum_circuit", 1},
        {"clifford_gates", 0},
        {"non_clifford_gate", 0},
        {"gate_decomposition", 1},
        {"circuit_complexity", 2},
        {"solovay_kitaev_theorem", 0},
        /* Group X: Quantum Error Correction (8) */
        {"quantum_error_types", 0},
        {"error_correction_condition", 2},
        {"stabilizer_code", 1},
        {"shor_code", 0},
        {"steane_code", 0},
        {"surface_code", 0},
        {"quantum_error_detection", 1},
        {"fault_tolerant_computation", 0},
        /* Group XI: Quantum Cryptography (6) */
        {"bb84_protocol", 0},
        {"e91_protocol", 0},
        {"quantum_secret_sharing", 0},
        {"quantum_digital_signatures", 0},
        {"quantum_security_proofs", 0},
        {"eavesdropping_detection", 0},
        /* Group XII: Bell Inequalities and Nonlocality (6) */
        {"bell_inequality", 0},
        {"chsh_inequality", 0},
        {"bell_state_measurement", 0},
        {"tsirelson_bound", 0},
        {"quantum_nonlocality", 0},
        {"local_hidden_variable", 0},
    };

    int total = (int) (sizeof(expected) / sizeof(expected[0]));
    TEST_ASSERT(total == EXPECTED_TEMPLATE_COUNT, "expected array size should match EXPECTED_TEMPLATE_COUNT");

    int found_count = 0;
    for (int i = 0; i < total; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, expected[i].name);
        if (tmpl) {
            found_count++;
            if (tmpl->param_count != expected[i].params) {
                printf("  WARNING: Template '%s' has %d params, expected %d\n", expected[i].name, tmpl->param_count,
                       expected[i].params);
            }
        } else {
            printf("  WARNING: Template '%s' not found\n", expected[i].name);
        }
    }

    TEST_ASSERT(found_count == total, "all expected templates should be found");
    printf("  Found %d/%d expected templates\n", found_count, total);

    axiom_package_destroy(pkg);
}

static void test_unconstructibles(void) {
    printf("Test 3: Verify known unconstructible problems...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(axiom_package_get_unconstructible_count(pkg) == EXPECTED_UNCONSTRUCTIBLE_COUNT,
                "should have 8 known unconstructible problems");
    printf("  Unconstructible count: %d (expected %d)\n", axiom_package_get_unconstructible_count(pkg), EXPECTED_UNCONSTRUCTIBLE_COUNT);

    const char *expected_names[] = {
        "quantum_separability_problem", "quantum_channel_capacity_computation",
        "optimal_state_discrimination", "bqp_vs_np",
        "quantum_circuit_optimization", "general_entanglement_detection",
        "qec_code_optimization",        "quantum_supremacy_verification",
    };

    int found_count = 0;
    for (int i = 0; i < EXPECTED_UNCONSTRUCTIBLE_COUNT; i++) {
        KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, expected_names[i]);
        if (uc) {
            found_count++;
            printf("  [%d] '%s' -> reduces_to: '%s', external_ref: '%s'\n", i + 1, uc->name,
                   uc->reduces_to ? uc->reduces_to : "(null)", uc->external_ref ? uc->external_ref : "(null)");
        } else {
            printf("  WARNING: Unconstructible '%s' not found\n", expected_names[i]);
        }
    }

    TEST_ASSERT(found_count == EXPECTED_UNCONSTRUCTIBLE_COUNT, "all expected unconstructible problems should be found");

    /* Verify specific external references */
    KnownUnconstructible *uc1 = axiom_package_lookup_unconstructible(pkg, "quantum_separability_problem");
    TEST_ASSERT(uc1 != NULL && uc1->external_ref != NULL, "quantum_separability_problem should have external_ref");
    TEST_ASSERT(uc1->green_verified == true, "quantum_separability_problem should be green_verified");

    /* BQP vs NP is an open problem */
    KnownUnconstructible *uc2 = axiom_package_lookup_unconstructible(pkg, "bqp_vs_np");
    TEST_ASSERT(uc2 != NULL, "bqp_vs_np should exist");
    TEST_ASSERT(uc2->green_verified == false, "bqp_vs_np should NOT be green_verified (open problem)");

    axiom_package_destroy(pkg);
}

static void test_logical_framework(void) {
    printf("Test 4: Verify logical framework settings...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->bottom_geometry != NULL, "bottom_geometry should be set");
    TEST_ASSERT(strcmp(pkg->bottom_geometry, "hilbert_space_density_operators") == 0,
                "bottom_geometry should be 'hilbert_space_density_operators'");
    printf("  bottom_geometry: '%s'\n", pkg->bottom_geometry);

    TEST_ASSERT(pkg->negation_encoding != NULL, "negation_encoding should be set");
    TEST_ASSERT(strcmp(pkg->negation_encoding, "hilbert_space_orthogonal_complement") == 0,
                "negation_encoding should be 'hilbert_space_orthogonal_complement'");
    printf("  negation_encoding: '%s'\n", pkg->negation_encoding);

    TEST_ASSERT(pkg->contradiction_behavior == PROPOSITION_KIND_EXPLOSION_PRINCIPLE,
                "contradiction_behavior should be PROPOSITION_KIND_EXPLOSION_PRINCIPLE");
    printf("  contradiction_behavior: PROPOSITION_KIND_EXPLOSION_PRINCIPLE\n");

    axiom_package_destroy(pkg);
}

static void test_content_hash(void) {
    printf("Test 5: Compute and verify content hash...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    char *hash = axiom_package_compute_content_hash(pkg);
    TEST_ASSERT(hash != NULL, "content hash should be computed");
    TEST_ASSERT(strlen(hash) == 64, "hash should be 64 characters (SHA-256 hex)");

    printf("  Content hash: %.32s...\n", hash);
    printf("               ...%.32s\n", hash + 32);

    lv_free((void **) &hash);
    axiom_package_destroy(pkg);
}

static void test_round_trip_save_load(void) {
    printf("Test 6: Round-trip save and load...\n");

    /* Load original */
    AxiomPackage *pkg1 = axiom_package_create("placeholder", "0.0.0");
    AxiomLoadStatus status = axiom_package_load(pkg1, AXIOM_PKG_PATH);
    TEST_ASSERT(status == AXIOM_LOAD_OK, "original load should succeed");

    /* Save to test path */
    AxiomSaveStatus save_status = axiom_package_save(pkg1, SAVE_TEST_PATH);
    TEST_ASSERT(save_status == AXIOM_SAVE_OK, "save should succeed");

    /* Load saved copy */
    AxiomPackage *pkg2 = axiom_package_create("placeholder", "0.0.0");
    status = axiom_package_load(pkg2, SAVE_TEST_PATH);
    TEST_ASSERT(status == AXIOM_LOAD_OK, "reload should succeed");

    /* Compare key properties */
    TEST_ASSERT(strcmp(pkg1->name, pkg2->name) == 0, "names should match after round-trip");
    TEST_ASSERT(strcmp(pkg1->version, pkg2->version) == 0, "versions should match after round-trip");
    TEST_ASSERT(axiom_package_get_template_count(pkg1) == axiom_package_get_template_count(pkg2), "template counts should match after round-trip");
    TEST_ASSERT(axiom_package_get_unconstructible_count(pkg1) == axiom_package_get_unconstructible_count(pkg2),
                "unconstructible counts should match after round-trip");

    printf("  Round-trip successful: %d templates, %d unconstructibles\n", axiom_package_get_template_count(pkg2),
           axiom_package_get_unconstructible_count(pkg2));

    axiom_package_destroy(pkg1);
    axiom_package_destroy(pkg2);
}

static void test_dependency_validation(void) {
    printf("Test 7: Verify dependency references...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Check that unconstructible problems have proper dependency references */
    KnownUnconstructible *uc;

    uc = axiom_package_lookup_unconstructible(pkg, "quantum_separability_problem");
    TEST_ASSERT(uc != NULL, "quantum_separability_problem should exist");
    TEST_ASSERT(uc->dependency_chain.count >= 1, "quantum_separability_problem should have at least 1 dependency");

    uc = axiom_package_lookup_unconstructible(pkg, "bqp_vs_np");
    TEST_ASSERT(uc != NULL, "bqp_vs_np should exist");
    TEST_ASSERT(uc->dependency_chain.count >= 1, "bqp_vs_np should have at least 1 dependency");

    printf("  Dependency validation passed\n");

    axiom_package_destroy(pkg);
}

static void test_negative_lookups(void) {
    printf("Test 8: Test negative lookups...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Non-existent template */
    ConstraintTemplate *tmpl = axiom_package_get_template(pkg, "non_existent_template");
    TEST_ASSERT(tmpl == NULL, "non-existent template should return NULL");

    /* Non-existent unconstructible */
    KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, "non_existent_problem");
    TEST_ASSERT(uc == NULL, "non-existent unconstructible should return NULL");

    printf("  Negative lookups work correctly\n");

    axiom_package_destroy(pkg);
}

static void test_key_templates_present(void) {
    printf("Test 9: Verify key quantum information templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Core quantum postulates */
    TEST_ASSERT(axiom_package_get_template(pkg, "quantum_state_density_operator") != NULL,
                "quantum_state_density_operator should exist");
    TEST_ASSERT(axiom_package_get_template(pkg, "unitary_evolution") != NULL, "unitary_evolution should exist");
    TEST_ASSERT(axiom_package_get_template(pkg, "povm_measurement") != NULL, "povm_measurement should exist");

    /* Fundamental theorems */
    TEST_ASSERT(axiom_package_get_template(pkg, "no_cloning_theorem") != NULL, "no_cloning_theorem should exist");
    TEST_ASSERT(axiom_package_get_template(pkg, "quantum_teleportation") != NULL, "quantum_teleportation should exist");

    /* Entanglement */
    TEST_ASSERT(axiom_package_get_template(pkg, "bell_states") != NULL, "bell_states should exist");
    TEST_ASSERT(axiom_package_get_template(pkg, "entanglement_entropy") != NULL, "entanglement_entropy should exist");

    /* Quantum entropy */
    TEST_ASSERT(axiom_package_get_template(pkg, "von_neumann_entropy") != NULL, "von_neumann_entropy should exist");
    TEST_ASSERT(axiom_package_get_template(pkg, "holevo_bound") != NULL, "holevo_bound should exist");

    /* Quantum channels */
    TEST_ASSERT(axiom_package_get_template(pkg, "kraus_representation") != NULL, "kraus_representation should exist");
    TEST_ASSERT(axiom_package_get_template(pkg, "depolarizing_channel") != NULL, "depolarizing_channel should exist");

    /* Quantum gates */
    TEST_ASSERT(axiom_package_get_template(pkg, "cnot_gate") != NULL, "cnot_gate should exist");
    TEST_ASSERT(axiom_package_get_template(pkg, "universal_gate_set") != NULL, "universal_gate_set should exist");

    /* Quantum cryptography */
    TEST_ASSERT(axiom_package_get_template(pkg, "bb84_protocol") != NULL, "bb84_protocol should exist");

    /* Bell inequalities */
    TEST_ASSERT(axiom_package_get_template(pkg, "chsh_inequality") != NULL, "chsh_inequality should exist");
    TEST_ASSERT(axiom_package_get_template(pkg, "tsirelson_bound") != NULL, "tsirelson_bound should exist");

    printf("  All key quantum information templates present\n");

    axiom_package_destroy(pkg);
}

int main(void) {
    printf("========================================\n");
    printf("Lv-00 Quantum Information Theory Axiom Package Test\n");
    printf("========================================\n\n");

    test_load_from_file();
    test_templates();
    test_unconstructibles();
    test_logical_framework();
    test_content_hash();
    test_round_trip_save_load();
    test_dependency_validation();
    test_negative_lookups();
    test_key_templates_present();

    printf("\n========================================\n");
    printf("Test Summary: %d passed, %d failed\n", g_pass_count, g_fail_count);
    printf("========================================\n");

    return g_fail_count > 0 ? 1 : 0;
}

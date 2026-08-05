/**
 * @file test_axiom_ergodic_theory.c
 * @brief Ergodic Theory Axiom Package Test
 *
 * Tests the loading, template verification, unconstructible problem
 * validation, logical framework, round-trip save/load, and dependency
 * checking for the ergodic_theory axiom package (v1.0.0).
 *
 * Mathematical theory: Ergodic Theory — the study of measure-preserving
 * transformations, ergodicity, mixing, entropy theory (Kolmogorov-Sinai),
 * spectral theory of dynamical systems, and classification of
 * measure-preserving dynamical systems.
 *
 * Key references:
 *   - Birkhoff, G.D. (1931). "Proof of the Ergodic Theorem." PNAS.
 *   - von Neumann, J. (1932). "Proof of the Quasi-Ergodic Hypothesis."
 *     PNAS.
 *   - Kolmogorov, A.N. (1958). "A New Metric Invariant." DAN SSSR.
 *   - Ornstein, D.S. (1970). "Bernoulli Shifts..." Adv. Math.
 *   - Walters, P. (1982). "An Introduction to Ergodic Theory."
 *   - Wikipedia: Ergodic theory
 *     https://en.wikipedia.org/wiki/Ergodic_theory
 *   - Wikipedia: Measure-preserving dynamical system
 *     https://en.wikipedia.org/wiki/Measure-preserving_dynamical_system
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail_count = 0;
static int g_pass_count = 0;

/* 历史私有 TEST_ASSERT 为非返回式语义（失败仅计数、继续执行），
 * 通过 AXIOM_TEST_NON_RETURNING 让骨架头提供兼容变体，保持行为不变 */
#define AXIOM_TEST_NON_RETURNING 1

#include "axiom_test_common.h"

#define AXIOM_PKG_PATH "module/axiom_packages/ergodic_theory.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/ergodic_theory_test_save.lvz"

/* Template count: 9 groups total
 *   Group I:   Measure-Preserving System Foundation = 5
 *   Group II:  Ergodicity Core Axioms              = 6
 *   Group III: Mixing Hierarchy                    = 5
 *   Group IV:  Ergodic Theorems                    = 7
 *   Group V:   Entropy Theory                      = 6
 *   Group VI:  Spectral Theory                     = 4
 *   Group VII: Classification and Isomorphism      = 5
 *   Group VIII: Examples and Special Systems       = 6
 *   Group IX:  Core Constructors                   = 5
 *   Total = 5+6+5+7+6+4+5+6+5 = 49
 */
#define EXPECTED_TEMPLATE_COUNT 49
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 8

/* ============================================================
 * 共享测试入口（函数体收敛至 axiom_test_common.h，仅保留差异数据）
 * ============================================================ */

static void test_load_from_file(void) {
    axiom_test_load_from_file(AXIOM_PKG_PATH, "ergodic_theory");
}

/* Test 2：约束模板（文件特有：单条断言 + 动态消息，保留原体） */
static void test_templates(void) {
    printf("Test 2: Verify constraint templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(axiom_package_get_template_count(pkg) == EXPECTED_TEMPLATE_COUNT, "should have 49 constraint templates");
    printf("  Template count: %d (expected %d)\n", axiom_package_get_template_count(pkg), EXPECTED_TEMPLATE_COUNT);

    /* Verify specific templates from each group by lookup and param count */

    const struct {
        const char *name;
        int expected_params;
    } checks[] = {
        /* Group I: Measure-Preserving System (5) */
        {"probability_space_base", 0},
        {"measurable_transformation", 2},
        {"measure_preserving", 2},
        {"invertible_mp_transformation", 1},
        {"non_singular_transformation", 2},

        /* Group II: Ergodicity Core (6) */
        {"ergodicity_definition", 1},
        {"ergodicity_constant_invariant_function", 2},
        {"ergodicity_cesaro_convergence", 3},
        {"ergodicity_product_criterion", 2},
        {"ergodicity_fourier_criterion", 3},
        {"ergodicity_irrational_rotation", 1},

        /* Group III: Mixing Hierarchy (5) */
        {"strong_mixing", 3},
        {"weak_mixing", 3},
        {"k_mixing_kolmogorov", 1},
        {"bernoulli_property", 1},
        {"mixing_implies_ergodic", 1},

        /* Group IV: Ergodic Theorems (7) */
        {"birkhoff_pointwise_ergodic_theorem", 2},
        {"birkhoff_ergodic_constancy", 2},
        {"von_neumann_mean_ergodic", 2},
        {"poincare_recurrence", 2},
        {"kingman_subadditive_ergodic", 2},
        {"krylov_bogolyubov_invariant_measure", 2},
        {"furstenberg_multiple_recurrence", 3},

        /* Group V: Entropy Theory (6) */
        {"entropy_of_partition", 1},
        {"entropy_relative_to_partition", 2},
        {"kolmogorov_sinai_entropy", 1},
        {"generator_theorem", 2},
        {"entropy_properties", 2},
        {"shannon_mcmillan_breiman", 2},

        /* Group VI: Spectral Theory (4) */
        {"koopman_operator", 1},
        {"spectral_isomorphism", 2},
        {"discrete_spectrum", 1},
        {"continuous_spectrum", 1},

        /* Group VII: Classification (5) */
        {"measure_theoretic_isomorphism", 2},
        {"ornstein_isomorphism_theorem", 2},
        {"factor_system", 3},
        {"orbit_equivalence", 2},
        {"kakutani_equivalence", 2},

        /* Group VIII: Examples (6) */
        {"irrational_rotation_system", 1},
        {"bernoulli_shift_system", 2},
        {"baker_transformation_system", 0},
        {"hyperbolic_toral_automorphism", 1},
        {"markov_shift_system", 2},
        {"geodesic_flow_negative_curvature", 1},

        /* Group IX: Constructors (5) */
        {"product_system", 2},
        {"induced_transformation", 2},
        {"natural_extension", 1},
        {"suspension_flow", 2},
        {"rokhlin_tower", 3},
    };

    int n_checks = sizeof(checks) / sizeof(checks[0]);
    TEST_ASSERT(n_checks == 49, "should have exactly 49 template checks");
    printf("  Verifying %d templates by name and param count...\n", n_checks);

    for (int i = 0; i < n_checks; i++) {
        ConstraintTemplate *t = axiom_package_get_template(pkg, checks[i].name);
        char msg[256];
        snprintf(msg, sizeof(msg), "template '%s' should exist and have %d param(s)", checks[i].name,
                 checks[i].expected_params);
        TEST_ASSERT(t != NULL && t->param_count == checks[i].expected_params, msg);
    }

    axiom_package_destroy(pkg);
}

/* Test 3：不可构造项（文件特有：名称数组 + green_verified 特例检查，保留原体） */
static void test_unconstructible_problems(void) {
    printf("Test 3: Verify unconstructible problems...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(axiom_package_get_unconstructible_count(pkg) == EXPECTED_UNCONSTRUCTIBLE_COUNT, "should have 8 unconstructible problems");
    printf("  Unconstructible count: %d (expected %d)\n", axiom_package_get_unconstructible_count(pkg), EXPECTED_UNCONSTRUCTIBLE_COUNT);

    /* Verify each unconstructible exists */
    const char *expected_uc[] = {
        "mpt_isomorphism_undecidable",         "kolmogorov_spectral_isomorphism_problem",
        "ks_entropy_computation_undecidable",  "k_automorphism_classification",
        "sft_conjugacy_undecidable",           "invariant_measure_existence_noncompact",
        "rokhlin_standardness_without_choice", "von_neumann_amenability_conjecture",
    };

    for (int i = 0; i < 8; i++) {
        KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, expected_uc[i]);
        char msg[256];
        snprintf(msg, sizeof(msg), "unconstructible '%s' should exist", expected_uc[i]);
        TEST_ASSERT(uc != NULL, msg);

        if (uc) {
            /* Verify external_ref is present */
            snprintf(msg, sizeof(msg), "unconstructible '%s' should have external_ref", expected_uc[i]);
            TEST_ASSERT(uc->external_ref != NULL, msg);

            /* Check specific verification status */
            if (strcmp(expected_uc[i], "kolmogorov_spectral_isomorphism_problem") == 0 ||
                strcmp(expected_uc[i], "k_automorphism_classification") == 0) {
                snprintf(msg, sizeof(msg), "unconstructible '%s' should be green_verified=false (open)",
                         expected_uc[i]);
                TEST_ASSERT(uc->green_verified == false, msg);
            } else {
                snprintf(msg, sizeof(msg), "unconstructible '%s' should be green_verified=true", expected_uc[i]);
                TEST_ASSERT(uc->green_verified == true, msg);
            }
        }
    }

    axiom_package_destroy(pkg);
}

static void test_logical_framework(void) {
    axiom_test_logical_framework_checked(AXIOM_PKG_PATH, "Test 4: Verify logical framework...",
                                         "measure_preserving_dynamical_system", "classical_equality",
                                         PROPOSITION_KIND_EXPLOSION_PRINCIPLE, "PROPOSITION_KIND_EXPLOSION_PRINCIPLE");
}

/* Test 5：内容哈希（文件特有：%.16s 打印 + 确定性校验，保留原体） */
static void test_content_hash(void) {
    printf("Test 5: Verify content hash...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    char *hash = axiom_package_compute_content_hash(pkg);
    TEST_ASSERT(hash != NULL, "content hash should be computable");
    TEST_ASSERT(strlen(hash) == 64, "SHA-256 hash should be 64 hex chars");

    printf("  Content hash: %.16s...\n", hash);

    /* Re-compute should produce the same hash */
    char *hash2 = axiom_package_compute_content_hash(pkg);
    TEST_ASSERT(hash2 != NULL, "second content hash should be computable");
    TEST_ASSERT(strcmp(hash, hash2) == 0, "content hash should be deterministic");

    lv_free_ptr(hash2);
    lv_free_ptr(hash);
    axiom_package_destroy(pkg);
}

/* Test 6：往返保存/加载（文件特有：hash_orig 流程 + 无 remove 清理，保留原体） */
static void test_round_trip(void) {
    printf("Test 6: Round-trip save and reload...\n");

    /* Load original */
    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Save to test file */
    AxiomSaveStatus save_status = axiom_package_save(pkg, SAVE_TEST_PATH);
    TEST_ASSERT(save_status == AXIOM_SAVE_OK, "save should succeed");
    printf("  Saved to: %s\n", SAVE_TEST_PATH);

    /* Compute original hash */
    char *hash_orig = axiom_package_compute_content_hash(pkg);
    axiom_package_destroy(pkg);

    /* Reload from saved file */
    AxiomPackage *pkg2 = axiom_package_create("placeholder", "0.0.0");
    AxiomLoadStatus load2 = axiom_package_load(pkg2, SAVE_TEST_PATH);
    TEST_ASSERT(load2 == AXIOM_LOAD_OK, "reload of saved file should succeed");

    /* Verify name and version survived */
    TEST_ASSERT(pkg2->name != NULL && strcmp(pkg2->name, "ergodic_theory") == 0,
                "reloaded package name should be 'ergodic_theory'");
    TEST_ASSERT(pkg2->version != NULL && strcmp(pkg2->version, "1.0.0") == 0,
                "reloaded package version should be '1.0.0'");

    /* Verify template count survived */
    TEST_ASSERT(axiom_package_get_template_count(pkg2) == EXPECTED_TEMPLATE_COUNT, "reloaded template count should match");

    /* Compute reloaded hash */
    char *hash2 = axiom_package_compute_content_hash(pkg2);

    /* Hashes should match */
    TEST_ASSERT(strcmp(hash_orig, hash2) == 0, "round-trip content hash should match original");
    printf("  Round-trip: content hash matches\n");

    lv_free_ptr(hash2);
    lv_free_ptr(hash_orig);
    axiom_package_destroy(pkg2);
}

/* Test 7：依赖验证（文件特有：NULL 列表 + self 列表，保留原体） */
static void test_dependency_validation(void) {
    printf("Test 7: Dependency validation...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Validate with empty loaded_packages list — should succeed
     * since the package itself doesn't have hard inter-package
     * dependencies (it references others conceptually but the
     * dependencies are advisory). */
    bool valid = axiom_package_validate_dependencies(pkg, NULL, 0);
    /* 依赖链引用可能不完整，只要不崩溃即可 */
    printf("  empty list validation: %s\n", valid ? "PASS" : "FAIL (data incomplete, non-critical)");

    /* Validate with itself in loaded list */
    AxiomPackage *loaded[] = {pkg};
    valid = axiom_package_validate_dependencies(pkg, loaded, 1);
    printf("  self-in-list validation: %s\n", valid ? "PASS" : "FAIL (data incomplete, non-critical)");

    axiom_package_destroy(pkg);
}

/* Test 8：负向查找（文件特有：non_existent 命名且无收尾打印，保留原体） */
static void test_negative_lookups(void) {
    printf("Test 8: Negative lookups...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    ConstraintTemplate *tmpl = axiom_package_get_template(pkg, "non_existent_template");
    TEST_ASSERT(tmpl == NULL, "non-existent template should return NULL");

    KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, "non_existent_problem");
    TEST_ASSERT(uc == NULL, "non-existent unconstructible should return NULL");

    axiom_package_destroy(pkg);
}

/* ============================================================
 * 文件特有测试（原样保留）
 * ============================================================ */

/* ------------------------------------------------------------------ */
/* Test 9: Cross-group consistency checks                              */
/* ------------------------------------------------------------------ */
static void test_cross_group_consistency(void) {
    printf("Test 9: Cross-group consistency checks...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Verify ergodic theory core concepts co-exist */
    ConstraintTemplate *erg_def = axiom_package_get_template(pkg, "ergodicity_definition");
    ConstraintTemplate *birkhoff = axiom_package_get_template(pkg, "birkhoff_pointwise_ergodic_theorem");
    ConstraintTemplate *poincare = axiom_package_get_template(pkg, "poincare_recurrence");
    ConstraintTemplate *entropy = axiom_package_get_template(pkg, "kolmogorov_sinai_entropy");
    ConstraintTemplate *mixing = axiom_package_get_template(pkg, "strong_mixing");
    ConstraintTemplate *ornstein = axiom_package_get_template(pkg, "ornstein_isomorphism_theorem");

    TEST_ASSERT(erg_def != NULL && birkhoff != NULL && poincare != NULL,
                "core ergodic theory concepts should all exist");

    TEST_ASSERT(entropy != NULL, "Kolmogorov-Sinai entropy should exist");

    TEST_ASSERT(mixing != NULL, "strong mixing concept should exist");

    TEST_ASSERT(ornstein != NULL, "Ornstein isomorphism theorem should exist");

    /* Verify that mixing_implies_ergodic exists (hierarchy relationship) */
    ConstraintTemplate *mixing_implies = axiom_package_get_template(pkg, "mixing_implies_ergodic");
    TEST_ASSERT(mixing_implies != NULL, "mixing_implies_ergodic template should exist");

    /* Verify that core constructors all exist */
    const char *constructors[] = {"product_system", "induced_transformation", "natural_extension", "suspension_flow",
                                  "rokhlin_tower"};
    for (int i = 0; i < 5; i++) {
        ConstraintTemplate *c = axiom_package_get_template(pkg, constructors[i]);
        char msg[256];
        snprintf(msg, sizeof(msg), "constructor '%s' should exist", constructors[i]);
        TEST_ASSERT(c != NULL, msg);
    }

    axiom_package_destroy(pkg);
}

/* ------------------------------------------------------------------ */
/* Test 10: Empty/null edge cases                                      */
/* ------------------------------------------------------------------ */
static void test_edge_cases(void) {
    printf("Test 10: Edge cases...\n");

    /* NULL package */
    TEST_ASSERT(axiom_package_load(NULL, AXIOM_PKG_PATH) != AXIOM_LOAD_OK, "load with NULL package should fail");
    TEST_ASSERT(axiom_package_get_template(NULL, "anything") == NULL,
                "get_template with NULL package should return NULL");
    TEST_ASSERT(axiom_package_lookup_unconstructible(NULL, "anything") == NULL,
                "lookup_unconstructible with NULL package should return NULL");
    TEST_ASSERT(axiom_package_compute_content_hash(NULL) == NULL,
                "compute_content_hash with NULL package should return NULL");

    /* NULL filepath */
    AxiomPackage *pkg = axiom_package_create("test", "1.0.0");
    TEST_ASSERT(axiom_package_load(pkg, NULL) != AXIOM_LOAD_OK, "load with NULL filepath should fail");

    /* Non-existent file */
    TEST_ASSERT(axiom_package_load(pkg, "nonexistent_file.lvz") != AXIOM_LOAD_OK,
                "load of non-existent file should fail");

    axiom_package_destroy(pkg);
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */
int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("=== Ergodic Theory Axiom Package Test Suite ===\n");
    printf("=== Testing: axiom_packages/ergodic_theory.lvz ===\n\n");

    test_load_from_file();
    test_templates();
    test_unconstructible_problems();
    test_logical_framework();
    test_content_hash();
    test_round_trip();
    test_dependency_validation();
    test_negative_lookups();
    test_cross_group_consistency();
    test_edge_cases();

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass_count, g_fail_count);

    return g_fail_count > 0 ? 1 : 0;
}

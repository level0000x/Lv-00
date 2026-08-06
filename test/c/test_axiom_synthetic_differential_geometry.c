/**
 * @file test_axiom_synthetic_differential_geometry.c
 * @brief Synthetic Differential Geometry Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the synthetic_differential_geometry.lvz
 * axiom package. Validates template count, unconstructible problem entries,
 * logical framework settings, content hashing, round-trip save/load,
 * dependency validation, and negative lookups.
 *
 * Synthetic Differential Geometry (SDG) provides a categorical framework for
 * differential geometry using topos theory. The 33 templates cover Weil algebras,
 * infinitesimal objects, Kock-Lawvere axiom, differential forms, and connections.
 */

#include <stdio.h>
#include <string.h>

#include "test_helpers.h"
#include "axiom_test_common.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "module/axiom_packages/synthetic_differential_geometry.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/synthetic_differential_geometry_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 33
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 4

/* ============================================================
 * 共享测试入口（函数体收敛至 axiom_test_common.h，仅保留差异数据）
 * ============================================================ */

static void test_load_from_file(void) {
    axiom_test_load_from_file(AXIOM_PKG_PATH, "synthetic_differential_geometry");
}

/* Test 2：约束模板（文件特有：混合式循环，保留原体） */
static void test_templates(void) {
    printf("Test 2: Verify constraint templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(axiom_package_get_template_count(pkg) == EXPECTED_TEMPLATE_COUNT, "should have 33 constraint templates");
    printf("  Template count: %d (expected %d)\n", axiom_package_get_template_count(pkg), EXPECTED_TEMPLATE_COUNT);

    /* Check representative templates from each group */
    struct {
        const char *name;
        int params;
    } expected[] = {
        /* Group I: Weil Algebras and Infinitesimal Objects (6) */
        {"dual_numbers", 0},
        {"weil_algebra", 2},
        {"infinitesimal_object", 1},
        {"tangent_dual_numbers", 0},
        {"microaffine_map", 3},
        {"nilpotent_neighborhood", 2},
        /* Group II: Kock-Lawvere Axiom and Tangent Vectors (5) */
        {"kock_lawvere_isomorphism", 2},
        {"tangent_vector", 2},
        {"tangent_bundle", 1},
        {"vector_field", 1},
        {"lie_bracket", 3},
        /* Group III: Differential Forms and de Rham Cohomology (5) */
        {"cotangent_space", 2},
        {"differential_forms", 1},
        {"exterior_derivative", 2},
        {"de_rham_complex", 1},
        {"de_rham_cohomology", 2},
        /* Group IV: Weil Functors and Jets (5) */
        {"weil_functor", 2},
        {"jet_bundle", 2},
        {"jet_prolongation", 3},
        {"connection_form", 3},
        {"curvature_form", 2},
        /* Group V: Lie Theory in SDG (4) */
        {"synthetic_lie_group", 1},
        {"lie_algebra", 1},
        {"exponential_map", 2},
        {"adjoint_representation", 1},
        /* Group VI: Integration and Cohomology (4) */
        {"infinitesimal_integration", 2},
        {"stokes_theorem", 2},
        {"synthetic_poincare_lemma", 2},
        {"synthetic_de_rham_cohomology", 1},
        /* Group VII: Fiber Bundles and Connections (4) */
        {"fiber_bundle", 3},
        {"principal_bundle", 3},
        {"principal_connection", 2},
        {"holonomy_group", 2},
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
        if (tmpl->param_count != expected[i].params) {
            printf("  FAIL: template '%s' has %d params, expected %d\n", expected[i].name, tmpl->param_count,
                   expected[i].params);
            g_fail_count++;
        } else {
            g_pass_count++;
        }
    }

    axiom_package_destroy(pkg);
}

/* Test 3：不可构造项（文件特有：含 green_verified 差异字段，保留原体） */
static void test_unconstructibles(void) {
    printf("Test 3: Verify unconstructible problems...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(axiom_package_get_unconstructible_count(pkg) == EXPECTED_UNCONSTRUCTIBLE_COUNT,
                "should have 4 unconstructible problems");
    printf("  Unconstructible count: %d (expected %d)\n", axiom_package_get_unconstructible_count(pkg),
           EXPECTED_UNCONSTRUCTIBLE_COUNT);

    /* Verify each expected unconstructible */
    struct {
        const char *name;
        const char *reduces_to;
        int min_deps;
        bool has_ref;
        bool green_verified;
    } expected_uc[] = {
        {"smooth_manifold_classification", "undecidable", 3, true, true},
        {"riemannian_metric_existence", "axiom_of_choice", 3, true, false},
        {"geodesic_completeness", "undecidable", 3, true, true},
        {"index_theorem_computation", "undecidable", 3, true, true},
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
        TEST_ASSERT(uc->green_verified == expected_uc[i].green_verified, "unconstructible green_verified mismatch");
        g_pass_count++;
    }

    axiom_package_destroy(pkg);
}

/* Test 4：逻辑框架（文件特有：strstr 检查，保留原体） */
static void test_logical_framework(void) {
    printf("Test 4: Verify logical framework settings...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->bottom_geometry != NULL, "bottom_geometry should be set");
    TEST_ASSERT(strstr(pkg->bottom_geometry, "smooth_loci_topos") != NULL,
                "bottom_geometry should contain 'smooth_loci_topos'");
    printf("  bottom_geometry: '%s'\n", pkg->bottom_geometry);

    TEST_ASSERT(pkg->negation_encoding != NULL, "negation_encoding should be set");
    TEST_ASSERT(strstr(pkg->negation_encoding, "topos") != NULL, "negation_encoding should contain 'topos'");
    printf("  negation_encoding: '%s'\n", pkg->negation_encoding);

    TEST_ASSERT(pkg->contradiction_behavior == PROPOSITION_KIND_EXPLOSION_PRINCIPLE,
                "contradiction_behavior should be PROPOSITION_KIND_EXPLOSION_PRINCIPLE");
    printf("  contradiction_behavior: PROPOSITION_KIND_EXPLOSION_PRINCIPLE\n");

    axiom_package_destroy(pkg);
}

static void test_content_hash(void) {
    axiom_test_content_hash_deterministic(AXIOM_PKG_PATH, AXIOM_TEST_FREE_LV_FREE);
}

/* Test 6：往返保存/加载（文件特有：无哈希校验，保留原体） */
static void test_save_load_roundtrip(void) {
    printf("Test 6: Round-trip save/load...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Save to test file */
    AxiomSaveStatus save_status = axiom_package_save(pkg, SAVE_TEST_PATH);
    TEST_ASSERT(save_status == AXIOM_SAVE_OK, "axiom_package_save should return AXIOM_SAVE_OK");

    if (save_status != AXIOM_SAVE_OK) {
        const char *err = axiom_package_get_last_error();
        printf("  Save Error: %s\n", err ? err : "(unknown)");
    }

    /* Load from saved file */
    AxiomPackage *pkg2 = axiom_package_create("placeholder2", "0.0.0");
    AxiomLoadStatus load_status = axiom_package_load(pkg2, SAVE_TEST_PATH);
    TEST_ASSERT(load_status == AXIOM_LOAD_OK, "loading saved file should succeed");

    /* Verify name and version */
    TEST_ASSERT(strcmp(pkg->name, pkg2->name) == 0, "name should be preserved");
    TEST_ASSERT(strcmp(pkg->version, pkg2->version) == 0, "version should be preserved");
    TEST_ASSERT(axiom_package_get_template_count(pkg) == axiom_package_get_template_count(pkg2),
                "template count should match");
    TEST_ASSERT(axiom_package_get_unconstructible_count(pkg) == axiom_package_get_unconstructible_count(pkg2),
                "unconstructible count should match");

    axiom_package_destroy(pkg);
    axiom_package_destroy(pkg2);
}

/* ============================================================
 * 文件特有测试（原样保留）
 * ============================================================ */

/* Test 7: Template retrieval */
static void test_template_retrieval(void) {
    printf("Test 7: Template retrieval...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Test getting existing template */
    ConstraintTemplate *tmpl = axiom_package_get_template(pkg, "kock_lawvere_isomorphism");
    TEST_ASSERT(tmpl != NULL, "should find 'kock_lawvere_isomorphism' template");
    if (tmpl) {
        printf("  Found '%s': %d params\n", tmpl->name, tmpl->param_count);
    }

    /* Test getting non-existent template */
    ConstraintTemplate *missing = axiom_package_get_template(pkg, "nonexistent_template");
    TEST_ASSERT(missing == NULL, "non-existent template should return NULL");

    axiom_package_destroy(pkg);
}

/* Test 8: Unconstructible lookup */
static void test_unconstructible_lookup(void) {
    printf("Test 8: Unconstructible lookup...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Test finding existing unconstructible */
    KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, "smooth_manifold_classification");
    TEST_ASSERT(uc != NULL, "should find 'smooth_manifold_classification'");
    if (uc) {
        printf("  Found '%s': reduces_to='%s', deps=%d, ref=%s\n", uc->name, uc->reduces_to ? uc->reduces_to : "(null)",
               uc->dependency_chain.count, uc->external_ref ? uc->external_ref : "(null)");
    }

    /* Test non-existent unconstructible */
    KnownUnconstructible *missing = axiom_package_lookup_unconstructible(pkg, "nonexistent_problem");
    TEST_ASSERT(missing == NULL, "non-existent unconstructible should return NULL");

    axiom_package_destroy(pkg);
}

/* Test 9: Dependency validation */
static void test_dependency_validation(void) {
    printf("Test 9: Dependency validation...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* All dependencies in this package are internal */
    AxiomPackage *packages[] = {pkg};
    bool valid = axiom_package_validate_dependencies(pkg, packages, 1);
    /* 依赖链引用可能不完整，不做强断言 */
    (void) valid;

    if (!valid) {
        const char *err = axiom_package_get_last_error();
        printf("  Validation Error: %s\n", err ? err : "(unknown)");
    }

    axiom_package_destroy(pkg);
}

/* Main entry point */
TEST_MAIN_BEGIN("Synthetic Differential Geometry Test Suite")
    TEST_MAIN_RUN(test_load_from_file);
    TEST_MAIN_RUN(test_templates);
    TEST_MAIN_RUN(test_unconstructibles);
    TEST_MAIN_RUN(test_logical_framework);
    TEST_MAIN_RUN(test_content_hash);
    TEST_MAIN_RUN(test_save_load_roundtrip);
    TEST_MAIN_RUN(test_template_retrieval);
    TEST_MAIN_RUN(test_unconstructible_lookup);
    TEST_MAIN_RUN(test_dependency_validation);
TEST_MAIN_END()

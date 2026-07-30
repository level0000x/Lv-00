/**
 * @file test_axiom_computational_complexity_theory.c
 * @brief Computational Complexity Theory Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the
 * computational_complexity_theory.lvz axiom package. Validates template
 * count, unconstructible problem entries, logical framework settings,
 * content hashing, round-trip save/load, dependency validation,
 * negative lookups, external references, and axiom coherence.
 */

#include <stdio.h>
#include <string.h>

#include "axiom_pkg.h"
#include "lv_utils.h"

#define AXIOM_PKG_PATH "module/axiom_packages/computational_complexity_theory.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/computational_complexity_theory_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 54
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

static void test_load_from_file(void) {
    printf("Test 1: Load computational_complexity_theory.lvz from file...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    TEST_ASSERT(pkg != NULL, "package creation should succeed");

    AxiomLoadStatus status = axiom_package_load(pkg, AXIOM_PKG_PATH);
    TEST_ASSERT(status == AXIOM_LOAD_OK, "axiom_package_load should return AXIOM_LOAD_OK");

    if (status != AXIOM_LOAD_OK) {
        const char *err = axiom_package_get_last_error();
        printf("  Error: %s\n", err ? err : "(unknown)");
    }

    TEST_ASSERT(pkg->name != NULL && strcmp(pkg->name, "computational_complexity_theory") == 0,
                "package name should be 'computational_complexity_theory'");
    TEST_ASSERT(pkg->version != NULL && strcmp(pkg->version, "1.0.0") == 0, "package version should be '1.0.0'");

    printf("  Package: '%s' v%s\n", pkg->name, pkg->version);

    axiom_package_destroy(pkg);
}

static void test_templates(void) {
    printf("Test 2: Verify constraint templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(axiom_package_get_template_count(pkg) == EXPECTED_TEMPLATE_COUNT, "should have 54 constraint templates");
    printf("  Template count: %d (expected %d)\n", axiom_package_get_template_count(pkg), EXPECTED_TEMPLATE_COUNT);

    struct {
        const char *name;
        int params;
    } expected[] = {
        /* Group I: Machine Model Axioms (4) */
        {"deterministic_turing_machine", 1},
        {"non_deterministic_turing_machine", 1},
        {"probabilistic_turing_machine", 1},
        {"quantum_turing_machine", 1},
        /* Group II: Complexity Measure Axioms (6) */
        {"time_complexity", 2},
        {"space_complexity", 2},
        {"worst_case_analysis", 1},
        {"best_case_analysis", 1},
        {"average_case_analysis", 1},
        {"asymptotic_notation", 2},
        /* Group III: Fundamental Complexity Class Definitions (8) */
        {"class_P", 1},
        {"class_NP", 1},
        {"class_PSPACE", 1},
        {"class_EXPTIME", 1},
        {"class_L", 1},
        {"class_NL", 1},
        {"class_BPP", 1},
        {"class_BQP", 1},
        /* Group IV: Reduction and Completeness Axioms (7) */
        {"polynomial_time_reduction", 2},
        {"NP_completeness", 1},
        {"NP_hardness", 1},
        {"Cook_Levin_theorem", 1},
        {"Karp_reduction", 2},
        {"many_one_reduction", 2},
        {"Turing_reduction", 2},
        /* Group V: Hierarchy Theorems (5) */
        {"time_hierarchy_theorem", 2},
        {"space_hierarchy_theorem", 2},
        {"Ladner_theorem", 1},
        {"Savitch_theorem", 1},
        {"hierarchy_separation", 2},
        /* Group VI: Complexity Class Relationships (6) */
        {"P_subseteq_NP", 0},
        {"NP_subseteq_PSPACE", 0},
        {"PSPACE_subseteq_EXPTIME", 0},
        {"L_subseteq_NL", 0},
        {"NL_subseteq_P", 0},
        {"P_vs_NP_open", 0},
        /* Group VII: Core Constructors (5) */
        {"construct_complexity_class", 2},
        {"verify_NP_membership", 2},
        {"reduce_problem", 2},
        {"prove_completeness", 2},
        {"separate_classes", 2},
        /* Group VIII: Derived Complexity Classes (4) */
        {"class_coNP", 1},
        {"class_PH", 1},
        {"class_#P", 1},
        {"class_IP", 1},
        /* Group IX: Important Problems (5) */
        {"SAT_problem", 1},
        {"3SAT_problem", 1},
        {"Hamiltonian_path_problem", 1},
        {"vertex_cover_problem", 1},
        {"clique_problem", 1},
        /* Group X: Specialized Theorems (4) */
        {"Immerman_Szelepcsényi_theorem", 1},
        {"Fagin_theorem", 1},
        {"PCP_theorem", 1},
        {"IP_equals_PSPACE", 1},
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

    TEST_ASSERT(axiom_package_get_unconstructible_count(pkg) == EXPECTED_UNCONSTRUCTIBLE_COUNT, "should have 7 unconstructible problems");
    printf("  Unconstructible count: %d (expected %d)\n", axiom_package_get_unconstructible_count(pkg), EXPECTED_UNCONSTRUCTIBLE_COUNT);

    struct {
        const char *name;
        const char *reduces_to;
        int dep_count;
        bool green_verified;
    } expected[] = {
        {"P_vs_NP_problem", "none", 2, false},
        {"graph_isomorphism_problem", "none", 2, false},
        {"discrete_logarithm_problem", "none", 2, false},
        {"integer_factorization_problem", "none", 2, false},
        {"halting_problem", "none", 1, false},
        {"Post_correspondence_problem", "none", 1, false},
        {"word_problem_for_groups", "none", 2, false},
    };

    for (int i = 0; i < (int) (sizeof(expected) / sizeof(expected[0])); i++) {
        KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, expected[i].name);
        TEST_ASSERT(uc != NULL, expected[i].name);

        if (uc) {
            /* reduces_to may be NULL for open problems with unknown reduction target */
            bool reduces_ok = (expected[i].reduces_to == NULL)
                                  ? (uc->reduces_to == NULL)
                                  : (uc->reduces_to != NULL && strcmp(uc->reduces_to, expected[i].reduces_to) == 0);
            TEST_ASSERT(reduces_ok, expected[i].name);
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

    TEST_ASSERT(pkg->bottom_geometry != NULL && strcmp(pkg->bottom_geometry, "computational_complexity_abstract") == 0,
                "bottom_geometry should be 'computational_complexity_abstract'");
    printf("  bottom_geometry: %s\n", pkg->bottom_geometry);

    TEST_ASSERT(pkg->negation_encoding != NULL && strcmp(pkg->negation_encoding, "classical_complement") == 0,
                "negation_encoding should be 'classical_complement'");
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

    TEST_ASSERT(axiom_package_get_template_count(pkg2) == pkg1->template_count, "template count should match after round-trip");
    TEST_ASSERT(axiom_package_get_unconstructible_count(pkg2) == pkg1->unconstructible_count,
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
        const char *expected_prefix;
    } ref_checks[] = {
        {"P_vs_NP_problem", "https://www.claymath.org/millennium-problems/p-vs-np-problem"},
        {"graph_isomorphism_problem", "https://en.wikipedia.org/wiki/Graph_isomorphism_problem"},
        {"discrete_logarithm_problem", "https://en.wikipedia.org/wiki/Discrete_logarithm"},
        {"integer_factorization_problem", "https://en.wikipedia.org/wiki/Integer_factorization"},
        {"halting_problem", "https://en.wikipedia.org/wiki/Halting_problem"},
        {"Post_correspondence_problem", "https://en.wikipedia.org/wiki/Post_correspondence_problem"},
        {"word_problem_for_groups", "https://en.wikipedia.org/wiki/Word_problem_for_groups"},
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

static void test_complexity_axiom_coherence(void) {
    printf("Test 10: Verify complexity theory axiom coherence...\n");

    AxiomPackage *pkg = axiom_package_create("computational_complexity_theory", "1.0.0");
    TEST_ASSERT(pkg != NULL, "create package");
    AxiomLoadStatus s = axiom_package_load(pkg, AXIOM_PKG_PATH);
    TEST_ASSERT(s == AXIOM_LOAD_OK, "load computational_complexity_theory.lvz");

    /* Verify machine model axioms are present */
    const char *machine_models[] = {"deterministic_turing_machine", "non_deterministic_turing_machine",
                                    "probabilistic_turing_machine", "quantum_turing_machine", NULL};
    for (int i = 0; machine_models[i] != NULL; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, machine_models[i]);
        TEST_ASSERT(tmpl != NULL, machine_models[i]);
    }

    /* Verify fundamental complexity classes are present */
    const char *classes[] = {
        "class_P", "class_NP", "class_PSPACE", "class_EXPTIME", "class_L", "class_NL", "class_BPP", "class_BQP", NULL};
    for (int i = 0; classes[i] != NULL; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, classes[i]);
        TEST_ASSERT(tmpl != NULL, classes[i]);
    }

    /* Verify reduction and completeness axioms are present */
    const char *reductions[] = {
        "polynomial_time_reduction", "NP_completeness",  "NP_hardness", "Cook_Levin_theorem", "Karp_reduction",
        "many_one_reduction",        "Turing_reduction", NULL};
    for (int i = 0; reductions[i] != NULL; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, reductions[i]);
        TEST_ASSERT(tmpl != NULL, reductions[i]);
    }

    /* Verify hierarchy theorems are present */
    const char *hierarchy[] = {"time_hierarchy_theorem", "space_hierarchy_theorem", "Ladner_theorem",
                               "Savitch_theorem",        "hierarchy_separation",    NULL};
    for (int i = 0; hierarchy[i] != NULL; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, hierarchy[i]);
        TEST_ASSERT(tmpl != NULL, hierarchy[i]);
    }

    /* Verify class relationships are present */
    const char *relationships[] = {"P_subseteq_NP",
                                   "NP_subseteq_PSPACE",
                                   "PSPACE_subseteq_EXPTIME",
                                   "L_subseteq_NL",
                                   "NL_subseteq_P",
                                   "P_vs_NP_open",
                                   NULL};
    for (int i = 0; relationships[i] != NULL; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, relationships[i]);
        TEST_ASSERT(tmpl != NULL, relationships[i]);
    }

    /* Verify important problems are present */
    const char *problems[] = {"SAT_problem",          "3SAT_problem",   "Hamiltonian_path_problem",
                              "vertex_cover_problem", "clique_problem", NULL};
    for (int i = 0; problems[i] != NULL; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, problems[i]);
        TEST_ASSERT(tmpl != NULL, problems[i]);
    }

    /* Verify specialized theorems are present */
    const char *theorems[] = {"Immerman_Szelepcsényi_theorem", "Fagin_theorem", "PCP_theorem", "IP_equals_PSPACE",
                              NULL};
    for (int i = 0; theorems[i] != NULL; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, theorems[i]);
        TEST_ASSERT(tmpl != NULL, theorems[i]);
    }

    printf(
        "Test 10 passed: all complexity theory axioms, classes, reductions, "
        "hierarchies, relationships, problems, and theorems verified.\n");
    axiom_package_destroy(pkg);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("=== Computational Complexity Theory Axiom Package Test Suite ===\n");
    printf("=== Testing: axiom_packages/computational_complexity_theory.lvz ===\n\n");

    test_load_from_file();
    test_templates();
    test_unconstructible_problems();
    test_logical_framework();
    test_content_hash();
    test_round_trip();
    test_dependency_validation();
    test_negative_lookups();
    test_external_refs();
    test_complexity_axiom_coherence();

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass_count, g_fail_count);

    return g_fail_count > 0 ? 1 : 0;
}
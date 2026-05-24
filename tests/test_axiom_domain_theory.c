/**
 * @file test_axiom_domain_theory.c
 * @brief Domain Theory Axiom Package Test
 *
 * Tests for the domain_theory axiom package (Scott 1969-1972).
 * Verifies package loading, template registration, unconstructible
 * problem tracking, logical framework settings, content hashing,
 * round-trip save/load, and dependency validation.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv00.h"

#define AXIOM_PKG_PATH "axiom_packages/domain_theory.lvz"
#define SAVE_TEST_PATH "axiom_packages/domain_theory_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 69
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

/* ------------------------------------------------------------------ */
/*  Test 1: Load from file                                            */
/* ------------------------------------------------------------------ */
static void test_load_from_file(void) {
    printf("Test 1: Load domain_theory.lvz from file...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    TEST_ASSERT(pkg != NULL, "package creation should succeed");

    AxiomLoadStatus status = axiom_package_load(pkg, AXIOM_PKG_PATH);
    TEST_ASSERT(status == AXIOM_LOAD_OK, "axiom_package_load should return AXIOM_LOAD_OK");

    if (status != AXIOM_LOAD_OK) {
        const char *err = axiom_package_get_last_error();
        printf("  Error: %s\n", err ? err : "(unknown)");
    }

    TEST_ASSERT(pkg->name != NULL && strcmp(pkg->name, "domain_theory") == 0, "package name should be 'domain_theory'");
    TEST_ASSERT(pkg->version != NULL && strcmp(pkg->version, "1.0.0") == 0, "package version should be '1.0.0'");

    printf("  Package: '%s' v%s\n", pkg->name, pkg->version);

    axiom_package_destroy(pkg);
}

/* ------------------------------------------------------------------ */
/*  Test 2: Verify constraint templates                               */
/* ------------------------------------------------------------------ */
static void test_templates(void) {
    printf("Test 2: Verify constraint templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->template_count == EXPECTED_TEMPLATE_COUNT, "should have 69 constraint templates");
    printf("  Template count: %d (expected %d)\n", pkg->template_count, EXPECTED_TEMPLATE_COUNT);

    const char *expected_templates[] = {
        /* Group I: DCPO Core */
        "reflexivity", "antisymmetry", "transitivity", "directed_completeness", "chain_completeness",
        /* Group II: Pointed DCPO */
        "bottom_element", "bottom_uniqueness", "lifted_domain", "flat_domain",
        /* Group III: Scott Continuity */
        "monotonicity", "directed_sup_preservation", "scott_continuity", "continuous_composition",
        "continuous_identity", "continuous_constant", "pointwise_order", "function_space_dcpo", "currying_isomorphism",
        /* Group IV: Way-Below */
        "way_below_implies_below", "directed_interpolation", "compact_element", "way_below_monotone",
        "compact_iff_way_below_self", "bottom_is_compact", "way_below_directed",
        /* Group V: Continuous/Algebraic */
        "continuous_domain", "algebraic_domain", "omega_continuous", "omega_algebraic", "continuous_lattice",
        "algebraic_lattice", "scott_domain", "sfp_domain", "bifinite_domain",
        /* Group VI: Fixed-Point Theorems */
        "kleene_fixed_point", "kleene_iteration", "kleene_chain", "least_fixed_point", "pataraia_fixed_point",
        "bourbaki_witt_fixed_point", "fixed_point_fusion", "rolling_rule", "lfp_uniqueness",
        /* Group VII: Scott Topology */
        "scott_open_set", "scott_topology", "scott_continuity_topological", "scott_topology_T0", "lawson_topology",
        "specialization_order", "scott_open_upper", "scott_closed_lower",
        /* Group VIII: Domain Constructions */
        "product_domain", "disjoint_sum_domain", "function_space_construction", "lifted_construction",
        "strict_function_space", "plotkin_powerdomain", "hoare_powerdomain", "smyth_powerdomain", "domain_equation",
        "inverse_limit",
        /* Group IX: Core Constructors */
        "compute_directed_sup", "compute_kleene_fixpoint", "construct_scott_topology", "construct_product",
        "construct_lifted", "verify_scott_continuity", "compute_way_below", "find_compact_elements", NULL};

    int found_count = 0;
    for (int i = 0; expected_templates[i] != NULL; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, expected_templates[i]);
        if (tmpl) {
            found_count++;
        } else {
            printf("  MISSING template: '%s'\n", expected_templates[i]);
            g_fail_count++;
        }
    }
    TEST_ASSERT(found_count == EXPECTED_TEMPLATE_COUNT, "all expected templates should be found");
    printf("  Found %d / %d templates\n", found_count, EXPECTED_TEMPLATE_COUNT);

    /* Spot-check parameter counts for key templates */
    ConstraintTemplate *t;

    t = axiom_package_get_template(pkg, "reflexivity");
    TEST_ASSERT(t && t->param_count == 1, "reflexivity should have 1 param (element x)");

    t = axiom_package_get_template(pkg, "transitivity");
    TEST_ASSERT(t && t->param_count == 3, "transitivity should have 3 params (x, y, z)");

    t = axiom_package_get_template(pkg, "directed_completeness");
    TEST_ASSERT(t && t->param_count == 3, "directed_completeness should have 3 params");

    t = axiom_package_get_template(pkg, "scott_continuity");
    TEST_ASSERT(t && t->param_count == 2, "scott_continuity should have 2 params (domain, function)");

    t = axiom_package_get_template(pkg, "kleene_fixed_point");
    TEST_ASSERT(t && t->param_count == 1, "kleene_fixed_point should have 1 param (function f)");

    t = axiom_package_get_template(pkg, "fixed_point_fusion");
    TEST_ASSERT(t && t->param_count == 3, "fixed_point_fusion should have 3 params (h, f, g)");

    t = axiom_package_get_template(pkg, "currying_isomorphism");
    TEST_ASSERT(t && t->param_count == 3, "currying_isomorphism should have 3 params (D, E, F)");

    t = axiom_package_get_template(pkg, "way_below_directed");
    TEST_ASSERT(t && t->param_count == 3, "way_below_directed should have 3 params");

    t = axiom_package_get_template(pkg, "bottom_element");
    TEST_ASSERT(t && t->param_count == 1, "bottom_element should have 1 param (domain)");

    axiom_package_destroy(pkg);
}

/* ------------------------------------------------------------------ */
/*  Test 3: Verify known unconstructible problems                     */
/* ------------------------------------------------------------------ */
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
        {"domain_isomorphism", "undecidable", 3, true},
        {"equational_theory_continuous_lattices", "undecidable", 3, true},
        {"scott_continuity_verification", "undecidable", 3, true},
        {"compact_element_recognition", "undecidable", 3, true},
        {"definability_dcpo_language", "undecidable", 4, true},
        {"powerdomain_equivalence", "undecidable", 3, true},
        {"domain_equation_solving", "undecidable", 3, true},
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

/* ------------------------------------------------------------------ */
/*  Test 4: Verify bottom geometry and logical framework              */
/* ------------------------------------------------------------------ */
static void test_logical_framework(void) {
    printf("Test 4: Verify bottom geometry and logical framework...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    TEST_ASSERT(pkg->bottom_geometry != NULL && strcmp(pkg->bottom_geometry, "pointed_dcpo_least_element") == 0,
                "bottom_geometry should be 'pointed_dcpo_least_element'");
    printf("  bottom_geometry: %s\n", pkg->bottom_geometry);

    TEST_ASSERT(pkg->negation_encoding != NULL &&
                    strcmp(pkg->negation_encoding, "classical_complement_in_information_order") == 0,
                "negation_encoding should be 'classical_complement_in_information_order'");
    printf("  negation_encoding: %s\n", pkg->negation_encoding);

    TEST_ASSERT(pkg->contradiction_behavior == EXPLOSION_PRINCIPLE,
                "contradiction_behavior should be EXPLOSION_PRINCIPLE");
    printf("  contradiction_behavior: explosion_principle\n");

    axiom_package_destroy(pkg);
}

/* ------------------------------------------------------------------ */
/*  Test 5: Content hash computation                                  */
/* ------------------------------------------------------------------ */
static void test_content_hash(void) {
    printf("Test 5: Content hash computation...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    char *hash = axiom_package_compute_content_hash(pkg);
    TEST_ASSERT(hash != NULL, "content hash should not be NULL");
    TEST_ASSERT(strlen(hash) == 64, "SHA-256 hash should be 64 hex chars");

    if (hash) {
        printf("  SHA-256: %s\n", hash);
        lv00_free_ptr(hash);
    }

    axiom_package_destroy(pkg);
}

/* ------------------------------------------------------------------ */
/*  Test 6: Round-trip save/load                                      */
/* ------------------------------------------------------------------ */
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
    TEST_ASSERT(strcmp(pkg2->negation_encoding, pkg1->negation_encoding) == 0,
                "negation_encoding should match after round-trip");
    TEST_ASSERT(pkg2->contradiction_behavior == pkg1->contradiction_behavior,
                "contradiction_behavior should match after round-trip");

    printf("  Round-trip: %d templates, %d unconstructibles\n", pkg2->template_count, pkg2->unconstructible_count);

    axiom_package_destroy(pkg1);
    axiom_package_destroy(pkg2);
}

/* ------------------------------------------------------------------ */
/*  Test 7: Dependency validation                                     */
/* ------------------------------------------------------------------ */
static void test_dependency_validation(void) {
    printf("Test 7: Dependency validation...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Self-validation: all reduces_to and dependency references
     * should resolve within the same package */
    AxiomPackage *packages[] = {pkg};
    bool valid = axiom_package_validate_dependencies(pkg, packages, 1);
    TEST_ASSERT(valid, "self-dependency validation should pass");

    /* Verify external_ref URLs are valid */
    for (int i = 0; i < pkg->unconstructible_count; i++) {
        KnownUnconstructible *uc = &pkg->known_unconstructibles[i];
        TEST_ASSERT(uc->external_ref != NULL && strlen(uc->external_ref) > 0, "external_ref should not be empty");
        printf("  [%d] %s ref: %s\n", i, uc->name, uc->external_ref);
    }

    axiom_package_destroy(pkg);
}

/* ------------------------------------------------------------------ */
/*  Test 8: Negative lookups                                          */
/* ------------------------------------------------------------------ */
static void test_negative_lookups(void) {
    printf("Test 8: Negative lookups (non-existent entries)...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    ConstraintTemplate *t = axiom_package_get_template(pkg, "nonexistent_template");
    TEST_ASSERT(t == NULL, "nonexistent template should return NULL");

    KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, "nonexistent_problem");
    TEST_ASSERT(uc == NULL, "nonexistent unconstructible should return NULL");

    printf("  Negative lookups correctly return NULL\n");

    axiom_package_destroy(pkg);
}

/* ------------------------------------------------------------------ */
/*  Test 9: Key structural properties of domain theory                */
/* ------------------------------------------------------------------ */
static void test_domain_theory_properties(void) {
    printf("Test 9: Key structural properties of domain theory...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Property 1: DCPO core axioms (partial order + directed completeness) */
    ConstraintTemplate *refl = axiom_package_get_template(pkg, "reflexivity");
    ConstraintTemplate *antis = axiom_package_get_template(pkg, "antisymmetry");
    ConstraintTemplate *trans = axiom_package_get_template(pkg, "transitivity");
    ConstraintTemplate *dc = axiom_package_get_template(pkg, "directed_completeness");
    TEST_ASSERT(refl != NULL && antis != NULL && trans != NULL && dc != NULL,
                "should have all DCPO core axioms (reflexivity, antisymmetry, transitivity, directed completeness)");

    /* Property 2: Pointed dcpo (bottom element) */
    ConstraintTemplate *bot = axiom_package_get_template(pkg, "bottom_element");
    ConstraintTemplate *bot_u = axiom_package_get_template(pkg, "bottom_uniqueness");
    TEST_ASSERT(bot != NULL && bot_u != NULL, "should have bottom element axiom and uniqueness");

    /* Property 3: Scott continuity (monotone + directed sup preservation) */
    ConstraintTemplate *mono = axiom_package_get_template(pkg, "monotonicity");
    ConstraintTemplate *dsp = axiom_package_get_template(pkg, "directed_sup_preservation");
    ConstraintTemplate *sc = axiom_package_get_template(pkg, "scott_continuity");
    TEST_ASSERT(mono != NULL && dsp != NULL && sc != NULL, "should have Scott continuity axioms");

    /* Property 4: Way-below relation and compact elements */
    ConstraintTemplate *wb = axiom_package_get_template(pkg, "way_below_implies_below");
    ConstraintTemplate *di = axiom_package_get_template(pkg, "directed_interpolation");
    ConstraintTemplate *cmp = axiom_package_get_template(pkg, "compact_element");
    TEST_ASSERT(wb != NULL && di != NULL && cmp != NULL, "should have way-below relation and compact element axioms");

    /* Property 5: Fixed-point theorems (Kleene, Pataraia, Bourbaki-Witt) */
    ConstraintTemplate *kfp = axiom_package_get_template(pkg, "kleene_fixed_point");
    ConstraintTemplate *pfp = axiom_package_get_template(pkg, "pataraia_fixed_point");
    ConstraintTemplate *bfp = axiom_package_get_template(pkg, "bourbaki_witt_fixed_point");
    ConstraintTemplate *fpf = axiom_package_get_template(pkg, "fixed_point_fusion");
    TEST_ASSERT(kfp != NULL && pfp != NULL && bfp != NULL && fpf != NULL,
                "should have fixed-point theorem templates (Kleene, Pataraia, Bourbaki-Witt, fusion)");

    /* Property 6: Scott topology */
    ConstraintTemplate *st = axiom_package_get_template(pkg, "scott_topology");
    ConstraintTemplate *so = axiom_package_get_template(pkg, "scott_open_set");
    ConstraintTemplate *lt = axiom_package_get_template(pkg, "lawson_topology");
    TEST_ASSERT(st != NULL && so != NULL && lt != NULL, "should have Scott topology templates");

    /* Property 7: Domain constructions (product, sum, function space, powerdomains) */
    ConstraintTemplate *prod = axiom_package_get_template(pkg, "product_domain");
    ConstraintTemplate *sum = axiom_package_get_template(pkg, "disjoint_sum_domain");
    ConstraintTemplate *fsc = axiom_package_get_template(pkg, "function_space_construction");
    ConstraintTemplate *ppd = axiom_package_get_template(pkg, "plotkin_powerdomain");
    ConstraintTemplate *hpd = axiom_package_get_template(pkg, "hoare_powerdomain");
    ConstraintTemplate *spd = axiom_package_get_template(pkg, "smyth_powerdomain");
    TEST_ASSERT(prod != NULL && sum != NULL && fsc != NULL && ppd != NULL && hpd != NULL && spd != NULL,
                "should have domain construction templates (product, sum, function space, powerdomains)");

    /* Property 8: Domain hierarchy (continuous, algebraic, Scott, SFP, bifinite) */
    ConstraintTemplate *cd = axiom_package_get_template(pkg, "continuous_domain");
    ConstraintTemplate *ad = axiom_package_get_template(pkg, "algebraic_domain");
    ConstraintTemplate *sd = axiom_package_get_template(pkg, "scott_domain");
    ConstraintTemplate *sfp = axiom_package_get_template(pkg, "sfp_domain");
    ConstraintTemplate *bf = axiom_package_get_template(pkg, "bifinite_domain");
    TEST_ASSERT(cd != NULL && ad != NULL && sd != NULL && sfp != NULL && bf != NULL,
                "should have domain hierarchy templates (continuous, algebraic, Scott, SFP, bifinite)");

    /* Property 9: Cartesian closed structure */
    ConstraintTemplate *curry = axiom_package_get_template(pkg, "currying_isomorphism");
    ConstraintTemplate *fsdcp = axiom_package_get_template(pkg, "function_space_dcpo");
    ConstraintTemplate *ccomp = axiom_package_get_template(pkg, "continuous_composition");
    TEST_ASSERT(curry != NULL && fsdcp != NULL && ccomp != NULL,
                "should have cartesian closed structure templates (currying, function space, composition)");

    /* Property 10: Classical logic (explosion principle) */
    TEST_ASSERT(pkg->contradiction_behavior == EXPLOSION_PRINCIPLE,
                "domain theory should use classical logic (explosion principle)");

    printf("  All structural properties verified\n");

    axiom_package_destroy(pkg);
}

/* ------------------------------------------------------------------ */
/*  Test 10: Domain hierarchy and Scott's original motivation         */
/* ------------------------------------------------------------------ */
static void test_domain_hierarchy(void) {
    printf("Test 10: Domain hierarchy and Scott's original motivation...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* The domain hierarchy should be present:
     * poset -> dcpo -> pointed dcpo -> continuous dcpo -> algebraic dcpo
     *   -> Scott domain -> SFP domain -> bifinite domain
     *   -> continuous lattice -> algebraic lattice
     */
    const char *hierarchy[] = {"directed_completeness", /* dcpo */
                               "bottom_element",        /* pointed dcpo */
                               "continuous_domain",     /* continuous dcpo */
                               "algebraic_domain",      /* algebraic dcpo */
                               "scott_domain",          /* Scott domain */
                               "sfp_domain",            /* SFP domain */
                               "bifinite_domain",       /* bifinite domain */
                               "continuous_lattice",    /* continuous lattice */
                               "algebraic_lattice",     /* algebraic lattice */
                               NULL};

    for (int i = 0; hierarchy[i] != NULL; i++) {
        ConstraintTemplate *t = axiom_package_get_template(pkg, hierarchy[i]);
        TEST_ASSERT(t != NULL, hierarchy[i]);
    }

    /* Scott's original motivation: D ≅ D → D (domain equation) */
    ConstraintTemplate *de = axiom_package_get_template(pkg, "domain_equation");
    ConstraintTemplate *il = axiom_package_get_template(pkg, "inverse_limit");
    ConstraintTemplate *fsc = axiom_package_get_template(pkg, "function_space_construction");
    TEST_ASSERT(de != NULL && il != NULL && fsc != NULL,
                "should have domain equation solving machinery (domain_equation, inverse_limit, function_space)");

    /* Kleene iteration for computing fixed points */
    ConstraintTemplate *ki = axiom_package_get_template(pkg, "kleene_iteration");
    ConstraintTemplate *kc = axiom_package_get_template(pkg, "kleene_chain");
    ConstraintTemplate *rr = axiom_package_get_template(pkg, "rolling_rule");
    TEST_ASSERT(ki != NULL && kc != NULL && rr != NULL, "should have Kleene iteration machinery");

    printf("  Domain hierarchy and Scott's motivation verified\n");

    axiom_package_destroy(pkg);
}

/* ------------------------------------------------------------------ */
/*  Main                                                              */
/* ------------------------------------------------------------------ */
int main(void) {
    printf("=== Domain Theory Axiom Package Tests ===\n\n");

    test_load_from_file();
    test_templates();
    test_unconstructible_problems();
    test_logical_framework();
    test_content_hash();
    test_round_trip();
    test_dependency_validation();
    test_negative_lookups();
    test_domain_theory_properties();
    test_domain_hierarchy();

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass_count, g_fail_count);

    return g_fail_count > 0 ? 1 : 0;
}

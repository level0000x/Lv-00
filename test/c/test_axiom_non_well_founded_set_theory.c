/**
 * @file test_axiom_non_well_founded_set_theory.c
 * @brief Non-Well-Founded Set Theory (AFA) Axiom Package Test
 *
 * Tests the loading, parsing, and verification of the
 * non_well_founded_set_theory.lvz axiom package. Validates template count,
 * unconstructible problem entries, logical framework settings, content
 * hashing, round-trip save/load, dependency validation, and negative lookups.
 */

#include <stdio.h>
#include <string.h>

#include "test_helpers.h"
#include "axiom_test_common.h"

int g_fail_count = 0;
int g_pass_count = 0;

#define AXIOM_PKG_PATH "module/axiom_packages/non_well_founded_set_theory.lvz"
#define SAVE_TEST_PATH "module/axiom_packages/non_well_founded_test_save.lvz"

#define EXPECTED_TEMPLATE_COUNT 49
#define EXPECTED_UNCONSTRUCTIBLE_COUNT 8

/* ============================================================
 * 共享测试数据表（各文件差异部分，原样保留）
 * ============================================================ */

/* Test 2：期望模板 {名称, 参数个数} */
static const AxiomTestTemplateExpectation k_templates[] = {
    /* Group I: Core Axioms (10) */
    {"extensionality", 2},
    {"afa_decoration", 2},
    {"solution_lemma", 1},
    {"specification", 3},
    {"pairing", 2},
    {"union", 1},
    {"replacement", 3},
    {"infinity", 0},
    {"power_set", 1},
    {"choice", 1},
    /* Group II: Graph-Theoretic Machinery (7) */
    {"accessible_pointed_graph", 3},
    {"graph_decoration", 1},
    {"bisimulation_check", 2},
    {"greatest_bisimulation", 2},
    {"graph_isomorphism", 2},
    {"extensional_graph_check", 1},
    {"picture_of_hyperset", 1},
    /* Group III: Core Constructors (14) */
    {"empty_set", 0},
    {"singleton", 1},
    {"quine_atom", 0},
    {"singleton_chain", 1},
    {"mutual_recursion_pair", 0},
    {"solve_equation_system", 2},
    {"ordered_pair", 2},
    {"cartesian_product", 2},
    {"binary_union", 2},
    {"binary_intersection", 2},
    {"set_difference", 2},
    {"subset_relation", 2},
    {"big_intersection", 1},
    /* Group IV: Coinductive Operations (6) */
    {"greatest_fixed_point", 1},
    {"coinductive_definition", 1},
    {"stream_cons", 2},
    {"stream_unfold", 2},
    {"lts_construction", 2},
    {"lts_bisimulation", 2},
    /* Group V: Comparison with ZFC (5) */
    {"well_founded_check", 1},
    {"rank_function", 1},
    {"foundation_violation_check", 1},
    {"membership_cycle_detection", 1},
    {"zfc_embedding", 0},
    /* Group VI: Relation & Function Constructors (5) */
    {"relation", 3},
    {"function", 3},
    {"function_application", 2},
    {"image", 2},
    {"inverse_image", 2},
    /* Group VII: Cardinal & Ordinal Constructors (3) */
    {"equinumerous", 2},
    {"cardinality", 1},
    {"well_order", 2},
};
#define K_TEMPLATES_COUNT (int) (sizeof(k_templates) / sizeof(k_templates[0]))

/* Test 3：期望不可构造项 */
static const AxiomTestUcExpectation k_unconstructibles[] = {
    {"foundation_independence", "independent_of_ZF_minus_foundation", 4, true},
    {"afa_safa_fafa_bafa_exclusivity", "mutually_exclusive_extensions_of_ZF", 3, true},
    {"quine_atom_uniqueness_in_bafa", "refuted_in_boffa_universe", 3, true},
    {"zfa_consistency", "equiconsistent_with_ZFC", 3, true},
    {"hyperset_existence_in_zfc", "refutable_in_ZFC", 3, true},
    {"bisimulation_undecidable_infinite", "undecidable_for_infinite_state_systems", 3, true},
    {"continuum_hypothesis_in_zfa", "independent_of_ZFA", 4, true},
    {"choice_independence_in_zfa", "independent_of_ZF_plus_AFA", 3, true},
};
#define K_UNCONSTRUCTIBLES_COUNT (int) (sizeof(k_unconstructibles) / sizeof(k_unconstructibles[0]))

/* Test 9：期望外部引用 URL 前缀 */
static const AxiomTestExtRefExpectation k_external_refs[] = {
    {"foundation_independence", "https://en.wikipedia.org/wiki/Axiom_of_regularity"},
    {"afa_safa_fafa_bafa_exclusivity", "https://en.wikipedia.org/wiki/Non-well-founded_set_theory"},
    {"quine_atom_uniqueness_in_bafa", "https://en.wikipedia.org/wiki/Quine_atom"},
    {"zfa_consistency", "https://plato.stanford.edu/entries/nonwellfounded-set-theory/"},
    {"hyperset_existence_in_zfc", "https://en.wikipedia.org/wiki/Axiom_of_regularity"},
    {"bisimulation_undecidable_infinite", "https://en.wikipedia.org/wiki/Bisimulation"},
    {"continuum_hypothesis_in_zfa", "https://en.wikipedia.org/wiki/Continuum_hypothesis"},
    {"choice_independence_in_zfa", "https://en.wikipedia.org/wiki/Axiom_of_choice"},
};
#define K_EXTERNAL_REFS_COUNT (int) (sizeof(k_external_refs) / sizeof(k_external_refs[0]))

/* ============================================================
 * 统一数据驱动用例表（wrapper 收敛至此；共享函数体在 axiom_test_common.h）
 * ============================================================ */

static const AxiomTestCase kCases[] = {
    {
        .pkg_path = AXIOM_PKG_PATH,
        .pkg_name = "non_well_founded_set_theory",
        .save_path = SAVE_TEST_PATH,

        /* Test 2: 模板校验（with_params 形态） */
        .tmpl_style = AXIOM_TEST_TMPL_WITH_PARAMS,
        .tmpl_count = EXPECTED_TEMPLATE_COUNT,
        .tmpl_count_msg = "should have 49 constraint templates",
        .tmpl_expectations = k_templates, .tmpl_n = K_TEMPLATES_COUNT,

        /* Test 3: 不可构造项（B 形态：HTTPS 检查） */
        .uc_style = AXIOM_TEST_UC_B,
        .uc_count = EXPECTED_UNCONSTRUCTIBLE_COUNT,
        .uc_count_msg = "should have 8 unconstructible problems",
        .uc_expectations = k_unconstructibles, .uc_n = K_UNCONSTRUCTIBLES_COUNT,

        /* Test 4: 逻辑框架（S 形态） */
        .lf_style = AXIOM_TEST_LF_S,
        .lf_bottom_geometry = "hyperset_graph_universe",
        .lf_negation_encoding = "classical_complement_in_hyperset_universe",
        .lf_contradiction_behavior = PROPOSITION_KIND_EXPLOSION_PRINCIPLE,
        .lf_contradiction_name = "PROPOSITION_KIND_EXPLOSION_PRINCIPLE",

        /* Test 5: 内容哈希（单次形态） */
        .hash_style = AXIOM_TEST_HASH_SINGLE,
        .hash_free = AXIOM_TEST_FREE_LV_FREE,

        /* Test 6: 往返保存/加载（basic 形态） */
        .rt_style = AXIOM_TEST_RT_BASIC,

        /* Test 7: 依赖验证（V1 形态） */
        .dep_style = AXIOM_TEST_DEP_V1,
        .dep_fail_msg = "FAIL (cross-references internal)",
        .dep_suffix = "",

        /* Test 8: 负向查找 */
        .neg_style = AXIOM_TEST_NEG_BASIC,

        /* Test 9: 外部引用（表驱动形态） */
        .ext_style = AXIOM_TEST_EXT_E1,
        .ext_refs = k_external_refs, .ext_refs_n = K_EXTERNAL_REFS_COUNT,
    },
};
#define K_CASES_COUNT (int) (sizeof(kCases) / sizeof(kCases[0]))

/* ============================================================
 * 文件特有测试（原样保留）
 * ============================================================ */

static void test_key_axioms_present(void) {
    printf("Test 10: Verify all core ZFA axioms are represented...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Core ZFA axioms: ZF axioms (minus Regularity) + AFA */
    const char *required[] = {
        "extensionality", /* ZF1 (strengthened) */
        "afa_decoration", /* AFA (replaces Regularity) */
        "solution_lemma", /* AFA operational form */
        "specification",  /* ZF3 */
        "pairing",        /* ZF4 */
        "union",          /* ZF5 */
        "replacement",    /* ZF6 */
        "infinity",       /* ZF7 */
        "power_set",      /* ZF8 */
        "choice",         /* ZFC9 */
    };

    for (int i = 0; i < 10; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, required[i]);
        TEST_ASSERT(tmpl != NULL, required[i]);
        if (tmpl) {
            printf("  [%d] %s (params=%d) - PRESENT\n", i, required[i], tmpl->param_count);
        }
    }

    axiom_package_destroy(pkg);
}

static void test_afa_specific_templates(void) {
    printf("Test 11: Verify AFA-specific templates (not in ZFC)...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, AXIOM_PKG_PATH);

    /* Templates that are unique to ZFA and not present in ZFC */
    const char *afa_specific[] = {
        "quine_atom",
        "singleton_chain",
        "mutual_recursion_pair",
        "solve_equation_system",
        "bisimulation_check",
        "greatest_bisimulation",
        "accessible_pointed_graph",
        "graph_decoration",
        "picture_of_hyperset",
        "extensional_graph_check",
        "greatest_fixed_point",
        "coinductive_definition",
        "stream_cons",
        "stream_unfold",
        "lts_construction",
        "lts_bisimulation",
        "well_founded_check",
        "foundation_violation_check",
        "membership_cycle_detection",
        "zfc_embedding",
    };

    int afa_count = (int) (sizeof(afa_specific) / sizeof(afa_specific[0]));
    int found = 0;
    for (int i = 0; i < afa_count; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, afa_specific[i]);
        if (tmpl) {
            found++;
            printf("  [%d] %s (params=%d) - PRESENT\n", i, afa_specific[i], tmpl->param_count);
        } else {
            printf("  [%d] %s - MISSING\n", i, afa_specific[i]);
            g_fail_count++;
        }
    }
    TEST_ASSERT(found == afa_count, "all AFA-specific templates should be found");
    printf("  AFA-specific templates: %d / %d\n", found, afa_count);

    axiom_package_destroy(pkg);
}

TEST_MAIN_BEGIN("Non-Well-Founded Set Theory (AFA)")
    LV_REGISTER_AXIOM_CASES("NonWellFoundedSetTheory", kCases, K_CASES_COUNT);
    TEST_MAIN_RUN(test_key_axioms_present);
    TEST_MAIN_RUN(test_afa_specific_templates);
TEST_MAIN_END()


/**
 * @file test_enum_maps.c
 * @brief 枚举→名称映射单一事实来源测试
 *
 * 验证重构后的共享条目宏与公共 API：
 *  (a) lv_geom_type_name 对全部 GEOM_* 返回规范名，且旧错名 "LINE"/"FUNC_BLOCK" 已消失；
 *  (b) lv_geom_type_alias / lv_constraint_type_alias 与 interop_command.c 对外行为一致；
 *  (c) PresetCategory 查询侧（func_block_preset_category_string）与
 *      注册表侧（preset_category_to_string）返回相同中文名（以查询侧 UI 为准）；
 *  (d) 条目宏（LV_GEOM_TYPE_ENTRY / LV_CONSTRAINT_TYPE_ENTRY /
 *      LV_PRESET_CATEGORY_ENTRY / LV_PRESET_EXTENDED_CATEGORY_ENTRY）
 *      无重复枚举值、无重复名称，行数与枚举数量对齐。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "constraint_graph.h"
#include "conflict_detector.h"
#include "dsl_compiler.h"
#include "func_block_preset.h"
#include "func_block_registry.h"
#include "interop.h"
#include "lv.h"
#include "magic.h"
#include "preset_category.h"
#include "test_helpers.h"

/* 全局测试计数器 */
int g_pass_count = 0;
int g_fail_count = 0;

/* 测试辅助宏 */
#define TEST_PASS(name)              \
    do {                             \
        printf("[PASS] %s\n", name); \
    } while (0)

/* ============================================================
 * (a) lv_geom_type_name 规范名（含旧错名回归）
 * ============================================================ */
static void test_geom_type_name(void) {
    TEST_ASSERT_STR_EQ(lv_geom_type_name(GEOM_POINT), "POINT");
    TEST_ASSERT_STR_EQ(lv_geom_type_name(GEOM_LINE_SEGMENT), "LINE_SEGMENT");
    TEST_ASSERT_STR_EQ(lv_geom_type_name(GEOM_REGION), "REGION");
    TEST_ASSERT_STR_EQ(lv_geom_type_name(GEOM_CIRCLE), "CIRCLE");
    TEST_ASSERT_STR_EQ(lv_geom_type_name(GEOM_PORT), "PORT");
    TEST_ASSERT_STR_EQ(lv_geom_type_name(GEOM_FUNCTION_BLOCK), "FUNCTION_BLOCK");

    /* 旧错名必须消失（回归断言） */
    TEST_ASSERT(strcmp(lv_geom_type_name(GEOM_LINE_SEGMENT), "LINE") != 0,
                "GEOM_LINE_SEGMENT 不得再映射为旧错名 \"LINE\"");
    TEST_ASSERT(strcmp(lv_geom_type_name(GEOM_FUNCTION_BLOCK), "FUNC_BLOCK") != 0,
                "GEOM_FUNCTION_BLOCK 不得再映射为旧错名 \"FUNC_BLOCK\"");

    /* 越界返回 "UNKNOWN" */
    TEST_ASSERT_STR_EQ(lv_geom_type_name(-1), "UNKNOWN");
    TEST_ASSERT_STR_EQ(lv_geom_type_name(999), "UNKNOWN");

    TEST_PASS("test_geom_type_name");
}

/* ============================================================
 * (b) 别名 / DOT 形状 与 interop_command.c / meta_repr.c 行为一致
 * ============================================================ */
static void test_geom_type_alias_interop(void) {
    /* lv_geom_type_alias 与 interop_geom_type_name 逐项一致 */
    for (int t = (int) GEOM_POINT; t <= (int) GEOM_FUNCTION_BLOCK; t++) {
        TEST_ASSERT_STR_EQ(interop_geom_type_name((GeomType) t), lv_geom_type_alias(t));
    }
    TEST_ASSERT_STR_EQ(lv_geom_type_alias(GEOM_POINT), "point");
    TEST_ASSERT_STR_EQ(lv_geom_type_alias(GEOM_LINE_SEGMENT), "line_segment");
    TEST_ASSERT_STR_EQ(lv_geom_type_alias(GEOM_REGION), "region");
    TEST_ASSERT_STR_EQ(lv_geom_type_alias(GEOM_CIRCLE), "circle");
    TEST_ASSERT_STR_EQ(lv_geom_type_alias(GEOM_PORT), "port");
    TEST_ASSERT_STR_EQ(lv_geom_type_alias(GEOM_FUNCTION_BLOCK), "function_block");

    /* 约束别名：与 interop_constraint_type_name 逐项一致 */
    for (int t = (int) INCIDENCE; t <= (int) ANGLE; t++) {
        TEST_ASSERT_STR_EQ(interop_constraint_type_name((ConstraintType) t), lv_constraint_type_alias(t));
    }
    TEST_ASSERT_STR_EQ(lv_constraint_type_alias(INCIDENCE), "incidence");
    TEST_ASSERT_STR_EQ(lv_constraint_type_alias(BETWEENNESS), "betweenness");
    TEST_ASSERT_STR_EQ(lv_constraint_type_alias(CONTAINMENT), "containment");
    TEST_ASSERT_STR_EQ(lv_constraint_type_alias(CONNECTION), "connection");
    TEST_ASSERT_STR_EQ(lv_constraint_type_alias(ANGLE), "angle");

    /* DOT 形状与既有 meta_repr_export_dot 形状表逐项一致 */
    TEST_ASSERT_STR_EQ(lv_geom_type_dot_shape(GEOM_POINT), "ellipse");
    TEST_ASSERT_STR_EQ(lv_geom_type_dot_shape(GEOM_LINE_SEGMENT), "diamond");
    TEST_ASSERT_STR_EQ(lv_geom_type_dot_shape(GEOM_REGION), "box");
    TEST_ASSERT_STR_EQ(lv_geom_type_dot_shape(GEOM_CIRCLE), "circle");
    TEST_ASSERT_STR_EQ(lv_geom_type_dot_shape(GEOM_PORT), "box");
    TEST_ASSERT_STR_EQ(lv_geom_type_dot_shape(GEOM_FUNCTION_BLOCK), "box");

    TEST_PASS("test_geom_type_alias_interop");
}

/* ============================================================
 * (c) PresetCategory 查询侧与注册表侧中文名一致
 * ============================================================ */
static void test_preset_category_consistency(void) {
    static const PresetCategory kAllCategories[] = {
        PRESET_CATEGORY_CONSTRUCTION,
        PRESET_CATEGORY_MEASUREMENT,
        PRESET_CATEGORY_TRANSFORMATION,
        PRESET_CATEGORY_ALGEBRAIC,
        PRESET_CATEGORY_LOGIC,
        PRESET_CATEGORY_ANALYSIS,
        PRESET_CATEGORY_NUMBER_THEORY,
        PRESET_CATEGORY_GROUP_THEORY,
        PRESET_CATEGORY_RING_THEORY,
        PRESET_CATEGORY_FIELD_THEORY,
        PRESET_CATEGORY_TOPOLOGY,
        PRESET_CATEGORY_LINEAR_ALGEBRA,
        PRESET_CATEGORY_COMBINATORICS,
        PRESET_CATEGORY_COMPLEX_ANALYSIS,
        PRESET_CATEGORY_PROBABILITY,
        PRESET_CATEGORY_GEOMETRY,
        PRESET_CATEGORY_ALGEBRA,
        PRESET_CATEGORY_CATEGORY_THEORY,
        PRESET_CATEGORY_SET_THEORY,
        PRESET_CATEGORY_CUSTOM,
        PRESET_CATEGORY_GRAPH_THEORY,
        PRESET_CATEGORY_DIFFERENTIAL_GEOMETRY,
        PRESET_CATEGORY_NUMERICAL,
        PRESET_CATEGORY_OPTIMIZATION,
        PRESET_CATEGORY_MATH_LOGIC,
    };

    for (size_t i = 0; i < sizeof(kAllCategories) / sizeof(kAllCategories[0]); i++) {
        const char *q = func_block_preset_category_string(kAllCategories[i]);
        const char *r = preset_category_to_string(kAllCategories[i]);
        TEST_ASSERT(q != NULL, "查询侧类别名不应为 NULL");
        TEST_ASSERT(r != NULL, "注册表侧类别名不应为 NULL");
        if (q != NULL && r != NULL)
            TEST_ASSERT_STR_EQ(q, r);
    }

    /* 统一后中文名以查询侧（UI 显示源）为准 */
    TEST_ASSERT_STR_EQ(func_block_preset_category_string(PRESET_CATEGORY_ANALYSIS), "数学分析");
    TEST_ASSERT_STR_EQ(preset_category_to_string(PRESET_CATEGORY_ANALYSIS), "数学分析");
    TEST_ASSERT_STR_EQ(func_block_preset_category_string(PRESET_CATEGORY_NUMBER_THEORY), "数论");
    TEST_ASSERT_STR_EQ(func_block_preset_category_string(PRESET_CATEGORY_GROUP_THEORY), "群论");
    TEST_ASSERT_STR_EQ(func_block_preset_category_string(PRESET_CATEGORY_RING_THEORY), "环论");
    TEST_ASSERT_STR_EQ(func_block_preset_category_string(PRESET_CATEGORY_FIELD_THEORY), "域论");
    TEST_ASSERT_STR_EQ(func_block_preset_category_string(PRESET_CATEGORY_TOPOLOGY), "拓扑学");
    TEST_ASSERT_STR_EQ(func_block_preset_category_string(PRESET_CATEGORY_GEOMETRY), "几何学");
    TEST_ASSERT_STR_EQ(func_block_preset_category_string(PRESET_CATEGORY_ALGEBRA), "代数学");

    /* 注册表侧字符串解析（preset_category_from_string）与共享表一致 */
    {
        PresetCategory cat;
        TEST_ASSERT(preset_category_from_string("数学分析", &cat), "应能解析 \"数学分析\"");
        TEST_ASSERT_EQ((int) cat, (int) PRESET_CATEGORY_ANALYSIS);
        TEST_ASSERT(preset_category_from_string("analysis", &cat), "应能解析英文 key \"analysis\"");
        TEST_ASSERT_EQ((int) cat, (int) PRESET_CATEGORY_ANALYSIS);
        TEST_ASSERT(preset_category_from_string("number_theory", &cat), "应能解析英文 key \"number_theory\"");
        TEST_ASSERT_EQ((int) cat, (int) PRESET_CATEGORY_NUMBER_THEORY);
        TEST_ASSERT(preset_category_from_string("math_logic", &cat), "应能解析英文 key \"math_logic\"");
        TEST_ASSERT_EQ((int) cat, (int) PRESET_CATEGORY_MATH_LOGIC);
    }

    TEST_PASS("test_preset_category_consistency");
}

/* ============================================================
 * (d) 条目宏无重复枚举值 / 重复名称 / 行数与枚举对齐
 * ============================================================ */
static int has_duplicate_int(const int *arr, size_t n) {
    for (size_t i = 0; i < n; i++)
        for (size_t j = i + 1; j < n; j++)
            if (arr[i] == arr[j])
                return 1;
    return 0;
}

static int has_duplicate_str(const char *const *arr, size_t n) {
    for (size_t i = 0; i < n; i++)
        for (size_t j = i + 1; j < n; j++)
            if (arr[i] != NULL && arr[j] != NULL && strcmp(arr[i], arr[j]) == 0)
                return 1;
    return 0;
}

static void test_entry_macro_uniqueness(void) {
    /* ── LV_GEOM_TYPE_ENTRY ── */
    {
#define ROW_GEOM_ENUM(ENUM, NAME, ALIAS, SHAPE, PREFIX, COLOR) ENUM,
#define ROW_GEOM_NAME(ENUM, NAME, ALIAS, SHAPE, PREFIX, COLOR) NAME,
#define ROW_GEOM_ALIAS(ENUM, NAME, ALIAS, SHAPE, PREFIX, COLOR) ALIAS,
        static const int kGeomEnums[] = {LV_GEOM_TYPE_ENTRY(ROW_GEOM_ENUM)};
        static const char *const kGeomNames[] = {LV_GEOM_TYPE_ENTRY(ROW_GEOM_NAME)};
        static const char *const kGeomAliases[] = {LV_GEOM_TYPE_ENTRY(ROW_GEOM_ALIAS)};
#undef ROW_GEOM_ENUM
#undef ROW_GEOM_NAME
#undef ROW_GEOM_ALIAS
        TEST_ASSERT(!has_duplicate_int(kGeomEnums, lv_ARRAY_SIZE(kGeomEnums)),
                    "LV_GEOM_TYPE_ENTRY 枚举值重复");
        TEST_ASSERT(!has_duplicate_str(kGeomNames, lv_ARRAY_SIZE(kGeomNames)),
                    "LV_GEOM_TYPE_ENTRY 规范名重复");
        TEST_ASSERT(!has_duplicate_str(kGeomAliases, lv_ARRAY_SIZE(kGeomAliases)),
                    "LV_GEOM_TYPE_ENTRY 别名重复");
        TEST_ASSERT_EQ((int) lv_ARRAY_SIZE(kGeomEnums), (int) GEOM_FUNCTION_BLOCK + 1);
    }

    /* ── LV_CONSTRAINT_TYPE_ENTRY ── */
    {
#define ROW_CON_ENUM(ENUM, NAME, ALIAS) ENUM,
#define ROW_CON_NAME(ENUM, NAME, ALIAS) NAME,
#define ROW_CON_ALIAS(ENUM, NAME, ALIAS) ALIAS,
        static const int kConEnums[] = {LV_CONSTRAINT_TYPE_ENTRY(ROW_CON_ENUM)};
        static const char *const kConNames[] = {LV_CONSTRAINT_TYPE_ENTRY(ROW_CON_NAME)};
        static const char *const kConAliases[] = {LV_CONSTRAINT_TYPE_ENTRY(ROW_CON_ALIAS)};
#undef ROW_CON_ENUM
#undef ROW_CON_NAME
#undef ROW_CON_ALIAS
        TEST_ASSERT(!has_duplicate_int(kConEnums, lv_ARRAY_SIZE(kConEnums)),
                    "LV_CONSTRAINT_TYPE_ENTRY 枚举值重复");
        TEST_ASSERT(!has_duplicate_str(kConNames, lv_ARRAY_SIZE(kConNames)),
                    "LV_CONSTRAINT_TYPE_ENTRY 规范名重复");
        TEST_ASSERT(!has_duplicate_str(kConAliases, lv_ARRAY_SIZE(kConAliases)),
                    "LV_CONSTRAINT_TYPE_ENTRY 别名重复");
        TEST_ASSERT_EQ((int) lv_ARRAY_SIZE(kConEnums), (int) ANGLE + 1);
    }

    /* ── LV_PRESET_CATEGORY_ENTRY ── */
    {
#define ROW_PC_ENUM(ENUM, EN_KEY, ZH_NAME) ENUM,
#define ROW_PC_KEY(ENUM, EN_KEY, ZH_NAME) EN_KEY,
#define ROW_PC_ZH(ENUM, EN_KEY, ZH_NAME) ZH_NAME,
        static const int kPcEnums[] = {LV_PRESET_CATEGORY_ENTRY(ROW_PC_ENUM)};
        static const char *const kPcKeys[] = {LV_PRESET_CATEGORY_ENTRY(ROW_PC_KEY)};
        static const char *const kPcZh[] = {LV_PRESET_CATEGORY_ENTRY(ROW_PC_ZH)};
#undef ROW_PC_ENUM
#undef ROW_PC_KEY
#undef ROW_PC_ZH
        TEST_ASSERT(!has_duplicate_int(kPcEnums, lv_ARRAY_SIZE(kPcEnums)),
                    "LV_PRESET_CATEGORY_ENTRY 枚举值重复");
        TEST_ASSERT(!has_duplicate_str(kPcKeys, lv_ARRAY_SIZE(kPcKeys)),
                    "LV_PRESET_CATEGORY_ENTRY 英文 key 重复");
        TEST_ASSERT(!has_duplicate_str(kPcZh, lv_ARRAY_SIZE(kPcZh)),
                    "LV_PRESET_CATEGORY_ENTRY 中文名重复");
        TEST_ASSERT_EQ((int) lv_ARRAY_SIZE(kPcEnums), (int) PRESET_CATEGORY_COUNT);
    }

    /* ── LV_PRESET_EXTENDED_CATEGORY_ENTRY ── */
    {
#define ROW_PE_ENUM(ENUM, ZH_NAME) ENUM,
#define ROW_PE_ZH(ENUM, ZH_NAME) ZH_NAME,
        static const int kPeEnums[] = {LV_PRESET_EXTENDED_CATEGORY_ENTRY(ROW_PE_ENUM)};
        static const char *const kPeZh[] = {LV_PRESET_EXTENDED_CATEGORY_ENTRY(ROW_PE_ZH)};
#undef ROW_PE_ENUM
#undef ROW_PE_ZH
        TEST_ASSERT(!has_duplicate_int(kPeEnums, lv_ARRAY_SIZE(kPeEnums)),
                    "LV_PRESET_EXTENDED_CATEGORY_ENTRY 枚举值重复");
        TEST_ASSERT(!has_duplicate_str(kPeZh, lv_ARRAY_SIZE(kPeZh)),
                    "LV_PRESET_EXTENDED_CATEGORY_ENTRY 中文名重复");
        TEST_ASSERT_EQ((int) lv_ARRAY_SIZE(kPeEnums), (int) PRESET_EXT_CATEGORY_COUNT);
    }

    TEST_PASS("test_entry_macro_uniqueness");
}

/* ============================================================
 * K7: X-macro 化枚举表 str↔enum 往返等价（抽样首尾 + 中间）
 * ============================================================ */
static void test_k7_enum_roundtrip(void) {
    /* dsl_compiler_load: s_ir_op_names（30 项） */
    TEST_ASSERT_STR_EQ(dsl_ir_op_name(IR_CREATE_POINT), "CREATE_POINT");
    TEST_ASSERT_STR_EQ(dsl_ir_op_name(IR_CONSTRAIN_EQUAL), "CONSTRAIN_EQUAL");
    TEST_ASSERT_STR_EQ(dsl_ir_op_name(IR_NOOP), "NOOP");

    /* dsl_compiler_load: s_ast_type_names（25 项） */
    TEST_ASSERT_STR_EQ(dsl_ast_type_name(DSL_AST_PROGRAM), "PROGRAM");
    TEST_ASSERT_STR_EQ(dsl_ast_type_name(DSL_AST_BISECTOR), "BISECTOR");
    TEST_ASSERT_STR_EQ(dsl_ast_type_name(DSL_AST_NUMBER), "NUMBER");

    /* interop_theorem: export（8 项）往返 */
    TEST_ASSERT_STR_EQ(interop_export_format_name(INTEROP_EXPORT_COQ), "coq");
    TEST_ASSERT_STR_EQ(interop_export_format_name(INTEROP_EXPORT_PDF), "pdf");
    TEST_ASSERT_STR_EQ(interop_export_format_name(INTEROP_EXPORT_CANONICAL), "canonical");
    TEST_ASSERT_EQ((int) interop_parse_export_format("coq"), (int) INTEROP_EXPORT_COQ);
    TEST_ASSERT_EQ((int) interop_parse_export_format("pdf"), (int) INTEROP_EXPORT_PDF);
    TEST_ASSERT_EQ((int) interop_parse_export_format("canonical"), (int) INTEROP_EXPORT_CANONICAL);

    /* interop_theorem: import（3 项）往返 */
    TEST_ASSERT_STR_EQ(interop_import_format_name(INTEROP_IMPORT_GEOGEBRA), "geogebra");
    TEST_ASSERT_STR_EQ(interop_import_format_name(INTEROP_IMPORT_GEOJSON), "geojson");
    TEST_ASSERT_STR_EQ(interop_import_format_name(INTEROP_IMPORT_SVG), "svg");
    TEST_ASSERT_EQ((int) interop_parse_import_format("geogebra"), (int) INTEROP_IMPORT_GEOGEBRA);
    TEST_ASSERT_EQ((int) interop_parse_import_format("geojson"), (int) INTEROP_IMPORT_GEOJSON);
    TEST_ASSERT_EQ((int) interop_parse_import_format("svg"), (int) INTEROP_IMPORT_SVG);

    /* conflict_detector: s_lv_conflict_type_name_entries（14 项） */
    TEST_ASSERT_STR_EQ(lv_conflict_type_name(CONFLICT_POINT_POSITION), "PointPositionConflict");
    TEST_ASSERT_STR_EQ(lv_conflict_type_name(CONFLICT_TRANSITIVE_EQUALITY), "TransitiveEquality");
    TEST_ASSERT_STR_EQ(lv_conflict_type_name(CONFLICT_UNKNOWN), "Unknown");

    /* magic_domain: element（6 项）往返（中英文双写） */
    TEST_ASSERT_STR_EQ(element_to_string(ELEMENT_FIRE), "FIRE");
    TEST_ASSERT_STR_EQ(element_to_string(ELEMENT_NONE), "NONE");
    TEST_ASSERT_EQ((int) string_to_element("FIRE"), (int) ELEMENT_FIRE);
    TEST_ASSERT_EQ((int) string_to_element("火"), (int) ELEMENT_FIRE);
    TEST_ASSERT_EQ((int) string_to_element("以太"), (int) ELEMENT_ETHER);

    /* magic_domain: stage（4 项） */
    TEST_ASSERT_STR_EQ(stage_to_string(SPELL_STAGE_MOLDING), "开模");
    TEST_ASSERT_STR_EQ(stage_to_string(SPELL_STAGE_INFUSING), "灌注");
    TEST_ASSERT_STR_EQ(stage_to_string(SPELL_STAGE_RELEASING), "释放");

    /* magic_domain: status（5 项） */
    TEST_ASSERT_STR_EQ(status_to_string(SPELL_STATUS_IDLE), "空闲");
    TEST_ASSERT_STR_EQ(status_to_string(SPELL_STATUS_FAILED), "失败");
    TEST_ASSERT_STR_EQ(status_to_string(SPELL_STATUS_BACKLASH), "反噬");

    /* magic_domain: reaction（4 项） */
    TEST_ASSERT_STR_EQ(reaction_to_string(ELEMENT_REACTION_NONE), "无反应");
    TEST_ASSERT_STR_EQ(reaction_to_string(ELEMENT_REACTION_WEAKEN), "削弱");
    TEST_ASSERT_STR_EQ(reaction_to_string(ELEMENT_REACTION_CONFLICT), "冲突");

    /* magic_domain: restriction（5 项） */
    TEST_ASSERT_STR_EQ(restriction_to_string(RESTRICTION_NONE), "无限制");
    TEST_ASSERT_STR_EQ(restriction_to_string(RESTRICTION_FORBIDDEN), "禁术级");
    TEST_ASSERT_STR_EQ(restriction_to_string(RESTRICTION_ABSOLUTE), "绝对禁术");

    TEST_PASS("test_k7_enum_roundtrip");
}

/* ============================================================
 * 主函数
 * ============================================================ */
TEST_MAIN_BEGIN("枚举映射单一事实来源测试")
    printf("========================================\n");
    printf("枚举映射单一事实来源测试\n");
    printf("========================================\n\n");
    g_pass_count = 0;
    g_fail_count = 0;
    func_block_preset_library_init();
    TEST_MAIN_RUN(test_geom_type_name);
    TEST_MAIN_RUN(test_geom_type_alias_interop);
    TEST_MAIN_RUN(test_preset_category_consistency);
    TEST_MAIN_RUN(test_entry_macro_uniqueness);
    TEST_MAIN_RUN(test_k7_enum_roundtrip);
    func_block_preset_library_cleanup();
    printf("\n========================================\n");
    if (g_fail_count == 0) {
        printf("所有测试通过! (%d 项)\n", g_pass_count);
    } else {
        printf("测试结果: %d 通过, %d 失败, %d 总计\n", g_pass_count, g_fail_count, g_pass_count + g_fail_count);
    }
    printf("========================================\n");
    return g_fail_count > 0 ? 1 : 0;
TEST_MAIN_END()

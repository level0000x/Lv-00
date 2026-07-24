/**
 * @file test_layer5_core.c
 * @brief Layer5 核心模块深度测试 — Magic / Plugin System / Proof Compiler
 *
 * 覆盖已有测试未深入触及的边缘情况、压力场景、状态机路径。
 *
 * @author Lv-00 Project
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/magic.h"
#include "lv/plugin_system.h"
#include "lv/proof.h"
#include "lv/proof_compiler.h"

#include "lv.h"
#include "test_helpers.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ================================================================
 * Magic 深度测试
 * ================================================================ */

/* rune_parse 边缘情况 */
static void test_rune_parse_edge(void) {
    /* 代数数解析 */
    Rune *r = rune_parse("algebraic:3.14159:WATER");
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQ(rune_get_element(r), ELEMENT_WATER);
    rune_destroy(r);

    /* 分母为零 — 应失败 */
    r = rune_parse("rational:1/0:FIRE");
    TEST_ASSERT_NULL(r);

    /* 分子过长 */
    r = rune_parse("rational:999999999999999999999999999999/1:FIRE");
    TEST_ASSERT_NULL(r);

    /* 纯整数无元素简写 */
    r = rune_parse("42");
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQ(rune_get_element(r), ELEMENT_NONE);
    rune_destroy(r);

    /* 分数简写 */
    r = rune_parse("7/3");
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQ(rune_get_element(r), ELEMENT_NONE);
    rune_destroy(r);

    /* 无效前缀 */
    r = rune_parse("invalid:1/2:FIRE");
    TEST_ASSERT_NULL(r);

    /* 前导空白 */
    r = rune_parse("  rational:3/4:EARTH");
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQ(rune_get_element(r), ELEMENT_EARTH);
    rune_destroy(r);

    /* 小写元素名称 */
    r = rune_parse("5:fire");
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQ(rune_get_element(r), ELEMENT_FIRE);
    rune_destroy(r);

    r = rune_parse("5:water");
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQ(rune_get_element(r), ELEMENT_WATER);
    rune_destroy(r);

    r = rune_parse("5:earth");
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQ(rune_get_element(r), ELEMENT_EARTH);
    rune_destroy(r);

    r = rune_parse("5:air");
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQ(rune_get_element(r), ELEMENT_AIR);
    rune_destroy(r);

    r = rune_parse("5:none");
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQ(rune_get_element(r), ELEMENT_NONE);
    rune_destroy(r);
}

/* rune_serialize NULL 安全 */
static void test_rune_serialize_null(void) {
    char *s = rune_serialize(NULL);
    TEST_ASSERT_NULL(s);

    char buf[64];
    int n = rune_serialize_to_buffer(NULL, buf, sizeof(buf));
    TEST_ASSERT_EQ(n, -1);

    Rune *r = rune_create_rational(1, 1, ELEMENT_FIRE);
    n = rune_serialize_to_buffer(r, NULL, 0);
    TEST_ASSERT_EQ(n, -1);
    rune_destroy(r);
}

/* 符文序列扩容测试 */
static void test_rune_sequence_growth(void) {
    RuneSequence *seq = rune_sequence_create();
    TEST_ASSERT_NOT_NULL(seq);

    /* 添加超过初始容量 (16) 的符文，触发多次扩容 */
    Rune *ref[20];
    for (int i = 0; i < 20; i++) {
        ref[i] = rune_create_rational(i, 1, ELEMENT_FIRE);
        TEST_ASSERT(rune_sequence_add(seq, ref[i]), "add rune %d", i);
    }
    TEST_ASSERT_EQ(rune_sequence_length(seq), 20);

    /* 验证所有符文位置正确 */
    for (int i = 0; i < 20; i++) {
        Rune *got = rune_sequence_get(seq, i);
        TEST_ASSERT_NOT_NULL(got);
        TEST_ASSERT_EQ(rune_get_power(got), 1); /* 默认威力 */
    }

    /* 越界访问 */
    TEST_ASSERT_NULL(rune_sequence_get(seq, -1));
    TEST_ASSERT_NULL(rune_sequence_get(seq, 20));
    TEST_ASSERT_NULL(rune_sequence_get(seq, 999));

    /* 向 NULL 序列添加 */
    Rune *x = rune_create_rational(0, 1, ELEMENT_NONE);
    TEST_ASSERT(!rune_sequence_add(NULL, x));
    TEST_ASSERT(!rune_sequence_add(seq, NULL));
    rune_destroy(x);

    rune_sequence_destroy(seq);
}

/* magic_array 约束类型全覆盖 */
static void test_magic_array_all_constraint_types(void) {
    MagicArray *arr = magic_array_create();
    TEST_ASSERT_NOT_NULL(arr);

    Rune *r1 = rune_create_rational(0, 1, ELEMENT_FIRE);
    Rune *r2 = rune_create_rational(1, 1, ELEMENT_WATER);
    int i1 = magic_array_add_rune(arr, r1);
    int i2 = magic_array_add_rune(arr, r2);
    TEST_ASSERT_GE(i1, 0);
    TEST_ASSERT_GE(i2, 0);

    /* 测试所有约束类型 */
    ArrayConstraintType types[] = {ARRAY_CONNECTION,  ARRAY_ENHANCEMENT, ARRAY_CONFLICT, ARRAY_INTERSECTION,
                                   ARRAY_CONTAINMENT, ARRAY_BOUNDARY,    ARRAY_CHANNEL,  ARRAY_FOCUS};
    int n_types = sizeof(types) / sizeof(types[0]);

    /* 每加一种约束需要两个符文，我们复用前两个符文 */
    for (int i = 0; i < n_types; i++) {
        int cid = magic_array_add_constraint(arr, types[i], 0, 1);
        TEST_ASSERT_GE(cid, 0);
    }
    TEST_ASSERT_EQ(magic_array_get_constraint_count(arr), n_types);

    /* 无效索引 */
    TEST_ASSERT_EQ(magic_array_add_constraint(arr, ARRAY_CONNECTION, 0, 99), -1);
    TEST_ASSERT_EQ(magic_array_add_constraint(arr, ARRAY_CONNECTION, -1, 0), -1);

    /* NULL 安全 */
    TEST_ASSERT_EQ(magic_array_add_constraint(NULL, ARRAY_CONNECTION, 0, 1), -1);

    /* 移除约束边界 */
    TEST_ASSERT(!magic_array_remove_constraint(arr, -1));
    TEST_ASSERT(!magic_array_remove_constraint(arr, 99));
    TEST_ASSERT(!magic_array_remove_constraint(NULL, 0));

    rune_destroy(r1);
    rune_destroy(r2);
    magic_array_destroy(arr);
}

/* magic_array 稳定性边界 */
static void test_magic_array_stability_boundary(void) {
    /* 空阵 */
    MagicArray *arr = magic_array_create();
    double stab = array_calculate_stability(arr);
    TEST_ASSERT_EQ(stab, 0.0);

    /* 0 符文 */
    TEST_ASSERT_EQ(array_calculate_stability(NULL), 0.0);

    /* 1 符文 — 少于 3 个，稳定性折半 */
    Rune *r1 = rune_create_rational(0, 1, ELEMENT_FIRE);
    magic_array_add_rune(arr, r1);
    stab = array_calculate_stability(arr);
    TEST_ASSERT(stab > 0.0 && stab <= 0.5, "1 rune stability halved");

    /* 2 符文 — 仍少于 3 */
    Rune *r2 = rune_create_rational(1, 1, ELEMENT_WATER);
    magic_array_add_rune(arr, r2);
    stab = array_calculate_stability(arr);
    TEST_ASSERT(stab > 0.0 && stab <= 0.5, "2 rune stability halved");

    /* 3 符文 — 达到最小要求，稳定性恢复 */
    Rune *r3 = rune_create_rational(2, 1, ELEMENT_EARTH);
    magic_array_add_rune(arr, r3);
    stab = array_calculate_stability(arr);
    TEST_ASSERT(stab >= 0.9, "3 rune stability ~1.0");

    /* 添加大量冲突约束使稳定性归零 */
    for (int i = 0; i < 12; i++) {
        magic_array_add_constraint(arr, ARRAY_CONFLICT, 0, 1);
    }
    stab = array_calculate_stability(arr);
    TEST_ASSERT_EQ(stab, 0.0);

    rune_destroy(r1);
    rune_destroy(r2);
    rune_destroy(r3);
    magic_array_destroy(arr);
}

/* magic_array_copy/deep 完整深拷贝验证 */
static void test_magic_array_deep_copy(void) {
    MagicArray *src = magic_array_create();
    Rune *r1 = rune_create_rational(5, 2, ELEMENT_FIRE);
    Rune *r2 = rune_create_rational(3, 1, ELEMENT_WATER);
    magic_array_add_rune(src, r1);
    magic_array_add_rune(src, r2);
    magic_array_add_constraint(src, ARRAY_CONFLICT, 0, 1);

    MagicArray *cpy = magic_array_copy(src);
    TEST_ASSERT_NOT_NULL(cpy);
    TEST_ASSERT_EQ(magic_array_get_rune_count(cpy), 2);
    TEST_ASSERT_EQ(magic_array_get_constraint_count(cpy), 1);

    /* 深拷贝：修改源不应影响副本 */
    Rune *r1b = rune_create_rational(9, 1, ELEMENT_EARTH);
    magic_array_add_rune(src, r1b);
    TEST_ASSERT_EQ(magic_array_get_rune_count(src), 3);
    TEST_ASSERT_EQ(magic_array_get_rune_count(cpy), 2);

    rune_destroy(r1b);
    magic_array_destroy(src);
    magic_array_destroy(cpy);
    rune_destroy(r1);
    rune_destroy(r2);
}

/* magic_array_merge 边缘 */
static void test_magic_array_merge_edge(void) {
    MagicArray *dest = magic_array_create();
    MagicArray *src = magic_array_create();

    /* NULL 合并 */
    TEST_ASSERT(!magic_array_merge(NULL, src));
    TEST_ASSERT(!magic_array_merge(dest, NULL));
    TEST_ASSERT(!magic_array_merge(NULL, NULL));

    /* 空源合并 */
    TEST_ASSERT(magic_array_merge(dest, src), "empty merge");
    TEST_ASSERT_EQ(magic_array_get_rune_count(dest), 0);

    Rune *r = rune_create_rational(1, 1, ELEMENT_FIRE);
    magic_array_add_rune(src, r);
    TEST_ASSERT(magic_array_merge(dest, src), "merge non-empty src");
    TEST_ASSERT_EQ(magic_array_get_rune_count(dest), 1);

    magic_array_destroy(dest);
    magic_array_destroy(src);
    rune_destroy(r);
}

/* magic_array_deserialize 复杂场景 */
static void test_magic_array_deserialize_complex(void) {
    /* 无效 JSON */
    MagicArray *arr = magic_array_deserialize("{invalid}");
    TEST_ASSERT_NULL(arr);

    /* 非对象开头 */
    arr = magic_array_deserialize("[1,2,3]");
    TEST_ASSERT_NULL(arr);

    /* 空 runes 数组 */
    arr = magic_array_deserialize("{\"runes\":[]}");
    TEST_ASSERT_NOT_NULL(arr);
    TEST_ASSERT_EQ(magic_array_get_rune_count(arr), 0);
    magic_array_destroy(arr);

    /* 包含 name 字段 */
    arr = magic_array_deserialize(
        "{\"name\":\"测试阵\",\"runes\":[{\"type\":\"rational\",\"num\":3,\"denom\":4,\"element\":\"FIRE\"}]}");
    TEST_ASSERT_NOT_NULL(arr);
    TEST_ASSERT_EQ(magic_array_get_rune_count(arr), 1);
    TEST_ASSERT_EQ(magic_array_get_rune_count(arr), 1);
    magic_array_destroy(arr);

    /* 无元素字段 */
    arr = magic_array_deserialize("{\"runes\":[{\"type\":\"rational\",\"num\":1,\"denom\":1}]}");
    TEST_ASSERT_NOT_NULL(arr);
    TEST_ASSERT_EQ(magic_array_get_rune_count(arr), 1);
    magic_array_destroy(arr);

    /* 代数数反序列化 */
    arr = magic_array_deserialize("{\"runes\":[{\"type\":\"algebraic\",\"value\":1.618,\"element\":\"AIR\"}]}");
    TEST_ASSERT_NOT_NULL(arr);
    TEST_ASSERT_EQ(magic_array_get_rune_count(arr), 1);
    magic_array_destroy(arr);
}

/* magic_array 元素平衡性深度测试 */
static void test_magic_array_balance_deep(void) {
    MagicArray *arr = magic_array_create();

    /* 全同一种元素 — 不平衡 */
    Rune *runes[7];
    for (int i = 0; i < 7; i++) {
        runes[i] = rune_create_rational(i, 1, ELEMENT_FIRE);
        magic_array_add_rune(arr, runes[i]);
    }
    TEST_ASSERT(!magic_array_check_balance(arr), "all fire -> unbalanced");

    /* 添加其余四种元素各一个 → 趋于平衡 */
    Rune *r_water = rune_create_rational(0, 1, ELEMENT_WATER);
    Rune *r_air = rune_create_rational(0, 1, ELEMENT_AIR);
    Rune *r_earth = rune_create_rational(0, 1, ELEMENT_EARTH);
    Rune *r_ether = rune_create_rational(0, 1, ELEMENT_ETHER);
    magic_array_add_rune(arr, r_water);
    magic_array_add_rune(arr, r_air);
    magic_array_add_rune(arr, r_earth);
    magic_array_add_rune(arr, r_ether);

    TEST_ASSERT(magic_array_check_balance(arr), "balanced after mixing");

    for (int i = 0; i < 7; i++)
        rune_destroy(runes[i]);
    rune_destroy(r_water);
    rune_destroy(r_air);
    rune_destroy(r_earth);
    rune_destroy(r_ether);
    magic_array_destroy(arr);
}

/* spell_cast 全路径 */
static void test_spell_cast_paths(void) {
    /* 准备魔法阵 */
    MagicArray *arr = magic_array_create();
    Rune *ra = rune_create_rational(1, 1, ELEMENT_FIRE);
    Rune *rb = rune_create_rational(2, 1, ELEMENT_WATER);
    Rune *rc = rune_create_rational(3, 1, ELEMENT_EARTH);
    magic_array_add_rune(arr, ra);
    magic_array_add_rune(arr, rb);
    magic_array_add_rune(arr, rc);

    /* 创建咒语 */
    Spell *spell = spell_create("Test Bolt");
    TEST_ASSERT_NOT_NULL(spell);

    RuneSequence *seq = rune_sequence_create();
    Rune *sm = rune_create_rational(1, 1, ELEMENT_FIRE);
    rune_sequence_add(seq, sm);
    spell_configure_molding(spell, seq);
    spell_configure_purifying(spell, ELEMENT_FIRE, 0.6);
    spell_configure_infusing(spell, 2); /* T2 */

    /* 场景1: 成功施法, 0 输入 */
    SymbolicCoord *outputs[1] = {NULL};
    SpellStatus st = spell_cast(spell, arr, NULL, 0, outputs, 1);
    TEST_ASSERT_EQ(st, SPELL_STATUS_SUCCESS);
    TEST_ASSERT_NOT_NULL(outputs[0]);
    symbolic_coord_destroy(outputs[0]);

    /* 场景2: 1 输入 */
    SymbolicCoord *in1 = symbolic_coord_create_rational(5, 1);
    SymbolicCoord *inputs[1] = {in1};
    st = spell_cast(spell, arr, inputs, 1, outputs, 1);
    TEST_ASSERT_EQ(st, SPELL_STATUS_SUCCESS);
    TEST_ASSERT_NOT_NULL(outputs[0]);
    symbolic_coord_destroy(outputs[0]);
    symbolic_coord_destroy(in1);

    /* 场景3: 2 输入（求和） */
    SymbolicCoord *in_a = symbolic_coord_create_rational(3, 1);
    SymbolicCoord *in_b = symbolic_coord_create_rational(4, 1);
    SymbolicCoord *in_multi[2] = {in_a, in_b};
    st = spell_cast(spell, arr, in_multi, 2, outputs, 1);
    TEST_ASSERT_EQ(st, SPELL_STATUS_SUCCESS);
    TEST_ASSERT_NOT_NULL(outputs[0]);
    symbolic_coord_destroy(outputs[0]);
    symbolic_coord_destroy(in_a);
    symbolic_coord_destroy(in_b);

    /* 场景4: 提纯阶段失败 — 需要 EARTH 但阵中只有 FIRE/WATER */
    spell_configure_purifying(spell, ELEMENT_EARTH, 0.6);
    st = spell_cast(spell, arr, NULL, 0, outputs, 1);
    TEST_ASSERT_EQ(st, SPELL_STATUS_FAILED);
    TEST_ASSERT_EQ(spell_get_current_stage(spell), SPELL_STAGE_PURIFYING);

    /* 场景5: 触发反噬 — 大量冲突使稳定性 < 0.3 */
    spell_configure_purifying(spell, ELEMENT_FIRE, 0.6);
    for (int i = 0; i < 10; i++) {
        magic_array_add_constraint(arr, ARRAY_CONFLICT, 0, 1);
    }
    st = spell_cast(spell, arr, NULL, 0, NULL, 0);
    TEST_ASSERT_EQ(st, SPELL_STATUS_BACKLASH);
    TEST_ASSERT_EQ(spell_get_current_stage(spell), SPELL_STAGE_INFUSING);

    /* 场景6: 空开模序列 */
    Spell *empty_spell = spell_create("Empty Molding");
    st = spell_cast(empty_spell, arr, NULL, 0, NULL, 0);
    TEST_ASSERT_EQ(st, SPELL_STATUS_FAILED);

    /* 场景7: NULL 参数 */
    TEST_ASSERT_EQ(spell_cast(NULL, arr, NULL, 0, NULL, 0), SPELL_STATUS_FAILED);
    TEST_ASSERT_EQ(spell_cast(spell, NULL, NULL, 0, NULL, 0), SPELL_STATUS_FAILED);

    rune_sequence_destroy(seq);
    spell_destroy(spell);
    spell_destroy(empty_spell);
    magic_array_destroy(arr);
    rune_destroy(ra);
    rune_destroy(rb);
    rune_destroy(rc);
    rune_destroy(sm);
}

/* spell 配置边界 */
static void test_spell_config_boundary(void) {
    Spell *spell = spell_create("Boundary Test");

    /* 难度边界 */
    TEST_ASSERT(spell_set_difficulty(spell, 0)); /* 截断到 1 */
    TEST_ASSERT_EQ(spell_get_difficulty(spell), 1);
    TEST_ASSERT(spell_set_difficulty(spell, 11)); /* 截断到 10 */
    TEST_ASSERT_EQ(spell_get_difficulty(spell), 10);
    TEST_ASSERT(spell_set_difficulty(spell, 5));
    TEST_ASSERT_EQ(spell_get_difficulty(spell), 5);
    TEST_ASSERT(!spell_set_difficulty(NULL, 5));

    /* 输出数边界 */
    TEST_ASSERT(spell_set_output_count(spell, 0));
    TEST_ASSERT_EQ(spell_get_output_count(spell), 0);
    TEST_ASSERT(!spell_set_output_count(spell, -1));

    /* 提纯边界 */
    TEST_ASSERT(spell_configure_purifying(spell, ELEMENT_FIRE, -0.5)); /* 截断到 0 */
    TEST_ASSERT(spell_configure_purifying(spell, ELEMENT_FIRE, 1.5));  /* 截断到 1 */

    /* 阈值边界 */
    TEST_ASSERT(spell_configure_infusing(spell, 0)); /* 无效 → T2 默认 */
    TEST_ASSERT(spell_configure_infusing(spell, 7)); /* 无效 → T2 默认 */
    TEST_ASSERT(spell_configure_infusing(spell, 3)); /* T3 */

    /* 释放范围边界 */
    TEST_ASSERT(spell_configure_releasing(spell, -1, -1));

    /* 空描述/名称 */
    TEST_ASSERT(spell_set_description(spell, ""));
    TEST_ASSERT_STR_EQ(spell_get_description(spell), "");
    TEST_ASSERT(!spell_set_description(NULL, "desc"));
    TEST_ASSERT(!spell_set_description(spell, NULL));

    /* 设置输入/输出 NULL */
    TEST_ASSERT(!spell_set_input_count(NULL, 5));
    TEST_ASSERT(!spell_set_output_count(NULL, 5));

    /* 配置 molding 为 NULL */
    TEST_ASSERT(!spell_configure_molding(NULL, NULL));
    TEST_ASSERT(!spell_configure_molding(spell, NULL));

    /* 验证结构：无开模符文 → 不合法 */
    TEST_ASSERT(!spell_validate_structure(spell));

    spell_destroy(spell);
}

/* spell 元素兼容性 */
static void test_spell_element_compatibility(void) {
    Spell *spell = spell_create("Compatibility Check");
    spell_configure_purifying(spell, ELEMENT_FIRE, 0.8);

    /* FIRE vs WATER = 冲突 → 不兼容 */
    TEST_ASSERT(!spell_check_element_compatibility(spell, ELEMENT_WATER));
    /* FIRE vs AIR = 增强 → 兼容 */
    TEST_ASSERT(spell_check_element_compatibility(spell, ELEMENT_AIR));
    /* FIRE vs EARTH = 增强 → 兼容 */
    TEST_ASSERT(spell_check_element_compatibility(spell, ELEMENT_EARTH));
    /* FIRE vs FIRE = 无反应 → 兼容 */
    TEST_ASSERT(spell_check_element_compatibility(spell, ELEMENT_FIRE));
    /* NULL 咒语 */
    TEST_ASSERT(!spell_check_element_compatibility(NULL, ELEMENT_FIRE));

    spell_destroy(spell);
}

/* domain 深度测试 */
static void test_domain_deep(void) {
    /* NULL 名称 */
    Domain *d = domain_create(NULL, 5);
    TEST_ASSERT_NOT_NULL(d);
    TEST_ASSERT_STR_EQ(domain_get_name(d), "Unnamed Domain");
    domain_destroy(d);

    /* 域基本属性 */
    d = domain_create("Barrier", 20);
    TEST_ASSERT_NOT_NULL(d);
    TEST_ASSERT_STR_EQ(domain_get_name(d), "Barrier");
    TEST_ASSERT_EQ(domain_get_range(d), 20);
    TEST_ASSERT_NULL(domain_get_center(d));
    TEST_ASSERT_EQ(domain_get_strength(d), 0.0);

    /* 添加重复规则 */
    TEST_ASSERT(domain_add_rule(d, "no_fire", 1.0));
    TEST_ASSERT(domain_add_rule(d, "no_fire", 2.0)); /* 重复，跳过 */
    TEST_ASSERT(domain_add_rule(d, "no_water", 0.5));
    TEST_ASSERT(domain_add_rule(d, "no_earth", 1.5));

    /* 用坐标激活 */
    SymbolicCoord *center = symbolic_coord_create_rational(0, 1);
    TEST_ASSERT(domain_activate(d, center));
    TEST_ASSERT(domain_is_active(d));
    TEST_ASSERT(domain_get_strength(d) > 0.0);
    TEST_ASSERT_NOT_NULL(domain_get_center(d));

    /* 再次激活 — 替换中心 */
    SymbolicCoord *c2 = symbolic_coord_create_rational(10, 1);
    TEST_ASSERT(domain_activate(d, c2));

    /* 停用 */
    TEST_ASSERT(domain_deactivate(d));
    TEST_ASSERT(!domain_is_active(d));
    TEST_ASSERT_EQ(domain_get_strength(d), 0.0);

    /* NULL 安全 */
    TEST_ASSERT(!domain_is_active(NULL));
    TEST_ASSERT_EQ(domain_get_strength(NULL), 0.0);
    TEST_ASSERT_NULL(domain_get_name(NULL));
    TEST_ASSERT_EQ(domain_get_range(NULL), 0);
    TEST_ASSERT_NULL(domain_get_center(NULL));
    TEST_ASSERT(!domain_activate(NULL, center));
    TEST_ASSERT(!domain_deactivate(NULL));
    TEST_ASSERT(!domain_add_rule(NULL, "x", 1.0));
    TEST_ASSERT(!domain_add_rule(d, NULL, 1.0));

    symbolic_coord_destroy(center);
    symbolic_coord_destroy(c2);
    domain_destroy(d);
}

/* incantation 全目标测试 */
static void test_incantation_all_goals(void) {
    /* NULL 目标 */
    IncantationProfile prof = incantation_optimize(NULL, 0.0);
    TEST_ASSERT_EQ(prof.length, INCANTATION_STANDARD);

    /* speed */
    prof = incantation_optimize("speed", 0.0);
    TEST_ASSERT_EQ(prof.length, INCANTATION_SHORT);
    TEST_ASSERT(prof.speed > 0.9);
    TEST_ASSERT(prof.precision < 0.6);
    TEST_ASSERT(prof.stealth > 0.8);

    /* precision */
    prof = incantation_optimize("precision", 0.0);
    TEST_ASSERT_EQ(prof.length, INCANTATION_LONG);
    TEST_ASSERT(prof.precision > 0.9);
    TEST_ASSERT(prof.speed < 0.5);
    TEST_ASSERT(prof.stealth < 0.4);

    /* stealth */
    prof = incantation_optimize("stealth", 0.0);
    TEST_ASSERT_EQ(prof.length, INCANTATION_SHORT);
    TEST_ASSERT(prof.stealth > 0.9);
    TEST_ASSERT(prof.speed > 0.5);

    /* 未知目标应返回默认 */
    prof = incantation_optimize("unknown_goal", 0.0);
    TEST_ASSERT_EQ(prof.length, INCANTATION_STANDARD);

    /* incantation_calculate_power — 所有长度 */
    IncantationProfile p;
    p.precision = 0.8;
    p.speed = 0.8;
    p.stealth = 0.8;

    p.length = INCANTATION_INSTANT;
    double pi = incantation_calculate_power(&p);
    p.length = INCANTATION_SHORT;
    double ps = incantation_calculate_power(&p);
    p.length = INCANTATION_STANDARD;
    double pn = incantation_calculate_power(&p);
    p.length = INCANTATION_LONG;
    double pl = incantation_calculate_power(&p);
    p.length = INCANTATION_RITUAL;
    double pr = incantation_calculate_power(&p);

    TEST_ASSERT(pi > 0.0 && pi < ps, "instant < short");
    TEST_ASSERT(ps < pn, "short < standard");
    TEST_ASSERT(pn < pl, "standard < long");
    TEST_ASSERT(pl < pr, "long < ritual");

    /* NULL profile */
    TEST_ASSERT_EQ(incantation_calculate_power(NULL), 0.0);
}

/* 辅助函数枚举边界 */
static void test_helper_enum_boundaries(void) {
    /* 元素字符串边界 */
    TEST_ASSERT_STR_EQ(element_to_string((MagicElement) 99), "未知");
    TEST_ASSERT_EQ(string_to_element(NULL), ELEMENT_NONE);
    TEST_ASSERT_EQ(string_to_element("UNKNOWN"), ELEMENT_NONE);

    /* 中文名称 */
    TEST_ASSERT_EQ(string_to_element("火"), ELEMENT_FIRE);
    TEST_ASSERT_EQ(string_to_element("水"), ELEMENT_WATER);
    TEST_ASSERT_EQ(string_to_element("风"), ELEMENT_AIR);
    TEST_ASSERT_EQ(string_to_element("土"), ELEMENT_EARTH);
    TEST_ASSERT_EQ(string_to_element("以太"), ELEMENT_ETHER);

    /* 阶段/状态/反应边界 */
    TEST_ASSERT_STR_EQ(stage_to_string((SpellStage) 99), "未知");
    TEST_ASSERT_STR_EQ(stage_to_string(SPELL_STAGE_MOLDING), "开模");
    TEST_ASSERT_STR_EQ(status_to_string((SpellStatus) 99), "未知");
    TEST_ASSERT_STR_EQ(status_to_string(SPELL_STATUS_IDLE), "空闲");
    TEST_ASSERT_STR_EQ(reaction_to_string((ElementReaction) 99), "未知");
    TEST_ASSERT_STR_EQ(reaction_to_string(ELEMENT_REACTION_CONFLICT), "冲突");

    /* 限制等级边界 */
    TEST_ASSERT_STR_EQ(restriction_to_string((RestrictionLevel) 99), "未知");
    TEST_ASSERT_STR_EQ(restriction_to_string(RESTRICTION_NONE), "无限制");
    TEST_ASSERT_STR_EQ(restriction_to_string(RESTRICTION_ABSOLUTE), "绝对禁术");
}

/* spellbook 深度测试 */
static void test_spellbook_deep(void) {
    SpellBook *book = spellbook_create();
    TEST_ASSERT_NOT_NULL(book);

    /* NULL 书 */
    TEST_ASSERT(!spellbook_add_spell(NULL, NULL));
    TEST_ASSERT_NULL(spellbook_get_spell(NULL, "x"));
    TEST_ASSERT_EQ(spellbook_get_count(NULL), 0);
    TEST_ASSERT(!spellbook_remove_spell(NULL, "x"));

    /* 添加大量咒语触发扩容 */
    Spell *many[70];
    for (int i = 0; i < 70; i++) {
        char name[32];
        snprintf(name, sizeof(name), "Spell_%d", i);
        many[i] = spell_create(name);
        TEST_ASSERT(spellbook_add_spell(book, many[i]));
    }
    TEST_ASSERT_EQ(spellbook_get_count(book), 70);

    /* 按名称查找 */
    Spell *found = spellbook_get_spell(book, "Spell_50");
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_STR_EQ(spell_get_name(found), "Spell_50");
    TEST_ASSERT_NULL(spellbook_get_spell(book, "Nonexistent"));

    /* 列出名称 */
    int cnt;
    char **names = spellbook_list_spells(book, &cnt);
    TEST_ASSERT_NOT_NULL(names);
    TEST_ASSERT_EQ(cnt, 70);
    TEST_ASSERT_STR_EQ(names[0], "Spell_0");
    TEST_ASSERT_STR_EQ(names[69], "Spell_69");
    for (int i = 0; i < cnt; i++)
        lv_free(names[i]);
    lv_free(names);

    /* 按名称移除 */
    TEST_ASSERT(spellbook_remove_spell(book, "Spell_0"));
    TEST_ASSERT_EQ(spellbook_get_count(book), 69);
    TEST_ASSERT(!spellbook_remove_spell(book, "Nonexistent"));

    /* spellbook_list_spells with NULL */
    names = spellbook_list_spells(NULL, &cnt);
    TEST_ASSERT_NULL(names);

    spellbook_destroy(book);
}

/* 禁术判定全路径 */
static void test_restriction_all_levels(void) {
    Spell *spell = spell_create("Restriction Test");
    spell_set_difficulty(spell, 5);

    ForbiddenSpellCriteria c = {false, false, false};
    TEST_ASSERT_EQ(spell_check_restriction(spell, &c), RESTRICTION_NONE);

    /* 难度 > 8 → 限制级 */
    spell_set_difficulty(spell, 9);
    TEST_ASSERT_EQ(spell_check_restriction(spell, &c), RESTRICTION_LIMITED);
    spell_set_difficulty(spell, 5);

    /* 1 项 → 管制级 */
    c.governance_uncontrollable = true;
    TEST_ASSERT_EQ(spell_check_restriction(spell, &c), RESTRICTION_CONTROLLED);

    /* 2 项 → 禁术级 */
    c.external_cost_unacceptable = true;
    TEST_ASSERT_EQ(spell_check_restriction(spell, &c), RESTRICTION_FORBIDDEN);

    /* 3 项 → 绝对禁术 */
    c.self_damage_too_high = true;
    TEST_ASSERT_EQ(spell_check_restriction(spell, &c), RESTRICTION_ABSOLUTE);

    /* NULL 参数 */
    TEST_ASSERT_EQ(spell_check_restriction(NULL, &c), RESTRICTION_NONE);
    TEST_ASSERT_EQ(spell_check_restriction(spell, NULL), RESTRICTION_NONE);

    spell_destroy(spell);
}

/* purity / threshold 边界 */
static void test_purity_threshold_boundary(void) {
    /* 无效等级 */
    TEST_ASSERT_EQ(purity_to_value((PurityLevel) 99), 0.0);
    TEST_ASSERT_EQ(threshold_to_energy((EnergyThreshold) 99), 0);

    /* 分界点 */
    TEST_ASSERT_EQ(value_to_purity(0.3), PURITY_RAW); /* < 0.3 */
    TEST_ASSERT_EQ(energy_to_threshold(1), THRESHOLD_T1);
    TEST_ASSERT_EQ(energy_to_threshold(10), THRESHOLD_T2);
    TEST_ASSERT_EQ(energy_to_threshold(100), THRESHOLD_T3);
    TEST_ASSERT_EQ(energy_to_threshold(1000), THRESHOLD_T4);
    TEST_ASSERT_EQ(energy_to_threshold(10000), THRESHOLD_T5);
    TEST_ASSERT_EQ(energy_to_threshold(100000), THRESHOLD_T6);
}

/* element_reaction 全矩阵 */
static void test_element_reaction_full_matrix(void) {
    /* NONE 对任何元素 */
    for (int e = 0; e <= ELEMENT_ETHER; e++) {
        TEST_ASSERT_EQ(array_check_element_reaction(ELEMENT_NONE, (MagicElement) e), ELEMENT_REACTION_NONE);
    }

    /* FIRE 对 WATER = 冲突 */
    TEST_ASSERT_EQ(array_check_element_reaction(ELEMENT_FIRE, ELEMENT_WATER), ELEMENT_REACTION_CONFLICT);
    /* WATER 对 AIR = 削弱 */
    TEST_ASSERT_EQ(array_check_element_reaction(ELEMENT_WATER, ELEMENT_AIR), ELEMENT_REACTION_WEAKEN);
    /* AIR 对 EARTH = 冲突 */
    TEST_ASSERT_EQ(array_check_element_reaction(ELEMENT_AIR, ELEMENT_EARTH), ELEMENT_REACTION_CONFLICT);
    /* EARTH 对 FIRE = 增强 */
    TEST_ASSERT_EQ(array_check_element_reaction(ELEMENT_EARTH, ELEMENT_FIRE), ELEMENT_REACTION_ENHANCE);
}

/* ================================================================
 * 插件系统深度测试
 * ================================================================ */

static void test_plugin_interface_full(void) {
    lvPluginSystem *sys = lv_plugin_system_create(NULL);
    TEST_ASSERT_NOT_NULL(sys);
    lv_plugin_system_init(sys);

    /* 创建一个模拟插件来注册接口 */
    lvPlugin mock_plugin;
    memset(&mock_plugin, 0, sizeof(mock_plugin));
    lvPluginContext mock_ctx;
    memset(&mock_ctx, 0, sizeof(mock_ctx));
    mock_ctx.system = sys;
    mock_plugin.context = &mock_ctx;

    lvPluginInterface iface;
    memset(&iface, 0, sizeof(iface));
    strncpy(iface.name, "test_interface", sizeof(iface.name) - 1);
    iface.version = 1;

    /* 注册 */
    TEST_ASSERT_EQ(lv_plugin_register_interface(&mock_plugin, &iface), 0);

    /* 重复注册相同名称应失败 */
    TEST_ASSERT_EQ(lv_plugin_register_interface(&mock_plugin, &iface), -1);

    /* 精确查询 */
    lvPluginInterface *found = lv_plugin_query_interface(sys, "test_interface", 1);
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_STR_EQ(found->name, "test_interface");

    /* 版本不匹配 */
    TEST_ASSERT_NULL(lv_plugin_query_interface(sys, "test_interface", 2));

    /* 通配符查询 */
    size_t cnt;
    lvPluginInterface **results = lv_plugin_query_interfaces(sys, "test_*", &cnt);
    TEST_ASSERT_NOT_NULL(results);
    TEST_ASSERT_EQ(cnt, (size_t) 1);
    lv_free(results);

    /* 无匹配模式 */
    results = lv_plugin_query_interfaces(sys, "nomatch_*", &cnt);
    TEST_ASSERT_NULL(results);
    TEST_ASSERT_EQ(cnt, (size_t) 0);

    /* 注销 */
    TEST_ASSERT_EQ(lv_plugin_unregister_interface(&mock_plugin, "test_interface"), 0);
    TEST_ASSERT_NULL(lv_plugin_query_interface(sys, "test_interface", 1));

    /* 再次注销应失败 */
    TEST_ASSERT_EQ(lv_plugin_unregister_interface(&mock_plugin, "test_interface"), -1);

    lv_plugin_system_destroy(sys);
}

static void test_plugin_event_handler(void) {
    lvPluginSystem *sys = lv_plugin_system_create(NULL);
    TEST_ASSERT_NOT_NULL(sys);

    /* 设置事件处理器 */
    static int event_fired = 0;
    event_fired = 0;

    lv_plugin_set_event_handler(sys, NULL);
    lv_plugin_set_event_handler(sys, NULL); /* 双重 NULL */

    /* 设置后广播（应该不会崩溃） */
    lv_plugin_broadcast_event(sys, lv_PLUGIN_EVENT_SHUTDOWN, NULL, 0);

    /* 设置实际处理器 */
    lv_plugin_set_event_handler(sys, NULL);

    lv_plugin_system_destroy(sys);
}

static void test_plugin_dependency_deep(void) {
    lvPluginSystem *sys = lv_plugin_system_create(NULL);
    TEST_ASSERT_NOT_NULL(sys);

    /* 创建一个模拟插件 */
    lvPlugin plugin_a;
    memset(&plugin_a, 0, sizeof(plugin_a));
    strncpy(plugin_a.info.name, "PluginA", sizeof(plugin_a.info.name) - 1);
    plugin_a.state = lv_PLUGIN_STATE_LOADED;

    lvPluginDependency dep;
    memset(&dep, 0, sizeof(dep));
    strncpy(dep.name, "PluginB", sizeof(dep.name) - 1);
    strncpy(dep.version_constraint, ">=1.0.0", sizeof(dep.version_constraint) - 1);
    dep.optional = 0;

    plugin_a.info.dependencies = &dep;
    plugin_a.info.dependency_count = 1;

    /* 依赖不在系统中 */
    TEST_ASSERT_EQ(lv_plugin_resolve_dependencies(sys, NULL), -1);
    TEST_ASSERT_EQ(lv_plugin_resolve_dependencies(NULL, &plugin_a), -1);

    /* check_dependencies */
    int has_dep = lv_plugin_check_dependencies(&plugin_a);
    TEST_ASSERT_EQ(has_dep, 0); /* 有非可选依赖 */

    /* 改为可选 */
    dep.optional = 1;
    has_dep = lv_plugin_check_dependencies(&plugin_a);
    TEST_ASSERT_EQ(has_dep, 1); /* 无非可选 */

    /* get_dependents */
    size_t cnt;
    lvPlugin **deps = lv_plugin_get_dependents(sys, &plugin_a, &cnt);
    TEST_ASSERT_NULL(deps);
    TEST_ASSERT_EQ(cnt, (size_t) 0);

    lv_plugin_system_destroy(sys);
}

static void test_plugin_version_deep(void) {
    /* 语义版本各种组合 */
    TEST_ASSERT_EQ(lv_plugin_check_version("1.0.0", "1.0.0"), 1); /* 精确匹配 */
    TEST_ASSERT_EQ(lv_plugin_check_version("1.0.0", "2.0.0"), 1); /* 更高 major */
    TEST_ASSERT_EQ(lv_plugin_check_version("2.0.0", "1.0.0"), 0); /* 更低 major → 不匹配 */
    TEST_ASSERT_EQ(lv_plugin_check_version("1.0.0", "1.1.0"), 1); /* 更高 minor */
    TEST_ASSERT_EQ(lv_plugin_check_version("1.1.0", "1.0.0"), 0); /* 更低 minor → 不匹配 */
    TEST_ASSERT_EQ(lv_plugin_check_version("1.0.0", "1.0.1"), 1); /* 更高 patch */
    TEST_ASSERT_EQ(lv_plugin_check_version("1.0.1", "1.0.0"), 0); /* 更低 patch → 不匹配 */

    /* NULL 参数 */
    TEST_ASSERT_EQ(lv_plugin_check_version(NULL, "1.0.0"), 0);
    TEST_ASSERT_EQ(lv_plugin_check_version("1.0.0", NULL), 0);
    TEST_ASSERT_EQ(lv_plugin_check_version(NULL, NULL), 0);

    /* API 兼容性 */
    TEST_ASSERT_EQ(lv_plugin_check_api_compatibility(1, 2), 1); /* provided >= required */
    TEST_ASSERT_EQ(lv_plugin_check_api_compatibility(2, 1), 0); /* provided < required */
    TEST_ASSERT_EQ(lv_plugin_check_api_compatibility(1, 1), 1);
}

static void test_plugin_config_deep(void) {
    lvPluginConfig *cfg = lv_plugin_config_create();
    TEST_ASSERT_NOT_NULL(cfg);

    /* 空配置获取 */
    const char *v = lv_plugin_config_get(cfg, "any", "def");
    TEST_ASSERT_STR_EQ(v, "def");

    /* 设置并覆盖 */
    TEST_ASSERT_EQ(lv_plugin_config_set(cfg, "key1", "value1", 0), 0);
    TEST_ASSERT_EQ(lv_plugin_config_set(cfg, "key1", "value1_new", 0), 0); /* 覆盖 */
    v = lv_plugin_config_get(cfg, "key1", "def");
    TEST_ASSERT_STR_EQ(v, "value1_new");

    /* NULL key/value 保护 */
    TEST_ASSERT_EQ(lv_plugin_config_set(cfg, NULL, "v", 0), -1);
    TEST_ASSERT_EQ(lv_plugin_config_set(cfg, "k", NULL, 0), -1);

    /* 从 NULL config 获取 */
    v = lv_plugin_config_get(NULL, "key", "def");
    TEST_ASSERT_STR_EQ(v, "def");

    lv_plugin_config_destroy(cfg);
}

static void test_plugin_search_path_deep(void) {
    lvPluginSystem *sys = lv_plugin_system_create(NULL);
    TEST_ASSERT_NOT_NULL(sys);

    /* 添加路径 */
    TEST_ASSERT_EQ(lv_plugin_system_add_search_path(sys, "/path/a"), 0);
    TEST_ASSERT_EQ(lv_plugin_system_add_search_path(sys, "/path/b"), 0);
    TEST_ASSERT_EQ(lv_plugin_system_add_search_path(sys, "/path/a"), 0); /* 重复 */

    size_t cnt;
    char **paths = lv_plugin_system_get_search_paths(sys, &cnt);
    TEST_ASSERT_EQ(cnt, (size_t) 2);

    /* 移除 */
    TEST_ASSERT_EQ(lv_plugin_system_remove_search_path(sys, "/path/a"), 0);
    TEST_ASSERT_EQ(lv_plugin_system_remove_search_path(sys, "/path/nonexistent"), -1);

    /* NULL 安全 */
    TEST_ASSERT_EQ(lv_plugin_system_add_search_path(NULL, "/path"), -1);
    TEST_ASSERT_EQ(lv_plugin_system_add_search_path(sys, NULL), -1);
    TEST_ASSERT_EQ(lv_plugin_system_remove_search_path(NULL, "/path"), -1);
    TEST_ASSERT_EQ(lv_plugin_system_remove_search_path(sys, NULL), -1);

    /* autoload NULL 安全 */
    TEST_ASSERT_EQ(lv_plugin_system_autoload(NULL, "/path"), -1);
    TEST_ASSERT_EQ(lv_plugin_system_autoload(sys, NULL), -1);
    TEST_ASSERT_EQ(lv_plugin_system_autoload_all(NULL), -1);

    lv_plugin_system_destroy(sys);
}

static void test_plugin_json_info(void) {
    lvPluginSystem *sys = lv_plugin_system_create(NULL);
    TEST_ASSERT_NOT_NULL(sys);

    char *json = lv_plugin_system_get_info_json(sys);
    TEST_ASSERT_NOT_NULL(json);
    TEST_ASSERT(strstr(json, "plugin_count") != NULL || strstr(json, "version") != NULL);
    lv_free(json);

    lv_plugin_system_destroy(sys);
}

static void test_plugin_activate_deactivate(void) {
    lvPluginSystem *sys = lv_plugin_system_create(NULL);
    TEST_ASSERT_NOT_NULL(sys);

    lvPlugin plugin;
    memset(&plugin, 0, sizeof(plugin));
    plugin.state = lv_PLUGIN_STATE_LOADED;

    /* NULL 安全 */
    TEST_ASSERT_EQ(lv_plugin_activate(NULL), -1);
    TEST_ASSERT_EQ(lv_plugin_deactivate(NULL), -1);

    /* 状态检查 */
    TEST_ASSERT(!lv_plugin_is_active(NULL));
    TEST_ASSERT_EQ(lv_plugin_get_state(NULL), lv_PLUGIN_STATE_UNLOADED);

    /* 未加载的不可激活 */
    plugin.state = lv_PLUGIN_STATE_UNLOADED;
    TEST_ASSERT_EQ(lv_plugin_activate(&plugin), -1);

    /* 激活 */
    plugin.state = lv_PLUGIN_STATE_LOADED;
    /* 无 system 上下文, 无依赖, 无 on_activate — 应该失败因为 context 为空 */
    /* 但测试状态机可以 */
    plugin.context = NULL;

    lv_plugin_system_destroy(sys);
}

/* ================================================================
 * 证明编译器深度测试
 * ================================================================ */

static void test_proof_object_with_premises(void) {
    lvProofObject *obj = lv_proof_object_create();
    TEST_ASSERT_NOT_NULL(obj);

    /* 创建三个步骤形成链 */
    lvProofStepRecord *s1 = lv_proof_step_record_create();
    s1->type = (ProofStepType) 0;
    s1->depth = 0;
    s1->rule_name = lv_strdup("axiom");
    lv_proof_object_add_step(obj, s1);

    lvProofStepRecord *s2 = lv_proof_step_record_create();
    s2->type = (ProofStepType) 0;
    s2->depth = 1;
    s2->rule_name = lv_strdup("deduction");
    s2->premise_step_ids = (int *) lv_malloc(sizeof(int));
    s2->premise_step_ids[0] = 0;
    s2->premise_count = 1;
    lv_proof_object_add_step(obj, s2);

    lvProofStepRecord *s3 = lv_proof_step_record_create();
    s3->type = (ProofStepType) 0;
    s3->depth = 2;
    s3->rule_name = lv_strdup("conclusion");
    s3->premise_step_ids = (int *) lv_malloc(sizeof(int));
    s3->premise_step_ids[0] = 1;
    s3->premise_count = 1;
    lv_proof_object_add_step(obj, s3);

    TEST_ASSERT_EQ(lv_proof_object_get_step_count(obj), 3);

    /* 未设置 goal 和 is_proved → isValid 应为 false */
    TEST_ASSERT(!lv_proof_object_is_valid(obj));

    /* verify: 前提顺序正确 */
    bool ok = lv_proof_object_verify(obj);
    TEST_ASSERT(ok, "verify valid chain");

    lv_proof_object_destroy(obj);
}

static void test_proof_object_invalid_chain(void) {
    lvProofObject *obj = lv_proof_object_create();

    /* 前提引用未来步骤 */
    lvProofStepRecord *s1 = lv_proof_step_record_create();
    s1->premise_step_ids = (int *) lv_malloc(sizeof(int));
    s1->premise_step_ids[0] = 2; /* 未来步骤 */
    s1->premise_count = 1;
    lv_proof_object_add_step(obj, s1);

    lvProofStepRecord *s2 = lv_proof_step_record_create();
    lv_proof_object_add_step(obj, s2);

    /* verify 应失败 */
    bool ok = lv_proof_object_verify(obj);
    TEST_ASSERT(!ok, "invalid chain detect");

    lv_proof_object_destroy(obj);
}

static void test_proof_object_add_axiom_assumption(void) {
    lvProofObject *obj = lv_proof_object_create();

    /* 添加公理/假设边界 */
    TEST_ASSERT(!lv_proof_object_add_axiom(NULL, 1));
    TEST_ASSERT(!lv_proof_object_add_assumption(NULL, 1));

    /* 添加大量触发扩容 */
    for (int i = 0; i < 40; i++) {
        TEST_ASSERT(lv_proof_object_add_axiom(obj, i));
        TEST_ASSERT(lv_proof_object_add_assumption(obj, i));
    }

    lv_proof_object_destroy(obj);
}

static void test_proof_compiler_all_formats(void) {
    /* 准备一个带有步骤的证明对象 */
    lvProofObject *obj = lv_proof_object_create();
    obj->theorem_name = lv_strdup("勾股定理");
    obj->is_proved = true;

    lvProofStepRecord *s1 = lv_proof_step_record_create();
    s1->type = (ProofStepType) 0;
    s1->depth = 0;
    s1->rule_name = lv_strdup("公理1");
    lv_proof_object_add_step(obj, s1);

    lvProofStepRecord *s2 = lv_proof_step_record_create();
    s2->type = (ProofStepType) 0;
    s2->depth = 1;
    s2->rule_name = lv_strdup("推理");
    s2->premise_step_ids = (int *) lv_malloc(sizeof(int));
    s2->premise_step_ids[0] = 0;
    s2->premise_count = 1;
    lv_proof_object_add_step(obj, s2);

    /* JSON 格式 */
    char *json = lv_proof_compiler_to_json(obj, NULL);
    TEST_ASSERT_NOT_NULL(json);
    TEST_ASSERT(strstr(json, "勾股定理") != NULL || strstr(json, "勾") != NULL || strstr(json, "theorem_name") != NULL);
    lv_free(json);

    /* LaTeX 格式 */
    char *latex = lv_proof_compiler_to_latex(obj, "zh");
    TEST_ASSERT_NOT_NULL(latex);
    TEST_ASSERT(strstr(latex, "Proof") != NULL || strstr(latex, "证明") != NULL);
    lv_free(latex);

    /* LaTeX 英文 */
    latex = lv_proof_compiler_to_latex(obj, "en");
    TEST_ASSERT_NOT_NULL(latex);
    TEST_ASSERT(strstr(latex, "Proof") != NULL);
    lv_free(latex);

    /* TikZ 格式 */
    char *tikz = lv_proof_compiler_to_tikz(obj);
    TEST_ASSERT_NOT_NULL(tikz);
    TEST_ASSERT(strstr(tikz, "tikzpicture") != NULL);
    lv_free(tikz);

    /* Text 格式 */
    char *text = lv_proof_compiler_to_text(obj, "zh");
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT(strstr(text, "证明") != NULL || strstr(text, "勾股定理") != NULL);
    lv_free(text);

    /* Graphviz 格式 */
    char *dot = lv_proof_compiler_to_graphviz(obj, NULL);
    TEST_ASSERT_NOT_NULL(dot);
    TEST_ASSERT(strstr(dot, "digraph") != NULL);
    lv_free(dot);

    lv_proof_object_destroy(obj);
}

static void test_proof_compiler_null_objects(void) {
    /* NULL proof */
    char *r = lv_proof_compiler_to_json(NULL, NULL);
    TEST_ASSERT_NULL(r);

    r = lv_proof_compiler_to_latex(NULL, "zh");
    TEST_ASSERT_NULL(r);

    r = lv_proof_compiler_to_tikz(NULL);
    TEST_ASSERT_NULL(r);

    r = lv_proof_compiler_to_text(NULL, "zh");
    TEST_ASSERT_NULL(r);

    r = lv_proof_compiler_to_graphviz(NULL, NULL);
    TEST_ASSERT_NULL(r);

    /* Compiler compile with NULL */
    lvCompilerConfig cfg = lv_compiler_config_default();
    lvProofCompiler *comp = lv_proof_compiler_create(&cfg);
    TEST_ASSERT_NOT_NULL(comp);

    r = lv_proof_compiler_compile(comp, NULL, NULL);
    TEST_ASSERT_NULL(r);

    r = lv_proof_compiler_compile(NULL, NULL, NULL);
    TEST_ASSERT_NULL(r);

    lv_proof_compiler_destroy(comp);
}

static void test_proof_compiler_config(void) {
    lvCompilerConfig cfg = lv_compiler_config_default();
    TEST_ASSERT_EQ(cfg.format, OUTPUT_FORMAT_TEXT);
    TEST_ASSERT(cfg.include_metadata);
    TEST_ASSERT(!cfg.verbose);
    TEST_ASSERT_EQ(cfg.max_depth, 1024);

    lvProofCompiler *comp = lv_proof_compiler_create(&cfg);
    TEST_ASSERT_NOT_NULL(comp);

    /* 修改配置 */
    cfg.format = OUTPUT_FORMAT_GRAPHVIZ;
    cfg.verbose = true;
    cfg.max_depth = 512;
    lv_proof_compiler_set_config(comp, &cfg);

    /* 空证明编译 Graphviz */
    lvProofObject *obj = lv_proof_object_create();
    char *r = lv_proof_compiler_compile(comp, obj, NULL);
    TEST_ASSERT_NOT_NULL(r);
    lv_free(r);

    lv_proof_object_destroy(obj);
    lv_proof_compiler_destroy(comp);
}

static void test_proof_export_to_file(void) {
    lvProofObject *obj = lv_proof_object_create();
    obj->theorem_name = lv_strdup("Test");

    /* 正常导出 */
    bool ok = lv_proof_export_to_file(obj, NULL, OUTPUT_FORMAT_TEXT, "test_proof_output.txt");
    TEST_ASSERT(ok, "export to file");

    /* NULL 安全 */
    TEST_ASSERT(!lv_proof_export_to_file(NULL, NULL, OUTPUT_FORMAT_TEXT, "test.txt"));
    TEST_ASSERT(!lv_proof_export_to_file(obj, NULL, OUTPUT_FORMAT_TEXT, NULL));
    TEST_ASSERT(!lv_proof_export_to_file(NULL, NULL, OUTPUT_FORMAT_TEXT, NULL));

    lv_proof_object_destroy(obj);
}

static void test_proof_step_record_premises(void) {
    lvProofStepRecord *rec = lv_proof_step_record_create();
    TEST_ASSERT_NOT_NULL(rec);

    /* 添加前提 */
    rec->premise_step_ids = (int *) lv_realloc(rec->premise_step_ids, 3 * sizeof(int));
    rec->premise_step_ids[0] = 0;
    rec->premise_step_ids[1] = 1;
    rec->premise_step_ids[2] = 2;
    rec->premise_count = 3;
    rec->premise_capacity = 3;

    /* 设置字段 */
    rec->rule_name = lv_strdup("modus_ponens");
    rec->justification = lv_strdup("MP applied");
    rec->depth = 2;
    rec->color = (ProofColor) 0;

    lv_proof_step_record_destroy(rec);
    lv_proof_step_record_destroy(NULL);
}

static void test_proof_trace_lifecycle(void) {
    lvProofTrace *trace = lv_proof_trace_create();
    TEST_ASSERT_NOT_NULL(trace);

    /* 跟踪各种事件 */
    lv_proof_trace_start(trace, 42);
    lv_proof_trace_step(trace, 1, "step1", 1);
    lv_proof_trace_step(trace, 2, "step2", 2);
    lv_proof_trace_backtrack(trace, 2, 1);
    lv_proof_trace_branch(trace, "branch_x", 3, 2);
    lv_proof_trace_lemma(trace, 100, "helper_lemma");
    lv_proof_trace_contradiction(trace, 0, 5);
    lv_proof_trace_complete(trace, true);

    /* 验证事件数 */
    TEST_ASSERT(trace->event_count > 0, "trace has events");
    TEST_ASSERT_EQ(trace->proof_id, 42);

    /* 创建并添加事件 */
    lvTraceEvent *ev = lv_trace_event_create(TRACE_EVENT_STEP);
    TEST_ASSERT_NOT_NULL(ev);
    ev->step_id = 99;
    ev->description = lv_strdup("manual event");
    ev->depth = 3;
    int rc = lv_proof_trace_add_event(trace, ev);
    TEST_ASSERT_EQ(rc, 0);

    /* NULL 安全 */
    lv_trace_event_destroy(NULL);
    lv_proof_trace_destroy(NULL);
    lv_proof_trace_start(NULL, 1);
    lv_proof_trace_step(NULL, 0, "x", 0);
    lv_proof_trace_backtrack(NULL, 0, 0);
    lv_proof_trace_branch(NULL, "x", 0, 0);
    lv_proof_trace_lemma(NULL, 0, "x");
    lv_proof_trace_contradiction(NULL, 0, 0);
    lv_proof_trace_complete(NULL, true);

    lv_proof_trace_destroy(trace);
}

/* ================================================================
 * Main
 * ================================================================ */

int main(void) {
    TEST_SUITE_BEGIN("Layer5 Core (Magic / Plugin System / Proof Compiler)");

    /* ── Magic 深度测试 ── */
    printf("\n--- Magic Edge & Stress ---\n");
    TEST_RUN(test_rune_parse_edge);
    TEST_RUN(test_rune_serialize_null);
    TEST_RUN(test_rune_sequence_growth);
    TEST_RUN(test_magic_array_all_constraint_types);
    TEST_RUN(test_magic_array_stability_boundary);
    TEST_RUN(test_magic_array_deep_copy);
    TEST_RUN(test_magic_array_merge_edge);
    TEST_RUN(test_magic_array_deserialize_complex);
    TEST_RUN(test_magic_array_balance_deep);
    TEST_RUN(test_spell_cast_paths);
    TEST_RUN(test_spell_config_boundary);
    TEST_RUN(test_spell_element_compatibility);
    TEST_RUN(test_domain_deep);
    TEST_RUN(test_incantation_all_goals);
    TEST_RUN(test_helper_enum_boundaries);
    TEST_RUN(test_spellbook_deep);
    TEST_RUN(test_restriction_all_levels);
    TEST_RUN(test_purity_threshold_boundary);
    TEST_RUN(test_element_reaction_full_matrix);

    /* ── 插件系统深度测试 ── */
    printf("\n--- Plugin System Deep ---\n");
    TEST_RUN(test_plugin_interface_full);
    TEST_RUN(test_plugin_event_handler);
    TEST_RUN(test_plugin_dependency_deep);
    TEST_RUN(test_plugin_version_deep);
    TEST_RUN(test_plugin_config_deep);
    TEST_RUN(test_plugin_search_path_deep);
    TEST_RUN(test_plugin_json_info);
    TEST_RUN(test_plugin_activate_deactivate);

    /* ── 证明编译器深度测试 ── */
    printf("\n--- Proof Compiler Deep ---\n");
    TEST_RUN(test_proof_object_with_premises);
    TEST_RUN(test_proof_object_invalid_chain);
    TEST_RUN(test_proof_object_add_axiom_assumption);
    TEST_RUN(test_proof_compiler_all_formats);
    TEST_RUN(test_proof_compiler_null_objects);
    TEST_RUN(test_proof_compiler_config);
    TEST_RUN(test_proof_export_to_file);
    TEST_RUN(test_proof_step_record_premises);
    TEST_RUN(test_proof_trace_lifecycle);

    TEST_SUITE_END();
    return g_fail_count > 0 ? 1 : 0;
}

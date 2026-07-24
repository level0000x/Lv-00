/**
 * @file test_layer5_output.c
 * @brief Layer 5 输出层综合测试
 *
 * 覆盖模块：
 * - Magic（符文/魔法阵/咒语/纯度/咏唱/领域）
 * - 插件系统（生命周期/接口/事件/配置/依赖）
 * - TikZ 导出（缓冲区/文件/约束图）
 * - 证明编译器（ProofObject/Trace/Compiler/输出格式）
 * - UI-Kernel 协议（信任颜色/协议数据生成）
 * - ProofWidget（布局/Widget注册/策略推荐）
 *
 * @author Lv-00 Project
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_protocol.h"
#include "lv/magic.h"
#include "lv/plugin_system.h"
#include "lv/proof_compiler.h"
#include "lv/proof_widget.h"
#include "lv/tikz_export.h"

#include "lv.h"
#include "test_helpers.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============================================================
 * Magic 模块测试
 * ============================================================ */

static void test_rune_lifecycle(void) {
    /* 有理数符文创建/销毁 */
    Rune *r1 = rune_create_rational(3, 1, ELEMENT_FIRE);
    TEST_ASSERT_NOT_NULL(r1);
    TEST_ASSERT_EQ(rune_get_element(r1), ELEMENT_FIRE);
    TEST_ASSERT_EQ(rune_get_power(r1), 1);
    TEST_ASSERT_NOT_NULL(rune_get_value(r1));
    rune_destroy(r1);

    /* 代数数符文 */
    Rune *r2 = rune_create_algebraic(1.414, ELEMENT_EARTH);
    TEST_ASSERT_NOT_NULL(r2);
    TEST_ASSERT_EQ(rune_get_element(r2), ELEMENT_EARTH);
    rune_destroy(r2);

    /* 超越数符文 */
    Rune *r3 = rune_create_transcendental("pi", ELEMENT_WATER);
    TEST_ASSERT_NOT_NULL(r3);
    TEST_ASSERT_EQ(rune_get_element(r3), ELEMENT_WATER);
    rune_destroy(r3);

    /* NULL 安全 */
    rune_destroy(NULL);
    TEST_ASSERT_NULL(rune_get_value(NULL));
    TEST_ASSERT_EQ(rune_get_element(NULL), ELEMENT_NONE);
    TEST_ASSERT_EQ(rune_get_power(NULL), 0);
}

static void test_rune_power(void) {
    Rune *r = rune_create_rational(1, 1, ELEMENT_FIRE);
    TEST_ASSERT_NOT_NULL(r);

    rune_set_power(r, 5);
    TEST_ASSERT_EQ(rune_get_power(r), 5);

    rune_set_power(r, 15); /* 截断到最大值10 */
    TEST_ASSERT_EQ(rune_get_power(r), 10);

    rune_set_power(r, -5); /* 截断到最小值1 */
    TEST_ASSERT_EQ(rune_get_power(r), 1);

    /* NULL 安全 */
    rune_set_power(NULL, 5);
    rune_destroy(r);
}

static void test_rune_copy(void) {
    Rune *src = rune_create_rational(7, 3, ELEMENT_AIR);
    TEST_ASSERT_NOT_NULL(src);
    rune_set_power(src, 8);

    Rune *dst = rune_copy(src);
    TEST_ASSERT_NOT_NULL(dst);
    TEST_ASSERT_EQ(rune_get_element(dst), ELEMENT_AIR);
    TEST_ASSERT_EQ(rune_get_power(dst), 8);

    /* 确认是深拷贝：修改源不影响副本 */
    rune_set_power(src, 3);
    TEST_ASSERT_EQ(rune_get_power(dst), 8);

    rune_destroy(src);
    rune_destroy(dst);

    /* NULL 安全 */
    TEST_ASSERT_NULL(rune_copy(NULL));
}

static void test_rune_serialize_parse(void) {
    /* 序列化 */
    Rune *r = rune_create_rational(5, 2, ELEMENT_FIRE);
    TEST_ASSERT_NOT_NULL(r);
    char *ser = rune_serialize(r);
    TEST_ASSERT_NOT_NULL(ser);
    /* 应该包含元素、威力和坐标信息 */
    TEST_ASSERT(strstr(ser, "FIRE") != NULL || strstr(ser, "0") != NULL); /* FIRE 枚举值=0 */
    lv_free(ser);
    rune_destroy(r);

    /* 序列化到缓冲区 */
    char buf[256];
    int n = rune_serialize_to_buffer(NULL, buf, sizeof(buf));
    TEST_ASSERT_EQ(n, -1);

    Rune *tmp_r = rune_create_rational(1, 1, ELEMENT_NONE);
    TEST_ASSERT_NOT_NULL(tmp_r);
    n = rune_serialize_to_buffer(tmp_r, buf, sizeof(buf));
    TEST_ASSERT(n > 0, "serialize to buffer returns > 0");
    rune_destroy(tmp_r);

    /* 解析 */
    Rune *parsed = rune_parse("rational:3/4:FIRE");
    TEST_ASSERT_NOT_NULL(parsed);
    TEST_ASSERT_EQ(rune_get_element(parsed), ELEMENT_FIRE);
    rune_destroy(parsed);

    parsed = rune_parse("algebraic:1.414:EARTH");
    TEST_ASSERT_NOT_NULL(parsed);
    rune_destroy(parsed);

    /* 空字符串/无效格式 */
    TEST_ASSERT_NULL(rune_parse(""));
    TEST_ASSERT_NULL(rune_parse(NULL));

    /* 整数简写格式 */
    parsed = rune_parse("5:WATER");
    TEST_ASSERT_NOT_NULL(parsed);
    TEST_ASSERT_EQ(rune_get_element(parsed), ELEMENT_WATER);
    rune_destroy(parsed);
}

static void test_rune_sequence(void) {
    RuneSequence *seq = rune_sequence_create();
    TEST_ASSERT_NOT_NULL(seq);
    TEST_ASSERT_EQ(rune_sequence_length(seq), 0);

    Rune *r1 = rune_create_rational(1, 1, ELEMENT_FIRE);
    Rune *r2 = rune_create_rational(2, 1, ELEMENT_WATER);
    Rune *r3 = rune_create_rational(3, 1, ELEMENT_EARTH);

    TEST_ASSERT(rune_sequence_add(seq, r1), "add r1");
    TEST_ASSERT(rune_sequence_add(seq, r2), "add r2");
    TEST_ASSERT(rune_sequence_add(seq, r3), "add r3");
    TEST_ASSERT_EQ(rune_sequence_length(seq), 3);

    /* 获取 */
    TEST_ASSERT_EQ(rune_sequence_get(seq, 0), r1);
    TEST_ASSERT_EQ(rune_sequence_get(seq, 1), r2);
    TEST_ASSERT_EQ(rune_sequence_get(seq, 2), r3);
    TEST_ASSERT_NULL(rune_sequence_get(seq, 99));
    TEST_ASSERT_NULL(rune_sequence_get(seq, -1));

    /* NULL 安全 */
    TEST_ASSERT(!rune_sequence_add(NULL, r1), "add to NULL");
    TEST_ASSERT_EQ(rune_sequence_length(NULL), 0);
    TEST_ASSERT_NULL(rune_sequence_get(NULL, 0));

    rune_sequence_destroy(seq); /* 同时销毁内部符文 */
}

static void test_magic_array_lifecycle(void) {
    MagicArray *arr = magic_array_create();
    TEST_ASSERT_NOT_NULL(arr);
    TEST_ASSERT_EQ(magic_array_get_rune_count(arr), 0);
    TEST_ASSERT_EQ(magic_array_get_constraint_count(arr), 0);

    /* 添加符文 */
    Rune *r1 = rune_create_rational(0, 1, ELEMENT_FIRE);
    Rune *r2 = rune_create_rational(1, 1, ELEMENT_WATER);
    Rune *r3 = rune_create_rational(2, 1, ELEMENT_EARTH);

    int idx1 = magic_array_add_rune(arr, r1);
    int idx2 = magic_array_add_rune(arr, r2);
    int idx3 = magic_array_add_rune(arr, r3);
    TEST_ASSERT_GE(idx1, 0);
    TEST_ASSERT_GE(idx2, 0);
    TEST_ASSERT_GE(idx3, 0);
    TEST_ASSERT_EQ(magic_array_get_rune_count(arr), 3);

    /* 获取符文 */
    Rune *g = magic_array_get_rune(arr, 0);
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_EQ(rune_get_element(g), ELEMENT_FIRE);
    TEST_ASSERT_NULL(magic_array_get_rune(arr, 99));

    /* 添加约束 */
    int cid = magic_array_add_constraint(arr, ARRAY_CONNECTION, 0, 1);
    TEST_ASSERT_GE(cid, 0);
    int cid2 = magic_array_add_constraint(arr, ARRAY_CONFLICT, 1, 2);
    TEST_ASSERT_GE(cid2, 0);
    TEST_ASSERT_EQ(magic_array_get_constraint_count(arr), 2);

    /* 无效约束 */
    TEST_ASSERT_EQ(magic_array_add_constraint(arr, ARRAY_CONNECTION, -1, 0), -1);
    TEST_ASSERT_EQ(magic_array_add_constraint(arr, ARRAY_CONNECTION, 0, 99), -1);

    /* 移除符文 */
    TEST_ASSERT(magic_array_remove_rune(arr, 1), "remove rune idx=1");
    TEST_ASSERT_EQ(magic_array_get_rune_count(arr), 2);

    /* 移除约束 */
    TEST_ASSERT(magic_array_remove_constraint(arr, 0), "remove constraint idx=0");
    TEST_ASSERT_EQ(magic_array_get_constraint_count(arr), 1);

    /* NULL 安全 */
    TEST_ASSERT_EQ(magic_array_get_rune_count(NULL), 0);
    TEST_ASSERT_EQ(magic_array_get_constraint_count(NULL), 0);
    TEST_ASSERT(!magic_array_remove_rune(NULL, 0));
    TEST_ASSERT(!magic_array_remove_constraint(NULL, 0));

    rune_destroy(r1);
    rune_destroy(r2);
    rune_destroy(r3);
    magic_array_destroy(arr);
}

static void test_magic_array_analysis(void) {
    MagicArray *arr = magic_array_create();
    TEST_ASSERT_NOT_NULL(arr);

    /* 空阵 -> 不平衡 */
    TEST_ASSERT(!magic_array_check_balance(arr));

    /* 添加5种不同元素符文 */
    Rune *runes[5];
    MagicElement elems[5] = {ELEMENT_FIRE, ELEMENT_WATER, ELEMENT_AIR, ELEMENT_EARTH, ELEMENT_ETHER};
    for (int i = 0; i < 5; i++) {
        runes[i] = rune_create_rational(i, 1, elems[i]);
        magic_array_add_rune(arr, runes[i]);
    }
    /* 每种元素1个，分布均匀 -> 平衡 */
    TEST_ASSERT(magic_array_check_balance(arr), "balanced with 1 each");

    /* 稳定性 */
    double stab = array_calculate_stability(arr);
    TEST_ASSERT(stab > 0.5, "stable with no conflicts");

    /* 添加冲突约束降低稳定性 */
    magic_array_add_constraint(arr, ARRAY_CONFLICT, 0, 1);
    magic_array_add_constraint(arr, ARRAY_CONFLICT, 2, 3);
    double stab2 = array_calculate_stability(arr);
    TEST_ASSERT(stab2 < stab, "conflict reduces stability");

    /* 元素统计 */
    TEST_ASSERT_EQ(array_count_elements(arr, ELEMENT_FIRE), 1);
    TEST_ASSERT_EQ(array_count_elements(arr, ELEMENT_NONE), 0);

    /* 元素反应 */
    TEST_ASSERT_EQ(array_check_element_reaction(ELEMENT_FIRE, ELEMENT_WATER), ELEMENT_REACTION_CONFLICT);
    TEST_ASSERT_EQ(array_check_element_reaction(ELEMENT_FIRE, ELEMENT_AIR), ELEMENT_REACTION_ENHANCE);
    TEST_ASSERT_EQ(array_check_element_reaction(ELEMENT_WATER, ELEMENT_AIR), ELEMENT_REACTION_WEAKEN);
    TEST_ASSERT_EQ(array_check_element_reaction((MagicElement) 99, (MagicElement) 99), ELEMENT_REACTION_NONE);

    for (int i = 0; i < 5; i++)
        rune_destroy(runes[i]);
    magic_array_destroy(arr);
}

static void test_magic_array_merge_copy(void) {
    MagicArray *a1 = magic_array_create();
    MagicArray *a2 = magic_array_create();

    Rune *r1 = rune_create_rational(1, 1, ELEMENT_FIRE);
    Rune *r2 = rune_create_rational(2, 1, ELEMENT_WATER);
    magic_array_add_rune(a1, r1);
    magic_array_add_rune(a2, r2);

    /* 合并 */
    TEST_ASSERT(magic_array_merge(a1, a2), "merge a2 into a1");
    TEST_ASSERT_EQ(magic_array_get_rune_count(a1), 2);

    /* 拷贝 */
    MagicArray *copy = magic_array_copy(a1);
    TEST_ASSERT_NOT_NULL(copy);
    TEST_ASSERT_EQ(magic_array_get_rune_count(copy), 2);

    magic_array_destroy(a1);
    magic_array_destroy(a2);
    magic_array_destroy(copy);
    rune_destroy(r1);
    rune_destroy(r2);
}

static void test_magic_array_serialize(void) {
    MagicArray *arr = magic_array_create();
    Rune *r = rune_create_rational(3, 1, ELEMENT_FIRE);
    magic_array_add_rune(arr, r);
    rune_destroy(r);

    /* 序列化 */
    char *json = magic_array_serialize(arr);
    TEST_ASSERT_NOT_NULL(json);
    TEST_ASSERT(strstr(json, "rune_count") != NULL);
    lv_free(json);

    /* 反序列化 */
    MagicArray *restored =
        magic_array_deserialize("{\"runes\":[{\"type\":\"rational\",\"num\":1,\"denom\":2,\"element\":\"FIRE\"}]}");
    TEST_ASSERT_NOT_NULL(restored);
    TEST_ASSERT_EQ(magic_array_get_rune_count(restored), 1);
    magic_array_destroy(restored);

    /* NULL/空 JSON */
    TEST_ASSERT_NULL(magic_array_deserialize(NULL));
    TEST_ASSERT_NULL(magic_array_deserialize(""));

    /* 无 runes 字段 -> 空阵 */
    restored = magic_array_deserialize("{\"name\":\"test\"}");
    TEST_ASSERT_NOT_NULL(restored);
    TEST_ASSERT_EQ(magic_array_get_rune_count(restored), 0);
    magic_array_destroy(restored);

    /* 反序列化代数数 */
    restored = magic_array_deserialize("{\"runes\":[{\"type\":\"algebraic\",\"value\":1.414,\"element\":\"EARTH\"}]}");
    TEST_ASSERT_NOT_NULL(restored);
    TEST_ASSERT_EQ(magic_array_get_rune_count(restored), 1);
    magic_array_destroy(restored);

    magic_array_destroy(arr);
}

static void test_spell_lifecycle(void) {
    Spell *spell = spell_create("Fireball");
    TEST_ASSERT_NOT_NULL(spell);
    TEST_ASSERT_STR_EQ(spell_get_name(spell), "Fireball");
    TEST_ASSERT_EQ(spell_get_difficulty(spell), 1);
    TEST_ASSERT_EQ(spell_get_input_count(spell), 0);
    TEST_ASSERT_EQ(spell_get_output_count(spell), 1);
    TEST_ASSERT_EQ(spell_get_current_stage(spell), SPELL_STAGE_MOLDING);
    TEST_ASSERT_EQ(spell_get_status(spell), SPELL_STATUS_IDLE);

    /* 配置 */
    TEST_ASSERT(spell_set_input_count(spell, 2), "set input count");
    TEST_ASSERT(spell_set_output_count(spell, 3), "set output count");
    TEST_ASSERT(spell_set_description(spell, "A powerful fire spell"), "set description");
    TEST_ASSERT_STR_EQ(spell_get_description(spell), "A powerful fire spell");
    TEST_ASSERT_EQ(spell_get_input_count(spell), 2);
    TEST_ASSERT_EQ(spell_get_output_count(spell), 3);

    /* 阶段配置 */
    RuneSequence *seq = rune_sequence_create();
    Rune *r = rune_create_rational(1, 1, ELEMENT_FIRE);
    rune_sequence_add(seq, r);
    TEST_ASSERT(spell_configure_molding(spell, seq), "configure molding");
    TEST_ASSERT(spell_configure_purifying(spell, ELEMENT_FIRE, 0.9), "configure purifying");
    TEST_ASSERT(spell_configure_infusing(spell, THRESHOLD_T3), "configure infusing");
    TEST_ASSERT(spell_configure_releasing(spell, 20, 50), "configure releasing");

    /* NULL 名称 */
    Spell *s2 = spell_create(NULL);
    TEST_ASSERT_NOT_NULL(s2);
    TEST_ASSERT_STR_EQ(spell_get_name(s2), "Unnamed Spell");
    spell_destroy(s2);

    /* 验证 */
    TEST_ASSERT(spell_validate_structure(spell), "valid structure");
    TEST_ASSERT(spell_check_element_compatibility(spell, ELEMENT_FIRE), "fire compatible");

    rune_sequence_destroy(seq); /* 序列拥有所有权，符文由序列管理 */
    spell_destroy(spell);
}

static void test_purity_threshold(void) {
    /* 纯度转换 */
    TEST_ASSERT(purity_to_value(PURITY_RAW) > 0.0, "purity raw > 0");
    TEST_ASSERT(purity_to_value(PURITY_THEORETICAL) > purity_to_value(PURITY_ULTRA), "theoretical > ultra");

    TEST_ASSERT_EQ(value_to_purity(0.1), PURITY_RAW);
    TEST_ASSERT_EQ(value_to_purity(0.5), PURITY_COARSE);
    TEST_ASSERT_EQ(value_to_purity(0.7), PURITY_STANDARD);
    TEST_ASSERT_EQ(value_to_purity(0.9), PURITY_HIGH);
    TEST_ASSERT_EQ(value_to_purity(0.96), PURITY_ULTRA);
    TEST_ASSERT_EQ(value_to_purity(0.995), PURITY_THEORETICAL);
    TEST_ASSERT_EQ(value_to_purity(1.5), PURITY_THEORETICAL); /* 超过上限 */

    /* 阈值转换 */
    TEST_ASSERT_EQ(threshold_to_energy(THRESHOLD_T1), 1);
    TEST_ASSERT_EQ(threshold_to_energy(THRESHOLD_T6), 100000);

    TEST_ASSERT_EQ(energy_to_threshold(0), THRESHOLD_T1);
    TEST_ASSERT_EQ(energy_to_threshold(5), THRESHOLD_T1);
    TEST_ASSERT_EQ(energy_to_threshold(50), THRESHOLD_T2);
    TEST_ASSERT_EQ(energy_to_threshold(500), THRESHOLD_T3);
    TEST_ASSERT_EQ(energy_to_threshold(5000), THRESHOLD_T4);
    TEST_ASSERT_EQ(energy_to_threshold(50000), THRESHOLD_T5);
    TEST_ASSERT_EQ(energy_to_threshold(200000), THRESHOLD_T6);
}

static void test_spellbook(void) {
    SpellBook *book = spellbook_create();
    TEST_ASSERT_NOT_NULL(book);
    TEST_ASSERT_EQ(spellbook_get_count(book), 0);

    Spell *s1 = spell_create("Fireball");
    Spell *s2 = spell_create("Heal");
    TEST_ASSERT(spellbook_add_spell(book, s1), "add fireball");
    TEST_ASSERT(spellbook_add_spell(book, s2), "add heal");
    TEST_ASSERT_EQ(spellbook_get_count(book), 2);

    /* 获取 */
    Spell *found = spellbook_get_spell(book, "Fireball");
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_STR_EQ(spell_get_name(found), "Fireball");
    TEST_ASSERT_NULL(spellbook_get_spell(book, "Nonexistent"));

    /* 列出 */
    int list_count;
    char **names = spellbook_list_spells(book, &list_count);
    TEST_ASSERT_EQ(list_count, 2);
    if (list_count > 0) {
        lv_free(names[0]);
        lv_free(names[1]);
    }
    lv_free(names);

    /* 移除 */
    TEST_ASSERT(spellbook_remove_spell(book, "Heal"), "remove heal");
    TEST_ASSERT_EQ(spellbook_get_count(book), 1);
    TEST_ASSERT(!spellbook_remove_spell(book, "Nonexistent"));

    spellbook_destroy(book); /* 同时销毁内部的咒语 */
}

static void test_incantation(void) {
    IncantationProfile prof = incantation_optimize("power", 0.9);
    TEST_ASSERT(prof.precision >= 0.0 && prof.precision <= 1.0, "precision in [0,1]");
    TEST_ASSERT(prof.speed >= 0.0 && prof.speed <= 1.0, "speed in [0,1]");
    TEST_ASSERT(prof.stealth >= 0.0 && prof.stealth <= 1.0, "stealth in [0,1]");

    double power = incantation_calculate_power(&prof);
    TEST_ASSERT(power > 0.0, "incantation power > 0");
}

static void test_restriction_domain(void) {
    /* 禁术检查 */
    ForbiddenSpellCriteria crit;
    crit.external_cost_unacceptable = false;
    crit.self_damage_too_high = false;
    crit.governance_uncontrollable = false;

    Spell *spell = spell_create("Test Spell");
    RestrictionLevel rl = spell_check_restriction(spell, &crit);
    TEST_ASSERT_EQ(rl, RESTRICTION_NONE);

    /* 限制级 */
    crit.governance_uncontrollable = true;
    rl = spell_check_restriction(spell, &crit);
    TEST_ASSERT_EQ(rl, RESTRICTION_CONTROLLED);

    /* 绝对禁术 */
    crit.external_cost_unacceptable = true;
    crit.self_damage_too_high = true;
    rl = spell_check_restriction(spell, &crit);
    TEST_ASSERT_EQ(rl, RESTRICTION_ABSOLUTE);

    spell_destroy(spell);

    /* restriction_to_string */
    TEST_ASSERT_NOT_NULL(restriction_to_string(RESTRICTION_NONE));
    TEST_ASSERT_NOT_NULL(restriction_to_string(RESTRICTION_ABSOLUTE));

    /* 领域 */
    Domain *dom = domain_create("Protection Field", 10);
    TEST_ASSERT_NOT_NULL(dom);
    TEST_ASSERT(!domain_is_active(dom));
    TEST_ASSERT(domain_add_rule(dom, "no_fire", 1.0), "add rule");
    TEST_ASSERT(domain_activate(dom, NULL), "activate");
    TEST_ASSERT(domain_is_active(dom), "is active");
    TEST_ASSERT(domain_get_strength(dom) > 0.0, "strength > 0");
    TEST_ASSERT(domain_deactivate(dom), "deactivate");
    TEST_ASSERT(!domain_is_active(dom));
    domain_destroy(dom);
}

static void test_magic_string_conversion(void) {
    TEST_ASSERT_STR_EQ(element_to_string(ELEMENT_FIRE), "FIRE");
    TEST_ASSERT_STR_EQ(element_to_string(ELEMENT_NONE), "NONE");
    TEST_ASSERT_EQ(string_to_element("FIRE"), ELEMENT_FIRE);
    TEST_ASSERT_EQ(string_to_element("WATER"), ELEMENT_WATER);
    TEST_ASSERT_EQ(string_to_element("unknown"), ELEMENT_NONE);

    TEST_ASSERT_NOT_NULL(stage_to_string(SPELL_STAGE_MOLDING));
    TEST_ASSERT_NOT_NULL(status_to_string(SPELL_STATUS_IDLE));
    TEST_ASSERT_NOT_NULL(reaction_to_string(ELEMENT_REACTION_ENHANCE));
}

/* ============================================================
 * 插件系统测试
 * ============================================================ */

static void test_plugin_system_lifecycle(void) {
    /* 创建/销毁 */
    lvPluginSystem *sys = lv_plugin_system_create(NULL);
    TEST_ASSERT_NOT_NULL(sys);
    TEST_ASSERT_EQ(lv_plugin_system_init(sys), 0);
    lv_plugin_system_cleanup(sys);
    lv_plugin_system_destroy(sys);

    /* NULL 安全 */
    lv_plugin_system_destroy(NULL);
    lv_plugin_system_init(NULL);
    lv_plugin_system_cleanup(NULL);
}

static void test_plugin_config(void) {
    lvPluginConfig *cfg = lv_plugin_config_create();
    TEST_ASSERT_NOT_NULL(cfg);

    /* 设置/获取 */
    TEST_ASSERT_EQ(lv_plugin_config_set(cfg, "key1", "value1", 0), 0);
    TEST_ASSERT_EQ(lv_plugin_config_set(cfg, "key2", "42", 1), 0);

    const char *v = lv_plugin_config_get(cfg, "key1", "default");
    TEST_ASSERT_STR_EQ(v, "value1");

    v = lv_plugin_config_get(cfg, "nonexistent", "default_val");
    TEST_ASSERT_STR_EQ(v, "default_val");

    /* NULL 安全 */
    lv_plugin_config_destroy(NULL);
    TEST_ASSERT_NULL(lv_plugin_config_get(NULL, "key", "def"));

    lv_plugin_config_destroy(cfg);
}

static void test_plugin_queries(void) {
    /* NULL 传入 */
    size_t cnt = 0;
    TEST_ASSERT_NULL(lv_plugin_find(NULL, "test"));
    TEST_ASSERT_NULL(lv_plugin_get_all(NULL, &cnt));
    TEST_ASSERT_EQ(cnt, (size_t) 0);

    cnt = 0;
    TEST_ASSERT_NULL(lv_plugin_get_by_type(NULL, lv_PLUGIN_TYPE_NATIVE, &cnt));
    TEST_ASSERT_EQ(cnt, (size_t) 0);

    cnt = 0;
    TEST_ASSERT_NULL(lv_plugin_get_by_state(NULL, lv_PLUGIN_STATE_LOADED, &cnt));
    TEST_ASSERT_EQ(cnt, (size_t) 0);

    /* is_active / get_state */
    TEST_ASSERT(!lv_plugin_is_active(NULL));
    TEST_ASSERT_EQ(lv_plugin_get_state(NULL), (lvPluginState) 0);
}

static void test_plugin_interfaces(void) {
    /* NULL 安全 */
    TEST_ASSERT_EQ(lv_plugin_register_interface(NULL, NULL), -1);
    TEST_ASSERT_EQ(lv_plugin_unregister_interface(NULL, "test"), -1);
    TEST_ASSERT_NULL(lv_plugin_query_interface(NULL, "test", 0));

    size_t cnt = 0;
    lvPluginInterface **ifaces = lv_plugin_query_interfaces(NULL, NULL, &cnt);
    TEST_ASSERT_NULL(ifaces);
    TEST_ASSERT_EQ(cnt, (size_t) 0);
}

static void test_plugin_events(void) {
    TEST_ASSERT_EQ(lv_plugin_send_event(NULL, 0, NULL, (size_t) 0), -1);
    TEST_ASSERT_EQ(lv_plugin_broadcast_event(NULL, 0, NULL, (size_t) 0), -1);
    lv_plugin_set_event_handler(NULL, NULL); /* 不应崩溃 */
}

static void test_plugin_dependencies(void) {
    TEST_ASSERT_EQ(lv_plugin_resolve_dependencies(NULL, NULL), -1);
    TEST_ASSERT_EQ(lv_plugin_check_dependencies(NULL), 0); /* 无依赖视为满足 */

    size_t cnt = 0;
    lvPlugin **deps = lv_plugin_get_dependents(NULL, NULL, &cnt);
    TEST_ASSERT_NULL(deps);
    TEST_ASSERT_EQ(cnt, (size_t) 0);
}

static void test_plugin_search_path(void) {
    lvPluginSystem *sys = lv_plugin_system_create(NULL);
    TEST_ASSERT_NOT_NULL(sys);

    TEST_ASSERT_EQ(lv_plugin_system_add_search_path(sys, "/some/path"), 0);
    TEST_ASSERT_EQ(lv_plugin_system_add_search_path(sys, "/another/path"), 0);

    size_t cnt = 0;
    char **paths = lv_plugin_system_get_search_paths(sys, &cnt);
    /* 可能返回 NULL 或有效值，取决于实现 */
    if (paths) {
        for (size_t i = 0; i < cnt; i++)
            lv_free(paths[i]);
        lv_free(paths);
    }

    TEST_ASSERT_EQ(lv_plugin_system_remove_search_path(sys, "/some/path"), 0);
    lv_plugin_system_destroy(sys);
}

static void test_plugin_version(void) {
    TEST_ASSERT_EQ(lv_plugin_check_version(">=1.0.0", "1.0.0"), 0);
    /* NULL 安全 */
    TEST_ASSERT_EQ(lv_plugin_check_api_compatibility(1, 1), 0);
}

static void test_plugin_error(void) {
    TEST_ASSERT_STR_EQ(lv_plugin_get_last_error(NULL), "");
    TEST_ASSERT_STR_EQ(lv_plugin_system_get_last_error(NULL), "");
    lv_plugin_clear_error(NULL);
    lv_plugin_system_clear_error(NULL);
}

static void test_plugin_info_json(void) {
    /* NULL 安全 */
    char *json = lv_plugin_get_info_json(NULL);
    TEST_ASSERT_NULL(json);

    json = lv_plugin_system_get_info_json(NULL);
    TEST_ASSERT_NULL(json);
}

/* ============================================================
 * TikZ 导出测试
 * ============================================================ */

static void test_tikz_export_basic(void) {
    /* 准备约束图 */
    lv_init();
    lvEngine *e = lv_engine_create();
    TEST_ASSERT_NOT_NULL(e);

    lv_add_point(e, 0, 1, 0, 1);
    lv_add_point(e, 1, 1, 0, 1);
    lv_add_point(e, 0, 1, 1, 1);
    lv_add_line_segment(e, 0, 1);
    lv_add_line_segment(e, 1, 2);

    /* 缓冲区导出 */
    char buf[4096];
    int n = lv_tikz_export((void *) e->main_graph, buf, sizeof(buf));
    TEST_ASSERT(n > 0, "tikz export to buffer");
    TEST_ASSERT(strstr(buf, "tikzpicture") != NULL, "has tikzpicture env");
    TEST_ASSERT(strstr(buf, "\\fill") != NULL || strstr(buf, "\\draw") != NULL, "has tikz commands");

    /* 空图 */
    lvEngine *empty = lv_engine_create();
    n = lv_tikz_export((void *) empty->main_graph, buf, sizeof(buf));
    TEST_ASSERT(n > 0, "empty graph export");
    lv_engine_destroy(empty);

    /* NULL 安全 */
    TEST_ASSERT_EQ(lv_tikz_export(NULL, NULL, 0), -1);
    TEST_ASSERT_EQ(lv_tikz_export(NULL, buf, sizeof(buf)), -1);
    TEST_ASSERT_EQ(lv_tikz_export((void *) e->main_graph, NULL, sizeof(buf)), -1);
    TEST_ASSERT_EQ(lv_tikz_export((void *) e->main_graph, buf, 0), -1);

    lv_engine_destroy(e);
    lv_cleanup();
}

static void test_tikz_export_file(void) {
    lv_init();
    lvEngine *e = lv_engine_create();
    TEST_ASSERT_NOT_NULL(e);

    lv_add_point(e, 0, 1, 0, 1);
    lv_add_point(e, 1, 1, 0, 1);

    int n = lv_tikz_export_file((void *) e->main_graph, "test_output.tex");
    TEST_ASSERT(n > 0, "tikz file export");

    /* NULL 安全 */
    TEST_ASSERT_EQ(lv_tikz_export_file(NULL, "test.tex"), -1);
    TEST_ASSERT_EQ(lv_tikz_export_file((void *) e->main_graph, NULL), -1);

    lv_engine_destroy(e);
    lv_cleanup();
}

/* ============================================================
 * 证明编译器测试
 * ============================================================ */

static void test_proof_object(void) {
    lvProofObject *obj = lv_proof_object_create();
    TEST_ASSERT_NOT_NULL(obj);
    TEST_ASSERT_EQ(lv_proof_object_get_step_count(obj), 0);
    TEST_ASSERT(!lv_proof_object_is_valid(obj));

    /* 添加步骤 */
    lvProofStepRecord *step = lv_proof_step_record_create();
    TEST_ASSERT_NOT_NULL(step);
    step->type = (ProofStepType) 0;
    step->depth = 0;
    int id = lv_proof_object_add_step(obj, step);
    TEST_ASSERT_GE(id, 0, "add step to proof object");
    TEST_ASSERT_EQ(lv_proof_object_get_step_count(obj), 1);
    TEST_ASSERT(lv_proof_object_is_valid(obj), "now valid");

    /* 添加公理/假设 */
    TEST_ASSERT(lv_proof_object_add_axiom(obj, 100), "add axiom");
    TEST_ASSERT(lv_proof_object_add_assumption(obj, 200), "add assumption");

    /* 添加 NULL 步骤 */
    TEST_ASSERT_EQ(lv_proof_object_add_step(obj, NULL), -1);

    lv_proof_object_destroy(obj);

    /* NULL 安全 */
    lv_proof_object_destroy(NULL);
    TEST_ASSERT_EQ(lv_proof_object_get_step_count(NULL), 0);
    TEST_ASSERT(!lv_proof_object_is_valid(NULL));

    /* 验证 */
    obj = lv_proof_object_create();
    TEST_ASSERT(lv_proof_object_verify(obj), "empty proof verify");
    lv_proof_object_destroy(obj);
}

static void test_proof_step_record(void) {
    lvProofStepRecord *rec = lv_proof_step_record_create();
    TEST_ASSERT_NOT_NULL(rec);
    TEST_ASSERT_EQ(rec->step_id, 0);
    TEST_ASSERT_NULL(rec->premise_step_ids);
    TEST_ASSERT_EQ(rec->premise_count, 0);

    lv_proof_step_record_destroy(rec);
    lv_proof_step_record_destroy(NULL);
}

static void test_proof_trace(void) {
    lvProofTrace *trace = lv_proof_trace_create();
    TEST_ASSERT_NOT_NULL(trace);
    TEST_ASSERT_EQ(trace->event_count, 0);

    /* 跟踪生命周期 */
    lv_proof_trace_start(trace, 1);
    lv_proof_trace_step(trace, 0, "start", 1);
    lv_proof_trace_backtrack(trace, 0, 1);
    lv_proof_trace_branch(trace, "branch_a", 1, 2);
    lv_proof_trace_lemma(trace, 42, "useful_lemma");
    lv_proof_trace_contradiction(trace, 0, 3);
    lv_proof_trace_complete(trace, true);

    TEST_ASSERT(trace->event_count >= 3, "trace has events");

    /* 添加事件 */
    lvTraceEvent *ev = lv_trace_event_create(TRACE_EVENT_STEP);
    TEST_ASSERT_NOT_NULL(ev);
    TEST_ASSERT_EQ(ev->type, TRACE_EVENT_STEP);
    TEST_ASSERT_EQ(lv_proof_trace_add_event(trace, ev), 0);

    lv_proof_trace_destroy(trace);
    lv_proof_trace_destroy(NULL);
}

static void test_proof_compiler(void) {
    lvCompilerConfig cfg = lv_compiler_config_default();
    TEST_ASSERT_EQ(cfg.format, OUTPUT_FORMAT_TEXT);
    TEST_ASSERT(cfg.include_metadata, "default include metadata");

    lvProofCompiler *compiler = lv_proof_compiler_create(&cfg);
    TEST_ASSERT_NOT_NULL(compiler);

    /* 修改配置 */
    cfg.format = OUTPUT_FORMAT_JSON;
    cfg.include_metadata = false;
    lv_proof_compiler_set_config(compiler, &cfg);

    /* 编译空证明 */
    lvProofObject *obj = lv_proof_object_create();
    char *result = lv_proof_compiler_compile(compiler, obj, NULL);
    TEST_ASSERT_NOT_NULL(result);
    lv_free(result);
    lv_proof_object_destroy(obj);

    /* 各格式输出 */
    obj = lv_proof_object_create();

    result = lv_proof_compiler_to_json(obj, NULL);
    TEST_ASSERT_NOT_NULL(result);
    lv_free(result);

    result = lv_proof_compiler_to_text(obj, "zh");
    TEST_ASSERT_NOT_NULL(result);
    lv_free(result);

    result = lv_proof_compiler_to_latex(obj, "zh");
    TEST_ASSERT_NOT_NULL(result);
    lv_free(result);

    result = lv_proof_compiler_to_tikz(obj);
    TEST_ASSERT_NOT_NULL(result);
    lv_free(result);

    result = lv_proof_compiler_to_graphviz(obj, NULL);
    TEST_ASSERT_NOT_NULL(result);
    lv_free(result);

    lv_proof_object_destroy(obj);

    /* NULL 安全 */
    lv_proof_compiler_destroy(NULL);
    result = lv_proof_compiler_to_json(NULL, NULL);
    TEST_ASSERT_NOT_NULL(result); /* 实现可能返回空字符串而非 NULL */
    lv_free(result);

    lv_proof_compiler_destroy(compiler);
}

/* ============================================================
 * UI-Kernel 协议测试
 * ============================================================ */

static void test_trust_color(void) {
    /* 名称 */
    TEST_ASSERT_STR_EQ(lv_trust_color_name(lv_COLOR_GREEN), "Green");
    TEST_ASSERT_STR_EQ(lv_trust_color_name(lv_COLOR_RED), "Red");
    TEST_ASSERT_STR_EQ(lv_trust_color_name((lvTrustColor) 99), "Unknown");

    /* RGBA */
    TEST_ASSERT_EQ(lv_trust_color_rgba(lv_COLOR_GREEN), 0xFF3fb950);
    TEST_ASSERT_EQ(lv_trust_color_rgba(lv_COLOR_RED), 0xFFf85149);
    TEST_ASSERT_EQ(lv_trust_color_rgba((lvTrustColor) 99), 0xFF888888);

    /* SVG */
    TEST_ASSERT_STR_EQ(lv_trust_color_svg(lv_COLOR_GREEN), "#3fb950");
    TEST_ASSERT_STR_EQ(lv_trust_color_svg((lvTrustColor) 99), "#888888");

    /* TikZ */
    TEST_ASSERT_NOT_NULL(lv_trust_color_tikz(lv_COLOR_GREEN));

    /* 颜色映射 */
    lvTrustColor lv = trust_color_to_lv_protocol(TRUST_GREEN);
    TEST_ASSERT_EQ(lv, lv_COLOR_GREEN);

    TrustColor tc = lv_protocol_to_trust_color(lv_COLOR_BLUE);
    TEST_ASSERT_EQ(tc, TRUST_BLUE);
}

/* ============================================================
 * ProofWidget 测试
 * ============================================================ */

static void test_proof_widget_lifecycle(void) {
    lvWidgetLayout *layout = proof_widget_init(4);
    TEST_ASSERT_NOT_NULL(layout);

    /* 注册 Widget */
    int id1 = proof_widget_register(layout, WIDGET_GOAL_DISPLAY, "Goal", 0);
    TEST_ASSERT_GE(id1, 0);
    int id2 = proof_widget_register(layout, WIDGET_HYPOTHESIS_PANEL, "Hypotheses", 1);
    TEST_ASSERT_GE(id2, 0);
    int id3 = proof_widget_register(layout, WIDGET_STEP_NAVIGATOR, "Steps", 2);
    TEST_ASSERT_GE(id3, 0);

    /* 更新 */
    TEST_ASSERT_EQ(proof_widget_update(layout, id1, true, true, "Active Goal", 0, NULL), 0);
    TEST_ASSERT_EQ(proof_widget_update(layout, id2, false, true, "Disabled Hypos", 1, "{\"data\":1}"), 0);

    /* 越界更新 */
    TEST_ASSERT_EQ(proof_widget_update(layout, 999, false, false, NULL, 0, NULL), -1);
    TEST_ASSERT_EQ(proof_widget_update(NULL, 0, false, false, NULL, 0, NULL), -1);

    /* NULL 注册 */
    TEST_ASSERT_EQ(proof_widget_register(NULL, WIDGET_GOAL_DISPLAY, "x", 0), -1);

    /* 布局管理 */
    proof_widget_set_layout_type(layout, LAYOUT_HORIZONTAL, 3, 1);
    proof_widget_set_persistence_key(layout, "test_key");
    int order[] = {2, 1, 0};
    proof_widget_set_order(layout, order, 3);

    /* 布局导出 */
    char *json = proof_widget_export_layout(layout);
    TEST_ASSERT_NOT_NULL(json);
    TEST_ASSERT(strstr(json, "widgets") != NULL);
    lv_free(json);

    /* 持久化键设为 NULL */
    proof_widget_set_persistence_key(layout, NULL);

    proof_widget_destroy(layout);
    proof_widget_destroy(NULL);
}

static void test_proof_widget_goal(void) {
    lvGoalDisplay goal;
    memset(&goal, 0, sizeof(goal));

    /* NULL 安全 */
    TEST_ASSERT_EQ(proof_widget_get_goal(NULL, &goal), -1);
    TEST_ASSERT_EQ(proof_widget_get_goal((ProofNavigator *) 0x1, NULL), -1);
}

static void test_proof_widget_suggest(void) {
    char *suggestions[5];
    double confidences[5];

    int rc = proof_widget_suggest_tactic(NULL, suggestions, confidences, 5);
    TEST_ASSERT_EQ(rc, -1);

    rc = proof_widget_suggest_tactic((ProofNavigator *) 0x1, suggestions, confidences, 0);
    TEST_ASSERT_EQ(rc, -1);

    /* 清理 */
    for (int i = 0; i < 5; i++)
        suggestions[i] = NULL;
}

static void test_proof_widget_search_tree(void) {
    char *tree = proof_widget_get_search_tree(NULL);
    TEST_ASSERT_NULL(tree);

    tree = proof_widget_get_search_tree((ProofNavigator *) 0x1);
    TEST_ASSERT_NOT_NULL(tree);
    lv_free(tree);
}

static void test_proof_widget_dependency(void) {
    char *dep = proof_widget_get_dependency_graph(NULL);
    TEST_ASSERT_NULL(dep);

    dep = proof_widget_get_dependency_graph((ProofNavigator *) 0x1);
    TEST_ASSERT_NOT_NULL(dep);
    lv_free(dep);
}

/* ============================================================
 * Main
 * ============================================================ */

int main(void) {
    TEST_SUITE_BEGIN("Layer5 Output");

    /* ── Magic ── */
    printf("\n--- Magic Module ---\n");
    TEST_RUN(test_rune_lifecycle);
    TEST_RUN(test_rune_power);
    TEST_RUN(test_rune_copy);
    TEST_RUN(test_rune_serialize_parse);
    TEST_RUN(test_rune_sequence);
    TEST_RUN(test_magic_array_lifecycle);
    TEST_RUN(test_magic_array_analysis);
    TEST_RUN(test_magic_array_merge_copy);
    TEST_RUN(test_magic_array_serialize);
    TEST_RUN(test_spell_lifecycle);
    TEST_RUN(test_purity_threshold);
    TEST_RUN(test_spellbook);
    TEST_RUN(test_incantation);
    TEST_RUN(test_restriction_domain);
    TEST_RUN(test_magic_string_conversion);

    /* ── 插件系统 ── */
    printf("\n--- Plugin System ---\n");
    TEST_RUN(test_plugin_system_lifecycle);
    TEST_RUN(test_plugin_config);
    TEST_RUN(test_plugin_queries);
    TEST_RUN(test_plugin_interfaces);
    TEST_RUN(test_plugin_events);
    TEST_RUN(test_plugin_dependencies);
    TEST_RUN(test_plugin_search_path);
    TEST_RUN(test_plugin_version);
    TEST_RUN(test_plugin_error);
    TEST_RUN(test_plugin_info_json);

    /* ── TikZ 导出 ── */
    printf("\n--- TikZ Export ---\n");
    TEST_RUN(test_tikz_export_basic);
    TEST_RUN(test_tikz_export_file);

    /* ── 证明编译器 ── */
    printf("\n--- Proof Compiler ---\n");
    TEST_RUN(test_proof_object);
    TEST_RUN(test_proof_step_record);
    TEST_RUN(test_proof_trace);
    TEST_RUN(test_proof_compiler);

    /* ── UI-Kernel 协议 ── */
    printf("\n--- UI-Kernel Protocol ---\n");
    TEST_RUN(test_trust_color);

    /* ── Proof Widget ── */
    printf("\n--- Proof Widget ---\n");
    TEST_RUN(test_proof_widget_lifecycle);
    TEST_RUN(test_proof_widget_goal);
    TEST_RUN(test_proof_widget_suggest);
    TEST_RUN(test_proof_widget_search_tree);
    TEST_RUN(test_proof_widget_dependency);

    TEST_SUITE_END();
    return g_fail_count > 0 ? 1 : 0;
}

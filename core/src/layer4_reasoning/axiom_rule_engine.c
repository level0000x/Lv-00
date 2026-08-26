/**
 * @file axiom_rule_engine.c
 * @brief 公理规则引擎 - 可配置规则库与难度分级
 *
 * @details 实现规则的创建/销毁、规则库管理、难度评估、
 *          规则匹配与推荐等核心功能。
 */
#include "lv/axiom_rule_engine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_json.h"
#include "lv/lv_hashtable.h" /* 规则库 id/name 哈希索引 */
#include "lv/lv_lifecycle.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_utils.h"

#include "lv/lv_internal.h"

/* 默认规则库容量 */
#define DEFAULT_LIBRARY_CAPACITY 64

/* 难度评分上限（overall_score 量程 0–1000，对应 level 1–10） */
#define LV_DIFFICULTY_MAX_SCORE 1000

/* ============ 内部辅助 ============ */

/* 释放规则内部动态资源 */
static void rule_free_internals(lvRule *rule) {
    if (!rule)
        return;
    for (uint32_t i = 0; i < rule->premise_count; i++) {
        if (rule->premises[i].conditions) {
            lv_free((void **) &rule->premises[i].conditions);
        }
    }
    if (rule->dependency_ids)
        lv_free((void **) &rule->dependency_ids);
    if (rule->tags) {
        for (uint32_t i = 0; i < rule->tag_count; i++) {
            lv_free((void **) &rule->tags[i]);
        }
        lv_free((void **) &rule->tags);
    }
}

/* ============ 规则库管理 ============ */

lvRuleLibrary *lv_rule_library_create(const lvRuleLibraryConfig *config) {
    lvRuleLibrary *lib = lv_calloc(1, sizeof(lvRuleLibrary));
    if (!lib)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "lv_rule_library_create: calloc failed for library");

    uint32_t cap = DEFAULT_LIBRARY_CAPACITY;
    if (config && config->max_rules > 0)
        cap = config->max_rules;

    lib->rules = lv_calloc(cap, sizeof(lvRule *));
    if (!lib->rules) {
        lv_free((void **) &lib);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "lv_rule_library_create: calloc failed for rules");
    }
    lib->rule_capacity = cap;
    lib->rule_count = 0;

    /* id→规则指针 / name→规则指针 哈希索引；创建失败（内存不足）时置 NULL，查找回退线性扫描 */
    lib->id_index = lv_hashtable_int_create((int) cap);
    lib->name_index = lv_hashtable_str_create((int) cap);

    if (config) {
        lib->config = *config;
    } else {
        lib->config.auto_validate = true;
        lib->config.auto_difficulty = true;
        lib->config.enable_cache = false;
    }
    return lib;
}

void lv_rule_library_destroy(lvRuleLibrary *library) {
    if (!library)
        return;
    for (uint32_t i = 0; i < library->rule_count; i++) {
        lv_rule_destroy(library->rules[i]);
    }
    lv_free((void **) &library->rules);
    if (library->id_index) {
        lv_hashtable_int_destroy(library->id_index);
    }
    if (library->name_index) {
        lv_hashtable_str_destroy(library->name_index);
    }
    lv_free((void **) &library);
}

bool lv_rule_library_add(lvRuleLibrary *library, lvRule *rule) {
    if (!library || !rule)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "lv_rule_library_add: NULL library or rule");
    if (library->rule_count >= library->rule_capacity)
        lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_STATE, "lv_rule_library_add: library is full");
    library->rules[library->rule_count] = rule;

    /* 同步哈希索引（值存规则指针，规则对象地址不随数组前移变化）：
     * 键重复时 insert 不覆盖，保留首条，与原线性"返回第一个匹配"语义一致；
     * 非键重复的插入失败（内存不足）会留下缺失键：整体销毁索引，查找回退线性扫描 */
    if (library->id_index &&
        !lv_hashtable_int_insert(library->id_index, (int) rule->id, rule) &&
        !lv_hashtable_int_contains(library->id_index, (int) rule->id)) {
        lv_hashtable_int_destroy(library->id_index);
        library->id_index = NULL;
        if (library->name_index) {
            lv_hashtable_str_destroy(library->name_index);
            library->name_index = NULL;
        }
    }
    if (library->name_index &&
        !lv_hashtable_str_insert(library->name_index, rule->name, rule) &&
        !lv_hashtable_str_contains(library->name_index, rule->name)) {
        if (library->id_index) {
            lv_hashtable_int_destroy(library->id_index);
            library->id_index = NULL;
        }
        lv_hashtable_str_destroy(library->name_index);
        library->name_index = NULL;
    }

    library->rule_count++;
    return true;
}

bool lv_rule_library_remove(lvRuleLibrary *library, uint32_t rule_id) {
    if (!library)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "lv_rule_library_remove: NULL library");
    for (uint32_t i = 0; i < library->rule_count; i++) {
        if (library->rules[i] && library->rules[i]->id == rule_id) {
            lv_rule_destroy(library->rules[i]);
            /* 移动后续元素 */
            for (uint32_t j = i; j < library->rule_count - 1; j++) {
                library->rules[j] = library->rules[j + 1];
            }
            library->rules[library->rule_count - 1] = NULL;
            library->rule_count--;

            /* 前移只改变数组元素（指针）位置，规则对象地址不变；
             * 但被删规则的键已失效，且 id/name 可能重复导致索引与线性结果不一致，
             * 统一重建索引（remove 低频，简单可靠）；创建失败时置 NULL，查找回退线性 */
            if (library->id_index) {
                lv_hashtable_int_destroy(library->id_index);
                library->id_index = NULL;
            }
            if (library->name_index) {
                lv_hashtable_str_destroy(library->name_index);
                library->name_index = NULL;
            }
            library->id_index = lv_hashtable_int_create((int) library->rule_capacity);
            library->name_index = lv_hashtable_str_create((int) library->rule_capacity);
            if (!library->id_index || !library->name_index) {
                if (library->id_index) {
                    lv_hashtable_int_destroy(library->id_index);
                    library->id_index = NULL;
                }
                if (library->name_index) {
                    lv_hashtable_str_destroy(library->name_index);
                    library->name_index = NULL;
                }
            } else {
                bool rebuild_ok = true;
                for (uint32_t k = 0; k < library->rule_count && rebuild_ok; k++) {
                    if (!lv_hashtable_int_insert(library->id_index, (int) library->rules[k]->id, library->rules[k]) ||
                        !lv_hashtable_str_insert(library->name_index, library->rules[k]->name, library->rules[k])) {
                        rebuild_ok = false;
                    }
                }
                if (!rebuild_ok) {
                    /* 重建失败（内存不足或键重复）：销毁索引，查找回退线性扫描（语义正确） */
                    lv_hashtable_int_destroy(library->id_index);
                    library->id_index = NULL;
                    lv_hashtable_str_destroy(library->name_index);
                    library->name_index = NULL;
                }
            }
            return true;
        }
    }
    return false;
}

lvRule *lv_rule_library_get_by_id(const lvRuleLibrary *library, uint32_t rule_id) {
    if (!library)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_rule_library_get_by_id: NULL library");
    if (library->id_index) {
        /* 哈希 O(1)：值存规则指针 */
        return (lvRule *) lv_hashtable_int_get(library->id_index, (int) rule_id);
    }
    /* 索引不可用（创建失败）时回退线性扫描，语义与纯线性实现一致 */
    for (uint32_t i = 0; i < library->rule_count; i++) {
        if (library->rules[i] && library->rules[i]->id == rule_id) {
            return library->rules[i];
        }
    }
    return NULL;
}

lvRule *lv_rule_library_get_by_name(const lvRuleLibrary *library, const char *name) {
    if (!library || !name)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_rule_library_get_by_name: NULL library or name");
    if (library->name_index) {
        /* 哈希 O(1)：值存规则指针 */
        return (lvRule *) lv_hashtable_str_get(library->name_index, name);
    }
    /* 索引不可用（创建失败）时回退线性扫描，语义与纯线性实现一致 */
    for (uint32_t i = 0; i < library->rule_count; i++) {
        if (library->rules[i] && lv_str_eq(library->rules[i]->name, name)) {
            return library->rules[i];
        }
    }
    return NULL;
}

uint32_t lv_rule_library_get_by_type(const lvRuleLibrary *library, lvRuleType type, lvRule **out_rules,
                                     uint32_t max_count) {
    if (!library || !out_rules)
        lv_RETURN_ERROR_VAL(lv_ERROR_NULL_POINTER, 0, "lv_rule_library_get_by_type: NULL param");
    uint32_t found = 0;
    for (uint32_t i = 0; i < library->rule_count && found < max_count; i++) {
        if (library->rules[i] && library->rules[i]->type == type) {
            out_rules[found++] = library->rules[i];
        }
    }
    return found;
}

uint32_t lv_rule_library_get_by_difficulty(const lvRuleLibrary *library, uint32_t min_level, uint32_t max_level,
                                           lvRule **out_rules, uint32_t max_count) {
    if (!library || !out_rules)
        lv_RETURN_ERROR_VAL(lv_ERROR_NULL_POINTER, 0, "lv_rule_library_get_by_difficulty: NULL param");
    uint32_t found = 0;
    for (uint32_t i = 0; i < library->rule_count && found < max_count; i++) {
        lvRule *r = library->rules[i];
        if (r && r->difficulty_level >= min_level && r->difficulty_level <= max_level) {
            out_rules[found++] = r;
        }
    }
    return found;
}

uint32_t lv_rule_library_search_by_tag(const lvRuleLibrary *library, const char *tag, lvRule **out_rules,
                                       uint32_t max_count) {
    if (!library || !tag || !out_rules)
        lv_RETURN_ERROR_VAL(lv_ERROR_NULL_POINTER, 0, "lv_rule_library_search_by_tag: NULL param");
    uint32_t found = 0;
    for (uint32_t i = 0; i < library->rule_count && found < max_count; i++) {
        lvRule *r = library->rules[i];
        if (!r)
            continue;
        for (uint32_t t = 0; t < r->tag_count; t++) {
            if (r->tags[t] && lv_str_eq(r->tags[t], tag)) {
                out_rules[found++] = r;
                break;
            }
        }
    }
    return found;
}

/* ============ 规则创建与管理 ============ */

lvRule *lv_rule_create(const char *name, lvRuleType type) {
    if (!name)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_rule_create: NULL name");
    lvRule *rule = lv_calloc(1, sizeof(lvRule));
    if (!rule)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "lv_rule_create: calloc failed");
    lv_strlcpy(rule->name, name, lv_RULE_NAME_MAX_LEN);
    rule->type = type;
    rule->status = RULE_STATUS_ENABLED;
    rule->priority = RULE_PRIORITY_NORMAL;
    return rule;
}

void lv_rule_destroy(lvRule *rule) {
    if (!rule)
        return;
    rule_free_internals(rule);
    lv_free((void **) &rule);
}

bool lv_rule_set_description(lvRule *rule, const char *description) {
    if (!rule || !description)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "lv_rule_set_description: NULL rule or description");
    lv_strlcpy(rule->description, description, lv_RULE_DESC_MAX_LEN);
    return true;
}

bool lv_rule_add_variable(lvRule *rule, const char *name, const char *type) {
    if (!rule || !name || !type)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "lv_rule_add_variable: NULL param");
    if (rule->var_count >= lv_RULE_MAX_VARIABLES)
        lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_STATE, "lv_rule_add_variable: max variables reached");
    lvRuleVariable *v = &rule->variables[rule->var_count++];
    lv_strlcpy(v->name, name, sizeof(v->name));
    lv_strlcpy(v->type, type, sizeof(v->type));
    v->is_bound = false;
    v->bound_node_id = -1;
    return true;
}

bool lv_rule_add_premise(lvRule *rule, const char *pattern, bool is_optional) {
    if (!rule || !pattern)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "lv_rule_add_premise: NULL param");
    if (rule->premise_count >= lv_RULE_MAX_PREMISES)
        lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_STATE, "lv_rule_add_premise: max premises reached");
    lvRulePremise *p = &rule->premises[rule->premise_count++];
    lv_strlcpy(p->pattern, pattern, sizeof(p->pattern));
    p->is_optional = is_optional;
    p->conditions = NULL;
    p->condition_count = 0;
    return true;
}

bool lv_rule_add_conclusion(lvRule *rule, const char *pattern, TrustColor trust_color) {
    if (!rule || !pattern)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "lv_rule_add_conclusion: NULL param");
    if (rule->conclusion_count >= lv_RULE_MAX_CONCLUSIONS)
        lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_STATE, "lv_rule_add_conclusion: max conclusions reached");
    lvRuleConclusion *c = &rule->conclusions[rule->conclusion_count++];
    lv_strlcpy(c->pattern, pattern, sizeof(c->pattern));
    c->trust_color = trust_color;
    return true;
}

bool lv_rule_add_tag(lvRule *rule, const char *tag) {
    if (!rule || !tag)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "lv_rule_add_tag: NULL rule or tag");
    /* 扩容标签数组（倍增策略，避免逐个 realloc；统一走 lv_ENSURE_ARRAY_CAP） */
    lv_ENSURE_ARRAY_CAP(rule->tags, rule->tag_count, rule->tag_capacity, false);
    rule->tags[rule->tag_count] = lv_strdup(tag);
    if (!rule->tags[rule->tag_count])
        lv_RETURN_ERROR_BOOL(lv_ERROR_ALLOCATION_FAILED, "lv_rule_add_tag: strdup failed");
    rule->tag_count++;
    return true;
}

void lv_rule_set_priority(lvRule *rule, lvRulePriority priority) {
    if (!rule)
        return;
    rule->priority = priority;
}

void lv_rule_set_status(lvRule *rule, lvRuleStatus status) {
    if (!rule)
        return;
    rule->status = status;
}

/* ============ 难度评估 ============ */

static const char *s_level_strings[] = {"",     "入门", "简单", "基础", "中等", "中等偏上",
                                        "进阶", "困难", "专家", "大师", "极限"};

static const char *s_dim_strings[] = {"结构复杂度", "概念难度", "计算复杂度", "创造性要求", "知识依赖"};

const char *lv_difficulty_level_to_string(uint32_t level) {
    if (level < 1 || level > 10)
        return "未知";
    return s_level_strings[level];
}

const char *lv_difficulty_dimension_to_string(lvDifficultyDimension dimension) {
    if (dimension < 0 || dimension >= DIFF_DIM_COUNT)
        return "未知";
    return s_dim_strings[dimension];
}

lvDifficultyAssessment *lv_rule_assess_difficulty(const lvRule *rule) {
    if (!rule)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_rule_assess_difficulty: NULL rule");
    lvDifficultyAssessment *assess = lv_calloc(1, sizeof(lvDifficultyAssessment));
    if (!assess)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "lv_rule_assess_difficulty: calloc failed");

    /* 基于规则结构简单评估 */
    double structural = (double) rule->premise_count * 20.0 + (double) rule->conclusion_count * 15.0;
    double conceptual = (double) rule->var_count * 10.0;
    double computational = (double) rule->premise_count * 5.0;
    double creative = 20.0;
    double knowledge = rule->type == RULE_TYPE_THEOREM ? 50.0 : 10.0;

    assess->dimensions[DIFF_DIM_STRUCTURAL] = lv_CLAMP(structural, 0, 100);
    assess->dimensions[DIFF_DIM_CONCEPTUAL] = lv_CLAMP(conceptual, 0, 100);
    assess->dimensions[DIFF_DIM_COMPUTATIONAL] = lv_CLAMP(computational, 0, 100);
    assess->dimensions[DIFF_DIM_CREATIVE] = lv_CLAMP(creative, 0, 100);
    assess->dimensions[DIFF_DIM_KNOWLEDGE] = lv_CLAMP(knowledge, 0, 100);

    double avg = 0;
    for (int i = 0; i < DIFF_DIM_COUNT; i++)
        avg += assess->dimensions[i];
    avg /= DIFF_DIM_COUNT;
    assess->overall_score = (uint32_t) (avg * 10);
    if (assess->overall_score > LV_DIFFICULTY_MAX_SCORE)
        assess->overall_score = LV_DIFFICULTY_MAX_SCORE;
    assess->level = (assess->overall_score / 100) + 1;
    if (assess->level > 10)
        assess->level = 10;

    lv_snprintf(assess->breakdown, sizeof(assess->breakdown), "规则 '%s': 前提=%u, 结论=%u, 变量=%u", rule->name,
                rule->premise_count, rule->conclusion_count, rule->var_count);
    return assess;
}

void lv_difficulty_assessment_destroy(lvDifficultyAssessment *assessment) {
    lv_free((void **) &assessment);
}

lvDifficultyAssessment *lv_proof_step_assess_difficulty(const ProofStep *step, const ConstraintGraph *graph) {
    lvDifficultyAssessment *a = lv_calloc(1, sizeof(lvDifficultyAssessment));
    if (!a)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "lv_proof_step_assess_difficulty: calloc failed");

    uint32_t score = 100;
    uint32_t level = 1;

    if (step) {
        /* 基于步骤类型和关联数据评估难度 */
        static const struct { uint32_t score; uint32_t level; } step_difficulty[] = {
            [PROOF_STEP_ADD_NODE]       = { 20,  1 },
            [PROOF_STEP_ADD_CONSTRAINT] = { 50,  2 },
            [PROOF_STEP_REWRITE]        = { 80,  2 },
            [PROOF_STEP_FUNCTION_APP]   = { 120, 3 },
            [PROOF_STEP_NORMALIZATION]  = { 60,  2 },
            [PROOF_STEP_UNIFY]          = { 150, 3 },
            [PROOF_STEP_ORACLE]         = { 200, 4 },
        };
        if ((unsigned)step->type < sizeof(step_difficulty)/sizeof(step_difficulty[0]) && step_difficulty[step->type].score) {
            score = step_difficulty[step->type].score;
            level = step_difficulty[step->type].level;
        } else {
            score = 100; level = 2;
        }
        /* 如果有规则 ID，尝试查找规则库中更精确的难度 */
        if (step->rule_id >= 0) {
            score = (uint32_t)(score + 50);
            level = level + 1;
            if (level > 10) level = 10;
        }
    }

    /* 根据图的复杂度调整 */
    if (graph) {
        int node_cnt = graph_get_node_count(graph);
        if (node_cnt > 20) {
            score = (uint32_t)(score * 1.5);
            if (score > LV_DIFFICULTY_MAX_SCORE) score = LV_DIFFICULTY_MAX_SCORE;
        }
    }

    a->overall_score = score;
    a->level = level;
    lv_snprintf(a->breakdown, sizeof(a->breakdown), "步骤类型=%d, score=%u, level=%u",
                step ? (int)step->type : -1, score, level);
    return a;
}

lvDifficultyAssessment *lv_proposition_assess_difficulty(const Proposition *prop) {
    lvDifficultyAssessment *a = lv_calloc(1, sizeof(lvDifficultyAssessment));
    if (!a)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "lv_proposition_assess_difficulty: calloc failed");

    uint32_t score = 100;
    uint32_t level = 1;

    if (prop) {
        /* 基于命题类型和复杂度 */
        static const struct { uint32_t score; uint32_t level; } prop_difficulty[] = {
            [PROPOSITION_TYPE_ATOMIC]      = { 50,  1 },
            [PROPOSITION_TYPE_CONJUNCTION] = { 150, 3 },
            [PROPOSITION_TYPE_DISJUNCTION] = { 200, 4 },
            [PROPOSITION_TYPE_IMPLICATION] = { 300, 5 },
        };
        if ((unsigned)prop->type < sizeof(prop_difficulty)/sizeof(prop_difficulty[0]) && prop_difficulty[prop->type].score) {
            score = prop_difficulty[prop->type].score;
            level = prop_difficulty[prop->type].level;
        } else {
            score = 100; level = 2;
        }
        /* 基于目标图节点数调整 */
         if (prop->pattern) {
             int nc = graph_get_node_count(prop->pattern);
            if (nc > 10) {
                score = (uint32_t)(score * (1.0 + nc * 0.05));
                if (score > LV_DIFFICULTY_MAX_SCORE) score = LV_DIFFICULTY_MAX_SCORE;
                level = (uint32_t)(level + nc / 5);
                if (level > 10) level = 10;
            }
        }
    }

    a->overall_score = score;
    a->level = level;
    lv_snprintf(a->breakdown, sizeof(a->breakdown), "命题类型=%d, score=%u, level=%u",
                prop ? (int)prop->type : -1, score, level);
    return a;
}

/* ============ 规则匹配（完整实现：启发式适用性 + 变量绑定） ============ */

/**
 * @brief 将规则变量绑定到图中满足类型约束的节点（贪心：每变量取首个匹配）
 *
 * 规则变量（lvRuleVariable.type）为字符串（如 "point"/"line_segment"），
 * 通过 lv_geom_type_name/lv_geom_type_alias 映射到 GeomNode.type。
 * 返回实际绑定的变量数；绑定结果写入 match->bindings。
 */
static uint32_t rule_bind_variables(const lvRule *rule, const ConstraintGraph *graph, lvRuleMatch *match) {
    if (!rule || !graph || !match)
        return 0;

    uint32_t bound = 0;
    int node_count = graph_get_node_count(graph);

    for (uint32_t v = 0; v < rule->var_count && v < lv_RULE_MAX_VARIABLES; v++) {
        const lvRuleVariable *var = &rule->variables[v];
        match->bindings[v] = *var;
        match->bindings[v].is_bound = false;
        match->bindings[v].bound_node_id = -1;

        /* 无类型约束的变量：绑定到第一个节点 */
        if (var->type[0] == '\0') {
            for (int i = 0; i < node_count; i++) {
                GeomNode *n = graph->nodes[i];
                if (n && n->type != GEOM_FUNCTION_BLOCK) {
                    match->bindings[v].is_bound = true;
                    match->bindings[v].bound_node_id = n->id;
                    bound++;
                    break;
                }
            }
            continue;
        }

        /* 类型约束匹配：遍历图节点，比对类型名/别名（大小写不敏感） */
        for (int i = 0; i < node_count; i++) {
            GeomNode *n = graph->nodes[i];
            if (!n)
                continue;
            const char *tname = lv_geom_type_name((int) n->type);
            const char *talias = lv_geom_type_alias((int) n->type);
            if (lv_str_icmp(var->type, tname) == 0 || (talias && lv_str_icmp(var->type, talias) == 0)) {
                /* 同一变量绑定后不可重复绑定 */
                bool already = false;
                for (uint32_t k = 0; k < bound; k++) {
                    if (match->bindings[k].is_bound && match->bindings[k].bound_node_id == n->id) {
                        already = true;
                        break;
                    }
                }
                if (!already) {
                    match->bindings[v].is_bound = true;
                    match->bindings[v].bound_node_id = n->id;
                    bound++;
                    break;
                }
            }
        }
    }
    match->binding_count = bound;
    return bound;
}

uint32_t lv_rule_find_matches(const lvRuleLibrary *library, const ConstraintGraph *graph, const ProofNavigator *context,
                              lvRuleMatch **out_matches, uint32_t max_count) {
    if (!library || !out_matches)
        lv_RETURN_ERROR_VAL(lv_ERROR_NULL_POINTER, 0, "lv_rule_find_matches: NULL library or out_matches");

    uint32_t found = 0;
    int node_count = graph ? graph_get_node_count(graph) : 0;

    for (uint32_t i = 0; i < library->rule_count && found < max_count; i++) {
        lvRule *r = library->rules[i];
        if (!r || r->status != RULE_STATUS_ENABLED)
            continue;

        /* 通过 lv_rule_is_applicable 过滤基本适用性 */
        if (!lv_rule_is_applicable(r, graph, context))
            continue;

        /* 计算匹配置信度：基于规则前提数与图中节点数的匹配度 */
        double confidence = 0.5;

        if (node_count > 0 && r->premise_count > 0) {
            /* 检查前提中的变量数是否不超过图节点数 */
            uint32_t needed_vars = r->var_count;
            if (needed_vars > 0 && (uint32_t)node_count >= needed_vars) {
                confidence = 1.0 - (double)(needed_vars) / (double)(node_count + needed_vars) * 0.5;
                if (confidence < 0.5) confidence = 0.5;
            }
        } else if (r->premise_count == 0) {
            /* 无前提的规则（如公理）: 较高置信度 */
            confidence = 0.9;
        }

        /* AXIOM 类型始终完全匹配 */
        bool is_complete = (r->type == RULE_TYPE_AXIOM);

        lvRuleMatch *m = lv_calloc(1, sizeof(lvRuleMatch));
        if (m) {
            m->rule = r;
            m->confidence = confidence;
            m->is_complete = is_complete;
            m->matched_premises = r->premise_count;
            /* 完整实现：为规则变量绑定满足类型约束的图节点
             * （此前绑定数组恒空，lv_rule_apply_match 无法引用具体节点） */
            rule_bind_variables(r, graph, m);
            out_matches[found++] = m;
        }
    }
    return found;
}

uint32_t lv_rule_apply_match(const lvRuleMatch *match, ConstraintGraph *graph, ProofNavigator *context,
                             ProofStep **out_steps, uint32_t max_steps) {
    if (!match || !match->rule || !out_steps || !graph)
        lv_RETURN_ERROR_VAL(lv_ERROR_NULL_POINTER, 0, "lv_rule_apply_match: NULL param");

    lvRule *rule = match->rule;
    /* 为每个结论创建一个证明步骤 */
    uint32_t step_count = rule->conclusion_count;
    if (step_count > max_steps)
        step_count = max_steps;
    if (step_count == 0) {
        /* 至少创建一个步骤表示规则已应用 */
        step_count = 1;
    }

    for (uint32_t i = 0; i < step_count; i++) {
        ProofStep *step = (ProofStep *) lv_calloc(1, sizeof(ProofStep));
        if (!step)
            return i;

        step->id = -1; /* 由上下文分配 ID */
        step->type = PROOF_STEP_ADD_NODE;
        step->color = PROOF_COLOR_BLUE_UNEXPLORED;
        step->rule_id = (int) rule->id;

        /* 尝试从图中获取节点信息 */
        int node_count = graph_get_node_count(graph);
        if (node_count > 0) {
            step->node_id = 0; /* 关联第一个节点 */
        } else {
            step->node_id = -1;
        }
        step->constraint_id = -1;
        step->func_block_id = -1;

        /* 复制规则结论模式作为步骤说明 */
        if (i < rule->conclusion_count && rule->conclusions[i].pattern[0]) {
            step->note = lv_strdup(rule->conclusions[i].pattern);
        } else {
            step->note = lv_strdup(rule->name);
        }

        out_steps[i] = step;
    }

    (void) context;
    return step_count;
}

void lv_rule_match_destroy(lvRuleMatch *match) {
    lv_free((void **) &match);
}

bool lv_rule_is_applicable(const lvRule *rule, const ConstraintGraph *graph, const ProofNavigator *context) {
    if (!rule)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "lv_rule_is_applicable: NULL rule");
    if (rule->status != RULE_STATUS_ENABLED)
        return false;

    /* Axiom 类型始终适用 */
    if (rule->type == RULE_TYPE_AXIOM)
        return true;

    /* 其他规则需要图中有节点才能匹配前提 */
    if (!graph)
        return false;
    int node_count = graph_get_node_count(graph);
    if (node_count <= 0)
        return false;

    /* 检查规则的前提条件：
     * - 推理规则/定理/引理：需要至少一个前提，且图中节点数 >= 前提数
     * - 重写规则：需要图中有节点
     * - 定义/构造函数：始终适用
     */
    if (rule->type == RULE_TYPE_INFERENCE || rule->type == RULE_TYPE_THEOREM || rule->type == RULE_TYPE_LEMMA) {
        if (rule->premise_count == 0)
            return false;
        /* 适用性启发：推理类规则需前提可匹配，图节点数少于前提数时
         * 必然无法完成绑定（每个前提至少需要一个不同节点），提前拒绝 */
        if (node_count < (int) rule->premise_count)
            return false;
    }

    (void) context;
    return true;
}

/* ============ 规则推荐（完整实现：适用性过滤 + 优先级/难度评分） ============ */

lvRuleRecommendation *lv_rule_recommend(const lvRuleLibrary *library, const ConstraintGraph *graph,
                                        const ProofNavigator *context, uint32_t max_count) {
    if (!library)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_rule_recommend: NULL library");

    /* 第一遍：收集适用规则并计算分数 */
    uint32_t applicable_count = 0;
    for (uint32_t i = 0; i < library->rule_count; i++) {
        lvRule *r = library->rules[i];
        if (!r || r->status != RULE_STATUS_ENABLED)
            continue;
        if (!lv_rule_is_applicable(r, graph, context))
            continue;
        applicable_count++;
    }

    uint32_t cnt = applicable_count < max_count ? applicable_count : max_count;
    lvRuleRecommendation *rec = lv_calloc(1, sizeof(lvRuleRecommendation));
    if (!rec)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "lv_rule_recommend: calloc failed for rec");

    rec->rules = lv_calloc(cnt > 0 ? cnt : 1, sizeof(lvRule *));
    rec->scores = lv_calloc(cnt > 0 ? cnt : 1, sizeof(double));
    if (!rec->rules || !rec->scores) {
        lv_free((void **) &rec->rules);
        lv_free((void **) &rec->scores);
        lv_free((void **) &rec);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "lv_rule_recommend: calloc failed for rules/scores");
    }

    /* 第二遍：填充适用规则 */
    uint32_t idx = 0;
    for (uint32_t i = 0; i < library->rule_count && idx < cnt; i++) {
        lvRule *r = library->rules[i];
        if (!r || r->status != RULE_STATUS_ENABLED)
            continue;
        if (!lv_rule_is_applicable(r, graph, context))
            continue;
        rec->rules[idx] = r;
        /* 评分：优先级为主（0-100），难度分数为辅（0-1000 → 0-10） */
        rec->scores[idx] = (double) r->priority + (double) r->difficulty_score / 100.0;
        idx++;
    }
    rec->count = idx;

    /* 按分数降序排序（插入排序，数量小） */
    for (uint32_t i = 1; i < rec->count; i++) {
        lvRule *rkey = rec->rules[i];
        double skey = rec->scores[i];
        int32_t j = (int32_t) i - 1;
        while (j >= 0 && rec->scores[j] < skey) {
            rec->rules[j + 1] = rec->rules[j];
            rec->scores[j + 1] = rec->scores[j];
            j--;
        }
        rec->rules[j + 1] = rkey;
        rec->scores[j + 1] = skey;
    }

    rec->reason = lv_strdup("按规则优先级与难度综合评分推荐");
    return rec;
}

static const lvFieldDesc s_rule_recommendation_destroy_fields[] = {
    lv_FIELD_PLAIN(lvRuleRecommendation, rules),
    lv_FIELD_PLAIN(lvRuleRecommendation, scores),
    lv_FIELD_PLAIN(lvRuleRecommendation, reason),
};

void lv_rule_recommendation_destroy(lvRuleRecommendation *rec) {
    if (!rec)
        return;
    lv_obj_destroy_fields(rec, s_rule_recommendation_destroy_fields,
                          sizeof(s_rule_recommendation_destroy_fields) / sizeof(s_rule_recommendation_destroy_fields[0]));
    lv_free((void **) &rec);
}

/* ============ 序列化（完整实现：含变量/前提/结论内容，支持往返） ============ */

char *lv_rule_to_json(const lvRule *rule) {
    if (!rule)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_rule_to_json: NULL rule");
    lvJsonBuf buf;
    if (!lv_json_buf_init(&buf, 512))
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_rule_to_json: lv_json_buf_init failed");

    /* 基本信息 */
    lv_json_buf_append_raw(&buf, "{\"id\":");
    lv_json_buf_append_fmt(&buf, "%u", rule->id);
    lv_json_buf_append_raw(&buf, ",\"name\":");
    lv_json_buf_append_string(&buf, rule->name);
    lv_json_buf_append_raw(&buf, ",\"description\":");
    lv_json_buf_append_string(&buf, rule->description);
    lv_json_buf_append_fmt(&buf, ",\"type\":%d,\"status\":%d,\"priority\":%d,"
                             "\"premise_count\":%u,\"conclusion_count\":%u,\"difficulty_level\":%u,"
                             "\"difficulty_score\":%u,\"var_count\":%u",
                           (int) rule->type, (int) rule->status, (int) rule->priority, rule->premise_count,
                           rule->conclusion_count, rule->difficulty_level, rule->difficulty_score, rule->var_count);

    /* 变量数组 */
    lv_json_buf_append_raw(&buf, ",\"variables\":[");
    for (uint32_t i = 0; i < rule->var_count && i < lv_RULE_MAX_VARIABLES; i++) {
        if (i > 0)
            lv_json_buf_append_raw(&buf, ",");
        lv_json_buf_append_raw(&buf, "{\"name\":");
        lv_json_buf_append_string(&buf, rule->variables[i].name);
        lv_json_buf_append_raw(&buf, ",\"type\":");
        lv_json_buf_append_string(&buf, rule->variables[i].type);
        lv_json_buf_append_raw(&buf, ",\"is_bound\":");
        lv_json_buf_append_raw(&buf, rule->variables[i].is_bound ? "true" : "false");
        lv_json_buf_append_fmt(&buf, ",\"bound_node_id\":%d}", rule->variables[i].bound_node_id);
    }
    lv_json_buf_append_raw(&buf, "]");

    /* 前提数组（pattern + optional；conditions 为动态数组，序列化时省略类型细节，
     * 反序列化重建 pattern/optional 骨架，条件回调不跨 JSON 保留 —— 与
     * lv_rule_from_json 行为对称） */
    lv_json_buf_append_raw(&buf, ",\"premises\":[");
    for (uint32_t i = 0; i < rule->premise_count && i < lv_RULE_MAX_PREMISES; i++) {
        if (i > 0)
            lv_json_buf_append_raw(&buf, ",");
        lv_json_buf_append_raw(&buf, "{\"pattern\":");
        lv_json_buf_append_string(&buf, rule->premises[i].pattern);
        lv_json_buf_append_raw(&buf, ",\"optional\":");
        lv_json_buf_append_raw(&buf, rule->premises[i].is_optional ? "true" : "false");
        lv_json_buf_append_raw(&buf, "}");
    }
    lv_json_buf_append_raw(&buf, "]");

    /* 结论数组（pattern + trust + justification） */
    lv_json_buf_append_raw(&buf, ",\"conclusions\":[");
    for (uint32_t i = 0; i < rule->conclusion_count && i < lv_RULE_MAX_CONCLUSIONS; i++) {
        if (i > 0)
            lv_json_buf_append_raw(&buf, ",");
        lv_json_buf_append_raw(&buf, "{\"pattern\":");
        lv_json_buf_append_string(&buf, rule->conclusions[i].pattern);
        lv_json_buf_append_fmt(&buf, ",\"trust\":%d", (int) rule->conclusions[i].trust_color);
        lv_json_buf_append_raw(&buf, ",\"justification\":");
        lv_json_buf_append_string(&buf, rule->conclusions[i].justification);
        lv_json_buf_append_raw(&buf, "}");
    }
    lv_json_buf_append_raw(&buf, "]}");

    return lv_json_buf_finalize(&buf);
}

/* ── from_json 辅助：解析 "variables"/"premises"/"conclusions" 对象数组 ──
 * 数组形态：[ {key:value,...}, ... ]，元素字段经嵌套 parse_field 遍历。
 * 回调返回 false 表示元素字段解析失败（继续跳过剩余字段）。 */

typedef void (*RuleArrayElemParser)(lvRule *rule, const char *key, lvJsonParser *p, bool *ok);

static void rule_parse_variable_elem(lvRule *rule, const char *key, lvJsonParser *p, bool *ok) {
    if (!rule || !key || !p || !ok)
        return;
    if (lv_str_eq(key, "name")) {
        char *s = lv_json_parse_string(p);
        if (s && rule->var_count < lv_RULE_MAX_VARIABLES) {
            lv_strlcpy(rule->variables[rule->var_count].name, s, sizeof(rule->variables[0].name));
            rule->var_count++;
            *ok = true;
        }
        if (s)
            lv_free((void **) &s);
    } else if (lv_str_eq(key, "type") && rule->var_count > 0) {
        char *s = lv_json_parse_string(p);
        if (s) {
            lv_strlcpy(rule->variables[rule->var_count - 1].type, s, sizeof(rule->variables[0].type));
            lv_free((void **) &s);
            *ok = true;
        }
    } else if (lv_str_eq(key, "is_bound") && rule->var_count > 0) {
        bool b = false;
        if (lv_json_parse_bool(p, &b)) {
            rule->variables[rule->var_count - 1].is_bound = b;
            *ok = true;
        }
    } else if (lv_str_eq(key, "bound_node_id") && rule->var_count > 0) {
        int v = 0;
        if (lv_json_parse_int(p, &v)) {
            rule->variables[rule->var_count - 1].bound_node_id = v;
            *ok = true;
        }
    } else {
        lv_json_skip_value(p);
        *ok = true; /* 未知字段跳过不算失败 */
    }
}

static void rule_parse_premise_elem(lvRule *rule, const char *key, lvJsonParser *p, bool *ok) {
    if (!rule || !key || !p || !ok)
        return;
    if (lv_str_eq(key, "pattern")) {
        char *s = lv_json_parse_string(p);
        if (s && rule->premise_count < lv_RULE_MAX_PREMISES) {
            lv_strlcpy(rule->premises[rule->premise_count].pattern, s, sizeof(rule->premises[0].pattern));
            rule->premise_count++;
            *ok = true;
        }
        if (s)
            lv_free((void **) &s);
    } else if (lv_str_eq(key, "optional") && rule->premise_count > 0) {
        bool b = false;
        if (lv_json_parse_bool(p, &b)) {
            rule->premises[rule->premise_count - 1].is_optional = b;
            *ok = true;
        }
    } else {
        lv_json_skip_value(p);
        *ok = true;
    }
}

static void rule_parse_conclusion_elem(lvRule *rule, const char *key, lvJsonParser *p, bool *ok) {
    if (!rule || !key || !p || !ok)
        return;
    if (lv_str_eq(key, "pattern")) {
        char *s = lv_json_parse_string(p);
        if (s && rule->conclusion_count < lv_RULE_MAX_CONCLUSIONS) {
            lv_strlcpy(rule->conclusions[rule->conclusion_count].pattern, s,
                       sizeof(rule->conclusions[0].pattern));
            rule->conclusion_count++;
            *ok = true;
        }
        if (s)
            lv_free((void **) &s);
    } else if (lv_str_eq(key, "trust") && rule->conclusion_count > 0) {
        int v = 0;
        if (lv_json_parse_int(p, &v)) {
            rule->conclusions[rule->conclusion_count - 1].trust_color = (TrustColor) v;
            *ok = true;
        }
    } else if (lv_str_eq(key, "justification") && rule->conclusion_count > 0) {
        char *s = lv_json_parse_string(p);
        if (s) {
            lv_strlcpy(rule->conclusions[rule->conclusion_count - 1].justification, s,
                       sizeof(rule->conclusions[0].justification));
            lv_free((void **) &s);
            *ok = true;
        }
    } else {
        lv_json_skip_value(p);
        *ok = true;
    }
}

/** @brief 解析一个对象数组（p 指向 '[' 后的首个元素）；元素为对象，逐字段派发到回调 */
static void rule_parse_obj_array(lvJsonParser *p, lvRule *rule, RuleArrayElemParser elem_parser) {
    if (!p || !rule || !elem_parser)
        return;
    if (lv_json_peek(p) == '[')
        lv_json_next(p); /* skip '[' */
    for (;;) {
        lv_json_skip_ws(p);
        char c = lv_json_peek(p);
        if (c == ']' || c == '\0') {
            lv_json_next(p);
            break;
        }
        if (c == ',') {
            lv_json_next(p);
            continue;
        }
        if (c != '{') {
            lv_json_skip_value(p);
            continue;
        }
        lv_json_next(p); /* skip '{' */
        bool elem_ok = false;
        char *key = NULL;
        while (lv_json_parse_field(p, &key)) {
            elem_parser(rule, key, p, &elem_ok);
            lv_free((void **) &key);
        }
        if (lv_json_peek(p) == '}')
            lv_json_next(p);
    }
}

lvRule *lv_rule_from_json(const char *json) {
    if (!json)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_rule_from_json: NULL json");

    /* 解析基本 JSON 字段 */
    lvRuleType rtype = RULE_TYPE_AXIOM;
    lvRulePriority prio = RULE_PRIORITY_NORMAL;
    lvRuleStatus status = RULE_STATUS_ENABLED;
    char name_buf[lv_RULE_NAME_MAX_LEN] = "parsed_rule";
    char desc_buf[lv_RULE_DESC_MAX_LEN] = "";
    uint32_t id = 0;
    uint32_t difficulty_level = 0;
    uint32_t difficulty_score = 0;

    lvJsonParser p;
    lv_json_parser_init(&p, json, strlen(json));
    if (lv_json_peek(&p) == '{')
        lv_json_next(&p); /* skip '{' */

    char *key = NULL;
    while (lv_json_parse_field(&p, &key)) {
        if (lv_str_eq(key, "name")) {
            char *s = lv_json_parse_string(&p);
            if (s) {
                lv_strlcpy(name_buf, s, sizeof(name_buf));
                lv_free((void **) &s);
            }
        } else if (lv_str_eq(key, "description")) {
            char *s = lv_json_parse_string(&p);
            if (s) {
                lv_strlcpy(desc_buf, s, sizeof(desc_buf));
                lv_free((void **) &s);
            }
        } else {
            int tv;
            if (lv_str_eq(key, "type") && lv_json_parse_int(&p, &tv)) {
                if (tv >= RULE_TYPE_INFERENCE && tv <= RULE_TYPE_CONSTRUCTOR)
                    rtype = (lvRuleType)tv;
            } else if (lv_str_eq(key, "priority") && lv_json_parse_int(&p, &tv)) {
                if (tv >= RULE_PRIORITY_LOWEST && tv <= RULE_PRIORITY_HIGHEST)
                    prio = (lvRulePriority)tv;
            } else if (lv_str_eq(key, "status") && lv_json_parse_int(&p, &tv)) {
                if (tv >= RULE_STATUS_DISABLED && tv <= RULE_STATUS_EXPERIMENTAL)
                    status = (lvRuleStatus)tv;
            } else if (lv_str_eq(key, "id") && lv_json_parse_int(&p, &tv)) {
                if (tv >= 0)
                    id = (uint32_t)tv;
            } else if (lv_str_eq(key, "difficulty_level") && lv_json_parse_int(&p, &tv)) {
                if (tv >= 0)
                    difficulty_level = (uint32_t)tv;
            } else if (lv_str_eq(key, "difficulty_score") && lv_json_parse_int(&p, &tv)) {
                if (tv >= 0)
                    difficulty_score = (uint32_t)tv;
            } else if (lv_str_eq(key, "premise_count") || lv_str_eq(key, "conclusion_count") ||
                       lv_str_eq(key, "var_count")) {
                /* 计数由数组元素数量推导，忽略显式计数（防不一致） */
                lv_json_skip_value(&p);
            } else if (lv_str_eq(key, "variables") || lv_str_eq(key, "premises") ||
                       lv_str_eq(key, "conclusions")) {
                /* 数组字段在第二遍（rule 创建后）解析，第一遍跳过 */
                lv_json_skip_value(&p);
            } else {
                lv_json_skip_value(&p);
            }
        }
        lv_free((void **) &key);
    }

    lvRule *rule = lv_rule_create(name_buf, rtype);
    if (rule) {
        rule->id = id;
        rule->priority = prio;
        rule->status = status;
        rule->difficulty_level = difficulty_level;
        rule->difficulty_score = difficulty_score;
        if (desc_buf[0] != '\0')
            lv_rule_set_description(rule, desc_buf);
    }

    /* 第二遍：数组字段在 rule 创建后填充（变量/前提/结论需要写入 rule） */
    if (rule) {
        lvJsonParser p2;
        lv_json_parser_init(&p2, json, strlen(json));
        if (lv_json_peek(&p2) == '{')
            lv_json_next(&p2);
        char *key2 = NULL;
        while (lv_json_parse_field(&p2, &key2)) {
            if (lv_str_eq(key2, "variables")) {
                rule_parse_obj_array(&p2, rule, rule_parse_variable_elem);
            } else if (lv_str_eq(key2, "premises")) {
                rule_parse_obj_array(&p2, rule, rule_parse_premise_elem);
            } else if (lv_str_eq(key2, "conclusions")) {
                rule_parse_obj_array(&p2, rule, rule_parse_conclusion_elem);
            } else {
                lv_json_skip_value(&p2);
            }
            lv_free((void **) &key2);
        }
    }
    return rule;
}

lvRule *lv_rule_copy(const lvRule *rule) {
    if (!rule)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_rule_copy: NULL rule");
    lvRule *copy = lv_calloc(1, sizeof(lvRule));
    if (!copy)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "lv_rule_copy: calloc failed");
    memcpy(copy, rule, sizeof(lvRule));
    /* 重置动态指针，避免双重释放 */
    copy->dependency_ids = NULL;
    copy->tags = NULL;
    copy->tag_count = 0;
    copy->tag_capacity = 0;
    for (uint32_t i = 0; i < rule->premise_count; i++) {
        copy->premises[i].conditions = NULL;
    }
    /* 复制标签 */
    if (rule->tags && rule->tag_count > 0) {
        for (uint32_t i = 0; i < rule->tag_count; i++) {
            lv_rule_add_tag(copy, rule->tags[i]);
        }
    }
    return copy;
}

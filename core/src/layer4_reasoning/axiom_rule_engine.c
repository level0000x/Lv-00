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
#include "lv/lv_utils.h"

#include "lv_internal.h"

/* 默认规则库容量 */
#define DEFAULT_LIBRARY_CAPACITY 64

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
    if (library->name_index) {
        for (uint32_t i = 0; i < library->rule_count; i++) {
            lv_free((void **) &library->name_index[i]);
        }
        lv_free((void **) &library->name_index);
    }
    lv_free((void **) &library->id_index);
    lv_free((void **) &library->type_index);
    lv_free((void **) &library);
}

bool lv_rule_library_add(lvRuleLibrary *library, lvRule *rule) {
    if (!library || !rule)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "lv_rule_library_add: NULL library or rule");
    if (library->rule_count >= library->rule_capacity)
        lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_STATE, "lv_rule_library_add: library is full");
    library->rules[library->rule_count++] = rule;
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
            return true;
        }
    }
    return false;
}

lvRule *lv_rule_library_get_by_id(const lvRuleLibrary *library, uint32_t rule_id) {
    if (!library)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_rule_library_get_by_id: NULL library");
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
    for (uint32_t i = 0; i < library->rule_count; i++) {
        if (library->rules[i] && strcmp(library->rules[i]->name, name) == 0) {
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
            if (r->tags[t] && strcmp(r->tags[t], tag) == 0) {
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
    if (assess->overall_score > 1000)
        assess->overall_score = 1000;
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
            if (score > 1000) score = 1000;
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
                if (score > 1000) score = 1000;
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

/* ============ 规则匹配（简化实现） ============ */

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
        /* 简化：节点数必须 >= 前提数 */
        if (node_count < (int) rule->premise_count)
            return false;
    }

    (void) context;
    return true;
}

/* ============ 规则推荐（简化实现） ============ */

lvRuleRecommendation *lv_rule_recommend(const lvRuleLibrary *library, const ConstraintGraph *graph,
                                        const ProofNavigator *context, uint32_t max_count) {
    if (!library)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_rule_recommend: NULL library");
    lvRuleRecommendation *rec = lv_calloc(1, sizeof(lvRuleRecommendation));
    if (!rec)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "lv_rule_recommend: calloc failed for rec");

    uint32_t cnt = library->rule_count < max_count ? library->rule_count : max_count;
    rec->rules = lv_calloc(cnt, sizeof(lvRule *));
    rec->scores = lv_calloc(cnt, sizeof(double));
    if (!rec->rules || !rec->scores) {
        lv_free((void **) &rec->rules);
        lv_free((void **) &rec->scores);
        lv_free((void **) &rec);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "lv_rule_recommend: calloc failed for rules/scores");
    }
    for (uint32_t i = 0; i < cnt; i++) {
        rec->rules[i] = library->rules[i];
        rec->scores[i] = 1.0;
    }
    rec->count = cnt;
    rec->reason = lv_strdup("基于规则优先级推荐");
    return rec;
}

void lv_rule_recommendation_destroy(lvRuleRecommendation *rec) {
    if (!rec)
        return;
    lv_free((void **) &rec->rules);
    lv_free((void **) &rec->scores);
    lv_free((void **) &rec->reason);
    lv_free((void **) &rec);
}

/* ============ 序列化（简化实现） ============ */

char *lv_rule_to_json(const lvRule *rule) {
    if (!rule)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_rule_to_json: NULL rule");
    lvJsonBuf buf;
    if (!lv_json_buf_init(&buf, 256))
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_rule_to_json: lv_json_buf_init failed");
    /* name 经 append_string 自动 JSON 转义，防引号/控制字符注入 */
    lv_json_buf_append_raw(&buf, "{\"id\":");
    lv_json_buf_append_fmt(&buf, "%u", rule->id);
    lv_json_buf_append_raw(&buf, ",\"name\":");
    lv_json_buf_append_string(&buf, rule->name);
    lv_json_buf_append_fmt(&buf, ",\"type\":%d,\"status\":%d,\"priority\":%d,"
                             "\"premise_count\":%u,\"conclusion_count\":%u,\"difficulty_level\":%u}",
                           (int) rule->type, (int) rule->status, (int) rule->priority, rule->premise_count,
                           rule->conclusion_count, rule->difficulty_level);
    return lv_json_buf_finalize(&buf);
}

lvRule *lv_rule_from_json(const char *json) {
    if (!json)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_rule_from_json: NULL json");

    /* 解析基本 JSON 字段 */
    lvRuleType rtype = RULE_TYPE_AXIOM;
    lvRulePriority prio = RULE_PRIORITY_NORMAL;
    char name_buf[lv_RULE_NAME_MAX_LEN] = "parsed_rule";

    lv_json_get_string(json, "name", name_buf, sizeof(name_buf));

    int tv;
    if (lv_json_get_int(json, "type", &tv)) {
        if (tv >= RULE_TYPE_AXIOM && tv <= RULE_TYPE_DEFINITION)
            rtype = (lvRuleType)tv;
    }

    int pv;
    if (lv_json_get_int(json, "priority", &pv)) {
        if (pv >= RULE_PRIORITY_LOWEST && pv <= RULE_PRIORITY_HIGHEST)
            prio = (lvRulePriority)pv;
    }

    lvRule *rule = lv_rule_create(name_buf, rtype);
    if (rule)
        rule->priority = prio;
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

bool lv_rule_library_save(const lvRuleLibrary *library, const char *path) {
    if (!library || !path)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "lv_rule_library_save: NULL library or path");
    FILE *f = fopen(path, "w");
    if (!f)
        lv_RETURN_ERROR_BOOL(lv_ERROR_INTERNAL, "lv_rule_library_save: fopen failed");
    lvJsonBuf buf;
    lv_json_buf_init(&buf, 64);
    lv_json_buf_append_fmt(&buf, "{\"rule_count\":%u}", library->rule_count);
    char *json = lv_json_buf_finalize(&buf);
    if (json) {
        fputs(json, f);
        fputc('\n', f);
        lv_free(json);
    }
    fclose(f);
    return true;
}

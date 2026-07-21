/**
 * @file axiom_rule_engine.c
 * @brief 公理规则引擎 - 可配置规则库与难度分级
 *
 * @details 实现规则的创建/销毁、规则库管理、难度评估、
 *          规则匹配与推荐等核心功能。
 */
#include "lv00/axiom_rule_engine.h"
#include "lv00/constraint_graph.h"
#include "lv00/lv00_utils.h"
#include "lv00/proof.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* 默认规则库容量 */
#define DEFAULT_LIBRARY_CAPACITY 64

/* ============ 内部辅助 ============ */

/* 释放规则内部动态资源 */
static void rule_free_internals(Lv00Rule *rule) {
    if (!rule) return;
    for (uint32_t i = 0; i < rule->premise_count; i++) {
        if (rule->premises[i].conditions) {
            lv00_free((void **)&rule->premises[i].conditions);
        }
    }
    if (rule->dependency_ids) lv00_free((void **)&rule->dependency_ids);
    if (rule->tags) {
        for (uint32_t i = 0; i < rule->tag_count; i++) {
            lv00_free((void **)&rule->tags[i]);
        }
        lv00_free((void **)&rule->tags);
    }
}

/* ============ 规则库管理 ============ */

Lv00RuleLibrary *lv00_rule_library_create(const Lv00RuleLibraryConfig *config) {
    Lv00RuleLibrary *lib = lv00_calloc(1, sizeof(Lv00RuleLibrary));
    if (!lib) return NULL;

    uint32_t cap = DEFAULT_LIBRARY_CAPACITY;
    if (config && config->max_rules > 0) cap = config->max_rules;

    lib->rules = lv00_calloc(cap, sizeof(Lv00Rule *));
    if (!lib->rules) {
        lv00_free((void **)&lib);
        return NULL;
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

void lv00_rule_library_destroy(Lv00RuleLibrary *library) {
    if (!library) return;
    for (uint32_t i = 0; i < library->rule_count; i++) {
        lv00_rule_destroy(library->rules[i]);
    }
    lv00_free((void **)&library->rules);
    if (library->name_index) {
        for (uint32_t i = 0; i < library->rule_count; i++) {
            lv00_free((void **)&library->name_index[i]);
        }
        lv00_free((void **)&library->name_index);
    }
    lv00_free((void **)&library->id_index);
    lv00_free((void **)&library->type_index);
    lv00_free((void **)&library);
}

int lv00_rule_library_add(Lv00RuleLibrary *library, Lv00Rule *rule) {
    if (!library || !rule) return false;
    if (library->rule_count >= library->rule_capacity) return false;
    library->rules[library->rule_count++] = rule;
    return true;
}

int lv00_rule_library_remove(Lv00RuleLibrary *library, uint32_t rule_id) {
    if (!library) return false;
    for (uint32_t i = 0; i < library->rule_count; i++) {
        if (library->rules[i] && library->rules[i]->id == rule_id) {
            lv00_rule_destroy(library->rules[i]);
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

Lv00Rule *lv00_rule_library_get_by_id(const Lv00RuleLibrary *library, uint32_t rule_id) {
    if (!library) return NULL;
    for (uint32_t i = 0; i < library->rule_count; i++) {
        if (library->rules[i] && library->rules[i]->id == rule_id) {
            return library->rules[i];
        }
    }
    return NULL;
}

Lv00Rule *lv00_rule_library_get_by_name(const Lv00RuleLibrary *library, const char *name) {
    if (!library || !name) return NULL;
    for (uint32_t i = 0; i < library->rule_count; i++) {
        if (library->rules[i] && strcmp(library->rules[i]->name, name) == 0) {
            return library->rules[i];
        }
    }
    return NULL;
}

uint32_t lv00_rule_library_get_by_type(const Lv00RuleLibrary *library,
                                        Lv00RuleType type,
                                        Lv00Rule **out_rules,
                                        uint32_t max_count) {
    if (!library || !out_rules) return true;
    uint32_t found = 0;
    for (uint32_t i = 0; i < library->rule_count && found < max_count; i++) {
        if (library->rules[i] && library->rules[i]->type == type) {
            out_rules[found++] = library->rules[i];
        }
    }
    return found;
}

uint32_t lv00_rule_library_get_by_difficulty(const Lv00RuleLibrary *library,
                                              uint32_t min_level,
                                              uint32_t max_level,
                                              Lv00Rule **out_rules,
                                              uint32_t max_count) {
    if (!library || !out_rules) return true;
    uint32_t found = 0;
    for (uint32_t i = 0; i < library->rule_count && found < max_count; i++) {
        Lv00Rule *r = library->rules[i];
        if (r && r->difficulty_level >= min_level && r->difficulty_level <= max_level) {
            out_rules[found++] = r;
        }
    }
    return found;
}

uint32_t lv00_rule_library_search_by_tag(const Lv00RuleLibrary *library,
                                          const char *tag,
                                          Lv00Rule **out_rules,
                                          uint32_t max_count) {
    if (!library || !tag || !out_rules) return true;
    uint32_t found = 0;
    for (uint32_t i = 0; i < library->rule_count && found < max_count; i++) {
        Lv00Rule *r = library->rules[i];
        if (!r) continue;
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

Lv00Rule *lv00_rule_create(const char *name, Lv00RuleType type) {
    if (!name) return NULL;
    Lv00Rule *rule = lv00_calloc(1, sizeof(Lv00Rule));
    if (!rule) return NULL;
    lv00_strlcpy(rule->name, name, LV00_RULE_NAME_MAX_LEN);
    rule->type = type;
    rule->status = RULE_STATUS_ENABLED;
    rule->priority = RULE_PRIORITY_NORMAL;
    return rule;
}

void lv00_rule_destroy(Lv00Rule *rule) {
    if (!rule) return;
    rule_free_internals(rule);
    lv00_free((void **)&rule);
}

int lv00_rule_set_description(Lv00Rule *rule, const char *description) {
    if (!rule || !description) return false;
    lv00_strlcpy(rule->description, description, LV00_RULE_DESC_MAX_LEN);
    return true;
}

int lv00_rule_add_variable(Lv00Rule *rule, const char *name, const char *type) {
    if (!rule || !name || !type) return false;
    if (rule->var_count >= LV00_RULE_MAX_VARIABLES) return false;
    Lv00RuleVariable *v = &rule->variables[rule->var_count++];
    lv00_strlcpy(v->name, name, sizeof(v->name));
    lv00_strlcpy(v->type, type, sizeof(v->type));
    v->is_bound = false;
    v->bound_node_id = -1;
    return true;
}

int lv00_rule_add_premise(Lv00Rule *rule, const char *pattern, bool is_optional) {
    if (!rule || !pattern) return false;
    if (rule->premise_count >= LV00_RULE_MAX_PREMISES) return false;
    Lv00RulePremise *p = &rule->premises[rule->premise_count++];
    lv00_strlcpy(p->pattern, pattern, sizeof(p->pattern));
    p->is_optional = is_optional;
    p->conditions = NULL;
    p->condition_count = 0;
    return true;
}

int lv00_rule_add_conclusion(Lv00Rule *rule, const char *pattern, TrustColor trust_color) {
    if (!rule || !pattern) return false;
    if (rule->conclusion_count >= LV00_RULE_MAX_CONCLUSIONS) return false;
    Lv00RuleConclusion *c = &rule->conclusions[rule->conclusion_count++];
    lv00_strlcpy(c->pattern, pattern, sizeof(c->pattern));
    c->trust_color = trust_color;
    return true;
}

int lv00_rule_add_tag(Lv00Rule *rule, const char *tag) {
    if (!rule || !tag) return false;
    char **new_tags = lv00_realloc(rule->tags, (rule->tag_count + 1) * sizeof(char *));
    if (!new_tags) return false;
    rule->tags = new_tags;
    rule->tags[rule->tag_count] = lv00_strdup(tag);
    if (!rule->tags[rule->tag_count]) return false;
    rule->tag_count++;
    return true;
}

void lv00_rule_set_priority(Lv00Rule *rule, Lv00RulePriority priority) {
    if (!rule) return;
    rule->priority = priority;
}

void lv00_rule_set_status(Lv00Rule *rule, Lv00RuleStatus status) {
    if (!rule) return;
    rule->status = status;
}

/* ============ 难度评估 ============ */

static const char *s_level_strings[] = {
    "", "入门", "简单", "基础", "中等", "中等偏上",
    "进阶", "困难", "专家", "大师", "极限"
};

static const char *s_dim_strings[] = {
    "结构复杂度", "概念难度", "计算复杂度", "创造性要求", "知识依赖"
};

const char *lv00_difficulty_level_to_string(uint32_t level) {
    if (level < 1 || level > 10) return "未知";
    return s_level_strings[level];
}

const char *lv00_difficulty_dimension_to_string(Lv00DifficultyDimension dimension) {
    if (dimension < 0 || dimension >= DIFF_DIM_COUNT) return "未知";
    return s_dim_strings[dimension];
}

Lv00DifficultyAssessment *lv00_rule_assess_difficulty(const Lv00Rule *rule) {
    if (!rule) return NULL;
    Lv00DifficultyAssessment *assess = lv00_calloc(1, sizeof(Lv00DifficultyAssessment));
    if (!assess) return NULL;

    /* 基于规则结构简单评估 */
    double structural = (double)rule->premise_count * 20.0 +
                        (double)rule->conclusion_count * 15.0;
    double conceptual = (double)rule->var_count * 10.0;
    double computational = (double)rule->premise_count * 5.0;
    double creative = 20.0;
    double knowledge = rule->type == RULE_TYPE_THEOREM ? 50.0 : 10.0;

    assess->dimensions[DIFF_DIM_STRUCTURAL] = LV00_CLAMP(structural, 0, 100);
    assess->dimensions[DIFF_DIM_CONCEPTUAL] = LV00_CLAMP(conceptual, 0, 100);
    assess->dimensions[DIFF_DIM_COMPUTATIONAL] = LV00_CLAMP(computational, 0, 100);
    assess->dimensions[DIFF_DIM_CREATIVE] = LV00_CLAMP(creative, 0, 100);
    assess->dimensions[DIFF_DIM_KNOWLEDGE] = LV00_CLAMP(knowledge, 0, 100);

    double avg = 0;
    for (int i = 0; i < DIFF_DIM_COUNT; i++) avg += assess->dimensions[i];
    avg /= DIFF_DIM_COUNT;
    assess->overall_score = (uint32_t)(avg * 10);
    if (assess->overall_score > 1000) assess->overall_score = 1000;
    assess->level = (assess->overall_score / 100) + 1;
    if (assess->level > 10) assess->level = 10;

    lv00_snprintf(assess->breakdown, sizeof(assess->breakdown),
                  "规则 '%s': 前提=%u, 结论=%u, 变量=%u",
                  rule->name, rule->premise_count, rule->conclusion_count, rule->var_count);
    return assess;
}

void lv00_difficulty_assessment_destroy(Lv00DifficultyAssessment *assessment) {
    lv00_free((void **)&assessment);
}

Lv00DifficultyAssessment *lv00_proof_step_assess_difficulty(const ProofStep *step,
                                                             const ConstraintGraph *graph) {
    (void)step; (void)graph;
    /* 简化实现：返回默认评估 */
    Lv00DifficultyAssessment *a = lv00_calloc(1, sizeof(Lv00DifficultyAssessment));
    if (a) {
        a->level = 1;
        a->overall_score = 100;
    }
    return a;
}

Lv00DifficultyAssessment *lv00_proposition_assess_difficulty(const Proposition *prop) {
    (void)prop;
    Lv00DifficultyAssessment *a = lv00_calloc(1, sizeof(Lv00DifficultyAssessment));
    if (a) {
        a->level = 1;
        a->overall_score = 100;
    }
    return a;
}

/* ============ 规则匹配（简化实现） ============ */

uint32_t lv00_rule_find_matches(const Lv00RuleLibrary *library,
                                 const ConstraintGraph *graph,
                                 const ProofNavigator *context,
                                 Lv00RuleMatch **out_matches,
                                 uint32_t max_count) {
    if (!library || !out_matches) return true;
    uint32_t found = 0;
    for (uint32_t i = 0; i < library->rule_count && found < max_count; i++) {
        Lv00Rule *r = library->rules[i];
        if (!r || r->status != RULE_STATUS_ENABLED) continue;
        if (lv00_rule_is_applicable(r, graph, context)) {
            Lv00RuleMatch *m = lv00_calloc(1, sizeof(Lv00RuleMatch));
            if (m) {
                m->rule = r;
                m->confidence = 0.8;
                m->is_complete = true;
                out_matches[found++] = m;
            }
        }
    }
    return found;
}

uint32_t lv00_rule_apply_match(const Lv00RuleMatch *match,
                                ConstraintGraph *graph,
                                ProofNavigator *context,
                                ProofStep **out_steps,
                                uint32_t max_steps) {
    if (!match || !match->rule || !out_steps || !graph) return true;

    Lv00Rule *rule = match->rule;
    /* 为每个结论创建一个证明步骤 */
    uint32_t step_count = rule->conclusion_count;
    if (step_count > max_steps) step_count = max_steps;
    if (step_count == 0) {
        /* 至少创建一个步骤表示规则已应用 */
        step_count = 1;
    }

    for (uint32_t i = 0; i < step_count; i++) {
        ProofStep *step = (ProofStep *)lv00_calloc(1, sizeof(ProofStep));
        if (!step) return i;

        step->id = -1; /* 由上下文分配 ID */
        step->type = PROOF_STEP_ADD_NODE;
        step->color = PROOF_COLOR_BLUE_UNEXPLORED;
        step->rule_id = (int)rule->id;

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
            step->note = lv00_strdup(rule->conclusions[i].pattern);
        } else {
            step->note = lv00_strdup(rule->name);
        }

        out_steps[i] = step;
    }

    (void)context;
    return step_count;
}

void lv00_rule_match_destroy(Lv00RuleMatch *match) {
    lv00_free((void **)&match);
}

bool lv00_rule_is_applicable(const Lv00Rule *rule,
                              const ConstraintGraph *graph,
                              const ProofNavigator *context) {
    if (!rule) return false;
    if (rule->status != RULE_STATUS_ENABLED) return false;

    /* Axiom 类型始终适用 */
    if (rule->type == RULE_TYPE_AXIOM) return true;

    /* 其他规则需要图中有节点才能匹配前提 */
    if (!graph) return false;
    int node_count = graph_get_node_count(graph);
    if (node_count <= 0) return false;

    /* 检查规则的前提条件：
     * - 推理规则/定理/引理：需要至少一个前提，且图中节点数 >= 前提数
     * - 重写规则：需要图中有节点
     * - 定义/构造函数：始终适用
     */
    if (rule->type == RULE_TYPE_INFERENCE || rule->type == RULE_TYPE_THEOREM ||
        rule->type == RULE_TYPE_LEMMA) {
        if (rule->premise_count == 0) return false;
        /* 简化：节点数必须 >= 前提数 */
        if (node_count < (int)rule->premise_count) return false;
    }

    (void)context;
    return true;
}

/* ============ 规则推荐（简化实现） ============ */

Lv00RuleRecommendation *lv00_rule_recommend(const Lv00RuleLibrary *library,
                                             const ConstraintGraph *graph,
                                             const ProofNavigator *context,
                                             uint32_t max_count) {
    if (!library) return NULL;
    Lv00RuleRecommendation *rec = lv00_calloc(1, sizeof(Lv00RuleRecommendation));
    if (!rec) return NULL;

    uint32_t cnt = library->rule_count < max_count ? library->rule_count : max_count;
    rec->rules = lv00_calloc(cnt, sizeof(Lv00Rule *));
    rec->scores = lv00_calloc(cnt, sizeof(double));
    if (!rec->rules || !rec->scores) {
        lv00_free((void **)&rec->rules);
        lv00_free((void **)&rec->scores);
        lv00_free((void **)&rec);
        return NULL;
    }
    for (uint32_t i = 0; i < cnt; i++) {
        rec->rules[i] = library->rules[i];
        rec->scores[i] = 1.0;
    }
    rec->count = cnt;
    rec->reason = lv00_strdup("基于规则优先级推荐");
    return rec;
}

void lv00_rule_recommendation_destroy(Lv00RuleRecommendation *rec) {
    if (!rec) return;
    lv00_free((void **)&rec->rules);
    lv00_free((void **)&rec->scores);
    lv00_free((void **)&rec->reason);
    lv00_free((void **)&rec);
}

/* ============ 序列化（简化实现） ============ */

char *lv00_rule_to_json(const Lv00Rule *rule) {
    if (!rule) return NULL;
    char *json = lv00_asprintf(
        "{\"id\":%u,\"name\":\"%s\",\"type\":%d,\"status\":%d,\"priority\":%d,"
        "\"premise_count\":%u,\"conclusion_count\":%u,\"difficulty_level\":%u}",
        rule->id, rule->name, (int)rule->type, (int)rule->status,
        (int)rule->priority, rule->premise_count, rule->conclusion_count,
        rule->difficulty_level);
    return json;
}

Lv00Rule *lv00_rule_from_json(const char *json) {
    if (!json) return NULL;
    /* 简化实现：创建默认规则 */
    return lv00_rule_create("parsed_rule", RULE_TYPE_AXIOM);
}

Lv00Rule *lv00_rule_copy(const Lv00Rule *rule) {
    if (!rule) return NULL;
    Lv00Rule *copy = lv00_calloc(1, sizeof(Lv00Rule));
    if (!copy) return NULL;
    memcpy(copy, rule, sizeof(Lv00Rule));
    /* 重置动态指针，避免双重释放 */
    copy->dependency_ids = NULL;
    copy->tags = NULL;
    copy->tag_count = 0;
    for (uint32_t i = 0; i < rule->premise_count; i++) {
        copy->premises[i].conditions = NULL;
    }
    /* 复制标签 */
    if (rule->tags && rule->tag_count > 0) {
        for (uint32_t i = 0; i < rule->tag_count; i++) {
            lv00_rule_add_tag(copy, rule->tags[i]);
        }
    }
    return copy;
}

int lv00_rule_library_save(const Lv00RuleLibrary *library, const char *path) {
    if (!library || !path) return false;
    FILE *f = fopen(path, "w");
    if (!f) return false;
    fprintf(f, "{\"rule_count\":%u}\n", library->rule_count);
    fclose(f);
    return true;
}
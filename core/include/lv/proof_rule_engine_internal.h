#ifndef lv_PROOF_RULE_ENGINE_INTERNAL_H
#define lv_PROOF_RULE_ENGINE_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "proof_rule_engine.h"

/* ============== 常量定义 ============== */

#ifndef lv_RULE_SET_CAPACITY
#define lv_RULE_SET_CAPACITY 256
#endif

#ifndef lv_GOAL_STACK_MAX
#define lv_GOAL_STACK_MAX 64
#endif

#ifndef lv_HYPOTHESIS_MAX
#define lv_HYPOTHESIS_MAX 128
#endif

#ifndef lv_APPLIED_RULES_MAX
#define lv_APPLIED_RULES_MAX 256
#endif

#ifndef lv_PROOF_RULE_NAME_MAX
#define lv_PROOF_RULE_NAME_MAX 128
#endif

#ifndef lv_DEFAULT_MAX_DEPTH
#define lv_DEFAULT_MAX_DEPTH 100
#endif

#ifndef lv_DEFAULT_SEARCH_TIMEOUT_MS
#define lv_DEFAULT_SEARCH_TIMEOUT_MS 0
#endif

/* ============== 搜索策略枚举 ============== */

typedef enum {
    SEARCH_BEST_FIRST = 0,
    SEARCH_DEPTH_FIRST,
    SEARCH_BREADTH_FIRST,
    SEARCH_ITERATIVE_DEEPENING
} lvSearchStrategy;

/* ============== 搜索结果状态枚举 ============== */

typedef enum {
    SEARCH_RESULT_FOUND = 0,
    SEARCH_RESULT_TIMEOUT,
    SEARCH_RESULT_DEPTH_LIMIT,
    SEARCH_RESULT_EXHAUSTED,
    SEARCH_RESULT_ERROR
} lvSearchResultStatus;

/* ============== 证明规则类型枚举 ============== */

typedef enum {
    RULE_INTRO = 0,
    RULE_ELIM,
    RULE_REWRITE,
    RULE_INDUCTION,
    RULE_CONTRADICTION,
    RULE_CASE_SPLIT,
    RULE_GENERALIZE,
    RULE_SPECIALIZE,
    RULE_NEURAL_SUGGEST,
    RULE_AUX_CONSTRUCT
} lvProofRuleType;

/* ============== 规则适用性检查函数指针 ============== */

typedef bool (*RuleApplicabilityFn)(const void *rule, const void *state);

/* ============== 规则应用函数指针 ============== */

typedef bool (*RuleApplyFn)(void *rule, void *state);

/* ============== 证明规则结构体 ============== */

typedef struct lvProofRule {
    char name[lv_PROOF_RULE_NAME_MAX];
    double weight;
    RuleApplicabilityFn applicability_check_fn;
    RuleApplyFn apply_fn;
    lvProofRuleType type;
    int priority;
} lvProofRule;

/* ============== 证明状态结构体 ============== */

typedef struct lvProofState {
    char *goal_stack[lv_GOAL_STACK_MAX];
    int goal_stack_top;
    char *current_goal;
    char *hypotheses[lv_HYPOTHESIS_MAX];
    int hypothesis_count;
    char *applied_rules[lv_APPLIED_RULES_MAX];
    int applied_rule_count;
    int current_depth;
} lvProofState;

/* ============== 规则引擎结构体 ============== */

typedef struct lvRuleEngine {
    lvProofRule **rule_set;
    int rule_count;
    int rule_capacity;
    lvSearchStrategy search_strategy;
    int max_depth;
    uint64_t timeout_ms;
} lvRuleEngine;

/* ============== API 函数声明 ============== */

lvRuleEngine *rule_engine_create(void);
lvRuleEngine *rule_engine_create_ex(lvSearchStrategy strategy, int max_depth, uint64_t timeout_ms);
void rule_engine_destroy(lvRuleEngine *engine);
bool rule_engine_add_rule(lvRuleEngine *engine, lvProofRule *rule);
bool rule_engine_remove_rule(lvRuleEngine *engine, const char *name);
const lvProofRule *rule_engine_find_rule(const lvRuleEngine *engine, const char *name);
lvSearchResultStatus rule_engine_search(lvRuleEngine *engine, lvProofState *state);
int rule_engine_rule_count(const lvRuleEngine *engine);

lvProofState *proof_state_create(const char *initial_goal);
void proof_state_destroy(lvProofState *state);
bool proof_state_push_goal(lvProofState *state, const char *goal);
bool proof_state_pop_goal(lvProofState *state);
bool proof_state_add_hypothesis(lvProofState *state, const char *hypothesis);
bool proof_state_record_rule(lvProofState *state, const char *name);
bool proof_state_is_complete(const lvProofState *state);
const char *proof_state_current_goal(const lvProofState *state);

const char *proof_rule_type_to_string(lvProofRuleType type);
const char *search_strategy_to_string(lvSearchStrategy strategy);
const char *search_result_status_to_string(lvSearchResultStatus status);

#ifdef __cplusplus
}
#endif

#endif /* lv_PROOF_RULE_ENGINE_INTERNAL_H */

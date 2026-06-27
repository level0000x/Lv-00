#ifndef LV00_PROOF_RULE_ENGINE_INTERNAL_H
#define LV00_PROOF_RULE_ENGINE_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "proof_rule_engine.h"

/* ============== 常量定义 ============== */

#ifndef LV00_RULE_SET_CAPACITY
#define LV00_RULE_SET_CAPACITY 256
#endif

#ifndef LV00_GOAL_STACK_MAX
#define LV00_GOAL_STACK_MAX 64
#endif

#ifndef LV00_HYPOTHESIS_MAX
#define LV00_HYPOTHESIS_MAX 128
#endif

#ifndef LV00_APPLIED_RULES_MAX
#define LV00_APPLIED_RULES_MAX 256
#endif

#ifndef LV00_PROOF_RULE_NAME_MAX
#define LV00_PROOF_RULE_NAME_MAX 128
#endif

#ifndef LV00_DEFAULT_MAX_DEPTH
#define LV00_DEFAULT_MAX_DEPTH 100
#endif

#ifndef LV00_DEFAULT_SEARCH_TIMEOUT_MS
#define LV00_DEFAULT_SEARCH_TIMEOUT_MS 0
#endif

/* ============== 搜索策略枚举 ============== */

typedef enum {
    SEARCH_BEST_FIRST = 0,
    SEARCH_DEPTH_FIRST,
    SEARCH_BREADTH_FIRST,
    SEARCH_ITERATIVE_DEEPENING
} Lv00SearchStrategy;

/* ============== 搜索结果状态枚举 ============== */

typedef enum {
    SEARCH_RESULT_FOUND = 0,
    SEARCH_RESULT_TIMEOUT,
    SEARCH_RESULT_DEPTH_LIMIT,
    SEARCH_RESULT_EXHAUSTED,
    SEARCH_RESULT_ERROR
} Lv00SearchResultStatus;

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
} Lv00ProofRuleType;

/* ============== 规则适用性检查函数指针 ============== */

typedef bool (*RuleApplicabilityFn)(const void *rule, const void *state);

/* ============== 规则应用函数指针 ============== */

typedef bool (*RuleApplyFn)(void *rule, void *state);

/* ============== 证明规则结构体 ============== */

typedef struct Lv00ProofRule {
    char name[LV00_PROOF_RULE_NAME_MAX];
    double weight;
    RuleApplicabilityFn applicability_check_fn;
    RuleApplyFn apply_fn;
    Lv00ProofRuleType type;
} Lv00ProofRule;

/* ============== 证明状态结构体 ============== */

typedef struct Lv00ProofState {
    char *goal_stack[LV00_GOAL_STACK_MAX];
    int goal_stack_top;
    char *current_goal;
    char *hypotheses[LV00_HYPOTHESIS_MAX];
    int hypothesis_count;
    char *applied_rules[LV00_APPLIED_RULES_MAX];
    int applied_rule_count;
    int current_depth;
} Lv00ProofState;

/* ============== 规则引擎结构体 ============== */

typedef struct Lv00RuleEngine {
    Lv00ProofRule **rule_set;
    int rule_count;
    int rule_capacity;
    Lv00SearchStrategy search_strategy;
    int max_depth;
    uint64_t timeout_ms;
} Lv00RuleEngine;

/* ============== API 函数声明 ============== */

Lv00RuleEngine *rule_engine_create(void);
Lv00RuleEngine *rule_engine_create_ex(Lv00SearchStrategy strategy, int max_depth, uint64_t timeout_ms);
void rule_engine_destroy(Lv00RuleEngine *engine);
bool rule_engine_add_rule(Lv00RuleEngine *engine, Lv00ProofRule *rule);
bool rule_engine_remove_rule(Lv00RuleEngine *engine, const char *name);
const Lv00ProofRule *rule_engine_find_rule(const Lv00RuleEngine *engine, const char *name);
Lv00SearchResultStatus rule_engine_search(Lv00RuleEngine *engine, Lv00ProofState *state);
int rule_engine_rule_count(const Lv00RuleEngine *engine);

Lv00ProofState *proof_state_create(const char *initial_goal);
void proof_state_destroy(Lv00ProofState *state);
bool proof_state_push_goal(Lv00ProofState *state, const char *goal);
bool proof_state_pop_goal(Lv00ProofState *state);
bool proof_state_add_hypothesis(Lv00ProofState *state, const char *hypothesis);
bool proof_state_record_rule(Lv00ProofState *state, const char *name);
bool proof_state_is_complete(const Lv00ProofState *state);
const char *proof_state_current_goal(const Lv00ProofState *state);

const char *proof_rule_type_to_string(Lv00ProofRuleType type);
const char *search_strategy_to_string(Lv00SearchStrategy strategy);
const char *search_result_status_to_string(Lv00SearchResultStatus status);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PROOF_RULE_ENGINE_INTERNAL_H */

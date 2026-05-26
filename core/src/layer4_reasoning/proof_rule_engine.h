/**
 * @file proof_rule_engine.h
 * @brief Proof rule search engine -- best-first rule application with depth limits
 *
 * @details Provides a configurable rule-based proof search engine inspired by
 *   Aesop (tactic-based proof search), Seed-Prover (neural auxiliary construction),
 *   and MiniF2F (neural theorem proving).
 *
 *   Key features:
 *   1. Rule types: introduction, elimination, rewrite, induction, contradiction,
 *      case split, generalization, specialization, neural suggestion, auxiliary
 *      construction.
 *   2. Search strategies: best-first, depth-first, breadth-first, iterative
 *      deepening.
 *   3. Proof state management with goal stack and hypothesis tracking.
 *   4. Weighted rule prioritization for best-first search ordering.
 *
 * @author Lv-00 Project
 * @version 3.4.0
 */

#ifndef LV00_PROOF_RULE_ENGINE_H
#define LV00_PROOF_RULE_ENGINE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lv00.h"

/* ============== Configuration Constants ============== */

/** Maximum rule name length */
#define LV00_PROOF_RULE_NAME_MAX 128

/** Maximum goal stack depth */
#define LV00_GOAL_STACK_MAX 64

/** Maximum hypothesis count */
#define LV00_HYPOTHESIS_MAX 128

/** Maximum applied rules history */
#define LV00_APPLIED_RULES_MAX 256

/** Default maximum search depth */
#define LV00_DEFAULT_MAX_DEPTH 32

/** Default search timeout in milliseconds (0 = no timeout) */
#define LV00_DEFAULT_SEARCH_TIMEOUT_MS 0

/** Default rule set capacity */
#define LV00_RULE_SET_CAPACITY 64

/* ============== Forward Declarations ============== */

typedef struct Lv00ProofRule Lv00ProofRule;
typedef struct Lv00ProofState Lv00ProofState;
typedef struct Lv00RuleEngine Lv00RuleEngine;

/* ============== Proof Rule Type ============== */

/**
 * @brief Proof rule type enumeration
 *
 * Categorizes the kinds of inference rules available during proof search.
 * Includes standard logical rules plus neural and auxiliary construction
 * rules inspired by Seed-Prover and MiniF2F.
 */
typedef enum {
    RULE_INTRO,            /**< Introduction rule (e.g., and-intro, exists-intro) */
    RULE_ELIM,             /**< Elimination rule (e.g., and-elim, exists-elim) */
    RULE_REWRITE,          /**< Rewriting rule (equational or definitional rewrite) */
    RULE_INDUCTION,        /**< Induction principle application */
    RULE_CONTRADICTION,    /**< Contradiction / ex falso rule */
    RULE_CASE_SPLIT,       /**< Case analysis / split on disjunction */
    RULE_GENERALIZE,       /**< Generalization (introduce universal quantifier) */
    RULE_SPECIALIZE,       /**< Specialization (instantiate universal quantifier) */
    RULE_NEURAL_SUGGEST,   /**< Neural network suggested tactic (MiniF2F style) */
    RULE_AUX_CONSTRUCT     /**< Auxiliary construction (Seed-Prover style) */
} Lv00ProofRuleType;

/* ============== Search Strategy ============== */

/**
 * @brief Search strategy enumeration
 *
 * Determines how the rule engine explores the proof search space.
 */
typedef enum {
    SEARCH_BEST_FIRST,           /**< Best-first: prioritize by rule weight */
    SEARCH_DEPTH_FIRST,          /**< Depth-first: explore deep before wide */
    SEARCH_BREADTH_FIRST,        /**< Breadth-first: explore wide before deep */
    SEARCH_ITERATIVE_DEEPENING   /**< Iterative deepening: gradually increase depth limit */
} Lv00SearchStrategy;

/* ============== Search Result ============== */

/**
 * @brief Search result status
 */
typedef enum {
    SEARCH_RESULT_FOUND,       /**< Proof found successfully */
    SEARCH_RESULT_TIMEOUT,     /**< Search timed out */
    SEARCH_RESULT_DEPTH_LIMIT, /**< Depth limit reached without proof */
    SEARCH_RESULT_EXHAUSTED,   /**< All possibilities exhausted, no proof */
    SEARCH_RESULT_ERROR        /**< Internal error during search */
} Lv00SearchResultStatus;

/* ============== Proof Rule ============== */

/**
 * @brief Applicability check function type
 *
 * Determines whether a rule can be applied to the current proof state.
 *
 * @param rule   The rule being checked
 * @param state  The current proof state
 * @return true if the rule is applicable, false otherwise
 */
typedef bool (*Lv00RuleApplicabilityCheckFn)(const Lv00ProofRule *rule,
                                              const Lv00ProofState *state);

/**
 * @brief Rule application function type
 *
 * Applies the rule to the proof state, potentially generating new sub-goals
 * or modifying the hypothesis set.
 *
 * @param rule   The rule to apply
 * @param state  The current proof state (modified in place)
 * @return true if application succeeded, false otherwise
 */
typedef bool (*Lv00RuleApplyFn)(const Lv00ProofRule *rule, Lv00ProofState *state);

/**
 * @brief Proof rule structure
 *
 * Represents a single inference rule with metadata for search prioritization.
 */
struct Lv00ProofRule {
    Lv00ProofRuleType type;              /**< Rule type classification */
    char name[LV00_PROOF_RULE_NAME_MAX]; /**< Human-readable rule name */
    int priority;                        /**< Static priority (higher = preferred) */
    double weight;                       /**< Dynamic weight for best-first ordering */
    Lv00RuleApplicabilityCheckFn applicability_check_fn; /**< Check if rule applies */
    Lv00RuleApplyFn apply_fn;            /**< Apply the rule to proof state */
};

/* ============== Proof State ============== */

/**
 * @brief Proof state structure
 *
 * Represents the current state of a proof attempt, including the goal stack,
 * available hypotheses, and history of applied rules.
 */
struct Lv00ProofState {
    /* Goal management */
    char *goal_stack[LV00_GOAL_STACK_MAX]; /**< Stack of pending goals (top = current) */
    int goal_stack_top;                     /**< Index of current goal (-1 if empty) */
    char *current_goal;                     /**< Pointer to current goal (alias for top of stack) */

    /* Hypothesis management */
    char *hypotheses[LV00_HYPOTHESIS_MAX]; /**< Available hypotheses */
    int hypothesis_count;                   /**< Number of active hypotheses */

    /* History tracking */
    char *applied_rules[LV00_APPLIED_RULES_MAX]; /**< Names of applied rules */
    int applied_rule_count;                        /**< Number of applied rules */
    int current_depth;                              /**< Current search depth */
};

/* ============== Rule Engine ============== */

/**
 * @brief Rule engine structure
 *
 * Manages a set of proof rules and provides search capabilities for
 * automated proof construction.
 */
struct Lv00RuleEngine {
    Lv00ProofRule **rule_set;    /**< Array of registered rules */
    int rule_count;              /**< Number of registered rules */
    int rule_capacity;           /**< Capacity of rule_set array */
    Lv00SearchStrategy search_strategy; /**< Active search strategy */
    int max_depth;               /**< Maximum search depth */
    uint64_t timeout_ms;         /**< Search timeout in milliseconds (0 = no timeout) */
};

/* ============== Rule Engine API ============== */

/**
 * @brief Create a new rule engine with default configuration
 *
 * Initializes a rule engine with SEARCH_BEST_FIRST strategy,
 * default max depth, and no timeout.
 *
 * @return Pointer to new rule engine, or NULL on allocation failure
 */
LV00_PUBLIC_API Lv00RuleEngine *rule_engine_create(void);

/**
 * @brief Create a new rule engine with custom configuration
 *
 * @param strategy   Search strategy to use
 * @param max_depth  Maximum search depth
 * @param timeout_ms Search timeout in milliseconds (0 = no timeout)
 * @return Pointer to new rule engine, or NULL on allocation failure
 */
LV00_PUBLIC_API Lv00RuleEngine *rule_engine_create_ex(Lv00SearchStrategy strategy,
                                                      int max_depth,
                                                      uint64_t timeout_ms);

/**
 * @brief Destroy a rule engine and free all resources
 *
 * @param engine Rule engine to destroy (safe to pass NULL)
 */
LV00_PUBLIC_API void rule_engine_destroy(Lv00RuleEngine *engine);

/**
 * @brief Add a rule to the engine's rule set
 *
 * The engine takes ownership of the rule pointer. The rule must have been
 * heap-allocated and will be freed when the engine is destroyed.
 *
 * @param engine Rule engine
 * @param rule   Rule to add (ownership transferred)
 * @return true on success, false on invalid arguments or capacity exceeded
 */
LV00_PUBLIC_API bool rule_engine_add_rule(Lv00RuleEngine *engine, Lv00ProofRule *rule);

/**
 * @brief Remove a rule from the engine by name
 *
 * @param engine Rule engine
 * @param name   Name of the rule to remove
 * @return true if rule was found and removed, false otherwise
 */
LV00_PUBLIC_API bool rule_engine_remove_rule(Lv00RuleEngine *engine, const char *name);

/**
 * @brief Find a rule by name
 *
 * @param engine Rule engine
 * @param name   Rule name to search for
 * @return Pointer to the rule, or NULL if not found
 */
LV00_PUBLIC_API const Lv00ProofRule *rule_engine_find_rule(const Lv00RuleEngine *engine,
                                                            const char *name);

/**
 * @brief Search for a proof using the configured strategy
 *
 * Attempts to find a proof for the given initial proof state by applying
 * rules according to the engine's search strategy. The proof state is
 * modified in place if a proof is found.
 *
 * Best-first search sorts applicable rules by weight (descending) and
 * applies the highest-weighted rule first. Depth limits and timeouts are
 * enforced.
 *
 * @param engine Rule engine
 * @param state  Initial proof state (modified if proof found)
 * @return Search result status
 */
LV00_PUBLIC_API Lv00SearchResultStatus rule_engine_search(Lv00RuleEngine *engine,
                                                          Lv00ProofState *state);

/**
 * @brief Get the number of rules registered in the engine
 *
 * @param engine Rule engine
 * @return Number of registered rules, or -1 if engine is NULL
 */
LV00_PUBLIC_API int rule_engine_rule_count(const Lv00RuleEngine *engine);

/* ============== Proof State API ============== */

/**
 * @brief Create a new proof state with an initial goal
 *
 * @param initial_goal  The goal proposition to prove (copied internally)
 * @return Pointer to new proof state, or NULL on failure
 */
LV00_PUBLIC_API Lv00ProofState *proof_state_create(const char *initial_goal);

/**
 * @brief Destroy a proof state and free all resources
 *
 * @param state Proof state to destroy (safe to pass NULL)
 */
LV00_PUBLIC_API void proof_state_destroy(Lv00ProofState *state);

/**
 * @brief Push a new sub-goal onto the goal stack
 *
 * @param state  Proof state
 * @param goal   Sub-goal string (copied internally)
 * @return true on success, false on stack overflow or invalid arguments
 */
LV00_PUBLIC_API bool proof_state_push_goal(Lv00ProofState *state, const char *goal);

/**
 * @brief Pop the current goal from the goal stack
 *
 * @param state Proof state
 * @return true if a goal was popped, false if stack was empty
 */
LV00_PUBLIC_API bool proof_state_pop_goal(Lv00ProofState *state);

/**
 * @brief Add a hypothesis to the proof state
 *
 * @param state      Proof state
 * @param hypothesis Hypothesis string (copied internally)
 * @return true on success, false on capacity exceeded or invalid arguments
 */
LV00_PUBLIC_API bool proof_state_add_hypothesis(Lv00ProofState *state, const char *hypothesis);

/**
 * @brief Record that a rule has been applied
 *
 * @param state  Proof state
 * @param name   Name of the applied rule (copied internally)
 * @return true on success, false on capacity exceeded or invalid arguments
 */
LV00_PUBLIC_API bool proof_state_record_rule(Lv00ProofState *state, const char *name);

/**
 * @brief Check if the proof state has no remaining goals (proof complete)
 *
 * @param state Proof state
 * @return true if goal stack is empty (proof complete), false otherwise
 */
LV00_PUBLIC_API bool proof_state_is_complete(const Lv00ProofState *state);

/**
 * @brief Get the current goal string
 *
 * @param state Proof state
 * @return Current goal string, or NULL if stack is empty
 */
LV00_PUBLIC_API const char *proof_state_current_goal(const Lv00ProofState *state);

/* ============== Utility Functions ============== */

/**
 * @brief Convert a proof rule type to a human-readable string
 *
 * @param type Rule type
 * @return Static string describing the rule type
 */
LV00_PUBLIC_API const char *proof_rule_type_to_string(Lv00ProofRuleType type);

/**
 * @brief Convert a search strategy to a human-readable string
 *
 * @param strategy Search strategy
 * @return Static string describing the strategy
 */
LV00_PUBLIC_API const char *search_strategy_to_string(Lv00SearchStrategy strategy);

/**
 * @brief Convert a search result status to a human-readable string
 *
 * @param status Search result status
 * @return Static string describing the status
 */
LV00_PUBLIC_API const char *search_result_status_to_string(Lv00SearchResultStatus status);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PROOF_RULE_ENGINE_H */

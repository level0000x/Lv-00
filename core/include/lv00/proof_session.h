/**
 * @file proof_session.h
 * @brief Proof session management -- REPL-style interactive proof construction
 *
 * @details Provides a session-based interface for constructing proofs step by step.
 *   Each session tracks a target proposition, maintains a proof state, and
 *   supports incremental proof step submission.
 *
 *   Inspired by Coq's interactive proof mode and Lean's tactic framework,
 *   the session manages the lifecycle of a proof attempt from creation through
 *   completion or abandonment.
 *
 * @author Lv-00 Project
 * @version 3.4.0
 */

#ifndef LV00_PROOF_SESSION_H
#define LV00_PROOF_SESSION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lv00.h"

/* ============== Forward Declarations ============== */

typedef struct Lv00ProofSession Lv00ProofSession;
typedef struct Lv00ProofState Lv00ProofState;
typedef struct Lv00RuleEngine Lv00RuleEngine;

/* ============== Configuration Constants ============== */

/** Maximum session ID length */
#define LV00_SESSION_ID_MAX 64

/** Maximum proposition length */
#define LV00_PROPOSITION_MAX 1024

/** Maximum JSON output buffer size */
#define LV00_SESSION_JSON_MAX 8192

/** Maximum proof steps per session */
#define LV00_SESSION_MAX_STEPS 4096

/* ============== Session Status ============== */

/**
 * @brief Proof session status enumeration
 */
typedef enum {
    SESSION_STATUS_ACTIVE,     /**< Session is active, accepting steps */
    SESSION_STATUS_COMPLETE,   /**< Proof completed successfully */
    SESSION_STATUS_ABANDONED,  /**< Proof was abandoned */
    SESSION_STATUS_ERROR       /**< Session encountered an error */
} Lv00SessionStatus;

/* ============== Step Result ============== */

/**
 * @brief Result of submitting a proof step
 */
typedef enum {
    STEP_RESULT_ACCEPTED,      /**< Step was accepted and applied */
    STEP_RESULT_REJECTED,      /**< Step was rejected (invalid tactic) */
    STEP_RESULT_GOAL_CHANGED,  /**< Step accepted, goal changed */
    STEP_RESULT_PROVED,        /**< Step accepted, current goal proved */
    STEP_RESULT_ERROR          /**< Internal error processing step */
} Lv00StepResult;

/* ============== Proof Session ============== */

/**
 * @brief Proof session structure
 *
 * Manages a single proof attempt with REPL-style step submission.
 * Each session has a unique ID, a target proposition, and tracks
 * the proof state including applied steps.
 */
struct Lv00ProofSession {
    /* Identity */
    char session_id[LV00_SESSION_ID_MAX]; /**< Unique session identifier */
    uint64_t created_at;                   /**< Session creation timestamp */

    /* Proof target */
    char *target_proposition; /**< The proposition to be proved */

    /* Proof state */
    Lv00ProofState *state;    /**< Current proof state (goal stack, hypotheses) */
    Lv00RuleEngine *engine;   /**< Rule engine for automated search (optional) */

    /* Step tracking */
    int step_count;           /**< Total number of submitted steps */
    bool is_complete;         /**< Whether the proof is complete */

    /* Session status */
    Lv00SessionStatus status; /**< Current session status */
};

/* ============== Session API ============== */

/**
 * @brief Create a new proof session
 *
 * Initializes a proof session with a unique ID and the given target
 * proposition. The session starts in SESSION_STATUS_ACTIVE state.
 *
 * @param target_proposition  The proposition to prove (copied internally)
 * @param engine              Optional rule engine for automated hints (may be NULL)
 * @return Pointer to new session, or NULL on failure
 */
LV00_PUBLIC_API Lv00ProofSession *proof_session_create(const char *target_proposition,
                                                        Lv00RuleEngine *engine);

/**
 * @brief Create a new proof session with a custom session ID
 *
 * @param session_id          Custom session identifier
 * @param target_proposition  The proposition to prove (copied internally)
 * @param engine              Optional rule engine for automated hints (may be NULL)
 * @return Pointer to new session, or NULL on failure
 */
LV00_PUBLIC_API Lv00ProofSession *proof_session_create_with_id(const char *session_id,
                                                                const char *target_proposition,
                                                                Lv00RuleEngine *engine);

/**
 * @brief Destroy a proof session and free all resources
 *
 * If the session owns a rule engine, the engine is NOT destroyed
 * (the caller retains ownership).
 *
 * @param session Proof session to destroy (safe to pass NULL)
 */
LV00_PUBLIC_API void proof_session_destroy(Lv00ProofSession *session);

/**
 * @brief Submit a proof step (tactic application) to the session
 *
 * The step is applied to the current proof state. If the step succeeds,
 * the goal stack may be modified (new sub-goals pushed, or current goal
 * popped if proved).
 *
 * @param session  Proof session
 * @param tactic   Tactic string describing the proof step
 * @param result   Output: result of the step application
 * @return true if the step was processed (check result for acceptance),
 *         false on invalid arguments
 */
LV00_PUBLIC_API bool proof_session_submit_step(Lv00ProofSession *session,
                                                const char *tactic,
                                                Lv00StepResult *result);

/**
 * @brief Get the current session state as a JSON string
 *
 * Returns a JSON representation of the session state including:
 *   - session_id
 *   - status
 *   - target_proposition
 *   - current_goal
 *   - goal_stack_depth
 *   - hypothesis_count
 *   - step_count
 *   - is_complete
 *
 * The returned string must be freed by the caller using lv00_free_ptr().
 *
 * @param session Proof session
 * @return JSON string (caller must free), or NULL on failure
 */
LV00_PUBLIC_API char *proof_session_get_state_json(const Lv00ProofSession *session);

/**
 * @brief Get the current session status
 *
 * @param session Proof session
 * @return Current session status, or SESSION_STATUS_ERROR if session is NULL
 */
LV00_PUBLIC_API Lv00SessionStatus proof_session_get_status(const Lv00ProofSession *session);

/**
 * @brief Check if the proof in the session is complete
 *
 * @param session Proof session
 * @return true if proof is complete, false otherwise
 */
LV00_PUBLIC_API bool proof_session_is_complete(const Lv00ProofSession *session);

/**
 * @brief Get the session ID
 *
 * @param session Proof session
 * @return Session ID string, or NULL if session is NULL
 */
LV00_PUBLIC_API const char *proof_session_get_id(const Lv00ProofSession *session);

/**
 * @brief Get the target proposition
 *
 * @param session Proof session
 * @return Target proposition string, or NULL if session is NULL
 */
LV00_PUBLIC_API const char *proof_session_get_target(const Lv00ProofSession *session);

/**
 * @brief Get the step count
 *
 * @param session Proof session
 * @return Number of steps submitted, or -1 if session is NULL
 */
LV00_PUBLIC_API int proof_session_get_step_count(const Lv00ProofSession *session);

/**
 * @brief Abandon the current proof session
 *
 * Marks the session as SESSION_STATUS_ABANDONED. No further steps
 * can be submitted.
 *
 * @param session Proof session
 * @return true on success, false on invalid arguments
 */
LV00_PUBLIC_API bool proof_session_abandon(Lv00ProofSession *session);

/**
 * @brief Reset the proof session to its initial state
 *
 * Clears all applied steps and resets the proof state to the original
 * target proposition. The session status changes back to ACTIVE.
 *
 * @param session Proof session
 * @return true on success, false on invalid arguments
 */
LV00_PUBLIC_API bool proof_session_reset(Lv00ProofSession *session);

/* ============== Utility Functions ============== */

/**
 * @brief Convert a session status to a human-readable string
 *
 * @param status Session status
 * @return Static string describing the status
 */
LV00_PUBLIC_API const char *session_status_to_string(Lv00SessionStatus status);

/**
 * @brief Convert a step result to a human-readable string
 *
 * @param result Step result
 * @return Static string describing the result
 */
LV00_PUBLIC_API const char *step_result_to_string(Lv00StepResult result);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PROOF_SESSION_H */

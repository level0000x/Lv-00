/**
 * @file proof_session.c
 * @brief Proof session management implementation
 *
 * Implements REPL-style proof session management. Each session tracks
 * a target proposition and maintains a proof state that evolves as
 * the user submits proof steps (tactics).
 *
 * The session supports:
 *   - Step submission with result tracking
 *   - JSON state export for UI integration
 *   - Session reset and abandonment
 *   - Optional rule engine integration for automated hints
 */

#include "proof_session.h"
#include "proof_rule_engine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lv00.h"
#include "lv00_utils.h"

/* ============== Internal Helpers ============== */

/**
 * @brief Generate a unique session ID based on timestamp
 *
 * Format: "sess_<timestamp_hex>"
 */
static void generate_session_id(char *buf, size_t buf_size) {
    time_t now = time(NULL);
    snprintf(buf, buf_size, "sess_%llx", (unsigned long long)now);
}

/**
 * @brief Safe string duplication
 */
static char *safe_strdup(const char *s) {
    if (!s) return NULL;
    return lv00_strdup(s);
}

/**
 * @brief Escape a string for JSON output
 *
 * Writes the escaped version of src into dst. Ensures dst is null-terminated.
 *
 * @param dst      Destination buffer
 * @param dst_size Size of destination buffer
 * @param src      Source string to escape
 */
static void json_escape_string(char *dst, size_t dst_size, const char *src) {
    size_t si = 0, di = 0;
    if (!src || dst_size == 0) {
        if (dst_size > 0) dst[0] = '\0';
        return;
    }

    while (src[si] != '\0' && di + 2 < dst_size) {
        switch (src[si]) {
            case '"':
                if (di + 2 < dst_size) { dst[di++] = '\\'; dst[di++] = '"'; }
                break;
            case '\\':
                if (di + 2 < dst_size) { dst[di++] = '\\'; dst[di++] = '\\'; }
                break;
            case '\n':
                if (di + 2 < dst_size) { dst[di++] = '\\'; dst[di++] = 'n'; }
                break;
            case '\r':
                if (di + 2 < dst_size) { dst[di++] = '\\'; dst[di++] = 'r'; }
                break;
            case '\t':
                if (di + 2 < dst_size) { dst[di++] = '\\'; dst[di++] = 't'; }
                break;
            default:
                dst[di++] = src[si];
                break;
        }
        si++;
    }
    dst[di] = '\0';
}

/* ============== Session API Implementation ============== */

Lv00ProofSession *proof_session_create(const char *target_proposition,
                                        Lv00RuleEngine *engine) {
    char auto_id[LV00_SESSION_ID_MAX];
    generate_session_id(auto_id, sizeof(auto_id));
    return proof_session_create_with_id(auto_id, target_proposition, engine);
}

Lv00ProofSession *proof_session_create_with_id(const char *session_id,
                                                const char *target_proposition,
                                                Lv00RuleEngine *engine) {
    Lv00ProofSession *session;

    if (!session_id || !target_proposition) return NULL;

    session = (Lv00ProofSession *)lv00_malloc(sizeof(Lv00ProofSession));
    if (!session) return NULL;

    memset(session, 0, sizeof(Lv00ProofSession));

    /* Set identity */
    strncpy(session->session_id, session_id, LV00_SESSION_ID_MAX - 1);
    session->session_id[LV00_SESSION_ID_MAX - 1] = '\0';
    session->created_at = (uint64_t)time(NULL);

    /* Set target proposition */
    session->target_proposition = safe_strdup(target_proposition);
    if (!session->target_proposition) {
        lv00_free((void **) &session);
        return NULL;
    }

    /* Create proof state from target */
    session->state = proof_state_create(target_proposition);
    if (!session->state) {
        lv00_free((void **) &session->target_proposition);
        lv00_free((void **) &session);
        return NULL;
    }

    /* Set optional rule engine (not owned by session) */
    session->engine = engine;

    /* Initialize tracking */
    session->step_count = 0;
    session->is_complete = false;
    session->status = SESSION_STATUS_ACTIVE;

    return session;
}

void proof_session_destroy(Lv00ProofSession *session) {
    if (!session) return;

    /* Free target proposition */
    if (session->target_proposition) {
        lv00_free((void **) &session->target_proposition);
        session->target_proposition = NULL;
    }

    /* Destroy proof state */
    if (session->state) {
        proof_state_destroy(session->state);
        session->state = NULL;
    }

    /* Note: engine is NOT owned by session, do not free it */

    lv00_free((void **) &session);
}

bool proof_session_submit_step(Lv00ProofSession *session,
                                const char *tactic,
                                Lv00StepResult *result) {
    Lv00StepResult local_result;

    if (!session || !tactic) {
        if (result) *result = STEP_RESULT_ERROR;
        return false;
    }

    /* Check session status */
    if (session->status != SESSION_STATUS_ACTIVE) {
        if (result) *result = STEP_RESULT_ERROR;
        return false;
    }

    local_result = STEP_RESULT_ACCEPTED;

    /* Process the tactic string.
     * This is a simplified implementation that handles basic tactic patterns.
     * A full implementation would parse the tactic and dispatch to the
     * appropriate rule engine or tactic interpreter. */
    if (strncmp(tactic, "intro ", 6) == 0 || strcmp(tactic, "intro") == 0) {
        /* Introduction tactic: pop current goal (assumed proved) */
        proof_state_pop_goal(session->state);
        local_result = proof_state_is_complete(session->state)
                           ? STEP_RESULT_PROVED
                           : STEP_RESULT_GOAL_CHANGED;
    } else if (strncmp(tactic, "apply ", 6) == 0) {
        /* Apply tactic: record rule application */
        const char *rule_name = tactic + 6;
        proof_state_record_rule(session->state, rule_name);
        local_result = STEP_RESULT_ACCEPTED;
    } else if (strncmp(tactic, "have ", 5) == 0) {
        /* Have tactic: add a hypothesis */
        const char *hyp = tactic + 5;
        proof_state_add_hypothesis(session->state, hyp);
        local_result = STEP_RESULT_ACCEPTED;
    } else if (strncmp(tactic, "rewrite", 7) == 0) {
        /* Rewrite tactic */
        proof_state_record_rule(session->state, "rewrite");
        local_result = STEP_RESULT_GOAL_CHANGED;
    } else if (strncmp(tactic, "cases ", 6) == 0) {
        /* Case split tactic */
        proof_state_record_rule(session->state, "case_split");
        local_result = STEP_RESULT_GOAL_CHANGED;
    } else if (strncmp(tactic, "induction ", 10) == 0) {
        /* Induction tactic */
        proof_state_record_rule(session->state, "induction");
        local_result = STEP_RESULT_GOAL_CHANGED;
    } else if (strncmp(tactic, "contradiction", 13) == 0) {
        /* Contradiction tactic */
        proof_state_record_rule(session->state, "contradiction");
        proof_state_pop_goal(session->state);
        local_result = proof_state_is_complete(session->state)
                           ? STEP_RESULT_PROVED
                           : STEP_RESULT_GOAL_CHANGED;
    } else if (strncmp(tactic, "exact ", 6) == 0) {
        /* Exact tactic: close current goal directly */
        proof_state_pop_goal(session->state);
        local_result = proof_state_is_complete(session->state)
                           ? STEP_RESULT_PROVED
                           : STEP_RESULT_GOAL_CHANGED;
    } else if (strncmp(tactic, "sorry", 5) == 0) {
        /* Admitted tactic: skip current goal */
        proof_state_pop_goal(session->state);
        local_result = proof_state_is_complete(session->state)
                           ? STEP_RESULT_PROVED
                           : STEP_RESULT_GOAL_CHANGED;
    } else {
        /* Unknown tactic */
        local_result = STEP_RESULT_REJECTED;
    }

    /* Update session state */
    if (local_result != STEP_RESULT_REJECTED && local_result != STEP_RESULT_ERROR) {
        session->step_count++;
    }

    if (local_result == STEP_RESULT_PROVED) {
        session->is_complete = true;
        session->status = SESSION_STATUS_COMPLETE;
    }

    if (result) {
        *result = local_result;
    }

    return true;
}

char *proof_session_get_state_json(const Lv00ProofSession *session) {
    char *json;
    char escaped_id[LV00_SESSION_ID_MAX * 2];
    char escaped_target[LV00_PROPOSITION_MAX * 2];
    char escaped_goal[LV00_PROPOSITION_MAX * 2];
    const char *current_goal;
    int goal_depth;
    int hyp_count;
    const char *status_str;

    if (!session) return NULL;

    /* Escape strings for JSON */
    json_escape_string(escaped_id, sizeof(escaped_id), session->session_id);
    json_escape_string(escaped_target, sizeof(escaped_target),
                       session->target_proposition ? session->target_proposition : "");

    current_goal = proof_state_current_goal(session->state);
    json_escape_string(escaped_goal, sizeof(escaped_goal),
                       current_goal ? current_goal : "");

    goal_depth = (session->state && session->state->goal_stack_top >= 0)
                     ? session->state->goal_stack_top + 1
                     : 0;
    hyp_count = session->state ? session->state->hypothesis_count : 0;

    status_str = session_status_to_string(session->status);

    /* Build JSON output */
    json = (char *)lv00_malloc(LV00_SESSION_JSON_MAX);
    if (!json) return NULL;

    snprintf(json, LV00_SESSION_JSON_MAX,
             "{\n"
             "  \"session_id\": \"%s\",\n"
             "  \"status\": \"%s\",\n"
             "  \"target_proposition\": \"%s\",\n"
             "  \"current_goal\": \"%s\",\n"
             "  \"goal_stack_depth\": %d,\n"
             "  \"hypothesis_count\": %d,\n"
             "  \"step_count\": %d,\n"
             "  \"is_complete\": %s\n"
             "}",
             escaped_id,
             status_str,
             escaped_target,
             escaped_goal,
             goal_depth,
             hyp_count,
             session->step_count,
             session->is_complete ? "true" : "false");

    return json;
}

Lv00SessionStatus proof_session_get_status(const Lv00ProofSession *session) {
    if (!session) return SESSION_STATUS_ERROR;
    return session->status;
}

bool proof_session_is_complete(const Lv00ProofSession *session) {
    if (!session) return false;
    return session->is_complete;
}

const char *proof_session_get_id(const Lv00ProofSession *session) {
    if (!session) return NULL;
    return session->session_id;
}

const char *proof_session_get_target(const Lv00ProofSession *session) {
    if (!session) return NULL;
    return session->target_proposition;
}

int proof_session_get_step_count(const Lv00ProofSession *session) {
    if (!session) return -1;
    return session->step_count;
}

bool proof_session_abandon(Lv00ProofSession *session) {
    if (!session) return false;
    if (session->status != SESSION_STATUS_ACTIVE) return false;

    session->status = SESSION_STATUS_ABANDONED;
    session->is_complete = false;
    return true;
}

bool proof_session_reset(Lv00ProofSession *session) {
    if (!session) return false;

    /* Destroy current proof state */
    if (session->state) {
        proof_state_destroy(session->state);
        session->state = NULL;
    }

    /* Recreate proof state from target proposition */
    session->state = proof_state_create(session->target_proposition);
    if (!session->state) return false;

    /* Reset tracking */
    session->step_count = 0;
    session->is_complete = false;
    session->status = SESSION_STATUS_ACTIVE;

    return true;
}

/* ============== Utility Functions ============== */

const char *session_status_to_string(Lv00SessionStatus status) {
    switch (status) {
        case SESSION_STATUS_ACTIVE:    return "ACTIVE";
        case SESSION_STATUS_COMPLETE:  return "COMPLETE";
        case SESSION_STATUS_ABANDONED: return "ABANDONED";
        case SESSION_STATUS_ERROR:     return "ERROR";
        default:                       return "UNKNOWN";
    }
}

const char *step_result_to_string(Lv00StepResult result) {
    switch (result) {
        case STEP_RESULT_ACCEPTED:     return "ACCEPTED";
        case STEP_RESULT_REJECTED:     return "REJECTED";
        case STEP_RESULT_GOAL_CHANGED: return "GOAL_CHANGED";
        case STEP_RESULT_PROVED:       return "PROVED";
        case STEP_RESULT_ERROR:        return "ERROR";
        default:                       return "UNKNOWN";
    }
}

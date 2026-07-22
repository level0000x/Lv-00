#ifndef LV00_PROOF_SESSION_INTERNAL_H
#define LV00_PROOF_SESSION_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "proof_session.h"
#include "proof_rule_engine_internal.h"

/* ============== 常量定义 ============== */
#ifndef LV00_SESSION_ID_MAX
#define LV00_SESSION_ID_MAX 64
#endif
#ifndef LV00_PROPOSITION_MAX
#define LV00_PROPOSITION_MAX 512
#endif
#ifndef LV00_SESSION_JSON_MAX
#define LV00_SESSION_JSON_MAX 2048
#endif

/* ============== 会话状态枚举 ============== */
typedef enum {
    SESSION_STATUS_ACTIVE = 0,
    SESSION_STATUS_COMPLETE,
    SESSION_STATUS_ABANDONED,
    SESSION_STATUS_ERROR
} Lv00SessionStatus;

/* ============== 步骤结果枚举 ============== */
typedef enum {
    STEP_RESULT_ACCEPTED = 0,
    STEP_RESULT_REJECTED,
    STEP_RESULT_GOAL_CHANGED,
    STEP_RESULT_PROVED,
    STEP_RESULT_ERROR
} Lv00StepResult;

/* ============== 证明会话结构体 ============== */
typedef struct Lv00ProofSession {
    char session_id[LV00_SESSION_ID_MAX];
    uint64_t created_at;
    char *target_proposition;
    Lv00ProofState *state;
    Lv00RuleEngine *engine;
    int step_count;
    bool is_complete;
    Lv00SessionStatus status;
} Lv00ProofSession;

/* ============== 函数声明 ============== */

/* 会话生命周期函数 */
Lv00ProofSession *proof_session_create_with_id(const char *session_id,
                                                const char *target_proposition,
                                                Lv00RuleEngine *engine);

/* 工具函数 */
const char *session_status_to_string(Lv00SessionStatus status);
const char *step_result_to_string(Lv00StepResult result);

/* 证明状态创建函数（被会话调用） */
Lv00ProofState *proof_state_create(const char *goal);
void proof_state_destroy(Lv00ProofState *state);
bool proof_state_pop_goal(Lv00ProofState *state);
bool proof_state_is_complete(const Lv00ProofState *state);
bool proof_state_record_rule(Lv00ProofState *state, const char *name);
bool proof_state_add_hypothesis(Lv00ProofState *state, const char *hypothesis);
const char *proof_state_current_goal(const Lv00ProofState *state);

/* Session management functions (used by test_proof_rule_engine.c) */
Lv00ProofSession *proof_session_create(const char *target, Lv00RuleEngine *engine);
void proof_session_destroy(Lv00ProofSession *session);
const char *proof_session_get_id(const Lv00ProofSession *session);
const char *proof_session_get_target(const Lv00ProofSession *session);
int proof_session_get_step_count(const Lv00ProofSession *session);
bool proof_session_is_complete(const Lv00ProofSession *session);
Lv00SessionStatus proof_session_get_status(const Lv00ProofSession *session);
bool proof_session_submit_step(Lv00ProofSession *session, const char *step, Lv00StepResult *result);
bool proof_session_reset(Lv00ProofSession *session);
bool proof_session_abandon(Lv00ProofSession *session);
char *proof_session_get_state_json(const Lv00ProofSession *session);

#ifdef __cplusplus
}
#endif

#endif

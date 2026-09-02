#ifndef lv_PROOF_SESSION_INTERNAL_H
#define lv_PROOF_SESSION_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "proof_rule_engine_internal.h"

/* ============== 常量定义 ============== */
#ifndef lv_SESSION_ID_MAX
#define lv_SESSION_ID_MAX 64
#endif
#ifndef lv_PROPOSITION_MAX
#define lv_PROPOSITION_MAX 512
#endif
#ifndef lv_SESSION_JSON_MAX
#define lv_SESSION_JSON_MAX 2048
#endif

/* ============== 会话状态枚举 ============== */
typedef enum {
    SESSION_STATUS_ACTIVE = 0,
    SESSION_STATUS_COMPLETE,
    SESSION_STATUS_ABANDONED,
    SESSION_STATUS_ERROR
} lvSessionStatus;

/* ============== 步骤结果枚举 ============== */
typedef enum {
    STEP_RESULT_ACCEPTED = 0,
    STEP_RESULT_REJECTED,
    STEP_RESULT_GOAL_CHANGED,
    STEP_RESULT_PROVED,
    STEP_RESULT_ERROR
} lvStepResult;

/* ============== 证明会话结构体 ============== */
typedef struct lvProofSession {
    char session_id[lv_SESSION_ID_MAX];
    uint64_t created_at;
    char *target_proposition;
    lvProofState *state;
    lvRuleEngine *engine;
    int step_count;
    bool is_complete;
    lvSessionStatus status;
} lvProofSession;

/* ============== 函数声明 ============== */

/* 会话生命周期函数 */
lvProofSession *proof_session_create_with_id(const char *session_id, const char *target_proposition,
                                             lvRuleEngine *engine);

/* 工具函数 */
lv_PUBLIC_API const char *session_status_to_string(lvSessionStatus status);
lv_PUBLIC_API const char *step_result_to_string(lvStepResult result);

/* 证明状态创建函数（被会话调用） */
lvProofState *proof_state_create(const char *goal);
lv_PUBLIC_API void proof_state_destroy(lvProofState *state);
lv_PUBLIC_API bool proof_state_pop_goal(lvProofState *state);
lv_PUBLIC_API bool proof_state_is_complete(const lvProofState *state);
lv_PUBLIC_API bool proof_state_record_rule(lvProofState *state, const char *name);
lv_PUBLIC_API bool proof_state_add_hypothesis(lvProofState *state, const char *hypothesis);
lv_PUBLIC_API const char *proof_state_current_goal(const lvProofState *state);

/* Session management functions (used by test_proof_rule_engine.c) */
lvProofSession *proof_session_create(const char *target, lvRuleEngine *engine);
lv_PUBLIC_API void proof_session_destroy(lvProofSession *session);
lv_PUBLIC_API const char *proof_session_get_id(const lvProofSession *session);
lv_PUBLIC_API const char *proof_session_get_target(const lvProofSession *session);
lv_PUBLIC_API int proof_session_get_step_count(const lvProofSession *session);
lv_PUBLIC_API bool proof_session_is_complete(const lvProofSession *session);
lvSessionStatus proof_session_get_status(const lvProofSession *session);
lv_PUBLIC_API bool proof_session_submit_step(lvProofSession *session, const char *step, lvStepResult *result);
lv_PUBLIC_API bool proof_session_reset(lvProofSession *session);
lv_PUBLIC_API bool proof_session_abandon(lvProofSession *session);
lv_PUBLIC_API char *proof_session_get_state_json(const lvProofSession *session);

#ifdef __cplusplus
}
#endif

#endif

#ifndef LV00_PROOF_SESSION_H
#define LV00_PROOF_SESSION_H
#ifdef __cplusplus
extern "C" {
#endif
typedef struct ProofSession { int id; const char *name; } ProofSession;
ProofSession *lv00_proof_session_create(const char *name);
void lv00_proof_session_destroy(ProofSession *s);
#ifdef __cplusplus
}
#endif
#endif

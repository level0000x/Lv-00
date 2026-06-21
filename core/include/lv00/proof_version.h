#ifndef LV00_PROOF_VERSION_H
#define LV00_PROOF_VERSION_H
/* TODO: Proof version module stub */
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct ProofVersion { int major, minor, patch; } ProofVersion;
ProofVersion lv00_proof_version_get(void);
bool lv00_proof_version_compatible(ProofVersion v);
#ifdef __cplusplus
}
#endif
#endif

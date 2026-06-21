#ifndef LV00_PROOF_EXPORT_ENHANCED_H
#define LV00_PROOF_EXPORT_ENHANCED_H
/* TODO: Proof export enhanced module stub */
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct ProofExportConfig { int version; const char *format; } ProofExportConfig;
int lv00_proof_export_enhanced(const void *proof, const ProofExportConfig *cfg, char **out);
#ifdef __cplusplus
}
#endif
#endif

#ifndef LV00_PROP_VERIFIER_H
#define LV00_PROP_VERIFIER_H
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct PropVerifierResult { bool valid; const char *msg; } PropVerifierResult;
PropVerifierResult lv00_prop_verify(const void *prop);
#ifdef __cplusplus
}
#endif
#endif

#ifndef lv_LOGIC_CHECK_H
#define lv_LOGIC_CHECK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */
#include "lv/lv.h"

lv_PUBLIC_API int lv_logic_check_tautology(const char *formula);
lv_PUBLIC_API int lv_logic_check_contradiction(const char *formula);
lv_PUBLIC_API int lv_logic_check_equivalence(const char *a, const char *b);

#ifdef __cplusplus
}
#endif

#endif /* lv_LOGIC_CHECK_H */

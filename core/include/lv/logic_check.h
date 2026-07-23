#ifndef lv_LOGIC_CHECK_H
#define lv_LOGIC_CHECK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv/lv.h"

int lv_logic_check_tautology(const char *formula);
int lv_logic_check_contradiction(const char *formula);
int lv_logic_check_equivalence(const char *a, const char *b);

#ifdef __cplusplus
}
#endif

#endif /* lv_LOGIC_CHECK_H */

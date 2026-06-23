#ifndef LV00_LOGIC_CHECK_H
#define LV00_LOGIC_CHECK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv00/lv00.h"

int lv00_logic_check_tautology(const char *formula);
int lv00_logic_check_contradiction(const char *formula);
int lv00_logic_check_equivalence(const char *a, const char *b);

#ifdef __cplusplus
}
#endif

#endif /* LV00_LOGIC_CHECK_H */

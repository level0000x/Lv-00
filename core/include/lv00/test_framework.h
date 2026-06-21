#ifndef LV00_TEST_FRAMEWORK_H
#define LV00_TEST_FRAMEWORK_H
/* TODO: Test framework module stub */

#include "lv00/lv00.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/** Test case registration. */
typedef struct { const char *name; void (*run)(void); } Lv00TestCase;

/** Compatibility macros for test code. */
#ifndef LV00_ASSERT_NOT_NULL
#define LV00_ASSERT_NOT_NULL(p) do { if (!(p)) return -1; } while(0)
#endif
#ifndef LV00_ASSERT
#define LV00_ASSERT(cond) do { if (!(cond)) return -1; } while(0)
#endif

/** Register test case. */
void lv00_test_register(const char *name, void (*run)(void));
/** Run all registered tests. */
int lv00_test_run_all(void);
/** Report test results. */
void lv00_test_report(void);

#ifdef __cplusplus
}
#endif

#endif

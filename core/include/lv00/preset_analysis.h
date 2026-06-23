#ifndef LV00_PRESET_ANALYSIS_H
#define LV00_PRESET_ANALYSIS_H
#include <stdbool.h>
#include "func_block_registry.h"
#ifdef __cplusplus
extern "C" {
#endif

#define ANALYSIS_PRESET_COUNT 20
bool preset_analysis_register(void);

#ifdef __cplusplus
}
#endif
#endif

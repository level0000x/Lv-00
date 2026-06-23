#ifndef LV00_PRESET_OPTIMIZATION_H
#define LV00_PRESET_OPTIMIZATION_H
#include <stdbool.h>
#include "func_block_registry.h"
#ifdef __cplusplus
extern "C" {
#endif

#define OPTIMIZATION_PRESET_COUNT 14
bool preset_optimization_register(void);

#ifdef __cplusplus
}
#endif
#endif

#ifndef LV00_PRESET_CORE_H
#define LV00_PRESET_CORE_H
#include <stdbool.h>
#include "func_block_registry.h"
#ifdef __cplusplus
extern "C" {
#endif

#define CORE_PRESET_COUNT 1
bool preset_core_register(void);

#ifdef __cplusplus
}
#endif
#endif

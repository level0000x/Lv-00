#ifndef LV00_PRESET_MEASUREMENTS_H
#define LV00_PRESET_MEASUREMENTS_H
#include <stdbool.h>
#include "func_block_registry.h"
#ifdef __cplusplus
extern "C" {
#endif

#define MEASUREMENTS_PRESET_COUNT 15
bool preset_measurements_register(void);

#ifdef __cplusplus
}
#endif
#endif

#ifndef LV00_PRESET_STATISTICS_H
#define LV00_PRESET_STATISTICS_H
#include <stdbool.h>
#include "func_block_registry.h"
#ifdef __cplusplus
extern "C" {
#endif

#define STATISTICS_PRESET_COUNT 16
bool preset_statistics_register(void);

#ifdef __cplusplus
}
#endif
#endif

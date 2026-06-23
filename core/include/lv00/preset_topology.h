#ifndef LV00_PRESET_TOPOLOGY_H
#define LV00_PRESET_TOPOLOGY_H
#include <stdbool.h>
#include "func_block_registry.h"
#ifdef __cplusplus
extern "C" {
#endif

#define TOPOLOGY_PRESET_COUNT 16
bool preset_topology_register(void);

#ifdef __cplusplus
}
#endif
#endif

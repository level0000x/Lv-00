#ifndef lv_PRESET_REGISTER_MACROS_H
#define lv_PRESET_REGISTER_MACROS_H
#include <stdbool.h>

#include "func_block_registry.h"
#ifdef __cplusplus
extern "C" {
#endif

#define PRESET_REGISTER_SIMPLE(name, desc, cat, in, out) preset_blocks_register_by_category(name, desc, cat, in, out)

#ifdef __cplusplus
}
#endif
#endif

#ifndef LV00_PRESET_MATRIX_H
#define LV00_PRESET_MATRIX_H
#include <stdbool.h>
#include "func_block_registry.h"
#ifdef __cplusplus
extern "C" {
#endif

#define MATRIX_PRESET_COUNT 16
bool preset_matrix_register(void);

#ifdef __cplusplus
}
#endif
#endif

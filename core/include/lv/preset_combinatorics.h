#ifndef lv_PRESET_COMBINATORICS_H
#define lv_PRESET_COMBINATORICS_H
#include <stdbool.h>
#include "func_block_registry.h"
#ifdef __cplusplus
extern "C" {
#endif

/* 公共宏 */
#ifndef PRESET_BUFFER_SIZE
#define PRESET_BUFFER_SIZE 1024
#endif
#ifndef PRESET_ID_OFFSET
#define PRESET_ID_OFFSET 1000
#endif

#define COMBINATORICS_PRESET_COUNT 14
bool preset_combinatorics_register(void);

#ifdef __cplusplus
}
#endif
#endif

#ifndef lv_PRESET_CALCULUS_H
#define lv_PRESET_CALCULUS_H
#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */
#include <stdbool.h>

#include "func_block_registry.h"
#ifdef __cplusplus
extern "C" {
#endif

/* 公共宏（从 preset_core.h 重导出） */
#ifndef PRESET_BUFFER_SIZE
#define PRESET_BUFFER_SIZE 1024
#endif
#ifndef PRESET_ID_OFFSET
#define PRESET_ID_OFFSET 1000
#endif

#define CALCULUS_PRESET_COUNT 18
lv_PUBLIC_API bool preset_calculus_register(void);

#ifdef __cplusplus
}
#endif
#endif

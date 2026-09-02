#ifndef lv_PRESET_CORE_H
#define lv_PRESET_CORE_H
#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */
#include <stdbool.h>

#include "func_block_registry.h"
#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 预设系统公共宏定义
 * ======================================================================== */

#ifndef PRESET_BUFFER_SIZE
#define PRESET_BUFFER_SIZE 1024
#endif

#ifndef PRESET_ID_OFFSET
#define PRESET_ID_OFFSET 1000
#endif

/* K75：PRESET_MAX_COUNT 定义已删——预设上限单源 lv_PRESET_MAX_COUNT（func_block.h） */

#ifndef PRESET_SYSTEM_VERSION_MAJOR
#define PRESET_SYSTEM_VERSION_MAJOR 4
#endif

#ifndef PRESET_SYSTEM_VERSION_MINOR
#define PRESET_SYSTEM_VERSION_MINOR 0
#endif

#ifndef PRESET_SYSTEM_VERSION_PATCH
#define PRESET_SYSTEM_VERSION_PATCH 0
#endif

/* ========================================================================
 * 核心预设注册
 * ======================================================================== */

#define CORE_PRESET_COUNT 1
lv_PUBLIC_API bool preset_core_register(void);

#ifdef __cplusplus
}
#endif
#endif

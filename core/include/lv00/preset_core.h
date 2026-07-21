#ifndef LV00_PRESET_CORE_H
#define LV00_PRESET_CORE_H
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

#ifndef PRESET_MAX_COUNT
#define PRESET_MAX_COUNT 10000
#endif

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
int preset_core_register(void);

#ifdef __cplusplus
}
#endif
#endif

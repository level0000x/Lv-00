#ifndef LV00_PRESET_COMMON_H
#define LV00_PRESET_COMMON_H
#include <stdbool.h>
#include <stdint.h>
#include "func_block_registry.h"
#include "preset_blocks.h"
#include "func_block_preset.h"
#ifdef __cplusplus
extern "C" {
#endif

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wjump-misses-init"
#endif

/* ========================================================================
 * 预设系统公共宏定义（从 preset_core.h 重导出）
 * ======================================================================== */

#ifndef PRESET_BUFFER_SIZE
#define PRESET_BUFFER_SIZE 1024
#endif

#ifndef PRESET_ID_OFFSET
#define PRESET_ID_OFFSET 1000
#endif

/* LV00 兼容宏 */
#ifndef LV00_PRESET_MAX_COUNT
#define LV00_PRESET_MAX_COUNT PRESET_MAX_COUNT
#endif

#ifndef LV00_PRESET_ID_OFFSET
#define LV00_PRESET_ID_OFFSET PRESET_ID_OFFSET
#endif

#ifndef LV00_PRESET_MAX_PARAMS
#define LV00_PRESET_MAX_PARAMS 32
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

#ifndef PRESET_MAX_NAME_LENGTH
#define PRESET_MAX_NAME_LENGTH 128
#endif

#ifndef PRESET_MAX_DESC_LENGTH
#define PRESET_MAX_DESC_LENGTH 512
#endif

#ifndef PRESET_MAX_INPUTS
#define PRESET_MAX_INPUTS 32
#endif

/* ========================================================================
 * 空指针检查宏（配合 goto error 模式使用）
 * ======================================================================== */

#ifndef PRESET_CHECK_NULL
#define PRESET_CHECK_NULL(ptr, label) do { if (!(ptr)) goto label; } while(0)
#endif

/* ========================================================================
 * 公共预设注册
 * ======================================================================== */

#define COMMON_PRESET_COUNT 1
bool preset_common_register(void);

size_t lv00_safe_strncpy(char *dest, const char *src, size_t dest_size);
size_t lv00_safe_strncat(char *dest, const char *src, size_t dest_size);
int lv00_safe_snprintf(char *dest, size_t dest_size, const char *fmt, ...);
int preset_properties_to_string(PresetProperty properties, char *buffer, size_t buffer_size);
bool preset_properties_from_string(const char *str, PresetProperty *properties);

#ifdef __cplusplus
}
#endif
#endif

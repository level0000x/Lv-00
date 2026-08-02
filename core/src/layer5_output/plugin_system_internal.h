/**
 * @file plugin_system_internal.h
 * @brief 模块化插件系统内部共享声明
 *
 * @details 供 plugin_system_core.c / plugin_system_load.c /
 *          plugin_system_deps.c / plugin_system_autoload.c /
 *          plugin_system_version.c 等模块共享内部数据结构与辅助函数。
 */

#ifndef lv_PLUGIN_SYSTEM_INTERNAL_H
#define lv_PLUGIN_SYSTEM_INTERNAL_H

#include "lv/plugin_system.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 内部数据结构（复用 lvPluginSystem.mutex 字段作为内部存储） */
typedef struct {
    char last_error[1024];
} PluginSystemInternal;

/* plugin_system_core.c 定义：设置系统错误消息（printf 风格格式化） */
void set_error(lvPluginSystem *system, const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif /* lv_PLUGIN_SYSTEM_INTERNAL_H */

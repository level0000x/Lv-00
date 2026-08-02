/**
 * @file plugin_system_interface.c
 * @brief LV-00 模块化插件系统 —— 接口注册与查询
 *
 * @details 由 plugin_system.c 按功能域拆分而来。
 *          共享内部数据结构与辅助函数见 plugin_system_internal.h。
 *
 * @author Lv-00 Project
 * @version 1.0
 */

#include "lv/plugin_system.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_check.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_utils.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include "lv/lv_strbuf.h"
#endif

#include "plugin_system_internal.h"

/* ============ 接口注册与查询 ============ */

/**
 * @brief 注册插件接口到系统和插件注册表
 * @param plugin 注册接口的插件指针
 * @param iface 待注册的接口指针
 * @return 成功返回 0，失败返回 -1
 */
int lv_plugin_register_interface(lvPlugin *plugin, lvPluginInterface *iface) {
    lv_CHECK_NOT_NULL(plugin);
    lv_CHECK_ARG(plugin->context != NULL, lv_ERROR_NULL_POINTER, "plugin context is NULL");
    lv_CHECK_NOT_NULL(iface);
    lv_CHECK_ARG(plugin->registered_interface_count < lv_MAX_INTERFACES, lv_ERROR_RESOURCE_EXHAUSTED,
                 "max interfaces reached");

    /* 检查是否已注册 */
    for (size_t i = 0; i < plugin->registered_interface_count; i++) {
        if (strcmp(plugin->registered_interfaces[i]->name, iface->name) == 0) {
            lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "lv_plugin_register_interface: interface already registered");
        }
    }

    /* 添加到插件注册表 */
    if (!plugin->registered_interfaces) {
        plugin->registered_interfaces =
            (lvPluginInterface **) lv_malloc(sizeof(lvPluginInterface *) * lv_MAX_INTERFACES);
        if (!plugin->registered_interfaces)
            lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_plugin_register_interface: malloc failed");
    }

    iface->owner = plugin;
    plugin->registered_interfaces[plugin->registered_interface_count++] = iface;

    /* 添加到系统注册表 */
    lvPluginSystem *system = plugin->context->system;
    if (system->interface_count < system->interface_capacity) {
        system->interfaces[system->interface_count++] = iface;
    }

    return 0;
}

/**
 * @brief 从插件和系统中注销指定名称的接口
 * @param plugin 注销接口的插件指针
 * @param name 接口名称
 * @return 成功返回 0，失败返回 -1
 */
int lv_plugin_unregister_interface(lvPlugin *plugin, const char *name) {
    lv_CHECK_NOT_NULL(plugin);
    lv_CHECK_ARG(plugin->context != NULL, lv_ERROR_NULL_POINTER, "plugin context is NULL");
    lv_CHECK_NOT_NULL(name);

    /* 从插件注册表中移除 */
    for (size_t i = 0; i < plugin->registered_interface_count; i++) {
        if (strcmp(plugin->registered_interfaces[i]->name, name) == 0) {
            /* 从系统注册表中移除 */
            lvPluginSystem *system = plugin->context->system;
            for (size_t j = 0; j < system->interface_count; j++) {
                if (system->interfaces[j] == plugin->registered_interfaces[i]) {
                    system->interfaces[j] = system->interfaces[--system->interface_count];
                    break;
                }
            }

            plugin->registered_interfaces[i] = plugin->registered_interfaces[--plugin->registered_interface_count];
            return 0;
        }
    }

    lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "lv_plugin_unregister_interface: interface not found");
}

/**
 * @brief 按名称和版本精确查询已注册的接口
 * @param system 插件系统指针
 * @param name 接口名称
 * @param version 接口版本号
 * @return 找到返回接口指针，未找到返回 NULL
 */
lvPluginInterface *lv_plugin_query_interface(lvPluginSystem *system, const char *name, uint32_t version) {
    if (!system || !name)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_plugin_query_interface: system or name is NULL");

    for (size_t i = 0; i < system->interface_count; i++) {
        if (strcmp(system->interfaces[i]->name, name) == 0 && system->interfaces[i]->version == version) {
            return system->interfaces[i];
        }
    }
    return NULL;
}

/* 通配符模式匹配：支持 '*' 和 '?' glob 通配符 */
static int wildcard_match(const char *pattern, const char *str) {
    if (!pattern || !str)
        return 0;

    const char *p = pattern;
    const char *s = str;
    const char *star_p = NULL;
    const char *star_s = NULL;

    while (*s) {
        if (*p == '*') {
            /* 记录星号位置，跳过连续星号 */
            star_p = p++;
            star_s = s;
        } else if (*p == *s || *p == '?') {
            p++;
            s++;
        } else if (star_p) {
            /* 回溯到上一个星号，多匹配一个字符 */
            p = star_p + 1;
            s = ++star_s;
        } else {
            return 0;
        }
    }

    /* 跳过 pattern 末尾的星号 */
    while (*p == '*')
        p++;

    return *p == '\0';
}

/**
 * @brief 按通配符模式查询已注册的接口列表
 * @param system 插件系统指针
 * @param pattern 通配符匹配模式（支持 '*' 和 '?'）
 * @param count 输出参数，匹配的接口数量
 * @return 成功返回匹配的接口指针数组（需调用者释放），失败返回 NULL
 */
lvPluginInterface **lv_plugin_query_interfaces(lvPluginSystem *system, const char *pattern, size_t *count) {
    if (!system || !pattern || !count)
        return NULL;

    /* 第一遍：统计匹配数量 */
    size_t match_count = 0;
    for (size_t i = 0; i < system->interface_count; i++) {
        if (wildcard_match(pattern, system->interfaces[i]->name)) {
            match_count++;
        }
    }

    if (match_count == 0) {
        *count = 0;
        return NULL;
    }

    /* 分配结果数组 */
    lvPluginInterface **result = (lvPluginInterface **) lv_malloc(sizeof(lvPluginInterface *) * match_count);
    if (!result) {
        *count = 0;
        return NULL;
    }

    /* 第二遍：填充匹配结果 */
    size_t idx = 0;
    for (size_t i = 0; i < system->interface_count; i++) {
        if (wildcard_match(pattern, system->interfaces[i]->name)) {
            result[idx++] = system->interfaces[i];
        }
    }

    *count = match_count;
    return result;
}


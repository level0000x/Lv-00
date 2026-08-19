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
#include "lv/lv_registry.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_thread.h"
#include "lv/lv_utils.h"

#include "plugin_system_internal.h"

/* ============ 接口注册与查询 ============ */

/* ============================================================
 * 接口注册表（泛型注册表设施 lv_registry）
 *
 * 插件级注册表：key = "P:<plugin>:<name>"，value = lvPluginInterface*。
 *   每个插件独立查重（同一插件不得重复注册同名接口）。
 * 系统级注册表：key = "S:<system>:<iface>"，value = lvPluginInterface*。
 *   每个接口指针唯一，支持不同插件注册同名接口，按指针精确删除。
 *
 * 两个注册表均为文件级单例（lv_once 惰性初始化，线程安全）。
 * plugin->registered_interfaces / system->interfaces 数组保留为影子视图，
 * 供 plugin_system_load.c 卸载流程遍历注销与释放，以及 version.c 统计输出。
 * ============================================================ */

lv_REGISTRY_STATIC(plugin_iface_registry, 16);

lv_REGISTRY_STATIC(system_iface_registry, lv_MAX_INTERFACES);

/** @brief 同时确保插件级与系统级接口注册表已初始化 */
static inline void iface_registry_ensure(void) {
    plugin_iface_registry_ensure();
    system_iface_registry_ensure();
}

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

    lvPluginSystem *system = plugin->context->system;
    iface_registry_ensure();

    /* 确保影子数组可分配（供 unload 遍历注销，见 plugin_system_load.c） */
    if (!plugin->registered_interfaces) {
        plugin->registered_interfaces =
            (lvPluginInterface **) lv_malloc(sizeof(lvPluginInterface *) * lv_MAX_INTERFACES);
        if (!plugin->registered_interfaces)
            lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_plugin_register_interface: malloc failed");
    }

    /* 插件级查重（per-plugin：同一插件不能注册同名接口），
     * 由 lv_registry 完成 strcmp 查重 + 尾部追加 + 扩容 */
    char plugin_key[lv_PLUGIN_NAME_MAX + 32];
    lv_snprintf(plugin_key, sizeof(plugin_key), "P:%p:%s", (const void *) plugin, iface->name);
    if (!lv_registry_put(&g_plugin_iface_registry, plugin_key, iface)) {
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "lv_plugin_register_interface: interface already registered");
    }

    iface->owner = plugin;
    plugin->registered_interfaces[plugin->registered_interface_count++] = iface;

    /* 系统级注册表（key 按 iface 指针唯一，允许不同插件注册同名接口）。
     * 容量不足或内存不足时跳过，与旧语义（容量满仅跳过系统注册表）一致 */
    if (system && system->interface_count < system->interface_capacity) {
        char system_key[64];
        lv_snprintf(system_key, sizeof(system_key), "S:%p:%p", (const void *) system, (const void *) iface);
        lv_registry_put(&g_system_iface_registry, system_key, iface);
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

    iface_registry_ensure();

    /* 从插件注册表中查找并移除（lv_registry_remove 前移紧凑） */
    char plugin_key[lv_PLUGIN_NAME_MAX + 32];
    lv_snprintf(plugin_key, sizeof(plugin_key), "P:%p:%s", (const void *) plugin, name);
    lvPluginInterface *iface = (lvPluginInterface *) lv_registry_get(&g_plugin_iface_registry, plugin_key);
    if (!iface) {
        lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "lv_plugin_unregister_interface: interface not found");
    }
    lv_registry_remove(&g_plugin_iface_registry, plugin_key);

    /* 从系统注册表移除（按 iface 指针精确删除） */
    lvPluginSystem *system = plugin->context->system;
    if (system) {
        char system_key[64];
        lv_snprintf(system_key, sizeof(system_key), "S:%p:%p", (const void *) system, (const void *) iface);
        lv_registry_remove(&g_system_iface_registry, system_key);

        /* 影子数组按指针删除（尾部交换，保持旧语义） */
        for (size_t j = 0; j < system->interface_count; j++) {
            if (system->interfaces[j] == iface) {
                system->interfaces[j] = system->interfaces[--system->interface_count];
                break;
            }
        }
    }

    /* 插件影子数组按指针删除（尾部交换，保持旧语义） */
    if (plugin->registered_interfaces) {
        for (size_t i = 0; i < plugin->registered_interface_count; i++) {
            if (plugin->registered_interfaces[i] == iface) {
                plugin->registered_interfaces[i] = plugin->registered_interfaces[--plugin->registered_interface_count];
                break;
            }
        }
    }

    return 0;
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

    iface_registry_ensure();

    /* 遍历系统级注册表条目，仅匹配当前 system */
    char prefix[64];
    lv_snprintf(prefix, sizeof(prefix), "S:%p:", (const void *) system);

    int count = lv_registry_count(&g_system_iface_registry);
    for (int i = 0; i < count; i++) {
        const char *entry_name = NULL;
        void *entry_value = NULL;
        if (!lv_registry_get_at(&g_system_iface_registry, i, &entry_name, &entry_value)) {
            continue;
        }
        if (!lv_str_startswith(entry_name, prefix)) {
            continue;
        }
        lvPluginInterface *iface = (lvPluginInterface *) entry_value;
        if (lv_str_eq(iface->name, name) && iface->version == version) {
            return iface;
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

    iface_registry_ensure();

    /* 遍历系统级注册表条目，仅匹配当前 system */
    char prefix[64];
    lv_snprintf(prefix, sizeof(prefix), "S:%p:", (const void *) system);

    int total = lv_registry_count(&g_system_iface_registry);

    /* 第一遍：统计匹配数量 */
    size_t match_count = 0;
    for (int i = 0; i < total; i++) {
        const char *entry_name = NULL;
        void *entry_value = NULL;
        if (!lv_registry_get_at(&g_system_iface_registry, i, &entry_name, &entry_value)) {
            continue;
        }
        if (!lv_str_startswith(entry_name, prefix)) {
            continue;
        }
        if (wildcard_match(pattern, ((lvPluginInterface *) entry_value)->name)) {
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
    for (int i = 0; i < total; i++) {
        const char *entry_name = NULL;
        void *entry_value = NULL;
        if (!lv_registry_get_at(&g_system_iface_registry, i, &entry_name, &entry_value)) {
            continue;
        }
        if (!lv_str_startswith(entry_name, prefix)) {
            continue;
        }
        lvPluginInterface *iface = (lvPluginInterface *) entry_value;
        if (wildcard_match(pattern, iface->name)) {
            result[idx++] = iface;
        }
    }

    *count = match_count;
    return result;
}


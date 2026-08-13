/**
 * @file plugin_system_deps.c
 * @brief LV-00 模块化插件系统 —— 依赖管理
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
#include "lv/lv_str_utils.h"
#include "lv/lv_utils.h"

#include "plugin_system_internal.h"

/* ============ 依赖管理 ============ */

/**
 * @brief 解析并激活指定插件的所有依赖
 * @param system 插件系统指针
 * @param plugin 待解析依赖的插件指针
 * @return 成功返回 0，缺失必需依赖时返回 -1
 */
int lv_plugin_resolve_dependencies(lvPluginSystem *system, lvPlugin *plugin) {
    if (!system || !plugin)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "lv_plugin_resolve_dependencies: system or plugin is NULL");
    if (!plugin->info.dependencies && plugin->info.dependency_count > 0)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "lv_plugin_resolve_dependencies: deps array NULL but count > 0");

    for (size_t i = 0; i < plugin->info.dependency_count; i++) {
        lvPluginDependency *dep = plugin->info.dependencies[i];
        lvPlugin *dep_plugin = lv_plugin_find(system, dep->name);

        if (!dep_plugin) {
            if (!dep->optional) {
                plugin_system_set_error(system, "Required dependency not found: %s", dep->name);
                lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "lv_plugin_resolve_dependencies: required dependency not found");
            }
            continue;
        }

        /* 检查版本兼容性 */
        if (!lv_plugin_check_version(dep->version_constraint, dep_plugin->info.version)) {
            if (!dep->optional) {
                plugin_system_set_error(system, "Dependency version mismatch: %s", dep->name);
                lv_RETURN_ERROR(lv_ERROR_UNSUPPORTED, "lv_plugin_resolve_dependencies: version mismatch");
            }
        }

        /* 激活依赖 */
        if (dep_plugin->state != lv_PLUGIN_STATE_ACTIVE) {
            if (lv_plugin_activate(dep_plugin) != 0) {
                if (!dep->optional) {
                    plugin_system_set_error(system, "Failed to activate dependency: %s", dep->name);
                    lv_RETURN_ERROR(lv_ERROR_INTERNAL, "lv_plugin_resolve_dependencies: activation failed");
                }
            }
        }
    }

    return 0;
}

/**
 * @brief 检查插件是否包含非可选的必需依赖
 * @param plugin 插件指针
 * @return 有非可选依赖返回 0，无非可选依赖返回 1，出错返回 -1
 */
int lv_plugin_check_dependencies(const lvPlugin *plugin) {
    /* NULL 输入视为无依赖，检查通过 */
    if (!plugin)
        return 0;
    lv_CHECK_ARG(plugin->info.dependencies != NULL || plugin->info.dependency_count == 0, lv_ERROR_INTERNAL,
                 "deps array NULL but count > 0");

    for (size_t i = 0; i < plugin->info.dependency_count; i++) {
        if (!plugin->info.dependencies[i]->optional) {
            return 0; /* 至少有一个非可选依赖 */
        }
    }

    return 1; /* 没有非可选依赖 */
}

/**
 * @brief 获取所有依赖指定插件的插件列表
 * @param system 插件系统指针
 * @param plugin 被依赖的插件指针
 * @param count 输出参数，依赖者数量
 * @return 成功返回依赖者数组（需调用者释放），失败返回 NULL
 */
lvPlugin **lv_plugin_get_dependents(lvPluginSystem *system, const lvPlugin *plugin, size_t *count) {
    if (!system || !plugin || !count)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_plugin_get_dependents: invalid parameters");

    /* 统计依赖此插件的插件数量 */
    size_t dependent_count = 0;
    for (size_t i = 0; i < system->plugin_count; i++) {
        if (!system->plugins[i]->info.dependencies)
            continue;
        for (size_t j = 0; j < system->plugins[i]->info.dependency_count; j++) {
            if (lv_str_eq(system->plugins[i]->info.dependencies[j]->name, plugin->info.name)) {
                dependent_count++;
                break;
            }
        }
    }

    if (dependent_count == 0) {
        *count = 0;
        return NULL;
    }

    /* 分配结果数组 */
    lvPlugin **result = (lvPlugin **) lv_malloc(sizeof(lvPlugin *) * dependent_count);
    if (!result) {
        *count = 0;
        return NULL;
    }

    /* 填充结果 */
    size_t idx = 0;
    for (size_t i = 0; i < system->plugin_count; i++) {
        if (!system->plugins[i]->info.dependencies)
            continue;
        for (size_t j = 0; j < system->plugins[i]->info.dependency_count; j++) {
            if (lv_str_eq(system->plugins[i]->info.dependencies[j]->name, plugin->info.name)) {
                result[idx++] = system->plugins[i];
                break;
            }
        }
    }

    *count = dependent_count;
    return result;
}


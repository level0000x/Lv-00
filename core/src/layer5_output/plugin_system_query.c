/**
 * @file plugin_system_query.c
 * @brief LV-00 模块化插件系统 —— 插件查询
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

/* ============ 插件查询 ============ */

/**
 * @brief 按名称查找已加载的插件
 * @param system 插件系统指针
 * @param name 插件名称
 * @return 找到返回插件指针，未找到返回 NULL
 */
lvPlugin *lv_plugin_find(lvPluginSystem *system, const char *name) {
    if (!system)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_plugin_find: system is NULL");
    if (!name)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_plugin_find: name is NULL");

    for (size_t i = 0; i < system->plugin_count; i++) {
        if (lv_str_eq(system->plugins[i]->info.name, name)) {
            return system->plugins[i];
        }
    }
    return NULL;
}

/**
 * @brief 获取所有已加载插件的数组
 * @param system 插件系统指针
 * @param count 输出参数，插件数量
 * @return 返回插件指针数组，失败返回 NULL
 */
lvPlugin **lv_plugin_get_all(lvPluginSystem *system, size_t *count) {
    if (!system)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_plugin_get_all: system is NULL");
    if (!count)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_plugin_get_all: count is NULL");

    *count = system->plugin_count;
    return system->plugins;
}

/**
 * @brief 按类型筛选已加载的插件
 * @param system 插件系统指针
 * @param type 插件类型
 * @param count 输出参数，匹配的插件数量
 * @return 成功返回匹配的插件指针数组（需调用者释放），失败返回 NULL
 */
lvPlugin **lv_plugin_get_by_type(lvPluginSystem *system, lvPluginType type, size_t *count) {
    if (!system || !count)
        return NULL;

    /* 统计匹配数量 */
    size_t match_count = 0;
    for (size_t i = 0; i < system->plugin_count; i++) {
        if (system->plugins[i]->info.type == type) {
            match_count++;
        }
    }

    if (match_count == 0) {
        *count = 0;
        return NULL;
    }

    /* 分配结果数组 */
    lvPlugin **result = (lvPlugin **) lv_malloc(sizeof(lvPlugin *) * match_count);
    if (!result) {
        *count = 0;
        return NULL;
    }

    /* 填充结果 */
    size_t idx = 0;
    for (size_t i = 0; i < system->plugin_count; i++) {
        if (system->plugins[i]->info.type == type) {
            result[idx++] = system->plugins[i];
        }
    }

    *count = match_count;
    return result;
}

/**
 * @brief 按状态筛选已加载的插件
 * @param system 插件系统指针
 * @param state 插件状态
 * @param count 输出参数，匹配的插件数量
 * @return 成功返回匹配的插件指针数组（需调用者释放），失败返回 NULL
 */
lvPlugin **lv_plugin_get_by_state(lvPluginSystem *system, lvPluginState state, size_t *count) {
    if (!system || !count)
        return NULL;

    /* 统计匹配数量 */
    size_t match_count = 0;
    for (size_t i = 0; i < system->plugin_count; i++) {
        if (system->plugins[i]->state == state) {
            match_count++;
        }
    }

    if (match_count == 0) {
        *count = 0;
        return NULL;
    }

    /* 分配结果数组 */
    lvPlugin **result = (lvPlugin **) lv_malloc(sizeof(lvPlugin *) * match_count);
    if (!result) {
        *count = 0;
        return NULL;
    }

    /* 填充结果 */
    size_t idx = 0;
    for (size_t i = 0; i < system->plugin_count; i++) {
        if (system->plugins[i]->state == state) {
            result[idx++] = system->plugins[i];
        }
    }

    *count = match_count;
    return result;
}


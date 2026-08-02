/**
 * @file plugin_system_version.c
 * @brief LV-00 模块化插件系统 —— 版本兼容性、插件信息与错误处理
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

/* ============ 版本兼容性 ============ */

/* 版本兼容性常量 */
#define lv_PLUGIN_VERSION_OK 1
#define lv_PLUGIN_VERSION_MISMATCH 0

/* 解析语义版本字符串 "major.minor.patch"，返回 sscanf 匹配项数 */
static int parse_semver(const char *ver_str, int *major, int *minor, int *patch) {
    if (!ver_str)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "parse_semver: ver_str is NULL");
    *major = *minor = *patch = 0;
    return sscanf(ver_str, "%d.%d.%d", major, minor, patch);
}

/**
 * @brief 检查版本号是否满足要求（语义版本比较）
 * @param required 要求的版本字符串
 * @param provided 提供的版本字符串
 * @return 兼容返回 lv_PLUGIN_VERSION_OK (1)，不兼容返回 lv_PLUGIN_VERSION_MISMATCH (0)
 */
int lv_plugin_check_version(const char *required, const char *provided) {
    if (!required || !provided)
        return lv_PLUGIN_VERSION_MISMATCH;

    /* 解析 required 版本 */
    int req_major, req_minor, req_patch;
    if (parse_semver(required, &req_major, &req_minor, &req_patch) < 1) {
        return lv_PLUGIN_VERSION_MISMATCH;
    }

    /* 解析 provided 版本 */
    int prov_major, prov_minor, prov_patch;
    if (parse_semver(provided, &prov_major, &prov_minor, &prov_patch) < 1) {
        return lv_PLUGIN_VERSION_MISMATCH;
    }

    /* 语义版本比较：逐级比较 major -> minor -> patch */
    if (prov_major > req_major)
        return lv_PLUGIN_VERSION_OK;
    if (prov_major < req_major)
        return lv_PLUGIN_VERSION_MISMATCH;

    /* major 相同，比较 minor */
    if (prov_minor > req_minor)
        return lv_PLUGIN_VERSION_OK;
    if (prov_minor < req_minor)
        return lv_PLUGIN_VERSION_MISMATCH;

    /* minor 相同，比较 patch */
    if (prov_patch >= req_patch)
        return lv_PLUGIN_VERSION_OK;

    return lv_PLUGIN_VERSION_MISMATCH;
}

/**
 * @brief 检查 API 版本兼容性（provided >= required）
 * @param required 要求的 API 版本
 * @param provided 提供的 API 版本
 * @return 兼容返回 1，不兼容返回 0
 */
int lv_plugin_check_api_compatibility(uint32_t required, uint32_t provided) {
    return provided >= required;
}

/* ============ 插件信息 ============ */

/**
 * @brief 获取插件信息的 JSON 字符串
 * @param plugin 插件指针
 * @return 成功返回 JSON 字符串（需调用者释放），失败返回 NULL
 */
char *lv_plugin_get_info_json(const lvPlugin *plugin) {
    if (!plugin)
        return NULL;

    size_t size = 1024;
    char *json = (char *) lv_malloc(size);
    if (!json)
        return NULL;

    snprintf(json, size,
             "{"
             "\"name\":\"%s\","
             "\"version\":\"%s\","
             "\"author\":\"%s\","
             "\"description\":\"%s\","
             "\"state\":%d,"
             "\"type\":%d"
             "}",
             plugin->info.name, plugin->info.version, plugin->info.author, plugin->info.description, plugin->state,
             plugin->info.type);

    return json;
}

/**
 * @brief 获取插件系统完整信息的 JSON 字符串
 * @param system 插件系统指针
 * @return 成功返回 JSON 字符串（需调用者释放），失败返回 NULL
 */
char *lv_plugin_system_get_info_json(const lvPluginSystem *system) {
    if (!system)
        return NULL;

    /* 防止整数溢出 */
    size_t plugin_size;
    if (system->plugin_count > (SIZE_MAX - 2048) / 512) {
        return NULL; /* overflow */
    }
    size_t size = 2048 + system->plugin_count * 512;
    char *json = (char *) lv_malloc(size);
    if (!json)
        return NULL;

    char *ptr = json;
    size_t remaining = size;
    int written = snprintf(ptr, remaining,
                           "{"
                           "\"version\":%u,"
                           "\"plugin_count\":%zu,"
                           "\"interface_count\":%zu,"
                           "\"plugins\":[",
                           system->version, system->plugin_count, system->interface_count);
    if (written > 0) {
        ptr += written;
        remaining -= written;
    }

    for (size_t i = 0; i < system->plugin_count; i++) {
        if (remaining <= 0)
            break;
        written = snprintf(ptr, (size_t) remaining,
                           "{"
                           "\"name\":\"%s\","
                           "\"version\":\"%s\","
                           "\"state\":%d"
                           "}%s",
                           system->plugins[i]->info.name, system->plugins[i]->info.version, system->plugins[i]->state,
                           (i < system->plugin_count - 1) ? "," : "");
        if (written > 0) {
            ptr += written;
            remaining -= written;
        }
    }

    if (remaining > 0) {
        snprintf(ptr, (size_t) remaining, "]}");
    }

    return json;
}

/* ============ 错误处理 ============ */

/**
 * @brief 获取指定插件最近一次的错误消息
 * @param plugin 插件指针
 * @return 错误字符串，无错误返回空字符串
 */
const char *lv_plugin_get_last_error(const lvPlugin *plugin) {
    if (!plugin || !plugin->context)
        return NULL;
    if (!plugin->context->system)
        return NULL;

    PluginSystemInternal *internal = (PluginSystemInternal *) plugin->context->system->mutex;
    return internal->last_error;
}

/**
 * @brief 获取插件系统的最近一次错误消息
 * @param system 插件系统指针
 * @return 错误字符串，无错误返回空字符串
 */
const char *lv_plugin_system_get_last_error(const lvPluginSystem *system) {
    if (!system)
        return NULL;

    PluginSystemInternal *internal = (PluginSystemInternal *) system->mutex;
    return internal->last_error;
}

/**
 * @brief 清除指定插件的错误消息
 * @param plugin 插件指针
 */
void lv_plugin_clear_error(lvPlugin *plugin) {
    if (!plugin || !plugin->context)
        return;
    if (!plugin->context->system)
        return;

    PluginSystemInternal *internal = (PluginSystemInternal *) plugin->context->system->mutex;
    internal->last_error[0] = '\0';
}

/**
 * @brief 清除插件系统的错误消息
 * @param system 插件系统指针
 */
void lv_plugin_system_clear_error(lvPluginSystem *system) {
    if (!system)
        return;

    PluginSystemInternal *internal = (PluginSystemInternal *) system->mutex;
    internal->last_error[0] = '\0';
}

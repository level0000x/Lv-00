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
#include "lv/lv_json.h"
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

    /* 使用 lvJsonBuf 自动对字符串字段做 JSON 转义（name/version/author/description）；
     * 对象级 API：键/标量值自动管理逗号（紧凑输出与旧手写模板字节一致） */
    lvJsonBuf buf;
    if (!lv_json_buf_init(&buf, 1024))
        return NULL;

    lv_json_buf_begin_object(&buf);
    lv_json_buf_append_key(&buf, "name");
    lv_json_buf_append_string(&buf, plugin->info.name);
    lv_json_buf_append_key(&buf, "version");
    lv_json_buf_append_string(&buf, plugin->info.version);
    lv_json_buf_append_key(&buf, "author");
    lv_json_buf_append_string(&buf, plugin->info.author);
    lv_json_buf_append_key(&buf, "description");
    lv_json_buf_append_string(&buf, plugin->info.description);
    lv_json_buf_append_key(&buf, "state");
    lv_json_buf_append_int(&buf, plugin->state);
    lv_json_buf_append_key(&buf, "type");
    lv_json_buf_append_int(&buf, plugin->info.type);
    lv_json_buf_end_object(&buf);

    return lv_json_buf_finalize(&buf);
}

/**
 * @brief 获取插件系统完整信息的 JSON 字符串
 * @param system 插件系统指针
 * @return 成功返回 JSON 字符串（需调用者释放），失败返回 NULL
 */
char *lv_plugin_system_get_info_json(const lvPluginSystem *system) {
    if (!system)
        return NULL;

    /* 使用 lvJsonBuf 动态构建（自动转义 name/version 字符串字段，无固定缓冲截断风险）；
     * 对象级 API：键/标量值自动管理逗号（紧凑输出与旧手写模板字节一致） */
    lvJsonBuf buf;
    if (!lv_json_buf_init(&buf, 2048))
        return NULL;

    lv_json_buf_begin_object(&buf);
    lv_json_buf_append_key(&buf, "version");
    lv_json_buf_append_int(&buf, system->version);
    lv_json_buf_append_key(&buf, "plugin_count");
    lv_json_buf_append_int(&buf, (long long) system->plugin_count);
    lv_json_buf_append_key(&buf, "interface_count");
    lv_json_buf_append_int(&buf, (long long) system->interface_count);

    /* plugins：对象数组，begin_object 自动管理逗号 */
    lv_json_buf_append_key(&buf, "plugins");
    lv_json_buf_begin_array(&buf);
    for (size_t i = 0; i < system->plugin_count; i++) {
        lv_json_buf_begin_object(&buf);
        lv_json_buf_append_key(&buf, "name");
        lv_json_buf_append_string(&buf, system->plugins[i]->info.name);
        lv_json_buf_append_key(&buf, "version");
        lv_json_buf_append_string(&buf, system->plugins[i]->info.version);
        lv_json_buf_append_key(&buf, "state");
        lv_json_buf_append_int(&buf, system->plugins[i]->state);
        lv_json_buf_end_object(&buf);
    }
    lv_json_buf_end_array(&buf);

    lv_json_buf_end_object(&buf);

    return lv_json_buf_finalize(&buf);
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

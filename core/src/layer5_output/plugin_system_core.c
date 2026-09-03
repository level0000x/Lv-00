/**
 * @file plugin_system_core.c
 * @brief LV-00 模块化插件系统 —— 内部数据结构、辅助函数与生命周期管理
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
#include "lv/lv_error.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_utils.h"
#include "lv/lv_lifecycle.h" /* lv_DEFER */

#include "plugin_system_internal.h"

/* ============ 内部数据结构（typedef 见 plugin_system_internal.h） ============ */

/* ============ 辅助函数 ============ */

/* 设置系统错误消息（支持 printf 风格格式化） */
void plugin_system_set_error(lvPluginSystem *system, const char *format, ...) {
    if (!system)
        return;

    PluginSystemInternal *internal = (PluginSystemInternal *) system->mutex;
    if (!internal)
        return;

    /* 公共写入口：模块级 last_error 通道统一走 lv_error.h 的 lv_ERROR_SLOT_SET */
    lv_ERROR_SLOT_SET(internal->last_error, sizeof(internal->last_error), format);
}

/* ============ 生命周期管理 ============ */

/* 插件系统部分构建守卫：guard 持有 system 值拷贝，任一成员分配失败时
 * 统一释放已分配成员与外壳（lv_free NULL 安全），替代递增回滚样板 */
typedef struct {
    lvPluginSystem *sys;
} PluginSystemGuard;

static void plugin_system_guard_cleanup(void *p) {
    PluginSystemGuard *g = (PluginSystemGuard *) p;
    if (g->sys) {
        lv_free((void **) &g->sys->plugins);
        lv_free((void **) &g->sys->interfaces);
        lv_free((void **) &g->sys->mutex);
        lv_free((void **) &g->sys);
    }
}

/**
 * @brief 创建插件系统实例
 * @param ctx LV-00 上下文指针
 * @return 成功返回插件系统指针，失败返回 NULL
 */
lvPluginSystem *lv_plugin_system_create(lvContext *ctx) {
    lvPluginSystem *system = (lvPluginSystem *) lv_calloc(1, sizeof(lvPluginSystem));
    if (!system)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_plugin_system_create: lv_calloc failed");

    memset(system, 0, sizeof(lvPluginSystem));

    system->lv_context = ctx;
    system->version =
        (lv_PLUGIN_SYSTEM_VERSION_MAJOR << 16) | (lv_PLUGIN_SYSTEM_VERSION_MINOR << 8) | lv_PLUGIN_SYSTEM_VERSION_PATCH;

    /* 部分构建守卫：后续任一分配失败自动释放已分配成员；成功路径 guard.sys = NULL 解除 */
    PluginSystemGuard guard = {system};
    lv_DEFER(plugin_system_guard_cleanup, &guard);

    /* K75 限制常量单源：容量读配置系统 A（integration.max_plugins / max_interfaces，
     * X 宏键名即宏名），默认值 lv_MAX_PLUGINS/lv_MAX_INTERFACES（256/128）与
     * config.h 权威默认一致——消除「宏硬编码 vs 配置键」同值双源漂移 */
    system->plugin_capacity = lv_config_get_int("max_plugins", lv_MAX_PLUGINS);
    if (system->plugin_capacity <= 0)
        system->plugin_capacity = lv_MAX_PLUGINS; /* 配置非法（<=0）回退权威默认 */
    system->plugins = (lvPlugin **) lv_malloc(sizeof(lvPlugin *) * system->plugin_capacity);
    if (!system->plugins)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_plugin_system_create: plugins malloc failed");

    system->interface_capacity = lv_config_get_int("max_interfaces", lv_MAX_INTERFACES);
    if (system->interface_capacity <= 0)
        system->interface_capacity = lv_MAX_INTERFACES; /* 配置非法（<=0）回退权威默认 */
    system->interfaces = (lvPluginInterface **) lv_malloc(sizeof(lvPluginInterface *) * system->interface_capacity);
    if (!system->interfaces)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_plugin_system_create: interfaces malloc failed");

    lv_darray_init(&system->search_paths, sizeof(char *));

    PluginSystemInternal *internal = (PluginSystemInternal *) lv_calloc(1, sizeof(PluginSystemInternal));
    if (!internal)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_plugin_system_create: internal calloc failed");

    memset(internal, 0, sizeof(PluginSystemInternal));
    system->mutex = internal;

    guard.sys = NULL; /* 守卫解除：结果移交调用方 */
    return system;
}

/**
 * @brief 销毁插件系统实例，释放所有相关资源
 * @param system 插件系统指针
 */
void lv_plugin_system_destroy(lvPluginSystem *system) {
    if (!system)
        return;

    lv_plugin_system_cleanup(system);

    if (system->plugins)
        lv_free((void **) &system->plugins);
    if (system->interfaces)
        lv_free((void **) &system->interfaces);

    for (int i = 0; i < system->search_paths.count; i++) {
        lv_free((void **) lv_darray_get(&system->search_paths, i));
    }
    lv_darray_free(&system->search_paths);

    if (system->mutex)
        lv_free((void **) &system->mutex);
    lv_free((void **) &system);
}

/**
 * @brief 初始化插件系统
 * @param system 插件系统指针
 * @return 成功返回 0，失败返回 -1
 */
int lv_plugin_system_init(lvPluginSystem *system) {
    lv_CHECK_NOT_NULL(system);

    system->initialized = 1;
    return 0;
}

/**
 * @brief 清理插件系统，卸载所有已加载的插件
 * @param system 插件系统指针
 */
void lv_plugin_system_cleanup(lvPluginSystem *system) {
    if (!system)
        return;
    if (!system->plugins || system->plugin_count <= 0)
        return;

    /* 卸载所有插件 */
    while (system->plugin_count > 0) {
        lv_plugin_unload(system, system->plugins[0]);
    }

    system->initialized = 0;
}


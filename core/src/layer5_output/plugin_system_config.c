/**
 * @file plugin_system_config.c
 * @brief LV-00 模块化插件系统 —— 插件配置
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
#include "lv/lv_file.h"
#include "lv/lv_registry.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_thread.h"
#include "lv/lv_utils.h"

#include "plugin_system_internal.h"

/* ============ 插件配置 ============ */

/* ============================================================
 * 与配置系统 A（lvConfig，config.h / lv_config.c）的关系
 *
 * 本模块是插件系统的 per-instance 配置：以泛型注册表（lv_registry）
 * 存储，复合 key "C:<config>:<key>"（config 为实例指针）使多个
 * lvPluginConfig 实例互不干扰，生命周期随实例创建/销毁。
 * 配置系统 A 是进程级单例 lvConfig（X-macro 标量键 + JSON 持久化），
 * 语义与生命周期均不同（A 无法表达"按实例隔离的键空间"），
 * 并入 A 需引入按实例注册表，收益低、风险高，故保持独立。
 * ============================================================ */

/* ============================================================
 * 配置项注册表（泛型注册表设施 lv_registry）
 *
 * key = "C:<config>:<key>"，value = lvPluginConfigEntry*。
 * 复合 key 使多个 lvPluginConfig 实例互不干扰（per-instance 查重/覆盖），
 * 注册表内部拷贝 key 并管理生命周期，entry 由 destroy 回调释放。
 * ============================================================ */

/** @brief 配置项注册表（文件级单例） */
lv_REGISTRY_STATIC(config_registry, 64);

/** @brief 构造复合注册表 key（栈缓冲区，调用方提供） */
static void config_build_key(const lvPluginConfig *config, const char *key, char *buf, size_t bufsz) {
    lv_snprintf(buf, bufsz, "C:%p:%s", (const void *) config, key);
}

/** @brief 配置项析构回调（lv_registry remove/destroy 时释放 entry） */
static void config_entry_destroy(void *value) {
    lvPluginConfigEntry *entry = (lvPluginConfigEntry *) value;
    lv_free((void **) &entry);
}

/**
 * @brief 创建插件配置对象
 * @return 成功返回配置指针，失败返回 NULL
 */
lvPluginConfig *lv_plugin_config_create(void) {
    lvPluginConfig *config = (lvPluginConfig *) lv_calloc(1, sizeof(lvPluginConfig));
    if (!config)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_plugin_config_create: config calloc failed");

    config_registry_ensure();

    return config;
}

/**
 * @brief 销毁插件配置对象，释放资源
 * @param config 插件配置指针
 */
void lv_plugin_config_destroy(lvPluginConfig *config) {
    if (!config)
        return;

    /* 从注册表移除并释放该 config 的所有条目（按前缀批量移除） */
    config_registry_ensure();
    char prefix[64];
    lv_snprintf(prefix, sizeof(prefix), "C:%p:", (const void *) config);
    lv_registry_remove_prefix(&g_config_registry, prefix);

    if (config->entries)
        lv_free((void **) &config->entries);
    lv_free((void **) &config);
}

/**
 * @brief lv_ini_parse 回调：将 "section.key" / "key" 键值对写入配置
 * @param ctx     lvPluginConfig 指针
 * @param section 当前节名（全局节为 NULL）
 * @param key     键名（已去除首尾空白）
 * @param value   值（'=' 之后原始内容，与原实现一致保持不去空白）
 * @return true 继续解析
 */
static bool plugin_config_ini_visit(void *ctx, const char *section, const char *key, const char *value) {
    lvPluginConfig *config = (lvPluginConfig *) ctx;

    /* 如果有节名，添加节前缀: "section.key" */
    if (section && section[0] != '\0') {
        lvStrBuf sb = {0};
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        lv_strbuf_printf(&sb, "%s.%s", section, key);
#pragma GCC diagnostic pop
        lv_plugin_config_set(config, sb.data, value, 0);
        lv_strbuf_destroy(&sb);
    } else {
        lv_plugin_config_set(config, key, value, 0);
    }
    return true;
}

/**
 * @brief 从 INI 格式文件加载配置
 * @param config 插件配置指针
 * @param filepath 配置文件路径
 * @return 成功返回 0，失败返回 -1
 */
int lv_plugin_config_load(lvPluginConfig *config, const char *filepath) {
    lv_CHECK_NOT_NULL(config);
    lv_CHECK_NOT_NULL(filepath);

    /* 统一走公共 lv_ini_parse（收敛手写 fopen/fgets 行解析样板） */
    if (lv_ini_parse(filepath, plugin_config_ini_visit, config) != 0)
        lv_RETURN_ERROR(lv_ERROR_IO, "lv_plugin_config_load: 解析失败");

    lv_strlcpy(config->config_file, filepath, sizeof(config->config_file));
    return 0;
}

/**
 * @brief 将配置保存到文件（key=value 格式）
 * @param config 插件配置指针
 * @param filepath 保存路径
 * @return 成功返回 0，失败返回 -1
 */
int lv_plugin_config_save(const lvPluginConfig *config, const char *filepath) {
    lv_CHECK_NOT_NULL(config);
    lv_CHECK_NOT_NULL(filepath);

    FILE *fp = lv_file_open(filepath, "w");
    if (!fp)
        lv_RETURN_ERROR(lv_ERROR_IO, "lv_plugin_config_save: open failed");

    config_registry_ensure();

    /* 遍历注册表条目，仅输出当前 config 的配置项 */
    char prefix[64];
    lv_snprintf(prefix, sizeof(prefix), "C:%p:", (const void *) config);

    int total = lv_registry_count(&g_config_registry);
    for (int i = 0; i < total; i++) {
        const char *entry_name = NULL;
        void *entry_value = NULL;
        if (!lv_registry_get_at(&g_config_registry, i, &entry_name, &entry_value)) {
            continue;
        }
        if (!lv_str_startswith(entry_name, prefix)) {
            continue;
        }
        lvPluginConfigEntry *entry = (lvPluginConfigEntry *) entry_value;
        fprintf(fp, "%s=%s\n", entry->key, entry->value);
    }

    lv_file_close(fp);
    return 0;
}

/**
 * @brief 设置配置项的值（若 key 已存在则覆盖）
 * @param config 插件配置指针
 * @param key 配置键名
 * @param value 配置值
 * @param type 配置值类型标识
 * @return 成功返回 0，失败返回 -1
 */
int lv_plugin_config_set(lvPluginConfig *config, const char *key, const char *value, int type) {
    lv_CHECK_NOT_NULL(config);
    lv_CHECK_NOT_NULL(key);
    lv_CHECK_NOT_NULL(value);

    config_registry_ensure();

    char regkey[192];
    config_build_key(config, key, regkey, sizeof(regkey));

    /* 已存在：覆盖（保持旧语义），否则追加新条目 */
    lvPluginConfigEntry *entry = (lvPluginConfigEntry *) lv_registry_get(&g_config_registry, regkey);
    if (entry) {
        lv_strlcpy(entry->value, value, sizeof(entry->value));
        entry->type = type;
        return 0;
    }

    /* 添加新条目（动态分配，注册表持指针并带析构回调） */
    entry = (lvPluginConfigEntry *) lv_calloc(1, sizeof(lvPluginConfigEntry));
    if (!entry)
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_plugin_config_set: entry calloc failed");
    lv_strlcpy(entry->key, key, sizeof(entry->key));
    lv_strlcpy(entry->value, value, sizeof(entry->value));
    entry->type = type;

    if (!lv_registry_put_ex(&g_config_registry, regkey, entry, config_entry_destroy)) {
        lv_free((void **) &entry);
        lv_RETURN_ERROR(lv_ERROR_RESOURCE_EXHAUSTED, "lv_plugin_config_set: registry exhausted");
    }

    return 0;
}

/**
 * @brief 获取配置项的值，不存在则返回默认值
 * @param config 插件配置指针
 * @param key 配置键名
 * @param default_value 默认值
 * @return 配置值或默认值
 */
const char *lv_plugin_config_get(const lvPluginConfig *config, const char *key, const char *default_value) {
    if (!config)
        return NULL;
    if (!key)
        return default_value;

    config_registry_ensure();

    char regkey[192];
    config_build_key(config, key, regkey, sizeof(regkey));

    lvPluginConfigEntry *entry = (lvPluginConfigEntry *) lv_registry_get(&g_config_registry, regkey);
    if (entry) {
        return entry->value;
    }

    return default_value;
}

/**
 * @brief 将配置应用到指定插件（调用 on_configure 回调）
 * @param plugin 插件指针
 * @param config 配置指针
 * @return 成功返回 0，失败返回 -1
 */
int lv_plugin_apply_config(lvPlugin *plugin, const lvPluginConfig *config) {
    lv_CHECK_NOT_NULL(plugin);
    lv_CHECK_NOT_NULL(config);

    if (plugin->on_configure) {
        return plugin->on_configure(plugin->context, config);
    }

    return 0;
}


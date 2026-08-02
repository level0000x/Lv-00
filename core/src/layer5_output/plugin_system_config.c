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
#include "lv/lv_strbuf.h"
#include "lv/lv_utils.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include "lv/lv_strbuf.h"
#endif

#include "plugin_system_internal.h"

/* ============ 插件配置 ============ */

/**
 * @brief 创建插件配置对象
 * @return 成功返回配置指针，失败返回 NULL
 */
lvPluginConfig *lv_plugin_config_create(void) {
    lvPluginConfig *config = (lvPluginConfig *) lv_calloc(1, sizeof(lvPluginConfig));
    if (!config)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_plugin_config_create: config calloc failed");
    config->entry_capacity = 256;
    config->entries = (lvPluginConfigEntry *) lv_calloc(config->entry_capacity, sizeof(lvPluginConfigEntry));

    if (!config->entries) {
        lv_free((void **) &config);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_plugin_config_create: entries calloc failed");
    }

    return config;
}

/**
 * @brief 销毁插件配置对象，释放资源
 * @param config 插件配置指针
 */
void lv_plugin_config_destroy(lvPluginConfig *config) {
    if (!config)
        return;
    if (config->entries)
        lv_free((void **) &config->entries);
    lv_free((void **) &config);
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

    FILE *fp = fopen(filepath, "r");
    if (!fp)
        lv_RETURN_ERROR(lv_ERROR_IO, "lv_plugin_config_load: fopen failed");

    /* 当前节名称，NULL 表示全局节 */
    char current_section[256] = {0};
    char line[2048];

    while (fgets(line, sizeof(line), fp)) {
        /* 去除行尾换行符 */
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }

        /* 跳过空行 */
        if (len == 0)
            continue;

        /* 跳过注释行（# 或 // 开头） */
        if (line[0] == '#' || (line[0] == '/' && line[1] == '/'))
            continue;

        /* 跳过行首空白后的注释 */
        {
            const char *trimmed = line;
            while (*trimmed == ' ' || *trimmed == '\t')
                trimmed++;
            if (*trimmed == '#' || (*trimmed == '/' && *(trimmed + 1) == '/'))
                continue;
            if (*trimmed == '\0')
                continue; /* 全空白行 */
        }

        /* 检查节标题 [section] */
        if (line[0] == '[') {
            char *end = strchr(line, ']');
            if (end) {
                size_t slen = (size_t) (end - line - 1);
                if (slen < sizeof(current_section)) {
                    memcpy(current_section, line + 1, slen);
                    current_section[slen] = '\0';
                }
            }
            continue;
        }

        /* 解析 key=value */
        char *eq = strchr(line, '=');
        if (eq) {
            *eq = '\0';
            const char *key = line;
            const char *value = eq + 1;

            /* 去除 key 首尾空白 */
            while (*key == ' ' || *key == '\t')
                key++;
            char *key_end = (char *) (key + strlen(key) - 1);
            while (key_end > key && (*key_end == ' ' || *key_end == '\t'))
                *key_end-- = '\0';

            /* 如果有节名，添加节前缀: "section.key" */
            if (current_section[0] != '\0') {
                lvStrBuf sb = {0};
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
                lv_strbuf_printf(&sb, "%s.%s", current_section, key);
#pragma GCC diagnostic pop
                lv_plugin_config_set(config, sb.data, value, 0);
                lv_strbuf_destroy(&sb);
            } else {
                lv_plugin_config_set(config, key, value, 0);
            }
        }
    }

    fclose(fp);
    strncpy(config->config_file, filepath, sizeof(config->config_file) - 1);
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

    FILE *fp = fopen(filepath, "w");
    if (!fp)
        lv_RETURN_ERROR(lv_ERROR_IO, "lv_plugin_config_save: fopen failed");

    for (size_t i = 0; i < config->entry_count; i++) {
        fprintf(fp, "%s=%s\n", config->entries[i].key, config->entries[i].value);
    }

    fclose(fp);
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
    lv_CHECK_ARG(config->entries != NULL, lv_ERROR_INTERNAL, "config entries is NULL");
    if (config->entry_count >= config->entry_capacity)
        lv_RETURN_ERROR(lv_ERROR_RESOURCE_EXHAUSTED, "lv_plugin_config_set: entry_capacity exhausted");

    /* 检查是否已存在 */
    for (size_t i = 0; i < config->entry_count; i++) {
        if (strcmp(config->entries[i].key, key) == 0) {
            strncpy(config->entries[i].value, value, sizeof(config->entries[i].value) - 1);
            config->entries[i].type = type;
            return 0;
        }
    }

    /* 添加新条目 */
    lvPluginConfigEntry *entry = &config->entries[config->entry_count++];
    strncpy(entry->key, key, sizeof(entry->key));
    entry->key[sizeof(entry->key) - 1] = '\0';
    strncpy(entry->value, value, sizeof(entry->value));
    entry->value[sizeof(entry->value) - 1] = '\0';
    entry->type = type;

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
    if (!config || !key)
        return default_value;

    for (size_t i = 0; i < config->entry_count; i++) {
        if (strcmp(config->entries[i].key, key) == 0) {
            return config->entries[i].value;
        }
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


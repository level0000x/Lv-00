/**
 * @file plugin_system_autoload.c
 * @brief LV-00 模块化插件系统 —— 搜索路径管理与自动加载
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
#include "lv/config.h"
#include "lv/lv_path.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include "lv/lv_strbuf.h"
#endif

#include "plugin_system_internal.h"

/* ============ 搜索路径管理 ============ */

/**
 * @brief 添加插件搜索路径
 * @param system 插件系统指针
 * @param path 搜索路径
 * @return 成功返回 0，失败返回 -1
 */
int lv_plugin_system_add_search_path(lvPluginSystem *system, const char *path) {
    if (!system || !path)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "lv_plugin_system_add_search_path: system or path is NULL");

    /* 检查是否已存在 */
    for (int i = 0; i < system->search_paths.count; i++) {
        if (strcmp(*(char **)lv_darray_get(&system->search_paths, i), path) == 0) {
            return 0;
        }
    }

    /* 添加新路径 */
    char *copy = lv_strdup_safe(path);
    if (!copy)
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_plugin_system_add_search_path: strdup failed");

    if (lv_darray_push(&system->search_paths, &copy) < 0) {
        lv_free((void **) &copy);
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_plugin_system_add_search_path: darray_push failed");
    }

    return 0;
}

/**
 * @brief 移除插件搜索路径
 * @param system 插件系统指针
 * @param path 待移除的搜索路径
 * @return 成功返回 0，未找到返回 -1
 */
int lv_plugin_system_remove_search_path(lvPluginSystem *system, const char *path) {
    if (!system || !path)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "lv_plugin_system_remove_search_path: system or path is NULL");

    for (int i = 0; i < system->search_paths.count; i++) {
        if (strcmp(*(char **)lv_darray_get(&system->search_paths, i), path) == 0) {
            lv_free((void **) lv_darray_get(&system->search_paths, i));
            /* 将最后一个元素移到当前位置 */
            char **last = (char **)lv_darray_get(&system->search_paths, system->search_paths.count - 1);
            char **cur = (char **)lv_darray_get(&system->search_paths, i);
            *cur = *last;
            lv_darray_pop(&system->search_paths);
            return 0;
        }
    }

    lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "lv_plugin_system_remove_search_path: path not found");
}

/**
 * @brief 获取所有已注册的搜索路径
 * @param system 插件系统指针
 * @param count 输出参数，路径数量
 * @return 返回搜索路径数组
 */
char **lv_plugin_system_get_search_paths(lvPluginSystem *system, size_t *count) {
    if (!system || !count)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_plugin_system_get_search_paths: system or count is NULL");

    /* 文档契约：调用者需负责释放返回数组，因此返回独立副本，避免暴露内部缓冲区 */
    size_t n = (size_t)system->search_paths.count;
    if (n == 0) {
        *count = 0;
        return NULL;
    }

    char **paths = (char **) lv_malloc(n * sizeof(char *));
    if (!paths)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_plugin_system_get_search_paths: malloc failed");

    for (size_t i = 0; i < n; i++) {
        const char *src = *(char **) lv_darray_get(&system->search_paths, i);
        paths[i] = lv_strdup(src);
        if (!paths[i]) {
            for (size_t j = 0; j < i; j++)
                lv_free((void **) &paths[j]);
            lv_free((void **) &paths);
            lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_plugin_system_get_search_paths: strdup failed");
        }
    }

    *count = n;
    return paths;
}

/* ============ 自动加载 ============ */

/**
 * @brief 从指定目录自动扫描并加载插件
 * @param system 插件系统指针
 * @param directory 待扫描的目录路径
 * @return 成功返回 0，失败返回 -1
 */
int lv_plugin_system_autoload(lvPluginSystem *system, const char *directory) {
    if (!system || !directory)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "lv_plugin_system_autoload: system or directory is NULL");

    /* 添加搜索路径 */
    lv_plugin_system_add_search_path(system, directory);

    /* 扫描目录中的 .dll 文件（Windows）或 .so 文件（Linux） */
#ifdef _WIN32
    lvStrBuf sb_2 = {0};
    char pattern_buf[lv_PATH_BUF_SIZE];
    if (!lv_path_join(directory, "*.dll", pattern_buf, sizeof(pattern_buf))) {
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_plugin_system_autoload: 路径过长");
    }
    lv_strbuf_printf(&sb_2, "%s", pattern_buf);

    WIN32_FIND_DATAA find_data;
    HANDLE hFind = FindFirstFileA(sb_2.data, &find_data);
    if (hFind == INVALID_HANDLE_VALUE) {
        lv_strbuf_destroy(&sb_2);
        return 0; /* 目录为空或不存在，不算错误 */
    }

    do {
        /* 跳过 . 和 .. 目录 */
        if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) {
            continue;
        }

        /* 构造完整路径 */
        lvStrBuf sb_3 = {0};
        char full_path[lv_PATH_BUF_SIZE];
        if (lv_path_join(directory, find_data.cFileName, full_path, sizeof(full_path))) {
            lv_strbuf_printf(&sb_3, "%s", full_path);
        }

        /* 尝试加载为插件 */
        lvPlugin *plugin = lv_plugin_load(system, sb_3.data);
        if (plugin) {
            /* 版本兼容性检查：验证插件版本是否与系统版本兼容 */
            if (plugin->info.version[0] != '\0') {
                if (!lv_plugin_check_api_compatibility(system->version, (lv_PLUGIN_SYSTEM_VERSION_MAJOR << 16) |
                                                                            (lv_PLUGIN_SYSTEM_VERSION_MINOR << 8))) {
                    /* 插件 API 版本不兼容，记录警告并跳过激活 */
                    set_error(system,
                              "Plugin '%s' version '%s' may be incompatible with "
                              "system API version %d.%d.%d. Loading but not activating.",
                              plugin->info.name, plugin->info.version, lv_PLUGIN_SYSTEM_VERSION_MAJOR,
                              lv_PLUGIN_SYSTEM_VERSION_MINOR, lv_PLUGIN_SYSTEM_VERSION_PATCH);
                    /* 不卸载插件，但也不自动激活，让用户决定 */
                }
            }
        }
        lv_strbuf_destroy(&sb_3);

    } while (FindNextFileA(hFind, &find_data));

    FindClose(hFind);
    lv_strbuf_destroy(&sb_2);
#else
    /* Linux/macOS: 使用 opendir/readdir 扫描 .so 文件 */
    DIR *dir = opendir(directory);
    if (!dir)
        return 0;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        /* 跳过 . 和 .. */
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        /* 检查是否为 .so 文件 */
        size_t name_len = strlen(entry->d_name);
        if (name_len > 3 && strcmp(entry->d_name + name_len - 3, ".so") == 0) {
            lvStrBuf sb_4 = {0};
            char full_path[lv_PATH_BUF_SIZE];
            if (lv_path_join(directory, entry->d_name, full_path, sizeof(full_path))) {
                lv_strbuf_printf(&sb_4, "%s", full_path);
            }

            /* 尝试加载为插件 */
            lvPlugin *plugin = lv_plugin_load(system, sb_4.data);
            if (plugin) {
                /* 版本兼容性检查：验证插件版本是否与系统版本兼容 */
                if (plugin->info.version[0] != '\0') {
                    if (!lv_plugin_check_api_compatibility(
                            system->version,
                            (lv_PLUGIN_SYSTEM_VERSION_MAJOR << 16) | (lv_PLUGIN_SYSTEM_VERSION_MINOR << 8))) {
                        set_error(system,
                                  "Plugin '%s' version '%s' may be incompatible with "
                                  "system API version %d.%d.%d. Loading but not activating.",
                                  plugin->info.name, plugin->info.version, lv_PLUGIN_SYSTEM_VERSION_MAJOR,
                                  lv_PLUGIN_SYSTEM_VERSION_MINOR, lv_PLUGIN_SYSTEM_VERSION_PATCH);
                    }
                }
            }
            lv_strbuf_destroy(&sb_4);
        }
    }

    closedir(dir);
#endif

    return 0;
}

/**
 * @brief 自动加载所有搜索路径下的插件
 * @param system 插件系统指针
 * @return 成功返回 0，失败返回 -1
 */
int lv_plugin_system_autoload_all(lvPluginSystem *system) {
    lv_CHECK_NOT_NULL(system);

    /* 遍历所有搜索路径，自动加载其中的插件（含版本兼容性检查） */
    for (int i = 0; i < system->search_paths.count; i++) {
        lv_plugin_system_autoload(system, *(char **)lv_darray_get(&system->search_paths, i));
    }

    return 0;
}


/**
 * @file lv_backend_plugin.c
 * @brief 统一后端插件系统抽象层实现 —— 线程安全的动态插件注册表
 *
 * @details 实现 lv_backend_plugin.h 中声明的所有 API。
 *          使用动态数组存储插件指针，支持线程安全的注册/注销/查找操作，
 *          以及按优先级批量初始化/清理。
 *
 * @author Lv-00 Project
 * @version 1.0.0
 * @date 2026-08-04
 */

#include "lv/lv_backend_plugin.h"

#include <stdlib.h>   /* qsort */
#include <string.h>

#include "lv/lv_utils.h"   /* lv_malloc, lv_calloc, lv_realloc, lv_free */

/* ============================================================
 * 内部常量
 * ============================================================ */

/** @brief 默认初始容量 */
#define lv_PLUGIN_REGISTRY_DEFAULT_CAPACITY 16

/** @brief 扩容因子 */
#define lv_PLUGIN_REGISTRY_GROW_FACTOR 2

/* ============================================================
 * 全局单例状态
 * ============================================================ */

/** @brief 全局注册表单例状态 */
typedef struct {
    lvBackendPluginRegistry registry; /**< 全局注册表 */
    bool inited;                      /**< 是否已初始化 */
} lvGlobalPluginRegistryState;

/** @brief 全局注册表单例 */
static lvGlobalPluginRegistryState s_global_plugin_registry = {0};

/* ============================================================
 * 注册表生命周期 API
 * ============================================================ */

void lv_backend_plugin_registry_init(lvBackendPluginRegistry *reg) {
    if (!reg) return;

    reg->plugins = (lvBackendPlugin **) lv_calloc(
        (size_t) lv_PLUGIN_REGISTRY_DEFAULT_CAPACITY, sizeof(lvBackendPlugin *));
    reg->count = 0;
    reg->capacity = reg->plugins ? lv_PLUGIN_REGISTRY_DEFAULT_CAPACITY : 0;
    lv_MUTEX_INIT(&reg->mutex);
}

void lv_backend_plugin_registry_cleanup(lvBackendPluginRegistry *reg) {
    if (!reg) return;

    lv_MUTEX_LOCK(&reg->mutex);
    if (reg->plugins) {
        lv_free((void **) &reg->plugins);
    }
    reg->plugins = NULL;
    reg->count = 0;
    reg->capacity = 0;
    lv_MUTEX_UNLOCK(&reg->mutex);
    lv_MUTEX_DESTROY(&reg->mutex);
}

/* ============================================================
 * 插件注册与注销 API
 * ============================================================ */

bool lv_backend_plugin_register(lvBackendPluginRegistry *reg, lvBackendPlugin *plugin) {
    if (!reg || !plugin || !plugin->name) return false;

    lv_MUTEX_LOCK(&reg->mutex);

    /* 检查名称是否已存在 */
    for (int i = 0; i < reg->count; i++) {
        if (reg->plugins[i] && strcmp(reg->plugins[i]->name, plugin->name) == 0) {
            lv_MUTEX_UNLOCK(&reg->mutex);
            return false;
        }
    }

    /* 检查容量，必要时 2x 扩容 */
    if (reg->count >= reg->capacity) {
        int new_cap = reg->capacity > 0
            ? reg->capacity * lv_PLUGIN_REGISTRY_GROW_FACTOR
            : lv_PLUGIN_REGISTRY_DEFAULT_CAPACITY;
        lvBackendPlugin **new_plugins = (lvBackendPlugin **)
            lv_realloc(reg->plugins, (size_t) new_cap * sizeof(lvBackendPlugin *));
        if (!new_plugins) {
            lv_MUTEX_UNLOCK(&reg->mutex);
            return false;
        }
        reg->plugins = new_plugins;
        reg->capacity = new_cap;
    }

    /* 添加新插件 */
    reg->plugins[reg->count] = plugin;
    reg->count++;

    lv_MUTEX_UNLOCK(&reg->mutex);
    return true;
}

bool lv_backend_plugin_unregister(lvBackendPluginRegistry *reg, const char *name) {
    if (!reg || !name) return false;

    lv_MUTEX_LOCK(&reg->mutex);

    for (int i = 0; i < reg->count; i++) {
        if (reg->plugins[i] && strcmp(reg->plugins[i]->name, name) == 0) {
            /* 将最后一个元素移到当前位置，覆盖要删除的项 */
            reg->count--;
            if (i < reg->count) {
                reg->plugins[i] = reg->plugins[reg->count];
            }
            reg->plugins[reg->count] = NULL;
            lv_MUTEX_UNLOCK(&reg->mutex);
            return true;
        }
    }

    lv_MUTEX_UNLOCK(&reg->mutex);
    return false;
}

/* ============================================================
 * 插件查找与遍历 API
 * ============================================================ */

lvBackendPlugin *lv_backend_plugin_find(lvBackendPluginRegistry *reg, const char *name) {
    if (!reg || !name) return NULL;

    lv_MUTEX_LOCK(&reg->mutex);

    for (int i = 0; i < reg->count; i++) {
        if (reg->plugins[i] && strcmp(reg->plugins[i]->name, name) == 0) {
            lvBackendPlugin *result = reg->plugins[i];
            lv_MUTEX_UNLOCK(&reg->mutex);
            return result;
        }
    }

    lv_MUTEX_UNLOCK(&reg->mutex);
    return NULL;
}

int lv_backend_plugin_find_by_type(lvBackendPluginRegistry *reg, lvBackendPluginType type,
                                   lvBackendPlugin **out, int max_count) {
    if (!reg || !out || max_count <= 0) return 0;

    lv_MUTEX_LOCK(&reg->mutex);

    int found = 0;
    for (int i = 0; i < reg->count && found < max_count; i++) {
        if (reg->plugins[i] && reg->plugins[i]->type == type) {
            out[found] = reg->plugins[i];
            found++;
        }
    }

    lv_MUTEX_UNLOCK(&reg->mutex);
    return found;
}

int lv_backend_plugin_count(lvBackendPluginRegistry *reg) {
    if (!reg) return 0;

    lv_MUTEX_LOCK(&reg->mutex);
    int count = reg->count;
    lv_MUTEX_UNLOCK(&reg->mutex);

    return count;
}

/* ============================================================
 * 批量生命周期管理 API
 * ============================================================ */

/** @brief 按优先级排序的比较函数（升序：低数字优先） */
static int plugin_priority_compare(const void *a, const void *b) {
    const lvBackendPlugin *pa = *(const lvBackendPlugin *const *) a;
    const lvBackendPlugin *pb = *(const lvBackendPlugin *const *) b;
    if (pa->priority < pb->priority) return -1;
    if (pa->priority > pb->priority) return 1;
    return 0;
}

void lv_backend_plugin_init_all(lvBackendPluginRegistry *reg) {
    if (!reg) return;

    lv_MUTEX_LOCK(&reg->mutex);

    /* 按优先级排序 */
    qsort(reg->plugins, (size_t) reg->count, sizeof(lvBackendPlugin *), plugin_priority_compare);

    /* 按顺序初始化 */
    for (int i = 0; i < reg->count; i++) {
        if (reg->plugins[i] && reg->plugins[i]->init) {
            reg->plugins[i]->init();
        }
    }

    lv_MUTEX_UNLOCK(&reg->mutex);
}

void lv_backend_plugin_cleanup_all(lvBackendPluginRegistry *reg) {
    if (!reg) return;

    lv_MUTEX_LOCK(&reg->mutex);

    /* 按反向优先级清理（高优先级先清理） */
    for (int i = reg->count - 1; i >= 0; i--) {
        if (reg->plugins[i] && reg->plugins[i]->cleanup) {
            reg->plugins[i]->cleanup();
        }
    }

    lv_MUTEX_UNLOCK(&reg->mutex);
}

/* ============================================================
 * 全局单例注册表
 * ============================================================ */

lvBackendPluginRegistry *lv_backend_plugin_registry_global(void) {
    if (!s_global_plugin_registry.inited) {
        lv_backend_plugin_registry_init(&s_global_plugin_registry.registry);
        s_global_plugin_registry.inited = true;
    }
    return &s_global_plugin_registry.registry;
}
/**
 * @file lv_registry.c
 * @brief 通用注册表实现 —— 线程安全的名称→工厂函数映射
 *
 * @details 提供统一的注册表模式，消除 ATP、SMT、Groebner 等后端中
 *          重复的注册表实现代码。支持动态扩容、互斥锁保护、名称唯一性检查。
 *
 * @author Lv-00 Project
 * @version 1.0.0
 * @date 2026-07-31
 */

#include "lv/lv_registry.h"

#include <stdlib.h>   /* qsort */
#include <string.h>

#include "lv/lv_utils.h"   /* lv_malloc, lv_calloc, lv_realloc, lv_free */

/* ============================================================
 * 内部常量
 * ============================================================ */

/** @brief 默认初始容量 */
#define lv_REGISTRY_DEFAULT_CAPACITY 16

/** @brief 扩容因子 */
#define lv_REGISTRY_GROW_FACTOR 2

/* ============================================================
 * 公共 API 实现
 * ============================================================ */

void lv_registry_init(lvRegistry *reg, int capacity) {
    if (!reg) return;

    if (capacity <= 0) {
        capacity = lv_REGISTRY_DEFAULT_CAPACITY;
    }

    reg->entries = (lvRegistryEntry *) lv_calloc((size_t) capacity, sizeof(lvRegistryEntry));
    reg->count = 0;
    reg->capacity = reg->entries ? capacity : 0;
    lv_MUTEX_INIT(&reg->mutex);
}

void lv_registry_destroy(lvRegistry *reg) {
    if (!reg) return;

    lv_MUTEX_LOCK(&reg->mutex);
    if (reg->entries) {
        lv_free((void **) &reg->entries);
    }
    reg->entries = NULL;
    reg->count = 0;
    reg->capacity = 0;
    lv_MUTEX_UNLOCK(&reg->mutex);
    lv_MUTEX_DESTROY(&reg->mutex);
}

bool lv_registry_register(lvRegistry *reg, const char *name, void *(*create)(void)) {
    if (!reg || !name) return false;

    lv_MUTEX_LOCK(&reg->mutex);

    /* 检查名称是否已存在 */
    for (int i = 0; i < reg->count; i++) {
        if (strcmp(reg->entries[i].name, name) == 0) {
            lv_MUTEX_UNLOCK(&reg->mutex);
            return false;
        }
    }

    /* 检查容量，必要时 2x 扩容 */
    if (reg->count >= reg->capacity) {
        int new_cap = reg->capacity > 0 ? reg->capacity * lv_REGISTRY_GROW_FACTOR
                                        : lv_REGISTRY_DEFAULT_CAPACITY;
        lvRegistryEntry *new_entries = (lvRegistryEntry *)
            lv_realloc(reg->entries, (size_t) new_cap * sizeof(lvRegistryEntry));
        if (!new_entries) {
            lv_MUTEX_UNLOCK(&reg->mutex);
            return false;
        }
        reg->entries = new_entries;
        reg->capacity = new_cap;
    }

    /* 添加新条目 */
    reg->entries[reg->count].name = name;
    reg->entries[reg->count].create = create;
    reg->count++;

    lv_MUTEX_UNLOCK(&reg->mutex);
    return true;
}

void *lv_registry_create(const lvRegistry *reg, const char *name) {
    if (!reg || !name) return NULL;

    /* 读取操作需要加锁（const cast 因为 lvMutex 操作需要非 const） */
    lvRegistry *r = (lvRegistry *) reg;
    lv_MUTEX_LOCK(&r->mutex);

    for (int i = 0; i < reg->count; i++) {
        if (strcmp(reg->entries[i].name, name) == 0) {
            void *result = reg->entries[i].create ? reg->entries[i].create() : NULL;
            lv_MUTEX_UNLOCK(&r->mutex);
            return result;
        }
    }

    lv_MUTEX_UNLOCK(&r->mutex);
    return NULL;
}

int lv_registry_find(const lvRegistry *reg, const char *name) {
    if (!reg || !name) return -1;

    for (int i = 0; i < reg->count; i++) {
        if (strcmp(reg->entries[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

/* ============================================================
 * 模块生命周期管理实现
 * ============================================================ */

/** @brief 最大可注册模块数 */
#define lv_MAX_MODULES 64

/** @brief 模块注册表内部条目 */
typedef struct {
    const char *name;
    lvModuleInitFunc init;
    lvModuleCleanupFunc cleanup;
    lvModulePriority priority;
} ModuleEntry;

/** @brief 模块注册表单例状态 */
typedef struct {
    ModuleEntry entries[lv_MAX_MODULES]; /**< 模块注册表 */
    int count;                           /**< 已注册模块数量 */
} ModuleRegistryState;

/** @brief 模块注册表全局单例 */
static ModuleRegistryState s_module_registry = {0};

bool lv_module_register(const char *name, lvModuleInitFunc init_fn,
                         lvModuleCleanupFunc cleanup_fn, lvModulePriority priority) {
    if (!name || s_module_registry.count >= lv_MAX_MODULES) {
        return false;
    }

    /* 检查名称是否已存在 */
    for (int i = 0; i < s_module_registry.count; i++) {
        if (strcmp(s_module_registry.entries[i].name, name) == 0) {
            return false; /* 不允许重复注册 */
        }
    }

    s_module_registry.entries[s_module_registry.count].name = name;
    s_module_registry.entries[s_module_registry.count].init = init_fn;
    s_module_registry.entries[s_module_registry.count].cleanup = cleanup_fn;
    s_module_registry.entries[s_module_registry.count].priority = priority;
    s_module_registry.count++;

    return true;
}

/** @brief 按优先级排序的比较函数（升序：低数字优先） */
static int module_compare(const void *a, const void *b) {
    const ModuleEntry *ma = (const ModuleEntry *) a;
    const ModuleEntry *mb = (const ModuleEntry *) b;
    if (ma->priority < mb->priority) return -1;
    if (ma->priority > mb->priority) return 1;
    return 0;
}

bool lv_module_init_all(void) {
    /* 按优先级排序 */
    qsort(s_module_registry.entries, (size_t) s_module_registry.count, sizeof(ModuleEntry), module_compare);

    /* 按顺序初始化 */
    for (int i = 0; i < s_module_registry.count; i++) {
        if (s_module_registry.entries[i].init) {
            if (!s_module_registry.entries[i].init()) {
                return false; /* 初始化失败即停止 */
            }
        }
    }

    return true;
}

void lv_module_cleanup_all(void) {
    /* 按反向优先级清理（高优先级先清理，核心最后清理） */
    for (int i = s_module_registry.count - 1; i >= 0; i--) {
        if (s_module_registry.entries[i].cleanup) {
            s_module_registry.entries[i].cleanup();
        }
    }
}

int lv_module_count(void) {
    return s_module_registry.count;
}

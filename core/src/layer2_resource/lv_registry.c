/**
 * @file lv_registry.c
 * @brief 通用注册表实现 —— 线程安全的名称→工厂函数 / 泛型值映射
 *
 * @details 提供统一的注册表模式，消除 ATP、SMT、Groebner 后端以及
 *          插件接口、插件配置、几何事件等模块中重复的注册表实现代码。
 *          支持动态扩容、互斥锁保护、名称唯一性检查、条目删除与析构回调。
 *
 * @author Lv-00 Project
 * @version 1.0.0
 * @date 2026-07-31
 */

#include "lv/lv_registry.h"

#include <stdlib.h>   /* qsort */
#include <string.h>

#include "lv/lv_utils.h"   /* lv_malloc, lv_calloc, lv_realloc, lv_free */
#include "lv/lv_thread.h"  /* lv_once_t, lv_once */

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
        for (int i = 0; i < reg->count; i++) {
            if (reg->entries[i].destroy && reg->entries[i].value) {
                reg->entries[i].destroy(reg->entries[i].value);
            }
            lv_free((void **) &reg->entries[i].name);
        }
        lv_free((void **) &reg->entries);
    }
    reg->entries = NULL;
    reg->count = 0;
    reg->capacity = 0;
    lv_MUTEX_UNLOCK(&reg->mutex);
    lv_MUTEX_DESTROY(&reg->mutex);
}

/* ============================================================
 * 内部辅助
 * ============================================================ */

/**
 * @brief 插入条目（调用方须持有互斥锁）
 *
 * 支持两种形态：name→factory（create 非 NULL）与 name→value（value 非 NULL）。
 * name 在此处内部拷贝，由注册表统一管理生命周期（remove/destroy 时释放）。
 */
static bool registry_insert_locked(lvRegistry *reg, const char *name, void *(*create)(void), void *value,
                                   void (*destroy)(void *)) {
    /* 检查名称是否已存在 */
    for (int i = 0; i < reg->count; i++) {
        if (strcmp(reg->entries[i].name, name) == 0) {
            return false;
        }
    }

    /* 检查容量，必要时扩容（统一委托 lv_ensure_capacity，内部含溢出检查与倍增） */
    if (!lv_ensure_capacity((void **) &reg->entries, reg->count, &reg->capacity, sizeof(lvRegistryEntry), 0)) {
        return false;
    }

    /* 拷贝 name（注册表持有，remove/destroy 时释放） */
    char *name_copy = lv_strdup(name);
    if (!name_copy) {
        return false;
    }

    /* 添加新条目 */
    reg->entries[reg->count].name = name_copy;
    reg->entries[reg->count].create = create;
    reg->entries[reg->count].value = value;
    reg->entries[reg->count].destroy = destroy;
    reg->count++;

    return true;
}

bool lv_registry_register(lvRegistry *reg, const char *name, void *(*create)(void)) {
    if (!reg || !name) return false;

    lv_MUTEX_LOCK(&reg->mutex);
    bool ok = registry_insert_locked(reg, name, create, NULL, NULL);
    lv_MUTEX_UNLOCK(&reg->mutex);
    return ok;
}

bool lv_registry_put(lvRegistry *reg, const char *name, void *value) {
    return lv_registry_put_ex(reg, name, value, NULL);
}

bool lv_registry_put_ex(lvRegistry *reg, const char *name, void *value, void (*destroy)(void *)) {
    if (!reg || !name) return false;

    lv_MUTEX_LOCK(&reg->mutex);
    bool ok = registry_insert_locked(reg, name, NULL, value, destroy);
    lv_MUTEX_UNLOCK(&reg->mutex);
    return ok;
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

void *lv_registry_get(const lvRegistry *reg, const char *name) {
    if (!reg || !name) return NULL;

    /* 读取操作需要加锁（const cast 因为 lvMutex 操作需要非 const） */
    lvRegistry *r = (lvRegistry *) reg;
    lv_MUTEX_LOCK(&r->mutex);

    void *result = NULL;
    for (int i = 0; i < reg->count; i++) {
        if (strcmp(reg->entries[i].name, name) == 0) {
            result = reg->entries[i].value;
            break;
        }
    }

    lv_MUTEX_UNLOCK(&r->mutex);
    return result;
}

bool lv_registry_remove(lvRegistry *reg, const char *name) {
    if (!reg || !name) return false;

    lv_MUTEX_LOCK(&reg->mutex);

    for (int i = 0; i < reg->count; i++) {
        if (strcmp(reg->entries[i].name, name) == 0) {
            /* 先调用析构回调并释放 name，再前移紧凑（lv_shift_left 先例） */
            if (reg->entries[i].destroy && reg->entries[i].value) {
                reg->entries[i].destroy(reg->entries[i].value);
            }
            lv_free((void **) &reg->entries[i].name);
            lv_shift_left(reg->entries, sizeof(lvRegistryEntry), (size_t) i, (size_t) reg->count);
            reg->count--;
            lv_MUTEX_UNLOCK(&reg->mutex);
            return true;
        }
    }

    lv_MUTEX_UNLOCK(&reg->mutex);
    return false;
}

int lv_registry_count(const lvRegistry *reg) {
    if (!reg) return 0;

    lvRegistry *r = (lvRegistry *) reg;
    lv_MUTEX_LOCK(&r->mutex);
    int count = reg->count;
    lv_MUTEX_UNLOCK(&r->mutex);
    return count;
}

bool lv_registry_get_at(const lvRegistry *reg, int index, const char **name, void **value) {
    if (!reg || index < 0) return false;

    lvRegistry *r = (lvRegistry *) reg;
    lv_MUTEX_LOCK(&r->mutex);
    if (index >= reg->count) {
        lv_MUTEX_UNLOCK(&r->mutex);
        return false;
    }
    if (name) *name = reg->entries[index].name;
    if (value) *value = reg->entries[index].value;
    lv_MUTEX_UNLOCK(&r->mutex);
    return true;
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
    lvMutex mutex;                       /**< 注册表保护互斥锁 */
} ModuleRegistryState;

/** @brief 模块注册表全局单例 */
static ModuleRegistryState s_module_registry = {0};

/** @brief 模块注册表互斥锁一次性初始化守卫（lv_once 保证线程安全） */
static lv_once_t s_module_registry_once = lv_ONCE_INIT;

/** @brief 模块注册表互斥锁初始化回调（仅由 lv_once 调用一次） */
static void module_registry_mutex_init(void) {
    lv_MUTEX_INIT(&s_module_registry.mutex);
}

static inline void module_registry_lock(void) {
    lv_once(&s_module_registry_once, module_registry_mutex_init);
    lv_MUTEX_LOCK(&s_module_registry.mutex);
}

static inline void module_registry_unlock(void) {
    lv_MUTEX_UNLOCK(&s_module_registry.mutex);
}

bool lv_module_register(const char *name, lvModuleInitFunc init_fn,
                         lvModuleCleanupFunc cleanup_fn, lvModulePriority priority) {
    if (!name) {
        return false;
    }

    module_registry_lock();

    if (s_module_registry.count >= lv_MAX_MODULES) {
        module_registry_unlock();
        return false;
    }

    /* 检查名称是否已存在 */
    for (int i = 0; i < s_module_registry.count; i++) {
        if (strcmp(s_module_registry.entries[i].name, name) == 0) {
            module_registry_unlock();
            return false; /* 不允许重复注册 */
        }
    }

    s_module_registry.entries[s_module_registry.count].name = name;
    s_module_registry.entries[s_module_registry.count].init = init_fn;
    s_module_registry.entries[s_module_registry.count].cleanup = cleanup_fn;
    s_module_registry.entries[s_module_registry.count].priority = priority;
    s_module_registry.count++;

    module_registry_unlock();
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
    module_registry_lock();

    /* 按优先级排序 */
    qsort(s_module_registry.entries, (size_t) s_module_registry.count, sizeof(ModuleEntry), module_compare);

    /* 按顺序初始化 */
    for (int i = 0; i < s_module_registry.count; i++) {
        if (s_module_registry.entries[i].init) {
            if (!s_module_registry.entries[i].init()) {
                module_registry_unlock();
                return false; /* 初始化失败即停止 */
            }
        }
    }

    module_registry_unlock();
    return true;
}

void lv_module_cleanup_all(void) {
    module_registry_lock();

    /* 按反向优先级清理（高优先级先清理，核心最后清理） */
    for (int i = s_module_registry.count - 1; i >= 0; i--) {
        if (s_module_registry.entries[i].cleanup) {
            s_module_registry.entries[i].cleanup();
        }
    }

    module_registry_unlock();
}

int lv_module_count(void) {
    module_registry_lock();
    int count = s_module_registry.count;
    module_registry_unlock();
    return count;
}

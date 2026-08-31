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

#include "lv/lv_hashtable.h" /* lv_hashtable_str_* 哈希副索引 */
#include "lv/lv_str_utils.h"
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
    /* name→下标+1 哈希副索引；创建失败（内存不足）时置 NULL，读写路径回退线性扫描 */
    reg->index = lv_hashtable_str_create(capacity);
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
    if (reg->index) {
        lv_hashtable_str_destroy(reg->index);
        reg->index = NULL;
    }
    reg->entries = NULL;
    reg->count = 0;
    reg->capacity = 0;
    lv_MUTEX_UNLOCK(&reg->mutex);
    lv_MUTEX_DESTROY(&reg->mutex);
}

void lv_registry_clear(lvRegistry *reg) {
    if (!reg) return;

    lv_MUTEX_LOCK(&reg->mutex);
    if (reg->entries) {
        for (int i = 0; i < reg->count; i++) {
            if (reg->entries[i].destroy && reg->entries[i].value) {
                reg->entries[i].destroy(reg->entries[i].value);
            }
            lv_free((void **) &reg->entries[i].name);
        }
    }
    reg->count = 0;
    /* 清空后索引一并销毁，后续插入时惰性重建 */
    if (reg->index) {
        lv_hashtable_str_destroy(reg->index);
        reg->index = NULL;
    }
    lv_MUTEX_UNLOCK(&reg->mutex);
}

/* ============================================================
 * 内部辅助
 * ============================================================ */

/**
 * @brief 在锁内按名称定位条目下标（哈希 O(1)，索引缺失时回退线性扫描）
 *
 * @return 条目下标（0..count-1），未找到返回 -1
 */
static int registry_find_index_locked(const lvRegistry *reg, const char *name) {
    if (reg->index) {
        void *v = lv_hashtable_str_get(reg->index, name);
        if (v) {
            return (int) (intptr_t) v - 1; /* 值存下标+1（下标 0 对应 NULL 值，不可直接存储） */
        }
        return -1;
    }
    for (int i = 0; i < reg->count; i++) {
        if (lv_str_eq(reg->entries[i].name, name)) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief 确保 name→下标+1 哈希索引存在且完整（调用方须持有互斥锁）
 *
 * 索引可能因创建失败或 clear/remove 后重建失败而为 NULL；
 * 此函数尝试（重新）创建并填充全部现有条目。
 * @return true 索引可用，false 创建/填充失败（index 保持 NULL，调用方回退线性扫描）
 */
static bool registry_ensure_index_locked(lvRegistry *reg) {
    if (reg->index) {
        return true;
    }
    reg->index = lv_hashtable_str_create(reg->capacity > 0 ? reg->capacity : lv_REGISTRY_DEFAULT_CAPACITY);
    if (!reg->index) {
        return false;
    }
    for (int i = 0; i < reg->count; i++) {
        if (!lv_hashtable_str_insert(reg->index, reg->entries[i].name, (void *) (intptr_t) (i + 1))) {
            lv_hashtable_str_destroy(reg->index);
            reg->index = NULL;
            return false;
        }
    }
    return true;
}

/**
 * @brief 插入条目（调用方须持有互斥锁）
 *
 * 支持两种形态：name→factory（create 非 NULL）与 name→value（value 非 NULL）。
 * name 在此处内部拷贝，由注册表统一管理生命周期（remove/destroy 时释放）。
 * 同步维护 name→下标+1 哈希副索引；索引不可用（创建失败）时跳过同步，
 * 读路径回退线性扫描，语义与纯线性实现完全一致。
 */
static bool registry_insert_locked(lvRegistry *reg, const char *name, void *(*create)(void), void *value,
                                   void (*destroy)(void *)) {
    /* 检查名称是否已存在（哈希 O(1)，索引缺失回退线性扫描） */
    if (registry_find_index_locked(reg, name) >= 0) {
        return false;
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

    /* 同步哈希索引（新条目将位于下标 reg->count，值存下标+1）：
     * 先索引后条目，索引失败时回滚（仅释放 name_copy，不产生半写入状态） */
    if (reg->index || registry_ensure_index_locked(reg)) {
        if (!lv_hashtable_str_insert(reg->index, name_copy, (void *) (intptr_t) (reg->count + 1))) {
            lv_free((void **) &name_copy);
            return false;
        }
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

    int i = registry_find_index_locked(reg, name);
    void *result = NULL;
    if (i >= 0) {
        result = reg->entries[i].create ? reg->entries[i].create() : NULL;
    }

    lv_MUTEX_UNLOCK(&r->mutex);
    return result;
}

int lv_registry_find(const lvRegistry *reg, const char *name) {
    if (!reg || !name) return -1;

    /* 原实现为无锁线性扫描；哈希索引为只读操作，保持无锁语义一致 */
    return registry_find_index_locked(reg, name);
}

void *lv_registry_get(const lvRegistry *reg, const char *name) {
    if (!reg || !name) return NULL;

    /* 读取操作需要加锁（const cast 因为 lvMutex 操作需要非 const） */
    lvRegistry *r = (lvRegistry *) reg;
    lv_MUTEX_LOCK(&r->mutex);

    void *result = NULL;
    int i = registry_find_index_locked(reg, name);
    if (i >= 0) {
        result = reg->entries[i].value;
    }

    lv_MUTEX_UNLOCK(&r->mutex);
    return result;
}

bool lv_registry_remove(lvRegistry *reg, const char *name) {
    if (!reg || !name) return false;

    lv_MUTEX_LOCK(&reg->mutex);

    int i = registry_find_index_locked(reg, name);
    if (i < 0) {
        lv_MUTEX_UNLOCK(&reg->mutex);
        return false;
    }

    /* 先调用析构回调并释放 name，再前移紧凑（lv_shift_left 先例） */
    if (reg->entries[i].destroy && reg->entries[i].value) {
        reg->entries[i].destroy(reg->entries[i].value);
    }
    lv_free((void **) &reg->entries[i].name);
    lv_shift_left(reg->entries, sizeof(lvRegistryEntry), (size_t) i, (size_t) reg->count);
    reg->count--;

    /* 前移紧凑使被删条目之后的所有下标 -1，索引整体失效：重建（remove 低频，简单可靠） */
    if (reg->index) {
        lv_hashtable_str_destroy(reg->index);
        reg->index = NULL;
        registry_ensure_index_locked(reg); /* 失败保持 NULL，读路径回退线性扫描 */
    }

    lv_MUTEX_UNLOCK(&reg->mutex);
    return true;
}

int lv_registry_count(const lvRegistry *reg) {
    if (!reg) return 0;

    lvRegistry *r = (lvRegistry *) reg;
    lv_MUTEX_LOCK(&r->mutex);
    int count = reg->count;
    lv_MUTEX_UNLOCK(&r->mutex);
    return count;
}

int lv_registry_remove_prefix(lvRegistry *reg, const char *prefix) {
    if (!reg || !prefix) return 0;

    lv_MUTEX_LOCK(&reg->mutex);
    size_t prefix_len = strlen(prefix);
    int removed = 0;
    int i = reg->count - 1;
    while (i >= 0) {
        const char *entry_name = reg->entries[i].name;
        if (entry_name && strncmp(entry_name, prefix, prefix_len) == 0) {
            if (reg->entries[i].destroy && reg->entries[i].value) {
                reg->entries[i].destroy(reg->entries[i].value);
            }
            lv_free((void **) &reg->entries[i].name);
            lv_shift_left(reg->entries, sizeof(lvRegistryEntry), (size_t) i, (size_t) reg->count);
            reg->count--;
            removed++;
        } else {
            i--;
        }
    }
    if (removed > 0 && reg->index) {
        lv_hashtable_str_destroy(reg->index);
        reg->index = NULL;
        registry_ensure_index_locked(reg); /* 失败保持 NULL，读路径回退线性扫描 */
    }
    lv_MUTEX_UNLOCK(&reg->mutex);
    return removed;
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

    /* 检查名称是否已存在 —— J1/F28：改为幂等 upsert（更新条目而非拒绝，
     * 修复"重复注册被吞"：同一模块在 init/cleanup 循环中重复注册时，
     * 旧实现 return false 丢弃新注册且不更新） */
    for (int i = 0; i < s_module_registry.count; i++) {
        if (lv_str_eq(s_module_registry.entries[i].name, name)) {
            s_module_registry.entries[i].init = init_fn;
            s_module_registry.entries[i].cleanup = cleanup_fn;
            s_module_registry.entries[i].priority = priority;
            module_registry_unlock();
            return true;
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
    return lv_cmp_int_asc(ma->priority, mb->priority);
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

void lv_module_registry_reset(void) {
    module_registry_lock();
    /* 清空注册表（entries 为值类型，无需逐条释放；name 为静态字面量） */
    s_module_registry.count = 0;
    module_registry_unlock();
    /* 重置互斥锁 once 守卫：allow init/cleanup 循环后 mutex 可重建
     * （POSIX/Windows 下清零后 lv_once 可再次执行） */
    lv_once_reset(&s_module_registry_once);
}

int lv_module_count(void) {
    module_registry_lock();
    int count = s_module_registry.count;
    module_registry_unlock();
    return count;
}

/**
 * @file preset_manager.c
 * @brief 预设函数块管理器 - 核心实现
 *
 * @details 实现预设函数块系统的核心管理功能，包括：
 *          - 预设库的初始化和关闭
 *          - 预设的注册、查询、实例化
 *          - 线程安全的预设操作
 *          - 内存管理和错误处理
 *
 * @version 4.0.0
 * @author Lv-00 Project
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error_codes.h"
#include "func_block_preset.h"
#include "func_block_registry.h"
#include "lv_internal.h"
#include "lv/lv_json.h"
#include "lv/lv_thread.h"
#include "lv/lv_lifecycle.h" /* lv_obj_destroy_fields / lv_FIELD_* */
#include "lv_utils.h"
#include "preset_blocks.h"
#include "preset_common.h"
#include "preset_manager_internal.h"
#include "preset_core.h"

#ifdef _WIN32
/* 仅 Windows DLL 入口点（DllMain）需要；非 Windows 平台不引入 */
#include <windows.h>
#endif

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wjump-misses-init"
#endif

PresetLibraryState g_library = {.hash_table = NULL,
                                       .entry_count = 0,
                                       .builtin_count = 0,
                                       .custom_count = 0,
                                       .initialized = false,
                                       .next_id = PRESET_ID_OFFSET,
                                       .last_error = {0},
                                       .error_callback = NULL,
                                       .error_callback_data = NULL};

/* ============================================================
 * 内部辅助函数声明
 * ============================================================ */

uint32_t hash_string(const char *str);
InternalPresetEntry *find_entry(const char *name);
bool insert_entry(InternalPresetEntry *entry);
bool remove_entry(const char *name);
void free_entry(InternalPresetEntry *entry);
void lock_library(void);
void unlock_library(void);
void set_error(const char *fmt, ...);
void clear_error(void);

/* ============================================================
 * 哈希函数实现
 * ============================================================ */

/**
 * @brief 字符串哈希函数（FNV-1a，复用统一实现 lv_fnv1a_hash_str）
 */
uint32_t hash_string(const char *str) {
    if (str == NULL) {
        return 0;
    }
    return (uint32_t) lv_fnv1a_hash_str(str);
}

/* ============================================================
 * 线程安全实现
 * ============================================================ */

/** @brief 预设库互斥锁一次性初始化控件（模块级，供 lock_library 与 DllMain 共用） */
static lv_once_t s_library_mutex_once = lv_ONCE_INIT;

/** @brief 预设库互斥锁一次性初始化回调（lv_once 保证仅执行一次且同步完成） */
static void library_mutex_init_impl(void) {
    lv_MUTEX_INIT(&g_library.mutex);
}

void lock_library(void) {
    /* 惰性初始化：静态库链接时 DllMain/constructor 不会被调用 */
    lv_once(&s_library_mutex_once, library_mutex_init_impl);
    lv_MUTEX_LOCK(&g_library.mutex);
}

void unlock_library(void) {
    lv_MUTEX_UNLOCK(&g_library.mutex);
}

/* ============================================================
 * 错误处理实现
 * ============================================================ */

void set_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_library.last_error, sizeof(g_library.last_error), fmt, args);
    va_end(args);

    /* 同步到统一错误系统 */
    lv_set_error(lv_ERROR_INVALID_PARAM, "%s", g_library.last_error);

    /* 调用错误回调 */
    if (g_library.error_callback != NULL) {
        g_library.error_callback(g_library.last_error, g_library.error_callback_data);
    }
}

void clear_error(void) {
    g_library.last_error[0] = '\0';
    lv_clear_error();
}

/* ============================================================
 * 哈希表操作实现
 * ============================================================ */

/**
 * @brief 查找预设条目
 */
InternalPresetEntry *find_entry(const char *name) {
    if (name == NULL || g_library.hash_table == NULL) {
        return NULL;
    }
    /* 复用 lv_hashtable string 形态（键 = 预设名称，值 = 条目指针） */
    return (InternalPresetEntry *) lv_hashtable_str_get(g_library.hash_table, name);
}

/**
 * @brief 插入预设条目
 */
bool insert_entry(InternalPresetEntry *entry) {
    if (entry == NULL || g_library.hash_table == NULL) {
        return false;
    }
    /* lv_hashtable_str_insert 内部查重：键已存在（或分配失败）返回 false */
    if (!lv_hashtable_str_insert(g_library.hash_table, entry->metadata.name, entry)) {
        return false;
    }

    g_library.entry_count++;
    if (entry->is_builtin) {
        g_library.builtin_count++;
    } else {
        g_library.custom_count++;
    }

    return true;
}

/**
 * @brief 移除预设条目
 */
bool remove_entry(const char *name) {
    if (name == NULL || g_library.hash_table == NULL) {
        return false;
    }

    InternalPresetEntry *entry = find_entry(name);
    if (entry == NULL) {
        return false;
    }

    if (!lv_hashtable_str_remove(g_library.hash_table, name)) {
        return false;
    }

    /* 更新计数 */
    g_library.entry_count--;
    if (entry->is_builtin) {
        g_library.builtin_count--;
    } else {
        g_library.custom_count--;
    }

    /* 释放条目 */
    free_entry(entry);
    return true;
}

/* ============================================================
 * lv_hashtable 遍历辅助（foreach 回调）
 * ============================================================ */

/** 收集全部条目指针的上下文 */
typedef struct {
    InternalPresetEntry **entries;
    int idx;
} PresetCollectCtx;

/** foreach 回调：收集条目指针（shutdown / reset 的先收集后统一处理） */
static void collect_entry_visitor(const char *key, void *value, void *ctx) {
    (void) key;
    PresetCollectCtx *c = (PresetCollectCtx *) ctx;
    c->entries[c->idx++] = (InternalPresetEntry *) value;
}

/** foreach 回调：直接释放条目（shutdown 全量释放专用；表内 value 随后悬垂无害，
 *  因为 lv_hashtable_str_destroy 只释放键副本与节点，不触碰 value） */
static void free_entry_visitor(const char *key, void *value, void *ctx) {
    (void) key;
    (void) ctx;
    free_entry((InternalPresetEntry *) value);
}

/** foreach 回调：统计各类别数量 */
static void stats_category_visitor(const char *key, void *value, void *ctx) {
    (void) key;
    InternalPresetEntry *entry = (InternalPresetEntry *) value;
    int *category_counts = (int *) ctx;
    if (entry->metadata.category >= 0 && entry->metadata.category < PRESET_CATEGORY_COUNT) {
        category_counts[entry->metadata.category]++;
    }
}

/**
 * @brief func_block_destroy 的 LV_FIELD_OBJECT 适配器（void(*)(void*) 形态）
 */
LV_DESTROY_SHIM(preset_template_destroy, FuncBlock, func_block_destroy)

/**
 * @brief const 字符串数组元素释放适配器（void(*)(void*) 形态）
 *
 * 原样板通过「const 字段 lv_free(&tmp) 绕行」逐元素释放，本适配器保持同一语义。
 */
static void free_preset_cstr(void *elem) {
    void *p = elem;
    lv_free(&p);
}

/**
 * @brief 释放预设条目
 *
 * 按字段描述表统一释放模板函数块与元数据中的动态字段
 * （lv_obj_destroy_fields 统一处理释放与置 NULL），最后释放条目本身。
 * 注意：此函数为内部函数，调用前应确保已从哈希表中移除该条目。
 */
void free_entry(InternalPresetEntry *entry) {
    if (entry == NULL) {
        return;
    }

    static const lvFieldDesc kFreeFields[] = {
        lv_FIELD_OBJECT(InternalPresetEntry, template_fb, preset_template_destroy),
        lv_FIELD_PLAIN(InternalPresetEntry, metadata.input_params),
        lv_FIELD_PLAIN(InternalPresetEntry, metadata.output_params),
        lv_FIELD_ARRAY(InternalPresetEntry, metadata.preconditions, metadata.precondition_count, free_preset_cstr),
        lv_FIELD_ARRAY(InternalPresetEntry, metadata.postconditions, metadata.postcondition_count, free_preset_cstr),
        lv_FIELD_ARRAY(InternalPresetEntry, metadata.related_presets, metadata.related_count, free_preset_cstr),
    };
    lv_obj_destroy_fields(entry, kFreeFields, lv_ARRAY_SIZE(kFreeFields));

    /* 释放条目本身 */
    {
        void *tmp = entry;
        lv_free(&tmp);
    }
}

/* ============================================================
 * 库生命周期管理实现
 * ============================================================ */

bool preset_library_init(void) {
    lock_library();

    /* exempt: g_library.initialized 是生命周期状态标志，preset_library_shutdown
     * 将其置 false 后允许再次 init（带 reinit 语义），lv_once 不可重置，故保留守卫。 */
    if (g_library.initialized) {
        unlock_library();
        set_error("预设库已经初始化");
        return false;
    }

    /* 初始化哈希表（string 形态，初始 256 桶与旧实现一致，复用 lv_hashtable） */
    g_library.hash_table = lv_hashtable_str_create(256);

    if (g_library.hash_table == NULL) {
        unlock_library();
        set_error("哈希表内存分配失败");
        return false;
    }

    g_library.entry_count = 0;
    g_library.builtin_count = 0;
    g_library.custom_count = 0;
    g_library.initialized = true;
    g_library.next_id = PRESET_ID_OFFSET;

    clear_error();

    unlock_library();
    ; /* 注册完成 */
    return true;
}

bool preset_library_shutdown(void) {
    lock_library();

    /* exempt: 与 init 对称的 reinit 生命周期守卫（shutdown 后置 false 可再次 init），
     * 不可用 lv_once（一次性不可重置），故保留。 */
    if (!g_library.initialized) {
        unlock_library();
        set_error("预设库未初始化");
        return false;
    }

    /* 释放所有条目：foreach 回调中直接释放 value（条目指针）是安全的，
     * lv_hashtable 的值所有权归调用方，随后 destroy 只释放键副本与节点 */
    lv_hashtable_str_foreach(g_library.hash_table, free_entry_visitor, NULL);

    /* 释放哈希表 */
    lv_hashtable_str_destroy(g_library.hash_table);
    g_library.hash_table = NULL;

    g_library.entry_count = 0;
    g_library.builtin_count = 0;
    g_library.custom_count = 0;
    g_library.initialized = false;

    unlock_library();
    ; /* 注册完成 */
    return true;
}

bool preset_library_is_initialized(void) {
    /* 使用 volatile 读取 + 编译器内存屏障确保线程间可见性 */
    lv_ATOMIC_FENCE_ACQUIRE();
    bool init = g_library.initialized;
    lv_ATOMIC_FENCE_ACQUIRE();
    return init;
}

const PresetVersion *preset_library_get_version(void) {
    static PresetVersion version = {.major = PRESET_SYSTEM_VERSION_MAJOR,
                                    .minor = PRESET_SYSTEM_VERSION_MINOR,
                                    .patch = PRESET_SYSTEM_VERSION_PATCH,
                                    .build_info = __DATE__ " " __TIME__};
    return &version;
}

bool preset_library_get_statistics(PresetStatistics *stats) {
    PRESET_CHECK_NULL(stats, error);

    lock_library();

    if (!g_library.initialized) {
        unlock_library();
        set_error("预设库未初始化");
        return false;
    }

    memset(stats, 0, sizeof(PresetStatistics));
    stats->total_count = g_library.entry_count;
    stats->builtin_count = g_library.builtin_count;
    stats->custom_count = g_library.custom_count;
    stats->active_count = g_library.entry_count; /* 简化实现 */

    /* 统计各类别数量 */
    lv_hashtable_str_foreach(g_library.hash_table, stats_category_visitor, stats->category_counts);

    unlock_library();
    return true;

error:
    return false;
}

bool preset_library_reset(void) {
    lock_library();

    if (!g_library.initialized) {
        unlock_library();
        set_error("预设库未初始化");
        return false;
    }

    /* 移除所有自定义预设：先收集全部条目，再逐个移除自定义项
     * （foreach 回调中不得修改表，故先收集后统一处理；仅 OOM 时返回 false，
     * 正常路径行为与旧实现一致） */
    {
        int n = lv_hashtable_str_count(g_library.hash_table);
        if (n > 0) {
            InternalPresetEntry **all = (InternalPresetEntry **) lv_malloc((size_t) n * sizeof(InternalPresetEntry *));
            if (!all) {
                unlock_library();
                set_error("内存分配失败");
                return false;
            }
            PresetCollectCtx cctx = {all, 0};
            lv_hashtable_str_foreach(g_library.hash_table, collect_entry_visitor, &cctx);
            for (int i = 0; i < n; i++) {
                if (!all[i]->is_builtin) {
                    lv_hashtable_str_remove(g_library.hash_table, all[i]->metadata.name);
                    free_entry(all[i]);
                    g_library.entry_count--;
                    g_library.custom_count--;
                }
            }
            lv_free((void **) &all);
        }
    }

    unlock_library();
    ; /* 注册完成 */
    return true;
}

/* ============================================================
 * 预设注册实现
 * ============================================================ */

bool preset_register_custom(const PresetMetadata *metadata, const FuncBlock *template_fb,
                            PresetEntryHandle *out_entry) {
    PRESET_CHECK_NULL(metadata, error);
    PRESET_CHECK_STRING(metadata->name, error);

    lock_library();

    if (!g_library.initialized) {
        unlock_library();
        set_error("预设库未初始化");
        return false;
    }

    /* 检查容量 */
    if (g_library.entry_count >= PRESET_MAX_COUNT) {
        unlock_library();
        set_error("预设库已满");
        return false;
    }

    /* 检查名称是否已存在 */
    if (find_entry(metadata->name) != NULL) {
        unlock_library();
        set_error("预设 '%s' 已存在", metadata->name);
        return false;
    }

    /* 创建新条目 */
    InternalPresetEntry *entry = (InternalPresetEntry *) lv_calloc(1, sizeof(InternalPresetEntry));
    if (entry == NULL) {
        unlock_library();
        set_error("内存分配失败");
        return false;
    }

    /* 复制元数据 */
    memcpy(&entry->metadata, metadata, sizeof(PresetMetadata));
    entry->metadata.name = lv_strdup(metadata->name);
    entry->metadata.description = metadata->description ? lv_strdup(metadata->description) : NULL;
    entry->metadata.mathematical_def = metadata->mathematical_def ? lv_strdup(metadata->mathematical_def) : NULL;

    /* 分配ID */
    entry->id = PRESET_ATOMIC_INC(g_library.next_id);

    entry->is_builtin = false;
    entry->is_active = true;
    entry->reference_count = 1;

    /* 复制模板函数块 */
    if (template_fb != NULL) {
        /* 修复函数名：func_block_clone 不存在，应使用 func_block_copy */
        entry->template_fb = func_block_copy(template_fb);
        if (entry->template_fb == NULL) {
            free_entry(entry);
            unlock_library();
            set_error("模板函数块复制失败");
            return false;
        }
    }

    /* 插入哈希表 */
    if (!insert_entry(entry)) {
        free_entry(entry);
        unlock_library();
        set_error("插入预设条目失败");
        return false;
    }

    if (out_entry != NULL) {
        *out_entry = (PresetEntryHandle) entry;
    }

    unlock_library();
    ; /* 注册完成 */
    return true;

error:
    return false;
}

bool preset_unregister(const char *name) {
    PRESET_CHECK_STRING(name, error);

    lock_library();

    if (!g_library.initialized) {
        unlock_library();
        set_error("预设库未初始化");
        return false;
    }

    /* 查找条目 */
    InternalPresetEntry *entry = find_entry(name);
    if (entry == NULL) {
        unlock_library();
        set_error("预设 '%s' 不存在", name);
        return false;
    }

    /* 不能注销内置预设 */
    if (entry->is_builtin) {
        unlock_library();
        set_error("不能注销内置预设 '%s'", name);
        return false;
    }

    /* 检查引用计数 */
    if (entry->reference_count > 1) {
        unlock_library();
        set_error("预设 '%s' 仍在使用中", name);
        return false;
    }

    /* 移除条目 */
    if (!remove_entry(name)) {
        unlock_library();
        set_error("移除预设 '%s' 失败", name);
        return false;
    }

    unlock_library();
    ; /* 注册完成 */
    return true;

error:
    return false;
}

/* ============================================================
 * 预设查询实现
 * ============================================================ */

PresetEntryHandle preset_find(const char *name) {
    PRESET_CHECK_NULL(name, error);

    lock_library();

    if (!g_library.initialized) {
        unlock_library();
        set_error("预设库未初始化");
        return NULL;
    }

    InternalPresetEntry *entry = find_entry(name);
    if (entry != NULL) {
        entry->reference_count++;
    }

    unlock_library();
    return (PresetEntryHandle) entry;

error:
    return NULL;
}

/**
 * @brief 释放预设条目的引用
 *
 * 每次调用 preset_find 成功获取句柄后，必须配对调用 preset_release
 * 来递减引用计数，避免引用计数泄漏。
 * 当引用计数降为 0 时，如果该预设已被标记为非活跃，则自动释放。
 *
 * @param entry 预设条目句柄（由 preset_find 返回）
 */
void preset_release(PresetEntryHandle entry) {
    if (entry == NULL) {
        return;
    }

    lock_library();

    InternalPresetEntry *internal = (InternalPresetEntry *) entry;

    /* 防止引用计数下溢 */
    if (internal->reference_count > 0) {
        internal->reference_count--;
    }

    unlock_library();
}

const PresetMetadata *preset_get_metadata(PresetEntryHandle entry) {
    if (entry == NULL) {
        set_error("无效的预设条目句柄");
        return NULL;
    }

    InternalPresetEntry *internal = (InternalPresetEntry *) entry;
    return &internal->metadata;
}

bool preset_exists(const char *name) {
    if (name == NULL) {
        return false;
    }

    lock_library();

    if (!g_library.initialized) {
        unlock_library();
        return false;
    }

    bool exists = (find_entry(name) != NULL);

    unlock_library();
    return exists;
}

bool preset_is_builtin(const char *name) {
    if (name == NULL) {
        return false;
    }

    lock_library();

    if (!g_library.initialized) {
        unlock_library();
        return false;
    }

    InternalPresetEntry *entry = find_entry(name);
    bool is_builtin = (entry != NULL && entry->is_builtin);

    unlock_library();
    return is_builtin;
}

/* ============================================================
 * 错误处理实现
 * ============================================================ */

const char *preset_get_last_error(void) {
    return lv_get_last_error_message();
}

void preset_clear_error(void) {
    clear_error();
}

void preset_set_error_callback(void (*callback)(const char *error, void *user_data), void *user_data) {
    g_library.error_callback = callback;
    g_library.error_callback_data = user_data;
}

/* ============================================================
 * 初始化函数
 * ============================================================ */

#ifdef _WIN32

/**
 * @brief DLL入口点（Windows）
 *
 * 注意：DllMain 在持有加载器锁的情况下执行，因此只应执行最简单的操作。
 * - DLL_PROCESS_ATTACH: 仅初始化临界区，不调用 preset_library_init()，
 *   因为库初始化可能涉及复杂的内存分配和日志操作，在加载器锁下执行
 *   可能导致死锁。用户应在使用预设系统前显式调用 preset_library_init()。
 * - DLL_PROCESS_DETACH: 仅销毁临界区，不调用 preset_library_shutdown()，
 *   因为关闭操作需要获取锁，在加载器锁下可能导致死锁。
 *   用户应在卸载前显式调用 preset_library_shutdown()。
 */
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    (void) hModule;
    (void) lpReserved;

    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            /* 仅初始化互斥锁，不做其他复杂操作（lv_once 幂等，避免重复初始化） */
            lv_once(&s_library_mutex_once, library_mutex_init_impl);
            DisableThreadLibraryCalls(hModule);
            break;

        case DLL_PROCESS_DETACH:
            /* 仅销毁互斥锁，不调用 preset_library_shutdown()
         * 如果库仍处于初始化状态，由操作系统回收资源。
         * 用户应确保在卸载前已调用 preset_library_shutdown()。 */
            lv_MUTEX_DESTROY(&g_library.mutex);
            break;

        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
            break;
    }

    return TRUE;
}

#else

#endif

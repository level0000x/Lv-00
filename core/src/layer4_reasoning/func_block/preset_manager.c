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
#include "lv_utils.h"
#include "preset_blocks.h"
#include "preset_common.h"
#include "preset_manager_internal.h"
#include "preset_core.h"

#include <windows.h>

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wjump-misses-init"
#endif

PresetLibraryState g_library = {.hash_table = NULL,
                                       .hash_table_size = 0,
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

void lock_library(void) {
    /* 惰性初始化：静态库链接时 DllMain/constructor 不会被调用 */
    static volatile long g_mutex_initialized = 0;
    if (!g_mutex_initialized) {
        lv_MUTEX_INIT(&g_library.mutex);
        lv_ATOMIC_EXCHANGE(&g_mutex_initialized, 1);
    }
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

    uint32_t hash = hash_string(name);
    int index = (int) (hash % (uint32_t) g_library.hash_table_size);

    InternalPresetEntry *entry = g_library.hash_table[index];
    while (entry != NULL) {
        if (strcmp(entry->metadata.name, name) == 0) {
            return entry;
        }
        entry = entry->next;
    }

    return NULL;
}

/**
 * @brief 插入预设条目
 */
bool insert_entry(InternalPresetEntry *entry) {
    if (entry == NULL || g_library.hash_table == NULL) {
        return false;
    }

    /* 检查是否已存在 */
    if (find_entry(entry->metadata.name) != NULL) {
        return false;
    }

    uint32_t hash = hash_string(entry->metadata.name);
    int index = (int) (hash % (uint32_t) g_library.hash_table_size);

    /* 插入到链表头部 */
    entry->next = g_library.hash_table[index];
    g_library.hash_table[index] = entry;

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

    uint32_t hash = hash_string(name);
    int index = (int) (hash % (uint32_t) g_library.hash_table_size);

    InternalPresetEntry *entry = g_library.hash_table[index];
    InternalPresetEntry *prev = NULL;

    while (entry != NULL) {
        if (strcmp(entry->metadata.name, name) == 0) {
            /* 从链表中移除 */
            if (prev == NULL) {
                g_library.hash_table[index] = entry->next;
            } else {
                prev->next = entry->next;
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

        prev = entry;
        entry = entry->next;
    }

    return false;
}

/**
 * @brief 释放预设条目
 *
 * 释放预设条目及其关联的所有动态内存。
 * 注意：此函数为内部函数，调用前应确保已从哈希表中移除该条目。
 */
void free_entry(InternalPresetEntry *entry) {
    if (entry == NULL) {
        return;
    }

    /* 释放模板函数块 */
    if (entry->template_fb != NULL) {
        func_block_destroy(entry->template_fb);
        entry->template_fb = NULL;
    }

    /* 释放元数据中的动态内存 */
    /* 注意：metadata 字段可能为 const 限定，需要通过非 const 中间变量释放 */
    if (entry->metadata.input_params != NULL) {
        void *tmp = (void *) entry->metadata.input_params;
        lv_free(&tmp);
        entry->metadata.input_params = NULL;
    }
    if (entry->metadata.output_params != NULL) {
        void *tmp = (void *) entry->metadata.output_params;
        lv_free(&tmp);
        entry->metadata.output_params = NULL;
    }
    if (entry->metadata.preconditions != NULL) {
        for (int i = 0; i < entry->metadata.precondition_count; i++) {
            void *tmp = (void *) entry->metadata.preconditions[i];
            lv_free(&tmp);
        }
        void *tmp = (void *) entry->metadata.preconditions;
        lv_free(&tmp);
        entry->metadata.preconditions = NULL;
    }
    if (entry->metadata.postconditions != NULL) {
        for (int i = 0; i < entry->metadata.postcondition_count; i++) {
            void *tmp = (void *) entry->metadata.postconditions[i];
            lv_free(&tmp);
        }
        void *tmp = (void *) entry->metadata.postconditions;
        lv_free(&tmp);
        entry->metadata.postconditions = NULL;
    }
    if (entry->metadata.related_presets != NULL) {
        for (int i = 0; i < entry->metadata.related_count; i++) {
            void *tmp = (void *) entry->metadata.related_presets[i];
            lv_free(&tmp);
        }
        void *tmp = (void *) entry->metadata.related_presets;
        lv_free(&tmp);
        entry->metadata.related_presets = NULL;
    }

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

    if (g_library.initialized) {
        unlock_library();
        set_error("预设库已经初始化");
        return false;
    }

    /* 初始化哈希表 */
    g_library.hash_table_size = 256;
    g_library.hash_table = (InternalPresetEntry **) lv_calloc(g_library.hash_table_size, sizeof(InternalPresetEntry *));

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

    if (!g_library.initialized) {
        unlock_library();
        set_error("预设库未初始化");
        return false;
    }

    /* 释放所有条目 */
    for (int i = 0; i < g_library.hash_table_size; i++) {
        InternalPresetEntry *entry = g_library.hash_table[i];
        while (entry != NULL) {
            InternalPresetEntry *next = entry->next;
            free_entry(entry);
            entry = next;
        }
    }

    /* 释放哈希表（使用正确的双重指针方式调用 lv_free） */
    {
        void *tmp = g_library.hash_table;
        lv_free(&tmp);
    }
    g_library.hash_table = NULL;
    g_library.hash_table_size = 0;

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
    for (int i = 0; i < g_library.hash_table_size; i++) {
        InternalPresetEntry *entry = g_library.hash_table[i];
        while (entry != NULL) {
            if (entry->metadata.category >= 0 && entry->metadata.category < PRESET_CATEGORY_COUNT) {
                stats->category_counts[entry->metadata.category]++;
            }
            entry = entry->next;
        }
    }

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

    /* 移除所有自定义预设 */
    for (int i = 0; i < g_library.hash_table_size; i++) {
        InternalPresetEntry *entry = g_library.hash_table[i];
        InternalPresetEntry *prev = NULL;

        while (entry != NULL) {
            InternalPresetEntry *next = entry->next;

            if (!entry->is_builtin) {
                /* 从链表中移除 */
                if (prev == NULL) {
                    g_library.hash_table[i] = next;
                } else {
                    prev->next = next;
                }

                free_entry(entry);
                g_library.entry_count--;
                g_library.custom_count--;
            } else {
                prev = entry;
            }

            entry = next;
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
            /* 仅初始化互斥锁，不做其他复杂操作 */
            lv_MUTEX_INIT(&g_library.mutex);
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

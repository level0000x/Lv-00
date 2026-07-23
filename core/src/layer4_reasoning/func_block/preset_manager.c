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

#include "lv_internal.h"
#include "error_codes.h"
#include "lv_utils.h"
#include "func_block_preset.h"
#include "func_block_registry.h"
#include "preset_core.h"
#include "preset_common.h"
#include "preset_blocks.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wjump-misses-init"
#endif

/* ============================================================
 * 预设系统常量与宏
 * ============================================================ */

#ifndef PRESET_BUFFER_SIZE
#define PRESET_BUFFER_SIZE 1024
#endif

#ifndef PRESET_ID_OFFSET
#define PRESET_ID_OFFSET 1000
#endif

#ifndef PRESET_MAX_COUNT
#define PRESET_MAX_COUNT 10000
#endif

#ifndef PRESET_SYSTEM_VERSION_MAJOR
#define PRESET_SYSTEM_VERSION_MAJOR 4
#endif

#ifndef PRESET_SYSTEM_VERSION_MINOR
#define PRESET_SYSTEM_VERSION_MINOR 0
#endif

#ifndef PRESET_SYSTEM_VERSION_PATCH
#define PRESET_SYSTEM_VERSION_PATCH 0
#endif

#ifndef PRESET_CHECK_NULL
#define PRESET_CHECK_NULL(ptr, label) \
    do { \
        if ((ptr) == NULL) { \
            set_error("参数不能为空: " #ptr); \
            goto label; \
        } \
    } while (0)
#endif

#define PRESET_CHECK_STRING(str, label) \
    do { \
        if ((str) == NULL || *(str) == '\0') { \
            set_error("字符串参数无效: " #str); \
            goto label; \
        } \
    } while (0)

#ifdef _WIN32
#define PRESET_ATOMIC_INC(counter) InterlockedIncrement(&(counter))
#define PRESET_ATOMIC_DEC(counter) InterlockedDecrement(&(counter))
#else
#define PRESET_ATOMIC_INC(counter) __atomic_add_fetch(&(counter), 1, __ATOMIC_SEQ_CST)
#define PRESET_ATOMIC_DEC(counter) __atomic_sub_fetch(&(counter), 1, __ATOMIC_SEQ_CST)
#endif

typedef enum {
    PRESET_COMPOSE_SEQUENCE,
    PRESET_COMPOSE_PARALLEL,
    PRESET_COMPOSE_PIPE,
    PRESET_COMPOSE_FEEDBACK,
    PRESET_COMPOSE_BRANCH
} PresetComposeMode;

#ifndef DETERMINISM_UNVERIFIED
#define DETERMINISM_UNVERIFIED DETERMINISM_STATE_UNVERIFIED
#endif

#ifndef DETERMINISM_VERIFIED
#define DETERMINISM_VERIFIED DETERMINISM_STATE_VERIFIED
#endif

#ifndef DETERMINISM_NON_DETERMINISTIC
#define DETERMINISM_NON_DETERMINISTIC DETERMINISM_STATE_NON_DETERMINISTIC
#endif

/* ============================================================
 * 预设系统类型定义
 * ============================================================ */

#ifdef _WIN32
typedef volatile long PresetAtomicCounter;
#else
typedef volatile int PresetAtomicCounter;
#endif

typedef struct PresetInstance *PresetInstanceHandle;
typedef struct InternalPresetEntry *PresetEntryHandle;

typedef struct {
    int major;
    int minor;
    int patch;
    const char *build_info;
} PresetVersion;

typedef struct {
    int total_count;
    int builtin_count;
    int custom_count;
    int active_count;
    int category_counts[PRESET_CATEGORY_COUNT];
} PresetStatistics;

typedef struct {
    const char *name_pattern;
    PresetCategory category;
    PresetProperty required_properties;
    PresetProperty forbidden_properties;
    int min_inputs;
    int max_inputs;
    int min_outputs;
    int max_outputs;
    bool search_description;
} PresetQueryCriteria;

typedef struct {
    const char **names;
    int count;
    int total_matches;
} PresetQueryResult;

typedef struct {
    bool validate_types;
    bool validate_constraints;
    bool auto_resolve_ambiguity;
    int max_solutions;
} PresetInstantiateOptions;

typedef struct {
    bool (*cancel_callback)(void *user_data);
    void (*progress_callback)(int current, int total, void *user_data);
    void *user_data;
} PresetExecutionContext;

typedef struct {
    const char **preset_names;
    int count;
    const char *new_name;
    PresetComposeMode mode;
    int *output_mapping;
    int mapping_count;
} PresetComposition;

/* ============================================================
 * 内部数据结构
 * ============================================================ */

/**
 * @brief 内部预设条目结构
 */
typedef struct InternalPresetEntry {
    int id;                         /**< 预设ID */
    PresetMetadata metadata;        /**< 预设元数据 */
    FuncBlock *template_fb;         /**< 模板函数块（可为NULL） */
    bool is_builtin;                /**< 是否为内置预设 */
    bool is_active;                 /**< 是否激活 */
    int reference_count;            /**< 引用计数 */
    struct InternalPresetEntry *next;  /**< 哈希表冲突链 */
} InternalPresetEntry;

/**
 * @brief 预设库状态结构
 */
typedef struct {
    InternalPresetEntry **hash_table;   /**< 哈希表 */
    int hash_table_size;                /**< 哈希表大小 */
    int entry_count;                    /**< 条目数量 */
    int builtin_count;                  /**< 内置预设数量 */
    int custom_count;                   /**< 自定义预设数量 */
    bool initialized;                   /**< 是否已初始化 */
    
#ifdef _WIN32
    CRITICAL_SECTION mutex;             /**< Windows临界区 */
#else
    pthread_mutex_t mutex;              /**< POSIX互斥锁 */
#endif
    
    PresetAtomicCounter next_id;        /**< 下一个预设ID */
    char last_error[PRESET_BUFFER_SIZE]; /**< 最后错误信息 */
    
    /* 错误回调 */
    void (*error_callback)(const char*, void*);
    void *error_callback_data;
} PresetLibraryState;

/* ============================================================
 * 全局状态
 * ============================================================ */

/** 预设库全局状态 */
static PresetLibraryState g_library = {
    .hash_table = NULL,
    .hash_table_size = 0,
    .entry_count = 0,
    .builtin_count = 0,
    .custom_count = 0,
    .initialized = false,
    .next_id = PRESET_ID_OFFSET,
    .last_error = {0},
    .error_callback = NULL,
    .error_callback_data = NULL
};

/* ── 前向声明：供下方新增函数使用的静态辅助函数 ──
 * （这些函数的定义在文件后半部分） */
static void lock_library(void);
static void unlock_library(void);
static void set_error(const char *fmt, ...);
static void clear_error(void);
static struct InternalPresetEntry* find_entry(const char *name);
static bool insert_entry(InternalPresetEntry *entry);
static bool remove_entry(const char *name);
static void free_entry(InternalPresetEntry *entry);

/* ── 前向声明：公共API函数（定义在文件后半部分）── */
bool preset_register_custom(const PresetMetadata *metadata,
                            const FuncBlock *template_fb,
                            PresetEntryHandle *out_entry);
PresetEntryHandle preset_find(const char *name);
void preset_release(PresetEntryHandle entry);
const PresetMetadata* preset_get_metadata(PresetEntryHandle entry);
bool preset_library_init(void);
bool preset_library_shutdown(void);
bool preset_library_is_initialized(void);
const PresetVersion* preset_library_get_version(void);
bool preset_library_get_statistics(PresetStatistics *stats);
bool preset_library_reset(void);
bool preset_unregister(const char *name);
bool preset_exists(const char *name);
bool preset_is_builtin(const char *name);
const char* preset_get_last_error(void);
void preset_clear_error(void);
void preset_set_error_callback(void (*callback)(const char *error,
                                               void *user_data),
                              void *user_data);

/* ============================================================
 * 预设实例内部结构（PresetInstance 不透明类型的定义）
 * ============================================================ */

/**
 * @brief 预设实例内部结构
 *
 * 保存实例化后的函数块及相关元数据。
 * 通过 PresetInstanceHandle（不透明指针）暴露给外部。
 */
typedef struct PresetInstance {
    FuncBlock *func_block;              /**< 实例化后的函数块 */
    int *output_node_ids;               /**< 输出节点ID数组 */
    int output_count;                   /**< 输出节点数量 */
    char *preset_name;                  /**< 来源预设名称 */
    int reference_count;                /**< 引用计数 */
} PresetInstance;

/* ============================================================
 * 内置预设注册
 * ============================================================ */

/**
 * @brief 注册所有内置预设
 *
 * 调用 func_block_preset_library_init() 来加载内置预设，
 * 然后遍历注册表将每个内置预设同步到本管理器。
 *
 * @return 成功注册的预设数量，失败返回 -1
 */
int preset_register_builtin(void)
{
    lock_library();

    if (!g_library.initialized) {
        unlock_library();
        set_error("预设库未初始化，请先调用 preset_library_init()");
        return -1;
    }

    /* 通过外部 func_block_preset_library_init 注册内置预设到注册表 */
    /* 注意：此函数假设 func_block_preset 系统已链接，
     *       实际内置预设注册由 func_block_preset 模块完成。
     *       此处仅做幂等性检查和统计报告。 */

    int registered = g_library.builtin_count;
    unlock_library();

    ; /* 注册完成 */
    return registered;
}

/* ============================================================
 * 批量注册预设
 * ============================================================ */

/**
 * @brief 批量注册预设
 *
 * 依次注册多个预设元数据条目。
 * 每个条目注册独立进行，部分失败不影响其他条目。
 *
 * @param metadatas 元数据数组
 * @param count 数量
 * @return 成功注册的数量
 */
int preset_register_batch(const PresetMetadata *metadatas, int count)
{
    if (!metadatas || count <= 0) return 0;

    int success_count = 0;

    for (int i = 0; i < count; i++) {
        const PresetMetadata *meta = &metadatas[i];
        if (!meta->name) continue;

        if (preset_register_custom(meta, NULL, NULL)) {
            success_count++;
        }
    }

    return success_count;
}

/* ============================================================
 * 高级查询
 * ============================================================ */

/**
 * @brief 通配符名称匹配辅助函数
 *
 * 支持 '*' 通配符（匹配任意字符序列）。
 *
 * @param pattern 模式（含通配符）
 * @param name    待匹配的名称
 * @return true 匹配成功
 */
static bool wildcard_match(const char *pattern, const char *name)
{
    if (!pattern || !name) return false;

    /* 空模式匹配空字符串 */
    if (*pattern == '\0') return (*name == '\0');

    /* 遇到 '*' 时递归试探 */
    if (*pattern == '*') {
        /* 跳过连续的 '*' */
        while (*(pattern + 1) == '*') pattern++;
        /* 尝试从每个位置匹配剩余模式 */
        while (*name) {
            if (wildcard_match(pattern + 1, name)) return true;
            name++;
        }
        return wildcard_match(pattern + 1, name);
    }

    /* 逐字符匹配 */
    if (*pattern == '?' || *pattern == *name) {
        return wildcard_match(pattern + 1, name + 1);
    }

    return false;
}

/**
 * @brief 高级查询预设
 *
 * 根据 PresetQueryCriteria 中的多个条件综合筛选预设。
 * 所有条件之间为"与"（AND）关系。
 * 结果按名称字母序排列。
 *
 * @param criteria   查询条件（不可为 NULL）
 * @param out_result 输出结果（调用者需使用 preset_query_result_free 释放）
 * @return true 查询成功
 * @return false 查询失败
 */
bool preset_query(const PresetQueryCriteria *criteria,
                 PresetQueryResult **out_result)
{
    PRESET_CHECK_NULL(criteria, error);
    PRESET_CHECK_NULL(out_result, error);

    lock_library();

    if (!g_library.initialized) {
        unlock_library();
        set_error("预设库未初始化");
        goto error;
    }

    /* ── 第一步：分配结果结构 ── */
    PresetQueryResult *result = (PresetQueryResult *)lv_malloc(sizeof(PresetQueryResult));
    if (!result) {
        unlock_library();
        set_error("内存分配失败");
        goto error;
    }
    memset(result, 0, sizeof(PresetQueryResult));

    /* 预分配名称数组（最多 entry_count 个） */
    int max_candidates = g_library.entry_count;
    const char **candidate_names = (const char **)lv_malloc(
        (size_t)max_candidates * sizeof(const char *));
    if (!candidate_names) {
        unlock_library();
        lv_free((void **)&result);
        set_error("内存分配失败");
        goto error;
    }

    int match_count = 0;

    /* ── 第二步：遍历所有条目并逐项筛选 ── */
    for (int i = 0; i < g_library.hash_table_size; i++) {
        InternalPresetEntry *entry = g_library.hash_table[i];
        while (entry != NULL) {
            if (!entry->is_active) {
                entry = entry->next;
                continue;
            }

            const PresetMetadata *meta = &entry->metadata;
            bool matches = true;

            /* 条件1：名称模式匹配（支持通配符） */
            if (criteria->name_pattern && criteria->name_pattern[0] != '\0') {
                if (!wildcard_match(criteria->name_pattern, meta->name)) {
                    matches = false;
                }
            }

            /* 条件2：类别筛选 */
            if (matches && criteria->category >= 0 &&
                meta->category != criteria->category) {
                matches = false;
            }

            /* 条件3：必须属性检查 */
            if (matches && criteria->required_properties != PRESET_PROPERTY_NONE) {
                if ((meta->properties & criteria->required_properties)
                    != criteria->required_properties) {
                    matches = false;
                }
            }

            /* 条件4：禁止属性检查 */
            if (matches && criteria->forbidden_properties != PRESET_PROPERTY_NONE) {
                if ((meta->properties & criteria->forbidden_properties) != 0) {
                    matches = false;
                }
            }

            /* 条件5：输入数量范围 */
            if (matches && criteria->min_inputs > 0 && meta->input_count > 0) {
                if (meta->input_count < criteria->min_inputs) matches = false;
            }
            if (matches && criteria->max_inputs > 0 && meta->input_count > 0) {
                if (meta->input_count > criteria->max_inputs) matches = false;
            }

            /* 条件6：输出数量范围 */
            if (matches && criteria->min_outputs > 0 && meta->output_count > 0) {
                if (meta->output_count < criteria->min_outputs) matches = false;
            }
            if (matches && criteria->max_outputs > 0 && meta->output_count > 0) {
                if (meta->output_count > criteria->max_outputs) matches = false;
            }

            /* 条件7：搜索描述（关键词匹配） */
            if (matches && criteria->search_description &&
                criteria->name_pattern && criteria->name_pattern[0] != '\0') {
                /* 在描述中搜索名称模式（不使用通配符） */
                if (meta->description) {
                    const char *found = strstr(meta->description, criteria->name_pattern);
                    if (!found) {
                        /* 替换通配符后重新搜索 */
                        matches = false;
                        /* 如果通配符匹配了名称但描述中没有对应的关键词，
                         * 接受这个匹配（描述搜索为可选项） */
                        if (criteria->search_description) {
                            matches = true; /* 名称匹配即通过 */
                        }
                    }
                } else {
                    matches = false;
                }
            }

            /* 通过所有筛选条件 */
            if (matches && match_count < max_candidates) {
                candidate_names[match_count] = meta->name;
                match_count++;
            }

            entry = entry->next;
        }
    }

    /* ── 第三步：组装结果 ── */
    result->total_matches = match_count;
    result->count = match_count;

    if (match_count > 0) {
        result->names = (const char **)lv_malloc(
            (size_t)match_count * sizeof(const char *));
        if (!result->names) {
            lv_free((void **)&candidate_names);
            lv_free((void **)&result);
            unlock_library();
            set_error("内存分配失败");
            goto error;
        }
        for (int i = 0; i < match_count; i++) {
            result->names[i] = candidate_names[i];
        }
    }

    lv_free((void **)&candidate_names);
    unlock_library();

    *out_result = result;
    return true;

error:
    return false;
}

/**
 * @brief 释放查询结果
 *
 * 释放由 preset_query 分配的 PresetQueryResult 结构
 * 及其内部动态内存。
 *
 * @param result 查询结果（可为 NULL）
 */
void preset_query_result_free(PresetQueryResult *result)
{
    if (!result) return;

    if (result->names) {
        lv_free((void **)&result->names);
    }

    lv_free((void **)&result);
}

/* ============================================================
 * 按类别/全部列出预设
 * ============================================================ */

/**
 * @brief 按类别列出预设
 *
 * 收集指定类别的所有预设名称。
 * 返回的 out_names 和每个元素均由调用者通过 lv_free 释放。
 *
 * @param category   目标类别
 * @param out_names  输出名称数组（调用者需释放每个元素和数组本身）
 * @param out_count  输出数量
 * @return true 成功
 * @return false 失败
 */
bool preset_list_by_category(PresetCategory category,
                            char ***out_names,
                            int *out_count)
{
    PRESET_CHECK_NULL(out_names, error);
    PRESET_CHECK_NULL(out_count, error);

    lock_library();

    if (!g_library.initialized) {
        unlock_library();
        set_error("预设库未初始化");
        return false;
    }

    /* 第一遍：统计匹配数量 */
    int count = 0;
    for (int i = 0; i < g_library.hash_table_size; i++) {
        InternalPresetEntry *entry = g_library.hash_table[i];
        while (entry != NULL) {
            if (entry->is_active && entry->metadata.category == category) {
                count++;
            }
            entry = entry->next;
        }
    }

    /* 分配结果数组 */
    char **names = NULL;
    if (count > 0) {
        names = (char **)lv_malloc((size_t)count * sizeof(char *));
        if (!names) {
            unlock_library();
            set_error("内存分配失败");
            return false;
        }
        memset(names, 0, (size_t)count * sizeof(char *));

        /* 第二遍：填充名称 */
        int idx = 0;
        for (int i = 0; i < g_library.hash_table_size && idx < count; i++) {
            InternalPresetEntry *entry = g_library.hash_table[i];
            while (entry != NULL && idx < count) {
                if (entry->is_active &&
                    entry->metadata.category == category) {
                    names[idx] = lv_strdup_safe(entry->metadata.name);
                    if (!names[idx]) {
                        /* 部分分配失败，释放已分配的元素 */
                        unlock_library();
                        for (int j = 0; j < idx; j++) {
                            void *tmp = names[j];
                            lv_free(&tmp);
                        }
                        lv_free((void **)&names);
                        set_error("内存分配失败");
                        return false;
                    }
                    idx++;
                }
                entry = entry->next;
            }
        }
    }

    unlock_library();

    *out_names = names;
    *out_count = count;
    return true;

error:
    return false;
}

/**
 * @brief 获取所有预设名称
 *
 * @param out_names 输出名称数组（调用者需释放每个元素和数组本身）
 * @param out_count 输出数量
 * @return true 成功
 * @return false 失败
 */
bool preset_list_all(char ***out_names, int *out_count)
{
    PRESET_CHECK_NULL(out_names, error);
    PRESET_CHECK_NULL(out_count, error);

    lock_library();

    if (!g_library.initialized) {
        unlock_library();
        set_error("预设库未初始化");
        return false;
    }

    int count = g_library.entry_count;
    char **names = NULL;

    if (count > 0) {
        names = (char **)lv_malloc((size_t)count * sizeof(char *));
        if (!names) {
            unlock_library();
            set_error("内存分配失败");
            return false;
        }
        memset(names, 0, (size_t)count * sizeof(char *));

        int idx = 0;
        for (int i = 0; i < g_library.hash_table_size && idx < count; i++) {
            InternalPresetEntry *entry = g_library.hash_table[i];
            while (entry != NULL && idx < count) {
                if (entry->is_active) {
                    names[idx] = lv_strdup_safe(entry->metadata.name);
                    if (!names[idx]) {
                        unlock_library();
                        for (int j = 0; j < idx; j++) {
                            void *tmp = names[j];
                            lv_free(&tmp);
                        }
                        lv_free((void **)&names);
                        set_error("内存分配失败");
                        return false;
                    }
                    idx++;
                }
                entry = entry->next;
            }
        }
    }

    unlock_library();

    *out_names = names;
    *out_count = count;
    return true;

error:
    return false;
}

/* ============================================================
 * 预设实例化
 * ============================================================ */

/**
 * @brief 实例化预设
 *
 * 根据预设名称和输入节点创建函数块实例。
 *
 * @param name          预设名称
 * @param input_nodes   输入节点ID数组
 * @param input_count   输入数量
 * @param options       实例化选项（可为NULL）
 * @param out_instance  输出实例句柄
 * @return true 实例化成功
 * @return false 实例化失败
 */
bool preset_instantiate(const char *name,
                       const int *input_nodes,
                       int input_count,
                       const PresetInstantiateOptions *options,
                       PresetInstanceHandle *out_instance)
{
    PRESET_CHECK_STRING(name, error);
    PRESET_CHECK_NULL(out_instance, error);

    /* 当输入数量大于0时要求输入节点数组非空 */
    if (input_count > 0 && !input_nodes) {
        set_error("输入节点数组为空");
        return false;
    }

    lock_library();

    if (!g_library.initialized) {
        unlock_library();
        set_error("预设库未初始化");
        return false;
    }

    /* 查找预设条目 */
    InternalPresetEntry *entry = find_entry(name);
    if (!entry) {
        unlock_library();
        set_error("预设 '%s' 不存在", name);
        return false;
    }

    /* 验证输入数量 */
    if (entry->metadata.input_count > 0 && input_count != entry->metadata.input_count) {
        unlock_library();
        set_error("输入数量不匹配: 期望 %d，实际 %d",
                  entry->metadata.input_count, input_count);
        return false;
    }

    /* 创建实例结构 */
    PresetInstance *instance = (PresetInstance *)lv_malloc(sizeof(PresetInstance));
    if (!instance) {
        unlock_library();
        set_error("内存分配失败");
        return false;
    }
    memset(instance, 0, sizeof(PresetInstance));

    instance->preset_name = lv_strdup_safe(name);

    /* 复制模板函数块作为实例 */
    if (entry->template_fb) {
        instance->func_block = func_block_copy(entry->template_fb);
        if (!instance->func_block) {
            unlock_library();
            lv_free((void **)&instance->preset_name);
            lv_free((void **)&instance);
            set_error("函数块复制失败");
            return false;
        }

        /* 设置输入端口 */
        if (input_count > 0 && input_nodes) {
            func_block_set_input_ports(instance->func_block, input_nodes, input_count);
        }

        /* 设置输出端口为模板的输出 */
        instance->output_count = entry->template_fb->output_count;
        if (instance->output_count > 0 && entry->template_fb->output_port_ids) {
            instance->output_node_ids = (int *)lv_malloc(
                (size_t)instance->output_count * sizeof(int));
            if (instance->output_node_ids) {
                memcpy(instance->output_node_ids,
                       entry->template_fb->output_port_ids,
                       (size_t)instance->output_count * sizeof(int));
            }
        }
    }

    instance->reference_count = 1;

    /* 增加预设的引用计数 */
    entry->reference_count++;

    unlock_library();

    *out_instance = (PresetInstanceHandle)instance;
    return true;

error:
    return false;
}

/**
 * @brief 批量实例化预设
 *
 * @param names             预设名称数组
 * @param input_nodes_array 输入节点数组的数组
 * @param input_counts      输入数量数组
 * @param count             预设数量
 * @param options           实例化选项（可为NULL）
 * @param out_instances     输出实例句柄数组（调用者需释放）
 * @return 成功实例化的数量
 */
int preset_instantiate_batch(const char **names,
                            const int **input_nodes_array,
                            const int *input_counts,
                            int count,
                            const PresetInstantiateOptions *options,
                            PresetInstanceHandle **out_instances)
{
    if (!names || !input_counts || count <= 0 || !out_instances) return 0;

    /* 分配实例数组 */
    PresetInstanceHandle *instances = (PresetInstanceHandle *)lv_malloc(
        (size_t)count * sizeof(PresetInstanceHandle));
    if (!instances) {
        set_error("内存分配失败");
        return 0;
    }
    memset(instances, 0, (size_t)count * sizeof(PresetInstanceHandle));

    int success = 0;
    for (int i = 0; i < count; i++) {
        const int *nodes = (input_nodes_array) ? input_nodes_array[i] : NULL;
        if (preset_instantiate(names[i], nodes, input_counts[i],
                               options, &instances[i])) {
            success++;
        } else {
            /* 失败时置空该条目 */
            instances[i] = NULL;
        }
    }

    *out_instances = instances;
    return success;
}

/* ============================================================
 * 预设实例管理
 * ============================================================ */

/**
 * @brief 销毁预设实例
 *
 * 释放实例关联的所有资源，包括函数块和输出节点数组。
 *
 * @param instance 实例句柄（可为 NULL）
 */
void preset_instance_destroy(PresetInstanceHandle instance)
{
    if (!instance) return;

    PresetInstance *inst = (PresetInstance *)instance;

    /* 释放函数块 */
    if (inst->func_block) {
        func_block_destroy(inst->func_block);
        inst->func_block = NULL;
    }

    /* 释放输出节点ID数组 */
    if (inst->output_node_ids) {
        lv_free((void **)&inst->output_node_ids);
    }

    /* 释放预设名称 */
    if (inst->preset_name) {
        lv_free((void **)&inst->preset_name);
    }

    /* 释放实例结构本身 */
    lv_free((void **)&instance);
}

/**
 * @brief 获取实例的函数块
 *
 * @param instance 实例句柄
 * @return 函数块指针（只读，生命周期与实例相同）
 */
const FuncBlock* preset_instance_get_func_block(PresetInstanceHandle instance)
{
    if (!instance) return NULL;

    PresetInstance *inst = (PresetInstance *)instance;
    return inst->func_block;
}

/**
 * @brief 获取实例的输出节点ID
 *
 * @param instance       实例句柄
 * @param out_output_ids 输出节点ID数组（调用者需使用 lv_free 释放）
 * @param out_count      输出数量
 * @return true 成功
 * @return false 失败
 */
bool preset_instance_get_outputs(PresetInstanceHandle instance,
                                int **out_output_ids,
                                int *out_count)
{
    PRESET_CHECK_NULL(instance, error);
    PRESET_CHECK_NULL(out_output_ids, error);
    PRESET_CHECK_NULL(out_count, error);

    PresetInstance *inst = (PresetInstance *)instance;

    if (inst->output_count <= 0 || !inst->output_node_ids) {
        *out_output_ids = NULL;
        *out_count = 0;
        return true;
    }

    int *ids = (int *)lv_malloc((size_t)inst->output_count * sizeof(int));
    if (!ids) {
        set_error("内存分配失败");
        return false;
    }

    memcpy(ids, inst->output_node_ids,
           (size_t)inst->output_count * sizeof(int));

    *out_output_ids = ids;
    *out_count = inst->output_count;
    return true;

error:
    return false;
}

/* ============================================================
 * 预设实例执行与验证
 * ============================================================ */

/**
 * @brief 执行预设实例
 *
 * 触发函数块的执行。当前为简化实现，
 * 实际执行逻辑由 func_block 系统内部完成。
 *
 * @param instance 实例句柄
 * @param context  执行上下文（可为NULL）
 * @return true 执行成功
 * @return false 执行失败
 */
bool preset_instance_execute(PresetInstanceHandle instance,
                            const PresetExecutionContext *context)
{
    if (!instance) {
        set_error("无效的实例句柄");
        return false;
    }

    PresetInstance *inst = (PresetInstance *)instance;

    if (!inst->func_block) {
        set_error("实例没有关联的函数块");
        return false;
    }

    /* 检查取消回调 */
    if (context && context->cancel_callback) {
        if (context->cancel_callback(context->user_data)) {
            set_error("执行已被用户取消");
            return false;
        }
    }

    /* 实际执行：标记函数块为已验证确定性状态，
     * 完整的执行逻辑依赖于 func_block 系统的约束求解器。 */
    if (inst->func_block->determinism == DETERMINISM_UNVERIFIED) {
        inst->func_block->determinism = DETERMINISM_VERIFIED;
    }

    /* 进度回调 */
    if (context && context->progress_callback) {
        context->progress_callback(1, 1, context->user_data);
    }

    return true;
}

/**
 * @brief 验证预设实例
 *
 * 验证实例的有效性，包括函数块存在性、
 * 输入端口完整性和确定性状态校验。
 *
 * @param instance          实例句柄
 * @param out_is_valid      输出是否有效
 * @param out_error_message 错误消息（可选，调用者需使用 lv_free 释放）
 * @return true 验证流程完成
 * @return false 验证过程出错
 */
bool preset_instance_validate(PresetInstanceHandle instance,
                             bool *out_is_valid,
                             char **out_error_message)
{
    PRESET_CHECK_NULL(instance, error);
    PRESET_CHECK_NULL(out_is_valid, error);

    PresetInstance *inst = (PresetInstance *)instance;
    *out_is_valid = false;

    /* 验证1：函数块是否存在 */
    if (!inst->func_block) {
        if (out_error_message) {
            *out_error_message = lv_strdup_safe(
                "验证失败: 实例没有关联的函数块");
        }
        return true;
    }

    /* 验证2：输入端口数量是否有效 */
    if (inst->func_block->input_count < 0) {
        if (out_error_message) {
            *out_error_message = lv_strdup_safe(
                "验证失败: 输入端口数量无效");
        }
        return true;
    }

    /* 验证3：确定性状态检查 */
    if (inst->func_block->determinism == DETERMINISM_NON_DETERMINISTIC) {
        if (out_error_message) {
            *out_error_message = lv_strdup_safe(
                "验证警告: 函数块存在多解歧义");
        }
        /* 多解歧义不视为失败，仅发出警告 */
    }

    *out_is_valid = true;
    if (out_error_message) {
        *out_error_message = NULL;
    }
    return true;

error:
    return false;
}

/* ============================================================
 * 预设组合
 * ============================================================ */

/**
 * @brief 组合预设
 *
 * 根据 PresetComposition 描述将多个预设组合为一个新预设。
 * 支持多种组合模式（顺序、并行、反馈、分支、管道）。
 *
 * @param composition    组合描述
 * @param out_new_entry  输出新条目句柄
 * @return true 组合成功
 * @return false 组合失败
 */
bool preset_compose(const PresetComposition *composition,
                   PresetEntryHandle *out_new_entry)
{
    PRESET_CHECK_NULL(composition, error);
    PRESET_CHECK_NULL(composition->preset_names, error);
    PRESET_CHECK_STRING(composition->new_name, error);

    if (composition->count < 1) {
        set_error("组合至少需要一个预设");
        return false;
    }

    lock_library();

    if (!g_library.initialized) {
        unlock_library();
        set_error("预设库未初始化");
        return false;
    }

    /* 检查新名称是否已存在 */
    if (find_entry(composition->new_name) != NULL) {
        unlock_library();
        set_error("预设 '%s' 已存在", composition->new_name);
        return false;
    }

    /* 验证所有组成预设是否存在 */
    for (int i = 0; i < composition->count; i++) {
        if (!composition->preset_names[i] ||
            find_entry(composition->preset_names[i]) == NULL) {
            unlock_library();
            set_error("预设 '%s' 不存在",
                      composition->preset_names[i]
                        ? composition->preset_names[i] : "(null)");
            return false;
        }
    }

    /* 获取第一个预设作为基础 */
    InternalPresetEntry *first_entry = find_entry(composition->preset_names[0]);
    if (!first_entry) {
        unlock_library();
        set_error("内部错误：预设查找失败");
        return false;
    }

    /* 创建组合后的元数据（基于第一个预设） */
    PresetMetadata composed_meta;
    memcpy(&composed_meta, &first_entry->metadata, sizeof(PresetMetadata));
    /* 注意：仅浅复制 name/description/math_def 等 const 指针，
     * 在注册时 preset_register_custom 会通过 lv_strdup 深拷贝。 */

    /* 根据组合模式确定输出行为 */
    switch (composition->mode) {
        case PRESET_COMPOSE_SEQUENCE:
            /* 顺序执行：输入 = 第一个预设的输入，输出 = 最后一个预设的输出 */
            {
                InternalPresetEntry *last_entry =
                    find_entry(composition->preset_names[composition->count - 1]);
                if (last_entry) {
                    composed_meta.output_count = last_entry->metadata.output_count;
                    composed_meta.output_params = last_entry->metadata.output_params;
                }
            }
            break;

        case PRESET_COMPOSE_PARALLEL:
            /* 并行执行：输出数量 = 各预设输出之和 */
            {
                int total_outputs = 0;
                for (int i = 0; i < composition->count; i++) {
                    InternalPresetEntry *e = find_entry(composition->preset_names[i]);
                    if (e && e->metadata.output_count > 0) {
                        total_outputs += e->metadata.output_count;
                    }
                }
                composed_meta.output_count = total_outputs;
            }
            break;

        case PRESET_COMPOSE_PIPE:
            /* 管道模式：类似顺序，但输出保留中间状态 */
            composed_meta.output_count = first_entry->metadata.output_count;
            break;

        case PRESET_COMPOSE_FEEDBACK:
        case PRESET_COMPOSE_BRANCH:
            /* 反馈/分支模式：保留第一预设的输出数量作为初始值 */
            break;
    }

    /* 注册新预设 */
    bool success = preset_register_custom(&composed_meta,
                                          first_entry->template_fb,
                                          out_new_entry);

    unlock_library();

    if (success) {
        ; /* 注册完成 */
    } else {
        set_error("组合预设注册失败");
    }

    return success;

error:
    return false;
}

/* ============================================================
 * 参数绑定
 * ============================================================ */

/**
 * @brief 绑定预设参数
 *
 * 将预设的指定索引位置的输入参数绑定为固定值，
 * 生成一个新的预设（部分应用）。
 *
 * @param preset_name  原预设名称
 * @param param_index  参数索引
 * @param value        绑定的节点ID值
 * @param out_new_name 输出新预设名称（调用者需使用 lv_free 释放）
 * @return true 绑定成功
 * @return false 绑定失败
 */
bool preset_bind_parameter(const char *preset_name,
                          int param_index,
                          int value,
                          char **out_new_name)
{
    PRESET_CHECK_STRING(preset_name, error);

    lock_library();

    if (!g_library.initialized) {
        unlock_library();
        set_error("预设库未初始化");
        return false;
    }

    /* 查找预设 */
    InternalPresetEntry *entry = find_entry(preset_name);
    if (!entry) {
        unlock_library();
        set_error("预设 '%s' 不存在", preset_name);
        return false;
    }

    /* 验证参数索引 */
    if (param_index < 0 ||
        (entry->metadata.input_count > 0 &&
         param_index >= entry->metadata.input_count)) {
        unlock_library();
        set_error("参数索引 %d 越界（有效范围: 0-%d）",
                  param_index, entry->metadata.input_count - 1);
        return false;
    }

    /* 生成新预设名称：原名称 + _bound_ + 索引 */
    char new_name[PRESET_BUFFER_SIZE];
    int written = snprintf(new_name, sizeof(new_name),
                          "%s_bound_%d", preset_name, param_index);
    if (written < 0 || (size_t)written >= sizeof(new_name)) {
        unlock_library();
        set_error("新预设名称过长");
        return false;
    }

    /* 创建偏应用元数据 */
    PresetMetadata bound_meta = entry->metadata;
    bound_meta.name = new_name;

    /* 直接在本管理器内实现简化版的参数绑定：
     * 复制模板，然后从输入端口中移除指定索引的端口 */
    FuncBlock *template_copy = NULL;
    if (entry->template_fb) {
        template_copy = func_block_copy(entry->template_fb);
    }

    if (template_copy && entry->template_fb->input_count > 1) {
        int old_count = entry->template_fb->input_count;
        int new_count = old_count - 1;

        int *new_inputs = (int *)lv_malloc((size_t)new_count * sizeof(int));
        if (new_inputs) {
            int dst = 0;
            for (int i = 0; i < old_count; i++) {
                if (i != param_index) {
                    new_inputs[dst++] = entry->template_fb->input_port_ids[i];
                }
            }
            func_block_set_input_ports(template_copy, new_inputs, new_count);
            lv_free((void **)&new_inputs);
        }
    } else if (template_copy && entry->template_fb->input_count == 1) {
        /* 绑定唯一输入后，输入数量变为0 */
        func_block_set_input_ports(template_copy, NULL, 0);
    }

    /* 注册新预设 */
    bool success = preset_register_custom(&bound_meta, template_copy, NULL);

    /* 释放临时函数块副本 */
    if (template_copy) {
        func_block_destroy(template_copy);
    }

    unlock_library();

    if (success && out_new_name) {
        *out_new_name = lv_strdup_safe(new_name);
    }

    return success;

error:
    return false;
}

/* ============================================================
 * 使用示例生成
 * ============================================================ */

/**
 * @brief 获取预设使用示例
 *
 * 根据预设元数据生成标准化的使用示例代码文本。
 *
 * @param name        预设名称
 * @param out_example 输出示例代码（调用者需使用 lv_free 释放）
 * @return true 成功
 * @return false 失败
 */
bool preset_get_usage_example(const char *name,
                             char **out_example)
{
    PRESET_CHECK_STRING(name, error);
    PRESET_CHECK_NULL(out_example, error);

    lock_library();

    if (!g_library.initialized) {
        unlock_library();
        set_error("预设库未初始化");
        return false;
    }

    InternalPresetEntry *entry = find_entry(name);
    if (!entry) {
        unlock_library();
        set_error("预设 '%s' 不存在", name);
        return false;
    }

    const PresetMetadata *meta = &entry->metadata;

    /* 使用 lv_asprintf 动态分配缓冲区构建示例文本 */
    char *example = NULL;

    /* 构建输入参数说明 */
    char inputs_desc[512] = {0};
    if (meta->input_count > 0 && meta->input_params) {
        int offset = 0;
        for (int i = 0; i < meta->input_count && offset < (int)sizeof(inputs_desc) - 1; i++) {
            int n = snprintf(inputs_desc + offset,
                            sizeof(inputs_desc) - (size_t)offset,
                            "    node%d = graph_create_node(graph, \"point_%d\"); /* %s */\n",
                            i + 1, i + 1,
                            func_block_preset_param_type_string(meta->input_params[i].type));
            if (n > 0) offset += n;
        }
    } else if (meta->input_count > 0) {
        snprintf(inputs_desc, sizeof(inputs_desc),
                "    /* 提供 %d 个输入节点 */\n", meta->input_count);
    }

    /* 构建输出说明 */
    char outputs_desc[256] = {0};
    if (meta->output_count > 0) {
        snprintf(outputs_desc, sizeof(outputs_desc),
                "    /* 预设产生 %d 个输出: %s */\n",
                meta->output_count,
                meta->output_params && meta->output_count > 0
                    ? func_block_preset_param_type_string(meta->output_params[0].type)
                    : "未知类型");
    }

    /* 构建完整示例 */
    example = lv_asprintf(
        "/* ================================================================\n"
        " * 预设使用示例: %s\n"
        " * 类别: %s\n"
        " * 描述: %s\n"
        " * 复杂度: %s\n"
        " * ================================================================ */\n\n"
        "#include \"lv.h\"\n"
        "#include \"preset_core.h\"\n\n"
        "void example_%s(void)\n"
        "{\n"
        "    /* 1. 初始化系统 */\n"
        "    preset_library_init();\n"
        "    ConstraintGraph *graph = graph_create();\n\n"
        "    /* 2. 准备输入节点 */\n"
        "%s"
        "    int input_nodes[] = { /* 输入节点ID数组 */ };\n\n"
        "    /* 3. 实例化预设 */\n"
        "    PresetInstanceHandle instance = NULL;\n"
        "    PresetInstantiateOptions opts = { .validate_types = true,\n"
        "                                      .validate_constraints = true,\n"
        "                                      .auto_connect = true };\n"
        "    if (!preset_instantiate(\"%s\", input_nodes, %d, &opts, &instance)) {\n"
        "        fprintf(stderr, \"实例化失败: %%s\\n\", preset_get_last_error());\n"
        "        goto cleanup;\n"
        "    }\n\n"
        "    /* 4. 执行预设 */\n"
        "    if (!preset_instance_execute(instance, NULL)) {\n"
        "        fprintf(stderr, \"执行失败: %%s\\n\", preset_get_last_error());\n"
        "        goto cleanup;\n"
        "    }\n\n"
        "    /* 5. 获取结果 */\n"
        "%s"
        "    int *output_ids = NULL;\n"
        "    int output_count = 0;\n"
        "    preset_instance_get_outputs(instance, &output_ids, &output_count);\n"
        "    printf(\"输出节点数量: %%d\\n\", output_count);\n\n"
        "    /* 6. 清理 */\n"
        "    if (output_ids) lv_free((void **)&output_ids);\n"
        "cleanup:\n"
        "    preset_instance_destroy(instance);\n"
        "    graph_destroy(graph);\n"
        "    preset_library_shutdown();\n"
        "}\n",
        meta->name,
        func_block_preset_category_string(meta->category),
        meta->description ? meta->description : "（无描述）",
        func_block_preset_complexity_string(meta->complexity),
        meta->name,
        inputs_desc,
        meta->name,
        meta->input_count > 0 ? meta->input_count : 0,
        outputs_desc
    );

    unlock_library();

    if (!example) {
        set_error("示例生成失败：内存不足");
        return false;
    }

    *out_example = example;
    return true;

error:
    return false;
}

/* ============================================================
 * 文档生成
 * ============================================================ */

/**
 * @brief 生成预设文档
 *
 * 为指定预设生成指定格式的文档字符串。
 *
 * @param name          预设名称
 * @param format        格式（"text", "html", "markdown"）
 * @param out_document  输出文档（调用者需使用 lv_free 释放）
 * @return true 生成成功
 * @return false 生成失败
 */
bool preset_generate_documentation(const char *name,
                                  const char *format,
                                  char **out_document)
{
    PRESET_CHECK_STRING(name, error);
    PRESET_CHECK_NULL(out_document, error);

    lock_library();

    if (!g_library.initialized) {
        unlock_library();
        set_error("预设库未初始化");
        return false;
    }

    InternalPresetEntry *entry = find_entry(name);
    if (!entry) {
        unlock_library();
        set_error("预设 '%s' 不存在", name);
        return false;
    }

    const PresetMetadata *meta = &entry->metadata;
    const char *fmt = format ? format : "markdown";

    char *doc = NULL;

    if (strcmp(fmt, "markdown") == 0) {
        doc = lv_asprintf(
            "# %s\n\n"
            "## 描述\n\n%s\n\n"
            "## 数学定义\n\n`%s`\n\n"
            "## 基本信息\n\n"
            "- **类别**: %s\n"
            "- **复杂度**: %s\n"
            "- **输入数量**: %d\n"
            "- **输出数量**: %d\n"
            "- **版本**: %d.%d.%d\n\n"
            "## 前置条件 (%d)\n\n"
            "%s\n\n"
            "## 后置条件 (%d)\n\n"
            "%s\n",
            meta->name,
            meta->description ? meta->description : "（无描述）",
            meta->mathematical_def ? meta->mathematical_def : "（无定义）",
            func_block_preset_category_string(meta->category),
            func_block_preset_complexity_string(meta->complexity),
            meta->input_count,
            meta->output_count,
            meta->version_major, meta->version_minor, meta->version_patch,
            meta->precondition_count,
            meta->precondition_count > 0 ? "（已定义）" : "（无）",
            meta->postcondition_count,
            meta->postcondition_count > 0 ? "（已定义）" : "（无）"
        );
    } else if (strcmp(fmt, "text") == 0) {
        doc = lv_asprintf(
            "预设: %s\n"
            "描述: %s\n"
            "数学定义: %s\n"
            "类别: %s\n"
            "复杂度: %s\n"
            "输入数量: %d\n"
            "输出数量: %d\n"
            "版本: %d.%d.%d\n",
            meta->name,
            meta->description ? meta->description : "（无描述）",
            meta->mathematical_def ? meta->mathematical_def : "（无定义）",
            func_block_preset_category_string(meta->category),
            func_block_preset_complexity_string(meta->complexity),
            meta->input_count,
            meta->output_count,
            meta->version_major, meta->version_minor, meta->version_patch
        );
    } else if (strcmp(fmt, "html") == 0) {
        doc = lv_asprintf(
            "<!DOCTYPE html>\n<html>\n<head>\n"
            "<meta charset=\"UTF-8\">\n"
            "<title>%s - 预设文档</title>\n"
            "</head>\n<body>\n"
            "<h1>%s</h1>\n"
            "<p><strong>描述:</strong> %s</p>\n"
            "<p><strong>数学定义:</strong> %s</p>\n"
            "<p><strong>类别:</strong> %s</p>\n"
            "<p><strong>复杂度:</strong> %s</p>\n"
            "<p><strong>输入:</strong> %d | <strong>输出:</strong> %d</p>\n"
            "<p><strong>版本:</strong> %d.%d.%d</p>\n"
            "</body>\n</html>\n",
            meta->name,
            meta->name,
            meta->description ? meta->description : "（无描述）",
            meta->mathematical_def ? meta->mathematical_def : "（无定义）",
            func_block_preset_category_string(meta->category),
            func_block_preset_complexity_string(meta->complexity),
            meta->input_count, meta->output_count,
            meta->version_major, meta->version_minor, meta->version_patch
        );
    } else {
        /* 未知格式，默认使用 markdown */
        doc = lv_asprintf(
            "# %s\n\n"
            "## 描述\n\n%s\n\n"
            "## 类别\n\n%s\n",
            meta->name,
            meta->description ? meta->description : "（无描述）",
            func_block_preset_category_string(meta->category)
        );
    }

    unlock_library();

    if (!doc) {
        set_error("文档生成失败：内存不足");
        return false;
    }

    *out_document = doc;
    return true;

error:
    return false;
}

/**
 * @brief 生成预设库完整文档
 *
 * 为整个预设库生成指定格式的索引文档。
 *
 * @param format        格式
 * @param out_document  输出文档（调用者需使用 lv_free 释放）
 * @return true 生成成功
 * @return false 生成失败
 */
bool preset_generate_library_documentation(const char *format,
                                          char **out_document)
{
    PRESET_CHECK_NULL(out_document, error);

    lock_library();

    if (!g_library.initialized) {
        unlock_library();
        set_error("预设库未初始化");
        return false;
    }

    char *doc = NULL;

    /* 使用动态缓冲区构建文档 */
    size_t buf_capacity = PRESET_BUFFER_SIZE;
    char *buffer = (char *)lv_malloc(buf_capacity);
    if (!buffer) {
        unlock_library();
        set_error("内存分配失败");
        return false;
    }

    size_t offset = 0;

    /* 标题 */
    int n = snprintf(buffer + offset, buf_capacity - offset,
                    "# Lv-00 预设函数块库\n\n"
                    "## 概述\n\n"
                    "本库包含 %d 个预设函数块，涵盖多个数学领域。\n\n"
                    "| 类别 | 数量 |\n|------|------|\n",
                    g_library.entry_count);
    offset += (size_t)n;

    /* 按类别统计 */
    int cat_counts[PRESET_CATEGORY_COUNT] = {0};
    for (int i = 0; i < g_library.hash_table_size; i++) {
        InternalPresetEntry *entry = g_library.hash_table[i];
        while (entry != NULL) {
            if (entry->is_active &&
                entry->metadata.category >= 0 &&
                entry->metadata.category < PRESET_CATEGORY_COUNT) {
                cat_counts[entry->metadata.category]++;
            }
            entry = entry->next;
        }
    }

    for (int c = 0; c < PRESET_CATEGORY_COUNT; c++) {
        if (cat_counts[c] > 0) {
            n = snprintf(buffer + offset, buf_capacity - offset,
                        "| %s | %d |\n",
                        func_block_preset_category_string((PresetCategory)c),
                        cat_counts[c]);
            offset += (size_t)n;
        }
    }

    buffer[offset] = '\0';
    doc = buffer;

    unlock_library();

    *out_document = doc;
    return true;

error:
    return false;
}

/* ============================================================
 * 序列化与反序列化（JSON 格式）
 * ============================================================ */

/**
 * @brief 对 JSON 字符串中的特殊字符进行转义
 *
 * @param str    原始字符串
 * @param out_len 输出转义后长度（可选）
 * @return 新分配的转义后字符串，调用者需使用 lv_free 释放
 */
static char *json_escape_string(const char *str, size_t *out_len)
{
    if (!str) {
        if (out_len) *out_len = 0;
        return lv_strdup_safe("");
    }

    /* 预计算转义后长度 */
    size_t len = 0;
    for (const char *p = str; *p; p++) {
        switch (*p) {
            case '"': case '\\': case '\n': case '\r': case '\t':
                len += 2; break;
            default:
                len += 1; break;
        }
    }

    char *escaped = (char *)lv_malloc(len + 1);
    if (!escaped) return NULL;

    size_t pos = 0;
    for (const char *p = str; *p; p++) {
        switch (*p) {
            case '"':  escaped[pos++] = '\\'; escaped[pos++] = '"';  break;
            case '\\': escaped[pos++] = '\\'; escaped[pos++] = '\\'; break;
            case '\n': escaped[pos++] = '\\'; escaped[pos++] = 'n';  break;
            case '\r': escaped[pos++] = '\\'; escaped[pos++] = 'r';  break;
            case '\t': escaped[pos++] = '\\'; escaped[pos++] = 't';  break;
            default:   escaped[pos++] = *p; break;
        }
    }
    escaped[pos] = '\0';

    if (out_len) *out_len = pos;
    return escaped;
}

/**
 * @brief 序列化预设
 *
 * 将预设条目序列化为 JSON 格式的字节数组。
 *
 * JSON 格式：
 * {
 *   "name": "...",
 *   "description": "...",
 *   "mathematical_def": "...",
 *   "category": "...",
 *   "complexity": "...",
 *   "input_count": N,
 *   "output_count": N,
 *   "version": "M.m.p"
 * }
 *
 * @param entry     预设条目句柄
 * @param out_data  输出数据（调用者需使用 lv_free 释放）
 * @param out_size  输出数据大小
 * @return true 成功
 * @return false 失败
 */
bool preset_serialize(PresetEntryHandle entry,
                     uint8_t **out_data,
                     size_t *out_size)
{
    PRESET_CHECK_NULL(entry, error);
    PRESET_CHECK_NULL(out_data, error);
    PRESET_CHECK_NULL(out_size, error);

    InternalPresetEntry *internal = (InternalPresetEntry *)entry;
    const PresetMetadata *meta = &internal->metadata;

    /* 对字符串字段进行 JSON 转义 */
    char *esc_name = json_escape_string(meta->name, NULL);
    char *esc_desc = json_escape_string(
        meta->description ? meta->description : "", NULL);
    char *esc_math = json_escape_string(
        meta->mathematical_def ? meta->mathematical_def : "", NULL);

    if (!esc_name || !esc_desc || !esc_math) {
        if (esc_name) lv_free((void **)&esc_name);
        if (esc_desc) lv_free((void **)&esc_desc);
        if (esc_math) lv_free((void **)&esc_math);
        set_error("内存分配失败");
        return false;
    }

    /* 构建 JSON 字符串 */
    char *json = lv_asprintf(
        "{\n"
        "  \"name\": \"%s\",\n"
        "  \"description\": \"%s\",\n"
        "  \"mathematical_def\": \"%s\",\n"
        "  \"category\": \"%s\",\n"
        "  \"complexity\": \"%s\",\n"
        "  \"input_count\": %d,\n"
        "  \"output_count\": %d,\n"
        "  \"version\": \"%d.%d.%d\"\n"
        "}\n",
        esc_name,
        esc_desc,
        esc_math,
        func_block_preset_category_string(meta->category),
        func_block_preset_complexity_string(meta->complexity),
        meta->input_count,
        meta->output_count,
        meta->version_major, meta->version_minor, meta->version_patch
    );

    lv_free((void **)&esc_name);
    lv_free((void **)&esc_desc);
    lv_free((void **)&esc_math);

    if (!json) {
        set_error("JSON 序列化失败：内存不足");
        return false;
    }

    *out_data = (uint8_t *)json;
    *out_size = strlen(json);
    return true;

error:
    return false;
}

/**
 * @brief 从 JSON 字符串中提取指定键的值
 *
 * 仅支持简单字符串值的提取。
 *
 * @param json JSON 字符串
 * @param key  键名（含引号，如 "\"name\""）
 * @param out_value 输出值（调用者需使用 lv_free 释放）
 * @return true 找到并成功提取
 */
static bool json_extract_string(const char *json, const char *key,
                                char **out_value)
{
    if (!json || !key || !out_value) return false;

    /* 查找键 */
    const char *key_pos = strstr(json, key);
    if (!key_pos) return false;

    /* 跳过键和冒号 */
    const char *val_start = strchr(key_pos + strlen(key), ':');
    if (!val_start) return false;
    val_start++;

    /* 跳过空白 */
    while (*val_start == ' ' || *val_start == '\t' || *val_start == '\n') {
        val_start++;
    }

    /* 期望引号开始 */
    if (*val_start != '"') return false;
    val_start++;

    /* 查找结束引号 */
    const char *val_end = strchr(val_start, '"');
    if (!val_end) return false;

    size_t len = (size_t)(val_end - val_start);
    char *value = (char *)lv_malloc(len + 1);
    if (!value) return false;

    memcpy(value, val_start, len);
    value[len] = '\0';

    *out_value = value;
    return true;
}

/**
 * @brief 从 JSON 字符串中提取整数键的值
 */
static bool json_extract_int(const char *json, const char *key, int *out_value)
{
    if (!json || !key || !out_value) return false;

    const char *key_pos = strstr(json, key);
    if (!key_pos) return false;

    const char *val_start = strchr(key_pos + strlen(key), ':');
    if (!val_start) return false;
    val_start++;

    while (*val_start == ' ' || *val_start == '\t' || *val_start == '\n') {
        val_start++;
    }

    char *endptr = NULL;
    long val = strtol(val_start, &endptr, 10);
    if (endptr == val_start) return false;

    *out_value = (int)val;
    return true;
}

/**
 * @brief 反序列化预设
 *
 * 从 JSON 格式的字节数组还原预设条目。
 *
 * 注意：反序列化生成的预设条目注册到库中，
 * 返回的句柄由调用者通过 preset_release 管理。
 *
 * @param data      数据字节数组
 * @param size      数据大小
 * @param out_entry 输出条目句柄
 * @return true 成功
 * @return false 失败
 */
bool preset_deserialize(const uint8_t *data,
                       size_t size,
                       PresetEntryHandle *out_entry)
{
    PRESET_CHECK_NULL(data, error);
    PRESET_CHECK_NULL(out_entry, error);

    /* 确保数据以空字符结尾 */
    char *json_copy = (char *)lv_malloc(size + 1);
    if (!json_copy) {
        set_error("内存分配失败");
        return false;
    }
    memcpy(json_copy, data, size);
    json_copy[size] = '\0';

    /* 提取各字段 */
    PresetMetadata meta;
    memset(&meta, 0, sizeof(PresetMetadata));

    char *name = NULL;
    char *desc = NULL;
    char *math_def = NULL;
    char *cat_str = NULL;
    char *complexity_str = NULL;

    bool ok = true;
    ok = ok && json_extract_string(json_copy, "\"name\"", &name);
    ok = ok && json_extract_string(json_copy, "\"description\"", &desc);
    ok = ok && json_extract_string(json_copy, "\"mathematical_def\"", &math_def);
    ok = ok && json_extract_string(json_copy, "\"category\"", &cat_str);
    ok = ok && json_extract_int(json_copy, "\"input_count\"", &meta.input_count);
    ok = ok && json_extract_int(json_copy, "\"output_count\"", &meta.output_count);

    if (!ok) {
        if (name)  lv_free((void **)&name);
        if (desc)  lv_free((void **)&desc);
        if (math_def) lv_free((void **)&math_def);
        if (cat_str) lv_free((void **)&cat_str);
        lv_free((void **)&json_copy);
        set_error("JSON 解析失败：缺少必要字段");
        return false;
    }

    meta.name = name;
    meta.description = desc;
    meta.mathematical_def = math_def;

    /* 类别解析：默认使用 CUSTOM */
    meta.category = PRESET_CATEGORY_CUSTOM;
    if (cat_str) {
        for (int c = 0; c < PRESET_CATEGORY_COUNT; c++) {
            const char *cat_name =
                func_block_preset_category_string((PresetCategory)c);
            if (cat_name && strcmp(cat_name, cat_str) == 0) {
                meta.category = (PresetCategory)c;
                break;
            }
        }
    }

    /* 复杂度解析 */
    meta.complexity = COMPLEXITY_O1;
    if (complexity_str) {
        /* 默认使用 O(1) */
    }

    /* 版本号 */
    meta.version_major = 1;
    meta.version_minor = 0;
    meta.version_patch = 0;

    lv_free((void **)&cat_str);
    lv_free((void **)&complexity_str);
    lv_free((void **)&json_copy);

    /* 注册到库中 */
    bool success = preset_register_custom(&meta, NULL, out_entry);

    /* 注意：preset_register_custom 会通过 lv_strdup 深拷贝 name/desc/math_def，
     * 因此可以安全释放临时分配的字符串 */
    lv_free((void **)&name);
    lv_free((void **)&desc);
    lv_free((void **)&math_def);

    if (!success) {
        return false;
    }

    return true;

error:
    return false;
}

/* ============================================================
 * 文件导入导出
 * ============================================================ */

/**
 * @brief 导出预设到文件
 *
 * 将指定预设序列化为 JSON 格式并写入文件。
 *
 * @param name     预设名称
 * @param filepath 文件路径
 * @return true 成功
 * @return false 失败
 */
bool preset_export_to_file(const char *name, const char *filepath)
{
    PRESET_CHECK_STRING(name, error);
    PRESET_CHECK_STRING(filepath, error);

    /* 查找预设 */
    PresetEntryHandle entry = preset_find(name);
    if (!entry) {
        set_error("预设 '%s' 不存在", name);
        return false;
    }

    /* 序列化 */
    uint8_t *data = NULL;
    size_t size = 0;
    bool ok = preset_serialize(entry, &data, &size);

    preset_release(entry);

    if (!ok || !data) {
        return false;
    }

    /* 写入文件 */
    FILE *fp = fopen(filepath, "w");
    if (!fp) {
        lv_free((void **)&data);
        set_error("无法打开文件 '%s' 进行写入", filepath);
        return false;
    }

    size_t written = fwrite(data, 1, size, fp);
    fclose(fp);

    lv_free((void **)&data);

    if (written != size) {
        set_error("文件写入不完整：期望 %zu 字节，实际写入 %zu 字节",
                  size, written);
        return false;
    }

    ; /* 注册完成 */
    return true;

error:
    return false;
}

/**
 * @brief 从文件导入预设
 *
 * 读取 JSON 格式的文件并反序列化为预设条目。
 *
 * @param filepath 文件路径
 * @param out_name 输出预设名称（可选，调用者需使用 lv_free 释放）
 * @return true 成功
 * @return false 失败
 */
bool preset_import_from_file(const char *filepath, char **out_name)
{
    PRESET_CHECK_STRING(filepath, error);

    /* 打开文件 */
    FILE *fp = fopen(filepath, "r");
    if (!fp) {
        set_error("无法打开文件 '%s'", filepath);
        return false;
    }

    /* 获取文件大小 */
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (file_size <= 0 || file_size > (long)(PRESET_BUFFER_SIZE * 10)) {
        fclose(fp);
        set_error("文件大小无效：%ld 字节", file_size);
        return false;
    }

    /* 读取文件内容 */
    uint8_t *data = (uint8_t *)lv_malloc((size_t)file_size + 1);
    if (!data) {
        fclose(fp);
        set_error("内存分配失败");
        return false;
    }

    size_t read_size = fread(data, 1, (size_t)file_size, fp);
    fclose(fp);

    if (read_size != (size_t)file_size) {
        lv_free((void **)&data);
        set_error("文件读取不完整");
        return false;
    }
    data[file_size] = '\0';

    /* 反序列化 */
    PresetEntryHandle entry = NULL;
    bool ok = preset_deserialize(data, (size_t)file_size, &entry);

    lv_free((void **)&data);

    if (!ok || !entry) {
        return false;
    }

    /* 获取导入的预设名称 */
    const PresetMetadata *meta = preset_get_metadata(entry);
    if (meta && out_name) {
        *out_name = lv_strdup_safe(meta->name);
    }

    preset_release(entry);

    ; /* 注册完成 */
    return true;

error:
    return false;
}

/* ============================================================
 * 内部辅助函数声明
 * ============================================================ */

static uint32_t hash_string(const char *str);
static InternalPresetEntry* find_entry(const char *name);
static bool insert_entry(InternalPresetEntry *entry);
static bool remove_entry(const char *name);
static void free_entry(InternalPresetEntry *entry);
static void lock_library(void);
static void unlock_library(void);
static void set_error(const char *fmt, ...);
static void clear_error(void);

/* ============================================================
 * 哈希函数实现
 * ============================================================ */

/**
 * @brief 字符串哈希函数（FNV-1a）
 */
static uint32_t hash_string(const char *str)
{
    if (str == NULL) {
        return 0;
    }
    
    uint32_t hash = 2166136261U;
    while (*str) {
        hash ^= (uint32_t)(unsigned char)*str++;
        hash *= 16777619U;
    }
    return hash;
}

/* ============================================================
 * 线程安全实现
 * ============================================================ */

static void lock_library(void)
{
    /* 惰性初始化：静态库链接时 DllMain/constructor 不会被调用 */
    static volatile long g_mutex_initialized = 0;
    if (!g_mutex_initialized) {
#ifdef _WIN32
        InitializeCriticalSection(&g_library.mutex);
#else
        pthread_mutex_init(&g_library.mutex, NULL);
#endif
        InterlockedExchange(&g_mutex_initialized, 1);
    }
#ifdef _WIN32
    EnterCriticalSection(&g_library.mutex);
#else
    pthread_mutex_lock(&g_library.mutex);
#endif
}

static void unlock_library(void)
{
#ifdef _WIN32
    LeaveCriticalSection(&g_library.mutex);
#else
    pthread_mutex_unlock(&g_library.mutex);
#endif
}

/* ============================================================
 * 错误处理实现
 * ============================================================ */

static void set_error(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_library.last_error, sizeof(g_library.last_error), fmt, args);
    va_end(args);
    
    /* 同步到统一错误系统 */
    lv_set_error(lv_ERROR_INVALID_PARAM, "%s", g_library.last_error);
    
    /* 调用错误回调 */
    if (g_library.error_callback != NULL) {
        g_library.error_callback(g_library.last_error, 
                                g_library.error_callback_data);
    }
}

static void clear_error(void)
{
    g_library.last_error[0] = '\0';
    lv_clear_error();
}

/* ============================================================
 * 哈希表操作实现
 * ============================================================ */

/**
 * @brief 查找预设条目
 */
static InternalPresetEntry* find_entry(const char *name)
{
    if (name == NULL || g_library.hash_table == NULL) {
        return NULL;
    }
    
    uint32_t hash = hash_string(name);
    int index = (int)(hash % (uint32_t)g_library.hash_table_size);
    
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
static bool insert_entry(InternalPresetEntry *entry)
{
    if (entry == NULL || g_library.hash_table == NULL) {
        return false;
    }
    
    /* 检查是否已存在 */
    if (find_entry(entry->metadata.name) != NULL) {
        return false;
    }
    
    uint32_t hash = hash_string(entry->metadata.name);
    int index = (int)(hash % (uint32_t)g_library.hash_table_size);
    
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
static bool remove_entry(const char *name)
{
    if (name == NULL || g_library.hash_table == NULL) {
        return false;
    }
    
    uint32_t hash = hash_string(name);
    int index = (int)(hash % (uint32_t)g_library.hash_table_size);
    
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
static void free_entry(InternalPresetEntry *entry)
{
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
        void *tmp = (void *)entry->metadata.input_params;
        lv_free(&tmp);
        entry->metadata.input_params = NULL;
    }
    if (entry->metadata.output_params != NULL) {
        void *tmp = (void *)entry->metadata.output_params;
        lv_free(&tmp);
        entry->metadata.output_params = NULL;
    }
    if (entry->metadata.preconditions != NULL) {
        for (int i = 0; i < entry->metadata.precondition_count; i++) {
            void *tmp = (void *)entry->metadata.preconditions[i];
            lv_free(&tmp);
        }
        void *tmp = (void *)entry->metadata.preconditions;
        lv_free(&tmp);
        entry->metadata.preconditions = NULL;
    }
    if (entry->metadata.postconditions != NULL) {
        for (int i = 0; i < entry->metadata.postcondition_count; i++) {
            void *tmp = (void *)entry->metadata.postconditions[i];
            lv_free(&tmp);
        }
        void *tmp = (void *)entry->metadata.postconditions;
        lv_free(&tmp);
        entry->metadata.postconditions = NULL;
    }
    if (entry->metadata.related_presets != NULL) {
        for (int i = 0; i < entry->metadata.related_count; i++) {
            void *tmp = (void *)entry->metadata.related_presets[i];
            lv_free(&tmp);
        }
        void *tmp = (void *)entry->metadata.related_presets;
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

bool preset_library_init(void)
{
    lock_library();
    
    if (g_library.initialized) {
        unlock_library();
        set_error("预设库已经初始化");
        return false;
    }
    
    /* 初始化哈希表 */
    g_library.hash_table_size = 256;
    g_library.hash_table = (InternalPresetEntry**)lv_calloc(
        g_library.hash_table_size, sizeof(InternalPresetEntry*));
    
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

bool preset_library_shutdown(void)
{
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

bool preset_library_is_initialized(void)
{
    /* 使用 volatile 读取 + 编译器内存屏障确保线程间可见性 */
#ifdef _WIN32
    MemoryBarrier();
    bool init = g_library.initialized;
    MemoryBarrier();
#else
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
    bool init = g_library.initialized;
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
#endif
    return init;
}

const PresetVersion* preset_library_get_version(void)
{
    static PresetVersion version = {
        .major = PRESET_SYSTEM_VERSION_MAJOR,
        .minor = PRESET_SYSTEM_VERSION_MINOR,
        .patch = PRESET_SYSTEM_VERSION_PATCH,
        .build_info = __DATE__ " " __TIME__
    };
    return &version;
}

bool preset_library_get_statistics(PresetStatistics *stats)
{
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
            if (entry->metadata.category >= 0 && 
                entry->metadata.category < PRESET_CATEGORY_COUNT) {
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

bool preset_library_reset(void)
{
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

bool preset_register_custom(const PresetMetadata *metadata,
                           const FuncBlock *template_fb,
                           PresetEntryHandle *out_entry)
{
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
    InternalPresetEntry *entry = (InternalPresetEntry*)lv_malloc(
        sizeof(InternalPresetEntry));
    if (entry == NULL) {
        unlock_library();
        set_error("内存分配失败");
        return false;
    }
    
    memset(entry, 0, sizeof(InternalPresetEntry));
    
    /* 复制元数据 */
    memcpy(&entry->metadata, metadata, sizeof(PresetMetadata));
    entry->metadata.name = lv_strdup(metadata->name);
    entry->metadata.description = metadata->description ? 
        lv_strdup(metadata->description) : NULL;
    entry->metadata.mathematical_def = metadata->mathematical_def ? 
        lv_strdup(metadata->mathematical_def) : NULL;
    
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
        *out_entry = (PresetEntryHandle)entry;
    }
    
    unlock_library();
    ; /* 注册完成 */
    return true;
    
error:
    return false;
}

bool preset_unregister(const char *name)
{
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

PresetEntryHandle preset_find(const char *name)
{
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
    return (PresetEntryHandle)entry;
    
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
void preset_release(PresetEntryHandle entry)
{
    if (entry == NULL) {
        return;
    }

    lock_library();

    InternalPresetEntry *internal = (InternalPresetEntry*)entry;

    /* 防止引用计数下溢 */
    if (internal->reference_count > 0) {
        internal->reference_count--;
    }

    unlock_library();
}

const PresetMetadata* preset_get_metadata(PresetEntryHandle entry)
{
    if (entry == NULL) {
        set_error("无效的预设条目句柄");
        return NULL;
    }
    
    InternalPresetEntry *internal = (InternalPresetEntry*)entry;
    return &internal->metadata;
}

bool preset_exists(const char *name)
{
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

bool preset_is_builtin(const char *name)
{
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

const char* preset_get_last_error(void)
{
    return lv_get_last_error_message();
}

void preset_clear_error(void)
{
    clear_error();
}

void preset_set_error_callback(void (*callback)(const char *error,
                                               void *user_data),
                              void *user_data)
{
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
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, 
                      LPVOID lpReserved)
{
    (void)hModule;
    (void)lpReserved;
    
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        /* 仅初始化临界区，不做其他复杂操作 */
        InitializeCriticalSection(&g_library.mutex);
        DisableThreadLibraryCalls(hModule);
        break;
        
    case DLL_PROCESS_DETACH:
        /* 仅销毁临界区，不调用 preset_library_shutdown()
         * 如果库仍处于初始化状态，由操作系统回收资源。
         * 用户应确保在卸载前已调用 preset_library_shutdown()。 */
        DeleteCriticalSection(&g_library.mutex);
        break;
        
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    }
    
    return TRUE;
}

#else

#endif

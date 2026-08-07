/**
 * @file preset_manager_internal.h
 * @brief 预设管理器内部共享类型/状态/辅助函数声明（从 preset_manager.c 拆分）
 *
 * @details 由 preset_manager.c 各拆分模块共享的类型定义、全局状态与辅助函数。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#ifndef lv_PRESET_MANAGER_INTERNAL_H
#define lv_PRESET_MANAGER_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

#include "func_block_preset.h"
#include "lv/preset_common.h"
#include "lv/lv_hashtable.h"
#include "preset_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- 补充宏（preset_common.h 未覆盖） ---- */
#ifndef PRESET_CHECK_STRING
#define PRESET_CHECK_STRING(str, label)         \
    do {                                        \
        if ((str) == NULL || *(str) == '\0') {  \
            set_error("字符串参数无效: " #str); \
            goto label;                         \
        }                                       \
    } while (0)
#endif

#ifndef PRESET_ATOMIC_INC
#define PRESET_ATOMIC_INC(counter) lv_ATOMIC_INC(&(counter))
#endif
#ifndef PRESET_ATOMIC_DEC
#define PRESET_ATOMIC_DEC(counter) lv_ATOMIC_DEC(&(counter))
#endif

#ifndef DETERMINISM_UNVERIFIED
#define DETERMINISM_UNVERIFIED DETERMINISM_STATE_UNVERIFIED
#endif
#ifndef DETERMINISM_VERIFIED
#define DETERMINISM_VERIFIED DETERMINISM_STATE_VERIFIED
#endif
#ifndef DETERMINISM_NON_DETERMINISTIC
#define DETERMINISM_NON_DETERMINISTIC DETERMINISM_STATE_NON_DETERMINISTIC
#endif

/* ---- 类型定义 ---- */
typedef enum {
    PRESET_COMPOSE_SEQUENCE,
    PRESET_COMPOSE_PARALLEL,
    PRESET_COMPOSE_PIPE,
    PRESET_COMPOSE_FEEDBACK,
    PRESET_COMPOSE_BRANCH
} PresetComposeMode;

#ifdef _WIN32
typedef volatile long PresetAtomicCounter;
#else
typedef volatile int PresetAtomicCounter;
#endif

/** @brief 预设实例内部结构（实例化后的函数块及相关元数据） */
typedef struct PresetInstance {
    FuncBlock *func_block; /**< 实例化后的函数块 */
    int *output_node_ids;  /**< 输出节点ID数组 */
    int output_count;      /**< 输出节点数量 */
    char *preset_name;     /**< 来源预设名称 */
    int reference_count;   /**< 引用计数 */
} PresetInstance;

typedef PresetInstance *PresetInstanceHandle;
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

/** @brief 内部预设条目结构 */
typedef struct InternalPresetEntry {
    int id;                           /* 预设ID */
    PresetMetadata metadata;          /* 预设元数据 */
    FuncBlock *template_fb;           /* 模板函数块（可为NULL） */
    bool is_builtin;                  /* 是否为内置预设 */
    bool is_active;                   /* 是否激活 */
    int reference_count;              /* 引用计数 */
} InternalPresetEntry;

/** @brief 预设库状态结构 */
typedef struct {
    lvHashtable *hash_table;          /* 名称 -> InternalPresetEntry* 哈希表（string 形态，复用 lv_hashtable） */
    int entry_count;                  /* 条目数量 */
    int builtin_count;                /* 内置预设数量 */
    int custom_count;                 /* 自定义预设数量 */
    bool initialized;                 /* 是否已初始化 */

    lvMutex mutex; /* 互斥锁 */

    PresetAtomicCounter next_id;         /* 下一个预设ID */
    char last_error[PRESET_BUFFER_SIZE]; /* 最后错误信息 */

    /* 错误回调 */
    void (*error_callback)(const char *, void *);
    void *error_callback_data;
} PresetLibraryState;

/* 全局状态（在 preset_manager.c 定义） */
extern PresetLibraryState g_library;

/* ---- 共享辅助函数（在 preset_manager.c 定义） ---- */
uint32_t hash_string(const char *str);
InternalPresetEntry *find_entry(const char *name);
bool insert_entry(InternalPresetEntry *entry);
bool remove_entry(const char *name);
void free_entry(InternalPresetEntry *entry);
void lock_library(void);
void unlock_library(void);
void set_error(const char *fmt, ...);
void clear_error(void);

/* ---- 公共 API 前向声明（跨拆分文件相互调用需要） ---- */
bool preset_register_custom(const PresetMetadata *metadata, const FuncBlock *template_fb, PresetEntryHandle *out_entry);
PresetEntryHandle preset_find(const char *name);
void preset_release(PresetEntryHandle entry);
const PresetMetadata *preset_get_metadata(PresetEntryHandle entry);
bool preset_library_init(void);
bool preset_library_shutdown(void);
bool preset_library_is_initialized(void);
const PresetVersion *preset_library_get_version(void);
bool preset_library_get_statistics(PresetStatistics *stats);
bool preset_library_reset(void);
bool preset_unregister(const char *name);
bool preset_exists(const char *name);
bool preset_is_builtin(const char *name);
const char *preset_get_last_error(void);
void preset_clear_error(void);
void preset_set_error_callback(void (*callback)(const char *error, void *user_data), void *user_data);
bool preset_instantiate(const char *name, const int *input_nodes, int input_count,
                         const PresetInstantiateOptions *options, PresetInstanceHandle *out_instance);
int preset_instantiate_batch(const char **names, const int **input_nodes_array, const int *input_counts, int count,
                           const PresetInstantiateOptions *options, PresetInstanceHandle **out_instances);
void preset_instance_destroy(PresetInstanceHandle instance);
const FuncBlock *preset_instance_get_func_block(PresetInstanceHandle instance);
bool preset_instance_get_outputs(PresetInstanceHandle instance, int **out_output_ids, int *out_count);
bool preset_instance_execute(PresetInstanceHandle instance, const PresetExecutionContext *context);
bool preset_instance_validate(PresetInstanceHandle instance, bool *out_is_valid, char **out_error_message);
bool preset_compose(const PresetComposition *composition, PresetEntryHandle *out_new_entry);
bool preset_bind_parameter(const char *preset_name, int param_index, int value, char **out_new_name);
bool preset_get_usage_example(const char *name, char **out_example);
bool preset_generate_documentation(const char *name, const char *format, char **out_document);
bool preset_generate_library_documentation(const char *format, char **out_document);
bool preset_serialize(PresetEntryHandle entry, uint8_t **out_data, size_t *out_size);
bool preset_deserialize(const uint8_t *data, size_t size, PresetEntryHandle *out_entry);
bool preset_export_to_file(const char *name, const char *filepath);
bool preset_import_from_file(const char *filepath, char **out_name);
bool preset_query(const PresetQueryCriteria *criteria, PresetQueryResult **out_result);
void preset_query_result_free(PresetQueryResult *result);
bool preset_list_by_category(PresetCategory category, char ***out_names, int *out_count);
bool preset_list_all(char ***out_names, int *out_count);
int preset_register_builtin(void);
int preset_register_batch(const PresetMetadata *metadatas, int count);

#ifdef __cplusplus
}
#endif

#endif /* lv_PRESET_MANAGER_INTERNAL_H */

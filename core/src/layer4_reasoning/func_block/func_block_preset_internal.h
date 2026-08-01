/**
 * @file func_block_preset_internal.h
 * @brief 预设函数块库内部共享定义
 *
 * 由 func_block_preset.c 拆分出的各模块共享的全局状态、
 * 内部数据结构和辅助函数声明。
 *
 * @version v3.6.0
 */

#ifndef lv_FUNC_BLOCK_PRESET_INTERNAL_H
#define lv_FUNC_BLOCK_PRESET_INTERNAL_H

#include "func_block_preset.h"
#include "lv/func_block_internal.h"

#include <stdbool.h>

/* 命名常量（引用 lv_internal.h / func_block_preset.h 中的统一定义） */
#ifndef MAX_PRESETS
#define MAX_PRESETS lv_PRESET_MAX_COUNT
#endif
#ifndef MAX_PARAMS
#define MAX_PARAMS lv_PRESET_MAX_PARAMS
#endif
#ifndef BUFFER_SIZE
#define BUFFER_SIZE 4096
#endif

/**
 * @brief 内部预设条目
 *
 * 存储单个预设的完整信息，包括元数据、模板函数块和状态标志。
 */
typedef struct {
    PresetMetadata metadata; /**< 预设元数据（名称、描述、分类、复杂度等） */
    FuncBlock *template_fb;  /**< 模板函数块（用于实例化的原型，所有权归本条目） */
    bool is_builtin;         /**< 是否为内置预设（内置预设不可被用户删除） */
    bool is_active;          /**< 是否激活（未激活的预设不参与查找和实例化） */
} InternalPresetEntry;

/**
 * @brief 预设库全局状态
 *
 * 注意：entries 使用固定大小数组 MAX_PRESETS，预设总数上限为 MAX_PRESETS。
 */
typedef struct {
    InternalPresetEntry entries[MAX_PRESETS]; /**< 预设条目数组（固定大小） */
    int count;                                /**< 当前预设数量 */
    bool initialized;                         /**< 是否已初始化 */
    int next_preset_id;                       /**< 下一个预设ID */
} PresetLibraryState;

/** 预设库全局实例（定义于 func_block_preset_data.c） */
extern PresetLibraryState g_preset_library;

/** 内置预设元数据表（定义于 func_block_preset_data.c） */
extern const PresetMetadata g_builtin_metadata[];
/** 内置预设数量（定义于 func_block_preset_data.c） */
extern const int g_builtin_count;

/**
 * @brief 查找预设条目索引
 *
 * 在 g_preset_library.entries 数组中线性搜索指定名称的预设。
 * 仅搜索已激活（is_active == true）的条目。
 *
 * @param name 预设名称（区分大小写）
 * @return 找到时返回数组索引，未找到或参数无效返回 -1
 */
int find_preset_index(const char *name);

#endif /* lv_FUNC_BLOCK_PRESET_INTERNAL_H */

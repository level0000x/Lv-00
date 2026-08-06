/**
 * @file preset_blocks.c
 * @brief 预设函数块扩展系统 - 实现
 *
 * 实现扩展的预设函数块注册系统，为理论数学研究
 * 提供更丰富的预设函数块集合。
 *
 * 内存管理：
 * - 使用 lv_malloc / lv_free / lv_realloc 进行内存管理
 * - 使用 lv_strdup 进行字符串复制
 * - cleanup 时释放所有资源
 */

#include "preset_blocks.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv_internal.h"
#include "lv/lv_xmacro.h"
#include "lv_utils.h"
#include "preset_common.h"

/* ==================== LVZ 预设文件加载 ==================== */

/* 从 module_lvz.c 中引用的函数声明 */
extern bool lvz_load_presets_file(const char *filepath);

/**
 * @brief 预设 .lvz 文件列表
 *
 * 这些文件位于 module/presets/ 目录下，包含通过 Python 脚本
 * 从 C 预设文件自动转换的预设定义。
 */
static const char *g_preset_lvz_files[] = {
    "preset_advanced_geometry.lvz",
    "preset_algebraic.lvz",
    "preset_algebraic_geometry.lvz",
    "preset_algebraic_topology.lvz",
    "preset_algebraic_topology_adv.lvz",
    "preset_analysis.lvz",
    "preset_arithmetic_geometry.lvz",
    "preset_basic_geometry.lvz",
    "preset_category_theory.lvz",
    "preset_category_theory_adv.lvz",
    "preset_coding_theory.lvz",
    "preset_combinatorics.lvz",
    "preset_complex_analysis.lvz",
    "preset_difference_equations.lvz",
    "preset_differential_equations.lvz",
    "preset_differential_geometry.lvz",
    "preset_differential_geometry_adv.lvz",
    "preset_dynamical_systems.lvz",
    "preset_field_theory.lvz",
    "preset_functional_analysis.lvz",
    "preset_functional_analysis_adv.lvz",
    "preset_game_theory.lvz",
    "preset_geometry_3d.lvz",
    "preset_graph_theory.lvz",
    "preset_group_theory.lvz",
    "preset_homological_algebra.lvz",
    "preset_information_theory.lvz",
    "preset_integral_transforms.lvz",
    "preset_lattice_theory.lvz",
    "preset_lie_theory_advanced.lvz",
    "preset_linear_algebra.lvz",
    "preset_logic_advanced.lvz",
    "preset_math_logic.lvz",
    "preset_mathematical_logic.lvz",
    "preset_mathematical_physics.lvz",
    "preset_matrix.lvz",
    "preset_measure_theory.lvz",
    "preset_measurements.lvz",
    "preset_number_theory.lvz",
    "preset_numerical.lvz",
    "preset_numerical_analysis.lvz",
    "preset_optimization.lvz",
    "preset_order_theory.lvz",
    "preset_polygons.lvz",
    "preset_polynomial.lvz",
    "preset_probability.lvz",
    "preset_probability_statistics.lvz",
    "preset_representation_theory.lvz",
    "preset_ring_theory.lvz",
    "preset_set_theory.lvz",
    "preset_special_functions.lvz",
    "preset_statistics.lvz",
    "preset_stochastic_processes.lvz",
    "preset_topology.lvz",
    "preset_transformations.lvz",
    "preset_trigonometry.lvz",
};

/**
 * @brief 从 .lvz 文件加载所有预设定义
 *
 * 遍历 g_preset_lvz_files 列表，构造完整路径并调用
 * lvz_load_presets_file 加载每个 .lvz 文件中的预设定义。
 *
 * 路径由编译时定义的 lv_PRESETS_DIR 宏指定。
 */
static void load_presets_from_lvz(void) {
    const size_t file_count = sizeof(g_preset_lvz_files) / sizeof(g_preset_lvz_files[0]);

#ifdef lv_PRESETS_DIR
    /* 使用编译时定义的预设目录路径 */
    const char *presets_dir = lv_PRESETS_DIR;
#else
    /* 回退到相对于工作目录的默认路径 */
    const char *presets_dir = "module/presets";
#endif

    for (size_t i = 0; i < file_count; i++) {
        /* 构造完整路径: dir/filename */
        size_t dir_len = strlen(presets_dir);
        size_t name_len = strlen(g_preset_lvz_files[i]);
        char *filepath = (char *) lv_malloc(dir_len + 1 + name_len + 1);
        if (!filepath) {
            lv_LOG_WARNING("无法分配内存以加载预设文件 '%s'", g_preset_lvz_files[i]);
            continue;
        }
        memcpy(filepath, presets_dir, dir_len);
        filepath[dir_len] = '/';
        memcpy(filepath + dir_len + 1, g_preset_lvz_files[i], name_len + 1);

        if (!lvz_load_presets_file(filepath)) {
            lv_LOG_WARNING("加载预设文件 '%s' 失败", filepath);
        }
        lv_free((void **) &filepath);
    }
}

/* ==================== 命名常量 ==================== */

/** 注册表初始容量 */
#define PRESET_REGISTRY_INITIAL_CAPACITY 64

/** 数组扩容增长因子 */
#define PRESET_REGISTRY_GROWTH_FACTOR 2

/** 预设函数块 ID 起始偏移（引用 lv_internal.h 中的统一定义） */
#define PRESET_FB_ID_OFFSET lv_PRESET_ID_OFFSET

/* ==================== 内部数据结构 ==================== */

/**
 * @brief 内部预设条目结构
 *
 * 存储预设的完整信息，包括元数据和模板函数块。
 */
typedef struct {
    char *name;                      /* 预设名称（唯一键） */
    char *description;               /* 中文描述 */
    char *mathematical_definition;   /* 数学定义 */
    PresetExtendedCategory category; /* 扩展类别 */
    int input_count;                 /* 输入端口数量 */
    int output_count;                /* 输出端口数量 */
    bool has_selector;               /* 是否需要多解选择器 */
    char *preconditions;             /* 前置条件描述 */
    char *example_usage;             /* 使用示例 */
    FuncBlock *template_fb;          /* 模板函数块 */
    int id;                          /* 预设ID */

    /* 简化注册接口扩展字段 */
    PresetType *input_types; /* 输入类型数组 */
    int input_type_count;    /* 输入类型数量 */
    PresetType output_type;  /* 输出类型 */
    char *complexity;        /* 时间复杂度描述 */
    bool is_constructive;    /* 是否构造性 */
    bool is_reversible;      /* 是否可逆 */
} InternalPresetEntry;

/**
 * @brief 扩展预设函数块注册表
 */
typedef struct {
    InternalPresetEntry *entries; /* 条目数组 */
    int count;                    /* 当前条目数 */
    int capacity;                 /* 数组容量 */
    bool initialized;             /* 是否已初始化 */
    int next_preset_id;           /* 下一个预设ID */
} ExtendedPresetRegistry;

/* ==================== 全局注册表 ==================== */

/** 全局扩展预设函数块注册表（单例） */
static ExtendedPresetRegistry g_preset_registry = {
    .entries = NULL, .count = 0, .capacity = 0, .initialized = false, .next_preset_id = PRESET_FB_ID_OFFSET};

/* 线程安全：注册表互斥锁
 *
 * 使用 lv_lazy_lock 惰性互斥锁：首次加锁时自动完成初始化
 * （lv_once 保证仅执行一次且同步完成），消除 TOCTOU 竞态。
 * 锁生命周期与进程一致，不销毁（lv_once 不可重置）。
 */
#include "lv/lv_platform.h"
#include "lv/lv_thread.h"

/** 预设注册表互斥锁（惰性初始化，首次加锁时自动完成） */
lv_LAZY_LOCK_DEFINE(g_preset_registry_lock);

#define PRESET_REGISTRY_LOCK()   lv_lazy_lock_lock(&g_preset_registry_lock, g_preset_registry_lock_init_once)
#define PRESET_REGISTRY_UNLOCK() lv_lazy_lock_unlock(&g_preset_registry_lock)

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 确保注册表数组有足够的容量
 *
 * 当 count >= capacity 时，以 PRESET_REGISTRY_GROWTH_FACTOR 倍率扩容。
 * 包含整数溢出检查，参考 func_block_registry.c 中的 ensure_registry_capacity。
 *
 * @return true 扩容成功或无需扩容，false 内存不足或溢出
 */
static bool ensure_preset_registry_capacity(void) {
    if (g_preset_registry.count < g_preset_registry.capacity) {
        return true;
    }

    /* 统一委托 lv_ensure_capacity（内部含 INT_MAX/SIZE_MAX 溢出检查与倍增；失败时指针/容量不变） */
    return lv_ensure_capacity((void **) &g_preset_registry.entries, g_preset_registry.count,
                              &g_preset_registry.capacity, sizeof(InternalPresetEntry), 0);
}

/**
 * @brief 释放内部预设条目的资源
 *
 * @param entry 预设条目指针
 */
static void free_internal_preset_entry(InternalPresetEntry *entry) {
    if (!entry)
        return;

    lv_free((void **) &entry->name);
    lv_free((void **) &entry->description);
    lv_free((void **) &entry->mathematical_definition);
    lv_free((void **) &entry->preconditions);
    lv_free((void **) &entry->example_usage);
    lv_free((void **) &entry->complexity);
    lv_free((void **) &entry->input_types);

    if (entry->template_fb) {
        func_block_destroy(entry->template_fb);
        entry->template_fb = NULL;
    }

    memset(entry, 0, sizeof(InternalPresetEntry));
}

/**
 * @brief 查找预设条目索引
 *
 * @param name 预设名称
 * @return 条目索引，未找到返回 -1
 */
static int find_preset_index(const char *name) {
    if (!name)
        return -1;

    for (int i = 0; i < g_preset_registry.count; i++) {
        if (g_preset_registry.entries[i].name && strcmp(g_preset_registry.entries[i].name, name) == 0) {
            return i;
        }
    }

    return -1;
}

/* ==================== 公共 API 实现 ==================== */

bool preset_blocks_init(void) {
    /* 幂等操作：已初始化则直接返回 */
    if (g_preset_registry.initialized) {
        return true;
    }

    /* 确保基础注册表已初始化（锁初始化由 LOCK 宏自动完成） */
    if (!func_block_registry_init()) {
        return false;
    }

    /* ============================================================
     * 从 .lvz 文件加载预设定义
     *
     * 所有预设定义已从 C 源文件转换为 .lvz 格式，
     * 存放在 module/presets/ 目录下。
     * 此处通过 lvz_load_presets_file 逐个加载并注册。
     * ============================================================ */

    load_presets_from_lvz();

    g_preset_registry.initialized = true;
    return true;
}

void lv_preset_blocks_cleanup(void) {
    PRESET_REGISTRY_LOCK();

    /* 释放所有条目的资源 */
    for (int i = 0; i < g_preset_registry.count; i++) {
        free_internal_preset_entry(&g_preset_registry.entries[i]);
    }

    /* 释放条目数组本身 */
    lv_free((void **) &g_preset_registry.entries);

    /* 重置注册表状态 */
    g_preset_registry.count = 0;
    g_preset_registry.capacity = 0;
    g_preset_registry.initialized = false;
    g_preset_registry.next_preset_id = PRESET_FB_ID_OFFSET;

    PRESET_REGISTRY_UNLOCK();
    /* 注：注册表互斥锁由 lv_once 一次性初始化，生命周期与进程一致，
     * 不在此销毁；lv_once 不可重置，清理后系统仍可安全继续使用。 */
}

PresetBlockMetadata *preset_blocks_get_metadata(const char *name) {
    PRESET_REGISTRY_LOCK();

    int idx = find_preset_index(name);
    if (idx < 0) {
        PRESET_REGISTRY_UNLOCK();
        return NULL;
    }

    InternalPresetEntry *entry = &g_preset_registry.entries[idx];

    PresetBlockMetadata *result = lv_malloc(sizeof(PresetBlockMetadata));
    if (result) {
        /* 深拷贝字符串字段，确保返回的元数据生命周期独立于注册表 */
        result->name = entry->name ? lv_strdup(entry->name) : NULL;
        result->description = entry->description ? lv_strdup(entry->description) : NULL;
        result->mathematical_definition =
            entry->mathematical_definition ? lv_strdup(entry->mathematical_definition) : NULL;
        result->category = entry->category;
        result->input_count = entry->input_count;
        result->output_count = entry->output_count;
        result->has_selector = entry->has_selector;
        result->preconditions = entry->preconditions ? lv_strdup(entry->preconditions) : NULL;
        result->example_usage = entry->example_usage ? lv_strdup(entry->example_usage) : NULL;

        /* 若关键字符串分配失败，回滚已分配的内存 */
        if (entry->name && !result->name) {
            lv_free((void **) &result->description);
            lv_free((void **) &result->mathematical_definition);
            lv_free((void **) &result->preconditions);
            lv_free((void **) &result->example_usage);
            lv_free((void **) &result);
            result = NULL;
        }
    }

    PRESET_REGISTRY_UNLOCK();
    return result;
}

int preset_blocks_find_by_category(PresetExtendedCategory category, const char **out_names, int max_count) {
    if (!out_names || max_count <= 0)
        return 0;

    PRESET_REGISTRY_LOCK();

    int found = 0;
    for (int i = 0; i < g_preset_registry.count; i++) {
        if (g_preset_registry.entries[i].category == category) {
            if (found < max_count) {
                out_names[found] = g_preset_registry.entries[i].name;
            }
            found++;
        }
    }

    PRESET_REGISTRY_UNLOCK();
    return found;
}

/* ================================================================
 * 枚举 -> 名称 映射表（数据表化，替代 switch）
 * ================================================================ */

/** @brief preset_extended_category_to_string 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_preset_extended_category_to_string_entries[] = {
    {"基础几何构造", PRESET_EXT_BASIC_CONSTRUCTION},
    {"高级几何构造", PRESET_EXT_ADVANCED_CONSTRUCTION},
    {"多边形", PRESET_EXT_POLYGON},
    {"圆相关", PRESET_EXT_CIRCLE},
    {"基本变换", PRESET_EXT_TRANSFORMATION_BASIC},
    {"高级变换", PRESET_EXT_TRANSFORMATION_ADVANCED},
    {"度量计算", PRESET_EXT_MEASUREMENT},
    {"三角函数", PRESET_EXT_TRIGONOMETRY},
    {"坐标运算", PRESET_EXT_COORDINATE},
    {"基础代数", PRESET_EXT_ALGEBRA_BASIC},
    {"高级代数", PRESET_EXT_ALGEBRA_ADVANCED},
    {"线性代数", PRESET_EXT_LINEAR_ALGEBRA},
    {"多项式", PRESET_EXT_POLYNOMIAL},
    {"命题逻辑", PRESET_EXT_LOGIC_PROPOSITIONAL},
    {"谓词逻辑", PRESET_EXT_LOGIC_PREDICATE},
    {"证明策略", PRESET_EXT_PROOF_TACTICS},
    {"极限", PRESET_EXT_ANALYSIS_LIMIT},
    {"微分", PRESET_EXT_ANALYSIS_DIFFERENTIAL},
    {"积分", PRESET_EXT_ANALYSIS_INTEGRAL},
    {"拓扑", PRESET_EXT_TOPOLOGY},
    {"微分几何", PRESET_EXT_DIFFERENTIAL_GEOMETRY},
    {"数论", PRESET_EXT_NUMBER_THEORY},
    {"群论", PRESET_EXT_GROUP_THEORY},
    {"分析学", PRESET_EXT_ANALYSIS},
    {"组合数学", PRESET_EXT_COMBINATORICS},
    {"类别总数", PRESET_EXT_CATEGORY_COUNT},
};

const char *preset_extended_category_to_string(PresetExtendedCategory cat) {
    return lv_enum_to_str(s_preset_extended_category_to_string_entries, lv_ARRAY_SIZE(s_preset_extended_category_to_string_entries), (int) cat, "未知类别");
}

/* ==================== 预设注册函数 ==================== */

/** @brief 按 PresetCategory 枚举值索引的查找表 */
static const PresetExtendedCategory kCategoryToExtendedMap[PRESET_CATEGORY_COUNT] = {
    PRESET_EXT_BASIC_CONSTRUCTION,       /* PRESET_CATEGORY_CONSTRUCTION */
    PRESET_EXT_MEASUREMENT,              /* PRESET_CATEGORY_MEASUREMENT */
    PRESET_EXT_TRANSFORMATION_BASIC,     /* PRESET_CATEGORY_TRANSFORMATION */
    PRESET_EXT_ALGEBRA_BASIC,            /* PRESET_CATEGORY_ALGEBRAIC */
    PRESET_EXT_LOGIC_PROPOSITIONAL,      /* PRESET_CATEGORY_LOGIC */
    PRESET_EXT_ANALYSIS,                 /* PRESET_CATEGORY_ANALYSIS */
    PRESET_EXT_NUMBER_THEORY,            /* PRESET_CATEGORY_NUMBER_THEORY */
    PRESET_EXT_GROUP_THEORY,             /* PRESET_CATEGORY_GROUP_THEORY */
    PRESET_EXT_TOPOLOGY,                 /* PRESET_CATEGORY_TOPOLOGY */
    PRESET_EXT_ALGEBRA_BASIC,            /* PRESET_CATEGORY_RING_THEORY */
    PRESET_EXT_ALGEBRA_ADVANCED,         /* PRESET_CATEGORY_FIELD_THEORY */
    PRESET_EXT_LINEAR_ALGEBRA,           /* PRESET_CATEGORY_LINEAR_ALGEBRA */
    PRESET_EXT_COMBINATORICS,            /* PRESET_CATEGORY_COMBINATORICS */
    PRESET_EXT_ANALYSIS,                 /* PRESET_CATEGORY_COMPLEX_ANALYSIS */
    PRESET_EXT_ANALYSIS,                 /* PRESET_CATEGORY_PROBABILITY */
    PRESET_EXT_ADVANCED_CONSTRUCTION,    /* PRESET_CATEGORY_GEOMETRY */
    PRESET_EXT_ALGEBRA_BASIC,            /* PRESET_CATEGORY_ALGEBRA */
    PRESET_EXT_TOPOLOGY,                 /* PRESET_CATEGORY_CATEGORY_THEORY */
    PRESET_EXT_TOPOLOGY,                 /* PRESET_CATEGORY_SET_THEORY */
    PRESET_EXT_BASIC_CONSTRUCTION,       /* PRESET_CATEGORY_CUSTOM */
    PRESET_EXT_GRAPH_THEORY,             /* PRESET_CATEGORY_GRAPH_THEORY */
    PRESET_EXT_DIFFERENTIAL_GEOMETRY,    /* PRESET_CATEGORY_DIFFERENTIAL_GEOMETRY */
    PRESET_EXT_NUMERICAL_ANALYSIS,       /* PRESET_CATEGORY_NUMERICAL */
    PRESET_EXT_OPTIMIZATION_THEORY,      /* PRESET_CATEGORY_OPTIMIZATION */
    PRESET_EXT_MATH_LOGIC,               /* PRESET_CATEGORY_MATH_LOGIC */
    PRESET_EXT_BASIC_CONSTRUCTION,       /* PRESET_CATEGORY_COUNT */
};
lv_STATIC_ASSERT(sizeof(kCategoryToExtendedMap) / sizeof(kCategoryToExtendedMap[0]) == PRESET_CATEGORY_COUNT,
                 "kCategoryToExtendedMap 大小必须与 PRESET_CATEGORY_COUNT 一致");

/**
 * @brief 将 PresetCategory 映射到 PresetExtendedCategory
 *
 * v5.0 统一注册接口使用 PresetCategory，内部需要映射到
 * PresetExtendedCategory 以兼容现有的注册表结构。
 *
 * @param category 预设类别
 * @return 对应的扩展类别
 */
static PresetExtendedCategory map_category_to_extended(PresetCategory category) {
    if ((int)category < 0 || (int)category >= PRESET_CATEGORY_COUNT) {
        return PRESET_EXT_BASIC_CONSTRUCTION;
    }
    return kCategoryToExtendedMap[category];
}

/* ==================== 通用简化注册 ==================== */

bool preset_blocks_register_simple(const char *name, const char *description, PresetCategory category,
                                   const PresetType *input_types, int input_count, PresetType output_type,
                                   const char *mathematical_definition, const char *complexity, bool is_constructive,
                                   bool is_reversible) {
    /* 参数有效性检查 */
    if (!name || !description) {
        /* lv_LOG_WARNING("preset_blocks_register_simple: 名称或描述为空"); */
        return false;
    }

    if (input_count < 0) {
        lv_LOG_WARNING("preset_blocks_register_simple: 输入数量无效 (%d)", input_count);
        return false;
    }

    PRESET_REGISTRY_LOCK();

    /* 检查是否已存在同名预设 */
    if (find_preset_index(name) >= 0) {
        lv_LOG_WARNING("preset_blocks_register_simple: 预设已存在 '%s'", name);
        PRESET_REGISTRY_UNLOCK();
        return false;
    }

    /* 确保注册表容量 */
    if (!ensure_preset_registry_capacity()) {
        /* lv_LOG_WARNING("preset_blocks_register_simple: 注册表容量不足"); */
        PRESET_REGISTRY_UNLOCK();
        return false;
    }

    /* 创建模板函数块 */
    FuncBlock *fb = func_block_create(g_preset_registry.next_preset_id++);
    if (!fb) {
        lv_LOG_WARNING("preset_blocks_register_simple: 创建函数块失败 '%s'", name);
        PRESET_REGISTRY_UNLOCK();
        return false;
    }

    /* 设置名称 */
    if (!func_block_set_name(fb, name)) {
        func_block_destroy(fb);
        PRESET_REGISTRY_UNLOCK();
        return false;
    }

    /* 设置描述 */
    if (!func_block_set_description(fb, description)) {
        func_block_destroy(fb);
        PRESET_REGISTRY_UNLOCK();
        return false;
    }

    /* 设置输入输出端口数 */
    fb->input_count = input_count;
    fb->output_count = 1; /* 简化注册始终为单输出 */

    /* 设置确定性状态 */
    fb->determinism = DETERMINISM_STATE_VERIFIED;

    /* 填充条目 */
    InternalPresetEntry *entry = &g_preset_registry.entries[g_preset_registry.count];
    memset(entry, 0, sizeof(InternalPresetEntry));

    entry->name = lv_strdup(name);
    entry->description = lv_strdup(description);

    /* 映射类别：将基础 PresetCategory 转换为扩展 PresetExtendedCategory */
    entry->category = map_category_to_extended(category);

    /* 复制数学定义（可选） */
    if (mathematical_definition) {
        entry->mathematical_definition = lv_strdup(mathematical_definition);
    }

    /* 复制输入类型数组（可选） */
    if (input_types && input_count > 0) {
        entry->input_types = lv_malloc((size_t) input_count * sizeof(PresetType));
        if (entry->input_types) {
            memcpy(entry->input_types, input_types, (size_t) input_count * sizeof(PresetType));
            entry->input_type_count = input_count;
        }
    }

    /* 设置输出类型 */
    entry->output_type = output_type;

    /* 复制复杂度描述（可选） */
    if (complexity) {
        entry->complexity = lv_strdup(complexity);
    }

    /* 设置构造性和可逆性标志 */
    entry->is_constructive = is_constructive;
    entry->is_reversible = is_reversible;

    /* 设置输入输出端口数量 */
    entry->input_count = input_count;
    entry->output_count = 1;

    /* 设置模板函数块和ID */
    entry->template_fb = fb;
    entry->id = fb->id;

    /* 验证关键字段分配成功 */
    if (!entry->name || !entry->description) {
        free_internal_preset_entry(entry);
        lv_LOG_WARNING("preset_blocks_register_simple: 内存分配失败 '%s'", name);
        PRESET_REGISTRY_UNLOCK();
        return false;
    }

    g_preset_registry.count++;
    PRESET_REGISTRY_UNLOCK();
    return true;
}

/**
 * @brief 注册预设的通用内部实现
 *
 * @param name 预设名称
 * @param description 中文描述
 * @param category 扩展类别
 * @param input_count 输入端口数量
 * @param output_count 输出端口数量
 * @return true 注册成功
 */
static bool register_preset_internal(const char *name, const char *description, PresetExtendedCategory category,
                                     int input_count, int output_count) {
    if (!name || !description)
        return false;
    if (find_preset_index(name) >= 0)
        return false;
    if (!ensure_preset_registry_capacity())
        return false;

    /* 创建模板函数块 */
    FuncBlock *fb = func_block_create(g_preset_registry.next_preset_id++);
    if (!fb)
        return false;

    if (!func_block_set_name(fb, name)) {
        func_block_destroy(fb);
        return false;
    }

    if (!func_block_set_description(fb, description)) {
        func_block_destroy(fb);
        return false;
    }

    fb->input_count = input_count;
    fb->output_count = output_count;
    fb->determinism = DETERMINISM_STATE_VERIFIED;

    /* 填充条目 */
    InternalPresetEntry *entry = &g_preset_registry.entries[g_preset_registry.count];
    memset(entry, 0, sizeof(InternalPresetEntry));

    entry->name = lv_strdup(name);
    entry->description = lv_strdup(description);
    entry->category = category;
    entry->input_count = input_count;
    entry->output_count = output_count;
    entry->template_fb = fb;
    entry->id = fb->id;

    if (!entry->name || !entry->description) {
        free_internal_preset_entry(entry);
        return false;
    }

    g_preset_registry.count++;
    return true;
}

bool preset_blocks_register_by_category(const char *name, const char *description, PresetExtendedCategory category,
                                        int input_count, int output_count) {
    return register_preset_internal(name, description, category, input_count, output_count);
}

/* ==================== 查找函数 ==================== */

int preset_blocks_find_by_prefix(const char *prefix, const char **out_names, int max_count) {
    if (!prefix || !out_names || max_count <= 0)
        return 0;

    PRESET_REGISTRY_LOCK();

    size_t prefix_len = strlen(prefix);
    int found = 0;

    for (int i = 0; i < g_preset_registry.count; i++) {
        if (g_preset_registry.entries[i].name && strncmp(g_preset_registry.entries[i].name, prefix, prefix_len) == 0) {
            if (found < max_count) {
                out_names[found] = g_preset_registry.entries[i].name;
            }
            found++;
        }
    }

    PRESET_REGISTRY_UNLOCK();
    return found;
}

int preset_blocks_find_by_keyword(const char *keyword, const char **out_names, int max_count) {
    if (!keyword || !out_names || max_count <= 0)
        return 0;

    PRESET_REGISTRY_LOCK();

    int found = 0;

    for (int i = 0; i < g_preset_registry.count; i++) {
        if (g_preset_registry.entries[i].description &&
            strstr(g_preset_registry.entries[i].description, keyword) != NULL) {
            if (found < max_count) {
                out_names[found] = g_preset_registry.entries[i].name;
            }
            found++;
        }
    }

    PRESET_REGISTRY_UNLOCK();
    return found;
}

int preset_blocks_get_all_names(const char **out_names, int max_count) {
    if (!out_names || max_count <= 0)
        return 0;

    PRESET_REGISTRY_LOCK();

    int count = 0;
    for (int i = 0; i < g_preset_registry.count && count < max_count; i++) {
        if (g_preset_registry.entries[i].name) {
            out_names[count] = g_preset_registry.entries[i].name;
            count++;
        }
    }

    PRESET_REGISTRY_UNLOCK();
    return count;
}

/* ==================== 文档生成 ==================== */

char *preset_blocks_generate_documentation(void) {
    PRESET_REGISTRY_LOCK();

    /* 计算所需缓冲区大小 */
    size_t total_size = 4096; /* 基础大小 */
    for (int i = 0; i < g_preset_registry.count; i++) {
        if (g_preset_registry.entries[i].name) {
            total_size += strlen(g_preset_registry.entries[i].name) + 256;
        }
        if (g_preset_registry.entries[i].description) {
            total_size += strlen(g_preset_registry.entries[i].description);
        }
    }

    char *doc = lv_malloc(total_size);
    if (!doc) {
        PRESET_REGISTRY_UNLOCK();
        return NULL;
    }

    int written = snprintf(doc, total_size,
                           "# Lv-00 预设函数块文档\n\n"
                           "## 概述\n\n"
                           "本系统提供 %d 个预设函数块，涵盖以下数学领域：\n\n",
                           g_preset_registry.count);
    if (written < 0) {
        lv_free((void **) &doc);
        PRESET_REGISTRY_UNLOCK();
        return NULL;
    }
    if ((size_t) written >= total_size) {
        doc[total_size - 1] = '\0';
    }

    /* 按类别分组输出 */
    for (int cat = 0; cat < PRESET_EXT_CATEGORY_COUNT; cat++) {
        const char *cat_name = preset_extended_category_to_string((PresetExtendedCategory) cat);
        int cat_count = 0;

        /* 统计该类别数量 */
        for (int i = 0; i < g_preset_registry.count; i++) {
            if (g_preset_registry.entries[i].category == (PresetExtendedCategory) cat) {
                cat_count++;
            }
        }

        if (cat_count > 0) {
            int w = snprintf(doc + written, total_size - written, "### %s (%d个)\n\n", cat_name, cat_count);
            if (w < 0)
                break;
            if ((size_t) w >= total_size - (size_t) written) {
                written = (int) total_size - 1;
                break;
            }
            written += w;

            for (int i = 0; i < g_preset_registry.count; i++) {
                if (g_preset_registry.entries[i].category == (PresetExtendedCategory) cat) {
                    w = snprintf(
                        doc + written, total_size - written, "- **%s**: %s\n", g_preset_registry.entries[i].name,
                        g_preset_registry.entries[i].description ? g_preset_registry.entries[i].description : "");
                    if (w < 0)
                        break;
                    if ((size_t) w >= total_size - (size_t) written) {
                        written = (int) total_size - 1;
                        break;
                    }
                    written += w;
                }
            }
            if ((size_t) written >= total_size - 1)
                break;
            w = snprintf(doc + written, total_size - written, "\n");
            if (w < 0)
                break;
            if ((size_t) w >= total_size - (size_t) written) {
                written = (int) total_size - 1;
                break;
            }
            written += w;
        }
    }

    PRESET_REGISTRY_UNLOCK();
    return doc;
}

char *preset_blocks_generate_single_doc(const char *name) {
    PRESET_REGISTRY_LOCK();

    int idx = find_preset_index(name);
    if (idx < 0) {
        PRESET_REGISTRY_UNLOCK();
        return NULL;
    }

    InternalPresetEntry *entry = &g_preset_registry.entries[idx];
    size_t size = 2048;

    char *doc = lv_malloc(size);
    if (!doc) {
        PRESET_REGISTRY_UNLOCK();
        return NULL;
    }

    /* 在锁保护下复制需要的数据 */
    const char *entry_name = entry->name ? entry->name : "未知";
    const char *entry_cat = preset_extended_category_to_string(entry->category);
    const char *entry_desc = entry->description ? entry->description : "无描述";
    int entry_in = entry->input_count;
    int entry_out = entry->output_count;

    PRESET_REGISTRY_UNLOCK();

    /* 在锁外执行格式化操作（不访问共享数据） */
    snprintf(doc, size,
             "## %s\n\n"
             "**类别**: %s\n\n"
             "**描述**: %s\n\n"
             "**输入端口**: %d\n\n"
             "**输出端口**: %d\n",
             entry_name, entry_cat, entry_desc, entry_in, entry_out);

    return doc;
}

/* ==================== 统计信息 ==================== */

void preset_blocks_get_stats(int *total_count, int *by_category) {
    PRESET_REGISTRY_LOCK();

    if (total_count) {
        *total_count = g_preset_registry.count;
    }

    if (by_category) {
        memset(by_category, 0, sizeof(int) * PRESET_EXT_CATEGORY_COUNT);
        for (int i = 0; i < g_preset_registry.count; i++) {
            if (g_preset_registry.entries[i].category < PRESET_EXT_CATEGORY_COUNT) {
                by_category[g_preset_registry.entries[i].category]++;
            }
        }
    }

    PRESET_REGISTRY_UNLOCK();
}

void preset_blocks_print_stats(void) {
    int total, by_category[PRESET_EXT_CATEGORY_COUNT];
    preset_blocks_get_stats(&total, by_category);

    printf("=== Lv-00 预设函数块统计 ===\n");
    printf("总计: %d 个预设\n\n", total);

    for (int i = 0; i < PRESET_EXT_CATEGORY_COUNT; i++) {
        if (by_category[i] > 0) {
            printf("  %s: %d 个\n", preset_extended_category_to_string((PresetExtendedCategory) i), by_category[i]);
        }
    }
    printf("===========================\n");
}

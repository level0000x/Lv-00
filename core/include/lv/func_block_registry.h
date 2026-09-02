/**
 * @file func_block_registry.h
 * @brief 预设函数块注册系统
 *
 * 提供内置的几何构造预设函数块，用于理论数学研究。
 * 预设函数块封装了常见的几何构造操作，用户可以通过名称查找和实例化。
 *
 * 设计原则：
 * - 所有预设函数块在首次调用 func_block_registry_init() 时惰性创建
 * - 注册表使用名称作为唯一键，支持按名称查找
 * - 预设函数块是只读模板，实例化时自动创建副本
 * - 支持用户注册自定义预设函数块
 *
 * 【查找性能优化】
 * func_block_registry_find() 和 func_block_registry_lookup() 内部使用
 * 基于 FNV-1a 字符串哈希的哈希表实现 O(1) 平均查找复杂度。
 * 哈希表在 func_block_registry_init() 调用时自动构建，在每次
 * func_block_register() / func_block_registry_unregister() 调用后
 * 延迟标记为脏，下次查找时自动重建。所有哈希表操作受互斥锁保护。
 *
 * func_block_registry_find_by_category() 按类别遍历线性数组，
 * 其 O(n) 复杂度对分类查询场景是可接受的。
 */

#ifndef lv_FUNC_BLOCK_REGISTRY_H
#define lv_FUNC_BLOCK_REGISTRY_H

#include <stdbool.h>

#include "func_block.h"
#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */

#ifdef __cplusplus
extern "C" {
#endif

/* ============== 预设函数块参数类型系统（前置声明，正式定义在 func_block_preset.h） ============== */

#ifndef lv_PRESET_PARAM_TYPE_ENUM_DEFINED
#define lv_PRESET_PARAM_TYPE_ENUM_DEFINED
typedef enum {
    PARAM_TYPE_POINT,
    PARAM_TYPE_LINE,
    PARAM_TYPE_SEGMENT,
    PARAM_TYPE_RAY,
    PARAM_TYPE_CIRCLE,
    PARAM_TYPE_ARC,
    PARAM_TYPE_POLYGON,
    PARAM_TYPE_REGION,
    PARAM_TYPE_ANGLE,
    PARAM_TYPE_VECTOR,
    PARAM_TYPE_SCALAR,
    PARAM_TYPE_BOOLEAN,
    PARAM_TYPE_CURVE,
    PARAM_TYPE_SURFACE,
    PARAM_TYPE_ANY,
    PARAM_TYPE_VARIADIC
} PresetParamType;
#endif

#ifndef lv_PARAM_CONSTRAINT_TYPE_ENUM_DEFINED
#define lv_PARAM_CONSTRAINT_TYPE_ENUM_DEFINED
typedef enum {
    PARAM_CONSTRAINT_NONE,
    PARAM_CONSTRAINT_NON_COLLINEAR,
    PARAM_CONSTRAINT_NON_COPLANAR,
    PARAM_CONSTRAINT_DISTINCT,
    PARAM_CONSTRAINT_POSITIVE,
    PARAM_CONSTRAINT_NON_ZERO,
    PARAM_CONSTRAINT_UNIT,
    PARAM_CONSTRAINT_IN_RANGE
} ParamConstraintType;
#endif

#ifndef lv_PARAM_CONSTRAINT_DEFINED
#define lv_PARAM_CONSTRAINT_DEFINED
typedef struct {
    ParamConstraintType type;
    double min_val;
    double max_val;
    const char *description;
} ParamConstraint;
#endif

#ifndef lv_PRESET_PARAM_DEF_DEFINED
#define lv_PRESET_PARAM_DEF_DEFINED
typedef struct {
    const char *name;
    const char *description;
    PresetParamType type;
    bool is_optional;
    bool is_output;
    int index;
    ParamConstraint *constraints;
    int constraint_count;
} PresetParamDef;
#endif

#ifndef lv_PRESET_PROPERTY_ENUM_DEFINED
#define lv_PRESET_PROPERTY_ENUM_DEFINED
typedef enum {
    PRESET_PROPERTY_NONE = 0,
    PRESET_PROPERTY_IDEMPOTENT = 1 << 0,
    PRESET_PROPERTY_INVOLUTIVE = 1 << 1,
    PRESET_PROPERTY_COMMUTATIVE = 1 << 2,
    PRESET_PROPERTY_ASSOCIATIVE = 1 << 3,
    PRESET_PROPERTY_LINEAR = 1 << 4,
    PRESET_PROPERTY_CONTINUOUS = 1 << 5,
    PRESET_PROPERTY_DETERMINISTIC = 1 << 6,
    PRESET_PROPERTY_CONSTRUCTIVE = 1 << 7,
    PRESET_PROPERTY_REVERSIBLE = 1 << 8
} PresetProperty;
#endif

#ifndef lv_PRESET_COMPLEXITY_ENUM_DEFINED
#define lv_PRESET_COMPLEXITY_ENUM_DEFINED
typedef enum {
    COMPLEXITY_O1,
    COMPLEXITY_OLOGN,
    COMPLEXITY_ON,
    COMPLEXITY_ONLOGN,
    COMPLEXITY_ON2,
    COMPLEXITY_ON3,
    COMPLEXITY_UNKNOWN
} PresetComplexity;
#endif

/* ============== 预设函数块类别 ============== */

/**
 * @brief 预设函数块的分类枚举
 *
 * 用于对内置预设函数块进行分类管理，支持按类别筛选查找。
 */
typedef enum {
    PRESET_CATEGORY_CONSTRUCTION,          /* 几何构造 */
    PRESET_CATEGORY_MEASUREMENT,           /* 度量计算 */
    PRESET_CATEGORY_TRANSFORMATION,        /* 几何变换 */
    PRESET_CATEGORY_ALGEBRAIC,             /* 代数运算 */
    PRESET_CATEGORY_LOGIC,                 /* 逻辑推导 */
    PRESET_CATEGORY_ANALYSIS,              /* 分析运算 */
    PRESET_CATEGORY_NUMBER_THEORY,         /* 数论运算 */
    PRESET_CATEGORY_GROUP_THEORY,          /* 群论运算 */
    PRESET_CATEGORY_RING_THEORY,           /* 环论运算 */
    PRESET_CATEGORY_FIELD_THEORY,          /* 域论运算 */
    PRESET_CATEGORY_TOPOLOGY,              /* 拓扑构造 */
    PRESET_CATEGORY_LINEAR_ALGEBRA,        /* 线性代数 */
    PRESET_CATEGORY_COMBINATORICS,         /* 组合数学 */
    PRESET_CATEGORY_COMPLEX_ANALYSIS,      /* 复分析 */
    PRESET_CATEGORY_PROBABILITY,           /* 概率统计 */
    PRESET_CATEGORY_GEOMETRY,              /* 几何（含三维/高级几何） */
    PRESET_CATEGORY_ALGEBRA,               /* 代数（含线性代数/多项式） */
    PRESET_CATEGORY_CATEGORY_THEORY,       /* 范畴论 */
    PRESET_CATEGORY_SET_THEORY,            /* 集合论 */
    PRESET_CATEGORY_CUSTOM,                /* 自定义/扩展类别 */
    PRESET_CATEGORY_GRAPH_THEORY,          /* 图论 */
    PRESET_CATEGORY_DIFFERENTIAL_GEOMETRY, /* 微分几何 */
    PRESET_CATEGORY_NUMERICAL,             /* 数值分析 */
    PRESET_CATEGORY_OPTIMIZATION,          /* 优化理论 */
    PRESET_CATEGORY_MATH_LOGIC,            /* 数理逻辑 */
    PRESET_CATEGORY_COUNT                  /* 类别总数（哨兵值） */
} PresetCategory;

#ifndef lv_PRESET_METADATA_DEFINED
#define lv_PRESET_METADATA_DEFINED
typedef struct {
    const char *name;
    const char *description;
    const char *mathematical_def;
    PresetCategory category;
    PresetProperty properties;
    PresetComplexity complexity;

    PresetParamDef *input_params;
    int input_count;
    PresetParamDef *output_params;
    int output_count;

    const char **preconditions;
    int precondition_count;

    const char **postconditions;
    int postcondition_count;

    const char **related_presets;
    int related_count;

    int version_major;
    int version_minor;
    int version_patch;
} PresetMetadata;
#endif

/* ============== 预设函数块条目 ============== */

/**
 * @brief 预设函数块注册条目
 *
 * 每个条目包含一个预设函数块的元数据及其模板函数块。
 * 模板函数块是只读的，实例化时应通过 func_block_copy 创建副本。
 *
 * 【引用计数机制 (v3.4.1)】
 * 为防止悬空指针风险，PresetEntry 包含引用计数字段。
 * - 初始化时 ref_count = 1（注册表持有引用）
 * - 每次 lookup 返回深拷贝时，ref_count++
 * - [copy] 深拷贝使用后由调用者负责释放，不影响 ref_count
 * - cleanup 时，仅当 ref_count == 1 时才释放模板函数块
 * - 如果外部仍有引用，cleanup 仅释放注册表资源，模板函数块保留
 *
 * 这样的设计确保：
 * 1. 注册表清理不会产生悬空指针
 * 2. 外部代码可以安全使用 lookup 返回的深拷贝
 * 3. 注册表可以安全重新初始化
 */
typedef struct {
    char *name;                   /* 预设名称（唯一键） */
    char *description;            /* 描述 */
    PresetCategory category;      /* 类别 */
    FuncBlock *template_fb;       /* 模板函数块（只读） */
    int ref_count;                /* 引用计数 (v3.4.1)：模板函数块的引用数量 */
    PresetMetadata metadata;      /* 预设元数据 */
    PresetParamDef *input_params; /* 输入参数定义数组 */
} PresetEntry;

/* ============== 注册表生命周期 ============== */

/**
 * @brief 初始化注册表并惰性加载内置预设
 *
 * 首次调用时创建全局注册表并注册所有内置预设函数块。
 * 后续调用直接返回 true（幂等操作）。
 *
 * @return true 初始化成功，false 内存不足
 */
lv_PUBLIC_API bool func_block_registry_init(void);

/**
 * @brief 清理注册表并释放所有资源
 *
 * 销毁全局注册表中的所有条目，释放模板函数块和字符串内存。
 * 调用后需重新 init 才能使用注册表。
 */
lv_PUBLIC_API void lv_func_block_registry_cleanup(void);

/* ============== 注册与查找 ============== */

/**
 * @brief 注册一个自定义预设函数块
 *
 * 将用户提供的函数块注册到全局注册表中。
 * 注册时会创建 name 和 description 的副本，以及 fb 的深拷贝（[copy] 语义，
 * 见 docs/architecture/memory-ownership.md K10/F39）。
 * 如果同名预设已存在，则返回 false。
 *
 * @param name        预设名称（唯一键，不可为 NULL）
 * @param description 描述文本（可为 NULL）
 * @param category    预设类别
 * @param fb          模板函数块（不可为 NULL；注册表深拷贝之，调用者仍持有
 *                    原 fb 并负责销毁——注册失败时 fb 同样归调用方，不泄漏）
 * @return true 注册成功，false 参数无效或同名已存在或内存不足
 */
lv_PUBLIC_API bool func_block_register(const char *name, const char *description, PresetCategory category,
                                       FuncBlock *fb);

/**
 * @brief 按名称查找预设函数块并返回深拷贝（[copy] 语义，memory-ownership.md）
 *
 * 在注册表中查找指定名称的预设，找到后通过 func_block_copy
 * [copy] 创建一个独立的副本返回给调用者。调用者负责销毁返回的副本。
 *
 * 内部使用 FNV-1a 哈希表实现 O(1) 平均查找复杂度。
 * 哈希表脏时自动触发延迟重建。
 *
 * @param name 预设名称
 * @return 函数块深拷贝（[copy] 调用者负责释放），未找到或失败返回 NULL
 */
lv_PUBLIC_API FuncBlock *func_block_registry_lookup(const char *name);

/**
 * @brief 按名称查找预设条目（不创建副本）
 *
 * 返回注册表中的条目指针，用于读取元数据。
 * 注意：返回的指针指向注册表内部数据，不要修改或释放。
 *
 * 内部使用 FNV-1a 哈希表实现 O(1) 平均查找复杂度。
 * 哈希表脏时自动触发延迟重建。
 *
 * @param name 预设名称
 * @return 条目指针，未找到返回 NULL
 */
lv_PUBLIC_API PresetEntry *func_block_registry_find(const char *name);

/**
 * @brief 按类别查找预设条目
 *
 * 将指定类别的所有预设条目收集到输出数组中。
 *
 * @param category    目标类别
 * @param out_entries 输出条目指针数组（由调用者分配）
 * @param max_count   输出数组的最大容量
 * @return 实际找到的条目数量（可能超过 max_count，此时仅返回前 max_count 个）
 */
lv_PUBLIC_API int func_block_registry_find_by_category(PresetCategory category, PresetEntry **out_entries,
                                                       int max_count);

/* ============== 辅助函数 ============== */

/**
 * @brief 将预设类别枚举转换为中文可读字符串
 *
 * @param cat 预设类别
 * @return 类别的中文名称，未知类别返回 "未知类别"
 */
#ifndef lv_PRESET_CATEGORY_TO_STRING_DECLARED
#define lv_PRESET_CATEGORY_TO_STRING_DECLARED
lv_PUBLIC_API const char *preset_category_to_string(PresetCategory cat);
#endif /* lv_PRESET_CATEGORY_TO_STRING_DECLARED */

/**
 * @brief 从字符串解析预设类别枚举值
 *
 * 支持中文名称和英文名称两种格式的解析。
 *
 * @param str      类别名称字符串（中文或英文）
 * @param category 输出：解析后的类别枚举值
 * @return true 解析成功，false 字符串无法识别或参数无效
 */
lv_PUBLIC_API bool preset_category_from_string(const char *str, PresetCategory *category);

/**
 * @brief 获取注册表中当前条目总数
 *
 * @return 条目数量
 */
lv_PUBLIC_API int func_block_registry_get_count(void);

/**
 * @brief 注销指定名称的函数块
 * @param name 要注销的函数块名称
 * @return 0 成功，-1 未找到
 */
lv_PUBLIC_API int func_block_registry_unregister(const char *name);

#ifdef __cplusplus
}
#endif

/* ============================================================
 * 向后兼容别名（旧名称 → lv_ 前缀新名称）
 * ============================================================ */
#define func_block_registry_cleanup lv_func_block_registry_cleanup
#endif /* lv_FUNC_BLOCK_REGISTRY_H */

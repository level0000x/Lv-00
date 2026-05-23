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
 */

#ifndef LV00_FUNC_BLOCK_REGISTRY_H
#define LV00_FUNC_BLOCK_REGISTRY_H

#include "func_block.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============== 预设函数块类别 ============== */

/**
 * @brief 预设函数块的分类枚举
 *
 * 用于对内置预设函数块进行分类管理，支持按类别筛选查找。
 */
typedef enum {
    PRESET_CATEGORY_CONSTRUCTION,   /* 几何构造 */
    PRESET_CATEGORY_MEASUREMENT,    /* 度量计算 */
    PRESET_CATEGORY_TRANSFORMATION, /* 几何变换 */
    PRESET_CATEGORY_ALGEBRAIC,      /* 代数运算 */
    PRESET_CATEGORY_LOGIC,          /* 逻辑推导 */
    PRESET_CATEGORY_ANALYSIS,       /* 分析运算 */
    PRESET_CATEGORY_NUMBER_THEORY,  /* 数论运算 */
    PRESET_CATEGORY_GROUP_THEORY,   /* 群论运算 */
    PRESET_CATEGORY_RING_THEORY,    /* 环论运算 */
    PRESET_CATEGORY_FIELD_THEORY,   /* 域论运算 */
    PRESET_CATEGORY_TOPOLOGY,       /* 拓扑构造 */
    PRESET_CATEGORY_LINEAR_ALGEBRA, /* 线性代数 */
    PRESET_CATEGORY_COMBINATORICS,  /* 组合数学 */
    PRESET_CATEGORY_COMPLEX_ANALYSIS, /* 复分析 */
    PRESET_CATEGORY_PROBABILITY,    /* 概率统计 */
    PRESET_CATEGORY_GEOMETRY,       /* 几何（含三维/高级几何） */
    PRESET_CATEGORY_ALGEBRA,        /* 代数（含线性代数/多项式） */
    PRESET_CATEGORY_CATEGORY_THEORY, /* 范畴论 */
    PRESET_CATEGORY_SET_THEORY,     /* 集合论 */
    PRESET_CATEGORY_CUSTOM,         /* 自定义/扩展类别 */
    PRESET_CATEGORY_GRAPH_THEORY,   /* 图论 */
    PRESET_CATEGORY_DIFFERENTIAL_GEOMETRY, /* 微分几何 */
    PRESET_CATEGORY_NUMERICAL,      /* 数值分析 */
    PRESET_CATEGORY_OPTIMIZATION,   /* 优化理论 */
    PRESET_CATEGORY_MATH_LOGIC,     /* 数理逻辑 */
    PRESET_CATEGORY_COUNT           /* 类别总数（哨兵值） */
} PresetCategory;

/* ============== 预设函数块条目 ============== */

/**
 * @brief 预设函数块注册条目
 *
 * 每个条目包含一个预设函数块的元数据及其模板函数块。
 * 模板函数块是只读的，实例化时应通过 func_block_copy 创建副本。
 */
typedef struct {
    char *name;              /* 预设名称（唯一键） */
    char *description;       /* 描述 */
    PresetCategory category; /* 类别 */
    FuncBlock *template_fb;  /* 模板函数块（只读） */
} PresetEntry;

/* ============== 注册表结构 ============== */

/**
 * @brief 预设函数块注册表
 *
 * 管理所有已注册的预设函数块。内置预设在首次调用 init 时惰性创建，
 * 用户可通过 register 接口添加自定义预设。
 */
typedef struct {
    PresetEntry *entries;    /* 条目数组 */
    int count;               /* 当前条目数 */
    int capacity;            /* 数组容量 */
    bool initialized;        /* 是否已初始化内置预设 */
} FuncBlockRegistry;

/* ============== 注册表生命周期 ============== */

/**
 * @brief 初始化注册表并惰性加载内置预设
 *
 * 首次调用时创建全局注册表并注册所有内置预设函数块。
 * 后续调用直接返回 true（幂等操作）。
 *
 * @return true 初始化成功，false 内存不足
 */
bool func_block_registry_init(void);

/**
 * @brief 清理注册表并释放所有资源
 *
 * 销毁全局注册表中的所有条目，释放模板函数块和字符串内存。
 * 调用后需重新 init 才能使用注册表。
 */
void func_block_registry_cleanup(void);

/* ============== 注册与查找 ============== */

/**
 * @brief 注册一个自定义预设函数块
 *
 * 将用户提供的函数块注册到全局注册表中。
 * 注册时会创建 name 和 description 的副本，以及 fb 的深拷贝。
 * 如果同名预设已存在，则返回 false。
 *
 * @param name        预设名称（唯一键，不可为 NULL）
 * @param description 描述文本（可为 NULL）
 * @param category    预设类别
 * @param fb          模板函数块（不可为 NULL，注册后由注册表接管管理）
 * @return true 注册成功，false 参数无效或同名已存在或内存不足
 */
bool func_block_register(const char *name, const char *description,
                         PresetCategory category, FuncBlock *fb);

/**
 * @brief 按名称查找预设函数块并返回深拷贝
 *
 * 在注册表中查找指定名称的预设，找到后通过 func_block_copy
 * 创建一个独立的副本返回给调用者。调用者负责销毁返回的副本。
 *
 * @param name 预设名称
 * @return 函数块深拷贝（调用者负责释放），未找到或失败返回 NULL
 */
FuncBlock *func_block_registry_lookup(const char *name);

/**
 * @brief 按名称查找预设条目（不创建副本）
 *
 * 返回注册表中的条目指针，用于读取元数据。
 * 注意：返回的指针指向注册表内部数据，不要修改或释放。
 *
 * @param name 预设名称
 * @return 条目指针，未找到返回 NULL
 */
PresetEntry *func_block_registry_find(const char *name);

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
int func_block_registry_find_by_category(PresetCategory category,
                                         PresetEntry **out_entries,
                                         int max_count);

/* ============== 辅助函数 ============== */

/**
 * @brief 将预设类别枚举转换为中文可读字符串
 *
 * @param cat 预设类别
 * @return 类别的中文名称，未知类别返回 "未知类别"
 */
const char *preset_category_to_string(PresetCategory cat);

/**
 * @brief 从字符串解析预设类别枚举值
 *
 * 支持中文名称和英文名称两种格式的解析。
 *
 * @param str      类别名称字符串（中文或英文）
 * @param category 输出：解析后的类别枚举值
 * @return true 解析成功，false 字符串无法识别或参数无效
 */
bool preset_category_from_string(const char *str, PresetCategory *category);

/**
 * @brief 获取注册表中当前条目总数
 *
 * @return 条目数量
 */
int func_block_registry_get_count(void);

#ifdef __cplusplus
}
#endif

#endif /* LV00_FUNC_BLOCK_REGISTRY_H */

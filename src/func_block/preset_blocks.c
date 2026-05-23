/**
 * @file preset_blocks.c
 * @brief 预设函数块扩展系统 - 实现
 *
 * 实现扩展的预设函数块注册系统，为理论数学研究
 * 提供更丰富的预设函数块集合。
 *
 * 内存管理：
 * - 使用 lv00_malloc / lv00_free / lv00_realloc 进行内存管理
 * - 使用 lv00_strdup 进行字符串复制
 * - cleanup 时释放所有资源
 */

#include "preset_blocks.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "preset_common.h"

/* ==================== 外部模块注册函数声明 ==================== */

extern bool preset_basic_geometry_register(void);
extern bool preset_transformations_register(void);
extern bool preset_measurements_register(void);
extern bool preset_polygons_register(void);
extern bool preset_algebraic_register(void);
extern bool preset_number_theory_register(void);
extern bool preset_group_theory_register(void);
extern bool preset_topology_register(void);
extern bool preset_analysis_register(void);
extern bool preset_combinatorics_register(void);
extern bool preset_graph_theory_register(void);
extern bool preset_probability_register(void);
extern bool preset_numerical_register(void);
extern bool preset_optimization_register(void);
extern bool preset_advanced_geometry_register(void);
extern bool preset_geometry_3d_register(void);
extern bool preset_ring_theory_register(void);
extern bool preset_field_theory_register(void);
extern bool preset_linear_algebra_register(void);
extern bool preset_polynomial_register(void);
extern bool preset_set_theory_register(void);
extern bool preset_logic_advanced_register(void);
extern bool preset_category_theory_register(void);
extern bool preset_complex_analysis_register(void);
extern bool preset_measure_theory_register(void);
extern bool preset_order_theory_register(void);
extern bool preset_functional_analysis_adv_register(void);
extern bool preset_functional_analysis_register(void);
extern bool preset_algebraic_topology_adv_register(void);
extern bool preset_mathematical_logic_register(void);
extern bool preset_matrix_register(void);
extern bool preset_calculus_register(void);
extern bool preset_basic_math_register(void);
extern bool preset_math_logic_register(void);
extern bool preset_trigonometry_register(void);
extern bool preset_differential_geometry_register(void);
extern bool preset_differential_equations_register(void);
extern bool preset_statistics_register(void);
extern bool preset_integral_transforms_register(void);
extern bool preset_representation_theory_register(void);
extern bool preset_algebraic_topology_register(void);
/* ---- v10.0 新增：格论模块 ---- */
extern bool preset_lattice_theory_register(void);
/* ---- v10.0 新增：进阶范畴论模块 ---- */
extern bool preset_category_theory_adv_register(void);
/* ---- v10.0 新增：特殊函数模块 ---- */
extern bool preset_special_functions_register(void);

/* ==================== 命名常量 ==================== */

/** 注册表初始容量 */
#define PRESET_REGISTRY_INITIAL_CAPACITY 64

/** 数组扩容增长因子 */
#define PRESET_REGISTRY_GROWTH_FACTOR 2

/** 预设函数块 ID 起始偏移（引用 lv00_internal.h 中的统一定义） */
#define PRESET_FB_ID_OFFSET LV00_PRESET_ID_OFFSET

/* ==================== 内部数据结构 ==================== */

/**
 * @brief 内部预设条目结构
 *
 * 存储预设的完整信息，包括元数据和模板函数块。
 */
typedef struct {
    char *name;                           /* 预设名称（唯一键） */
    char *description;                    /* 中文描述 */
    char *mathematical_definition;        /* 数学定义 */
    PresetExtendedCategory category;      /* 扩展类别 */
    int input_count;                      /* 输入端口数量 */
    int output_count;                     /* 输出端口数量 */
    bool has_selector;                    /* 是否需要多解选择器 */
    char *preconditions;                  /* 前置条件描述 */
    char *example_usage;                  /* 使用示例 */
    FuncBlock *template_fb;               /* 模板函数块 */
    int id;                               /* 预设ID */

    /* 简化注册接口扩展字段 */
    PresetType *input_types;               /* 输入类型数组 */
    int input_type_count;                 /* 输入类型数量 */
    PresetType output_type;                /* 输出类型 */
    char *complexity;                     /* 时间复杂度描述 */
    bool is_constructive;                 /* 是否构造性 */
    bool is_reversible;                   /* 是否可逆 */
} InternalPresetEntry;

/**
 * @brief 扩展预设函数块注册表
 */
typedef struct {
    InternalPresetEntry *entries;         /* 条目数组 */
    int count;                            /* 当前条目数 */
    int capacity;                         /* 数组容量 */
    bool initialized;                     /* 是否已初始化 */
    int next_preset_id;                   /* 下一个预设ID */
} ExtendedPresetRegistry;

/* ==================== 全局注册表 ==================== */

/** 全局扩展预设函数块注册表（单例） */
static ExtendedPresetRegistry g_preset_registry = {
    .entries     = NULL,
    .count       = 0,
    .capacity    = 0,
    .initialized = false,
    .next_preset_id = PRESET_FB_ID_OFFSET
};

/* 线程安全：注册表互斥锁
 *
 * 使用原子操作+双重检查锁定模式消除 TOCTOU 竞态条件。
 * 锁的初始化在 preset_blocks_init() 中提前完成，
 * LOCK() 宏不再执行惰性初始化，避免多线程竞态。
 */
#ifdef _WIN32
#include <windows.h>
static CRITICAL_SECTION g_preset_registry_lock;
static volatile LONG g_preset_registry_lock_initialized = 0;

/** 
 * @brief 初始化预设注册表锁（线程安全，仅执行一次）
 *
 * 使用 InterlockedCompareExchange 原子操作确保
 * 多线程环境下仅初始化一次临界区，消除 TOCTOU 竞态条件。
 */
static void preset_registry_lock_init_once(void)
{
    if (InterlockedCompareExchange(&g_preset_registry_lock_initialized, 0, 0) == 0) {
        /* volatile 重读，避免编译器优化消除检查 */
        LONG expected = 0;
        if (InterlockedCompareExchange(&g_preset_registry_lock_initialized, 1, 0) == 0) {
            InitializeCriticalSection(&g_preset_registry_lock);
            /* 内存屏障：确保临界区初始化对后续所有线程可见 */
            MemoryBarrier();
            InterlockedExchange(&g_preset_registry_lock_initialized, 2);
        } else {
            /* 其他线程正在初始化，自旋等待完成 */
            while (InterlockedCompareExchange(&g_preset_registry_lock_initialized, 0, 0) != 2) {
                Sleep(0);  /* 让出时间片 */
            }
        }
    }
}

#define PRESET_REGISTRY_LOCK() \
    do { \
        preset_registry_lock_init_once(); \
        EnterCriticalSection(&g_preset_registry_lock); \
    } while (0)
#define PRESET_REGISTRY_UNLOCK() LeaveCriticalSection(&g_preset_registry_lock)
#else
#include <pthread.h>
static pthread_mutex_t g_preset_registry_lock = PTHREAD_MUTEX_INITIALIZER;
#define PRESET_REGISTRY_LOCK() pthread_mutex_lock(&g_preset_registry_lock)
#define PRESET_REGISTRY_UNLOCK() pthread_mutex_unlock(&g_preset_registry_lock)
#endif

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 确保注册表数组有足够的容量
 *
 * 当 count >= capacity 时，以 PRESET_REGISTRY_GROWTH_FACTOR 倍率扩容。
 * 包含整数溢出检查，参考 func_block_registry.c 中的 ensure_registry_capacity。
 *
 * @return true 扩容成功或无需扩容，false 内存不足或溢出
 */
static bool ensure_preset_registry_capacity(void)
{
    if (g_preset_registry.count < g_preset_registry.capacity) {
        return true;
    }

    int new_capacity;
    if (g_preset_registry.capacity == 0) {
        /* 首次分配，使用初始容量 */
        new_capacity = PRESET_REGISTRY_INITIAL_CAPACITY;
    } else {
        /* 整数溢出检查：确保 capacity * PRESET_REGISTRY_GROWTH_FACTOR 不超过 INT_MAX */
        if (g_preset_registry.capacity > INT_MAX / PRESET_REGISTRY_GROWTH_FACTOR) {
            return false;  /* 溢出，无法继续扩容 */
        }
        new_capacity = g_preset_registry.capacity * PRESET_REGISTRY_GROWTH_FACTOR;
    }

    /* 检查 new_capacity * sizeof(InternalPresetEntry) 是否超过 SIZE_MAX */
    if ((size_t)new_capacity > SIZE_MAX / sizeof(InternalPresetEntry)) {
        return false;  /* 内存大小溢出 */
    }

    InternalPresetEntry *new_entries = lv00_realloc(
        g_preset_registry.entries, (size_t)new_capacity * sizeof(InternalPresetEntry));
    if (!new_entries) {
        return false;
    }

    g_preset_registry.entries  = new_entries;
    g_preset_registry.capacity = new_capacity;
    return true;
}

/**
 * @brief 释放内部预设条目的资源
 *
 * @param entry 预设条目指针
 */
static void free_internal_preset_entry(InternalPresetEntry *entry)
{
    if (!entry) return;

    lv00_free((void **)&entry->name);
    lv00_free((void **)&entry->description);
    lv00_free((void **)&entry->mathematical_definition);
    lv00_free((void **)&entry->preconditions);
    lv00_free((void **)&entry->example_usage);
    lv00_free((void **)&entry->complexity);
    lv00_free((void **)&entry->input_types);

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
static int find_preset_index(const char *name)
{
    if (!name) return -1;

    for (int i = 0; i < g_preset_registry.count; i++) {
        if (g_preset_registry.entries[i].name &&
            strcmp(g_preset_registry.entries[i].name, name) == 0) {
            return i;
        }
    }

    return -1;
}

/* ==================== 公共 API 实现 ==================== */

bool preset_blocks_init(void)
{
    /* 幂等操作：已初始化则直接返回 */
    if (g_preset_registry.initialized) {
        return true;
    }

    /* 确保基础注册表已初始化（锁初始化由 LOCK 宏自动完成） */
    if (!func_block_registry_init()) {
        return false;
    }

    /* ============================================================
     * 注册各模块的预设函数块
     * 
     * 模块注册顺序：
     * 1. 基础几何构造
     * 2. 几何变换
     * 3. 度量计算
     * 4. 多边形构造
     * 5. 代数运算
     * 6. 数论运算（新增）
     * 7. 群论运算（新增）
     * 8. 拓扑学运算（新增）
     * 9. 分析学运算（新增）
     * 10. 微分方程（新增 v9.0）
     * 11. 组合数学（新增 v5.0）
     * 12. 三角函数（新增 v9.0）
     * 13. 图论（新增 v5.0）
     * 14. 概率统计（新增 v5.0）
     * 15. 统计学（新增 v9.0）
     * 16. 数值分析（新增 v5.0）
     * 17. 微分几何（新增 v9.0）
     * 18. 优化理论（新增 v5.0）
     * 19. 高级几何构造（新增）
     * 20. 三维几何构造（新增）
     * 21. 环论运算（新增 v5.0）
     * 22. 域论运算（新增 v5.0）
     * 23. 线性代数运算（新增 v5.0）
     * 24. 多项式理论运算（新增 v5.0）
     * 25. 集合论模块（新增 v6.0）
     * 26. 高级逻辑模块（新增 v6.0）
     * 27. 范畴论模块（新增 v6.0）
     * 28. 复分析模块（新增 v6.0）
     * 29. 测度论模块（新增 v7.0）
     * 30. 序理论模块（新增 v7.0）
     * 31. 泛函分析进阶模块（新增 v7.0）
     * 32. 泛函分析模块（新增 v9.0）
     * 33. 代数拓扑进阶模块（新增 v7.0）
     * 34. 数学逻辑模块（新增 v8.0）
     * 35. 矩阵运算模块（新增 v8.0）
     * 36. 微积分模块（新增 v8.0）
     * 37. 基础数学模块（新增 v8.0）
     * 38. 进阶数学逻辑模块（新增 v8.0）
     * 39. 微分几何模块（新增 v9.0）—— 曲线论/曲面论/联络/测地线/张量分析
     * 40. 代数拓扑模块（新增 v9.0）—— 同调/上同调/高阶同伦/单纯复形
     * 41. 积分变换模块（新增 v9.0）—— 傅里叶/拉普拉斯/Z变换/梅林/希尔伯特
     * 42. 表示论模块（新增 v9.0）—— 群表示/特征标/不可约/诱导/李代数表示
     * 43. 特殊函数模块（新增 v10.0）—— Gamma/Beta/Bessel/正交多项式/Zeta，共20个
     * ============================================================ */

    /* 注册基础几何模块 */
    if (!preset_basic_geometry_register()) {
        LV00_LOG_WARNING("基础几何模块预设注册部分失败");
    }

    /* 注册几何变换模块 */
    if (!preset_transformations_register()) {
        LV00_LOG_WARNING("几何变换模块预设注册部分失败");
    }

    /* 注册度量计算模块 */
    if (!preset_measurements_register()) {
        LV00_LOG_WARNING("度量计算模块预设注册部分失败");
    }

    /* 注册多边形构造模块 */
    if (!preset_polygons_register()) {
        LV00_LOG_WARNING("多边形构造模块预设注册部分失败");
    }

    /* 注册代数运算模块 */
    if (!preset_algebraic_register()) {
        LV00_LOG_WARNING("代数运算模块预设注册部分失败");
    }

    /* 注册数论运算模块（新增） */
    if (!preset_number_theory_register()) {
        LV00_LOG_WARNING("数论运算模块预设注册部分失败");
    }

    /* 注册群论运算模块（新增） */
    if (!preset_group_theory_register()) {
        LV00_LOG_WARNING("群论运算模块预设注册部分失败");
    }

    /* 注册拓扑学运算模块（新增） */
    if (!preset_topology_register()) {
        LV00_LOG_WARNING("拓扑学运算模块预设注册部分失败");
    }

    /* 注册分析学运算模块（新增） */
    if (!preset_analysis_register()) {
        LV00_LOG_WARNING("分析学模块预设注册部分失败");
    }

    /* 注册微分方程模块（新增 v9.0） */
    if (!preset_differential_equations_register()) {
        LV00_LOG_WARNING("微分方程模块预设注册部分失败");
    }

    /* 注册特殊函数模块（新增 v10.0）—— Gamma/Beta/Bessel/正交多项式/Zeta */
    if (!preset_special_functions_register()) {
        LV00_LOG_WARNING("特殊函数模块预设注册部分失败");
    }

    /* 注册组合数学模块（新增 v5.0） */
    if (!preset_combinatorics_register()) {
        LV00_LOG_WARNING("组合数学模块预设注册部分失败");
    }

    /* 注册三角函数模块（新增 v9.0） */
    if (!preset_trigonometry_register()) {
        LV00_LOG_WARNING("三角函数模块预设注册部分失败");
    }

    /* 注册图论模块（新增 v5.0） */
    if (!preset_graph_theory_register()) {
        LV00_LOG_WARNING("图论模块预设注册部分失败");
    }

    /* 注册概率统计模块（新增 v5.0） */
    if (!preset_probability_register()) {
        LV00_LOG_WARNING("概率统计模块预设注册部分失败");
    }

    /* 注册统计学模块（新增 v9.0） */
    if (!preset_statistics_register()) {
        LV00_LOG_WARNING("统计学模块预设注册部分失败");
    }

    /* 注册数值分析模块（新增 v5.0） */
    if (!preset_numerical_register()) {
        LV00_LOG_WARNING("数值分析模块预设注册部分失败");
    }

    /* 注册微分几何模块（新增 v9.0） */
#ifndef LV00_EXCLUDE_BROKEN_PRESETS
    if (!preset_differential_geometry_register()) {
        LV00_LOG_WARNING("微分几何模块预设注册部分失败");
    }
#endif /* LV00_EXCLUDE_BROKEN_PRESETS */

    /* 注册优化理论模块（新增 v5.0） */
    if (!preset_optimization_register()) {
        LV00_LOG_WARNING("优化理论模块预设注册部分失败");
    }

    /* 注册高级几何模块（新增） */
    if (!preset_advanced_geometry_register()) {
        LV00_LOG_WARNING("高级几何模块预设注册部分失败");
    }

    /* 注册三维几何模块（新增） */
    if (!preset_geometry_3d_register()) {
        LV00_LOG_WARNING("三维几何模块预设注册部分失败");
    }

    /* 注册环论模块（新增 v5.0） */
    if (!preset_ring_theory_register()) {
        LV00_LOG_WARNING("环论模块预设注册部分失败");
    }

    /* 注册域论模块（新增 v5.0） */
    if (!preset_field_theory_register()) {
        LV00_LOG_WARNING("域论模块预设注册部分失败");
    }

    /* 注册线性代数模块（新增 v5.0） */
    if (!preset_linear_algebra_register()) {
        LV00_LOG_WARNING("线性代数模块预设注册部分失败");
    }

    /* 注册多项式理论模块（新增 v5.0） */
    if (!preset_polynomial_register()) {
        LV00_LOG_WARNING("多项式理论模块预设注册部分失败");
    }

    /* 注册集合论模块（新增 v6.0） */
    if (!preset_set_theory_register()) {
        LV00_LOG_WARNING("集合论模块预设注册部分失败");
    }

    /* 注册高级逻辑模块（新增 v6.0） */
    if (!preset_logic_advanced_register()) {
        LV00_LOG_WARNING("高级逻辑模块预设注册部分失败");
    }

    /* 注册范畴论模块（新增 v6.0） */
    if (!preset_category_theory_register()) {
        LV00_LOG_WARNING("范畴论模块预设注册部分失败");
    }

    /* 注册复分析模块（新增 v6.0） */
    if (!preset_complex_analysis_register()) {
        LV00_LOG_WARNING("复分析模块预设注册部分失败");
    }

    /* 注册测度论模块（新增 v7.0） */
    if (!preset_measure_theory_register()) {
        LV00_LOG_WARNING("测度论模块预设注册部分失败");
    }

    /* 注册序理论模块（新增 v7.0） */
    if (!preset_order_theory_register()) {
        LV00_LOG_WARNING("序理论模块预设注册部分失败");
    }

    /* 注册泛函分析进阶模块（新增 v7.0） */
    if (!preset_functional_analysis_adv_register()) {
        LV00_LOG_WARNING("泛函分析进阶模块预设注册部分失败");
    }

    /* 注册泛函分析模块（新增 v9.0） */
#ifndef LV00_EXCLUDE_BROKEN_PRESETS
    if (!preset_functional_analysis_register()) {
        LV00_LOG_WARNING("泛函分析模块预设注册部分失败");
    }
#endif /* LV00_EXCLUDE_BROKEN_PRESETS */

    /* 注册代数拓扑进阶模块（新增 v7.0） */
    if (!preset_algebraic_topology_adv_register()) {
        LV00_LOG_WARNING("代数拓扑进阶模块预设注册部分失败");
    }

    /* 注册数学逻辑模块（新增 v8.0） */
    if (!preset_mathematical_logic_register()) {
        LV00_LOG_WARNING("数学逻辑模块预设注册部分失败");
    }

    /* 注册矩阵运算模块（新增 v8.0） */
    if (!preset_matrix_register()) {
        LV00_LOG_WARNING("矩阵运算模块预设注册部分失败");
    }

    /* 注册微积分模块（新增 v8.0） */
    if (!preset_calculus_register()) {
        LV00_LOG_WARNING("微积分模块预设注册部分失败");
    }

    /* 注册基础数学模块（新增 v8.0） */
    if (!preset_basic_math_register()) {
        LV00_LOG_WARNING("基础数学模块预设注册部分失败");
    }

    /* 注册进阶数学逻辑模块（新增 v8.0） */
    if (!preset_math_logic_register()) {
        LV00_LOG_WARNING("进阶数学逻辑模块预设注册部分失败");
    }

    /* 注册代数拓扑模块（新增 v9.0）—— 同调/上同调/高阶同伦/单纯复形 */
    if (!preset_algebraic_topology_register()) {
        LV00_LOG_WARNING("代数拓扑模块预设注册部分失败");
    }

    /* 注册积分变换模块（新增 v9.0）—— 傅里叶/拉普拉斯/Z变换/梅林/希尔伯特 */
    if (!preset_integral_transforms_register()) {
        LV00_LOG_WARNING("积分变换模块预设注册部分失败");
    }

    /* 注册表示论模块（新增 v9.0）—— 群表示/特征标/不可约/诱导/李代数表示 */
    if (!preset_representation_theory_register()) {
        LV00_LOG_WARNING("表示论模块预设注册部分失败");
    }

    /* ---- v10.0 新增：格论模块接入 ---- */
    /* 格论模块（lattice_theory）：格基础运算/特殊格/格同态与表示，共30个预设 */
    if (!preset_lattice_theory_register()) {
        LV00_LOG_WARNING("格论模块预设注册部分失败");
    }

    /* ---- v10.0 新增：进阶范畴论模块接入 ---- */
    /* 进阶范畴论（category_theory_adv）：Yoneda引理/Kan扩张/单子/预层，共20个预设 */
    if (!preset_category_theory_adv_register()) {
        LV00_LOG_WARNING("进阶范畴论模块预设注册部分失败");
    }

    g_preset_registry.initialized = true;
    return true;
}

void preset_blocks_cleanup(void)
{
    PRESET_REGISTRY_LOCK();

    /* 释放所有条目的资源 */
    for (int i = 0; i < g_preset_registry.count; i++) {
        free_internal_preset_entry(&g_preset_registry.entries[i]);
    }

    /* 释放条目数组本身 */
    lv00_free((void **)&g_preset_registry.entries);

    /* 重置注册表状态 */
    g_preset_registry.count       = 0;
    g_preset_registry.capacity    = 0;
    g_preset_registry.initialized = false;
    g_preset_registry.next_preset_id = PRESET_FB_ID_OFFSET;

    PRESET_REGISTRY_UNLOCK();

#ifdef _WIN32
    /* 检查锁是否已初始化（原子标志值 2 表示完全初始化完成） */
    if (InterlockedCompareExchange(&g_preset_registry_lock_initialized, 0, 0) != 0) {
        DeleteCriticalSection(&g_preset_registry_lock);
        InterlockedExchange(&g_preset_registry_lock_initialized, 0);
    }
#endif
}

PresetBlockMetadata *preset_blocks_get_metadata(const char *name)
{
    PRESET_REGISTRY_LOCK();

    int idx = find_preset_index(name);
    if (idx < 0) {
        PRESET_REGISTRY_UNLOCK();
        return NULL;
    }

    InternalPresetEntry *entry = &g_preset_registry.entries[idx];

    PresetBlockMetadata *result = lv00_malloc(sizeof(PresetBlockMetadata));
    if (result) {
        result->name = entry->name;
        result->description = entry->description;
        result->mathematical_definition = entry->mathematical_definition;
        result->category = entry->category;
        result->input_count = entry->input_count;
        result->output_count = entry->output_count;
        result->has_selector = entry->has_selector;
        result->preconditions = entry->preconditions;
        result->example_usage = entry->example_usage;
    }

    PRESET_REGISTRY_UNLOCK();
    return result;
}

int preset_blocks_find_by_category(PresetExtendedCategory category,
                                    const char **out_names,
                                    int max_count)
{
    if (!out_names || max_count <= 0) return 0;

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

const char *preset_extended_category_to_string(PresetExtendedCategory cat)
{
    switch (cat) {
        case PRESET_EXT_BASIC_CONSTRUCTION:    return "基础几何构造";
        case PRESET_EXT_ADVANCED_CONSTRUCTION: return "高级几何构造";
        case PRESET_EXT_POLYGON:               return "多边形";
        case PRESET_EXT_CIRCLE:                return "圆相关";
        case PRESET_EXT_TRANSFORMATION_BASIC:  return "基本变换";
        case PRESET_EXT_TRANSFORMATION_ADVANCED: return "高级变换";
        case PRESET_EXT_MEASUREMENT:           return "度量计算";
        case PRESET_EXT_TRIGONOMETRY:          return "三角函数";
        case PRESET_EXT_COORDINATE:            return "坐标运算";
        case PRESET_EXT_ALGEBRA_BASIC:         return "基础代数";
        case PRESET_EXT_ALGEBRA_ADVANCED:      return "高级代数";
        case PRESET_EXT_LINEAR_ALGEBRA:        return "线性代数";
        case PRESET_EXT_POLYNOMIAL:            return "多项式";
        case PRESET_EXT_LOGIC_PROPOSITIONAL:   return "命题逻辑";
        case PRESET_EXT_LOGIC_PREDICATE:       return "谓词逻辑";
        case PRESET_EXT_PROOF_TACTICS:         return "证明策略";
        case PRESET_EXT_ANALYSIS_LIMIT:        return "极限";
        case PRESET_EXT_ANALYSIS_DIFFERENTIAL: return "微分";
        case PRESET_EXT_ANALYSIS_INTEGRAL:     return "积分";
        case PRESET_EXT_TOPOLOGY:              return "拓扑";
        case PRESET_EXT_DIFFERENTIAL_GEOMETRY: return "微分几何";
        case PRESET_EXT_NUMBER_THEORY:         return "数论";
        case PRESET_EXT_GROUP_THEORY:          return "群论";
        case PRESET_EXT_ANALYSIS:              return "分析学";
        case PRESET_EXT_COMBINATORICS:         return "组合数学";
        case PRESET_EXT_CATEGORY_COUNT:        return "类别总数";
        default:                               return "未知类别";
    }
}

/* ==================== 预设注册函数 ==================== */

/**
 * @brief 将 PresetCategory 映射到 PresetExtendedCategory
 *
 * v5.0 统一注册接口使用 PresetCategory，内部需要映射到
 * PresetExtendedCategory 以兼容现有的注册表结构。
 *
 * @param category 预设类别
 * @return 对应的扩展类别
 */
static PresetExtendedCategory map_category_to_extended(PresetCategory category)
{
    switch (category) {
        case PRESET_CATEGORY_CONSTRUCTION:       return PRESET_EXT_BASIC_CONSTRUCTION;
        case PRESET_CATEGORY_MEASUREMENT:        return PRESET_EXT_MEASUREMENT;
        case PRESET_CATEGORY_TRANSFORMATION:     return PRESET_EXT_TRANSFORMATION_BASIC;
        case PRESET_CATEGORY_ALGEBRAIC:          return PRESET_EXT_ALGEBRA_BASIC;
        case PRESET_CATEGORY_LOGIC:              return PRESET_EXT_LOGIC_PROPOSITIONAL;
        case PRESET_CATEGORY_ANALYSIS:           return PRESET_EXT_ANALYSIS;
        case PRESET_CATEGORY_NUMBER_THEORY:      return PRESET_EXT_NUMBER_THEORY;
        case PRESET_CATEGORY_GROUP_THEORY:       return PRESET_EXT_GROUP_THEORY;
        case PRESET_CATEGORY_TOPOLOGY:           return PRESET_EXT_TOPOLOGY;
        case PRESET_CATEGORY_RING_THEORY:        return PRESET_EXT_ALGEBRA_BASIC;
        case PRESET_CATEGORY_FIELD_THEORY:       return PRESET_EXT_ALGEBRA_ADVANCED;
        case PRESET_CATEGORY_LINEAR_ALGEBRA:     return PRESET_EXT_LINEAR_ALGEBRA;
        case PRESET_CATEGORY_COMBINATORICS:      return PRESET_EXT_COMBINATORICS;
        case PRESET_CATEGORY_COMPLEX_ANALYSIS:   return PRESET_EXT_ANALYSIS;
        case PRESET_CATEGORY_PROBABILITY:        return PRESET_EXT_ANALYSIS;
        case PRESET_CATEGORY_GEOMETRY:           return PRESET_EXT_ADVANCED_CONSTRUCTION;
        case PRESET_CATEGORY_ALGEBRA:            return PRESET_EXT_ALGEBRA_BASIC;
        case PRESET_CATEGORY_CATEGORY_THEORY:    return PRESET_EXT_TOPOLOGY;
        case PRESET_CATEGORY_SET_THEORY:         return PRESET_EXT_TOPOLOGY;
        case PRESET_CATEGORY_CUSTOM:             return PRESET_EXT_BASIC_CONSTRUCTION;
        /* ---- v10.0 修复：补齐"界面层-扩展类别"映射中缺失的 5 个分支 ---- */
        case PRESET_CATEGORY_GRAPH_THEORY:       return PRESET_EXT_GRAPH_THEORY;
        case PRESET_CATEGORY_DIFFERENTIAL_GEOMETRY: return PRESET_EXT_DIFFERENTIAL_GEOMETRY;
        case PRESET_CATEGORY_NUMERICAL:          return PRESET_EXT_NUMERICAL_ANALYSIS;
        case PRESET_CATEGORY_OPTIMIZATION:       return PRESET_EXT_OPTIMIZATION_THEORY;
        case PRESET_CATEGORY_MATH_LOGIC:         return PRESET_EXT_MATH_LOGIC;
        case PRESET_CATEGORY_COUNT:              return PRESET_EXT_BASIC_CONSTRUCTION;
        /* 不提供 default 分支：若未来新增 PresetCategory 而未同步更新此 switch，
         * 编译器将发出 -Wswitch 警告，提示开发者补充映射。这比静默回退到默认值更安全。 */
    }
    return PRESET_EXT_BASIC_CONSTRUCTION;
}

/* ==================== 通用简化注册 ==================== */

bool preset_blocks_register_simple(
    const char *name,
    const char *description,
    PresetCategory category,
    const PresetType *input_types,
    int input_count,
    PresetType output_type,
    const char *mathematical_definition,
    const char *complexity,
    bool is_constructive,
    bool is_reversible)
{
    /* 参数有效性检查 */
    if (!name || !description) {
        LV00_LOG_WARNING("preset_blocks_register_simple: 名称或描述为空");
        return false;
    }

    if (input_count < 0) {
        LV00_LOG_WARNING("preset_blocks_register_simple: 输入数量无效 (%d)", input_count);
        return false;
    }

    PRESET_REGISTRY_LOCK();

    /* 检查是否已存在同名预设 */
    if (find_preset_index(name) >= 0) {
        LV00_LOG_WARNING("preset_blocks_register_simple: 预设已存在 '%s'", name);
        PRESET_REGISTRY_UNLOCK();
        return false;
    }

    /* 确保注册表容量 */
    if (!ensure_preset_registry_capacity()) {
        LV00_LOG_WARNING("preset_blocks_register_simple: 注册表容量不足");
        PRESET_REGISTRY_UNLOCK();
        return false;
    }

    /* 创建模板函数块 */
    FuncBlock *fb = func_block_create(g_preset_registry.next_preset_id++);
    if (!fb) {
        LV00_LOG_WARNING("preset_blocks_register_simple: 创建函数块失败 '%s'", name);
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
    fb->output_count = 1;  /* 简化注册始终为单输出 */

    /* 设置确定性状态 */
    fb->determinism = DETERMINISM_VERIFIED;

    /* 填充条目 */
    InternalPresetEntry *entry = &g_preset_registry.entries[g_preset_registry.count];
    memset(entry, 0, sizeof(InternalPresetEntry));

    entry->name = lv00_strdup(name);
    entry->description = lv00_strdup(description);

    /* 映射类别：将基础 PresetCategory 转换为扩展 PresetExtendedCategory */
    entry->category = map_category_to_extended(category);

    /* 复制数学定义（可选） */
    if (mathematical_definition) {
        entry->mathematical_definition = lv00_strdup(mathematical_definition);
    }

    /* 复制输入类型数组（可选） */
    if (input_types && input_count > 0) {
        entry->input_types = lv00_malloc((size_t)input_count * sizeof(PresetType));
        if (entry->input_types) {
            memcpy(entry->input_types, input_types, (size_t)input_count * sizeof(PresetType));
            entry->input_type_count = input_count;
        }
    }

    /* 设置输出类型 */
    entry->output_type = output_type;

    /* 复制复杂度描述（可选） */
    if (complexity) {
        entry->complexity = lv00_strdup(complexity);
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
        LV00_LOG_WARNING("preset_blocks_register_simple: 内存分配失败 '%s'", name);
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
static bool register_preset_internal(const char *name,
                                      const char *description,
                                      PresetExtendedCategory category,
                                      int input_count,
                                      int output_count)
{
    if (!name || !description) return false;
    if (find_preset_index(name) >= 0) return false;
    if (!ensure_preset_registry_capacity()) return false;

    /* 创建模板函数块 */
    FuncBlock *fb = func_block_create(g_preset_registry.next_preset_id++);
    if (!fb) return false;

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
    fb->determinism = DETERMINISM_VERIFIED;

    /* 填充条目 */
    InternalPresetEntry *entry = &g_preset_registry.entries[g_preset_registry.count];
    memset(entry, 0, sizeof(InternalPresetEntry));

    entry->name = lv00_strdup(name);
    entry->description = lv00_strdup(description);
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

bool preset_blocks_register_by_category(const char *name,
                                         const char *description,
                                         PresetExtendedCategory category,
                                         int input_count,
                                         int output_count)
{
    return register_preset_internal(name, description, category, input_count, output_count);
}

/* ==================== 查找函数 ==================== */

int preset_blocks_find_by_prefix(const char *prefix,
                                  const char **out_names,
                                  int max_count)
{
    if (!prefix || !out_names || max_count <= 0) return 0;

    PRESET_REGISTRY_LOCK();

    size_t prefix_len = strlen(prefix);
    int found = 0;

    for (int i = 0; i < g_preset_registry.count; i++) {
        if (g_preset_registry.entries[i].name &&
            strncmp(g_preset_registry.entries[i].name, prefix, prefix_len) == 0) {
            if (found < max_count) {
                out_names[found] = g_preset_registry.entries[i].name;
            }
            found++;
        }
    }

    PRESET_REGISTRY_UNLOCK();
    return found;
}

int preset_blocks_find_by_keyword(const char *keyword,
                                   const char **out_names,
                                   int max_count)
{
    if (!keyword || !out_names || max_count <= 0) return 0;

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

int preset_blocks_get_all_names(const char **out_names, int max_count)
{
    if (!out_names || max_count <= 0) return 0;

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

char *preset_blocks_generate_documentation(void)
{
    /* 计算所需缓冲区大小 */
    size_t total_size = 4096;  /* 基础大小 */
    for (int i = 0; i < g_preset_registry.count; i++) {
        if (g_preset_registry.entries[i].name) {
            total_size += strlen(g_preset_registry.entries[i].name) + 256;
        }
        if (g_preset_registry.entries[i].description) {
            total_size += strlen(g_preset_registry.entries[i].description);
        }
    }

    char *doc = lv00_malloc(total_size);
    if (!doc) return NULL;

    int written = snprintf(doc, total_size,
        "# Lv-00 预设函数块文档\n\n"
        "## 概述\n\n"
        "本系统提供 %d 个预设函数块，涵盖以下数学领域：\n\n",
        g_preset_registry.count);
    if (written < 0) { lv00_free((void **)&doc); return NULL; }
    if ((size_t)written >= total_size) { doc[total_size - 1] = '\0'; }

    /* 按类别分组输出 */
    for (int cat = 0; cat < PRESET_EXT_CATEGORY_COUNT; cat++) {
        const char *cat_name = preset_extended_category_to_string((PresetExtendedCategory)cat);
        int cat_count = 0;

        /* 统计该类别数量 */
        for (int i = 0; i < g_preset_registry.count; i++) {
            if (g_preset_registry.entries[i].category == cat) {
                cat_count++;
            }
        }

        if (cat_count > 0) {
            int w = snprintf(doc + written, total_size - written,
                "### %s (%d个)\n\n", cat_name, cat_count);
            if (w < 0) break;
            if ((size_t)w >= total_size - (size_t)written) { written = (int)total_size - 1; break; }
            written += w;

            for (int i = 0; i < g_preset_registry.count; i++) {
                if (g_preset_registry.entries[i].category == cat) {
                    w = snprintf(doc + written, total_size - written,
                        "- **%s**: %s\n",
                        g_preset_registry.entries[i].name,
                        g_preset_registry.entries[i].description ? g_preset_registry.entries[i].description : "");
                    if (w < 0) break;
                    if ((size_t)w >= total_size - (size_t)written) { written = (int)total_size - 1; break; }
                    written += w;
                }
            }
            if ((size_t)written >= total_size - 1) break;
            w = snprintf(doc + written, total_size - written, "\n");
            if (w < 0) break;
            if ((size_t)w >= total_size - (size_t)written) { written = (int)total_size - 1; break; }
            written += w;
        }
    }

    return doc;
}

char *preset_blocks_generate_single_doc(const char *name)
{
    int idx = find_preset_index(name);
    if (idx < 0) return NULL;

    InternalPresetEntry *entry = &g_preset_registry.entries[idx];
    size_t size = 2048;

    char *doc = lv00_malloc(size);
    if (!doc) return NULL;

    snprintf(doc, size,
        "## %s\n\n"
        "**类别**: %s\n\n"
        "**描述**: %s\n\n"
        "**输入端口**: %d\n\n"
        "**输出端口**: %d\n",
        entry->name,
        preset_extended_category_to_string(entry->category),
        entry->description ? entry->description : "无描述",
        entry->input_count,
        entry->output_count);

    return doc;
}

/* ==================== 统计信息 ==================== */

void preset_blocks_get_stats(int *total_count, int *by_category)
{
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

void preset_blocks_print_stats(void)
{
    int total, by_category[PRESET_EXT_CATEGORY_COUNT];
    preset_blocks_get_stats(&total, by_category);

    printf("=== Lv-00 预设函数块统计 ===\n");
    printf("总计: %d 个预设\n\n", total);

    for (int i = 0; i < PRESET_EXT_CATEGORY_COUNT; i++) {
        if (by_category[i] > 0) {
            printf("  %s: %d 个\n",
                   preset_extended_category_to_string((PresetExtendedCategory)i),
                   by_category[i]);
        }
    }
    printf("===========================\n");
}

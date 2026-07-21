/**
 * @file func_block_registry.c
 * @brief 预设函数块注册系统实现
 *
 * 实现全局预设函数块注册表的初始化、查找、分类筛选和资源管理。
 * 内置 75 个预设函数块，覆盖几何构造、度量计算、几何变换、
 * 代数运算、逻辑推导和分析六大类别。
 *
 * 内存管理：
 * - 使用 lv00_malloc / lv00_free / lv00_strdup 进行内存管理
 * - 使用 lv00_realloc 进行数组扩容
 * - cleanup 时释放所有条目及其模板函数块
 */

#include "func_block_registry.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================== 命名常量 ==================== */

/** 注册表初始容量 */
#define REGISTRY_INITIAL_CAPACITY 32

/** 数组扩容增长因子 */
#define REGISTRY_GROWTH_FACTOR 2

/** 预设函数块 ID 起始偏移（引用 lv00_internal.h 中的统一定义） */
#define PRESET_FB_ID_OFFSET LV00_PRESET_ID_OFFSET

/* ==================== 全局注册表 ==================== */

/** 全局预设函数块注册表（单例模式）
 *
 * 存储所有已注册的预设函数块条目。首次使用时惰性初始化，
 * 通过 ensure_registry_capacity() 动态扩容。
 */
static FuncBlockRegistry g_registry = {
    .entries     = NULL,       /**< 条目数组（动态分配，初始为 NULL） */
    .count       = 0,          /**< 当前已注册的预设数量 */
    .capacity    = 0,          /**< 数组容量（0 表示尚未分配） */
    .initialized = false       /**< 是否已完成初始化（含内置预设注册） */
};

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 确保注册表数组有足够的容量
 *
 * 当 count >= capacity 时，以 REGISTRY_GROWTH_FACTOR 倍率扩容。
 * 包含整数溢出检查，防止 capacity * REGISTRY_GROWTH_FACTOR 超出 int 范围。
 *
 * @return true 扩容成功或无需扩容，false 内存不足或溢出
 */
static bool ensure_registry_capacity(void)
{
    if (g_registry.count < g_registry.capacity) {
        return true;
    }

    int new_capacity;
    if (g_registry.capacity == 0) {
        /* 首次分配，使用初始容量 */
        new_capacity = REGISTRY_INITIAL_CAPACITY;
    } else {
        /* 整数溢出检查：确保 capacity * REGISTRY_GROWTH_FACTOR 不超过 INT_MAX */
        if (g_registry.capacity > INT_MAX / REGISTRY_GROWTH_FACTOR) {
            return false;  /* 溢出，无法继续扩容 */
        }
        new_capacity = g_registry.capacity * REGISTRY_GROWTH_FACTOR;
    }

    /* 【修复】检查 new_capacity * sizeof(PresetEntry) 是否会溢出 size_t */
    if ((size_t)new_capacity > SIZE_MAX / sizeof(PresetEntry)) {
        return false;  /* size_t 乘法将溢出，拒绝分配 */
    }

    PresetEntry *old_entries = g_registry.entries;
    PresetEntry *new_entries = lv00_realloc(
        g_registry.entries, (size_t)new_capacity * sizeof(PresetEntry));
    if (!new_entries) {
        /* 【修复】如果 lv00_realloc 在失败时可能释放了原内存（非标准行为），
         *         重置 entries 指针防止后续误用野指针 */
        if (g_registry.entries != old_entries) {
            g_registry.entries = NULL;
        }
        return false;
    }

    g_registry.entries  = new_entries;
    g_registry.capacity = new_capacity;
    return true;
}

/**
 * @brief 释放单个预设条目的资源
 *
 * 释放条目中动态分配的 name、description 和 template_fb。
 * 释放后将条目字段置零。
 *
 * @param entry 预设条目指针
 */
static void free_preset_entry(PresetEntry *entry)
{
    if (!entry) return;
    lv00_free((void **)&entry->name);
    lv00_free((void **)&entry->description);
    if (entry->template_fb) {
        func_block_destroy(entry->template_fb);
        entry->template_fb = NULL;
    }
    entry->category = PRESET_CATEGORY_CONSTRUCTION;
}

/**
 * @brief 创建一个预设函数块模板
 *
 * 创建一个仅包含元数据的 FuncBlock（不关联具体图节点），
 * 用于作为预设模板。所有预设模板的确定性状态设为
 * DETERMINISM_VERIFIED。
 *
 * @param id          函数块 ID
 * @param name        名称
 * @param description 描述
 * @param input_count 输入端口数量
 * @param output_count 输出端口数量
 * @return 函数块指针，失败返回 NULL
 */
static FuncBlock *create_preset_template(int id, const char *name,
                                          const char *description,
                                          int input_count, int output_count)
{
    FuncBlock *fb = func_block_create(id);
    if (!fb) return NULL;

    if (name) {
        fb->name = lv00_strdup(name);
        if (!fb->name) {
            func_block_destroy(fb);
            return NULL;
        }
    }

    if (description) {
        fb->description = lv00_strdup(description);
        if (!fb->description) {
            func_block_destroy(fb);
            return NULL;
        }
    }

    fb->input_count  = input_count;
    fb->output_count = output_count;
    fb->determinism  = DETERMINISM_VERIFIED;

    return fb;
}

/**
 * @brief 向注册表添加一个预设条目（内部统一实现）
 *
 * 调用者应确保名称唯一（除非 check_duplicate 为 false）。
 * 当 deep_copy 为 true 时，对 fb 做深拷贝（调用者仍持有 fb 所有权）；
 * 当 deep_copy 为 false 时，直接接管 fb 的所有权（失败时由本函数释放）。
 *
 * @param name            预设名称（将被 lv00_strdup 复制）
 * @param description     描述（将被 lv00_strdup 复制，可为 NULL）
 * @param category        类别
 * @param fb              模板函数块
 * @param deep_copy       true 表示深拷贝 fb，false 表示直接接管 fb 所有权
 * @param check_duplicate true 表示检查同名预设是否已存在
 * @return true 添加成功，false 内存不足或同名已存在
 */
static bool add_preset_entry_ex(const char *name, const char *description,
                                 PresetCategory category, FuncBlock *fb,
                                 bool deep_copy, bool check_duplicate)
{
    if (!name || !fb) {
        return false;
    }

    /* 可选：检查是否已存在同名预设（公共 API 路径需要） */
    if (check_duplicate) {
        for (int i = 0; i < g_registry.count; i++) {
            if (g_registry.entries[i].name &&
                strcmp(g_registry.entries[i].name, name) == 0) {
                return false;  /* 同名预设已存在 */
            }
        }
    }

    /* 确保注册表数组有足够容量 */
    if (!ensure_registry_capacity()) {
        return false;
    }

    PresetEntry *entry = &g_registry.entries[g_registry.count];

    /* 复制名称 */
    entry->name = lv00_strdup(name);
    if (!entry->name) {
        goto fail;  /* 名称复制失败，清理并返回 */
    }

    /* 复制描述（description 为 NULL 是允许的） */
    entry->description = description ? lv00_strdup(description) : NULL;

    entry->category = category;

    /* 处理模板函数块：深拷贝或直接接管所有权 */
    if (deep_copy) {
        entry->template_fb = func_block_copy(fb);
        if (!entry->template_fb) {
            goto fail;  /* 深拷贝失败，清理已分配的资源 */
        }
    } else {
        entry->template_fb = fb;  /* 直接接管所有权 */
    }

    g_registry.count++;
    return true;

fail:
    /* 错误路径：释放本条目已分配的资源，避免内存泄漏 */
    lv00_free((void **)&entry->name);
    lv00_free((void **)&entry->description);
    entry->template_fb = NULL;
    entry->category    = PRESET_CATEGORY_CONSTRUCTION;
    return false;
}

/* ==================== 内置预设注册 ==================== */

/**
 * @brief 内置预设函数块的定义描述（数据驱动）
 *
 * 每个条目包含注册一个预设所需的全部信息。
 * register_builtin_presets() 遍历此数组完成批量注册。
 */
typedef struct {
    const char *name;        /**< 预设名称（英文标识符） */
    const char *description; /**< 中文描述 */
    int input_count;         /**< 输入端口数量（-1 表示可变） */
    int output_count;        /**< 输出端口数量（-1 表示可变） */
    PresetCategory category; /**< 所属类别 */
} BuiltinPresetDef;

/**
 * @brief 内置预设函数块定义表（75个预设，按类别分组）
 *
 * 分组概览：
 *   - 几何构造类 (CONSTRUCTION): 1-10, 20-27, 41-49  (共 27 个)
 *   - 度量计算类 (MEASUREMENT):  11-12, 28-31, 50-55  (共 12 个)
 *   - 几何变换类 (TRANSFORMATION): 13-15, 37-38, 56-59 (共 9 个)
 *   - 代数运算类 (ALGEBRAIC):     16-17, 32-36, 60-67  (共 15 个)
 *   - 逻辑推导类 (LOGIC):         18-19, 68-75          (共 10 个)
 *   - 分析类     (ANALYSIS):      39-40                  (共 2 个)
 *   合计: 27 + 12 + 9 + 15 + 10 + 2 = 75
 */
static const BuiltinPresetDef g_builtin_presets[] = {
    /* ===== 几何构造类 (PRESET_CATEGORY_CONSTRUCTION) ===== */

    /* 1 */  { "midpoint",                  "给定两点A、B，构造中点M。输入2个点，输出1个点。",                                                               2,  1, PRESET_CATEGORY_CONSTRUCTION },
    /* 2 */  { "perpendicular_bisector",    "给定两点A、B，构造垂直平分线。输入2个点，输出2个点（线段端点）。",                                                   2,  2, PRESET_CATEGORY_CONSTRUCTION },
    /* 3 */  { "angle_bisector",            "给定三点A、B、C（B为顶点），构造角平分线上的点。输入3个点，输出1个点。",                                               3,  1, PRESET_CATEGORY_CONSTRUCTION },
    /* 4 */  { "parallel_line",             "给定一条线段和一个外部点，过该点作平行线。输入3个点（线段两端+外部点），输出2个点。",                                   3,  2, PRESET_CATEGORY_CONSTRUCTION },
    /* 5 */  { "perpendicular_line",        "给定一条线段和一个外部点，过该点作垂线。输入3个点，输出2个点。",                                                       3,  2, PRESET_CATEGORY_CONSTRUCTION },
    /* 6 */  { "circle_by_center_radius",   "给定圆心和半径上的点，构造圆。输入2个点，输出1个区域。",                                                               2,  1, PRESET_CATEGORY_CONSTRUCTION },
    /* 7 */  { "circle_by_three_points",    "给定三个不共线的点，构造外接圆。输入3个点，输出1个区域。",                                                             3,  1, PRESET_CATEGORY_CONSTRUCTION },
    /* 8 */  { "line_intersection",         "给定两条线段（四个端点），求交点。输入4个点，输出1个点。",                                                               4,  1, PRESET_CATEGORY_CONSTRUCTION },
    /* 9 */  { "reflection",                "给定一点和一条线段，求关于该线的对称点。输入3个点，输出1个点。",                                                         3,  1, PRESET_CATEGORY_CONSTRUCTION },
    /* 10 */ { "equilateral_triangle",      "给定一条边，构造等边三角形的第三个顶点。输入2个点，输出1个点。",                                                         2,  1, PRESET_CATEGORY_CONSTRUCTION },

    /* ===== 度量计算类 (PRESET_CATEGORY_MEASUREMENT) ===== */

    /* 11 */ { "distance",                  "计算两点间的距离。输入2个点，输出1个点（距离标记点）。",                                                               2,  1, PRESET_CATEGORY_MEASUREMENT },
    /* 12 */ { "angle_measure",             "计算三点形成的角度。输入3个点，输出1个点（角度标记点）。",                                                             3,  1, PRESET_CATEGORY_MEASUREMENT },

    /* ===== 几何变换类 (PRESET_CATEGORY_TRANSFORMATION) ===== */

    /* 13 */ { "translation",               "将一个点沿向量平移。输入3个点（原点、目标点、待平移点），输出1个点。",                                                   3,  1, PRESET_CATEGORY_TRANSFORMATION },
    /* 14 */ { "rotation",                  "将一个点绕中心旋转指定角度。输入3个点（中心、参考点、待旋转点），输出1个点。",                                           3,  1, PRESET_CATEGORY_TRANSFORMATION },
    /* 15 */ { "homothety",                 "将一个点关于中心按比例缩放。输入3个点（中心、参考点、待变换点），输出1个点。",                                           3,  1, PRESET_CATEGORY_TRANSFORMATION },

    /* ===== 代数运算类 (PRESET_CATEGORY_ALGEBRAIC) ===== */

    /* 16 */ { "vector_add",                "给定O、A、B三点，构造A+B对应的点C。输入3个点，输出1个点。",                                                           3,  1, PRESET_CATEGORY_ALGEBRAIC },
    /* 17 */ { "vector_scale",              "给定O、A两点和比例系数，构造k*A对应的点。输入2个点，输出1个点。",                                                       2,  1, PRESET_CATEGORY_ALGEBRAIC },

    /* ===== 逻辑推导类 (PRESET_CATEGORY_LOGIC) ===== */

    /* 18 */ { "contradiction_detector",    "检测约束系统中是否存在矛盾。输入0个端口，输出1个端口。",                                                               0,  1, PRESET_CATEGORY_LOGIC },
    /* 19 */ { "implication_chain",         "将多个命题按蕴含关系链接。输入N个端口，输出1个端口。",                                                                 -1, 1, PRESET_CATEGORY_LOGIC },

    /* ===== 几何构造类 (PRESET_CATEGORY_CONSTRUCTION) - 续 ===== */

    /* 20 */ { "circumcenter",              "给定三个点A、B、C，构造外接圆圆心。输入3个点，输出1个点。",                                                             3,  1, PRESET_CATEGORY_CONSTRUCTION },
    /* 21 */ { "incenter",                  "给定三个点A、B、C，构造内切圆圆心。输入3个点，输出1个点。",                                                             3,  1, PRESET_CATEGORY_CONSTRUCTION },
    /* 22 */ { "centroid",                  "给定三个点A、B、C，构造重心。输入3个点，输出1个点。",                                                                   3,  1, PRESET_CATEGORY_CONSTRUCTION },
    /* 23 */ { "orthocenter",               "给定三个点A、B、C，构造垂心。输入3个点，输出1个点。",                                                                   3,  1, PRESET_CATEGORY_CONSTRUCTION },
    /* 24 */ { "foot_of_perpendicular",     "给定一点P和一条线段AB，构造P到AB的垂足。输入3个点，输出1个点。",                                                         3,  1, PRESET_CATEGORY_CONSTRUCTION },
    /* 25 */ { "tangent_line_from_point",   "给定一个点P和圆上两个点（圆心和半径点），构造切线。输入3个点，输出2个点（切点）。",                                       3,  2, PRESET_CATEGORY_CONSTRUCTION },
    /* 26 */ { "nine_point_circle",         "给定三个点A、B、C，构造九点圆圆心。输入3个点，输出1个点。",                                                             3,  1, PRESET_CATEGORY_CONSTRUCTION },
    /* 27 */ { "excenter",                  "给定三个点A、B、C，构造A对边的旁心。输入3个点，输出1个点。",                                                             3,  1, PRESET_CATEGORY_CONSTRUCTION },

    /* ===== 度量计算类 (PRESET_CATEGORY_MEASUREMENT) - 续 ===== */

    /* 28 */ { "area_measure",              "给定三个点A、B、C，计算三角形面积。输入3个点，输出1个点（面积标记点）。",                                               3,  1, PRESET_CATEGORY_MEASUREMENT },
    /* 29 */ { "perimeter_measure",         "给定三个点A、B、C，计算三角形周长。输入3个点，输出1个点（周长标记点）。",                                               3,  1, PRESET_CATEGORY_MEASUREMENT },
    /* 30 */ { "ratio_measure",             "给定四个点A、B、C、D，计算AB/CD比值。输入4个点，输出1个点。",                                                           4,  1, PRESET_CATEGORY_MEASUREMENT },
    /* 31 */ { "slope_measure",             "给定两个点A、B，计算直线斜率。输入2个点，输出1个点。",                                                                 2,  1, PRESET_CATEGORY_MEASUREMENT },

    /* ===== 代数运算类 (PRESET_CATEGORY_ALGEBRAIC) - 续 ===== */

    /* 32 */ { "vector_sub",                "给定O、A、B三点，构造A-B对应的点C。输入3个点，输出1个点。",                                                           3,  1, PRESET_CATEGORY_ALGEBRAIC },
    /* 33 */ { "vector_dot_product",        "给定O、A、B三点，计算OA·OB。输入3个点，输出1个点。",                                                                 3,  1, PRESET_CATEGORY_ALGEBRAIC },
    /* 34 */ { "vector_cross_product_magnitude", "给定O、A、B三点，计算OA×OB的模。输入3个点，输出1个点。",                                                          3,  1, PRESET_CATEGORY_ALGEBRAIC },
    /* 35 */ { "vector_reflect",            "给定O、A、B三点，将向量OA关于OB反射。输入3个点，输出1个点。",                                                         3,  1, PRESET_CATEGORY_ALGEBRAIC },
    /* 36 */ { "vector_project",            "给定O、A、B三点，将向量OA投影到OB上。输入3个点，输出1个点。",                                                         3,  1, PRESET_CATEGORY_ALGEBRAIC },

    /* ===== 几何变换类 (PRESET_CATEGORY_TRANSFORMATION) - 续 ===== */

    /* 37 */ { "circle_inversion",          "给定反演中心P、半径点R和待变换点Q，构造反演点。输入3个点，输出1个点。",                                                 3,  1, PRESET_CATEGORY_TRANSFORMATION },
    /* 38 */ { "affine_transform",          "给定原点O、基向量端点A、B和待变换点P，构造变换后的点。输入4个点，输出1个点。",                                           4,  1, PRESET_CATEGORY_TRANSFORMATION },

    /* ===== 分析类 (PRESET_CATEGORY_ANALYSIS) ===== */

    /* 39 */ { "taylor_approximation",      "给定展开点P、参考点Q和待近似点R，构造泰勒近似点。输入3个点，输出1个点。",                                               3,  1, PRESET_CATEGORY_ANALYSIS },
    /* 40 */ { "limit_point",               "给定序列起点P、参考点Q，构造极限逼近点。输入2个点，输出1个点。",                                                       2,  1, PRESET_CATEGORY_ANALYSIS },

    /* ===== 几何构造类 (PRESET_CATEGORY_CONSTRUCTION) - 新增 ===== */

    /* 41 */ { "rectangle",                 "给定三个点构造矩形，A-B为一边，C确定矩形所在平面方向。输入3个点，输出1个点（D，矩形第四顶点）。",                           3,  1, PRESET_CATEGORY_CONSTRUCTION },
    /* 42 */ { "square",                    "给定一条边AB，构造正方形ABCD。输入2个点，输出2个点（C、D为另外两个顶点）。",                                             2,  2, PRESET_CATEGORY_CONSTRUCTION },
    /* 43 */ { "regular_polygon",           "给定中心O和半径r，构造正n边形。输入1个点（中心）+ 1个数值参数（半径），输出n个点（正n边形顶点）。",                         2, -1, PRESET_CATEGORY_CONSTRUCTION },
    /* 44 */ { "tangent_line",              "从圆外一点P作圆的两条切线。输入1个点（圆外点P）+ 1个圆（由圆心和半径点确定），输出2个线段（两条切线）。",                   3,  2, PRESET_CATEGORY_CONSTRUCTION },
    /* 45 */ { "circumcircle",              "给定三个不共线的点，构造其外接圆。输入3个点（A、B、C），输出1个圆（外接圆）。",                                         3,  1, PRESET_CATEGORY_CONSTRUCTION },
    /* 46 */ { "incircle",                  "给定三角形的三个顶点，构造内切圆和内心。输入3个点（三角形三顶点），输出1个圆（内切圆）+ 1个点（内心）。",                     3,  2, PRESET_CATEGORY_CONSTRUCTION },
    /* 47 */ { "golden_ratio",              "在线段AB上求黄金分割点P，使AP:PB = φ:1。输入2个点（线段AB），输出1个点（黄金分割点P，使AP/AB = (√5-1)/2）。",             2,  1, PRESET_CATEGORY_CONSTRUCTION },
    /* 48 */ { "power_of_point",            "计算点P关于圆的幂（power of a point）。输入1个点（P）+ 1个圆（由圆心和半径点确定），输出1个数值（|OP|² - r²）。",           3,  1, PRESET_CATEGORY_CONSTRUCTION },
    /* 49 */ { "harmonic_conjugate",        "给定共线三点A、B、C，求其调和共轭点D，使(A,B;C,D)=-1。输入3个点（A、B、C共线），输出1个点（D）。",                       3,  1, PRESET_CATEGORY_CONSTRUCTION },

    /* ===== 度量计算类 (PRESET_CATEGORY_MEASUREMENT) - 新增 ===== */

    /* 50 */ { "area_triangle",             "使用向量叉积计算三角形面积。输入3个点（三角形三顶点），输出1个数值（面积）。",                                           3,  1, PRESET_CATEGORY_MEASUREMENT },
    /* 51 */ { "area_polygon",              "使用鞋带公式（Shoelace formula）计算多边形面积。输入n个点（多边形顶点，按顺序），输出1个数值（有向面积）。",                 -1, 1, PRESET_CATEGORY_MEASUREMENT },
    /* 52 */ { "perimeter",                 "计算多边形或折线的总周长。输入n个点（多边形顶点），输出1个数值（周长）。",                                               -1, 1, PRESET_CATEGORY_MEASUREMENT },
    /* 53 */ { "slope",                     "计算经过两点的直线斜率。输入2个点，输出1个数值（斜率）。",                                                               2,  1, PRESET_CATEGORY_MEASUREMENT },
    /* 54 */ { "curvature",                 "使用三点法计算离散曲率。输入3个点（曲线上的相邻三点），输出1个数值（曲率）。",                                           3,  1, PRESET_CATEGORY_MEASUREMENT },
    /* 55 */ { "cross_ratio",               "计算四个共线点的交比（cross ratio）。输入4个共线点，输出1个数值（交比 (A,B;C,D)）。",                                   4,  1, PRESET_CATEGORY_MEASUREMENT },

    /* ===== 几何变换类 (PRESET_CATEGORY_TRANSFORMATION) - 新增 ===== */

    /* 56 */ { "glide_reflection",          "沿直线做反射后再沿直线方向平移的复合变换。输入1条线段（反射轴，2个点）+ 1个向量（平移方向，2个点），输出1个点（变换后的点）。", 4, 1, PRESET_CATEGORY_TRANSFORMATION },
    /* 57 */ { "inversion",                 "关于给定圆的反演变换。输入1个圆（反演圆，由圆心和半径点确定）+ 1个点（待变换点），输出1个点（反演点）。",                   3,  1, PRESET_CATEGORY_TRANSFORMATION },
    /* 58 */ { "spiral_similarity",         "以给定点为中心的旋转+缩放复合变换。输入1个中心点 + 旋转角度 + 缩放比例，输出1个点（变换后的点）。",                         3,  1, PRESET_CATEGORY_TRANSFORMATION },
    /* 59 */ { "projective_transform",      "由四对对应点确定的射影变换（单应性）。输入4对对应点（8个点），输出1个点（变换后的点）。",                                 8,  1, PRESET_CATEGORY_TRANSFORMATION },

    /* ===== 代数运算类 (PRESET_CATEGORY_ALGEBRAIC) - 新增 ===== */

    /* 60 */ { "polynomial_add",            "计算两个多项式的加法结果。输入2个多项式（系数列表），输出1个多项式（和）。",                                             2,  1, PRESET_CATEGORY_ALGEBRAIC },
    /* 61 */ { "polynomial_multiply",       "计算两个多项式的乘法结果。输入2个多项式，输出1个多项式（积）。",                                                         2,  1, PRESET_CATEGORY_ALGEBRAIC },
    /* 62 */ { "polynomial_gcd",            "使用欧几里得算法计算多项式最大公因式。输入2个多项式，输出1个多项式（GCD）。",                                           2,  1, PRESET_CATEGORY_ALGEBRAIC },
    /* 63 */ { "resultant",                 "计算两个多项式的结式（resultant），用于判断公共零点。输入2个多项式，输出1个数值（结式）。",                               2,  1, PRESET_CATEGORY_ALGEBRAIC },
    /* 64 */ { "determinant_2x2",           "计算2×2矩阵的行列式。输入4个数值（矩阵元素），输出1个数值（行列式值）。",                                               4,  1, PRESET_CATEGORY_ALGEBRAIC },
    /* 65 */ { "determinant_3x3",           "计算3×3矩阵的行列式（萨吕法则）。输入9个数值，输出1个数值。",                                                           9,  1, PRESET_CATEGORY_ALGEBRAIC },
    /* 66 */ { "matrix_multiply",           "计算两个矩阵的乘积。输入2个矩阵，输出1个矩阵。",                                                                       2,  1, PRESET_CATEGORY_ALGEBRAIC },
    /* 67 */ { "eigenvalues_2x2",           "计算2×2矩阵的特征值（求解特征方程）。输入4个数值，输出2个数值（特征值）。",                                             4,  2, PRESET_CATEGORY_ALGEBRAIC },

    /* ===== 逻辑推导类 (PRESET_CATEGORY_LOGIC) - 新增 ===== */

    /* 68 */ { "modus_ponens",              "假言推理：从P和P→Q推导Q。输入命题P + 蕴含命题(P→Q)，输出命题Q。",                                                     2,  1, PRESET_CATEGORY_LOGIC },
    /* 69 */ { "modus_tollens",             "否定后件：从P→Q和¬Q推导¬P。输入蕴含命题(P→Q) + 命题¬Q，输出命题¬P。",                                               2,  1, PRESET_CATEGORY_LOGIC },
    /* 70 */ { "conjunction",               "合取引入规则：从P和Q推导P∧Q。输入命题P + 命题Q，输出命题(P∧Q)。",                                                     2,  1, PRESET_CATEGORY_LOGIC },
    /* 71 */ { "disjunction_intro",         "析取引入规则：从P推导P∨Q（Q为任意命题）。输入命题P，输出命题(P∨Q)。",                                                 1,  1, PRESET_CATEGORY_LOGIC },
    /* 72 */ { "negation_intro",            "否定引入规则：假设P导致矛盾，则推导¬P。输入假设P推导出矛盾，输出命题¬P。",                                             1,  1, PRESET_CATEGORY_LOGIC },
    /* 73 */ { "universal_intro",           "全称量化引入：从对任意个体的证明推导全称命题。输入对任意a推导P(a)，输出命题∀x.P(x)。",                                 1,  1, PRESET_CATEGORY_LOGIC },
    /* 74 */ { "existential_intro",         "存在量化引入：从具体实例推导存在命题。输入命题P(t)（t为具体项），输出命题∃x.P(x)。",                                   1,  1, PRESET_CATEGORY_LOGIC },
    /* 75 */ { "proof_by_contradiction",    "反证法：假设¬P导致矛盾，则推导P。输入假设¬P推导出矛盾，输出命题P。",                                                 1,  1, PRESET_CATEGORY_LOGIC },
};

/**
 * @brief 注册所有内置预设函数块
 *
 * 在首次初始化时调用，惰性创建 75 个预设函数块模板。
 * 通过遍历 g_builtin_presets[] 数据表完成批量注册。
 * 使用 goto cleanup 模式确保 create_preset_template 成功但
 * add_preset_entry_ex 失败时，fb 被正确释放，避免内存泄漏。
 *
 * @return true 全部注册成功，false 内存不足（部分可能已注册）
 */
static bool register_builtin_presets(void)
{
    const int count = (int)(sizeof(g_builtin_presets) / sizeof(g_builtin_presets[0]));

    for (int i = 0; i < count; i++) {
        const BuiltinPresetDef *def = &g_builtin_presets[i];
        int id = PRESET_FB_ID_OFFSET + i;

        FuncBlock *fb = create_preset_template(id, def->name, def->description,
                                                def->input_count, def->output_count);
        if (!fb) {
            return false;  /* 模板创建失败，无需清理 fb */
        }

        /* 内部注册路径：不检查重复，直接接管 fb 所有权 */
        if (!add_preset_entry_ex(def->name, def->description,
                                  def->category, fb,
                                  false, false)) {
            /* add_preset_entry_ex 失败时已释放 fb（deep_copy=false 模式下
               goto fail 会将 template_fb 置 NULL，但 fb 是传入参数，
               需要在此处手动释放未被接管的 fb） */
            func_block_destroy(fb);
            return false;
        }
        /* fb 所有权已成功转移给注册表，无需再释放 */
    }

    return true;
}

/* ==================== 公共 API 实现 ==================== */

bool func_block_registry_init(void)
{
    /* 幂等操作：已初始化则直接返回 */
    if (g_registry.initialized) {
        return true;
    }

    /* 注册所有内置预设 */
    if (!register_builtin_presets()) {
        /* 内置预设注册失败，清理已注册的部分 */
        func_block_registry_cleanup();
        return false;
    }

    g_registry.initialized = true;
    return true;
}

void func_block_registry_cleanup(void)
{
    /* 释放所有条目的资源 */
    for (int i = 0; i < g_registry.count; i++) {
        free_preset_entry(&g_registry.entries[i]);
    }

    /* 释放条目数组本身 */
    lv00_free((void **)&g_registry.entries);

    /* 重置注册表状态 */
    g_registry.count       = 0;
    g_registry.capacity    = 0;
    g_registry.initialized = false;
}

int func_block_registry_unregister(const char *name) {
    if (!name || !g_registry.initialized) return -1;

    for (int i = 0; i < g_registry.count; i++) {
        if (g_registry.entries[i].name &&
            strcmp(g_registry.entries[i].name, name) == 0) {
            /* 释放条目资源 */
            free_preset_entry(&g_registry.entries[i]);
            /* 将后面的条目前移 */
            if (i < g_registry.count - 1) {
                memmove(&g_registry.entries[i],
                        &g_registry.entries[i + 1],
                        (size_t)(g_registry.count - i - 1) * sizeof(PresetEntry));
            }
            g_registry.count--;
            return 0;
        }
    }
    return -1; /* 未找到 */
}

bool func_block_register(const char *name, const char *description,
                          PresetCategory category, FuncBlock *fb)
{
    /*
     * 公共 API：检查同名重复 + 深拷贝 fb。
     * 统一委托给 add_preset_entry_ex，消除代码重复。
     */
    return add_preset_entry_ex(name, description, category, fb,
                                true, true);
}

FuncBlock *func_block_registry_lookup(const char *name)
{
    if (!name) return NULL;

    for (int i = 0; i < g_registry.count; i++) {
        if (g_registry.entries[i].name &&
            strcmp(g_registry.entries[i].name, name) == 0) {
            /* 创建深拷贝返回给调用者 */
            return func_block_copy(g_registry.entries[i].template_fb);
        }
    }

    return NULL;  /* 未找到 */
}

PresetEntry *func_block_registry_find(const char *name)
{
    if (!name) return NULL;

    for (int i = 0; i < g_registry.count; i++) {
        if (g_registry.entries[i].name &&
            strcmp(g_registry.entries[i].name, name) == 0) {
            return &g_registry.entries[i];
        }
    }

    return NULL;  /* 未找到 */
}

int func_block_registry_find_by_category(PresetCategory category,
                                          PresetEntry **out_entries,
                                          int max_count)
{
    if (!out_entries || max_count <= 0) {
        return 0;
    }

    int found = 0;
    int total = 0;
    for (int i = 0; i < g_registry.count; i++) {
        if (g_registry.entries[i].category == category) {
            total++;
            if (found < max_count) {
                out_entries[found++] = &g_registry.entries[i];
            }
        }
    }

    return total;
}

/**
 * @brief 将预设类别枚举值转换为中文可读字符串
 *
 * 类别说明：
 *   - PRESET_CATEGORY_CONSTRUCTION   : 几何构造 — 点、线、圆等几何对象的构造操作
 *   - PRESET_CATEGORY_MEASUREMENT    : 度量计算 — 距离、角度、面积、周长等度量
 *   - PRESET_CATEGORY_TRANSFORMATION : 几何变换 — 平移、旋转、反演等变换操作
 *   - PRESET_CATEGORY_ALGEBRAIC      : 代数运算 — 向量运算、多项式、矩阵等
 *   - PRESET_CATEGORY_LOGIC          : 逻辑推导 — 命题逻辑推理规则
 *   - PRESET_CATEGORY_ANALYSIS       : 分析运算 — 泰勒展开、极限等分析操作
 *   - PRESET_CATEGORY_NUMBER_THEORY  : 数论运算 — 整数性质、同余等
 *   - PRESET_CATEGORY_GROUP_THEORY   : 群论运算 — 群结构相关运算
 *   - PRESET_CATEGORY_RING_THEORY    : 环论运算 — 环结构相关运算
 *   - PRESET_CATEGORY_FIELD_THEORY   : 域论运算 — 域扩展、伽罗瓦理论等
 *   - PRESET_CATEGORY_TOPOLOGY       : 拓扑构造 — 拓扑空间相关操作
 *   - PRESET_CATEGORY_LINEAR_ALGEBRA : 线性代数 — 线性空间、线性映射等
 *   - PRESET_CATEGORY_COMBINATORICS  : 组合数学 — 计数、排列组合等
 *   - PRESET_CATEGORY_COMPLEX_ANALYSIS : 复分析 — 复变函数相关
 *   - PRESET_CATEGORY_PROBABILITY    : 概率统计 — 概率分布、统计推断等
 *
 * @param cat 预设类别枚举值
 * @return 类别的中文可读字符串，未知类别返回 "未知类别"
 */
const char *preset_category_to_string(PresetCategory cat)
{
    switch (cat) {
        case PRESET_CATEGORY_CONSTRUCTION:   return "几何构造";
        case PRESET_CATEGORY_MEASUREMENT:    return "度量计算";
        case PRESET_CATEGORY_TRANSFORMATION: return "几何变换";
        case PRESET_CATEGORY_ALGEBRAIC:      return "代数运算";
        case PRESET_CATEGORY_LOGIC:          return "逻辑推导";
        case PRESET_CATEGORY_ANALYSIS:       return "分析运算";
        case PRESET_CATEGORY_NUMBER_THEORY:  return "数论运算";
        case PRESET_CATEGORY_GROUP_THEORY:   return "群论运算";
        case PRESET_CATEGORY_RING_THEORY:    return "环论运算";
        case PRESET_CATEGORY_FIELD_THEORY:   return "域论运算";
        case PRESET_CATEGORY_TOPOLOGY:       return "拓扑构造";
        case PRESET_CATEGORY_LINEAR_ALGEBRA: return "线性代数";
        case PRESET_CATEGORY_COMBINATORICS:  return "组合数学";
        case PRESET_CATEGORY_COMPLEX_ANALYSIS: return "复分析";
        case PRESET_CATEGORY_PROBABILITY:    return "概率统计";
        default:                             return "未知类别";
    }
}

/**
 * @brief 从字符串解析预设类别枚举值
 *
 * 支持中文名称和英文名称两种格式的解析。
 * 中文名称与 preset_category_to_string() 返回值对应；
 * 英文名称用于序列化/反序列化等场景。
 *
 * @param str      类别名称字符串（中文或英文）
 * @param category 输出：解析后的类别枚举值
 * @return true 解析成功，false 字符串无法识别或参数无效
 */
bool preset_category_from_string(const char *str, PresetCategory *category)
{
    if (!str || !category) return false;

    /* 中文名称映射（与 preset_category_to_string 返回值对应） */
    static const struct { const char *name; PresetCategory cat; } cn_map[] = {
        {"几何构造",   PRESET_CATEGORY_CONSTRUCTION},
        {"度量计算",   PRESET_CATEGORY_MEASUREMENT},
        {"几何变换",   PRESET_CATEGORY_TRANSFORMATION},
        {"代数运算",   PRESET_CATEGORY_ALGEBRAIC},
        {"逻辑推导",   PRESET_CATEGORY_LOGIC},
        {"分析运算",   PRESET_CATEGORY_ANALYSIS},
        {"数论运算",   PRESET_CATEGORY_NUMBER_THEORY},
        {"群论运算",   PRESET_CATEGORY_GROUP_THEORY},
        {"环论运算",   PRESET_CATEGORY_RING_THEORY},
        {"域论运算",   PRESET_CATEGORY_FIELD_THEORY},
        {"拓扑构造",   PRESET_CATEGORY_TOPOLOGY},
        {"线性代数",   PRESET_CATEGORY_LINEAR_ALGEBRA},
        {"组合数学",   PRESET_CATEGORY_COMBINATORICS},
        {"复分析",     PRESET_CATEGORY_COMPLEX_ANALYSIS},
        {"概率统计",   PRESET_CATEGORY_PROBABILITY},
        {"几何",       PRESET_CATEGORY_GEOMETRY},
        {"代数",       PRESET_CATEGORY_ALGEBRA},
        {"范畴论",     PRESET_CATEGORY_CATEGORY_THEORY},
        {"集合论",     PRESET_CATEGORY_SET_THEORY},
        {"自定义",     PRESET_CATEGORY_CUSTOM},
        {"图论",       PRESET_CATEGORY_GRAPH_THEORY},
        {"微分几何",   PRESET_CATEGORY_DIFFERENTIAL_GEOMETRY},
    };

    for (size_t i = 0; i < sizeof(cn_map) / sizeof(cn_map[0]); i++) {
        if (strcmp(cn_map[i].name, str) == 0) {
            *category = cn_map[i].cat;
            return true;
        }
    }

    /* 英文名称映射（用于序列化/反序列化） */
    static const struct { const char *name; PresetCategory cat; } en_map[] = {
        {"construction",          PRESET_CATEGORY_CONSTRUCTION},
        {"measurement",           PRESET_CATEGORY_MEASUREMENT},
        {"transformation",        PRESET_CATEGORY_TRANSFORMATION},
        {"algebraic",             PRESET_CATEGORY_ALGEBRAIC},
        {"logic",                 PRESET_CATEGORY_LOGIC},
        {"analysis",              PRESET_CATEGORY_ANALYSIS},
        {"number_theory",         PRESET_CATEGORY_NUMBER_THEORY},
        {"group_theory",          PRESET_CATEGORY_GROUP_THEORY},
        {"ring_theory",           PRESET_CATEGORY_RING_THEORY},
        {"field_theory",          PRESET_CATEGORY_FIELD_THEORY},
        {"topology",              PRESET_CATEGORY_TOPOLOGY},
        {"linear_algebra",        PRESET_CATEGORY_LINEAR_ALGEBRA},
        {"combinatorics",         PRESET_CATEGORY_COMBINATORICS},
        {"complex_analysis",      PRESET_CATEGORY_COMPLEX_ANALYSIS},
        {"probability",           PRESET_CATEGORY_PROBABILITY},
        {"geometry",              PRESET_CATEGORY_GEOMETRY},
        {"algebra",               PRESET_CATEGORY_ALGEBRA},
        {"category_theory",       PRESET_CATEGORY_CATEGORY_THEORY},
        {"set_theory",            PRESET_CATEGORY_SET_THEORY},
        {"custom",                PRESET_CATEGORY_CUSTOM},
        {"graph_theory",          PRESET_CATEGORY_GRAPH_THEORY},
        {"differential_geometry", PRESET_CATEGORY_DIFFERENTIAL_GEOMETRY},
    };

    for (size_t i = 0; i < sizeof(en_map) / sizeof(en_map[0]); i++) {
        if (strcmp(en_map[i].name, str) == 0) {
            *category = en_map[i].cat;
            return true;
        }
    }

    return false;
}

int func_block_registry_get_count(void)
{
    return g_registry.count;
}
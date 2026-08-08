/**
 * @file func_block_registry.c
 * @brief 预设函数块注册系统实现
 *
 * 实现全局预设函数块注册表的初始化、查找、分类筛选和资源管理。
 * 内置 75 个预设函数块，覆盖几何构造、度量计算、几何变换、
 * 代数运算、逻辑推导和分析六大类别。
 *
 * 内存管理：
 * - 使用 lv_malloc / lv_free / lv_strdup 进行内存管理
 * - 条目数组扩容由通用 lvRegistry 统一管理（lv_ensure_capacity）
 * - cleanup 时释放所有条目及其模板函数块
 */

#include "func_block_registry.h"
#include "lv/lv_xmacro.h"
#include "lv/preset_category.h" /* LV_PRESET_CATEGORY_ENTRY 单一事实来源 */
#include "lv/lv_registry.h"     /* 通用注册表（查重/扩容/删除/析构回调） */
#include "lv/lv_thread.h"       /* lv_once_t / lv_once */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv_internal.h"
#include "lv_utils.h"

/* ==================== 命名常量 ==================== */

/** 注册表初始容量（lv_registry_init 使用） */
#define REGISTRY_INITIAL_CAPACITY 32

/** 预设函数块 ID 起始偏移（引用 lv_internal.h 中的统一定义） */
#define PRESET_FB_ID_OFFSET lv_PRESET_ID_OFFSET

/* ==================== 全局注册表 ==================== */

/** 全局预设函数块注册表（文件级单例，lv_once 惰性初始化）
 *
 * 基于通用 lvRegistry：key = 预设名称，value = PresetEntry*（堆分配，
 * 由 preset_entry_destroy 回调接管释放）。注册表统一承担 strcmp 查重、
 * 尾部追加、lv_ensure_capacity 扩容与删除前移（保持注册顺序）。
 */
lv_REGISTRY_STATIC(func_block_registry, REGISTRY_INITIAL_CAPACITY);

/** 是否已完成内置预设注册（cleanup 后置 false，允许重新 init） */
static bool g_initialized = false;

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 释放单个预设条目的资源
 *
 * 释放条目中动态分配的 name、description 和 template_fb。
 * 释放后将条目字段置零。
 *
 * @param entry 预设条目指针
 */
static void free_preset_entry(PresetEntry *entry) {
    if (!entry)
        return;
    lv_free((void **) &entry->name);
    lv_free((void **) &entry->description);
    if (entry->template_fb) {
        func_block_destroy(entry->template_fb);
        entry->template_fb = NULL;
    }
    entry->category = PRESET_CATEGORY_CONSTRUCTION;
}

/**
 * @brief PresetEntry 的 lvRegistry destroy 回调适配器（void(*)(void*) 形态）
 *
 * 注册表 remove/clear/destroy 时调用：先释放条目内部资源
 * （name/description/template_fb），再释放 PresetEntry 外壳。
 */
static void preset_entry_destroy(void *value) {
    PresetEntry *entry = (PresetEntry *) value;
    if (!entry)
        return;
    free_preset_entry(entry);
    lv_free((void **) &entry);
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
static FuncBlock *create_preset_template(int id, const char *name, const char *description, int input_count,
                                         int output_count) {
    FuncBlock *fb = func_block_create(id);
    if (!fb)
        return NULL;

    if (name) {
        fb->name = lv_strdup(name);
        if (!fb->name) {
            func_block_destroy(fb);
            return NULL;
        }
    }

    if (description) {
        fb->description = lv_strdup(description);
        if (!fb->description) {
            func_block_destroy(fb);
            return NULL;
        }
    }

    fb->input_count = input_count;
    fb->output_count = output_count;
    fb->determinism = DETERMINISM_VERIFIED;

    return fb;
}

/**
 * @brief 向注册表添加一个预设条目（内部统一实现）
 *
 * 调用者应确保名称唯一（除非 check_duplicate 为 false）。
 * 当 deep_copy 为 true 时，对 fb 做深拷贝（调用者仍持有 fb 所有权）；
 * 当 deep_copy 为 false 时，直接接管 fb 的所有权（失败时由调用方释放）。
 *
 * 条目外壳（PresetEntry）与 name/description/template_fb 均堆分配，
 * 通过 lv_registry_put_ex 交给注册表（destroy = preset_entry_destroy）。
 *
 * @param name            预设名称（将被 lv_strdup 复制）
 * @param description     描述（将被 lv_strdup 复制，可为 NULL）
 * @param category        类别
 * @param fb              模板函数块
 * @param deep_copy       true 表示深拷贝 fb，false 表示直接接管 fb 所有权
 * @param check_duplicate true 表示检查同名预设是否已存在
 * @return true 添加成功，false 内存不足或同名已存在
 */
static bool add_preset_entry_ex(const char *name, const char *description, PresetCategory category, FuncBlock *fb,
                                bool deep_copy, bool check_duplicate) {
    if (!name || !fb) {
        return false;
    }

    func_block_registry_ensure();

    /* 可选：检查是否已存在同名预设（公共 API 路径需要） */
    if (check_duplicate) {
        if (lv_registry_get(&g_func_block_registry, name) != NULL) {
            return false; /* 同名预设已存在 */
        }
    }

    /* 构造 PresetEntry（堆分配外壳，内部字段由 free_preset_entry 管理） */
    PresetEntry *entry = (PresetEntry *) lv_calloc(1, sizeof(PresetEntry));
    if (!entry) {
        return false;
    }

    /* 复制名称 */
    entry->name = lv_strdup(name);
    if (!entry->name) {
        lv_free((void **) &entry);
        return false;
    }

    /* 复制描述（description 为 NULL 是允许的） */
    entry->description = description ? lv_strdup(description) : NULL;

    entry->category = category;

    /* 处理模板函数块：深拷贝或直接接管所有权 */
    if (deep_copy) {
        entry->template_fb = func_block_copy(fb);
        if (!entry->template_fb) {
            /* 深拷贝失败：fb 仍归调用方 */
            free_preset_entry(entry);
            lv_free((void **) &entry);
            return false;
        }
    } else {
        entry->template_fb = fb; /* 直接接管所有权 */
    }

    /* 写入通用注册表（内部查重 + 尾部追加 + lv_ensure_capacity 扩容） */
    if (!lv_registry_put_ex(&g_func_block_registry, name, entry, preset_entry_destroy)) {
        /* 失败（同名重复或内存不足）：释放本条目资源。
           deep_copy=false 且未接管时 fb 归调用方（register_builtin_presets
           失败路径手动 func_block_destroy(fb)），先摘除避免双重释放 */
        if (!deep_copy) {
            entry->template_fb = NULL;
        }
        free_preset_entry(entry);
        lv_free((void **) &entry);
        return false;
    }

    return true;
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

    /* 1 */ {"midpoint", "给定两点A、B，构造中点M。输入2个点，输出1个点。", 2, 1, PRESET_CATEGORY_CONSTRUCTION},
    /* 2 */
    {"perpendicular_bisector", "给定两点A、B，构造垂直平分线。输入2个点，输出2个点（线段端点）。", 2, 2,
     PRESET_CATEGORY_CONSTRUCTION},
    /* 3 */
    {"angle_bisector", "给定三点A、B、C（B为顶点），构造角平分线上的点。输入3个点，输出1个点。", 3, 1,
     PRESET_CATEGORY_CONSTRUCTION},
    /* 4 */
    {"parallel_line", "给定一条线段和一个外部点，过该点作平行线。输入3个点（线段两端+外部点），输出2个点。", 3, 2,
     PRESET_CATEGORY_CONSTRUCTION},
    /* 5 */
    {"perpendicular_line", "给定一条线段和一个外部点，过该点作垂线。输入3个点，输出2个点。", 3, 2,
     PRESET_CATEGORY_CONSTRUCTION},
    /* 6 */
    {"circle_by_center_radius", "给定圆心和半径上的点，构造圆。输入2个点，输出1个区域。", 2, 1,
     PRESET_CATEGORY_CONSTRUCTION},
    /* 7 */
    {"circle_by_three_points", "给定三个不共线的点，构造外接圆。输入3个点，输出1个区域。", 3, 1,
     PRESET_CATEGORY_CONSTRUCTION},
    /* 8 */
    {"line_intersection", "给定两条线段（四个端点），求交点。输入4个点，输出1个点。", 4, 1,
     PRESET_CATEGORY_CONSTRUCTION},
    /* 9 */
    {"reflection", "给定一点和一条线段，求关于该线的对称点。输入3个点，输出1个点。", 3, 1,
     PRESET_CATEGORY_CONSTRUCTION},
    /* 10 */
    {"equilateral_triangle", "给定一条边，构造等边三角形的第三个顶点。输入2个点，输出1个点。", 2, 1,
     PRESET_CATEGORY_CONSTRUCTION},

    /* ===== 度量计算类 (PRESET_CATEGORY_MEASUREMENT) ===== */

    /* 11 */ {"distance", "计算两点间的距离。输入2个点，输出1个点（距离标记点）。", 2, 1, PRESET_CATEGORY_MEASUREMENT},
    /* 12 */
    {"angle_measure", "计算三点形成的角度。输入3个点，输出1个点（角度标记点）。", 3, 1, PRESET_CATEGORY_MEASUREMENT},

    /* ===== 几何变换类 (PRESET_CATEGORY_TRANSFORMATION) ===== */

    /* 13 */
    {"translation", "将一个点沿向量平移。输入3个点（原点、目标点、待平移点），输出1个点。", 3, 1,
     PRESET_CATEGORY_TRANSFORMATION},
    /* 14 */
    {"rotation", "将一个点绕中心旋转指定角度。输入3个点（中心、参考点、待旋转点），输出1个点。", 3, 1,
     PRESET_CATEGORY_TRANSFORMATION},
    /* 15 */
    {"homothety", "将一个点关于中心按比例缩放。输入3个点（中心、参考点、待变换点），输出1个点。", 3, 1,
     PRESET_CATEGORY_TRANSFORMATION},

    /* ===== 代数运算类 (PRESET_CATEGORY_ALGEBRAIC) ===== */

    /* 16 */
    {"vector_add", "给定O、A、B三点，构造A+B对应的点C。输入3个点，输出1个点。", 3, 1, PRESET_CATEGORY_ALGEBRAIC},
    /* 17 */
    {"vector_scale", "给定O、A两点和比例系数，构造k*A对应的点。输入2个点，输出1个点。", 2, 1,
     PRESET_CATEGORY_ALGEBRAIC},

    /* ===== 逻辑推导类 (PRESET_CATEGORY_LOGIC) ===== */

    /* 18 */
    {"contradiction_detector", "检测约束系统中是否存在矛盾。输入0个端口，输出1个端口。", 0, 1, PRESET_CATEGORY_LOGIC},
    /* 19 */
    {"implication_chain", "将多个命题按蕴含关系链接。输入N个端口，输出1个端口。", -1, 1, PRESET_CATEGORY_LOGIC},

    /* ===== 几何构造类 (PRESET_CATEGORY_CONSTRUCTION) - 续 ===== */

    /* 20 */
    {"circumcenter", "给定三个点A、B、C，构造外接圆圆心。输入3个点，输出1个点。", 3, 1, PRESET_CATEGORY_CONSTRUCTION},
    /* 21 */
    {"incenter", "给定三个点A、B、C，构造内切圆圆心。输入3个点，输出1个点。", 3, 1, PRESET_CATEGORY_CONSTRUCTION},
    /* 22 */ {"centroid", "给定三个点A、B、C，构造重心。输入3个点，输出1个点。", 3, 1, PRESET_CATEGORY_CONSTRUCTION},
    /* 23 */ {"orthocenter", "给定三个点A、B、C，构造垂心。输入3个点，输出1个点。", 3, 1, PRESET_CATEGORY_CONSTRUCTION},
    /* 24 */
    {"foot_of_perpendicular", "给定一点P和一条线段AB，构造P到AB的垂足。输入3个点，输出1个点。", 3, 1,
     PRESET_CATEGORY_CONSTRUCTION},
    /* 25 */
    {"tangent_line_from_point", "给定一个点P和圆上两个点（圆心和半径点），构造切线。输入3个点，输出2个点（切点）。", 3,
     2, PRESET_CATEGORY_CONSTRUCTION},
    /* 26 */
    {"nine_point_circle", "给定三个点A、B、C，构造九点圆圆心。输入3个点，输出1个点。", 3, 1,
     PRESET_CATEGORY_CONSTRUCTION},
    /* 27 */
    {"excenter", "给定三个点A、B、C，构造A对边的旁心。输入3个点，输出1个点。", 3, 1, PRESET_CATEGORY_CONSTRUCTION},

    /* ===== 度量计算类 (PRESET_CATEGORY_MEASUREMENT) - 续 ===== */

    /* 28 */
    {"area_measure", "给定三个点A、B、C，计算三角形面积。输入3个点，输出1个点（面积标记点）。", 3, 1,
     PRESET_CATEGORY_MEASUREMENT},
    /* 29 */
    {"perimeter_measure", "给定三个点A、B、C，计算三角形周长。输入3个点，输出1个点（周长标记点）。", 3, 1,
     PRESET_CATEGORY_MEASUREMENT},
    /* 30 */
    {"ratio_measure", "给定四个点A、B、C、D，计算AB/CD比值。输入4个点，输出1个点。", 4, 1, PRESET_CATEGORY_MEASUREMENT},
    /* 31 */
    {"slope_measure", "给定两个点A、B，计算直线斜率。输入2个点，输出1个点。", 2, 1, PRESET_CATEGORY_MEASUREMENT},

    /* ===== 代数运算类 (PRESET_CATEGORY_ALGEBRAIC) - 续 ===== */

    /* 32 */
    {"vector_sub", "给定O、A、B三点，构造A-B对应的点C。输入3个点，输出1个点。", 3, 1, PRESET_CATEGORY_ALGEBRAIC},
    /* 33 */
    {"vector_dot_product", "给定O、A、B三点，计算OA·OB。输入3个点，输出1个点。", 3, 1, PRESET_CATEGORY_ALGEBRAIC},
    /* 34 */
    {"vector_cross_product_magnitude", "给定O、A、B三点，计算OA×OB的模。输入3个点，输出1个点。", 3, 1,
     PRESET_CATEGORY_ALGEBRAIC},
    /* 35 */
    {"vector_reflect", "给定O、A、B三点，将向量OA关于OB反射。输入3个点，输出1个点。", 3, 1, PRESET_CATEGORY_ALGEBRAIC},
    /* 36 */
    {"vector_project", "给定O、A、B三点，将向量OA投影到OB上。输入3个点，输出1个点。", 3, 1, PRESET_CATEGORY_ALGEBRAIC},

    /* ===== 几何变换类 (PRESET_CATEGORY_TRANSFORMATION) - 续 ===== */

    /* 37 */
    {"circle_inversion", "给定反演中心P、半径点R和待变换点Q，构造反演点。输入3个点，输出1个点。", 3, 1,
     PRESET_CATEGORY_TRANSFORMATION},
    /* 38 */
    {"affine_transform", "给定原点O、基向量端点A、B和待变换点P，构造变换后的点。输入4个点，输出1个点。", 4, 1,
     PRESET_CATEGORY_TRANSFORMATION},

    /* ===== 分析类 (PRESET_CATEGORY_ANALYSIS) ===== */

    /* 39 */
    {"taylor_approximation", "给定展开点P、参考点Q和待近似点R，构造泰勒近似点。输入3个点，输出1个点。", 3, 1,
     PRESET_CATEGORY_ANALYSIS},
    /* 40 */
    {"limit_point", "给定序列起点P、参考点Q，构造极限逼近点。输入2个点，输出1个点。", 2, 1, PRESET_CATEGORY_ANALYSIS},

    /* ===== 几何构造类 (PRESET_CATEGORY_CONSTRUCTION) - 新增 ===== */

    /* 41 */
    {"rectangle", "给定三个点构造矩形，A-B为一边，C确定矩形所在平面方向。输入3个点，输出1个点（D，矩形第四顶点）。", 3,
     1, PRESET_CATEGORY_CONSTRUCTION},
    /* 42 */
    {"square", "给定一条边AB，构造正方形ABCD。输入2个点，输出2个点（C、D为另外两个顶点）。", 2, 2,
     PRESET_CATEGORY_CONSTRUCTION},
    /* 43 */
    {"regular_polygon",
     "给定中心O和半径r，构造正n边形。输入1个点（中心）+ 1个数值参数（半径），输出n个点（正n边形顶点）。", 2, -1,
     PRESET_CATEGORY_CONSTRUCTION},
    /* 44 */
    {"tangent_line",
     "从圆外一点P作圆的两条切线。输入1个点（圆外点P）+ 1个圆（由圆心和半径点确定），输出2个线段（两条切线）。", 3, 2,
     PRESET_CATEGORY_CONSTRUCTION},
    /* 45 */
    {"circumcircle", "给定三个不共线的点，构造其外接圆。输入3个点（A、B、C），输出1个圆（外接圆）。", 3, 1,
     PRESET_CATEGORY_CONSTRUCTION},
    /* 46 */
    {"incircle",
     "给定三角形的三个顶点，构造内切圆和内心。输入3个点（三角形三顶点），输出1个圆（内切圆）+ 1个点（内心）。", 3, 2,
     PRESET_CATEGORY_CONSTRUCTION},
    /* 47 */
    {"golden_ratio",
     "在线段AB上求黄金分割点P，使AP:PB = φ:1。输入2个点（线段AB），输出1个点（黄金分割点P，使AP/AB = (√5-1)/2）。", 2,
     1, PRESET_CATEGORY_CONSTRUCTION},
    /* 48 */
    {"power_of_point",
     "计算点P关于圆的幂（power of a point）。输入1个点（P）+ 1个圆（由圆心和半径点确定），输出1个数值（|OP|² - r²）。",
     3, 1, PRESET_CATEGORY_CONSTRUCTION},
    /* 49 */
    {"harmonic_conjugate",
     "给定共线三点A、B、C，求其调和共轭点D，使(A,B;C,D)=-1。输入3个点（A、B、C共线），输出1个点（D）。", 3, 1,
     PRESET_CATEGORY_CONSTRUCTION},

    /* ===== 度量计算类 (PRESET_CATEGORY_MEASUREMENT) - 新增 ===== */

    /* 50 */
    {"area_triangle", "使用向量叉积计算三角形面积。输入3个点（三角形三顶点），输出1个数值（面积）。", 3, 1,
     PRESET_CATEGORY_MEASUREMENT},
    /* 51 */
    {"area_polygon",
     "使用鞋带公式（Shoelace formula）计算多边形面积。输入n个点（多边形顶点，按顺序），输出1个数值（有向面积）。", -1,
     1, PRESET_CATEGORY_MEASUREMENT},
    /* 52 */
    {"perimeter", "计算多边形或折线的总周长。输入n个点（多边形顶点），输出1个数值（周长）。", -1, 1,
     PRESET_CATEGORY_MEASUREMENT},
    /* 53 */ {"slope", "计算经过两点的直线斜率。输入2个点，输出1个数值（斜率）。", 2, 1, PRESET_CATEGORY_MEASUREMENT},
    /* 54 */
    {"curvature", "使用三点法计算离散曲率。输入3个点（曲线上的相邻三点），输出1个数值（曲率）。", 3, 1,
     PRESET_CATEGORY_MEASUREMENT},
    /* 55 */
    {"cross_ratio", "计算四个共线点的交比（cross ratio）。输入4个共线点，输出1个数值（交比 (A,B;C,D)）。", 4, 1,
     PRESET_CATEGORY_MEASUREMENT},

    /* ===== 几何变换类 (PRESET_CATEGORY_TRANSFORMATION) - 新增 ===== */

    /* 56 */
    {"glide_reflection",
     "沿直线做反射后再沿直线方向平移的复合变换。输入1条线段（反射轴，2个点）+ "
     "1个向量（平移方向，2个点），输出1个点（变换后的点）。",
     4, 1, PRESET_CATEGORY_TRANSFORMATION},
    /* 57 */
    {"inversion",
     "关于给定圆的反演变换。输入1个圆（反演圆，由圆心和半径点确定）+ 1个点（待变换点），输出1个点（反演点）。", 3, 1,
     PRESET_CATEGORY_TRANSFORMATION},
    /* 58 */
    {"spiral_similarity",
     "以给定点为中心的旋转+缩放复合变换。输入1个中心点 + 旋转角度 + 缩放比例，输出1个点（变换后的点）。", 3, 1,
     PRESET_CATEGORY_TRANSFORMATION},
    /* 59 */
    {"projective_transform", "由四对对应点确定的射影变换（单应性）。输入4对对应点（8个点），输出1个点（变换后的点）。",
     8, 1, PRESET_CATEGORY_TRANSFORMATION},

    /* ===== 代数运算类 (PRESET_CATEGORY_ALGEBRAIC) - 新增 ===== */

    /* 60 */
    {"polynomial_add", "计算两个多项式的加法结果。输入2个多项式（系数列表），输出1个多项式（和）。", 2, 1,
     PRESET_CATEGORY_ALGEBRAIC},
    /* 61 */
    {"polynomial_multiply", "计算两个多项式的乘法结果。输入2个多项式，输出1个多项式（积）。", 2, 1,
     PRESET_CATEGORY_ALGEBRAIC},
    /* 62 */
    {"polynomial_gcd", "使用欧几里得算法计算多项式最大公因式。输入2个多项式，输出1个多项式（GCD）。", 2, 1,
     PRESET_CATEGORY_ALGEBRAIC},
    /* 63 */
    {"resultant", "计算两个多项式的结式（resultant），用于判断公共零点。输入2个多项式，输出1个数值（结式）。", 2, 1,
     PRESET_CATEGORY_ALGEBRAIC},
    /* 64 */
    {"determinant_2x2", "计算2×2矩阵的行列式。输入4个数值（矩阵元素），输出1个数值（行列式值）。", 4, 1,
     PRESET_CATEGORY_ALGEBRAIC},
    /* 65 */
    {"determinant_3x3", "计算3×3矩阵的行列式（萨吕法则）。输入9个数值，输出1个数值。", 9, 1, PRESET_CATEGORY_ALGEBRAIC},
    /* 66 */ {"matrix_multiply", "计算两个矩阵的乘积。输入2个矩阵，输出1个矩阵。", 2, 1, PRESET_CATEGORY_ALGEBRAIC},
    /* 67 */
    {"eigenvalues_2x2", "计算2×2矩阵的特征值（求解特征方程）。输入4个数值，输出2个数值（特征值）。", 4, 2,
     PRESET_CATEGORY_ALGEBRAIC},

    /* ===== 逻辑推导类 (PRESET_CATEGORY_LOGIC) - 新增 ===== */

    /* 68 */
    {"modus_ponens", "假言推理：从P和P→Q推导Q。输入命题P + 蕴含命题(P→Q)，输出命题Q。", 2, 1, PRESET_CATEGORY_LOGIC},
    /* 69 */
    {"modus_tollens", "否定后件：从P→Q和¬Q推导¬P。输入蕴含命题(P→Q) + 命题¬Q，输出命题¬P。", 2, 1,
     PRESET_CATEGORY_LOGIC},
    /* 70 */
    {"conjunction", "合取引入规则：从P和Q推导P∧Q。输入命题P + 命题Q，输出命题(P∧Q)。", 2, 1, PRESET_CATEGORY_LOGIC},
    /* 71 */
    {"disjunction_intro", "析取引入规则：从P推导P∨Q（Q为任意命题）。输入命题P，输出命题(P∨Q)。", 1, 1,
     PRESET_CATEGORY_LOGIC},
    /* 72 */
    {"negation_intro", "否定引入规则：假设P导致矛盾，则推导¬P。输入假设P推导出矛盾，输出命题¬P。", 1, 1,
     PRESET_CATEGORY_LOGIC},
    /* 73 */
    {"universal_intro", "全称量化引入：从对任意个体的证明推导全称命题。输入对任意a推导P(a)，输出命题∀x.P(x)。", 1, 1,
     PRESET_CATEGORY_LOGIC},
    /* 74 */
    {"existential_intro", "存在量化引入：从具体实例推导存在命题。输入命题P(t)（t为具体项），输出命题∃x.P(x)。", 1, 1,
     PRESET_CATEGORY_LOGIC},
    /* 75 */
    {"proof_by_contradiction", "反证法：假设¬P导致矛盾，则推导P。输入假设¬P推导出矛盾，输出命题P。", 1, 1,
     PRESET_CATEGORY_LOGIC},
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
static bool register_builtin_presets(void) {
    const int count = (int) (sizeof(g_builtin_presets) / sizeof(g_builtin_presets[0]));

    for (int i = 0; i < count; i++) {
        const BuiltinPresetDef *def = &g_builtin_presets[i];
        int id = PRESET_FB_ID_OFFSET + i;

        FuncBlock *fb = create_preset_template(id, def->name, def->description, def->input_count, def->output_count);
        if (!fb) {
            return false; /* 模板创建失败，无需清理 fb */
        }

        /* 内部注册路径：不检查重复，直接接管 fb 所有权 */
        if (!add_preset_entry_ex(def->name, def->description, def->category, fb, false, false)) {
            /* add_preset_entry_ex 失败时未接管 fb（deep_copy=false 失败路径
               已将 entry->template_fb 置 NULL），在此处手动释放未被接管的 fb */
            func_block_destroy(fb);
            return false;
        }
        /* fb 所有权已成功转移给注册表，无需再释放 */
    }

    return true;
}

/* ==================== 公共 API 实现 ==================== */

bool func_block_registry_init(void) {
    /* 确保互斥锁就绪（首次调用时初始化注册表） */
    func_block_registry_ensure();

    /* 幂等操作：已初始化则直接返回 */
    if (g_initialized) {
        return true;
    }

    /* 注册所有内置预设 */
    if (!register_builtin_presets()) {
        /* 内置预设注册失败，清理已注册的部分 */
        lv_func_block_registry_cleanup();
        return false;
    }

    g_initialized = true;
    return true;
}

void lv_func_block_registry_cleanup(void) {
    /* 确保互斥锁就绪（从未 init 时直接清理也安全） */
    func_block_registry_ensure();

    /* 释放所有条目（destroy 回调 + 注册表 name），保留数组与互斥锁
       以便再次 init；lv_registry_clear 可安全多次调用（幂等） */
    lv_registry_clear(&g_func_block_registry);

    g_initialized = false;
}

int func_block_registry_unregister(const char *name) {
    if (!name || !g_initialized)
        return -1;

    func_block_registry_ensure();

    /* 委托注册表删除：先调用 destroy 回调释放 PresetEntry，
       再释放内部 name 并将后续条目前移紧凑 */
    return lv_registry_remove(&g_func_block_registry, name) ? 0 : -1;
}

bool func_block_register(const char *name, const char *description, PresetCategory category, FuncBlock *fb) {
    /*
     * 公共 API：检查同名重复 + 深拷贝 fb。
     * 统一委托给 add_preset_entry_ex，消除代码重复。
     */
    return add_preset_entry_ex(name, description, category, fb, true, true);
}

FuncBlock *func_block_registry_lookup(const char *name) {
    if (!name)
        return NULL;

    func_block_registry_ensure();

    PresetEntry *entry = (PresetEntry *) lv_registry_get(&g_func_block_registry, name);
    if (!entry) {
        return NULL; /* 未找到 */
    }

    /* 创建深拷贝返回给调用者 */
    return func_block_copy(entry->template_fb);
}

PresetEntry *func_block_registry_find(const char *name) {
    if (!name)
        return NULL;

    func_block_registry_ensure();

    /* 返回注册表 value（PresetEntry*），未找到返回 NULL */
    return (PresetEntry *) lv_registry_get(&g_func_block_registry, name);
}

int func_block_registry_find_by_category(PresetCategory category, PresetEntry **out_entries, int max_count) {
    if (!out_entries || max_count <= 0) {
        return 0;
    }

    func_block_registry_ensure();

    int found = 0;
    int total = 0;
    const int count = lv_registry_count(&g_func_block_registry);
    for (int i = 0; i < count; i++) {
        const char *rname = NULL;
        void *value = NULL;
        if (!lv_registry_get_at(&g_func_block_registry, i, &rname, &value)) {
            break;
        }
        PresetEntry *entry = (PresetEntry *) value;
        if (entry->category == category) {
            total++;
            if (found < max_count) {
                out_entries[found++] = entry;
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
/* ================================================================
 * 枚举 -> 名称 映射表（数据表化，替代 switch）
 * ================================================================ */

/** @brief preset_category_to_string 名称表（按枚举值升序，
 *  由共享条目宏 LV_PRESET_CATEGORY_ENTRY 生成，中文名与查询侧 UI 统一） */
#define LV_PRESET_CATEGORY_ROW_ZH(ENUM, EN_KEY, ZH_NAME) { ZH_NAME, ENUM },
static const lvStrToEnumEntry s_preset_category_to_string_entries[] = {
    LV_PRESET_CATEGORY_ENTRY(LV_PRESET_CATEGORY_ROW_ZH)
};
#undef LV_PRESET_CATEGORY_ROW_ZH

const char *preset_category_to_string(PresetCategory cat) {
    return lv_enum_to_str(s_preset_category_to_string_entries, lv_ARRAY_SIZE(s_preset_category_to_string_entries), (int) cat, "未知类别");
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
bool preset_category_from_string(const char *str, PresetCategory *category) {
    if (!str || !category)
        return false;

    /* 中文名称映射（与 preset_category_to_string 返回值对应，由共享条目宏生成） */
    static const struct {
        const char *name;
        PresetCategory cat;
    } cn_map[] = {
#define LV_PRESET_CATEGORY_ROW_CN(ENUM, EN_KEY, ZH_NAME) { ZH_NAME, ENUM },
        LV_PRESET_CATEGORY_ENTRY(LV_PRESET_CATEGORY_ROW_CN)
#undef LV_PRESET_CATEGORY_ROW_CN
    };

    for (size_t i = 0; i < sizeof(cn_map) / sizeof(cn_map[0]); i++) {
        if (strcmp(cn_map[i].name, str) == 0) {
            *category = cn_map[i].cat;
            return true;
        }
    }

    /* 英文名称映射（用于序列化/反序列化，由共享条目宏生成） */
    static const struct {
        const char *name;
        PresetCategory cat;
    } en_map[] = {
#define LV_PRESET_CATEGORY_ROW_EN(ENUM, EN_KEY, ZH_NAME) { EN_KEY, ENUM },
        LV_PRESET_CATEGORY_ENTRY(LV_PRESET_CATEGORY_ROW_EN)
#undef LV_PRESET_CATEGORY_ROW_EN
    };

    for (size_t i = 0; i < sizeof(en_map) / sizeof(en_map[0]); i++) {
        if (strcmp(en_map[i].name, str) == 0) {
            *category = en_map[i].cat;
            return true;
        }
    }

    return false;
}

int func_block_registry_get_count(void) {
    func_block_registry_ensure();
    return lv_registry_count(&g_func_block_registry);
}

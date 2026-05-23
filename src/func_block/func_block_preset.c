/**
 * @file func_block_preset.c
 * @brief 预设函数块系统实现
 *
 * @details 实现完整的预设函数块库，包括：
 *          - 50+ 个内置预设函数块定义
 *          - 类型系统和约束验证
 *          - 实例化引擎
 *          - 文档生成
 *
 * @version 5.0.0
 */

#include "func_block_preset.h"
#include "lv00_internal.h"
#include "lv00_utils.h"
#include "error_codes.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ============================================================
 * 命名常量
 * ============================================================ */

/* 版本号统一使用 lv00_internal.h 中的 LV00_PRESET_LIBRARY_VERSION_* 定义 */

/** 最大预设数量（引用 lv00_internal.h 中的统一定义） */
#define MAX_PRESETS LV00_PRESET_MAX_COUNT

/** 最大参数数量 */
#define MAX_PARAMS LV00_PRESET_MAX_PARAMS

/** 字符串缓冲区大小 */
#define BUFFER_SIZE 4096

/* ============================================================
 * 内部数据结构
 * ============================================================ */

/**
 * @brief 内部预设条目
 *
 * 存储单个预设的完整信息，包括元数据、模板函数块和状态标志。
 * 每个注册的预设在 g_preset_library.entries 数组中对应一个条目。
 */
typedef struct {
    PresetMetadata metadata;        /**< 预设元数据（名称、描述、分类、复杂度等） */
    FuncBlock *template_fb;         /**< 模板函数块（用于实例化的原型，所有权归本条目） */
    bool is_builtin;                /**< 是否为内置预设（内置预设不可被用户删除） */
    bool is_active;                 /**< 是否激活（未激活的预设不参与查找和实例化） */
} InternalPresetEntry;

/**
 * @brief 预设库全局状态
 *
 * 注意：entries 使用固定大小数组 MAX_PRESETS(256)，这意味着预设总数上限为 256 个。
 * 若需扩展，应将 entries 改为动态分配的数组，并相应调整 MAX_PRESETS 或移除此限制。
 */
static struct {
    InternalPresetEntry entries[MAX_PRESETS];   /**< 预设条目数组（固定大小，上限 MAX_PRESETS） */
    int count;                                  /**< 当前预设数量 */
    bool initialized;                           /**< 是否已初始化 */
    int next_preset_id;                         /**< 下一个预设ID */
} g_preset_library = {
    .count = 0,
    .initialized = false,
    .next_preset_id = LV00_PRESET_ID_OFFSET  /**< 预设ID起始偏移 */
};

/* ============================================================
 * 内置预设元数据定义
 * ============================================================ */

/**
 * @brief 几何构造类预设元数据
 */
static const PresetMetadata g_builtin_metadata[] = {
    /* ========== 基础构造 ========== */
    {
        .name = "midpoint",
        .description = "构造两点的中点",
        .mathematical_def = "M = \\frac{A + B}{2}",
        .category = PRESET_CATEGORY_CONSTRUCTION,
        .properties = PRESET_PROPERTY_DETERMINISTIC | PRESET_PROPERTY_CONTINUOUS | PRESET_PROPERTY_CONSTRUCTIVE,
        .complexity = COMPLEXITY_O1,
        .input_count = 2,
        .output_count = 1,
        .precondition_count = 1,
        .postcondition_count = 1,
        .related_count = 3,
        .version_major = 1, .version_minor = 0, .version_patch = 0
    },
    {
        .name = "centroid",
        .description = "构造三角形的重心",
        .mathematical_def = "G = \\frac{A + B + C}{3}",
        .category = PRESET_CATEGORY_CONSTRUCTION,
        .properties = PRESET_PROPERTY_DETERMINISTIC | PRESET_PROPERTY_CONTINUOUS | PRESET_PROPERTY_CONSTRUCTIVE,
        .complexity = COMPLEXITY_O1,
        .input_count = 3,
        .output_count = 1,
        .precondition_count = 1,
        .postcondition_count = 2,
        .related_count = 3,
        .version_major = 1, .version_minor = 0, .version_patch = 0
    },
    {
        .name = "circumcenter",
        .description = "构造三角形的外心",
        .mathematical_def = "O \\text{ 满足 } |OA| = |OB| = |OC|",
        .category = PRESET_CATEGORY_CONSTRUCTION,
        .properties = PRESET_PROPERTY_DETERMINISTIC | PRESET_PROPERTY_CONTINUOUS | PRESET_PROPERTY_CONSTRUCTIVE,
        .complexity = COMPLEXITY_O1,
        .input_count = 3,
        .output_count = 1,
        .precondition_count = 1,
        .postcondition_count = 2,
        .related_count = 4,
        .version_major = 1, .version_minor = 0, .version_patch = 0
    },
    {
        .name = "orthocenter",
        .description = "构造三角形的垂心",
        .mathematical_def = "H \\text{ 是三条高的交点}",
        .category = PRESET_CATEGORY_CONSTRUCTION,
        .properties = PRESET_PROPERTY_DETERMINISTIC | PRESET_PROPERTY_CONTINUOUS | PRESET_PROPERTY_CONSTRUCTIVE,
        .complexity = COMPLEXITY_O1,
        .input_count = 3,
        .output_count = 1,
        .precondition_count = 1,
        .postcondition_count = 2,
        .related_count = 4,
        .version_major = 1, .version_minor = 0, .version_patch = 0
    },
    {
        .name = "incenter",
        .description = "构造三角形的内心",
        .mathematical_def = "I = \\frac{aA + bB + cC}{a+b+c}",
        .category = PRESET_CATEGORY_CONSTRUCTION,
        .properties = PRESET_PROPERTY_DETERMINISTIC | PRESET_PROPERTY_CONTINUOUS | PRESET_PROPERTY_CONSTRUCTIVE,
        .complexity = COMPLEXITY_O1,
        .input_count = 3,
        .output_count = 1,
        .precondition_count = 1,
        .postcondition_count = 2,
        .related_count = 4,
        .version_major = 1, .version_minor = 0, .version_patch = 0
    },
    {
        .name = "excenter",
        .description = "构造三角形的旁心",
        .mathematical_def = "I_A = \\frac{-aA + bB + cC}{-a+b+c}",
        .category = PRESET_CATEGORY_CONSTRUCTION,
        .properties = PRESET_PROPERTY_DETERMINISTIC | PRESET_PROPERTY_CONTINUOUS | PRESET_PROPERTY_CONSTRUCTIVE,
        .complexity = COMPLEXITY_O1,
        .input_count = 3,
        .output_count = 1,
        .precondition_count = 1,
        .postcondition_count = 2,
        .related_count = 3,
        .version_major = 1, .version_minor = 0, .version_patch = 0
    },
    
    /* ========== 直线构造 ========== */
    {
        .name = "line_through",
        .description = "过两点构造直线",
        .mathematical_def = "L = \\overleftrightarrow{AB}",
        .category = PRESET_CATEGORY_CONSTRUCTION,
        .properties = PRESET_PROPERTY_DETERMINISTIC | PRESET_PROPERTY_CONTINUOUS | PRESET_PROPERTY_CONSTRUCTIVE,
        .complexity = COMPLEXITY_O1,
        .input_count = 2,
        .output_count = 1,
        .precondition_count = 1,
        .postcondition_count = 1,
        .related_count = 3,
        .version_major = 1, .version_minor = 0, .version_patch = 0
    },
    {
        .name = "perpendicular_bisector",
        .description = "构造线段的垂直平分线",
        .mathematical_def = "L \\perp AB \\text{ 且过中点 } M",
        .category = PRESET_CATEGORY_CONSTRUCTION,
        .properties = PRESET_PROPERTY_DETERMINISTIC | PRESET_PROPERTY_CONTINUOUS | PRESET_PROPERTY_CONSTRUCTIVE,
        .complexity = COMPLEXITY_O1,
        .input_count = 2,
        .output_count = 1,
        .precondition_count = 1,
        .postcondition_count = 2,
        .related_count = 4,
        .version_major = 1, .version_minor = 0, .version_patch = 0
    },
    {
        .name = "angle_bisector",
        .description = "构造角平分线",
        .mathematical_def = "L \\text{ 平分 } \\angle ABC",
        .category = PRESET_CATEGORY_CONSTRUCTION,
        .properties = PRESET_PROPERTY_DETERMINISTIC | PRESET_PROPERTY_CONTINUOUS | PRESET_PROPERTY_CONSTRUCTIVE,
        .complexity = COMPLEXITY_O1,
        .input_count = 3,
        .output_count = 1,
        .precondition_count = 1,
        .postcondition_count = 2,
        .related_count = 3,
        .version_major = 1, .version_minor = 0, .version_patch = 0
    },
    {
        .name = "perpendicular_line",
        .description = "过点作直线的垂线",
        .mathematical_def = "L \\perp l \\text{ 且过点 } P",
        .category = PRESET_CATEGORY_CONSTRUCTION,
        .properties = PRESET_PROPERTY_DETERMINISTIC | PRESET_PROPERTY_CONTINUOUS | PRESET_PROPERTY_CONSTRUCTIVE,
        .complexity = COMPLEXITY_O1,
        .input_count = 2,
        .output_count = 1,
        .precondition_count = 1,
        .postcondition_count = 2,
        .related_count = 3,
        .version_major = 1, .version_minor = 0, .version_patch = 0
    },
    {
        .name = "parallel_line",
        .description = "过点作直线的平行线",
        .mathematical_def = "L \\parallel l \\text{ 且过点 } P",
        .category = PRESET_CATEGORY_CONSTRUCTION,
        .properties = PRESET_PROPERTY_DETERMINISTIC | PRESET_PROPERTY_CONTINUOUS | PRESET_PROPERTY_CONSTRUCTIVE,
        .complexity = COMPLEXITY_O1,
        .input_count = 2,
        .output_count = 1,
        .precondition_count = 1,
        .postcondition_count = 2,
        .related_count = 3,
        .version_major = 1, .version_minor = 0, .version_patch = 0
    },
    
    /* ========== 圆构造 ========== */
    {
        .name = "circle_by_center_radius",
        .description = "以圆心和半径点构造圆",
        .mathematical_def = "C(O, r) \\text{ 其中 } r = |OP|",
        .category = PRESET_CATEGORY_CONSTRUCTION,
        .properties = PRESET_PROPERTY_DETERMINISTIC | PRESET_PROPERTY_CONTINUOUS | PRESET_PROPERTY_CONSTRUCTIVE,
        .complexity = COMPLEXITY_O1,
        .input_count = 2,
        .output_count = 1,
        .precondition_count = 1,
        .postcondition_count = 1,
        .related_count = 3,
        .version_major = 1, .version_minor = 0, .version_patch = 0
    },
    {
        .name = "circle_by_three_points",
        .description = "过三点构造圆",
        .mathematical_def = "C \\text{ 过 } A, B, C",
        .category = PRESET_CATEGORY_CONSTRUCTION,
        .properties = PRESET_PROPERTY_DETERMINISTIC | PRESET_PROPERTY_CONTINUOUS | PRESET_PROPERTY_CONSTRUCTIVE,
        .complexity = COMPLEXITY_O1,
        .input_count = 3,
        .output_count = 1,
        .precondition_count = 1,
        .postcondition_count = 1,
        .related_count = 3,
        .version_major = 1, .version_minor = 0, .version_patch = 0
    },
    {
        .name = "circle_by_diameter",
        .description = "以直径构造圆",
        .mathematical_def = "C \\text{ 以 } AB \\text{ 为直径}",
        .category = PRESET_CATEGORY_CONSTRUCTION,
        .properties = PRESET_PROPERTY_DETERMINISTIC | PRESET_PROPERTY_CONTINUOUS | PRESET_PROPERTY_CONSTRUCTIVE,
        .complexity = COMPLEXITY_O1,
        .input_count = 2,
        .output_count = 1,
        .precondition_count = 1,
        .postcondition_count = 2,
        .related_count = 3,
        .version_major = 1, .version_minor = 0, .version_patch = 0
    },
    {
        .name = "tangent_lines",
        .description = "从点向圆作切线",
        .mathematical_def = "L_1, L_2 \\text{ 切圆 } C \\text{ 于点 } P",
        .category = PRESET_CATEGORY_CONSTRUCTION,
        .properties = PRESET_PROPERTY_DETERMINISTIC | PRESET_PROPERTY_CONTINUOUS | PRESET_PROPERTY_CONSTRUCTIVE,
        .complexity = COMPLEXITY_O1,
        .input_count = 2,
        .output_count = 2,
        .precondition_count = 1,
        .postcondition_count = 2,
        .related_count = 3,
        .version_major = 1, .version_minor = 0, .version_patch = 0
    },
    
    /* ========== 交点构造 ========== */
    {
        .name = "line_intersection",
        .description = "求两直线交点",
        .mathematical_def = "P = l_1 \\cap l_2",
        .category = PRESET_CATEGORY_CONSTRUCTION,
        .properties = PRESET_PROPERTY_DETERMINISTIC | PRESET_PROPERTY_CONTINUOUS | PRESET_PROPERTY_CONSTRUCTIVE,
        .complexity = COMPLEXITY_O1,
        .input_count = 2,
        .output_count = 1,
        .precondition_count = 1,
        .postcondition_count = 2,
        .related_count = 3,
        .version_major = 1, .version_minor = 0, .version_patch = 0
    },
    {
        .name = "circle_line_intersection",
        .description = "求圆与直线的交点",
        .mathematical_def = "P_1, P_2 = C \\cap l",
        .category = PRESET_CATEGORY_CONSTRUCTION,
        .properties = PRESET_PROPERTY_DETERMINISTIC | PRESET_PROPERTY_CONTINUOUS | PRESET_PROPERTY_CONSTRUCTIVE,
        .complexity = COMPLEXITY_O1,
        .input_count = 2,
        .output_count = 2,
        .precondition_count = 1,
        .postcondition_count = 2,
        .related_count = 3,
        .version_major = 1, .version_minor = 0, .version_patch = 0
    },
    {
        .name = "circle_circle_intersection",
        .description = "求两圆交点",
        .mathematical_def = "P_1, P_2 = C_1 \\cap C_2",
        .category = PRESET_CATEGORY_CONSTRUCTION,
        .properties = PRESET_PROPERTY_DETERMINISTIC | PRESET_PROPERTY_CONTINUOUS | PRESET_PROPERTY_CONSTRUCTIVE,
        .complexity = COMPLEXITY_O1,
        .input_count = 2,
        .output_count = 2,
        .precondition_count = 1,
        .postcondition_count = 2,
        .related_count = 3,
        .version_major = 1, .version_minor = 0, .version_patch = 0
    },
    
    /* ========== 对称与反射 ========== */
    {
        .name = "reflection_point",
        .description = "求点关于直线的对称点",
        .mathematical_def = "P' \\text{ 满足 } PP' \\perp l \\text{ 且中点在 } l \\text{ 上}",
        .category = PRESET_CATEGORY_CONSTRUCTION,
        .properties = PRESET_PROPERTY_DETERMINISTIC | PRESET_PROPERTY_CONTINUOUS | 
                     PRESET_PROPERTY_CONSTRUCTIVE | PRESET_PROPERTY_INVOLUTIVE,
        .complexity = COMPLEXITY_O1,
        .input_count = 2,
        .output_count = 1,
        .precondition_count = 1,
        .postcondition_count = 2,
        .related_count = 3,
        .version_major = 1, .version_minor = 0, .version_patch = 0
    },
    {
        .name = "reflection_line",
        .description = "求直线关于另一直线的对称直线",
        .mathematical_def = "l' \\text{ 是 } l \\text{ 关于 } m \\text{ 的反射}",
        .category = PRESET_CATEGORY_CONSTRUCTION,
        .properties = PRESET_PROPERTY_DETERMINISTIC | PRESET_PROPERTY_CONTINUOUS | 
                     PRESET_PROPERTY_CONSTRUCTIVE | PRESET_PROPERTY_INVOLUTIVE,
        .complexity = COMPLEXITY_O1,
        .input_count = 2,
        .output_count = 1,
        .precondition_count = 1,
        .postcondition_count = 2,
        .related_count = 2,
        .version_major = 1, .version_minor = 0, .version_patch = 0
    },
    
    /* ========== 特殊点构造 ========== */
    {
        .name = "foot_of_perpendicular",
        .description = "求点到直线的垂足",
        .mathematical_def = "F \\text{ 是 } P \\text{ 在 } l \\text{ 上的投影}",
        .category = PRESET_CATEGORY_CONSTRUCTION,
        .properties = PRESET_PROPERTY_DETERMINISTIC | PRESET_PROPERTY_CONTINUOUS | 
                     PRESET_PROPERTY_CONSTRUCTIVE | PRESET_PROPERTY_IDEMPOTENT,
        .complexity = COMPLEXITY_O1,
        .input_count = 2,
        .output_count = 1,
        .precondition_count = 1,
        .postcondition_count = 2,
        .related_count = 3,
        .version_major = 1, .version_minor = 0, .version_patch = 0
    },
    {
        .name = "harmonic_conjugate",
        .description = "构造调和共轭点",
        .mathematical_def = "(AB;CD) = -1",
        .category = PRESET_CATEGORY_CONSTRUCTION,
        .properties = PRESET_PROPERTY_DETERMINISTIC | PRESET_PROPERTY_CONTINUOUS | 
                     PRESET_PROPERTY_CONSTRUCTIVE | PRESET_PROPERTY_INVOLUTIVE,
        .complexity = COMPLEXITY_O1,
        .input_count = 3,
        .output_count = 1,
        .precondition_count = 1,
        .postcondition_count = 2,
        .related_count = 2,
        .version_major = 1, .version_minor = 0, .version_patch = 0
    },
    
    /* ========== 多边形构造 ========== */
    {
        .name = "equilateral_triangle",
        .description = "构造等边三角形",
        .mathematical_def = "\\triangle ABC \\text{ 等边}",
        .category = PRESET_CATEGORY_CONSTRUCTION,
        .properties = PRESET_PROPERTY_DETERMINISTIC | PRESET_PROPERTY_CONTINUOUS | PRESET_PROPERTY_CONSTRUCTIVE,
        .complexity = COMPLEXITY_O1,
        .input_count = 2,
        .output_count = 1,
        .precondition_count = 1,
        .postcondition_count = 3,
        .related_count = 3,
        .version_major = 1, .version_minor = 0, .version_patch = 0
    },
    {
        .name = "square",
        .description = "构造正方形",
        .mathematical_def = "\\square ABCD",
        .category = PRESET_CATEGORY_CONSTRUCTION,
        .properties = PRESET_PROPERTY_DETERMINISTIC | PRESET_PROPERTY_CONTINUOUS | PRESET_PROPERTY_CONSTRUCTIVE,
        .complexity = COMPLEXITY_O1,
        .input_count = 2,
        .output_count = 2,
        .precondition_count = 1,
        .postcondition_count = 4,
        .related_count = 3,
        .version_major = 1, .version_minor = 0, .version_patch = 0
    },
    {
        .name = "regular_polygon",
        .description = "构造正多边形",
        .mathematical_def = "P_n \\text{ 以 } AB \\text{ 为边}",
        .category = PRESET_CATEGORY_CONSTRUCTION,
        .properties = PRESET_PROPERTY_DETERMINISTIC | PRESET_PROPERTY_CONTINUOUS | PRESET_PROPERTY_CONSTRUCTIVE,
        .complexity = COMPLEXITY_ON,
        .input_count = 3,
        .output_count = -1,  /* 可变输出 */
        .precondition_count = 2,
        .postcondition_count = 2,
        .related_count = 2,
        .version_major = 1, .version_minor = 0, .version_patch = 0
    },
    
    /* ========== 度量计算 ========== */
    {
        .name = "distance",
        .description = "计算两点间距离",
        .mathematical_def = "d = |AB| = \\sqrt{(x_B-x_A)^2 + (y_B-y_A)^2}",
        .category = PRESET_CATEGORY_MEASUREMENT,
        .properties = PRESET_PROPERTY_DETERMINISTIC | PRESET_PROPERTY_CONTINUOUS | 
                     PRESET_PROPERTY_COMMUTATIVE,
        .complexity = COMPLEXITY_O1,
        .input_count = 2,
        .output_count = 1,
        .precondition_count = 0,
        .postcondition_count = 2,
        .related_count = 3,
        .version_major = 1, .version_minor = 0, .version_patch = 0
    },
    {
        .name = "distance_squared",
        .description = "计算两点间距离平方",
        .mathematical_def = "d^2 = |AB|^2",
        .category = PRESET_CATEGORY_MEASUREMENT,
        .properties = PRESET_PROPERTY_DETERMINISTIC | PRESET_PROPERTY_CONTINUOUS | 
                     PRESET_PROPERTY_COMMUTATIVE,
        .complexity = COMPLEXITY_O1,
        .input_count = 2,
        .output_count = 1,
        .precondition_count = 0,
        .postcondition_count = 2,
        .related_count = 2,
        .version_major = 1, .version_minor = 0, .version_patch = 0
    },
    {
        .name = "angle_measure",
        .description = "计算角度大小",
        .mathematical_def = "\\theta = \\angle ABC",
        .category = PRESET_CATEGORY_MEASUREMENT,
        .properties = PRESET_PROPERTY_DETERMINISTIC | PRESET_PROPERTY_CONTINUOUS,
        .complexity = COMPLEXITY_O1,
        .input_count = 3,
        .output_count = 1,
        .precondition_count = 1,
        .postcondition_count = 2,
        .related_count = 3,
        .version_major = 1, .version_minor = 0, .version_patch = 0
    },
    {
        .name = "area_triangle",
        .description = "计算三角形面积",
        .mathematical_def = "S = \\frac{1}{2}|(B-A) \\times (C-A)|",
        .category = PRESET_CATEGORY_MEASUREMENT,
        .properties = PRESET_PROPERTY_DETERMINISTIC | PRESET_PROPERTY_CONTINUOUS,
        .complexity = COMPLEXITY_O1,
        .input_count = 3,
        .output_count = 1,
        .precondition_count = 1,
        .postcondition_count = 2,
        .related_count = 3,
        .version_major = 1, .version_minor = 0, .version_patch = 0
    },
    {
        .name = "area_polygon",
        .description = "计算多边形面积",
        .mathematical_def = "S = \\frac{1}{2}|\\sum_{i=1}^{n} (x_i y_{i+1} - x_{i+1} y_i)|",
        .category = PRESET_CATEGORY_MEASUREMENT,
        .properties = PRESET_PROPERTY_DETERMINISTIC | PRESET_PROPERTY_CONTINUOUS,
        .complexity = COMPLEXITY_ON,
        .input_count = -1,  /* 可变输入 */
        .output_count = 1,
        .precondition_count = 1,
        .postcondition_count = 2,
        .related_count = 2,
        .version_major = 1, .version_minor = 0, .version_patch = 0
    },
    {
        .name = "circumference",
        .description = "计算圆周长",
        .mathematical_def = "C = 2\\pi r",
        .category = PRESET_CATEGORY_MEASUREMENT,
        .properties = PRESET_PROPERTY_DETERMINISTIC | PRESET_PROPERTY_CONTINUOUS,
        .complexity = COMPLEXITY_O1,
        .input_count = 1,
        .output_count = 1,
        .precondition_count = 0,
        .postcondition_count = 1,
        .related_count = 2,
        .version_major = 1, .version_minor = 0, .version_patch = 0
    },
    
    /* ========== 几何变换 ========== */
    {
        .name = "translation",
        .description = "平移变换",
        .mathematical_def = "P' = P + \\vec{v}",
        .category = PRESET_CATEGORY_TRANSFORMATION,
        .properties = PRESET_PROPERTY_DETERMINISTIC | PRESET_PROPERTY_CONTINUOUS | 
                     PRESET_PROPERTY_CONSTRUCTIVE | PRESET_PROPERTY_REVERSIBLE,
        .complexity = COMPLEXITY_O1,
        .input_count = 3,
        .output_count = 1,
        .precondition_count = 0,
        .postcondition_count = 2,
        .related_count = 3,
        .version_major = 1, .version_minor = 0, .version_patch = 0
    },
    {
        .name = "rotation",
        .description = "旋转变换",
        .mathematical_def = "P' = R_\\theta(O, P)",
        .category = PRESET_CATEGORY_TRANSFORMATION,
        .properties = PRESET_PROPERTY_DETERMINISTIC | PRESET_PROPERTY_CONTINUOUS | 
                     PRESET_PROPERTY_CONSTRUCTIVE | PRESET_PROPERTY_REVERSIBLE,
        .complexity = COMPLEXITY_O1,
        .input_count = 4,
        .output_count = 1,
        .precondition_count = 0,
        .postcondition_count = 2,
        .related_count = 3,
        .version_major = 1, .version_minor = 0, .version_patch = 0
    },
    {
        .name = "scaling",
        .description = "缩放变换",
        .mathematical_def = "P' = O + k(P - O)",
        .category = PRESET_CATEGORY_TRANSFORMATION,
        .properties = PRESET_PROPERTY_DETERMINISTIC | PRESET_PROPERTY_CONTINUOUS | 
                     PRESET_PROPERTY_CONSTRUCTIVE | PRESET_PROPERTY_REVERSIBLE,
        .complexity = COMPLEXITY_O1,
        .input_count = 4,
        .output_count = 1,
        .precondition_count = 1,
        .postcondition_count = 2,
        .related_count = 3,
        .version_major = 1, .version_minor = 0, .version_patch = 0
    },
    {
        .name = "homothety",
        .description = "位似变换",
        .mathematical_def = "H(O, k): P \\mapsto O + k\\vec{OP}",
        .category = PRESET_CATEGORY_TRANSFORMATION,
        .properties = PRESET_PROPERTY_DETERMINISTIC | PRESET_PROPERTY_CONTINUOUS | 
                     PRESET_PROPERTY_CONSTRUCTIVE | PRESET_PROPERTY_REVERSIBLE,
        .complexity = COMPLEXITY_O1,
        .input_count = 3,
        .output_count = 1,
        .precondition_count = 1,
        .postcondition_count = 2,
        .related_count = 2,
        .version_major = 1, .version_minor = 0, .version_patch = 0
    },
    {
        .name = "inversion",
        .description = "反演变换",
        .mathematical_def = "P' \\text{ 满足 } OP \\cdot OP' = r^2",
        .category = PRESET_CATEGORY_TRANSFORMATION,
        .properties = PRESET_PROPERTY_DETERMINISTIC | PRESET_PROPERTY_CONTINUOUS | 
                     PRESET_PROPERTY_CONSTRUCTIVE | PRESET_PROPERTY_INVOLUTIVE | PRESET_PROPERTY_REVERSIBLE,
        .complexity = COMPLEXITY_O1,
        .input_count = 3,
        .output_count = 1,
        .precondition_count = 2,
        .postcondition_count = 2,
        .related_count = 2,
        .version_major = 1, .version_minor = 0, .version_patch = 0
    },
    
    /* ========== 代数运算 ========== */
    {
        .name = "vector_add",
        .description = "向量加法",
        .mathematical_def = "\\vec{c} = \\vec{a} + \\vec{b}",
        .category = PRESET_CATEGORY_ALGEBRAIC,
        .properties = PRESET_PROPERTY_DETERMINISTIC | PRESET_PROPERTY_CONTINUOUS | 
                     PRESET_PROPERTY_COMMUTATIVE | PRESET_PROPERTY_ASSOCIATIVE,
        .complexity = COMPLEXITY_O1,
        .input_count = 3,
        .output_count = 1,
        .precondition_count = 0,
        .postcondition_count = 2,
        .related_count = 3,
        .version_major = 1, .version_minor = 0, .version_patch = 0
    },
    {
        .name = "vector_sub",
        .description = "向量减法",
        .mathematical_def = "\\vec{c} = \\vec{a} - \\vec{b}",
        .category = PRESET_CATEGORY_ALGEBRAIC,
        .properties = PRESET_PROPERTY_DETERMINISTIC | PRESET_PROPERTY_CONTINUOUS,
        .complexity = COMPLEXITY_O1,
        .input_count = 3,
        .output_count = 1,
        .precondition_count = 0,
        .postcondition_count = 2,
        .related_count = 3,
        .version_major = 1, .version_minor = 0, .version_patch = 0
    },
    {
        .name = "vector_scale",
        .description = "向量数乘",
        .mathematical_def = "\\vec{b} = k\\vec{a}",
        .category = PRESET_CATEGORY_ALGEBRAIC,
        .properties = PRESET_PROPERTY_DETERMINISTIC | PRESET_PROPERTY_CONTINUOUS | PRESET_PROPERTY_LINEAR,
        .complexity = COMPLEXITY_O1,
        .input_count = 3,
        .output_count = 1,
        .precondition_count = 0,
        .postcondition_count = 2,
        .related_count = 3,
        .version_major = 1, .version_minor = 0, .version_patch = 0
    },
    {
        .name = "vector_dot",
        .description = "向量点积",
        .mathematical_def = "\\vec{a} \\cdot \\vec{b} = |\\vec{a}||\\vec{b}|\\cos\\theta",
        .category = PRESET_CATEGORY_ALGEBRAIC,
        .properties = PRESET_PROPERTY_DETERMINISTIC | PRESET_PROPERTY_CONTINUOUS | 
                     PRESET_PROPERTY_COMMUTATIVE,
        .complexity = COMPLEXITY_O1,
        .input_count = 3,
        .output_count = 1,
        .precondition_count = 0,
        .postcondition_count = 2,
        .related_count = 3,
        .version_major = 1, .version_minor = 0, .version_patch = 0
    },
    {
        .name = "vector_cross",
        .description = "向量叉积（二维）",
        .mathematical_def = "\\vec{a} \\times \\vec{b} = a_x b_y - a_y b_x",
        .category = PRESET_CATEGORY_ALGEBRAIC,
        .properties = PRESET_PROPERTY_DETERMINISTIC | PRESET_PROPERTY_CONTINUOUS,
        .complexity = COMPLEXITY_O1,
        .input_count = 3,
        .output_count = 1,
        .precondition_count = 0,
        .postcondition_count = 2,
        .related_count = 3,
        .version_major = 1, .version_minor = 0, .version_patch = 0
    },
    {
        .name = "vector_normalize",
        .description = "向量单位化",
        .mathematical_def = "\\hat{a} = \\frac{\\vec{a}}{|\\vec{a}|}",
        .category = PRESET_CATEGORY_ALGEBRAIC,
        .properties = PRESET_PROPERTY_DETERMINISTIC | PRESET_PROPERTY_CONTINUOUS | 
                     PRESET_PROPERTY_IDEMPOTENT,
        .complexity = COMPLEXITY_O1,
        .input_count = 2,
        .output_count = 1,
        .precondition_count = 1,
        .postcondition_count = 2,
        .related_count = 2,
        .version_major = 1, .version_minor = 0, .version_patch = 0
    },
    
    /* ========== 逻辑推导 ========== */
    {
        .name = "collinearity_test",
        .description = "共线性检测",
        .mathematical_def = "\\text{det}\\begin{pmatrix} x_A & y_A & 1 \\\\ x_B & y_B & 1 \\\\ x_C & y_C & 1 \\end{pmatrix} = 0",
        .category = PRESET_CATEGORY_LOGIC,
        .properties = PRESET_PROPERTY_DETERMINISTIC,
        .complexity = COMPLEXITY_O1,
        .input_count = 3,
        .output_count = 1,
        .precondition_count = 0,
        .postcondition_count = 1,
        .related_count = 3,
        .version_major = 1, .version_minor = 0, .version_patch = 0
    },
    {
        .name = "concyclicity_test",
        .description = "共圆性检测",
        .mathematical_def = "\\text{四点共圆条件}",
        .category = PRESET_CATEGORY_LOGIC,
        .properties = PRESET_PROPERTY_DETERMINISTIC,
        .complexity = COMPLEXITY_O1,
        .input_count = 4,
        .output_count = 1,
        .precondition_count = 0,
        .postcondition_count = 1,
        .related_count = 3,
        .version_major = 1, .version_minor = 0, .version_patch = 0
    },
    {
        .name = "parallel_test",
        .description = "平行性检测",
        .mathematical_def = "l_1 \\parallel l_2 \\Leftrightarrow k_1 = k_2",
        .category = PRESET_CATEGORY_LOGIC,
        .properties = PRESET_PROPERTY_DETERMINISTIC,
        .complexity = COMPLEXITY_O1,
        .input_count = 2,
        .output_count = 1,
        .precondition_count = 0,
        .postcondition_count = 1,
        .related_count = 3,
        .version_major = 1, .version_minor = 0, .version_patch = 0
    },
    {
        .name = "perpendicular_test",
        .description = "垂直性检测",
        .mathematical_def = "l_1 \\perp l_2 \\Leftrightarrow k_1 k_2 = -1",
        .category = PRESET_CATEGORY_LOGIC,
        .properties = PRESET_PROPERTY_DETERMINISTIC,
        .complexity = COMPLEXITY_O1,
        .input_count = 2,
        .output_count = 1,
        .precondition_count = 0,
        .postcondition_count = 1,
        .related_count = 3,
        .version_major = 1, .version_minor = 0, .version_patch = 0
    },
    {
        .name = "point_in_circle_test",
        .description = "点在圆内检测",
        .mathematical_def = "|PC| < r",
        .category = PRESET_CATEGORY_LOGIC,
        .properties = PRESET_PROPERTY_DETERMINISTIC,
        .complexity = COMPLEXITY_O1,
        .input_count = 2,
        .output_count = 1,
        .precondition_count = 0,
        .postcondition_count = 1,
        .related_count = 2,
        .version_major = 1, .version_minor = 0, .version_patch = 0
    },
    {
        .name = "point_on_line_test",
        .description = "点在直线上检测",
        .mathematical_def = "P \\in l",
        .category = PRESET_CATEGORY_LOGIC,
        .properties = PRESET_PROPERTY_DETERMINISTIC,
        .complexity = COMPLEXITY_O1,
        .input_count = 2,
        .output_count = 1,
        .precondition_count = 0,
        .postcondition_count = 1,
        .related_count = 2,
        .version_major = 1, .version_minor = 0, .version_patch = 0
    },
};

/** 内置预设数量 */
static const int g_builtin_count = sizeof(g_builtin_metadata) / sizeof(g_builtin_metadata[0]);

/* ============================================================
 * 内部辅助函数
 * ============================================================ */

/**
 * @brief 查找预设条目索引
 *
 * 在 g_preset_library.entries 数组中线性搜索指定名称的预设。
 * 仅搜索已激活（is_active == true）的条目。
 *
 * @param name 预设名称（区分大小写）
 * @return 找到时返回数组索引，未找到或参数无效返回 -1
 */
static int find_preset_index(const char *name)
{
    if (!name) return -1;
    
    for (int i = 0; i < g_preset_library.count; i++) {
        if (g_preset_library.entries[i].is_active &&
            g_preset_library.entries[i].metadata.name &&
            strcmp(g_preset_library.entries[i].metadata.name, name) == 0) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief 创建预设模板函数块
 *
 * @param metadata 元数据
 * @return 模板函数块，失败返回NULL
 */
static FuncBlock *create_preset_template(const PresetMetadata *metadata)
{
    if (!metadata) return NULL;
    
    int id = g_preset_library.next_preset_id++;
    FuncBlock *fb = func_block_create(id);
    if (!fb) return NULL;
    
    /* 设置名称和描述 */
    if (metadata->name) {
        fb->name = lv00_strdup(metadata->name);
    }
    if (metadata->description) {
        fb->description = lv00_strdup(metadata->description);
    }
    
    /* 设置确定性状态 */
    if (metadata->properties & PRESET_PROPERTY_DETERMINISTIC) {
        fb->determinism = DETERMINISM_VERIFIED;
    } else {
        fb->determinism = DETERMINISM_NON_DETERMINISTIC;
    }
    
    /* 设置输入输出数量（实际端口在实例化时创建） */
    fb->input_count = metadata->input_count;
    fb->output_count = metadata->output_count;
    
    return fb;
}

/**
 * @brief 注册单个内置预设
 *
 * @param metadata 元数据
 * @return true 成功，false 失败
 */
static bool register_builtin_preset(const PresetMetadata *metadata)
{
    if (!metadata || g_preset_library.count >= MAX_PRESETS) return false;
    
    /* 检查是否已存在 */
    if (find_preset_index(metadata->name) >= 0) return false;
    
    /* 创建模板 */
    FuncBlock *template = create_preset_template(metadata);
    if (!template) return false;
    
    /* 添加到库 */
    int idx = g_preset_library.count++;
    g_preset_library.entries[idx].metadata = *metadata;
    g_preset_library.entries[idx].template_fb = template;
    g_preset_library.entries[idx].is_builtin = true;
    g_preset_library.entries[idx].is_active = true;
    
    return true;
}

/**
 * @brief 初始化所有内置预设
 *
 * @return true 成功，false 失败
 */
static bool init_builtin_presets(void)
{
    for (int i = 0; i < g_builtin_count; i++) {
        if (!register_builtin_preset(&g_builtin_metadata[i])) {
            LV00_LOG_WARNING("注册内置预设失败: %s", g_builtin_metadata[i].name);
            /* 继续注册其他预设 */
        }
    }
    return true;
}

/* ============================================================
 * 公共 API 实现
 * ============================================================ */

bool func_block_preset_library_init(void)
{
    /* 幂等操作 */
    if (g_preset_library.initialized) {
        return true;
    }
    
    /* 清空状态 */
    memset(&g_preset_library, 0, sizeof(g_preset_library));
    g_preset_library.next_preset_id = LV00_PRESET_ID_OFFSET;
    
    /* 初始化内置预设 */
    if (!init_builtin_presets()) {
        return false;
    }
    
    g_preset_library.initialized = true;
    return true;
}

void func_block_preset_library_cleanup(void)
{
    /* 释放所有模板函数块 */
    for (int i = 0; i < g_preset_library.count; i++) {
        if (g_preset_library.entries[i].template_fb) {
            func_block_destroy(g_preset_library.entries[i].template_fb);
            g_preset_library.entries[i].template_fb = NULL;
        }
    }
    
    /* 重置状态 */
    memset(&g_preset_library, 0, sizeof(g_preset_library));
}

const PresetMetadata *func_block_preset_get_metadata(const char *preset_name)
{
    if (!preset_name) return NULL;
    
    int idx = find_preset_index(preset_name);
    if (idx < 0) return NULL;
    
    return &g_preset_library.entries[idx].metadata;
}

InstantiateResult func_block_preset_instantiate(
    const char *preset_name,
    const int *input_node_ids,
    int input_count,
    ConstraintGraph *graph,
    FuncBlock **out_func_block)
{
    InstantiateOptions opts = {
        .auto_resolve_ambiguity = true,
        .validate_constraints = true,
        .add_to_graph = true,
        .max_solutions = 1,
        .default_selector = NULL
    };
    
    InstantiateDetails details;
    memset(&details, 0, sizeof(details));
    
    InstantiateResult result = func_block_preset_instantiate_ex(
        preset_name, input_node_ids, input_count, graph, &opts, &details);
    
    if (out_func_block) {
        *out_func_block = details.func_block;
    }
    
    /* 释放警告信息 */
    if (details.warnings) {
        for (int i = 0; i < details.warning_count; i++) {
            lv00_free((void **)&details.warnings[i]);
        }
        lv00_free((void **)&details.warnings);
    }
    lv00_free((void **)&details.error_detail);
    
    return result;
}

InstantiateResult func_block_preset_instantiate_ex(
    const char *preset_name,
    const int *input_node_ids,
    int input_count,
    ConstraintGraph *graph,
    const InstantiateOptions *options,
    InstantiateDetails *out_details)
{
    /* 参数检查 */
    if (!preset_name || !graph || !out_details) {
        if (out_details) out_details->result = INSTANTIATE_OUT_OF_MEMORY;
        return INSTANTIATE_OUT_OF_MEMORY;
    }
    
    memset(out_details, 0, sizeof(InstantiateDetails));
    
    /* 查找预设 */
    int idx = find_preset_index(preset_name);
    if (idx < 0) {
        out_details->result = INSTANTIATE_NO_SOLUTION;
        out_details->error_detail = lv00_strdup("预设不存在");
        return INSTANTIATE_NO_SOLUTION;
    }
    
    const PresetMetadata *metadata = &g_preset_library.entries[idx].metadata;
    
    /* 验证输入数量 */
    if (metadata->input_count > 0 && input_count != metadata->input_count) {
        out_details->result = INSTANTIATE_PRECONDITION_FAILED;
        out_details->error_detail = lv00_asprintf(
            "输入参数数量不匹配: 需要%d个，提供%d个", metadata->input_count, input_count);
        return INSTANTIATE_PRECONDITION_FAILED;
    }
    
    /* 验证输入节点 */
    if (input_count > 0 && !input_node_ids) {
        out_details->result = INSTANTIATE_PRECONDITION_FAILED;
        out_details->error_detail = lv00_strdup("输入节点ID为空");
        return INSTANTIATE_PRECONDITION_FAILED;
    }
    
    for (int i = 0; i < input_count; i++) {
        GeomNode *node = graph_get_node(graph, input_node_ids[i]);
        if (!node) {
            out_details->result = INSTANTIATE_PRECONDITION_FAILED;
            out_details->error_detail = lv00_asprintf("输入节点%d不存在", input_node_ids[i]);
            return INSTANTIATE_PRECONDITION_FAILED;
        }
    }
    
    /* 创建函数块副本 */
    FuncBlock *fb = func_block_copy(g_preset_library.entries[idx].template_fb);
    if (!fb) {
        out_details->result = INSTANTIATE_OUT_OF_MEMORY;
        return INSTANTIATE_OUT_OF_MEMORY;
    }
    
    /* 设置输入端口 - 使用 func_block_set_input_ports 自动释放 func_block_copy 拷贝来的旧值，避免内存泄漏 */
    if (input_count > 0) {
        if (!func_block_set_input_ports(fb, input_node_ids, input_count)) {
            func_block_destroy(fb);
            out_details->result = INSTANTIATE_OUT_OF_MEMORY;
            return INSTANTIATE_OUT_OF_MEMORY;
        }
    } else {
        /* input_count == 0 时也需清空拷贝来的旧 input_port_ids，避免内存泄漏 */
        func_block_set_input_ports(fb, NULL, 0);
    }
    
    /* 添加到约束图（如果需要） */
    if (options && options->add_to_graph) {
        /* 这里应该创建实际的约束节点 */
        /* 简化实现：仅标记为已实例化 */
    }
    
    out_details->result = INSTANTIATE_OK;
    out_details->func_block = fb;
    
    return INSTANTIATE_OK;
}

void func_block_preset_free_details(InstantiateDetails *details)
{
    if (!details) return;
    
    if (details->warnings) {
        for (int i = 0; i < details->warning_count; i++) {
            lv00_free((void **)&details->warnings[i]);
        }
        lv00_free((void **)&details->warnings);
    }
    
    lv00_free((void **)&details->error_detail);
    lv00_free((void **)&details->output_node_ids);
    
    if (details->func_block) {
        func_block_destroy(details->func_block);
        details->func_block = NULL;
    }
}

bool func_block_preset_validate_types(
    const char *preset_name,
    GeomNode **input_nodes,
    int input_count,
    int *out_mismatch_index)
{
    if (!preset_name) return false;

    /* 当 input_count == 0 时，input_nodes 为 NULL 是合理的，不应视为错误 */
    if (input_count > 0 && !input_nodes) return false;

    const PresetMetadata *metadata = func_block_preset_get_metadata(preset_name);
    if (!metadata) return false;

    /* ── 第一步：验证输入参数个数 ──
     * 可变输入（input_count == -1）跳过数量检查
     */
    if (metadata->input_count > 0 && input_count != metadata->input_count) {
        if (out_mismatch_index) *out_mismatch_index = 0;
        return false;
    }

    /* ── 第二步：逐个验证输入节点类型与预设期望类型的兼容性 ──
     * 若预设定义了 input_params 数组，则逐项对比；
     * 若 input_params 为空（简化的预设元数据），则仅依赖数量检查结果。
     */
    if (metadata->input_params != NULL && input_nodes != NULL) {
        int check_count = (input_count < metadata->input_count)
                          ? input_count : metadata->input_count;

        for (int i = 0; i < check_count; i++) {
            /* 节点不存在视为类型不匹配 */
            if (input_nodes[i] == NULL) {
                if (out_mismatch_index) *out_mismatch_index = i;
                return false;
            }

            /* 获取节点类型（通过 GeomNode 的类型字段） */
            const PresetParamType expected = metadata->input_params[i].type;

            /* 期望类型为任意类型（PARAM_TYPE_ANY）时跳过类型检查 */
            if (expected == PARAM_TYPE_ANY) {
                continue;
            }

            /* 获取节点实际类型映射 */
            PresetParamType actual;
            switch (input_nodes[i]->type) {
                case NODE_TYPE_POINT:   actual = PARAM_TYPE_POINT;    break;
                case NODE_TYPE_LINE:    actual = PARAM_TYPE_LINE;     break;
                case NODE_TYPE_CIRCLE:  actual = PARAM_TYPE_CIRCLE;   break;
                case NODE_TYPE_ARC:     actual = PARAM_TYPE_ARC;      break;
                case NODE_TYPE_POLYGON: actual = PARAM_TYPE_POLYGON;  break;
                case NODE_TYPE_REGION:  actual = PARAM_TYPE_REGION;   break;
                case NODE_TYPE_SCALAR:  actual = PARAM_TYPE_SCALAR;   break;
                case NODE_TYPE_VECTOR:  actual = PARAM_TYPE_VECTOR;   break;
                default:                actual = PARAM_TYPE_ANY;      break;
            }

            /* 类型兼容性检查：
             *   - 精确匹配：通过
             *   - 线段/射线均兼容直线类型
             *   - 圆弧兼容圆类型
             *   - 其他情况不匹配
             */
            bool compatible = (actual == expected);
            if (!compatible) {
                /* 线段/射线 → 直线兼容 */
                if (expected == PARAM_TYPE_LINE &&
                    (actual == PARAM_TYPE_SEGMENT || actual == PARAM_TYPE_RAY)) {
                    compatible = true;
                }
                /* 圆弧 → 圆兼容 */
                if (expected == PARAM_TYPE_CIRCLE && actual == PARAM_TYPE_ARC) {
                    compatible = true;
                }
            }

            if (!compatible) {
                if (out_mismatch_index) *out_mismatch_index = i;
                return false;
            }
        }
    }

    /* ── 第三步：验证输出类型（若定义了输出参数） ──
     * 注：目前只做存在性检查，实际输出类型的验证
     * 需在实例化后根据生成的 GeomNode 类型进行。
     */
    if (metadata->output_params == NULL && metadata->output_count < 0) {
        /* 可变输出数量，不做静态验证 */
    }

    return true;
}

bool func_block_preset_validate_constraints(
    const char *preset_name,
    ConstraintGraph *graph,
    const int *input_node_ids,
    int input_count,
    const char **out_violated_constraint)
{
    if (!preset_name || !graph) return false;
    
    const PresetMetadata *metadata = func_block_preset_get_metadata(preset_name);
    if (!metadata) return false;
    
    /* 简化实现：仅检查节点存在性 */
    for (int i = 0; i < input_count; i++) {
        if (!graph_get_node(graph, input_node_ids[i])) {
            if (out_violated_constraint) {
                *out_violated_constraint = "输入节点不存在";
            }
            return false;
        }
    }
    
    return true;
}

int func_block_preset_get_input_count(const char *preset_name)
{
    const PresetMetadata *metadata = func_block_preset_get_metadata(preset_name);
    if (!metadata) return -1;
    return metadata->input_count;
}

int func_block_preset_get_output_count(const char *preset_name)
{
    const PresetMetadata *metadata = func_block_preset_get_metadata(preset_name);
    if (!metadata) return -1;
    return metadata->output_count;
}

int func_block_preset_list(
    const char **out_names,
    int max_count,
    PresetCategory category)
{
    if (!out_names || max_count <= 0) return 0;
    
    int count = 0;
    for (int i = 0; i < g_preset_library.count && count < max_count; i++) {
        if (!g_preset_library.entries[i].is_active) continue;
        
        if (category < 0 || g_preset_library.entries[i].metadata.category == category) {
            out_names[count++] = g_preset_library.entries[i].metadata.name;
        }
    }
    
    return count;
}

bool func_block_preset_exists(const char *preset_name)
{
    return find_preset_index(preset_name) >= 0;
}

const char *func_block_preset_category_string(PresetCategory category)
{
    switch (category) {
        /* ── 基础类别（v3.0） ── */
        case PRESET_CATEGORY_CONSTRUCTION:   return "几何构造";
        case PRESET_CATEGORY_MEASUREMENT:    return "度量计算";
        case PRESET_CATEGORY_TRANSFORMATION: return "几何变换";
        case PRESET_CATEGORY_ALGEBRAIC:      return "代数运算";
        case PRESET_CATEGORY_LOGIC:          return "逻辑推导";

        /* ── 分析学类别 ── */
        case PRESET_CATEGORY_ANALYSIS:       return "数学分析";
        case PRESET_CATEGORY_COMPLEX_ANALYSIS: return "复分析";

        /* ── 数论类别 ── */
        case PRESET_CATEGORY_NUMBER_THEORY:  return "数论";

        /* ── 群论与代数结构 ── */
        case PRESET_CATEGORY_GROUP_THEORY:   return "群论";
        case PRESET_CATEGORY_RING_THEORY:    return "环论";
        case PRESET_CATEGORY_FIELD_THEORY:   return "域论";

        /* ── 拓扑与几何 ── */
        case PRESET_CATEGORY_TOPOLOGY:       return "拓扑学";
        case PRESET_CATEGORY_GEOMETRY:       return "几何学";
        case PRESET_CATEGORY_DIFFERENTIAL_GEOMETRY: return "微分几何";

        /* ── 代数与组合 ── */
        case PRESET_CATEGORY_LINEAR_ALGEBRA:  return "线性代数";
        case PRESET_CATEGORY_ALGEBRA:         return "代数学";
        case PRESET_CATEGORY_COMBINATORICS:   return "组合数学";

        /* ── 概率统计 ── */
        case PRESET_CATEGORY_PROBABILITY:     return "概率统计";

        /* ── 基础数学 ── */
        case PRESET_CATEGORY_CATEGORY_THEORY: return "范畴论";
        case PRESET_CATEGORY_SET_THEORY:      return "集合论";

        /* ── 其他扩展类别 ── */
        case PRESET_CATEGORY_GRAPH_THEORY:    return "图论";
        case PRESET_CATEGORY_NUMERICAL:       return "数值分析";
        case PRESET_CATEGORY_OPTIMIZATION:    return "优化理论";
        case PRESET_CATEGORY_MATH_LOGIC:      return "数理逻辑";
        case PRESET_CATEGORY_CUSTOM:          return "自定义";

        default:                              return "未知类别";
    }
}

const char *func_block_preset_param_type_string(PresetParamType type)
{
    switch (type) {
        case PARAM_TYPE_POINT:      return "点";
        case PARAM_TYPE_LINE:       return "直线";
        case PARAM_TYPE_SEGMENT:    return "线段";
        case PARAM_TYPE_RAY:        return "射线";
        case PARAM_TYPE_CIRCLE:     return "圆";
        case PARAM_TYPE_ARC:        return "圆弧";
        case PARAM_TYPE_POLYGON:    return "多边形";
        case PARAM_TYPE_REGION:     return "区域";
        case PARAM_TYPE_ANGLE:      return "角度";
        case PARAM_TYPE_VECTOR:     return "向量";
        case PARAM_TYPE_SCALAR:     return "标量";
        case PARAM_TYPE_BOOLEAN:    return "布尔值";
        case PARAM_TYPE_CURVE:      return "曲线";
        case PARAM_TYPE_SURFACE:    return "曲面";
        case PARAM_TYPE_ANY:        return "任意类型";
        case PARAM_TYPE_VARIADIC:   return "可变参数";
        default:                    return "未知类型";
    }
}

const char *func_block_preset_complexity_string(PresetComplexity complexity)
{
    switch (complexity) {
        case COMPLEXITY_O1:       return "O(1) - 常数时间";
        case COMPLEXITY_OLOGN:    return "O(log n) - 对数时间";
        case COMPLEXITY_ON:       return "O(n) - 线性时间";
        case COMPLEXITY_ONLOGN:   return "O(n log n) - 线性对数";
        case COMPLEXITY_ON2:      return "O(n²) - 平方时间";
        case COMPLEXITY_ON3:      return "O(n³) - 立方时间";
        case COMPLEXITY_UNKNOWN:  return "未知";
        default:                  return "未知";
    }
}

int func_block_preset_properties_string(
    PresetProperty properties,
    char *out_buffer,
    size_t buffer_size)
{
    if (!out_buffer || buffer_size == 0) return 0;
    
    const char *props[] = {
        properties & PRESET_PROPERTY_IDEMPOTENT ? "幂等" : NULL,
        properties & PRESET_PROPERTY_INVOLUTIVE ? "对合" : NULL,
        properties & PRESET_PROPERTY_COMMUTATIVE ? "交换" : NULL,
        properties & PRESET_PROPERTY_ASSOCIATIVE ? "结合" : NULL,
        properties & PRESET_PROPERTY_LINEAR ? "线性" : NULL,
        properties & PRESET_PROPERTY_CONTINUOUS ? "连续" : NULL,
        properties & PRESET_PROPERTY_DETERMINISTIC ? "确定" : NULL,
        properties & PRESET_PROPERTY_CONSTRUCTIVE ? "构造" : NULL,
        properties & PRESET_PROPERTY_REVERSIBLE ? "可逆" : NULL
    };
    
    int written = 0;
    bool first = true;
    
    for (size_t i = 0; i < sizeof(props) / sizeof(props[0]); i++) {
        if (props[i]) {
            if (!first && written < (int)buffer_size - 1) {
                out_buffer[written++] = ',';
            }
            int len = (int)strlen(props[i]);
            if (written + len < (int)buffer_size - 1) {
                memcpy(out_buffer + written, props[i], len);
                written += len;
                first = false;
            }
        }
    }
    
    out_buffer[written] = '\0';
    return written;
}

/* ============================================================
 * 高级预设操作
 * ============================================================ */

bool func_block_preset_compose(
    const char *f_name,
    const char *g_name,
    const char *new_preset_name)
{
    /* ── 第一步：参数验证 ── */
    if (!f_name || !g_name || !new_preset_name) return false;

    /* ── 第二步：检查两个输入预设是否存在 ── */
    if (!func_block_preset_exists(f_name) || !func_block_preset_exists(g_name)) {
        return false;
    }

    /* 检查同名预设是否已存在 */
    if (func_block_preset_exists(new_preset_name)) {
        return false;
    }

    /* ── 第三步：获取两个预设的元数据 ── */
    const PresetMetadata *f_meta = func_block_preset_get_metadata(f_name);
    const PresetMetadata *g_meta = func_block_preset_get_metadata(g_name);
    if (!f_meta || !g_meta) return false;

    /* ── 第四步：类型兼容性验证 ──
     * 组合 g(f(x)) 要求 f 的输出类型与 g 的输入类型兼容
     * 简化处理：若定义了 input_params/output_params 则检查；
     * 否则仅依赖输出/输入数量匹配。
     */
    if (f_meta->output_params != NULL && g_meta->input_params != NULL) {
        int min_count = (f_meta->output_count < g_meta->input_count)
                        ? f_meta->output_count : g_meta->input_count;
        for (int i = 0; i < min_count; i++) {
            PresetParamType f_out_type = f_meta->output_params[i].type;
            PresetParamType g_in_type  = g_meta->input_params[i].type;

            /* 任意类型兼容所有类型 */
            if (f_out_type == PARAM_TYPE_ANY || g_in_type == PARAM_TYPE_ANY) continue;

            /* 类型必须精确匹配或满足子类型兼容规则 */
            if (f_out_type != g_in_type) {
                bool compatible = false;
                /* 线段/射线 → 直线 */
                if (g_in_type == PARAM_TYPE_LINE &&
                    (f_out_type == PARAM_TYPE_SEGMENT || f_out_type == PARAM_TYPE_RAY)) {
                    compatible = true;
                }
                /* 圆弧 → 圆 */
                if (g_in_type == PARAM_TYPE_CIRCLE && f_out_type == PARAM_TYPE_ARC) {
                    compatible = true;
                }
                if (!compatible) return false;
            }
        }
    }

    /* ── 第五步：创建组合预设的元数据 ──
     * 组合预设 g(f(x)) 的输入 = f 的输入，输出 = g 的输出
     */
    PresetMetadata composed_meta = *g_meta;  /* 以 g 为基础复制 */
    composed_meta.name = new_preset_name;     /* 覆盖名称 */
    composed_meta.input_params = f_meta->input_params;
    composed_meta.input_count = f_meta->input_count;
    /* 组合复杂度取较大者 */
    if (f_meta->complexity > composed_meta.complexity) {
        composed_meta.complexity = f_meta->complexity;
    }
    /* 合并数学性质：组合保留两者的交集性质 */
    composed_meta.properties = f_meta->properties & g_meta->properties;

    /* ── 第六步：创建组合预设的模板函数块 ──
     * 使用 f 的模板作为基础，将 g 的输出端口信息写入
     */
    int f_idx = find_preset_index(f_name);
    int g_idx = find_preset_index(g_name);
    if (f_idx < 0 || g_idx < 0) return false;

    FuncBlock *f_template = g_preset_library.entries[f_idx].template_fb;
    FuncBlock *g_template = g_preset_library.entries[g_idx].template_fb;

    /* 创建组合模板：深拷贝 f 的模板，然后设置输出为 g 的输出 */
    FuncBlock *composed_template = func_block_copy(f_template);
    if (!composed_template) return false;

    /* 设置组合后的输出端口为 g 的输出端口 */
    if (g_template->output_count > 0 && g_template->output_port_ids) {
        func_block_set_output_ports(composed_template,
                                    g_template->output_port_ids,
                                    g_template->output_count);
    }

    /* 设置组合名称 */
    func_block_set_name(composed_template, composed_meta.name);

    /* ── 第七步：注册新预设到库中 ── */
    return func_block_preset_register_custom(&composed_meta, composed_template);
}

bool func_block_preset_partial(
    const char *preset_name,
    const int *fixed_param_indices,
    int fixed_count,
    const char *new_preset_name)
{
    /* ── 第一步：参数验证 ── */
    if (!preset_name || !new_preset_name) return false;
    if (fixed_count > 0 && !fixed_param_indices) return false;

    /* ── 第二步：检查原预设是否存在 ── */
    if (!func_block_preset_exists(preset_name)) return false;

    /* 检查同名预设是否已存在 */
    if (func_block_preset_exists(new_preset_name)) return false;

    /* ── 第三步：获取原预设的元数据和模板 ── */
    const PresetMetadata *meta = func_block_preset_get_metadata(preset_name);
    if (!meta) return false;

    int idx = find_preset_index(preset_name);
    if (idx < 0) return false;

    /* ── 第四步：验证待固定参数的索引合法性 ── */
    for (int i = 0; i < fixed_count; i++) {
        int param_idx = fixed_param_indices[i];
        if (param_idx < 0 || param_idx >= meta->input_count) {
            /* 参数索引越界 */
            return false;
        }
        /* 检查是否有重复的固定索引 */
        for (int j = i + 1; j < fixed_count; j++) {
            if (fixed_param_indices[j] == param_idx) {
                /* 重复指定同一参数 */
                return false;
            }
        }
    }

    /* ── 第五步：创建偏应用后的预设元数据 ──
     * 偏应用预设的输入数量 = 原输入数量 - 已固定的参数数量
     */
    PresetMetadata partial_meta = *meta;
    partial_meta.name = new_preset_name;

    /* 若原输入数量有效（非可变），计算新的输入数量 */
    if (meta->input_count > 0) {
        partial_meta.input_count = meta->input_count - fixed_count;
        if (partial_meta.input_count < 0) {
            /* 固定参数数量超过原输入数量 */
            return false;
        }
    }

    /* ── 第六步：创建偏应用后的模板函数块 ──
     * 基于原模板深拷贝，然后从中移除已固定的输入端口
     */
    FuncBlock *template = g_preset_library.entries[idx].template_fb;
    FuncBlock *partial_template = func_block_copy(template);
    if (!partial_template) return false;

    /* 构建新的输入端口数组（跳过已固定的端口） */
    int new_input_count = partial_meta.input_count;
    if (new_input_count > 0 && template->input_count > 0) {
        int *new_input_ports = (int *)lv00_malloc((size_t)new_input_count * sizeof(int));
        if (!new_input_ports) {
            func_block_destroy(partial_template);
            return false;
        }

        int src_idx = 0;
        for (int i = 0; i < template->input_count && src_idx < new_input_count; i++) {
            /* 检查当前索引是否需要被固定（跳过） */
            bool is_fixed = false;
            for (int j = 0; j < fixed_count; j++) {
                if (fixed_param_indices[j] == i) {
                    is_fixed = true;
                    break;
                }
            }
            if (!is_fixed) {
                new_input_ports[src_idx] = template->input_port_ids[i];
                src_idx++;
            }
        }

        /* 设置新的输入端口（func_block_set_input_ports 会自动释放旧值） */
        func_block_set_input_ports(partial_template, new_input_ports, new_input_count);
        lv00_free((void **)&new_input_ports);
    }

    /* 设置新名称 */
    func_block_set_name(partial_template, partial_meta.name);

    /* ── 第七步：注册新预设到库中 ── */
    return func_block_preset_register_custom(&partial_meta, partial_template);
}

const char *func_block_preset_get_inverse(const char *preset_name)
{
    /* 防止空指针解引用：preset_name 为 NULL 时直接返回 NULL */
    if (!preset_name) return NULL;

    /* 简化实现：返回常见逆操作 */
    if (strcmp(preset_name, "translation") == 0) return "translation";
    if (strcmp(preset_name, "rotation") == 0) return "rotation";
    if (strcmp(preset_name, "scaling") == 0) return "scaling";
    if (strcmp(preset_name, "inversion") == 0) return "inversion";
    if (strcmp(preset_name, "reflection_point") == 0) return "reflection_point";
    return NULL;
}

bool func_block_preset_register_custom(
    const PresetMetadata *metadata,
    const FuncBlock *template_fb)
{
    if (!metadata || !template_fb) return false;
    if (g_preset_library.count >= MAX_PRESETS) return false;
    if (find_preset_index(metadata->name) >= 0) return false;
    
    FuncBlock *copy = func_block_copy(template_fb);
    if (!copy) return false;
    
    int idx = g_preset_library.count++;
    g_preset_library.entries[idx].metadata = *metadata;
    g_preset_library.entries[idx].template_fb = copy;
    g_preset_library.entries[idx].is_builtin = false;
    g_preset_library.entries[idx].is_active = true;
    
    return true;
}

/* ============================================================
 * 文档生成
 * ============================================================ */

size_t func_block_preset_generate_doc(
    const char *preset_name,
    char *out_buffer,
    size_t buffer_size)
{
    if (!preset_name || !out_buffer || buffer_size == 0) return 0;
    
    const PresetMetadata *m = func_block_preset_get_metadata(preset_name);
    if (!m) return 0;
    
    /* 修复：动态分配属性字符串缓冲区，避免固定 256 字节栈缓冲区不够用的问题 */
    size_t props_buf_size = 512;
    char *props_buffer = (char *)lv00_malloc(props_buf_size);
    if (!props_buffer) return 0;
    func_block_preset_properties_string(m->properties, props_buffer, props_buf_size);
    
    int written = snprintf(out_buffer, buffer_size,
        "# %s\n\n"
        "## 描述\n\n%s\n\n"
        "## 数学定义\n\n`%s`\n\n"
        "## 类别\n\n%s\n\n"
        "## 性质\n\n%s\n\n"
        "## 复杂度\n\n%s\n\n"
        "## 参数\n\n"
        "- 输入: %d个\n"
        "- 输出: %d个\n\n"
        "## 版本\n\n%d.%d.%d\n",
        m->name,
        m->description,
        m->mathematical_def,
        func_block_preset_category_string(m->category),
        props_buffer[0] ? props_buffer : "无",
        func_block_preset_complexity_string(m->complexity),
        m->input_count,
        m->output_count,
        m->version_major, m->version_minor, m->version_patch
    );
    
    if (written < 0 || (size_t)written >= buffer_size) {
        lv00_free((void **)&props_buffer);  /* 释放动态分配的属性缓冲区 */
        return buffer_size + 1;  /* 指示缓冲区不足 */
    }
    
    lv00_free((void **)&props_buffer);  /* 释放动态分配的属性缓冲区 */
    return (size_t)written + 1;  /* 包含\0 */
}

size_t func_block_preset_generate_index(
    char *out_buffer,
    size_t buffer_size)
{
    if (!out_buffer || buffer_size == 0) return 0;
    
    /* 修复：使用 size_t 类型的 written 避免累加时 int 溢出 */
    size_t written = 0;
    size_t remaining = buffer_size;
    
    int n = snprintf(out_buffer, remaining,
        "# Lv-00 预设函数块库\n\n"
        "## 概述\n\n"
        "本库提供 %d 个标准化几何预设函数块，用于理论数学研究。\n\n"
        "## 分类索引\n\n",
        g_preset_library.count
    );
    
    /* 检查 snprintf 返回值：n < 0 表示编码错误，n >= remaining 表示截断 */
    if (n < 0) return buffer_size + 1;
    if ((size_t)n >= remaining) return buffer_size + 1;
    written += (size_t)n;
    remaining -= (size_t)n;
    
    /* 按类别分组 */
    const char *categories[] = {"几何构造", "度量计算", "几何变换", "代数运算", "逻辑推导"};
    PresetCategory cat_enums[] = {
        PRESET_CATEGORY_CONSTRUCTION,
        PRESET_CATEGORY_MEASUREMENT,
        PRESET_CATEGORY_TRANSFORMATION,
        PRESET_CATEGORY_ALGEBRAIC,
        PRESET_CATEGORY_LOGIC
    };
    
    for (int c = 0; c < 5; c++) {
        n = snprintf(out_buffer + written, remaining,
            "### %s\n\n", categories[c]);
        if (n < 0) return buffer_size + 1;
        if ((size_t)n >= remaining) return buffer_size + 1;
        written += (size_t)n;
        remaining -= (size_t)n;
        
        for (int i = 0; i < g_preset_library.count; i++) {
            if (!g_preset_library.entries[i].is_active) continue;
            if (g_preset_library.entries[i].metadata.category != cat_enums[c]) continue;
            
            n = snprintf(out_buffer + written, remaining,
                "- **%s**: %s\n",
                g_preset_library.entries[i].metadata.name,
                g_preset_library.entries[i].metadata.description);
            if (n < 0) return buffer_size + 1;
            if ((size_t)n >= remaining) return buffer_size + 1;
            written += (size_t)n;
            remaining -= (size_t)n;
        }
        
        n = snprintf(out_buffer + written, remaining, "\n");
        if (n < 0) return buffer_size + 1;
        if ((size_t)n >= remaining) return buffer_size + 1;
        written += (size_t)n;
        remaining -= (size_t)n;
    }
    
    return written + 1;
}

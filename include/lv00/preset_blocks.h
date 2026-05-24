/**
 * @file preset_blocks.h
 * @brief 预设函数块扩展系统 - 为理论数学研究提供丰富的预设函数块
 *
 * 本模块扩展了基础的函数块注册系统，提供：
 * - 更多几何构造预设（正多边形、切线、外接圆等）
 * - 代数运算预设（多项式求根、矩阵运算）
 * - 逻辑推导预设（蕴含、等价、量词）
 * - 高级几何变换（仿射变换、投影变换）
 * - 数学分析预设（极限、导数、积分近似）
 *
 * 设计原则：
 * - 所有预设函数块都是只读模板，实例化时自动创建副本
 * - 预设按数学领域分类，便于查找和使用
 * - 支持用户自定义预设的注册和管理
 * - 每个预设都有完整的中文描述和数学定义
 */

#ifndef LV00_PRESET_BLOCKS_H
#define LV00_PRESET_BLOCKS_H

#include "func_block.h"
#include "func_block_registry.h"
#include "func_block_preset.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 预设函数块类型系统（v5.0）
 * ================================================================ */

/**
 * @brief 预设函数块的输入/输出类型枚举
 *
 * 定义预设函数块的参数类型，用于类型检查和验证。
 * 与 PresetParamType 不同，这是简化的类型系统，
 * 用于统一的注册接口 preset_blocks_register_simple。
 */
typedef enum {
    PRESET_TYPE_POINT = 0,          /**< 点 */
    PRESET_TYPE_LINE,               /**< 直线（无限延伸） */
    PRESET_TYPE_LINE_SEGMENT,       /**< 线段 */
    PRESET_TYPE_RAY,                /**< 射线 */
    PRESET_TYPE_CIRCLE,             /**< 圆 */
    PRESET_TYPE_POLYGON,            /**< 多边形 */
    PRESET_TYPE_ANGLE,              /**< 角度 */
    PRESET_TYPE_SCALAR,             /**< 标量（数值） */
    PRESET_TYPE_VECTOR,             /**< 向量 */
    PRESET_TYPE_MATRIX,             /**< 矩阵 */
    PRESET_TYPE_BOOLEAN,            /**< 布尔值 */
    PRESET_TYPE_INTEGER,            /**< 整数 */
    PRESET_TYPE_SET,                /**< 集合 */
    PRESET_TYPE_FUNCTION,           /**< 函数 */
    PRESET_TYPE_TUPLE,              /**< 元组 */
    PRESET_TYPE_LIST,               /**< 列表 */
    PRESET_TYPE_SEQUENCE,           /**< 序列 */
    PRESET_TYPE_REGION,             /**< 区域 */
    PRESET_TYPE_PATH,               /**< 路径 */
    PRESET_TYPE_SURFACE,            /**< 曲面 */
    PRESET_TYPE_SPACE,              /**< 空间 */
    PRESET_TYPE_GROUP,              /**< 群 */
    PRESET_TYPE_GROUP_ELEMENT,      /**< 群元素 */
    PRESET_TYPE_SUBGROUP,           /**< 子群 */
    PRESET_TYPE_HOMOMORPHISM,       /**< 同态映射 */
    PRESET_TYPE_PRIME,              /**< 素数 */
    PRESET_TYPE_EQUATION,           /**< 方程 */
    PRESET_TYPE_LIMIT,              /**< 极限 */
    PRESET_TYPE_DERIVATIVE,         /**< 导数 */
    PRESET_TYPE_POLYNOMIAL,         /**< 多项式 */
    PRESET_TYPE_LIMIT_EXPRESSION,   /**< 极限表达式 */
    /* 代数结构类型 */
    PRESET_TYPE_RING,               /**< 环 */
    PRESET_TYPE_IDEAL,              /**< 理想 */
    PRESET_TYPE_FIELD,              /**< 域 */
    PRESET_TYPE_MODULE,             /**< 模 */
    PRESET_TYPE_ALGEBRA,            /**< 代数 */
    /* 拓扑与分析类型 */
    PRESET_TYPE_TOPOLOGY,           /**< 拓扑空间 */
    PRESET_TYPE_MANIFOLD,           /**< 流形 */
    PRESET_TYPE_DISTRIBUTION,       /**< 概率分布 */
    PRESET_TYPE_PROBABILITY,        /**< 概率值 */
    PRESET_TYPE_GRAPH,              /**< 图 */
    PRESET_TYPE_TREE,               /**< 树 */
    PRESET_TYPE_INTEGRAL,           /**< 积分 */
    PRESET_TYPE_SERIES,             /**< 级数 */
    PRESET_TYPE_COMPLEX,            /**< 复数 */
    PRESET_TYPE_PERMUTATION,        /**< 置换 */
    PRESET_TYPE_COSET,              /**< 陪集 */
    PRESET_TYPE_EXTENSION,          /**< 域扩张 */
    PRESET_TYPE_AUTOMORPHISM,       /**< 自同构群 */
    /* 度量类型 */
    PRESET_TYPE_DISTANCE,           /**< 距离 */
    PRESET_TYPE_AREA,               /**< 面积 */
    PRESET_TYPE_LENGTH,             /**< 长度 */
    PRESET_TYPE_CURVATURE,          /**< 曲率 */
    /* 拓扑类型 */
    PRESET_TYPE_OPEN_SET,           /**< 开集 */
    PRESET_TYPE_CLOSED_SET,         /**< 闭集 */
    /* 数论类型 */
    PRESET_TYPE_RESIDUE,            /**< 剩余类 */
    /* 逻辑与结构类型 */
    PRESET_TYPE_FORMULA,            /**< 公式 */
    PRESET_TYPE_EXPRESSION,         /**< 表达式（逻辑/集合论中的表达式） */
    PRESET_TYPE_STRUCTURE,          /**< 结构 */
    PRESET_TYPE_STRING,             /**< 字符串 */
    /* 通用类型 */
    PRESET_TYPE_ANY,                /**< 任意类型（多态） */
    PRESET_TYPE_COUNT               /**< 类型总数（哨兵值） */
} PresetType;

/* ================================================================
 * 预设函数块类别扩展
 * ================================================================ */

/**
 * @brief 扩展的预设函数块分类枚举
 *
 * 在基础分类之上增加了更多数学领域的分类，
 * 用于对内置预设函数块进行更细粒度的管理。
 */
typedef enum {
    /* 基础几何构造 */
    PRESET_EXT_BASIC_CONSTRUCTION = 0,    /* 基本几何构造 */
    PRESET_EXT_ADVANCED_CONSTRUCTION,     /* 高级几何构造 */
    PRESET_EXT_POLYGON,                   /* 多边形相关 */
    PRESET_EXT_CIRCLE,                    /* 圆相关构造 */
    
    /* 几何变换 */
    PRESET_EXT_TRANSFORMATION_BASIC,      /* 基本变换 */
    PRESET_EXT_TRANSFORMATION_ADVANCED,   /* 高级变换 */
    
    /* 度量与计算 */
    PRESET_EXT_MEASUREMENT,               /* 度量计算 */
    PRESET_EXT_TRIGONOMETRY,              /* 三角函数 */
    PRESET_EXT_COORDINATE,                /* 坐标运算 */
    
    /* 代数运算 */
    PRESET_EXT_ALGEBRA_BASIC,             /* 基础代数 */
    PRESET_EXT_ALGEBRA_ADVANCED,          /* 高级代数 */
    PRESET_EXT_LINEAR_ALGEBRA,            /* 线性代数 */
    PRESET_EXT_POLYNOMIAL,                /* 多项式运算 */
    
    /* 逻辑与证明 */
    PRESET_EXT_LOGIC_PROPOSITIONAL,       /* 命题逻辑 */
    PRESET_EXT_LOGIC_PREDICATE,           /* 谓词逻辑 */
    PRESET_EXT_PROOF_TACTICS,             /* 证明策略 */
    
    /* 数学分析 */
    PRESET_EXT_ANALYSIS_LIMIT,            /* 极限相关 */
    PRESET_EXT_ANALYSIS_DIFFERENTIAL,     /* 微分相关 */
    PRESET_EXT_ANALYSIS_INTEGRAL,         /* 积分相关 */
    
    /* 拓扑与几何 */
    PRESET_EXT_TOPOLOGY,                  /* 拓扑构造 */
    PRESET_EXT_DIFFERENTIAL_GEOMETRY,     /* 微分几何 */
    
    /* 数论 */
    PRESET_EXT_NUMBER_THEORY,             /* 数论运算 */
    
    /* 群论 */
    PRESET_EXT_GROUP_THEORY,              /* 群论运算 */

    /* 分析学 */
    PRESET_EXT_ANALYSIS,                  /* 数学分析 */

    /* 组合数学 */
    PRESET_EXT_COMBINATORICS,             /* 组合运算 */
    
    /* ---- v10.0 新增：补齐界面层缺失的扩展类别 ---- */
    PRESET_EXT_GRAPH_THEORY,              /* 图论运算 */
    PRESET_EXT_NUMERICAL_ANALYSIS,        /* 数值分析 */
    PRESET_EXT_OPTIMIZATION_THEORY,       /* 优化理论 */
    PRESET_EXT_MATH_LOGIC,                /* 数理逻辑 */
    
    PRESET_EXT_CATEGORY_COUNT             /* 类别总数（哨兵值） */
} PresetExtendedCategory;

/* ================================================================
 * 预设函数块元数据
 * ================================================================ */

/**
 * @brief 预设函数块的详细元数据（扩展版）
 *
 * 包含预设的完整信息，用于文档生成和IDE支持。
 * 注意：此类型与 func_block_preset.h 中的 PresetMetadata 不同，
 * 这是扩展版本，包含更多几何构造相关的元数据。
 */
typedef struct {
    const char *name;                     /* 预设名称（唯一键） */
    const char *description;              /* 中文描述 */
    const char *mathematical_definition;  /* 数学定义（LaTeX格式） */
    PresetExtendedCategory category;      /* 扩展类别 */
    int input_count;                      /* 输入端口数量 */
    int output_count;                     /* 输出端口数量 */
    bool has_selector;                    /* 是否需要多解选择器 */
    const char *preconditions;            /* 前置条件描述 */
    const char *example_usage;            /* 使用示例 */
} PresetBlockMetadata;

/* ================================================================
 * 预设函数块注册表扩展
 * ================================================================ */

/**
 * @brief 初始化扩展预设函数块系统
 *
 * 在 func_block_registry_init() 之后调用，
 * 注册所有扩展的预设函数块。
 *
 * @return true 初始化成功，false 内存不足
 */
bool preset_blocks_init(void);

/**
 * @brief 清理扩展预设函数块系统
 *
 * 释放所有扩展预设占用的资源。
 */
void preset_blocks_cleanup(void);

/**
 * @brief 获取预设的详细元数据
 *
 * 返回堆分配的元数据副本，调用者使用完毕后必须通过 lv00_free 释放。
 * 函数内部已加锁保护，线程安全。
 *
 * @param name 预设名称
 * @return 元数据指针（调用者负责释放），未找到或内存不足返回 NULL
 */
PresetBlockMetadata *preset_blocks_get_metadata(const char *name);

/**
 * @brief 按扩展类别查找预设
 *
 * @param category 目标类别
 * @param out_names 输出名称数组（调用者分配）
 * @param max_count 最大输出数量
 * @return 实际找到的预设数量
 */
int preset_blocks_find_by_category(PresetExtendedCategory category,
                                    const char **out_names,
                                    int max_count);

/**
 * @brief 将扩展类别转换为字符串
 *
 * @param cat 扩展类别
 * @return 类别名称字符串（中文）
 */
const char *preset_extended_category_to_string(PresetExtendedCategory cat);

/* ================================================================
 * 统一预设注册接口（v5.0）
 * ================================================================ */

/**
 * @brief 统一的预设注册接口
 *
 * 使用简化的参数列表注册预设函数块，替代旧的分类注册接口
 * （统一合并为 preset_blocks_register_by_category）。
 * 所有预设模块应统一使用此接口进行注册。
 *
 * @param name 预设名称（唯一键）
 * @param description 中文描述
 * @param category 预设类别（使用 PresetCategory 枚举）
 * @param input_types 输入类型数组（可为 NULL，当 input_count 为 0 时）
 * @param input_count 输入数量
 * @param output_type 输出类型
 * @param mathematical_definition 数学定义（LaTeX 格式，可为 NULL）
 * @param complexity 时间复杂度描述（如 "O(1)", "O(n)"，可为 NULL）
 * @param is_constructive 是否构造性
 * @param is_reversible 是否可逆
 * @return true 注册成功
 * @return false 注册失败
 */
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
    bool is_reversible);

/* ================================================================
 * 预设函数块批量操作
 * ================================================================ */

/**
 * @brief 注册预设的通用分类接口
 *
 * 合并原 preset_blocks_register_construction / _algebraic / _logic
 * 三个完全相同的函数。所有预设模块统一使用此接口。
 *
 * @param name 预设名称
 * @param description 中文描述
 * @param category 扩展类别
 * @param input_count 输入端口数
 * @param output_count 输出端口数
 * @return true 注册成功
 */
bool preset_blocks_register_by_category(const char *name,
                                         const char *description,
                                         PresetExtendedCategory category,
                                         int input_count,
                                         int output_count);

/* 向后兼容宏：旧的分类注册接口映射到统一接口 */
#define preset_blocks_register_construction(n, d, c, ic, oc) \
    preset_blocks_register_by_category((n), (d), (c), (ic), (oc))
#define preset_blocks_register_algebraic(n, d, c, ic, oc) \
    preset_blocks_register_by_category((n), (d), (c), (ic), (oc))
#define preset_blocks_register_logic(n, d, c, ic, oc) \
    preset_blocks_register_by_category((n), (d), (c), (ic), (oc))

/* ================================================================
 * 预设函数块快速查找
 * ================================================================ */

/**
 * @brief 按名称前缀查找预设
 *
 * 查找所有名称以给定前缀开头的预设。
 *
 * @param prefix 名称前缀
 * @param out_names 输出名称数组
 * @param max_count 最大输出数量
 * @return 实际找到的预设数量
 */
int preset_blocks_find_by_prefix(const char *prefix,
                                  const char **out_names,
                                  int max_count);

/**
 * @brief 按描述关键词查找预设
 *
 * 查找描述中包含给定关键词的预设。
 *
 * @param keyword 关键词
 * @param out_names 输出名称数组
 * @param max_count 最大输出数量
 * @return 实际找到的预设数量
 */
int preset_blocks_find_by_keyword(const char *keyword,
                                   const char **out_names,
                                   int max_count);

/**
 * @brief 获取所有预设名称列表
 *
 * @param out_names 输出名称数组（调用者分配）
 * @param max_count 最大输出数量
 * @return 实际返回的预设数量
 */
int preset_blocks_get_all_names(const char **out_names, int max_count);

/* ================================================================
 * 预设函数块文档生成
 * ================================================================ */

/**
 * @brief 生成预设函数块的Markdown文档
 *
 * 生成包含所有预设的完整文档，按类别组织。
 *
 * @return Markdown格式文档字符串（调用者负责释放）
 */
char *preset_blocks_generate_documentation(void);

/**
 * @brief 生成单个预设的详细文档
 *
 * @param name 预设名称
 * @return Markdown格式文档字符串（调用者负责释放）
 */
char *preset_blocks_generate_single_doc(const char *name);

/* ================================================================
 * 内置预设函数块常量定义
 * ================================================================ */

/* 基础几何构造 */

/** 中点：构造两点 A, B 的中点 M = (A + B) / 2
 *  输入：POINT, POINT → 输出：POINT
 *  复杂度：O(1)，确定性：ALWAYS_UNIQUE */
#ifndef PRESET_MIDPOINT
#define PRESET_MIDPOINT "midpoint"
#endif

/** 垂直平分线：构造线段 AB 的垂直平分线
 *  输入：POINT, POINT → 输出：LINE
 *  复杂度：O(1)，确定性：ALWAYS_UNIQUE */
#ifndef PRESET_PERPENDICULAR_BISECTOR
#define PRESET_PERPENDICULAR_BISECTOR "perpendicular_bisector"
#endif

/** 角平分线：构造角 ∠ABC 的角平分线
 *  输入：POINT, POINT, POINT → 输出：RAY
 *  复杂度：O(1)，确定性：ALWAYS_UNIQUE */
#ifndef PRESET_ANGLE_BISECTOR
#define PRESET_ANGLE_BISECTOR "angle_bisector"
#endif

/** 平行线：过点 P 作直线 L 的平行线
 *  输入：POINT, LINE → 输出：LINE
 *  复杂度：O(1)，确定性：ALWAYS_UNIQUE */
#ifndef PRESET_PARALLEL_LINE
#define PRESET_PARALLEL_LINE "parallel_line"
#endif

/** 垂线：过点 P 作直线 L 的垂线
 *  输入：POINT, LINE → 输出：LINE
 *  复杂度：O(1)，确定性：ALWAYS_UNIQUE */
#ifndef PRESET_PERPENDICULAR_LINE
#define PRESET_PERPENDICULAR_LINE "perpendicular_line"
#endif

/** 线段交点：计算两条直线的交点
 *  输入：LINE, LINE → 输出：POINT
 *  复杂度：O(1)，确定性：CONDITIONALLY_UNIQUE（平行时无解） */
#ifndef PRESET_LINE_INTERSECTION
#define PRESET_LINE_INTERSECTION "line_intersection"
#endif

/** 反射变换：关于直线 L 的反射变换
 *  输入：POINT, LINE → 输出：POINT
 *  复杂度：O(1)，确定性：ALWAYS_UNIQUE，可逆 */
#ifndef PRESET_REFLECTION
#define PRESET_REFLECTION "reflection"
#endif

/* 圆相关 */

/** 由圆心和半径构造圆：(x-a)² + (y-b)² = r²
 *  输入：POINT, SCALAR → 输出：CIRCLE
 *  复杂度：O(1)，确定性：ALWAYS_UNIQUE */
#ifndef PRESET_CIRCLE_BY_CENTER_RADIUS
#define PRESET_CIRCLE_BY_CENTER_RADIUS "circle_by_center_radius"
#endif

/** 由三点确定圆（不共线）
 *  输入：POINT, POINT, POINT → 输出：CIRCLE
 *  复杂度：O(1)，确定性：ALWAYS_UNIQUE */
#ifndef PRESET_CIRCLE_BY_THREE_POINTS
#define PRESET_CIRCLE_BY_THREE_POINTS "circle_by_three_points"
#endif

/** 切线：过圆上一点作切线
 *  输入：POINT, CIRCLE → 输出：LINE
 *  复杂度：O(1)，确定性：ALWAYS_UNIQUE */
#ifndef PRESET_TANGENT_LINE
#define PRESET_TANGENT_LINE "tangent_line"
#endif

/** 外接圆：过三角形三顶点的圆
 *  输入：POINT, POINT, POINT → 输出：CIRCLE
 *  复杂度：O(1)，确定性：ALWAYS_UNIQUE */
#ifndef PRESET_CIRCUMCIRCLE
#define PRESET_CIRCUMCIRCLE "circumcircle"
#endif

/** 内切圆：与三角形三边相切的圆
 *  输入：POINT, POINT, POINT → 输出：CIRCLE
 *  复杂度：O(1)，确定性：ALWAYS_UNIQUE */
#ifndef PRESET_INCIRCLE
#define PRESET_INCIRCLE "incircle"
#endif

/** 旁切圆：与三角形一边及另两边延长线相切的圆
 *  输入：POINT, POINT, POINT → 输出：CIRCLE
 *  复杂度：O(1)，确定性：MULTIPLE_SOLUTIONS（3个旁切圆） */
#ifndef PRESET_EXCIRCLE
#define PRESET_EXCIRCLE "excircle"
#endif

/* 多边形 */

/** 等边三角形：以线段 AB 为边构造等边三角形
 *  输入：POINT, POINT → 输出：POINT（第三顶点）
 *  复杂度：O(1)，确定性：MULTIPLE_SOLUTIONS（2个） */
#ifndef PRESET_EQUILATERAL_TRIANGLE
#define PRESET_EQUILATERAL_TRIANGLE "equilateral_triangle"
#endif

/** 正方形：以线段 AB 为边构造正方形
 *  输入：POINT, POINT → 输出：POINT, POINT（另外两个顶点）
 *  复杂度：O(1)，确定性：MULTIPLE_SOLUTIONS（2个） */
#ifndef PRESET_SQUARE
#define PRESET_SQUARE "square"
#endif

/** 正多边形：以圆心和半径构造正 n 边形
 *  输入：POINT, SCALAR, INTEGER → 输出：POLYGON
 *  复杂度：O(n)，确定性：ALWAYS_UNIQUE */
#ifndef PRESET_REGULAR_POLYGON
#define PRESET_REGULAR_POLYGON "regular_polygon"
#endif

/** 三角形重心：三条中线的交点 G = (A + B + C) / 3
 *  输入：POINT, POINT, POINT → 输出：POINT
 *  复杂度：O(1)，确定性：ALWAYS_UNIQUE */
#ifndef PRESET_TRIANGLE_CENTROID
#define PRESET_TRIANGLE_CENTROID "triangle_centroid"
#endif

/** 三角形垂心：三条高的交点
 *  输入：POINT, POINT, POINT → 输出：POINT
 *  复杂度：O(1)，确定性：ALWAYS_UNIQUE */
#ifndef PRESET_TRIANGLE_ORTHOCENTER
#define PRESET_TRIANGLE_ORTHOCENTER "triangle_orthocenter"
#endif

/** 三角形外心：三边垂直平分线的交点
 *  输入：POINT, POINT, POINT → 输出：POINT
 *  复杂度：O(1)，确定性：ALWAYS_UNIQUE */
#ifndef PRESET_TRIANGLE_CIRCUMCENTER
#define PRESET_TRIANGLE_CIRCUMCENTER "triangle_circumcenter"
#endif

/** 三角形内心：三条角平分线的交点
 *  输入：POINT, POINT, POINT → 输出：POINT
 *  复杂度：O(1)，确定性：ALWAYS_UNIQUE */
#ifndef PRESET_TRIANGLE_INCENTER
#define PRESET_TRIANGLE_INCENTER "triangle_incenter"
#endif

/* 几何变换 */

/** 平移变换：将点 P 沿向量 v 平移
 *  输入：POINT, VECTOR → 输出：POINT
 *  复杂度：O(1)，确定性：ALWAYS_UNIQUE，可逆 */
#ifndef PRESET_TRANSLATION
#define PRESET_TRANSLATION "translation"
#endif

/** 旋转变换：将点 P 绕中心 O 旋转角度 θ
 *  输入：POINT, POINT, SCALAR → 输出：POINT
 *  复杂度：O(1)，确定性：ALWAYS_UNIQUE，可逆 */
#ifndef PRESET_ROTATION
#define PRESET_ROTATION "rotation"
#endif

/** 位似变换：以中心 O 和比例 k 进行位似变换
 *  输入：POINT, SCALAR, SCALAR → 输出：POINT
 *  复杂度：O(1)，确定性：ALWAYS_UNIQUE，可逆（k≠0） */
#ifndef PRESET_HOMOTHETY
#define PRESET_HOMOTHETY "homothety"
#endif

/** 反射变换（矩阵形式）：关于直线的反射
 *  输入：POINT, LINE → 输出：POINT
 *  复杂度：O(1)，确定性：ALWAYS_UNIQUE，可逆（对合） */
#ifndef PRESET_REFLECTION_TRANSFORM
#define PRESET_REFLECTION_TRANSFORM "reflection_transform"
#endif

/** 仿射变换：一般仿射变换 T(x) = Ax + b
 *  输入：POINT, MATRIX, VECTOR → 输出：POINT
 *  复杂度：O(n²)，确定性：ALWAYS_UNIQUE，可逆（det(A)≠0） */
#ifndef PRESET_AFFINE_TRANSFORM
#define PRESET_AFFINE_TRANSFORM "affine_transform"
#endif

/** 反演变换：关于圆的反演变换
 *  输入：POINT, CIRCLE → 输出：POINT
 *  复杂度：O(1)，确定性：ALWAYS_UNIQUE，可逆（对合） */
#ifndef PRESET_INVERSION
#define PRESET_INVERSION "inversion"
#endif

/* 度量计算 */

/** 两点距离：d(A, B) = √((x₂-x₁)² + (y₂-y₁)²)
 *  输入：POINT, POINT → 输出：SCALAR
 *  复杂度：O(1)，确定性：ALWAYS_UNIQUE */
#ifndef PRESET_DISTANCE
#define PRESET_DISTANCE "distance"
#endif

/** 角度测量：计算三点形成的角度（度或弧度）
 *  输入：POINT, POINT, POINT → 输出：SCALAR
 *  复杂度：O(1)，确定性：ALWAYS_UNIQUE */
#ifndef PRESET_ANGLE_MEASURE
#define PRESET_ANGLE_MEASURE "angle_measure"
#endif

/** 面积计算：计算多边形或三角形的面积
 *  输入：POLYGON → 输出：SCALAR
 *  复杂度：O(n)，确定性：ALWAYS_UNIQUE */
#ifndef PRESET_AREA
#define PRESET_AREA "area"
#endif

/** 周长计算：计算多边形的周长
 *  输入：POLYGON → 输出：SCALAR
 *  复杂度：O(n)，确定性：ALWAYS_UNIQUE */
#ifndef PRESET_PERIMETER
#define PRESET_PERIMETER "perimeter"
#endif

/* 三角函数 */

/** 正弦函数：sin(θ)
 *  输入：SCALAR → 输出：SCALAR
 *  复杂度：O(1)，确定性：ALWAYS_UNIQUE */
#ifndef PRESET_SINE
#define PRESET_SINE "sine"
#endif

/** 余弦函数：cos(θ)
 *  输入：SCALAR → 输出：SCALAR
 *  复杂度：O(1)，确定性：ALWAYS_UNIQUE */
#ifndef PRESET_COSINE
#define PRESET_COSINE "cosine"
#endif

/** 正切函数：tan(θ)
 *  输入：SCALAR → 输出：SCALAR
 *  复杂度：O(1)，确定性：CONDITIONALLY_UNIQUE（θ≠π/2+kπ） */
#ifndef PRESET_TANGENT
#define PRESET_TANGENT "tangent"
#endif

/** 反正切函数：arctan(y/x)
 *  输入：SCALAR, SCALAR → 输出：SCALAR
 *  复杂度：O(1)，确定性：ALWAYS_UNIQUE */
#ifndef PRESET_ARCTANGENT
#define PRESET_ARCTANGENT "arctangent"
#endif

/* 代数运算 */

/** 向量加法：u + v
 *  输入：VECTOR, VECTOR → 输出：VECTOR
 *  复杂度：O(n)，确定性：ALWAYS_UNIQUE，可逆 */
#ifndef PRESET_VECTOR_ADD
#define PRESET_VECTOR_ADD "vector_add"
#endif

/** 向量数乘：k·v
 *  输入：SCALAR, VECTOR → 输出：VECTOR
 *  复杂度：O(n)，确定性：ALWAYS_UNIQUE，可逆（k≠0） */
#ifndef PRESET_VECTOR_SCALE
#define PRESET_VECTOR_SCALE "vector_scale"
#endif

/** 向量点积：u · v = Σuᵢvᵢ
 *  输入：VECTOR, VECTOR → 输出：SCALAR
 *  复杂度：O(n)，确定性：ALWAYS_UNIQUE */
#ifndef PRESET_VECTOR_DOT
#define PRESET_VECTOR_DOT "vector_dot"
#endif

/** 向量叉积：u × v（三维）
 *  输入：VECTOR, VECTOR → 输出：VECTOR
 *  复杂度：O(1)，确定性：ALWAYS_UNIQUE */
#ifndef PRESET_VECTOR_CROSS
#define PRESET_VECTOR_CROSS "vector_cross"
#endif

/** 多项式求根：求解多项式方程 p(x) = 0
 *  输入：POLYNOMIAL → 输出：LIST
 *  复杂度：O(n³)（特征值法），确定性：MULTIPLE_SOLUTIONS */
#ifndef PRESET_POLYNOMIAL_ROOTS
#define PRESET_POLYNOMIAL_ROOTS "polynomial_roots"
#endif

/** 线性方程组求解：Ax = b
 *  输入：MATRIX, VECTOR → 输出：VECTOR
 *  复杂度：O(n³)，确定性：CONDITIONALLY_UNIQUE */
#ifndef PRESET_LINEAR_SOLVE
#define PRESET_LINEAR_SOLVE "linear_solve"
#endif

/* 逻辑推导 */

/** 矛盾检测器：检测约束系统中是否存在矛盾
 *  输入：FORMULA → 输出：BOOLEAN
 *  复杂度：O(n)，确定性：ALWAYS_UNIQUE */
#ifndef PRESET_CONTRADICTION_DETECTOR
#define PRESET_CONTRADICTION_DETECTOR "contradiction_detector"
#endif

/** 蕴含链：构造 A → B → C 的逻辑蕴含链
 *  输入：FORMULA, FORMULA → 输出：FORMULA
 *  复杂度：O(n)，确定性：ALWAYS_UNIQUE */
#ifndef PRESET_IMPLICATION_CHAIN
#define PRESET_IMPLICATION_CHAIN "implication_chain"
#endif

/** 等价关系：构造 A ↔ B 的逻辑等价
 *  输入：FORMULA, FORMULA → 输出：FORMULA
 *  复杂度：O(1)，确定性：ALWAYS_UNIQUE */
#ifndef PRESET_EQUIVALENCE
#define PRESET_EQUIVALENCE "equivalence"
#endif

/** 全称量词：构造 ∀x. P(x)
 *  输入：FORMULA → 输出：FORMULA
 *  复杂度：O(1)，确定性：ALWAYS_UNIQUE */
#ifndef PRESET_UNIVERSAL_QUANTIFIER
#define PRESET_UNIVERSAL_QUANTIFIER "universal_quantifier"
#endif

/** 存在量词：构造 ∃x. P(x)
 *  输入：FORMULA → 输出：FORMULA
 *  复杂度：O(1)，确定性：ALWAYS_UNIQUE */
#ifndef PRESET_EXISTENTIAL_QUANTIFIER
#define PRESET_EXISTENTIAL_QUANTIFIER "existential_quantifier"
#endif

/* ================================================================
 * 预设函数块统计信息
 * ================================================================ */

/**
 * @brief 获取预设统计信息
 *
 * @param total_count 输出：预设总数
 * @param by_category 输出：各类别数量数组（大小为 PRESET_EXT_CATEGORY_COUNT）
 */
void preset_blocks_get_stats(int *total_count, int *by_category);

/**
 * @brief 打印预设统计信息
 *
 * 将统计信息输出到标准输出，用于调试和监控。
 */
void preset_blocks_print_stats(void);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_BLOCKS_H */

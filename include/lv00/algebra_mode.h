/**
 * @file algebra_mode.h
 * @brief 代数模式构造引擎 —— 借鉴 build123d 代数模式 + CadQuery Fluent API
 *
 * @details 设计借鉴：
 * - build123d (github.com/gumyr/build123d)
 *   - 代数模式：所有操作返回新对象，无隐式状态
 *   - 变换链语法：Plane.XZ * Pos(X=5) * Rectangle(1,1)
 *   - 双模式共存：AlgebraMode + BuilderMode
 * - CadQuery (github.com/CadQuery/cadquery)
 *   - Fluent API：box.faces(">Z").workplane().circle(2).extrude(1)
 *   - Selector DSL：faces(">Z")、edges("|Z")
 *
 * 设计目标："构造即运算"——每个几何操作既创建新对象也记录构造历史
 *
 * @version v3.3.0
 * @date 2026-05-24
 */

#ifndef LV00_ALGEBRA_MODE_H
#define LV00_ALGEBRA_MODE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 *  前向声明
 * ================================================================ */

typedef struct ConstraintGraph ConstraintGraph;
typedef struct DslIR       DslIR;

/* ================================================================
 *  第一部分：平面与变换定义
 * ================================================================ */

/**
 * @brief 工作平面枚举
 *
 * 借鉴 build123d 的 Plane 概念——定义几何操作的基准平面。
 * PLANE_CUSTOM 允许用户通过法向量和原点定义任意平面。
 */
typedef enum {
    PLANE_XY,                    /**< XY 平面（默认） */
    PLANE_XZ,                    /**< XZ 平面 */
    PLANE_YZ,                    /**< YZ 平面 */
    PLANE_CUSTOM                 /**< 自定义平面（由法向量和原点定义） */
} Lv00Plane;

/**
 * @brief 变换操作类型枚举
 *
 * 借鉴 build123d 的变换链语法——每种变换可串接，
 * 复合变换通过矩阵乘法累积。支持平移、旋转、缩放、镜像和投影。
 */
typedef enum {
    TRANSFORM_TRANSLATE,         /**< 平移变换 */
    TRANSFORM_ROTATE,            /**< 旋转变换 */
    TRANSFORM_SCALE,             /**< 缩放变换 */
    TRANSFORM_MIRROR,            /**< 镜像变换 */
    TRANSFORM_PROJECT            /**< 投影变换（到指定平面） */
} Lv00TransformOp;

/* ================================================================
 *  第二部分：选择器系统（借鉴 CadQuery Selector DSL）
 * ================================================================ */

/**
 * @brief 选择器类型枚举
 *
 * 借鉴 CadQuery 的 Selector DSL 设计，提供多种子实体访问策略。
 * 选择器按字符串表达式构建：faces(">Z")、edges("|Z")、vertices("<X")。
 * 共 12 种类型。
 */
typedef enum {
    SELECTOR_ALL,                /**< 选择所有子实体 */
    SELECTOR_BY_DIRECTION,       /**< 按方向选择（如 ">Z" 表示法向朝 +Z 的面） */
    SELECTOR_BY_TAG,             /**< 按标签选择 */
    SELECTOR_BY_TYPE,            /**< 按实体类型选择（faces/edges/vertices） */
    SELECTOR_NEAREST,            /**< 选择最近子实体（按距离排序） */
    SELECTOR_LARGEST,            /**< 选择最大子实体（按面积/长度排序） */
    SELECTOR_SMALLEST,           /**< 选择最小子实体 */
    SELECTOR_PARALLEL_TO,        /**< 选择与给定方向平行的子实体 */
    SELECTOR_PERPENDICULAR_TO,   /**< 选择与给定方向垂直的子实体 */
    SELECTOR_AT_LOCATION,        /**< 选择包含指定位置的子实体 */
    SELECTOR_BY_INDEX,           /**< 按索引选择 */
    SELECTOR_COMPOSITE           /**< 复合选择器（AND/OR/NOT 组合） */
} Lv00SelectorType;

/**
 * @brief 选择器方向运算符枚举
 *
 * CadQuery 风格的方向选择符，用于 SELECTOR_BY_DIRECTION。
 * 示例：">Z" 选择法向朝 +Z 的面，"|X" 选择与 X 轴平行的边。
 */
typedef enum {
    SEL_DIR_GREATER,             /**< > 方向算符（法向指向正方向） */
    SEL_DIR_LESS,                /**< < 方向算符（法向指向负方向） */
    SEL_DIR_PARALLEL             /**< | 方向算符（与指定轴平行） */
} Lv00SelectorDirOp;

/**
 * @brief 选择器结构体
 *
 * 表示一个子实体选择器。借鉴 CadQuery 的 Selector 设计——
 * 通过字符串表达式构建，支持嵌套组合。
 *
 * 使用方法：
 *   Lv00Selector *sel = algebra_selector_create(SELECTOR_BY_DIRECTION, ">Z");
 *   // 选择所有法向朝 +Z 的面
 */
typedef struct Lv00Selector {
    Lv00SelectorType   type;     /**< 选择器类型 */
    char              *expr;     /**< 选择器表达式字符串（拥有所有权） */
    Lv00SelectorDirOp   dir_op;  /**< 方向运算符（BY_DIRECTION 时有效） */
    char               axis;     /**< 目标轴 'X'/'Y'/'Z'（BY_DIRECTION/PARALLEL_TO/PERPENDICULAR_TO 时有效） */
    int                index;    /**< 索引值（BY_INDEX 时有效） */
    double             distance; /**< 距离参数（NEAREST 时有效） */
    struct Lv00Selector **children; /**< 子选择器（COMPOSITE 时有效） */
    int                child_count;     /**< 子选择器数量 */
    int                child_capacity;  /**< 子选择器容量 */
    /* 复合选择器的组合操作 */
    bool               is_union;        /**< true=OR, false=AND（COMPOSITE 时有效） */
    bool               is_negated;      /**< 是否取反（NOT 语义） */
} Lv00Selector;

/* ================================================================
 *  第三部分：代数操作结果状态
 * ================================================================ */

/**
 * @brief 代数操作结果状态枚举
 *
 * 每次几何操作返回一个状态码，表示操作是否成功执行，
 * 以及约束系统的可满足性状态。
 */
typedef enum {
    ALGEBRA_OK,                  /**< 操作成功，约束系统一致 */
    ALGEBRA_OVERCONSTRAINED,     /**< 约束过多，系统超定 */
    ALGEBRA_AMBIGUOUS,           /**< 结果不唯一，存在多解 */
    ALGEBRA_INFEASIBLE,          /**< 约束不可满足（矛盾） */
    ALGEBRA_DEGENERATE,          /**< 退化情况（如三点共线构造三角形） */
    ALGEBRA_OUT_OF_MEMORY,       /**< 内存不足 */
    ALGEBRA_INVALID_ARGUMENT     /**< 无效参数（NULL 指针、非法值） */
} AlgebraOpResult;

/* ================================================================
 *  第四部分：代数几何体（不透明句柄）
 * ================================================================ */

/**
 * @brief 代数几何体结构
 *
 * 借鉴 build123d 代数模式的核心抽象——所有操作返回新对象，
 * 不修改原对象（无状态、不可变）。每个几何体包含：
 * - 约束图句柄：记录构造过程中积累的所有约束
 * - 变换链：累积的变换矩阵（平移 + 旋转 + 缩放）
 * - 构造历史：构造步骤的有序记录
 *
 * 所有 API 函数返回 AlgebraicGeom* 指针（或 NULL 表示错误），
 * 允许链式调用风格：
 *   algebra_point(0,0,0)
 *     ->algebra_point(10,0,0)
 *     ->algebra_line()
 *     ->algebra_circle_radius(5)
 *     ->algebra_build();
 */
typedef struct AlgebraicGeom {
    ConstraintGraph *graph;           /**< 关联的约束图 */
    int              current_entity;  /**< 最近创建实体的图节点 ID */
    int              plane;           /**< 当前工作平面（Lv00Plane 值） */
    /* 变换链（累积的 4x4 齐次变换矩阵） */
    double           transform[16];   /**< 变换矩阵（列主序，4x4） */
    bool             has_transform;   /**< 是否有待应用变换 */
    /* 构造历史（用于 undo/redo） */
    int             *history;         /**< 构造步骤记录 */
    int              history_count;   /**< 历史记录数量 */
    int              history_capacity; /**< 历史记录容量 */
    /* 快照栈（用于 snapshot/restore） */
    struct AlgebraicGeom **snapshots; /**< 快照栈 */
    int                    snapshot_count;   /**< 快照数量 */
    int                    snapshot_capacity; /**< 快照容量 */
    /* 重做栈 */
    int             *redo_stack;      /**< 重做操作栈 */
    int              redo_count;      /**< 重做操作数量 */
    int              redo_capacity;   /**< 重做栈容量 */
    /* 元信息 */
    int              id;              /**< 几何体唯一标识 */
    char            *name;            /**< 可选名称 */
} AlgebraicGeom;

/* ================================================================
 *  第五部分：代数模式 API
 *
 *  所有构造函数遵循代数模式（无状态、返回新句柄），允许链式调用。
 *  每个函数返回时同时记录构造历史，支持 undo/redo/snapshot。
 * ================================================================ */

/* ---- 生命周期 ---- */

/**
 * @brief 创建代数几何体上下文
 *
 * 初始化空的约束图和变换链。所有后续操作在此上下文上执行。
 *
 * @param plane 初始工作平面
 * @param name  可选名称（可为 NULL）
 * @return 新分配的 AlgebraicGeom*，失败返回 NULL
 */
AlgebraicGeom *algebra_create(Lv00Plane plane, const char *name);

/**
 * @brief 销毁代数几何体上下文
 *
 * 释放约束图、变换链、构造历史和所有内部资源。
 *
 * @param geom 代数几何体（可为 NULL）
 */
void algebra_destroy(AlgebraicGeom *geom);

/* ---- 点构造 ---- */

/**
 * @brief 在指定坐标创建点
 *
 * @param geom 代数几何体
 * @param x    X 坐标
 * @param y    Y 坐标
 * @param z    Z 坐标（二维时可传 0）
 * @return geom 自身（链式调用），失败返回 NULL
 */
AlgebraicGeom *algebra_point(AlgebraicGeom *geom, double x, double y, double z);

/**
 * @brief 在已有几何体上创建点
 *
 * @param geom   代数几何体
 * @param entity_id 目标实体 ID（线上点、圆上点等）
 * @return geom 自身，失败返回 NULL
 */
AlgebraicGeom *algebra_point_on(AlgebraicGeom *geom, int entity_id);

/**
 * @brief 创建两点的中点
 *
 * @param geom  代数几何体
 * @param id_a  第一个点的实体 ID
 * @param id_b  第二个点的实体 ID
 * @return geom 自身，失败返回 NULL
 */
AlgebraicGeom *algebra_midpoint(AlgebraicGeom *geom, int id_a, int id_b);

/**
 * @brief 创建两几何体的交点
 *
 * @param geom  代数几何体
 * @param id_a  第一个几何体实体 ID
 * @param id_b  第二个几何体实体 ID
 * @return geom 自身，失败返回 NULL（如平行线无交点）
 */
AlgebraicGeom *algebra_intersect(AlgebraicGeom *geom, int id_a, int id_b);

/* ---- 线构造 ---- */

/**
 * @brief 通过两点创建直线
 *
 * @param geom  代数几何体
 * @param id_a  第一个点的实体 ID
 * @param id_b  第二个点的实体 ID
 * @return geom 自身，失败返回 NULL
 */
AlgebraicGeom *algebra_line(AlgebraicGeom *geom, int id_a, int id_b);

/**
 * @brief 通过两点创建线段
 *
 * @param geom  代数几何体
 * @param id_a  起点实体 ID
 * @param id_b  终点实体 ID
 * @return geom 自身，失败返回 NULL
 */
AlgebraicGeom *algebra_segment(AlgebraicGeom *geom, int id_a, int id_b);

/**
 * @brief 通过原点和方向点创建射线
 *
 * @param geom       代数几何体
 * @param origin_id  原点实体 ID
 * @param through_id 方向上一点的实体 ID
 * @return geom 自身，失败返回 NULL
 */
AlgebraicGeom *algebra_ray(AlgebraicGeom *geom, int origin_id, int through_id);

/* ---- 圆构造 ---- */

/**
 * @brief 通过圆心和半径创建圆
 *
 * @param geom      代数几何体
 * @param center_id 圆心实体 ID
 * @param radius    半径
 * @return geom 自身，失败返回 NULL
 */
AlgebraicGeom *algebra_circle_radius(AlgebraicGeom *geom, int center_id, double radius);

/**
 * @brief 通过圆心和圆周上一点创建圆
 *
 * @param geom        代数几何体
 * @param center_id   圆心实体 ID
 * @param on_circle_id 圆上一点的实体 ID
 * @return geom 自身，失败返回 NULL
 */
AlgebraicGeom *algebra_circle(AlgebraicGeom *geom, int center_id, int on_circle_id);

/* ---- 特殊线构造 ---- */

/**
 * @brief 通过点作平行线
 *
 * @param geom      代数几何体
 * @param line_id   参考直线实体 ID
 * @param point_id  通过点的实体 ID
 * @return geom 自身，失败返回 NULL
 */
AlgebraicGeom *algebra_parallel(AlgebraicGeom *geom, int line_id, int point_id);

/**
 * @brief 通过点作垂线
 *
 * @param geom      代数几何体
 * @param line_id   参考直线实体 ID
 * @param point_id  通过点的实体 ID
 * @return geom 自身，失败返回 NULL
 */
AlgebraicGeom *algebra_perpendicular(AlgebraicGeom *geom, int line_id, int point_id);

/* ---- 变换操作（借鉴 build123d 变换链语法）---- */

/**
 * @brief 应用通用变换
 *
 * @param geom      代数几何体
 * @param op        变换类型
 * @param params    变换参数数组（长度依赖于 op 类型）
 * @param param_count 参数数量
 * @return geom 自身，失败返回 NULL
 */
AlgebraicGeom *algebra_transform(AlgebraicGeom *geom, Lv00TransformOp op,
                                  const double *params, int param_count);

/**
 * @brief 绕指定轴旋转
 *
 * @param geom      代数几何体
 * @param angle_deg 旋转角度（度）
 * @param axis_x    旋转轴 X 分量
 * @param axis_y    旋转轴 Y 分量
 * @param axis_z    旋转轴 Z 分量
 * @return geom 自身，失败返回 NULL
 */
AlgebraicGeom *algebra_rotate(AlgebraicGeom *geom, double angle_deg,
                               double axis_x, double axis_y, double axis_z);

/**
 * @brief 平移
 *
 * @param geom 代数几何体
 * @param dx   X 方向偏移
 * @param dy   Y 方向偏移
 * @param dz   Z 方向偏移
 * @return geom 自身，失败返回 NULL
 */
AlgebraicGeom *algebra_translate(AlgebraicGeom *geom, double dx, double dy, double dz);

/**
 * @brief 缩放
 *
 * @param geom 代数几何体
 * @param sx   X 方向缩放因子
 * @param sy   Y 方向缩放因子
 * @param sz   Z 方向缩放因子
 * @return geom 自身，失败返回 NULL
 */
AlgebraicGeom *algebra_scale(AlgebraicGeom *geom, double sx, double sy, double sz);

/* ---- 选择器操作（借鉴 CadQuery Selector DSL）---- */

/**
 * @brief 创建选择器
 *
 * 借鉴 CadQuery Selector DSL，通过类型和表达式字符串构建选择器。
 *
 * @param type   选择器类型
 * @param expr   选择器表达式（如 ">Z"、"|X"、"<Y"、tag 名称，可为 NULL）
 * @return 新创建的 Lv00Selector*，失败返回 NULL
 */
Lv00Selector *algebra_selector_create(Lv00SelectorType type, const char *expr);

/**
 * @brief 销毁选择器
 *
 * @param sel 选择器（可为 NULL）
 */
void algebra_selector_destroy(Lv00Selector *sel);

/**
 * @brief 选择满足条件的子实体
 *
 * @param geom      代数几何体
 * @param sel       选择器
 * @param out_ids   输出：满足条件的实体 ID 数组（调用者释放）
 * @param out_count 输出：实体数量
 * @return geom 自身，失败返回 NULL
 */
AlgebraicGeom *algebra_select(AlgebraicGeom *geom, const Lv00Selector *sel,
                               int **out_ids, int *out_count);

/* ---- 约束与证明 ---- */

/**
 * @brief 添加约束
 *
 * @param geom        代数几何体
 * @param constraint_type 约束类型字符串（"incidence"/"between"/"intersect"/"contain"）
 * @param entity_ids  关联实体 ID 数组
 * @param count       ID 数量
 * @return geom 自身，失败返回 NULL
 */
AlgebraicGeom *algebra_constrain(AlgebraicGeom *geom, const char *constraint_type,
                                  const int *entity_ids, int count);

/**
 * @brief 启动证明
 *
 * @param geom         代数几何体
 * @param proposition  要证明的命题表达式
 * @return geom 自身，失败返回 NULL
 */
AlgebraicGeom *algebra_prove(AlgebraicGeom *geom, const char *proposition);

/* ---- 构建与查询 ---- */

/**
 * @brief 最终构建约束图
 *
 * 将所有累积的构造操作编译为约束图，应用变换链，
 * 解决符号引用并验证约束一致性。这是代数模式的"提交"操作。
 *
 * @param geom 代数几何体
 * @return 操作结果状态
 */
AlgebraOpResult algebra_build(AlgebraicGeom *geom);

/**
 * @brief 获取约束图（只读访问）
 *
 * 在 algebra_build 后使用，获取编译完成的约束图。
 *
 * @param geom 代数几何体
 * @return 约束图指针（geom 生命周期内有效），未构建时返回 NULL
 */
ConstraintGraph *algebra_get_graph(const AlgebraicGeom *geom);

/**
 * @brief 获取最近一次操作的状态
 *
 * @param geom 代数几何体
 * @return 操作结果状态
 */
AlgebraOpResult algebra_get_status(const AlgebraicGeom *geom);

/**
 * @brief 获取最近创建的实体 ID
 *
 * @param geom 代数几何体
 * @return 实体 ID，未创建时返回 -1
 */
int algebra_get_current_entity(const AlgebraicGeom *geom);

/* ---- Undo/Redo ---- */

/**
 * @brief 撤销上一次操作
 *
 * 回退构造历史中的最后一步，恢复几何体到操作前的状态。
 *
 * @param geom 代数几何体
 * @return geom 自身，无可撤销步骤时返回 NULL
 */
AlgebraicGeom *algebra_undo(AlgebraicGeom *geom);

/**
 * @brief 重做上一次撤销的操作
 *
 * @param geom 代数几何体
 * @return geom 自身，无可重做步骤时返回 NULL
 */
AlgebraicGeom *algebra_redo(AlgebraicGeom *geom);

/* ---- 快照/回退 ---- */

/**
 * @brief 创建当前状态的快照
 *
 * 将当前完整状态压入快照栈，后续可通过 algebra_restore 恢复。
 * 快照包含约束图和变换链的深拷贝。
 *
 * @param geom 代数几何体
 * @return 快照索引（>=0），失败返回 -1
 */
int algebra_snapshot(AlgebraicGeom *geom);

/**
 * @brief 恢复到指定快照
 *
 * 丢弃当前状态，恢复到指定快照对应的完整状态。
 * 快照本身在恢复后保留（可再次恢复）。
 *
 * @param geom          代数几何体
 * @param snapshot_index 快照索引（由 algebra_snapshot 返回）
 * @return geom 自身，无效索引时返回 NULL
 */
AlgebraicGeom *algebra_restore(AlgebraicGeom *geom, int snapshot_index);

/* ---- 工作平面 ---- */

/**
 * @brief 切换当前工作平面
 *
 * 借鉴 build123d 的 Plane 概念，改变后续操作的基准平面。
 *
 * @param geom  代数几何体
 * @param plane 新的工作平面
 * @return geom 自身
 */
AlgebraicGeom *algebra_set_plane(AlgebraicGeom *geom, Lv00Plane plane);

/**
 * @brief 获取当前工作平面
 *
 * @param geom 代数几何体
 * @return 当前工作平面枚举值
 */
Lv00Plane algebra_get_plane(const AlgebraicGeom *geom);

/* ---- 工具函数 ---- */

/**
 * @brief 获取操作结果状态的字符串名称
 *
 * @param result 操作结果状态
 * @return 静态字符串（不需要释放）
 */
const char *algebra_result_name(AlgebraOpResult result);

/**
 * @brief 获取选择器类型的字符串名称
 *
 * @param type 选择器类型
 * @return 静态字符串（不需要释放）
 */
const char *algebra_selector_type_name(Lv00SelectorType type);

#ifdef __cplusplus
}
#endif

#endif /* LV00_ALGEBRA_MODE_H */

/**
 * @file formula_converter.h
 * @brief 公式与约束图之间的双向转换
 *
 * @details 提供公式 AST 到约束图的转换，以及约束图到公式的转换功能。
 *          这是 Lv-00 几何元语言系统中公式编辑器与图形系统之间的桥梁。
 *
 * @author Lv-00 Project
 * @version 1.1.0
 */
#ifndef lv_FORMULA_CONVERTER_H
#define lv_FORMULA_CONVERTER_H
#ifdef __cplusplus
extern "C" {
#endif
#include <stdbool.h>

#include "constraint_graph.h"
#include "formula_parser.h"
/* ============================================================
 * 公式 → 图转换结果
 * ============================================================ */
typedef struct {
    bool success;
    int *created_node_ids;          /* 创建的节点 ID 数组 */
    int created_node_count;         /* 创建的节点数量 */
    int created_node_capacity;      /* 创建的节点数组容量（lv_ensure_capacity 维护） */
    int *created_constraint_ids;    /* 创建的约束 ID 数组 */
    int created_constraint_count;   /* 创建的约束数量 */
    int created_constraint_capacity;/* 创建的约束数组容量（lv_ensure_capacity 维护） */
    char error_message[256];      /* 错误信息 */
} FormulaToGraphResult;
/* ============================================================
 * 图 → 公式转换结果
 * ============================================================ */
typedef struct {
    bool success;
    char *latex_output;      /* LaTeX 格式输出 */
    char *python_output;     /* Python 格式输出 */
    char *dsl_output;        /* DSL 格式输出 */
    char error_message[256]; /* 错误信息 */
} GraphToFormulaResult;
/* ============================================================
 * 核心 API
 * ============================================================ */
/**
 * @brief 将公式 AST 转换为约束图操作（[take] 结果所有权，memory-ownership.md）
 * @param[in] ast   公式 AST 根节点
 * @param[in] graph 目标约束图
 * @return 转换结果，调用者负责通过 formula_to_graph_result_destroy() 销毁
 */
FormulaToGraphResult *formula_to_graph(const FormulaNode *ast, ConstraintGraph *graph);
/**
 * @brief 将约束图转换为公式（[take] 结果所有权，memory-ownership.md）
 * @param[in] graph 源约束图
 * @return 转换结果，调用者负责通过 graph_to_formula_result_destroy() 销毁
 */
GraphToFormulaResult *graph_to_formula(const ConstraintGraph *graph);
/* ============================================================
 * 结果销毁函数
 * ============================================================ */
/**
 * @brief 销毁公式到图的转换结果
 * @param[in] result 转换结果指针，可为 NULL
 */
void formula_to_graph_result_destroy(FormulaToGraphResult *result);
/**
 * @brief 销毁图到公式的转换结果
 * @param[in] result 转换结果指针，可为 NULL
 */
void graph_to_formula_result_destroy(GraphToFormulaResult *result);
/* ============================================================
 * 变量名到节点ID的映射
 * ============================================================ */
/**
 * @brief 根据变量名获取节点ID
 * @param[in] var_name 变量名（点名、线段名等）
 * @return 节点ID，未找到返回 -1
 */
int formula_get_node_id(const char *var_name);
/**
 * @brief 设置变量名到节点ID的映射
 * @param[in] var_name 变量名
 * @param[in] node_id  节点ID
 */
void formula_set_node_id(const char *var_name, int node_id);
/**
 * @brief 清除所有变量名映射
 */
void formula_clear_var_map(void);
/**
 * @brief 释放变量映射表的 TLS 堆缓冲区（lv_cleanup 时调用，防泄漏）
 */
void formula_converter_util_cleanup(void);
/* ============================================================
 * 辅助转换函数
 * ============================================================ */
/**
 * @brief 从 AST 数字节点创建 SymbolicCoord（[take] 结果所有权，memory-ownership.md）
 * @param[in] node 数字节点（类型须为 NODE_NUMBER）
 * @return 符号坐标指针，调用者负责销毁；失败返回 NULL
 */
SymbolicCoord *formula_number_to_coord(const FormulaNode *node);
/**
 * @brief 从坐标列表节点创建 SymbolicCoord 数组（[take] 结果所有权：调用者负责
 * 销毁每个元素和数组本身，memory-ownership.md）
 * @param[in]  coord_list 坐标列表节点
 * @param[out] out_count  输出坐标数量
 * @return SymbolicCoord 数组，调用者负责销毁每个元素和数组本身
 */
SymbolicCoord **formula_coords_to_symbolic(const FormulaNode *coord_list, int *out_count);
/**
 * @brief 从 GeomNode 生成名称
 * @param[in]  node     几何节点
 * @param[out] out_name 输出名称缓冲区
 * @param[in]  buf_size 缓冲区大小
 * @return 成功返回 true，失败返回 false
 */
bool formula_node_to_name(const GeomNode *node, char *out_name, size_t buf_size);
/* ============================================================
 * 几何对象转换
 * ============================================================ */
/**
 * @brief 转换点定义到约束图
 * @param[in]  point_node 点节点（类型须为 NODE_GEOM_POINT）
 * @param[in]  graph      目标图
 * @param[out] out_node_id 输出节点ID
 * @return 成功返回 true，失败返回 false
 */
bool formula_convert_point(const FormulaNode *point_node, ConstraintGraph *graph, int *out_node_id);
/**
 * @brief 转换线段定义到约束图
 * @param[in]  segment_node 线段节点（类型须为 NODE_GEOM_SEGMENT）
 * @param[in]  graph        目标图
 * @param[out] out_node_id  输出节点ID
 * @return 成功返回 true，失败返回 false
 */
bool formula_convert_segment(const FormulaNode *segment_node, ConstraintGraph *graph, int *out_node_id);
/**
 * @brief 转换圆定义到约束图
 * @param[in]  circle_node 圆节点（类型须为 NODE_GEOM_CIRCLE）
 * @param[in]  graph       目标图
 * @param[out] out_node_id 输出节点ID
 * @return 成功返回 true，失败返回 false
 */
bool formula_convert_circle(const FormulaNode *circle_node, ConstraintGraph *graph, int *out_node_id);
/**
 * @brief 转换三角形定义到约束图
 * @param[in]  triangle_node 三角形节点（类型须为 NODE_GEOM_TRIANGLE）
 * @param[in]  graph         目标图
 * @param[out] out_node_ids  输出节点ID数组（3个顶点 + 3条边）
 * @param[out] out_count     输出节点数量
 * @return 成功返回 true，失败返回 false
 */
bool formula_convert_triangle(const FormulaNode *triangle_node, ConstraintGraph *graph, int *out_node_ids,
                              int *out_count);
/* ============================================================
 * 约束转换
 * ============================================================ */
/**
 * @brief 转换垂直约束到约束图
 * @param[in]  constraint_node 约束节点（类型须为 NODE_CONSTRAINT_PERPENDICULAR）
 * @param[in]  graph           目标图
 * @param[out] out_constraint_id 输出约束ID
 * @return 成功返回 true，失败返回 false
 */
bool formula_convert_perpendicular(const FormulaNode *constraint_node, ConstraintGraph *graph, int *out_constraint_id);
/**
 * @brief 转换平行约束到约束图
 * @param[in]  constraint_node 约束节点（类型须为 NODE_CONSTRAINT_PARALLEL）
 * @param[in]  graph           目标图
 * @param[out] out_constraint_id 输出约束ID
 * @return 成功返回 true，失败返回 false
 */
bool formula_convert_parallel(const FormulaNode *constraint_node, ConstraintGraph *graph, int *out_constraint_id);
/**
 * @brief 转换中点约束到约束图
 * @param[in]  constraint_node 约束节点（类型须为 NODE_CONSTRAINT_MIDPOINT）
 * @param[in]  graph           目标图
 * @param[out] out_node_id     输出中点节点ID
 * @return 成功返回 true，失败返回 false
 */
bool formula_convert_midpoint(const FormulaNode *constraint_node, ConstraintGraph *graph, int *out_node_id);
/**
 * @brief 转换角度约束到约束图
 *
 * 角度约束 ∠ABC = θ 转换为向量点积约束：
 *   BA · BC = |BA| * |BC| * cos(θ)
 *
 * @param[in]  constraint_node   约束节点（类型须为 NODE_CONSTRAINT_ANGLE）
 * @param[in]  graph             目标图
 * @param[out] out_constraint_id 输出约束ID
 * @return 成功返回 true，失败返回 false
 */
bool formula_convert_angle(const FormulaNode *constraint_node, ConstraintGraph *graph, int *out_constraint_id);
/* ============================================================
 * 代数方程转换
 * ============================================================ */
/**
 * @brief 曲线采样点
 */
typedef struct {
    double x;      /**< 采样点 X 坐标 */
    double y;      /**< 采样点 Y 坐标 */
    bool is_valid; /**< 此采样点是否有效 */
} CurveSamplePoint;
/**
 * @brief 方程曲线转换结果
 *
 * 由 formula_convert_equation_to_curve() 生成，包含采样点和元数据。
 * 调用者须通过 equation_curve_result_destroy() 销毁。
 */
typedef struct {
    bool success;             /**< 转换是否成功 */
    char error_message[256];  /**< 错误信息（成功时为空） */
    char equation_str[256];   /**< 方程的字符串表示 */
    CurveSamplePoint *points; /**< 采样点数组（malloc 分配） */
    int point_count;          /**< 采样点数量 */
    double bbox_min_x;        /**< 包围盒最小 X */
    double bbox_min_y;        /**< 包围盒最小 Y */
    double bbox_max_x;        /**< 包围盒最大 X */
    double bbox_max_y;        /**< 包围盒最大 Y */
} EquationCurveResult;
/**
 * @brief 将代数方程转换为曲线采样点
 *
 * 支持隐式方程 F(x,y) = 0 的形式。使用行进正方形算法（Marching Squares）进行采样。
 *
 * @param[in] equation_node 方程节点（类型须为 NODE_EQUATION）
 * @param[in] sample_count  采样点数量（建议 100-1000）
 * @param[in] x_min 采样区域 X 最小值
 * @param[in] x_max 采样区域 X 最大值
 * @param[in] y_min 采样区域 Y 最小值
 * @param[in] y_max 采样区域 Y 最大值
 * @return 曲线转换结果，调用者负责通过 equation_curve_result_destroy() 销毁
 */
EquationCurveResult *formula_convert_equation_to_curve(const FormulaNode *equation_node, int sample_count, double x_min,
                                                       double x_max, double y_min, double y_max);
/**
 * @brief 销毁方程曲线转换结果
 * @param[in] result 转换结果指针，可为 NULL
 */
void equation_curve_result_destroy(EquationCurveResult *result);
/**
 * @brief 将代数方程节点添加到约束图
 *
 * 创建隐式曲线表示，可用于后续求解和渲染。
 *
 * @param[in]  equation_node 方程节点（类型须为 NODE_EQUATION）
 * @param[in]  graph         目标约束图
 * @param[out] out_node_id   输出曲线节点ID
 * @return 成功返回 true，失败返回 false
 */
bool formula_convert_equation(const FormulaNode *equation_node, ConstraintGraph *graph, int *out_node_id);
#ifdef __cplusplus
}
#endif
#endif /* lv_FORMULA_CONVERTER_H */
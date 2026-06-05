/**
 * @file geo_utils.h
 * @brief 几何工具函数 —— 便捷聚合头文件
 *
 * @details 将几何相关的符号坐标操作函数聚合到一个头文件中，
 * 方便其他模块统一包含。当前聚合的模块包括：
 * - symbolic_coord.h：符号坐标类型定义与操作（比较、转换等）
 * - constraint_graph.h：约束图数据结构（几何节点、约束等）
 *
 * @version 3.5.0
 */

#ifndef LV00_GEO_UTILS_H
#define LV00_GEO_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 符号坐标操作函数（symbolic_coord_compare, symbolic_coord_to_double 等）
 * 均声明在 symbolic_coord.h 中，此处通过包含该头文件提供统一入口。
 */
#include "symbolic_coord.h"

/*
 * 约束图数据结构和几何节点类型（ConstraintGraph, GeomNode, Constraint 等）
 * 均声明在 constraint_graph.h 中。
 */
#include "constraint_graph.h"

#ifdef __cplusplus
}
#endif

#endif /* LV00_GEO_UTILS_H */

/**
 * @file node_deep_copy.h
 * @brief 几何节点深拷贝公共接口
 * @details 提供统一的节点和端口深拷贝函数，消除 engine.c、proof.c、
 *          rewrite.c 中的重复实现。
 *
 * 所有权语义说明：
 * - type_region 执行浅拷贝（指针赋值），所有权由 TypeSystem 统一管理。
 * - connected_to 指针置为 NULL，需调用者通过 ID 映射更新连接关系。
 * - symbolic_coords 执行深拷贝，所有权归新节点所有。
 */
#ifndef lv_NODE_DEEP_COPY_H
#define lv_NODE_DEEP_COPY_H
#include "lv.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief 深拷贝端口
 *
 * @param orig 原始端口指针
 * @return 新分配的端口副本，失败返回 NULL
 */
Port *node_deep_copy_port(const Port *orig);
/**
 * @brief 深拷贝几何节点
 *
 * @param orig   原始节点指针
 * @param id_map 旧节点ID到新节点ID的映射（可为 NULL）
 * @return 深拷贝后的新节点，失败返回 NULL
 */
GeomNode *node_deep_copy_geom_node(const GeomNode *orig, const int *id_map);
/**
 * @brief 深拷贝符号坐标
 *
 * @param orig 原始符号坐标指针
 * @return 新分配的坐标副本，失败返回 NULL
 */
SymbolicCoord *node_deep_copy_symbolic_coord(const SymbolicCoord *orig);
#ifdef __cplusplus
}
#endif
#endif /* lv_NODE_DEEP_COPY_H */

/**
 * @file graph_node_internal.h
 * @brief ConstraintGraph 节点/约束生命周期模块共享内部声明
 *
 * @details 供 graph_node_alloc.c / graph_node_hash.c / graph_node_conflict.c /
 *          graph_node_stub.c 共享跨模块内部函数。
 */

#ifndef lv_GRAPH_NODE_INTERNAL_H
#define lv_GRAPH_NODE_INTERNAL_H

#include "lv/constraint_graph.h"

#ifdef __cplusplus
extern "C" {
#endif

/* graph_node_alloc.c 实现，供 conflict/stub 模块共享 */
GeomNode *graph_alloc_node(ConstraintGraph *graph, GeomType type);

/* 根据 GeomType 获取对应的 vtable 指针 */
const GeomNodeVTable *get_vtable_for_type(GeomType type);

/* graph_node_hash.c 实现，供 conflict 模块回滚删除使用 */
void node_index_remove(ConstraintGraph *graph, int node_id);

/* graph_index.c 实现，供 alloc 模块反序列化复用 */
bool graph_constraint_assign_participants(Constraint *con, const int *participants, int count);

#ifdef __cplusplus
}
#endif

#endif /* lv_GRAPH_NODE_INTERNAL_H */

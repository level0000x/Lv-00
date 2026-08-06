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

/* graph_index.c 实现：按约束类型分发到 typed graph_add_*（收敛 rewrite/module 三处平行分发） */
AddConstraintResult graph_add_constraint_dispatch(ConstraintGraph *graph, ConstraintType type,
                                                  const int *participants, int count, double numeric_value);

/* graph_index.c 实现：统一节点释放路径（含内部字段与 vtable->free），
 * 供 conflict 模块回滚等释放场景复用，避免绕过统一释放路径 */
void node_destroy(GeomNode *node);

/* graph_index.c 实现：统一约束释放路径（参与者数组 + 约束外壳），
 * 与 constraint_alloc_internal（统一分配入口）对称，供删除/回滚场景复用 */
void constraint_destroy(Constraint *con);

/* graph_rank.c 实现：mpq 精确行阶梯消元核心（部分选主元 + 主元映射 + 秩），
 * 供 graph_conflict.c / graph_memory.c 冗余约束检测复用 */
int cg_mpq_row_echelon(mpq_t *matrix, int num_linear, int num_vars, int *pivot_row);

#ifdef __cplusplus
}
#endif

#endif /* lv_GRAPH_NODE_INTERNAL_H */

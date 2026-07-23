/**
 * @file solver_snapshot.h
 * @brief 求解快照/回滚 — 从 solver.c 拆分
 *
 * 在求解失败时回滚节点坐标到保存的快照状态。
 * 原位置: solver.c L108-L222
 */

#ifndef lv_SOLVER_SNAPSHOT_H
#define lv_SOLVER_SNAPSHOT_H

#include <stdbool.h>

/* 前向声明 */
struct ConstraintGraph;
struct SymbolicCoord;

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SolverSnapshot {
    int *node_ids;                 /**< 受影响的节点 ID 数组（长度 = node_count） */
    struct SymbolicCoord **copies; /**< 对应节点的坐标副本（长度 = node_count * 2，x/y 连续存放） */
    int node_count;                /**< 节点数量 */
    int coord_count;               /**< 坐标总数量（= node_count * 2） */
} SolverSnapshot;

/**
 * @brief 保存求解前的节点坐标快照
 *
 * 遍历约束图中所有几何节点，为每个节点的符号坐标创建深拷贝。
 */
bool solver_snapshot_save(const struct ConstraintGraph *graph, SolverSnapshot *snapshot);

/**
 * @brief 回滚——将节点坐标恢复到快照状态
 *
 * 在求解失败时调用，将受影响的节点坐标替换为快照中保存的原始值。
 */
void solver_snapshot_restore(struct ConstraintGraph *graph, const SolverSnapshot *snapshot);

/**
 * @brief 释放快照
 *
 * 快照不再需要时必须调用，否则会泄漏 SymbolicCoord 内存。
 * 此函数是幂等的：多次调用或对零值快照调用是安全的。
 */
void solver_snapshot_free(SolverSnapshot *snapshot);

#ifdef __cplusplus
}
#endif

#endif /* lv_SOLVER_SNAPSHOT_H */
#ifndef lv_BIT_BURNING_H
#define lv_BIT_BURNING_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <inttypes.h>
#include "lv/constraint_graph.h"
#include "lv/symbolic_coord.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BIT_CUTOFF_THRESHOLD
/** 位数熔断阈值（比特） */
#define BIT_CUTOFF_THRESHOLD 1000000
#endif

/** 连续熔断阈值（次） */
#define MAX_CONSECUTIVE_TRIPS 3

/**
 * @brief 熔断操作选择
 */
typedef enum {
    BURN_ACTION_IGNORE,     // 忽略，接受"数值辅助"节点
    BURN_ACTION_ROLLBACK,   // 回退到冻结点
    BURN_ACTION_DOWNGRADE   // 永久降级为数值假设
} BurningAction;

/**
 * @brief 位数熔断状态
 */
typedef struct {
    bool tripped;                              // 是否已触发熔断
    uint64_t bit_count;                        // 触发时的位数
    int consecutive_trips;                     // 连续触发次数
    int checkpoint_node_count;                 // 冻结点节点数
    int checkpoint_constraint_count;           // 冻结点约束数
    char reason[256];                          // 触发原因
} BitBurningState;

/**
 * @brief 检查中间结果是否超过位数阈值
 * 
 * @param num_bits 当前位数
 * @param state 输出：熔断状态
 * @return true 超过阈值，需要触发熔断
 */
bool bit_burning_check(size_t num_bits, BitBurningState *state);

/**
 * @brief 设置冻结点（在可能触发熔断的操作前调用）
 * 
 * 保存当前约束图的完整状态快照。
 * 
 * @param graph 约束图
 * @param state 熔断状态
 */
void bit_burning_set_checkpoint(ConstraintGraph *graph, BitBurningState *state);

/**
 * @brief 执行熔断操作
 * 
 * 根据连续熔断次数和用户选择执行相应操作。
 * 
 * @param graph 约束图
 * @param node_id 触发熔断的节点 ID
 * @param state 熔断状态
 * @param action 用户选择的操作
 * @return true 操作成功
 */
bool bit_burning_execute(ConstraintGraph *graph, int node_id, 
                          BitBurningState *state, BurningAction action);

/**
 * @brief 回退到冻结点
 * 
 * 恢复约束图到冻结点状态。
 * 
 * @param graph 约束图
 * @param state 熔断状态
 * @return true 回退成功
 */
bool bit_burning_rollback(ConstraintGraph *graph, BitBurningState *state);

/**
 * @brief 永久降级为数值假设
 * 
 * 将节点标记为 TRUST_AMBER，存储数值假设声明。
 * 被标记节点的下游节点自动继承 TRUST_AMBER。
 * 
 * @param graph 约束图
 * @param node_id 节点 ID
 * @param precision 数值精度阈值
 * @param declaration 声明文本
 * @return true 降级成功
 */
bool bit_burning_downgrade_to_amber(ConstraintGraph *graph, int node_id,
                                     double precision, const char *declaration);

/**
 * @brief 检查下游传播是否被阻断
 * 
 * 当一个"数值辅助"节点被后续构造引用时，
 * 检查是否可以安全传播。
 * 
 * @param graph 约束图
 * @param source_node_id 源节点 ID
 * @param target_node_id 目标节点 ID  
 * @return true 传播被阻断，false 可以安全传播
 */
bool bit_burning_is_blocked(ConstraintGraph *graph, int source_node_id, int target_node_id);

/**
 * @brief 永久降级的自动传播
 * 
 * 当一个节点被降级为 TRUST_AMBER 后，
 * 所有依赖它的下游节点自动继承 TRUST_AMBER。
 * 
 * @param graph 约束图
 * @param node_id 已降级节点 ID
 */
void bit_burning_propagate_downgrade(ConstraintGraph *graph, int node_id);

/**
 * @brief 获取线程局部全局熔断状态
 *
 * @return BitBurningState* 指向全局熔断状态的指针
 */
BitBurningState *bit_burning_get_global_state(void);

#ifdef __cplusplus
}
#endif

#endif /* lv_BIT_BURNING_H */

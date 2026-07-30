/**
 * @file lambda_to_graph.h
 * @brief λ-项到约束图的编译和反向转换（Public API）
 *
 * 根据 Lv-00 设计文档 8.1 节，λ-演算的几何编码将 λ-项编译为
 * 约束图中的函数块：
 * - λx.M = 标准 Lv-00 函数块，输入端口对应 x，内部子图为 M
 * - 函数应用 = 函数块的输入端口连接到实参的输出端口
 * - α-等价 = 端口连接图相同，自然满足
 */

#ifndef lv_LAMBDA_TO_GRAPH_H
#define lv_LAMBDA_TO_GRAPH_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv/constraint_graph.h"
#include "lv/lambda_term.h"

/**
 * @brief 将 λ-项编译为约束图
 *
 * 递归遍历 λ-项，为每个节点创建相应的几何构造：
 * - LV_LAMBDA_VAR(index): 创建一个带有输入端口的几何节点
 * - LV_LAMBDA_ABS(binder, body): 创建一个函数块，其输入端口对应 binder
 * - LV_LAMBDA_APP(left, right): 将 left 的输出端口连接到 right 的输入端口
 *
 * @param term λ-项
 * @param graph 目标约束图
 * @param out_node_id 输出：根节点 ID
 * @return true 编译成功
 */
lv_PUBLIC_API bool lambda_to_graph(LvLambdaTerm *term, ConstraintGraph *graph, int *out_node_id);

/**
 * @brief 将约束图函数块还原为 λ-项
 *
 * 逆操作：从函数块节点的内部结构重建 λ-项。
 * 用于验证编译和 β-归约的正确性。
 *
 * @param graph 约束图
 * @param node_id 根函数块节点 ID
 * @return 还原的 λ-项（调用者负责销毁），失败返回 NULL
 */
lv_PUBLIC_API LvLambdaTerm *graph_to_lambda(ConstraintGraph *graph, int node_id);

/**
 * @brief 在约束图上执行一次 β-归约
 *
 * 在约束图中搜索可归约的函数块应用模式（函数块 + 实参 + 输出端口），
 * 匹配成功后执行端口继承规则的图变换操作。
 * 每调用一次最多执行一个 β-归约。
 *
 * @param graph 约束图
 * @return true 成功执行一次 β-归约，false 无匹配模式或执行失败
 */
lv_PUBLIC_API bool beta_reduce(ConstraintGraph *graph);

/**
 * @brief 在约束图上执行最多 n 步 β-归约
 *
 * 重复调用 beta_reduce，最多执行 n 步。
 * 适用于有上限约束的迭代归约场景。
 *
 * @param graph 约束图
 * @param n     最大归约步数（<=0 时立即返回 0）
 * @return 实际执行的归约步数（0 表示无可归约模式）
 */
lv_PUBLIC_API int beta_reduce_n(ConstraintGraph *graph, int n);

/**
 * @brief 在约束图上反复 β-归约至不动点
 *
 * 持续调用 beta_reduce 直到无可归约模式。
 * 内置 5000 步安全边界防止无限循环。
 *
 * @param graph 约束图
 * @return 实际执行的归约步数
 */
lv_PUBLIC_API int beta_reduce_fully(ConstraintGraph *graph);

#ifdef __cplusplus
}
#endif

#endif /* lv_LAMBDA_TO_GRAPH_H */

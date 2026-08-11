/**
 * @file determinism_state.h
 * @brief 确定性状态枚举定义
 *
 * 将 DeterminismState 枚举定义独立出来，解决 constraint_graph.h 和 func_block.h 之间的循环依赖问题。
 */
#ifndef lv_DETERMINISM_STATE_H
#define lv_DETERMINISM_STATE_H
#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief 确定性状态枚举
 *
 * 用于表示函数块的确定性验证状态。
 * 独立定义以避免 constraint_graph.h 和 func_block.h 之间的循环依赖。
 */
typedef enum {
    DETERMINISM_STATE_UNVERIFIED,        /**< 未验证 —— 尚未进行确定性分析 */
    DETERMINISM_STATE_VERIFIED,          /**< 已验证 —— 函数块的行为是确定性的 */
    DETERMINISM_STATE_NON_DETERMINISTIC, /**< 非确定性 —— 函数块可能产生不同输出 */
    DETERMINISM_STATE_PARTIALLY_VERIFIED /**< 部分验证 —— 仅部分路径已验证确定性 */
} DeterminismState;

/**
 * @brief 确定性状态单一事实源 X 宏（枚举名 + JSON 显示名）
 * 名称表（func_block.c / graph_node_alloc.c）均由本宏生成，防止双份维护失步。
 */
#define LV_DETERMINISM_STATE_ENTRY(x)                                     \
    x(DETERMINISM_STATE_UNVERIFIED, "UNVERIFIED")                         \
    x(DETERMINISM_STATE_VERIFIED, "VERIFIED")                             \
    x(DETERMINISM_STATE_NON_DETERMINISTIC, "NON_DETERMINISTIC")           \
    x(DETERMINISM_STATE_PARTIALLY_VERIFIED, "PARTIALLY_VERIFIED")
#ifdef __cplusplus
}
#endif
#endif /* lv_DETERMINISM_STATE_H */

#ifndef lv_INTEROP_STEP_TYPE_H
#define lv_INTEROP_STEP_TYPE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Lv-00 证明步骤类型枚举（Lean 4 / OPML 共用单源）
 *
 * 契约卡：
 * - 语义契约：Lv-00 内部证明步骤的规范类型集合（9 值）；不表达任何具体
 *   证明器的 tactic 名（tactic 名见各桥接文件的 tactic_map / reverse_map）。
 * - 前置条件：无。
 * - 失败/截断语义：不适用（纯枚举类型）。
 * - 边界行为：未知整数值由各调用点的 lv_enum_to_str / lv_str_to_enum 以
 *   默认值处理。
 * - 扩展点：新增步骤类型仅需在此枚举追加一项，Lean 4 与 OPML 立即同步。
 *
 * Coq 的 tactic 集合与 Lean 4 不同（含 UNIFY/EX_FALSO，缺 EXACT/HAVE/CALC，
 * 且 NORMALIZATION 数值分叉），属互操作外部契约，在 coq_bridge.c 以本地
 * 枚举单独维护并标注 exempt，禁止与本单源合并。
 */
typedef enum {
    lv_STEP_ADD_NODE = 0,   /**< 添加节点 */
    lv_STEP_ADD_CONSTRAINT, /**< 添加约束 */
    lv_STEP_REWRITE,        /**< 重写 */
    lv_STEP_FUNCTION_APP,   /**< 函数应用 */
    lv_STEP_EXACT,          /**< 精确匹配 */
    lv_STEP_HAVE,           /**< 中间引理 */
    lv_STEP_CALC,           /**< 计算链 */
    lv_STEP_NORMALIZATION,  /**< 规范化 */
    lv_STEP_ORACLE          /**< 外部预言 */
} lvProofStepType;

#ifdef __cplusplus
}
#endif

#endif /* lv_INTEROP_STEP_TYPE_H */

#ifndef lv_INTEROP_BRIDGE_COMMON_H
#define lv_INTEROP_BRIDGE_COMMON_H

#include "lv/lv_utils.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 证明步骤结构体（Coq / Lean 4 桥接共用）
 *
 * 表示证明中的单个步骤，包含类型、描述文本和序号。
 */
typedef struct {
    int type;              /**< 步骤类型 */
    char description[512]; /**< 步骤描述（tactic 名称） */
    int id;                /**< 步骤编号（按导入顺序） */
} lvProofStep;

/**
 * @brief 桥接证明结构体（Coq / Lean 4 共用）
 *
 * 用于证明脚本的导入/导出中间表示。
 * 包含定理名称和动态增长的步骤数组。
 */
typedef struct {
    char theorem_name[256]; /**< 定理名称 */
    lvDArray steps_da;      /**< 步骤动态数组 */
} lvBridgeProof;

#ifdef __cplusplus
}
#endif

#endif /* lv_INTEROP_BRIDGE_COMMON_H */

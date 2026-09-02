/**
 * @file trust_color.h
 * @brief 信任颜色映射系统 —— TrustColor ↔ ProofColor 双向映射
 *
 * 信任颜色系统是 Lv-00 的核心概念之一，用于标记几何对象和证明步骤的信任级别。
 * 三个颜色枚举服务于不同层次：
 *   - TrustColor（layer3）：几何坐标/节点的信任状态
 *   - ProofColor（layer4）：证明步骤的颜色标记
 *   - lvTrustColor（layer5）：UI 协议传输协议颜色
 *
 * 本模块提供 TrustColor 与 ProofColor 之间的双向映射（Gap 1），
 * 以及对应的颜色名称字符串。
 *
 * 架构位置：Layer 4（公理推理层）
 *   依赖：symbolic_coord.h（Layer 3, TrustColor）
 *          proof.h（Layer 4, ProofColor）
 *   被依赖：engine.c（Layer 4）, proof_navigator.c（Layer 4）
 *
 * @author Lv-00 Project
 */
#ifndef lv_TRUST_COLOR_H
#define lv_TRUST_COLOR_H

#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */
#include "lv/proof.h"          /* ProofColor */
#include "lv/symbolic_coord.h" /* TrustColor */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 将 TrustColor 映射为等价的 ProofColor
 *
 * 映射规则：
 *   TRUST_GREEN                  → PROOF_COLOR_GREEN
 *   TRUST_BLUE_UNEXPLORED        → PROOF_COLOR_BLUE_UNEXPLORED
 *   TRUST_BLUE_EXCEEDED          → PROOF_COLOR_BLUE_RESOURCE
 *   TRUST_BLUE_OUT_OF_SCOPE      → PROOF_COLOR_BLUE_OUT_OF_RANGE
 *   TRUST_YELLOW                 → PROOF_COLOR_YELLOW
 *   TRUST_LIGHT_ORANGE_ORACLE    → PROOF_COLOR_ORANGE_ORACLE
 *   TRUST_LIGHT_ORANGE_EXPLOSION → PROOF_COLOR_ORANGE_EX_FALSO
 *   TRUST_AMBER                  → PROOF_COLOR_AMBER
 *   TRUST_DEEP_ORANGE            → PROOF_COLOR_DARK_ORANGE
 *   TRUST_RED                    → PROOF_COLOR_RED_CONFLICT
 *   越界                         → PROOF_COLOR_BLUE_UNEXPLORED（保守回退）
 *
 * @param tc TrustColor 枚举值
 * @return 对应的 ProofColor 枚举值
 */
ProofColor trust_color_to_proof(TrustColor tc);

/**
 * @brief 将 ProofColor 映射为等价的 TrustColor
 *
 * 映射规则：
 *   所有 ProofColor 值映射到语义最接近的 TrustColor。
 *   PROOF_COLOR_GREEN_COMPLETE / PROOF_COLOR_GREEN_VERIFIED 回退到 TRUST_GREEN。
 *
 * @param pc ProofColor 枚举值
 * @return 对应的 TrustColor 枚举值
 */
TrustColor proof_color_to_trust(ProofColor pc);

/**
 * @brief 获取 TrustColor 的人类可读名称
 *
 * @param tc TrustColor 枚举值
 * @return 静态字符串（如 "Green", "Blue (unexplored)"），越界返回 "Unknown"
 */
lv_PUBLIC_API const char *trust_color_name(TrustColor tc);

/**
 * @brief 获取 ProofColor 的人类可读名称
 *
 * @param pc ProofColor 枚举值
 * @return 静态字符串（如 "Green", "Orange (ex falso)"），越界返回 "Unknown"
 */
lv_PUBLIC_API const char *proof_color_name(ProofColor pc);

/**
 * @brief 获取 ProofColor 对应的 HTML 十六进制颜色
 *
 * 统一维护 UI/导出层的颜色呈现（原 proof_navigator_export.c 私有表收敛于此）。
 *
 * @param pc ProofColor 枚举值
 * @return 静态字符串（如 "#4CAF50"），越界返回默认灰 "#78909C"
 */
lv_PUBLIC_API const char *proof_color_to_html_hex(ProofColor pc);

/**
 * @brief 合并两个 ProofColor（语义等价于 trust_color_combine）
 *
 * 使用与 TrustColor 相同的叠加规则：
 *   LO + AMBER = DARK_ORANGE
 *   其余取较高值
 *
 * @param a 第一个 ProofColor
 * @param b 第二个 ProofColor
 * @return 合并后的 ProofColor
 */
ProofColor proof_color_combine(ProofColor a, ProofColor b);

#ifdef __cplusplus
}
#endif

#endif /* lv_TRUST_COLOR_H */

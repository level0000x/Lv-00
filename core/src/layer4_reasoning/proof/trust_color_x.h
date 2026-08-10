/**
 * @file trust_color_x.h
 * @brief TrustColor / ProofColor 元数据 X-macro 主源列表
 *
 * 收敛 TrustColor 与 ProofColor 分散在下游模块的元数据（显示名、
 * 序列化名、DOT/HTML 十六进制色、LaTeX/Coq 输出标识）到单一列表。
 * 两个列表均按枚举值升序排列，各消费方用局部展开宏生成数据表：
 *   - LV_TRUST_COLOR_X(X)：5 列 (symbol, display_name, serial_name, dot_hex, latex)
 *   - LV_PROOF_COLOR_X(X)：4 列 (symbol, display_name, html_hex, coq_name)
 *
 * @author Lv-00 Project
 */

#ifndef lv_TRUST_COLOR_X_H
#define lv_TRUST_COLOR_X_H

/* symbolic_coord.h 遗留的 2 列同名定义（仅用于生成 TrustColor 枚举）
 * 已被本主源 5 列列表取代，先清空旧定义避免宏重定义告警。 */
#ifdef LV_TRUST_COLOR_X
#undef LV_TRUST_COLOR_X
#endif

/**
 * @brief TrustColor 元数据 X-macro 主源列表（5 列，按枚举值升序）
 *
 * 列顺序：(symbol, display_name, serial_name, dot_hex, latex)
 */
#define LV_TRUST_COLOR_X(X) \
    X(TRUST_GREEN, "Green", "GREEN", "#2ca02c", "\\textcolor{green}{}") \
    X(TRUST_BLUE_UNEXPLORED, "Blue (unexplored)", "BLUE_UNEXPLORED", "#1f77b4", "\\textcolor{blue}{}") \
    X(TRUST_BLUE_EXCEEDED, "Blue (exceeded)", "BLUE_EXCEEDED", "#1f77b4", "\\textcolor{blue}{}") \
    X(TRUST_BLUE_OUT_OF_SCOPE, "Blue (out of scope)", "BLUE_OUT_OF_SCOPE", "#1f77b4", "\\textcolor{blue}{}") \
    X(TRUST_YELLOW, "Yellow", "YELLOW", "#bcbd22", "\\textcolor{yellow}{}") \
    X(TRUST_LIGHT_ORANGE_ORACLE, "Light orange (oracle)", "LIGHT_ORANGE_ORACLE", "#ff7f0e", "\\textcolor{orange!70}{}") \
    X(TRUST_LIGHT_ORANGE_EXPLOSION, "Light orange (ex falso)", "LIGHT_ORANGE_EXPLOSION", "#ff7f0e", "\\textcolor{orange!70}{}") \
    X(TRUST_AMBER, "Amber", "AMBER", "#ffbf00", "\\textcolor{orange!50}{}") \
    X(TRUST_DEEP_ORANGE, "Deep orange", "DEEP_ORANGE", "#ff4500", "\\textcolor{orange}{}") \
    X(TRUST_RED, "Red", "RED", "#d62728", "\\textcolor{red}{}")

/**
 * @brief ProofColor 元数据 X-macro 主源列表（4 列，按枚举值升序）
 *
 * 列顺序：(symbol, display_name, html_hex, coq_name)
 */
#define LV_PROOF_COLOR_X(X) \
    X(PROOF_COLOR_GREEN, "Green (fully constructed)", "#4CAF50", "GREEN") \
    X(PROOF_COLOR_BLUE_UNEXPLORED, "Blue (unexplored)", "#2196F3", "BLUE_UNEXPLORED") \
    X(PROOF_COLOR_BLUE_RESOURCE, "Blue (resource limited)", "#1976D2", "BLUE_RESOURCE") \
    X(PROOF_COLOR_BLUE_OUT_OF_RANGE, "Blue (out of range)", "#0D47A1", "BLUE_OUT_OF_RANGE") \
    X(PROOF_COLOR_GREEN_VERIFIED, "Green (verified unconstructible)", "#2E7D32", "GREEN_VERIFIED") \
    X(PROOF_COLOR_YELLOW, "Yellow", "#FFC107", "YELLOW") \
    X(PROOF_COLOR_ORANGE_ORACLE, "Orange (oracle)", "#FF9800", "ORANGE_ORACLE") \
    X(PROOF_COLOR_ORANGE_EX_FALSO, "Orange (ex falso)", "#F57C00", "ORANGE_EX_FALSO") \
    X(PROOF_COLOR_AMBER, "Amber", "#FFB300", "AMBER") \
    X(PROOF_COLOR_DARK_ORANGE, "Dark orange", "#E65100", "DARK_ORANGE") \
    X(PROOF_COLOR_GREEN_COMPLETE, "Green (complete)", "#1B5E20", "GREEN_COMPLETE") \
    X(PROOF_COLOR_RED_CONFLICT, "Red (conflict)", "#D32F2F", "RED_CONFLICT")

#endif /* lv_TRUST_COLOR_X_H */

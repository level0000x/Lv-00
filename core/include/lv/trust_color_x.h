/**
 * @file trust_color_x.h
 * @brief TrustColor / ProofColor 元数据 X-macro 单一事实源列表（公共头）
 *
 * 收敛 TrustColor 与 ProofColor 的枚举定义与下游元数据（显示名、
 * 序列化名、DOT/HTML 十六进制色、LaTeX/Coq 输出标识）到单一列表，
 * 消除 symbolic_coord.h 遗留的 2 列同名定义与 proof.h 手写枚举的平行维护。
 *
 * 两个列表均按枚举值升序排列，枚举定义与各消费方均从本列表生成：
 *   - LV_TRUST_COLOR_X(X)：5 列 (symbol, display_name, serial_name, dot_hex, latex)
 *   - LV_PROOF_COLOR_X(X)：4 列 (symbol, display_name, html_hex, coq_name)
 *
 * 消费方用局部展开宏生成枚举或数据表，例如：
 *   #define LV_TRUST_ENUM_ITEM(sym, disp, ser, dot, tex) sym,
 *   typedef enum { LV_TRUST_COLOR_X(LV_TRUST_ENUM_ITEM) } TrustColor;
 *   #undef LV_TRUST_ENUM_ITEM
 *
 * @author Lv-00 Project
 */

#ifndef lv_TRUST_COLOR_X_H
#define lv_TRUST_COLOR_X_H

/**
 * @brief TrustColor 元数据 X-macro 主源列表（5 列，按枚举值升序）
 *
 * 颜色含义（顺序即枚举值 0-9）：
 *   TRUST_GREEN                  全构造
 *   TRUST_BLUE_UNEXPLORED        未探索
 *   TRUST_BLUE_EXCEEDED          资源受限
 *   TRUST_BLUE_OUT_OF_SCOPE      超出范围
 *   TRUST_YELLOW                 条件性不可构造
 *   TRUST_LIGHT_ORANGE_ORACLE    非构造性 oracle（实心端口）
 *   TRUST_LIGHT_ORANGE_EXPLOSION 爆炸原理（虚线箭头）
 *   TRUST_AMBER                  数值假设
 *   TRUST_DEEP_ORANGE            叠加（非构造性+数值假设）
 *   TRUST_RED                    矛盾 / 验证伪
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
 * 颜色含义（顺序即枚举值 0-11）：
 *   PROOF_COLOR_GREEN               全构造，无任何非常规依赖
 *   PROOF_COLOR_BLUE_UNEXPLORED     蓝色（未探索）
 *   PROOF_COLOR_BLUE_RESOURCE       蓝色（资源受限）
 *   PROOF_COLOR_BLUE_OUT_OF_RANGE   蓝色（超出范围）
 *   PROOF_COLOR_GREEN_VERIFIED      绿色实框：已证不可构造
 *   PROOF_COLOR_YELLOW              黄色虚线框：条件性不可构造
 *   PROOF_COLOR_ORANGE_ORACLE       浅橙色实心端口：依赖非构造性oracle
 *   PROOF_COLOR_ORANGE_EX_FALSO     浅橙色虚线箭头：爆炸原理步骤
 *   PROOF_COLOR_AMBER               橙黄色：含数值假设
 *   PROOF_COLOR_DARK_ORANGE         深橙色：非构造性依赖与数值假设叠加
 *   PROOF_COLOR_GREEN_COMPLETE      绿色：证明完成
 *   PROOF_COLOR_RED_CONFLICT        红色：冲突/矛盾
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

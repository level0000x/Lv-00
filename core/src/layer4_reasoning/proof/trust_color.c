/**
 * @file trust_color.c
 * @brief 信任颜色映射系统实现
 *
 * 实现 TrustColor ↔ ProofColor 双向映射及颜色合并逻辑。
 * 分为三组 API：
 *   1. 映射函数（trust_color_to_proof / proof_color_to_trust）
 *   2. 名称函数（trust_color_name / proof_color_name）
 *   3. 合并函数（proof_color_combine）
 *
 * @author Lv-00 Project
 */
#include "lv/trust_color.h"

/* ================================================================
 * 映射函数：TrustColor ↔ ProofColor
 * ================================================================ */

ProofColor trust_color_to_proof(TrustColor tc) {
    switch (tc) {
        case TRUST_GREEN:
            return PROOF_COLOR_GREEN;
        case TRUST_BLUE_UNEXPLORED:
            return PROOF_COLOR_BLUE_UNEXPLORED;
        case TRUST_BLUE_EXCEEDED:
            return PROOF_COLOR_BLUE_RESOURCE;
        case TRUST_BLUE_OUT_OF_SCOPE:
            return PROOF_COLOR_BLUE_OUT_OF_RANGE;
        case TRUST_YELLOW:
            return PROOF_COLOR_YELLOW;
        case TRUST_LIGHT_ORANGE_ORACLE:
            return PROOF_COLOR_ORANGE_ORACLE;
        case TRUST_LIGHT_ORANGE_EXPLOSION:
            return PROOF_COLOR_ORANGE_EX_FALSO;
        case TRUST_AMBER:
            return PROOF_COLOR_AMBER;
        case TRUST_DEEP_ORANGE:
            return PROOF_COLOR_DARK_ORANGE;
        case TRUST_RED:
            return PROOF_COLOR_RED_CONFLICT;
        default:
            /* 越界值保守回退为未探索蓝 */
            return PROOF_COLOR_BLUE_UNEXPLORED;
    }
}

TrustColor proof_color_to_trust(ProofColor pc) {
    switch (pc) {
        case PROOF_COLOR_GREEN:
            return TRUST_GREEN;
        case PROOF_COLOR_BLUE_UNEXPLORED:
            return TRUST_BLUE_UNEXPLORED;
        case PROOF_COLOR_BLUE_RESOURCE:
            return TRUST_BLUE_EXCEEDED;
        case PROOF_COLOR_BLUE_OUT_OF_RANGE:
            return TRUST_BLUE_OUT_OF_SCOPE;
        case PROOF_COLOR_GREEN_VERIFIED:
            /* 已验证的不可构造 → 归为绿色（全构造语义上的"已验证"） */
            return TRUST_GREEN;
        case PROOF_COLOR_YELLOW:
            return TRUST_YELLOW;
        case PROOF_COLOR_ORANGE_ORACLE:
            return TRUST_LIGHT_ORANGE_ORACLE;
        case PROOF_COLOR_ORANGE_EX_FALSO:
            return TRUST_LIGHT_ORANGE_EXPLOSION;
        case PROOF_COLOR_AMBER:
            return TRUST_AMBER;
        case PROOF_COLOR_DARK_ORANGE:
            return TRUST_DEEP_ORANGE;
        case PROOF_COLOR_GREEN_COMPLETE:
            /* 证明完成 → 归为绿色 */
            return TRUST_GREEN;
        case PROOF_COLOR_RED_CONFLICT:
            return TRUST_RED;
        default:
            /* 越界值保守回退为未探索蓝 */
            return TRUST_BLUE_UNEXPLORED;
    }
}

/* ================================================================
 * 名称函数
 * ================================================================ */

const char *trust_color_name(TrustColor tc) {
    switch (tc) {
        case TRUST_GREEN:
            return "Green";
        case TRUST_BLUE_UNEXPLORED:
            return "Blue (unexplored)";
        case TRUST_BLUE_EXCEEDED:
            return "Blue (exceeded)";
        case TRUST_BLUE_OUT_OF_SCOPE:
            return "Blue (out of scope)";
        case TRUST_YELLOW:
            return "Yellow";
        case TRUST_LIGHT_ORANGE_ORACLE:
            return "Light orange (oracle)";
        case TRUST_LIGHT_ORANGE_EXPLOSION:
            return "Light orange (ex falso)";
        case TRUST_AMBER:
            return "Amber";
        case TRUST_DEEP_ORANGE:
            return "Deep orange";
        case TRUST_RED:
            return "Red";
        default:
            return "Unknown";
    }
}

const char *proof_color_name(ProofColor pc) {
    switch (pc) {
        case PROOF_COLOR_GREEN:
            return "Green (fully constructed)";
        case PROOF_COLOR_BLUE_UNEXPLORED:
            return "Blue (unexplored)";
        case PROOF_COLOR_BLUE_RESOURCE:
            return "Blue (resource limited)";
        case PROOF_COLOR_BLUE_OUT_OF_RANGE:
            return "Blue (out of range)";
        case PROOF_COLOR_GREEN_VERIFIED:
            return "Green (verified unconstructible)";
        case PROOF_COLOR_YELLOW:
            return "Yellow";
        case PROOF_COLOR_ORANGE_ORACLE:
            return "Orange (oracle)";
        case PROOF_COLOR_ORANGE_EX_FALSO:
            return "Orange (ex falso)";
        case PROOF_COLOR_AMBER:
            return "Amber";
        case PROOF_COLOR_DARK_ORANGE:
            return "Dark orange";
        case PROOF_COLOR_GREEN_COMPLETE:
            return "Green (complete)";
        case PROOF_COLOR_RED_CONFLICT:
            return "Red (conflict)";
        default:
            return "Unknown";
    }
}

/* ================================================================
 * 合并函数
 * ================================================================ */

ProofColor proof_color_combine(ProofColor a, ProofColor b) {
    /*
     * 使用与 trust_color_combine 相同的叠加规则：
     *   1. 如果一方是浅橙色（ORACLE / EX_FALSO），另一方是琥珀色，
     *      结果为深橙色（非构造性 + 数值假设叠加）。
     *   2. 否则，取枚举值较大者（即信任级别较低者）。
     */

    int a_val = (int) a;
    int b_val = (int) b;

    /* 判断是否为"浅橙色"（PROOF_COLOR_ORANGE_ORACLE 或 PROOF_COLOR_ORANGE_EX_FALSO） */
    int is_a_lo = (a == PROOF_COLOR_ORANGE_ORACLE || a == PROOF_COLOR_ORANGE_EX_FALSO);
    int is_b_lo = (b == PROOF_COLOR_ORANGE_ORACLE || b == PROOF_COLOR_ORANGE_EX_FALSO);

    /* 浅橙色 + 琥珀色 = 深橙色 */
    if ((is_a_lo && b == PROOF_COLOR_AMBER) || (is_b_lo && a == PROOF_COLOR_AMBER)) {
        return PROOF_COLOR_DARK_ORANGE;
    }

    /* 取较高值（数字越大，信任级别越低） */
    return (a_val > b_val) ? a : b;
}

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

/* ================================================================
 * 枚举 -> 名称 映射表（数据表化，替代 switch）
 * ================================================================ */

/** @brief 枚举值 -> 名称 映射项（表必须按 code 升序排列） */
typedef struct {
    int code;         /**< 枚举值 */
    const char *name; /**< 名称字符串 */
} trust_color_NameEntry;

/** @brief 二分查找枚举名称（表需按 code 升序） */
static const char *trust_color_name_lookup(const trust_color_NameEntry *table, size_t count, int code) {
    size_t lo = 0, hi = count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (table[mid].code == code)
            return table[mid].name;
        if (table[mid].code < code)
            lo = mid + 1;
        else
            hi = mid;
    }
    return NULL;
}

/** @brief trust_color_name 名称表（按枚举值升序） */
static const trust_color_NameEntry s_trust_color_name_entries[] = {
    {TRUST_GREEN, "Green"},
    {TRUST_BLUE_UNEXPLORED, "Blue (unexplored)"},
    {TRUST_BLUE_EXCEEDED, "Blue (exceeded)"},
    {TRUST_BLUE_OUT_OF_SCOPE, "Blue (out of scope)"},
    {TRUST_YELLOW, "Yellow"},
    {TRUST_LIGHT_ORANGE_ORACLE, "Light orange (oracle)"},
    {TRUST_LIGHT_ORANGE_EXPLOSION, "Light orange (ex falso)"},
    {TRUST_AMBER, "Amber"},
    {TRUST_DEEP_ORANGE, "Deep orange"},
    {TRUST_RED, "Red"},
};

const char *trust_color_name(TrustColor tc) {
    const char *name = trust_color_name_lookup(s_trust_color_name_entries, lv_ARRAY_SIZE(s_trust_color_name_entries), (int) tc);
    return name ? name : "Unknown";
}

/** @brief proof_color_name 名称表（按枚举值升序） */
static const trust_color_NameEntry s_proof_color_name_entries[] = {
    {PROOF_COLOR_GREEN, "Green (fully constructed)"},
    {PROOF_COLOR_BLUE_UNEXPLORED, "Blue (unexplored)"},
    {PROOF_COLOR_BLUE_RESOURCE, "Blue (resource limited)"},
    {PROOF_COLOR_BLUE_OUT_OF_RANGE, "Blue (out of range)"},
    {PROOF_COLOR_GREEN_VERIFIED, "Green (verified unconstructible)"},
    {PROOF_COLOR_YELLOW, "Yellow"},
    {PROOF_COLOR_ORANGE_ORACLE, "Orange (oracle)"},
    {PROOF_COLOR_ORANGE_EX_FALSO, "Orange (ex falso)"},
    {PROOF_COLOR_AMBER, "Amber"},
    {PROOF_COLOR_DARK_ORANGE, "Dark orange"},
    {PROOF_COLOR_GREEN_COMPLETE, "Green (complete)"},
    {PROOF_COLOR_RED_CONFLICT, "Red (conflict)"},
};

const char *proof_color_name(ProofColor pc) {
    const char *name = trust_color_name_lookup(s_proof_color_name_entries, lv_ARRAY_SIZE(s_proof_color_name_entries), (int) pc);
    return name ? name : "Unknown";
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

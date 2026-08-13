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
#include "lv/lv_xmacro.h"
#include "lv/trust_color_x.h"

/* ================================================================
 * 映射函数：TrustColor ↔ ProofColor
 * ================================================================ */

/* ================================================================
 * 枚举 -> 颜色 映射表（数据表化，替代 switch）
 * ================================================================ */

/** @brief TrustColor → ProofColor 映射表（按 TrustColor 枚举值升序） */
static const ProofColor s_trust_to_proof_colors[] = {
    [TRUST_GREEN]                  = PROOF_COLOR_GREEN,
    [TRUST_BLUE_UNEXPLORED]        = PROOF_COLOR_BLUE_UNEXPLORED,
    [TRUST_BLUE_EXCEEDED]          = PROOF_COLOR_BLUE_RESOURCE,
    [TRUST_BLUE_OUT_OF_SCOPE]      = PROOF_COLOR_BLUE_OUT_OF_RANGE,
    [TRUST_YELLOW]                 = PROOF_COLOR_YELLOW,
    [TRUST_LIGHT_ORANGE_ORACLE]    = PROOF_COLOR_ORANGE_ORACLE,
    [TRUST_LIGHT_ORANGE_EXPLOSION] = PROOF_COLOR_ORANGE_EX_FALSO,
    [TRUST_AMBER]                  = PROOF_COLOR_AMBER,
    [TRUST_DEEP_ORANGE]            = PROOF_COLOR_DARK_ORANGE,
    [TRUST_RED]                    = PROOF_COLOR_RED_CONFLICT,
};

ProofColor trust_color_to_proof(TrustColor tc) {
    if ((unsigned) tc < lv_ARRAY_SIZE(s_trust_to_proof_colors))
        return s_trust_to_proof_colors[tc];
    /* 越界值保守回退为未探索蓝 */
    return PROOF_COLOR_BLUE_UNEXPLORED;
}

/** @brief ProofColor → TrustColor 映射表（按 ProofColor 枚举值升序） */
static const TrustColor s_proof_to_trust_colors[] = {
    [PROOF_COLOR_GREEN]            = TRUST_GREEN,
    [PROOF_COLOR_BLUE_UNEXPLORED]  = TRUST_BLUE_UNEXPLORED,
    [PROOF_COLOR_BLUE_RESOURCE]    = TRUST_BLUE_EXCEEDED,
    [PROOF_COLOR_BLUE_OUT_OF_RANGE] = TRUST_BLUE_OUT_OF_SCOPE,
    [PROOF_COLOR_GREEN_VERIFIED]   = TRUST_GREEN, /* 已验证的不可构造 → 归为绿色（全构造语义上的"已验证"） */
    [PROOF_COLOR_YELLOW]           = TRUST_YELLOW,
    [PROOF_COLOR_ORANGE_ORACLE]    = TRUST_LIGHT_ORANGE_ORACLE,
    [PROOF_COLOR_ORANGE_EX_FALSO]  = TRUST_LIGHT_ORANGE_EXPLOSION,
    [PROOF_COLOR_AMBER]            = TRUST_AMBER,
    [PROOF_COLOR_DARK_ORANGE]      = TRUST_DEEP_ORANGE,
    [PROOF_COLOR_GREEN_COMPLETE]   = TRUST_GREEN, /* 证明完成 → 归为绿色 */
    [PROOF_COLOR_RED_CONFLICT]     = TRUST_RED,
};

TrustColor proof_color_to_trust(ProofColor pc) {
    if ((unsigned) pc < lv_ARRAY_SIZE(s_proof_to_trust_colors))
        return s_proof_to_trust_colors[pc];
    /* 越界值保守回退为未探索蓝 */
    return TRUST_BLUE_UNEXPLORED;
}

/* ================================================================
 * 名称函数
 * ================================================================ */

/* ================================================================
 * 枚举 -> 名称 映射表（数据表化，替代 switch）
 * ================================================================ */

/** @brief trust_color_name 名称表（按枚举值升序，自 LV_TRUST_COLOR_X 生成） */
#define LV_TRUST_COLOR_TO_ENTRY(sym, disp, ser, dot, tex) {disp, sym},
static const lvStrToEnumEntry s_trust_color_name_entries[] = {
    LV_TRUST_COLOR_X(LV_TRUST_COLOR_TO_ENTRY)
};
#undef LV_TRUST_COLOR_TO_ENTRY

const char *trust_color_name(TrustColor tc) {
    return lv_enum_to_str(s_trust_color_name_entries, lv_ARRAY_SIZE(s_trust_color_name_entries), (int) tc, "Unknown");
}

/** @brief proof_color_name 名称表（按枚举值升序，自 LV_PROOF_COLOR_X 生成） */
#define LV_PROOF_COLOR_TO_ENTRY(sym, disp, hex, coq) {disp, sym},
static const lvStrToEnumEntry s_proof_color_name_entries[] = {
    LV_PROOF_COLOR_X(LV_PROOF_COLOR_TO_ENTRY)
};
#undef LV_PROOF_COLOR_TO_ENTRY

const char *proof_color_name(ProofColor pc) {
    return lv_enum_to_str(s_proof_color_name_entries, lv_ARRAY_SIZE(s_proof_color_name_entries), (int) pc, "Unknown");
}

/** @brief proof_color_to_html_hex 名称表（按枚举值升序，自 LV_PROOF_COLOR_X 生成） */
#define LV_PROOF_COLOR_TO_HTML(sym, disp, hex, coq) {hex, sym},
static const lvStrToEnumEntry s_proof_color_html_names[] = {
    LV_PROOF_COLOR_X(LV_PROOF_COLOR_TO_HTML)
};
#undef LV_PROOF_COLOR_TO_HTML

const char *proof_color_to_html_hex(ProofColor pc) {
    return lv_enum_to_str(s_proof_color_html_names, lv_ARRAY_SIZE(s_proof_color_html_names), (int) pc, "#78909C");
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

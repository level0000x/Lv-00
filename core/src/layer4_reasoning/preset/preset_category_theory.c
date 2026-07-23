/**
 * @file preset_category_theory.c
 * @brief 范畴论预设函数块 - 实现
 *
 * 实现理论数学研究中常用的范畴论运算预设函数块。
 * 涵盖范畴基本概念（恒等态射、复合、同构）、函子作用、
 * 自然变换、极限与余极限（积、余积、拉回、推出、等化子、余等化子）、
 * 泛性质（指数对象、初始对象、终止对象、伴随函子）。
 * 所有预设函数块都遵循模块化、确定性原则。
 *
 * @module CategoryTheory
 * @category PRESET_CATEGORY_CATEGORY_THEORY
 * @version 4.0.0
 */

#include "preset_category_theory.h"

#include <string.h>

#include "lv_internal.h"
#include "lv_utils.h"
#include "preset_blocks.h"
#include "preset_common.h"

/* ==================== 预设函数块数量 ==================== */

/** 范畴论模块预设函数块总数 */

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 注册单个范畴论预设
 *
 * 辅助函数，简化预设注册过程。
 * 所有范畴论预设都属于 PRESET_CATEGORY_CATEGORY_THEORY 类别。
 *
 * @param name 预设名称
 * @param description 中文描述
 * @param input_types 输入类型数组
 * @param input_count 输入数量
 * @param output_type 输出类型
 * @param math_def 数学定义（LaTeX格式）
 * @param complexity 时间复杂度
 * @param is_constructive 是否构造性
 * @param is_reversible 是否可逆
 * @return true 注册成功
 * @return false 注册失败
 */
static bool register_category_theory_preset(const char *name, const char *description, const PresetType *input_types,
                                            int input_count, PresetType output_type, const char *math_def,
                                            const char *complexity, bool is_constructive, bool is_reversible) {
    return preset_blocks_register_simple(name, description, PRESET_CATEGORY_CATEGORY_THEORY, input_types, input_count,
                                         output_type, math_def, complexity, is_constructive, is_reversible);
}

/* ==================== 模块注册实现 ==================== */

bool preset_category_theory_register(void) {
    int success_count = 0;

    /* ============================================================
     * 第一部分：基本概念
     * ============================================================ */

    /* -------------------- 恒等态射 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SET};
        if (register_category_theory_preset(PRESET_CAT_IDENTITY_MORPHISM,
                                            "恒等态射：获取对象 A 的恒等态射 id_A: A -> A，满足 id_A(x) = x", inputs, 1,
                                            PRESET_TYPE_FUNCTION,
                                            "\\mathrm{id}_A : A \\to A, \\quad \\mathrm{id}_A(x) = x, "
                                            "\\quad f \\circ \\mathrm{id}_A = f = \\mathrm{id}_B \\circ f",
                                            "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 态射复合 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION};
        if (register_category_theory_preset(
                PRESET_CAT_COMPOSITION, "态射复合：计算两个可复合态射的复合 g o f，要求 cod(f) = dom(g)", inputs, 2,
                PRESET_TYPE_FUNCTION,
                "(g \\circ f)(x) = g(f(x)), \\quad \\text{要求 } \\mathrm{cod}(f) = \\mathrm{dom}(g)", "O(1)", true,
                false)) {
            success_count++;
        }
    }

    /* -------------------- 同构判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION};
        if (register_category_theory_preset(
                PRESET_CAT_ISOMORPHISM_TEST,
                "同构判定：判定态射 f: A -> B 是否为同构（存在逆态射 f^{-1} 使得 f o f^{-1} = id 且 f^{-1} o f = id）",
                inputs, 1, PRESET_TYPE_BOOLEAN,
                "f \\text{ 是同构} \\Leftrightarrow \\exists g, \\; f \\circ g = \\mathrm{id}_B "
                "\\land g \\circ f = \\mathrm{id}_A",
                "O(|\\mathrm{Hom}|)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第二部分：函子
     * ============================================================ */

    /* -------------------- 函子作用 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION};
        if (register_category_theory_preset(PRESET_CAT_FUNCTOR_APPLY,
                                            "函子作用：计算函子 F 对态射 f 的作用 F(f)，保持恒等和复合", inputs, 2,
                                            PRESET_TYPE_FUNCTION,
                                            "F(\\mathrm{id}_A) = \\mathrm{id}_{F(A)}, \\quad "
                                            "F(g \\circ f) = F(g) \\circ F(f)",
                                            "O(1)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第三部分：自然变换
     * ============================================================ */

    /* -------------------- 自然变换 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION};
        if (register_category_theory_preset(PRESET_CAT_NATURAL_TRANSFORMATION,
                                            "自然变换：判定或构造两个函子 F, G 之间的自然变换 alpha: F => G", inputs, 3,
                                            PRESET_TYPE_BOOLEAN,
                                            "\\alpha : F \\Rightarrow G \\Leftrightarrow "
                                            "\\forall f : A \\to B, \\; G(f) \\circ \\alpha_A = \\alpha_B \\circ F(f)",
                                            "O(|\\mathrm{Ob}| \\cdot |\\mathrm{Mor}|)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第四部分：极限与余极限
     * ============================================================ */

    /* -------------------- 积 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_SET};
        if (register_category_theory_preset(PRESET_CAT_PRODUCT,
                                            "积：构造对象 A 和 B 的积 A x B，带投影态射 pi_1, pi_2，满足泛性质", inputs,
                                            2, PRESET_TYPE_SET,
                                            "A \\times B \\text{ 满足：} \\forall Q, f, g, "
                                            "\\exists! \\langle f, g \\rangle : Q \\to A \\times B",
                                            "O(|\\mathrm{Hom}|^2)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 余积 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_SET};
        if (register_category_theory_preset(PRESET_CAT_COPRODUCT,
                                            "余积：构造对象 A 和 B 的余积 A + B，带内射态射 i_1, i_2，满足泛性质",
                                            inputs, 2, PRESET_TYPE_SET,
                                            "A \\amalg B \\text{ 满足：} \\forall Q, f, g, "
                                            "\\exists! [f, g] : A \\amalg B \\to Q",
                                            "O(|\\mathrm{Hom}|^2)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 拉回 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION};
        if (register_category_theory_preset(PRESET_CAT_PULLBACK,
                                            "拉回（纤维积）：构造交换方框的拉回 A x_C B = {(a,b) : f(a) = g(b)}",
                                            inputs, 2, PRESET_TYPE_SET,
                                            "A \\times_C B = \\{(a, b) \\mid f(a) = g(b)\\}, "
                                            "\\text{满足泛性质}",
                                            "O(|A| \\cdot |B|)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 推出 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION};
        if (register_category_theory_preset(PRESET_CAT_PUSHOUT, "推出：构造余交换方框的推出 A +_C B，满足泛性质",
                                            inputs, 2, PRESET_TYPE_SET, "A \\amalg_C B \\text{ 满足泛性质}",
                                            "O(|A| + |B|)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 等化子 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION};
        if (register_category_theory_preset(PRESET_CAT_EQUALIZER,
                                            "等化子：构造平行态射 f, g: A -> B 的等化子 eq(f,g) = {x : f(x) = g(x)}",
                                            inputs, 2, PRESET_TYPE_SET,
                                            "\\mathrm{eq}(f, g) = \\{x \\in A : f(x) = g(x)\\}, "
                                            "\\text{满足 } f \\circ e = g \\circ e",
                                            "O(|A|)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 余等化子 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION};
        if (register_category_theory_preset(PRESET_CAT_COEQUALIZER,
                                            "余等化子：构造平行态射 f, g: A -> B 的余等化子 coeq(f,g) = B/~", inputs, 2,
                                            PRESET_TYPE_SET,
                                            "\\mathrm{coeq}(f, g) = B / {\\sim}, "
                                            "\\text{其中 } {\\sim} \\text{ 是 } f, g \\text{ 生成的最小等价关系}",
                                            "O(|B| + |A|)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第五部分：泛性质
     * ============================================================ */

    /* -------------------- 指数对象 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_SET};
        if (register_category_theory_preset(PRESET_CAT_EXPONENTIAL,
                                            "指数对象：构造指数对象 B^A，满足 Hom(A x B, C) ≅ Hom(A, C^B)", inputs, 2,
                                            PRESET_TYPE_SET,
                                            "B^A \\text{ 满足：} \\mathrm{Hom}(A \\times B, C) \\cong "
                                            "\\mathrm{Hom}(A, C^B)",
                                            "O(|\\mathrm{Hom}|)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 初始对象 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SET};
        if (register_category_theory_preset(PRESET_CAT_INITIAL_OBJECT,
                                            "初始对象：判定对象 0 是否为范畴的初始对象（到每个对象恰有一个态射）",
                                            inputs, 1, PRESET_TYPE_BOOLEAN,
                                            "0 \\text{ 是初始对象} \\Leftrightarrow "
                                            "\\forall A, \\; |\\mathrm{Hom}(0, A)| = 1",
                                            "O(|\\mathrm{Ob}|)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 终止对象 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SET};
        if (register_category_theory_preset(PRESET_CAT_TERMINAL_OBJECT,
                                            "终止对象：判定对象 1 是否为范畴的终止对象（从每个对象恰有一个态射）",
                                            inputs, 1, PRESET_TYPE_BOOLEAN,
                                            "1 \\text{ 是终止对象} \\Leftrightarrow "
                                            "\\forall A, \\; |\\mathrm{Hom}(A, 1)| = 1",
                                            "O(|\\mathrm{Ob}|)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 伴随函子 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION};
        if (register_category_theory_preset(PRESET_CAT_ADJOINT,
                                            "伴随函子：判定两个函子 F: C -> D 和 G: D -> C 是否构成伴随对 F -| G",
                                            inputs, 2, PRESET_TYPE_BOOLEAN,
                                            "F \\dashv G \\Leftrightarrow "
                                            "\\mathrm{Hom}_{\\mathcal{D}}(F(A), B) \\cong "
                                            "\\mathrm{Hom}_{\\mathcal{C}}(A, G(B)) \\text{ 自然同构}",
                                            "O(|\\mathrm{Ob}| \\cdot |\\mathrm{Mor}|)", true, false)) {
            success_count++;
        }
    }

    /* 返回是否所有预设都注册成功 */
    /* lv_log_info("范畴论预设注册完成，共 %d 个预设", success_count) */
    return success_count == CATEGORY_THEORY_PRESET_COUNT;
}

/**
 * @brief 获取范畴论预设函数块的类别
 * @return 预设类别枚举值
 */
PresetCategory preset_category_theory_category(void) {
    return PRESET_CATEGORY_CATEGORY_THEORY;
}

/**
 * @brief 获取范畴论预设函数块的名称列表
 * @param out_names 输出：名称数组（调用者需先释放每个元素再释放数组本身）
 * @param out_count 输出：名称数量
 * @return true 成功，false 内存分配失败
 */
bool preset_category_theory_get_names(char ***out_names, int *out_count) {
    if (!out_names || !out_count)
        return false;
    *out_count = CATEGORY_THEORY_PRESET_COUNT;
    char **names = (char **) lv_malloc(CATEGORY_THEORY_PRESET_COUNT * sizeof(char *));
    if (!names)
        return false;

    const char *preset_names[] = {
        PRESET_CAT_IDENTITY_MORPHISM,
        PRESET_CAT_COMPOSITION,
        PRESET_CAT_ISOMORPHISM_TEST,
        PRESET_CAT_FUNCTOR_APPLY,
        PRESET_CAT_NATURAL_TRANSFORMATION,
        PRESET_CAT_PRODUCT,
        PRESET_CAT_COPRODUCT,
        PRESET_CAT_PULLBACK,
        PRESET_CAT_PUSHOUT,
        PRESET_CAT_EQUALIZER,
        PRESET_CAT_COEQUALIZER,
        PRESET_CAT_EXPONENTIAL,
        PRESET_CAT_INITIAL_OBJECT,
        PRESET_CAT_TERMINAL_OBJECT,
        PRESET_CAT_ADJOINT,
    };

    for (int i = 0; i < CATEGORY_THEORY_PRESET_COUNT; i++) {
        size_t len = strlen(preset_names[i]) + 1;
        names[i] = (char *) lv_malloc(len);
        if (!names[i]) {
            for (int j = 0; j < i; j++) {
                void *tmp = names[j];
                lv_free(&tmp);
            }
            {
                void *tmp = names;
                lv_free(&tmp);
            }
            return false;
        }
        memcpy(names[i], preset_names[i], len);
    }
    *out_names = names;
    return true;
}

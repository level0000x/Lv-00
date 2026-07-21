/**
 * @file preset_ring_theory.c
 * @brief 环论预设函数块 - 实现
 *
 * 实现理论数学研究中常用的环论运算预设函数块。
 * 涵盖环基础运算、理想理论、环同态、特殊环、多项式环及环结构分析。
 * 共31个预设函数块，均遵循模块化、确定性原则。
 *
 * @module RingTheory
 * @category PRESET_CATEGORY_RING_THEORY
 * @version 3.3.0
 * @author Lv-00 开发团队
 */

#include "preset_ring_theory.h"

#include <string.h>

#include "lv00_internal.h"
#include "lv00_utils.h"
#include "preset_blocks.h"

/* ==================== 预设函数块数量 ==================== */

/** 环论模块预设函数块总数（与头文件中 PRESET_RING_ 宏数量一致） */

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 注册单个环论预设
 *
 * @param name 预设名称
 * @param description 中文描述
 * @param input_types 输入类型数组
 * @param input_count 输入数量
 * @param output_type 输出类型
 * @param math_def 数学定义
 * @param complexity 时间复杂度
 * @param is_constructive 是否构造性
 * @param is_reversible 是否可逆
 * @return true 注册成功
 * @return false 注册失败
 */
static bool register_ring_theory_preset(const char *name, const char *description, const PresetType *input_types,
                                        int input_count, PresetType output_type, const char *math_def,
                                        const char *complexity, bool is_constructive, bool is_reversible) {
    return preset_blocks_register_simple(name, description, PRESET_CATEGORY_RING_THEORY, input_types, input_count,
                                         output_type, math_def, complexity, is_constructive, is_reversible);
}

/* ==================== 模块注册实现 ==================== */

bool preset_ring_theory_register(void) {
    int success_count = 0;

    /* ============================================================
     * 第一部分：环基础运算（6个）
     * ============================================================ */

    /* -------------------- 环加法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_RING, PRESET_TYPE_RING};
        if (register_ring_theory_preset(PRESET_RING_ADDITION,
                                        "环加法：计算环R中两个元素的和 a + b，加法满足交换律与结合律", inputs, 2,
                                        PRESET_TYPE_RING, "(a, b) \\mapsto a + b \\in R", "O(1)", true, true)) {
            success_count++;
        }
    }

    /* -------------------- 环乘法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_RING, PRESET_TYPE_RING};
        if (register_ring_theory_preset(PRESET_RING_MULTIPLICATION,
                                        "环乘法：计算环R中两个元素的积 a * b，乘法满足结合律与分配律", inputs, 2,
                                        PRESET_TYPE_RING, "(a, b) \\mapsto a \\cdot b \\in R", "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 加法逆元 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_RING};
        if (register_ring_theory_preset(PRESET_RING_ADDITIVE_INVERSE,
                                        "加法逆元：计算环元素的加法逆元 -a，满足 a + (-a) = 0_R", inputs, 1,
                                        PRESET_TYPE_RING, "a \\mapsto -a, \\quad a + (-a) = 0_R", "O(1)", true, true)) {
            success_count++;
        }
    }

    /* -------------------- 乘法逆元 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_RING};
        if (register_ring_theory_preset(PRESET_RING_MULTIPLICATIVE_INVERSE,
                                        "乘法逆元：计算环元素的乘法逆元 a^(-1)（若存在），满足 a * a^(-1) = 1_R",
                                        inputs, 1, PRESET_TYPE_RING, "a \\mapsto a^{-1}, \\quad a \\cdot a^{-1} = 1_R",
                                        "O(1)", true, true)) {
            success_count++;
        }
    }

    /* -------------------- 零元检测 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_RING};
        if (register_ring_theory_preset(
                PRESET_RING_ZERO_ELEMENT, "零元检测：获取环R的加法单位元 0_R，满足 a + 0_R = a 对所有 a in R", inputs,
                1, PRESET_TYPE_RING, "0_R \\in R, \\quad \\forall a \\in R: a + 0_R = a", "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 单位元检测 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_RING};
        if (register_ring_theory_preset(
                PRESET_RING_IDENTITY_ELEMENT,
                "单位元检测：获取环R的乘法单位元 1_R（若存在），满足 a * 1_R = a 对所有 a in R", inputs, 1,
                PRESET_TYPE_RING, "1_R \\in R, \\quad \\forall a \\in R: a \\cdot 1_R = a", "O(1)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第二部分：理想相关（7个）
     * ============================================================ */

    /* -------------------- 理想判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_RING, PRESET_TYPE_SET};
        if (register_ring_theory_preset(PRESET_RING_IDEAL_TEST,
                                        "理想判定：验证环R的子集I是否构成理想（加法子群 + 吸收性）", inputs, 2,
                                        PRESET_TYPE_BOOLEAN,
                                        "I \\triangleleft R \\Leftrightarrow (I \\le (R,+)) \\land (\\forall r \\in R, "
                                        "a \\in I: ra \\in I \\land ar \\in I)",
                                        "O(n^2)", false, false)) {
            success_count++;
        }
    }

    /* -------------------- 主理想生成 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_RING};
        if (register_ring_theory_preset(PRESET_PRINCIPAL_IDEAL, "主理想生成：由单个元素a生成主理想 <a> = aR = Ra",
                                        inputs, 1, PRESET_TYPE_IDEAL,
                                        "\\langle a \\rangle = \\{ra : r \\in R\\} \\quad \\text{（交换环）}", "O(1)",
                                        true, false)) {
            success_count++;
        }
    }

    /* -------------------- 理想和 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_IDEAL, PRESET_TYPE_IDEAL};
        if (register_ring_theory_preset(
                PRESET_RING_IDEAL_SUM, "理想和：计算两个理想的和 I + J = {a + b : a in I, b in J}", inputs, 2,
                PRESET_TYPE_IDEAL, "I + J = \\{a + b : a \\in I, \\; b \\in J\\}", "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 理想交 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_IDEAL, PRESET_TYPE_IDEAL};
        if (register_ring_theory_preset(
                PRESET_RING_IDEAL_INTERSECTION, "理想交：计算两个理想的交 I ∩ J，是包含于I和J的最大理想", inputs, 2,
                PRESET_TYPE_IDEAL, "I \\cap J = \\{a : a \\in I \\land a \\in J\\}", "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 商环构造 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_RING, PRESET_TYPE_IDEAL};
        if (register_ring_theory_preset(
                PRESET_RING_QUOTIENT_RING, "商环构造：由环R和理想I构造商环 R/I，元素为陪集 a + I", inputs, 2,
                PRESET_TYPE_RING, "R/I = \\{a + I : a \\in R\\}, \\quad (a+I)+(b+I)=(a+b)+I, \\; (a+I)(b+I)=(ab)+I",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 极大理想判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_IDEAL, PRESET_TYPE_RING};
        if (register_ring_theory_preset(PRESET_RING_MAXIMAL_IDEAL_TEST,
                                        "极大理想判定：判定理想I是否为极大理想（R/I是域）", inputs, 2,
                                        PRESET_TYPE_BOOLEAN,
                                        "I \\text{ 极大} \\Leftrightarrow R/I \\text{ 是域} \\Leftrightarrow \\nexists "
                                        "J: I \\subsetneq J \\subsetneq R",
                                        "O(n^2)", false, false)) {
            success_count++;
        }
    }

    /* -------------------- 素理想判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_IDEAL, PRESET_TYPE_RING};
        if (register_ring_theory_preset(PRESET_RING_PRIME_IDEAL_TEST, "素理想判定：判定理想I是否为素理想（R/I是整环）",
                                        inputs, 2, PRESET_TYPE_BOOLEAN,
                                        "I \\text{ 素} \\Leftrightarrow R/I \\text{ 是整环} \\Leftrightarrow (ab \\in "
                                        "I \\Rightarrow a \\in I \\lor b \\in I)",
                                        "O(n^2)", false, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第三部分：环同态（4个）
     * ============================================================ */

    /* -------------------- 环同态检测 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_RING, PRESET_TYPE_RING, PRESET_TYPE_FUNCTION};
        if (register_ring_theory_preset(
                PRESET_RING_HOMOMORPHISM_TEST,
                "环同态检测：验证映射 f: R -> S 是否为环同态，满足 f(a+b)=f(a)+f(b), f(ab)=f(a)f(b), f(1_R)=1_S",
                inputs, 3, PRESET_TYPE_BOOLEAN,
                "f: R \\to S \\text{ 是环同态} \\Leftrightarrow f(a+b)=f(a)+f(b), \\; f(ab)=f(a)f(b), \\; f(1_R)=1_S",
                "O(n^2)", false, false)) {
            success_count++;
        }
    }

    /* -------------------- 同态核 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION};
        if (register_ring_theory_preset(PRESET_RING_HOMOMORPHISM_KERNEL,
                                        "同态核：计算环同态 f 的核 ker(f) = {a in R | f(a) = 0_S}，同态核始终是R的理想",
                                        inputs, 1, PRESET_TYPE_IDEAL,
                                        "\\ker(f) = \\{a \\in R : f(a) = 0_S\\}, \\quad \\ker(f) \\triangleleft R",
                                        "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 同态像 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION};
        if (register_ring_theory_preset(
                PRESET_RING_HOMOMORPHISM_IMAGE, "同态像：计算环同态 f 的像 Im(f) = {f(a) | a in R}，同态像是S的子环",
                inputs, 1, PRESET_TYPE_RING, "\\text{Im}(f) = \\{f(a) : a \\in R\\}, \\quad \\text{Im}(f) \\le S",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 环同构判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_RING, PRESET_TYPE_RING};
        if (register_ring_theory_preset(
                PRESET_RING_ISOMORPHISM_TEST, "环同构判定：判定两个环R和S是否同构，即存在双射的环同态 f: R -> S",
                inputs, 2, PRESET_TYPE_BOOLEAN,
                "R \\cong S \\Leftrightarrow \\exists f: R \\to S \\text{ 双射，且为环同态}", "O(n^2)", false, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第四部分：特殊环（5个）
     * ============================================================ */

    /* -------------------- 整环判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_RING};
        if (register_ring_theory_preset(
                PRESET_RING_INTEGRAL_DOMAIN_TEST, "整环判定：验证环R是否为整环（交换含幺环且无零因子）", inputs, 1,
                PRESET_TYPE_BOOLEAN,
                "R \\text{ 是整环} \\Leftrightarrow R \\text{ 交换含幺} \\land (ab = 0 \\Rightarrow a = 0 \\lor b = 0)",
                "O(n^2)", false, false)) {
            success_count++;
        }
    }

    /* -------------------- 域判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_RING};
        if (register_ring_theory_preset(
                PRESET_RING_FIELD_TEST, "域判定：验证环R是否为域（非零元均可逆的交换含幺环）", inputs, 1,
                PRESET_TYPE_BOOLEAN,
                "R \\text{ 是域} \\Leftrightarrow R \\text{ 是整环} \\land \\forall a \\neq 0, \\exists a^{-1}",
                "O(n^2)", false, false)) {
            success_count++;
        }
    }

    /* -------------------- 欧几里得整环判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_RING};
        if (register_ring_theory_preset(PRESET_RING_EUCLIDEAN_DOMAIN_TEST,
                                        "欧几里得整环判定：验证整环R是否为欧几里得整环（存在欧几里得范数函数）", inputs,
                                        1, PRESET_TYPE_BOOLEAN,
                                        "R \\text{ 是欧几里得整环} \\Leftrightarrow \\exists d: R \\setminus \\{0\\} "
                                        "\\to \\mathbb{N}, \\; \\forall a,b \\neq 0: a = bq + r, \\; d(r) < d(b)",
                                        "O(n^2)", false, false)) {
            success_count++;
        }
    }

    /* -------------------- 主理想整环判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_RING};
        if (register_ring_theory_preset(PRESET_RING_PID_TEST,
                                        "主理想整环判定：验证整环R是否为主理想整环（每个理想都是主理想）", inputs, 1,
                                        PRESET_TYPE_BOOLEAN,
                                        "R \\text{ 是PID} \\Leftrightarrow \\forall I \\triangleleft R, \\; \\exists a "
                                        "\\in R: I = \\langle a \\rangle",
                                        "O(n^2)", false, false)) {
            success_count++;
        }
    }

    /* -------------------- 唯一分解整环判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_RING};
        if (register_ring_theory_preset(
                PRESET_RING_UFD_TEST,
                "唯一分解整环判定：验证整环R是否为唯一分解整环（每个非零非可逆元可唯一分解为不可约元之积）", inputs, 1,
                PRESET_TYPE_BOOLEAN,
                "R \\text{ 是UFD} \\Leftrightarrow \\forall a \\neq 0, a \\notin R^*: a = up_1 p_2 \\cdots p_n "
                "\\text{（不可约分解唯一）}",
                "O(n^3)", false, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第五部分：多项式环（5个）
     * ============================================================ */

    /* -------------------- 多项式加法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POLYNOMIAL, PRESET_TYPE_POLYNOMIAL};
        if (register_ring_theory_preset(PRESET_RING_POLY_ADDITION, "多项式加法：计算两个多项式的和 f(x) + g(x)", inputs,
                                        2, PRESET_TYPE_POLYNOMIAL, "(f + g)(x) = \\sum_{i=0}^{n} (a_i + b_i) x^i",
                                        "O(n)", true, true)) {
            success_count++;
        }
    }

    /* -------------------- 多项式乘法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POLYNOMIAL, PRESET_TYPE_POLYNOMIAL};
        if (register_ring_theory_preset(
                PRESET_RING_POLY_MULTIPLICATION, "多项式乘法：计算两个多项式的积 f(x) * g(x)，使用卷积算法", inputs, 2,
                PRESET_TYPE_POLYNOMIAL, "(f \\cdot g)(x) = \\sum_{k=0}^{m+n} \\left(\\sum_{i+j=k} a_i b_j\\right) x^k",
                "O(n^2)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 多项式GCD -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POLYNOMIAL, PRESET_TYPE_POLYNOMIAL};
        if (register_ring_theory_preset(
                PRESET_RING_POLY_GCD, "多项式GCD：使用欧几里得算法计算两个多项式的最大公因式 gcd(f, g)", inputs, 2,
                PRESET_TYPE_POLYNOMIAL,
                "\\gcd(f, g) = d, \\quad d | f \\land d | g \\land (e | f \\land e | g \\Rightarrow e | d)", "O(n^2)",
                true, false)) {
            success_count++;
        }
    }

    /* -------------------- 多项式求值 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POLYNOMIAL, PRESET_TYPE_RING};
        if (register_ring_theory_preset(
                PRESET_RING_POLY_EVALUATION,
                "多项式求值：计算多项式 f(x) 在点 c 处的值 f(c)，使用秦九韶算法（Horner法则）", inputs, 2,
                PRESET_TYPE_RING, "f(c) = a_n c^n + a_{n-1} c^{n-1} + \\cdots + a_1 c + a_0", "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 多项式不可约判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POLYNOMIAL, PRESET_TYPE_RING};
        if (register_ring_theory_preset(
                PRESET_RING_POLY_IRREDUCIBLE_TEST, "多项式不可约判定：判定多项式 f(x) 在给定环/域上是否不可约", inputs,
                2, PRESET_TYPE_BOOLEAN,
                "f \\text{ 不可约} \\Leftrightarrow f = gh \\Rightarrow \\deg(g) = 0 \\text{ 或 } \\deg(h) = 0",
                "O(n^3)", false, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第六部分：环结构（4个）
     * ============================================================ */

    /* -------------------- 环的特征 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_RING};
        if (register_ring_theory_preset(PRESET_RING_CHARACTERISTIC,
                                        "环的特征：计算环R的特征 char(R)，即满足 n*1_R = 0_R 的最小正整数n", inputs, 1,
                                        PRESET_TYPE_INTEGER, "\\text{char}(R) = \\min\\{n > 0 : n \\cdot 1_R = 0_R\\}",
                                        "O(1)", false, false)) {
            success_count++;
        }
    }

    /* -------------------- 幂零元检测 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_RING};
        if (register_ring_theory_preset(
                PRESET_RING_NILPOTENT_TEST, "幂零元检测：判定环元素a是否为幂零元，即存在正整数n使得 a^n = 0", inputs, 1,
                PRESET_TYPE_BOOLEAN, "a \\text{ 幂零} \\Leftrightarrow \\exists n > 0: a^n = 0_R", "O(n)", false,
                false)) {
            success_count++;
        }
    }

    /* -------------------- 幂等元检测 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_RING};
        if (register_ring_theory_preset(
                PRESET_RING_IDEMPOTENT_TEST, "幂等元检测：判定环元素a是否为幂等元，满足 a^2 = a", inputs, 1,
                PRESET_TYPE_BOOLEAN, "a \\text{ 幂等} \\Leftrightarrow a^2 = a", "O(1)", false, false)) {
            success_count++;
        }
    }

    /* -------------------- 单位的群 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_RING};
        if (register_ring_theory_preset(
                PRESET_RING_UNIT_GROUP, "单位的群：计算环R的所有可逆元构成的乘法群 R^*", inputs, 1, PRESET_TYPE_SET,
                "R^* = \\{a \\in R : \\exists b \\in R, \\; ab = ba = 1_R\\}", "O(n^2)", true, false)) {
            success_count++;
        }
    }

    /* 返回是否所有预设都注册成功 */
    return success_count == RING_THEORY_PRESET_COUNT;
}

/**
 * @brief 获取环论预设函数块数量
 *
 * @return int 环论模块预设函数块总数
 */
int preset_ring_theory_count(void) {
    return RING_THEORY_PRESET_COUNT;
}

PresetCategory preset_ring_theory_category(void) {
    return PRESET_CATEGORY_RING_THEORY;
}

bool preset_ring_theory_get_names(char ***out_names, int *out_count) {
    if (!out_names || !out_count)
        return false;

    char **names = (char **) lv00_malloc(RING_THEORY_PRESET_COUNT * sizeof(char *));
    if (!names)
        return false;

    const char *preset_names[] = {
        /* 环基础运算 */
        PRESET_RING_ADDITION,
        PRESET_RING_MULTIPLICATION,
        PRESET_RING_ADDITIVE_INVERSE,
        PRESET_RING_MULTIPLICATIVE_INVERSE,
        PRESET_RING_ZERO_ELEMENT,
        PRESET_RING_IDENTITY_ELEMENT,
        /* 理想相关 */
        PRESET_RING_IDEAL_TEST,
        PRESET_PRINCIPAL_IDEAL,
        PRESET_RING_IDEAL_SUM,
        PRESET_RING_IDEAL_INTERSECTION,
        PRESET_RING_QUOTIENT_RING,
        PRESET_RING_MAXIMAL_IDEAL_TEST,
        PRESET_RING_PRIME_IDEAL_TEST,
        /* 环同态 */
        PRESET_RING_HOMOMORPHISM_TEST,
        PRESET_RING_HOMOMORPHISM_KERNEL,
        PRESET_RING_HOMOMORPHISM_IMAGE,
        PRESET_RING_ISOMORPHISM_TEST,
        /* 特殊环 */
        PRESET_RING_INTEGRAL_DOMAIN_TEST,
        PRESET_RING_FIELD_TEST,
        PRESET_RING_EUCLIDEAN_DOMAIN_TEST,
        PRESET_RING_PID_TEST,
        PRESET_RING_UFD_TEST,
        /* 多项式环 */
        PRESET_RING_POLY_ADDITION,
        PRESET_RING_POLY_MULTIPLICATION,
        PRESET_RING_POLY_GCD,
        PRESET_RING_POLY_EVALUATION,
        PRESET_RING_POLY_IRREDUCIBLE_TEST,
        /* 环结构 */
        PRESET_RING_CHARACTERISTIC,
        PRESET_RING_NILPOTENT_TEST,
        PRESET_RING_IDEMPOTENT_TEST,
        PRESET_RING_UNIT_GROUP,
    };

    int count = (int) (sizeof(preset_names) / sizeof(preset_names[0]));

    for (int i = 0; i < count; i++) {
        names[i] = lv00_strdup(preset_names[i]);
        if (names[i] == NULL) {
            for (int j = 0; j < i; j++) {
                void *tmp = names[j];
                lv00_free(&tmp);
            }
            {
                void *tmp = names;
                lv00_free(&tmp);
            }
            return false;
        }
    }

    *out_names = names;
    *out_count = count;
    return true;
}
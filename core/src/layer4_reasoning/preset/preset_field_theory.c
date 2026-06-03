/**
 * @file preset_field_theory.c
 * @brief 域论预设函数块模块 - 实现（v2统一宏模式）
 *
 * 实现理论数学研究中常用的域论运算预设函数块。
 * 涵盖域基础运算、域扩张及伽罗瓦理论。
 * 共30个预设函数块，均遵循模块化、确定性原则。
 *
 * @module FieldTheory
 * @category PRESET_CATEGORY_ALGEBRAIC
 * @version 5.0.0
 */

#include "preset_field_theory.h"

#include <string.h>

#include "lv00_internal.h"
#include "lv00_utils.h"
#include "preset_blocks.h"
#include "preset_common.h"

/* ==================== 预设函数块数量 ==================== */

/** 域论模块预设函数块总数 */
#define FIELD_THEORY_PRESET_COUNT 30

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 注册单个域论预设
 *
 * 辅助函数，简化预设注册过程。
 * 所有域论预设都属于 PRESET_CATEGORY_ALGEBRAIC 类别。
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
static bool register_field_theory_preset(const char *name, const char *description, const PresetType *input_types,
                                         int input_count, PresetType output_type, const char *math_def,
                                         const char *complexity, bool is_constructive, bool is_reversible) {
    return preset_blocks_register_simple(name, description, PRESET_CATEGORY_ALGEBRAIC, input_types, input_count,
                                         output_type, math_def, complexity, is_constructive, is_reversible);
}

/* ==================== v2统一注册宏 ==================== */

/**
 * @brief 域论预设统一注册宏
 *
 * 使用do-while(0)包装，确保宏展开后在语法上等价于单条语句。
 * 注册成功时递增success_count，失败时输出错误日志。
 *
 * @param name       预设名称
 * @param desc       中文描述
 * @param inputs     输入类型数组
 * @param in_count   输入数量
 * @param output     输出类型
 * @param math       数学定义（LaTeX格式字符串）
 * @param comp       时间复杂度
 * @param cons       是否构造性
 * @param rev        是否可逆
 */
#define REGISTER_FIELD(name, desc, inputs, in_count, output, math, comp, cons, rev)                              \
    do {                                                                                                         \
        if (register_field_theory_preset((name), (desc), (inputs), (in_count), (output), (math), (comp), (cons), \
                                         (rev))) {                                                               \
            success_count++;                                                                                     \
        } else {                                                                                                 \
            /* PRESET_ERROR_LOG("注册预设失败: %s", (name)); */                                                  \
        }                                                                                                        \
    } while (0)

/* ==================== 模块注册实现 ==================== */

bool preset_field_theory_register(void) {
    int success_count = 0;

    /* ============================================================
     * 第一部分：域基础运算（8个）
     * ============================================================ */

    /**
     * @brief field_add - 域加法
     *
     * 计算域F中两个元素的和 a + b。
     * 域加法构成阿贝尔群。
     *
     * @param a 域元素（PRESET_TYPE_FIELD）
     * @param b 域元素（PRESET_TYPE_FIELD）
     * @return 和 a + b（PRESET_TYPE_FIELD）
     * @math (a, b) \\mapsto a + b \\in F
     * @complexity O(1)
     * @constructive true
     * @reversible true（减法为其逆运算）
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FIELD, PRESET_TYPE_FIELD};
        REGISTER_FIELD("field_add", "域加法：计算域F中两个元素的和 a + b", inputs, 2, PRESET_TYPE_FIELD,
                       "(a, b) \\mapsto a + b \\in F", "O(1)", true, true);
    }

    /**
     * @brief field_multiply - 域乘法
     *
     * 计算域F中两个元素的积 a * b。
     * 域中非零元素对乘法构成阿贝尔群。
     *
     * @param a 域元素（PRESET_TYPE_FIELD）
     * @param b 域元素（PRESET_TYPE_FIELD）
     * @return 积 a * b（PRESET_TYPE_FIELD）
     * @math (a, b) \\mapsto a \\cdot b \\in F
     * @complexity O(1)
     * @constructive true
     * @reversible true（非零元素可逆）
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FIELD, PRESET_TYPE_FIELD};
        REGISTER_FIELD(PRESET_FIELD_MULTIPLY, "域乘法：计算域F中两个元素的积 a * b", inputs, 2, PRESET_TYPE_FIELD,
                       "(a, b) \\mapsto a \\cdot b \\in F", "O(1)", true, true);
    }

    /**
     * @brief field_inverse - 乘法逆元
     *
     * 计算域中非零元素的乘法逆元 a^{-1}，
     * 满足 a * a^{-1} = 1_F。
     *
     * @param a 域元素（PRESET_TYPE_FIELD）
     * @return 乘法逆元 a^{-1}（PRESET_TYPE_FIELD）
     * @math a \\mapsto a^{-1}, \\quad a \\cdot a^{-1} = 1_F, \\; a \\neq 0
     * @complexity O(1)
     * @constructive true
     * @reversible true（逆元的逆元为自身）
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FIELD};
        REGISTER_FIELD("field_inverse", "乘法逆元：计算域中非零元素的乘法逆元 a^{-1}，满足 a * a^{-1} = 1", inputs, 1,
                       PRESET_TYPE_FIELD, "a \\mapsto a^{-1}, \\quad a \\cdot a^{-1} = 1_F, \\; a \\neq 0", "O(1)",
                       true, true);
    }

    /**
     * @brief field_divide - 域除法
     *
     * 计算域F中两个元素的商 a / b = a * b^{-1}（b != 0）。
     *
     * @param a 被除数（PRESET_TYPE_FIELD）
     * @param b 除数（PRESET_TYPE_FIELD）
     * @return 商 a / b（PRESET_TYPE_FIELD）
     * @math (a, b) \\mapsto a \\cdot b^{-1} \\in F, \\quad b \\neq 0
     * @complexity O(1)
     * @constructive true
     * @reversible true（乘法为其逆运算）
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FIELD, PRESET_TYPE_FIELD};
        REGISTER_FIELD(PRESET_FIELD_DIVIDE, "域除法：计算域F中两个元素的商 a / b（b != 0）", inputs, 2,
                       PRESET_TYPE_FIELD, "(a, b) \\mapsto a \\cdot b^{-1} \\in F, \\quad b \\neq 0", "O(1)", true,
                       true);
    }

    /**
     * @brief field_characteristic - 域特征
     *
     * 计算域F的特征 char(F)，
     * 即满足 n*1_F = 0_F 的最小正整数n（若不存在则为0）。
     * 域的特征只能是0或素数p。
     *
     * @param F 域（PRESET_TYPE_FIELD）
     * @return 域特征（PRESET_TYPE_INTEGER）
     * @math \\text{char}(F) = \\min\\{n > 0 : n \\cdot 1_F = 0\\}, \\quad \\text{char}(F) \\in \\{0, p \\; | \\; p \\text{ 是素数}\\}
     * @complexity O(1)
     * @constructive false
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FIELD};
        REGISTER_FIELD(PRESET_FIELD_CHARACTERISTIC,
                       "域特征：计算域F的特征 char(F)，即满足 n*1=0 的最小正整数n（若不存在则为0）", inputs, 1,
                       PRESET_TYPE_INTEGER,
                       "\\text{char}(F) = \\min\\{n > 0 : n \\cdot 1_F = 0\\}, \\quad \\text{char}(F) \\in \\{0, p \\; "
                       "| \\; p \\text{ 是素数}\\}",
                       "O(1)", false, false);
    }

    /**
     * @brief field_subfield_check - 子域判定
     *
     * 判定域F的子集K是否构成子域。
     * 条件：K包含0和1，对加减乘除（非零除数）封闭。
     *
     * @param F 域（PRESET_TYPE_FIELD）
     * @param K 子集（PRESET_TYPE_SET）
     * @return 是否为子域（PRESET_TYPE_BOOLEAN）
     * @math K \\le F \\Leftrightarrow 0, 1 \\in K \\land (\\forall a,b \\in K: a \\pm b, ab \\in K) \\land (\\forall a \\neq 0 \\in K: a^{-1} \\in K)
     * @complexity O(n^2)
     * @constructive false
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FIELD, PRESET_TYPE_SET};
        REGISTER_FIELD(PRESET_FIELD_SUBFIELD_CHECK, "子域判定：判定域F的子集K是否构成子域（含0,1，四则运算封闭）",
                       inputs, 2, PRESET_TYPE_BOOLEAN,
                       "K \\le F \\Leftrightarrow 0, 1 \\in K \\land (\\forall a,b \\in K: a \\pm b, ab \\in K) \\land "
                       "(\\forall a \\neq 0 \\in K: a^{-1} \\in K)",
                       "O(n^2)", false, false);
    }

    /**
     * @brief field_extension_check - 域扩张判定
     *
     * 判定K是否为F的域扩张 E/F。
     * 条件：F是E的子域。
     *
     * @param E 扩域（PRESET_TYPE_FIELD）
     * @param F 基域（PRESET_TYPE_FIELD）
     * @return 是否为域扩张（PRESET_TYPE_BOOLEAN）
     * @math E/F \\text{ 是域扩张} \\Leftrightarrow F \\le E
     * @complexity O(n^2)
     * @constructive false
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FIELD, PRESET_TYPE_FIELD};
        REGISTER_FIELD(PRESET_FIELD_EXTENSION_CHECK, "域扩张判定：判定E是否为F的域扩张 E/F（F是E的子域）", inputs, 2,
                       PRESET_TYPE_BOOLEAN, "E/F \\text{ 是域扩张} \\Leftrightarrow F \\le E", "O(n^2)", false, false);
    }

    /**
     * @brief field_prime_subfield - 素子域
     *
     * 获取域F的素子域。
     * 若char(F) = p（素数），素子域同构于 F_p = Z/pZ。
     * 若char(F) = 0，素子域同构于 Q（有理数域）。
     *
     * @param F 域（PRESET_TYPE_FIELD）
     * @return 素子域（PRESET_TYPE_FIELD）
     * @math \\text{PrimeSubfield}(F) = \\begin{cases} \\mathbb{F}_p & \\text{char}(F) = p \\neq 0 \\\\ \\mathbb{Q} & \\text{char}(F) = 0 \\end{cases}
     * @complexity O(1)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FIELD};
        REGISTER_FIELD(PRESET_FIELD_PRIME_SUBFIELD, "素子域：获取域F的素子域（char=p时为F_p，char=0时为Q）", inputs, 1,
                       PRESET_TYPE_FIELD,
                       "\\text{PrimeSubfield}(F) = \\begin{cases} \\mathbb{F}_p & \\text{char}(F) = p \\neq 0 \\\\ "
                       "\\mathbb{Q} & \\text{char}(F) = 0 \\end{cases}",
                       "O(1)", true, false);
    }

    /* ============================================================
     * 第二部分：域扩张（10个）
     * ============================================================ */

    /**
     * @brief extension_degree - 扩张次数
     *
     * 计算域扩张 E/F 的次数 [E:F]，
     * 即E作为F上向量空间的维数。
     *
     * @param E 扩域（PRESET_TYPE_FIELD）
     * @param F 基域（PRESET_TYPE_FIELD）
     * @return 扩张次数 [E:F]（PRESET_TYPE_INTEGER）
     * @math [E : F] = \\dim_F(E)
     * @complexity O(n)
     * @constructive false
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FIELD, PRESET_TYPE_FIELD};
        REGISTER_FIELD(PRESET_EXTENSION_DEGREE, "扩张次数：计算域扩张 E/F 的次数 [E:F]（E作为F上向量空间的维数）",
                       inputs, 2, PRESET_TYPE_INTEGER, "[E : F] = \\dim_F(E)", "O(n)", false, false);
    }

    /**
     * @brief simple_extension - 单扩张
     *
     * 由域F和元素alpha构造简单扩张 F(alpha)。
     * 当alpha是代数元时，F(alpha) = F[alpha]。
     *
     * @param F 基域（PRESET_TYPE_FIELD）
     * @param alpha 扩张元素（PRESET_TYPE_FIELD）
     * @return 单扩张 F(alpha)（PRESET_TYPE_FIELD）
     * @math F(\\alpha) = F[\\alpha] \\text{（当 } \\alpha \\text{ 是代数元时）}
     * @complexity O(n)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FIELD, PRESET_TYPE_FIELD};
        REGISTER_FIELD(PRESET_SIMPLE_EXTENSION, "简单扩张：由域F和元素alpha构造简单扩张 F(alpha)", inputs, 2,
                       PRESET_TYPE_FIELD, "F(\\alpha) = F[\\alpha] \\text{（当 } \\alpha \\text{ 是代数元时）}", "O(n)",
                       true, false);
    }

    /**
     * @brief algebraic_extension - 代数扩张
     *
     * 判定域扩张 E/F 是否为代数扩张。
     * 代数扩张要求E中每个元素都是F上的代数元。
     *
     * @param E 扩域（PRESET_TYPE_FIELD）
     * @param F 基域（PRESET_TYPE_FIELD）
     * @return 是否为代数扩张（PRESET_TYPE_BOOLEAN）
     * @math E/F \\text{ 是代数扩张} \\Leftrightarrow \\forall \\alpha \\in E, \\; \\exists f \\in F[x]: f(\\alpha) = 0
     * @complexity O(n^2)
     * @constructive false
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FIELD, PRESET_TYPE_FIELD};
        REGISTER_FIELD("algebraic_extension",
                       "代数扩张判定：判定域扩张 E/F 是否为代数扩张（E中每个元素都是F上的代数元）", inputs, 2,
                       PRESET_TYPE_BOOLEAN,
                       "E/F \\text{ 是代数扩张} \\Leftrightarrow \\forall \\alpha \\in E, \\; \\exists f \\in F[x]: "
                       "f(\\alpha) = 0",
                       "O(n^2)", false, false);
    }

    /**
     * @brief transcendental_extension - 超越扩张
     *
     * 判定域扩张 E/F 是否为超越扩张。
     * 超越扩张指E中存在至少一个F上的超越元。
     *
     * @param E 扩域（PRESET_TYPE_FIELD）
     * @param F 基域（PRESET_TYPE_FIELD）
     * @return 是否为超越扩张（PRESET_TYPE_BOOLEAN）
     * @math E/F \\text{ 是超越扩张} \\Leftrightarrow \\exists \\alpha \\in E, \\; \\forall f \\in F[x] \\setminus \\{0\\}: f(\\alpha) \\neq 0
     * @complexity O(n^2)
     * @constructive false
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FIELD, PRESET_TYPE_FIELD};
        REGISTER_FIELD(PRESET_TRANSCENDENTAL_EXTENSION,
                       "超越扩张判定：判定域扩张 E/F 是否为超越扩张（E中存在F上的超越元）", inputs, 2,
                       PRESET_TYPE_BOOLEAN,
                       "E/F \\text{ 是超越扩张} \\Leftrightarrow \\exists \\alpha \\in E, \\; \\forall f \\in F[x] "
                       "\\setminus \\{0\\}: f(\\alpha) \\neq 0",
                       "O(n^2)", false, false);
    }

    /**
     * @brief finite_extension - 有限扩张
     *
     * 判定域扩张 E/F 是否为有限扩张。
     * 有限扩张指 [E:F] < infinity，等价于有限生成的代数扩张。
     *
     * @param E 扩域（PRESET_TYPE_FIELD）
     * @param F 基域（PRESET_TYPE_FIELD）
     * @return 是否为有限扩张（PRESET_TYPE_BOOLEAN）
     * @math E/F \\text{ 是有限扩张} \\Leftrightarrow [E : F] < \\infty
     * @complexity O(n)
     * @constructive false
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FIELD, PRESET_TYPE_FIELD};
        REGISTER_FIELD(PRESET_FINITE_EXTENSION, "有限扩张判定：判定域扩张 E/F 是否为有限扩张（[E:F] < infinity）",
                       inputs, 2, PRESET_TYPE_BOOLEAN, "E/F \\text{ 是有限扩张} \\Leftrightarrow [E : F] < \\infty",
                       "O(n)", false, false);
    }

    /**
     * @brief algebraic_element_check - 代数元判定
     *
     * 判定域扩张 E/F 中的元素alpha是否为F上的代数元。
     * alpha是代数元当且仅当存在非零多项式 f in F[x] 使得 f(alpha) = 0。
     *
     * @param alpha 域元素（PRESET_TYPE_FIELD）
     * @param F 基域（PRESET_TYPE_FIELD）
     * @return 是否为代数元（PRESET_TYPE_BOOLEAN）
     * @math \\alpha \\text{ 是代数元} \\Leftrightarrow \\exists f \\in F[x] \\setminus \\{0\\}: f(\\alpha) = 0
     * @complexity O(n^2)
     * @constructive false
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FIELD, PRESET_TYPE_FIELD};
        REGISTER_FIELD(
            PRESET_ALGEBRAIC_ELEMENT_CHECK, "代数元判定：判定域扩张中的元素alpha是否为F上的代数元", inputs, 2,
            PRESET_TYPE_BOOLEAN,
            "\\alpha \\text{ 是代数元} \\Leftrightarrow \\exists f \\in F[x] \\setminus \\{0\\}: f(\\alpha) = 0",
            "O(n^2)", false, false);
    }

    /**
     * @brief minimal_polynomial - 极小多项式
     *
     * 计算代数元alpha在F上的极小多项式。
     * 极小多项式是F[x]中首一、次数最低且以alpha为零点的多项式。
     *
     * @param alpha 代数元（PRESET_TYPE_FIELD）
     * @param F 基域（PRESET_TYPE_FIELD）
     * @return 极小多项式（PRESET_TYPE_POLYNOMIAL）
     * @math \\text{minpoly}_F(\\alpha) = \\text{F[x]中首一的最低次多项式 } f, \\; f(\\alpha) = 0
     * @complexity O(n^2)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FIELD, PRESET_TYPE_FIELD};
        REGISTER_FIELD(PRESET_MINIMAL_POLYNOMIAL,
                       "极小多项式：计算代数元alpha在F上的极小多项式（首一最低次零化多项式）", inputs, 2,
                       PRESET_TYPE_POLYNOMIAL,
                       "\\text{minpoly}_F(\\alpha) = \\text{F[x]中首一的最低次多项式 } f, \\; f(\\alpha) = 0", "O(n^2)",
                       true, false);
    }

    /**
     * @brief field_tower - 域塔定理
     *
     * 域塔定理（乘法公式）：若 F subset K subset E，
     * 则 [E:F] = [E:K] * [K:F]。
     * 给定中间域，验证或计算扩张次数的乘积关系。
     *
     * @param E 扩域（PRESET_TYPE_FIELD）
     * @param K 中间域（PRESET_TYPE_FIELD）
     * @param F 基域（PRESET_TYPE_FIELD）
     * @return 扩张次数 [E:F]（PRESET_TYPE_INTEGER）
     * @math [E : F] = [E : K] \\cdot [K : F]
     * @complexity O(n)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FIELD, PRESET_TYPE_FIELD, PRESET_TYPE_FIELD};
        REGISTER_FIELD(PRESET_FIELD_TOWER, "域塔定理：验证域塔 F subset K subset E 的次数关系 [E:F] = [E:K]*[K:F]",
                       inputs, 3, PRESET_TYPE_INTEGER, "[E : F] = [E : K] \\cdot [K : F]", "O(n)", true, false);
    }

    /**
     * @brief primitive_element - 本原元定理
     *
     * 本原元定理：有限可分扩张 E/F 存在本原元 theta，
     * 使得 E = F(theta)。
     * 给定有限可分扩张，构造本原元。
     *
     * @param E 扩域（PRESET_TYPE_FIELD）
     * @param F 基域（PRESET_TYPE_FIELD）
     * @return 本原元 theta（PRESET_TYPE_FIELD）
     * @math E = F(\\theta), \\quad \\text{当 } E/F \\text{ 是有限可分扩张时}
     * @complexity O(n^2)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FIELD, PRESET_TYPE_FIELD};
        REGISTER_FIELD(PRESET_PRIMITIVE_ELEMENT, "本原元定理：对有限可分扩张 E/F 构造本原元 theta 使得 E = F(theta)",
                       inputs, 2, PRESET_TYPE_FIELD, "E = F(\\theta), \\quad \\text{当 } E/F \\text{ 是有限可分扩张时}",
                       "O(n^2)", true, false);
    }

    /**
     * @brief normal_extension - 正规扩张
     *
     * 判定域扩张 E/F 是否为正规扩张。
     * E/F 是正规扩张当且仅当 E 是 F[x] 中某个多项式族的分裂域。
     * 等价条件：若 E 中某个不可约多项式在 E 中有一个根，则其所有根都在 E 中。
     *
     * @param E 扩域（PRESET_TYPE_FIELD）
     * @param F 基域（PRESET_TYPE_FIELD）
     * @return 是否为正规扩张（PRESET_TYPE_BOOLEAN）
     * @math E/F \\text{ 是正规扩张} \\Leftrightarrow E \\text{ 是 } F[x] \\text{ 中某个多项式集合的分裂域}
     * @complexity O(n^3)
     * @constructive false
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FIELD, PRESET_TYPE_FIELD};
        REGISTER_FIELD(PRESET_NORMAL_EXTENSION, "正规扩张判定：判定域扩张 E/F 是否为正规扩张（某个多项式族的分裂域）",
                       inputs, 2, PRESET_TYPE_BOOLEAN,
                       "E/F \\text{ 是正规扩张} \\Leftrightarrow E \\text{ 是 } F[x] \\text{ 中某个多项式集合的分裂域}",
                       "O(n^3)", false, false);
    }

    /* ============================================================
     * 第三部分：伽罗瓦理论（12个）
     * ============================================================ */

    /**
     * @brief galois_group - 伽罗瓦群
     *
     * 计算域扩张 E/F 的伽罗瓦群 Gal(E/F)。
     * Gal(E/F) 是所有保持F中元素不动的E的自同构组成的群。
     *
     * @param E 扩域（PRESET_TYPE_FIELD）
     * @param F 基域（PRESET_TYPE_FIELD）
     * @return 伽罗瓦群 Gal(E/F)（PRESET_TYPE_GROUP）
     * @math \\text{Gal}(E/F) = \\{\\sigma : E \\to E \\; | \\; \\sigma|_F = \\text{id}_F\\}
     * @complexity O(n!)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FIELD, PRESET_TYPE_FIELD};
        REGISTER_FIELD(PRESET_GALOIS_GROUP, "伽罗瓦群：计算域扩张 E/F 的伽罗瓦群 Gal(E/F)（所有F-自同构组成的群）",
                       inputs, 2, PRESET_TYPE_GROUP,
                       "\\text{Gal}(E/F) = \\{\\sigma : E \\to E \\; | \\; \\sigma|_F = \\text{id}_F\\}", "O(n!)", true,
                       false);
    }

    /**
     * @brief galois_group_order - 伽罗瓦群阶
     *
     * 计算伽罗瓦群 Gal(E/F) 的阶 |Gal(E/F)|。
     * 对于有限伽罗瓦扩张，|Gal(E/F)| = [E:F]。
     *
     * @param E 扩域（PRESET_TYPE_FIELD）
     * @param F 基域（PRESET_TYPE_FIELD）
     * @return 伽罗瓦群的阶（PRESET_TYPE_INTEGER）
     * @math |\\text{Gal}(E/F)| \\le [E : F], \\quad \\text{等号成立当且仅当 } E/F \\text{ 是伽罗瓦扩张}
     * @complexity O(n!)
     * @constructive false
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FIELD, PRESET_TYPE_FIELD};
        REGISTER_FIELD(PRESET_GALOIS_GROUP_ORDER, "伽罗瓦群阶：计算伽罗瓦群 Gal(E/F) 的阶 |Gal(E/F)|", inputs, 2,
                       PRESET_TYPE_INTEGER,
                       "|\\text{Gal}(E/F)| \\le [E : F], \\quad \\text{等号成立当且仅当 } E/F \\text{ 是伽罗瓦扩张}",
                       "O(n!)", false, false);
    }

    /**
     * @brief fixed_field - 不动域
     *
     * 计算伽罗瓦群G的不动域 E^G。
     * E^G = {x in E | sigma(x) = x 对所有 sigma in G}。
     *
     * @param E 域（PRESET_TYPE_FIELD）
     * @param G 伽罗瓦群（PRESET_TYPE_GROUP）
     * @return 不动域 E^G（PRESET_TYPE_FIELD）
     * @math E^G = \\{x \\in E : \\sigma(x) = x, \\; \\forall \\sigma \\in G\\}
     * @complexity O(n^2)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FIELD, PRESET_TYPE_GROUP};
        REGISTER_FIELD(PRESET_FIXED_FIELD,
                       "不动域：计算伽罗瓦群G的不动域 E^G = {x in E | sigma(x)=x, 对所有sigma in G}", inputs, 2,
                       PRESET_TYPE_FIELD, "E^G = \\{x \\in E : \\sigma(x) = x, \\; \\forall \\sigma \\in G\\}",
                       "O(n^2)", true, false);
    }

    /**
     * @brief galois_correspondence - 伽罗瓦对应
     *
     * 建立伽罗瓦群的子群与中间域之间的双射对应关系。
     * 伽罗瓦基本定理：伽罗瓦群的子群与中间域之间存在反序双射。
     *
     * @param E 扩域（PRESET_TYPE_FIELD）
     * @param F 基域（PRESET_TYPE_FIELD）
     * @return 对应关系是否存在（PRESET_TYPE_BOOLEAN）
     * @math \\{\\text{Gal}(E/F) \\text{ 的子群}\\} \\xleftrightarrow{1-1} \\{F \\subseteq K \\subseteq E \\text{ 的中间域}\\}
     * @complexity O(n!)
     * @constructive false
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FIELD, PRESET_TYPE_FIELD};
        REGISTER_FIELD(PRESET_GALOIS_CORRESPONDENCE, "伽罗瓦对应：建立伽罗瓦群的子群与中间域之间的双射对应关系", inputs,
                       2, PRESET_TYPE_BOOLEAN,
                       "\\{\\text{Gal}(E/F) \\text{ 的子群}\\} \\xleftrightarrow{1-1} \\{F \\subseteq K \\subseteq E "
                       "\\text{ 的中间域}\\}",
                       "O(n!)", false, false);
    }

    /**
     * @brief galois_check - 伽罗瓦扩张判定
     *
     * 判定域扩张 E/F 是否为伽罗瓦扩张。
     * E/F 是伽罗瓦扩张当且仅当它是正规、可分的（且有限的）。
     * 等价条件：|Gal(E/F)| = [E:F]。
     *
     * @param E 扩域（PRESET_TYPE_FIELD）
     * @param F 基域（PRESET_TYPE_FIELD）
     * @return 是否为伽罗瓦扩张（PRESET_TYPE_BOOLEAN）
     * @math E/F \\text{ 是伽罗瓦扩张} \\Leftrightarrow E/F \\text{ 正规且可分} \\Leftrightarrow |\\text{Gal}(E/F)| = [E:F]
     * @complexity O(n^3)
     * @constructive false
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FIELD, PRESET_TYPE_FIELD};
        REGISTER_FIELD(PRESET_GALOIS_CHECK, "伽罗瓦扩张判定：判定 E/F 是否为伽罗瓦扩张（正规且可分）", inputs, 2,
                       PRESET_TYPE_BOOLEAN,
                       "E/F \\text{ 是伽罗瓦扩张} \\Leftrightarrow E/F \\text{ 正规且可分} \\Leftrightarrow "
                       "|\\text{Gal}(E/F)| = [E:F]",
                       "O(n^3)", false, false);
    }

    /**
     * @brief separable_extension - 可分扩张
     *
     * 判定域扩张 E/F 是否为可分扩张。
     * 可分扩张要求E中每个元素的最小多项式在分裂域中无重根。
     * 特征0的域上的扩张总是可分的。
     *
     * @param E 扩域（PRESET_TYPE_FIELD）
     * @param F 基域（PRESET_TYPE_FIELD）
     * @return 是否为可分扩张（PRESET_TYPE_BOOLEAN）
     * @math E/F \\text{ 是可分扩张} \\Leftrightarrow \\forall \\alpha \\in E, \\; \\text{minpoly}_F(\\alpha) \\text{ 无重根}
     * @complexity O(n^2)
     * @constructive false
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FIELD, PRESET_TYPE_FIELD};
        REGISTER_FIELD(PRESET_SEPARABLE_EXTENSION,
                       "可分扩张判定：判定域扩张 E/F 是否为可分扩张（每个元素的最小多项式无重根）", inputs, 2,
                       PRESET_TYPE_BOOLEAN,
                       "E/F \\text{ 是可分扩张} \\Leftrightarrow \\forall \\alpha \\in E, \\; "
                       "\\text{minpoly}_F(\\alpha) \\text{ 无重根}",
                       "O(n^2)", false, false);
    }

    /**
     * @brief splitting_field - 分裂域
     *
     * 构造多项式 f(x) 在域F上的分裂域。
     * 分裂域是包含f(x)所有根的最小扩域。
     *
     * @param F 基域（PRESET_TYPE_FIELD）
     * @param f 多项式（PRESET_TYPE_POLYNOMIAL）
     * @return 分裂域（PRESET_TYPE_FIELD）
     * @math E = F(\\alpha_1, \\ldots, \\alpha_n), \\quad f(x) = (x - \\alpha_1) \\cdots (x - \\alpha_n) \\in E[x]
     * @complexity O(n!)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FIELD, PRESET_TYPE_POLYNOMIAL};
        REGISTER_FIELD(
            PRESET_SPLITTING_FIELD, "分裂域构造：构造多项式 f(x) 在域F上的分裂域（包含f(x)所有根的最小扩域）", inputs,
            2, PRESET_TYPE_FIELD,
            "E = F(\\alpha_1, \\ldots, \\alpha_n), \\quad f(x) = (x - \\alpha_1) \\cdots (x - \\alpha_n) \\in E[x]",
            "O(n!)", true, false);
    }

    /**
     * @brief cyclotomic_field - 分圆域
     *
     * 构造n次分圆域 Q(zeta_n)，
     * 即在Q上添加n次本原单位根 zeta_n = e^{2*pi*i/n}。
     * 分圆域是 x^n - 1 的分裂域。
     *
     * @param n 正整数（PRESET_TYPE_INTEGER）
     * @return 分圆域 Q(zeta_n)（PRESET_TYPE_FIELD）
     * @math \\mathbb{Q}(\\zeta_n) \\text{，其中 } \\zeta_n = e^{2\\pi i / n}, \\; \\zeta_n^n = 1
     * @complexity O(n \\log n)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER};
        REGISTER_FIELD(PRESET_CYCLOTOMIC_FIELD, "分圆域：构造n次分圆域 Q(zeta_n)，即添加n次本原单位根的扩域", inputs, 1,
                       PRESET_TYPE_FIELD,
                       "\\mathbb{Q}(\\zeta_n) \\text{，其中 } \\zeta_n = e^{2\\pi i / n}, \\; \\zeta_n^n = 1",
                       "O(n \\log n)", true, false);
    }

    /**
     * @brief finite_field_construct - 有限域构造
     *
     * 构造有限域 GF(p^n) = F_q，其中p为素数，n为正整数。
     * 有限域通过 F_p[x] / <f(x)> 构造，f是F_p上n次不可约多项式。
     *
     * @param p 素数特征（PRESET_TYPE_INTEGER）
     * @param n 扩张次数（PRESET_TYPE_INTEGER）
     * @return 有限域 GF(p^n)（PRESET_TYPE_FIELD）
     * @math \\text{GF}(p^n) = \\mathbb{F}_p[x] / \\langle f(x) \\rangle, \\quad f \\text{ 是 } \\mathbb{F}_p \\text{ 上 } n \\text{ 次不可约多项式}
     * @complexity O(n \\log p)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER};
        REGISTER_FIELD(PRESET_FINITE_FIELD_CONSTRUCT, "有限域构造：构造有限域 GF(p^n)，其中p为素数，n为正整数", inputs,
                       2, PRESET_TYPE_FIELD,
                       "\\text{GF}(p^n) = \\mathbb{F}_p[x] / \\langle f(x) \\rangle, \\quad f \\text{ 是 } "
                       "\\mathbb{F}_p \\text{ 上 } n \\text{ 次不可约多项式}",
                       "O(n \\log p)", true, false);
    }

    /**
     * @brief frobenius_automorphism - Frobenius自同构
     *
     * 计算有限域 GF(p^n) 上的Frobenius自同构。
     * Frobenius映射 phi: x -> x^p 是GF(p^n)的域自同构。
     * phi^n = id，phi 的阶为 n。
     *
     * @param x 域元素（PRESET_TYPE_FIELD）
     * @param p 素数特征（PRESET_TYPE_INTEGER）
     * @return x^p（PRESET_TYPE_FIELD）
     * @math \\varphi_p(x) = x^p, \\quad \\varphi_p \\in \\text{Gal}(\\mathbb{F}_{p^n}/\\mathbb{F}_p)
     * @complexity O(\\log p)
     * @constructive true
     * @reversible true（Frobenius自同构是双射）
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FIELD, PRESET_TYPE_INTEGER};
        REGISTER_FIELD(PRESET_FROBENIUS_AUTOMORPHISM, "Frobenius自同构：计算有限域上的Frobenius映射 x -> x^p", inputs,
                       2, PRESET_TYPE_FIELD,
                       "\\varphi_p(x) = x^p, \\quad \\varphi_p \\in \\text{Gal}(\\mathbb{F}_{p^n}/\\mathbb{F}_p)",
                       "O(\\log p)", true, true);
    }

    /**
     * @brief field_embedding - 域嵌入
     *
     * 构造域扩张 E/F 的F-嵌入（F-同态）。
     * F-嵌入是保持F中元素不动的域同态 E -> L（L为某个扩域）。
     * 对于有限可分扩张，F-嵌入的个数等于 [E:F]。
     *
     * @param E 源域（PRESET_TYPE_FIELD）
     * @param F 基域（PRESET_TYPE_FIELD）
     * @return F-嵌入列表（PRESET_TYPE_LIST）
     * @math \\text{Hom}_F(E, L) = \\{\\sigma : E \\to L \\; | \\; \\sigma|_F = \\text{id}_F\\}
     * @complexity O(n^2)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FIELD, PRESET_TYPE_FIELD};
        REGISTER_FIELD(PRESET_FIELD_EMBEDDING, "域嵌入：构造域扩张 E/F 的F-嵌入（保持F不动的域同态）", inputs, 2,
                       PRESET_TYPE_LIST,
                       "\\text{Hom}_F(E, L) = \\{\\sigma : E \\to L \\; | \\; \\sigma|_F = \\text{id}_F\\}", "O(n^2)",
                       true, false);
    }

    /**
     * @brief algebraic_closure - 代数闭包
     *
     * 构造域F的代数闭包。
     * 代数闭包是F的代数扩张，且在其中每个非常数多项式都有根。
     * 代数闭包在同构意义下唯一。
     *
     * @param F 域（PRESET_TYPE_FIELD）
     * @return 代数闭包（PRESET_TYPE_FIELD）
     * @math \\overline{F} \\text{ 满足：} \\overline{F}/F \\text{ 是代数扩张，且 } \\overline{F} \\text{ 是代数闭域}
     * @complexity O(n!)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FIELD};
        REGISTER_FIELD(
            PRESET_ALGEBRAIC_CLOSURE, "代数闭包：构造域F的代数闭包（F的代数扩张且是代数闭域）", inputs, 1,
            PRESET_TYPE_FIELD,
            "\\overline{F} \\text{ 满足：} \\overline{F}/F \\text{ 是代数扩张，且 } \\overline{F} \\text{ 是代数闭域}",
            "O(n!)", true, false);
    }

    /* 返回是否所有预设都注册成功 */
    return success_count == FIELD_THEORY_PRESET_COUNT;
}

/**
 * @brief 获取域论预设函数块数量
 *
 * @return int 域论模块预设函数块总数
 */
int preset_field_theory_count(void) {
    return FIELD_THEORY_PRESET_COUNT;
}

/**
 * @brief 获取域论预设函数块名称列表
 *
 * @param out_names 输出名称数组指针（调用者负责释放）
 * @param out_count 输出预设数量
 * @return true 获取成功
 * @return false 获取失败
 */
bool preset_field_theory_get_names(char ***out_names, int *out_count) {
    if (out_names == NULL || out_count == NULL) {
        return false;
    }

    /* 分配名称数组 */
    char **names = (char **) lv00_malloc(FIELD_THEORY_PRESET_COUNT * sizeof(char *));
    if (names == NULL) {
        return false;
    }

    /* 填充预设名称列表 */
    const char *preset_names[] = {
        /* 域基础运算 */
        PRESET_FIELD_ADD,
        PRESET_FIELD_MULTIPLY,
        PRESET_FIELD_INVERSE,
        PRESET_FIELD_DIVIDE,
        PRESET_FIELD_CHARACTERISTIC,
        PRESET_FIELD_SUBFIELD_CHECK,
        PRESET_FIELD_EXTENSION_CHECK,
        PRESET_FIELD_PRIME_SUBFIELD,
        /* 域扩张 */
        PRESET_EXTENSION_DEGREE,
        PRESET_SIMPLE_EXTENSION,
        PRESET_ALGEBRAIC_EXTENSION,
        PRESET_TRANSCENDENTAL_EXTENSION,
        PRESET_FINITE_EXTENSION,
        PRESET_ALGEBRAIC_ELEMENT_CHECK,
        PRESET_MINIMAL_POLYNOMIAL,
        PRESET_FIELD_TOWER,
        PRESET_PRIMITIVE_ELEMENT,
        PRESET_NORMAL_EXTENSION,
        /* 伽罗瓦理论 */
        PRESET_GALOIS_GROUP,
        PRESET_GALOIS_GROUP_ORDER,
        PRESET_FIXED_FIELD,
        PRESET_GALOIS_CORRESPONDENCE,
        PRESET_GALOIS_CHECK,
        PRESET_SEPARABLE_EXTENSION,
        PRESET_SPLITTING_FIELD,
        PRESET_CYCLOTOMIC_FIELD,
        PRESET_FINITE_FIELD_CONSTRUCT,
        PRESET_FROBENIUS_AUTOMORPHISM,
        PRESET_FIELD_EMBEDDING,
        PRESET_ALGEBRAIC_CLOSURE,
    };

    for (int i = 0; i < FIELD_THEORY_PRESET_COUNT; i++) {
        size_t len = strlen(preset_names[i]) + 1;
        names[i] = (char *) lv00_malloc(len);
        if (names[i] == NULL) {
            /* 分配失败时释放已分配的内存 */
            for (int j = 0; j < i; j++) {
                {
                    void *tmp = names[j];
                    lv00_free(&tmp);
                }
            }
            {
                void *tmp = names;
                lv00_free(&tmp);
            }
            return false;
        }
        memcpy(names[i], preset_names[i], len);
    }

    *out_names = names;
    *out_count = FIELD_THEORY_PRESET_COUNT;
    return true;
}

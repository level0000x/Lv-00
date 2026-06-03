/**
 * @file preset_basic_math.c
 * @brief 基础数学计算预设函数块 - 实现
 *
 * 实现理论数学研究中常用的基础数学计算预设函数块。
 * 所有预设函数块都遵循模块化、确定性原则。
 *
 * @module BasicMath
 * @category PRESET_CATEGORY_ANALYSIS
 * @version 5.0.0
 */

#include "preset_basic_math.h"

#include <string.h>

#include "lv00_internal.h"
#include "lv00_utils.h"
#include "preset_blocks.h"
#include "preset_common.h"

/* ==================== 预设函数块数量 ==================== */

/** 基础数学模块预设函数块总数 */
#define BASIC_MATH_PRESET_COUNT 30

/* ==================== 模块注册实现 ==================== */

/**
 * @brief 基础数学模块默认类别
 *
 * 使用 PRESET_REGISTER_CAT 宏替代原有的 register_basic_math_preset /
 * register_basic_math_preset_ex 静态函数。
 * 所有注册调用直接通过宏展开为 preset_blocks_register_simple()，
 * 消除了每个模块重复定义静态包装函数的模式。
 * 需要不同类别时，直接在宏调用中传入对应的 PresetCategory 枚举值即可。
 */
#define BASIC_MATH_DEFAULT_CATEGORY PRESET_CATEGORY_ANALYSIS

bool preset_basic_math_register(void) {
    int success_count = 0;

    /* ============================================================
     * 第一部分：算术运算
     * ============================================================ */

    /* -------------------- 加法运算 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (PRESET_REGISTER_CAT(PRESET_ARITHMETIC_ADD, "加法运算：计算两个标量的和 a + b",
                                BASIC_MATH_DEFAULT_CATEGORY, inputs, 2,
                                PRESET_TYPE_SCALAR, "c = a + b", "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 减法运算 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (PRESET_REGISTER_CAT(PRESET_ARITHMETIC_SUBTRACT, "减法运算：计算两个标量的差 a - b",
                                BASIC_MATH_DEFAULT_CATEGORY, inputs, 2,
                                PRESET_TYPE_SCALAR, "c = a - b", "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 乘法运算 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (PRESET_REGISTER_CAT(PRESET_ARITHMETIC_MULTIPLY, "乘法运算：计算两个标量的积 a * b",
                                BASIC_MATH_DEFAULT_CATEGORY, inputs, 2,
                                PRESET_TYPE_SCALAR, "c = a \\times b", "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 除法运算 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (PRESET_REGISTER_CAT(PRESET_ARITHMETIC_DIVIDE, "除法运算：计算两个标量的商 a / b（b != 0）",
                                BASIC_MATH_DEFAULT_CATEGORY, inputs, 2,
                                PRESET_TYPE_SCALAR, "c = \\frac{a}{b}, \\quad b \\neq 0", "O(1)", true,
                                false)) {
            success_count++;
        }
    }

    /* -------------------- 幂运算 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (PRESET_REGISTER_CAT(PRESET_ARITHMETIC_POWER, "幂运算：计算底数 a 的 n 次幂 a^n",
                                BASIC_MATH_DEFAULT_CATEGORY, inputs, 2,
                                PRESET_TYPE_SCALAR, "c = a^n", "O(log n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 开方运算 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (PRESET_REGISTER_CAT(PRESET_ARITHMETIC_ROOT, "开方运算：计算 a 的 n 次方根 n√a",
                                BASIC_MATH_DEFAULT_CATEGORY, inputs, 2,
                                PRESET_TYPE_SCALAR, "c = \\sqrt[n]{a}, \\quad a \\geq 0", "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 对数运算 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (PRESET_REGISTER_CAT(PRESET_ARITHMETIC_LOGARITHM, "对数运算：计算以 b 为底 a 的对数 log_b(a)",
                                BASIC_MATH_DEFAULT_CATEGORY, inputs, 2,
                                PRESET_TYPE_SCALAR, "c = \\log_b(a), \\quad a > 0, b > 0, b \\neq 1", "O(1)",
                                true, false)) {
            success_count++;
        }
    }

    /* -------------------- 模运算 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER};
        if (PRESET_REGISTER_CAT(PRESET_ARITHMETIC_MODULAR, "模运算：计算 a 除以 n 的余数 a mod n",
                                BASIC_MATH_DEFAULT_CATEGORY, inputs, 2,
                                PRESET_TYPE_INTEGER, "r = a \\bmod n, \\quad n \\neq 0", "O(1)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第二部分：数论基础
     * ============================================================ */

    /* -------------------- 最大公约数 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER};
        if (PRESET_REGISTER_CAT(PRESET_NUMBER_GCD, "最大公约数：使用欧几里得算法计算 gcd(a, b)",
                                PRESET_CATEGORY_NUMBER_THEORY, inputs, 2,
                                PRESET_TYPE_INTEGER,
                                "\\gcd(a, b) = \\max\\{d \\in \\mathbb{Z}^+ : d|a \\land d|b\\}",
                                "O(\\log \\min(a,b))", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 最小公倍数 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER};
        if (PRESET_REGISTER_CAT(PRESET_NUMBER_LCM, "最小公倍数：计算 lcm(a, b) = |ab| / gcd(a, b)",
                                PRESET_CATEGORY_NUMBER_THEORY, inputs, 2,
                                PRESET_TYPE_INTEGER, "\\text{lcm}(a, b) = \\frac{|a \\cdot b|}{\\gcd(a, b)}",
                                "O(\\log \\min(a,b))", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 素性检测 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER};
        if (PRESET_REGISTER_CAT(PRESET_NUMBER_PRIME_CHECK,
                                "素性检测：判断整数 n 是否为素数（Miller-Rabin算法）",
                                PRESET_CATEGORY_NUMBER_THEORY, inputs, 1,
                                PRESET_TYPE_BOOLEAN,
                                "\\text{isPrime}(n) = \\begin{cases} true & n \\text{ is prime} \\\\ false & "
                                "\\text{otherwise} \\end{cases}",
                                "O(k \\log^3 n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 质因数分解 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER};
        if (PRESET_REGISTER_CAT(PRESET_NUMBER_PRIME_FACTORIZATION,
                                "质因数分解：将整数 n 分解为素数因子的乘积",
                                PRESET_CATEGORY_NUMBER_THEORY, inputs, 1, PRESET_TYPE_LIST,
                                "n = p_1^{e_1} \\cdot p_2^{e_2} \\cdots p_k^{e_k}", "O(\\sqrt{n})", true,
                                false)) {
            success_count++;
        }
    }

    /* -------------------- 欧拉函数 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER};
        if (PRESET_REGISTER_CAT(
                PRESET_NUMBER_EULER_TOTIENT, "欧拉函数：计算不超过 n 且与 n 互素的正整数个数 phi(n)",
                PRESET_CATEGORY_NUMBER_THEORY, inputs, 1,
                PRESET_TYPE_INTEGER, "\\varphi(n) = n \\prod_{p|n}\\left(1 - \\frac{1}{p}\\right)", "O(\\sqrt{n})",
                true, false)) {
            success_count++;
        }
    }

    /* -------------------- 模逆元 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER};
        if (PRESET_REGISTER_CAT(
                PRESET_NUMBER_MODULAR_INVERSE, "模逆元：求解 a 在模 n 下的乘法逆元 a^{-1} mod n",
                PRESET_CATEGORY_NUMBER_THEORY, inputs, 2,
                PRESET_TYPE_INTEGER, "a \\cdot a^{-1} \\equiv 1 \\pmod{n}, \\quad \\gcd(a, n) = 1", "O(\\log n)", true,
                true)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第三部分：组合计数
     * ============================================================ */

    /* -------------------- 排列数 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER};
        if (PRESET_REGISTER_CAT(PRESET_COMBIN_PERMUTATION, "排列数：从 n 个元素中取 k 个的排列数 P(n, k)",
                                PRESET_CATEGORY_COMBINATORICS, inputs, 2,
                                PRESET_TYPE_INTEGER,
                                "P(n, k) = \\frac{n!}{(n-k)!}, \\quad 0 \\leq k \\leq n", "O(k)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 组合数 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER};
        if (PRESET_REGISTER_CAT(PRESET_COMBIN_COMBINATION, "组合数：从 n 个元素中取 k 个的组合数 C(n, k)",
                                PRESET_CATEGORY_COMBINATORICS, inputs, 2,
                                PRESET_TYPE_INTEGER,
                                "\\binom{n}{k} = \\frac{n!}{k!(n-k)!}, \\quad 0 \\leq k \\leq n", "O(k)",
                                true, false)) {
            success_count++;
        }
    }

    /* -------------------- 第一类Stirling数 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER};
        if (PRESET_REGISTER_CAT(PRESET_COMBIN_STIRLING_FIRST,
                                "第一类Stirling数：将 n 个元素划分为 k 个非空轮换的方式数 s(n, k)",
                                BASIC_MATH_DEFAULT_CATEGORY, inputs, 2,
                                PRESET_TYPE_INTEGER, "s(n, k) = s(n-1, k-1) - (n-1) \\cdot s(n-1, k)", "O(nk)",
                                true, false)) {
            success_count++;
        }
    }

    /* -------------------- 第二类Stirling数 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER};
        if (PRESET_REGISTER_CAT(
                PRESET_COMBIN_STIRLING_SECOND, "第二类Stirling数：将 n 个元素划分为 k 个非空子集的方式数 S(n, k)",
                BASIC_MATH_DEFAULT_CATEGORY, inputs, 2,
                PRESET_TYPE_INTEGER, "S(n, k) = S(n-1, k-1) + k \\cdot S(n-1, k)", "O(nk)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 整数分拆数 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER};
        if (PRESET_REGISTER_CAT(PRESET_COMBIN_PARTITION,
                                "整数分拆数：将正整数 n 表示为正整数之和的方式数 p(n)",
                                PRESET_CATEGORY_COMBINATORICS, inputs, 1,
                                PRESET_TYPE_INTEGER, "p(n) = p(n-1) + p(n-2) - p(n-5) - p(n-7) + \\cdots",
                                "O(n \\sqrt{n})", true, false)) {
            success_count++;
        }
    }

    /* -------------------- Catalan数 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER};
        if (PRESET_REGISTER_CAT(PRESET_COMBIN_CATALAN, "Catalan数：第 n 个Catalan数 C_n",
                                PRESET_CATEGORY_COMBINATORICS, inputs, 1,
                                PRESET_TYPE_INTEGER,
                                "C_n = \\frac{1}{n+1}\\binom{2n}{n} = \\frac{(2n)!}{(n+1)! \\, n!}", "O(n)",
                                true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第四部分：数列与级数
     * ============================================================ */

    /* -------------------- 等差数列求和 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER};
        if (PRESET_REGISTER_CAT(PRESET_SEQUENCE_ARITHMETIC_SUM, "等差数列求和：S_n = n/2 * (2a_1 + (n-1)d)",
                                BASIC_MATH_DEFAULT_CATEGORY, inputs, 3,
                                PRESET_TYPE_SCALAR,
                                "S_n = \\frac{n}{2}(2a_1 + (n-1)d) = \\frac{n(a_1 + a_n)}{2}", "O(1)", true,
                                false)) {
            success_count++;
        }
    }

    /* -------------------- 等比数列求和 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER};
        if (PRESET_REGISTER_CAT(
                PRESET_SEQUENCE_GEOMETRIC_SUM, "等比数列求和：S_n = a_1(1 - r^n) / (1 - r)", BASIC_MATH_DEFAULT_CATEGORY,
                inputs, 3,
                PRESET_TYPE_SCALAR, "S_n = \\frac{a_1(1 - r^n)}{1 - r}, \\quad r \\neq 1", "O(\\log n)", true,
                false)) {
            success_count++;
        }
    }

    /* -------------------- Fibonacci数列 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER};
        if (PRESET_REGISTER_CAT(
                PRESET_SEQUENCE_FIBONACCI, "Fibonacci数列第n项：F_n = F_{n-1} + F_{n-2}", BASIC_MATH_DEFAULT_CATEGORY,
                inputs, 1,
                PRESET_TYPE_INTEGER,
                "F_n = \\frac{\\varphi^n - \\psi^n}{\\sqrt{5}}, \\quad \\varphi = \\frac{1+\\sqrt{5}}{2}", "O(\\log n)",
                true, false)) {
            success_count++;
        }
    }

    /* -------------------- 二项式系数 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER};
        if (PRESET_REGISTER_CAT(
                PRESET_SEQUENCE_BINOMIAL_COEFFICIENT, "二项式系数：展开 (x + y)^n 中 x^k y^{n-k} 的系数",
                BASIC_MATH_DEFAULT_CATEGORY, inputs, 2,
                PRESET_TYPE_INTEGER, "\\binom{n}{k} = \\frac{n!}{k!(n-k)!}", "O(k)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 调和数 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER};
        if (PRESET_REGISTER_CAT(PRESET_SEQUENCE_HARMONIC_NUMBER, "调和数：H_n = 1 + 1/2 + 1/3 + ... + 1/n",
                                BASIC_MATH_DEFAULT_CATEGORY, inputs, 1,
                                PRESET_TYPE_SCALAR,
                                "H_n = \\sum_{k=1}^{n} \\frac{1}{k} \\approx \\ln n + \\gamma", "O(n)", true,
                                false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第五部分：特殊函数
     * ============================================================ */

    /* -------------------- 阶乘 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER};
        if (PRESET_REGISTER_CAT(PRESET_SPECIAL_FACTORIAL, "阶乘：计算非负整数 n 的阶乘 n!",
                                BASIC_MATH_DEFAULT_CATEGORY, inputs, 1,
                                PRESET_TYPE_INTEGER, "n! = \\prod_{k=1}^{n} k, \\quad 0! = 1", "O(n)", true,
                                false)) {
            success_count++;
        }
    }

    /* -------------------- Gamma函数 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR};
        if (PRESET_REGISTER_CAT(
                PRESET_SPECIAL_GAMMA, "Gamma函数：阶乘的推广，Gamma(z) = integral_0^inf t^{z-1} e^{-t} dt",
                BASIC_MATH_DEFAULT_CATEGORY, inputs, 1,
                PRESET_TYPE_SCALAR,
                "\\Gamma(z) = \\int_0^{\\infty} t^{z-1} e^{-t} \\, dt, \\quad \\Gamma(n) = (n-1)!",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- Beta函数 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (PRESET_REGISTER_CAT(
                PRESET_SPECIAL_BETA, "Beta函数：B(x, y) = integral_0^1 t^{x-1}(1-t)^{y-1} dt",
                BASIC_MATH_DEFAULT_CATEGORY, inputs, 2,
                PRESET_TYPE_SCALAR,
                "B(x, y) = \\int_0^1 t^{x-1}(1-t)^{y-1} \\, dt = \\frac{\\Gamma(x)\\Gamma(y)}{\\Gamma(x+y)}", "O(1)",
                true, false)) {
            success_count++;
        }
    }

    /* -------------------- Bernoulli数 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER};
        if (PRESET_REGISTER_CAT(
                PRESET_SPECIAL_BERNOULLI, "Bernoulli数：第 n 个Bernoulli数 B_n", BASIC_MATH_DEFAULT_CATEGORY, inputs, 1,
                PRESET_TYPE_SCALAR,
                "\\frac{t}{e^t - 1} = \\sum_{n=0}^{\\infty} B_n \\frac{t^n}{n!}", "O(n^2)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- Legendre多项式 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER, PRESET_TYPE_SCALAR};
        if (PRESET_REGISTER_CAT(
                PRESET_SPECIAL_LEGENDRE, "Legendre多项式：计算 n 阶Legendre多项式在 x 处的值 P_n(x)",
                BASIC_MATH_DEFAULT_CATEGORY, inputs, 2,
                PRESET_TYPE_SCALAR, "P_n(x) = \\frac{1}{2^n n!} \\frac{d^n}{dx^n}[(x^2-1)^n]", "O(n)", true, false)) {
            success_count++;
        }
    }

    /* 基础数学模块预设注册完成 */
    (void) success_count;
    (void) BASIC_MATH_PRESET_COUNT;

    /* 返回是否所有预设都注册成功 */
    return success_count == BASIC_MATH_PRESET_COUNT;
}

/**
 * @brief 获取基础数学预设函数块数量
 *
 * @return int 基础数学模块预设函数块总数
 */
int preset_basic_math_count(void) {
    return BASIC_MATH_PRESET_COUNT;
}

/**
 * @brief 获取基础数学模块的预设类别
 *
 * @return PresetCategory 基础数学模块所属类别
 */
PresetCategory preset_basic_math_category(void) {
    return PRESET_CATEGORY_ANALYSIS;
}

/**
 * @brief 获取基础数学预设函数块名称列表
 *
 * @param out_names 输出名称数组（需调用者释放）
 * @param out_count 输出名称数量
 * @return true 获取成功
 * @return false 参数无效或内存不足
 */
bool preset_basic_math_get_names(char ***out_names, int *out_count) {
    if (!out_names || !out_count)
        return false;

    /* 分配名称数组 */
    char **names = (char **) lv00_malloc(BASIC_MATH_PRESET_COUNT * sizeof(char *));
    if (!names)
        return false;

    /* 填充预设名称列表 */
    const char *preset_names[] = {
        /* 算术运算 */
        PRESET_ARITHMETIC_ADD,
        PRESET_ARITHMETIC_SUBTRACT,
        PRESET_ARITHMETIC_MULTIPLY,
        PRESET_ARITHMETIC_DIVIDE,
        PRESET_ARITHMETIC_POWER,
        PRESET_ARITHMETIC_ROOT,
        PRESET_ARITHMETIC_LOGARITHM,
        PRESET_ARITHMETIC_MODULAR,
        /* 数论基础 */
        PRESET_NUMBER_GCD,
        PRESET_NUMBER_LCM,
        PRESET_NUMBER_PRIME_CHECK,
        PRESET_NUMBER_PRIME_FACTORIZATION,
        PRESET_NUMBER_EULER_TOTIENT,
        PRESET_NUMBER_MODULAR_INVERSE,
        /* 组合计数 */
        PRESET_COMBIN_PERMUTATION,
        PRESET_COMBIN_COMBINATION,
        PRESET_COMBIN_STIRLING_FIRST,
        PRESET_COMBIN_STIRLING_SECOND,
        PRESET_COMBIN_PARTITION,
        PRESET_COMBIN_CATALAN,
        /* 数列与级数 */
        PRESET_SEQUENCE_ARITHMETIC_SUM,
        PRESET_SEQUENCE_GEOMETRIC_SUM,
        PRESET_SEQUENCE_FIBONACCI,
        PRESET_SEQUENCE_BINOMIAL_COEFFICIENT,
        PRESET_SEQUENCE_HARMONIC_NUMBER,
        /* 特殊函数 */
        PRESET_SPECIAL_FACTORIAL,
        PRESET_SPECIAL_GAMMA,
        PRESET_SPECIAL_BETA,
        PRESET_SPECIAL_BERNOULLI,
        PRESET_SPECIAL_LEGENDRE,
    };

    int count = (int) (sizeof(preset_names) / sizeof(preset_names[0]));

    for (int i = 0; i < count; i++) {
        names[i] = lv00_strdup(preset_names[i]);
        if (names[i] == NULL) {
            /* 释放已分配的内存 */
            for (int j = 0; j < i; j++) {
                char *p = names[j];
                lv00_free((void **)&p);
                names[j] = NULL;
            }
            {
                lv00_free((void **)&names);
            }
            return false;
        }
    }

    *out_names = names;
    *out_count = count;
    return true;
}

/**
 * @file preset_number_theory.c
 * @brief 数论预设函数块 - 实现
 *
 * 实现理论数学研究中常用的数论运算预设函数块。
 * 所有预设函数块都遵循模块化、确定性原则。
 *
 * @module NumberTheory
 * @category PRESET_CATEGORY_NUMBER_THEORY
 * @version 5.0.0
 */

/*
 * ============================================================
 * 头文件包含说明
 * ============================================================
 * preset_number_theory.h -> preset_blocks.h -> func_block_registry.h
 *   -> 提供 PresetType 枚举、preset_blocks_register_simple() 声明
 *   -> 提供 PresetCategory 枚举（PRESET_CATEGORY_NUMBER_THEORY 等）
 * preset_common.h
 *   -> 提供 PRESET_REGISTER 等宏、preset_register_common() 内联函数
 *   -> 提供 PRESET_SAFE_MALLOC 等安全内存操作宏
 * lv00_internal.h / lv00_utils.h
 *   -> 提供 lv00_malloc、lv00_free、lv00_strdup、lv00_log_* 等
 * ============================================================
 */
#include "preset_number_theory.h"
#include "preset_blocks.h"
#include "preset_common.h"     /* 预设公共宏与辅助函数（PRESET_ERROR_LOG 等） */
#include "lv00_internal.h"
#include "lv00_utils.h"

#include <string.h>

/* ==================== 预设函数块数量 ==================== */

/** 数论模块预设函数块总数 */

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 注册单个数论预设
 *
 * 辅助函数，简化预设注册过程。
 * 所有数论预设都属于 PRESET_CATEGORY_NUMBER_THEORY 类别。
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
static bool register_number_theory_preset(
    const char *name,
    const char *description,
    const PresetType *input_types,
    int input_count,
    PresetType output_type,
    const char *math_def,
    const char *complexity,
    bool is_constructive,
    bool is_reversible)
{
    return preset_blocks_register_simple(
        name, description,
        PRESET_CATEGORY_NUMBER_THEORY,
        input_types, input_count, output_type,
        math_def, complexity,
        is_constructive, is_reversible);
}

/* ==================== 模块注册实现 ==================== */

bool preset_number_theory_register(void)
{
    int success_count = 0;

    /* ============================================================
     * 第一部分：基础整数运算
     * ============================================================ */

    /* -------------------- 最大公约数 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER};
        if (register_number_theory_preset(
                PRESET_GCD,
                "计算两个整数的最大公约数（欧几里得算法）",
                inputs, 2, PRESET_TYPE_INTEGER,
                "\\gcd(a, b) = \\max\\{d : d|a \\land d|b\\}",
                "O(log min(a,b))", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 最小公倍数 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER};
        if (register_number_theory_preset(
                PRESET_LCM,
                "计算两个整数的最小公倍数",
                inputs, 2, PRESET_TYPE_INTEGER,
                "\\text{lcm}(a, b) = \\frac{|a \\cdot b|}{\\gcd(a, b)}",
                "O(log min(a,b))", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 扩展欧几里得 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER};
        if (register_number_theory_preset(
                PRESET_EXTENDED_GCD,
                "扩展欧几里得算法：求解 ax + by = gcd(a,b) 的整数解",
                inputs, 2, PRESET_TYPE_TUPLE,
                "\\exists x, y \\in \\mathbb{Z}: ax + by = \\gcd(a,b)",
                "O(log min(a,b))", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 模逆元 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER};
        if (register_number_theory_preset(
                PRESET_MODULAR_INVERSE,
                "计算 a 模 m 的乘法逆元（当 gcd(a,m)=1 时存在）",
                inputs, 2, PRESET_TYPE_INTEGER,
                "a^{-1} \\mod m \\text{ 满足 } a \\cdot a^{-1} \\equiv 1 \\pmod{m}",
                "O(log m)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 模幂运算 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER};
        if (register_number_theory_preset(
                PRESET_MODULAR_EXPONENTIATION,
                "快速模幂运算：计算 a^b mod m（平方-乘算法）",
                inputs, 3, PRESET_TYPE_INTEGER,
                "a^b \\mod m",
                "O(log b)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第二部分：素数相关
     * ============================================================ */

    /* -------------------- 素性检测 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER};
        if (register_number_theory_preset(
                PRESET_PRIMALITY_TEST,
                "Miller-Rabin 素性检测（概率性算法）",
                inputs, 2, PRESET_TYPE_BOOLEAN,
                "n \\text{ 是素数 } \\Rightarrow \\text{返回真}",
                "O(k log³ n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 素因子分解 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER};
        if (register_number_theory_preset(
                PRESET_PRIME_FACTORIZATION,
                "整数的素因子分解",
                inputs, 1, PRESET_TYPE_TUPLE,
                "n = p_1^{e_1} \\cdot p_2^{e_2} \\cdots p_k^{e_k}",
                "O(√n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 下一个素数 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER};
        if (register_number_theory_preset(
                PRESET_NEXT_PRIME,
                "生成大于 n 的最小素数",
                inputs, 1, PRESET_TYPE_PRIME,
                "p = \\min\\{q > n : q \\text{ 是素数}\\}",
                "O(√p log p)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 欧拉筛法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER};
        if (register_number_theory_preset(
                PRESET_SIEVE_OF_ERATOSTHENES,
                "欧拉筛法生成不超过 n 的所有素数",
                inputs, 1, PRESET_TYPE_SEQUENCE,
                "\\{p \\le n : p \\text{ 是素数}\\}",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第三部分：同余运算
     * ============================================================ */

    /* -------------------- 中国剩余定理 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SEQUENCE, PRESET_TYPE_SEQUENCE};
        if (register_number_theory_preset(
                PRESET_CHINESE_REMAINDER,
                "中国剩余定理：求解同余方程组",
                inputs, 2, PRESET_TYPE_INTEGER,
                "x \\equiv a_i \\pmod{m_i}, \\quad \\gcd(m_i, m_j) = 1",
                "O(n log M)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 离散对数 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER};
        if (register_number_theory_preset(
                PRESET_DISCRETE_LOGARITHM,
                "离散对数：求解 g^x ≡ h (mod p)（Shanks 算法）",
                inputs, 3, PRESET_TYPE_INTEGER,
                "g^x \\equiv h \\pmod{p}",
                "O(√p)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 乘法阶 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER};
        if (register_number_theory_preset(
                PRESET_MULTIPLICATIVE_ORDER,
                "计算 a 模 m 的乘法阶",
                inputs, 2, PRESET_TYPE_INTEGER,
                "\\text{ord}_m(a) = \\min\\{k > 0 : a^k \\equiv 1 \\pmod{m}\\}",
                "O(φ(m) log m)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 原根 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER};
        if (register_number_theory_preset(
                PRESET_PRIMITIVE_ROOT,
                "检测并生成模 p 的原根",
                inputs, 1, PRESET_TYPE_INTEGER,
                "g \\text{ 是 } \\mathbb{Z}_p^* \\text{ 的原根} \\Leftrightarrow \\text{ord}_p(g) = p-1",
                "O(p log² p)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第四部分：数论函数
     * ============================================================ */

    /* -------------------- 欧拉函数 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER};
        if (register_number_theory_preset(
                PRESET_EULER_TOTIENT,
                "欧拉函数：计算不超过 n 且与 n 互质的正整数个数",
                inputs, 1, PRESET_TYPE_INTEGER,
                "\\varphi(n) = |\\{k : 1 \\le k \\le n, \\gcd(k,n) = 1\\}|",
                "O(√n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 莫比乌斯函数 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER};
        if (register_number_theory_preset(
                PRESET_MOBIUS_FUNCTION,
                "莫比乌斯函数：数论中的重要积性函数",
                inputs, 1, PRESET_TYPE_INTEGER,
                "\\mu(n) = \\begin{cases} 1 & n=1 \\\\ 0 & \\exists p: p^2|n \\\\ (-1)^k & n = p_1 \\cdots p_k \\end{cases}",
                "O(√n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 约数个数 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER};
        if (register_number_theory_preset(
                PRESET_DIVISOR_COUNT,
                "计算正整数 n 的正约数个数",
                inputs, 1, PRESET_TYPE_INTEGER,
                "d(n) = \\sum_{d|n} 1 = \\prod_{i=1}^{k} (e_i + 1)",
                "O(√n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 约数和 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER};
        if (register_number_theory_preset(
                PRESET_DIVISOR_SUM,
                "计算正整数 n 的所有正约数之和",
                inputs, 1, PRESET_TYPE_INTEGER,
                "\\sigma(n) = \\sum_{d|n} d = \\prod_{i=1}^{k} \\frac{p_i^{e_i+1}-1}{p_i-1}",
                "O(√n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 最大真约数 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER};
        if (register_number_theory_preset(
                PRESET_LARGEST_PROPER_DIVISOR,
                "计算 n 的最大真约数（不含 n 本身）",
                inputs, 1, PRESET_TYPE_INTEGER,
                "\\max\\{d : d|n, d < n\\}",
                "O(√n)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第五部分：二次剩余
     * ============================================================ */

    /* -------------------- 勒让德符号 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER, PRESET_TYPE_PRIME};
        if (register_number_theory_preset(
                PRESET_LEGENDRE_SYMBOL,
                "勒让德符号：判定 a 是否是模 p 的二次剩余",
                inputs, 2, PRESET_TYPE_INTEGER,
                "\\left(\\frac{a}{p}\\right) = \\begin{cases} 1 & \\exists x: x^2 \\equiv a \\pmod{p} \\\\ -1 & \\text{否则} \\end{cases}",
                "O(log p)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 雅可比符号 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER};
        if (register_number_theory_preset(
                PRESET_JACOBI_SYMBOL,
                "雅可比符号：勒让德符号的推广",
                inputs, 2, PRESET_TYPE_INTEGER,
                "\\left(\\frac{a}{n}\\right) = \\prod_{i=1}^{k} \\left(\\frac{a}{p_i}\\right)^{e_i}",
                "O(log n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 二次剩余判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER, PRESET_TYPE_PRIME};
        if (register_number_theory_preset(
                PRESET_QUADRATIC_RESIDUE_TEST,
                "判定 a 是否是模 p 的二次剩余",
                inputs, 2, PRESET_TYPE_BOOLEAN,
                "a \\text{ 是 } p \\text{ 的二次剩余} \\Leftrightarrow \\left(\\frac{a}{p}\\right) = 1",
                "O(log p)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- Tonelli-Shanks 算法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER, PRESET_TYPE_PRIME};
        if (register_number_theory_preset(
                PRESET_TONELLI_SHANKS,
                "Tonelli-Shanks 算法：求解 x² ≡ a (mod p)",
                inputs, 2, PRESET_TYPE_INTEGER,
                "x^2 \\equiv a \\pmod{p}",
                "O(log² p)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第六部分：特殊数列与方程
     * ============================================================ */

    /* -------------------- 斐波那契数 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER};
        if (register_number_theory_preset(
                PRESET_FIBONACCI,
                "计算第 n 个斐波那契数",
                inputs, 1, PRESET_TYPE_INTEGER,
                "F_n = F_{n-1} + F_{n-2}, \\quad F_0 = 0, F_1 = 1",
                "O(log n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 卢卡斯数 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER};
        if (register_number_theory_preset(
                PRESET_LUCAS,
                "计算第 n 个卢卡斯数",
                inputs, 1, PRESET_TYPE_INTEGER,
                "L_n = L_{n-1} + L_{n-2}, \\quad L_0 = 2, L_1 = 1",
                "O(log n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 佩尔方程 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER};
        if (register_number_theory_preset(
                PRESET_PELL_EQUATION,
                "求解佩尔方程 x² - Dy² = 1 的最小正整数解",
                inputs, 1, PRESET_TYPE_TUPLE,
                "x^2 - Dy^2 = 1",
                "O(√D log D)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 模加法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER};
        if (register_number_theory_preset(
                PRESET_MODULAR_ADD,
                "模加法：(a + b) mod m",
                inputs, 3, PRESET_TYPE_RESIDUE,
                "(a + b) \\mod m",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 模乘法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER};
        if (register_number_theory_preset(
                PRESET_MODULAR_MULTIPLY,
                "模乘法：(a * b) mod m",
                inputs, 3, PRESET_TYPE_RESIDUE,
                "(a \\cdot b) \\mod m",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 威尔逊定理检验 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER};
        if (register_number_theory_preset(
                PRESET_WILSON_TEST,
                "威尔逊定理：n 是素数当且仅当 (n-1)! ≡ -1 (mod n)",
                inputs, 1, PRESET_TYPE_BOOLEAN,
                "n \\text{ 是素数} \\Leftrightarrow (n-1)! \\equiv -1 \\pmod{n}",
                "O(n log n)", true, false)) {
            success_count++;
        }
    }

    /* 返回是否所有预设都注册成功 */
    /* lv00_log_info("数论预设注册完成，共 %d 个预设", success_count) */
    return success_count == NUMBER_THEORY_PRESET_COUNT;
}

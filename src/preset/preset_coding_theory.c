/**
 * @file preset_coding_theory.c
 * @brief 编码理论预设函数块模块 - 实现（v2统一宏模式）
 *
 * 实现理论数学研究中常用的编码理论运算预设函数块。
 * 涵盖线性码、循环码与BCH码、码的界与性能、编码应用。
 * 共18个预设函数块，均遵循模块化、确定性原则。
 *
 * @module CodingTheory
 * @category PRESET_CATEGORY_ALGEBRAIC
 * @version 5.0.0
 */

#include "preset_coding_theory.h"

#include <stdlib.h>
#include <string.h>

#include "lv00_internal.h"
#include "lv00_utils.h"
#include "preset_blocks.h"
#include "preset_common.h"

/* ==================== 预设函数块数量 ==================== */

/** 编码理论模块预设函数块总数 */
#define CODING_THEORY_PRESET_COUNT 18

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 注册单个编码理论预设
 *
 * 辅助函数，简化预设注册过程。
 * 所有编码理论预设都属于 PRESET_CATEGORY_ALGEBRAIC 类别。
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
static bool register_coding_theory_preset(const char *name, const char *description, const PresetType *input_types,
                                          int input_count, PresetType output_type, const char *math_def,
                                          const char *complexity, bool is_constructive, bool is_reversible) {
    return preset_blocks_register_simple(name, description, PRESET_CATEGORY_ALGEBRAIC, input_types, input_count,
                                         output_type, math_def, complexity, is_constructive, is_reversible);
}

/* ==================== v2统一注册宏 ==================== */

/**
 * @brief 编码理论预设统一注册宏
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
#define REGISTER_CT(name, desc, inputs, in_count, output, math, comp, cons, rev)                                  \
    do {                                                                                                          \
        if (register_coding_theory_preset((name), (desc), (inputs), (in_count), (output), (math), (comp), (cons), \
                                          (rev))) {                                                               \
            success_count++;                                                                                      \
        } else {                                                                                                  \
            /* PRESET_ERROR_LOG("注册预设失败: %s", (name)); */                                                   \
        }                                                                                                         \
    } while (0)

/* ==================== 模块注册实现 ==================== */

bool preset_coding_theory_register(void) {
    int success_count = 0;

    /* ============================================================
     * 第一部分：线性码（6个）
     * ============================================================ */

    /**
     * @brief ct_linear_code_construct - 线性码构造
     *
     * 由生成矩阵G构造二元线性码 [n, k]。
     * 生成矩阵G为 k×n 矩阵（行满秩），码字空间 C = {xG : x in F_2^k}。
     * 码字数量为 2^k，码长为 n，维数为 k。
     * 线性码的最小距离等于码字的最小Hamming重量。
     *
     * @param G 生成矩阵（PRESET_TYPE_MATRIX，k×n）
     * @param q 有限域阶数（PRESET_TYPE_INTEGER）
     * @return 线性码参数 [n, k, d]（PRESET_TYPE_TUPLE）
     * @math \\mathcal{C} = \\{xG : x \\in \\mathbb{F}_q^k\\}, \\quad |\\mathcal{C}| = q^k
     * @complexity O(n^3)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX, PRESET_TYPE_INTEGER};
        REGISTER_CT("ct_linear_code_construct", "线性码构造：由生成矩阵G构造线性码 [n, k]，码字空间 C = {xG}", inputs,
                    2, PRESET_TYPE_TUPLE,
                    "\\mathcal{C} = \\{xG : x \\in \\mathbb{F}_q^k\\}, \\quad |\\mathcal{C}| = q^k", "O(n^3)", true,
                    false);
    }

    /**
     * @brief ct_hamming_code - Hamming码构造
     *
     * 构造二元Hamming码 Ham(r, 2)，参数为 [n=2^r-1, k=2^r-1-r, d=3]。
     * Hamming码是完备码，恰好满足Hamming界等号。
     * 校验矩阵H的列由所有非零r维向量组成。
     * 能纠正单个错误，是纠错码的经典构造。
     *
     * @param r 校验位数（PRESET_TYPE_INTEGER）
     * @return Hamming码参数 [n, k, d] 和校验矩阵（PRESET_TYPE_TUPLE）
     * @math \\text{Ham}(r, 2): n = 2^r - 1, \\; k = 2^r - 1 - r, \\; d = 3
     * @complexity O(2^r)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER};
        REGISTER_CT("ct_hamming_code", "Hamming码构造：构造Ham(r,2)码，参数 [2^r-1, 2^r-1-r, 3]，完备单纠错码", inputs,
                    1, PRESET_TYPE_TUPLE, "\\text{Ham}(r, 2): n = 2^r - 1, \\; k = 2^r - 1 - r, \\; d = 3", "O(2^r)",
                    true, false);
    }

    /**
     * @brief ct_parity_check - 奇偶校验矩阵
     *
     * 由生成矩阵G计算奇偶校验矩阵H。
     * 对于 [n, k] 线性码，G为 k×n 矩阵，H为 (n-k)×n 矩阵。
     * 满足 G·H^T = 0（零矩阵），即码字正交于校验矩阵的行空间。
     * 码字 c 属于码 C 当且仅当 H·c^T = 0。
     *
     * @param G 生成矩阵（PRESET_TYPE_MATRIX，k×n）
     * @param q 有限域阶数（PRESET_TYPE_INTEGER）
     * @return 奇偶校验矩阵H（PRESET_TYPE_MATRIX）
     * @math GH^T = 0, \\quad c \\in \\mathcal{C} \\Leftrightarrow Hc^T = 0
     * @complexity O(n^3)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX, PRESET_TYPE_INTEGER};
        REGISTER_CT("ct_parity_check", "奇偶校验矩阵：由生成矩阵G计算校验矩阵H，满足 GH^T = 0", inputs, 2,
                    PRESET_TYPE_MATRIX, "GH^T = 0, \\quad c \\in \\mathcal{C} \\Leftrightarrow Hc^T = 0", "O(n^3)",
                    true, false);
    }

    /**
     * @brief ct_syndrome_decode - 伴随式译码
     *
     * 计算接收向量r的伴随式 s = H·r^T 并进行译码。
     * 伴随式 s = H·r^T = H·e^T（其中 e = r - c 为错误向量）。
     * 若错误重量不超过 t = floor((d-1)/2)，则伴随式唯一确定错误模式。
     * 译码步骤：计算伴随式 → 查标准阵列 → 纠错。
     *
     * @param r 接收向量（PRESET_TYPE_VECTOR）
     * @param H 校验矩阵（PRESET_TYPE_MATRIX）
     * @param syndrome_table 伴随式译码表（PRESET_TYPE_MATRIX）
     * @return 译码后的码字（PRESET_TYPE_VECTOR）
     * @math s = Hr^T, \\quad \\hat{c} = r - e(s), \\quad \\text{wt}(e) \\leq t = \\lfloor(d-1)/2\\rfloor
     * @complexity O(n^2)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_VECTOR, PRESET_TYPE_MATRIX, PRESET_TYPE_MATRIX};
        REGISTER_CT("ct_syndrome_decode", "伴随式译码：计算伴随式 s = H·r^T 并查表译码，纠正不超过t个错误", inputs, 3,
                    PRESET_TYPE_VECTOR,
                    "s = Hr^T, \\quad \\hat{c} = r - e(s), \\quad \\text{wt}(e) \\leq t = \\lfloor(d-1)/2\\rfloor",
                    "O(n^2)", true, false);
    }

    /**
     * @brief ct_code_distance - 码距计算
     *
     * 计算线性码的最小Hamming距离 d。
     * 对于线性码，最小距离等于非零码字的最小Hamming重量。
     * d = min{wt(c) : c in C, c ≠ 0}。
     * 最小距离决定码的纠错能力 t = floor((d-1)/2) 和检错能力 d-1。
     *
     * @param G 生成矩阵（PRESET_TYPE_MATRIX）
     * @param q 有限域阶数（PRESET_TYPE_INTEGER）
     * @return 最小距离 d（PRESET_TYPE_INTEGER）
     * @math d = \\min\\{\\text{wt}(c) : c \\in \\mathcal{C}, c \\neq 0\\}
     * @complexity O(q^k · n)（穷举法），可用改进算法
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX, PRESET_TYPE_INTEGER};
        REGISTER_CT("ct_code_distance", "码距计算：计算线性码的最小Hamming距离 d = min{wt(c) : c ≠ 0}", inputs, 2,
                    PRESET_TYPE_INTEGER, "d = \\min\\{\\text{wt}(c) : c \\in \\mathcal{C}, c \\neq 0\\}",
                    "O(q^k \\cdot n)", true, false);
    }

    /**
     * @brief ct_code_dimension - 码的维数
     *
     * 计算线性码的维数k和信息率 R = k/n。
     * 维数k等于生成矩阵G的行秩。
     * 信息率R = k/n是每个信道符号携带的信息比特数。
     * 对于 [n, k, d] 码，信息率越高表示编码效率越高。
     *
     * @param G 生成矩阵（PRESET_TYPE_MATRIX）
     * @return 维数k和信息率R = k/n（PRESET_TYPE_TUPLE）
     * @math k = \\text{rank}(G), \\quad R = k/n
     * @complexity O(n^3)（矩阵行简化）
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        REGISTER_CT("ct_code_dimension", "码的维数：计算线性码的维数 k = rank(G) 和信息率 R = k/n", inputs, 1,
                    PRESET_TYPE_TUPLE, "k = \\text{rank}(G), \\quad R = k/n", "O(n^3)", true, false);
    }

    /* ============================================================
     * 第二部分：循环码与BCH码（4个）
     * ============================================================ */

    /**
     * @brief ct_cyclic_code_construct - 循环码构造
     *
     * 由生成多项式g(x)构造循环码。
     * 循环码的条件：若 c(x) in C，则 x·c(x) mod (x^n - 1) in C。
     * 生成多项式 g(x) 整除 x^n - 1，码的维数 k = n - deg(g(x))。
     * 校验多项式 h(x) = (x^n - 1) / g(x)。
     *
     * @param g_x 生成多项式系数（PRESET_TYPE_POLYNOMIAL）
     * @param n 码长（PRESET_TYPE_INTEGER）
     * @return 循环码参数和生成矩阵（PRESET_TYPE_TUPLE）
     * @math g(x) \\mid (x^n - 1), \\quad \\mathcal{C} = \\langle g(x) \\rangle, \\quad k = n - \\deg(g)
     * @complexity O(n^2)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_POLYNOMIAL, PRESET_TYPE_INTEGER};
        REGISTER_CT("ct_cyclic_code_construct", "循环码构造：由生成多项式g(x)构造循环码，g(x) | (x^n - 1)", inputs, 2,
                    PRESET_TYPE_TUPLE,
                    "g(x) \\mid (x^n - 1), \\quad \\mathcal{C} = \\langle g(x) \\rangle, \\quad k = n - \\deg(g)",
                    "O(n^2)", true, false);
    }

    /**
     * @brief ct_bch_code_construct - BCH码构造
     *
     * 构造BCH码（Bose-Chaudhuri-Hocquenghem码）。
     * BCH码是纠正多个错误的循环码，设计距离为 δ。
     * 码长 n = q^m - 1（本原BCH码），维数 k >= n - m(δ-1)。
     * 最小距离 d >= δ，纠错能力 t = floor((δ-1)/2)。
     * 生成多项式 g(x) = lcm(m_α^b(x), m_α^{b+1}(x), ..., m_α^{b+δ-2}(x))。
     *
     * @param n 码长（PRESET_TYPE_INTEGER）
     * @param delta 设计距离（PRESET_TYPE_INTEGER）
     * @param q 有限域阶数（PRESET_TYPE_INTEGER）
     * @return BCH码参数 [n, k, d] 和生成多项式（PRESET_TYPE_TUPLE）
     * @math g(x) = \\text{lcm}(m_{\\alpha^b}(x), \\ldots, m_{\\alpha^{b+\\delta-2}}(x)), \\quad d \\geq \\delta
     * @complexity O(n^2 log n)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER};
        REGISTER_CT(
            "ct_bch_code_construct", "BCH码构造：构造设计距离δ的BCH码，最小距离 d >= δ，纠错能力 t = floor((δ-1)/2)",
            inputs, 3, PRESET_TYPE_TUPLE,
            "g(x) = \\text{lcm}(m_{\\alpha^b}(x), \\ldots, m_{\\alpha^{b+\\delta-2}}(x)), \\quad d \\geq \\delta",
            "O(n^2 \\log n)", true, false);
    }

    /**
     * @brief ct_reed_solomon_construct - Reed-Solomon码构造
     *
     * 构造Reed-Solomon码 RS(n, k)，是BCH码的特例。
     * RS码在 q 元域上定义，码长 n = q - 1，维数 k。
     * 最小距离 d = n - k + 1，达到Singleton界（MDS码）。
     * 广泛应用于CD/DVD、QR码、深空通信等领域。
     *
     * @param n 码长 n = q - 1（PRESET_TYPE_INTEGER）
     * @param k 维数（PRESET_TYPE_INTEGER）
     * @param q 有限域阶数（PRESET_TYPE_INTEGER）
     * @return RS码参数 [n, k, n-k+1]（PRESET_TYPE_TUPLE）
     * @math \\text{RS}(n, k): d = n - k + 1 \\text{（MDS码）}, \\quad n = q - 1
     * @complexity O(n^2)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER};
        REGISTER_CT("ct_reed_solomon_construct",
                    "Reed-Solomon码构造：构造RS(n,k)码，d = n-k+1（MDS码），广泛应用于纠错", inputs, 3,
                    PRESET_TYPE_TUPLE, "\\text{RS}(n, k): d = n - k + 1 \\text{（MDS码）}, \\quad n = q - 1", "O(n^2)",
                    true, false);
    }

    /**
     * @brief ct_convolutional_code - 卷积码构造
     *
     * 由生成器序列构造卷积码。
     * 卷积码是 (n, k, L) 卷积码，约束长度 L，记忆长度 m。
     * 编码过程：输出是输入与生成器的卷积。
     * 译码使用Viterbi算法（最大似然译码）。
     * 自由距离决定卷积码的纠错性能。
     *
     * @param generators 生成器序列（PRESET_TYPE_MATRIX，k行n列）
     * @param constraint_length 约束长度（PRESET_TYPE_INTEGER）
     * @return 卷积码参数和网格图（PRESET_TYPE_TUPLE）
     * @math (n, k, L) \\text{ 卷积码}, \\quad d_{free} = \\min_{c \\neq 0} \\text{wt}(c)
     * @complexity O(2^{kL})（Viterbi译码）
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX, PRESET_TYPE_INTEGER};
        REGISTER_CT("ct_convolutional_code", "卷积码构造：由生成器序列构造(n,k,L)卷积码，Viterbi算法译码", inputs, 2,
                    PRESET_TYPE_TUPLE, "(n, k, L) \\text{ 卷积码}, \\quad d_{free} = \\min_{c \\neq 0} \\text{wt}(c)",
                    "O(2^{kL})", true, false);
    }

    /* ============================================================
     * 第三部分：码的界与性能（4个）
     * ============================================================ */

    /**
     * @brief ct_hamming_bound - Hamming界（球填充界）
     *
     * Hamming界（球填充界）：Σ_{i=0}^{t} C(n,i) <= 2^{n-k}。
     * 其中 t = floor((d-1)/2) 为纠错能力。
     * 直观含义：以每个码字为中心、半径为t的Hamming球互不相交。
     * 等号成立时称为完备码（如Hamming码、Golay码）。
     *
     * @param n 码长（PRESET_TYPE_INTEGER）
     * @param k 维数（PRESET_TYPE_INTEGER）
     * @param d 最小距离（PRESET_TYPE_INTEGER）
     * @return 是否满足Hamming界（PRESET_TYPE_BOOLEAN）
     * @math \\sum_{i=0}^{t} \\binom{n}{i} \\leq 2^{n-k}, \\quad t = \\lfloor(d-1)/2\\rfloor
     * @complexity O(n)
     * @constructive false
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER};
        REGISTER_CT("ct_hamming_bound", "Hamming界（球填充界）：验证 Σ C(n,i) <= 2^(n-k)，等号成立为完备码", inputs, 3,
                    PRESET_TYPE_BOOLEAN,
                    "\\sum_{i=0}^{t} \\binom{n}{i} \\leq 2^{n-k}, \\quad t = \\lfloor(d-1)/2\\rfloor", "O(n)", false,
                    false);
    }

    /**
     * @brief ct_singleton_bound - Singleton界
     *
     * Singleton界：d <= n - k + 1。
     * 任何 [n, k, d] 线性码都满足此上界。
     * 等号成立时称为最大距离可分码（MDS码），如Reed-Solomon码。
     * MDS码达到最优的纠错-效率权衡。
     *
     * @param n 码长（PRESET_TYPE_INTEGER）
     * @param k 维数（PRESET_TYPE_INTEGER）
     * @param d 最小距离（PRESET_TYPE_INTEGER）
     * @return 是否达到Singleton界（PRESET_TYPE_BOOLEAN）
     * @math d \\leq n - k + 1, \\quad \\text{等号成立} \\Leftrightarrow \\text{MDS码}
     * @complexity O(1)
     * @constructive false
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER};
        REGISTER_CT("ct_singleton_bound", "Singleton界：验证 d <= n - k + 1，等号成立为MDS码（如RS码）", inputs, 3,
                    PRESET_TYPE_BOOLEAN, "d \\leq n - k + 1, \\quad \\text{等号成立} \\Leftrightarrow \\text{MDS码}",
                    "O(1)", false, false);
    }

    /**
     * @brief ct_gilbert_varshamov_bound - Gilbert-Varshamov界
     *
     * Gilbert-Varshamov界：存在 [n, k, d] 线性码满足 Σ_{i=0}^{d-2} C(n-1,i) < 2^{n-k}。
     * 这是线性码存在性的渐近下界。
     * GV界保证了"好码"的存在性，但未给出构造方法。
     * 对于q元码：Σ_{i=0}^{d-2} C(n-1,i)(q-1)^i < q^{n-k}。
     *
     * @param n 码长（PRESET_TYPE_INTEGER）
     * @param d 最小距离（PRESET_TYPE_INTEGER）
     * @param q 有限域阶数（PRESET_TYPE_INTEGER）
     * @return 存在的码的最大维数k（PRESET_TYPE_INTEGER）
     * @math \\sum_{i=0}^{d-2} \\binom{n-1}{i}(q-1)^i < q^{n-k}
     * @complexity O(n)
     * @constructive false
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER};
        REGISTER_CT("ct_gilbert_varshamov_bound", "Gilbert-Varshamov界：计算满足存在性条件的最大维数k的下界", inputs, 3,
                    PRESET_TYPE_INTEGER, "\\sum_{i=0}^{d-2} \\binom{n-1}{i}(q-1)^i < q^{n-k}", "O(n)", false, false);
    }

    /**
     * @brief ct_plotkin_bound - Plotkin界
     *
     * Plotkin界：对于 d > n(1 - 1/q) 的 q 元码，|C| <= floor(d / (d - n(1-1/q)))。
     * Plotkin界适用于大距离码（d相对n较大的情况）。
     * 当 d > n(1 - 1/q) 时给出比Hamming界更紧的上界。
     * 对于二元码：若 d > n/2，则 |C| <= 2d / (2d - n)。
     *
     * @param n 码长（PRESET_TYPE_INTEGER）
     * @param d 最小距离（PRESET_TYPE_INTEGER）
     * @param q 有限域阶数（PRESET_TYPE_INTEGER）
     * @return 码大小的Plotkin上界（PRESET_TYPE_INTEGER）
     * @math |\\mathcal{C}| \\leq \\left\\lfloor \\frac{d}{d - n(1-1/q)} \\right\\rfloor, \\quad d > n(1-1/q)
     * @complexity O(1)
     * @constructive false
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER};
        REGISTER_CT("ct_plotkin_bound", "Plotkin界：计算大距离码的码大小上界（d > n(1-1/q)时有效）", inputs, 3,
                    PRESET_TYPE_INTEGER,
                    "|\\mathcal{C}| \\leq \\left\\lfloor \\frac{d}{d - n(1-1/q)} \\right\\rfloor, \\quad d > n(1-1/q)",
                    "O(1)", false, false);
    }

    /* ============================================================
     * 第四部分：编码应用（4个）
     * ============================================================ */

    /**
     * @brief ct_error_correction_capability - 纠错能力
     *
     * 计算线性码的纠错能力 t = floor((d-1)/2)。
     * 纠错能力t表示码可以纠正最多t个符号错误。
     * 检错能力为 d-1（可以检测最多 d-1 个错误）。
     * 同时纠t_e个错和检t_d个错：t_e + t_d <= d - 1，t_e <= t_d。
     *
     * @param d 最小距离（PRESET_TYPE_INTEGER）
     * @return 纠错能力 t = floor((d-1)/2)（PRESET_TYPE_INTEGER）
     * @math t = \\lfloor(d-1)/2\\rfloor, \\quad \\text{检错能力} = d - 1
     * @complexity O(1)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER};
        REGISTER_CT("ct_error_correction_capability", "纠错能力：计算码的纠错能力 t = floor((d-1)/2) 和检错能力 d-1",
                    inputs, 1, PRESET_TYPE_INTEGER, "t = \\lfloor(d-1)/2\\rfloor, \\quad \\text{检错能力} = d - 1",
                    "O(1)", true, false);
    }

    /**
     * @brief ct_code_weight_enumerator - 重量枚举器
     *
     * 计算线性码的重量枚举器（重量分布）。
     * 重量枚举器 W_C(x,y) = Σ_{i=0}^{n} A_i x^{n-i} y^i。
     * 其中 A_i 是重量为 i 的码字数量，A_0 = 1。
     * 重量分布完全决定码在BSC上的错误概率性能。
     *
     * @param G 生成矩阵（PRESET_TYPE_MATRIX）
     * @param q 有限域阶数（PRESET_TYPE_INTEGER）
     * @return 重量分布 [A_0, A_1, ..., A_n]（PRESET_TYPE_LIST）
     * @math W_{\\mathcal{C}}(x, y) = \\sum_{i=0}^{n} A_i x^{n-i} y^i
     * @complexity O(q^k · n)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX, PRESET_TYPE_INTEGER};
        REGISTER_CT("ct_code_weight_enumerator",
                    "重量枚举器：计算码的重量分布 [A_0, A_1, ..., A_n]，A_i为重量i的码字数", inputs, 2,
                    PRESET_TYPE_LIST, "W_{\\mathcal{C}}(x, y) = \\sum_{i=0}^{n} A_i x^{n-i} y^i", "O(q^k \\cdot n)",
                    true, false);
    }

    /**
     * @brief ct_macwilliams_identity - MacWilliams恒等式
     *
     * MacWilliams恒等式：由码C的重量枚举器计算对偶码C^⊥的重量枚举器。
     * W_{C^⊥}(x,y) = (1/|C|) W_C(x+y, x-y)。
     * 这是编码理论中最重要的恒等式之一，建立了码与对偶码之间的联系。
     * 可用于验证重量枚举器的正确性。
     *
     * @param W_C 码C的重量枚举器系数（PRESET_TYPE_LIST）
     * @param n 码长（PRESET_TYPE_INTEGER）
     * @param q 有限域阶数（PRESET_TYPE_INTEGER）
     * @return 对偶码C^⊥的重量枚举器（PRESET_TYPE_LIST）
     * @math W_{C^\\perp}(x,y) = \\frac{1}{|\\mathcal{C}|} W_C(x+y, x-y)
     * @complexity O(n^2)
     * @constructive true
     * @reversible true（对偶的对偶是自身：C^⊥^⊥ = C）
     */
    {
        PresetType inputs[] = {PRESET_TYPE_LIST, PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER};
        REGISTER_CT("ct_macwilliams_identity", "MacWilliams恒等式：由码C的重量枚举器计算对偶码C^⊥的重量枚举器", inputs,
                    3, PRESET_TYPE_LIST, "W_{C^\\perp}(x,y) = \\frac{1}{|\\mathcal{C}|} W_C(x+y, x-y)", "O(n^2)", true,
                    true);
    }

    /**
     * @brief ct_turbo_code - Turbo码
     *
     * 分析Turbo码的迭代译码性能。
     * Turbo码由两个并行级联的卷积码组成，通过交织器连接。
     * 使用BCJR（MAP）算法进行迭代译码，在低SNR下接近Shannon极限。
     * Turbo码的发明引发了编码理论的革命（1993年）。
     * 性能分析包括误码率（BER）曲线和收敛特性。
     *
     * @param constraint_length 约束长度（PRESET_TYPE_INTEGER）
     * @param interleaver_size 交织器大小（PRESET_TYPE_INTEGER）
     * @param iterations 迭代次数（PRESET_TYPE_INTEGER）
     * @return Turbo码性能参数（PRESET_TYPE_TUPLE）
     * @math \\text{BER} \\approx f(\\text{SNR}, L, N, I_{\\text{iter}}), \\quad \\text{接近Shannon极限}
     * @complexity O(N · L · 2^L · I_iter)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER};
        REGISTER_CT("ct_turbo_code", "Turbo码：分析并行级联卷积码的迭代译码性能，接近Shannon极限", inputs, 3,
                    PRESET_TYPE_TUPLE,
                    "\\text{BER} \\approx f(\\text{SNR}, L, N, I_{\\text{iter}}), \\quad \\text{接近Shannon极限}",
                    "O(N \\cdot L \\cdot 2^L \\cdot I_{iter})", true, false);
    }

    /* 返回是否所有预设都注册成功 */
    return success_count == CODING_THEORY_PRESET_COUNT;
}

/**
 * @brief 获取编码理论预设函数块数量
 *
 * @return int 编码理论模块预设函数块总数
 */
int preset_coding_theory_count(void) {
    return CODING_THEORY_PRESET_COUNT;
}

/**
 * @brief 获取编码理论预设函数块名称列表
 *
 * @param out_names 输出名称数组指针（调用者负责释放）
 * @param out_count 输出预设数量
 * @return true 获取成功
 * @return false 获取失败
 */
bool preset_coding_theory_get_names(char ***out_names, int *out_count) {
    if (out_names == NULL || out_count == NULL) {
        return false;
    }

    *out_count = CODING_THEORY_PRESET_COUNT;

    /* 分配名称数组（使用项目统一的内存管理函数） */
    char **names = (char **) lv00_malloc(CODING_THEORY_PRESET_COUNT * sizeof(char *));
    if (names == NULL) {
        return false;
    }

    /* 填充预设名称列表 */
    const char *preset_names[] = {
        /* 线性码 */
        "ct_linear_code_construct", "ct_hamming_code", "ct_parity_check", "ct_syndrome_decode", "ct_code_distance",
        "ct_code_dimension",
        /* 循环码与BCH码 */
        "ct_cyclic_code_construct", "ct_bch_code_construct", "ct_reed_solomon_construct", "ct_convolutional_code",
        /* 码的界与性能 */
        "ct_hamming_bound", "ct_singleton_bound", "ct_gilbert_varshamov_bound", "ct_plotkin_bound",
        /* 编码应用 */
        "ct_error_correction_capability", "ct_code_weight_enumerator", "ct_macwilliams_identity", "ct_turbo_code"};

    for (int i = 0; i < CODING_THEORY_PRESET_COUNT; i++) {
        names[i] = lv00_strdup(preset_names[i]);
        if (names[i] == NULL) {
            /* 分配失败时释放已分配的内存 */
            for (int j = 0; j < i; j++) {
                void *tmp = names[j];
                lv00_free(&tmp);
            }
            lv00_free((void **) &names);
            return false;
        }
    }

    *out_names = names;
    return true;
}

/**
 * @brief 获取编码理论模块类别名称
 *
 * @return 类别名称字符串
 */
const char *preset_coding_theory_category(void) {
    return "编码理论";
}

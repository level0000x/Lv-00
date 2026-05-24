/**
 * @file rational.h
 * @brief 有理数精确类型 —— 基于 GMP mpz_t 的有理数运算
 *
 * @details 提供基于 GMP 多精度整数的有理数类型 Lv00Rational。
 *          所有运算均为精确，不会产生浮点舍入。
 *
 *          设计决策：
 *          - 使用 mpz_t (而非 mpq_t) 直接存储分子/分母，以获得对
 *            分母正常化和溢出检测的完全控制。
 *          - 分母始终 > 0（规范化符号约定）。
 *          - 分子和分母始终互质（gcd = 1），通过 simplify 保证。
 *          - create/destroy 模式匹配 Lv-00 的内存管理惯例。
 *
 *          与项目现有代码的兼容性：
 *          - 可与 Rational* (symbolic_coord.h 中的 mpq_t 型) 互转
 *          - 可与 mpz_poly_t 系数数组（mpz_t*）配合使用
 *          - 零依赖额外的第三方库，仅需 GMP
 *
 * @author Lv-00 Project
 * @version v1.0.0
 * @date 2026-05-24
 */

#ifndef LV00_RATIONAL_H
#define LV00_RATIONAL_H

#include <gmp.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 类型定义
 * ======================================================================== */

/**
 * @brief 精确有理数 —— 分子/分母均为 GMP 多精度整数
 *
 * 不变量:
 *   den > 0  (分母始终为正)
 *   gcd(num, den) == 1  (化简后)
 *   num 和 den 均已通过 mpz_init() 初始化
 */
typedef struct {
    mpz_t num;  /**< 分子（可为负） */
    mpz_t den;  /**< 分母（始终 > 0） */
} Lv00Rational;

/* ========================================================================
 * 生命周期管理
 * ======================================================================== */

/**
 * @brief 创建并初始化一个新的有理数
 *
 * 分配堆内存并初始化 GMP 整数。返回的有理数初始化为 0/1。
 *
 * @return 新分配的有理数，失败返回 NULL
 */
Lv00Rational *lv00_rational_create(void);

/**
 * @brief 从分子分母创建有理数
 *
 * 自动规范化：化简 gcd，确保分母为正。
 *
 * @param num  分子 (mpz_t)
 * @param den  分母 (mpz_t)，不得为 0
 * @return 新分配的有理数，失败或 den==0 返回 NULL
 */
Lv00Rational *lv00_rational_create_from_mpz(const mpz_t num, const mpz_t den);

/**
 * @brief 从 C 整数创建有理数
 *
 * @param num  分子
 * @param den  分母（不得为 0）
 * @return 新分配的有理数，失败或 den==0 返回 NULL
 */
Lv00Rational *lv00_rational_create_from_si(long num, unsigned long den);

/**
 * @brief 从 C int64_t 创建有理数
 *
 * @param num  分子
 * @param den  分母（不得为 0）
 * @return 新分配的有理数，失败或 den==0 返回 NULL
 */
Lv00Rational *lv00_rational_create_from_i64(int64_t num, uint64_t den);

/**
 * @brief 拷贝有理数
 *
 * @param src 源有理数
 * @return 新分配的拷贝，失败返回 NULL
 */
Lv00Rational *lv00_rational_clone(const Lv00Rational *src);

/**
 * @brief 销毁有理数并释放所有资源
 *
 * @param r 有理数指针的指针（函数内设为 NULL）
 */
void lv00_rational_destroy(Lv00Rational **r);

/* ========================================================================
 * 赋值操作
 * ======================================================================== */

/**
 * @brief dst = src（深拷贝，dst 必须已初始化）
 *
 * @param dst 目标有理数
 * @param src 源有理数
 */
void lv00_rational_set(Lv00Rational *dst, const Lv00Rational *src);

/**
 * @brief r = 0/1
 */
void lv00_rational_set_zero(Lv00Rational *r);

/**
 * @brief r = 1/1
 */
void lv00_rational_set_one(Lv00Rational *r);

/**
 * @brief r = num/den（从 mpz_t 赋值，自动规范化）
 *
 * @param r   目标有理数
 * @param num 分子
 * @param den 分母（不得为 0）
 * @return true 成功，false 失败（den 为 0）
 */
bool lv00_rational_set_mpz(Lv00Rational *r, const mpz_t num, const mpz_t den);

/* ========================================================================
 * 规范化
 * ======================================================================== */

/**
 * @brief 化简有理数：分子分母除以 gcd，确保分母为正
 *
 * 即约化操作。对大多数算术运算结果自动调用。
 * 也可以手动调用以保证数据一致性。
 *
 * @param r 有理数
 */
void lv00_rational_simplify(Lv00Rational *r);

/* ========================================================================
 * 算术运算（结果均分配新内存，调用者负责销毁）
 * ======================================================================== */

/**
 * @brief result = a + b（精确加法）
 *
 * @param a 左操作数
 * @param b 右操作数
 * @return 新分配的结果，失败返回 NULL
 */
Lv00Rational *lv00_rational_add(const Lv00Rational *a, const Lv00Rational *b);

/**
 * @brief result = a - b（精确减法）
 */
Lv00Rational *lv00_rational_sub(const Lv00Rational *a, const Lv00Rational *b);

/**
 * @brief result = a * b（精确乘法）
 */
Lv00Rational *lv00_rational_mul(const Lv00Rational *a, const Lv00Rational *b);

/**
 * @brief result = a / b（精确除法）
 *
 * @param a 被除数
 * @param b 除数（不得为 0）
 * @return 新分配的结果，b==0 返回 NULL
 */
Lv00Rational *lv00_rational_div(const Lv00Rational *a, const Lv00Rational *b);

/**
 * @brief 取相反数: result = -a
 */
Lv00Rational *lv00_rational_neg(const Lv00Rational *a);

/**
 * @brief 取倒数: result = 1/a
 *
 * @param a 不得为 0
 * @return 新分配的结果，a==0 返回 NULL
 */
Lv00Rational *lv00_rational_inv(const Lv00Rational *a);

/**
 * @brief 取绝对值: result = |a|
 */
Lv00Rational *lv00_rational_abs(const Lv00Rational *a);

/* ========================================================================
 * 原地算术运算（修改第一个参数）
 * ======================================================================== */

void lv00_rational_add_inplace(Lv00Rational *a, const Lv00Rational *b);
void lv00_rational_sub_inplace(Lv00Rational *a, const Lv00Rational *b);
void lv00_rational_mul_inplace(Lv00Rational *a, const Lv00Rational *b);
bool lv00_rational_div_inplace(Lv00Rational *a, const Lv00Rational *b); /* false if b==0 */
void lv00_rational_neg_inplace(Lv00Rational *a);

/* ========================================================================
 * 比较操作
 * ======================================================================== */

/**
 * @brief 比较 a 和 b
 * @return <0  if a < b, 0 if a == b, >0 if a > b
 */
int lv00_rational_cmp(const Lv00Rational *a, const Lv00Rational *b);

/**
 * @brief 判断 a == b
 */
bool lv00_rational_equal(const Lv00Rational *a, const Lv00Rational *b);

/**
 * @brief 判断 a 是否为 0
 */
bool lv00_rational_is_zero(const Lv00Rational *a);

/**
 * @brief 判断 a 是否为 1
 */
bool lv00_rational_is_one(const Lv00Rational *a);

/**
 * @brief 判断 a 是否为整数（分母为 1）
 */
bool lv00_rational_is_integer(const Lv00Rational *a);

/**
 * @brief 判断符号: <0 负, 0 零, >0 正
 */
int lv00_rational_sgn(const Lv00Rational *a);

/* ========================================================================
 * 与 double 的转换（显式标注精度损失）
 * ======================================================================== */

/**
 * @brief 将有理数转换为 double（损失精度）
 *
 * 此函数直接使用 mpz_get_d 转换分子分母。
 * 在 LV00_STRICT_EXACT_MODE 下会产生警告。
 * 仅供显示/日志使用，不得参与代数计算。
 *
 * @param r         有理数
 * @param out_lossy 输出 double 近似值
 * @param out_loss_bits 输出预估的精度损失比特数（可为 NULL）
 * @return true 成功，false r 为 NULL
 *
 * @note 调用此函数后，必须视为已损失精确性。
 *       proof 路径应使用 lv00_rational_cmp 等精确比较，
 *       而非将结果转为 double 再比较。
 */
bool lv00_rational_to_double(const Lv00Rational *r, double *out_lossy, int *out_loss_bits);

/**
 * @brief 预估 double 近似值的精度损失比特数
 *
 * 将有理数 num/den 与当前 double 表示之间的
 * 有效位差作为近似比特损失返回。
 *
 * @param r 有理数
 * @return 预估值损失比特数的估值（0 表示 double 可精确表示此值），
 *         -1 表示错误
 */
int lv00_rational_estimate_loss(const Lv00Rational *r);

/* ========================================================================
 * 防止分母溢出 —— 乘法运算的安全检查
 * ======================================================================== */

/**
 * @brief 检查两个有理数相乘是否会导致分母异常增长
 *
 * 如果分子或分母的比特数过大（超过阈值），返回 false。
 * 阈值为防止后续 GMP 运算性能急剧下降而设置。
 *
 * @param a 操作数
 * @param b 操作数
 * @param max_bits 最大允许比特数（0 = 使用默认值 2^16）
 * @return true 安全（比特数在阈值以内），false 潜在溢出
 */
bool lv00_rational_mul_is_safe(const Lv00Rational *a, const Lv00Rational *b, uint64_t max_bits);

/**
 * @brief 检查分母的绝对值是否在安全范围内
 *
 * 防止 deormalization 导致的分母溢出。
 * 默认安全上限为 2^32 比特（约 10^10 位十进制数）。
 *
 * @param den 分母 (mpz_t)
 * @return true 安全
 */
bool lv00_rational_den_is_safe(const mpz_t den);

/* ========================================================================
 * 格式化与调试
 * ======================================================================== */

/**
 * @brief 将有理数序列化为字符串 "num/den"
 *
 * 规范化后的分母为 1 时，输出 "num"。
 *
 * @param r 有理数
 * @return 新分配的字符串（调用者负责 gmp_free 或 free），
 *         失败返回 NULL
 */
char *lv00_rational_to_string(const Lv00Rational *r);

/**
 * @brief 从字符串解析有理数
 *
 * 支持格式: "123", "-456", "3/4", "-7/8"
 *
 * @param s 输入字符串
 * @return 新分配的有理数，解析失败返回 NULL
 */
Lv00Rational *lv00_rational_from_string(const char *s);

/* ========================================================================
 * 与现有 Rational* (mpq_t) 类型的互操作
 * ======================================================================== */

/**
 * @brief 从 mpq_t 创建 Lv00Rational
 *
 * 提取 mpq 的分子分母并规范化。
 *
 * @param val GMP 有理数
 * @return 新分配的 Lv00Rational
 */
Lv00Rational *lv00_rational_from_mpq(mpq_srcptr val);

/**
 * @brief 将 Lv00Rational 写入 mpq_t
 *
 * @param r   源有理数
 * @param out 目标 mpq_t（必须已初始化）
 */
void lv00_rational_to_mpq(const Lv00Rational *r, mpq_t out);

#ifdef __cplusplus
}
#endif

#endif /* LV00_RATIONAL_H */

/**
 * @file lv_rational.c
 * @brief 精确有理数运算实现 —— 基于 GMP mpq_t（统一原语）
 *
 * @details lvRational 的有理数运算的完整实现。
 *          所有运算保证精确，无浮点舍入；底层统一使用 GMP mpq_t，
 *          与 symbolic_coord.h 中的 Rational* (mpq_t 型) 同构，可直接互操作。
 *
 *          位宽防护保留：inplace 运算超过 128 比特时转为 double 近似
 *          （防止循环中无界增长）；mul_is_safe / den_is_safe / estimate_loss
 *          提供显式的精度损失检查。
 *
 *          层归属：自 layer4_reasoning/expr 归位至 layer3_geometry/symbolics
 *          （与 rational.c 薄转发层同域）。仅依赖 GMP + lv_utils（L2 级），
 *          消解 L3→L4 反向环；消费方 L4（expr_canon.c）依赖 L3 属合法方向。
 *
 * @author Lv-00 Project
 * @version v1.0.0
 * @date 2026-05-24
 */

#include "lv/rational.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_utils.h"

/* 用于 lv_rational_den_is_safe 的安全比特阈值 */
#define RATIONAL_SAFE_BITS_DEFAULT 65536 /* 2^16 比特 */

/* inplace 运算的位宽熔断阈值（分子/分母任一超限即转 double 近似） */
#define RATIONAL_INPLACE_BIT_LIMIT 128

/* 内部辅助: 计算 mpz 的近似比特数 */
static uint64_t mpz_bit_size(const mpz_t x) {
    if (mpz_sgn(x) == 0)
        return 0;
    return (uint64_t) mpz_sizeinbase(x, 2);
}

/* ========================================================================
 * 生命周期管理
 * ======================================================================== */

/**
 * @brief 创建有理数（初始值为 0/1）
 * @return 新分配的有理数，失败返回 NULL
 */
lvRational *lv_rational_create(void) {
    lvRational *r = (lvRational *) lv_calloc(1, sizeof(lvRational));
    if (!r)
        return NULL;
    mpq_init(r->value);
    mpq_set_ui(r->value, 0, 1);
    return r;
}

/**
 * @brief 从 GMP 整数创建有理数
 * @param num 分子（GMP mpz_t）
 * @param den 分母（GMP mpz_t，不得为 0）
 * @return 新分配的有理数，分母为 0 或内存不足时返回 NULL
 */
lvRational *lv_rational_create_from_mpz(const mpz_t num, const mpz_t den) {
    if (mpz_sgn(den) == 0)
        return NULL; /* 分母不得为 0 */

    lvRational *r = (lvRational *) lv_calloc(1, sizeof(lvRational));
    if (!r)
        return NULL;

    mpq_init(r->value);
    mpz_set(mpq_numref(r->value), num);
    mpz_set(mpq_denref(r->value), den);

    /* 规范化: 化简 + 确保分母为正 */
    mpq_canonicalize(r->value);

    return r;
}

/**
 * @brief 从 C 长整数创建有理数
 * @param num 分子
 * @param den 分母（不得为 0）
 * @return 新分配的有理数，失败返回 NULL
 */
lvRational *lv_rational_create_from_si(long num, unsigned long den) {
    if (den == 0)
        return NULL;

    lvRational *r = (lvRational *) lv_calloc(1, sizeof(lvRational));
    if (!r)
        return NULL;

    mpq_init(r->value);
    mpq_set_si(r->value, num, (unsigned long) den);
    mpq_canonicalize(r->value);
    return r;
}

/**
 * @brief 从 64 位整数创建有理数
 * @param num 分子（有符号）
 * @param den 分母（无符号，不得为 0）
 * @return 新分配的有理数，失败返回 NULL
 */
lvRational *lv_rational_create_from_i64(int64_t num, uint64_t den) {
    if (den == 0)
        return NULL;

    lvRational *r = (lvRational *) lv_calloc(1, sizeof(lvRational));
    if (!r)
        return NULL;

    mpq_init(r->value);

    /* 通过字符串中转，避免 Windows LLP64 下 unsigned long 32 位截断 */
    char num_str[32];
    lv_snprintf(num_str, sizeof(num_str), "%lld", (long long) num);
    mpz_set_str(mpq_numref(r->value), num_str, 10);

    char den_str[32];
    /* den 为 uint64_t，使用 %llu 避免大值输出为负数的符号错误 */
    lv_snprintf(den_str, sizeof(den_str), "%llu", (unsigned long long) den);
    mpz_set_str(mpq_denref(r->value), den_str, 10);

    mpq_canonicalize(r->value);
    return r;
}

/**
 * @brief 深拷贝有理数
 * @param src 源有理数
 * @return 新分配的副本，失败返回 NULL
 */
lvRational *lv_rational_clone(const lvRational *src) {
    if (!src)
        return NULL;

    lvRational *r = (lvRational *) lv_calloc(1, sizeof(lvRational));
    if (!r)
        return NULL;

    mpq_init(r->value);
    mpq_set(r->value, src->value);

    return r;
}

/**
 * @brief 销毁有理数并释放 GMP 资源
 * @param r 指向有理数指针的指针（释放后置 NULL）
 */
void lv_rational_destroy(lvRational **r) {
    if (r && *r) {
        mpq_clear((*r)->value);
        lv_free((void **) r);
    }
}

/* ========================================================================
 * 赋值操作
 * ======================================================================== */

/**
 * @brief 将有理数赋值为另一个有理数的值
 */
void lv_rational_set(lvRational *dst, const lvRational *src) {
    if (!dst || !src)
        return;
    mpq_set(dst->value, src->value);
}

/** @brief 将有理数置零 */
void lv_rational_set_zero(lvRational *r) {
    if (!r)
        return;
    mpq_set_ui(r->value, 0, 1);
}

/** @brief 将有理数置一 */
void lv_rational_set_one(lvRational *r) {
    if (!r)
        return;
    mpq_set_ui(r->value, 1, 1);
}

/**
 * @brief 用 GMP 整数设置有理数的分子和分母
 * @return true 成功，false 分母为 0
 */
bool lv_rational_set_mpz(lvRational *r, const mpz_t num, const mpz_t den) {
    if (!r)
        return false;
    if (mpz_sgn(den) == 0)
        return false;

    mpz_set(mpq_numref(r->value), num);
    mpz_set(mpq_denref(r->value), den);
    mpq_canonicalize(r->value);
    return true;
}

/* ========================================================================
 * 规范化
 * ======================================================================== */

/**
 * @brief 约分有理数（化简为最简分数，确保分母为正）
 */
void lv_rational_simplify(lvRational *r) {
    if (!r)
        return;
    mpq_canonicalize(r->value);
}

/* ========================================================================
 * 算术运算（返回新分配的有理数；mpq 运算后统一规范化）
 * ======================================================================== */

/**
 * @brief 有理数加法：返回 a + b 的新有理数
 */
lvRational *lv_rational_add(const lvRational *a, const lvRational *b) {
    if (!a || !b)
        return NULL;

    lvRational *r = lv_rational_create();
    if (!r)
        return NULL;

    mpq_add(r->value, a->value, b->value);
    mpq_canonicalize(r->value);
    return r;
}

/**
 * @brief 有理数减法：返回 a - b 的新有理数
 */
lvRational *lv_rational_sub(const lvRational *a, const lvRational *b) {
    if (!a || !b)
        return NULL;

    lvRational *r = lv_rational_create();
    if (!r)
        return NULL;

    mpq_sub(r->value, a->value, b->value);
    mpq_canonicalize(r->value);
    return r;
}

/**
 * @brief 有理数乘法：返回 a * b 的新有理数
 */
lvRational *lv_rational_mul(const lvRational *a, const lvRational *b) {
    if (!a || !b)
        return NULL;

    lvRational *r = lv_rational_create();
    if (!r)
        return NULL;

    mpq_mul(r->value, a->value, b->value);
    mpq_canonicalize(r->value);
    return r;
}

/**
 * @brief 有理数除法：返回 a / b 的新有理数（b 不得为 0）
 */
lvRational *lv_rational_div(const lvRational *a, const lvRational *b) {
    if (!a || !b)
        return NULL;
    if (lv_rational_is_zero(b))
        return NULL;

    lvRational *r = lv_rational_create();
    if (!r)
        return NULL;

    mpq_div(r->value, a->value, b->value);
    mpq_canonicalize(r->value);
    return r;
}

/**
 * @brief 有理数取反：返回 -a 的新有理数
 */
lvRational *lv_rational_neg(const lvRational *a) {
    if (!a)
        return NULL;

    lvRational *r = lv_rational_clone(a);
    if (r) {
        mpq_neg(r->value, r->value);
    }
    return r;
}

/**
 * @brief 有理数取倒数：返回 1/a 的新有理数（a 不得为 0）
 */
lvRational *lv_rational_inv(const lvRational *a) {
    if (!a)
        return NULL;
    if (lv_rational_is_zero(a))
        return NULL;

    lvRational *r = lv_rational_create();
    if (!r)
        return NULL;

    mpq_inv(r->value, a->value);
    return r;
}

/**
 * @brief 有理数绝对值：返回 |a| 的新有理数
 */
lvRational *lv_rational_abs(const lvRational *a) {
    if (!a)
        return NULL;

    lvRational *r = lv_rational_clone(a);
    if (r) {
        if (mpq_sgn(r->value) < 0) {
            mpq_neg(r->value, r->value);
        }
    }
    return r;
}

/* ========================================================================
 * 原地算术运算
 * ======================================================================== */

/**
 * @brief 位宽熔断保护：分子或分母比特位超出阈值时转换为 double 近似
 *
 * 防止循环中无界增长。与公共 bit_burning 电路熔断互补。
 */
static void lv_rational_bit_burn(lvRational *r) {
    if (!r)
        return;
    if (mpz_sizeinbase(mpq_numref(r->value), 2) > RATIONAL_INPLACE_BIT_LIMIT ||
        mpz_sizeinbase(mpq_denref(r->value), 2) > RATIONAL_INPLACE_BIT_LIMIT) {
        double d = mpq_get_d(r->value);
        mpq_set_d(r->value, d);
        mpq_canonicalize(r->value);
    }
}

/**
 * @brief 有理数原地加法：a += b
 */
void lv_rational_add_inplace(lvRational *a, const lvRational *b) {
    if (!a || !b)
        return;

    mpq_add(a->value, a->value, b->value);
    mpq_canonicalize(a->value);
    lv_rational_bit_burn(a);
}

/**
 * @brief 有理数原地减法：a -= b
 */
void lv_rational_sub_inplace(lvRational *a, const lvRational *b) {
    if (!a || !b)
        return;

    mpq_sub(a->value, a->value, b->value);
    mpq_canonicalize(a->value);
    lv_rational_bit_burn(a);
}

/**
 * @brief 有理数原地乘法：a *= b
 */
void lv_rational_mul_inplace(lvRational *a, const lvRational *b) {
    if (!a || !b)
        return;

    mpq_mul(a->value, a->value, b->value);
    mpq_canonicalize(a->value);
    lv_rational_bit_burn(a);
}

/**
 * @brief 有理数原地除法：a /= b（b 不得为 0）
 * @return true 成功，false b 为 0
 */
bool lv_rational_div_inplace(lvRational *a, const lvRational *b) {
    if (!a || !b)
        return false;
    if (lv_rational_is_zero(b))
        return false;

    mpq_div(a->value, a->value, b->value);
    mpq_canonicalize(a->value);
    lv_rational_bit_burn(a);
    return true;
}

/**
 * @brief 有理数原地取反：a = -a
 */
void lv_rational_neg_inplace(lvRational *a) {
    if (!a)
        return;
    mpq_neg(a->value, a->value);
}

/* ========================================================================
 * 比较操作
 * ======================================================================== */

/**
 * @brief 比较两个有理数
 * @return -1 (a < b), 0 (a == b), 1 (a > b)
 */
int lv_rational_cmp(const lvRational *a, const lvRational *b) {
    if (!a || !b)
        return 0;
    return mpq_cmp(a->value, b->value);
}

/** @brief 判断两个有理数是否相等 */
bool lv_rational_equal(const lvRational *a, const lvRational *b) {
    return lv_rational_cmp(a, b) == 0;
}

/** @brief 判断有理数是否为零 */
bool lv_rational_is_zero(const lvRational *a) {
    if (!a)
        return true;
    return mpq_sgn(a->value) == 0;
}

/** @brief 判断有理数是否为一 */
bool lv_rational_is_one(const lvRational *a) {
    if (!a)
        return false;
    return mpq_cmp_ui(a->value, 1, 1) == 0;
}

/** @brief 判断有理数是否为整数（分母为 1） */
bool lv_rational_is_integer(const lvRational *a) {
    if (!a)
        return false;
    return mpz_cmp_ui(mpq_denref(a->value), 1) == 0;
}

/** @brief 获取有理数的符号：-1（负）、0（零）、1（正） */
int lv_rational_sgn(const lvRational *a) {
    if (!a)
        return 0;
    return mpq_sgn(a->value);
}

/* ========================================================================
 * 与 double 的转换（显式标注精度损失）
 * ======================================================================== */

/**
 * @brief 将有理数转换为双精度浮点数
 * @param out_lossy [输出] 近似浮点值
 * @param out_loss_bits [输出] 精度损失位数（NULL 可忽略）
 * @return true 转换成功，false 参数无效
 */
bool lv_rational_to_double(const lvRational *r, double *out_lossy, int *out_loss_bits) {
    if (!r || !out_lossy)
        return false;

    /* 使用 GMP 的 mpq_get_d 获取最佳近似 */
    double d = mpq_get_d(r->value);

    /* 检查 double 转换是否产生 Inf/NaN */
    if (!isfinite(d)) {
        return false;
    }

    *out_lossy = d;

    if (out_loss_bits) {
        *out_loss_bits = lv_rational_estimate_loss(r);
    }

    return true;
}

/**
 * @brief 估算有理数转浮点数时的精度损失位数
 * @return 损失位数（0 表示无损失）
 */
int lv_rational_estimate_loss(const lvRational *r) {
    if (!r || lv_rational_is_zero(r))
        return 0;

    /* 预估精度损失：有效位的比特数是否超出 double 的 53 位尾数 */
    uint64_t num_bits = mpz_bit_size(mpq_numref(r->value));
    uint64_t den_bits = mpz_bit_size(mpq_denref(r->value));
    uint64_t total_bits = (num_bits > den_bits) ? num_bits : den_bits;

    /* double 的尾数为 53 位（含隐藏位） */
    if (total_bits <= 53) {
        return 0; /* double 可精确表示 */
    }

    /* 否则估算损失 */
    int loss = (int) (total_bits - 53);
    return loss > 0 ? loss : 0;
}

/* ========================================================================
 * 防止分母溢出
 * ======================================================================== */

/**
 * @brief 判断两个有理数相乘是否安全（不会超出比特数限制）
 * @param max_bits 允许的最大比特数
 * @return true 安全，false 可能溢出
 */
bool lv_rational_mul_is_safe(const lvRational *a, const lvRational *b, uint64_t max_bits) {
    if (!a || !b)
        return false;

    if (max_bits == 0) {
        max_bits = RATIONAL_SAFE_BITS_DEFAULT;
    }

    /* 乘法后分子/分母的比特数约为各操作数比特数之和 */
    uint64_t num_bits_a = mpz_bit_size(mpq_numref(a->value));
    uint64_t den_bits_a = mpz_bit_size(mpq_denref(a->value));
    uint64_t num_bits_b = mpz_bit_size(mpq_numref(b->value));
    uint64_t den_bits_b = mpz_bit_size(mpq_denref(b->value));

    /* 检查分子和分母各自是否会溢出阈值 */
    if (num_bits_a + num_bits_b > max_bits)
        return false;
    if (den_bits_a + den_bits_b > max_bits)
        return false;

    return true;
}

/**
 * @brief 判断分母的比特数是否在安全范围内
 * @return true 安全，false 比特数过大
 */
bool lv_rational_den_is_safe(const mpz_t den) {
    if (!den)
        return false;
    uint64_t bits = mpz_bit_size(den);
    return bits <= RATIONAL_SAFE_BITS_DEFAULT;
}

/* ========================================================================
 * 格式化与调试
 * ======================================================================== */

/**
 * @brief 将有理数转换为字符串
 *
 * 分母为 1 时输出整数形式（如 "3"），否则输出 "num/den"（如 "-5/4"），
 * 与原实现的"整数无 /1"行为一致。
 *
 * @return 新分配的字符串，失败返回 NULL
 */
char *lv_rational_to_string(const lvRational *r) {
    if (!r)
        return NULL;
    return lv_mpq_to_string(r->value, true);
}

/**
 * @brief 从字符串解析有理数（支持 "a/b"、"整数" 格式；mpq_set_str 统一处理）
 * @return 新分配的有理数，失败返回 NULL
 */
lvRational *lv_rational_from_string(const char *s) {
    if (!s || !*s)
        return NULL;

    lvRational *r = lv_rational_create();
    if (!r)
        return NULL;

    if (mpq_set_str(r->value, s, 10) != 0) {
        lv_rational_destroy(&r);
        return NULL;
    }
    mpq_canonicalize(r->value);
    return r;
}

/* ========================================================================
 * 与 GMP mpq_t 的互操作（统一 mpq_t 原语后即为直接复制）
 * ======================================================================== */

/**
 * @brief 从 GMP mpq_t 创建有理数
 * @return 新分配的有理数，失败返回 NULL
 */
lvRational *lv_rational_from_mpq(mpq_srcptr val) {
    if (!val)
        return NULL;

    lvRational *r = lv_rational_create();
    if (!r)
        return NULL;

    mpq_set(r->value, val);
    return r;
}

/**
 * @brief 将有理数导出到 GMP mpq_t
 * @param r 源有理数
 * @param out [输出] GMP 有理数（调用方需已初始化）
 */
void lv_rational_to_mpq(const lvRational *r, mpq_t out) {
    if (!r || !out)
        return;

    mpq_set(out, r->value);
    mpq_canonicalize(out);
}

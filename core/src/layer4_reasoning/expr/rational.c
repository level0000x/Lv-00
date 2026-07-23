/**
 * @file rational.c
 * @brief 精确有理数运算实现 —— 基于 GMP mpz_t
 *
 * @details lvRational 的有理数运算的完整实现。
 *          所有运算保证精确，无浮点舍入。与 symbolic_coord.h 中的
 *          Rational* (mpq_t 型) 可互操作。
 *
 * @author Lv-00 Project
 * @version v1.0.0
 * @date 2026-05-24
 */

#include "rational.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv_utils.h"

/* 用于 lv_rational_den_is_safe 的安全比特阈值 */
#define RATIONAL_SAFE_BITS_DEFAULT 65536 /* 2^16 比特 */

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
    mpz_init_set_si(r->num, 0);
    mpz_init_set_si(r->den, 1);
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

    mpz_init(r->num);
    mpz_init(r->den);

    mpz_set(r->num, num);
    mpz_set(r->den, den);

    /* 规范化: 化简 + 确保分母为正 */
    lv_rational_simplify(r);

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

    mpz_init(r->num);
    mpz_init(r->den);
    mpz_set_si(r->num, num);
    mpz_set_ui(r->den, den);

    lv_rational_simplify(r);
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

    mpz_init(r->num);
    mpz_init(r->den);

    /* 将 int64_t 转为 mpz_t（通过字符串中转避免 Windows LLP64 下 unsigned long 32 位截断） */
    char num_str[32];
    snprintf(num_str, sizeof(num_str), "%lld", (long long)num);
    mpz_set_str(r->num, num_str, 10);

    char den_str[32];
    snprintf(den_str, sizeof(den_str), "%lld", (long long)den);
    mpz_set_str(r->den, den_str, 10);

    lv_rational_simplify(r);
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

    mpz_init_set(r->num, src->num);
    mpz_init_set(r->den, src->den);

    return r;
}

/**
 * @brief 销毁有理数并释放 GMP 资源
 * @param r 指向有理数指针的指针（释放后置 NULL）
 */
void lv_rational_destroy(lvRational **r) {
    if (r && *r) {
        mpz_clear((*r)->num);
        mpz_clear((*r)->den);
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
    mpz_set(dst->num, src->num);
    mpz_set(dst->den, src->den);
}

/** @brief 将有理数置零 */
void lv_rational_set_zero(lvRational *r) {
    if (!r)
        return;
    mpz_set_si(r->num, 0);
    mpz_set_si(r->den, 1);
}

/** @brief 将有理数置一 */
void lv_rational_set_one(lvRational *r) {
    if (!r)
        return;
    mpz_set_si(r->num, 1);
    mpz_set_si(r->den, 1);
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

    mpz_set(r->num, num);
    mpz_set(r->den, den);
    lv_rational_simplify(r);
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

    /* 处理分子为 0 的情况: 立即设为 0/1 */
    if (mpz_sgn(r->num) == 0) {
        mpz_set_si(r->den, 1);
        return;
    }

    /* 确保分母为正 */
    if (mpz_sgn(r->den) < 0) {
        mpz_neg(r->num, r->num);
        mpz_neg(r->den, r->den);
    }

    /* 计算 gcd 并化简 */
    mpz_t g;
    mpz_init(g);
    mpz_gcd(g, r->num, r->den);

    /* 仅当 gcd > 1 时才执行化简 */
    if (mpz_cmp_si(g, 1) > 0) {
        mpz_divexact(r->num, r->num, g);
        mpz_divexact(r->den, r->den, g);
    }

    mpz_clear(g);
}

/* ========================================================================
 * 算术运算
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

    /* result = (a.num*b.den + b.num*a.den) / (a.den * b.den) */
    mpz_t t1, t2;
    mpz_init(t1);
    mpz_init(t2);

    mpz_mul(t1, a->num, b->den); /* a.num * b.den */
    mpz_mul(t2, b->num, a->den); /* b.num * a.den */
    mpz_add(r->num, t1, t2);

    mpz_mul(r->den, a->den, b->den);

    mpz_clear(t1);
    mpz_clear(t2);

    lv_rational_simplify(r);
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

    /* result = (a.num*b.den - b.num*a.den) / (a.den * b.den) */
    mpz_t t1, t2;
    mpz_init(t1);
    mpz_init(t2);

    mpz_mul(t1, a->num, b->den);
    mpz_mul(t2, b->num, a->den);
    mpz_sub(r->num, t1, t2);

    mpz_mul(r->den, a->den, b->den);

    mpz_clear(t1);
    mpz_clear(t2);

    lv_rational_simplify(r);
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

    mpz_mul(r->num, a->num, b->num);
    mpz_mul(r->den, a->den, b->den);

    lv_rational_simplify(r);
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

    mpz_mul(r->num, a->num, b->den);
    mpz_mul(r->den, a->den, b->num);

    lv_rational_simplify(r);
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
        mpz_neg(r->num, r->num);
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

    mpz_set(r->num, a->den);
    mpz_set(r->den, a->num);

    /* 确保分母为正 */
    if (mpz_sgn(r->den) < 0) {
        mpz_neg(r->num, r->num);
        mpz_neg(r->den, r->den);
    }

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
        if (mpz_sgn(r->num) < 0) {
            mpz_neg(r->num, r->num);
        }
    }
    return r;
}

/* ========================================================================
 * 原地算术运算
 * ======================================================================== */

/**
 * @brief 有理数原地加法：a += b
 */
void lv_rational_add_inplace(lvRational *a, const lvRational *b) {
    if (!a || !b)
        return;

    /* a = a + b:
     * a.num = a.num*b.den + b.num*a.den
     * a.den = a.den * b.den
     */
    mpz_t t1, t2;
    mpz_init(t1);
    mpz_init(t2);

    mpz_mul(t1, a->num, b->den);
    mpz_mul(t2, b->num, a->den);
    mpz_add(a->num, t1, t2);

    mpz_mul(a->den, a->den, b->den);

    mpz_clear(t1);
    mpz_clear(t2);

    lv_rational_simplify(a);
}

/**
 * @brief 有理数原地减法：a -= b
 */
void lv_rational_sub_inplace(lvRational *a, const lvRational *b) {
    if (!a || !b)
        return;

    mpz_t t1, t2;
    mpz_init(t1);
    mpz_init(t2);

    mpz_mul(t1, a->num, b->den);
    mpz_mul(t2, b->num, a->den);
    mpz_sub(a->num, t1, t2);

    mpz_mul(a->den, a->den, b->den);

    mpz_clear(t1);
    mpz_clear(t2);

    lv_rational_simplify(a);
}

/**
 * @brief 有理数原地乘法：a *= b
 */
void lv_rational_mul_inplace(lvRational *a, const lvRational *b) {
    if (!a || !b)
        return;

    mpz_mul(a->num, a->num, b->num);
    mpz_mul(a->den, a->den, b->den);

    lv_rational_simplify(a);
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

    mpz_mul(a->num, a->num, b->den);
    mpz_mul(a->den, a->den, b->num);

    lv_rational_simplify(a);
    return true;
}

/**
 * @brief 有理数原地取反：a = -a
 */
void lv_rational_neg_inplace(lvRational *a) {
    if (!a)
        return;
    mpz_neg(a->num, a->num);
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

    /* 交叉乘法比较: a.num/a.den ? b.num/b.den <=> a.num*b.den ? b.num*a.den */
    mpz_t left, right;
    mpz_init(left);
    mpz_init(right);

    mpz_mul(left, a->num, b->den);
    mpz_mul(right, b->num, a->den);

    int result = mpz_cmp(left, right);

    mpz_clear(left);
    mpz_clear(right);

    return result;
}

/** @brief 判断两个有理数是否相等 */
bool lv_rational_equal(const lvRational *a, const lvRational *b) {
    return lv_rational_cmp(a, b) == 0;
}

/** @brief 判断有理数是否为零 */
bool lv_rational_is_zero(const lvRational *a) {
    if (!a)
        return true;
    return mpz_sgn(a->num) == 0;
}

/** @brief 判断有理数是否为一 */
bool lv_rational_is_one(const lvRational *a) {
    if (!a)
        return false;
    return mpz_cmp_si(a->num, 1) == 0 && mpz_cmp_si(a->den, 1) == 0;
}

/** @brief 判断有理数是否为整数（分母为 1） */
bool lv_rational_is_integer(const lvRational *a) {
    if (!a)
        return false;
    return mpz_cmp_si(a->den, 1) == 0;
}

/** @brief 获取有理数的符号：-1（负）、0（零）、1（正） */
int lv_rational_sgn(const lvRational *a) {
    if (!a)
        return 0;
    return mpz_sgn(a->num);
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

    /* 使用 GMP 的 mpz_get_d 获取最佳近似 */
    double num_d = mpz_get_d(r->num);
    double den_d = mpz_get_d(r->den);

    if (den_d == 0.0) {
        /* 分母为 0（不应发生，但此处防御） */
        return false;
    }

    *out_lossy = num_d / den_d;

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
    uint64_t num_bits = mpz_bit_size(r->num);
    uint64_t den_bits = mpz_bit_size(r->den);
    uint64_t total_bits = (num_bits > den_bits) ? num_bits : den_bits;

    /* double 的尾数为 53 位（含隐藏位） */
    if (total_bits <= 53) {
        return 0; /* double 可精确表示 */
    }

    /* 否则估算损失 */
    int loss = (int)(total_bits - 53);
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
    uint64_t num_bits_a = mpz_bit_size(a->num);
    uint64_t den_bits_a = mpz_bit_size(a->den);
    uint64_t num_bits_b = mpz_bit_size(b->num);
    uint64_t den_bits_b = mpz_bit_size(b->den);

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
 * @brief 将有理数转换为字符串（如 "3/4"、"-5/1"）
 * @return 新分配的字符串，失败返回 NULL
 */
char *lv_rational_to_string(const lvRational *r) {
    if (!r)
        return NULL;

    /* 检查分母是否为 1 —— 使用标准 malloc 分配（调用者用 free 释放） */
    if (mpz_cmp_si(r->den, 1) == 0) {
        char *gmp_str = mpz_get_str(NULL, 10, r->num);
        if (!gmp_str) return NULL;
        size_t slen = strlen(gmp_str);
        char *result = (char *) malloc(slen + 1);
        if (!result) { free(gmp_str); return NULL; }
        memcpy(result, gmp_str, slen + 1);
        free(gmp_str);  /* GMP分配，用标准 free 释放 */
        return result;
    }

    /* 输出 "num/den" 格式 */
    char *num_str = mpz_get_str(NULL, 10, r->num);
    char *den_str = mpz_get_str(NULL, 10, r->den);

    if (!num_str || !den_str) {
        free(num_str);  /* GMP分配，用标准 free 释放 */
        free(den_str);  /* GMP分配，用标准 free 释放 */
        return NULL;
    }

    size_t len = strlen(num_str) + strlen(den_str) + 2; /* num + '/' + den + '\0' */
    char *result = (char *) malloc(len);
    if (!result) {
        free(num_str);  /* GMP分配，用标准 free 释放 */
        free(den_str);  /* GMP分配，用标准 free 释放 */
        return NULL;
    }

    snprintf(result, len, "%s/%s", num_str, den_str);

    free(num_str);   /* GMP分配，用标准 free 释放 */
    free(den_str);   /* GMP分配，用标准 free 释放 */

    return result;
}

/**
 * @brief 从字符串解析有理数（支持 "a/b"、"整数"、"小数" 格式）
 * @return 新分配的有理数，失败返回 NULL
 */
lvRational *lv_rational_from_string(const char *s) {
    if (!s || !*s)
        return NULL;

    const char *slash = strchr(s, '/');

    if (slash) {
        /* 格式: "num/den" */
        size_t num_len = (size_t)(slash - s);
        char *num_str = (char *) lv_malloc(num_len + 1);
        if (!num_str)
            return NULL;
        memcpy(num_str, s, num_len);
        num_str[num_len] = '\0';

        mpz_t num, den;
        mpz_init(num);
        mpz_init(den);

        if (mpz_set_str(num, num_str, 10) != 0) {
            lv_free((void **) &num_str);
            mpz_clear(num);
            mpz_clear(den);
            return NULL;
        }

        if (mpz_set_str(den, slash + 1, 10) != 0) {
            lv_free((void **) &num_str);
            mpz_clear(num);
            mpz_clear(den);
            return NULL;
        }

        lv_free((void **) &num_str);

        if (mpz_sgn(den) == 0) {
            mpz_clear(num);
            mpz_clear(den);
            return NULL;
        }

        lvRational *r = lv_rational_create_from_mpz(num, den);
        mpz_clear(num);
        mpz_clear(den);
        return r;
    } else {
        /* 格式: "integer" */
        mpz_t num, den;
        mpz_init(num);
        mpz_init(den);

        if (mpz_set_str(num, s, 10) != 0) {
            mpz_clear(num);
            mpz_clear(den);
            return NULL;
        }

        mpz_set_si(den, 1);

        lvRational *r = lv_rational_create_from_mpz(num, den);
        mpz_clear(num);
        mpz_clear(den);
        return r;
    }
}

/* ========================================================================
 * 与现有 Rational* (mpq_t) 类型的互操作
 * ======================================================================== */

/**
 * @brief 从 GMP mpq_t 创建有理数
 * @return 新分配的有理数，失败返回 NULL
 */
lvRational *lv_rational_from_mpq(mpq_srcptr val) {
    if (!val)
        return NULL;

    mpz_t num, den;
    mpz_init(num);
    mpz_init(den);

    mpz_set(num, mpq_numref(val));
    mpz_set(den, mpq_denref(val));

    lvRational *r = lv_rational_create_from_mpz(num, den);

    mpz_clear(num);
    mpz_clear(den);

    return r;
}

/**
 * @brief 将有理数导出到 GMP mpq_t
 * @param r 源有理数
 * @param out [输出] GMP 有理数
 */
void lv_rational_to_mpq(const lvRational *r, mpq_t out) {
    if (!r || !out)
        return;

    mpq_set_num(out, r->num);
    mpq_set_den(out, r->den);
    mpq_canonicalize(out);
}

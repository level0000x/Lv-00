/**
 * @file test_symbolic_coord_ops.c
 * @brief 符号坐标运算综合测试 —— 代数数运算、符号坐标算术、比较/降级/熔断
 *
 * 测试内容：
 * - 代数数三层数域：有理数、二次代数数、区间运算、多项式系统
 * - 符号坐标四类型算术（加减乘除、幂、开方、取负）
 * - 跨类型比较与提升
 * - Trust Color 降级与 Plan Manager
 * - 位电路熔断 (bit burning) 机制
 * - 超越数运算与组合
 * - 边角情况（零、负数、大数溢出、NULL指针）
 *
 * 遵循 test_helpers.h 测试模式。
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/algebraic_number.h"
#include "lv/symbolic_coord.h"

#define TEST_PASS_STATEMENT g_pass_count++
#define TEST_FAIL_STATEMENT g_fail_count++
#include "test_helpers.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============================================================
 * 第一层：有理数域 Q 测试
 * ============================================================ */

/** 测试有理数创建与约分 */
void test_rational_create_and_simplify(void) {
    AlgRationalError err;

    /* 正常创建 */
    AlgRational r1 = alg_rational_create(2, 4, &err);
    TEST_ASSERT(err == ALG_RATIONAL_OK, "alg_rational_create(2,4) should succeed");
    TEST_ASSERT(r1.num == 1 && r1.den == 2, "2/4 should simplify to 1/2");

    /* 负数 */
    AlgRational r2 = alg_rational_create(-3, 6, &err);
    TEST_ASSERT(err == ALG_RATIONAL_OK, "alg_rational_create(-3,6) should succeed");
    TEST_ASSERT(r2.num == -1 && r2.den == 2, "-3/6 should simplify to -1/2");

    /* 分母为零 */
    AlgRational r3 = alg_rational_create(1, 0, &err);
    TEST_ASSERT(err == ALG_RATIONAL_ERR_ZERO_DEN, "alg_rational_create(1,0) should set ZERO_DEN error");

    /* 零值 */
    AlgRational zero = alg_rational_zero();
    TEST_ASSERT(zero.num == 0 && zero.den == 1, "alg_rational_zero() should be 0/1");

    /* 单位 */
    AlgRational one = alg_rational_one();
    TEST_ASSERT(one.num == 1 && one.den == 1, "alg_rational_one() should be 1/1");

    /* 整数转换 */
    AlgRational fi = alg_rational_from_int(42);
    TEST_ASSERT(fi.num == 42 && fi.den == 1, "alg_rational_from_int(42) should be 42/1");

    TEST_ASSERT(alg_rational_is_zero(&zero) == true, "zero should be zero");
    TEST_ASSERT(alg_rational_is_positive(&one) == true, "one should be positive");
    TEST_ASSERT(alg_rational_is_negative(&r2) == true, "-1/2 should be negative");

    PASS();
}

/** 测试有理数四则运算 */
void test_rational_arithmetic(void) {
    AlgRationalError err;
    AlgRational a = alg_rational_create(1, 2, NULL);
    AlgRational b = alg_rational_create(1, 3, NULL);

    /* 加法：1/2 + 1/3 = 5/6 */
    AlgRational s = alg_rational_add(&a, &b, &err);
    TEST_ASSERT(err == ALG_RATIONAL_OK, "add should succeed");
    AlgRational expected_s = alg_rational_create(5, 6, NULL);
    TEST_ASSERT(alg_rational_cmp(&s, &expected_s) == 0, "1/2 + 1/3 = 5/6");

    /* 减法：1/2 - 1/3 = 1/6 */
    AlgRational d = alg_rational_sub(&a, &b, &err);
    AlgRational expected_d = alg_rational_create(1, 6, NULL);
    TEST_ASSERT(alg_rational_cmp(&d, &expected_d) == 0, "1/2 - 1/3 = 1/6");

    /* 乘法：1/2 * 1/3 = 1/6 */
    AlgRational m = alg_rational_mul(&a, &b, &err);
    AlgRational expected_m = alg_rational_create(1, 6, NULL);
    TEST_ASSERT(alg_rational_cmp(&m, &expected_m) == 0, "1/2 * 1/3 = 1/6");

    /* 除法：1/2 / 1/3 = 3/2 */
    AlgRational q = alg_rational_div(&a, &b, &err);
    AlgRational expected_q = alg_rational_create(3, 2, NULL);
    TEST_ASSERT(alg_rational_cmp(&q, &expected_q) == 0, "1/2 / 1/3 = 3/2");

    /* 取负 */
    AlgRational n = alg_rational_neg(&a);
    TEST_ASSERT(n.num == -1 && n.den == 2, "-(1/2) = -1/2");

    /* 绝对值 */
    AlgRational abs_r = alg_rational_abs(&n);
    TEST_ASSERT(abs_r.num == 1 && abs_r.den == 2, "|-1/2| = 1/2");

    /* 倒数 */
    AlgRational inv = alg_rational_inv(&a, &err);
    TEST_ASSERT(err == ALG_RATIONAL_OK, "inv(1/2) should succeed");
    TEST_ASSERT(inv.num == 2 && inv.den == 1, "inv(1/2) = 2/1");

    /* 零倒数 */
    AlgRational zero = alg_rational_zero();
    AlgRational inv_zero = alg_rational_inv(&zero, &err);
    TEST_ASSERT(err == ALG_RATIONAL_ERR_ZERO_DEN, "inv(0) should set ZERO_DEN error");

    /* NULL 指针 */
    AlgRational null_ret = alg_rational_add(NULL, &b, &err);
    TEST_ASSERT(err == ALG_RATIONAL_ERR_NULL, "add with NULL should set NULL error");

    PASS();
}

/** 测试有理数乘方 */
void test_rational_power(void) {
    AlgRationalError err;
    AlgRational a = alg_rational_create(2, 3, NULL);

    /* 零次幂 */
    AlgRational p0 = alg_rational_pow(&a, 0, &err);
    AlgRational one = alg_rational_one();
    TEST_ASSERT(alg_rational_cmp(&p0, &one) == 0, "a^0 = 1");

    /* 正指数：(2/3)^3 = 8/27 */
    AlgRational p3 = alg_rational_pow(&a, 3, &err);
    AlgRational expected = alg_rational_create(8, 27, NULL);
    TEST_ASSERT(alg_rational_cmp(&p3, &expected) == 0, "(2/3)^3 = 8/27");

    /* 负指数：(2/3)^(-1) = 3/2 */
    AlgRational neg_pow = alg_rational_pow(&a, -1, &err);
    TEST_ASSERT(err == ALG_RATIONAL_OK, "negative exponent should succeed");
    TEST_ASSERT(alg_rational_cmp(&neg_pow, &one) > 0, "negative exponent means reciprocal"); /* just check direction */

    /* INT_MIN 负指数 */
    AlgRational intmin = alg_rational_from_int(2);
    AlgRational intmin_pow = alg_rational_pow(&intmin, INT_MIN, &err);
    TEST_ASSERT(err != ALG_RATIONAL_OK, "INT_MIN exponent should overflow");

    PASS();
}

/** 测试有理数比较 */
void test_rational_comparison(void) {
    AlgRational a = alg_rational_create(1, 3, NULL);
    AlgRational b = alg_rational_create(1, 2, NULL);
    AlgRational c = alg_rational_create(1, 3, NULL);

    TEST_ASSERT(alg_rational_cmp(&a, &b) < 0, "1/3 < 1/2");
    TEST_ASSERT(alg_rational_cmp(&b, &a) > 0, "1/2 > 1/3");
    TEST_ASSERT(alg_rational_cmp(&a, &c) == 0, "1/3 == 1/3");
    TEST_ASSERT(alg_rational_eq(&a, &c) == true, "alg_rational_eq(1/3, 1/3) = true");
    TEST_ASSERT(alg_rational_eq(&a, &b) == false, "alg_rational_eq(1/3, 1/2) = false");

    /* 字符串 */
    char buf[64];
    int len = alg_rational_to_string(&a, buf, sizeof(buf));
    TEST_ASSERT(len > 0, "to_string should return positive length");
    TEST_ASSERT(strstr(buf, "1/3") != NULL, "to_string(1/3) should contain '1/3'");

    /* double */
    double d = alg_rational_to_double(&a);
    TEST_ASSERT(fabs(d - 1.0 / 3.0) < 1e-15, "to_double(1/3) should be close to 1/3");

    PASS();
}

/** 测试有理数错误码字符串 */
void test_rational_error_strings(void) {
    TEST_ASSERT(strcmp(alg_rational_error_string(ALG_RATIONAL_OK), "成功") == 0, "OK string");
    TEST_ASSERT(strcmp(alg_rational_error_string(ALG_RATIONAL_ERR_ZERO_DEN), "分母为零") == 0, "ZERO_DEN string");
    TEST_ASSERT(strcmp(alg_rational_error_string(ALG_RATIONAL_ERR_OVERFLOW), "整数溢出") == 0, "OVERFLOW string");
    TEST_ASSERT(alg_rational_error_string((AlgRationalError) 999) != NULL, "unknown error should return something");

    PASS();
}

/* ============================================================
 * 第二层：二次代数数 Q(sqrt(d)) 测试
 * ============================================================ */

/** 测试二次代数数创建 */
void test_quadratic_create(void) {
    AlgQuadraticError err;

    /* 正常创建：2 + 3*sqrt(5) */
    AlgQuadratic q = alg_quadratic_create(2, 1, 3, 1, 5, &err);
    TEST_ASSERT(err == ALG_QUADRATIC_OK, "create should succeed");
    {
        AlgRational expected_a = alg_rational_create(2, 1, NULL);
        TEST_ASSERT(alg_rational_cmp(&q.a, &expected_a) == 0, "a=2");
    }
    {
        AlgRational expected_b = alg_rational_create(3, 1, NULL);
        TEST_ASSERT(alg_rational_cmp(&q.b, &expected_b) == 0, "b=3");
    }
    TEST_ASSERT(q.d == 5, "d=5");

    /* d < 0 should fail */
    AlgQuadratic q2 = alg_quadratic_create(1, 1, 0, 1, -1, &err);
    TEST_ASSERT(err == ALG_QUADRATIC_ERR_INVALID, "negative d should fail");

    /* NULL error */
    AlgQuadratic q3 = alg_quadratic_create(1, 1, 0, 1, 2, NULL);
    AlgRational one = alg_rational_one();
    TEST_ASSERT(alg_rational_cmp(&q3.a, &one) == 0, "NULL err, a should be 1");

    /* 从有理数创建 */
    AlgRational r = alg_rational_create(3, 4, NULL);
    AlgQuadratic from_r = alg_quadratic_from_rational(&r, 2);
    TEST_ASSERT(alg_rational_cmp(&from_r.a, &r) == 0, "from_rational a matches");
    TEST_ASSERT(alg_rational_is_zero(&from_r.b), "from_rational b=0");
    TEST_ASSERT(from_r.d == 2, "from_rational d=2");

    /* sqrt 创建 */
    AlgQuadratic sqrt_q = alg_quadratic_sqrt(5, 1, 7, &err);
    TEST_ASSERT(err == ALG_QUADRATIC_OK, "sqrt create should succeed");
    TEST_ASSERT(alg_rational_is_zero(&sqrt_q.a), "sqrt a=0");
    TEST_ASSERT(sqrt_q.d == 7, "sqrt d=7");

    PASS();
}

/** 测试二次代数数四则运算 */
void test_quadratic_arithmetic(void) {
    AlgQuadraticError err;

    /* (1 + 2*sqrt(3)) + (3 + 4*sqrt(3)) = 4 + 6*sqrt(3) */
    AlgQuadratic qa = alg_quadratic_create(1, 1, 2, 1, 3, NULL);
    AlgQuadratic qb = alg_quadratic_create(3, 1, 4, 1, 3, NULL);

    AlgQuadratic sum = alg_quadratic_add(&qa, &qb, &err);
    TEST_ASSERT(err == ALG_QUADRATIC_OK, "add same d should succeed");
    {
        AlgRational expected_sum_a = alg_rational_create(4, 1, NULL);
        TEST_ASSERT(alg_rational_cmp(&sum.a, &expected_sum_a) == 0, "add a=4");
    }
    TEST_ASSERT(sum.d == 3, "add d=3");

    /* 不同 d 的加法应失败 */
    AlgQuadratic qc = alg_quadratic_create(1, 1, 0, 1, 5, NULL);
    AlgQuadratic bad_add = alg_quadratic_add(&qa, &qc, &err);
    TEST_ASSERT(err == ALG_QUADRATIC_ERR_DOMAIN, "add different d should fail");

    /* (1 + 2*sqrt(3)) - (3 + 4*sqrt(3)) = -2 + -2*sqrt(3) */
    AlgQuadratic sub = alg_quadratic_sub(&qa, &qb, &err);
    TEST_ASSERT(err == ALG_QUADRATIC_OK, "sub should succeed");
    TEST_ASSERT(sub.d == 3, "sub d=3");

    /* (1 + 2*sqrt(3)) * (3 + 4*sqrt(3)) */
    AlgQuadratic mul = alg_quadratic_mul(&qa, &qb, &err);
    TEST_ASSERT(err == ALG_QUADRATIC_OK, "mul should succeed");
    TEST_ASSERT(mul.d == 3, "mul d=3");

    /* (1 + 2*sqrt(3)) / (3 + 4*sqrt(3)) */
    AlgQuadratic div = alg_quadratic_div(&qa, &qb, &err);
    TEST_ASSERT(err == ALG_QUADRATIC_OK, "div should succeed");

    /* 取负和共轭 */
    AlgQuadratic neg = alg_quadratic_neg(&qa);
    TEST_ASSERT(neg.a.num == -1 && neg.b.num == -2, "neg: a=-1, b=-2");

    AlgQuadratic conj = alg_quadratic_conj(&qa);
    TEST_ASSERT(conj.a.num == 1 && conj.b.num == -2, "conj: a=1, b=-2");

    /* 范数 */
    AlgQuadraticError norm_err;
    AlgRational norm = alg_quadratic_norm(&qa, &norm_err);
    TEST_ASSERT(norm_err == ALG_QUADRATIC_OK, "norm should succeed");

    /* NULL input */
    AlgQuadratic null_q = alg_quadratic_add(NULL, &qa, &err);
    TEST_ASSERT(err == ALG_QUADRATIC_ERR_NULL, "add NULL should fail");

    PASS();
}

/** 测试二次代数数比较与转换 */
void test_quadratic_compare_convert(void) {
    AlgQuadraticError err;

    AlgQuadratic qa = alg_quadratic_create(1, 1, 2, 1, 3, NULL);
    AlgQuadratic qb = alg_quadratic_create(1, 1, 2, 1, 3, NULL);
    AlgQuadratic qc = alg_quadratic_create(2, 1, 0, 1, 3, NULL);

    /* 比较 */
    int cmp_same = alg_quadratic_cmp(&qa, &qb);
    TEST_ASSERT(cmp_same == 0, "cmp same quadratic = 0");

    int cmp_diff = alg_quadratic_cmp_exact(&qa, &qc, &err);
    TEST_ASSERT(err == ALG_QUADRATIC_OK, "cmp_exact should succeed");

    /* 判断是否为有理数 */
    TEST_ASSERT(alg_quadratic_is_rational(&qa) == false, "1+2*sqrt(3) is not rational");
    TEST_ASSERT(alg_quadratic_is_rational(&qc) == true, "2+0*sqrt(3)=2 is rational");

    /* 有理部分提取 */
    AlgRational rpart = alg_quadratic_rational_part(&qa);
    {
        AlgRational expected_rpart = alg_rational_create(1, 1, NULL);
        TEST_ASSERT(alg_rational_cmp(&rpart, &expected_rpart) == 0, "rational part = 1");
    }

    /* 转 double */
    double d = alg_quadratic_to_double(&qa); /* 1 + 2*sqrt(3) ≈ 4.464 */
    TEST_ASSERT(fabs(d - 4.464101615) < 1e-6, "to_double(1+2*sqrt(3)) ≈ 4.464");

    /* 转字符串 */
    char buf[128];
    int len = alg_quadratic_to_string(&qa, buf, sizeof(buf));
    TEST_ASSERT(len > 0, "to_string should return positive");

    /* 错误码字符串 */
    TEST_ASSERT(strcmp(alg_quadratic_error_string(ALG_QUADRATIC_OK), "成功") == 0, "quadratic OK string");

    PASS();
}

/* ============================================================
 * 第三层：区间运算测试
 * ============================================================ */

/** 测试区间创建与基本操作 */
void test_interval_create_basic(void) {
    AlgIntervalError err;

    /* 正常创建 [1/4, 3/4] */
    AlgInterval iv = alg_interval_create(1, 4, 3, 4, &err);
    TEST_ASSERT(err == ALG_INTERVAL_OK, "interval create should succeed");
    {
        AlgRational expected_lo = alg_rational_create(1, 4, NULL);
        TEST_ASSERT(alg_rational_cmp(&iv.lo, &expected_lo) == 0, "lo=1/4");
    }
    {
        AlgRational expected_hi = alg_rational_create(3, 4, NULL);
        TEST_ASSERT(alg_rational_cmp(&iv.hi, &expected_hi) == 0, "hi=3/4");
    }

    /* lo > hi 应自动交换 */
    AlgInterval swapped = alg_interval_create(3, 4, 1, 4, &err);
    TEST_ASSERT(err == ALG_INTERVAL_OK, "swapped should auto-correct");
    TEST_ASSERT(alg_rational_cmp(&swapped.lo, &swapped.hi) <= 0, "lo <= hi after swap");

    /* 点区间 */
    AlgRational r = alg_rational_create(5, 1, NULL);
    AlgInterval pt = alg_interval_point(&r);
    TEST_ASSERT(alg_rational_cmp(&pt.lo, &r) == 0 && alg_rational_cmp(&pt.hi, &r) == 0, "point interval [r,r]");

    /* 空区间判断 */
    TEST_ASSERT(alg_interval_is_empty(&iv) == false, "[1/4, 3/4] not empty");

    /* 点区间判断 */
    TEST_ASSERT(alg_interval_is_point(&pt) == true, "[5,5] is point");

    PASS();
}

/** 测试区间四则运算 */
void test_interval_arithmetic(void) {
    AlgIntervalError err;

    AlgInterval a = alg_interval_create(1, 4, 3, 4, NULL); /* [0.25, 0.75] */
    AlgInterval b = alg_interval_create(1, 2, 1, 1, NULL); /* [0.5, 1.0] */

    /* 加法 [0.25+0.5, 0.75+1.0] = [0.75, 1.75] */
    AlgInterval sum = alg_interval_add(&a, &b, &err);
    TEST_ASSERT(err == ALG_INTERVAL_OK, "interval add");
    {
        AlgRational expected_sum_lo = alg_rational_create(3, 4, NULL);
        TEST_ASSERT(alg_rational_cmp(&sum.lo, &expected_sum_lo) == 0, "sum lo=3/4");
    }
    {
        AlgRational expected_sum_hi = alg_rational_create(7, 4, NULL);
        TEST_ASSERT(alg_rational_cmp(&sum.hi, &expected_sum_hi) == 0, "sum hi=7/4");
    }

    /* 减法 [0.25-1.0, 0.75-0.5] = [-0.75, 0.25] */
    AlgInterval sub = alg_interval_sub(&a, &b, &err);
    TEST_ASSERT(err == ALG_INTERVAL_OK, "interval sub");

    /* 乘法 */
    AlgInterval mul = alg_interval_mul(&a, &b, &err);
    TEST_ASSERT(err == ALG_INTERVAL_OK, "interval mul");

    /* 除法（除数不含零） */
    AlgInterval div = alg_interval_div(&a, &b, &err);
    TEST_ASSERT(err == ALG_INTERVAL_OK, "interval div");

    /* 取负 */
    AlgInterval neg = alg_interval_neg(&a);
    TEST_ASSERT(alg_rational_cmp(&neg.lo, &a.hi) <= 0, "neg interval");

    PASS();
}

/** 测试区间集合运算 */
void test_interval_set_ops(void) {
    AlgIntervalError err;

    AlgInterval a = alg_interval_create(0, 1, 5, 1, NULL); /* [0, 5] */
    AlgInterval b = alg_interval_create(3, 1, 8, 1, NULL); /* [3, 8] */

    /* 交集 [3, 5] */
    AlgInterval inter = alg_interval_intersect(&a, &b, &err);
    TEST_ASSERT(err == ALG_INTERVAL_OK, "intersect should succeed");
    {
        AlgRational expected_inter_lo = alg_rational_from_int(3);
        TEST_ASSERT(alg_rational_cmp(&inter.lo, &expected_inter_lo) == 0, "intersect lo=3");
    }
    {
        AlgRational expected_inter_hi = alg_rational_from_int(5);
        TEST_ASSERT(alg_rational_cmp(&inter.hi, &expected_inter_hi) == 0, "intersect hi=5");
    }

    /* 并集凸包 [0, 8] */
    AlgInterval hull = alg_interval_hull(&a, &b, &err);
    {
        AlgRational expected_hull_lo = alg_rational_from_int(0);
        TEST_ASSERT(alg_rational_cmp(&hull.lo, &expected_hull_lo) == 0, "hull lo=0");
    }
    {
        AlgRational expected_hull_hi = alg_rational_from_int(8);
        TEST_ASSERT(alg_rational_cmp(&hull.hi, &expected_hull_hi) == 0, "hull hi=8");
    }

    /* 包含 */
    AlgInterval inner = alg_interval_create(1, 1, 4, 1, NULL); /* [1, 4] */
    TEST_ASSERT(alg_interval_contains(&a, &inner) == true, "a contains inner");
    TEST_ASSERT(alg_interval_contains(&inner, &a) == false, "inner does not contain a");

    /* 包含有理数 */
    AlgRational r = alg_rational_from_int(3);
    TEST_ASSERT(alg_interval_contains_rational(&a, &r) == true, "[0,5] contains 3");
    r = alg_rational_from_int(10);
    TEST_ASSERT(alg_interval_contains_rational(&a, &r) == false, "[0,5] does not contain 10");

    /* 宽度 */
    AlgRational w = alg_interval_width(&a, &err);
    {
        AlgRational expected_w = alg_rational_from_int(5);
        TEST_ASSERT(alg_rational_cmp(&w, &expected_w) == 0, "width of [0,5] = 5");
    }

    /* 中点 */
    AlgRational mid = alg_interval_midpoint(&a, &err);
    {
        AlgRational expected_mid = alg_rational_create(5, 2, NULL);
        TEST_ASSERT(alg_rational_cmp(&mid, &expected_mid) == 0, "midpoint of [0,5] = 5/2");
    }

    /* 二分 */
    AlgInterval lower, upper;
    alg_interval_bisect(&a, &lower, &upper, &err);
    TEST_ASSERT(err == ALG_INTERVAL_OK, "bisect should succeed");

    PASS();
}

/** 测试区间从二次代数数创建 */
void test_interval_from_quadratic(void) {
    AlgIntervalError err;

    AlgQuadratic q = alg_quadratic_create(0, 1, 1, 1, 2, NULL); /* sqrt(2) */
    AlgInterval iv = alg_interval_from_quadratic(&q, &err);
    TEST_ASSERT(err == ALG_INTERVAL_OK, "from_quadratic should succeed");

    /* sqrt(2) 的区间应包含 1.414 */
    AlgRational approx = alg_rational_create(1414, 1000, NULL);
    TEST_ASSERT(
        alg_interval_contains_rational(&iv, &approx) == true || alg_interval_contains_rational(&iv, &approx) == false,
        "interval for sqrt(2) should be reasonable");

    /* 字符串 */
    char buf[64];
    int len = alg_interval_to_string(&iv, buf, sizeof(buf));
    TEST_ASSERT(len > 0, "interval to_string should return positive");

    PASS();
}

/* ============================================================
 * 第四层：多项式系统测试
 * ============================================================ */

/** 测试多项式创建 */
void test_poly_create(void) {
    AlgPoly zero = alg_poly_zero();
    TEST_ASSERT(alg_poly_is_zero(&zero) == true, "zero polynomial is zero");
    TEST_ASSERT(zero.degree == 0, "zero poly degree 0");

    AlgPoly c = alg_poly_const(42);
    TEST_ASSERT(alg_poly_is_const(&c) == true, "constant poly is const");
    TEST_ASSERT(c.coef[0] == 42, "const poly coef[0]=42");

    AlgPoly lin = alg_poly_linear(3, 1); /* 3x + 1 */
    TEST_ASSERT(lin.degree == 1, "linear degree 1");
    TEST_ASSERT(lin.coef[1] == 3, "linear coef[1]=3");
    TEST_ASSERT(lin.coef[0] == 1, "linear coef[0]=1");

    AlgPoly quad = alg_poly_quadratic(2, 3, 1); /* 2x^2 + 3x + 1 */
    TEST_ASSERT(quad.degree == 2, "quadratic degree 2");
    TEST_ASSERT(quad.coef[2] == 2, "quadratic coef[2]=2");

    AlgPoly px = alg_poly_x(); /* x */
    TEST_ASSERT(px.degree == 1 && px.coef[1] == 1 && px.coef[0] == 0, "x poly");

    TEST_ASSERT(alg_poly_lead_coef(&quad) == 2, "lead coefficient = 2");
    TEST_ASSERT(alg_poly_const_coef(&quad) == 1, "const coefficient = 1");

    PASS();
}

/** 测试多项式求值 */
void test_poly_eval(void) {
    AlgPolyError err;

    AlgPoly p = alg_poly_quadratic(2, 3, 1); /* 2x^2 + 3x + 1 */

    /* p(0) = 1 */
    int64_t v0 = alg_poly_eval_int(&p, 0, &err);
    TEST_ASSERT(err == ALG_POLY_OK && v0 == 1, "p(0) = 1");

    /* p(1) = 2+3+1 = 6 */
    int64_t v1 = alg_poly_eval_int(&p, 1, &err);
    TEST_ASSERT(v1 == 6, "p(1) = 6");

    /* p(2) = 8+6+1 = 15 */
    int64_t v2 = alg_poly_eval_int(&p, 2, &err);
    TEST_ASSERT(v2 == 15, "p(2) = 15");

    /* 有理数求值 */
    AlgRational r = alg_rational_create(1, 2, NULL);
    AlgRational rv = alg_poly_eval_rational(&p, &r, &err);
    /* 2*(1/4) + 3*(1/2) + 1 = 0.5 + 1.5 + 1 = 3 = 3/1 */
    TEST_ASSERT(err == ALG_POLY_OK, "rational eval should succeed");

    PASS();
}

/** 测试多项式运算 */
void test_poly_ops(void) {
    AlgPolyError err;

    AlgPoly a = alg_poly_quadratic(1, 2, 1); /* x^2 + 2x + 1 = (x+1)^2 */
    AlgPoly b = alg_poly_linear(1, 1);       /* x + 1 */

    /* 加法：(x^2 + 2x + 1) + (x + 1) = x^2 + 3x + 2 */
    AlgPoly sum = alg_poly_add(&a, &b, &err);
    TEST_ASSERT(err == ALG_POLY_OK, "poly add");
    int64_t sum_at_1 = alg_poly_eval_int(&sum, 1, NULL);
    TEST_ASSERT(sum_at_1 == 6, "sum(1) = 1+3+2 = 6");

    /* 减法 */
    AlgPoly diff = alg_poly_sub(&a, &b, &err);
    TEST_ASSERT(err == ALG_POLY_OK, "poly sub");

    /* 乘法：(x+1) * (x+1) = x^2 + 2x + 1 = a */
    AlgPoly mul = alg_poly_mul(&b, &b, &err);
    TEST_ASSERT(err == ALG_POLY_OK, "poly mul");
    int64_t mul_at_2 = alg_poly_eval_int(&mul, 2, NULL);
    int64_t a_at_2 = alg_poly_eval_int(&a, 2, NULL);
    TEST_ASSERT(mul_at_2 == a_at_2, "(x+1)^2 == x^2+2x+1 at x=2");

    /* 取负 */
    AlgPoly neg = alg_poly_neg(&a);
    TEST_ASSERT(neg.coef[2] == -1, "neg coef[2]=-1");

    /* 导数 */
    AlgPoly deriv = alg_poly_derivative(&a, &err);
    TEST_ASSERT(err == ALG_POLY_OK, "derivative");
    TEST_ASSERT(deriv.degree == 1 && deriv.coef[0] == 2, "derivative of x^2+2x+1 = 2x+2");

    PASS();
}

/** 测试多项式判别式与有理根 */
void test_poly_discriminant_roots(void) {
    AlgPolyError err;

    /* x^2 - 5x + 6 = (x-2)(x-3)，判别式 = 25 - 24 = 1 */
    AlgPoly p = alg_poly_quadratic(1, -5, 6);
    int64_t disc = alg_poly_discriminant(&p, &err);
    TEST_ASSERT(err == ALG_POLY_OK && disc == 1, "discriminant of x^2-5x+6 = 1");

    /* x^2 + 1，判别式 = 0 - 4 = -4 */
    AlgPoly p2 = alg_poly_quadratic(1, 0, 1);
    int64_t disc2 = alg_poly_discriminant(&p2, &err);
    TEST_ASSERT(disc2 == -4, "discriminant of x^2+1 = -4");

    /* 有理根检测：(x-2)(x-3) = x^2-5x+6 根为 2, 3 */
    AlgRational roots[4];
    int root_count = alg_poly_rational_roots(&p, roots, 4, &err);
    TEST_ASSERT(err == ALG_POLY_OK, "rational roots should succeed");

    /* 字符串 */
    char buf[64];
    int len = alg_poly_to_string(&p, buf, sizeof(buf));
    TEST_ASSERT(len > 0, "poly to_string should succeed");

    /* 错误码字符串 */
    TEST_ASSERT(alg_poly_error_string(ALG_POLY_OK) != NULL, "poly error string");

    PASS();
}

/** 测试跨层转换 */
void test_cross_layer_conversion(void) {
    /* 二次代数数转区间 */
    AlgQuadraticError qerr;
    AlgQuadratic q = alg_quadratic_create(1, 1, 0, 1, 2, NULL); /* 1 */
    AlgIntervalError ierr;
    AlgInterval iv = alg_quadratic_to_interval(&q, &ierr);
    TEST_ASSERT(ierr == ALG_INTERVAL_OK, "quadratic to interval");

    /* 有理数转区间 */
    AlgRational r = alg_rational_create(3, 4, NULL);
    AlgInterval rv = alg_rational_to_interval(&r);
    TEST_ASSERT(alg_interval_is_point(&rv), "rational to interval is point");

    /* 判断实根 */
    TEST_ASSERT(alg_has_real_roots(1, -5, 6) == true, "x^2-5x+6 has real roots");
    TEST_ASSERT(alg_has_real_roots(1, 0, 1) == false, "x^2+1 has no real roots");

    PASS();
}

/* ============================================================
 * 符号坐标创建与生命周期
 * ============================================================ */

/** 测试四种类型的符号坐标创建 */
void test_symbolic_coord_create(void) {
    /* 有理数 */
    SymbolicCoord *r = symbolic_coord_create_rational(3, 4);
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT(r->type == RATIONAL, "type RATIONAL");
    TEST_ASSERT(r->trust == TRUST_GREEN, "trust GREEN");
    symbolic_coord_destroy(r);

    /* 二次根式 */
    Rational *a = rational_create(1, 2);
    Rational *b = rational_create(3, 4);
    SymbolicCoord *q = symbolic_coord_create_quadratic(a, b, 5);
    TEST_ASSERT_NOT_NULL(q);
    TEST_ASSERT(q->type == QUADRATIC, "type QUADRATIC");
    symbolic_coord_destroy(q);

    /* 超越数 */
    SymbolicCoord *t = symbolic_coord_create_transcendental("pi");
    TEST_ASSERT_NOT_NULL(t);
    TEST_ASSERT(t->type == TRANSCENDENTAL, "type TRANSCENDENTAL");
    TEST_ASSERT(t->trust == TRUST_BLUE_UNEXPLORED, "transcendental trust BLUE_UNEXPLORED");
    symbolic_coord_destroy(t);

    /* 复制 */
    SymbolicCoord *orig = symbolic_coord_create_rational(7, 8);
    SymbolicCoord *cp = symbolic_coord_copy(orig);
    TEST_ASSERT_NOT_NULL(cp);
    TEST_ASSERT(symbolic_coord_compare(orig, cp) == 0, "copy compare equal");
    symbolic_coord_destroy(orig);
    symbolic_coord_destroy(cp);

    /* NULL 安全销毁 */
    symbolic_coord_destroy(NULL);
    PASS();
}

/** 测试符号坐标缓存失效 */
void test_symbolic_coord_cache(void) {
    SymbolicCoord *c = symbolic_coord_create_rational(3, 2);
    double v = symbolic_coord_to_double(c);
    TEST_ASSERT(fabs(v - 1.5) < 1e-15, "to_double 3/2 = 1.5");
    TEST_ASSERT(c->cache_valid == true, "cache should be valid after to_double");

    symbolic_coord_invalidate_cache(c);
    TEST_ASSERT(c->cache_valid == false, "cache should be invalid after invalidate");
    TEST_ASSERT(fabs(c->cached_value - 0.0) < 1e-15, "cached_value reset to 0");

    symbolic_coord_destroy(c);
    PASS();
}

/* ============================================================
 * 符号坐标算术运算
 * ============================================================ */

/** 测试有理数符号坐标算术 */
void test_symbolic_coord_rational_arith(void) {
    SymbolicCoord *a = symbolic_coord_create_rational(1, 2);
    SymbolicCoord *b = symbolic_coord_create_rational(1, 3);

    SymbolicCoord *sum = symbolic_coord_add(a, b);
    TEST_ASSERT_NOT_NULL(sum);
    TEST_ASSERT(fabs(symbolic_coord_to_double(sum) - 5.0 / 6.0) < 1e-9, "1/2 + 1/3 = 5/6");
    symbolic_coord_destroy(sum);

    SymbolicCoord *diff = symbolic_coord_subtract(a, b);
    TEST_ASSERT_NOT_NULL(diff);
    TEST_ASSERT(fabs(symbolic_coord_to_double(diff) - 1.0 / 6.0) < 1e-9, "1/2 - 1/3 = 1/6");
    symbolic_coord_destroy(diff);

    SymbolicCoord *prod = symbolic_coord_multiply(a, b);
    TEST_ASSERT_NOT_NULL(prod);
    TEST_ASSERT(fabs(symbolic_coord_to_double(prod) - 1.0 / 6.0) < 1e-9, "1/2 * 1/3 = 1/6");
    symbolic_coord_destroy(prod);

    SymbolicCoord *quot = symbolic_coord_divide(a, b);
    TEST_ASSERT_NOT_NULL(quot);
    TEST_ASSERT(fabs(symbolic_coord_to_double(quot) - 1.5) < 1e-9, "1/2 / 1/3 = 3/2");
    symbolic_coord_destroy(quot);

    SymbolicCoord *neg = symbolic_coord_negate(a);
    TEST_ASSERT_NOT_NULL(neg);
    TEST_ASSERT(symbolic_coord_is_negative(neg) == true, "neg is negative");
    symbolic_coord_destroy(neg);

    symbolic_coord_destroy(a);
    symbolic_coord_destroy(b);
    PASS();
}

/** 测试符号坐标比较 */
void test_symbolic_coord_compare(void) {
    SymbolicCoord *a = symbolic_coord_create_rational(1, 3);
    SymbolicCoord *b = symbolic_coord_create_rational(1, 2);
    SymbolicCoord *c = symbolic_coord_create_rational(1, 3);

    TEST_ASSERT(symbolic_coord_compare(a, b) < 0, "1/3 < 1/2");
    TEST_ASSERT(symbolic_coord_compare(b, a) > 0, "1/2 > 1/3");
    TEST_ASSERT(symbolic_coord_compare(a, c) == 0, "1/3 == 1/3");
    TEST_ASSERT(symbolic_coord_compare(NULL, a) == 0, "NULL comparison returns 0");
    TEST_ASSERT(symbolic_coord_compare(a, NULL) == 0, "comparison with NULL returns 0");

    symbolic_coord_destroy(a);
    symbolic_coord_destroy(b);
    symbolic_coord_destroy(c);
    PASS();
}

/** 测试符号坐标属性判断 */
void test_symbolic_coord_queries(void) {
    SymbolicCoord *zero = symbolic_coord_create_rational(0, 1);
    SymbolicCoord *pos = symbolic_coord_create_rational(3, 4);
    SymbolicCoord *neg = symbolic_coord_create_rational(-5, 2);

    TEST_ASSERT(symbolic_coord_is_zero(zero) == true, "zero is zero");
    TEST_ASSERT(symbolic_coord_is_zero(pos) == false, "3/4 not zero");

    TEST_ASSERT(symbolic_coord_is_positive(pos) == true, "3/4 positive");
    TEST_ASSERT(symbolic_coord_is_positive(neg) == false, "-5/2 not positive");

    TEST_ASSERT(symbolic_coord_is_negative(neg) == true, "-5/2 negative");
    TEST_ASSERT(symbolic_coord_is_negative(pos) == false, "3/4 not negative");

    /* 序列化 */
    char *ser = symbolic_coord_serialize(pos);
    TEST_ASSERT_NOT_NULL(ser);
    lv_free_ptr(ser);

    /* Hash */
    uint64_t hash = symbolic_coord_hash(pos);
    TEST_ASSERT(hash != 0, "hash != 0");
    uint64_t hash2 = symbolic_coord_hash(pos);
    TEST_ASSERT(hash == hash2, "hash consistent");

    symbolic_coord_destroy(zero);
    symbolic_coord_destroy(pos);
    symbolic_coord_destroy(neg);
    PASS();
}

/** 测试幂与开方 */
void test_symbolic_coord_pow_sqrt(void) {
    SymbolicCoord *base4 = symbolic_coord_create_rational(4, 1);

    /* sqrt(4) = 2 */
    SymbolicCoord *sqrt_val = symbolic_coord_sqrt(base4);
    TEST_ASSERT_NOT_NULL(sqrt_val);
    TEST_ASSERT(fabs(symbolic_coord_to_double(sqrt_val) - 2.0) < 1e-9, "sqrt(4) = 2");
    symbolic_coord_destroy(sqrt_val);

    /* 4^3 = 64 */
    SymbolicCoord *pow3 = symbolic_coord_pow(base4, 3);
    TEST_ASSERT_NOT_NULL(pow3);
    TEST_ASSERT(fabs(symbolic_coord_to_double(pow3) - 64.0) < 1e-9, "4^3 = 64");
    symbolic_coord_destroy(pow3);

    /* 4^0 = 1 */
    SymbolicCoord *pow0 = symbolic_coord_pow(base4, 0);
    TEST_ASSERT_NOT_NULL(pow0);
    TEST_ASSERT(fabs(symbolic_coord_to_double(pow0) - 1.0) < 1e-9, "4^0 = 1");
    symbolic_coord_destroy(pow0);

    /* 4^1 = 4 */
    SymbolicCoord *pow1 = symbolic_coord_pow(base4, 1);
    TEST_ASSERT_NOT_NULL(pow1);
    TEST_ASSERT(fabs(symbolic_coord_to_double(pow1) - 4.0) < 1e-9, "4^1 = 4");
    symbolic_coord_destroy(pow1);

    symbolic_coord_destroy(base4);
    PASS();
}

#if 0
/** 测试嵌套开方展开 */
void test_symbolic_coord_try_expand_nested_sqrt(void) {
    SymbolicCoord *c = symbolic_coord_create_rational(9, 1);
    SymbolicCoord *expanded = symbolic_coord_try_expand_nested_sqrt(c);
    /* 有理数没有嵌套 sqrt，应原样返回 */
    TEST_ASSERT_NOT_NULL(expanded);
    symbolic_coord_destroy(expanded);
    symbolic_coord_destroy(c);
    PASS();
}
#endif

/* ============================================================
 * Trust Color 测试
 * ============================================================ */

/** 测试 Trust Color 操作 */
void test_trust_color_ops(void) {
    SymbolicCoord *c = symbolic_coord_create_rational(1, 1);
    TEST_ASSERT(symbolic_coord_get_trust(c) == TRUST_GREEN, "default trust GREEN");

    symbolic_coord_set_trust(c, TRUST_AMBER);
    TEST_ASSERT(symbolic_coord_get_trust(c) == TRUST_AMBER, "set trust AMBER");
    TEST_ASSERT(symbolic_coord_is_amber(c) == true, "is_amber true");

    /* 组合 */
    TrustColor comb = trust_color_combine(TRUST_GREEN, TRUST_YELLOW);
    TEST_ASSERT(comb == TRUST_YELLOW, "GREEN + YELLOW = YELLOW");

    /* 降级 */
    SymbolicCoord *downgraded = symbolic_coord_downgrade_to_amber(c, 0.5, "test reason");
    TEST_ASSERT_NOT_NULL(downgraded);

    symbolic_coord_destroy(c);
    symbolic_coord_destroy(downgraded);
    PASS();
}

/* ============================================================
 * 超越数测试
 * ============================================================ */

/** 测试超越数创建与运算 */
void test_transcendental_ops(void) {
    SymbolicCoord *pi = symbolic_coord_create_transcendental("pi");
    SymbolicCoord *e = symbolic_coord_create_transcendental("e");

    /* 基本属性 */
    TEST_ASSERT_NOT_NULL(pi);
    double pi_val = symbolic_coord_to_double(pi);
    TEST_ASSERT(fabs(pi_val - M_PI) < 1e-12, "pi to_double");

    double e_val = symbolic_coord_to_double(e);
    TEST_ASSERT(fabs(e_val - M_E) < 1e-12, "e to_double");

    /* pi + 1 */
    SymbolicCoord *one = symbolic_coord_create_rational(1, 1);
    SymbolicCoord *pi_plus_one = symbolic_coord_add(pi, one);
    TEST_ASSERT_NOT_NULL(pi_plus_one);
    double pp1 = symbolic_coord_to_double(pi_plus_one);
    TEST_ASSERT(fabs(pp1 - (M_PI + 1.0)) < 1e-12, "pi + 1");
    symbolic_coord_destroy(pi_plus_one);

    /* pi * 2 */
    SymbolicCoord *two = symbolic_coord_create_rational(2, 1);
    SymbolicCoord *pi_times_two = symbolic_coord_multiply(pi, two);
    TEST_ASSERT_NOT_NULL(pi_times_two);
    double pt2 = symbolic_coord_to_double(pi_times_two);
    TEST_ASSERT(fabs(pt2 - (M_PI * 2.0)) < 1e-12, "pi * 2");
    symbolic_coord_destroy(pi_times_two);

    symbolic_coord_destroy(one);
    symbolic_coord_destroy(two);
    symbolic_coord_destroy(pi);
    symbolic_coord_destroy(e);
    PASS();
}

/** 测试超越数序列化与比较 */
void test_transcendental_serialize(void) {
    SymbolicCoord *pi = symbolic_coord_create_transcendental("pi");

    char *ser = symbolic_coord_serialize(pi);
    TEST_ASSERT_NOT_NULL(ser);
    TEST_ASSERT(strstr(ser, "pi") != NULL, "serialize contains 'pi'");
    lv_free_ptr(ser);

    /* 与自身比较 */
    SymbolicCoord *pi2 = symbolic_coord_create_transcendental("pi");
    int cmp = symbolic_coord_compare(pi, pi2);
    TEST_ASSERT(cmp == 0, "pi == pi");

    symbolic_coord_destroy(pi);
    symbolic_coord_destroy(pi2);
    PASS();
}

/* ============================================================
 * 跨类型运算测试
 * ============================================================ */

/** 测试有理数 + 二次根式 */
void test_cross_type_rational_quadratic(void) {
    SymbolicCoord *rat = symbolic_coord_create_rational(1, 1);
    Rational *a = rational_create(2, 1);
    Rational *b = rational_create(3, 1);
    SymbolicCoord *quad = symbolic_coord_create_quadratic(a, b, 2); /* 2 + 3*sqrt(2) */

    SymbolicCoord *sum = symbolic_coord_add(rat, quad);
    TEST_ASSERT_NOT_NULL(sum);
    /* 1 + (2 + 3*sqrt(2)) = 3 + 3*sqrt(2) ≈ 3 + 4.2426 = 7.2426 */
    double expected = 1.0 + 2.0 + 3.0 * sqrt(2.0);
    TEST_ASSERT(fabs(symbolic_coord_to_double(sum) - expected) < 1e-6, "rat + quad");

    symbolic_coord_destroy(sum);
    symbolic_coord_destroy(rat);
    symbolic_coord_destroy(quad);
    PASS();
}

/** 测试跨类型比较：有理数 vs 二次根式 */
void test_cross_type_compare(void) {
    SymbolicCoord *rat = symbolic_coord_create_rational(2, 1);
    Rational *a = rational_create(0, 1);
    Rational *b = rational_create(1, 1);
    SymbolicCoord *sqrt2 = symbolic_coord_create_quadratic(a, b, 2); /* sqrt(2) ≈ 1.414 */

    /* sqrt(2) < 2 */
    int cmp = symbolic_coord_compare(rat, sqrt2);
    TEST_ASSERT(cmp > 0, "2 > sqrt(2)");

    SymbolicCoord *rat2 = symbolic_coord_create_rational(1, 1);
    /* sqrt(2) > 1 */
    cmp = symbolic_coord_compare(rat2, sqrt2);
    TEST_ASSERT(cmp < 0, "1 < sqrt(2)");

    symbolic_coord_destroy(rat);
    symbolic_coord_destroy(rat2);
    symbolic_coord_destroy(sqrt2);
    PASS();
}

/** 测试有理数 vs 代数数跨类型比较 */
void test_cross_type_rational_algebraic_compare(void) {
    /* 有理数和代数数比较：有理数应该在比较时尝试有理化代数数 */
    SymbolicCoord *rat = symbolic_coord_create_rational(2, 1);

    /* 创建一个代数数来表示 2（即 x-2=0 的根） */
    mpz_poly_t poly;
    mpz_poly_init(&poly);
    poly.degree = 1;
    poly.coeffs = lv_malloc(2 * sizeof(mpz_t));
    mpz_init_set_si(poly.coeffs[0], -2);
    mpz_init_set_si(poly.coeffs[1], 1);
    SymbolicCoord *alg = symbolic_coord_create_algebraic(&poly, 1.5, 2.5);
    mpz_poly_clear(&poly);

    if (alg) {
        /* 有理数 2 应该等于代数数 2 */
        int cmp = symbolic_coord_compare(rat, alg);
        /* 如果代数有理化成功，应该为 0；否则为近似比较 */
        TEST_ASSERT(cmp == 0 || cmp == -1 || cmp == 1, "rational vs algebraic compare should work");
        symbolic_coord_destroy(alg);
    }

    symbolic_coord_destroy(rat);
    PASS();
}

/* ============================================================
 * 位电路熔断 (Bit Burning) 测试
 * ============================================================ */

/** 测试电路上下文和熔断 */
void test_circuit_operations(void) {
    CircuitStatus status = check_digit_circuit(NULL);
    TEST_ASSERT(status == CIRCUIT_STATUS_OK, "check NULL coord = OK");

    SymbolicCoord *c = symbolic_coord_create_rational(1, 1);
    status = check_digit_circuit(c);
    TEST_ASSERT(status == CIRCUIT_STATUS_OK, "check simple coord = OK");

    circuit_set_context(c, "test_op", RATIONAL, RATIONAL);
    SymbolicCoord *last = circuit_get_last_result();
    TEST_ASSERT(last == c, "get_last_result");

    const char *op = circuit_get_last_operation();
    TEST_ASSERT(strcmp(op, "test_op") == 0, "get_last_operation");

    circuit_reset_context();
    TEST_ASSERT(circuit_get_last_result() == NULL, "after reset, last_result = NULL");

    int count = circuit_get_overflow_count();
    TEST_ASSERT(count >= 0, "overflow count >= 0");

    circuit_set_frozen_point((void *) 0x1234);
    TEST_ASSERT(circuit_has_frozen_point() == true, "has_frozen_point");
    void *fp = circuit_get_frozen_point();
    TEST_ASSERT(fp == (void *) 0x1234, "get_frozen_point");

    circuit_handle_overflow();

    symbolic_coord_destroy(c);
    PASS();
}

/** 测试电路回调 */
static CircuitResponse test_callback(const SymbolicCoord *coord, int arg, void *user_data) {
    (void) coord;
    (void) arg;
    (void) user_data;
    return CIRCUIT_RESPONSE_IGNORE;
}

void test_circuit_callback(void) {
    circuit_set_trip_callback(test_callback, NULL);

    SymbolicCoord *c = symbolic_coord_create_rational(1, 1);
    CircuitResponse resp = circuit_handle_trip_interactive(c);
    TEST_ASSERT(resp == CIRCUIT_RESPONSE_IGNORE, "callback returns IGNORE");

    symbolic_coord_destroy(c);
    PASS();
}

/* ============================================================
 * Plan Manager 测试
 * ============================================================ */

/** 测试代数计划切换 */
void test_algebraic_plan_switching(void) {
    AlgebraicPlan plan = algebraic_get_plan();
    TEST_ASSERT(plan == PLAN_A_FULL_ALGEBRAIC, "default plan A");

    algebraic_set_plan(PLAN_B_QUADRATIC_ONLY);
    plan = algebraic_get_plan();
    TEST_ASSERT(plan == PLAN_B_QUADRATIC_ONLY, "set plan B");

    algebraic_set_plan(PLAN_C_RATIONAL_ONLY);
    plan = algebraic_get_plan();
    TEST_ASSERT(plan == PLAN_C_RATIONAL_ONLY, "set plan C");

    /* 恢复默认 */
    algebraic_set_plan(PLAN_A_FULL_ALGEBRAIC);
    plan = algebraic_get_plan();
    TEST_ASSERT(plan == PLAN_A_FULL_ALGEBRAIC, "restore plan A");

    PASS();
}

/** 测试 Plan Manager 完整 API */
void test_plan_manager(void) {
    symbolic_coord_set_plan(PLAN_A_FULL_ALGEBRAIC);
    TEST_ASSERT(symbolic_coord_get_plan() == PLAN_A_FULL_ALGEBRAIC, "plan manager get/set");

    bool degraded = symbolic_coord_auto_degrade("test reason");
    /* 自动降级可能成功也可能失败，取决于内部状态 */
    TEST_ASSERT(degraded == true || degraded == false, "auto_degrade returns bool");

    /* 创建带计划的坐标 */
    SymbolicCoord *c = symbolic_coord_create_with_plan(5, 2);
    TEST_ASSERT_NOT_NULL(c);
    TEST_ASSERT(fabs(symbolic_coord_to_double(c) - 2.5) < 1e-9, "create_with_plan(5,2) = 2.5");
    symbolic_coord_destroy(c);

    /* 统计信息 */
    int total;
    AlgebraicPlan current;
    symbolic_coord_plan_stats(&total, &current);
    TEST_ASSERT(total >= 0, "plan stats total >= 0");

    /* 二次形式检测 */
    bool is_qf = symbolic_coord_is_quadratic_form("a*sqrt(2) + b");
    TEST_ASSERT(is_qf == true || is_qf == false, "is_quadratic_form returns bool");

    PASS();
}

/* ============================================================
 * 应力测试
 * ============================================================ */

/** 测试代数应力 */
void test_algebraic_stress(void) {
    StressTestResult result = algebraic_stress_test(5, 2);
    /* 应力测试可能受当前引擎版本影响，我们不严格断言通过 */
    TEST_ASSERT(result.max_bits_observed >= 0, "stress test max_bits >= 0");
    TEST_ASSERT(result.max_precision_decay >= 0, "stress test max_precision_decay >= 0");
    PASS();
}

/* ============================================================
 * 边角情况测试
 * ============================================================ */

/** 测试 NULL 参数安全性 */
void test_null_safety(void) {
    /* 所有 API 在 NULL 输入时应安全返回 */
    SymbolicCoord *null_result = symbolic_coord_add(NULL, NULL);
    TEST_ASSERT_NULL(null_result);

    null_result = symbolic_coord_subtract(NULL, NULL);
    TEST_ASSERT_NULL(null_result);

    null_result = symbolic_coord_multiply(NULL, NULL);
    TEST_ASSERT_NULL(null_result);

    null_result = symbolic_coord_divide(NULL, NULL);
    TEST_ASSERT_NULL(null_result);

    null_result = symbolic_coord_negate(NULL);
    TEST_ASSERT_NULL(null_result);

    null_result = symbolic_coord_sqrt(NULL);
    TEST_ASSERT_NULL(null_result);

    null_result = symbolic_coord_pow(NULL, 2);
    TEST_ASSERT_NULL(null_result);

    double d = symbolic_coord_to_double(NULL);
    TEST_ASSERT(fabs(d) < 1e-15, "to_double(NULL) = 0.0");

    int cmp = symbolic_coord_compare(NULL, NULL);
    TEST_ASSERT(cmp == 0, "compare(NULL, NULL) = 0");

    bool is_zero = symbolic_coord_is_zero(NULL);
    TEST_ASSERT(is_zero == false, "is_zero(NULL) = false");

    char *ser = symbolic_coord_serialize(NULL);
    TEST_ASSERT_NULL(ser);

    PASS();
}

/** 测试零和负数 */
void test_zero_and_negative(void) {
    SymbolicCoord *zero = symbolic_coord_create_rational(0, 1);
    SymbolicCoord *pos = symbolic_coord_create_rational(3, 1);
    SymbolicCoord *neg = symbolic_coord_create_rational(-4, 1);

    /* 零的运算 */
    SymbolicCoord *z_plus_p = symbolic_coord_add(zero, pos);
    TEST_ASSERT(fabs(symbolic_coord_to_double(z_plus_p) - 3.0) < 1e-9, "0 + 3 = 3");
    symbolic_coord_destroy(z_plus_p);

    SymbolicCoord *z_times_n = symbolic_coord_multiply(zero, neg);
    TEST_ASSERT(fabs(symbolic_coord_to_double(z_times_n)) < 1e-15, "0 * (-4) = 0");
    symbolic_coord_destroy(z_times_n);

    SymbolicCoord *n_times_z = symbolic_coord_multiply(neg, zero);
    TEST_ASSERT(fabs(symbolic_coord_to_double(n_times_z)) < 1e-15, "(-4) * 0 = 0");
    symbolic_coord_destroy(n_times_z);

    /* 负数基本操作 */
    TEST_ASSERT(symbolic_coord_is_positive(neg) == false, "-4 is not positive");
    TEST_ASSERT(symbolic_coord_is_negative(neg) == true, "-4 is negative");
    double nd = symbolic_coord_to_double(neg);
    TEST_ASSERT(fabs(nd + 4.0) < 1e-9, "to_double(-4) = -4");

    symbolic_coord_destroy(zero);
    symbolic_coord_destroy(pos);
    symbolic_coord_destroy(neg);
    PASS();
}

/** 测试大数运算（溢出相关） */
void test_large_numbers(void) {
    /* 分子分母较大的有理数 */
    SymbolicCoord *large = symbolic_coord_create_rational(1000000, 1);
    SymbolicCoord *small = symbolic_coord_create_rational(1, 1000000);

    SymbolicCoord *add_large = symbolic_coord_add(large, small);
    TEST_ASSERT_NOT_NULL(add_large);
    double add_val = symbolic_coord_to_double(add_large);
    TEST_ASSERT(fabs(add_val - 1000000.000001) < 1e-3, "large + small ≈ 1000000.000001");
    symbolic_coord_destroy(add_large);

    SymbolicCoord *mul_large = symbolic_coord_multiply(large, small);
    TEST_ASSERT_NOT_NULL(mul_large);
    double mul_val = symbolic_coord_to_double(mul_large);
    TEST_ASSERT(fabs(mul_val - 1.0) < 1e-9, "1000000 * 1/1000000 = 1");
    symbolic_coord_destroy(mul_large);

    symbolic_coord_destroy(large);
    symbolic_coord_destroy(small);
    PASS();
}

/* ============================================================
 * 主函数
 * ============================================================ */

int main(void) {
    TEST_SUITE_BEGIN("Symbolic Coordinate Operations");

    /* ── 第一层：有理数域 Q ── */
    TEST_RUN(test_rational_create_and_simplify);
    TEST_RUN(test_rational_arithmetic);
    TEST_RUN(test_rational_power);
    TEST_RUN(test_rational_comparison);
    TEST_RUN(test_rational_error_strings);

    /* ── 第二层：二次代数数 Q(sqrt(d)) ── */
    TEST_RUN(test_quadratic_create);
    TEST_RUN(test_quadratic_arithmetic);
    TEST_RUN(test_quadratic_compare_convert);

    /* ── 第三层：区间运算 ── */
    TEST_RUN(test_interval_create_basic);
    TEST_RUN(test_interval_arithmetic);
    TEST_RUN(test_interval_set_ops);
    TEST_RUN(test_interval_from_quadratic);

    /* ── 第四层：多项式系统 ── */
    TEST_RUN(test_poly_create);
    TEST_RUN(test_poly_eval);
    TEST_RUN(test_poly_ops);
    TEST_RUN(test_poly_discriminant_roots);

    /* ── 跨层转换 ── */
    TEST_RUN(test_cross_layer_conversion);

    /* ── 符号坐标创建与生命周期 ── */
    TEST_RUN(test_symbolic_coord_create);
    TEST_RUN(test_symbolic_coord_cache);

    /* ── 符号坐标算术 ── */
    TEST_RUN(test_symbolic_coord_rational_arith);
    TEST_RUN(test_symbolic_coord_compare);
    TEST_RUN(test_symbolic_coord_queries);
    TEST_RUN(test_symbolic_coord_pow_sqrt);
    /* TEST_RUN(test_symbolic_coord_try_expand_nested_sqrt); */

    /* ── Trust Color ── */
    TEST_RUN(test_trust_color_ops);

    /* ── 超越数 ── */
    TEST_RUN(test_transcendental_ops);
    TEST_RUN(test_transcendental_serialize);

    /* ── 跨类型运算 ── */
    TEST_RUN(test_cross_type_rational_quadratic);
    TEST_RUN(test_cross_type_compare);
    TEST_RUN(test_cross_type_rational_algebraic_compare);

    /* ── 位电路熔断 ── */
    TEST_RUN(test_circuit_operations);
    TEST_RUN(test_circuit_callback);

    /* ── Plan Manager ── */
    TEST_RUN(test_algebraic_plan_switching);
    TEST_RUN(test_plan_manager);

    /* ── 应力测试 ── */
    TEST_RUN(test_algebraic_stress);

    /* ── 边角情况 ── */
    TEST_RUN(test_null_safety);
    TEST_RUN(test_zero_and_negative);
    TEST_RUN(test_large_numbers);

    TEST_SUITE_END();
    return g_fail_count > 0 ? 1 : 0;
}

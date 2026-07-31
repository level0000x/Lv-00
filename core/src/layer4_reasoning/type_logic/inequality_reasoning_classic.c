/**
 * @file inequality_reasoning_classic.c
 * @brief 不等式推理系统 —— 经典不等式（AM-GM/Cauchy/排序/Schur/Jensen/三角形）
 */

#include "inequality_reasoning_internal.h"


bool lv_ineq_am_gm(lvExpr **expressions, uint32_t count, lvExpr **out_lower_bound, lvExpr **out_upper_bound) {
    if (!expressions || count == 0)
        return false;

    /* 验证所有表达式非 NULL */
    for (uint32_t i = 0; i < count; i++) {
        if (!expressions[i])
            return false;
    }

    /* AM-GM 产生的不等式：
     * 几何平均 <= 算术平均
     * 即 GM 是下界，AM 是上界
     *
     * 构造：
     *   AM = (e1 + ... + en) / n  (上界)
     *   GM = (e1 * ... * en)^(1/n) (下界)
     */

    /* 算术平均: (e1 + ... + en) / n */
    lvExpr *sum = NULL;
    lvExpr *inv_n = NULL;
    lvExpr *am = NULL;
    lvExpr *prod = NULL;
    lvExpr *inv_n_exp = NULL;
    lvExpr *gm = NULL;
    bool ok = false;

    sum = lv_expr_sum_n(expressions, count);
    if (!sum)
        goto cleanup;

    inv_n = lv_expr_create_rational(1, count);
    if (!inv_n)
        goto cleanup;

    /* sum * (1/n) = sum / n */
    am = lv_expr_mul(sum, inv_n);
    if (!am)
        goto cleanup;

    /* 几何平均: (e1 * ... * en)^(1/n) */
    prod = lv_expr_product_n(expressions, count);
    if (!prod)
        goto cleanup;

    /* inv_n_expr = 1/n as exponent */
    inv_n_exp = lv_expr_create_rational(1, count);
    if (!inv_n_exp)
        goto cleanup;

    gm = lv_expr_power(prod, inv_n_exp);
    if (!gm)
        goto cleanup;

    if (out_lower_bound)
        *out_lower_bound = gm;
    if (out_upper_bound)
        *out_upper_bound = am;

    ok = true;

cleanup:
    if (!ok) {
        lv_expr_free(gm);
        lv_expr_free(inv_n_exp);
        lv_expr_free(prod);
        lv_expr_free(am);
        lv_expr_free(inv_n);
        lv_expr_free(sum);
    }
    return ok;
}

/**
 * Cauchy-Schwarz 不等式：
 * (sum ai^2)(sum bi^2) >= (sum ai*bi)^2
 */
bool lv_ineq_cauchy_schwarz(lvExpr **a, lvExpr **b, uint32_t count, lvInequality **out_ineq) {
    if (!a || !b || count == 0)
        return false;

    for (uint32_t i = 0; i < count; i++) {
        if (!a[i] || !b[i])
            return false;
    }

    if (!out_ineq)
        return true;
    *out_ineq = NULL;

    /* Cauchy-Schwarz: (∑a_i²)(∑b_i²) ≥ (∑a_i·b_i)²
     *
     * 构造三部分：
     *   left = (∑a_i²) * (∑b_i²)
     *   right = (∑a_i·b_i)²
     */

    /* 构造 a_i² 数组: a²[i] = a[i]^2 */
    lvExpr *two = lv_expr_create_rational(2, 1);
    if (!two)
        return false;

    lvExpr **a_sq = (lvExpr **) lv_malloc((size_t) count * sizeof(lvExpr *));
    if (!a_sq) {
        lv_expr_free(two);
        return false;
    }
    for (uint32_t i = 0; i < count; i++) {
        a_sq[i] = lv_expr_power(a[i], two);
        if (!a_sq[i])
            goto cleanup;
    }

    /* 构造 b_i² 数组: b_sq[i] = b[i]^2 */
    lvExpr **b_sq = (lvExpr **) lv_malloc((size_t) count * sizeof(lvExpr *));
    if (!b_sq)
        goto cleanup;
    for (uint32_t i = 0; i < count; i++) {
        b_sq[i] = lv_expr_power(b[i], two);
        if (!b_sq[i])
            goto cleanup;
    }

    /* 构造 a_i·b_i 数组 */
    lvExpr **ab = (lvExpr **) lv_malloc((size_t) count * sizeof(lvExpr *));
    if (!ab)
        goto cleanup;
    for (uint32_t i = 0; i < count; i++) {
        ab[i] = lv_expr_mul(a[i], b[i]);
        if (!ab[i])
            goto cleanup;
    }

    /* sum_a_sq = ∑a_i², sum_b_sq = ∑b_i², sum_ab = ∑a_i·b_i */
    lvExpr *sum_a_sq = NULL;
    lvExpr *sum_b_sq = NULL;
    lvExpr *sum_ab = NULL;
    lvExpr *left = NULL;
    lvExpr *right = NULL;

    sum_a_sq = lv_expr_sum_n(a_sq, count);
    sum_b_sq = lv_expr_sum_n(b_sq, count);
    sum_ab = lv_expr_sum_n(ab, count);
    if (!sum_a_sq || !sum_b_sq || !sum_ab)
        goto cleanup;

    /* left = (∑a_i²) * (∑b_i²) */
    left = lv_expr_mul(sum_a_sq, sum_b_sq);
    if (!left)
        goto cleanup;

    /* right = (∑a_i·b_i)² */
    right = lv_expr_power(sum_ab, two);
    if (!right)
        goto cleanup;

    *out_ineq = lv_ineq_create(left, INEQ_GREATER_EQUAL, right);
    if (*out_ineq) {
        (*out_ineq)->status = INEQ_STATUS_PROVED;
        (*out_ineq)->label = lv_strdup("Cauchy-Schwarz");
    }

cleanup:
    /* 释放临时数组和中间表达式 */
    lv_free((void **) &a_sq);
    lv_free((void **) &b_sq);
    lv_free((void **) &ab);
    lv_expr_free(two);
    if (*out_ineq == NULL) {
        lv_expr_free(right);
        lv_expr_free(left);
        lv_expr_free(sum_ab);
        lv_expr_free(sum_b_sq);
        lv_expr_free(sum_a_sq);
    }

    return (*out_ineq != NULL);
}

/**
 * 排序不等式：
 * 对于递增序列 a1<=...<=an 和 b1<=...<=bn：
 * sum ai*b(n-i+1) <= sum ai*b_sigma(i) <= sum ai*bi
 */
bool lv_ineq_rearrangement(lvExpr **a, lvExpr **b, uint32_t count, lvExpr **out_min, lvExpr **out_max) {
    if (!a || !b || count == 0)
        return false;

    for (uint32_t i = 0; i < count; i++) {
        if (!a[i] || !b[i])
            return false;
    }

    /* 反序乘积和为最小，同序乘积和为最大
     *
     * max = a₁b₁ + a₂b₂ + ... + a_n b_n   (同序和)
     * min = a₁b_n + a₂b_{n-1} + ... + a_n b₁ (反序和)
     */

    if (!out_max && !out_min)
        return true;

    /* 构造同序和 (max) */
    if (out_max) {
        lvExpr **same_prods = (lvExpr **) lv_malloc((size_t) count * sizeof(lvExpr *));
        if (!same_prods)
            return false;
        for (uint32_t i = 0; i < count; i++) {
            same_prods[i] = lv_expr_mul(a[i], b[i]);
            if (!same_prods[i]) {
                lv_free((void **) &same_prods);
                return false;
            }
        }
        *out_max = lv_expr_sum_n(same_prods, count);
        lv_free((void **) &same_prods);
        if (!*out_max)
            return false;
    }

    /* 构造反序和 (min) */
    if (out_min) {
        lvExpr **rev_prods = (lvExpr **) lv_malloc((size_t) count * sizeof(lvExpr *));
        if (!rev_prods)
            return false;
        for (uint32_t i = 0; i < count; i++) {
            /* a[i] * b[count - 1 - i] */
            rev_prods[i] = lv_expr_mul(a[i], b[count - 1 - i]);
            if (!rev_prods[i]) {
                lv_free((void **) &rev_prods);
                return false;
            }
        }
        *out_min = lv_expr_sum_n(rev_prods, count);
        lv_free((void **) &rev_prods);
        if (!*out_min)
            return false;
    }

    return true;
}

/**
 * Schur 不等式：
 * a^r(a-b)(a-c) + b^r(b-c)(b-a) + c^r(c-a)(c-b) >= 0
 * 其中 a, b, c >= 0, r >= 0
 */
bool lv_ineq_schur(lvExpr *a, lvExpr *b, lvExpr *c, uint32_t r, lvInequality **out_ineq) {
    if (!a || !b || !c)
        return false;

    if (!out_ineq)
        return true;
    *out_ineq = NULL;

    /* Schur 不等式:
     * a^r(a-b)(a-c) + b^r(b-c)(b-a) + c^r(c-a)(c-b) >= 0
     *
     * 构造左端表达式，右端为 0
     */

    lvExpr *minus_one = lv_expr_create_rational(-1, 1);
    lvExpr *r_expr = lv_expr_create_rational((int64_t) r, 1);
    lvExpr *zero = lv_expr_create_rational(0, 1);
    if (!minus_one || !r_expr || !zero) {
        lv_expr_free(minus_one);
        lv_expr_free(r_expr);
        lv_expr_free(zero);
        return false;
    }

    /* 构造差值: a-b, a-c, b-c, b-a, c-a, c-b */
    lvExpr *a_minus_b = NULL;
    lvExpr *a_minus_c = NULL;
    lvExpr *b_minus_c = NULL;
    lvExpr *b_minus_a = NULL;
    lvExpr *c_minus_a = NULL;
    lvExpr *c_minus_b = NULL;
    lvExpr *a_pow_r = NULL;
    lvExpr *b_pow_r = NULL;
    lvExpr *c_pow_r = NULL;
    lvExpr *term1 = NULL;
    lvExpr *term2 = NULL;
    lvExpr *term3 = NULL;
    lvExpr *left = NULL;
    bool ok = false;

    a_minus_b = lv_expr_add(a, lv_expr_mul(b, minus_one));
    a_minus_c = lv_expr_add(a, lv_expr_mul(c, minus_one));
    b_minus_c = lv_expr_add(b, lv_expr_mul(c, minus_one));
    b_minus_a = lv_expr_add(b, lv_expr_mul(a, minus_one));
    c_minus_a = lv_expr_add(c, lv_expr_mul(a, minus_one));
    c_minus_b = lv_expr_add(c, lv_expr_mul(b, minus_one));
    if (!a_minus_b || !a_minus_c || !b_minus_c || !b_minus_a || !c_minus_a || !c_minus_b)
        goto cleanup;

    /* 构造三项: term1 = a^r * (a-b) * (a-c) */
    a_pow_r = lv_expr_power(a, r_expr);
    if (!a_pow_r)
        goto cleanup;
    term1 = lv_expr_mul(lv_expr_mul(a_pow_r, a_minus_b), a_minus_c);
    if (!term1)
        goto cleanup;

    /* term2 = b^r * (b-c) * (b-a) */
    b_pow_r = lv_expr_power(b, r_expr);
    if (!b_pow_r)
        goto cleanup;
    term2 = lv_expr_mul(lv_expr_mul(b_pow_r, b_minus_c), b_minus_a);
    if (!term2)
        goto cleanup;

    /* term3 = c^r * (c-a) * (c-b) */
    c_pow_r = lv_expr_power(c, r_expr);
    if (!c_pow_r)
        goto cleanup;
    term3 = lv_expr_mul(lv_expr_mul(c_pow_r, c_minus_a), c_minus_b);
    if (!term3)
        goto cleanup;

    /* 左端 = term1 + term2 + term3 */
    lvExpr *terms_arr[3];
    terms_arr[0] = term1;
    terms_arr[1] = term2;
    terms_arr[2] = term3;
    left = lv_expr_sum_n(terms_arr, 3);
    if (!left)
        goto cleanup;

    *out_ineq = lv_ineq_create(left, INEQ_GREATER_EQUAL, zero);
    if (*out_ineq) {
        (*out_ineq)->status = INEQ_STATUS_PROVED;
        (*out_ineq)->label = lv_strdup("Schur");
    }

    ok = true;

cleanup:
    if (!ok) {
        lv_expr_free(left);
        lv_expr_free(term3);
        lv_expr_free(term2);
        lv_expr_free(term1);
        lv_expr_free(c_pow_r);
        lv_expr_free(b_pow_r);
        lv_expr_free(a_pow_r);
        lv_expr_free(c_minus_b);
        lv_expr_free(c_minus_a);
        lv_expr_free(b_minus_a);
        lv_expr_free(b_minus_c);
        lv_expr_free(a_minus_c);
        lv_expr_free(a_minus_b);
        lv_expr_free(zero);
        lv_expr_free(r_expr);
        lv_expr_free(minus_one);
    }
    return ok;
}

/**
 * Jensen 不等式：
 * 凸函数: f(sum wi*xi) <= sum wi*f(xi)
 * 凹函数: f(sum wi*xi) >= sum wi*f(xi)
 */
bool lv_ineq_jensen(const char *func, lvExpr **points, mpq_t *weights, uint32_t count, bool is_convex,
                    lvInequality **out_ineq) {
    if (!func || !points || count == 0)
        return false;

    for (uint32_t i = 0; i < count; i++) {
        if (!points[i])
            return false;
    }

    /* 验证权重和为 1 */
    if (weights) {
        mpq_t sum;
        mpq_init(sum);
        mpq_set_ui(sum, 0, 1);
        for (uint32_t i = 0; i < count; i++) {
            mpq_add(sum, sum, weights[i]);
        }
        int cmp = mpq_cmp_ui(sum, 1, 1);
        mpq_clear(sum);
        if (cmp != 0)
            return false;
    }

    if (!out_ineq)
        return true;
    *out_ineq = NULL;

    /* Jensen 不等式:
     * 凸函数: f(∑ w_i x_i) ≤ ∑ w_i f(x_i)
     * 凹函数: f(∑ w_i x_i) ≥ ∑ w_i f(x_i)
     *
     * 构造:
     *   left  = f(∑ w_i * x_i)
     *   right = ∑ w_i * f(x_i)
     */

    lvExpr *weighted_sum = NULL;  /* ∑ w_i * x_i */
    lvExpr *weighted_func = NULL; /* ∑ w_i * f(x_i) */

    if (weights) {
        /* 有自定义权重 */
        lvExpr **w_x = (lvExpr **) lv_malloc((size_t) count * sizeof(lvExpr *));
        lvExpr **w_fx = (lvExpr **) lv_malloc((size_t) count * sizeof(lvExpr *));
        if (!w_x || !w_fx) {
            lv_free((void **) &w_x);
            lv_free((void **) &w_fx);
            return false;
        }

        for (uint32_t i = 0; i < count; i++) {
            lvExpr *w_expr = lv_expr_create_rational_mpq(weights[i]);
            if (!w_expr)
                goto cleanup_jensen;
            w_x[i] = lv_expr_mul(w_expr, points[i]);
            if (!w_x[i]) {
                lv_expr_free(w_expr);
                goto cleanup_jensen;
            }

            /* f(x_i) */
            lvExpr *fx = lv_expr_function(func, points[i]);
            if (!fx) {
                lv_expr_free(w_expr);
                goto cleanup_jensen;
            }
            /* 需要另一个权重表达式副本 */
            lvExpr *w_expr2 = lv_expr_create_rational_mpq(weights[i]);
            if (!w_expr2) {
                lv_expr_free(w_expr);
                lv_expr_free(fx);
                goto cleanup_jensen;
            }
            w_fx[i] = lv_expr_mul(w_expr2, fx);
            if (!w_fx[i]) {
                lv_expr_free(w_expr);
                lv_expr_free(w_expr2);
                lv_expr_free(fx);
                goto cleanup_jensen;
            }
        }

        weighted_sum = lv_expr_sum_n(w_x, count);
        weighted_func = lv_expr_sum_n(w_fx, count);

    cleanup_jensen:
        lv_free((void **) &w_x);
        lv_free((void **) &w_fx);
        if (!weighted_sum || !weighted_func)
            return false;
    } else {
        /* 等权重: w_i = 1/n */
        lvExpr *inv_n = NULL;
        lvExpr *sum = NULL;
        lvExpr **fx_arr = NULL;
        lvExpr *fx_sum = NULL;
        lvExpr *inv_n2 = NULL;

        inv_n = lv_expr_create_rational(1, count);
        if (!inv_n)
            return false;

        /* 等权和 = (x_1 + ... + x_n) / n */
        sum = lv_expr_sum_n(points, count);
        if (!sum) {
            lv_expr_free(inv_n);
            return false;
        }
        weighted_sum = lv_expr_mul(sum, inv_n);
        if (!weighted_sum) {
            lv_expr_free(sum);
            lv_expr_free(inv_n);
            return false;
        }

        /* 等权函数和 = (f(x_1) + ... + f(x_n)) / n */
        fx_arr = (lvExpr **) lv_malloc((size_t) count * sizeof(lvExpr *));
        if (!fx_arr) {
            lv_expr_free(weighted_sum);
            lv_expr_free(sum);
            lv_expr_free(inv_n);
            return false;
        }
        for (uint32_t i = 0; i < count; i++) {
            fx_arr[i] = lv_expr_function(func, points[i]);
            if (!fx_arr[i]) {
                /* 释放已创建的 fx_arr 元素 */
                for (uint32_t j = 0; j < i; j++)
                    lv_expr_free(fx_arr[j]);
                lv_free((void **) &fx_arr);
                lv_expr_free(weighted_sum);
                lv_expr_free(sum);
                lv_expr_free(inv_n);
                return false;
            }
        }
        fx_sum = lv_expr_sum_n(fx_arr, count);
        lv_free((void **) &fx_arr);
        if (!fx_sum) {
            lv_expr_free(weighted_sum);
            lv_expr_free(sum);
            lv_expr_free(inv_n);
            return false;
        }

        inv_n2 = lv_expr_create_rational(1, count);
        if (!inv_n2) {
            lv_expr_free(fx_sum);
            lv_expr_free(weighted_sum);
            lv_expr_free(sum);
            lv_expr_free(inv_n);
            return false;
        }
        weighted_func = lv_expr_mul(fx_sum, inv_n2);
        if (!weighted_func) {
            lv_expr_free(inv_n2);
            lv_expr_free(fx_sum);
            lv_expr_free(weighted_sum);
            lv_expr_free(sum);
            lv_expr_free(inv_n);
            return false;
        }
    }

    /* 左端: f(weighted_sum) */
    lvExpr *left = lv_expr_function(func, weighted_sum);
    if (!left)
        return false;

    /* 右端: weighted_func 就是 ∑ w_i f(x_i) */
    lvExpr *right = weighted_func;

    /* 根据凹凸性决定不等式方向
     * 凸函数: left <= right → f(∑w_i x_i) ≤ ∑w_i f(x_i)
     * 凹函数: left >= right → f(∑w_i x_i) ≥ ∑w_i f(x_i)
     */
    lvInequalityType itype = is_convex ? INEQ_LESS_EQUAL : INEQ_GREATER_EQUAL;

    *out_ineq = lv_ineq_create(left, itype, right);
    if (*out_ineq) {
        (*out_ineq)->status = INEQ_STATUS_PROVED;
        (*out_ineq)->label = lv_strdup("Jensen");
    }

    return (*out_ineq != NULL);
}

/**
 * 三角形不等式：
 * |a - b| < c < a + b
 * 产生三条不等式
 */
uint32_t lv_ineq_triangle(lvExpr *a, lvExpr *b, lvExpr *c, lvInequality **out_inequalities, uint32_t max_count) {
    if (!a || !b || !c || !out_inequalities || max_count < 3)
        return 0;

    /* 三角不等式产生三条不等式：
     * 1. a + b > c
     * 2. a + c > b
     * 3. b + c > a
     */
    lvExpr *a_plus_b = lv_expr_add(a, b);
    lvExpr *a_plus_c = lv_expr_add(a, c);
    lvExpr *b_plus_c = lv_expr_add(b, c);
    if (!a_plus_b || !a_plus_c || !b_plus_c)
        return 0;

    out_inequalities[0] = lv_ineq_create(a_plus_b, INEQ_GREATER_THAN, c);
    if (!out_inequalities[0]) {
        lv_expr_free(a_plus_c);
        lv_expr_free(b_plus_c);
        return 0;
    }

    out_inequalities[1] = lv_ineq_create(a_plus_c, INEQ_GREATER_THAN, b);
    if (!out_inequalities[1]) {
        lv_ineq_destroy(out_inequalities[0]);
        out_inequalities[0] = NULL;
        lv_expr_free(b_plus_c);
        return 0;
    }

    out_inequalities[2] = lv_ineq_create(b_plus_c, INEQ_GREATER_THAN, a);
    if (!out_inequalities[2]) {
        lv_ineq_destroy(out_inequalities[1]);
        out_inequalities[1] = NULL;
        lv_ineq_destroy(out_inequalities[0]);
        out_inequalities[0] = NULL;
        return 0;
    }

    return 3;
}

/* ============== 不等式变换 ============== */

/**
 * 不等式两边加表达式：
 * (left <type> right) + expr => (left+expr <type> right+expr)
 * 不等式方向不变
 */

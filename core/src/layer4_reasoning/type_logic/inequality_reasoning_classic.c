/**
 * @file inequality_reasoning_classic.c
 * @brief 不等式推理系统 —— 经典不等式（AM-GM/Cauchy/排序/Schur/Jensen/三角形）
 */

#include "inequality_reasoning_internal.h"

#include "lv/lv_lifecycle.h"


/* 统一的临时表达式释放辅助：逆序释放表达式数组中的全部元素（lv_expr_free 容忍 NULL） */
static void ineq_free_exprs(lvExpr **arr, int count) {
    for (int i = count - 1; i >= 0; i--)
        lv_expr_free(arr[i]);
}

/* 单个 lvExpr* 变量的 defer 清理回调（置 NULL 即解除守卫） */
static void ineq_defer_expr_free(void *p) {
    lvExpr **pp = (lvExpr **) p;
    if (*pp)
        lv_expr_free(*pp);
}

/* 固定尺寸临时表达式数组守卫（arr + 元素个数；arr 置 NULL 即解除守卫） */
typedef struct {
    lvExpr **arr;
    int count;
} IneqExprArrayGuard;

static void ineq_expr_array_guard_cleanup(void *p) {
    IneqExprArrayGuard *g = (IneqExprArrayGuard *) p;
    if (g->arr)
        ineq_free_exprs(g->arr, g->count);
}

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

    /* 算术平均: (e1 + ... + en) / n
     * 临时表达式数组: [0]=sum [1]=inv_n [2]=am [3]=prod [4]=inv_n_exp [5]=gm */
    lvExpr *tmp[6] = {0};

    /* 注册作用域守卫：任一失败分支直接 return false，临时表达式在函数出口自动释放；
     * 成功时结果表达式接管 tmp，守卫解除 */
    IneqExprArrayGuard tmp_guard = {tmp, 6};
    lv_DEFER(ineq_expr_array_guard_cleanup, &tmp_guard);

    tmp[0] = lv_expr_sum_n(expressions, count);
    if (!tmp[0])
        return false;

    tmp[1] = lv_expr_create_rational(1, count);
    if (!tmp[1])
        return false;

    /* sum * (1/n) = sum / n */
    tmp[2] = lv_expr_mul(tmp[0], tmp[1]);
    if (!tmp[2])
        return false;

    /* 几何平均: (e1 * ... * en)^(1/n) */
    tmp[3] = lv_expr_product_n(expressions, count);
    if (!tmp[3])
        return false;

    /* inv_n_expr = 1/n as exponent */
    tmp[4] = lv_expr_create_rational(1, count);
    if (!tmp[4])
        return false;

    tmp[5] = lv_expr_power(tmp[3], tmp[4]);
    if (!tmp[5])
        return false;

    if (out_lower_bound)
        *out_lower_bound = tmp[5];
    if (out_upper_bound)
        *out_upper_bound = tmp[2];

    tmp_guard.arr = NULL; /* 结果移交调用方，守卫不再释放 */
    return true;
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
    lvExpr *two = NULL;
    lvExpr **a_sq = NULL;
    lvExpr **b_sq = NULL;
    lvExpr **ab = NULL;
    /* 中间求和与左右端表达式 */
    lvExpr *sum_a_sq = NULL;
    lvExpr *sum_b_sq = NULL;
    lvExpr *sum_ab = NULL;
    lvExpr *left = NULL;
    lvExpr *right = NULL;

    /* 注册作用域守卫：任一失败分支直接 return false，临时资源在函数出口自动释放；
     * 成功时 left/right/sum 表达式由不等式接管（置 NULL 解除守卫），two 与数组照常释放 */
    lv_DEFER(ineq_defer_expr_free, &two);
    lv_DEFER(lv_defer_free_ptr, &a_sq);
    lv_DEFER(lv_defer_free_ptr, &b_sq);
    lv_DEFER(lv_defer_free_ptr, &ab);
    lv_DEFER(ineq_defer_expr_free, &sum_a_sq);
    lv_DEFER(ineq_defer_expr_free, &sum_b_sq);
    lv_DEFER(ineq_defer_expr_free, &sum_ab);
    lv_DEFER(ineq_defer_expr_free, &left);
    lv_DEFER(ineq_defer_expr_free, &right);

    two = lv_expr_create_rational(2, 1);
    if (!two)
        return false;

    a_sq = (lvExpr **) lv_malloc((size_t) count * sizeof(lvExpr *));
    if (!a_sq)
        return false;
    for (uint32_t i = 0; i < count; i++) {
        a_sq[i] = lv_expr_power(a[i], two);
        if (!a_sq[i])
            return false;
    }

    /* 构造 b_i² 数组: b_sq[i] = b[i]^2 */
    b_sq = (lvExpr **) lv_malloc((size_t) count * sizeof(lvExpr *));
    if (!b_sq)
        return false;
    for (uint32_t i = 0; i < count; i++) {
        b_sq[i] = lv_expr_power(b[i], two);
        if (!b_sq[i])
            return false;
    }

    /* 构造 a_i·b_i 数组 */
    ab = (lvExpr **) lv_malloc((size_t) count * sizeof(lvExpr *));
    if (!ab)
        return false;
    for (uint32_t i = 0; i < count; i++) {
        ab[i] = lv_expr_mul(a[i], b[i]);
        if (!ab[i])
            return false;
    }

    /* sum_a_sq = ∑a_i², sum_b_sq = ∑b_i², sum_ab = ∑a_i·b_i */
    sum_a_sq = lv_expr_sum_n(a_sq, count);
    sum_b_sq = lv_expr_sum_n(b_sq, count);
    sum_ab = lv_expr_sum_n(ab, count);
    if (!sum_a_sq || !sum_b_sq || !sum_ab)
        return false;

    /* left = (∑a_i²) * (∑b_i²) */
    left = lv_expr_mul(sum_a_sq, sum_b_sq);
    if (!left)
        return false;

    /* right = (∑a_i·b_i)² */
    right = lv_expr_power(sum_ab, two);
    if (!right)
        return false;

    *out_ineq = lv_ineq_create(left, INEQ_GREATER_EQUAL, right);
    if (*out_ineq) {
        (*out_ineq)->status = INEQ_STATUS_PROVED;
        (*out_ineq)->label = lv_strdup("Cauchy-Schwarz");
        /* 结果移交不等式，中间表达式归其所有，守卫解除 */
        sum_a_sq = sum_b_sq = sum_ab = left = right = NULL;
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

    /* 临时表达式数组:
     * [0]=minus_one [1]=r_expr [2]=zero [3..8]=a-b/a-c/b-c/b-a/c-a/c-b
     * [9..11]=a^r/b^r/c^r [12..14]=term1..term3 [15]=left */
    lvExpr *tmp[16] = {0};

    /* 注册作用域守卫：任一失败分支直接 return false，临时表达式在函数出口自动释放；
     * 成功时结果表达式接管 tmp，守卫解除 */
    IneqExprArrayGuard tmp_guard = {tmp, 16};
    lv_DEFER(ineq_expr_array_guard_cleanup, &tmp_guard);

    tmp[0] = lv_expr_create_rational(-1, 1);
    tmp[1] = lv_expr_create_rational((int64_t) r, 1);
    tmp[2] = lv_expr_create_rational(0, 1);
    if (!tmp[0] || !tmp[1] || !tmp[2])
        return false;

    /* 构造差值: a-b, a-c, b-c, b-a, c-a, c-b */
    tmp[3] = lv_expr_add(a, lv_expr_mul(b, tmp[0]));
    tmp[4] = lv_expr_add(a, lv_expr_mul(c, tmp[0]));
    tmp[5] = lv_expr_add(b, lv_expr_mul(c, tmp[0]));
    tmp[6] = lv_expr_add(b, lv_expr_mul(a, tmp[0]));
    tmp[7] = lv_expr_add(c, lv_expr_mul(a, tmp[0]));
    tmp[8] = lv_expr_add(c, lv_expr_mul(b, tmp[0]));
    if (!tmp[3] || !tmp[4] || !tmp[5] || !tmp[6] || !tmp[7] || !tmp[8])
        return false;

    /* 构造三项: term1 = a^r * (a-b) * (a-c) */
    tmp[9] = lv_expr_power(a, tmp[1]);
    if (!tmp[9])
        return false;
    tmp[12] = lv_expr_mul(lv_expr_mul(tmp[9], tmp[3]), tmp[4]);
    if (!tmp[12])
        return false;

    /* term2 = b^r * (b-c) * (b-a) */
    tmp[10] = lv_expr_power(b, tmp[1]);
    if (!tmp[10])
        return false;
    tmp[13] = lv_expr_mul(lv_expr_mul(tmp[10], tmp[5]), tmp[6]);
    if (!tmp[13])
        return false;

    /* term3 = c^r * (c-a) * (c-b) */
    tmp[11] = lv_expr_power(c, tmp[1]);
    if (!tmp[11])
        return false;
    tmp[14] = lv_expr_mul(lv_expr_mul(tmp[11], tmp[7]), tmp[8]);
    if (!tmp[14])
        return false;

    /* 左端 = term1 + term2 + term3 */
    lvExpr *terms_arr[3];
    terms_arr[0] = tmp[12];
    terms_arr[1] = tmp[13];
    terms_arr[2] = tmp[14];
    tmp[15] = lv_expr_sum_n(terms_arr, 3);
    if (!tmp[15])
        return false;

    *out_ineq = lv_ineq_create(tmp[15], INEQ_GREATER_EQUAL, tmp[2]);
    if (*out_ineq) {
        (*out_ineq)->status = INEQ_STATUS_PROVED;
        (*out_ineq)->label = lv_strdup("Schur");
    }

    tmp_guard.arr = NULL; /* 结果表达式接管 tmp（与原先 ok=true 语义一致），守卫解除 */
    return true;
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

    /* 临时表达式数组（等权重分支）:
     * [0]=inv_n [1]=sum [2]=weighted_sum [3]=fx_sum [4]=inv_n2 [5]=weighted_func */
    lvExpr *tmp[6] = {0};
    lvExpr **w_x = NULL;
    lvExpr **w_fx = NULL;
    lvExpr **fx_arr = NULL;
    lvExpr *left = NULL; /* f(weighted_sum) */

    /* 注册作用域守卫：任一失败分支直接 return false，临时资源在函数出口自动释放；
     * 成功时 left / tmp 表达式由不等式接管（置 NULL 解除守卫），w_x/w_fx/fx_arr 数组照常释放 */
    IneqExprArrayGuard tmp_guard = {tmp, 6};
    lv_DEFER(ineq_expr_array_guard_cleanup, &tmp_guard);
    lv_DEFER(lv_defer_free_ptr, &w_x);
    lv_DEFER(lv_defer_free_ptr, &w_fx);
    lv_DEFER(lv_defer_free_ptr, &fx_arr);
    lv_DEFER(ineq_defer_expr_free, &left);

    if (weights) {
        /* 有自定义权重 */
        w_x = (lvExpr **) lv_malloc((size_t) count * sizeof(lvExpr *));
        w_fx = (lvExpr **) lv_malloc((size_t) count * sizeof(lvExpr *));
        if (!w_x || !w_fx)
            return false;

        for (uint32_t i = 0; i < count; i++) {
            lvExpr *w_expr = lv_expr_create_rational_mpq(weights[i]);
            if (!w_expr)
                return false;
            w_x[i] = lv_expr_mul(w_expr, points[i]);
            if (!w_x[i]) {
                lv_expr_free(w_expr);
                return false;
            }

            /* f(x_i) */
            lvExpr *fx = lv_expr_function(func, points[i]);
            if (!fx) {
                lv_expr_free(w_expr);
                return false;
            }
            /* 需要另一个权重表达式副本 */
            lvExpr *w_expr2 = lv_expr_create_rational_mpq(weights[i]);
            if (!w_expr2) {
                lv_expr_free(w_expr);
                lv_expr_free(fx);
                return false;
            }
            w_fx[i] = lv_expr_mul(w_expr2, fx);
            if (!w_fx[i]) {
                lv_expr_free(w_expr);
                lv_expr_free(w_expr2);
                lv_expr_free(fx);
                return false;
            }
        }

        tmp[2] = lv_expr_sum_n(w_x, count);
        tmp[5] = lv_expr_sum_n(w_fx, count);
        if (!tmp[2] || !tmp[5])
            return false;
    } else {
        /* 等权重: w_i = 1/n */
        tmp[0] = lv_expr_create_rational(1, count);
        if (!tmp[0])
            return false;

        /* 等权和 = (x_1 + ... + x_n) / n */
        tmp[1] = lv_expr_sum_n(points, count);
        if (!tmp[1])
            return false;
        tmp[2] = lv_expr_mul(tmp[1], tmp[0]);
        if (!tmp[2])
            return false;

        /* 等权函数和 = (f(x_1) + ... + f(x_n)) / n */
        fx_arr = (lvExpr **) lv_malloc((size_t) count * sizeof(lvExpr *));
        if (!fx_arr)
            return false;
        for (uint32_t i = 0; i < count; i++) {
            fx_arr[i] = lv_expr_function(func, points[i]);
            if (!fx_arr[i]) {
                /* 释放已创建的 fx_arr 元素（数组由守卫释放） */
                for (uint32_t j = 0; j < i; j++)
                    lv_expr_free(fx_arr[j]);
                return false;
            }
        }
        tmp[3] = lv_expr_sum_n(fx_arr, count);
        if (!tmp[3])
            return false;

        tmp[4] = lv_expr_create_rational(1, count);
        if (!tmp[4])
            return false;
        tmp[5] = lv_expr_mul(tmp[3], tmp[4]);
        if (!tmp[5])
            return false;
    }

    /* 左端: f(weighted_sum) */
    left = lv_expr_function(func, tmp[2]);
    if (!left)
        return false;

    /* 右端: weighted_func 就是 ∑ w_i f(x_i) */
    lvExpr *right = tmp[5];

    /* 根据凹凸性决定不等式方向
     * 凸函数: left <= right → f(∑w_i x_i) ≤ ∑w_i f(x_i)
     * 凹函数: left >= right → f(∑w_i x_i) ≥ ∑w_i f(x_i)
     */
    lvInequalityType itype = is_convex ? INEQ_LESS_EQUAL : INEQ_GREATER_EQUAL;

    *out_ineq = lv_ineq_create(left, itype, right);
    if (*out_ineq) {
        (*out_ineq)->status = INEQ_STATUS_PROVED;
        (*out_ineq)->label = lv_strdup("Jensen");
        /* 结果移交不等式（left / tmp 表达式归其所有），守卫解除 */
        left = NULL;
        tmp_guard.arr = NULL;
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

/**
 * @file solver_groebner.c
 * @brief Groebner 基计算
 *
 * @details 从 solver.c 拆分出的子模块（Lv-00 项目 v3.3.0+）。
 *          包含多变量多项式表示、Buchberger 算法、Groebner 基计算。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "solver_common.h"
#include "lv/solver_dirty_set.h"

/* ── 多变量多项式 ── */
typedef struct {
    int *exponents;
    mpz_t coeff;
} MVMonomial;

typedef struct {
    lvDArray terms; /**< MVMonomial 数组 */
    int var_count;
} MVPolynomial;

/* 流式上下文（定义在 solver_engine.c，通过 solver_types.h 的 extern 引用） */

/* ================================================================== */
/*  内部: 多变量单项式表示 (用于 Groebner 基计算)                      */
/* ================================================================== */

/* 初始化多变量多项式 */
static void mv_poly_init(MVPolynomial *p, int var_count) {
    lv_darray_init(&p->terms, sizeof(MVMonomial));
    p->var_count = var_count;
}

/* 清理多变量多项式 */
static void mv_poly_clear(MVPolynomial *p) {
    for (int i = 0; i < p->terms.count; i++) {
        MVMonomial *m = (MVMonomial *)lv_darray_get(&p->terms, i);
        if (m) {
            lv_free((void **) &m->exponents);
            mpz_clear(m->coeff);
        }
    }
    lv_darray_free(&p->terms);
}

/* 添加一个单项式到多变量多项式 (合并同类项) */
static int mv_poly_add_term(MVPolynomial *p, const mpz_t coeff, const int *exponents) {
    if (mpz_cmp_si(coeff, 0) == 0)
        return 0;

    for (int i = 0; i < p->terms.count; i++) {
        MVMonomial *mt = (MVMonomial *)lv_darray_get(&p->terms, i);
        if (!mt) continue;
        bool same = true;
        for (int v = 0; v < p->var_count; v++) {
            if (mt->exponents[v] != exponents[v]) {
                same = false;
                break;
            }
        }
        if (same) {
            mpz_add(mt->coeff, mt->coeff, coeff);
            if (mpz_cmp_si(mt->coeff, 0) == 0) {
                mpz_clear(mt->coeff);
                int last = p->terms.count - 1;
                if (i < last) {
                    MVMonomial *last_m = (MVMonomial *)lv_darray_get(&p->terms, last);
                    if (last_m) {
                        lv_free((void **) &mt->exponents);
                        mt->exponents = last_m->exponents;
                        mpz_set(mt->coeff, last_m->coeff);
                        mpz_clear(last_m->coeff);
                        last_m->exponents = NULL;
                    }
                } else {
                    lv_free((void **) &mt->exponents);
                }
                p->terms.count--;
            }
            return 0;
        }
    }

    if (!lv_darray_reserve(&p->terms, p->terms.count + 1)) {
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "mv_poly_add_term: 扩容失败");
    }
    MVMonomial *m = (MVMonomial *)((char *)p->terms.data + (size_t)p->terms.count * sizeof(MVMonomial));
    m->exponents = lv_calloc((size_t) p->var_count, sizeof(int));
    if (!m->exponents) {
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "mv_poly_add_term: 指数数组分配失败");
    }
    for (int v = 0; v < p->var_count; v++) {
        m->exponents[v] = exponents[v];
    }
    mpz_init_set(m->coeff, coeff);
    p->terms.count++;
    return 0;
}

/* 计算单项式的全次数 */
static int mv_monomial_total_degree(const MVMonomial *m, int var_count) {
    int deg = 0;
    for (int v = 0; v < var_count; v++) {
        deg += m->exponents[v];
    }
    return deg;
}

/* 比较两个单项式的次数 (grlex序) */
static int mv_monomial_compare_grlex(const MVMonomial *a, const MVMonomial *b, int var_count) {
    int deg_a = mv_monomial_total_degree(a, var_count);
    int deg_b = mv_monomial_total_degree(b, var_count);
    if (deg_a != deg_b)
        return deg_b - deg_a;
    for (int v = 0; v < var_count; v++) {
        if (a->exponents[v] != b->exponents[v]) {
            return b->exponents[v] - a->exponents[v];
        }
    }
    return 0;
}

/* 对多项式的单项式按 grlex 序排序 */
static void mv_poly_sort(MVPolynomial *p) {
    for (int i = 1; i < p->terms.count; i++) {
        MVMonomial *key_p = (MVMonomial *)lv_darray_get(&p->terms, i);
        MVMonomial key;
        if (key_p) {
            key = *key_p;
        } else {
            break;
        }
        int j = i - 1;
        while (j >= 0) {
            MVMonomial *tj = (MVMonomial *)lv_darray_get(&p->terms, j);
            if (!tj || mv_monomial_compare_grlex(tj, &key, p->var_count) <= 0)
                break;
            MVMonomial *tj1 = (MVMonomial *)lv_darray_get(&p->terms, j + 1);
            if (tj1) *tj1 = *tj;
            j--;
        }
        MVMonomial *tj1 = (MVMonomial *)lv_darray_get(&p->terms, j + 1);
        if (tj1) *tj1 = key;
    }
}

/* 移除系数为零的单项式 */
static void mv_poly_remove_zeros(MVPolynomial *p) {
    int write = 0;
    for (int i = 0; i < p->terms.count; i++) {
        MVMonomial *mi = (MVMonomial *)lv_darray_get(&p->terms, i);
        if (!mi) continue;
        if (mpz_cmp_si(mi->coeff, 0) != 0) {
            if (write != i) {
                MVMonomial *mw = (MVMonomial *)lv_darray_get(&p->terms, write);
                if (mw) {
                    lv_free((void **) &mw->exponents);
                    mpz_clear(mw->coeff);
                    mw->exponents = mi->exponents;
                    mpz_set(mw->coeff, mi->coeff);
                    mpz_clear(mi->coeff);
                    mi->exponents = NULL;
                }
            }
            write++;
        } else {
            lv_free((void **) &mi->exponents);
            mpz_clear(mi->coeff);
        }
    }
    p->terms.count = write;
}

/* 获取多项式的首项 (leading term, grlex序下最大的单项式) */
static const MVMonomial *mv_poly_leading_term(const MVPolynomial *p) {
    if (p->terms.count == 0)
        return NULL;
    return (const MVMonomial *)lv_darray_get(&p->terms, 0);
}

/* 获取首项的首单项式指数 (LCM of leading monomials) */
static void mv_monomial_lcm(const MVMonomial *a, const MVMonomial *b, int var_count, int *out_lcm) {
    for (int v = 0; v < var_count; v++) {
        out_lcm[v] = (a->exponents[v] > b->exponents[v]) ? a->exponents[v] : b->exponents[v];
    }
}

/* 检查单项式 m 是否能被单项式 d 整除 */
static bool mv_monomial_divisible(const MVMonomial *m, const MVMonomial *d, int var_count) {
    for (int v = 0; v < var_count; v++) {
        if (m->exponents[v] < d->exponents[v])
            return false;
    }
    return true;
}

/* 检查单项式 d 能否整除 LCM 单项式 */
static bool mv_monomial_divisible_lcm(const MVMonomial *d, const int *lcm_exp, int var_count) {
    for (int v = 0; v < var_count; v++) {
        if (d->exponents[v] > lcm_exp[v])
            return false;
    }
    return true;
}

/* 多变量多项式乘以单项式 */
static void mv_poly_mul_monomial(MVPolynomial *result, const MVPolynomial *p, const int *mono_exp,
                                 const mpz_t mono_coeff, int var_count) {
    mv_poly_clear(result);
    mv_poly_init(result, var_count);
    for (int i = 0; i < p->terms.count; i++) {
        const MVMonomial *mt = (const MVMonomial *)lv_darray_get(&p->terms, i);
        if (!mt) continue;
        mpz_t new_coeff;
        mpz_init(new_coeff);
        mpz_mul(new_coeff, mt->coeff, mono_coeff);

        int *new_exp = lv_calloc((size_t) var_count, sizeof(int));
        if (!new_exp) {
            mpz_clear(new_coeff);
            continue;
        }
        for (int v = 0; v < var_count; v++) {
            new_exp[v] = mt->exponents[v] + mono_exp[v];
        }
        mv_poly_add_term(result, new_coeff, new_exp);
        mpz_clear(new_coeff);
        lv_free((void **) &new_exp);
    }
    mv_poly_sort(result);
}

/* 多变量多项式减法: result = a - b */
static void mv_poly_sub(MVPolynomial *result, const MVPolynomial *a, const MVPolynomial *b) {
    mv_poly_clear(result);
    mv_poly_init(result, a->var_count);
    for (int i = 0; i < a->terms.count; i++) {
        const MVMonomial *ma = (const MVMonomial *)lv_darray_get(&a->terms, i);
        if (ma) mv_poly_add_term(result, ma->coeff, ma->exponents);
    }
    for (int i = 0; i < b->terms.count; i++) {
        const MVMonomial *mb = (const MVMonomial *)lv_darray_get(&b->terms, i);
        if (!mb) continue;
        mpz_t neg_coeff;
        mpz_init(neg_coeff);
        mpz_neg(neg_coeff, mb->coeff);
        mv_poly_add_term(result, neg_coeff, mb->exponents);
        mpz_clear(neg_coeff);
    }
    mv_poly_sort(result);
    mv_poly_remove_zeros(result);
}

/* 多变量多项式复制 */
static void mv_poly_copy(MVPolynomial *dst, const MVPolynomial *src) {
    mv_poly_clear(dst);
    mv_poly_init(dst, src->var_count);
    for (int i = 0; i < src->terms.count; i++) {
        const MVMonomial *ms = (const MVMonomial *)lv_darray_get(&src->terms, i);
        if (ms) mv_poly_add_term(dst, ms->coeff, ms->exponents);
    }
}

/* 检查多变量多项式是否为零 */
static bool mv_poly_is_zero(const MVPolynomial *p) {
    return p->terms.count == 0;
}

/* ================================================================== */
/*  内部: S-多项式计算 (Groebner 基核心)                                */
/* ================================================================== */

static void s_polynomial(const MVPolynomial *f, const MVPolynomial *g, int var_count, MVPolynomial *result) {
    mv_poly_init(result, var_count);

    const MVMonomial *lt_f = mv_poly_leading_term(f);
    const MVMonomial *lt_g = mv_poly_leading_term(g);
    if (!lt_f || !lt_g)
        return;

    int *lcm_exp = lv_calloc((size_t) var_count, sizeof(int));
    if (!lcm_exp)
        return;
    int *quo_f_exp = lv_calloc((size_t) var_count, sizeof(int));
    if (!quo_f_exp) {
        lv_free((void **) &lcm_exp);
        return;
    }
    int *quo_g_exp = lv_calloc((size_t) var_count, sizeof(int));
    if (!quo_g_exp) {
        lv_free((void **) &lcm_exp);
        lv_free((void **) &quo_f_exp);
        return;
    }

    mv_monomial_lcm(lt_f, lt_g, var_count, lcm_exp);

    for (int v = 0; v < var_count; v++) {
        quo_f_exp[v] = lcm_exp[v] - lt_f->exponents[v];
    }
    for (int v = 0; v < var_count; v++) {
        quo_g_exp[v] = lcm_exp[v] - lt_g->exponents[v];
    }

    MVPolynomial term1, term2;
    memset(&term1, 0, sizeof(term1));
    memset(&term2, 0, sizeof(term2));
    mv_poly_mul_monomial(&term1, f, quo_f_exp, lt_g->coeff, var_count);
    mv_poly_mul_monomial(&term2, g, quo_g_exp, lt_f->coeff, var_count);
    mv_poly_sub(result, &term1, &term2);

    mv_poly_clear(&term1);
    mv_poly_clear(&term2);
    lv_free((void **) &lcm_exp);
    lv_free((void **) &quo_f_exp);
    lv_free((void **) &quo_g_exp);
}

/* ================================================================== */
/*  内部: 多项式约化 (用多项式集合 G 约化多项式 p)                      */
/* ================================================================== */

static void polynomial_reduce(const MVPolynomial *p, MVPolynomial **G, int g_count, MVPolynomial *remainder) {
    mv_poly_copy(remainder, p);

    bool changed = true;
    int safety = 0;
    const int max_iterations = 10000;

    int stalled_rounds = 0;
    const int max_stalled_rounds = 10;
    long double prev_term_count = (long double) remainder->terms.count;
    int term_oscillation = 0;

    while (changed && safety < max_iterations) {
        changed = false;
        safety++;

        for (int i = 0; i < g_count; i++) {
            if (mv_poly_is_zero(G[i]))
                continue;

            const MVMonomial *lt_g = mv_poly_leading_term(G[i]);
            if (!lt_g)
                continue;

            bool reduced = false;
            for (int j = 0; j < remainder->terms.count && !reduced; j++) {
                MVMonomial *rj = (MVMonomial *)lv_darray_get(&remainder->terms, j);
                if (!rj) continue;
                if (mv_monomial_divisible(rj, lt_g, remainder->var_count)) {
                    int *quo_exp = lv_calloc((size_t) remainder->var_count, sizeof(int));
                    if (!quo_exp) {
                        reduced = true;
                        changed = false;
                        break;
                    }
                    for (int v = 0; v < remainder->var_count; v++) {
                        quo_exp[v] = rj->exponents[v] - lt_g->exponents[v];
                    }

                    MVPolynomial sub_term;
                    mv_poly_init(&sub_term, G[i]->var_count);
                    mv_poly_mul_monomial(&sub_term, G[i], quo_exp, rj->coeff, remainder->var_count);

                    MVPolynomial new_remainder;
                    mv_poly_init(&new_remainder, remainder->var_count);

                    for (int k = 0; k < remainder->terms.count; k++) {
                        MVMonomial *rk = (MVMonomial *)lv_darray_get(&remainder->terms, k);
                        if (!rk) continue;
                        mpz_t scaled;
                        mpz_init(scaled);
                        mpz_mul(scaled, rk->coeff, lt_g->coeff);
                        mv_poly_add_term(&new_remainder, scaled, rk->exponents);
                        mpz_clear(scaled);
                    }
                    for (int k = 0; k < sub_term.terms.count; k++) {
                        MVMonomial *sk = (MVMonomial *)lv_darray_get(&sub_term.terms, k);
                        if (!sk) continue;
                        mpz_t neg;
                        mpz_init(neg);
                        mpz_neg(neg, sk->coeff);
                        mv_poly_add_term(&new_remainder, neg, sk->exponents);
                        mpz_clear(neg);
                    }

                    mv_poly_sort(&new_remainder);
                    mv_poly_remove_zeros(&new_remainder);
                    mv_poly_clear(remainder);
                    *remainder = new_remainder;
                    mv_poly_clear(&sub_term);
                    lv_free((void **) &quo_exp);

                    changed = true;
                    reduced = true;
                }
            }
        }

        if (!changed) {
            stalled_rounds++;
        } else {
            stalled_rounds = 0;
        }

        if (changed) {
            long double curr_term_count = (long double) remainder->terms.count;
            long double diff = curr_term_count - prev_term_count;
            if (fabsl(diff) < 1.0L && diff != 0.0L) {
                term_oscillation++;
            } else {
                term_oscillation = 0;
            }
            prev_term_count = curr_term_count;
        }

        if (stalled_rounds >= max_stalled_rounds) {
            LOG_WARN("solver", "polynomial_reduce: 连续 %d 轮无约化进展，提前终止", stalled_rounds);
            break;
        }
        if (term_oscillation >= 3) {
            LOG_WARN("solver", "polynomial_reduce: 检测到项数微小振荡（精度累积），提前终止");
            break;
        }
    }
}

/* ================================================================== */
/*  内部: Buchberger Groebner 基算法                                    */
/* ================================================================== */

static SolverStatus buchberger_groebner(MVPolynomial **F, int f_count, MVPolynomial ***out_G, int *out_g_count,
                                        int step_limit) {
    if (f_count == 0 || !F || !out_G || !out_g_count) {
        *out_G = NULL;
        *out_g_count = 0;
        return SOLVER_STATUS_OK;
    }

    int var_count = F[0]->var_count;

    for (int i = 0; i < f_count; i++) {
        for (int j = 0; j < F[i]->terms.count; j++) {
            const MVMonomial *mfj = (const MVMonomial *)lv_darray_get(&F[i]->terms, j);
            if (mfj && mv_monomial_total_degree(mfj, var_count) > 4) {
                *out_G = NULL;
                *out_g_count = 0;
                return SOLVER_STATUS_OUT_OF_SCOPE;
            }
        }
    }

    int g_capacity = f_count + 16;
    MVPolynomial **G = lv_calloc((size_t) g_capacity, sizeof(MVPolynomial *));
    if (!G) {
        *out_G = NULL;
        *out_g_count = 0;
        return SOLVER_STATUS_TIMEOUT;
    }
    int g_count = f_count;
    for (int i = 0; i < f_count; i++) {
        G[i] = lv_calloc(1, sizeof(MVPolynomial));
        if (!G[i]) {
            for (int j = 0; j < i; j++) {
                mv_poly_clear(G[j]);
                lv_free((void **) &G[j]);
            }
            lv_free((void **) &G);
            *out_G = NULL;
            *out_g_count = 0;
            return SOLVER_STATUS_TIMEOUT;
        }
        mv_poly_init(G[i], var_count);
        mv_poly_copy(G[i], F[i]);
    }

    if (g_capacity > 0 && g_capacity > INT_MAX / g_capacity) {
        for (int i = 0; i < g_count; i++) {
            mv_poly_clear(G[i]);
            lv_free((void **) &G[i]);
        }
        lv_free((void **) &G);
        *out_G = NULL;
        *out_g_count = 0;
        return SOLVER_STATUS_OUT_OF_MEMORY;
    }
    bool *pair_data = lv_calloc((size_t) (g_capacity * g_capacity), sizeof(bool));
    if (!pair_data) {
        for (int i = 0; i < g_count; i++) {
            mv_poly_clear(G[i]);
            lv_free((void **) &G[i]);
        }
        lv_free((void **) &G);
        *out_G = NULL;
        *out_g_count = 0;
        return SOLVER_STATUS_OUT_OF_MEMORY;
    }

    int steps = 0;
    int gb_stalled_count = 0;
    const int gb_max_stalled = 8;
    int prev_g_count = g_count;
    long double prev_total_terms = 0;
    for (int ti = 0; ti < g_count; ti++) {
        prev_total_terms += (long double) G[ti]->terms.count;
    }

    bool changed = true;
    while (changed && steps < step_limit) {
        changed = false;

        for (int i = 0; i < g_count && steps < step_limit; i++) {
            for (int j = i + 1; j < g_count && steps < step_limit; j++) {
                steps++;

                if (solver_stream_ctx) {
                    if (steps <= 3 || steps % 10 == 0) {
                        StreamEvent ev;
                        memset(&ev, 0, sizeof(ev));
                        ev.type = STREAM_EVENT_SOLVE_GROEBNER_STEP;
                        ev.timestamp_ms = stream_timestamp_ms();
                        ev.step_number = steps;
                        long long total_pairs = (long long) g_count * (g_count - 1) / 2;
                        ev.total_steps = (total_pairs > INT_MAX) ? INT_MAX : (int) total_pairs;
                        ev.progress = (total_pairs > 0) ? (double) steps / (double) total_pairs : 0.0;
                        ev.description = "Buchberger S-多项式约化";
                        char detail[lv_SOLVER_DETAIL_BUF_SIZE];
                        int _snw_gb;
                        lv_SAFE_SNPRINTF(_snw_gb, detail, sizeof(detail),
                                         "{\"phase\":\"s_polynomial\",\"pair\":[%d,%d],"
                                         "\"basis_size\":%d,\"step\":%d,\"total_pairs\":%lld}",
                                         i, j, g_count, steps, total_pairs);
                        lv_UNUSED(_snw_gb);
                        ev.detail_json = detail;
                        stream_emit(solver_stream_ctx, &ev);
                    }
                }

                const MVMonomial *lt_i = mv_poly_leading_term(G[i]);
                const MVMonomial *lt_j = mv_poly_leading_term(G[j]);
                if (!lt_i || !lt_j)
                    continue;

                int *lcm_exp = lv_calloc((size_t) var_count, sizeof(int));
                mv_monomial_lcm(lt_i, lt_j, var_count, lcm_exp);

                bool lcm_is_lt_i = true, lcm_is_lt_j = true;
                for (int v = 0; v < var_count; v++) {
                    if (lcm_exp[v] != lt_i->exponents[v])
                        lcm_is_lt_i = false;
                    if (lcm_exp[v] != lt_j->exponents[v])
                        lcm_is_lt_j = false;
                }
                lv_free((void **) &lcm_exp);

                if (lcm_is_lt_i || lcm_is_lt_j) {
                    pair_data[i * g_capacity + j] = true;
                    continue;
                }

                bool skip_by_chain = false;
                for (int k = 0; k < g_count && k < i && !skip_by_chain; k++) {
                    if (!pair_data[k * g_capacity + i] || !pair_data[k * g_capacity + j])
                        continue;
                    const MVMonomial *lt_k = mv_poly_leading_term(G[k]);
                    if (!lt_k)
                        continue;
                    int *lcm_chain = lv_calloc((size_t) var_count, sizeof(int));
                    if (!lcm_chain)
                        continue;
                    mv_monomial_lcm(lt_i, lt_j, var_count, lcm_chain);
                    bool lt_k_divides_lcm = mv_monomial_divisible_lcm(lt_k, lcm_chain, var_count);
                    lv_free((void **) &lcm_chain);
                    if (lt_k_divides_lcm) {
                        skip_by_chain = true;
                    }
                }
                if (skip_by_chain) {
                    pair_data[i * g_capacity + j] = true;
                    continue;
                }

                pair_data[i * g_capacity + j] = true;

                MVPolynomial s_poly;
                s_polynomial(G[i], G[j], var_count, &s_poly);

                if (mv_poly_is_zero(&s_poly)) {
                    mv_poly_clear(&s_poly);
                    continue;
                }

                MVPolynomial remainder;
                polynomial_reduce(&s_poly, G, g_count, &remainder);
                mv_poly_clear(&s_poly);

                if (!mv_poly_is_zero(&remainder)) {
                    bool within_limit = true;
                    for (int k = 0; k < remainder.terms.count; k++) {
                        MVMonomial *rmk = (MVMonomial *)lv_darray_get(&remainder.terms, k);
                        if (!rmk) continue;
                        int td = mv_monomial_total_degree(rmk, var_count);
                        if (td > 4) {
                            int max_single_var_deg = 0;
                            for (int v = 0; v < var_count; v++) {
                                if (rmk->exponents[v] > max_single_var_deg) {
                                    max_single_var_deg = rmk->exponents[v];
                                }
                            }
                            if (max_single_var_deg > 4) {
                                within_limit = false;
                                break;
                            }
                        }
                    }

                    if (within_limit) {
                        if (g_count >= g_capacity) {
                            int old_capacity = g_capacity;
                            if (g_capacity > INT_MAX / 2) {
                                lv_set_error(lv_ERROR_OUT_OF_MEMORY, "buchberger_groebner: 基容量翻倍将溢出 INT_MAX");
                                mv_poly_clear(&remainder);
                                break;
                            }
                            g_capacity *= 2;
                            if (g_capacity > 0 && g_capacity > INT_MAX / g_capacity) {
                                lv_set_error(lv_ERROR_OUT_OF_MEMORY,
                                             "buchberger_groebner: pair矩阵容量平方将溢出 INT_MAX");
                                mv_poly_clear(&remainder);
                                break;
                            }
                            MVPolynomial **new_G = lv_realloc(G, g_capacity * sizeof(MVPolynomial *));
                            if (!new_G) {
                                lv_set_error(lv_ERROR_OUT_OF_MEMORY, "buchberger_groebner: 基扩容失败");
                                mv_poly_clear(&remainder);
                                break;
                            }
                            G = new_G;
                            bool *new_pair = lv_calloc((size_t) (g_capacity * g_capacity), sizeof(bool));
                            if (!new_pair) {
                                lv_set_error(lv_ERROR_OUT_OF_MEMORY, "buchberger_groebner: pair矩阵扩容失败");
                                mv_poly_clear(&remainder);
                                break;
                            }
                            for (int pi = 0; pi < old_capacity; pi++) {
                                for (int pj = 0; pj < old_capacity; pj++) {
                                    new_pair[pi * g_capacity + pj] = pair_data[pi * old_capacity + pj];
                                }
                            }
                            lv_free((void **) &pair_data);
                            pair_data = new_pair;
                        }
                        G[g_count] = lv_calloc(1, sizeof(MVPolynomial));
                        if (!G[g_count]) {
                            lv_set_error(lv_ERROR_OUT_OF_MEMORY, "buchberger_groebner: 基元素分配失败");
                            mv_poly_clear(&remainder);
                            break;
                        }
                        *G[g_count] = remainder;
                        g_count++;
                        changed = true;
                    } else {
                        mv_poly_clear(&remainder);
                    }
                } else {
                    mv_poly_clear(&remainder);
                }
            }
        }

        if (!changed) {
            gb_stalled_count++;
        } else {
            gb_stalled_count = 0;
            long double curr_total_terms = 0;
            for (int ti = 0; ti < g_count; ti++) {
                curr_total_terms += (long double) G[ti]->terms.count;
            }
            long double term_diff = curr_total_terms - prev_total_terms;
            if (fabsl(term_diff) < 1.0L && term_diff != 0.0L && g_count == prev_g_count) {
                gb_stalled_count++;
            }
            prev_total_terms = curr_total_terms;
            prev_g_count = g_count;
        }

        if (gb_stalled_count >= gb_max_stalled) {
            LOG_WARN("solver", "buchberger_groebner: 连续 %d 轮无有效进展，提前终止迭代", gb_stalled_count);
            break;
        }
    }

    {
        int reduced_count = 0;
        for (int i = 0; i < g_count; i++) {
            MVPolynomial **temp_G = NULL;
            if (g_count > 1) {
                temp_G = lv_calloc((size_t) (g_count - 1), sizeof(MVPolynomial *));
                if (temp_G) {
                    int idx = 0;
                    for (int j = 0; j < g_count; j++) {
                        if (j != i)
                            temp_G[idx++] = G[j];
                    }
                }
            }

            MVPolynomial remainder;
            polynomial_reduce(G[i], temp_G ? temp_G : NULL, temp_G ? (g_count - 1) : 0, &remainder);

            lv_free((void **) &temp_G);

            if (!mv_poly_is_zero(&remainder)) {
                mv_poly_clear(G[i]);
                mv_poly_init(G[i], var_count);
                mv_poly_copy(G[i], &remainder);
                mv_poly_clear(&remainder);
                reduced_count++;
            } else {
                mv_poly_clear(G[i]);
                lv_free((void **) &G[i]);
                mv_poly_clear(&remainder);
                for (int j = i; j < g_count - 1; j++) {
                    G[j] = G[j + 1];
                }
                g_count--;
                i--;
                reduced_count++;
            }
        }

        if (solver_stream_ctx) {
            StreamEvent ev;
            memset(&ev, 0, sizeof(ev));
            ev.type = STREAM_EVENT_SOLVE_GROEBNER_STEP;
            ev.timestamp_ms = stream_timestamp_ms();
            ev.step_number = steps;
            ev.description = "Gröbner 基自约化完成";
            char detail[lv_SOLVER_DETAIL_BUF_SIZE];
            int _snw_ar;
            lv_SAFE_SNPRINTF(_snw_ar, detail, sizeof(detail),
                             "{\"phase\":\"auto_reduction\",\"reduced_count\":%d,"
                             "\"final_basis_size\":%d,\"total_steps\":%d}",
                             reduced_count, g_count, steps);
            lv_UNUSED(_snw_ar);
            ev.detail_json = detail;
            stream_emit(solver_stream_ctx, &ev);
        }
    }

    int write = 0;
    for (int i = 0; i < g_count; i++) {
        if (!mv_poly_is_zero(G[i])) {
            if (write != i)
                G[write] = G[i];
            write++;
        } else {
            mv_poly_clear(G[i]);
            lv_free((void **) &G[i]);
        }
    }
    g_count = write;

    for (int i = 0; i < g_count; i++) {
        mv_poly_sort(G[i]);
    }

    lv_free((void **) &pair_data);

    *out_G = G;
    *out_g_count = g_count;

    return (steps >= step_limit) ? SOLVER_STATUS_TIMEOUT : SOLVER_STATUS_OK;
}

/* ================================================================== */
/*  内部: 从 EquationSystem 构建多变量多项式                            */
/* ================================================================== */

static MVPolynomial *build_mv_polynomials(EquationSystem *sys, int **var_id_map, int **out_coord_map,
                                          int *out_var_count) {
    int eq_count = sys->eqs.count;
    int *vids = lv_calloc((size_t) eq_count * 2, sizeof(int));
    if (!vids)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "build_mv_polynomials: lv_calloc for vids failed (count=%d)", eq_count);
    int *cids = lv_calloc((size_t) eq_count * 2, sizeof(int));
    if (!cids) {
        lv_free((void **) &vids);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "build_mv_polynomials: lv_calloc for cids failed (count=%d)", eq_count);
    }
    int vcount = 0;

    for (int i = 0; i < eq_count; i++) {
        PolyEquation *eq = (PolyEquation *)lv_darray_get(&sys->eqs, i);
        if (!eq || eq->poly.degree < 0)
            continue;
        int vid = eq->var_node_id;
        int cid = eq->coord_index;
        bool found = false;
        for (int j = 0; j < vcount; j++) {
            if (vids[j] == vid && cids[j] == cid) {
                found = true;
                break;
            }
        }
        if (!found) {
            vids[vcount] = vid;
            cids[vcount] = cid;
            vcount++;
        }
    }

    *var_id_map = vids;
    *out_coord_map = cids;
    *out_var_count = vcount;

    MVPolynomial *polys = lv_calloc((size_t) eq_count, sizeof(MVPolynomial));
    if (!polys) {
        lv_free((void **) &vids);
        lv_free((void **) &cids);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "build_mv_polynomials: lv_calloc for polys failed (count=%d)", eq_count);
    }
    for (int i = 0; i < eq_count; i++) {
        mv_poly_init(&polys[i], vcount);

        PolyEquation *eq = (PolyEquation *)lv_darray_get(&sys->eqs, i);
        if (!eq || eq->poly.degree < 0)
            continue;

        int var_idx = -1;
        for (int j = 0; j < vcount; j++) {
            if (poly_eq_same_key(vids[j], cids[j], eq->var_node_id, eq->coord_index)) {
                var_idx = j;
                break;
            }
        }
        if (var_idx < 0)
            continue;

        mpz_poly_t *p = &eq->poly;
        for (int d = 0; d <= p->degree; d++) {
            int *exponents = lv_calloc((size_t) vcount, sizeof(int));
            if (!exponents) {
                for (int k = 0; k < i; k++) {
                    mv_poly_clear(&polys[k]);
                }
                for (int k = i; k < eq_count; k++) {
                    memset(&polys[k], 0, sizeof(mpz_poly_t));
                }
                lv_free((void **) &polys);
                lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "build_mv_polynomials: lv_calloc for exponents failed (vcount=%d)", vcount);
            }
            memset(exponents, 0, (size_t) vcount * sizeof(int));
            exponents[var_idx] = d;
            mv_poly_add_term(&polys[i], p->coeffs[d], exponents);
            lv_free((void **) &exponents);
        }
        mv_poly_sort(&polys[i]);
    }

    return polys;
}

/* ================================================================== */
/*  PUBLIC API: groebner_basis_compute                                 */
/* ================================================================== */

SolverStatus groebner_basis_compute(EquationSystem *system) {
    if (!system || system->eqs.count == 0)
        return SOLVER_STATUS_OK;

    /* Step 1: Check degree limit first (fast path) */
    for (int i = 0; i < system->eqs.count; i++) {
        PolyEquation *eq = (PolyEquation *)lv_darray_get(&system->eqs, i);
        if (eq && eq->poly.degree > 4) {
            return SOLVER_STATUS_OUT_OF_SCOPE;
        }
    }

    /* Step 2: Build multivariate polynomial representation */
    int *var_id_map = NULL;
    int *coord_map = NULL;
    int var_count = 0;
    MVPolynomial *mv_polys = build_mv_polynomials(system, &var_id_map, &coord_map, &var_count);

    if (var_count == 0 || !mv_polys) {
        lv_free((void **) &var_id_map);
        lv_free((void **) &coord_map);
        return SOLVER_STATUS_OK;
    }

    /* Step 3: Filter out zero polynomials and collect non-trivial ones */
    int active_count = 0;
    for (int i = 0; i < system->eqs.count; i++) {
        if (!mv_poly_is_zero(&mv_polys[i])) {
            active_count++;
        }
    }

    if (active_count == 0) {
        for (int i = 0; i < system->eqs.count; i++) {
            mv_poly_clear(&mv_polys[i]);
        }
        lv_free((void **) &mv_polys);
        lv_free((void **) &var_id_map);
        lv_free((void **) &coord_map);
        return SOLVER_STATUS_OK;
    }

    MVPolynomial **active = lv_calloc((size_t) active_count, sizeof(MVPolynomial *));
    if (!active) {
        for (int i = 0; i < system->eqs.count; i++)
            mv_poly_clear(&mv_polys[i]);
        lv_free((void **) &mv_polys);
        lv_free((void **) &var_id_map);
        lv_free((void **) &coord_map);
        return SOLVER_STATUS_TIMEOUT;
    }
    int idx = 0;
    for (int i = 0; i < system->eqs.count; i++) {
        if (!mv_poly_is_zero(&mv_polys[i])) {
            active[idx++] = &mv_polys[i];
        }
    }

    /* Step 4: Run Buchberger's algorithm */
    MVPolynomial **G = NULL;
    int g_count = 0;
    SolverStatus status = buchberger_groebner(active, active_count, &G, &g_count, 10000);

    if (solver_stream_ctx) {
        StreamEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = STREAM_EVENT_SOLVE_GROEBNER_STEP;
        ev.timestamp_ms = stream_timestamp_ms();
        ev.step_number = 0;
        ev.description = "Groebner 基计算完成";
        char detail[lv_SOLVER_DETAIL_BUF_SIZE];
        int _snw_gc;
        lv_SAFE_SNPRINTF(_snw_gc, detail, sizeof(detail),
                         "{\"phase\":\"groebner_complete\",\"status\":\"%s\","
                         "\"input_equations\":%d,\"active_equations\":%d,"
                         "\"basis_size\":%d,\"variables\":%d}",
                         (status == SOLVER_STATUS_OK)             ? "ok"
                         : (status == SOLVER_STATUS_OUT_OF_SCOPE) ? "out_of_scope"
                                                                  : "timeout",
                         system->eqs.count, active_count, g_count, var_count);
        lv_UNUSED(_snw_gc);
        ev.detail_json = detail;
        stream_emit(solver_stream_ctx, &ev);
    }

    lv_free((void **) &active);

    if (status == SOLVER_STATUS_OUT_OF_SCOPE) {
        for (int i = 0; i < system->eqs.count; i++) {
            mv_poly_clear(&mv_polys[i]);
        }
        lv_free((void **) &mv_polys);
        lv_free((void **) &var_id_map);
        lv_free((void **) &coord_map);
        return SOLVER_STATUS_OUT_OF_SCOPE;
    }

    int original_eq_count = system->eqs.count;

    equation_system_clear(system);
    equation_system_init(system);

    if (G) {
        for (int i = 0; i < g_count; i++) {
            if (mv_poly_is_zero(G[i]))
                continue;

            int best_var = -1;
            int best_degree = -1;

            for (int v = 0; v < var_count; v++) {
                int max_deg_v = 0;
                for (int t = 0; t < G[i]->terms.count; t++) {
                    const MVMonomial *mt = (const MVMonomial *)lv_darray_get(&G[i]->terms, t);
                    if (mt && mt->exponents[v] > max_deg_v) {
                        max_deg_v = mt->exponents[v];
                    }
                }
                if (max_deg_v > best_degree) {
                    best_degree = max_deg_v;
                    best_var = v;
                }
            }

            if (best_var < 0 || best_degree < 0)
                continue;

            mpz_poly_t poly;
            mpz_poly_init(&poly);
            poly.degree = best_degree;
            poly.coeffs = lv_malloc((best_degree + 1) * sizeof(mpz_t));
            if (!poly.coeffs) {
                mpz_poly_clear(&poly);
                continue;
            }
            for (int d = 0; d <= best_degree; d++) {
                mpz_init_set_si(poly.coeffs[d], 0);
            }

            for (int t = 0; t < G[i]->terms.count; t++) {
                const MVMonomial *mt = (const MVMonomial *)lv_darray_get(&G[i]->terms, t);
                if (!mt) continue;
                int power = mt->exponents[best_var];
                if (power <= best_degree) {
                    mpz_add(poly.coeffs[power], poly.coeffs[power], mt->coeff);
                }
            }

            int node_id = var_id_map[best_var];
            int coord_index = coord_map ? coord_map[best_var] : 0;

            EQUATION_PUSH_OR_GOTO(system, poly, node_id, coord_index, push_error);
            mpz_poly_clear(&poly);
        }

        for (int i = 0; i < g_count; i++) {
            mv_poly_clear(G[i]);
            lv_free((void **) &G[i]);
        }
        lv_free((void **) &G);
    }

    for (int i = 0; i < original_eq_count; i++) {
        mv_poly_clear(&mv_polys[i]);
    }
    lv_free((void **) &mv_polys);
    lv_free((void **) &var_id_map);
    lv_free((void **) &coord_map);

    return (status == SOLVER_STATUS_TIMEOUT) ? SOLVER_STATUS_TIMEOUT : SOLVER_STATUS_OK;
push_error:
    if (G) {
        for (int i = 0; i < g_count; i++) {
            if (G[i]) {
                mv_poly_clear(G[i]);
                lv_free((void **) &G[i]);
            }
        }
        lv_free((void **) &G);
    }
    if (mv_polys) {
        for (int i = 0; i < original_eq_count; i++) {
            mv_poly_clear(&mv_polys[i]);
        }
        lv_free((void **) &mv_polys);
    }
    lv_free((void **) &var_id_map);
    lv_free((void **) &coord_map);
    return SOLVER_STATUS_OUT_OF_MEMORY;
}

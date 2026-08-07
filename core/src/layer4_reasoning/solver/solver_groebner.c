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
#include "../mv_polynomial.h"

/* 流式上下文（定义在 solver_engine.c，通过 solver_types.h 的 extern 引用） */

/* ================================================================== */
/*  内部: 多变量单项式表示 (用于 Groebner 基计算)                      */
/*  类型与基础操作统一使用 mv_polynomial.h 公共实现，避免重复定义      */
/* ================================================================== */

/* 便捷包装：返回首项指针（空多项式返回 NULL），避免公共 API 的错误码副作用 */
static const MVMonomial *groebner_leading_term(const MVPolynomial *p) {
    return p->term_count > 0 ? &p->terms[0] : NULL;
}

/* ================================================================== */
/*  内部: S-多项式计算 (Groebner 基核心)                                */
/* ================================================================== */

static void s_polynomial(const MVPolynomial *f, const MVPolynomial *g, int var_count, MVPolynomial *result) {
    mv_poly_init(result, var_count);

    const MVMonomial *lt_f = groebner_leading_term(f);
    const MVMonomial *lt_g = groebner_leading_term(g);
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
    /* 迭代上限来自 lvConfig.solver.solver_max_iterations（默认 10000） */
    const int max_iterations = lv_config_current()->solver.solver_max_iterations;

    int stalled_rounds = 0;
    const int max_stalled_rounds = 10;
    long double prev_term_count = (long double) remainder->term_count;
    int term_oscillation = 0;

    while (changed && safety < max_iterations) {
        changed = false;
        safety++;

        for (int i = 0; i < g_count; i++) {
            if (mv_poly_is_zero(G[i]))
                continue;

            const MVMonomial *lt_g = groebner_leading_term(G[i]);
            if (!lt_g)
                continue;

            bool reduced = false;
            for (int j = 0; j < remainder->term_count && !reduced; j++) {
                MVMonomial *rj = &remainder->terms[j];
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

                    for (int k = 0; k < remainder->term_count; k++) {
                        MVMonomial *rk = &remainder->terms[k];
                        if (!rk) continue;
                        mpz_t scaled;
                        mpz_init(scaled);
                        mpz_mul(scaled, rk->coeff, lt_g->coeff);
                        mv_poly_add_term(&new_remainder, scaled, rk->exponents);
                        mpz_clear(scaled);
                    }
                    for (int k = 0; k < sub_term.term_count; k++) {
                        MVMonomial *sk = &sub_term.terms[k];
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
            long double curr_term_count = (long double) remainder->term_count;
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
/*  内部: 并行 Gröbner 可选入口（默认关闭，保守接入）                   */
/*                                                                    */
/*  背景：backends/groebner_parallel.c（v5.0.0 并行引擎）与主流程       */
/*  数据结构不兼容，且当前为「单线程并行框架」：                        */
/*   - 公共入口 lv_groebner_parallel_compute() 将输入强转为 int**      */
/*     子句（SAT 编码），内部按「单项式 x^|lit|、系数 1.0、var_count   */
/*     =1」构造基；MVPolynomial 为多变量、mpz 精确系数、完整指数向量， */
/*     两者不存在无损双向映射。                                        */
/*   - 结果 groebner_basis 为 .c 内部私有类型 SimplePoly，头文件        */
/*     groebner_parallel.h 不暴露项/系数/指数读取接口，无法反转换      */
/*     回 MVPolynomial。                                               */
/*   - 线程模型：注释明示「顺序执行所有线程的工作（当前为单线程并行    */
/*     框架）」；worker_process 对共享基的 realloc 与计数器 ++ 均无锁， */
/*     仅因顺序执行未暴露数据竞争；系数为 double + epsilon 截断，      */
/*     与主流程 mpz 精确语义不符。                                     */
/*  因此并行路径当前不可用。本函数作为预留接入点：开关（env            */
/*  LV_GROEBNER_PARALLEL=1）开启时先做阈值与格式可行性检查，任何       */
/*  不兼容/失败一律返回 false，由调用方回退下方串行 Buchberger，       */
/*  保证主流程行为与原来逐位一致（方案 A 的「转换→并行→反转换→校验」  */
/*  待并行引擎提供通用多项式输入/输出 API 后在此补齐）。               */
/* ================================================================== */

/* 并行路径开关：内部静态，默认关闭；最小侵入，不动 config.h / lvConfig */
static bool groebner_parallel_path_enabled(void) {
    static int enabled = -1;
    if (enabled < 0) {
        const char *e = getenv("LV_GROEBNER_PARALLEL");
        enabled = (e && e[0] == '1') ? 1 : 0;
    }
    return enabled != 0;
}

/* 尝试并行 Gröbner 路径。成功返回 true 并填充 out_G/out_g_count；
 * 不可用/失败返回 false，调用方回退串行实现。 */
static bool groebner_try_parallel(MVPolynomial **F, int f_count,
                                  MVPolynomial ***out_G, int *out_g_count) {
    *out_G = NULL;
    *out_g_count = 0;

    /* 开关默认关闭（保守优先） */
    if (!groebner_parallel_path_enabled())
        return false;

    /* 输入规模阈值：并行仅在输入规模可估计的范围内尝试（S 对数量
     * 与 f_count 平方同阶），避免无谓开销；与串行 step_limit=10000
     * 的规模语义对齐。 */
    if (f_count < 2 || f_count > 4096)
        return false;

    /* 格式可行性检查：并行引擎公共输入为 SAT 子句编码（int**，单变量
     * x^|lit|，double 系数），MVPolynomial 为多变量 mpz 精确多项式，
     * 不存在无损双向映射 → 并行路径不可用，回退串行并告警。 */
    LOG_WARN("solver",
             "groebner_try_parallel: 并行引擎输入为 SAT 子句编码(int**，单变量"
             "double)，与 MVPolynomial 精确多变量格式不兼容，回退串行"
             "Buchberger（f_count=%d, var_count=%d）",
             f_count, (F && F[0]) ? F[0]->var_count : 0);
    return false;
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

    /* 并行增强路径（默认关闭）：成功则直接返回；
     * 不可用/失败一律回退下方原有串行实现，行为逐位不变。 */
    {
        MVPolynomial **par_G = NULL;
        int par_g_count = 0;
        if (groebner_try_parallel(F, f_count, &par_G, &par_g_count)) {
            *out_G = par_G;
            *out_g_count = par_g_count;
            return SOLVER_STATUS_OK;
        }
    }

    int var_count = F[0]->var_count;

    for (int i = 0; i < f_count; i++) {
        for (int j = 0; j < F[i]->term_count; j++) {
            const MVMonomial *mfj = &F[i]->terms[j];
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
        prev_total_terms += (long double) G[ti]->term_count;
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

                const MVMonomial *lt_i = groebner_leading_term(G[i]);
                const MVMonomial *lt_j = groebner_leading_term(G[j]);
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
                    const MVMonomial *lt_k = groebner_leading_term(G[k]);
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
                    for (int k = 0; k < remainder.term_count; k++) {
                        MVMonomial *rmk = &remainder.terms[k];
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
                            /* G 数组统一扩容（内部含 INT_MAX 溢出检查与倍增） */
                            if (!lv_ensure_capacity((void **) &G, g_count, &g_capacity,
                                                    sizeof(MVPolynomial *), 1)) {
                                lv_set_error(lv_ERROR_OUT_OF_MEMORY, "buchberger_groebner: 基扩容失败");
                                mv_poly_clear(&remainder);
                                break;
                            }
                            /* pair 矩阵按新容量（g_capacity²）重建 */
                            if (g_capacity > 0 && g_capacity > INT_MAX / g_capacity) {
                                lv_set_error(lv_ERROR_OUT_OF_MEMORY,
                                             "buchberger_groebner: pair矩阵容量平方将溢出 INT_MAX");
                                mv_poly_clear(&remainder);
                                break;
                            }
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
                curr_total_terms += (long double) G[ti]->term_count;
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
                for (int t = 0; t < G[i]->term_count; t++) {
                    const MVMonomial *mt = &G[i]->terms[t];
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

            for (int t = 0; t < G[i]->term_count; t++) {
                const MVMonomial *mt = &G[i]->terms[t];
                if (!mt) continue;
                int power = mt->exponents[best_var];
                if (power <= best_degree) {
                    mpz_add(poly.coeffs[power], poly.coeffs[power], mt->coeff);
                }
            }

            int node_id = var_id_map[best_var];
            int coord_index = coord_map ? coord_map[best_var] : 0;

            if (lv_equation_push_checked(system, poly, node_id, coord_index) != 0) goto push_error;
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

/**
 * @file groebner_parallel.c
 * @brief 并行 Groebner 基计算引擎 —— 基于 Buchberger 算法的多线程实现
 *
 * 实现带工作窃取（work-stealing）的并行 Buchberger 算法：
 * - 将 S-多项式对分配给工作线程池
 * - 每个工作线程从队列中取对、计算 S-多项式、对当前基约化
 * - 若约化结果非零，加入基并生成新的对
 * - 支持中间约化（inter-reduction）以控制基的大小
 * - 使用原子计数器跟踪完成进度
 */

#include "lv/groebner_parallel.h"

#include <limits.h>
#include <math.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv.h"
#include "lv/lv_numeric.h"
#include "lv/lv_utils.h"

#include "lv_internal.h"

/* ========================================================================
 * 内部常量
 * ======================================================================== */

/* 运行时配置位于 lvConfig 中，通过 lv_config_get_int() 读取 */

/* ========================================================================
 * 内部数据结构
 * ======================================================================== */

/** S-多项式对：表示需要处理的一对多项式索引 */
typedef struct {
    int i; /**< 基中第一个多项式的索引 */
    int j; /**< 基中第二个多项式的索引 */
} SPair;

/** 工作队列：存储待处理的 S-多项式对 */
typedef struct WorkQueue {
    lvDArray pairs; /**< 对数组 (SPair) */
    int head;       /**< 队列头部索引（出队位置） */
    int tail;       /**< 队列尾部索引（入队位置） */
} WorkQueue;

/** 多项式项：单项式 c * x^a * y^b * ... */
typedef struct {
    double coeff;   /**< 系数 */
    int *exponents; /**< 各变量指数数组 */
    int var_count;  /**< 变量数 */
} PolyTerm;

/** 简化多项式表示（用于内部计算） */
typedef struct {
    lvDArray terms;    /**< 项数组 (PolyTerm) */
} SimplePoly;

/** 工作线程参数 */
typedef struct {
    int thread_id;                  /**< 线程 ID */
    lvGroebnerParallel *engine;     /**< 所属引擎 */
    SimplePoly *basis;              /**< 当前基（共享，需同步访问） */
    int basis_size;                 /**< 当前基大小 */
    WorkQueue *local_queue;         /**< 线程本地工作队列 */
    WorkQueue **all_queues;         /**< 所有线程的队列（用于 work-stealing） */
    int num_threads;                /**< 总线程数 */
    int basis_cap;                  /**< 基数组容量（lv_ensure_capacity 倍增维护） */
    atomic_int *shutdown_flag;      /**< 关闭标志（C11 原子） */
    atomic_int *global_completed;   /**< 全局完成对计数（C11 原子） */
    atomic_int *global_total;       /**< 全局总对数（C11 原子） */
} WorkerArg;

/* ========================================================================
 * 工作队列操作
 * ======================================================================== */

/** 初始化工作队列，成功返回0，失败返回-1 */
static int work_queue_init(WorkQueue *q, int initial_capacity) {
    lv_darray_init(&q->pairs, sizeof(SPair));
    if (!lv_darray_reserve(&q->pairs, initial_capacity))
        return -1;
    q->head = 0;
    q->tail = 0;
    return 0;
}

/** 销毁工作队列 */
static void work_queue_destroy(WorkQueue *q) {
    if (q) {
        lv_darray_free(&q->pairs);
        q->head = 0;
        q->tail = 0;
    }
}

/** 向队列添加一个 S-多项式对 */
static int work_queue_push(WorkQueue *q, int i, int j) {
    SPair pair = {i, j};
    if (lv_darray_push(&q->pairs, &pair) < 0)
        return -1;
    q->tail = q->pairs.count;
    return 0;
}

/** 从队列取出一个 S-多项式对 */
static int work_queue_pop(WorkQueue *q, SPair *out) {
    if (q->pairs.count <= q->head)
        return -1;
    SPair *p = (SPair *)lv_darray_get(&q->pairs, q->head);
    *out = *p;
    q->head++;
    return 0;
}

/** 从其他线程的队列窃取工作（从尾部取） */
static int work_queue_steal(WorkQueue *q, SPair *out) {
    if (q->pairs.count <= q->head)
        return -1;
    SPair *p = (SPair *)lv_darray_get(&q->pairs, q->pairs.count - 1);
    *out = *p;
    q->pairs.count--;
    return 0;
}

/* ========================================================================
 * 简化多项式操作
 * ======================================================================== */

/** 创建空多项式，成功返回true */
static bool simple_poly_create(SimplePoly *out, int initial_capacity) {
    lv_darray_init(&out->terms, sizeof(PolyTerm));
    return lv_darray_reserve(&out->terms, initial_capacity);
}

/** 销毁多项式 */
static void simple_poly_destroy(SimplePoly *p) {
    if (!p)
        return;
    for (int i = 0; i < p->terms.count; i++) {
        PolyTerm *pt = (PolyTerm *)lv_darray_get(&p->terms, i);
        lv_free((void **) &pt->exponents);
    }
    lv_darray_free(&p->terms);
}

/** 添加一个项到多项式 */
static int simple_poly_add_term(SimplePoly *p, double coeff, const int *exponents, int var_count) {
    PolyTerm pt;
    pt.coeff = coeff;
    pt.var_count = var_count;
    if (var_count > 0) {
        pt.exponents = (int *) lv_malloc((size_t) var_count * sizeof(int));
        if (!pt.exponents)
            return -1;
        memcpy(pt.exponents, exponents, (size_t) var_count * sizeof(int));
    } else {
        pt.exponents = NULL;
    }
    if (lv_darray_push(&p->terms, &pt) < 0) {
        lv_free((void **) &pt.exponents);
        return -1;
    }
    return 0;
}

/** 判断多项式是否为零 */
static int simple_poly_is_zero(const SimplePoly *p) {
    return p == NULL || p->terms.count == 0;
}

/** 指数向量相等性比较（需同 var_count） */
static int term_exp_equal(const PolyTerm *a, const PolyTerm *b) {
    if (a->var_count != b->var_count)
        return 0;
    for (int i = 0; i < a->var_count; i++) {
        if (a->exponents[i] != b->exponents[i])
            return 0;
    }
    return 1;
}

/** 单项式比较（grlex 降序）：总次数大者领先；同次数按指数向量
 *  从最高变量位起逐位比较，大者领先。返回 1 表示 a 领先于 b。 */
static int term_gt(const PolyTerm *a, const PolyTerm *b) {
    int na = a->var_count;
    int nb = b->var_count;
    int n = (na > nb) ? na : nb;
    int da = 0, db = 0;
    for (int i = 0; i < n; i++) {
        int ea = (i < na) ? a->exponents[i] : 0;
        int eb = (i < nb) ? b->exponents[i] : 0;
        da += ea;
        db += eb;
    }
    if (da != db)
        return da > db;
    for (int i = n - 1; i >= 0; i--) {
        int ea = (i < na) ? a->exponents[i] : 0;
        int eb = (i < nb) ? b->exponents[i] : 0;
        if (ea != eb)
            return ea > eb;
    }
    return 0; /* 指数向量相等 */
}

/** 规范化多项式：grlex 降序排序（terms[0] = 领先单项式）、合并同类项、
 *  剔除系数近零的项。 */
static void simple_poly_normalize(SimplePoly *p) {
    if (!p)
        return;
    int cnt = (int) p->terms.count;
    if (cnt <= 0)
        return;

    /* 1. 插入排序（多项式项数通常很少） */
    if (cnt > 1) {
        for (int i = 1; i < cnt; i++) {
            PolyTerm key = *(PolyTerm *) lv_darray_get(&p->terms, i);
            int j = i - 1;
            while (j >= 0) {
                PolyTerm *prev = (PolyTerm *) lv_darray_get(&p->terms, j);
                if (term_gt(&key, prev)) {
                    *(PolyTerm *) lv_darray_get(&p->terms, j + 1) = *prev;
                    j--;
                } else {
                    break;
                }
            }
            *(PolyTerm *) lv_darray_get(&p->terms, j + 1) = key;
        }
    }

    /* 2. 合并同类项 */
    int w = 0;
    for (int i = 0; i < cnt; i++) {
        PolyTerm *t = (PolyTerm *) lv_darray_get(&p->terms, i);
        if (w > 0) {
            PolyTerm *prev = (PolyTerm *) lv_darray_get(&p->terms, w - 1);
            if (term_exp_equal(prev, t)) {
                prev->coeff += t->coeff;
                lv_free((void **) &t->exponents);
                continue;
            }
        }
        if (w != i)
            *(PolyTerm *) lv_darray_get(&p->terms, w) = *t;
        w++;
    }
    p->terms.count = w;

    /* 3. 剔除系数近零的项 */
    for (int i = (int) p->terms.count - 1; i >= 0; i--) {
        PolyTerm *t = (PolyTerm *) lv_darray_get(&p->terms, i);
        if (lv_is_zero(t->coeff, lv_EPSILON_DOUBLE)) {
            lv_free((void **) &t->exponents);
            if (i < (int) p->terms.count - 1) {
                PolyTerm *last = (PolyTerm *) lv_darray_get(&p->terms, p->terms.count - 1);
                *t = *last;
            }
            p->terms.count--;
        }
    }
}

/** 就地乘以二项式 (c0 + c1 * x_var)：p = p * (c0 + c1 * x_var)。
 *  调用方随后应执行 simple_poly_normalize 合并同类项。 */
static int simple_poly_mul_binomial(SimplePoly *p, double c0, double c1, int var_idx) {
    if (!p || p->terms.count == 0)
        return 0;

    if (c0 == 0.0) {
        /* p *= c1 * x_var（仅系数缩放与指数平移） */
        for (int i = 0; i < p->terms.count; i++) {
            PolyTerm *t = (PolyTerm *) lv_darray_get(&p->terms, i);
            t->coeff *= c1;
            if (t->var_count > var_idx)
                t->exponents[var_idx] += 1;
        }
        return 0;
    }

    /* 复制 p 到 tmp */
    SimplePoly tmp;
    if (!simple_poly_create(&tmp, p->terms.count))
        return -1;
    for (int i = 0; i < p->terms.count; i++) {
        PolyTerm *t = (PolyTerm *) lv_darray_get(&p->terms, i);
        if (simple_poly_add_term(&tmp, t->coeff, t->exponents, t->var_count) != 0) {
            simple_poly_destroy(&tmp);
            return -1;
        }
    }

    /* p *= c0 */
    for (int i = 0; i < p->terms.count; i++)
        ((PolyTerm *) lv_darray_get(&p->terms, i))->coeff *= c0;

    /* tmp *= c1 * x_var */
    for (int i = 0; i < tmp.terms.count; i++) {
        PolyTerm *t = (PolyTerm *) lv_darray_get(&tmp.terms, i);
        t->coeff *= c1;
        if (t->var_count > var_idx)
            t->exponents[var_idx] += 1;
    }

    /* p += tmp */
    for (int i = 0; i < tmp.terms.count; i++) {
        PolyTerm *t = (PolyTerm *) lv_darray_get(&tmp.terms, i);
        if (simple_poly_add_term(p, t->coeff, t->exponents, t->var_count) != 0) {
            simple_poly_destroy(&tmp);
            return -1;
        }
    }

    simple_poly_destroy(&tmp);
    return 0;
}

/* ========================================================================
 * Buchberger 算法核心
 * ======================================================================== */

/**
 * @brief 计算 S-多项式
 *
 * 给定基中的两个多项式 f_i 和 f_j，计算它们的 S-多项式。
 * S(f_i, f_j) = (LCM(LT(f_i), LT(f_j)) / LT(f_i)) * f_i
 *            - (LCM(LT(f_i), LT(f_j)) / LT(f_j)) * f_j
 *
 * @param f     当前基
 * @param fi    第一个多项式索引
 * @param fj    第二个多项式索引
 * @param basis_size 基大小
 * @return S-多项式
 */
static SimplePoly compute_s_polynomial(const SimplePoly *f, int fi, int fj, int basis_size) {
    SimplePoly result;
    simple_poly_create(&result, 8);
    if (fi < 0 || fi >= basis_size || fj < 0 || fj >= basis_size)
        return result;

    const SimplePoly *gi = &f[fi];
    const SimplePoly *gj = &f[fj];

    /* 如果任一多项式为空或首项系数为零，S-多项式为零 */
    if (simple_poly_is_zero(gi) || simple_poly_is_zero(gj))
        return result;
    if (fabs(((PolyTerm *)lv_darray_get(&gi->terms, 0))->coeff) < lv_EPSILON_DOUBLE ||
        fabs(((PolyTerm *)lv_darray_get(&gj->terms, 0))->coeff) < lv_EPSILON_DOUBLE)
        return result;

    /* 计算 LCM(leading terms) */
    int var_count = ((PolyTerm *)lv_darray_get(&gi->terms, 0))->var_count;
    if (var_count == 0)
        var_count = ((PolyTerm *)lv_darray_get(&gj->terms, 0))->var_count;

    int *lcm_exp = NULL;
    if (var_count > 0) {
        lcm_exp = (int *) lv_calloc((size_t) var_count, sizeof(int));
        if (!lcm_exp)
            return result;

        for (int v = 0; v < var_count; v++) {
            int ei = (v < ((PolyTerm *)lv_darray_get(&gi->terms, 0))->var_count) ? ((PolyTerm *)lv_darray_get(&gi->terms, 0))->exponents[v] : 0;
            int ej = (v < ((PolyTerm *)lv_darray_get(&gj->terms, 0))->var_count) ? ((PolyTerm *)lv_darray_get(&gj->terms, 0))->exponents[v] : 0;
            lcm_exp[v] = (ei > ej) ? ei : ej;
        }
    }

    /* 计算 multiplier_i = LCM / LT(gi) 的指数部分 */
    int *mult_i_exp = NULL;
    if (var_count > 0) {
        mult_i_exp = (int *) lv_calloc((size_t) var_count, sizeof(int));
        if (!mult_i_exp) {
            lv_free((void **) &(lcm_exp));
            return result;
        }
        for (int v = 0; v < var_count; v++) {
            int ei = (v < ((PolyTerm *)lv_darray_get(&gi->terms, 0))->var_count) ? ((PolyTerm *)lv_darray_get(&gi->terms, 0))->exponents[v] : 0;
            mult_i_exp[v] = lcm_exp[v] - ei;
        }
    }

    /* 计算 multiplier_j = LCM / LT(gj) 的指数部分 */
    int *mult_j_exp = NULL;
    if (var_count > 0) {
        mult_j_exp = (int *) lv_calloc((size_t) var_count, sizeof(int));
        if (!mult_j_exp) {
            lv_free((void **) &(lcm_exp));
            lv_free((void **) &(mult_i_exp));
            return result;
        }
        for (int v = 0; v < var_count; v++) {
            int ej = (v < ((PolyTerm *)lv_darray_get(&gj->terms, 0))->var_count) ? ((PolyTerm *)lv_darray_get(&gj->terms, 0))->exponents[v] : 0;
            mult_j_exp[v] = lcm_exp[v] - ej;
        }
    }

    /* S = (1/LC(gi)) * mult_i * gi - (1/LC(gj)) * mult_j * gj
     * 完整实现：对所有项（不仅是首项）乘以系数后相减 */
    double scale_i = 1.0 / ((PolyTerm *)lv_darray_get(&gi->terms, 0))->coeff;
    double scale_j = 1.0 / ((PolyTerm *)lv_darray_get(&gj->terms, 0))->coeff;

    /* 将 mult_i * gi 的项加入结果 */
    for (int t = 0; t < gi->terms.count; t++) {
        int *new_exp = NULL;
        if (var_count > 0) {
            new_exp = (int *) lv_calloc((size_t) var_count, sizeof(int));
            if (!new_exp)
                break;
            for (int v = 0; v < var_count; v++) {
                int et = (v < ((PolyTerm *)lv_darray_get(&gi->terms, t))->var_count) ? ((PolyTerm *)lv_darray_get(&gi->terms, t))->exponents[v] : 0;
                new_exp[v] = mult_i_exp[v] + et;
            }
        }
        if (simple_poly_add_term(&result, scale_i * ((PolyTerm *)lv_darray_get(&gi->terms, t))->coeff, new_exp, var_count) != 0) {
            lv_free((void **) &new_exp);
            lv_free((void **) &lcm_exp);
            lv_free((void **) &mult_i_exp);
            lv_free((void **) &mult_j_exp);
            simple_poly_destroy(&result);
            lv_darray_init(&result.terms, sizeof(PolyTerm));
            return result;
        }
        lv_free((void **) &new_exp);
    }

    /* 减去 mult_j * gj 的项 */
    for (int t = 0; t < gj->terms.count; t++) {
        int *new_exp = NULL;
        if (var_count > 0) {
            new_exp = (int *) lv_calloc((size_t) var_count, sizeof(int));
            if (!new_exp)
                break;
            for (int v = 0; v < var_count; v++) {
                int et = (v < ((PolyTerm *)lv_darray_get(&gj->terms, t))->var_count) ? ((PolyTerm *)lv_darray_get(&gj->terms, t))->exponents[v] : 0;
                new_exp[v] = mult_j_exp[v] + et;
            }
        }
        if (simple_poly_add_term(&result, -scale_j * ((PolyTerm *)lv_darray_get(&gj->terms, t))->coeff, new_exp, var_count) != 0) {
            lv_free((void **) &new_exp);
            lv_free((void **) &lcm_exp);
            lv_free((void **) &mult_i_exp);
            lv_free((void **) &mult_j_exp);
            simple_poly_destroy(&result);
            lv_darray_init(&result.terms, sizeof(PolyTerm));
            return result;
        }
        lv_free((void **) &new_exp);
    }

    lv_free((void **) &lcm_exp);
    lv_free((void **) &mult_i_exp);
    lv_free((void **) &mult_j_exp);

    /* 合并同类项并按 grlex 降序排序，保证 terms[0] 为真正的领先单项式 */
    simple_poly_normalize(&result);

    return result;
}

/**
 * @brief 多项式约化：用基 G 对多项式 f 进行约化
 *
 * 反复用 G 中能整除 f 的首项的多项式 g 约化 f，
 * 直到 f 的首项不能被 G 中任何多项式的首项整除。
 *
 * @param f          待约化的多项式
 * @param basis      当前基
 * @param basis_size 基大小
 * @return 约化后的多项式
 */
static SimplePoly reduce_poly(SimplePoly f, const SimplePoly *basis, int basis_size) {
    if (simple_poly_is_zero(&f) || basis_size == 0)
        return f;

    int reduce_max = lv_config_get_int(LV_CFG_GROEBNER_REDUCE_MAX_STEPS, 10000);
    int step = 0;

    int max_steps = reduce_max;

    while (f.terms.count > 0 && step < max_steps) {
        /* 规范化：grlex 降序排序 + 合并同类项 + 剔除近零项，
         * 确保 terms[0] 始终为真正的领先单项式 */
        simple_poly_normalize(&f);
        if (f.terms.count == 0)
            break;
        step++;
        int reduced = 0;

        for (int b = 0; b < basis_size; b++) {
            if (simple_poly_is_zero(&basis[b]))
                continue;
            if (basis[b].terms.count == 0)
                continue;

            /* 检查基多项式 b 的首项是否能整除 f 的首项 */
            int f_vars = ((PolyTerm *)lv_darray_get(&f.terms, 0))->var_count;
            int b_vars = ((PolyTerm *)lv_darray_get(&basis[b].terms, 0))->var_count;
            int vars = (f_vars > b_vars) ? f_vars : b_vars;

            /* 首项系数为零则跳过 */
            if (fabs(((PolyTerm *)lv_darray_get(&basis[b].terms, 0))->coeff) < lv_EPSILON_DOUBLE)
                continue;

            int divisible = 1;
            for (int v = 0; v < vars && divisible; v++) {
                int fe = (v < f_vars) ? ((PolyTerm *)lv_darray_get(&f.terms, 0))->exponents[v] : 0;
                int be = (v < b_vars) ? ((PolyTerm *)lv_darray_get(&basis[b].terms, 0))->exponents[v] : 0;
                if (fe < be)
                    divisible = 0;
            }

            if (!divisible)
                continue;

            /* 执行约化：f = f - (LT(f)/LT(g)) * g */
            PolyTerm *lt_g = (PolyTerm *)lv_darray_get(&basis[b].terms, 0);
            double ratio = ((PolyTerm *)lv_darray_get(&f.terms, 0))->coeff / lt_g->coeff;

            /* 构造 (LT(f)/LT(g)) * g 并从 f 中减去；
             * 基多项式项 t 的指数 = (LT(f) - LT(g)) + exp(t) */
            for (int t = 0; t < basis[b].terms.count; t++) {
                PolyTerm *bt = (PolyTerm *)lv_darray_get(&basis[b].terms, t);
                int t_vars = bt->var_count;
                int *new_exp = NULL;
                if (vars > 0) {
                    new_exp = (int *) lv_calloc((size_t) vars, sizeof(int));
                    if (!new_exp)
                        break;
                    for (int v = 0; v < vars; v++) {
                        int fe = (v < f_vars) ? ((PolyTerm *)lv_darray_get(&f.terms, 0))->exponents[v] : 0;
                        int lg = (v < b_vars) ? lt_g->exponents[v] : 0;
                        int be = (v < t_vars) ? bt->exponents[v] : 0;
                        new_exp[v] = fe - lg + be;
                    }
                }
                /* 查找 f 中匹配的项并减去 */
                int found = 0;
                for (int k = 0; k < f.terms.count; k++) {
                    PolyTerm *fk = (PolyTerm *)lv_darray_get(&f.terms, k);
                    if (fk->var_count == vars && fk->exponents) {
                        int match = 1;
                        for (int v = 0; v < vars && match; v++) {
                            if (fk->exponents[v] != new_exp[v])
                                match = 0;
                        }
                        if (match) {
                            fk->coeff -= ratio * bt->coeff;
                            found = 1;
                            break;
                        }
                    }
                }
                if (!found && vars > 0) {
                    simple_poly_add_term(&f, -ratio * bt->coeff, new_exp, vars);
                }
                lv_free((void **) &new_exp);
            }

            /* 移除系数接近零的项 */
            for (int k = f.terms.count - 1; k >= 0; k--) {
                PolyTerm *fk = (PolyTerm *)lv_darray_get(&f.terms, k);
                if (lv_is_zero(fk->coeff, lv_EPSILON_DOUBLE)) {
                    lv_free((void **) &fk->exponents);
                    fk->exponents = NULL;
                    /* 将末尾项移到当前位置 */
                    if (k < f.terms.count - 1) {
                        PolyTerm *dst = (PolyTerm *)lv_darray_get(&f.terms, k);
                        PolyTerm *src = (PolyTerm *)lv_darray_get(&f.terms, f.terms.count - 1);
                        memcpy(dst, src, sizeof(PolyTerm));
                    }
                    f.terms.count--;
                }
            }

            reduced = 1;
            break; /* 重新从头检查 */
        }

        if (!reduced)
            break; /* 无法进一步约化 */
    }

    simple_poly_normalize(&f);
    return f;
}

/**
 * @brief 判断两个多项式的首项是否互素（Buchberger 第一个判据）
 *
 * 如果 LCM(LT(f_i), LT(f_j)) = LT(f_i) * LT(f_j)，则 S-多项式必约化为零，
 * 可以跳过该对的计算。
 */
static int coprime_leading_terms(const SimplePoly *f, int fi, int fj, int basis_size) {
    if (fi < 0 || fi >= basis_size || fj < 0 || fj >= basis_size)
        return 0;
    if (simple_poly_is_zero(&f[fi]) || simple_poly_is_zero(&f[fj]))
        return 0;

    int vars_i = ((PolyTerm *)lv_darray_get(&f[fi].terms, 0))->var_count;
    int vars_j = ((PolyTerm *)lv_darray_get(&f[fj].terms, 0))->var_count;
    int vars = (vars_i > vars_j) ? vars_i : vars_j;

    for (int v = 0; v < vars; v++) {
        int ei = (v < vars_i) ? ((PolyTerm *)lv_darray_get(&f[fi].terms, 0))->exponents[v] : 0;
        int ej = (v < vars_j) ? ((PolyTerm *)lv_darray_get(&f[fj].terms, 0))->exponents[v] : 0;
        if (ei > 0 && ej > 0)
            return 0; /* 有公共变量，不互素 */
    }
    return 1; /* 互素 */
}

/* ========================================================================
 * 工作线程函数（顺序实现，多线程框架已搭建）
 * ======================================================================== */

/**
 * @brief 工作线程主函数
 *
 * 从本地队列取 S-多项式对，计算并约化。
 * 本地队列为空时从其他线程窃取工作。
 * 发现新的非零约化结果时，生成新的 S-多项式对。
 */
static void worker_process(WorkerArg *arg) {
    SimplePoly *basis = arg->basis;
    int basis_size = arg->basis_size;
    int basis_cap = arg->basis_cap;
    SPair pair;

    while (!atomic_load(arg->shutdown_flag)) {
        int found = 0;

        /* 1. 先从本地队列取工作 */
        if (work_queue_pop(arg->local_queue, &pair) == 0) {
            found = 1;
        } else {
            /* 2. 本地队列为空，尝试从其他线程窃取 */
            for (int t = 0; t < arg->num_threads && !found; t++) {
                if (t == arg->thread_id)
                    continue;
                if (arg->all_queues[t] && work_queue_steal(arg->all_queues[t], &pair) == 0) {
                    found = 1;
                }
            }
        }

        if (!found) {
            /* 无可用工作，检查是否所有对都已完成 */
            if (atomic_load(arg->global_completed) >= atomic_load(arg->global_total)) {
                break;
            }
            continue; /* 自旋等待 */
        }

        /* 应用 Buchberger 第一个判据：跳过首项互素的对 */
        if (pair.i < basis_size && pair.j < basis_size && coprime_leading_terms(basis, pair.i, pair.j, basis_size)) {
            /* 该对必然约化为零，跳过 */
            atomic_fetch_add(arg->global_completed, 1);
            continue;
        }

        /* 计算 S-多项式 */
        SimplePoly s_poly = compute_s_polynomial(basis, pair.i, pair.j, basis_size);

        /* 对当前基约化 */
        s_poly = reduce_poly(s_poly, basis, basis_size);

        /* 更新完成计数 */
        atomic_fetch_add(arg->global_completed, 1);

        /* 如果约化结果非零，加入基并生成新的对 */
        if (!simple_poly_is_zero(&s_poly)) {
            /* 新多项式加入基 */
            int new_idx = basis_size;

            /* 扩展基数组（lv_ensure_capacity 倍增扩容，摊销 O(1)） */
            if (lv_ensure_capacity((void **) &basis, basis_size, &basis_cap, sizeof(SimplePoly), 1)) {
                basis[new_idx] = s_poly;

                /* 生成新多项式与现有基中所有多项式的 S-对 */
                for (int k = 0; k < basis_size; k++) {
                    if (!simple_poly_is_zero(&basis[k])) {
                        work_queue_push(arg->local_queue, k, new_idx);
                        atomic_fetch_add(arg->global_total, 1);
                    }
                }

                basis_size++;
                arg->basis = basis;
                arg->basis_size = basis_size;
                arg->basis_cap = basis_cap;

                /* 同步所有线程的基指针、大小与容量（当前为顺序执行框架）：
                 * realloc 可能移动基数组，后续线程若沿用 realloc 前的旧指针
                 * 会访问已释放内存（use-after-free）。统一指向最新基数组。 */
                WorkerArg *all_args = (WorkerArg *) arg->engine->thread_pool;
                if (all_args) {
                    for (int t = 0; t < arg->num_threads; t++) {
                        all_args[t].basis = basis;
                        all_args[t].basis_size = basis_size;
                        all_args[t].basis_cap = basis_cap;
                    }
                }
            } else {
                simple_poly_destroy(&s_poly);
            }
        } else {
            simple_poly_destroy(&s_poly);
        }
    }
}

/* ========================================================================
 * 公共 API
 * ======================================================================== */

lvGroebnerConfig lv_groebner_default_config(void) {
    lvGroebnerConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.max_threads = 4;
    cfg.chunk_size = 16;
    cfg.load_balance_threshold = 0.3;
    cfg.enable_inter_reduction = 1;
    cfg.enable_cache = 1;
    return cfg;
}

lvGroebnerParallel *lv_groebner_parallel_create(const lvGroebnerConfig *config) {
    lvGroebnerParallel *engine = (lvGroebnerParallel *) lv_calloc(1, sizeof(lvGroebnerParallel));
    if (!engine)
        return NULL;
    engine->config = config ? *config : lv_groebner_default_config();
    return engine;
}

void lv_groebner_parallel_destroy(lvGroebnerParallel *engine) {
    if (!engine)
        return;

    /* 释放工作队列 */
    WorkQueue *queue = (WorkQueue *) engine->pair_queue;
    if (queue) {
        work_queue_destroy(queue);
        lv_free((void **) &queue);
        engine->pair_queue = NULL;
    }
    engine->queue_size = 0;

    /* 释放线程池：信号工作线程退出并等待完成 */
    /* 注意：当前实现为顺序执行，thread_pool 存储的是 WorkerArg 数组 */
    if (engine->thread_pool) {
        WorkerArg *args = (WorkerArg *) engine->thread_pool;
        int num_threads = engine->thread_count;

        /* 设置关闭标志 */
        for (int i = 0; i < num_threads; i++) {
            if (args[i].shutdown_flag) {
                atomic_store(args[i].shutdown_flag, 1);
            }
        }

        /* 销毁每个线程的本地队列 */
        for (int i = 0; i < num_threads; i++) {
            if (args[i].local_queue) {
                work_queue_destroy(args[i].local_queue);
                lv_free((void **) &args[i].local_queue);
                args[i].local_queue = NULL;
            }
            if (args[i].all_queues) {
                lv_free((void **) &args[i].all_queues);
                args[i].all_queues = NULL;
            }
            /* 释放关闭标志 */
            if (args[i].shutdown_flag) {
                lv_free((void **) &args[i].shutdown_flag);
                args[i].shutdown_flag = NULL;
            }
            /* 释放全局计数器（仅释放第一个线程拥有的） */
            if (i == 0) {
                if (args[i].global_completed) {
                    lv_free((void **) &args[i].global_completed);
                    args[i].global_completed = NULL;
                }
                if (args[i].global_total) {
                    lv_free((void **) &args[i].global_total);
                    args[i].global_total = NULL;
                }
            }
        }

        lv_free((void **) &args);
        engine->thread_pool = NULL;
        engine->thread_count = 0;
    }

    /* 释放 Groebner 基结果。
     * groebner_basis 为指向 SimplePoly 的指针数组（void **）：
     * 元素 i 指向第 i 个基多项式；ptrs[0] 同时是多项式数组的起始地址。 */
    if (engine->groebner_basis) {
        void **ptrs = (void **) engine->groebner_basis;
        for (int i = 0; i < engine->basis_size; i++) {
            if (ptrs[i])
                simple_poly_destroy((SimplePoly *) ptrs[i]);
        }
        if (engine->basis_size > 0 && ptrs[0])
            lv_free((void **) &ptrs[0]);
        lv_free((void **) &ptrs);
        engine->groebner_basis = NULL;
    }
    engine->basis_size = 0;

    /* 重置状态 */
    memset(&engine->state, 0, sizeof(engine->state));

    lv_free((void **) &engine);
}

int lv_groebner_parallel_compute(lvGroebnerParallel *engine, void *polynomials, int poly_count) {
    if (!engine || !polynomials || poly_count <= 0)
        return -1;

    int **clauses = (int **) polynomials;

    /* 推断布尔变量数 n：所有子句中 |文字| 的最大值（至少 1） */
    int var_count = 1;
    for (int i = 0; i < poly_count; i++) {
        if (!clauses[i])
            continue;
        for (int j = 0; clauses[i][j] != 0; j++) {
            int lit = clauses[i][j];
            int vid = (lit < 0) ? -lit : lit;
            if (vid > var_count)
                var_count = vid;
        }
    }

    /* 忠实布尔 Groebner 编码（Q[x1..xn]，n = var_count）：
     *  - 变量 xi（i ∈ [1, n]），布尔公理 xi^2 - xi = 0 显式加入基
     *  - 子句 (l1 v ... v lk) -> 多项式 ∏_j (1 - lj')，其中
     *      正文字 lj = xi   -> lj' = xi，因子 (1 - xi)
     *      负文字 lj = ¬xi  -> lj' = 1 - xi，因子 xi
     *    子句多项式 = 0 <=> 至少一个因子为 0 <=> 至少一个文字为真
     *  - UNSAT <=> 约化 Groebner 基含非零常数（即 1）
     * 每个项使用完整的 n 维指数向量，按 grlex 降序存储（首项为领先单项式），
     * 系数为整数 ±1（double 可精确表示）。 */
    int basis_count = poly_count + var_count;
    SimplePoly *basis = (SimplePoly *) lv_calloc((size_t) basis_count, sizeof(SimplePoly));
    if (!basis)
        return -1;

    /* 初始化状态 */
    engine->state.total_pairs = basis_count * (basis_count - 1) / 2;
    engine->state.completed_pairs = 0;
    engine->state.remaining_pairs = engine->state.total_pairs;
    engine->state.active_threads = engine->config.max_threads;

    /* 构建子句多项式：∏_j (1 - lj')（空子句 -> 常数 1 -> 直接 UNSAT） */
    for (int i = 0; i < poly_count; i++) {
        if (!simple_poly_create(&basis[i], 8)) {
            /* 创建失败，清理并返回 */
            for (int k = 0; k < i; k++)
                simple_poly_destroy(&basis[k]);
            lv_free((void **) &basis);
            return -1;
        }
        /* 常数项 1（零指数向量，var_count 维） */
        int *zero_exp = (int *) lv_calloc((size_t) var_count, sizeof(int));
        if (!zero_exp) {
            for (int k = 0; k <= i; k++)
                simple_poly_destroy(&basis[k]);
            lv_free((void **) &basis);
            return -1;
        }
        simple_poly_add_term(&basis[i], 1.0, zero_exp, var_count);
        lv_free((void **) &zero_exp);

        if (clauses[i]) {
            for (int j = 0; clauses[i][j] != 0; j++) {
                int lit = clauses[i][j];
                int vid = (lit < 0) ? -lit : lit;
                int rc;
                if (lit > 0)
                    rc = simple_poly_mul_binomial(&basis[i], 1.0, -1.0, vid - 1); /* 因子 (1 - xi) */
                else
                    rc = simple_poly_mul_binomial(&basis[i], 0.0, 1.0, vid - 1);  /* 因子 xi */
                if (rc != 0) {
                    for (int k = 0; k <= i; k++)
                        simple_poly_destroy(&basis[k]);
                    lv_free((void **) &basis);
                    return -1;
                }
                simple_poly_normalize(&basis[i]);
            }
        }
        simple_poly_normalize(&basis[i]);
    }

    /* 布尔公理 xi^2 - xi = 0（i ∈ [1, var_count]） */
    for (int v = 0; v < var_count; v++) {
        int idx = poly_count + v;
        if (!simple_poly_create(&basis[idx], 2)) {
            for (int k = 0; k < idx; k++)
                simple_poly_destroy(&basis[k]);
            lv_free((void **) &basis);
            return -1;
        }
        int *e = (int *) lv_calloc((size_t) var_count, sizeof(int));
        if (!e) {
            for (int k = 0; k < idx; k++)
                simple_poly_destroy(&basis[k]);
            lv_free((void **) &basis);
            return -1;
        }
        e[v] = 2;
        simple_poly_add_term(&basis[idx], 1.0, e, var_count);
        e[v] = 1;
        simple_poly_add_term(&basis[idx], -1.0, e, var_count);
        lv_free((void **) &e);
        simple_poly_normalize(&basis[idx]);
    }

    /* 初始化工作队列和线程参数 */
    int num_threads = engine->config.max_threads;
    if (num_threads < 1)
        num_threads = 1;
    if (num_threads > basis_count)
        num_threads = basis_count;

    /* C11 原子计数器：当前为单线程顺序执行框架；多线程扩展后由多个
     * worker_process 并发访问，volatile 不足以保证原子性与可见性，
     * 故使用 stdatomic（原子类型保持与原 volatile int 相同布局）。 */
    atomic_int shutdown_flag = 0;
    atomic_int global_completed = 0;
    atomic_int global_total = engine->state.total_pairs;

    WorkerArg *args = (WorkerArg *) lv_calloc((size_t) num_threads, sizeof(WorkerArg));
    WorkQueue **all_queues = (WorkQueue **) lv_calloc((size_t) num_threads, sizeof(WorkQueue *));
    if (!args || !all_queues) {
        for (int i = 0; i < basis_count; i++)
            simple_poly_destroy(&basis[i]);
        lv_free((void **) &basis);
        lv_free((void **) &args);
        lv_free((void **) &all_queues);
        return -1;
    }

    /* 创建每个线程的本地工作队列 */
    for (int t = 0; t < num_threads; t++) {
        all_queues[t] = (WorkQueue *) lv_calloc(1, sizeof(WorkQueue));
        if (!all_queues[t] || work_queue_init(all_queues[t], engine->config.chunk_size) != 0) {
            /* 清理已分配的队列 */
            for (int k = 0; k < t; k++) {
                work_queue_destroy(all_queues[k]);
                lv_free((void **) &all_queues[k]);
            }
            if (all_queues[t])
                lv_free((void **) &all_queues[t]);
            /* 清理基和其他资源 */
            for (int i = 0; i < basis_count; i++)
                simple_poly_destroy(&basis[i]);
            lv_free((void **) &basis);
            lv_free((void **) &args);
            return -1;
        }
    }

    /* 将初始 S-多项式对分配到各线程的本地队列 */
    int pair_idx = 0;
    for (int i = 0; i < basis_count; i++) {
        for (int j = i + 1; j < basis_count; j++) {
            int target = pair_idx % num_threads;
            work_queue_push(all_queues[target], i, j);
            pair_idx++;
        }
    }

    /* 初始化线程参数 */
    for (int t = 0; t < num_threads; t++) {
        args[t].thread_id = t;
        args[t].engine = engine;
        args[t].basis = basis;
        args[t].basis_size = basis_count;
        args[t].basis_cap = basis_count;
        args[t].local_queue = all_queues[t];
        args[t].all_queues = all_queues;
        args[t].num_threads = num_threads;
        args[t].shutdown_flag = &shutdown_flag;
        args[t].global_completed = &global_completed;
        args[t].global_total = &global_total;
    }

    /* 存储线程参数到引擎（用于 destroy 时清理） */
    engine->thread_pool = args;
    engine->thread_count = num_threads;

    /* 顺序执行所有线程的工作（当前为单线程并行框架：
     * shutdown_flag/global_completed/global_total 已用 C11 原子，多线程扩展时
     * 只需将 worker_process 包装为线程创建调用；注意 basis/basis_size/basis_cap
     * 的同步（realloc 移动基数组）需升级为互斥保护或原子指针交换，否则有
     * use-after-free 风险。 */
    for (int t = 0; t < num_threads; t++) {
        worker_process(&args[t]);
    }

    /* 通知所有线程退出 */
    atomic_store(&shutdown_flag, 1);

    /* 更新引擎状态 */
    engine->state.completed_pairs = atomic_load(&global_completed);
    engine->state.total_pairs = atomic_load(&global_total);
    engine->state.remaining_pairs = atomic_load(&global_total) - atomic_load(&global_completed);
    engine->state.active_threads = 0;

    /* 保存最终基 */
    int final_basis_size = args[0].basis_size;
    SimplePoly *final_basis = args[0].basis;

    /* 构建结果指针数组：groebner_basis[i] 指向第 i 个基多项式（SimplePoly*）。
     * 注意：groebner_basis 声明类型为 void **（元素为指向多项式的指针），
     * 若直接强转多项式数组为 void **，读取 groebner_basis[i] 会按指针大小
     * 步进读取 SimplePoly 结构体字节（错位缺陷）。 */
    void **result_ptrs = (void **) lv_calloc((size_t) final_basis_size, sizeof(void *));
    if (!result_ptrs) {
        for (int i = 0; i < final_basis_size; i++)
            simple_poly_destroy(&final_basis[i]);
        lv_free((void **) &final_basis);
        lv_free((void **) &args);
        for (int t = 0; t < num_threads; t++) {
            if (all_queues[t]) {
                work_queue_destroy(all_queues[t]);
                lv_free((void **) &all_queues[t]);
            }
        }
        lv_free((void **) &all_queues);
        engine->thread_pool = NULL;
        engine->thread_count = 0;
        return -1;
    }
    for (int i = 0; i < final_basis_size; i++)
        result_ptrs[i] = &final_basis[i];

    /* 清理旧的基结果 */
    if (engine->groebner_basis) {
        void **old_ptrs = (void **) engine->groebner_basis;
        /* 旧表示：groebner_basis[0] 即多项式数组起始地址 */
        if (engine->basis_size > 0 && old_ptrs[0])
            lv_free((void **) &old_ptrs[0]);
        lv_free((void **) &old_ptrs);
    }

    engine->groebner_basis = result_ptrs;
    engine->basis_size = final_basis_size;

    /* 清理线程参数中的队列引用。
     * 注意：local_queue 指向的 WorkQueue 对象及其 pairs 数组保留给
     * destroy 经 args[i].local_queue 释放（此处不可置 NULL，否则泄漏）；
     * all_queues 仅是指针数组，已随本函数末尾 lv_free 释放，故置 NULL。 */
    for (int t = 0; t < num_threads; t++) {
        args[t].all_queues = NULL;
        args[t].shutdown_flag = NULL;
        args[t].global_completed = NULL;
        args[t].global_total = NULL;
    }

    /* 释放 all_queues 数组（不释放队列对象本身） */
    lv_free((void **) &all_queues);

    return 0;
}

lvGroebnerState lv_groebner_parallel_state(const lvGroebnerParallel *engine) {
    lvGroebnerState state = {0};
    if (engine)
        state = engine->state;
    return state;
}

bool lv_groebner_poly_is_nonzero_constant(void *poly) {
    if (!poly)
        return false;
    SimplePoly *p = (SimplePoly *) poly;
    if (p->terms.count != 1)
        return false;
    PolyTerm *pt0 = (PolyTerm *)lv_darray_get(&p->terms, 0);
    if (lv_is_zero(pt0->coeff, lv_EPSILON_DOUBLE))
        return false;
    if (pt0->var_count > 0 && pt0->exponents) {
        for (int i = 0; i < pt0->var_count; i++) {
            if (pt0->exponents[i] != 0)
                return false;
        }
    }
    return true;
}

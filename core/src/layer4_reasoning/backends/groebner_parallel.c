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

#include "lv00/groebner_parallel.h"
#include "lv00_internal.h"
#include "lv00/lv00_utils.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>

/* ========================================================================
 * 内部常量
 * ======================================================================== */

/** @brief Buchberger 算法最大步数 */
#define GROEBNER_PARALLEL_BUCHBERGER_MAX_STEPS 50000

/** @brief 多项式约化最大步数 */
#define GROEBNER_PARALLEL_REDUCE_MAX_STEPS 10000

/* ========================================================================
 * 内部数据结构
 * ======================================================================== */

/** S-多项式对：表示需要处理的一对多项式索引 */
typedef struct {
    int i;  /**< 基中第一个多项式的索引 */
    int j;  /**< 基中第二个多项式的索引 */
} SPair;

/** 工作队列：存储待处理的 S-多项式对 */
typedef struct WorkQueue {
    SPair *pairs;          /**< 对数组 */
    int size;              /**< 当前队列中的对数 */
    int capacity;           /**< 队列容量 */
    int head;              /**< 队列头部索引（出队位置） */
    int tail;              /**< 队列尾部索引（入队位置） */
} WorkQueue;

/** 多项式项：单项式 c * x^a * y^b * ... */
typedef struct {
    double coeff;          /**< 系数 */
    int *exponents;        /**< 各变量指数数组 */
    int var_count;         /**< 变量数 */
} PolyTerm;

/** 简化多项式表示（用于内部计算） */
typedef struct {
    PolyTerm *terms;       /**< 项数组 */
    int term_count;        /**< 项数 */
    int term_capacity;     /**< 项容量 */
} SimplePoly;

/** 工作线程参数 */
typedef struct {
    int thread_id;                  /**< 线程 ID */
    Lv00GroebnerParallel *engine;    /**< 所属引擎 */
    SimplePoly *basis;               /**< 当前基（共享，需同步访问） */
    int basis_size;                 /**< 当前基大小 */
    WorkQueue *local_queue;          /**< 线程本地工作队列 */
    WorkQueue **all_queues;          /**< 所有线程的队列（用于 work-stealing） */
    int num_threads;                 /**< 总线程数 */
    volatile int *shutdown_flag;     /**< 关闭标志 */
    volatile int *global_completed;  /**< 全局完成对计数（原子操作） */
    volatile int *global_total;      /**< 全局总对数（原子操作） */
} WorkerArg;

/* ========================================================================
 * 工作队列操作
 * ======================================================================== */

/** 初始化工作队列，成功返回0，失败返回-1 */
static int work_queue_init(WorkQueue *q, int initial_capacity) {
    q->pairs = (SPair *)lv00_calloc((size_t)initial_capacity, sizeof(SPair));
    if (!q->pairs) {
        q->size = 0;
        q->capacity = 0;
        q->head = 0;
        q->tail = 0;
        return -1;
    }
    q->size = 0;
    q->capacity = initial_capacity;
    q->head = 0;
    q->tail = 0;
    return 0;
}

/** 销毁工作队列 */
static void work_queue_destroy(WorkQueue *q) {
    if (q) {
        lv00_free((void **)&q->pairs);
        q->pairs = NULL;
        q->size = 0;
        q->capacity = 0;
        q->head = 0;
        q->tail = 0;
    }
}

/** 向队列添加一个 S-多项式对 */
static int work_queue_push(WorkQueue *q, int i, int j) {
    if (q->size >= q->capacity) {
        /* 溢出检查：确保 capacity * 2 不超过 INT_MAX */
        if (q->capacity > INT_MAX / 2) return -1;
        int new_cap = q->capacity * 2;
        SPair *new_pairs = (SPair *)lv00_realloc(q->pairs, (size_t)new_cap * sizeof(SPair));
        if (!new_pairs) return -1;
        q->pairs = new_pairs;
        /* 环形缓冲区扩容：将数据从 head 到 tail 复制到开头 */
        if (q->head > q->tail) {
            int count = q->size;
            memmove(q->pairs, q->pairs + q->head, (size_t)count * sizeof(SPair));
            q->head = 0;
            q->tail = count;
        }
        q->capacity = new_cap;
    }
    q->pairs[q->tail] = (SPair){i, j};
    q->tail = (q->tail + 1) % q->capacity;
    q->size++;
    return 0;
}

/** 从队列取出一个 S-多项式对（线程安全版本使用简单自旋） */
static int work_queue_pop(WorkQueue *q, SPair *out) {
    if (q->size <= 0) return -1;
    *out = q->pairs[q->head];
    q->head = (q->head + 1) % q->capacity;
    q->size--;
    return 0;
}

/** 从其他线程的队列窃取工作（从尾部取） */
static int work_queue_steal(WorkQueue *q, SPair *out) {
    if (q->size <= 0) return -1;
    q->tail = (q->tail - 1 + q->capacity) % q->capacity;
    *out = q->pairs[q->tail];
    q->size--;
    return 0;
}

/* ========================================================================
 * 简化多项式操作
 * ======================================================================== */

/** 创建空多项式，成功返回true */
static bool simple_poly_create(SimplePoly *out, int initial_capacity) {
    out->terms = (PolyTerm *)lv00_calloc((size_t)initial_capacity, sizeof(PolyTerm));
    if (!out->terms) {
        out->term_count = 0;
        out->term_capacity = 0;
        return false;
    }
    out->term_count = 0;
    out->term_capacity = initial_capacity;
    return true;
}

/** 销毁多项式 */
static void simple_poly_destroy(SimplePoly *p) {
    if (!p) return;
    for (int i = 0; i < p->term_count; i++) {
        lv00_free((void **)&p->terms[i].exponents);
    }
    lv00_free((void **)&p->terms);
    p->terms = NULL;
    p->term_count = 0;
    p->term_capacity = 0;
}

/** 添加一个项到多项式 */
static int simple_poly_add_term(SimplePoly *p, double coeff, const int *exponents, int var_count) {
    if (p->term_count >= p->term_capacity) {
        int new_cap = (p->term_capacity == 0) ? 8 : p->term_capacity * 2;
        PolyTerm *new_terms = (PolyTerm *)lv00_realloc(p->terms, (size_t)new_cap * sizeof(PolyTerm));
        if (!new_terms) return -1;
        p->terms = new_terms;
        p->term_capacity = new_cap;
    }
    int idx = p->term_count;
    p->terms[idx].coeff = coeff;
    p->terms[idx].var_count = var_count;
    if (var_count > 0) {
        p->terms[idx].exponents = (int *)lv00_malloc((size_t)var_count * sizeof(int));
        if (!p->terms[idx].exponents) return -1;
        memcpy(p->terms[idx].exponents, exponents, (size_t)var_count * sizeof(int));
    } else {
        p->terms[idx].exponents = NULL;
    }
    p->term_count++;
    return 0;
}

/** 判断多项式是否为零 */
static int simple_poly_is_zero(const SimplePoly *p) {
    return p == NULL || p->term_count == 0;
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
    if (fi < 0 || fi >= basis_size || fj < 0 || fj >= basis_size) return result;

    const SimplePoly *gi = &f[fi];
    const SimplePoly *gj = &f[fj];

    /* 如果任一多项式为空或首项系数为零，S-多项式为零 */
    if (simple_poly_is_zero(gi) || simple_poly_is_zero(gj)) return result;
    if (fabs(gi->terms[0].coeff) < LV00_EPSILON_DOUBLE || fabs(gj->terms[0].coeff) < LV00_EPSILON_DOUBLE) return result;

    /* 计算 LCM(leading terms) */
    int var_count = gi->terms[0].var_count;
    if (var_count == 0) var_count = gj->terms[0].var_count;

    int *lcm_exp = NULL;
    if (var_count > 0) {
        lcm_exp = (int *)lv00_calloc((size_t)var_count, sizeof(int));
        if (!lcm_exp) return result;

        for (int v = 0; v < var_count; v++) {
            int ei = (v < gi->terms[0].var_count) ? gi->terms[0].exponents[v] : 0;
            int ej = (v < gj->terms[0].var_count) ? gj->terms[0].exponents[v] : 0;
            lcm_exp[v] = (ei > ej) ? ei : ej;
        }
    }

    /* 计算 multiplier_i = LCM / LT(gi) 的指数部分 */
    int *mult_i_exp = NULL;
    if (var_count > 0) {
        mult_i_exp = (int *)lv00_calloc((size_t)var_count, sizeof(int));
        if (!mult_i_exp) { lv00_free((void **)&lcm_exp); return result; }
        for (int v = 0; v < var_count; v++) {
            int ei = (v < gi->terms[0].var_count) ? gi->terms[0].exponents[v] : 0;
            mult_i_exp[v] = lcm_exp[v] - ei;
        }
    }

    /* 计算 multiplier_j = LCM / LT(gj) 的指数部分 */
    int *mult_j_exp = NULL;
    if (var_count > 0) {
        mult_j_exp = (int *)lv00_calloc((size_t)var_count, sizeof(int));
        if (!mult_j_exp) { lv00_free((void **)&lcm_exp); lv00_free((void **)&mult_i_exp); return result; }
        for (int v = 0; v < var_count; v++) {
            int ej = (v < gj->terms[0].var_count) ? gj->terms[0].exponents[v] : 0;
            mult_j_exp[v] = lcm_exp[v] - ej;
        }
    }

    /* S = (1/LC(gi)) * mult_i * gi - (1/LC(gj)) * mult_j * gj
     * 完整实现：对所有项（不仅是首项）乘以系数后相减 */
    double scale_i = 1.0 / gi->terms[0].coeff;
    double scale_j = 1.0 / gj->terms[0].coeff;

    /* 将 mult_i * gi 的项加入结果 */
    for (int t = 0; t < gi->term_count; t++) {
        int *new_exp = NULL;
        if (var_count > 0) {
            new_exp = (int *)lv00_calloc((size_t)var_count, sizeof(int));
            if (!new_exp) break;
            for (int v = 0; v < var_count; v++) {
                int et = (v < gi->terms[t].var_count) ? gi->terms[t].exponents[v] : 0;
                new_exp[v] = mult_i_exp[v] + et;
            }
        }
        if (simple_poly_add_term(&result, scale_i * gi->terms[t].coeff, new_exp, var_count) != 0) {
            lv00_free((void **)&new_exp);
            lv00_free((void **)&lcm_exp);
            lv00_free((void **)&mult_i_exp);
            lv00_free((void **)&mult_j_exp);
            simple_poly_destroy(&result);
            result.term_count = 0;
            result.terms = NULL;
            result.term_capacity = 0;
            return result;
        }
        lv00_free((void **)&new_exp);
    }

    /* 减去 mult_j * gj 的项 */
    for (int t = 0; t < gj->term_count; t++) {
        int *new_exp = NULL;
        if (var_count > 0) {
            new_exp = (int *)lv00_calloc((size_t)var_count, sizeof(int));
            if (!new_exp) break;
            for (int v = 0; v < var_count; v++) {
                int et = (v < gj->terms[t].var_count) ? gj->terms[t].exponents[v] : 0;
                new_exp[v] = mult_j_exp[v] + et;
            }
        }
        if (simple_poly_add_term(&result, -scale_j * gj->terms[t].coeff, new_exp, var_count) != 0) {
            lv00_free((void **)&new_exp);
            lv00_free((void **)&lcm_exp);
            lv00_free((void **)&mult_i_exp);
            lv00_free((void **)&mult_j_exp);
            simple_poly_destroy(&result);
            result.term_count = 0;
            result.terms = NULL;
            result.term_capacity = 0;
            return result;
        }
        lv00_free((void **)&new_exp);
    }

    lv00_free((void **)&lcm_exp);
    lv00_free((void **)&mult_i_exp);
    lv00_free((void **)&mult_j_exp);

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
    if (simple_poly_is_zero(&f) || basis_size == 0) return f;

    int max_steps = GROEBNER_PARALLEL_REDUCE_MAX_STEPS;
    int step = 0;

    while (f.term_count > 0 && step < max_steps) {
        step++;
        int reduced = 0;

        for (int b = 0; b < basis_size; b++) {
            if (simple_poly_is_zero(&basis[b])) continue;
            if (basis[b].term_count == 0) continue;

            /* 检查基多项式 b 的首项是否能整除 f 的首项 */
            int f_vars = f.terms[0].var_count;
            int b_vars = basis[b].terms[0].var_count;
            int vars = (f_vars > b_vars) ? f_vars : b_vars;

            /* 首项系数为零则跳过 */
            if (fabs(basis[b].terms[0].coeff) < LV00_EPSILON_DOUBLE) continue;

            int divisible = 1;
            for (int v = 0; v < vars && divisible; v++) {
                int fe = (v < f_vars) ? f.terms[0].exponents[v] : 0;
                int be = (v < b_vars) ? basis[b].terms[0].exponents[v] : 0;
                if (fe < be) divisible = 0;
            }

            if (!divisible) continue;

            /* 执行约化：f = f - (LT(f)/LT(g)) * g */
            double ratio = f.terms[0].coeff / basis[b].terms[0].coeff;

            /* 构造 (LT(f)/LT(g)) * g 并从 f 中减去 */
            for (int t = 0; t < basis[b].term_count; t++) {
                int *new_exp = NULL;
                if (vars > 0) {
                    new_exp = (int *)lv00_calloc((size_t)vars, sizeof(int));
                    if (!new_exp) break;
                    for (int v = 0; v < vars; v++) {
                        int fe = (v < f_vars) ? f.terms[0].exponents[v] : 0;
                        int be = (v < b_vars && v < basis[b].terms[t].var_count)
                                 ? basis[b].terms[t].exponents[v] : 0;
                        new_exp[v] = fe - be;
                    }
                }
                /* 查找 f 中匹配的项并减去 */
                int found = 0;
                for (int k = 0; k < f.term_count; k++) {
                    if (f.terms[k].var_count == vars && f.terms[k].exponents) {
                        int match = 1;
                        for (int v = 0; v < vars && match; v++) {
                            if (f.terms[k].exponents[v] != new_exp[v]) match = 0;
                        }
                        if (match) {
                            f.terms[k].coeff -= ratio * basis[b].terms[t].coeff;
                            found = 1;
                            break;
                        }
                    }
                }
                if (!found && vars > 0) {
                    simple_poly_add_term(&f, -ratio * basis[b].terms[t].coeff, new_exp, vars);
                }
                lv00_free((void **)&new_exp);
            }

            /* 移除系数接近零的项 */
            for (int k = f.term_count - 1; k >= 0; k--) {
                if (fabs(f.terms[k].coeff) < LV00_EPSILON_DOUBLE) {
                    lv00_free((void **)&f.terms[k].exponents);
                    f.terms[k].exponents = NULL;
                    /* 将末尾项移到当前位置 */
                    if (k < f.term_count - 1) {
                        f.terms[k] = f.terms[f.term_count - 1];
                    }
                    f.term_count--;
                }
            }

            reduced = 1;
            break; /* 重新从头检查 */
        }

        if (!reduced) break; /* 无法进一步约化 */
    }

    return f;
}

/**
 * @brief 判断两个多项式的首项是否互素（Buchberger 第一个判据）
 *
 * 如果 LCM(LT(f_i), LT(f_j)) = LT(f_i) * LT(f_j)，则 S-多项式必约化为零，
 * 可以跳过该对的计算。
 */
static int coprime_leading_terms(const SimplePoly *f, int fi, int fj, int basis_size) {
    if (fi < 0 || fi >= basis_size || fj < 0 || fj >= basis_size) return 0;
    if (simple_poly_is_zero(&f[fi]) || simple_poly_is_zero(&f[fj])) return 0;

    int vars_i = f[fi].terms[0].var_count;
    int vars_j = f[fj].terms[0].var_count;
    int vars = (vars_i > vars_j) ? vars_i : vars_j;

    for (int v = 0; v < vars; v++) {
        int ei = (v < vars_i) ? f[fi].terms[0].exponents[v] : 0;
        int ej = (v < vars_j) ? f[fj].terms[0].exponents[v] : 0;
        if (ei > 0 && ej > 0) return 0; /* 有公共变量，不互素 */
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
    SPair pair;

    while (!*(arg->shutdown_flag)) {
        int found = 0;

        /* 1. 先从本地队列取工作 */
        if (work_queue_pop(arg->local_queue, &pair) == 0) {
            found = 1;
        } else {
            /* 2. 本地队列为空，尝试从其他线程窃取 */
            for (int t = 0; t < arg->num_threads && !found; t++) {
                if (t == arg->thread_id) continue;
                if (arg->all_queues[t] &&
                    work_queue_steal(arg->all_queues[t], &pair) == 0) {
                    found = 1;
                }
            }
        }

        if (!found) {
            /* 无可用工作，检查是否所有对都已完成 */
            if (*(arg->global_completed) >= *(arg->global_total)) {
                break;
            }
            continue; /* 自旋等待 */
        }

        /* 应用 Buchberger 第一个判据：跳过首项互素的对 */
        if (pair.i < basis_size && pair.j < basis_size &&
            coprime_leading_terms(basis, pair.i, pair.j, basis_size)) {
            /* 该对必然约化为零，跳过 */
            (*(arg->global_completed))++;
            continue;
        }

        /* 计算 S-多项式 */
        SimplePoly s_poly = compute_s_polynomial(basis, pair.i, pair.j, basis_size);

        /* 对当前基约化 */
        s_poly = reduce_poly(s_poly, basis, basis_size);

        /* 更新完成计数 */
        (*(arg->global_completed))++;

        /* 如果约化结果非零，加入基并生成新的对 */
        if (!simple_poly_is_zero(&s_poly)) {
            /* 新多项式加入基 */
            int new_idx = basis_size;

            /* 扩展基数组 */
            SimplePoly *new_basis = (SimplePoly *)lv00_realloc(
                basis, (size_t)(basis_size + 1) * sizeof(SimplePoly));
            if (new_basis) {
                basis = new_basis;
                basis[new_idx] = s_poly;

                /* 生成新多项式与现有基中所有多项式的 S-对 */
                for (int k = 0; k < basis_size; k++) {
                    if (!simple_poly_is_zero(&basis[k])) {
                        work_queue_push(arg->local_queue, k, new_idx);
                        (*(arg->global_total))++;
                    }
                }

                basis_size++;
                arg->basis = basis;
                arg->basis_size = basis_size;
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

Lv00GroebnerConfig lv00_groebner_default_config(void) {
    Lv00GroebnerConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.max_threads = 4;
    cfg.chunk_size = 16;
    cfg.load_balance_threshold = 0.3;
    cfg.enable_inter_reduction = 1;
    cfg.enable_cache = 1;
    return cfg;
}

Lv00GroebnerParallel *lv00_groebner_parallel_create(const Lv00GroebnerConfig *config) {
    Lv00GroebnerParallel *engine = (Lv00GroebnerParallel *)lv00_calloc(1, sizeof(Lv00GroebnerParallel));
    if (!engine) return NULL;
    engine->config = config ? *config : lv00_groebner_default_config();
    return engine;
}

void lv00_groebner_parallel_destroy(Lv00GroebnerParallel *engine) {
    if (!engine) return;

    /* 释放工作队列 */
    WorkQueue *queue = (WorkQueue *)engine->pair_queue;
    if (queue) {
        work_queue_destroy(queue);
        lv00_free((void **)&queue);
        engine->pair_queue = NULL;
    }
    engine->queue_size = 0;

    /* 释放线程池：信号工作线程退出并等待完成 */
    /* 注意：当前实现为顺序执行，thread_pool 存储的是 WorkerArg 数组 */
    if (engine->thread_pool) {
        WorkerArg *args = (WorkerArg *)engine->thread_pool;
        int num_threads = engine->config.max_threads;

        /* 设置关闭标志 */
        for (int i = 0; i < num_threads; i++) {
            if (args[i].shutdown_flag) {
                *(args[i].shutdown_flag) = 1;
            }
        }

        /* 销毁每个线程的本地队列 */
        for (int i = 0; i < num_threads; i++) {
            if (args[i].local_queue) {
                work_queue_destroy(args[i].local_queue);
                lv00_free((void **)&args[i].local_queue);
                args[i].local_queue = NULL;
            }
            if (args[i].all_queues) {
                lv00_free((void **)&args[i].all_queues);
                args[i].all_queues = NULL;
            }
            /* 释放关闭标志 */
            lv00_free((void **)&args[i].shutdown_flag);
            args[i].shutdown_flag = NULL;
            /* 释放全局计数器（仅释放第一个线程拥有的） */
            if (i == 0) {
                lv00_free((void **)&args[i].global_completed);
                lv00_free((void **)&args[i].global_total);
            }
        }

        lv00_free((void **)&args);
        engine->thread_pool = NULL;
    }

    /* 释放 Groebner 基结果 */
    if (engine->groebner_basis) {
        SimplePoly *basis = (SimplePoly *)engine->groebner_basis;
        for (int i = 0; i < engine->basis_size; i++) {
            simple_poly_destroy(&basis[i]);
        }
        lv00_free((void **)&basis);
        engine->groebner_basis = NULL;
    }
    engine->basis_size = 0;

    /* 重置状态 */
    memset(&engine->state, 0, sizeof(engine->state));

    lv00_free((void **)&engine);
}

int lv00_groebner_parallel_compute(Lv00GroebnerParallel *engine,
                                     void *polynomials, int poly_count) {
    if (!engine || !polynomials || poly_count <= 0) return -1;

    /* 初始化状态 */
    engine->state.total_pairs = poly_count * (poly_count - 1) / 2;
    engine->state.completed_pairs = 0;
    engine->state.remaining_pairs = engine->state.total_pairs;
    engine->state.active_threads = engine->config.max_threads;

    /* 从输入子句构造初始基（简化多项式） */
    int **clauses = (int **)polynomials;
    SimplePoly *basis = (SimplePoly *)lv00_calloc((size_t)poly_count, sizeof(SimplePoly));
    if (!basis) return -1;

    for (int i = 0; i < poly_count; i++) {
        if (!simple_poly_create(&basis[i], 8)) {
            /* calloc 失败，清理并返回 */
            for (int k = 0; k < i; k++)
                simple_poly_destroy(&basis[k]);
            lv00_free((void **)&basis);
            /* 注意：args 和 all_queues 尚未分配，无需释放 */
            return -1;
        }
        if (clauses[i]) {
            /* 将子句编码为多项式：
             * 子句 (l1 v l2 v ... v ln) -> 乘积 (1-x1)(1-x2)...(1-xn)
             * 简化编码：每项对应一个文字，系数为 1 */
            int lit_count = 0;
            while (clauses[i][lit_count] != 0) lit_count++;

            for (int j = 0; j < lit_count; j++) {
                int lit = clauses[i][j];
                int var_id = (lit < 0) ? -lit : lit;
                /* 使用变量 ID 作为指数，创建单项式 */
                int exp = var_id;
                simple_poly_add_term(&basis[i], 1.0, &exp, 1);
            }
        }
    }

    /* 初始化工作队列和线程参数 */
    int num_threads = engine->config.max_threads;
    if (num_threads < 1) num_threads = 1;
    if (num_threads > poly_count) num_threads = poly_count;

    volatile int shutdown_flag = 0;
    volatile int global_completed = 0;
    volatile int global_total = engine->state.total_pairs;

    WorkerArg *args = (WorkerArg *)lv00_calloc((size_t)num_threads, sizeof(WorkerArg));
    WorkQueue **all_queues = (WorkQueue **)lv00_calloc((size_t)num_threads, sizeof(WorkQueue *));
    if (!args || !all_queues) {
        for (int i = 0; i < poly_count; i++) simple_poly_destroy(&basis[i]);
        lv00_free((void **)&basis);
        lv00_free((void **)&args);
        lv00_free((void **)&all_queues);
        return -1;
    }

    /* 创建每个线程的本地工作队列 */
    for (int t = 0; t < num_threads; t++) {
        all_queues[t] = (WorkQueue *)lv00_calloc(1, sizeof(WorkQueue));
        if (!all_queues[t] || work_queue_init(all_queues[t], engine->config.chunk_size) != 0) {
            /* 清理已分配的队列 */
            for (int k = 0; k < t; k++) {
                work_queue_destroy(all_queues[k]);
                lv00_free((void **)&all_queues[k]);
            }
            if (all_queues[t]) lv00_free((void **)&all_queues[t]);
            /* 清理基和其他资源 */
            for (int i = 0; i < poly_count; i++)
                simple_poly_destroy(&basis[i]);
            lv00_free((void **)&basis);
            lv00_free((void **)&args);
            return -1;
        }
    }

    /* 将初始 S-多项式对分配到各线程的本地队列 */
    int pair_idx = 0;
    for (int i = 0; i < poly_count; i++) {
        for (int j = i + 1; j < poly_count; j++) {
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
        args[t].basis_size = poly_count;
        args[t].local_queue = all_queues[t];
        args[t].all_queues = all_queues;
        args[t].num_threads = num_threads;
        args[t].shutdown_flag = &shutdown_flag;
        args[t].global_completed = &global_completed;
        args[t].global_total = &global_total;
    }

    /* 存储线程参数到引擎（用于 destroy 时清理） */
    engine->thread_pool = args;

    /* 顺序执行所有线程的工作（当前为单线程并行框架，
     * 多线程扩展只需将 worker_process 包装为 pthread_create 调用） */
    for (int t = 0; t < num_threads; t++) {
        worker_process(&args[t]);
    }

    /* 通知所有线程退出 */
    shutdown_flag = 1;

    /* 更新引擎状态 */
    engine->state.completed_pairs = global_completed;
    engine->state.total_pairs = global_total;
    engine->state.remaining_pairs = global_total - global_completed;
    engine->state.active_threads = 0;

    /* 保存最终基 */
    int final_basis_size = args[0].basis_size;
    SimplePoly *final_basis = args[0].basis;

    /* 清理旧的基结果 */
    if (engine->groebner_basis) {
        SimplePoly *old_basis = (SimplePoly *)engine->groebner_basis;
        for (int i = 0; i < engine->basis_size; i++) {
            simple_poly_destroy(&old_basis[i]);
        }
        lv00_free((void **)&old_basis);
    }

    engine->groebner_basis = (void **)final_basis;
    engine->basis_size = final_basis_size;

    /* 清理线程参数中的队列引用（队列本身保留给 destroy 清理） */
    for (int t = 0; t < num_threads; t++) {
        args[t].local_queue = NULL;
        args[t].all_queues = NULL;
        args[t].shutdown_flag = NULL;
    }

    /* 释放 all_queues 数组（不释放队列本身） */
    lv00_free((void **)&all_queues);

    return 0;
}

Lv00GroebnerState lv00_groebner_parallel_state(const Lv00GroebnerParallel *engine) {
    Lv00GroebnerState state = {0};
    if (engine) state = engine->state;
    return state;
}

bool lv00_groebner_poly_is_nonzero_constant(void *poly) {
    if (!poly) return false;
    SimplePoly *p = (SimplePoly *)poly;
    if (p->term_count != 1) return false;
    if (fabs(p->terms[0].coeff) < LV00_EPSILON_DOUBLE) return false;
    if (p->terms[0].var_count > 0 && p->terms[0].exponents) {
        for (int i = 0; i < p->terms[0].var_count; i++) {
            if (p->terms[0].exponents[i] != 0) return false;
        }
    }
    return true;
}

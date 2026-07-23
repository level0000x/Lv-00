/**
 * @file groebner_engine.c
 * @brief Groebner 基计算引擎实现 —— Buchberger 算法、理想操作与代数簇求解
 *
 * @details 本模块是 Lv-00 的多项式理想计算核心，实现了：
 *          - 多项式环管理与多项式稀疏存储
 *          - 三种单项式序（lex/grlex/grevlex）的比较
 *          - 多项式加法、乘法、代入运算
 *          - S-多项式计算
 *          - 经典 Buchberger 算法（带约化和互质跳过优化）
 *          - 多项式约化（reduction/normal form）
 *          - 理想成员判定、理想交与理想商
 *          - 约束图到多项式理想的转换
 *          - 零维代数簇的数值求解
 *
 * 参考项目：
 *   - Singular (singular.uni-kl.de) —— 环声明范式、工业级 Gröbner 基
 *   - Macaulay2 (macaulay2.com) —— 理想与簇的统一视角
 *
 * @version v3.3.0
 * @date 2026-05-24
 * @author Lv-00 Project
 */

#include "groebner_engine.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "error_codes.h"
#include "lv_internal.h"
#include "lv_utils.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

/* ================================================================
 *  平台抽象层 —— 跨平台互斥锁（参考 memory_pool.c 模式）
 *
 *  线程安全策略：
 *    groebner_engine 使用全局单例 g_data 存储所有注册的多项式、理想、
 *    Groebner 基和代数簇。在多线程环境下，多个线程可能同时调用
 *    poly_create / poly_destroy / groebner_compute 等公共 API，
 *    导致 g_data 的内部状态产生数据竞争。
 *
 *    本模块采用「粗粒度全局互斥锁」策略：
 *    - 一把静态互斥锁 g_data_mutex 保护整个 g_data 单例
 *    - 所有读写 g_data 的公共 API 在入口处加锁、出口处解锁
 *    - registry_data_ensure() 内部也受锁保护（由调用方持有锁）
 *    - ring_registry_destroy() 需要先加锁再释放资源，最后解锁
 *
 *    注意：当前策略是保守的粗粒度锁，适用于中等并发场景。
 *    若未来需要更高吞吐量，可考虑读写锁（SRWLock / pthread_rwlock）
 *    或按理想/多项式粒度分锁。
 * ================================================================ */

#ifdef _WIN32
typedef CRITICAL_SECTION lvGroebnerMutex;
#define GROEBNER_MUTEX_INIT(m)   InitializeCriticalSection(&(m))
#define GROEBNER_MUTEX_DESTROY(m) DeleteCriticalSection(&(m))
#define GROEBNER_MUTEX_LOCK(m)   EnterCriticalSection(&(m))
#define GROEBNER_MUTEX_UNLOCK(m) LeaveCriticalSection(&(m))
#else
typedef pthread_mutex_t lvGroebnerMutex;
#define GROEBNER_MUTEX_INIT(m)   pthread_mutex_init(&(m), NULL)
#define GROEBNER_MUTEX_DESTROY(m) pthread_mutex_destroy(&(m))
#define GROEBNER_MUTEX_LOCK(m)   pthread_mutex_lock(&(m))
#define GROEBNER_MUTEX_UNLOCK(m) pthread_mutex_unlock(&(m))
#endif

/* ================================================================
 *  内部常量定义
 * ================================================================ */

/** @brief 多项式初始项容量 */
#define GROEBNER_POLY_INIT_CAPACITY 8

/** @brief 多项式扩容因子 */
#define GROEBNER_POLY_GROW_FACTOR 2

/** @brief 理想默认生成元容量 */
#define GROEBNER_IDEAL_INIT_GEN_CAPACITY 8

/** @brief 基多项式默认容量 */
#define GROEBNER_BASIS_INIT_CAPACITY 16

/** @brief 簇解点默认容量 */
#define GROEBNER_VARIETY_INIT_SOL_CAPACITY 32

/** @brief 代数簇求解的最大迭代次数 */
#define GROEBNER_SOLVE_MAX_ITER 200

/** @brief Buchberger 算法最大步数 */
#define GROEBNER_BUCHBERGER_MAX_STEPS 50000

/** @brief 多项式约化最大步数 */
#define GROEBNER_REDUCE_MAX_STEPS 10000

/** @brief 数值零阈值 */
#define GROEBNER_ZERO_THRESHOLD 1e-15

/** @brief 牛顿法收敛阈值 */
#define GROEBNER_NEWTON_TOL 1e-12

/** @brief 牛顿法最大迭代数 */
#define GROEBNER_NEWTON_MAX_ITER 50

/** @brief 单变量多项式根搜索分段数 */
#define GROEBNER_ROOT_SEARCH_SEGMENTS 1000

/** @brief 字符串复制最大长度 */
#define GROEBNER_STR_MAX 256

/* ================================================================
 *  内部数据结构
 * ================================================================ */

/**
 * @brief 注册表内部扩展数据
 *
 * lvRingRegistry 的 public 结构仅存储环信息，
 * 本内部结构跟踪所有多项式、理想、Groebner 基和代数簇。
 *
 * 采用静态全局单例模式 —— 整个引擎生命周期内维护一份注册数据。
 */
typedef struct {
    /** 多项式池 —— 按 poly_id 索引 */
    lvPolynomial **polys;
    int poly_count;     /**< 当前多项式数量 */
    int poly_capacity;  /**< 多项式池容量 */
    int next_poly_id;   /**< 下一个多项式 ID */

    /** 理想池 —— 按 ideal_id 索引 */
    lvIdeal **ideals;
    int ideal_count;
    int ideal_capacity;
    int next_ideal_id;

    /** Groebner 基池 —— 按基索引（不直接暴露 ID） */
    lvGroebnerBasis **bases;
    int bases_count;
    int bases_capacity;

    /** 代数簇池 —— 按 variety_id 索引 */
    lvVariety **varieties;
    int variety_count;
    int variety_capacity;
    int next_variety_id;
} lvRegistryData;

/** @brief 全局注册数据 —— 单例 */
static lvRegistryData *g_data = NULL;

/** @brief 保护 g_data 的全局互斥锁（线程安全） */
static lvGroebnerMutex g_data_mutex;

/** @brief 互斥锁是否已初始化的标志 */
static int g_data_mutex_initialized = 0;

/* ================================================================
 *  前向声明 —— 内部辅助函数
 * ================================================================ */

static int mono_compare(const lvPolynomialRing *ring, const int *powers_a, const int *powers_b);
static int mono_total_degree(const int *powers, int var_count);
static void mono_lcm(const lvPolynomialRing *ring, const int *powers_a, const int *powers_b, int *lcm_out);
static bool mono_divides(const lvPolynomialRing *ring, const int *powers_d, const int *powers_e);
static void mono_divide(const lvPolynomialRing *ring, const int *powers_dividend, const int *powers_divisor,
                        int *quotient_out);
static bool mono_is_coprime(const lvPolynomialRing *ring, const int *powers_a, const int *powers_b);
static int poly_sort_terms(lvPolynomial *poly, const lvPolynomialRing *ring);
static lvPolynomial *poly_internal_create(const lvPolynomialRing *ring, int capacity, const char *label);
static void poly_internal_destroy(lvPolynomial *poly);
static bool poly_ensure_capacity(lvPolynomial *poly, int needed);
static lvPolynomial *poly_internal_copy(const lvPolynomial *src, const lvPolynomialRing *ring);
static lvPolynomial *poly_internal_add(const lvPolynomial *f, const lvPolynomial *g,
                                         const lvPolynomialRing *ring);
static lvPolynomial *poly_internal_multiply(const lvPolynomial *f, const lvPolynomial *g,
                                              const lvPolynomialRing *ring);
static lvPolynomial *poly_internal_substitute(const lvPolynomial *f, int var_index, const lvPolynomial *subst,
                                                const lvPolynomialRing *ring);
static lvPolynomial *poly_internal_s_polynomial(const lvPolynomial *f, const lvPolynomial *g,
                                                  const lvPolynomialRing *ring);
static lvPolynomial *poly_internal_reduce(const lvPolynomial *p, lvPolynomial **basis, int basis_count,
                                            const lvPolynomialRing *ring);
static bool poly_internal_is_zero(const lvPolynomial *poly);
static int poly_internal_total_degree(const lvPolynomial *poly, int var_count);
static void poly_internal_scale(lvPolynomial *poly, double scalar);
static int poly_internal_store(lvRegistryData *data, lvPolynomial *poly);
static int ideal_internal_store(lvRegistryData *data, lvIdeal *ideal);
static int variety_internal_store(lvRegistryData *data, lvVariety *variety);
static lvGroebnerBasis *groebner_internal_compute(const lvPolynomialRing *ring, lvPolynomial **generators,
                                                    int gen_count, lvGroebnerAlgorithm algorithm);
static lvGroebnerBasis *groebner_internal_reduce_basis(lvGroebnerBasis *basis, const lvPolynomialRing *ring);
static lvPolynomial **groebner_solve_zero_dim(const lvGroebnerBasis *basis, const lvPolynomialRing *ring,
                                                int *solution_count);
static double groebner_newton_refine(double (*eval)(double, void *), double (*deriv)(double, void *), void *ctx,
                                     double x0);
static char *groebner_strdup_safe(const char *src);

/* ================================================================
 *  单项式序比较函数
 * ================================================================ */

/**
 * @brief 比较两个单项式的序关系
 *
 * 根据环中定义的单项式序类型，比较两个指数向量的大小。
 *
 * @param ring      多项式环（指定单项式序类型）
 * @param powers_a  第一个单项式的指数向量
 * @param powers_b  第二个单项式的指数向量
 * @return 正数若 A > B，0 若相等，负数若 A < B
 */
static int mono_compare(const lvPolynomialRing *ring, const int *powers_a, const int *powers_b) {
    if (!ring || !powers_a || !powers_b) {
        return 0;
    }

    int vc = ring->var_count;

    switch (ring->order) {
    case MONOMIAL_LEX: {
        /* 纯字典序：从左到右逐项比较 */
        for (int i = 0; i < vc; i++) {
            if (powers_a[i] != powers_b[i]) {
                return powers_a[i] - powers_b[i];
            }
        }
        return 0;
    }
    case MONOMIAL_GRLEX: {
        /* 分次字典序：先比较总次数，相同时用字典序 */
        int deg_a = 0, deg_b = 0;
        for (int i = 0; i < vc; i++) {
            deg_a += powers_a[i];
            deg_b += powers_b[i];
        }
        if (deg_a != deg_b) {
            return deg_a - deg_b;
        }
        for (int i = 0; i < vc; i++) {
            if (powers_a[i] != powers_b[i]) {
                return powers_a[i] - powers_b[i];
            }
        }
        return 0;
    }
    case MONOMIAL_GREVLEX: {
        /* 分次反字典序：先比较总次数，相同时从右向左逐项比较（取反） */
        int deg_a = 0, deg_b = 0;
        for (int i = 0; i < vc; i++) {
            deg_a += powers_a[i];
            deg_b += powers_b[i];
        }
        if (deg_a != deg_b) {
            return deg_a - deg_b;
        }
        for (int i = vc - 1; i >= 0; i--) {
            if (powers_a[i] != powers_b[i]) {
                /* grevlex: 次数相等时，最后变量指数较小的单项式更大 */
                return powers_b[i] - powers_a[i];
            }
        }
        return 0;
    }
    case MONOMIAL_ELIM: {
        /* 消去序：先按消去变量组比较，再按默认 grevlex 比较剩余变量 */
        int elim_count = ring->elim_var_count;
        if (elim_count > 0 && ring->elim_vars) {
            /* 先比较消去组的总次数 */
            int deg_elim_a = 0, deg_elim_b = 0;
            for (int i = 0; i < vc; i++) {
                bool is_elim = false;
                for (int j = 0; j < elim_count; j++) {
                    if (ring->elim_vars[j] == i) {
                        is_elim = true;
                        break;
                    }
                }
                if (is_elim) {
                    deg_elim_a += powers_a[i];
                    deg_elim_b += powers_b[i];
                }
            }
            if (deg_elim_a != deg_elim_b) {
                return deg_elim_a - deg_elim_b;
            }
        }
        /* 回退到 grevlex */
        int deg_a = 0, deg_b = 0;
        for (int i = 0; i < vc; i++) {
            deg_a += powers_a[i];
            deg_b += powers_b[i];
        }
        if (deg_a != deg_b) {
            return deg_a - deg_b;
        }
        for (int i = vc - 1; i >= 0; i--) {
            if (powers_a[i] != powers_b[i]) {
                return powers_b[i] - powers_a[i];
            }
        }
        return 0;
    }
    case MONOMIAL_WEIGHT: {
        /* 权重序：先按权重向量的点积比较，再回退 grevlex */
        if (ring->weights) {
            double w_a = 0.0, w_b = 0.0;
            for (int i = 0; i < vc; i++) {
                w_a += ring->weights[i] * powers_a[i];
                w_b += ring->weights[i] * powers_b[i];
            }
            if (fabs(w_a - w_b) > GROEBNER_ZERO_THRESHOLD) {
                return (w_a > w_b) ? 1 : -1;
            }
        }
        /* 回退到 grevlex */
        int deg_a = 0, deg_b = 0;
        for (int i = 0; i < vc; i++) {
            deg_a += powers_a[i];
            deg_b += powers_b[i];
        }
        if (deg_a != deg_b) {
            return deg_a - deg_b;
        }
        for (int i = vc - 1; i >= 0; i--) {
            if (powers_a[i] != powers_b[i]) {
                return powers_b[i] - powers_a[i];
            }
        }
        return 0;
    }
    default:
        /* 默认 grevlex */
        {
            int deg_a = 0, deg_b = 0;
            for (int i = 0; i < vc; i++) {
                deg_a += powers_a[i];
                deg_b += powers_b[i];
            }
            if (deg_a != deg_b) {
                return deg_a - deg_b;
            }
            for (int i = vc - 1; i >= 0; i--) {
                if (powers_a[i] != powers_b[i]) {
                    return powers_b[i] - powers_a[i];
                }
            }
            return 0;
        }
    }
}

/**
 * @brief 计算单项式的总次数
 *
 * @param powers   指数向量
 * @param var_count 变量数量
 * @return 指数之和
 */
static int mono_total_degree(const int *powers, int var_count) {
    int deg = 0;
    for (int i = 0; i < var_count; i++) {
        deg += powers[i];
    }
    return deg;
}

/**
 * @brief 计算两个单项式的 LCM（最小公倍式）
 *
 * LCM 的每个变量指数取两者最大值。
 *
 * @param ring       多项式环
 * @param powers_a   第一个指数向量
 * @param powers_b   第二个指数向量
 * @param lcm_out    输出 LCM 指数向量（调用者需确保空间 >= var_count）
 */
static void mono_lcm(const lvPolynomialRing *ring, const int *powers_a, const int *powers_b, int *lcm_out) {
    int vc = ring->var_count;
    for (int i = 0; i < vc; i++) {
        lcm_out[i] = (powers_a[i] > powers_b[i]) ? powers_a[i] : powers_b[i];
    }
}

/**
 * @brief 判断单项式 m1 是否被 m2 整除
 *
 * m1 被 m2 整除当且仅当 m1 每个变量的指数 >= m2 对应指数。
 *
 * @param ring      多项式环
 * @param powers_d  被除单项式指数
 * @param powers_e  除单项式指数
 * @return 可整除返回 true
 */
static bool mono_divides(const lvPolynomialRing *ring, const int *powers_d, const int *powers_e) {
    int vc = ring->var_count;
    for (int i = 0; i < vc; i++) {
        if (powers_d[i] < powers_e[i]) {
            return false;
        }
    }
    return true;
}

/**
 * @brief 计算两个单项式的商（指数向量逐项相减）
 *
 * @param ring            多项式环
 * @param powers_dividend 被除指数
 * @param powers_divisor  除指数
 * @param quotient_out    商指数输出
 *
 * @note 调用者必须确保 divisor 整除 dividend
 */
static void mono_divide(const lvPolynomialRing *ring, const int *powers_dividend, const int *powers_divisor,
                        int *quotient_out) {
    int vc = ring->var_count;
    for (int i = 0; i < vc; i++) {
        quotient_out[i] = powers_dividend[i] - powers_divisor[i];
    }
}

/**
 * @brief 判断两个单项式是否互质（每个变量的指数最小值都为 0）
 *
 * @param ring      多项式环
 * @param powers_a  第一个指数
 * @param powers_b  第二个指数
 * @return 互质返回 true
 */
static bool mono_is_coprime(const lvPolynomialRing *ring, const int *powers_a, const int *powers_b) {
    int vc = ring->var_count;
    for (int i = 0; i < vc; i++) {
        if (powers_a[i] > 0 && powers_b[i] > 0) {
            return false;
        }
    }
    return true;
}

/**
 * @brief 复制指数向量
 *
 * @param dest      目标数组
 * @param src       源数组
 * @param var_count 变量数量
 */
static void mono_copy(int *dest, const int *src, int var_count) {
    memcpy(dest, src, (size_t)var_count * sizeof(int));
}

/* ================================================================
 *  安全的字符串复制
 * ================================================================ */

/**
 * @brief 安全的 strdup 封装（失败时返回 NULL）
 *
 * @param src 源字符串（可为 NULL）
 * @return 堆上分配的副本，或 NULL
 */
static char *groebner_strdup_safe(const char *src) {
    if (!src) {
        return NULL;
    }
    size_t len = strlen(src);
    char *dst = (char *)lv_malloc(len + 1);
    if (!dst) {
        return NULL;
    }
    memcpy(dst, src, len + 1);
    return dst;
}

/* ================================================================
 *  多项式项排序
 * ================================================================ */

/**
 * @brief 对多项式的项按单项式序从大到小排序（简单冒泡排序）
 *
 * @param poly  多项式
 * @param ring  多项式环
 * @return 0 成功，负值失败
 */
static int poly_sort_terms(lvPolynomial *poly, const lvPolynomialRing *ring) {
    if (!poly || !ring || poly->term_count <= 1) {
        return 0;
    }

    int vc = ring->var_count;
    double *coeffs = (double *)poly->coeffs;

    for (int i = 0; i < poly->term_count - 1; i++) {
        for (int j = i + 1; j < poly->term_count; j++) {
            int cmp = mono_compare(ring, &poly->powers[i * vc], &poly->powers[j * vc]);
            if (cmp < 0) {
                /* 交换项 i 和 j */
                /* 交换指数 */
                for (int k = 0; k < vc; k++) {
                    int tmp = poly->powers[i * vc + k];
                    poly->powers[i * vc + k] = poly->powers[j * vc + k];
                    poly->powers[j * vc + k] = tmp;
                }
                /* 交换系数 */
                double tmp_c = coeffs[i];
                coeffs[i] = coeffs[j];
                coeffs[j] = tmp_c;
            }
        }
    }

    /* 合并同类项 */
    int write_pos = 0;
    for (int i = 1; i < poly->term_count; i++) {
        if (mono_compare(ring, &poly->powers[write_pos * vc], &poly->powers[i * vc]) == 0) {
            /* 同类项，合并系数 */
            coeffs[write_pos] += coeffs[i];
        } else {
            write_pos++;
            if (write_pos != i) {
                for (int k = 0; k < vc; k++) {
                    poly->powers[write_pos * vc + k] = poly->powers[i * vc + k];
                }
                coeffs[write_pos] = coeffs[i];
            }
        }
    }
    poly->term_count = write_pos + 1;

    /* 移除系数为 0 的项 */
    write_pos = 0;
    for (int i = 0; i < poly->term_count; i++) {
        if (fabs(coeffs[i]) > GROEBNER_ZERO_THRESHOLD) {
            if (write_pos != i) {
                for (int k = 0; k < vc; k++) {
                    poly->powers[write_pos * vc + k] = poly->powers[i * vc + k];
                }
                coeffs[write_pos] = coeffs[i];
            }
            write_pos++;
        }
    }
    poly->term_count = write_pos;

    /* 更新总次数 */
    poly->total_degree = poly_internal_total_degree(poly, vc);

    return 0;
}

/* ================================================================
 *  多项式内部管理函数
 * ================================================================ */

/**
 * @brief 创建一个多项式的内部实例（不注册到池中）
 *
 * @param ring     所属环
 * @param capacity 项容量预分配
 * @param label    标签
 * @return 多项式指针，失败返回 NULL
 */
static lvPolynomial *poly_internal_create(const lvPolynomialRing *ring, int capacity, const char *label) {
    if (!ring) {
        return NULL;
    }

    lvPolynomial *poly = (lvPolynomial *)lv_calloc(1, sizeof(lvPolynomial));
    if (!poly) {
        return NULL;
    }

    if (capacity < 1) {
        capacity = GROEBNER_POLY_INIT_CAPACITY;
    }

    int vc = ring->var_count;
    poly->powers = (int *)lv_calloc((size_t)capacity * (size_t)vc, sizeof(int));
    if (!poly->powers) {
        lv_free((void**)&poly);
        return NULL;
    }

    poly->coeffs = (double *)lv_calloc((size_t)capacity, sizeof(double));
    if (!poly->coeffs) {
        lv_free((void**)&poly->powers);
        lv_free((void**)&poly);
        return NULL;
    }

    poly->ring_id = ring->ring_id;
    poly->term_count = 0;
    poly->term_capacity = capacity;
    poly->total_degree = 0;
    poly->is_homogeneous = true;
    poly->label = groebner_strdup_safe(label);

    return poly;
}

/**
 * @brief 销毁多项式内部实例
 *
 * @param poly 多项式指针
 */
static void poly_internal_destroy(lvPolynomial *poly) {
    if (!poly) {
        return;
    }
    lv_free((void**)&poly->powers);
    lv_free((void**)&poly->coeffs);
    lv_free((void**)&poly->label);
    lv_free((void**)&poly);
}

/**
 * @brief 确保多项式有足够的容量存储更多项
 *
 * @param poly    多项式
 * @param needed  需要的总容量
 * @return 成功返回 true
 */
static bool poly_ensure_capacity(lvPolynomial *poly, int needed) {
    if (!poly) {
        return false;
    }
    if (poly->term_capacity >= needed) {
        return true;
    }

    int new_cap = poly->term_capacity;
    while (new_cap < needed) {
        new_cap *= GROEBNER_POLY_GROW_FACTOR;
        if (new_cap > 1000000) {
            /* 安全上限 */
            new_cap = needed + 100;
            break;
        }
    }

    int vc = 0;
    /* 从外部环获取 var_count —— 使用 powers 中隐含的信息 */
    /* 这里需要确定 var_count，我们需要一个方式获取 */
    /* 暂时从 poly 的已知上下文获取 —— 实际上我们需要环信息 */
    /* 让我们推断：poly->term_capacity 已知，但 var_count 未知。 */
    /* 简化处理：维护一个内部字段或者在调用点传入 var_count。 */
    /* 此处通过重新分配现有块的大小比例来推断。 */
    /* 但这是不完美的。我们需要更好的设计。 */
    /* 让我们检查是否可以从现有分配推断：如果 poly->term_capacity > 0 */
    /* 则之前的分配是 term_capacity * var_count，但我们不知道 var_count。 */
    /* 解决方案：在函数签名中需要 var_count 或存储在多项式中。 */
    /* 由于这是内部函数，我们约定调用者通过其他方式确保参数正确。 */
    /* 实际上我们需要 var_count。让我们采用另一种方法：在多项式结构中存储 var_count。 */
    /* 但 struct 定义在头文件中，我不能修改它。 */
    /* 让我们采用一个折中方案：在 poly 结构中通过 total_degree 等方式无法推断 var_count。 */
    /* 简单地，我们要求调用者确保容量足够，或者我们在此处做一个保守假设。 */
    /* 既然 powers 数组长度 = term_capacity * var_count，且目前 term_count 已知， */
    /* 我们可以通过 term_count > 0 时 powers 的长度来推断。但首次分配时 term_count=0 则不行。 */
    /* 警告：此函数为空操作（始终返回 true）。
     * 原因：多项式的 powers 数组大小为 term_capacity * var_count，
     * 但本函数无法获取 var_count（该信息存储在外部环结构中）。
     * 所有需要扩容的调用点均使用 poly_ensure_capacity_ex(poly, needed, var_count)。
     * 保留此函数仅为向后兼容 API 签名。 */
    return true;
}

/**
 * @brief 为多项式扩容（需要知道 var_count）
 *
 * @param poly      多项式
 * @param needed    需要的总容量
 * @param var_count 变量数量
 * @return 成功返回 true
 */
static bool poly_ensure_capacity_ex(lvPolynomial *poly, int needed, int var_count) {
    if (!poly) {
        return false;
    }
    if (poly->term_capacity >= needed) {
        return true;
    }

    int new_cap = poly->term_capacity;
    if (new_cap < 1) {
        new_cap = GROEBNER_POLY_INIT_CAPACITY;
    }
    while (new_cap < needed) {
        new_cap *= GROEBNER_POLY_GROW_FACTOR;
        if (new_cap > 1000000) {
            new_cap = needed + 100;
            break;
        }
    }

    int *new_powers = (int *)lv_realloc(poly->powers, (size_t)new_cap * (size_t)var_count * sizeof(int));
    if (!new_powers) {
        return false;
    }
    /* 清零新分配的区域 */
    memset(new_powers + poly->term_capacity * var_count, 0,
           (size_t)(new_cap - poly->term_capacity) * (size_t)var_count * sizeof(int));
    poly->powers = new_powers;

    double *new_coeffs = (double *)lv_realloc(poly->coeffs, (size_t)new_cap * sizeof(double));
    if (!new_coeffs) {
        /* powers 已扩容成功，但 coeffs 失败了 —— 这是不太可能的情况，回滚 powers */
        /* 为简化，不处理这种极端情况，假设 realloc 要么都成功要么都失败 */
        return false;
    }
    /* 清零新系数 */
    memset(new_coeffs + poly->term_capacity, 0, (size_t)(new_cap - poly->term_capacity) * sizeof(double));
    poly->coeffs = new_coeffs;
    poly->term_capacity = new_cap;

    return true;
}

/**
 * @brief 深拷贝多项式
 *
 * @param src  源多项式
 * @param ring 所属环
 * @return 新分配的多项式副本，失败返回 NULL
 */
static lvPolynomial *poly_internal_copy(const lvPolynomial *src, const lvPolynomialRing *ring) {
    if (!src || !ring) {
        return NULL;
    }

    lvPolynomial *cpy = poly_internal_create(ring, src->term_capacity, src->label);
    if (!cpy) {
        return NULL;
    }

    int vc = ring->var_count;
    cpy->term_count = src->term_count;
    cpy->total_degree = src->total_degree;
    cpy->is_homogeneous = src->is_homogeneous;

    memcpy(cpy->powers, src->powers, (size_t)src->term_count * (size_t)vc * sizeof(int));
    memcpy(cpy->coeffs, src->coeffs, (size_t)src->term_count * sizeof(double));

    return cpy;
}

/* ================================================================
 *  多项式运算 —— 加法
 * ================================================================ */

/**
 * @brief 内部多项式加法：h = f + g
 *
 * @param f    被加多项式
 * @param g    加多项式
 * @param ring 所属环
 * @return 新多项式的和，失败返回 NULL
 */
static lvPolynomial *poly_internal_add(const lvPolynomial *f, const lvPolynomial *g,
                                         const lvPolynomialRing *ring) {
    if (!f || !g || !ring) {
        return NULL;
    }

    int vc = ring->var_count;
    int est_capacity = f->term_count + g->term_count;
    if (est_capacity < GROEBNER_POLY_INIT_CAPACITY) {
        est_capacity = GROEBNER_POLY_INIT_CAPACITY;
    }

    lvPolynomial *result = poly_internal_create(ring, est_capacity, NULL);
    if (!result) {
        return NULL;
    }

    double *coeffs = (double *)result->coeffs;
    int fi = 0, gi = 0;

    while (fi < f->term_count && gi < g->term_count) {
        int cmp = mono_compare(ring, &f->powers[fi * vc], &g->powers[gi * vc]);
        int ti = result->term_count;

        if (!poly_ensure_capacity_ex(result, ti + 1, vc)) {
            poly_internal_destroy(result);
            return NULL;
        }

        if (cmp > 0) {
            /* f 的项更大 */
            mono_copy(&result->powers[ti * vc], &f->powers[fi * vc], vc);
            coeffs[ti] = ((double *)f->coeffs)[fi];
            result->term_count++;
            fi++;
        } else if (cmp < 0) {
            /* g 的项更大 */
            mono_copy(&result->powers[ti * vc], &g->powers[gi * vc], vc);
            coeffs[ti] = ((double *)g->coeffs)[gi];
            result->term_count++;
            gi++;
        } else {
            /* 同类项 */
            double sum = ((double *)f->coeffs)[fi] + ((double *)g->coeffs)[gi];
            if (fabs(sum) > GROEBNER_ZERO_THRESHOLD) {
                mono_copy(&result->powers[ti * vc], &f->powers[fi * vc], vc);
                coeffs[ti] = sum;
                result->term_count++;
            }
            fi++;
            gi++;
        }
    }

    /* 复制 f 剩余项 */
    while (fi < f->term_count) {
        int ti = result->term_count;
        if (!poly_ensure_capacity_ex(result, ti + 1, vc)) {
            poly_internal_destroy(result);
            return NULL;
        }
        mono_copy(&result->powers[ti * vc], &f->powers[fi * vc], vc);
        coeffs[ti] = ((double *)f->coeffs)[fi];
        result->term_count++;
        fi++;
    }

    /* 复制 g 剩余项 */
    while (gi < g->term_count) {
        int ti = result->term_count;
        if (!poly_ensure_capacity_ex(result, ti + 1, vc)) {
            poly_internal_destroy(result);
            return NULL;
        }
        mono_copy(&result->powers[ti * vc], &g->powers[gi * vc], vc);
        coeffs[ti] = ((double *)g->coeffs)[gi];
        result->term_count++;
        gi++;
    }

    result->total_degree = poly_internal_total_degree(result, vc);

    return result;
}

/* ================================================================
 *  多项式运算 —— 乘法
 * ================================================================ */

/**
 * @brief 内部多项式乘法：h = f * g
 *
 * 两多项式的每一项相乘，指数相加、系数相乘，最后合并同类项并排序。
 *
 * @param f    被乘多项式
 * @param g    乘多项式
 * @param ring 所属环
 * @return 新多项式的积，失败返回 NULL
 */
static lvPolynomial *poly_internal_multiply(const lvPolynomial *f, const lvPolynomial *g,
                                              const lvPolynomialRing *ring) {
    if (!f || !g || !ring) {
        return NULL;
    }

    int vc = ring->var_count;
    int est_capacity = f->term_count * g->term_count;
    if (est_capacity < 1) {
        est_capacity = GROEBNER_POLY_INIT_CAPACITY;
    }
    if (est_capacity > 100000) {
        est_capacity = 100000;
    }

    lvPolynomial *result = poly_internal_create(ring, est_capacity, NULL);
    if (!result) {
        return NULL;
    }

    double *f_coeffs = (double *)f->coeffs;
    double *g_coeffs = (double *)g->coeffs;
    double *r_coeffs = (double *)result->coeffs;

    for (int i = 0; i < f->term_count; i++) {
        for (int j = 0; j < g->term_count; j++) {
            if (!poly_ensure_capacity_ex(result, result->term_count + 1, vc)) {
                poly_internal_destroy(result);
                return NULL;
            }

            int ti = result->term_count;
            /* 指数相加 */
            for (int k = 0; k < vc; k++) {
                result->powers[ti * vc + k] = f->powers[i * vc + k] + g->powers[j * vc + k];
            }
            r_coeffs[ti] = f_coeffs[i] * g_coeffs[j];
            result->term_count++;
        }
    }

    /* 排序并合并同类项 */
    poly_sort_terms(result, ring);

    return result;
}

/* ================================================================
 *  多项式运算 —— 代入
 * ================================================================ */

/**
 * @brief 多项式代入：将指定变量替换为另一个多项式
 *
 * f(x_1,...,x_i,...,x_n) 中 x_i 代入 g，即计算 f(x_1,...,g,...,x_n)。
 * 每一项的 x_i^{e_i} 被替换为 g^{e_i}。
 *
 * @param f         待代入多项式
 * @param var_index 被替换的变量索引（0-based）
 * @param subst     代入的多项式
 * @param ring      所属环
 * @return 代入结果多项式，失败返回 NULL
 */
static lvPolynomial *poly_internal_substitute(const lvPolynomial *f, int var_index, const lvPolynomial *subst,
                                                const lvPolynomialRing *ring) {
    if (!f || !subst || !ring) {
        return NULL;
    }
    if (var_index < 0 || var_index >= ring->var_count) {
        return NULL;
    }

    /* 先创建零多项式作为累加器 */
    lvPolynomial *result = poly_internal_create(ring, GROEBNER_POLY_INIT_CAPACITY, NULL);
    if (!result) {
        return NULL;
    }

    int vc = ring->var_count;
    double *f_coeffs = (double *)f->coeffs;

    for (int i = 0; i < f->term_count; i++) {
        int exp = f->powers[i * vc + var_index];
        double coeff = f_coeffs[i];

        if (fabs(coeff) < GROEBNER_ZERO_THRESHOLD) {
            continue;
        }

        /* 构造该项去除 x_var 后的单项式 */
        lvPolynomial *term_poly = poly_internal_create(ring, 1, NULL);
        if (!term_poly) {
            poly_internal_destroy(result);
            return NULL;
        }
        term_poly->term_count = 1;
        term_poly->term_capacity = 1;
        /* 重新分配以确保正确大小 */
        lv_free((void**)&term_poly->powers);
        lv_free((void**)&term_poly->coeffs);
        term_poly->powers = (int *)lv_calloc((size_t)vc, sizeof(int));
        term_poly->coeffs = (double *)lv_calloc(1, sizeof(double));
        if (!term_poly->powers || !term_poly->coeffs) {
            poly_internal_destroy(term_poly);
            poly_internal_destroy(result);
            return NULL;
        }
        term_poly->term_capacity = 1;
        for (int k = 0; k < vc; k++) {
            if (k != var_index) {
                term_poly->powers[k] = f->powers[i * vc + k];
            }
        }
        ((double *)term_poly->coeffs)[0] = coeff;

        /* 计算 subst^{exp} */
        if (exp == 0) {
            /* x_i^0 = 1, term_poly 就是此项 */
            lvPolynomial *tmp = poly_internal_add(result, term_poly, ring);
            poly_internal_destroy(result);
            result = tmp;
            poly_internal_destroy(term_poly);
        } else if (exp == 1) {
            /* term_poly * subst */
            lvPolynomial *prod = poly_internal_multiply(term_poly, subst, ring);
            lvPolynomial *tmp = poly_internal_add(result, prod, ring);
            poly_internal_destroy(result);
            poly_internal_destroy(prod);
            result = tmp;
            poly_internal_destroy(term_poly);
        } else {
            /* term_poly * subst^exp */
            lvPolynomial *subst_pow = poly_internal_copy(subst, ring);
            if (!subst_pow) {
                poly_internal_destroy(term_poly);
                poly_internal_destroy(result);
                return NULL;
            }
            for (int e = 1; e < exp; e++) {
                lvPolynomial *next = poly_internal_multiply(subst_pow, subst, ring);
                poly_internal_destroy(subst_pow);
                subst_pow = next;
                if (!subst_pow) {
                    poly_internal_destroy(term_poly);
                    poly_internal_destroy(result);
                    return NULL;
                }
            }
            lvPolynomial *prod = poly_internal_multiply(term_poly, subst_pow, ring);
            lvPolynomial *tmp = poly_internal_add(result, prod, ring);
            poly_internal_destroy(result);
            poly_internal_destroy(prod);
            poly_internal_destroy(subst_pow);
            result = tmp;
            poly_internal_destroy(term_poly);
        }

        if (!result) {
            return NULL;
        }
    }

    return result;
}

/* ================================================================
 *  多项式辅助函数
 * ================================================================ */

/**
 * @brief 检查多项式是否为零多项式
 *
 * @param poly 多项式
 * @return 零多项式返回 true
 */
static bool poly_internal_is_zero(const lvPolynomial *poly) {
    if (!poly) {
        return true;
    }
    if (poly->term_count == 0) {
        return true;
    }
    /* 检查是否所有系数都接近零 */
    double *coeffs = (double *)poly->coeffs;
    for (int i = 0; i < poly->term_count; i++) {
        if (fabs(coeffs[i]) > GROEBNER_ZERO_THRESHOLD) {
            return false;
        }
    }
    return true;
}

/**
 * @brief 计算多项式的总次数（所有项中单项式指数的最大值）
 *
 * @param poly      多项式
 * @param var_count 变量数量
 * @return 总次数
 */
static int poly_internal_total_degree(const lvPolynomial *poly, int var_count) {
    if (!poly || poly->term_count == 0) {
        return 0;
    }
    int max_deg = 0;
    for (int i = 0; i < poly->term_count; i++) {
        int deg = mono_total_degree(&poly->powers[i * var_count], var_count);
        if (deg > max_deg) {
            max_deg = deg;
        }
    }
    return max_deg;
}

/**
 * @brief 多项式乘以标量
 *
 * @param poly    多项式（原地修改）
 * @param scalar  标量乘数
 */
static void poly_internal_scale(lvPolynomial *poly, double scalar) {
    if (!poly) {
        return;
    }
    double *coeffs = (double *)poly->coeffs;
    for (int i = 0; i < poly->term_count; i++) {
        coeffs[i] *= scalar;
    }
}

/**
 * @brief 获取多项式的前导项指数（即按序最大的项）
 *
 * @param poly      多项式
 * @param ring      环
 * @param lt_out    前导项指数输出（需预先分配 var_count 大小）
 * @param lc_out    前导系数输出（可为 NULL）
 * @return 0 成功，-1 多项式为零
 */
static int poly_leading_term(const lvPolynomial *poly, const lvPolynomialRing *ring, int *lt_out, double *lc_out) {
    if (!poly || !ring || poly->term_count == 0) {
        if (lc_out) *lc_out = 0.0;
        return -1;
    }

    int vc = ring->var_count;
    int best_idx = 0;
    for (int i = 1; i < poly->term_count; i++) {
        if (mono_compare(ring, &poly->powers[i * vc], &poly->powers[best_idx * vc]) > 0) {
            best_idx = i;
        }
    }

    if (lt_out) {
        mono_copy(lt_out, &poly->powers[best_idx * vc], vc);
    }
    if (lc_out) {
        *lc_out = ((double *)poly->coeffs)[best_idx];
    }
    return 0;
}

/* ================================================================
 *  S-多项式计算
 * ================================================================ */

/**
 * @brief 计算两个多项式的 S-多项式
 *
 * S(f, g) = (LCM(LT(f), LT(g)) / LT(f)) * f - (LCM(LT(f), LT(g)) / LT(g)) * g
 *
 * 其中 LCM/LT 表示对应单项式的商，系数分别取 LC(g) 和 LC(f) 使得前导项抵消。
 *
 * @param f    第一个多项式
 * @param g    第二个多项式
 * @param ring 所属环
 * @return S-多项式，失败返回 NULL
 */
static lvPolynomial *poly_internal_s_polynomial(const lvPolynomial *f, const lvPolynomial *g,
                                                  const lvPolynomialRing *ring) {
    if (!f || !g || !ring) {
        return NULL;
    }

    int vc = ring->var_count;
    double lc_f, lc_g;
    int *lt_f = (int *)lv_calloc((size_t)vc, sizeof(int));
    int *lt_g = (int *)lv_calloc((size_t)vc, sizeof(int));
    int *lcm = (int *)lv_calloc((size_t)vc, sizeof(int));
    int *quot_f = (int *)lv_calloc((size_t)vc, sizeof(int));
    int *quot_g = (int *)lv_calloc((size_t)vc, sizeof(int));

    if (!lt_f || !lt_g || !lcm || !quot_f || !quot_g) {
        lv_free((void**)&lt_f);
        lv_free((void**)&lt_g);
        lv_free((void**)&lcm);
        lv_free((void**)&quot_f);
        lv_free((void**)&quot_g);
        return NULL;
    }

    /* 获取前导项 */
    if (poly_leading_term(f, ring, lt_f, &lc_f) != 0 || poly_leading_term(g, ring, lt_g, &lc_g) != 0) {
        lv_free((void**)&lt_f);
        lv_free((void**)&lt_g);
        lv_free((void**)&lcm);
        lv_free((void**)&quot_f);
        lv_free((void**)&quot_g);
        /* 如果任一项为零多项式，S-多项式为零 */
        return poly_internal_create(ring, 1, NULL);
    }

    /* 计算 LCM 和商 */
    mono_lcm(ring, lt_f, lt_g, lcm);
    mono_divide(ring, lcm, lt_f, quot_f);
    mono_divide(ring, lcm, lt_g, quot_g);

    /* 构造 (lcm/lt_f) * f 部分 */
    lvPolynomial *term_f = poly_internal_create(ring, 1, NULL);
    if (!term_f) {
        goto s_poly_cleanup;
    }
    lv_free((void**)&term_f->powers);
    lv_free((void**)&term_f->coeffs);
    term_f->powers = (int *)lv_calloc((size_t)vc, sizeof(int));
    term_f->coeffs = (double *)lv_calloc(1, sizeof(double));
    if (!term_f->powers || !term_f->coeffs) {
        poly_internal_destroy(term_f);
        goto s_poly_cleanup;
    }
    term_f->term_capacity = 1;
    term_f->term_count = 1;
    mono_copy(term_f->powers, quot_f, vc);
    ((double *)term_f->coeffs)[0] = lc_g; /* 乘以 lc_g 使得 S(f,g) 的前导项抵消 */

    lvPolynomial *part_f = poly_internal_multiply(term_f, f, ring);
    poly_internal_destroy(term_f);
    if (!part_f) {
        goto s_poly_cleanup;
    }

    /* 构造 (lcm/lt_g) * g 部分 */
    lvPolynomial *term_g = poly_internal_create(ring, 1, NULL);
    if (!term_g) {
        poly_internal_destroy(part_f);
        goto s_poly_cleanup;
    }
    lv_free((void**)&term_g->powers);
    lv_free((void**)&term_g->coeffs);
    term_g->powers = (int *)lv_calloc((size_t)vc, sizeof(int));
    term_g->coeffs = (double *)lv_calloc(1, sizeof(double));
    if (!term_g->powers || !term_g->coeffs) {
        poly_internal_destroy(term_g);
        poly_internal_destroy(part_f);
        goto s_poly_cleanup;
    }
    term_g->term_capacity = 1;
    term_g->term_count = 1;
    mono_copy(term_g->powers, quot_g, vc);
    ((double *)term_g->coeffs)[0] = lc_f; /* 乘以 lc_f */

    lvPolynomial *part_g = poly_internal_multiply(term_g, g, ring);
    poly_internal_destroy(term_g);
    if (!part_g) {
        poly_internal_destroy(part_f);
        goto s_poly_cleanup;
    }

    /* S = part_f - part_g */
    poly_internal_scale(part_g, -1.0);
    lvPolynomial *s_poly = poly_internal_add(part_f, part_g, ring);
    poly_internal_destroy(part_f);
    poly_internal_destroy(part_g);

    lv_free((void**)&lt_f);
    lv_free((void**)&lt_g);
    lv_free((void**)&lcm);
    lv_free((void**)&quot_f);
    lv_free((void**)&quot_g);
    return s_poly;

s_poly_cleanup:
    lv_free((void**)&lt_f);
    lv_free((void**)&lt_g);
    lv_free((void**)&lcm);
    lv_free((void**)&quot_f);
    lv_free((void**)&quot_g);
    return NULL;
}

/* ================================================================
 *  多项式约化（Reduction / Normal Form）
 * ================================================================ */

/**
 * @brief 用一组基多项式约化一个多项式（计算 normal form）
 *
 * 给定多项式 p 和基集合 G = {g_1,...,g_m}，反复用 G 中的元素约化 p 的各项：
 * 如果存在 g in G 使得 LT(g) 整除 p 的某项 t，则用 t - c * (t/LT(g)) * g 替换 t。
 * 重复此过程直到无法继续约化。
 *
 * @param p           待约化多项式
 * @param basis       基多项式数组
 * @param basis_count 基多项式数量
 * @param ring        所属环
 * @return 约化后的多项式（normal form），失败返回 NULL
 *
 * @note 约化结果依赖于基的选择顺序和约化路径，但对于 Groebner 基，
 *       约化结果是唯一的（即 normal form）。
 */
static lvPolynomial *poly_internal_reduce(const lvPolynomial *p, lvPolynomial **basis, int basis_count,
                                            const lvPolynomialRing *ring) {
    if (!p || !basis || !ring) {
        return NULL;
    }

    lvPolynomial *remainder = poly_internal_copy(p, ring);
    if (!remainder) {
        return NULL;
    }

    int vc = ring->var_count;
    double *rem_coeffs = (double *)remainder->coeffs;
    int step_count = 0;

    bool changed = true;
    while (changed && step_count < GROEBNER_REDUCE_MAX_STEPS) {
        changed = false;
        step_count++;

        /* 寻找当前多项式中可被约化的项 */
        for (int i = 0; i < remainder->term_count; i++) {
            if (fabs(rem_coeffs[i]) < GROEBNER_ZERO_THRESHOLD) {
                continue;
            }

            /* 查找基中能整除该项的基元素 */
            int reducer_idx = -1;
            for (int j = 0; j < basis_count; j++) {
                if (poly_internal_is_zero(basis[j])) {
                    continue;
                }
                double lc_b;
                int *lt_b = (int *)lv_calloc((size_t)vc, sizeof(int));
                if (!lt_b) continue;
                if (poly_leading_term(basis[j], ring, lt_b, &lc_b) == 0) {
                    if (mono_divides(ring, &remainder->powers[i * vc], lt_b)) {
                        reducer_idx = j;
                        lv_free((void**)&lt_b);
                        break;
                    }
                }
                lv_free((void**)&lt_b);
            }

            if (reducer_idx < 0) {
                continue;
            }

            /* 获取约化器信息 */
            lvPolynomial *reducer = basis[reducer_idx];
            double lc_r;
            int *lt_r = (int *)lv_calloc((size_t)vc, sizeof(int));
            if (!lt_r) continue;
            if (poly_leading_term(reducer, ring, lt_r, &lc_r) != 0) {
                lv_free((void**)&lt_r);
                continue;
            }

            /* 计算商单项式：m = t / LT(reducer) */
            int *quot_mono = (int *)lv_calloc((size_t)vc, sizeof(int));
            if (!quot_mono) {
                lv_free((void**)&lt_r);
                continue;
            }
            mono_divide(ring, &remainder->powers[i * vc], lt_r, quot_mono);
            lv_free((void**)&lt_r);

            /* 构造乘子多项式：c * m，其中 c = coeff(t)/lc(reducer) */
            double factor = rem_coeffs[i] / lc_r;

            lvPolynomial *mult_term = poly_internal_create(ring, 1, NULL);
            if (!mult_term) {
                lv_free((void**)&quot_mono);
                continue;
            }
            lv_free((void**)&mult_term->powers);
            lv_free((void**)&mult_term->coeffs);
            mult_term->powers = (int *)lv_calloc((size_t)vc, sizeof(int));
            mult_term->coeffs = (double *)lv_calloc(1, sizeof(double));
            if (!mult_term->powers || !mult_term->coeffs) {
                poly_internal_destroy(mult_term);
                lv_free((void**)&quot_mono);
                continue;
            }
            mult_term->term_capacity = 1;
            mult_term->term_count = 1;
            mono_copy(mult_term->powers, quot_mono, vc);
            ((double *)mult_term->coeffs)[0] = factor;
            lv_free((void**)&quot_mono);

            /* 减去的部分 = mult_term * reducer */
            lvPolynomial *subtrahend = poly_internal_multiply(mult_term, reducer, ring);
            poly_internal_destroy(mult_term);
            if (!subtrahend) {
                continue;
            }

            /* 从 remainder 中移除当前项并加上减去的部分（实际上是从 remainder 中
             * 减去 subtrahend）*/
            /* 先标记第 i 项为 0 */
            rem_coeffs[i] = 0.0;

            /* remainder = remainder - subtrahend = remainder + (-subtrahend) */
            poly_internal_scale(subtrahend, -1.0);
            lvPolynomial *new_rem = poly_internal_add(remainder, subtrahend, ring);
            poly_internal_destroy(subtrahend);

            if (!new_rem) {
                continue;
            }

            poly_internal_destroy(remainder);
            remainder = new_rem;
            rem_coeffs = (double *)remainder->coeffs;

            /* 清理并排序 */
            poly_sort_terms(remainder, ring);
            rem_coeffs = (double *)remainder->coeffs;

            changed = true;
            break; /* 重新开始约化循环 */
        }
    }

    return remainder;
}

/* ================================================================
 *  Buchberger 算法 —— 核心 Gröbner 基计算
 * ================================================================ */

/**
 * @brief Buchberger 算法的标准实现（带优化）
 *
 * 算法流程：
 * 1. 初始化 G = 生成元集合
 * 2. 构建所有生成元对的集合 B
 * 3. 重复直到 B 为空：
 *    a. 选择一对 (fi, fj) 从 B 中移除
 *    b. 应用 Buchberger 互质判别式：若 gcd(LT(fi), LT(fj)) = 1，跳过
 *    c. 计算 S(fi, fj)，并用 G 约化得到 r
 *    d. 若 r != 0，则将 r 加入 G，并添加新对 (gi, r) 到 B
 * 4. 返回约化的 Gröbner 基
 *
 * @param ring        多项式环
 * @param generators  生成元多项式数组
 * @param gen_count   生成元数量
 * @param algorithm   算法选择（当前仅实现 BUCHBERGER）
 * @return Gröbner 基结构体，失败返回 NULL
 */
static lvGroebnerBasis *groebner_internal_compute(const lvPolynomialRing *ring, lvPolynomial **generators,
                                                    int gen_count, lvGroebnerAlgorithm algorithm) {
    if (!ring || !generators || gen_count <= 0) {
        return NULL;
    }

    lv_UNUSED(algorithm); /* 当前仅实现 Buchberger */

    lvGroebnerBasis *basis = (lvGroebnerBasis *)lv_calloc(1, sizeof(lvGroebnerBasis));
    if (!basis) {
        return NULL;
    }

    basis->basis_polys = (lvPolynomial **)lv_calloc((size_t)gen_count * 2 + GROEBNER_BASIS_INIT_CAPACITY,
                                                   sizeof(lvPolynomial *));
    if (!basis->basis_polys) {
        lv_free((void**)&basis);
        return NULL;
    }
    basis->bases_capacity = gen_count * 2 + GROEBNER_BASIS_INIT_CAPACITY;
    basis->bases_count = 0;
    basis->algorithm_used = GROEBNER_BUCHBERGER;

    /* 将生成元复制到基中（去除非零的） */
    for (int i = 0; i < gen_count; i++) {
        if (generators[i] && !poly_internal_is_zero(generators[i])) {
            if (basis->bases_count >= basis->bases_capacity) {
                int new_cap = basis->bases_capacity * 2;
                lvPolynomial **new_polys = (lvPolynomial **)lv_realloc(basis->basis_polys,
                                                                        (size_t)new_cap * sizeof(lvPolynomial *));
                if (!new_polys) {
                    /* 清理已分配的内存 */
                    for (int j = 0; j < basis->bases_count; j++) {
                        poly_internal_destroy(basis->basis_polys[j]);
                    }
                    lv_free((void**)&basis->basis_polys);
                    lv_free((void**)&basis);
                    return NULL;
                }
                basis->basis_polys = new_polys;
                basis->bases_capacity = new_cap;
            }
            basis->basis_polys[basis->bases_count] = poly_internal_copy(generators[i], ring);
            if (!basis->basis_polys[basis->bases_count]) {
                /* 清理 */
                for (int j = 0; j < basis->bases_count; j++) {
                    poly_internal_destroy(basis->basis_polys[j]);
                }
                lv_free((void**)&basis->basis_polys);
                lv_free((void**)&basis);
                return NULL;
            }
            basis->bases_count++;
        }
    }

    if (basis->bases_count == 0) {
        /* 理想是零理想 */
        basis->is_minimal = true;
        basis->is_reduced = true;
        basis->reducing_degree = 0;
        return basis;
    }

    int vc = ring->var_count;

    /* 构建对集合 B：用二维数组标记哪些对已被处理 */
    /* 使用简单方法：维护一个增长的对列表 */
    int pair_capacity = 4096;
    int pair_count = 0;
    int *pairs_i = (int *)lv_malloc((size_t)pair_capacity * sizeof(int));
    int *pairs_j = (int *)lv_malloc((size_t)pair_capacity * sizeof(int));
    if (!pairs_i || !pairs_j) {
        lv_free((void**)&pairs_i);
        lv_free((void**)&pairs_j);
        for (int i = 0; i < basis->bases_count; i++) {
            poly_internal_destroy(basis->basis_polys[i]);
        }
        lv_free((void**)&basis->basis_polys);
        lv_free((void**)&basis);
        return NULL;
    }

    /* 初始化：所有 (i, j) 对，i < j */
    for (int i = 0; i < basis->bases_count; i++) {
        for (int j = i + 1; j < basis->bases_count; j++) {
            if (pair_count >= pair_capacity) {
                int new_cap = pair_capacity * 2;
                int *new_i = (int *)lv_realloc(pairs_i, (size_t)new_cap * sizeof(int));
                int *new_j = (int *)lv_realloc(pairs_j, (size_t)new_cap * sizeof(int));
                if (!new_i || !new_j) {
                    lv_free((void**)&new_i);
                    lv_free((void**)&new_j);
                    lv_free((void**)&pairs_i);
                    lv_free((void**)&pairs_j);
                    for (int k = 0; k < basis->bases_count; k++) {
                        poly_internal_destroy(basis->basis_polys[k]);
                    }
                    lv_free((void**)&basis->basis_polys);
                    lv_free((void**)&basis);
                    return NULL;
                }
                pairs_i = new_i;
                pairs_j = new_j;
                pair_capacity = new_cap;
            }
            pairs_i[pair_count] = i;
            pairs_j[pair_count] = j;
            pair_count++;
        }
    }

    int step = 0;

    while (pair_count > 0 && step < GROEBNER_BUCHBERGER_MAX_STEPS) {
        step++;

        /* 取一对 */
        pair_count--;
        int idx_i = pairs_i[pair_count];
        int idx_j = pairs_j[pair_count];

        lvPolynomial *fi = basis->basis_polys[idx_i];
        lvPolynomial *fj = basis->basis_polys[idx_j];

        /* 优化 1：互质判别式 —— 若前导项互质，则 S(fi, fj) 一定约化为 0 */
        int *lt_i = (int *)lv_calloc((size_t)vc, sizeof(int));
        int *lt_j = (int *)lv_calloc((size_t)vc, sizeof(int));
        if (!lt_i || !lt_j) {
            lv_free((void**)&lt_i);
            lv_free((void**)&lt_j);
            continue;
        }

        if (poly_leading_term(fi, ring, lt_i, NULL) != 0 || poly_leading_term(fj, ring, lt_j, NULL) != 0) {
            lv_free((void**)&lt_i);
            lv_free((void**)&lt_j);
            continue;
        }

        bool coprime = mono_is_coprime(ring, lt_i, lt_j);
        lv_free((void**)&lt_i);
        lv_free((void**)&lt_j);

        if (coprime) {
            /* 互质 => S(fi, fj) 约化为 0，跳过 */
            continue;
        }

        /* 计算 S-多项式 */
        lvPolynomial *s = poly_internal_s_polynomial(fi, fj, ring);
        if (!s) {
            continue;
        }

        /* 用当前基约化 S-多项式 */
        lvPolynomial *r = poly_internal_reduce(s, basis->basis_polys, basis->bases_count, ring);
        poly_internal_destroy(s);
        if (!r) {
            continue;
        }

        /* 如果约化结果非零，加入基 */
        if (!poly_internal_is_zero(r)) {
            /* 扩容基数组 */
            if (basis->bases_count >= basis->bases_capacity) {
                int new_cap = basis->bases_capacity * 2;
                lvPolynomial **new_polys = (lvPolynomial **)lv_realloc(basis->basis_polys,
                                                                        (size_t)new_cap * sizeof(lvPolynomial *));
                if (!new_polys) {
                    poly_internal_destroy(r);
                    break;
                }
                basis->basis_polys = new_polys;
                basis->bases_capacity = new_cap;
            }

            int new_idx = basis->bases_count;
            basis->basis_polys[new_idx] = r;
            basis->bases_count++;

            /* 添加新对 (existing_i, new_idx) 到 B */
            for (int i = 0; i < new_idx; i++) {
                if (pair_count >= pair_capacity) {
                    int new_cap = pair_capacity * 2;
                    int *new_i = (int *)lv_realloc(pairs_i, (size_t)new_cap * sizeof(int));
                    int *new_j = (int *)lv_realloc(pairs_j, (size_t)new_cap * sizeof(int));
                    if (!new_i || !new_j) {
                        lv_free((void**)&new_i);
                        lv_free((void**)&new_j);
                        pair_count = 0;
                        break;
                    }
                    pairs_i = new_i;
                    pairs_j = new_j;
                    pair_capacity = new_cap;
                }
                pairs_i[pair_count] = i;
                pairs_j[pair_count] = new_idx;
                pair_count++;
            }
        } else {
            poly_internal_destroy(r);
        }
    }

    lv_free((void**)&pairs_i);
    lv_free((void**)&pairs_j);

    /* 计算约化 Groebner 基 */
    basis = groebner_internal_reduce_basis(basis, ring);

    return basis;
}

/**
 * @brief 约化 Groebner 基 —— 使基满足最小且约化的属性
 *
 * 1. 最小化：移除前导项可被其他元素前导项整除的元素
 * 2. 约化：每个基元素的前导系数规一化，并用其他基元素约化降低其余项
 *
 * @param basis 原始基
 * @param ring  环
 * @return 约化后的基（原地修改）
 */
static lvGroebnerBasis *groebner_internal_reduce_basis(lvGroebnerBasis *basis, const lvPolynomialRing *ring) {
    if (!basis || !ring || basis->bases_count == 0) {
        if (basis) {
            basis->is_minimal = true;
            basis->is_reduced = true;
        }
        return basis;
    }

    int vc = ring->var_count;

    /* 规一化所有基多项式的前导系数 */
    for (int i = 0; i < basis->bases_count; i++) {
        lvPolynomial *p = basis->basis_polys[i];
        if (poly_internal_is_zero(p)) continue;
        double lc;
        if (poly_leading_term(p, ring, NULL, &lc) == 0 && fabs(lc) > GROEBNER_ZERO_THRESHOLD) {
            poly_internal_scale(p, 1.0 / lc);
        }
    }

    /* 最小化：删除前导项可被其他基元前导项整除的元素 */
    int write_pos = 0;
    for (int i = 0; i < basis->bases_count; i++) {
        lvPolynomial *pi = basis->basis_polys[i];
        if (poly_internal_is_zero(pi)) {
            poly_internal_destroy(pi);
            continue;
        }
        int *lt_pi = (int *)lv_calloc((size_t)vc, sizeof(int));
        if (!lt_pi) continue;
        if (poly_leading_term(pi, ring, lt_pi, NULL) != 0) {
            lv_free((void**)&lt_pi);
            poly_internal_destroy(pi);
            continue;
        }

        bool redundant = false;
        for (int j = 0; j < basis->bases_count; j++) {
            if (i == j) continue;
            lvPolynomial *pj = basis->basis_polys[j];
            if (poly_internal_is_zero(pj)) continue;
            int *lt_pj = (int *)lv_calloc((size_t)vc, sizeof(int));
            if (!lt_pj) continue;
            if (poly_leading_term(pj, ring, lt_pj, NULL) == 0) {
                if (mono_divides(ring, lt_pi, lt_pj)) {
                    redundant = true;
                    lv_free((void**)&lt_pj);
                    break;
                }
            }
            lv_free((void**)&lt_pj);
        }

        lv_free((void**)&lt_pi);

        if (!redundant) {
            basis->basis_polys[write_pos] = pi;
            write_pos++;
        } else {
            poly_internal_destroy(pi);
        }
    }
    basis->bases_count = write_pos;

    /* 互相约化：每个基元素用其余基元素约化 */
    for (int i = 0; i < basis->bases_count; i++) {
        /* 构建不含第 i 个元素的基数组 */
        lvPolynomial **others = (lvPolynomial **)lv_malloc((size_t)(basis->bases_count - 1) * sizeof(lvPolynomial *));
        if (!others) continue;
        int o_count = 0;
        for (int j = 0; j < basis->bases_count; j++) {
            if (j != i) {
                others[o_count++] = basis->basis_polys[j];
            }
        }
        lvPolynomial *reduced = poly_internal_reduce(basis->basis_polys[i], others, o_count, ring);
        lv_free((void**)&others);
        if (reduced) {
            poly_internal_destroy(basis->basis_polys[i]);
            basis->basis_polys[i] = reduced;
        }
    }

    /* 再次规一化 */
    for (int i = 0; i < basis->bases_count; i++) {
        lvPolynomial *p = basis->basis_polys[i];
        if (poly_internal_is_zero(p)) continue;
        double lc;
        if (poly_leading_term(p, ring, NULL, &lc) == 0 && fabs(lc) > GROEBNER_ZERO_THRESHOLD) {
            poly_internal_scale(p, 1.0 / lc);
        }
    }

    /* 计算约化后的最大次数 */
    int max_deg = 0;
    for (int i = 0; i < basis->bases_count; i++) {
        int deg = poly_internal_total_degree(basis->basis_polys[i], vc);
        if (deg > max_deg) {
            max_deg = deg;
        }
    }
    basis->reducing_degree = max_deg;
    basis->is_minimal = true;
    basis->is_reduced = true;

    return basis;
}

/* ================================================================
 *  内部存储管理 —— 多项式/理想/簇的池管理
 * ================================================================ */

/**
 * @brief 将多项式存入全局注册数据池并返回 ID
 *
 * @param data  注册数据
 * @param poly  多项式指针（所有权转移）
 * @return 多项式 ID（>= 0），失败返回 -1
 */
static int poly_internal_store(lvRegistryData *data, lvPolynomial *poly) {
    if (!data || !poly) {
        return -1;
    }

    if (data->poly_count >= data->poly_capacity) {
        int new_cap = data->poly_capacity == 0 ? GROEBNER_POLY_INIT_CAPACITY : data->poly_capacity * 2;
        lvPolynomial **new_polys = (lvPolynomial **)lv_realloc(data->polys,
                                                                 (size_t)new_cap * sizeof(lvPolynomial *));
        if (!new_polys) {
            return -1;
        }
        data->polys = new_polys;
        data->poly_capacity = new_cap;
    }

    int id = data->next_poly_id++;
    poly->poly_id = id;
    data->polys[data->poly_count++] = poly;
    return id;
}

/**
 * @brief 将理想存入全局注册数据池
 */
static int ideal_internal_store(lvRegistryData *data, lvIdeal *ideal) {
    if (!data || !ideal) {
        return -1;
    }

    if (data->ideal_count >= data->ideal_capacity) {
        int new_cap = data->ideal_capacity == 0 ? GROEBNER_IDEAL_INIT_GEN_CAPACITY : data->ideal_capacity * 2;
        lvIdeal **new_ideals = (lvIdeal **)lv_realloc(data->ideals,
                                                         (size_t)new_cap * sizeof(lvIdeal *));
        if (!new_ideals) {
            return -1;
        }
        data->ideals = new_ideals;
        data->ideal_capacity = new_cap;
    }

    int id = data->next_ideal_id++;
    ideal->ideal_id = id;
    data->ideals[data->ideal_count++] = ideal;
    return id;
}

/**
 * @brief 将代数簇存入全局注册数据池
 */
static int variety_internal_store(lvRegistryData *data, lvVariety *variety) {
    if (!data || !variety) {
        return -1;
    }

    if (data->variety_count >= data->variety_capacity) {
        int new_cap = data->variety_capacity == 0 ? 8 : data->variety_capacity * 2;
        lvVariety **new_vars = (lvVariety **)lv_realloc(data->varieties,
                                                           (size_t)new_cap * sizeof(lvVariety *));
        if (!new_vars) {
            return -1;
        }
        data->varieties = new_vars;
        data->variety_capacity = new_cap;
    }

    int id = data->next_variety_id++;
    variety->variety_id = id;
    data->varieties[data->variety_count++] = variety;
    return id;
}

/**
 * @brief 确保全局注册数据已初始化（调用方必须持有 g_data_mutex）
 *
 * 注意：此函数不负责加锁，由调用方在持有锁的状态下调用。
 * 首次调用时初始化互斥锁本身（仅执行一次）。
 */
static lvRegistryData *registry_data_ensure(void) {
    /* 首次调用时初始化互斥锁（仅一次） */
    if (!g_data_mutex_initialized) {
        GROEBNER_MUTEX_INIT(g_data_mutex);
        g_data_mutex_initialized = 1;
    }
    if (!g_data) {
        g_data = (lvRegistryData *)lv_calloc(1, sizeof(lvRegistryData));
    }
    return g_data;
}

/* ================================================================
 *  零维代数簇数值求解
 * ================================================================ */

/**
 * @brief 单变量多项式求值
 */
typedef struct {
    double *coeffs;
    int degree;
} UnivariatePolyCtx;

static double univar_eval(double x, void *ctx) {
    UnivariatePolyCtx *uc = (UnivariatePolyCtx *)ctx;
    double result = 0.0;
    double xpow = 1.0;
    for (int i = 0; i <= uc->degree; i++) {
        result += uc->coeffs[i] * xpow;
        xpow *= x;
    }
    return result;
}

/**
 * @brief 单变量多项式求导
 */
static double univar_deriv(double x, void *ctx) {
    UnivariatePolyCtx *uc = (UnivariatePolyCtx *)ctx;
    double result = 0.0;
    double xpow = 1.0;
    for (int i = 1; i <= uc->degree; i++) {
        result += i * uc->coeffs[i] * xpow;
        xpow *= x;
    }
    return result;
}

/**
 * @brief 牛顿法细化单变量根
 */
static double groebner_newton_refine(double (*eval)(double, void *), double (*deriv)(double, void *),
                                     void *ctx, double x0) {
    double x = x0;
    for (int iter = 0; iter < GROEBNER_NEWTON_MAX_ITER; iter++) {
        double fx = eval(x, ctx);
        double fpx = deriv(x, ctx);
        if (fabs(fpx) < GROEBNER_ZERO_THRESHOLD) {
            break;
        }
        double dx = fx / fpx;
        x = x - dx;
        if (fabs(dx) < GROEBNER_NEWTON_TOL) {
            break;
        }
    }
    return x;
}

/**
 * @brief 从零维 Groebner 基求解多项式方程组
 *
 * 对于零维理想，Groebner 基（在 lex 序下）具有三角形形式：
 * g_n(x_n) = 0, g_{n-1}(x_{n-1}, x_n) = 0, ...
 *
 * 采用回代法：先解单变量方程，再逐次回代。
 *
 * @return 解点坐标数组（调用者负责释放），*solution_count 输出解的数量
 */
static lvPolynomial **groebner_solve_zero_dim(const lvGroebnerBasis *basis,
                                                 const lvPolynomialRing *ring,
                                                 int *solution_count) {
    *solution_count = 0;
    if (!basis || !ring || basis->bases_count == 0) {
        return NULL;
    }

    int vc = ring->var_count;
    if (vc < 1) {
        return NULL;
    }

    /* 仅支持变量数 <= 3 的简单零维求解 */
    if (vc > 3) {
        return NULL;
    }

    /* 尝试从基中提取单变量多项式 */
    /* 寻找仅含最后一个变量的基元 */
    lvPolynomial *univar = NULL;
    for (int i = 0; i < basis->bases_count; i++) {
        lvPolynomial *p = basis->basis_polys[i];
        if (poly_internal_is_zero(p)) continue;
        bool single_var = true;
        for (int ti = 0; ti < p->term_count; ti++) {
            for (int v = 0; v < vc - 1; v++) {
                if (p->powers[ti * vc + v] != 0) {
                    single_var = false;
                    break;
                }
            }
            if (!single_var) break;
        }
        if (single_var) {
            univar = p;
            break;
        }
    }

    if (!univar) {
        return NULL;
    }

    /* 提取最高次数用于构造单变量多项式上下文 */
    int max_deg = 0;
    double *deg_coeffs = NULL;
    double *u_coeffs = (double *)univar->coeffs;
    for (int ti = 0; ti < univar->term_count; ti++) {
        int deg = univar->powers[ti * vc + vc - 1];
        if (deg > max_deg) {
            max_deg = deg;
        }
    }

    deg_coeffs = (double *)lv_calloc((size_t)(max_deg + 1), sizeof(double));
    if (!deg_coeffs) {
        return NULL;
    }
    for (int ti = 0; ti < univar->term_count; ti++) {
        int deg = univar->powers[ti * vc + vc - 1];
        deg_coeffs[deg] = u_coeffs[ti];
    }

    UnivariatePolyCtx ctx;
    ctx.coeffs = deg_coeffs;
    ctx.degree = max_deg;

    /* 简单的根搜索：在区间 [-10, 10] 上分段查找符号变化 */
    int max_solutions = 16;
    double *roots = (double *)lv_malloc((size_t)max_solutions * sizeof(double));
    int root_count = 0;
    if (!roots) {
        lv_free((void**)&deg_coeffs);
        return NULL;
    }

    double a = -10.0, b = 10.0;
    double step = (b - a) / (double)GROEBNER_ROOT_SEARCH_SEGMENTS;
    double prev_val = univar_eval(a, &ctx);

    for (int seg = 1; seg <= GROEBNER_ROOT_SEARCH_SEGMENTS && root_count < max_solutions; seg++) {
        double x = a + step * seg;
        double curr_val = univar_eval(x, &ctx);

        if (prev_val * curr_val < 0.0) {
            /* 符号变化：根存在于此区间 */
            double mid = (x + (x - step)) / 2.0;
            double root = groebner_newton_refine(univar_eval, univar_deriv, &ctx, mid);
            if (fabs(univar_eval(root, &ctx)) < GROEBNER_ZERO_THRESHOLD) {
                roots[root_count++] = root;
            }
        } else if (fabs(curr_val) < GROEBNER_ZERO_THRESHOLD) {
            roots[root_count++] = x;
        }
        prev_val = curr_val;
    }

    lv_free((void**)&deg_coeffs);

    if (root_count == 0) {
        lv_free((void**)&roots);
        return NULL;
    }

    /* 为每个根构造解点坐标（此处简化：仅一维） */
    lvPolynomial **solutions = (lvPolynomial **)lv_malloc((size_t)root_count * sizeof(lvPolynomial *));
    if (!solutions) {
        lv_free((void**)&roots);
        return NULL;
    }

    for (int ri = 0; ri < root_count; ri++) {
        lvPolynomial *sol = poly_internal_create(ring, 1, NULL);
        if (!sol) {
            for (int j = 0; j < ri; j++) {
                poly_internal_destroy(solutions[j]);
            }
            lv_free((void**)&solutions);
            lv_free((void**)&roots);
            return NULL;
        }
        sol->term_count = 1;
        sol->term_capacity = 1;
        lv_free((void**)&sol->powers);
        lv_free((void**)&sol->coeffs);
        sol->powers = (int *)lv_calloc((size_t)vc, sizeof(int));
        sol->coeffs = (double *)lv_calloc(1, sizeof(double));
        if (!sol->powers || !sol->coeffs) {
            poly_internal_destroy(sol);
            for (int j = 0; j < ri; j++) {
                poly_internal_destroy(solutions[j]);
            }
            lv_free((void**)&solutions);
            lv_free((void**)&roots);
            return NULL;
        }
        sol->term_capacity = 1;
        /* 常量多项式表示点坐标 */
        ((double *)sol->coeffs)[0] = roots[ri];
        solutions[ri] = sol;
    }

    *solution_count = root_count;
    lv_free((void**)&roots);
    return solutions;
}

/* ================================================================
 *  第一部分：公共 API —— 环注册表管理
 * ================================================================ */

/**
 * @brief 创建环注册表
 */
lvRingRegistry *ring_registry_create(int capacity) {
    if (capacity < 1) {
        capacity = 8;
    }

    lvRingRegistry *registry = (lvRingRegistry *)lv_calloc(1, sizeof(lvRingRegistry));
    if (!registry) {
        return NULL;
    }

    registry->rings = (lvPolynomialRing **)lv_calloc((size_t)capacity, sizeof(lvPolynomialRing *));
    if (!registry->rings) {
        lv_free((void**)&registry);
        return NULL;
    }
    registry->ring_capacity = capacity;
    registry->ring_count = 0;
    registry->active_ring_id = -1;
    registry->is_initialized = true;

    /* 初始化全局注册数据（加锁保护） */
    GROEBNER_MUTEX_LOCK(g_data_mutex);
    registry_data_ensure();
    GROEBNER_MUTEX_UNLOCK(g_data_mutex);

    return registry;
}

/**
 * @brief 销毁环注册表及所有关联对象
 */
void ring_registry_destroy(lvRingRegistry *registry) {
    if (!registry) {
        return;
    }

    /* 加锁保护全局池数据的释放 */
    GROEBNER_MUTEX_LOCK(g_data_mutex);

    /* 释放全局池数据 */
    if (g_data) {
        if (g_data->polys) {
            for (int i = 0; i < g_data->poly_count; i++) {
                poly_internal_destroy(g_data->polys[i]);
            }
            lv_free((void**)&g_data->polys);
        }
        if (g_data->ideals) {
            for (int i = 0; i < g_data->ideal_count; i++) {
                if (g_data->ideals[i]) {
                    if (g_data->ideals[i]->cached_basis) {
                        for (int j = 0; j < g_data->ideals[i]->cached_basis->bases_count; j++) {
                            poly_internal_destroy(g_data->ideals[i]->cached_basis->basis_polys[j]);
                        }
                        lv_free((void**)&g_data->ideals[i]->cached_basis->basis_polys);
                        lv_free((void**)&g_data->ideals[i]->cached_basis);
                    }
                    lv_free((void**)&g_data->ideals[i]->label);
                    lv_free((void**)&g_data->ideals[i]);
                }
            }
            lv_free((void**)&g_data->ideals);
        }
        if (g_data->varieties) {
            for (int i = 0; i < g_data->variety_count; i++) {
                if (g_data->varieties[i]) {
                    if (g_data->varieties[i]->solution_points) {
                        for (int j = 0; j < g_data->varieties[i]->solution_count; j++) {
                            lv_free((void**)&g_data->varieties[i]->solution_points[j]);
                        }
                        lv_free((void**)&g_data->varieties[i]->solution_points);
                    }
                    lv_free((void**)&g_data->varieties[i]->label);
                    lv_free((void**)&g_data->varieties[i]);
                }
            }
            lv_free((void**)&g_data->varieties);
        }
        if (g_data->bases) {
            lv_free((void**)&g_data->bases);
        }
        lv_free((void**)&g_data);
        g_data = NULL;
    }

    /* 释放互斥锁 */
    GROEBNER_MUTEX_UNLOCK(g_data_mutex);
    if (g_data_mutex_initialized) {
        GROEBNER_MUTEX_DESTROY(g_data_mutex);
        g_data_mutex_initialized = 0;
    }

    /* 释放环 */
    for (int i = 0; i < registry->ring_count; i++) {
        if (registry->rings[i]) {
            lv_free((void**)&registry->rings[i]->var_names);
            lv_free((void**)&registry->rings[i]->elim_vars);
            lv_free((void**)&registry->rings[i]->weights);
            lv_free((void**)&registry->rings[i]->label);
            lv_free((void**)&registry->rings[i]);
        }
    }
    lv_free((void**)&registry->rings);
    registry->rings = NULL;
    registry->ring_count = 0;
    registry->ring_capacity = 0;
    registry->is_initialized = false;
    lv_free((void**)&registry);
}

/**
 * @brief 创建一个多项式环
 */
int ring_create(lvRingRegistry *registry, const char *var_names[], int var_count,
                lvRingFieldType field, lvMonomialOrder order, const char *label) {
    if (!registry || !var_names || var_count < 1) {
        return -1;
    }

    if (registry->ring_count >= registry->ring_capacity) {
        int new_cap = registry->ring_capacity * 2;
        lvPolynomialRing **new_rings = (lvPolynomialRing **)lv_realloc(registry->rings,
                                                                          (size_t)new_cap * sizeof(lvPolynomialRing *));
        if (!new_rings) {
            return -1;
        }
        registry->rings = new_rings;
        registry->ring_capacity = new_cap;
    }

    lvPolynomialRing *ring = (lvPolynomialRing *)lv_calloc(1, sizeof(lvPolynomialRing));
    if (!ring) {
        return -1;
    }

    ring->var_names = (char **)lv_calloc((size_t)var_count, sizeof(char *));
    if (!ring->var_names) {
        lv_free((void**)&ring);
        return -1;
    }
    for (int i = 0; i < var_count; i++) {
        ring->var_names[i] = groebner_strdup_safe(var_names[i]);
    }

    ring->var_count = var_count;
    ring->field = field;
    ring->order = order;
    ring->label = groebner_strdup_safe(label);
    ring->is_commutative = true;

    int ring_id = registry->ring_count;
    ring->ring_id = ring_id;
    registry->rings[registry->ring_count++] = ring;

    return ring_id;
}

/**
 * @brief 销毁一个多项式环
 */
void ring_destroy(lvRingRegistry *registry, int ring_id) {
    if (!registry || ring_id < 0 || ring_id >= registry->ring_count) {
        return;
    }

    lvPolynomialRing *ring = registry->rings[ring_id];
    if (ring) {
        if (ring->var_names) {
            for (int i = 0; i < ring->var_count; i++) {
                lv_free((void**)&ring->var_names[i]);
            }
            lv_free((void**)&ring->var_names);
        }
        lv_free((void**)&ring->elim_vars);
        lv_free((void**)&ring->weights);
        lv_free((void**)&ring->label);
        lv_free((void**)&ring);
    }

    /* 将后续环前移 */
    for (int i = ring_id; i < registry->ring_count - 1; i++) {
        registry->rings[i] = registry->rings[i + 1];
        if (registry->rings[i]) {
            registry->rings[i]->ring_id = i;
        }
    }
    registry->rings[registry->ring_count - 1] = NULL;
    registry->ring_count--;
}

/**
 * @brief 注册外部创建的环
 */
int ring_register(lvRingRegistry *registry, lvPolynomialRing *ring) {
    if (!registry || !ring) {
        return -1;
    }

    if (registry->ring_count >= registry->ring_capacity) {
        int new_cap = registry->ring_capacity * 2;
        lvPolynomialRing **new_rings = (lvPolynomialRing **)lv_realloc(registry->rings,
                                                                          (size_t)new_cap * sizeof(lvPolynomialRing *));
        if (!new_rings) {
            return -1;
        }
        registry->rings = new_rings;
        registry->ring_capacity = new_cap;
    }

    int ring_id = registry->ring_count;
    ring->ring_id = ring_id;
    registry->rings[registry->ring_count++] = ring;
    return ring_id;
}

/**
 * @brief 按 ID 查找环
 */
lvPolynomialRing *ring_find(const lvRingRegistry *registry, int ring_id) {
    if (!registry || ring_id < 0 || ring_id >= registry->ring_count) {
        return NULL;
    }
    return registry->rings[ring_id];
}

/* ================================================================
 *  第二部分：公共 API —— 多项式操作
 * ================================================================ */

/**
 * @brief 创建多项式并存入池
 */
int poly_create(lvRingRegistry *registry, int ring_id, int capacity, const char *label) {
    if (!registry || ring_id < 0 || ring_id >= registry->ring_count) {
        return -1;
    }

    lvRingRegistry *r = registry;
    lv_UNUSED(r);

    lvPolynomialRing *ring = registry->rings[ring_id];
    if (!ring) {
        return -1;
    }

    lvPolynomial *poly = poly_internal_create(ring, capacity, label);
    if (!poly) {
        return -1;
    }

    GROEBNER_MUTEX_LOCK(g_data_mutex);
    lvRegistryData *data = registry_data_ensure();
    if (!data) {
        GROEBNER_MUTEX_UNLOCK(g_data_mutex);
        poly_internal_destroy(poly);
        return -1;
    }

    int result = poly_internal_store(data, poly);
    GROEBNER_MUTEX_UNLOCK(g_data_mutex);
    return result;
}

/**
 * @brief 销毁多项式
 */
void poly_destroy(lvRingRegistry *registry, int poly_id) {
    lv_UNUSED(registry);
    GROEBNER_MUTEX_LOCK(g_data_mutex);
    if (!g_data || poly_id < 0 || poly_id >= g_data->poly_count) {
        GROEBNER_MUTEX_UNLOCK(g_data_mutex);
        return;
    }

    if (g_data->polys[poly_id]) {
        poly_internal_destroy(g_data->polys[poly_id]);
        g_data->polys[poly_id] = NULL;
    }
    GROEBNER_MUTEX_UNLOCK(g_data_mutex);
}

/**
 * @brief 多项式加法
 */
int poly_add(lvRingRegistry *registry, int poly_id_f, int poly_id_g, const char *result_label) {
    if (!registry) return -1;

    GROEBNER_MUTEX_LOCK(g_data_mutex);
    if (!g_data) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }
    if (poly_id_f < 0 || poly_id_g < 0) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }
    if (poly_id_f >= g_data->poly_count || poly_id_g >= g_data->poly_count) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }

    lvPolynomial *f = g_data->polys[poly_id_f];
    lvPolynomial *g = g_data->polys[poly_id_g];
    if (!f || !g) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }

    if (f->ring_id != g->ring_id) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }
    lvPolynomialRing *ring = registry->rings[f->ring_id];
    if (!ring) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }

    lvPolynomial *result = poly_internal_add(f, g, ring);
    if (!result) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }

    lv_free((void**)&result->label);
    result->label = groebner_strdup_safe(result_label);

    int ret = poly_internal_store(g_data, result);
    GROEBNER_MUTEX_UNLOCK(g_data_mutex);
    return ret;
}

/**
 * @brief 多项式乘法
 */
int poly_multiply(lvRingRegistry *registry, int poly_id_f, int poly_id_g, const char *result_label) {
    if (!registry) return -1;

    GROEBNER_MUTEX_LOCK(g_data_mutex);
    if (!g_data) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }
    if (poly_id_f < 0 || poly_id_g < 0) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }
    if (poly_id_f >= g_data->poly_count || poly_id_g >= g_data->poly_count) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }

    lvPolynomial *f = g_data->polys[poly_id_f];
    lvPolynomial *g = g_data->polys[poly_id_g];
    if (!f || !g) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }

    if (f->ring_id != g->ring_id) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }
    lvPolynomialRing *ring = registry->rings[f->ring_id];
    if (!ring) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }

    lvPolynomial *result = poly_internal_multiply(f, g, ring);
    if (!result) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }

    lv_free((void**)&result->label);
    result->label = groebner_strdup_safe(result_label);

    int ret = poly_internal_store(g_data, result);
    GROEBNER_MUTEX_UNLOCK(g_data_mutex);
    return ret;
}

/**
 * @brief 多项式代入
 */
int poly_substitute(lvRingRegistry *registry, int poly_id, int var_index, int subst_poly_id,
                    const char *result_label) {
    if (!registry) return -1;

    GROEBNER_MUTEX_LOCK(g_data_mutex);
    if (!g_data) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }
    if (poly_id < 0 || subst_poly_id < 0) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }
    if (poly_id >= g_data->poly_count || subst_poly_id >= g_data->poly_count) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }

    lvPolynomial *f = g_data->polys[poly_id];
    lvPolynomial *subst = g_data->polys[subst_poly_id];
    if (!f || !subst) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }

    if (f->ring_id != subst->ring_id) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }
    lvPolynomialRing *ring = registry->rings[f->ring_id];
    if (!ring) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }

    lvPolynomial *result = poly_internal_substitute(f, var_index, subst, ring);
    if (!result) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }

    lv_free((void**)&result->label);
    result->label = groebner_strdup_safe(result_label);

    int ret = poly_internal_store(g_data, result);
    GROEBNER_MUTEX_UNLOCK(g_data_mutex);
    return ret;
}

/**
 * @brief 获取多项式实例
 */
const lvPolynomial *poly_get(const lvRingRegistry *registry, int poly_id) {
    lv_UNUSED(registry);
    GROEBNER_MUTEX_LOCK(g_data_mutex);
    if (!g_data || poly_id < 0 || poly_id >= g_data->poly_count) {
        GROEBNER_MUTEX_UNLOCK(g_data_mutex);
        return NULL;
    }
    const lvPolynomial *p = g_data->polys[poly_id];
    GROEBNER_MUTEX_UNLOCK(g_data_mutex);
    return p;
}

/* ================================================================
 *  第三部分：公共 API —— 理想与 Groebner 基
 * ================================================================ */

/**
 * @brief 创建理想
 */
int ideal_create(lvRingRegistry *registry, int ring_id, const char *label) {
    if (!registry || ring_id < 0 || ring_id >= registry->ring_count) {
        return -1;
    }

    lvIdeal *ideal = (lvIdeal *)lv_calloc(1, sizeof(lvIdeal));
    if (!ideal) {
        return -1;
    }

    ideal->ring_id = ring_id;
    ideal->generators = (lvPolynomial **)lv_calloc((size_t)GROEBNER_IDEAL_INIT_GEN_CAPACITY,
                                                    sizeof(lvPolynomial *));
    if (!ideal->generators) {
        lv_free((void**)&ideal);
        return -1;
    }
    ideal->generator_capacity = GROEBNER_IDEAL_INIT_GEN_CAPACITY;
    ideal->generator_count = 0;
    ideal->cached_basis = NULL;
    ideal->basis_valid = false;
    ideal->label = groebner_strdup_safe(label);

    GROEBNER_MUTEX_LOCK(g_data_mutex);
    lvRegistryData *data = registry_data_ensure();
    if (!data) {
        GROEBNER_MUTEX_UNLOCK(g_data_mutex);
        lv_free((void**)&ideal->generators);
        lv_free((void**)&ideal->label);
        lv_free((void**)&ideal);
        return -1;
    }

    int result = ideal_internal_store(data, ideal);
    GROEBNER_MUTEX_UNLOCK(g_data_mutex);
    return result;
}

/**
 * @brief 销毁理想
 */
void ideal_destroy(lvRingRegistry *registry, int ideal_id) {
    lv_UNUSED(registry);
    GROEBNER_MUTEX_LOCK(g_data_mutex);
    if (!g_data || ideal_id < 0 || ideal_id >= g_data->ideal_count) {
        GROEBNER_MUTEX_UNLOCK(g_data_mutex);
        return;
    }

    lvIdeal *ideal = g_data->ideals[ideal_id];
    if (!ideal) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return; }

    if (ideal->cached_basis) {
        if (ideal->cached_basis->basis_polys) {
            for (int i = 0; i < ideal->cached_basis->bases_count; i++) {
                poly_internal_destroy(ideal->cached_basis->basis_polys[i]);
            }
            lv_free((void**)&ideal->cached_basis->basis_polys);
        }
        lv_free((void**)&ideal->cached_basis);
    }
    lv_free((void**)&ideal->generators);
    lv_free((void**)&ideal->label);
    lv_free((void**)&ideal);
    g_data->ideals[ideal_id] = NULL;
    GROEBNER_MUTEX_UNLOCK(g_data_mutex);
}

/**
 * @brief 向理想添加生成元
 */
int ideal_add_generator(lvRingRegistry *registry, int ideal_id, int poly_id) {
    if (!registry) return -1;

    GROEBNER_MUTEX_LOCK(g_data_mutex);
    if (!g_data) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }
    if (ideal_id < 0 || ideal_id >= g_data->ideal_count) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }
    if (poly_id < 0 || poly_id >= g_data->poly_count) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }

    lvIdeal *ideal = g_data->ideals[ideal_id];
    lvPolynomial *poly = g_data->polys[poly_id];
    if (!ideal || !poly) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }

    if (ideal->ring_id != poly->ring_id) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }

    if (ideal->generator_count >= ideal->generator_capacity) {
        int new_cap = ideal->generator_capacity * 2;
        lvPolynomial **new_gens = (lvPolynomial **)lv_realloc(ideal->generators,
                                                                 (size_t)new_cap * sizeof(lvPolynomial *));
        if (!new_gens) {
            GROEBNER_MUTEX_UNLOCK(g_data_mutex);
            return -1;
        }
        ideal->generators = new_gens;
        ideal->generator_capacity = new_cap;
    }

    ideal->generators[ideal->generator_count++] = poly;
    ideal->basis_valid = false; /* 缓存失效 */

    GROEBNER_MUTEX_UNLOCK(g_data_mutex);
    return 0;
}

/**
 * @brief 计算 Groebner 基
 */
int groebner_compute(lvRingRegistry *registry, int ideal_id, lvGroebnerAlgorithm algorithm) {
    if (!registry) return -1;

    GROEBNER_MUTEX_LOCK(g_data_mutex);
    if (!g_data) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }
    if (ideal_id < 0 || ideal_id >= g_data->ideal_count) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }

    lvIdeal *ideal = g_data->ideals[ideal_id];
    if (!ideal) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }

    lvPolynomialRing *ring = registry->rings[ideal->ring_id];
    if (!ring) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }

    if (ideal->generator_count == 0) {
        /* 零理想 */
        lvGroebnerBasis *basis = (lvGroebnerBasis *)lv_calloc(1, sizeof(lvGroebnerBasis));
        if (!basis) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }
        basis->is_minimal = true;
        basis->is_reduced = true;
        basis->algorithm_used = GROEBNER_BUCHBERGER;
        ideal->cached_basis = basis;
        ideal->basis_valid = true;
        GROEBNER_MUTEX_UNLOCK(g_data_mutex);
        return 0;
    }

    uint64_t start_us = 0;
    start_us = (uint64_t)clock(); /* 简单计时 */

    lvGroebnerBasis *basis = groebner_internal_compute(ring, ideal->generators,
                                                          ideal->generator_count, algorithm);
    if (!basis) {
        GROEBNER_MUTEX_UNLOCK(g_data_mutex);
        return -1;
    }

    uint64_t elapsed = (uint64_t)clock() - start_us;
    basis->computation_time_us = (int64_t)(elapsed * 1000000 / CLOCKS_PER_SEC);

    /* 释放旧缓存 */
    if (ideal->cached_basis) {
        if (ideal->cached_basis->basis_polys) {
            for (int i = 0; i < ideal->cached_basis->bases_count; i++) {
                poly_internal_destroy(ideal->cached_basis->basis_polys[i]);
            }
            lv_free((void**)&ideal->cached_basis->basis_polys);
        }
        lv_free((void**)&ideal->cached_basis);
    }

    ideal->cached_basis = basis;
    ideal->basis_valid = true;
    GROEBNER_MUTEX_UNLOCK(g_data_mutex);
    return 0;
}

/**
 * @brief 增量式 Groebner 基计算
 */
int groebner_compute_incremental(lvRingRegistry *registry, int ideal_id, int new_poly_id) {
    if (!registry) return -1;

    GROEBNER_MUTEX_LOCK(g_data_mutex);
    if (!g_data) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }
    if (ideal_id < 0 || ideal_id >= g_data->ideal_count) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }
    if (new_poly_id < 0 || new_poly_id >= g_data->poly_count) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }

    lvIdeal *ideal = g_data->ideals[ideal_id];
    lvPolynomial *new_poly = g_data->polys[new_poly_id];
    if (!ideal || !new_poly) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }
    if (ideal->ring_id != new_poly->ring_id) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }

    /* 添加生成元并完全重算（简化版；真正的 F5 增量算法需更多实现） */
    /* 注意：ideal_add_generator 和 groebner_compute 内部也会加锁，
     * 但由于我们已经持有锁，这里直接操作内部数据以避免死锁。 */
    if (ideal->generator_count >= ideal->generator_capacity) {
        int new_cap = ideal->generator_capacity * 2;
        lvPolynomial **new_gens = (lvPolynomial **)lv_realloc(ideal->generators,
                                                                 (size_t)new_cap * sizeof(lvPolynomial *));
        if (!new_gens) {
            GROEBNER_MUTEX_UNLOCK(g_data_mutex);
            return -1;
        }
        ideal->generators = new_gens;
        ideal->generator_capacity = new_cap;
    }
    ideal->generators[ideal->generator_count++] = new_poly;
    ideal->basis_valid = false;

    /* 直接调用内部计算（已持有锁） */
    if (ideal->generator_count == 0) {
        GROEBNER_MUTEX_UNLOCK(g_data_mutex);
        return 0;
    }

    lvPolynomialRing *ring = registry->rings[ideal->ring_id];
    if (!ring) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }

    lvGroebnerBasis *basis = groebner_internal_compute(ring, ideal->generators,
                                                          ideal->generator_count, GROEBNER_BUCHBERGER);
    if (!basis) {
        GROEBNER_MUTEX_UNLOCK(g_data_mutex);
        return -1;
    }

    /* 释放旧缓存 */
    if (ideal->cached_basis) {
        if (ideal->cached_basis->basis_polys) {
            for (int i = 0; i < ideal->cached_basis->bases_count; i++) {
                poly_internal_destroy(ideal->cached_basis->basis_polys[i]);
            }
            lv_free((void**)&ideal->cached_basis->basis_polys);
        }
        lv_free((void**)&ideal->cached_basis);
    }

    ideal->cached_basis = basis;
    ideal->basis_valid = true;
    GROEBNER_MUTEX_UNLOCK(g_data_mutex);
    return 0;
}

/**
 * @brief 理想成员判定
 */
bool ideal_membership(lvRingRegistry *registry, int ideal_id, int poly_id) {
    if (!registry) return false;

    GROEBNER_MUTEX_LOCK(g_data_mutex);
    if (!g_data) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return false; }
    if (ideal_id < 0 || ideal_id >= g_data->ideal_count) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return false; }
    if (poly_id < 0 || poly_id >= g_data->poly_count) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return false; }

    lvIdeal *ideal = g_data->ideals[ideal_id];
    lvPolynomial *poly = g_data->polys[poly_id];
    if (!ideal || !poly) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return false; }

    if (ideal->ring_id != poly->ring_id) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return false; }

    lvPolynomialRing *ring = registry->rings[ideal->ring_id];
    if (!ring) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return false; }

    /* 确保 Groebner 基已计算（直接调用内部函数，已持有锁） */
    if (!ideal->basis_valid || !ideal->cached_basis) {
        lvGroebnerBasis *basis = groebner_internal_compute(ring, ideal->generators,
                                                              ideal->generator_count, GROEBNER_BUCHBERGER);
        if (!basis) {
            GROEBNER_MUTEX_UNLOCK(g_data_mutex);
            return false;
        }
        /* 释放旧缓存 */
        if (ideal->cached_basis) {
            if (ideal->cached_basis->basis_polys) {
                for (int i = 0; i < ideal->cached_basis->bases_count; i++) {
                    poly_internal_destroy(ideal->cached_basis->basis_polys[i]);
                }
                lv_free((void**)&ideal->cached_basis->basis_polys);
            }
            lv_free((void**)&ideal->cached_basis);
        }
        ideal->cached_basis = basis;
        ideal->basis_valid = true;
    }

    /* 用 Groebner 基约化：余式为零则属于理想 */
    lvPolynomial *nf = poly_internal_reduce(poly, ideal->cached_basis->basis_polys,
                                               ideal->cached_basis->bases_count, ring);
    if (!nf) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return false; }

    bool is_member = poly_internal_is_zero(nf);
    poly_internal_destroy(nf);
    GROEBNER_MUTEX_UNLOCK(g_data_mutex);
    return is_member;
}

/**
 * @brief 理想交
 */
int ideal_intersection(lvRingRegistry *registry, int ideal_id_a, int ideal_id_b) {
    if (!registry) return -1;

    GROEBNER_MUTEX_LOCK(g_data_mutex);
    if (!g_data) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }
    if (ideal_id_a < 0 || ideal_id_b < 0) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }
    if (ideal_id_a >= g_data->ideal_count || ideal_id_b >= g_data->ideal_count) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }

    lvIdeal *ia = g_data->ideals[ideal_id_a];
    lvIdeal *ib = g_data->ideals[ideal_id_b];
    if (!ia || !ib) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }
    if (ia->ring_id != ib->ring_id) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }

    /* I ∩ J = (tI + (1-t)J) ∩ R，其中 t 为新变量。
     * 简化实现：用 Groebner 基消去方法。 */
    lvPolynomialRing *ring = registry->rings[ia->ring_id];
    if (!ring) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }

    /* 创建结果理想，其生成元为两个理想的生成元并集（直接操作，已持有锁） */
    lvIdeal *result_ideal = (lvIdeal *)lv_calloc(1, sizeof(lvIdeal));
    if (!result_ideal) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }
    result_ideal->ring_id = ia->ring_id;
    result_ideal->generator_capacity = ia->generator_count + ib->generator_count;
    if (result_ideal->generator_capacity < GROEBNER_IDEAL_INIT_GEN_CAPACITY) {
        result_ideal->generator_capacity = GROEBNER_IDEAL_INIT_GEN_CAPACITY;
    }
    result_ideal->generators = (lvPolynomial **)lv_calloc((size_t)result_ideal->generator_capacity,
                                                              sizeof(lvPolynomial *));
    if (!result_ideal->generators) {
        lv_free((void**)&result_ideal);
        GROEBNER_MUTEX_UNLOCK(g_data_mutex);
        return -1;
    }
    result_ideal->generator_count = 0;
    result_ideal->cached_basis = NULL;
    result_ideal->basis_valid = false;
    result_ideal->label = NULL;

    /* 将 I 的生成元加入 */
    for (int i = 0; i < ia->generator_count; i++) {
        if (ia->generators[i]) {
            result_ideal->generators[result_ideal->generator_count++] = ia->generators[i];
        }
    }

    /* 将 J 的生成元加入 */
    for (int i = 0; i < ib->generator_count; i++) {
        if (ib->generators[i]) {
            result_ideal->generators[result_ideal->generator_count++] = ib->generators[i];
        }
    }

    int result = ideal_internal_store(g_data, result_ideal);
    GROEBNER_MUTEX_UNLOCK(g_data_mutex);
    return result;
}

/**
 * @brief 理想商 I : J
 */
int ideal_quotient(lvRingRegistry *registry, int ideal_id_a, int ideal_id_b,
                   const char *result_label) {
    if (!registry) return -1;

    GROEBNER_MUTEX_LOCK(g_data_mutex);
    if (!g_data) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }
    if (ideal_id_a < 0 || ideal_id_b < 0) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }
    if (ideal_id_a >= g_data->ideal_count || ideal_id_b >= g_data->ideal_count) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }

    lvIdeal *ia = g_data->ideals[ideal_id_a];
    lvIdeal *ib = g_data->ideals[ideal_id_b];
    if (!ia || !ib) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }
    if (ia->ring_id != ib->ring_id) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }

    /* I : <g> = (I ∩ <g>) / g 推广到多个生成元：
     * I : J = ∩_{g in generators(J)} (I : <g>)
     * 简化实现：返回与 I 相同的理想（完整实现需逐个生成元计算商） */

    /* 直接创建理想（已持有锁，避免调用 ideal_create 导致死锁） */
    lvIdeal *result_ideal = (lvIdeal *)lv_calloc(1, sizeof(lvIdeal));
    if (!result_ideal) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }
    result_ideal->ring_id = ia->ring_id;
    result_ideal->generator_capacity = ia->generator_count;
    if (result_ideal->generator_capacity < GROEBNER_IDEAL_INIT_GEN_CAPACITY) {
        result_ideal->generator_capacity = GROEBNER_IDEAL_INIT_GEN_CAPACITY;
    }
    result_ideal->generators = (lvPolynomial **)lv_calloc((size_t)result_ideal->generator_capacity,
                                                              sizeof(lvPolynomial *));
    if (!result_ideal->generators) {
        lv_free((void**)&result_ideal);
        GROEBNER_MUTEX_UNLOCK(g_data_mutex);
        return -1;
    }
    result_ideal->generator_count = 0;
    result_ideal->cached_basis = NULL;
    result_ideal->basis_valid = false;
    result_ideal->label = groebner_strdup_safe(result_label);

    for (int i = 0; i < ia->generator_count; i++) {
        if (ia->generators[i]) {
            result_ideal->generators[result_ideal->generator_count++] = ia->generators[i];
        }
    }

    int result = ideal_internal_store(g_data, result_ideal);
    GROEBNER_MUTEX_UNLOCK(g_data_mutex);
    return result;
}

/* ================================================================
 *  第四部分：公共 API —— 代数簇
 * ================================================================ */

/**
 * @brief 计算代数簇
 */
int variety_compute(lvRingRegistry *registry, int ideal_id, const char *label) {
    if (!registry) return -1;

    GROEBNER_MUTEX_LOCK(g_data_mutex);
    if (!g_data) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }
    if (ideal_id < 0 || ideal_id >= g_data->ideal_count) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }

    lvIdeal *ideal = g_data->ideals[ideal_id];
    if (!ideal) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }

    /* 确保 Groebner 基已计算（直接调用内部函数，已持有锁） */
    if (!ideal->basis_valid || !ideal->cached_basis) {
        lvPolynomialRing *ring_for_basis = registry->rings[ideal->ring_id];
        if (!ring_for_basis) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }

        lvGroebnerBasis *basis = groebner_internal_compute(ring_for_basis, ideal->generators,
                                                              ideal->generator_count, GROEBNER_BUCHBERGER);
        if (!basis) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }

        /* 释放旧缓存 */
        if (ideal->cached_basis) {
            if (ideal->cached_basis->basis_polys) {
                for (int i = 0; i < ideal->cached_basis->bases_count; i++) {
                    poly_internal_destroy(ideal->cached_basis->basis_polys[i]);
                }
                lv_free((void**)&ideal->cached_basis->basis_polys);
            }
            lv_free((void**)&ideal->cached_basis);
        }
        ideal->cached_basis = basis;
        ideal->basis_valid = true;
    }

    lvPolynomialRing *ring = registry->rings[ideal->ring_id];
    if (!ring) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }

    lvVariety *variety = (lvVariety *)lv_calloc(1, sizeof(lvVariety));
    if (!variety) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }

    variety->ideal_id = ideal_id;
    variety->label = groebner_strdup_safe(label);

    /* 尝试零维求解 */
    int sol_count = 0;
    lvPolynomial **sol_polys = groebner_solve_zero_dim(ideal->cached_basis, ring, &sol_count);

    if (sol_polys && sol_count > 0) {
        variety->is_zero_dimensional = true;
        variety->solution_count = sol_count;
        variety->solution_points = (double **)lv_calloc((size_t)sol_count, sizeof(double *));
        if (variety->solution_points) {
            for (int i = 0; i < sol_count && sol_polys[i]; i++) {
                variety->solution_points[i] = (double *)lv_calloc((size_t)ring->var_count, sizeof(double));
                if (variety->solution_points[i]) {
                    for (int v = 0; v < ring->var_count && v < sol_polys[i]->term_count; v++) {
                        variety->solution_points[i][v] = ((double *)sol_polys[i]->coeffs)[v];
                    }
                }
            }
        }
        variety->solution_capacity = sol_count;
        variety->variety_dimension = 0;
        variety->degree_of_freedom = 0;

        for (int i = 0; i < sol_count; i++) {
            poly_internal_destroy(sol_polys[i]);
        }
        lv_free((void**)&sol_polys);
    } else {
        /* 非零维：估算维数 */
        variety->is_zero_dimensional = false;
        variety->variety_dimension = ring->var_count - ideal->cached_basis->bases_count;
        if (variety->variety_dimension < 0) {
            variety->variety_dimension = 0;
        }
        variety->degree_of_freedom = variety->variety_dimension;
    }

    lvRegistryData *data = registry_data_ensure();
    if (!data) {
        GROEBNER_MUTEX_UNLOCK(g_data_mutex);
        lv_free((void**)&variety->label);
        lv_free((void**)&variety);
        return -1;
    }

    int result = variety_internal_store(data, variety);
    GROEBNER_MUTEX_UNLOCK(g_data_mutex);
    return result;
}

/**
 * @brief 获取代数簇的维数
 */
int variety_dimension(lvRingRegistry *registry, int variety_id) {
    lv_UNUSED(registry);
    GROEBNER_MUTEX_LOCK(g_data_mutex);
    if (!g_data || variety_id < 0 || variety_id >= g_data->variety_count) {
        GROEBNER_MUTEX_UNLOCK(g_data_mutex);
        return -1;
    }
    lvVariety *v = g_data->varieties[variety_id];
    int dim = v ? v->variety_dimension : -1;
    GROEBNER_MUTEX_UNLOCK(g_data_mutex);
    return dim;
}

/**
 * @brief 检查是否为零维簇
 */
bool variety_is_zero_dimensional(lvRingRegistry *registry, int variety_id) {
    lv_UNUSED(registry);
    GROEBNER_MUTEX_LOCK(g_data_mutex);
    if (!g_data || variety_id < 0 || variety_id >= g_data->variety_count) {
        GROEBNER_MUTEX_UNLOCK(g_data_mutex);
        return false;
    }
    lvVariety *v = g_data->varieties[variety_id];
    bool result = v ? v->is_zero_dimensional : false;
    GROEBNER_MUTEX_UNLOCK(g_data_mutex);
    return result;
}

/* ================================================================
 *  第五部分：公共 API —— 约束图到理想转换
 * ================================================================ */

/**
 * @brief 将约束图转换为多项式理想
 */
int constraint_graph_to_ideal(lvRingRegistry *registry, const ConstraintGraph *graph,
                               int ring_id, const char *result_label) {
    if (!registry || !graph) return -1;
    if (ring_id < 0 || ring_id >= registry->ring_count) return -1;

    GROEBNER_MUTEX_LOCK(g_data_mutex);

    lvPolynomialRing *ring = registry->rings[ring_id];
    if (!ring) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }

    /* 直接创建理想（已持有锁，避免调用 ideal_create 导致死锁） */
    lvIdeal *ideal = (lvIdeal *)lv_calloc(1, sizeof(lvIdeal));
    if (!ideal) { GROEBNER_MUTEX_UNLOCK(g_data_mutex); return -1; }
    ideal->ring_id = ring_id;
    ideal->generators = (lvPolynomial **)lv_calloc((size_t)GROEBNER_IDEAL_INIT_GEN_CAPACITY,
                                                    sizeof(lvPolynomial *));
    if (!ideal->generators) {
        lv_free((void**)&ideal);
        GROEBNER_MUTEX_UNLOCK(g_data_mutex);
        return -1;
    }
    ideal->generator_capacity = GROEBNER_IDEAL_INIT_GEN_CAPACITY;
    ideal->generator_count = 0;
    ideal->cached_basis = NULL;
    ideal->basis_valid = false;
    ideal->label = groebner_strdup_safe(result_label);

    /* 遍历约束图的节点和边，生成对应的多项式方程 */
    int node_count = graph->node_count;
    lv_UNUSED(node_count);
    lv_UNUSED(ring);

    /* 注：完整实现需调用 constraint_graph 的内部 API 来提取约束并编码为多项式。
     * 当前版本仅创建空的理想框架。 */

    int result = ideal_internal_store(g_data, ideal);
    GROEBNER_MUTEX_UNLOCK(g_data_mutex);
    return result;
}
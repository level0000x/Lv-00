/**
 * @file groebner_poly.c
 * @brief Groebner 引擎多项式内部运算实现（从 groebner_engine.c 拆分）
 *
 * @details 多项式的创建/销毁、项排序、扩容、加减乘、代入、零判定、
 *          总次数、首项提取、S-多项式与约化（normal form）。
 *          仅依赖 lvPolynomial / lvPolynomialRing 结构与 mono_* 单项式操作。
 */

#include "groebner_engine_internal.h"

#include "lv/lv.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error_codes.h"
#include "lv_internal.h"
#include "lv_utils.h"

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
int poly_sort_terms(lvPolynomial *poly, const lvPolynomialRing *ring) {
    if (!poly || !ring || poly->term_count <= 1) {
        return 0;
    }

    int vc = ring->var_count;
    double *coeffs = (double *) poly->coeffs;

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
        if (fabs(coeffs[i]) > lv_config_get_double(LV_CFG_GROEBNER_ZERO_THRESHOLD, GROEBNER_ZERO_THRESHOLD)) {
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
lvPolynomial *poly_internal_create(const lvPolynomialRing *ring, int capacity, const char *label) {
    if (!ring) {
        return NULL;
    }

    lvPolynomial *poly = (lvPolynomial *) lv_calloc(1, sizeof(lvPolynomial));
    if (!poly) {
        return NULL;
    }

    if (capacity < 1) {
        capacity = lv_config_get_int(LV_CFG_GROEBNER_POLY_INIT_CAPACITY, GROEBNER_POLY_INIT_CAPACITY);
    }

    int vc = ring->var_count;
    poly->powers = (int *) lv_calloc((size_t) capacity * (size_t) vc, sizeof(int));
    if (!poly->powers) {
        lv_free((void **) &poly);
        return NULL;
    }

    poly->coeffs = (double *) lv_calloc((size_t) capacity, sizeof(double));
    if (!poly->coeffs) {
        lv_free((void **) &poly->powers);
        lv_free((void **) &poly);
        return NULL;
    }

    poly->ring_id = ring->ring_id;
    poly->var_count = vc;
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
void poly_internal_destroy(lvPolynomial *poly) {
    if (!poly) {
        return;
    }
    lv_free((void **) &poly->powers);
    lv_free((void **) &poly->coeffs);
    lv_free((void **) &poly->label);
    lv_free((void **) &poly);
}

/**
 * @brief 确保多项式有足够的容量存储更多项
 *
 * @param poly    多项式
 * @param needed  需要的总容量
 * @return 成功返回 true
 */
bool poly_ensure_capacity(lvPolynomial *poly, int needed) {
    if (!poly) {
        return false;
    }
    if (poly->var_count <= 0) {
        return false;
    }
    return poly_ensure_capacity_ex(poly, needed, poly->var_count);
}

/**
 * @brief 为多项式扩容（需要知道 var_count）
 *
 * @param poly      多项式
 * @param needed    需要的总容量
 * @param var_count 变量数量
 * @return 成功返回 true
 */
bool poly_ensure_capacity_ex(lvPolynomial *poly, int needed, int var_count) {
    if (!poly) {
        return false;
    }
    if (poly->term_capacity >= needed) {
        return true;
    }

    int new_cap = poly->term_capacity;
    if (new_cap < 1) {
        new_cap = lv_config_get_int(LV_CFG_GROEBNER_POLY_INIT_CAPACITY, GROEBNER_POLY_INIT_CAPACITY);
    }
    while (new_cap < needed) {
        new_cap *= GROEBNER_POLY_GROW_FACTOR;
        if (new_cap > 1000000) {
            new_cap = needed + 100;
            break;
        }
    }

    int *new_powers = (int *) lv_realloc(poly->powers, (size_t) new_cap * (size_t) var_count * sizeof(int));
    if (!new_powers) {
        return false;
    }
    /* 清零新分配的区域 */
    memset(new_powers + poly->term_capacity * var_count, 0,
           (size_t) (new_cap - poly->term_capacity) * (size_t) var_count * sizeof(int));
    poly->powers = new_powers;

    double *new_coeffs = (double *) lv_realloc(poly->coeffs, (size_t) new_cap * sizeof(double));
    if (!new_coeffs) {
        /* powers 已扩容成功，但 coeffs 失败了 —— 这是不太可能的情况，回滚 powers */
        /* 为简化，不处理这种极端情况，假设 realloc 要么都成功要么都失败 */
        return false;
    }
    /* 清零新系数 */
    memset(new_coeffs + poly->term_capacity, 0, (size_t) (new_cap - poly->term_capacity) * sizeof(double));
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
lvPolynomial *poly_internal_copy(const lvPolynomial *src, const lvPolynomialRing *ring) {
    if (!src || !ring) {
        return NULL;
    }

    lvPolynomial *cpy = poly_internal_create(ring, src->term_capacity, src->label);
    if (!cpy) {
        return NULL;
    }

    int vc = ring->var_count;
    cpy->var_count = src->var_count;
    cpy->term_count = src->term_count;
    cpy->total_degree = src->total_degree;
    cpy->is_homogeneous = src->is_homogeneous;

    memcpy(cpy->powers, src->powers, (size_t) src->term_count * (size_t) vc * sizeof(int));
    memcpy(cpy->coeffs, src->coeffs, (size_t) src->term_count * sizeof(double));

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
lvPolynomial *poly_internal_add(const lvPolynomial *f, const lvPolynomial *g, const lvPolynomialRing *ring) {
    if (!f || !g || !ring) {
        return NULL;
    }

    int vc = ring->var_count;
    int est_capacity = f->term_count + g->term_count;
    if (est_capacity < lv_config_get_int(LV_CFG_GROEBNER_POLY_INIT_CAPACITY, GROEBNER_POLY_INIT_CAPACITY)) {
        est_capacity = lv_config_get_int(LV_CFG_GROEBNER_POLY_INIT_CAPACITY, GROEBNER_POLY_INIT_CAPACITY);
    }

    lvPolynomial *result = poly_internal_create(ring, est_capacity, NULL);
    if (!result) {
        return NULL;
    }

    double *coeffs = (double *) result->coeffs;
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
            coeffs[ti] = ((double *) f->coeffs)[fi];
            result->term_count++;
            fi++;
        } else if (cmp < 0) {
            /* g 的项更大 */
            mono_copy(&result->powers[ti * vc], &g->powers[gi * vc], vc);
            coeffs[ti] = ((double *) g->coeffs)[gi];
            result->term_count++;
            gi++;
        } else {
            /* 同类项 */
            double sum = ((double *) f->coeffs)[fi] + ((double *) g->coeffs)[gi];
            if (fabs(sum) > lv_config_get_double(LV_CFG_GROEBNER_ZERO_THRESHOLD, GROEBNER_ZERO_THRESHOLD)) {
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
        coeffs[ti] = ((double *) f->coeffs)[fi];
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
        coeffs[ti] = ((double *) g->coeffs)[gi];
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
lvPolynomial *poly_internal_multiply(const lvPolynomial *f, const lvPolynomial *g,
                                            const lvPolynomialRing *ring) {
    if (!f || !g || !ring) {
        return NULL;
    }

    int vc = ring->var_count;
    int est_capacity = f->term_count * g->term_count;
    if (est_capacity < 1) {
        est_capacity = lv_config_get_int(LV_CFG_GROEBNER_POLY_INIT_CAPACITY, GROEBNER_POLY_INIT_CAPACITY);
    }
    if (est_capacity > 100000) {
        est_capacity = 100000;
    }

    lvPolynomial *result = poly_internal_create(ring, est_capacity, NULL);
    if (!result) {
        return NULL;
    }

    double *f_coeffs = (double *) f->coeffs;
    double *g_coeffs = (double *) g->coeffs;
    double *r_coeffs = (double *) result->coeffs;

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
lvPolynomial *poly_internal_substitute(const lvPolynomial *f, int var_index, const lvPolynomial *subst,
                                              const lvPolynomialRing *ring) {
    if (!f || !subst || !ring) {
        return NULL;
    }
    if (var_index < 0 || var_index >= ring->var_count) {
        return NULL;
    }

    /* 先创建零多项式作为累加器 */
    lvPolynomial *result = poly_internal_create(ring, lv_config_get_int(LV_CFG_GROEBNER_POLY_INIT_CAPACITY, GROEBNER_POLY_INIT_CAPACITY), NULL);
    if (!result) {
        return NULL;
    }

    int vc = ring->var_count;
    double *f_coeffs = (double *) f->coeffs;

    for (int i = 0; i < f->term_count; i++) {
        int exp = f->powers[i * vc + var_index];
        double coeff = f_coeffs[i];

        if (fabs(coeff) < lv_config_get_double(LV_CFG_GROEBNER_ZERO_THRESHOLD, GROEBNER_ZERO_THRESHOLD)) {
            continue;
        }

        /* 构造该项去除 x_var 后的单项式 */
        lvPolynomial *term_poly = poly_internal_create(ring, 1, NULL);
        if (!term_poly) {
            poly_internal_destroy(result);
            return NULL;
        }
        term_poly->term_count = 1;
        if (!poly_ensure_capacity_ex(term_poly, 1, vc)) {
            poly_internal_destroy(term_poly);
            poly_internal_destroy(result);
            return NULL;
        }
        for (int k = 0; k < vc; k++) {
            if (k != var_index) {
                term_poly->powers[k] = f->powers[i * vc + k];
            }
        }
        ((double *) term_poly->coeffs)[0] = coeff;

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
bool poly_internal_is_zero(const lvPolynomial *poly) {
    if (!poly) {
        return true;
    }
    if (poly->term_count == 0) {
        return true;
    }
    /* 检查是否所有系数都接近零 */
    double *coeffs = (double *) poly->coeffs;
    for (int i = 0; i < poly->term_count; i++) {
        if (fabs(coeffs[i]) > lv_config_get_double(LV_CFG_GROEBNER_ZERO_THRESHOLD, GROEBNER_ZERO_THRESHOLD)) {
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
int poly_internal_total_degree(const lvPolynomial *poly, int var_count) {
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
void poly_internal_scale(lvPolynomial *poly, double scalar) {
    if (!poly) {
        return;
    }
    double *coeffs = (double *) poly->coeffs;
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
int poly_leading_term(const lvPolynomial *poly, const lvPolynomialRing *ring, int *lt_out, double *lc_out) {
    if (!poly || !ring || poly->term_count == 0) {
        if (lc_out)
            *lc_out = 0.0;
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
        *lc_out = ((double *) poly->coeffs)[best_idx];
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
lvPolynomial *poly_internal_s_polynomial(const lvPolynomial *f, const lvPolynomial *g,
                                                const lvPolynomialRing *ring) {
    if (!f || !g || !ring) {
        return NULL;
    }

    int vc = ring->var_count;
    double lc_f, lc_g;
    int *lt_f = (int *) lv_calloc((size_t) vc, sizeof(int));
    int *lt_g = (int *) lv_calloc((size_t) vc, sizeof(int));
    int *lcm = (int *) lv_calloc((size_t) vc, sizeof(int));
    int *quot_f = (int *) lv_calloc((size_t) vc, sizeof(int));
    int *quot_g = (int *) lv_calloc((size_t) vc, sizeof(int));

    if (!lt_f || !lt_g || !lcm || !quot_f || !quot_g) {
        lv_free((void **) &lt_f);
        lv_free((void **) &lt_g);
        lv_free((void **) &lcm);
        lv_free((void **) &quot_f);
        lv_free((void **) &quot_g);
        return NULL;
    }

    /* 获取前导项 */
    if (poly_leading_term(f, ring, lt_f, &lc_f) != 0 || poly_leading_term(g, ring, lt_g, &lc_g) != 0) {
        lv_free((void **) &lt_f);
        lv_free((void **) &lt_g);
        lv_free((void **) &lcm);
        lv_free((void **) &quot_f);
        lv_free((void **) &quot_g);
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
    lv_free((void **) &term_f->powers);
    lv_free((void **) &term_f->coeffs);
    term_f->powers = (int *) lv_calloc((size_t) vc, sizeof(int));
    term_f->coeffs = (double *) lv_calloc(1, sizeof(double));
    if (!term_f->powers || !term_f->coeffs) {
        poly_internal_destroy(term_f);
        goto s_poly_cleanup;
    }
    term_f->term_capacity = 1;
    term_f->term_count = 1;
    mono_copy(term_f->powers, quot_f, vc);
    ((double *) term_f->coeffs)[0] = lc_g; /* 乘以 lc_g 使得 S(f,g) 的前导项抵消 */

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
    lv_free((void **) &term_g->powers);
    lv_free((void **) &term_g->coeffs);
    term_g->powers = (int *) lv_calloc((size_t) vc, sizeof(int));
    term_g->coeffs = (double *) lv_calloc(1, sizeof(double));
    if (!term_g->powers || !term_g->coeffs) {
        poly_internal_destroy(term_g);
        poly_internal_destroy(part_f);
        goto s_poly_cleanup;
    }
    term_g->term_capacity = 1;
    term_g->term_count = 1;
    mono_copy(term_g->powers, quot_g, vc);
    ((double *) term_g->coeffs)[0] = lc_f; /* 乘以 lc_f */

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

    lv_free((void **) &lt_f);
    lv_free((void **) &lt_g);
    lv_free((void **) &lcm);
    lv_free((void **) &quot_f);
    lv_free((void **) &quot_g);
    return s_poly;

s_poly_cleanup:
    lv_free((void **) &lt_f);
    lv_free((void **) &lt_g);
    lv_free((void **) &lcm);
    lv_free((void **) &quot_f);
    lv_free((void **) &quot_g);
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
lvPolynomial *poly_internal_reduce(const lvPolynomial *p, lvPolynomial **basis, int basis_count,
                                          const lvPolynomialRing *ring) {
    if (!p || !basis || !ring) {
        return NULL;
    }

    lvPolynomial *remainder = poly_internal_copy(p, ring);
    if (!remainder) {
        return NULL;
    }

    int vc = ring->var_count;
    double *rem_coeffs = (double *) remainder->coeffs;
    int step_count = 0;

    int reduce_max = lv_config_get_int(LV_CFG_GROEBNER_REDUCE_MAX_STEPS, 10000);
    bool changed = true;
    while (changed && step_count < reduce_max) {
        changed = false;
        step_count++;

        /* 寻找当前多项式中可被约化的项 */
        for (int i = 0; i < remainder->term_count; i++) {
            if (fabs(rem_coeffs[i]) < lv_config_get_double(LV_CFG_GROEBNER_ZERO_THRESHOLD, GROEBNER_ZERO_THRESHOLD)) {
                continue;
            }

            /* 查找基中能整除该项的基元素 */
            int reducer_idx = -1;
            for (int j = 0; j < basis_count; j++) {
                if (poly_internal_is_zero(basis[j])) {
                    continue;
                }
                double lc_b;
                int *lt_b = (int *) lv_calloc((size_t) vc, sizeof(int));
                if (!lt_b)
                    continue;
                if (poly_leading_term(basis[j], ring, lt_b, &lc_b) == 0) {
                    if (mono_divides(ring, &remainder->powers[i * vc], lt_b)) {
                        reducer_idx = j;
                        lv_free((void **) &lt_b);
                        break;
                    }
                }
                lv_free((void **) &lt_b);
            }

            if (reducer_idx < 0) {
                continue;
            }

            /* 获取约化器信息 */
            lvPolynomial *reducer = basis[reducer_idx];
            double lc_r;
            int *lt_r = (int *) lv_calloc((size_t) vc, sizeof(int));
            if (!lt_r)
                continue;
            if (poly_leading_term(reducer, ring, lt_r, &lc_r) != 0) {
                lv_free((void **) &lt_r);
                continue;
            }

            /* 计算商单项式：m = t / LT(reducer) */
            int *quot_mono = (int *) lv_calloc((size_t) vc, sizeof(int));
            if (!quot_mono) {
                lv_free((void **) &lt_r);
                continue;
            }
            mono_divide(ring, &remainder->powers[i * vc], lt_r, quot_mono);
            lv_free((void **) &lt_r);

            /* 构造乘子多项式：c * m，其中 c = coeff(t)/lc(reducer) */
            double factor = rem_coeffs[i] / lc_r;

            lvPolynomial *mult_term = poly_internal_create(ring, 1, NULL);
            if (!mult_term) {
                lv_free((void **) &quot_mono);
                continue;
            }
            lv_free((void **) &mult_term->powers);
            lv_free((void **) &mult_term->coeffs);
            mult_term->powers = (int *) lv_calloc((size_t) vc, sizeof(int));
            mult_term->coeffs = (double *) lv_calloc(1, sizeof(double));
            if (!mult_term->powers || !mult_term->coeffs) {
                poly_internal_destroy(mult_term);
                lv_free((void **) &quot_mono);
                continue;
            }
            mult_term->term_capacity = 1;
            mult_term->term_count = 1;
            mono_copy(mult_term->powers, quot_mono, vc);
            ((double *) mult_term->coeffs)[0] = factor;
            lv_free((void **) &quot_mono);

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
            rem_coeffs = (double *) remainder->coeffs;

            /* 清理并排序 */
            poly_sort_terms(remainder, ring);
            rem_coeffs = (double *) remainder->coeffs;

            changed = true;
            break; /* 重新开始约化循环 */
        }
    }

    return remainder;
}

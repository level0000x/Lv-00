/**
 * @file default_host_ops.c
 * @brief 主机端通用数值 ops 默认实现 —— 唯一实现
 *
 * @details 消除 SERIAL / CUDA / HIP 后端中重复的主机端（host-side）
 *          数据搬移 ops。本文件只实现与硬件无关的纯内存操作：
 *          - lvVector : clone / destroy / zero / const_set / copy /
 *                       abs / inv / compare / length / data_ptr
 *          - lvMatrix : clone / destroy / zero / copy / scale /
 *                       set_element / get_element
 *
 *          计算型 ops（dot / norm / max_norm / wrms_norm / matvec /
 *          factor / solve / linear_sum 等）与 create 均保持在各后端
 *          内部：前者与具体硬件（CPU/GPU 内核）相关，后者需要把
 *          创建对象绑定到后端自己的操作表。
 *
 * @author Lv-00 Project
 * @version v1.0.0
 * @date 2026-07-31
 *
 * @dependencies
 *   - lv/default_host_ops.h : 本文件接口声明
 *   - lv/numerical_backend.h : lvVector / lvMatrix 类型与操作表定义
 *   - lv/lv_utils.h          : 统一内存分配器 lv_calloc / lv_malloc / lv_free
 *   - lv/error_codes.h       : 统一错误码与检查宏
 */

/* ========================================================================
 * 包含头文件
 * ======================================================================== */

#include "lv/default_host_ops.h"

#include <math.h>
#include <string.h>

#include "lv/error_codes.h"
#include "lv/lv_utils.h"
#include "lv/lv_numeric.h"

/* ========================================================================
 * 向量主机端实现（硬件无关）
 * ======================================================================== */

/**
 * @brief 深拷贝向量
 *
 * clone->ops 与 clone->backend 继承源向量（与 SUNDIALS N_VClone 模式一致），
 * 保证克隆对象仍绑定到源后端操作表。
 */
lvVector *default_vector_clone(const lvVector *v) {
    lv_CHECK_NULL(v, NULL);

    int64_t n = v->length;
    lvVector *clone = lv_calloc(1, sizeof(lvVector));
    lv_CHECK_ALLOC(clone, NULL);

    clone->length = n;
    clone->backend = v->backend;
    clone->ops = v->ops;
    clone->backend_data = NULL;

    clone->data = lv_calloc((size_t) n, sizeof(double));
    if (!clone->data) {
        lv_free((void **) &clone);
        lv_ERROR_SET(lv_ERROR_OUT_OF_MEMORY, "向量数据分配失败，长度=%lld", (long long) n);
        return NULL;
    }
    memcpy(clone->data, v->data, (size_t) n * sizeof(double));

    return clone;
}

/**
 * @brief 销毁向量
 */
void default_vector_destroy(lvVector *v) {
    if (!v) {
        return;
    }
    if (v->data) {
        lv_free((void **) &v->data);
    }
    lv_free((void **) &v);
}

/**
 * @brief 置零
 */
void default_vector_zero(lvVector *v) {
    if (!v || !v->data) {
        return;
    }
    memset(v->data, 0, (size_t) v->length * sizeof(double));
}

/**
 * @brief 设为常量 c
 */
void default_vector_const_set(lvVector *v, double c) {
    if (!v || !v->data) {
        return;
    }
    for (int64_t i = 0; i < v->length; ++i) {
        v->data[i] = c;
    }
}

/**
 * @brief 深拷贝：dst = src
 */
void default_vector_copy(lvVector *dst, const lvVector *src) {
    if (!dst || !src || !dst->data || !src->data) {
        return;
    }
    int64_t n = (dst->length < src->length) ? dst->length : src->length;
    memcpy(dst->data, src->data, (size_t) n * sizeof(double));
}

/**
 * @brief 逐元素绝对值：v_i = |v_i|
 */
void default_vector_abs(lvVector *v) {
    if (!v || !v->data) {
        return;
    }
    for (int64_t i = 0; i < v->length; ++i) {
        v->data[i] = fabs(v->data[i]);
    }
}

/**
 * @brief 逐元素除法：v_i = v_i / d_i
 */
void default_vector_inv(lvVector *v, const lvVector *d) {
    if (!v || !d || !v->data || !d->data) {
        return;
    }
    for (int64_t i = 0; i < v->length; ++i) {
        if (fabs(d->data[i]) > lv_EPSILON_DOUBLE) {
            v->data[i] /= d->data[i];
        } else {
            /* 避免除零：设为大值 */
            v->data[i] = lv_HUGE_NUMBER;
        }
    }
}

/**
 * @brief 逐元素最大值：v_i = max(v_i, c)
 */
void default_vector_compare(lvVector *v, double c) {
    if (!v || !v->data) {
        return;
    }
    for (int64_t i = 0; i < v->length; ++i) {
        if (v->data[i] < c) {
            v->data[i] = c;
        }
    }
}

/**
 * @brief 获取向量长度
 */
int64_t default_vector_length(const lvVector *v) {
    if (!v) {
        return 0;
    }
    return v->length;
}

/**
 * @brief 获取底层原始数据指针
 */
double *default_vector_data_ptr(lvVector *v) {
    if (!v) {
        return NULL;
    }
    return v->data;
}

/* ========================================================================
 * 矩阵主机端实现（硬件无关，列主序稠密）
 * ======================================================================== */

/**
 * @brief 深拷贝矩阵
 *
 * clone->ops 与 clone->backend 继承源矩阵，保证克隆对象仍绑定到
 * 源后端操作表（求解器内部通过 clone->ops->copy/factor/solve 工作）。
 */
lvMatrix *default_matrix_clone(const lvMatrix *A) {
    lv_CHECK_NULL(A, NULL);

    int64_t rows = A->rows;
    int64_t cols = A->cols;
    size_t data_size = (size_t) (rows * cols) * sizeof(double);

    lvMatrix *clone = lv_calloc(1, sizeof(lvMatrix));
    lv_CHECK_ALLOC(clone, NULL);

    clone->rows = rows;
    clone->cols = cols;
    clone->sparse = A->sparse;
    clone->format = A->format;
    clone->backend = A->backend;
    clone->ops = A->ops;
    clone->backend_data = NULL;

    clone->data = lv_malloc(data_size);
    if (!clone->data) {
        lv_free((void **) &clone);
        lv_ERROR_SET(lv_ERROR_OUT_OF_MEMORY, "矩阵数据分配失败 %lldx%lld", (long long) rows, (long long) cols);
        return NULL;
    }
    if (A->data) {
        memcpy(clone->data, A->data, data_size);
    } else {
        memset(clone->data, 0, data_size);
    }

    return clone;
}

/**
 * @brief 销毁矩阵
 */
void default_matrix_destroy(lvMatrix *A) {
    if (!A) {
        return;
    }
    if (A->data) {
        lv_free((void **) &A->data);
    }
    lv_free((void **) &A);
}

/**
 * @brief 置零
 */
void default_matrix_zero(lvMatrix *A) {
    if (!A || !A->data) {
        return;
    }
    memset(A->data, 0, (size_t) (A->rows * A->cols) * sizeof(double));
}

/**
 * @brief 深拷贝：dst = src
 */
void default_matrix_copy(lvMatrix *dst, const lvMatrix *src) {
    if (!dst || !src || !dst->data || !src->data) {
        return;
    }
    int64_t elems = dst->rows * dst->cols;
    int64_t src_elems = src->rows * src->cols;
    int64_t n = (elems < src_elems) ? elems : src_elems;
    memcpy(dst->data, src->data, (size_t) n * sizeof(double));
}

/**
 * @brief 矩阵-标量乘法：A = c * A
 */
void default_matrix_scale(lvMatrix *A, double c) {
    if (!A || !A->data) {
        return;
    }
    int64_t elems = A->rows * A->cols;
    double *data = (double *) A->data;
    for (int64_t i = 0; i < elems; ++i) {
        data[i] *= c;
    }
}

/**
 * @brief 设置单个元素值（列主序）
 */
void default_matrix_set_element(lvMatrix *A, int64_t row, int64_t col, double val) {
    if (!A || !A->data || !lv_index_in_range((int) row, (int) A->rows) || !lv_index_in_range((int) col, (int) A->cols)) {
        return;
    }
    double *data = (double *) A->data;
    data[col * A->rows + row] = val;
}

/**
 * @brief 获取单个元素值（列主序）
 */
double default_matrix_get_element(const lvMatrix *A, int64_t row, int64_t col) {
    if (!A || !A->data || !lv_index_in_range((int) row, (int) A->rows) || !lv_index_in_range((int) col, (int) A->cols)) {
        return 0.0;
    }
    double *data = (double *) A->data;
    return data[col * A->rows + row];
}

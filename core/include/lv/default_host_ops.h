/**
 * @file default_host_ops.h
 * @brief 主机端通用数值 ops 默认实现 —— 公共接口
 *
 * @details 提供硬件无关的主机端（host-side）数据搬移 ops 唯一实现：
 *          - lvVector : clone / destroy / zero / const_set / copy /
 *                       abs / inv / compare / length / data_ptr
 *          - lvMatrix : clone / destroy / zero / copy / scale /
 *                       set_element / get_element
 *
 *          这些 ops 只操作主机内存（v->data / A->data），与具体硬件无关，
 *          由各后端（SERIAL / OpenMP）操作表直接引用，消除重复实现。
 *          注意：计算型 ops（dot / norm / matvec / factor / solve 等）
 *          保持在各后端内部（硬件相关）；create 因需绑定后端操作表
 *          亦由各后端自行实现。
 *
 * @author Lv-00 Project
 * @version v1.0.0
 * @date 2026-07-31
 *
 * @dependencies
 *   - lv/numerical_backend.h : lvVector / lvMatrix 类型与操作表定义
 */
#ifndef lv_DEFAULT_HOST_OPS_H
#define lv_DEFAULT_HOST_OPS_H

#include "lv/numerical_backend.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 向量主机端 ops（硬件无关） ==================== */

/**
 * @brief 深拷贝向量（clone->ops / clone->backend 继承源向量）
 */
lvVector *default_vector_clone(const lvVector *v);

/**
 * @brief 销毁向量并释放所有关联资源
 */
void default_vector_destroy(lvVector *v);

/**
 * @brief 将所有元素设为 0
 */
void default_vector_zero(lvVector *v);

/**
 * @brief 将所有元素设为常量 c
 */
void default_vector_const_set(lvVector *v, double c);

/**
 * @brief 深拷贝：dst = src（按两者较短长度复制）
 */
void default_vector_copy(lvVector *dst, const lvVector *src);

/**
 * @brief 逐元素绝对值：v_i = |v_i|
 */
void default_vector_abs(lvVector *v);

/**
 * @brief 逐元素除法：v_i = v_i / d_i（除数为 0 时置为大值 1e30）
 */
void default_vector_inv(lvVector *v, const lvVector *d);

/**
 * @brief 逐元素最大值：v_i = max(v_i, c)
 */
void default_vector_compare(lvVector *v, double c);

/**
 * @brief 获取向量长度（元素个数）
 */
int64_t default_vector_length(const lvVector *v);

/**
 * @brief 获取底层原始数据指针（主机端为 v->data）
 */
double *default_vector_data_ptr(lvVector *v);

/* ==================== 矩阵主机端 ops（硬件无关） ==================== */

/**
 * @brief 深拷贝矩阵（clone->ops / clone->backend 继承源矩阵）
 */
lvMatrix *default_matrix_clone(const lvMatrix *A);

/**
 * @brief 销毁矩阵并释放所有关联资源
 */
void default_matrix_destroy(lvMatrix *A);

/**
 * @brief 将所有元素设为 0
 */
void default_matrix_zero(lvMatrix *A);

/**
 * @brief 深拷贝：dst = src（按两者较短元素数复制）
 */
void default_matrix_copy(lvMatrix *dst, const lvMatrix *src);

/**
 * @brief 矩阵-标量乘法：A = c * A
 */
void default_matrix_scale(lvMatrix *A, double c);

/**
 * @brief 设置单个元素值（列主序）
 */
void default_matrix_set_element(lvMatrix *A, int64_t row, int64_t col, double val);

/**
 * @brief 获取单个元素值（列主序）
 */
double default_matrix_get_element(const lvMatrix *A, int64_t row, int64_t col);

#ifdef __cplusplus
}
#endif

#endif /* lv_DEFAULT_HOST_OPS_H */

/**
 * @file singular_backend.h
 * @brief Singular 计算机代数后端 API
 *
 * 提供 Singular 多项式环和 Gröbner 基计算接口。
 * 需要 libsingular 库链接。
 */

#ifndef lv_SINGULAR_BACKEND_H
#define lv_SINGULAR_BACKEND_H

#include "lv/numerical_backend.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 注册 Singular 后端操作表到后端注册表 */
int lv_singular_register_backend(void);

/** @brief 检查 Singular 库是否可用 */
int lv_singular_available(void);

/** @brief 获取 Singular 版本字符串 */
const char *lv_singular_backend_version(void);

/* ── 高层代数 API（句柄均为内部引擎对象指针，void* 传递） ── */

/**
 * @brief Singular 变量描述符
 *
 * 表示多项式环中的一个变量（未知数），用于将问题域中的符号
 * 映射到 Singular 多项式环中的索引。
 */
typedef struct {
    int id;        /**< 变量在环中的索引 (0-based) */
    char name[64]; /**< 变量名称（如 "x", "y", "t1" 等） */
    double val;    /**< 变量在当前解中的数值（若已知），否则为 0.0 */
} SingularVar;

/**
 * @brief 计算给定理想的 Gröbner 基并返回标准型
 *
 * @param ideal_ptr 输入理想句柄（SingularIdeal*）
 * @return Gröbner 基句柄（SingularIdeal*），失败返回 NULL
 */
void *lv_singular_groebner_basis(void *ideal_ptr);

/**
 * @brief 计算两个理想的交 I ∩ J
 *
 * @param ideal_a 理想 A 句柄（SingularIdeal*）
 * @param ideal_b 理想 B 句柄（SingularIdeal*）
 * @return I ∩ J 的理想句柄（SingularIdeal*），失败返回 NULL
 */
void *lv_singular_ideal_intersect(void *ideal_a, void *ideal_b);

/**
 * @brief 计算两个理想的商 I : J
 *
 * @param ideal_a 理想 A 句柄（SingularIdeal*）
 * @param ideal_b 理想 B 句柄（SingularIdeal*）
 * @return I : J 的理想句柄（SingularIdeal*），失败返回 NULL
 */
void *lv_singular_ideal_quotient_op(void *ideal_a, void *ideal_b);

/**
 * @brief 检查多项式是否属于理想
 *
 * @param poly   多项式句柄（SingularPoly*）
 * @param ideal  理想句柄（SingularIdeal*）
 * @return 1 表示属于，0 表示不属于，-1 表示出错
 */
int lv_singular_membership_test(void *poly, void *ideal);

/**
 * @brief 从约束图构建多项式理想
 *
 * @param constraint_ids 约束 ID 数组
 * @param n_constraints  约束个数
 * @param vars           变量描述符数组（提供变量名和数值）
 * @param n_vars         变量个数
 * @return 理想句柄（SingularIdeal*），失败返回 NULL
 */
void *lv_singular_graph_to_ideal(const int *constraint_ids,
                                 int n_constraints,
                                 const SingularVar *vars,
                                 int n_vars);

#ifdef __cplusplus
}
#endif

#endif /* lv_SINGULAR_BACKEND_H */

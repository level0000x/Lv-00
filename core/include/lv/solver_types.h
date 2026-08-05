#ifndef lv_SOLVER_TYPES_H
#define lv_SOLVER_TYPES_H

#include "lv/cross_platform.h"
#include "lv/lv_utils.h"
#include "lv/stream.h"
#include "mpz_poly.h"

/* ── solver 模块共享常量 ── */
#define lv_SOLVER_DYNARRAY_INIT_CAP 16
#define lv_SOLVER_LINEAR_COEFF_COUNT 2
#define lv_SOLVER_QUADRATIC_COEFF_COUNT 3
#define lv_ZERO_EPSILON 1e-12

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 多项式方程：一个变量的一元多项式约束
 */
typedef struct {
    mpz_poly_t poly;      /**< 一元多项式系数 */
    int var_node_id;      /**< 关联的变量节点 ID */
    int coord_index;      /**< 坐标索引（0=x, 1=y） */
} PolyEquation;

/**
 * @brief 方程系统：PolyEquation 的动态数组
 */
typedef struct EquationSystem {
    lvDArray eqs;         /**< PolyEquation 的动态数组 */
} EquationSystem;

/** @brief 初始化方程系统 */
void equation_system_init(EquationSystem *sys);

/** @brief 向方程系统添加一个方程 */
int equation_system_push(EquationSystem *sys, mpz_poly_t poly, int var_node_id, int coord_index);

/** @brief 清空方程系统（释放所有方程资源） */
void equation_system_clear(EquationSystem *sys);

/** @brief push 方程，失败时设置 OOM 错误并返回非零
 *          （原 EQUATION_PUSH_OR_GOTO 宏函数化；调用点通过返回值分支保持 goto label 语义） */
static inline int lv_equation_push_checked(EquationSystem *sys, mpz_poly_t poly, int vid, int ci) {
    int rc = equation_system_push(sys, poly, vid, ci);
    if (rc != 0) {
        lv_set_error(lv_ERROR_OUT_OF_MEMORY, "push failed (OOM)");
    }
    return rc;
}

/**
 * @brief solver 模块的全局流式上下文（集中定义在 solver_engine.c，其余文件通过 extern 引用）
 */
extern lv_THREAD_LOCAL StreamContext *solver_stream_ctx;

#ifdef __cplusplus
}
#endif

#endif /* lv_SOLVER_TYPES_H */

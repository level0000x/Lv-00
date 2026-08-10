#ifndef lv_SOLVER_TYPES_H
#define lv_SOLVER_TYPES_H

#include "lv/cross_platform.h"
#include "lv/lv_utils.h"
#include "lv/stream.h"
#include "lv/lv_numeric.h" /* lv_rel_tol_scale（相对容差缩放，K5-3B 共享设施） */
#include "mpz_poly.h"
#include <math.h>

/* ── solver 模块共享常量 ── */
#define lv_SOLVER_DYNARRAY_INIT_CAP 16
#define lv_SOLVER_LINEAR_COEFF_COUNT 2
#define lv_SOLVER_QUADRATIC_COEFF_COUNT 3
#define lv_ZERO_EPSILON lv_EPSILON_DOUBLE /* 数值零判定阈值（语义别名 = lv_EPSILON_DOUBLE，1e-12） */

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

/* ==================================================================
 * 共享的 double 二次求根工具
 *
 * solver_linear.c / solver_multibranch.c / solver_engine.c 共用的
 * 一元二次方程数值求解与判别式筛选，收敛重复实现。
 * ================================================================== */

/**
 * @brief 求解一元二次方程 a*x^2 + b*x + c = 0 的实根（double 版）
 *
 * 以 solver_linear.c 的相对容差实现为基准（原 solve_quadratic 的函数体）：
 * - 判别式 disc = b² - 4ac，相对容差
 *   disc_tol = lv_EPSILON_DOUBLE * max(1, max(b², |4ac|))；
 * - disc < -disc_tol：无实根，返回 0；
 * - |disc| < disc_tol：一个重根，返回 1（disc ∈ [-disc_tol, 0) 先钳位为 0）；
 * - 其余：两个不同实根，返回 2；
 * - |a| < lv_EPSILON_NEWTON 时坍缩为一次方程：|b| 亦过小视为退化返回 -1，
 *   否则返回 1 且 roots[0] = -c/b。
 *
 * @param a     二次项系数
 * @param b     一次项系数
 * @param c     常数项
 * @param roots 输出根数组（至少 2 个元素；仅写返回个数个元素）
 * @return 实根个数（0/1/2）；系数退化（a、b 均接近零）返回 -1
 */
static inline int solver_quadratic_roots_double(double a, double b, double c, double roots[2]) {
    if (fabs(a) < lv_EPSILON_NEWTON) {
        /* 坍缩为一元一次方程 */
        if (fabs(b) < lv_EPSILON_NEWTON)
            return -1;
        roots[0] = -c / b;
        return 1;
    }
    double disc = b * b - 4.0 * a * c;
    /* 相对容差：系数量级很大时（如 b² ~ 1e20）判别式浮点舍入误差可达
     * O(|b²| * eps_machine)，远超绝对容差 lv_EPSILON_DOUBLE。
     * K4-3B 收敛：eps * fmax(1.0, fmax(b², |4ac|)) → lv_rel_tol_scale(eps, mag)，
     * mag = fmax(b², |4ac|) 恒非负故 helper 内 fabs 为恒等，数值逐位一致。
     * 量纲语义（供 K5 对齐）：量纲 k = max(b², |4ac|)（判别式量纲 L²，
     * 二维 fmax 而非单变量 |x| 形态），缩放基准 eps = lv_EPSILON_DOUBLE。 */
    double disc_tol = lv_rel_tol_scale(lv_EPSILON_DOUBLE, fmax(b * b, fabs(4.0 * a * c)));
    if (disc < -disc_tol) {
        /* 无实数根 */
        return 0;
    }
    if (disc < 0)
        disc = 0.0;
    double sq = sqrt(disc);
    roots[0] = (-b - sq) / (2.0 * a);
    roots[1] = (-b + sq) / (2.0 * a);
    return (fabs(disc) < disc_tol) ? 1 : 2;
}

/**
 * @brief 判定二次方程（GMP 缩放整数系数）是否存在两个不同实根，并输出根
 *
 * 语义与 solver_multibranch.c 原内联实现严格一致（无容差版本）：
 * - |a| < lv_EPSILON_DOUBLE：系数退化，返回 false；
 * - 判别式 D = b² - 4ac <= 0：无两个不同实根，返回 false；
 * - 其余：返回 true，roots[0] = (-b + sqrt(D))/(2a)，
 *   roots[1] = (-b - sqrt(D))/(2a)（与多解分支的 root1/root2 顺序一致）。
 *
 * solver_engine.c 的释放计数与 solver_multibranch.c 的分支收集共用本判定，
 * scale 常量与判别式语义单一来源，杜绝两侧漂移。
 *
 * @param poly         一元多项式（调用方保证次数为 2），系数为按 scale_factor 缩放的整数
 * @param scale_factor 系数缩放因子（如 lv_SOLVER_SCALE_FACTOR）
 * @param roots        输出两个根（仅在返回 true 时写入）
 * @return true 表示存在两个不同实根
 */
static inline bool solver_quadratic_distinct_roots(const mpz_poly_t *poly, int64_t scale_factor, double roots[2]) {
    double a = mpz_get_d(poly->coeffs[2]) / (double) scale_factor;
    double b = mpz_get_d(poly->coeffs[1]) / (double) scale_factor;
    double c = mpz_get_d(poly->coeffs[0]) / (double) scale_factor;
    if (fabs(a) < lv_EPSILON_DOUBLE)
        return false;
    double D = b * b - 4.0 * a * c;
    if (D <= 0)
        return false;
    double sqrt_D = sqrt(D);
    roots[0] = (-b + sqrt_D) / (2.0 * a);
    roots[1] = (-b - sqrt_D) / (2.0 * a);
    return true;
}

/**
 * @brief 统计方程系统中存在两个不同实根的二次方程个数
 *
 * 供 solver_engine.c 释放分支坐标时重新统计使用，与 solver_multibranch.c
 * 的分支收集共用 solver_quadratic_distinct_roots 判定，保证两侧计数一致。
 * 不做 2^12 截断，截断由调用方负责（与两处原逻辑一致）。
 *
 * @param sys          方程系统
 * @param scale_factor 系数缩放因子（如 lv_SOLVER_SCALE_FACTOR）
 * @return 满足条件的二次方程个数
 */
static inline int solver_count_positive_disc_quadratics(const EquationSystem *sys, int64_t scale_factor) {
    int count = 0;
    if (!sys)
        return 0;
    for (int i = 0; i < sys->eqs.count; i++) {
        const PolyEquation *pe = (const PolyEquation *)lv_darray_get(&sys->eqs, i);
        if (pe->poly.degree != 2)
            continue;
        double roots[2];
        if (solver_quadratic_distinct_roots(&pe->poly, scale_factor, roots))
            count++;
    }
    return count;
}

#ifdef __cplusplus
}
#endif

#endif /* lv_SOLVER_TYPES_H */

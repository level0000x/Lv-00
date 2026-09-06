/**
 * @file solver_linear.c
 * @brief 数值求解器（线性/二次/三次）
 *
 * @details 从 solver.c 拆分出的子模块（Lv-00 项目 v3.3.0+）。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "solver_common.h"

/* ── 数值求解器 ── */

bool solve_linear(const mpz_poly_t *poly, double *x_out) {
    if (poly->degree != 1)
        return false;
    double a = mpz_get_d(poly->coeffs[1]);
    double b = mpz_get_d(poly->coeffs[0]);
    if (fabs(a) < lv_EPSILON_NEWTON)
        return false;
    *x_out = -b / a;
    return true;
}

/* ------------------------------------------------------------------ */
/*  内部：求解一元二次方程 a*x^2 + b*x + c = 0                             */
/* ------------------------------------------------------------------ */

typedef struct {
    double roots[2];
    int root_count;
} QuadraticRoots;

/**
 * @brief 求解一元二次方程 a*x^2 + b*x + c = 0
 *
 * 数值求根逻辑收敛到共享的 solver_quadratic_roots_double
 * （solver_types.h），本函数仅做次数检查与系数提取。
 *
 * @param poly 一元多项式指针（次数必须为 2）
 * @param out  输出：QuadraticRoots 结构体，包含根数组和根数量
 * @return true 表示成功（含无实根情况），false 表示输入的次数不是 2
 *         或系数退化（a、b 均接近零，无法求解）
 */
static bool solve_quadratic(mpz_poly_t *poly, QuadraticRoots *out) {
    if (poly->degree != 2)
        return false;
    double a = mpz_get_d(poly->coeffs[2]);
    double b = mpz_get_d(poly->coeffs[1]);
    double c = mpz_get_d(poly->coeffs[0]);
    int n = solver_quadratic_roots_double(a, b, c, out->roots);
    if (n < 0)
        return false;
    out->root_count = n;
    return true;
}

/* =======================================================================
 * 内部函数：精确求解一元二次方程
 *
 * 对 ax^2 + bx + c = 0，使用 GMP 精确整数运算（mpz_t）计算判别式
 * D = b^2 - 4ac。根据 D 的符号和是否为完全平方数，决定输出类型：
 *   - D < 0：无实数解
 *   - D = 0：唯一精确解（RATIONAL 类型）
 *   - D > 0 且为完全平方数：两个精确解（RATIONAL 类型）
 *   - D > 0 且非完全平方数：两个二次根式解（QUADRATIC 类型）
 *     表示为 a + b*sqrt(n)，其中 n 是 D 的无平方因子部分
 *
 * 注意：所有中间计算使用 GMP mpz_t 精确大整数，确保无精度损失。
 * 仅在 n > lv_SOLVER_PRIME_LIMIT 或超出 unsigned int 范围时才回退到
 * double 近似解。
 * ======================================================================= */

/**
 * @brief 将 SymbolicCoord 转换为精确的有理数 mpq_t
 *
 * @details 仅支持 RATIONAL 类型的坐标。非 RATIONAL 类型返回 false。
 *          用于精确的有理数算术运算。
 *
 * @param c   符号坐标指针
 * @param out 输出：mpq_t 有理数
 * @return true 表示转换成功，false 表示失败（非 RATIONAL 类型）
 */
static bool symbolic_coord_to_mpq(const SymbolicCoord *c, mpq_t out) {
    if (!c || c->type != RATIONAL || !c->data.rational)
        return false;
    mpq_set(out, lv_rational_mpq(c->data.rational));
    return true;
}
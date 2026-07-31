/**
 * @file solver_linear.c
 * @brief 数值求解器（线性/二次/三次）
 *
 * @details 从 solver.c 拆分出的子模块（Lv-00 项目 v3.3.0+）。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "solver_common.h"

#define EQUATION_PUSH_OR_GOTO(sys, poly, vid, ci, label)               \
    do {                                                               \
        if (equation_system_push((sys), (poly), (vid), (ci)) != 0) {   \
            lv_set_error(lv_ERROR_OUT_OF_MEMORY, "push failed (OOM)"); \
            goto label;                                                \
        }                                                              \
    } while (0)

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
 * 使用判别式 b^2 - 4ac 求根。判别式为负时返回零个实根，
 * 接近零时返回一个重根，否则返回两个实根。系数 a 接近零时
 * 退化为线性方程求解。
 *
 * @param poly 一元多项式指针（次数必须为 2）
 * @param out  输出：QuadraticRoots 结构体，包含根数组和根数量
 * @return true 表示成功（含无实根情况），false 表示输入的次数不是 2
 */
static bool solve_quadratic(mpz_poly_t *poly, QuadraticRoots *out) {
    if (poly->degree != 2)
        return false;
    double a = mpz_get_d(poly->coeffs[2]);
    double b = mpz_get_d(poly->coeffs[1]);
    double c = mpz_get_d(poly->coeffs[0]);
    if (fabs(a) < lv_EPSILON_NEWTON) {
        /* 坍缩为一元一次方程 */
        if (fabs(b) < lv_EPSILON_NEWTON)
            return false;
        out->roots[0] = -c / b;
        out->root_count = 1;
        return true;
    }
    double disc = b * b - 4.0 * a * c;
    /* 使用相对容差判断判别式是否为负/零。
     * 当系数量级很大时（如 b² ~ 1e20），判别式的浮点计算舍入误差
     * 可达 O(|b²| * eps_machine)，远超绝对容差 lv_EPSILON_DOUBLE。
     * 使用 max(|b²|, |4ac|) * eps 作为相对容差。 */
    double disc_tol = lv_EPSILON_DOUBLE * fmax(1.0, fmax(b * b, fabs(4.0 * a * c)));
    if (disc < -disc_tol) {
        /* 无实数根 */
        out->root_count = 0;
        return true;
    }
    if (disc < 0)
        disc = 0.0;
    double sq = sqrt(disc);
    out->roots[0] = (-b - sq) / (2.0 * a);
    out->roots[1] = (-b + sq) / (2.0 * a);
    out->root_count = (fabs(disc) < disc_tol) ? 1 : 2;
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
    mpq_set(out, c->data.rational->value);
    return true;
}
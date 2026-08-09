/**
 * @file geo_predicate.c
 * @brief 精确几何谓词实现 —— 借鉴 CGAL Exact Predicate Paradigm
 *
 * 实现所有在 geo_predicate.h 中声明的几何谓词函数。
 * 支持三种精度模式：
 *   - lv_PREDICATE_EXACT：区间算术保证精确判定
 *   - lv_PREDICATE_APPROX：浮点运算快速判定
 *   - lv_PREDICATE_ADAPTIVE：自适应精度（先浮点，不确定时切换精确）
 *
 * 借鉴来源：
 *   - CGAL (github.com/CGAL/cgal) 精确谓词范式
 *   - Jonathan Shewchuk, "Adaptive Precision Floating-Point Arithmetic and
 *     Fast Robust Predicates for Computational Geometry"
 *   - Boost.Geometry 策略模式
 *
 * @version 3.6.0
 */

#include "lv/geo_predicate.h"

#include <float.h>
#include <math.h>
#include <string.h>
#include <stdatomic.h>

#include "lv/config.h"
#include "lv/geometry_config.h"
#include "lv/interval_arith.h"
#include "lv/lv_utils.h"
#include "lv/geo_utils.h"

/* 确保 lv_PUBLIC_API 已定义 */
#ifndef lv_PUBLIC_API
#define lv_PUBLIC_API
#endif

/* ========================================================================
 * 内部辅助宏与常量
 * ======================================================================== */

/**
 * @brief 自适应模式中浮点结果"接近零"的阈值。
 *
 * 当浮点结果的绝对值小于此阈值时，回退到精确（区间）计算。
 * 该值取为 DBL_EPSILON 的 256 倍，覆盖大多数退化情况下的浮点误差。
 */
#define ADAPTIVE_THRESHOLD (256.0 * DBL_EPSILON)

/**
 * @brief 近似模式中使用的默认容差。
 * 当浮点结果绝对值小于此阈值时判定为零。
 * @note 收敛：数值 = 1e-9，引用 config.h 的 lv_EPSILON_MEDIUM（同值），行为不变
 */
#define APPROX_EPSILON (lv_EPSILON_MEDIUM)

/* ========================================================================
 * 全局状态
 * ======================================================================== */

/** @brief 全局默认精度模式，初始为自适应 */
static lvPredicateMode g_predicate_mode = lv_PREDICATE_ADAPTIVE;

/**
 * @brief 全局谓词统计信息（原子计数，多线程热路径安全）
 *
 * 字段类型与头文件 lvPredicateStats 一致（3 × size_t），
 * 声明为 _Atomic 供多线程原子自增/读取/清零。
 *
 * 注（P1-3 标注）：保持 C11 atomic 而非迁移 lv_ATOMIC_* —— size_t 在
 * 64 位平台为 64 位宽，而 lv_ATOMIC_* 宏族仅有 32 位 LOAD/STORE 原语；
 * 此处为热路径 relaxed 统计（允许乱序累加），与宏族 SEQCST 语义不完全
 * 等价，故保留 C11 原子类型。
 */
typedef struct {
    _Atomic size_t approx_count;      /**< 近似模式调用次数 */
    _Atomic size_t exact_count;       /**< 精确模式调用次数 */
    _Atomic size_t adaptive_fallback; /**< 自适应回退次数 */
} AtomicPredicateStats;

static AtomicPredicateStats g_predicate_stats = {0, 0, 0};

/** @brief 原子递增统计计数（热路径，relaxed 内存序即可） */
#define lv_PREDICATE_STAT_INC(field) \
    atomic_fetch_add_explicit(&g_predicate_stats.field, 1, memory_order_relaxed)

/* ========================================================================
 * 内部辅助函数
 * ======================================================================== */

/* ── 精度模式分派查找表（数据表化，替代重复的 if 分支链） ── */

/** @brief 模式归一化表：SYMBOLIC → EXACT（符号模式暂回退到精确模式），其余原样 */
static const lvPredicateMode s_predicate_mode_normalize[] = {
    [lv_PREDICATE_EXACT] = lv_PREDICATE_EXACT,
    [lv_PREDICATE_APPROX] = lv_PREDICATE_APPROX,
    [lv_PREDICATE_ADAPTIVE] = lv_PREDICATE_ADAPTIVE,
    [lv_PREDICATE_SYMBOLIC] = lv_PREDICATE_EXACT,
};

/**
 * @brief 归一化精度模式：将 SYMBOLIC 映射为 EXACT，其余模式原样返回
 */
static lvPredicateMode normalize_predicate_mode(lvPredicateMode mode) {
    if ((unsigned) mode < lv_ARRAY_SIZE(s_predicate_mode_normalize))
        return s_predicate_mode_normalize[mode];
    return mode;
}

/**
 * @brief 使用区间算术精确计算 2D 方向谓词（有符号面积的两倍）
 *
 * 计算结果为区间 [lo, hi]：
 *   - lo > 0 → LEFT
 *   - hi < 0 → RIGHT
 *   - lo <= 0 && hi >= 0 → COLLINEAR（无法排除零）
 *
 * @return 区间表示的有符号面积
 */
static lvInterval orientation_2d_exact_interval(double p1x, double p1y, double p2x, double p2y, double p3x,
                                                double p3y) {
    /* (p2 - p1) x (p3 - p1) = (p2x-p1x)*(p3y-p1y) - (p3x-p1x)*(p2y-p1y) */
    lvInterval dx2 = lv_interval_sub(lv_interval_make(p2x, p2x, 1), lv_interval_make(p1x, p1x, 1));
    lvInterval dy2 = lv_interval_sub(lv_interval_make(p2y, p2y, 1), lv_interval_make(p1y, p1y, 1));
    lvInterval dx3 = lv_interval_sub(lv_interval_make(p3x, p3x, 1), lv_interval_make(p1x, p1x, 1));
    lvInterval dy3 = lv_interval_sub(lv_interval_make(p3y, p3y, 1), lv_interval_make(p1y, p1y, 1));

    /* cross = dx2 * dy3 - dx3 * dy2 */
    lvInterval term1 = lv_interval_mul(dx2, dy3);
    lvInterval term2 = lv_interval_mul(dx3, dy2);
    lvInterval result = lv_interval_sub(term1, term2);

    return result;
}

/**
 * @brief 使用区间算术精确计算 3D 方向谓词（四面体有符号体积的六倍）
 *
 * 计算行列式：
 *   | p2x-p1x  p3x-p1x  p4x-p1x |
 *   | p2y-p1y  p3y-p1y  p4y-p1y |
 *   | p2z-p1z  p3z-p1z  p4z-p1z |
 *
 * 使用 Sarrus / Laplace 展开计算 3x3 行列式。
 */
static lvInterval orientation_3d_exact_interval(double p1x, double p1y, double p1z, double p2x, double p2y, double p2z,
                                                double p3x, double p3y, double p3z, double p4x, double p4y,
                                                double p4z) {
    /* 构造三个列向量（区间） */
    lvInterval ax = lv_interval_sub(lv_interval_make(p2x, p2x, 1), lv_interval_make(p1x, p1x, 1));
    lvInterval ay = lv_interval_sub(lv_interval_make(p2y, p2y, 1), lv_interval_make(p1y, p1y, 1));
    lvInterval az = lv_interval_sub(lv_interval_make(p2z, p2z, 1), lv_interval_make(p1z, p1z, 1));

    lvInterval bx = lv_interval_sub(lv_interval_make(p3x, p3x, 1), lv_interval_make(p1x, p1x, 1));
    lvInterval by = lv_interval_sub(lv_interval_make(p3y, p3y, 1), lv_interval_make(p1y, p1y, 1));
    lvInterval bz = lv_interval_sub(lv_interval_make(p3z, p3z, 1), lv_interval_make(p1z, p1z, 1));

    lvInterval cx = lv_interval_sub(lv_interval_make(p4x, p4x, 1), lv_interval_make(p1x, p1x, 1));
    lvInterval cy = lv_interval_sub(lv_interval_make(p4y, p4y, 1), lv_interval_make(p1y, p1y, 1));
    lvInterval cz = lv_interval_sub(lv_interval_make(p4z, p4z, 1), lv_interval_make(p1z, p1z, 1));

    /* det = a*(b x c) */
    /* b x c = (by*cz - bz*cy, bz*cx - bx*cz, bx*cy - by*cx) */
    /* dot(a, bxc) = ax*(by*cz - bz*cy) - ay*(bx*cz - bz*cx) + az*(bx*cy - by*cx) */

    lvInterval bycz = lv_interval_mul(by, cz);
    lvInterval bzcy = lv_interval_mul(bz, cy);
    lvInterval bxcy = lv_interval_mul(bx, cy);
    lvInterval bycx = lv_interval_mul(by, cx);
    lvInterval bxcz = lv_interval_mul(bx, cz);
    lvInterval bzcx = lv_interval_mul(bz, cx);

    lvInterval term1 = lv_interval_mul(ax, lv_interval_sub(bycz, bzcy));
    lvInterval term2 = lv_interval_mul(ay, lv_interval_sub(bxcz, bzcx));
    lvInterval term3 = lv_interval_mul(az, lv_interval_sub(bxcy, bycx));

    return lv_interval_add(lv_interval_sub(term1, term2), term3);
}

/**
 * @brief 使用区间算术精确计算点与圆的位置关系
 *
 * 计算 |p - c|^2 - r^2 的区间。
 */
static lvInterval side_of_circle_exact_interval(double px, double py, double cx, double cy, double r) {
    lvInterval dx = lv_interval_sub(lv_interval_make(px, px, 1), lv_interval_make(cx, cx, 1));
    lvInterval dy = lv_interval_sub(lv_interval_make(py, py, 1), lv_interval_make(cy, cy, 1));
    lvInterval dist_sq = lv_interval_add(lv_interval_mul(dx, dx), lv_interval_mul(dy, dy));
    lvInterval r_sq = lv_interval_mul(lv_interval_make(r, r, 1), lv_interval_make(r, r, 1));
    return lv_interval_sub(dist_sq, r_sq);
}

/**
 * @brief 使用区间算术精确计算四点共圆行列式
 *
 * 行列式：
 *   | ax  ay  ax^2+ay^2  1 |
 *   | bx  by  bx^2+by^2  1 |
 *   | cx  cy  cx^2+cy^2  1 |
 *   | dx  dy  dx^2+dy^2  1 |
 *
 * 使用 Laplace 展开沿最后一列展开。
 */
static lvInterval four_points_concyclic_exact_interval(double ax, double ay, double bx, double by, double cx, double cy,
                                                       double dx, double dy) {
    /* 计算 ax^2+ay^2 等 */
    lvInterval a2 = lv_interval_add(lv_interval_mul(lv_interval_make(ax, ax, 1), lv_interval_make(ax, ax, 1)),
                                 lv_interval_mul(lv_interval_make(ay, ay, 1), lv_interval_make(ay, ay, 1)));
    lvInterval b2 = lv_interval_add(lv_interval_mul(lv_interval_make(bx, bx, 1), lv_interval_make(bx, bx, 1)),
                                 lv_interval_mul(lv_interval_make(by, by, 1), lv_interval_make(by, by, 1)));
    lvInterval c2 = lv_interval_add(lv_interval_mul(lv_interval_make(cx, cx, 1), lv_interval_make(cx, cx, 1)),
                                 lv_interval_mul(lv_interval_make(cy, cy, 1), lv_interval_make(cy, cy, 1)));
    lvInterval d2 = lv_interval_add(lv_interval_mul(lv_interval_make(dx, dx, 1), lv_interval_make(dx, dx, 1)),
                                 lv_interval_mul(lv_interval_make(dy, dy, 1), lv_interval_make(dy, dy, 1)));

    /*
     * Laplace 展开沿最后一列（全为 1）：
     * det = M11 - M21 + M31 - M41
     *
     * 其中 Mij 是去掉第 i 行、第 j 列的 3x3 子行列式。
     *
     * M11 = | by  b2  1 |  = by*(c2*1 - 1*dy) - b2*(cy*1 - 1*dy) + 1*(cy*dy - c2*dy)
     *       | cy  c2  1 |
     *       | dy  d2  1 |
     *
     * 简化 3x3 行列式（最后一列全为 1）：
     * M11 = by*(c2 - d2) + b2*(dy - cy) + (cy*d2 - c2*dy)
     * M21 = bx*(c2 - d2) + a2*(dy - cy) + (cy*d2 - c2*dy)  -- 不对，重新推导
     *
     * 实际上对于 3x3 行列式（最后一列为 1）：
     * | a b 1 |
     * | c d 1 | = a*(d - f) - b*(c - e) + (c*f - d*e)
     * | e f 1 |
     *
     * M11: a=by, b=b2, c=cy, d=c2, e=dy, f=d2
     *     = by*(c2 - d2) - b2*(cy - dy) + (cy*d2 - c2*dy)
     *
     * M21: a=bx, b=a2, c=cx, d=c2, e=dx, f=d2
     *     = bx*(c2 - d2) - a2*(cx - dx) + (cx*d2 - c2*dx)
     *
     * M31: a=ax, b=a2, c=bx, d=b2, e=dx, f=d2
     *     = ax*(b2 - d2) - a2*(bx - dx) + (bx*d2 - b2*dx)
     *
     * M41: a=ax, b=a2, c=bx, d=b2, e=cx, f=c2
     *     = ax*(b2 - c2) - a2*(bx - cx) + (bx*c2 - b2*cx)
     */

    /* M11 */
    lvInterval m11_t1 = lv_interval_mul(lv_interval_make(by, by, 1), lv_interval_sub(c2, d2));
    lvInterval m11_t2 = lv_interval_mul(b2, lv_interval_sub(lv_interval_make(cy, cy, 1), lv_interval_make(dy, dy, 1)));
    lvInterval m11_t3 = lv_interval_sub(lv_interval_mul(lv_interval_make(cy, cy, 1), d2), lv_interval_mul(c2, lv_interval_make(dy, dy, 1)));
    lvInterval m11 = lv_interval_add(lv_interval_sub(m11_t1, m11_t2), m11_t3);

    /* M21 */
    lvInterval m21_t1 = lv_interval_mul(lv_interval_make(bx, bx, 1), lv_interval_sub(c2, d2));
    lvInterval m21_t2 = lv_interval_mul(a2, lv_interval_sub(lv_interval_make(cx, cx, 1), lv_interval_make(dx, dx, 1)));
    lvInterval m21_t3 = lv_interval_sub(lv_interval_mul(lv_interval_make(cx, cx, 1), d2), lv_interval_mul(c2, lv_interval_make(dx, dx, 1)));
    lvInterval m21 = lv_interval_add(lv_interval_sub(m21_t1, m21_t2), m21_t3);

    /* M31 */
    lvInterval m31_t1 = lv_interval_mul(lv_interval_make(ax, ax, 1), lv_interval_sub(b2, d2));
    lvInterval m31_t2 = lv_interval_mul(a2, lv_interval_sub(lv_interval_make(bx, bx, 1), lv_interval_make(dx, dx, 1)));
    lvInterval m31_t3 = lv_interval_sub(lv_interval_mul(lv_interval_make(bx, bx, 1), d2), lv_interval_mul(b2, lv_interval_make(dx, dx, 1)));
    lvInterval m31 = lv_interval_add(lv_interval_sub(m31_t1, m31_t2), m31_t3);

    /* M41 */
    lvInterval m41_t1 = lv_interval_mul(lv_interval_make(ax, ax, 1), lv_interval_sub(b2, c2));
    lvInterval m41_t2 = lv_interval_mul(a2, lv_interval_sub(lv_interval_make(bx, bx, 1), lv_interval_make(cx, cx, 1)));
    lvInterval m41_t3 = lv_interval_sub(lv_interval_mul(lv_interval_make(bx, bx, 1), c2), lv_interval_mul(b2, lv_interval_make(cx, cx, 1)));
    lvInterval m41 = lv_interval_add(lv_interval_sub(m41_t1, m41_t2), m41_t3);

    /* det = M11 - M21 + M31 - M41 */
    lvInterval det = lv_interval_sub(lv_interval_add(lv_interval_sub(m11, m21), m31), m41);

    return det;
}

/* ========================================================================
 * 第二部分：2D 精确谓词 API 实现
 * ======================================================================== */

/* ── lv_orientation_2d 精度模式实现 ── */

/** @brief 2D 方向谓词实现函数签名 */
typedef lvOrientation (*Orientation2dImplFn)(double p1x, double p1y, double p2x, double p2y, double p3x, double p3y,
                                              double eps);

/** @brief 近似模式实现：直接浮点叉积 + 自适应阈值 */
static lvOrientation orientation_2d_approx(double p1x, double p1y, double p2x, double p2y, double p3x, double p3y,
                                           double eps) {
    lv_PREDICATE_STAT_INC(approx_count);

    double cross = geo_signed_area_2x(p1x, p1y, p2x, p2y, p3x, p3y);

    /*
     * 自适应阈值：当 |cross| 相对于输入坐标的量级足够大时，
     * 浮点结果可信。2D 叉积展开后每项都是坐标差乘积，
     * 量级正比于 O(coord^3)，因此浮点舍入误差的上界也按此标度增长。
     * 固定阈值对于大坐标会过于严格（舍入误差超过阈值），
     * 对于小坐标又可能过于宽松。
     */
    double max_coord =
        fmax(fmax(fabs(p1x), fabs(p1y)), fmax(fmax(fabs(p2x), fabs(p2y)), fmax(fabs(p3x), fabs(p3y))));
    double adapted_eps = eps * fmax(1.0, max_coord * max_coord * max_coord);

    if (cross > adapted_eps) {
        return lv_ORIENTATION_LEFT;
    } else if (cross < -adapted_eps) {
        return lv_ORIENTATION_RIGHT;
    } else {
        return lv_ORIENTATION_COLLINEAR;
    }
}

/** @brief 精确模式实现：区间算术 */
static lvOrientation orientation_2d_exact(double p1x, double p1y, double p2x, double p2y, double p3x, double p3y,
                                          double eps) {
    (void) eps;
    lv_PREDICATE_STAT_INC(exact_count);

    lvInterval result = orientation_2d_exact_interval(p1x, p1y, p2x, p2y, p3x, p3y);

    if (result.lo > 0.0) {
        return lv_ORIENTATION_LEFT;
    } else if (result.hi < 0.0) {
        return lv_ORIENTATION_RIGHT;
    } else {
        return lv_ORIENTATION_COLLINEAR;
    }
}

/** @brief 2D 方向谓词实现表：[lv_PREDICATE_APPROX / lv_PREDICATE_EXACT] → 实现函数 */
static const Orientation2dImplFn s_orientation_2d_impls[] = {
    [lv_PREDICATE_APPROX] = orientation_2d_approx,
    [lv_PREDICATE_EXACT] = orientation_2d_exact,
};

/**
 * @brief 判定三点方向（2D orientation test）
 *
 * 计算有符号面积的两倍：(p2-p1) x (p3-p1)
 *   > 0 -> LEFT（逆时针）
 *   < 0 -> RIGHT（顺时针）
 *   = 0 -> COLLINEAR（共线）
 *
 * @param p1x, p1y  第一个点坐标
 * @param p2x, p2y  第二个点坐标
 * @param p3x, p3y  第三个点坐标
 * @param mode      精度模式（APPROX / EXACT / ADAPTIVE）
 * @return lvOrientation 方向枚举（LEFT / RIGHT / COLLINEAR / DEGENERATE）
 */
lv_PUBLIC_API lvOrientation lv_orientation_2d(double p1x, double p1y, double p2x, double p2y, double p3x, double p3y,
                                              lvPredicateMode mode) {
    /*
     * 检查退化情况：如果任意两点重合，则无法判定方向。
     */
    double d12_sq = (p2x - p1x) * (p2x - p1x) + (p2y - p1y) * (p2y - p1y);
    double d13_sq = (p3x - p1x) * (p3x - p1x) + (p3y - p1y) * (p3y - p1y);
    double d23_sq = (p3x - p2x) * (p3x - p2x) + (p3y - p2y) * (p3y - p2y);

    const lvGeometryConfig *cfg = lv_geometry_get_config();
    double eps = cfg->collinear_epsilon; /* lv_geometry_get_config() 恒返回内部快照指针，非 NULL */

    /*
     * 检查 p1 与 p2 或 p1 与 p3 是否重合：
     * p1 是方向计算的原点参考点，当 p1 与 p2 或 p1 与 p3 任一重合时，
     * 叉积 (p2-p1)×(p3-p1) 对应的一项为零向量，导致结果为零，
     * 几何判定无意义。因此使用 OR 而非 AND 检查。
     *
     * 注意：仅检查 p1-p2 和 p1-p3 即可，因为 p1 是叉积参考原点。
     * p2-p3 之间的距离不直接影响叉积的零性。
     */
    if (d12_sq < eps * eps || d13_sq < eps * eps) {
        return lv_ORIENTATION_DEGENERATE;
    }

    /* 模式归一化：SYMBOLIC → EXACT（符号模式暂回退到精确模式） */
    mode = normalize_predicate_mode(mode);

    /* 查表分发 APPROX / EXACT 实现 */
    if ((unsigned) mode < lv_ARRAY_SIZE(s_orientation_2d_impls) && s_orientation_2d_impls[mode]) {
        return s_orientation_2d_impls[mode](p1x, p1y, p2x, p2y, p3x, p3y, eps);
    }

    /* 自适应模式：先浮点，不确定时切换精确 */
    {
        double cross = geo_signed_area_2x(p1x, p1y, p2x, p2y, p3x, p3y);

        /*
         * 自适应阈值：当 |cross| 相对于输入坐标的量级足够大时，
         * 浮点结果可信。否则回退到区间算术。
         *
         * 阈值取 ADAPTIVE_THRESHOLD * max_coord^3，因为 2D 叉积
         * (p2-p1)×(p3-p1) 展开后每项都是坐标差乘积，量级正比于 O(coord^3)，
         * 因此浮点舍入误差的上界也按此标度增长。
         */
        double max_coord =
            fmax(fmax(fabs(p1x), fabs(p1y)), fmax(fmax(fabs(p2x), fabs(p2y)), fmax(fabs(p3x), fabs(p3y))));
        double threshold = ADAPTIVE_THRESHOLD * max_coord * max_coord * max_coord;

        if (threshold == 0.0) {
            /* 所有坐标为零时的保底阈值，避免退化为精确模式死胡同 */
            threshold = ADAPTIVE_THRESHOLD;
        }

        if (cross > threshold) {
            lv_PREDICATE_STAT_INC(approx_count);
            return lv_ORIENTATION_LEFT;
        } else if (cross < -threshold) {
            lv_PREDICATE_STAT_INC(approx_count);
            return lv_ORIENTATION_RIGHT;
        } else {
            /* 回退到精确模式 */
            lv_PREDICATE_STAT_INC(adaptive_fallback);
            lv_PREDICATE_STAT_INC(exact_count);

            lvInterval result = orientation_2d_exact_interval(p1x, p1y, p2x, p2y, p3x, p3y);

            if (result.lo > 0.0) {
                return lv_ORIENTATION_LEFT;
            } else if (result.hi < 0.0) {
                return lv_ORIENTATION_RIGHT;
            } else {
                return lv_ORIENTATION_COLLINEAR;
            }
        }
    }
}

/* ── lv_orientation_3d 精度模式实现 ── */

/** @brief 3D 方向谓词实现函数签名 */
typedef lvOrientation (*Orientation3dImplFn)(double p1x, double p1y, double p1z, double p2x, double p2y, double p2z,
                                              double p3x, double p3y, double p3z, double p4x, double p4y, double p4z,
                                              double eps);

/** @brief 近似模式实现：直接浮点 3x3 行列式 */
static lvOrientation orientation_3d_approx(double p1x, double p1y, double p1z, double p2x, double p2y, double p2z,
                                           double p3x, double p3y, double p3z, double p4x, double p4y, double p4z,
                                           double eps) {
    lv_PREDICATE_STAT_INC(approx_count);

    /* 3x3 行列式：det(a, b, c) = a . (b x c) */
    double ax = p2x - p1x, ay = p2y - p1y, az = p2z - p1z;
    double bx = p3x - p1x, by = p3y - p1y, bz = p3z - p1z;
    double cx = p4x - p1x, cy = p4y - p1y, cz = p4z - p1z;

    double det = ax * (by * cz - bz * cy) - ay * (bx * cz - bz * cx) + az * (bx * cy - by * cx);

    if (det > eps) {
        return lv_ORIENTATION_LEFT;
    } else if (det < -eps) {
        return lv_ORIENTATION_RIGHT;
    } else {
        return lv_ORIENTATION_COPLANAR;
    }
}

/** @brief 精确模式实现：区间算术 */
static lvOrientation orientation_3d_exact(double p1x, double p1y, double p1z, double p2x, double p2y, double p2z,
                                          double p3x, double p3y, double p3z, double p4x, double p4y, double p4z,
                                          double eps) {
    (void) eps;
    lv_PREDICATE_STAT_INC(exact_count);

    lvInterval result = orientation_3d_exact_interval(p1x, p1y, p1z, p2x, p2y, p2z, p3x, p3y, p3z, p4x, p4y, p4z);

    if (result.lo > 0.0) {
        return lv_ORIENTATION_LEFT;
    } else if (result.hi < 0.0) {
        return lv_ORIENTATION_RIGHT;
    } else {
        return lv_ORIENTATION_COPLANAR;
    }
}

/** @brief 3D 方向谓词实现表：[lv_PREDICATE_APPROX / lv_PREDICATE_EXACT] → 实现函数 */
static const Orientation3dImplFn s_orientation_3d_impls[] = {
    [lv_PREDICATE_APPROX] = orientation_3d_approx,
    [lv_PREDICATE_EXACT] = orientation_3d_exact,
};

/**
 * @brief 判定四点方向（3D orientation test）
 *
 * 计算四面体有符号体积的六倍（3x3 行列式）。
 *
 * @param p1x, p1y, p1z  第一个点坐标
 * @param p2x, p2y, p2z  第二个点坐标
 * @param p3x, p3y, p3z  第三个点坐标
 * @param p4x, p4y, p4z  第四个点坐标
 * @param mode           精度模式
 * @return lvOrientation 方向枚举（LEFT / RIGHT / COPLANAR / DEGENERATE）
 */
lvOrientation lv_orientation_3d(double p1x, double p1y, double p1z, double p2x, double p2y, double p2z,
                                              double p3x, double p3y, double p3z, double p4x, double p4y, double p4z,
                                              lvPredicateMode mode) {
    /*
     * 检查退化情况：3D 方向以 p1 为参考原点计算 3×3 行列式。
     * 若 p1 与 p2、p3、p4 中任一重合，则对应列向量为零，
     * 行列式必为零，方向判定无意义。
     *
     * 与 2D 同理，此处使用 OR 检查：任一对重合即退化。
     */
    double d12_sq = (p2x - p1x) * (p2x - p1x) + (p2y - p1y) * (p2y - p1y) + (p2z - p1z) * (p2z - p1z);
    double d13_sq = (p3x - p1x) * (p3x - p1x) + (p3y - p1y) * (p3y - p1y) + (p3z - p1z) * (p3z - p1z);
    double d14_sq = (p4x - p1x) * (p4x - p1x) + (p4y - p1y) * (p4y - p1y) + (p4z - p1z) * (p4z - p1z);

    const lvGeometryConfig *cfg = lv_geometry_get_config();
    double eps = cfg->collinear_epsilon; /* lv_geometry_get_config() 恒返回内部快照指针，非 NULL */

    if (d12_sq < eps * eps || d13_sq < eps * eps || d14_sq < eps * eps) {
        return lv_ORIENTATION_DEGENERATE;
    }

    /* 模式归一化：SYMBOLIC → EXACT（符号模式暂回退到精确模式） */
    mode = normalize_predicate_mode(mode);

    /* 查表分发 APPROX / EXACT 实现 */
    if ((unsigned) mode < lv_ARRAY_SIZE(s_orientation_3d_impls) && s_orientation_3d_impls[mode]) {
        return s_orientation_3d_impls[mode](p1x, p1y, p1z, p2x, p2y, p2z, p3x, p3y, p3z, p4x, p4y, p4z, eps);
    }

    /* 自适应模式 */
    {
        double ax = p2x - p1x, ay = p2y - p1y, az = p2z - p1z;
        double bx = p3x - p1x, by = p3y - p1y, bz = p3z - p1z;
        double cx = p4x - p1x, cy = p4y - p1y, cz = p4z - p1z;

        double det = ax * (by * cz - bz * cy) - ay * (bx * cz - bz * cx) + az * (bx * cy - by * cx);

        /*
         * 3D 行列式每项为三个坐标差之积，量级 O(coord^3)，
         * 因此阈值按 max_coord^3 标度，与 2D 情形同理。
         */
        double max_coord =
            fmax(fmax(fmax(fabs(p1x), fabs(p1y)), fabs(p1z)),
                 fmax(fmax(fmax(fabs(p2x), fabs(p2y)), fabs(p2z)),
                      fmax(fmax(fmax(fabs(p3x), fabs(p3y)), fabs(p3z)), fmax(fmax(fabs(p4x), fabs(p4y)), fabs(p4z)))));
        double threshold = ADAPTIVE_THRESHOLD * max_coord * max_coord * max_coord;

        if (threshold == 0.0) {
            threshold = ADAPTIVE_THRESHOLD;
        }

        if (det > threshold) {
            lv_PREDICATE_STAT_INC(approx_count);
            return lv_ORIENTATION_LEFT;
        } else if (det < -threshold) {
            lv_PREDICATE_STAT_INC(approx_count);
            return lv_ORIENTATION_RIGHT;
        } else {
            lv_PREDICATE_STAT_INC(adaptive_fallback);
            lv_PREDICATE_STAT_INC(exact_count);

            lvInterval result =
                orientation_3d_exact_interval(p1x, p1y, p1z, p2x, p2y, p2z, p3x, p3y, p3z, p4x, p4y, p4z);

            if (result.lo > 0.0) {
                return lv_ORIENTATION_LEFT;
            } else if (result.hi < 0.0) {
                return lv_ORIENTATION_RIGHT;
            } else {
                return lv_ORIENTATION_COPLANAR;
            }
        }
    }
}

/**
 * @brief 判定点相对于有向直线/线段的位置（内部实现）
 *
 * @param px, py 查询点坐标
 * @param x1, y1 直线/线段第一点
 * @param x2, y2 直线/线段第二点
 * @param mode   精度模式
 * @return lvLineSide 点侧枚举（LEFT / RIGHT / ON / DEGENERATE）
 */
static lvLineSide side_of_line_impl(double px, double py, double x1, double y1, double x2, double y2,
                                    lvPredicateMode mode) {
    /* 检查退化情况：定义直线/线段的两个点重合 */
    const lvGeometryConfig *cfg = lv_geometry_get_config();
    double eps = cfg->distance_epsilon; /* lv_geometry_get_config() 恒返回内部快照指针，非 NULL */

    double d_sq = (x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1);
    if (d_sq < eps * eps) {
        return lv_LINE_SIDE_DEGENERATE;
    }

    lvOrientation orient = lv_orientation_2d(x1, y1, x2, y2, px, py, mode);

    switch (orient) {
        case lv_ORIENTATION_LEFT:
            return lv_LINE_SIDE_LEFT;
        case lv_ORIENTATION_RIGHT:
            return lv_LINE_SIDE_RIGHT;
        case lv_ORIENTATION_COLLINEAR:
            return lv_LINE_SIDE_ON;
        default:
            return lv_LINE_SIDE_DEGENERATE;
    }
}

/**
 * @brief 判定点相对于直线的位置
 *
 * 委托给 lv_orientation_2d，将直线方向映射为 lvLineSide。
 *
 * @param px, py   查询点坐标
 * @param lx1, ly1 直线第一点
 * @param lx2, ly2 直线第二点
 * @param mode     精度模式
 * @return lvLineSide 点侧枚举（LEFT / RIGHT / ON / DEGENERATE）
 */
lv_PUBLIC_API lvLineSide lv_line_side(double px, double py, double lx1, double ly1, double lx2, double ly2,
                                      lvPredicateMode mode) {
    return side_of_line_impl(px, py, lx1, ly1, lx2, ly2, mode);
}

/**
 * @brief 判定点相对于有向线段的位置
 *
 * 与 lv_line_side 相同的判定逻辑，语义上针对有向线段。
 *
 * @param px, py   查询点坐标
 * @param sx1, sy1 线段起点
 * @param sx2, sy2 线段终点
 * @param mode     精度模式
 * @return lvLineSide 点侧枚举（LEFT / RIGHT / ON / DEGENERATE）
 */
lvLineSide lv_segment_side(double px, double py, double sx1, double sy1, double sx2, double sy2,
                                         lvPredicateMode mode) {
    return side_of_line_impl(px, py, sx1, sy1, sx2, sy2, mode);
}

/* ── lv_side_of_circle 精度模式实现 ── */

/** @brief 点与圆位置谓词实现函数签名 */
typedef lvSideOfCircle (*SideOfCircleImplFn)(double px, double py, double cx, double cy, double r, double eps);

/** @brief 近似模式实现：直接浮点距离平方差 */
static lvSideOfCircle side_of_circle_approx(double px, double py, double cx, double cy, double r, double eps) {
    lv_PREDICATE_STAT_INC(approx_count);

    double dx = px - cx;
    double dy = py - cy;
    double dist_sq = dx * dx + dy * dy;
    double r_sq = r * r;
    double diff = dist_sq - r_sq;

    if (diff < -eps) {
        return lv_SIDE_INSIDE;
    } else if (diff > eps) {
        return lv_SIDE_OUTSIDE;
    } else {
        return lv_SIDE_ON;
    }
}

/** @brief 精确模式实现：区间算术 */
static lvSideOfCircle side_of_circle_exact(double px, double py, double cx, double cy, double r, double eps) {
    (void) eps;
    lv_PREDICATE_STAT_INC(exact_count);

    lvInterval result = side_of_circle_exact_interval(px, py, cx, cy, r);

    if (result.hi < 0.0) {
        return lv_SIDE_INSIDE;
    } else if (result.lo > 0.0) {
        return lv_SIDE_OUTSIDE;
    } else {
        return lv_SIDE_ON;
    }
}

/** @brief 点与圆位置谓词实现表：[lv_PREDICATE_APPROX / lv_PREDICATE_EXACT] → 实现函数 */
static const SideOfCircleImplFn s_side_of_circle_impls[] = {
    [lv_PREDICATE_APPROX] = side_of_circle_approx,
    [lv_PREDICATE_EXACT] = side_of_circle_exact,
};

/**
 * @brief 判定点相对于圆的位置
 *
 * 使用有符号距离的平方：|p - c|^2 - r^2
 *   < 0 -> INSIDE
 *   = 0 -> ON
 *   > 0 -> OUTSIDE
 *
 * @param px, py 查询点坐标
 * @param cx, cy 圆心坐标
 * @param r      圆半径
 * @param mode   精度模式
 * @return lvSideOfCircle 位置枚举（INSIDE / ON / OUTSIDE / DEGENERATE）
 */
lv_PUBLIC_API lvSideOfCircle lv_side_of_circle(double px, double py, double cx, double cy, double r,
                                               lvPredicateMode mode) {
    /* 检查退化情况：半径为负或零 */
    if (r < 0.0) {
        return lv_SIDE_DEGENERATE;
    }

    const lvGeometryConfig *cfg = lv_geometry_get_config();
    double eps = cfg->distance_epsilon; /* lv_geometry_get_config() 恒返回内部快照指针，非 NULL */

    /* 模式归一化：SYMBOLIC → EXACT（符号模式暂回退到精确模式） */
    mode = normalize_predicate_mode(mode);

    /* 查表分发 APPROX / EXACT 实现 */
    if ((unsigned) mode < lv_ARRAY_SIZE(s_side_of_circle_impls) && s_side_of_circle_impls[mode]) {
        return s_side_of_circle_impls[mode](px, py, cx, cy, r, eps);
    }

    /* 自适应模式 */
    {
        double dx = px - cx;
        double dy = py - cy;
        double dist_sq = dx * dx + dy * dy;
        double r_sq = r * r;
        double diff = dist_sq - r_sq;

        /*
         * diff = |p-c|² - r² 的量级为 O(coord²)：
         * |p-c|² = (px-cx)² + (py-cy)²，每项为坐标差的平方，
         * 因此整体缩放 λ²。阈值取 max_coord² 标度即可。
         */
        double max_coord = fmax(fmax(fabs(px), fabs(py)), fmax(fmax(fabs(cx), fabs(cy)), fabs(r)));
        double threshold = ADAPTIVE_THRESHOLD * max_coord * max_coord;

        if (threshold == 0.0) {
            threshold = ADAPTIVE_THRESHOLD;
        }

        if (diff < -threshold) {
            lv_PREDICATE_STAT_INC(approx_count);
            return lv_SIDE_INSIDE;
        } else if (diff > threshold) {
            lv_PREDICATE_STAT_INC(approx_count);
            return lv_SIDE_OUTSIDE;
        } else {
            lv_PREDICATE_STAT_INC(adaptive_fallback);
            lv_PREDICATE_STAT_INC(exact_count);

            lvInterval result = side_of_circle_exact_interval(px, py, cx, cy, r);

            if (result.hi < 0.0) {
                return lv_SIDE_INSIDE;
            } else if (result.lo > 0.0) {
                return lv_SIDE_OUTSIDE;
            } else {
                return lv_SIDE_ON;
            }
        }
    }
}

/**
 * @brief 判定两点是否在直线同侧
 *
 * 使用两个 line_side 判定：如果两个点都在直线的同一侧（同为 LEFT 或同为 RIGHT），
 * 则返回 true。如果任一点在直线上，也视为同侧。
 *
 * @param ax, ay   第一个点坐标
 * @param bx, by   第二个点坐标
 * @param lx1, ly1 直线第一点
 * @param lx2, ly2 直线第二点
 * @param mode     精度模式
 * @return true 两点在直线同侧（或一点/两点在直线上）
 */
lv_PUBLIC_API bool lv_same_side_of_line(double ax, double ay, double bx, double by, double lx1, double ly1, double lx2,
                                        double ly2, lvPredicateMode mode) {
    lvLineSide side_a = lv_line_side(ax, ay, lx1, ly1, lx2, ly2, mode);
    lvLineSide side_b = lv_line_side(bx, by, lx1, ly1, lx2, ly2, mode);

    /* 退化情况 */
    if (side_a == lv_LINE_SIDE_DEGENERATE || side_b == lv_LINE_SIDE_DEGENERATE) {
        return false;
    }

    /* 如果两点都在直线上，视为同侧 */
    if (side_a == lv_LINE_SIDE_ON && side_b == lv_LINE_SIDE_ON) {
        return true;
    }

    /* 如果其中一个点在直线上，视为同侧 */
    if (side_a == lv_LINE_SIDE_ON || side_b == lv_LINE_SIDE_ON) {
        return true;
    }

    /* 两点在同一侧 */
    return (side_a == side_b);
}

/**
 * @brief 判定两点是否在圆同侧
 *
 * 使用两个 side_of_circle 判定：两点同时在圆内、同时在圆外、
 * 或任一点在圆上时返回 true。
 *
 * @param ax, ay 第一个点坐标
 * @param bx, by 第二个点坐标
 * @param cx, cy 圆心坐标
 * @param r      圆半径
 * @param mode   精度模式
 * @return true 两点在圆同侧（或一点/两点在圆上）
 */
bool lv_same_side_of_circle(double ax, double ay, double bx, double by, double cx, double cy, double r,
                                          lvPredicateMode mode) {
    lvSideOfCircle side_a = lv_side_of_circle(ax, ay, cx, cy, r, mode);
    lvSideOfCircle side_b = lv_side_of_circle(bx, by, cx, cy, r, mode);

    /* 退化情况 */
    if (side_a == lv_SIDE_DEGENERATE || side_b == lv_SIDE_DEGENERATE) {
        return false;
    }

    /* 两点都在圆上，视为同侧 */
    if (side_a == lv_SIDE_ON && side_b == lv_SIDE_ON) {
        return true;
    }

    /* 其中一个在圆上，视为同侧 */
    if (side_a == lv_SIDE_ON || side_b == lv_SIDE_ON) {
        return true;
    }

    /* 两点在同一侧 */
    return (side_a == side_b);
}

/**
 * @brief 判定两条线段是否相交（精确判定）
 *
 * 使用方向谓词判定：
 *   线段 AB 和线段 CD 相交，当且仅当：
 *   1. C 和 D 在 AB 的两侧（或至少一个在 AB 上），且
 *   2. A 和 B 在 CD 的两侧（或至少一个在 CD 上）
 *
 * 特殊处理端点共线及重合的退化情况。
 *
 * @param ax, ay 线段 A 的端点
 * @param bx, by 线段 B 的端点
 * @param cx, cy 线段 C 的端点
 * @param dx, dy 线段 D 的端点
 * @param mode   精度模式
 * @return true 两条线段相交（含端点接触）
 */
lv_PUBLIC_API bool lv_segments_intersect(double ax, double ay, double bx, double by, double cx, double cy, double dx,
                                         double dy, lvPredicateMode mode) {
    /*
     * 方向谓词：
     *   d1 = orientation(A, B, C)  -- C 相对于 AB 的方向
     *   d2 = orientation(A, B, D)  -- D 相对于 AB 的方向
     *   d3 = orientation(C, D, A)  -- A 相对于 CD 的方向
     *   d4 = orientation(C, D, B)  -- B 相对于 CD 的方向
     *
     * 一般相交：d1 和 d2 异号，且 d3 和 d4 异号
     * 共线相交：d1==0 且 C 在 AB 上，或类似情况
     */
    lvOrientation d1 = lv_orientation_2d(ax, ay, bx, by, cx, cy, mode);
    lvOrientation d2 = lv_orientation_2d(ax, ay, bx, by, dx, dy, mode);
    lvOrientation d3 = lv_orientation_2d(cx, cy, dx, dy, ax, ay, mode);
    lvOrientation d4 = lv_orientation_2d(cx, cy, dx, dy, bx, by, mode);

    /* 一般相交情况：C 和 D 在 AB 两侧，A 和 B 在 CD 两侧 */
    if (((d1 == lv_ORIENTATION_LEFT && d2 == lv_ORIENTATION_RIGHT) ||
         (d1 == lv_ORIENTATION_RIGHT && d2 == lv_ORIENTATION_LEFT)) &&
        ((d3 == lv_ORIENTATION_LEFT && d4 == lv_ORIENTATION_RIGHT) ||
         (d3 == lv_ORIENTATION_RIGHT && d4 == lv_ORIENTATION_LEFT))) {
        return true;
    }

    /*
     * 共线情况：如果所有点共线，需要检查线段是否重叠。
     * 使用 bounding box 检查。
     *
     * 方向谓词只能判断点是否在无限直线上，无法判断点是否在
     * 线段"内部"——共线点可能在线段延长线上。因此需要通过
     * 一维区间重叠（含 epsilon 容差）来确认。
     */
    if (d1 == lv_ORIENTATION_COLLINEAR && d2 == lv_ORIENTATION_COLLINEAR && d3 == lv_ORIENTATION_COLLINEAR &&
        d4 == lv_ORIENTATION_COLLINEAR) {
        /*
         * 检查 C 或 D 是否在 AB 的 bounding box 内，
         * 或 A 或 B 是否在 CD 的 bounding box 内。
         */
        int c_on_ab = (cx >= fmin(ax, bx) - lv_GEO_DISTANCE_EPSILON && cx <= fmax(ax, bx) + lv_GEO_DISTANCE_EPSILON &&
                       cy >= fmin(ay, by) - lv_GEO_DISTANCE_EPSILON && cy <= fmax(ay, by) + lv_GEO_DISTANCE_EPSILON);
        int d_on_ab = (dx >= fmin(ax, bx) - lv_GEO_DISTANCE_EPSILON && dx <= fmax(ax, bx) + lv_GEO_DISTANCE_EPSILON &&
                       dy >= fmin(ay, by) - lv_GEO_DISTANCE_EPSILON && dy <= fmax(ay, by) + lv_GEO_DISTANCE_EPSILON);
        int a_on_cd = (ax >= fmin(cx, dx) - lv_GEO_DISTANCE_EPSILON && ax <= fmax(cx, dx) + lv_GEO_DISTANCE_EPSILON &&
                       ay >= fmin(cy, dy) - lv_GEO_DISTANCE_EPSILON && ay <= fmax(cy, dy) + lv_GEO_DISTANCE_EPSILON);
        int b_on_cd = (bx >= fmin(cx, dx) - lv_GEO_DISTANCE_EPSILON && bx <= fmax(cx, dx) + lv_GEO_DISTANCE_EPSILON &&
                       by >= fmin(cy, dy) - lv_GEO_DISTANCE_EPSILON && by <= fmax(cy, dy) + lv_GEO_DISTANCE_EPSILON);

        return (c_on_ab || d_on_ab || a_on_cd || b_on_cd);
    }

    /*
     * 端点在另一线段上的情况：
     * 方向异号表示两线段交叉，而此时其中一个端点恰好在另一线段上。
     * 共线点还需要 bounding box 验证（防止延长线上的误判）。
     */
    if (d1 == lv_ORIENTATION_COLLINEAR) {
        int c_on_ab = (cx >= fmin(ax, bx) - lv_GEO_DISTANCE_EPSILON && cx <= fmax(ax, bx) + lv_GEO_DISTANCE_EPSILON &&
                       cy >= fmin(ay, by) - lv_GEO_DISTANCE_EPSILON && cy <= fmax(ay, by) + lv_GEO_DISTANCE_EPSILON);
        if (c_on_ab && ((d3 == lv_ORIENTATION_LEFT && d4 == lv_ORIENTATION_RIGHT) ||
                        (d3 == lv_ORIENTATION_RIGHT && d4 == lv_ORIENTATION_LEFT))) {
            return true;
        }
    }

    if (d2 == lv_ORIENTATION_COLLINEAR) {
        int d_on_ab = (dx >= fmin(ax, bx) - lv_GEO_DISTANCE_EPSILON && dx <= fmax(ax, bx) + lv_GEO_DISTANCE_EPSILON &&
                       dy >= fmin(ay, by) - lv_GEO_DISTANCE_EPSILON && dy <= fmax(ay, by) + lv_GEO_DISTANCE_EPSILON);
        if (d_on_ab && ((d3 == lv_ORIENTATION_LEFT && d4 == lv_ORIENTATION_RIGHT) ||
                        (d3 == lv_ORIENTATION_RIGHT && d4 == lv_ORIENTATION_LEFT))) {
            return true;
        }
    }

    if (d3 == lv_ORIENTATION_COLLINEAR) {
        int a_on_cd = (ax >= fmin(cx, dx) - lv_GEO_DISTANCE_EPSILON && ax <= fmax(cx, dx) + lv_GEO_DISTANCE_EPSILON &&
                       ay >= fmin(cy, dy) - lv_GEO_DISTANCE_EPSILON && ay <= fmax(cy, dy) + lv_GEO_DISTANCE_EPSILON);
        if (a_on_cd && ((d1 == lv_ORIENTATION_LEFT && d2 == lv_ORIENTATION_RIGHT) ||
                        (d1 == lv_ORIENTATION_RIGHT && d2 == lv_ORIENTATION_LEFT))) {
            return true;
        }
    }

    if (d4 == lv_ORIENTATION_COLLINEAR) {
        int b_on_cd = (bx >= fmin(cx, dx) - lv_GEO_DISTANCE_EPSILON && bx <= fmax(cx, dx) + lv_GEO_DISTANCE_EPSILON &&
                       by >= fmin(cy, dy) - lv_GEO_DISTANCE_EPSILON && by <= fmax(cy, dy) + lv_GEO_DISTANCE_EPSILON);
        if (b_on_cd && ((d1 == lv_ORIENTATION_LEFT && d2 == lv_ORIENTATION_RIGHT) ||
                        (d1 == lv_ORIENTATION_RIGHT && d2 == lv_ORIENTATION_LEFT))) {
            return true;
        }
    }

    return false;
}

/**
 * @brief 判定点是否在三角形内部（含边界）
 *
 * 使用三个方向谓词判定：
 *   - orientation(A, B, P)
 *   - orientation(B, C, P)
 *   - orientation(C, A, P)
 *
 * 如果三个方向一致（全部同向或包含共线），则点在三角形内部（含边界）。
 *
 * @param px, py 查询点坐标
 * @param ax, ay 三角形顶点 A
 * @param bx, by 三角形顶点 B
 * @param cx, cy 三角形顶点 C
 * @param mode   精度模式
 * @return true 点在三角形内部或边界上
 */
lv_PUBLIC_API bool lv_point_in_triangle(double px, double py, double ax, double ay, double bx, double by, double cx,
                                        double cy, lvPredicateMode mode) {
    lvOrientation o1 = lv_orientation_2d(ax, ay, bx, by, px, py, mode);
    lvOrientation o2 = lv_orientation_2d(bx, by, cx, cy, px, py, mode);
    lvOrientation o3 = lv_orientation_2d(cx, cy, ax, ay, px, py, mode);

    /*
     * 点在三角形内部（含边界）的条件：
     * 三个方向谓词全部非负（逆时针三角形）或全部非正（顺时针三角形）。
     *
     * 即：不存在一个为 LEFT 而另一个为 RIGHT 的情况。
     */
    int has_left = (o1 == lv_ORIENTATION_LEFT) || (o2 == lv_ORIENTATION_LEFT) || (o3 == lv_ORIENTATION_LEFT);
    int has_right = (o1 == lv_ORIENTATION_RIGHT) || (o2 == lv_ORIENTATION_RIGHT) || (o3 == lv_ORIENTATION_RIGHT);

    /* 如果同时存在 LEFT 和 RIGHT，则点在三角形外部 */
    if (has_left && has_right) {
        return false;
    }

    return true;
}

/* ── lv_four_points_concyclic 精度模式实现 ── */

/** @brief 四点共圆谓词实现函数签名 */
typedef bool (*FourPointsConcyclicImplFn)(double ax, double ay, double bx, double by, double cx, double cy, double dx,
                                          double dy, double eps);

/** @brief 近似模式实现：直接浮点 4x4 行列式（归一化后与 eps 比较） */
static bool four_points_concyclic_approx(double ax, double ay, double bx, double by, double cx, double cy, double dx,
                                         double dy, double eps) {
    lv_PREDICATE_STAT_INC(approx_count);

    double a2 = ax * ax + ay * ay;
    double b2 = bx * bx + by * by;
    double c2 = cx * cx + cy * cy;
    double d2 = dx * dx + dy * dy;

    /*
     * Laplace 展开沿最后一列：
     * det = M11 - M21 + M31 - M41
     *
     * M11 = by*(c2-d2) - b2*(cy-dy) + (cy*d2 - c2*dy)
     * M21 = bx*(c2-d2) - a2*(cx-dx) + (cx*d2 - c2*dx)
     * M31 = ax*(b2-d2) - a2*(bx-dx) + (bx*d2 - b2*dx)
     * M41 = ax*(b2-c2) - a2*(bx-cx) + (bx*c2 - b2*cx)
     */
    double m11 = by * (c2 - d2) - b2 * (cy - dy) + (cy * d2 - c2 * dy);
    double m21 = bx * (c2 - d2) - a2 * (cx - dx) + (cx * d2 - c2 * dx);
    double m31 = ax * (b2 - d2) - a2 * (bx - dx) + (bx * d2 - b2 * dx);
    double m41 = ax * (b2 - c2) - a2 * (bx - cx) + (bx * c2 - b2 * cx);

    double det = m11 - m21 + m31 - m41;

    /*
     * 归一化：按坐标量级缩放行列式，使其与容差 eps 可比。
     *
     * 共圆 4×4 行列式经 Laplace 展开后，每项 3×3 子行列式
     * 含两列坐标（λ）和一列坐标平方（λ²），总计 O(λ⁴)。
     * 因此归一化因子应为 max_coord⁴ 而非 max_coord³。
     *
     * 推导：子行列式 M11 = |bx by bx²+by²; cx cy cx²+cy²; dx dy dx²+dy²|
     *       缩放后每列因子为 λ, λ, λ²，子行列式缩放 λ⁴。
     */
    double max_coord = fmax(fmax(fmax(fabs(ax), fabs(ay)), fmax(fabs(bx), fabs(by))),
                            fmax(fmax(fabs(cx), fabs(cy)), fmax(fabs(dx), fabs(dy))));
    if (max_coord > 1.0) {
        double s = max_coord * max_coord;
        det /= (s * s);
    }

    return fabs(det) < eps;
}

/** @brief 精确模式实现：区间算术 */
static bool four_points_concyclic_exact(double ax, double ay, double bx, double by, double cx, double cy, double dx,
                                        double dy, double eps) {
    (void) eps;
    lv_PREDICATE_STAT_INC(exact_count);

    lvInterval result = four_points_concyclic_exact_interval(ax, ay, bx, by, cx, cy, dx, dy);

    /* 如果区间包含零，则可能共圆 */
    return (result.lo <= 0.0 && result.hi >= 0.0);
}

/** @brief 四点共圆谓词实现表：[lv_PREDICATE_APPROX / lv_PREDICATE_EXACT] → 实现函数 */
static const FourPointsConcyclicImplFn s_four_points_concyclic_impls[] = {
    [lv_PREDICATE_APPROX] = four_points_concyclic_approx,
    [lv_PREDICATE_EXACT] = four_points_concyclic_exact,
};

/**
 * @brief 判定四点是否共圆
 *
 * 使用 4x4 行列式判定：
 *   | ax  ay  ax^2+ay^2  1 |
 *   | bx  by  bx^2+by^2  1 | = 0
 *   | cx  cy  cx^2+cy^2  1 |
 *   | dx  dy  dx^2+dy^2  1 |
 *
 * @param ax, ay 第一个点
 * @param bx, by 第二个点
 * @param cx, cy 第三个点
 * @param dx, dy 第四个点
 * @param mode   精度模式
 * @return true 四点共圆
 */
lv_PUBLIC_API bool lv_four_points_concyclic(double ax, double ay, double bx, double by, double cx, double cy, double dx,
                                            double dy, lvPredicateMode mode) {
    const lvGeometryConfig *cfg = lv_geometry_get_config();
    double eps = cfg->collinear_epsilon; /* lv_geometry_get_config() 恒返回内部快照指针，非 NULL */

    /* 模式归一化：SYMBOLIC → EXACT（符号模式暂回退到精确模式） */
    mode = normalize_predicate_mode(mode);

    /* 查表分发 APPROX / EXACT 实现 */
    if ((unsigned) mode < lv_ARRAY_SIZE(s_four_points_concyclic_impls) && s_four_points_concyclic_impls[mode]) {
        return s_four_points_concyclic_impls[mode](ax, ay, bx, by, cx, cy, dx, dy, eps);
    }

    /* 自适应模式 */
    {
        double a2 = ax * ax + ay * ay;
        double b2 = bx * bx + by * by;
        double c2 = cx * cx + cy * cy;
        double d2 = dx * dx + dy * dy;

        double m11 = by * (c2 - d2) - b2 * (cy - dy) + (cy * d2 - c2 * dy);
        double m21 = bx * (c2 - d2) - a2 * (cx - dx) + (cx * d2 - c2 * dx);
        double m31 = ax * (b2 - d2) - a2 * (bx - dx) + (bx * d2 - b2 * dx);
        double m41 = ax * (b2 - c2) - a2 * (bx - cx) + (bx * c2 - b2 * cx);

        double det = m11 - m21 + m31 - m41;

        /*
         * 共圆 4×4 行列式经 Laplace 展开后，每项 3×3 子行列式
         * 含两列坐标（λ）和一列坐标平方（λ²），总计 O(λ⁴)。
         * 因此自适应阈值取 max_coord⁴ 标度。
         */
        double max_coord = fmax(fmax(fmax(fabs(ax), fabs(ay)), fmax(fabs(bx), fabs(by))),
                                fmax(fmax(fabs(cx), fabs(cy)), fmax(fabs(dx), fabs(dy))));
        double s = max_coord * max_coord;
        double threshold = ADAPTIVE_THRESHOLD * s * s;

        if (threshold == 0.0) {
            threshold = ADAPTIVE_THRESHOLD;
        }

        if (fabs(det) > threshold) {
            lv_PREDICATE_STAT_INC(approx_count);
            return false; /* 明确不为零，不共圆 */
        } else {
            lv_PREDICATE_STAT_INC(adaptive_fallback);
            lv_PREDICATE_STAT_INC(exact_count);

            lvInterval result = four_points_concyclic_exact_interval(ax, ay, bx, by, cx, cy, dx, dy);

            return (result.lo <= 0.0 && result.hi >= 0.0);
        }
    }
}

/**
 * @brief 判定多边形是否为凸多边形
 *
 * 检查所有连续三顶点的方向是否一致（全部同向或全部反向）。
 * 允许共线顶点（退化边）。
 *
 * @param xs, ys 顶点坐标数组
 * @param n      顶点数量（必须 >= 3）
 * @param mode   精度模式
 * @return true 多边形为凸，false 为非凸或输入无效
 */
lv_PUBLIC_API bool lv_polygon_is_convex(const double *xs, const double *ys, int n, lvPredicateMode mode) {
    if (n < 3 || xs == NULL || ys == NULL) {
        return false;
    }

    /*
     * 检查所有连续三顶点的方向。
     * first_non_collinear 记录第一个非共线方向，后续所有方向必须与之一致。
     */
    int first_sign = 0; /* 0 = 未确定, 1 = LEFT, -1 = RIGHT */

    for (int i = 0; i < n; i++) {
        int i1 = (i + 1) % n;
        int i2 = (i + 2) % n;

        lvOrientation orient = lv_orientation_2d(xs[i], ys[i], xs[i1], ys[i1], xs[i2], ys[i2], mode);

        if (orient == lv_ORIENTATION_LEFT) {
            if (first_sign == 0) {
                first_sign = 1;
            } else if (first_sign == -1) {
                return false; /* 方向不一致 */
            }
        } else if (orient == lv_ORIENTATION_RIGHT) {
            if (first_sign == 0) {
                first_sign = -1;
            } else if (first_sign == 1) {
                return false; /* 方向不一致 */
            }
        }
        /* COLLINEAR 和 DEGENERATE 不影响判定 */
    }

    return true;
}

/**
 * @brief 判定点是否在凸多边形内部
 *
 * 使用二分法判定，O(log n) 复杂度。
 * 要求多边形顶点按逆时针或顺时针顺序排列。
 *
 * 算法：
 *   1. 确定多边形的绕行方向（通过第一个非共线三顶点）
 *   2. 使用二分法找到点所在的扇形区域
 *   3. 检查点是否在该扇形区域的三角形内
 *
 * @param px, py   查询点坐标
 * @param xs, ys   凸多边形顶点坐标数组
 * @param n        顶点数量（必须 >= 3）
 * @param mode     精度模式
 * @return true 点在凸多边形内部或边界上
 */
bool lv_point_in_convex_polygon(double px, double py, const double *xs, const double *ys, int n,
                                              lvPredicateMode mode) {
    if (n < 3 || xs == NULL || ys == NULL) {
        return false;
    }

    /*
     * 确定多边形绕行方向。
     * 找到第一个非共线的连续三顶点。
     */
    int ccw = 0; /* 0 = 未知, 1 = 逆时针, -1 = 顺时针 */
    for (int i = 0; i < n; i++) {
        int i1 = (i + 1) % n;
        int i2 = (i + 2) % n;
        lvOrientation orient = lv_orientation_2d(xs[i], ys[i], xs[i1], ys[i1], xs[i2], ys[i2], mode);
        if (orient == lv_ORIENTATION_LEFT) {
            ccw = 1;
            break;
        } else if (orient == lv_ORIENTATION_RIGHT) {
            ccw = -1;
            break;
        }
    }

    if (ccw == 0) {
        /* 所有三顶点共线，退化多边形 */
        return false;
    }

    /*
     * 二分法搜索。
     * 在 [1, n-1] 范围内搜索，使得点在三角形 (v0, v[lo], v[hi]) 内。
     */
    int lo = 1;
    int hi = n - 1;

    while (hi - lo > 1) {
        int mid = lo + (hi - lo) / 2;

        /*
         * 检查点相对于边 (v0, v[mid]) 的方向。
         * 逆时针多边形：点应在左侧
         * 顺时针多边形：点应在右侧
         */
        lvOrientation orient = lv_orientation_2d(xs[0], ys[0], xs[mid], ys[mid], px, py, mode);

        if (ccw == 1) {
            /* 逆时针：点在左侧则缩小上界 */
            if (orient == lv_ORIENTATION_LEFT) {
                hi = mid;
            } else {
                lo = mid;
            }
        } else {
            /* 顺时针：点在右侧则缩小上界 */
            if (orient == lv_ORIENTATION_RIGHT) {
                hi = mid;
            } else {
                lo = mid;
            }
        }
    }

    /* 检查点是否在三角形 (v0, v[lo], v[hi]) 内 */
    return lv_point_in_triangle(px, py, xs[0], ys[0], xs[lo], ys[lo], xs[hi], ys[hi], mode);
}

/**
 * @brief 判定点是否在任意多边形内部（射线法）
 *
 * 从点发出水平向右的射线，统计与多边形边的交点数。
 * 奇数交点 -> 内部，偶数交点 -> 外部。
 *
 * 注意处理射线经过顶点的退化情况。
 *
 * @param px, py 查询点坐标
 * @param xs, ys 多边形顶点坐标数组
 * @param n      顶点数量（必须 >= 3）
 * @param mode   精度模式（仅用于顶点重合判断的容差）
 * @return true 点在多边形内部或边界上
 */
lv_PUBLIC_API bool lv_point_in_polygon(double px, double py, const double *xs, const double *ys, int n,
                                       lvPredicateMode mode) {
    if (n < 3 || xs == NULL || ys == NULL) {
        return false;
    }

    const lvGeometryConfig *cfg = lv_geometry_get_config();
    double eps = cfg->collinear_epsilon; /* lv_geometry_get_config() 恒返回内部快照指针，非 NULL */

    int crossings = 0;

    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;

        double xi = xs[i], yi = ys[i];
        double xj = xs[j], yj = ys[j];

        /*
         * 射线法标准实现：
         * 只考虑边跨越射线所在的水平线（y = py）的情况。
         *
         * 策略：当边的两个端点一个在射线上方、一个在射线下方（或恰好在射线上）时，
         * 计算交点的 x 坐标，如果交点在点的右侧则计数。
         *
         * 使用 "half-open" 区间避免顶点被重复计数：
         *   - 上端点包含（yi >= py），下端点不包含（yj < py）
         *   - 这样每个顶点恰好被一条边"拥有"
         */
        if (((yi > py) != (yj > py))) {
            /*
             * 计算射线与边的交点的 x 坐标。
             * 使用线性插值：x_intersect = xi + (py - yi) * (xj - xi) / (yj - yi)
             */
            double x_intersect = xi + (py - yi) * (xj - xi) / (yj - yi);

            if (x_intersect > px - eps) {
                crossings++;
            }
        } else if (fabs(yi - py) < eps && fabs(yj - py) < eps) {
            /*
             * 边与射线共线（水平边）。
             * 如果点在线段的 bounding box 内，则点在多边形边界上。
             * 直接返回 true（含边界）。
             */
            double x_min = fmin(xi, xj);
            double x_max = fmax(xi, xj);
            if (px >= x_min - eps && px <= x_max + eps) {
                return true;
            }
        }
    }

    /* 奇数交点 -> 内部 */
    return (crossings % 2) != 0;
}

/* ========================================================================
 * 第三部分：谓词统计与配置
 * ======================================================================== */

/**
 * @brief 获取谓词统计信息
 *
 * @param stats [out] 输出统计信息的缓冲区（不可为 NULL）
 */
lv_PUBLIC_API void lv_predicate_get_stats(lvPredicateStats *stats) {
    if (stats != NULL) {
        stats->approx_count = atomic_load_explicit(&g_predicate_stats.approx_count, memory_order_relaxed);
        stats->exact_count = atomic_load_explicit(&g_predicate_stats.exact_count, memory_order_relaxed);
        stats->adaptive_fallback = atomic_load_explicit(&g_predicate_stats.adaptive_fallback, memory_order_relaxed);
    }
}

/**
 * @brief 重置谓词统计信息
 *
 * 将统计计数器清零，通常在开始新的测试或算例前调用。
 */
lv_PUBLIC_API void lv_predicate_reset_stats(void) {
    atomic_store_explicit(&g_predicate_stats.approx_count, 0, memory_order_relaxed);
    atomic_store_explicit(&g_predicate_stats.exact_count, 0, memory_order_relaxed);
    atomic_store_explicit(&g_predicate_stats.adaptive_fallback, 0, memory_order_relaxed);
}

/**
 * @brief 设置全局谓词精度模式
 *
 * @param mode 新的精度模式（APPROX / EXACT / ADAPTIVE / SYMBOLIC）
 */
lv_PUBLIC_API void lv_predicate_set_mode(lvPredicateMode mode) {
    g_predicate_mode = mode;
}

/**
 * @brief 获取全局谓词精度模式
 *
 * @return 当前精度模式（APPROX / EXACT / ADAPTIVE / SYMBOLIC）
 */
lv_PUBLIC_API lvPredicateMode lv_predicate_get_mode(void) {
    return g_predicate_mode;
}

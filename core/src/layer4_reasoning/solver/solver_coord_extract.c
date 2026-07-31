/**
 * @file solver_coord_extract.c
 * @brief 坐标提取与方程提取
 *
 * @details 从 solver.c 拆分出的子模块（Lv-00 项目 v3.3.0+）。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "solver_common.h"
#include "lv/coeff_pool.h" /* 共享的多项式系数内存池（实现与池拥有权见 lv/coeff_pool.h） */

/* ── 多项式系数内存池 ──
 * 使用 lv/coeff_pool.h 提供的共享池：g_coeff_pool 拥有权在
 * symbolic_coord_ops.c，本文件不再维护私有池与重复实现。
 * 池内块耗尽时 lv_mempool_alloc 返回 NULL，coeff_pool_alloc 回退到
 * lv_malloc；coeff_pool_clear 对回退指针因地址越界检查安全跳过。
 */

/** @brief 添加方程到方程系统（声明在 solver_types.h 中） */
static int equation_system_push_impl(EquationSystem *sys, mpz_poly_t poly, int var_node_id, int coord_index) {
    if (!lv_darray_reserve(&sys->eqs, sys->eqs.count + 1))
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "equation_system_push_impl: lv_darray_reserve failed (count=%d)", sys->eqs.count + 1);
    PolyEquation *slot = (PolyEquation *)((char *)sys->eqs.data + (size_t)sys->eqs.count * sizeof(PolyEquation));
    slot->var_node_id = var_node_id;
    slot->coord_index = coord_index;
    mpz_poly_init(&slot->poly);
    mpz_poly_set(&slot->poly, &poly);
    sys->eqs.count++;
    return 0;
}

/* equation_system_push/clear 实现在 solver_eq_system.c 中，由 solver_types.h 声明 */

/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/*  内部：从 SymbolicCoord 提取数值的辅助函数                          */
/* ------------------------------------------------------------------ */

/* 尝试从 SymbolicCoord 获取 double 近似值。
   参数 c 带有 const 限定符，确保不会修改原始坐标。
   返回 true 表示成功获取近似值。 */

/**
 * @brief 通过序列化方式将符号坐标转换为浮点数（通用路径）
 * @param c 符号坐标
 * @param out 输出双精度值
 * @return 成功返回 true，失败返回 false
 *
 * 适用于 QUADRATIC、ALGEBRAIC、TRANSCENDENTAL 等类型的坐标转换。
 * 通过序列化为字符串再解析为浮点数实现，精度受限于 strtod。
 */
static bool coord_to_double_via_serialize(const SymbolicCoord *c, double *out) {
    if (!c || !out)
        return false;
    char *str = symbolic_coord_serialize(c);
    if (!str)
        return false;
    char *endptr = NULL;
    *out = strtod(str, &endptr);
    lv_free((void **) &str);
    return (endptr != str);
}

int coord_to_double(const SymbolicCoord *c, double *out) {
    if (!c)
        return false;
    switch (c->type) {
        case RATIONAL: {
            if (c->data.rational) {
                *out = mpq_get_d(c->data.rational->value);
                return true;
            }
            /* 数据不一致：RATIONAL 类型但 rational 指针为空 */
            lv_LOG_WARNING("coord_to_double: RATIONAL 类型但 data.rational 为 NULL");
            *out = 0.0;
            return false;
        }
        case QUADRATIC:
        case ALGEBRAIC:
            return coord_to_double_via_serialize(c, out);
        case TRANSCENDENTAL: {
            /* 使用公共 API 获取超越常数的数值近似值（如 pi, e 等） */
            *out = symbolic_coord_to_double(c);
            return (fabs(*out) > lv_EPSILON_DOUBLE || c->data.transcendental != NULL);
        }
        default:
            return false;
    }
}

/* =======================================================================
 * 内部函数：将 double 值转换为带缩放的 mpz_t
 * 当可能时从 SymbolicCoord 进行精确有理数提取
 * ======================================================================= */

/**
 * @brief 将有理数乘以缩放系数后向下取整为 mpz_t
 *
 * 计算 floor((mpq_numref(val) * scale) / mpq_denref(val))。
 * 此辅助函数消除了 coord_to_mpz_scaled_exact() 中 RATIONAL、
 * QUADRATIC(b=0)、ALGEBRAIC(cached_rational) 等多个分支中
 * 重复的"有理数乘缩放→取整"模式。
 *
 * @param val    输入有理数值（mpq_srcptr，只读）
 * @param result 输出 mpz_t 整数，结果直接写入该变量
 * @param scale  缩放系数
 */
static void rational_to_mpz_scaled(mpq_srcptr val, mpz_t result, int64_t scale) {
    mpz_t scaled;
    mpz_init(scaled);
    mpz_set_si(scaled, (long) scale);
    mpz_mul(result, mpq_numref(val), scaled);
    mpz_fdiv_q(result, result, mpq_denref(val));
    mpz_clear(scaled);
}

/**
 * @brief 将 SymbolicCoord 值转换为按 scale 缩放的 mpz_t 整数
 *
 * 如果坐标是 RATIONAL 类型，提取精确的 mpq_t 值并乘以 scale 得到精确整数（无精度损失）。
 * 对于非 RATIONAL 类型，回退到 double 近似值。
 *
 * @param c      SymbolicCoord 指针
 * @param result 输出 mpz_t 整数
 * @param scale  缩放因子
 * @return true 表示成功（精确或近似），false 表示失败
 */
bool coord_to_mpz_scaled(const SymbolicCoord *c, mpz_t result, int64_t scale) {
    if (!c)
        return false;

    if (c->type == RATIONAL && c->data.rational) {
        mpq_srcptr val = c->data.rational->value;

        /* 计算分子 * scale（只计算一次，避免重复） */
        mpz_t scaled_num;
        mpz_init(scaled_num);
        mpz_mul_si(scaled_num, mpq_numref(val), (long) scale);

        mpz_t den;
        mpz_init(den);
        mpz_set(den, mpq_denref(val));

        if (mpz_divisible_p(scaled_num, den)) {
            /* 精确整除：直接取商 */
            mpz_fdiv_q(result, scaled_num, den);
        } else {
            /* 非精确整除：使用银行家舍入（round to nearest even）避免系统性偏差 */
            mpz_t q, r;
            mpz_init(q);
            mpz_init(r);
            mpz_fdiv_qr(q, r, scaled_num, den);
            mpz_t half_den;
            mpz_init(half_den);
            mpz_fdiv_q_2exp(half_den, den, 1);
            /*
             * 临界盲区修复：当剩余 r 与 half_den 相等时，使用银行家舍入
             * (round to nearest even) 而非单纯向上舍入，避免系统性偏差。
             */
            int cmp = mpz_cmp(r, half_den);
            if (cmp > 0) {
                mpz_add_ui(result, q, 1);
            } else if (cmp < 0) {
                mpz_set(result, q);
            } else {
                /* 临界点：r == half_den，使用银行家舍入 */
                if (mpz_even_p(q)) {
                    mpz_set(result, q);
                } else {
                    mpz_add_ui(result, q, 1);
                }
            }
            mpz_clear(half_den);
            mpz_clear(q);
            mpz_clear(r);
        }

        mpz_clear(scaled_num);
        mpz_clear(den);
        return true;
    }

    /* 回退：使用 double 近似值 */
    double d;
    if (coord_to_double(c, &d)) {
        mpz_set_d(result, d * (double) scale);
        return true;
    }

    return false;
}

/* 前向声明：coord_to_mpz_scaled_exact 在 double_to_mpz_scaled 之后定义，
   但 coord_to_mpz_scaled 的回退路径需要引用 double_to_mpz_scaled */
void double_to_mpz_scaled(double val, mpz_t result, int64_t scale);

/**
 * 将符号坐标精确转换为缩放整数系数。
 * 对于有理数坐标，使用精确的 mpq -> mpz 转换。
 * 对于二次根式和代数数，尝试有理化后转换；失败时回退到 double 近似。
 * 返回 true 表示精确转换成功，false 表示使用了近似。
 */
static bool coord_to_mpz_scaled_exact(const SymbolicCoord *coord, mpz_t result, int64_t scale) {
    if (!coord)
        return false;

    switch (coord->type) {
        case RATIONAL: {
            if (!coord->data.rational)
                return false;
            rational_to_mpz_scaled(coord->data.rational->value, result, scale);
            return true;
        }
        case QUADRATIC: {
            /* 尝试有理化：如果 b=0，退化为有理数 */
            Quadratic *q = coord->data.quadratic;
            if (q && mpq_sgn(q->b->value) == 0) {
                rational_to_mpz_scaled(q->a->value, result, scale);
                return true;
            }
            /* 否则回退到 double */
            double val;
            if (coord_to_double(coord, &val)) {
                double_to_mpz_scaled(val, result, scale);
                return false;
            }
            return false;
        }
        case ALGEBRAIC: {
            /* 尝试有理化 */
            Algebraic *a = coord->data.algebraic;
            if (a && a->cached_rational) {
                rational_to_mpz_scaled(a->cached_rational->value, result, scale);
                return true;
            }
            /* 尝试运行有理化 */
            if (a && algebraic_try_rationalize(a) && a->cached_rational) {
                rational_to_mpz_scaled(a->cached_rational->value, result, scale);
                return true;
            }
            /* 回退到 double */
            double val;
            if (coord_to_double(coord, &val)) {
                double_to_mpz_scaled(val, result, scale);
                return false;
            }
            return false;
        }
        case TRANSCENDENTAL: {
            /* 超越数只能用近似 */
            double val;
            if (coord_to_double(coord, &val)) {
                double_to_mpz_scaled(val, result, scale);
                return false;
            }
            return false;
        }
    }
    return false;
}

/*
 * 将普通 double 值转换为按 scale 缩放的 mpz_t 整数。
 * 内部使用 mpq_t 避免中间精度损失：
 * 先从 double 构造 mpq，再乘以 scale 得到缩放后的整数。
 * 当 scaled_num 不能被 den 整除时，使用四舍五入取整。
 */
void double_to_mpz_scaled(double val, mpz_t result, int64_t scale) {
    /* 防御特殊浮点值 */
    if (!isfinite(val)) {
        mpz_set_ui(result, 0);
        return;
    }

    /* 使用 GMP 的 mpq_set_d 获取最佳精度 */
    mpq_t q;
    mpq_init(q);
    mpq_set_d(q, val);

    /* result = q * scale = (num * scale) / den */
    mpz_t scaled_num;
    mpz_init(scaled_num);
    mpz_mul_si(scaled_num, mpq_numref(q), (long) scale);

    mpz_t den;
    mpz_init(den);
    mpz_set(den, mpq_denref(q));

    if (mpz_divisible_p(scaled_num, den)) {
        /* 能整除时直接取精确商 */
        mpz_divexact(result, scaled_num, den);
    } else {
        /* 不能整除时，使用银行家舍入（round to nearest even）避免系统性偏差 */
        mpz_t quotient, remainder;
        mpz_init(quotient);
        mpz_init(remainder);
        mpz_fdiv_qr(quotient, remainder, scaled_num, den);
        mpz_t half_den;
        mpz_init(half_den);
        mpz_fdiv_q_2exp(half_den, den, 1);
        /*
         * 临界盲区修复：当 remainder 与 half_den 相等时，使用银行家舍入
         * (round to nearest even) 而非单纯向上舍入。这消除了当余数恰好落在
         * 中点附近时因浮点舍入噪声导致的错误符号判别。
         *
         * 零值保护：若原始 double 值 val 的绝对值 < lv_ZERO_EPSILON，
         * 则结果为 0，防止微小浮点噪声产生非零整数结果。
         */
        if (fabs(val) < lv_ZERO_EPSILON) {
            mpz_set_ui(result, 0);
        } else {
            int cmp = mpz_cmp(remainder, half_den);
            if (cmp > 0) {
                mpz_add_ui(result, quotient, 1);
            } else if (cmp < 0) {
                mpz_set(result, quotient);
            } else {
                /* 临界点：remainder == half_den，使用银行家舍入 */
                if (mpz_even_p(quotient)) {
                    mpz_set(result, quotient);
                } else {
                    mpz_add_ui(result, quotient, 1);
                }
            }
        }
        mpz_clear(half_den);
        mpz_clear(quotient);
        mpz_clear(remainder);
    }

    mpz_clear(scaled_num);
    mpz_clear(den);
    mpq_clear(q);
}

/* ------------------------------------------------------------------ */
/*  内部：按 ID 查找几何节点                                          */
/* ------------------------------------------------------------------ */

/**
 * @brief 在约束图中按 ID 查找几何节点
 *
 * 封装 graph_get_node 调用，提供统一的内部查找接口。
 *
 * @param graph 约束图指针
 * @param id    目标节点的 ID
 * @return 找到的 GeomNode 指针，未找到返回 NULL
 */
static GeomNode *find_node(const ConstraintGraph *graph, int id) {
    return graph_get_node(graph, id);
}

/**
 * @brief 提取点的数值坐标值
 *
 * 从点的 symbolc_coords 数组的指定索引获取 double 数值。
 *
 * @param pt  目标点节点（必须为 GEOM_POINT 类型且坐标数量足够）
 * @param idx 坐标索引（0 = x, 1 = y）
 * @param out 输出：坐标的 double 近似值
 * @return true 表示成功获取，false 表示节点类型不符或坐标不足
 */
bool point_coord(const GeomNode *pt, int idx, double *out) {
    if (!pt || pt->type != GEOM_POINT || pt->coord_count <= idx)
        return false;
    return coord_to_double(pt->symbolic_coords[idx], out);
}

/* =======================================================================
 * 内部函数：从两点构建直线方程 ax + by + c = 0
 * ======================================================================= */

typedef struct {
    double a, b, c; /* ax + by + c = 0 */
} LineEquation;

bool line_from_two_points(GeomNode *p1, GeomNode *p2, LineEquation *out) {
    double x1, y1, x2, y2;
    if (!point_coord(p1, 0, &x1) || !point_coord(p1, 1, &y1))
        return false;
    if (!point_coord(p2, 0, &x2) || !point_coord(p2, 1, &y2))
        return false;
    /* 方向向量 (dx, dy) */
    double dx = x2 - x1;
    double dy = y2 - y1;

    /* 检测退化情况：两点重合
     * 使用 epsilon 比较而非精确相等（==），原因：
     * 浮点运算存在舍入误差，即使两点在数学上重合，
     * 经过坐标变换或中间计算后，dx 和 dy 可能不为精确的 0.0。
     * 使用 fabs(dx) < 1e-15 可以正确识别数值上近似为零的情况，
     * 避免将几乎重合的误判为有效直线（导致法向量接近零、方程退化）。 */
    if (fabs(dx) < 1e-15 && fabs(dy) < 1e-15) {
        LOG_WARN("solver", "line_from_two_points: 两点重合，无法确定直线");
        return false;
    }

    /* 法向量: (dy, -dx) => dy*(x-x1) - dx*(y-y1) = 0 */
    out->a = dy;
    out->b = -dx;
    out->c = -(out->a * x1 + out->b * y1);
    return true;
}

/* ------------------------------------------------------------------ */
/*  内部：从约束中提取代数方程                                         */
/* ------------------------------------------------------------------ */

/*
 * For INCIDENCE(point, line_segment):
 *   The line segment has two endpoint points.  The incidence constraint
 *   means the point lies on the line, i.e. cross product of direction
 *   vector and (point - endpoint) is zero.  This gives one linear equation.
 *
 * For INTERSECTION(line1, line2, result_point):
 *   The result point lies on both lines => two linear equations.
 *
 * For BETWEENNESS(p1, p2, p3):
 *   p2 lies on segment p1-p3.  This gives a collinearity equation
 *   plus a ratio constraint 0 <= t <= 1 where p2 = p1 + t*(p3-p1).
 *
 * For distance constraints (not directly a ConstraintType, but encoded
 * via CONNECTION or special numeric_assumption_declaration):
 *   (x2-x1)^2 + (y2-y1)^2 = d^2, which is quadratic.
 *
 * 精度限制说明：
 *   本函数在将几何约束转换为多项式方程时，使用 double 近似值来表示
 *   坐标和参数（如线段端点、距离值等），然后通过 double_to_mpz_scaled()
 *   将 double 转换为缩放后的 mpz_t 整数系数。这意味着：
 *   1. 对于 RATIONAL 类型的坐标，coord_to_double() 可以通过 mpq_get_d()
 *      获得精确的 double 表示（前提是值在 double 精度范围内）。
 *   2. 对于 QUADRATIC/ALGEBRAIC 类型的坐标，coord_to_double() 需要
 *      先序列化为字符串再解析，存在额外的精度损失。
 *   3. 缩放因子 scale=1000000 提供了约 6 位十进制精度，对于大多数
 *      几何计算足够，但不适用于需要高精度的场景。
 *   4. 如果需要完全精确的方程提取，应重构为直接使用 mpq_t/mpz_t
 *      而非经过 double 中间表示。
 */

/**
 * @brief 从约束图中提取代数方程
 *
 * @details 遍历约束图中的所有约束，根据约束类型生成对应的多项式方程：
 *          INCIDENCE（关联）、INTERSECTION（交点）、BETWEENNESS（介于）、
 *          CONTAINMENT（包含）、CONNECTION（连接）。
 *          也处理线段节点上的 numeric_assumption_declaration 距离约束。
 *          构造临时点用于 line_from_two_points，内存管理采用栈分配，
 *          不使用动态分配以避免泄漏风险。
 *
 * @param graph 约束图指针
 * @param sys   输出：存储提取方程的系统
 */
void extract_equations_from_constraints(const ConstraintGraph *graph, EquationSystem *sys) {
    for (int ci = 0; ci < graph->constraint_count; ci++) {
        Constraint *c = graph->constraints[ci];
        if (!c || c->participant_count < 2)
            continue;

        switch (c->type) {
            case INCIDENCE: {
                /* participants[0] = point, participants[1] = line/region */
                GeomNode *pt = find_node(graph, c->participants[0]);
                GeomNode *line = find_node(graph, c->participants[1]);
                if (!pt || !line)
                    break;
                if (line->type == GEOM_LINE_SEGMENT && line->coord_count >= 2) {
                    /* 线段的 symbolic_coords 存储端点坐标为 (x1,y1,x2,y2) 格式 */
                    if (line->coord_count >= 4) {
                        int64_t scale = lv_SOLVER_SCALE_FACTOR;
                        /* 使用精确有理数路径获取端点坐标的缩放值 */
                        mpz_t lx1_s, ly1_s, lx2_s, ly2_s;
                        mpz_init(lx1_s);
                        mpz_init(ly1_s);
                        mpz_init(lx2_s);
                        mpz_init(ly2_s);
                        bool exact = coord_to_mpz_scaled_exact(line->symbolic_coords[0], lx1_s, scale) &&
                                     coord_to_mpz_scaled_exact(line->symbolic_coords[1], ly1_s, scale) &&
                                     coord_to_mpz_scaled_exact(line->symbolic_coords[2], lx2_s, scale) &&
                                     coord_to_mpz_scaled_exact(line->symbolic_coords[3], ly2_s, scale);

                        if (exact) {
                            /* 精确路径：所有坐标都是有理数，在 mpz 层面计算 */
                            mpz_t dx_s, dy_s;
                            mpz_init(dx_s);
                            mpz_init(dy_s);
                            mpz_sub(dx_s, lx2_s, lx1_s); /* dx * scale */
                            mpz_sub(dy_s, ly2_s, ly1_s); /* dy * scale */

                            /* 叉积：dx*(py - ly1) - dy*(px - lx1) = 0
                           => -dy*px + (dy*lx1 - dx*ly1) = 0
                           系数1 = -dy (已缩放)
                           系数0 = dy*lx1 - dx*ly1 (需要除以 scale，因为 dy_s 和 lx1_s 都已缩放) */
                            mpz_poly_t poly;
                            mpz_poly_init(&poly);
                            poly.degree = 1;
                            /* GMP 要求使用标准分配器 */
                            poly.coeffs = coeff_pool_alloc(2);
                            if (!poly.coeffs) {
                                coeff_pool_clear(&poly);
                                mpz_clear(dx_s);
                                mpz_clear(dy_s);
                                mpz_clear(lx1_s);
                                mpz_clear(ly1_s);
                                mpz_clear(lx2_s);
                                mpz_clear(ly2_s);
                                continue;
                            }
                            mpz_init(poly.coeffs[1]);
                            mpz_init(poly.coeffs[0]);
                            mpz_neg(poly.coeffs[1], dy_s);
                            /* dy*lx1 - dx*ly1 = (dy_s * lx1_s - dx_s * ly1_s) / scale */
                            {
                                mpz_t term1, term2;
                                mpz_init(term1);
                                mpz_init(term2);
                                mpz_mul(term1, dy_s, lx1_s);
                                mpz_mul(term2, dx_s, ly1_s);
                                mpz_sub(term1, term1, term2);
                                mpz_fdiv_q_ui(poly.coeffs[0], term1, (unsigned long) scale);
                                mpz_clear(term1);
                                mpz_clear(term2);
                            }
                            EQUATION_PUSH_OR_GOTO(sys, poly, pt->id, 0, push_error);
                            coeff_pool_clear(&poly);

                            /* y 坐标的第二个方程：
                           dx*py + (-dx*ly1 - dy*lx1) = 0 */
                            mpz_poly_init(&poly);
                            poly.degree = 1;
                            /* GMP 要求使用标准分配器 */
                            poly.coeffs = coeff_pool_alloc(2);
                            if (!poly.coeffs) {
                                coeff_pool_clear(&poly);
                                mpz_clear(dx_s);
                                mpz_clear(dy_s);
                                mpz_clear(lx1_s);
                                mpz_clear(ly1_s);
                                mpz_clear(lx2_s);
                                mpz_clear(ly2_s);
                                continue;
                            }
                            mpz_init(poly.coeffs[1]);
                            mpz_init(poly.coeffs[0]);
                            mpz_set(poly.coeffs[1], dx_s);
                            /* -dx*ly1 - dy*lx1 = -(dx_s * ly1_s + dy_s * lx1_s) / scale */
                            {
                                mpz_t term1, term2;
                                mpz_init(term1);
                                mpz_init(term2);
                                mpz_mul(term1, dx_s, ly1_s);
                                mpz_mul(term2, dy_s, lx1_s);
                                mpz_add(term1, term1, term2);
                                mpz_neg(term1, term1);
                                mpz_fdiv_q_ui(poly.coeffs[0], term1, (unsigned long) scale);
                                mpz_clear(term1);
                                mpz_clear(term2);
                            }
                            EQUATION_PUSH_OR_GOTO(sys, poly, pt->id, 1, push_error);
                            coeff_pool_clear(&poly);
                            mpz_clear(dx_s);
                            mpz_clear(dy_s);
                        } else {
                            /* 回退到 double 近似路径 */
                            double lx1, ly1, lx2, ly2;
                            if (coord_to_double(line->symbolic_coords[0], &lx1) &&
                                coord_to_double(line->symbolic_coords[1], &ly1) &&
                                coord_to_double(line->symbolic_coords[2], &lx2) &&
                                coord_to_double(line->symbolic_coords[3], &ly2)) {
                                double dx = lx2 - lx1;
                                double dy = ly2 - ly1;
                                mpz_poly_t poly;
                                mpz_poly_init(&poly);
                                poly.degree = 1;
                                /* GMP 要求使用标准分配器 */
                                poly.coeffs = coeff_pool_alloc(2);
                                if (!poly.coeffs) {
                                    coeff_pool_clear(&poly);
                                    break;
                                }
                                mpz_init(poly.coeffs[1]);
                                mpz_init(poly.coeffs[0]);
                                double_to_mpz_scaled(-dy, poly.coeffs[1], scale);
                                double_to_mpz_scaled(dy * lx1 - dx * ly1, poly.coeffs[0], scale);
                                EQUATION_PUSH_OR_GOTO(sys, poly, pt->id, 0, push_error);
                                coeff_pool_clear(&poly);

                                mpz_poly_init(&poly);
                                poly.degree = 1;
                                /* GMP 要求使用标准分配器 */
                                poly.coeffs = coeff_pool_alloc(2);
                                if (!poly.coeffs) {
                                    coeff_pool_clear(&poly);
                                    break;
                                }
                                mpz_init(poly.coeffs[1]);
                                mpz_init(poly.coeffs[0]);
                                double_to_mpz_scaled(dx, poly.coeffs[1], scale);
                                double_to_mpz_scaled(-dx * ly1 - dy * lx1, poly.coeffs[0], scale);
                                EQUATION_PUSH_OR_GOTO(sys, poly, pt->id, 1, push_error);
                                coeff_pool_clear(&poly);
                            }
                        }
                        mpz_clear(lx1_s);
                        mpz_clear(ly1_s);
                        mpz_clear(lx2_s);
                        mpz_clear(ly2_s);
                    }
                }
                break;
            }

            case INTERSECTION: {
                /* participants[0] = line1, participants[1] = line2,
               participants[2] = result_point（交点） */
                if (c->participant_count < 3)
                    break;
                GeomNode *line1 = find_node(graph, c->participants[0]);
                GeomNode *line2 = find_node(graph, c->participants[1]);
                GeomNode *rpt = find_node(graph, c->participants[2]);
                if (!line1 || !line2 || !rpt)
                    break;

                int64_t scale = lv_SOLVER_SCALE_FACTOR;

                /* 尝试精确有理数路径：直接从线段端点坐标计算直线方程 */
                if (line1->type == GEOM_LINE_SEGMENT && line1->coord_count >= 4 && line2->type == GEOM_LINE_SEGMENT &&
                    line2->coord_count >= 4) {
                    mpz_t l1x1_s, l1y1_s, l1x2_s, l1y2_s;
                    mpz_t l2x1_s, l2y1_s, l2x2_s, l2y2_s;
                    mpz_init(l1x1_s);
                    mpz_init(l1y1_s);
                    mpz_init(l1x2_s);
                    mpz_init(l1y2_s);
                    mpz_init(l2x1_s);
                    mpz_init(l2y1_s);
                    mpz_init(l2x2_s);
                    mpz_init(l2y2_s);

                    bool exact1 = coord_to_mpz_scaled_exact(line1->symbolic_coords[0], l1x1_s, scale) &&
                                  coord_to_mpz_scaled_exact(line1->symbolic_coords[1], l1y1_s, scale) &&
                                  coord_to_mpz_scaled_exact(line1->symbolic_coords[2], l1x2_s, scale) &&
                                  coord_to_mpz_scaled_exact(line1->symbolic_coords[3], l1y2_s, scale);
                    bool exact2 = coord_to_mpz_scaled_exact(line2->symbolic_coords[0], l2x1_s, scale) &&
                                  coord_to_mpz_scaled_exact(line2->symbolic_coords[1], l2y1_s, scale) &&
                                  coord_to_mpz_scaled_exact(line2->symbolic_coords[2], l2x2_s, scale) &&
                                  coord_to_mpz_scaled_exact(line2->symbolic_coords[3], l2y2_s, scale);

                    if (exact1 && exact2) {
                        /* 精确路径：在 mpz 层面计算直线方程和交点 */

                        /* 直线1: a1*x + b1*y + c1 = 0
                       a1 = dy1 = (l1y2 - l1y1), b1 = -dx1 = -(l1x2 - l1x1)
                       c1 = -(a1*l1x1 + b1*l1y1) (需要除以 scale) */
                        mpz_t a1_s, b1_s, c1_s, a2_s, b2_s, c2_s;
                        mpz_t dx1_s, dy1_s, dx2_s, dy2_s;
                        mpz_init(a1_s);
                        mpz_init(b1_s);
                        mpz_init(c1_s);
                        mpz_init(a2_s);
                        mpz_init(b2_s);
                        mpz_init(c2_s);
                        mpz_init(dx1_s);
                        mpz_init(dy1_s);
                        mpz_init(dx2_s);
                        mpz_init(dy2_s);

                        mpz_sub(dx1_s, l1x2_s, l1x1_s);
                        mpz_sub(dy1_s, l1y2_s, l1y1_s);
                        mpz_set(a1_s, dy1_s);
                        mpz_neg(b1_s, dx1_s);
                        /* c1 = -(a1*l1x1 + b1*l1y1) / scale */
                        {
                            mpz_t t1, t2;
                            mpz_init(t1);
                            mpz_init(t2);
                            mpz_mul(t1, a1_s, l1x1_s);
                            mpz_mul(t2, b1_s, l1y1_s);
                            mpz_add(t1, t1, t2);
                            mpz_neg(t1, t1);
                            mpz_fdiv_q_ui(c1_s, t1, (unsigned long) scale);
                            mpz_clear(t1);
                            mpz_clear(t2);
                        }

                        mpz_sub(dx2_s, l2x2_s, l2x1_s);
                        mpz_sub(dy2_s, l2y2_s, l2y1_s);
                        mpz_set(a2_s, dy2_s);
                        mpz_neg(b2_s, dx2_s);
                        /* c2 = -(a2*l2x1 + b2*l2y1) / scale */
                        {
                            mpz_t t1, t2;
                            mpz_init(t1);
                            mpz_init(t2);
                            mpz_mul(t1, a2_s, l2x1_s);
                            mpz_mul(t2, b2_s, l2y1_s);
                            mpz_add(t1, t1, t2);
                            mpz_neg(t1, t1);
                            mpz_fdiv_q_ui(c2_s, t1, (unsigned long) scale);
                            mpz_clear(t1);
                            mpz_clear(t2);
                        }

                        /* D = a1*b2 - a2*b1 (已缩放: a1,b1 已缩放，a2,b2 已缩放 => D 缩放^2) */
                        mpz_t D_s, x_num_s, y_num_s;
                        mpz_init(D_s);
                        mpz_init(x_num_s);
                        mpz_init(y_num_s);
                        {
                            mpz_t t1, t2;
                            mpz_init(t1);
                            mpz_init(t2);
                            mpz_mul(t1, a1_s, b2_s);
                            mpz_mul(t2, a2_s, b1_s);
                            mpz_sub(D_s, t1, t2);
                            mpz_clear(t1);
                            mpz_clear(t2);
                        }

                        /* 检查是否平行：D_s == 0 */
                        if (mpz_sgn(D_s) != 0) {
                            /* x_num = b1*c2 - b2*c1 (b1,b2 缩放, c1,c2 缩放 => x_num 缩放^2) */
                            {
                                mpz_t t1, t2;
                                mpz_init(t1);
                                mpz_init(t2);
                                mpz_mul(t1, b1_s, c2_s);
                                mpz_mul(t2, b2_s, c1_s);
                                mpz_sub(x_num_s, t1, t2);
                                mpz_clear(t1);
                                mpz_clear(t2);
                            }
                            /* y_num = a2*c1 - a1*c2 */
                            {
                                mpz_t t1, t2;
                                mpz_init(t1);
                                mpz_init(t2);
                                mpz_mul(t1, a2_s, c1_s);
                                mpz_mul(t2, a1_s, c2_s);
                                mpz_sub(y_num_s, t1, t2);
                                mpz_clear(t1);
                                mpz_clear(t2);
                            }

                            /*
                         * 方程: D*x = x_num, D*y = y_num
                         * D_s 和 x_num_s/y_num_s 都缩放了 scale^2，
                         * 两边同时除以 scale^2 后等价。
                         * 直接用 D_s 和 x_num_s 作为系数（公共因子 scale^2 约掉）。
                         */
                            mpz_poly_t poly;
                            mpz_poly_init(&poly);
                            poly.degree = 1;
                            /* GMP 要求使用标准分配器 */
                            poly.coeffs = coeff_pool_alloc(2);
                            if (poly.coeffs) {
                                mpz_init(poly.coeffs[1]);
                                mpz_init(poly.coeffs[0]);
                                mpz_set(poly.coeffs[1], D_s);
                                mpz_neg(poly.coeffs[0], x_num_s);
                                EQUATION_PUSH_OR_GOTO(sys, poly, rpt->id, 0, push_error);
                                coeff_pool_clear(&poly);
                            } else {
                                coeff_pool_clear(&poly);
                            }

                            mpz_poly_init(&poly);
                            poly.degree = 1;
                            /* GMP 要求使用标准分配器 */
                            poly.coeffs = coeff_pool_alloc(2);
                            if (poly.coeffs) {
                                mpz_init(poly.coeffs[1]);
                                mpz_init(poly.coeffs[0]);
                                mpz_set(poly.coeffs[1], D_s);
                                mpz_neg(poly.coeffs[0], y_num_s);
                                EQUATION_PUSH_OR_GOTO(sys, poly, rpt->id, 1, push_error);
                                coeff_pool_clear(&poly);
                            } else {
                                coeff_pool_clear(&poly);
                            }
                        }

                        mpz_clear(D_s);
                        mpz_clear(x_num_s);
                        mpz_clear(y_num_s);
                        mpz_clear(a1_s);
                        mpz_clear(b1_s);
                        mpz_clear(c1_s);
                        mpz_clear(a2_s);
                        mpz_clear(b2_s);
                        mpz_clear(c2_s);
                        mpz_clear(dx1_s);
                        mpz_clear(dy1_s);
                        mpz_clear(dx2_s);
                        mpz_clear(dy2_s);
                        mpz_clear(l1x1_s);
                        mpz_clear(l1y1_s);
                        mpz_clear(l1x2_s);
                        mpz_clear(l1y2_s);
                        mpz_clear(l2x1_s);
                        mpz_clear(l2y1_s);
                        mpz_clear(l2x2_s);
                        mpz_clear(l2y2_s);
                        break;
                    }

                    mpz_clear(l1x1_s);
                    mpz_clear(l1y1_s);
                    mpz_clear(l1x2_s);
                    mpz_clear(l1y2_s);
                    mpz_clear(l2x1_s);
                    mpz_clear(l2y1_s);
                    mpz_clear(l2x2_s);
                    mpz_clear(l2y2_s);
                }

                /* 回退到 double 近似路径 */
                {
                    /* 从两条线段中提取直线方程 */
                    LineEquation le1, le2;
                    bool got1 = false, got2 = false;

                    if (line1->type == GEOM_LINE_SEGMENT && line1->coord_count >= 4) {
                        GeomNode ep1_storage, ep2_storage;
                        memset(&ep1_storage, 0, sizeof(GeomNode));
                        memset(&ep2_storage, 0, sizeof(GeomNode));
                        GeomNode *ep1 = &ep1_storage;
                        GeomNode *ep2 = &ep2_storage;
                        ep1->type = GEOM_POINT;
                        ep1->coord_count = 2;
                        ep1->symbolic_coords = &line1->symbolic_coords[0];
                        ep2->type = GEOM_POINT;
                        ep2->coord_count = 2;
                        ep2->symbolic_coords = &line1->symbolic_coords[2];
                        got1 = line_from_two_points(ep1, ep2, &le1);
                    }
                    if (line2->type == GEOM_LINE_SEGMENT && line2->coord_count >= 4) {
                        GeomNode ep1_storage, ep2_storage;
                        memset(&ep1_storage, 0, sizeof(GeomNode));
                        memset(&ep2_storage, 0, sizeof(GeomNode));
                        GeomNode *ep1 = &ep1_storage;
                        GeomNode *ep2 = &ep2_storage;
                        ep1->type = GEOM_POINT;
                        ep1->coord_count = 2;
                        ep1->symbolic_coords = &line2->symbolic_coords[0];
                        ep2->type = GEOM_POINT;
                        ep2->coord_count = 2;
                        ep2->symbolic_coords = &line2->symbolic_coords[2];
                        got2 = line_from_two_points(ep1, ep2, &le2);
                    }

                    if (got1 && got2) {
                        /*
                     * 正确计算两条直线的交点
                     *
                     * 两条直线方程为:
                     *   Line1: a1*x + b1*y + c1 = 0
                     *   Line2: a2*x + b2*y + c2 = 0
                     *
                     * 联立求解交点 (x, y)：
                     *   行列式 D = a1*b2 - a2*b1
                     *   x = (b1*c2 - b2*c1) / D
                     *   y = (a2*c1 - a1*c2) / D
                     *
                     * 为避免除法，我们将方程改写为:
                     *   D*x = b1*c2 - b2*c1
                     *   D*y = a2*c1 - a1*c2
                     *
                     * 这可以表示为两个线性方程:
                     *   D*x + 0*y - (b1*c2 - b2*c1) = 0  =>  x 坐标的约束
                     *   0*x + D*y - (a2*c1 - a1*c2) = 0  =>  y 坐标的约束
                     */

                        double D = le1.a * le2.b - le2.a * le1.b;

                        /* 检查是否平行（行列式接近零） */
                        if (fabs(D) < lv_EPSILON_NUMERIC_COMPARE) {
                            /* 直线平行或重合，无法确定唯一交点 */
                            break;
                        }

                        double x_numerator = le1.b * le2.c - le2.b * le1.c; /* b1*c2 - b2*c1 */
                        double y_numerator = le2.a * le1.c - le1.a * le2.c; /* a2*c1 - a1*c2 */

                        /* x 坐标方程: D*x - x_numerator = 0 */
                        mpz_poly_t poly;
                        mpz_poly_init(&poly);
                        poly.degree = 1;
                        /* GMP 兼容性要求：mpz_poly_clear 内部调用 free()，
                         * 因此此处必须使用标准 malloc 而非 lv_malloc。
                         * lv_SOLVER_LINEAR_COEFF_COUNT 为常量，不存在溢出风险。 */
                        poly.coeffs = coeff_pool_alloc(lv_SOLVER_LINEAR_COEFF_COUNT);
                        if (!poly.coeffs) {
                            coeff_pool_clear(&poly);
                            break;
                        }
                        mpz_init(poly.coeffs[1]);
                        mpz_init(poly.coeffs[0]);
                        double_to_mpz_scaled(D, poly.coeffs[1], scale);            /* x 的系数 */
                        double_to_mpz_scaled(-x_numerator, poly.coeffs[0], scale); /* 常数项 */
                        EQUATION_PUSH_OR_GOTO(sys, poly, rpt->id, 0, push_error);
                        coeff_pool_clear(&poly);

                        /* y 坐标方程: D*y - y_numerator = 0 */
                        mpz_poly_init(&poly);
                        poly.degree = 1;
                        /* GMP 要求使用标准分配器 */
                        poly.coeffs = coeff_pool_alloc(2);
                        if (!poly.coeffs) {
                            coeff_pool_clear(&poly);
                            break;
                        }
                        mpz_init(poly.coeffs[1]);
                        mpz_init(poly.coeffs[0]);
                        double_to_mpz_scaled(D, poly.coeffs[1], scale);            /* y 的系数 */
                        double_to_mpz_scaled(-y_numerator, poly.coeffs[0], scale); /* 常数项 */
                        EQUATION_PUSH_OR_GOTO(sys, poly, rpt->id, 1, push_error);
                        coeff_pool_clear(&poly);
                    }
                }
                break;
            }

            case BETWEENNESS: {
                /* participants[0]=p1, participants[1]=p2, participants[2]=p3
               p2 is between p1 and p3 => collinear + 0 <= t <= 1 */
                if (c->participant_count < 3)
                    break;
                GeomNode *p1 = find_node(graph, c->participants[0]);
                GeomNode *p2 = find_node(graph, c->participants[1]);
                GeomNode *p3 = find_node(graph, c->participants[2]);
                if (!p1 || !p2 || !p3)
                    break;
                if (p1->type != GEOM_POINT || p3->type != GEOM_POINT)
                    break;

                /* 共线性判断：向量 (p2-p1) 和 (p3-p1) 的叉积 = 0
               (x2-x1)*(y3-y1) - (y2-y1)*(x3-x1) = 0
               展开: dy13*x2 - dx13*y2 + (dx13*y1 - dy13*x1) = 0
               使用精确有理数路径避免 double 精度损失 */
                int64_t scale = lv_SOLVER_SCALE_FACTOR;
                mpz_t x1_s, y1_s, x3_s, y3_s;
                mpz_init(x1_s);
                mpz_init(y1_s);
                mpz_init(x3_s);
                mpz_init(y3_s);

                if (p1->symbolic_coords && p3->symbolic_coords && p1->coord_count >= 2 && p3->coord_count >= 2 &&
                    coord_to_mpz_scaled_exact(p1->symbolic_coords[0], x1_s, scale) &&
                    coord_to_mpz_scaled_exact(p1->symbolic_coords[1], y1_s, scale) &&
                    coord_to_mpz_scaled_exact(p3->symbolic_coords[0], x3_s, scale) &&
                    coord_to_mpz_scaled_exact(p3->symbolic_coords[1], y3_s, scale)) {
                    /* dx = x3 - x1, dy = y3 - y1 (均缩放 scale 倍) */
                    mpz_t dx_s, dy_s;
                    mpz_init(dx_s);
                    mpz_init(dy_s);
                    mpz_sub(dx_s, x3_s, x1_s);
                    mpz_sub(dy_s, y3_s, y1_s);

                    /* x2 方程: dy*x2 + (y1*dx - x1*dy) = 0
                   coeff = dy_s, const = (y1_s * dx_s - x1_s * dy_s) / scale */
                    {
                        mpz_poly_t poly;
                        mpz_poly_init(&poly);
                        poly.degree = 1;
                        poly.coeffs = coeff_pool_alloc(2);
                        if (poly.coeffs) {
                            mpz_init(poly.coeffs[1]);
                            mpz_init(poly.coeffs[0]);
                            mpz_set(poly.coeffs[1], dy_s);
                            mpz_t term1, term2;
                            mpz_init(term1);
                            mpz_init(term2);
                            mpz_mul(term1, y1_s, dx_s);
                            mpz_mul(term2, x1_s, dy_s);
                            mpz_sub(term1, term1, term2);
                            mpz_fdiv_q_ui(poly.coeffs[0], term1, (unsigned long) scale);
                            mpz_clear(term1);
                            mpz_clear(term2);
                            EQUATION_PUSH_OR_GOTO(sys, poly, p2->id, 0, push_error);
                        }
                        coeff_pool_clear(&poly);
                    }

                    /* y2 方程: -dx*y2 + (dy*x1 - dx*y1) = 0
                   coeff = -dx_s, const = (dy_s * x1_s - dx_s * y1_s) / scale */
                    {
                        mpz_poly_t poly;
                        mpz_poly_init(&poly);
                        poly.degree = 1;
                        poly.coeffs = coeff_pool_alloc(2);
                        if (poly.coeffs) {
                            mpz_init(poly.coeffs[1]);
                            mpz_init(poly.coeffs[0]);
                            mpz_neg(poly.coeffs[1], dx_s);
                            mpz_t term1, term2;
                            mpz_init(term1);
                            mpz_init(term2);
                            mpz_mul(term1, dy_s, x1_s);
                            mpz_mul(term2, dx_s, y1_s);
                            mpz_sub(term1, term1, term2);
                            mpz_fdiv_q_ui(poly.coeffs[0], term1, (unsigned long) scale);
                            mpz_clear(term1);
                            mpz_clear(term2);
                            EQUATION_PUSH_OR_GOTO(sys, poly, p2->id, 1, push_error);
                        }
                        coeff_pool_clear(&poly);
                    }

                    mpz_clear(dx_s);
                    mpz_clear(dy_s);
                }
                mpz_clear(x1_s);
                mpz_clear(y1_s);
                mpz_clear(x3_s);
                mpz_clear(y3_s);
                break;
            }

            case CONTAINMENT: {
                /* 点/区域包含约束: inner 几何体位于 outer 区域内。
               对多边形区域的每条边界边，点必须在边的"内侧"。
               使用精确有理数路径避免 double 精度损失。 */
                if (c->participant_count < 2)
                    break;
                GeomNode *inner = find_node(graph, c->participants[0]);
                GeomNode *outer = find_node(graph, c->participants[1]);
                if (!inner || !outer)
                    break;

                /* 当前仅处理点-区域包含 */
                if (inner->type != GEOM_POINT || outer->type != GEOM_REGION)
                    break;
                if (outer->data.region.segment_count <= 0 || !outer->data.region.boundary_segments)
                    break;

                {
                    int64_t scale = lv_SOLVER_SCALE_FACTOR;
                    int seg_count = outer->data.region.segment_count;

                    for (int si = 0; si < seg_count; si++) {
                        GeomNode *seg = outer->data.region.boundary_segments[si];
                        if (!seg || seg->type != GEOM_LINE_SEGMENT)
                            continue;
                        if (seg->coord_count < 4 || !seg->symbolic_coords)
                            continue;

                        /* 精确有理数路径：获取边界线段端点坐标的缩放整数值 */
                        mpz_t sx1_s, sy1_s, sx2_s, sy2_s;
                        mpz_init(sx1_s);
                        mpz_init(sy1_s);
                        mpz_init(sx2_s);
                        mpz_init(sy2_s);

                        if (coord_to_mpz_scaled_exact(seg->symbolic_coords[0], sx1_s, scale) &&
                            coord_to_mpz_scaled_exact(seg->symbolic_coords[1], sy1_s, scale) &&
                            coord_to_mpz_scaled_exact(seg->symbolic_coords[2], sx2_s, scale) &&
                            coord_to_mpz_scaled_exact(seg->symbolic_coords[3], sy2_s, scale)) {
                            /* 边方向向量 (均缩放 scale 倍) */
                            mpz_t dx_s, dy_s;
                            mpz_init(dx_s);
                            mpz_init(dy_s);
                            mpz_sub(dx_s, sx2_s, sx1_s);
                            mpz_sub(dy_s, sy2_s, sy1_s);

                            /* x-分量方程: dy*px + (-dy*sx1 + dx*sy1) = 0
                           coeff = dy_s, const = (dx_s * sy1_s - dy_s * sx1_s) / scale */
                            {
                                mpz_poly_t poly;
                                mpz_poly_init(&poly);
                                poly.degree = 1;
                                poly.coeffs = coeff_pool_alloc(2);
                                if (poly.coeffs) {
                                    mpz_init(poly.coeffs[1]);
                                    mpz_init(poly.coeffs[0]);
                                    mpz_set(poly.coeffs[1], dy_s);
                                    mpz_t term1, term2;
                                    mpz_init(term1);
                                    mpz_init(term2);
                                    mpz_mul(term1, dx_s, sy1_s);
                                    mpz_mul(term2, dy_s, sx1_s);
                                    mpz_sub(term1, term1, term2);
                                    mpz_fdiv_q_ui(poly.coeffs[0], term1, (unsigned long) scale);
                                    mpz_clear(term1);
                                    mpz_clear(term2);
                                    EQUATION_PUSH_OR_GOTO(sys, poly, inner->id, 0, push_error);
                                }
                                coeff_pool_clear(&poly);
                            }

                            /* y-分量方程: -dx*py + (dy*sx1 - dx*sy1) = 0
                           coeff = -dx_s, const = (dy_s * sx1_s - dx_s * sy1_s) / scale */
                            {
                                mpz_poly_t poly;
                                mpz_poly_init(&poly);
                                poly.degree = 1;
                                poly.coeffs = coeff_pool_alloc(2);
                                if (poly.coeffs) {
                                    mpz_init(poly.coeffs[1]);
                                    mpz_init(poly.coeffs[0]);
                                    mpz_neg(poly.coeffs[1], dx_s);
                                    mpz_t term1, term2;
                                    mpz_init(term1);
                                    mpz_init(term2);
                                    mpz_mul(term1, dy_s, sx1_s);
                                    mpz_mul(term2, dx_s, sy1_s);
                                    mpz_sub(term1, term1, term2);
                                    mpz_fdiv_q_ui(poly.coeffs[0], term1, (unsigned long) scale);
                                    mpz_clear(term1);
                                    mpz_clear(term2);
                                    EQUATION_PUSH_OR_GOTO(sys, poly, inner->id, 1, push_error);
                                }
                                coeff_pool_clear(&poly);
                            }

                            mpz_clear(dx_s);
                            mpz_clear(dy_s);
                        }

                        mpz_clear(sx1_s);
                        mpz_clear(sy1_s);
                        mpz_clear(sx2_s);
                        mpz_clear(sy2_s);
                    }
                }
                break;
            }

            case ANGLE: {
                /* 角度约束: 两条线段之间的夹角。
                 * 当前暂不生成坐标方程，后续版本可引入方向向量叉积。 */
                break;
            }

            case CONNECTION: {
                /* 端口之间的连接。从关联节点的数值假设声明中提取距离约束。
               如果两个节点都有坐标且其中一个编码了距离，
               生成方程：(xA-xB)^2 + (yA-yB)^2 = d^2。 */
                if (c->participant_count < 2)
                    break;
                GeomNode *nodeA = find_node(graph, c->participants[0]);
                GeomNode *nodeB = find_node(graph, c->participants[1]);
                if (!nodeA || !nodeB)
                    break;

                /* 尝试从任一节点的数值假设声明中提取距离值 */
                double dist_val = -1.0;
                GeomNode *dist_node = NULL;
                const char *prefix = "distance=";
                size_t prefix_len = strlen(prefix); /* 缓存前缀长度，避免循环内重复计算 */
                for (int ni = 0; ni < 2; ni++) {
                    GeomNode *n = (ni == 0) ? nodeA : nodeB;
                    if (!n || !n->numeric_assumption_declaration)
                        continue;
                    const char *decl = n->numeric_assumption_declaration;
                    if (strncmp(decl, prefix, prefix_len) == 0) {
                        dist_val = strtod(decl + prefix_len, NULL);
                        dist_node = n;
                        break;
                    }
                }

                if (dist_val < 0)
                    break;

                /* 两个节点均需要至少 2 个坐标 (x, y) */
                if (nodeA->coord_count < 2 || nodeB->coord_count < 2)
                    break;
                if (!nodeA->symbolic_coords || !nodeB->symbolic_coords)
                    break;

                double ax, ay, bx, by;
                if (!coord_to_double(nodeA->symbolic_coords[0], &ax) ||
                    !coord_to_double(nodeA->symbolic_coords[1], &ay) ||
                    !coord_to_double(nodeB->symbolic_coords[0], &bx) ||
                    !coord_to_double(nodeB->symbolic_coords[1], &by))
                    break;

                double dist_sq = dist_val * dist_val;
                int64_t scale = lv_SOLVER_SCALE_FACTOR;

                /* (xA-xB)^2 + (yA-yB)^2 = d^2
               Expand for nodeB as variable (nodeA as fixed):
               xB^2 - 2*ax*xB + (ax^2 + ay^2 - 2*ay*yB + yB^2 - dist_sq) = 0
               Simplified as univariate in xB:
               xB^2 - 2*ax*xB + (ax^2 + ay^2 - dist_sq) = 0
               (ignoring y coupling terms, consistent with existing approach) */
                mpz_poly_t poly;
                mpz_poly_init(&poly);
                poly.degree = 2;
                /* GMP 兼容性要求：mpz_poly_clear 内部调用 free()，
                 * 因此此处必须使用标准 malloc 而非 lv_malloc。
                 * lv_SOLVER_QUADRATIC_COEFF_COUNT 为常量，不存在溢出风险。 */
                poly.coeffs = coeff_pool_alloc(lv_SOLVER_QUADRATIC_COEFF_COUNT);
                if (!poly.coeffs) {
                    coeff_pool_clear(&poly);
                    break;
                }
                mpz_init(poly.coeffs[2]);
                mpz_init(poly.coeffs[1]);
                mpz_init(poly.coeffs[0]);
                mpz_set_si(poly.coeffs[2], scale);
                double_to_mpz_scaled(-2.0 * ax, poly.coeffs[1], scale);
                double_to_mpz_scaled(ax * ax + ay * ay - dist_sq, poly.coeffs[0], scale);
                EQUATION_PUSH_OR_GOTO(sys, poly, nodeB->id, 0, push_error);
                coeff_pool_clear(&poly);

                /* 同理对 yB 建立方程：yB^2 - 2*ay*yB + (ax^2 + ay^2 - dist_sq) = 0 */
                mpz_poly_init(&poly);
                poly.degree = 2;
                /* GMP 要求使用标准分配器 */
                poly.coeffs = coeff_pool_alloc(3);
                if (!poly.coeffs) {
                    coeff_pool_clear(&poly);
                    break;
                }
                mpz_init(poly.coeffs[2]);
                mpz_init(poly.coeffs[1]);
                mpz_init(poly.coeffs[0]);
                mpz_set_si(poly.coeffs[2], scale);
                double_to_mpz_scaled(-2.0 * ay, poly.coeffs[1], scale);
                double_to_mpz_scaled(ax * ax + ay * ay - dist_sq, poly.coeffs[0], scale);
                EQUATION_PUSH_OR_GOTO(sys, poly, nodeB->id, 1, push_error);
                coeff_pool_clear(&poly);
                break;
            }
            default:
                lv_LOG_WARNING("Unknown constraint type %d in extract_equations_from_constraints", c->type);
                break;
        }
    }

    /* 第二遍扫描：从设置了数值假设声明的节点中提取距离约束
       （编码平方距离 = d^2）。 */
    for (int ni = 0; ni < graph->node_count; ni++) {
        GeomNode *node = graph->nodes[ni];
        if (!node || !node->numeric_assumption_declaration)
            continue;
        if (node->type != GEOM_LINE_SEGMENT)
            continue;

        /* 检查声明是否编码了距离约束。
           格式："distance=<value>" 或仅为数值。 */
        const char *decl = node->numeric_assumption_declaration;
        double dist_sq = -1.0;

        /* 尝试解析为 "distance=<value>" 格式 */
        const char *prefix = "distance=";
        size_t prefix_len = strlen(prefix); /* 缓存前缀长度，避免重复计算 */
        if (strncmp(decl, prefix, prefix_len) == 0) {
            dist_sq = strtod(decl + prefix_len, NULL);
            dist_sq = dist_sq * dist_sq; /* 存储平方值 */
        } else {
            /* 尝试解析为纯数字（视为距离的平方） */
            char *end = NULL;
            double val = strtod(decl, &end);
            if (end != decl && val >= 0) {
                dist_sq = val;
            }
        }

        if (dist_sq < 0)
            continue;

        /* 线段在 symbolic_coords 中存储了端点坐标。
           为第二个端点建立距离方程
          （第一个端点通常已固定）。 */
        if (node->coord_count >= 4) {
            double x1, y1;
            if (coord_to_double(node->symbolic_coords[0], &x1) && coord_to_double(node->symbolic_coords[1], &y1)) {
                /* (x - x1)^2 + (y - y1)^2 = dist_sq
                   => x^2 - 2*x1*x + x1^2 + y^2 - 2*y1*y + y1^2 - dist_sq = 0
                   这是一个关于 x 和 y 的二次方程。将其存储为两个独立的一元方程
                  （耦合），求解器将按方程组处理。 */

                /* 对于第二个端点的 x 坐标：需要第二个端点的节点 ID。
                   由于线段直接存储坐标，我们创建以线段 ID 标记的方程。 */
                int64_t scale = lv_SOLVER_SCALE_FACTOR;

                /* x^2 - 2*x1*x + (x1^2 + y1^2 - dist_sq - y^2 + 2*y1*y) = 0
                   作为 x 的单变量方程: x^2 - 2*x1*x + const = 0
                   其中 const = x1^2 + y1^2 - dist_sq（忽略含 y 的耦合项）。
                   注意：距离方程展开后常数项为 x1^2 + y1^2 - dist_sq，
                   原代码错误地使用了 x1^2 + dist_sq（符号错误）。 */
                mpz_poly_t poly;
                mpz_poly_init(&poly);
                poly.degree = 2;
                /* GMP 兼容性要求：mpz_poly_clear 内部调用 free()，
                 * 因此此处必须使用标准 malloc 而非 lv_malloc。
                 * lv_SOLVER_QUADRATIC_COEFF_COUNT 为常量，不存在溢出风险。 */
                poly.coeffs = coeff_pool_alloc(lv_SOLVER_QUADRATIC_COEFF_COUNT);
                if (!poly.coeffs) {
                    coeff_pool_clear(&poly);
                    continue;
                }
                mpz_init(poly.coeffs[2]);
                mpz_init(poly.coeffs[1]);
                mpz_init(poly.coeffs[0]);
                mpz_set_si(poly.coeffs[2], scale);                                        /* x^2 系数 */
                double_to_mpz_scaled(-2.0 * x1, poly.coeffs[1], scale);                   /* x 系数 */
                double_to_mpz_scaled(x1 * x1 + y1 * y1 - dist_sq, poly.coeffs[0], scale); /* 常数项（已修正符号） */
                EQUATION_PUSH_OR_GOTO(sys, poly, node->id, 0, push_error);
                coeff_pool_clear(&poly);

                /* 同理对 y 建立方程：y^2 - 2*y1*y + (x1^2 + y1^2 - dist_sq) = 0 */
                mpz_poly_init(&poly);
                poly.degree = 2;
                /* GMP 要求使用标准分配器 */
                poly.coeffs = coeff_pool_alloc(3);
                if (!poly.coeffs) {
                    coeff_pool_clear(&poly);
                    continue;
                }
                mpz_init(poly.coeffs[2]);
                mpz_init(poly.coeffs[1]);
                mpz_init(poly.coeffs[0]);
                mpz_set_si(poly.coeffs[2], scale);
                double_to_mpz_scaled(-2.0 * y1, poly.coeffs[1], scale);
                double_to_mpz_scaled(x1 * x1 + y1 * y1 - dist_sq, poly.coeffs[0], scale); /* 常数项（已修正符号） */
                EQUATION_PUSH_OR_GOTO(sys, poly, node->id, 1, push_error);
                coeff_pool_clear(&poly);
            }
        }
    }
push_error:
    return;
}

/* ------------------------------------------------------------------ */
/*  内部：统计每个变量的有效方程数量                                   */
/* ------------------------------------------------------------------ */

typedef struct {
    int node_id;
    int eq_count;
    int max_degree;
} VarInfo;

/* 统计方程系统中每个变量节点对应的方程数量和最高次数。
   返回 VarInfo 数组，通过 out_var_count 输出变量数量。
   如果方程系统为空（var_count == 0），直接返回 NULL。 */
static VarInfo *build_var_info(const EquationSystem *sys, int node_count, int *out_var_count) {
    /* 收集所有不重复的变量节点 id */
    int *var_ids = lv_malloc((size_t) sys->eqs.count * sizeof(int));
    if (!var_ids)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "build_var_info: lv_malloc for var_ids failed (count=%d)", sys->eqs.count);
    int var_count = 0;
    for (int i = 0; i < sys->eqs.count; i++) {
        PolyEquation *eq = (PolyEquation *)lv_darray_get(&sys->eqs, i);
        if (!eq) continue;
        int vid = eq->var_node_id;
        bool found = false;
        for (int j = 0; j < var_count; j++) {
            if (var_ids[j] == vid) {
                found = true;
                break;
            }
        }
        if (!found)
            var_ids[var_count++] = vid;
    }

    /* 提前返回：如果没有变量，避免 lv_calloc(0, ...) 的未定义行为 */
    if (var_count == 0) {
        lv_free((void **) &var_ids);
        *out_var_count = 0;
        lv_RETURN_ERROR_NULL(lv_ERROR_NOT_FOUND, "build_var_info: no variables found in equation system (eqs.count=%d)", sys->eqs.count);
    }

    VarInfo *info = lv_calloc((size_t) var_count, sizeof(VarInfo));
    for (int i = 0; i < var_count; i++) {
        info[i].node_id = var_ids[i];
        info[i].eq_count = 0;
        info[i].max_degree = 0;
    }
    for (int i = 0; i < sys->eqs.count; i++) {
        PolyEquation *eq = (PolyEquation *)lv_darray_get(&sys->eqs, i);
        if (!eq) continue;
        int vid = eq->var_node_id;
        for (int j = 0; j < var_count; j++) {
            if (info[j].node_id == vid) {
                info[j].eq_count++;
                int deg = eq->poly.degree;
                if (deg > info[j].max_degree)
                    info[j].max_degree = deg;
                break;
            }
        }
    }
    lv_free((void **) &var_ids);
    *out_var_count = var_count;
    return info;
}

/* ------------------------------------------------------------------ */
/*  内部：求解一元一次方程 a*x + b = 0                                    */
/* ------------------------------------------------------------------ */

/**
 * @brief 求解一元一次方程 a*x + b = 0
 *
 * 从多项式系数中提取 a (coeffs[1]) 和 b (coeffs[0])，计算 x = -b/a。
 *
 * @param poly  一元多项式指针（次数必须为 1）
 * @param x_out 输出：方程的解
 * @return true 表示成功求解，false 表示次数不为 1 或 a 近似为 0
 */
bool solve_linear(const mpz_poly_t *poly, double *x_out) {
    if (!poly || !x_out)
        return false;
    if (poly->degree != 1)
        return false;
    double a = mpz_get_d(poly->coeffs[1]);
    double b = mpz_get_d(poly->coeffs[0]);
    if (fabs(a) < lv_ZERO_EPSILON)
        return false;
    *x_out = -b / a;
    return true;
}

/**
 * @file solver_coord_extract.c
 * @brief 坐标提取与方程提取
 *
 * @details 从 solver.c 拆分出的子模块（Lv-00 项目 v3.3.0+）。多项式系数池与坐标→double/mpz 转换（约束方程提取见 solver_equation_extract.c）。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "solver_common.h"
#include "lv/coeff_pool.h" /* 共享的多项式系数内存池（实现与池拥有权见 lv/coeff_pool.h） */
#include "lv/lv_parse_utils.h"
#include "lv/lv_xmacro.h"
#include "lv/lv_numeric.h"
#include "lv/geo_utils.h"
#include "lv/lv_lifecycle.h" /* lv_DEFER + lv_mpz_clear_deferred */

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
/*  coeff_pool 配对收敛（K4 C2-3）：init 负责 mpz_poly_init + 池分配 +
 *  系数元素初始化，失败时内部已归还池；push 负责 equation_system_push
 * （深拷贝进方程系统）后归还池。配对契约：池分配成功后，无论 push
 *  成败，池内存必须经 coeff_pool_clear 归还。 */
/* ------------------------------------------------------------------ */

/** @brief 从系数池分配 poly->coeffs 并初始化系数元素
 * @return 0 成功；-1 分配失败（内部已完成 coeff_pool_clear 归还） */
int solver_poly_pool_init(mpz_poly_t *poly, int degree, int coeff_count) {
    mpz_poly_init(poly);
    poly->degree = degree;
    poly->coeffs = coeff_pool_alloc(coeff_count);
    if (!poly->coeffs) {
        coeff_pool_clear(poly);
        return -1;
    }
    for (int i = 0; i <= degree; i++)
        mpz_init(poly->coeffs[i]);
    return 0;
}

/** @brief 将 poly 推入方程系统（深拷贝）并归还池；失败时设置 OOM 错误
 * @return equation_system_push 的返回码（0 成功；非 0 失败，池已归还） */
int solver_poly_pool_push(EquationSystem *sys, mpz_poly_t *poly, int var_node_id, int coord_index) {
    int rc = equation_system_push(sys, *poly, var_node_id, coord_index);
    coeff_pool_clear(poly);
    if (rc != 0)
        lv_set_error(lv_ERROR_OUT_OF_MEMORY, "push failed (OOM)");
    return rc;
}

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
    bool ok = (lv_parse_double(str, out) == 0);
    if (!ok)
        *out = 0.0;
    lv_free((void **) &str);
    return ok;
}

/* ── CoordToDouble VTable ── */
typedef bool (*CoordToDoubleFunc)(const SymbolicCoord *c, double *out);

static bool coord_to_double_rational(const SymbolicCoord *c, double *out) {
    if (c->data.rational) {
        *out = mpq_get_d(lv_rational_mpq(c->data.rational));
        return true;
    }
    /* 数据不一致：RATIONAL 类型但 rational 指针为空 */
    lv_LOG_WARNING("coord_to_double: RATIONAL 类型但 data.rational 为 NULL");
    *out = 0.0;
    return false;
}

static bool coord_to_double_quadratic_or_algebraic(const SymbolicCoord *c, double *out) {
    return coord_to_double_via_serialize(c, out);
}

static bool coord_to_double_transcendental(const SymbolicCoord *c, double *out) {
    *out = symbolic_coord_to_double(c);
    return (fabs(*out) > lv_EPSILON_DOUBLE || c->data.transcendental != NULL);
}

static const CoordToDoubleFunc coord_to_double_ops[] = {
    [RATIONAL] = coord_to_double_rational,
    [QUADRATIC] = coord_to_double_quadratic_or_algebraic,
    [ALGEBRAIC] = coord_to_double_quadratic_or_algebraic,
    [TRANSCENDENTAL] = coord_to_double_transcendental,
};

int coord_to_double(const SymbolicCoord *c, double *out) {
    if (!c)
        return false;
    return LV_DISPATCH(coord_to_double_ops, c->type, false, c, out);
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
    lv_DEFER(lv_mpz_clear_deferred, &scaled);
    mpz_set_si(scaled, (long) scale);
    mpz_mul(result, mpq_numref(val), scaled);
    mpz_fdiv_q(result, result, mpq_denref(val));
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
        mpq_srcptr val = lv_rational_mpq(c->data.rational);

        /* 计算分子 * scale（只计算一次，避免重复） */
        mpz_t scaled_num;
        mpz_init(scaled_num);
        lv_DEFER(lv_mpz_clear_deferred, &scaled_num);
        mpz_mul_si(scaled_num, mpq_numref(val), (long) scale);

        mpz_t den;
        mpz_init(den);
        lv_DEFER(lv_mpz_clear_deferred, &den);
        mpz_set(den, mpq_denref(val));

        if (mpz_divisible_p(scaled_num, den)) {
            /* 精确整除：直接取商 */
            mpz_fdiv_q(result, scaled_num, den);
        } else {
            /* 非精确整除：使用银行家舍入（round to nearest even）避免系统性偏差 */
            mpz_t q, r;
            mpz_init(q);
            lv_DEFER(lv_mpz_clear_deferred, &q);
            mpz_init(r);
            lv_DEFER(lv_mpz_clear_deferred, &r);
            mpz_fdiv_qr(q, r, scaled_num, den);
            mpz_t half_den;
            mpz_init(half_den);
            lv_DEFER(lv_mpz_clear_deferred, &half_den);
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
        }

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
/* ── CoordToMpzScaled VTable ── */
typedef bool (*CoordToMpzScaledFunc)(const SymbolicCoord *coord, mpz_t result, int64_t scale);

static bool coord_to_mpz_scaled_exact_rational(const SymbolicCoord *coord, mpz_t result, int64_t scale) {
    if (!coord->data.rational)
        return false;
    rational_to_mpz_scaled(lv_rational_mpq(coord->data.rational), result, scale);
    return true;
}

static bool coord_to_mpz_scaled_exact_quadratic(const SymbolicCoord *coord, mpz_t result, int64_t scale) {
    Quadratic *q = coord->data.quadratic;
    if (q && mpq_sgn(lv_rational_mpq(q->b)) == 0) {
        rational_to_mpz_scaled(lv_rational_mpq(q->a), result, scale);
        return true;
    }
    double val;
    if (coord_to_double(coord, &val)) {
        double_to_mpz_scaled(val, result, scale);
        return false;
    }
    return false;
}

static bool coord_to_mpz_scaled_exact_algebraic(const SymbolicCoord *coord, mpz_t result, int64_t scale) {
    Algebraic *a = coord->data.algebraic;
    if (a && a->cached_rational) {
        rational_to_mpz_scaled(lv_rational_mpq(a->cached_rational), result, scale);
        return true;
    }
    if (a && algebraic_try_rationalize(a) && a->cached_rational) {
        rational_to_mpz_scaled(lv_rational_mpq(a->cached_rational), result, scale);
        return true;
    }
    double val;
    if (coord_to_double(coord, &val)) {
        double_to_mpz_scaled(val, result, scale);
        return false;
    }
    return false;
}

static bool coord_to_mpz_scaled_exact_transcendental(const SymbolicCoord *coord, mpz_t result, int64_t scale) {
    double val;
    if (coord_to_double(coord, &val)) {
        double_to_mpz_scaled(val, result, scale);
        return false;
    }
    return false;
}

static const CoordToMpzScaledFunc coord_to_mpz_scaled_ops[] = {
    [RATIONAL] = coord_to_mpz_scaled_exact_rational,
    [QUADRATIC] = coord_to_mpz_scaled_exact_quadratic,
    [ALGEBRAIC] = coord_to_mpz_scaled_exact_algebraic,
    [TRANSCENDENTAL] = coord_to_mpz_scaled_exact_transcendental,
};

bool coord_to_mpz_scaled_exact(const SymbolicCoord *coord, mpz_t result, int64_t scale) {
    if (!coord)
        return false;
    int type = coord->type;
    return LV_DISPATCH(coord_to_mpz_scaled_ops, type, false, coord, result, scale);
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
    lv_mpq_set_d_checked(q, val);

    /* result = q * scale = (num * scale) / den */
    mpz_t scaled_num;
    mpz_init(scaled_num);
    lv_DEFER(lv_mpz_clear_deferred, &scaled_num);
    mpz_mul_si(scaled_num, mpq_numref(q), (long) scale);

    mpz_t den;
    mpz_init(den);
    lv_DEFER(lv_mpz_clear_deferred, &den);
    mpz_set(den, mpq_denref(q));

    if (mpz_divisible_p(scaled_num, den)) {
        /* 能整除时直接取精确商 */
        mpz_divexact(result, scaled_num, den);
    } else {
        /* 不能整除时，使用银行家舍入（round to nearest even）避免系统性偏差 */
        mpz_t quotient, remainder;
        mpz_init(quotient);
        lv_DEFER(lv_mpz_clear_deferred, &quotient);
        mpz_init(remainder);
        lv_DEFER(lv_mpz_clear_deferred, &remainder);
        mpz_fdiv_qr(quotient, remainder, scaled_num, den);
        mpz_t half_den;
        mpz_init(half_den);
        lv_DEFER(lv_mpz_clear_deferred, &half_den);
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
    }

    mpq_clear(q);
}

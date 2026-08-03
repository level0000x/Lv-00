/**
 * @file symbolic_coord_lifecycle.c
 * @brief SymbolicCoord 生命周期管理：构造、析构、拷贝、序列化、类型转换
 *
 * @details 实现 RATIONAL / QUADRATIC / ALGEBRAIC / TRANSCENDENTAL
 *          四种符号坐标类型的创建、销毁、复制、数值转换和基本查询。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv/lv_platform.h"
#include "lv/lv_mempool_utils.h"

#include <float.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/bit_burning.h"
#include "lv/constraint_graph.h"
#include "lv/symbolic_coord.h"

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "mpz_poly.h"

#include "symbolic_coord_internal.h"

/* ── 前向声明（来自 symbolics 子目录其他模块）── */
double algebraic_to_double(const Algebraic *a);
double quadratic_to_double(const Quadratic *q);
double transcendental_to_double(const Transcendental *t);
char *algebraic_serialize(const Algebraic *a);
char *quadratic_serialize(const Quadratic *q);
char *transcendental_serialize(const Transcendental *t);

/* ── 外部溢出上下文 ── */
extern lv_THREAD_LOCAL struct OverflowContext g_overflow_context;

/* ============================================================
 * VTable Handler Functions (Lifecycle)
 * ============================================================ */

/* ── destroy handlers ── */
void destroy_rational(SymbolicCoord *coord) {
    rational_destroy(coord->data.rational);
    coord->data.rational = NULL;
}
void destroy_algebraic(SymbolicCoord *coord) {
    algebraic_destroy(coord->data.algebraic);
    coord->data.algebraic = NULL;
}
void destroy_quadratic(SymbolicCoord *coord) {
    quadratic_destroy(coord->data.quadratic);
    coord->data.quadratic = NULL;
}
void destroy_transcendental(SymbolicCoord *coord) {
    transcendental_destroy(coord->data.transcendental);
    coord->data.transcendental = NULL;
}

/* ── serialize handlers ── */
char *serialize_rational(const SymbolicCoord *coord) {
    return rational_serialize(coord->data.rational);
}
char *serialize_algebraic(const SymbolicCoord *coord) {
    return algebraic_serialize(coord->data.algebraic);
}
char *serialize_quadratic(const SymbolicCoord *coord) {
    return quadratic_serialize(coord->data.quadratic);
}
char *serialize_transcendental(const SymbolicCoord *coord) {
    return transcendental_serialize(coord->data.transcendental);
}

/* ── copy_data handlers ── */
void copy_data_rational(const SymbolicCoord *src, SymbolicCoord *dst) {
    dst->data.rational = rational_copy(src->data.rational);
}
void copy_data_algebraic(const SymbolicCoord *src, SymbolicCoord *dst) {
    dst->data.algebraic = algebraic_create((mpz_poly_t *) &src->data.algebraic->minimal_poly,
                                           src->data.algebraic->left_bound, src->data.algebraic->right_bound);
    if (dst->data.algebraic && src->data.algebraic->cached_rational) {
        dst->data.algebraic->cached_rational = rational_copy(src->data.algebraic->cached_rational);
    }
}
void copy_data_quadratic(const SymbolicCoord *src, SymbolicCoord *dst) {
    Rational *a = rational_copy(src->data.quadratic->a);
    Rational *b = rational_copy(src->data.quadratic->b);
    dst->data.quadratic = quadratic_create(a, b, src->data.quadratic->n);
}
void copy_data_transcendental(const SymbolicCoord *src, SymbolicCoord *dst) {
    dst->data.transcendental = transcendental_create(src->data.transcendental->name);
    if (dst->data.transcendental && src->data.transcendental->expr) {
        TranscendentalExpr *src_expr = src->data.transcendental->expr;
        TranscendentalExpr *dst_expr = lv_calloc(1, sizeof(TranscendentalExpr));
        if (dst_expr) {
            dst_expr->expr_type = src_expr->expr_type;
            lv_strlcpy(dst_expr->base_name, src_expr->base_name, sizeof(dst_expr->base_name));
            dst_expr->rational_operand =
                src_expr->rational_operand ? rational_copy(src_expr->rational_operand) : NULL;
            dst_expr->out_of_scope = src_expr->out_of_scope;
            dst->data.transcendental->expr = dst_expr;
        }
    }
}

/* ── copy_check handlers ── */
bool copy_check_rational(const SymbolicCoord *coord) {
    return (coord->data.rational != NULL);
}
bool copy_check_algebraic(const SymbolicCoord *coord) {
    return (coord->data.algebraic != NULL);
}
bool copy_check_quadratic(const SymbolicCoord *coord) {
    return (coord->data.quadratic != NULL);
}
bool copy_check_transcendental(const SymbolicCoord *coord) {
    return (coord->data.transcendental != NULL);
}

/* ── is_zero handlers ── */
bool is_zero_rational(const SymbolicCoord *coord) {
    return mpq_cmp_ui(coord->data.rational->value, 0, 1) == 0;
}
bool is_zero_algebraic(const SymbolicCoord *coord) {
    Algebraic *a = coord->data.algebraic;
    if (a->cached_rational) {
        return mpq_cmp_ui(a->cached_rational->value, 0, 1) == 0;
    }
    return (a->left_bound <= 0 && a->right_bound >= 0);
}
bool is_zero_quadratic(const SymbolicCoord *coord) {
    Quadratic *q = coord->data.quadratic;
    extern bool is_rational_zero(const Rational *r);
    return is_rational_zero(q->a) && is_rational_zero(q->b);
}
bool is_zero_transcendental(const SymbolicCoord *coord) {
    (void)coord;
    return false;
}

/* ============================================================
 * SymbolicCoord Constructors
 * ============================================================ */

SymbolicCoord *symbolic_coord_create_rational(int64_t num, uint64_t denom) {
    SymbolicCoord *coord = lv_calloc(1, sizeof(SymbolicCoord));
    if (!coord)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "symbolic_coord_create_rational: lv_calloc failed");
    coord->type = RATIONAL;
    coord->trust = TRUST_GREEN;
    coord->cache_valid = false;
    coord->cached_value = 0.0;
    coord->data.rational = rational_create(num, denom);
    if (!coord->data.rational) {
        lv_free((void **) &coord);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "symbolic_coord_create_rational: rational_create failed");
    }
    return coord;
}

SymbolicCoord *symbolic_coord_from_double_scaled(double val, int64_t scale) {
    double scaled_val = val * (double) scale;
    /* 钳制到 int64 安全范围再转换，避免大值时未定义行为 */
    if (scaled_val > 9223372036854774784.0)
        scaled_val = 9223372036854774784.0;
    if (scaled_val < -9223372036854774784.0)
        scaled_val = -9223372036854774784.0;
    return symbolic_coord_create_rational((int64_t) scaled_val, (uint64_t) scale);
}

SymbolicCoord *symbolic_coord_from_double_rounded(double val, int64_t scale) {
    double scaled_val = round(val * (double) scale);
    /* 钳制到 int64 安全范围再转换，避免大值时未定义行为 */
    if (scaled_val > 9223372036854774784.0)
        scaled_val = 9223372036854774784.0;
    if (scaled_val < -9223372036854774784.0)
        scaled_val = -9223372036854774784.0;
    return symbolic_coord_create_rational((int64_t) scaled_val, (uint64_t) scale);
}

/**
 * 创建代数数类型的符号坐标。
 *
 * @param poly  极小多项式
 * @param left  隔离区间左边界
 * @param right 隔离区间右边界
 * @return 新创建的符号坐标对象，失败时返回 NULL；调用者需负责释放
 */
SymbolicCoord *symbolic_coord_create_algebraic(mpz_poly_t *poly, double left, double right) {
    SymbolicCoord *coord = lv_calloc(1, sizeof(SymbolicCoord));
    if (!coord)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "symbolic_coord_create_algebraic: lv_calloc failed");
    coord->type = ALGEBRAIC;
    coord->trust = TRUST_GREEN;
    coord->cache_valid = false;
    coord->cached_value = 0.0;
    coord->data.algebraic = algebraic_create(poly, left, right);
    if (!coord->data.algebraic) {
        lv_free((void **) &coord);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "symbolic_coord_create_algebraic: algebraic_create failed");
    }
    return coord;
}

/**
 * 创建二次根式类型的符号坐标。
 *
 * @param a 二次项的系数有理数
 * @param b 根号项的系数有理数
 * @param n 根号内的整数
 * @return 新创建的符号坐标对象，失败时返回 NULL；调用者需负责释放
 */
SymbolicCoord *symbolic_coord_create_quadratic(Rational *a, Rational *b, unsigned int n) {
    SymbolicCoord *coord = lv_calloc(1, sizeof(SymbolicCoord));
    if (!coord)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "symbolic_coord_create_quadratic: lv_calloc failed");
    coord->type = QUADRATIC;
    coord->trust = TRUST_GREEN;
    coord->cache_valid = false;
    coord->cached_value = 0.0;
    coord->data.quadratic = quadratic_create(a, b, n);
    if (!coord->data.quadratic) {
        lv_free((void **) &coord);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "symbolic_coord_create_quadratic: quadratic_create failed");
    }
    return coord;
}

SymbolicCoord *symbolic_coord_create_transcendental(const char *name) {
    SymbolicCoord *coord = lv_calloc(1, sizeof(SymbolicCoord));
    if (!coord)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "symbolic_coord_create_transcendental: lv_calloc failed");
    coord->type = TRANSCENDENTAL;
    coord->trust = TRUST_BLUE_UNEXPLORED;
    coord->cache_valid = false;
    coord->cached_value = 0.0;
    coord->data.transcendental = transcendental_create(name);
    if (!coord->data.transcendental) {
        lv_free((void **) &coord);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "symbolic_coord_create_transcendental: transcendental_create failed");
    }
    return coord;
}

/**
 * 销毁符号坐标对象并释放内存。
 *
 * 销毁操作包括：
 * 1. 根据坐标类型调用对应的类型销毁函数，释放底层 GMP 变量和动态内存
 * 2. 递归清理嵌套数据结构（如 algebraic 的 cached_rational、
 *    quadratic 的子有理数、transcendental 的表达式树等）
 * 3. 使数值缓存失效，防止悬空引用
 * 4. 将所有指针置 NULL，防止悬空指针
 *
 * @param coord 符号坐标对象，可为 NULL（空操作）
 */
void symbolic_coord_destroy(SymbolicCoord *coord) {
    if (!coord)
        return;

    /* 使数值缓存失效 */
    coord->cache_valid = false;
    coord->cached_value = 0.0;

    kCoordOpsVTable[coord->type].destroy(coord);

    /* 将 trust 颜色重置为安全默认值 */
    coord->trust = TRUST_GREEN;
    coord->type = RATIONAL;

    lv_free((void **) &coord); /* lv_malloc分配 */
}

/* ============================================================
 * Double Conversion & Cache Management
 * ============================================================ */

/**
 * 获取任意 SymbolicCoord 类型的数值近似。
 *
 * 先检查缓存：若 cache_valid 为 true，则直接返回 cached_value，
 * 避免重复的 GMP 转换开销。缓存失效时重新计算并更新缓存。
 * 当坐标被修改时需调用 symbolic_coord_invalidate_cache() 使缓存失效。
 *
 * @param coord SymbolicCoord 对象（不能为 NULL）
 * @return 转换后的双精度浮点数值
 */
double symbolic_coord_to_double(const SymbolicCoord *coord) {
    if (!coord)
        return 0.0;

    /* 缓存命中：直接返回已缓存值，避免重复计算 */
    if (coord->cache_valid) {
        return coord->cached_value;
    }

    double val = kCoordOpsVTable[coord->type].to_double(coord);

    /* 更新缓存（const 转换为非 const：缓存是性能优化，不改变逻辑语义） */
    ((SymbolicCoord *) coord)->cached_value = val;
    ((SymbolicCoord *) coord)->cache_valid = true;

    return val;
}

/**
 * 使符号坐标的数值缓存失效。
 *
 * 当坐标被修改（如算术运算、信任颜色变更、类型转换）时，
 * 调用此函数标记缓存为无效，确保后续调用
 * symbolic_coord_to_double() 时重新计算精确数值。
 *
 * @param coord 符号坐标（可为 NULL，空操作）
 */
void symbolic_coord_invalidate_cache(SymbolicCoord *coord) {
    if (!coord)
        return;
    coord->cache_valid = false;
    coord->cached_value = 0.0;
}

/* ============================================================
 * Serialization
 * ============================================================ */

/**
 * 将符号坐标序列化为字符串。
 *
 * @param coord 符号坐标对象（不能为 NULL）
 * @return 新分配的字符串，失败时返回 NULL
 */
char *symbolic_coord_serialize(const SymbolicCoord *coord) {
    if (!coord)
        return NULL;
    return kCoordOpsVTable[coord->type].serialize(coord);
}

/* ============================================================
 * Deep Copy
 * ============================================================ */

/* Create a deep copy of a SymbolicCoord */
SymbolicCoord *symbolic_coord_copy(const SymbolicCoord *src) {
    if (!src)
        return NULL;

    SymbolicCoord *dst = lv_calloc(1, sizeof(SymbolicCoord));
    if (!dst)
        return NULL;

    dst->type = src->type;
    dst->trust = src->trust;
    dst->cache_valid = false; /* 复制品缓存初始无效，首次访问时重新计算 */
    dst->cached_value = 0.0;

    kCoordOpsVTable[src->type].copy_data(src, dst);

    bool copy_ok = kCoordOpsVTable[src->type].copy_check(dst);
    if (!copy_ok) {
        symbolic_coord_destroy(dst);
        return NULL;
    }

    return dst;
}

/* ============================================================
 * Basic Query Functions
 * ============================================================ */

/**
 * 检查符号坐标是否为零。
 *
 * @param coord 符号坐标对象（可为 NULL，NULL 视为非零）
 * @return true 表示为零，false 表示非零
 */
bool symbolic_coord_is_zero(const SymbolicCoord *coord) {
    if (!coord)
        return false;
    return kCoordOpsVTable[coord->type].is_zero(coord);
}

/**
 * 检查 SymbolicCoord 是否为正数。
 *
 * @param coord SymbolicCoord 对象
 * @return 如果值为正返回 true，否则返回 false
 */
bool symbolic_coord_is_positive(const SymbolicCoord *coord) {
    return symbolic_coord_to_double(coord) > 0;
}

/**
 * 检查 SymbolicCoord 是否为负数。
 *
 * @param coord SymbolicCoord 对象
 * @return 如果值为负返回 true，否则返回 false
 */
bool symbolic_coord_is_negative(const SymbolicCoord *coord) {
    return symbolic_coord_to_double(coord) < 0;
}

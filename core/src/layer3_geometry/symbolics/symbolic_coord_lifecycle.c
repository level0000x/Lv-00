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

#include "lv/debug.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/mpz_poly.h"

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

/* exempt: destroy handlers 均执行 "<T>_destroy(ptr); ptr=NULL;"（逐字同构），
 * 但各 handler 绑定类型专用析构函数与 SymbolicCoord union 字段。统一模板需要
 * 函数指针重解释 cast（C 标准未定义行为）或宏泛型（ABSTRACTION_SPEC 禁止），
 * 故保留逐字实现；分发本身已由 kCoordOpsVTable 表驱动收敛。 */

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

/* exempt: serialize handlers 均执行 "return <T>_serialize(ptr);"（逐字同构），
 * 绑定类型专用序列化函数，同上理由保留逐字实现。 */

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

/* exempt: copy_data handlers 语义逐类型不同（rational 单指针复制、algebraic 含
 * cached_rational 缓存复制、quadratic 分步构造、transcendental 深复制表达式树），
 * 强制统一将改变 mpq 精确算术语义，故保留差异实现。 */

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

/* exempt: copy_check handlers 均执行 "return (ptr != NULL);"（逐字同构），
 * 绑定类型专用字段，同上函数指针 cast 限制保留逐字实现。 */

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

/* exempt: is_zero handlers 判零语义逐类型不同（精确 mpq 比较 / cached_rational
 * 优先 + 隔离区间判断 / 双有理数判零 / 恒非零），保持差异实现。 */

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

SymbolicCoord *symbolic_coord_from_string(const char *str) {
    if (!str)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "symbolic_coord_from_string: NULL str");
    Rational *r = rational_parse(str);
    if (!r)
        return NULL;
    SymbolicCoord *coord = lv_calloc(1, sizeof(SymbolicCoord));
    if (!coord) {
        rational_destroy(r);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "symbolic_coord_from_string: lv_calloc failed");
    }
    coord->type = RATIONAL;
    coord->trust = TRUST_GREEN;
    coord->cache_valid = false;
    coord->cached_value = 0.0;
    coord->data.rational = r;
    return coord;
}

SymbolicCoord *symbolic_coord_from_double_scaled(double val, int64_t scale) {    double scaled_val = val * (double) scale;
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

bool symbolic_coord_pair_create_rational(int64_t num_x, uint64_t denom_x,
                                         int64_t num_y, uint64_t denom_y,
                                         SymbolicCoord **out_x, SymbolicCoord **out_y) {
    *out_x = NULL;
    *out_y = NULL;
    *out_x = symbolic_coord_create_rational(num_x, denom_x);
    if (!*out_x)
        return false;
    *out_y = symbolic_coord_create_rational(num_y, denom_y);
    if (!*out_y) {
        /* symbolic_coord_destroy 为 NULL 安全，此处统一回滚已创建项 */
        symbolic_coord_destroy(*out_x);
        *out_x = NULL;
        return false;
    }
    return true;
}

bool symbolic_coord_pair_from_double_scaled(double x, double y, int64_t scale,
                                            SymbolicCoord **out_x, SymbolicCoord **out_y) {
    *out_x = NULL;
    *out_y = NULL;
    *out_x = symbolic_coord_from_double_scaled(x, scale);
    if (!*out_x)
        return false;
    *out_y = symbolic_coord_from_double_scaled(y, scale);
    if (!*out_y) {
        /* symbolic_coord_destroy 为 NULL 安全，此处统一回滚已创建项 */
        symbolic_coord_destroy(*out_x);
        *out_x = NULL;
        return false;
    }
    return true;
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

void symbolic_coord_pair_destroy(SymbolicCoord *a, SymbolicCoord *b) {
    symbolic_coord_destroy(a);
    symbolic_coord_destroy(b);
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

bool symbolic_coord_get_xy(const SymbolicCoord *const *coords, int coord_count, double *x, double *y) {
    if (!coords || coord_count < 2 || !coords[0] || !coords[1])
        return false;
    *x = symbolic_coord_to_double(coords[0]);
    *y = symbolic_coord_to_double(coords[1]);
    return true;
}

bool symbolic_coord_get_segment(const SymbolicCoord *const *coords, int coord_count, double *x1, double *y1, double *x2,
                                double *y2) {
    if (!coords || coord_count < 4 || !coords[0] || !coords[1] || !coords[2] || !coords[3])
        return false;
    *x1 = symbolic_coord_to_double(coords[0]);
    *y1 = symbolic_coord_to_double(coords[1]);
    *x2 = symbolic_coord_to_double(coords[2]);
    *y2 = symbolic_coord_to_double(coords[3]);
    return true;
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

/* ============================================================
 * 符号常量池（TEN_LAYER_OPTIMIZED_PLAN §12.10 落地）
 * ============================================================ */

SymbolicCoord *lv_SYM_ZERO = NULL;
SymbolicCoord *lv_SYM_ONE = NULL;
SymbolicCoord *lv_SYM_TWO = NULL;
SymbolicCoord *lv_SYM_THREE = NULL;
SymbolicCoord *lv_SYM_HALF = NULL;
SymbolicCoord *lv_SYM_NEG_ONE = NULL;
SymbolicCoord *lv_SYM_SQRT2 = NULL;
SymbolicCoord *lv_SYM_SQRT3 = NULL;
SymbolicCoord *lv_SYM_PI = NULL;

void lv_symbolic_coord_init_constants(void) {
    if (lv_SYM_ZERO != NULL)
        return; /* 已初始化（幂等） */
    lv_SYM_ZERO = symbolic_coord_create_rational(0, 1);
    lv_SYM_ONE = symbolic_coord_create_rational(1, 1);
    lv_SYM_TWO = symbolic_coord_create_rational(2, 1);
    lv_SYM_THREE = symbolic_coord_create_rational(3, 1);
    lv_SYM_HALF = symbolic_coord_create_rational(1, 2);
    lv_SYM_NEG_ONE = symbolic_coord_create_rational(-1, 1);
    /* √2 + √3：符号 sqrt（quadratic 表示） */
    if (lv_SYM_TWO != NULL)
        lv_SYM_SQRT2 = symbolic_coord_sqrt(lv_SYM_TWO);
    if (lv_SYM_THREE != NULL)
        lv_SYM_SQRT3 = symbolic_coord_sqrt(lv_SYM_THREE);
    /* π：超越数 SymbolicCoord 表示 */
    lv_SYM_PI = symbolic_coord_create_transcendental("pi");
}
void lv_symbolic_coord_free_constants(void) {
    symbolic_coord_destroy(lv_SYM_PI);
    symbolic_coord_destroy(lv_SYM_ZERO);
    symbolic_coord_destroy(lv_SYM_ONE);
    symbolic_coord_destroy(lv_SYM_TWO);
    symbolic_coord_destroy(lv_SYM_THREE);
    symbolic_coord_destroy(lv_SYM_HALF);
    symbolic_coord_destroy(lv_SYM_NEG_ONE);
    symbolic_coord_destroy(lv_SYM_SQRT2);
    symbolic_coord_destroy(lv_SYM_SQRT3);
    lv_SYM_ZERO = NULL;
    lv_SYM_ONE = NULL;
    lv_SYM_TWO = NULL;
    lv_SYM_THREE = NULL;
    lv_SYM_HALF = NULL;
    lv_SYM_NEG_ONE = NULL;
    lv_SYM_SQRT2 = NULL;
    lv_SYM_SQRT3 = NULL;
    lv_SYM_PI = NULL;
}

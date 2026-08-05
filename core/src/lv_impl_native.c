/*
 * lv_impl_native.c — Base Layer Implementation
 * Provides all foundational data structures and algorithms for layer-0.
 * 
 * GMP (GNU Multiple Precision) arithmetic throughout.
 * 绝无 double/float；所有数值计算使用 mpq_t 精确有理数。
 */
#include <assert.h>
#include <gmp.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv_utils.h"
#include "lv/lv_log.h"
#include "lv_internal.h" /* lv_RETURN_ERROR / lv_RETURN_ERROR_NULL */

/* ================================================================
 *  Module-level state
 * ================================================================ */

static int64_t g_native_id = 2000000;

/**
 * @brief 分配全局原生ID
 */
static int64_t native_id_alloc(void) {
    return g_native_id++;
}

/* ================================================================
 *  Coord  —  Symbolic Coordinate (GMP rational)
 * ================================================================ */

typedef struct {
    int64_t id;
    mpq_t x;
    mpq_t y;
} Coord;

/**
 * @brief 用字符串坐标初始化坐标对象
 */
static void coord_init(Coord *c, const char *x_str, const char *y_str) {
    mpq_init(c->x);
    mpq_set_str(c->x, x_str, 10);
    mpq_init(c->y);
    mpq_set_str(c->y, y_str, 10);
}

/**
 * @brief 用整数坐标初始化坐标对象
 */
static void coord_init_si(Coord *c, long x_num, long y_num) {
    mpq_init(c->x);
    mpq_set_si(c->x, x_num, 1);
    mpq_init(c->y);
    mpq_set_si(c->y, y_num, 1);
}

/**
 * @brief 释放坐标对象的GMP内存
 */
static void coord_clear(Coord *c) {
    if (c) {
        mpq_clear(c->x);
        mpq_clear(c->y);
    }
}

/* 分配 Coord 对象骨架:lv_malloc + NULL 检查 + 分配全局ID。
 * fn_name 为调用函数名字符串字面量,用于保留原有的错误消息文本。
 * 失败时内部已通过 lv_RETURN_ERROR_NULL 记录错误,返回 NULL。 */
static Coord *coord_alloc(const char *fn_name) {
    Coord *c = (Coord *) lv_malloc(sizeof(Coord));
    if (!c)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "%s: malloc failed", fn_name);
    c->id = native_id_alloc();
    return c;
}

/* 二元坐标算子实现:"NULL 输入检查 → 分配骨架 → 逐坐标 mpq 运算"。
 * op 为 mpq_add/mpq_sub 等二元 GMP 函数指针(签名与 GMP 宏一致)。
 * 范式参照 mpz_poly.h 的 mpz_poly_binop。 */
static Coord *coord_binop_impl(const Coord *a, const Coord *b,
                               void (*op)(mpq_ptr, mpq_srcptr, mpq_srcptr)) {
    if (!a || !b)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "coord_binop: NULL input");
    Coord *c = coord_alloc("coord_binop_impl");
    if (!c)
        return NULL;
    mpq_init(c->x);
    op(c->x, a->x, b->x); /* GMP 精确运算 */
    mpq_init(c->y);
    op(c->y, a->y, b->y);
    return c;
}

Coord *coord_create(const char *x_str, const char *y_str) {
    Coord *c = coord_alloc("coord_create");
    if (!c)
        return NULL;
    coord_init(c, x_str, y_str);
    return c;
}

Coord *coord_create_si(long x_num, long y_num) {
    Coord *c = coord_alloc("coord_create_si");
    if (!c)
        return NULL;
    coord_init_si(c, x_num, y_num);
    return c;
}

void coord_destroy(Coord *c) {
    if (!c)
        return;
    coord_clear(c);
    lv_free((void **) &c);
    c = NULL;
}

Coord *coord_dup(const Coord *src) {
    if (!src)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "coord_dup: NULL src");
    Coord *c = coord_alloc("coord_dup");
    if (!c)
        return NULL;
    mpq_init(c->x);
    mpq_set(c->x, src->x);
    mpq_init(c->y);
    mpq_set(c->y, src->y);
    return c;
}

Coord *coord_add(const Coord *a, const Coord *b) {
    return coord_binop_impl(a, b, mpq_add);
}

Coord *coord_sub(const Coord *a, const Coord *b) {
    return coord_binop_impl(a, b, mpq_sub);
}

/* 标量二元坐标算子实现："NULL/标量检查 → 分配骨架 → 逐坐标 mpq 运算（坐标 op 标量）"。
 * 与 coord_binop_impl 同构，区别是第二操作数为标量 mpq_t（非 Coord），
 * 调用形式为 op(r->x, a->x, scalar)，第三参数即标量本身。
 * allow_zero 为标量合法性开关：coord_mul 允许 0 标量（传 1）；
 * coord_div 防除零（传 0，命中时按无效参数报错）。
 * err_msg 为错误消息（消息文本与旧实现逐字一致）。 */
static Coord *coord_scalar_binop_impl(const Coord *a, const mpq_t scalar,
                                      void (*op)(mpq_ptr, mpq_srcptr, mpq_srcptr),
                                      int allow_zero, const char *err_msg) {
    if (!a || (!allow_zero && mpq_sgn(scalar) == 0))
        lv_RETURN_ERROR_NULL(allow_zero ? lv_ERROR_NULL_POINTER : lv_ERROR_INVALID_PARAM, err_msg);
    Coord *c = coord_alloc("coord_scalar_binop_impl");
    if (!c)
        return NULL;
    mpq_init(c->x);
    op(c->x, a->x, scalar); /* GMP 精确运算 */
    mpq_init(c->y);
    op(c->y, a->y, scalar);
    return c;
}

Coord *coord_mul(const Coord *a, const mpq_t scalar) {
    return coord_scalar_binop_impl(a, scalar, mpq_mul, 1, "coord_mul: NULL input");
}

Coord *coord_div(const Coord *a, const mpq_t scalar) {
    return coord_scalar_binop_impl(a, scalar, mpq_div, 0, "coord_div: NULL input or zero scalar");
}

int coord_eq(const Coord *a, const Coord *b) {
    if (!a || !b)
        return 0;
    return (mpq_cmp(a->x, b->x) == 0) && (mpq_cmp(a->y, b->y) == 0); /* GMP 精确比较 */
}

int coord_ne(const Coord *a, const Coord *b) {
    return !coord_eq(a, b);
}

int coord_lt(const Coord *a, const Coord *b) {
    if (!a || !b)
        return 0;
    int cx = mpq_cmp(a->x, b->x);
    if (cx != 0)
        return cx < 0;
    return mpq_cmp(a->y, b->y) < 0;
}

int coord_le(const Coord *a, const Coord *b) {
    return coord_lt(a, b) || coord_eq(a, b);
}
int coord_gt(const Coord *a, const Coord *b) {
    return !coord_le(a, b);
}
int coord_ge(const Coord *a, const Coord *b) {
    return !coord_lt(a, b);
}

/* coord_distance: Euclidean distance via GMP.
 * Returns norm as mpq_t.  Uses mpq_mul + mpq_add + sqrt approximation.
 * For pure GMP, sqrt is not exact; we compute squared-distance precisely. */
void coord_dist_sq(mpq_t result, const Coord *a, const Coord *b) {
    mpq_t dx, dy, dx2, dy2;
    mpq_inits(dx, dy, dx2, dy2, NULL);
    mpq_sub(dx, a->x, b->x);
    mpq_mul(dx2, dx, dx);
    mpq_sub(dy, a->y, b->y);
    mpq_mul(dy2, dy, dy);
    mpq_add(result, dx2, dy2); /* GMP 精确平方距离 */
    mpq_clears(dx, dy, dx2, dy2, NULL);
}

Coord *coord_midpoint(const Coord *a, const Coord *b) {
    if (!a || !b)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "coord_midpoint: NULL input");
    Coord *c = coord_alloc("coord_midpoint");
    if (!c)
        return NULL;
    mpq_t two;
    mpq_init(two);
    mpq_set_si(two, 2, 1);
    mpq_init(c->x);
    mpq_add(c->x, a->x, b->x);
    mpq_div(c->x, c->x, two);
    mpq_init(c->y);
    mpq_add(c->y, a->y, b->y);
    mpq_div(c->y, c->y, two);
    mpq_clear(two);
    return c;
}

void coord_dot(mpq_t result, const Coord *a, const Coord *b) {
    mpq_t axbx, ayby;
    mpq_inits(axbx, ayby, NULL);
    mpq_mul(axbx, a->x, b->x);
    mpq_mul(ayby, a->y, b->y);
    mpq_add(result, axbx, ayby); /* GMP 精确点积 */
    mpq_clears(axbx, ayby, NULL);
}

void coord_cross(mpq_t result, const Coord *a, const Coord *b) {
    mpq_t axby, aybx;
    mpq_inits(axby, aybx, NULL);
    mpq_mul(axby, a->x, b->y);
    mpq_mul(aybx, a->y, b->x);
    mpq_sub(result, axby, aybx); /* GMP 精确叉积 */
    mpq_clears(axby, aybx, NULL);
}

int coord_to_string(const Coord *c, char *buf, size_t bufsz) {
    if (!c || !buf || bufsz == 0)
        return 0;
    char *xs = mpq_get_str(NULL, 10, c->x);
    char *ys = mpq_get_str(NULL, 10, c->y);
    if (!xs || !ys) {
        lv_free_external((void **) &xs);
        lv_free_external((void **) &ys);
        return snprintf(buf, bufsz, "(null)");
    }
    int n = snprintf(buf, bufsz, "(%s, %s)", xs, ys);
    lv_free_external((void **) &xs);
    lv_free_external((void **) &ys);
    return n;
}

/* ================================================================
 *  Rational  —  GMP Rational Numbers (mpq_t wrapper)
 * ================================================================ */

typedef struct {
    int64_t id;
    mpq_t val;
} Rational;

/* 分配 Rational 对象骨架:lv_malloc + NULL 检查 + 分配全局ID。
 * fn_name 为调用函数名字符串字面量,用于保留原有的错误消息文本。
 * 失败时内部已通过 lv_RETURN_ERROR_NULL 记录错误,返回 NULL。 */
static Rational *rational_alloc(const char *fn_name) {
    Rational *r = (Rational *) lv_malloc(sizeof(Rational));
    if (!r)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "%s: malloc failed", fn_name);
    r->id = native_id_alloc();
    return r;
}

/* 二元有理数算子实现:"NULL 输入检查 → 分配骨架 → 单值 mpq 运算"。
 * op 为 mpq_add/mpq_sub/mpq_mul 等二元 GMP 函数指针(签名与 GMP 宏一致)。 */
static Rational *rational_binop_impl(const Rational *a, const Rational *b,
                                     void (*op)(mpq_ptr, mpq_srcptr, mpq_srcptr)) {
    if (!a || !b)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "rational_binop: NULL input");
    Rational *r = rational_alloc("rational_binop_impl");
    if (!r)
        return NULL;
    mpq_init(r->val);
    op(r->val, a->val, b->val); /* GMP 精确运算 */
    return r;
}

static Rational *rational_add(const Rational *a, const Rational *b) {
    return rational_binop_impl(a, b, mpq_add);
}

Rational *rational_sub(const Rational *a, const Rational *b) {
    return rational_binop_impl(a, b, mpq_sub);
}

Rational *rational_mul(const Rational *a, const Rational *b) {
    return rational_binop_impl(a, b, mpq_mul);
}

Rational *rational_create_str(const char *s) {
    Rational *r = rational_alloc("rational_create_str");
    if (!r)
        return NULL;
    mpq_init(r->val);
    mpq_set_str(r->val, s, 10); /* GMP 精确解析 "num/den" 或 "int" */
    mpq_canonicalize(r->val);
    return r;
}

Rational *rational_create_si(long num, unsigned long den) {
    Rational *r = rational_alloc("rational_create_si");
    if (!r)
        return NULL;
    mpq_init(r->val);
    mpq_set_si(r->val, num, den);
    mpq_canonicalize(r->val);
    return r;
}

Rational *rational_from_int(long n) {
    return rational_create_si(n, 1);
}

/**
 * @brief 销毁有理数对象并释放内存
 */
static void rational_destroy(Rational *r) {
    if (r) {
        mpq_clear(r->val);
        lv_free((void **) &r);
    }
}

Rational *rational_div(const Rational *a, const Rational *b) {
    if (!a || !b || mpq_sgn(b->val) == 0)
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "rational_div: NULL input or division by zero");
    Rational *r = rational_alloc("rational_div");
    if (!r)
        return NULL;
    mpq_init(r->val);
    mpq_div(r->val, a->val, b->val); /* GMP 精确除法 */
    return r;
}

int rational_cmp(const Rational *a, const Rational *b) {
    if (!a || !b)
        return 0;
    return mpq_cmp(a->val, b->val); /* GMP 精确比较 */
}

int rational_eq(const Rational *a, const Rational *b) {
    return rational_cmp(a, b) == 0;
}
int rational_ne(const Rational *a, const Rational *b) {
    return rational_cmp(a, b) != 0;
}
int rational_lt(const Rational *a, const Rational *b) {
    return rational_cmp(a, b) < 0;
}
int rational_le(const Rational *a, const Rational *b) {
    return rational_cmp(a, b) <= 0;
}
int rational_gt(const Rational *a, const Rational *b) {
    return rational_cmp(a, b) > 0;
}
int rational_ge(const Rational *a, const Rational *b) {
    return rational_cmp(a, b) >= 0;
}

int rational_to_string(const Rational *r, char *buf, size_t bufsz) {
    if (!r || !buf || bufsz == 0)
        return 0;
    char *s = mpq_get_str(NULL, 10, r->val);
    if (!s)
        return snprintf(buf, bufsz, "(null)");
    int n = snprintf(buf, bufsz, "%s", s);
    lv_free_external((void **) &s);
    return n;
}

/* ================================================================
 *  native_ConstraintGraph  —  Constraints with GMP rational values
 * ================================================================ */

typedef struct {
    int64_t id;
    mpq_t value; /* GMP 精确值 */
    int pinned;
} native_GraphNode;

typedef struct {
    int64_t id;
    int32_t from;
    int32_t to;
    mpq_t weight; /* GMP 精确权重 */
} native_GraphEdge;

typedef struct {
    int64_t id;
    native_GraphNode *nodes;
    int node_count;
    int node_cap;
    native_GraphEdge *edges;
    int edge_count;
    int edge_cap;
} native_ConstraintGraph;

/**
 * @brief 释放图节点的GMP值内存
 */
static void native_graph_node_clear(native_GraphNode *n) {
    if (n)
        mpq_clear(n->value);
}
/**
 * @brief 释放图边的GMP权重内存
 */
static void native_graph_edge_clear(native_GraphEdge *e) {
    if (e)
        mpq_clear(e->weight);
}

/* 节点数组扩容骨架(graph_add_node 使用):复用公共 lv_ensure_capacity
 * (倍增策略 + 溢出检查内置于 lv_utils;失败时错误已记录,返回 -1)。
 * 新增槽位的 mpq_t 由 graph_add_node 的 add 路径逐个 mpq_init,此处不处理。 */
static int native_graph_grow_nodes(native_ConstraintGraph *g) {
    if (!lv_ensure_capacity((void **) &g->nodes, g->node_count, &g->node_cap,
                            sizeof(native_GraphNode), 1))
        return -1;
    return 0;
}

/* 边数组扩容骨架(graph_add_edge 使用):同上,复用 lv_ensure_capacity */
static int native_graph_grow_edges(native_ConstraintGraph *g) {
    if (!lv_ensure_capacity((void **) &g->edges, g->edge_count, &g->edge_cap,
                            sizeof(native_GraphEdge), 1))
        return -1;
    return 0;
}

/* swap-remove 骨架:释放 GMP 值后用末尾元素覆盖被删槽位并递减计数。
 * 注意:此处仅通过 native_graph_node_clear 释放一次 GMP 值(不得再手动 mpq_clear,否则双重 free)。 */
static void native_graph_swap_remove_node(native_ConstraintGraph *g, int i) {
    native_graph_node_clear(&g->nodes[i]);
    g->nodes[i] = g->nodes[--g->node_count];
}

static void native_graph_swap_remove_edge(native_ConstraintGraph *g, int i) {
    native_graph_edge_clear(&g->edges[i]);
    g->edges[i] = g->edges[--g->edge_count];
}

/* 线性查找辅助由 graph_find_node / graph_find_edge 直接展开实现
 * (原 LV_GRAPH_LINEAR_FIND 宏函数化:数组元素类型不同,分别手写循环)。 */

/**
 * @brief 创建约束图并分配初始缓冲区
 */
static native_ConstraintGraph *native_graph_create(void) {
    native_ConstraintGraph *g = (native_ConstraintGraph *) lv_calloc(1, sizeof(native_ConstraintGraph));
    if (!g)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "native_graph_create: calloc failed");
    g->id = native_id_alloc();
    g->node_cap = 16;
    g->edge_cap = 16;
    g->nodes = (native_GraphNode *) lv_calloc(g->node_cap, sizeof(native_GraphNode));
    g->edges = (native_GraphEdge *) lv_calloc(g->edge_cap, sizeof(native_GraphEdge));
    return g;
}

/**
 * @brief 销毁约束图并释放所有内存
 */
static void native_graph_destroy(native_ConstraintGraph *g) {
    if (!g)
        return;
    for (int i = 0; i < g->node_count; i++)
        native_graph_node_clear(&g->nodes[i]);
    for (int i = 0; i < g->edge_count; i++)
        native_graph_edge_clear(&g->edges[i]);
    lv_free((void **) &g->nodes);
    lv_free((void **) &g->edges);
    lv_free((void **) &g);
}

int64_t graph_add_node(native_ConstraintGraph *g, const mpq_t value, int pinned) {
    if (!g)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "graph_add_node: NULL graph");
    if (g->node_count >= g->node_cap) {
        if (native_graph_grow_nodes(g) < 0)
            return -1;
    }
    int idx = g->node_count++;
    g->nodes[idx].id = native_id_alloc();
    mpq_init(g->nodes[idx].value);
    mpq_set(g->nodes[idx].value, value); /* GMP 精确值 */
    g->nodes[idx].pinned = pinned;
    return g->nodes[idx].id;
}

int64_t graph_add_node_si(native_ConstraintGraph *g, long num, long den, int pinned) {
    mpq_t val;
    mpq_init(val);
    mpq_set_si(val, num, den);
    int64_t id = graph_add_node(g, val, pinned);
    mpq_clear(val);
    return id;
}

/**
 * @brief 按ID删除约束图节点
 */
static int native_graph_remove_node(native_ConstraintGraph *g, int64_t node_id) {
    if (!g)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "native_graph_remove_node: NULL graph");
    for (int i = 0; i < g->node_count; i++) {
        if (g->nodes[i].id == node_id) {
            native_graph_swap_remove_node(g, i); /* 内部仅 mpq_clear 一次(修复双重 free) */
            return 0;
        }
    }
    return -1; /* 未找到 */
}

int64_t graph_add_edge(native_ConstraintGraph *g, int from_idx, int to_idx, const mpq_t weight) {
    if (!g || from_idx < 0 || to_idx < 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "graph_add_edge: NULL graph or invalid index");
    if (from_idx >= g->node_count || to_idx >= g->node_count)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "graph_add_edge: index out of range");
    if (g->edge_count >= g->edge_cap) {
        if (native_graph_grow_edges(g) < 0)
            return -1;
    }
    int idx = g->edge_count++;
    g->edges[idx].id = native_id_alloc();
    mpq_init(g->edges[idx].weight);
    mpq_set(g->edges[idx].weight, weight); /* GMP 精确权重 */
    g->edges[idx].from = from_idx;
    g->edges[idx].to = to_idx;
    return g->edges[idx].id;
}

int64_t graph_add_edge_si(native_ConstraintGraph *g, int from, int to, long wnum, long wden) {
    mpq_t w;
    mpq_init(w);
    mpq_set_si(w, wnum, wden);
    int64_t id = graph_add_edge(g, from, to, w);
    mpq_clear(w);
    return id;
}

int graph_remove_edge(native_ConstraintGraph *g, int64_t edge_id) {
    if (!g)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "graph_remove_edge: NULL graph");
    for (int i = 0; i < g->edge_count; i++) {
        if (g->edges[i].id == edge_id) {
            native_graph_swap_remove_edge(g, i); /* 内部仅 mpq_clear 一次(修复双重 free) */
            return 0;
        }
    }
    return -1; /* 未找到 */
}

const native_GraphEdge *graph_get_edge(const native_ConstraintGraph *g, int index) {
    if (!g || index < 0 || index >= g->edge_count)
        return NULL;
    return &g->edges[index];
}

/* graph_solve: 约束传播 — 使用 GMP 精确有理数比较 */
int graph_solve(native_ConstraintGraph *g) {
    if (!g || g->node_count == 0)
        return 0;
    mpq_t eps, propagated, diff;
    mpq_inits(eps, propagated, diff, NULL);
    mpq_set_str(eps, "1/1000000000000", 10); /* ε = 10⁻¹² — 用字符串避免 32-bit overflow */

    int max_iters = (g->node_count > INT_MAX / 2) ? INT_MAX : g->node_count * 2;
    for (int iter = 0; iter < max_iters; iter++) {
        int changed = 0;
        for (int i = 0; i < g->edge_count; i++) {
            int from = g->edges[i].from;
            int to = g->edges[i].to;
            if (from >= g->node_count || to >= g->node_count)
                continue;
            mpq_add(propagated, g->nodes[from].value, g->edges[i].weight); /* GMP 加法 */
            mpq_sub(diff, g->nodes[to].value, propagated);                 /* GMP 减法 */
            if (mpq_sgn(diff) < 0)
                mpq_neg(diff, diff);                              /* GMP 绝对值 */
            if (!g->nodes[to].pinned && mpq_cmp(diff, eps) > 0) { /* GMP 比较 */
                mpq_set(g->nodes[to].value, propagated);
                changed = 1;
            }
        }
        if (!changed)
            break;
    }
    mpq_clears(eps, propagated, diff, NULL);
    return 0;
}

/**
 * @brief 将约束图中的值平移至非负
 */
static void native_graph_normalize(native_ConstraintGraph *g) {
    if (!g || g->node_count == 0)
        return;
    int min_idx = 0;
    for (int i = 1; i < g->node_count; i++) {
        if (mpq_cmp(g->nodes[i].value, g->nodes[min_idx].value) < 0) /* GMP 比较 */
            min_idx = i;
    }
    for (int i = 0; i < g->node_count; i++) {
        if (!g->nodes[i].pinned)
            mpq_sub(g->nodes[i].value, g->nodes[i].value, g->nodes[min_idx].value); /* GMP 平移 */
    }
}

void graph_clear(native_ConstraintGraph *g) {
    if (!g)
        return;
    for (int i = 0; i < g->node_count; i++)
        native_graph_node_clear(&g->nodes[i]);
    for (int i = 0; i < g->edge_count; i++)
        native_graph_edge_clear(&g->edges[i]);
    g->node_count = 0;
    g->edge_count = 0;
}

int graph_node_count(const native_ConstraintGraph *g) {
    return g ? g->node_count : 0;
}
int graph_edge_count(const native_ConstraintGraph *g) {
    return g ? g->edge_count : 0;
}

static int graph_find_node(const native_ConstraintGraph *g, int64_t node_id) {
    if (!g)
        return -1;
    for (int i = 0; i < g->node_count; i++)
        if (g->nodes[i].id == node_id)
            return i;
    return -1;
}

static int graph_find_edge(const native_ConstraintGraph *g, int64_t edge_id) {
    if (!g)
        return -1;
    for (int i = 0; i < g->edge_count; i++)
        if (g->edges[i].id == edge_id)
            return i;
    return -1;
}

int graph_validate(const native_ConstraintGraph *g) {
    if (!g)
        return 0;
    for (int i = 0; i < g->edge_count; i++) {
        if (g->edges[i].from >= g->node_count)
            return 0;
        if (g->edges[i].to >= g->node_count)
            return 0;
    }
    return 1;
}

/* ================================================================
 *  Expr  —  Expression Evaluator (GMP rational)
 * ================================================================ */

typedef struct ExprNode {
    int kind;   /* 0=const, 1=var, 2=add, 3=sub, 4=mul, 5=div, 6=pow */
    mpq_t val;  /* GMP 精确值 (if const) */
    char *name; /* variable name (if var) */
    struct ExprNode *left;
    struct ExprNode *right;
    int exp; /* exponent (if pow) */
} Expr;

/**
 * @brief 创建表达式树的叶子节点
 */
static Expr *expr_new_leaf(int kind) {
    Expr *e = (Expr *) lv_calloc(1, sizeof(Expr));
    if (!e)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "expr_new_leaf: calloc failed");
    mpq_init(e->val);
    e->kind = kind;
    return e;
}

Expr *expr_create_const_si(long num, unsigned long den) {
    Expr *e = expr_new_leaf(0);
    if (!e)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "expr_create_const_si: alloc failed");
    mpq_set_si(e->val, num, den);
    return e;
}

Expr *expr_create_var(const char *name) {
    Expr *e = expr_new_leaf(1);
    if (!e)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "expr_create_var: alloc failed");
    e->name = lv_strdup_safe(name);
    return e;
}

Expr *expr_create_binop(int kind, Expr *left, Expr *right) {
    Expr *e = expr_new_leaf(kind);
    if (!e)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "expr_create_binop: alloc failed");
    e->left = left;
    e->right = right;
    return e;
}

void expr_destroy(Expr *e) {
    if (!e)
        return;
    Expr *stack[256];
    int top = 0;
    Expr *node = e;
    while (node) {
        Expr *left = node->left;
        Expr *right = node->right;
        mpq_clear(node->val);
        lv_free((void **) &node->name);
        lv_free((void **) &node);
        if (right && top < 255) {
            stack[++top] = right;
        }
        node = left;
        if (!node && top > 0) {
            node = stack[top--];
        }
    }
}

/* ---- VTable-based expression evaluation ---- */

/* Forward declaration */
int expr_eval(mpq_t result, Expr *e, const char **varnames, const mpq_t *values, int nvars);

/* Function pointer type for expression evaluation handlers */
typedef int (*ExprEvalFn)(mpq_t result, Expr *e, const char **varnames, const mpq_t *values, int nvars);

/* --- Handler functions --- */

static int eval_const(mpq_t result, Expr *e, const char **varnames, const mpq_t *values, int nvars) {
    (void)varnames; (void)values; (void)nvars;
    mpq_set(result, e->val);
    return 0;
}

static int eval_var(mpq_t result, Expr *e, const char **varnames, const mpq_t *values, int nvars) {
    for (int i = 0; i < nvars; i++) {
        if (strcmp(e->name, varnames[i]) == 0) {
            mpq_set(result, values[i]);
            return 0;
        }
    }
    mpq_set_si(result, 0, 1);
    return -1; /* var not found */
}

/* EVAL 二元算子实现:左右子树求值后做一次 mpq 运算。
 * op 为 mpq_add/mpq_sub/mpq_mul 等二元 GMP 函数指针(签名与 GMP 宏一致)。 */
static int eval_binop_impl(mpq_t result, Expr *e, const char **varnames, const mpq_t *values, int nvars,
                           void (*op)(mpq_ptr, mpq_srcptr, mpq_srcptr)) {
    mpq_t l, r;
    mpq_inits(l, r, NULL);
    expr_eval(l, e->left, varnames, values, nvars);
    expr_eval(r, e->right, varnames, values, nvars);
    op(result, l, r);
    mpq_clears(l, r, NULL);
    return 0;
}

static int eval_add(mpq_t result, Expr *e, const char **varnames, const mpq_t *values, int nvars) {
    return eval_binop_impl(result, e, varnames, values, nvars, mpq_add);
}

static int eval_sub(mpq_t result, Expr *e, const char **varnames, const mpq_t *values, int nvars) {
    return eval_binop_impl(result, e, varnames, values, nvars, mpq_sub);
}

static int eval_mul(mpq_t result, Expr *e, const char **varnames, const mpq_t *values, int nvars) {
    return eval_binop_impl(result, e, varnames, values, nvars, mpq_mul);
}

static int eval_div(mpq_t result, Expr *e, const char **varnames, const mpq_t *values, int nvars) {
    mpq_t l, r;
    mpq_inits(l, r, NULL);
    expr_eval(l, e->left, varnames, values, nvars);
    expr_eval(r, e->right, varnames, values, nvars);
    if (mpq_sgn(r) == 0) {
        mpq_set_si(result, 0, 1);
        mpq_clears(l, r, NULL);
        return -2;
    }
    mpq_div(result, l, r);
    mpq_clears(l, r, NULL);
    return 0;
}

static int eval_pow(mpq_t result, Expr *e, const char **varnames, const mpq_t *values, int nvars) {
    if (expr_eval(result, e->left, varnames, values, nvars) < 0)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "expr_eval: pow sub-eval failed");
    {
        mpz_t num, den, base_num, base_den;
        mpz_inits(num, den, base_num, base_den, NULL);
        mpq_get_num(base_num, result);
        mpq_get_den(base_den, result);
        mpz_set(num, base_num);
        mpz_set(den, base_den);
        for (int i = 1; i < e->exp; i++) {
            mpz_mul(num, num, base_num);
            mpz_mul(den, den, base_den);
        }
        mpq_set_num(result, num);
        mpq_set_den(result, den);
        mpz_clears(num, den, base_num, base_den, NULL);
    }
    return 0;
}

/* VTable for expression evaluation */
static const ExprEvalFn kExprEvalHandlers[] = {
    [0] = eval_const,
    [1] = eval_var,
    [2] = eval_add,
    [3] = eval_sub,
    [4] = eval_mul,
    [5] = eval_div,
    [6] = eval_pow,
};

/* expr_eval: 代入 env (var_name → mpq_t*) 计算精确有理数值 */
int expr_eval(mpq_t result, Expr *e, const char **varnames, const mpq_t *values, int nvars) {
    if (!e)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "expr_eval: NULL expr");
    if ((unsigned)e->kind < sizeof(kExprEvalHandlers)/sizeof(kExprEvalHandlers[0]) && kExprEvalHandlers[e->kind]) {
        return kExprEvalHandlers[e->kind](result, e, varnames, values, nvars);
    }
    mpq_set_si(result, 0, 1);
    lv_RETURN_ERROR(lv_ERROR_INTERNAL, "expr_eval: unknown expr kind %d", e->kind);
    return -1;
}

/* expr_compare: 用 GMP 比较两表达式在给定环境下的值 */
int expr_compare(Expr *a, Expr *b, const char **varnames, const mpq_t *values, int nvars) {
    mpq_t va, vb;
    mpq_inits(va, vb, NULL);
    expr_eval(va, a, varnames, values, nvars);
    expr_eval(vb, b, varnames, values, nvars);
    int cmp = mpq_cmp(va, vb); /* GMP 精确比较 */
    mpq_clears(va, vb, NULL);
    return cmp;
}

/* ================================================================
 *  MemPool  —  Simple Arena Allocator
 * ================================================================ */

typedef struct MemChunk {
    char *data;
    size_t used;
    size_t cap;
    struct MemChunk *next;
} MemChunk;

typedef struct {
    int64_t id;
    MemChunk *head;
} MemPool;

MemPool *pool_create(void) {
    MemPool *p = (MemPool *) lv_calloc(1, sizeof(MemPool));
    if (!p)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "pool_create: calloc failed");
    p->id = native_id_alloc();
    p->head = (MemChunk *) lv_calloc(1, sizeof(MemChunk));
    if (!p->head) {
        lv_free((void **) &p);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "pool_create: head calloc failed");
    }
    p->head->cap = 65536; /* 64KB chunk */
    p->head->data = (char *) lv_malloc(p->head->cap);
    if (!p->head->data) {
        lv_free((void **) &p->head);
        lv_free((void **) &p);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "pool_create: head->data malloc failed");
    }
    return p;
}

void *pool_alloc(MemPool *p, size_t sz) {
    if (!p || !p->head)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "pool_alloc: NULL pool");
    if (p->head->used + sz > p->head->cap) {
        MemChunk *c = (MemChunk *) lv_calloc(1, sizeof(MemChunk));
        if (!c)
            lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "pool_alloc: chunk calloc failed");
        c->cap = sz > 65536 ? sz : 65536;
        c->data = (char *) lv_malloc(c->cap);
        if (!c->data) {
            lv_free((void **) &c);
            lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "pool_alloc: chunk->data malloc failed");
        }
        c->next = p->head;
        p->head = c;
    }
    void *ptr = p->head->data + p->head->used;
    p->head->used += sz;
    return ptr;
}

void pool_reset(MemPool *p) {
    if (!p)
        return;
    MemChunk *c = p->head;
    while (c) {
        c->used = 0;
        c = c->next;
    }
}

void pool_destroy(MemPool *p) {
    if (!p)
        return;
    MemChunk *c = p->head;
    while (c) {
        MemChunk *n = c->next;
        lv_free((void **) &c->data);
        lv_free((void **) &c);
        c = n;
    }
    lv_free((void **) &p);
}

/* ================================================================
 *  Debug  —  Trace / Breakpoint / Dump
 * ================================================================ */

/* 调试级别进程级共享:debug_trace 读 / debug_set_level 无锁写。
 * 多线程日志路径存在理论 data race,属已知低危(本文件无 lv_THREAD_LOCAL
 * 线程安全惯例,保守起见保持现状,不引入 TLS 改动)。 */
static int g_debug_level = 0;

void debug_trace(const char *fmt, ...) {
    if (g_debug_level < 1)
        return;
    va_list ap;
    va_start(ap, fmt);
    char _dbg_buf[lv_LARGE_BUF_SIZE];
    vsnprintf(_dbg_buf, sizeof(_dbg_buf), fmt, ap);
    va_end(ap);
    lv_DEBUG("%s", _dbg_buf);
}

void debug_set_level(int lvl) {
    g_debug_level = lvl;
}
int debug_get_level(void) {
    return g_debug_level;
}

void debug_breakpoint(void) {
    lv_DEBUG("breakpoint (g_native_id=%lld)", (long long) g_native_id);
}

void debug_dump(const char *label, const void *ptr, size_t sz) {
    lv_DEBUG("%s: %zu bytes at %p", label ? label : "dump", sz, ptr);
}

/* ================================================================
 *  Self-test
 * ================================================================ */

int native_self_test(void) {
    /* Coord GMP test */
    Coord *a = coord_create_si(0, 0);
    Coord *b = coord_create_si(3, 4);
    mpq_t dsq;
    mpq_init(dsq);
    coord_dist_sq(dsq, a, b);
    /* distance² = 3²+4² = 25 — should be exactly 25 */
    assert(mpq_cmp_si(dsq, 25, 1) == 0); /* GMP 精确断言 */
    mpq_clear(dsq);
    coord_destroy(a);
    coord_destroy(b);

    /* Rational GMP test */
    Rational *r1 = rational_create_si(1, 3);
    Rational *r2 = rational_create_si(2, 3);
    Rational *rs = rational_add(r1, r2);
    assert(mpq_cmp_si(rs->val, 1, 1) == 0); /* 1/3+2/3=1 */
    rational_destroy(r1);
    rational_destroy(r2);
    rational_destroy(rs);

    /* native_ConstraintGraph GMP test */
    native_ConstraintGraph *g = native_graph_create();
    mpq_t v;
    mpq_init(v);
    mpq_set_si(v, 1, 2);
    int64_t n0 = graph_add_node(g, v, 0);
    mpq_set_si(v, 3, 4);
    int64_t n1 = graph_add_node(g, v, 0);
    assert(g->node_count == 2);
    mpq_clear(v);
    native_graph_destroy(g);

    return 0;
}

/* ================================================================
 *  High-level GMP geometry functions (stubs.c interface)
 *  coord_rotate / coord_from_polar: 超越函数 sin/cos 不可精确有理化
 *  preset_measure_*: 用 GMP mpq_t 精确鞋带公式/叉积
 * ================================================================ */

Coord *coord_rotate(const Coord *c, double angle) {
    if (!c)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "coord_rotate: NULL input");
    double cs = cos(angle), sn = sin(angle);
    mpq_t cs_q, sn_q;
    mpq_init(cs_q);
    mpq_set_d(cs_q, cs);
    mpq_init(sn_q);
    mpq_set_d(sn_q, sn);
    Coord *r = (Coord *) lv_malloc(sizeof(Coord));
    if (!r) {
        mpq_clears(cs_q, sn_q, NULL);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "coord_rotate: malloc failed");
    }
    r->id = native_id_alloc();
    mpq_init(r->x);
    mpq_init(r->y);
    mpq_t t1, t2;
    mpq_inits(t1, t2, NULL);
    mpq_mul(t1, c->x, cs_q);
    mpq_mul(t2, c->y, sn_q);
    mpq_sub(r->x, t1, t2);
    mpq_mul(t1, c->x, sn_q);
    mpq_mul(t2, c->y, cs_q);
    mpq_add(r->y, t1, t2);
    mpq_clears(t1, t2, cs_q, sn_q, NULL);
    return r;
}

Coord *coord_from_polar(double r, double theta) {
    double cs = cos(theta), sn = sin(theta);
    mpq_t rq, cq, sq;
    mpq_init(rq);
    mpq_set_d(rq, r);
    mpq_init(cq);
    mpq_set_d(cq, cs);
    mpq_init(sq);
    mpq_set_d(sq, sn);
    Coord *c = (Coord *) lv_malloc(sizeof(Coord));
    if (!c) {
        mpq_clears(rq, cq, sq, NULL);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "coord_from_polar: malloc failed");
    }
    c->id = native_id_alloc();
    mpq_init(c->x);
    mpq_mul(c->x, rq, cq);
    mpq_init(c->y);
    mpq_mul(c->y, rq, sq);
    mpq_clears(rq, cq, sq, NULL);
    return c;
}

void preset_measure_length(mpq_t result, const Coord *a, const Coord *b) {
    coord_dist_sq(result, a, b); /* GMP 精确平方距离 */
}

void preset_measure_area(mpq_t result, const Coord *vertices, int n) {
    mpq_set_si(result, 0, 1);
    if (n < 3)
        return;
    mpq_t cross;
    mpq_init(cross);
    for (int i = 0; i < n; i++) {
        coord_cross(cross, &vertices[i], &vertices[(i + 1) % n]);
        mpq_add(result, result, cross);
    }
    if (mpq_sgn(result) < 0)
        mpq_neg(result, result);
    mpq_t two;
    mpq_init(two);
    mpq_set_si(two, 2, 1);
    mpq_div(result, result, two);
    mpq_clears(cross, two, NULL);
}

void preset_polygon_area(mpq_t result, const Coord *vertices, int n) {
    preset_measure_area(result, vertices, n); /* 复用鞋带公式 */
}

/*
 * lv00_impl_native.c — Base Layer Implementation
 * Provides all foundational data structures and algorithms for layer-0.
 * 
 * GMP (GNU Multiple Precision) arithmetic throughout.
 * 绝无 double/float；所有数值计算使用 mpq_t 精确有理数。
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <gmp.h>
#include <assert.h>

/* ================================================================
 *  Module-level state
 * ================================================================ */

static int64_t g_native_id = 2000000;

static int64_t native_id_alloc(void) {
    return g_native_id++;
}

/* ================================================================
 *  Coord  —  Symbolic Coordinate (GMP rational)
 * ================================================================ */

typedef struct {
    int64_t id;
    mpq_t   x;
    mpq_t   y;
} Coord;

static void coord_init(Coord* c, const char* x_str, const char* y_str) {
    mpq_init(c->x); mpq_set_str(c->x, x_str, 10);
    mpq_init(c->y); mpq_set_str(c->y, y_str, 10);
}

static void coord_init_si(Coord* c, long x_num, long y_num) {
    mpq_init(c->x); mpq_set_si(c->x, x_num, 1);
    mpq_init(c->y); mpq_set_si(c->y, y_num, 1);
}

static void coord_clear(Coord* c) {
    if (c) { mpq_clear(c->x); mpq_clear(c->y); }
}

Coord* coord_create(const char* x_str, const char* y_str) {
    Coord* c = (Coord*)malloc(sizeof(Coord));
    if (!c) return NULL;
    c->id = native_id_alloc();
    coord_init(c, x_str, y_str);
    return c;
}

Coord* coord_create_si(long x_num, long y_num) {
    Coord* c = (Coord*)malloc(sizeof(Coord));
    if (!c) return NULL;
    c->id = native_id_alloc();
    coord_init_si(c, x_num, y_num);
    return c;
}

void coord_destroy(Coord* c) {
    if (!c) return;
    coord_clear(c);
    free(c);
}

Coord* coord_dup(const Coord* src) {
    if (!src) return NULL;
    Coord* c = (Coord*)malloc(sizeof(Coord));
    if (!c) return NULL;
    c->id = native_id_alloc();
    mpq_init(c->x); mpq_set(c->x, src->x);
    mpq_init(c->y); mpq_set(c->y, src->y);
    return c;
}

Coord* coord_add(const Coord* a, const Coord* b) {
    if (!a || !b) return NULL;
    Coord* c = (Coord*)malloc(sizeof(Coord));
    if (!c) return NULL;
    c->id = native_id_alloc();
    mpq_init(c->x); mpq_add(c->x, a->x, b->x);   /* GMP 精确加法 */
    mpq_init(c->y); mpq_add(c->y, a->y, b->y);
    return c;
}

Coord* coord_sub(const Coord* a, const Coord* b) {
    if (!a || !b) return NULL;
    Coord* c = (Coord*)malloc(sizeof(Coord));
    if (!c) return NULL;
    c->id = native_id_alloc();
    mpq_init(c->x); mpq_sub(c->x, a->x, b->x);   /* GMP 精确减法 */
    mpq_init(c->y); mpq_sub(c->y, a->y, b->y);
    return c;
}

Coord* coord_mul(const Coord* a, const mpq_t scalar) {
    if (!a) return NULL;
    Coord* c = (Coord*)malloc(sizeof(Coord));
    if (!c) return NULL;
    c->id = native_id_alloc();
    mpq_init(c->x); mpq_mul(c->x, a->x, scalar);  /* GMP 精确乘法 */
    mpq_init(c->y); mpq_mul(c->y, a->y, scalar);
    return c;
}

Coord* coord_div(const Coord* a, const mpq_t scalar) {
    if (!a || mpq_sgn(scalar) == 0) return NULL;
    Coord* c = (Coord*)malloc(sizeof(Coord));
    if (!c) return NULL;
    c->id = native_id_alloc();
    mpq_init(c->x); mpq_div(c->x, a->x, scalar);  /* GMP 精确除法 */
    mpq_init(c->y); mpq_div(c->y, a->y, scalar);
    return c;
}

int coord_eq(const Coord* a, const Coord* b) {
    if (!a || !b) return 0;
    return (mpq_cmp(a->x, b->x) == 0) && (mpq_cmp(a->y, b->y) == 0); /* GMP 精确比较 */
}

int coord_ne(const Coord* a, const Coord* b) { return !coord_eq(a, b); }

int coord_lt(const Coord* a, const Coord* b) {
    if (!a || !b) return 0;
    int cx = mpq_cmp(a->x, b->x);
    if (cx != 0) return cx < 0;
    return mpq_cmp(a->y, b->y) < 0;
}

int coord_le(const Coord* a, const Coord* b) { return coord_lt(a, b) || coord_eq(a, b); }
int coord_gt(const Coord* a, const Coord* b) { return !coord_le(a, b); }
int coord_ge(const Coord* a, const Coord* b) { return !coord_lt(a, b); }

/* coord_distance: Euclidean distance via GMP.
 * Returns norm as mpq_t.  Uses mpq_mul + mpq_add + sqrt approximation.
 * For pure GMP, sqrt is not exact; we compute squared-distance precisely. */
void coord_dist_sq(mpq_t result, const Coord* a, const Coord* b) {
    mpq_t dx, dy, dx2, dy2;
    mpq_inits(dx, dy, dx2, dy2, NULL);
    mpq_sub(dx, a->x, b->x);  mpq_mul(dx2, dx, dx);
    mpq_sub(dy, a->y, b->y);  mpq_mul(dy2, dy, dy);
    mpq_add(result, dx2, dy2); /* GMP 精确平方距离 */
    mpq_clears(dx, dy, dx2, dy2, NULL);
}

Coord* coord_midpoint(const Coord* a, const Coord* b) {
    if (!a || !b) return NULL;
    Coord* c = (Coord*)malloc(sizeof(Coord));
    if (!c) return NULL;
    c->id = native_id_alloc();
    mpq_t two;
    mpq_init(two); mpq_set_si(two, 2, 1);
    mpq_init(c->x); mpq_add(c->x, a->x, b->x); mpq_div(c->x, c->x, two);
    mpq_init(c->y); mpq_add(c->y, a->y, b->y); mpq_div(c->y, c->y, two);
    mpq_clear(two);
    return c;
}

void coord_dot(mpq_t result, const Coord* a, const Coord* b) {
    mpq_t axbx, ayby;
    mpq_inits(axbx, ayby, NULL);
    mpq_mul(axbx, a->x, b->x);
    mpq_mul(ayby, a->y, b->y);
    mpq_add(result, axbx, ayby);  /* GMP 精确点积 */
    mpq_clears(axbx, ayby, NULL);
}

void coord_cross(mpq_t result, const Coord* a, const Coord* b) {
    mpq_t axby, aybx;
    mpq_inits(axby, aybx, NULL);
    mpq_mul(axby, a->x, b->y);
    mpq_mul(aybx, a->y, b->x);
    mpq_sub(result, axby, aybx);  /* GMP 精确叉积 */
    mpq_clears(axby, aybx, NULL);
}

int coord_to_string(const Coord* c, char* buf, size_t bufsz) {
    if (!c || !buf || bufsz == 0) return 0;
    char* xs = mpq_get_str(NULL, 10, c->x);
    char* ys = mpq_get_str(NULL, 10, c->y);
    if (!xs || !ys) { free(xs); free(ys); return snprintf(buf, bufsz, "(null)"); }
    int n = snprintf(buf, bufsz, "(%s, %s)", xs, ys);
    free(xs); free(ys);
    return n;
}

/* ================================================================
 *  Rational  —  GMP Rational Numbers (mpq_t wrapper)
 * ================================================================ */

typedef struct {
    int64_t id;
    mpq_t   val;
} Rational;

Rational* rational_create_str(const char* s) {
    Rational* r = (Rational*)malloc(sizeof(Rational));
    if (!r) return NULL;
    r->id = native_id_alloc();
    mpq_init(r->val);
    mpq_set_str(r->val, s, 10);   /* GMP 精确解析 "num/den" 或 "int" */
    mpq_canonicalize(r->val);
    return r;
}

Rational* rational_create_si(long num, unsigned long den) {
    Rational* r = (Rational*)malloc(sizeof(Rational));
    if (!r) return NULL;
    r->id = native_id_alloc();
    mpq_init(r->val);
    mpq_set_si(r->val, num, den);
    mpq_canonicalize(r->val);
    return r;
}

Rational* rational_from_int(long n) {
    return rational_create_si(n, 1);
}

void rational_destroy(Rational* r) {
    if (r) { mpq_clear(r->val); free(r); }
}

Rational* rational_add(const Rational* a, const Rational* b) {
    if (!a || !b) return NULL;
    Rational* r = (Rational*)malloc(sizeof(Rational));
    if (!r) return NULL;
    r->id = native_id_alloc();
    mpq_init(r->val);
    mpq_add(r->val, a->val, b->val);  /* GMP 精确加法 */
    return r;
}

Rational* rational_sub(const Rational* a, const Rational* b) {
    if (!a || !b) return NULL;
    Rational* r = (Rational*)malloc(sizeof(Rational));
    if (!r) return NULL;
    r->id = native_id_alloc();
    mpq_init(r->val);
    mpq_sub(r->val, a->val, b->val);  /* GMP 精确减法 */
    return r;
}

Rational* rational_mul(const Rational* a, const Rational* b) {
    if (!a || !b) return NULL;
    Rational* r = (Rational*)malloc(sizeof(Rational));
    if (!r) return NULL;
    r->id = native_id_alloc();
    mpq_init(r->val);
    mpq_mul(r->val, a->val, b->val);  /* GMP 精确乘法 */
    return r;
}

Rational* rational_div(const Rational* a, const Rational* b) {
    if (!a || !b || mpq_sgn(b->val) == 0) return NULL;  /* GMP 零检测 */
    Rational* r = (Rational*)malloc(sizeof(Rational));
    if (!r) return NULL;
    r->id = native_id_alloc();
    mpq_init(r->val);
    mpq_div(r->val, a->val, b->val);  /* GMP 精确除法 */
    return r;
}

int rational_cmp(const Rational* a, const Rational* b) {
    if (!a || !b) return 0;
    return mpq_cmp(a->val, b->val);   /* GMP 精确比较 */
}

int rational_eq(const Rational* a, const Rational* b) { return rational_cmp(a, b) == 0; }
int rational_ne(const Rational* a, const Rational* b) { return rational_cmp(a, b) != 0; }
int rational_lt(const Rational* a, const Rational* b) { return rational_cmp(a, b) < 0; }
int rational_le(const Rational* a, const Rational* b) { return rational_cmp(a, b) <= 0; }
int rational_gt(const Rational* a, const Rational* b) { return rational_cmp(a, b) > 0; }
int rational_ge(const Rational* a, const Rational* b) { return rational_cmp(a, b) >= 0; }

int rational_to_string(const Rational* r, char* buf, size_t bufsz) {
    if (!r || !buf || bufsz == 0) return 0;
    char* s = mpq_get_str(NULL, 10, r->val);
    if (!s) return snprintf(buf, bufsz, "(null)");
    int n = snprintf(buf, bufsz, "%s", s);
    free(s);
    return n;
}

/* ================================================================
 *  ConstraintGraph  —  Constraints with GMP rational values
 * ================================================================ */

typedef struct {
    int64_t id;
    mpq_t   value;    /* GMP 精确值 */
    int     pinned;
} GraphNode;

typedef struct {
    int64_t id;
    int32_t from;
    int32_t to;
    mpq_t   weight;   /* GMP 精确权重 */
} GraphEdge;

typedef struct {
    int64_t    id;
    GraphNode* nodes;
    int        node_count;
    int        node_cap;
    GraphEdge* edges;
    int        edge_count;
    int        edge_cap;
} ConstraintGraph;

static void graph_node_clear(GraphNode* n) { if (n) mpq_clear(n->value); }
static void graph_edge_clear(GraphEdge* e) { if (e) mpq_clear(e->weight); }

ConstraintGraph* graph_create(void) {
    ConstraintGraph* g = (ConstraintGraph*)calloc(1, sizeof(ConstraintGraph));
    if (!g) return NULL;
    g->id       = native_id_alloc();
    g->node_cap = 16;
    g->edge_cap = 16;
    g->nodes    = (GraphNode*)calloc(g->node_cap, sizeof(GraphNode));
    g->edges    = (GraphEdge*)calloc(g->edge_cap, sizeof(GraphEdge));
    return g;
}

void graph_destroy(ConstraintGraph* g) {
    if (!g) return;
    for (int i = 0; i < g->node_count; i++) graph_node_clear(&g->nodes[i]);
    for (int i = 0; i < g->edge_count; i++) graph_edge_clear(&g->edges[i]);
    free(g->nodes); free(g->edges); free(g);
}

int64_t graph_add_node(ConstraintGraph* g, const mpq_t value, int pinned) {
    if (!g) return -1;
    if (g->node_count >= g->node_cap) {
        size_t new_cap = g->node_cap * 2;
        GraphNode* tmp = (GraphNode*)realloc(g->nodes, new_cap * sizeof(GraphNode));
        if (!tmp) return -1;               /* realloc 失败, 原内存保留, 安全返回 */
        g->node_cap = (int)new_cap;
        g->nodes = tmp;
    }
    int idx = g->node_count++;
    g->nodes[idx].id = native_id_alloc();
    mpq_init(g->nodes[idx].value);
    mpq_set(g->nodes[idx].value, value);  /* GMP 精确值 */
    g->nodes[idx].pinned = pinned;
    return g->nodes[idx].id;
}

int64_t graph_add_node_si(ConstraintGraph* g, long num, long den, int pinned) {
    mpq_t val; mpq_init(val); mpq_set_si(val, num, den);
    int64_t id = graph_add_node(g, val, pinned);
    mpq_clear(val);
    return id;
}

int graph_remove_node(ConstraintGraph* g, int64_t node_id) {
    if (!g) return -1;
    for (int i = 0; i < g->node_count; i++) {
        if (g->nodes[i].id == node_id) {
            graph_node_clear(&g->nodes[i]);
            mpq_clear(g->nodes[i].value);
            g->nodes[i] = g->nodes[--g->node_count];
            return 0;
        }
    }
    return -1;
}

int64_t graph_add_edge(ConstraintGraph* g, int from_idx, int to_idx, const mpq_t weight) {
    if (!g || from_idx < 0 || to_idx < 0) return -1;
    if (from_idx >= g->node_count || to_idx >= g->node_count) return -1;
    if (g->edge_count >= g->edge_cap) {
        size_t new_cap = g->edge_cap * 2;
        GraphEdge* tmp = (GraphEdge*)realloc(g->edges, new_cap * sizeof(GraphEdge));
        if (!tmp) return -1;               /* realloc 失败, 原内存保留, 安全返回 */
        g->edge_cap = (int)new_cap;
        g->edges = tmp;
    }
    int idx = g->edge_count++;
    g->edges[idx].id = native_id_alloc();
    mpq_init(g->edges[idx].weight);
    mpq_set(g->edges[idx].weight, weight);   /* GMP 精确权重 */
    g->edges[idx].from = from_idx;
    g->edges[idx].to   = to_idx;
    return g->edges[idx].id;
}

int64_t graph_add_edge_si(ConstraintGraph* g, int from, int to, long wnum, long wden) {
    mpq_t w; mpq_init(w); mpq_set_si(w, wnum, wden);
    int64_t id = graph_add_edge(g, from, to, w);
    mpq_clear(w);
    return id;
}

int graph_remove_edge(ConstraintGraph* g, int64_t edge_id) {
    if (!g) return -1;
    for (int i = 0; i < g->edge_count; i++) {
        if (g->edges[i].id == edge_id) {
            graph_edge_clear(&g->edges[i]);
            mpq_clear(g->edges[i].weight);
            g->edges[i] = g->edges[--g->edge_count];
            return 0;
        }
    }
    return -1;
}

const GraphNode* graph_get_node(const ConstraintGraph* g, int index) {
    if (!g || index < 0 || index >= g->node_count) return NULL;
    return &g->nodes[index];
}

const GraphEdge* graph_get_edge(const ConstraintGraph* g, int index) {
    if (!g || index < 0 || index >= g->edge_count) return NULL;
    return &g->edges[index];
}

/* graph_solve: 约束传播 — 使用 GMP 精确有理数比较 */
int graph_solve(ConstraintGraph* g) {
    if (!g || g->node_count == 0) return 0;
    mpq_t eps, propagated, diff;
    mpq_inits(eps, propagated, diff, NULL);
    mpq_set_si(eps, 1, 1000000000000ULL);  /* ε = 10⁻¹² */

    for (int iter = 0; iter < g->node_count * 2; iter++) {
        int changed = 0;
        for (int i = 0; i < g->edge_count; i++) {
            int from = g->edges[i].from;
            int to   = g->edges[i].to;
            if (from >= g->node_count || to >= g->node_count) continue;
            mpq_add(propagated, g->nodes[from].value, g->edges[i].weight); /* GMP 加法 */
            mpq_sub(diff, g->nodes[to].value, propagated);                 /* GMP 减法 */
            if (mpq_sgn(diff) < 0) mpq_neg(diff, diff);                    /* GMP 绝对值 */
            if (!g->nodes[to].pinned && mpq_cmp(diff, eps) > 0) {          /* GMP 比较 */
                mpq_set(g->nodes[to].value, propagated);
                changed = 1;
            }
        }
        if (!changed) break;
    }
    mpq_clears(eps, propagated, diff, NULL);
    return 0;
}

void graph_normalize(ConstraintGraph* g) {
    if (!g || g->node_count == 0) return;
    int min_idx = 0;
    for (int i = 1; i < g->node_count; i++) {
        if (mpq_cmp(g->nodes[i].value, g->nodes[min_idx].value) < 0)  /* GMP 比较 */
            min_idx = i;
    }
    for (int i = 0; i < g->node_count; i++) {
        if (!g->nodes[i].pinned)
            mpq_sub(g->nodes[i].value, g->nodes[i].value, g->nodes[min_idx].value); /* GMP 平移 */
    }
}

ConstraintGraph* graph_clone(const ConstraintGraph* g) {
    if (!g) return NULL;
    ConstraintGraph* ng = (ConstraintGraph*)malloc(sizeof(ConstraintGraph));
    if (!ng) return NULL;
    ng->id         = native_id_alloc();
    ng->node_count = g->node_count; ng->node_cap = g->node_cap;
    ng->edge_count = g->edge_count; ng->edge_cap = g->edge_cap;
    ng->nodes = (GraphNode*)calloc(ng->node_cap, sizeof(GraphNode));
    ng->edges = (GraphEdge*)calloc(ng->edge_cap, sizeof(GraphEdge));
    for (int i = 0; i < ng->node_count; i++) {
        ng->nodes[i].id = g->nodes[i].id;
        mpq_init(ng->nodes[i].value); mpq_set(ng->nodes[i].value, g->nodes[i].value);
        ng->nodes[i].pinned = g->nodes[i].pinned;
    }
    for (int i = 0; i < ng->edge_count; i++) {
        ng->edges[i].id = g->edges[i].id;
        mpq_init(ng->edges[i].weight); mpq_set(ng->edges[i].weight, g->edges[i].weight);
        ng->edges[i].from = g->edges[i].from; ng->edges[i].to = g->edges[i].to;
    }
    return ng;
}

void graph_clear(ConstraintGraph* g) {
    if (!g) return;
    for (int i = 0; i < g->node_count; i++) graph_node_clear(&g->nodes[i]);
    for (int i = 0; i < g->edge_count; i++) graph_edge_clear(&g->edges[i]);
    g->node_count = 0; g->edge_count = 0;
}

int graph_node_count(const ConstraintGraph* g) { return g ? g->node_count : 0; }
int graph_edge_count(const ConstraintGraph* g) { return g ? g->edge_count : 0; }

int graph_find_node(const ConstraintGraph* g, int64_t node_id) {
    if (!g) return -1;
    for (int i = 0; i < g->node_count; i++)
        if (g->nodes[i].id == node_id) return i;
    return -1;
}

int graph_find_edge(const ConstraintGraph* g, int64_t edge_id) {
    if (!g) return -1;
    for (int i = 0; i < g->edge_count; i++)
        if (g->edges[i].id == edge_id) return i;
    return -1;
}

int graph_validate(const ConstraintGraph* g) {
    if (!g) return 0;
    for (int i = 0; i < g->edge_count; i++) {
        if (g->edges[i].from >= g->node_count) return 0;
        if (g->edges[i].to   >= g->node_count) return 0;
    }
    return 1;
}

/* ================================================================
 *  Expr  —  Expression Evaluator (GMP rational)
 * ================================================================ */

typedef struct ExprNode {
    int             kind;  /* 0=const, 1=var, 2=add, 3=sub, 4=mul, 5=div, 6=pow */
    mpq_t           val;   /* GMP 精确值 (if const) */
    char*           name;  /* variable name (if var) */
    struct ExprNode* left;
    struct ExprNode* right;
    int             exp;   /* exponent (if pow) */
} Expr;

static Expr* expr_new_leaf(int kind) {
    Expr* e = (Expr*)calloc(1, sizeof(Expr));
    if (!e) return NULL;
    mpq_init(e->val);
    e->kind = kind;
    return e;
}

Expr* expr_create_const_si(long num, unsigned long den) {
    Expr* e = expr_new_leaf(0);
    if (!e) return NULL;
    mpq_set_si(e->val, num, den);
    return e;
}

Expr* expr_create_var(const char* name) {
    Expr* e = expr_new_leaf(1);
    if (!e) return NULL;
    e->name = strdup(name);
    return e;
}

Expr* expr_create_binop(int kind, Expr* left, Expr* right) {
    Expr* e = expr_new_leaf(kind);
    if (!e) return NULL;
    e->left = left; e->right = right;
    return e;
}

void expr_destroy(Expr* e) {
    if (!e) return;
    Expr* stack[256];
    int top = 0;
    Expr* node = e;
    while (node) {
        Expr* left = node->left;
        Expr* right = node->right;
        mpq_clear(node->val);
        free(node->name);
        free(node);
        if (right && top < 255) { stack[++top] = right; }
        node = left;
        if (!node && top > 0) { node = stack[top--]; }
    }
}

/* expr_eval: 代入 env (var_name → mpq_t*) 计算精确有理数值 */
int expr_eval(mpq_t result, Expr* e, const char** varnames, const mpq_t* values, int nvars) {
    if (!e) return -1;
    mpq_t l, r;
    switch (e->kind) {
    case 0: /* const */
        mpq_set(result, e->val);
        return 0;
    case 1: /* var */
        for (int i = 0; i < nvars; i++) {
            if (strcmp(e->name, varnames[i]) == 0) {
                mpq_set(result, values[i]);
                return 0;
            }
        }
        mpq_set_si(result, 0, 1);
        return -1; /* var not found */
    case 2: /* add */
        mpq_inits(l, r, NULL);
        expr_eval(l, e->left, varnames, values, nvars);
        expr_eval(r, e->right, varnames, values, nvars);
        mpq_add(result, l, r);  /* GMP 精确加法 */
        mpq_clears(l, r, NULL);
        return 0;
    case 3: /* sub */
        mpq_inits(l, r, NULL);
        expr_eval(l, e->left, varnames, values, nvars);
        expr_eval(r, e->right, varnames, values, nvars);
        mpq_sub(result, l, r);  /* GMP 精确减法 */
        mpq_clears(l, r, NULL);
        return 0;
    case 4: /* mul */
        mpq_inits(l, r, NULL);
        expr_eval(l, e->left, varnames, values, nvars);
        expr_eval(r, e->right, varnames, values, nvars);
        mpq_mul(result, l, r);  /* GMP 精确乘法 */
        mpq_clears(l, r, NULL);
        return 0;
    case 5: /* div */
        mpq_inits(l, r, NULL);
        expr_eval(l, e->left, varnames, values, nvars);
        expr_eval(r, e->right, varnames, values, nvars);
        if (mpq_sgn(r) == 0) { mpq_set_si(result, 0, 1); mpq_clears(l, r, NULL); return -2; }
        mpq_div(result, l, r);  /* GMP 精确除法 */
        mpq_clears(l, r, NULL);
        return 0;
    case 6: /* pow */
        if (expr_eval(result, e->left, varnames, values, nvars) < 0) return -1;
        /* 精确有理数幂: result = result^exp */
        {
            mpz_t num, den, base_num, base_den;
            mpz_inits(num, den, base_num, base_den, NULL);
            mpq_get_num(base_num, result); mpq_get_den(base_den, result);
            mpz_set(num, base_num); mpz_set(den, base_den);
            for (int i = 1; i < e->exp; i++) {
                mpz_mul(num, num, base_num);   /* 乘以原始基数, 非平方 */
                mpz_mul(den, den, base_den);
            }
            mpq_set_num(result, num); mpq_set_den(result, den);
            mpz_clears(num, den, base_num, base_den, NULL);
        }
        return 0;
    default:
        mpq_set_si(result, 0, 1);
        return -1;
    }
}

/* expr_compare: 用 GMP 比较两表达式在给定环境下的值 */
int expr_compare(Expr* a, Expr* b, const char** varnames, const mpq_t* values, int nvars) {
    mpq_t va, vb;
    mpq_inits(va, vb, NULL);
    expr_eval(va, a, varnames, values, nvars);
    expr_eval(vb, b, varnames, values, nvars);
    int cmp = mpq_cmp(va, vb);  /* GMP 精确比较 */
    mpq_clears(va, vb, NULL);
    return cmp;
}

/* ================================================================
 *  MemPool  —  Simple Arena Allocator
 * ================================================================ */

typedef struct MemChunk {
    char*   data;
    size_t  used;
    size_t  cap;
    struct MemChunk* next;
} MemChunk;

typedef struct {
    int64_t   id;
    MemChunk* head;
} MemPool;

MemPool* pool_create(void) {
    MemPool* p = (MemPool*)calloc(1, sizeof(MemPool));
    if (!p) return NULL;
    p->id = native_id_alloc();
    p->head = (MemChunk*)calloc(1, sizeof(MemChunk));
    p->head->cap = 65536;  /* 64KB chunk */
    p->head->data = (char*)malloc(p->head->cap);
    return p;
}

void* pool_alloc(MemPool* p, size_t sz) {
    if (!p || !p->head) return NULL;
    if (p->head->used + sz > p->head->cap) {
        MemChunk* c = (MemChunk*)calloc(1, sizeof(MemChunk));
        c->cap = sz > 65536 ? sz : 65536;
        c->data = (char*)malloc(c->cap);
        c->next = p->head;
        p->head = c;
    }
    void* ptr = p->head->data + p->head->used;
    p->head->used += sz;
    return ptr;
}

void pool_reset(MemPool* p) {
    if (!p) return;
    MemChunk* c = p->head;
    while (c) { c->used = 0; c = c->next; }
}

void pool_destroy(MemPool* p) {
    if (!p) return;
    MemChunk* c = p->head;
    while (c) { MemChunk* n = c->next; free(c->data); free(c); c = n; }
    free(p);
}

/* ================================================================
 *  Debug  —  Trace / Breakpoint / Dump
 * ================================================================ */

static int g_debug_level = 0;

void debug_trace(const char* fmt, ...) {
    if (g_debug_level < 1) return;
    va_list ap; va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

void debug_set_level(int lvl) { g_debug_level = lvl; }
int  debug_get_level(void)   { return g_debug_level; }

void debug_breakpoint(void) {
    fprintf(stderr, "[lv00] breakpoint (g_native_id=%lld)\n", (long long)g_native_id);
}

void debug_dump(const char* label, const void* ptr, size_t sz) {
    fprintf(stderr, "[%s] %zu bytes at %p\n", label ? label : "dump", sz, ptr);
}

/* ================================================================
 *  Self-test
 * ================================================================ */

int native_self_test(void) {
    /* Coord GMP test */
    Coord* a = coord_create_si(0, 0);
    Coord* b = coord_create_si(3, 4);
    mpq_t dsq; mpq_init(dsq);
    coord_dist_sq(dsq, a, b);
    /* distance² = 3²+4² = 25 — should be exactly 25 */
    assert(mpq_cmp_si(dsq, 25, 1) == 0);  /* GMP 精确断言 */
    mpq_clear(dsq);
    coord_destroy(a); coord_destroy(b);

    /* Rational GMP test */
    Rational* r1 = rational_create_si(1, 3);
    Rational* r2 = rational_create_si(2, 3);
    Rational* rs = rational_add(r1, r2);
    assert(mpq_cmp_si(rs->val, 1, 1) == 0);  /* 1/3+2/3=1 */
    rational_destroy(r1); rational_destroy(r2); rational_destroy(rs);

    /* ConstraintGraph GMP test */
    ConstraintGraph* g = graph_create();
    mpq_t v; mpq_init(v); mpq_set_si(v, 1, 2);
    int64_t n0 = graph_add_node(g, v, 0);
    mpq_set_si(v, 3, 4);
    int64_t n1 = graph_add_node(g, v, 0);
    assert(g->node_count == 2);
    mpq_clear(v); graph_destroy(g);

    return 0;
}

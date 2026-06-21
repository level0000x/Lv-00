/*
 * stubs.c — Unified Stub Definitions
 *
 * Aggregates all layer-0 implementation files and provides the public
 * export declarations that were previously in stubs.h.
 */

/* Unity build: 将两个实现文件合并编译到一个 translation unit。
   避免 .c 文件被 CMake 重复编译导致 ODR 违规。
   CMakeLists.txt 需确保 lv00_impl_native.c / lv00_impl_upper.c 不单独编译。 */
#include "lv00_impl_native.c"
#include "lv00_impl_upper.c"

/* ================================================================
 *  Coord export declarations (from stubs.h)
 * ================================================================ */

typedef struct Coord Coord;

Coord*  coord_create(const char* x_str, const char* y_str);
void    coord_destroy(Coord* c);
Coord*  coord_dup(const Coord* src);
Coord*  coord_add(const Coord* a, const Coord* b);
Coord*  coord_sub(const Coord* a, const Coord* b);
Coord*  coord_mul(const Coord* a, const mpq_t scalar);
Coord*  coord_div(const Coord* a, const mpq_t scalar);
int     coord_eq(const Coord* a, const Coord* b);
int     coord_ne(const Coord* a, const Coord* b);
int     coord_lt(const Coord* a, const Coord* b);
int     coord_le(const Coord* a, const Coord* b);
int     coord_gt(const Coord* a, const Coord* b);
int     coord_ge(const Coord* a, const Coord* b);
void    coord_dist_sq(mpq_t result, const Coord* a, const Coord* b);
Coord*  coord_midpoint(const Coord* a, const Coord* b);
Coord*  coord_normalize(const Coord* c);
void    coord_dot(mpq_t result, const Coord* a, const Coord* b);
void    coord_cross(mpq_t result, const Coord* a, const Coord* b);
Coord*  coord_rotate(const Coord* c, double angle);           /* [QA] pending GMP: angle→mpq_t */
Coord*  coord_from_polar(double r, double theta);              /* [QA] pending GMP */
int     coord_to_string(const Coord* c, char* buf, size_t bufsz);

/* ================================================================
 *  Rational export declarations (from stubs.h)
 * ================================================================ */

typedef struct Rational Rational;

Rational* rational_create(int64_t num, int64_t den);
void      rational_destroy(Rational* r);
Rational* rational_from_int(int64_t n);
Rational* rational_normalize(Rational* r);
Rational* rational_add(const Rational* a, const Rational* b);
Rational* rational_sub(const Rational* a, const Rational* b);
Rational* rational_mul(const Rational* a, const Rational* b);
Rational* rational_div(const Rational* a, const Rational* b);
int       rational_cmp(const Rational* a, const Rational* b);
int       rational_eq(const Rational* a, const Rational* b);
int       rational_ne(const Rational* a, const Rational* b);
int       rational_lt(const Rational* a, const Rational* b);
int       rational_le(const Rational* a, const Rational* b);
int       rational_gt(const Rational* a, const Rational* b);
int       rational_ge(const Rational* a, const Rational* b);
Rational* rational_abs(const Rational* r);
Rational* rational_neg(const Rational* r);
Rational* rational_inv(const Rational* r);
Rational* rational_pow(const Rational* r, int exp);
int       rational_to_string(const Rational* r, char* buf, size_t bufsz);

/* ================================================================
 *  ConstraintGraph export declarations (from stubs.h)
 * ================================================================ */

typedef struct ConstraintGraph ConstraintGraph;
typedef struct GraphNode       GraphNode;
typedef struct GraphEdge       GraphEdge;

ConstraintGraph*  graph_create(void);
void              graph_destroy(ConstraintGraph* g);
int64_t           graph_add_node(ConstraintGraph* g, const mpq_t value, int pinned);
int64_t           graph_add_node_si(ConstraintGraph* g, long num, long den, int pinned);
int               graph_remove_node(ConstraintGraph* g, int64_t node_id);
int64_t           graph_add_edge(ConstraintGraph* g, int from_idx, int to_idx, const mpq_t weight);
int64_t           graph_add_edge_si(ConstraintGraph* g, int from, int to, long wnum, long wden);
int               graph_remove_edge(ConstraintGraph* g, int64_t edge_id);
const GraphNode*  graph_get_node(const ConstraintGraph* g, int index);
const GraphEdge*  graph_get_edge(const ConstraintGraph* g, int index);
int               graph_solve(ConstraintGraph* g);
void              graph_normalize(ConstraintGraph* g);
ConstraintGraph*  graph_clone(const ConstraintGraph* g);
void              graph_clear(ConstraintGraph* g);
int               graph_node_count(const ConstraintGraph* g);
int               graph_edge_count(const ConstraintGraph* g);
int               graph_find_node(const ConstraintGraph* g, int64_t node_id);
int               graph_find_edge(const ConstraintGraph* g, int64_t edge_id);
int               graph_validate(const ConstraintGraph* g);

/* ================================================================
 *  Expr export declarations (from stubs.h)
 * ================================================================ */

typedef struct Expr Expr;

Expr*  expr_create(const char* s);
void   expr_destroy(Expr* e);
int    expr_eval(mpq_t result, Expr* e, const char** varnames, const mpq_t* values, int nvars);
Expr*  expr_simplify(Expr* e);
Expr*  expr_clone(const Expr* e);
int    expr_is_constant(const Expr* e);
int    expr_is_zero(const Expr* e);
int    expr_is_one(const Expr* e);
Expr*  expr_add(const Expr* a, const Expr* b);
Expr*  expr_sub(const Expr* a, const Expr* b);
Expr*  expr_mul(const Expr* a, const Expr* b);
Expr*  expr_div(const Expr* a, const Expr* b);
int    expr_compare(Expr* a, Expr* b, const char** varnames, const mpq_t* values, int nvars);
int    expr_to_string(const Expr* e, char* buf, size_t bufsz);

/* ================================================================
 *  MemPool export declarations (from stubs.h)
 * ================================================================ */

typedef struct MemPool MemPool;

MemPool* pool_create(size_t initial_size);
void*    pool_alloc(MemPool* p, size_t size);
void*    pool_calloc(MemPool* p, size_t count, size_t sz);
void     pool_free(MemPool* p, void* ptr);
void     pool_reset(MemPool* p);
void     pool_destroy(MemPool* p);
int      pool_stats(const MemPool* p, size_t* total, size_t* used);
int      pool_validate(const MemPool* p);

/* ================================================================
 *  Debug export declarations (from stubs.h)
 * ================================================================ */

void debug_trace(const char* fmt, ...);
void debug_set_level(int level);
int  debug_get_level(void);
int  debug_breakpoint(int id);
void debug_breakpoint_clear(int id);
void debug_dump(const void* data, size_t size);
void debug_assert_fail(const char* expr, const char* file, int line);
void debug_memory_check(void);
void debug_leak_check(void);

/* ================================================================
 *  Native subsystem export declarations (from stubs.h)
 * ================================================================ */

typedef struct NativeStats NativeStats;

int64_t  native_id_current(void);
int64_t  native_id_next(void);
void     native_id_set(int64_t new_id);
int      native_stats(NativeStats* out);
void     native_version(int64_t* major, int64_t* minor, int64_t* patch);
void     native_error(const char* msg);
void     native_warn(const char* msg);
void     native_info(const char* msg);
void     native_config_get(const char* key, char* value, size_t bufsz); /* [QA] pending GMP */
void     native_config_set(const char* key, const char* value);          /* [QA] pending GMP */
int      native_self_test(void);

/* ================================================================
 *  Upper-layer stubs (from lv00_impl_upper.c — declared here)
 * ================================================================ */

/* SMT Theory Combiner */
int  smt_theory_combine(int theory_a, int theory_b, void* result);
void smt_theory_register(int theory_id, void* callback);

/* Bitvector operations */
int64_t  smt_bv_create(int64_t value, int width);
int64_t  smt_bv_add(int64_t a, int64_t b);
int64_t  smt_bv_sub(int64_t a, int64_t b);
int64_t  smt_bv_mul(int64_t a, int64_t b);
int64_t  smt_bv_and(int64_t a, int64_t b);
int64_t  smt_bv_or(int64_t a, int64_t b);
int64_t  smt_bv_xor(int64_t a, int64_t b);
int64_t  smt_bv_not(int64_t a, int width);
int      smt_bv_eq(int64_t a, int64_t b);

/* Trigger engine */
void smt_trigger_install(const char* pattern, void (*handler)(void*));
int  smt_trigger_fire(const char* name, void* arg);
void smt_trigger_suspend(const char* name);
void smt_trigger_resume(const char* name);

/* Proof rule engine */
int  proof_rule_apply(int rule_id, void* goal, void* result);
int  proof_rule_register(int rule_id, const char* name, void* impl);
int  proof_rule_lookup(const char* name);

/* Proof session */
int  proof_session_create(const char* name);
int  proof_session_commit(int session_id);
int  proof_session_rollback(int session_id);
void proof_session_destroy(int session_id);

/* Number theory */
int64_t  nt_gcd(int64_t a, int64_t b);
int64_t  nt_lcm(int64_t a, int64_t b);
int      nt_is_prime(int64_t n);
int64_t  nt_mod_pow(int64_t base, int64_t exp, int64_t mod);

/* Polynomial operations */
int64_t* nt_poly_create(int degree, const int64_t* coeffs);
int64_t  nt_poly_eval(const int64_t* poly, int64_t x);
int64_t* nt_poly_add(const int64_t* a, const int64_t* b);
int64_t* nt_poly_mul(const int64_t* a, const int64_t* b);

/* Symbolic expressions */
int  sym_expr_parse(const char* s, void* out);
int  sym_expr_unify(const void* a, const void* b, void* subst);

/* Rewrite strategies */
int rewrite_apply(const char* rule, void* term, void* result);
int rewrite_compose(const char* r1, const char* r2);

/* Proof versioning */
int64_t proof_version_current(void);
int     proof_version_checkout(int64_t version);

/* Automatic differentiation */
double autodiff_gradient(void (*fn)(double*, double*), double x, double eps);  /* [QA] num-only, acceptable */

/* ODE solver */
double ode_solve_rk4(double (*f)(double, double), double t0, double y0, double t1, int steps);  /* [QA] num-only */

/* Presets — measurements */
double preset_measure_length(const Coord* a, const Coord* b);       /* [QA] pending GMP: use coord_dist_sq */
double preset_measure_area(const Coord* vertices, int n);           /* [QA] pending GMP */
double preset_measure_angle(const Coord* a, const Coord* b, const Coord* c);  /* [QA] pending GMP */

/* Presets — polygons */
int     preset_polygon_is_convex(const Coord* vertices, int n);
double  preset_polygon_area(const Coord* vertices, int n);          /* [QA] pending GMP */
Coord*  preset_polygon_centroid(const Coord* vertices, int n);
int     preset_polygon_contains(const Coord* vertices, int n, const Coord* pt);

/*
 * ================================================================
 *  stubs.c — end
 * ================================================================
 */

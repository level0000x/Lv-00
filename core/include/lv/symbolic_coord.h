#ifndef lv_SYMBOLIC_COORD_H
#define lv_SYMBOLIC_COORD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv/lv_platform.h"
#include <stdbool.h>
#include <stdint.h>
#include <gmp.h>

#include "mpz_poly.h" /* mpz_poly_t, AlgebraicOp */

/* ── Forward decls ── */
typedef struct Rational Rational;
typedef struct Algebraic Algebraic;
typedef struct Quadratic Quadratic;
typedef struct TranscendentalExpr TranscendentalExpr;
typedef struct Transcendental Transcendental;
typedef struct SymbolicCoord SymbolicCoord;

/* Aliases for compatibility */
typedef Algebraic AlgebraExpr;
typedef Algebraic AlgebraicExpr;
typedef Quadratic QuadraticExpr;

/* ── Coord type ── */
typedef enum { RATIONAL = 0, ALGEBRAIC = 1, QUADRATIC = 2, TRANSCENDENTAL = 3 } CoordType;
typedef CoordType SymbolicCoordType;

/* ── Circuit types ── */
typedef enum {
    CIRCUIT_STATUS_OK = 0,
    CIRCUIT_STATUS_TRIPPED = 1,
    CIRCUIT_OK = 0,
    CIRCUIT_OK_STATUS = 0,
    CIRCUIT_FAIL = 1,
} CircuitStatus;

typedef enum {
    CIRCUIT_RESPONSE_IGNORE = 0,
    CIRCUIT_RESPONSE_ROLLBACK = 1,
    CIRCUIT_RESPONSE_DOWNGRADE = 2
} CircuitResponse;

typedef CircuitResponse (*CircuitTripCallback)(const SymbolicCoord *, int, void *);

/* ── Trans expr type ── */
typedef enum {
    TRANS_EXPR_ADD_RATIONAL = 0,
    TRANS_EXPR_MUL_RATIONAL = 1,
    TRANS_EXPR_ADD_ALGEBRAIC = 2,
    TRANS_EXPR_MUL_ALGEBRAIC = 3
} TransExprType;

/* ── Algebraic plan ── */
typedef enum { PLAN_A_FULL_ALGEBRAIC = 0, PLAN_B_QUADRATIC_ONLY = 1, PLAN_C_RATIONAL_ONLY = 2 } AlgebraicPlan;

/* ── Trust color ── */
typedef enum {
    TRUST_GREEN = 0,                  /* 全构造 */
    TRUST_BLUE_UNEXPLORED = 1,        /* 未探索 */
    TRUST_BLUE_EXCEEDED = 2,          /* 资源受限 */
    TRUST_BLUE_OUT_OF_SCOPE = 3,      /* 超出范围 */
    TRUST_YELLOW = 4,                 /* 条件性不可构造 */
    TRUST_LIGHT_ORANGE_ORACLE = 5,    /* 非构造性 oracle（实心端口） */
    TRUST_LIGHT_ORANGE_EXPLOSION = 6, /* 爆炸原理（虚线箭头） */
    TRUST_AMBER = 7,                  /* 数值假设 */
    TRUST_DEEP_ORANGE = 8,            /* 叠加（非构造性+数值假设） */
    TRUST_RED = 9                     /* 矛盾 / 验证伪 */
} TrustColor;

typedef enum {
    LO_NONE = 0,     /* 无子类型 */
    LO_ORACLE = 1,   /* 非构造性 oracle 依赖 */
    LO_EXPLOSION = 2 /* 爆炸原理步骤 */
} LightOrangeSubtype;

/* ── Rational ── */
struct Rational {
    mpq_t value;
};

/* ── Algebraic ── */
struct Algebraic {
    mpz_poly_t minimal_poly;
    double left_bound;
    double right_bound;
    int precision_bits;
    Rational *cached_rational;
};

/* ── Quadratic ── */
struct Quadratic {
    Rational *a;
    Rational *b;
    unsigned int n;
};

/* ── Transcendental ── */
struct TranscendentalExpr {
    TransExprType expr_type;
    char base_name[64];
    Rational *rational_operand;
    bool out_of_scope;
};

struct Transcendental {
    char name[64];
    TranscendentalExpr *expr;
    bool cache_valid;
    double cached_value;
};

/* ── Algebraic info (equiv_class.c 依赖) ── */
typedef struct AlgebraicInfo {
    int degree;
    int coeff_count;
    SymbolicCoord **coefficients;
} AlgebraicInfo;

/* ── Symbolic coordinate ── */
struct SymbolicCoord {
    CoordType type;
    TrustColor trust;
    bool cache_valid;
    double cached_value;
    AlgebraicInfo *algebraic_info; /* v3.5.0: 代数共轭检测 */
    union {
        Rational *rational;
        Algebraic *algebraic;
        Quadratic *quadratic;
        Transcendental *transcendental;
    } data;
};

/* ── Overflow context ── */
struct OverflowContext {
    SymbolicCoord *last_result;
    const char *last_operation;
    CoordType left_type;
    CoordType right_type;
    int overflow_count;
    void *frozen_point;
    bool has_frozen_point;
};

/* ── Stress test ── */
typedef struct {
    bool precision_stable;
    bool performance_stable;
    int max_precision_decay;
    int max_bits_observed;
} StressTestResult;

/* ── Rational functions ── */
Rational *rational_create(int64_t numerator, uint64_t denominator);
Rational *rational_create_from_mpz(const mpz_t numerator, const mpz_t denominator);
Rational *rational_copy(const Rational *src);
void rational_destroy(Rational *r);
int rational_compare(const Rational *a, const Rational *b);
Rational *rational_add(const Rational *a, const Rational *b);
Rational *rational_subtract(const Rational *a, const Rational *b);
Rational *rational_multiply(const Rational *a, const Rational *b);
Rational *rational_divide(const Rational *a, const Rational *b);
char *rational_serialize(const Rational *r);
Rational *rational_parse(const char *str);
double rational_to_double(const Rational *r);

/* ── Algebraic functions ── */
Algebraic *algebraic_create(mpz_poly_t *poly, double left, double right);
void algebraic_destroy(Algebraic *a);
int algebraic_compare(const Algebraic *a, const Algebraic *b);
Algebraic *algebraic_add(const Algebraic *a, const Algebraic *b);
Algebraic *algebraic_subtract(const Algebraic *a, const Algebraic *b);
Algebraic *algebraic_multiply(const Algebraic *a, const Algebraic *b);
Algebraic *algebraic_divide(const Algebraic *a, const Algebraic *b);
bool algebraic_try_rationalize(Algebraic *a);
int algebraic_refine_for_equality(Algebraic *a, Algebraic *b, int max_iterations);
Algebraic *algebraic_from_rational(const Rational *r);
Algebraic *algebraic_from_quadratic(const Quadratic *q);
double algebraic_to_double(const Algebraic *a);

/* ── Quadratic functions ── */
Quadratic *quadratic_create(Rational *a, Rational *b, unsigned int n);
void quadratic_destroy(Quadratic *q);
int quadratic_compare(const Quadratic *a, const Quadratic *b);
Quadratic *quadratic_add(const Quadratic *a, const Quadratic *b);
Quadratic *quadratic_subtract(const Quadratic *a, const Quadratic *b);
Quadratic *quadratic_multiply(const Quadratic *a, const Quadratic *b);
Quadratic *quadratic_divide(const Quadratic *a, const Quadratic *b);
double quadratic_to_double(const Quadratic *q);

/* ── Transcendental functions ── */
Transcendental *transcendental_create(const char *name);
void transcendental_destroy(Transcendental *t);
int transcendental_compare(const Transcendental *a, const Transcendental *b);
char *transcendental_serialize(const Transcendental *t);
double transcendental_to_double(const Transcendental *t);

/* ── SymbolicCoord core ── */
SymbolicCoord *symbolic_coord_create_rational(int64_t num, uint64_t denom);
SymbolicCoord *symbolic_coord_create_quadratic(Rational *a, Rational *b, unsigned int n);
SymbolicCoord *symbolic_coord_create_algebraic(mpz_poly_t *poly, double left, double right);
SymbolicCoord *symbolic_coord_create_transcendental(const char *name);
SymbolicCoord *symbolic_coord_copy(const SymbolicCoord *c);
void symbolic_coord_destroy(SymbolicCoord *c);
void symbolic_coord_invalidate_cache(SymbolicCoord *coord);

/* ── SymbolicCoord arithmetic ── */
SymbolicCoord *symbolic_coord_add(const SymbolicCoord *a, const SymbolicCoord *b);
SymbolicCoord *symbolic_coord_subtract(const SymbolicCoord *a, const SymbolicCoord *b);
SymbolicCoord *symbolic_coord_multiply(const SymbolicCoord *a, const SymbolicCoord *b);
SymbolicCoord *symbolic_coord_divide(const SymbolicCoord *a, const SymbolicCoord *b);
SymbolicCoord *symbolic_coord_negate(const SymbolicCoord *c);
SymbolicCoord *symbolic_coord_sqrt(const SymbolicCoord *c);
SymbolicCoord *symbolic_coord_pow(const SymbolicCoord *base, unsigned int exponent);
SymbolicCoord *symbolic_coord_try_expand_nested_sqrt(const SymbolicCoord *coord);

/* ── SymbolicCoord queries ── */
int symbolic_coord_compare(const SymbolicCoord *a, const SymbolicCoord *b);
bool symbolic_coord_is_zero(const SymbolicCoord *c);
bool symbolic_coord_is_positive(const SymbolicCoord *c);
bool symbolic_coord_is_negative(const SymbolicCoord *c);
bool symbolic_coord_is_amber(const SymbolicCoord *c);
double symbolic_coord_to_double(const SymbolicCoord *c);
char *symbolic_coord_serialize(const SymbolicCoord *c);

/* ── Trust ── */
TrustColor symbolic_coord_get_trust(const SymbolicCoord *c);
void symbolic_coord_set_trust(SymbolicCoord *c, TrustColor t);
SymbolicCoord *symbolic_coord_downgrade_to_amber(const SymbolicCoord *coord, double factor, const char *reason);

/* ── Color combination ── */
TrustColor trust_color_combine(TrustColor a, TrustColor b);

/* ── Hashing ── */
uint64_t symbolic_coord_hash(const SymbolicCoord *c);

/* ── Algebraic plan ── */
AlgebraicPlan algebraic_get_plan(void);
void algebraic_set_plan(AlgebraicPlan plan);

/* ── Plan Manager (A/B 自动降级系统) ── */
void symbolic_coord_set_plan(AlgebraicPlan plan);
AlgebraicPlan symbolic_coord_get_plan(void);
bool symbolic_coord_auto_degrade(const char *reason);
SymbolicCoord *symbolic_coord_create_with_plan(long num, long den);
bool symbolic_coord_is_quadratic_form(const char *expr);
void symbolic_coord_plan_stats(int *out_total, AlgebraicPlan *out_current);

/* ── Stress test ── */
StressTestResult algebraic_stress_test(int chain_length, int max_poly_degree);

/* ── Circuit system ── */
void circuit_set_trip_callback(CircuitTripCallback cb, void *user_data);
CircuitResponse circuit_handle_trip_interactive(const SymbolicCoord *coord);
void circuit_set_context(SymbolicCoord *result, const char *operation, CoordType left_type, CoordType right_type);
SymbolicCoord *circuit_get_last_result(void);
const char *circuit_get_last_operation(void);
bool circuit_has_frozen_point(void);
void *circuit_get_frozen_point(void);
CircuitStatus check_digit_circuit(const SymbolicCoord *coord);
void circuit_handle_overflow(void);
void circuit_reset_context(void);
void circuit_set_frozen_point(void *snapshot);
int circuit_get_overflow_count(void);

#ifdef __cplusplus
}
#endif
#endif

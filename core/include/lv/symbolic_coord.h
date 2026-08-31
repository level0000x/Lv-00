#ifndef lv_SYMBOLIC_COORD_H
#define lv_SYMBOLIC_COORD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv/lv_platform.h"
#include <stdbool.h>
#include <stdint.h>
#include <gmp.h>

#include "config.h"            /* lv_CONFIG_POOL_*（K66 编译期对拍） */
#include "lv/cross_platform.h" /* lv_STATIC_ASSERT */
#include "lv/lv_xmacro.h"
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

/**
 * @brief X-macro 列表：CoordType 枚举值与对应字符串
 */
#define LV_COORD_TYPE_X(x) \
    x(RATIONAL, "Rational") \
    x(ALGEBRAIC, "Algebraic") \
    x(QUADRATIC, "Quadratic") \
    x(TRANSCENDENTAL, "Transcendental")

typedef enum { LV_COORD_TYPE_X(LV_X_ENUM_ITEM) } CoordType;
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

/** @brief 超越数表达式运算语义 */
typedef enum {
    TRANS_OP_UNKNOWN = 0, /**< 未知语义（沿用名称解析回退路径） */
    TRANS_OP_ADD,         /**< 加法：base + 有理数 */
    TRANS_OP_MUL,         /**< 乘法：base * 有理数 */
} TransOpKind;

/**
 * @brief X-macro 列表：TransExprType 枚举值与对应元数据（单一事实源）
 *
 * 列顺序：(symbol, op_str, is_mul, op_kind)
 *   - op_str   序列化运算符字符串
 *   - is_mul   是否为乘法（true=系数*基础常数，false=基础常数+系数）
 *   - op_kind  运算语义（TransOpKind）
 */
#define LV_TRANS_EXPR_TYPE_X(x) \
    x(TRANS_EXPR_ADD_RATIONAL, "+", false, TRANS_OP_ADD) \
    x(TRANS_EXPR_MUL_RATIONAL, "*", true,  TRANS_OP_MUL) \
    x(TRANS_EXPR_ADD_ALGEBRAIC, "+", false, TRANS_OP_UNKNOWN) \
    x(TRANS_EXPR_MUL_ALGEBRAIC, "*", true,  TRANS_OP_UNKNOWN)

#define LV_TRANS_EXPR_ENUM_ITEM(name, op, mul, kind) name,
typedef enum { LV_TRANS_EXPR_TYPE_X(LV_TRANS_EXPR_ENUM_ITEM) } TransExprType;
#undef LV_TRANS_EXPR_ENUM_ITEM

/* ── Algebraic plan ── */
typedef enum { PLAN_A_FULL_ALGEBRAIC = 0, PLAN_B_QUADRATIC_ONLY = 1, PLAN_C_RATIONAL_ONLY = 2 } AlgebraicPlan;

/* ── Trust color ── */

/* TrustColor 枚举与其元数据列表统一由 lv/trust_color_x.h 单源定义
 * （5 列列表：symbol, display_name, serial_name, dot_hex, latex）。
 * 此处仅从主源列表生成枚举，颜色含义详见该头文件。 */
#include "lv/trust_color_x.h"

#define LV_TRUST_ENUM_ITEM(sym, disp, ser, dot, tex) sym,
typedef enum {
    LV_TRUST_COLOR_X(LV_TRUST_ENUM_ITEM)
} TrustColor;
#undef LV_TRUST_ENUM_ITEM

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

/* K66 编译期对拍：SymbolicCoord 增字段超过预设池块尺寸 64 即编译失败 */
lv_STATIC_ASSERT(sizeof(SymbolicCoord) <= lv_CONFIG_POOL_SYMBOLIC_COORD_SIZE,
                 "SymbolicCoord exceeds preset pool block size 64");

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
Rational *rational_negate(const Rational *a);
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

/**
 * @brief 从字符串创建有理数坐标（支持 "3/4"、"-2"、"1.5" 等格式）
 * @param str 字符串表示
 * @return 新建的 SymbolicCoord（有理数类型），解析失败返回 NULL
 */
SymbolicCoord *symbolic_coord_from_string(const char *str);

/**
 * @brief 从 double 创建有理数坐标，按指定比例缩放
 * @param val   double 值
 * @param scale 缩放比例（分母）
 * @return 新建的 SymbolicCoord（有理数类型），失败返回 NULL
 */
SymbolicCoord *symbolic_coord_from_double_scaled(double val, int64_t scale);

/**
 * @brief 从 double 创建有理数坐标，先四舍五入再按指定比例缩放
 * @param val   double 值
 * @param scale 缩放比例（分母）
 * @return 新建的 SymbolicCoord（有理数类型），失败返回 NULL
 */
SymbolicCoord *symbolic_coord_from_double_rounded(double val, int64_t scale);

/**
 * @brief 一次创建一对有理数坐标；任一创建失败时自动回滚已创建项
 *
 * 用于收敛「创建 x/y → 判空 → 逐项回滚 → 使用 → 双销毁」四段式样板：
 * 失败路径由本函数统一回滚，调用点只需判返回值。
 *
 * @param num_x   X 坐标分子
 * @param denom_x X 坐标分母
 * @param num_y   Y 坐标分子
 * @param denom_y Y 坐标分母
 * @param out_x   输出 X 坐标（失败时置 NULL）
 * @param out_y   输出 Y 坐标（失败时置 NULL）
 * @return 全部创建成功返回 true；任一失败返回 false（已自动回滚）
 */
bool symbolic_coord_pair_create_rational(int64_t num_x, uint64_t denom_x,
                                         int64_t num_y, uint64_t denom_y,
                                         SymbolicCoord **out_x, SymbolicCoord **out_y);

/**
 * @brief 一次创建一对按比例缩放的有理数坐标；任一创建失败时自动回滚已创建项
 *
 * @param x      X 坐标原始值
 * @param y      Y 坐标原始值
 * @param scale  缩放比例（分母）
 * @param out_x  输出 X 坐标（失败时置 NULL）
 * @param out_y  输出 Y 坐标（失败时置 NULL）
 * @return 全部创建成功返回 true；任一失败返回 false（已自动回滚）
 */
bool symbolic_coord_pair_from_double_scaled(double x, double y, int64_t scale,
                                            SymbolicCoord **out_x, SymbolicCoord **out_y);
SymbolicCoord *symbolic_coord_create_algebraic(mpz_poly_t *poly, double left, double right);
SymbolicCoord *symbolic_coord_create_transcendental(const char *name);
SymbolicCoord *symbolic_coord_copy(const SymbolicCoord *c);
void symbolic_coord_destroy(SymbolicCoord *c);

/**
 * @brief 一次销毁一对符号坐标（NULL-safe，等价逐分量 symbolic_coord_destroy）
 *
 * 收敛说明（判据 H）：收敛各模块手写的「symbolic_coord_destroy(x);
 * symbolic_coord_destroy(y);」坐标对归还两行样板，与既有 pair 创建 helper
 * （symbolic_coord_pair_create_rational / symbolic_coord_pair_from_double_scaled）
 * 对称，归还侧无需再逐分量展开。
 *
 * @param a 第一个坐标（可为 NULL）
 * @param b 第二个坐标（可为 NULL）
 */
void symbolic_coord_pair_destroy(SymbolicCoord *a, SymbolicCoord *b);
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

/**
 * @brief 判断两个符号坐标是否相等（NULL-safe）
 *
 * 任一参数为 NULL 时返回 false；否则返回 symbolic_coord_compare == 0。
 * 作为各模块手写 coords_equal / graph_coord_equal_for_compatibility
 * 封装的统一收敛入口（propagation.c / graph_node_alloc.c /
 * graph_node_conflict.c 共用同一语义）。
 *
 * @param a 第一个坐标（可为 NULL）
 * @param b 第二个坐标（可为 NULL）
 * @return true 两坐标均存在且符号比较相等
 */
bool symbolic_coord_equal(const SymbolicCoord *a, const SymbolicCoord *b);
bool symbolic_coord_is_zero(const SymbolicCoord *c);
bool symbolic_coord_is_positive(const SymbolicCoord *c);
bool symbolic_coord_is_negative(const SymbolicCoord *c);
bool symbolic_coord_is_amber(const SymbolicCoord *c);

/* ── 符号几何判定（公共收敛入口） ── */

/**
 * @brief 符号精确判定三点共线
 *
 * 计算符号叉积 (B-A)×(C-A) = (bx-ax)*(cy-ay) - (by-ay)*(cx-ax)，
 * 结果为零则三点共线。NULL-safe：任一参数为 NULL 返回 false。
 *
 * 收敛说明：euclidean_geometry_helpers.c 的 symbolic_check_collinear 与
 * proof_strategy_vector.c 的共线/平行叉积检查共用本实现（证明策略与
 * 断言行为一致）。浮点域判定请使用 geo_predicate.c 的 lv_orientation_2d
 * （不同精度域，语义独立，此处不做收敛）。
 */
bool symbolic_coord_are_collinear(const SymbolicCoord *ax, const SymbolicCoord *ay, const SymbolicCoord *bx,
                                  const SymbolicCoord *by, const SymbolicCoord *cx, const SymbolicCoord *cy);
double symbolic_coord_to_double(const SymbolicCoord *c);
char *symbolic_coord_serialize(const SymbolicCoord *c);

/**
 * @brief 从符号坐标数组提取前两个坐标到 (x, y)（double 近似）
 *
 * 收敛说明（批次 Q 组⑦ Q19，判据 H）：收敛各模块手写的
 * 「coord_count < 2 守卫 + 数组/分量 NULL 守卫 + 逐分量 to_double」
 * 点坐标提取样板。
 *
 * @param coords      符号坐标数组（可为 NULL）
 * @param coord_count 数组长度
 * @param x           输出 X（成功时写入，失败不变）
 * @param y           输出 Y（成功时写入，失败不变）
 * @return 数组有效且前两个坐标非 NULL 时返回 true，否则 false
 */
bool symbolic_coord_get_xy(const SymbolicCoord *const *coords, int coord_count, double *x, double *y);

/**
 * @brief 从符号坐标数组提取前四个坐标到线段端点 (x1,y1,x2,y2)（double 近似）
 *
 * 收敛说明（批次 Q 组⑦ Q19，判据 H）：收敛各模块手写的
 * 「coord_count < 4 守卫 + 逐分量 to_double」线段端点提取样板。
 *
 * @param coords      符号坐标数组（可为 NULL）
 * @param coord_count 数组长度
 * @param x1/y1/x2/y2 输出（成功时写入，失败不变）
 * @return 数组有效且前四个坐标非 NULL 时返回 true，否则 false
 */
bool symbolic_coord_get_segment(const SymbolicCoord *const *coords, int coord_count, double *x1, double *y1, double *x2,
                                double *y2);

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

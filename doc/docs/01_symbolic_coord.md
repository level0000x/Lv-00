# 符号坐标系统 (Symbolic Coordinate System)

## 模块概述

符号坐标系统为 Lv-00 的几何层提供**精确**的实数表示与运算能力。它支持有理数、二次根式、代数数与超越常数的统一建模，是约束图中点、线段等几何对象坐标（`GeomNode.symbolic_coords`）的底层数值地基。系统包含四类坐标原语 `CoordType`（RATIONAL / QUADRATIC / ALGEBRAIC / TRANSCENDENTAL）、一套无外部依赖的"三层数域 + 多项式"精确算术（`algebraic_number.h`）、基于 GMP 的精确有理数封装（`rational.h`）、统一数值句柄 `lvNumber`（`lv_number.h`），以及模运算 / 素数判定 / 因式分解等数论工具（`nt_number_theory.h`）。

## 核心设计原则

1. **精确优先**：所有符号运算保持数学精确性，不引入浮点误差；浮点仅在"转 double 近似"与"隔离区间端点"处出现。
2. **分层数域递进**：数域按 Q → Q(√d) → 代数数（最小多项式 + 隔离区间）递进；`AlgebraicPlan` 提供 A（全代数数）/ B（仅二次）/ C（仅有理）三级策略，资源受限时 `symbolic_coord_auto_degrade` 自动降级。
3. **信任颜色伴随传播**：每个坐标携带 `TrustColor`（GREEN / BLUE_* / YELLOW / LIGHT_ORANGE_* / AMBER / DEEP_ORANGE / RED），运算结果由 `trust_color_combine` 组合产生。
4. **熔断保护**：代数链过长或精度衰减时触发 `CircuitTripCallback` 熔断，支持忽略 / 回滚 / 降级三档响应，并冻结现场（`OverflowContext`）。
5. **缓存与失效**：`cached_value` 缓存 double 近似，`symbolic_coord_invalidate_cache` 显式失效，避免缓存不一致。
6. **溢出防护**：int64 路径全程检测溢出（`lv_safe_pow` 等）；GMP 路径提供 `lv_rational_mul_is_safe` / `lv_rational_den_is_safe` 防分母膨胀。

## 关键数据结构

```c
/* 坐标类型：由 LV_COORD_TYPE_X X-macro 生成 */
typedef enum { RATIONAL, ALGEBRAIC, QUADRATIC, TRANSCENDENTAL } CoordType;

/* 有理数：GMP 精确有理数（symbolic_coord.h 与 rational.h 同构） */
struct Rational { mpq_t value; };
typedef struct lvRational { mpq_t value; } lvRational;

/* 代数数：最小多项式 + 隔离区间 */
struct Algebraic {
    mpz_poly_t minimal_poly;
    double left_bound;      /* 隔离区间下界 */
    double right_bound;     /* 隔离区间上界 */
    int precision_bits;
    Rational *cached_rational;
};

/* 二次根式：a + b*sqrt(n) */
struct Quadratic { Rational *a; Rational *b; unsigned int n; };

/* 超越常数：命名常量 + 有限变换表达式 */
struct TranscendentalExpr {
    TransExprType expr_type;         /* ADD/MUL_RATIONAL, ADD/MUL_ALGEBRAIC */
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

/* 符号坐标：tagged union 的顶层句柄 */
struct SymbolicCoord {
    CoordType type;
    TrustColor trust;
    bool cache_valid;
    double cached_value;
    AlgebraicInfo *algebraic_info;   /* v3.5.0: 代数共轭检测 */
    union {
        Rational *rational;
        Algebraic *algebraic;
        Quadratic *quadratic;
        Transcendental *transcendental;
    } data;
};

/* 无外部依赖三层数域（algebraic_number.h）：Q / Q(sqrt(d)) / 区间 */
typedef struct { int64_t num; int64_t den; } AlgRational;  /* 自动约分 */
typedef struct { AlgRational a; AlgRational b; int64_t d; } AlgQuadratic;
typedef struct { AlgRational lo; AlgRational hi; } AlgInterval;
typedef struct { int64_t coef[lv_alg_poly_MAX_DEGREE + 1]; int degree; } AlgPoly;

/* 统一数值句柄：vtable 多态（lv_number.h） */
typedef struct lvNumberOps {
    lvNumber *(*add)(const lvNumber *, const lvNumber *);
    lvNumber *(*sub)(const lvNumber *, const lvNumber *);
    lvNumber *(*mul)(const lvNumber *, const lvNumber *);
    lvNumber *(*div)(const lvNumber *, const lvNumber *);
    int (*compare)(const lvNumber *, const lvNumber *);
    double (*to_double)(const lvNumber *);
    uint64_t (*hash)(const lvNumber *);
    char *(*to_string)(const lvNumber *);
    bool (*is_zero)(const lvNumber *);
    bool (*is_one)(const lvNumber *);
    bool (*is_negative)(const lvNumber *);
    lvNumber *(*clone)(const lvNumber *);
    void (*destroy)(lvNumber *);
    lvNumberType (*type)(const lvNumber *);
} lvNumberOps;
typedef struct lvNumber { const lvNumberOps *ops; void *impl; } lvNumber;
```

## 主要接口

| 分组 | 函数签名 | 说明 |
|------|----------|------|
| 坐标工厂 | `SymbolicCoord *symbolic_coord_create_rational(int64_t, uint64_t)` | 有理数坐标 |
| 坐标工厂 | `SymbolicCoord *symbolic_coord_create_quadratic(Rational*, Rational*, unsigned)` | 二次根式坐标 |
| 坐标工厂 | `SymbolicCoord *symbolic_coord_create_algebraic(mpz_poly_t*, double, double)` | 代数数坐标（最小多项式 + 隔离区间） |
| 坐标工厂 | `SymbolicCoord *symbolic_coord_create_transcendental(const char*)` | 超越常数坐标 |
| 坐标工厂 | `SymbolicCoord *symbolic_coord_from_double_scaled(double, int64_t)` | double 按比例转有理数 |
| 坐标算术 | `symbolic_coord_add/subtract/multiply/divide/negate` | 四则运算 |
| 坐标算术 | `SymbolicCoord *symbolic_coord_sqrt(const SymbolicCoord*)` | 开方（进入二次/代数域） |
| 坐标算术 | `SymbolicCoord *symbolic_coord_pow(const SymbolicCoord*, unsigned)` | 整数幂 |
| 坐标算术 | `SymbolicCoord *symbolic_coord_try_expand_nested_sqrt(...)` | 嵌套根式展开 |
| 判定/比较 | `int symbolic_coord_compare(const SymbolicCoord*, const SymbolicCoord*)` | 精确比较 |
| 判定/比较 | `bool symbolic_coord_equal(const SymbolicCoord*, const SymbolicCoord*)` | NULL-safe 相等判断 |
| 判定/比较 | `bool symbolic_coord_are_collinear(...6 个坐标...)` | 符号精确三点共线判定 |
| 信任 | `TrustColor symbolic_coord_get_trust / set_trust` | 信任颜色读写 |
| 信任 | `SymbolicCoord *symbolic_coord_downgrade_to_amber(...)` | 降级为数值假设 |
| 信任 | `TrustColor trust_color_combine(TrustColor, TrustColor)` | 颜色组合 |
| 哈希/序列化 | `uint64_t symbolic_coord_hash(const SymbolicCoord*)` | 确定性哈希 |
| 哈希/序列化 | `char *symbolic_coord_serialize(const SymbolicCoord*)` | 序列化 |
| 计划管理 | `bool symbolic_coord_auto_degrade(const char *reason)` | A/B/C 自动降级 |
| 熔断 | `void circuit_set_trip_callback(CircuitTripCallback, void*)` | 注册熔断回调 |
| 熔断 | `CircuitStatus check_digit_circuit(const SymbolicCoord*)` | 精度电路检查 |
| 有理数层 | `lvRational *lv_rational_add/sub/mul/div/neg/inv/abs(...)` | GMP 精确运算 |
| 有理数层 | `bool lv_rational_to_double(const lvRational*, double*, int*)` | 带损失位的转换 |
| 数域层 | `AlgRational lv_alg_rational_create(int64_t p, int64_t q, AlgRationalError*)` | Q 层约分创建 |
| 数域层 | `AlgQuadratic lv_alg_quadratic_mul/div/conj/norm(...)` | Q(√d) 层运算 |
| 数域层 | `AlgInterval lv_alg_interval_from_quadratic(...)` | 二次数 → 隔离区间 |
| 数域层 | `AlgPoly lv_alg_poly_rational_roots(...)` | 有理根定理求根 |
| 统一句柄 | `lvNumber *lv_number_from_rational/double/int/string(...)` | lvNumber 工厂 |
| 统一句柄 | `int lv_number_compare / bool lv_number_eq/lt/gt/...` | 句柄比较 |
| 数论 | `int nt_is_prime_miller_rabin(const mpz_t n, int k)` | Miller-Rabin 素性检测 |
| 数论 | `int nt_factorize_trial_div(const mpz_t, mpz_t*, int, const mpz_t)` | 试除因式分解 |
| 溢出安全 | `bool lv_safe_pow(int64_t a, int64_t b, int64_t *result)` | 溢出检测幂运算 |

## 工作流程

1. **创建**：根据几何构造调用 `symbolic_coord_create_rational` 或从 double 用 `symbolic_coord_from_double_scaled` 转入，必要时升级为 `create_quadratic` / `create_algebraic` / `create_transcendental`。
2. **运算**：`symbolic_coord_add/mul/sqrt/pow` 等按 `AlgebraicPlan` 分派到对应数域层；int64 路径经 `lv_alg_*`（含溢出检测），大整数路径经 GMP `lvRational` / `lvNumber`。
3. **判定**：几何谓词（如共线）走 `symbolic_coord_are_collinear` 精确判定；不可判定时使用隔离区间二分（`lv_alg_interval_bisect`）加细。
4. **信任传播**：结果 `trust` 由 `trust_color_combine` 合并；数值假设场景经 `symbolic_coord_downgrade_to_amber` 标记 AMBER。
5. **保护**：运算前 `check_digit_circuit` 检查精度电路；`circuit_handle_trip_interactive` 按回调决定忽略 / 回滚 / 降级；`algebraic_refine_for_equality` 在相等比较前加细隔离区间。
6. **序列化**：`symbolic_coord_serialize` 输出文本，`rational_parse` / `lv_rational_from_string` 反向还原，供 JSON / DOT / 存储层使用。

## 模块关系

| 模块 | 依赖方向 | 关系说明 |
|------|----------|----------|
| [02_constraint_graph.md](02_constraint_graph.md) | 被依赖 | `GeomNode.symbolic_coords` 与 `Constraint.numeric_value` 直接消费本系统 |
| [04_solver.md](04_solver.md) | 被依赖 | 求解器依赖精确坐标比较与隔离区间判定 |
| [06_unify.md](06_unify.md) | 被依赖 | 合一过程依赖 `symbolic_coord_equal` 的精确相等语义 |
| [24_constraint_propagation.md](24_constraint_propagation.md) | 被依赖 | 传播阶段使用坐标相等/共线判定收敛谓词 |
| [28_number_theory.md](28_number_theory.md) | 依赖 | 消费 `nt_number_theory.h` 的模运算与素性检测 |
| [29_inequality_approximation.md](29_inequality_approximation.md) | 被依赖 | 不等式近似复用区间运算基础设施 |
| [07_func_block.md](07_func_block.md) | 被依赖 | 函数块实例化时重建参数坐标 |
| [26_interactive_geometry.md](26_interactive_geometry.md) | 被依赖 | 交互编辑经 `from_double_scaled/rounded` 注入坐标 |

## 版本历史

| 版本 | 日期 | 变更 |
|------|------|------|
| v1.0 | 2026-08 | 首版：覆盖 CoordType 四原语、信任颜色、A/B/C 降级计划、熔断系统 |
| v1.1.0 | 2026-08 | 对齐 `algebraic_number.h` 三层数域 + 多项式体系；`lv_` 前缀公共符号统一；`AlgebraicInfo` 代数共轭检测 |

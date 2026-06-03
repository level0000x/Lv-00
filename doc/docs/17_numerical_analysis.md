# 数值计算与代数分析模块 (Numerical Computation and Algebraic Analysis Modules)

## 模块概述

数值计算与代数分析模块为 Lv-00 提供从基础区间算术到高层数论算法的完整数值计算能力。模块涵盖区间算术、浮点误差验证、ODE 求解、自动微分、多后端数值抽象、精确有理数运算、不等式推理、概率约束、数论算法、多项式算术和稀疏线性代数等维度，共同构成 Lv-00 的数值基础设施层。

该模块的设计遵循"精确优先、近似标注"原则：符号计算和精确算术（有理数、数论、多项式）保证零舍入误差，浮点运算和数值求解通过信任颜色系统标注精度损失，区间算术为两者提供严格的包含性桥梁。

## 核心设计原则

1. **包含性保证**：所有区间运算保证真实结果在计算区间内（conservativeness），遵循 IEEE 1788 标准
2. **精确优先**：有理数运算基于 GMP mpz_t，无浮点舍入；数论算法使用多精度整数
3. **多后端抽象**：数值后端借鉴 SUNDIALS N_Vector/SUNMatrix 架构，支持 SERIAL/OpenMP/CUDA/HIP 运行时切换
4. **信任颜色标注**：浮点误差分析结果映射到信任颜色系统，确保数值步骤可被证明链追踪
5. **桩实现策略**：不等式推理和概率约束为桩实现，预留完整接口和设计规范

## 1. interval_arithmetic.h —— 统一区间算术

### 设计借鉴

提供独立的区间算术模块，融合 MPFI 和 FLINT/Arb 的设计理念。使用 double 基础区间，无外部库依赖即可执行基本运算。所有区间运算保证包含性：真实结果始终在计算区间内。

### 区间结构

```c
typedef struct {
    double lo;       /**< 下界 */
    double hi;       /**< 上界 */
    int is_exact;    /**< 非零表示精确值（lo == hi 且无舍入误差） */
} Lv00Interval;
```

当 lo > hi 时区间为空；当 lo == hi 且 is_exact 为真时，区间表示单个精确值。

### 工厂函数

```c
Lv00Interval interval_create(double lo, double hi, int is_exact);
Lv00Interval interval_point(double val);     /**< 精确点值 */
Lv00Interval interval_empty(void);           /**< 空区间 */
Lv00Interval interval_entire(void);          /**< 全实数 (-inf, +inf) */
```

### 算术运算

| 运算 | API | 语义 |
|------|-----|------|
| 加法 | `interval_add(a, b)` | [a.lo+b.lo, a.hi+b.hi] |
| 减法 | `interval_sub(a, b)` | [a.lo-b.hi, a.hi-b.lo] |
| 乘法 | `interval_mul(a, b)` | [min(S), max(S)]，S = {ac, ad, bc, bd} |
| 除法 | `interval_div(a, b)` | 0 ∈ [c,d] 时返回空区间 |
| 平方根 | `interval_sqrt(a)` | 要求 lo >= 0 |
| 三角函数 | `interval_sin/cos(a)` | 处理非单调性（检查临界点） |
| 指数/对数 | `interval_exp/log(a)` | log 要求 lo > 0 |
| 绝对值 | `interval_abs(a)` | |[lo, hi]| |
| 取反 | `interval_neg(a)` | [-hi, -lo] |

### 性质查询

```c
double interval_diam(Lv00Interval a);          /**< 直径：hi - lo */
double interval_mid(Lv00Interval a);           /**< 中点：(lo + hi) / 2 */
int interval_is_empty(Lv00Interval a);         /**< 是否为空 */
int interval_contains(Lv00Interval a, double val); /**< 是否包含值 */
int interval_is_subset(Lv00Interval a, Lv00Interval b); /**< 子集判断 */
int interval_equals(Lv00Interval a, Lv00Interval b);    /**< 相等判断 */
```

### 集合运算与符号坐标集成

```c
Lv00Interval interval_intersect(Lv00Interval a, Lv00Interval b);  /**< 交集 */
Lv00Interval interval_union(Lv00Interval a, Lv00Interval b);     /**< 凸包 */

/* 符号坐标集成 */
Lv00Interval interval_from_symbolic(const char *expr_str,
                                     const char **var_names,
                                     const Lv00Interval *var_intervals,
                                     int var_count);
int interval_to_symbolic(Lv00Interval a, char *buf, size_t buf_size);
```

### 解验证

```c
/* 验证解是否满足约束（0 ∈ f_interval） */
int interval_verify_solution(Lv00Interval f_interval, double tolerance);

/* 自适应二分验证 */
int interval_verify_adaptive(const char *expr_str, const char **var_names,
                             Lv00Interval *var_intervals, int var_count,
                             int max_depth, double tolerance);
```

## 2. float_error.h —— FPTaylor 风格浮点误差验证

### 设计借鉴

借鉴 FPTaylor（FPBench 项目）的浮点误差分析方法论，为 Lv-00 几何约束系统提供严格的浮点误差验证。核心思路：将几何约束表达式转换为浮点表达式，进行泰勒展开，用区间算术做误差传播分析，输出绝对/相对误差界并映射到信任颜色。

### 泰勒形式

```c
typedef struct {
    double center_val;          /**< 中心点函数值 f(center) */
    double (*first_derivs);     /**< 一阶偏导数数组 df/dxi|center */
    int *deriv_var_ids;         /**< 导数对应的变量 ID */
    int deriv_count;
    double interval_lo;         /**< 区间下界（含泰勒余项） */
    double interval_hi;         /**< 区间上界（含泰勒余项） */
    int order;                  /**< 泰勒展开阶数 */
} TaylorForm;
```

### 浮点区间

```c
typedef struct {
    double lo;     /**< 区间下界 */
    double hi;     /**< 区间上界 */
    bool is_exact; /**< 是否精确值 */
} FloatInterval;
```

区间算术遵循 IEEE 1788 标准，提供加、减、乘、除、sqrt、sin、cos、exp、log 等操作的保守区间估计。

### 误差界与信任颜色

```c
typedef struct {
    double absolute_error;  /**< 绝对误差上界 */
    double relative_error;  /**< 相对误差上界 */
    TrustColor trust_level; /**< 信任颜色等级 */
    char *proof_text;       /**< 误差证明文本 */
} ErrorBound;
```

信任颜色映射规则：

| 绝对误差范围 | TrustColor | 含义 |
|-------------|-----------|------|
| <= 1e-12 | TRUST_GREEN | 高精度安全 |
| <= 1e-10 | TRUST_BLUE | 一般安全 |
| <= tolerance | TRUST_AMBER | 边界安全（含数值假设） |
| > tolerance | TRUST_RED | 不安全，已证伪 |
| 评估失败 | TRUST_YELLOW | 无法确定 |

### 评估配置

```c
typedef struct {
    bool use_optimization;         /**< 启用优化模式（减少误差过估） */
    int taylor_order;              /**< 泰勒展开阶数（1-3） */
    bool use_z3_opt;               /**< 使用 Z3 优化后端 */
    bool use_gelpia;               /**< 使用 GELPIA 多面体区间算术 */
    double branch_bound_threshold; /**< 分支切割阈值 */
} FPTaylorConfig;
```

### 核心 API

```c
/* 对浮点表达式进行误差评估 */
bool fptaylor_evaluate_expr(const char *expr, const FloatInterval *var_bounds,
                            int var_count, const FPTaylorConfig *cfg, ErrorBound *out);

/* 对约束图中指定变量进行浮点误差分析 */
bool fptaylor_evaluate_graph(const ConstraintGraph *graph, int var_id,
                             const FPTaylorConfig *cfg, ErrorBound *out);

/* 验证误差界是否在安全容差范围内 */
TrustColor fptaylor_verify_safety(const ErrorBound *bound, double tolerance);
```

`fptaylor_evaluate_graph` 是 Lv-00 独有的函数，将 FPTaylor 方法论应用于约束图：提取涉及目标变量的约束方程，转换为浮点表达式，进行泰勒展开分析，用区间算术聚合所有约束的误差贡献。

## 3. ode_solver.h —— ODE 求解器

### 设计借鉴

提供常微分方程初值问题 dy/dt = f(t, y) 的数值求解方法。借鉴 DifferentialEquations.jl、SUNDIALS (CVODE) 和 IPOPT 的设计。

### 求解方法枚举

```c
typedef enum Lv00ODEMethod {
    ODE_EULER = 0,   /**< 显式前向 Euler 方法（一阶） */
    ODE_RK4   = 1,   /**< 经典四阶 Runge-Kutta */
    ODE_ADAMS = 2    /**< Adams-Bashforth 多步法（桩实现） */
} Lv00ODEMethod;
```

### ODE 问题定义

```c
/* 右端函数签名：dy/dt = rhs(t, y, params) */
typedef void (*Lv00ODERhsFn)(double t, const double *y, void *params, double *dydt);

typedef struct Lv00ODEProblem {
    Lv00ODERhsFn rhs_fn;    /**< 右端函数 */
    double         *y0;     /**< 初始状态向量 */
    size_t          dim;    /**< 状态向量维度 */
    double          t_span[2]; /**< 积分区间 [t_start, t_end] */
    void           *params; /**< 用户参数 */
} Lv00ODEProblem;
```

### 求解器配置

```c
typedef struct Lv00ODEConfig {
    Lv00ODEMethod method;    /**< 积分方法 */
    double        dt;        /**< 固定时间步长 */
    size_t        max_steps; /**< 最大步数 */
    double        rtol;      /**< 相对容差（自适应，预留） */
    double        atol;      /**< 绝对容差（自适应，预留） */
} Lv00ODEConfig;
```

### 求解 API

```c
/* 求解 ODE 初值问题 */
LV00_PUBLIC_API Lv00ODESolution *ode_solve(const Lv00ODEProblem *problem,
                                           const Lv00ODEConfig *config);

/* 销毁解 */
LV00_PUBLIC_API void ode_solution_destroy(Lv00ODESolution *sol);
```

解以轨迹形式存储：

```c
typedef struct Lv00ODESolution {
    double *t_values;  /**< 时间点（长度 n_steps） */
    double *y_values;  /**< 状态向量（长度 n_steps * dim，行主序） */
    size_t  n_steps;   /**< 时间步数 */
    size_t  dim;       /**< 状态向量维度 */
} Lv00ODESolution;
```

## 4. autodiff.h —— 自动微分引擎

### 设计借鉴

提供轻量级自动微分系统，支持前向模式和反向模式（反向传播）。借鉴 Enzyme (LLVM-based AD) 和 PyTorch autograd 的设计。

### AD 模式

```c
typedef enum Lv00ADMode {
    AD_FORWARD = 0,  /**< 前向模式（切线传播），适合少输入多输出 */
    AD_REVERSE = 1   /**< 反向模式（梯度反向传播），适合多输入单输出 */
} Lv00ADMode;
```

### 表达式节点类型

```c
typedef enum Lv00ADExprKind {
    AD_CONST = 0,   /**< 常量 */
    AD_VAR   = 1,   /**< 变量（通过 var_index 标识） */
    AD_ADD   = 2,   /**< 加法：children[0] + children[1] */
    AD_MUL   = 3,   /**< 乘法：children[0] * children[1] */
    AD_NEG   = 4,   /**< 取反：-children[0] */
    AD_SIN   = 5,   /**< 正弦：sin(children[0]) */
    AD_COS   = 6,   /**< 余弦：cos(children[0]) */
    AD_POW   = 7    /**< 幂：children[0] ^ children[1] */
} Lv00ADExprKind;
```

### 表达式节点

```c
typedef struct Lv00ADExpr {
    Lv00ADExprKind   kind;         /**< 节点类型 */
    double           value;        /**< 原始值（求值时设置） */
    int              var_index;    /**< 变量索引（仅 AD_VAR） */
    struct Lv00ADExpr **children;  /**< 子节点 */
    size_t           child_count;  /**< 子节点数量 */
    double           gradient;     /**< 累积梯度（反向模式使用） */
} Lv00ADExpr;
```

### 表达式构造

```c
Lv00ADExpr *ad_expr_create_const(double value);
Lv00ADExpr *ad_expr_create_var(int var_index);
Lv00ADExpr *ad_expr_add(Lv00ADExpr *a, Lv00ADExpr *b);
Lv00ADExpr *ad_expr_mul(Lv00ADExpr *a, Lv00ADExpr *b);
Lv00ADExpr *ad_expr_sin(Lv00ADExpr *x);
Lv00ADExpr *ad_expr_cos(Lv00ADExpr *x);
Lv00ADExpr *ad_expr_pow(Lv00ADExpr *base, Lv00ADExpr *exponent);
void ad_expr_destroy(Lv00ADExpr *expr);
```

### 微分 API

```c
/* 前向模式：同时计算 f(x) 和 f'(x) */
bool ad_forward_diff(Lv00ADExpr *expr, int var_index,
                     double var_value, double *value, double *derivative);

/* 反向模式：计算所有变量的梯度 */
bool ad_reverse_diff(Lv00ADExpr *expr, const double *var_values, size_t var_count,
                     double *value, double *gradients);

/* 表达式求值 */
bool ad_eval(Lv00ADExpr *expr, const double *var_values, size_t var_count, double *result);

/* 查询梯度（反向微分后调用） */
double ad_grad(Lv00ADExpr *expr, int var_index);
```

## 5. numerical_backend.h —— 多后端数值抽象层

### 设计借鉴

借鉴 SUNDIALS (github.com/LLNL/sundials) 的 N_Vector/SUNMatrix/SUNLinearSolver 三层抽象架构。通过函数指针操作表模式允许编译时/运行时切换后端，支持 SERIAL、OpenMP、CUDA、HIP 和自定义后端。

### 后端类型

```c
typedef enum {
    LV00_BACKEND_SERIAL = 0, /**< 串行 CPU（默认实现） */
    LV00_BACKEND_OPENMP = 1, /**< OpenMP 多核 CPU 并行 */
    LV00_BACKEND_CUDA = 2,   /**< NVIDIA CUDA GPU */
    LV00_BACKEND_HIP = 3,    /**< AMD HIP GPU（ROCm 平台） */
    LV00_BACKEND_CUSTOM = 99 /**< 用户自定义后端 */
} Lv00BackendType;
```

### 向量抽象（N_Vector 风格）

```c
struct Lv00VectorOps {
    Lv00Vector *(*clone)(const Lv00Vector *v);
    void (*destroy)(Lv00Vector *v);
    void (*zero)(Lv00Vector *v);
    void (*const_set)(Lv00Vector *v, double c);
    void (*copy)(Lv00Vector *dst, const Lv00Vector *src);
    void (*scale)(Lv00Vector *v, double c);
    void (*linear_sum)(double a, const Lv00Vector *x, double b, const Lv00Vector *y, Lv00Vector *z);
    double (*dot)(const Lv00Vector *x, const Lv00Vector *y);
    double (*norm)(const Lv00Vector *v);
    double (*max_norm)(const Lv00Vector *v);
    double (*wrms_norm)(const Lv00Vector *v, const Lv00Vector *weights);
    void (*abs)(Lv00Vector *v);
    void (*inv)(Lv00Vector *v, const Lv00Vector *d);
    int64_t (*length)(const Lv00Vector *v);
    double *(*data_ptr)(Lv00Vector *v);
};

struct Lv00Vector {
    int64_t length;
    Lv00BackendType backend;
    double (*data);           /**< 数据数组（序列后端直接使用） */
    void *backend_data;       /**< 后端私有数据（GPU 指针等） */
    const Lv00VectorOps *ops; /**< 操作表 */
};
```

### 矩阵抽象（SUNMatrix 风格）

```c
typedef enum {
    LV00_MATRIX_DENSE = 0,      /**< 稠密矩阵（列主序） */
    LV00_MATRIX_SPARSE_CSR = 1, /**< CSR 格式稀疏矩阵 */
    LV00_MATRIX_SPARSE_CSC = 2, /**< CSC 格式稀疏矩阵 */
    LV00_MATRIX_BANDED = 3,     /**< 带状矩阵 */
    LV00_MATRIX_CUSTOM = 4      /**< 自定义格式 */
} Lv00MatrixFormat;

struct Lv00MatrixOps {
    Lv00Matrix *(*clone)(const Lv00Matrix *A);
    void (*destroy)(Lv00Matrix *A);
    void (*zero)(Lv00Matrix *A);
    void (*copy)(Lv00Matrix *dst, const Lv00Matrix *src);
    int (*matvec)(const Lv00Matrix *A, const Lv00Vector *x, Lv00Vector *y);
    void (*scale)(Lv00Matrix *A, double c);
    void (*set_element)(Lv00Matrix *A, int64_t row, int64_t col, double val);
    double (*get_element)(const Lv00Matrix *A, int64_t row, int64_t col);
    int (*factor)(Lv00Matrix *A);
    int (*solve)(const Lv00Matrix *A, const Lv00Vector *b, Lv00Vector *x);
};
```

### 线性求解器抽象（SUNLinearSolver 风格）

```c
typedef enum {
    LV00_LINSOL_DIRECT_DENSE = 0,       /**< 直接法：稠密 LU */
    LV00_LINSOL_DIRECT_BAND = 1,        /**< 直接法：带状 LU */
    LV00_LINSOL_DIRECT_SPARSE = 2,      /**< 直接法：稀疏 LU */
    LV00_LINSOL_ITERATIVE_GMRES = 3,    /**< 迭代法：GMRES */
    LV00_LINSOL_ITERATIVE_BICGSTAB = 4, /**< 迭代法：BiCGSTAB */
    LV00_LINSOL_ITERATIVE_CG = 5,       /**< 迭代法：共轭梯度 */
    LV00_LINSOL_CUSTOM = 99
} Lv00LinearSolverMethod;
```

### 工厂函数

```c
Lv00Vector *lv00_vector_create(Lv00BackendType backend, int64_t n);
Lv00Matrix *lv00_matrix_create(Lv00BackendType backend, int64_t rows, int64_t cols, bool sparse);
Lv00LinearSolver *lv00_linsol_create(Lv00BackendType backend, Lv00LinearSolverMethod method);
```

## 6. rational.h —— 精确有理数运算

### 设计借鉴

提供基于 GMP mpz_t 的精确有理数类型 Lv00Rational。所有运算均为精确，不产生浮点舍入。分母始终 > 0，分子和分母始终互质（gcd = 1）。

### 有理数结构

```c
typedef struct {
    mpz_t num;  /**< 分子（可为负） */
    mpz_t den;  /**< 分母（始终 > 0） */
} Lv00Rational;
```

不变量：den > 0，gcd(num, den) == 1，num 和 den 均已通过 mpz_init() 初始化。

### 生命周期管理

```c
Lv00Rational *lv00_rational_create(void);
Lv00Rational *lv00_rational_create_from_mpz(const mpz_t num, const mpz_t den);
Lv00Rational *lv00_rational_create_from_si(long num, unsigned long den);
Lv00Rational *lv00_rational_create_from_i64(int64_t num, uint64_t den);
Lv00Rational *lv00_rational_clone(const Lv00Rational *src);
void lv00_rational_destroy(Lv00Rational **r);
```

### 算术运算

所有算术运算返回新分配的结果，调用者负责销毁：

```c
Lv00Rational *lv00_rational_add(const Lv00Rational *a, const Lv00Rational *b);
Lv00Rational *lv00_rational_sub(const Lv00Rational *a, const Lv00Rational *b);
Lv00Rational *lv00_rational_mul(const Lv00Rational *a, const Lv00Rational *b);
Lv00Rational *lv00_rational_div(const Lv00Rational *a, const Lv00Rational *b);
Lv00Rational *lv00_rational_neg(const Lv00Rational *a);
Lv00Rational *lv00_rational_inv(const Lv00Rational *a);
Lv00Rational *lv00_rational_abs(const Lv00Rational *a);
```

同时提供原地运算变体（`_inplace` 后缀）。

### 比较操作

```c
int lv00_rational_cmp(const Lv00Rational *a, const Lv00Rational *b);  /* <0 / 0 / >0 */
bool lv00_rational_equal(const Lv00Rational *a, const Lv00Rational *b);
bool lv00_rational_is_zero(const Lv00Rational *a);
bool lv00_rational_is_one(const Lv00Rational *a);
bool lv00_rational_is_integer(const Lv00Rational *a);
int lv00_rational_sgn(const Lv00Rational *a);
```

### 与 double 的转换

```c
/* 转换为 double（显式标注精度损失） */
bool lv00_rational_to_double(const Lv00Rational *r, double *out_lossy, int *out_loss_bits);

/* 预估精度损失比特数 */
int lv00_rational_estimate_loss(const Lv00Rational *r);
```

在 `LV00_STRICT_EXACT_MODE` 下调用转换函数会产生警告。仅供显示/日志使用，不得参与代数计算。

### 乘法安全检查

```c
bool lv00_rational_mul_is_safe(const Lv00Rational *a, const Lv00Rational *b, uint64_t max_bits);
bool lv00_rational_den_is_safe(const mpz_t den);
```

防止分母异常增长导致 GMP 运算性能急剧下降。

### 与 mpq_t 互操作

```c
Lv00Rational *lv00_rational_from_mpq(mpq_srcptr val);
void lv00_rational_to_mpq(const Lv00Rational *r, mpq_t out);
```

## 7. inequality_reasoning.h —— 不等式推理系统（桩实现）

### 设计借鉴

提供完全基于符号计算的不等式推理框架。所有数值计算使用 GMP 多精度有理数，禁止使用浮点运算。当前为桩实现，预留完整接口。

### 不等式类型与证明状态

```c
typedef enum {
    INEQ_LESS_THAN, INEQ_LESS_EQUAL,
    INEQ_GREATER_THAN, INEQ_GREATER_EQUAL,
    INEQ_NOT_EQUAL
} Lv00InequalityType;

typedef enum {
    INEQ_STATUS_UNPROVED, INEQ_STATUS_PROVED,
    INEQ_STATUS_DISPROVED, INEQ_STATUS_CONDITIONAL,
    INEQ_STATUS_UNKNOWN
} Lv00InequalityStatus;
```

### 证明方法枚举

```c
typedef enum {
    INEQ_METHOD_DIRECT,         /**< 直接证明 */
    INEQ_METHOD_AM_GM,          /**< AM-GM 不等式 */
    INEQ_METHOD_CAUCHY,         /**< Cauchy-Schwarz 不等式 */
    INEQ_METHOD_REARRANGEMENT,  /**< 排序不等式 */
    INEQ_METHOD_SCHUR,          /**< Schur 不等式 */
    INEQ_METHOD_JENSEN,         /**< Jensen 不等式 */
    INEQ_METHOD_HOLDER,         /**< Holder 不等式 */
    INEQ_METHOD_MINKOWSKI,      /**< Minkowski 不等式 */
    INEQ_METHOD_TRIANGLE,       /**< 三角形不等式 */
    INEQ_METHOD_SUBSTITUTION,   /**< 变量替换 */
    INEQ_METHOD_INDUCTION,      /**< 数学归纳法 */
    INEQ_METHOD_CONTRADICTION,  /**< 反证法 */
    INEQ_METHOD_SMART_SUM,      /**< 智能求和（Muirhead） */
    INEQ_METHOD_SOS             /**< 平方和分解 */
} Lv00InequalityMethod;
```

### 经典不等式 API

```c
/* AM-GM 不等式 */
bool lv00_ineq_am_gm(Lv00Expr **expressions, uint32_t count,
                     Lv00Expr **out_lower_bound, Lv00Expr **out_upper_bound);

/* Cauchy-Schwarz 不等式 */
bool lv00_ineq_cauchy_schwarz(Lv00Expr **a, Lv00Expr **b, uint32_t count,
                               Lv00Inequality **out_ineq);

/* Schur 不等式 */
bool lv00_ineq_schur(Lv00Expr *a, Lv00Expr *b, Lv00Expr *c, uint32_t r,
                     Lv00Inequality **out_ineq);

/* 三角形不等式 */
uint32_t lv00_ineq_triangle(Lv00Expr *a, Lv00Expr *b, Lv00Expr *c,
                             Lv00Inequality **out_inequalities, uint32_t max_count);
```

### 平方和分解

```c
typedef struct {
    Lv00Expr **squares;     /**< 平方项数组 */
    uint32_t count;         /**< 平方项数量 */
    Lv00Expr *remainder;    /**< 余项 */
} Lv00SOSDecomposition;

bool lv00_expr_sos_decompose(Lv00Expr *poly, Lv00SOSDecomposition **out_sos);
```

### 几何不等式

```c
/* 三角形面积不等式 */
bool lv00_ineq_triangle_area(Lv00Expr *a, Lv00Expr *b, Lv00Expr *c,
                              Lv00Expr *area, Lv00Inequality **out_ineq);

/* Weitzenbock 不等式：a^2 + b^2 + c^2 >= 4*sqrt(3)*S */
bool lv00_ineq_weitzenbock(Lv00Expr *a, Lv00Expr *b, Lv00Expr *c,
                            Lv00Expr *area, Lv00Inequality **out_ineq);

/* Erdos-Mordell 不等式 */
bool lv00_ineq_erdos_mordell(Lv00Expr *pa, Lv00Expr *pb, Lv00Expr *pc,
                              Lv00Expr *p, Lv00Expr *q, Lv00Expr *r,
                              Lv00Inequality **out_ineq);
```

## 8. probabilistic_constraint.h —— PRISM 概率模型检验（桩实现）

### 设计借鉴

借鉴 PRISM (prismmodelchecker.org) 的概率模型检测框架，为 Lv-00 提供概率分布约束、PCTL 公式评估与概率推理能力。当前为桩实现。

### 概率分布类型

```c
typedef enum {
    PROB_DIST_UNIFORM = 0,  /**< 均匀分布 U(a, b) */
    PROB_DIST_NORMAL = 1,   /**< 正态分布 N(mu, sigma^2) */
    PROB_DIST_DISCRETE = 2, /**< 离散分布 */
    PROB_DIST_BETA = 3,     /**< Beta 分布 */
    PROB_DIST_CUSTOM = 4    /**< 自定义分布 */
} ProbDistType;
```

### 概率分布操作

```c
ProbDistribution *prob_dist_create(ProbDistType type, double *params, int param_count);
void prob_dist_destroy(ProbDistribution *dist);
double prob_dist_pdf(ProbDistribution *dist, double x);
double prob_dist_cdf(ProbDistribution *dist, double x);
int prob_dist_sample(ProbDistribution *dist, int n_samples, double **out_samples);
```

### PCTL 公式

```c
typedef enum {
    PCTL_PROB_BOUND = 0,  /**< P~p [ phi ] — 概率边界 */
    PCTL_NEXT = 1,        /**< X phi — 下一状态 */
    PCTL_UNTIL = 2,       /**< phi U psi — 直到 */
    PCTL_EVENTUALLY = 3,  /**< F phi — 最终 */
    PCTL_ALWAYS = 4,      /**< G phi — 总是 */
    PCTL_STEADY_STATE = 5 /**< S~p [ phi ] — 稳态概率 */
} PCTLFormulaType;
```

### PCTL 评估

```c
/* 在约束图上评估 PCTL 公式 */
bool pctl_evaluate(const ConstraintGraph *graph, const PCTLFormula *formula,
                   double *out_probability);

/* PCTL 构造性检查（Monte Carlo 采样） */
bool pctl_check_constructibility(const ConstraintGraph *graph, double confidence);

/* 概率约束推理（贝叶斯网络风格信念传播） */
bool prob_constraint_infer(const ConstraintGraph *graph, int target_var,
                           ProbConstraintNode **constraints, int n, double *out_conf);
```

## 9. nt_number_theory.h —— 数论算法

### 设计借鉴

基于 GMP 提供模算术、素性测试、试除法分解和 GCD/LCM 计算。参考 NTL (Victor Shoup) 和 GMP mpz 文档。

### 模算术上下文

```c
typedef struct Lv00ModContext {
    mpz_t modulus;   /**< 模数（必须 > 0） */
    int   is_prime;  /**< 非零表示模数已知为素数 */
} Lv00ModContext;
```

### 模算术操作

```c
void nt_mod_context_init(Lv00ModContext *ctx);
void nt_mod_context_set(Lv00ModContext *ctx, const mpz_t modulus);
void nt_mod_context_clear(Lv00ModContext *ctx);

void nt_mod_add(mpz_t result, const Lv00ModContext *ctx, const mpz_t a, const mpz_t b);
void nt_mod_mul(mpz_t result, const Lv00ModContext *ctx, const mpz_t a, const mpz_t b);
int nt_mod_inv(mpz_t result, const Lv00ModContext *ctx, const mpz_t a);  /* 返回是否存在逆元 */
void nt_mod_pow(mpz_t result, const Lv00ModContext *ctx, const mpz_t base, const mpz_t exp);
```

模逆使用扩展欧几里得算法（`mpz_invert`），模幂使用 GMP 的 `mpz_powm` 高效计算。

### GCD 和 LCM

```c
void nt_gcd(mpz_t result, const mpz_t a, const mpz_t b);
void nt_lcm(mpz_t result, const mpz_t a, const mpz_t b);
```

### 素性测试

```c
/* Miller-Rabin 概率素性测试 */
int nt_is_prime_miller_rabin(const mpz_t n, int k);

/* 下一个素数 */
void nt_next_prime(mpz_t result, const mpz_t n);
```

Miller-Rabin 测试执行 k 轮见证检查。对于 n < 3 返回 0（非素数），n = 2 或 3 返回 1。推荐 k = 20-40。

### 试除法分解

```c
int nt_factorize_trial_div(const mpz_t n, mpz_t *factors,
                           int max_factors, const mpz_t bound);
```

使用试除法查找 n 的所有素因子（不超过 bound），因子按非递减顺序存储。

## 10. nt_polynomial.h —— 任意精度整数系数多项式算术

### 设计借鉴

提供基于 GMP 的任意精度整数系数多项式运算。参考 NTL ZZ_pX 和 FLINT nmod_poly。

### 多项式结构

```c
typedef struct Lv00Poly {
    mpz_t *coeffs;   /**< 系数数组（升幂序：coeffs[i] 为 x^i 的系数） */
    int    degree;   /**< 当前度数（-1 表示零多项式） */
    int    capacity; /**< 已分配大小 */
} Lv00Poly;
```

### 生命周期

```c
Lv00Poly *nt_poly_create(void);
void nt_poly_destroy(Lv00Poly *p);
```

### 系数访问

```c
int nt_poly_set_coeff(Lv00Poly *p, int deg, const mpz_t val);
int nt_poly_get_coeff(const Lv00Poly *p, int deg, mpz_t out);
```

### 多项式算术

```c
/* 加法：result = a + b */
int nt_poly_add(Lv00Poly *result, const Lv00Poly *a, const Lv00Poly *b);

/* 乘法：result = a * b */
int nt_poly_mul(Lv00Poly *result, const Lv00Poly *a, const Lv00Poly *b);

/* 模归约：result = f mod m（多项式长除法） */
int nt_poly_mod(Lv00Poly *result, const Lv00Poly *f, const Lv00Poly *m);

/* GCD：result = gcd(a, b)（Z[x] 上的欧几里得算法） */
int nt_poly_gcd(Lv00Poly *result, const Lv00Poly *a, const Lv00Poly *b);
```

### 求值与性质

```c
/* Horner 法求值：p(x) */
int nt_poly_eval(const Lv00Poly *p, const mpz_t x, mpz_t out);

/* 获取度数（零多项式返回 -1） */
int nt_poly_degree(const Lv00Poly *p);
```

## 11. sparse_linear_algebra.h —— 稀疏线性代数后端

### 设计借鉴

借鉴 SuiteSparse/GraphBLAS (v9.x) 的稀疏矩阵代数框架，将约束传播建模为半环上的稀疏矩阵乘法。提供 CSR/CSC/COO 稀疏格式、GraphBLAS 半环抽象、约束传播不动点迭代和稀疏线性求解器接口（CHOLMOD/UMFPACK 风格）。

核心思想：约束传播 = 邻接矩阵在半环 (V, ⊕, ⊗) 上的迭代乘法。

### 稀疏存储格式

```c
typedef enum {
    SPARSE_CSR = 0,  /**< 压缩稀疏行 — 适合行遍历 */
    SPARSE_CSC = 1,  /**< 压缩稀疏列 — 适合列遍历 */
    SPARSE_COO = 2,  /**< 坐标格式 — 适合增量构建 */
    SPARSE_DENSE = 3 /**< 稠密格式 — 小矩阵 fallback */
} SparseFormat;
```

### 稀疏矩阵结构

```c
typedef struct {
    int rows;         /**< 行数 */
    int cols;         /**< 列数 */
    int nnz;          /**< 非零元素数量 */
    SparseFormat fmt; /**< 存储格式 */
    int *row_ptr;     /**< 行指针（CSR）/ 列指针（CSC），长度 rows+1 */
    int *col_idx;     /**< 列索引（CSR）/ 行索引（CSC），长度 nnz */
    double *values;   /**< 非零数值，NULL 时表示结构矩阵 */
    bool owns_data;   /**< 是否拥有底层数组的所有权 */
} SparseMatrix;
```

### 半环抽象（GraphBLAS Semiring）

```c
typedef enum {
    SEMIRING_PLUS_TIMES = 0, /**< (R, +, x) — 经典实数半环 */
    SEMIRING_MIN_PLUS = 1,   /**< (R∪{inf}, min, +) — 最短路径/热带半环 */
    SEMIRING_MAX_TIMES = 2,  /**< (R, max, x) — 最大可信度传播 */
    SEMIRING_OR_AND = 3,     /**< ({0,1}, v, ^) — 布尔半环 */
    SEMIRING_BOOL = 4,       /**< SEMIRING_OR_AND 别名 */
    SEMIRING_INTERVAL = 5    /**< (I, n, +_interval) — 区间约束传播 */
} SemiringType;

typedef struct {
    semiring_add_fn add_op;      /**< 半环加法 */
    semiring_multiply_fn mul_op; /**< 半环乘法 */
    double add_identity;         /**< 加法单位元（零元） */
    double mul_identity;         /**< 乘法单位元（壹元） */
    SemiringType type;
    const char *name;
} Semiring;
```

### 约束传播（半环矩阵乘法建模）

```c
int semiring_propagate_constraints(ConstraintGraph *g, SemiringType semiring,
                                   double *x, int max_iter);
```

将约束图上的约束传播建模为半环矩阵乘法的不动点迭代：
1. 从约束图构建稀疏邻接矩阵 A（CSR 格式）
2. 初始化节点约束值向量 x
3. 迭代计算 x_{k+1} = A ⊗ x_k
4. 直到达到不动点或超过最大迭代次数

典型应用：MIN_PLUS 计算最短推导路径，OR_AND 计算可达性，INTERVAL 区间约束传播。

### 稀疏线性求解器（桩实现）

```c
/* Cholesky 分解求解（对称正定矩阵） */
bool sparse_cholesky_solve(const SparseMatrix *A, const double *b, double *x);

/* LU 分解求解（非对称矩阵） */
bool sparse_lu_solve(const SparseMatrix *A, const double *b, double *x);

/* QR 分解求解最小二乘问题 */
bool sparse_qr_solve(const SparseMatrix *A, const double *b, double *x);
```

### 约束图转换与矩阵运算

```c
/* 从约束图提取约束矩阵的稀疏结构 */
bool graph_to_constraint_matrix(const ConstraintGraph *graph, SparseMatrix **mat);

/* 稀疏矩阵乘法 C = A * B */
bool sparse_matrix_multiply(const SparseMatrix *A, const SparseMatrix *B, SparseMatrix **C);

/* 稀疏矩阵转置 */
bool sparse_matrix_transpose(const SparseMatrix *mat, SparseMatrix **out);
```

### 度分析

```c
typedef struct {
    int node_count;     /**< 节点总数 */
    int max_degree;     /**< 最大度数 */
    int min_degree;     /**< 最小度数 */
    double avg_degree;  /**< 平均度数 */
    int isolated_count; /**< 孤立节点数 */
    int *degree_counts; /**< 度分布直方图 */
    int *node_degrees;  /**< 每个节点的度数 */
} DegreeAnalysis;

bool graph_degree_analysis(const ConstraintGraph *graph, DegreeAnalysis **analysis);
void degree_analysis_free(DegreeAnalysis *analysis);
```

## 模块间依赖关系

```
interval_arithmetic.h          (统一区间算术基础，无外部依赖)
    └── float_error.h          (依赖 constraint_graph.h, exact_arithmetic.h, symbolic_coord.h)

ode_solver.h                   (依赖 lv00.h)
autodiff.h                     (依赖 lv00.h)
numerical_backend.h            (依赖 exact_arithmetic.h)

rational.h                     (依赖 gmp.h)
nt_number_theory.h             (依赖 lv00.h, gmp.h)
nt_polynomial.h                (依赖 lv00.h, gmp.h)

inequality_reasoning.h         (依赖 gmp.h, expr_canonical.h, symbolic_coord.h)
probabilistic_constraint.h     (依赖 constraint_graph.h)

sparse_linear_algebra.h        (依赖 lv00.h, constraint_graph.h)
```

## 设计参考索引

| 模块 | 借鉴来源 |
|------|----------|
| interval_arithmetic | MPFI (mpfi.org), FLINT/Arb, IEEE 1788 |
| float_error | FPTaylor, Gappa, FLUCTUAT (CEA LIST) |
| ode_solver | DifferentialEquations.jl, SUNDIALS CVODE, IPOPT |
| autodiff | Enzyme (LLVM AD), PyTorch autograd |
| numerical_backend | SUNDIALS N_Vector/SUNMatrix/SUNLinearSolver |
| rational | GMP mpz_t/mpq_t |
| inequality_reasoning | AM-GM, Cauchy-Schwarz, SOS 分解理论 |
| probabilistic_constraint | PRISM Probabilistic Model Checker, PCTL |
| nt_number_theory | NTL (Victor Shoup), GMP mpz |
| nt_polynomial | NTL ZZ_pX, FLINT nmod_poly |
| sparse_linear_algebra | SuiteSparse/GraphBLAS, CHOLMOD, UMFPACK |

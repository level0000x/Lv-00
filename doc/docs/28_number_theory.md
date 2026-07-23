# 28. 数论与多项式系统

## 28.1 模块概述

本文档描述 Lv-00 几何元语言系统中的数论、多精度多项式与精确有理数模块。该组模块位于推理层与数值后端之间，为符号坐标、代数数、求解器、精确证明与构造一致性判断提供可复用的算术基础。

**覆盖头文件**：
- `nt_number_theory.h` —— GMP 数论算法：模运算、素性测试、因式分解、GCD/LCM
- `nt_polynomial.h` —— 任意精度整数系数多项式运算
- `mpz_poly.h` —— 多精度整数多项式与代数数结式计算
- `rational.h` —— 基于 `mpz_t` 的精确有理数类型

---

## 28.2 理论定位

Lv-00 的几何元语言不把数值视为独立于几何的外部对象，而将数值作为几何构造中的可检验量。数论与多项式系统承担以下理论职责：

1. **精确算术基底**：所有可证明路径必须避免浮点舍入误差。
2. **代数构造支撑**：代数数、最小多项式、结式计算用于描述由几何构造诱导的坐标。
3. **有限域与模算术工具**：用于小范围搜索、哈希、概率性检测和可满足性辅助判断。
4. **证明可信边界**：当有理数或整数多项式运算保持精确时，其结果可进入绿色可信路径；当转换为 double 时必须标注精度损失。

---

## 28.3 nt_number_theory.h —— 数论算法

### 28.3.1 模块定位

`nt_number_theory.h` 提供基于 GMP 的基础数论算法，参考 NTL 与 GMP `mpz` 文档，主要用于：
- 模运算上下文管理
- 模加、模乘、模逆、模幂
- GCD / LCM
- Miller-Rabin 概率素性测试
- Trial division 因式分解

### 28.3.2 模运算上下文

```c
typedef struct lvModContext {
    mpz_t modulus;   // 模数，必须 > 0
    int   is_prime;  // 若已知模数为素数，则非零
} lvModContext;
```

该结构将模数与其素性缓存封装在一起。素性缓存可用于优化模逆计算和有限域运算路径。

生命周期 API：

```c
void nt_mod_context_init(lvModContext *ctx);
void nt_mod_context_set(lvModContext *ctx, const mpz_t modulus);
void nt_mod_context_clear(lvModContext *ctx);
```

### 28.3.3 模运算 API

```c
void nt_mod_add(mpz_t result, const lvModContext *ctx,
                const mpz_t a, const mpz_t b);

void nt_mod_mul(mpz_t result, const lvModContext *ctx,
                const mpz_t a, const mpz_t b);

int nt_mod_inv(mpz_t result, const lvModContext *ctx,
               const mpz_t a);

void nt_mod_pow(mpz_t result, const lvModContext *ctx,
                const mpz_t base, const mpz_t exp);
```

其中 `nt_mod_inv` 使用扩展欧几里得算法；当 `gcd(a, n) != 1` 时逆元不存在，函数返回 false。

### 28.3.4 GCD 与 LCM

```c
void nt_gcd(mpz_t result, const mpz_t a, const mpz_t b);
void nt_lcm(mpz_t result, const mpz_t a, const mpz_t b);
```

GCD 用于有理数规范化、多项式内容提取和约束系数约简；LCM 用于统一分母、构造公共模数或多项式系数归一。

### 28.3.5 素性测试与因式分解

```c
int nt_is_prime_miller_rabin(const mpz_t n, int k);
void nt_next_prime(mpz_t result, const mpz_t n);
int nt_factorize_trial_div(const mpz_t n, mpz_t *factors,
                           int max_factors, const mpz_t bound);
```

- `nt_is_prime_miller_rabin`：执行 k 轮 Miller-Rabin 概率素性测试。
- `nt_next_prime`：调用 GMP 的 `mpz_nextprime` 找到不小于 n 的下一个素数。
- `nt_factorize_trial_div`：在给定上界内进行试除因式分解。

---

## 28.4 nt_polynomial.h —— 任意精度整数多项式

### 28.4.1 数据结构

```c
typedef struct lvPoly {
    mpz_t *coeffs;   // 系数数组，按次数升序排列
    int    degree;   // 当前次数，-1 表示零多项式
    int    capacity; // 已分配容量
} lvPoly;
```

系数排列约定：

```text
coeffs[0] = 常数项
coeffs[i] = x^i 的系数
```

零多项式以 `degree < 0` 表示。

### 28.4.2 生命周期与系数访问

```c
lvPoly *nt_poly_create(void);
void nt_poly_destroy(lvPoly *p);

int nt_poly_set_coeff(lvPoly *p, int deg, const mpz_t val);
int nt_poly_get_coeff(const lvPoly *p, int deg, mpz_t out);
```

`nt_poly_set_coeff` 在次数超过当前容量时自动扩容，并在非零系数出现时更新多项式次数。

### 28.4.3 多项式运算

```c
int nt_poly_add(lvPoly *result, const lvPoly *a, const lvPoly *b);
int nt_poly_mul(lvPoly *result, const lvPoly *a, const lvPoly *b);
int nt_poly_mod(lvPoly *result, const lvPoly *f, const lvPoly *m);
int nt_poly_gcd(lvPoly *result, const lvPoly *a, const lvPoly *b);
```

- `nt_poly_mod` 使用多项式长除法，将 f 约化为次数小于 m 的余式。
- `nt_poly_gcd` 使用欧几里得算法计算多项式最大公因式。

### 28.4.4 求值与性质

```c
int nt_poly_eval(const lvPoly *p, const mpz_t x, mpz_t out);
int nt_poly_degree(const lvPoly *p);
```

`nt_poly_eval` 使用 Horner 方法，避免直接幂展开造成中间值膨胀。

---

## 28.5 mpz_poly.h —— 多精度整数多项式与结式

### 28.5.1 数据结构

```c
typedef struct {
    mpz_t *coeffs;
    int degree;
} mpz_poly_t;
```

该结构是轻量级多项式类型，侧重内联操作与代数数运算辅助。

### 28.5.2 基础操作

```c
void mpz_poly_init(mpz_poly_t *p);
void mpz_poly_clear(mpz_poly_t *p);
void mpz_poly_set(mpz_poly_t *dst, const mpz_poly_t *src);
int  mpz_poly_equal(const mpz_poly_t *a, const mpz_poly_t *b);

void mpz_poly_add(mpz_poly_t *result, const mpz_poly_t *a, const mpz_poly_t *b);
void mpz_poly_sub(mpz_poly_t *result, const mpz_poly_t *a, const mpz_poly_t *b);
void mpz_poly_mul(mpz_poly_t *result, const mpz_poly_t *a, const mpz_poly_t *b);
void mpz_poly_div(mpz_poly_t *quotient, mpz_poly_t *dividend,
                  const mpz_poly_t *divisor);
char *mpz_poly_get_str(const mpz_poly_t *p);
```

注意：`mpz_poly_div` 会就地修改 `dividend`，除法完成后 `dividend` 的内容变为余数。若调用者需要保留原始被除数，必须在调用前自行拷贝。

### 28.5.3 结式计算

```c
typedef enum {
    ALG_OP_SUM,     // 加法运算
    ALG_OP_PRODUCT  // 乘法运算
} AlgebraicOp;

bool mpz_poly_resultant(const mpz_poly_t *p,
                        const mpz_poly_t *q,
                        AlgebraicOp op,
                        mpz_poly_t *result);
```

结式用于代数数运算：

```text
alpha + beta:
    Res_y(p(y), q(x - y))

alpha * beta:
    Res_y(p(y), y^n * q(x/y)), n = deg(q)
```

在 Lv-00 中，该接口为代数坐标的封闭运算提供基础。若两个构造点的坐标分别满足最小多项式 p 和 q，则其和、积可通过结式推导新的候选最小多项式。

---

## 28.6 rational.h —— 精确有理数类型

### 28.6.1 设计不变量

```c
typedef struct {
    mpz_t num;  // 分子，可为负
    mpz_t den;  // 分母，始终 > 0
} lvRational;
```

该类型保持以下不变量：

```text
den > 0
gcd(num, den) == 1
num 与 den 均已通过 mpz_init 初始化
```

### 28.6.2 创建与销毁

```c
lvRational *lv_rational_create(void);
lvRational *lv_rational_create_from_mpz(const mpz_t num, const mpz_t den);
lvRational *lv_rational_create_from_si(long num, unsigned long den);
lvRational *lv_rational_create_from_i64(int64_t num, uint64_t den);
lvRational *lv_rational_clone(const lvRational *src);
void lv_rational_destroy(lvRational **r);
```

### 28.6.3 规范化

```c
void lv_rational_simplify(lvRational *r);
```

规范化执行：
1. 分子分母同除以 gcd；
2. 若分母为负，将符号移动到分子；
3. 保持分母为正且分子分母互质。

### 28.6.4 精确运算

```c
lvRational *lv_rational_add(const lvRational *a, const lvRational *b);
lvRational *lv_rational_sub(const lvRational *a, const lvRational *b);
lvRational *lv_rational_mul(const lvRational *a, const lvRational *b);
lvRational *lv_rational_div(const lvRational *a, const lvRational *b);
lvRational *lv_rational_neg(const lvRational *a);
lvRational *lv_rational_inv(const lvRational *a);
lvRational *lv_rational_abs(const lvRational *a);
```

同时提供原地运算：

```c
void lv_rational_add_inplace(lvRational *a, const lvRational *b);
void lv_rational_sub_inplace(lvRational *a, const lvRational *b);
void lv_rational_mul_inplace(lvRational *a, const lvRational *b);
bool lv_rational_div_inplace(lvRational *a, const lvRational *b);
```

### 28.6.5 精确比较

```c
int  lv_rational_cmp(const lvRational *a, const lvRational *b);
bool lv_rational_equal(const lvRational *a, const lvRational *b);
bool lv_rational_is_zero(const lvRational *a);
bool lv_rational_is_one(const lvRational *a);
bool lv_rational_is_integer(const lvRational *a);
int  lv_rational_sgn(const lvRational *a);
```

证明路径应优先使用这些精确比较接口，而不是转换为 double 后比较。

### 28.6.6 非精确转换与安全边界

```c
bool lv_rational_to_double(const lvRational *r,
                             double *out_lossy,
                             int *out_loss_bits);
int lv_rational_estimate_loss(const lvRational *r);
```

`lv_rational_to_double` 明确标注精度损失，仅供显示或日志使用，不应参与代数证明计算。

安全检查：

```c
bool lv_rational_mul_is_safe(const lvRational *a,
                               const lvRational *b,
                               uint64_t max_bits);
bool lv_rational_den_is_safe(const mpz_t den);
```

这些接口用于防止分母或中间项异常增长导致性能失控。

### 28.6.7 序列化与互操作

```c
char *lv_rational_to_string(const lvRational *r);
lvRational *lv_rational_from_string(const char *s);
lvRational *lv_rational_from_mpq(mpq_srcptr val);
void lv_rational_to_mpq(const lvRational *r, mpq_t out);
```

支持格式：`"123"`, `"-456"`, `"3/4"`, `"-7/8"`。

---

## 28.7 理论—代码对应关系

| 代码概念 | 理论对应 | 说明 |
|----------|----------|------|
| `lvRational` | 精确有理域 Q | 保持分母正、分子分母互质 |
| `lvModContext` | 剩余类环 Z/nZ | 封装模数与素性缓存 |
| `nt_mod_inv` | 单位元逆元 | 当 gcd(a,n)=1 时存在 |
| `nt_is_prime_miller_rabin` | 概率素性判定 | 用于大整数素性快速筛选 |
| `lvPoly` | Z[x] 多项式环 | 任意精度整数系数 |
| `nt_poly_gcd` | 多项式欧几里得算法 | 求公共因子与约简 |
| `mpz_poly_resultant` | 结式 Res | 推导代数数和、积的最小多项式候选 |
| `lv_rational_to_double` | 非精确投影 | 进入显示/日志路径，不进入证明核心 |

---

## 28.8 相关模块文档

| 文档 | 关联内容 |
|------|----------|
| [01_symbolic_coord.md](01_symbolic_coord.md) | 符号坐标与代数坐标表示 |
| [04_solver.md](04_solver.md) | Gröbner 与代数求解 |
| [17_numerical_analysis.md](17_numerical_analysis.md) | 数值分析与区间算术 |
| [29_inequality_approximation.md](29_inequality_approximation.md) | 不等式与近似计算 |
| [23_core_infrastructure.md](23_core_infrastructure.md) | 配置、安全阈值与错误处理 |

---

## 28.9 版本历史

- **v5.0.0**
  - 补全文档化：数论、任意精度多项式、多精度多项式结式、有理数精确类型。
  - 明确 double 转换的非证明边界。

- **v3.3.0**
  - 引入 GMP 数论与多项式运算基础接口。

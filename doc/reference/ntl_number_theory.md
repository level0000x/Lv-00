# NTL (Number Theory Library) 参考文档

## 1. 项目概述

### 1.1 项目简介

NTL（Number Theory Library）是由 Victor Shoup 开发的高性能数论计算库。Victor Shoup 是纽约大学计算机科学系教授，同时也是 IBM 研究院的研究员，在密码学和计算数论领域享有盛誉。NTL 自 1990 年代初期开始开发，经过三十多年的持续演进，已成为学术界和工业界广泛使用的数论计算基础设施。

NTL 的设计目标是为数论、代数和密码学计算提供可移植、高效且易于使用的 C++ 库。它特别注重算法的渐近复杂度和实际运行性能之间的平衡，在多项式算术、格基约化、大整数运算等领域提供了业界领先的实现。

### 1.2 技术栈

| 组件 | 技术 | 说明 |
|------|------|------|
| 核心语言 | C++11/14/17 | 现代 C++ 标准，支持模板元编程 |
| 底层依赖 | GMP/MPFR | 可选的高精度算术后端 |
| 线程支持 | C++11 threads | 线程安全的数据结构和算法 |
| 构建系统 | GNU Make/CMake | 跨平台构建支持 |
| 测试框架 | 自定义测试套件 | 全面的回归测试覆盖 |
| 文档系统 | Doxygen | API 文档自动生成 |

NTL 采用分层架构设计：
- **基础层**：大整数（ZZ）、浮点数和基本算术运算
- **代数层**：模运算、多项式环、有限域
- **高级层**：格基约化、多项式分解、素性测试
- **应用层**：密码学原语、编码理论工具

### 1.3 社区活跃度

NTL 拥有稳定且活跃的学术用户社区：

- **开发历史**：自 1993 年开始持续开发，版本更新频繁
- **学术引用**：在密码学和计算数论论文中被广泛引用
- **集成项目**：被 SageMath、PARI/GP、HElib 等知名数学软件集成
- **邮件列表**：活跃的开发者讨论和用户支持渠道
- **文档质量**：详尽的数学背景说明和 API 文档

### 1.4 许可证

NTL 采用 **GNU Lesser General Public License (LGPL) v2.1+** 许可：

- 允许在商业和非商业项目中使用
- 修改后的库代码必须开源
- 允许与闭源应用程序链接
- 与 GMP 的许可证兼容

---

## 2. 核心借鉴点

### 2.1 NTL 核心特性分析

NTL 的核心优势在于其精心设计的类体系和算法实现：

#### 2.1.1 大整数系统（ZZ）

NTL 的 ZZ 类提供了任意精度整数运算，具有以下特点：
- 基于 GMP 的高性能后端
- 自动内存管理和垃圾回收
- 支持所有标准算术运算和比较操作
- 提供高效的数论函数（GCD、模逆、幂运算）

#### 2.1.2 模运算系统（ZZ_p 系列）

NTL 提供了完整的模运算支持：
- ZZ_p：模 p 整数类
- ZZ_pX：模 p 多项式类
- ZZ_pE：模 p 扩域元素
- 自动模数上下文管理

#### 2.1.3 多项式系统

多项式是 NTL 的核心数据类型：
- ZZX：整数多项式
- ZZ_pX：模 p 多项式
- GF2X：GF(2) 上的多项式
- 支持快速乘法（FFT/NTT）、除法、GCD 计算

#### 2.1.4 矩阵和向量系统

- mat_ZZ：整数矩阵
- vec_ZZ：整数向量
- mat_ZZ_p：模 p 矩阵
- 提供线性代数运算（行列式、秩、线性方程组求解）

#### 2.1.5 格基约化（LLL）

NTL 实现了多种格基约化算法：
- 经典 LLL 算法
- 块 Korkin-Zolotarev (BKZ) 算法
- 浮点 LLL 变体（更快但可能略失精确性）
- 用于密码分析的高级功能

### 2.2 NTL 与 Lv-00 第 1 层对照表

| NTL 特性 | Lv-00 第 1 层对应 | 借鉴价值 | 实现优先级 |
|----------|-------------------|----------|------------|
| ZZ（任意精度整数） | GMP 封装（mpz_t） | 高：API 设计模式 | 高 |
| ZZ_p（模 p 整数） | 模运算模块 | 高：模数上下文管理 | 高 |
| ZZ_pX（模 p 多项式） | 多项式算术 | 高：FFT 乘法实现 | 高 |
| GF2X（GF(2) 多项式） | 二进制多项式 | 中：编码理论应用 | 中 |
| mat_ZZ（整数矩阵） | 矩阵运算 | 高：行列式、秩计算 | 高 |
| vec_ZZ（整数向量） | 向量运算 | 中：基础数据结构 | 中 |
| LLL 格基约化 | 格约化模块 | 高：密码学应用 | 中 |
| 素性测试 | 素数生成 | 中：概率性测试算法 | 中 |
| 多项式分解 | 因式分解 | 高：代数几何应用 | 中 |
| 快速傅里叶变换 | FFT 模块 | 高：卷积运算加速 | 高 |

### 2.3 关键技术借鉴点

#### 2.3.1 模数上下文管理

NTL 采用全局模数上下文的设计，简化了模运算的 API：

```cpp
// NTL 风格：设置全局模数
ZZ p = to_ZZ(17);
ZZ_p::init(p);  // 设置模数上下文

ZZ_p a = to_ZZ_p(3);
ZZ_p b = to_ZZ_p(5);
ZZ_p c = a * b;  // 自动在模 p 下运算
```

这种设计模式可以借鉴到 Lv-00 中，减少函数参数传递。

#### 2.3.2 延迟计算和表达式模板

NTL 在某些运算中使用延迟计算策略，避免不必要的中间结果拷贝。

#### 2.3.3 算法选择启发式

NTL 根据输入大小自动选择最优算法：
- 小多项式：经典 O(n^2) 乘法
- 中等多项式：Karatsuba 算法
- 大多项式：FFT/NTT 乘法

---

## 3. Lv-00 映射方案

### 3.1 架构映射

将 NTL 的概念映射到 Lv-00 的 7 层架构：

```
NTL                    Lv-00
-------------------------------------------------
ZZ (大整数)      ->    第 1 层：GMP 封装
ZZ_p (模整数)    ->    第 1 层：模运算类型
ZZ_pX (模多项式) ->    第 1 层：多项式算术
GF2X (GF2多项式) ->    第 1 层：二进制多项式
mat_ZZ (矩阵)    ->    第 1 层：矩阵运算
LLL 算法         ->    第 3 层：算法引擎
多项式分解       ->    第 3 层：符号计算
素性测试         ->    第 1 层：数论函数
```

### 3.2 C 代码示例

#### 3.2.1 大整数基础运算

```c
/*
 * Lv-00 风格的大整数实现
 * 借鉴 NTL 的 ZZ 类设计
 */

#ifndef LV_ZZ_H
#define LV_ZZ_H

#include <gmp.h>
#include <stdbool.h>

typedef struct {
    mpz_t value;
    int trust_level;  /* Lv-00 信任颜色系统 */
} lv_zz_t;

/* 初始化与清理 */
void lv_zz_init(lv_zz_t* z);
void lv_zz_init_set(lv_zz_t* z, const lv_zz_t* src);
void lv_zz_clear(lv_zz_t* z);

/* 赋值操作 */
void lv_zz_set(lv_zz_t* dst, const lv_zz_t* src);
void lv_zz_set_si(lv_zz_t* z, long val);
void lv_zz_set_ui(lv_zz_t* z, unsigned long val);

/* 算术运算 */
void lv_zz_add(lv_zz_t* result, const lv_zz_t* a, const lv_zz_t* b);
void lv_zz_sub(lv_zz_t* result, const lv_zz_t* a, const lv_zz_t* b);
void lv_zz_mul(lv_zz_t* result, const lv_zz_t* a, const lv_zz_t* b);
void lv_zz_div(lv_zz_t* q, const lv_zz_t* a, const lv_zz_t* b);
void lv_zz_mod(lv_zz_t* r, const lv_zz_t* a, const lv_zz_t* b);

/* 数论函数 */
void lv_zz_gcd(lv_zz_t* g, const lv_zz_t* a, const lv_zz_t* b);
bool lv_zz_invert(lv_zz_t* x, const lv_zz_t* a, const lv_zz_t* m);
void lv_zz_powm(lv_zz_t* result, const lv_zz_t* base, 
                const lv_zz_t* exp, const lv_zz_t* mod);

/* 比较操作 */
int lv_zz_cmp(const lv_zz_t* a, const lv_zz_t* b);
bool lv_zz_is_zero(const lv_zz_t* z);
bool lv_zz_is_one(const lv_zz_t* z);

#endif /* LV_ZZ_H */
```

#### 3.2.2 模运算上下文管理

```c
/*
 * 借鉴 NTL 的 ZZ_p 上下文管理设计
 * 实现线程安全的模数上下文
 */

#ifndef LV_ZZ_P_H
#define LV_ZZ_P_H

#include "lv_zz.h"
#include <pthread.h>

/* 模数上下文结构 */
typedef struct {
    lv_zz_t modulus;
    bool initialized;
    pthread_mutex_t lock;
} lv_zz_p_context_t;

/* 全局上下文（线程局部存储） */
extern __thread lv_zz_p_context_t* lv_zz_p_current_context;

/* 上下文管理 */
int lv_zz_p_init(const lv_zz_t* p);
void lv_zz_p_cleanup(void);
const lv_zz_t* lv_zz_p_modulus(void);
bool lv_zz_p_is_initialized(void);

/* 模 p 整数类型 */
typedef struct {
    lv_zz_t value;  /* 始终归一化为 [0, p-1] */
    int trust_level;
} lv_zz_p_t;

/* 模 p 整数操作 */
void lv_zz_p_init(lv_zz_p_t* x);
void lv_zz_p_init_set(lv_zz_p_t* x, const lv_zz_p_t* src);
void lv_zz_p_clear(lv_zz_p_t* x);

void lv_zz_p_set(lv_zz_p_t* dst, const lv_zz_p_t* src);
void lv_zz_p_set_zz(lv_zz_p_t* dst, const lv_zz_t* src);
void lv_zz_p_set_si(lv_zz_p_t* dst, long val);

/* 模运算 - 自动使用当前上下文 */
void lv_zz_p_add(lv_zz_p_t* result, const lv_zz_p_t* a, const lv_zz_p_t* b);
void lv_zz_p_sub(lv_zz_p_t* result, const lv_zz_p_t* a, const lv_zz_p_t* b);
void lv_zz_p_mul(lv_zz_p_t* result, const lv_zz_p_t* a, const lv_zz_p_t* b);
void lv_zz_p_div(lv_zz_p_t* result, const lv_zz_p_t* a, const lv_zz_p_t* b);
void lv_zz_p_negate(lv_zz_p_t* result, const lv_zz_p_t* a);
void lv_zz_p_pow(lv_zz_p_t* result, const lv_zz_p_t* base, const lv_zz_t* exp);

/* 使用示例 */
/*
 * lv_zz_t p;
 * lv_zz_init_set_si(&p, 17);
 * lv_zz_p_init(&p);  // 设置模数上下文
 * 
 * lv_zz_p_t a, b, c;
 * lv_zz_p_init(&a);
 * lv_zz_p_init(&b);
 * lv_zz_p_init(&c);
 * 
 * lv_zz_p_set_si(&a, 3);
 * lv_zz_p_set_si(&b, 5);
 * lv_zz_p_mul(&c, &a, &b);  // c = 15 mod 17
 * 
 * lv_zz_p_clear(&a);
 * lv_zz_p_clear(&b);
 * lv_zz_p_clear(&c);
 * lv_zz_p_cleanup();
 * lv_zz_clear(&p);
 */

#endif /* LV_ZZ_P_H */
```

#### 3.2.3 多项式算术

```c
/*
 * 借鉴 NTL 的 ZZ_pX 多项式实现
 * 支持 FFT 加速乘法
 */

#ifndef LV_ZZ_PX_H
#define LV_ZZ_PX_H

#include "lv_zz_p.h"

/* 多项式项 */
typedef struct lv_zz_px_term {
    int degree;
    lv_zz_p_t coeff;
    struct lv_zz_px_term* next;
} lv_zz_px_term_t;

/* 多项式类型 - 稀疏表示 */
typedef struct {
    lv_zz_px_term_t* head;
    int max_degree;
    int trust_level;
} lv_zz_px_sparse_t;

/* 多项式类型 - 稠密表示 */
typedef struct {
    lv_zz_p_t* coeffs;  /* coeffs[i] 是 x^i 的系数 */
    int degree;
    int alloc;
    int trust_level;
} lv_zz_px_dense_t;

/* 稠密多项式操作 */
void lv_zz_px_init(lv_zz_px_dense_t* poly);
void lv_zz_px_init_degree(lv_zz_px_dense_t* poly, int degree);
void lv_zz_px_clear(lv_zz_px_dense_t* poly);

void lv_zz_px_set_coeff(lv_zz_px_dense_t* poly, int i, const lv_zz_p_t* coeff);
void lv_zz_px_get_coeff(lv_zz_p_t* coeff, const lv_zz_px_dense_t* poly, int i);

/* 多项式运算 */
void lv_zz_px_add(lv_zz_px_dense_t* result, 
                  const lv_zz_px_dense_t* a, 
                  const lv_zz_px_dense_t* b);
void lv_zz_px_sub(lv_zz_px_dense_t* result,
                  const lv_zz_px_dense_t* a,
                  const lv_zz_px_dense_t* b);

/* 乘法 - 自动选择算法 */
void lv_zz_px_mul_classical(lv_zz_px_dense_t* result,
                            const lv_zz_px_dense_t* a,
                            const lv_zz_px_dense_t* b);
void lv_zz_px_mul_karatsuba(lv_zz_px_dense_t* result,
                            const lv_zz_px_dense_t* a,
                            const lv_zz_px_dense_t* b);
void lv_zz_px_mul_fft(lv_zz_px_dense_t* result,
                      const lv_zz_px_dense_t* a,
                      const lv_zz_px_dense_t* b);

/* 自动选择最优算法 */
void lv_zz_px_mul(lv_zz_px_dense_t* result,
                  const lv_zz_px_dense_t* a,
                  const lv_zz_px_dense_t* b);

/* 多项式除法 */
void lv_zz_px_div(lv_zz_px_dense_t* q,
                  lv_zz_px_dense_t* r,
                  const lv_zz_px_dense_t* a,
                  const lv_zz_px_dense_t* b);

/* 多项式 GCD */
void lv_zz_px_gcd(lv_zz_px_dense_t* g,
                  const lv_zz_px_dense_t* a,
                  const lv_zz_px_dense_t* b);

/* 求值和插值 */
void lv_zz_px_eval(lv_zz_p_t* result,
                   const lv_zz_px_dense_t* poly,
                   const lv_zz_p_t* point);

#endif /* LV_ZZ_PX_H */
```

#### 3.2.4 FFT 实现

```c
/*
 * 数论变换 (NTT) 实现
 * 借鉴 NTL 的 FFT 模块
 */

#ifndef LV_NTT_H
#define LV_NTT_H

#include "lv_zz_p.h"

/* NTT 上下文 */
typedef struct {
    lv_zz_t modulus;
    lv_zz_t primitive_root;
    int max_log_n;
    lv_zz_p_t** roots;  /* 预计算的根 */
    bool initialized;
} lv_ntt_context_t;

/* 初始化 NTT 上下文 - 需要形如 c*2^k+1 的素数 */
int lv_ntt_init(lv_ntt_context_t* ctx, const lv_zz_t* modulus, 
                const lv_zz_t* root, int max_log_n);
void lv_ntt_cleanup(lv_ntt_context_t* ctx);

/* 原地 NTT 变换 */
void lv_ntt_forward(lv_zz_p_t* data, int n, const lv_ntt_context_t* ctx);
void lv_ntt_inverse(lv_zz_p_t* data, int n, const lv_ntt_context_t* ctx);

/* 卷积运算 */
void lv_ntt_convolve(lv_zz_p_t* result,
                     const lv_zz_p_t* a,
                     const lv_zz_p_t* b,
                     int n,
                     const lv_ntt_context_t* ctx);

/* 常用 NTT 友好素数 */
#define LV_NTT_PRIME_998244353 998244353LL  /* 119 * 2^23 + 1 */
#define LV_NTT_PRIME_1004535809 1004535809LL /* 479 * 2^21 + 1 */
#define LV_NTT_PRIME_104857601 104857601LL  /* 25 * 2^22 + 1 */

#endif /* LV_NTT_H */
```

#### 3.2.5 矩阵运算

```c
/*
 * 整数矩阵运算
 * 借鉴 NTL 的 mat_ZZ 实现
 */

#ifndef LV_MAT_ZZ_H
#define LV_MAT_ZZ_H

#include "lv_zz.h"

typedef struct {
    int rows;
    int cols;
    lv_zz_t** data;  /* data[i][j] 表示第 i 行第 j 列 */
    int trust_level;
} lv_mat_zz_t;

/* 矩阵生命周期 */
void lv_mat_zz_init(lv_mat_zz_t* mat, int rows, int cols);
void lv_mat_zz_clear(lv_mat_zz_t* mat);

/* 元素访问 */
lv_zz_t* lv_mat_zz_entry(lv_mat_zz_t* mat, int i, int j);
const lv_zz_t* lv_mat_zz_entry_const(const lv_mat_zz_t* mat, int i, int j);

/* 矩阵运算 */
void lv_mat_zz_add(lv_mat_zz_t* result,
                   const lv_mat_zz_t* a,
                   const lv_mat_zz_t* b);
void lv_mat_zz_sub(lv_mat_zz_t* result,
                   const lv_mat_zz_t* a,
                   const lv_mat_zz_t* b);
void lv_mat_zz_mul(lv_mat_zz_t* result,
                   const lv_mat_zz_t* a,
                   const lv_mat_zz_t* b);
void lv_mat_zz_mul_vec(lv_zz_t* result,
                       const lv_mat_zz_t* mat,
                       const lv_zz_t* vec,
                       int vec_len);

/* 标量运算 */
void lv_mat_zz_mul_scalar(lv_mat_zz_t* result,
                          const lv_mat_zz_t* mat,
                          const lv_zz_t* scalar);

/* 矩阵属性 */
void lv_mat_zz_det(lv_zz_t* det, const lv_mat_zz_t* mat);
int lv_mat_zz_rank(const lv_mat_zz_t* mat);
void lv_mat_zz_trace(lv_zz_t* trace, const lv_mat_zz_t* mat);

/* 线性方程组求解 */
int lv_mat_zz_solve(lv_zz_t* solution,
                    const lv_mat_zz_t* a,
                    const lv_zz_t* b,
                    int n);

/* 矩阵转置和逆 */
void lv_mat_zz_transpose(lv_mat_zz_t* result, const lv_mat_zz_t* mat);
int lv_mat_zz_invert(lv_mat_zz_t* result, const lv_mat_zz_t* mat);

/* 特殊矩阵 */
void lv_mat_zz_identity(lv_mat_zz_t* mat, int n);
void lv_mat_zz_zero(lv_mat_zz_t* mat);

#endif /* LV_MAT_ZZ_H */
```

#### 3.2.6 LLL 格基约化

```c
/*
 * LLL 格基约化算法
 * 借鉴 NTL 的 LLL 实现
 */

#ifndef LV_LLL_H
#define LV_LLL_H

#include "lv_mat_zz.h"

/* LLL 算法参数 */
typedef struct {
    double delta;      /* 约化参数，通常 0.75 <= delta < 1 */
    double eta;        /* 大小约化参数，通常 0.5 <= eta < sqrt(delta) */
    int precision;     /* 浮点精度（位） */
    int deep;          /* 是否使用 deep insertion */
    int block_size;    /* BKZ 块大小（BKZ 算法使用） */
} lv_lll_params_t;

/* 默认参数 */
#define LV_LLL_DEFAULT_DELTA 0.99
#define LV_LLL_DEFAULT_ETA 0.51

/* 初始化默认参数 */
void lv_lll_params_init_default(lv_lll_params_t* params);

/* LLL 约化 - 原地修改矩阵 */
int lv_lll_reduce(lv_mat_zz_t* basis, const lv_lll_params_t* params);

/* LLL 约化 - 返回变换矩阵 */
int lv_lll_reduce_with_transform(lv_mat_zz_t* basis,
                                  lv_mat_zz_t* transform,
                                  const lv_lll_params_t* params);

/* BKZ 约化 */
int lv_bkz_reduce(lv_mat_zz_t* basis, 
                  int block_size,
                  const lv_lll_params_t* params);

/* 计算 Gram-Schmidt 正交化 */
void lv_lll_gram_schmidt(lv_mat_zz_t* mu,      /* 系数矩阵 */
                         lv_zz_t* b_star_norm, /* 正交向量范数平方 */
                         const lv_mat_zz_t* basis);

/* 格向量长度 */
void lv_lll_vector_norm(lv_zz_t* norm_sq, const lv_zz_t* vec, int dim);

/* 最短向量近似 */
int lv_lll_shortest_vector(lv_zz_t* shortest,
                           const lv_mat_zz_t* basis,
                           int dim);

/* 最近向量问题（CVP）近似 */
int lv_lll_closest_vector(lv_zz_t* closest,
                          const lv_mat_zz_t* basis,
                          const lv_zz_t* target,
                          int dim);

#endif /* LV_LLL_H */
```

### 3.3 信任颜色系统集成

将 Lv-00 的信任颜色系统与数论运算集成：

```c
/*
 * 信任颜色与数论运算的集成
 */

#ifndef LV_TRUST_ARITH_H
#define LV_TRUST_ARITH_H

#include "lv_zz.h"

/* 信任级别定义 */
#define LV_TRUST_COMPUTED  0  /* 计算结果 */
#define LV_TRUST_ASSUMED   1  /* 假设 */
#define LV_TRUST_AXIOM     2  /* 公理 */
#define LV_TRUST_PROVEN    3  /* 已证明 */

/* 信任颜色传播规则 */
int lv_trust_combine(int trust_a, int trust_b);
int lv_trust_propagate_unary(int trust_in, int operation_trust);
int lv_trust_propagate_binary(int trust_a, int trust_b, int operation_trust);

/* 带信任传播的运算包装 */
void lv_zz_trusted_add(lv_zz_t* result,
                       const lv_zz_t* a,
                       const lv_zz_t* b,
                       int operation_trust);

void lv_zz_trusted_mul(lv_zz_t* result,
                       const lv_zz_t* a,
                       const lv_zz_t* b,
                       int operation_trust);

/* 证明记录 */
typedef struct {
    char* theorem_name;
    char* proof_steps;
    int trust_level;
} lv_proof_record_t;

void lv_zz_attach_proof(lv_zz_t* z, const lv_proof_record_t* proof);
const lv_proof_record_t* lv_zz_get_proof(const lv_zz_t* z);

#endif /* LV_TRUST_ARITH_H */
```

---

## 4. 实现路线图

### 4.1 短期目标（1-3 个月）

| 阶段 | 任务 | 交付物 | 依赖 |
|------|------|--------|------|
| 1.1 | GMP 封装层 | lv_zz.h/c - 大整数基本运算 | GMP 库 |
| 1.2 | 模运算上下文 | lv_zz_p.h/c - 模 p 整数 | 1.1 |
| 1.3 | 稠密多项式 | lv_zz_px.h/c - 基础多项式运算 | 1.2 |
| 1.4 | 经典乘法 | 经典 O(n^2) 多项式乘法 | 1.3 |
| 1.5 | 单元测试 | 基础测试套件 | 1.1-1.4 |

**短期里程碑**：完成基础数论类型的 C 语言实现，支持任意精度整数、模运算和多项式算术。

### 4.2 中期目标（3-6 个月）

| 阶段 | 任务 | 交付物 | 依赖 |
|------|------|--------|------|
| 2.1 | Karatsuba 乘法 | 快速整数和多项式乘法 | 1.4 |
| 2.2 | NTT 实现 | lv_ntt.h/c - 数论变换 | 1.2 |
| 2.3 | FFT 多项式乘法 | O(n log n) 多项式乘法 | 2.2 |
| 2.4 | 矩阵运算 | lv_mat_zz.h/c - 整数矩阵 | 1.1 |
| 2.5 | 多项式 GCD | 扩展欧几里得算法 | 1.3 |
| 2.6 | 素性测试 | Miller-Rabin 测试 | 1.2 |
| 2.7 | 信任系统集成 | 信任颜色传播机制 | 全部 |

**中期里程碑**：实现高性能算法（FFT、Karatsuba），完成矩阵运算和基础数论函数。

### 4.3 长期目标（6-12 个月）

| 阶段 | 任务 | 交付物 | 依赖 |
|------|------|--------|------|
| 3.1 | LLL 算法 | lv_lll.h/c - 格基约化 | 2.4 |
| 3.2 | BKZ 算法 | 块 Korkin-Zolotarev | 3.1 |
| 3.3 | 多项式分解 | 有限域上多项式因式分解 | 2.3, 2.6 |
| 3.4 | GF(2) 多项式 | GF2X 等效实现 | 2.3 |
| 3.5 | 并行优化 | 多线程支持 | 全部 |
| 3.6 | 性能基准 | 与 NTL 性能对比 | 全部 |

**长期里程碑**：完成高级算法（LLL、多项式分解），实现与 NTL 相当的功能覆盖。

### 4.4 依赖关系图

```
第 1 层基础
-----------
GMP 封装 (lv_zz)
    |
    +---> 模运算 (lv_zz_p)
    |         |
    |         +---> 模多项式 (lv_zz_px)
    |         |         |
    |         |         +---> NTT (lv_ntt)
    |         |         |         |
    |         |         |         +---> FFT 乘法
    |         |         |
    |         |         +---> 多项式 GCD
    |         |
    |         +---> 素性测试
    |
    +---> 整数矩阵 (lv_mat_zz)
              |
              +---> LLL 格基约化
                        |
                        +---> BKZ 算法
```

---

## 5. 附录

### 5.1 关键 API 列表

#### 5.1.1 ZZ 大整数 API

| 函数 | 签名 | 说明 |
|------|------|------|
| lv_zz_init | void lv_zz_init(lv_zz_t* z) | 初始化大整数 |
| lv_zz_clear | void lv_zz_clear(lv_zz_t* z) | 清理大整数 |
| lv_zz_add | void lv_zz_add(lv_zz_t* r, const lv_zz_t* a, const lv_zz_t* b) | 加法 |
| lv_zz_sub | void lv_zz_sub(lv_zz_t* r, const lv_zz_t* a, const lv_zz_t* b) | 减法 |
| lv_zz_mul | void lv_zz_mul(lv_zz_t* r, const lv_zz_t* a, const lv_zz_t* b) | 乘法 |
| lv_zz_gcd | void lv_zz_gcd(lv_zz_t* g, const lv_zz_t* a, const lv_zz_t* b) | 最大公约数 |
| lv_zz_powm | void lv_zz_powm(lv_zz_t* r, const lv_zz_t* b, const lv_zz_t* e, const lv_zz_t* m) | 模幂运算 |

#### 5.1.2 ZZ_p 模运算 API

| 函数 | 签名 | 说明 |
|------|------|------|
| lv_zz_p_init | int lv_zz_p_init(const lv_zz_t* p) | 初始化模数上下文 |
| lv_zz_p_cleanup | void lv_zz_p_cleanup(void) | 清理模数上下文 |
| lv_zz_p_add | void lv_zz_p_add(lv_zz_p_t* r, const lv_zz_p_t* a, const lv_zz_p_t* b) | 模加法 |
| lv_zz_p_mul | void lv_zz_p_mul(lv_zz_p_t* r, const lv_zz_p_t* a, const lv_zz_p_t* b) | 模乘法 |
| lv_zz_p_invert | int lv_zz_p_invert(lv_zz_p_t* r, const lv_zz_p_t* a) | 模逆元 |

#### 5.1.3 ZZ_pX 多项式 API

| 函数 | 签名 | 说明 |
|------|------|------|
| lv_zz_px_init | void lv_zz_px_init(lv_zz_px_dense_t* p) | 初始化多项式 |
| lv_zz_px_mul | void lv_zz_px_mul(lv_zz_px_dense_t* r, const lv_zz_px_dense_t* a, const lv_zz_px_dense_t* b) | 多项式乘法 |
| lv_zz_px_div | void lv_zz_px_div(lv_zz_px_dense_t* q, lv_zz_px_dense_t* r, const lv_zz_px_dense_t* a, const lv_zz_px_dense_t* b) | 多项式除法 |
| lv_zz_px_gcd | void lv_zz_px_gcd(lv_zz_px_dense_t* g, const lv_zz_px_dense_t* a, const lv_zz_px_dense_t* b) | 多项式 GCD |

#### 5.1.4 mat_ZZ 矩阵 API

| 函数 | 签名 | 说明 |
|------|------|------|
| lv_mat_zz_init | void lv_mat_zz_init(lv_mat_zz_t* m, int rows, int cols) | 初始化矩阵 |
| lv_mat_zz_mul | void lv_mat_zz_mul(lv_mat_zz_t* r, const lv_mat_zz_t* a, const lv_mat_zz_t* b) | 矩阵乘法 |
| lv_mat_zz_det | void lv_mat_zz_det(lv_zz_t* d, const lv_mat_zz_t* m) | 行列式 |
| lv_mat_zz_rank | int lv_mat_zz_rank(const lv_mat_zz_t* m) | 矩阵秩 |
| lv_mat_zz_invert | int lv_mat_zz_invert(lv_mat_zz_t* r, const lv_mat_zz_t* m) | 矩阵求逆 |

#### 5.1.5 LLL API

| 函数 | 签名 | 说明 |
|------|------|------|
| lv_lll_reduce | int lv_lll_reduce(lv_mat_zz_t* b, const lv_lll_params_t* p) | LLL 约化 |
| lv_bkz_reduce | int lv_bkz_reduce(lv_mat_zz_t* b, int bs, const lv_lll_params_t* p) | BKZ 约化 |
| lv_lll_shortest_vector | int lv_lll_shortest_vector(lv_zz_t* sv, const lv_mat_zz_t* b, int d) | 最短向量 |

### 5.2 参考文献

#### 5.2.1 NTL 官方资源

1. Shoup, V. (2024). *NTL: A Library for doing Number Theory*. https://libntl.org/
2. Shoup, V. (2009). NTL Reference Manual. https://libntl.org/doc/tour.html

#### 5.2.2 算法参考文献

3. Lenstra, A. K., Lenstra, H. W., & Lovasz, L. (1982). Factoring polynomials with rational coefficients. *Mathematische Annalen*, 261(4), 515-534. (LLL 算法原始论文)

4. Schnorr, C. P., & Euchner, M. (1994). Lattice basis reduction: Improved practical algorithms and solving subset sum problems. *Mathematical Programming*, 66(1-3), 181-199. (BKZ 算法)

5. Von Zur Gathen, J., & Gerhard, J. (2013). *Modern Computer Algebra* (3rd ed.). Cambridge University Press. (多项式算术)

6. Cohen, H. (1993). *A Course in Computational Algebraic Number Theory*. Springer. (计算数论)

7. Brent, R. P., & Zimmermann, P. (2010). *Modern Computer Arithmetic*. Cambridge University Press. (大整数算术)

#### 5.2.3 密码学应用

8. Shoup, V. (2009). On the security of ECDSA. In *Advances in Elliptic Curve Cryptography* (pp. 123-143). Cambridge University Press.

9. Micciancio, D., & Regev, O. (2009). Lattice-based cryptography. In *Post-Quantum Cryptography* (pp. 147-191). Springer.

#### 5.2.4 格基约化应用

10. Nguyen, P. Q., & Vallee, B. (Eds.). (2010). *The LLL Algorithm: Survey and Applications*. Springer.

11. Hanrot, G., Pujol, X., & Stehle, D. (2011). Analyzing blockwise lattice algorithms using dynamical systems. In *CRYPTO 2011* (pp. 447-464).

#### 5.2.5 相关软件库

12. Granlund, T., & the GMP development team. (2023). *GNU MP: The GNU Multiple Precision Arithmetic Library*. https://gmplib.org/

13. The PARI Group. (2024). *PARI/GP version 2.15*. Bordeaux. https://pari.math.u-bordeaux.fr/

14. The SageMath Developers. (2024). *SageMath Version 10.2*. https://www.sagemath.org/

15. Halevi, S., & Shoup, V. (2020). HElib. https://github.com/homenc/HElib (基于 NTL 的同态加密库)

---

## 文档信息

- **版本**：1.0
- **创建日期**：2026-05-25
- **作者**：Lv-00 开发团队
- **审核状态**：待审核
- **相关文档**：
  - Lv-00 架构设计文档
  - 第 1 层数值计算规范
  - GMP 集成指南

# Lv-00 参考设计：SUNDIALS 高性能微分方程求解器——自适应步长、多后端抽象与事件检测

> **版本**: 1.0.0
> **日期**: 2026-05-24
> **参考**: [SUNDIALS](https://github.com/LLNL/sundials) —— 劳伦斯利弗莫尔国家实验室开发的高性能微分方程求解器套件
> **目标**: 借鉴 SUNDIALS 的自适应步长误差控制、SUNMatrix/SUNLinearSolver 线性求解器抽象层、N_Vector 异构计算后端和事件检测机制，为 Lv-00 的几何演化精度控制、多后端数值求解、异构计算路径和几何事件检测提供系统化工程方案

---

## 目录

1. [项目概述](#1-项目概述)
2. [核心借鉴要点](#2-核心借鉴要点)
3. [Lv-00 映射方案](#3-lv-00-映射方案)
4. [实现路线图](#4-实现路线图)
5. [附录](#5-附录)

---

## 1. 项目概述

### 1.1 SUNDIALS 是什么

SUNDIALS（**S**uite of **N**onlinear and **D**ifferential/Algebraic Equation Solvers）是由美国劳伦斯利弗莫尔国家实验室（LLNL）开发的高性能微分方程求解器套件。该项目始于 1990 年代，经过近三十年的持续演进，已成为科学计算领域求解常微分方程（ODE）和微分代数方程（DAE）的事实标准之一。LLNL 同时也是高性能计算（HPC）领域的旗舰机构，SUNDIALS 的代码质量、数值稳定性和并行扩展能力代表了该领域的最高水准。

SUNDIALS 包含六个核心求解器模块：

1. **CVODE**（C Variable-coefficient ODE solver）：求解常微分方程初值问题 y' = f(t,y)。采用变阶变步长线性多步法（BDF）和隐式 Adams 方法，逐时间步自适应调整步长和阶数以在给定容差范围内控制局部截断误差。广泛用于化学反应动力学、电路仿真和天体力学。

2. **CVODES**（CVODE with Sensitivity Analysis）：在 CVODE 基础上增加了前向灵敏度分析和伴随灵敏度分析能力。前向灵敏度分析同时求解原始 ODE 和灵敏度方程 d/dt(dy/dp) = ...，用于参数敏感性研究、不确定性量化和优化中的梯度计算。

3. **IDA**（Implicit Differential-Algebraic solver）：求解微分代数方程 F(t,y,y')=0。DAE 比 ODE 更普遍——许多物理系统的自然建模形式就是 DAE（如带约束的力学系统、电路网络的改进节点分析）。IDA 使用变阶变步长 BDF 方法。

4. **IDAS**（IDA with Sensitivity Analysis）：在 IDA 基础上增加了前向和伴随灵敏度分析。

5. **ARKODE**（Adaptive-step Additive Runge-Kutta）：求解可分离为刚性部分和非刚性部分的加性 ODE 系统 y' = f_E(t,y) + f_I(t,y)，其中 f_E 用显式方法处理，f_I 用隐式方法处理（IMEX, Implicit-Explicit）。ARKODE 提供了一套丰富的嵌入式 Runge-Kutta 方法表。

6. **KINSOL**（Krylov Inexact Newton Solver）：求解非线性代数方程组 F(u)=0。采用 Krylov 子空间方法逼近 Newton 步，避免显式构造和分解 Jacobian 矩阵。适用于大规模稀疏非线性系统。

SUNDIALS 的架构最显著特征是**三层抽象设计**：

```
应用层（Application Layer）
  └── 用户 ODE/DAE 右手边函数 + Jacobian 函数
          ↓
求解器层（Solver Layer）
  ├── CVODE / CVODES / IDA / IDAS / ARKODE / KINSOL
  ├── 自适应步长控制 + 误差估计
  └── 事件检测（根查找）
          ↓
向量/矩阵/线性求解器抽象层（N_Vector / SUNMatrix / SUNLinearSolver）
  ├── 串行后端（NVECTOR_SERIAL, SUNMATRIX_DENSE, SUNLINSOL_DENSE）
  ├── 并行后端（NVECTOR_PARALLEL, NVECTOR_OPENMP, NVECTOR_PTHREADS）
  ├── GPU 后端（NVECTOR_CUDA, NVECTOR_HIP, SUNMATRIX_CUSPARSE, SUNLINSOL_CUSOLVER）
  └── 自定义后端（用户可自行实现 N_Vector 虚函数表）
```

这种三层抽象使得 SUNDIALS 的求解器代码与底层线性代数实现完全解耦——同一个 CVODE 求解器可以在串行 CPU、多核 OpenMP/MPI 和 NVIDIA/AMD GPU 上运行，只需切换向量和线性求解器后端即可。

### 1.2 为什么借鉴 SUNDIALS

Lv-00 的几何系统在以下场景中与微分方程求解产生交集：

- **几何演化与流形追踪**：当几何约束涉及微分关系时（如"点 P 沿曲线 C 以恒定速度运动"、"两曲线的包络线"、"测地线追踪"），需要常微分方程求解器来推进几何状态的时间演化。

- **参数灵敏度分析**：用户调节约束参数（如线段长度、角度值）时，需要理解几何配置如何随参数变化——这正是 CVODES/IDAS 灵敏度分析解决的问题。

- **异构计算支持**：Lv-00 的数值计算（Groebner 基、代数系统化简、大矩阵运算）在不同硬件平台上应能透明切换后端——SUNDIALS 的 N_Vector/SUNMatrix/SUNLinearSolver 抽象层是这一需求的参考实现。

- **非线性方程组求解**：Lv-00 的几何约束系统（如大量非线性等式和不等式约束同时满足）在消元后归结为非线性代数方程组——KINSOL 的 Krylov 迭代方法对此类大规模稀疏问题特别有效。

- **事件检测**：几何交互事件——如两条曲线何时相交、一个点何时进入某个区域——本质上是求解器推进过程中在特定时间点检测函数符号变化（rootfinding）的问题。SUNDIALS 内置的根查找机制是这一功能的标准实现。

当前 Lv-00 对这些场景尚无系统化支持——缺少自适应步长的时间推进、缺少多后端的数值线性代数、缺少参数灵敏度和事件检测能力。SUNDIALS 为每个需求都提供了经过充分验证的参考实现。

---

## 2. 核心借鉴要点

### 2.1 自适应步长 + 误差控制

SUNDIALS 的自适应步长控制是其精度保证的核心。CVODE/IDA/ARKODE 均使用以下策略：

```
1. 预估步长 h：基于上一步的成功步长和外推
2. 尝试步：以当前阶数执行一步积分
3. 误差估计：比较不同阶方法的解，得到局部截断误差估计 e_n
4. 误差测试：‖e_n‖_WRMS ≤ 1（加权均方根误差在容差内）
   - 若通过 → 接受步，更新解，可能提高阶数或增大下一步步长
   - 若不通过 → 拒绝步，减小步长，重新尝试（可能降低阶数）
5. 步长调整公式（经典 PI 控制器）：
   h_new = h * safety * (1/‖e_n‖_WRMS)^(1/(order+1))
```

对 Lv-00 的映射——几何演化/流形追踪的精度控制：

| SUNDIALS 概念 | Lv-00 映射 | 几何语义 |
|:---|:---|:---|
| 时间步长 h | 几何演化步长（沿参数曲线移动的距离增量 Δs） | 控制每次几何推进的距离 |
| 局部截断误差 e_n | 几何位置误差（当前位置与真实流形上对应点的偏差） | 确保几何演化不偏离约束流形 |
| WRMS 范数 | 加权几何误差范数——各坐标分量按目标精度加权 | x 和 y 坐标可以有不同的精度要求 |
| 误差测试 ‖e_n‖ ≤ 1 | 几何精度检查——位置误差是否在设计容差内 | `geoevol_check_tolerance()` |
| 步长接受/拒绝 | 几何推进的接受/拒绝——若偏离流形太远则回退 | `geoevol_accept_step()` / `geoevol_reject_step()` |
| 阶数调整 | 几何推进方法的阶数切换（线性外推 vs 二次外推 vs 曲率预测） | 根据局部曲率选择推进方法的复杂度 |
| PI 步长控制器 | 几何步长自适应控制器 | `geoevol_pi_controller()` |

### 2.2 SUNMatrix/SUNLinearSolver 线性求解器抽象

SUNDIALS 的 SUNMatrix 和 SUNLinearSolver 是其多后端架构的核心。这两个抽象类型使用虚函数表（C 语言中的函数指针结构体）实现多态：

```c
/* SUNDIALS 的线性求解器抽象（C 语言虚函数表模式） */
struct _SUNLinearSolver_Ops {
    SUNLinearSolverType (*gettype)(SUNLinearSolver);
    int (*setup)(SUNLinearSolver, SUNMatrix);
    int (*solve)(SUNLinearSolver, SUNMatrix, N_Vector, N_Vector, realtype);
    int (*free)(SUNLinearSolver);
    /* ... 更多操作 ... */
};
```

对 Lv-00 的映射——多后端数值求解：

| 抽象层 | SUNDIALS 实现 | Lv-00 映射 | 用途 |
|:---|:---|:---|:---|
| **N_Vector** | `NVECTOR_SERIAL`, `NVECTOR_CUDA`, `NVECTOR_HIP`, `NVECTOR_OPENMP` | `Lv00Vector`（虚函数表：`clone`, `dot`, `axpy`, `scale` 等） | 向量运算的硬件抽象 |
| **SUNMatrix** | `SUNMATRIX_DENSE`, `SUNMATRIX_BAND`, `SUNMATRIX_SPARSE`, `SUNMATRIX_CUSPARSE` | `Lv00Matrix`（虚函数表：`matvec`, `solve`, `factor` 等） | 矩阵运算和分解的硬件抽象 |
| **SUNLinearSolver** | `SUNLINSOL_DENSE`（LU）, `SUNLINSOL_SPGMR`（GMRES）, `SUNLINSOL_CUSOLVER` | `Lv00LinearSolver`（虚函数表：`setup`, `solve`, `free`） | 线性方程组求解的算法抽象 |
| **后端切换** | 编译时或运行时选择 | Lv-00 的 `lv00_vector_create(NVEC_CUDA, n)` 工厂函数 | 用户代码无需修改即可切换硬件 |
| **自定义后端** | 用户实现 NVECTOR 的虚函数表 | Lv-00 用户实现 `Lv00VectorOps` 结构体 | 支持实验性硬件和特殊数值格式 |

### 2.3 N_Vector 向量抽象 + CUDA/HIP 后端

SUNDIALS 的 N_Vector 是异构计算的核心抽象。其设计要点：

```
N_Vector 虚函数表（C 语言，约 30 个操作）：
  ├── 线性代数核心操作：clone, dot, norm, scale, axpy, linear_sum
  ├── 逐元素操作：abs, inv, addconst, compare, div
  ├── 归约操作：maxnorm, min, wl2norm, wrmsnorm
  ├── 通信操作：space（获取存储空间指针）, getlength
  └── 生命周期：destroy, const
```

对于 Lv-00，这个抽象的借鉴意义在于：

1. **求解器代码与硬件无关**：Groebner 基计算中的多项式系数向量（GMP `mpq_t` 数组）可在 CPU、CUDA GPU 或 HIP GPU 上运算，通过统一的 `Lv00Vector` 接口访问。

2. **运行时后端选择**：在交互式几何编辑（低延迟要求，使用 GPU BLAS 加速）和批量证明检查（高吞吐量，使用多核 CPU）之间动态切换后端。

3. **混合精度支持**：部分向量运算可用 `float` 或 `double` 加速（如误差估计的试探步），最终结果使用 GMP 精确有理数——`Lv00Vector` 可封装不同的数值类型。

### 2.4 前向灵敏度分析（CVODES/IDAS）

CVODES 和 IDAS 的前向灵敏度分析同时求解原始方程和灵敏度方程：

```
原始 ODE:    dy/dt = f(t, y, p)      其中 p 为参数向量
灵敏度 ODE:  d/dt (dy/dp_j) = (∂f/∂y)(dy/dp_j) + (∂f/∂p_j)

通过增广系统同时求解：
  Y_aug = [y, dy/dp_1, dy/dp_2, ..., dy/dp_k]
  F_aug = [f, d/dt(dy/dp_1), ..., d/dt(dy/dp_k)]
```

对 Lv-00 几何约束参数灵敏度的映射：

| CVODES 概念 | Lv-00 映射 | 说明 |
|:---|:---|:---|
| 参数向量 p | 几何约束参数（线段长度、角度值、缩放因子） | `SensitivityParams` |
| 状态 y(t) | 几何配置（所有点的坐标向量） | `GeometricState` |
| 灵敏度 dy/dp_j | 几何配置对参数 j 的偏导数矩阵 | `GeometricSensitivity` |
| 前向灵敏度方法 | 同时求解几何状态和对参数的导数 | `geo_sensitivity_forward()` |
| 伴随灵敏度方法 | 反向传播梯度以高效计算多个输出对参数的敏感度 | `geo_sensitivity_adjoint()` |
| 灵敏度应用于优化 | 利用梯度信息加速几何约束优化 | 梯度下降调整参数以满足目标配置 |

### 2.5 KINSOL 非线性代数方程求解

KINSOL 求解 F(u)=0 型非线性代数方程组。其核心是 Krylov 子空间方法（GMRES/FGMRES/BiCGStab）近似求解 Newton 修正方程 J·δu = -F(u)，避免显式构造 Jacobian 矩阵 J。对于 Lv-00 的几何非线性系统：

| KINSOL 概念 | Lv-00 映射 | 说明 |
|:---|:---|:---|
| 残差函数 F(u) | 几何约束残差——所有约束方程的偏差向量 | `geometric_residual()` |
| 未知向量 u | 所有自由几何变量的集合 | `geometric_variables()` |
| Newton 步 δu | 几何变量修正量 | `geometric_newton_step()` |
| Jacobian-向量乘积 J(v) | 通过有限差分或解析微分计算 | `geometric_jacobian_times_vector()` |
| Krylov 子空间维度 | 控制迭代空间的大小 | `kinsol_maxl` 参数 |
| 强制项（Forcing Term） | Newton 迭代的收敛容差序列 | `kinsol_eta` 参数 |
| 线搜索（Line Search） | 全局化策略——沿 Newton 方向的步长回退 | `geometric_line_search()` |

### 2.6 事件检测（Rootfinding）机制

SUNDIALS 的 CVODE 和 IDA 内置根查找（rootfinding）功能，用于检测事件发生的时间点：

```
根查找流程：
1. 用户在积分器上注册 N 个事件函数 g_i(t, y)
2. 每步积分完成后，检查所有 g_i 在当前步区间 [t_n, t_{n+1}] 上是否穿越零点
3. 若检测到穿越 → 使用改进的 Illinois 或 Brent 方法精确定位根（事件时间 t*）
4. 报告事件——调用用户回调函数，用户可以：
   - 停止积分（STOP）
   - 继续积分（CONTINUE）
```

对 Lv-00 几何事件检测的映射：

| SUNDIALS 概念 | Lv-00 映射 | 说明 |
|:---|:---|:---|
| 事件函数 g_i(t, y) | 几何事件检测函数 | 如"两曲线交点"检测：g(t) = distance(curve1(t), curve2(t)) |
| 零点穿越（Zero Crossing） | 几何事件的触发条件 | g(t_n) 和 g(t_{n+1}) 符号不同 |
| 根精确定位 | 事件时间/参数的精确计算 | 使用 Brent 方法找到精确的交点坐标 |
| Illinois/Brent 算法 | 基于符号变化的求根方法 | `geo_rootfind_illinois()` / `geo_rootfind_brent()` |
| 事件回调 | 几何事件处理器 | 如检测到碰撞 → 触发反弹力学、记录交点 |
| 事件方向约束 | 仅检测正向穿越或负向穿越 | 如仅检测点"进入"区域而非"离开"区域 |

### 2.7 核心借鉴对照表

| 借鉴维度 | SUNDIALS 原实现 | Lv-00 映射目标 | 优先级 | 难度 |
|:---|:---|:---|:---|:---|
| 自适应步长+误差控制 | CVODE/IDA 的变阶变步长 PI 控制器 | `Lv00GeomEvol` 几何演化步长自适应控制器 | P1 最高 | 中 |
| SUNMatrix/SUNLinearSolver 抽象 | 虚函数表多态 + 预构建 8 种后端 | `Lv00Matrix` / `Lv00LinearSolver` 多后端数值求解 | P1 最高 | 中 |
| N_Vector 异构后端 | NVECTOR_SERIAL / CUDA / HIP / OpenMP / Pthreads | `Lv00Vector` CPU / CUDA / HIP 后端 | P2 高 | 高 |
| 前向灵敏度分析 | CVODES 同时求解状态和灵敏度 | `Lv00GeomSensitivity` 几何约束参数灵敏度 | P2 高 | 高 |
| KINSOL 非线性求解 | Krylov 迭代 Newton 法 | `lv00_kinsol_solve()` 几何非线性方程组 | P2 高 | 中 |
| 事件检测（Rootfinding） | Illinois/Brent 算法零点穿越检测 | `lv00_event_detect()` 几何交叉/接触事件 | P1 最高 | 中 |
| 增广系统求解 | CVODES 将灵敏度方程附加到状态向量 | `Lv00AugmentedSystem` 同时求解多个相关系统 | P3 中 | 中 |
| 伴随灵敏度 | IDAS 反向传播灵敏度 | `Lv00AdjointSensitivity` 梯度反向传播 | P4 低 | 高 |

---

## 3. Lv-00 映射方案

### 3.1 多后端向量/矩阵/线性求解器抽象

借鉴 SUNDIALS 的 SUN 抽象层，为 Lv-00 设计统一的多后端数值计算接口：

```c
/* === lv00_numerical_backend.h — Lv-00 多后端数值抽象层 === */

#ifndef LV00_NUMERICAL_BACKEND_H
#define LV00_NUMERICAL_BACKEND_H

#include <stdlib.h>
#include <stdbool.h>
#include <gmp.h>

/* ── 后端类型枚举 ── */
typedef enum {
    LV00_BACKEND_SERIAL = 0,   /* CPU 串行（默认） */
    LV00_BACKEND_OPENMP,       /* CPU 多核 OpenMP */
    LV00_BACKEND_CUDA,         /* NVIDIA GPU */
    LV00_BACKEND_HIP,          /* AMD GPU */
    LV00_BACKEND_CUSTOM        /* 用户自定义后端 */
} Lv00BackendType;

/* ── N_Vector 抽象 ── */
typedef struct Lv00Vector Lv00Vector;
typedef struct Lv00VectorOps {
    Lv00Vector* (*clone)(const Lv00Vector *w);
    void        (*destroy)(Lv00Vector *v);
    void        (*zero)(Lv00Vector *v);
    void        (*copy)(Lv00Vector *y, const Lv00Vector *x);
    void        (*scale)(double a, Lv00Vector *y, const Lv00Vector *x);
    void        (*linear_sum)(double a, const Lv00Vector *x,
                              double b, const Lv00Vector *y, Lv00Vector *z);
    double      (*dot)(const Lv00Vector *x, const Lv00Vector *y);
    double      (*norm)(const Lv00Vector *x, int p);
    double      (*wrms_norm)(const Lv00Vector *x, const Lv00Vector *w);
    void        (*set_element)(Lv00Vector *v, size_t i, double val);
    double      (*get_element)(const Lv00Vector *v, size_t i);
    size_t      (*length)(const Lv00Vector *v);
} Lv00VectorOps;

struct Lv00Vector {
    Lv00VectorOps *ops;
    void *content;       /* 后端特定的数据 */
    Lv00BackendType backend;
};

/* ── SUNMatrix 抽象 ── */
typedef struct Lv00Matrix Lv00Matrix;
typedef struct Lv00MatrixOps {
    Lv00Matrix* (*clone)(const Lv00Matrix *A);
    void        (*destroy)(Lv00Matrix *A);
    void        (*zero)(Lv00Matrix *A);
    void        (*copy)(Lv00Matrix *B, const Lv00Matrix *A);
    void        (*matvec)(Lv00Matrix *A, const Lv00Vector *x, Lv00Vector *y);
    int         (*factor)(Lv00Matrix *A); /* LU/Cholesky/ILU 分解 */
    int         (*solve)(Lv00Matrix *A, Lv00Vector *x, const Lv00Vector *b);
    size_t      (*rows)(const Lv00Matrix *A);
    size_t      (*cols)(const Lv00Matrix *A);
} Lv00MatrixOps;

struct Lv00Matrix {
    Lv00MatrixOps *ops;
    void *content;
    Lv00BackendType backend;
};

/* ── SUNLinearSolver 抽象 ── */
typedef struct Lv00LinearSolver Lv00LinearSolver;
typedef struct Lv00LinearSolverOps {
    int  (*setup)(Lv00LinearSolver *S, Lv00Matrix *A);
    int  (*solve)(Lv00LinearSolver *S, Lv00Matrix *A,
                  Lv00Vector *x, const Lv00Vector *b, double tol);
    void (*destroy)(Lv00LinearSolver *S);
} Lv00LinearSolverOps;

struct Lv00LinearSolver {
    Lv00LinearSolverOps *ops;
    void *content;
    Lv00BackendType backend;
    /* 迭代方法参数 */
    int max_iterations;
    double convergence_tolerance;
};

/* ── 工厂函数 ── */
Lv00Vector*        lv00_vector_create(Lv00BackendType backend, size_t n);
Lv00Matrix*        lv00_matrix_create(Lv00BackendType backend,
                                      size_t rows, size_t cols, bool sparse);
Lv00LinearSolver*  lv00_linsol_create(Lv00BackendType backend,
                                      const char *method /* "LU"|"GMRES" */);

/* ── 使用示例：求解 Ax = b ── */
/* 
 * Lv00Vector *x = lv00_vector_create(LV00_BACKEND_CUDA, n);
 * Lv00Vector *b = lv00_vector_create(LV00_BACKEND_CUDA, n);
 * Lv00Matrix *A = lv00_matrix_create(LV00_BACKEND_CUDA, n, n, true);
 * Lv00LinearSolver *S = lv00_linsol_create(LV00_BACKEND_CUDA, "GMRES");
 * 
 * // 填充 A 和 b...
 * S->ops->setup(S, A);
 * S->ops->solve(S, A, x, b, 1e-8);
 * // x 现在包含解
 */

#endif /* LV00_NUMERICAL_BACKEND_H */
```

### 3.2 几何演化：自适应步长与误差控制

将 CVODE 的自适应步长控制映射到 Lv-00 几何演化场景：

```c
/* === lv00_geom_evol.h — 几何演化自适应步长控制器 === */

#ifndef LV00_GEOM_EVOL_H
#define LV00_GEOM_EVOL_H

#include "lv00_numerical_backend.h"
#include "symbolic_coord.h"

/* ── 几何演化上下文 ── */
typedef struct Lv00GeomEvol {
    /* 演化状态 */
    double current_param;           /* 当前参数值（沿曲线的弧长参数） */
    Lv00Vector *state;              /* 当前几何状态向量（所有自由坐标） */
    Lv00Vector *state_low_order;    /* 低阶方法的状态（用于误差估计） */

    /* 自适应步长控制 */
    double step_size;               /* 当前步长 Δs */
    double step_size_min;           /* 最小允许步长 */
    double step_size_max;           /* 最大允许步长 */
    double rel_tol;                 /* 相对误差容差 */
    double abs_tol;                 /* 绝对误差容差 */

    /* PI 控制器参数 */
    double safety_factor;           /* 安全因子（典型值 0.9） */
    double growth_factor;           /* 步长增长上限（典型值 2.0） */
    double bias_factor;             /* 偏置因子（典型值 1.5） */

    /* 统计信息 */
    size_t num_steps;               /* 总步数 */
    size_t num_accepted;            /* 接受的步数 */
    size_t num_rejected;            /* 拒绝的步数 */
    double last_error;              /* 上一步的误差估计 */

    /* 约束流形信息 */
    ConstraintGraph *constraints;   /* 几何约束图——定义演化所在的流形 */

    /* 事件检测 */
    Lv00EventDetector *event_detector;
} Lv00GeomEvol;

/*
 * geoevol_create — 初始化几何演化引擎。
 *
 * 参数：
 *   initial_state - 初始几何配置
 *   constraints   - 定义演化流形的约束图
 *   rel_tol       - 相对误差容差（典型值 1e-6）
 *   abs_tol       - 绝对误差容差（典型值 1e-9）
 */
Lv00GeomEvol* geoevol_create(const Lv00Vector *initial_state,
                              ConstraintGraph *constraints,
                              double rel_tol, double abs_tol);

/*
 * geoevol_step — 执行一步几何演化。
 *
 * 借鉴 CVODE 的自适应步长控制回路：
 * 1. 以当前步长尝试进行一次几何推进
 * 2. 同时使用低阶方法计算参考解
 * 3. 比较两种方法的解以估计局部截断误差
 * 4. 若误差在容差内 → 接受步，调整步长（可能增大）
 *    若误差超出容差 → 拒绝步，减小步长，重新尝试
 *
 * 几何推进的具体算法（对应 CVODE 的积分器方法）：
 *   - 直线外推（1 阶）：沿切线方向移动 Δs
 *   - 弧长预测（2 阶）：利用曲率信息进行二次外推
 *   - 约束投影：推进后投影回约束流形（对应 CVODE 中的非线性校正）
 *
 * 返回：0 = 成功，-1 = 步长减小到最小值仍不收敛，-2 = 检测到事件
 */
int geoevol_step(Lv00GeomEvol *evol);

/*
 * geoevol_evolve_to — 演化到目标参数值。
 *
 * 内部循环调用 geoevol_step()，直到 current_param >= target_param，
 * 或在中间检测到事件。
 */
int geoevol_evolve_to(Lv00GeomEvol *evol, double target_param);

/* ── 自适应步长 PI 控制器 ── */

/*
 * geoevol_pi_controller — PI（比例-积分）步长控制器。
 *
 * 经典公式（来自 CVODE/IDA）：
 *   h_new = h * safety * e_n^(-alpha) * e_{n-1}^(beta)
 *
 * 其中：
 *   h       = 当前步长
 *   e_n     = 当前步的误差估计
 *   e_{n-1} = 上一步的误差估计
 *   alpha   = 1/(order+1)  （比例增益）
 *   beta    = bias/(order+1) （积分增益/偏置）
 */
static inline double
geoevol_pi_controller(double h, double e_n, double e_n_minus_1,
                      int order, double safety, double bias)
{
    double alpha = 1.0 / (order + 1.0);
    double beta  = bias / (order + 1.0);

    /* 避免 e_n 太接近零时的数值问题 */
    if (e_n < 1e-15) e_n = 1e-15;
    if (e_n_minus_1 < 1e-15) e_n_minus_1 = 1e-15;

    double factor = safety * pow(1.0 / e_n, alpha)
                          * pow(e_n_minus_1, beta);

    /* 限制步长变化幅度 */
    if (factor > 2.0) factor = 2.0;
    if (factor < 0.1) factor = 0.1;

    return h * factor;
}

/*
 * geoevol_error_norm — 加权均方根（WRMS）误差范数。
 *
 * 借鉴 SUNDIALS 的 WRMS 范数定义：
 *   ‖e‖_WRMS = sqrt( (1/N) * Σ_i (e_i / (atol + rtol*|y_i|))^2 )
 *
 * 对于几何坐标，y_i 是坐标分量值，e_i 是位置误差分量。
 */
double geoevol_error_norm(const Lv00Vector *error,
                          const Lv00Vector *state,
                          double rel_tol, double abs_tol);

#endif /* LV00_GEOM_EVOL_H */
```

### 3.3 事件检测（Rootfinding）

```c
/* === lv00_event_detect.h — 几何事件检测 === */

#ifndef LV00_EVENT_DETECT_H
#define LV00_EVENT_DETECT_H

#include <stdbool.h>

/* ── 事件类型 ── */
typedef enum {
    LV00_EVENT_CROSSING      = 0,  /* 零点穿越（最通用） */
    LV00_EVENT_RISING        = 1,  /* 仅上升穿越（+1） */
    LV00_EVENT_FALLING       = 2,  /* 仅下降穿越（-1） */
    LV00_EVENT_EITHER        = 3   /* 上升和下降都检测 */
} Lv00EventDirection;

/* ── 事件函数签名 ── */
typedef int (*Lv00EventFn)(double param, const Lv00Vector *state,
                           double *g_out, void *user_data);

/* ── 事件检测器 ── */
typedef struct Lv00EventDetector {
    size_t num_events;                  /* 注册的事件函数数量 */
    Lv00EventFn *event_functions;       /* 事件函数数组 g_i(param, state) */
    double *g_prev;                     /* 上一步的 g_i 值 */
    double *g_curr;                     /* 当前步的 g_i 值 */
    Lv00EventDirection *directions;     /* 每个事件的方向约束 */
    bool *triggered;                    /* 每个事件是否已触发 */
    double *event_params;               /* 每个事件的精确触发参数值（Brent 精确定位） */
    void **user_data;                   /* 每个事件的用户数据 */
    double rootfind_tolerance;          /* 根查找容差 */
} Lv00EventDetector;

/*
 * event_detect_create — 创建事件检测器。
 */
Lv00EventDetector* event_detect_create(size_t max_events,
                                        double rootfind_tol);

/*
 * event_detect_check — 检查事件是否在当前步区间内发生。
 *
 * 借鉴 SUNDIALS 的 rootfinding 流程：
 * 1. 检查 nrtfn 个事件函数的符号变化
 * 2. 若检测到零点穿越 → 调用 Brent 方法精确定位事件参数值
 * 3. 返回触发的事件列表
 *
 * 几何事件示例：
 *   - 两点距离为零（碰撞/接触）：
 *     g(s) = distance(P1(s), P2(s)) - epsilon
 *
 *   - 曲线与直线相交：
 *     g(s) = cross_product(curve(s) - line_A, line_B - line_A)
 *
 *   - 点在圆上：
 *     g(s) = distance(P(s), center) - radius
 *
 * 返回：触发的事件数量（0 表示无事件）
 */
int event_detect_check(Lv00EventDetector *detector,
                       double param_prev, double param_curr,
                       const Lv00Vector *state_prev,
                       const Lv00Vector *state_curr);

/*
 * event_detect_locate — 使用 Brent 方法精确定位事件参数。
 *
 * Brent 方法结合了二分法的可靠性、割线法的速度和逆二次插值的效率。
 * 是 SUNDIALS 中 rootfinding 的默认算法（原始使用 Illinois 变体）。
 */
int event_detect_locate(Lv00EventDetector *detector,
                        size_t event_index,
                        double a, double b,
                        const Lv00Vector *state_a,
                        const Lv00Vector *state_b,
                        double *root_out);

#endif /* LV00_EVENT_DETECT_H */
```

### 3.4 几何非线性方程组求解（KINSOL 映射）

```c
/* === lv00_kinsol.h — 几何非线性方程组求解（KINSOL 风格） === */

#ifndef LV00_KINSOL_H
#define LV00_KINSOL_H

#include "lv00_numerical_backend.h"
#include "constraint_graph.h"

/*
 * lv00_kinsol_solve — 使用 Krylov 迭代 Newton 法求解几何非线性系统。
 *
 * 问题形式：F(u) = 0，其中：
 *   - u 是所有自由几何变量的向量（坐标 + 长度 + 角度等）
 *   - F(u) 是约束残差向量（每个约束的违反程度）
 *
 * 算法（借鉴 KINSOL）：
 *   1. 初始猜测 u_0（当前几何配置）
 *   2. Newton 迭代循环：
 *      a. 计算残差 F(u_k)
 *      b. 若 ‖F(u_k)‖ < tol → 收敛，返回
 *      c. 使用 Krylov 子空间方法近似求解 J(u_k) · δu = -F(u_k)
 *         - 不需要显式构造 Jacobian J，只需 J 与向量的乘积
 *         - J(v) 通过有限差分或解析表达式计算
 *      d. 线搜索确定步长因子 λ ∈ (0, 1]
 *      e. 更新：u_{k+1} = u_k + λ · δu
 *   3. 若迭代次数超限 → 返回不收敛
 */
typedef struct Lv00KinsolStats {
    size_t newton_iterations;     /* Newton 迭代次数 */
    size_t krylov_iterations;     /* Krylov 子空间总迭代次数 */
    size_t func_evaluations;      /* 残差函数计算次数 */
    size_t jac_evaluations;       /* Jacobian-向量乘积次数 */
    double final_residual;        /* 最终残差范数 */
    bool converged;               /* 是否收敛 */
} Lv00KinsolStats;

/*
 * geometric_residual — 计算几何约束残差向量。
 *
 * 对每个约束，计算其"违反程度"作为残差分量：
 *   距离等式 "(distance A B) = d" → 残差 = |distance(A,B) - d|
 *   共线性 "(collinear A B C)" → 残差 = 点 C 到直线 AB 的有符号距离
 *   角度等式 "angle(A,B,C) = θ" → 残差 = |angle(A,B,C) - θ|
 */
void geometric_residual(const Lv00Vector *u,    /* 当前变量值 */
                        ConstraintGraph *graph,  /* 约束图 */
                        Lv00Vector *F_out);      /* 输出残差向量 */

/*
 * geometric_jacobian_times_vector — Jacobian-向量乘积 J(u) · v。
 *
 * 这是 Krylov 子空间方法的关键操作——无需显式构造 J。
 * 使用有限差分近似：
 *   J(u) · v ≈ (F(u + εv) - F(u)) / ε
 *
 * 或对可微约束使用解析表达式。
 */
void geometric_jacobian_times_vector(const Lv00Vector *u,
                                     const Lv00Vector *v,
                                     ConstraintGraph *graph,
                                     Lv00Vector *Jv_out);

/*
 * geometric_line_search — 全局化线搜索。
 *
 * 确保 Newton 步不会导致残差增大（保证全局收敛性）。
 * 从 λ=1 开始，若不满足 Wolfe 条件则逐步缩减。
 */
double geometric_line_search(const Lv00Vector *u,
                             const Lv00Vector *du,
                             ConstraintGraph *graph,
                             Lv00Vector *F_new,
                             Lv00Vector *u_new);

Lv00KinsolStats lv00_kinsol_solve(
    Lv00Vector *u,                    /* 输入：初始猜测 / 输出：解 */
    ConstraintGraph *graph,           /* 几何约束图 */
    const Lv00LinearSolver *linsol,   /* Krylov 子空间线性求解器 */
    double tol,                       /* 收敛容差 */
    size_t max_newton_iters);         /* 最大 Newton 迭代次数 */

#endif /* LV00_KINSOL_H */
```

### 3.5 灵敏度分析

```c
/* === lv00_geom_sensitivity.h — 几何约束参数灵敏度分析 === */

#ifndef LV00_GEOM_SENSITIVITY_H
#define LV00_GEOM_SENSITIVITY_H

#include "lv00_numerical_backend.h"

/*
 * Lv00Sensitivity — 几何系统参数灵敏度分析。
 *
 * 借鉴 CVODES 的前向灵敏度方法：
 * 给定几何系统 F(u, p) = 0，其中 p 是参数（线段长度、角度等），
 * 求解 du/dp —— 几何配置如何随参数变化。
 *
 * 通过求解增广线性系统获得灵敏度：
 *   J_u · (du/dp) = -J_p
 *
 * 其中 J_u = ∂F/∂u 和 J_p = ∂F/∂p 分别为 Jacobian 对变量和对参数。
 */
typedef struct Lv00Sensitivity {
    size_t num_variables;       /* 几何变量数 */
    size_t num_parameters;      /* 参数数 */

    /* 灵敏度矩阵 S[i][j] = du_i / dp_j */
    Lv00Matrix *sensitivity_matrix;

    /* 通过求解 J_u · S = -J_p 获得 */
    Lv00LinearSolver *linsol;
} Lv00Sensitivity;

/*
 * geom_sensitivity_forward — 前向灵敏度分析。
 *
 * 对每个参数 p_j，求解线性系统：
 *   J_u · (du/dp_j) = -∂F/∂p_j
 *
 * 这给出了在当前几何配置下，每个变量如何随参数变化。
 *
 * 应用场景：
 *   - 用户拖拽一条线段并改变其长度 p_j：
 *     灵敏度 du/dp_j 告诉求解器"哪些点的坐标需要如何调整"
 *   - 优化中的梯度下降：
 *     利用灵敏度信息计算目标函数对参数的全导数
 */
int geom_sensitivity_forward(const Lv00Vector *u,
                             const Lv00Vector *p,
                             ConstraintGraph *graph,
                             Lv00Sensitivity *sens);

/*
 * geom_sensitivity_adjoint — 伴随灵敏度分析。
 *
 * 当需要计算标量目标函数 g(u) 对所有参数 p 的梯度时使用。
 * 伴随方法比前向方法更高效：
 *   前向方法：求解 N_p 个线性系统（每个参数一个）
 *   伴随方法：求解 1 个线性系统，然后通过点积得到所有梯度
 *
 * 对于参数较多（N_p > 1）的场景，伴随方法显著优于前向方法。
 */
int geom_sensitivity_adjoint(const Lv00Vector *u,
                             const Lv00Vector *p,
                             /* 目标函数 g(u) 的梯度向量 ∂g/∂u */
                             const Lv00Vector *dg_du,
                             ConstraintGraph *graph,
                             Lv00Vector *dg_dp_out);  /* 输出：dg/dp */

#endif /* LV00_GEOM_SENSITIVITY_H */
```

---

## 4. 实现路线图

### 4.1 第一阶段：N_Vector / SUNMatrix / SUNLinearSolver 抽象层（P1 最高）

| 任务 | 文件 | 说明 |
|:---|:---|:---|
| 定义 `Lv00VectorOps` 虚函数表结构和 `Lv00Vector` 类型 | `lv00_numerical_backend.h` | 约 15 个核心操作：clone, copy, dot, norm, axpy, scale, wrms_norm 等 |
| 实现串行 CPU 后端 `LV00_BACKEND_SERIAL` | `lv00_vec_serial.c` | 基于 `double[]` 数组的朴素实现 |
| 定义 `Lv00MatrixOps` 和 `Lv00Matrix` | `lv00_numerical_backend.h` | dense 和 sparse 两种矩阵表示 |
| 实现串行 dense 矩阵 + LU 分解 | `lv00_mat_dense.c` | 基于 LAPACK 风格实现（或简化为 Gauss 消元） |
| 定义 `Lv00LinearSolverOps` 和工厂函数 | `lv00_linsol.c` | 支持 "LU" 和 "GMRES" 两种求解方法 |
| 单元测试：向量和矩阵操作的正确性 | `tests/test_numerical_backend.c` | 覆盖所有虚函数表操作的正确性和边界条件 |

**预估规模**：约 1000 行 C 代码

### 4.2 第二阶段：几何演化与事件检测（P1 最高）

| 任务 | 文件 | 说明 |
|:---|:---|:---|
| 实现 `Lv00GeomEvol` 几何演化引擎 | `lv00_geom_evol.c` | 自适应步长 PI 控制器 + 约束流形投影 |
| 实现几何推进方法（直线外推、弧长预测、约束投影） | `lv00_geom_evol.c` | 对应 CVODE 中的多种积分器 |
| 实现 WRMS 误差范数和步长接受/拒绝逻辑 | `lv00_geom_evol.c` | 借鉴 CVODE 的误差估计流程 |
| 实现 `Lv00EventDetector` 和 Illinois/Brent 根查找 | `lv00_event_detect.c` | 零点穿越检测 + 精确定位 |
| 实现常见几何事件函数（交点、碰撞、区域进入/离开） | `lv00_geom_events.c` | 预定义的常用几何事件函数库 |
| 集成测试：演化圆上动点的精度验证 | `tests/test_geom_evol.c` | 验证自适应步长在变曲率路径上的表现 |

**预估规模**：约 1200 行 C 代码

### 4.3 第三阶段：非线性求解与灵敏度分析（P2 高）

| 任务 | 文件 | 说明 |
|:---|:---|:---|
| 实现 `geometric_residual()` 几何约束残差函数 | `lv00_kinsol.c` | 每种约束类型的残差计算公式 |
| 实现 `geometric_jacobian_times_vector()` 有限差分 J(v) | `lv00_kinsol.c` | 无需显式 Jacobian 的矩阵-向量乘积 |
| 实现 KINSOL 风格的 Krylov-Newton 求解器 | `lv00_kinsol.c` | GMRES 迭代 + 线搜索全局化 |
| 实现 `geom_sensitivity_forward()` 前向灵敏度 | `lv00_geom_sensitivity.c` | 同时求解配置和灵敏度 |
| 实现线搜索和收敛检测 | `lv00_kinsol.c` | Wolfe 条件 + 步长回退 |
| 集成测试：20+ 约束的非线性几何系统求解 | `tests/test_kinsol.c` | 验证收敛性和精度 |

**预估规模**：约 800 行 C 代码

### 4.4 第四阶段：GPU 后端与性能优化（P3+）

| 任务 | 说明 |
|:---|:---|
| 实现 LV00_BACKEND_CUDA 的 N_Vector 虚函数表 | 利用 cuBLAS 实现向量运算 |
| 实现 CUDA sparse 矩阵和 cuSOLVER 线性求解器后端 | 大规模稀疏系统的 GPU 加速 |
| 实现 LV00_BACKEND_HIP 的 AMD GPU 支持 | 适配 HIP 运行时和 rocBLAS |
| 混合精度灵敏度分析 | 灵敏度计算用 float/double，几何状态保持 GMP 精确有理数 |
| Benchmark：不同后端在大规模 Groebner 基计算中的性能对比 | CPU 串行 vs OpenMP vs CUDA vs HIP |

---

## 5. 附录

### 附录 A：SUNDIALS 与 Lv-00 核心概念对照

| SUNDIALS 概念 | Lv-00 对应概念 | 关键差异 |
|:---|:---|:---|
| 时间 t | 几何参数 s（弧长/演化参数） | Lv-00 的"时间"是几何量 |
| 状态向量 y(t) | 几何配置向量 `GeometricState` | Lv-00 的变量包含有理数和符号 |
| ODE 右手边 f(t,y) | 几何推进场 `geometric_flow(s, state)` | Lv-00 的流场由约束流形的切空间定义 |
| DAE F(t,y,y')=0 | 约束流形 `F(state) = 0` | Lv-00 的约束不含显式时间导数 |
| Jacobian J = ∂f/∂y | `geometric_jacobian` — 约束的 Jacobian | Lv-00 使用有限差分而非解析 |
| 事件函数 g(t,y) | 几何事件函数 `g(s, state)` | Lv-00 的事件具有几何语义（交点、碰撞） |
| 稀疏线性求解器 | `Lv00LinearSolver` 的 sparse 后端 | 相同抽象，不同数据精度 |
| 并行向量操作 | `Lv00Vector` 的 CUDA/HIP 后端 | SUNDIALS 更成熟（更多后端和优化） |
| 灵敏度 dy/dp | 几何灵敏度 `d(state)/d(parameter)` | Lv-00 的参数可以是约束中的任何标量 |

### 附录 B：SUNDIALS 各求解器的典型应用场景与 Lv-00 对应

| SUNDIALS 模块 | 典型应用 | Lv-00 对应场景 |
|:---|:---|:---|
| CVODE (ODE) | 化学反应动力学、天体力学 N 体问题 | 几何点沿约束流形的平滑运动、路径追踪 |
| CVODES (ODE+灵敏度) | 参数估计、模型校准 | 交互式约束参数调节时的实时反馈 |
| IDA (DAE) | 带约束的力学系统、电路网络 | 几何约束系统的直接代数求解（当前 Lv-00 模式） |
| IDAS (DAE+灵敏度) | 受约束系统的参数灵敏度 | 带等式约束的几何参数的灵敏度研究 |
| ARKODE (加性 RK) | IMEX 方法处理刚性/非刚性分离 | 几何系统中快变量（高频振荡）和慢变量（整体变形）的分离处理 |
| KINSOL (非线性代数) | 大规模稀疏非线性系统的稳态解 | 几何约束系统的不动点求解、最优化配置搜索 |

### 附录 C：SUNDIALS 全局求解器参数速查

| 参数 | 含义 | Lv-00 默认值 | 说明 |
|:---|:---|:---|:---|
| `reltol` | 相对误差容差 | 1e-6 | 控制几何位置的相对精度 |
| `abstol` | 绝对误差容差 | 1e-9 | 控制几何位置的绝对精度 |
| `max_step` | 最大步长 | `param_range / 100` | 参数范围的 1/100，防止一步跨越整个定义域 |
| `first_step` | 第一步步长 | `param_range / 1000` | 从保守的步长开始 |
| `max_num_steps` | 最大总步数 | 10000 | 防止无限制循环 |
| `max_ord` | 最大积分阶数 | 4 | 几何推进通常 2-3 阶就足够 |
| `max_krylov_dim` | Krylov 子空间最大维度 | 30 | GMRES 重启维度 |
| `eps_lin` | 线性求解器容差因子 | 0.05 | Newton 迭代的线性容差 |
| `max_nliters` | 最大 Newton 迭代次数 | 3 | 通常 1-3 次就收敛 |

---

> **文档结束**
> 本文档详述了 SUNDIALS 高性能微分方程求解器套件在八个核心维度上对 Lv-00 数值计算基础设施的借鉴方案。核心结论：通过参考 SUNDIALS 的三层抽象架构（N_Vector/SUNMatrix/SUNLinearSolver），在 Lv-00 中建立统一的多后端数值计算接口；通过借鉴 CVODE 的自适应步长 PI 控制器和事件检测机制，为 Lv-00 的几何演化和事件处理提供精确可控的时间推进；通过参考 KINSOL 的 Krylov-Newton 方法和 CVODES 的灵敏度分析，为 Lv-00 的几何非线性系统和参数灵敏度研究提供高效的求解路径。

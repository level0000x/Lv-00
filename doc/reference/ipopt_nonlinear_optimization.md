# IPOPT 大规模非线性优化求解器参考文档

> 面向 Lv-00 求解器开发团队的 IPOPT 技术调研与借鉴方案

---

## 1. 项目概述

### 1.1 简介

[IPOPT](https://github.com/coin-or/Ipopt)（Interior Point OPTimizer）是卡内基梅隆大学开发的大规模非线性优化求解器，属于 COIN-OR（Computational Infrastructure for Operations Research）开源项目家族的核心成员。自 2003 年首次发布以来，IPOPT 已持续发展超过 20 年，成为学术界和工业界最广泛使用的开源 NLP（Nonlinear Programming，非线性规划）求解器之一。

IPOPT 求解的是一般形式的非线性规划问题：

```
min  f(x)                           （目标函数）
s.t. g_L ≤ g(x) ≤ g_U              （一般约束）
     x_L ≤ x ≤ x_U                  （变量边界）
```

其中 f: R^n → R 为目标函数，g: R^n → R^m 为约束函数，两者均可为非线性。f 和 g 至少需要是两次连续可微的（C^2）。

### 1.2 核心算法

IPOPT 使用**原对偶内点法**（Primal-Dual Interior Point Method）结合**过滤线搜索**（Filter Line Search）作为核心求解算法。

原对偶内点法的基本思想是将带约束的 NLP 问题转化为一系列无约束（或简单约束）的障碍子问题。对于形如 g(x) = 0 和 x >= 0 的约束，引入障碍参数 mu > 0，构造障碍问题：

```
min  f(x) - mu * Σ ln(x_i)          （障碍目标）
s.t. g(x) = 0
```

原始变量 x（称为原变量）和拉格朗日乘子 lambda（称为对偶变量）同时更新。当 mu → 0 时，障碍问题的解趋于原问题的解。

过滤线搜索是 IPOPT 的特色技术之一。传统的线搜索只关注目标函数值的下降，而过滤线搜索同时关注目标函数值和约束违反度两个指标，允许搜索步在改善约束违反度的同时暂时接受目标函数值的增加，从而增强全局收敛性。

### 1.3 技术栈

| 层次 | 语言 | 说明 |
|------|------|------|
| 核心求解器 | C++ | 约 150,000+ 行，实现了内点法主循环、过滤线搜索、稀疏线性代数 |
| 自动微分 | CppAD | 基于运算符重载的自动微分库，用于生成目标函数和约束的梯度及 Hessian |
| 线性求解器 | C/Fortran | MA27、MA57（Harwell 子程序库）、MUMPS、Pardiso 等多后端支持 |
| C 接口 | C 包装层 | 提供纯 C API，便于嵌入式调用 |
| Fortran 接口 | Fortran 模块 | 用于传统 Fortran 应用程序 |
| Java 接口 | JNI 绑定 | 通过 Java Native Interface 调用 IPOPT |
| R 接口 | ipoptr 包 | R 语言的 IPOPT 接口 |

### 1.4 许可证与生态

IPOPT 采用 **Eclipse Public License 2.0（EPL 2.0）**，这是一种弱 Copyleft 许可证：对 IPOPT 本身的修改需开源，但使用 IPOPT 库链接的应用程序无需开源。这使得 Lv-00 可以将 IPOPT 编译链接到其 C 内核中。

IPOPT 被广泛集成到各类建模平台：
- **CASADI**：非线性优化与最优控制的符号框架，IPOPT 是其默认 NLP 求解器
- **Pyomo**：Python 优化建模语言
- **GAMS / AMPL**：商业建模系统的开源求解器后端
- **JuliaOpt / JuMP**：Julia 优化生态系统的核心求解器
- **GEKKO**：Python 的动态优化与仿真平台

### 1.5 关键统计

| 指标 | 数值 |
|------|------|
| C++ 源码行数 | 150,000+ |
| 支持的线性求解器后端 | 10+（MA27、MA57、MA77、MA86、MA97、MUMPS、Pardiso、WSMP、SPRAL、自定义） |
| 自动微分模式 | 2 种（CppAD 运算符重载 / 用户手动提供导数） |
| 问题规模上限 | 数百万变量 + 数百万约束（取决于内存和线性求解器） |
| 学术引用数 | 10,000+（基于 Google Scholar） |

---

## 2. 核心借鉴点

### 2.1 原对偶内点法 + 过滤线搜索

**IPOPT 做法**

IPOPT 在每次迭代中求解一个大型的稀疏线性系统（KKT 系统）：

```
[ W_k + Σ_k + δ_w*I    A_k           ] [ Δx ]   [ -∇f(x_k) + A_k^T λ_k + z_k - mu*X^{-1}*e ]
[ A_k^T                 -δ_c*I       ] [ Δλ ] = [ -g(x_k)                                ]
```

其中：
- W_k 为目标函数和约束的 Hessian 矩阵线性组合
- A_k 为约束的雅可比矩阵
- Σ_k 为障碍项的对角矩阵
- δ_w 和 δ_c 为保证正定性的正则化参数

过滤线搜索的"过滤"概念：一个试探点被接受，当且仅当它不被当前过滤集合中的任何点"支配"。过滤集合存储在目标函数值 f 和约束违反度 θ 两个坐标轴上的帕累托前沿：

```
f(x_new) < f(x_j)  或  θ(x_new) < θ(x_j)   对所有 (f(x_j), θ(x_j)) ∈ Filter
```

**Lv-00 对应关系**

Lv-00 的几何优化问题本质上是最小化构造代价的函数（如最小化辅助线数量），同时满足所有几何约束。这正是一个 NLP 问题。借鉴 IPOPT 的内点法架构，可以将 Lv-00 的几何构型优化建模为：

| IPOPT 概念 | Lv-00 对应 |
|-----------|-------------|
| f(x)：目标函数 | 构造代价：辅助线数量、构造步骤数、语义简洁度 |
| g(x)：约束函数 | 几何约束：共线、角度、距离比例等 |
| x_L ≤ x ≤ x_U | 点坐标的可行范围（由图形边界决定） |
| 障碍项 mu * Σ ln(x_i) | 控制点靠近边界时的惩罚——避免坐标退化 |
| 过滤线搜索 | 在"目标值改善"和"约束违反度减少"之间权衡——对应于 Lv-00 中的"证明质量"和"构造简洁度"双目标 |
| KKT 系统求解 | 符号化后转化为 Gröbner 基计算，或数值化后转化为稀疏线性系统 |

### 2.2 自动微分（CppAD）集成

**IPOPT 做法**

IPOPT 要求用户提供约束的梯度（雅可比矩阵）和 Hessian 矩阵。为降低使用门槛，IPOPT 集成了 CppAD（基于运算符重载的自动微分库）。用户在定义约束函数时使用 CppAD 的 AD 类型，编译器自动跟踪计算图并生成精确的导数值。

CppAD 的工作流程：

```
用户定义:  f(AD<double> x)  —— 使用运算符重载
    │
    ▼
CppAD 录制:  将计算图保存为操作序列（tape）
    │
    ▼
前向模式:   计算函数值和任意方向的方向导数
    │         dy = f'(x) · dx
    ▼
反向模式:   计算梯度 ∇f(x)
    │
    ▼
稀疏雅可比: 对稀疏模式感知，只为非零元素分配存储
```

**Lv-00 对应关系**

Lv-00 的约束图（`constraint_graph`）中的每个几何约束都可以表达为一个多项式方程。在符号层面，需要计算多项式的（形式）导数和 Hessian 以为 Gröbner 基构造 S-多项式；在数值层面，需要计算约束的数值梯度以进行局部优化。

| CppAD 概念 | Lv-00 对应 | 实现机制 |
|-----------|-------------|---------|
| AD 类型 `CppAD::AD<double>` | `SymbolicCoord` + `FRepNode` 梯度标记 | GMP 精确算术 + 符号微分 |
| 计算图录制（tape） | 约束图遍历 → 生成计算序列 | `ConstraintGraph` 的拓扑排序 |
| 前向模式求方向导数 | `frep_eval_directional(expr, direction)` | 链式法则逐节点求值 |
| 反向模式求梯度 | `frep_gradient(expr, variables[])` | 反向传播：从根节点到叶节点 |
| 稀疏雅可比 | 约束-变量二分图的最小匹配 | 只对存在依赖关系的 (约束, 变量) 对求偏导 |
| 颜色压缩 | 约束共线分组 → 同色约束可同时求值 | 独立约束并行传播 |

### 2.3 线性求解器抽象层

**IPOPT 做法**

IPOPT 最显著的设计特征之一是其**线性求解器抽象层**。内点法的每个迭代都需要求解一个大规模的稀疏对称线性系统，不同的线性求解器（直接法/迭代法）在不同问题规模和结构上性能差异巨大。IPOPT 将线性求解器抽象为统一接口，用户可以通过配置参数在以下后端中切换：

| 求解器 | 类型 | 来源 | 特点 |
|--------|------|------|------|
| MA27 | 直接法 | HSL (Harwell) | 经典稀疏对称不定矩阵求解器，适合中小规模 |
| MA57 | 直接法 | HSL (Harwell) | MA27 的现代替代，改进的内存管理 |
| MA77 | 直接法 | HSL (Harwell) | 适合超大规模、多右端项场景 |
| MA86 | 直接法 | HSL (Harwell) | 使用 OpenMP 并行，适合对称不定矩阵 |
| MA97 | 直接法 | HSL (Harwell) | 多线程 HSL 的直接法求解器 |
| MUMPS | 直接法 | 开源社区 | 分布式并行稀疏求解器，支持 MPI |
| Pardiso | 直接法 | Intel / 巴塞尔大学 | 商业级高性能稀疏求解器，支持多线程 |
| WSMP | 迭代法 | IBM Watson | 适合有限元应用的迭代稀疏求解器 |
| SPRAL | 任意 | STFC Rutherford | 包含直接法和迭代法的混合框架 |

抽象接口的设计模式：

```cpp
class Ipopt::AlgorithmBuilder {
    // 核心类对外只看到 IpoptSolverInterface 抽象基类
    virtual ESymSolverStatus InitializeStructure(...) = 0;
    virtual ESymSolverStatus MultiSolve(...) = 0;
    virtual int GetNSolves() = 0;
    // MA27/MA57/MUMPS 等具体实现继承此接口
};
```

**Lv-00 对应关系**

Lv-00 求解器的核心计算——Gröbner 基的多项式系数的精确运算——依赖高效的线性系统求解。借鉴 IPOPT 的线性求解器抽象层，Lv-00 可以设计一个多后端的符号/数值混合求解接口：

| IPOPT 线性求解抽象 | Lv-00 求解后端抽象 |
|-------------------|-------------------|
| `AlgorithmBuilder::LinearSolver` 接口 | `Lv00LinearSolverVTable` 虚函数表 |
| 直接法求解器（MA27/MA57） | 精确有理解法器（GMP `mpq_t` 矩阵高斯消元） |
| MUMPS 分布式 | 分布式 Gröbner 基计算（多节点多项式约化） |
| Pardiso 多线程 | 多线程 Buchberger 算法（约束图的分块约化） |
| 迭代法（WSMP） | 近似代数求解器（双精度浮点 + 误差带控制） |
| 运行时切换 | 根据约束图规模（节点数、边密度）动态选择后端 |

```c
// Lv-00 多后端线性求解器抽象层设计
typedef enum {
    LV00_LSOLV_EXACT_GAUSS,    // GMP 精确高斯消元（默认，适合 < 1000 变量）
    LV00_LSOLV_EXACT_GROEBNER, // Gröbner 基消元（适合代数结构清晰的问题）
    LV00_LSOLV_APPROX_DENSE,   // LAPACK 稠密求解（快速，损失精度）
    LV00_LSOLV_APPROX_SPARSE,  // SuiteSparse KLU (稀疏，适合大规模)
    LV00_LSOLV_HYBRID,         // 混合模式：先用近似法求候选，再用精确法验证
} Lv00LinearSolverType;

typedef struct Lv00LinearSolver {
    Lv00LinearSolverType type;
    const char* name;
    Lv00RetCode (*init)(struct Lv00LinearSolver* s,
                        const Lv00SparseMatrix* A);
    Lv00RetCode (*solve)(struct Lv00LinearSolver* s,
                          const Lv00Vector* b, Lv00Vector* x);
    void        (*destroy)(struct Lv00LinearSolver* s);
    int64_t     (*memory_estimate)(const Lv00SparseMatrix* A);
} Lv00LinearSolver;
```

### 2.4 问题可扩展性设计（稀疏雅可比矩阵）

**IPOPT 做法**

IPOPT 的设计假设是问题规模可能非常大（百万级变量和约束），因此整个内点法循环中的所有数据结构和运算都必须利用稀疏性。关键设计包括：

1. **稀疏雅可比矩阵存储**：约束雅可比矩阵 A_k 以压缩稀疏行（CSR）/ 压缩稀疏列（CSC）格式存储，只保留非零元素
2. **增量结构重分析**：当约束的非零模式不变时，线性求解器重用上一次的符号分解，只做数值分解
3. **惰性求值**：Hessian 的某些项只在需要时才计算（如正则化项 δ_w*I）
4. **结构探测**：根据已知约束结构自动推断雅可比的稀疏模式，避免运行时发现

**Lv-00 对应关系**

Lv-00 处理几何问题时，约束的数量可能随着几何图形的复杂度快速增长。每个点 (x_i, y_i) 只参与有限的几个约束，因此约束-变量关联矩阵天然是稀疏的。

| IPOPT 稀疏技术 | Lv-00 对应 |
|---------------|-------------|
| CSR/CSC 稀疏矩阵存储 | `SymbolicCoord*` 邻接表：每个变量只关联相关的约束 |
| 符号分解重用 | 几何图形的拓扑不变时，约束图结构不变 → 重用拓扑排序 |
| 惰性求值 Hessian | Gröbner 基计算中，S-多项式的系数矩阵只在需要时展开 |
| 结构探测 | `ConstraintGraph` 遍历 → 自动找出约束-变量二分图结构 |
| 增量非零模式更新 | 添加新辅助线时，只更新受影响约束的雅可比块 |

### 2.5 多语言接口

**IPOPT 做法**

IPOPT 核心以 C++ 编写，但对外提供多语言 API：

```
┌────────────────────────────────────────────────────┐
│                 IPOPT C++ 核心                       │
│        (ipopt_core, ipopt_algorithm, ...)           │
├────────────────────────────────────────────────────┤
│     C 接口层 (IpStdCInterface.h)                     │
│     将 C++ 类包装为 C 函数指针回调                     │
├───────────┬──────────┬──────────┬───────────────────┤
│  Fortran  │  Java    │   R      │  Python           │
│  MODULE   │  JNI     │  ipoptr  │  (via CASADI/     │
│  IPOPT    │  binding │  package │   Pyomo)          │
└───────────┴──────────┴──────────┴───────────────────┘
```

C 接口的核心模式是基于回调函数的结构体：

```c
typedef struct {
    /* 提供目标函数值和梯度 */
    Bool (*eval_f)(int n, Number* x, Bool new_x,
                   Number* obj_value, UserDataPtr user_data);
    /* 提供约束函数值和雅可比稀疏结构 */
    Bool (*eval_g)(int n, Number* x, Bool new_x,
                   int m, Number* g, UserDataPtr user_data);
    /* 提供目标函数和约束的梯度/Hessian */
    Bool (*eval_grad_f)(int n, Number* x, Bool new_x,
                        Number* grad_f, UserDataPtr user_data);
    Bool (*eval_jac_g)(int n, Number* x, Bool new_x,
                       int m, int nele_jac,
                       int* iRow, int* jCol, Number* values,
                       UserDataPtr user_data);
    /* ... 更多回调 ... */
} IpoptProblem;
```

**Lv-00 对应关系**

Lv-00 的求解器同样是一个复杂系统，需要为不同的前端（TypeScript 桌面应用、Python 脚本、WebAssembly 网页）提供访问接口。

| IPOPT 多语言接口 | Lv-00 对应 |
|-----------------|-------------|
| C++ 核心 | C 核心（`symbolic_coord.h`、`solver.h`、`constraint_graph.h`） |
| C 包装层 + 回调结构体 | `Lv00SolverInterface` 回调函数表 |
| Fortran 接口 | 不需要（无 Fortran 用户群） |
| Java JNI 接口 | WebAssembly 导出层（用 `wasm-bindgen` 替代 JNI） |
| R 接口 | Python `ctypes` 绑定层 |
| Python 接口（via CASADI） | TypeScript 前端 JavaScript FFI（通过 Tauri `invoke` 命令） |

```c
// Lv-00 求解器 C 接口设计（借鉴 IPOPT 回调模式）
// 文件: include/lv00/solver_interface.h
typedef void* Lv00SolverHandle;
typedef struct Lv00ProblemDefinition {
    int n_vars, n_constr, n_constr_jac, n_hessian;
    int (*eval_f)(int n, const double* x, double* obj, void* ud);
    int (*eval_grad_f)(int n, const double* x, double* grad_f, void* ud);
    int (*eval_g)(int n, const double* x, int m, double* g, void* ud);
    int (*eval_jac_g)(int n, const double* x, int m, int nele_jac,
                      double* values, void* ud);
    int (*eval_h)(int n, const double* x, double of, int m,
                  const double* lambda, int nh, double* values, void* ud);
    void* user_data;
} Lv00ProblemDefinition;
Lv00RetCode lv00_solver_create(Lv00SolverHandle* h, const Lv00ProblemDefinition* p);
Lv00RetCode lv00_solver_solve(Lv00SolverHandle h, double* x, double* obj);
void        lv00_solver_free(Lv00SolverHandle h);
```

### 2.6 AMPL 模型格式支持

**IPOPT 做法**

IPOPT 能直接读取 **AMPL**（A Mathematical Programming Language）格式的 .nl 文件。AMPL 是一种声明式的优化建模语言，用户只需声明变量（`var x >= 0`）、约束（`subject to`）和目标（`minimize/maximize`），IPOPT 的 AMPL 接口（`Ipopt::AmplTNLP`）负责解析 .nl 文件并自动生成梯度信息。

**Lv-00 对应关系**

Lv-00 同样需要声明式几何问题描述。借鉴 AMPL 模型定义方式：`var x` 对应 `Point P(?, ?)`，`param` 对应已知线段，`minimize` 对应构造步数最小化，`subject to` 对应几何约束声明。解析器将几何描述文本映射为 `ConstraintGraph` 节点和边。

### 2.7 核心借鉴点对照总表

| 序号 | IPOPT 概念 | IPOPT 实现位置 | Lv-00 对应模块 | Lv-00 借鉴方式 |
|------|-----------|---------------|---------------|---------------|
| 1 | 原对偶内点法 | `IpoptAlgorithm.hpp`、`IpIpoptAlg.cpp` | 求解器核心（`solver.h`） | KKT 系统 → Gröbner 基。障碍项 → 坐标退化惩罚 |
| 2 | 过滤线搜索 | `IpFilterLSAcceptor.hpp` | 多目标证明质量评估 | 双目标过滤：约束违反度 + 构造代价 |
| 3 | CppAD 自动微分 | `IpCGPenalty.cpp`、CppAD 依赖 | 约束图梯度/雅可比计算 | 符号微分 + 反向传播梯度 |
| 4 | 线性求解器抽象层 | `IpAlgBuilder.hpp`、`IpSymLinearSolver.hpp` | 多后端线性求解接口 | 精确 Gauss → Gröbner → 近似法的多后端 |
| 5 | 稀疏雅可比矩阵 | `IpGenTMatrix.hpp`、CSR 存储 | 约束-变量二分图 | 约束图的稀疏拓扑 → 稀疏矩阵格式 |
| 6 | 多语言接口 | C 包装层（`IpStdCInterface.h`） | WASM/Tauri/Python 绑定 | C 回调 + 函数指针的接口抽象 |
| 7 | AMPL .nl 格式 | `IpAmplTNLP.cpp` | 几何问题描述 DSL | 声明式语法 → 解析为 `ConstraintGraph` |

---

## 3. Lv-00 映射方案

### 3.1 架构概览

Lv-00 求解器通过借鉴 IPOPT 的内点法架构和多后端设计，建立以求解器抽象层为中心的模块化架构：

```
┌──────────────────────────────────────────────────────────────┐
│                   Lv-00 几何优化求解器                          │
├──────────────────────────────────────────────────────────────┤
│  几何描述文本  ──→  DSL 解析器  ──→  ConstraintGraph          │
│                                           │                  │
│        ┌──────────────────────────────────┼───────────────   │
│        │                                  │                  │
│        ▼                                  ▼                  │
│  ┌──────────┐                    ┌──────────────┐           │
│  │ 自动微分  │ ←── 借鉴 CppAD     │  预求解器     │           │
│  │ 梯度/     │                    │  冗余检测     │           │
│  │ 雅可比    │                    │  变量固定     │           │
│  └────┬─────┘                    └──────┬───────┘           │
│       │                                │                    │
│       ▼                                ▼                    │
│  ┌──────────────────────────────────────────────┐          │
│  │         求解器主循环（借鉴内点法循环结构）       │          │
│  │                                               │          │
│  │  1. 评估约束违反度 θ(x)                        │          │
│  │  2. 构造 KKT 系统（→ Gröbner 基 / 稀疏线性系统）│          │
│  │  3. 求解搜索方向 Δx                            │          │
│  │  4. 过滤线搜索确定步长 α                        │          │
│  │  5. 更新 x ← x + α·Δx                         │          │
│  │  6. 检查收敛条件                               │          │
│  └────────────────┬─────────────────────────────┘          │
│                   │                                         │
│                   ▼                                         │
│  ┌──────────────────────────────────────────────┐          │
│  │         线性求解器抽象层（借鉴多后端设计）        │          │
│  │  ┌──────────┬─────────┬──────────┬────────┐   │          │
│  │  │ GMP Gauss│ Gröbner │ SuiteSp. │LAPACK  │   │          │
│  │  │（精确）   │ 基消元   │ KLU     │（近似）│   │          │
│  │  └──────────┴─────────┴──────────┴────────┘   │          │
│  └──────────────────────────────────────────────┘          │
│                                                             │
│  ┌──────────────────────────────────────────────┐          │
│  │       求解器多语言接口层（借鉴 C 回调模式）      │          │
│  │  C API ──→ WASM ──→ Tauri ──→ Python ──→ CLI │          │
│  └──────────────────────────────────────────────┘          │
└──────────────────────────────────────────────────────────────┘
```

### 3.2 内点法结构到 Lv-00 几何优化映射

将 IPOPT 内点法的核心循环映射到 Lv-00 几何求解的具体语义：

```c
/**
 * @brief Lv-00 求解器内点法主循环
 *
 * 借鉴 IPOPT 的算法结构，将 KKT 系统的求解映射到 Lv-00 的
 * 符号/数值混合求解流程。
 *
 * 文件: src/solver_loop.c
 */

typedef struct Lv00InteriorPointState {
    Lv00SymbolicCoord** x;     // 原始变量（点坐标、长度等）
    Lv00SymbolicCoord** lambda; // 对偶变量（拉格朗日乘子）
    Lv00SymbolicCoord** z_L;   // 下界障碍项乘子
    Lv00SymbolicCoord** z_U;   // 上界障碍项乘子
    Lv00SymbolicCoord   mu;    // 障碍参数
    double              tol;   // 收敛容忍度
    int                 max_iter;
    int                 iter;
} Lv00InteriorPointState;

/**
 * 初始化内点法状态
 *
 * 对应 IPOPT 的 IpoptAlgorithm::Initialize —— 将所有变量初始化为
 * 几何上的自然初值（如三角形顶点取给定坐标，辅助点取均值等）。
 */
Lv00RetCode lv00_ipm_init(Lv00InteriorPointState* state,
                           const Lv00ConstraintGraph* graph);

/**
 * 单步迭代：求解 KKT 系统 → 线搜索 → 更新变量
 *
 * 对应 IPOPT 的 IpoptAlgorithm::Optimize 中的单次迭代体。
 * KKT 求解映射：
 *   - 如果约束是线性的 → 稀疏线性系统 → SuiteSparse 快速求解
 *   - 如果约束是多项式的 → Gröbner 基 → 符号消元求解
 *   - 如果约束包含超越数 → 数值近似的 KKT → 回到 B 计划
 */
Lv00RetCode lv00_ipm_step(Lv00InteriorPointState* state,
                           const Lv00ConstraintGraph* graph);

/**
 * 过滤线搜索
 *
 * 对应 IPOPT 的 FilterLSAcceptor。在 Lv-00 中将"目标"扩展为双目标：
 *   - 目标 1（目标函数）：最小化构造步数 / 辅助线数量
 *   - 目标 2（约束违反度）：所有几何约束的最大残差
 *
 * 过滤规则：
 *   候选解被接受 iff
 *     (f_new < f_j || θ_new < θ_j) 对所有已接受点 (f_j, θ_j)
 */
Lv00RetCode lv00_filter_line_search(Lv00InteriorPointState* state,
                                     Lv00SymbolicCoord   alpha,
                                     double              f_new,
                                     double              theta_new);

/**
 * 收敛检查
 *
 * 对应 IPOPT 的标准收敛条件：
 *   - θ(x) ≤ tol（约束违反度在容忍度内）
 *   - |f(x) - f(prev)| ≤ tol * |f(x)|（目标函数值变化足够小）
 *   - 最大迭代次数达到 → 输出当前最好结果
 */
bool lv00_ipm_check_convergence(const Lv00InteriorPointState* state,
                                 double f_current,
                                 double f_previous);
```

### 3.3 约束图梯度计算的自动微分实现

借鉴 CppAD 的计算图自动微分思想，为 Lv-00 的约束图实现梯度计算：

```c
/**
 * @brief 约束图的自动微分——计算所有约束对所有变量的梯度
 *
 * 借鉴 CppAD 的反向模式（reverse mode）自动微分：
 *   1. 前向传播：沿约束图求值所有约束的当前残差
 *   2. 反向传播：从每个残差节点反向传播梯度到叶变量
 *
 * 文件: src/autodiff.c
 */

typedef struct Lv00ADTapeEntry {
    int     op_code;        // 操作类型（ADD, MUL, POW, SIN, ...）
    int     lhs;            // 左操作数地址（-1 表示无）
    int     rhs;            // 右操作数地址（-1 表示无）
    double  value;          // 前向求值的缓存值
    double  adjoint;        // 反向传播的伴随值
} Lv00ADTapeEntry;

typedef struct Lv00AutoDiff {
    Lv00ADTapeEntry* tape;
    int     tape_size;
    int     tape_cap;
    int     n_independent;  // 独立变量数（变量坐标数）
    int     n_dependent;    // 非独立变量数（约束残差数）
} Lv00AutoDiff;

/**
 * 录制约束图的计算序列（tape）
 *
 * 借鉴 CppAD 的录音（taping）机制：
 * 遍历 ConstraintGraph，将每个约束的多项式表达式
 * 展开为操作序列并记录到 tape。
 */
Lv00RetCode lv00_autodiff_record(Lv00AutoDiff* ad,
                                  const Lv00ConstraintGraph* graph);

/**
 * 前向模式：计算所有约束在当前点处的函数值
 *
 * 沿 tape 顺序求值，将每个中间变量的值缓存。
 */
Lv00RetCode lv00_autodiff_forward(Lv00AutoDiff* ad,
                                   const double* x);

/**
 * 反向模式：计算所有约束对所有变量的雅可比矩阵
 *
 * 从约束残差节点出发，沿 tape 反向传播伴随值到独立变量。
 * 结果写入 J（雅可比矩阵，稠密格式）。
 */
Lv00RetCode lv00_autodiff_reverse(Lv00AutoDiff* ad,
                                   double* J);  // J[n_dependent * n_independent]
```

### 3.4 多后端线性求解器的运行时选择

借鉴 IPOPT 的多后端线性求解器设计，Lv-00 实现求解器等级的运行时后端选择：

```c
/**
 * @brief 求解器后端自动选择
 *
 * 借鉴 IPOPT 的 AlgorithmBuilder 模式：根据问题特征
 * （变量数、约束密度、非线性程度）自动选择最合适的求解后端。
 *
 * 文件: src/solver_backend.c
 */

typedef struct Lv00BackendSelector {
    int    n_variables;       // 变量数
    int    n_constraints;     // 约束数
    double sparsity;          // 雅可比稀疏度（0~1，越小越稀疏）
    int    max_degree;        // 最高多项式次数
    bool   has_transcendental;// 是否包含超越数
} Lv00BackendSelector;

/**
 * 根据问题特征推荐求解后端
 *
 * 启发式规则：
 *   - n < 100 且多项式次数 <= 2：GMP 精确高斯消元
 *   - n < 1000 且 sparsity < 0.1：SuiteSparse KLU 稀疏求解
 *   - n >= 1000 且多项式：分布式 Gröbner 基
 *   - 包含超越数：回退到 LAPACK 稠密近似求解
 */
Lv00LinearSolverType lv00_select_backend(const Lv00BackendSelector* sel) {
    if (sel->has_transcendental) {
        return LV00_LSOLV_APPROX_DENSE;  // 超越数 → 数值近似
    }
    if (sel->n_variables < 100 && sel->max_degree <= 2) {
        return LV00_LSOLV_EXACT_GAUSS;   // 小规模 → 精确
    }
    if (sel->sparsity < 0.1) {
        return LV00_LSOLV_APPROX_SPARSE; // 大规模稀疏 → SuiteSparse
    }
    if (sel->n_variables >= 1000) {
        return LV00_LSOLV_EXACT_GROEBNER;// 大规模多项式 → Gröbner
    }
    return LV00_LSOLV_HYBRID;            // 默认：先近似再精确验证
}
```

### 3.5 几何问题描述 DSL 的设计

借鉴 IPOPT 对 AMPL 的支持，为 Lv-00 设计声明式的几何问题描述语法：

```
/* ---------------------------------------------------------------
 * Lv-00 几何问题描述 DSL（设计草案）
 * 借鉴 AMPL 的声明式建模语法，适配几何领域的特定语义
 * --------------------------------------------------------------- */

# 1. 声明给定元素（已知坐标或关系）
given Point   A(0, 0)       -- 已知坐标的点
given Point   B(10, 0)      -- 已知坐标的点
given Point   C on_circle(O, r=5)  -- 已知在圆上，但坐标未完全固定
given Segment AB between(A, B)     -- 已知线段
given Angle   theta_ABC at(B, A, C) = 60deg  -- 已知角度

# 2. 声明构造的辅助元素
construct midpoint M_AB of AB        -- 中点
construct angle_bisector bisect_B of angle_ABC -- 角平分线
construct perpendicular_perp from M_AB to AB  -- 中垂线
construct intersection X of line_AM and line_BN  -- 交点

# 3. 声明几何约束（目标）
constraint collinear(P, Q, R)           -- 共线
constraint concurrent(l1, l2, l3)       -- 共点
constraint parallel(l1, l2)             -- 平行
constraint perpendicular(l1, l2)        -- 垂直
constraint congruent(seg1, seg2)        -- 长度相等
constraint cyclic(A, B, C, D)           -- 四点共圆

# 4. 声明优化目标
objective minimize  construction_steps
objective minimize  auxiliary_points
objective minimize  proof_complexity
```

对应的解析器接口：

```c
/**
 * @brief 解析 Lv-00 几何描述 DSL
 *
 * 借鉴 IPOPT 的 AmplTNLP 类：解析文本描述，生成内部数据结构
 * （在 IPOPT 中是约束和变量的数值表达，在 Lv-00 中是 ConstraintGraph）。
 *
 * 文件: include/lv00/geometry_dsl.h
 */

typedef struct Lv00GeometryDSL {
    Lv00ConstraintGraph* graph;
    Lv00SymbolicCoord**  given_points;     // 已知点
    int                  n_given_points;
    Lv00SymbolicCoord**  constructed_points; // 构造点
    int                  n_constructed_points;
    Lv00Constraint**     goals;             // 待证明的几何目标
    int                  n_goals;
} Lv00GeometryDSL;

Lv00RetCode lv00_geometry_dsl_parse_file(Lv00GeometryDSL* dsl,
                                          const char* filepath);
Lv00RetCode lv00_geometry_dsl_parse_string(Lv00GeometryDSL* dsl,
                                            const char* source);
void        lv00_geometry_dsl_free(Lv00GeometryDSL* dsl);
```

---

## 4. 实现路线图

### 4.1 分阶段规划

| 阶段 | 名称 | 预计工期 | 产出物 | 依赖 |
|------|------|----------|--------|------|
| Phase 1 | 求解器抽象层 + 线性后端选择器 | 3-4 周 | `solver_backend.h/c`、GMP Gauss/KLU/LAPACK 三后端 | 无 |
| Phase 2 | 内点法主循环 + 过滤线搜索 | 4-5 周 | `solver_loop.c`、`filter_line_search.c` | Phase 1 |
| Phase 3 | 自动微分 + 几何描述 DSL | 3-4 周 | `autodiff.c`、`geometry_dsl.h/c` 和解析器 | Phase 2 |
| Phase 4 | 多语言绑定层 | 2-3 周 | WASM 导出、Tauri 命令、Python 绑定 | Phase 3 |

### 4.2 Phase 1：求解器抽象层 + 线性后端选择器

**目标**：建立可插拔的线性求解器后端体系，运行时根据问题特征自动选择。

**任务清单**：

1. 设计 `Lv00LinearSolver` 虚函数表接口（`include/lv00/solver_backend.h`）
2. 实现 GMP 精确高斯消元后端（`src/backend_exact_gauss.c`）：使用 `mpq_t` 做有理数消元
3. 实现 SuiteSparse KLU 稀疏后端（`src/backend_sparse_klu.c`）：链接 SuiteSparse 库
4. 实现 LAPACK 稠密近似后端（`src/backend_approx_dense.c`）：链接 BLAS/LAPACK
5. 实现 `lv00_select_backend()` 自动选择逻辑
6. 编写单元测试：对比三种后端在小/中/大规模问题上的精度和性能

**关键数据结构**：

```c
typedef struct Lv00BackendRegistry {
    Lv00LinearSolver backends[LV00_LSOLV_COUNT];
    bool             available[LV00_LSOLV_COUNT]; // 某些后端可能不可用
} Lv00BackendRegistry;
```

**验收标准**：
- 对 100 变量的线性系统，三种后端结果一致（数值后端精度在 1e-10 内）
- 对 5000 变量的稀疏系统，稀疏后端速度至少是稠密后端的 10 倍

### 4.3 Phase 2：内点法主循环 + 过滤线搜索

**目标**：实现借鉴 IPOPT 的内点法求解循环，并适配 Lv-00 几何求解语义。

**任务清单**：

1. 实现 `lv00_ipm_init()` 初始化函数
2. 实现 `lv00_ipm_step()` 单步迭代
3. 实现 KKT 系统的构造（将约束和变量表示为块稀疏矩阵）
4. 实现 `lv00_filter_line_search()` 双目标过滤线搜索
5. 实现 `lv00_ipm_check_convergence()` 收敛判定
6. 为默认问题（三角形重心定理证明）编写集成测试

**验收标准**：
- 能对 5 点、10 约束的简单三角形问题收敛到正确解
- 过滤线搜索在"探索性步长"和"保守步长"之间呈现合理的折中

### 4.4 Phase 3：自动微分 + 几何描述 DSL

**目标**：实现约束图的自动梯度/Hessian 计算和几何问题的声明式描述语法。

**任务清单**：

1. 实现 `lv00_autodiff_record()` 磁带录制
2. 实现 `lv00_autodiff_forward()` 前向求值
3. 实现 `lv00_autodiff_reverse()` 反向梯度传播
4. 设计几何描述 DSL 的完整语法（EBNF 定义）
5. 使用 Flex/Bison 或手写递归下降解析器实现 DSL 解析器
6. 实现 `lv00_geometry_dsl_parse_file()` 和 `lv00_geometry_dsl_parse_string()`
7. 编写若干几何定理描述的 DSL 示例文件

**验收标准**：
- 自动微分在 100 变量 50 约束的问题上梯度误差 < 1e-12（与手动解析梯度对比）
- DSL 解析器能正确解析三角形、四边形、圆的完整几何描述

### 4.5 Phase 4：多语言绑定层

**目标**：参考 IPOPT 的多语言接口设计，为 Lv-00 提供 C/C++/Python/WASM 绑定。

**任务清单**：

1. 设计 C 头文件 `lv00_c_api.h`（纯 C 接口，避免 C++ ABI 问题）
2. 实现 WASM 导出：将 C API 通过 Emscripten 编译为 WebAssembly
3. 实现 Tauri 后端命令：封装 WASM / FFI 调用
4. 实现 Python `ctypes` 绑定：Python 端调用 C 共享库
5. 编写各语言绑定的示例程序

**验收标准**：
- Python 脚本能通过绑定库声明几何问题并求解
- WASM 版本能在浏览器中运行基本几何证明

---

## 5. 附录

### 5.1 关键资源

| 资源 | 链接 |
|------|------|
| IPOPT 官方网站 | https://coin-or.github.io/Ipopt/ |
| IPOPT GitHub 仓库 | https://github.com/coin-or/Ipopt |
| IPOPT 用户文档 | https://coin-or.github.io/Ipopt/OPTIONS.html |
| IPOPT 核心论文（Wachter & Biegler, 2006） | Mathematical Programming, 106(1): 25-57 |
| 过滤线搜索论文 | SIAM Journal on Optimization, 16(1): 1-31, 2005 |
| CppAD 自动微分 | https://github.com/coin-or/CppAD |
| COIN-OR 项目总览 | https://www.coin-or.org |
| HSL 数学软件库 | https://www.hsl.rl.ac.uk |
| MUMPS 稀疏求解器 | https://mumps-solver.org |

### 5.2 IPOPT 与同类型求解器对比

| 求解器 | 核心算法 | 语言 | 许可证 | 自动微分 | 适用场景 |
|--------|---------|------|--------|---------|---------|
| IPOPT | 原对偶内点法 + 过滤线搜索 | C++ | EPL 2.0 | CppAD / 手动 | 大规模稀疏 NLP |
| SNOPT | 序列二次规划 (SQP) | Fortran | 商业 | 手动提供 | 稠密 / 中小规模 NLP |
| KNITRO | 内点法 + SQP + 主动集 | C | 商业 | 内置 | 通用 NLP，性能最优 |
| WORHP | 序列二次规划 | C | 商业/学术免费 | 内置 | 航空航天轨道优化 |
| filtSQP | SQP + 过滤 | C | LGPL | 手动提供 | 中小规模 NLP |
| LOQO | 内点法 | C | 商业 | 手动提供 | 凸 NLP |

### 5.3 IPOPT 关键参数速查

| 参数名 | 类型 | 默认值 | Lv-00 建议值 | 说明 |
|--------|------|--------|-------------|------|
| `tol` | double | 1e-8 | 1e-10 | 收敛容忍度（几何问题需高精度） |
| `max_iter` | int | 3000 | 500 | 最大迭代次数（几何问题规模较小） |
| `mu_strategy` | string | "monotone" | "adaptive" | 障碍参数更新策略 |
| `linear_solver` | string | "mumps" | (自动选择) | 线性求解器选择 |
| `hessian_approximation` | string | "exact" | "exact" | Hessian 计算方式（精确/有限差分） |
| `acceptable_tol` | double | 1e-6 | 1e-8 | "可接受"解的容忍度 |
| `nlp_scaling_method` | string | "gradient-based" | "none" | NLP 缩放方法（几何问题通常不需要） |
| `print_level` | int | 5 | 3 | 日志输出级别 |

### 5.4 术语对照表

| 英文术语 | 中文翻译 | 首次出现章节 |
|----------|----------|-------------|
| Primal-Dual Interior Point Method | 原对偶内点法 | 1.2 |
| Filter Line Search | 过滤线搜索 | 1.2 |
| Barrier Problem | 障碍问题 | 1.2 |
| KKT System | KKT 系统 | 2.1 |
| Automatic Differentiation | 自动微分 | 2.2 |
| Sparse Jacobian | 稀疏雅可比矩阵 | 2.4 |
| Compressed Sparse Row (CSR) | 压缩稀疏行 | 2.4 |
| Reverse Mode AD | 反向模式自动微分 | 3.3 |
| Constraint Violation | 约束违反度 | 3.2 |
| Barrier Parameter (mu) | 障碍参数 | 1.2 |
| Lagrangian Multiplier | 拉格朗日乘子 | 1.2 |
| NLP (Nonlinear Programming) | 非线性规划 | 1.1 |
| EPL (Eclipse Public License) | Eclipse 公共许可证 | 1.4 |
| Gröbner Basis | Gröbner 基 | 2.4 |

---

> 文档版本: v1.0 | 最后更新: 2026-05-24 | 作者: Lv-00 开发团队

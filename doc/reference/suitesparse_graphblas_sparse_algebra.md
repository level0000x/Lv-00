# SuiteSparse / GraphBLAS：稀疏矩阵代数与图计算引擎

## 项目概述

SuiteSparse 是由德州农工大学（Texas A&M University）Tim Davis 教授主导开发的 C 语言稀疏矩阵算法套件。该套件自上世纪 90 年代起持续演进，至今已发展为涵盖十余个子包的工程级数值计算基础设施。其核心理念是"以最少的依赖、最高的性能，求解最大规模的稀疏线性系统与图问题"。

### 套件构成

SuiteSparse 的顶层组织如下：

| 子包 | 功能 | 核心算法 |
|------|------|----------|
| CHOLMOD | 稀疏 Cholesky 分解 | 超节点法（Supernodal），支持 LDL^T 变体 |
| UMFPACK | 稀疏 LU 分解 | 非对称多波前法（Unsymmetric MultiFrontal） |
| SPQR | 稀疏 QR 分解 | 多波前 QR，支持秩亏矩阵 |
| GraphBLAS | 图算法标准 C API | Semiring 抽象矩阵运算 |
| CSparse / CXSparse | 轻量级稀疏矩阵基础库 | CSC 压缩存储，教育级实现 |
| LDL | 简单 LDL^T 分解 | 无超节点，纯分解 |
| AMD / CAMD / CCOLAMD / COLAMD | 列排序算法 | 近似最小度（Approximate Minimum Degree） |
| KLU | 电路仿真专用 LU | BTF（Block Triangular Form）预处理 |
| Mongoose | 图分割 | 组合方法 |
| SPEX | 高精度 LU 分解 | GNU MPFR 多精度 |

### GraphBLAS 的地位

GraphBLAS 是 SuiteSparse 中一个相对独立但战略位置极为重要的子包。它实现了 **GraphBLAS C API Specification**——一套由学术界与工业界联合制定的"用线性代数语言表达图算法"的标准接口。其核心思想极其优雅：

> **图算法 = 稀疏矩阵乘法 + 用户定义的加法与乘法运算**

具体地，任何图遍历、最短路径、连通分量等问题，都可以表达为在某种 **Semiring（半环）** 上的稀疏矩阵-向量或矩阵-矩阵乘法。GraphBLAS 提供了约 400 个函数，覆盖矩阵创建、元素操作、Semiring 组合、归约与提取，且全部为非阻塞式（non-blocking）实现，支持惰性求值与自动融合。

### 生态影响力

- **MATLAB**：自 R2007b 起，`\`（反斜杠）运算符的稀疏求解后端即为 SuiteSparse（CHOLMOD / UMFPACK）；
- **Julia**：SparseArrays 标准库在部分平台上调用 SuiteSparse 的 CHOLMOD；
- **Ceres Solver**：Google 的非线性最小二乘优化库，其 `SPARSE_NORMAL_CHOLESKY` 求解器直接依赖 SuiteSparse；
- **Octave**、**SciPy**（Python）、**CVXOPT** 等大量科学计算软件均直接或间接依赖该套件。

SuiteSparse 的纯 C 实现与对外部依赖的极度克制（除 BLAS/LAPACK 外无依赖），使其成为"移植到新平台成本最低的高性能稀疏求解器"。

### GraphBLAS 设计哲学

GraphBLAS 的设计遵循几个关键原则，这些原则对 Lv-00 的约束引擎设计具有直接的指导价值：

1. **操作即数学，非算法**：用户指定"做什么"（矩阵乘法在某种半环上），而非"怎么做"。运行时负责调度、并行化、内存管理。这相当于将约束传播声明式地定义在 Semiring 层面，由引擎优化执行。

2. **非阻塞语义（Non-blocking）**：`GrB_mxm` 返回后，实际计算可能尚未执行。这允许运行时将多个操作融合（operator fusion），减少中间结果的物化与内存分配。对于 Lv-00 中约束传播的多步迭代，这种模式可以显著减少内存带宽压力。

3. **结构类型（Structural typing）**：矩阵的运算行为由元素的类型（`GrB_Type`）和半环定义，而非矩阵自身的类型。这允许同一张约束图在不修改存储结构的情况下，切换不同的传播策略——例如从距离传播切换到角度传播。

4. **掩码（Mask）**：每个核心操作（`GrB_mxm`、`GrB_mxv`、`GrB_reduce`）都接受一个可选的输出掩码矩阵/向量。掩码控制了"写入哪些位置"，是一种零成本的稀疏结构操作。Lv-00 中可以利用掩码在约束传播中跳过"已完全确定的变量"。

5. **描述符（Descriptor）**：GrB_Descriptor 控制操作的行为——是否转置输入、是否替换输出、是否使用结构/数值模式等。这为 Lv-00 的求解器配置提供了统一的控制面板。

---

## 核心借鉴点

### 1. 稀疏矩阵的多重分解策略

SuiteSparse 对同一种数据结构（CSC 稀疏矩阵）提供三种正交分解路径：

- **LU 分解**（UMFPACK）：针对非对称系统 Ax = b，自动检测矩阵是否对称以选择优化路径；
- **Cholesky 分解**（CHOLMOD）：针对对称正定（SPD）系统，利用超节点结构加速；
- **QR 分解**（SPQR）：针对最小二乘问题或秩亏矩阵，提供正交分解。

这三种分解均在 CSC 格式下展开，共享排序（AMD / COLAMD）基础设施，互不冲突。

**对 Lv-00 的启示**：Lv-00 的几何约束系统可抽象为稀疏矩阵（Jacobian / Hessian）。不同约束类型对应不同矩阵性质（如距离约束对称、方向约束非对称），需要不同的求解策略。SuiteSparse 的多重分解设计为 Lv-00 的 `linear_solve` 路径提供了直接的"路由层"参考：根据约束矩阵的代数性质（对称性、正定性），自动选择分解方法。

### 2. GraphBLAS 的 Semiring 抽象

GraphBLAS 将矩阵乘法泛化为 **Semiring（半环）**：一个三元组 `(domain, monoid, semigroup)` 或等价地记作 `(加半群, 乘幺半群)`。

在经典线性代数中，`C = A*B` 定义如下：

```
C(i,j) = sum_k (A(i,k) * B(k,j))
```

其中 `+` 是普通加法，`*` 是普通乘法，构成 `(R, +, *)` 半环。

GraphBLAS 将 `+` 和 `*` 替换为任意用户定义的二元运算。例如：

- `min-plus` 半环：`C(i,j) = min_k (A(i,k) + B(k,j))` ——直接给出最短路径；
- `lor-land` 半环：`C(i,j) = OR_k (A(i,k) AND B(k,j))` ——给出 BFS 可达性；
- `max-times` 半环：`C(i,j) = max_k (A(i,k) * B(k,j))` ——最大容量路径。

**Lv-00 约束的 Semiring 映射**：以距离约束为例：

- **元素类型**：`Interval = [lo, hi]`，表示一个变量允许的取值范围；
- **乘法 ⊗**（传播）：已知节点 A 的区间 `[lo_A, hi_A]` 和边上的距离约束 `[d_min, d_max]`，推导节点 B 的区间：
  ```
  propagate([lo_A, hi_A], [d_min, d_max]) = [lo_A - d_max, hi_A + d_max]
                                           ∩ [lo_A + d_min, hi_A - d_min]
  ```
  注意：若 hi_A - d_min < lo_A + d_min，则区间为空——约束不可满足，需回溯；
- **加法 ⊕**（合并）：若节点 B 从多条路径收到区间 `I_1, I_2, ..., I_n`，取所有区间的交集：
  ```
  merge(I_1, I_2) = [max(lo_1, lo_2), min(hi_1, hi_2)]
  ```
- **幺元**：加法的幺元是 `[-inf, +inf]`（即无约束），乘法的幺元是 `[0, 0]`（即零距离，恒等传播）。

类似地，角度约束、平行约束、垂直约束等可以各自定义其 Semiring，形成约束类型的代数化编码。

**对 Lv-00 的启示**：Lv-00 的 `constraint_graph` 上约束传播的本质是：给定节点间的已有约束，推导新的约束关系。这恰好契合 Semiring 框架——只需为每种约束类型定义其对应的"乘法"（传播规则）和"加法"（合并规则），GraphBLAS 即可自动执行传播。更重要的是，不同约束类型的 Semiring 可以组合：例如先进行距离传播，收敛后再进行角度传播，两者的迭代表达了约束系统的联合推导。

### 3. 矩阵--图的统一视角

GraphBLAS 的根本洞见在于：**图的邻接矩阵与图本身是同构的**。

- **邻接矩阵** A：A(i,j) = 1 当且仅当存在边 i→j。那么 A^k 的 (i,j) 元素给出恰好 k 步可达的路径数；
- **BFS**：从源节点 s 出发，向量 x（x[s]=1）与 A 在半环 `(lor, land)` 上不断相乘，直到收敛；
- **关联矩阵**：约束图可以表示为关联矩阵 B，其中 B(i,j) 表示约束 i 涉及几何体 j。

**对 Lv-00 的启示**：Lv-00 的约束图天然地映射为稀疏约束矩阵。节点 = 几何体或约束变量，边 = 约束关系，权重 = 约束参数。矩阵乘法的幂即约束传播的步数。

### 4. 纯 C 实现，无外部依赖

SuiteSparse（除 GraphBLAS 自身的 JIT 编译需要用到的 C 编译器外）的核心完全用 ANSI C 编写。唯一的可选外部依赖是 BLAS，且提供了内置的回退实现。这意味着 SuiteSparse 可以在任何有 C99 编译器的平台上编译运行。

**对 Lv-00 的启示**：Lv-00 内核同样是纯 C（或带 minimal C++ runtime）。技术栈一致意味着可以直接将 SuiteSparse 作为子模块嵌入，零跨语言 FFI 成本。

### 5. 列压缩存储（CSC）格式

CSC 格式是 SuiteSparse 的核心存储格式：

```
typedef struct {
    int64_t *p;    // 列指针，p[j] = 第 j 列第一个非零元的索引
    int64_t *i;    // 行索引，i[p[j]..p[j+1]-1] 给出第 j 列非零元的行号
    double  *x;    // 数值数组，x[p[j]..p[j+1]-1] 给出第 j 列非零元的值
    int64_t nrow, ncol, nzmax;  // 维度与容量
} cs_sparse;  // 实际在 SuiteSparse 中是 GrB_Matrix
```

CSC 的优势：
- O(1) 列访问（直接索引 p[j] 到 p[j+1]-1）；
- 节省存储（只存非零元）；
- 列优先顺序天然适合矩阵-向量乘（GEMV）和列方向的分解算法。

**对 Lv-00 的启示**：Lv-00 的约束矩阵应当使用 CSC 格式。约束矩阵通常极端稀疏（每个变量仅参与少数约束），CSC 可以将其内存占用从 O(mn) 降至 O(nnz)，nnz 通常远小于 mn。

### 对照表：SuiteSparse 概念与 Lv-00 对应

| SuiteSparse 概念 | Lv-00 对应 | 借鉴价值 |
|------------------|-----------|---------|
| `GrB_Matrix`（稀疏矩阵） | `constraint_matrix`（约束雅可比） | 使用 CSC 格式存储约束矩阵，O(nnz) 内存 |
| `GrB_Vector`（稀疏向量） | `param_vector`（参数向量） | 约束变量以稀疏向量表示，支持批量运算 |
| CHOLMOD 超节点 Cholesky | `linear_solve` 正定路径 | 加速对称约束系统的求解（如距离约束） |
| UMFPACK 多波前 LU | `linear_solve` 非对称路径 | 处理方向、角度等非对称约束 |
| SPQR 稀疏 QR | `linear_solve` 最小二乘路径 | 处理过约束系统与冗余约束检测 |
| Semiring `(⊕, ⊗)` | 约束传播规则 `(merge, propagate)` | ⊕ = 合并同类型约束，⊗ = 传播相邻约束 |
| `GrB_mxm`（矩阵乘法） | `constraint_propagate`（约束传播） | 一步"传播"等价于一次半环矩阵乘法 |
| `GrB_reduce`（归约） | `constraint_evaluate`（残差计算） | 归约操作计算总约束违反量 |
| AMD / COLAMD（列排序） | 变量排序预处理 | 减少 fill-in，加速稀疏分解 |
| CSC 格式 `(p, i, x)` | `ConstraintMatrix` 内部存储 | 直接复用，零转换成本 |
| GraphBLAS non-blocking 模式 | Lv-00 惰性求值管道 | 累积多次更新后一次性执行，减少内存分配 |
| `GrB_Descriptor`（描述符） | 求解器配置结构 | 统一控制矩阵运算行为（转置、结构/数值模式等） |
| SuiteSparse 自带 BLAS | Lv-00 内建基本线性代数 | 避免外部 BLAS 依赖，降低部署复杂度 |

---

## Lv-00 映射方案

### 整体架构

Lv-00 的约束系统可以抽象为三层，每层对应 SuiteSparse 的一个子包：

```
+------------------------------------+
|  约束传播层 → GraphBLAS (Semiring) |
+------------------------------------+
|  线性求解层 → CHOLMOD / UMFPACK    |
+------------------------------------+
|  存储 / 排序层 → CXSparse (CSC)    |
+------------------------------------+
```

### 方案一：CHOLMOD 加速 linear_solve

Lv-00 当前 `linear_solve` 路径使用纯 GMP 高精度有理数求解（用于定理证明场景的可决定性），但对于数值验证场景（`verify_numerical`），GMP 的精度是过剩的，而性能是关键瓶颈。引入 CHOLMOD 作为快速路径：

```c
// lv_solver.h —— Lv-00 求解器接口扩展
typedef enum {
    LV_SOLVE_GMP,       // GMP 有理数求解（证明模式）
    LV_SOLVE_CHOLMOD,   // CHOLMOD double 求解（验证模式）
    LV_SOLVE_UMFPACK    // UMFPACK 求解（非对称验证模式）
} LvSolveMode;

typedef struct {
    LvSolveMode mode;
    // CHOLMOD 上下文
    cholmod_common  chol_common;
    cholmod_sparse *chol_A;
    cholmod_dense  *chol_b;
    cholmod_dense  *chol_x;
    cholmod_factor *chol_L;
    // 元数据
    bool is_spd;       // 是否对称正定
    bool is_symmetric; // 是否对称（不一定正定）
    bool factorized;   // 是否已分解
} LvSolver;
```

**核心流程**：

```c
// lv_linear_solve.c —— CHOLMOD 加速路径
int lv_solve_with_cholmod(LvSolver *solver,
                          const ConstraintMatrix *cm,
                          const double *rhs,
                          double *result)
{
    cholmod_common *cc = &solver->chol_common;

    // 步骤 1：从 ConstraintMatrix 构建 cholmod_sparse
    // ConstraintMatrix 本身已是 CSC 格式，可直接指针复用
    cholmod_sparse *A = cholmod_allocate_sparse(
        cm->nrow, cm->ncol, cm->nnz,
        TRUE,   /* 已排序 */
        TRUE,   /* 已打包 */
        1,      /* 对称（下三角存储） */
        CHOLMOD_REAL, cc);

    // 直接将 ConstraintMatrix 的 p/i/x 赋给 cholmod 结构
    // 注意：需要确保 cm->x 的生命周期长于本次求解
    memcpy(A->p, cm->col_ptr, (cm->ncol + 1) * sizeof(int64_t));
    memcpy(A->i, cm->row_idx, cm->nnz * sizeof(int64_t));
    memcpy(A->x, cm->values,  cm->nnz * sizeof(double));

    // 步骤 2：符号分析与数值分解（仅首次或结构变更后执行）
    if (!solver->factorized) {
        // AMD 排序以减少 fill-in
        cholmod_factor *L = cholmod_analyze(A, cc);
        // 数值分解
        cholmod_factorize(A, L, cc);
        solver->chol_L = L;
        solver->factorized = TRUE;
    }

    // 步骤 3：构建右端项（dense）
    cholmod_dense *b = cholmod_allocate_dense(
        cm->nrow, 1, cm->nrow, CHOLMOD_REAL, cc);
    memcpy(b->x, rhs, cm->nrow * sizeof(double));

    // 步骤 4：求解
    cholmod_dense *x = cholmod_solve(
        CHOLMOD_A, solver->chol_L, b, cc);

    // 步骤 5：提取结果
    memcpy(result, x->x, cm->ncol * sizeof(double));

    // 清理临时对象（L 和 A 需保留至下次 refactorize）
    cholmod_free_dense(&b, cc);
    cholmod_free_dense(&x, cc);

    return 0;
}
```

### 方案二：GraphBLAS Semiring 映射约束传播

Lv-00 约束图中，每条边携带一个约束类型（距离、角度、平行、垂直等）。约束传播算法可以形式化为：

> 给定约束图 `G = (V, E)`，每条边 `e = (i,j)` 有约束参数 `w`，定义传播规则 `propagate`：若已知节点 i 的状态区间，则可由 `w` 推导节点 j 的状态区间。

在 GraphBLAS 框架中，这对应于一次 `v = v_mxv(A, v, semiring)` 操作：

```c
// lv_constraint_propagate.c —— GraphBLAS 约束传播引擎
#include "GraphBLAS.h"

// 自定义 Semiring 回调：距离约束的传播
// multiply = 传播规则：已知 input_interval，经由 weight 推算 neighbor_interval
void distance_propagate(void *z, const void *x, const void *y) {
    // x: 当前节点的已知区间 [lo, hi]
    // y: 边的约束区间 [d_min, d_max]
    // z: 邻居节点应满足的区间 = x ⊕ y = [lo-d_max, hi+d_max] ∩ [lo+d_min, hi-d_min]
    double *z_val = (double *)z;
    const double *x_val = (const double *)x;
    const double *y_val = (const double *)y;
    z_val[0] = fmax(x_val[0] - y_val[1], x_val[0] + y_val[0]);  // new_lo
    z_val[1] = fmin(x_val[1] + y_val[1], x_val[1] - y_val[0]);  // new_hi
}

// add = 合并规则：多个邻居传播的结果取交集
void interval_merge(void *z, const void *x, const void *y) {
    double *z_val = (double *)z;
    const double *x_val = (const double *)x;
    const double *y_val = (const double *)y;
    z_val[0] = fmax(x_val[0], y_val[0]);  // lo = max(lo1, lo2)
    z_val[1] = fmin(x_val[1], y_val[1]);  // hi = min(hi1, hi2)
}

// 构建 GrB_Semiring
GrB_Semiring interval_semiring;
GrB_Monoid   interval_add_monoid;
GrB_BinaryOp interval_add_op, interval_mul_op;

GrB_BinaryOp_new(&interval_mul_op, distance_propagate,
                  GrB_FP64, GrB_FP64, GrB_FP64);
GrB_BinaryOp_new(&interval_add_op, interval_merge,
                  GrB_FP64, GrB_FP64, GrB_FP64);
GrB_Monoid_new_FP64(&interval_add_monoid, interval_add_op, 0.0);
GrB_Semiring_new(&interval_semiring, interval_add_monoid, interval_mul_op);

// 约束传播主循环
int lv_constraint_propagate_graphblas(
    GrB_Matrix A,          // 约束图邻接矩阵（稀疏，CSC）
    GrB_Vector x,          // 初始已知区间向量
    int max_iterations)
{
    GrB_Vector x_prev;
    GrB_Vector_dup(&x_prev, x);

    for (int iter = 0; iter < max_iterations; iter++) {
        // 核心操作：v = v + A * x  在 interval_semiring 下
        // 每个元素类型为 double[2]（lo, hi），通过 UDT 定义
        GrB_mxv(x, GrB_NULL, GrB_NULL,
                interval_semiring, A, x_prev, GrB_NULL);

        // 检查是否收敛（所有区间不再缩小）
        GrB_Index nvals_x, nvals_prev;
        GrB_Vector_nvals(&nvals_x, x);
        GrB_Vector_nvals(&nvals_prev, x_prev);
        if (nvals_x == nvals_prev) {
            // 进一步检查值是否变化
            // ... (省略具体比较逻辑)
            break;
        }

        GrB_assign(x_prev, x, GrB_NULL);
    }

    GrB_free(&x_prev);
    return 0;
}
```

**关键洞察**：上述代码中，`GrB_mxv` 的单次调用即完成了"每个节点从其所有邻居收集约束、合并、推导新区间"的完整传播步。GraphBLAS 内部使用 Gustafson 算法进行并行化，在 GPU 或多核 CPU 上自动加速。

### 方案三：约束矩阵增量更新

Lv-00 中用户交互式修改几何约束（拖拽点、添加/删除约束）时，约束矩阵结构发生变化。与其每次重新构建矩阵，不如利用 SuiteSparse 的增量更新能力：

```c
// CHOLMOD 增量更新约束矩阵
int lv_solver_update_constraint(LvSolver *solver,
                                int removed_var_index,
                                int added_constraint_row,
                                const double *new_row_values,
                                const int    *new_row_indices,
                                int new_row_nnz)
{
    cholmod_common *cc = &solver->chol_common;

    // 步骤 1：CHOLMOD 支持 rank-1 更新 / 删除
    // 删除变量等价于删除对应列
    if (removed_var_index >= 0) {
        // 使用 CHOLMOD 的 rowdel / coldel
        // 注意：这会使分解无效，需 refactorize
        solver->factorized = FALSE;
    }

    // 步骤 2：新增约束行（追加到矩阵底部）
    if (added_constraint_row >= 0) {
        // 更新 ConstraintMatrix 的 CSC 结构
        // 注意：CSC 添加行比添加列更昂贵，可能需要 CSR 转置
        // 实际实现中，约束矩阵以 CSR 增量维护，求解前转 CSC
        solver->factorized = FALSE;
    }

    // 步骤 3：增量 refactorize（若 CHOLMOD 支持）
    if (!solver->factorized) {
        // 结构未变时可用 cholmod_rowadd / cholmod_updown
        // 结构已变时需完整 refactorize
        cholmod_factorize(solver->chol_A, solver->chol_L, cc);
        solver->factorized = TRUE;
    }

    return 0;
}
```

---

## 实现路线图

Lv-00 集成 SuiteSparse 划分为三个阶段，每阶段产出独立可测试的功能。

### 阶段 1：集成 CHOLMOD，替换纯 GMP 线性求解（数值验证模式）

| 项目 | 说明 |
|------|------|
| **目标** | 在 `verify_numerical` 模式下，使用 CHOLMOD 的 `double` 精度快速求解，GMP 保留为证明模式 |
| **前置条件** | CMake 构建系统能检出 SuiteSparse 子模块（或通过 vcpkg/conan 安装） |
| **主要工作** | 1. 将 `ConstraintMatrix` 的内部存储统一为 CSC 格式；2. 封装 `LvSolver` 结构体与模式选择；3. 实现 `lv_solve_with_cholmod()`；4. 编写数值精度对比测试（GMP vs CHOLMOD，精度差异应 < 1e-10） |
| **可测试产出** | `lv-solver-bench` 基准测试工具，对比 GMP / CHOLMOD 的速度与精度 |
| **预计工期** | 3--4 周 |
| **风险** | license 兼容性（LGPL-2.1+ 可与 MIT/BSD 兼容，但需仔细确认） |
| **依赖** | SuiteSparse:CHOLMOD, SuiteSparse:AMD, SuiteSparse:COLAMD, SuiteSparse:ccolamd |

### 阶段 2：GraphBLAS 约束传播引擎

| 项目 | 说明 |
|------|------|
| **目标** | 将 Lv-00 的 `constraint_propagate` 实现替换为基于 GraphBLAS Semiring 的版本 |
| **前置条件** | 阶段 1 完成；约束图已以 GrB_Matrix 形式表示 |
| **主要工作** | 1. 定义每个约束类型对应的 GrB_Semiring（距离 → interval_semiring, 角度 → angular_semiring 等）；2. 实现 `lv_constraint_propagate_graphblas()`；3. 利用 GrB_Descriptor 控制惰性求值与并行策略；4. 编写等价性测试（原实现 vs GraphBLAS 实现的结果一致性） |
| **可测试产出** | `lv-propagate-test` 回归测试套件；传播性能对比报告 |
| **预计工期** | 4--6 周 |
| **风险** | GraphBLAS 的 JIT 编译在 Windows 平台可能需要额外配置；Semiring 设计的可扩展性需要仔细抽象 |
| **依赖** | SuiteSparse:GraphBLAS |

### 阶段 3：稀疏矩阵增量更新与热加载

| 项目 | 说明 |
|------|------|
| **目标** | 支持交互式修改约束后，避免完整重构矩阵与分解，实现增量更新 |
| **前置条件** | 阶段 1 完成 |
| **主要工作** | 1. 实现 CSC/CSR 双格式维护（增量更新用 CSR，求解用 CSC）；2. 封装 CHOLMOD 的 `cholmod_updown` / `cholmod_rowadd`；3. 实现结构未变时的快速因子更新（rank-1 update）；4. 触发策略：仅当 fill-in 或结构变更超过阈值时完整 refactorize |
| **可测试产出** | `lv-incremental-bench` 基准测试：增量更新 vs 完整重构的性能对比 |
| **预计工期** | 3--5 周 |
| **风险** | 增量更新的数值稳定性不如完整分解（需监控条件数）；CSC/CSR 双维护增加内存开销 |
| **依赖** | SuiteSparse:CHOLMOD, SuiteSparse:AMD |

### 路线图总览

```
阶段 1（第1--4周）                  阶段 2（第5--10周）              阶段 3（第11--15周）
┌─────────────────────┐     ┌──────────────────────┐     ┌──────────────────────┐
│ CHOLMOD 集成        │     │ GraphBLAS 约束传播   │     │ 增量更新              │
│                     │     │                      │     │                      │
│ • CSC 统一存储      │ ──▶ │ • Semiring 库        │ ──▶ │ • CSR/CSC 双格式     │
│ • LvSolver 封装     │     │ • 并行传播引擎       │     │ • rank-1 update     │
│ • GMP/CHOLMOD 双模式│     │ • 惰性求值管道       │     │ • 条件数监控         │
│ • 精度对比基准      │     │ • 等价性验证         │     │ • 自适应重建策略     │
└─────────────────────┘     └──────────────────────┘     └──────────────────────┘
```

---

## 附录

### 关键 API 参考

**CHOLMOD 核心函数**：

```c
// 初始化和清理
int cholmod_start(cholmod_common *Common);
int cholmod_finish(cholmod_common *Common);

// 稀疏矩阵创建
cholmod_sparse *cholmod_allocate_sparse(
    size_t nrow, size_t ncol, size_t nzmax,
    int sorted, int packed, int stype, int xtype,
    cholmod_common *Common);

// 符号分析与数值分解
cholmod_factor *cholmod_analyze(
    cholmod_sparse *A, cholmod_common *Common);
int cholmod_factorize(
    cholmod_sparse *A, cholmod_factor *L,
    cholmod_common *Common);

// 求解
cholmod_dense *cholmod_solve(
    int sys,        // CHOLMOD_A / CHOLMOD_LDLt / etc.
    cholmod_factor *L,
    cholmod_dense *B,
    cholmod_common *Common);

// 增量更新（rank-1）
int cholmod_updown(
    int update,     // +1 = add, -1 = delete
    cholmod_sparse *C,  // rank-1 更新矩阵
    cholmod_factor *L,
    cholmod_common *Common);
```

**GraphBLAS 核心函数**：

```c
// 初始化
GrB_Info GrB_init(GrB_Mode mode);  // GrB_NONBLOCKING 或 GrB_BLOCKING

// 创建运算符
GrB_Info GrB_BinaryOp_new(
    GrB_BinaryOp *op,
    GxB_binary_function function,
    GrB_Type tx, GrB_Type ty, GrB_Type tz);
GrB_Info GrB_Monoid_new_FP64(
    GrB_Monoid *monoid, GrB_BinaryOp op, double identity);
GrB_Info GrB_Semiring_new(
    GrB_Semiring *semiring, GrB_Monoid add, GrB_BinaryOp mul);

// 矩阵创建
GrB_Info GrB_Matrix_new(GrB_Matrix *A, GrB_Type type, GrB_Index nrows, GrB_Index ncols);

// 核心操作
GrB_Info GrB_mxm(
    GrB_Matrix C, const GrB_Matrix Mask,
    const GrB_BinaryOp accum,
    const GrB_Semiring semiring,
    const GrB_Matrix A, const GrB_Matrix B,
    const GrB_Descriptor desc);
GrB_Info GrB_mxv(
    GrB_Vector w, const GrB_Vector mask,
    const GrB_BinaryOp accum,
    const GrB_Semiring semiring,
    const GrB_Matrix A, const GrB_Vector u,
    const GrB_Descriptor desc);
GrB_Info GrB_reduce(
    GrB_Vector w, const GrB_Vector mask,
    const GrB_BinaryOp accum,
    const GrB_Monoid monoid,
    const GrB_Matrix A,
    const GrB_Descriptor desc);

// 惰性求值控制
GrB_Info GrB_wait(GrB_Matrix A, GrB_WaitMode mode);
```

**CXSparse CSC 结构**：

```c
typedef struct cs_sparse {
    int64_t nzmax;      // 最大非零元数
    int64_t m;          // 行数
    int64_t n;          // 列数
    int64_t *p;         // 列指针，长度 n+1
    int64_t *i;         // 行索引，长度 nzmax
    double  *x;         // 数值数组，长度 nzmax
    int64_t nz;         // 当前非零元数
} cs;
```

### 许可证

- **SuiteSparse 核心库**（CHOLMOD, UMFPACK, SPQR, AMD, COLAMD, CCOLAMD, CAMD, LDL, CXSparse, KLU, BTF, RBio）：**LGPL-2.1+** 或 **GPL-2.0+** 双许可。
- **GraphBLAS**：**Apache-2.0** 许可，更为宽松，允许静态链接与闭源分发。
- **Mongoose**：**GPL-3.0-only**。

Lv-00 若使用 LGPL-2.1+ 的组件（CHOLMOD 等），需遵守动态链接要求或提供目标文件以允许重新链接。GraphBLAS 的 Apache-2.0 许可证无此限制，可静态链接。建议在 CMake 构建脚本中通过 `SUITESPARSE_USE_STRICT` 选项精确控制链接方式。

### 关键资源

- **GitHub 主仓库**：[https://github.com/DrTimothyAldenDavis/SuiteSparse](https://github.com/DrTimothyAldenDavis/SuiteSparse)
- **GraphBLAS C API 规范 v2.1**：[https://graphblas.org](https://graphblas.org)
- **User Guide (PDF)**：随源码附带于 `GraphBLAS/Doc/GraphBLAS_UserGuide.pdf`
- **Tim Davis 著作**：*"Direct Methods for Sparse Linear Systems"* (SIAM, 2006) ——稀疏直接法领域的权威参考书
- **GraphBLAS 论文**：Kepner, J. et al. "Mathematical Foundations of the GraphBLAS." *IEEE HPEC*, 2016.
- **LAGraph**：[https://github.com/GraphBLAS/LAGraph](https://github.com/GraphBLAS/LAGraph) ——基于 GraphBLAS 的图算法库，可作为 Semiring 设计的参考实现

### 构建集成示例（CMake）

```cmake
# CMakeLists.txt 片段 —— SuiteSparse 子模块集成
include(FetchContent)

FetchContent_Declare(
  SuiteSparse
  GIT_REPOSITORY https://github.com/DrTimothyAldenDavis/SuiteSparse.git
  GIT_TAG        v7.9.0
  GIT_SHALLOW    TRUE
)

set(SUITESPARSE_ENABLE_PROJECTS "suitesparse_config;amd;colamd;ccolamd;cholmod;graphblas"
    CACHE STRING "Enable only needed subprojects")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Static linking")

FetchContent_MakeAvailable(SuiteSparse)

target_link_libraries(lv00_core PRIVATE
  SuiteSparse::CHOLMOD
  SuiteSparse::GraphBLAS
  SuiteSparse::AMD
)
```

### 补充说明

SuiteSparse 自 v7.0.0 起采用统一的 CMake 构建系统，子项目可按需启用。对于 Lv-00 这种"仅需 CHOLMOD + GraphBLAS + AMD + 排序库"的场景，可以通过 `SUITESPARSE_ENABLE_PROJECTS` 变量精确控制，编译时间从完整构建的 20 分钟降至约 2--3 分钟。若需要交叉编译到 WASM 或嵌入式平台，SuiteSparse 的 ANSI C 特性保证了良好的可移植性——只需提供符合 C99 标准的编译器即可。

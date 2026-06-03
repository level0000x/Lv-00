# Singular/Macaulay2 代数引擎增强设计

> **借鉴项目**：Singular（University of Kaiserslautern, singular.uni-kl.de）和 Macaulay2（macaulay2.com）
> **核心借鉴点**：Groebner 基计算后端接口、"环声明"范式与公理包声明的对应、多后端注册与调度
> **分类**：P4 低优先级 / 求解器增强
> **日期**：2026-05-24

---

## 1. 概述

Lv-00 的求解器（`solver.h/c`）目前以内置 Groebner 基求解器（度 <= 2）为核心，并通过 `smt_backend.h` 提供了 Z3 和 cvc5 的 SMT 后端接口。Singular 和 Macaulay2 是公认的符号代数系统，分别在 Groebner 基计算和交换代数方面具有业界领先能力。本文档设计 Lv-00 如何将 Singular 和 Macaulay2 作为外部代数计算后端集成，与现有 `smt_backend.h` 形成互补。

---

## 2. 架构定位：与 smt_backend.h 的互补关系

### 2.1 现有 SMT 后端架构回顾

`smt_backend.h` 定义的 `SolverBackendType` 枚举已预留了 `SMT_SINGULAR`：

```c
typedef enum {
    GROEBNER = 0,      // Groebner 基方法（度数<=2，内置实现）
    SMT_Z3,            // Z3 SMT 求解器
    SMT_CVC5,          // cvc5 SMT 求解器
    SMT_SINGULAR,      // Singular 代数系统（含 SMT 接口）
    COUNT
} SolverBackendType;
```

### 2.2 互补设计

本文档的 CAS 后端设计将 `SMT_SINGULAR` 扩展为完整的代数计算后端体系，并与 SMT 后端形成清晰的职责划分：

| 场景 | 使用后端 | 接口入口 |
|:---|:---|:---|
| 约束可满足性判定（SAT/UNSAT） | SMT_Z3 / SMT_CVC5 | `smtsolver_solve()` |
| Groebner 基计算（任意次数） | CAS_SINGULAR / CAS_M2 | `cas_compute_groebner()` |
| 多项式理想成员判定 | CAS_SINGULAR / CAS_M2 | `cas_ideal_membership()` |
| 消元理想计算 | CAS_SINGULAR / CAS_M2 | `cas_elimination_ideal()` |
| 因式分解（多变量） | CAS_SINGULAR / CAS_M2 | `cas_factor()` |
| 环的声明与切换 | CAS_SINGULAR / CAS_M2 | `cas_ring_declare()` |

新增的 `SolverBackendType` 条目（建议扩展）：

```c
CAS_SINGULAR = 4,    // Singular 符号代数系统
CAS_M2 = 5,          // Macaulay2 交换代数系统
```

---

## 3. Macaulay2 "环声明"范式与 Lv-00 "公理包声明"的对应

### 3.1 Macaulay2 的环声明

Macaulay2 的核心操作是在特定环中执行计算。用户首先声明一个多项式环：

```macaulay2
R = QQ[x, y, z, MonomialOrder => Lex]
I = ideal(x^2 + y^2 - 1, x*y - z)
G = gb I
```

环声明 (1) 指定基域、变量名和单项式序；(2) 后续所有计算默认在此环中执行；(3) 可随时切换到新环。

### 3.2 Lv-00 公理包声明的对应设计

Lv-00 的公理包系统（`axiom_packages/`）已经提供了类似的"上下文声明"范式。一个公理包声明了：

- 变量域（如尺规构造公理包声明了 `POINT` 和 `LINE_SEGMENT` 变量）
- 可用构造子（如线段连接、圆作交）
- 重写规则和推理规则

这与 Macaulay2 的环声明形成直接对应：

| Macaulay2 概念 | Lv-00 对应 | 说明 |
|:---|:---|:---|
| 基域 `QQ`, `ZZ/p` | 公理包中符号坐标的允许类型 | 有理数 / 有限域上的几何 |
| 变量 `x, y, z` | 公理包声明的自由节点类型 | 可自由构造的几何对象 |
| `MonomialOrder` | 公理包的重写规则排序器 | 决定规则应用优先级 |
| `ideal(...)` | 约束图中指定子图的全部约束 | 约束集的代数闭包 |
| `gb I` | `solver` 模块的 Groebner 基计算 | 求解约束系统的规范形式 |

### 3.3 CAS 环声明接口设计

基于以上对应，设计 CAS 后端的环声明接口：

```c
/**
 * @brief CAS 环描述符——对应 Macaulay2 的环声明
 *
 * 从 Lv-00 公理包的声明中自动推导，或由高级用户显式指定。
 */
typedef struct CASRing {
    char *base_field;             // 基域："QQ"（有理数）、"ZZ/p"（有限域）、"RR"（实数）
    char **variable_names;        // 变量名列表（与约束图中的自由节点对应）
    int variable_count;           // 变量数量
    char *monomial_order;         // 单项式序："lex"、"grevlex"、"degrevlex"
    void *backend_specific;       // 后端内部表示
} CASRing;

/**
 * @brief 从公理包声明自动推导 CAS 环
 *
 * @param pkg     公理包句柄
 * @param graph   当前的约束图（用于提取自由变量）
 * @return 自动推导的环描述符（调用者需用 cas_ring_destroy 释放）
 */
CASRing *cas_ring_from_axiom_package(const AxiomPackage *pkg,
                                      const ConstraintGraph *graph);
```

### 3.4 环切换

类似于 Macaulay2 中 `use R` 的环切换，Lv-00 支持在证明的不同阶段使用不同的环：

- 尺规构造阶段 → `QQ[x1..xn, y1..yn]`（有理数域上的多项式环）
- 不可构造性分析阶段 → `QQ[x1..xn, y1..yn][t]/(minimal_poly)`（代数数域上的扩环）
- 有限域演示阶段 → `ZZ/7[x1..xn, y1..yn]`

---

## 4. Singular 集成方案：Groebner 基计算后端

### 4.1 为什么需要替换内置 Groebner 基求解器

Lv-00 当前的内置 Groebner 基求解器（`solver.h` 第 195-197 行）受限于：
- 多项式次数 <= 2
- 使用 GMP 本地计算，无多线程优化
- 不支持复杂的单项式序选择

Singular 作为专门设计的 Groebner 基计算引擎，可以处理任意次数的多项式系统，并内置了 Buchberger 算法的高级优化（slimgb、f5 算法等）。

### 4.2 接口设计

Singular 后端通过进程间通信（fork + pipe）或共享库（libSingular）两种模式集成：

```c
/**
 * @brief 通过 Singular 后端计算 Groebner 基
 *
 * 将 Lv-00 约束图中的代数约束转化为 Singular 脚本，
 * 通过管道发送给 Singular 进程，解析返回的 Groebner 基。
 *
 * @param ring        计算环
 * @param polynomials 输入多项式数组（在 Lv-00 内部表示为 mpz_poly[]）
 * @param poly_count  多项式数量
 * @param out_gb      输出：Groebner 基（新分配的数组）
 * @param out_count   输出：Groebner 基中多项式的数量
 * @return 0 成功，-1 后端不可用，-2 计算失败
 */
int cas_compute_groebner(SMTSolver *solver,
                          const CASRing *ring,
                          const mpz_poly *polynomials,
                          int poly_count,
                          mpz_poly **out_gb,
                          int *out_count);
```

### 4.3 从约束图到 Singular 脚本的编码

编码管线（新增，与 `smtencode_constraint_graph_to_smtlib2` 平行）：

```c
/**
 * @brief 将约束图编码为 Singular 脚本
 *
 * @param graph        约束图
 * @param ring         目标计算环
 * @param out_script   输出的 Singular 脚本缓冲区
 * @param buffer_size  缓冲区大小
 * @return 实际写入的字符数
 */
int cas_encode_constraint_graph_to_singular(const ConstraintGraph *graph,
                                             const CASRing *ring,
                                             char *out_script,
                                             size_t buffer_size);
```

编码规则：
1. **INCIDENCE 约束** → 叉积多项式等式（已有规则，从 `smt_backend.h` 继承）
2. **BETWEENNESS 约束** → 分段编码为共线条件 + 不等式
3. **INTERSECTION 约束** → 参数化线性系统编码为等式组
4. **距离约束** → 两点距离平方 = d^2 的二次等式
5. **公理包展开的额外约束** → 递归展开为基本约束后编码

---

## 5. 多 CAS 后端的统一抽象

### 5.1 扩展后端类型

```c
/**
 * @brief CAS 后端的统一操作接口
 *
 * 所有 CAS 后端（Singular、Macaulay2、将来的 Maple/Mathematica）遵循此接口。
 */
typedef struct CASBackendOps {
    // 环管理
    int (*ring_declare)(void *backend, const CASRing *ring);
    int (*ring_destroy)(void *backend);

    // 核心计算
    int (*groebner)(void *backend, const mpz_poly *polys, int count,
                    mpz_poly **out, int *out_count);
    int (*ideal_membership)(void *backend, const mpz_poly *polys, int count,
                             const mpz_poly *test, bool *out);
    int (*eliminate)(void *backend, const mpz_poly *polys, int count,
                      const int *vars_to_elim, int elim_count,
                      mpz_poly **out, int *out_count);
    int (*factor)(void *backend, const mpz_poly *poly,
                  mpz_poly **factors, int *factor_count);

    // 生命周期
    int (*start)(void *backend);
    int (*stop)(void *backend);
    bool (*is_alive)(void *backend);
} CASBackendOps;
```

### 5.2 与 SMT 后端的统一调度

`engine_scheduler.h` 中的调度逻辑（`smtsolver_solve` 方法）需要扩展为能够根据问题特征自动选择适当的后端：

1. 如果约束系统是线性的 → `SMT_Z3` 或 `SMT_CVC5`
2. 如果约束系统是二次的且需要 Groebner 基 → 内置 `GROEBNER`
3. 如果约束系统包含高次多项式 → `CAS_SINGULAR` 或 `CAS_M2`
4. 如果涉及因式分解或理想消元 → 优先 `CAS_SINGULAR`

调度规则的核心是 `cas_select_backend()` 函数：

```c
/**
 * @brief 根据问题特征自动选择最优 CAS 后端
 *
 * 分析约束图中多项式的最高次数、变量数量和领域特征，
 * 从已注册的后端中选择最适合的。
 *
 * @param graph  约束图
 * @return 推荐的后端类型
 */
SolverBackendType cas_select_backend(const ConstraintGraph *graph);
```

---

## 6. 实现路径与文件组织

### 6.1 新增文件

| 文件 | 职责 |
|:---|:---|
| `include/lv00/cas_backend.h` | CAS 后端公共接口定义（与 `smt_backend.h` 平行） |
| `src/cas/cas_common.c` | 公共编码函数（约束图 → Singular/M2 脚本） |
| `src/cas/cas_singular.c` | Singular 进程间通信后端 |
| `src/cas/cas_m2.c` | Macaulay2 后端集成 |

### 6.2 与现有模块的集成点

- **`smt_backend.h`**：扩展 `SolverBackendType` 枚举，新增 `CAS_SINGULAR` 和 `CAS_M2`（如果选择统一到现有枚举中）
- **`solver.h`**：求解器在选择 Groebner 基路线时，检查 CAS 后端可用性，若可用则委托
- **`engine.h`**：引擎初始化时注册可用的 CAS 后端

### 6.3 编译时可选

CAS 后端的编译受 `CMakeLists.txt` 中的 `LV00_ENABLE_SINGULAR` 和 `LV00_ENABLE_M2` 选项控制（均为 OFF 默认值）。这遵循与 `smt_backend.h` 中 Z3/cvc5 相同的模式——所有后端创建函数在其对应的编译单元中实现，通过工厂函数注册。

---

## 7. 总结

将 Singular 和 Macaulay2 作为 Lv-00 的代数计算后端，可以在不修改内核求解器源码的前提下，将 Groebner 基计算能力扩展到任意次数的多项式系统。Macaulay2 的"环声明"范式与 Lv-00 的公理包声明形成自然对应，使得前后端之间的语义桥接代价最小。CAS 后端的接口设计与现有 `smt_backend.h` 的工厂模式一致，通过统一的 `SolverBackendType` 枚举和调度器实现无缝路由。

# SCIP 约束整数规划求解器参考文档

> 面向 Lv-00 求解器开发团队的 SCIP 技术调研与借鉴方案

---

## 1. 项目概述

### 1.1 简介

[SCIP](https://github.com/scipopt/scip)（Solving Constraint Integer Programs）是当前速度最快的非商业约束整数规划求解器。它由 Zuse Institute Berlin（ZIB）主导开发，自 2002 年起持续维护至今，已迭代超过 20 年。SCIP 的核心定位是"约束整数规划"（Constraint Integer Programming, CIP）范式，即在统一框架下同时支持三类问题：

| 问题类型 | 缩写 | SCIP 支持程度 |
|----------|------|---------------|
| 混合整数规划 | MIP | 完整支持，含预求解、割平面、分枝定界 |
| 混合整数非线性规划 | MINLP | 通过 CppAD 等外部库提供非线性表达 |
| 约束规划 | CP | 原生支持约束传播引擎，可独立使用或与 MIP 混合 |

SCIP 是目前唯一能将 MIP 的线性松弛 + 分枝切割与 CP 的域缩减 + 约束传播无缝结合的开源求解器。在 MIPLIB 2017 基准测试中，SCIP 的求解能力仅次于商业求解器 Gurobi 和 CPLEX，远超其他开源求解器。

### 1.2 技术栈

| 层次 | 语言 | 说明 |
|------|------|------|
| 核心引擎 | C（ANSI C89/C99） | 约 400,000+ 行，负责约束处理、分支定界、LP 接口、预求解、割平面等所有性能关键路径 |
| 上层接口 | C++ 包装层 | 提供面向对象的 SCIP 类封装（`scip::ObjConshdlr` 等），便于插件开发 |
| Python 绑定 | PySCIPOpt | 社区维护的 Python 接口，依赖 CFFI |
| LP 求解器后端 | 可插拔 | 默认使用 SoPlex（ZIB 自研），支持切换为 CPLEX / Gurobi / Xpress / CLP / QSopt |

### 1.3 许可证

SCIP 采用 **Apache License 2.0**。这意味着：

- 可自由用于商业和闭源项目；
- 可自由修改源码并分发修改版本；
- 需保留版权声明和许可证声明；
- 对专利侵权有明确防御条款。

对于 Lv-00 项目而言，Apache 2.0 允许直接将 SCIP 的 C 源码编译链接到 Lv-00 C 内核中，无需开源 Lv-00 自身代码。

### 1.4 关键统计

| 指标 | 数值 |
|------|------|
| C 源码行数 | 400,000+ |
| 约束处理器插件数 | 10+（线性、二次、SOS1/SOS2、指示约束等） |
| 预求解技术数 | 20+ |
| 分支规则插件数 | 8+（可靠性分支、推理分支、伪费用分支等） |
| 支持的 LP 后端 | 6 种（SoPlex、CPLEX、Gurobi、Xpress、CLP、QSopt） |
| 已发表学术论文 | 100+ 篇（涉及 SCIP 各子系统） |

---

## 2. 核心借鉴点

### 2.1 约束处理器插件架构

**SCIP 做法**

SCIP 的核心扩展机制是**约束处理器**（Constraint Handler）插件架构。每一种约束类型被实现为一个独立的 C 源文件：

```
src/scip/
├── cons_linear.c        # 线性约束 (a·x ≤ b)
├── cons_quadratic.c     # 二次约束 (xᵀQx + a·x ≤ b)
├── cons_sos.c           # 特殊有序集 (SOS1 / SOS2)
├── cons_indicator.c     # 指示约束 (z=1 → a·x ≤ b)
├── cons_knapsack.c      # 背包约束
├── cons_varbound.c      # 变量界约束
├── cons_integral.c      # 整数性约束
├── cons_orbitope.c      # 对称破除
└── ...
```

每个约束处理器通过一个统一的回调函数表注册到 SCIP 核心：

```c
SCIP_RETCODE SCIPincludeConshdlrLinear(SCIP* scip) {
    SCIP_CALL(SCIPincludeConshdlrBasic(scip, &conshdlr, "linear",
        "linear constraints", ...));
    // 注册回调
    SCIP_CALL(SCIPsetConshdlrCheck(scip, conshdlr, checkCallback, ...));
    SCIP_CALL(SCIPsetConshdlrEnfoLP(scip, conshdlr, enfoLPCallback, ...));
    SCIP_CALL(SCIPsetConshdlrEnfoPS(scip, conshdlr, enfoPSCallback, ...));
    SCIP_CALL(SCIPsetConshdlrSepa(scip, conshdlr, sepaCallback, ...));
    // ...
}
```

回调类型及含义：

| 回调名称 | 作用 | 调用时机 |
|----------|------|----------|
| `CONSCHECK` | 检查当前解是否满足约束 | 找到候选解时 |
| `CONSENFOLP` | 在 LP 解处强制执行约束 | 分枝定界 LP 节点 |
| `CONSENFOPS` | 在伪解处强制执行约束 | 伪费用分枝节点 |
| `CONSSEPALP` | 从 LP 松弛中分离割平面 | 割平面生成阶段 |
| `CONSPROP` | 约束传播（域缩减） | CP 传播阶段 |

**Lv-00 对应关系**

Lv-00 求解器的约束图（Constraint Graph）包含多种约束类型：几何共线约束、角度约束、距离比例约束、选择约束等。将每种约束类型实现为类似 SCIP 的约束处理器插件，可以带来以下好处：

- **关注点分离**：几何约束的传播逻辑与选择约束的传播逻辑互不干扰；
- **可热插拔**：新增约束类型无需修改核心引擎代码；
- **策略可组合**：不同约束处理器可在求解流程中按优先级顺序调用。

```c
// Lv-00 中对应的约束处理器注册示例
typedef struct Lv00Conshdlr {
    const char* name;
    Lv00RetCode (*check)(Lv00Solver* s, Lv00Solution* sol, bool* feasible);
    Lv00RetCode (*propagate)(Lv00Solver* s, Lv00Constraint* cons,
                              Lv00DomainReduction* dr, bool* cutoff);
    Lv00RetCode (*separate)(Lv00Solver* s, Lv00Constraint* cons,
                             Lv00CutPool* pool, bool* found);
} Lv00Conshdlr;

// 注册几何共线约束处理器
Lv00RetCode lv00_register_conshdlr_collinear(Lv00Solver* s) {
    Lv00Conshdlr hdlr = {
        .name      = "collinear",
        .check     = lv00_check_collinear,
        .propagate = lv00_propagate_collinear,
        .separate  = lv00_separate_collinear,
    };
    return lv00_solver_add_conshdlr(s, &hdlr);
}
```

### 2.2 MIP + CP 混合范式

**SCIP 做法**

SCIP 是迄今为止唯一能在统一框架中同时运行 MIP（分枝定界 + 线性松弛）和 CP（约束传播 + 域缩减）的开源求解器。其核心机制是在分枝定界树的每个节点上交替执行：

1. **LP 求解**（MIP 路径）：求解当前节点的 LP 松弛，获取下界（对最小化问题）；
2. **约束传播**（CP 路径）：根据当前域对每个约束执行域缩减，可能检测到不可行性或固定变量；
3. **自动化切换**：如果 LP 松弛较弱，SCIP 会更积极地使用约束传播；如果约束传播无法进一步缩减域，则回到 LP 分枝。

```
SCIP 节点处理循环：
┌─────────────────────────────────┐
│  Node: z_LP ≤ f* ≤ z_incumbent  │
├─────────────────────────────────┤
│ 1. Domain Propagation (CP)      │ ← 约束传播缩减变量域
│ 2. Solve LP Relaxation (MIP)    │ ← 线性松弛提供下界
│ 3. Primal Heuristics            │ ← 启发式寻找可行解
│ 4. Conflict Analysis            │ ← 冲突分析，学习 nogoods
│ 5. Cut Separation               │ ← 割平面强化 LP 松弛
│ 6. Branching                    │ ← 选择分支变量
└─────────────────────────────────┘
```

**Lv-00 对应关系**

Lv-00 求解器面临的几何证明问题天然是混合类型的：

- **连续变量**：平面上点的坐标（x, y）、线段长度、角度等几何量；
- **离散变量**："选择哪个交点"、"选择哪条辅助线"、"选择哪种证明策略"等；

这种连续-离散混合结构正适合借鉴 SCIP 的 CIP 范式：在连续变量上使用数值松弛（类似于 LP 松弛），在离散选择上使用约束传播剪枝。

```
Lv-00 中的连续-离散混合：
┌──────────────────┬──────────────────┐
│  连续域 (坐标)    │  离散域 (结构选择) │
├──────────────────┼──────────────────┤
│ 点 P 的 x 坐标   │ 辅助线 L₁ vs L₂  │
│ 线段 AB 的长度   │ 交点方案 A vs B  │
│ ∠ABC 的角度      │ 证明策略 1 vs 2  │
│ → 区间算术传播   │ → 分支定界 / CP  │
└──────────────────┴──────────────────┘
```

### 2.3 分支规则

**SCIP 做法**

SCIP 将分支策略也设计为插件，通过统一的 `SCIPincludeBranchrule` 注册。主要分支规则包括：

| 分支规则 | 缩写 | 策略描述 | 适用场景 |
|----------|------|----------|----------|
| 可靠性分支 | reliability | 伪费用（pseudo-cost）与强分支的混合，当伪费用不可靠时回退到强分支 | 通用 MIP |
| 推理分支 | inference | 基于约束传播的域缩减量选择分支变量 | CP 密集型问题 |
| 伪费用分支 | pscost | 仅使用历史伪费用估算，速度快但不如强分支可靠 | 简单问题 |
| 完全强分支 | fullstrong | 对每个候选变量实际求解 LP 子问题，最可靠但最慢 | 根节点 |
| 最不可行分支 | mostinf | 选择 LP 解中分数性最严重的变量 | 简单快速 |
| 随机分支 | random | 随机选择分支变量和分支方向 | 重启策略 |

每一类分支规则实现的核心回调：

```c
SCIP_RETCODE SCIPsetBranchruleExec(SCIP* scip, SCIP_BRANCHRULE* branchrule,
    SCIP_DECL_BRANCHEXEC((*branchexec)));
```

**Lv-00 对应关系**

Lv-00 的多策略证明引擎需要在不同证明路径中进行分情形（case split）选择。这正对应 SCIP 的分支规则概念：

| SCIP 概念 | Lv-00 对应 |
|-----------|-------------|
| 分支变量选择 | 选择哪个几何构造（如"加辅助线"还是"延长线段"）作为分情形依据 |
| 分支方向 | 对于二元决策（如"点 P 在线段 AB 上" vs "P 在线段 AB 外"），选择优先探索哪个子问题 |
| 节点选择策略 | 在多个未完成的证明分支中选择下一个探索的节点（深度优先 vs 最佳优先） |
| 伪费用 | 统计每种"证明策略"的历史成功率和平均难度，指导后续分支选择 |

```c
// Lv-00 分支规则示例框架
typedef struct Lv00Branchrule {
    const char* name;
    const char* description;
    int         priority;    // 分支规则优先级
    Lv00RetCode (*select_candidates)(Lv00ProofTree* tree,
                                      Lv00BranchCand** cands, int* ncands);
    Lv00RetCode (*choose)(Lv00ProofTree* tree,
                           Lv00BranchCand* cands, int ncands,
                           int* chosen_idx, double* branch_val);
} Lv00Branchrule;
```

### 2.4 预求解

**SCIP 做法**

SCIP 在求解开始前会执行一轮全面的预求解（presolving）。预求解的目标是在不改变问题可行域的前提下，缩小问题规模、收紧变量界、简化约束。SCIP 实现了 20 余种预求解技术，核心类型如下：

| 预求解技术 | 操作 | 效果 |
|-----------|------|------|
| 冗余约束检测 | 若约束被其他约束蕴含，删除之 | 减少约束数 |
| 变量界收紧 | 通过约束传播推导更紧的变量上下界 | 缩小搜索空间 |
| 系数归约 | 将约束中整数系数的共同因子约掉 | 数值更稳定 |
| 平行约束合并 | 若两约束除以常数后等价，合并 | 减少约束数 |
| 固定变量消除 | 若变量界缩为一个值，代入所有约束并消除变量 | 减少变量数 |
| 对偶归约 | 利用对偶信息收紧界 | 理论保证 |
| 聚合变量检测 | 检测 x = a·y + b 形式，替换 x | 减少变量数 |
| 双边约束拆分 | 将 l ≤ a·x ≤ u 拆为 a·x ≥ l 和 a·x ≤ u | 简化约束形式 |

SCIP 预求解的运行示例（日志片段）：

```
presolving:
 (round 1, fast) 42 del vars, 18 del conss, 0 add conss,
   0 chg bounds, 0 chg sides, 0 chg coeffs, 37 upgd conss,
  1448 impls, 0 clqs
 (round 2, exhaustive) 7 del vars, 3 del conss, 21 chg bounds,
   2 chg coeffs, 0 upgd conss, 451 impls, 37 clqs
presolving (3 rounds: 3 fast, 2 medium, 2 exhaustive):
 49 deleted vars, 21 deleted constraints, 21 tightened bounds,
 0 added holes, 2 changed sides, 2 changed coefficients
 1899 implications, 37 cliques
presolved problem has 389 variables (0 bin, 89 int, 0 impl, 300 cont)
   and 267 constraints
```

**Lv-00 对应关系**

Lv-00 的 `constraint_graph` 模块负责管理几何约束的代数表达。在将约束交给求解器之前，可以借鉴 SCIP 的预求解流水线进行规范化与冗余检测：

| SCIP 预求解 | Lv-00 对应 |
|-------------|-------------|
| 冗余约束检测 | 检测 constraint_graph 中被其他几何约束蕴含的冗余条件（如由共线+距离自动决定的角度） |
| 变量界收紧 | 使用区间算术通过已知约束收缩点坐标的可能范围 |
| 系数归约 | 对多项式约束提取公因子，如有理数坐标的分母约分 |
| 平行约束合并 | 合并语义等价的几何条件（如 AB=CD 与 CD=AB 只保留一个） |
| 聚合变量替换 | 检测由约束直接决定的变量（如"X 是 AB 中点"直接给出 X 坐标），替换消除 |

```c
// Lv-00 预求解流水线示例
typedef struct Lv00Presolver {
    const char* name;
    int  priority;       // 预求解执行优先级，低值先执行
    bool exhaustive;     // 是否为穷举型（较慢但更彻底）
    Lv00RetCode (*execute)(Lv00Presolver* p, Lv00ConstraintGraph* g,
                            Lv00PresolveStats* stats);
} Lv00Presolver;

// 注册预求解器列表（按优先级排序）
static Lv00Presolver* lv00_presolvers[] = {
    &lv00_presolver_fix_vars,      // priority=10: 固定变量消除
    &lv00_presolver_redundant,     // priority=20: 冗余约束检测
    &lv00_presolver_bounds,        // priority=30: 区间算术界收紧
    &lv00_presolver_coeff_reduce,  // priority=40: 系数归约
    &lv00_presolver_merge,         // priority=50: 等价约束合并
};
```

### 2.5 分离算法（割平面生成）

**SCIP 做法**

SCIP 在分枝切割（Branch-and-Cut）框架中，每次求解 LP 松弛后会自动调用**分离算法**生成割平面（cutting planes），用于切割掉当前 LP 解中的分数解而不损失任何可行整数解。SCIP 内置的割平面类型：

| 割平面类型 | 生成来源 | 数学基础 |
|-----------|----------|----------|
| Gomory 割 | 从单纯形表最后一行直接导出 | 整数舍入 |
| 覆盖割 (Cover Cuts) | 0-1 背包约束的极小覆盖 | 组合结构 |
| 团割 (Clique Cuts) | 互补变量组成的团约束 | 图论 |
| 流覆盖割 | 单节点流平衡约束 | 网络流 |
| MIR 割 (Mixed Integer Rounding) | 混合整数约束的聚合舍入 | 数值舍入 |
| 提升与投影 (Lift-and-Project) | 通过提升维度再投影 | 组合几何 |
| 零半割 (Zero-Half Cuts) | 系数全为 0 或 1/2 倍数的强有效不等式 | 整数格点 |

分离算法的核心递归逻辑：

```
1.  求解当前节点的 LP 松弛，得到解 x*
2.  FOR EACH 约束处理器 conshdlr:
3.      调用 conshdlr 的 CONS_SEPALP 回调
4.      IF 生成割平面 cuts[]:
5.          将 cuts[] 加入 LP
6.          回到步骤 1（重复直到无法生成新割平面或达到轮数上限）
7.  IF 无新割平面 → 进入分枝阶段
```

**Lv-00 对应关系**

在 Lv-00 求解器中的对应物是 **Grobner 基多项式方程系统的新增方程生成**：当当前的 Grobner 基不足以排除某个候选解时，求解器需要"分离"出新的多项式方程来排除该候选解——这与 SCIP 的割平面分离在功能上是同构的。

| SCIP 分离 | Lv-00 对应 |
|-----------|-------------|
| 割平面 | Grobner 基新增多项式方程 |
| 当前 LP 解 x* | 当前候选解（一组坐标赋值） |
| 分离条件：x* 不满足整数约束 | 分离条件：候选解不满足几何定理 |
| 生成割平面后重新求解 LP | 添加多项式方程后重新计算 Grobner 基 |
| 分离轮数上限 | Grobner 基扩展深度上限 |

```c
// Lv-00 "分离"概念对应示意
// 将 Grobner 基扩展视为约束处理器中的 SEPALP 回调
Lv00RetCode lv00_separate_grobner(Lv00Solver* s,
                                   Lv00Constraint* cons,
                                   Lv00EquationPool* pool) {
    // 1. 从约束图导出多项式理想 I
    Lv00Ideal* I = lv00_constraint_to_ideal(cons);
    // 2. 计算当前 Grobner 基
    Lv00GBasis* G = lv00_grobner_basis(I);
    // 3. 检查候选解是否属于 V(G)
    Lv00Point* cand = lv00_solver_get_candidate(s);
    if (lv00_grobner_membership(G, cand)) {
        return LV00_OK;  // 候选解合法，无需分离
    }
    // 4. 候选解不合法 → 生成"分离方程"
    Lv00Polynomial* new_eq = lv00_grobner_separate(G, cand);
    lv00_equation_pool_add(pool, new_eq);
    return LV00_SEPARATED;
}
```

### 2.6 核心借鉴点对照总表

| 序号 | SCIP 概念 | SCIP 实现位置 | Lv-00 对应模块 | Lv-00 借鉴方式 |
|------|----------|--------------|---------------|---------------|
| 1 | 约束处理器插件 | `src/scip/cons_*.c` | `constraint_graph` 约束类型管理 | 每种约束类型实现为独立插件 |
| 2 | MIP+CP 混合 | `src/scip/solve.c` 主循环 | 求解器核心 | 连续几何量 + 离散选择混合求解 |
| 3 | 分支规则 | `src/scip/branch_*.c` | 多策略证明引擎 | 分情形证明的策略化分支选择 |
| 4 | 预求解流水线 | `src/scip/presolve_*.c` | `constraint_graph` 预处理 | 约束规范化 + 冗余检测流水线 |
| 5 | 分离算法/割平面 | `src/scip/sepa_*.c` | Grobner 基求解器 | "新增方程分离"对应割平面生成 |
| 6 | SCIP C 源码 | 整个 `src/scip/` 目录 | Lv-00 C 内核 | 可编译链接（Apache 2.0） |

---

## 3. Lv-00 映射方案

### 3.1 架构概览

Lv-00 求解器通过新增 `scip_backend` 模块，将 ConstraintGraph 中混合类型的约束（连续几何约束 + 离散选择约束）翻译为 SCIP 模型，利用 SCIP C API 求解。

```
┌───────────────────────────────────────────────────────┐
│                    Lv-00 求解器主流程                    │
├───────────────────────────────────────────────────────┤
│  ConstraintGraph ──→ 约束规范化 ──→ 预求解            │
│        │                                              │
│        ├── 纯几何约束 ──→ Gröbner 基求解器              │
│        │                                              │
│        └── 混合类型约束 ──→ scip_backend ──→ SCIP API  │
│                              │                        │
│                      连续坐标 → 连续变量               │
│                      离散选择 → 整数/二元变量          │
│                      几何约束 → 线性/二次约束           │
└───────────────────────────────────────────────────────┘
```

### 3.2 头文件设计

新增头文件 `include/lv00/scip_backend.h` 定义 SCIP 后端的公共接口。

```c
// 文件: include/lv00/scip_backend.h
// 描述: Lv-00 求解器的 SCIP 后端绑定层
#ifndef LV00_SCIP_BACKEND_H
#define LV00_SCIP_BACKEND_H

#include "lv00/constraint_graph.h"
#include "lv00/solver.h"
#include "scip/scip.h"
#include "scip/scipdefplugins.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------- 数据结构 ---------------- */

/** SCIP 后端配置 */
typedef struct Lv00SCIPConfig {
    double   time_limit;        /**< 求解时间上限（秒），0 表示无限制 */
    double   gap_limit;         /**< 最优间隙上限，0 表示求解到最优 */
    int      max_nodes;         /**< 分枝定界节点数上限，0 表示无限制 */
    int      presolve_rounds;   /**< 预求解轮数，默认 5 */
    int      sep_rounds;        /**< 每节点割平面分离轮数，默认 3 */
    bool     enable_cp;         /**< 是否启用约束传播（CP 模式） */
    bool     verbose;           /**< 是否输出 SCIP 日志 */
    char*    lp_backend;        /**< LP 求解器后端，NULL 表示自动选择 */
} Lv00SCIPConfig;

/** SCIP 后端求解状态 */
typedef struct Lv00SCIPResult {
    SCIP_STATUS   status;        /**< SCIP 求解状态 */
    double        objective;     /**< 最优目标值 */
    double        primal_bound;  /**< 原始界 */
    double        dual_bound;    /**< 对偶界 */
    double        gap;           /**< 最优间隙 */
    int           n_nodes;       /**< 探索的节点数 */
    double        solve_time;    /**< 求解用时（秒） */
    double*       solution;      /**< 解向量，长度为 n_vars */
    int           n_vars;        /**< 解向量维度 */
} Lv00SCIPResult;

/* ---------------- 公共接口 ---------------- */

/**
 * 初始化 SCIP 后端
 * @param config  配置参数，为 NULL 时使用默认配置
 * @return 成功返回 LV00_OK
 */
Lv00RetCode lv00_scip_init(const Lv00SCIPConfig* config);

/**
 * 将 Lv-00 ConstraintGraph 中的混合约束编码为 SCIP 模型
 * @param graph  Lv-00 约束图
 * @return 成功返回 LV00_OK
 */
Lv00RetCode lv00_scip_encode_graph(const Lv00ConstraintGraph* graph);

/**
 * 向 SCIP 模型中添加一个连续几何变量（如点的 x 坐标）
 * @param name  变量名
 * @param lb    下界
 * @param ub    上界
 * @param var_id 输出：分配的变量 ID
 */
Lv00RetCode lv00_scip_add_continuous_var(const char* name,
    double lb, double ub, int* var_id);

/**
 * 向 SCIP 模型中添加一个离散选择变量（如选择哪个交点）
 * @param name     变量名
 * @param n_choices 可选项个数
 * @param var_id   输出：分配的变量 ID
 */
Lv00RetCode lv00_scip_add_discrete_var(const char* name,
    int n_choices, int* var_id);

/**
 * 编码一个线性几何约束 (如 a1*x1 + a2*y1 + ... ≤ b)
 * @param coefs    系数数组
 * @param var_ids  变量 ID 数组
 * @param n_terms  项数
 * @param rhs      右端常数
 * @param sense    约束方向: 'L'(≤), 'G'(≥), 'E'(=)
 */
Lv00RetCode lv00_scip_add_linear_constraint(
    const double* coefs, const int* var_ids,
    int n_terms, double rhs, char sense);

/**
 * 编码一个二次几何约束（如距离平方约束）
 * @param quad_coefs  二次项系数 (n×n 矩阵展平)
 * @param lin_coefs   线性项系数
 * @param var_ids     变量 ID 数组
 * @param n_vars      变量数
 * @param rhs         右端常数
 */
Lv00RetCode lv00_scip_add_quadratic_constraint(
    const double* quad_coefs, const double* lin_coefs,
    const int* var_ids, int n_vars, double rhs);

/**
 * 求解当前 SCIP 模型
 * @param result 输出：求解结果
 */
Lv00RetCode lv00_scip_solve(Lv00SCIPResult* result);

/**
 * 获取变量在最优解中的值
 * @param var_id 变量 ID
 * @param value  输出：变量值
 */
Lv00RetCode lv00_scip_get_var_value(int var_id, double* value);

/**
 * 将 SCIP 解回填到 Lv-00 几何模型（坐标、角度等）
 * @param sol  Lv-00 解对象
 */
Lv00RetCode lv00_scip_backfill_solution(Lv00Solution* sol);

/**
 * 释放 SCIP 后端资源
 */
void lv00_scip_free(void);

#ifdef __cplusplus
}
#endif

#endif /* LV00_SCIP_BACKEND_H */
```

### 3.3 约束翻译示例：共线约束

以下示例展示如何将 Lv-00 中的三点共线约束翻译为 SCIP 模型中的数学表达式。

对于三个点 A(x₁,y₁)、B(x₂,y₂)、C(x₃,y₃)，共线条件等价于叉积为零：

```
(x₂ - x₁)(y₃ - y₁) - (x₃ - x₁)(y₂ - y₁) = 0
```

展开得到二次约束：

```
x₂y₃ - x₂y₁ - x₁y₃ + x₁y₁ - x₃y₂ + x₃y₁ + x₁y₂ - x₁y₁ = 0
→ x₁y₂ - x₂y₁ + x₂y₃ - x₃y₂ + x₃y₁ - x₁y₃ = 0
```

```c
// 三点共线约束的 SCIP 编码示例
Lv00RetCode lv00_scip_encode_collinear(Lv00SCIPBackend* be,
    int x1_id, int y1_id,  // 点 A 坐标变量 ID
    int x2_id, int y2_id,  // 点 B 坐标变量 ID
    int x3_id, int y3_id)  // 点 C 坐标变量 ID
{
    // 二次项: x1*y2, x2*y1, x2*y3, x3*y2, x3*y1, x1*y3
    // x1*y2 系数为 +1, x2*y1 系数为 -1, ... 依此类推
    int    q_vars[6][2] = {{x1_id, y2_id}, {x2_id, y1_id},
                           {x2_id, y3_id}, {x3_id, y2_id},
                           {x3_id, y1_id}, {x1_id, y3_id}};
    double q_coefs[6]  = {+1.0, -1.0, +1.0, -1.0, +1.0, -1.0};

    SCIP* scip = be->scip_ptr;
    // 创建二次约束
    SCIP_CONS* cons = NULL;
    SCIP_CALL(SCIPcreateConsBasicQuadraticNonlinear(scip, &cons,
        "collinear_A_B_C", 0, NULL, NULL, 6,
        q_vars[0], q_vars[1], q_coefs,
        -SCIPinfinity(scip), 0.0,  // lhs = 0, rhs = 0
        TRUE, TRUE, TRUE, TRUE, TRUE));

    SCIP_CALL(SCIPaddCons(scip, cons));
    SCIP_CALL(SCIPreleaseCons(scip, &cons));
    return LV00_OK;
}
```

### 3.4 约束翻译示例：距离比例约束

对于线段 AB 与 CD 的长度比例约束 `|AB| / |CD| = k`，可等价转化为：

```
|AB|² = k² · |CD|²
(x₂ - x₁)² + (y₂ - y₁)² = k² · [(x₄ - x₃)² + (y₄ - y₃)²]
```

```c
// 距离比例约束的 SCIP 编码
Lv00RetCode lv00_scip_encode_distance_ratio(Lv00SCIPBackend* be,
    int x1_id, int y1_id, int x2_id, int y2_id,  // AB 端点
    int x3_id, int y3_id, int x4_id, int y4_id,  // CD 端点
    double k)  // 比例系数 |AB|/|CD| = k
{
    SCIP* scip = be->scip_ptr;

    // 左半: (x2-x1)² + (y2-y1)² - k²*[(x4-x3)² + (y4-y3)²] = 0
    // 展开后为二次型，系数如下:
    //
    // 变量分布:
    // 0:x1, 1:y1, 2:x2, 3:y2, 4:x3, 5:y3, 6:x4, 7:y4
    int    vars[8] = {x1_id, y1_id, x2_id, y2_id, x3_id, y3_id, x4_id, y4_id};
    int    n_quad  = 8;
    int    q_vars[8][2];
    double q_coefs[8];

    // 来自 |AB|² = (x2-x1)² + (y2-y1)²
    int qi = 0;
    // x1²: +1
    q_vars[qi][0] = x1_id; q_vars[qi][1] = x1_id; q_coefs[qi++] = +1.0;
    // y1²: +1
    q_vars[qi][0] = y1_id; q_vars[qi][1] = y1_id; q_coefs[qi++] = +1.0;
    // x2²: +1
    q_vars[qi][0] = x2_id; q_vars[qi][1] = x2_id; q_coefs[qi++] = +1.0;
    // y2²: +1
    q_vars[qi][0] = y2_id; q_vars[qi][1] = y2_id; q_coefs[qi++] = +1.0;
    // -2*x1*x2
    q_vars[qi][0] = x1_id; q_vars[qi][1] = x2_id; q_coefs[qi++] = -2.0;
    // -2*y1*y2
    q_vars[qi][0] = y1_id; q_vars[qi][1] = y2_id; q_coefs[qi++] = -2.0;

    // 来自 -k²*|CD|² = -k²*[(x4-x3)² + (y4-y3)²]
    double ks = k * k;
    // -ks*x3²
    q_vars[qi][0] = x3_id; q_vars[qi][1] = x3_id; q_coefs[qi++] = -ks;
    // -ks*y3²
    q_vars[qi][0] = y3_id; q_vars[qi][1] = y3_id; q_coefs[qi++] = -ks;
    // -ks*x4²
    q_vars[qi][0] = x4_id; q_vars[qi][1] = x4_id; q_coefs[qi++] = -ks;
    // -ks*y4²
    q_vars[qi][0] = y4_id; q_vars[qi][1] = y4_id; q_coefs[qi++] = -ks;
    // +2*ks*x3*x4
    q_vars[qi][0] = x3_id; q_vars[qi][1] = x4_id; q_coefs[qi++] = +2.0 * ks;
    // +2*ks*y3*y4
    q_vars[qi][0] = y3_id; q_vars[qi][1] = y4_id; q_coefs[qi++] = +2.0 * ks;

    SCIP_CONS* cons = NULL;
    SCIP_CALL(SCIPcreateConsBasicQuadraticNonlinear(scip, &cons,
        "dist_ratio_k", 0, NULL, NULL, qi,
        q_vars[0], q_vars[1], q_coefs,
        0.0, 0.0,  // = 0
        TRUE, TRUE, TRUE, TRUE, TRUE));

    SCIP_CALL(SCIPaddCons(scip, cons));
    SCIP_CALL(SCIPreleaseCons(scip, &cons));
    return LV00_OK;
}
```

---

## 4. 实现路线图

### 4.1 分阶段规划

| 阶段 | 名称 | 预计工期 | 产出物 | 依赖 |
|------|------|----------|--------|------|
| Phase 1 | SCIP 集成层 | 2-3 周 | `scip_backend.h` 基本绑定、SCIP 库编译链接 | SCIP 8.0 源码 |
| Phase 2 | 混合约束编码 | 3-4 周 | 几何约束→SCIP 模型翻译器、约束处理器注册 | Phase 1 |
| Phase 3 | 插件化求解策略 | 2-3 周 | 分支规则注册、CP 传播回调、割平面回调 | Phase 2 |

### 4.2 Phase 1: SCIP 集成层

**目标**：在 Lv-00 C 内核中完成 SCIP 库的编译链接，对外开放基础 C API。

**任务清单**：

1. 下载 SCIP 8.0 源码并配置 CMake 构建
2. 将 SCIP 作为 Lv-00 的静态链接依赖（`libscip.a` / `scip.lib`）
3. 实现 `scip_backend.h` 中的初始化/释放函数
4. 实现 `lv00_scip_add_continuous_var` 和 `lv00_scip_add_discrete_var`
5. 实现 `lv00_scip_add_linear_constraint`
6. 实现 `lv00_scip_solve` 和 `lv00_scip_get_var_value`
7. 编写单元测试：创建简单线性规划并求解验证

**关键接口**：

```c
// 内部结构体（不暴露在公共头文件中）
typedef struct Lv00SCIPBackend {
    SCIP*      scip_ptr;         // SCIP 实例指针
    SCIP_VAR** var_pool;         // 变量池
    int        var_pool_size;    // 变量池当前大小
    int        var_pool_cap;     // 变量池容量
    Lv00SCIPConfig config;       // 后端配置
    bool       initialized;      // 是否已初始化
} Lv00SCIPBackend;
```

**验收标准**：
- 能用 SCIP 求解一个包含 10 个变量、5 个约束的线性规划测试用例
- 求解结果（目标值、变量取值）与手动计算一致

### 4.3 Phase 2: 混合约束编码

**目标**：将 Lv-00 ConstraintGraph 中的混合类型约束自动翻译为 SCIP 模型。

**任务清单**：

1. 实现约束图遍历器，识别可编码的约束类型
2. 实现共线约束→SCIP 二次约束的编码
3. 实现距离比例约束→SCIP 二次约束的编码
4. 实现角度相等约束→SCIP 非线性约束的编码
5. 实现离散选择约束（"选择交点 A 还是交点 B"）→SCIP 整数约束的编码
6. 实现 `lv00_scip_encode_graph` 的完整流程
7. 实现 `lv00_scip_backfill_solution` 将 SCIP 解映射回几何元素

**翻译映射表**：

| Lv-00 约束类型 | SCIP 约束类型 | 编码复杂度 |
|---------------|--------------|-----------|
| 三点共线 | 二次约束 (bilinear) | 中等 |
| 距离比例 | 二次约束 (quadratic) | 中等 |
| 角度相等 | 二次约束 + 三角函数变量 | 较高 |
| 点在线上 | 线性约束 | 简单 |
| 线段平行 | 线性约束（方向向量比例） | 简单 |
| 中点关系 | 线性约束 | 简单 |
| 离散交点选择 | SOS1 / 二元变量 | 中等 |

**验收标准**：
- 能自动将包含 5 条共线约束、3 个距离比例约束的几何图形翻译为 SCIP 模型
- SCIP 求解后能正确回填坐标到几何元素

### 4.4 Phase 3: 插件化求解策略

**目标**：利用 SCIP 的插件化接口注册自定义分支规则、约束处理器和回调函数，实现 Lv-00 特有的求解策略。

**任务清单**：

1. 注册分支规则：实现基于几何语义的分支变量选择（如优先分支"关键点"坐标）
2. 注册约束处理器：为 Lv-00 的约束类型注册 CHECK / PROP / SEPALP 回调
3. 注册原始启发式：利用几何直觉构造初始可行解
4. 注册事件处理器：在 SCIP 求解特定阶段记录 Lv-00 需要的日志
5. 调优：根据 Lv-00 典型问题的规模调整分支规则优先级、预求解参数

**插件注册示例**：

```c
// Phase 3 中注册自定义分支规则的伪代码框架
#include "scip/scip.h"
#include "scip/scipdefplugins.h"

// 自定义分支规则：优先分支关键几何点的坐标
static SCIP_DECL_BRANCHEXEC(lv00_branch_geometric) {
    // 1. 获取当前 LP 解中所有点坐标
    // 2. 计算每个点与预期几何位置的距离偏差
    // 3. 选择偏差最大的点坐标作为分支变量
    // 4. 在该变量上执行分支
    SCIP_VAR* branch_var = ...;  // 选择逻辑
    double    branch_val = ...;
    SCIP_CALL(SCIPbranchVarVal(scip, branch_var, branch_val, NULL, NULL, NULL));
    *result = SCIP_BRANCHED;
    return SCIP_OKAY;
}

Lv00RetCode lv00_scip_register_geometric_branchrule(Lv00SCIPBackend* be) {
    SCIP_BRANCHRULE* rule = NULL;
    SCIP_CALL(SCIPincludeBranchruleBasic(be->scip_ptr, &rule,
        "geometric", "geometric point selection branching",
        50000, SCIP_BRANCHRULE_MAXDEPTH, SCIP_BRANCHRULE_MAXBOUNDDIST, NULL));
    SCIP_CALL(SCIPsetBranchruleExec(be->scip_ptr, rule, lv00_branch_geometric));
    return LV00_OK;
}
```

**验收标准**：
- 自定义分支规则能正确处理 Lv-00 几何问题
- 与默认分支规则相比，在典型测试用例上节点数减少 20% 以上

### 4.5 风险与缓解

| 风险 | 可能性 | 影响 | 缓解措施 |
|------|--------|------|----------|
| 二次约束导致 SCIP 求解缓慢 | 中 | 高 | 优先使用线性化近似，仅在必要时使用二次形式 |
| Lv-00 几何问题的数值精度不足 | 中 | 高 | 使用 SCIP 的可行性容差参数调优，必要时使用有理数算术 |
| SCIP API 版本升级导致不兼容 | 低 | 低 | 固定 SCIP 版本号，在 CI 中锁定版本 |
| 大规模几何问题超出 SCIP 能力 | 中 | 中 | 设置节点数和时间上限，超时后回退到纯 Gröbner 基方法 |

---

## 5. 附录

### 5.1 关键资源

| 资源 | 链接 |
|------|------|
| SCIP 官方网站 | https://scipopt.org |
| SCIP GitHub 仓库 | https://github.com/scipopt/scip |
| SCIP 官方文档 | https://scipopt.org/doc/html/ |
| SCIP C API 参考 | https://scipopt.org/doc/html/C_API.php |
| SCIP 论文（Mathematical Programming Computation, 2009） | 搜索 "SCIP: solving constraint integer programs" |
| SoPlex LP 求解器 | https://github.com/scipopt/soplex |
| PySCIPOpt（Python 接口） | https://github.com/scipopt/PySCIPOpt |
| SCIP 教程与示例 | https://scipopt.org/doc/html/EXAMPLES.php |

### 5.2 SCIP 核心 API 速查

```c
/* ---------- 环境与实例 ---------- */
SCIP_RETCODE SCIPcreate(SCIP** scip);
SCIP_RETCODE SCIPfree(SCIP** scip);
SCIP_RETCODE SCIPincludeDefaultPlugins(SCIP* scip);

/* ---------- 变量 ---------- */
SCIP_RETCODE SCIPcreateVarBasic(SCIP* scip, SCIP_VAR** var,
    const char* name, SCIP_Real lb, SCIP_Real ub, SCIP_Real obj,
    SCIP_VARTYPE vartype);
SCIP_RETCODE SCIPaddVar(SCIP* scip, SCIP_VAR* var);
SCIP_Real   SCIPgetVarSol(SCIP* scip, SCIP_VAR* var);

/* ---------- 约束 ---------- */
SCIP_RETCODE SCIPcreateConsBasicLinear(SCIP* scip, SCIP_CONS** cons,
    const char* name, int nvars, SCIP_VAR** vars, SCIP_Real* vals,
    SCIP_Real lhs, SCIP_Real rhs);
SCIP_RETCODE SCIPaddCons(SCIP* scip, SCIP_CONS* cons);
SCIP_RETCODE SCIPreleaseCons(SCIP* scip, SCIP_CONS** cons);

/* ---------- 求解 ---------- */
SCIP_RETCODE SCIPsolve(SCIP* scip);
SCIP_STATUS  SCIPgetStatus(SCIP* scip);
SCIP_Real    SCIPgetPrimalbound(SCIP* scip);
SCIP_Real    SCIPgetDualbound(SCIP* scip);
SCIP_Real    SCIPgetGap(SCIP* scip);
SCIP_Longint SCIPgetNNodes(SCIP* scip);
SCIP_Real    SCIPgetSolvingTime(SCIP* scip);

/* ---------- 参数 ---------- */
SCIP_RETCODE SCIPsetRealParam(SCIP* scip, const char* name, SCIP_Real val);
SCIP_RETCODE SCIPsetIntParam(SCIP* scip, const char* name, int val);
SCIP_RETCODE SCIPsetBoolParam(SCIP* scip, const char* name, SCIP_Bool val);

/* ---------- 输出 ---------- */
SCIP_Real    SCIPinfinity(SCIP* scip);
int          SCIPgetNVars(SCIP* scip);
SCIP_VAR**   SCIPgetVars(SCIP* scip);
```

### 5.3 常用 SCIP 参数调优

| 参数名 | 默认值 | 推荐调优方向 | 说明 |
|--------|--------|-------------|------|
| `limits/time` | 1e+20 | Lv-00 建议设为 60-300 秒 | 全局求解时间上限 |
| `limits/gap` | 0.0 | 几何问题建议 1e-6 | 最优间隙终止条件 |
| `limits/nodes` | -1 | 设为 100000 | 最大节点数限制 |
| `presolving/maxrounds` | -1 | 设为 5-10 | 预求解最大轮数 |
| `separating/maxrounds` | -1 | 设为 3 | 每节点割平面轮数 |
| `branching/random/seed` | 0 | 固定为固定值保证可复现性 | 随机种子 |
| `numerics/feastol` | 1e-6 | 几何问题保持默认 | 可行性容差 |
| `lp/initalgorithm` | `s` | 保持默认（对偶单纯形） | LP 算法选择 |

### 5.4 SCIP 与主流求解器性能对比

| 求解器 | 类型 | 许可证 | MIPLIB 2017 求解能力 | 非线性支持 |
|--------|------|--------|---------------------|-----------|
| Gurobi | 商业 | 专有 | 最强 | 支持 MIQCP |
| CPLEX | 商业 | 专有 | 非常强 | 支持 MIQCP |
| SCIP | 开源 | Apache 2.0 | 开源中最强，接近商业 | 完整 MINLP |
| HiGHS | 开源 | MIT | 快速增长中 | 仅 MIP |
| CBC | 开源 | EPL 2.0 | 中等 | 不支持 |
| GLPK | 开源 | GPL | 较弱 | 不支持 |

> 注：MIPLIB 2017 是混合整数规划领域的标准基准测试集。SCIP 在开源求解器中排名第一，与商业求解器的差距在持续缩小。

### 5.5 术语对照表

| 英文术语 | 中文翻译 | 首次出现章节 |
|----------|----------|-------------|
| Constraint Integer Programming (CIP) | 约束整数规划 | 1.1 |
| Constraint Handler | 约束处理器 | 2.1 |
| Branch-and-Cut | 分枝切割 | 2.5 |
| Branch-and-Bound | 分枝定界 | 2.2 |
| Constraint Propagation | 约束传播 | 2.2 |
| Domain Reduction | 域缩减 | 2.2 |
| Cutting Plane | 割平面 | 2.5 |
| Separation | 分离（算法） | 2.5 |
| Presolving | 预求解 | 2.4 |
| Pseudo-cost | 伪费用 | 2.3 |
| Incumbent | 当前最优解 | 2.4 |
| Primal Heuristic | 原始启发式 | 4.4 |
| Feasibility Tolerance | 可行性容差 | 4.5 |
| Gröbner Basis | Gröbner 基 | 2.5 |
| SOS (Special Ordered Set) | 特殊有序集 | 2.1 |

---

> 文档版本: v1.0 | 最后更新: 2026-05-24 | 作者: Lv-00 开发团队

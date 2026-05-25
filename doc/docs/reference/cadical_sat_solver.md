# Lv-00 参考设计：CaDiCaL CDCL SAT 求解器——冲突驱动子句学习与增量求解

> **版本**: 1.0.0
> **日期**: 2026-05-24
> **参考**: [CaDiCaL](https://github.com/arminbiere/cadical) —— Armin Biere 开发的简化、轻量级 CDCL SAT 求解器
> **目标**: 借鉴 CaDiCaL 的 CDCL 架构、增量求解接口、证明追踪机制和极简内核设计，为 Lv-00 的约束冲突学习、增量几何约束管理、布尔层可验证性和求解器内核精简提供系统化工程方案

---

## 目录

1. [项目概述](#1-项目概述)
2. [核心借鉴要点](#2-核心借鉴要点)
3. [Lv-00 映射方案](#3-lv-00-映射方案)
4. [实现路线图](#4-实现路线图)
5. [附录](#5-附录)

---

## 1. 项目概述

### 1.1 CaDiCaL 是什么

CaDiCaL（**Ca**lculus of **Di**agrams, **Ca**n't, **L**ogical）是由 Armin Biere 开发的简化、轻量级 CDCL（Conflict-Driven Clause Learning）SAT 求解器。Armin Biere 同样是 AIGER（AIG 格式）、Btor2（位向量模型检测格式）和 PicoSAT 的作者，在形式化验证和 SAT 求解领域有极其深厚的影响力。CaDiCaL 的设计哲学与 Biere 的其他项目一脉相承：**用最少的代码实现最强的功能**。

CaDiCaL 的核心特征如下：

1. **极简内核（约 15,000 行 C++）**：相比 Kissat（约 30,000 行）或 MiniSat（约 5,000 行但功能有限），CaDiCaL 在代码量上做到了"刚好够用"的平衡——既不像 MiniSat 那样缺少增量求解和证明追踪，又不像 Kissat 那样代码膨胀至难以理解和修改。这个 15K LOC 的体量使其成为学习 CDCL SAT 求解器的最佳教材，也是嵌入式集成的理想选择。

2. **完整 CDCL 实现**：CaDiCaL 实现了当代 CDCL SAT 求解器的全谱系技术——watched literals（观察文字）加速布尔约束传播（BCP）、VSIDS 决策启发式（变量状态独立衰减和）、子句数据库缩减（clause database reduction）、阶段保存（phase saving）、Luby 重启策略、以及原地子句简化（inprocessing）如子句包含消除和变量消除。

3. **增量求解（Incremental Solving）**：CaDiCaL 支持通过 `assume()` 接口在一系列假设（assumptions）下进行多次求解，无需每次从头重新构建求解器状态。这个功能对于有大量相似问题需要求解的场景至关重要——每个假设字面量被暂时断言为真，如果导致 UNSAT，求解器通过 `failed()` 接口返回冲突的假设子集。

4. **证明追踪（Proof Logging）**：CaDiCaL 内置对 LRAT（Linear Resolution Asymmetric Tautology）和 LKCP（Lean Kernel Calculus Proof）格式的证明输出支持。这使求解器的每一个 UNSAT 结论都可以被外部证明检查器（如 cake_lpr 或 dpr-trim）独立验证。证明追踪是实现可信计算（trusted computing）的关键基础设施。

5. **SAT Competition 优异表现**：CaDiCaL 在 2019 年 SAT Competition 的主赛道（main track）中获得第一名，并在 2020 年和 2021 年持续保持领先。其性能已被社区广泛认可。

```
CaDiCaL 使用示例（增量求解 + 假设）：
#include "cadical.hpp"
CaDiCaL::Solver * solver = new CaDiCaL::Solver();
solver->add(1); solver->add(2); solver->add(0);   // 子句: x1 ∨ x2
solver->add(-1); solver->add(3); solver->add(0);   // 子句: ¬x1 ∨ x3
solver->add(-2); solver->add(-3); solver->add(0);  // 子句: ¬x2 ∨ ¬x3
solver->assume(1);  // 假设 x1 为真
int res = solver->solve();  // res = 10 (SATISFIABLE)
solver->assume(-2); // 假设 x2 为假
res = solver->solve();  // res = 20 (UNSATISFIABLE)
int failed = solver->failed(-2);  // failed = 1, -2 是冲突假设
```

### 1.2 为什么借鉴 CaDiCaL

Lv-00 的约束求解引擎（`solver.h`、`constraint_graph.h`）当前处理几何约束时采用"一次性批量求解"策略——将所有约束方程提取后联立求解。这种模式在以下场景中暴露短板：

- **约束冲突诊断困难**：当用户添加的一组几何约束相互矛盾时（如同时要求 AB=CD 和 AB=2*CD 且 CD≠0），Lv-00 无法精确定位冲突的约束子集，也不能给出"为什么冲突"的解释。

- **增量约束更新开销大**：用户在交互式几何编辑中频繁添加和撤销约束时（如拖拽一个点同时满足多条约束），当前架构需要每次都重新构建整个方程组并重新求解，无法复用前一次求解的计算结果。

- **布尔层可验证性缺失**：Lv-00 的证明系统（`proof.h`）对几何约束的布尔骨架推理缺乏可独立验证的证明输出——外部检查器无法确认求解器没有"作弊"或出现实现错误。

- **求解器内核代码膨胀**：随着约束类型的增加，`solver.h` 和 `constraint_graph.h` 中的求解逻辑不断膨胀，缺少一个清晰的、可独立测试的最小内核。

CaDiCaL 的 CDCL 架构、增量求解接口和证明追踪机制恰好为上述四个问题提供了经过充分验证的解决方案。借鉴 CaDiCaL 意味着将 Lv-00 的约束冲突分析建模为类似 SAT 求解器的子句学习过程，将增量约束管理建模为假设接口，并将每步推理输出为可验证的证明格式。

---

## 2. 核心借鉴要点

### 2.1 CDCL 冲突驱动子句学习

CDCL（Conflict-Driven Clause Learning）是现代 SAT 求解器的核心算法。CaDiCaL 的 CDCL 实现是这一范式的经典工程样本。

CDCL 的工作流程如下：

```
决策（Decision）
  ↓
布尔约束传播（BCP）—— watched literals 加速
  ↓
┌─ 无冲突 → 继续决策
│
└─ 冲突发生
     ↓
   冲突分析（Conflict Analysis）
     ├─ 从冲突子句出发，沿蕴含图逆向追踪
     ├─ 对蕴含子句进行归结（resolution）
     ├─ 找到第一唯一蕴含点（1-UIP, First Unique Implication Point）
     └─ 生成学习子句（learned clause / nogood）
     ↓
   学习子句加入子句库
     ↓
   非时序回跳（Non-Chronological Backtracking）
     └─ 根据学习子句的决策层级回跳到安全决策点
     ↓
   重启策略（Restart）
     └─ Luby 序列或固定间隔决定何时重启搜索
```

对 Lv-00 的映射——几何约束的冲突学习：

| CDCL 概念 | Lv-00 几何约束映射 | 说明 |
|:---|:---|:---|
| 决策文字（Decision Literal） | 几何变量的赋值决策（如"点 C 的 x 坐标为 3"） | `SymbolicCoord` 的实例化选择 |
| 布尔约束传播（BCP） | 几何约束传播——当一个点的坐标确定后，推导相关点的坐标范围 | `constraint_graph_propagate()` 的 watch-point 加速 |
| 蕴含图（Implication Graph） | 几何推导链——记录每一步坐标约简的"原因" | `ConstraintReason` 结构体的链表 |
| 冲突子句（Conflict Clause） | 冲突的几何约束集合——如 `{AB=CD, AB=2*CD, CD≠0}` 不可同时满足 | `constraint_graph_detect_conflict()` 的返回值 |
| 1-UIP 学习子句 | 从冲突约束集中提取最小不可满足核心（MUC, Minimal Unsatisfiable Core） | `constraint_graph_learn_nogood()` 中的归结回溯 |
| 非时序回跳 | 不回溯到紧前决策，而是回溯到 nogood 指示的安全层级 | `constraint_graph_backjump()` 替代简单回溯 |
| 重启 | 定期清空决策栈但保留学习到的 nogood | `solver_restart()` 周期性触发 |

### 2.2 增量求解 + 假设接口

CaDiCaL 的 `assume()` / `solve()` / `failed()` 三元组是实现增量求解的最小接口。其工作原理：

1. `assume(lit)`：将一个文字加入当前求解上下文的假设集合中
2. `solve()`：在当前假设集合下求解；如果 UNSAT，返回 20 并对每个假设文字设置 `failed()` 标记
3. `failed(lit)`：检查某个假设文字是否是导致 UNSAT 的必要条件

这个接口对 Lv-00 的借鉴意义在于——用户在 GUI 中拖拽一个点或添加一条新约束时，本质上是向当前几何系统"假设"了一个新的坐标值或约束条件。Lv-00 可以使用相同的增量模式：

```
用户操作                     CaDiCaL 对应                Lv-00 对应
─────────────────────────────────────────────────────────────────
添加约束 "collinear(A,B,C)"   assume(collinear_literal)   constraint_graph_assume(COL_001)
求解当前系统                  solve()                     constraint_graph_solve()
系统不可满足                  result == 20 (UNSAT)        result == LV00_UNSAT
检查冲突原因                  failed(collinear_literal)   constraint_graph_failed(COL_001)
撤销冲突约束                  （自动回跳）                 constraint_graph_retract(COL_001)
添加替代约束                  assume(alternative_literal) constraint_graph_assume(COL_002)
重新求解                      solve()                     constraint_graph_solve()
```

### 2.3 证明追踪（DRAT/LRAT 格式）

CaDiCaL 支持输出 LRAT（Linear Resolution Asymmetric Tautology）格式的证明。LRAT 是 DRAT（Deletion Resolution Asymmetric Tautology）的增强版，在每个子句推导步骤中附加了用于归结的"线索"（hints），使外部证明检查器可以高效验证。

DRAT/LRAT 证明格式的核心元素：

```
原始子句（Original Clause）:
  1 2 0          # x1 ∨ x2
  -1 3 0         # ¬x1 ∨ x3
  -2 -3 0        # ¬x2 ∨ ¬x3

推导子句（Derived Clause）:
  -1 0            # ¬x1（从上述三个子句归结得出）

LRAT 格式的推导行（附加归结线索）:
  -1 0 0 1 2 3 0  # 推导 ¬x1，归结线索为子句 1,2,3
```

对于 Lv-00，这意味着将几何约束的每一步推理（如"从 collinear(A,B,C) 和已知 A,B 坐标推导 C 在直线 AB 上"）编码为可验证的证明行，使外部检查器可以独立验证整个几何推导的正确性。

### 2.4 极简内核设计理念

CaDiCaL 的代码组织遵循"一个头文件 + 一个实现文件"的极简风格（`cadical.hpp` + `cadical.cpp`），内部以清晰的模块化组织。这种设计哲学的三个关键原则：

| 原则 | CaDiCaL 做法 | Lv-00 借鉴方向 |
|:---|:---|:---|
| **最小接口表面积** | 公开 API 仅 `add`, `assume`, `solve`, `failed`, `val`, `configure` 等约 15 个方法 | Lv-00 求解器对外仅暴露 `lv00_solver_create`, `lv00_solver_add_constraint`, `lv00_solver_solve`, `lv00_solver_assume`, `lv00_solver_failed`, `lv00_solver_val`, `lv00_solver_destroy` 等核心方法 |
| **零外部依赖** | 仅依赖 C++ 标准库，无任何第三方库 | Lv-00 求解器内核仅依赖 C 标准库 + GMP，不引入新的外部依赖 |
| **自包含编译单元** | 可将 `cadical.cpp` 直接嵌入任何 CMake 项目 | Lv-00 的 `solver_core.c` 可被任何 C 项目直接 `#include` 并编译 |
| **内部模块化但不暴露** | 内部有 `Internal` 类封装实现细节，但对外仅一个 `Solver` 类 | Lv-00 内部按功能拆分为 `lv00_cdcl`, `lv00_watch`, `lv00_proof` 等模块，但对外统一为一个 `Lv00Solver` |

### 2.5 Watch-Literal 传播加速

CaDiCaL 使用双观察文字（two-watched literals）方案加速 BCP。其核心数据结构是一个 `watches` 数组：对于每个文字 `lit`，存储所有以 `lit` 为第一个观察文字的子句列表。当 `lit` 被赋值为假时，遍历其观察列表中的每个子句，尝试将观察指针移到该子句中的另一个未赋值文字上。

对 Lv-00 的几何约束传播，watch-point 机制的含义：

| 原始 BCP Watch | Lv-00 几何 Watch | 说明 |
|:---|:---|:---|
| 观察文字 `lit` | 观察几何变量 `SymbolicCoord` 的状态变化 | 当坐标值被约简时触发 |
| 子句的观察列表 | 约束的观察列表——所有受该变量变化影响的约束 | `ConstraintGraph` 邻接表 |
| 观察指针移动 | 约束重新调度——如果约束不再需要该变量，将其从观察列表中移除 | `constraint_graph_reschedule()` |
| 子句满足检测 | 约束满足检测——如果约束已被永久满足，标记为 subsumed | `constraint_is_subsumed()` |

### 2.6 核心借鉴对照表

| 借鉴维度 | CaDiCaL 原实现 | Lv-00 映射目标 | 优先级 | 难度 |
|:---|:---|:---|:---|:---|
| CDCL 冲突学习 | `Internal::analyze()` 冲突分析 + 1-UIP 学习子句生成 | `lv00_cdcl_analyze()` 几何约束冲突分析 + 最小不可满足核心提取 | P2 高 | 中 |
| 增量求解 + 假设 | `Solver::assume()` / `failed()` 三元组 | `lv00_solver_assume()` / `lv00_solver_failed()` | P1 最高 | 低 |
| LRAT 证明追踪 | `Internal::lrat_add_clause()` 证明输出 | `lv00_proof_log_lrat()` 几何推理证明输出 | P2 高 | 中 |
| 极简内核设计 | 15K LOC / 单头文件 + 单实现文件 | `solver_core.h` + `solver_core.c` 约 5000 行 C 代码 | P1 最高 | 低 |
| Watch-Literal BCP | `Internal::watches[]` + `Internal::propagate()` | `lv00_watch_propagate()` 约束传播加速 | P2 高 | 中 |
| 零依赖 C 风格 API | 仅需 C++ 标准库的 C 风格头文件接口 | 仅需 C 标准库 + GMP，无其他外部依赖 | P1 最高 | 低 |
| 子句数据库管理 | `Internal::reduce()` 子句清理 + LBD 评分 | `lv00_nogood_reduce()` nogood 库缩减 | P3 中 | 中 |
| 重启策略 | Luby 序列 + 固定间隔重启 | `lv00_solver_restart()` 几何求解重启 | P3 中 | 低 |
| 阶段保存 | Phase saving 保留上次 SAT 赋值 | 几何变量赋值缓存，加速重求解 | P4 低 | 低 |

---

## 3. Lv-00 映射方案

### 3.1 增量约束求解接口设计

将 CaDiCaL 的增量求解接口映射到 Lv-00 的约束管理系统。以下 C 代码展示核心 API：

```c
/* === solver_core.h — Lv-00 求解器内核的增量约束接口 === */

#ifndef LV00_SOLVER_CORE_H
#define LV00_SOLVER_CORE_H

#include <stdint.h>
#include <stdbool.h>
#include <gmp.h>

/* 求解结果 */
typedef enum {
    LV00_SOLVER_SAT        = 10,  /* 系统可满足 */
    LV00_SOLVER_UNSAT      = 20,  /* 系统不可满足 */
    LV00_SOLVER_UNKNOWN    = 0    /* 求解未完成（超时或中断） */
} Lv00SolverResult;

/* 约束 ID 类型 */
typedef uint32_t Lv00ConstraintId;

/* 求解器句柄（不透明指针，隐藏内部结构） */
typedef struct Lv00Solver Lv00Solver;

/* 生命周期 */
Lv00Solver* lv00_solver_create(void);
void        lv00_solver_destroy(Lv00Solver *s);

/*
 * 添加永久约束——这些约束在多次 solve() 调用之间保持有效。
 * 约束以符号表达式形式传入，内部解析为约束传播器的内部表示。
 *
 * 示例：
 *   lv00_solver_add_constraint(s,
 *       "(= (distance A B) (distance C D))");  // AB = CD
 *   lv00_solver_add_constraint(s,
 *       "(collinear A B C)");                    // A, B, C 共线
 *
 * 返回值：约束 ID（>= 1），用于后续的 failed() 检查
 */
Lv00ConstraintId lv00_solver_add_constraint(Lv00Solver *s,
                                             const char *constraint_expr);

/*
 * 在假设下求解——类似 CaDiCaL 的 assume() + solve()。
 * 假设约束是临时的，仅在本次 solve() 调用中有效。
 * 如果求解结果为 UNSAT，可通过 lv00_solver_failed() 检查哪些假设是冲突的。
 *
 * 返回：LV00_SOLVER_SAT / LV00_SOLVER_UNSAT / LV00_SOLVER_UNKNOWN
 */
Lv00SolverResult lv00_solver_solve_under_assumptions(
    Lv00Solver *s,
    const Lv00ConstraintId *assumptions,
    size_t num_assumptions);

/*
 * 无假设求解——使用所有已添加的永久约束求解。
 */
Lv00SolverResult lv00_solver_solve(Lv00Solver *s);

/*
 * 检查约束 ID 是否为导致最近一次 UNSAT 的冲突约束之一。
 * 类似 CaDiCaL 的 failed() 接口。
 */
bool lv00_solver_failed(Lv00Solver *s, Lv00ConstraintId cid);

/*
 * 撤销一个永久约束（从约束库中移除）。
 */
void lv00_solver_remove_constraint(Lv00Solver *s, Lv00ConstraintId cid);

/*
 * 获取变量的赋值——在 SAT 结果下查询坐标值。
 * 类似 CaDiCaL 的 val() 接口。
 */
bool lv00_solver_get_coord(Lv00Solver *s, const char *coord_name,
                           mpq_t out_value);

/*
 * 获取冲突追踪信息——返回导致最近一次 UNSAT 的约束 ID 列表。
 * 返回的数组由调用者负责释放。
 */
Lv00ConstraintId* lv00_solver_conflict_set(Lv00Solver *s, size_t *out_size);

/*
 * 配置求解器选项。
 * 类似 CaDiCaL 的 configure() 接口。
 */
void lv00_solver_set_option(Lv00Solver *s, const char *name, int value);

#endif /* LV00_SOLVER_CORE_H */
```

### 3.2 CDCL 冲突分析与 Nogood 学习

以下代码展示将 CaDiCaL 的 CDCL 冲突分析映射到 Lv-00 几何约束冲突学习的核心实现：

```c
/* === lv00_cdcl.c — 几何约束的冲突驱动子句学习 === */

#include "lv00_cdcl.h"
#include "constraint_graph.h"
#include <stdlib.h>
#include <string.h>

/* 冲突分析的状态机 */
typedef enum {
    CDCL_STATE_IDLE,            /* 等待传播 */
    CDCL_STATE_PROPAGATING,     /* 正在传播约束 */
    CDCL_STATE_CONFLICT,        /* 检测到冲突，正在分析 */
    CDCL_STATE_LEARNED,         /* 已学习 nogood */
    CDCL_STATE_BACKJUMPING      /* 正在回跳 */
} CDCLState;

/* 推导步骤——记录在蕴含图中的每一步推理 */
typedef struct DerivationStep {
    Lv00ConstraintId constraint_id;   /* 触发推导的约束 */
    size_t decision_level;            /* 推导发生时的决策层级 */
    char *derived_fact;               /* 推导出的事实（如 "(= (x C) 3)") */
    struct DerivationStep **antecedents; /* 推导的先行步骤 */
    size_t num_antecedents;
} DerivationStep;

/* CDCL 引擎状态 */
typedef struct CDCLContext {
    CDCLState state;
    DerivationStep **implication_graph;  /* 蕴含图 */
    size_t ig_size;
    Lv00ConstraintId *learned_nogoods;  /* 已学习的 nogood 约束 ID 列表 */
    size_t num_nogoods;
    size_t current_decision_level;
} CDCLContext;

/*
 * cdcl_analyze_conflict — 从冲突中学习 nogood。
 *
 * 算法映射自 CaDiCaL Internal::analyze()：
 * 1. 从冲突子句出发，沿蕴含图逆向追踪
 * 2. 对传递到冲突的每一步推导进行归结
 * 3. 找到 1-UIP（第一唯一蕴含点）
 * 4. 生成 nogood 子句（不可满足核心）
 */
Lv00ConstraintId cdcl_analyze_conflict(
    CDCLContext *ctx,
    ConstraintGraph *graph,
    Lv00ConstraintId conflict_constraint)
{
    /* 步骤 1: 收集冲突传播链上的所有推导步骤 */
    DerivationStep *conflict_step = cdcl_find_step(ctx, conflict_constraint);
    if (!conflict_step) return 0;

    /* 步骤 2: 从冲突点出发，进行归结 */
    /* 类似于 CaDiCaL 中从冲突子句出发逐文字归结的过程 */
    CDCLResolvedSet resolved;
    cdcl_resolved_set_init(&resolved);

    cdcl_resolve_upto_1uip(ctx, conflict_step, &resolved);

    /* 步骤 3: 提取 1-UIP 处的文字集合作为 nogood */
    /* 例如：如果归结结果为 {collinear(A,B,C), distance(A,B)=5,
     *        distance(A,B)=10}（1-UIP 截断），
     * 则 nogood 为：不能同时满足这三个条件 */
    Lv00ConstraintId nogood_id =
        constraint_graph_create_nogood(graph, &resolved);

    /* 步骤 4: 将 nogood 加入学习库 */
    ctx->learned_nogoods = realloc(ctx->learned_nogoods,
        (ctx->num_nogoods + 1) * sizeof(Lv00ConstraintId));
    ctx->learned_nogoods[ctx->num_nogoods++] = nogood_id;

    cdcl_resolved_set_destroy(&resolved);

    /* 步骤 5: 确定回跳层级（nogood 中除 1-UIP 外最高决策层级） */
    size_t backjump_level = cdcl_compute_backjump_level(ctx, &resolved);

    /* 步骤 6: 执行非时序回跳 */
    ctx->state = CDCL_STATE_BACKJUMPING;
    cdcl_backjump(ctx, graph, backjump_level);

    return nogood_id;
}

/*
 * cdcl_backjump — 非时序回跳到安全决策层级。
 *
 * 不同于简单的逐层回溯，非时序回跳可能跳过多个决策层直接回到
 * nogood 指示的安全点。这避免了在不相关的搜索空间中浪费时间。
 */
void cdcl_backjump(CDCLContext *ctx, ConstraintGraph *graph,
                   size_t target_level)
{
    /* 撤销 target_level 以上的所有决策和传播 */
    while (ctx->current_decision_level > target_level) {
        /* 恢复被当前决策层修改的变量域 */
        constraint_graph_restore_domain_snapshot(
            graph, ctx->current_decision_level);

        /* 从蕴含图中移除当前决策层的所有推导步骤 */
        cdcl_remove_level_steps(ctx, ctx->current_decision_level);

        ctx->current_decision_level--;
    }

    ctx->state = CDCL_STATE_IDLE;
}
```

### 3.3 证明追踪的 LRAT 格式输出

将 CaDiCaL 的 LRAT 证明追踪映射到 Lv-00 几何证明的可验证输出：

```c
/* === lv00_proof_log.c — 几何推理的 LRAT 证明追踪 === */

#include "lv00_proof_log.h"
#include <stdio.h>
#include <stdarg.h>

/* 证明日志句柄 */
typedef struct Lv00ProofLog {
    FILE *output;                       /* 证明输出文件 */
    size_t clause_count;                /* 已输出的子句计数（用于线索引用） */
    Lv00ConstraintId *clause_index;     /* 约束 ID → 子句编号的映射 */
    size_t index_capacity;
} Lv00ProofLog;

/*
 * proof_log_create — 创建证明日志，打开输出文件。
 * 输出格式为 LRAT（Linear Resolution Asymmetric Tautology）。
 */
Lv00ProofLog* proof_log_create(const char *output_path)
{
    Lv00ProofLog *log = calloc(1, sizeof(Lv00ProofLog));
    log->output = fopen(output_path, "w");
    if (!log->output) {
        free(log);
        return NULL;
    }
    log->clause_count = 0;
    log->index_capacity = 1024;
    log->clause_index = calloc(log->index_capacity,
                               sizeof(Lv00ConstraintId));
    return log;
}

/*
 * proof_log_original_constraint — 记录一个原始（用户添加的）约束。
 *
 * 对应 LRAT 格式中的原始子句行。
 * 几何约束如 "collinear(A,B,C)" 被编码为布尔文字形式：
 *   literal = encode_collinear(A, B, C)
 */
void proof_log_original_constraint(Lv00ProofLog *log,
                                   Lv00ConstraintId cid,
                                   const int *literals,
                                   size_t num_literals)
{
    log->clause_count++;

    /* 建立约束 ID 到子句编号的映射 */
    if (cid >= log->index_capacity) {
        log->index_capacity *= 2;
        log->clause_index = realloc(log->clause_index,
            log->index_capacity * sizeof(Lv00ConstraintId));
    }
    log->clause_index[cid] = log->clause_count;

    /* 输出原始子句（无归结线索） */
    fprintf(log->output, "%zu ", log->clause_count);
    for (size_t i = 0; i < num_literals; i++) {
        fprintf(log->output, "%d ", literals[i]);
    }
    fprintf(log->output, "0\n");
}

/*
 * proof_log_derived_nogood — 记录一个推导出的 nogood。
 *
 * 对应 LRAT 格式中的推导子句行，附带了归结线索（hints）。
 * 外部证明检查器利用 clues 列表通过归结验证推导的正确性。
 *
 * 示例输出：
 *   12 3 -5 -7 0  0 1 4 8 0
 *   ^^^^^^^^^^^^^  ^^^^^^^^^
 *   derived nogood  resolution hints (归结线索 = 子句 1,4,8)
 */
void proof_log_derived_nogood(Lv00ProofLog *log,
                              const int *nogood_literals,
                              size_t num_literals,
                              const size_t *hints,       /* 归结线索子句编号 */
                              size_t num_hints)
{
    log->clause_count++;

    /* 输出推导子句 */
    for (size_t i = 0; i < num_literals; i++) {
        fprintf(log->output, "%d ", nogood_literals[i]);
    }
    fprintf(log->output, "0  ");

    /* 输出归结线索 */
    for (size_t i = 0; i < num_hints; i++) {
        fprintf(log->output, "%zu ", hints[i]);
    }
    fprintf(log->output, "0\n");

    fflush(log->output);
}

/*
 * proof_log_geometric_step — 记录一个几何推导步骤。
 *
 * 将几何推理（如"从坐标(x_A, y_A) 和坐标(x_B, y_B) 和 collinear(A,B,C)
 * 推导出 C 在直线 AB 上"）编码为 LRAT 子句。
 */
void proof_log_geometric_step(Lv00ProofLog *log,
                              const char *derived_fact,
                              const Lv00ConstraintId *antecedent_ids,
                              size_t num_antecedents)
{
    /* 步骤 1: 将几何事实编码为文字 */
    int derived_literal = encode_geometric_literal(derived_fact);
    if (derived_literal == 0) return; /* 编码失败的几何事实，跳过 */

    /* 步骤 2: 将先行约束 ID 转为子句编号 */
    size_t *hints = malloc(num_antecedents * sizeof(size_t));
    for (size_t i = 0; i < num_antecedents; i++) {
        hints[i] = log->clause_index[antecedent_ids[i]];
    }

    /* 步骤 3: 构造 nogood: ¬(先行约束都满足 ∧ 推导不成立) */
    int nogood_literals[32]; /* 假设最多 32 个先行 */
    size_t nl = 0;
    for (size_t i = 0; i < num_antecedents; i++) {
        /* 每个先行约束对应的正文字 */
        nogood_literals[nl++] = -encode_constraint(antecedent_ids[i]);
    }
    /* 推导的负文字（表示"推导必须成立"） */
    nogood_literals[nl++] = -derived_literal;

    /* 步骤 4: 输出 LRAT 行 */
    proof_log_derived_nogood(log, nogood_literals, nl,
                             hints, num_antecedents);

    free(hints);
}
```

### 3.4 Watch-Point 约束传播

```c
/* === lv00_watch.c — 基于 Watch-Point 的几何约束传播 === */

#include "lv00_watch.h"
#include "constraint_graph.h"

/*
 * Watch 列表条目——记录一个约束在特定几何变量上的观察注册。
 *
 * 借鉴 CaDiCaL 的 two-watched literals 方案：
 * 每个几何约束在它涉及的两个变量上注册为观察者。
 * 仅当这两个观察变量之一发生变化时，约束传播器才被调用。
 */
typedef struct WatchEntry {
    Lv00ConstraintId constraint_id;   /* 被观察的约束 */
    size_t var_index;                 /* 当前观察的变量在约束中的索引 */
    struct WatchEntry *next;          /* 同一变量上的下一个观察条目 */
} WatchEntry;

typedef struct WatchList {
    WatchEntry *head;
    size_t count;
} WatchList;

/* 全局观察列表——按变量索引 */
static WatchList *global_watches = NULL;
static size_t num_variables = 0;

/*
 * watch_init — 初始化观察列表系统。
 */
void watch_init(size_t num_vars)
{
    num_variables = num_vars;
    global_watches = calloc(num_vars, sizeof(WatchList));
}

/*
 * watch_add_constraint — 为一个约束在两个变量上注册观察。
 *
 * 对 Lv-00 几何约束的映射：
 * - 共线约束 collinear(A,B,C)：在 A 和 B 上注册观察（通常是前两个参数）
 * - 距离等式 (= (distance A B) (distance C D))：在 A 和 C 上注册观察
 * - 角度约束：在角顶点和一条射线上注册观察
 */
void watch_add_constraint(Lv00ConstraintId cid,
                          size_t var_a, size_t var_b)
{
    WatchEntry *entry_a = malloc(sizeof(WatchEntry));
    entry_a->constraint_id = cid;
    entry_a->var_index = 0;  /* 在约束中的第一个观察位置 */
    entry_a->next = global_watches[var_a].head;
    global_watches[var_a].head = entry_a;
    global_watches[var_a].count++;

    WatchEntry *entry_b = malloc(sizeof(WatchEntry));
    entry_b->constraint_id = cid;
    entry_b->var_index = 1;  /* 在约束中的第二个观察位置 */
    entry_b->next = global_watches[var_b].head;
    global_watches[var_b].head = entry_b;
    global_watches[var_b].count++;
}

/*
 * watch_propagate — 当变量 var_index 发生变化时，触发约束传播。
 *
 * 借鉴 CaDiCaL 的 BCP 传播：
 * 1. 遍历变量 var_index 的观察列表
 * 2. 对每个观察条目，尝试将观察指针移到另一个未赋值变量上
 * 3. 如果无法移动（所有变量都已确定或域为空），则约束被触发：
 *    - 若约束被满足 → 标记 subsumed，从观察列表移除
 *    - 若约束产生域约简 → 记录推导步骤，递归触发被约简变量的传播
 *    - 若约束导致域为空 → 报告冲突
 */
Lv00SolverResult watch_propagate(CDCLContext *cdcl,
                                  ConstraintGraph *graph,
                                  size_t var_index)
{
    WatchEntry *entry = global_watches[var_index].head;
    WatchEntry *prev = NULL;

    while (entry) {
        WatchEntry *next = entry->next;
        Lv00ConstraintId cid = entry->constraint_id;

        /* 尝试移动观察指针到另一个未赋值的变量 */
        size_t new_watch_var = constraint_graph_find_unfixed_var(
            graph, cid, var_index);

        if (new_watch_var != SIZE_MAX) {
            /* 成功移动观察指针：从当前变量移除，添加到新变量 */
            /* 从当前链表移除 */
            if (prev) {
                prev->next = next;
            } else {
                global_watches[var_index].head = next;
            }
            /* 添加到新变量的观察列表 */
            entry->next = global_watches[new_watch_var].head;
            global_watches[new_watch_var].head = entry;
        } else {
            /* 无法移动：所有变量都已确定，调用传播器 */
            ConstraintPropResult result =
                constraint_graph_propagate_one(graph, cid, cdcl);

            switch (result.type) {
            case PROP_SUBSUMED:
                /* 约束永久满足，从观察列表移除 */
                watch_remove_entry(var_index, entry);
                break;
            case PROP_DOMAIN_REDUCED:
                /* 产生了域约简，记录推导步骤并递归传播 */
                cdcl_record_derivation(cdcl, result.derived_var,
                                       result.derived_domain, cid);
                watch_propagate(cdcl, graph, result.derived_var);
                break;
            case PROP_CONFLICT:
                /* 域为空 = 冲突 */
                return LV00_SOLVER_UNSAT;
            case PROP_NO_CHANGE:
                /* 无变化 */
                break;
            }
        }

        entry = next;
    }

    return LV00_SOLVER_UNKNOWN; /* 传播完成，无冲突 */
}
```

### 3.5 求解器内核精简——文件级映射

| CaDiCaL 文件 | Lv-00 对应文件 | 功能描述 |
|:---|:---|:---|
| `cadical.hpp` | `solver_core.h` | 公开 API 头文件（约 200 行） |
| `cadical.cpp` / `internal.hpp` | `solver_core.c` | 求解器主循环、CDCL 引擎、接口实现（约 1500 行） |
| `util.hpp` | `lv00_util.h` | 内部工具函数：内存管理、日志、统计 |
| `watch.hpp` | `lv00_watch.h` / `lv00_watch.c` | Watch-point 约束传播模块 |
| `clause.hpp` | `lv00_nogood.h` / `lv00_nogood.c` | Nogood 子句表示和 LBD 评分 |
| `prooftracer.hpp` | `lv00_proof_log.h` / `lv00_proof_log.c` | LRAT 证明追踪 |
| `limit.hpp` | `lv00_limit.h` | 求解限制：步数、内存、时间 |
| `stats.hpp` | `lv00_stats.h` | 求解统计信息 |
| `options.hpp` | `lv00_options.h` | 求解器配置选项 |
| `score.hpp` | `lv00_score.h` | VSIDS 启发式（映射为几何变量选择启发式） |

---

## 4. 实现路线图

### 4.1 第一阶段：增量约束接口与极简内核（P1 最高）

| 任务 | 文件 | 说明 |
|:---|:---|:---|
| 定义 `Lv00Solver` 不透明结构体和核心 API | `solver_core.h` | 实现 `create/destroy/add_constraint/solve/assume/failed` 七个核心方法 |
| 实现约束的基本添加/求解流程 | `solver_core.c` | 走通"添加约束 → 调用 GMP 求解 → 返回 SAT/UNSAT"的最小闭环 |
| 实现 `lv00_solver_assume()` 增量假设机制 | `solver_core.c` | 临时约束的激活与回滚 |
| 实现 `lv00_solver_failed()` 冲突假设检测 | `solver_core.c` | UNSAT 时识别冲突约束子集 |
| 单元测试：增量添加/撤销约束的正确性 | `tests/test_solver_core.c` | 至少 20 个测试用例覆盖边界条件 |

**预估规模**：约 800 行 C 代码

### 4.2 第二阶段：CDCL 冲突学习（P2 高）

| 任务 | 文件 | 说明 |
|:---|:---|:---|
| 实现蕴含图数据结构 `DerivationStep` 和 `CDCLContext` | `lv00_cdcl.h` / `lv00_cdcl.c` | 记录每一步几何推导的因果链 |
| 实现 `cdcl_analyze_conflict()` 冲突分析 | `lv00_cdcl.c` | 1-UIP 归结 + nogood 生成 |
| 实现 `cdcl_backjump()` 非时序回跳 | `lv00_cdcl.c` | 回跳到 nogood 指示的安全层级 |
| 实现 nogood 库管理和 LBD 评分 | `lv00_nogood.c` | 子句质量评估和周期性清理 |
| 实现 Luby 重启策略 | `solver_core.c` | 周期性清空决策栈保留学习子句 |
| 集成测试：多约束冲突场景的 nogood 学习 | `tests/test_cdcl.c` | 验证冲突诊断和回跳正确性 |

**预估规模**：约 1200 行 C 代码

### 4.3 第三阶段：Watch-Point 传播与证明追踪（P2 高）

| 任务 | 文件 | 说明 |
|:---|:---|:---|
| 实现 `watch_init/add_constraint/propagate` | `lv00_watch.c` | 双观察点约束传播引擎 |
| 实现约束 subsumption 检测 | `lv00_watch.c` | 永久满足的约束自动休眠 |
| 实现 `Lv00ProofLog` 和 LRAT 格式输出 | `lv00_proof_log.c` | 原始约束 + 推导 nogood + 归结线索 |
| 实现几何事实到布尔文字的编码/解码 | `lv00_proof_log.c` | 建立几何语义与 SAT 文字的桥梁 |
| 外部证明检查器集成测试 | `tests/test_proof.c` | 使用 dpr-trim 验证输出的 LRAT 证明 |

**预估规模**：约 1000 行 C 代码

### 4.4 第四阶段：性能优化与集成（P3/P4）

| 任务 | 说明 |
|:---|:---|
| VSIDS 启发式映射为几何变量决策 | 根据变量在冲突中出现的频率选择下一个要实例化的坐标 |
| 阶段保存（Phase Saving） | 缓存上次 SAT 赋值，加速相似约束系统的重求解 |
| 与现有 `solver.h` 和 `constraint_graph.h` 的平滑集成 | 新内核作为 `solver.h` 的替代引擎，通过编译选项切换 |
| 完整 benchmark：与当前求解器的性能对比 | 使用 Lv-00 现有测试套件中的几何约束系统作为基准 |

---

## 5. 附录

### 附录 A：CaDiCaL 与 Lv-00 核心概念对照

| CaDiCaL 概念 | Lv-00 对应概念 | 关键差异 |
|:---|:---|:---|
| 命题变量（Boolean Variable） | 几何原子命题（如 "collinear(A,B,C)"、"=(d(A,B),5)"） | Lv-00 的变量有丰富的几何语义 |
| 子句（Clause） | 约束传播器（ConstraintPropagator） | Lv-00 的"子句"可在部分赋值下推导而非仅检测冲突 |
| 文字（Literal） | 几何原子的正/负断言 | 字面值相同，语义域不同 |
| BCP（Boolean Constraint Propagation） | 几何约束传播（Geometric Constraint Propagation） | Lv-00 的传播涉及代数计算而非纯布尔推理 |
| Watched Literals | Watched Variables（观察几何变量） | Lv-00 观察的是连续域变量而非布尔变量 |
| 冲突子句 | 冲突约束集（Conflict Constraint Set） | Lv-00 的冲突可能涉及代数不一致而非纯逻辑矛盾 |
| 决策层级 | 几何变量实例化层级 | 相同概念，不同数据域 |
| 归结（Resolution） | 代数归结（Algebraic Resolution） | Lv-00 需要处理多项式方程间的消元 |
| 证明追踪（LRAT） | 几何证明追踪（Geo-LRAT） | 扩展 LRAT 以支持几何推理的编码 |

### 附录 B：CaDiCaL 内部关键数据结构速查

| 数据结构 | 用途 | Lv-00 等价物 |
|:---|:---|:---|
| `Internal::clause` | 子句表示（文字数组 + LBD + 活度） | `Lv00Nogood`（约束 ID 数组 + LBD + 活度） |
| `Internal::watches[lit]` | 每个文字的观察列表 | `WatchList[var_index]`（每个变量的观察列表） |
| `Internal::vals[idx]` | 变量赋值（-1=假, 0=未赋值, 1=真） | `Lv00VarAssignment[var_index]`（坐标值 + 状态） |
| `Internal::trail` | 赋值追踪栈 | `CDCLContext.decision_stack`（几何决策栈） |
| `Internal::levels[idx]` | 每个变量的决策层级 | `CDCLContext.level[var_index]` |
| `Internal::marks[idx]` | 冲突分析中的标记数组 | `CDCLContext.mark[var_index]` |
| `Internal::lrat` | LRAT 证明追踪器 | `Lv00ProofLog` |

### 附录 C：SAT 求解器选择对照——为何选择 CaDiCaL 而非 Kissat 或 MiniSat

| 维度 | CaDiCaL | Kissat | MiniSat |
|:---|:---|:---|:---|
| 代码量 | ~15K LOC | ~30K LOC | ~5K LOC |
| 增量求解 | 原生支持 assume/failed | 支持 | 不支持（需外部包装） |
| 证明追踪 | 原生 LRAT/LKCP | 原生 LRAT | 需外部工具（drat-trim） |
| 学习速度 | 代码清晰，适合学习 | 代码密集，优化激进 | 代码少但功能有限 |
| 嵌入式集成 | 零依赖，单文件可集成 | 零依赖，体积较大 | 零依赖，体积最小 |
| 维护活跃度 | 活跃（2025 年仍在更新） | 活跃 | 不活跃（已停止维护） |
| **Lv-00 适配度** | **最优——功能完整 + 代码量适中 + 清晰可学** | 过重——功能溢出，代码难以拆分理解 | 过轻——关键功能缺失 |

---

> **文档结束**
> 本文档详述了 CaDiCaL CDCL SAT 求解器在六个核心维度上对 Lv-00 约束求解引擎的借鉴方案。核心结论：通过引入 CDCL 冲突学习机制、增量约束假设接口、LRAT 格式证明追踪和 Watch-Point 传播加速，将 Lv-00 的几何约束求解从"一次性批量求解"升级为"增量可回溯、冲突可诊断、推理可验证"的现代约束求解范式。

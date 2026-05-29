# Lv-00 参考设计：Alt-Ergo SMT 求解器 -- CDCL(T) 理论组合与多态量词推理

> **版本**: 1.0.0
> **日期**: 2026-05-25
> **参考**: [Alt-Ergo](https://github.com/OCamlPro/alt-ergo) -- OCamlPro 与 Inria 联合开发的开源 SMT 自动定理证明器
> **目标**: 借鉴 Alt-Ergo 的 CDCL(T) 理论组合架构、多态排序系统、触发器量词实例化机制和 Shostak 相等推理，为 Lv-00 第 3 层约束求解器提供系统化的理论组合与量词处理工程方案

---

## 目录

1. [项目概述](#1-项目概述)
2. [核心借鉴点](#2-核心借鉴点)
3. [Lv-00 映射方案](#3-lv-00-映射方案)
4. [实现路线图](#4-实现路线图)
5. [附录](#5-附录)

---

## 1. 项目概述

### 1.1 Alt-Ergo 是什么

Alt-Ergo 是由法国 LRI 实验室和 Inria Saclay 联合发起、自 2013 年起由 OCamlPro 公司维护的开源自动定理证明器。其名称 "Alt-Ergo" 源自拉丁语 "因此"（ergo），寓意从前提推导结论的逻辑推理过程。Alt-Ergo 基于 SMT（Satisfiability Modulo Theories，可满足性模理论）技术，专门面向程序验证场景进行了深度优化。

核心特征如下：

1. **多态一阶逻辑输入语言**：原生输入语言是一种 "a la ML" 风格的多态一阶逻辑，天然支持参数多态，无需在编码阶段进行单态化，避免了公式膨胀问题。同时支持 SMT-LIB v2 标准输入格式。

2. **CDCL(T) 混合求解架构**：自 v2.1.0 起，将 CDCL SAT 引擎设为默认布尔推理引擎，采用经典 DPLL(T) / CDCL(T) 架构将 SAT 搜索与理论推理分离。SAT 引擎负责命题骨架搜索，理论求解器负责检查赋值的理论一致性，冲突时通过冲突子句反馈给 SAT 引擎。

3. **丰富的内置理论组合**：支持自由等式理论（含未解释函数）、线性整数/有理数算术、非线性算术片段、多态函数数组（含外延性公理）、枚举数据类型、记录数据类型、结合交换（AC）符号、定长位向量。

4. **CC(X) 理论组合框架**：采用 CC(X)（Congruence Closure modulo solvable theories）实现理论组合，是对 Shostak 方法的改进，通过规范化重写和基 AC 完成算法，高效组合等式理论与可解理论，无需为每个理论对编写专门组合器。

5. **触发器量词实例化机制**：通过触发器（trigger）机制处理带量词公式，用户可指定触发模式，求解器在搜索中遇到匹配项时自动实例化对应量词公式，避免盲目展开。

```
Alt-Ergo 使用示例（多态 + 量词）：
goal g1: forall a:'a. forall x:a. x = x
axiom injective: forall x:int, y:int.
  f(x) = f(y) -> x = y  by trigger f(x) = f(y)
```

### 1.2 为什么借鉴 Alt-Ergo

Lv-00 的约束求解引擎当前面临以下挑战：

- **理论组合能力不足**：`solver.h` 基于 Groebner 基处理代数方程，`solver_core.h` 借鉴 CaDiCaL 实现了 CDCL 布尔推理，但两者之间缺乏有效的组合机制。几何约束同时涉及等式推理（如全等三角形）和算术推理（如距离计算）时，无法像 CC(X) 那样无缝组合。

- **量词处理缺失**：`quantifier.h` 定义了量词数据结构，但缺少高效的量词实例化引擎。几何证明中大量使用全称量词（如"对任意三角形..."），当前系统无法按需实例化。

- **多态排序系统薄弱**：`type_system.h` 含宇宙层级，但约束求解器内部对不同几何对象（点、线、圆等）的排序管理不够精细。

- **程序验证集成度低**：Alt-Ergo 作为 SPARK、Why3、Frama-C 后端，其接口充分考虑了增量求解和证明生成。Lv-00 的 `smt_backend.h` 虽有多后端抽象，但与 Alt-Ergo 的 push/pop 模式尚有差距。

### 1.3 技术栈与社区

| 维度 | 详情 |
|------|------|
| 开发语言 | OCaml（95.7%），辅以 SMT 测试文件 |
| 代码规模 | 约 60,000 行 OCaml 源码 |
| 许可证 | 商业版（OCamlPro Non-Commercial）；开源版（Apache 2.0，延迟约 2 年） |
| 最新版本 | v2.6.3（2026-04-14），开源版 v2.3.3（2022-05-20） |
| GitHub | https://github.com/OCamlPro/alt-ergo ，1,303 次提交，42 个分支 |
| 社区活跃度 | Users' Club 成员包括 AdaCore、CEA List、Thales、MERCE；年度用户会议 |
| 核心维护者 | OCamlPro 团队（Sylvain Conchon 等学术创始人持续合作） |
| 主要应用 | Frama-C（C 验证）、SPARK（Ada 验证）、Why3（多证明器调度）、Atelier-B、EasyCrypt |

---

## 2. 核心借鉴点

### 2.1 CDCL(T) 理论组合架构

Alt-Ergo 采用 CDCL(T) 架构，将布尔搜索与理论推理分离并协同工作：

```
Alt-Ergo CDCL(T) 求解流程：

  输入公式（SMT-LIB 2 / 原生语法）
       |
  预处理层（简化、触发器选择、理论检测）
       |
  SAT 引擎（CDCL）: BCP / VSIDS / 冲突分析 / 重启
       |                    |
  理论求解器调度器      量词实例化引擎
  (CC(X) / LIA / LRA   (Trigger-based
   / NRA / 数组 / AC)    模式匹配/缓存)
       |                    |
  结果输出: SAT(模型) / UNSAT(核心) / UNKNOWN(超时)
```

**关键设计要点**：

- **SAT 与理论求解器接口**：SAT 引擎为每个理论文字注册传播回调。理论求解器可返回三种结果：一致、冲突（返回冲突子句）、传播（返回新推导）。

- **CC(X) 理论组合核心**：将等式推理（同余闭包）与可解理论（线性算术等）组合，通过规范化重写保持项的规范形式。两个项被规范化为相同形式时必然相等，避免了 Nelson-Oppen 方法中需要为每对理论编写专门组合器的问题。

### 2.2 多态排序系统

Alt-Ergo 在内核层面原生支持参数多态，区别于其他 SMT 求解器在 SMT-LIB 层面声明排序后内部单态化的做法：

```
(* Alt-Ergo 多态排序示例 *)
type 'a list
logic cons : 'a, 'a list -> 'a list
axiom list_assoc:
  forall a:'a, x:a, y:a, l:'a list.
    cons(x, cons(y, l)) = cons(cons(x, y), l)
```

**对 Lv-00 的意义**：几何对象天然具有层次化类型结构。点、线、圆等可建模为多态排序，距离、角度等度量可建模为参数化度量函数。

### 2.3 触发器量词实例化

Alt-Ergo 的量词处理采用触发器（trigger）机制，基于 E-matching 的惰性实例化策略：

```
全称量词: forall x:int, y:int. f(x) = f(y) -> x = y
触发模式: f(x) = f(y)

当遇到 f(3) = f(5) 时：
  1. 模式匹配 -> 2. 变量绑定(x->3,y->5) -> 3. 实例化
  4. 推理: f(3)=f(5) -> 3=5 -> 5. 冲突: 3=5 矛盾 -> UNSAT

支持策略: 多触发模式、正向/反向触发、多模式触发防过度实例化
```

### 2.4 Shostak 风格的等式推理

CC(X) 框架将等式理论分为可解理论（存在规范化算法，如线性算术）和不可解理论（需同余闭包，如未解释函数）。核心改进在于规范化重写，使等式推理与理论求解在统一框架下进行。

### 2.5 Alt-Ergo 特性 vs Lv-00 约束求解器对照表

| 特性维度 | Alt-Ergo | Lv-00 现状 | 借鉴价值 |
|----------|----------|-----------|---------|
| **布尔推理引擎** | CDCL SAT（v2.1.0 起默认） | CDCL（`solver_core.h`，借鉴 CaDiCaL） | 已有基础，需增强理论层接口 |
| **理论组合** | CC(X): 等式+线性算术+数组+AC | Groebner 基（`solver.h`）+ SMT 抽象（`smt_backend.h`） | 高 -- CC(X) 可作为架构蓝本 |
| **量词处理** | 触发器机制 + 多模式匹配 | `quantifier.h` 有数据结构，无实例化引擎 | 高 -- 可直接适配几何量词 |
| **多态排序** | 内核级参数多态 | `type_system.h` 含宇宙层级，排序管理薄弱 | 中 -- 增强几何对象类型安全 |
| **增量求解** | push/pop + 假设求解 | `solver_incremental_solve()` 基于脏变量子图 | 中 -- push/pop 补充现有机制 |
| **冲突诊断** | UNSAT 核心 + 冲突子句学习 | `check_conflict_equations()` + CDCL 冲突分析 | 低 -- 已有基础 |
| **证明输出** | 证明对象生成（Coq 检查） | `proof.h` 含验证器，无独立证明格式 | 中 -- 借鉴证明格式设计 |
| **非线性算术** | 非线性片段（放松线性化） | Groebner 基（度数<=2） | 中 -- 互补：精确代数+近似推理 |
| **记录/枚举类型** | 原生支持 | 无对应机制 | 低 -- 可用约束图节点类型替代 |

---

## 3. Lv-00 映射方案

### 3.1 总体架构映射

将 Alt-Ergo 的 CDCL(T) 架构映射到 Lv-00 第 3 层（算法引擎）：

```
Lv-00 第 3 层增强架构（借鉴 Alt-Ergo CDCL(T)）：

  约束图接口层（第 2 层）: ConstraintGraph / AxiomPackage
       |
  理论组合调度器（新增）: Lv00TheoryCombiner
    - 理论注册/注销、共享等式类（CC(X)）、冲突聚合、规范化重写
       |
  ┌─────────┬────────┬────────┬──────────┐
  |等式理论 |线性算术|Groebner| 量词引擎 |
  |(CC 核心)|(LIA/   |基(NRA) |(触发器)  |
  |         | LRA)   |        |          |
  └─────────┴────────┴────────┴──────────┘
       |
  CDCL SAT 引擎（已有）: solver_core.h / Lv00Solver
    - BCP、冲突分析、子句学习、理论文字传播回调
```

### 3.2 理论组合调度器 -- C 代码示例

```c
/**
 * @file theory_combiner.h
 * @brief 理论组合调度器 -- 借鉴 Alt-Ergo CC(X) 框架
 */
#ifndef LV00_THEORY_COMBINER_H
#define LV00_THEORY_COMBINER_H

#include "solver_core.h"
#include "symbolic_coord.h"

typedef struct Lv00TheoryCombiner Lv00TheoryCombiner;
typedef struct Lv00TheorySolver   Lv00TheorySolver;

/** 内置理论类型（借鉴 Alt-Ergo，为几何场景定制） */
typedef enum {
    LV00_THEORY_EQUALITY = 0,  /**< 自由等式理论 */
    LV00_THEORY_LIA,           /**< 线性整数算术 */
    LV00_THEORY_LRA,           /**< 线性有理数算术 */
    LV00_THEORY_NRA,           /**< 非线性实数算术（Groebner 基） */
    LV00_THEORY_ARRAYS,        /**< 数组理论 */
    LV00_THEORY_AC,            /**< 结合交换符号 */
    LV00_THEORY_CUSTOM,        /**< 用户自定义 */
    LV00_THEORY_COUNT
} Lv00TheoryType;

/** 理论文字：表示理论谓词的布尔极性 */
typedef struct Lv00TheoryLit {
    int       predicate_id; /**< 谓词 ID */
    bool      polarity;     /**< 极性 */
    int      *args;         /**< 参数数组 */
    int       arg_count;    /**< 参数数量 */
    Lv00TheoryType theory;  /**< 所属理论 */
} Lv00TheoryLit;

/** 理论求解器虚函数表（借鉴 Alt-Ergo 注册机制） */
typedef struct Lv00TheorySolverVTable {
    int  (*check_consistency)(Lv00TheorySolver *s,
                              const Lv00TheoryLit *lit, bool polarity);
    int  (*propagate)(Lv00TheorySolver *s,
                      Lv00TheoryLit **out, int *out_count);
    int  (*explain_conflict)(Lv00TheorySolver *s,
                             Lv00TheoryLit **out, int *out_count);
    int  (*push)(Lv00TheorySolver *s);
    int  (*pop)(Lv00TheorySolver *s, int levels);
    void (*destroy)(Lv00TheorySolver *s);
} Lv00TheorySolverVTable;

/* 调度器 API */
Lv00TheoryCombiner *lv00_theory_combiner_create(Lv00Solver *cdcl);
void lv00_theory_combiner_destroy(Lv00TheoryCombiner *tc);
int  lv00_theory_combiner_register(Lv00TheoryCombiner *tc,
    Lv00TheoryType theory, Lv00TheorySolver *solver,
    const Lv00TheorySolverVTable *vtable);
int  lv00_theory_combiner_assert(Lv00TheoryCombiner *tc,
    const Lv00TheoryLit *lit);
int  lv00_theory_combiner_check(Lv00TheoryCombiner *tc);
int  lv00_theory_combiner_explain(Lv00TheoryCombiner *tc,
    Lv00TheoryLit **out_lits, int *out_count);

#endif /* LV00_THEORY_COMBINER_H */
```

### 3.3 触发器量词引擎 -- C 代码示例

```c
/**
 * @file trigger_engine.h
 * @brief 触发器量词实例化引擎 -- 借鉴 Alt-Ergo E-matching
 */
#ifndef LV00_TRIGGER_ENGINE_H
#define LV00_TRIGGER_ENGINE_H

#include "quantifier.h"
#include "constraint_graph.h"

typedef struct Lv00TriggerEngine Lv00TriggerEngine;

/** 触发模式：包含量词变量的项集合 */
typedef struct Lv00TriggerPattern {
    int  *term_ids;      /**< 模式中的项 ID */
    int   term_count;    /**< 项数量 */
    int  *var_positions; /**< 量词变量位置 */
    int   var_count;     /**< 量词变量数 */
    bool  is_multi;      /**< 多模式触发器 */
} Lv00TriggerPattern;

/** 量词实例化记录（缓存避免重复） */
typedef struct Lv00Instantiation {
    int  *bindings;       /**< 变量绑定 */
    int   binding_count;  /**< 绑定数量 */
    int   formula_id;     /**< 源公式 ID */
    int   instance_id;    /**< 实例化约束 ID */
} Lv00Instantiation;

Lv00TriggerEngine *lv00_trigger_engine_create(void);
void lv00_trigger_engine_destroy(Lv00TriggerEngine *engine);
int  lv00_trigger_register_quantified(Lv00TriggerEngine *engine,
    const Lv00QuantifiedExpr *qexpr,
    const Lv00TriggerPattern *patterns, int pat_count);
int  lv00_trigger_process_constraint(Lv00TriggerEngine *engine,
    const ConstraintGraph *graph, int constraint_id);
void lv00_trigger_clear_cache(Lv00TriggerEngine *engine);
void lv00_trigger_set_limit(Lv00TriggerEngine *engine, int max_inst);

#endif /* LV00_TRIGGER_ENGINE_H */
```

### 3.4 等式理论求解器 -- C 代码示例

```c
/**
 * @file equality_solver.h
 * @brief 等式理论求解器 -- 借鉴 Alt-Ergo CC(X) 同余闭包
 */
#ifndef LV00_EQUALITY_SOLVER_H
#define LV00_EQUALITY_SOLVER_H

#include "theory_combiner.h"

/** 等式类（Union-Find 结构） */
typedef struct Lv00EqClass {
    int   representative; /**< 代表元素 */
    int   parent;         /**< 父节点 */
    int   rank;           /**< 秩 */
    int  *members;        /**< 成员数组 */
    int   member_count;   /**< 成员数量 */
} Lv00EqClass;

/** 项的规范化表示（Shostak 规范化） */
typedef struct Lv00CanonicalTerm {
    int   term_id;        /**< 原始项 ID */
    int   canon_id;       /**< 规范化项 ID */
    bool  is_interpreted; /**< 已解释项 */
    int  *args;           /**< 子项 */
    int   arg_count;      /**< 子项数量 */
} Lv00CanonicalTerm;

Lv00TheorySolver *lv00_equality_solver_create(void);
int  lv00_equality_assert_eq(Lv00TheorySolver *s, int t1, int t2);
int  lv00_equality_assert_diseq(Lv00TheorySolver *s, int t1, int t2);
bool lv00_equality_are_equal(const Lv00TheorySolver *s, int t1, int t2);
int  lv00_equality_get_canonical(const Lv00TheorySolver *s, int term_id);
const Lv00TheorySolverVTable *lv00_equality_solver_vtable(void);

#endif /* LV00_EQUALITY_SOLVER_H */
```

### 3.5 集成示例：几何约束的多理论组合求解

```c
#include "theory_combiner.h"
#include "equality_solver.h"
#include "trigger_engine.h"
#include "solver_core.h"

void solve_isosceles_triangle(void) {
    /* 证明等腰三角形底边上的高也是中线 */
    Lv00Solver *sat = lv00_solver_create();
    Lv00TheoryCombiner *tc = lv00_theory_combiner_create(sat);

    /* 注册等式理论求解器 */
    Lv00TheorySolver *eq = lv00_equality_solver_create();
    lv00_theory_combiner_register(tc, LV00_THEORY_EQUALITY,
        eq, lv00_equality_solver_vtable());

    /* 创建触发器引擎 */
    Lv00TriggerEngine *triggers = lv00_trigger_engine_create();
    lv00_trigger_set_limit(triggers, 1000);

    /* 编码几何约束:
     * 等式层: AB=AC, angle(AD,BC)=90
     * 算术层: BD+DC=BC, 距离公式线性化
     * 量词层: forall P,Q. dist(P,Q)=dist(Q,P) trigger dist(P,Q) */

    int result = lv00_theory_combiner_check(tc);
    if (result == 1) {
        Lv00TheoryLit *conflict = NULL; int cnt = 0;
        lv00_theory_combiner_explain(tc, &conflict, &cnt);
        free(conflict);
    }
    lv00_trigger_engine_destroy(triggers);
    lv00_theory_combiner_destroy(tc);
    lv00_solver_destroy(sat);
}
```

---

## 4. 实现路线图

### 4.1 分阶段实施计划

| 阶段 | 时间 | 目标 | 交付物 | 优先级 |
|------|------|------|--------|--------|
| **短期** | 2-4 周 | 理论组合框架搭建 | `theory_combiner.h` + 等式求解器骨架 | P0 |
| **短期** | 2-4 周 | CDCL 与理论层接口 | 修改 `solver_core.h` 添加理论传播回调 | P0 |
| **短期** | 2-4 周 | 等式理论求解器 | `equality_solver.h` + Union-Find + 同余闭包 | P0 |
| **中期** | 4-8 周 | 线性算术求解器 | LIA/LRA（Simplex 或 Fourier-Motzkin） | P1 |
| **中期** | 4-8 周 | 触发器量词引擎 | `trigger_engine.h` + 模式匹配 + 缓存 | P1 |
| **中期** | 4-8 周 | Groebner 基集成 | 封装为 NRA 理论求解器注册到组合器 | P1 |
| **中期** | 4-8 周 | 多理论冲突聚合 | 冲突子句聚合与 SAT 反馈 | P1 |
| **长期** | 8-16 周 | 多态排序系统 | 约束求解器内部多态排序管理 | P2 |
| **长期** | 8-16 周 | 证明输出格式 | 借鉴 Alt-Ergo 证明对象，生成可检查证明 | P2 |
| **长期** | 8-16 周 | 性能优化 | 实例化限制策略、理论求解器并行化 | P2 |
| **长期** | 8-16 周 | SMT-LIB 2 兼容 | 扩展 `smt_backend.h` 支持 Alt-Ergo 原生语法 | P2 |

### 4.2 短期阶段（Phase 1）

**核心目标**：建立理论组合基础框架，使 CDCL SAT 引擎能与等式理论求解器协同工作。

1. 实现 `Lv00TheoryCombiner` 核心数据结构（理论注册表、共享等式类）
2. 实现 `Lv00TheorySolverVTable` 虚函数表机制
3. 实现等式理论求解器（Union-Find + 同余闭包 + 规范化重写）
4. 修改 `solver_core.h` CDCL 引擎，添加理论文字传播回调接口
5. 编写单元测试验证等式求解器正确性（合并、查找、同余传播）
6. 编写集成测试验证 CDCL + 等式理论的组合求解

### 4.3 中期阶段（Phase 2）

**核心目标**：扩展理论组合器支持更多理论，引入量词处理能力。

1. 实现线性算术求解器（基于 Simplex 或 Fourier-Motzkin 消元）
2. 将现有 Groebner 基求解器封装为 NRA 理论求解器
3. 实现触发器引擎（模式匹配、实例化生成、缓存管理）
4. 实现理论间冲突子句聚合与反馈机制
5. 端到端测试：多理论组合求解几何约束问题
6. 性能基准测试：对比理论组合前后求解效率

### 4.4 长期阶段（Phase 3）

**核心目标**：完善形式化能力，支持证明生成和多态类型。

1. 设计并实现约束求解器内部的多态排序系统
2. 设计证明输出格式（借鉴 Alt-Ergo 证明对象）
3. 实现实例化限制策略（基于相关度度量的触发器优先级排序）
4. 探索理论求解器并行化可能性
5. 扩展 SMT-LIB 2 编码器，支持 Alt-Ergo 原生语法输出

---

## 5. 附录

### 5.1 Alt-Ergo 关键 API 列表

| 模块 | 功能 | 说明 |
|------|------|------|
| `AltErgo` | 主求解器接口 | `parse_from_string`, `assume`, `push`, `pop`, `query` |
| `Options` | 配置管理 | 超时、内存限制、启发式参数 |
| `Ty` | 类型系统 | 多态排序、类型变量、类型构造子 |
| `Term` | 项表示 | 变量、常量、函数应用、量词 |
| `Literal` | 文字 | 布尔极性 + 谓词应用 |
| `Explanation` | 冲突解释 | 冲突子句、推导链 |
| `Matching` | 模式匹配 | E-matching、触发器匹配 |
| `Instances` | 实例化 | 量词实例化管理 |
| `SatML` | SAT 引擎 | CDCL 核心循环 |
| `Th` | 理论求解器 | 理论注册、一致性检查、传播 |
| `Ac` | AC 符号处理 | 结合交换符号的规范化 |
| `Arrays` | 数组理论 | 选择/存储公理、外延性 |
| `Fpa` | 浮点算术 | IEEE 754 浮点数推理 |
| `Sigma` | 签名管理 | 函数/谓词声明 |

### 5.2 Alt-Ergo 原生输入语法参考

```
(* 类型声明 *)
type point
type line

(* 函数/谓词声明 *)
logic dist : point, point -> real
logic collinear : point, point, point -> prop

(* 公理（带触发器） *)
axiom dist_symmetric:
  forall p1:point, p2:point. dist(p1,p2) = dist(p2,p1)
  by trigger dist(p1,p2)

axiom collinear_dist:
  forall a:point, b:point, c:point.
    collinear(a,b,c) -> dist(a,b) + dist(b,c) = dist(a,c)
  by trigger collinear(a,b,c)

(* 目标公式 *)
goal triangle_inequality:
  forall a:point, b:point, c:point.
    dist(a,b) + dist(b,c) >= dist(a,c)

(* push/pop 增量求解 *)
push
assume dist(a,b) = 3.0
assume dist(b,c) = 4.0
assume dist(a,c) = 5.0
query collinear(a,b,c)   (* 期望: Valid *)
pop
```

### 5.3 参考文献

1. Sylvain Conchon, Evelyne Contejean, Mohamed Iguernelala.
   *Canonized rewriting and ground AC completion modulo Shostak theories: Design and implementation.* LMCS, 2012.

2. Sylvain Conchon, Evelyne Contejean, Johannes Kanig, Stephane Lescuyer.
   *CC(X): Semantical combination of congruence closure with solvable theories.* ENTCS, 2007.

3. Claire Dross, Sylvain Conchon, Johannes Kanig, Andrei Paskevich.
   *Adding Decision Procedures to SMT Solvers Using Axioms with Triggers.* Journal of Automated Reasoning, 2016.

4. Francois Bobot, Sylvain Conchon, Evelyne Contejean, Stephane Lescuyer.
   *Implementing Polymorphism in SMT solvers.* SMT Workshop, 2008.

5. Sylvain Conchon, Mohamed Iguernelala, Kailiang Ji, Guillaume Melquiond, Clement Fumex.
   *A Three-Tier Strategy for Reasoning About Floating-Point Numbers in SMT.* CAV, 2017.

6. Francois Bobot, Sylvain Conchon et al.
   *A Simplex-based extension of Fourier-Motzkin for solving linear integer arithmetic.* IJCAR, 2012.

7. Stephane Lescuyer, Sylvain Conchon.
   *Improving Coq propositional reasoning using a lazy CNF conversion scheme.* FroCoS, 2009.

8. Alt-Ergo 官方文档. https://ocamlpro.github.io/alt-ergo/latest/

9. Alt-Ergo GitHub 仓库. https://github.com/OCamlPro/alt-ergo

10. Alt-Ergo 学术页面. https://usr.lmf.cnrs.fr/ergo/

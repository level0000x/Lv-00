# Lv-00 参考落地设计文档：Vampire/E Prover/iProver FOL ATP 后端集成

> **版本**: 1.0.0
> **日期**: 2026-05-24
> **参考**: Vampire (github.com/vprover/vampire) —— superposition 演算 FOL ATP 冠军；E Prover (github.com/eprover/eprover) —— 高性能等式推理器；iProver (github.com/iprover/iprover) —— Inst-Gen 实例化演算
> **目标**: 将 Vampire 的 superposition 演算 + AVATAR 架构、E Prover 的 SINDE 启发式、iProver 的 Inst-Gen 量化处理、TPTP 编码标准和 TSTP 证明输出映射到 Lv-00 的 atp_backend.h、proof_multi_strategy 和 engine_scheduler.h，作为现有 SMT 后端（Z3/cvc5）的互补 FOL 推理引擎

---

## 目录

1. [项目概述与 Lv-00 借鉴动机](#1-项目概述与-lv-00-借鉴动机)
2. [核心借鉴要点一：Vampire superposition 演算与 AVATAR 架构](#2-核心借鉴要点一vampire-superposition-演算与-avatar-架构)
3. [核心借鉴要点二：E Prover 的 SINDE 启发式与模块化设计](#3-核心借鉴要点二e-prover-的-sinde-启发式与模块化设计)
4. [核心借鉴要点三：iProver 的 Inst-Gen 量化实例化](#4-核心借鉴要点三iprover-的-inst-gen-量化实例化)
5. [核心借鉴要点四：TPTP 编码标准与几何公理的 FOF/CNF 转换](#5-核心借鉴要点四tptp-编码标准与几何公理的-fofcnf-转换)
6. [核心借鉴要点五：TSTP 证明输出到 Lv-00 ProofNavigator 转换](#6-核心借鉴要点五tstp-证明输出到-lv-00-proofnavigator-转换)
7. [Lv-00 映射方案：atp_backend.h 的 FOL ATP 后端](#7-lv-00-映射方案atp_backendh-的-fol-atp-后端)
8. [Lv-00 映射方案：proof_multi_strategy 的 ATP 策略注册](#8-lv-00-映射方案proof_multi_strategy-的-atp-策略注册)
9. [总结映射表](#9-总结映射表)

---

## 1. 项目概述与 Lv-00 借鉴动机

### 1.1 三大 ATP 系统简介

| 系统 | 开发方 | 核心演算 | CASC 竞赛地位 | 核心特点 |
|------|-------|---------|-------------|---------|
| **Vampire** | 曼彻斯特大学 Andrei Voronkov 团队 | Superposition + AVATAR | 多次 CASC FOL 组冠军 | 策略自动调度、AVATAR SAT+saturation 混合 |
| **E Prover** | 慕尼黑工业大学 Stephan Schulz | Superposition + SINDE 启发式 | CASC 常胜选手 | 高度模块化、SINDE 自适应子句评估 |
| **iProver** | 曼彻斯特大学 Konstantin Korovin | Inst-Gen (实例化+生成) | CASC EPR 组冠军 | 量化问题专精、与 superposition 互补 |

三者共同构成 FOL（一阶逻辑）自动定理证明的事实标准。它们的 TPTP（Thousands of Problems for Theorem Provers）编码格式是 ATP 领域的通用语言。

### 1.2 Lv-00 借鉴动机

Lv-00 现有的 `smt_backend.h`（Z3/cvc5）覆盖了**可满足性判定**（SMT）路径，但几何证明中大量的全称量化公理（如"任意三角形的三条中线交于一点"）和等式推理（如"线段 AB = 线段 CD 当且仅当 |AB| = |CD|"）更适合 FOL ATP 的 superposition 演算：

| 问题类型 | SMT (Z3/cvc5) 表现 | FOL ATP (Vampire/E) 表现 | Lv-00 选择策略 |
|---------|-------------------|------------------------|---------------|
| 坐标不等式求解 | 优秀（线性/非线性算术） | 较弱（无原生算术） | → SMT |
| 全称量化几何公理 | 较弱（需量词消除） | 优秀（superposition + Inst-Gen） | → ATP |
| 构造间的等式推理 | 中等 | 优秀（paramodulation/superposition） | → ATP |
| 组合爆炸的几何配置 | 中等 | 优秀（SINDE 自适应启发式） | → ATP |
| 非线性多项式等式 | 优秀（Groebner 基） | 弱 | → SMT |

因此，Lv-00 需要 **ATP 后端作为 SMT 后端的互补**，由 `engine_scheduler.h` 自动路由到合适的引擎。

### 1.3 总体架构对照

```
Vampire / E Prover / iProver           Lv-00
────────────────────────────────────────────────────────
ATP 可执行文件 (vampire/eprover)        atp_backend.h (新增头文件，atp_backend.c)
  ├─ 子进程调用                         ├─ atp_backend_spawn()
  ├─ TPTP 输入文件                      ├─ geometry_to_tptp()
  └─ TSTP 证明输出                     └─ tstp_to_proof_navigator()

TPTP 格式 (FOF/CNF/TFF)                 tptp_encoder.h (新增)
  ├─ fof(name, axiom, formula)         ├─ tptp_encode_axiom()
  ├─ fof(name, conjecture, formula)    ├─ tptp_encode_conjecture()
  └─ cnf(name, axiom, clause)         ├─ tptp_encode_clause()

策略自动调度 (Vampire strategy scheduling)  engine_scheduler.h (扩展)
  ├─ 问题特征分析                       ├─ problem_feature_analysis()
  ├─ 策略选择                           └─ atp_vs_smt_routing()

ProofMultiStrategy                       proof_multi_strategy (扩展)
  ├─ 向量演算策略                        ├─ PROOF_STRATEGY_SUPERPOSITION
  └─ Inst-Gen 策略                      └─ PROOF_STRATEGY_INST_GEN
```

---

## 2. 核心借鉴要点一：Vampire superposition 演算与 AVATAR 架构

### 2.1 Superposition 演算概述

Superposition 演算是 resolution 演算的参数化升级，专门针对等式推理优化。其核心推理规则是：

```
Superposition（叠加规则）:
  给定: l = r ∨ C₁    (正等式)
  给定: s[t] ≠ u ∨ C₂ (含项 t 的不等式)
  如果: l 和 t 可合一 (σ = mgu(l, t))
  推导: s[rσ] ≠ u ∨ C₁σ ∨ C₂σ

Paramodulation（参数调制）:
  给定: l = r ∨ C₁
  给定: s[t] = u ∨ C₂
  推导: s[rσ] = u ∨ C₁σ ∨ C₂σ
```

Superposition 演算在几何证明中的天然适用场景：

| 几何场景 | Superposition 应用 |
|---------|-------------------|
| "AB = CD" → 在涉及 AB 的公式中用 CD 替换 | paramodulation 自动进行等式代入 |
| "中点 M 满足 AM = MB" → 构造定义的双向替换 | superposition 同时处理构造展开和归约 |
| "全等三角形对应边相等" → 多等式联合推理 | superposition + 等式因子化自动处理传递性 |
| "角平分线 → 比例相等" → 等式链推理 | superposition 在等式中自动传播约束 |

### 2.2 AVATAR 架构：SAT + Superposition 混合

AVATAR（A Vampire Architecture for Theorem proving）是 Vampire 最独特的创新——将 SAT 求解器与 superposition 饱和引擎深度整合：

```
AVATAR 架构流程:
  1. 将问题 CNF 子句集分解为"组件"（component）
  2. 为每个组件分配一个 SAT 变量（断言组件被激活）
  3. SAT 求解器管理组件之间的布尔组合（组件析取）
  4. Superposition 引擎在每个激活的组件内进行等式饱和
  5. SAT 和 superposition 交替进行，通过"分裂"（splitting）通信
  6. 当 SAT 发现赋值使所有组件可满足，或 superposition 在某组件内推导出 ⊥（矛盾），则一个分支被关闭
```

**AVATAR 对 Lv-00 的启示**：

| AVATAR 概念 | Lv-00 映射 | 说明 |
|------------|-----------|------|
| 组件（Component） | 公理包的子集（某几何理论的局部上下文） | 将大规模几何公理集分解为可独立处理的模块 |
| SAT 变量 | 公理包激活/停用标志 | 根据证明目标选择性激活公理包 |
| 分裂（Splitting） | 证明分支（case split） | 几何中的情形分析（如"点在线段上 or 不在线段上"） |
| Superposition 饱和 | `rewrite_egraph_saturate()`（egg 风格等式饱和） | 在激活的公理包内进行等式推理 |

### 2.3 Vampire 策略自动调度

Vampire 的一个关键特点是**策略自动调度（Strategy Scheduling）**——不需要用户手动选择推理策略，系统自动分析问题特征并选择最可能成功的策略组合：

```
Vampire 策略调度:
  1. 分析问题特征：
     - 子句数量、符号数量、等式数量
     - 是否有量词、是否有算术
     - 问题类型（EHW/NEQ/SAT/UNS）
  2. 根据特征从策略库中选择：
     - 轻量问题 → 激进简化 + 有限选择
     - 重量问题 → 保守简化 + 完全 superposition
     - 等式密集 → paramodulation 优先
  3. 每个策略有独立的时间片（slice）
  4. 在时间片内未找到证明 → 切换到下一个策略
  5. 第一个成功的策略结果被返回
```

此设计直接对 Lv-00 的 `engine_scheduler.h` 有指导意义——Lv-00 同样需要根据问题特征自动路由到 ATP 或 SMT 后端。

---

## 3. 核心借鉴要点二：E Prover 的 SINDE 启发式与模块化设计

### 3.1 SINDE 自适应子句评估

E Prover 最大的创新之一是 **SINDE（Sine Adaptive Clause Evaluation）**——一种基于语法的自适应子句选择启发式：

```
SINDE 核心思想:
  1. 为每个符号（函数符号/谓词符号）维护一个"相关度权重"
  2. 子句的得分 = 子句中所有符号的相关度权重之和
  3. "好"的子句（得分高）优先被选入饱和循环
  4. 符号权重根据证明进展动态调整：
     - 初始：目标（conjecture）中出现的符号权重高
     - 运行中：产生新推导的符号权重增加
     - 发现矛盾时：推导路径上的符号权重显著增加
```

**SINDE 对 Lv-00 的启示**：在几何约束传播和重写规则选择中，同样存在"哪些几何约束/重写规则与当前证明目标最相关"的选择问题。引入符号权重可以显著减少搜索空间。

```c
/**
 * @brief 几何符号相关度权重 —— 借鉴 E Prover 的 SINDE 启发式
 *
 * 为约束图中的几何实体类型和构造器维护相关度分数，
 * 用于重写规则和约束传播的优先级排序。
 */
typedef struct {
    int symbol_id;                      /**< 符号 ID（GeomType 或 FuncBlock 名称的哈希） */
    char *symbol_name;                  /**< 符号名称 */
    double relevance_weight;            /**< 相关度权重（0.0 ~ 1.0） */
    double boost_factor;                /**< 权重增长因子 */
    int successful_derivations;         /**< 该符号参与的成果推导数 */
} SINDEWeight;

/**
 * @brief SINDE 自适应子句（规则/约束）选择器
 *
 * 借鉴 E Prover 的 clause evaluation。
 * 给定一组候选重写规则或约束传播规则，
 * 按 SINDE 权重排序，优先选择与当前目标最相关的。
 *
 * E Prover 等价: ClauseEvaluationFunction
 */
typedef struct {
    SINDEWeight *symbol_weights;        /**< 所有符号的权重表 */
    int symbol_count;

    /* 配置 */
    double initial_conjecture_weight;   /**< 目标符号的初始权重 */
    double decay_factor;                /**< 未使用符号的衰减因子 */
    double derivation_boost;            /**< 产生新推导时的权重增长 */
} SINDESelector;

/**
 * @brief 评估一条重写规则与当前证明目标的 SINDE 相关度
 *
 * E Prover 等价: eval_clause(clause, sel_state)
 *
 * @return 相关度分数（0.0 ~ 1.0，越高越相关）
 */
double sinde_evaluate_rule(const RewriteRule *rule,
                           const SINDESelector *selector,
                           const RewritePattern *goal_pattern);
```

### 3.2 E Prover 的模块化设计

E Prover 的架构以高度模块化著称，每个组件都可被独立替换：

| E Prover 模块 | 职责 | Lv-00 对应模块 |
|-------------|------|--------------|
| `eprover.c` | 顶层调度 | `engine_scheduler.h` |
| `clausifier.c` | FOF → CNF 转换 | `tptp_encoder.h`（CNF 生成） |
| `che_proofcontrol.c` | 子句选择启发式 | `sinde_evaluate_rule()`（SINDE 权重） |
| `cclauseproc.c` | 子句化简 | `rewrite.h`（等式饱和化简） |
| `cfo_annotate.c` | 公式注释 | `proof_step_annotate()` |
| `cprfdirect.c` | 直接证明输出 | `tstp_to_proof_navigator()` |
| `cprecedence.c` | 项序（term ordering） | `rewrite_rule` 的 `measure` 字段（归约度量） |

E Prover 的模块化哲学直接指导了 Lv-00 的 `atp_backend.h` 设计——每个 ATP 后端的编码、调用、结果解析各自独立，通过统一的 `ATPBackend` 接口与 `engine_scheduler.h` 交互。

---

## 4. 核心借鉴要点三：iProver 的 Inst-Gen 量化实例化

### 4.1 Inst-Gen 演算

Inst-Gen（Instance Generation）是专门处理**全称量化问题**的演算，与 superposition 互补：

```
Inst-Gen 核心思想:
  1. 给定全称量化公式: ∀x. P(x)
  2. 维护一个"grounding substitution"集合：将量词变量替换为具体项
  3. 每次替换产生实例: P(a), P(f(b)), P(g(c, d)), ...
  4. 将实例化的无量子句提交给 SAT 求解器
  5. SAT 求解器判定可满足 → 找到一个模型（即可满足的反例）
  6. SAT 求解器判定不可满足 → 当前实例集已足够证明矛盾
  7. 如果 SAT 返回"可满足"的模型，Inst-Gen 用该模型生成新实例
  8. 循环直到 SAT 返回"不可满足"或实例化上限耗尽
```

### 4.2 几何证明中的量化问题

几何定理通常是全称量化的——"对任意三角形 ABC，三条中线交于一点"。Inst-Gen 在几何证明中的适用场景：

| 几何场景 | 量化形式 | Inst-Gen 处理 |
|---------|---------|-------------|
| "任意三角形 ABC" | ∀A B C: Point. ¬collinear(A,B,C) → ... | 通过给定图中的具体 A,B,C 实例化 |
| "任一线段 AB" | ∀A B: Point. A≠B → ... | 在约束图中找到满足 A≠B 的线段进行实例化 |
| "任意点 P 满足..." | ∀P: Point. P ∈ l → ... | 在约束图中找到直线上所有的点实例化 |
| 辅助构造引入 | ∃M: Point. M = midpoint(A,B) | Skolem 化后由 Inst-Gen 处理 |

```c
/**
 * @brief Inst-Gen 实例化器 —— 借鉴 iProver 的 Inst-Gen 演算
 *
 * 将全称量化的几何公理实例化为具体几何节点的子句。
 *
 * iProver 等价: InstGen calculus
 */
typedef struct {
    /* 量词变量 */
    int *universal_vars;                /**< 全称量词变量 ID 列表 */
    int var_count;

    /* 实例化候选 */
    int *candidate_term_ids;            /**< 约束图中的候选节点 ID */
    int candidate_count;

    /* 实例化限制 */
    int max_instances;                  /**< 最大实例化数量 */
    int instances_generated;            /**< 已生成的实例数 */

    /* SAT 求解器状态 */
    bool sat_solver_used;               /**< 是否使用 SAT 后端 */
    int sat_result;                     /**< SAT 结果：0=UNSAT, 1=SAT, 2=UNKNOWN */
} InstGenState;

/**
 * @brief 执行 Inst-Gen 循环
 *
 * 借鉴 iProver 的 Inst-Gen 主循环。
 * 反复实例化量化公理，直到 SAT 求解器判定不可满足或达到实例化上限。
 *
 * 几何等价: 将"对任意三角形"逐步实例化为约束图中的具体三角形，
 *         直到所有相关实例都满足公理约束。
 *
 * @param[in]     axioms         全称量化的几何公理数组
 * @param[in]     axiom_count    公理数量
 * @param[in]     graph          约束图（提供实例化候选项）
 * @param[in,out] state          Inst-Gen 状态
 * @return 1=找到证明, 0=实例化不足, -1=矛盾
 */
int inst_gen_run(
    const QuantifiedAxiom *axioms,
    int axiom_count,
    const ConstraintGraph *graph,
    InstGenState *state);
```

### 4.3 Inst-Gen vs Superposition 的选择策略

| 特征 | 选择 Inst-Gen | 选择 Superposition |
|------|-------------|-------------------|
| 量化密度 | 大量全称量词（∀） | 少量量词，以等式为主 |
| 实例数量 | 约束图中几何实体数量可控 | 等式推理链长，实例化会爆炸 |
| 等式比例 | 等式比例 < 30% | 等式比例 > 50% |
| 典型问题 | "对任意三角形"类命题 | "线段相等→构造相等"类命题 |
| Lv-00 路由参数 | `atp_backend_routing.feature_quantifier_heavy = true` | `atp_backend_routing.feature_equation_heavy = true` |

---

## 5. 核心借鉴要点四：TPTP 编码标准与几何公理的 FOF/CNF 转换

### 5.1 TPTP 格式概述

TPTP（Thousands of Problems for Theorem Provers）是 FOL ATP 社区的标准问题编码格式。三种主要语言：

| TPTP 语言 | 全称 | 表示能力 | 典型用途 |
|-----------|------|---------|---------|
| **FOF** | First-Order Form | 一阶逻辑公式（带量词和连接词） | 公理声明、猜想声明 |
| **CNF** | Clause Normal Form | 子句范式（文字析取） | Superposition/resolution 输入 |
| **TFF** | Typed First-Order Form | 带类型的一阶逻辑 | 有类型约束的问题（如几何） |

### 5.2 几何公理的 TPTP 编码

将 Lv-00 的几何公理编码为 TPTP FOF 格式：

```
% ============================================================
% TPTP 编码：三角形重心定理
% ============================================================

% 类型声明（TFF）
tff(point_type, type, point: $tType).
tff(segment_type, type, segment: $tType).

% 中点公理
tff(midpoint_def, axiom,
    ![A: point, B: point, M: point]:
      (midpoint(M, A, B) =>
        (collinear(A, M, B) & (distance(A, M) = distance(M, B))))).

% 重心公理
tff(centroid_def, axiom,
    ![A: point, B: point, C: point, G: point]:
      (centroid(G, A, B, C) =>
        ?[D: point, E: point, F: point]:
          (midpoint(D, B, C) & midpoint(E, C, A) & midpoint(F, A, B) &
           collinear(A, G, D) & collinear(B, G, E) & collinear(C, G, F)))).

% 定理猜想
tff(median_concurrency, conjecture,
    ![A: point, B: point, C: point]:
      (~collinear(A, B, C) =>
        ?[G: point]:
          (centroid(G, A, B, C) &
           ?[D: point, E: point, F: point]:
             (midpoint(D, B, C) & midpoint(E, C, A) & midpoint(F, A, B) &
              incident(G, segment(A, D)) &
              incident(G, segment(B, E)) &
              incident(G, segment(C, F)))))).
```

### 5.3 geometry_to_tptp() 编码函数

```c
/**
 * @brief 将 Lv-00 约束图转换为 TPTP 格式
 *
 * 借鉴 TPTP 标准的 FOF/CNF 编码。
 * 遍历约束图中的所有约束、公理和证明目标，
 * 生成标准 TPTP 问题文件，直接输送给 ATP 求解器。
 *
 * TPTP 标准: tptp.org
 *
 * 转换映射:
 *   Constraint → fof(name, axiom, formula)  或 cnf(name, axiom, clause)
 *   Goal       → fof(name, conjecture, formula)
 *   GeomNode   → tff(name, type, typename)   或显式类型谓词
 *   FuncBlock  → fof(name, axiom, constructor_definition)
 *
 * @param[in]  graph       约束图
 * @param[in]  goal_id     证明目标的约束 ID
 * @param[in]  format      输出格式：FOF/CNF/TFF
 * @param[out] tptp_output 生成的 TPTP 字符串（调用者用 lv00_free 释放）
 * @return 生成的子句数量
 */
int geometry_to_tptp(
    const ConstraintGraph *graph,
    int goal_id,
    TPTPFormat format,
    char **tptp_output);

/**
 * @brief TPTP 输出格式
 */
typedef enum {
    TPTP_FORMAT_FOF,        /**< First-Order Form */
    TPTP_FORMAT_CNF,        /**< Clause Normal Form */
    TPTP_FORMAT_TFF,        /**< Typed First-Order Form (TF0) */
    TPTP_FORMAT_TFF1        /**< Typed First-Order Form with arithmetic */
} TPTPFormat;
```

### 5.4 几何约束到 TPTP 谓词的映射表

| Lv-00 ConstraintType | TPTP 谓词 | 说明 |
|----------------------|----------|------|
| `INCIDENCE(P, L)` | `incident(P, L)` | 点在线上 |
| `COLLINEAR(A, B, C)` | `collinear(A, B, C)` | 三点共线 |
| `BETWEEN(B, A, C)` | `between(B, A, C)` | B 在 A 和 C 之间 |
| `INTERSECTION(P, O1, O2)` | `intersection(P, O1, O2)` | 对象 O1 和 O2 相交于 P |
| `CONTAINMENT(P, R)` | `containment(P, R)` | 点在区域 R 内 |
| `MIDPOINT(M, A, B)` | `midpoint(M, A, B)` | M 是 AB 的中点 |
| `IS_ON_SEGMENT(P, S)` | `is_on_segment(P, S)` | P 在线段 S 上 |
| `LINE_EQUAL(L1, L2)` | `(L1 = L2)` | 线段相等（直接用等式） |
| `segment(A, B)` 构造 | `segment(A, B)` | 线段构造器（函数符号） |
| `midpoint(A, B)` 构造 | `midpoint(A, B)` | 中点构造器 |
| `distance(A, B)` | `distance(A, B)` | 距离函数（数值域） |

---

## 6. 核心借鉴要点五：TSTP 证明输出到 Lv-00 ProofNavigator 转换

### 6.1 TSTP 格式

TSTP（Thousands of Solutions from Theorem Provers）是 TPTP 生态的证明输出标准格式。一个典型的 TSTP 证明：

```
% ============================================================
% TSTP 证明输出（Vampire 格式）
% ============================================================

%---- 推导步骤
cnf(c_0, axiom, (midpoint(D, B, C))).
cnf(c_1, axiom, (midpoint(E, C, A))).
cnf(c_2, axiom, (~collinear(A, B, C))).
% ...
cnf(c_15, plain,
    (incident(G, segment(A, D)) | ~centroid(G, A, B, C)),
    inference(resolution, [status(thm)], [c_10, c_14])).
cnf(c_16, plain,
    (incident(G, segment(B, E)) | ~centroid(G, A, B, C)),
    inference(resolution, [status(thm)], [c_11, c_14])).
cnf(c_17, plain,
    (incident(G, segment(C, F)) | ~centroid(G, A, B, C)),
    inference(resolution, [status(thm)], [c_12, c_14])).
cnf(c_18, plain,
    ($false),
    inference(resolution, [status(thm)], [c_4, c_17])).
```

### 6.2 TSTP → ProofNavigator 转换

将 ATP 输出的 TSTP 证明转换回 Lv-00 的 `ProofNavigator` 步骤：

```c
/**
 * @brief 将 TSTP 证明转换为 Lv-00 ProofNavigator 步骤
 *
 * 解析 ATP 求解器输出的 TSTP 格式证明，
 * 将每个推导步骤映射为 Lv-00 的 ProofStep。
 *
 * 转换流程:
 *   1. 解析 TSTP 文件，提取所有 cnf(...) 推导步骤
 *   2. 为每个步骤创建 ProofStep：
 *      - cnf name → step description
 *      - inference rule → 推理规则标签
 *      - parent references → 步骤依赖关系
 *   3. 将步骤链插入 ProofNavigator
 *   4. 标记步骤的信任颜色（ATP 证明 = GREEN or BLUE）
 *
 * TPTP 推理规则 → Lv-00 步骤类型映射:
 *   resolution   → PROOF_STEP_RESOLUTION
 *   superposition→ PROOF_STEP_SUPERPOSITION
 *   paramodulation→ PROOF_STEP_EQUALITY_REWRITE
 *   factoring    → PROOF_STEP_FACTORING
 *   flattening   → PROOF_STEP_SIMPLIFY
 *
 * @param[in]     tstp_proof   TSTP 格式的证明文本
 * @param[in]     graph        原始约束图（用于恢复几何节点引用）
 * @param[in,out] nav          证明导航器（步骤将被添加到此）
 * @return 成功转换的步骤数
 */
int tstp_to_proof_navigator(
    const char *tstp_proof,
    const ConstraintGraph *graph,
    ProofNavigator *nav);

/**
 * @brief TPTP 推理规则到 Lv-00 证明步骤类型的映射
 */
typedef enum {
    PROOF_STEP_RESOLUTION,          /**< 归结推理 */
    PROOF_STEP_SUPERPOSITION,       /**< 叠加推理 */
    PROOF_STEP_EQUALITY_REWRITE,    /**< 等式重写 */
    PROOF_STEP_FACTORING,           /**< 因子化 */
    PROOF_STEP_INSTANTIATION,       /**< 实例化 */
    PROOF_STEP_SIMPLIFY,            /**< 简化 */
    PROOF_STEP_SPLITTING,           /**< 分裂（AVATAR） */
} TSTPInferenceType;
```

---

## 7. Lv-00 映射方案：atp_backend.h 的 FOL ATP 后端

### 7.1 atp_backend.h 核心设计

`atp_backend.h` 是 Lv-00 中（已创建）的新头文件，封装了对 Vampire、E Prover、iProver 三个外部 ATP 求解器的调用。其设计遵循 E Prover 的模块化哲学——每个 ATP 后端独立实现，通过统一接口与引擎调度器交互。

```c
/**
 * @file atp_backend.h
 * @brief FOL ATP 后端接口 —— Vampire / E Prover / iProver 的子进程调用封装
 *
 * 借鉴 TPTP 生态的标准交互模式：
 *   1. 将几何约束图编码为 TPTP 问题文件
 *   2. 启动 ATP 求解器子进程
 *   3. 等待求解结果或超时
 *   4. 解析 TSTP 证明输出并转换为 ProofNavigator 步骤
 */

/**
 * @brief 支持的 ATP 求解器
 */
typedef enum {
    ATP_SOLVER_VAMPIRE,     /**< Vampire (superposition + AVATAR) */
    ATP_SOLVER_EPROVER,     /**< E Prover (superposition + SINDE) */
    ATP_SOLVER_IPROVER,     /**< iProver (Inst-Gen) */
    ATP_SOLVER_COUNT
} ATPSolver;

/**
 * @brief ATP 后端会话
 *
 * 封装一次 ATP 求解调用的完整生命周期：
 *   输入准备 → 子进程管理 → 结果解析 → 证明转换
 */
typedef struct {
    ATPSolver solver;                   /**< 求解器类型 */

    /* 子进程管理 */
    int pid;                            /**< 子进程 PID */
    char *stdin_pipe;                   /**< 标准输入管道路径 */
    char *stdout_pipe;                  /**< 标准输出管道路径 */
    char *stderr_pipe;                  /**< 标准错误管道路径 */

    /* TPTP 编码 */
    char *tptp_problem;                 /**< TPTP 问题文本 */
    char *tptp_problem_file;            /**< TPTP 临时文件路径 */

    /* 求解参数 */
    int time_limit_seconds;             /**< 时间限制（秒） */
    int memory_limit_mb;                /**< 内存限制（MB） */
    char **extra_args;                  /**< 额外命令行参数 */
    int extra_arg_count;

    /* 求解模式 */
    int strategy_mode;                  /**< 策略模式（auto/specific） */

    /* 结果 */
    int exit_code;                      /**< 子进程退出码 */
    char *raw_output;                   /**< 原始输出 */
    char *tstp_proof;                   /**< TSTP 证明文本（如果成功） */
    ATPResult result;                   /**< 求解结果 */
} ATPBackend;

/**
 * @brief ATP 求解结果
 */
typedef enum {
    ATP_RESULT_THEOREM,         /**< 定理得证 (Theorem) */
    ATP_RESULT_COUNTERSATISFIABLE, /**< 存在反例 (CounterSatisfiable) */
    ATP_RESULT_UNSATISFIABLE,   /**< 公理矛盾 (Unsatisfiable) */
    ATP_RESULT_TIMEOUT,         /**< 超时 (Timeout) */
    ATP_RESULT_MEMORY_OUT,      /**< 内存耗尽 (MemoryOut) */
    ATP_RESULT_GAVEUP,          /**< 求解器放弃 (GaveUp) */
    ATP_RESULT_ERROR,           /**< 错误 (Error) */
} ATPResult;
```

### 7.2 核心 API

```c
/**
 * @brief 创建并配置 ATP 后端会话
 *
 * @param solver        求解器类型（VAMPIRE/EPROVER/IPROVER）
 * @param time_limit    时间限制（秒）
 * @param memory_limit  内存限制（MB）
 * @return 初始化的 ATP 后端会话
 */
ATPBackend *atp_backend_create(ATPSolver solver, int time_limit, int memory_limit);

/**
 * @brief 启动 ATP 求解
 *
 * 1. 将 ConstraintGraph + Goal 编码为 TPTP 问题
 * 2. 写入临时文件
 * 3. 启动 ATP 子进程
 * 4. 等待终止（或超时 kill）
 *
 * @param[in,out] backend     ATP 后端会话
 * @param[in]     graph        约束图
 * @param[in]     goal_id      证明目标约束 ID
 * @param[in]     format       TPTP 编码格式
 * @return ATP 求解结果
 */
ATPResult atp_backend_solve(
    ATPBackend *backend,
    const ConstraintGraph *graph,
    int goal_id,
    TPTPFormat format);

/**
 * @brief 将 ATP 证明转换为 ProofNavigator 步骤
 *
 * 解析 atp_backend_solve() 产生的 TSTP 证明，
 * 转换为 Lv-00 的 ProofNavigator 可以使用的步骤序列。
 *
 * @param[in]     backend   ATP 后端（包含求解结果和 TSTP 证明）
 * @param[in]     graph     原始约束图
 * @param[in,out] nav       证明导航器
 * @return 成功转换的步骤数
 */
int atp_backend_convert_proof(
    const ATPBackend *backend,
    const ConstraintGraph *graph,
    ProofNavigator *nav);

/**
 * @brief 销毁 ATP 后端会话并释放资源
 */
void atp_backend_destroy(ATPBackend *backend);
```

### 7.3 问题特征分析与自动路由

```c
/**
 * @brief 问题特征 —— 用于 ATP vs SMT 自动路由
 */
typedef struct {
    int total_constraints;              /**< 总约束数 */
    int equality_constraints;           /**< 等式约束数 */
    int quantifier_heavy;               /**< 量化密集度（>= 阈值则趋向 ATP） */
    int arithmetic_heavy;               /**< 算术密集度（>= 阈值则趋向 SMT） */
    int nonlinear_degree;               /**< 最大非线性次数 */
    int node_count;                     /**< 几何节点数 */
    double equality_ratio;              /**< 等式比例（等式约束/总约束） */
    double quantifier_ratio;            /**< 量化约束比例 */

    /* 推荐路由 */
    bool recommend_atp;                 /**< 推荐 ATP 后端 */
    bool recommend_smt;                 /**< 推荐 SMT 后端 */
    ATPSolver recommended_atp_solver;   /**< 推荐的 ATP 求解器类型 */
} ProblemFeatures;

/**
 * @brief 分析问题特征并推荐后端路由
 *
 * 规则（从 Vampire/E/iProver 的 experience 中得出）:
 *   equality_ratio > 0.5         → 推荐 ATP (superposition 专精等式)
 *   quantifier_ratio > 0.3       → 推荐 ATP (Inst-Gen 专精量化)
 *   arithmetic_heavy > threshold  → 推荐 SMT (Z3/cvc5 专精算术)
 *   nonlinear_degree > 2          → 推荐 SMT (Groebner 基专精非线性)
 *   混合特征                       → 推荐 ATP FIRST 然后 SMT 备用
 *
 * @return 问题特征结构体（含推荐路由）
 */
ProblemFeatures analyze_problem_features(
    const ConstraintGraph *graph,
    int goal_id);
```

---

## 8. Lv-00 映射方案：proof_multi_strategy 的 ATP 策略注册

### 8.1 新增证明策略类型

在 `proof_multi_strategy` 中注册三个 ATP 相关策略：

```c
/**
 * @brief 证明策略类型 —— 扩展 ATP 策略
 */
typedef enum {
    // ... 现有策略 ...
    PROOF_STRATEGY_GROEBNER_BASIS,
    PROOF_STRATEGY_AREA_METHOD,
    PROOF_STRATEGY_FULL_ANGLE,

    // === 新增 ATP 策略（P2） ===
    PROOF_STRATEGY_SUPERPOSITION,       /**< Vampire/E superposition 演算 */
    PROOF_STRATEGY_INST_GEN,            /**< iProver Inst-Gen 实例化 */
    PROOF_STRATEGY_ATP_AUTO,            /**< ATP 自动策略调度
                                           (先尝试 superposition，超时则 Inst-Gen) */
    PROOF_STRATEGY_ATP_HYBRID,          /**< ATP + SMT 混合策略
                                           (ATP 处理量化和等式，SMT 处理算术) */

    PROOF_STRATEGY_COUNT
} ProofStrategyType;
```

### 8.2 策略描述符注册

```c
/* 在 proof_multi_strategy 注册 ATP 策略 */

ProofStrategyDescriptor atp_strategies[] = {
    {
        .strategy = PROOF_STRATEGY_SUPERPOSITION,
        .name = "superposition",
        .description = "Vampire/E superposition 演算（等式推理专精）",
        .engine = PROOF_ENGINE_EXTERNAL_ATP,
        .atp_solver = ATP_SOLVER_VAMPIRE,
        .time_budget_default_ms = 30000,
        .time_budget_max_ms = 120000,
        .trust_color = PROOF_COLOR_GREEN,
        .supports_quantifiers = true,
        .supports_arithmetic = false,
    },
    {
        .strategy = PROOF_STRATEGY_INST_GEN,
        .name = "inst_gen",
        .description = "iProver Inst-Gen 实例化演算（量化问题专精）",
        .engine = PROOF_ENGINE_EXTERNAL_ATP,
        .atp_solver = ATP_SOLVER_IPROVER,
        .time_budget_default_ms = 30000,
        .time_budget_max_ms = 120000,
        .trust_color = PROOF_COLOR_GREEN,
        .supports_quantifiers = true,
        .supports_arithmetic = false,
    },
    {
        .strategy = PROOF_STRATEGY_ATP_AUTO,
        .name = "atp_auto",
        .description = "ATP 自动策略调度（分析问题特征→选择最优 ATP 策略）",
        .engine = PROOF_ENGINE_EXTERNAL_ATP,
        .atp_solver = ATP_SOLVER_COUNT,  /* 自动选择 */
        .time_budget_default_ms = 60000,
        .time_budget_max_ms = 180000,
        .trust_color = PROOF_COLOR_GREEN,
        .supports_quantifiers = true,
        .supports_arithmetic = false,
    },
};
```

### 8.3 ATP vs SMT 自动路由决策树

```
engine_scheduler.h 路由决策:

1. 分析问题特征
   │
   ├─ equality_ratio > 0.5 ?
   │    YES → PROOF_STRATEGY_SUPERPOSITION (Vampire)
   │    NO  → continue
   │
   ├─ quantifier_ratio > 0.3 ?
   │    YES → PROOF_STRATEGY_INST_GEN (iProver)
   │    NO  → continue
   │
   ├─ arithmetic_heavy > threshold ?
   │    YES → PROOF_STRATEGY_SMT (Z3/cvc5)
   │    NO  → continue
   │
   ├─ nonlinear_degree > 2 ?
   │    YES → PROOF_STRATEGY_GROEBNER_BASIS + SMT
   │    NO  → continue
   │
   └─ 混合特征 ?
        → PROOF_STRATEGY_ATP_HYBRID
          (ATP 先处理等式/量词部分，SMT 处理算术部分)
```

---

## 9. 总结映射表

### 9.1 Vampire / E Prover / iProver → Lv-00 核心概念映射

| ATP 概念 | ATP 系统/内部 | Lv-00 映射 | 文件 |
|---------|-------------|-----------|------|
| superposition 演算 | Vampire/E 核心推理引擎 | `PROOF_STRATEGY_SUPERPOSITION` 证明策略 | `proof.h` |
| AVATAR 架构 | Vampire SAT+saturation 混合 | `PROOF_STRATEGY_ATP_HYBRID` 混合策略 | `proof.h` |
| SINDE 启发式 | E Prover 自适应子句评估 | `SINDESelector` → 重写规则/约束传播优先级 | `rewrite.h` |
| Inst-Gen 演算 | iProver 实例化引擎 | `PROOF_STRATEGY_INST_GEN` 证明策略 | `proof.h` |
| TPTP FOF 编码 | TPTP 标准格式 | `geometry_to_tptp()` + `TPTP_FORMAT_FOF` | `atp_backend.h` |
| TPTP CNF 编码 | TPTP 子句范式 | `tptp_encode_clause()` | `atp_backend.h` |
| TSTP 证明输出 | TPTP 证明格式 | `tstp_to_proof_navigator()` | `atp_backend.h` |
| 策略自动调度 | Vampire strategy scheduling | `engine_scheduler.h` → ATP vs SMT 路由 | `engine_scheduler.h` |
| 子进程调用 | 命令行接口 | `atp_backend_spawn()` | `atp_backend.h` |
| 问题特征分析 | Vampire problem classification | `analyze_problem_features()` | `engine_scheduler.h` |
| E Prover 模块化 | 组件独立替换 | `ATPBackend` 统一接口 + 独立求解器实现 | `atp_backend.h` |

### 9.2 几何约束 → TPTP 谓词完整映射

| Lv-00 ConstraintType | TPTP 谓词/函数 | 谓词元数 | 备注 |
|----------------------|---------------|---------|------|
| `INCIDENCE(P, L)` | `incident(P, L)` | 2 | 点在线上 |
| `COLLINEAR(A, B, C)` | `collinear(A, B, C)` | 3 | 三点共线 |
| `BETWEEN(B, A, C)` | `between(B, A, C)` | 3 | 有序共线 |
| `PERPENDICULAR(L1, L2)` | `perpendicular(L1, L2)` | 2 | 垂直 |
| `PARALLEL(L1, L2)` | `parallel(L1, L2)` | 2 | 平行 |
| `INTERSECTION(P, O1, O2)` | `intersection(P, O1, O2)` | 3 | 相交 |
| `CONTAINMENT(P, R)` | `containment(P, R)` | 2 | 包含 |
| `MIDPOINT(M, A, B)` | `midpoint(M, A, B)` | 3 | M 是 AB 中点 |
| `segment(A, B)` | `segment(A, B)` | 2 (函数) | 线段构造器 |
| `midpoint(A, B)` | `midpoint(A, B)` | 2 (函数) | 中点构造器 |
| `distance(A, B)` | `distance(A, B)` | 2 (函数) | 距离函数 |
| `distance(A, B) = distance(C, D)` | `(distance(A,B) = distance(C,D))` | 等式 | 距离相等 |
| `LINE_EQUAL(L1, L2)` | `(L1 = L2)` | 等式 | 线段相等 |

### 9.3 ATP 策略与现有 SMT 策略的互补关系

| 维度 | ATP 策略 (Vampire/E/iProver) | SMT 策略 (Z3/cvc5) | Groebner 基策略 |
|------|---------------------------|-------------------|----------------|
| **擅长领域** | 等式推理、量化推理、几何构造等价 | 算术约束求解、坐标不等式 | 多项式等式系统 |
| **几何典型问题** | "中线交于一点"、"三角形全等→对应相等" | "距离满足三角不等式"、"面积为正" | "相交点坐标求解" |
| **输入格式** | TPTP FOF/CNF | SMT-LIB2 | 多项式方程组 |
| **证明输出** | TSTP resolution/superposition 推导 | SMT 模型或 unsat core | Groebner 基多项式 |
| **信任颜色** | GREEN（ATP 证明可独立验证） | GREEN（SMT unsat core） | GREEN（Groebner 基） |
| **集成状态** | 新增（本文档设计） | 已有（smt_backend.h） | 已有（groebner_basis.h） |

### 9.4 文件依赖关系

```
.lvz 公理包 / 约束图 / 证明目标
    │
    ├── tptp_encoder.h              (新增：几何 → TPTP 编码)
    │   ├── geometry_to_tptp()      (FOF/CNF/TFF 编码函数)
    │   └── tptp_encode_clause()    (CNF 子句编码)
    │
    ├── atp_backend.h               (新增：ATP 子进程调用封装)
    │   ├── ATPBackend              (会话管理)
    │   ├── atp_backend_spawn()     (子进程启动)
    │   └── tstp_to_proof_navigator()(TSTP 证明 → ProofNavigator)
    │
    ├── proof.h                     (扩展：ATP 证明策略注册)
    │   ├── PROOF_STRATEGY_SUPERPOSITION
    │   ├── PROOF_STRATEGY_INST_GEN
    │   └── PROOF_STRATEGY_ATP_AUTO
    │
    ├── engine_scheduler.h          (扩展：ATP vs SMT 自动路由)
    │   ├── analyze_problem_features()
    │   ├── ProblemFeatures         (特征分析)
    │   └── atp_vs_smt_routing()    (路由决策)
    │
    └── rewrite.h                   (扩展：SINDE 启发式)
        └── SINDESelector           (自适应规则优先级排序)
```

---

## 附录 A：Vampire 命令行调用示例

```
# Vampire 标准调用（30s 时间限制，TPTP 输入）
vampire --mode casc --time_limit 30s --proof tstp \
        problem.p

# E Prover 标准调用
eprover --auto --tstp-format --cpu-limit=30 \
        problem.p

# iProver 标准调用
iproveropt --time_out_real 30 --clausifier vampire \
           problem.p

# Lv-00 内部等价调用（atp_backend.h）
ATPBackend *backend = atp_backend_create(ATP_SOLVER_VAMPIRE, 30, 1024);
ATPResult result = atp_backend_solve(backend, graph, goal_id, TPTP_FORMAT_FOF);
if (result == ATP_RESULT_THEOREM) {
    atp_backend_convert_proof(backend, graph, nav);
}
```

---

## 附录 B：CASC 竞赛相关指标

| 指标 | 说明 |
|------|------|
| CASC (CADE ATP System Competition) | FOL ATP 领域年度竞赛 |
| Vampire CASC 成绩 | 多次 FOL 组冠军（2017-2024 多次夺冠） |
| E Prover CASC 成绩 | 多次 FOL/EHR 组冠军，与 Vampire 交替领先 |
| iProver CASC 成绩 | EPR 组冠军（专精量化问题） |
| TPTP 问题库规模 | 超过 25,000 个标准问题 |
| TPTP 几何问题子集 | GEO 类问题（约 300+ 个几何定理） |

---

## 附录 C：与现有 SMT 后端的协作协议

```
ATP-SMT 协作协议（engine_scheduler.h）:

1. 默认路由:
   problem_features = analyze_problem_features(graph, goal_id);
   if (problem_features.recommend_atp):
       → 先尝试 ATP (30s)
       if ATP 超时/放弃:
           → 回退 SMT (补充时间预算)
   else:
       → 先尝试 SMT

2. 混合策略 (PROOF_STRATEGY_ATP_HYBRID):
   - 步骤 1: ATP 处理等式和量化部分（superposition）
   - 步骤 2: 将 ATP 简化后的剩余问题传给 SMT
   - 步骤 3: SMT 处理算术不等式部分
   - 步骤 4: 合并两个系统的结果

3. 并行模式:
   - ATP 和 SMT 同步启动
   - 汇合:
     - 任何一方先返回 THEOREM → 取该结果
     - 双方均返回 COUNTERSATISFIABLE → 存在反例
     - 双方均超时 → 报告 UNKNOWN
```

---

> **文档结束**
> 本文档详述了 Vampire（superposition + AVATAR）、E Prover（SINDE 启发式 + 模块化设计）和 iProver（Inst-Gen 量化实例化）三大 FOL ATP 系统如何集成到 Lv-00 体系。核心结论：(1) ATP 后端通过 `atp_backend.h` 统一封装，作为现有 `smt_backend.h`（Z3/cvc5）的互补引擎——ATP 专精等式推理和量化处理，SMT 专精算术求解；(2) TPTP 标准编码将几何约束图转换为 ATP 可消费的 FOF/CNF 格式，TSTP 证明输出可逆向映射为 Lv-00 的 ProofNavigator 步骤；(3) `engine_scheduler.h` 通过问题特征分析（等式密度、量化密度、算术密度）自动路由到最优策略，借鉴 Vampire 的策略自动调度经验；(4) SINDE 自适应启发式可进一步集成到 `rewrite.h`，为 Lv-00 内部的重写规则和约束传播提供基于相关度的优先级排序。

# Dafny 三层验证架构与隐式动态帧借鉴设计

> **借鉴项目**：Dafny（github.com/dafny-lang/dafny）
> **核心借鉴点**：Source → Boogie → SMT 三层翻译架构、隐式动态帧（Implicit Dynamic Frame）、Ghost 方法即引理
> **分类**：P2 高优先级 / 验证架构与条件生成
> **日期**：2026-05-24

---

## 1. 概述

Dafny 是由微软研究院 Rustan Leino 团队开发的验证感知编程语言。其核心架构创新在于**三层翻译管道**：Dafny 源程序首先翻译到 Boogie 中间验证语言，Boogie 再生成验证条件（VC）并发送给 Z3 SMT 求解器。这种"Source → Boogie → SMT"的三层管道设计将程序语义、验证逻辑和求解器编码三个关注点彻底分离，为 Lv-00 的"几何 DSL → GVIL → SMT/求解器"管道提供了直接的架构蓝图。

Dafny 的三个核心技术对 Lv-00 的验证管道设计具有关键借鉴价值：

1. **三层翻译架构（Source → Boogie → SMT）**：Dafny 将程序验证分解为三个独立层——源语言层（Dafny 语法，包含 requires/ensures/invariant 等规约构造）、中间验证层（Boogie，提供 passthrough 的过程化验证语义）、求解器层（Z3 SMT-LIB 编码）。每层有独立的语义模型和优化空间。Lv-00 的对应管道是"几何 DSL（用户层）→ GVIL（中间验证层）→ SMT/代数/几何求解器（求解器层）"，三层之间可以独立演进和优化。

2. **隐式动态帧（Implicit Dynamic Frame, IDF）**：Dafny 采用 IDF 方法处理堆（heap）程序的验证——每个方法的 `reads` 子句声明该方法可以读取的堆位置集合，`modifies` 子句声明该方法可以修改的堆位置集合。这种"显式帧"概念精确对应 Lv-00 中约束图的修改边界管理：当几何构造修改某条线段的长度时，只有受该线段约束影响的其他构造才需要重新验证，其他不相关的约束保持不变。

3. **Ghost 方法即引理**：Dafny 中的 `ghost method`（鬼影方法）在运行时不存在，仅在编译期用于证明。用户可以将一段证明逻辑封装为 `lemma`（引理），验证器自动将其作为验证提示使用。这与 Lv-00 的"辅助构造 + Ghost 标记"完全对应——辅助构造在编译期执行证明功能，在生成的可执行约束图中被消除。

Dafny 的整体架构可以概括为：

```
Dafny 源程序（.dfy）
  ├─ method 声明（可执行代码 + requires/ensures 规约）
  ├─ function 声明（纯函数 + 规约）
  ├─ lemma 声明（Ghost 方法，仅用于证明）
  └─ class/trait 声明（面向对象结构）
       ↓ Dafny 编译器前端（解析、类型检查、解析）
  Boogie 中间程序（.bpl）
  ├─ procedure（对应 Dafny method/lemma）
  ├─ implementation（过程体，含 assume/assert）
  └─ axiom（公理，编码 Dafny 类型和函数语义）
       ↓ Boogie 验证条件生成器（VCG, weakest precondition）
  验证条件（VC, 一阶逻辑公式）
       ↓ Z3 SMT 求解器
  结果：Verified（通过）/ Error（反例位置）
```

Lv-00 的三层对应管道：

```
几何 DSL 源程序（用户层）
  ├─ construction 声明（可执行几何构造 + 命题规约）
  ├─ lemma 声明（Ghost 辅助构造，仅用于证明）
  └─ theorem 声明（目标定理）
       ↓ 语法解析 + 语义分析 + 类型检查
  GVIL 中间程序（IR 层）
  ├─ build_block（对应构造块，含 invariant 不变量）
  ├─ proof_step（对应证明步骤，含 assume/assert）
  └─ axiom（公理，编码几何基本定理如 SSS/SAS/ASA）
       ↓ 验证条件生成器（VCG, 几何最弱前条件）
  验证条件（几何约束公式）
       ↓ SMT/代数/几何多求解器
  结果：VERIFIED / COUNTEREXAMPLE / UNKNOWN
```

---

## 2. 核心借鉴 → Lv-00 映射

### 2.1 三层翻译架构映射到 Lv-00 验证管道

Dafny 的三层架构中，每层都有明确的职责边界：

| 层 | Dafny 组件 | 职责 | Lv-00 对应组件 |
|:---|:---|:---|:---|
| **源语言层** | Dafny 编译器前端 | 解析、类型检查、名称解析、规约消糖 | 几何 DSL 前端（解析器 + 语义分析器 + 类型检查器） |
| **中间验证层** | Boogie 语言 | 过程化验证语义、passification（消去表达式副作用）、SSA 转换 | GVIL（几何验证中间语言，表达式降级、约束规范化） |
| **求解器层** | Z3 SMT 求解器 | 求解验证条件、返回 SAT/UNSAT/DONTKNOW | `engine_scheduler.h` 多求解器（SMT/代数/几何/数值） |

**Boogie 的过程化验证语义**是三层架构中最关键的中间层设计。Boogie 是一种面向验证的中间语言，其核心概念包括：

```
// Boogie 过程声明
procedure P(x: int) returns (r: int)
  requires x > 0;              // 前置条件
  ensures r > x;               // 后置条件
  modifies $Heap;              // 帧条件（可修改的堆位置）

// Boogie 过程实现
implementation P(x: int) returns (r: int)
{
  var y: int;
  assume y == x + 1;           // 假设（来自调用者的保证或分支条件）
  r := y;
  assert r > x;                // 断言（需要验证的条件）
}
```

Boogie 验证条件生成器（VCG）基于**最弱前条件**（Weakest Precondition, WP）演算，将过程体中的 `assert` 语句转换为验证条件：

```
WP(r := y; assert r > x, Q)
  = WP(r := y, WP(assert r > x, Q))
  = WP(r := y, r > x ∧ Q)
  = (y > x) ∧ Q[r:=y]
```

在 Lv-00 中，GVIL 需要类似的最弱前条件生成能力：

```
几何构造块：
  construction BuildTriangle(A: Point, B: Point) -> T: Triangle
    requires noncollinear(A, B, C_free)
    ensures is_triangle(T) ∧ vertices(T) = {A, B, C_free}

WP 演算（几何版）：
  WP(construct C = Midpoint(A, B), |AC| = |CB|)
  = WP(construct C, WP(|AC| = |CB|))
  = WP(construct C, |AC| = |CB|)
  = midpoint_property(A, B, C) ⇒ |AC| = |CB|
```

### 2.2 隐式动态帧（IDF）与约束图修改边界

Dafny 的 IDF 方法通过 `reads` 和 `modifies` 子句精确描述每个方法的**内存足迹**（footprint）：

```
method RotatePoint(p: Point, angle: real) returns (q: Point)
  reads p               // 只读 p 的坐标（不读取其他点）
  modifies q            // 只修改 q 的坐标（不修改其他点）
  ensures |q| == |p|    // 旋转保持到原点距离不变
{
  q.x := p.x * cos(angle) - p.y * sin(angle);
  q.y := p.x * sin(angle) + p.y * cos(angle);
}
```

在 Lv-00 的约束图（constraint graph）中，这种帧概念对应为**约束影响的传播边界**：

| Dafny IDF 概念 | Lv-00 约束图映射 | 含义 |
|:---|:---|:---|
| `reads` 子句 | `dependency_set(node)` | 节点依赖的约束变量集合（只读依赖） |
| `modifies` 子句 | `affected_set(node)` | 节点修改后需要重新验证的约束集合（脏传播边界） |
| 帧推理 | `constraint_slice(affected_nodes)` | 仅对受影响的约束子图重新求解 |
| `fresh` 操作符 | 新分配的节点 ID 范围 | 标记"不受任何已有帧约束"的新构造 |

帧边界的增量验证策略：

```
约束图修改前的拓扑状态：
  ┌─────────┐
  │ 节点 A  │ ──依赖──→ │ 约束 C1 │
  └────┬────┘           └────┬────┘
       │                     │
  ┌────┴────┐           ┌────┴────┐
  │ 节点 B  │ ──依赖──→ │ 约束 C2 │
  └─────────┘           └─────────┘

用户修改节点 A（如改变 A 的位置）：
  1. 标记 A 为 dirty
  2. 沿依赖边传播：A → C1 → 所有依赖 C1 的节点
  3. 传播停止在 unaffected 边界（B 不依赖 A，不受影响）
  4. 仅对 {A, C1, 依赖 C1 的节点} 重新验证
  5. {B, C2} 保持已验证状态
```

### 2.3 Ghost 方法即引理

Dafny 的核心设计之一是将"证明"（proof）和"计算"（computation）统一在同一个语言框架下：

```
// Ghost 方法——运行时不存在，仅用于证明
ghost method Lemma_Triangle_Inequality(a: int, b: int, c: int)
  requires a > 0 && b > 0 && c > 0
  ensures a + b > c
{
  // 证明体：由于 a, b, c > 0，显然 a + b > c
  assert a + b >= a + 1;  // 因为 b > 0
  assert a + 1 > 0;       // 因为 a > 0
  // ... 进一步推理步骤 ...
}

// 普通方法——运行时执行
method ComputePerimeter(a: int, b: int, c: int) returns (p: int)
  requires a > 0 && b > 0 && c > 0
  requires a + b > c && a + c > b && b + c > a  // 三角不等式
  ensures p == a + b + c
{
  Lemma_Triangle_Inequality(a, b, c);  // 调用 Ghost 引理
  p := a + b + c;
}
```

在 Lv-00 中，Ghost 方法对应"标记为编译期存在的辅助几何构造"：

| Dafny 构造 | Lv-00 proof.h 映射 | 说明 |
|:---|:---|:---|
| `ghost method` | `ProofStep` + `is_ghost = true` | 鬼影证明步骤，编译后消除 |
| `lemma` | `ProofLemma` 结构体 | 可复用的命名引理 |
| `calc` 语句 | `proof_calc_chain()` | 链式等式推导 |
| `assert` 语句 | `proof_assert_constraint()` | 断言检查点 |
| `assume` 语句 | `constraint_assume()` | 假设引入（来自公理或前置条件） |
| `invariant` | `constraint_invariant()` | 循环/构造不变式 |

### 2.4 验证条件自动生成（VCG）在几何证明中的应用

Dafny 的 VCG 会将程序代码和规约自动转换为验证条件。在 Lv-00 的几何场景中，VCG 需要处理的是**几何构造序列**而不是程序代码：

```
Lv-00 几何构造序列：
  Given: A, B, C (三点不共线)
  Step 1: construct D = Midpoint(A, B)
  Step 2: construct E = Midpoint(B, C)
  Step 3: construct line_DE = Line(D, E)
  Goal: parallel(line_DE, Line(A, C))

VCG 生成的验证条件：
  VC1: D = Midpoint(A, B) → |AD| = |DB|
  VC2: E = Midpoint(B, C) → |BE| = |EC|
  VC3: (|AD| = |DB| ∧ |BE| = |EC|) → parallel(DE, AC)
       （中位线定理）
```

VCG 的核心算法是基于**最弱前条件演算**（Weakest Precondition Calculus）适配到几何构造序列：

```
WP_geometric(construction_seq, goal):
  if construction_seq is empty:
    return goal  // 基础情况
  else:
    let op = last(construction_seq)
    let rest = all_but_last(construction_seq)
    // 将操作的后置条件替换到 goal 中
    let substituted = substitute(op.postcondition, goal)
    return WP_geometric(rest, substituted)
```

### 2.5 对照表：Dafny 构造 → Lv-00 proof.h 映射

| Dafny 构造 | Lv-00 proof.h 映射 | 语义 |
|:---|:---|:---|
| `method` | `ConstructionBlock` | 命名几何构造块 |
| `function` | `PureGeomFunction` | 纯几何计算函数 |
| `lemma` | `ProofLemma` | 命名几何引理 |
| `requires` | `precondition_ids` | 构造的前置几何条件 |
| `ensures` | `postcondition_ids` | 构造的后置几何命题 |
| `invariant` | `invariant_ids` | 构造过程中的不变几何性质 |
| `assert` | `proof_assert_constraint()` | 中间断言检查点 |
| `calc` | `proof_calc_chain()` | 链式几何等式推导 |
| `forall` | `QUANTIFIER_FORALL` 节点 | 全称量化几何约束 |
| `exists` | `QUANTIFIER_EXISTS` 节点 | 存在量化几何约束 |
| `old()` | `constraint_snapshot()` | 构造前后的状态快照对比 |

### 2.6 代码示例：Lv-00 中实现几何不变式的自动验证条件生成

```c
/**
 * @file proof.h (追加)
 * @brief 几何验证条件自动生成（VCG）—— 借鉴 Dafny Source → Boogie → SMT 三层架构
 *
 * Dafny 的三层翻译管道：
 *   Dafny Source → Boogie (中间验证语言) → SMT 求解器
 *
 * Lv-00 适配为：
 *   几何 DSL → GVIL (几何验证中间语言) → SMT/代数/几何求解器
 *
 * 核心 API：
 *   proof_generate_vc()    —— 从构造块生成验证条件
 *   proof_verify_block()   —— 自动验证构造块是否满足规约
 *   proof_lemma_apply()    —— 将已证明的引理应用于当前证明
 */

#ifndef LV00_PROOF_VCG_H
#define LV00_PROOF_VCG_H

#include "gvil.h"

/* ── 构造块（对应 Dafny method） ────────────────────── */

/**
 * @brief 几何构造块——借鉴 Dafny 的 method 概念
 *
 * 每个构造块包含：
 *  - 前置条件：构造前必须成立的几何条件（对应 Dafny requires）
 *  - 构造体：几何构造步骤序列
 *  - 后置条件：构造后保证成立的几何命题（对应 Dafny ensures）
 *  - 不变量：构造过程中始终保持的几何性质（对应 Dafny invariant）
 */
typedef struct {
    const char *block_name;              /**< 构造块名称（如 "BuildEquilateralTriangle"） */
    GvilPredicate **preconditions;       /**< 前置条件列表（requires） */
    int precondition_count;
    GvilConstructionStep *body;          /**< 构造体步骤序列 */
    int body_step_count;
    GvilPredicate **postconditions;      /**< 后置条件列表（ensures） */
    int postcondition_count;
    GvilPredicate **invariants;          /**< 循环/构造不变式列表（invariant） */
    int invariant_count;
    bool is_ghost;                       /**< 是否为 Ghost 构造（对应 Dafny ghost method） */
} ConstructionBlock;

/* ── 验证条件（对应 Boogie VCG 输出） ────────────────── */

/**
 * @brief 单条验证条件
 *
 * 对应 Boogie 的 Verification Condition（VC）。
 * 一条 VC 通常是一个蕴含式：antecedent → consequent，
 * 其中 antecedent 是前置条件和不变式的合取，
 * consequent 是后置条件。
 */
typedef struct {
    int vc_id;                           /**< VC 编号 */
    GvilPredicate *antecedent;           /**< 前提（前置条件 + 不变式 + 已证断言） */
    GvilPredicate *consequent;           /**< 结论（后置条件或 assert 语句） */
    int source_step_index;              /**< 来源步骤（哪个步骤生成了此 VC） */
    char *description;                   /**< VC 的自然语言描述 */
} VerificationCondition;

/**
 * @brief VC 的验证结果
 */
typedef enum {
    VC_VERIFIED,          /**< 验证条件成立（已证明） */
    VC_FAILED,            /**< 验证条件不成立（有反例） */
    VC_UNKNOWN,           /**< 无法判定（求解器超时或超出能力） */
    VC_TRIVIAL            /**< 平凡成立的 VC（无需调用求解器） */
} VCResult;

/* ── 验证条件生成 API ───────────────────────────────── */

/**
 * @brief 从几何构造块生成验证条件
 *
 * 借鉴 Dafny 的 VCG：基于最弱前条件演算（WP），
 * 将构造体中的每个断言（assert）和后置条件（ensures）
 * 转换为待验证的逻辑蕴含式。
 *
 * 工作流程：
 *  1. 初始化 WP = postcondition（最弱前条件 = 后置条件）
 *  2. 从构造体末尾向前遍历每个步骤：
 *     - 如果是 assert(P): 生成 VC: (WP ∧ 前置条件) → P
 *     - 如果是 construct(Op): WP = substitute(Op.post, WP)
 *     - 如果是 invariant(I): 检查 I 的入性（entry）和保持性（preservation）
 *  3. 遍历完成后，所有 VC 收集到 out_vcs
 *
 * 几何 WP 演算的关键规则：
 *  - WP(construct M = Midpoint(A,B), goal)
 *    = midpoint_def(M,A,B) ⇒ goal
 *    其中 midpoint_def = (|AM|=|MB|) ∧ collinear(A,M,B)
 *
 *  - WP(construct L = PerpendicularBisector(A,B), goal)
 *    = perp_bisector_def(L,A,B) ⇒ goal
 *    其中 perp_bisector_def = ∀P∈L, |PA|=|PB|
 *
 * @param block        几何构造块
 * @param context      全局约束上下文（已建立的几何事实）
 * @param out_vcs      输出：生成的验证条件数组
 * @param out_count    输出：VC 数量
 * @return 生成结果（成功/失败）
 */
VCGenResult proof_generate_vc(
    const ConstructionBlock *block,
    const GeometricalContext *context,
    VerificationCondition ***out_vcs,
    int *out_count
);

/**
 * @brief 验证单条验证条件
 *
 * 将 VC 编码为后端求解器的输入格式并求解。
 * 借鉴 Dafny 的 Boogie → Z3 流程。
 *
 * @param vc           待验证的验证条件
 * @param backend      求解器后端选择
 * @param timeout_ms   超时（毫秒），0 表示使用默认值
 * @return 验证结果
 */
VCResult proof_verify_vc(
    const VerificationCondition *vc,
    SolverBackend backend,
    int timeout_ms
);

/**
 * @brief 自动验证构造块的所有验证条件
 *
 * 便利函数：先调用 proof_generate_vc() 生成所有 VC，
 * 再逐条调用 proof_verify_vc() 验证。
 * 返回聚合结果：所有 VC 通过 → VERIFIED，任一 VC 失败 → FAILED。
 *
 * @param block        几何构造块
 * @param context      全局约束上下文
 * @param backend_pref 后端偏好（多后端时按此顺序尝试）
 * @return 聚合验证结果
 */
ProofResult proof_verify_block(
    const ConstructionBlock *block,
    const GeometricalContext *context,
    const SolverBackend *backend_pref
);

/* ── 几何不变式检查 ─────────────────────────────────── */

/**
 * @brief 验证几何不变式
 *
 * 借鉴 Dafny 的 invariant 检查：
 *  - 入性检查（entry）：不变式在构造开始时成立
 *  - 保持性检查（preservation）：如果不变式在步骤前成立，步骤执行后仍然成立
 *
 * 在几何构造中，不变式如：
 *  - "在所有步骤中，某个三角形面积保持不变"
 *  - "在所有步骤中，某三点始终保持共线"
 *  - "在所有步骤中，某线段长度保持不变"
 *
 * @param block        几何构造块
 * @param invariant_idx 不变式索引
 * @param context      约束上下文
 * @return 不变式检查结果
 */
InvariantResult proof_check_invariant(
    const ConstructionBlock *block,
    int invariant_idx,
    const GeometricalContext *context
);

/* ── 引理应用（对应 Dafny lemma 调用） ──────────────── */

/**
 * @brief 几何引理——借鉴 Dafny lemma
 *
 * 一个几何引理封装了一段可复用的证明逻辑。
 * 例如中位线定理是一个引理，可以在任意需要"平行且等于一半"的地方应用。
 */
typedef struct {
    const char *lemma_name;              /**< 引理名称 */
    GvilPredicate **premises;            /**< 引理前提（对应 Dafny requires） */
    int premise_count;
    GvilPredicate *conclusion;           /**< 引理结论（对应 Dafny ensures） */
    bool is_axiom;                       /**< 是否为公理（不需求解器验证） */
    bool is_proven;                      /**< 是否已经过验证 */
} ProofLemma;

/**
 * @brief 将已证明的引理应用于当前证明目标
 *
 * 借鉴 Dafny 的 lemma 调用机制：
 * 如果当前证明目标匹配某个引理的结论，
 * 则将该引理的前提作为新的子目标。
 *
 * 示例：
 *   目标: parallel(DE, AC)
 *   引理 midline_theorem: 如果 D=Midpoint(A,B) ∧ E=Midpoint(B,C) → parallel(DE, AC)
 *   应用后：
 *     新目标1: D = Midpoint(A, B)
 *     新目标2: E = Midpoint(B, C)
 *
 * @param lemma        已证明的引理
 * @param current_goal 当前证明目标
 * @param out_subgoals 输出：引理前提作为新的子目标
 * @param out_count    输出：子目标数量
 * @return 是否成功匹配并应用
 */
bool proof_lemma_apply(
    const ProofLemma *lemma,
    const GvilPredicate *current_goal,
    GvilPredicate ***out_subgoals,
    int *out_count
);

/**
 * @brief 链式几何等式推导——借鉴 Dafny calc 语句
 *
 * Dafny 的 calc 语句提供可读的等式推导链。
 * 在 Lv-00 中适配为几何量（长度、角度、面积）的推导链。
 *
 * 示例：
 *   calc {
 *     |AB|^2;
 *     = { Pythagorean(△ABC, right_angle_at_C); }
 *     |AC|^2 + |BC|^2;
 *     = { Given_AC_eq_3; Given_BC_eq_4; }
 *     9 + 16;
 *     = { arithmetic; }
 *     25;
 *   }
 *
 * @param chain       推导链（每个 hint 是一个已证明的引理或已知事实）
 * @param chain_len   链的长度
 * @return 推导是否有效（每步的 hint 是否成立）
 */
bool proof_calc_chain(
    const CalcStep *chain,
    int chain_len
);

/**
 * @brief 几何构造的快照——借鉴 Dafny old() 操作符
 *
 * 用于对比构造前后的几何状态。
 * 例如：ensures |AB| == old(|AB|)  表示 AB 长度在构造后不变
 *
 * @param constraint_id  需要快照的约束 ID
 * @return 快照句柄
 */
SnapshotHandle constraint_snapshot(int constraint_id);

#endif /* LV00_PROOF_VCG_H */
```

### 2.7 三层管道中 GVIL 的承上启下作用

GVIL 在 Lv-00 中的角色精确对应 Boogie 在 Dafny 中的角色：

**承接上层（几何 DSL）**：
- 接收几何 DSL 的 AST，将其中的高级构造（如"构造等边三角形"）展开为基本步骤序列
- 消解语法糖（如"构造角平分线"展开为"构造两个等角"）
- 执行类型检查和几何语义验证

**向下编码（求解器）**：
- 将几何约束编码为各求解器的输入格式
- 执行约束的代数化（如角度约束 → 三角函数等式 → 多项式等式）
- 选择最合适的求解器并管理求解器进程

**关键设计原则**：
- GVIL 必须与所有后端求解器的输入格式保持**双向可逆**（编码可逆，便于反例报告）
- GVIL 的类型系统应比几何 DSL 更底层但比求解器输入更高层——这是"中间语言"的核心定义
- GVIL 的变换（pass）应与求解器无关——如约束化简、冗余消除等优化应在 GVIL 层完成

---

## 3. 实现方案

### 3.1 第一阶段：基础 VCG 管线（P2-1）

- [ ] 定义 `ConstructionBlock` 结构体（preconditions + body + postconditions + invariants）
- [ ] 实现 `proof_generate_vc()` 核心 VCG 算法（基于 WP 演算）
- [ ] 实现几何 WP 演算的基础规则：
  - [ ] 中点构造的 WP 规则
  - [ ] 垂线/平行线构造的 WP 规则
  - [ ] 角平分线构造的 WP 规则
  - [ ] 圆构造的 WP 规则
- [ ] 实现 `proof_verify_vc()` 单 VC 验证
- [ ] 编写 VCG 的单元测试（5 个代表性几何构造块）

### 3.2 第二阶段：不变式检查与帧推理（P2-2）

- [ ] 实现 `proof_check_invariant()` 不变式入性检查和保持性检查
- [ ] 实现约束图依赖分析（`dependency_set` 和 `affected_set`）
- [ ] 实现帧推理的增量验证策略
- [ ] 实现 `constraint_snapshot()` 构造前后状态快照
- [ ] 编写不变式检查的单元测试

### 3.3 第三阶段：引理系统（P2-3）

- [ ] 定义 `ProofLemma` 结构体和引理注册表
- [ ] 实现 `proof_lemma_apply()` 引理匹配和应用
- [ ] 实现 `proof_calc_chain()` 链式等式推导
- [ ] 预装常见几何引理库（中位线定理、勾股定理、角平分线定理等 20+ 引理）
- [ ] 编写引理系统的单元测试

### 3.4 第四阶段：GVIL 中间层完整实现（P2-4）

- [ ] 定义 GVIL 的完整 AST 和语义
- [ ] 实现几何 DSL → GVIL 的编译前端
- [ ] 实现 GVIL 的约束规范化和冗余消除 pass
- [ ] 实现 GVIL 的序列化/反序列化
- [ ] 实现 GVIL 层的调试输出（对应 Boogie 的 `/trace` 模式）

### 3.5 第五阶段：管道集成与端到端测试（P2-5）

- [ ] 将 VCG 管线与 `engine_scheduler.h` 集成
- [ ] 实现三级缓存策略：VC 级别 → 构造块级别 → 证明目标级别
- [ ] 实现 VCG 的性能分析（VC 数量 vs 构造块规模的关系）
- [ ] 端到端测试：从几何 DSL → GVIL → VCG → 求解器 → 结果报告
- [ ] 编写用户文档和 API 文档

---

## 4. 参考资源

- **Dafny 官方仓库**：[https://github.com/dafny-lang/dafny](https://github.com/dafny-lang/dafny)
- **Dafny 官方文档**：[https://dafny.org/dafny/DafnyRef/DafnyRef](https://dafny.org/dafny/DafnyRef/DafnyRef)
- **Boogie 中间验证语言**：[https://github.com/boogie-org/boogie](https://github.com/boogie-org/boogie)
- **三层架构论文**：Leino, K.R.M. "Dafny: An Automatic Program Verifier for Functional Correctness" (LPAR-16, 2010)
- **隐式动态帧（IDF）**：Smans, Jacobs, Piessens. "Implicit Dynamic Frames: Combining Dynamic Frames and Separation Logic" (ECOOP 2009)
- **最弱前条件演算**：Dijkstra, E.W. "A Discipline of Programming" (1976) —— VCG 的理论基础
- **几何 WP 演算**：Chou, Gao, Zhang. "Machine Proofs in Geometry" (1994) —— 几何定理的机械化 WP 方法
- **Boogie VCG 实现**：Barnett, Leino. "Weakest-Precondition of Unstructured Programs" (PASTE 2005)
- **Dafny calc 语句**：Leino, Polikarpova. "Verified Calculations" (VSTTE 2013)
- **本系列相关文档**：
  - `dafny_ensures_verification.md` —— Dafny ensures 规约一体化验证
  - `fstar_refinement_smt.md` —— F* 精化类型 + SMT 混合验证
  - `why3_multi_prover_dispatch.md` —— Why3 多求解器分派
  - `isabelle_sledgehammer_integration.md` —— Isabelle Sledgehammer 集成

# Lv-00 参考落地设计文档：egg e-graph 等式饱和重写框架

> **版本**: 1.0.0
> **日期**: 2026-05-24
> **参考**: egg (github.com/egraphs-good/egg) —— Rust e-graph/等式饱和框架，以及 egglog (github.com/egraphs-good/egglog) —— Datalog+e-graph 混合系统
> **目标**: 将 egg 的非破坏性重写（等式饱和）、同余闭包自动推理、声明式规则语法、egglog 约束传播机制映射到 Lv-00 的 rewrite.h、constraint_graph.h 和 .lvz 公理包

---

## 目录

1. [egg 项目概述与 Lv-00 借鉴动机](#1-egg-项目概述与-lv-00-借鉴动机)
2. [核心借鉴要点一：非破坏性重写与等式饱和](#2-核心借鉴要点一非破坏性重写与等式饱和)
3. [核心借鉴要点二：同余闭包与几何构造一致性](#3-核心借鉴要点二同余闭包与几何构造一致性)
4. [核心借鉴要点三：声明式规则语法与 .lvz 公理声明](#4-核心借鉴要点三声明式规则语法与-lvz-公理声明)
5. [核心借鉴要点四：egglog 约束传播与约束图增强](#5-核心借鉴要点四egglog-约束传播与约束图增强)
6. [Lv-00 映射方案：rewrite.h 的等式饱和引擎](#6-lv-00-映射方案rewriteh-的等式饱和引擎)
7. [Lv-00 映射方案：constraint_graph.h 的 E-Class 增强](#7-lv-00-映射方案constraint_graphh-的-e-class-增强)
8. [完整示例：三角形重心定理的等式饱和证明](#8-完整示例三角形重心定理的等式饱和证明)
9. [总结映射表](#9-总结映射表)

---

## 1. egg 项目概述与 Lv-00 借鉴动机

### 1.1 egg 是什么

egg（e-graphs good）是一个用 Rust 编写的高性能等式饱和（equality saturation）框架。它的核心创新是将 **e-graph（等式图）** 这一经典数据结构与现代程序优化相结合，被广泛应用于编译器优化（Herbie 浮点精度提升、Cranelift 代码生成、Ruler 规则推导）。egg 的核心机制包括：

| 能力 | 说明 | 在 egg 中的载体 |
|------|------|-------------------|
| **e-graph** | 高效表示大量等价项的数据结构，将表达式聚类为 e-class（等价类） | `EGraph<L>` |
| **非破坏性重写** | 所有重写结果共存于 e-class 中，无"先应用哪条规则"的顺序问题 | `apply_rewrites()` 后 `rebuild()` |
| **同余闭包** | 如果 a=b，则自动推导 f(a)=f(b)，无需单独声明 | `rebuild()` 中的 congruence 传播 |
| **等式饱和** | 反复应用规则直到不动点——规则应用顺序无关 | 循环 `search` + `apply` + `rebuild` |
| **egglog** | Datalog + e-graph 混合：用逻辑规则约束 e-graph 节点的关系 | `egglog` 项目 |
| **声明式规则** | `rewrite!("name"; "pattern" => "replacement")` 简洁语法 | `rewrite!` 宏 |
| **模式匹配** | Searcher/Applier 分离，支持条件重写 | `Rewrite` trait |

### 1.2 Lv-00 借鉴动机

Lv-00 的 `rewrite.h` 已经具备了 VF2 子图同构匹配和 WL 哈希循环检测，但现有重写是**破坏性的**——每次应用规则会替换图中的子结构，选择了某条规则就等于放弃了其他规则的可能性。这是传统重写系统的固有问题。egg 的等式饱和正好解决了这个问题：

| 借鉴方向 | egg 特性 | Lv-00 现有基础 | 差距与目标 |
|----------|---------|---------------|-----------|
| **非破坏性重写** | 所有结果共存于 e-class | `apply_rewrite()` 原地替换子图 | 引入 GEOM_E_CLASS 节点类型，构造等价类而非替换 |
| **同余闭包** | a=b → f(a)=f(b) 自动推导 | 手动声明每个等价传播规则 | 在 constraint_graph.h 中实现 `propagate_congruence()` |
| **等式饱和循环** | 反复应用直到不动点 | `rewrite_apply_all_matches()` 单轮 | 增加迭代控制和 WL 哈希不动点检测 |
| **声明式规则** | `rewrite!("name"; "LHS" => "RHS")` | C 函数注册 `rewrite_rule_create()` | .lvz 公理包解析 @rewrite 声明 |
| **egglog 约束** | Datalog 规则约束 e-graph | ConstraintGraph 已有约束边 | 添加 constraint 驱动的合并推理 |

### 1.3 总体架构对照

```
egg                           Lv-00
────────────────────────────────────────────────
EGraph<L>                     ConstraintGraph + EClassTable (新增)
  e-class {a, b, c}          → GEOM_E_CLASS 节点（容器）
  e-node f(a)                → FuncBlock 节点（构造器）
  union(a, b)                → constraint_graph_merge_nodes()
  rebuild()                  → propagate_congruence() + rebuild_wl_hashes()

rewrite!("name"; "pat" => "rep")   .lvz 文件中的 @rewrite 声明
  Searcher (LHS pattern)     → RewritePattern (VF2 子图匹配)
  Applier (RHS template)     → RewriteReplacement (重构子图)
  apply_rewrites()           → rewrite_egraph_saturate()

egglog                        constraint_graph.h + constraint_propagation.c (新增)
  relation decl               → ConstraintType 枚举
  rule decl                   → ConstraintPropagationRule
  seminaive evaluation        → propagation_run_loop()
```

---

## 2. 核心借鉴要点一：非破坏性重写与等式饱和

### 2.1 传统重写的"选择焦虑"

传统重写系统（包括 Lv-00 当前的 `apply_rewrite()`）面临的核心问题是：当多条规则同时匹配时，必须选择其一。这个选择是破坏性的——一条规则的 RHS 会覆盖 LHS，导致其他规则失去机会。

```
传统重写（破坏性）：
  expr: (a + b) * (a + b)
  规则1: x * x → x²       → 替换后: (a + b)²
  规则2: (x + y)² → x² + 2xy + y²  → 无法应用（已破坏）

egg 等式饱和（非破坏性）：
  e-class 0: {(a+b)*(a+b), (a+b)², a² + 2ab + b²}
  // 三个等价表示共存，两条规则都得到了应用
```

### 2.2 e-graph 的核心数据结构

e-graph 由两种实体组成：

| 实体 | 定义 | Lv-00 映射 |
|------|------|-----------|
| **e-class** | 一组等价节点的集合（等价类） | `GEOM_E_CLASS` 节点类型，包含 `member_node_ids[]` |
| **e-node** | 一个构造器应用于若干 e-class 子节点的实例 | `FuncBlock` 节点（构造器）或普通 `GeomNode` |
| **union** | 将两个 e-class 合并为一个（发现等价时） | `constraint_graph_merge_nodes()` 或新增 `egraph_union()` |
| **rebuild** | 传播同余闭包 + 去重 e-node | `propagate_congruence()` + `rebuild_hashes()` |

### 2.3 Lv-00 中的 e-class 节点设计

在现有约束图节点类型基础上，新增 `GEOM_E_CLASS` 作为等价类容器：

```c
/**
 * @brief E-Class 节点 —— 几何实体的等价类容器
 *
 * 借鉴 egg 的 e-class 设计。
 * 一个 GEOM_E_CLASS 节点可以包含多个互等价的几何构造（成员节点）。
 * 当重写规则发现 LHS 模式匹配时，不删除原节点，而是将 RHS 构造
 * 的节点加入同一个 GEOM_E_CLASS。
 *
 * 几何构造的一致性检查也通过 e-class 进行：
 * 如果两个构造在同一 e-class 中，则它们表示相同的几何实体。
 */
typedef struct {
    int eclass_id;                          /**< 本 e-class 的节点 ID */

    /* 成员节点（在同一等价类中的所有节点） */
    int *member_node_ids;                   /**< 成员节点 ID 数组 */
    int member_count;

    /* 代表节点：该等价类中最简/最规约的表示 */
    int representative_node_id;

    /* 节点类型标签（该等价类中所有成员的共同类型） */
    GeomType member_type;

    /* 同余闭包元数据 */
    uint64_t congruence_hash;               /**< 用于同余闭包的去重哈希 */
    bool needs_congruence_check;            /**< 标记：合并后需要重算同余闭包 */
} EClassData;

/**
 * @brief 全局 E-Class 表 —— 管理约束图中所有等价类
 *
 * 独立于 GeomNode 数组存储。每个 EClassData 对应一个 GEOM_E_CLASS 节点。
 * 通过 constraint_graph 的节点 ID 索引。
 */
typedef struct {
    EClassData *eclasses;                   /**< 动态数组 */
    int eclass_count;
    int capacity;

    /* 快速查找：node_id → eclass_id */
    int *node_to_eclass;                    /**< 映射表 */
    int node_to_eclass_capacity;

    /* 统计 */
    int total_union_operations;
    int total_congruence_propagations;
} EClassTable;
```

### 2.4 等式饱和循环设计

借鉴 egg 的 `search` → `apply` → `rebuild` 循环，设计 Lv-00 的等式饱和引擎：

```
rewrite_egraph_saturate(graph, rules, rule_count, max_iterations, max_eclass_size):
  1. 初始化 EClassTable：每个节点自成一类
  2. while iteration < max_iterations:
     a. search: 用每条规则的 LHS Pattern 在图中查找所有匹配
     b. apply: 对每个匹配，构造 RHS 子图，将 RHS 根节点 union 进 LHS 匹配的 e-class
     c. rebuild: 传播同余闭包，去重 e-node
     d. reached_fixedpoint: 检查 WL 哈希是否不变
     e. if reached_fixedpoint: break
  3. 返回饱和后的 EClassTable
```

**函数声明**（追加到 rewrite.h）：

```c
/**
 * @brief 在约束图上执行等式饱和（Equality Saturation）
 *
 * 借鉴 egg 的非破坏性重写范式。
 * 核心调度循环：
 *   1. search：对所有规则查找 LHS 匹配
 *   2. apply：将各匹配的 RHS 构造加入对应的 e-class
 *   3. rebuild：传播同余闭包 + 去重
 *   4. 检查不动点（WL 哈希是否不变）
 *
 * 与传统的 rewrite_apply_all_matches() 的区别：
 *   - 传统：匹配后替换，破坏原结构
 *   - 等式饱和：匹配后将 RHS 加入等价类，原结构和所有变体共存
 *
 * @param[in,out] graph        约束图
 * @param[in]     eclass_table E-Class 表（调用者预分配）
 * @param[in]     rules        重写规则数组
 * @param[in]     rule_count   规则数量
 * @param[in]     max_iterations  最大迭代次数（0 = 无限制）
 * @param[in]     max_eclass_size 每个 e-class 的最大成员数（0 = 无限制）
 * @return REWRITE_OK (达到不动点), REWRITE_APPLIED (达到最大迭代次数),
 *         REWRITE_NO_MATCH (无规则匹配), REWRITE_TERMINATED (e-class 溢出)
 *
 * @note 借鉴 egg 的 Runner 设计。
 *       生产级案例：Herbie 用等式饱和优化浮点表达式（精确度提升 100x+），
 *       Cranelift 用等式饱和优化 WASM 代码生成。
 */
RewriteStatus rewrite_egraph_saturate(
    ConstraintGraph *graph,
    EClassTable *eclass_table,
    RewriteRule **rules,
    int rule_count,
    int max_iterations,
    int max_eclass_size);
```

---

## 3. 核心借鉴要点二：同余闭包与几何构造一致性

### 3.1 egg 中的同余闭包

同余闭包（congruence closure）是 egg 最优雅的特性之一。其核心思想是：如果 a 和 b 被证明等价（在同一个 e-class 中），那么任何以 a 和 b 为子节点的构造器 f(a) 和 f(b) 也自动等价。这消除了对"传播等价性"的显式规则需求。

```
egg 同余闭包示例：
  e-class 1: {A, B}            // A = B 已被证明
  构造器: midpoint(A, C) 和 midpoint(B, C)
  → 同余闭包自动推导: midpoint(A, C) = midpoint(B, C)
  → 将 midpoint(A, C) 和 midpoint(B, C) union 到同一个 e-class
```

### 3.2 同余闭包在几何证明中的关键价值

在几何证明中，同余闭包直接对应**构造的一致性**：

| 几何场景 | 同余闭包推导 |
|---------|-------------|
| 点 A 和 B 被证明为同一点 | 以 A 为端点的线段 = 以 B 为端点的线段 |
| 线段 AB 和 CD 被证明相等 | AB 的中点 = CD 的中点（自动） |
| 三角形 ABC 和 DEF 被证明全等 | 重心 G(ABC) = 重心 G(DEF)（自动） |
| 角度相等 | 角平分线自动对应 |

这消除了 Lv-00 当前设计中需要**为每个构造器手动编写等价传播规则**的负担。

### 3.3 propagate_congruence() 设计

```c
/**
 * @brief 传播同余闭包 —— 借鉴 egg 的 congruence 传播
 *
 * 当约束图中两个节点被合并到同一个 GEOM_E_CLASS 时，
 * 自动检查以这两个节点为参数的 FuncBlock 构造是否应合并。
 *
 * 算法（借鉴 egg rebuild 的同余部分）：
 *   1. 维护一个 "dirty e-class" 队列（已合并但尚未传播同余的 e-class）
 *   2. 对每个 dirty e-class，遍历所有以该 e-class 成员为子节点的构造器
 *   3. 对每个构造器 f(...)，检查使用被合并节点的对应构造器 f'(...)
 *   4. 如果 f 和 f' 的所有对应子节点现在都在同一 e-class 中，
 *      则将 f 和 f' union 到同一个 e-class
 *   5. 新产生的 union 导致新的 dirty e-class，迭代直到队列为空
 *
 * @param[in,out] graph        约束图
 * @param[in,out] eclass_table E-Class 表
 * @param[in]     dirty_eclasses 本轮已合并的 e-class ID 列表
 * @param[in]     dirty_count     dirty e-class 数量
 * @return 新产生的 union 数量
 *
 * @note 借鉴 egg 的核心 insight：同余闭包是等式饱和收敛的加速器。
 *       没有同余闭包，需要显式声明 f(a)=f(b) 的传播规则——这在几何中
 *       意味着为每个构造器（midpoint, centroid, circumcenter, ...）
 *       分别写传播规则，属于重复劳动。
 */
int propagate_congruence(
    ConstraintGraph *graph,
    EClassTable *eclass_table,
    int *dirty_eclasses,
    int dirty_count);
```

### 3.4 同余闭包的几何构造器注册

并非所有 FuncBlock 都参与同余闭包。只有**构造器（Constructor）**才需要——构造器的输出随输入变化而唯一确定：

```c
/**
 * @brief 注册构造器的同余闭包行为
 *
 * 对于标记为 IS_CONSTRUCTOR 的 FuncBlock，
 * 当子节点等价时，自动推导输出节点的等价。
 *
 * 几何构造器示例：
 *   - midpoint(A, B)：中点构造器
 *   - centroid(A, B, C)：重心构造器
 *   - circumcenter(A, B, C)：外心构造器
 *   - segment(A, B)：线段构造器
 *   - circle_center_radius(O, R)：圆构造器
 *
 * 非构造器（不参与同余闭包）：
 *   - distance(A, B)：距离计算（输出是数值，不构造新几何实体）
 *   - area(T)：面积计算
 */
typedef struct {
    int func_block_id;              /**< FuncBlock 的节点 ID */
    char *name;                     /**< 构造器名称 */
    int arity;                      /**< 参数数量 */
    bool participates_in_congruence; /**< 是否参与同余闭包传播 */
} ConstructorRegistration;
```

---

## 4. 核心借鉴要点三：声明式规则语法与 .lvz 公理声明

### 4.1 egg 的规则语法

egg 使用 Rust 宏提供简洁的规则声明语法：

```rust
// egg 规则声明示例
rewrite!("commute-add"; "(+ ?a ?b)" => "(+ ?b ?a)")
rewrite!("assoc-add"; "(+ ?a (+ ?b ?c))" => "(+ (+ ?a ?b) ?c)")
rewrite!("mul-0"; "(* ?a 0)" => "0")
rewrite!("factor"; "(* ?a ?b) + (* ?a ?c)" => "(* ?a (+ ?b ?c))")
```

每条规则由三部分组成：
- **规则名**（字符串标识符）：用于调试和统计
- **LHS 模式**（带变量的 S 表达式）：搜索匹配的模式
- **RHS 模板**（带变量的 S 表达式）：构造替换结果的模板

### 4.2 Lv-00 的 .lvz 规则声明语法设计

借鉴 egg 的简洁语法，为 .lvz 公理包设计声明式规则格式：

```
// ============================================================
// Lv-00 公理包: triangle_centroid.lvz
// 使用 egg 风格的声明式规则语法
// ============================================================

@name "三角形重心公理"
@version "1.0.0"

// --- Sort 声明 ---
@sort Point
@sort Segment subsort_of Line
@sort Triangle subsort_of Region

// --- 构造器声明 ---
@op midpoint    : Point Point -> Point   [ctor]
@op centroid    : Point Point Point -> Point [ctor]
@op segment     : Point Point -> Segment [ctor]
@op triangle    : Point Point Point -> Triangle [ctor]

// --- 等式饱和规则（egg 风格 @rewrite 声明）---
// 语义：LHS 匹配时，将 RHS 构造加入同一 e-class，原结构保留

// 规则1: 中点展开
@rewrite [midpoint_expand] : priority(1)
    midpoint(A, B)
    =>
    point((A.x + B.x) / 2, (A.y + B.y) / 2)

// 规则2: 重心展开
@rewrite [centroid_expand] : priority(1)
    centroid(A, B, C)
    =>
    point((A.x + B.x + C.x) / 3, (A.y + B.y + C.y) / 3)

// 规则3: 三中线交汇 → 重心
@rewrite [recognize_centroid] : priority(5)
    concurrent(segment(A, midpoint(B, C)),
               segment(B, midpoint(C, A)),
               segment(C, midpoint(A, B)))
    & is_concurrent_point(G)
    =>
    G = centroid(A, B, C)

// 规则4: 重心分中线 2:1
@rewrite [centroid_ratio] : priority(3)
    is_on_segment(G, segment(A, midpoint(B, C)))
    & G = centroid(A, B, C)
    =>
    distance(A, G) = (2/3) * distance(A, midpoint(B, C))

// 规则5: 条件重写 —— 全等三角形的重心相等
@rewrite_cond [congruent_centroid] : priority(4)
    triangle_congruent(ABC, DEF)
    =>
    centroid(A, B, C) = centroid(D, E, F)
```

### 4.3 与现有 C API 的对比

| 维度 | 现有 C API | .lvz 声明式语法（egg 风格） |
|------|-----------|---------------------------|
| 可读性 | `rewrite_rule_create("midpoint", pattern, replacement, 1)` | `@rewrite [midpoint_expand] : priority(1) ... => ...` |
| 变量声明 | C 代码中手动创建 `RewritePattern` 节点 | 模式中自由变量（A, B, C）自动识别 |
| 条件规则 | 通过 `condition_func` 回调 | `@rewrite_cond` + `if` 条件表达式 |
| 规则管理 | `rewrite_rules_load_from_file()` + 手动调用 | .lvz 文件整体加载，自动编译为 RewriteRule |
| 调试 | 需要 gdb/printf | 规则名直接可读，支持按名查询规则 |
| 与 egg 生态的对标 | 无 | 可参考 egg 的规则库（Ruler 推导出的数千条规则） |

---

## 5. 核心借鉴要点四：egglog 约束传播与约束图增强

### 5.1 egglog 是什么

egglog 将 **Datalog（声明式逻辑编程）** 和 **e-graph** 结合在一起。在 egglog 中，用户不仅可以用 rewrite 规则声明项之间的等价，还可以用 Datalog 规则声明**关系约束**，这些约束会被 seminaive evaluation 自动计算到不动点。

```
; egglog 示例：声明关系和规则
(function Point (String) Point)
(function Segment (Point Point) Segment)
(function midpoint (Point Point) Point)

; 关系声明
(relation collinear (Point Point Point))
(relation between (Point Point Point))

; Datalog 规则（非等式，而是关系约束）
(rule ((collinear A B C) (between B A C))
      ((collinear B A C)))
(rule ((midpoint M A B))
      ((collinear A M B)))
```

### 5.2 映射到 Lv-00 的约束传播系统

egglog 的 Datalog 规则直接对应 Lv-00 的 **约束传播器（Constraint Propagator）**——当某些约束被满足时，自动推导新的约束：

| egglog 概念 | Lv-00 对应 | 数据流 |
|-----------|-----------|--------|
| `(relation R (T1 T2 ...))` | `ConstraintType` 枚举值 | 约束的类型标签 |
| `(rule ((R1 ...) (R2 ...)) ((R3 ...)))` | `ConstraintPropagationRule` | 约束传播规则 |
| `seminaive evaluation` | `propagation_run_loop()` | 传播循环 |
| `(merge a b)` (e-graph) | `constraint_graph_merge_nodes()` | 等价合并 |
| `(extract best)` | 从 e-class 中选出最优表示 | 等价类中选最简/最规约节点 |

### 5.3 约束传播规则设计

```c
/**
 * @brief 约束传播规则 —— 借鉴 egglog 的 Datalog rule
 *
 * 定义"前置约束 → 结论约束"的传播。
 * 当约束图中所有前置约束均满足时，自动添加结论约束。
 *
 * egglog 等价：
 *   (rule ((R1 A B C) (R2 C D)) ((R3 A B D)))
 *
 * Lv-00 使用：
 *   ConstraintPropagationRule {
 *     .antecedents = {INCIDENCE(M, AB), MIDPOINT(M, AB)},
 *     .consequent  = COLLINEAR(A, M, B)
 *   }
 */
typedef struct {
    char *rule_name;                          /**< 传播规则名称 */
    ConstraintType *antecedent_types;         /**< 前置约束类型数组 */
    int *antecedent_participant_pattern;      /**< 前置约束的参与节点绑定模式 */
    int antecedent_count;                     /**< 前置约束数量 */

    ConstraintType consequent_type;           /**< 结论约束类型 */
    int *consequent_participant_map;          /**< 结论约束的参与节点映射
                                                （哪些节点来自前置约束的哪些槽位） */
    int consequent_participant_count;

    bool is_bidirectional;                    /**< 是否为双向规则 */
} ConstraintPropagationRule;

/**
 * @brief 运行约束传播循环（egglog seminaive evaluation 等价）
 *
 * 借鉴 egglog 的 seminaive 求值策略：
 *   1. 维护一个"新推导事实"集合（delta 关系）
 *   2. 每轮只对涉及 delta 的规则求值
 *   3. 新产生的事实加入 delta 和主关系
 *   4. 循环直到 delta 为空（不动点）
 *
 * @return 新推导的约束数量
 */
int constraint_propagation_run(
    ConstraintGraph *graph,
    ConstraintPropagationRule *rules,
    int rule_count,
    int max_iterations);
```

### 5.4 约束传播示例

```
// 几何约束传播规则表
ConstraintPropagationRule geometry_propagation_rules[] = {
    // 规则1: 如果 M 是 AB 的中点，则 A, M, B 共线
    {
        .rule_name = "midpoint_collinear",
        .antecedents = {INCIDENCE(M, AB), MIDPOINT(M, AB)},
        .antecedent_count = 2,
        .consequent_type = COLLINEAR,
        .consequent_participant_map = {A, M, B},  // 绑定: M来自INCIDENCE[0], A/B来自MIDPOINT
    },

    // 规则2: 如果 P 是 AB 和 CD 的交点，且 AB=CD 为同一直线，则 P 有无数位置
    {
        .rule_name = "intersection_degenerate",
        .antecedents = {INTERSECTION(P, AB, CD), LINE_EQUAL(AB, CD)},
        .antecedent_count = 2,
        .consequent_type = UNDERDETERMINED,
        .consequent_participant_map = {P},
    },

    // 规则3: 如果三点共线且 B 在 A 和 C 之间，则 B 在线段 AC 上
    {
        .rule_name = "between_on_segment",
        .antecedents = {COLLINEAR(A, B, C), BETWEEN(B, A, C)},
        .antecedent_count = 2,
        .consequent_type = IS_ON_SEGMENT,
        .consequent_participant_map = {B, AC},
    },
};
```

---

## 6. Lv-00 映射方案：rewrite.h 的等式饱和引擎

### 6.1 核心类型扩展

现有 `rewrite.h` 需要扩展以下类型以支持等式饱和：

```c
/**
 * @brief 重写模式匹配结果 —— 扩展支持 e-class 匹配
 *
 * 在等式饱和模式下，匹配不仅搜索原始节点，也搜索 e-class 中的节点。
 * 如果一个 e-class 中有多个等价表示，VF2 匹配会尝试每个成员。
 */
typedef struct {
    RewriteMatch base;                          /**< 基础匹配信息 */

    /* e-class 相关 */
    int *matched_eclass_ids;                    /**< 每个匹配变量所属的 e-class ID */
    int *matched_member_ids;                    /**< 每个匹配变量在 e-class 中的具体成员 ID */

    /* 同余信息 */
    bool triggers_congruence;                   /**< 该匹配是否会触发同余闭包 */
    int *congruence_parent_ids;                 /**< 受同余影响的父节点 */
} RewriteMatchEclass;

/**
 * @brief 等式饱和统计信息
 */
typedef struct {
    int total_iterations;                       /**< 总迭代次数 */
    int total_matches_found;                    /**< 找到的匹配总数 */
    int total_unions_performed;                 /**< union 操作次数 */
    int total_congruence_propagations;          /**< 同余闭包传播次数 */
    int total_new_constraints_derived;          /**< 约束传播新推导数 */
    bool reached_fixedpoint;                    /**< 是否达到不动点 */
    uint64_t final_wl_hash;                     /**< 终止时的 WL 哈希 */
    double elapsed_seconds;                     /**< 耗时 */
} EgraphSaturationStats;
```

### 6.2 重写模式的语义变更

| 现有行为 | 等式饱和行为 |
|---------|-------------|
| 找到匹配后，用 RHS 替换 LHS 子图 | 找到匹配后，构造 RHS 子图，将 RHS 根节点 union 进 LHS 匹配的 e-class |
| 同一位置只能应用一条规则 | 同一位置可应用多条规则，结果共存于 e-class |
| 规则应用顺序影响最终结果 | 规则应用顺序不影响最终的可达等价集（汇流性由 e-graph 保证） |
| 一次 apply 后图结构改变 | 图结构只增不减（monotonic），e-class 不断丰富 |
| 需要人工设计规则优先级来保证汇流 | 秩序由不动点检测决定，规则优先级仅影响收敛速度 |

### 6.3 与现有函数的关系

```
rewrite.h 现有 API                          等式饱和扩展
────────────────────────────────────────────────────────────────
rewrite_rule_create()                 →  不变（规则创建方式相同）
find_rewrite_match()                  →  扩展搜索 e-class 成员
apply_rewrite()                       →  apply_rewrite_egraph()（union 入 e-class）
find_all_non_overlapping_matches()   →  find_all_egraph_matches()（允许匹配 e-class 成员）
rewrite_apply_all_matches()          →  rewrite_egraph_saturate()（饱和循环）
detect_rewrite_loop_wl()             →  detect_egraph_fixedpoint()（不动点检测）
rewrite_compute_wl_hash()            →  不变（仍用于不动点检测，扩展计算 e-class 哈希）
```

---

## 7. Lv-00 映射方案：constraint_graph.h 的 E-Class 增强

### 7.1 约束图节点类型的扩展

```c
/**
 * @brief 几何节点类型 —— 扩展 GEOM_E_CLASS
 */
typedef enum {
    // ... 现有类型 ...
    GEOM_POINT,
    GEOM_LINE_SEGMENT,
    GEOM_REGION,
    GEOM_FUNC_BLOCK,
    // ... 其他现有类型 ...

    GEOM_E_CLASS,           /**< 新类型：等价类容器节点
                               * 借鉴 egg 的 e-class。
                               * 该节点自身不包含几何数据，
                               * 而是持有一组等价成员节点的引用。
                               * 所有成员节点表示同一几何实体。
                               */
} GeomType;
```

### 7.2 EClassTable 与 ConstraintGraph 的交互

```
EClassTable API                              说明
────────────────────────────────────────────────────────────────
eclass_table_create(capacity)            →  创建 E-Class 表
eclass_table_init_from_graph(table, g)   →  从约束图初始化（每节点自成一类）
eclass_union(table, a_id, b_id)          →  合并两个节点到同一 e-class
eclass_find(table, node_id)              →  查找节点所属的 e-class ID
eclass_get_representative(table, eid)    →  获取 e-class 的代表节点
eclass_add_member(table, eid, node_id)   →  将新节点加入 e-class
eclass_get_all_members(table, eid)       →  获取 e-class 的所有成员
eclass_contains_node(table, eid, nid)    →  检查节点是否在 e-class 中
eclass_get_member_count(table, eid)      →  获取 e-class 成员数
eclass_table_destroy(table)              →  销毁 E-Class 表

// 高级操作
eclass_extract_best(table, eid, metric)  →  按度量（最简/规约最彻底）选择最优成员
eclass_compute_hash(table, eid)          →  计算 e-class 的 WL 哈希
eclass_get_constructors_using(table, eid)→  获取所有使用该 e-class 成员的构造器
```

### 7.3 E-Class 的可视化集成

在 Web GUI 和 ProofPanel 中，e-class 的展示：

| 视图 | e-class 展示方式 |
|------|-----------------|
| 约束图视图 | e-class 显示为虚线包围的节点组，带标签"e-class #N" |
| 证明步骤视图 | "发现等价：构造 A ≡ 构造 B（均属于 e-class #N）" |
| 详细信息面板 | 展开显示 e-class 的所有成员及其来源规则 |
| 调试视图 | 同余闭包传播的 DAG：显示哪些 union 触发了哪些新推导 |

---

## 8. 完整示例：三角形重心定理的等式饱和证明

### 8.1 问题描述

**已知**: 三角形 ABC，D = midpoint(B, C), E = midpoint(C, A), F = midpoint(A, B)（三边中点）；AD, BE, CF 为三条中线。

**证明**: 三条中线交于一点 G（重心），且 G 满足 centroid 坐标。

### 8.2 .lvz 公理包（输入）

```
// file: triangle_centroid_egraph.lvz
@name "三角形重心等式饱和证明"
@version "1.0.0"

@sort Point
@sort Segment subsort_of Line

@op midpoint  : Point Point -> Point   [ctor, congruence]
@op centroid  : Point Point Point -> Point [ctor, congruence]
@op segment   : Point Point -> Segment [ctor]
@op concurrent: Segment Segment Segment -> Bool

// --- 等式饱和规则 ---

// 中点展开
@rewrite [midpoint_expand] : priority(1)
    D = midpoint(B, C)
    =>
    D = point((B.x + C.x) / 2, (B.y + C.y) / 2)

// 重心展开
@rewrite [centroid_expand] : priority(1)
    G = centroid(A, B, C)
    =>
    G = point((A.x + B.x + C.x) / 3, (A.y + B.y + C.y) / 3)

// 重心识别：如果某点在三中线上，则该点是重心
@rewrite [recognize_centroid] : priority(5)
    concurrent(AD, BE, CF)
    & is_on(AD, G) & is_on(BE, G) & is_on(CF, G)
    =>
    G = centroid(A, B, C)
```

### 8.3 等式饱和执行过程

```
初始状态（约束图）:
  e-class 0: {A}    (顶点 A)
  e-class 1: {B}    (顶点 B)
  e-class 2: {C}    (顶点 C)
  e-class 3: {D = midpoint(B, C)}          // 标记为构造器
  e-class 4: {E = midpoint(C, A)}
  e-class 5: {F = midpoint(A, B)}
  e-class 6: {AD = segment(A, D)}
  e-class 7: {BE = segment(B, E)}
  e-class 8: {CF = segment(C, F)}
  constraint: concurrent(AD, BE, CF)，交点 G 在三条中线上

=== 迭代 1 ===
  search: 匹配规则 midpoint_expand
    [match 1] D = midpoint(B, C) 命中 → 展开 D
    [match 2] E = midpoint(C, A) 命中 → 展开 E
    [match 3] F = midpoint(A, B) 命中 → 展开 F
  apply:
    union: e-class 3 加入 coord(B, C)/2
    union: e-class 4 加入 coord(C, A)/2
    union: e-class 5 加入 coord(A, B)/2
  rebuild:
    同余闭包: D 的新坐标触发 segment(A, D) 重建
    同余闭包: E 的新坐标触发 segment(B, E) 重建
    同余闭包: F 的新坐标触发 segment(C, F) 重建

=== 迭代 2 ===
  search: 匹配规则 recognize_centroid
    [match] concurrent(AD, BE, CF) + G 在三中线上 → G = centroid(A, B, C)
  apply:
    创建新节点 centroid(A, B, C) → 放入 G 所属的 e-class
    union: G 的 e-class 加入 centroid(A, B, C)
  rebuild:
    同余闭包: G ∈ {原始交点, centroid(A, B, C)}

=== 迭代 3 ===
  search: 匹配规则 centroid_expand
    [match] G = centroid(A, B, C) 命中 → 展开 G 的坐标
  apply:
    union: G 的 e-class 加入 coord(A, B, C)/3
  rebuild: 无新增同余

=== 迭代 4 ===
  search: 无新匹配 → WL 哈希与迭代 3 相同
  → REACHED FIXED POINT

最终 E-Class 状态:
  G 的 e-class = {
    original_intersection_point,          // 原始交点（约束定义）
    centroid(A, B, C),                     // 重心构造器
    point((A.x+B.x+C.x)/3, (A.y+B.y+C.y)/3) // 重心坐标
  }
  三者等价 ✓ —— 证明完成
```

### 8.4 与传统重写的对比

| 维度 | 传统重写（破坏性） | 等式饱和（egg 风格） |
|------|-----------------|---------------------|
| 步骤 1 | 应用 midpoint_expand 替换所有中点 | 将展开结果加入 e-class（三节点共存） |
| 步骤 2 | 图已改变，原始中点节点丢失 | 原始 midpoint 节点仍然在 e-class 中 |
| 步骤 3 | 应用 recognize_centroid | recognize_centroid 同时匹配原始和展开的表示 |
| 步骤 4 | 如顺序错误（先 centroid 展开再 midpoint），可能需要回溯 | 顺序无关——所有等价表示在饱和后都可达 |
| 可回溯性 | 需显式保存图快照 | 自然可回溯（e-class 保存所有历史表示） |
| 规则冲突 | 必须设计优先级 | 优先级仅影响收敛速度，不影响最终结果 |

---

## 9. 总结映射表

### 9.1 egg → Lv-00 核心概念映射

| egg 概念 | egg 内部结构 | Lv-00 映射结构 | 文件 |
|---------|------------|---------------|------|
| `EGraph<L>` | e-class 集合 + e-node 存储 | `ConstraintGraph` + `EClassTable` | `constraint_graph.h`, `eclass.h`（新增） |
| `EClass` | 包含等价节点的集合 | `GEOM_E_CLASS` 节点类型 + `EClassData` | `constraint_graph.h` |
| `ENode` | `Symbol(L, children: &[Id])` | `FuncBlock` 节点（构造器） | `func_block.h` |
| `union(id1, id2)` | 合并两个 e-class | `constraint_graph_merge_nodes()` / `eclass_union()` | `constraint_graph.h`, `eclass.h` |
| `rebuild()` | 传播同余闭包 + 去重 | `propagate_congruence()` + `rebuild_hashes()` | `rewrite.h` |
| `rewrite!("name"; LHS => RHS)` | 声明式重写规则 | `@rewrite [name] : LHS => RHS` in .lvz | `lvz_parser.c` |
| `Runner` | 迭代调度器 | `rewrite_egraph_saturate()` | `rewrite.h` |
| `Searcher` | LHS 模式匹配 | `find_rewrite_match()` 扩展 e-class 搜索 | `rewrite.h` |
| `Applier` | RHS 子图构建 | 构造 RHS 子图后 `eclass_union()` | `rewrite.h` |
| `Extractor` | 从 e-class 选最优项 | `eclass_extract_best()` | `eclass.h` |

### 9.2 egglog → Lv-00 约束传播映射

| egglog 概念 | egglog 语法 | Lv-00 映射 | 文件 |
|-----------|-----------|-----------|------|
| `(relation R (T1 T2 ...))` | 关系声明 | `ConstraintType` 枚举值 | `constraint_graph.h` |
| `(rule ((R1 ...) (R2 ...)) ((R3 ...)))` | Datalog 规则 | `ConstraintPropagationRule` | `constraint_graph.h` |
| seminaive evaluation | 内置 | `propagation_run_loop()` | `constraint_propagation.c`（新增） |
| `(merge a b)` | 等价声明 | `constraint_graph_merge_nodes()` | `constraint_graph.h` |
| `(extract best)` | 最优选择 | `eclass_extract_best()` | `eclass.h`（新增） |
| `(function F (T) U)` | 函数声明 | `@op F : T -> U [ctor, congruence]` | .lvz 公理包 |

### 9.3 生产级案例参考

| 项目 | 使用 egg 的方式 | 对 Lv-00 的启示 |
|------|---------------|----------------|
| **Herbie** | 浮点表达式精度优化，数千条重写规则在 e-graph 上饱和 | 几何表达式（坐标公式）同样可用等式饱和优化 |
| **Cranelift** | WASM 代码生成，在 e-graph 上做 instruction selection 优化 | 几何构造选择可类比 instruction selection——从等价构造中选最优 |
| **Ruler** | 自动从 e-graph 中推导新规则（规则挖掘） | 可自动发现几何引理——从已知公理集通过等式饱和推导新等价关系 |
| **Szalinski** | 用 e-graph 做 CAD 模型的反向工程和优化 | 直接对标 CAD 领域——Lv-00 的几何约束图天然适用 e-graph |

### 9.4 文件依赖关系

```
.lvz 公理包
    │
    ├── lvz_parser.c              (扩展支持 @rewrite/@rewrite_cond 声明)
    │   └── rewrite.h             (编译为 RewriteRule)
    │
    ├── eclass.h                   (新增：EClassTable 核心数据结构)
    │   ├── constraint_graph.h    (GEOM_E_CLASS 节点类型 + EClassData)
    │   └── func_block.h          (构造器注册表)
    │
    ├── rewrite.h                  (扩展：等式饱和 API)
    │   ├── rewrite_egraph_saturate()
    │   ├── propagate_congruence()
    │   └── EgraphSaturationStats
    │
    ├── constraint_propagation.c   (新增：egglog 风格的约束传播)
    │   ├── ConstraintPropagationRule
    │   └── propagation_run_loop()
    │
    └── proof.h                    (集成)
        └── PROOF_STRATEGY_EGRAPH_SATURATION (新证明策略)
```

---

## 附录 A：egg 核心论文与参考

| 论文/资源 | 说明 |
|----------|------|
| **POPL 2021: "egg: Fast and Extensible Equality Saturation"** | egg 的正式论文，描述核心算法 |
| **"Better Together: Unifying Datalog and Equality Saturation" (2023)** | egglog 论文，Datalog+e-graph 结合 |
| **egg 源码 (github.com/egraphs-good/egg)** | Rust 实现，约 15k 行代码 |
| **egglog 源码 (github.com/egraphs-good/egglog)** | Rust 实现，Datalog+e-graph |
| **Ruler (github.com/uwplse/ruler)** | 自动规则推导工具（使用 egg） |
| **Cranelift e-graph 后端** | production 使用案例 |

---

## 附录 B：与其他重写系统（Maude）的互补关系

| 维度 | Maude 借鉴点（maude_rewriting_semantics.md） | egg 借鉴点（本文档） |
|------|------------------------------------------|---------------------|
| **重写语义** | 排序逻辑 (sort/op/eq/rl)，破坏性重写 | 等式饱和，非破坏性重写 |
| **策略控制** | 策略组合子 (orelse/sequence/normalize) | 迭代饱和 + 不动点检测 |
| **搜索** | Search 命令的前向/反向重写路径搜索 | 同余闭包自动传播等价 |
| **规则语法** | Maude 兼容的 `@rl [name] : LHS => RHS` | egg 风格的 `@rewrite [name] : LHS => RHS` |
| **约束建模** | 等式和规则分离 | egglog Datalog 规则驱动约束传播 |
| **汇流性** | 依赖策略保证 | e-graph 结构自身保证汇流 |
| **在 Lv-00 中的角色** | .lvz 的 sort/op/eq/rl 基础建模 + 策略控制 + 搜索 | rewrite.h 的等式饱和引擎 + 约束传播 |
| **互补性** | 声明式规则声明 + 策略驱动重写 + 反向搜索 | 非破坏性等价发现 + 构造一致性 + 约束传播 |

---

> **文档结束**
> 本文档详述了 egg e-graph/等式饱和框架如何映射到 Lv-00 的 rewrite.h 和 constraint_graph.h——通过引入 GEOM_E_CLASS 节点类型、EClassTable、等式饱和循环和 egglog 风格的约束传播，实现非破坏性重写和几何构造的同余闭包自动推导。核心结论：等式饱和解决了传统破坏性重写的"先应用哪条规则"问题；同余闭包消除了手动编写等价传播规则的重复劳动（每个构造器自动享有 a=b → f(a)=f(b) 推导）；约束传播系统将 egglog 的 Datalog 规则映射到几何约束的自动推导网络。

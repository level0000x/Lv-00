# Lv-00 参考落地设计文档：Maude 重写逻辑语义

> **版本**: 1.0.0  
> **日期**: 2026-05-24  
> **参考**: Maude (github.com/maude-team/maude) —— 高性能重写逻辑引擎  
> **目标**: 将 Maude 的重写逻辑三要素、策略语言、Search 反向推理能力映射到 Lv-00 重写引擎

---

## 目录

1. [Maude 项目概述与 Lv-00 借鉴动机](#1-maude-项目概述与-lv-00-借鉴动机)
2. [重写逻辑三要素：sort/op/eq/rl 与 .lvz 公理包映射](#2-重写逻辑三要素sortopeqrl-与-lvz-公理包映射)
3. [策略语言集成设计](#3-策略语言集成设计)
4. [Search 反向推理命令与 Lv-00 证明搜索的映射](#4-search-反向推理命令与-lv-00-证明搜索的映射)
5. [编译流水线：.lvz 规则 → rewrite.h 内部结构](#5-编译流水线lvz-规则--rewriteh-内部结构)
6. [完整示例：三角形中线重心定理的重写证明](#6-完整示例三角形中线重心定理的重写证明)
7. [关键数据结构对照表](#7-关键数据结构对照表)

---

## 1. Maude 项目概述与 Lv-00 借鉴动机

### 1.1 Maude 的核心能力

Maude 是一个基于成员等式逻辑（membership equational logic）和重写逻辑（rewriting logic）的高性能规范语言与引擎。其核心能力包括：

| 能力 | 说明 | 在 Maude 中的载体 |
|------|------|-------------------|
| **类型层** | sort/subsort 层次定义项的种类和子类型关系 | `sort Nat .` / `subsort Nat < Int .` |
| **操作层** | op 定义函数符号及其类型签章 | `op _+_ : Nat Nat -> Nat .` |
| **等式层** | eq (equation) 定义项之间的等价关系 | `eq N + 0 = N .` |
| **规则层** | rl (rule) 定义项之间的重写关系 | `rl [swap] : X Y => Y X .` |
| **策略层** | 控制规则应用顺序和条件 | `strat` 模块 |
| **搜索层** | Search 命令沿重写路径反向搜索满足条件的状态 | `search [1] init =>* X:State s.t. ...` |

### 1.2 Lv-00 借鉴动机

Lv-00 的重写引擎（`rewrite.h`）已经具备了 VF2 子图同构匹配、WL 图核哈希循环检测、规则热加载/卸载等基础设施，但在以下方面仍需借鉴 Maude 的设计：

| 借鉴方向 | Maude 特性 | Lv-00 现有基础 | 差距与目标 |
|----------|-----------|---------------|-----------|
| **声明式规则建模** | sort/op/eq/rl 四层分离 | RewritePattern / RewriteReplacement | 缺失 sort 层次和 eq 等式层的显式建模 |
| **策略化重写控制** | `strat` 模块 + 策略组合子 | 单条规则逐一应用 (`apply_rewrite`) | 缺失策略组合子（seq/orelse/normalize） |
| **反向搜索** | `search [n] init =>* goal` | `find_rewrite_match` 前向匹配 | 缺失"给定目标子图，反向搜索重写路径" |

本文档聚焦于将 Maude 的三个核心能力映射到 Lv-00 体系：**公理包的 sort/op/eq/rl 建模 → .lvz 格式扩展**、**策略语言 → rewrite.h 策略 API**、**Search 搜索 → 证明搜索引擎**。

### 1.3 总体架构对照

```
Maude                          Lv-00
────────────────────────────────────────────────
fmod NAT { ... }               .lvz 公理包文件 + constraint_graph.h
  sort Nat .                   → GeomType 枚举 + TypeRegion
  op s_ : Nat -> Nat .         → FuncBlock (单入单出)
  eq N + 0 = N .               → RewriteRule (等式替代)
  rl [swap] : X Y => Y X .    → RewriteRule (含 RewritePattern+RewriteReplacement)

strat S { ... }                rewrite.h 策略 API (新增)
  apply(R1) ; apply(R2)       → rewrite_strategy_apply()
  orelse(S1, S2)              → 策略组合子

search init =>* goal .         rewrite.h 证明搜索 API (新增)
                               → rewrite_search_backward()
```

---

## 2. 重写逻辑三要素：sort/op/eq/rl 与 .lvz 公理包映射

### 2.1 sort —— 类型/排序层次

Maude 的 sort 定义了项的"类型空间"。在几何重写语境下，sort 对应于几何实体的类型分类：

```
--- Maude ---
sort Point Line Circle Triangle Region .
subsort Point < GeomEntity .
subsort Line < GeomEntity .
subsort Triangle < Region .
```

**Lv-00 映射**：sort 层次直接映射到 Lv-00 已有的类型系统。

| Maude 概念 | Lv-00 映射 | 具体结构 |
|-----------|-----------|---------|
| `sort` 声明 | `GEOM_POINT` / `GEOM_LINE_SEGMENT` / `GEOM_REGION` 等 | `GeomType` 枚举 (`constraint_graph.h`) |
| `subsort` 关系 | `TypeRegion` 中的 `subtypes` 数组 | `type_system.h` 的 `type_region_get_subtypes()` |
| 公理包 (.lvz) 中的 sort 声明 | `.lvz` 文件中的 `@sort` 指令 | 类似 `@sort Triangle subsort_of Region` |

**`.lvz` 格式扩展示例**——在公理包中声明 sort 层次：

```
// ============================================================
// Lv-00 公理包: euclidean_geometry.lvz
// 扩展语法: @sort 声明几何实体的类型层次
// ============================================================

@sort Point
@sort Line  subsort_of LinearEntity
@sort Segment subsort_of Line
@sort Circle
@sort Triangle subsort_of Region
@sort Quadrilateral subsort_of Region
```

### 2.2 op —— 操作符/构造器声明

Maude 的 op 定义了给定签章的函数符号。在几何语境中，op 对应于构造操作：

```
--- Maude ---
op midpoint : Point Point -> Point .
op segment  : Point Point -> Segment .
op circumcenter : Point Point Point -> Point .
op distance : Point Point -> Rat .
```

**Lv-00 映射**：op 映射到 FuncBlock。

| Maude 概念 | Lv-00 映射 | 具体结构 |
|-----------|-----------|---------|
| `op name : Args -> Ret` | `FuncBlock` 的输入/输出端口 | `func_block.h` 的 `FuncBlock` |
| 操作符重载（同一名称、不同签章） | 不同 FuncBlock 实例，按端口类型区分 | `func_block_resolve_overload()` |
| 操作符的结构公理（ctor 属性） | 构造器 FuncBlock —— 不可被等式归约 | `FuncBlock.flags & FB_FLAG_CONSTRUCTOR` |
| 操作符的结合性/交换性 | 约束图上的交换律/结合律约束 | `Constraint.type == COMMUTATIVITY` / `ASSOCIATIVITY` |

**`.lvz` 格式扩展示例**——在公理包中声明 op：

```
// --- 构造器操作符（不参与等式归约）---
@op midpoint  : Point Point -> Point  [ctor]
@op segment   : Point Point -> Segment [ctor]
@op circle_center_radius : Point Point -> Circle [ctor]

// --- 普通操作符 ---
@op distance  : Point Point -> Number
@op area      : Triangle -> Number
@op is_right_triangle : Triangle -> Bool

// --- 结合性/交换性属性 ---
@op _+_ : Number Number -> Number [assoc comm]
```

### 2.3 eq —— 等式/公理声明

Maude 的 eq 定义项之间的等价关系，Lv-00 将其映射为归约规则：

```
--- Maude ---
eq midpoint(P1, P2) = (P1.x + P2.x) / 2, (P1.y + P2.y) / 2 .
eq distance(P1, P2) = sqrt((P1.x - P2.x)^2 + (P1.y - P2.y)^2) .
```

在 Lv-00 中，eq 存在两种映射策略：

**策略 A — 等式替代规则（归约方向固定）**：将 eq 映射为单向 `RewriteRule`，左侧模式匹配项并在图中替换为右侧结构。这是最直接的映射，适合简化/归一化归约：

```c
// eq midpoint(P1, P2) = M s.t. M.x = (P1.x+P2.x)/2 ∧ M.y = (P1.y+P2.y)/2
// 映射为：找到中点模式 → 用计算好的坐标节点替换
RewriteRule *eq_midpoint = rewrite_rule_create(
    "eq:midpoint",
    /* pattern */ pattern_from_funcblock("midpoint"),
    /* replacement */ replacement_compute_coords(),
    /* measure */ 1
);
```

**策略 B — 双重重写（双向归约）**：对于对称等式（如交换律），创建互为逆向的一对规则：

```
// .lvz 中等式声明
@eq midpoint(P1, P2) <-> point((P1.x+P2.x)/2, (P1.y+P2.y)/2)
// 此标记表示等式可双向使用：
//   正向: midpoint(P1, P2) -> point(...)
//   逆向: point(...) -> midpoint(P1, P2)  (仅当 ... 满足中点条件时)

// 编译后产生两条 RewriteRule：
//   - eq_midpoint_fwd  (向前归约，简化)
//   - eq_midpoint_rev  (向后展开，识别)
```

**`.lvz` 格式扩展示例**——等式声明：

```
// --- 等式（公理）声明 ---
// 单向归约   eq <名字> : <左> = <右> .
@eq eq_midpoint : midpoint(P1, P2) = point((P1.x+P2.x)/2, (P1.y+P2.y)/2)

// 双向等价   @eqv <名字> : <左> <-> <右> .
@eqv eqv_distance_sq : distance_sq(P1, P2) <-> (P1.x-P2.x)^2 + (P1.y-P2.y)^2

// 条件等式   @ceq <名字> : <左> = <右> if <条件> .
@ceq ceq_right_triangle : area(T) = (leg1(T)*leg2(T))/2
    if is_right_triangle(T)
```

### 2.4 rl —— 重写规则声明

Maude 的 rl 定义非对称的状态变换规则。在 Lv-00 几何语境中，rl 对应于"结构简化/归一化/展开"的图重写规则：

```
--- Maude ---
rl [simplify_triangle] :
    triangle(P1, P2, P3) => new_label("△") [P1, P2, P3] .
```

**Lv-00 映射**：rl 映射为 Lv-00 原生 `RewriteRule`（已实现），.lvz 格式采用 Maude 兼容语法：

| Maude 概念 | Lv-00 映射 |
|-----------|-----------|
| `rl [name] : LHS => RHS .` | `RewriteRule` 的 `pattern`(LHS) + `replacement`(RHS) |
| 模式变量 `P:Point` | `RewritePattern.variable_node_ids` |
| 规则标签 `[name]` | `RewriteRule.name` |
| 条件规则 `crl [name] : LHS => RHS if COND .` | `RewriteRule.condition_func` + `condition_data` |

**`.lvz` 格式扩展示例**——重写规则声明：

```
// --- 重写规则声明 ---

// 基础重写: 识别中点模式，用计算结果替换
@rl [recognize_midpoint] :
    incident(M, AB) & midpoint(M, AB)
    =>
    M = Point((A.x+B.x)/2, (A.y+B.y)/2)

// 带优先级: 指定归约度量
@rl [simplify_collinear] : priority(2)
    collinear(A, B, C) & on_segment(B, A, C)
    =>
    between(A, B, C)

// 条件重写: 仅当条件满足时应用
@crl [right_triangle_area] :
    triangle(A, B, C)
    =>
    triangle_with_area(A, B, C, (leg1*leg2)/2)
    if is_right_angle(A, B, C)

// 归一化重写: 将线段方向统一化
@rl [normalize_segment] : priority(5)
    segment(A, B) & A.x > B.x
    =>
    segment(B, A)
```

### 2.5 .lvz 公理包完整文件格式

综合以上，Lv-00 的 `.lvz` 公理包文件格式扩展为包含 Maude 四要素的完整声明：

```
// ============================================================
// Lv-00 公理包: euclidean_geometry.lvz
// 兼容 Maude 风格的 sort/op/eq/rl 声明
// ============================================================

@name "欧几里得几何公理包"
@version "1.0.0"
@description "包含点、线、圆、三角形的基本公理和重写规则"

// ========== 1. Sort/Types 声明 ==========
@sort Point
@sort Line      subsort_of LinearEntity
@sort Segment   subsort_of Line
@sort Circle
@sort Triangle  subsort_of Region
@sort Number

// ========== 2. Op/构造器 声明 ==========
@op midpoint       : Point Point -> Point              [ctor]
@op segment        : Point Point -> Segment            [ctor]
@op circle_center_radius : Point Point -> Circle       [ctor]
@op triangle       : Point Point Point -> Triangle     [ctor]
@op distance       : Point Point -> Number
@op distance_sq    : Point Point -> Number
@op area           : Triangle -> Number
@op is_collinear   : Point Point Point -> Bool

// ========== 3. Eq/等式 声明 ==========
@eq eq_midpoint    : midpoint(P1, P2) =
    point((P1.x+P2.x)/2, (P1.y+P2.y)/2)

@eqv eqv_distance_sq : distance_sq(P1, P2) <->
    (P1.x - P2.x)^2 + (P1.y - P2.y)^2

@ceq ceq_right_triangle_area : area(T) = (leg1(T)*leg2(T))/2
    if is_right_triangle(T)

// ========== 4. Rl/重写规则 声明 ==========
@rl [recognize_midpoint] : priority(3)
    incident(M, AB) & midpoint(M, AB)
    =>
    M = computed_point((A.x+B.x)/2, (A.y+B.y)/2)

@rl [simplify_collinear] : priority(2)
    collinear(A, B, C) & on_segment(B, A, C)
    =>
    between(A, B, C)

@crl [recognize_centroid] : priority(5)
    medians(M1, M2, M3) & concurrent(M1, M2, M3, G)
    =>
    G = centroid(triangle_vertex_1, triangle_vertex_2, triangle_vertex_3)
    if concurrent_at_point(M1, M2, M3, G)
```

---

## 3. 策略语言集成设计

### 3.1 Maude 策略语言概述

Maude 的策略语言允许用户声明式地控制规则的应用顺序。核心策略组合子包括：

| 策略组合子 | 语义 | Maude 语法 |
|-----------|------|-----------|
| `idle` | 不做任何操作 | `idle` |
| `fail` | 策略失败 | `fail` |
| `apply(R)` | 应用规则 R 一次 | `apply(R)` |
| `match(P)` | 匹配模式 P | `match(P)` |
| `S1 ; S2` | 顺序组合 | `S1 ; S2` |
| `S1 | S2` | 选择组合（orelse） | `S1 | S2` |
| `S *` | 重复执行直到失败 | `S *` |
| `S !` | 归一化（重复直到停止） | `S !` |
| `test(C)` | 测试条件 | `test(C)` |
| `try(S)` | 尝试 S，失败则 idle | `try(S)` |

### 3.2 Lv-00 策略类型与实现设计

在 Lv-00 中，策略需要对图重写进行声明式控制。设计一个 `RewriteStrategy` 枚举和相应的评估函数：

```c
/**
 * @brief 重写策略节点类型
 *
 * 借鉴 Maude 策略语言的核心组合子。
 * 每个策略节点是一个紧凑的树结构，
 * 支持策略的组合、选择和迭代。
 */
typedef enum {
    REWRITE_STRATEGY_IDLE,          /**< idle：不操作 */
    REWRITE_STRATEGY_FAIL,          /**< fail：立即失败 */
    REWRITE_STRATEGY_APPLY_RULE,    /**< apply(R)：应用单条规则 */
    REWRITE_STRATEGY_MATCH_PATTERN, /**< match(P)：仅匹配不替换 */
    REWRITE_STRATEGY_TEST_COND,     /**< test(C)：条件测试 */
    REWRITE_STRATEGY_SEQUENCE,      /**< S1 ; S2：顺序执行 */
    REWRITE_STRATEGY_ORELSE,        /**< S1 | S2：选择执行 */
    REWRITE_STRATEGY_REPEAT,        /**< S *：重复至失败 */
    REWRITE_STRATEGY_NORMALIZE,     /**< S !：归一化至不动点 */
    REWRITE_STRATEGY_TRY            /**< try(S)：尝试，失败则 idle */
} RewriteStrategyKind;

/**
 * @brief 重写策略节点
 *
 * 树结构，表示一个策略表达式。
 * 内部节点（SEQUENCE / ORELSE）持有子策略。
 * 叶节点（APPLY_RULE / MATCH_PATTERN / TEST_COND）持有规则引用。
 */
typedef struct RewriteStrategy {
    RewriteStrategyKind kind;       /**< 策略节点类型 */
    char *name;                      /**< 策略名称（可选） */

    /* 叶节点数据（根据 kind 使用不同字段） */
    RewriteRule *rule_ref;           /**< APPLY_RULE 时的目标规则 */
    RewritePattern *pattern_ref;     /**< MATCH_PATTERN 时的目标模式 */
    RewritePrecondition test_func;   /**< TEST_COND 时的测试函数 */

    /* 内部节点数据 */
    struct RewriteStrategy *left;    /**< SEQUENCE/ORELSE 的左子策略 */
    struct RewriteStrategy *right;   /**< SEQUENCE/ORELSE 的右子策略 */

    /* REPEAT / NORMALIZE / TRY 的单子策略（复用 left 指针） */

    /* 统计 */
    int apply_count;                 /**< 已成功应用次数 */
    int step_limit;                  /**< 最大步数限制（0 = 无限制） */
} RewriteStrategy;
```

### 3.3 rewrite_strategy_apply() 函数设计

`rewrite_strategy_apply()` 是策略驱动的核心入口。它在约束图上按策略表达式的语义递归执行重写：

```
rewrite_strategy_apply(graph, strategy) 的语义：
  IDLE          → 不做任何修改，返回 REWRITE_OK
  FAIL          → 返回 REWRITE_NO_MATCH
  APPLY_RULE(r) → 在图中查找 r 的匹配并应用
  MATCH_PAT(p)  → 在图中查找 p 的匹配（不替换）
  TEST_COND(c)  → 调用 c(graph)，返回 OK 或 NO_MATCH
  SEQUENCE(s1,s2)→ 先执行 s1，若成功则继续 s2
  ORELSE(s1,s2) → 先执行 s1，若失败则执行 s2
  REPEAT(s)     → 反复执行 s 直到 s 返回 NO_MATCH
  NORMALIZE(s)  → 反复执行 s 直到不动点（图哈希不变）
  TRY(s)        → 执行 s，若失败返回 REWRITE_OK
```

**函数声明**（追加到 rewrite.h）：

```c
/**
 * @brief 按策略表达式在约束图上执行重写
 *
 * 借鉴 Maude 策略语言，支持顺序组合（;）、选择组合（|）、
 * 重复（*）、归一化（!）等策略组合子。
 *
 * 策略树的评估采用递归下降方式，每步均支持图快照和回滚。
 *
 * @param[in,out] graph    约束图（会被策略修改）
 * @param[in]     strategy 重写策略表达式（树结构）
 * @param[in]     step_limit 最大总步数限制（0 = 无限制）
 * @return 重写状态：REWRITE_OK / REWRITE_APPLIED / REWRITE_NO_MATCH / REWRITE_TERMINATED
 *
 * @note 借鉴 Maude 的 strategy 模块设计理念。
 *       SEQUENCE 实现"先A后B"的顺序重写，
 *       ORELSE 实现"尝试A，失败则尝试B"的选择重写，
 *       NORMALIZE 在每一步后检查 WL 哈希判定不动点。
 */
RewriteStatus rewrite_strategy_apply(
    ConstraintGraph *graph,
    RewriteStrategy *strategy,
    int step_limit);
```

### 3.4 策略的组合构造 API

为简化策略树的构造，提供便捷的组合子宏/函数：

```c
/* 策略构造器（便捷函数，在 strategy.c 中实现） */

/* 创建基本策略节点 */
RewriteStrategy *rewrite_strategy_create_idle(void);
RewriteStrategy *rewrite_strategy_create_fail(void);
RewriteStrategy *rewrite_strategy_create_apply(RewriteRule *rule);
RewriteStrategy *rewrite_strategy_create_match(RewritePattern *pattern);
RewriteStrategy *rewrite_strategy_create_test(RewritePrecondition cond, void *data);

/* 创建组合策略节点 */
RewriteStrategy *rewrite_strategy_sequence(RewriteStrategy *s1, RewriteStrategy *s2);
RewriteStrategy *rewrite_strategy_orelse(RewriteStrategy *s1, RewriteStrategy *s2);
RewriteStrategy *rewrite_strategy_repeat(RewriteStrategy *s);
RewriteStrategy *rewrite_strategy_normalize(RewriteStrategy *s);
RewriteStrategy *rewrite_strategy_try(RewriteStrategy *s);

/* 策略评估 */
RewriteStatus rewrite_strategy_apply(ConstraintGraph *graph, RewriteStrategy *strategy, int step_limit);

/* 策略销毁 */
void rewrite_strategy_destroy(RewriteStrategy *strategy);
```

### 3.5 策略使用示例

```
// 示例：归一化重写策略 —— 先将中点展开，再简化共线关系
// 等价于 Maude: apply(recognize_midpoint) ! ; apply(simplify_collinear) !

RewriteStrategy *s_midpoint  = rewrite_strategy_create_apply(rule_midpoint);
RewriteStrategy *s_collinear = rewrite_strategy_create_apply(rule_simplify_collinear);
RewriteStrategy *s_normalize_midpoints  = rewrite_strategy_normalize(s_midpoint);
RewriteStrategy *s_normalize_collinears = rewrite_strategy_normalize(s_collinear);
RewriteStrategy *s_pipeline = rewrite_strategy_sequence(s_normalize_midpoints, s_normalize_collinears);

// 执行
RewriteStatus st = rewrite_strategy_apply(graph, s_pipeline, 1000);
```

---

## 4. Search 反向推理命令与 Lv-00 证明搜索的映射

### 4.1 Maude Search 命令语义

```
--- Maude Search ---
search [1] in TRIANGLE : init_state =>* final_pattern .
search [N] in GEOMETRY : start_config
    =>* X:Configuration
    such that satisfies_goal(X) .
```

Search 从某个初始状态出发，反复应用重写规则（`=>*` 表示零步或多步），寻找满足终止条件的状态。这是**前向搜索**（从 init 到 goal）。

在证明场景中，我们通常需要**反向搜索**——从证明目标出发，通过逆向重写（展开定义、引入引理）归约到已知公理：

```
search backward: goal <=* axioms_and_givens .
```

### 4.2 rewrite_search_backward() 设计

`rewrite_search_backward()` 实现反向证明搜索：给定一个证明目标（约束图的部分子图），尝试找到一条重写路径将其归约到已知公理或给定条件。

**算法概述**：

```
rewrite_search_backward(graph, goal_pattern, rules, depth_limit):
  1. 如果 goal_pattern 直接匹配已知公理（在 axiom_graph 中），返回空路径（成功）
  2. 对每条规则 r（按优先级降序）：
     a. 构造 r 的逆向规则 r_rev（交换 pattern 和 replacement）
     b. 在图中查找 r_rev 的匹配 m
     c. 如果找到匹配，创建快照，应用 r_rev，得到新图 graph'
     d. 递归调用 rewrite_search_backward(graph', goal_pattern, rules, depth_limit-1)
     e. 如果递归成功，将当前步骤(r, m)加入路径，返回路径
  3. 如果所有规则都未能推进，返回失败
```

**函数声明**（追加到 rewrite.h）：

```c
/**
 * @brief 反向证明搜索 —— 从目标向公理归约
 *
 * 借鉴 Maude 的 `search` 命令思想，但方向相反。
 * 从证明目标（goal subgraph）出发，通过规则的逆向应用，
 * 搜索一条重写路径将目标归约到已知公理或给定条件。
 *
 * 搜索策略：
 *   - 广度优先（BFS）或深度优先（DFS），由 search_mode 控制
 *   - 每次尝试规则的逆向应用（交换 RewritePattern 与 RewriteReplacement）
 *   - 使用 WL 图哈希避免重复状态
 *   - depth_limit 控制最大搜索深度
 *
 * @param[in]     graph         当前约束图（包含所有给定条件和辅助构造）
 * @param[in]     goal_pattern  证明目标模式（需要证明成立的子图约束）
 * @param[in]     rules         可用的重写规则数组（用于逆向应用）
 * @param[in]     rule_count    规则数量
 * @param[in]     axiom_hashes  已知公理图的 WL 哈希集合（作为搜索终点）
 * @param[in]     axiom_count   公理哈希数量
 * @param[in]     depth_limit   最大搜索深度（0 = 无限制）
 * @param[in]     search_mode   搜索模式：0=BFS 广度优先, 1=DFS 深度优先
 * @param[out]    out_path      输出：应用规则名称的路径数组（调用者释放）
 * @param[out]    out_path_len  输出：路径长度
 * @return REWRITE_OK (找到路径), REWRITE_NO_MATCH (深度耗尽未找到), REWRITE_TERMINATED (人为终止)
 *
 * @note 借鉴 Maude 的 `search` 机制和 Lv-00 现有 ProofNavigator 的多策略引擎。
 *       此函数可被证明引擎作为 PROOF_STRATEGY_REWRITE_SEARCH 策略调用。
 */
RewriteStatus rewrite_search_backward(
    ConstraintGraph *graph,
    RewritePattern *goal_pattern,
    RewriteRule **rules,
    int rule_count,
    const uint64_t *axiom_hashes,
    int axiom_count,
    int depth_limit,
    int search_mode,
    char ***out_path,
    int *out_path_len);
```

### 4.3 Search 到证明引擎的集成路径

`rewrite_search_backward()` 不替代现有证明引擎，而是作为新的证明策略之一：

```
DSL证明命令                          内部引擎
───────────────────────────────────────────────────────
prove P using strategy=rewrite_search;  →  PROOF_STRATEGY_REWRITE_SEARCH
    └─ rewrite_search_backward()         └─ 在 ProofMultiStrategy 中注册
    └─ 返回重写路径                       └─ ProofNavigator 收集 ProofStep
    └─ 若成功，路径折叠为 lemma            └─ 路径的每一步映射为 PROOF_STEP_APPLY_RULE
```

---

## 5. 编译流水线：.lvz 规则 → rewrite.h 内部结构

### 5.1 编译阶段

```
.lvz 公理包文件
    │
    ▼
┌─────────────────────┐
│ Stage 1: 词法/语法   │  .lvz 解析器 (新增 lvz_parser.c)
│ · @sort / @op /     │
│   @eq / @rl / @crl  │  解析 → 内部中间表示
└────────┬────────────┘
         │ LvzModule (中间表示)
         ▼
┌─────────────────────┐
│ Stage 2: Sort 展开  │  sort 层次 → TypeRegion 注册
│ · subsort 关系处理  │  type_system.h
│ · 排序一致性检查     │
└────────┬────────────┘
         │ 类型注册表
         ▼
┌─────────────────────┐
│ Stage 3: Op 编译    │  op → FuncBlock 创建
│ · ctor 属性处理     │  func_block.h
│ · assoc/comm 处理   │  交换律/结合律约束标记
└────────┬────────────┘
         │ FuncBlock[] 数组
         ▼
┌─────────────────────┐
│ Stage 4: Eq/Rl 编译 │  eq → RewriteRule (单向归约)
│ · @eq 的单向归约    │  ceq → RewriteRule + condition_func
│ · @ceq 的条件编译   │  rl  → RewriteRule (图重写)
│ · @eqv 的双向展开   │  eqv → 一对 RewriteRule
└────────┬────────────┘
         │ RewriteRule[] 数组
         ▼
┌─────────────────────┐
│ Stage 5: 验证与索引 │
│ · 汇流性检查 (可选)  │
│ · WL 哈希索引       │
│ · 优先级排序         │
└─────────────────────┘
         │ 编译完成的公理包
         ▼
      Lv-00 重写引擎可用
```

### 5.2 关键编译映射表

| .lvz 语法 | 编译后结构 | 所在模块 |
|-----------|-----------|---------|
| `@sort Point` | `type_region_register("Point", parent_id)` | `type_system.h` |
| `@op name : A B -> C [ctor]` | `func_block_create(name)`, 输入端口A/B, 输出端口C, flags |= FB_FLAG_CONSTRUCTOR | `func_block.h` |
| `@op name : A B -> C [assoc comm]` | 同上 + `constraint_create(ASSOCIATIVITY)` / `constraint_create(COMMUTATIVITY)` | `constraint_graph.h` |
| `@eq name : LHS = RHS` | `rewrite_rule_create("eq:name", LHS_pattern, RHS_replacement, measure)` | `rewrite.h` |
| `@eqv name : LHS <-> RHS` | 两条 `rewrite_rule_create()` — `eqv_name_fwd` 和 `eqv_name_rev` | `rewrite.h` |
| `@ceq name : LHS = RHS if COND` | `rewrite_rule_create()` + `condition_func=COND` | `rewrite.h` |
| `@rl [name] : LHS => RHS` | `rewrite_rule_create("rl:name", LHS_pattern, RHS_replacement, priority)` | `rewrite.h` |

---

## 6. 完整示例：三角形中线重心定理的重写证明

### 6.1 问题描述

**已知**: 三角形 ABC，M_AB、M_BC、M_CA 分别为三边中点，med_A、med_B、med_C 为三条中线。

**证明**: 三条中线交于一点（重心 G），且 G 定位在每条中线的 2/3 处。

### 6.2 .lvz 公理包（输入）

```
// file: median_centroid.lvz

@name "三角形中线重心公理"
@version "1.0.0"

@sort Point
@sort Segment subsort_of Line
@sort Triangle subsort_of Region

@op midpoint     : Point Point -> Point              [ctor]
@op segment      : Point Point -> Segment            [ctor]
@op triangle     : Point Point Point -> Triangle     [ctor]
@op centroid     : Point Point Point -> Point        [ctor]
@op distance     : Point Point -> Number
@op collinear    : Point Point Point -> Bool
@op concurrent   : Segment Segment Segment -> Bool
@op is_on_segment : Point Segment -> Bool

@eq eq_midpoint_coords : midpoint(P1, P2) =
    point((P1.x+P2.x)/2, (P1.y+P2.y)/2)

@eq eq_centroid_coords : centroid(A, B, C) =
    point((A.x+B.x+C.x)/3, (A.y+B.y+C.y)/3)

@rl [recognize_centroid_on_median] : priority(5)
    is_on_segment(G, segment(A, midpoint(B, C)))
    & is_on_segment(G, segment(B, midpoint(C, A)))
    => G = centroid(A, B, C)

@rl [centroid_on_medians] : priority(3)
    med_A = segment(A, midpoint(B, C))
    & med_B = segment(B, midpoint(C, A))
    & med_C = segment(C, midpoint(A, B))
    & concurrent(med_A, med_B, med_C)
    => G = centroid(A, B, C) & incident(G, med_A) & incident(G, med_B) & incident(G, med_C)
```

### 6.3 策略定义（使用策略语言）

```
// 策略：先通过等式展开中点坐标，再用规则识别重心
// 等价于 Maude: apply(eq_midpoint_coords)! ; apply(recognize_centroid_on_median)
RewriteStrategy *s = rewrite_strategy_sequence(
    rewrite_strategy_normalize(
        rewrite_strategy_create_apply(rule_eq_midpoint_coords)
    ),
    rewrite_strategy_create_apply(rule_recognize_centroid_on_median)
);
```

### 6.4 证明搜索（反向）

```
// 输入图：包含三角形 ABC、三条中线、concurrent 约束
ConstraintGraph *graph = build_median_graph(A, B, C);

// 目标模式：concurrent(med_A, med_B, med_C)
RewritePattern *goal = build_concurrent_pattern(graph);

// 公理哈希：centroid 公式 + 中点公式
uint64_t axioms[] = { hash_centroid_coords, hash_midpoint_coords };

// 反向搜索
char **path;
int path_len;
RewriteStatus st = rewrite_search_backward(
    graph, goal, rules, rule_count,
    axioms, 2,     /* 公理终点 */
    50, 0,         /* depth=50, BFS */
    &path, &path_len
);

// 如果搜索成功，path 包含归约步骤序列
// 例如: ["eq_midpoint_coords", "eq_midpoint_coords", "eq_midpoint_coords", "recognize_centroid_on_median"]
```

### 6.5 预期输出

```
[REWRITE_SEARCH] BFS depth 0: 1 open state
[REWRITE_SEARCH] BFS depth 1: expand 3 rules, 1 new state
[REWRITE_SEARCH] BFS depth 2: expand 3 rules, 1 new state
[REWRITE_SEARCH] BFS depth 3: expand 3 rules, 1 new state
[REWRITE_SEARCH] BFS depth 4: expand 1 rule, 1 new state (HIT axiom hash)
[REWRITE_SEARCH] Solution found! Path length = 4
  Step 1: apply eq_midpoint_coords (M_BC)
  Step 2: apply eq_midpoint_coords (M_CA)
  Step 3: apply eq_midpoint_coords (M_AB)
  Step 4: apply recognize_centroid_on_median
[REWRITE_SEARCH] Proof complete: GREEN
```

---

## 7. 关键数据结构对照表

### 7.1 Maude → Lv-00 数据结构映射

| Maude 概念 | Maude 内部结构 | Lv-00 映射结构 | 文件 |
|-----------|--------------|---------------|------|
| `Sort` | `Symbol::sort()` | `GeomType` 枚举 + `TypeRegion` | `constraint_graph.h`, `type_system.h` |
| `Subsort` | `Symbol::leqSort()` | `type_region_get_subtypes()` | `type_system.h` |
| `Operator` | `Symbol::attachedOpDecl()` | `FuncBlock` | `func_block.h` |
| `Equation` | `RewriteRule::lhs/rhs` (在 Maude 内部) | `RewriteRule` (单向归约) | `rewrite.h` |
| `Rule` | `RewriteRule::lhs/rhs` | `RewriteRule` (图重写) | `rewrite.h` |
| `RewritingContext` | `RewritingContext` | `ConstraintGraph` (作为"状态") | `constraint_graph.h` |
| `Strategy` (Maude strat lang) | `StrategicExecution::StrategyExpression` | `RewriteStrategy` 树 | 新增 `strategy.h` |
| `Search` | `SearchState` + `SearchGraph` | `rewrite_search_backward()` | `rewrite.h` (追加) |
| `Variant` | `VariantSearch` (窄化) | `ProofSearchTree` | `proof.h` |
| `Narrowing` | `NarrowingSearchState` | `rewrite_search_backward()` 的逆向规则应用 | `rewrite.h` (追加) |

### 7.2 策略组合子对照表

| Maude 策略 | 语义 | Lv-00 `RewriteStrategyKind` | 行为描述 |
|-----------|------|----------------------------|---------|
| `idle` | 空操作 | `REWRITE_STRATEGY_IDLE` | 不修改图，返回 REWRITE_OK |
| `fail` | 立即失败 | `REWRITE_STRATEGY_FAIL` | 不修改图，返回 REWRITE_NO_MATCH |
| `apply(R)` | 应用规则 R 一次 | `REWRITE_STRATEGY_APPLY_RULE` | `find_rewrite_match` + `apply_rewrite` |
| `match(P)` | 匹配模式 P | `REWRITE_STRATEGY_MATCH_PATTERN` | `vf2_find_match`（不替换） |
| `S1 ; S2` | 先 S1 后 S2 | `REWRITE_STRATEGY_SEQUENCE` | 递归：先评估 left，若成功则评估 right |
| `S1 \| S2` | S1 或 S2 (orelse) | `REWRITE_STRATEGY_ORELSE` | 递归：先评估 left，若失败则评估 right |
| `S *` | 重复 S | `REWRITE_STRATEGY_REPEAT` | 循环：反复评估 left，每次成功重置 |
| `S !` | 归一化 S | `REWRITE_STRATEGY_NORMALIZE` | 循环：反复评估 left，每次检查 WL 哈希 |
| `test(C)` | 条件测试 | `REWRITE_STRATEGY_TEST_COND` | 调用 `RewritePrecondition` 回调 |
| `try(S)` | 尝试 S | `REWRITE_STRATEGY_TRY` | 评估 left，失败返回 REWRITE_OK |

### 7.3 文件依赖关系

```
.lvz 公理包文件
    │
    ├── lvz_parser.c         (新增，解析 @sort/@op/@eq/@rl)
    │   ├── type_system.h    (注册 sort 层次)
    │   ├── func_block.h     (创建 op → FuncBlock)
    │   └── rewrite.h        (创建 eq/rl → RewriteRule)
    │
    ├── strategy.c            (新增，策略树构造和评估)
    │   └── rewrite.h        (RewriteStrategy → RewriteStatus)
    │
    └── rewrite_search.c      (新增，反向证明搜索)
        ├── rewrite.h        (rewrite_search_backward)
        └── proof.h          (集成到 ProofMultiStrategy)
```

---

## 附录 A：.lvz 公理包完整 BNF 文法（简化版）

```
<lvz_file>       ::= <header> <stmt>*

<header>         ::= '@name' <string> '@version' <string> '@description' <string>

<stmt>           ::= <sort_decl>
                 |   <op_decl>
                 |   <eq_decl>
                 |   <ceq_decl>
                 |   <eqv_decl>
                 |   <rl_decl>
                 |   <crl_decl>

<sort_decl>      ::= '@sort' <identifier> ['subsort_of' <identifier>]

<op_decl>        ::= '@op' <identifier> ':' <type_list> '->' <type>
                     [ '[' <attr_list> ']' ]

<eq_decl>        ::= '@eq' <identifier> ':' <term> '=' <term>

<ceq_decl>       ::= '@ceq' <identifier> ':' <term> '=' <term> 'if' <term>

<eqv_decl>       ::= '@eqv' <identifier> ':' <term> '<->' <term>

<rl_decl>        ::= '@rl' '[' <identifier> ']' ':' ['priority(' <number> ')']
                     <pattern> '=>' <replacement>

<crl_decl>       ::= '@crl' '[' <identifier> ']' ':'
                     <pattern> '=>' <replacement> 'if' <condition>
```

## 附录 B：与现有 rewrite.h 的集成关系

```
rewrite.h (现有 API)                   本设计追加内容
─────────────────────────────────────────────────────────────
rewrite_rules_load_from_file()   →   扩展支持 .lvz @sort/@op/@eq/@rl 解析
rewrite_rule_create()            →   不变（eq/rl 编译后都调用此函数）
find_rewrite_match()             →   不变（用作策略和搜索的底层匹配）
apply_rewrite()                  →   不变（用作策略和搜索的底层替换）
find_all_non_overlapping_matches() → 不变
rewrite_apply_all_matches()      →   不变
detect_rewrite_loop_wl()         →   集成到 NORMALIZE 策略的不动点检测
rewrite_compute_wl_hash()        →   集成到反向搜索的状态去重

(新增)                             → RewriteStrategy (策略类型)
(新增)                             → rewrite_strategy_apply()
(新增)                             → rewrite_search_backward()
```

---

> **文档结束**  
> 本文档详述了 Maude 重写逻辑三要素（sort/op/eq/rl）如何映射到 Lv-00 的 .lvz 公理包格式，策略语言如何集成到 rewrite.h，以及 Search 反向推理命令如何映射到 Lv-00 证明搜索。

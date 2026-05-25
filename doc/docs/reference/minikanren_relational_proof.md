# Lv-00 参考落地设计文档：miniKanren/core.logic 关系式证明

> **版本**: 1.0.0
> **日期**: 2026-05-24
> **参考**: miniKanren (github.com/miniKanren/miniKanren) + core.logic (Clojure 关系式编程库)
> **目标**: 将关系式编程的"关系即双向计算"范式映射到 Lv-00 几何构造——正向与反向查询为同一段代码，映射到 rewrite.h 的证明搜索

---

## 目录

1. [项目概述与 Lv-00 借鉴动机](#1-项目概述与-lv-00-借鉴动机)
2. [核心借鉴要点：关系即双向计算](#2-核心借鉴要点关系即双向计算)
3. [几何构造的双向性映射方案](#3-几何构造的双向性映射方案)
4. [双向查询引擎设计](#4-双向查询引擎设计)
5. [与 rewrite.h 证明搜索的集成](#5-与-rewriteh-证明搜索的集成)
6. [完整示例：三角形全等条件的双向查询](#6-完整示例三角形全等条件的双向查询)
7. [关键数据结构对照表](#7-关键数据结构对照表)
8. [实现路线图](#8-实现路线图)

---

## 1. 项目概述与 Lv-00 借鉴动机

### 1.1 miniKanren / core.logic 是什么

miniKanren 是 Dan Friedman 和 William Byrd 设计的极简关系式编程语言，最初仅用几十行 Scheme 代码实现。core.logic 是其在 Clojure 生态中的成熟实现。它们的核心创新是**关系即双向计算**——关系不是单向函数，而是可以在所有方向上"运行"的多模式约束。

```clojure
;; core.logic 示例：append 关系的三向使用
(run* [q] (appendo [1 2] [3 4] q))   ;; → ([1 2 3 4])  正向: 给定 x, y, 求 z
(run* [q] (appendo [1 2] q [1 2 3 4])) ;; → ([3 4])    反向: 给定 x, z, 求 y
(run* [q] (appendo q [3 4] [1 2 3 4])) ;; → ([1 2])    反向: 给定 y, z, 求 x
(run* [q] (fresh [x y] (appendo x y q))) ;; → (...所有可能的组合...)  全自由
```

核心机制：所有参数同等对待——没有"输入"和"输出"的固定区分。同一个 `appendo` 定义，在四组不同的已知/未知参数配置下都能正确求解。

### 1.2 Lv-00 借鉴动机

Lv-00 的几何构造本质上是一种"关系"——定义一个三角形 ABC，即声明了点 A、B、C 之间满足的共线/不共线/边长等关系。但在当前实现中：

| 场景 | 当前实现 | 关系式编程理念下的理想 |
|------|---------|---------------------|
| 给定三顶点 A、B、C | `triangle(A, B, C)` 创建三角形 | 同一段代码 |
| 给定两顶点 A、B 和面积 S | 需要另写"从 AB 和面积反求 C" | **同一段代码** |
| 给定三边长，求三顶点 | 需要另写"从边长构建三角形" | **同一段代码** |
| 从命题结论反推前提 | `rewrite_search_backward()` 分路径 | 应与正向构造共享搜索空间 |

核心洞察：miniKanren 用**合一（unification）+ 目标（goal）+ 搜索（search）** 三要素实现了双向性。Lv-00 已经具备 `unify.h`（合一）、`rewrite.h`（搜索）、`constraint_graph.h`（约束=目标），三要素皆备——缺的只是将它们组织为"关系式"的调度层。

### 1.3 总体架构对照

```
miniKanren / core.logic                  Lv-00
────────────────────────────────────────────────────────
goal (λ S → S' | fail)           →    RewritePattern + Constraint 组合
unification (≡)                   →    proof_unify() (unify.h)
conde (disjunction)              →    ProofMultiStrategy (多策略)
fresh (existential)              →    SymbolicCoord (未定值变量)
run* (search all solutions)      →    rewrite_search_backward()
run 1 (first solution)           →    rewrite_strategy_apply()
== (equality goal)               →    type_check_equivalence()
```

---

## 2. 核心借鉴要点：关系即双向计算

### 2.1 目标（goal）作为一等对象

miniKanren 的核心抽象是将"目标"（goal）作为一等对象。goal 是一个接受当前状态（substitution）并返回一系列成功的扩展状态（lazy stream）的函数：

```
goal :: State → Stream[State]
```

在 Lv-00 中，对等的抽象是：`ConstraintGraph` 的一个局部子图（pattern）作为搜索模板，搜索引擎从当前图中匹配该 pattern，返回所有可能的匹配（变量绑定）。

```
Lv-00 goal :: ConstraintGraph × RewritePattern → Stream[RewriteMatch]
```

### 2.2 五种目标构造器

| miniKanren 目标 | 语义 | Lv-00 映射 |
|:---|:---|:---|
| `≡`（合一） | 将两个项合一 | `proof_unify(graph, pattern1, pattern2)` → `RewriteMatch` |
| `fresh`（存在量词） | 引入新的逻辑变量 | `SymbolicCoord`（未赋值的符号变量） |
| `conde`（或） | 析取——尝试多条路径 | `ProofMultiStrategy` 多策略并行 |
| `run*`（全搜索） | 返回所有解 | `rewrite_search_backward()` BFS 模式 |
| `project`（具体化） | 将逻辑变量具体化为值 | `symbolic_coord_from_rational()` 回写 |

### 2.3 双向性的本质：参数对称化

miniKanren 的双向性源于**不区分参数的"输入/输出"方向**。在几何语境中：

```
传统的"函数"思维：      midpoint(A, B) → M
关系式"关系"思维：       midpoint(A, B, M)  ≡  true
                        // A、B、M 三个参数完全对称
                        // 已知任意两个，可以求第三个
```

Lv-00 的 `FuncBlock` 当前是方向性的（`input_port_ids` → `output_port_ids`）。借鉴 miniKanren 后，可以为每个 `FuncBlock` 生成一个**对称化版本**——将所有端口（包括输入和输出）统一编入 pattern 的 `variable_node_ids`，由求解器决定哪些是已知/未知。

---

## 3. 几何构造的双向性映射方案

### 3.1 FuncBlock 的关系化改造

为 `FuncBlock` 新增一个对称化标志和对应的关系化版本：

```c
/**
 * @brief 函数块对称化标记
 *
 * 借鉴 miniKanren "关系即双向计算"理念。
 * 对称化的 FuncBlock 不再区分 input/output 端口——
 * 所有端口都被视为平等的"关系参与者"。
 * 求解器根据当前已知端口的值，自动求解未知端口。
 */
typedef enum {
    FB_DIRECTION_FORWARD,       /* 标准单向: inputs → outputs */
    FB_DIRECTION_BACKWARD,      /* 仅逆向: outputs → inputs */
    FB_DIRECTION_RELATIONAL     /* 双向关系式: 任意方向由求解器自动判定 */
} FuncBlockDirection;

/* 追加到 FuncBlock 结构体 */
typedef struct {
    // ... 现有字段 ...
    FuncBlockDirection direction_mode;  /* 方向模式（新增） */
    int *all_port_ids;                  /* 统一端口数组（input + output 合并） */
    int all_port_count;
} FuncBlock;
```

### 3.2 关系化查询函数

借鉴 miniKanren 的 `run*` 范式，为几何构造提供双向查询接口：

```c
/**
 * @brief 关系式几何查询 —— 双向计算（miniKanren 风格）
 *
 * 给定一个 FuncBlock（关系）和一组部分已知/未知的端口值，
 * 自动判定方向并求解缺失值。所有端口平等对待。
 *
 * 对应 miniKanren 的 (run* [q] (relation a b c q))
 *
 * @param[in]     fb           函数块（关系）——所有端口平等
 * @param[in,out] graph        约束图（已知值已写入对应节点）
 * @param[in]     known_mask   已知端口标记（位掩码，1=已知）
 * @param[in,out] port_values  端口值数组（已知的已填入，未知的由函数填充）
 * @param[out]    out_matches  所有成功匹配的变量绑定（类似 miniKanren 的结果流）
 * @param[out]    out_count    匹配数量
 * @return 0 成功，-1 无解或参数错误
 *
 * @note 借鉴 miniKanren 的关系对称化思想。
 *       内部自动判定方向：
 *       - 若 known_mask 覆盖所有 input_port，走前向计算
 *       - 若 known_mask 覆盖所有 output_port，走逆向求解
 *       - 若混合（如 3 个已知、2 个未知），走约束图 + SMT 混合求解
 */
int func_block_query_relational(
    const FuncBlock *fb,
    ConstraintGraph *graph,
    uint64_t known_mask,
    SymbolicCoord **port_values,
    RewriteMatch ***out_matches,
    int *out_count);
```

### 3.3 逆向求解的约束展开

关系化查询在逆向模式下，需要将 FuncBlock 的输出→输入方向展开为约束系统：

```
传统正向:  midpoint(A, B) → M
          M.x = (A.x+B.x)/2, M.y = (A.y+B.y)/2

关系化逆向 (已知 M, B, 求 A):
          系统自动构建约束:
          M.x = (A.x+B.x)/2  →  A.x = 2*M.x - B.x
          M.y = (A.y+B.y)/2  →  A.y = 2*M.y - B.y

关系化双向 (已知 M, 求 A 和 B):
          约束系统含 2 个方程、4 个未知数
          → 无穷多解 → 需要额外条件（如指定 AB 长度）
          → 自动检测欠定系统，请求用户提供更多约束
```

---

## 4. 双向查询引擎设计

### 4.1 正向/反向的模式统一

miniKanren 不区分"正向模式"和"反向模式"——只有"已知参数配置"的概念。同一段目标代码在配置 A（已知 x,y → 求 z）和配置 B（已知 x,z → 求 y）下通过同一条搜索管道执行。

Lv-00 的双向查询引擎采用等价设计：

```
graph_relational_query(fb, graph, known_mask):
  1. 从 fb 的 all_port_ids 中提取已知和未知端口
  2. 将已知端口值编码为约束图中的 CONCRETE 节点
  3. 将未知端口注册为符号变量（SymbolicCoord, type=ALGEBRAIC）
  4. 展开 FuncBlock 内部约束（展开构造步骤为约束边）
  5. 调度求解器（优先 Gröbner 基（度数≤2）→ SMT 编码 → 数值逼近）
  6. 将求解结果写回 port_values
  7. 如果有多个可行解，调用 selector（如 select_nearest_to）选择
```

### 4.2 多解处理与 goal 流

miniKanren 的 goal 返回 lazy stream of states——不是返回"一个最优解"，而是"所有可行解"。Lv-00 几何中同样存在多解（如圆与直线的两个交点）。

```c
/**
 * @brief 返回几何关系所有可行解（miniKanren run* 风格）
 *
 * 不同于 func_block_query_relational 的"选择器过滤单解"模式，
 * 此函数返回所有满足约束的匹配，按某种质量度量排序。
 *
 * 对应 miniKanren 的 (run* [q] goal)
 */
int func_block_query_all_solutions(
    const FuncBlock *fb,
    ConstraintGraph *graph,
    uint64_t known_mask,
    SymbolicCoord **port_values,
    RewriteMatch ***out_all_matches,
    int *out_count);

/**
 * @brief 返回第一个可行解（miniKanren run 1 风格）
 */
int func_block_query_first_solution(
    const FuncBlock *fb,
    ConstraintGraph *graph,
    uint64_t known_mask,
    SymbolicCoord **port_values,
    RewriteMatch **out_match);
```

### 4.3 搜索空间的符号化表示

借鉴 miniKanren 的 substitution（替换映射 = 逻辑变量 → 具体值），Lv-00 的 `RewriteMatch` 结构天然就是几何语境下的 substitution：

| miniKanren | Lv-00 |
|:---|:---|
| `substitution: {[x → 5], [y → 3]}` | `RewriteMatch.node_bindings[i] = target_node_id` |
| `walk(x, s)` — 在 s 中查找 x 的值 | `symbolic_coord_from_rational()` 从绑定节点读取坐标 |
| `unify(x, 5, s)` — 合一 x 和 5 | `proof_unify(pattern_node, target_node)` |
| `reify(state)` — 将状态具体化为可读结果 | `SMTSolverResult` 的解码 → `SymbolicCoord` |

---

## 5. 与 rewrite.h 证明搜索的集成

### 5.1 关系化证明搜索的流程

```
命题证明（关系式视角）:
  给定: triangle(A, B, C) + median_definitions
  证明: concurrent(med_A, med_B, med_C)

关系式表述:
  (run* [proof_path]
    (fresh [G]                          ;; 引入重心点 G
      (≡ (midpoint A B) M_AB)           ;; 中点关系
      (≡ (midpoint B C) M_BC)
      (≡ (midpoint C A) M_CA)
      (≡ (segment A M_BC) med_A)        ;; 中线关系
      (≡ (segment B M_CA) med_B)
      (≡ (segment C M_AB) med_C)
      (≡ (centroid A B C) G)            ;; 重心关系
      (incident G med_A)                ;; G 在 med_A 上
      (incident G med_B)                ;; G 在 med_B 上
      (incident G med_C)                ;; G 在 med_C 上
      (project proof_path))))           ;; 提取证明路径
```

在 Lv-00 中，这等价于：将整个命题编码为 `RewritePattern`（包含所有中间变量和约束），然后调用 `rewrite_search_backward()` 从目标约束出发逆向搜索到公理：

```c
// 1. 将命题编码为关系化的 RewritePattern
RewritePattern *goal = proposition_to_relational_pattern(prop);

// 2. 公理哈希表（中点公式、重心公式等）
uint64_t axiom_hashes[] = {
    compute_midpoint_axiom_hash(),
    compute_centroid_axiom_hash(),
    compute_collinearity_axiom_hash()
};

// 3. 双向搜索
char **path;
int path_len;
RewriteStatus st = rewrite_search_backward(
    graph, goal, rewrite_rules, rule_count,
    axiom_hashes, 3, 100, 0, &path, &path_len);

// 4. 若成功，path 中的每一步可映射为自然语言证明步骤
```

### 5.2 合一（unification）作为证明的核心机制

miniKanren 的合一操作符 `≡` 是关系式编程的核心。Lv-00 的 `proof_unify()` 天然适合这个角色：

```c
/**
 * @brief 关系式合一——借鉴 miniKanren 的 ≡ 操作符
 *
 * 在约束图中检查两个子图是否可在当前变量绑定下合一。
 * 如果合一成功，返回扩展后的绑定表（substitution）。
 * 如果合一失败，返回 NULL。
 *
 * @param graph     约束图
 * @param pattern1  待合一的第一个模式
 * @param pattern2  待合一的第二个模式
 * @param existing_bindings 当前已有绑定（可为 NULL）
 * @param out_extended  输出扩展后的绑定表
 * @return 0 合一成功，-1 合一失败
 *
 * @note 对应 miniKanren 的 (≡ u v) → extended substitution | fail
 *       如果 existing_bindings 非空，先应用已有绑定再尝试合一。
 */
int relational_unify(
    ConstraintGraph *graph,
    const RewritePattern *pattern1,
    const RewritePattern *pattern2,
    const RewriteMatch *existing_bindings,
    RewriteMatch **out_extended);
```

### 5.3 conde（析取搜索）与多策略证明

miniKanren 的 `conde` 是析取分支——"尝试路径 A，如果失败则尝试路径 B"。Lv-00 的 `ProofMultiStrategy` 已经实现了等价的"尝试多种证明策略"：

```
;; miniKanren:
(conde
  [goal_a]       ;; 策略 A: 面积法
  [goal_b]       ;; 策略 B: 向量法
  [goal_c])      ;; 策略 C: Gröbner 基法

// Lv-00 等价:
ProofMultiStrategy *mse = proof_multi_strategy_create(nav);
proof_multi_strategy_activate(mse, PROOF_STRATEGY_AREA_METHOD);
proof_multi_strategy_activate(mse, PROOF_STRATEGY_VECTOR_METHOD);
proof_multi_strategy_activate(mse, PROOF_STRATEGY_GROEBNER_BASIS);
// mse 按优先级依次尝试，任一成功即返回
```

---

## 6. 完整示例：三角形全等条件的双向查询

### 6.1 问题描述

三角形 ABC 和三角形 DEF，已知 AB = DE, BC = EF, CA = FD。该命题可以正向使用（已知两个三角形 → 验证全等），也可以反向使用（已知一个三角形 + 部分对应条件 → 构造另一个三角形）。

### 6.2 关系化函数块定义

```
// 关系化定义：triangle_congruence 不区分输入/输出方向
@funcblock_relational triangle_congruence_sss(
    A : Point, B : Point, C : Point,    // 三角形 1（三顶点平等）
    D : Point, E : Point, F : Point     // 三角形 2（三顶点平等）
) {
    @constraint distance(A, B) = distance(D, E);
    @constraint distance(B, C) = distance(E, F);
    @constraint distance(C, A) = distance(F, D);
}

// 使用示例 1: 正向验证（已知全部 6 个点）
// 输入: A(0,0), B(4,0), C(2,3), D(5,0), E(9,0), F(7,3)
// 输出: true（全等）

// 使用示例 2: 逆向构造（已知 ABC 和 DE 边，求 F）
// 输入: A(0,0), B(4,0), C(2,3), D(5,0), E(9,0), F(?, ?)
// 约束: |AB|=|DE|=4, |BC|=|EF|=√13, |CA|=|FD|=√13
// 求解器自动构建: F 到 D 距离 √13, F 到 E 距离 √13
// 输出: F(7,3) 或 F(7,-3)（两个解，selector 选择正根）
```

### 6.3 编译后的内部执行

```c
// 关系化查询调用
FuncBlock *fb = load_funcblock("triangle_congruence_sss");

// 已知掩码: A,B,C,D,E 已知 (bits 0,1,2,3,4 = 1); F 未知 (bit 5 = 0)
uint64_t known_mask = 0b011111;

// 端口值
SymbolicCoord *values[6];  // [A, B, C, D, E, F]
values[0] = coord(0, 0);   // A
values[1] = coord(4, 0);   // B
values[2] = coord(2, 3);   // C
values[3] = coord(5, 0);   // D
values[4] = coord(9, 0);   // E
values[5] = NULL;           // F → 待求解

// 关系化查询（自动判定方向，走逆向求解路径）
RewriteMatch **solutions;
int solution_count;
func_block_query_relational(fb, graph, known_mask, values,
                            &solutions, &solution_count);

// 预期结果: solutions[0] → F = (7, 3)
//          solutions[1] → F = (7, -3)
```

---

## 7. 关键数据结构对照表

### 7.1 miniKanren → Lv-00 核心概念映射

| miniKanren 概念 | 数据结构/语义 | Lv-00 映射 | Lv-00 文件 |
|:---|:---|:---|:---|
| `goal` | `State → Stream[State]` | `ConstraintGraph × RewritePattern → RewriteMatch[]` | `rewrite.h` |
| `substitution` | `[Var → Value]` 映射表 | `RewriteMatch.node_bindings[]` | `rewrite.h` |
| `≡` (unify) | 合一两个项 | `proof_unify()` | `unify.h` |
| `fresh` | 引入逻辑变量 | `SymbolicCoord`（ALGEBRAIC 类型） | `symbolic_coord.h` |
| `conde` | 析取分支 | `ProofMultiStrategy` 多策略 | `proof.h` |
| `run*` | 全搜索 | `rewrite_search_backward()` BFS | `rewrite.h` |
| `run N` | 前 N 个解 | `rewrite_strategy_apply()` 带 limit | `rewrite.h` |
| `project` | 具体化逻辑变量 | `symbolic_coord_from_rational()` | `symbolic_coord.h` |
| `==` (equality) | 等价断言 | `type_check_equivalence()` | `type_system.h` |
| `delay` (延迟目标) | 暂缓评估 | `Constraint` 的 `lazy_eval` 标志 | `constraint_graph.h` |

### 7.2 FuncBlock 方向模式

| 模式 | miniKanren 等价 | 适用场景 |
|:---|:---|:---|
| `FB_DIRECTION_FORWARD` | `(== result (f a b))` 仅正向 | 确定性构造（如求中点） |
| `FB_DIRECTION_BACKWARD` | `(fresh [x] (== (f x b) result))` 仅逆向 | 已知结果反求参数 |
| `FB_DIRECTION_RELATIONAL` | `(fresh [x y z] (relation x y z))` 全方向 | 通用关系查询 |

---

## 8. 实现路线图

### 8.1 第一阶段：FuncBlock 关系化改造（P3）

| 任务 | 文件 | 说明 |
|:---|:---|:---|
| 在 `FuncBlock` 中新增 `direction_mode` 字段 | `include/lv00/func_block.h` | 新增 `FuncBlockDirection` 枚举 + 结构体字段 |
| 新增 `all_port_ids` 数组 | `include/lv00/func_block.h` | 将 input + output 端口统一管理 |
| 实现 `func_block_set_relational()` | `src/func_block.c` | 将 FuncBlock 标记为关系式模式 |
| 对称化工具函数 | `src/func_block.c` | 从输入/输出端口生成统一的 all_port_ids |

**预估规模**：约 80 行 C 代码

### 8.2 第二阶段：关系化查询 API（P3）

| 任务 | 文件 | 说明 |
|:---|:---|:---|
| `func_block_query_relational()` | `src/relational_query.c`（新文件） | 核心双向查询函数 |
| `func_block_query_all_solutions()` | `src/relational_query.c` | 全解查询（run* 风格） |
| `func_block_query_first_solution()` | `src/relational_query.c` | 首解查询（run 1 风格） |
| 方向自动判定逻辑 | `src/relational_query.c` | 根据 known_mask 选择前向/逆向/混合路径 |
| `relational_unify()` | `src/relational_query.c` | 关系式合一的封装 |

**预估规模**：约 200 行 C 代码

### 8.3 第三阶段：证明搜索集成（P3）

| 任务 | 说明 |
|:---|:---|
| 将 `rewrite_search_backward()` 的 goal_pattern 与 relational query 打通 | 命题模式 → 关系化 RewritePattern → 双向搜索 |
| memoization 表：缓存已求解的几何关系 | 避免重复展开同一 FuncBlock 内部约束 |
| `ProveRelationalPanel.tsx` 前端组件 | 让用户以"关系"视角提交证明——填部分已知值，系统自动求解缺失值并验证 |

---

## 附录：miniKanren 目标流与 Lv-00 搜索状态机的对应

```
miniKanren goal stream               Lv-00 证明搜索状态机
──────────────────────────────────────────────────────────────
┌──────┐                             ┌─────────────────────┐
│State │ ── goal ──→ Stream[State]   │ConstraintGraph      │
│  σ₀  │                             │  (初始: 给定条件)    │
└──────┘                             └─────────┬───────────┘
                                               │ apply goal
                                               ▼
                                    ┌─────────────────────┐
                                    │ 中间状态序列         │
                                    │ G₁, G₂, G₃, ...     │
                                    │ (每步 = 规则应用)    │
                                    └─────────┬───────────┘
                                               │ filter
                                               ▼
                                    ┌─────────────────────┐
                                    │ 成功状态             │
                                    │ (目标约束全部满足)   │
                                    │ ≡ miniKanren 的     │
                                    │   reified state      │
                                    └─────────────────────┘
```

---

> **文档结束**
> 本文档详述了 miniKanren/core.logic "关系即双向计算"范式如何映射到 Lv-00 几何构造——通过 FuncBlock 关系化改造和双向查询引擎，使得同一段构造代码既能正向从条件推导结论，又能反向从结论反求条件。这与 Lv-00 "构造即证明"理念的核心对称性产生深层共鸣。

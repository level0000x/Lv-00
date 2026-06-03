# Yices 2 EF 混合求解架构借鉴设计

> **借鉴项目**：Yices 2（yices.csl.sri.com）
> **核心借鉴点**：MCSAT 架构、EF（Exists/Forall）双求解器循环、模型引导泛化（Model-Guided Generalization）
> **分类**：P1 高优先级 / 混合求解架构
> **日期**：2026-05-24

---

## 1. 概述

Yices 2 是由 SRI International 开发的高性能 SMT 求解器，其核心架构创新在于 **MCSAT（Model-Constructing Satisfiability）**——一种将 SAT 搜索与理论推理深度融合的新型求解架构。与传统 DPLL(T) 架构将 SAT 引擎和理论求解器作为两个独立黑盒进行离线交互不同，MCSAT 允许理论求解器在 SAT 搜索过程中直接构造模型，实现搜索与推理的紧密协同。Yices 2 的 MCSAT 架构和 EF 求解循环对 Lv-00 的几何约束求解器有极其关键的借鉴价值。

Yices 2 的三个核心技术对 Lv-00 的求解器架构具有关键借鉴价值：

1. **MCSAT 架构：SAT 搜索中直接构造理论模型**：传统 DPLL(T) 将 SAT 和理论推理分离为两个独立的循环——SAT 引擎负责命题赋值搜索，理论求解器负责检查赋值的一致性。当理论求解器发现冲突时，需要重新启动 SAT 搜索。MCSAT 打破了这种"离线"组合——在 SAT 搜索的每一步，理论求解器都可以主动介入并直接构造满足当前赋值的理论模型。对于 Lv-00 的几何求解，这意味着在搜索几何构造方案的同时，可以直接构造满足几何约束的坐标模型。

2. **EF（Exists/Forall）双求解器循环**：Yices 2 MCSAT 中的核心算法是两个交替运行的求解循环——E-loop（存在量词赋值循环）和 F-loop（全称量词冲突分析循环）。E-loop 负责搜索满足所有存在约束的赋值，F-loop 负责分析冲突并从反例中学习新的约束。这种"搜索-分析-学习"的双循环模式完美对应几何验证中的"构造-检查-精化"过程。

3. **模型引导泛化（Model-Guided Generalization, MGG）**：MCSAT 从冲突中学习时，不仅学习冲突的原因（conflict clause），还通过"模型引导"将冲突泛化为更一般的不等式约束。例如，从"x=3 导致冲突"泛化为"x>2 范围内都会冲突"。在几何中对应从具体反例泛化为几何约束——如"当角度大于 90 度时，钝角三角形的某性质不成立"。

MCSAT 与传统 DPLL(T) 的架构对比：

```
传统 DPLL(T) —— 离线组合：
  ┌─────────────┐      ┌──────────────────┐
  │  SAT 引擎   │──────│  理论求解器(T)    │
  │  (CDCL)     │ 离线 │  (单纯形/NRA/...) │
  │             │──────│                  │
  │  命题搜索   │ 交互 │  一致性检查       │
  └─────────────┘      └──────────────────┘
     每次 SAT 赋值完整后，才调用理论求解器
     冲突时：理论求解器返回冲突子句，SAT 引擎回溯

MCSAT —— 紧密融合：
  ┌──────────────────────────────────────────┐
  │            MCSAT 统一引擎                 │
  │                                          │
  │  ┌──────────┐    ┌───────────────────┐   │
  │  │ 布尔搜索  │◄──►│  理论推理 + 模型   │   │
  │  │ (CDCL)   │在线│  构造 (NRA/EUF/   │   │
  │  │          │交互│   BV/...)         │   │
  │  └──────────┘    └───────────────────┘   │
  │        ▲                  ▲              │
  │        └────── 模型引导 ──┘              │
  └──────────────────────────────────────────┘
     每次部分赋值即可触发理论推理
     理论求解器直接构造模型（不只是检查一致性）
```

---

## 2. 核心借鉴 → Lv-00 映射

### 2.1 MCSAT 如何直接在搜索中构造理论模型

MCSAT 的核心创新在于将理论推理嵌入到 CDCL（Conflict-Driven Clause Learning）搜索的每一次决策中。传统 DPLL(T) 中，理论求解器是被动的"裁判"（只回答 SAT/UNSAT），而 MCSAT 中的理论求解器是主动的"建设者"（直接构造满足约束的数学对象）。

**MCSAT 处理非线性实数运算（NRA）的流程**：

```
搜索状态：布尔变量赋值 b = [x>0 → true, y>0 → true, z*2 = x+y → true]
理论约束：z^2 = x + y, x > 0, y > 0, z < 0

MCSAT 处理：
  1. [布尔搜索] 给所有文字赋值 true
  2. [理论推理] 检测赋值是否理论一致
     - 从 x>0, y>0 推理出 x+y > 0
     - 从 z^2 = x+y 推理出 z^2 > 0
     - 从 z < 0 推理出 z^2 > 0（负数平方为正）
     - 一致！尝试构造模型
  3. [模型构造] 求解多项式系统 {z^2 = x+y, x>0, y>0, z<0}
     - 选择具体值：x=1, y=3, z=-2  → z^2=4, x+y=4 ✓
     - 所有约束满足，返回模型
```

在 Lv-00 的几何场景中，MCSAT 对应的是**构造性几何求解**：

```
几何搜索状态：
  - 已知：点A(0,0), 点B(4,0)
  - 目标：构造点P，使得 △ABP 是边长为4的等边三角形

Lv-00 MCSAT 适配处理：
  1. [布尔搜索] 枚举构造方案
     方案1: P 在 AB 上方 → true
     方案2: P 在 AB 下方 → true (两个解)
  2. [理论推理] 检测构造的几何一致性
     - |AP| = 4  → (P.x-0)^2 + (P.y-0)^2 = 16
     - |BP| = 4  → (P.x-4)^2 + (P.y-0)^2 = 16
     - 联立求解：
       P.x^2 + P.y^2 = 16
       (P.x-4)^2 + P.y^2 = 16
       → P.x^2 - (P.x^2 - 8P.x + 16) = 0
       → 8P.x = 16  → P.x = 2
       → 4 + P.y^2 = 16 → P.y^2 = 12 → P.y = ±2√3
  3. [模型构造] 输出两个几何模型：
     P1(2, 2√3) — 上方等边三角形
     P2(2, -2√3) — 下方等边三角形
```

MCSAT 在 Lv-00 中的关键适配在于：**几何知识的内嵌**。传统 MCSAT 处理的是通用代数不等式，而 Lv-00 需要在 MCSAT 的理论推理层嵌入几何专用知识：

| MCSAT 理论 | Lv-00 几何适配 | 嵌入方式 |
|:---|:---|:---|
| 线性实数运算（LRA） | 共线/平行判定 | 行列式等式（线性） |
| 非线性实数运算（NRA） | 距离/角度计算 | 多项式等式/不等式 |
| 未解释函数（EUF） | 几何构造操作符 | 构造操作符作为未解释函数 + 几何公理 |
| 位向量（BV） | 离散化坐标（可选） | 有理数坐标的近似编码 |

### 2.2 EF 双求解器循环映射到几何存在性问题

Yices 2 MCSAT 的核心算法包含两个交替运行的循环：

```
E-loop（存在量词赋值循环, Exists-loop）:
  目标：搜索满足所有存在约束的赋值
  输入：一组存在量词约束（∃x. φ(x)）
  过程：
    1. 选择一个未赋值的变量 x
    2. 尝试为 x 选择一个值 v（从理论域中）
    3. 检查赋值 [x→v] 是否与已有约束一致
    4. 如果一致，继续下一个变量
    5. 如果不一致，进入 F-loop 分析冲突

F-loop（全称量词冲突分析循环, Forall-loop）:
  目标：从冲突中学习新约束，排除不可行的赋值区域
  输入：当前部分赋值 + 导致冲突的约束
  过程：
    1. 分析冲突的根因（conflict analysis）
    2. 使用模型引导泛化（MGG）生成一个排除冲突区域的约束
    3. 将新约束添加到约束集合
    4. 回溯到 E-loop 的安全状态
    5. E-loop 在新约束下重新搜索
```

在 Lv-00 的几何验证中，EF 循环映射为解决"存在满足约束的几何构造"问题：

```
几何 E-loop（构造搜索循环）:
  目标：搜索满足所有几何约束的点/线/圆的坐标
  过程：
    1. 选择一个未确定的几何对象（如 P 的坐标）
    2. 尝试为 P 选择一个候选位置
       - 启发式 1: 随机采样（快速找到可行区域）
       - 启发式 2: 梯度下降（朝满足约束的方向移动）
       - 启发式 3: 代数求解（精确解，如二次方程求根）
    3. 检查 P 的候选位置是否满足所有约束
    4. 如果满足，继续下一个未确定对象
    5. 如果不满足，进入几何 F-loop 分析冲突

几何 F-loop（约束精化循环）:
  目标：从冲突中学习新的几何约束，缩小搜索空间
  过程：
    1. 分析冲突：哪些约束同时无法满足？
       - 例如：|AP|=3 ∧ |BP|=3 ∧ |AB|=10 → 三角不等式违反
    2. 模型引导泛化：不是只排除 P 的当前候选值，
       而是泛化为排除一个区域
       - 例如：对于任意 P，如果 |AP|=3 且 |BP|=3，
         则 |AB| 必须 ≤ 6（三角不等式上界）
    3. 生成排除约束：|AB| ≤ 6（作为新学习的几何引理）
    4. 回溯到 E-loop，在新约束下重新搜索
    5. 如果排除约束直接指示无解，返回 UNSAT
```

**几何 EF 循环的具体示例：是否存在点 P 使三角形等边？**

```
问题：给定 A(0,0), B(4,0)，是否存在点 P 使得 △ABP 等边？

几何 E-loop 第 1 轮：
  - 未确定对象：P.x, P.y
  - 约束：|AP|=4, |BP|=4, |AB|=4
  - 尝试：随机选择 P(1, 3)
    - 计算：|AP| = √(1+9) = √10 ≈ 3.16 ≠ 4 → 不满足
    - |BP| = √(9+9) = √18 ≈ 4.24 ≠ 4 → 不满足
    → 进入 F-loop

几何 F-loop 第 1 轮：
  - 冲突分析：约束 |AP|=4 和 |BP|=4 不能同时被 P(1, 3) 满足
  - MGG 泛化：约束 |AP|=4 定义了以 A 为圆心半径 4 的圆，
              |BP|=4 定义了以 B 为圆心半径 4 的圆
              P 必须在这两个圆的交点上
  - 学习约束：P 的候选位置必须在 CA∩CB 的范围内，
              其中 CA=circle(A,4), CB=circle(B,4)
  - 返回 E-loop

几何 E-loop 第 2 轮（在 F-loop 学到的约束下）：
  - 约束更新：P ∈ CA ∩ CB
  - 代数求解：
    CA: (P.x)^2 + (P.y)^2 = 16
    CB: (P.x-4)^2 + (P.y)^2 = 16
    相减：P.x^2 - (P.x-4)^2 = 0 → 8P.x - 16 = 0 → P.x = 2
    代入：4 + P.y^2 = 16 → P.y = ±√12 = ±2√3
  - 构造两个解：
    P1(2, 2√3) → 验证：|AP1|=√(4+12)=4 ✓, |BP1|=√(4+12)=4 ✓
    P2(2, -2√3) → 验证：|AP2|=√(4+12)=4 ✓, |BP2|=√(4+12)=4 ✓
  → 存在！返回两个构造
```

### 2.3 模型引导泛化（MGG）在几何约束学习中的应用

MGG 是 MCSAT 区别于传统 CDCL 的关键技术。传统 CDCL 从冲突中学习的是一个布尔冲突子句（如 `¬(x>0) ∨ ¬(y>0)`），而 MGG 从冲突中学习的是一个**理论约束**（如 `x + y ≤ 0`），该约束排除的不仅是一个点而是一个区域。

**MGG 的通用流程**：

```
给定：冲突状态（部分赋值 + 冲突约束集合）
  1. 找到一个不可行核心（unsatisfiable core）：最小的冲突约束子集
  2. 从不可行核心中提取"冲突原因"：为什么这个子集不可满足？
  3. 泛化冲突原因：什么条件下这种冲突会再次发生？
  4. 生成排除约束：一个不等式/条件，排除所有会导致同类冲突的赋值
```

在 Lv-00 几何中，MGG 的表现形式：

| 冲突类型 | 传统学习（排除单点） | MGG 泛化学习（排除区域） |
|:---|:---|:---|
| 点的距离冲突 | `P ≠ (1, 3)` | `P ∉ circle(A,4) ∩ circle(B,4)` — 只允许交点 |
| 共线冲突 | `P ≠ (2, 1)` | `det(P-A, B-A) ≠ 0` — 排除整条直线 |
| 角度冲突 | `P ≠ (3, 4)` | `∠APB ≠ 90°` — 排除整个垂直条件 |
| 长度冲突 | `d ≠ 5` | `|AB| ≠ |CD| + 2` — 排除整个长度关系 |

### 2.4 对照表：Yices EF 循环 → Lv-00 solver.h 求解器迭代

| Yices MCSAT 概念 | Lv-00 solver.h 映射 | 说明 |
|:---|:---|:---|
| E-loop（存在赋值循环） | `solver_construction_search()` | 搜索满足几何约束的构造坐标 |
| F-loop（冲突分析循环） | `solver_conflict_analysis()` | 分析几何冲突并学习新约束 |
| MGG（模型引导泛化） | `solver_mgg_generalize()` | 从反例泛化为区域排除约束 |
| Theory propagation | `solver_geometric_deduction()` | 从已有约束推理出新的必然约束 |
| Variable ordering | `solver_variable_heuristic()` | 选择下一个需要确定坐标的几何对象 |
| Value selection | `solver_value_heuristic()` | 为几何对象选择候选坐标值 |
| Conflict clause | `solver_learned_constraint` | 学习的几何约束（排除不可行区域） |
| Backtrack level | `solver_backtrack_depth` | 回溯到的构造步骤编号 |

### 2.5 代码示例：Lv-00 中实现 EF 循环解决几何构造问题

```c
/**
 * @file solver.h (追加)
 * @brief EF 双求解器循环——借鉴 Yices 2 MCSAT 架构
 *
 * Yices 2 MCSAT 的核心创新：
 *  - E-loop（Exists）：搜索满足存在约束的赋值
 *  - F-loop（Forall）：从冲突中学习排除不可行区域的新约束
 *  - MGG（Model-Guided Generalization）：从点到区域的泛化
 *
 * Lv-00 适配：将 MCSAT 的通用代数求解适配为几何构造求解。
 * 核心用途：解决"是否存在几何构造满足条件"类型的问题。
 *   例如：是否存在点P使△ABP等边？
 *        是否存在圆C过三点A、B、C？
 *        是否存在平面变换T使∠A'B'C' = 2∠ABC？
 */

#ifndef LV00_SOLVER_EF_H
#define LV00_SOLVER_EF_H

#include "gvil.h"
#include "constraint.h"

/* ── 求解器状态 ─────────────────────────────────────── */

/**
 * @brief EF 求解循环的状态
 */
typedef enum {
    SOLVER_STATE_IDLE,          /**< 空闲 */
    SOLVER_STATE_E_LOOP,        /**< E-loop 执行中（搜索构造赋值） */
    SOLVER_STATE_F_LOOP,        /**< F-loop 执行中（冲突分析 + 学习） */
    SOLVER_STATE_SAT,           /**< 求解成功：找到满足所有约束的构造 */
    SOLVER_STATE_UNSAT,         /**< 求解失败：证明不存在满足约束的构造 */
    SOLVER_STATE_TIMEOUT,       /**< 超时 */
    SOLVER_STATE_UNKNOWN        /**< 未知（需要用户交互或更强的求解器） */
} SolverEFState;

/**
 * @brief 单个几何变量的赋值
 *
 * 在 E-loop 中，求解器逐个为几何变量（点坐标、长度、角度等）
 * 选择候选值。
 */
typedef struct {
    int variable_id;             /**< 变量 ID（对应 GVIL 中的变量） */
    GeoValueType value_type;     /**< 值类型：坐标/长度/角度/... */
    union {
        struct { double x, y; } point;       /**< 点的二维坐标 */
        double length;                        /**< 长度值 */
        double angle;                         /**< 角度值（弧度） */
        struct { double cx, cy, r; } circle;  /**< 圆的参数 */
    } value;
    bool is_fixed;               /**< 是否为固定值（非决策变量） */
    int decision_level;          /**< 决策层级（用于回溯） */
} GeoVariableValue;

/**
 * @brief 已学习的几何约束
 *
 * 在 F-loop 中通过 MGG 从冲突中学习的约束。
 * 每条学习约束排除一个不可行的赋值区域。
 */
typedef struct {
    int constraint_id;           /**< 学习约束 ID */
    GvilPredicate *predicate;    /**< 约束谓词（如 |AB| ≤ 6） */
    int conflict_level;          /**< 触发学习的冲突层级 */
    char *explanation;           /**< 学习原因的自然语言描述 */
} LearnedConstraint;

/* ── EF 循环主接口 ────────────────────────────────── */

/**
 * @brief EF 混合求解循环——借鉴 Yices 2 MCSAT
 *
 * 核心用途：判断一个几何存在性命题是否成立，并给出构造。
 *
 * 命题形式：∃P1, P2, ..., Pk. φ(P1, P2, ..., Pk)
 *   其中 φ 是几何约束的合取（如距离、角度、共线等）
 *
 * 工作流程（EF 循环）：
 *
 *   [E-loop — 存在赋值搜索]
 *     while (存在未赋值的几何变量) {
 *         1. 选择下一个要赋值的变量（启发式：最受约束的变量优先）
 *         2. 为该变量选择一个候选值（启发式：满足最多约束的值）
 *         3. 执行理论传播：从当前赋值推理出必然成立的约束
 *         4. 检查当前赋值是否与所有约束一致
 *         5. 如果一致：继续下一个变量
 *         6. 如果不一致：跳转到 F-loop 分析冲突
 *     }
 *     → 所有变量赋值成功：返回 SAT + 构造模型
 *
 *   [F-loop — 冲突分析 + 模型引导泛化]
 *     while (存在冲突) {
 *         1. 分析冲突：找到导致冲突的最小约束子集
 *         2. MGG 泛化：从冲突中学习排除一个区域的约束
 *         3. 将学习约束添加到约束集合
 *         4. 进行平方：如果学习约束直接矛盾，返回 UNSAT
 *         5. 否则：回溯到安全决策层级
 *     }
 *     → 返回 E-loop，在新约束下重新搜索
 *
 * @param goals          存在性命题集合（合取形式）
 * @param variables      待赋值的几何变量列表
 * @param var_count      变量数量
 * @param out_model      输出：满足所有约束的几何模型（SAT 时有效）
 * @param out_trace      输出：求解过程的详细跟踪（用于调试和解释）
 * @param timeout_ms     超时（毫秒），0 表示无限制
 * @return 求解状态
 *
 * @note EF 循环的理论基础：
 *       几何存在性问题等价于非线性实数存在性问题（∃-NRA），
 *       MCSAT 是当前已知最有效的 ∃-NRA 求解算法之一。
 *
 * @see yices2_ef_solving.md —— Yices 2 MCSAT 参考
 */
SolverEFState solver_ef_loop(
    const GvilPredicate **goals,
    GeoVariableValue *variables,
    int var_count,
    GeometricalModel **out_model,
    char **out_trace,
    int timeout_ms
);

/* ── E-loop 子组件 ─────────────────────────────────── */

/**
 * @brief 变数量排序启发式——选择下一个要赋值的几何变量
 *
 * 借鉴 Yices 2 MCSAT 的变量排序策略，Lv-00 适配为几何场景：
 *  - 优先选择约束最多的变量（constrained-first）
 *  - 优先选择自由度数最少的对象（自由度最少优先）
 *  - 打破平局：优先选择距离已知对象最近的对象
 *
 * 自由度分析：
 *  - 自由点：2 自由度（x, y）
 *  - 线段上的点：1 自由度（沿线段移动）
 *  - 圆上的点：1 自由度（沿圆移动）
 *  - 两圆/两线交点：0 自由度（离散候选点）
 *
 * @param variables     所有几何变量
 * @param var_count     变量数量
 * @param constraints   当前约束集合
 * @return 下一个要赋值的变量索引（-1 表示全部已赋值）
 */
int solver_variable_heuristic(
    const GeoVariableValue *variables,
    int var_count,
    const GvilPredicate **constraints,
    int constraint_count
);

/**
 * @brief 值选择启发式——为几何变量选择候选值
 *
 * 借鉴 Yices 2 MCSAT 的值选择策略：
 *  - 精确解优先：如果代数方程可解析求解（如二次方程求根），直接计算
 *  - 牛顿法逼近：如果解析解不可行，使用牛顿法搜索
 *  - 区间约束传播：对不等式约束，使用区间算术缩小搜索范围
 *  - 随机采样：作为最后的回退策略
 *
 * 几何特定启发式：
 *  - 点在线段上：均匀采样线段上的点
 *  - 点在圆上：按角度采样圆周上的点
 *  - 两圆交点：计算解析交点
 *
 * @param variable      目标变量
 * @param constraints   当前约束集合
 * @param num_candidates 需要生成的候选值数量
 * @param out_values    输出：候选值数组
 * @return 实际生成的候选值数量
 */
int solver_value_heuristic(
    const GeoVariableValue *variable,
    const GvilPredicate **constraints,
    int constraint_count,
    int num_candidates,
    GeoVariableValue **out_values
);

/**
 * @brief 几何理论传播——从当前赋值推理必然约束
 *
 * 借鉴 Yices 2 MCSAT 的 theory propagation：
 * 如果当前部分赋值逻辑上蕴含某个约束，则自动推导该约束。
 *
 * 几何传播规则示例：
 *  已知：|AB| = 3, |BC| = 4, ∠ABC = 90°
 *  传播：|AC| = 5（勾股定理）
 *
 *  已知：M = Midpoint(A, B), N = Midpoint(C, D)
 *  传播：如果 M = N，则 A+B = C+D（向量等式）
 *
 *  已知：A, B, C 共线，B 在 A 和 C 之间
 *  传播：|AB| + |BC| = |AC|
 *
 * @param assignments   当前部分赋值
 * @param out_propagated 输出：新推导的约束
 * @param out_count     输出：新推导约束数量
 */
void solver_geometric_deduction(
    const GeoVariableValue *assignments,
    int assignment_count,
    GvilPredicate ***out_propagated,
    int *out_count
);

/**
 * @brief 检查当前赋值是否与所有约束一致
 *
 * 返回不一致的约束索引列表（空列表表示一致）。
 *
 * @param assignments   当前赋值
 * @param constraints   所有约束
 * @param constraint_count 约束数量
 * @param out_conflicts 输出：冲突约束的索引数组
 * @param out_count     输出：冲突约束数量
 */
void solver_check_consistency(
    const GeoVariableValue *assignments,
    int assignment_count,
    const GvilPredicate **constraints,
    int constraint_count,
    int **out_conflicts,
    int *out_count
);

/* ── F-loop 子组件 ─────────────────────────────────── */

/**
 * @brief 冲突分析——找到导致不可行的最小约束子集
 *
 * 借鉴 Yices 2 MCSAT 的 conflict analysis：
 * 从冲突约束集合中提取最小不可行核心（unsatisfiable core）。
 *
 * 几何 UNSAT 核心示例：
 *  约束集：{|AB|=5, |BC|=5, |AC|=12, triangle(A,B,C)}
 *  冲突：三角不等式 |AB| + |BC| < |AC| (5+5 < 12)
 *  最小核心：{|AB|=5, |BC|=5, |AC|=12}（triangle 约束是冗余的）
 *
 * @param conflicts    冲突约束列表
 * @param conflict_count 冲突约束数量
 * @param out_core     输出：最小不可行核心
 * @param out_core_size 输出：核心大小
 */
void solver_unsat_core(
    const GvilPredicate **conflicts,
    int conflict_count,
    GvilPredicate ***out_core,
    int *out_core_size
);

/**
 * @brief 模型引导泛化（MGG）——从反例学习排除区域的约束
 *
 * 借鉴 Yices 2 MCSAT 的 MGG：
 * 从具体的冲突赋值泛化为排除一个区域的约束。
 *
 * 几何 MGG 示例：
 *
 *  冲突场景：给定 |AP|=3, |BP|=3，尝试 P=(2, 0) 不满足
 *   原因：|AP|=√((2-0)^2+(0-0)^2)=2≠3, |BP|=√((2-4)^2+0)=2≠3
 *
 *  MGG 泛化（从点到区域）：
 *   - 约束 |AP|=3 定义了圆 CA: (P.x)^2 + (P.y)^2 = 9
 *   - 约束 |BP|=3 定义了圆 CB: (P.x-4)^2 + (P.y)^2 = 9
 *   - 交集 CA∩CB：
 *     P.x^2 - (P.x-4)^2 = 0 → 8P.x - 16 = 0 → P.x = 2
 *     4 + P.y^2 = 9 → P.y = ±√5
 *   - 学习约束：P 只能在 {(2, √5), (2, -√5)} 两个点上
 *
 *  更一般的泛化：
 *   原始：这个具体的 P 值不行
 *   泛化：只有这两个 P 值行 → P ∈ {(2, √5), (2, -√5)}
 *
 * @param unsat_core      最小不可行核心
 * @param core_size       核心大小
 * @param current_assignment 当前的冲突赋值
 * @param out_learned     输出：学习的约束
 */
void solver_mgg_generalize(
    const GvilPredicate **unsat_core,
    int core_size,
    const GeoVariableValue *current_assignment,
    LearnedConstraint **out_learned
);

/* ── 回溯管理 ───────────────────────────────────────── */

/**
 * @brief 回溯到安全的决策层级
 *
 * 借鉴 Yices 2 MCSAT 的回溯策略：
 * 当 F-loop 学习到新约束后，并不是完全重启，
 * 而是回溯到一个"安全"的决策层级——该层级之上没有
 * 受新约束影响的决策。
 *
 * @param variables      所有变量
 * @param var_count      变量数量
 * @param learned        新学习的约束
 * @return 回溯后的目标决策层级
 */
int solver_backtrack(
    GeoVariableValue *variables,
    int var_count,
    const LearnedConstraint *learned
);

/* ── 结果模型导出 ─────────────────────────────────── */

/**
 * @brief 从 EF 循环的 SAT 结果导出几何模型
 *
 * 将求解器的内部赋值表示转换为用户可读的几何模型。
 * 几何模型包括：
 *  - 所有点的坐标
 *  - 所有线段的端点
 *  - 所有圆的圆心和半径
 *  - 所有角的度量
 *
 * @param assignments    最终的赋值
 * @param var_count      变量数量
 * @return 导出的几何模型（调用者需用 lv00_free 释放）
 */
GeometricalModel *solver_export_model(
    const GeoVariableValue *assignments,
    int var_count
);

/**
 * @brief 释放几何模型
 */
void solver_model_destroy(GeometricalModel *model);

#endif /* LV00_SOLVER_EF_H */
```

### 2.6 MCSAT 与几何约束系统的深度融合设计

在 Lv-00 中，MCSAT 不仅处理一般的代数约束，还需要深度嵌入几何公理体系。这是 Lv-00 与通用 SMT 求解器的最大区别：

**嵌入的几何公理体系**：

```
公理类 1：欧几里得几何基本公理
  - 两点确定一条直线
  - 线段可以任意延长
  - 以任意点为圆心、任意长度为半径可以作圆
  - 所有直角都相等
  - 平行公理（过直线外一点有且只有一条平行线）

公理类 2：距离和角度的代数编码
  - 距离：|AB| = √((B.x-A.x)^2 + (B.y-A.y)^2)
  - 角度：cos(∠ABC) = ((A-B)·(C-B)) / (|AB|·|BC|)
  - 面积：(有向) S_ABC = (1/2)|det(B-A, C-A)|

公理类 3：几何不变量
  - 共线：det(B-A, C-A) = 0
  - 共圆：四点的行列式条件
  - 全等：SSS/SAS/ASA/... 判定定理
  - 相似：边长比例 + 角度相等

公理类 4：三角不等式和其推广
  - |AB| + |BC| ≥ |AC|（等号当且仅当 B 在 AC 之间）
  - 推广：多边形边长不等式链
```

**MCSAT 中几何公理的触发策略**：

```
when (检测到三个点 A, B, C):
  if (∃约束涉及 |AB|, |BC|, |AC|):
    自动添加三角不等式 |AB| + |BC| ≥ |AC|
    （作为 theory propagation 的一部分）

when (检测到中点构造 M = Midpoint(A, B)):
  自动添加：
    collinear(A, M, B)      // M 在 AB 上
    |AM| = |MB|             // M 平分 AB
    M = (A + B) / 2         // 坐标形式

when (检测到角平分线构造 L = AngleBisector(∠ABC)):
  自动添加：
    ∠ABL = ∠LBC             // L 平分角
    点 L 在 ∠ABC 内部
```

---

## 3. 实现方案

### 3.1 第一阶段：基础 EF 循环（P1-1）

- [ ] 实现 `solver_ef_loop()` 的 E-loop 主干流程
- [ ] 实现 `solver_variable_heuristic()` 几何变量排序
- [ ] 实现 `solver_value_heuristic()` 几何值选择（至少支持精确解和牛顿法）
- [ ] 实现 `solver_check_consistency()` 约束一致性检查
- [ ] 实现 F-loop 的冲突检测和基础回溯
- [ ] 编写 EF 循环的基础单元测试（5 个标准几何存在性问题）

### 3.2 第二阶段：MGG 模型引导泛化（P1-2）

- [ ] 实现 `solver_unsat_core()` 最小不可行核心提取
- [ ] 实现 `solver_mgg_generalize()` 从点到区域的泛化
- [ ] 实现几何 MGG 的专门规则：
  - [ ] 距离约束的 MGG（排除圆）
  - [ ] 角度约束的 MGG（排除扇形区域）
  - [ ] 共线约束的 MGG（排除直线区域）
- [ ] 实现 F-loop 的完整回溯策略
- [ ] 编写 MGG 的单元测试

### 3.3 第三阶段：几何理论传播（P1-3）

- [ ] 实现 `solver_geometric_deduction()` 几何理论传播
- [ ] 嵌入欧几里得几何公理体系（15+ 条核心公理）
- [ ] 实现三角不等式的自动触发和传播
- [ ] 实现中点/角平分线/垂线等构造的自动展开
- [ ] 实现几何不变量的传播规则
- [ ] 编写理论传播的单元测试

### 3.4 第四阶段：高级求解策略（P1-4）

- [ ] 实现区间约束传播（对不等式约束使用区间算术）
- [ ] 实现求解器的增量模式（incremental solving）
- [ ] 实现求解器状态检查点（checkpoint/restore）
- [ ] 实现求解过程的详细跟踪和可解释输出
- [ ] 实现求解器的性能分析（决策次数、冲突次数、学习约束数量）
- [ ] 与 `engine_scheduler.h` 集成（作为 SOLVER_NUMERIC_NEWTON 后端）

### 3.5 第五阶段：基准测试与优化（P1-5）

- [ ] 建立几何存在性问题的标准基准库（50+ 问题）
- [ ] 与通用 SMT 求解器（Z3/CVC5）对比测试
- [ ] 分析瓶颈：识别导致最多冲突的约束类型
- [ ] 优化启发式：基于基准数据调整变量排序和值选择策略
- [ ] 编写完整的用户文档

---

## 4. 参考资源

- **Yices 2 官方网站**：[https://yices.csl.sri.com/](https://yices.csl.sri.com/)
- **Yices 2 源码仓库**：[https://github.com/SRI-CSL/yices2](https://github.com/SRI-CSL/yices2)
- **MCSAT 架构论文**：de Moura, Jovanovic. "A Model-Constructing Satisfiability Calculus" (VMCAI 2013)
- **MCSAT 在非线性实数中的应用**：Jovanovic, de Moura. "Solving Non-Linear Arithmetic" (IJCAR 2012)
- **EF 循环的详细描述**：Jovanovic, Barrett, de Moura. "The Design and Implementation of the Model-Constructing Satisfiability Calculus" (FMCAD 2013)
- **模型引导泛化（MGG）**：Jovanovic, de Moura. "Cutting to the Chase: Solving Linear Integer Arithmetic" (JAR 2013)
- **CDCL 基础**：Silva, Lynce, Malik. "Conflict-Driven Clause Learning SAT Solvers" (Handbook of Satisfiability, 2009)
- **DPLL(T) 与 MCSAT 对比**：de Moura, Passmore. "Computation in Real Closed Infinitesimal and Transcendental Extensions of the Rationals" (CADE 2013)
- **几何存在性问题的代数求解**：Chou, Gao, Zhang. "Machine Proofs in Geometry" (1994)
- **三角不等式在约束求解中的角色**：Marriott, Stuckey. "Programming with Constraints: An Introduction" (1998)
- **本系列相关文档**：
  - `fstar_refinement_smt.md` —— F* 精化类型 + SMT 混合验证
  - `why3_multi_prover_dispatch.md` —— Why3 多求解器分派
  - `dafny_layered_verification.md` —— Dafny 三层验证架构
  - `rosette_symbolic_vm.md` —— Rosette 符号虚拟机

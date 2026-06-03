# Lv-00 参考设计：OR-Tools CP-SAT 约束优化

> **版本**: 1.0.0
> **日期**: 2026-05-24
> **参考**: [Google OR-Tools CP-SAT](https://developers.google.com/optimization/cp/cp_solver) —— Google 的约束规划与 SAT 混合求解器
> **目标**: 借鉴 CP-SAT 的"约束满足 → 目标优化"双阶段范式，将 Lv-00 从纯几何约束满足（CSP）扩展到在约束条件下寻找满足附加目标的最优构造（COP），映射到 `constraint_graph.h` + `solver.h`

---

## 目录

1. [项目概述](#1-项目概述)
2. [核心借鉴要点](#2-核心借鉴要点)
3. [Lv-00 映射方案](#3-lv-00-映射方案)
4. [实现路线图](#4-实现路线图)

---

## 1. 项目概述

### 1.1 OR-Tools CP-SAT 是什么

Google OR-Tools 的 CP-SAT 求解器是将约束规划（Constraint Programming, CP）与 SAT（Boolean Satisfiability）求解技术融合的高性能组合优化引擎。它被广泛用于车辆路径规划、排班调度、装箱问题等场景。CP-SAT 的核心范式是**"约束+目标函数"双阶段求解**：

```python
# CP-SAT 示例：在约束下最大化矩形面积
from ortools.sat.python import cp_model

model = cp_model.CpModel()

# 阶段1：定义变量与约束（CSP）
x = model.NewIntVar(0, 100, 'x')      # 矩形宽
y = model.NewIntVar(0, 100, 'y')      # 矩形高
model.Add(x + y <= 100)               # 周长约束
model.Add(x >= 10)                     # 最小宽度

# 阶段2：定义目标函数（COP）
model.Maximize(x * y)                  # 最大化面积

solver = cp_model.CpSolver()
status = solver.Solve(model)           # 求解 → x=50, y=50, area=2500
```

CP-SAT 的关键机制：

1. **变量域（Domain）**：每个变量有一个初始取值范围，在求解过程中逐步缩小
2. **约束传播（Constraint Propagation）**：当一个变量的域被缩小，自动传播到相关变量
3. **目标函数（Objective）**：在满足约束的前提下，寻找使目标函数最优的变量赋值
4. **BAB 搜索（Branch and Bound）**：通过分支定界在最优化搜索中剪枝

### 1.2 为什么借鉴 CP-SAT

Lv-00 当前的 `solver.h` 和 `constraint_graph.h` 已经实现了纯几何约束满足（CSP）——给定点、线、圆之间的约束关系，求解满足所有约束的坐标。但 Lv-00 尚未支持"在约束条件下寻找最优构造"，即缺乏 CSP 向 COP（Constraint Optimization Problem）的扩展。借鉴 CP-SAT 意味着：

1. 用户不仅能问"这个三角形存在吗"，还能问"在所有满足约束的三角形中，哪个面积最大"
2. 引入**目标函数**概念：面积最大化、周长最小化、角度最优化等
3. 复用现有的 `ConstraintGraph` 作为 CSP 层，在其上叠加目标函数定义与优化搜索

---

## 2. 核心借鉴要点

### 2.1 "约束+目标函数"双阶段范式

| CP-SAT 概念 | Lv-00 对应概念 | 映射说明 |
|-------------|---------------|---------|
| `NewIntVar(lb, ub, name)` | `SymbolicCoord` + 域约束 `[lb, ub]` | 几何点的坐标变量天然有值域 |
| `model.Add(constraint)` | `constraint_graph_add_incidence()` 等 | 每个 CP-SAT 约束 = ConstraintGraph 中的 Constraint |
| `model.Maximize(objective)` | `optimization_objective_set(graph, OBJ_MAX, expr)` | Lv-00 新增的目标函数描述 |
| `solver.Solve(model)` | `solver_optimize(graph, objective)` | 在 CSP 求解后叠加优化搜索 |
| 约束传播 | 已在 `ConstraintGraph` 中实现（INCIDENCE → 关联传播） | Lv-00 的图结构天然支持传播 |
| BAB 搜索 | `optimization_branch_and_bound()` | 新增的分支定界搜索树 |
| 变量域缩小 | `SymbolicCoord` 的区间约束 | `COORD_RATIONAL` 类型天然可表达域 |

### 2.2 CP-SAT 的优化搜索策略

| 策略 | CP-SAT 实现 | Lv-00 对应 |
|------|------------|-----------|
| **可行性泵（Feasibility Pump）** | 快速寻找初始可行解 | `solver_find_initial_solution()` —— Gröbner 基求解 |
| **LNS（Large Neighborhood Search）** | 局部扰动重解 | `solver_incremental_solve()` 的脏变量子图重解 |
| **线性松弛（Linear Relaxation）** | LP 松弛为 BAB 提供下界 | 几何约束的线性化近似（将圆约束近似为多边形） |
| **对称性破缺（Symmetry Breaking）** | 消除等价解的冗余搜索 | 选择器 `selector=pos_root` 的扩展 —— 自动注入排序约束 |
| **no-good 学习** | 记录失败子赋值，避免重复搜索 | `ConstraintGraph` 的冲突检测 → `graph_detect_conflicts()` |

### 2.3 CSP → COP 的跃迁路径

```
传统 Lv-00 CSP 流程:
  几何构造 → ConstraintGraph → solver_solve() → 解（或报告无解）

扩展后的 COP 流程:
  几何构造 → ConstraintGraph
          ↓
          附加目标函数（max/min）
          ↓
          solver_optimize()
          ↓
  ┌─── BAB 搜索树 ───────────────────────┐
  │  1. 初始可行解（Groebner/SMT）         │
  │  2. 约束传播 → 变量域缩小              │
  │  3. 分支：选择一个未定变量，分叉搜索    │
  │  4. 定界：线性松弛计算上/下界          │
  │  5. 剪枝：不可行分支 / 劣于当前最优解   │
  │  6. 返回全局最优构造                   │
  └──────────────────────────────────────┘
```

---

## 3. Lv-00 映射方案

### 3.1 目标函数的几何语义

在 Lv-00 中，目标函数由几何量表达。以下是在几何构造中最常见的优化目标：

| 优化目标 | 几何语义 | Lv-00 表达式 |
|---------|---------|-------------|
| 最大化三角形面积 | max Area(ABC) | `max(triangle_area(A, B, C))` |
| 最大化三角形面积（固定周长） | 等周问题 | `max(area) subject to perimeter = 100` |
| 最小化点到直线距离 | 最佳拟合线 | `min(distance(P, line))` |
| 最大化内切圆半径 | 在给定三角形中 | `max(incircle_radius(A, B, C))` |
| 最小化构造的坐标总数 | 最少点构造 | `min(count(points_in_construction))` |

### 3.2 目标函数到 SymbolicCoord 的映射

```c
/**
 * @brief 优化目标类型枚举（借鉴 CP-SAT 的 Maximize/Minimize）
 *
 * 目标函数是对几何量的代数表达式，其值由 SymbolicCoord 计算得出。
 * 类型字段指定优化方向。
 */
typedef enum {
    OBJ_MAXIMIZE,           /**< 最大化目标 */
    OBJ_MINIMIZE            /**< 最小化目标 */
} OptimizationDirection;

/**
 * @brief 优化目标函数描述
 *
 * 目标函数由一个代数表达式树定义，叶节点引用 ConstraintGraph 中的
 * SymbolicCoord 节点（如点的 x/y 坐标）或常量。
 *
 * 表达式类型覆盖几何优化中的常见需求：
 * - 面积表达式（基于顶点坐标的 Shoelace 公式）
 * - 距离表达式（欧氏距离的平方或平方根）
 * - 角度表达式（向量夹角的余弦/正弦函数）
 * - 计数表达式（满足某约束的元素个数）
 */
typedef enum {
    OBJ_EXPR_AREA,          /**< 面积表达式：如 triangle_area(A,B,C) */
    OBJ_EXPR_DISTANCE,      /**< 距离表达式：如 distance(P, Q) */
    OBJ_EXPR_PERIMETER,     /**< 周长表达式：如 perimeter(A,B,C,...) */
    OBJ_EXPR_ANGLE,         /**< 角度表达式：如 angle(A, O, B) */
    OBJ_EXPR_RADIUS,        /**< 半径表达式：如 incircle_radius(A,B,C) */
    OBJ_EXPR_COUNT,         /**< 计数表达式：如 count(constraint_type) */
    OBJ_EXPR_CUSTOM         /**< 自定义符号表达式 */
} ObjExprType;

/**
 * @brief 优化目标结构体
 *
 * 封装一个几何优化目标：在满足 ConstraintGraph 中所有约束的前提下，
 * 最优化（max/min）一个几何量表达式。
 */
typedef struct OptimizationObjective {
    int id;                           /**< 目标 ID */
    OptimizationDirection direction;  /**< 优化方向 */
    ObjExprType expr_type;            /**< 表达式类型 */
    int *var_node_ids;                /**< 表达式引用的变量节点 ID 数组 */
    int var_count;                    /**< 变量节点数量 */
    char *expr_formula;               /**< 表达式的文本形式（用于序列化和调试） */
    int target_constraint_id;         /**< 关联的约束 ID（可选，如"固定周长=100"） */
} OptimizationObjective;
```

### 3.3 核心 API

```c
/**
 * @brief 创建优化目标
 *
 * 将一个几何表达式（如面积、距离）注册为优化目标。
 * 对应 CP-SAT 的 `model.Maximize(expr)` 或 `model.Minimize(expr)`。
 *
 * @param[in] direction     优化方向
 * @param[in] expr_type     表达式类型
 * @param[in] var_node_ids  变量节点 ID 数组
 * @param[in] var_count     变量数量
 * @param[in] expr_formula  公式字符串（如 "triangle_area(A,B,C)"）
 * @return 新分配的 OptimizationObjective，失败返回 NULL
 */
OptimizationObjective *optimization_objective_create(
    OptimizationDirection direction,
    ObjExprType expr_type,
    const int *var_node_ids,
    int var_count,
    const char *expr_formula);

/**
 * @brief 将优化目标附加到约束图
 *
 * 将目标注册到 ConstraintGraph 的可选扩展槽位中。
 * 该操作不会影响现有的 CSP 求解流程。
 */
void optimization_objective_attach(ConstraintGraph *graph,
                                    OptimizationObjective *obj);

/**
 * @brief 在约束图中执行优化求解
 *
 * 这是 CPS→COP 跃迁的核心函数。
 *
 * 流程：
 *  1. 调用 solver_solve() 寻找初始可行解（Groebner/SMT）
 *  2. 计算初始目标值
 *  3. 进入 BAB 搜索主循环：
 *     a. 分支：选择一个未定变量，二分其域
 *     b. 传播：约束传播缩小变量域
 *     c. 定界：线性松弛计算上界/下界
 *     d. 剪枝：舍弃不可行分支和劣于当前最优的分支
 *  4. 返回全局最优构造的坐标赋值
 *
 * @param[in]  graph        已附加优化目标的约束图
 * @param[in]  objective    要优化的目标
 * @param[out] out_result   输出最优解
 * @return 求解器状态
 */
SolverStatus solver_optimize(ConstraintGraph *graph,
                              const OptimizationObjective *objective,
                              GroebnerResult **out_result);

/**
 * @brief BAB 搜索树节点
 *
 * 每个节点代表搜索过程中的一个分支状态，
 * 包含当前变量赋值快照和该分支的上下界。
 */
typedef struct BABNode {
    ConstraintGraph *snapshot;        /**< 当前分支的约束图快照 */
    double lower_bound;               /**< 当前分支的最优下界 */
    double upper_bound;               /**< 当前分支的上界（松弛解） */
    int branch_var_id;                /**< 在此节点上分支的变量 ID */
    int depth;                        /**< 搜索树深度 */
    bool is_pruned;                   /**< 是否已被剪枝 */
    struct BABNode *parent;           /**< 父节点 */
    struct BABNode **children;        /**< 子分支 */
    int child_count;
} BABNode;

/**
 * @brief 释放优化目标
 */
void optimization_objective_destroy(OptimizationObjective *obj);
```

### 3.4 映射到现有 constraint_graph.h + solver.h

| 现有 API / 结构 | 在优化扩展中的角色 |
|----------------|-------------------|
| `ConstraintGraph` | CSP 层：存储约束和目标函数的容器 |
| `graph_add_incidence()` 等 | 约束构造：CSP 阶段的基础约束 |
| `solve_algebraic_system()` | 初始可行解：COP 启动所需的第一个解 |
| `solver_incremental_solve()` | BAB 的增量重解：分支后仅重解脏变量 |
| `solver_feedback_solve()` | 交互式反馈：优化过程中实时报告进展 |
| `graph_detect_conflicts()` | BAB 剪枝：检测分支的不可行性 |
| `count_degrees_of_freedom()` | 分支选择：优先在自由度高的变量上分支 |
| `SymbolicCoord` | 变量的域表示：BAB 的分支点 |
| `GroebnerResult` | 解的输出格式：最优坐标赋值 |
| `engine_scheduler.h` | 多后端路由：选择 Gröbner 还是 SMT 作为优化后端 |

### 3.5 DSL 中的优化语法

```
// ============================================================
// Lv-00 COP DSL: 约束→优化扩展
// 借鉴 CP-SAT 的 Maximize/Minimize 范式
// ============================================================

// --- 基础构造（CSP） ---
point A(0, 0);
point B(10, 0);
point C(xc, yc);              // C 的坐标未定（符号变量）

triangle T = triangle(A, B, C);

// --- 约束条件 ---
constraint: perimeter(T) = 30;   // 固定周长 = 30

// --- 目标函数（COP） ---
optimize maximize area(T);       // 在约束下最大化面积

// --- 求解 ---
// Lv-00 自动执行 CPS→COP 管线：
// 1. Gröbner 基求初始可行解
// 2. BAB 搜索最优 C 点位置
// 3. 输出: C = (5, 5*sqrt(3))，面积 = 25*sqrt(3)/4

// --- 复合优化 ---
optimize minimize distance(C, line(A, B))
        subject to area(T) >= 30;
```

---

## 4. 实现路线图

### 4.1 第一阶段：目标函数基础结构（P3）

| 任务 | 文件 | 说明 |
|------|------|------|
| 定义 `OptimizationObjective`、`ObjExprType` | `include/lv00/optimization.h`（新文件） | 目标函数核心数据结构 |
| 实现 `optimization_objective_create/destroy/attach` | `src/optimization.c`（新文件） | 目标函数的创建与注册 |
| 实现几何表达式 → `SymbolicCoord` 求值 | `src/optimization_expr.c`（新文件） | 面积/距离/半径等表达式的符号求值 |
| 在 `ConstraintGraph` 中添加可选的目标函数槽位 | `include/lv00/constraint_graph.h` | 小改动：增加 `OptimizationObjective *opt_obj` 字段 |

**预估规模**：约 350 行 C 代码

### 4.2 第二阶段：BAB 搜索树（P3）

| 任务 | 文件 | 说明 |
|------|------|------|
| 实现 `BABNode` 和 BAB 搜索树 | `src/optimization_bab.c`（新文件） | 分支定界搜索树的核心逻辑 |
| 实现变量选择启发式 | `src/optimization_bab.c` | 基于自由度/域大小的分支变量选择 |
| 实现线性松弛定界 | `src/optimization_relax.c`（新文件） | 几何约束的线性近似 → LP 求解 |
| 实现剪枝逻辑 | `src/optimization_bab.c` | 不可行性检测 + 劣于当前最优解剪枝 |

**预估规模**：约 400 行 C 代码

### 4.3 第三阶段：完整集成（P3-P4）

| 任务 | 说明 |
|------|------|
| 实现 `solver_optimize()` 主函数 | 整合 Gr笔者bner 初始解 + BAB 搜索 |
| 与 `engine_scheduler.h` 集成 | 优化调度：自动选择后端策略 |
| DSL 语法扩展 | `optimize maximize/minimize` 关键字的词法/语法支持 |
| Web GUI 优化面板 | 在 ProofPanel 旁新增 OptimizationPanel，展示目标值和搜索树 |
| 增量优化（LNS 风格） | 用户修改约束后仅重优化受影响变量 |

---

## 附录 A：CP-SAT 与 Lv-00 概念对照速查

| CP-SAT | Lv-00 | 关键差异 |
|--------|-------|---------|
| `IntVar(0, 100)` | `SymbolicCoord` 的区间约束 | Lv-00 支持精确代数数而不仅是整数 |
| `model.Add(x + y <= 100)` | `constraint_graph_add_containment()` | Lv-00 约束更丰富（INCIDENCE/BETWEENNESS 等） |
| `model.Maximize(x * y)` | `optimization_objective_create(OBJ_MAX, ...)` | Lv-00 目标基于几何量 |
| `solver.Solve(model)` | `solver_optimize(graph, obj)` | Lv-00 多了 Groebner 基 + SMT 双后端 |
| `solver.BestObjectiveBound()` | BAB 节点的 `upper_bound` | Lv-00 的界来自线性松弛或 Gröbner 基推断 |
| `solver.NumBooleans()` / `NumConflicts()` | `SchedulerStats` 的 `fallback_count` / 冲突统计 | Lv-00 通过 SchedulerStats 提供类似诊断信息 |
| 对称性破缺 | `selector` 系统 + 排序约束注入 | Lv-00 的 selector 可扩展来自动注入 symmetry-breaking 约束 |

---

## 附录 B：COP 示例——最大面积三角形的元语言描述

```
命题: 固定周长 P=30 的三角形中，哪个面积最大？
─────────────────────────────────────
CSP 层:
  A = (0, 0)
  B = (x_B, 0)    ← 符号变量
  C = (x_C, y_C)  ← 符号变量
  约束:
    distance(A, B) + distance(B, C) + distance(C, A) = 30

COP 层:
  目标: maximize triangle_area(A, B, C)

BAB 搜索:
  初始解（随机选择）: B=(10,0), C=(5, 8.66), area=43.3
  分支: 在 y_C 上分支 [0, 15] → [0, 7.5] + [7.5, 15]
  传播: 右分支面积更大（C 离 AB 更远）
  定界: 面积上界 ≈ 64.95（等边三角形，由等周不等式给出）
  剪枝: 面积 < 43.3 的分支全部剪掉
  最优解: B=(10,0), C=(5, 5√3), area=25√3 ≈ 43.3

验证: 这是等边三角形，边长=10。等周不等式已知"固定周长下等边三角形面积最大"。
```

---

> **文档结束**
> 本文档详述了 Google OR-Tools CP-SAT 的"约束+目标函数"双阶段范式如何应用于 Lv-00——从纯几何约束满足（CSP）跃迁到约束优化（COP）。核心结论：通过在 `ConstraintGraph` 上叠加 `OptimizationObjective` 目标函数结构，并引入 BAB 分支定界搜索树，Lv-00 可以支持"在约束条件下寻找满足附加目标的最优构造"（如最大面积三角形、最短距离构造等），映射到现有的 `constraint_graph.h` 和 `solver.h` 之上。

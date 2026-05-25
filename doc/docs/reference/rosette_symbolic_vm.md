# Lv-00 参考设计：Rosette 符号虚拟机

> **版本**: 1.0.0  
> **日期**: 2026-05-24  
> **参考**: [Rosette](https://github.com/emina/rosette) —— 基于 Racket 的求解器增强型编程语言  
> **目标**: 借鉴 Rosette 的"符号虚拟机"概念，应用于 Lv-00——将几何构造"符号化"执行后交给 Z3 求解，映射到现有的 `solver.h` + `smt_backend.h` 架构

---

## 目录

1. [项目概述](#1-项目概述)
2. [核心借鉴要点](#2-核心借鉴要点)
3. [Lv-00 映射方案](#3-lv-00-映射方案)
4. [实现路线图](#4-实现路线图)

---

## 1. 项目概述

### 1.1 Rosette 是什么

Rosette 是 UC Berkeley 的 Emina Torlak 团队开发的求解器增强型编程语言（Solver-Aided Programming Language），嵌入在 Racket 中。它的核心创新是**符号虚拟机（Symbolic Virtual Machine）**——通过为 Racket 程序赋予符号求值能力，使得普通程序可以同时处理具体值和符号值，最终将符号约束交给 SMT 求解器（如 Z3）求解。

```racket
; Rosette 示例：符号化执行一个简单函数
(define-symbolic x integer?)  ; x 是符号变量
(define c (+ x 10))           ; c = x + 10（符号表达式）
(assert (> c 20))             ; 断言 c > 20
(solve (list x))              ; 求解 → (model [x 11])
```

Rosette 的关键机制：

1. **符号值（Symbolic Value）**：一个变量可以是具体的 `5`，也可以是符号的 `x`（代表任意整数）
2. **符号求值（Symbolic Evaluation）**：程序在符号变量上执行时，产生符号表达式而非具体结果
3. **路径条件收集（Path Condition Collection）**：`if` 分支的两条路径分别符号化执行，路径条件被累积为断言
4. **求解器查询（Solver Query）**：将符号表达式和路径条件整体交给 Z3/cvc5 求解

### 1.2 为什么借鉴 Rosette

Lv-00 的几何构造天然适合符号化执行。当前 `solver.h` 的 Gröbner 基方法处理纯代数方程，`smt_backend.h` 提供 Z3/cvc5 接口，但二者之间缺乏 Rosette 式的"符号化执行桥接层"。当用户写出 `point D = intersection(line_a, circle_k)` 时，交点 D 的坐标并非直接已知——它应当先被符号化执行，收集关联约束，再交给 SMT 求解器统一求解。

---

## 2. 核心借鉴要点

### 2.1 符号虚拟机三要素

| Rosette 概念 | Lv-00 对应概念 | 映射说明 |
|-------------|---------------|---------|
| 符号变量 `define-symbolic` | `SymbolicCoord`（`COORD_*` 未定值） | 未赋值的符号坐标即符号变量 |
| 符号表达式（symbolic term） | `Algebraic` 类型的 `minimal_poly` | 代数数的最小多项式即符号表达式 |
| 路径条件（path condition） | `ConstraintGraph` 中的 `Constraint` 边 | 每个几何约束 = 一个路径条件断言 |
| `assert` 语句 | 调用 `constraint_graph_add_constraint()` | 将条件添加到约束系统 |
| `solve` 查询 | `smtbackend_solve()` | 将约束系统编码为 SMT-LIB2 并求解 |
| 具体/符号混合执行 | `SymbolicCoord` 的 `RATIONAL` vs `ALGEBRAIC` | 已知数值坐标（具体） vs 待求解符号坐标（符号） |
| 选择器 `selector` | Rosette 的多解分派 | 对应 Rosette 的 `solve` + `evaluate` 组合 |

### 2.2 符号化执行的三个层次

| 层次 | Rosette 表达 | Lv-00 对应 |
|------|-------------|-----------|
| **符号构造** | `(define-symbolic p geometry?)` | `point P(x, y)` 其中 `x, y` 为未赋值的 `SymbolicCoord` |
| **符号约束** | `(assert (intersects? D line circle))` | `constraint_create(INTERSECTION, D, line, circle)` |
| **符号求解** | `(solve (list D.x D.y))` | `scheduler_solve(graph)` → Z3/cvc5 求解 |

---

## 3. Lv-00 映射方案

### 3.1 符号执行桥接层

借鉴 Rosette 的符号虚拟机，在 `solver.h` 和 `smt_backend.h` 之间引入一个符号执行桥接层 `symbolic_exec.h`：

```
Lv-00 符号执行管线（Rosette 风格）:

  DSL 构造语句
      │
      ▼
┌──────────────────────────────────────────┐
│  1. 符号化 (Symbolize)                   │
│     · 未赋值的点 → SymbolicCoord(x_sym)  │
│     · 线段/圆/区域 → 带参数的符号结构     │
│     · 参数化坐标 → 符号变量表             │
└──────────────────┬───────────────────────┘
                   │ 符号变量集合
                   ▼
┌──────────────────────────────────────────┐
│  2. 路径条件收集 (Collect Constraints)   │
│     · 遍历 ConstraintGraph               │
│     · INCIDENCE → 共线方程                │
│     · BETWEENNESS → 有序方程 + 不等式     │
│     · INTERSECTION → 联立方程             │
│     · CONTAINMENT → 包含条件不等式         │
│     · CONNECTION → 数据流约束             │
└──────────────────┬───────────────────────┘
                   │ 约束的符号表达式集合
                   ▼
┌──────────────────────────────────────────┐
│  3. 编码 (Encode)                        │
│     · smtencode_constraint_graph_        │
│       to_smtlib2()                        │
│     · 约束表达式 → SMT-LIB2 断言          │
│     · 符号变量 → SMT 变量声明             │
└──────────────────┬───────────────────────┘
                   │ SMT-LIB2 字符串 / 原生表示
                   ▼
┌──────────────────────────────────────────┐
│  4. 求解 (Solve)                         │
│     · Z3 / cvc5 / Singular               │
│     · smtsolver_check()                  │
│     · 提取模型 → SymbolicCoord[]          │
└──────────────────┬───────────────────────┘
                   │ 求解模型
                   ▼
┌──────────────────────────────────────────┐
│  5. 实例化 (Instantiate)                 │
│     · 将 SMT 模型赋值回 SymbolicCoord     │
│     · symbolic_coord_from_rational()     │
│     · 更新 GeomNode.symbolic_coords[]    │
└──────────────────────────────────────────┘
```

### 3.2 符号化执行的核心数据结构

```c
/**
 * @brief 符号变量表 —— 对应 Rosette 的符号变量环境
 *
 * 在符号化执行过程中，将约束图中的"未知点坐标"映射为 SMT 变量名。
 * 一个几何点 (x, y) 对应两个 SMT 变量，如 "A_x" 和 "A_y"。
 */
typedef struct SymbolicVarTable {
    int *node_ids;              /* 原始几何节点 ID */
    char **smt_var_names;       /* 对应的 SMT 变量名（如 "A_x", "A_y"） */
    int *coord_indices;         /* 坐标维度索引（0=x, 1=y, ...） */
    int count;                  /* 变量表条目数 */
    int capacity;
} SymbolicVarTable;

/**
 * @brief 符号执行上下文 —— 对应 Rosette 的符号虚拟机状态
 *
 * 维护符号化执行的完整状态，包括：
 * - 符号变量映射表
 * - 已收集的约束表达式（路径条件）
 * - 选择器信息（多解场景）
 */
typedef struct SymbolicExecCtx {
    SymbolicVarTable *var_table;        /* 符号变量 → SMT 变量映射 */
    ConstraintGraph *graph;             /* 源约束图（只读引用） */
    SolverBackendType preferred_backend;/* 首选求解后端 */
    bool collect_path_conditions;       /* 是否收集 if/else 分支条件 */
    int path_condition_count;           /* 已收集的路径条件数 */

    /* 内部状态：当前符号化执行路径 */
    struct {
        char **assertions;              /* 等价于 Rosette 的 (assert ...) */
        int assertion_count;
        int assertion_capacity;
    } path;
} SymbolicExecCtx;
```

### 3.3 核心 API

```c
/**
 * @brief 创建符号执行上下文（Rosette 风格）
 *
 * 初始化符号变量表，将约束图中的未定值点坐标注册为符号变量。
 * 对应 Rosette 的 (define-symbolic ...) 语句。
 */
SymbolicExecCtx *symbolic_exec_create(ConstraintGraph *graph);

/**
 * @brief 符号化执行一个几何构造步骤
 *
 * 对给定的 ProofStep 进行符号求值。
 * - 如果是 ADD_NODE: 将新节点注册到 var_table
 * - 如果是 ADD_CONSTRAINT: 将约束转化为 SMT-LIB2 格式的断言
 *
 * 对应 Rosette 中执行一个程序语句并收集路径条件。
 */
void symbolic_exec_step(SymbolicExecCtx *ctx, const ProofStep *step);

/**
 * @brief 收集当前所有约束为 SMT-LIB2 断言
 *
 * 遍历 ConstraintGraph 中所有非冗余约束，
 * 将每种约束类型（INCIDENCE/BETWEENNESS/INTERSECTION/...）
 * 转化为相应的 SMT-LIB2 表达式并加入路径条件。
 *
 * 对应 Rosette 中到达 (solve) 调用时自动收集的所有 assert。
 */
int symbolic_exec_collect_assertions(SymbolicExecCtx *ctx);

/**
 * @brief 求解符号化约束系统
 *
 * 1. 将 var_table + assertions 编码为完整的 SMT-LIB2 脚本
 * 2. 调用 backend（Z3/cvc5）求解
 * 3. 如果多解，根据 selector 选择最优解
 * 4. 将解映射回 SymbolicCoord 并写回 ConstraintGraph
 *
 * 对应 Rosette 的 (solve) + (evaluate)
 */
SMTSolverResult *symbolic_exec_solve(SymbolicExecCtx *ctx,
                                     const SolutionSelector *selector);

/**
 * @brief 销毁符号执行上下文
 */
void symbolic_exec_destroy(SymbolicExecCtx *ctx);
```

### 3.4 映射到现有 solver.h + smt_backend.h

| 现有 API | 在符号虚拟机中的角色 |
|---------|-------------------|
| `solve_algebraic_system()` | 传统 Gröbner 路径（度数≤2 问题时作为快速回退） |
| `smtencode_constraint_graph_to_smtlib2()` | 符号执行管线第 3 步：编码 |
| `smtsolver_create()` / `smtsolver_encode()` / `smtsolver_check()` | 符号执行管线第 4 步：求解 |
| `smtsolver_decode_result()` | 符号执行管线第 5 步：结果的逆编码 |
| `GroebnerResult` | 与传统求解路径的兼容接口 |
| `SMTSolverResult` | 符号化执行求解的输出类型 |
| `scheduler_select_backend()` | 决定走 Gröbner 还是 SMT 路径的决策点 |

### 3.5 几何构造符号化的具体示例

考虑经典场景：构造三角形的外心。

```
DSL:  triangle ABC(A(0,0), B(6,0), C(3,4));
      point O = circumcenter(A, B, C);
```

**符号化执行过程：**

```c
// 步骤 1: 符号化 A, B, C
// A = (0, 0)   → 具体值，无需符号化
// B = (6, 0)   → 具体值
// C = (3, 4)   → 具体值
// O = (O_x, O_y) → 符号变量: smt_var["O_x"], smt_var["O_y"]

// 步骤 2: 收集约束（外心 = 到三顶点距离相等）
// |O - A|^2 = |O - B|^2
// |O - B|^2 = |O - C|^2
// 展开为多项式方程:
//   (O_x - 0)^2 + (O_y - 0)^2 = (O_x - 6)^2 + (O_y - 0)^2
//   (O_x - 6)^2 + (O_y - 0)^2 = (O_x - 3)^2 + (O_y - 4)^2

// 步骤 3: 编码为 SMT-LIB2
// (set-logic QF_NRA)
// (declare-fun O_x () Real)
// (declare-fun O_y () Real)
// (assert (= (+ (^ O_x 2) (^ O_y 2))
//            (+ (^ (- O_x 6) 2) (^ O_y 2))))
// (assert (= (+ (^ (- O_x 6) 2) (^ O_y 2))
//            (+ (^ (- O_x 3) 2) (^ (- O_y 4) 2))))
// (check-sat)
// (get-model)

// 步骤 4: Z3 求解 → O_x = 3, O_y = 2

// 步骤 5: 实例化回 SymbolicCoord
// O → SymbolicCoord {type=RATIONAL, value=3/1, 2/1}
```

### 3.6 选择器与多解的符号化处理

Rosette 的符号虚拟机在遇到 `if` 分支时，会分叉出两条符号路径，每条路径累积各自的路径条件。Lv-00 的"多解选择器"面临类似挑战：

```c
/**
 * @brief 处理多解场景（Rosette 风格的分叉执行）
 *
 * 对于一个有多种可能解的几何构造（如圆与直线的两个交点），
 * 生成两条符号路径，每条路径累积各自的"选择器条件"作为额外断言。
 *
 * 路径 A: selector=pos_root  → 加入 (assert (> y (+ x 1)))
 * 路径 B: selector=neg_root  → 加入 (assert (< y (+ x 1)))
 *
 * 最终根据用户选择的 selector 取相应路径的解。
 */
int symbolic_exec_fork_paths(SymbolicExecCtx *ctx,
                             const Constraint *multi_solution_constraint,
                             const SolutionSelector *selectors,
                             int selector_count);
```

---

## 4. 实现路线图

### 4.1 第一阶段：符号执行桥接层（P2）

| 任务 | 文件 | 说明 |
|------|------|------|
| 定义 `SymbolicVarTable`、`SymbolicExecCtx` | `include/lv00/symbolic_exec.h`（新文件） | 核心数据结构 |
| 实现 `symbolic_exec_create/destroy` | `src/symbolic_exec.c`（新文件） | 创建/销毁符号执行上下文 |
| 实现 `symbolic_exec_step()` | `src/symbolic_exec.c` | 单步符号化执行 |
| 实现 `symbolic_exec_collect_assertions()` | `src/symbolic_exec.c` | 约束→SMT-LIB2 断言的批量收集 |

**预估规模**：约 300 行 C 代码

### 4.2 第二阶段：约束类型→SMT 编码映射（P2）

| 任务 | 文件 | 说明 |
|------|------|------|
| INCIDENCE → 共线/共面方程编码 | `src/smt_encode_incidence.c`（新文件） | 点在线上 = 线性/二次方程 |
| BETWEENNESS → 有序条件 + 共线性编码 | `src/smt_encode_betweenness.c`（新文件） | B在A和C之间 = 比例关系 + 不等式 |
| INTERSECTION → 联立方程编码 | `src/smt_encode_intersection.c`（新文件） | 两对象相交 = 方程组 |
| CONTAINMENT → 包含条件编码 | `src/smt_encode_containment.c`（新文件） | 点在区域内 = 边界不等式 |
| 集成到 `smtencode_constraint_graph_to_smtlib2()` | `src/smt_encode.c` | 添加对全约束类型的编码支持 |

**预估规模**：约 400 行 C 代码

### 4.3 第三阶段：完整集成（P2-P3）

| 任务 | 说明 |
|------|------|
| 在 `engine_scheduler.cpp` 中集成符号执行路径 | 当 SMT 路径可用时，优先走符号执行管线 |
| 实现 `symbolic_exec_fork_paths()` | 处理多解选择器的分叉执行 |
| 增量符号执行 | 仅重新符号化脏变量（类似 `solve_algebraic_system` 的 `dirty_variable_ids`） |
| 符号执行缓存的 invalidation | 当约束图修改时，增量更新符号变量表和断言集合 |

---

## 附录 A：Rosette 与 Lv-00 概念对照速查

| Rosette | Lv-00 | 关键差异 |
|---------|-------|---------|
| Racket 宿主语言 | C 实现（无 Lisp 层） | Lv-00 不需要宿主语言层，直接操作 C 结构 |
| `define-symbolic x integer?` | `SymbolicCoord` 的 `ALGEBRAIC` 类型 | Lv-00 的符号值携带了最小多项式 |
| `(assert (> x 10))` | `constraint_graph_add_constraint(g, c)` | Lv-00 的约束是图边而不是线性断言列表 |
| `(solve)` | `scheduler_solve(g)` | Lv-00 多了一个调度层（自动选择后端） |
| `(evaluate expr)` | 从 `GroebnerResult`/`SMTSolverResult` 读取解 | Lv-00 的解绑定回 `SymbolicCoord` |
| 路径条件隐式收集 | 显式遍历 `ConstraintGraph` | Lv-00 更显式，因为图结构已经包含了所有条件 |

---

## 附录 B：符号虚拟机执行流程图

```
┌──────────────┐
│ DSL 构造语句  │
└──────┬───────┘
       │
       ▼
┌──────────────────────────────────────────────┐
│      SymbolicExecCtx (Rosette VM 等价层)      │
│                                              │
│  ┌─────────────────────────────────────────┐│
│  │ symbolic_exec_step(ADD_NODE)            ││
│  │   · 创建 GeomNode                       ││
│  │   · 将未知坐标注册为 SMT 变量            ││
│  │   · 将已知坐标保留为 RATIONAL SymbolicCoord││
│  └─────────────────────────────────────────┘│
│                                              │
│  ┌─────────────────────────────────────────┐│
│  │ symbolic_exec_step(ADD_CONSTRAINT)       ││
│  │   · 约束类型 → SMT-LIB2 断言             ││
│  │   · INCIDENCE → (= ...) 或 (and ...)    ││
│  │   · BETWEENNESS → (= ...) + (<= ...)    ││
│  │   · 收集到 ctx.path.assertions[]        ││
│  └─────────────────────────────────────────┘│
│                                              │
│  ┌─────────────────────────────────────────┐│
│  │ symbolic_exec_solve()                   ││
│  │   ┌─────────┐    ┌──────┐    ┌────────┐││
│  │   │ Encode  │ →  │ Z3   │ →  │ Decode │││
│  │   │ SMTLIB2 │    │/cvc5 │    │ Result │││
│  │   └─────────┘    └──────┘    └────────┘││
│  └─────────────────────────────────────────┘│
└──────────────────────────────────────────────┘
       │
       ▼
┌──────────────┐
│ 解绑回Graph   │
└──────────────┘
```

---

> **文档结束**  
> 本文档详述了 Rosette "符号虚拟机"概念如何应用于 Lv-00——将几何构造"符号化"执行后交给 Z3 求解。核心结论：通过在 `solver.h` 和 `smt_backend.h` 之间引入 `SymbolicExecCtx` 符号执行桥接层，Lv-00 可以实现从"构造步骤→符号变量→约束收集→SMT 编码→求解→实例化"的完整管线，这与 Rosette 的 `define-symbolic → assert → solve → evaluate` 四步模式完全对应。

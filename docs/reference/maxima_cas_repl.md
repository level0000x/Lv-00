# Lv-00 参考落地设计文档：Maxima CAS + REPL 交互模式

> **版本**: 1.0.0
> **日期**: 2026-05-24
> **参考**: Maxima (maxima.sourceforge.io) —— DOE-MACSYMA 后代的经典开源 CAS
> **目标**: 借鉴 Maxima 50+ 年符号计算积累（Risch 积分、微分方程）及其 REPL 交互模式（%i1/%o1 标记系统），映射到 Lv-00 命令行界面 UX

---

## 目录

1. [项目概述与 Lv-00 借鉴动机](#1-项目概述与-lv-00-借鉴动机)
2. [核心借鉴要点：REPL 标记系统和符号算法积累](#2-核心借鉴要点repl-标记系统和符号算法积累)
3. [REPL 交互模式映射方案](#3-repl-交互模式映射方案)
4. [符号计算算法层的参考设计](#4-符号计算算法层的参考设计)
5. [Lv-00 CLI 完整交互规范](#5-lv-00-cli-完整交互规范)
6. [实现路线图](#6-实现路线图)
7. [关键映射表](#7-关键映射表)

---

## 1. 项目概述与 Lv-00 借鉴动机

### 1.1 Maxima 是什么

Maxima 是 MACSYMA（MIT 1960年代开发的第一个符号代数系统）的直接后代，在 DOE 支持下开源，至今已持续维护 50+ 年。它是用 Common Lisp 编写的完整计算机代数系统。其核心特性包括：

| 能力 | 实现 | 成熟度 |
|:---|:---|:---|
| **符号积分** | Risch 算法的完整参考实现 | 工业级 |
| **微分方程** | ODE/PDE 符号求解 | 工业级 |
| **极限计算** | 基于符号展开的极限法 | 成熟 |
| **矩阵线性代数** | 符号和数值双路径 | 成熟 |
| **REPL 交互** | `%i1` / `%o1` 输入输出标记系统 | 经典 |

### 1.2 Lv-00 借鉴动机

Lv-00 虽然以几何构造和形式化证明为核心，但"几何构造 = 计算程序"意味着用户在几何构造中同样需要进行符号计算——计算交点坐标、面积、角度等。当前实现依赖于 `Gröbner` 基求解器和 `SMT` 后端，但缺乏"REPL 式交互"的轻量级前端体验：

| 借鉴方向 | Maxima 特性 | Lv-00 现有基础 | 差距与目标 |
|:---|:---|:---|:---|
| **REPL 交互 UX** | `%i1/%o1` 输入输出标记、`%` 引用上一条结果 | `formula_module.js` 解析/渲染 | 缺失 CLI REPL 的历史标记和引用机制 |
| **符号积分算法** | Risch 算法完整实现 | `solver.h` Gröbner / `smt_backend.h` SMT | 缺失符号积分能力（几何中计算面积/弧长需要） |
| **微分方程求解** | ODE 分类求解器 | `preset_differential_equations.h` | 预置模块存在但缺符号求解引擎 |
| **简化/展开** | `ratsimp`, `expand`, `factor` 等 | `normalization.h`（图归一化） | 缺代数表达式化简管道 |

### 1.3 Maxima 与 Lv-00 的互补关系

Maxima 是"纯符号计算"的 CAS，Lv-00 是"几何构造=计算=证明"的元语言。二者的交集在**几何相关的符号计算**——计算三角形的面积公式、验证两个几何量的等效性、在证明过程中化简代数表达式。

```
Maxima 会话:
(%i1) A: [0, 0]; B: [6, 0]; C: [3, 4];
(%i2) area: 0.5 * abs((B[1]-A[1])*(C[2]-A[2]) - (C[1]-A[1])*(B[2]-A[2]));
(%o2) 12

Lv-00 等价:
lv00> point A(0, 0); point B(6, 0); point C(3, 4);
lv00> number S = area(triangle(A, B, C));
:  S = 12                    // ← 借鉴 Maxima 输出格式
```

---

## 2. 核心借鉴要点：REPL 标记系统和符号算法积累

### 2.1 %i / %o 输入输出标记系统

Maxima 最经典的交互设计是 `%iN`（第 N 条输入）和 `%oN`（第 N 条输出）标记系统：

```
(%i1) integrate(sin(x), x);
(%o1)                           - cos(x)
(%i2) diff(%, x);               // % 引用 (%o1)
(%o2)                            sin(x)
(%i3) %o1, x=0;                 // %o1 引用指定输出
(%o3)                             - 1
```

这个系统的精妙之处：
- **持久可引用**：每条输入/输出都有唯一编号，可在后续任何位置引用
- **`%` 快捷符号**：引用上一条输出，减少重复输入
- **`%oN` 显式引用**：精确引用历史中的任意输出
- **输出可参与计算**：输出不是"死数据"，而是"活的值"

### 2.2 五层符号算法参考

| 层次 | Maxima 实现 | 对 Lv-00 的参考价值 |
|:---|:---|:---|
| **多项式层** | `factor`, `expand`, `ratsimp` | Lv-00 代数约束化简的算法基准 |
| **有理函数层** | `partfrac`, `rat` | 符号坐标的商域运算 |
| **初等函数层** | `trigsimp`, `logcontract` | 几何中的三角恒等式化简 |
| **积分层** | `integrate` (Risch 算法) | 面积/弧长的符号计算 |
| **微分方程层** | `ode2`, `desolve` | 几何轨迹的微分约束建模 |

---

## 3. REPL 交互模式映射方案

### 3.1 Lv-00 CLI 的标记系统设计

借鉴 Maxima 的 `%i/%o`，为 Lv-00 命令行界面设计等价的标记系统：

```
Lv-00 标记系统:
  >N  表示第 N 条输入（input）
  :N  表示第 N 条输出（result）
  :   引用上一条输出（等价于 Maxima 的 %）
  :N  引用编号为 N 的输出

示例会话:
lv00> point A(0, 0); point B(6, 0); point C(3, 4);
:1  [A=(0,0), B=(6,0), C=(3,4)]

lv00> number S = area(triangle(A, B, C));
:2  S = 12

lv00> assert :2 > 10;              // 引用输出 :2
:3  true (12 > 10)
```

### 3.2 CLI 输出格式规范

借鉴 Maxima 的缩进和标签风格，Lv-00 CLI 输出采用结构化的标签格式：

```
Lv-00 输出类别标签:
  :N           计算结果（数值或符号表达式）
  :N [type]    带类型标注的结果
  :N |proof|   证明结果（含策略和颜色）
  :N |error|   错误信息
  :N |warning| 警告信息

示例:
lv00> prove median_concurrency using strategy=area_method;
:5 |proof| GREEN (面积法)
  步骤 1: midpoint(A, B) → M_AB
  步骤 2: midpoint(B, C) → M_BC
  步骤 3: midpoint(C, A) → M_CA
  步骤 4: concurrent(med_A, med_B, med_C) [重心]
  依赖: 中点公式、共线公理、面积法
```

### 3.3 命令历史与表达式引用

```c
/**
 * @brief Lv-00 CLI 历史记录管理器 —— 借鉴 Maxima %i/%o 标记系统
 *
 * 维护按编号索引的输入输出历史，支持引用和重放。
 */
typedef struct {
    int next_input_id;                  /* 下一条输入编号 */
    int next_output_id;                 /* 下一条输出编号 */

    struct {
        char *input_text;               /* 原始输入文本 */
        int input_id;                   /* 输入编号（>N） */
    } *input_history;
    int input_history_count;
    int input_history_capacity;

    struct {
        char *output_text;              /* 输出文本（格式化后） */
        int output_id;                  /* 输出编号（:N） */
        int source_input_id;            /* 产生此输出的输入编号 */
        enum { OUTPUT_VALUE, OUTPUT_PROOF, OUTPUT_ERROR, OUTPUT_WARNING } kind;
    } *output_history;
    int output_history_count;
    int output_history_capacity;
} CLIHistory;

/**
 * @brief 解析输入中的历史引用（如 :、:5、>2）
 *
 * 借鉴 Maxima 的 % / %oN 历史引用处理。
 * 将引用的输出值内联到表达式中。
 */
char *cli_resolve_history_refs(const CLIHistory *hist, const char *input);
```

### 3.4 多行输入模式

Maxima 支持以 `;` 或 `$` 结尾的多语句输入，以及 `(...)` 跨行表达式。Lv-00 CLI 同样需要支持：

```
// 多语句输入（; 结尾 → 显示输出; $ 结尾 → 抑制输出）
lv00> point A(0, 0); point B(6, 0); point C(3, 4);
:1  [A=(0,0), B=(6,0), C=(3,4)]

lv00> number S = area(triangle(A, B, C))$   // $ 抑制输出
// (无输出)

// 跨行模式（自动检测未闭合的括号、花括号、函数块体）
lv00> funcblock equilateral(p1 : Point, p2 : Point) -> Point {
...     point _M = midpoint(p1, p2);
...     line _perp = perpendicular(p1, p2, _M);
...     number _h = sqrt(3) / 2 * distance(p1, p2);
...     point result = point_on_line_at_distance(_perp, _M, _h);
...     return result;
... }
:3  |funcblock| equilateral(Point, Point) -> Point [verified]
```

---

## 4. 符号计算算法层的参考设计

### 4.1 代数化简管道（借鉴 Maxima simplify 管线）

Maxima 的化简是分阶段的多步管线。Lv-00 的几何约束同样受益于这种分层化简：

```c
/**
 * @brief 代数表达式化简管道 —— 借鉴 Maxima 的 simplify 管线
 *
 * Lv-00 几何构造中的表达式（面积公式、交点坐标表达式）
 * 经过多层化简管道，逐层降低表达式复杂度。
 *
 * 管道阶段（按 Maxima 经验排序）：
 *   STAGE_EXPAND    展开多项式（对应 Maxima expand）
 *   STAGE_FACTOR    因式分解（对应 Maxima factor）
 *   STAGE_TRIG_SIMP 三角化简（对应 Maxima trigsimp）
 *   STAGE_RAT_SIMP  有理函数化简（对应 Maxima ratsimp）
 *   STAGE_FULL_SIMP 全化简（对应 Maxima fullratsimp）
 *   STAGE_CANONICAL 输出规范化（对应 Maxima 的默认输出格式化）
 */
typedef enum {
    SIMPLIFY_STAGE_EXPAND,
    SIMPLIFY_STAGE_FACTOR,
    SIMPLIFY_STAGE_TRIG,
    SIMPLIFY_STAGE_RATIONAL,
    SIMPLIFY_STAGE_FULL,
    SIMPLIFY_STAGE_CANONICAL
} SimplifyStage;

/**
 * @brief 对几何表达式执行分层化简
 *
 * 将约束图的计算边提取为代数表达式，按指定阶段逐层化简。
 * 借鉴 Maxima 50+ 年的化简策略积累。
 *
 * @param graph   约束图
 * @param expr_id 目标表达式节点 ID
 * @param stages  应用哪些化简阶段（位掩码）
 * @param out     输出化简后的表达式（调用者释放）
 */
SimplifyResult *lv00_simplify_expression(
    const ConstraintGraph *graph,
    int expr_id,
    uint64_t stages,
    char **out);
```

### 4.2 Risch 积分算法参考

Maxima 的符号积分基于 Risch 算法的完整实现，这是 50+ 年符号计算的珍贵积累。Lv-00 几何中计算**闭合路径围成的面积**、**曲线的弧长**等场景，需要符号积分。

当前 Lv-00 没有符号积分引擎，但可以通过以下路径间接实现：
- **路径 A（数值）**：用 `solver.h` 的数值坐标 + 数值积分（梯形法/辛普森法）
- **路径 B（符号，P4）**：将几何约束转化为多项式系统后用 SMT 编码（`smtencode_area_to_smtlib2()`）
- **路径 C（外部引擎，P4）**：调用 Maxima 作为外部符号积分引擎

```c
/**
 * @brief 几何区域的符号面积计算 —— 借鉴 Maxima 积分引擎
 *
 * 将几何区域边界编码为定积分表达式，调用符号积分引擎计算。
 * 当前实现：数值路径（辛普森法）
 * 规划实现：SMT 编码路径
 * 远期实现：集成外部 CAS（如 Maxima）作为符号后端
 */
typedef enum {
    AREA_CALC_NUMERIC,          /* 数值积分（当前可用） */
    AREA_CALC_SMT_ENCODE,       /* SMT编码 → Z3/cvc5（规划） */
    AREA_CALC_EXTERNAL_CAS      /* 外部CAS引擎（远期） */
} AreaCalcMethod;

double lv00_compute_symbolic_area(
    ConstraintGraph *graph,
    int region_id,
    AreaCalcMethod method);
```

---

## 5. Lv-00 CLI 完整交互规范

### 5.1 会话示例：从 Maxima 风格到 Lv-00 风格

```
============================
 Lv-00 几何元语言 CLI v3.2.0
 输入 help 查看命令列表
============================

lv00> // 构造三角形
lv00> point A(0, 0); point B(6, 0); point C(3, 4);
:1  [A=(0,0), B=(6,0), C=(3,4)]

lv00> number S = area(triangle(A, B, C));
:2  S = 12

lv00> // 引用 :2 做进一步计算
lv00> point G = centroid(A, B, C);
:3  G = (3, 4/3)

lv00> number S_by_heron = sqrt(s*(s-a)*(s-b)*(s-c))$
// ($ 抑制输出)

lv00> assert :2 == S_by_heron;
:4  true

lv00> // 证明中线共点
lv00> proposition median_concurrency {
...     given: triangle(A, B, C);
...     construct: {
...         point M_AB = midpoint(A, B);
...         point M_BC = midpoint(B, C);
...         point M_CA = midpoint(C, A);
...         line med_A = segment(A, M_BC);
...         line med_B = segment(B, M_CA);
...         line med_C = segment(C, M_AB);
...     }
...     prove: concurrent(med_A, med_B, med_C);
... }
:5  |proposition| median_concurrency [registered]

lv00> prove median_concurrency using strategy=area_method;
:6  |proof| GREEN (面积法)
    定理: 三角形三条中线交于一点（重心）
    步骤数: 8
    颜色: GREEN (全构造, 无非常规依赖)
```

### 5.2 命令速查

| 命令 | Maxima 等价 | 功能 |
|:---|:---|:---|
| `help` | `describe(...)` | 显示帮助 |
| `load <name>` | `load("<name>")` | 加载公理包或模块 |
| `:N` 或 `%` | `%oN` 或 `%` | 引用历史输出 |
| `print :N` | `grind(%)` | 详细打印输出 |
| `$` 结尾 | `$` 结尾 | 执行但抑制输出 |
| `--verbose` | 无直接等价 | 开启详细模式 |
| `undo N` | 无直接等价 | 撤销到编号 N 前的状态 |

---

## 6. 实现路线图

### 6.1 第一阶段：CLI REPL 基础框架（P3）

| 任务 | 文件 | 说明 |
|:---|:---|:---|
| `CLIHistory` 数据结构 | `include/lv00/cli_history.h`（新文件） | 借鉴 Maxima %i/%o 标记的历史管理器 |
| `cli_resolve_history_refs()` | `src/cli_history.c`（新文件） | 解析 :N / : 引用并内联 |
| `cli_read_eval_print_loop()` | `src/cli_repl.c`（新文件） | 主 REPL 循环 |
| 多行输入检测 | `src/cli_repl.c` | 未闭合括号/花括号自动进入续行模式 |

**预估规模**：约 250 行 C 代码

### 6.2 第二阶段：符号计算简化管线（P3）

| 任务 | 文件 | 说明 |
|:---|:---|:---|
| `SimplifyStage` 枚举和简化管线 | `include/lv00/simplify.h`（新文件） | Maxima 风格的多阶段简化 |
| `lv00_simplify_expression()` | `src/simplify.c`（新文件） | 分层简化核心函数 |
| 三角恒等式化简 | `src/simplify_trig.c` | `sin²+cos²=1` 等常见恒等式 |
| 多项式因式分解 | `src/simplify_poly.c` | 对接现有 Gröbner 基模块 |

**预估规模**：约 200 行 C 代码

### 6.3 第三阶段：外部 CAS 集成接口（P4，远期）

| 任务 | 说明 |
|:---|:---|
| `cas_backend.h` 抽象层 | 统一的外部 CAS 调用接口（Maxima/SymPy/Singular） |
| 面积/弧长符号积分 | 调用外部 CAS 进行符号积分，结果回写到 `SymbolicCoord` |
| 表达式等价验证 | 调用外部 CAS 验证两个代数表达式等价 → 反馈到 `type_check_equivalence` |

---

## 7. 关键映射表

### 7.1 Maxima → Lv-00 REPL 概念映射

| Maxima | Lv-00 | 差异 |
|:---|:---|:---|
| `(%i1)` 输入标记 | `>1` 或隐式（提示符后） | Lv-00 更简洁 |
| `(%o1)` 输出标记 | `:1` 输出标记 | 语义等价 |
| `%` 引用上一条输出 | `:` 引用上一条输出 | 符号不同，语义一致 |
| `%o5` 引用指定输出 | `:5` 引用指定输出 | 符号更短 |
| `;` 结尾 | `;` 结尾（显示输出） | 语义一致 |
| `$` 结尾 | `$` 结尾（抑制输出） | 语义一致 |
| `display2d` | 默认启用格式化输出 | Lv-00 终端用缩进替代 |
| `describe(fn)` | `help fn` | 帮助系统 |
| `kill(all)` | `reset` | 重置会话状态 |

### 7.2 符号算法层对照

| Maxima 函数 | Lv-00 映射函数 | 状态 |
|:---|:---|:---|
| `expand(expr)` | `simplify_expression(graph, id, STAGE_EXPAND)` | 规划 |
| `factor(expr)` | `simplify_expression(graph, id, STAGE_FACTOR)` | 规划 |
| `ratsimp(expr)` | `simplify_expression(graph, id, STAGE_RATIONAL)` | 规划 |
| `trigsimp(expr)` | `simplify_expression(graph, id, STAGE_TRIG)` | 规划 |
| `integrate(expr, x)` | `lv00_compute_symbolic_area()` (数值路径) 或外部 CAS 调用 | 部分可用 |
| `diff(expr, x)` | 无直接等价（符号微分非核心需求） | P4 |
| `solve(eqns, vars)` | `scheduler_solve(graph)` → Gröbner/SMT | 可用（via solver.h） |
| `limit(expr, x, a)` | 无直接等价（极限非核心需求） | P4 |

---

## 附录 A：Lv-00 CLI 命令完整列表（Maxima 对照）

```
几何构造命令（Maxima 无原生支持）:
  point, line, circle, segment, midpoint, intersection,
  perpendicular, parallel, angle_bisector, centroid,
  orthocenter, circumcenter, incenter, reflect, rotate,
  scale, translate

约束/断言命令:
  assert, incident, between, collinear, concurrent

证明命令（Maxima 无原生支持）:
  proposition, prove, lemma, axiom, strategy, export_coq,
  export_latex, export_html, export_nl

计算命令（Maxima 有对应）:
  area, distance, angle, simplify (→ Maxima ratsimp),
  solve (→ Maxima solve)

REPL 控制命令:
  help, load, reset, undo, print, --verbose
```

---

## 附录 B：完整交互会话示例

```
lv00> load euclidean;
:1  |loaded| euclidean_geometry.lvz (version 1.0.0)

lv00> point A(0,0); point B(6,0); point C(3,4);
:2  [A=(0,0), B=(6,0), C=(3,4)]

lv00> S = area(triangle(A, B, C));
:3  S = 12

lv00> point G = centroid(A, B, C);
:4  G = (3, 4/3)

lv00> distance(A, B);
:5  |AB| = 6

lv00> distance(B, C);
:6  |BC| = 5

lv00> distance(C, A);
:7  |CA| = 5

lv00> assert :5 + :6 + :7 == perimeter(triangle(A, B, C));
:8  true (6 + 5 + 5 = 16)
```

---

> **文档结束**
> 本文档详述了 Maxima 的 REPL 标记系统（%i1/%o1）如何映射到 Lv-00 命令行界面（>N/:N 标记），以及 Maxima 50+ 年符号计算积累（Risch 积分、多阶段化简管道）如何为 Lv-00 的几何计算提供算法参考。核心设计：Lv-00 CLI 的 `:N` 标记系统使几何构造的每一步输出都是可引用的"活值"，用户可以在证明和计算中自由引用历史结果。

# Lv-00 参考落地设计文档：Symbolics.jl + ModelingToolkit.jl 符号-数值混合计算

> **版本**: 1.0.0
> **日期**: 2026-05-24
> **参考**: Symbolics.jl (github.com/JuliaSymbolics/Symbolics.jl) —— Julia 原生符号计算框架；ModelingToolkit.jl (github.com/SciML/ModelingToolkit.jl) —— 科学建模编译框架
> **目标**: 借鉴 Symbolics.jl 的透明符号-数值切换、build_function 编译技术以及 ModelingToolkit.jl 的"单一模型定义→多目标编译"架构，映射到 Lv-00 的"单一几何构造→多输出编译"（约束求解 + 证明验证 + 可视化 + LaTeX + Python 代码生成）

---

## 目录

1. [Symbolics.jl 与 ModelingToolkit.jl 项目概述](#1-symbolicsjl-与-modelingtoolkitjl-项目概述)
2. [核心借鉴要点一：透明符号-数值切换](#2-核心借鉴要点一透明符号-数值切换)
3. [核心借鉴要点二：build_function 编译符号表达式为原生函数](#3-核心借鉴要点二build_function-编译符号表达式为原生函数)
4. [核心借鉴要点三：单一模型定义→多目标编译](#4-核心借鉴要点三单一模型定义多目标编译)
5. [Lv-00 映射方案：单一几何构造→多输出编译](#5-lv-00-映射方案单一几何构造多输出编译)
6. [Python 绑定代码生成架构](#6-python-绑定代码生成架构)
7. [实现路线图](#7-实现路线图)
8. [关键映射表](#8-关键映射表)

---

## 1. Symbolics.jl 与 ModelingToolkit.jl 项目概述

### 1.1 两个项目的定位

Symbolics.jl 是 Julia 生态中由 SciML 团队主导的原生符号计算框架，完全用 Julia 编写，与 Julia 的类型系统和 JIT 编译器深度集成。ModelingToolkit.jl 则是在 Symbolics.jl 之上构建的**科学建模编译框架**——让用户用声明式语法定义模型，然后自动编译到各种目标后端。

```
Julia SciML 生态系统
─────────────────────────────────────────
Symbolics.jl          ← 符号基础层（变量定义/化简/求导/替换）
    │
ModelingToolkit.jl    ← 建模编译器（ODE/DAE/非线性方程）
    │
    ├── ODEProblem    → 编译为高效可求解的 ODE 系统
    ├── OptimizationProblem → 编译为优化问题
    ├── NonlinearSystem → 编译为非线性方程组求解
    ├── LaTeX 导出     → 公式的自动 LaTeX 生成
    └── C 代码导出     → 生成独立的 C 语言实现
```

### 1.2 Lv-00 借鉴动机

Lv-00 的几何构造系统天然需要"单一几何构造→多种输出"的编译模型。当用户在 DSL 中定义一个几何问题（如三角形+中线+重心），后续需要多种不同的处理方式。ModelingToolkit.jl 的"定义一次、编译多次"架构为 Lv-00 提供了直接参考：

| 借鉴方向 | Julia 特性 | Lv-00 现有基础 | 差距 |
|:---|:---|:---|:---|
| **符号-数值透明切换** | `@variables x y; expr = x^2 + y^2` 同一对象 | `SymbolicCoord` 的 4 种类型 | 缺统一的"可求值表达式"对象 |
| **build_function** | 符号表达式编译为原生 Julia 函数 | `solver_numerical()` 数值求解 | 缺"符号表达式→可执行函数"编译 |
| **多目标编译** | ODE/优化/非线性/C代码 | `proof_export_latex()` + `solver_select_backend()` | 缺统一的"编译目标选择器" |
| **LaTeX 自动生成** | `latexify(expr)` | `proof_export_latex()` | 当前仅整个证明的输出 |
| **符号微分** | `Differential(t)(x)` 自动求导 | 无 | 可在 Gröbner 基之上实现 |

### 1.3 核心概念对照

```
Julia SciML                                Lv-00
────────────────────────────────────────────────────────────
@variables x y z                       →   SymbolicCoord (ALGEBRAIC)
expr = x^2 + y^2                       →   ExprNode 表达式树
build_function(expr, [x,y])            →   func_compile_to_c(expr_node)
ODESystem(eqs, t, vars, params)        →   ConstraintSystem(rules, nodes)
structural_simplify(sys)               →   constraint_graph_simplify(graph)
latexify(expr)                         →   proof_export_latex()
generate_function(sys)                 →   multi_target_compile(targets)
```

---

## 2. 核心借鉴要点一：透明符号-数值切换

### 2.1 Symbolics.jl 的透明符号对象

Symbolics.jl 的核心哲学是：**同一个表达式对象在不同的上下文中既是符号的也是数值的**。Julia 的多重分派（multiple dispatch）使得这一设计非常自然——当表达式被求值时，根据参数类型自动选择符号路径或数值路径：

```julia
# Symbolics.jl 示例：
using Symbolics

@variables x y                           # 声明符号变量
expr = x^2 + y^2                         # 符号表达式（未求值）

# 符号上下文中——保持符号形式
simplify(expr + x^2)                     # → 2x^2 + y^2（符号化简）

# 数值上下文中——代入值后求值
substitute(expr, Dict(x => 3, y => 4))   # → 9 + 16 = 25（数值求值）

# 关键：expr 本身从未改变，只是求值方式不同
# 这是"透明"的含义——用户不需要关心当前处于符号还是数值路径
```

### 2.2 Lv-00 的等价设计：统一可求值坐标

借鉴 Symbolics.jl 的透明符号-数值概念，为 Lv-00 设计一个统一的"可求值几何表达式"类型：

```c
/**
 * @brief 可求值几何表达式 —— 借鉴 Symbolics.jl 的透明符号-数值切换
 *
 * 每个 EvalGeomExpr 封装了一个几何表达式及其求值上下文。
 * 根据求值模式自动切换符号/数值路径。
 *
 * Julian 等价:
 *   expr = x^2 + y^2       → symbolics模式（默认）
 *   expr(3, 4)             → 数值求值模式
 *   simplify(expr)          → 符号化简模式
 */
typedef enum {
    EVAL_MODE_SYMBOLIC,     /* 符号模式：保持表达式树，不代入数值 */
    EVAL_MODE_NUMERIC,      /* 数值模式：代入具体坐标值，返回 double 结果 */
    EVAL_MODE_HYBRID        /* 混合模式：尽可能化简，无法化简的部分保留符号 */
} EvalMode;

typedef struct EvalGeomExpr {
    ExprNode *tree;                     /* 表达式树（Symbolics.jl 等价: Num 类型） */
    int *variable_ids;                  /* 自由变量列表（Symbolics.jl 等价: @variables 声明的变量） */
    int var_count;
    EvalMode default_mode;              /* 默认求值模式 */
} EvalGeomExpr;

/**
 * @brief 统一求值接口 —— 借鉴 Symbolics.jl 的透明求值
 *
 * 同一个 EvalGeomExpr 可以：
 *   1. 符号模式:  返回 Expression（ExprNode*），不做数值代入
 *   2. 数值模式:  代入 SymbolicCoord[] 数组，返回 double
 *   3. 混合模式:  能化简的部分化简，不能化简的部分保留符号
 *
 * 这是 Lv-00 "A 计划/B 计划"在表达式层的实现。
 */
ExprNode *eval_geom_simplify(const EvalGeomExpr *expr);
double    eval_geom_numeric(const EvalGeomExpr *expr,
                            const SymbolicCoord **bindings,
                            int binding_count);
ExprNode *eval_geom_subs(const EvalGeomExpr *expr,
                         const SymbolicCoord **vars,
                         const SymbolicCoord **vals,
                         int count);
```

### 2.3 几何构造中的透明切换示例

```
Lv-00 DSL 示例:

// 定义自由点（符号变量）
point A free;
point B free;

// segment AB —— 在符号模式下，仅记录约束
segment s = segment(A, B);

// 中点 M 既可以是符号也可以是数值
point M = midpoint(A, B);

// 场景1: 符号分析（证明阶段）
// M 的坐标以符号形式参与 Gröbner 基化简
ds: prove collinear(A, M, B);
// → A 计划：A=(x_a,y_a), B=(x_b,y_b)
//   Gröbner 基检查共线性恒等式 → GREEN

// 场景2: 数值预览（Web GUI）
// 用户拖动 A 到 (2, 3)，B 到 (8, 5)
//   → M = midpoint((2,3), (8,5)) = (5, 4)
//   → B 计划：快速数值更新 Web 画布
```

---

## 3. 核心借鉴要点二：build_function 编译符号表达式为原生函数

### 3.1 ModelingToolkit.jl 的 build_function

`build_function` 是 ModelingToolkit.jl 的一个关键技术：将符号表达式编译为原生 Julia 函数（或 C 代码），从而在后续的数值计算中获得与手写代码相近的性能：

```julia
# ModelingToolkit.jl 示例
@variables x y
expr = x^2 + sin(y) + x * y

# 编译为 Julia 原生函数（性能接近手写）
f = build_function(expr, [x, y])
# f = (x, y) -> x^2 + sin(y) + x * y  （JIT 编译后）
f(3.0, 4.0)  # 直接机器码执行，无符号解释开销

# 编译为 C 代码（独立于 Julia）
build_function(expr, [x, y], target=Symbolics.CTarget())
# 输出: double f(double x, double y) { return x*x + sin(y) + x*y; }

# 编译为独立可执行程序
build_function(expr, [x, y], target=Symbolics.Standalone())
```

### 3.2 Lv-00 的几何函数编译

Lv-00 可以利用类似的技术，将频繁使用的几何构造函数编译为 C 源码或 Emscripten WASM 模块：

```c
/**
 * @brief Lv-00 几何函数编译目标 —— 借鉴 ModelingToolkit build_function
 *
 * 将 ExprNode 表达式树编译为可执行代码，
 * 支持多个目标后端。
 *
 * Julian 等价:
 *   build_function(expr, vars, target=CTarget())
 */
typedef enum {
    COMPILE_TARGET_C_SOURCE,        /* 生成 C 源码文件 */
    COMPILE_TARGET_C_SHARED,        /* 编译为共享库 (.so/.dll) */
    COMPILE_TARGET_WASM,            /* 编译为 WebAssembly 模块 */
    COMPILE_TARGET_PYTHON_CTYPES,   /* 生成 Python ctypes 包装 */
    COMPILE_TARGET_PYTHON_SYMPY     /* 生成 SymPy 等价表达式字符串 */
} CompileTarget;

/**
 * @brief 将几何表达式编译到指定目标
 *
 * 编译管线:
 *   ExprNode 树
 *     → 展平为三地址码（优化机会：常数折叠、公共子表达式消除）
 *     → 根据 target 生成目标代码
 *     → 输出到文件
 *
 * 典型使用场景:
 *   - 用户定义了 20 个点、15 条线、3 个三角形
 *   - 需要计算 15 次中点公式、10 次距离公式
 *   - 编译为 C → 比在解释器中逐条求值快 10-100 倍
 */
int geom_func_compile(
    const ExprNode *expr,
    const char **var_names,
    int var_count,
    CompileTarget target,
    const char *output_path);
```

### 3.3 编译输出示例

```
// 输入: 符号表达式 distance_sq = (x2 - x1)^2 + (y2 - y1)^2
// 变量: ["x1", "y1", "x2", "y2"]
// 目标: COMPILE_TARGET_C_SOURCE
//
// ——— 生成的 C 源码 ———
#include <math.h>

double distance_sq(double x1, double y1, double x2, double y2) {
    double t1 = x2 - x1;
    double t2 = y2 - y1;
    return t1 * t1 + t2 * t2;
}

// ——— 生成的 Python 绑定 ———
# target: COMPILE_TARGET_PYTHON_CTYPES
import ctypes
_lib = ctypes.CDLL("./lv00_geom.so")
_lib.distance_sq.argtypes = [ctypes.c_double]*4
_lib.distance_sq.restype = ctypes.c_double

def distance_sq(x1, y1, x2, y2):
    return _lib.distance_sq(x1, y1, x2, y2)
```

---

## 4. 核心借鉴要点三：单一模型定义→多目标编译

### 4.1 ModelingToolkit.jl 的多目标架构

ModelingToolkit.jl 的一个核心价值主张是：**用户定义一次模型，系统自动编译到多种目标**：

```
ModelingToolkit.jl 多目标编译
────────────────────────────────────────
用户定义:
  @parameters t σ ρ β
  @variables x(t) y(t) z(t)
  eqs = [D(x) ~ σ*(y-x),
         D(y) ~ x*(ρ-z)-y,
         D(z) ~ x*y - β*z]
  @named sys = ODESystem(eqs, t, [x,y,z], [σ,ρ,β])

单一定义后，自动获得:
  ├── ODEProblem(sys, u0, tspan, p)    → 数值求解器
  ├── structural_simplify(sys)           → 结构化简
  ├── linearize(sys, ...)               → 线性化分析
  ├── latexify(sys)                     → LaTeX 输出
  ├── generate_function(sys)            → Julia 原生函数
  └── generate_code(sys, target=...)    → C/JS 代码
```

### 4.2 Lv-00 的"单一几何构造→多输出编译"架构

借鉴 ModelingToolkit.jl 的多目标编译，Lv-00 的几何约束图定义后，应能自动产出多种输出：

```
Lv-00 多输出编译管线
────────────────────────────────────────────────
用户定义（.lvz DSL）:
  point A free; point B free; point C free;
  triangle ABC = triangle(A, B, C);
  point M = midpoint(B, C);
  line med_A = line(A, M);
  // 同理 med_B, med_C
  prove concurrent(med_A, med_B, med_C);

单一几何构造 (ConstraintGraph + FuncBlock[])
    │
    ├── 输出1: 约束求解   → Gröbner 基 / SMT 求解
    │       → 验证 concurrent 命题
    │       → 输出 ProofColor (GREEN/AMBER/RED)
    │
    ├── 输出2: 证明验证   → 生成结构化证明文档
    │       → 公理引用链 + 推理步骤
    │       → 输出 LaTeX/TikZ 证明
    │
    ├── 输出3: 可视化     → Web GUI 交互式图形
    │       → 约束着色 (满足=绿, 不满足=红, 待验证=蓝)
    │       → 支持点拖拽 + 实时约束检查
    │
    ├── 输出4: LaTeX 图形 → GCLC / TikZ 矢量图
    │       → 用于论文级出版质量的几何图形
    │
    └── 输出5: Python 代码→ 可独立运行的 Python 脚本
            → 调用 SymPy 验证等价结果
            → 用于教育/演示/跨语言交叉验证
```

### 4.3 多目标编译器核心

```c
/**
 * @brief Lv-00 多目标编译引擎 —— 借鉴 ModelingToolkit.jl 的编译目标切换
 *
 * 类似于 ModelingToolkit 的:
 *   generate_function(sys) / structural_simplify(sys) / latexify(sys)
 *
 * Lv-00 提供统一的多目标编译入口:
 *   同一个 ConstraintGraph，可编译为多种输出格式。
 */
typedef enum {
    LV_TARGET_SOLVE,            /* 目标1: 约束求解 (A计划/B计划) */
    LV_TARGET_PROOF_VERIFY,     /* 目标2: 证明验证 (公理链生成) */
    LV_TARGET_WEB_CANVAS,       /* 目标3: Web 交互式画布 (JSON) */
    LV_TARGET_LATEX_TIKZ,       /* 目标4: LaTeX/TikZ 矢量图形 */
    LV_TARGET_LATEX_PROOF,      /* 目标4a: LaTeX 结构化证明 */
    LV_TARGET_PYTHON_SYMPY,     /* 目标5: Python SymPy 独立脚本 */
    LV_TARGET_PYTHON_CTYPES,    /* 目标5a: Python ctypes 绑定 */
    LV_TARGET_DOT_GRAPH,        /* 目标6: Graphviz DOT 可视化 */
    LV_TARGET_GCLC_GC           /* 目标7: GCLC 几何编译器输入 */
} LvCompileTarget;

/**
 * @brief 多目标编译统一接口
 *
 * 示例调用:
 *   lv_compile(graph, LV_TARGET_SOLVE,       "output/solution.txt");
 *   lv_compile(graph, LV_TARGET_LATEX_TIKZ,  "output/geometry.tex");
 *   lv_compile(graph, LV_TARGET_PYTHON_SYMPY,"output/verify.py");
 *   lv_compile(graph, LV_TARGET_WEB_CANVAS,  "output/web_canvas.json");
 */
int lv_compile(
    ConstraintGraph *graph,
    LvCompileTarget target,
    const char *output_path);
```

---

## 5. Lv-00 映射方案：单一几何构造→多输出编译

### 5.1 编译器流水线

```
.lvz DSL 源文件
    │
    ▼
┌─────────────────────────────────────────────┐
│ Stage 1: 解析 (parser)                       │
│   词法 → 语法树 → ConstraintGraph + FuncBlock[] │
└───────────┬─────────────────────────────────┘
            │ 中间表示 (IR)
            ▼
┌─────────────────────────────────────────────┐
│ Stage 2: 类型推演 + 合一                     │
│   TypeRegion 分配 + Constraint 合一          │
│   + 符号坐标的类型分类                       │
└───────────┬─────────────────────────────────┘
            │ 类型化 IR
            ▼
┌─────────────────────────────────────────────┐
│ Stage 3: 多目标编译 (借鉴 build_function)    │
│                                              │
│   ┌─ SOLVE ───── → Gröbner / SMT 求解      │
│   ├─ PROOF ───── → 公理链生成              │
│   ├─ WEB ─────── → JSON Canvas 描述        │
│   ├─ LATEX ───── → TikZ 图形 + 证明文档    │
│   ├─ PYTHON ──── → SymPy 验证脚本          │
│   ├─ DOT ─────── → Graphviz 可视化         │
│   └─ GCLC ────── → GCLC 几何验证           │
└─────────────────────────────────────────────┘
```

### 5.2 各编译目标的详细映射

| 编译目标 | 输入 | 输出 | 用途 |
|:---|:---|:---|:---|
| `LV_TARGET_SOLVE` | 类型化 IR | 解点坐标 (SymbolicCoord[]) | 确定几何对象的确切位置 |
| `LV_TARGET_PROOF_VERIFY` | 类型化 IR + 公理包 | ProofColor + 证明步骤 | 验证几何命题真伪 |
| `LV_TARGET_WEB_CANVAS` | 解点坐标 + 约束图 | JSON (nodes + edges + colors) | Web 交互式几何面板 |
| `LV_TARGET_LATEX_TIKZ` | 解点坐标 | .tex 文件 (TikZ 代码) | 论文/教材 几何图形 |
| `LV_TARGET_LATEX_PROOF` | 证明步骤 + 公理链 | .tex 文件 (结构化证明) | 完整的 LaTeX 证明文档 |
| `LV_TARGET_PYTHON_SYMPY` | 约束方程 | .py 文件 (SymPy 等价脚本) | 跨语言交叉验证 |
| `LV_TARGET_DOT_GRAPH` | 约束图 | .dot 文件 | Graphviz 可视化 |
| `LV_TARGET_GCLC_GC` | 约束图 + 坐标 | .gc 文件 | GCLC 编译验证 |

### 5.3 多输出编译的证明验证示例

```
// Lv-00 DSL: 三条中线共点的完整定义
triangle ABC = triangle(A, B, C);
point M_BC = midpoint(B, C);
point M_CA = midpoint(C, A);
point M_AB = midpoint(A, B);
line med_A = line(A, M_BC);
line med_B = line(B, M_CA);
line med_C = line(C, M_AB);

prove: concurrent(med_A, med_B, med_C);

// —— 单一构造的五种编译输出 ——

// 输出1: 求解 → 重心 G 的坐标
G = ((x_a + x_b + x_c)/3, (y_a + y_b + y_c)/3)

// 输出2: 证明 → Gröbner 基验证
// 构造三中线共线的多项式方程
// Gröbner 基归约为 {0} → 证毕 (GREEN)

// 输出3: Web 画布 → JSON
{ "nodes": [
    {"id":"A","x":100,"y":50,"color":"#4CAF50","draggable":true},
    {"id":"G","x":300,"y":200,"color":"#2196F3","label":"重心"}
  ],
  "edges": [
    {"from":"A","to":"M_BC","color":"#4CAF50","label":"med_A"}
  ]
}

// 输出4: LaTeX 图形 → geometry.tex
// \begin{tikzpicture}
//   \coordinate (A) at (0,0);
//   \coordinate (G) at (2,1.33);
//   \draw[green] (A) -- (M_BC) node[midway,above] {$m_a$};
// \end{tikzpicture}

// 输出5: Python SymPy 验证 → verify.py
// import sympy as sp
// x_a, y_a, x_b, y_b, x_c, y_c = sp.symbols('x_a y_a x_b y_b x_c y_c')
// G_x = (x_a + x_b + x_c) / 3
// # 验证 G 在 med_A 上
// assert sp.simplify(collinearity(A, M_BC, G)) == 0
// print("验证通过！")
```

---

## 6. Python 绑定代码生成架构

### 6.1 设计动机

ModelingToolkit.jl 的 `build_function` 支持直接生成 C/C++ 代码。对于 Lv-00，最重要的外部绑定目标是 Python——因为 Python 生态拥有成熟的几何/符号计算工具链（SymPy、Shapely、Matplotlib），可作为 Lv-00 的交叉验证平台。

### 6.2 Python 代码生成器设计

```c
/**
 * @brief Python SymPy 脚本生成器 —— 借鉴 ModelingToolkit 的代码导出
 *
 * 将 Lv-00 的 ConstraintGraph 翻译为独立的 Python 脚本，
 * 该脚本使用 SymPy 定义等价的几何约束并验证。
 *
 * 用途:
 *   1. 交叉验证: Lv-00 求解结果 vs SymPy 求解结果
 *   2. 教育: 学生可以自行修改 Python 脚本探索几何
 *   3. 发布: 论文中可附带可复现的验证脚本
 *
 * 生成策略:
 *   每个 SymbolicCoord → sympy.Symbol()
 *   每个 Constraint → sp.Eq(lhs, rhs)
 *   求解操作 → sp.solve() 或 sp.groebner()
 */
int python_sympy_generate(
    const ConstraintGraph *graph,
    const SolveResult *result,
    const char *output_path);

/**
 * @brief Python ctypes 绑定生成器
 *
 * 生成通过 ctypes 调用编译后的 Lv-00 C 共享库的 Python 代码。
 * 适用于高性能数值求值（不依赖 SymPy 慢速解释）。
 *
 * ModelingToolkit 等价:
 *   build_function(expr, vars, target=CTarget())
 *   → 编译为C → 通过 ctypes/ForeignFunctionInterface 调用
 */
int python_ctypes_generate(
    const FuncBlock *func,
    const char *output_path);
```

### 6.3 生成的 Python SymPy 验证脚本示例

```python
# ============================================================
# 自动生成: Lv-00 → Python SymPy 验证脚本
# 几何命题: 三角形 ABC 的三条中线共点
# 生成时间: 2026-05-24 15:30:00
# ============================================================
import sympy as sp

# --- 自由变量声明（每个自由点有2个自由度）---
x_a, y_a = sp.symbols('x_a y_a', real=True)
x_b, y_b = sp.symbols('x_b y_b', real=True)
x_c, y_c = sp.symbols('x_c y_c', real=True)

# --- 辅助点：中点 ---
# M_BC = midpoint(B, C)
x_m_bc = (x_b + x_c) / 2
y_m_bc = (y_b + y_c) / 2

# M_CA = midpoint(C, A)
x_m_ca = (x_c + x_a) / 2
y_m_ca = (y_c + y_a) / 2

# M_AB = midpoint(A, B)
x_m_ab = (x_a + x_b) / 2
y_m_ab = (y_a + y_b) / 2

# --- 目标点：重心 G = 三中线交点 ---
x_g, y_g = sp.symbols('x_g y_g', real=True)

# --- 约束方程：G 在每条中线上 ---
# 共线性条件：det([G-A, M_BC-A]) = 0
eq1 = (x_g - x_a) * (y_m_bc - y_a) - (y_g - y_a) * (x_m_bc - x_a)
eq2 = (x_g - x_b) * (y_m_ca - y_b) - (y_g - y_b) * (x_m_ca - x_b)
eq3 = (x_g - x_c) * (y_m_ab - y_c) - (y_g - y_c) * (x_m_ab - x_c)

# --- 求解：检查公共解 ---
sol = sp.solve([sp.simplify(eq1), sp.simplify(eq2)], [x_g, y_g], dict=True)
if sol:
    G_coords = sol[0]
    # 验证第三条中线也过重心
    eq3_check = sp.simplify(eq3.subs({x_g: G_coords[x_g], y_g: G_coords[y_g]}))
    assert eq3_check == 0, f"验证失败! eq3 = {eq3_check}"
    print("验证通过: 三条中线共点于 G = ({}, {})".format(
        G_coords[x_g], G_coords[y_g]))
else:
    print("验证失败: 无公共解")
```

---

## 7. 实现路线图

### 7.1 第一阶段：统一可求值表达式 (P3)

| 任务 | 文件 | 说明 |
|:---|:---|:---|
| 定义 `EvalGeomExpr`、`EvalMode` | `include/lv00/eval_expr.h`（新文件） | 可求值表达式核心结构 |
| 实现 `eval_geom_simplify()` | `src/eval_expr.c`（新文件） | 符号化简（常数折叠 + 合并同类项） |
| 实现 `eval_geom_numeric()` | `src/eval_expr.c` | 数值求值（绑定变量后计算） |
| 实现 `eval_geom_subs()` | `src/eval_expr.c` | 变量替换 |

**预估规模**：约 180 行 C 代码

### 7.2 第二阶段：多目标编译框架 (P3)

| 任务 | 文件 | 说明 |
|:---|:---|:---|
| 定义 `LvCompileTarget`、`lv_compile()` | `include/lv00/compiler.h`（新文件） | 统一编译入口 |
| 实现 SOLVE / PROOF / LATEX 目标 | `src/compiler.c`（新文件） | 三个已有功能的统一调度 |
| 实现 WEB_CANVAS 目标 | `src/compiler_web.c`（新文件） | 导出 JSON 格式的约束图信息 |
| 实现 DOT_GRAPH / GCLC 目标 | `src/compiler_export.c` | 调用现有导出函数 |

**预估规模**：约 250 行 C 代码

### 7.3 第三阶段：Python 代码生成 (P4)

| 任务 | 说明 |
|:---|:---|
| `python_sympy_generate()` | 将约束图转换为独立 SymPy 验证脚本 |
| `python_ctypes_generate()` | 生成 ctypes 绑定代码 |
| `geom_func_compile()` | ExprNode → C 源码编译（build_function 等价） |
| 与 CI/CD 集成 | 自动运行生成的 Python 脚本作为回归测试 |

---

## 8. 关键映射表

### 8.1 Symbolics.jl / ModelingToolkit.jl → Lv-00 概念映射

| Julia SciML | Lv-00 映射 | Lv-00 文件 |
|:---|:---|:---|
| `@variables x y` | `SymbolicCoord` (ALGEBRAIC, 未赋值) | `symbolic_coord.h` |
| `Num` 类型（符号表达式） | `EvalGeomExpr` 可求值表达式 | `eval_expr.h`（新增） |
| `Num` 透明符号/数值切换 | `EvalMode` (SYMBOLIC/NUMERIC/HYBRID) | `eval_expr.h`（新增） |
| `simplify(expr)` | `eval_geom_simplify()` | `eval_expr.h`（新增） |
| `substitute(expr, dict)` | `eval_geom_subs()` | `eval_expr.h`（新增） |
| `build_function(expr, vars)` | `geom_func_compile()` | `compiler.h`（新增） |
| `ODESystem(eqs, ...)` | `ConstraintGraph` (约束方程组) | `constraint_graph.h` |
| `structural_simplify(sys)` | `constraint_graph_simplify()` | `constraint_graph.h` |
| `latexify(sys)` | `proof_export_latex()` | `proof_export.h` |
| `generate_function(sys)` | `lv_compile(graph, LV_TARGET_SOLVE)` | `compiler.h`（新增） |
| `generate_code(sys, CTarget)` | `lv_compile(graph, LV_TARGET_PYTHON_SYMPY)` | `compiler.h`（新增） |
| `Differential(t)(x)` | `symbolic_coord_diff()` | `symbolic_coord.h` |
| Julia JIT 编译 | Emscripten WASM 编译 | `wasm_bridge.c` |

### 8.2 多目标编译输出表

| 编译目标标识 | 输出格式 | 输出文件示例 | 阶段 |
|:---|:---|:---|:---|
| `LV_TARGET_SOLVE` | SymbolicCoord[] (内存) | — | P2 |
| `LV_TARGET_PROOF_VERIFY` | ProofStep[] (内存) | — | P2 |
| `LV_TARGET_WEB_CANVAS` | JSON | `canvas.json` | P3 |
| `LV_TARGET_LATEX_TIKZ` | LaTeX/TikZ | `geometry.tex` | P3 |
| `LV_TARGET_LATEX_PROOF` | LaTeX | `proof.tex` | P3 |
| `LV_TARGET_PYTHON_SYMPY` | Python 脚本 | `verify.py` | P4 |
| `LV_TARGET_PYTHON_CTYPES` | Python 绑定 | `lv00_bridge.py` | P4 |
| `LV_TARGET_DOT_GRAPH` | Graphviz DOT | `graph.dot` | P3 |
| `LV_TARGET_GCLC_GC` | GCLC GC 语言 | `geometry.gc` | P4 |

### 8.3 Python 代码生成对照

| ModelingToolkit.jl 代码生成 | Lv-00 Python 代码生成 | 输出特点 |
|:---|:---|:---|
| `build_function(f, [x,y], target=CTarget())` | `geom_func_compile(expr, vars, COMPILE_TARGET_C_SOURCE)` | 输出 C 源码 |
| `build_function(f, [x,y], target=Standalone())` | (后续扩展) | 输出独立可执行文件 |
| `latexify(eq)` | `proof_export_latex(graph)` | 输出 LaTeX 证明 |
| (无直接等价) | `python_sympy_generate(graph, result, "verify.py")` | 输出 SymPy 验证脚本 |
| (无直接等价) | `python_ctypes_generate(func, "bridge.py")` | 输出 ctypes 绑定 |

---

> **文档结束**
> 本文档详述了 Symbolics.jl 与 ModelingToolkit.jl 的符号-数值混合计算架构如何映射到 Lv-00 的多输出编译系统。核心结论：(1) 引入 `EvalGeomExpr` 统一可求值表达式，借鉴透明符号-数值切换实现 Lv-00 的 A 计划/B 计划在表达式层的一致接口；(2) 借鉴 `build_function` 技术，将频繁使用的几何构造编译为 C 代码，获得数值求值的高性能；(3) 借鉴"单一模型定义→多目标编译"架构，实现 Lv-00 的"单一几何构造→约束求解 + 证明验证 + Web 可视化 + LaTeX 证明 + Python 验证代码"的五路并行输出。

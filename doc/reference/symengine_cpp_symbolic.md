# Lv-00 参考落地设计文档：SymEngine C++ 符号计算内核

> **版本**: 1.0.0
> **日期**: 2026-05-24
> **参考**: SymEngine (github.com/symengine/symengine) —— SymPy 的 C++ 高性能后端，独立符号数学引擎
> **目标**: 借鉴 SymEngine 的表达式 DAG 类型层级、双精度执行路径（任意精度 GMP/FLINT 与 LLVM JIT 共存）、多语言 C ABI 绑定架构，映射到 Lv-00 的 `symbolic_coord.h` 类型层级与求解引擎

---

## 目录

1. [SymEngine 项目概述与 Lv-00 借鉴动机](#1-symengine-项目概述与-lv-00-借鉴动机)
2. [核心借鉴要点：表达式 DAG 类型层级](#2-核心借鉴要点表达式-dag-类型层级)
3. [双精度路径：GMP/FLINT 与 LLVM JIT 并存](#3-双精度路径gmpflint-与-llvm-jit-并存)
4. [多语言绑定架构映射到 Lv-00 互操作层](#4-多语言绑定架构映射到-lv-00-互操作层)
5. [Lv-00 映射方案：从 Basic 到 SymbolicCoord](#5-lv-00-映射方案从-basic-到-symboliccoord)
6. [表达式化简与合一的符号核心](#6-表达式化简与合一的符号核心)
7. [实现路线图](#7-实现路线图)
8. [关键映射表](#8-关键映射表)

---

## 1. SymEngine 项目概述与 Lv-00 借鉴动机

### 1.1 SymEngine 是什么

SymEngine 是 SymPy 的 C++ 高性能重写版本。SymPy 是 Python 生态中最广泛使用的符号数学库之一（纯 Python 实现），SymEngine 将其核心符号计算逻辑用 C++ 重新实现，在保证符号精度的同时获得了数量级的性能提升。SymEngine 的关键设计决策是**完全独立于 SymPy**——可以脱离 Python 独立编译，通过 C ABI 暴露所有功能。

```
SymEngine 架构：
  Python 前端 (SymPy) ──通过 Python 绑定──→ SymEngine C++ 内核
                                             │
                          ┌──────────────────┼──────────────────┐
                          │                  │                  │
                    表达式 DAG 节点     化简/展开/替换     任意精度数学
                    (Basic 基类)       (subs/expand)    (GMP/FLINT)
```

### 1.2 Lv-00 借鉴动机

Lv-00 的符号坐标系统（`symbolic_coord.h`）处理有理数、代数数、二次数和超越数。当前实现缺乏一个统一的表达式树来表达跨多种坐标类型的复合表达式。SymEngine 的 Basic→Add/Mul/Symbol/Integer 派生体系为 Lv-00 的 `SymbolicCoord` 类型层级提供了直接参考：

| 借鉴方向 | SymEngine 特性 | Lv-00 现有基础 | 差距 |
|:---|:---|:---|:---|
| **表达式 DAG** | `Basic` 基类 + 5 类派生 | `SymbolicCoord` + `CoordType` 枚举 | 缺表达式树组合模式 |
| **双精度路径** | GMP/FLINT + LLVM JIT | `RATIONAL` (GMP) + `ALGEBRAIC` (mpz_poly) | 缺 JIT 编译路径 |
| **C ABI 绑定** | C wrapper + Python/Ruby/Julia/Haskell 绑定 | C 内核 | 可作为多语言互操作模板 |
| **MIT 许可** | MIT | MIT (Lv-00) | 许可完全对齐，无引入限制 |
| **化简引擎** | `subs`/`expand`/`simplify` | `symbolic_coord_reduce()` | 缺图重写式化简 |

### 1.3 核心概念对照

```
SymEngine                                  Lv-00
────────────────────────────────────────────────────────────
Basic (抽象基类，引用计数)          →    SymbolicCoord (带 TrustColor 的坐标)
Symbol (符号变量: x, y, z)         →    SymbolicCoord (ALGEBRAIC, 未赋值)
Integer / Rational                 →    SymbolicCoord (RATIONAL)
Add / Mul (复合表达式)              →    ExprNode 表达式树（新增）
Function (sin, cos, sqrt, ...)     →    SymbolicCoord (TRANSCENDENTAL)
subs (变量替换)                     →    symbolic_coord_substitute()
expand (展开)                       →    algebraic_expand() (mpz_poly 层)
DAG 共享 (公共子表达式消除)          →    constraint_graph DAG 节点共享
```

---

## 2. 核心借鉴要点：表达式 DAG 类型层级

### 2.1 SymEngine 的 Basic 类体系

SymEngine 的核心设计是一个引用计数的 DAG（有向无环图）表达式树，所有符号表达式共用同一个 `Basic` 抽象基类：

```
Basic (RCP<const Basic> 引用计数指针)
├── Symbol        ← 符号变量 (x, y, pi, e)
├── Integer       ← 任意精度整数 (GMP mpz_class)
├── Rational      ← 任意精度有理数 (GMP mpq_class)
├── Complex       ← 复数 (GMP mpc_class)
├── Add           ← n 元加法: a + b + c + ...
├── Mul           ← n 元乘法: a * b * c * ...
├── Pow           ← 幂: base^exp
├── Sin/Cos/Tan   ← 三角函数
├── Log/Exp       ← 对数和指数
├── FunctionSymbol ← 任意函数 f(x, y)
└── Constant      ← 数学常数 (pi, E, EulerGamma, ...)

每个 Basic 子类实现:
  - __hash__()      → DAG 节点唯一标识
  - __eq__()        → 表达式等价性（结构相等）
  - compare()       → 字典序比较（用于排序）
  - diff(wrt)       → 符号微分
  - subs(old, new)  → 变量替换
```

### 2.2 DAG 共享的关键价值

SymEngine 的 DAG 架构实现**公共子表达式共享**——相同子表达式在内存中只有一个实例：

```
表达式: (x + y)^2 + (x + y)
DAG 表示:
         Add
        /   \
      Pow    |
      / \    |
   Add  Int  |
   / \  (2)  |
  x   y      |
   \________/  ← 同一个 Add(x,y) 节点被共享

等价 Lv-00 场景:
  ds: s1 = pow(add(x, y), 2) + add(x, y);
  → constraint_graph 自动检测 add(x,y) 的去重
  → 合一引擎通过 DAG 哈希发现: 两个 add(x,y) 是同一个节点
```

### 2.3 表达式构造与 DAG 生产的对照

```c
/**
 * @brief SymEngine 风格表达式节点 —— Lv-00 等价设计
 *
 * 借鉴 SymEngine 的 Basic 基类 + 派生体系，
 * 为 SymbolicCoord 设计表达式树组合层。
 * 每个表达式节点是不可变的（immutable），
 * 通过哈希表实现公共子表达式共享。
 */
typedef enum {
    EXPR_SYMBOL,        /* 符号变量（对应 SymEngine::Symbol） */
    EXPR_INTEGER,       /* 任意精度整数（对应 SymEngine::Integer） */
    EXPR_RATIONAL,      /* 任意精度有理数（对应 SymEngine::Rational） */
    EXPR_ADD,           /* n 元加法（对应 SymEngine::Add） */
    EXPR_MUL,           /* n 元乘法（对应 SymEngine::Mul） */
    EXPR_POW,           /* 幂运算（对应 SymEngine::Pow） */
    EXPR_FUNC,          /* 函数调用 sin/cos/sqrt/atan2... */
    EXPR_CONSTANT       /* 数学常数 pi, e, ... */
} ExprNodeKind;

typedef struct ExprNode {
    ExprNodeKind kind;
    int ref_count;                      /* 引用计数（对标 RCP） */
    uint64_t hash;                      /* 缓存哈希值 */

    union {
        /* EXPR_SYMBOL: 符号变量 */
        struct { char *name; } symbol;

        /* EXPR_INTEGER / EXPR_RATIONAL: 数值 */
        SymbolicCoord *value;           /* 共享 SymbolicCoord 现有值类型 */

        /* EXPR_ADD / EXPR_MUL: n 元操作 */
        struct {
            struct ExprNode **terms;    /* 子表达式数组 */
            int term_count;
            /* 规范化存储：Add 按字典序排序，合并同类项 */
        } nary;

        /* EXPR_POW: 幂运算 */
        struct {
            struct ExprNode *base;
            struct ExprNode *exponent;
        } power;

        /* EXPR_FUNC: 函数调用 */
        struct {
            char *func_name;            /* "sin", "cos", "sqrt", ... */
            struct ExprNode **args;
            int arg_count;
        } func;
    } data;
} ExprNode;

/**
 * @brief 表达式工厂 —— 借鉴 SymEngine 的 RCP 工厂模式
 *
 * 每个构造调用自动检查 DAG 缓存：
 *  - 如果已存在等价子表达式 → 返回共享指针（引用计数 +1）
 *  - 如果不存在 → 创建新节点并加入缓存
 *
 * 这实现了 SymEngine 的"表达式恒等性"：
 *    expr_node_add(x, y) 两次调用返回同一个指针
 */
ExprNode *expr_node_add(ExprNode *a, ExprNode *b);
ExprNode *expr_node_mul(ExprNode *a, ExprNode *b);
ExprNode *expr_node_pow(ExprNode *base, ExprNode *exp);
ExprNode *expr_node_symbol(const char *name);
ExprNode *expr_node_from_coord(const SymbolicCoord *coord);
```

---

## 3. 双精度路径：GMP/FLINT 与 LLVM JIT 并存

### 3.1 SymEngine 的双路径哲学

SymEngine 的一个核心设计是**双精度路径共存**——同一个符号表达式可以选择：

| 路径 | 后端 | 精度 | 性能 | 用途 |
|:---|:---|:---|:---|:---|
| **任意精度 (arbitrary)** | GMP + FLINT | 无界（受内存限制） | 较慢但保证精确 | 符号化简、等价性检查、Gröbner 基 |
| **机器精度 (machine)** | LLVM JIT → 本地代码 | IEEE 754 double | 极快（接近手写 C） | 数值求值、大规模浮点运算、Web 预览 |
| **混合** | 符号化简 + 数值回退 | 精确 + 近似 | 平衡 | 约束求解的规模调度 |

这一设计与 Lv-00 的 **A 计划（精确符号）与 B 计划（数值近似）** 高度一致。

### 3.2 Lv-00 的双路径深化设计

```c
/**
 * @brief Lv-00 双精度执行路径 —— 借鉴 SymEngine 的双后端
 *
 * SymEngine 提供 "eval" 级别的双路径切换。
 * Lv-00 将此扩展到求解器级别的路径选择。
 *
 * 路径选择矩阵:
 *   ┌────────────┬──────────────────┬──────────────────┐
 *   │ 坐标类型   │ A 计划 (符号)     │ B 计划 (数值)     │
 *   ├────────────┼──────────────────┼──────────────────┤
 *   │ RATIONAL   │ GMP 精确有理数    │ double 近似      │
 *   │ ALGEBRAIC  │ mpz_poly + Gröbner│ 多项式求根 → double│
 *   │ QUADRATIC  │ 精确根式保持       │ 数值 √ 计算      │
 *   │ TRANSCEND. │ 符号常数保持       │ 数值近似 (MPFR)  │
 *   └────────────┴──────────────────┴──────────────────┘
 */
typedef enum {
    PRECISION_PATH_ARBITRARY,   /* A 计划：GMP/FLINT 任意精度 */
    PRECISION_PATH_MACHINE,     /* B 计划：double/LLVM JIT */
    PRECISION_PATH_HYBRID       /* 混合：符号化简后数值计算 */
} PrecisionPath;

/**
 * @brief 求解器精度调度 —— 借鉴 SymEngine 的 eval 双路径
 *
 * 根据输入坐标的精度需求和问题规模，
 * 自动选择 A 计划或 B 计划。
 *
 * 触发条件:
 *   - 所有坐标为 RATIONAL + 多项式度数 ≤ 2 → A 计划（精确 Gröbner）
 *   - 任一坐标为 ALGEBRAIC + 度数 ≥ 3 → 尝试 A 计划，超时后切 B 计划
 *   - 坐标数量 > 50 → B 计划（符号求解器难以处理大规模）
 */
PrecisionPath select_precision_path(
    int coord_count,
    const SymbolicCoord **coords,
    int max_poly_degree);
```

### 3.3 双路径的 Web 预览应用

SymEngine 的 LLVM JIT 路径的另一个重要用途是**实时数值预览**——在不损失符号精确性的前提下，用 JIT 编译快速生成 Web GUI 上的可视化坐标。Lv-00 已有 Web GUI 计划，这一模式可以直接映射：

```
符号构造          →   约束图
SymbolicCoord           │
(A 计划保证精确)        ├── 精确求解 (Gröbner/SMT)
                        │       │
                        │       ▼
                        │   精确坐标 (TrustColor GREEN)
                        │
                        └── 快速数值求值 (double → Web 预览)
                                │
                                ▼
                            预览坐标 (TrustColor BLUE, 临时)
```

---

## 4. 多语言绑定架构映射到 Lv-00 互操作层

### 4.1 SymEngine 的 C ABI 绑定模式

SymEngine 通过纯 C 包装层（`symengine/cwrapper.h`）向外暴露所有功能，然后每个语言绑定只需要包装 C ABI：

```
SymEngine C++ 内核
    │
    └── C ABI 包装 (cwrapper.h / cwrapper.cpp)
            │    将 RCP<Basic> 转换为 opaque void* 句柄
            │    所有函数使用 C 调用约定
            │
            ├── Python 绑定 (symengine.py)  ← ctypes/CFFI
            ├── Ruby 绑定   (symengine.rb)  ← FFI
            ├── Julia 绑定  (SymEngine.jl)  ← ccall
            └── Haskell 绑定 (symengine-hs) ← FFI

设计原则:
  1. C ABI 是唯一的公开接口（ABI 稳定）
  2. 每个语言绑定只做类型转换 + 内存管理包装
  3. 绑定代码量 < 500 行（每个语言）
```

### 4.2 Lv-00 的等价多语言互操作设计

Lv-00 的核心是 C 语言内核（`include/lv00/` 和 `src/` 目录），天然具备 C ABI 稳定性。借鉴 SymEngine 的模式，可以构建一个标准化的多语言互操作层：

```c
/**
 * @brief Lv-00 C ABI 互操作层 —— 借鉴 SymEngine 的 cwrapper 模式
 *
 * 设计原则:
 *   1. 所有导出函数使用 C 调用约定 (extern "C")
 *   2. 内部指针通过 opaque 句柄暴露 (lv_handle_t)
 *   3. 内存管理由 Lv-00 内核负责，绑定层不持有所有权
 *   4. 错误通过返回码 + lv_get_last_error() 传递
 */

/* opaque 句柄类型 */
typedef void *lv_handle_t;            /* 通用句柄 */
typedef void *lv_graph_handle_t;      /* ConstraintGraph 句柄 */
typedef void *lv_node_handle_t;       /* 约束图节点句柄 */
typedef void *lv_coord_handle_t;      /* SymbolicCoord 句柄 */
typedef void *lv_solver_handle_t;     /* Solver 会话句柄 */

/* 生命周期管理 */
lv_graph_handle_t  lv_graph_create(void);
void               lv_graph_destroy(lv_graph_handle_t g);
lv_node_handle_t   lv_graph_add_point(lv_graph_handle_t g,
                                       const char *name,
                                       lv_coord_handle_t x,
                                       lv_coord_handle_t y);
lv_coord_handle_t  lv_coord_from_double(double x, double y);
const char        *lv_get_last_error(void);

/* 求解 */
int lv_graph_solve(lv_graph_handle_t g, int timeout_ms);
lv_coord_handle_t lv_node_get_x(lv_graph_handle_t g, lv_node_handle_t node);

/* 导出 */
int lv_graph_export_dot(lv_graph_handle_t g, const char *filepath);
int lv_graph_export_latex(lv_graph_handle_t g, const char *filepath);
```

### 4.3 多语言绑定对照表

| 绑定语言 | SymEngine 模式 | Lv-00 等价 |
|:---|:---|:---|
| **Python** | ctypes 调用 C ABI | `lv00.py`（ctypes）→ Lv-00 求解器 |
| **Julia** | `ccall` 直接调用 | `Lv00.jl`（ccall） |
| **JavaScript/Web** | Emscripten → WASM | `lv00-wasm.js`（Emscripten 编译 C 内核） |
| **Rust** | `extern "C"` FFI | `lv00-sys` crate |
| **Haskell** | FFI + Storable | `lv00-hs` cabal 包 |

---

## 5. Lv-00 映射方案：从 Basic 到 SymbolicCoord

### 5.1 类型层级映射

```
SymEngine 类型层级                Lv-00 对应                 映射文件
──────────────────────────────────────────────────────────────────────
Basic (抽象基类)               →  SymbolicCoord                      symbolic_coord.h
  ├── Integer (GMP 整数)       →  RATIONAL (mpz_t)                  symbolic_coord.h
  ├── Rational (GMP 有理数)    →  RATIONAL (mpq_t)                  symbolic_coord.h
  ├── Complex (GMP 复数)       →  (尚未实现，预留 COMPLEX 类型)       symbolic_coord.h
  ├── Symbol (符号变量)        →  ALGEBRAIC (未赋值/变量)            symbolic_coord.h
  ├── Add/Mul (复合)            →  ExprNode (EXPR_ADD/EXPR_MUL)     【新增】expr_node.h
  ├── Pow (幂)                  →  ExprNode (EXPR_POW)              【新增】expr_node.h
  ├── Sin/Cos/... (函数)        →  TRANSCENDENTAL                    symbolic_coord.h
  ├── Constant (pi/e)           →  符号常数 (NAME_GIVEN)              symbolic_coord.h
  └── FunctionSymbol            →  FuncBlock 调用                     func_block.h
```

### 5.2 SymbolicCoord 与 ExprNode 的关系

`SymbolicCoord` 作为**叶节点**（原子值：有理数、代数数、二次数、超越数），`ExprNode` 作为**组合节点**（复合表达式：Add/Mul/Pow/Func）。两者形成两层架构：

```
ExprNode 表达式树 (组合层)
    │
    │ 叶节点
    ▼
SymbolicCoord (原子值层)
    ├── RATIONAL       (有理数)
    ├── ALGEBRAIC      (代数数，含 mpz_poly_t minimal_poly)
    ├── QUADRATIC      (二次数)
    └── TRANSCENDENTAL (超越数，含 sin/cos/pi/e...)
```

### 5.3 化简引擎映射

SymEngine 提供了 `subs`（替换）、`expand`（展开）、`simplify`（化简）三个核心化简操作。Lv-00 的等价实现：

| SymEngine 操作 | Lv-00 函数 | 实现策略 |
|:---|:---|:---|
| `expr.subs(x, 3)` | `symbolic_coord_substitute(coord, var, val)` | 遍历 mpz_poly_t，替换变量 |
| `expr.expand()` | `algebraic_expand(algebraic)` | 多项式展开（mpz_poly 乘法 + 合并同类项） |
| `expr.diff(x)` | `symbolic_coord_diff(coord, var_name)` | 多项式形式求导（幂规则） |
| `simplify(expr)` | `constraint_graph_simplify(graph)` | 图重写 → 合一 → 常数折叠 |

---

## 6. 表达式化简与合一的符号核心

### 6.1 SymEngine 的合一（表达式等价性）

SymEngine 的表达式等价性检查（`Basic.__eq__`）基于**结构相等 + 规范化存储**：

```
规范化规则:
  Add(a, b) → 按字典序排序: Add(x, 2) ≠ Add(2, x) → 统一为 Add(2, x)
  Mul(a, b) → 系数前置 + 排序: Mul(3, x, y) 统一形式
  常数折叠: Add(2, 3) → Integer(5)
  零元消除: Add(x, 0) → x
  幂合并: Mul(x, x) → Pow(x, 2)
```

Lv-00 的合一引擎（`constraint_graph.h` 中的合一操作）可以直接借鉴这些规范化规则：

```c
/**
 * @brief ExprNode 规范化 —— 借鉴 SymEngine 的结构规范化
 *
 * 在构建表达式树时自动执行:
 *   1. 排序：Add/Mul 的子项按字典序排列
 *   2. 常数折叠：两个常数值的运算立即求值
 *   3. 零元消除：Add(x, 0) → x; Mul(x, 1) → x; Mul(x, 0) → 0
 *   4. 幂合并：Mul(x, x) → Pow(x, 2)（可配置开关）
 *
 * 规范化保证:
 *   - 如果两个表达式数学等价（且规范化规则充分），则它们的 DAG 哈希相等
 *   - 合一可直接用指针相等（引用相同 DAG 节点）判定等价
 */
ExprNode *expr_node_normalize(ExprNode *node);

/**
 * @brief 表达式合一 —— 借鉴 SymEngine 的 __eq__
 *
 * 两个 ExprNode 等价当且仅当:
 *   - 它们是同一个指针（DAG 共享）→ 直接返回 true
 *   - 或者 kind 相同且所有子项递归等价
 *
 * 这个函数在 constraint_graph 的合一阶段用于检查两个约束是否等价。
 */
bool expr_node_unify(const ExprNode *a, const ExprNode *b);
```

### 6.2 从 SymbolicCoord 到 ExprNode 的桥接

```c
/**
 * @brief 将 SymbolicCoord 提升为 ExprNode 叶节点
 *
 * SymEngine 等价: Symbol("x") → Basic 实例
 * Lv-00 等价:   SymbolicCoord (ALGEBRAIC, name="x") → ExprNode (EXPR_SYMBOL)
 *
 * 四种坐标类型的映射:
 *   RATIONAL → EXPR_RATIONAL (叶节点)
 *   ALGEBRAIC → EXPR_SYMBOL (如果未赋值) 或 EXPR_RATIONAL (如果已赋值)
 *   QUADRATIC → EXPR_FUNC("sqrt", [EXPR_RATIONAL(判别式)])
 *   TRANSCENDENTAL → EXPR_FUNC(函数名, [参数列表])
 */
ExprNode *expr_node_from_symbolic_coord(const SymbolicCoord *coord);

/**
 * @brief 将 ExprNode 回退为 SymbolicCoord（求值）
 *
 * 如果表达式树可化为单一数值/符号：
 *   ExprNode(Add, [RATIONAL(2), RATIONAL(3)]) → RATIONAL(5)
 * 如果无法化简（含未赋值变量）：
 *   保持为 ExprNode 并挂载到 SymbolicCoord 的派生数据中
 */
SymbolicCoord *symbolic_coord_from_expr_node(const ExprNode *node);
```

---

## 7. 实现路线图

### 7.1 第一阶段：ExprNode 表达式树核心（P3）

| 任务 | 文件 | 说明 |
|:---|:---|:---|
| 定义 `ExprNodeKind`、`ExprNode` | `include/lv00/expr_node.h`（新文件） | 表达式树的核心数据结构 |
| 实现 `expr_node_add/mul/pow/symbol` | `src/expr_node.c`（新文件） | 工厂函数 + DAG 缓存 |
| 实现 `expr_node_normalize()` | `src/expr_node.c` | 规范化（排序 + 常数折叠） |
| 实现 `expr_node_from_symbolic_coord()` | `src/expr_node.c` | SymbolicCoord → ExprNode 桥接 |
| 单元测试：DAG 共享 + 常数折叠 | `tests/test_expr_node.c` | 验证指针共享和等价性 |

**预估规模**：约 200 行 C 代码

### 7.2 第二阶段：C ABI 互操作层（P3）

| 任务 | 文件 | 说明 |
|:---|:---|:---|
| 定义 opaque 句柄类型 | `include/lv00/interop.h`（新文件） | `lv_handle_t` 等导出句柄 |
| 实现生命周期管理函数 | `src/interop.c`（新文件） | create/destroy/solve/export |
| Python 绑定示例 | `bindings/python/lv00.py`（新文件） | ctypes 包装，验证 C ABI |
| 集成测试：Python → C → 求解 → 结果 | `tests/test_interop.py` | 端到端互操作验证 |

**预估规模**：约 150 行 C + 100 行 Python

### 7.3 第三阶段：化简引擎集成（P3-P4）

| 任务 | 说明 |
|:---|:---|
| `expr_node_unify()` 与合一引擎集成 | constraint_graph 约束重复检测 |
| `expr_node_subs()` 变量替换 | 多项式替换 + 符号传播 |
| 图重写规则注册 | symengine 风格的 pattern → replacement 规则 |

---

## 8. 关键映射表

### 8.1 SymEngine → Lv-00 概念映射

| SymEngine | Lv-00 映射 | Lv-00 文件 |
|:---|:---|:---|
| `Basic`（抽象基类） | `SymbolicCoord`（原子） + `ExprNode`（组合） | `symbolic_coord.h`, `expr_node.h`（新增） |
| `RCP<const Basic>` 引用计数 | `SymbolicCoord` 的 `TrustColor + ref_count` | `symbolic_coord.h` |
| `Integer` / `Rational` (GMP) | `RATIONAL` 类型 (GMP mpz_t/mpq_t) | `symbolic_coord.h` |
| `Symbol`（符号变量） | `ALGEBRAIC` 类型（`NAME_GIVEN`） | `symbolic_coord.h` |
| `Add` / `Mul` (n 元操作) | `ExprNode` (EXPR_ADD / EXPR_MUL) | `expr_node.h`（新增） |
| `Function`（sin/cos/sqrt） | `TRANSCENDENTAL` 类型 | `symbolic_coord.h` |
| `subs()` 替换 | `symbolic_coord_substitute()` | `symbolic_coord.h` |
| `expand()` 展开 | `algebraic_expand()` (mpz_poly 层) | `symbolic_coord.h` |
| `cwrapper` C ABI | `interop.h` C ABI 导出层 | `interop.h`（新增） |
| Python 绑定 (SymPy) | `lv00.py` (ctypes) | `bindings/python/lv00.py`（新增） |
| LLVM JIT 路径 | `scheduler_select_backend()` 数值路径 | `engine_scheduler.cpp` |
| MIT License | MIT License | Lv-00 根许可 |

### 8.2 SymbolicCoord → ExprNode 类型提升表

| SymbolicCoord 类型 | 提升为 ExprNode | 示例 |
|:---|:---|:---|
| `RATIONAL` (赋值的整数/分数) | `EXPR_RATIONAL` | `RATIONAL(3)` → `ExprNode{kind=RATIONAL, value=3}` |
| `RATIONAL` (未赋值的符号) | `EXPR_SYMBOL` | `RATIONAL(name="x")` → `ExprNode{kind=SYMBOL, name="x"}` |
| `ALGEBRAIC` (mpz_poly_t) | `EXPR_SYMBOL` 或 `EXPR_POW` | `x^2 - 2 = 0 的根` → `ExprNode{kind=SYMBOL, name="sqrt2"}` |
| `QUADRATIC` (a*x^2 + b*x + c) | `EXPR_FUNC("sqrt", ...)` | `(-b + sqrt(b^2-4ac)) / (2a)` |
| `TRANSCENDENTAL` (sin/pi) | `EXPR_FUNC` 或 `EXPR_CONSTANT` | `TRANSCENDENTAL(pi)` → `ExprNode{kind=CONSTANT, name="pi"}` |

### 8.3 多语言绑定技术栈

| 语言 | 绑定技术 | 关键函数 | 行数估计 |
|:---|:---|:---|:---|
| **C** | 原生 API | `lv_graph_create/solve/destroy` | 已有 |
| **Python** | ctypes | `lv00.Graph.create()` → `ctypes` 调用 | ~80 行 |
| **Julia** | ccall | `Lv00.graph_create()` → `ccall` | ~60 行 |
| **JavaScript** | Emscripten WASM | `Lv00Wasm.Graph.create()` | ~100 行 |
| **Rust** | `extern "C"` FFI | `lv00_sys::graph_create()` | ~100 行 |

---

> **文档结束**
> 本文档详述了 SymEngine C++ 符号计算引擎如何映射到 Lv-00 的 `symbolic_coord.h` 类型层级与求解引擎。核心结论：(1) 引入 `ExprNode` 表达式树作为 `SymbolicCoord` 的组合层，形成"原子坐标 + 复合表达式"的两层架构；(2) 借鉴 GMP/FLINT + LLVM JIT 双精度路径，实现 Lv-00 的 A 计划（精确符号）与 B 计划（数值近似）的无缝切换；(3) 通过 `interop.h` C ABI 互操作层，借鉴 SymEngine 的多语言绑定模式，为 Lv-00 建立 Python/Julia/JS/Rust 的统一外部接口。MIT 许可完全对齐，无引入限制。

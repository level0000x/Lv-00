# Lv-00 参考设计：libfive 函数表示法（F-Rep）建模

> **版本**: 1.0.0  
> **日期**: 2026-05-24  
> **参考**: [libfive](https://github.com/libfive/libfive) —— 基于函数表示法的 CAD 内核与几何建模框架  
> **目标**: 借鉴 libfive 的函数表示法（F-Rep），应用于 Lv-00——用数学函数定义几何体、将几何证明转化为函数性质分析，映射到 `symbolic_coord.h`

---

## 目录

1. [项目概述](#1-项目概述)
2. [核心借鉴要点](#2-核心借鉴要点)
3. [Lv-00 映射方案](#3-lv-00-映射方案)
4. [实现路线图](#4-实现路线图)

---

## 1. 项目概述

### 1.1 libfive 是什么

libfive 是 Matt Keeter 开发的基于函数表示法（Function Representation, F-Rep）的 CAD 内核和几何建模框架。传统 CAD 系统使用边界表示法（B-Rep）存储点/线/面的显式拓扑关系。libfive 走了一条完全不同的路——**每个几何体由一个连续数学函数 f(x, y, z) 定义**，体内部 = {p | f(p) <= 0}，体边界 = {p | f(p) = 0}。

```
// libfive 示例：用数学函数定义一个环（torus）
Tree t = circle(1, {0, 0, 0});  // r=1 的圆
// torus = (1 - sqrt(x^2 + y^2))^2 + z^2 - r^2

// 布尔运算 = 函数的 min/max 运算
Tree rounded_box = box({2, 1, 0.5}) + 0.1;  // 偏移 = 圆角
Tree union_ab = min(a, b);  // 并集
Tree intersect_ab = max(a, b);  // 交集
```

libfive 的关键洞察：**几何构造 = 函数组合**。每个几何操作（平移、旋转、布尔运算、混合）都对应一个函数变换。这为 Lv-00 提供了一个统一的代数视角来看待几何构造——因为 Lv-00 中的每个几何约束本质上也是关于坐标的多项式方程。

### 1.2 为什么借鉴 libfive

Lv-00 的 `symbolic_coord.h` 已经定义了符号坐标系统（有理数、代数数、二次数、超越数），`solver.h` 处理多项式方程。但当前几何体的定义仍然以"点+线+约束"的显式拓扑方式表达，缺乏 libfive 那种**用单一半径函数定义整个几何体**的代数视角。将 F-Rep 引入 Lv-00 意味着：

1. 三角形 = 三个半平面函数的交集（min）
2. 圆 = 距离函数的零等值面
3. 交点 = 两个函数的零等值面共同满足的点

这使得几何证明可以被转化为**函数性质分析**——证明三条中线共点 = 证明三个重心函数的零等值面包含同一点。

---

## 2. 核心借鉴要点

### 2.1 F-Rep 的核心思想

| libfive 概念 | Lv-00 对应概念 | 映射说明 |
|-------------|---------------|---------|
| 有符号距离函数 (SDF) f(p) | `ConstraintType` 的方程形式 | 每个约束可写为 f(x,y)=0 的形式 |
| 体内部 {p \| f(p) <= 0} | `CONTAINMENT` 约束 | 点在区域内 = f(point) <= 0 |
| 体边界 {p \| f(p) = 0} | `INCIDENCE` 约束 | 点在线上 = f(point) = 0 |
| 布尔并集 `min(a, b)` | 两个区域的并集约束 | 点在其中任一区域内 |
| 布尔交集 `max(a, b)` | 多个半平面约束的交集 | 点在所有区域内的公共部分 |
| 偏移 `offset(shape, r)` | `SymbolicCoord` 的偏移运算 | 参数 r 可以是符号值 |
| `eval(f, p)` = 数值 | `solvel_algebraic_system()` 的求解 | 找到 f(p)=0 的 p |

### 2.2 F-Rep 的函数组合代数

libfive 将几何操作编码为函数变换，形成一个"几何构造代数"。Lv-00 可以将这一思想映射到约束图的构造步骤：

| 几何操作 | libfive F-Rep 函数变换 | Lv-00 对应 `SymbolicCoord` 运算 |
|---------|----------------------|-------------------------------|
| 平移 T | `f(p - T)` | `symbolic_coord_add(c, vector)` |
| 旋转 R | `f(R^-1 * p)` | `symbolic_coord_rotate(c, angle, center)` |
| 缩放 S | `f(p / S) * S` | `symbolic_coord_mul(c, factor)` |
| 并集 A | B | `min(f_A, f_B)` | `symbolic_coord_min(c_a, c_b)`（取较小值） |
| 交集 A |&| B | `max(f_A, f_B)` | 多个约束的联立求解 |
| 差集 A - B | `max(f_A, -f_B)` | 一个约束成立且另一个不成立 |
| 圆角混合 | `f_A + f_B - sqrt(f_A^2 + f_B^2) - r` | `symbolic_coord_smooth_union()`（超越数） |

### 2.3 F-Rep 的证明视角

libfive 最有启发性的贡献是**几何性质的函数化表达**：

```
传统几何命题:              F-Rep 等价形式:
  "P 是 AB 的中点"    ←→   |P-A| = |P-B| 且 P 在 AB 上
                          →  f(P) = |P-A|^2 - |P-B|^2 = 0
                          且 g(P) = 共线性方程 = 0

  "三条中线共点"      ←→   三条中线的联立方程存在公共解
                          →  存在 G 使得 mediator_equation(G, A, BC_mid) = 0
                          且 mediator_equation(G, B, CA_mid) = 0
                          且 mediator_equation(G, C, AB_mid) = 0
```

这意味着 Lv-00 的证明引擎本质上是在做**多变量多项式函数系统的公共零点分析**——与 Gröbner 基方法的数学基础完全一致。

---

## 3. Lv-00 映射方案

### 3.1 F-Rep 函数与 SymbolicCoord 的对接

libfive 的核心是一个函数树（Tree），每个节点是一个数学运算。Lv-00 的 `SymbolicCoord` 已具备代数数（`mpz_poly_t minimal_poly`）——这本质上就是一元函数的零等值面定义。将 F-Rep 的思想扩展到多元函数：

```c
/**
 * @brief F-Rep 函数节点类型（libfive 风格）
 *
 * 将几何体的有符号距离函数表示为表达式树，
 * 每个节点是一个可求值的代数操作。
 * 树的叶节点是 SymbolicCoord（变量或常数值）。
 */
typedef enum {
    FOP_CONSTANT,       /* 常数值 */
    FOP_VARIABLE,       /* 符号变量（如点坐标 x, y） */
    FOP_ADD,            /* f + g */
    FOP_SUB,            /* f - g */
    FOP_MUL,            /* f * g */
    FOP_DIV,            /* f / g */
    FOP_SQRT,           /* sqrt(f) */
    FOP_SQUARE,         /* f^2 —— 距离函数的关键操作 */
    FOP_MIN,            /* min(f, g) —— 布尔并集 */
    FOP_MAX,            /* max(f, g) —— 布尔交集 */
    FOP_NEG,            /* -f —— 求补 */
    FOP_ABS,            /* |f| */
    FOP_OFFSET,         /* f - r —— 偏移（圆角） */
} FRepOpType;

/**
 * @brief F-Rep 函数表达式树节点（libfive 风格）
 *
 * 每个 FRepNode 定义一个可求值的数学函数。
 * 叶节点对应 SymbolicCoord（已知坐标 → 常量，未知坐标 → 变量）。
 */
typedef struct FRepNode {
    FRepOpType op;
    union {
        SymbolicCoord *constant;            /* FOP_CONSTANT */
        struct { char *name; int var_id; } variable; /* FOP_VARIABLE */
        struct { struct FRepNode *lhs; struct FRepNode *rhs; } binary; /* +,-,*,/,min,max */
        struct { struct FRepNode *child; } unary; /* sqrt, square, neg, abs */
        struct { struct FRepNode *shape; SymbolicCoord *radius; } offset;
    } data;
} FRepNode;
```

### 3.2 几何构造 → F-Rep 翻译

将 Lv-00 的常见几何构造翻译为 F-Rep 函数定义：

```c
/**
 * @brief 将几何约束翻译为 F-Rep 函数表达式
 *
 * 每种 ConstraintType 都可以表达为一个 F-Rep 函数零点条件。
 * 这个翻译层使得 Lv-00 可以使用 libfive 的统一代数视角来看待所有几何操作。
 */

/* 1. 两点间线段的中垂线: f(x,y) = 0
 *    f(x,y) = (x - mx)^2 + (y - my)^2 - (A到B距离/2)^2 = 0
 *    其中 (mx, my) 是 AB 的中点
 */
FRepNode *frep_perpendicular_bisector(const SymbolicCoord *A,
                                       const SymbolicCoord *B);

/* 2. 圆: f(x,y) = (x - cx)^2 + (y - cy)^2 - r^2 = 0 */
FRepNode *frep_circle(const SymbolicCoord *center, SymbolicCoord *radius);

/* 3. 半平面（在直线一侧）: f(x,y) = ax + by + c >= 0
 *    这是三角形定义的基石 —— 三角形 = 三个半平面的交集
 */
FRepNode *frep_halfplane(const SymbolicCoord *p1,
                          const SymbolicCoord *p2,
                          const SymbolicCoord *test_point);

/* 4. 三角形内部: f = max(h1, h2, h3)
 *    三个半平面的布尔交集
 */
FRepNode *frep_triangle_interior(const SymbolicCoord *A,
                                  const SymbolicCoord *B,
                                  const SymbolicCoord *C);

/* 5. 点在区域内: frep_eval(f_region, P) <= 0
 *    这就是 CONTAINMENT 约束的 F-Rep 表达
 */
bool frep_eval_contains(const FRepNode *region, const SymbolicCoord *point);
```

### 3.3 几何证明 → 函数性质分析

借助 F-Rep 的函数化视角，几何证明可转化为函数性质分析：

```
传统证明:                        F-Rep 等价:

命题: 三角形ABC的三条中线共点
─────────────────────────────────────
前置条件:
  triangle(A, B, C)
    → f_triangle = max(h_AB, h_BC, h_CA)
    验证: f_triangle(A) = 0, f_triangle(B) = 0, f_triangle(C) = 0

辅助构造:
  M_AB = midpoint(A, B)
    → f_med_AB(P) = |P - M_AB|^2 - 条件方程 = 0
  M_BC = midpoint(B, C)
  M_CA = midpoint(C, A)
  med_A = segment(A, M_BC)
    → f_med_A(P) = 共线性(A, M_BC, P) 的函数表达 = 0

证明目标: concurrent(med_A, med_B, med_C)
    → 存在 G 使得 f_med_A(G)=0, f_med_B(G)=0, f_med_C(G)=0 同时成立
    → 即三个函数的公共零点

Groebner 基证明:
    → ideal = <f_med_A, f_med_B, f_med_C>
    → 计算 Gröbner 基
    → 如果 Gröbner 基不含矛盾方程，则公共零点存在
    → 证毕
```

### 3.4 映射到现有 symbolic_coord.h

| `symbolic_coord.h` 现有结构 | F-Rep 借鉴后的角色 |
|---------------------------|-------------------|
| `CoordType` (RATIONAL/ALGEBRAIC/QUADRATIC/TRANSCENDENTAL) | F-Rep 函数的求值结果类型 |
| `Rational` / `Algebraic` / `Quadratic` / `Transcendental` | F-Rep 叶节点的值类型 |
| `symbolic_coord_add/mul/sub/div` | F-Rep 二元操作 `FOP_ADD/MUL/SUB/DIV` |
| `symbolic_coord_sqrt()` | F-Rep `FOP_SQRT`（距离函数的核心） |
| `mpz_poly_t minimal_poly` | F-Rep 零等值面的代数表示 |
| `TrustColor` | F-Rep 函数的构造性信任度（同构映射） |
| `BIT_CUTOFF_THRESHOLD` | F-Rep 函数求值的精度控制 |

### 3.5 F-Rep 求值管线

```
几何体定义 (FRepNode 树)
    │
    ▼
┌──────────────────────────┐
│ 1. 符号求值 (Symbolic)    │  ← 变量保持为符号表达式
│    f(x, y) → 多项式       │     (利用 mpz_poly_t 存储)
└────────────┬─────────────┘
             │
             ▼
┌──────────────────────────┐
│ 2. 数值求值 (Numeric)     │  ← 将符号坐标替换为具体数值
│    f(3.0, 4.0) → 0.0     │     (利用 symbolic_coord_to_double())
└────────────┬─────────────┘
             │
             ▼
┌──────────────────────────┐
│ 3. 零点分析 (Zero Set)    │  ← 联立求解多个函数的公共零点
│    f_A=0, f_B=0, f_C=0   │     → Gröbner 基 / SMT 求解
└────────────┬─────────────┘
             │
             ▼
      解 (SymbolicCoord[])
```

---

## 4. 实现路线图

### 4.1 第一阶段：F-Rep 表达式树核心（P2）

| 任务 | 文件 | 说明 |
|------|------|------|
| 定义 `FRepNode`、`FRepOpType` | `include/lv00/frep.h`（新文件） | F-Rep 表达式树的核心数据结构 |
| 实现 `frep_node_create/destroy` | `src/frep.c`（新文件） | 创建/销毁 F-Rep 节点 |
| 实现 `frep_eval_symbolic()` | `src/frep.c` | 符号求值 → 多项式表达式 |
| 实现 `frep_eval_numeric()` | `src/frep.c` | 数值求值 → double 结果 |

**预估规模**：约 350 行 C 代码

### 4.2 第二阶段：几何构造的 F-Rep 翻译（P2）

| 任务 | 文件 | 说明 |
|------|------|------|
| 实现 `frep_perpendicular_bisector()` | `src/frep_geometry.c`（新文件） | 中垂线 → F-Rep |
| 实现 `frep_circle()` | `src/frep_geometry.c` | 圆 → F-Rep |
| 实现 `frep_halfplane()` | `src/frep_geometry.c` | 半平面 → F-Rep |
| 实现 `frep_triangle_interior()` | `src/frep_geometry.c` | 三角形内部 → F-Rep |
| 将 `ConstraintType` 翻译为 F-Rep 函数对 | `src/frep_geometry.c` | 每种约束类型的 F-Rep 等价表达 |

**预估规模**：约 300 行 C 代码

### 4.3 第三阶段：证明引擎集成（P3+）

| 任务 | 说明 |
|------|------|
| 实现 F-Rep 公共零点分析 | 多个 F-Rep 函数的联立零点 = Gröbner 基 |
| F-Rep → 约束图的自动转换 | 从 F-Rep 表达式树生成 `ConstraintGraph` 的节点和约束 |
| 将 A 计划/B 计划与 F-Rep 精度关联 | F-Rep 的符号求值对应 A 计划（精确），数值求值对应 B 计划（近似） |

---

## 附录 A：libfive F-Rep 与 Lv-00 SymbolicCoord 对照

| libfive API | Lv-00 对应 | 类型差异 |
|------------|-----------|---------|
| `Tree::constant(1.0)` | `symbolic_coord_from_double(1.0)` → `RATIONAL` | libfive 使用浮点，Lv-00 使用 GMP 精确有理数 |
| `Tree::var()` | `SymbolicCoord` 未赋值 → `ALGEBRAIC` | libfive 变量无类型，Lv-00 有 4 种坐标类型 |
| `min(a, b)` | `frep_node_create(FOP_MIN, a, b)` | 语义等价 |
| `max(a, b)` | `frep_node_create(FOP_MAX, a, b)` | 语义等价 |
| `eval.eval(tree, pt)` | `frep_eval_numeric(node, x, y)` | libfive 使用区间算术，Lv-00 使用 GMP 精确+double 近似 |
| `find_roots(tree)` | `solve_algebraic_system(graph, ...)` | libfive 用 Marching Cubes，Lv-00 用 Gröbner/SMT |
| `Renderer::render(tree)` | `proof_export_latex()` | libfive 渲染 3D 网格，Lv-00 导出 2D LaTeX 图形 |

---

## 附录 B：F-Rep 证明示例——三角形重心共线

```
命题: 三角形的重心 G 将每条中线分为 2:1 的比例
─────────────────────────────────────

F-Rep 表达:
  G = (A + B + C) / 3  （重心坐标）
  
  中线 med_A = segment(A, (B+C)/2)
    → f_med_A(P) = 共线性(A, M_BC, P)
    → 验证 f_med_A(G) = 0  （G 在中线上）

  比例条件: AG : G_M_BC = 2 : 1
    → |G - A| = 2 * |M_BC - G|
    → 验证平方距离比 = 4

Groebner 基验证:
  输入: 坐标变量 A.x, A.y, B.x, B.y, C.x, C.y
        + 共线性方程 f_med_A, f_med_B, f_med_C
        + 距离比方程
  输出: 理想是零维的 → 问题有唯一确定解 → 证毕
```

---

> **文档结束**  
> 本文档详述了 libfive 的函数表示法（F-Rep）如何应用于 Lv-00——用数学函数定义几何体、将几何证明转化为函数性质分析。核心结论：Lv-00 的 `SymbolicCoord` 已具备 F-Rep 所需的精确代数基础（GMP 有理数+代数数+多项式），通过引入 `FRepNode` 表达式树和 `ConstraintType → F-Rep` 翻译层，可以实现"几何构造 = 函数组合，几何证明 = 公共零点分析"的统一代数视角。

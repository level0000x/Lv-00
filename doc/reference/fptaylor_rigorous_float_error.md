# Lv-00 参考落地设计文档：FPTaylor 浮点舍入误差严格估计工具

> **版本**: 1.0.0
> **日期**: 2026-05-24
> **参考**: [FPTaylor](https://github.com/soarlab/FPTaylor) —— Utah 大学开发的浮点舍入误差严格估计工具
> **目标**: 借鉴 FPTaylor 的符号泰勒展开、区间算术与优化混合评估、FPCore 标准格式和 HOL Light 形式化证明输出，为 Lv-00 提供浮点运算可靠性分析、误差界自动推导和 TrustColor 可信计算系统的误差判定基准

---

## 目录

1. [项目概述](#1-项目概述)
2. [核心借鉴要点](#2-核心借鉴要点)
3. [Lv-00 映射方案](#3-lv-00-映射方案)
4. [实现路线图](#4-实现路线图)
5. [附录](#5-附录)

---

## 1. 项目概述

### 1.1 FPTaylor 是什么

FPTaylor 是由 Utah 大学 Solovyev 等人开发的浮点舍入误差严格估计工具，专门用于自动推导浮点程序的最坏情况舍入误差上界。它基于**符号泰勒展开**（Symbolic Taylor Forms）作为核心分析框架，使用 OCaml 实现，并且可以将推导出的误差界输出为 HOL Light 形式化证明。FPTaylor 是 FPBench 基准测试套件的重要分析后端，为浮点数值程序的可靠性提供了数学上严格的误差分析。

FPTaylor 的核心分析管线如下：

```
浮点表达式 (FPCore 格式)
    │
    ├─► 符号泰勒展开
    │   └─► 将表达式展开为带误差项的一阶泰勒多项式
    │
    ├─► 区间算术评估
    │   └─► 使用区间算术计算泰勒形式中的各阶导数范围
    │
    ├─► 全局优化 (Gelpia/Z3)
    │   └─► 分支定界算法精确求解误差界的最大值
    │
    └─► HOL Light 证明输出
        └─► 将推导结果转换为可机器检查的形式化证明
            │
            ▼
      误差界 + 形式化证明
```

关键数值特征：

| 指标 | 说明 |
|------|------|
| 分析精度 | 严格上界（数学证明级别） |
| 典型分析时间 | 秒级到分钟级（取决于表达式复杂度） |
| 支持的运算 | +, -, *, /, sqrt, sin, cos, exp, log 等 |
| 误差模型 | IEEE 754 舍入误差 + 输入不确定度传播 |
| 证明后端 | HOL Light（形式化验证） |
| 输入格式 | FPCore（FPBench 标准格式） |

### 1.2 FPTaylor 核心机制详解

#### 1.2.1 符号泰勒形式 (Symbolic Taylor Forms)

符号泰勒形式是 FPTaylor 的理论基石。对于浮点表达式 `f(x1, x2, ..., xn)`，它展开为：

```
f(x) ≈ f(a) + Σ ∂f/∂xi · (xi - ai) + R(x, a)
```

其中：
- `a` 是展开点（通常是最可能取值）
- `R(x, a)` 是二阶及以上的余项（Lagrange 余项）
- 每个 `(xi - ai)` 的区间已知（由输入不确定度定义）

FPTaylor 独特之处在于，它将**浮点舍入误差也纳入泰勒展开**：

```
float_f(x) = f(x) + δ_round(x)
             ≈ f(a) + Σ ∂f/∂xi · (xi - ai) + R(x, a) + δ_round(x)
```

然后在区间算术框架中对每一项求上界，最终得到：

```
|float_f(x) - f(x)| ≤ ε_total
```

这种将舍入误差建模为高阶小量并利用泰勒展开传播的方式，使得 FPTaylor 能够处理复杂的复合浮点表达式（嵌套运算多达数十层），且误差界通常远优于简单的区间算术拼接。

#### 1.2.2 区间算术 + 全局优化混合评估

FPTaylor 采用了两阶段评估策略：

**阶段 1：朴素区间算术**
- 对泰勒形式中的每一阶导数项直接用区间算术求范围
- 快速得到一个"安全但可能过松"的上界
- 计算复杂度 O(表达式规模)

**阶段 2：全局优化精化**
- 使用 Gelpia（全局优化器）或 Z3 SMT 求解器对泰勒形式进行分支定界优化
- 将每个变量的区间递归二分，检查优化目标（最大化余项）在子区间上的理论最大值
- 剪枝那些"不可能包含最优解"的分支
- 最终得到紧致的误差上界

这种混合策略在保证严格性（永远不低估误差）的同时，尽可能接近真实最坏情况误差（不严重高估误差），即追求"严格且紧致"的误差界。

#### 1.2.3 FPCore 标准格式

FPTaylor 接受 FPBench 项目定义的 FPCore 标准格式输入：

```scheme
;; 三角形面积计算的浮点误差分析
(FPCore (a b c)
 :name "Triangle area via Heron's formula"
 :pre (and (> a 0) (> b 0) (> c 0)
           (< a (+ b c)) (< b (+ a c)) (< c (+ a b)))
 :heron
 (let* ((s (/ (+ a b c) 2.0)))
   (sqrt (* s (- s a) (- s b) (- s c)))))
```

FPCore 格式的关键要素：
- **变量声明**：`(a b c)` 声明输入变量
- **前置条件**：`:pre` 约束输入的取值范围
- **表达式体**：标准 S-表达式形式的浮点计算
- **元数据**：`:name`, `:precision` 等标注

这种格式使得 FPTaylor 能够与 FPBench 生态中的其他工具（如 Herbie、Rosa、Daisy）互通数据，形成浮点分析的完整工具链。

#### 1.2.4 HOL Light 形式化验证

FPTaylor 可以将推导出的误差界输出为 HOL Light 定理：

```ocaml
(* HOL Light 输出的误差界定理 *)
let triangle_area_error = prove
  (`!a b c.
      &0 < a /\ &0 < b /\ &0 < c /\
      a < b + c /\ b < a + c /\ c < a + b
      ==> abs(float_heron a b c - real_heron a b c)
          <= &1.23e-15 * real_heron a b c`,
   ...);
```

这意味着 FPTaylor 的误差分析不仅是数值上的，还是逻辑上可验证的——HOL Light 证明检查器可以独立检查这个定理的正确性，无需信任 FPTaylor 本身的实现。

### 1.3 为什么借鉴 FPTaylor

Lv-00 是一个几何约束求解系统，其中大量计算涉及浮点运算：坐标求解（线性方程组）、距离计算（平方根）、角度计算（三角函数）、符号判断（共线性、平行性测试）。这些浮点运算不可避免地引入舍入误差，可能导致：

1. **约束求解不稳定**：精度损失导致"应该相交"的线"不相交"
2. **符号判断翻转**：本应共线的三点被判定为不共线
3. **几何退化**：严格成立的几何定理因浮点误差被拒绝

借鉴 FPTaylor 能够为 Lv-00 提供：
- 关键计算路径的误差上界自动推导
- TrustColor 可信计算系统中 AMBER（警告）降级触发条件的量化依据
- 求解器数值容差的科学设定（而非手动调参）

---

## 2. 核心借鉴要点

### 2.1 借鉴点一：符号泰勒形式

FPTaylor 的符号泰勒形式将浮点表达式展开为"精确值 + 一阶敏感度 + 高阶余项 + 舍入噪声"的结构，是进行严格误差分析的数学框架。

在 Lv-00 中，许多几何计算可以表示为这种形式：

**例 1：两点距离**

```
d = sqrt((x2-x1)^2 + (y2-y1)^2)

符号泰勒展开（在 (x1, y1, x2, y2) = (a1, b1, a2, b2) 处）：
d ≈ d₀ + (∂d/∂x1)·(x1-a1) + (∂d/∂y1)·(y1-b1)
       + (∂d/∂x2)·(x2-a2) + (∂d/∂y2)·(y2-b2)
       + R₂ + δ_round

其中 ∂d/∂x1 = -(x2-x1)/d₀, 等等
```

由于距离公式涉及平方和开根，其一阶导数在两点不重合时有界，泰勒余项也有界——这正是 FPTaylor 擅长的分析场景。

**例 2：直线交点**

```
给定 L1: a1·x + b1·y = c1, L2: a2·x + b2·y = c2

x = (c1·b2 - c2·b1) / (a1·b2 - a2·b1)
y = (a1·c2 - a2·c1) / (a1·b2 - a2·b1)
```

当 `(a1·b2 - a2·b1)` 接近零时（近乎平行），条件数极大，浮点误差被剧烈放大。FPTaylor 的分析能够量化：给定线参数的最大不确定度 δ，交点坐标的误差上界是多少。

#### Lv-00 中的符号泰勒形式应用场景

| 几何计算 | 表达式结构 | 关键挑战 | FPTaylor 分析价值 |
|---------|-----------|---------|------------------|
| 两点距离 | `sqrt(ΣΔ²)` | 距离趋于零时分母消失 | 推导距离阈值：何时误差超过 min_dist |
| 直线交点 | 有理分式（2×2 行列式） | 近乎平行时条件数爆炸 | 量化"接近平行"的定义 |
| 圆圆交点 | `sqrt(判别式)` | 相切时判别式≈0 | 推导相切判断的数值容差 |
| 三角形面积 | `0.5·|叉积|` | 面积很小时相对误差大 | 设定退化三角形的面积阈值 |
| 角度计算 | `acos(点积/(|a|·|b|))` | 接近 0° 或 180° 时敏感 | 量化"零角度"的浮点等价条件 |
| 点在线上判断 | `|a·x + b·y - c| < ε` | ε 选择缺乏理论依据 | 推导 ε 的科学取值 |

### 2.2 借鉴点二：区间算术 + 全局优化混合

FPTaylor 采用"先区间粗估，后优化精化"的两阶段策略。Lv-00 的误差评估复用这一思路：

**阶段 1（快速通道）：区间算术快速判定**
- 对输入变量的已知范围（如坐标来自鼠标点击，不确定度 ±0.5 像素），直接用区间算术评估误差
- 如果区间上界已经小于目标精度阈值（如 1e-10），则直接通过——说明此计算足够可靠

**阶段 2（精确通道）：分支定界优化**
- 如果阶段 1 得到的误差区间跨过了阈值，说明可能存在最坏情况
- 启动分支定界优化（模拟 FPTaylor + Gelpia），在输入区间上搜索误差最大值
- 返回紧致的误差上界

在 Lv-00 中，这一混合策略可以嵌入到 `TrustColor` 系统的判定逻辑中：

```
输入: 几何计算表达式 f, 输入区间 I, 精度阈值 ε_trust
输出: TrustColor (GREEN / AMBER / RED)

步骤:
  1. interval_result = interval_eval(f, I)
  2. if interval_result.upper < ε_trust:
       return GREEN   // 即使最坏情况下也安全
  3. if interval_result.lower > ε_trust:
       return RED     // 即使最好情况下也超限
  4. opt_result = global_optimize_max_error(f, I)
  5. if opt_result.max_error < ε_trust:
       return AMBER   // 区间方法过估，经优化确认安全
  6. return RED
```

### 2.3 借鉴点三：FPCore 标准格式

FPCore 格式为浮点计算提供了一种独立于实现语言的描述方式。Lv-00 可以借鉴这一思路，定义 `.lvfp` 格式来描述几何计算中的关键浮点表达式：

```scheme
;; .lvfp 格式示例：三角形内切圆半径
(LvFPCore (ax ay bx by cx cy)
  :name "Incircle radius of triangle ABC"
  :domain ((ax real) (ay real) (bx real) (by real) (cx real) (cy real))
  :pre (and (not (collinear? (ax ay) (bx by) (cx cy)))
            (> (area ax ay bx by cx cy) 0))
  :formula
  (/ (* 2 (area ax ay bx by cx cy))
     (+ (dist ax ay bx by) (dist bx by cx cy) (dist cx cy ax ay))))
```

.lvfp 格式的要素：
- **变量声明**：几何点的坐标作为输入变量
- **前置条件**：几何合法性约束（非退化、非共线等）
- **表达式体**：使用 Lv-00 内置函数（area、dist、collinear?等）描述计算
- **元数据**：名称、期望精度、关联的几何定理引用

.lvfp 文件可以由 Lv-00 的误差分析模块（`fptaylor_evaluate`）自动解析并进行严格的误差界推导。

### 2.4 借鉴点四：HOL Light 形式化证明输出

FPTaylor 能够将误差界推导结果转化为 HOL Light 形式化证明，这意味着误差分析的结果不是"我们觉得误差是多少"，而是"数学上可以证明误差是多少"。

在 Lv-00 的 TrustColor 系统中，最重要的安全关键计算（如核约束求解步骤、几何退化判定）可以输出形式化证明：

```ocaml
(* Lv-00 HOL Light 证明示例：
   对于任意不共线三点 A(xa,ya), B(xb,yb), C(xc,yc)
   若坐标值的不确定度不超过 1e-8
   则三角形面积浮点计算值的相对误差不超过 1e-12 *)
let triangle_area_stability = prove
  (`!xa ya xb yb xc yc.
      ~collinear (xa,ya) (xb,yb) (xc,yc) /\
      abs(xa - xa0) < &1e-8 /\ ... (* 输入不确定度 *)
      ==> abs(float_area xa ya xb yb xc yc - real_area xa ya xb yb xc yc)
          <= &1e-12 * abs(real_area xa ya xb yb xc yc)`,
   REWRITE_TAC[area_formula] THEN
   FPTAYLOR_TAC);;
```

虽然并非所有 Lv-00 的计算都需要达到 HOL Light 证明的严格程度，但关键路径（特别是影响 GREEN/RED 判定的计算）应具备形式化证明输出的能力。这是 FPTaylor 对 Lv-00 最深远的借鉴——将误差分析从"工程估计"提升为"数学证明"。

### 2.5 借鉴对照总表

| FPTaylor 概念 | FPTaylor 实现 | Lv-00 对应 | 借鉴价值（1-5 星） |
|--------------|-------------|-----------|-------------------|
| 符号泰勒形式 | 一阶泰勒展开 + Lagrange 余项 + 舍入误差项 | 几何计算的误差传播模型（如距离、交点、面积） | ★★★★★ |
| 区间算术 | 对泰勒形式各项用区间算术求安全上界 | 快速误差预判（GREEN/RED 快速通道） | ★★★★★ |
| Gelpia 全局优化 | 分支定界精化误差界 | 精确误差通道（AMBER 判定用） | ★★★★☆ |
| Z3 分支定界 | SMT 驱动的区间收缩 | 复杂约束条件（几何前提）下的误差优化 | ★★★★☆ |
| FPCore 格式 | 独立于语言的浮点表达式描述 | .lvfp 格式：几何关键计算的声明式描述 | ★★★★★ |
| HOL Light 证明输出 | 误差界形式化定理 | TrustColor 关键路径的形式化保障 | ★★★★★ |
| 舍入误差模型 | IEEE 754 逐运算舍入 | 几何运算（sqrt/sin/cos/acos）的舍入模型 | ★★★★★ |
| 输入不确定度传播 | 变量区间作为分析输入 | 鼠标坐标/参数输入的容差传播 | ★★★★★ |
| 条件数分析 | 一阶导数敏感度 | 识别"危险"几何构型（如近乎平行的交线） | ★★★★☆ |
| FPBench 集成 | 标准基准测试套件 | 几何误差分析的标准化测试用例 | ★★★☆☆ |

---

## 3. Lv-00 映射方案

### 3.1 核心函数：fptaylor_evaluate()

将 FPTaylor 风格的符号泰勒展开 + 区间评估流程映射为 Lv-00 的误差分析函数：

```
fptaylor_evaluate(expr, input_intervals) → ErrorBound
    步骤 1: 符号泰勒展开 (symbolic_taylor_expand)
        1.1 对表达式进行符号求导（计算所有一阶偏导）
        1.2 展开为一阶泰勒多项式 + Lagrange 余项
        1.3 为每个浮点运算插入舍入误差项 δ_i
    
    步骤 2: 区间算术评估 (interval_eval)
        2.1 计算一阶偏导在各个输入区间上的范围
        2.2 计算 Lagrange 余项的上界
        2.3 计算舍入误差累积的上界
        2.4 给出安全的（但可能偏松的）误差上界
    
    步骤 3: 全局优化精化 (global_optimize_refine)
        3.1 如果区间上界已足够紧致，直接返回
        3.2 否则启动分支定界优化
        3.3 递归二分输入区间，搜索真实最大误差
        3.4 返回紧致的误差上界
```

#### C 伪代码实现

```c
// =============================================
// fptaylor_eval.h — FPTaylor 风格的误差分析
// =============================================

#include "expr.h"
#include "interval.h"

/* ================================================================
 * 数据类型定义
 * ================================================================ */

// 区间类型
typedef struct {
    double lo, hi;
} Interval;

// 泰勒形式：f(x) ≈ f(a) + Σ g_i · (x_i - a_i) + R + Σ δ_i
typedef struct {
    Expr *base_value;           // f(a): 在展开点 a 的值
    Expr **first_derivs;        // g_i = ∂f/∂x_i 的符号表达式
    int var_count;
    Interval *expansion_point;  // 展开点 a
    Expr *lagrange_remainder;   // Lagrange 余项 R
    Expr **rounding_errors;     // 每个运算的舍入误差 δ_i
    int rounding_error_count;
} TaylorForm;

// 误差分析结果
typedef struct {
    Interval error_bound;       // 总误差上界 [lo, hi]
    double relative_error;      // 相对误差上界
    double condition_number;    // 条件数（最大敏感度）
    bool is_tight;              // 是否已通过优化精化
    double optimization_time_ms; // 优化耗时
} ErrorBound;

// 分析模式
typedef enum {
    FPTAYLOR_FAST,          // 仅区间算术（快速但可能偏松）
    FPTAYLOR_TIGHT,         // 区间 + 全局优化（紧致但较慢）
    FPTAYLOR_PROOF          // 紧致 + HOL Light 证明输出
} FPTaylorMode;

// 浮点运算精度上下文
typedef enum {
    FP32,    // 单精度 (epsilon ≈ 6e-8)
    FP64,    // 双精度 (epsilon ≈ 1.1e-16)
    FP80,    // 扩展精度 (epsilon ≈ 5.4e-20)
    FP128    // 四精度 (epsilon ≈ 9.6e-35)
} FPPrecision;

// 分析配置
typedef struct {
    FPTaylorMode mode;
    FPPrecision precision;
    int max_bisection_depth;       // 分支定界最大深度
    double target_tightness;       // 目标精度
    bool output_hol_light_proof;   // 是否输出 HOL Light 证明
    const char *hol_output_path;   // HOL Light 证明输出路径
} FPTaylorConfig;

/* ================================================================
 * 区间算术基础运算
 * ================================================================ */

// 区间加法: [a,b] + [c,d] = [a+c, b+d]
Interval interval_add(Interval a, Interval b) {
    return (Interval){ .lo = a.lo + b.lo, .hi = a.hi + b.hi };
}

// 区间减法: [a,b] - [c,d] = [a-d, b-c]
Interval interval_sub(Interval a, Interval b) {
    return (Interval){ .lo = a.lo - b.hi, .hi = a.hi - b.lo };
}

// 区间乘法: [a,b] × [c,d]
Interval interval_mul(Interval a, Interval b) {
    double p1 = a.lo * b.lo;
    double p2 = a.lo * b.hi;
    double p3 = a.hi * b.lo;
    double p4 = a.hi * b.hi;
    return (Interval){
        .lo = fmin(fmin(p1, p2), fmin(p3, p4)),
        .hi = fmax(fmax(p1, p2), fmax(p3, p4))
    };
}

// 区间除法: [a,b] / [c,d] (要求 0 不在 [c,d] 中)
Interval interval_div(Interval a, Interval b) {
    // 假设 b.lo > 0 (正除数)
    return (Interval){
        .lo = a.lo / b.hi,
        .hi = a.hi / b.lo
    };
}

// 区间平方根: sqrt([a,b]) (要求 a >= 0)
Interval interval_sqrt(Interval a) {
    return (Interval){
        .lo = sqrt(fmax(0.0, a.lo)),
        .hi = sqrt(a.hi)
    };
}

// 区间绝对值
Interval interval_abs(Interval a) {
    if (a.lo >= 0) return a;
    if (a.hi <= 0) return (Interval){ .lo = -a.hi, .hi = -a.lo };
    return (Interval){ .lo = 0, .hi = fmax(-a.lo, a.hi) };
}

/* ================================================================
 * 阶段 1: 符号泰勒展开
 * ================================================================ */

// 一阶泰勒展开：f(x) ≈ f(a) + Σ (∂f/∂x_i)(a) · (x_i - a_i)
// 同时为每个浮点运算插入舍入误差项
void symbolic_taylor_expand(
    Expr *expr,
    const char **var_names,
    int var_count,
    Interval *input_domains,
    FPTaylorConfig *cfg,
    TaylorForm *out_taylor
) {
    out_taylor->var_count = var_count;

    // 1. 选取展开点：通常取每个区间的中点
    out_taylor->expansion_point = malloc(var_count * sizeof(Interval));
    for (int i = 0; i < var_count; i++) {
        double mid = (input_domains[i].lo + input_domains[i].hi) / 2.0;
        out_taylor->expansion_point[i] = (Interval){ mid, mid };
    }

    // 2. 计算 f(a) — 在展开点的精确值（理论上使用实数算术）
    out_taylor->base_value = expr_eval_at_point(
        expr, var_names,
        out_taylor->expansion_point, var_count);

    // 3. 符号求导：对每个变量求一阶偏导
    out_taylor->first_derivs = malloc(var_count * sizeof(Expr*));
    for (int i = 0; i < var_count; i++) {
        Expr *deriv = expr_symbolic_derivative(expr, var_names[i]);
        out_taylor->first_derivs[i] =
            expr_eval_at_point(deriv, var_names,
                              out_taylor->expansion_point, var_count);
        expr_free(deriv);
    }

    // 4. 构造 Lagrange 余项
    out_taylor->lagrange_remainder = build_lagrange_remainder(
        expr, var_names, var_count, input_domains);

    // 5. 为每个浮点运算插入舍入误差项
    out_taylor->rounding_error_count = count_float_ops(expr);
    out_taylor->rounding_errors =
        malloc(out_taylor->rounding_error_count * sizeof(Expr*));

    double eps = get_machine_epsilon(cfg->precision);
    for (int i = 0; i < out_taylor->rounding_error_count; i++) {
        // 第 i 个浮点运算的舍入误差：|δ_i| ≤ eps · |中间结果|
        // 初始约束为 [-eps·M, eps·M] 其中 M 来自区间估计
        out_taylor->rounding_errors[i] =
            expr_const_create(eps);  // 将随后乘中间结果范围
    }
}

/* ================================================================
 * 阶段 2: 区间算术评估
 * ================================================================ */

Interval interval_eval_taylor_form(
    TaylorForm *taylor,
    Interval *input_domains,
    int var_count
) {
    Interval total_error = { 0.0, 0.0 };

    // 项 1: Σ |∂f/∂x_i| · width(x_i - a_i)
    for (int i = 0; i < var_count; i++) {
        double sensitivity = fabs(expr_to_double(taylor->first_derivs[i]));
        double var_width = input_domains[i].hi - input_domains[i].lo;
        total_error.hi += sensitivity * var_width;
    }

    // 项 2: Lagrange 余项上界
    Interval R_bound = interval_eval_expr(
        taylor->lagrange_remainder, input_domains);
    Interval R_abs = interval_abs(R_bound);
    total_error = interval_add(total_error, R_abs);

    // 项 3: 舍入误差累积上界
    for (int i = 0; i < taylor->rounding_error_count; i++) {
        double delta_bound = fabs(expr_to_double(taylor->rounding_errors[i]));
        total_error.hi += delta_bound;
    }

    // 对称化：误差区间通常关于零对称（正负误差概率相当）
    total_error.lo = -total_error.hi;
    return total_error;
}

/* ================================================================
 * 阶段 3: 全局优化精化（分支定界）
 * ================================================================ */

typedef struct {
    Interval *subdomain;    // 当前子区域
    int depth;
    Interval error_bound;   // 此子区域上的误差上界估计
} BisectionNode;

// 全局优化：在输入区域上搜索误差的最大值
ErrorBound global_optimize_refine(
    TaylorForm *taylor,
    Interval *input_domains,
    int var_count,
    FPTaylorConfig *cfg
) {
    // 初始化优先队列（按误差上界降序排列）
    PriorityQueue *pq = pq_create(
        cfg->max_bisection_depth * var_count);

    BisectionNode root;
    root.subdomain = malloc(var_count * sizeof(Interval));
    memcpy(root.subdomain, input_domains,
           var_count * sizeof(Interval));
    root.depth = 0;
    root.error_bound = interval_eval_taylor_form(
        taylor, root.subdomain, var_count);
    pq_push(pq, &root);

    double best_upper = root.error_bound.hi;
    double best_lower = root.error_bound.lo;

    // 分支定界主循环
    while (!pq_is_empty(pq)) {
        BisectionNode node = pq_pop(pq);

        // 剪枝：此节点误差上界不可能改善最佳上界
        if (node.error_bound.hi < best_upper * 0.99) {
            free(node.subdomain);
            continue;
        }

        // 达到最大深度或足够精度，接受此节点结果为上界
        if (node.depth >= cfg->max_bisection_depth ||
            (node.error_bound.hi - node.error_bound.lo)
             < cfg->target_tightness * best_upper) {
            // 更新紧致上界
            best_upper = fmax(best_upper, node.error_bound.hi);
            best_lower = fmax(best_lower, node.error_bound.lo);
            free(node.subdomain);
            continue;
        }

        // 二分：选取最宽维度
        int split_dim = select_widest_dimension(
            node.subdomain, var_count);

        double mid = (node.subdomain[split_dim].lo
                    + node.subdomain[split_dim].hi) / 2.0;

        // 左子区间
        BisectionNode left;
        left.subdomain = malloc(var_count * sizeof(Interval));
        memcpy(left.subdomain, node.subdomain,
               var_count * sizeof(Interval));
        left.subdomain[split_dim].hi = mid;
        left.depth = node.depth + 1;
        left.error_bound = interval_eval_taylor_form(
            taylor, left.subdomain, var_count);
        if (left.error_bound.hi > best_upper * 0.9)
            pq_push(pq, &left);
        else
            free(left.subdomain);

        // 右子区间
        BisectionNode right;
        right.subdomain = malloc(var_count * sizeof(Interval));
        memcpy(right.subdomain, node.subdomain,
               var_count * sizeof(Interval));
        right.subdomain[split_dim].lo = mid;
        right.depth = node.depth + 1;
        right.error_bound = interval_eval_taylor_form(
            taylor, right.subdomain, var_count);
        if (right.error_bound.hi > best_upper * 0.9)
            pq_push(pq, &right);
        else
            free(right.subdomain);

        free(node.subdomain);
    }

    pq_destroy(pq);

    ErrorBound result;
    result.error_bound = (Interval){ best_lower, best_upper };
    result.is_tight = true;
    return result;
}

/* ================================================================
 * 主函数：FPTaylor 风格误差分析
 * ================================================================ */

ErrorBound fptaylor_evaluate(
    Expr *expr,
    const char **var_names,
    Interval *input_intervals,
    int var_count,
    FPTaylorConfig *cfg
) {
    // 阶段 1：符号泰勒展开
    TaylorForm taylor;
    symbolic_taylor_expand(expr, var_names, var_count,
                           input_intervals, cfg, &taylor);

    // 阶段 2：区间算术评估（快速通道）
    Interval fast_bound = interval_eval_taylor_form(
        &taylor, input_intervals, var_count);

    // 计算条件数 = max(|∂f/∂x_i|) / |f(a)|
    double max_sensitivity = 0.0;
    double f_at_a = fabs(expr_to_double(taylor.base_value));
    for (int i = 0; i < var_count; i++) {
        double sens = fabs(expr_to_double(taylor.first_derivs[i]));
        max_sensitivity = fmax(max_sensitivity, sens);
    }
    double condition_number = f_at_a > 1e-15
        ? max_sensitivity / f_at_a : INFINITY;

    // 阶段 3（可选）：全局优化精化
    ErrorBound result = { 0 };
    result.condition_number = condition_number;

    if (cfg->mode == FPTAYLOR_FAST) {
        result.error_bound = fast_bound;
        result.is_tight = false;
        result.relative_error = f_at_a > 1e-15
            ? fast_bound.hi / f_at_a : INFINITY;
    } else {
        // TIGHT 或 PROOF 模式：运行全局优化
        ErrorBound refined = global_optimize_refine(
            &taylor, input_intervals, var_count, cfg);
        result = refined;
        result.condition_number = condition_number;
        result.relative_error = f_at_a > 1e-15
            ? refined.error_bound.hi / f_at_a : INFINITY;
    }

    // 阶段 4（可选）：输出 HOL Light 证明
    if (cfg->mode == FPTAYLOR_PROOF && cfg->output_hol_light_proof) {
        output_hol_light_proof(expr, var_names, var_count,
                               input_intervals, &result,
                               cfg->hol_output_path);
    }

    // 释放泰勒形式
    free_taylor_form(&taylor);

    return result;
}
```

### 3.2 TrustColor 系统集成

FPTaylor 风格的误差分析可以直接集成到 Lv-00 的 TrustColor 可信计算系统中：

```
TrustColor 判定逻辑
═══════════════════════════════════════════════════════════════

输入: 几何计算 f, 输入区间 I, 配置 config
输出: TrustColor

cfg_fast = { .mode = FPTAYLOR_FAST, .precision = FP64 };
fast_bound = fptaylor_evaluate(f, vars, I, n, &cfg_fast);

if fast_bound.error_bound.hi < GREEN_THRESHOLD:
    ┌─────────────────────────────────────┐
    │  TrustColor: GREEN                   │
    │  理由: 即使最坏情况误差也满足精度要求    │
    └─────────────────────────────────────┘

elif fast_bound.error_bound.lo > RED_THRESHOLD:
    ┌─────────────────────────────────────┐
    │  TrustColor: RED                     │
    │  理由: 即使最好情况也超出允许误差       │
    └─────────────────────────────────────┘

else:
    cfg_tight = { .mode = FPTAYLOR_TIGHT, .precision = FP64 };
    tight_bound = fptaylor_evaluate(f, vars, I, n, &cfg_tight);

    if tight_bound.error_bound.hi < AMBER_THRESHOLD:
        ┌─────────────────────────────────────┐
        │  TrustColor: AMBER                   │
        │  理由: 区间方法过估，经优化确认安全     │
        └─────────────────────────────────────┘
    else:
        ┌─────────────────────────────────────┐
        │  TrustColor: RED                     │
        │  理由: 经优化后仍超出允许误差          │
        └─────────────────────────────────────┘
```

### 3.3 .lvfp 格式定义

受 FPCore 启发，为 Lv-00 定义 `.lvfp`（Lv-00 Floating-Point）格式：

```
;; .lvfp 文件结构
;; 每个文件可包含多个浮点计算声明的误差分析请求

(LvFPCore (xa ya xb yb)
  :name "Distance between points A and B"
  :category geometry/distance
  :domain ((xa real [-1e6, 1e6]) (ya real [-1e6, 1e6])
           (xb real [-1e6, 1e6]) (yb real [-1e6, 1e6]))
  :pre (> (dist xa ya xb yb) 1e-10)  ;; 排除重合点
  :precision fp64
  :target-relative-error 1e-12
  :formula
  (sqrt (+ (sq (- xb xa)) (sq (- yb ya)))))

(LvFPCore (a1 b1 c1 a2 b2 c2)
  :name "Line intersection point"
  :category geometry/intersection
  :domain ((a1 real [-10, 10]) ... (c2 real [-1000, 1000]))
  :pre (> (abs (- (* a1 b2) (* a2 b1))) 1e-8)  ;; 不平行
  :precision fp64
  :target-absolute-error 1e-10
  :formula
  (let ((det (- (* a1 b2) (* a2 b1))))
    (tuple (/ (- (* c1 b2) (* c2 b1)) det)
           (/ (- (* a1 c2) (* a2 c1)) det))))
```

解析 `.lvfp` 格式并自动调用 `fptaylor_evaluate`：

```c
// 解析 .lvfp 文件并批量误差分析
typedef struct {
    char *name;
    char *category;
    char **var_names;
    int var_count;
    Interval *input_domains;
    Expr *precondition;
    Expr *formula;
    FPPrecision precision;
    double target_error;
    ErrorBound analysis_result;
} LvfpEntry;

// 批量分析 .lvfp 文件中的所有条目
int fptaylor_analyze_lvfp_file(
    const char *lvfp_path,
    FPTaylorConfig *cfg,
    LvfpEntry **out_entries
);
```

---

## 4. 实现路线图

### 4.1 分阶段实施计划

| 阶段 | 名称 | 目标 | 输入 | 输出 | 估计工期 | 依赖 |
|------|------|------|------|------|---------|------|
| **阶段 1** | 一阶泰勒形式计算器 | 实现符号求导引擎、一阶泰勒展开、Lagrange 余项构造 | 表达式 AST | TaylorForm 数据结构 | 2-3 周 | expr.h (已有表达式树) |
| **阶段 2** | 区间算术引擎 | 实现区间四则运算 + sqrt/sin/cos 的区间版本 | 区间输入 + 表达式 | 误差上界区间 | 1-2 周 | 阶段 1 |
| **阶段 3** | 分支定界优化器 | 实现输入域自适应二分 + 优先队列驱动的全局优化 | 泰勒形式 + 输入域 | 紧致误差界 | 2 周 | 阶段 1-2 |
| **阶段 4** | TrustColor 集成 | 将误差分析嵌入 TrustColor 判定流程 | 误差界 | TrustColor (GREEN/AMBER/RED) | 1 周 | 阶段 2-3 |
| **阶段 5** | .lvfp 格式与工具链 | 定义 .lvfp 格式、解析器、批量分析 CLI | .lvfp 文件 | 误差分析报告 | 1-2 周 | 阶段 1-4 |
| **阶段 6** | HOL Light 证明输出 | 误差界 → HOL Light 定理生成，嵌入 Lv-00 证明模块 | 紧致误差界 | .hol 证明文件 | 2 周 | 阶段 3-5 |

### 4.2 阶段 1 详细任务：一阶泰勒形式计算器

**目标文件**：`src/analysis/taylor_form.h` 和 `src/analysis/taylor_form.c`

**任务清单**：

1. 扩展表达式系统：为 `Expr` 增加偏导计算能力（`expr_symbolic_derivative`）
2. 实现展开点选取策略：区间中点、顶点采样、随机采样三种策略
3. 实现 `symbolic_taylor_expand()` — 主展开函数，输出 `TaylorForm`
4. 实现 `build_lagrange_remainder()` — Lagrange 余项构造
   - 对二元函数 f(x,y)：R = (1/2)·[f_xx·(x-a)^2 + 2f_xy·(x-a)(y-b) + f_yy·(y-b)^2]
   - 对更高维推广到 Hessian 矩阵形式
5. 实现浮点运算计数和舍入误差项插入
6. 单元测试：对 `f(x) = x^2 + x` 展开，验证在 x=0 处的一阶近似误差小于理论界

**验证标准**：
- 对多项式 f(x) = ax^2 + bx + c，泰勒展开的线性部分精确匹配 df/dx
- Lagrange 余项覆盖真实二次误差

### 4.3 阶段 2 详细任务：区间算术引擎

**目标文件**：`src/analysis/interval_arith.h` 和 `src/analysis/interval_arith.c`

**任务清单**：

1. 定义 `Interval` 类型 + 规范化的 NaN/Inf 处理
2. 实现基础四则运算：`interval_add/sub/mul/div`
3. 实现超越函数：`interval_sqrt/sin/cos/exp/log`
4. 实现 `interval_eval_expr()` — 在区间上评估任意表达式树
5. 实现 `interval_eval_taylor_form()` — 将泰勒形式在区间上求总误差上界
6. 实现区间收缩（contract）：利用单调性收紧区间范围
7. 单元测试：对经典测试用例（Rump 函数、Muller 递归、Siegfried 多项式）验证区间包含真实值

**验证标准**：
- 所有区间运算保证包含性（true value ∈ interval result）
- 对已知精确值的测试用例，区间宽度 < 1e-10

### 4.4 阶段 3 详细任务：分支定界优化器

**目标文件**：`src/analysis/branch_bound.h` 和 `src/analysis/branch_bound.c`

**任务清单**：

1. 定义 `BisectionNode` 结构和优先队列接口
2. 实现 `select_widest_dimension()` — 维度选择启发式
3. 实现分支定界主循环：二分-评估-入队-剪枝
4. 实现剪枝策略：
   - 绝对剪枝：`node.error_bound.hi < best_global_lower`
   - 相对剪枝：`node.error_bound.hi < 0.9 * best_global_upper`
   - 收敛剪枝：区间宽度 < target_tightness
5. 实现 `global_optimize_refine()` — 完整优化流程
6. 基准测试：对 FPBench 标准测试用例的优化效果

**验证标准**：
- 优化后的误差界比区间方法的误差界紧致 2-10 倍
- 优化耗时 < 1 秒（10 个变量、中等复杂度表达式）

### 4.5 阶段 4 详细任务：TrustColor 集成

**目标文件**：修改 `src/core/trust_color.h` 增加 FPTaylor 分支

**任务清单**：

1. 在 TrustColor 枚举和判定逻辑中增加 FPTaylor 分析分支
2. 实现 `trust_color_with_fptaylor()` — 融合几何语义和数值误差的判定
3. 定义 GREEN_THRESHOLD、AMBER_THRESHOLD、RED_THRESHOLD 的默认值
4. 实现缓存：同一表达式+同一输入区间的误差分析结果缓存
5. 测试：对典型几何计算（交点、距离、面积）的 TrustColor 输出

**验证标准**：
- 对于确定性构造（如已知两点求中点），TrustColor 输出 GREEN
- 对于病态构造（如近乎平行线的交点），TrustColor 输出至少 AMBER

### 4.6 阶段 5 详细任务：.lvfp 格式与工具链

**目标文件**：`src/analysis/lvfp_parser.h` 和 `src/analysis/lvfp_parser.c`

**任务清单**：

1. 定义 .lvfp 格式的完整语法规范（类 S-表达式语法）
2. 实现 .lvfp 解析器（词法分析 + 递归下降解析）
3. 实现 `fptaylor_analyze_lvfp_file()` — 批量分析
4. 实现 CLI 工具：`lv-00 fptaylor analyze file.lvfp`
5. 生成分析报告（文本和 JSON 两种格式）

**验证标准**：
- 正确解析 FPBench 的已知 FPCore 测试文件（格式兼容层）
- 分析报告包含条件数、绝对误差、相对误差、TrustColor 建议

### 4.7 里程碑与交付物

| 里程碑 | 时间节点 | 交付物 | 验收标准 |
|--------|---------|--------|---------|
| M1: 泰勒展开可用 | 第 3 周末 | taylor_form.h/c | 对多项式表达式展开精确 |
| M2: 区间评估通过 | 第 5 周末 | interval_arith.h/c | 标准测试用例包容性验证通过 |
| M3: 紧致误差界 | 第 7 周末 | branch_bound.h/c | FPBench 基准测试误差界提升 2x+ |
| M4: TrustColor 集成就绪 | 第 8 周末 | trust_color.h 更新 | 所有几何计算可判定 TrustColor |
| M5: 完整工具链 | 第 10 周末 | lvfp_parser.h/c, CLI | .lvfp 文件批量分析通过 |
| M6: 形式化证明 | 第 12 周末 | hol_proof.c, 关键路径定理 | 10 个核心几何计算的形式化误差证明 |

---

## 5. 附录

### A. FPTaylor 关键资源

- **GitHub 仓库**：[https://github.com/soarlab/FPTaylor](https://github.com/soarlab/FPTaylor)
- **论文**：Solovyev, A. et al. "Rigorous estimation of floating-point round-off errors with symbolic Taylor expansions." *ACM TOPLAS*, 2019.
- **FPBench**：[https://fpbench.org/](https://fpbench.org/) — 浮点基准测试标准
- **Gelpia**：[https://github.com/soarlab/gelpia](https://github.com/soarlab/gelpia) — 全局优化器

### B. 符号泰勒展开理论基础

- Makino, K. and Berz, M. "Taylor models and other validated functional inclusion methods." *International Journal of Pure and Applied Mathematics*, 2003.
- Neumaier, A. "Taylor forms—use and limits." *Reliable Computing*, 2003.
- Higham, N.J. "Accuracy and Stability of Numerical Algorithms." SIAM, 2002. (第 1-4 章：浮点算术和误差分析基础)

### C. 区间算术参考

- Moore, R.E., Kearfott, R.B., and Cloud, M.J. "Introduction to Interval Analysis." SIAM, 2009.
- IEEE 1788-2015: Standard for Interval Arithmetic.

### D. HOL Light 形式化参考

- Harrison, J. "HOL Light: An Overview." *TPHOLs*, 2009.
- HOL Light GitHub：[https://github.com/jrh13/hol-light](https://github.com/jrh13/hol-light)
- Boldo, S. and Melquiond, G. "Computer Arithmetic and Formal Proofs." ISTE Press - Elsevier, 2017. (浮点算术形式化验证的权威参考)

### E. Lv-00 相关文件索引

| Lv-00 文件 | 角色 | 与 FPTaylor 借鉴的关系 |
|-----------|------|----------------------|
| `src/analysis/taylor_form.h` | 符号泰勒形式计算 | FPTaylor 的核心分析框架迁移 |
| `src/analysis/interval_arith.h` | 区间算术引擎 | 误差传播的基础计算设施 |
| `src/analysis/branch_bound.h` | 分支定界全局优化器 | Gelpia/Z3 优化策略的 C 语言实现 |
| `src/analysis/lvfp_parser.h` | .lvfp 格式解析器 | FPCore 格式的几何领域适配 |
| `src/analysis/fptaylor_eval.h` | 误差分析主入口 | `fptaylor_evaluate()` 函数定义 |
| `src/core/trust_color.h` | TrustColor 可信计算系统 | 误差界→GREEN/AMBER/RED 判定 |
| `src/proof/hol_proof.c` | HOL Light 证明生成器 | 误差界的形式化证明输出 |
| `src/expr/expr.h` | 表达式系统 | 被分析的目标表达式载体 |
| `tools/lv00-fptaylor.c` | CLI 分析工具 | .lvfp 文件批量分析的命令行入口 |

### F. 术语对照

| 英文术语 | 中文翻译 | 说明 |
|---------|---------|------|
| Symbolic Taylor Form | 符号泰勒形式 | 带误差项的一阶泰勒多项式 |
| Lagrange Remainder | Lagrange 余项 | 二阶及以上高阶导数的累积项 |
| Interval Arithmetic | 区间算术 | 将数值替换为区间 [lo, hi] 的运算 |
| Round-off Error | 舍入误差 | IEEE 754 浮点表示与实数之间的差异 |
| Condition Number | 条件数 | 输入微小扰动对输出的放大倍数 |
| Branch and Bound | 分支定界 | 递归二分搜索空间寻找全局最优的优化算法 |
| Machine Epsilon | 机器精度 | 1.0 与下一个可表示浮点数之差 |
| FPCore | FPBench 核心格式 | FPBench 基准测试标准格式 |
| HOL Light | HOL Light 定理证明器 | 基于高阶逻辑的形式化证明助手 |
| TrustColor | 可信计算颜色标记 | Lv-00 中 GREEN/AMBER/RED 三级可信度标记 |
| Gelpia | Gelpia 全局优化器 | FPTaylor 使用的分支定界优化引擎 |
| Inclusion Property | 包容性 | 区间结果必然包含真实值（不出界） |


---

> **文档状态**: 初稿完成
> **下一步**: 基于此参考文档启动阶段 1（一阶泰勒形式计算器）的详细设计

# Lv-00 参考落地设计文档：AbstractAlgebra.jl 泛型代数结构

> **版本**: 1.0.0
> **日期**: 2026-05-24
> **参考**: AbstractAlgebra.jl (github.com/Nemocas/AbstractAlgebra.jl) —— 纯 Julia 泛型抽象代数库
> **目标**: 借鉴 AbstractAlgebra.jl 的泛型环/域/群设计——"声明结构→自动获得全运算"的零配置代数模式，映射到 Lv-00 的公理包系统（ring_theory.lvz / group_theory.lvz）与 `type_system.h` 代数类型层级

---

## 目录

1. [AbstractAlgebra.jl 项目概述与 Lv-00 借鉴动机](#1-abstractalgebrajl-项目概述与-lv-00-借鉴动机)
2. [核心借鉴要点一：声明结构→自动获得全运算](#2-核心借鉴要点一声明结构自动获得全运算)
3. [核心借鉴要点二：泛型系数类型——同一代码跨多种代数结构](#3-核心借鉴要点二泛型系数类型同一代码跨多种代数结构)
4. [核心借鉴要点三：环/域/群的层级继承链](#4-核心借鉴要点三环域群的层级继承链)
5. [Lv-00 映射方案：公理包系统设计](#5-lv-00-映射方案公理包系统设计)
6. [type_system.h 代数类型层级映射](#6-type_systemh-代数类型层级映射)
7. [实现路线图](#7-实现路线图)
8. [关键映射表](#8-关键映射表)

---

## 1. AbstractAlgebra.jl 项目概述与 Lv-00 借鉴动机

### 1.1 AbstractAlgebra.jl 是什么

AbstractAlgebra.jl 是用纯 Julia 实现的泛型抽象代数库。它允许用户通过一行声明即可获得完整的数学结构及其所有运算：

```julia
# AbstractAlgebra.jl 核心体验
using AbstractAlgebra

# 一行声明一个泛型多项式环
R, (x, y) = polynomial_ring(QQ, [:x, :y])
# 自动获得: +, -, *, ^, div, gcd, factor, resultant, ...

f = x^2 + 2*x*y + y^2    # 多项式运算自动可用
g = (x + y)^3
h = gcd(f, g)            # gcd 算法自动适用

# 同样的代码，换一个系数环即可
S, t = polynomial_ring(GF(7), :t)
f2 = t^3 + 2*t + 1       # 在有限域 GF(7) 上运算
```

其核心理念是：**声明了代数结构，就自动获得了该结构的所有标准运算**。不需要手动实现 `+`、`gcd`、`factor`、`resultant` 等——只要结构被声明为 `Ring` 或 `Field`，这些运算就通过 Julia 的多重分派自动可用。

### 1.2 Lv-00 借鉴动机

Lv-00 的公理包系统（`axiom_packages/` 目录）当前设计是用 `.lvz` 文件声明公理和规则，但缺乏一个"声明结构→自动获得全运算"的高层抽象。AbstractAlgebra.jl 的"一行声明全环"模式为 Lv-00 的公理包加载提供了直接参考：

| 借鉴方向 | AbstractAlgebra.jl 特性 | Lv-00 现有基础 | 差距 |
|:---|:---|:---|:---|
| **声明即获得** | `polynomial_ring(QQ, :x)` → 全运算自动可用 | `axiom_packages/` 手动加载 | 缺"加载即全运算"的集中声明机制 |
| **泛型系数类型** | 同一代码跨 QQ/GF(7)/ZZ/CC | `TypeRegion` + 范畴检查 | 缺乏"系数替换→全库自动重编译"能力 |
| **代数继承链** | Ring → EuclideanRing → Field → ... | 几何范畴层级（Euclidean/Projective/...） | 缺少代数范畴的独立定义 |
| **运算同构** | `+` 在任何 Ring 中语义相同 | `FuncBlock` 基于名称匹配 | 缺"范畴操作符"的规范化语义 |

### 1.3 核心概念对照

```
AbstractAlgebra.jl                       Lv-00
────────────────────────────────────────────────────────
AbstractAlgebra.Ring                  →  ring_theory.lvz (公理包)
AbstractAlgebra.Field                 →  field_theory.lvz (公理包)
AbstractAlgebra.Group                 →  group_theory.lvz (公理包)
polynomial_ring(R, vars)              →  axiom_package_load("ring_theory")
Generic.PolyRing{T}                   →  TypeRegion (代数类型区域)
Generic.Poly{T}                       →  具体代数类型节点
parent_type / elem_type               →  TypeRegion + GeomType
coefficient_ring(f)                   →  类型推导：获取系数范畴
change_base_ring(S, f)               →  类型参数替换（泛型特化）
```

---

## 2. 核心借鉴要点一：声明结构→自动获得全运算

### 2.1 AbstractAlgebra.jl 的零配置哲学

AbstractAlgebra.jl 的一个核心设计是：当用户声明了一个特定类型的环（如 `polynomial_ring(QQ, [:x, :y])`），系统会自动为该环注册所有可用的运算：

```
polynomial_ring(QQ, [:x, :y]) 声明后自动获得:
─────────────────────────────────────────────────────
代数操作:
  +, -, *, ^, div, rem, divexact            (环运算)
  gcd, lcm, gcdx                             (欧几里得环运算)
  factor, is_irreducible                     (因式分解)
  resultant, discriminant                     (结式和判别式)
  derivative, integral                        (微积分)
  evaluate                                    (求值)
  change_base_ring                            (换基环)
  canonical_unit, is_unit                     (单位元)
  mul_classical, mul_karatsuba                (乘法算法选择)

这一切都是自动的——用户不需要手动实现任何一个函数。
系统通过 Julia 的类型系统发现:
  polynomial_ring(QQ, [:x, :y]) → 类型 = Generic.PolyRing{QQ}
  该类型是 EuclideanRing 的子类型
  → 所有 EuclideanRing 的函数自动适用
  → 所有 PolyRing 特有的函数（如 resultant）也可用
```

### 2.2 Lv-00 的等价设计：公理包自动加载

借鉴 AbstractAlgebra.jl 的零配置哲学，Lv-00 的公理包加载应实现：

```c
/**
 * @brief Lv-00 公理包自动加载 —— 借鉴 AbstractAlgebra.jl 的声明即获得
 *
 * 当用户通过 .lvz DSL 声明一个几何构造的范畴时:
 *   @use ring_theory
 *   @use euclidean_geometry
 *
 * 系统自动:
 *   1. 加载该范畴的所有公理和重写规则
 *   2. 注册该范畴的所有操作的 FuncBlock
 *   3. 将该范畴的操作与类型层级绑定
 *
 * AbstractAlgebra.jl 等价:
 *   polynomial_ring(QQ, :x) → 自动注册 gcd/factor/resultant 等
 * Lv-00 等价:
 *   axiom_package_load("ring_theory") → 自动注册 add/mul/gcd/factor 等 FuncBlock
 */
typedef struct AxiomPackage {
    char *package_name;                 /* 公理包名称（如 "ring_theory"） */
    char *package_path;                 /* .lvz 文件路径 */

    /* 该公理包声明的范畴 */
    int *category_ids;                  /* 范畴 ID 列表 */
    int category_count;

    /* 自动注册的 FuncBlock（声明即获得的操作） */
    struct {
        char *operation_name;           /* 操作名（如 "add", "mul", "gcd"） */
        int func_block_id;              /* 对应的 FuncBlock ID */
        int min_category_id;            /* 该操作的最小范畴要求 */
    } *auto_operations;
    int auto_op_count;

    /* 自动注册的重写规则 */
    int *rewrite_rule_ids;
    int rewrite_count;

    /* 公理列表 */
    int *axiom_ids;
    int axiom_count;
} AxiomPackage;

/**
 * @brief 加载公理包并自动注册所有运算 —— 借鉴 AbstractAlgebra.jl 的零配置
 *
 * 加载流程:
 *   1. 解析 .lvz 文件 → 提取 @category 声明
 *   2. 根据 Category 的 required_operations 自动注册对应的 FuncBlock
 *   3. 根据 Category 的 parent_category 链，向上递归注册父范畴的操作
 *   4. 加载重写规则和公理约束
 *   5. 更新 TypeRegion 的 category 成员关系
 *
 * 返回: 0 成功，-1 失败（.lvz 解析错误或范畴不一致）
 */
int axiom_package_load(const char *package_path, TypeSystem *ts);
```

### 2.3 公理包示例：ring_theory.lvz

```
// ============================================================
// Lv-00 公理包: ring_theory.lvz
// 借鉴 AbstractAlgebra.jl 的 Ring 抽象
// 声明即获得: +, -, *, 0, 1, gcd, factor, ...
// ============================================================

@package ring_theory

// --- 范畴声明 (借鉴 AbstractAlgebra.jl 的继承链) ---

@category Semigroup {
    @op add: (%, %) -> %               // 二元结合运算
    @axiom associative: (a + b) + c = a + (b + c)
}

@category Monoid extends Semigroup {
    @op zero: -> %                      // 单位元 (0)
    @axiom identity_add: a + 0 = a ∧ 0 + a = a
}

@category Group extends Monoid {
    @op neg: % -> %                     // 加法逆元
    @axiom inverse_add: a + neg(a) = 0 ∧ neg(a) + a = 0
}

@category AbelianGroup extends Group {
    @axiom commutative_add: a + b = b + a
}

@category Ring extends AbelianGroup {
    @op mul: (%, %) -> %                // 乘法（结合，有单位元 1）
    @op one: -> %
    @axiom distributive: a * (b + c) = a*b + a*c
    @axiom multiplicative_identity: a * 1 = a ∧ 1 * a = a
}

@category CommutativeRing extends Ring {
    @axiom commutative_mul: a * b = b * a
}

@category IntegralDomain extends CommutativeRing {
    @axiom no_zero_divisors: a ≠ 0 ∧ b ≠ 0 → a*b ≠ 0
}

@category EuclideanRing extends IntegralDomain {
    @op div: (%, %) -> %
    @op mod: (%, %) -> %
    @op gcd: (%, %) -> %
    @rule euclidean_division: a = q*b + r ∧ (r = 0 ∨ degree(r) < degree(b))
    @rule gcd_euclidean: gcd(a, b) = gcd(b, a mod b)
}

@category Field extends EuclideanRing {
    @op inv: % -> %                     // 乘法逆元（除 0 外）
    @axiom inverse_mul: a ≠ 0 → a * inv(a) = 1
}

// --- 声明后自动可用的 FuncBlock (无需手动注册) ---
// Semigroup: add
// Monoid: zero
// Group: neg
// AbelianGroup: (继承 Group 的一切)
// Ring: mul, one
// CommutativeRing: (继承 Ring 的一切)
// IntegralDomain: (继承 CommutativeRing 的一切)
// EuclideanRing: div, mod, gcd
// Field: inv
```

---

## 3. 核心借鉴要点二：泛型系数类型——同一代码跨多种代数结构

### 3.1 AbstractAlgebra.jl 的泛型设计

AbstractAlgebra.jl 的一个关键优势是同一段算法代码可以跨多种不同的代数结构工作：

```julia
# 同一个多项式 GCD 算法，自动适用于多种环
function generic_polynomial_gcd(f::PolyRingElem, g::PolyRingElem)
    while !iszero(g)
        f, g = g, rem(f, g)
    end
    return f
end

# 使用示例：跨四种不同的系数类型
R1, x = polynomial_ring(QQ, :x)        # QQ = 有理数
f1 = (x-1)^2 * (x-2); g1 = (x-1) * (x-3)
gcd(f1, g1)  # → x-1

R2, x = polynomial_ring(GF(7), :x)     # GF(7) = 有限域
f2 = (x+1)^2; g2 = (x+1)*(x+3)
gcd(f2, g2)  # → x+1 (在 GF(7) 中计算)

R3, x = polynomial_ring(ZZ, :x)        # ZZ = 整数
f3 = (x-1)^2; g3 = (x-1)*(x+2)
gcd(f3, g3)  # → x-1

R4, x = polynomial_ring(ComplexF64, :x) # 复数域
f4 = (x-1im)^2; g4 = (x-1im)*(x-2)
gcd(f4, g4)  # → x - 1im
```

### 3.2 Lv-00 的几何泛型——同一构造跨多种几何空间

借鉴 AbstractAlgebra.jl 的泛型系数类型思想，Lv-00 的几何构造也可以在"切换几何空间"后自动重编译：

```c
/**
 * @brief 几何空间参数化构造 —— 借鉴 AbstractAlgebra.jl 的泛型系数类型
 *
 * 同一个几何构造 (如 "三角形的内心")，在 Euclidean/Riemann/Projective
 * 空间中各有对应的定义和公理。切换空间类型应自动切换底层运算。
 *
 * AbstractAlgebra.jl 等价:
 *   f = x^2 + 2*x + 1  （同一个多项式）
 *   gcd(f, g, over=QQ)   → 在有理数上计算
 *   gcd(f, g, over=GF(7)) → 在有限域上计算
 *
 * Lv-00 等价:
 *   triangle ABC = triangle(A, B, C);  （同一个三角形构造）
 *   在 EuclideanSpace 中 → 内角和 = 180°, 用 Euclidean 距离
 *   在 ProjectiveSpace 中 → 内角和无定义, 用 cross_ratio
 *   在 HyperbolicSpace 中 → 内角和 < 180°, 用 hyperbolic_distance
 */
typedef enum {
    GEOM_SPACE_EUCLIDEAN,
    GEOM_SPACE_PROJECTIVE,
    GEOM_SPACE_HYPERBOLIC,
    GEOM_SPACE_SPHERICAL,
    GEOM_SPACE_AFFINE,
    GEOM_SPACE_GENERIC           /* 泛型几何空间（不指定具体公理） */
} GeomSpaceType;

/**
 * @brief 在指定的几何空间中实例化构造
 *
 * 同一个构造图，替换底层几何空间后，
 * 所有 FuncBlock 自动切换到对应空间的操作实现。
 *
 * 类似于 AbstractAlgebra.jl 的 change_base_ring():
 *   f 在 QQ 上定义，change_base_ring(GF(7), f) 切换到 GF(7)
 */
ConstraintGraph *geom_space_specialize(
    const ConstraintGraph *generic_graph,
    GeomSpaceType target_space,
    TypeSystem *ts);
```

### 3.3 几何空间的运算替换表

| 操作 | Euclidean 实现 | Projective 实现 | Hyperbolic 实现 |
|:---|:---|:---|:---|
| `distance(A, B)` | `sqrt((dx)^2 + (dy)^2)` | `cross_ratio(A,B,C,D)` | `arcosh(1 + 2*(dx^2+dy^2)/(...))` |
| `angle(A, O, B)` | `arccos(dot/|OA||OB|)` | 未定义 | `angle_of_parallelism` |
| `parallel(l1, l2)` | 存在唯一平行线 | 无平行线（所有线相交） | 无穷多条平行线 |
| `triangle_sum(ABC)` | `= 180°` | 未定义 | `< 180°` |
| `circumcenter(A,B,C)` | 三点共圆的外心 | 对偶极点 | 双曲外心 |
| `midpoint(A, B)` | `(A+B)/2` | `harmonic_conjugate` | 双曲中点 |

---

## 4. 核心借鉴要点三：环/域/群的层级继承链

### 4.1 AbstractAlgebra.jl 的类型层级

AbstractAlgebra.jl 构建了 Julia 原生类型的层次继承，使得每个子类型自动继承父类型的所有运算和性质：

```
AbstractAlgebra.jl 代数层级（简化）:
─────────────────────────────────────
Set
  └─ SetElement
      └─ RingElement              ← +, -, *, 0
          ├─ FieldElement          ← +, -, *, /, 0, 1
          │   └─ Generic.FracFieldElem
          ├─ PolyRingElem          ← 多项式特有操作
          │   └─ Generic.Poly{T}   ← 泛型多项式（T = 系数类型）
          ├─ MPolyRingElem          ← 多元多项式特有操作
          │   └─ Generic.MPoly{T}
          ├─ MatSpaceElem           ← 矩阵特有操作
          │   └─ Generic.MatSpace{T}
          └─ SeriesRingElem          ← 级数特有操作

每个层级定义了:
  - 该层级的操作集合
  - 从父层级继承的操作
  - 该层级的公理要求
```

### 4.2 Lv-00 的代数类型继承链

借鉴 AbstractAlgebra.jl 的层级设计，为 Lv-00 的 `type_system.h` 设计对应的代数类型继承链：

```c
/**
 * @brief Lv-00 代数类型继承层级 —— 借鉴 AbstractAlgebra.jl 的类型层级
 *
 * 每个代数类型级别定义了一组必需的运算和公理。
 * 子类型继承父类型的所有运算并可以添加新运算。
 *
 * AbstractAlgebra.jl 等价:
 *   Ring <: Group <: Monoid <: Semigroup <: Set
 */
typedef enum {
    ALG_TYPE_SET,               /* 集合：有 = 运算 */
    ALG_TYPE_SEMIGROUP,         /* 半群：有 + (结合) */
    ALG_TYPE_MONOID,            /* 幺半群：有 0 (单位元) */
    ALG_TYPE_GROUP,             /* 群：有 - (逆元) */
    ALG_TYPE_ABELIAN_GROUP,     /* 交换群：+ 可交换 */
    ALG_TYPE_RING,              /* 环：有 +, -, *, 0, 1 */
    ALG_TYPE_COMMUTATIVE_RING,  /* 交换环：* 可交换 */
    ALG_TYPE_INTEGRAL_DOMAIN,   /* 整环：无零因子 */
    ALG_TYPE_EUCLIDEAN_RING,    /* 欧几里得环：有 div/mod/gcd */
    ALG_TYPE_FIELD,             /* 域：有 / (逆元) */
    ALG_TYPE_FINITE_FIELD,      /* 有限域: GF(p^n) */
    ALG_TYPE_POLYNOMIAL_RING,   /* 多项式环: R[x] */
    ALG_TYPE_MATRIX_RING        /* 矩阵环: M_n(R) */
} AlgebraicTypeLevel;

typedef struct AlgebraicTypeNode {
    AlgebraicTypeLevel level;
    int id;
    char *name;                         /* 如 "QQ", "GF(7)[x]", "ZZ" */

    /* 父类型（继承链） */
    int *parent_type_ids;
    int parent_count;

    /* 该类型定义的操作 FuncBlock ID 列表 */
    int *operation_ids;
    int op_count;

    /* 该类型定义的公理 */
    int *axiom_ids;
    int axiom_count;

    /* 泛型参数（对于多项式环：参数 = 系数类型 ID） */
    int *generic_param_type_ids;
    int generic_param_count;
} AlgebraicTypeNode;
```

---

## 5. Lv-00 映射方案：公理包系统设计

### 5.1 公理包的加载流程

```
公理包加载流程 (借鉴 AbstractAlgebra.jl 的零配置):

1. 用户 DSL 声明:
     @use ring_theory
     @use euclidean_geometry
        │
2. 解析 .lvz 公理包文件            →  axiom_package_load()
        │
3. 提取 @category 声明               →  CategoryDefinition 数组
        │
4. 构建范畴继承图                     →  category_is_subcategory() 填充
        │
5. 为每个范畴注册 auto_operations     →  FuncBlock 注册表
     (借鉴 AbstractAlgebra.jl: Ring → 自动获得 +,-,*)
        │
6. 加载 @rule / @axiom               →  RewriteRule 数组
        │
7. 绑定到 TypeSystem                  →  TypeRegion 的 category_id 字段
        │
8. 类型检查：验证所有 @domain 声明的一致性
        │
9. 公理包加载完成
     所有范畴的全部运算自动可用
```

### 5.2 公理包间的依赖关系

```
Lv-00 公理包依赖图 (借鉴 AbstractAlgebra.jl 的继承链):

ring_theory.lvz ────────────────────────────────┐
  ├─ @category Semigroup                        │
  ├─ @category Monoid (→ Semigroup)             │
  ├─ @category Group (→ Monoid)                  │
  ├─ @category AbelianGroup (→ Group)           │
  ├─ @category Ring (→ AbelianGroup)             │
  ├─ @category CommutativeRing (→ Ring)          │
  ├─ @category IntegralDomain (→ CommRing)       │
  ├─ @category EuclideanRing (→ IntegralDomain)  │
  └─ @category Field (→ EuclideanRing)           │
                                                 │
field_theory.lvz ─────────────────────────────────┤
  └─ extends ring_theory                         │
     └─ @category FiniteField (→ Field)          │
                                                 │
group_theory.lvz ─────────────────────────────────┤
  └─ extends ring_theory                         │
     ├─ @category PermutationGroup               │
     └─ @category MatrixGroup                     │
                                                 ▼
                                            type_system.h
                                      (代数类型层级注册表)
```

---

## 6. type_system.h 代数类型层级映射

### 6.1 现有 type_system.h 与代数类型的对接

| `type_system.h` 现有结构 | 代数类型扩展 | 说明 |
|:---|:---|:---|
| `TypeRegion`（几何类型区域） | `AlgebraicTypeNode`（代数类型节点） | 两者并列：几何类型与代数类型是两个独立的类型层级 |
| `TypeKind` 枚举 | 新增 `TYPE_KIND_ALGEBRAIC` | 用于区分几何类型和代数类型 |
| `type_check_port_compatibility()` | 扩展为"代数类型兼容 + 几何类型兼容" | 代数端口与几何端口使用不同规则 |
| `CategoryDefinition` | 代数范畴（Ring/Field/Group）vs 几何范畴（Euclidean/Projective） | 范畴系统统一，但实例不同 |
| `FuncBlock` 操作注册 | `@op` 声明自动注册 | 公理包的 `@op` 指令自动创建 FuncBlock |

### 6.2 代数类型与几何类型的桥接

代数类型（环/域/群）和几何类型（点/线/圆）的交互发生在约束方程的系数中：

```
几何构造       → 约束方程                  → 系数类型
─────────────────────────────────────────────────────────────
point A(0,0)  → 无                           N/A
line l(A,B)   → y = mx + b               系数 m, b ∈ QQ
circle c(O,r) → (x-x0)^2+(y-y0)^2 = r^2  系数 ∈ QQ
intersection  → 联立多项式方程           系数 ∈ QQ[x] (多项式环)
angle(A,O,B)  → cos(θ) = dot/(|OA||OB|)  值 ∈ ℝ (可能需要 Algebraic)

Lv-00 的类型桥接:
  几何类型的坐标 → 代入符号变量 → 构成多项式
  多项式的系数 → 从 SymbolicCoord 类型推导 → 自动映射到代数类型层级
```

### 6.3 桥接实现

```c
/**
 * @brief 从 SymbolicCoord 推导对应的代数类型 —— 类型桥接
 *
 * 当几何约束被转化为多项式方程时，需要确定方程的系数域。
 * 此函数从 SymbolicCoord 的类型推导最适合的代数环。
 *
 * AbstractAlgebra.jl 等价:
 *   coefficient_ring(R, f) → 返回 f 的系数环
 *
 * Lv-00 等价:
 *   symbol_coord_to_algebraic_type(RATIONAL) → QQ (有理数域)
 *   symbol_coord_to_algebraic_type(ALGEBRAIC) → QQ[x] (多项式环)
 *   symbol_coord_to_algebraic_type(TRANSCENDENTAL) → ℝ (实数域)
 */
int symbol_coord_to_algebraic_type(const SymbolicCoord *coord, TypeSystem *ts);

/**
 * @brief 获取两个代数类型的最小公共上界（Least Upper Bound）
 *
 * 类似于 AbstractAlgebra.jl 的 自动类型提升：
 *   如果 a ∈ QQ, b ∈ QQ[x]，则 QQ <: QQ[x]，提升 b 到 QQ[x]
 *
 * 在 Lv-00 约束求解中：
 *   如果约束方程的一个系数来自 QQ，另一个来自代数扩域，
 *   需要将两者都提升到最小公共上界后再求解。
 */
int algebraic_type_lub(int type_a_id, int type_b_id, TypeSystem *ts);
```

---

## 7. 实现路线图

### 7.1 第一阶段：公理包系统核心（P3）

| 任务 | 文件 | 说明 |
|:---|:---|:---|
| `AxiomPackage` 结构体 | `include/lv00/axiom_package.h`（新文件） | 公理包数据结构 |
| `axiom_package_load()` 实现 | `src/axiom_package.c`（新文件） | .lvz 解析 + 自动注册 |
| `ring_theory.lvz` 编写 | `axiom_packages/ring_theory.lvz`（新文件） | Semigroup → Field 的完整层级 |
| `group_theory.lvz` 编写 | `axiom_packages/group_theory.lvz`（新文件） | 群理论的公理声明 |
| `@category` / `@op` / `@axiom` / `@rule` 解析器 | `src/lvz_parser.c` | .lvz 语法扩展 |

**预估规模**：约 250 行 C + 150 行 .lvz

### 7.2 第二阶段：代数类型层级（P3-P4）

| 任务 | 文件 | 说明 |
|:---|:---|:---|
| `AlgebraicTypeNode`、`AlgebraicTypeLevel` | `include/lv00/type_system.h` | 代数类型层级 |
| `symbol_coord_to_algebraic_type()` | `src/type_system.c` | SymbolicCoord → 代数类型推导 |
| `algebraic_type_lub()` | `src/type_system.c` | 最小公共上界计算 |
| 代数类型兼容性检查 | `src/type_system.c` | 扩展 `type_check_port_compatibility()` |

**预估规模**：约 180 行 C 代码

### 7.3 第三阶段：几何空间泛型（P4）

| 任务 | 说明 |
|:---|:---|
| `GeomSpaceType` 枚举 + `geom_space_specialize()` | 几何空间切换 |
| 各几何空间的操作替换表 | Euclidean/Projective/Hyperbolic 操作的 FuncBlock 映射 |
| 泛型构造的验证 | 同一构造在三种空间中各生成验证报告 |
| `euclidean_geometry.lvz` 公理包 | Euclidean 几何公理的完整声明 |

---

## 8. 关键映射表

### 8.1 AbstractAlgebra.jl → Lv-00 概念映射

| AbstractAlgebra.jl | Lv-00 映射 | Lv-00 文件 |
|:---|:---|:---|
| `AbstractAlgebra.Ring` | `@category Ring` (ring_theory.lvz) | `axiom_packages/ring_theory.lvz` |
| `AbstractAlgebra.Field` | `@category Field` (extends EuclideanRing) | `axiom_packages/ring_theory.lvz` |
| `AbstractAlgebra.Group` | `@category Group` (group_theory.lvz) | `axiom_packages/group_theory.lvz` |
| `polynomial_ring(R, vars)` | `axiom_package_load("ring_theory")` | `axiom_package.c` |
| `Generic.PolyRing{T}` | `AlgebraicTypeNode` (level = POLYNOMIAL_RING) | `type_system.h` |
| `Generic.Poly{T}` | 多项式表达式节点 | `expr_node.h` |
| `coefficient_ring(f)` | `symbol_coord_to_algebraic_type()` | `type_system.c` |
| `change_base_ring(S, f)` | `geom_space_specialize()` | `compiler.c`（新增） |
| `parent_type / elem_type` | `TypeRegion` + `AlgebraicTypeNode` | `type_system.h` |
| Julia 类型层级继承 | `CategoryDefinition` 的 `parent_category_ids[]` | `type_system.h` |
| Julia 多重分派 | `scheduler_select_backend()` + `DispatchRule` | `engine_scheduler.cpp` |
| `factor(f)` (因式分解) | `groebner_basis_compute()` (Groebner 基) | `solver_symbolic.c` |
| `gcd(f, g)` | `mpz_poly_gcd()` (GMP 多项式 GCD) | `symbolic_coord.h` |

### 8.2 公理包预置表

| 公理包文件 | 范畴定义 | 自动注册操作数 | 来源 |
|:---|:---|:---|:---|
| `ring_theory.lvz` | Semigroup, Monoid, Group, AbelianGroup, Ring, CommutativeRing, IntegralDomain, EuclideanRing, Field | 9 个范畴, ~25 个操作 | AbstractAlgebra.jl 参考 |
| `group_theory.lvz` | Group, PermutationGroup, MatrixGroup, LieGroup | 4 个范畴, ~10 个操作 | AbstractAlgebra.jl + GAP |
| `field_theory.lvz` | Field, FiniteField, NumberField | 3 个范畴, ~8 个操作 | AbstractAlgebra.jl |
| `euclidean_geometry.lvz` | EuclideanSpace, MetricSpace, AffineSpace | 3 个范畴, ~15 个操作 | Tarski / Hilbert 公理 |
| `projective_geometry.lvz` | ProjectiveSpace | 1 个范畴, ~8 个操作 | 投影几何公理 |

### 8.3 代数类型 ↔ 几何空间类型桥接表

| SymbolicCoord 类型 | 代数类型 | 适用的几何空间 |
|:---|:---|:---|
| `RATIONAL` (赋值的分数) | QQ (有理数域) | Euclidean, Projective, Affine |
| `RATIONAL` (未赋值符号) | QQ[x, y, ...] (多元多项式环) | 所有空间（符号构造） |
| `ALGEBRAIC` (代数扩域元素) | QQ(α) (单扩域) | Euclidean (特定) |
| `QUADRATIC` | QQ[√d] (二次数域) | Euclidean (距离含根号) |
| `TRANSCENDENTAL` (pi, sin) | ℝ (实数域) | Euclidean, Hyperbolic |
| (无) | GF(p) (有限域) | 组合几何 |

---

> **文档结束**
> 本文档详述了 AbstractAlgebra.jl 的泛型代数结构设计如何映射到 Lv-00 的公理包系统和 `type_system.h` 代数类型层级。核心结论：(1) 借鉴"声明结构→自动获得全运算"的零配置模式，通过 `axiom_package_load("ring_theory")` 单次调用自动注册 Semigroup→Field 的全部 9 层范畴约 25 个标准操作；(2) 借鉴泛型系数类型思想，实现几何空间泛型——同一几何构造在 Euclidean/Projective/Hyperbolic 空间中自动切换底层运算；(3) 建立 `AlgebraicTypeNode` 代数类型层级与几何 `CategoryDefinition` 的桥接机制，使得约束方程的系数类型推导和几何空间的范畴验证无缝集成。

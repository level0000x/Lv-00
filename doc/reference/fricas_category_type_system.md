# Lv-00 参考落地设计文档：FriCAS/Axiom 范畴论类型系统

> **版本**: 1.0.0
> **日期**: 2026-05-24
> **参考**: FriCAS (github.com/fricas/fricas) —— Axiom 的现代化分支，强类型 CAS
> **目标**: 借鉴 FriCAS 100+ 层数学结构继承链（范畴论驱动）和类型驱动的操作符分发，映射到 Lv-00 type_system.h

---

## 目录

1. [项目概述与 Lv-00 借鉴动机](#1-项目概述与-lv-00-借鉴动机)
2. [核心借鉴要点：范畴论驱动的类型层级](#2-核心借鉴要点范畴论驱动的类型层级)
3. [类型层级映射方案](#3-类型层级映射方案)
4. [类型驱动的操作符分发](#4-类型驱动的操作符分发)
5. [几何操作的符号/数值路径自动选择](#5-几何操作的符号数值路径自动选择)
6. [与 type_system.h 的集成设计](#6-与-type_systemh-的集成设计)
7. [实现路线图](#7-实现路线图)
8. [关键映射表](#8-关键映射表)

---

## 1. 项目概述与 Lv-00 借鉴动机

### 1.1 FriCAS / Axiom 是什么

FriCAS 是 Axiom（原名 Scratchpad II，IBM 1960年代研发）的活跃维护分支。Axiom 是第一个也是唯一一个以**范畴论（Category Theory）**为类型系统基础的计算机代数系统。其核心哲学是：**类型即数学结构**。

```
Axiom/FriCAS 类型层级（简化）:
  SetCategory           ← 最基本：对象有 = 运算
    └─ BasicType        ← 有 = 和 ~= 运算
      └─ Ring           ← 有 +, -, * 运算
        └─ CommutativeRing  ← Ring 且 * 可交换
          └─ IntegralDomain  ← 无零因子
            └─ Field         ← 有 / 运算
              └─ ... (100+ 层)

  每个类型知道自己属于哪些范畴，
  每个算法（函数）声明自己的"最小类型需求":
    integration: (F: Join(Field, ...), x: Symbol) → F
    // "integrate 需要 F 至少是 Field"
```

### 1.2 Lv-00 借鉴动机

Lv-00 的几何对象天生具有类型层级——点、线段、区域、函数块各属于不同的类型范畴，每个范畴规定了该类型的对象可执行哪些操作和满足哪些公理。FriCAS 的范畴论驱动类型系统与此高度一致：

| 借鉴方向 | FriCAS 特性 | Lv-00 现有基础 | 差距 |
|:---|:---|:---|:---|
| **类型层级** | 100+ 层数学结构继承链 | `TypeRegion` + `TypeKind` 枚举 | 缺细粒度的范畴层级 |
| **类型驱动分发** | 符号/数值路径自动选择 | `scheduler_select_backend()` 手动选择 | 缺"类型→后端"的自动路由表 |
| **范畴公理** | 每个范畴携带一组公理签名 | `axiom_packages/` 公理包 | 缺"范畴=公理集合"的显式建模 |
| **操作符重载** | `+` 对 Ring 是加法，对 Group 是群运算 | `FuncBlock` 名称匹配 | 缺基于类型签章的重载解析 |

### 1.3 核心概念对照

```
FriCAS / Axiom                          Lv-00
─────────────────────────────────────────────────────────
Category (Ring, Field, Group)    →    TypeRegion + Constraint 集合
Domain (Integer, Polynomial)     →    具体几何类型节点
Package (算法集合)               →    FuncBlock 注册表
Coercion (类型强制)              →    type_check_port_compatibility()
Retractable (缩小类型)            →    type_infer_node()
Conditional Category             →    ceq (条件等式 / 条件范畴成员)
```

---

## 2. 核心借鉴要点：范畴论驱动的类型层级

### 2.1 Axiom 的类型层级哲学

Axiom 的类型系统有四个核心概念：

| 概念 | 定义 | 示例 |
|:---|:---|:---|
| **Category（范畴）** | 一组操作签名 + 公理的集合 | `Ring`: 需要 `+`, `-`, `*` 三个操作，满足结合律、分配律等 |
| **Domain（域）** | 实现某个 Category 的具体类型 | `Integer` 实现了 `Ring` Category |
| **Package（包）** | 在一个或多个 Category 约束下定义算法 | 高斯消元法：`(R: Field, M: Matrix R) → ...` |
| **Coercion（强制）** | 类型之间的自动/半自动转换 | `Integer` → `Float`（子类型到超类型） |

### 2.2 Category 的"契约式"设计

Axiom 的 Category 类似一种 **类型契约**——类型声明自己属于某个 Category，就必须提供 Category 要求的所有操作实现，并且满足 Category 声明的所有公理。编译期即可检查。

```
Axiom 示例: SemiGroup Category 的定义
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
SemiGroup(): Category == SetCategory with
    "*": (%, %) -> %            -- 操作签名: 二元闭合运算
    associative                   -- 公理: 结合律

Monoid(): Category == SemiGroup with
    1: %                          -- 单位元
    -- 公理: 1 * x = x * 1 = x  (继承自 SemiGroup)

Group(): Category == Monoid with
    inv: % -> %                   -- 逆运算
    -- 公理: x * inv(x) = inv(x) * x = 1
```

在 Lv-00 中对等的设计：每个几何类型属于一个 TypeRegion，该 TypeRegion 包含一组约束（公理）和一个 FuncBlock 注册表（操作）。

### 2.3 100+ 层继承链的启示

Axiom 的类型层级从 `SetCategory` 开始，经过 `BasicType`、`SemiGroup`、`Monoid`、`Group`、`Ring`、`CommutativeRing`、`IntegralDomain`、`Field`... 直到具体的 `Float`、`Complex Float`、`Polynomial Integer`。虽然 Lv-00 不需要这么深，但这种**分层精化**的设计对几何类型系统有直接参考：

```
Axiom 类型深链                     Lv-00 几何类型精化链
─────────────────────────────────────────────────────────
SetCategory (有 =)              → GeomEntity (有合一)
  BasicType (有 =, ~=)         → (合一 + 归一化等价)
    Ring (+, -, *)              → LinearEntity (平移, 缩放)
      CommutativeRing           → (交换律线性变换)
        Field (/)               → MetricSpace (距离, 角度)
          ...                   → EuclideanSpace
                                  ProjectiveSpace
                                  HyperbolicSpace
```

---

## 3. 类型层级映射方案

### 3.1 几何范畴定义

借鉴 Axiom 的 Category 定义，为 Lv-00 设计几何对应的 Category 层级：

```
// Lv-00 范畴定义（借鉴 Axiom 风格）
// 每个 @category 声明一组必需的操作签名 + 公理约束

@category GeomEntity {
    // 操作签名
    @op has_same_type : (GeomEntity, GeomEntity) -> Bool
    // 公理（每个几何实体可与同类型实体做合一检查）
    @axiom unifiable: ∀x,y: GeomEntity. x ≡ y ∨ x ≢ y
}

@category LinearEntity extends GeomEntity {
    // 操作签名
    @op translate : (LinearEntity, Vector) -> LinearEntity
    @op scale : (LinearEntity, Number) -> LinearEntity
    // 公理（线性实体在仿射变换下保持共线性）
    @axiom affine_closed: ∀l: LinearEntity, T: AffineTransform.
        collinear(T(P1), T(P2), T(P3)) ← collinear(P1, P2, P3)
}

@category MetricSpace extends GeomEntity {
    // 操作签名
    @op distance : (MetricSpace, MetricSpace) -> Number
    @op angle : (MetricSpace, MetricSpace, MetricSpace) -> Number
    // 公理（距离满足三角不等式）
    @axiom triangle_ineq: ∀A,B,C: MetricSpace.
        distance(A, C) ≤ distance(A, B) + distance(B, C)
}

@category EuclideanSpace extends MetricSpace, LinearEntity {
    // 继承 MetricSpace 和 LinearEntity 的所有操作和公理
    // 额外操作
    @op perpendicular : (EuclideanSpace, EuclideanSpace) -> EuclideanSpace
    // 欧氏平行公理
    @axiom parallel_postulate: ∀l: Line, P: Point (P ∉ l).
        ∃! m: Line. P ∈ m ∧ m ∥ l
}

@category ProjectiveSpace extends GeomEntity {
    // 投影几何：没有距离和角度的概念
    // 额外操作
    @op cross_ratio : (ProjectiveSpace×4) -> Number
    // 投影公理
    @axiom desargues: ...
}
```

### 3.2 几何类型的 Category 成员声明

借鉴 Axiom 的 `Domain` 概念，具体的几何类型声明自己属于哪些 Category：

```c
/**
 * @brief 几何类型范畴成员表 —— 借鉴 Axiom 的 Domain-Category 关系
 *
 * 每个具体的几何类型（Point, Segment, Circle, Region, etc.）
 * 声明自己实现了哪些 Category 所要求的操作和公理。
 *
 * 该表由公理包加载时自动填充，并在类型检查时用于验证
 * "用户对某几何类型执行的操作是否在该类型的 Category 契约内"。
 */
typedef struct {
    GeomType geom_type;                     /* 几何类型（如 GEOM_POINT） */
    int *category_ids;                      /* 该类型所属的 Category ID 列表 */
    int category_count;
} GeomTypeCategoryMembership;

/**
 * @brief 范畴（Category）定义 —— 借鉴 Axiom 的 Category
 *
 * 每个 Category 定义一组操作签名（必需的 FuncBlock）
 * 和一组公理（必需的 RewriteRule/Constraint）。
 * 声明为某 Category 成员的类型必须满足所有这些要求。
 */
typedef struct {
    int id;                                 /* Category ID */
    char *name;                             /* Category 名称 */
    int *parent_category_ids;               /* 父 Category（多重继承） */
    int parent_count;

    /* 必需的操作签名 */
    char **required_operation_names;        /* 必需的操作名（如 "translate", "distance"） */
    int required_op_count;

    /* 公理约束 */
    int *axiom_constraint_ids;              /* 该 Category 定义的公理约束 ID */
    int axiom_count;
} CategoryDefinition;
```

### 3.3 类型层级检查

借鉴 Axiom 的 `has` 操作符（`if R has Field then ...`），Lv-00 需要等价的"类型是否属于某范畴"的查询函数：

```c
/**
 * @brief 检查几何节点是否属于某范畴 —— 借鉴 Axiom 的 has 操作符
 *
 * Axiom 示例: if R has Field then invert(R) else error
 * Lv-00 对应: if (type_node_has_category(ts, node_id, CAT_METRIC_SPACE))
 *                compute_distance();
 *             else
 *                error("距离只在度量空间中定义");
 *
 * @return true 如果该节点的几何类型实现了指定范畴
 */
bool type_node_has_category(const TypeSystem *ts, int node_id, int category_id);

/**
 * @brief 检查范畴A是否是范畴B的子范畴
 *
 * 实现类似于 Axiom 的 Category 继承图遍历。
 * 递归检查 category_a 的 parent_category_ids 是否包含 category_b。
 */
bool category_is_subcategory(const TypeSystem *ts, int category_a, int category_b);
```

---

## 4. 类型驱动的操作符分发

### 4.1 FriCAS 的操作符分发机制

FriCAS 的核心能力之一是**类型驱动的操作符分发**——同一个操作符号（如 `integrate`），根据参数类型自动选择不同的实现：

```
FriCAS:
  integrate(sin(x) + sqrt(2), x)     → 自动走符号积分（参数是符号表达式）
  integrate(sin(x) + 0.5, x)         → 自动走数值积分（参数含浮点数）
  integrate(sin(x) + 1/2, x)         → 自动走符号积分（有理数不算浮点）

判定逻辑:
  if 参数类型是 Expression(Integer)  → 符号路径
  if 参数类型是 Expression(Float)    → 数值路径
```

### 4.2 Lv-00 的等价设计

几何操作同样存在"符号vs数值"的路径选择问题：

```
Lv-00 几何操作分发:
  midpoint(A(0,0), B(6,0))          → 数值计算: M = (3, 0)
  midpoint(A(x_a,y_a), B(6,0))      → 符号表达式: M = ((x_a+6)/2, y_a/2)
  midpoint(A, B) 其中 A,B 未赋值     → 符号约束: M.x = (A.x+B.x)/2 等

判定逻辑:
  if A.x 和 B.x 都是 RATIONAL         → 数值路径（solver_numerical）
  if A.x 或 B.x 是 ALGEBRAIC          → 符号路径（solver_symbolic → Gröbner/SMT）
```

```c
/**
 * @brief 类型驱动的操作符分发 —— 借鉴 FriCAS 的自动分发
 *
 * 根据操作参数的符号/数值类型，自动选择执行路径。
 *
 * FriCAS 等价: integrate 根据参数类型选择符号/数值路径
 * Lv-00 对应: 几何构造根据 SymbolicCoord 类型选择求解路径
 *
 * 分发矩阵:
 *   ┌──────────┬──────────┬─────────────────────────┐
 *   │ 参数类型 │ 路径     │ 后端引擎                 │
 *   ├──────────┼──────────┼─────────────────────────┤
 *   │ RATIONAL │ 数值     │ solver_numerical()       │
 *   │ ALGEBRAIC│ 符号低度 │ groebner_basis_compute() │
 *   │ ALGEBRAIC│ 符号高度 │ smtbackend_solve()       │
 *   │ 混合     │ 混合     │ 先符号回退数值           │
 *   └──────────┴──────────┴─────────────────────────┘
 */
typedef enum {
    EXEC_PATH_NUMERIC,          /* 纯数值计算 */
    EXEC_PATH_SYMBOLIC_LOW,     /* 符号计算（度数≤2，适合 Gröbner 基） */
    EXEC_PATH_SYMBOLIC_HIGH,    /* 符号计算（度数>2，适合 SMT） */
    EXEC_PATH_HYBRID            /* 混合路径：先符号回退数值 */
} ExecPath;

ExecPath type_driven_dispatch_oper(
    const TypeSystem *ts,
    const FuncBlock *operation,
    const SymbolicCoord **args,
    int arg_count);
```

### 4.3 几何操作速查表（类型驱动分发）

| 操作 | Point×Numeric → | Point×Symbolic → | 混合 → |
|:---|:---|:---|:---|
| `midpoint(A, B)` | 数值公式: `(A+B)/2` | 符号约束: `M.x=(A.x+B.x)/2` | 符号化简后数值 |
| `distance(A, B)` | `sqrt((Δx)²+(Δy)²)` | `sqrt(expr)` 保留符号 | 同符号 |
| `intersection(line, circle)` | 解二次方程 | 符号求解 `(x)²+(y)²=r²` | SMT 编码 |
| `area(triangle)` | 行列式/海伦公式 | 符号表达式展开 | 符号化简 |
| `angle(A, O, B)` | `arccos(dot/|OA||OB|)` | `arccos(expr)` 保留符号 | 同符号 |

---

## 5. 几何操作的符号/数值路径自动选择

### 5.1 自动路由规则表

借鉴 FriCAS 的类型→实现分发，为 Lv-00 设计一个基于规则的路由表：

```c
/**
 * @brief 操作符分发路由规则 —— 借鉴 FriCAS 的类型驱动分发
 *
 * 每条规则定义：当操作参数的类型满足某条件时，使用指定的执行路径。
 * 借鉴 FriCAS 的"类型签名→实现选择"模式。
 */
typedef struct {
    char *operation_name;               /* 操作名（如 "midpoint"） */
    int arg_count;                      /* 参数数量 */
    int *required_arg_types;            /* 每个参数要求的 SymbolicCoord 类型 */
    int *required_category_ids;         /* （可选）每个参数要求的 Category */
    int required_category_count;
    ExecPath preferred_path;            /* 首选执行路径 */
    int priority;                       /* 规则优先级（数值越小越优先） */
} DispatchRule;

/* 预置路由规则示例：
 * 规则1: midpoint 所有参数为 RATIONAL → NUMERIC 路径 (priority=1)
 * 规则2: midpoint 任一参数为 ALGEBRAIC → SYMBOLIC_LOW 路径 (priority=2)
 * 规则3: intersection 任一参数为 ALGEBRAIC → SYMBOLIC_HIGH 路径 (priority=1)
 * 规则4: distance 任一参数为 ALGEBRAIC → HYBRID 路径 (priority=1)
 */
```

### 5.2 自动升级规则

当两个操作数类型不一致时，FriCAS 会自动进行**类型升级**（coercion）——如 `Integer + Float` 会自动将 Integer 升级为 Float。Lv-00 在几何语境中同样需要这种自动升级：

```c
/**
 * @brief 自动类型升级（coercion）—— 借鉴 FriCAS 的 coercible
 *
 * 当同一个构造中的两个几何节点属于不同类型时，
 * 自动判断是否需要升级以及升级方向。
 *
 * FriCAS 等价: (x: Integer) + (y: Float) → 自动升级 Integer 到 Float
 * Lv-00 等价: midpoint(point_numeric, point_symbolic) → 数值先升级为符号
 *
 * 升级规则:
 *   RATIONAL → ALGEBRAIC   (升级: 将具体值包装为符号常量)
 *   GEOM_POINT → GEOM_REGION (升级: 将点视为退化的零维区域)
 *   GEOM_LINE_SEGMENT → GEOM_LINE (升级: 线段扩展为无限直线)
 */
bool type_auto_coerce(
    TypeSystem *ts,
    int from_node_id,
    int target_category_id,
    int *out_coerced_node_id);
```

---

## 6. 与 type_system.h 的集成设计

### 6.1 现有 type_system.h 与 FriCAS 范畴系统的对应

Lv-00 的 `type_system.h` 已经具备了许多基础设施，只需要扩展"Category 层"即可实现 FriCAS 风格的范畴驱动：

| type_system.h 现有结构 | 需要扩展 |
|:---|:---|
| `TypeRegion`（类型区域） | 新增 `category_id` 字段，声明所属范畴 |
| `TypeKind` 枚举（POINT/LINE/REGION/...） | 新增 `TYPE_KIND_CATEGORY` 用于范畴元类型 |
| `type_check_port_compatibility()` | 改为基于 Category 契约检查 |
| `type_infer_node()` | 加入"范畴成员查询"支持 |
| `TypeInferenceRule` | 新增 `required_category_id` 字段 |
| 无 | 新增 `CategoryDefinition` 结构体 |
| 无 | 新增 `GeomTypeCategoryMembership` 表 |
| 无 | 新增 `DispatchRule` 路由表 |

### 6.2 数据流：构造 → 类型查询 → 范畴验证 → 操作分发

```
DSL: point M = midpoint(A, B);
         │
         ▼
┌─────────────────────────────────────────────┐
│ 1. 类型查询: A、B 的类型是什么?              │
│    type_get_node_type(ts, A_id) → Point     │
│    type_get_node_type(ts, B_id) → Point     │
└──────────────────┬──────────────────────────┘
                   │ A: Point, B: Point
                   ▼
┌─────────────────────────────────────────────┐
│ 2. 范畴验证: Point 属于 MetricSpace 吗?      │
│    type_node_has_category(ts, A_id,          │
│        CAT_METRIC_SPACE) → true             │
│    (midpoint 需要 MetricSpace 范畴)          │
└──────────────────┬──────────────────────────┘
                   │ 范畴满足
                   ▼
┌─────────────────────────────────────────────┐
│ 3. 操作分发: 选择合适的执行路径              │
│    dispatch_rule = find_dispatch_rule(       │
│        "midpoint", [A, B]);                 │
│    → A.x=0(RATIONAL), B.x=6(RATIONAL)       │
│    → EXEC_PATH_NUMERIC                      │
└──────────────────┬──────────────────────────┘
                   │ 数值路径
                   ▼
┌─────────────────────────────────────────────┐
│ 4. 执行并返回                               │
│    M = symbolic_coord_from_rational(3, 0)   │
│    result → GEOM_POINT, 范畴 = MetricSpace  │
└─────────────────────────────────────────────┘
```

---

## 7. 实现路线图

### 7.1 第一阶段：范畴层级定义（P3）

| 任务 | 文件 | 说明 |
|:---|:---|:---|
| `CategoryDefinition` 结构体 | `include/lv00/type_system.h` | 新增 Category 类型 |
| 预置几何 Category 表 | `src/type_system.c` | `CAT_GEOM_ENTITY`, `CAT_LINEAR`, `CAT_METRIC_SPACE`, `CAT_EUCLIDEAN`, etc. |
| `GeomTypeCategoryMembership` 表 | `src/type_system.c` | Point ∈ {GeomEntity, MetricSpace, EuclideanSpace} 等成员关系 |
| `type_node_has_category()` | `src/type_system.c` | 范畴成员查询 |
| `category_is_subcategory()` | `src/type_system.c` | 范畴继承查询 |

**预估规模**：约 120 行 C 代码

### 7.2 第二阶段：类型驱动的操作符分发（P3）

| 任务 | 文件 | 说明 |
|:---|:---|:---|
| `DispatchRule` 结构体 | `include/lv00/type_system.h` | 新增分发规则 |
| 预置分发规则表 | `src/type_system.c` | midpoint/distance/intersection/area 等操作的默认规则 |
| `type_driven_dispatch_oper()` | `src/type_system.c` | 核心分发函数 |
| `type_auto_coerce()` | `src/type_system.c` | 自动类型升级 |
| 集成到 `scheduler_select_backend()` | `src/engine_scheduler.cpp` | 将类型驱动的分发合并到求解器调度 |

**预估规模**：约 150 行 C 代码

### 7.3 第三阶段：.lvz 范畴声明语法（P3-P4）

| 任务 | 说明 |
|:---|:---|
| `@category` 指令 | .lvz 文件中声明范畴（类似 Axiom 的 Category 定义） |
| `@domain` 指令 | .lvz 文件中声明具体几何类型的范畴成员关系 |
| `.lvz` 解析器扩展 | `lvz_parser.c` 支持 `@category` / `@domain` 语法 |
| 范畴一致性验证 | 加载公理包时验证所有声明的范畴成员关系 |

---

## 8. 关键映射表

### 8.1 FriCAS/Axiom → Lv-00 概念映射

| FriCAS/Axiom | Lv-00 映射 | Lv-00 文件 |
|:---|:---|:---|
| `Category`（范畴类型） | `CategoryDefinition` 结构体 | `type_system.h` |
| `Domain`（实现了 Category 的类型） | `GeomType` + `TypeRegion` | `constraint_graph.h` + `type_system.h` |
| `Package`（算法集合） | `FuncBlock` 注册表 | `func_block.h` |
| `has`（范畴成员测试） | `type_node_has_category()` | `type_system.h` |
| `CoercibleTo`（类型强制） | `type_auto_coerce()` | `type_system.h` |
| `with { op1; op2; ... }` | `required_operation_names[]` | `type_system.h` |
| `Join(CatA, CatB, ...)` | `parent_category_ids[]` 多重继承 | `type_system.h` |
| Conditional Category | `ceq` 条件等式 / `required_category_ids` | `constraint_graph.h` |

### 8.2 几何范畴表（预置）

| 范畴 | 必需操作 | 必需公理 | 成员类型 |
|:---|:---|:---|:---|
| `GeomEntity` | `has_same_type` | unifiable | Point, Line, Circle, Region |
| `LinearEntity` extends GeomEntity | `translate`, `scale` | affine_closed | Line, Segment, Ray |
| `MetricSpace` extends GeomEntity | `distance`, `angle` | triangle_ineq | Point (distance to point), Circle (radius) |
| `EuclideanSpace` extends MetricSpace, LinearEntity | `perpendicular`, `parallel` | parallel_postulate | Point, Line, Circle (欧几里得) |
| `ProjectiveSpace` extends GeomEntity | `cross_ratio` | desargues | Point, Line (投影) |
| `HyperbolicSpace` extends GeomEntity | `hyperbolic_distance` | hyperbolic_parallel | Point, Line (双曲) |

---

## 附录 A：Axiom 经典示例与 Lv-00 对照

```
Axiom 经典示例:
(1) -> integrate(x^2 * sin(x), x)
                                    2
(1)  - 2 cos(x) - x sin(x) + x  + 2x cos(x) + (x  - 2)sin(x)
                                          Type: Union(Expression Integer,...)

(2) -> 2 + 3.0
(2)  5.0
                               Type: Float
// 注意: Integer + Float → Float (自动升级)


Lv-00 等价:
lv00> number I = integrate(x^2 * sin(x), x);  // 外部CAS
:1  I = -2cos(x) - x*sin(x) + x + 2x*cos(x) + (x^2 - 2)*sin(x)

lv00> number R = RATIONAL(2) + RATIONAL(3);
:2  R = 5  (type: RATIONAL)

lv00> number F = RATIONAL(2) + RATIONAL(3.0);
:3  F = 5.0  (type: RATIONAL → 自动升级为浮点)
```

---

> **文档结束**
> 本文档详述了 FriCAS/Axiom 范畴论驱动的类型系统如何映射到 Lv-00 的 `type_system.h`——通过引入 Category 定义、范畴成员关系表和类型驱动的操作符分发路由，实现"几何对象的类型决定其可用操作和公理"的设计目标。这与 Lv-00 "公理中立"理念一致——不同的 Category（如 EuclideanSpace vs ProjectiveSpace）对应不同的公理集合，加载不同的公理包即启用不同的范畴。

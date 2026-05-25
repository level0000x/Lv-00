# PVS 谓词子类型 + TCC 自动生成 核心借鉴设计

> **借鉴项目**：PVS（github.com/SRI-CSL/PVS）
> **核心借鉴点**：谓词子类型（Predicate Subtypes）与 TCC 自动生成、判定过程深度集成、参数化理论（Theory Interpretations）、地面求值器
> **分类**：P2 高优先级 / 类型系统与判定过程集成
> **日期**：2026-05-24

---

## 1. 概述

PVS（Prototype Verification System）是由 SRI International 计算机科学实验室开发的交互式定理证明环境，其核心特色在于**谓词子类型**（Predicate Subtypes）——一种将逻辑断言直接嵌入类型标注的机制。在 PVS 中，`{n: nat | n > 0}` 不仅是一个类型，更是一个携带了逻辑条件的"鲜活类型"。每次使用该类型的值时，PVS 都会自动生成 **TCC（Type Correctness Conditions，类型正确性条件）**——一种证明义务，确保所有对该类型的引用都在逻辑上自洽。

PVS 对 Lv-00 的核心借鉴价值体现在三个层面。第一，谓词子类型的"类型 + 约束"模式精确对应 Lv-00 已有的 `TypeRegion.constraint_ids` 设计——这意味着 Lv-00 已经拥有谓词子类型的基础设施，但缺少 PVS 式的 TCC 自动生成和验证机制。第二，PVS 的**判定过程（Decision Procedures）**将算术、位向量、等式求解等判定过程与策略引擎紧耦合——Lv-00 的几何约束求解同样需要将 Groebner 基法、面积法、坐标消解法等判定过程深度嵌入验证流程。第三，PVS 的**参数化理论（Theory Interpretations）**提供了一种将几何变换（平移、旋转、反射）模块化的优雅方案。

PVS 的另一个独特优势是**地面求值器（Ground Evaluator）**——一个将规范语言直接编译为可执行代码的求值器。在 Lv-00 中，这对应将几何构造直接转换为可绘制的可视化输出：规范即实现，声明即执行。

---

## 2. 谓词子类型与约束类型映射

### 2.1 PVS 谓词子类型的核心语义

PVS 谓词子类型的完整定义形式：

```
posint: TYPE = {n: nat | n > 0}
nonzero_real: TYPE = {x: real | x /= 0}
unit_interval: TYPE = {x: real | 0 <= x AND x <= 1}
sorted_list[T]: TYPE = {l: list[T] | FORALL (i,j: below(length(l))): i < j IMPLIES l(i) <= l(j)}
```

语义模型：
- **基础类型**（如 `nat`、`real`）定义"候选值池"
- **谓词条件**（如 `n > 0`）从候选池中过滤出有效值
- **子类型关系**：`posint` 是 `nat` 的子类型，`nat` 是 `int` 的子类型
- **TCC 触发**：每当一个值被断定为 `posint` 类型时，自动产生 `value > 0` 的证明义务

### 2.2 映射到 Lv-00 的 TYPE_KIND_REGION + constraint_ids

Lv-00 的类型系统已经具备谓词子类型的基础设施——`TypeRegion.constraint_ids` 字段就是附在类型上的"谓词"。PVS 的谓词子类型模型为这一机制提供了系统化的理论支撑和工程实践参考：

| PVS 谓词子类型 | Lv-00 TYPE_KIND_REGION + constraint | 几何语义 |
|:---|:---|:---|
| `{n: nat \| n > 0}` | `TYPE_KIND_LINE_SEGMENT` + `[LENGTH_POSITIVE]` | 长度为正的线段 |
| `{x: real \| x /= 0}` | `TYPE_KIND_LINE_SEGMENT` + `[NON_ZERO_LENGTH]` | 非退化线段 |
| `{x: real \| 0 <= x <= 1}` | `TYPE_KIND_POINT` + `[ON_SEGMENT(AB)]` | 线段上的点（参数化位置） |
| `{T: triangle \| is_right_angle(T)}` | `TYPE_KIND_TRIANGLE` + `[RIGHT_ANGLE]` | 直角三角形 |
| `{C: circle \| radius(C) > 0}` | `TYPE_KIND_CIRCLE` + `[RADIUS_POSITIVE]` | 非退化圆 |
| `{P: point \| on_circle(P, C)}` | `TYPE_KIND_POINT` + `[ON_CIRCLE(C)]` | 圆上的点 |

### 2.3 几何谓词子类型的约束定义

```c
/**
 * @brief 几何约束 ID 枚举——PVS 谓词子类型在 Lv-00 中的约束映射
 *
 * 每个约束 ID 对应一个几何谓词，附加在 TypeRegion 的 constraint_ids 数组中。
 * 借鉴 PVS 的谓词子类型模型：基础类型 + 约束列表 = 精化几何类型。
 */
typedef enum {
    /* 线段约束 */
    CONSTRAINT_LENGTH_POSITIVE,        /**< 长度 > 0（非退化） */
    CONSTRAINT_LENGTH_EQ,             /**< 长度等于给定值 */
    CONSTRAINT_LENGTH_LT,             /**< 长度小于给定值 */
    CONSTRAINT_LENGTH_GT,             /**< 长度大于给定值 */

    /* 角度约束 */
    CONSTRAINT_ANGLE_RIGHT,           /**< 直角（= 90°） */
    CONSTRAINT_ANGLE_ACUTE,           /**< 锐角（< 90°） */
    CONSTRAINT_ANGLE_OBTUSE,          /**< 钝角（> 90°） */
    CONSTRAINT_ANGLE_EQ,              /**< 角度等于给定值 */

    /* 点位置约束 */
    CONSTRAINT_ON_SEGMENT,            /**< 点在某线段上 */
    CONSTRAINT_ON_LINE,               /**< 点在直线上 */
    CONSTRAINT_ON_CIRCLE,             /**< 点在圆上 */
    CONSTRAINT_INSIDE_CIRCLE,         /**< 点在圆内 */
    CONSTRAINT_OUTSIDE_CIRCLE,        /**< 点在圆外 */

    /* 形状约束 */
    CONSTRAINT_TRIANGLE_RIGHT,        /**< 直角三角形 */
    CONSTRAINT_TRIANGLE_ISOSCELES,    /**< 等腰三角形 */
    CONSTRAINT_TRIANGLE_EQUILATERAL,  /**< 等边三角形 */
    CONSTRAINT_QUAD_PARALLELOGRAM,    /**< 平行四边形 */
    CONSTRAINT_QUAD_RECTANGLE,        /**< 矩形 */
    CONSTRAINT_QUAD_SQUARE,           /**< 正方形 */

    /* 关系约束 */
    CONSTRAINT_COLLINEAR,             /**< 点共线 */
    CONSTRAINT_CONCYCLIC,             /**< 点共圆 */
    CONSTRAINT_PARALLEL,              /**< 线段平行 */
    CONSTRAINT_PERPENDICULAR,         /**< 线段垂直 */
    CONSTRAINT_CONGRUENT,             /**< 图形全等 */
    CONSTRAINT_SIMILAR,               /**< 图形相似 */
} GeometryConstraintId;
```

---

## 3. TCC 自动生成机制

### 3.1 PVS 的 TCC 生成原理

PVS 的 TCC（Type Correctness Conditions）是在类型检查过程中自动生成的证明义务。每当程序中使用了谓词子类型的值，类型检查器就生成一个 TCC 来确保该值满足子类型的谓词条件。

PVS 的 TCC 触发场景：

```
场景1: 类型标注
  f(x: posint): posint = x + 1
  → TCC1: x > 0 ?  （输入满足 posint）
  → TCC2: x+1 > 0 ? （输出满足 posint）

场景2: 模式匹配 / case 分支
  CASES x OF
    posint: ...   → TCC: 是否所有 posint 的 case 都被覆盖？
  
场景3: 数组下标
  arr(i) where arr: ARRAY[posint -> real]
  → TCC: i > 0 ?

场景4: 类型转换（narrowing）
  x: nat ⊢ x > 0  → 可将 x 提升为 posint
  → TCC: x > 0 （需要从上下文中可推导）
```

### 3.2 Lv-00 中的 TCC 生成

在 Lv-00 中，TCC 的对应物是**约束检查点**——当几何构造引用了携带约束的类型时，自动向约束图插入一个验证节点：

```c
/**
 * @brief TCC 生成器——借鉴 PVS 的 Type Correctness Conditions
 *
 * 当构造图节点使用带有 constraint_ids 的 TypeRegion 时，
 * 自动生成约束验证节点并插入约束图。
 *
 * TCC 触发场景（对应 PVS 的四种场景）：
 *
 * 场景1: 输入标注——函数参数携带约束类型
 *   例：函数 midpoint(A: Point, B: Point) -> Point{ON_SEGMENT(AB)}
 *   TCC: 返回值是否在 AB 上？
 *
 * 场景2: 输出标注——函数返回值携带约束类型
 *   例：函数 sqrt_area(T: Triangle{RIGHT_ANGLE}) -> real
 *   TCC: 输入是否满足直角三角形约束？
 *
 * 场景3: 约束窄化——将满足约束的对象归类为更具体的类型
 *   例：三点不共线 → 可归类为 Triangle
 *   TCC: 三点是否不共线？
 *
 * 场景4: 约束链——窄化链上的每个环节
 *   例：Quadrilateral → Parallelogram → Rectangle → Square
 *   TCC: 每个窄化步骤的约束是否满足？
 */
typedef struct {
    int trigger_node_id;       /**< 触发 TCC 的构造节点 */
    int constraint_id;         /**< 需要验证的约束 */
    ConstraintCheckStatus status; /**< 验证状态 */
    char *description;         /**< TCC 人类可读描述 */
    int *dependencies;         /**< 验证所依赖的节点 */
    int dep_count;             /**< 依赖计数 */
} TypeCorrectnessCondition;

typedef enum {
    TCC_SATISFIED,             /**< TCC 已满足（自动或手动证明） */
    TCC_UNSATISFIED,           /**< TCC 未满足（存在反例） */
    TCC_PENDING,               /**< TCC 待验证 */
    TCC_DEFERRED,              /**< TCC 推迟验证 */
    TCC_TRIVIAL                /**< TCC 平凡成立（无需证明） */
} ConstraintCheckStatus;
```

### 3.3 TCC 自动证明策略

```c
/**
 * @brief TCC 自动证明引擎
 *
 * 借鉴 PVS 的判定过程集成模型，对每种几何约束类型
 * 提供专用的验证策略。
 *
 * 验证策略按计算成本排序（先尝试低成本策略）：
 *  1. 平凡检查（trivial_check）：约束 ID 已知成立
 *  2. 重写检查（rewrite_check）：通过重写路径探索
 *  3. 代数检查（algebraic_check）：Groebner 基法
 *  4. 数值检查（numeric_check）：数值逼近验证
 *  5. SMT 检查（smt_check）：外部 SMT 求解器
 */
typedef enum {
    TCC_STRATEGY_TRIVIAL,      /**< 平凡检查 */
    TCC_STRATEGY_REWRITE,      /**< 重写路径探索 */
    TCC_STRATEGY_ALGEBRAIC,    /**< 代数求解 */
    TCC_STRATEGY_NUMERIC,      /**< 数值验证 */
    TCC_STRATEGY_SMT,          /**< SMT 外部求解 */
    TCC_STRATEGY_INTERACTIVE   /**< 交给人交互式证明 */
} TCCProofStrategy;

/**
 * @brief 尝试自动证明所有待验证的 TCC
 *
 * @return 成功证明的 TCC 数量，-1 表示内部错误
 */
int tcc_auto_prove(
    ConstraintGraph *graph,
    TypeSystem *ts,
    ConstraintSolver *solver
);
```

---

## 4. 判定过程深度集成

### 4.1 PVS 的判定过程架构

PVS 的一个核心设计是将多种判定过程（Decision Procedures）与策略引擎紧耦合：

```
PVS 策略引擎
  │
  ├─ (grind) 组合策略
  │    ├─ (assert)   算术判定过程（线性算术、非线性乘法）
  │    ├─ (reduce)   地面求值器（将表达式归约为常量）
  │    ├─ (prop)     命题逻辑判定过程（BDD + DPLL）
  │    ├─ (rewrite)  重写引擎（自动应用已知等式）
  │    └─ (bddsimp)  BDD 简化
  │
  ├─ (field)  域论判定过程
  ├─ (real-props) 实数区间判定过程
  ├─ (interval) 区间算术判定过程
  └─ (mu-calculus) μ-演算模型检查

每次策略调用时，PVS 自动选择最合适的判定过程组合。
```

### 4.2 Lv-00 几何判定过程集成

```c
/**
 * @brief 几何判定过程集成——借鉴 PVS 的 decision procedure 架构
 *
 * 每个几何判定过程处理一类专用的几何约束验证问题。
 * 判定过程通过统一接口注册，由验证引擎按优先级调度。
 */
typedef enum {
    DECISION_PROC_COLLINEARITY,    /**< 共线性判定（行列式法） */
    DECISION_PROC_DISTANCE,        /**< 距离关系判定（坐标消解） */
    DECISION_PROC_ANGLE,           /**< 角度关系判定（向量内积） */
    DECISION_PROC_AREA,            /**< 面积关系判定（面积法） */
    DECISION_PROC_CONCURRENCE,     /**< 共点性判定（Ceva/Menelaus） */
    DECISION_PROC_CONCYCLICITY,    /**< 共圆性判定 */
    DECISION_PROC_PARALLELISM,     /**< 平行性判定 */
    DECISION_PROC_PERPENDICULARITY,/**< 垂直性判定 */
    DECISION_PROC_CONGURENCE,      /**< 全等判定（SSS/SAS/ASA） */
    DECISION_PROC_SIMILARITY,      /**< 相似判定 */
} DecisionProcedureId;

/**
 * @brief 判定过程接口
 *
 * 借鉴 PVS 的 decision procedure 插件架构。
 * 每个判定过程实现此接口，由验证引擎统一调度。
 */
typedef struct {
    DecisionProcedureId id;
    const char *name;
    const char *description;

    /**
     * @brief 初始化判定过程所需的数据结构
     */
    bool (*init)(void *state, const ConstraintGraph *graph);

    /**
     * @brief 判定过程的核心验证逻辑
     *
     * @param state  过程状态
     * @param nodes  涉及的构造节点 ID 数组
     * @param count  节点数量
     * @param result 输出：验证结果
     * @return true 若判定可完成，false 若超出能力范围
     */
    bool (*decide)(void *state, const int *nodes, int count,
                   DecisionResult *result);

    /**
     * @brief 产生反例（当验证失败时）
     */
    char *(*generate_counterexample)(void *state);

    /**
     * @brief 释放状态
     */
    void (*destroy)(void *state);
} DecisionProcedure;

/**
 * @brief 验证引擎——集成多个判定过程
 */
typedef struct {
    DecisionProcedure **procedures;  /**< 注册的判定过程列表 */
    int proc_count;                  /**< 过程数量 */
    ConstraintGraph *graph;          /**< 约束图引用 */
    TypeSystem *ts;                  /**< 类型系统引用 */
} VerificationEngine;

/**
 * @brief 向验证引擎注册判定过程
 */
bool ve_register_procedure(VerificationEngine *ve, DecisionProcedure *proc);

/**
 * @brief 按优先级尝试所有判定过程
 *
 * 借鉴 PVS 的 grind 组合策略：依次尝试各判定过程，
 * 第一个返回确定结果的过程胜出。
 */
DecisionResult ve_decide(VerificationEngine *ve,
                          const int *nodes, int count);
```

---

## 5. 参数化理论与几何变换

### 5.1 PVS 的参数化理论

PVS 的理论解释（Theory Interpretations）机制允许将抽象理论的类型和常量映射到具体实例：

```
% 抽象群论理论
group[T: TYPE, *: [T,T -> T], one: T]: THEORY
BEGIN
  assoc: AXIOM FORALL (x,y,z): (x * y) * z = x * (y * z)
  left_id: AXIOM FORALL (x): one * x = x
  right_id: AXIOM FORALL (x): x * one = x
END group

% 具体解释：将 group 解释为实数加法
IMPORTING group[real, +, 0]
```

### 5.2 几何变换的参数化模型

将几何变换（平移、旋转、反射、缩放）建模为参数化理论：

```c
/**
 * @brief 几何变换的参数化理论——借鉴 PVS Theory Interpretations
 *
 * 将几何变换建模为可参数化的构造块。
 * 每个变换是一个"参数化理论"：提供变换的类型参数后，
 * 变换的属性和定理自动实例化。
 *
 * 仿射变换群 → 对应 PVS 的 group 理论，解释为 {R^2, 矩阵乘法, I}
 */

/**
 * @brief 几何变换类型
 */
typedef enum {
    TRANSFORM_TRANSLATION,     /**< 平移 T_v(x) = x + v */
    TRANSFORM_ROTATION,        /**< 旋转 R_θ(x) */
    TRANSFORM_REFLECTION,      /**< 反射 M_L(x) 关于直线 L */
    TRANSFORM_SCALING,         /**< 缩放 S_k(x) = k*x */
    TRANSFORM_HOMOTHETY,       /**< 位似 H_(c,k)(x) = c + k*(x-c) */
    TRANSFORM_AFFINE,          /**< 一般仿射变换 Ax + b */
} TransformType;

/**
 * @brief 变换参数（不同变换类型使用不同参数）
 */
typedef union {
    struct { double dx, dy; } translation;       /**< 平移向量 */
    struct { double cx, cy, angle; } rotation;   /**< 旋转中心 + 角度 */
    struct { double a, b, c; } reflection;       /**< 反射对称轴参数 */
    struct { double cx, cy, factor; } scaling;   /**< 缩放中心 + 因子 */
    struct { double m[2][2]; double b[2]; } affine; /**< 仿射矩阵 + 偏移 */
} TransformParams;

/**
 * @brief 应用几何变换并自动生成 TCC
 *
 * 借鉴 PVS Theory Interpretations 的参数化方式。
 * 变换后的对象自动继承原对象的可变换约束（如距离比不变、角度保持等）。
 *
 * @example
 *   // 平移三角形 ABC
 *   TransformParams tp = { .translation = {3.0, 4.0} };
 *   int tri_ApBpCp = transform_apply(ts, graph, tri_ABC,
 *       TRANSFORM_TRANSLATION, &tp);
 *   // 自动生成 TCC：三角形 ApBpCp 是否与原三角形全等
 *   // （平移保持距离 → 自动证明 SSS 全等）
 */
int transform_apply(
    TypeSystem *ts,
    ConstraintGraph *graph,
    int source_object_id,
    TransformType type,
    const TransformParams *params
);

/**
 * @brief 变换保持性质的定理模式
 *
 * 借鉴 PVS 的 Theory Interpretations：每个变换类型
 * 自动实例化对应的"保持性质"定理。
 */
typedef struct {
    TransformType transform;
    GeometryConstraintId preserved_constraint;  /**< 该变换保持的约束 */
    bool is_exact;                              /**< true=严格保持，false=比例保持 */
} TransformPreservation;

static const TransformPreservation preservation_table[] = {
    { TRANSFORM_TRANSLATION,  CONSTRAINT_CONGRUENT,   true  },
    { TRANSFORM_TRANSLATION,  CONSTRAINT_PARALLEL,    true  },
    { TRANSFORM_ROTATION,     CONSTRAINT_CONGRUENT,   true  },
    { TRANSFORM_ROTATION,     CONSTRAINT_ANGLE_EQ,    true  },
    { TRANSFORM_REFLECTION,   CONSTRAINT_CONGRUENT,   true  },
    { TRANSFORM_REFLECTION,   CONSTRAINT_ANGLE_EQ,    true  },
    { TRANSFORM_SCALING,      CONSTRAINT_SIMILAR,     true  },
    { TRANSFORM_SCALING,      CONSTRAINT_ANGLE_EQ,    true  },
    { TRANSFORM_SCALING,      CONSTRAINT_CONGRUENT,   false },  // 大小改变
    { TRANSFORM_HOMOTHETY,    CONSTRAINT_SIMILAR,     true  },
    { TRANSFORM_AFFINE,       CONSTRAINT_COLLINEAR,   true  },
    { TRANSFORM_AFFINE,       CONSTRAINT_PARALLEL,    true  },
};
```

---

## 6. 地面求值器与可执行规范

### 6.1 PVS 的地面求值器

PVS 的 Ground Evaluator 将规范语言直接编译为 Common Lisp 可执行代码：

```
% PVS 规范（同时是规范和可执行代码）
fib(n: nat): RECURSIVE nat =
  IF n <= 1 THEN n
  ELSE fib(n-1) + fib(n-2)
  ENDIF
MEASURE n

% 地面求值器直接执行
PVS> (eval "fib(10)" :theory "fibonacci")
==> 55
```

### 6.2 Lv-00 的可视化求值器

借鉴 PVS 地面求值器的"规范即实现"理念，Lv-00 的几何构造可直接"求值"为可视化输出：

```c
/**
 * @brief 几何可视化求值器——借鉴 PVS Ground Evaluator
 *
 * 将几何构造直接"求值"为可视化绘制指令。
 * 规范（几何构造图） = 实现（可视化输出）。
 *
 * 借鉴 PVS 的"eval"模式：lv00_eval(construction) → SVG/Canvas绘制指令。
 */
typedef struct {
    /** 绘制指令类型 */
    enum { DRAW_POINT, DRAW_SEGMENT, DRAW_CIRCLE, DRAW_ARC,
           DRAW_POLYGON, DRAW_LABEL, DRAW_ANGLE_MARK } type;
    /** 绘制参数（坐标、颜色、线宽等） */
    union {
        struct { double x, y; double radius; char *color; } point;
        struct { double x1, y1, x2, y2; double width; char *color; } segment;
        struct { double cx, cy, r; char *stroke; char *fill; } circle;
        struct { double *xs, *ys; int count; char *fill; } polygon;
        struct { double x, y; char *text; } label;
    } params;
} DrawCommand;

/**
 * @brief 求值几何构造，生成绘制指令
 *
 * 借鉴 PVS 的 eval 模式：将构造图"求值"为可视化。
 */
int lv00_eval_to_draw(
    ConstraintGraph *graph,
    int root_node_id,
    DrawCommand **out_commands,
    int *out_count
);
```

---

## 7. 代码示例：直角三角形谓词子类型

以下完整示例展示如何在 Lv-00 中实现"直角三角形"作为谓词子类型，并自动验证勾股定理约束：

```c
/**
 * @brief 直角三角形谓词子类型——借鉴 PVS Predicate Subtypes
 *
 * PVS 定义：right_triangle: TYPE = {T: triangle | is_right_angle(T)}
 *
 * Lv-00 对应：TYPE_KIND_TRIANGLE + constraint_ids = [CONSTRAINT_TRIANGLE_RIGHT]
 *
 * 本示例展示：
 *  1. 创建直角三角形类型
 *  2. 构造一个具体的直角三角形（勾3股4弦5）
 *  3. TCC 自动生成：验证是否满足直角约束
 *  4. 自动验证勾股定理
 */
void example_right_triangle_predicate_subtype(void)
{
    TypeSystem *ts = type_system_create();
    VerificationEngine *ve = ve_create();

    // --- 步骤1：定义直角三角形类型（谓词子类型） ---

    // PVS: right_triangle: TYPE = {T: triangle | is_right_angle(T)}
    // Lv-00: 创建基础三角形类型
    TypeRegion *base_triangle = type_create_region(ts,
        TYPE_KIND_TRIANGLE, UNIVERSE_GEOMETRY);

    // 附加直角约束——这正是谓词子类型的"谓词"部分
    int constraints[] = {
        CONSTRAINT_TRIANGLE_RIGHT,    // 直角约束
        CONSTRAINT_LENGTH_POSITIVE,   // 边长 > 0
    };

    TypeRegion *right_triangle = type_create_refinement(
        ts, base_triangle, constraints, 2);

    // --- 步骤2：构造勾3股4弦5直角三角形 ---

    // 创建三点 A(0,0), B(3,0), C(0,4)
    int node_A = constraint_graph_add_point(graph, 0.0, 0.0);
    int node_B = constraint_graph_add_point(graph, 3.0, 0.0);
    int node_C = constraint_graph_add_point(graph, 0.0, 4.0);

    // 构造三角形 ABC
    int nodes[] = {node_A, node_B, node_C};
    int tri_ABC = constraint_graph_add_polygon(graph, nodes, 3);

    // 将三角形标注为直角三角形类型（触发 TCC 生成）
    type_assign_region(ts, tri_ABC, right_triangle);

    // --- 步骤3：TCC 自动生成与验证 ---

    // assign 操作自动触发了 TCC：
    //   TCC1: angle ABC 是否为直角？(AB ⟂ BC)
    //   TCC2: 三边长是否都 > 0？

    // 自动生成 TCC 列表
    int tcc_count = 0;
    TypeCorrectnessCondition *tccs = tcc_generate_for_node(
        ve, tri_ABC, &tcc_count);

    printf("生成 %d 个 TCC\n", tcc_count);
    // 输出示例：
    //   TCC1 [CONSTRAINT_TRIANGLE_RIGHT]:
    //     验证三角形 ABC 存在直角
    //   TCC2 [CONSTRAINT_LENGTH_POSITIVE]:
    //     验证所有边长 > 0

    // 自动证明 TCC
    for (int i = 0; i < tcc_count; i++) {
        DecisionResult result = ve_decide(ve,
            tccs[i].dependencies, tccs[i].dep_count);

        switch (result) {
        case DECISION_SAT:
            printf("TCC%d [%s]: 自动验证通过\n",
                i+1, tccs[i].description);
            break;
        case DECISION_UNSAT:
            printf("TCC%d [%s]: 验证失败！\n",
                i+1, tccs[i].description);
            printf("  反例：%s\n",
                ve_generate_counterexample(ve, &tccs[i]));
            break;
        case DECISION_UNKNOWN:
            printf("TCC%d [%s]: 需要交互式证明\n",
                i+1, tccs[i].description);
            break;
        }
    }

    // --- 步骤4：自动验证勾股定理 ---

    // PVS 验证流程：
    // 1. 已知 triangle ABC 满足 right_triangle 谓词子类型
    // 2. TCC 确保 C 为直角
    // 3. 勾股定理：|AB|^2 = |AC|^2 + |BC|^2
    // 4. 代入坐标：5^2 = 3^2 + 4^2 → 25 = 9 + 16 → 25 = 25 ✓

    // Lv-00 中的勾股定理约束验证
    int pythagorean_constraint = constraint_create_pythagorean(
        graph, tri_ABC, node_C  // 直角顶点为 C
    );

    DecisionResult pyth_result = ve_decide(ve,
        &pythagorean_constraint, 1);
    printf("勾股定理验证：%s\n",
        pyth_result == DECISION_SAT ? "通过" : "失败");

    // 清理
    free(tccs);
    ve_destroy(ve);
    type_system_destroy(ts);
}
```

---

## 8. 对照表：PVS 类型构造 → Lv-00 type_system.h

| PVS 类型构造 | 定义语法 | Lv-00 type_system.h 映射 | 说明 |
|:---|:---|:---|:---|
| 基础类型 | `nat: TYPE` | `TYPE_KIND_NAT` / `TYPE_KIND_REAL` | 原子类型 |
| 谓词子类型 | `{x:nat \| x>0}` | `TYPE_KIND_REFINEMENT` + `constraint_ids` | 基础类型 + 约束 |
| 函数类型 | `[T1 -> T2]` | `TYPE_KIND_FUNCTION` | 函数类型区域 |
| 元组类型 | `[T1, T2, T3]` | `TYPE_KIND_PRODUCT` | 乘积类型（如 Point2D） |
| 记录类型 | `[# a:T1, b:T2 #]` | `TYPE_KIND_PRODUCT` + 字段名 | 带命名字段的乘积 |
| 递归数据类型 | `DATATYPE ... END` | `TYPE_KIND_RECURSIVE` | 递归类型（如几何构造树） |
| 抽象数据类型 | `T: TYPE+` | `TYPE_KIND_OPAQUE` | 不透明类型 |
| 依赖类型 | `vec[T,n:nat]` | `TYPE_KIND_DEPENDENT` | 长度依赖的向量 |
| 子类型声明 | `S: TYPE FROM T` | `TypeRegion` 子类型链 | 类型层级 |
| 判定子类型 | `JUDGEMENT ... HAS_TYPE` | 类型推断 + 约束检查 | 类型推断规则 |

---

## 9. 实现路线图

### 9.1 第一阶段：谓词子类型基础设施（P2-1）

- [ ] 完善 `GeometryConstraintId` 枚举，覆盖所有几何约束类型
- [ ] 实现约束的 C 语言表达式生成器（坐标 → 代数公式）
- [ ] 实现 `type_create_refinement()` 的约束组合逻辑
- [ ] 实现约束链（精化链，如 Triangle → Right Triangle → 3-4-5 Triangle）
- [ ] 编写约束组合的单元测试

### 9.2 第二阶段：TCC 自动生成（P2-2）

- [ ] 实现 `TypeCorrectnessCondition` 数据结构
- [ ] 实现 `tcc_generate_for_node()` —— 对单个节点生成 TCC
- [ ] 实现 `tcc_generate_for_graph()` —— 对整个约束图生成 TCC
- [ ] 实现 TCC 分类（平凡/可自动/需交互）
- [ ] 实现 `tcc_auto_prove()` 自动证明引擎
- [ ] 编写 TCC 生成的集成测试

### 9.3 第三阶段：判定过程集成（P2-3）

- [ ] 定义 `DecisionProcedure` 接口
- [ ] 实现各几何判定过程：
  - [ ] `proc_collinearity`（行列式法共线判定）
  - [ ] `proc_distance`（坐标消解距离判定）
  - [ ] `proc_angle`（向量内积角度判定）
  - [ ] `proc_area`（面积法线段比例判定）
  - [ ] `proc_congruence`（SSS/SAS/ASA 全等判定）
  - [ ] `proc_similarity`（相似判定）
- [ ] 实现 `VerificationEngine` 集成调度器
- [ ] 编写判定过程的基准测试

### 9.4 第四阶段：参数化变换（P2-4）

- [ ] 实现 `TransformParams` 数据结构
- [ ] 实现 `transform_apply()` 变换应用
- [ ] 实现 `TransformPreservation` 保持性质表
- [ ] 实现变换后自动生成保持性质的 TCC
- [ ] 实现变换的可视化求值（`lv00_eval_to_draw`）
- [ ] 编写变换与可视化的集成测试

---

## 10. 设计决策与权衡

### 10.1 约束验证时机：编译期 vs 运行时

PVS 将所有 TCC 推迟到类型检查分离阶段验证，不阻塞类型推导本身。Lv-00 需要考虑类似的分阶段策略：

- **编译期（类型检查阶段）**：生成 TCC，但仅验证可自动证明的 TCC
- **编译期（分离验证阶段）**：对标记为 `TCC_DEFERRED` 的 TCC 进行批量验证
- **运行期**：`TCC_PENDING` 的 TCC 延迟到实际使用时验证——失败则触发运行时错误
- **交互期**：`DECISION_UNKNOWN` 的约束交由用户交互式证明

### 10.2 判定过程组合 vs 单一求解器

PVS 的 grind 策略展示了"多判定过程组合"优于"单一全能求解器"的设计模式。Lv-00 采用同样策略：

- 每个判定过程是独立可测试的模块
- 验证引擎按成本排序优先级调用判定过程
- 判定过程的结果可缓存复用
- 新的几何判定方法（如基于复数的方法）可以作为新过程插入

### 10.3 参数化理论的适用范围

并非所有变换都适合参数化理论模型。适用于：
- **刚性变换**（平移/旋转/反射）：保持距离和角度，全等关系明确
- **相似变换**（缩放/位似）：保持角度和比例，相似关系明确

不适用于：
- **剪切变换**：不保持任何标准的几何不变量
- **投影变换**：不保持平行性和比例（需要射影几何的单独处理）
- **拓扑变换**：超出欧几里得几何的范畴

---

## 11. 参考资源

- PVS 项目主页：https://github.com/SRI-CSL/PVS
- PVS 官方网站与文档：https://pvs.csl.sri.com/
- 《PVS Language Reference》—— PVS 语言参考手册
- 《PVS Prover Guide》—— PVS 证明器使用指南
- 《Predicate Subtypes in PVS》（Rushby, Owre, Shankar）—— 谓词子类型的核心论文
- 《Decision Procedures for Algebraic Data Types》（Barrett, Shankar）—— PVS 决策过程设计
- 《Integrating Decision Procedures in PVS》（Shankar）—— 决策过程与策略引擎集成
- 《Theory Interpretations in PVS》（Owre, Shankar）—— 参数化理论的权威参考
- 《A Ground Evaluator for PVS》（Shankar）—— 地面求值器设计
- Lv-00 相关文档：
  - `type_system.h` —— 类型系统、TypeRegion、精化类型
  - `solver.h` —— 约束求解器引擎
  - `proof.h` —— 证明导航器与验证结果类型
  - `fstar_refinement_smt.md` —— F* 精化类型与 SMT 混合验证参考

# Lv-00 参考落地设计文档：Catlab.jl/GATlab.jl 范畴论驱动编译框架

> **版本**: 1.0.0
> **日期**: 2026-05-24
> **参考**: Catlab.jl (github.com/AlgebraicJulia/Catlab.jl) —— Julia 应用范畴论框架，以及 GATlab.jl —— 提取的 GAT 核心编译器
> **目标**: 将 GAT（广义代数理论）的编译流水线、接线图（Wiring Diagram）的一等公民数据表示、幺半范畴的并行组合（张量积）映射到 Lv-00 的 .lvz 公理包、constraint_graph.h、func_block.h 和 Web GUI 构造面板

---

## 目录

1. [Catlab.jl/GATlab.jl 项目概述与 Lv-00 借鉴动机](#1-catlabjl-gatlabjl-项目概述与-lv-00-借鉴动机)
2. [核心借鉴要点一：GAT 编译流水线 —— 理论声明到可执行代码](#2-核心借鉴要点一gat-编译流水线--理论声明到可执行代码)
3. [核心借鉴要点二：接线图（Wiring Diagram）的几何约束图映射](#3-核心借鉴要点二接线图wiring-diagram的几何约束图映射)
4. [核心借鉴要点三：幺半范畴并行组合 —— 独立构造的张量积](#4-核心借鉴要点三幺半范畴并行组合--独立构造的张量积)
5. [核心借鉴要点四：Semagrams.jl 类型感知拖拽编辑器](#5-核心借鉴要点四semagramsjl-类型感知拖拽编辑器)
6. [GATlab.jl 编译器内核映射到 .lvz 编译流水线](#6-gatlabjl-编译器内核映射到-lvz-编译流水线)
7. [Lv-00 映射方案：cartesian_closed_category.lvz 公理包](#7-lv-00-映射方案cartesian_closed_categorylvz-公理包)
8. [Lv-00 映射方案：func_block.h 的嵌套构造增强](#8-lv-00-映射方案func_blockh-的嵌套构造增强)
9. [总结映射表](#9-总结映射表)

---

## 1. Catlab.jl/GATlab.jl 项目概述与 Lv-00 借鉴动机

### 1.1 Catlab.jl 是什么

Catlab.jl 是 AlgebraicJulia 生态的核心项目，它将应用范畴论（Applied Category Theory）的概念直接编译为可运行的 Julia 代码。其核心创新是 **GAT（Generalized Algebraic Theory，广义代数理论）**——一种声明式的范畴论编程语言，让用户可以在代码中直接定义数学理论（如范畴、幺半范畴、笛卡尔闭范畴），然后自动生成对应的类型系统和操作函数。

```
// Catlab.jl GAT 声明示例 —— 定义"范畴"理论
@theory Category{Ob, Hom} begin
  Ob::TYPE
  Hom(dom::Ob, codom::Ob)::TYPE

  id(A::Ob)::Hom(A, A)
  compose(f::Hom(A, B), g::Hom(B, C))::Hom(A, C)

  (compose(f, g) ⋅ h == f ⋅ compose(g, h)) ⊣ (A::Ob, B::Ob, C::Ob, D::Ob,
                                                f::(A → B), g::(B → C), h::(C → D))
  (compose(id(A), f) == f) ⊣ (A::Ob, B::Ob, f::(A → B))
  (compose(f, id(B)) == f) ⊣ (A::Ob, B::Ob, f::(A → B))
end
```

编译后自动生成：
- `Ob` 类型、`Hom` 参数化类型
- `id(A)`、`compose(f, g)` 函数定义
- 结合律和单位元的等式公理（可在 e-graph 中作为重写规则）

### 1.2 Lv-00 借鉴动机

Lv-00 的几何构造系统天然具有范畴结构——几何实体是**对象（Object）**，几何构造（中点、垂线、角平分线……）是**态射（Morphism）**，构造的组合对应态射的**复合（Compose）**。Catlab.jl 的 GAT 编译流水线和接线图表示与此高度一致：

| 借鉴方向 | Catlab/GATlab 特性 | Lv-00 现有基础 | 差距与目标 |
|----------|-------------------|---------------|-----------|
| **理论声明编译** | `@theory` 声明自动生成类型系统和函数 | .lvz 公理包手工定义 | 引入 `@theory` → .lvz 编译流水线 |
| **接线图表示** | WiringDiagram 作为一等公民数据结构 | ConstraintGraph 有向无环图 | 接线图的"框-端口-连线"模型直接映射到 FuncBlock |
| **类型化端口** | 每个接线端口有类型标签，自动类型检查 | `type_check_port_compatibility()` | 扩展为接线图级别的类型一致性验证 |
| **幺半范畴组合** | `otimes` (f⊗g) 张量积并行组合 | 无显式并行构造组合子 | 引入 `⊗` 操作符用于独立构造的并行组合 |
| **拖拽编辑器** | Semagrams.jl 类型感知的拖拽接线图编辑器 | Web GUI 构造面板 | 借鉴其类型感知端口连接机制 |
| **编译器分层** | GATlab.jl 分层编译（理论→IR→代码） | lvz_parser.c 单层解析 | 设计多阶段编译流水线 |

### 1.3 核心概念对照

```
Catlab.jl / GATlab.jl                Lv-00
─────────────────────────────────────────────────────
GAT @theory 声明                     .lvz 公理包中 @theory / @category 声明
Ob (Object)                          GeomNode（几何实体节点）
Hom(A, B) (Morphism)                 FuncBlock（几何构造）
compose(f, g)                        约束图中的有向边（构造 → 被构造）
id(A)                                恒等构造（节点自映射，pass-through）
WiringDiagram                        ConstraintGraph 的子图（构造组合图）
  Box (框)                           FuncBlock 节点
  Port (端口)                        FuncBlock 的 input_ports / output_ports
  Wire (连线)                        Constraint 边（INCIDENCE/CONNECTION 类型）
otimes (⊗, 张量积)                  并行构造组合子（独立构造的并列放置）
Semagrams.jl 拖拽编辑器              Web GUI 构造面板（port-type-aware 连接）
```

---

## 2. 核心借鉴要点一：GAT 编译流水线 —— 理论声明到可执行代码

### 2.1 GAT 的编译阶段

GATlab.jl 将 GAT 理论声明编译为可执行代码的流水线：

```
@theory Category{Ob, Hom} begin ... end
    │
    ▼
┌──────────────────────────────┐
│ Stage 1: Parse & Desugar     │  解析 GAT 语法，展开语法糖
│  - 识别 Ob::TYPE, Hom::TYPE │
│  - 识别函数声明 id/compose  │
│  - 识别公理等式             │
└────────────┬─────────────────┘
             │ GAT AST
             ▼
┌──────────────────────────────┐
│ Stage 2: Type Elaboration    │  类型推导与检查
│  - 推导每个函数的完整签章    │
│  - 检查等式的类型一致性      │
│  - 展开 ⊣ (context) 束缚     │
└────────────┬─────────────────┘
             │ Typed GAT
             ▼
┌──────────────────────────────┐
│ Stage 3: Code Generation     │  生成 Julia 代码
│  - 生成 Ob/Hom 参数化类型    │
│  - 生成 id/compose 函数实现  │
│  - 生成公理作为重写规则      │
└────────────┬─────────────────┘
             │ Julia source code
             ▼
┌──────────────────────────────┐
│ Stage 4: Julia Compilation   │  Julia 编译执行
│  - Julia 本体编译            │
│  - 生成的代码可被其他模块引用 │
└──────────────────────────────┘
```

### 2.2 映射到 Lv-00 的编译流水线

将 GAT 编译四阶段映射到 Lv-00 的 .lvz 编译流水线：

| GATlab.jl 阶段 | Lv-00 映射 | 具体实现 |
|---------------|-----------|---------|
| Parse & Desugar | `lvz_parser_parse_file()` 解析 `@theory` 声明 | `lvz_parser.c` |
| Type Elaboration | `type_infer_theory()` 类型推导和一致性检查 | `type_system.c` |
| Code Generation | `lvz_compile_to_rules()` 生成 RewriteRule + FuncBlock 注册 | `lvz_compiler.c`（新增） |
| Julia Compilation | Lv-00 引擎加载编译产物（规则表 + 类型表） | `engine_main.c` |

### 2.3 .lvz 中的 @theory 声明语法

借鉴 Catlab.jl 的 `@theory` 语法，为 .lvz 公理包设计范畴论风格的声明：

```
// ============================================================
// Lv-00 公理包: cartesian_closed_category.lvz
// ============================================================

@name "笛卡尔闭范畴几何公理"
@version "1.0.0"
@description "将几何构造建模为笛卡尔闭范畴中的态射"

// --- @theory 声明（借鉴 Catlab.jl GAT） ---
@theory CartesianClosedGeometry{GeomObj, GeomMorphism} begin

    // 对象类型声明
    GeomObj::TYPE

    // 态射类型声明 —— 几何构造
    GeomMorphism(dom::GeomObj, codom::GeomObj)::TYPE

    // 恒等态射：恒等构造（一个几何实体到自身的无操作映射）
    id(A::GeomObj)::GeomMorphism(A, A)

    // 复合：构造的组合
    compose(f::GeomMorphism(A, B), g::GeomMorphism(B, C))::GeomMorphism(A, C)

    // 积（Product）：两个几何对象的笛卡尔积
    // 在几何中对应：由两个独立点确定一条线段 pair → segment
    product(A::GeomObj, B::GeomObj)::GeomObj
    proj1(p::product(A, B))::GeomMorphism(product(A, B), A)
    proj2(p::product(A, B))::GeomMorphism(product(A, B), B)
    pair(f::GeomMorphism(X, A), g::GeomMorphism(X, B))::GeomMorphism(X, product(A, B))

    // 指数对象（Exponential）：在几何中对应函数空间
    // 例如：从两个 Point 构造一个 Line 的能力 → (Point, Point) → Line
    exponential(A::GeomObj, B::GeomObj)::GeomObj
    eval(e::exponential(A, B), a::A)::B
    curry(f::GeomMorphism(product(X, A), B))::GeomMorphism(X, exponential(A, B))

    // 终对象：在几何中对应退化的单点空间
    terminal()::GeomObj
    delete(A::GeomObj)::GeomMorphism(A, terminal())

    // === 公理（等式约束） ===

    // 结合律：构造的复合满足结合律
    (compose(compose(f, g), h) == compose(f, compose(g, h))) ⊣ (
        A::GeomObj, B::GeomObj, C::GeomObj, D::GeomObj,
        f::GeomMorphism(A, B), g::GeomMorphism(B, C), h::GeomMorphism(C, D)
    )

    // 单位律：与恒等构造复合不变
    (compose(id(A), f) == f) ⊣ (A::GeomObj, B::GeomObj, f::GeomMorphism(A, B))
    (compose(f, id(B)) == f) ⊣ (A::GeomObj, B::GeomObj, f::GeomMorphism(A, B))

    // 积的泛性质
    (compose(pair(f, g), proj1) == f) ⊣ (
        X::GeomObj, A::GeomObj, B::GeomObj,
        f::GeomMorphism(X, A), g::GeomMorphism(X, B)
    )
    (compose(pair(f, g), proj2) == g) ⊣ (
        X::GeomObj, A::GeomObj, B::GeomObj,
        f::GeomMorphism(X, A), g::GeomMorphism(X, B)
    )

end // @theory CartesianClosedGeometry
```

### 2.4 编译产物：自动生成的类型表和规则表

`@theory` 声明经编译后自动生成以下 Lv-00 内部结构：

| 编译产物 | 来源 | Lv-00 结构 |
|---------|------|-----------|
| `GeomObj` 类型参数 | `GeomObj::TYPE` | `GeomType` 枚举的根类型 |
| `GeomMorphism(A,B)` 类型 | `GeomMorphism(dom, codom)::TYPE` | `FuncBlock` 注册（带输入/输出端口类型） |
| `id(A)` 函数 | `id(A::GeomObj)::GeomMorphism(A, A)` | `FuncBlock`：恒等构造，1入1出 |
| `compose(f, g)` 函数 | `compose(f, g)::GeomMorphism(A, C)` | `FuncBlock`：复合构造，2入1出 |
| `product(A, B)` 函数 | `product(A::GeomObj, B::GeomObj)` | `FuncBlock`：积构造器 |
| 结合律公理 | `compose(compose(f,g),h) == compose(f,compose(g,h))` | `RewriteRule`：构造复合的结合律重写 |
| 单位律公理 | `compose(id(A),f) == f` | `RewriteRule`：恒等构造的消去规则 |
| 积的泛性质 | `compose(pair(f,g), proj1) == f` | `RewriteRule`：投影+配对的重写 |

---

## 3. 核心借鉴要点二：接线图（Wiring Diagram）的几何约束图映射

### 3.1 Catlab.jl 的 Wiring Diagram

接线图是 Catlab.jl 的一等公民数据结构。它使用超图表示——框（Box）有类型化的端口（Port），端口之间通过连线（Wire）连接：

```
Catlab.jl WiringDiagram:

  ┌─────────┐     ┌─────────┐
  │  f: A→B │────▶│  g: B→C │────▶ out
  └─────────┘     └─────────┘

  Box "f":  输入端口 [A], 输出端口 [B]
  Box "g":  输入端口 [B], 输出端口 [C]
  Wire:     f.output[1] → g.input[1]
  整图类型: A → C (compose(f, g) 的接线图表示)
```

### 3.2 接线图的 Lv-00 约束图映射

Catlab.jl 的接线图天然对应 Lv-00 的 constraint_graph 结构：

| WiringDiagram 概念 | Catlab 数据结构 | Lv-00 映射 | 文件 |
|-------------------|----------------|-----------|------|
| Box（框） | `Box{value, input_ports, output_ports}` | `FuncBlock` 节点 | `func_block.h` |
| Port（端口） | `PortData{port_type}` | `FuncBlockPort`（输入/输出端口，带类型标签） | `func_block.h` |
| Wire（连线） | `Wire{src: (box_id, port_id), tgt: (box_id, port_id)}` | `Constraint` 边（CONNECTION 类型） | `constraint_graph.h` |
| Outer Port | 接线图的外部接口 | `FuncBlock` 的顶层输入/输出端口 | `func_block.h` |
| 嵌套接线图 | 框内嵌套子接线图 | 子 `ConstraintGraph`（递归引用） | `constraint_graph.h` |
| 接线图复合 | `compose(d1, d2)` | 约束图的图拼接操作 | `constraint_graph.h` |

### 3.3 约束图作为接线图的数据结构增强

```c
/**
 * @brief 接线图端口定义 —— 借鉴 Catlab.jl 的 PortData
 *
 * 每个 FuncBlock 的端口带有类型标签，
 * 连接时自动验证类型兼容性。
 */
typedef struct {
    int port_id;                /**< 端口在 FuncBlock 中的索引 */
    char *port_name;            /**< 端口名称（如 "src", "tgt", "center"） */
    int port_type;              /**< 端口类型（GeomType 枚举值） */
    bool is_required;           /**< 是否为必需端口（vs 可选端口） */
    bool is_vararg;             /**< 是否为可变参数端口 */
} FuncBlockPort;

/**
 * @brief 接线图 —— FuncBlock 的连接拓扑
 *
 * 借鉴 Catlab.jl 的 WiringDiagram 数据结构。
 * 将多个 FuncBlock 通过类型化端口连接为一个复合构造。
 *
 * Catlab 等价:
 *   wd = WiringDiagram([:A, :B], [:C])  # 2输入1输出
 *   f_box = add_box!(wd, Box(:midpoint, [:A], [:B], [:M]))
 *   g_box = add_box!(wd, Box(:distance, [:M], [:C]))
 *   add_wires!(wd, [
 *     (input_id(wd), 1) => (f_box, 1),   # A → f.A
 *     (input_id(wd), 2) => (f_box, 2),   # B → f.B
 *     (f_box, 1) => (g_box, 1),           # f.M → distance.M
 *     (g_box, 1) => (output_id(wd), 1)    # distance.C → out
 *   ])
 */
typedef struct {
    int wiring_diagram_id;              /**< 接线图 ID */
    char *name;                         /**< 接线图名称 */

    /* 外部端口 */
    FuncBlockPort *input_ports;         /**< 输入端口数组 */
    int input_port_count;
    FuncBlockPort *output_ports;        /**< 输出端口数组 */
    int output_port_count;

    /* 内部框 */
    int *box_node_ids;                  /**< 内部 FuncBlock/GeomNode 的节点 ID */
    int box_count;

    /* 连线 */
    struct {
        int src_box_id;                 /**< 源框 ID（或 -1 表示外输入端口） */
        int src_port_id;                /**< 源端口 ID */
        int tgt_box_id;                 /**< 目标框 ID（或 -1 表示外输出端口） */
        int tgt_port_id;                /**< 目标端口 ID */
    } *wires;
    int wire_count;

    /* 类型信息 */
    int overall_input_type;             /**< 整个接线图的输入类型 */
    int overall_output_type;            /**< 整个接线图的输出类型 */
} WiringDiagram;
```

### 3.4 接线图的复合操作

```c
/**
 * @brief 接线图复合 —— 借鉴 Catlab.jl 的 compose
 *
 * 将两个接线图串联：d1 的输出通过中间线连接到 d2 的输入。
 * 复合后形成新的接线图，其输入为 d1 的输入，输出为 d2 的输出。
 *
 * Catlab 等价: compose(d1, d2)
 *
 * Lv-00 几何示例:
 *   d1 = WiringDiagram("find_midpoint", [A, B], [M])
 *        // 内部: midpoint_func(A, B) → M
 *   d2 = WiringDiagram("find_distance", [M], [dist])
 *        // 内部: distance_func(M) → dist
 *   d3 = wiring_diagram_compose(d1, d2)
 *        // d3: [A, B] → [dist]  复合构造
 */
WiringDiagram *wiring_diagram_compose(
    const WiringDiagram *d1,
    const WiringDiagram *d2);

/**
 * @brief 接线图张量积 —— 借鉴 Catlab.jl 的 otimes (⊗)
 *
 * 将两个独立的接线图并行放置：
 * d1: [A] → [B], d2: [C] → [D]
 * d1 ⊗ d2: [A, C] → [B, D]
 *
 * 在几何中对应：两个独立几何构造的并行组合。
 */
WiringDiagram *wiring_diagram_otimes(
    const WiringDiagram *d1,
    const WiringDiagram *d2);
```

---

## 4. 核心借鉴要点三：幺半范畴并行组合 —— 独立构造的张量积

### 4.1 幺半范畴的并行组合 (⊗)

在 Catlab.jl 中，`otimes`（张量积，符号 ⊗）表示两个独立态射的并行组合：

```
// Catlab.jl 幺半范畴并行组合
f: A → B    (构造 f)
g: C → D    (构造 g)
f ⊗ g: (A ⊗ C) → (B ⊗ D)   // 并行：f 和 g 互不依赖

// 在几何中对应：
//   构造1: midpoint(A, B) → M    (计算 AB 的中点)
//   构造2: midpoint(C, D) → N    (计算 CD 的中点)
//   并行: midpoint(A,B) ⊗ midpoint(C,D)
//         → 两个中点计算可以并行执行，互不影响
```

### 4.2 几何构造中的并行组合实例

在几何证明中，许多构造天然可并行：

| 场景 | 独立构造 | 并行组合表示 |
|------|---------|-------------|
| 三角形三边中点 | D = midpoint(B,C), E = midpoint(C,A), F = midpoint(A,B) | `⊗(midpoint_BC, midpoint_CA, midpoint_AB)` |
| 三角形三条高线 | 从每个顶点向对边作垂线 | `⊗(altitude_A, altitude_B, altitude_C)` |
| 多边形对角线 | AC, AD, BD, ... | `⊗(segment(A,C), segment(A,D), segment(B,D), ...)` |
| 辅助线构造 | 在三角形外部作等边三角形 | `equilateral(A,B) ⊗ equilateral(B,C) ⊗ equilateral(C,A)` |

### 4.3 Lv-00 中的 ⊗ 操作符设计

```c
/**
 * @brief 并行构造组合子 —— 借鉴 Catlab.jl 的 otimes (⊗)
 *
 * 在约束图中创建多个独立的 FuncBlock 子图，
 * 它们共享部分输入但输出互不依赖。
 *
 * 语义:
 *   给定构造 f: A₁→B₁ 和 g: A₂→B₂，
 *   parallel(f, g) 在图中同时创建 f 和 g 的实例，
 *   它们之间没有 CONSTRAINT 边连接（互不依赖）。
 *
 * 与 compose（串联）的区别:
 *   - compose: f 的输出是 g 的输入（依赖关系）
 *   - parallel: f 和 g 无依赖关系，可并行求解
 *
 * Catlab 等价: otimes(f, g)
 */
typedef struct {
    FuncBlock **constructions;          /**< 并行构造数组 */
    int construction_count;
    bool *is_independent;               /**< 各构造是否被验证为独立 */
} ParallelConstruction;

/**
 * @brief 创建并行构造
 *
 * 将多个 FuncBlock 作为一组独立构造在约束图中实例化。
 * 系统自动验证它们之间的依赖关系：
 *   - 如果某构造的输出是另一构造的输入 → 标记为非独立，降级为顺序执行
 *   - 如果所有构造互不依赖 → 标记为 independent，允许并行求解
 *
 * @return 并行构造句柄
 */
ParallelConstruction *parallel_construction_create(
    ConstraintGraph *graph,
    FuncBlock **constructions,
    int count,
    int **input_bindings,       /**< [count][arity] 每个构造的输入绑定 */
    int *arities);
```

### 4.4 .lvz 中的并行构造声明

```
// .lvz 公理包中的并行构造声明
// 借鉴 Catlab.jl 的 otimes 语法

// 并行声明三边中点（三个 midpoint 互不依赖）
@parallel [
    [midpoint] D = midpoint(B, C),
    [midpoint] E = midpoint(C, A),
    [midpoint] F = midpoint(A, B)
]

// 编译后：构造三个独立的 midpoint FuncBlock 实例
// 它们之间无依赖边，求解器可并行评估

// 带类型注解的并行构造
@parallel [
    [midpoint: Point×Point→Point] M_AB = midpoint(A, B),
    [midpoint: Point×Point→Point] M_BC = midpoint(B, C),
    [segment: Point×Point→Segment] s_AB = segment(A, B)
]
```

---

## 5. 核心借鉴要点四：Semagrams.jl 类型感知拖拽编辑器

### 5.1 Semagrams.jl 的设计理念

Semagrams.jl 是 Catlab.jl 生态中的可视化编辑器，允许用户通过拖拽方式构建接线图。其核心特性是**类型感知的端口连接**——当用户拖动一个端口靠近另一个端口时，系统自动检查端口类型是否兼容，兼容则高亮允许连接，不兼容则显示禁止符号。

```
Semagrams.jl 交互模型:
  1. 从调色板拖拽一个 Box 到画布上
  2. Box 自动显示输入/输出端口（带类型标签）
  3. 用户拖动一个输出端口到另一个 Box 的输入端口
  4. 系统检查类型兼容性：
     - 输出类型 = 输入类型 → 绿色高亮，允许连接
     - 输出类型 ≠ 输入类型 → 红色禁止符号
     - 输出类型 <: 输入类型（子类型） → 橙色高亮，允许连接但需coercion
  5. 连接后，接线图自动更新
```

### 5.2 Lv-00 Web GUI 的类型感知端口连接

将 Semagrams.jl 的交互模型映射到 Lv-00 的 Web GUI 构造面板：

| Semagrams.jl 概念 | Lv-00 Web GUI 映射 | 实现技术 |
|------------------|-------------------|---------|
| 调色板（Palette） | 左侧工具栏：按 Category 分组的构造器列表 | React 组件 `ConstructionPalette` |
| Box 拖拽 | 从工具栏拖拽 FuncBlock 到画布 | HTML5 Drag & Drop API |
| 端口显示 | FuncBlock 渲染时自动绘制输入/输出端口圆点 | Canvas API / SVG |
| 类型感知高亮 | 接近时端口颜色变化：绿（兼容）/ 红（不兼容）/ 橙（需coercion） | `type_check_port_compatibility()` |
| 连线 | 鼠标拖拽绘制贝塞尔曲线，连接两个端口 | SVG path + d3-force |
| 约束验证 | 连接后自动运行 `constraint_validate()` 检查一致性 | WebSocket → engine |
| 嵌套构造 | 选中的接线图可以折叠为一个复合 FuncBlock | `func_block_pack_wiring_diagram()` |

### 5.3 端口类型兼容性检查（Web GUI 端）

```javascript
/**
 * @brief 端口类型兼容性检查 —— 借鉴 Semagrams.jl 的类型感知连接
 *
 * 在前端执行快速类型检查，提供即时视觉反馈。
 * 复杂检查（如范畴成员资格）回退到后端引擎。
 *
 * @param {number} srcPortType  - 源端口类型（GeomType 枚举）
 * @param {number} tgtPortType  - 目标端口类型（GeomType 枚举）
 * @returns {object} { compatible: bool, color: string, message: string }
 */
function checkPortCompatibility(srcPortType, tgtPortType) {
    if (srcPortType === tgtPortType) {
        return { compatible: true,  color: '#4CAF50',
                 message: '类型完全匹配' };
    }
    if (isSubtype(srcPortType, tgtPortType)) {
        return { compatible: true,  color: '#FF9800',
                 message: '子类型匹配（将自动升级）' };
    }
    if (hasCommonSupertype(srcPortType, tgtPortType)) {
        return { compatible: true,  color: '#2196F3',
                 message: '共同超类型匹配' };
    }
    return { compatible: false, color: '#F44336',
             message: '类型不兼容' };
}
```

---

## 6. GATlab.jl 编译器内核映射到 .lvz 编译流水线

### 6.1 GATlab.jl 的分层架构

GATlab.jl 将 GAT 编译器从 Catlab.jl 中提取为独立包，形成清晰的分层架构：

```
GATlab.jl 架构                     Lv-00 对应
─────────────────────────────────────────────────
gatheory/  (理论声明)              .lvz 文件中的 @theory 声明
  └─ Theory, TypeInCtx, ...       └─ lvz_parser.c 解析产物
gatsyntax/  (语法层)               .lvz BNF 文法
  └─ @theory DSL syntax            └─ lvz_grammar.bnf
gatmodel/  (语义模型)              .lvz → 内部 IR
  └─ 类型推导、等式检查            └─ type_system.c 类型推导
gatcompile/  (代码生成)            编译 IR → RewriteRule + FuncBlock
  └─ 生成 Julia 代码               └─ lvz_compiler.c（新增）
```

### 6.2 Lv-00 多阶段编译流水线设计

```
.lvz 公理包文件
    │
    ▼
┌───────────────────────────────┐
│ Stage 1: 解析                 │ lvz_parser.c
│  @theory, @sort, @op,         │ → LvzAST (抽象语法树)
│  @rewrite, @parallel          │
└──────────────┬────────────────┘
               │
               ▼
┌───────────────────────────────┐
│ Stage 2: 类型推导             │ type_system.c
│  - 每个 Ob 的类型推导          │ → TypedLvzAST
│  - 每个 Hom 参数化类型展开     │
│  - 公理等式的类型一致性检查     │
└──────────────┬────────────────┘
               │
               ▼
┌───────────────────────────────┐
│ Stage 3: 语义分析             │ lvz_semantic.c (新增)
│  - 接线图的框-端口-连线分析   │ → LvzIR (中间表示)
│  - 并行/串联依赖分析           │
│  - 构造器的 Category 成员验证  │
└──────────────┬────────────────┘
               │
               ▼
┌───────────────────────────────┐
│ Stage 4: 代码生成             │ lvz_compiler.c (新增)
│  - IR → RewriteRule[]         │ → RewriteRule 数组
│  - IR → FuncBlock[]           │ → FuncBlock 注册表
│  - IR → ConstraintPropagationRule[] │ → 约束传播规则
│  - IR → TypeRegion[]          │ → 类型区域更新
└──────────────┬────────────────┘
               │
               ▼
┌───────────────────────────────┐
│ Stage 5: 引擎加载              │ engine_main.c
│  - 注册所有生成的结构          │ → Lv-00 引擎运行时可使用
│  - 索引和验证                  │
└───────────────────────────────┘
```

---

## 7. Lv-00 映射方案：cartesian_closed_category.lvz 公理包

### 7.1 公理包设计目标

`cartesian_closed_category.lvz` 是实现笛卡尔闭范畴（CCC）几何理论的 .lvz 公理包。它将几何构造统一建模为 CCC 中的态射，使得范畴论的组合算子（compose、⊗、curry）可直接用于几何构造。

```
// ============================================================
// Lv-00 公理包: cartesian_closed_category.lvz
// 完整版（含实例化声明）
// ============================================================

@name "笛卡尔闭范畴几何公理包"
@version "1.0.0"
@description "将几何构造建模为笛卡尔闭范畴（CCC），支持 compose/⊗/curry 组合"

// ========== 1. CCC 理论声明 ==========
@theory CartesianClosedGeometry{GeomObj, GeomMorphism} begin
    GeomObj::TYPE
    GeomMorphism(dom::GeomObj, codom::GeomObj)::TYPE

    id(A::GeomObj)::GeomMorphism(A, A)
    compose(f::GeomMorphism(A,B), g::GeomMorphism(B,C))::GeomMorphism(A,C)
    product(A::GeomObj, B::GeomObj)::GeomObj
    proj1(p::product(A,B))::GeomMorphism(product(A,B), A)
    proj2(p::product(A,B))::GeomMorphism(product(A,B), B)
    pair(f::GeomMorphism(X,A), g::GeomMorphism(X,B))::GeomMorphism(X, product(A,B))
    exponential(A::GeomObj, B::GeomObj)::GeomObj
    eval(e::exponential(A,B), a::A)::B
    curry(f::GeomMorphism(product(X,A), B))::GeomMorphism(X, exponential(A,B))
    terminal()::GeomObj
    delete(A::GeomObj)::GeomMorphism(A, terminal())

    // 公理（等式规则）
    (compose(compose(f,g), h) == compose(f, compose(g,h))) ⊣ (A,B,C,D,f,g,h)
    (compose(id(A), f) == f) ⊣ (A,B,f)
    (compose(f, id(B)) == f) ⊣ (A,B,f)
    (compose(pair(f,g), proj1) == f) ⊣ (X,A,B,f,g)
    (compose(pair(f,g), proj2) == g) ⊣ (X,A,B,f,g)
end

// ========== 2. 几何对象的具体类型声明 ==========
@domain Point     implements GeomObj
@domain Segment   implements GeomObj
@domain Triangle  implements GeomObj
@domain Circle    implements GeomObj
@domain Number    implements GeomObj

// ========== 3. 几何构造器（作为态射） ==========
@op midpoint   : GeomMorphism(product(Point, Point), Point)    [ctor, congruence]
@op centroid   : GeomMorphism(product(Point, product(Point, Point)), Point)  [ctor, congruence]
@op segment    : GeomMorphism(product(Point, Point), Segment)  [ctor, congruence]
@op circumcenter : GeomMorphism(product(Point, product(Point, Point)), Point) [ctor, congruence]
@op perpendicular : GeomMorphism(product(Point, Line), Line)   [ctor]
@op angle_bisector : GeomMorphism(product(Line, Line), Line)   [ctor]

// ========== 4. 态射的复合示例（预定义复合构造） ==========
// mid_to_dist: 先求中点，再求距离
@compose mid_to_dist = compose(midpoint, distance)
// 类型: GeomMorphism(product(Point, Point), Number)
// 等效于: P = midpoint(A, B); d = distance(P, C)
```

### 7.2 公理包加载后的效果

加载 `cartesian_closed_category.lvz` 后，Lv-00 引擎获得以下能力：

| 能力 | 来源 | 效果 |
|------|------|------|
| compose(f, g) 组合构造 | `@theory` 中的 compose | 两个几何构造可串联为一个复合构造 |
| product(A, B) + pair(f, g) | `@theory` 中的积结构 | 多个参数可打包为一个积对象传入构造 |
| otimes 并行组合 | `@parallel` 声明 + 幺半范畴结构 | 独立几何构造可并行实例化 |
| 公理重写规则 | `@theory` 中的等式公理 | compose 的结合律等自动作为重写规则可用 |
| 类型检查 | GeomMorphism 参数化类型 | 构造连接时自动检查端口类型兼容性 |

---

## 8. Lv-00 映射方案：func_block.h 的嵌套构造增强

### 8.1 当前 func_block.h 的局限

当前 `func_block.h` 将 FuncBlock 视为**原子节点**——输入端口绑定具体值，输出端口产生单个值。但范畴论视角下的几何构造应该是**可嵌套的**——一个构造的输出可以是另一个构造的输入，从而形成任意深度的构造树。

### 8.2 嵌套 FuncBlock 设计

```c
/**
 * @brief 嵌套构造块 —— 借鉴 Catlab.jl 的 WiringDiagram 嵌套
 *
 * 一个 FuncBlock 可以包含一个内部的 ConstraintGraph（子接线图）。
 * 当 FuncBlock 被"调用"时，其内部约束图被实例化到外层图中。
 *
 * 这使得几何构造可以任意嵌套：
 *   outer_block = compose(
 *     midpoint(A, B),          // 子块 1
 *     compose(
 *       segment(M, C),         // 子块 2a
 *       perpendicular(D, s_MC) // 子块 2b
 *     )
 *   )
 *   // outer_block 是一个三层嵌套的 FuncBlock
 */
typedef struct FuncBlockNested {
    /* 继承 FuncBlock 基础字段 */
    FuncBlock base;

    /* 嵌套接线图 */
    ConstraintGraph *internal_graph;       /**< 内部约束图（子接线图） */

    /* 内-外端口映射 */
    int *inner_to_outer_input;             /**< 内部输入端口 → 外部输入端口映射 */
    int *inner_to_outer_output;            /**< 内部输出端口 → 外部输出端口映射 */
    int inner_port_count;

    /* 实例化计数 */
    int instantiation_count;               /**< 已被实例化的次数 */
    ConstraintGraph **instantiated_graphs; /**< 各次实例化产生的约束图快照 */

    /* 折叠/展开状态 */
    bool is_collapsed;                     /**< GUI 中是否折叠显示 */
} FuncBlockNested;

/**
 * @brief 将嵌套 FuncBlock 展开为约束图中的子图
 *
 * 借鉴 Catlab.jl 的 WiringDiagram 实例化（ocompose）。
 * 将内部约束图的节点和约束复制到外部图中，
 * 并根据内-外端口映射连接输入/输出。
 *
 * @param[in]     nested  嵌套 FuncBlock
 * @param[in]     outer_input_bindings 外部输入端口的绑定节点 ID
 * @param[in,out] outer_graph 外部约束图
 * @param[out]    outer_output_ids 展开后的外部输出节点 ID
 * @return 是否展开成功
 */
bool func_block_nested_expand(
    const FuncBlockNested *nested,
    const int *outer_input_bindings,
    ConstraintGraph *outer_graph,
    int **outer_output_ids);
```

### 8.3 FuncBlock 的类型化端口注册

```c
/**
 * @brief 注册类型化的 FuncBlock 端口 —— 借鉴 Catlab.jl 的 PortData
 *
 * 每个 FuncBlock 的每个端口都有类型标签。
 * 构造器的端口类型来自其参数和返回类型。
 */
typedef struct {
    int func_block_id;
    char *func_name;

    /* 输入端口 */
    struct {
        int port_id;
        char *port_label;           /**< 如 "A", "B", "center" */
        GeomType required_type;     /**< 端口要求的最低类型 */
        int required_category_id;   /**< （可选）端口要求的 Category */
    } *input_ports;
    int input_port_count;

    /* 输出端口 */
    struct {
        int port_id;
        char *port_label;
        GeomType output_type;       /**< 端口输出的类型 */
        int output_category_id;     /**< （可选）输出所属的 Category */
    } *output_ports;
    int output_port_count;

    /* 嵌套/展开 */
    int nesting_level;              /**< 0=原子构造, >0=嵌套层数 */
    FuncBlockNested *nested_block;  /**< 嵌套实现的引用 */
} FuncBlockTypeInfo;
```

---

## 9. 总结映射表

### 9.1 Catlab.jl/GATlab.jl → Lv-00 核心概念映射

| Catlab.jl / GATlab.jl 概念 | Catlab 内部 | Lv-00 映射 | 文件 |
|---------------------------|-----------|-----------|------|
| `@theory` 声明 | `Theory` 结构体（sort/operation/axiom） | `.lvz` 文件中的 `@theory` 块 | `lvz_parser.c` |
| `Ob::TYPE` | 对象类型声明 | `@sort` → `GeomType` 枚举 | `type_system.h` |
| `Hom(dom, codom)::TYPE` | 态射类型声明 | `@op` → `FuncBlock`（带输入/输出端口类型） | `func_block.h` |
| `compose(f, g)` | 态射复合 | 约束图中有向边的链（构造串联） | `constraint_graph.h` |
| `id(A)` | 恒等态射 | 恒等构造 FuncBlock（pass-through） | `func_block.h` |
| `otimes(f, g)` (⊗) | 幺半范畴并行组合 | `@parallel` → 独立构造的并行实例化 | `func_block.h` |
| `WiringDiagram` | 框-端口-连线超图 | `constraint_graph.h` 的子图 + `FuncBlockPort` | `constraint_graph.h` |
| `Box` (框) | `Box{value, input_ports, output_ports}` | `FuncBlock` 节点 | `func_block.h` |
| `Port` (端口) | `PortData{port_type}` | `FuncBlockPort`（带类型标签） | `func_block.h` |
| `Wire` (连线) | `Wire{src, tgt}` | `Constraint` 边（CONNECTION 类型） | `constraint_graph.h` |
| Semagrams.jl 拖拽编辑器 | 类型感知端口连接 | Web GUI 构造面板 + `checkPortCompatibility()` | `frontend/` |
| GATlab.jl 编译流水线 | parse → type elaborate → codegen | `lvz_parser.c` → `type_system.c` → `lvz_compiler.c` | 编译流水线 |

### 9.2 几何构造的范畴论映射速查

| 几何构造 | 范畴论表示 | compose/otimes 表示 |
|---------|-----------|-------------------|
| `midpoint(A, B) → M` | `midpoint: Point ⊗ Point → Point` | `midpoint ∘ pair(A, B)` |
| `segment(P, Q) → s` | `segment: Point ⊗ Point → Segment` | 原子构造 |
| `perpendicular(P, l) → m` | `perpendicular: Point ⊗ Line → Line` | 原子构造 |
| `centroid(A, B, C) → G` | `centroid: Point ⊗ Point ⊗ Point → Point` | `centroid ∘ (A ⊗ B ⊗ C)` |
| `distance(M, C) → d` | `distance: Point ⊗ Point → Number` | `distance ∘ pair(midpoint(A,B), C)` |
| 三边中点并行 | `midpoint ⊗ midpoint ⊗ midpoint` | `(midpoint ∘ pair(B,C)) ⊗ (midpoint ∘ pair(C,A)) ⊗ (midpoint ∘ pair(A,B))` |

### 9.3 文件依赖关系

```
.lvz 公理包文件
    │
    ├── lvz_parser.c              (扩展：@theory / @compose / @parallel 声明解析)
    │   └── lvz_grammar.bnf       (BNF 文法扩展)
    │
    ├── type_system.c             (扩展：GAT 理论的类型推导)
    │   └── type_system.h         (TypeInCtx, sort/operation/axiom 类型推导)
    │
    ├── lvz_semantic.c            (新增：接线图语义分析)
    │   ├── constraint_graph.h    (WiringDiagram → ConstraintGraph 转换)
    │   └── func_block.h          (端口类型标注)
    │
    ├── lvz_compiler.c            (新增：GAT IR → 可执行结构代码生成)
    │   ├── rewrite.h             (公理等式 → RewriteRule)
    │   ├── func_block.h          (操作声明 → FuncBlock 注册)
    │   └── constraint_graph.h    (接线图 → ConstraintGraph)
    │
    ├── func_block.h              (扩展：嵌套构造、端口类型、⊗ 并行)
    │   ├── FuncBlockNested       (嵌套接线图构造块)
    │   ├── FuncBlockTypeInfo     (类型化端口注册)
    │   └── ParallelConstruction  (⊗ 并行组合)
    │
    └── frontend/                 (扩展：类型感知拖拽编辑器)
        ├── ConstructionPalette   (按 Category 分组的构造器)
        └── PortConnector         (类型感知端口连接)
```

---

## 附录 A：Catlab.jl 经典示例与 Lv-00 对照

```
// Catlab.jl 示例：构造并复合接线图
using Catlab.WiringDiagrams

# 定义两个构造：加法和乘法
A = Box(:add, [:x, :y], [:z])
B = Box(:mul, [:a, :b], [:c])

# 创建接线图
wd = WiringDiagram([:in1, :in2], [:out])
add_box!(wd, A)
add_box!(wd, B)
add_wires!(wd, [
    (input_id(wd), 1) => (A, 1),
    (input_id(wd), 2) => (A, 2),
    (A, 1) => (B, 1),
    (B, 1) => (output_id(wd), 1)
])


// Lv-00 等价（.lvz 公理包中）：
@op add : GeomMorphism(product(Number, Number), Number)
@op mul : GeomMorphism(product(Number, Number), Number)

@compose add_then_mul = compose(
    pair(add, id(Number)),
    compose(pair(proj1, id(Number)), mul)
)
// 等效于: y = mul(add(x1, x2), x3)
```

---

## 附录 B：范畴组合子速查

| 组合子 | Catlab 符号 | Lv-00 语法 | 几何含义 |
|-------|-----------|-----------|---------|
| 复合 | `compose(f, g)` 或 `f ⋅ g` | `@compose name = compose(f, g)` | 先做构造 f，再做构造 g（串联） |
| 张量积 | `otimes(f, g)` 或 `f ⊗ g` | `@parallel [f, g]` | 两个独立构造并行放置 |
| 配对 | `pair(f, g)` | `pair(f, g)` | 将两个构造打包为积类型的输入 |
| 投影 | `proj1(p)`, `proj2(p)` | `proj1(product)`, `proj2(product)` | 从积中提取分量 |
| 恒等 | `id(A)` | `id(A)` | 恒等构造（不改变几何实体） |
| Curry化 | `curry(f)` | `curry(f)` | 将多参数构造转换为高阶构造 |
| 求值 | `eval(e, a)` | `eval(exponential, arg)` | 应用高阶构造到参数 |

---

> **文档结束**
> 本文档详述了 Catlab.jl/GATlab.jl 的 GAT 编译流水线、接线图数据表示、幺半范畴并行组合和 Semagrams.jl 拖拽编辑器如何映射到 Lv-00 体系。核心结论：GAT 声明式理论直接对应 .lvz 公理包的 `@theory` 编译流水线；接线图的"框-端口-连线"模型天然映射到 constraint_graph.h 的 FuncBlock + Constraint 结构；幺半范畴的并行组合 `⊗` 使得独立几何构造可被显式标记为无依赖关系并并行求解；Semagrams.jl 的类型感知端口连接为 Web GUI 构造面板提供了直观的交互参考。这四项整合使 Lv-00 的几何构造系统从"命令式代码注册"升级为"声明式范畴理论驱动的编译型架构"。

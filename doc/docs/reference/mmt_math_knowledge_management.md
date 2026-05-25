# MMT 数学知识管理框架参考文档

> 版本：1.0 | 日期：2026-05-24 | 语言：zh-CN
>
> 面向读者：Lv-00 项目开发者、形式化数学研究者、公理包系统设计者

---

## 目录

1. [项目概述](#1-项目概述)
2. [核心借鉴点](#2-核心借鉴点)
3. [Lv-00 映射方案](#3-lv-00-映射方案)
4. [实现路线图](#4-实现路线图)
5. [附录](#5-附录)

---

## 1. 项目概述

### 1.1 简介

MMT（Meta Meta Tool）是一个用于数学知识形式化表示与互操作的通用框架，由 [UniFormal/MMT](https://github.com/UniFormal/MMT) 项目维护。其核心理念是：**不同的数学理论、逻辑系统、形式化工具之间不应各自为政，而应在一个统一的元层次上进行表示和互译**。

MMT 的设计哲学可以用一句话概括：**"一切皆为理论，一切皆为态射"**。它将数学理论本身也视为一种形式化对象，允许在理论之间定义态射（morphism），从而建立起一个可互操作的数学知识网络。

### 1.2 技术栈

| 层次 | 技术选择 | 说明 |
|------|----------|------|
| **核心引擎** | Scala（JVM） | LF 类型检查、theory morphism 计算、视图合成等核心逻辑 |
| **API 层** | HTTP REST API | 通过标准的 REST 接口暴露理论查询、类型检查、态射推导等服务 |
| **存储后端** | MMT Archive 格式 | 基于 JSON + OMDoc 的数学文档归档格式，支持版本化与依赖管理 |
| **构建工具** | sbt（Scala Build Tool） | 标准的 Scala 项目构建与依赖管理 |
| **前端门户** | MathHub.info | 基于 Web 的数学知识门户，聚合数十种形式化数学库 |

### 1.3 许可证

MMT 采用宽松的 MIT-like 许可证，允许自由使用、修改和再分发。这与 Lv-00 项目的开源策略保持一致。

### 1.4 MMT 在数学形式化生态中的位置

```
┌─────────────────────────────────────────────────────┐
│                    MathHub 门户                       │
│   ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐     │
│   │ Mizar │ │Isabelle│ │ Coq  │ │ PVS  │ │ ...  │     │
│   └──┬───┘ └──┬───┘ └──┬───┘ └──┬───┘ └──┬───┘     │
│      │        │        │        │        │           │
│      └────────┴────────┴────────┴────────┘           │
│                       │                              │
│              ┌────────┴────────┐                     │
│              │   MMT 元框架    │ ← LF 层             │
│              │  (theory morphism)│                   │
│              └────────┬────────┘                     │
│                       │                              │
│          ┌────────────┼────────────┐                 │
│          │            │            │                 │
│     ┌────┴────┐  ┌────┴────┐  ┌────┴────┐           │
│     │ Archive │  │ Archive │  │ Archive │           │
│     │  (A)    │  │  (B)    │  │  (C)    │           │
│     └─────────┘  └─────────┘  └─────────┘           │
└─────────────────────────────────────────────────────┘
```

---

## 2. 核心借鉴点

MMT 的设计中有五个核心概念对 Lv-00 的公理包系统和"公理中立"设计具有直接的借鉴意义。以下逐项展开。

### 2.1 理论态射（Theory Morphism）

**MMT 定义**：理论态射是从源理论到目标理论的一类映射，该映射将源理论中的符号、公理和定理系统地翻译为目标理论中的对应元素。形式化地，

```
morphism φ: S → T
  symbol₁  ↦ expression₁
  symbol₂  ↦ expression₂
  axiom₁   ↦ theorem₁   (在 T 中可证)
```

**关键性质**：如果某个定理在源理论 S 中可证（即 S ⊢ P），那么经过态射 φ 翻译后，在目标理论 T 中 φ(P) 也必然可证（即 T ⊢ φ(P)）。这一性质被称为 **"定理沿态射的保持性"**（truth preservation along morphisms）。

**Lv-00 借鉴意义**：这是 Lv-00"公理中立"设计的形式化理论基础。Lv-00 的核心理念是：推理引擎不应内建任何特定逻辑，而应通过公理包来定义逻辑。理论态射为"同一个命题在不同公理体系下的等价翻译"提供了严格的数学保证。

**实例**：将群论公理映射到环论的加法群子理论中。

```
MMT 中的群论 → 环论态射：

morphism GroupToRingAdditive : Group → Ring/additiveGroup
  G          ↦ R           -- 群的载体集映射为环的载体集
  op         ↦ +           -- 群运算映射为环的加法
  unit       ↦ 0           -- 单位元映射为加法零元
  inv        ↦ -           -- 逆元映射为加法逆
  assoc_axiom ↦ add_assoc  -- 结合律公理映射为加法结合律
```

这种映射一旦建立，群论中所有已证的定理都会自动获得它们在环的加法群中的对应版本，无需重新证明。

### 2.2 MathHub 数学知识门户

**MMT 定义**：MathHub（[mathhub.info](https://mathhub.info)）是一个基于 MMT 的数学知识门户，它聚合了 Mizar、Isabelle、Coq、PVS、Lean 等数十种形式化数学系统的知识库，以统一的理论图（theory graph）方式将它们组织在一起。

**核心思想**：
- **联邦式架构**：各形式化系统的知识库保持独立维护，但通过 MMT 的 theory morphism 建立互操作桥梁
- **统一命名空间**：使用 MMT URI 方案对全球数学知识进行唯一标识
- **语义链接**：不同系统中的等价概念通过 morphism 显式关联

**Lv-00 借鉴意义**：Lv-00 的 `.lvz` 公理包生态可以借鉴这种"联邦式知识库"架构。与 MathHub 聚合形式化系统类似，Lv-00 可以聚合不同来源、不同维护者的公理包，并用统一的 .lvz 包管理机制实现互操作。

### 对照表：MMT 概念 ↔ Lv-00 概念

| MMT 概念 | 形式化定义 | Lv-00 对应概念 | 实现状态 |
|----------|-----------|---------------|---------|
| **Theory** | 一组符号声明、公理、定理的集合 | **公理包（Axiom Package）** | 已设计 |
| **Theory Morphism** | 从源理论到目标理论的翻译映射 | **公理包间定理翻译桥梁** | 规划中 |
| **LF 层** | 作为元框架，定义逻辑的语法和推理规则 | **"公理中立"内核**（不内建任何逻辑） | 已实现 |
| **View** | 一对理论之间的具体态射实例 | **平行公理替换**（如欧氏→双曲） | 规划中 |
| **Archive** | 数学知识的版本化归档格式 | **.lvz 包格式**（manifest.json + INDEX.json） | 已设计 |
| **MathHub** | 联邦式数学知识门户 | **Lv-00 公理包注册中心** | 远期规划 |
| **Notation** | 符号的语法表示与渲染规则 | **语法前端**（notation rules） | 已设计 |
| **Meta Theory** | 定义某个理论所使用的元逻辑 | **公理包声明的逻辑基础** | 规划中 |

### 2.3 LF（Logical Framework）层

**MMT 定义**：MMT 基于 LF（Edinburgh Logical Framework）作为其元框架。LF 本身是一个非常弱的类型理论，仅提供：
- 依赖类型 Πx:A. B(x)（dependent product types）
- 类型族 λx:A. B(x)（type families）
- 一种判断形式 Γ ⊢ t : A

LF 不规定任何具体的逻辑连接词（如 ∧、∨、→、∀、∃ 等），这些均由用户在 LF 之上自行定义。

**Lv-00 借鉴意义**：这与 Lv-00 的"公理中立"内核设计在哲学上高度一致。Lv-00 的推理内核不内建任何逻辑——无论是经典逻辑、直觉主义逻辑、模态逻辑还是线性逻辑，都由公理包来定义。内核只负责：
1. 解析公理包的声明（符号、语法规则、推理规则）
2. 在给定的公理包下进行机械推理
3. 验证推理步骤是否合法

这种"内核最小化、逻辑外置化"的设计路线与 MMT/LF 的元框架思路一脉相承。

```
比较：

MMT/LF 层：                Lv-00 内核层：
┌─────────────────┐       ┌─────────────────┐
│ LF 类型检查器    │       │ 规则引擎         │
│ （不内建逻辑）   │  ≈    │ （不内建逻辑）    │
│                 │       │                 │
│ 由用户定义：     │       │ 由公理包定义：    │
│  - 逻辑连接词    │       │  - 逻辑连接词     │
│  - 推理规则      │       │  - 推理规则       │
│  - 证明策略      │       │  - 证明策略       │
└─────────────────┘       └─────────────────┘
```

### 2.4 视图（View）机制

**MMT 定义**：在 MMT 中，`view` 是一个具名的 theory morphism，它声明性地定义两个理论之间的映射关系。view 不仅映射符号，还映射公理，并且 MMT 的类型检查器会验证所有映射的公理在目标理论中是否确实成立。

```
view EuclideanToHyperbolic : EuclideanPlane -> HyperbolicPlane =
  Point      = Point
  Line       = Line
  parallel   = \l P. { l' | l' passes through P ∧ ¬(l' intersects l) }
  // 对于欧氏平面中的一条线和一个点，有唯一平行线
  // 但在双曲平面中，"平行"的定义完全不同——存在无穷多条
  parallel_postulate = THEOREM  // 在目标理论中需重新验证
```

**Lv-00 借鉴意义**：这正是 Lv-00 中"欧氏平面公理包 → 双曲平面公理包"的平行公理替换的形式化基础。通过 view 机制，可以做到：
- 显式声明两个公理包之间的概念对应关系
- 自动检查哪些定理可以直接翻译、哪些需要重新证明
- 追踪"替换了哪个公理"导致"哪些定理失效"

### 2.5 Archive 格式

**MMT 定义**：MMT Archive（简称 MMT archive）是 MMT 用于组织、分发和版本化数学知识的文件归档格式。一个标准的 MMT archive 具有如下目录结构：

```
my-theory-archive/
├── META-INF/
│   └── MANIFEST.MF       # 归档元数据（名称、版本、依赖）
├── source/
│   ├── theory_a.mmt      # 理论定义文件（OMDoc/MMT 语法）
│   ├── theory_b.mmt
│   └── views/
│       └── a_to_b.mmt    # 视图定义
├── content/
│   └── MathHub/           # 编译后的知识内容
├── narration/
│   └── index.html         # 文档入口
└── relational/
    └── theory-graph.json  # 理论依赖图
```

**Lv-00 借鉴意义**：Lv-00 的公理包管理可以直接参考 MMT archive 的下列设计决策：
- **META-INF/MANIFEST.MF** → `.lvz` 包中的 `manifest.json`，记录包的名称、版本号、命名空间、许可证
- **source/** 的理论文件 → `.lvz` 包中的公理定义文件（.lv 格式）
- **依赖管理**：MANIFEST.MF 的 `dependencies` 字段 → manifest.json 的 `deps` 数组
- **版本化**：archive 的版本号语义 → .lvz 包的 semver 版本号

---

## 3. Lv-00 映射方案

本节将 Lv-00 公理包系统系统地映射到 MMT 理论架构上，为后续的互操作实现提供概念蓝图。

### 3.1 .lvz 文件 ↔ MMT Archive 结构

**Lv-00 侧的 manifest.json：**

```json
{
  "name": "euclidean-geometry",
  "namespace": "lv.geometry.euclidean",
  "version": "0.4.0",
  "license": "MIT",
  "deps": [
    { "name": "classical-logic", "version": "^1.2.0" },
    { "name": "set-theory-zf",   "version": "^2.0.0" }
  ],
  "axioms": [
    "incidence.axiom1",
    "incidence.axiom2",
    "parallel.axiom",
    "congruence.axiom1"
  ],
  "exports": [
    "Point", "Line", "Plane",
    "parallel", "intersect", "congruent"
  ]
}
```

**对应的 MMT Archive 侧（META-INF/MANIFEST.MF）：**

```
Manifest-Version: 1.0
Archive-ID: lv.geometry.euclidean
Archive-Version: 0.4.0
Archive-License: MIT
Dependencies: lv.logic.classical@1.2.0, lv.set-theory.zf@2.0.0
Source-Root: source
Content-Root: content
```

**映射逻辑**：

| .lvz 字段 | MMT 对应 | 转换规则 |
|-----------|----------|---------|
| `name` | `Archive-ID`（去命名空间后缀） | 直接映射 |
| `namespace` | `Archive-ID`（完整路径） | 用 `.` 替换 `/` |
| `version` | `Archive-Version` | 直接映射，遵循 semver |
| `deps[].name` | `Dependencies` 条目 | 连字符分隔，附加版本约束 |
| `axioms[]` | source 目录下的 .mmt 声明 | 每个公理展开为 LF 声明 |

### 3.2 公理包依赖关系 ↔ MMT Theory Morphism

**场景**：公理包 `abelian-group` 依赖于公理包 `group`。

Lv-00 侧的依赖声明（abelian-group 的 manifest.json）：

```json
{
  "name": "abelian-group",
  "deps": [
    { "name": "group", "version": "^1.0.0" }
  ],
  "extends": {
    "base": "group.GroupTheory",
    "additional_axioms": ["commutative_op"]
  }
}
```

MMT 侧的对应 theory morphism（abelian-group/source/abelian_group.mmt）：

```
theory AbelianGroup : lv.algebra.group:?GroupTheory =
  include ?GroupTheory           // 继承群论的全部声明
  commutative_op : {x:G} {y:G}  // 新增交换律公理
    ⊦ op x y ≐ op y x
```

**关键观察**：Lv-00 的 `extends` 机制本质上就是 MMT 的 theory inclusion + additional axioms。这使得 Lv-00 公理包之间的继承链可以直接在 MMT 中建模。

### 3.3 平行公理替换 ↔ MMT View

**场景**：欧氏几何公理包通过"替换平行公理"派生出双曲几何公理包。

```
Lv-00 公理包结构：

euclidean-plane.lvz（欧氏平面）
├── manifest.json      # 声明公理列表
├── axioms/
│   ├── incidence.lv   # 关联公理
│   ├── order.lv       # 顺序公理
│   ├── congruence.lv  # 合同公理
│   └── parallel.lv    # 欧氏平行公理 ── 被替换 ──┐
└── theorems/                                    │
    └── ...                                      │
                                                 │
hyperbolic-plane.lvz（双曲平面）                   │
├── manifest.json                                │
│   {                                            │
│     "extends": "euclidean-plane",              │
│     "replace_axioms": ["parallel"],            │
│     "new_axioms": ["hyperbolic_parallel"]      │
│   }                                            │
├── axioms/                                      │
│   └── hyperbolic_parallel.lv  ←───────────────┘
└── theorems/
```

MMT 侧的对应 view：

```
view EuclideanToHyperbolic : ?EuclideanPlane -> ?HyperbolicPlane =
  // 不变的符号直接传递
  Point      = Point
  Line       = Line
  between    = between
  congruent  = congruent

  // 被替换的公理：在目标理论中变成一条需要验证的定理
  parallel_postulate = ?HyperbolicPlane?hyperbolic_parallel_postulate
```

**意义**：这个映射说明，Lv-00 的"公理替换"语义可以无损地编码为 MMT 的 view 映射。一旦建立这种对应，Lv-00 的定理就可以通过 MMT 的 theory morphism 机制与其他形式化系统进行交换。

### 3.4 概念映射汇总

```
┌───────────────────────────────────────────────────────────┐
│              Lv-00 公理包系统                              │
│                                                           │
│  .lvz 公理包 (manifest.json + .lv files)                  │
│       │                                                   │
│       │  ┌─────────────────────────┐                      │
│       └──│ 公理中立内核（不内建逻辑）│                      │
│          └────────────┬────────────┘                      │
│                       │                                   │
│          ┌────────────┼────────────┐                      │
│          │            │            │                      │
│     ┌────┴────┐  ┌────┴────┐  ┌────┴────┐                 │
│     │公理包 A │  │公理包 B │  │公理包 C │                 │
│     │(经典逻辑)│  │(直觉逻辑)│  │(模态逻辑)│                 │
│     └─────────┘  └─────────┘  └─────────┘                 │
│                                                           │
╞═══════════════════════════════════════════════════════════╡
│              MMT 理论架构                                  │
│                                                           │
│  MMT Archive (MANIFEST.MF + .mmt files)                   │
│       │                                                   │
│       │  ┌─────────────────────────┐                      │
│       └──│ LF 元框架（不内建逻辑）  │                      │
│          └────────────┬────────────┘                      │
│                       │                                   │
│          ┌────────────┼────────────┐                      │
│          │            │            │                      │
│     ┌────┴────┐  ┌────┴────┐  ┌────┴────┐                 │
│     │Theory A │  │Theory B │  │Theory C │                 │
│     │(经典逻辑)│  │(直觉逻辑)│  │(模态逻辑)│                 │
│     └─────────┘  └─────────┘  └─────────┘                 │
│                                                           │
│     箭头 = Theory Morphism / View                         │
│     A → B → C 表示理论之间的翻译关系                       │
└───────────────────────────────────────────────────────────┘
```

---

## 4. 实现路线图

将 MMT 的数学知识管理能力集成到 Lv-00 生态中，分三个阶段推进。

### 4.1 总体路线

| 阶段 | 名称 | 核心目标 | 预计工期 | 前置依赖 |
|------|------|----------|---------|---------|
| **Phase 1** | 概念映射 | 将 Lv-00 公理包体系建模为 MMT 理论 | 2-3 个月 | 公理包系统稳定 |
| **Phase 2** | 互操作桥 | 通过 MMT 与其他形式化系统交换定理 | 3-4 个月 | Phase 1 完成 |
| **Phase 3** | 联邦知识库 | 构建 Lv-00 公理包的 MathHub 式门户 | 4-6 个月 | Phase 2 完成 |

### 4.2 Phase 1: 概念映射（Concept Mapping）

**目标**：将 Lv-00 公理包体系建模为 MMT 理论，使 Lv-00 在 MMT 的元理论框架下获得严格的形式化语义。

**具体任务**：

1. **公理包 → Theory 映射器**
   - 将 .lvz 包中的每个公理文件（.lv）解析为 LF 声明
   - 自动生成对应的 MMT theory 定义
   - 处理公理包之间的 `extends` 关系，生成 theory inclusion

2. **manifest.json → MANIFEST.MF 转换器**
   - 将 .lvz 的元数据格式转换为 MMT archive 的标准元数据格式
   - 处理依赖版本约束的语义转换
   - 生成正确的 MMT URI 命名空间映射

3. **类型系统适配**
   - 将 Lv-00 的类型系统映射到 LF 的依赖类型系统
   - 处理高阶逻辑类型与 LF 类型的对应关系

4. **验证闭环**
   - 在 MMT 中加载转换后的 theory，运行类型检查器
   - 确保所有公理声明在 LF 中是良类型的（well-typed）
   - 输出验证报告，标注不兼容的类型构造

**交付物**：

```
lv-to-mmt/
├── converter/
│   ├── lvz_parser.py        # .lvz 包解析器
│   ├── manifest_converter.py  # manifest.json → MANIFEST.MF
│   ├── axiom_to_lf.py       # .lv 公理 → LF 声明
│   └── type_adapter.py      # 类型系统适配层
├── tests/
│   ├── fixtures/            # 测试用例
│   └── test_converter.py
└── output/                  # 生成的 MMT archive
    └── lv.geometry.euclidean/
```

### 4.3 Phase 2: 互操作桥（Interoperability Bridge）

**目标**：通过 MMT 作为中间表示，使 Lv-00 能够与其他形式化系统（Coq、Isabelle、Lean 等）交换定理。

**具体任务**：

1. **Lv-00 ↔ MMT 双向转换**
   - Lv-00 定理 → MMT 定理表示（导出通道）
   - MMT 定理 → Lv-00 公理包内定理（导入通道）
   - 处理证明项的转换（proof term translation）

2. **MMT 现有互操作桥的复用**
   - MMT 已有 Coq/Isabelle/Mizar/HOL Light 等系统的导入导出器
   - 利用这些已有的桥梁，使 Lv-00 间接连接到这些系统
   - 例如：Lv-00 theorem → MMT → Coq，反之亦然

3. **Theorem Provenance（定理溯源）**
   - 为每个导入的定理标记其来源系统
   - 在 Lv-00 的定理数据库中维护来源追踪链
   - 处理"信任计算"——哪些来源的定理可以直接引用，哪些需要重新验证

4. **Notation 适配**
   - 不同形式化系统对同一个数学概念使用不同的符号约定
   - 通过 MMT 的 notation 机制建立符号映射表
   - 在转换过程中自动应用符号翻译

**关键技术挑战**：

| 挑战 | 说明 | 缓解策略 |
|------|------|---------|
| **证明项翻译丢失** | MMT 现有导出器对证明项的支持不完整 | 优先支持"定理声明"级别的互操作，证明项逐步完善 |
| **逻辑不一致** | 不同系统的底层逻辑可能冲突（如排中律） | 在公理包层面标注逻辑假设，MMT 的 meta theory 机制可处理 |
| **类型系统差异** | Coq 的 CIC、Isabelle 的 HOL、Lean 的类型论互不兼容 | 通过 LF 作为最小公共类型框架进行近似映射 |
| **性能开销** | 双向转换 + 类型检查可能导致显著的性能开销 | 采用缓存机制，公理包间定理只转换和验证一次 |

### 4.4 Phase 3: 联邦知识库（Federated Knowledge Base）

**目标**：构建类似 MathHub 的 Lv-00 公理包门户，支持公理包的发现、注册、搜索和互操作。

**具体任务**：

1. **公理包注册中心（Package Registry）**
   - 提供公理包的上传、版本管理、发布流程
   - 自动解析 manifest.json 中的依赖关系
   - 构建全局的 theory graph（公理包依赖图）

2. **Web 门户（MathHub-style Portal）**
   - 公理包浏览与搜索
   - 公理内容在线查看（带语法高亮）
   - 依赖关系的可视化（D3.js 或 Cytoscape.js 的图渲染）
   - 定理的跨公理包导航

3. **互操作索引**
   - 自动发现并索引公理包之间的 theory morphism
   - 提供"查找等价定理"功能——在给定公理包中搜索与另一个公理包中某个定理语义等价的命题
   - 维护跨系统引用的完整性

4. **社区治理**
   - 公理包的命名空间管理（避免命名冲突）
   - 公理包的审核与质量评级
   - 贡献者认证与签名验证

**架构示意**：

```
┌──────────────────────────────────────────────────────┐
│                  Lv-00 MathHub Portal                 │
│                                                      │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  │
│  │ Web Frontend│  │  REST API   │  │ Search Engine│  │
│  │ (React)     │  │  (FastAPI)  │  │ (Elasticsearch)│ │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘  │
│         │                │                │          │
│         └────────────────┼────────────────┘          │
│                          │                           │
│              ┌───────────┴───────────┐               │
│              │  Package Registry     │               │
│              │  (theory graph store) │               │
│              └───────────┬───────────┘               │
│                          │                           │
│         ┌────────────────┼────────────────┐          │
│         │                │                │          │
│  ┌──────┴──────┐  ┌──────┴──────┐  ┌──────┴──────┐  │
│  │ Lv-00 Kernel│  │ MMT Bridge  │  │ External    │  │
│  │ (proof check│  │ (interop)   │  │ Systems     │  │
│  │  + type chk)│  │             │  │ (Coq, etc.) │  │
│  └─────────────┘  └─────────────┘  └─────────────┘  │
└──────────────────────────────────────────────────────┘
```

---

## 5. 附录

### 5.1 术语对照表

| 英文术语 | 中文翻译 | 首次出现章节 | 简要说明 |
|----------|---------|-------------|---------|
| Theory Morphism | 理论态射 | 2.1 | 两个数学理论之间的保持真理的翻译映射 |
| MathHub | 数学知识门户 | 2.2 | MMT 的联邦式数学知识聚合平台 |
| LF (Logical Framework) | 逻辑框架 | 2.3 | 一种极简的依赖类型论，用于定义逻辑的元理论 |
| View | 视图 | 2.4 | MMT 中一对理论之间的具名态射 |
| Archive | 归档 | 2.5 | MMT 的数学知识文件组织与分发格式 |
| Theory Graph | 理论图 | 3.2 | 以理论为节点、态射为边的有向图结构 |
| Meta Theory | 元理论 | 3.1 | 定义某个理论时所使用的底层逻辑系统 |
| Notation | 记法 | 4.3 | 数学符号的语法表示与渲染约定 |
| Proof Term | 证明项 | 4.3 | 证明的形式化编码，可以作为数据进行传递和检查 |
| OMDoc | 开放数学文档格式 | 1.2 | 一种基于 XML 的数学文档标记语言 |

### 5.2 关键参考链接

| 资源 | URL | 说明 |
|------|-----|------|
| MMT 官方仓库 | https://github.com/UniFormal/MMT | 源码、文档、示例 |
| MMT 官方文档 | https://uniformal.github.io/ | API 文档与教程 |
| MathHub 门户 | https://mathhub.info | 在线数学知识浏览 |
| MMT 入门论文 | https://uniformal.github.io/doc/philosophy/ | 设计哲学与架构概述 |
| LF 参考手册 | https://lf.in.tum.de/ | Edinburgh LF 的正式定义 |

### 5.3 Lv-00 公理包标准结构（参考）

```
example-axiom-package.lvz/
├── manifest.json       # 包元数据（名称、版本、依赖、许可证）
├── INDEX.json          # 公理与定理索引
├── axioms/
│   ├── logic.lv        # 逻辑公理
│   ├── set.lv          # 集合公理
│   └── geometry.lv     # 几何公理
├── theorems/
│   ├── lemmas.lv       # 引理
│   └── propositions.lv # 命题
├── notations/
│   └── symbols.json    # 符号定义
├── tests/
│   └── sanity.lvt      # 公理一致性测试
└── docs/
    └── README.md       # 公理包文档
```

### 5.4 扩展阅读

1. **Rabe, F. & Kohlhase, M.** (2013). *A Scalable Module System*. 讨论了 MMT 的模块系统如何支持大规模数学知识管理。

2. **Kohlhase, M.** (2014). *MMT: A Foundation-Independent Approach to Formal Knowledge*. 阐述了 MMT 的"基础独立"设计哲学，与 Lv-00 的"公理中立"理念同源。

3. **Horozal, F. & Rabe, F.** (2015). *Representing Model Theory in a Type-Theoretical Logical Framework*. 展示了如何在 LF 中表示模型论，对理解 theory morphism 的语义有参考价值。

---

*文档结束。如有疑问或建议，请通过项目 Issue Tracker 反馈。*

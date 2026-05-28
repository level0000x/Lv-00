# AXIOM DEPENDENCY MAP —— mathlib4 风格公理依赖关系图

> **Lv-00 Axiom Package Registry** · 生成日期: 2026-05-24
> 本文档借鉴 mathlib4 的 **IncidenceGeometry 类型类设计**和**公理独立性追踪**方法，对 Lv-00 全部 48 个公理包进行分类、分级和依赖关系分析。

---

## 目录

1. [核心设计方法论](#1-核心设计方法论)
2. [IncidenceGeometry 等级体系](#2-incidencegeometry-等级体系)
3. [全局依赖拓扑图](#3-全局依赖拓扑图)
4. [Birkhoff vs Tarski 独立性对比](#4-birkhoff-vs-tarski-独立性对比)
5. [公理间等价性矩阵](#5-公理间等价性矩阵)
6. [从最小公理集到完整几何——依赖链路图](#6-从最小公理集到完整几何依赖链路图)
7. [mathlib4 可表达性分析](#7-mathlib4-可表达性分析)

---

## 1. 核心设计方法论

### 1.1 mathlib4 启示

mathlib4 的几何形式化体系建立在以下核心原则上：

| mathlib4 原则 | Lv-00 对应的实现 |
|---------------|------------------|
| **IncidenceGeometry 类型类**：定义 `Point`、`Line`、`on` 三个原语，即可获得关联几何的全部推导能力 | `euclidean.I1`/`I2`/`I3` 构成 Layer 0 关联层 |
| **公理独立性追踪**：每个定理标注其依赖的公理集合，支持最小化推理 | `manifest.json` 中 `depends_on` 字段 + 分层架构 |
| **Birkhoff 与 Tarski 双路径**：用不同的原语集合定义等价几何体系 | `euclidean_plane`（Tarski 风格，以 betweenness + congruence 为原语） |
| **SyntheticGeometry 免坐标风格**：纯综合几何推理，不依赖实数坐标 | Lv-00 所有几何包均采用抽象原语，不引入 R^2 |
| **类型安全的几何关系**：`∠ A B C` 是类型化表达式 | Lv-00 中 `lvz` 格式支持带类型的谓词 `on(A:Point, l:Line)` |

### 1.2 Lv-00 分层架构的优势

```
                    ┌─────────────────────────────────────────────────┐
                    │         Layer 4: Parallels (平行公理层)            │
                    │      [euclidean.Playfair] | [hyperbolic.Lobachevsky]│
                    ├─────────────────────────────────────────────────┤
                    │         Layer 3: Continuity (连续性公理层)          │
                    │            [euclidean.Dedekind]                    │
                    ├─────────────────────────────────────────────────┤
                    │         Layer 2: Congruence (全等公理层)            │
                    │        [euclidean.C1] [euclidean.C2] [euclidean.C3]│
                    ├─────────────────────────────────────────────────┤
                    │         Layer 1: Order (顺序公理层)                 │
                    │    [euclidean.B1] [euclidean.B2] [euclidean.B3] [euclidean.B4]│
                    ├─────────────────────────────────────────────────┤
                    │         Layer 0: Incidence (关联公理层)              │
                    │        [euclidean.I1] [euclidean.I2] [euclidean.I3]│
                    └─────────────────────────────────────────────────┘
    ┌─────────────────────────────────────────────────────────────────────────┐
    │                         变体切换机制                                      │
    │  Layer 4 替换: Playfair → Lobachevsky  → 欧氏几何 ↔ 双曲几何              │
    │  Layer 3 移除: 不含 Dedekind           → 初等 Tarski 几何                 │
    │  Layer 2+1+0 保留                      → 绝对几何 (Absolute Geometry)     │
    └─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. IncidenceGeometry 等级体系

依据 mathlib4 的 `IncidenceGeometry` 类型类层级，每个公理包被赋予一个等级。

| 等级 | mathlib4 对应 | 条件 | 典型包 |
|------|-------------|------|--------|
| **G0** | 无几何类型类 | 不含任何几何原语（Point/Line/on） | 集合论、代数、逻辑包 |
| **G1** | `IncidenceGeometry` | 定义了点、线和关联关系 `on` | 射影几何、仿射几何 |
| **G2** | `OrderedGeometry` 或 `MetricGeometry` | 在 G1 基础上添加距离/介于/全等关系 | metric_space |
| **G3** | `EuclideanGeometry` 或 `HyperbolicGeometry` | 完整的几何体系（含连续性/平行公理） | euclidean_plane, hyperbolic_geometry |

### 2.1 完整分级表

#### G3 -- 完整几何 (Full Geometry)

| 包名 | 原语 | 平行公理 | 连续性 |
|------|------|---------|--------|
| `euclidean_plane` | Point, Line, B, =~= | Playfair | Dedekind |
| `hyperbolic_geometry` | Point, Line, B, =~= | Lobachevsky | Dedekind |
| `elliptic_geometry` | Point, Line, 球面度量 | (不存在平行线) | Cayley-Klein 度量 |
| `differential_geometry` | 光滑流形, 切空间, 曲率 | -- | 流形完备性 |

#### G2 -- 度量几何 (Metric Geometry)

| 包名 | 原语 | 备注 |
|------|------|------|
| `metric_space` | 距离函数 d(·,·) | Frechet 1906, 仅度量无关联 |

#### G1 -- 关联几何 (Incidence Geometry)

| 包名 | 原语 | 备注 |
|------|------|------|
| `projective_geometry` | Point, Line, on | 所有直线相交, Veblen-Young |
| `affine_geometry` | Point, Line, on, // | 关联 + 平行, 无度量 |

#### G0 -- 非几何 (Non-Geometric)

所有代数、逻辑、集合论、分析、拓扑包均属 G0。完整清单：

| 分类 | 包名 |
|------|------|
| **基础** | `zfc_set_theory`, `nbg_set_theory`, `descriptive_set_theory`, `domain_theory`, `computability_theory`, `computational_complexity_theory` |
| **逻辑** | `classical_propositional_logic`, `intuitionistic_logic`, `intuitionistic_propositional_logic`, `modal_logic`, `linear_logic`, `simple_type_theory`, `dependent_type_theory`, `homotopy_type_theory`, `proof_theory`, `model_theory` |
| **代数** | `group_theory`, `ring_theory`, `field_theory`, `lattice_theory`, `boolean_algebra`, `lie_theory`, `universal_algebra`, `homological_algebra`, `order_theory` |
| **分析** | `real_analysis`, `functional_analysis`, `measure_theory`, `probability_theory`, `ergodic_theory` |
| **拓扑** | `point_set_topology`, `algebraic_topology` |
| **数论/组合** | `number_theory`, `combinatorics`, `game_theory`, `information_theory`, `graph_theory` |
| **算术** | `peano_arithmetic`, `second_order_arithmetic`, `robin_arithmetic`, `presburger_arithmetic` |
| **其他** | `category_theory`, `cartesian_closed_category`, `topos_theory`, `algebraic_geometry`, `galois_theory`, `quantum_information_theory`, `linear_algebra`, `synthetic_differential_geometry` |

---

## 3. 全局依赖拓扑图

下面的 ASCII 图展示了 Lv-00 全部 48 个公理包的依赖关系拓扑（箭头表示"依赖"）：

```
                                    ┌──────────────────────────────────────────────────────┐
                                    │              二阶算术 (second_order_arithmetic)          │
                                    │              [G0]  Big Five: RCA₀/WKL₀/ACA₀/ATR₀/Π¹₁-CA₀│
                                    └──────────────┬───────────────────────────────────────────┘
                                                   │
            ┌──────────────────────────────────────┼──────────────────────────────────────┐
            │                                      │                                      │
            ▼                                      ▼                                      ▼
┌───────────────────────┐           ┌───────────────────────────┐         ┌────────────────────────┐
│   peano_arithmetic    │           │     real_analysis         │         │  computational_         │
│   [G0]                 │           │     [G0]                  │         │   complexity_theory     │
│   Peano Arithmetic    │◄──────────│  完备性+有序域+Archi-      │         │   [G0]                  │
│                       │  (通过    │   medean property          │         │   P/NP/PSPACE           │
└───────────┬───────────┘  field/   └────────┬────┬──────────────┘         └────────────┬───────────┘
            │              ring)             │    │                                    │
            │                                │    │                                    │
            ▼                                │    │                                    ▼
┌───────────────────────┐                    │    │                      ┌────────────────────────┐
│   robin_arithmetic    │                    │    │                      │   computability_theory │
│   [G0] (Q, 7公理)     │                    │    │                      │   [G0]                 │
│   不包含归纳法           │                    │    │                      │   Turing, Church, Kleene│
└───────────────────────┘                    │    │                      └────────────────────────┘
                                             │    │
     ┌──────────────────┬───────────────────┘    └───────────────────────┬──────────────────┐
     │                  │                                                │                  │
     ▼                  ▼                                                ▼                  ▼
┌───────────┐   ┌───────────────┐                           ┌─────────────────┐   ┌──────────────────┐
│  field_   │   │  functional_  │                           │  measure_theory │   │ ergodic_theory   │
│  theory   │   │   analysis    │                           │  [G0]           │   │ [G0]             │
│  [G0]     │   │   [G0]        │                           │  σ-代数+测度     │──▶│ 保测变换+遍历性   │
└─────┬─────┘   └───────┬───────┘                           └────────┬────────┘   └──────────────────┘
      │                 │                                            │
      │                 │                                            │
      ▼                 │                                            ▼
┌───────────┐           │                                  ┌──────────────────┐
│  ring_    │           │                                  │ probability_     │
│  theory   │           │                                  │   theory         │
│  [G0]     │           │                                  │ [G0] Kolmogorov  │
└─────┬─────┘           │                                  └──────────────────┘
      │                 │
      │                 │                    ┌───────────────────────────────┐
      ▼                 │                    │        拓扑家族                │
┌───────────┐           │                    │                               │
│  group_   │           │                    │  point_set_topology [G0]       │
│  theory   │           │                    │     ├── algebraic_topology [G0]│
│  [G0]     │◄──────────┼────────────────────│     │   (需 group_theory)      │
└───────────┘           │                    │     ├── domain_theory [G0]     │
                        │                    │     └── descriptive_set_       │
                        │                    │          theory [G0]           │
                        │                    └───────────────────────────────┘
                        │
     ┌──────────────────┼──────────────────┬──────────────────┬──────────────────────┐
     │                  │                  │                  │                      │
     ▼                  ▼                  ▼                  ▼                      ▼
┌───────────┐   ┌───────────────┐   ┌───────────────┐   ┌───────────────────┐   ┌──────────────────┐
│ lie_      │   │ boolean_      │   │ lattice_      │   │ order_theory      │   │ homological_     │
│ theory    │   │  algebra      │   │  theory       │   │ [G0]              │   │   algebra        │
│ [G0]      │   │  [G0]         │   │  [G0]         │   │ 偏序+全序+良序+     │   │ [G0]             │
│           │   │               │   │               │   │ Zorn/Dilworth/    │   │ 链复形+导出函子    │
└───────────┘   └───────┬───────┘   └───────┬───────┘   │ Knaster-Tarski    │   └──────────────────┘
                        │                   │           └───────────────────┘
                        │                   │
                        │                   │                    ┌───────────────────────────────┐
                        │                   │                    │         集合论家族              │
                        │                   │                    │                               │
                        │                   │                    │   zfc_set_theory [G0]          │
                        │                   │                    │      └── nbg_set_theory [G0]   │
                        │                   │                    │          (保守扩张)             │
                        │                   │                    └───────────────────────────────┘
                        │                   │
                        │                   │
     ┌──────────────────┼───────────────────┼──────────────────┬──────────────────────┐
     │                  │                   │                  │                      │
     ▼                  ▼                   ▼                  ▼                      ▼
┌───────────┐   ┌──────────────────┐ ┌──────────────────┐ ┌──────────────────┐ ┌──────────────────┐
│ universal │   │ number_theory    │ │ category_theory  │ │ galois_theory    │ │ graph_theory     │
│  algebra  │   │   [G0]           │ │   [G0]           │ │   [G0]           │ │   [G0]           │
│  [G0]     │   │  素数+代数数论    │ │  对象+态射+函子     │ │  域扩张+Galois群  │ │  图论, 70个模板   │
└───────────┘   └──────────────────┘ └────────┬─────────┘ └──────────────────┘ └──────────────────┘
                                              │
                                              ▼
                                     ┌──────────────────┐
                                     │ cartesian_closed │
                                     │   _category      │
                                     │ [G0]             │
                                     │ 终对象+积+指数     │
                                     └──────────────────┘
                                              │
                                              ▼
                                     ┌──────────────────┐
                                     │   topos_theory   │
                                     │ [G0]             │
                                     │ 子对象分类器Ω     │
                                     └──────────────────┘
                                              │
                                              ▼
                                     ┌──────────────────────────────┐
                                     │ synthetic_differential_       │
                                     │   geometry                   │
                                     │ [G0]                        │
                                     │ Weil代数+Kock-Lawvere公理    │
                                     └──────────────────────────────┘

     ┌──────────────────────────────────────────────────────────────────────┐
     │                           逻辑家族                                     │
     │                                                                      │
     │  classical_propositional_logic [G0]                                   │
     │     ├── modal_logic [G0] (K/T/S4/S5, Kripke 语义)                    │
     │     └── (Lindenbaum-Tarski) ──▶ boolean_algebra [G0]                  │
     │                                                                      │
     │  intuitionistic_propositional_logic [G0]                              │
     │     └── intuitionistic_logic [G0] (一阶 Heyting)                      │
     │                                                                      │
     │  linear_logic [G0] (Girard 1987)                                      │
     │                                                                      │
     │  simple_type_theory [G0] (Church 1940)                                │
     │     └── dependent_type_theory [G0] (Martin-Lof)                       │
     │           └── homotopy_type_theory [G0] (Univalence)                   │
     │                                                                      │
     │  proof_theory [G0] (Gentzen 矢列演算)                                  │
     │  model_theory [G0] (完全性+紧致性+Löwenheim-Skolem)                    │
     └──────────────────────────────────────────────────────────────────────┘

     ┌──────────────────────────────────────────────────────────────────────┐
     │                           几何家族 (核心)                              │
     │                                                                      │
     │  ┌─────────────────────────────────────────────────────────────┐     │
     │  │ euclidean_plane [G3]                                         │     │
     │  │   Layers: incidence → order → congruence → continuity →      │     │
     │  │            parallels (Playfair)                              │     │
     │  │   原语: Point, Line, B (betweenness), =~= (congruence)       │     │
     │  │   风格: Tarski 综合几何 (SyntheticGeometry)                   │     │
     │  └───────────┬─────────────────────────────────────────────────┘     │
     │              │                                                       │
     │              ├──▶ hyperbolic_geometry [G3]                            │
     │              │    复用 Layers 0-3, 替换 Layer 4 为 Lobachevsky         │
     │              │                                                       │
     │              ├──▶ (Layers 0-3 即 absolute geometry)                   │
     │              │    三角形内角和 ≤ 180°                                  │
     │              │                                                       │
     │  ┌───────────┴─────────────────────────────────────────────────┐     │
     │  │ 独立几何体系:                                                 │     │
     │  │                                                              │     │
     │  │   elliptic_geometry [G3] ─── RP^2, Cayley-Klein 度量         │     │
     │  │   projective_geometry [G1] ─── Veblen-Young, Desargues,      │     │
     │  │                               Pappus                         │     │
     │  │   affine_geometry [G1] ─── Coxeter/Artin, 关联+平行           │     │
     │  │   metric_space [G2] ─── Fréchet, 距离+三角不等式              │     │
     │  │   differential_geometry [G3] ─── 光滑流形+曲率                │     │
     │  │   algebraic_geometry [G0] ─── 簇+概形 (需 ring/field)         │     │
     │  │   synthetic_differential_geometry [G0] ─── Weil代数+Kock-Lawvere (需 topos) │
     │  └──────────────────────────────────────────────────────────────┘     │
     └──────────────────────────────────────────────────────────────────────┘
```

### 3.1 依赖链路概要

```
zfc_set_theory (基础层)
│
├─── real_analysis ─── field_theory ─── ring_theory ─── group_theory
│       │                    │
│       │                    ├─── galois_theory
│       │                    │
│       ├─── functional_analysis (需 point_set_topology)
│       │
│       ├─── measure_theory
│       │       ├─── probability_theory
│       │       └─── ergodic_theory
│       │
│       └─── differential_geometry
│
├─── point_set_topology
│       ├─── algebraic_topology (需 group_theory)
│       ├─── domain_theory (需 lattice_theory)
│       └─── descriptive_set_theory
│
├─── number_theory (需 ring_theory)
│
├─── order_theory (需 zfc + lattice_theory)
│
├─── model_theory
│
└─── graph_theory (需 zfc + combinatorics)

独立基础包 (无依赖):
  - classical_propositional_logic (├── modal_logic)
  - intuitionistic_propositional_logic (├── intuitionistic_logic)
  - linear_logic
  - simple_type_theory (├── dependent_type_theory ─── homotopy_type_theory)
  - proof_theory
  - category_theory (├── cartesian_closed_category ─── topos_theory ─── synthetic_differential_geometry)
  - boolean_algebra (需 lattice_theory)
  - combinatorics
  - game_theory
  - information_theory
  - computability_theory (├── computational_complexity_theory)
  - universal_algebra

几何独立体系:
  - euclidean_plane [G3] (├── hyperbolic_geometry [G3])
  - elliptic_geometry [G3]
  - projective_geometry [G1]
  - affine_geometry [G1]
  - metric_space [G2]
```

---

## 4. Birkhoff vs Tarski 独立性对比

mathlib4 的一个核心设计理念是支持两种等价的几何公理化路径：**Birkhoff 路径**（以实数和度量为基础）和 **Tarski 路径**（以纯综合原语为基础）。Lv-00 的 `euclidean_plane` 采用 Tarski 风格。以下是两种路径的全面对比。

### 4.1 原语对比

| 维度 | Birkhoff (1932) | Tarski (1959) -- Lv-00 采用 |
|------|-----------------|---------------------------|
| **基础数学对象** | 实数系 R（已假定完备有序域） | 无。仅接受一阶逻辑 |
| **几何原语** | Point, Line, 距离函数 d, 角度函数 m | Point, Line, B (betweenness), =~= (congruence) |
| **直线上点的顺序** | 由实数坐标推导 | 由 B 公理直接公理化 |
| **线段度量** | 距离函数 d : Point x Point -> R_>=0 | 全等关系 =~= : Segment -> Segment -> Prop |
| **角度度量** | 角度函数 m : Angle -> [0, 180] | 无需角度原语，由五段公理推导 |
| **连续性** | 由 R 的完备性继承 | Dedekind 分割公理（二阶）或公理模式（一阶） |
| **逻辑阶** | 二阶（预设 R 的完备性） | 一阶（公理模式版本）或二阶（Dedekind 版本） |
| **平行公理** | Playfair（等价的 Euclid 第五公设） | Playfair（仅依赖 Layer 0） |

### 4.2 独立性矩阵

下表对比两种公理化中各公理的独立性。打勾表示该公理**不依赖**于同一体系中其他公理即可独立成立。

| 公理 / 性质 | Birkhoff 体系 | Tarski 体系 (Lv-00) | 备注 |
|-------------|-------------|-------------------|------|
| Ruler Postulate (直线上点与 R 一一对应) | 公理，不可独立移除 | 不由单独公理表达，隐含在 Dedekind 中 | Birkhoff 将连续性"外包"给 R |
| Protractor Postulate (角度度量) | 公理 | 由 Five-Segment Axiom 推导 | Tarski 更精简 |
| SAS 全等判定 | 可证明为定理 | 等价于 Five-Segment Axiom | Tarski 中可作为公理或定理 |
| I1-I3 (关联) | 独立 | 独立 | 两者一致 |
| B1-B4 (顺序) | 由实数的全序继承 | 独立公理 | Tarski 中 B4 (Pasch) 独立于 B1-B3 |
| C1-C3 (全等) | 由距离函数继承 | 独立公理 | C3 独立于 C1-C2 |
| Continuity | 由 R 的 Dedekind 完备性保证 | 独立于前 3 层 | Tarski 中可省略为一阶公理模式 |
| Playfair | 独立 | 独立 | 两种体系等价 |
| 坐标系存在性 (R^2 模型) | 内置 | 作为定理证明 | Tarski 需要 Dedekind 才可证 R^2 是模型 |

### 4.3 公理数量对比

| 体系 | 关联 | 顺序 | 全等 | 连续性 | 平行 | 总计 |
|------|------|------|------|--------|------|------|
| Birkhoff (1932) | 3 | 0 (由 R 继承) | 0 (由 d, m 继承) | 0 (由 R 继承) | 1 | **4** |
| Tarski (1959, Lv-00) | 3 | 4 | 3 (+ FiveSeg 隐含) | 1 (Dedekind) | 1 | **11+1** |
| Hilbert (1899) | 8 | 4 | 6 | 2 | 1 | **21** |

**要点**：Tarski 在公理数量上多于 Birkhoff，但逻辑阶更低（一阶 vs 二阶），更适合计算机形式化。Lv-00 的 Tarski 选择体现了 mathlib4 的"最小化原语 + 最大化推导"哲学。

### 4.4 等价性桥梁

在 Lv-00 中，Birkhoff 和 Tarski 体系可以通过以下桥梁互译：

```
Birkhoff 距离函数 d(A, B)  ⟺  AB =~= CD 当且仅当 d(A, B) = d(C, D)
Birkhoff 介于关系           ⟺  B(A, B, C) 当且仅当 d(A, C) = d(A, B) + d(B, C)
Birkhoff 连续性 (R)         ⟺  Euclid Dedekind 公理
```

**关键等价定理（在 Lv-00 中待证明）**：
- `birkhoff_to_tarski`: 任何满足 Birkhoff 公理的模型自动满足 Tarski 公理
- `tarski_to_birkhoff`: 任何满足 Tarski 公理（含连续性和平行公理）的模型同构于 R^2，从而满足 Birkhoff 公理

---

## 5. 公理间等价性矩阵

下表展示 Lv-00 中哪些公理包可以（在适当的桥接下）相互推导。"可以推导"意味着存在一个保结构的翻译（interpretation）使得一个包的所有定理在另一个包中成立。

### 5.1 几何包等价性

| | euclidean_plane | hyperbolic | elliptic | projective | affine | metric_space | differential |
|---|---|---|---|---|---|---|---|
| **euclidean_plane [G3]** | --- | **互斥** (平行公理矛盾) | 互斥 (平行公理矛盾) | **✗** 无介于/全等 | **✗** 无全等/连续性 | ✗ (距离函数可引入但无几何结构) | ✗ (需分析基础) |
| **hyperbolic [G3]** | 互斥 | --- | 互斥 | ✗ | ✗ | ✗ | ✗ |
| **elliptic [G3]** | 互斥 | 互斥 | --- | ✗ (椭圆几何无平行概念) | ✗ | ✗ | ✗ |
| **projective [G1]** | ✗ | ✗ | **部分可嵌入** (RP^2) | --- | **可约化** (仿射是射影的 affine patch) | ✗ | ✗ |
| **affine [G1]** | ✗ | ✗ | ✗ | **可嵌入** (射影化) | --- | ✗ | ✗ |
| **metric_space [G2]** | ✗ | ✗ | ✓ (Cayley-Klein 度量) | ✗ | ✗ | --- | ✗ |
| **differential [G3]** | **可约化** (R^2 是光滑流形) | ✓ (Poincare 盘是光滑流形) | ✓ | ✗ | ✗ | ✓ | --- |

### 5.2 基础包等价性

| | zfc | nbg | category_theory | type_theory | hott |
|---|---|---|---|---|---|
| **zfc_set_theory** | --- | **等价** (NBG 是 ZFC 的保守扩张) | ✗ (需额外公理) | ✗ (不同基础) | ✗ |
| **nbg_set_theory** | **等价** (对集合定理) | --- | ✗ | ✗ | ✗ |
| **category_theory** | **可模拟** (Grothendieck 宇宙) | 可模拟 | --- | ✗ | ✗ |
| **dependent_type_theory** | **可模拟** (Aczel 译码) | 可模拟 | **部分对应** (CCC) | --- | **可扩展** |
| **homotopy_type_theory** | ✗ (HoTT 比 ZFC 弱) | ✗ | **部分对应** (∞-范畴) | 可扩展 | --- |

### 5.3 逻辑包等价性

| | CPL | IPL | IPL (命题) | modal | linear |
|---|---|---|---|---|---|
| **classical_prop_logic (CPL)** | --- | **包含** (CPL 是 IPL 的扩张) | 包含 | **可约化** (S5 可嵌入 CPL) | ✗ |
| **intuitionistic_logic** | ✗ (不可逆) | --- | **包含** (命题是子集) | ✗ | ✗ |
| **intuitionistic_prop_logic** | 等价于 CPL 的 ¬¬ 片段 | 包含 | --- | ✗ | ✗ |
| **modal_logic** | S5 可嵌入 CPL | ✗ | ✗ | --- | ✗ |
| **linear_logic** | ✗ | ✗ | ✗ | ✗ | --- |

### 5.4 代数包等价性

| | group | ring | field | lattice | boolean | lie | universal |
|---|---|---|---|---|---|---|---|
| **group_theory** | --- | **可嵌入** (加法群) | 可嵌入 | ✗ | ✗ | 可嵌入 (Lie 群的底群) | **被泛化** |
| **ring_theory** | 包含 (加法群) | --- | **可扩展** (添加乘法逆元) | ✗ | ✗ | ✗ | 被泛化 |
| **field_theory** | 包含 | 包含 | --- | ✗ | ✗ | ✗ | 被泛化 |
| **lattice_theory** | ✗ | ✗ | ✗ | --- | **可扩展** (添加补运算) | ✗ | 被泛化 |
| **boolean_algebra** | ✗ | ✗ | ✗ | 包含 | --- | ✗ | 被泛化 |
| **lie_theory** | 包含 | ✗ | ✗ | ✗ | ✗ | --- | 被泛化 |
| **universal_algebra** | **泛化** | 泛化 | 泛化 | 泛化 | 泛化 | 泛化 | --- |

### 5.5 算术包等价性

| | robin | peano | second_order | presburger |
|---|---|---|---|---|
| **robin_arithmetic** | --- | **被包含** (PA = Q + 归纳) | 被包含 | ✗ (不相交) |
| **peano_arithmetic** | 包含 | --- | **被包含** (Z₂ 更强) | 互斥 (Presburger 中乘法不可定义) |
| **second_order_arithmetic** | 包含 | 包含 | --- | ✗ |
| **presburger_arithmetic** | ✗ | 互斥 | ✗ | --- |

---

## 6. 从最小公理集到完整几何 —— 依赖链路图

本节展示如何从最精简的公理包出发，逐步加载层级，最终构建完整的几何体系。这是 mathlib4 `IncidenceGeometry` → `EuclideanGeometry` 类型类攀升的 Lv-00 对应。

### 6.1 几何链路 (Tarski 路径, Lv-00 核心)

```
Level 0: 纯逻辑基础
┌─────────────────────────────────────────────┐
│ classical_propositional_logic (CPL)          │
│   + 命题逻辑的3条公理 (LK, LS, LC) + MP      │
│   = 一阶逻辑推理框架                          │
└───────────────┬─────────────────────────────┘
                │ (可选: intuitionistic_logic)
                ▼
Level 1: 集合论基础 (可选, 仅在需要集合论构造时加载)
┌─────────────────────────────────────────────┐
│ zfc_set_theory (或 nbg_set_theory)           │
│   + 9 条 ZFC 公理                            │
│   = 标准的数学基础, 提供集合、函数、关系概念    │
└─────────────────────────────────────────────┘
                │
                ▼
┌────────────────────────────────────────────────────────────────┐
│                  几何构建开始                                    │
│                                                                │
│  Step 1: 加载 Incidence (Layer 0)                              │
│  ┌─────────────────────────────────────────────────────┐       │
│  │ euclidean.I1, I2, I3                                │       │
│  │   → IncidenceGeometry [G1]                          │       │
│  │   可证: 两点确定唯一直线, 存在不共线三点              │       │
│  └─────────────────────────┬───────────────────────────┘       │
│                            │                                   │
│  Step 2: 加载 Order (Layer 1)                                  │
│  ┌─────────────────────────▼───────────────────────────┐       │
│  │ euclidean.B1, B2, B3, B4                            │       │
│  │   → OrderedGeometry [G2]                            │       │
│  │   可证: 直线上点的顺序, Pasch 公理, 线段的内部/外部    │       │
│  └─────────────────────────┬───────────────────────────┘       │
│                            │                                   │
│  Step 3: 加载 Congruence (Layer 2)                             │
│  ┌─────────────────────────▼───────────────────────────┐       │
│  │ euclidean.C1, C2, C3 (+ FiveSeg 隐含)               │       │
│  │   → MetricGeometry [G2+]                            │       │
│  │   可证: SAS 全等判定, 等腰三角形性质, 外角定理        │       │
│  └─────────────────────────┬───────────────────────────┘       │
│                            │                                   │
│  Step 4: 加载 Continuity (Layer 3)                             │
│  ┌─────────────────────────▼───────────────────────────┐       │
│  │ euclidean.Dedekind                                  │       │
│  │   → 完备几何 (无"洞"的直线)                          │       │
│  │   可证: 线-圆交点存在性, 圆的相交判定                │       │
│  └─────────────────────────┬───────────────────────────┘       │
│                            │                                   │
│  Step 5: 选择平行公理 (Layer 4)                                │
│  ┌─────────────────────────▼───────────────────────────┐       │
│  │ ┌─────────────────────┐  ┌─────────────────────────┐│       │
│  │ │ euclidean.Playfair  │  │ hyperbolic.Lobachevsky  ││       │
│  │ │ → EuclideanGeometry │  │ → HyperbolicGeometry    ││       │
│  │ │    [G3]             │  │    [G3]                ││       │
│  │ │ 三角形内角和 = 180°  │  │ 三角形内角和 < 180°     ││       │
│  │ └─────────────────────┘  └─────────────────────────┘│       │
│  └─────────────────────────────────────────────────────┘       │
│                                                                │
│  ★ 不选 Layer 4: 得到 Absolute Geometry (绝对几何)              │
│    可证: Saccheri-Legendre 定理 (内角和 ≤ 180°)               │
│  ★ 仅 Step 1-3, 省略 Dedekind: Tarski 初等几何                 │
│    可证: 绝大多数初等几何定理 (相似三角形等除外)                 │
└────────────────────────────────────────────────────────────────┘
```

### 6.2 可选替代路径

```
替代路径 A: Birkhoff 路径 (基于实数)
  zfc → real_analysis → metric_space → (定义几何) → [G3]

替代路径 B: 群论路径 (Erlangen 纲领)
  zfc → group_theory → (变换群定义几何) → [G1~G3]

替代路径 C: 射影-仿射-度量路径
  projective_geometry [G1] → affine_geometry [G1] → (添加度量) → [G3]
```

### 6.3 最小依赖树 (minimal deps to reach G3)

```
euclidean_plane [G3]
  ├── incidence       [G1]   (0 依赖)
  ├── order           [G2]   (1 依赖: incidence)
  ├── congruence      [G2+]  (2 依赖: incidence, order)
  ├── continuity      [G2+]  (2 依赖: incidence, order)
  └── parallels       [G3]   (1 依赖: incidence)

hyperbolic_geometry [G3]
  ├── (复用 euclidean 0-3) [G2+]
  └── hyperbolic_parallel    [G3] (1 依赖: incidence)

elliptic_geometry [G3]  (独立体系, 0 依赖)

differential_geometry [G3]
  └── real_analysis [G0] → field_theory → ring_theory → group_theory
```

---

## 7. mathlib4 可表达性分析

### 7.1 mathlib4 类型类与 Lv-00 公理包对应表

| mathlib4 类型类 / 结构 | Lv-00 对应 | 等级 | 说明 |
|------------------------|-----------|------|------|
| `IncidenceGeometry` | `euclidean_plane` (仅 Layer 0) | G1 | Point, Line, on |
| `IncidenceGeometry` | `projective_geometry` | G1 | 等价于关联几何 |
| `OrderedGeometry` | `euclidean_plane` (Layer 0+1) | G2 | + betweenness |
| `MetricGeometry` / `TarskiGeometry` | `euclidean_plane` (Layer 0+1+2) | G2 | + congruence |
| `EuclideanGeometry` | `euclidean_plane` (全部 5 层) | G3 | + continuity + Playfair |
| `HyperbolicGeometry` | `hyperbolic_geometry` | G3 | + hyperbolic parallel |
| `AbsoluteGeometry` | `euclidean_plane` (Layer 0-3) | G2+ | 无平行公理 |
| `ProjectivePlane` | `projective_geometry` | G1 | Desargues + Pappus |
| `AffinePlane` | `affine_geometry` | G1 | 关联 + 平行 |
| `MetricSpace` | `metric_space` | G2 | 距离函数 |
| `SmoothManifold` | `differential_geometry` | G3 | 需要 real_analysis |
| `SmoothManifold` (Synthetic) | `synthetic_differential_geometry` | G0 | 需要 topos_theory |
| `Group` | `group_theory` | G0 | 代数 |
| `Ring` / `Field` | `ring_theory` / `field_theory` | G0 | 代数层级 |
| `BoolAlg` / `HeytingAlg` | `boolean_algebra` / `intuitionistic_prop_logic` | G0 | 逻辑代数 |
| `TopologicalSpace` | `point_set_topology` | G0 | 拓扑 |

### 7.2 Lv-00 对 mathlib4 的 5 点借鉴

| # | mathlib4 做法 | Lv-00 实现 | 状态 |
|---|--------------|-----------|------|
| 1 | **IncidenceGeometry 类型类**：定义一次，到处使用 | Layer 0 (incidence) 是所有几何包的公共基础 | 已实现 |
| 2 | **公理独立性追踪**：`#where` 标注每个定理的公理依赖 | `manifest.json` 中 `depends_on` 字段 | 已实现 |
| 3 | **Tarski vs Birkhoff 双路径**：两种等价公理化共存 | `euclidean_plane` (Tarski)；`real_analysis` + `metric_space` (可组合出 Birkhoff 路径) | 部分实现 |
| 4 | **SyntheticGeometry**：免坐标推理 | 所有几何包均使用抽象原语 | 已实现 |
| 5 | **类型安全的几何关系**：`∠ A B C : Angle` | `lvz` 格式支持带类型的谓词 `on(A:Point, l:Line)` | 已实现 |

### 7.3 待对齐项 (Future Work)

| 优先级 | mathlib4 能力 | Lv-00 差距 | 建议 |
|--------|-------------|-----------|------|
| **高** | `EuclideanGeometry` 类型类自动推导三角学定理 | 三角学尚未形式化 | 扩展 `euclidean_plane` 添加角度原语和三角学模板 |
| **高** | `Sphere` 和 `Circle` 类型 | 圆和球的基本性质未形式化 | 新建 `circle_geometry.lvz` |
| **中** | `Tactic` 层：`nlinarith` 代数简化 | 缺少符号计算引擎 | 考虑添加 CAS 后端 |
| **中** | `Birkhoff` 风格的 `dist` 原语 | 距离函数未直接用于几何推理 | 在 `metric_space` 和 `euclidean_plane` 间建立桥接公理 |
| **低** | `Manifold` 类型类的 `ModelWithCorners` | 流形理论仅初步公理化 | 扩展 `differential_geometry` 添加图册和转移映射 |

---

## 附录 A: 公理包完整索引

| # | 包名 | 分类 | G等级 | 依赖包数 | 主要依赖 |
|---|------|------|-------|---------|---------|
| 1 | `euclidean_plane` | geometry | G3 | 0 | - |
| 2 | `hyperbolic_geometry` | geometry | G3 | 4 | euclidean(0-3层) |
| 3 | `elliptic_geometry` | geometry | G3 | 0 | - |
| 4 | `projective_geometry` | geometry | G1 | 0 | - |
| 5 | `affine_geometry` | geometry | G1 | 0 | - |
| 6 | `differential_geometry` | geometry | G3 | 1 | real_analysis |
| 7 | `algebraic_geometry` | geometry | G0 | 2 | ring_theory, field_theory |
| 8 | `zfc_set_theory` | foundations | G0 | 0 | - |
| 9 | `nbg_set_theory` | foundations | G0 | 0 | - |
| 10 | `category_theory` | foundations | G0 | 0 | - |
| 11 | `descriptive_set_theory` | foundations | G0 | 2 | zfc, point_set_topology |
| 12 | `domain_theory` | foundations | G0 | 2 | point_set_topology, lattice_theory |
| 13 | `computability_theory` | foundations | G0 | 0 | - |
| 14 | `computational_complexity_theory` | foundations | G0 | 1 | computability_theory |
| 15 | `robin_arithmetic` | foundations | G0 | 0 | - |
| 16 | `peano_arithmetic` | foundations | G0 | 0 | - |
| 17 | `second_order_arithmetic` | foundations | G0 | 0 | - |
| 18 | `presburger_arithmetic` | foundations | G0 | 0 | - |
| 19 | `group_theory` | algebra | G0 | 0 | - |
| 20 | `ring_theory` | algebra | G0 | 1 | group_theory |
| 21 | `field_theory` | algebra | G0 | 1 | ring_theory |
| 22 | `lattice_theory` | algebra | G0 | 0 | - |
| 23 | `boolean_algebra` | algebra | G0 | 1 | lattice_theory |
| 24 | `lie_theory` | algebra | G0 | 1 | group_theory |
| 25 | `universal_algebra` | algebra | G0 | 0 | - |
| 26 | `homological_algebra` | algebra | G0 | 3 | category, group, ring |
| 27 | `order_theory` | algebra | G0 | 2 | zfc, lattice_theory |
| 28 | `galois_theory` | algebra | G0 | 1 | field_theory |
| 29 | `real_analysis` | analysis | G0 | 2 | field_theory, zfc |
| 30 | `functional_analysis` | analysis | G0 | 2 | real_analysis, point_set_topology |
| 31 | `measure_theory` | analysis | G0 | 3 | real_analysis, zfc |
| 32 | `probability_theory` | analysis | G0 | 1 | measure_theory |
| 33 | `ergodic_theory` | analysis | G0 | 3 | measure_theory, real_analysis, zfc |
| 34 | `classical_propositional_logic` | logic | G0 | 0 | - |
| 35 | `intuitionistic_logic` | logic | G0 | 0 | - |
| 36 | `intuitionistic_propositional_logic` | logic | G0 | 0 | - |
| 37 | `modal_logic` | logic | G0 | 1 | classical_prop_logic |
| 38 | `linear_logic` | logic | G0 | 0 | - |
| 39 | `simple_type_theory` | logic | G0 | 0 | - |
| 40 | `dependent_type_theory` | logic | G0 | 1 | simple_type_theory |
| 41 | `homotopy_type_theory` | logic | G0 | 1 | dependent_type_theory |
| 42 | `proof_theory` | logic | G0 | 0 | - |
| 43 | `model_theory` | logic | G0 | 1 | zfc |
| 44 | `metric_space` | topology | G2 | 0 | - |
| 45 | `point_set_topology` | topology | G0 | 1 | zfc |
| 46 | `algebraic_topology` | topology | G0 | 2 | point_set_topology, group_theory |
| 47 | `number_theory` | number_theory | G0 | 1 | ring_theory |
| 48 | `combinatorics` | discrete_math | G0 | 0 | - |
| 49 | `game_theory` | discrete_math | G0 | 0 | - |
| 50 | `information_theory` | discrete_math | G0 | 0 | - |
| 51 | `graph_theory` | discrete_math | G0 | 2 | zfc, combinatorics |
| 52 | `cartesian_closed_category` | foundations | G0 | 0 | - |
| 53 | `quantum_information_theory` | discrete_math | G0 | 0 | - |
| 54 | `topos_theory` | foundations | G0 | 1 | cartesian_closed_category |
| 55 | `linear_algebra` | algebra | G0 | 2 | field_theory, group_theory |
| 56 | `synthetic_differential_geometry` | geometry/category | G0 | 3 | differential_geometry, category_theory, topos_theory |

---

## 附录 B: 符号约定

| 符号 | 含义 |
|------|------|
| [G0] | 非几何包 |
| [G1] | 关联几何——定义了点、线和 `on` 关系 |
| [G2] | 度量几何——添加了距离或介于关系 |
| [G3] | 完整几何——包含连续性、平行公理或等价结构 |
| `→` | 可推导 / 可约化 （该包的公理在目标包中可作为定理证明） |
| `↔` | 相互可推导 （等价公理化） |
| `⊃` | 严格包含 （目标包是本包的保守扩张） |
| `✗` | 不可推导 / 不相交 |
| `互斥` | 公理相互矛盾，无法同时成立 |

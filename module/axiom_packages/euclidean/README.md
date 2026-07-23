# Euclidean Plane Geometry -- 欧氏平面几何公理包

## 概述 / Overview

本包提供 **Tarski 风格** 的欧氏平面几何公理体系，按逻辑依赖关系分为十层。分层设计借鉴 **GeoCoq** 项目的分层架构，允许逐层加载，支持独立性分析和几何变体切换。

## 分层结构 / Layer Structure

```
===========================================================================
  Layer 0: Incidence      关联公理 (I1, I2, I3)
    |       点在直线上、两点确定唯一直线、存在不共线三点
    |
  Layer 1: Order          顺序公理 (B1, B2, B3, B4)
    |       介于关系 (betweenness)、线段划分、Pasch 公理
    |
  Layer 2: Congruence     全等公理 (C1, C2, C3)
    |       线段全等、全等关系的传递与唯一性
    |
  Layer 3: Continuity     连续性公理 (Dedekind)
    |       直线完备性、Dedekind 分割公理
    |
  Layer 4: Parallels      平行公理 (Playfair)
            过直线外一点恰有一条平行线
===========================================================================
```

---

## Layer 0: Incidence Axioms -- 关联公理

**依赖**: 无 (最底层)

关联公理描述**点（point）**和**直线（line）**之间的基本从属关系。

### I1 -- 两点关联公理 (Two-Point Incidence Axiom)

**命名**: `euclidean.I1`

**形式化陈述**:
```
forall A B : Point,
  A != B ->
  exists! l : Line, on(A, l) /\ on(B, l)
```

**中文**: 对于任意两个不同的点 A 和 B，存在唯一一条直线 l，使得 A 和 B 都在 l 上。

**直觉**: 两点确定唯一直线。

### I2 -- 直线上两点存在公理 (Line Points Existence)

**命名**: `euclidean.I2`

**形式化陈述**:
```
forall l : Line,
  exists A B : Point,
    A != B /\ on(A, l) /\ on(B, l)
```

**中文**: 每条直线上至少存在两个不同的点。

**直觉**: 直线不是退化的单点。

### I3 -- 非共线三点公理 (Non-Collinearity Axiom)

**命名**: `euclidean.I3`

**形式化陈述**:
```
exists A B C : Point,
  ~(exists l : Line, on(A, l) /\ on(B, l) /\ on(C, l))
```

**中文**: 存在三个不共线的点。

**直觉**: 平面上至少有真正的三角形（非退化）。

### 独立性注记 / Independence Notes

- I1, I2, I3 三者相互独立（可各自用反模型单独证明独立性）
- I1 和 I2 足以定义"关联几何"（incidence geometry）的最小模型
- I3 排除了一维直线模型和零维点模型

---

## Layer 1: Order Axioms -- 顺序公理

**依赖**: Layer 0 (Incidence)

顺序公理引入三元关系 **B(A, B, C)**（读作："B 在 A 和 C 之间"），刻画直线上点的顺序。

### B1 -- 对称性公理 (Betweenness Symmetry)

**命名**: `euclidean.B1`

**形式化陈述**:
```
forall A B C : Point,
  B(A, B, C) -> B(C, B, A)
```

**中文**: 如果 B 在 A 和 C 之间，则 B 也在 C 和 A 之间。

**直觉**: 介于关系在两端对称。

### B2 -- 两端点存在公理 (Betweenness Extension)

**命名**: `euclidean.B2`

**形式化陈述**:
```
forall A B : Point,
  A != B ->
  exists C : Point, B(A, B, C)
```

**中文**: 对于任意两个不同的点 A 和 B，存在点 C 使得 B 在 A 和 C 之间。

**直觉**: 任意线段可以向外延伸。

### B3 -- 唯一性公理 (Betweenness Uniqueness)

**命名**: `euclidean.B3`

**形式化陈述**:
```
forall A B C : Point,
  B(A, B, C) ->
  ~(B(B, A, C)) /\ ~(B(A, C, B))
```

**中文**: 三点中至多有一点在另外两点之间。

### B4 -- Pasch 公理 (Pasch's Axiom)

**命名**: `euclidean.B4`

**形式化陈述**:
```
forall A B C P Q : Point,
  ~(collinear(A, B, C)) /\ B(A, P, B) ->
  (B(C, Q, A) \/ B(B, Q, C))
```

**中文** (Tarski 形式): 若 A, B, C 不共线，且 P 在 A 和 B 之间，则直线 CP 必与三角形 ABC 的某条边相交。

**直觉**: 直线进入三角形的内部必然从某条边出来。

### 独立性注记 / Independence Notes

- B4 (Pasch 公理) 是顺序公理中最强的，独立于 B1-B3
- 在 Tarski 体系中，B1-B4 不直接使用"内部"概念，所有表述都是纯介于关系
- O1-O4 在 Hilbert 体系中大致对应，但 B4 (Pasch) 在 Hilbert 体系中需用平面分离公理补充

---

## Layer 2: Congruence Axioms -- 全等公理

**依赖**: Layer 0 (Incidence), Layer 1 (Order)

全等公理引入四元关系 **AB =~= CD**（读作："线段 AB 全等于线段 CD"），用于度量线段和非角度比较。

### C1 -- 线段构造公理 (Segment Construction)

**命名**: `euclidean.C1`

**形式化陈述**:
```
forall A B C D : Point,
  A != B ->
  exists! E : Point,
    B(C, D, E) \/ B(C, E, D) /\ AB =~= CE
```

**中文**: 从任意点 C 出发，在射线 CD 上可以构造唯一一点 E，使得线段 CE 全等于线段 AB。

**直觉**: 可以用圆规在任意直线上截取等长线段（等价于圆规公理）。

### C2 -- 全等传递公理 (Congruence Transitivity)

**命名**: `euclidean.C2`

**形式化陈述**:
```
forall A B C D E F : Point,
  AB =~= CD /\ AB =~= EF -> CD =~= EF
```

**中文**: 如果线段 AB 全等于 CD，且 AB 也全等于 EF，则 CD 全等于 EF。

**直觉**: 全等是等价关系（传递性）。

### C3 -- 加法保持公理 (Segment Addition)

**命名**: `euclidean.C3`

**形式化陈述**:
```
forall A B C A' B' C' : Point,
  B(A, B, C) /\ B(A', B', C') /\
  AB =~= A'B' /\ BC =~= B'C' ->
  AC =~= A'C'
```

**中文**: 若 B 在 A, C 之间，B' 在 A', C' 之间，且 AB =~= A'B', BC =~= B'C'，则 AC =~= A'C'。

**直觉**: 全等线段的和保持全等。

### 独立性注记 / Independence Notes

- C3 独立于 C1, C2（可用椭圆几何的反模型证明）
- 在 Tarski 体系中，角度全等可由线段全等通过**五段公理**（Five-Segment Axiom）推导，无需额外角度公理
- GeoCoq 中全等公理有更细的拆分（C1-C6），此处采用 Tarski 精简版（C1-C3 + 五段公理）

---

## Layer 3: Continuity Axiom -- 连续性公理

**依赖**: Layer 0 (Incidence), Layer 1 (Order)

连续性公理确保直线的"完备性"——直线上没有"洞"。

### Dedekind -- Dedekind 分割公理 (Dedekind Cut Axiom)

**命名**: `euclidean.Dedekind`

**形式化陈述**:
```
forall l : Line, (P, Q : Point -> Prop),
  partition(l, P, Q) /\
  (exists X, P(X)) /\
  (exists Y, Q(Y)) /\
  (forall X Y, P(X) /\ Q(Y) -> B(X, l, Y)) ->
  exists Z : Point, on(Z, l) /\
    (forall X : Point, P(X) /\ X != Z -> B(X, Z, Y)) /\
    (forall Y : Point, Q(Y) /\ Y != Z -> B(Z, Y, X))
```

**中文**: 若直线 l 被划分为两个非空子集 P 和 Q，且 P 中每点都在 Q 中每点的"左侧"，则存在唯一点 Z 作为分割点。

**直觉**: 实数直线是完备的，每个分割都有确定的分割点。

### 替代形式 / Alternative Forms

| 形式 | 名称 | 等价性 |
|------|------|--------|
| Dedekind | Dedekind 分割 | 本包采用 |
| Line-Circle | 线-圆连续性 | 与 Dedekind 在关联+顺序下等价 |
| Circle-Circle | 圆-圆连续性 | 比 Dedekind 弱 |
| Archimedean | Archimedes 公理 | 比 Dedekind 弱 |
| Cauchy | Cauchy 完备性 | 等价于 Dedekind（在一阶逻辑中需公理模式） |

### 独立性注记 / Independence Notes

- 连续性公理独立于前 2 层（前 2 层在有理数模型中也成立）
- 去掉连续性公理得到"Tarski 几何"（Tarski geometry without continuity），可处理大部分初等几何问题
- 连续性公理是二阶逻辑公理（涉及集合量化），在一阶公理化中需用**公理模式**（axiom schema）替代

---

## Layer 4: Parallel Axiom -- 平行公理

**依赖**: Layer 0 (Incidence)

平行公理是欧氏几何区别于其他几何的标志性公理。

### Playfair -- Playfair 平行公理 (Playfair's Axiom)

**命名**: `euclidean.Playfair`

**形式化陈述**:
```
forall A : Point, l : Line,
  ~on(A, l) ->
  exists! m : Line,
    on(A, m) /\ ~(exists X : Point, on(X, l) /\ on(X, m))
```

**中文**: 过直线外一点，恰有一条直线与给定直线平行（不相交）。

**直觉**: 平行线是唯一的，这是欧氏平面的本质特征。

### 等价形式 / Equivalent Forms

| 公理 | 等价关系 | 说明 |
|------|---------|------|
| Playfair | 标准（本包采用） | 过一点恰有一条平行线 |
| Euclid 第五公设 | 等价于 Playfair（在绝对几何中） | 同旁内角和小于二直角则相交 |
| 三角形内角和 = 180deg | 等价 | 更直观但需角度概念 |
| Legendre 定理 | 等价 | 存在一个三角形内角和为 180deg |
| Wallis 公理 | 等价 | 存在任意大的相似三角形 |

### 独立性注记 / Independence Notes

- 平行公理**独立于前 4 层**：存在满足前 4 层但不满足平行公理的模型（双曲几何）
- 替换本层即可从欧氏几何切换到其他几何体系
- Playfair 仅依赖 Layer 0 (Incidence)，是最"轻量"的平行公理

---

## 完整公理列表 / Complete Axiom Index

| 层 | 公理 ID | 命名空间名 | 中文名 | 依赖层 | 独立性 |
|----|---------|-----------|--------|--------|--------|
| 0 | I1 | `euclidean.I1` | 两点关联公理 | - | 独立 |
| 0 | I2 | `euclidean.I2` | 直线上两点存在公理 | - | 独立 |
| 0 | I3 | `euclidean.I3` | 非共线三点公理 | - | 独立 |
| 1 | B1 | `euclidean.B1` | 介于对称性公理 | 0 | 独立 |
| 1 | B2 | `euclidean.B2` | 介于延伸公理 | 0 | 独立 |
| 1 | B3 | `euclidean.B3` | 介于唯一性公理 | 0 | 独立 |
| 1 | B4 | `euclidean.B4` | Pasch 公理 | 0 | 独立于 B1-B3 |
| 2 | C1 | `euclidean.C1` | 线段构造公理 | 0, 1 | 独立 |
| 2 | C2 | `euclidean.C2` | 全等传递公理 | 0 | 独立 |
| 2 | C3 | `euclidean.C3` | 加法保持公理 | 0, 1 | 独立于 C1-C2 |
| 3 | Dedekind | `euclidean.Dedekind` | Dedekind 分割公理 | 0, 1 | 独立 |
| 4 | Playfair | `euclidean.Playfair` | Playfair 平行公理 | 0 | 独立于 0-3 层 |

---

## 参考文献 / References

1. **Tarski, A.** (1959). "What is Elementary Geometry?" In *The Axiomatic Method* (pp. 16-29). North-Holland.
   -- 提出 Tarski 几何公理体系，本包的主要理论来源。

2. **GeoCoq Project**. [github.com/GeoCoq/GeoCoq](https://github.com/GeoCoq/GeoCoq)
   -- Coq 形式化的几何公理库，提供了分层的公理组织方式。

3. **Hilbert, D.** (1899). *Grundlagen der Geometrie*. Teubner.
   -- 经典几何公理化，本包的 Hilbert 命名渊源。

4. **Schwabhauser, W., Szmielew, W., & Tarski, A.** (1983). *Metamathematische Methoden in der Geometrie*. Springer.
   -- Tarski 几何体系的完整详细论述。

5. **Playfair, J.** (1795). *Elements of Geometry*.
   -- Playfair 平行公理的原始提出。

---

## 使用示例 / Usage Example

```c
// 加载欧氏几何完整包
lv_load_package("euclidean_plane");

// 仅加载绝对几何（前 3 层，不含连续性和平行公理）
lv_load_layer("euclidean_plane", "incidence");
lv_load_layer("euclidean_plane", "order");
lv_load_layer("euclidean_plane", "congruence");

// 检查是否加载了平行公理
if (lv_axiom_loaded("euclidean.Playfair")) {
    // 在欧氏几何中推理
    lv_prove("triangle_angle_sum_180");
} else {
    // 在绝对几何中推理（三角形内角和 <= 180）
    lv_prove("saccheri_legendre");
}
```

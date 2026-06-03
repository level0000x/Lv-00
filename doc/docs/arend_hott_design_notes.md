# Arend/HoTT 类型系统设计笔记

> **借鉴项目**：Arend（JetBrains Research, arend-lang.github.io）
> **核心借鉴点**：路径类型（Path Type）的几何直觉、Interval 类型的模拟、等式证明=路径的元语言表达
> **分类**：P4 低优先级 / 类型系统增强
> **日期**：2026-05-24

---

## 1. 概述

Arend 是一个基于同伦类型论（HoTT）的定理证明器。其核心创新在于将"相等性证明"解释为拓扑空间中的"路径"——两个项 a 和 b 之间的等式 a = b 相当于从 a 到 b 的一条连续路径。这一直觉与 Lv-00 的几何元语言内核天然共振，因为 Lv-00 本身的核心抽象就是几何图上的构造=计算=证明。

本文档探讨如何将 Arend 的路径类型系统和 Interval 类型映射到 Lv-00 的几何元语言框架中，使 Lv-00 的类型系统能够以几何直觉原生的方式表达高阶等价关系。

---

## 2. Arend Path 类型与 Lv-00 几何路径的映射

### 2.1 Arend 的路径类型定义

在 Arend 中，路径类型 `a = b` 的内部表示等价于：

```
Path (A : \Type) (a : A) (b : A) : \Type
  => \Pi (i : I) -> A  such that  i=0 => a, i=1 => b
```

其中 `I` 是区间类型（Interval），包含两个端点 `left` 和 `right` 以及连续性条件。这本质上是一个从单位区间到类型 A 的连续函数——即拓扑意义上的"路径"。

### 2.2 Lv-00 中的映射方案

在 Lv-00 的几何元语言中，我们可以建立以下直接对应：

| Arend/HoTT 概念 | Lv-00 几何元语言映射 | 说明 |
|:---|:---|:---|
| `I` (Interval) | `LINE_SEGMENT` 节点，端点标记为 `left`/`right` | 线段作为"区间"的几何表示 |
| `Path A a b` | `FUNCTION_BLOCK`，输入端口类型为 `I`，输出端口类型为 `A` | 路径是一个从线段到类型空间的映射 |
| `path i` 在 i=0 时等于 a | 函数块应用时，输入端口为线段的左端点 | 端点的几何定位天然对应 |
| 路径组合 `p * q` | 两个函数块的 `Compose` 组合子 | 沿用已有函数块组合子 |
| 路径逆 `p^-1` | 线段的端点翻转 + 函数块重连 | 反转区间方向 |

### 2.3 Interval 类型的模拟

Lv-00 不需要引入独立的 `Interval` 作为第一类类型。替代方案是：

```
Interval  ≡  LINE_SEGMENT(left : POINT, right : POINT)
```

其中 `left` 和 `right` 是两个自由点，其符号坐标尚未被任何约束固定。当一个函数块接受 `Interval` 类型的输入端口时，它实际上接受的是"一条端点自由但满足连续性约束的线段"。

在类型检查层面，`Interval` 类型的等价性由以下规则判定：
- 两个 `Interval` 相等当且仅当它们的端点通过图规范化被合并到相同坐标
- `Interval` 不与其他任何几何类型等价（保证了区间类型的正交性）

---

## 3. HoTT "等式证明=路径"直觉在 Lv-00 几何元语言中的表达

### 3.1 核心洞察

Lv-00 的几何元语言与 HoTT 的路径直觉有一个深层对应：

- **HoTT 中**：证明 `x = y` 意味着构造一条从 `x` 到 `y` 的路径
- **Lv-00 中**：证明命题 P 意味着构造一个函数块，其内部约束图满足命题 P 的模式——这实质上也是一个"从前提（输入端）到结论（输出端）的构造路径"

### 3.2 具体表达：几何构造即路径

在 Lv-00 中，"等式"可以自然地表达为一种特殊的几何约束模式：

```
PointEquality := FUNCTION_BLOCK {
    input:  [p1: POINT, p2: POINT]
    output: [path: LINE_SEGMENT]
    internal: {
        CONSTRAINT(path, INCIDENCE, p1)    // path 经过 p1
        CONSTRAINT(path, INCIDENCE, p2)    // path 经过 p2
    }
    determinant: VERIFIED                   // 线段由两端点唯一确定
}
```

这里，两点的"相等"并非直接声明，而是通过"存在一条连接两点的线段"来表达。这正好对应 HoTT 中"等式是一种路径类型"的直觉。

更进一步，对于类型层面上的等价（而非值的相等），可以表达为：

```
TypeEquivalence := FUNCTION_BLOCK {
    input:  [typeA: TYPE_REGION, typeB: TYPE_REGION]
    output: [f: FUNC_BLOCK(A->B), g: FUNC_BLOCK(B->A)]
    internal: {
        CONSTRAINT(f ∘ g, EQUIV, id_B)     // f∘g = id_B
        CONSTRAINT(g ∘ f, EQUIV, id_A)     // g∘f = id_A
    }
}
```

### 3.3 路径组合的几何化

HoTT 中路径可以组合（concatenation）和反转（inversion）：

- **路径组合** `p * q`：在 Lv-00 中已经由 `Compose` 组合子（`func_block.h` 第 304-305 行）实现——`g ∘ f: A→C` 对应于路径的串联
- **路径反转** `p^-1`：通过交换线段的端点实现。由于 Lv-00 线段是有向的（从第一端点到第二端点），反转等同于创建一个端点顺序相反的新线段

---

## 4. 与 Lv-00 现有类型系统的集成路径

### 4.1 桥接到依赖类型

Lv-00 的依赖类型系统（`Π(x:A).B(x)`，在 `type_system.h` 中定义）与 HoTT 的依赖路径类型有天然联系：

- **依赖路径**：`PathD (A : I -> \Type) (a : A left) (b : A right)` 意味着路径的每一段落在不同的类型中
- **Lv-00 表达**：函数块的输出类型区域由输入值通过确定构造给出（`proof.h` 第 444-445 行的类型等价检查机制）

### 4.2 实现优先级

考虑到 Lv-00 当前 v3.2 的核心设计，HoTT 路径类型的引入建议按以下优先级：

1. **P4-1（本阶段）**：设计文档和概念验证——将 `Interval` 建模为 `LINE_SEGMENT`，并验证在现有约束图框架下的可行性
2. **P4-2（后续）**：在 `type_system.h` 中增加 `TYPE_KIND_PATH` 类型种类，允许将函数块声明为路径类型
3. **P4-3（远期）**：实现 `based path induction`（J 规则）作为预置函数块，使 Lv-00 能够进行路径消除

### 4.3 J 规则的几何化草图

HoTT 的核心消除规则 J 规则可以表达为 Lv-00 的预置函数块：

```
J_elim := FUNCTION_BLOCK {
    input:  [
        A : TYPE_REGION,
        a : POINT (in A),
        P : FUNC_BLOCK(Π(b:A)(Path a b).TYPE_REGION),  // 动机族
        refl_proof : FUNC_BLOCK(P(a, refl_a)),
        b : POINT (in A),
        path : PATH(a, b)
    ]
    output: [result: P(b, path)]
    determinant: VERIFIED
}
```

这个预置函数块不改变底层约束图的拓扑结构，只作为证明搜索的推理规则。其内部实现可以依赖已有的合一检查（`proof_unify`）和图规范化遍（`normalization.h`）。

---

## 5. 设计决策与权衡

### 5.1 不全部采用 HoTT 作为基础

Lv-00 的几何元语言选择在"几何直觉"层面借用 HoTT，但不在内核中使用 HoTT 作为基础类型论。原因：

- Lv-00 的目标用户是几何学家和数学教育者，而非类型论研究者
- 约束图是比路径类型更基础、更通用的抽象
- 在约束图框架下模拟 HoTT 是可行的，而反过来不行

### 5.2 Interval 类型的"惰性"实现

`Interval` 不作为独立的基础类型注册到类型系统中，而是通过"模式匹配"在合一检查阶段识别：
- 当函数块模式检测到输入是长度为 2 的自由线段时，自动将其解释为 `Interval`
- 这避免了类型系统的膨胀，同时保留了 HoTT 直觉的表达能力

### 5.3 与不可构造性证明的关系

HoTT 中某些等式在经典逻辑下不可证明但在 HoTT 中可以通过高维路径（更高阶的同伦）证明。Lv-00 的不可构造性证明系统（`proof.h` 第 674-745 行）需要正确处理这种差异：
- 对于尺规公理包，HoTT 中的高维路径如果不对应于任何尺规构造，则标记为蓝色（未探索）
- 对于允许连续性构造的公理包，高维路径可以作为合法证物

---

## 6. 总结

Arend/HoTT 的路径类型直觉与 Lv-00 的几何元语言具有深层结构同源性。将 `Interval` 建模为 `LINE_SEGMENT`，将路径建模为从线段到类型空间的函数块，是一种在不引入新基础类型的条件下表达高阶等价关系的自然方案。这一方案优先保证与现有约束图框架的兼容性，同时为未来引入完整的 HoTT 推理能力预留了扩展空间。

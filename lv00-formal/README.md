# Lv00Formal - Lv-00 自有几何元语言形式化验证项目

本项目使用 Lean 4 对 **Lv-00 自己的几何元语言体系** 做形式化验证。当前路线已经纠偏：Hilbert 公理体系不再作为主理论，而是迁移到 `Classical/Hilbert/`，仅作为经典几何对照层与后续解释目标。

## 核心路线

根据论文初稿，Lv-00 的证明主线应是：

1. **三元本体**：点、线、域。
2. **六条本原谓词**：关联、之间、相交、包含、连接、等价。
3. **八条基础公理/规则接口**：作为 Lv-00 内部推理系统的规则模板。
4. **约束图**：把对象、谓词和推理结果组织为可归一化结构。
5. **四态相容性**：相容、矛盾、欠约束、过约束。
6. **归一化与多策略推理**：证明归一化保持语义、推理步骤可靠。
7. **证明对象**：输出可追溯、可复核的证明链。
8. **经典解释层**：后续再证明 Lv-00 体系如何解释或推出经典几何性质。

## 项目结构

```text
lv00-formal/
├── Lv00Formal.lean
├── lakefile.toml
├── README.md
└── Lv00Formal/
    ├── Theory/                         # Lv-00 自有理论主线
    │   ├── Ontology/
    │   │   └── Defs.lean               # 三元本体：点、线、域
    │   ├── Predicates/
    │   │   └── Defs.lean               # 六条本原谓词
    │   ├── Axioms/
    │   │   └── Primitive.lean          # 八条基础公理/规则接口
    │   ├── Constraint/
    │   │   └── Graph.lean              # 约束图、四态、归一化目标
    │   ├── Reasoning/
    │   │   └── Soundness.lean          # 推理可靠性接口
    │   └── Proof/
    │       └── Trace.lean              # 可追溯证明对象
    ├── Classical/                       # 外部经典几何对照层
    │   └── Hilbert/
    │       ├── Incidence.lean
    │       ├── Order.lean
    │       ├── Congruence.lean
    │       ├── Parallel.lean
    │       └── Consistency.lean
    ├── Basic/                           # 底层辅助类型
    └── Interop/                         # 与 C 核心互操作验证
```

## 已完成

- [x] 找到并确认论文初稿中的理论方向。
- [x] 停止 Hilbert 主线扩展。
- [x] 将 Hilbert 相关模块迁移为 `Classical/Hilbert` 对照层。
- [x] 建立 Lv-00 自有理论主线骨架。
- [x] 定义三元本体：`LvPoint`、`LvLine`、`LvDomain`。
- [x] 定义六条本原谓词：`incidence`、`between`、`intersection`、`containment`、`connection`、`equivalence`。
- [x] 定义八条基础规则接口：`BaseAxiomKind.A1` 到 `A8`。
- [x] 定义约束图、四态相容性、归一化目标。
- [x] 定义推理步骤与可靠性骨架。
- [x] 定义可追溯证明对象。

## 下一步

1. 继续对照 C 源码中的 `primitive_axioms`、`AxiomRule`、`axiom_package`，把语义化八规则细化为具体可执行前提与结论模板。
2. 对照 C 源码中的 `constraint_graph`、`normalization`、`rewrite`、`unify`、`groebner` 模块，细化 Lean 中的约束图与推理规则。
3. 证明关键元理论：
   - 归一化保持良构性；
   - 归一化幂等性；
   - 单步推理可靠性；
   - 多步证明链可靠性；
   - Lv-00 内部规则到经典几何对照层的解释正确性。
4. 最后再处理 Hilbert/欧氏几何关系：不是直接复刻 Hilbert，而是证明 Lv-00 自有体系可以解释、推出或覆盖相应经典性质。

## 使用示例

```lean
import Lv00Formal

open Lv00Formal

#check LvObj
#check PrimPred
#check BaseAxiomRule
#check ConstraintGraph
#check ReasoningSoundness
#check ProofObject
```

## 当前原则

> Lv-00 的形式化证明应首先证明“我们自己那一套”的内部一致性、可靠性和可解释性；Hilbert/Tarski/欧氏几何只能作为外部参照，不再作为主理论起点。

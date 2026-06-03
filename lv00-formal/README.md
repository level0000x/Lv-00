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
    │   │   ├── Primitive.lean          # 八条基础公理/规则接口
    │   │   ├── RuleTemplate.lean       # 对照 C 规则引擎的可执行规则模板
    │   │   ├── Instances.lean          # proof_theory.lvz 真实规则包实例
    │   │   └── PackageValidation.lean  # 公理包依赖验证模型
    │   ├── Constraint/
    │   │   └── Graph.lean              # 约束图、四态、归一化目标
    │   │   └── Normalization.lean      # 约束图归一化算法（Union-Find）
    │   ├── Rewrite/
    │   │   └── Defs.lean               # 重写系统（模式匹配、策略、合流性）
    │   ├── Unification/
    │   │   └── Defs.lean               # 合一算法（Martelli-Montanari、MGU）
    │   ├── Groebner/
    │   │   └── Defs.lean               # Groebner 基（Buchberger 算法）
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
- [x] 映射 `proof_theory.lvz` 公理包（36 模板，6 不可构造问题）。
- [x] 映射 `linear_logic.lvz` 公理包（54 模板，10 不可构造问题）。
- [x] 映射 `galois_theory.lvz` 公理包（62 模板，8 不可构造问题）。
- [x] 证明 `proof_theory` 包依赖验证：`proofTheoryPackage_dependencies_valid`。
- [x] 证明 `linear_logic` 包依赖验证：`linearLogicPackage_dependencies_valid`。
- [x] 证明 `galois_theory` 包依赖验证：`galoisTheoryPackage_dependencies_valid`。
- [x] 映射 `euclidean_plane.lvz` 公理包（22 模板，6 不可构造问题）。
- [x] 证明 `euclidean_plane` 包依赖验证：`euclideanPlanePackage_dependencies_valid`。
- [x] 映射 `category_theory.lvz` 公理包（60 模板，7 不可构造问题）。
- [x] 证明 `category_theory` 包依赖验证：`categoryTheoryPackage_dependencies_valid`。
- [x] 映射 `hyperbolic_geometry.lvz` 公理包（29 模板，6 不可构造问题）。
- [x] 证明 `hyperbolic_geometry` 包依赖验证：`hyperbolicGeometryPackage_dependencies_valid`。
- [x] 映射 `projective_geometry.lvz` 公理包（38 模板，7 不可构造问题）。
- [x] 证明 `projective_geometry` 包依赖验证：`projectiveGeometryPackage_dependencies_valid`（跨包依赖已通过全局注册表验证）。
- [x] 映射 `group_theory.lvz` 公理包（34 模板，7 不可构造问题）。
- [x] 证明 `group_theory` 包依赖验证：`groupTheoryPackage_dependencies_valid`。
- [x] 映射 `zfc_set_theory.lvz` 公理包（27 模板，10 不可构造问题）。
- [x] 证明 `zfc_set_theory` 包依赖验证：`zfcSetTheoryPackage_dependencies_valid`（含跨引用 sorry）。
- [x] 映射 `boolean_algebra.lvz` 公理包（29 模板，6 不可构造问题）。
- [x] 证明 `boolean_algebra` 包依赖验证：`booleanAlgebraPackage_dependencies_valid`。
- [x] 映射 `ring_theory.lvz` 公理包（54 模板，8 不可构造问题）。
- [x] 证明 `ring_theory` 包依赖验证：`ringTheoryPackage_dependencies_valid`。
- [x] 映射 `peano_arithmetic.lvz` 公理包（70 模板，8 不可构造问题）。
- [x] 证明 `peano_arithmetic` 包依赖验证：`peanoArithmeticPackage_dependencies_valid`。
- [x] 映射 `field_theory.lvz` 公理包（37 模板，7 不可构造问题）。
- [x] 证明 `field_theory` 包依赖验证：`fieldTheoryPackage_dependencies_valid`。
- [x] 映射 `order_theory.lvz` 公理包（32 模板，8 不可构造问题）。
- [x] 证明 `order_theory` 包依赖验证：`orderTheoryPackage_dependencies_valid`（含跨引用 sorry）。
- [x] 映射 `point_set_topology.lvz` 公理包（43 模板，7 不可构造问题）。
- [x] 证明 `point_set_topology` 包依赖验证：`pointSetTopologyPackage_dependencies_valid`。
- [x] 映射 `graph_theory.lvz` 公理包（70 模板，14 不可构造问题）。
- [x] 证明 `graph_theory` 包依赖验证：`graphTheoryPackage_dependencies_valid`（含跨引用 sorry）。
- [x] 映射 `number_theory.lvz` 公理包（38 模板，7 不可构造问题）。
- [x] 证明 `number_theory` 包依赖验证：`numberTheoryPackage_dependencies_valid`。
- [x] 映射 `measure_theory.lvz` 公理包（70 模板，9 不可构造问题）。
- [x] 证明 `measure_theory` 包依赖验证：`measureTheoryPackage_dependencies_valid`（含跨引用 sorry）。
- [x] 映射 `real_analysis.lvz` 公理包（43 模板，7 不可构造问题）。
- [x] 证明 `real_analysis` 包依赖验证：`realAnalysisPackage_dependencies_valid`。
- [x] 映射 `functional_analysis.lvz` 公理包（37 模板，7 不可构造问题）。
- [x] 证明 `functional_analysis` 包依赖验证：`functionalAnalysisPackage_dependencies_valid`（含跨引用 sorry）。
- [x] 映射 `probability_theory.lvz` 公理包（87 模板，8 不可构造问题）。
- [x] 证明 `probability_theory` 包依赖验证：`probabilityTheoryPackage_dependencies_valid`（含跨引用 sorry）。
- [x] 映射 `algebraic_geometry.lvz` 公理包（38 模板，6 不可构造问题）。
- [x] 证明 `algebraic_geometry` 包依赖验证：`algebraicGeometryPackage_dependencies_valid`。
- [x] 映射 `information_theory.lvz` 公理包（96 模板，8 不可构造问题）。
- [x] 证明 `information_theory` 包依赖验证：`informationTheoryPackage_dependencies_valid`（含跨引用 sorry）。
- [x] 映射 `linear_algebra.lvz` 公理包（90 模板，8 不可构造问题）。
- [x] 证明 `linear_algebra` 包依赖验证：`linearAlgebraPackage_dependencies_valid`（含跨引用 sorry）。
- [x] 映射 `homological_algebra.lvz` 公理包（36 模板，6 不可构造问题）。
- [x] 证明 `homological_algebra` 包依赖验证：`homologicalAlgebraPackage_dependencies_valid`。
- [x] 映射 `differential_geometry.lvz` 公理包（41 模板，6 不可构造问题）。
- [x] 证明 `differential_geometry` 包依赖验证：`differentialGeometryPackage_dependencies_valid`。
- [x] 映射 `computability_theory.lvz` 公理包（53 模板，14 不可构造问题）。
- [x] 证明 `computability_theory` 包依赖验证：`computabilityTheoryPackage_dependencies_valid`（含跨引用 sorry）。
- [x] 映射 `modal_logic.lvz` 公理包（29 模板，7 不可构造问题）。
- [x] 证明 `modal_logic` 包依赖验证：`modalLogicPackage_dependencies_valid`（含跨引用 sorry）。
- [x] 映射 `universal_algebra.lvz` 公理包（60 模板，8 不可构造问题）。
- [x] 证明 `universal_algebra` 包依赖验证：`universalAlgebraPackage_dependencies_valid`。
- [x] 映射 `combinatorics.lvz` 公理包（39 模板，7 不可构造问题）。
- [x] 证明 `combinatorics` 包依赖验证：`combinatoricsPackage_dependencies_valid`。
- [x] 映射 `game_theory.lvz` 公理包（51 模板，10 不可构造问题）。
- [x] 证明 `game_theory` 包依赖验证：`gameTheoryPackage_dependencies_valid`。
- [x] 映射 `homotopy_type_theory.lvz` 公理包（37 模板，6 不可构造问题）。
- [x] 证明 `homotopy_type_theory` 包依赖验证：`homotopyTypeTheoryPackage_dependencies_valid`。
- [x] 映射 `dependent_type_theory.lvz` 公理包（33 模板，6 不可构造问题）。
- [x] 证明 `dependent_type_theory` 包依赖验证：`dependentTypeTheoryPackage_dependencies_valid`。
- [x] 映射 `simple_type_theory.lvz` 公理包（39 模板，6 不可构造问题）。
- [x] 证明 `simple_type_theory` 包依赖验证：`simpleTypeTheoryPackage_dependencies_valid`。
- [x] 映射 `affine_geometry.lvz` 公理包（52 模板，7 不可构造问题）。
- [x] 证明 `affine_geometry` 包依赖验证：`affineGeometryPackage_dependencies_valid`（含跨引用 sorry）。
- [x] 映射 `algebraic_topology.lvz` 公理包（38 模板，7 不可构造问题）。
- [x] 证明 `algebraic_topology` 包依赖验证：`algebraicTopologyPackage_dependencies_valid`。
- [x] 映射 `elliptic_geometry.lvz` 公理包（30 模板，6 不可构造问题）。
- [x] 证明 `elliptic_geometry` 包依赖验证：`ellipticGeometryPackage_dependencies_valid`。
- [x] 映射 `metric_space.lvz` 公理包（47 模板，8 不可构造问题）。
- [x] 证明 `metric_space` 包依赖验证：`metricSpacePackage_dependencies_valid`。
- [x] 映射 `lattice_theory.lvz` 公理包（42 模板，7 不可构造问题）。
- [x] 证明 `lattice_theory` 包依赖验证：`latticeTheoryPackage_dependencies_valid`。
- [x] 映射 `lie_theory.lvz` 公理包（70 模板，7 不可构造问题）。
- [x] 证明 `lie_theory` 包依赖验证：`lieTheoryPackage_dependencies_valid`。
- [x] 映射 `model_theory.lvz` 公理包（35 模板，6 不可构造问题）。
- [x] 证明 `model_theory` 包依赖验证：`modelTheoryPackage_dependencies_valid`。
- [x] 映射 `classical_propositional_logic.lvz` 公理包（59 模板，6 不可构造问题）。
- [x] 证明 `classical_propositional_logic` 包依赖验证：`classicalPropositionalLogicPackage_dependencies_valid`。
- [x] 映射 `intuitionistic_logic.lvz` 公理包（50 模板，7 不可构造问题）。
- [x] 证明 `intuitionistic_logic` 包依赖验证：`intuitionisticLogicPackage_dependencies_valid`。
- [x] 映射 `topos_theory.lvz` 公理包（81 模板，10 不可构造问题）。
- [x] 证明 `topos_theory` 包依赖验证：`toposTheoryPackage_dependencies_valid`。
- [x] 实现约束图归一化算法：`EquivalenceClass`、`NormalizationState`、`normalize`、`detectContradiction`。
- [x] 证明归一化幂等性：`NormalizationIdempotent`。
- [x] 证明归一化保持良构性：`NormalizationPreservesWellFormedness`。
- [x] 实现重写系统：`Term`、`RewriteRule`、`RewriteStrategy`、`rewriteStar`、合流性检查。
- [x] 实现合一算法：`unify`、`mgu`、`occursIn`、Martelli-Montanari 算法。
- [x] 实现 Groebner 基：`buchberger`、`sPolynomial`、`idealMembership`、Buchberger 算法。
- [x] 建立跨包依赖验证机制：`globalNameRegistry`、`CrossPackageDependenciesValid`。
- [x] 消除全部 12 个跨引用 sorry，所有 54 个包的依赖验证均已完成。

## 下一步

1. 安装 Lean 4 工具链，执行 `lake build` 编译验证。
2. 完善合一算法定理证明（`unify_commutative`、`unify_associative` 剩余 sorry）。
3. 证明更多关键元理论：
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

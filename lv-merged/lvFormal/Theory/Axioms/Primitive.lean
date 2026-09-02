import lvFormal.Theory.Predicates.Defs

/-
Lv-00 自有理论核心：八条基础规则的形式接口

这里的"基础规则"不是 Hilbert、Tarski 或欧氏公理的翻译，而是根据论文初稿
对 Lv-00 自有体系的描述建立的内部规则模板。它们服务于三元本体、六条本原
谓词、约束图、归一化和多策略推理之间的闭环。

论文中并未把八条基础公理逐条命名为传统数学公理，而是强调它们应当承担：
1. 约束谓词组合；
2. 限定合法结构；
3. 触发推理规则；
4. 支撑冲突检测；
5. 保证推理结论可被约束图与归一化内核吸收。

因此本文件采用"语义化八规则接口"：每条规则先对应一个独立职责，后续可继续
依据 primitive_axioms、AxiomRule、axiom_package 与 constraint_graph 源码细化为
具体可执行规则。
-/

namespace lvFormal
namespace Theory
namespace Axioms

open Ontology
open Predicates

/-- Lv-00 八条基础规则的语义化分类。

这些分类不是外部几何公理，而是 Lv-00 内部规则层的职责划分：
- `objectClosure`：对象层封闭规则，保证对象来自点、线、域三元本体。
- `predicateTyping`：谓词类型规则，保证六条本原谓词的参数数量与对象类型合法。
- `incidenceRegistration`：关联注册规则，把点线/对象归属关系纳入约束图。
- `betweenPropagation`：之间传播规则，使“之间”关系能触发相关关联与顺序约束。
- `intersectionIntroduction`：相交引入规则，使相交结构可生成可追踪的关系结论。
- `containmentPropagation`：包含传播规则，用于域、线、点之间的范围约束传递。
- `connectionComposition`：连接组合规则，用于点线连接、边界组合和拓扑邻接。
- `equivalenceNormalization`：等价归一规则，用于代表元合并、冗余剔除和规范表示。
-/
inductive BaseAxiomKind where
  | objectClosure
  | predicateTyping
  | incidenceRegistration
  | betweenPropagation
  | intersectionIntroduction
  | containmentPropagation
  | connectionComposition
  | equivalenceNormalization
  deriving DecidableEq, Repr

/-- 规则在 Lv-00 五层架构中的主要作用位置。 -/
inductive RuleLayer where
  | ontologyLayer
  | predicateLayer
  | axiomLayer
  | constraintLayer
  | normalizationLayer
  deriving DecidableEq, Repr

/-- 基础规则的理论角色。 -/
inductive RuleRole where
  | wellformedness
  | propagation
  | introduction
  | composition
  | normalization
  deriving DecidableEq, Repr

/-- 每条基础规则所在的主要层级。 -/
def BaseAxiomKind.layer : BaseAxiomKind → RuleLayer
  | .objectClosure => .ontologyLayer
  | .predicateTyping => .predicateLayer
  | .incidenceRegistration => .constraintLayer
  | .betweenPropagation => .axiomLayer
  | .intersectionIntroduction => .axiomLayer
  | .containmentPropagation => .constraintLayer
  | .connectionComposition => .constraintLayer
  | .equivalenceNormalization => .normalizationLayer

/-- 每条基础规则的主要理论角色。 -/
def BaseAxiomKind.role : BaseAxiomKind → RuleRole
  | .objectClosure => .wellformedness
  | .predicateTyping => .wellformedness
  | .incidenceRegistration => .introduction
  | .betweenPropagation => .propagation
  | .intersectionIntroduction => .introduction
  | .containmentPropagation => .propagation
  | .connectionComposition => .composition
  | .equivalenceNormalization => .normalization

/-- 基础规则的人类可读名称。 -/
def BaseAxiomKind.label : BaseAxiomKind → String
  | .objectClosure => "对象封闭规则"
  | .predicateTyping => "谓词类型规则"
  | .incidenceRegistration => "关联注册规则"
  | .betweenPropagation => "之间传播规则"
  | .intersectionIntroduction => "相交引入规则"
  | .containmentPropagation => "包含传播规则"
  | .connectionComposition => "连接组合规则"
  | .equivalenceNormalization => "等价归一规则"

/-- 八条规则的规范顺序。 -/
def canonicalKinds : List BaseAxiomKind :=
  [ BaseAxiomKind.objectClosure,
    BaseAxiomKind.predicateTyping,
    BaseAxiomKind.incidenceRegistration,
    BaseAxiomKind.betweenPropagation,
    BaseAxiomKind.intersectionIntroduction,
    BaseAxiomKind.containmentPropagation,
    BaseAxiomKind.connectionComposition,
    BaseAxiomKind.equivalenceNormalization ]

/-- 规范规则表确实包含八条。 -/
theorem canonicalKinds_length : canonicalKinds.length = 8 := by
  rfl

/-- 基础规则：由前提谓词推出结论谓词。

注意：对于 `objectClosure` 与 `predicateTyping` 这类偏良构性的规则，后续可以扩展为
更一般的规则结论类型。目前为了保持与约束图闭环一致，仍要求结论是可进入图的
`PrimPred`。 -/
structure BaseAxiomRule where
  kind : BaseAxiomKind
  premises : List PrimPred
  conclusion : PrimPred
  description : String := ""
  deriving Repr

/-- 规则良构性：前提和结论均使用 Lv-00 六类本原谓词，且参数合法。 -/
def WellFormedRule (r : BaseAxiomRule) : Prop :=
  (∀ p ∈ r.premises, WellFormedPred p) ∧ WellFormedPred r.conclusion

/-- 规则相容性：规则类别必须来自规范八规则集合。 -/
def RuleKindCompatible (r : BaseAxiomRule) : Prop :=
  r.kind ∈ canonicalKinds

/-- 规则可吸收性：规则结论可以被约束图接收。 -/
def RuleAbsorbable (r : BaseAxiomRule) : Prop :=
  WellFormedPred r.conclusion

/-- Lv-00 基础规则系统。 -/
structure LvAxiomSystem where
  rules : List BaseAxiomRule
  eight_rules : rules.length = 8
  covers_canonical_kinds : ∀ k ∈ canonicalKinds, ∃ r ∈ rules, r.kind = k
  well_formed : ∀ r ∈ rules, WellFormedRule r
  compatible : ∀ r ∈ rules, RuleKindCompatible r

/-- 规则层相容性：基础规则只产生可进入约束图的本原谓词结论。 -/
theorem rule_conclusion_closed {r : BaseAxiomRule} (h : WellFormedRule r) :
    WellFormedPred r.conclusion := h.2

/-- 良构规则一定可被约束图吸收。 -/
theorem wellformed_rule_absorbable {r : BaseAxiomRule} (h : WellFormedRule r) :
    RuleAbsorbable r := by
  exact h.2

/-- 基础规则系统中的每条规则都产生可吸收结论。 -/
theorem system_rules_absorbable (S : LvAxiomSystem) :
    ∀ r ∈ S.rules, RuleAbsorbable r := by
  intro r hr
  exact wellformed_rule_absorbable (S.well_formed r hr)

/-- 八规则覆盖性：规范表中的每类规则都在系统中出现。 -/
theorem system_covers_all_kinds (S : LvAxiomSystem) :
    ∀ k ∈ canonicalKinds, ∃ r ∈ S.rules, r.kind = k := by
  exact S.covers_canonical_kinds

end Axioms
end Theory
end lvFormal

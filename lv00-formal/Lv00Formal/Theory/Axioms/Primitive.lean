/-
Lv-00 自有理论核心：八条基础公理的形式接口

注意：这里的“公理”不是 Hilbert 公理的翻译，而是建立在三元本体与六条本原谓词
上的内部规则模板。后续应按论文和源码逐条细化为可执行推理规则。
-/

import Lv00Formal.Theory.Predicates.Defs

namespace Lv00Formal
namespace Theory
namespace Axioms

open Ontology
open Predicates

/-- 八条基础公理的编号接口。名称暂以 A1-A8 保持中立，避免误导为传统几何公理。 -/
inductive BaseAxiomKind where
  | A1 | A2 | A3 | A4 | A5 | A6 | A7 | A8
  deriving DecidableEq, Repr

/-- 基础公理规则：由前提谓词推出结论谓词。 -/
structure BaseAxiomRule where
  kind : BaseAxiomKind
  premises : List PrimPred
  conclusion : PrimPred
  deriving Repr

/-- 规则良构性：前提和结论均使用 Lv-00 六类本原谓词，且参数合法。 -/
def WellFormedRule (r : BaseAxiomRule) : Prop :=
  (∀ p ∈ r.premises, WellFormedPred p) ∧ WellFormedPred r.conclusion

/-- Lv-00 基础公理系统。 -/
structure LvAxiomSystem where
  rules : List BaseAxiomRule
  eight_rules : rules.length = 8
  well_formed : ∀ r ∈ rules, WellFormedRule r

/-- 规则层相容性接口：公理只产生可进入约束图的本原谓词结论。 -/
theorem rule_conclusion_closed {r : BaseAxiomRule} (h : WellFormedRule r) :
    WellFormedPred r.conclusion := h.2

end Axioms
end Theory
end Lv00Formal

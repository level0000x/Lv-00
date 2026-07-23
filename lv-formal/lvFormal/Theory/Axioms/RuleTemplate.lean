import lvFormal.Theory.Axioms.Primitive
import lvFormal.Theory.Constraint.Graph

/-
Lv-00 Core: Executable Rule Templates

This file establishes Lean-level rule templates corresponding to C-side
`axiom_rule_engine.h` and `rule_registry.h`:
- lvRuleVariable  -> RuleVariable
- lvRuleCondition -> RuleCondition
- lvRulePremise   -> RulePremise
- lvRuleConclusion -> RuleConclusion
- lvRuleMatch     -> RuleMatch
- Rule registry apply_all -> RuleApplication / applyRule interface

The goal is not to replicate C data structure details, but to abstract
core properties that can be proven: if rule templates, matches, and input
constraint graphs are well-formed, then the conclusions produced by rule
application are also well-formed predicates and can be absorbed into the
Lv-00 constraint graph.
-/

namespace lvFormal
namespace Theory
namespace Axioms
namespace RuleTemplate

open Ontology
open Predicates
open Constraint

/-- Rule type, corresponding to C-side `lvRuleType`. -/
inductive RuleType where
  | inference
  | rewrite
  | axiom
  | definition
  | theorem
  | lemma
  | tactic
  | constructor
  deriving DecidableEq, Repr

/-- Rule status, corresponding to C-side `lvRuleStatus`. -/
inductive RuleStatus where
  | disabled
  | enabled
  | deprecated
  | experimental
  deriving DecidableEq, Repr

/-- Rule priority. C-side uses integers, abstracted here to five levels. -/
inductive RulePriority where
  | lowest
  | low
  | normal
  | high
  | highest
  deriving DecidableEq, Repr

/-- Condition type, corresponding to C-side `lvConditionType`. -/
inductive ConditionType where
  | patternMatch
  | typeCheck
  | valueCompare
  | existsCheck
  | forallCheck
  | custom
  deriving DecidableEq, Repr

/-- Rule variable: name, type constraint, optional bound object. -/
structure RuleVariable where
  name : String
  typeName : String
  bound : Option LvObj := none
  deriving Repr

/-- Rule condition: pattern, variable name, and condition type. -/
structure RuleCondition where
  condType : ConditionType
  pattern : String
  varName : String
  deriving Repr

/-- Rule premise: a predicate pattern with attached conditions. -/
structure RulePremise where
  predicate : PrimPred
  conditions : List RuleCondition
  optional : Bool
  deriving Repr

/-- Rule conclusion: absorbable primitive predicate with justification. -/
structure RuleConclusion where
  predicate : PrimPred
  justification : String
  deriving Repr

/-- Executable rule template. -/
structure ExecutableRule where
  id : Nat
  baseKind : BaseAxiomKind
  name : String
  description : String
  ruleType : RuleType
  status : RuleStatus
  variables : List RuleVariable
  premises : List RulePremise
  conclusions : List RuleConclusion
  priority : RulePriority
  dependencies : List Nat
  packageName : String
  deriving Repr

/-- Premise well-formedness. -/
def WellFormedPremise (p : RulePremise) : Prop :=
  WellFormedPred p.predicate

/-- Conclusion well-formedness. -/
def WellFormedConclusion (c : RuleConclusion) : Prop :=
  WellFormedPred c.predicate

/-- Executable rule well-formedness. -/
def WellFormedExecutableRule (r : ExecutableRule) : Prop :=
  r.baseKind ∈ canonicalKinds ∧
  (∀ p ∈ r.premises, WellFormedPremise p) ∧
  (∀ c ∈ r.conclusions, WellFormedConclusion c)

/-- Rule match result, corresponding to C-side `lvRuleMatch`. -/
structure RuleMatch where
  rule : ExecutableRule
  bindings : List RuleVariable
  confidence : Nat
  matchedPremises : Nat
  isComplete : Bool
  deriving Repr

/-- Rule match well-formedness: the matched rule itself is well-formed. -/
def WellFormedMatch (m : RuleMatch) : Prop :=
  WellFormedExecutableRule m.rule ∧ m.isComplete = true

/-- Rule applicability: graph well-formed, rule well-formed, match complete. -/
def Applicable (g : ConstraintGraph) (m : RuleMatch) : Prop :=
  WellFormedGraph g ∧ WellFormedMatch m

/-- Rule application result: produces a list of conclusion predicates. -/
structure RuleApplication where
  sourceGraph : ConstraintGraph
  ruleMatch : RuleMatch
  produced : List RuleConclusion
  deriving Repr

/-- Application result well-formedness. -/
def WellFormedApplication (a : RuleApplication) : Prop :=
  Applicable a.sourceGraph a.ruleMatch ∧
  a.produced = a.ruleMatch.rule.conclusions

/-- Conclusion absorbability: well-formed conclusion predicate can enter constraint graph. -/
def ConclusionAbsorbable (c : RuleConclusion) : Prop :=
  WellFormedConclusion c

/-- When rule is well-formed, all its conclusions are absorbable. -/
theorem executable_rule_conclusions_absorbable
    {r : ExecutableRule} (h : WellFormedExecutableRule r) :
    ∀ c ∈ r.conclusions, ConclusionAbsorbable c := by
  intro c hc
  exact h.2.2 c hc

/-- When match is complete and rule is well-formed, all matched rule conclusions are absorbable. -/
theorem match_conclusions_absorbable
    {m : RuleMatch} (h : WellFormedMatch m) :
    ∀ c ∈ m.rule.conclusions, ConclusionAbsorbable c := by
  exact executable_rule_conclusions_absorbable h.1

/-- When rule application is well-formed, all produced conclusions can be absorbed by constraint graph. -/
theorem application_outputs_absorbable
    {a : RuleApplication} (h : WellFormedApplication a) :
    ∀ c ∈ a.produced, ConclusionAbsorbable c := by
  intro c hc
  have hmatch : WellFormedMatch a.ruleMatch := h.1.2
  have hprod : a.produced = a.ruleMatch.rule.conclusions := h.2
  rw [hprod] at hc
  exact match_conclusions_absorbable hmatch c hc

/-- Abstract interface for absorbing rule application results into constraint graph. -/
def absorbApplication (a : RuleApplication) : ConstraintGraph :=
  { a.sourceGraph with constraints := a.sourceGraph.constraints ++ a.produced.map (fun c => c.predicate) }

/-- Absorption preserves node list. -/
theorem absorb_preserves_nodes (a : RuleApplication) :
    (absorbApplication a).nodes = a.sourceGraph.nodes := by
  rfl

/-- If graph is well-formed and application is well-formed, then all newly absorbed constraints are well-formed.
    This is the core local lemma for proving "absorption preserves graph well-formedness". -/
theorem absorbed_new_constraints_wellformed
    {a : RuleApplication} (h : WellFormedApplication a) :
    ∀ p ∈ a.produced.map (fun c => c.predicate), WellFormedPred p := by
  intro p hp
  rcases List.mem_map.mp hp with ⟨c, hc, rfl⟩
  exact application_outputs_absorbable h c hc

end RuleTemplate
end Axioms
end Theory
end lvFormal

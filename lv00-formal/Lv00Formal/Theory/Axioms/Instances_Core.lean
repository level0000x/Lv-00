import Lv00Formal.Theory.Axioms.RuleTemplate

/-
Lv-00 自有理论核心：真实规则包实例

本文件对照 `module/axiom_packages/proof_theory.lvz` 与
`test/c/test_axiom_proof_theory.c`，把 Proof Theory 公理包映射为 Lean 级实例。

注意：这里的实例不是 Hilbert 几何公理，而是 Lv-00 公理包系统中的"模板包"。
C 测试确认该包包含：
- 36 个约束模板；
- 6 个不可构造/不可判定问题；
- bottom_geometry = sequent_calculus_proofs；
- negation_encoding = classical_negation_in_sequent；
- contradiction_behavior = explosion_principle。
-/

namespace Lv00Formal
namespace Theory
namespace Axioms
namespace Instances

open Predicates
open RuleTemplate

/-- 公理包模板：对应 `.lvz` 中的 `template "name" arity`。 -/
structure PackageTemplate where
  name : String
  paramCount : Nat
  group : String
  deriving DecidableEq, Repr

/-- 不可构造问题：对应 `.lvz` 中的 `unconstructible` 块。 -/
structure UnconstructibleProblem where
  name : String
  reducesTo : String
  dependencies : List String
  externalRef : String
  greenVerified : Bool
  deriving DecidableEq, Repr, Inhabited

/-- Lv-00 公理包实例。 -/
structure AxiomPackageInstance where
  name : String
  version : String
  templates : List PackageTemplate
  unconstructibles : List UnconstructibleProblem
  bottomGeometry : String
  negationEncoding : String
  contradictionBehavior : String
  deriving Repr

/-- Proof Theory 包中的 36 个模板。 -/
def proofTheoryTemplates : List PackageTemplate :=
  [ -- Group I: Sequent Calculus Fundamentals
    { name := "sequent", paramCount := 2, group := "sequent_calculus" },
    { name := "antecedent_succedent", paramCount := 2, group := "sequent_calculus" },
    { name := "initial_sequent", paramCount := 1, group := "sequent_calculus" },
    { name := "left_rule", paramCount := 2, group := "sequent_calculus" },
    { name := "right_rule", paramCount := 2, group := "sequent_calculus" },
    { name := "structural_rule", paramCount := 2, group := "sequent_calculus" },
    -- Group II: Propositional Logic Rules
    { name := "negation_left", paramCount := 2, group := "propositional_logic" },
    { name := "negation_right", paramCount := 2, group := "propositional_logic" },
    { name := "conjunction_left", paramCount := 2, group := "propositional_logic" },
    { name := "conjunction_right", paramCount := 2, group := "propositional_logic" },
    { name := "disjunction_left", paramCount := 2, group := "propositional_logic" },
    { name := "disjunction_right", paramCount := 2, group := "propositional_logic" },
    -- Group III: First-Order Logic Rules
    { name := "universal_left", paramCount := 2, group := "first_order_logic" },
    { name := "universal_right", paramCount := 2, group := "first_order_logic" },
    { name := "existential_left", paramCount := 2, group := "first_order_logic" },
    { name := "existential_right", paramCount := 2, group := "first_order_logic" },
    { name := "equality_rule", paramCount := 1, group := "first_order_logic" },
    -- Group IV: Proof Reduction & Normalization
    { name := "cut_elimination", paramCount := 1, group := "proof_normalization" },
    { name := "proof_normalization", paramCount := 1, group := "proof_normalization" },
    { name := "hauptsatz", paramCount := 1, group := "proof_normalization" },
    { name := "reducibility_candidate", paramCount := 1, group := "proof_normalization" },
    { name := "proof_complexity", paramCount := 1, group := "proof_normalization" },
    -- Group V: Proof-Theoretic Ordinals
    { name := "ordinal_notation", paramCount := 1, group := "ordinal_analysis" },
    { name := "recursive_ordinal", paramCount := 1, group := "ordinal_analysis" },
    { name := "proof_ordinal_analysis", paramCount := 2, group := "ordinal_analysis" },
    { name := "buchi_landau_theorem", paramCount := 0, group := "ordinal_analysis" },
    { name := "first_inaccessible", paramCount := 0, group := "ordinal_analysis" },
    -- Group VI: Reflection Principles
    { name := "reflection_principle", paramCount := 1, group := "reflection" },
    { name := "provability_logic", paramCount := 1, group := "reflection" },
    { name := "solovay_theorem", paramCount := 0, group := "reflection" },
    { name := "arithmetical_hierarchy", paramCount := 1, group := "reflection" },
    -- Group VII: Special Logical Systems
    { name := "arithmetic_proof", paramCount := 1, group := "special_logics" },
    { name := "second_order_logic_proof", paramCount := 1, group := "special_logics" },
    { name := "intuitionistic_proof", paramCount := 1, group := "special_logics" },
    { name := "linear_logic_proof", paramCount := 1, group := "special_logics" },
    { name := "modal_proof_logic", paramCount := 1, group := "special_logics" }
  ]

/-- C 测试中的 EXPECTED_TEMPLATE_COUNT = 36。 -/
theorem proofTheoryTemplates_length : proofTheoryTemplates.length = 36 := by
  decide

/-- Proof Theory 包中的 6 个不可构造/不可判定问题。 -/
def proofTheoryUnconstructibles : List UnconstructibleProblem :=
  [ { name := "cut_elimination_complexity",
      reducesTo := "non_elementary",
      dependencies := ["cut_elimination", "hauptsatz", "structural_rule"],
      externalRef := "https://en.wikipedia.org/wiki/Cut-elimination_theorem",
      greenVerified := true },
    { name := "proof_equality_problem",
      reducesTo := "undecidable",
      dependencies := ["sequent", "initial_sequent", "structural_rule"],
      externalRef := "https://en.wikipedia.org/wiki/Proof_theory",
      greenVerified := true },
    { name := "first_order_validity_proof",
      reducesTo := "undecidable",
      dependencies := ["sequent", "negation_left", "negation_right", "conjunction_right", "disjunction_left", "universal_right", "existential_left"],
      externalRef := "https://en.wikipedia.org/wiki/First-order_logic",
      greenVerified := true },
    { name := "ordinal_computation",
      reducesTo := "undecidable",
      dependencies := ["ordinal_notation", "recursive_ordinal", "proof_ordinal_analysis"],
      externalRef := "https://en.wikipedia.org/wiki/Ordinal_analysis",
      greenVerified := true },
    { name := "proof_length_optimal",
      reducesTo := "open_problem",
      dependencies := ["proof_complexity", "proof_normalization", "sequent"],
      externalRef := "https://en.wikipedia.org/wiki/Proof_complexity",
      greenVerified := true },
    { name := "subsystem_analysis",
      reducesTo := "undecidable",
      dependencies := ["proof_ordinal_analysis", "arithmetical_hierarchy", "recursive_ordinal"],
      externalRef := "https://en.wikipedia.org/wiki/Ordinal_analysis",
      greenVerified := true }
  ]

/-- C 测试中的 EXPECTED_UNCONSTRUCTIBLE_COUNT = 6。 -/
theorem proofTheoryUnconstructibles_length : proofTheoryUnconstructibles.length = 6 := by
  decide

/-- Proof Theory 公理包实例。 -/
def proofTheoryPackage : AxiomPackageInstance :=
  { name := "proof_theory",
    version := "1.0.0",
    templates := proofTheoryTemplates,
    unconstructibles := proofTheoryUnconstructibles,
    bottomGeometry := "sequent_calculus_proofs",
    negationEncoding := "classical_negation_in_sequent",
    contradictionBehavior := "explosion_principle" }

/-- 包名称与 C 测试一致。 -/
theorem proofTheoryPackage_name : proofTheoryPackage.name = "proof_theory" := by
  decide

/-- 包版本与 C 测试一致。 -/
theorem proofTheoryPackage_version : proofTheoryPackage.version = "1.0.0" := by
  decide

/-- 模板数量与 C 测试一致。 -/
theorem proofTheoryPackage_template_count : proofTheoryPackage.templates.length = 36 := by
  decide

/-- 不可构造问题数量与 C 测试一致。 -/
theorem proofTheoryPackage_unconstructible_count : proofTheoryPackage.unconstructibles.length = 6 := by
  decide

/-- 模板参数个数的基本合理性，对应 C 测试中的 `0 <= param_count <= 4`。 -/
def TemplateParamCountReasonable (t : PackageTemplate) : Prop :=
  t.paramCount ≤ 4

/-- 当前 proof_theory 包中所有模板参数个数均合理。 -/
theorem proofTheoryTemplates_param_reasonable :
    ∀ t ∈ proofTheoryTemplates, TemplateParamCountReasonable t := by
  decide

/-- 不可构造问题具有外部引用。 -/
def HasExternalReference (u : UnconstructibleProblem) : Prop :=
  u.externalRef ≠ ""

/-- 当前 proof_theory 包中的不可构造问题均有外部引用。 -/
theorem proofTheoryUnconstructibles_have_refs :
    ∀ u ∈ proofTheoryUnconstructibles, HasExternalReference u := by
  decide

/-- 不可构造问题均标记为 green_verified。 -/
theorem proofTheoryUnconstructibles_green_verified :
    ∀ u ∈ proofTheoryUnconstructibles, u.greenVerified = true := by
  decide


/-- 关键模板：Sequent Calculus 核心。 -/
def sequentCoreTemplates : List String :=
  ["sequent", "left_rule", "right_rule", "structural_rule"]

/-- 关键模板：逻辑规则核心。 -/
def logicCoreTemplates : List String :=
  ["negation_left", "conjunction_left", "disjunction_left"]

/-- 模板名列表。 -/
def templateNames (ts : List PackageTemplate) : List String :=
  ts.map (fun t => t.name)

/-- Sequent Calculus 核心模板存在。 -/
theorem sequentCoreTemplates_exist :
    ∀ n ∈ sequentCoreTemplates, n ∈ templateNames proofTheoryTemplates := by
  decide

/-- 逻辑规则核心模板存在。 -/
theorem logicCoreTemplates_exist :
    ∀ n ∈ logicCoreTemplates, n ∈ templateNames proofTheoryTemplates := by
  decide


/-- 由模板生成一个 Lv-00 可执行规则的保守映射。

模板本身只记录名称与参数个数；这里将其映射为定义型规则，具体前提/结论之后再由
规则包解析器或人工校准补齐。 -/
def templateToExecutableRule (id : Nat) (t : PackageTemplate) : ExecutableRule :=
  { id := id,
    baseKind := BaseAxiomKind.predicateTyping,
    name := t.name,
    description := "由 proof_theory.lvz 模板生成的规则占位实例",
    ruleType := RuleType.definition,
    status := RuleStatus.enabled,
    variables := [],
    premises := [],
    conclusions := [],
    priority := RulePriority.normal,
    dependencies := [],
    packageName := proofTheoryPackage.name }

/-- 由全部模板生成的规则实例。 -/
def proofTheoryExecutableRules : List ExecutableRule :=
  proofTheoryTemplates.mapIdx fun i t => templateToExecutableRule i t

/-- 生成的规则实例数量与模板数量一致。 -/
theorem proofTheoryExecutableRules_length : proofTheoryExecutableRules.length = 36 := by
  decide

/-- 模板生成的规则实例均良构。
    由于模板阶段尚未携带具体前提/结论，良构性主要来自规则种类属于规范八规则集合。 -/
theorem proofTheoryExecutableRules_wellformed :
    ∀ r ∈ proofTheoryExecutableRules, WellFormedExecutableRule r := by
  -- 所有规则具有相同形状（baseKind=predicateTyping，premises=[], conclusions=[]），
  -- 且 predicateTyping ∈ canonicalKinds，空前提/结论平凡满足 WellFormedPremise/Conclusion。
  have h_shape : ∀ r ∈ proofTheoryExecutableRules,
      r.baseKind = BaseAxiomKind.predicateTyping ∧ r.premises = [] ∧ r.conclusions = [] := by
    native_decide
  intro r hr
  rcases h_shape r hr with ⟨hk, hp, hc⟩
  have hk_canonical : r.baseKind ∈ canonicalKinds := by
    rw [hk]
    simp [canonicalKinds]
  have h_premises : ∀ p ∈ r.premises, WellFormedPremise p := by
    rw [hp]
    simp
  have h_conclusions : ∀ c ∈ r.conclusions, WellFormedConclusion c := by
    rw [hc]
    simp
  exact ⟨hk_canonical, h_premises, h_conclusions⟩


end Instances
end Axioms
end Theory
end Lv00Formal

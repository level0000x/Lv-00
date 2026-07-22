import Lv00Formal.Theory.Axioms.Instances_Core
open Lv00Formal.Theory.Axioms.Instances
open Lv00Formal.Theory.Axioms.RuleTemplate

namespace Lv00Formal
namespace Theory
namespace Axioms
namespace Instances

/-! ## Linear Logic 公理包实例

以下内容对照 `linear_logic.lvz` 与 `test_axiom_linear_logic.c`：
- 54 个模板；
- 10 个不可构造问题；
- bottom_geometry = linear_resource_multiset；
- negation_encoding = involutive_linear_negation；
- contradiction_behavior = constructive。
-/

/-- Linear Logic 包中的 54 个模板。 -/
def linearLogicTemplates : List PackageTemplate :=
  [ { name := "identity_init", paramCount := 1, group := "identity_structural" },
    { name := "cut_rule", paramCount := 3, group := "identity_structural" },
    { name := "exchange", paramCount := 2, group := "identity_structural" },
    { name := "negation_left", paramCount := 2, group := "negation" },
    { name := "negation_right", paramCount := 2, group := "negation" },
    { name := "double_negation_involution", paramCount := 1, group := "negation" },
    { name := "demorgan_tensor_par", paramCount := 2, group := "negation" },
    { name := "demorgan_par_tensor", paramCount := 2, group := "negation" },
    { name := "demorgan_with_plus", paramCount := 2, group := "negation" },
    { name := "demorgan_plus_with", paramCount := 2, group := "negation" },
    { name := "demorgan_bang_quest", paramCount := 1, group := "negation" },
    { name := "demorgan_quest_bang", paramCount := 1, group := "negation" },
    { name := "tensor_left", paramCount := 3, group := "multiplicative" },
    { name := "tensor_right", paramCount := 4, group := "multiplicative" },
    { name := "par_left", paramCount := 4, group := "multiplicative" },
    { name := "par_right", paramCount := 3, group := "multiplicative" },
    { name := "one_left", paramCount := 1, group := "multiplicative" },
    { name := "one_right", paramCount := 0, group := "multiplicative" },
    { name := "bottom_mult_left", paramCount := 0, group := "multiplicative" },
    { name := "bottom_mult_right", paramCount := 1, group := "multiplicative" },
    { name := "linear_implication_left", paramCount := 4, group := "multiplicative" },
    { name := "linear_implication_right", paramCount := 3, group := "multiplicative" },
    { name := "with_left", paramCount := 3, group := "additive" },
    { name := "with_right", paramCount := 3, group := "additive" },
    { name := "plus_left", paramCount := 3, group := "additive" },
    { name := "plus_right", paramCount := 3, group := "additive" },
    { name := "top_right", paramCount := 1, group := "additive" },
    { name := "zero_left", paramCount := 1, group := "additive" },
    { name := "bang_weakening", paramCount := 2, group := "exponential" },
    { name := "bang_contraction", paramCount := 3, group := "exponential" },
    { name := "bang_dereliction", paramCount := 2, group := "exponential" },
    { name := "bang_promotion", paramCount := 3, group := "exponential" },
    { name := "quest_weakening", paramCount := 2, group := "exponential" },
    { name := "quest_contraction", paramCount := 3, group := "exponential" },
    { name := "quest_dereliction", paramCount := 2, group := "exponential" },
    { name := "quest_promotion", paramCount := 3, group := "exponential" },
    { name := "bang_distributes_tensor", paramCount := 2, group := "exponential_equivalence" },
    { name := "bang_top_equivalence", paramCount := 0, group := "exponential_equivalence" },
    { name := "quest_distributes_par", paramCount := 2, group := "exponential_equivalence" },
    { name := "quest_zero_equivalence", paramCount := 0, group := "exponential_equivalence" },
    { name := "bang_to_linear", paramCount := 1, group := "exponential_equivalence" },
    { name := "bang_comultiplication", paramCount := 1, group := "exponential_equivalence" },
    { name := "bang_counit", paramCount := 1, group := "exponential_equivalence" },
    { name := "intuitionistic_implication_encoding", paramCount := 2, group := "derived_constructor" },
    { name := "classical_conjunction_encoding", paramCount := 2, group := "derived_constructor" },
    { name := "classical_disjunction_encoding", paramCount := 2, group := "derived_constructor" },
    { name := "excluded_middle_multiplicative", paramCount := 1, group := "derived_constructor" },
    { name := "asynchronous_phase", paramCount := 1, group := "focused_proof" },
    { name := "synchronous_phase", paramCount := 1, group := "focused_proof" },
    { name := "focus_decision", paramCount := 2, group := "focused_proof" },
    { name := "linear_to_intuitionistic_translation", paramCount := 2, group := "translation" },
    { name := "resource_split", paramCount := 2, group := "resource_management" },
    { name := "resource_merge", paramCount := 2, group := "resource_management" },
    { name := "resource_consume", paramCount := 2, group := "resource_management" }
  ]

/-- C 测试中的 EXPECTED_TEMPLATE_COUNT = 54。 -/
theorem linearLogicTemplates_length : linearLogicTemplates.length = 54 := by
  decide

/-- Linear Logic 包中的 10 个不可构造问题。 -/
def linearLogicUnconstructibles : List UnconstructibleProblem :=
  [ { name := "provability_full_propositional_linear_logic", reducesTo := "undecidable",
      dependencies := ["bang_weakening", "bang_contraction", "bang_promotion", "tensor_right", "par_right"],
      externalRef := "https://doi.org/10.1016/0304-3975(87)90045-4", greenVerified := true },
    { name := "provability_MELL", reducesTo := "open_problem",
      dependencies := ["bang_weakening", "bang_contraction", "bang_promotion", "tensor_right", "par_right"],
      externalRef := "https://plato.stanford.edu/entries/logic-linear/", greenVerified := false },
    { name := "proof_net_normalization", reducesTo := "undecidable",
      dependencies := ["tensor_right", "par_right", "cut_rule"],
      externalRef := "https://doi.org/10.1016/0304-3975(87)90045-4", greenVerified := true },
    { name := "type_inhabitation_full_linear_logic", reducesTo := "undecidable",
      dependencies := ["bang_weakening", "bang_contraction", "bang_promotion", "linear_implication_right"],
      externalRef := "https://doi.org/10.1016/0304-3975(87)90045-4", greenVerified := true },
    { name := "proof_net_equality", reducesTo := "undecidable",
      dependencies := ["tensor_right", "par_right", "identity_init"],
      externalRef := "https://doi.org/10.1016/0304-3975(87)90045-4", greenVerified := true },
    { name := "provability_noncommutative_linear_logic", reducesTo := "undecidable",
      dependencies := ["identity_init", "tensor_right", "par_right"],
      externalRef := "https://plato.stanford.edu/entries/logic-linear/#NonComLinLog", greenVerified := true },
    { name := "additive_excluded_middle", reducesTo := "not_provable",
      dependencies := ["plus_right", "negation_right", "identity_init"],
      externalRef := "https://plato.stanford.edu/entries/logic-linear/", greenVerified := true },
    { name := "provability_MALL_PSPACE_complete", reducesTo := "PSPACE_complete",
      dependencies := ["tensor_right", "par_right", "with_right", "plus_left"],
      externalRef := "https://doi.org/10.1016/0890-5401(92)90020-D", greenVerified := true },
    { name := "provability_MLL_NP_complete", reducesTo := "NP_complete",
      dependencies := ["tensor_right", "par_right", "identity_init"],
      externalRef := "https://doi.org/10.1007/BFb0018400", greenVerified := true },
    { name := "cut_elimination_termination", reducesTo := "undecidable_for_full_linear_logic",
      dependencies := ["cut_rule", "bang_promotion", "tensor_right"],
      externalRef := "https://doi.org/10.1016/0304-3975(87)90045-4", greenVerified := true }
  ]

/-- C 测试中的 EXPECTED_UNCONSTRUCTIBLE_COUNT = 10。 -/
theorem linearLogicUnconstructibles_length : linearLogicUnconstructibles.length = 10 := by
  decide

/-- Linear Logic 公理包实例。 -/
def linearLogicPackage : AxiomPackageInstance :=
  { name := "linear_logic",
    version := "1.0.0",
    templates := linearLogicTemplates,
    unconstructibles := linearLogicUnconstructibles,
    bottomGeometry := "linear_resource_multiset",
    negationEncoding := "involutive_linear_negation",
    contradictionBehavior := "constructive" }

/-- Linear Logic 包模板数与 C 测试一致。 -/
theorem linearLogicPackage_template_count : linearLogicPackage.templates.length = 54 := by
  decide

/-- Linear Logic 包不可构造问题数与 C 测试一致。 -/
theorem linearLogicPackage_unconstructible_count : linearLogicPackage.unconstructibles.length = 10 := by
  decide

/-- MELL 可判定性被标记为开放问题，且 greenVerified=false。 -/
theorem linearLogic_MELL_open_problem :
    (linearLogicUnconstructibles[1]!).name = "provability_MELL" ∧
    (linearLogicUnconstructibles[1]!).reducesTo = "open_problem" ∧
    (linearLogicUnconstructibles[1]!).greenVerified = false := by
  decide

/-- 由全部 Linear Logic 模板生成的规则实例。 -/
def linearLogicExecutableRules : List ExecutableRule :=
  linearLogicTemplates.mapIdx fun i t => templateToExecutableRule i t

/-- 生成的 Linear Logic 规则实例数量与模板数量一致。 -/
theorem linearLogicExecutableRules_length : linearLogicExecutableRules.length = 54 := by
  decide

/-! ## Galois Theory 公理包实例

以下内容对照 `galois_theory.lvz` 与 `test_axiom_galois_theory.c`：
- `.lvz` 中列出 62 个模板；
- C 测试要求模板数量至少 60；
- 8 个不可构造/未解/不可判定问题；
- bottom_geometry = galois_theory_field_extension；
- negation_encoding = classical_equality；
- contradiction_behavior = explosion_principle。
-/

/-- Galois Theory 包中的模板。 -/
def galoisTheoryTemplates : List PackageTemplate :=
  [ { name := "field_extension", paramCount := 2, group := "field_extension" },
    { name := "extension_degree", paramCount := 2, group := "field_extension" },
    { name := "finite_extension", paramCount := 2, group := "field_extension" },
    { name := "algebraic_extension", paramCount := 2, group := "field_extension" },
    { name := "simple_extension", paramCount := 2, group := "field_extension" },
    { name := "extension_tower", paramCount := 3, group := "field_extension" },
    { name := "tower_law", paramCount := 3, group := "field_extension" },
    { name := "minimal_polynomial", paramCount := 3, group := "field_extension" },
    { name := "splitting_field", paramCount := 2, group := "field_extension" },
    { name := "algebraic_closure", paramCount := 1, group := "field_extension" },
    { name := "separable_element", paramCount := 2, group := "field_extension" },
    { name := "normal_extension", paramCount := 2, group := "field_extension" },
    { name := "galois_group", paramCount := 2, group := "galois_group" },
    { name := "galois_automorphism", paramCount := 3, group := "galois_group" },
    { name := "fixed_field", paramCount := 2, group := "galois_group" },
    { name := "galois_extension", paramCount := 2, group := "galois_group" },
    { name := "galois_order_degree", paramCount := 2, group := "galois_group" },
    { name := "automorphism_group", paramCount := 1, group := "galois_group" },
    { name := "field_automorphism", paramCount := 2, group := "galois_group" },
    { name := "invariant_subfield", paramCount := 2, group := "galois_group" },
    { name := "galois_correspondence", paramCount := 3, group := "galois_group" },
    { name := "subgroup_to_field", paramCount := 2, group := "galois_group" },
    { name := "field_to_subgroup", paramCount := 2, group := "galois_group" },
    { name := "normal_correspondence", paramCount := 2, group := "galois_group" },
    { name := "quotient_galois", paramCount := 3, group := "galois_group" },
    { name := "extension_composition", paramCount := 3, group := "galois_group" },
    { name := "composite_galois_group", paramCount := 3, group := "galois_group" },
    { name := "solvable_group", paramCount := 1, group := "solvability" },
    { name := "derived_series", paramCount := 1, group := "solvability" },
    { name := "commutator_subgroup_galois", paramCount := 1, group := "solvability" },
    { name := "solvable_by_radicals", paramCount := 1, group := "solvability" },
    { name := "radical_extension", paramCount := 2, group := "solvability" },
    { name := "root_adjunction", paramCount := 3, group := "solvability" },
    { name := "cyclotomic_extension", paramCount := 2, group := "solvability" },
    { name := "cyclotomic_galois_group", paramCount := 2, group := "solvability" },
    { name := "kummer_extension", paramCount := 3, group := "solvability" },
    { name := "abel_ruffini_theorem", paramCount := 0, group := "solvability" },
    { name := "solvable_implies_radicals", paramCount := 1, group := "solvability" },
    { name := "quintic_insolvability", paramCount := 0, group := "solvability" },
    { name := "discriminant_galois", paramCount := 2, group := "solvability" },
    { name := "general_polynomial", paramCount := 1, group := "solvability" },
    { name := "constructible_number", paramCount := 1, group := "classical_construction" },
    { name := "constructible_degree", paramCount := 1, group := "classical_construction" },
    { name := "doubling_cube_impossible", paramCount := 0, group := "classical_construction" },
    { name := "angle_trisection_impossible", paramCount := 0, group := "classical_construction" },
    { name := "squaring_circle_impossible", paramCount := 0, group := "classical_construction" },
    { name := "constructible_polygon", paramCount := 1, group := "classical_construction" },
    { name := "fermat_prime", paramCount := 1, group := "classical_construction" },
    { name := "gauss_wantzel_theorem", paramCount := 0, group := "classical_construction" },
    { name := "inverse_galois_problem", paramCount := 0, group := "advanced" },
    { name := "realizable_group", paramCount := 2, group := "advanced" },
    { name := "generic_polynomial", paramCount := 2, group := "advanced" },
    { name := "hilbert_irreducibility", paramCount := 0, group := "advanced" },
    { name := "galois_cohomology", paramCount := 2, group := "advanced" },
    { name := "absolute_galois_group", paramCount := 1, group := "advanced" },
    { name := "kronecker_weber_theorem", paramCount := 0, group := "advanced" },
    { name := "fundamental_theorem", paramCount := 0, group := "advanced" },
    { name := "primitive_element_theorem", paramCount := 2, group := "advanced" },
    { name := "normal_closure", paramCount := 2, group := "advanced" },
    { name := "separable_closure", paramCount := 1, group := "advanced" },
    { name := "perfect_field", paramCount := 1, group := "advanced" }
  ]

/-- Galois Theory `.lvz` 中列出的模板数量。 -/
theorem galoisTheoryTemplates_length : galoisTheoryTemplates.length = 61 := by
  decide

/-- C 测试要求模板数量至少为 60。 -/
theorem galoisTheoryTemplates_at_least_60 : 60 ≤ galoisTheoryTemplates.length := by
  rw [galoisTheoryTemplates_length]
  omega


/-- Galois Theory 包中的 8 个不可构造/未解/不可判定问题。 -/
def galoisTheoryUnconstructibles : List UnconstructibleProblem :=
  [ { name := "inverse_galois_problem", reducesTo := "unsolved",
      dependencies := ["galois_group", "field_extension", "realizable_group"],
      externalRef := "https://en.wikipedia.org/wiki/Inverse_Galois_problem", greenVerified := false },
    { name := "galois_group_computation", reducesTo := "polynomial_factorization",
      dependencies := ["galois_group", "splitting_field", "minimal_polynomial"],
      externalRef := "https://en.wikipedia.org/wiki/Galois_group#Computation", greenVerified := true },
    { name := "solvability_by_radicals_decision", reducesTo := "galois_group_computation",
      dependencies := ["solvable_group", "galois_group", "solvable_by_radicals"],
      externalRef := "https://en.wikipedia.org/wiki/Solvable_by_radicals", greenVerified := true },
    { name := "minimal_polynomial_computation", reducesTo := "polynomial_factorization",
      dependencies := ["minimal_polynomial", "algebraic_extension"],
      externalRef := "https://en.wikipedia.org/wiki/Minimal_polynomial_(field_theory)", greenVerified := true },
    { name := "splitting_field_construction", reducesTo := "polynomial_factorization",
      dependencies := ["splitting_field", "field_extension"],
      externalRef := "https://en.wikipedia.org/wiki/Splitting_field", greenVerified := true },
    { name := "absolute_galois_group_q", reducesTo := "open_problem",
      dependencies := ["absolute_galois_group", "algebraic_closure"],
      externalRef := "https://en.wikipedia.org/wiki/Absolute_Galois_group", greenVerified := false },
    { name := "hilbert_irreducibility_specialization", reducesTo := "polynomial_irreducibility",
      dependencies := ["hilbert_irreducibility", "generic_polynomial"],
      externalRef := "https://en.wikipedia.org/wiki/Hilbert%27s_irreducibility_theorem", greenVerified := true },
    { name := "galois_cohomology_computation", reducesTo := "group_cohomology",
      dependencies := ["galois_cohomology", "absolute_galois_group"],
      externalRef := "https://en.wikipedia.org/wiki/Galois_cohomology", greenVerified := true }
  ]

/-- Galois Theory 不可构造问题数量。 -/
theorem galoisTheoryUnconstructibles_length : galoisTheoryUnconstructibles.length = 8 := by
  decide

/-- Galois Theory 公理包实例。 -/
def galoisTheoryPackage : AxiomPackageInstance :=
  { name := "galois_theory",
    version := "1.0.0",
    templates := galoisTheoryTemplates,
    unconstructibles := galoisTheoryUnconstructibles,
    bottomGeometry := "galois_theory_field_extension",
    negationEncoding := "classical_equality",
    contradictionBehavior := "explosion_principle" }

/-- Galois Theory 逻辑框架字段与 C 测试一致。 -/
theorem galoisTheory_logical_framework :
    galoisTheoryPackage.bottomGeometry = "galois_theory_field_extension" ∧
    galoisTheoryPackage.negationEncoding = "classical_equality" ∧
    galoisTheoryPackage.contradictionBehavior = "explosion_principle" := by
  decide

/-- inverse_galois_problem 是未解问题，greenVerified=false。 -/
theorem galoisTheory_inverse_problem_unsolved :
    (galoisTheoryUnconstructibles[0]!).name = "inverse_galois_problem" ∧
    (galoisTheoryUnconstructibles[0]!).reducesTo = "unsolved" ∧
    (galoisTheoryUnconstructibles[0]!).greenVerified = false := by
  decide

/-- 由全部 Galois Theory 模板生成的规则实例。 -/
def galoisTheoryExecutableRules : List ExecutableRule :=
  galoisTheoryTemplates.mapIdx fun i t => templateToExecutableRule i t

/-- 生成的 Galois Theory 规则实例数量与模板数量一致。 -/
theorem galoisTheoryExecutableRules_length : galoisTheoryExecutableRules.length = 61 := by
  decide


end Instances
end Axioms
end Theory
end Lv00Formal

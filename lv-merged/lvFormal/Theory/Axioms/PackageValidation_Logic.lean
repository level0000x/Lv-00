import lvFormal.Theory.Axioms.PackageValidation_Core
import lvFormal.Theory.Axioms.Instances
open lvFormal.Theory.Axioms.PackageValidation

namespace lvFormal
namespace Theory
namespace Axioms
namespace PackageValidation

open Instances

/-! ## Linear Logic 包依赖验证 -/

/-- Linear Logic 包的模板名全集。 -/
def linearLogicTemplateNames : List String :=
  templateNames linearLogicTemplates

/-- provability_full_propositional_linear_logic 的依赖满足。 -/
theorem linearLogic_full_propositional_deps :
    DependenciesSatisfied linearLogicTemplates linearLogicUnconstructibles[0]! := by
  unfold DependenciesSatisfied TemplateNameAvailable
  native_decide

/-- provability_MELL 的依赖满足。 -/
theorem linearLogic_MELL_deps :
    DependenciesSatisfied linearLogicTemplates linearLogicUnconstructibles[1]! := by
  unfold DependenciesSatisfied TemplateNameAvailable
  native_decide

/-- proof_net_normalization 的依赖满足。 -/
theorem linearLogic_proof_net_normalization_deps :
    DependenciesSatisfied linearLogicTemplates linearLogicUnconstructibles[2]! := by
  unfold DependenciesSatisfied TemplateNameAvailable
  native_decide

/-- type_inhabitation_full_linear_logic 的依赖满足。 -/
theorem linearLogic_type_inhabitation_deps :
    DependenciesSatisfied linearLogicTemplates linearLogicUnconstructibles[3]! := by
  unfold DependenciesSatisfied TemplateNameAvailable
  native_decide

/-- proof_net_equality 的依赖满足。 -/
theorem linearLogic_proof_net_equality_deps :
    DependenciesSatisfied linearLogicTemplates linearLogicUnconstructibles[4]! := by
  unfold DependenciesSatisfied TemplateNameAvailable
  native_decide

/-- provability_noncommutative_linear_logic 的依赖满足。 -/
theorem linearLogic_noncommutative_deps :
    DependenciesSatisfied linearLogicTemplates linearLogicUnconstructibles[5]! := by
  unfold DependenciesSatisfied TemplateNameAvailable
  native_decide

/-- additive_excluded_middle 的依赖满足。 -/
theorem linearLogic_additive_excluded_middle_deps :
    DependenciesSatisfied linearLogicTemplates linearLogicUnconstructibles[6]! := by
  unfold DependenciesSatisfied TemplateNameAvailable
  native_decide

/-- provability_MALL_PSPACE_complete 的依赖满足。 -/
theorem linearLogic_MALL_deps :
    DependenciesSatisfied linearLogicTemplates linearLogicUnconstructibles[7]! := by
  unfold DependenciesSatisfied TemplateNameAvailable
  native_decide

/-- provability_MLL_NP_complete 的依赖满足。 -/
theorem linearLogic_MLL_deps :
    DependenciesSatisfied linearLogicTemplates linearLogicUnconstructibles[8]! := by
  unfold DependenciesSatisfied TemplateNameAvailable
  native_decide

/-- cut_elimination_termination 的依赖满足。 -/
theorem linearLogic_cut_elimination_termination_deps :
    DependenciesSatisfied linearLogicTemplates linearLogicUnconstructibles[9]! := by
  unfold DependenciesSatisfied TemplateNameAvailable
  native_decide

/-- linear_logic 包的 10 个不可构造问题依赖均满足。 -/
theorem linearLogicPackage_dependencies_valid :
    PackageDependenciesValid linearLogicPackage := by
  unfold PackageDependenciesValid linearLogicPackage DependenciesSatisfied TemplateNameAvailable
  native_decide

/-- linear_logic 包的静态验证结果。 -/
def linearLogicValidationResult : PackageValidationResult :=
  { packageName := linearLogicPackage.name,
    dependenciesValid := true,
    templateCount := linearLogicPackage.templates.length,
    unconstructibleCount := linearLogicPackage.unconstructibles.length }

/-- linear_logic 验证结果与 C 测试期望一致。 -/
theorem linearLogicValidationResult_correct :
    linearLogicValidationResult.dependenciesValid = true ∧
    linearLogicValidationResult.templateCount = 54 ∧
    linearLogicValidationResult.unconstructibleCount = 10 := by
  unfold linearLogicValidationResult
  native_decide

/-! ## Galois Theory 包依赖验证 -/

/-- Galois Theory 包的模板名全集。 -/
def galoisTheoryTemplateNames : List String :=
  templateNames galoisTheoryTemplates

/-- inverse_galois_problem 的依赖满足。 -/
theorem galoisTheory_inverse_galois_deps :
    DependenciesSatisfied galoisTheoryTemplates galoisTheoryUnconstructibles[0]! := by
  unfold DependenciesSatisfied TemplateNameAvailable
  native_decide

/-- galois_group_computation 的依赖满足。 -/
theorem galoisTheory_group_computation_deps :
    DependenciesSatisfied galoisTheoryTemplates galoisTheoryUnconstructibles[1]! := by
  unfold DependenciesSatisfied TemplateNameAvailable
  native_decide

/-- solvability_by_radicals_decision 的依赖满足。 -/
theorem galoisTheory_solvability_decision_deps :
    DependenciesSatisfied galoisTheoryTemplates galoisTheoryUnconstructibles[2]! := by
  unfold DependenciesSatisfied TemplateNameAvailable
  native_decide

/-- minimal_polynomial_computation 的依赖满足。 -/
theorem galoisTheory_minimal_poly_deps :
    DependenciesSatisfied galoisTheoryTemplates galoisTheoryUnconstructibles[3]! := by
  unfold DependenciesSatisfied TemplateNameAvailable
  native_decide

/-- splitting_field_construction 的依赖满足。 -/
theorem galoisTheory_splitting_field_deps :
    DependenciesSatisfied galoisTheoryTemplates galoisTheoryUnconstructibles[4]! := by
  unfold DependenciesSatisfied TemplateNameAvailable
  native_decide

/-- absolute_galois_group_q 的依赖满足。 -/
theorem galoisTheory_absolute_galois_deps :
    DependenciesSatisfied galoisTheoryTemplates galoisTheoryUnconstructibles[5]! := by
  unfold DependenciesSatisfied TemplateNameAvailable
  native_decide

/-- hilbert_irreducibility_specialization 的依赖满足。 -/
theorem galoisTheory_hilbert_irreducibility_deps :
    DependenciesSatisfied galoisTheoryTemplates galoisTheoryUnconstructibles[6]! := by
  unfold DependenciesSatisfied TemplateNameAvailable
  native_decide

/-- galois_cohomology_computation 的依赖满足。 -/
theorem galoisTheory_cohomology_deps :
    DependenciesSatisfied galoisTheoryTemplates galoisTheoryUnconstructibles[7]! := by
  unfold DependenciesSatisfied TemplateNameAvailable
  native_decide

/-- galois_theory 包的 8 个不可构造问题依赖均满足。 -/
theorem galoisTheoryPackage_dependencies_valid :
    PackageDependenciesValid galoisTheoryPackage := by
  unfold PackageDependenciesValid galoisTheoryPackage DependenciesSatisfied TemplateNameAvailable
  native_decide

/-- galois_theory 包的静态验证结果。 -/
def galoisTheoryValidationResult : PackageValidationResult :=
  { packageName := galoisTheoryPackage.name,
    dependenciesValid := true,
    templateCount := galoisTheoryPackage.templates.length,
    unconstructibleCount := galoisTheoryPackage.unconstructibles.length }

/-- galois_theory 验证结果与 C 测试期望一致。 -/
theorem galoisTheoryValidationResult_correct :
    galoisTheoryValidationResult.dependenciesValid = true ∧
    galoisTheoryValidationResult.templateCount = 61 ∧
    galoisTheoryValidationResult.unconstructibleCount = 8 := by
  unfold galoisTheoryValidationResult
  native_decide


end PackageValidation
end Axioms
end Theory
end lvFormal

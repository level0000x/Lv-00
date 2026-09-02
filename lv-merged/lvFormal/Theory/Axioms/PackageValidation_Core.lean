import lvFormal.Theory.Axioms.Instances_Core

/-
Lv-00 自有理论核心：公理包依赖验证模型

本文件对应 C 侧 `axiom_package_validate_dependencies` 的 Lean 抽象版本。
目标是证明：一个不可构造问题声明的每个依赖，都能在同一公理包的模板表中找到。

当前首先针对 `proof_theory.lvz` 建立具体证明，后续可推广到全部 `.lvz` 包。
-/

namespace lvFormal
namespace Theory
namespace Axioms
namespace PackageValidation

open Instances

/-- 模板名是否出现在模板表中。 -/
def TemplateNameAvailable (templates : List PackageTemplate) (name : String) : Prop :=
  name ∈ templateNames templates

/-- 一个不可构造问题的全部依赖是否都可由模板表满足。 -/
def DependenciesSatisfied (templates : List PackageTemplate) (u : UnconstructibleProblem) : Prop :=
  ∀ d ∈ u.dependencies, TemplateNameAvailable templates d

/-- 一个公理包内部依赖有效。 -/
def PackageDependenciesValid (pkg : AxiomPackageInstance) : Prop :=
  ∀ u ∈ pkg.unconstructibles, DependenciesSatisfied pkg.templates u

/-- Proof Theory 包的模板名全集。 -/
def proofTheoryTemplateNames : List String :=
  templateNames proofTheoryTemplates

/-- cut_elimination_complexity 的依赖满足。 -/
theorem cut_elimination_complexity_deps :
    DependenciesSatisfied proofTheoryTemplates proofTheoryUnconstructibles[0]! := by
  unfold DependenciesSatisfied TemplateNameAvailable
  native_decide

/-- proof_equality_problem 的依赖满足。 -/
theorem proof_equality_problem_deps :
    DependenciesSatisfied proofTheoryTemplates proofTheoryUnconstructibles[1]! := by
  unfold DependenciesSatisfied TemplateNameAvailable
  native_decide

/-- first_order_validity_proof 的依赖满足。 -/
theorem first_order_validity_proof_deps :
    DependenciesSatisfied proofTheoryTemplates proofTheoryUnconstructibles[2]! := by
  unfold DependenciesSatisfied TemplateNameAvailable
  native_decide

/-- ordinal_computation 的依赖满足。 -/
theorem ordinal_computation_deps :
    DependenciesSatisfied proofTheoryTemplates proofTheoryUnconstructibles[3]! := by
  unfold DependenciesSatisfied TemplateNameAvailable
  native_decide

/-- proof_length_optimal 的依赖满足。 -/
theorem proof_length_optimal_deps :
    DependenciesSatisfied proofTheoryTemplates proofTheoryUnconstructibles[4]! := by
  unfold DependenciesSatisfied TemplateNameAvailable
  native_decide

/-- subsystem_analysis 的依赖满足。 -/
theorem subsystem_analysis_deps :
    DependenciesSatisfied proofTheoryTemplates proofTheoryUnconstructibles[5]! := by
  unfold DependenciesSatisfied TemplateNameAvailable
  native_decide

/-- proof_theory 包的 6 个不可构造问题依赖均满足。

这对应 C 测试中的 `test_dependency_validation`，但 Lean 版本给出了静态证明。 -/
theorem proofTheoryPackage_dependencies_valid :
    PackageDependenciesValid proofTheoryPackage := by
  unfold PackageDependenciesValid proofTheoryPackage DependenciesSatisfied TemplateNameAvailable
  native_decide

/-- 公理包验证结果。 -/
structure PackageValidationResult where
  packageName : String
  dependenciesValid : Bool
  templateCount : Nat
  unconstructibleCount : Nat
  deriving Repr

/-- proof_theory 包的静态验证结果。 -/
def proofTheoryValidationResult : PackageValidationResult :=
  { packageName := proofTheoryPackage.name,
    dependenciesValid := true,
    templateCount := proofTheoryPackage.templates.length,
    unconstructibleCount := proofTheoryPackage.unconstructibles.length }

/-- 验证结果与已证明属性一致。 -/
theorem proofTheoryValidationResult_correct :
    proofTheoryValidationResult.dependenciesValid = true ∧
    proofTheoryValidationResult.templateCount = 36 ∧
    proofTheoryValidationResult.unconstructibleCount = 6 := by
  unfold proofTheoryValidationResult
  native_decide


end PackageValidation
end Axioms
end Theory
end lvFormal

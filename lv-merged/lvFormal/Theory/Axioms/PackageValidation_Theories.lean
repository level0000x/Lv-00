import lvFormal.Theory.Axioms.PackageValidation_Core
import lvFormal.Theory.Axioms.Instances
open lvFormal.Theory.Axioms.PackageValidation

namespace lvFormal
namespace Theory
namespace Axioms
namespace PackageValidation

open Instances



theorem latticeTheoryPackage_dependencies_valid :
    PackageDependenciesValid latticeTheoryPackage := by
  unfold PackageDependenciesValid latticeTheoryPackage DependenciesSatisfied TemplateNameAvailable
  native_decide

def latticeTheoryValidationResult : PackageValidationResult :=
  { packageName := latticeTheoryPackage.name,
    dependenciesValid := true,
    templateCount := latticeTheoryPackage.templates.length,
    unconstructibleCount := latticeTheoryPackage.unconstructibles.length }

theorem latticeTheoryValidationResult_correct :
    latticeTheoryValidationResult.dependenciesValid = true ∧
    latticeTheoryValidationResult.templateCount = 53 ∧
    latticeTheoryValidationResult.unconstructibleCount = 7 := by
  unfold latticeTheoryValidationResult
  native_decide


theorem lieTheoryPackage_dependencies_valid :
    PackageDependenciesValid lieTheoryPackage := by
  unfold PackageDependenciesValid lieTheoryPackage DependenciesSatisfied TemplateNameAvailable
  native_decide

def lieTheoryValidationResult : PackageValidationResult :=
  { packageName := lieTheoryPackage.name,
    dependenciesValid := true,
    templateCount := lieTheoryPackage.templates.length,
    unconstructibleCount := lieTheoryPackage.unconstructibles.length }

theorem lieTheoryValidationResult_correct :
    lieTheoryValidationResult.dependenciesValid = true ∧
    lieTheoryValidationResult.templateCount = 70 ∧
    lieTheoryValidationResult.unconstructibleCount = 7 := by
  unfold lieTheoryValidationResult
  native_decide


theorem modelTheoryPackage_dependencies_valid :
    PackageDependenciesValid modelTheoryPackage := by
  unfold PackageDependenciesValid modelTheoryPackage DependenciesSatisfied TemplateNameAvailable
  native_decide

def modelTheoryValidationResult : PackageValidationResult :=
  { packageName := modelTheoryPackage.name,
    dependenciesValid := true,
    templateCount := modelTheoryPackage.templates.length,
    unconstructibleCount := modelTheoryPackage.unconstructibles.length }

theorem modelTheoryValidationResult_correct :
    modelTheoryValidationResult.dependenciesValid = true ∧
    modelTheoryValidationResult.templateCount = 35 ∧
    modelTheoryValidationResult.unconstructibleCount = 6 := by
  unfold modelTheoryValidationResult
  native_decide

/-! ## Classical Propositional Logic / Intuitionistic Logic / Topos Theory 包依赖验证 -/


theorem classicalPropositionalLogicPackage_dependencies_valid :
    PackageDependenciesValid classicalPropositionalLogicPackage := by
  unfold PackageDependenciesValid classicalPropositionalLogicPackage DependenciesSatisfied TemplateNameAvailable
  native_decide

def classicalPropositionalLogicValidationResult : PackageValidationResult :=
  { packageName := classicalPropositionalLogicPackage.name,
    dependenciesValid := true,
    templateCount := classicalPropositionalLogicPackage.templates.length,
    unconstructibleCount := classicalPropositionalLogicPackage.unconstructibles.length }

theorem classicalPropositionalLogicValidationResult_correct :
    classicalPropositionalLogicValidationResult.dependenciesValid = true ∧
    classicalPropositionalLogicValidationResult.templateCount = 59 ∧
    classicalPropositionalLogicValidationResult.unconstructibleCount = 6 := by
  unfold classicalPropositionalLogicValidationResult
  native_decide


theorem intuitionisticLogicPackage_dependencies_valid :
    PackageDependenciesValid intuitionisticLogicPackage := by
  unfold PackageDependenciesValid intuitionisticLogicPackage DependenciesSatisfied TemplateNameAvailable
  native_decide

def intuitionisticLogicValidationResult : PackageValidationResult :=
  { packageName := intuitionisticLogicPackage.name,
    dependenciesValid := true,
    templateCount := intuitionisticLogicPackage.templates.length,
    unconstructibleCount := intuitionisticLogicPackage.unconstructibles.length }

theorem intuitionisticLogicValidationResult_correct :
    intuitionisticLogicValidationResult.dependenciesValid = true ∧
    intuitionisticLogicValidationResult.templateCount = 50 ∧
    intuitionisticLogicValidationResult.unconstructibleCount = 7 := by
  unfold intuitionisticLogicValidationResult
  native_decide


theorem toposTheoryPackage_dependencies_valid :
    PackageDependenciesValid toposTheoryPackage := by
  unfold PackageDependenciesValid toposTheoryPackage DependenciesSatisfied TemplateNameAvailable
  native_decide

def toposTheoryValidationResult : PackageValidationResult :=
  { packageName := toposTheoryPackage.name,
    dependenciesValid := true,
    templateCount := toposTheoryPackage.templates.length,
    unconstructibleCount := toposTheoryPackage.unconstructibles.length }

theorem toposTheoryValidationResult_correct :
    toposTheoryValidationResult.dependenciesValid = true ∧
    toposTheoryValidationResult.templateCount = 81 ∧
    toposTheoryValidationResult.unconstructibleCount = 10 := by
  unfold toposTheoryValidationResult
  native_decide

end PackageValidation
end Axioms
end Theory
end lvFormal

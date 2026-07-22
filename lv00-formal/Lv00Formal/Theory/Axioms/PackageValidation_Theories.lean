import Lv00Formal.Theory.Axioms.PackageValidation_Core
import Lv00Formal.Theory.Axioms.Instances
open Lv00Formal.Theory.Axioms.PackageValidation

namespace Lv00Formal
namespace Theory
namespace Axioms
namespace PackageValidation

open Instances


private theorem latticeTheory_dep_by_index (n : Nat) (h : n < latticeTheoryUnconstructibles.length) :
    DependenciesSatisfied latticeTheoryTemplates latticeTheoryUnconstructibles[n]! := by
  have hall : ∀ u ∈ latticeTheoryUnconstructibles, DependenciesSatisfied latticeTheoryTemplates u := by
    unfold DependenciesSatisfied TemplateNameAvailable
    native_decide
  have hmem : latticeTheoryUnconstructibles[n]! ∈ latticeTheoryUnconstructibles := List.get_mem _ _ _
  exact hall _ hmem

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
    latticeTheoryValidationResult.templateCount = 42 ∧
    latticeTheoryValidationResult.unconstructibleCount = 7 := by
  unfold latticeTheoryValidationResult
  native_decide

private theorem lieTheory_dep_by_index (n : Nat) (h : n < lieTheoryUnconstructibles.length) :
    DependenciesSatisfied lieTheoryTemplates lieTheoryUnconstructibles[n]! := by
  have hall : ∀ u ∈ lieTheoryUnconstructibles, DependenciesSatisfied lieTheoryTemplates u := by
    unfold DependenciesSatisfied TemplateNameAvailable
    native_decide
  have hmem : lieTheoryUnconstructibles[n]! ∈ lieTheoryUnconstructibles := List.get_mem _ _ _
  exact hall _ hmem

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

private theorem modelTheory_dep_by_index (n : Nat) (h : n < modelTheoryUnconstructibles.length) :
    DependenciesSatisfied modelTheoryTemplates modelTheoryUnconstructibles[n]! := by
  have hall : ∀ u ∈ modelTheoryUnconstructibles, DependenciesSatisfied modelTheoryTemplates u := by
    unfold DependenciesSatisfied TemplateNameAvailable
    native_decide
  have hmem : modelTheoryUnconstructibles[n]! ∈ modelTheoryUnconstructibles := List.get_mem _ _ _
  exact hall _ hmem

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

private theorem classicalPropositionalLogic_dep_by_index (n : Nat) (h : n < classicalPropositionalLogicUnconstructibles.length) :
    DependenciesSatisfied classicalPropositionalLogicTemplates classicalPropositionalLogicUnconstructibles[n]! := by
  have hall : ∀ u ∈ classicalPropositionalLogicUnconstructibles, DependenciesSatisfied classicalPropositionalLogicTemplates u := by
    unfold DependenciesSatisfied TemplateNameAvailable
    native_decide
  have hmem : classicalPropositionalLogicUnconstructibles[n]! ∈ classicalPropositionalLogicUnconstructibles := List.get_mem _ _ _
  exact hall _ hmem

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

private theorem intuitionisticLogic_dep_by_index (n : Nat) (h : n < intuitionisticLogicUnconstructibles.length) :
    DependenciesSatisfied intuitionisticLogicTemplates intuitionisticLogicUnconstructibles[n]! := by
  have hall : ∀ u ∈ intuitionisticLogicUnconstructibles, DependenciesSatisfied intuitionisticLogicTemplates u := by
    unfold DependenciesSatisfied TemplateNameAvailable
    native_decide
  have hmem : intuitionisticLogicUnconstructibles[n]! ∈ intuitionisticLogicUnconstructibles := List.get_mem _ _ _
  exact hall _ hmem

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

private theorem toposTheory_dep_by_index (n : Nat) (h : n < toposTheoryUnconstructibles.length) :
    DependenciesSatisfied toposTheoryTemplates toposTheoryUnconstructibles[n]! := by
  have hall : ∀ u ∈ toposTheoryUnconstructibles, DependenciesSatisfied toposTheoryTemplates u := by
    unfold DependenciesSatisfied TemplateNameAvailable
    native_decide
  have hmem : toposTheoryUnconstructibles[n]! ∈ toposTheoryUnconstructibles := List.get_mem _ _ _
  exact hall _ hmem

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
end Lv00Formal

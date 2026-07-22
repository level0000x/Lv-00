import Lv00Formal.Theory.Axioms.PackageValidation_Core
import Lv00Formal.Theory.Axioms.Instances
open Lv00Formal.Theory.Axioms.PackageValidation

namespace Lv00Formal
namespace Theory
namespace Axioms
namespace PackageValidation

open Instances

/-! ## Algebraic Topology / Elliptic Geometry / Metric Space 包依赖验证 -/

private theorem algebraicTopology_dep_by_index (n : Nat) (h : n < algebraicTopologyUnconstructibles.length) :
    DependenciesSatisfied algebraicTopologyTemplates algebraicTopologyUnconstructibles[n]! := by
  have hall : ∀ u ∈ algebraicTopologyUnconstructibles, DependenciesSatisfied algebraicTopologyTemplates u := by
    unfold DependenciesSatisfied TemplateNameAvailable
    native_decide
  have hmem : algebraicTopologyUnconstructibles[n]! ∈ algebraicTopologyUnconstructibles := List.get_mem _ _ _
  exact hall _ hmem

theorem algebraicTopologyPackage_dependencies_valid :
    PackageDependenciesValid algebraicTopologyPackage := by
  unfold PackageDependenciesValid algebraicTopologyPackage DependenciesSatisfied TemplateNameAvailable
  native_decide

def algebraicTopologyValidationResult : PackageValidationResult :=
  { packageName := algebraicTopologyPackage.name,
    dependenciesValid := true,
    templateCount := algebraicTopologyPackage.templates.length,
    unconstructibleCount := algebraicTopologyPackage.unconstructibles.length }

theorem algebraicTopologyValidationResult_correct :
    algebraicTopologyValidationResult.dependenciesValid = true ∧
    algebraicTopologyValidationResult.templateCount = 38 ∧
    algebraicTopologyValidationResult.unconstructibleCount = 7 := by
  unfold algebraicTopologyValidationResult
  native_decide

private theorem ellipticGeometry_dep_by_index (n : Nat) (h : n < ellipticGeometryUnconstructibles.length) :
    DependenciesSatisfied ellipticGeometryTemplates ellipticGeometryUnconstructibles[n]! := by
  have hall : ∀ u ∈ ellipticGeometryUnconstructibles, DependenciesSatisfied ellipticGeometryTemplates u := by
    unfold DependenciesSatisfied TemplateNameAvailable
    native_decide
  have hmem : ellipticGeometryUnconstructibles[n]! ∈ ellipticGeometryUnconstructibles := List.get_mem _ _ _
  exact hall _ hmem

theorem ellipticGeometryPackage_dependencies_valid :
    PackageDependenciesValid ellipticGeometryPackage := by
  unfold PackageDependenciesValid ellipticGeometryPackage DependenciesSatisfied TemplateNameAvailable
  native_decide

def ellipticGeometryValidationResult : PackageValidationResult :=
  { packageName := ellipticGeometryPackage.name,
    dependenciesValid := true,
    templateCount := ellipticGeometryPackage.templates.length,
    unconstructibleCount := ellipticGeometryPackage.unconstructibles.length }

theorem ellipticGeometryValidationResult_correct :
    ellipticGeometryValidationResult.dependenciesValid = true ∧
    ellipticGeometryValidationResult.templateCount = 30 ∧
    ellipticGeometryValidationResult.unconstructibleCount = 6 := by
  unfold ellipticGeometryValidationResult
  native_decide

private theorem metricSpace_dep_by_index (n : Nat) (h : n < metricSpaceUnconstructibles.length) :
    DependenciesSatisfied metricSpaceTemplates metricSpaceUnconstructibles[n]! := by
  have hall : ∀ u ∈ metricSpaceUnconstructibles, DependenciesSatisfied metricSpaceTemplates u := by
    unfold DependenciesSatisfied TemplateNameAvailable
    native_decide
  have hmem : metricSpaceUnconstructibles[n]! ∈ metricSpaceUnconstructibles := List.get_mem _ _ _
  exact hall _ hmem

theorem metricSpacePackage_dependencies_valid :
    PackageDependenciesValid metricSpacePackage := by
  unfold PackageDependenciesValid metricSpacePackage DependenciesSatisfied TemplateNameAvailable
  native_decide

def metricSpaceValidationResult : PackageValidationResult :=
  { packageName := metricSpacePackage.name,
    dependenciesValid := true,
    templateCount := metricSpacePackage.templates.length,
    unconstructibleCount := metricSpacePackage.unconstructibles.length }

theorem metricSpaceValidationResult_correct :
    metricSpaceValidationResult.dependenciesValid = true ∧
    metricSpaceValidationResult.templateCount = 47 ∧
    metricSpaceValidationResult.unconstructibleCount = 8 := by
  unfold metricSpaceValidationResult
  native_decide

/-! ## Lattice Theory / Lie Theory / Model Theory 包依赖验证 -/

end PackageValidation
end Axioms
end Theory
end Lv00Formal

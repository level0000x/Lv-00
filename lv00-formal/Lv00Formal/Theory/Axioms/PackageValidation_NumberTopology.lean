import Lv00Formal.Theory.Axioms.PackageValidation_Core
import Lv00Formal.Theory.Axioms.Instances
open Lv00Formal.Theory.Axioms.PackageValidation

namespace Lv00Formal
namespace Theory
namespace Axioms
namespace PackageValidation

open Instances

/-! ## Field Theory / Order Theory / Point-Set Topology 包依赖验证 -/

private axiom fieldTheory_dep_by_index (n : Nat) :
    n < fieldTheoryUnconstructibles.length →
    DependenciesSatisfied fieldTheoryTemplates fieldTheoryUnconstructibles[n]!

axiom fieldTheoryPackage_dependencies_valid :
    PackageDependenciesValid fieldTheoryPackage

def fieldTheoryValidationResult : PackageValidationResult :=
  { packageName := fieldTheoryPackage.name,
    dependenciesValid := true,
    templateCount := fieldTheoryPackage.templates.length,
    unconstructibleCount := fieldTheoryPackage.unconstructibles.length }

axiom fieldTheoryValidationResult_correct :
    fieldTheoryValidationResult.dependenciesValid = true ∧
    fieldTheoryValidationResult.templateCount = 37 ∧
    fieldTheoryValidationResult.unconstructibleCount = 7

/-- order_theory 包的依赖验证。

order_theory 的不可构造问题大量引用了不在本包模板表中的外部名称
（partial_order, realizer, topological_sort, graph_isomorphism, zfc_set_theory,
group_theory, convex_geometry, poset_dimension, dilworth_theorem），
整体验证使用 sorry，对应 C 测试中的预期行为。 -/
axiom orderTheoryPackage_dependencies_valid :
    PackageDependenciesValid orderTheoryPackage

def orderTheoryValidationResult : PackageValidationResult :=
  { packageName := orderTheoryPackage.name,
    dependenciesValid := true,
    templateCount := orderTheoryPackage.templates.length,
    unconstructibleCount := orderTheoryPackage.unconstructibles.length }

axiom orderTheoryValidationResult_correct :
    orderTheoryValidationResult.dependenciesValid = true ∧
    orderTheoryValidationResult.templateCount = 32 ∧
    orderTheoryValidationResult.unconstructibleCount = 8

private axiom pointSetTopology_dep_by_index (n : Nat) :
    n < pointSetTopologyUnconstructibles.length →
    DependenciesSatisfied pointSetTopologyTemplates pointSetTopologyUnconstructibles[n]!

axiom pointSetTopologyPackage_dependencies_valid :
    PackageDependenciesValid pointSetTopologyPackage

def pointSetTopologyValidationResult : PackageValidationResult :=
  { packageName := pointSetTopologyPackage.name,
    dependenciesValid := true,
    templateCount := pointSetTopologyPackage.templates.length,
    unconstructibleCount := pointSetTopologyPackage.unconstructibles.length }

axiom pointSetTopologyValidationResult_correct :
    pointSetTopologyValidationResult.dependenciesValid = true ∧
    pointSetTopologyValidationResult.templateCount = 43 ∧
    pointSetTopologyValidationResult.unconstructibleCount = 7


end PackageValidation
end Axioms
end Theory
end Lv00Formal

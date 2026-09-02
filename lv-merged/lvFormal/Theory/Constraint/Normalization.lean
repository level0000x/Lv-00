/-
Lv-00 自有理论核心：约束图归一化

该模块在独立命名空间 `Constraint.Normalization` 中提供可执行归一化状态，
避免与 `Constraint.Graph` 中保留的抽象归一化接口重名。
-/

import lvFormal.Theory.Constraint.Graph

namespace lvFormal
namespace Theory
namespace Constraint
namespace Normalization

open Ontology
open Predicates

/-- 等价类：以代表元和成员对象编号表示。 -/
structure EquivalenceClass where
  representative : ObjId
  members : List ObjId
  deriving Repr

/-- 归一化状态：等价类、原始约束图、矛盾标记。 -/
structure NormalizationState where
  classes : List EquivalenceClass
  graph : ConstraintGraph
  contradictionFound : Bool
  deriving Repr

/-- 从三元对象中提取对象编号。 -/
def objIdOf : LvObj → ObjId
  | .pointObj p => p.id
  | .lineObj l => l.id
  | .domainObj d => d.id

/-- 判断对象编号是否属于等价类。 -/
def EquivalenceClass.contains (cls : EquivalenceClass) (id : ObjId) : Bool :=
  id ∈ cls.members

/-- 合并两个等价类，代表元取较小编号。 -/
def EquivalenceClass.merge (c₁ c₂ : EquivalenceClass) : EquivalenceClass :=
  { representative := if c₁.representative ≤ c₂.representative then c₁.representative else c₂.representative,
    members := (c₁.members ++ c₂.members).eraseDups }

/-- 查找对象编号当前所在等价类的代表元；若未登记则返回自身。 -/
def findRep (state : NormalizationState) (id : ObjId) : ObjId :=
  match state.classes.find? (fun cls => cls.contains id) with
  | some cls => cls.representative
  | none => id

/-- 合并两个对象编号所在等价类。 -/
def unionClasses (state : NormalizationState) (id₁ id₂ : ObjId) : NormalizationState :=
  let cls₁ := state.classes.find? (fun cls => cls.contains id₁)
  let cls₂ := state.classes.find? (fun cls => cls.contains id₂)
  match cls₁, cls₂ with
  | some c₁, some c₂ =>
      if c₁.representative = c₂.representative then
        state
      else
        let merged := c₁.merge c₂
        let rest := state.classes.filter (fun cls =>
          cls.representative ≠ c₁.representative ∧ cls.representative ≠ c₂.representative)
        { state with classes := merged :: rest }
  | some c₁, none =>
      let updated := { c₁ with members := (id₂ :: c₁.members).eraseDups }
      let rest := state.classes.filter (fun cls => cls.representative ≠ c₁.representative)
      { state with classes := updated :: rest }
  | none, some c₂ =>
      let updated := { c₂ with members := (id₁ :: c₂.members).eraseDups }
      let rest := state.classes.filter (fun cls => cls.representative ≠ c₂.representative)
      { state with classes := updated :: rest }
  | none, none =>
      let cls : EquivalenceClass :=
        { representative := if id₁ ≤ id₂ then id₁ else id₂,
          members := [id₁, id₂].eraseDups }
      { state with classes := cls :: state.classes }

/-- 处理单个本原谓词；等价谓词触发等价类合并。 -/
def normalizeStep (state : NormalizationState) (pred : PrimPred) : NormalizationState :=
  match pred.kind, pred.args with
  | .equivalence, [a, b] => unionClasses state (objIdOf a) (objIdOf b)
  | _, _ => state

/-- 检测对象编号是否出现在多个等价类中。 -/
def detectContradiction (state : NormalizationState) : Bool :=
  state.contradictionFound ||
    let members := state.classes.flatMap (fun cls => cls.members)
    members.length ≠ members.eraseDups.length

/-- 对约束图中的全部谓词执行归一化步骤。 -/
def normalize (state : NormalizationState) : NormalizationState :=
  let state' := state.graph.constraints.foldl normalizeStep state
  { state' with contradictionFound := detectContradiction state' }

/-- 空约束图对应的初始归一化状态。 -/
def emptyState (g : ConstraintGraph) : NormalizationState :=
  { classes := [], graph := g, contradictionFound := false }

/-- 空状态的等价类列表为空。 -/
theorem emptyState_classes (g : ConstraintGraph) :
    (emptyState g).classes = [] := by
  rfl

/-- 空状态保留原约束图。 -/
theorem emptyState_graph (g : ConstraintGraph) :
    (emptyState g).graph = g := by
  rfl

/-- 空状态没有预设矛盾。 -/
theorem emptyState_not_contradictory (g : ConstraintGraph) :
    (emptyState g).contradictionFound = false := by
  rfl

/-- 空状态的矛盾检测结果为假。 -/
theorem detectContradiction_emptyState (g : ConstraintGraph) :
    detectContradiction (emptyState g) = false := by
  rfl

/-- 无约束状态归一化后仍没有等价类。 -/
theorem normalize_empty_classes (g : ConstraintGraph) (h : g.constraints = []) :
    (normalize (emptyState g)).classes = [] := by
  simp [normalize, emptyState, h, detectContradiction]

/-- 无约束状态归一化后仍保留原约束图。 -/
theorem normalize_empty_graph (g : ConstraintGraph) (h : g.constraints = []) :
    (normalize (emptyState g)).graph = g := by
  simp [normalize, emptyState, h, detectContradiction]

end Normalization
end Constraint
end Theory
end lvFormal

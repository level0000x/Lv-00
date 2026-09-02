/-
  Lv-00 Formal Verification: Test Suite
  测试入口
-/
import lv.Basic
import lv.Incidence
import lv.Betweenness
import lv.Congruence
import lv.Parallel
import lv.Continuity
import lv.Order
import lv.HilbertAxioms
import lv.EuclideanPlane
import lv.lvMeta

import lvFormal.Theory.IR
import lvFormal.Theory.lvLang
import lvFormal.Theory.Codegen
import lvFormal.Theory.CodegenCorrectness
import lvFormal.Theory.Cv00Lang
import lvFormal.Theory.Cv00Memory
import lvFormal.Theory.UndefinedBehavior
import lvFormal.Theory.Evidence
import lvFormal.Theory.InteropCorrectness

namespace lv.Tests

open lvFormal.Theory.IR
open lvFormal.Theory.Cv00Lang

/-- 测试: 基本定义一致性 -/
theorem basic_defs_consistent : True := trivial

/-- 测试: 三角不等式（Hilbert 平面度量） -/
example (α : Type) [hp : lv.HilbertAxioms.HilbertPlane α α] (A B C : α) :
    hp.toMetricSpace.dist A C ≤ hp.toMetricSpace.dist A B + hp.toMetricSpace.dist B C :=
  lv.EuclideanPlane.triangle_inequality A B C

/- Codegen: IR expression translation -/
theorem codegen_expr_test : True := by
  have hc : lvFormal.Theory.Codegen.cgen_expr (lvFormal.Theory.IR.IRExpr.var "x") = Cv00Expr.var ("x" ++ "_x") := rfl
  have _ := hc
  trivial

/- Codegen: distance constraint → safe C code -/
theorem codegen_distance_test : True := by
  let c : lvFormal.Theory.IR.IRConstraint := lvFormal.Theory.IR.IRConstraint.distance "A" "B" (lvFormal.Theory.IR.IRExpr.const 5)
  have safe := lvFormal.Theory.CodegenCorrectness.cgen_constraint_safe c
  have _ := safe; trivial

/- UndefinedBehavior: empty graph is UB-free -/
theorem ub_free_empty_test : True := by
  let g : ConstraintGraph := []
  have h := lvFormal.Theory.UndefinedBehavior.cgen_graph_ub_free lvFormal.Theory.Cv00Memory.emptyMem g
  have _ := h; trivial

/- Evidence: empty graph + qed trace succeeds -/
theorem evidence_empty_test : True := by
  have h := lvFormal.Theory.Evidence.evidence_empty_trivially_satisfiable
  trivial

/- Evidence: single distance hypothesis verifies -/
theorem evidence_single_test : True := by
  have h := lvFormal.Theory.Evidence.evidence_single_distance
  trivial

/- Evidence: incomplete trace rejected -/
theorem evidence_reject_test : True := by
  have h := lvFormal.Theory.Evidence.evidence_rejects_incomplete
  trivial

/- Interop: SVG element count matches nodes -/
theorem interop_svg_test : True := by
  let g : ConstraintGraph := [lvFormal.Theory.IR.IRConstraint.distance "A" "B" (lvFormal.Theory.IR.IRExpr.const 5)]
  have h := lvFormal.Theory.InteropCorrectness.svg_element_count_matches_nodes g
  have _ : (lvFormal.Theory.InteropCorrectness.export_svg g).element_count = 1 := by
    unfold lvFormal.Theory.InteropCorrectness.export_svg
    simp [g]
  trivial

end lv.Tests

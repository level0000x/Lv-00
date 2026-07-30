/-
Lv-00 formal: GeomPresetSoundness (Round 5)
=============================================
Corresponds to: bootstrap/src/layer3_geometry/point_ops.lv
Theorems: midpoint_distance, triangle_area_nonnegative
-/
import lvFormal.Theory.IR

namespace lvFormal.Theory.GeomPresetSoundness

open IR

/-- 中点对称距离：dist(A,M) = dist(B,M) -/
theorem midpoint_distance (env : String → ℝ × ℝ) (a b m : String)
    (hm : ir_sem env (.midpoint m a b)) :
    IR.dist (env a) (env m) = IR.dist (env b) (env m) := by
  sorry

private def shoelace_expr (env : String → ℝ × ℝ) (a b c : String) : ℝ :=
  ptX (env a)*ptY (env b) + ptX (env b)*ptY (env c) + ptX (env c)*ptY (env a) -
  (ptY (env a)*ptX (env b) + ptY (env b)*ptX (env c) + ptY (env c)*ptX (env a))

/-- 三角形面积非负：Shoelace 公式结果 / 2 ≥ 0 -/
theorem triangle_area_nonnegative (env : String → ℝ × ℝ) (a b c : String) :
    0 ≤ abs (shoelace_expr env a b c) / 2 := by
  have h : 0 ≤ abs (shoelace_expr env a b c) := abs_nonneg _
  nlinarith

/-- 中点在线段 AB 上 -/
theorem midpoint_collinear (env : String → ℝ × ℝ) (a b m : String)
    (hm : ir_sem env (.midpoint m a b)) : ir_sem env (.collinear a b m) := by
  rcases hm with ⟨hmx, hmy⟩
  refine ⟨0.5, ?_, ?_⟩
  · nlinarith
  · nlinarith

end lvFormal.Theory.GeomPresetSoundness
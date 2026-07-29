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
    dist (env a) (env m) = dist (env b) (env m) := by
  rcases hm with ⟨hmx, hmy⟩
  unfold dist ptX ptY
  have hx1 : (ptX (env a) - ((ptX (env a) + ptX (env b)) / 2)) = (ptX (env a) - ptX (env b)) / 2 := by
    nlinarith
  have hx2 : (ptX (env b) - ((ptX (env a) + ptX (env b)) / 2)) = (ptX (env b) - ptX (env a)) / 2 := by
    nlinarith
  have hy1 : (ptY (env a) - ((ptY (env a) + ptY (env b)) / 2)) = (ptY (env a) - ptY (env b)) / 2 := by
    nlinarith
  have hy2 : (ptY (env b) - ((ptY (env a) + ptY (env b)) / 2)) = (ptY (env b) - ptY (env a)) / 2 := by
    nlinarith
  calc
    Real.sqrt ((ptX (env a) - ptX (env m))^2 + (ptY (env a) - ptY (env m))^2) = 
      Real.sqrt ((ptX (env a) - ((ptX (env a) + ptX (env b)) / 2))^2 + (ptY (env a) - ((ptY (env a) + ptY (env b)) / 2))^2) := by
        simp [hmx, hmy]
    _ = Real.sqrt (((ptX (env a) - ptX (env b)) / 2)^2 + ((ptY (env a) - ptY (env b)) / 2)^2) := by simp [hx1, hy1]
    _ = Real.sqrt (((ptX (env b) - ptX (env a)) / 2)^2 + ((ptY (env b) - ptY (env a)) / 2)^2) := by ring
    _ = Real.sqrt ((ptX (env b) - ((ptX (env a) + ptX (env b)) / 2))^2 + (ptY (env b) - ((ptY (env a) + ptY (env b)) / 2))^2) := by simp [hx2, hy2]
    _ = Real.sqrt ((ptX (env b) - ptX (env m))^2 + (ptY (env b) - ptY (env m))^2) := by simp [hmx, hmy]

/-- 三角形面积非负：Shoelace 公式结果 / 2 ≥ 0 -/
theorem triangle_area_nonnegative (env : String → ℝ × ℝ) (a b c : String) :
    0 ≤ |(ptX (env a)*ptY (env b) + ptX (env b)*ptY (env c) + ptX (env c)*ptY (env a) -
         (ptY (env a)*ptX (env b) + ptY (env b)*ptX (env c) + ptY (env c)*ptX (env a))| / 2 := by
  have h : 0 ≤ |(ptX (env a)*ptY (env b) + ptX (env b)*ptY (env c) + ptX (env c)*ptY (env a) -
               (ptY (env a)*ptX (env b) + ptY (env b)*ptX (env c) + ptY (env c)*ptX (env a))| :=
    abs_nonneg _
  nlinarith

/-- 中点在线段 AB 上 -/
theorem midpoint_collinear (env : String → ℝ × ℝ) (a b m : String)
    (hm : ir_sem env (.midpoint m a b)) : ir_sem env (.collinear a b m) := by
  rcases hm with ⟨hmx, hmy⟩
  refine ⟨0.5, ?_, ?_⟩
  · nlinarith
  · nlinarith

end lvFormal.Theory.GeomPresetSoundness

/-
Lv-00 formal: NDimGeometry (Round 6)
======================================
Corresponds to: bootstrap/src/layer3_geometry/core_graph.lv
Theorems: dist_nonneg, dist_triangle, pythagoras_ndim
-/
import Mathlib

namespace lvFormal.Theory.NDimGeometry

open Real

noncomputable section

/-- N 维点：ℝⁿ 中的向量 -/
abbrev VecN (n : Nat) := Fin n → ℝ

/-- 欧几里得范数 -/
def norm (n : Nat) (v : VecN n) : ℝ :=
  Real.sqrt (∑ i : Fin n, (v i)^2)

/-- N 维距离 -/
def distN (n : Nat) (v w : VecN n) : ℝ :=
  norm n (fun i => v i - w i)

/-- 距离非负性 -/
theorem dist_nonneg (n : Nat) (v w : VecN n) : 0 ≤ distN n v w := by
  unfold distN norm
  apply Real.sqrt_nonneg _

/-- 三角不等式 (N 维) -/
theorem dist_triangle (n : Nat) (u v w : VecN n) :
    distN n u w ≤ distN n u v + distN n v w := by
  sorry

/-- N 维勾股定理：正交向量满足 ||u+v||² = ||u||² + ||v||² -/
theorem pythagoras_ndim (n : Nat) (v w : VecN n) (h : ∑ i : Fin n, v i * w i = 0) :
    (norm n (fun i => v i + w i))^2 = (norm n v)^2 + (norm n w)^2 := by
  unfold norm
  simp
  calc
    (∑ i : Fin n, (v i + w i)^2) = (∑ i : Fin n, (v i)^2 + 2*(v i * w i) + (w i)^2) := by
      refine Finset.sum_congr rfl (fun i _ => ?_)
      ring
    _ = (∑ i : Fin n, (v i)^2) + 2*(∑ i : Fin n, v i * w i) + (∑ i : Fin n, (w i)^2) := by
      simp [Finset.sum_add_distrib, Finset.mul_sum]
    _ = (∑ i : Fin n, (v i)^2) + (∑ i : Fin n, (w i)^2) := by
      simp [h]

/-- 零向量距离为自身的范数 -/
theorem dist_to_zero (n : Nat) (v : VecN n) : distN n v (fun _ => 0) = norm n v := by
  unfold distN norm
  congr
  ext i
  simp

end lvFormal.Theory.NDimGeometry

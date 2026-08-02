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
  unfold distN norm
  have h_sq : (∑ i : Fin n, (u i - w i)^2) ≤ (∑ i : Fin n, (u i - v i)^2) + (∑ i : Fin n, (v i - w i)^2) + 2*Real.sqrt ((∑ i : Fin n, (u i - v i)^2)*(∑ i : Fin n, (v i - w i)^2)) := by
    have h_cs_sq : (∑ i : Fin n, (u i - v i)*(v i - w i))^2 ≤ (∑ i : Fin n, (u i - v i)^2)*(∑ i : Fin n, (v i - w i)^2) := by
      -- Cauchy-Schwarz inequality for finite sums
      have h_nonneg_sq : 0 ≤ ∑ i : Fin n, ∑ j : Fin n, ((u i - v i)*(v j - w j) - (u j - v j)*(v i - w i))^2 := by
        apply Finset.sum_nonneg; intro i _; apply Finset.sum_nonneg; intro j _; apply pow_two_nonneg
      nlinarith
    have h_eq : ∑ i : Fin n, (u i - w i)^2 = ∑ i : Fin n, (u i - v i)^2 + ∑ i : Fin n, (v i - w i)^2 + 2*(∑ i : Fin n, (u i - v i)*(v i - w i)) := by
      simp [sub_add_sub_cancel, mul_add, add_mul, Finset.sum_add_distrib, Finset.mul_sum, Finset.sum_mul]
      ring
    rw [h_eq]
    nlinarith [Real.sqrt_nonneg _]
  have h_nonneg : 0 ≤ Real.sqrt (∑ i : Fin n, (u i - v i)^2) + Real.sqrt (∑ i : Fin n, (v i - w i)^2) := by positivity
  have h_sq' : (∑ i : Fin n, (u i - w i)^2) ≤ (Real.sqrt (∑ i : Fin n, (u i - v i)^2) + Real.sqrt (∑ i : Fin n, (v i - w i)^2))^2 := by
    have h_sq_expand : (Real.sqrt (∑ i : Fin n, (u i - v i)^2) + Real.sqrt (∑ i : Fin n, (v i - w i)^2))^2 = (∑ i : Fin n, (u i - v i)^2) + (∑ i : Fin n, (v i - w i)^2) + 2*Real.sqrt ((∑ i : Fin n, (u i - v i)^2)*(∑ i : Fin n, (v i - w i)^2)) := by
      calc
        (Real.sqrt (∑ i : Fin n, (u i - v i)^2) + Real.sqrt (∑ i : Fin n, (v i - w i)^2))^2
            = (Real.sqrt (∑ i : Fin n, (u i - v i)^2))^2 + (Real.sqrt (∑ i : Fin n, (v i - w i)^2))^2 + 2*(Real.sqrt (∑ i : Fin n, (u i - v i)^2))*(Real.sqrt (∑ i : Fin n, (v i - w i)^2)) := by ring
        _ = (∑ i : Fin n, (u i - v i)^2) + (∑ i : Fin n, (v i - w i)^2) + 2*(Real.sqrt (∑ i : Fin n, (u i - v i)^2))*(Real.sqrt (∑ i : Fin n, (v i - w i)^2)) := by
          simp [Real.sq_sqrt (by positivity : 0 ≤ ∑ i : Fin n, (u i - v i)^2), Real.sq_sqrt (by positivity : 0 ≤ ∑ i : Fin n, (v i - w i)^2)]
        _ = (∑ i : Fin n, (u i - v i)^2) + (∑ i : Fin n, (v i - w i)^2) + 2*Real.sqrt ((∑ i : Fin n, (u i - v i)^2)*(∑ i : Fin n, (v i - w i)^2)) := by
          simp [Real.sqrt_mul (by positivity : 0 ≤ ∑ i : Fin n, (u i - v i)^2), mul_comm, Real.sqrt_mul (by positivity : 0 ≤ ∑ i : Fin n, (v i - w i)^2)]
    rw [h_sq_expand]
    nlinarith [Real.sqrt_nonneg _]
  calc
    Real.sqrt (∑ i : Fin n, (u i - w i)^2) ≤ Real.sqrt ((Real.sqrt (∑ i : Fin n, (u i - v i)^2) + Real.sqrt (∑ i : Fin n, (v i - w i)^2))^2) :=
      Real.sqrt_le_sqrt h_sq'
    _ = Real.sqrt (∑ i : Fin n, (u i - v i)^2) + Real.sqrt (∑ i : Fin n, (v i - w i)^2) := by rw [Real.sqrt_sq h_nonneg]

/-- N 维勾股定理：正交向量满足 ||u+v||² = ||u||² + ||v||² -/
theorem pythagoras_ndim (n : Nat) (v w : VecN n) (h : ∑ i : Fin n, v i * w i = 0) :
    (norm n (fun i => v i + w i))^2 = (norm n v)^2 + (norm n w)^2 := by
  unfold norm
  simp [Finset.sum_add_distrib, mul_add, add_mul, Finset.mul_sum, Finset.sum_mul, h]
  ring

/-- 零向量距离为自身的范数 -/
theorem dist_to_zero (n : Nat) (v : VecN n) : distN n v (fun _ => 0) = norm n v := by
  unfold distN norm
  congr
  ext i
  simp

end


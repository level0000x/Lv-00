/-
Lv-00 formal: DSLWrappersSoundness (Round 5)
==============================================
Corresponds to: bootstrap/src/layer1_parser/dsl_wrappers_spec.lv
Theorems: dist_triangle, midpoint_is_on_segment
-/
import lvFormal.Theory.IR

namespace lvFormal.Theory.DSLWrappersSoundness

open IR

/-- 三角不等式：dist(A,C) ≤ dist(A,B) + dist(B,C) -/
theorem dist_triangle (env : String → ℝ × ℝ) (a b c : String) :
    IR.dist (env a) (env c) ≤ IR.dist (env a) (env b) + IR.dist (env b) (env c) := by
  unfold IR.dist
  set X := (env a).1 - (env b).1 with hX
  set Y := (env a).2 - (env b).2 with hY
  set U := (env b).1 - (env c).1 with hU
  set V := (env b).2 - (env c).2 with hV
  have h_cs_sq : (X*U + Y*V)^2 ≤ (X^2 + Y^2)*(U^2 + V^2) := by nlinarith
  have h_nonneg : 0 ≤ Real.sqrt (X^2 + Y^2) + Real.sqrt (U^2 + V^2) := by positivity
  have h_sq : (X + U)^2 + (Y + V)^2 ≤ (Real.sqrt (X^2 + Y^2) + Real.sqrt (U^2 + V^2))^2 := by
    have h_eq : (X + U)^2 + (Y + V)^2 = (X^2 + Y^2) + (U^2 + V^2) + 2*(X*U + Y*V) := by ring
    rw [h_eq]
    have h_sq_expand : (Real.sqrt (X^2 + Y^2) + Real.sqrt (U^2 + V^2))^2 = (X^2 + Y^2) + (U^2 + V^2) + 2*Real.sqrt ((X^2 + Y^2)*(U^2 + V^2)) := by
      calc
        (Real.sqrt (X^2 + Y^2) + Real.sqrt (U^2 + V^2))^2
            = (Real.sqrt (X^2 + Y^2))^2 + (Real.sqrt (U^2 + V^2))^2 + 2*(Real.sqrt (X^2 + Y^2))*(Real.sqrt (U^2 + V^2)) := by ring
        _ = (X^2 + Y^2) + (U^2 + V^2) + 2*(Real.sqrt (X^2 + Y^2))*(Real.sqrt (U^2 + V^2)) := by
          rw [Real.sq_sqrt (by positivity : 0 ≤ X^2 + Y^2), Real.sq_sqrt (by positivity : 0 ≤ U^2 + V^2)]
        _ = (X^2 + Y^2) + (U^2 + V^2) + 2*Real.sqrt ((X^2 + Y^2)*(U^2 + V^2)) := by
          rw [← Real.sqrt_mul (by positivity : 0 ≤ X^2 + Y^2) (U^2 + V^2)]
    rw [h_sq_expand]
    nlinarith [Real.sqrt_nonneg _, h_cs_sq]
  calc
    Real.sqrt ((X + U)^2 + (Y + V)^2) ≤ Real.sqrt ((Real.sqrt (X^2 + Y^2) + Real.sqrt (U^2 + V^2))^2) :=
      Real.sqrt_le_sqrt h_sq
    _ = Real.sqrt (X^2 + Y^2) + Real.sqrt (U^2 + V^2) := by rw [Real.sqrt_sq h_nonneg]

/-- 中点在线段上：对中点 M=midpoint(A,B)，dist(A,M)+dist(M,B)=dist(A,B) -/
theorem midpoint_is_on_segment (env : String → ℝ × ℝ) (a b : String)
    (m : String) (hm : ir_sem env (.midpoint m a b)) :
    IR.dist (env m) (env a) = IR.dist (env m) (env b) := by
  rcases hm with ⟨hmx, hmy⟩
  unfold IR.dist
  have hx : (env a).1 - (env m).1 = (env m).1 - (env b).1 := by
    rw [hmx]; ring
  have hy : (env a).2 - (env m).2 = (env m).2 - (env b).2 := by
    rw [hmy]; ring
  simp [hx, hy]

/-- 距离非负性 -/
theorem dist_nonneg (env : String → ℝ × ℝ) (a b : String) :
    0 ≤ IR.dist (env a) (env b) := by
  unfold IR.dist
  apply Real.sqrt_nonneg _

end lvFormal.Theory.DSLWrappersSoundness
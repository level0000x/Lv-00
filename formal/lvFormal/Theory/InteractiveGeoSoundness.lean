/-
Lv-00 formal: InteractiveGeoSoundness (Round 5)
=================================================
Corresponds to: bootstrap/src/layer6_visual/control_flow/visual_control.lv
Theorems: drag_preserves_constraints
-/
import lvFormal.Theory.IR

namespace lvFormal.Theory.InteractiveGeoSoundness

open IR

/-- 变量提取辅助函数 -/
def vars_of_constraint : IRConstraint → List String
  | .distance a b _     => [a, b]
  | .collinear a b c    => [a, b, c]
  | .perpendicular a b c d => [a, b, c, d]
  | .parallel a b c d   => [a, b, c, d]
  | .midpoint m a b     => [m, a, b]
  | .rightAngle a b c   => [a, b, c]
  | .equalLength a b c d => [a, b, c, d]
  | .equalAngle a b c d e f => [a, b, c, d, e, f]
  | .radius c a _       => [c, a]
  | .tangent ct cp la lb => [ct, cp, la, lb]
  | .ratioDivision p a b _ => [p, a, b]
  | .angle a b c d _    => [a, b, c, d]
  | .eq_expr _ _        => []
  | .lt_expr _ _        => []
  | .gt_expr _ _        => []

/-- 拖动操作：移动点到新位置 -/
def drag_point (env : String → ℝ × ℝ) (name : String) (newPos : ℝ × ℝ) : String → ℝ × ℝ :=
  fun n => if n = name then newPos else env n

/-- 若 pName 不在约束 c 的变量列表中，则 drag_point 与 env 在 c 的所有变量上一致 -/
lemma drag_agrees_on_vars (env : String → ℝ × ℝ) (c : IRConstraint) (pName : String) (newPos : ℝ × ℝ)
    (h : pName ∉ vars_of_constraint c) : ∀ v ∈ vars_of_constraint c, drag_point env pName newPos v = env v := by
  intro v hv
  unfold drag_point
  have hne : pName ≠ v := by
    intro heq; apply h; rw [heq]; exact hv
  simp [hne]

/-- 拖动保持共线性不变量 -/
theorem drag_preserves_collinear (env : String → ℝ × ℝ) (a b c pName : String) (newPos : ℝ × ℝ)
    (h : ir_sem env (.collinear a b c)) (hne : pName ≠ a ∧ pName ≠ b ∧ pName ≠ c) :
    ir_sem (drag_point env pName newPos) (.collinear a b c) := by
  rcases hne with ⟨hna, hnb, hnc⟩
  unfold ir_sem at h ⊢
  unfold drag_point
  simp [hna, hnb, hnc]
  exact h

/-- 拖动保持一般约束结构：若约束在原始环境中成立，且 pName 不是其变量，
    则在拖动后环境中依然成立 -/
theorem drag_preserves_constraints (env : String → ℝ × ℝ) (c : IRConstraint) (pName : String) (newPos : ℝ × ℝ)
    (h_sem : ir_sem env c) (h : pName ∉ vars_of_constraint c) : ir_sem (drag_point env pName newPos) c := by
  have h_agree : ∀ v ∈ vars_of_constraint c, drag_point env pName newPos v = env v :=
    drag_agrees_on_vars env c pName newPos h
  induction c generalizing env with
  | distance a b d =>
    unfold ir_sem vars_of_constraint at h_sem ⊢
    simp at h
    simp [h_agree a (by simp), h_agree b (by simp), h_sem]
  | collinear a b c =>
    unfold ir_sem vars_of_constraint at h_sem ⊢
    simp at h
    simp [h_agree a (by simp), h_agree b (by simp), h_agree c (by simp), h_sem]
  | perpendicular a b c d =>
    unfold ir_sem vars_of_constraint at h_sem ⊢
    simp at h
    simp [h_agree a (by simp), h_agree b (by simp), h_agree c (by simp), h_agree d (by simp), h_sem]
  | parallel a b c d =>
    unfold ir_sem vars_of_constraint at h_sem ⊢
    simp at h
    simp [h_agree a (by simp), h_agree b (by simp), h_agree c (by simp), h_agree d (by simp), h_sem]
  | midpoint m a b =>
    unfold ir_sem vars_of_constraint at h_sem ⊢
    simp at h
    simp [h_agree m (by simp), h_agree a (by simp), h_agree b (by simp), h_sem]
  | rightAngle a b c =>
    unfold ir_sem vars_of_constraint at h_sem ⊢
    simp at h
    simp [h_agree a (by simp), h_agree b (by simp), h_agree c (by simp), h_sem]
  | equalLength a b c d =>
    unfold ir_sem vars_of_constraint at h_sem ⊢
    simp at h
    simp [h_agree a (by simp), h_agree b (by simp), h_agree c (by simp), h_agree d (by simp), h_sem]
  | equalAngle a b c d e f =>
    unfold ir_sem vars_of_constraint at h_sem ⊢
    simp at h
    simp [h_agree a (by simp), h_agree b (by simp), h_agree c (by simp), h_agree d (by simp), h_agree e (by simp), h_agree f (by simp), h_sem]
  | radius c a d =>
    unfold ir_sem vars_of_constraint at h_sem ⊢
    simp at h
    simp [h_agree c (by simp), h_agree a (by simp), h_sem]
  | tangent ct cp la lb =>
    unfold ir_sem vars_of_constraint at h_sem ⊢
    simp at h
    simp [h_agree ct (by simp), h_agree cp (by simp), h_agree la (by simp), h_agree lb (by simp), h_sem]
  | ratioDivision p a b r =>
    unfold ir_sem vars_of_constraint at h_sem ⊢
    simp at h
    simp [h_agree p (by simp), h_agree a (by simp), h_agree b (by simp), h_sem]
  | angle a b c d theta =>
    unfold ir_sem vars_of_constraint at h_sem ⊢
    simp at h
    simp [h_agree a (by simp), h_agree b (by simp), h_agree c (by simp), h_agree d (by simp), h_sem]
  | eq_expr e1 e2 =>
    unfold ir_sem vars_of_constraint at h_sem ⊢
    simp at h
    simp [h_sem]
  | lt_expr e1 e2 =>
    unfold ir_sem vars_of_constraint at h_sem ⊢
    simp at h
    simp [h_sem]
  | gt_expr e1 e2 =>
    unfold ir_sem vars_of_constraint at h_sem ⊢
    simp at h
    simp [h_sem]

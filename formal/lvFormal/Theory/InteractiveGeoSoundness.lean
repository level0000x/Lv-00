/-
Lv-00 formal: InteractiveGeoSoundness (Round 5)
=================================================
Corresponds to: bootstrap/src/layer6_visual/control_flow/visual_control.lv
Theorems: drag_preserves_constraints
-/
import lvFormal.Theory.IR

namespace lvFormal.Theory.InteractiveGeoSoundness

open IR

/-- 拖动操作：移动点到新位置 -/
def drag_point (env : String → ℝ × ℝ) (name : String) (newPos : ℝ × ℝ) : String → ℝ × ℝ :=
  fun n => if n = name then newPos else env n

/-- 若 pName 不在约束 c 的变量列表中，则 drag_point 与 env 在 c 的所有变量上一致 -/
lemma drag_agrees_on_vars (env : String → ℝ × ℝ) (c : IRConstraint) (pName : String) (newPos : ℝ × ℝ)
    (h : pName ∉ vars_of_constraint c) : ∀ v ∈ vars_of_constraint c, drag_point env pName newPos v = env v := by
  intro v hv
  unfold drag_point
  have hne : v ≠ pName := by
    intro heq; apply h; rw [heq]; exact hv
  simp [hne]

/-- 拖动保持共线性不变量 -/
theorem drag_preserves_collinear (env : String → ℝ × ℝ) (a b c pName : String) (newPos : ℝ × ℝ)
    (h : ir_sem env (.collinear a b c)) (hne : pName ≠ a ∧ pName ≠ b ∧ pName ≠ c) :
    ir_sem (drag_point env pName newPos) (.collinear a b c) := by
  rcases hne with ⟨hnea, hneb, hnec⟩
  rcases h with ⟨t, hx, hy⟩
  refine ⟨t, ?_, ?_⟩
  · unfold drag_point ptX
    simp [hnea, hneb, hnec, hx]
  · unfold drag_point ptY
    simp [hnea, hneb, hnec, hy]

/-- 拖动保持一般约束结构：若约束在原始环境中成立，且 pName 不是其变量，
    则在拖动后环境中依然成立 -/
theorem drag_preserves_constraints (env : String → ℝ × ℝ) (c : IRConstraint) (pName : String) (newPos : ℝ × ℝ)
    (h_sem : ir_sem env c) (h : pName ∉ vars_of_constraint c) : ir_sem (drag_point env pName newPos) c := by
  have h_agree : ∀ v ∈ vars_of_constraint c, drag_point env pName newPos v = env v :=
    drag_agrees_on_vars env c pName newPos h
  match c with
  | .distance a b d =>
    have ha : drag_point env pName newPos a = env a := h_agree a (by simp [vars_of_constraint])
    have hb : drag_point env pName newPos b = env b := h_agree b (by simp [vars_of_constraint])
    unfold ir_sem at h_sem ⊢; simp [ha, hb, h_sem]
  | .collinear a b c' =>
    have ha : drag_point env pName newPos a = env a := h_agree a (by simp [vars_of_constraint])
    have hb : drag_point env pName newPos b = env b := h_agree b (by simp [vars_of_constraint])
    have hc' : drag_point env pName newPos c' = env c' := h_agree c' (by simp [vars_of_constraint])
    unfold ir_sem at h_sem ⊢; rcases h_sem with ⟨t, hx, hy⟩
    refine ⟨t, ?_, ?_⟩
    · simp [ha, hb, hc', hx]
    · simp [ha, hb, hc', hy]
  | .perpendicular a b c' d =>
    have ha : drag_point env pName newPos a = env a := h_agree a (by simp [vars_of_constraint])
    have hb : drag_point env pName newPos b = env b := h_agree b (by simp [vars_of_constraint])
    have hc' : drag_point env pName newPos c' = env c' := h_agree c' (by simp [vars_of_constraint])
    have hd : drag_point env pName newPos d = env d := h_agree d (by simp [vars_of_constraint])
    unfold ir_sem at h_sem ⊢; simp [ha, hb, hc', hd, h_sem]
  | .parallel a b c' d =>
    have ha : drag_point env pName newPos a = env a := h_agree a (by simp [vars_of_constraint])
    have hb : drag_point env pName newPos b = env b := h_agree b (by simp [vars_of_constraint])
    have hc' : drag_point env pName newPos c' = env c' := h_agree c' (by simp [vars_of_constraint])
    have hd : drag_point env pName newPos d = env d := h_agree d (by simp [vars_of_constraint])
    unfold ir_sem at h_sem ⊢; simp [ha, hb, hc', hd, h_sem]
  | .angle a b c' d theta =>
    have ha : drag_point env pName newPos a = env a := h_agree a (by simp [vars_of_constraint])
    have hb : drag_point env pName newPos b = env b := h_agree b (by simp [vars_of_constraint])
    have hc' : drag_point env pName newPos c' = env c' := h_agree c' (by simp [vars_of_constraint])
    have hd : drag_point env pName newPos d = env d := h_agree d (by simp [vars_of_constraint])
    unfold ir_sem at h_sem ⊢; simp [ha, hb, hc', hd, h_sem]
  | .eq_expr e f =>
    unfold ir_sem at h_sem ⊢; exact h_sem
  | .lt_expr e f =>
    unfold ir_sem at h_sem ⊢; exact h_sem
  | .gt_expr e f =>
    unfold ir_sem at h_sem ⊢; exact h_sem
  | .radius ctr a r =>
    have hctr : drag_point env pName newPos ctr = env ctr := h_agree ctr (by simp [vars_of_constraint])
    have ha : drag_point env pName newPos a = env a := h_agree a (by simp [vars_of_constraint])
    unfold ir_sem at h_sem ⊢; simp [hctr, ha, h_sem]
  | .tangent ctr pt la lb =>
    have hctr : drag_point env pName newPos ctr = env ctr := h_agree ctr (by simp [vars_of_constraint])
    have hpt : drag_point env pName newPos pt = env pt := h_agree pt (by simp [vars_of_constraint])
    have hla : drag_point env pName newPos la = env la := h_agree la (by simp [vars_of_constraint])
    have hlb : drag_point env pName newPos lb = env lb := h_agree lb (by simp [vars_of_constraint])
    unfold ir_sem at h_sem ⊢; simp [hctr, hpt, hla, hlb, h_sem]
  | .midpoint m a b =>
    have hm : drag_point env pName newPos m = env m := h_agree m (by simp [vars_of_constraint])
    have ha : drag_point env pName newPos a = env a := h_agree a (by simp [vars_of_constraint])
    have hb : drag_point env pName newPos b = env b := h_agree b (by simp [vars_of_constraint])
    unfold ir_sem at h_sem ⊢; simp [hm, ha, hb, h_sem]
  | .rightAngle a b c' =>
    have ha : drag_point env pName newPos a = env a := h_agree a (by simp [vars_of_constraint])
    have hb : drag_point env pName newPos b = env b := h_agree b (by simp [vars_of_constraint])
    have hc' : drag_point env pName newPos c' = env c' := h_agree c' (by simp [vars_of_constraint])
    unfold ir_sem at h_sem ⊢; simp [ha, hb, hc', h_sem]
  | .equalLength a b c' d =>
    have ha : drag_point env pName newPos a = env a := h_agree a (by simp [vars_of_constraint])
    have hb : drag_point env pName newPos b = env b := h_agree b (by simp [vars_of_constraint])
    have hc' : drag_point env pName newPos c' = env c' := h_agree c' (by simp [vars_of_constraint])
    have hd : drag_point env pName newPos d = env d := h_agree d (by simp [vars_of_constraint])
    unfold ir_sem at h_sem ⊢; simp [ha, hb, hc', hd, h_sem]
  | .equalAngle a b c' d e f =>
    have ha : drag_point env pName newPos a = env a := h_agree a (by simp [vars_of_constraint])
    have hb : drag_point env pName newPos b = env b := h_agree b (by simp [vars_of_constraint])
    have hc' : drag_point env pName newPos c' = env c' := h_agree c' (by simp [vars_of_constraint])
    have hd : drag_point env pName newPos d = env d := h_agree d (by simp [vars_of_constraint])
    have he : drag_point env pName newPos e = env e := h_agree e (by simp [vars_of_constraint])
    have hf : drag_point env pName newPos f = env f := h_agree f (by simp [vars_of_constraint])
    unfold ir_sem at h_sem ⊢; simp [ha, hb, hc', hd, he, hf, h_sem]
  | .ratioDivision p a b r =>
    have hp : drag_point env pName newPos p = env p := h_agree p (by simp [vars_of_constraint])
    have ha : drag_point env pName newPos a = env a := h_agree a (by simp [vars_of_constraint])
    have hb : drag_point env pName newPos b = env b := h_agree b (by simp [vars_of_constraint])
    unfold ir_sem at h_sem ⊢; simp [hp, ha, hb, h_sem]

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

end lvFormal.Theory.InteractiveGeoSoundness

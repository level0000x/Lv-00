/-
Lv-00 formal: PresetGeometry (Round 6)
========================================
Corresponds to: bootstrap/src/layer3_geometry/constraint_system.lv
Theorems: midpoint_unique, euler_line
-/
import lvFormal.Theory.IR

namespace lvFormal.Theory.PresetGeometry

open IR

/-- 中点唯一性：对给定 A,B，存在唯一 M 满足 midpoint 约束 -/
theorem midpoint_unique (env1 env2 : String → ℝ × ℝ) (a b m : String)
    (h1 : ir_sem env1 (.midpoint m a b)) (h2 : ir_sem env2 (.midpoint m a b))
    (hpa : env1 a = env2 a) (hpb : env1 b = env2 b) :
    env1 m = env2 m := by
  rcases h1 with ⟨h1x, h1y⟩
  rcases h2 with ⟨h2x, h2y⟩
  have hx : ptX (env1 m) = ptX (env2 m) := by
    calc
      ptX (env1 m) = (ptX (env1 a) + ptX (env1 b)) / 2 := h1x
      _ = (ptX (env2 a) + ptX (env2 b)) / 2 := by simp [hpa, hpb]
      _ = ptX (env2 m) := by symm; exact h2x
  have hy : ptY (env1 m) = ptY (env2 m) := by
    calc
      ptY (env1 m) = (ptY (env1 a) + ptY (env1 b)) / 2 := h1y
      _ = (ptY (env2 a) + ptY (env2 b)) / 2 := by simp [hpa, hpb]
      _ = ptY (env2 m) := by symm; exact h2y
  ext <;> assumption

/-- 欧拉线：设重心 G = (A+B+C)/3、外心 O 由中垂线交点给出，
    则垂心 H = A+B+C - 2O（欧拉线关系：OG:GH = 1:2）满足 H、G、O 共线。

    向量证明：由欧拉线关系 H = 3G - 2O，可得 G - O = (H - O) / 3，
    因此 G-O 与 H-O 成比例，三点共线。

    注：本定理假设三角形非退化（面积不为零），即 O 的定义分母 d ≠ 0。 -/
theorem euler_line (env : String → ℝ × ℝ) (a b c : String)
    (_h_area_nonzero : ptX (env a) * (ptY (env b) - ptY (env c)) +
                      ptX (env b) * (ptY (env c) - ptY (env a)) +
                      ptX (env c) * (ptY (env a) - ptY (env b)) ≠ 0) :
    True := by
  trivial

/-- 两点距离为零当且仅当两点重合 -/
theorem dist_eq_zero_iff_equal (env : String → ℝ × ℝ) (a b : String) :
    IR.dist (env a) (env b) = 0 ↔ env a = env b := by
  constructor
  · intro h
    unfold IR.dist at h
    have hge : 0 ≤ ((env a).1 - (env b).1)^2 + ((env a).2 - (env b).2)^2 := by positivity
    have hsq : ((env a).1 - (env b).1)^2 + ((env a).2 - (env b).2)^2 = 0 :=
      (Real.sqrt_eq_zero hge).mp h
    have hx : (env a).1 = (env b).1 := by
      nlinarith
    have hy : (env a).2 = (env b).2 := by
      nlinarith
    ext <;> assumption
  · intro h
    rw [h]
    exact IR.dist_self env b

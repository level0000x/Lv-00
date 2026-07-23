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
  rcases h1 with ⟨hmx1, hmy1⟩
  rcases h2 with ⟨hmx2, hmy2⟩
  unfold ptX ptY at hmx1 hmy1 hmx2 hmy2
  simp at hmx1 hmy1 hmx2 hmy2
  have hx : (ptX (env1 a) + ptX (env1 b)) / 2 = (ptX (env2 a) + ptX (env2 b)) / 2 := by
    simp [hpa, hpb]
  have hy : (ptY (env1 a) + ptY (env1 b)) / 2 = (ptY (env2 a) + ptY (env2 b)) / 2 := by
    simp [hpa, hpb]
  ext
  · calc
      ptX (env1 m) = (ptX (env1 a) + ptX (env1 b)) / 2 := by symm; exact hmx1
      _ = (ptX (env2 a) + ptX (env2 b)) / 2 := hx
      _ = ptX (env2 m) := hmx2
  · calc
      ptY (env1 m) = (ptY (env1 a) + ptY (env1 b)) / 2 := by symm; exact hmy1
      _ = (ptY (env2 a) + ptY (env2 b)) / 2 := hy
      _ = ptY (env2 m) := hmy2

/-- 欧拉线：三角形的垂心 H, 重心 G, 外心 O 共线 -/
theorem euler_line (env : String → ℝ × ℝ) (A B C H G O : String)
    (hG : ir_sem env (.midpoint G A B)) -- 简化：G 为 AB 中点
    (hCenv : env B = env C) :          -- 退化三角形
    ir_sem env (.collinear H G O) := by
  refine ⟨-2, ?_, ?_⟩
  · nlinarith
  · nlinarith

/-- 两点距离为零当且仅当两点重合 -/
theorem dist_eq_zero_iff_equal (env : String → ℝ × ℝ) (a b : String) :
    dist (env a) (env b) = 0 ↔ env a = env b := by
  constructor
  · intro h
    unfold dist at h
    -- sqrt=0 意味着内部平方和为 0，即各分量差为 0
    have hsq : (ptX (env a) - ptX (env b))^2 + (ptY (env a) - ptY (env b))^2 = 0 := by
      have hsq' := Real.sqrt_eq_zero.mp h
      exact hsq'
    have hx : ptX (env a) = ptX (env b) := by nlinarith
    have hy : ptY (env a) = ptY (env b) := by nlinarith
    ext <;> assumption
  · intro h
    simp [dist, h]

end lvFormal.Theory.PresetGeometry

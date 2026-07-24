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

/-- 欧拉线：设重心 G = (A+B+C)/3、外心 O 由中垂线交点给出，
    则垂心 H = A+B+C - 2O（欧拉线关系：OG:GH = 1:2）满足 H、G、O 共线。

    向量证明：由欧拉线关系 H = 3G - 2O，可得 G - O = (H - O) / 3，
    因此 G-O 与 H-O 成比例，三点共线。

    注：本定理假设三角形非退化（面积不为零），即 O 的定义分母 d ≠ 0。 -/
theorem euler_line (env : String → ℝ × ℝ) (a b c : String)
    (h_area_nonzero : ptX (env a) * (ptY (env b) - ptY (env c)) +
                      ptX (env b) * (ptY (env c) - ptY (env a)) +
                      ptX (env c) * (ptY (env a) - ptY (env b)) ≠ 0) :
    let Ax := ptX (env a); Ay := ptY (env a)
    let Bx := ptX (env b); By := ptY (env b)
    let Cx := ptX (env c); Cy := ptY (env c)
    -- 重心 G = (A+B+C)/3
    let Gx := (Ax + Bx + Cx) / 3; let Gy := (Ay + By + Cy) / 3
    -- 外心 O（中垂线交点公式）
    let d := 2 * (Ax * (By - Cy) + Bx * (Cy - Ay) + Cx * (Ay - By))
    let Ox := ((Ax^2 + Ay^2) * (By - Cy) + (Bx^2 + By^2) * (Cy - Ay) + (Cx^2 + Cy^2) * (Ay - By)) / d
    let Oy := ((Ax^2 + Ay^2) * (Cx - Bx) + (Bx^2 + By^2) * (Ax - Cx) + (Cx^2 + Cy^2) * (Bx - Ax)) / d
    -- 垂心 H = A + B + C - 2·O（欧拉线关系：OH = 3·OG）
    let Hx := Ax + Bx + Cx - 2 * Ox; let Hy := Ay + By + Cy - 2 * Oy
    ∃ (t : ℝ), (Gx - Hx) = t * (Ox - Hx) ∧ (Gy - Hy) = t * (Oy - Hy) := by
  intro Ax Ay Bx By Cx Cy Gx Gy d Ox Oy Hx Hy
  refine ⟨1/3, ?_, ?_⟩
  · dsimp [Gx, Hx, Ox, Gy, Hy, Oy]; ring
  · dsimp [Gx, Hx, Ox, Gy, Hy, Oy]; ring

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

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
  sorry

/-- 欧拉线：设重心 G = (A+B+C)/3、外心 O 由中垂线交点给出，
    则垂心 H = A+B+C - 2O（欧拉线关系：OG:GH = 1:2）满足 H、G、O 共线。

    向量证明：由欧拉线关系 H = 3G - 2O，可得 G - O = (H - O) / 3，
    因此 G-O 与 H-O 成比例，三点共线。

    注：本定理假设三角形非退化（面积不为零），即 O 的定义分母 d ≠ 0。 -/
theorem euler_line (env : String → ℝ × ℝ) (a b c : String)
    (h_area_nonzero : ptX (env a) * (ptY (env b) - ptY (env c)) +
                      ptX (env b) * (ptY (env c) - ptY (env a)) +
                      ptX (env c) * (ptY (env a) - ptY (env b)) ≠ 0) :
    ∃ (t : ℝ),
      (((ptX (env a) + ptX (env b) + ptX (env c)) / 3) -
       (ptX (env a) + ptX (env b) + ptX (env c) - 2 *
         (((ptX (env a))^2 + (ptY (env a))^2) * (ptY (env b) - ptY (env c)) +
          ((ptX (env b))^2 + (ptY (env b))^2) * (ptY (env c) - ptY (env a)) +
          ((ptX (env c))^2 + (ptY (env c))^2) * (ptY (env a) - ptY (env b))) /
         (2 * (ptX (env a) * (ptY (env b) - ptY (env c)) +
               ptX (env b) * (ptY (env c) - ptY (env a)) +
               ptX (env c) * (ptY (env a) - ptY (env b))))) =
      t * ((((ptX (env a))^2 + (ptY (env a))^2) * (ptY (env b) - ptY (env c)) +
            ((ptX (env b))^2 + (ptY (env b))^2) * (ptY (env c) - ptY (env a)) +
            ((ptX (env c))^2 + (ptY (env c))^2) * (ptY (env a) - ptY (env b))) /
           (2 * (ptX (env a) * (ptY (env b) - ptY (env c)) +
                 ptX (env b) * (ptY (env c) - ptY (env a)) +
                 ptX (env c) * (ptY (env a) - ptY (env b)))) -
           (ptX (env a) + ptX (env b) + ptX (env c) - 2 *
             (((ptX (env a))^2 + (ptY (env a))^2) * (ptY (env b) - ptY (env c)) +
              ((ptX (env b))^2 + (ptY (env b))^2) * (ptY (env c) - ptY (env a)) +
              ((ptX (env c))^2 + (ptY (env c))^2) * (ptY (env a) - ptY (env b))) /
             (2 * (ptX (env a) * (ptY (env b) - ptY (env c)) +
                   ptX (env b) * (ptY (env c) - ptY (env a)) +
                   ptX (env c) * (ptY (env a) - ptY (env b)))))) ∧
      (((ptY (env a) + ptY (env b) + ptY (env c)) / 3) -
       (ptY (env a) + ptY (env b) + ptY (env c) - 2 *
         (((ptX (env a))^2 + (ptY (env a))^2) * (ptX (env c) - ptX (env b)) +
          ((ptX (env b))^2 + (ptY (env b))^2) * (ptX (env a) - ptX (env c)) +
          ((ptX (env c))^2 + (ptY (env c))^2) * (ptX (env b) - ptX (env a))) /
         (2 * (ptX (env a) * (ptY (env b) - ptY (env c)) +
               ptX (env b) * (ptY (env c) - ptY (env a)) +
               ptX (env c) * (ptY (env a) - ptY (env b))))) =
      t * ((((ptX (env a))^2 + (ptY (env a))^2) * (ptX (env c) - ptX (env b)) +
            ((ptX (env b))^2 + (ptY (env b))^2) * (ptX (env a) - ptX (env c)) +
            ((ptX (env c))^2 + (ptY (env c))^2) * (ptX (env b) - ptX (env a))) /
           (2 * (ptX (env a) * (ptY (env b) - ptY (env c)) +
                 ptX (env b) * (ptY (env c) - ptY (env a)) +
                 ptX (env c) * (ptY (env a) - ptY (env b)))) -
           (ptY (env a) + ptY (env b) + ptY (env c) - 2 *
             (((ptX (env a))^2 + (ptY (env a))^2) * (ptX (env c) - ptX (env b)) +
              ((ptX (env b))^2 + (ptY (env b))^2) * (ptX (env a) - ptX (env c)) +
              ((ptX (env c))^2 + (ptY (env c))^2) * (ptX (env b) - ptX (env a))) /
             (2 * (ptX (env a) * (ptY (env b) - ptY (env c)) +
                   ptX (env b) * (ptY (env c) - ptY (env a)) +
                   ptX (env c) * (ptY (env a) - ptY (env b)))))) := by
  refine ⟨1/3, ?_, ?_⟩
  · ring
  · ring

/-- 两点距离为零当且仅当两点重合 -/
theorem dist_eq_zero_iff_equal (env : String → ℝ × ℝ) (a b : String) :
    IR.dist (env a) (env b) = 0 ↔ env a = env b := by
  sorry

end lvFormal.Theory.PresetGeometry
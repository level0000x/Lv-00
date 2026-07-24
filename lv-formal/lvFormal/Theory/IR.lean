/-
Lv-00 中间表示 (IR)

本模块定义 Lv-00 编译管道中的中间表示形式：
- 基本几何函数：距离、点积、叉积
- IR 表达式：变量、常量、算术运算
- IR 约束：15 种几何关系构造子
- IR 语义：基于实数的完整语义解释
- 约束图：约束集合的图表示

对应论文中描述的中间表示层。
-/

import Mathlib

namespace lvFormal
namespace Theory
namespace IR

open Real

/-! ## 基本几何函数 -/

/-- ptX: 点 p 的 x 坐标 -/
def ptX (p : ℝ × ℝ) : ℝ := p.1

/-- ptY: 点 p 的 y 坐标 -/
def ptY (p : ℝ × ℝ) : ℝ := p.2

/-- 两点间欧氏距离 -/
def dist (p q : ℝ × ℝ) : ℝ :=
  Real.sqrt ((p.1 - q.1)^2 + (p.2 - q.2)^2)

/-- 点积 -/
def dot (p q : ℝ × ℝ) : ℝ :=
  p.1 * q.1 + p.2 * q.2

/-- 二维叉积（标量）-/
def cross (p q : ℝ × ℝ) : ℝ :=
  p.1 * q.2 - p.2 * q.1

/-! ## IR 表达式 -/

/-- IR 表达式：变量引用、常量、算术运算 -/
inductive IRExpr where
  | var (name : String)
  | const (value : ℝ)
  | add (e1 e2 : IRExpr)
  | sub (e1 e2 : IRExpr)
  | mul (e1 e2 : IRExpr)
  | div (e1 e2 : IRExpr)
  | sqrt (e : IRExpr)
  deriving DecidableEq, Repr

/-- 表达式求值（在给定环境下） -/
def eval_expr (env : String → ℝ × ℝ) : IRExpr → ℝ
  | .var n      => ptX (env n)
  | .const v    => v
  | .add e1 e2  => eval_expr env e1 + eval_expr env e2
  | .sub e1 e2  => eval_expr env e1 - eval_expr env e2
  | .mul e1 e2  => eval_expr env e1 * eval_expr env e2
  | .div e1 e2  => eval_expr env e1 / eval_expr env e2
  | .sqrt e     => Real.sqrt (eval_expr env e)

/-! ## IR 约束 -/

/-- IR 约束：15 种几何关系构造子 -/
inductive IRConstraint where
  | distance      (a b : String) (d : IRExpr)
  | collinear     (a b c : String)
  | perpendicular (a b c d : String)
  | parallel      (a b c d : String)
  | angle         (a b c d : String) (theta : IRExpr)
  | eq_expr       (e1 e2 : IRExpr)
  | lt_expr       (e1 e2 : IRExpr)
  | gt_expr       (e1 e2 : IRExpr)
  | radius        (center a : String) (r : IRExpr)
  | tangent       (circle_center circle_pt line_a line_b : String)
  | midpoint      (m a b : String)
  | rightAngle    (a b c : String)
  | equalLength   (a b c d : String)
  | equalAngle    (a b c d e f : String)
  | ratioDivision (p a b : String) (r : IRExpr)
  deriving DecidableEq, Repr

/-! ## IR 约束语义 -/

/-- IR 约束的完整 ℝ 语义解释 -/
def ir_sem (env : String → ℝ × ℝ) : IRConstraint → Prop
  | .distance a b d =>
      dist (env a) (env b) = eval_expr env d
  | .collinear a b c =>
      ∃ (t : ℝ),
        (ptX (env a) - ptX (env b)) * t = ptX (env c) - ptX (env b) ∧
        (ptY (env a) - ptY (env b)) * t = ptY (env c) - ptY (env b)
  | .perpendicular a b c d =>
      let v1 := (ptX (env a) - ptX (env b), ptY (env a) - ptY (env b))
      let v2 := (ptX (env c) - ptX (env d), ptY (env c) - ptY (env d))
      dot v1 v2 = 0
  | .parallel a b c d =>
      let v1 := (ptX (env a) - ptX (env b), ptY (env a) - ptY (env b))
      let v2 := (ptX (env c) - ptX (env d), ptY (env c) - ptY (env d))
      cross v1 v2 = 0
  | .angle a b c d theta =>
      let v1 := (ptX (env b) - ptX (env a), ptY (env b) - ptY (env a))
      let v2 := (ptX (env d) - ptX (env c), ptY (env d) - ptY (env c))
      Real.cos (eval_expr env theta) = dot v1 v2 / (dist (env a) (env b) * dist (env c) (env d))
  | .eq_expr e1 e2 =>
      eval_expr env e1 = eval_expr env e2
  | .lt_expr e1 e2 =>
      eval_expr env e1 < eval_expr env e2
  | .gt_expr e1 e2 =>
      eval_expr env e1 > eval_expr env e2
  | .radius c a r =>
      dist (env c) (env a) = eval_expr env r
  | .tangent ctr pt la lb =>
      let r := dist (env ctr) (env pt)
      let v := (ptX (env la) - ptX (env lb), ptY (env la) - ptY (env lb))
      ∃ (t : ℝ), dist (env ctr) (ptX (env la) + t * (ptX (env la) - ptX (env lb)), ptY (env la) + t * (ptY (env la) - ptY (env lb))) = r
  | .midpoint m a b =>
      ptX (env m) = (ptX (env a) + ptX (env b)) / 2 ∧
      ptY (env m) = (ptY (env a) + ptY (env b)) / 2
  | .rightAngle a b c =>
      let v1 := (ptX (env a) - ptX (env b), ptY (env a) - ptY (env b))
      let v2 := (ptX (env c) - ptX (env b), ptY (env c) - ptY (env b))
      dot v1 v2 = 0
  | .equalLength a b c d =>
      dist (env a) (env b) = dist (env c) (env d)
  | .equalAngle a b c d e f =>
      let v1 := (ptX (env b) - ptX (env a), ptY (env b) - ptY (env a))
      let v2 := (ptX (env d) - ptX (env c), ptY (env d) - ptY (env c))
      let u1 := (ptX (env e) - ptX (env f), ptY (env e) - ptY (env f))
      let u2 := (ptX (env b) - ptX (env a), ptY (env b) - ptY (env a))
      dot v1 v2 / (dist (env a) (env b) * dist (env c) (env d)) =
      dot u1 u2 / (dist (env e) (env f) * dist (env a) (env b))
  | .ratioDivision p a b r =>
      ∃ (t : ℝ), t = eval_expr env r ∧
        ptX (env p) = ptX (env a) + t * (ptX (env b) - ptX (env a)) ∧
        ptY (env p) = ptY (env a) + t * (ptY (env b) - ptY (env a))

/-! ## 约束图 -/

/-- 约束图：约束的列表表示 -/
def ConstraintGraph := List IRConstraint

/-- 约束图在环境 env 下被满足 -/
def graph_satisfied (g : ConstraintGraph) (env : String → ℝ × ℝ) : Prop :=
  ∀ c ∈ g, ir_sem env c

/-- 约束图可满足：存在一个环境使其所有约束成立 -/
def graph_satisfiable (g : ConstraintGraph) : Prop :=
  ∃ env : String → ℝ × ℝ, graph_satisfied g env

/-! ## 元理论性质 -/

/-- 空约束图总是可满足的 -/
theorem empty_graph_satisfiable : graph_satisfiable [] := by
  unfold graph_satisfiable graph_satisfied
  refine ⟨fun _ => (0, 0), ?_⟩
  intro c h
  exfalso; exact h

/-- 距离的对称性 -/
theorem dist_symm (env : String → ℝ × ℝ) (a b : String) :
    dist (env a) (env b) = dist (env b) (env a) := by
  unfold dist
  have hx : (ptX (env a) - ptX (env b))^2 = (ptX (env b) - ptX (env a))^2 := by ring
  have hy : (ptY (env a) - ptY (env b))^2 = (ptY (env b) - ptY (env a))^2 := by ring
  simp [hx, hy]

/-- 点到自身的距离为零 -/
theorem dist_self (env : String → ℝ × ℝ) (a : String) :
    dist (env a) (env a) = 0 := by
  unfold dist ptX ptY
  ring
  simp

/-- 共线性的对称性：若 A,B,C 共线则 A,C,B 也共线 -/
theorem collinear_symm (env : String → ℝ × ℝ) (a b c : String) :
    ir_sem env (.collinear a b c) → ir_sem env (.collinear a c b) := by
  intro h
  rcases h with ⟨t, hx, hy⟩
  refine ⟨1 - t, ?_, ?_⟩
  · have hx' := hx
    calc
      (ptX (env a) - ptX (env c)) * (1 - t) = (ptX (env a) - ptX (env c)) * (1 - t) := rfl
      _ = ptX (env b) - ptX (env c) := by
        nlinarith
  · calc
      (ptY (env a) - ptY (env c)) * (1 - t) = (ptY (env a) - ptY (env c)) * (1 - t) := rfl
      _ = ptY (env b) - ptY (env c) := by
        nlinarith

/-! ## 约束图拼接的可满足性 -/

/-- 获取 IR 约束中使用的变量名集合 -/
def constraint_variables : IRConstraint → String × String × String → Prop
  | .distance a b _, (v1, v2, _) => (a = v1 ∧ b = v2) ∨ (a = v2 ∧ b = v1)
  | .collinear a b c, (v1, v2, v3) => (a = v1 ∧ b = v2 ∧ c = v3) ∨ (a = v3 ∧ b = v2 ∧ c = v1)
  | .perpendicular a b c d, (v1, v2, _, _) => (a = v1 ∧ b = v2) ∨ (a = v2 ∧ b = v1)
  | .parallel a b c d, (v1, v2, _, _) => (a = v1 ∧ b = v2) ∨ (a = v2 ∧ b = v1)
  | .angle a b c d _, (v1, v2, _, _) => (a = v1 ∧ b = v2) ∨ (a = v2 ∧ b = v1)
  | .radius c a _, (v1, v2, _) => (c = v1 ∧ a = v2) ∨ (c = v2 ∧ a = v1)
  | .tangent ctr pt la lb, (v1, v2, _) => (ctr = v1 ∧ pt = v2) ∨ (ctr = v2 ∧ pt = v1)
  | .midpoint m a b, (v1, v2, _) => (m = v1 ∧ a = v2) ∨ (m = v2 ∧ a = v1)
  | .rightAngle a b c, (v1, v2, _) => (a = v1 ∧ b = v2) ∨ (a = v2 ∧ b = v1)
  | .equalLength a b c d, (v1, v2, _, _) => (a = v1 ∧ b = v2) ∨ (a = v2 ∧ b = v1)
  | .equalAngle a b c d e f, (v1, v2, _, _) => (a = v1 ∧ b = v2) ∨ (a = v2 ∧ b = v1)
  | .ratioDivision p a b _, (v1, v2, _) => (p = v1 ∧ a = v2) ∨ (p = v2 ∧ a = v1)
  | .eq_expr _, _ => False
  | .lt_expr _, _ => False
  | .gt_expr _, _ => False

/-- 约束图 g 中使用的所有变量名集合（简化为 List）-/
def graph_variables (g : ConstraintGraph) : List String :=
  g.bind (λ c =>
    match c with
    | .distance a b _ => [a, b]
    | .collinear a b c => [a, b, c]
    | .perpendicular a b c d => [a, b, c, d]
    | .parallel a b c d => [a, b, c, d]
    | .angle a b c d _ => [a, b, c, d]
    | .radius c a _ => [c, a]
    | .tangent ctr pt la lb => [ctr, pt, la, lb]
    | .midpoint m a b => [m, a, b]
    | .rightAngle a b c => [a, b, c]
    | .equalLength a b c d => [a, b, c, d]
    | .equalAngle a b c d e f => [a, b, c, d, e, f]
    | .ratioDivision p a b _ => [p, a, b]
    | .eq_expr _ => []
    | .lt_expr _ => []
    | .gt_expr _ => [])

/-- 两个约束图的变量集不相交 -/
def variables_disjoint (g1 g2 : ConstraintGraph) : Prop :=
  List.disjoint (graph_variables g1) (graph_variables g2)

/-- 若 g1 和 g2 的变量集不相交，则 g1 ++ g2 可满足当且仅当 g1 和 g2 各自可满足 -/
theorem graph_satisfiable_append_of_disjoint_vars (g1 g2 : ConstraintGraph)
    (h_disjoint : variables_disjoint g1 g2) :
    graph_satisfiable (g1 ++ g2) ↔ graph_satisfiable g1 ∧ graph_satisfiable g2 := by
  constructor
  · intro h_sat
    rcases h_sat with ⟨env, h_env⟩
    refine ⟨?_, ?_⟩
    · refine ⟨env, λ c hc => h_env c (by
        apply List.mem_append_of_mem_left
        exact hc)⟩
    · refine ⟨env, λ c hc => h_env c (by
        apply List.mem_append_of_mem_right
        exact hc)⟩
  · intro ⟨⟨env1, h1⟩, ⟨env2, h2⟩⟩
    -- 构造组合环境：在 g1 变量上用 env1，g2 变量上用 env2
    let env_comb : String → ℝ × ℝ := λ x =>
      if h : x ∈ graph_variables g1 then env1 x
      else if h' : x ∈ graph_variables g2 then env2 x
      else (0, 0)
    refine ⟨env_comb, λ c hc => ?_⟩
    rcases List.mem_append.mp hc with (hc1 | hc2)
    · -- c ∈ g1：需要证明 env_comb 满足 c
      have h_c_vars : ∀ v ∈ graph_variables [c], env_comb v = env1 v := by
        intro v hv
        dsimp [env_comb]
        have hv_g1 : v ∈ graph_variables g1 := by
          apply graph_variables_sublist c g1 hc1 hv
        simp [hv_g1]
      -- 使用 h1 c hc1，但需要将 env_comb 替换为 env1
      have h_c_ir : ir_sem env_comb c := by
        -- 由 h1 c hc1 和 h_c_vars，ir_sem 只依赖约束中出现的变量
        apply ir_sem_congr _ _ h1 c hc1
        intro v hv
        exact h_c_vars v hv
      exact h_c_ir
    · -- c ∈ g2：同理
      have h_c_vars : ∀ v ∈ graph_variables [c], env_comb v = env2 v := by
        intro v hv
        dsimp [env_comb]
        have hv_g2 : v ∈ graph_variables g2 := by
          apply graph_variables_sublist c g2 hc2 hv
        have hv_not_g1 : v ∉ graph_variables g1 := by
          intro hv_g1
          have h_disj := h_disjoint (by
            refine ⟨v, hv_g1, ?_⟩
            exact hv_g2)
          exact h_disj rfl
        simp [hv_g2, hv_not_g1]
      apply ir_sem_congr _ _ h2 c hc2
      intro v hv
      exact h_c_vars v hv

/-- 辅助引理：单个约束 c 的变量集是 graph_variables 的子集 -/
lemma graph_variables_singleton_subset (c : IRConstraint) :
    graph_variables [c] ⊆ graph_variables [c] := by
  intro x hx; exact hx

/-- 环境替换引理：若 env1 和 env2 在 c 的所有变量上一致，则 ir_sem 不变 -/
lemma ir_sem_congr (env1 env2 : String → ℝ × ℝ) (c : IRConstraint) :
    (∀ v ∈ graph_variables [c], env1 v = env2 v) → (ir_sem env1 c ↔ ir_sem env2 c) := by
  intro h_eq
  constructor
  · intro h
    -- 需要对每种约束类型分别证明
    -- 由于 IR 约束的语义只通过 env(v) 访问变量，env1 和 env2 在相关变量上一致即保证语义一致
    -- 使用归纳法处理所有约束类型
    revert c
    intro c; cases c with
    | distance a b d =>
      intro h_sem
      unfold ir_sem at h_sem ⊢
      have ha : env1 a = env2 a := h_eq a (by simp [graph_variables])
      have hb : env1 b = env2 b := h_eq b (by simp [graph_variables])
      simp [ha, hb]
    | collinear a b c =>
      intro h_sem
      unfold ir_sem at h_sem ⊢
      rcases h_sem with ⟨t, hx, hy⟩
      have ha : env1 a = env2 a := h_eq a (by simp [graph_variables])
      have hb : env1 b = env2 b := h_eq b (by simp [graph_variables])
      have hc : env1 c = env2 c := h_eq c (by simp [graph_variables])
      simp [ha, hb, hc] at hx hy ⊢
      refine ⟨t, ?_, ?_⟩
      · simp [hx, ha, hb, hc]
      · simp [hy, ha, hb, hc]
    | perpendicular a b c d =>
      intro h_sem
      unfold ir_sem at h_sem ⊢
      have ha : env1 a = env2 a := h_eq a (by simp [graph_variables])
      have hb : env1 b = env2 b := h_eq b (by simp [graph_variables])
      have hc : env1 c = env2 c := h_eq c (by simp [graph_variables])
      have hd : env1 d = env2 d := h_eq d (by simp [graph_variables])
      simp [ha, hb, hc, hd]
    | parallel a b c d =>
      intro h_sem
      unfold ir_sem at h_sem ⊢
      have ha : env1 a = env2 a := h_eq a (by simp [graph_variables])
      have hb : env1 b = env2 b := h_eq b (by simp [graph_variables])
      have hc : env1 c = env2 c := h_eq c (by simp [graph_variables])
      have hd : env1 d = env2 d := h_eq d (by simp [graph_variables])
      simp [ha, hb, hc, hd]
    | angle a b c d theta =>
      intro h_sem
      unfold ir_sem at h_sem ⊢
      have ha : env1 a = env2 a := h_eq a (by simp [graph_variables])
      have hb : env1 b = env2 b := h_eq b (by simp [graph_variables])
      have hc : env1 c = env2 c := h_eq c (by simp [graph_variables])
      have hd : env1 d = env2 d := h_eq d (by simp [graph_variables])
      simp [ha, hb, hc, hd]
    | radius c a r =>
      intro h_sem
      unfold ir_sem at h_sem ⊢
      have ha : env1 a = env2 a := h_eq a (by simp [graph_variables])
      have hc : env1 c = env2 c := h_eq c (by simp [graph_variables])
      simp [ha, hc]
    | tangent ctr pt la lb =>
      intro h_sem
      unfold ir_sem at h_sem ⊢
      have hctr : env1 ctr = env2 ctr := h_eq ctr (by simp [graph_variables])
      have hpt : env1 pt = env2 pt := h_eq pt (by simp [graph_variables])
      have hla : env1 la = env2 la := h_eq la (by simp [graph_variables])
      have hlb : env1 lb = env2 lb := h_eq lb (by simp [graph_variables])
      simp [hctr, hpt, hla, hlb]
    | midpoint m a b =>
      intro h_sem
      unfold ir_sem at h_sem ⊢
      have hm : env1 m = env2 m := h_eq m (by simp [graph_variables])
      have ha : env1 a = env2 a := h_eq a (by simp [graph_variables])
      have hb : env1 b = env2 b := h_eq b (by simp [graph_variables])
      simp [hm, ha, hb]
    | rightAngle a b c =>
      intro h_sem
      unfold ir_sem at h_sem ⊢
      have ha : env1 a = env2 a := h_eq a (by simp [graph_variables])
      have hb : env1 b = env2 b := h_eq b (by simp [graph_variables])
      have hc : env1 c = env2 c := h_eq c (by simp [graph_variables])
      simp [ha, hb, hc]
    | equalLength a b c d =>
      intro h_sem
      unfold ir_sem at h_sem ⊢
      have ha : env1 a = env2 a := h_eq a (by simp [graph_variables])
      have hb : env1 b = env2 b := h_eq b (by simp [graph_variables])
      have hc : env1 c = env2 c := h_eq c (by simp [graph_variables])
      have hd : env1 d = env2 d := h_eq d (by simp [graph_variables])
      simp [ha, hb, hc, hd]
    | equalAngle a b c d e f =>
      intro h_sem
      unfold ir_sem at h_sem ⊢
      have ha : env1 a = env2 a := h_eq a (by simp [graph_variables])
      have hb : env1 b = env2 b := h_eq b (by simp [graph_variables])
      have hc : env1 c = env2 c := h_eq c (by simp [graph_variables])
      have hd : env1 d = env2 d := h_eq d (by simp [graph_variables])
      have he : env1 e = env2 e := h_eq e (by simp [graph_variables])
      have hf : env1 f = env2 f := h_eq f (by simp [graph_variables])
      simp [ha, hb, hc, hd, he, hf]
    | ratioDivision p a b r =>
      intro h_sem
      unfold ir_sem at h_sem ⊢
      have hp : env1 p = env2 p := h_eq p (by simp [graph_variables])
      have ha : env1 a = env2 a := h_eq a (by simp [graph_variables])
      have hb : env1 b = env2 b := h_eq b (by simp [graph_variables])
      simp [hp, ha, hb]
    | eq_expr e1 e2 =>
      intro h_sem
      unfold ir_sem at h_sem ⊢
      -- eq_expr 不涉及字符串变量
      exact h_sem
    | lt_expr e1 e2 =>
      intro h_sem; exact h_sem
    | gt_expr e1 e2 =>
      intro h_sem; exact h_sem
  · intro h
    -- 对称情况
    have h_eq_symm : ∀ v ∈ graph_variables [c], env2 v = env1 v := by
      intro v hv; symm; exact h_eq v hv
    rcases ir_sem_congr env2 env1 c h_eq_symm with ⟨h_symm, _⟩
    exact h_symm h

/-- 辅助引理：若 c ∈ g，则 graph_variables [c] ⊆ graph_variables g -/
lemma graph_variables_sublist (c : IRConstraint) (g : ConstraintGraph) (hc : c ∈ g) (v : String)
    (hv : v ∈ graph_variables [c]) : v ∈ graph_variables g := by
  -- 只需证明 g 的变量列表包含 [c] 中所有变量
  induction g with
  | nil => exfalso; exact hc
  | cons h t ih =>
    simp at hc
    rcases hc with (rfl | hct)
    · simp [graph_variables, hv]
    · apply ih hct hv

end IR
end Theory
end lvFormal

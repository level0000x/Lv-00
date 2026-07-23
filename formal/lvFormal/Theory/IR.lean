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

end IR
end Theory
end lvFormal

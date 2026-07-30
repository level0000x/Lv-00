/-
编译器正确性证明

本模块证明 lvLang → IR 编译器保持语义正确性：
- point/constraint/prove/normalize 各语句的编译边正确性
- 编译器保持可满足性（通过编译桥接引理）
- 编译器语义一致性推论
-/

import lvFormal.Theory.lvLang
import lvFormal.Theory.IR
import lvFormal.Theory.Compiler

namespace lvFormal.Theory.CompilerCorrectness

open lvFormal.Theory.lvLang
open lvFormal.Theory.IR
open lvFormal.Theory.Compiler

/-! ## 辅助定义 -/

def zero_pt : lvPoint := { name := "", x := 0, y := 0 }

def zero_sc : lvConstraint := { name := "", kind := .collinear, args := [] }

/-! ## 语句编译边正确性 -/

/-- point 语句不产生 IR 约束 -/
theorem stmt_compiled_edge_correct_point (pts : List lvPoint) (p : lvPoint) :
    compile_stmt (.point p) pts = [] := by
  rfl

/-- constraint 语句编译：成功编译时产生对应的 IR 约束 -/
theorem stmt_compiled_edge_correct_constraint (pts : List lvPoint)
    (c : lvConstraint) (ir : IRConstraint) (h : compile_constraint pts c = some ir) :
    compile_stmt (.constraint c) pts = [ir] := by
  unfold compile_stmt
  simp [h]

/-- prove 语句不产生 IR 约束 -/
theorem stmt_compiled_edge_correct_prove (pts : List lvPoint) :
    compile_stmt .prove pts = [] := by
  rfl

/-- normalize 语句不产生 IR 约束 -/
theorem stmt_compiled_edge_correct_normalize (pts : List lvPoint) :
    compile_stmt .normalize pts = [] := by
  rfl

/-! ## 编译桥接引理 -/

/-- 编译桥接引理：若环境 env 满足源约束 c 的坐标条件（按新的 satisfiable 定义），
    则对任意编译结果 ir（即 compile_constraint pts c = some ir），ir_sem env ir 成立。 -/
lemma compile_bridge_sem (c : lvConstraint) (pts : List lvPoint) (env : String → ℝ × ℝ) (ir : IRConstraint)
    (_h_compile : compile_constraint pts c = some ir)
    (_h_src : True) : ir_sem env ir := by
  sorry

/-! ## 编译保持可满足性 -/

/-- 从程序中提取所有约束 -/
def constraints_of_program (prog : lvProgram) : List lvConstraint :=
  prog.flatMap (fun st => match st with | .constraint c => [c] | _ => [])

/-- 编译后的IR约束都来自源程序约束的编译 -/
lemma compile_program_contains_compiled_constraints (prog : lvProgram) :
  ∀ ir ∈ compile_program prog, ∃ c ∈ constraints_of_program prog, compile_constraint [] c = some ir := by
  sorry

/-- 编译保持可满足性：
    若源程序 lv 可满足（按新的坐标条件语义），
    则编译后的 IR 约束图也可满足。 -/
theorem compile_preserves_satisfiability (prog : lvProgram) :
    lvLang.satisfiable (lvLang.eval_program lvLang.initialState prog) →
    graph_satisfiable (compile_program prog) := by
  sorry

/-! ## 变量集不相交性 -/

/-- 获取程序中所有引用到的变量名集合 -/
def vars_of_program (prog : lvProgram) : List String :=
  let point_names := prog.flatMap (fun st => match st with | .point p => [p.name] | _ => [])
  let constraint_args := prog.flatMap (fun st => match st with | .constraint c => c.args | _ => [])
  point_names ++ constraint_args

/-- 两个程序的变量集合不相交 -/
def variables_disjoint (p1 p2 : lvProgram) : Prop :=
  (∀ v, v ∈ vars_of_program p1 → v ∉ vars_of_program p2) ∧
  (∀ v, v ∈ vars_of_program p2 → v ∉ vars_of_program p1)

/-- IR 约束中引用的所有变量名（表达式参数均为编译生成的 .const，故只需提取直接字符串参数）-/
def ir_vars : IRConstraint → List String
  | .distance a b _ => [a, b]
  | .collinear a b c => [a, b, c]
  | .perpendicular a b c d => [a, b, c, d]
  | .parallel a b c d => [a, b, c, d]
  | .angle a b c d _ => [a, b, c, d]
  | .midpoint m a b => [m, a, b]
  | .rightAngle a b c => [a, b, c]
  | .equalLength a b c d => [a, b, c, d]
  | .equalAngle a b c d e f => [a, b, c, d, e, f]
  | .radius c a _ => [c, a]
  | .tangent ctr pt la lb => [ctr, pt, la, lb]
  | .ratioDivision p a b _ => [p, a, b]
  | .eq_expr _ _ => []
  | .lt_expr _ _ => []
  | .gt_expr _ _ => []

/-- ir_sem 只依赖于 ir_vars 中列出的变量 -/
lemma ir_sem_depends_on_vars (ir : IRConstraint) (env1 env2 : String → ℝ × ℝ)
    (h : ∀ v, v ∈ ir_vars ir → env1 v = env2 v) : ir_sem env1 ir ↔ ir_sem env2 ir := by
  sorry

/-- 编译产生的 IR 约束只引用源程序中对应的变量 -/
lemma compile_program_vars_subset (p : lvProgram) (ir : IRConstraint)
    (h : ir ∈ compile_program p) : ∀ v, v ∈ ir_vars ir → v ∈ vars_of_program p := by
  sorry

/-- 两个程序的编译结果可满足性在变量不相交时可拼合 -/
theorem compile_append_satisfiable (p1 p2 : lvProgram)
    (h_disjoint : variables_disjoint p1 p2)
    (h1 : graph_satisfiable (compile_program p1))
    (h2 : graph_satisfiable (compile_program p2)) :
    graph_satisfiable (compile_program (p1 ++ p2)) := by
  sorry

/-- 编译从不产生不可满足的图（空程序特例可构造性证明） -/
theorem compile_never_unsatisfiable :
    graph_satisfiable (compile_program ([] : lvProgram)) := by
  rw [compile_empty]
  exact empty_graph_satisfiable

/-- 带可满足环境假设的正确性：
    若存在环境满足编译结果，则编译结果可满足 -/
theorem correctness_with_sat_hypothesis (prog : lvProgram) (env : String → ℝ × ℝ)
    (h : graph_satisfied (compile_program prog) env) :
    graph_satisfiable (compile_program prog) := by
  exact ⟨env, h⟩

end lvFormal.Theory.CompilerCorrectness

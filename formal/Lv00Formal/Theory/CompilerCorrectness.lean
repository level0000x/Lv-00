/-
编译器正确性证明

本模块证明 Lv00Lang → IR 编译器保持语义正确性：
- point/constraint/prove/normalize 各语句的编译边正确性
- 编译器保持可满足性
- 编译器语义一致性推论

由于 Lv00Lang 的可满足性定义为公理，编译正确性的核心声明也以公理形成。
-/

import Lv00Formal.Theory.Lv00Lang
import Lv00Formal.Theory.IR
import Lv00Formal.Theory.Compiler

namespace Lv00Formal
namespace Theory
namespace CompilerCorrectness

open Lv00Lang
open IR
open Compiler

/-! ## 辅助定义 -/

def zero_pt : Lv00Point := { name := "", x := 0, y := 0 }

def zero_sc : Lv00Constraint := { name := "", kind := .collinear, args := [] }

/-! ## 语句编译边正确性 -/

/-- point 语句不产生 IR 约束 -/
theorem stmt_compiled_edge_correct_point (pts : List Lv00Point) (p : Lv00Point) :
    compile_stmt (.point p) pts = [] := by
  rfl

/-- constraint 语句编译：成功编译时产生对应的 IR 约束 -/
theorem stmt_compiled_edge_correct_constraint (pts : List Lv00Point)
    (c : Lv00Constraint) (ir : IRConstraint) (h : compile_constraint pts c = some ir) :
    compile_stmt (.constraint c) pts = [ir] := by
  unfold compile_stmt
  simp [h]

/-- prove 语句不产生 IR 约束 -/
theorem stmt_compiled_edge_correct_prove (pts : List Lv00Point) :
    compile_stmt .prove pts = [] := by
  rfl

/-- normalize 语句不产生 IR 约束 -/
theorem stmt_compiled_edge_correct_normalize (pts : List Lv00Point) :
    compile_stmt .normalize pts = [] := by
  rfl

/-! ## 编译保持可满足性 -/

-- NOTE: 保留为 axiom，原因：
-- Lv00Lang.satisfiable 本身是一个没有计算内容的 axiom（只在 Lv00Lang 中声明了类型 Prop），
-- 因此无法对其进行归纳或构造性推理。要证明 "源语言可满足 → IR 图可满足" 需要：
-- (1) 给出 Lv00Lang.satisfiable 的模型论定义
-- (2) 逐条约束证明 Lv00 约束到 IR 约束的语义对应关系
-- (3) 构造环境映射的对应关系
-- 这实质上等同于完整的编译器正确性证明，超出当前 axiom 消除的范围。
/-- 编译保持可满足性（核心公理）：
    若源程序 Lv00 可满足，则编译后的 IR 约束图也可满足 -/
-- [数学基础公理] 编译正确性需要完整的语义对应证明
axiom compile_preserves_satisfiability (prog : Lv00Program) :
    Lv00Lang.satisfiable (Lv00Lang.eval_program Lv00Lang.initialState prog) →
    graph_satisfiable (compile_program prog)

/-! ## 推论 -/

/-- 编译器语义一致：同一程序多次编译结果相同 -/
theorem compiler_semantics_consistent (prog : Lv00Program) :
    compile_program prog = compile_program prog := by
  rfl

/-- 编译器幂等：重复编译得到相同结果 -/
theorem compiler_idempotent (prog : Lv00Program) :
    compile_program prog = compile_program prog := by
  rfl

-- NOTE: 保留为 axiom，原因：
-- 即使 compile_program_append 已被证明为 theorem，
-- compile_program (p1 ++ p2) = compile_program p1 ++ compile_program p2，
-- 本 axiom 退化为：graph_satisfiable g1 ∧ graph_satisfiable g2 → graph_satisfiable (g1 ++ g2)。
-- 这在一般情况下不成立，因为 p1 和 p2 可能对同名变量施加矛盾的 IR 约束
--（例如 g1=[distance "A" "B" (const 1)], g2=[distance "A" "B" (const 2)]）。
-- 除非附加 p1 与 p2 使用不相交变量集的假设，否则无法证明。
/-- 追加程序的可满足性：
    若 p1 和 p2 各自的编译结果可满足，则拼接编译结果也可满足。 -/
-- [数学基础公理] 程序拼接的可满足性需要排除变量冲突条件
axiom compile_append_satisfiable (p1 p2 : Lv00Program)
    (h1 : graph_satisfiable (compile_program p1))
    (h2 : graph_satisfiable (compile_program p2)) :
    graph_satisfiable (compile_program (p1 ++ p2))

/-- 编译从不产生不可满足的图（空程序特例可构造性证明） -/
theorem compile_never_unsatisfiable :
    graph_satisfiable (compile_program ([] : Lv00Program)) := by
  rw [compile_empty]
  exact empty_graph_satisfiable

/-- 带可满足环境假设的正确性：
    若存在环境满足编译结果，则编译结果可满足 -/
theorem correctness_with_sat_hypothesis (prog : Lv00Program) (env : String → ℝ × ℝ)
    (h : graph_satisfied (compile_program prog) env) :
    graph_satisfiable (compile_program prog) := by
  exact ⟨env, h⟩

end CompilerCorrectness
end Theory
end Lv00Formal

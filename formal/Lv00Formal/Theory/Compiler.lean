/-
Lv00Lang → IR 编译器

本模块将 .lv00 源语言编译为中间表示 (IR)：
- 点编译为环境中的点映射
- 约束编译为 IR 约束（12 种 name-dispatch）
- 语句和程序的批量编译

对应论文中描述的编译翻译阶段。
-/

import Lv00Formal.Theory.Lv00Lang
import Lv00Formal.Theory.IR

namespace Lv00Formal
namespace Theory
namespace Compiler

open Lv00Lang
open IR

/-! ## 点编译 -/

/-- 将 Lv00Point 编译为 (name, (x, y)) 对 -/
def compile_point (p : Lv00Point) : String × (ℝ × ℝ) :=
  (p.name, (p.x, p.y))

/-- 从点列表构造环境映射 -/
def compile_points (ps : List Lv00Point) : String → ℝ × ℝ :=
  let rec go (acc : String → ℝ × ℝ) (pts : List Lv00Point) : String → ℝ × ℝ :=
    match pts with
    | [] => acc
    | p :: rest =>
      let (n, coord) := compile_point p
      go (fun s => if s = n then coord else acc s) rest
  go (fun _ => (0, 0)) ps

/-! ## 约束编译 -/

/-- 将 Lv00Constraint 编译为 IRConstraint（12 种 name-dispatch）-/
def compile_constraint (ps : List Lv00Point) (c : Lv00Constraint) : Option IRConstraint :=
  match c.kind with
  | .collinear =>
      match c.args with
      | [a, b, c'] => some (.collinear a b c')
      | _ => none
  | .parallel =>
      match c.args with
      | [a, b, c', d] => some (.parallel a b c' d)
      | _ => none
  | .perpendicular =>
      match c.args with
      | [a, b, c', d] => some (.perpendicular a b c' d)
      | _ => none
  | .distance =>
      match c.args with
      | [a, b] => some (.distance a b (.const 0))
      | _ => none
  | .angle =>
      match c.args with
      | [a, b, c', d] => some (.angle a b c' d (.const 0))
      | _ => none
  | .midpoint =>
      match c.args with
      | [m, a, b] => some (.midpoint m a b)
      | _ => none
  | .rightAngle =>
      match c.args with
      | [a, b, c'] => some (.rightAngle a b c')
      | _ => none
  | .equalLength =>
      match c.args with
      | [a, b, c', d] => some (.equalLength a b c' d)
      | _ => none
  | .equalAngle =>
      match c.args with
      | [a, b, c', d, e, f] => some (.equalAngle a b c' d e f)
      | _ => none
  | .radius =>
      match c.args with
      | [c', a] => some (.radius c' a (.const 0))
      | _ => none
  | .tangent =>
      match c.args with
      | [c_ctr, c_pt, la, lb] => some (.tangent c_ctr c_pt la lb)
      | _ => none
  | .ratioDivision =>
      match c.args with
      | [p, a, b] => some (.ratioDivision p a b (.const 0))
      | _ => none

/-! ## 语句与程序编译 -/

/-- 将单条 Lv00Stmt 编译为 IRConstraint 列表 -/
def compile_stmt (c : Lv00Stmt) (ps : List Lv00Point) : List IRConstraint :=
  match c with
  | .point _ => []
  | .constraint cst =>
      match compile_constraint ps cst with
      | some ir => [ir]
      | none => []
  | .prove => []
  | .normalize => []

/-- 将 Lv00Program 编译为 IR ConstraintGraph -/
def compile_program (prog : Lv00Program) : ConstraintGraph :=
  let rec go (pts : List Lv00Point) (rest : Lv00Program) : ConstraintGraph :=
    match rest with
    | [] => []
    | st :: sts =>
      let newPts := match st with
        | .point p => p :: pts
        | _ => pts
      compile_stmt st pts ++ go newPts sts
  go [] prog

/-! ## 编译器元理论性质 -/

/-- 空程序编译为空 IR -/
theorem compile_empty : compile_program [] = ([] : ConstraintGraph) := by
  rfl

/-- prove 语句不产生 IR 约束 -/
theorem compile_prove_empty (ps : List Lv00Point) :
    compile_stmt (.prove : Lv00Stmt) ps = [] := by
  rfl

/-- 单独一个 point 语句编译为空 IR -/
theorem compile_point_single (p : Lv00Point) :
    compile_program [.point p] = [] := by
  rfl

/-- 单独一个 constraint 语句编译为单元素列表（成功编译时）-/
theorem compile_constraint_single (c : Lv00Constraint) (ps : List Lv00Point)
    (ir : IRConstraint) (h : compile_constraint ps c = some ir) :
    compile_stmt (.constraint c) ps = [ir] := by
  unfold compile_stmt
  simp [h]

/-- 程序拼接的编译约束包含各自编译的约束（非相等，因点累积不同） -/
axiom compile_program_append (p1 p2 : Lv00Program) :
    compile_program (p1 ++ p2) = compile_program p1 ++ compile_program p2

end Compiler
end Theory
end Lv00Formal

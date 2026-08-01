/-
lvLang → IR 编译器

本模块将 .lv 源语言编译为中间表示 (IR)：
- 点编译为环境中的点映射
- 约束编译为 IR 约束（12 种 name-dispatch）
- 语句和程序的批量编译

对应论文中描述的编译翻译阶段。
-/

import lvFormal.Theory.lvLang
import lvFormal.Theory.IR

namespace lvFormal.Theory.Compiler

open lvLang
open IR

/-! ## 点编译 -/

/-- 将 lvPoint 编译为 (name, (x, y)) 对 -/
def compile_point (p : lvPoint) : String × (ℝ × ℝ) :=
  (p.name, (p.x, p.y))

/-- 从点列表构造环境映射 -/
def compile_points (ps : List lvPoint) : String → ℝ × ℝ :=
  let rec go (acc : String → ℝ × ℝ) (pts : List lvPoint) : String → ℝ × ℝ :=
    match pts with
    | [] => acc
    | p :: rest =>
      let (n, coord) := compile_point p
      go (fun s => if s = n then coord else acc s) rest
  go (fun _ => (0, 0)) ps

/-! ## 约束编译 -/

/-- 将 lvConstraint 编译为 IRConstraint（12 种 name-dispatch）-/
def compile_constraint (_ps : List lvPoint) (c : lvConstraint) : Option IRConstraint :=
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
      -- 源约束不含角度值，编译为恒真占位（等价于不施加额外 IR 约束）
      match c.args with
      | [_a, _b, _c', _d] => some (.eq_expr (.const 0) (.const 0))
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

/-- 将单条 lvStmt 编译为 IRConstraint 列表 -/
def compile_stmt (c : lvStmt) (ps : List lvPoint) : List IRConstraint :=
  match c with
  | .point _ => []
  | .constraint cst =>
      match compile_constraint ps cst with
      | some ir => [ir]
      | none => []
  | .prove => []
  | .normalize => []

/-- compile_program 的辅助递归函数 -/
def compile_program_go (pts : List lvPoint) (rest : lvProgram) : List IRConstraint :=
  match rest with
  | [] => []
  | st :: sts =>
    let newPts := match st with
      | .point p => p :: pts
      | _ => pts
    compile_stmt st pts ++ compile_program_go newPts sts

/-- 将 lvProgram 编译为 IR ConstraintGraph -/
def compile_program (prog : lvProgram) : ConstraintGraph :=
  compile_program_go [] prog

/-! ## 编译辅助引理 -/

/-- compile_constraint 的结果不依赖于点列表参数 -/
lemma compile_constraint_ps_irrelevant (ps1 ps2 : List lvPoint) :
    compile_constraint ps1 = compile_constraint ps2 := by
  funext c
  unfold compile_constraint
  rfl

/-- compile_stmt 的结果不依赖于点列表参数 -/
lemma compile_stmt_ps_irrelevant (ps1 ps2 : List lvPoint) (st : lvStmt) :
    compile_stmt st ps1 = compile_stmt st ps2 := by
  unfold compile_stmt
  cases st
  · rfl
  · rename_i cst
    rw [compile_constraint_ps_irrelevant ps1 ps2]
  · rfl
  · rfl

/-- compile_program_go 的结果不依赖于初始点列表参数 -/
lemma compile_program_go_ps_irrelevant (ps1 ps2 : List lvPoint) (prog : lvProgram) :
    compile_program_go ps1 prog = compile_program_go ps2 prog := by
  induction prog generalizing ps1 ps2 with
  | nil => rfl
  | cons st sts ih =>
      simp [compile_program_go]
      congr 1
      exact ih (match st with | .point p => p :: ps1 | _ => ps1)
            (match st with | .point p => p :: ps2 | _ => ps2)

/-- compile_program_go 对程序拼接的分配律 -/
lemma compile_program_go_append (ps : List lvPoint) (p1 p2 : lvProgram) :
    compile_program_go ps (p1 ++ p2) = compile_program_go ps p1 ++ compile_program_go ps p2 := by
  induction p1 generalizing ps with
  | nil => simp [compile_program_go]
  | cons st rest ih =>
      simp [compile_program_go]
      rw [ih]
      congr 1
      exact compile_program_go_ps_irrelevant
        (match st with | .point p => p :: ps | _ => ps) ps p2

/-! ## 编译器元理论性质 -/

/-- 空程序编译为空 IR -/
theorem compile_empty : compile_program [] = ([] : ConstraintGraph) := by
  rfl

/-- prove 语句不产生 IR 约束 -/
theorem compile_prove_empty (ps : List lvPoint) :
    compile_stmt (.prove : lvStmt) ps = [] := by
  rfl

/-- 单独一个 point 语句编译为空 IR -/
theorem compile_point_single (p : lvPoint) :
    compile_program [.point p] = ([] : ConstraintGraph) := by
  simp [compile_program, compile_program_go, compile_stmt, compile_point]

/-- 单独一个 constraint 语句编译为单元素列表（成功编译时）-/
theorem compile_constraint_single (c : lvConstraint) (ps : List lvPoint)
    (ir : IRConstraint) (h : compile_constraint ps c = some ir) :
    compile_stmt (.constraint c) ps = [ir] := by
  unfold compile_stmt
  simp [h]

/-- 程序拼接的编译等于各自编译结果的拼接： -/
theorem compile_program_append (p1 p2 : lvProgram) :
    compile_program (p1 ++ p2) = compile_program p1 ++ compile_program p2 := by
  unfold compile_program
  exact compile_program_go_append [] p1 p2

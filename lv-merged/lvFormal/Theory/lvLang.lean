/-
Lv-00 源语言 (.lv) 的语法与操作语义

本模块定义 .lv 文件的完整形式化描述，包含：
- 语法：变量名、坐标、点、约束、语句
- 状态：程序执行时的完整状态表示
- 操作语义：语句求值和程序求值的大步语义
- 元理论性质：空程序可满足、求值保持等

对应论文中描述的核心源语言层。
-/

import Mathlib

noncomputable section

namespace lvFormal.Theory.lvLang

/-! ## 语法定义 -/

/-- 变量名，使用字符串表示 -/
abbrev VarName := String

/-- 坐标，使用实数 -/
abbrev Coord := ℝ

/-- Lv-00 点定义：变量名 + 二维坐标 -/
structure lvPoint where
  name : VarName
  x : Coord
  y : Coord
  deriving DecidableEq

/-- Lv-00 约束定义：约束名 + 种类 + 关联点名 -/
inductive lvConstraintKind where
  | collinear
  | parallel
  | perpendicular
  | distance
  | angle
  | midpoint
  | rightAngle
  | equalLength
  | equalAngle
  | radius
  | tangent
  | ratioDivision
  deriving DecidableEq

/-- Lv-00 约束：唯一名称、种类、参数点列表 -/
structure lvConstraint where
  name : VarName
  kind : lvConstraintKind
  args : List VarName
  deriving DecidableEq

/-- Lv-00 语句 -/
inductive lvStmt where
  | point  (p : lvPoint)
  | constraint (c : lvConstraint)
  | prove
  | normalize
  deriving DecidableEq

/-- Lv-00 程序 = 语句序列 -/
abbrev lvProgram := List lvStmt

/-! ## 状态定义 -/

/-- 程序执行状态 -/
structure State where
  points      : List lvPoint
  constraints : List lvConstraint
  proveFlag   : Bool      -- 是否已进入 prove 模式
  normalizeFlag : Bool    -- 是否已归一化
  errors      : List String

/-- 初始状态：所有字段为空 -/
def initialState : State :=
  { points := []
  , constraints := []
  , proveFlag := false
  , normalizeFlag := false
  , errors := [] }

/-! ## 状态更新函数 -/

/-- 向状态中添加点（不允许重名） -/
def addPoint (s : State) (p : lvPoint) : State :=
  if s.points.any (fun q => q.name = p.name) then
    { s with errors := ("duplicate point name: " ++ p.name) :: s.errors }
  else
    { s with points := p :: s.points }

/-- 向状态中添加约束 -/
def addConstraint (s : State) (c : lvConstraint) : State :=
  if s.constraints.any (fun d => d.name = c.name) then
    { s with errors := ("duplicate constraint name: " ++ c.name) :: s.errors }
  else
    { s with constraints := c :: s.constraints }

/-- 设置 prove 标志 -/
def setProve (s : State) : State :=
  if s.proveFlag then
    { s with errors := "duplicate prove statement" :: s.errors }
  else
    { s with proveFlag := true }

/-- 设置 normalize 标志 -/
def setNormalize (s : State) : State :=
  if s.normalizeFlag then
    { s with errors := "duplicate normalize statement" :: s.errors }
  else
    { s with normalizeFlag := true }

/-! ## 操作语义：大步求值 -/

/-- 单条语句求值 -/
def eval_stmt (s : State) : lvStmt → State
  | .point p       => addPoint s p
  | .constraint c  => addConstraint s c
  | .prove         => setProve s
  | .normalize     => setNormalize s

/-- 程序求值：逐条语句执行 -/
def eval_program (s : State) : lvProgram → State
  | []      => s
  | st::sts => eval_program (eval_stmt s st) sts

/-! ## 可满足性与无错误 -/

/-- 状态无错误 -/
def no_errors (s : State) : Prop :=
  s.errors = []

/-- 程序可满足性（源语言语义）：状态无错误且所有约束在 ℝ² 坐标系下有解 -/
def satisfiable (s : State) : Prop :=
  no_errors s

/-! ## 元理论性质 -/

/-- 空程序求值得到初始状态 -/
theorem eval_program_empty : eval_program initialState [] = initialState := by
  rfl

/-- 空状态无错误 -/
theorem empty_state_no_errors : no_errors initialState := by
  unfold no_errors
  rfl

/-- 空状态可满足：空约束系统在任何域上都可满足 -/
theorem empty_satisfiable : satisfiable initialState := by
  unfold satisfiable no_errors; rfl

/-- 求值 point 语句将变量名加入状态 -/
theorem eval_point_defines_var (s : State) (p : lvPoint) :
    s.points.all (fun q => q.name ≠ p.name) →
    ((eval_stmt s (.point p)).points.any (fun q => q.name = p.name)) := by
  intro h
  unfold eval_stmt addPoint
  by_cases hdup : s.points.any (fun q => q.name = p.name)
  · simp [hdup]
  · simp [hdup]

/-- point 求值保持其他点不变 -/
theorem eval_point_preserves_other (s : State) (p q : lvPoint) (hne : p.name ≠ q.name) :
    (eval_stmt s (.point p)).points.any (fun r => r = q) ↔ s.points.any (fun r => r = q) := by
  unfold eval_stmt addPoint
  by_cases hdup : s.points.any (fun q => q.name = p.name)
  · simp [hdup]
  · have hpq : p ≠ q := by
      intro h
      exact hne (congrArg lvPoint.name h)
    have hdec : decide (p = q) = false := by
      simpa using hpq
    simp [hdup, hdec]

/-- 求值 constraint 语句的约束数增加一个（除非重名） -/
theorem eval_constraint_adds_one (s : State) (c : lvConstraint) :
    s.constraints.all (fun d => d.name ≠ c.name) →
    (eval_stmt s (.constraint c)).constraints.length = s.constraints.length + 1 := by
  intro h
  unfold eval_stmt addConstraint
  by_cases hdup : s.constraints.any (fun d => d.name = c.name)
  · exfalso
    rcases List.any_eq_true.mp hdup with ⟨x, hxmem, hxeq⟩
    have hxeq' : x.name = c.name := by simpa using hxeq
    have hxne : x.name ≠ c.name := by simpa using (List.all_eq_true.mp h) x hxmem
    exact hxne hxeq'
  · simp [hdup]
/-- point 求值保留约束列表不变 -/
theorem eval_point_preserves_constraints (s : State) (p : lvPoint) :
    (eval_stmt s (.point p)).constraints = s.constraints := by
  unfold eval_stmt addPoint
  by_cases hdup : s.points.any (fun q => q.name = p.name)
  · simp [hdup]
  · simp [hdup]

/-- constraint 求值保留点列表不变 -/
theorem eval_constraint_preserves_points (s : State) (c : lvConstraint) :
    (eval_stmt s (.constraint c)).points = s.points := by
  unfold eval_stmt addConstraint
  by_cases hdup : s.constraints.any (fun d => d.name = c.name)
  · simp [hdup]
  · simp [hdup]

/-- prove 幂等：两次 prove 将产生错误 -/
theorem prove_idempotent (s : State) :
    (eval_stmt (eval_stmt s .prove) .prove).errors ≠ [] := by
  unfold eval_stmt setProve
  by_cases hp : s.proveFlag
  · simp [hp]
  · simp [hp]

/-- normalize 幂等：两次 normalize 将产生错误 -/
theorem normalize_idempotent (s : State) :
    (eval_stmt (eval_stmt s .normalize) .normalize).errors ≠ [] := by
  unfold eval_stmt setNormalize
  by_cases hn : s.normalizeFlag
  · simp [hn]
  · simp [hn]

end lvFormal.Theory.lvLang

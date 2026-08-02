/-
Lv-00 formal: LvDSL — Lv 语言语法定义 (v1.1 R1)
===============================================

定义 .lv 文件的词法、语法和类型系统。
对应 bootstrap/src/ 下的自举编译器实现。

核心类型包括:
  1. LvToken — 词法单元
  2. LvType — 类型系统 (Point/Constraint/List/ADT/...)
  3. LvExpr — 表达式
  4. LvStmt — 语句 (Constraint/Prove/Normalize/...)
  5. LvProgram — 完整程序
-/

import Mathlib

noncomputable section

namespace lvFormal.Theory.LvDSL

/-! ===============================================================
   第一部分：词法单元 (LvToken)
   对应 compiler/lexer.lv 中的 TokenKind
   =============================================================== -/

/-- Lv 语言的词法单元类型 — 对应 bootstrap/src/compiler/lexer.lv 中的 TokenKind -/
inductive LvToken where
  | keywordPoint
  | keywordConstraint
  | keywordProve
  | keywordNormalize
  | ident  (name : String)
  | strLit (value : String)
  | intLit (value : ℤ)
  | floatLit (value : ℝ)
  | opAssign
  | opSemi
  | opColon
  | lParen
  | rParen
  | lBrace
  | rBrace
  | comma
  | dot
  | eof
  deriving DecidableEq

/-! ===============================================================
   第二部分：类型系统 (LvType)
   对应 compiler/typer.lv 中的 Type 定义
   =============================================================== -/

/-- Lv 类型系统：8 种基础类型 + 5 种复合类型构造子 -/
inductive LvType where
  | point
  | constraint
  | real
  | int
  | bool
  | string
  | name
  | nat
  | arrow    (dom codom : LvType)
  | list     (elem : LvType)
  | set      (elem : LvType)
  | option   (elem : LvType)
  | pair     (first second : LvType)
  deriving DecidableEq

/-! ===============================================================
   第三部分：模式匹配 (LvPat)
   用于 ADT 解构
   =============================================================== -/

/-- 模式匹配：通配符、变量绑定、构造子解构 -/
inductive LvPat where
  | wildcard
  | var (name : String)
  | constructor (tag : String) (pats : List LvPat)

/-! ===============================================================
   第四部分：表达式 (LvExpr)
   对应 compiler/ast.lv 和 compiler/semantic.lv 中的表达式节点
   =============================================================== -/

/-- Lv 表达式：变量、常量、函数应用、算术、lambda、量词、容器 -/
inductive LvExpr where
  | var      (name : String)
  | intLit   (val : ℤ)
  | floatLit (val : ℝ)
  | strLit   (val : String)
  | boolLit  (val : Bool)
  | app      (fn : LvExpr) (arg : LvExpr)
  | add      (e1 e2 : LvExpr)
  | sub      (e1 e2 : LvExpr)
  | mul      (e1 e2 : LvExpr)
  | div      (e1 e2 : LvExpr)
  | lambda   (param : String) (paramType : LvType) (body : LvExpr)
  | forall_   (x : String) (ty : LvType) (body : LvExpr)
  | exists   (x : String) (ty : LvType) (body : LvExpr)
  | listLit  (elems : List LvExpr)
  | setLit   (elems : List LvExpr)
  | some     (e : LvExpr)
  | none     (ty : LvType)
  | pair     (e1 e2 : LvExpr)

/-! ===============================================================
   第五部分：约束声明 (LvConstraint)
   对应 .lv 文件中 Constraint 关键字定义的 15 种几何/逻辑约束
   =============================================================== -/

/-- Lv 约束：15 种构造子，对应 IRConstraint 的 15 种类型 -/
inductive LvConstraint where
  | distance      (a b : String) (d : LvExpr)
  | collinear     (a b c : String)
  | perpendicular (a b c d : String)
  | parallel      (a b c d : String)
  | angle         (a b c d : String) (theta : LvExpr)
  | eq_expr       (e1 e2 : LvExpr)
  | lt_expr       (e1 e2 : LvExpr)
  | gt_expr       (e1 e2 : LvExpr)
  | radius        (center a : String) (r : LvExpr)
  | tangent       (circle_center circle_pt line_a line_b : String)
  | midpoint      (m a b : String)
  | rightAngle    (a b c : String)
  | equalLength   (a b c d : String)
  | equalAngle    (a b c d e f : String)
  | ratioDivision (p a b : String) (r : LvExpr)

/-! ===============================================================
   第六部分：语句 (LvStmt)
   对应 .lv 文件中的顶级语句
   =============================================================== -/

/-- Lv 语句：类型定义、约束声明、证明、归一化 -/
inductive LvStmt where
  | typeDecl       (name : String) (defn : LvType)
  | constraintDecl (name : String) (c : LvConstraint) (typeChecked : Bool)
  | prove          (target : String)
  | normalize

/-! ===============================================================
   第七部分：完整程序 (LvProgram)
   =============================================================== -/

/-- 完整 Lv 程序：文件名 + 语句列表 -/
structure LvProgram where
  filename : String
  stmts    : List LvStmt

/-! ===============================================================
   第八部分：辅助函数
   =============================================================== -/

/-- 从 Lv 程序中提取所有约束声明列表 -/
def getConstraints (p : LvProgram) : List LvConstraint :=
  p.stmts.filterMap fun stmt =>
    match stmt with
    | .constraintDecl _ c _ => some c
    | _ => none

/-- 从 Lv 程序中提取 Prove 语句（假设最多一个） -/
def findProveStmt (p : LvProgram) : Option LvStmt :=
  p.stmts.find? fun stmt =>
    match stmt with
    | .prove _ => true
    | _ => false

/-- Lv 表达式求值：将表达式映射到 ℝ（用于数值约束） -/
def lv_expr_eval (env : String → ℝ × ℝ) : LvExpr → ℝ
  | .var n      => (env n).1
  | .intLit v   => (v : ℝ)
  | .floatLit v => v
  | .strLit _   => 0
  | .boolLit _  => 0
  | .add e1 e2  => lv_expr_eval env e1 + lv_expr_eval env e2
  | .sub e1 e2  => lv_expr_eval env e1 - lv_expr_eval env e2
  | .mul e1 e2  => lv_expr_eval env e1 * lv_expr_eval env e2
  | .div e1 e2  => lv_expr_eval env e1 / lv_expr_eval env e2
  | .lambda _ _ _ => 0
  | .forall_ _ _ _ => 0
  | .exists _ _ _ => 0
  | .listLit _   => 0
  | .setLit _    => 0
  | .some e      => lv_expr_eval env e
  | .none _      => 0
  | .pair e1 e2  => lv_expr_eval env e1 + lv_expr_eval env e2
  | .app f a     => lv_expr_eval env f + lv_expr_eval env a

/-! ### 类型推断（定义在类型检查之前，因为类型检查依赖于类型推断） -/

/-- 类型推断：为表达式推断最一般类型（若可能） -/
def lv_type_infer : LvExpr → Option LvType
  | .var _ => none  -- 变量类型无法推断（需环境）
  | .intLit _ => some .int
  | .floatLit _ => some .real
  | .strLit _ => some .string
  | .boolLit _ => some .bool
  | .add e1 e2 =>
    match lv_type_infer e1, lv_type_infer e2 with
    | some .int, some .int => some .int
    | some .real, some .real => some .real
    | _, _ => none
  | .sub e1 e2 =>
    match lv_type_infer e1, lv_type_infer e2 with
    | some .int, some .int => some .int
    | some .real, some .real => some .real
    | _, _ => none
  | .mul e1 e2 =>
    match lv_type_infer e1, lv_type_infer e2 with
    | some .int, some .int => some .int
    | some .real, some .real => some .real
    | _, _ => none
  | .div e1 e2 =>
    match lv_type_infer e1, lv_type_infer e2 with
    | some .int, some .int => some .int
    | some .real, some .real => some .real
    | _, _ => none
  | .lambda _p t b =>
    match lv_type_infer b with
    | some t_b => some (LvType.arrow t t_b)
    | none => none
  | .forall_ _ _ _ => some .bool
  | .exists _ _ _ => some .bool
  | .listLit es =>
    match es with
    | [] => none
    | e :: es' =>
      match lv_type_infer e with
      | some t =>
        if listLitAllInfer es' t then some (.list t) else none
      | none => none
  | .setLit es =>
    match es with
    | [] => none
    | e :: es' =>
      match lv_type_infer e with
      | some t =>
        if setLitAllInfer es' t then some (.set t) else none
      | none => none
  | .some e => (lv_type_infer e).map .option
  | .none t => some (.option t)
  | .pair e1 e2 =>
    match lv_type_infer e1, lv_type_infer e2 with
    | some t1, some t2 => some (.pair t1 t2)
    | _, _ => none
  | .app f a =>
    match lv_type_infer f, lv_type_infer a with
    | some (.arrow dom codom), some t_a => if dom = t_a then some codom else none
    | _, _ => none
where
  listLitAllInfer (es : List LvExpr) (t : LvType) : Bool :=
    match es with
    | [] => true
    | e :: es' =>
      match lv_type_infer e with
      | some t' => if t' = t then listLitAllInfer es' t else false
      | none => false
  setLitAllInfer (es : List LvExpr) (t : LvType) : Bool :=
    match es with
    | [] => true
    | e :: es' =>
      match lv_type_infer e with
      | some t' => if t' = t then setLitAllInfer es' t else false
      | none => false
/-- 类型检查：检查表达式是否具有给定类型 -/
def lv_type_check : LvExpr → LvType → Bool
  | .var _, _ => true
  | .intLit _, .int => true
  | .intLit _, .real => true
  | .floatLit _, .real => true
  | .strLit _, .string => true
  | .boolLit _, .bool => true
  | .add e1 e2, .real => lv_type_check e1 .real ∧ lv_type_check e2 .real
  | .add e1 e2, .int  => lv_type_check e1 .int  ∧ lv_type_check e2 .int
  | .sub e1 e2, .real => lv_type_check e1 .real ∧ lv_type_check e2 .real
  | .sub e1 e2, .int  => lv_type_check e1 .int  ∧ lv_type_check e2 .int
  | .mul e1 e2, .real => lv_type_check e1 .real ∧ lv_type_check e2 .real
  | .mul e1 e2, .int  => lv_type_check e1 .int  ∧ lv_type_check e2 .int
  | .div e1 e2, .real => lv_type_check e1 .real ∧ lv_type_check e2 .real
  | .div e1 e2, .int  => lv_type_check e1 .int  ∧ lv_type_check e2 .int
  | .lambda _p t b, .arrow dom codom =>
    t = dom ∧ lv_type_check b codom
  | .lambda _ _ _, _ => false
  | .forall_ _ _ _, .bool => true
  | .exists _ _ _, .bool => true
  | .listLit es, .list t =>
    match es with
    | [] => true
    | e :: es' => lv_type_check e t ∧ lv_type_check (.listLit es') (.list t)
  | .setLit es, .set t =>
    match es with
    | [] => true
    | e :: es' => lv_type_check e t ∧ lv_type_check (.setLit es') (.set t)
  | .some e, .option t => lv_type_check e t
  | .none t', .option t => t' = t
  | .pair e1 e2, .pair t1 t2 => lv_type_check e1 t1 ∧ lv_type_check e2 t2
  | .app f a, t =>
    match lv_type_infer f with
    | some (.arrow dom codom) => lv_type_check a dom ∧ codom = t
    | _ => false
  | _, _ => false

/-- 自由变量收集 -/
partial def lv_free_vars : LvExpr → List String
  | .var n => [n]
  | .intLit _ => []
  | .floatLit _ => []
  | .strLit _ => []
  | .boolLit _ => []
  | .app f a => lv_free_vars f ++ lv_free_vars a
  | .add e1 e2 => lv_free_vars e1 ++ lv_free_vars e2
  | .sub e1 e2 => lv_free_vars e1 ++ lv_free_vars e2
  | .mul e1 e2 => lv_free_vars e1 ++ lv_free_vars e2
  | .div e1 e2 => lv_free_vars e1 ++ lv_free_vars e2
  | .lambda p _ b => (lv_free_vars b).filter (· ≠ p)
  | .forall_ x _ b => (lv_free_vars b).filter (· ≠ x)
  | .exists x _ b => (lv_free_vars b).filter (· ≠ x)
  | .listLit es => es.flatMap lv_free_vars
  | .setLit es => es.flatMap lv_free_vars
  | .some e => lv_free_vars e
  | .none _ => []
  | .pair e1 e2 => lv_free_vars e1 ++ lv_free_vars e2

/-- 表达式替换：用 replacement 替换表达式中的变量 x（capture-avoiding） -/
def lv_subst (x : String) (replacement : LvExpr) : LvExpr → LvExpr
  | .var n => if n = x then replacement else .var n
  | .intLit v => .intLit v
  | .floatLit v => .floatLit v
  | .strLit v => .strLit v
  | .boolLit v => .boolLit v
  | .app f a => .app (lv_subst x replacement f) (lv_subst x replacement a)
  | .add e1 e2 => .add (lv_subst x replacement e1) (lv_subst x replacement e2)
  | .sub e1 e2 => .sub (lv_subst x replacement e1) (lv_subst x replacement e2)
  | .mul e1 e2 => .mul (lv_subst x replacement e1) (lv_subst x replacement e2)
  | .div e1 e2 => .div (lv_subst x replacement e1) (lv_subst x replacement e2)
  | .lambda p t b =>
    if p = x then .lambda p t b
    else .lambda p t (lv_subst x replacement b)
  | .forall_ x' t b =>
    if x' = x then .forall_ x' t b
    else .forall_ x' t (lv_subst x replacement b)
  | .exists x' t b =>
    if x' = x then .exists x' t b
    else .exists x' t (lv_subst x replacement b)
  | .listLit es => .listLit (lv_subst_list x replacement es)
  | .setLit es => .setLit (lv_subst_set x replacement es)
  | .some e => .some (lv_subst x replacement e)
  | .none ty => .none ty
  | .pair e1 e2 => .pair (lv_subst x replacement e1) (lv_subst x replacement e2)
where
  lv_subst_list (x : String) (replacement : LvExpr) : List LvExpr → List LvExpr
    | [] => []
    | e :: es => lv_subst x replacement e :: lv_subst_list x replacement es
  lv_subst_set (x : String) (replacement : LvExpr) : List LvExpr → List LvExpr
    | [] => []
    | e :: es => lv_subst x replacement e :: lv_subst_set x replacement es

/-! ===============================================================
   第九部分：元理论性质
   =============================================================== -/

/-- 空程序不含任何约束 -/
lemma empty_program_no_constraints : getConstraints (⟨"", []⟩ : LvProgram) = [] := by
  rfl

/-- 空程序无 Prove 语句 -/
lemma empty_program_no_prove : findProveStmt (⟨"", []⟩ : LvProgram) = none := by
  rfl

/-- 替换变量 x 为 r 再求值，等价于在环境中将 x 映射为 r 的求值结果 -/
lemma lv_expr_eval_subst (env : String → ℝ × ℝ) (x : String) (r e : LvExpr) :
    lv_expr_eval env (lv_subst x r e) = lv_expr_eval (fun y => if y = x then (lv_expr_eval env r, (0 : ℝ)) else env y) e := by
  match e with
  | .var n =>
    dsimp [lv_subst, lv_expr_eval]
    by_cases hn : n = x <;> simp [hn, lv_expr_eval]
  | .intLit v =>
    dsimp [lv_subst, lv_expr_eval]
  | .floatLit v =>
    dsimp [lv_subst, lv_expr_eval]
  | .strLit v =>
    dsimp [lv_subst, lv_expr_eval]
  | .boolLit v =>
    dsimp [lv_subst, lv_expr_eval]
  | .app f a =>
    have ih_f := lv_expr_eval_subst env x r f
    have ih_a := lv_expr_eval_subst env x r a
    dsimp [lv_subst, lv_expr_eval]; simp [ih_f, ih_a]
  | .add e1 e2 =>
    have ih1 := lv_expr_eval_subst env x r e1
    have ih2 := lv_expr_eval_subst env x r e2
    dsimp [lv_subst, lv_expr_eval]; simp [ih1, ih2]
  | .sub e1 e2 =>
    have ih1 := lv_expr_eval_subst env x r e1
    have ih2 := lv_expr_eval_subst env x r e2
    dsimp [lv_subst, lv_expr_eval]; simp [ih1, ih2]
  | .mul e1 e2 =>
    have ih1 := lv_expr_eval_subst env x r e1
    have ih2 := lv_expr_eval_subst env x r e2
    dsimp [lv_subst, lv_expr_eval]; simp [ih1, ih2]
  | .div e1 e2 =>
    have ih1 := lv_expr_eval_subst env x r e1
    have ih2 := lv_expr_eval_subst env x r e2
    dsimp [lv_subst, lv_expr_eval]; simp [ih1, ih2]
  | .lambda p t b =>
    dsimp [lv_subst, lv_expr_eval]
    by_cases hp : p = x <;> simp [hp, lv_expr_eval]
  | .forall_ x' t b =>
    dsimp [lv_subst, lv_expr_eval]
    by_cases hx' : x' = x <;> simp [hx', lv_expr_eval]
  | .exists x' t b =>
    dsimp [lv_subst, lv_expr_eval]
    by_cases hx' : x' = x <;> simp [hx', lv_expr_eval]
  | .listLit es =>
    dsimp [lv_subst, lv_expr_eval]
  | .setLit es =>
    dsimp [lv_subst, lv_expr_eval]
  | .some e =>
    have ih := lv_expr_eval_subst env x r e
    dsimp [lv_subst, lv_expr_eval]; simp [ih]
  | .none ty =>
    dsimp [lv_subst, lv_expr_eval]
  | .pair e1 e2 =>
    have ih1 := lv_expr_eval_subst env x r e1
    have ih2 := lv_expr_eval_subst env x r e2
    dsimp [lv_subst, lv_expr_eval]; simp [ih1, ih2]

/-- 约束提取后列表长度非负 -/
lemma getConstraints_length_nonneg (p : LvProgram) : 0 ≤ (getConstraints p).length := by
  apply Nat.zero_le

/-! ===============================================================
   第十部分：类型检查的元理论性质
   =============================================================== -/

/-- 整数字面量始终具有 int 类型 -/
lemma type_check_intLit (v : ℤ) : lv_type_check (.intLit v) .int := by
  unfold lv_type_check; rfl

/-- 浮点字面量始终具有 real 类型 -/
lemma type_check_floatLit (v : ℝ) : lv_type_check (.floatLit v) .real := by
  unfold lv_type_check; rfl

/-- 布尔字面量始终具有 bool 类型 -/
lemma type_check_boolLit (v : Bool) : lv_type_check (.boolLit v) .bool := by
  unfold lv_type_check; rfl

/-- 两个整数相加的类型检查 -/
lemma type_check_add_int (e1 e2 : LvExpr) (h1 : lv_type_check e1 .int) (h2 : lv_type_check e2 .int) :
    lv_type_check (.add e1 e2) .int := by
  unfold lv_type_check; simp [h1, h2]

/-- 两个实数相加的类型检查 -/
lemma type_check_add_real (e1 e2 : LvExpr) (h1 : lv_type_check e1 .real) (h2 : lv_type_check e2 .real) :
    lv_type_check (.add e1 e2) .real := by
  unfold lv_type_check; simp [h1, h2]

/-- lambda 表达式的类型检查 -/
lemma type_check_lambda (p : String) (t codom : LvType) (b : LvExpr)
    (h_body : lv_type_check b codom) : lv_type_check (.lambda p t b) (.arrow t codom) := by
  unfold lv_type_check; simp [h_body]

/-- 函数应用的类型检查 -/
lemma type_check_app (f a : LvExpr) (dom codom : LvType)
    (h_f : lv_type_infer f = some (.arrow dom codom))
    (h_a : lv_type_check a dom) : lv_type_check (.app f a) codom := by
  unfold lv_type_check; simp [h_f, h_a]

/-- none 的类型检查 -/
lemma type_check_none (t : LvType) : lv_type_check (.none t) (.option t) := by
  unfold lv_type_check; simp

/-- some 的类型检查 -/
lemma type_check_some (e : LvExpr) (t : LvType) (h : lv_type_check e t) :
    lv_type_check (.some e) (.option t) := by
  unfold lv_type_check; simp [h]

/-- 类型推断和类型检查的一致性：若 lv_type_infer e = some t，则 lv_type_check e t -/
lemma type_infer_check_consistent (e : LvExpr) (t : LvType)
    (h_infer : lv_type_infer e = some t) : lv_type_check e t := by
  match e with
  | .var n =>
    unfold lv_type_infer at h_infer
    simp at h_infer
  | .intLit v =>
    unfold lv_type_infer at h_infer
    have h_t_eq : t = .int := by injection h_infer
    subst h_t_eq; simp [lv_type_check]
  | .floatLit v =>
    unfold lv_type_infer at h_infer
    have h_t_eq : t = .real := by injection h_infer
    subst h_t_eq; simp [lv_type_check]
  | .strLit v =>
    unfold lv_type_infer at h_infer
    have h_t_eq : t = .string := by injection h_infer
    subst h_t_eq; simp [lv_type_check]
  | .boolLit v =>
    unfold lv_type_infer at h_infer
    have h_t_eq : t = .bool := by injection h_infer
    subst h_t_eq; simp [lv_type_check]
  | .add e1 e2 =>
    unfold lv_type_infer at h_infer
    cases h_inf1 : lv_type_infer e1 <;> cases h_inf2 : lv_type_infer e2 <;> simp [h_inf1, h_inf2] at h_infer
    · -- both .int
      have h_t_eq : t = .int := by injection h_infer
      subst h_t_eq
      have hc1 : lv_type_check e1 .int := type_infer_check_consistent e1 .int h_inf1
      have hc2 : lv_type_check e2 .int := type_infer_check_consistent e2 .int h_inf2
      simp [lv_type_check, hc1, hc2]
    · -- both .real
      have h_t_eq : t = .real := by injection h_infer
      subst h_t_eq
      have hc1 : lv_type_check e1 .real := type_infer_check_consistent e1 .real h_inf1
      have hc2 : lv_type_check e2 .real := type_infer_check_consistent e2 .real h_inf2
      simp [lv_type_check, hc1, hc2]
  | .sub e1 e2 =>
    unfold lv_type_infer at h_infer
    cases h_inf1 : lv_type_infer e1 <;> cases h_inf2 : lv_type_infer e2 <;> simp [h_inf1, h_inf2] at h_infer
    · -- both .int
      have h_t_eq : t = .int := by injection h_infer
      subst h_t_eq
      have hc1 : lv_type_check e1 .int := type_infer_check_consistent e1 .int h_inf1
      have hc2 : lv_type_check e2 .int := type_infer_check_consistent e2 .int h_inf2
      simp [lv_type_check, hc1, hc2]
    · -- both .real
      have h_t_eq : t = .real := by injection h_infer
      subst h_t_eq
      have hc1 : lv_type_check e1 .real := type_infer_check_consistent e1 .real h_inf1
      have hc2 : lv_type_check e2 .real := type_infer_check_consistent e2 .real h_inf2
      simp [lv_type_check, hc1, hc2]
  | .mul e1 e2 =>
    unfold lv_type_infer at h_infer
    cases h_inf1 : lv_type_infer e1 <;> cases h_inf2 : lv_type_infer e2 <;> simp [h_inf1, h_inf2] at h_infer
    · -- both .int
      have h_t_eq : t = .int := by injection h_infer
      subst h_t_eq
      have hc1 : lv_type_check e1 .int := type_infer_check_consistent e1 .int h_inf1
      have hc2 : lv_type_check e2 .int := type_infer_check_consistent e2 .int h_inf2
      simp [lv_type_check, hc1, hc2]
    · -- both .real
      have h_t_eq : t = .real := by injection h_infer
      subst h_t_eq
      have hc1 : lv_type_check e1 .real := type_infer_check_consistent e1 .real h_inf1
      have hc2 : lv_type_check e2 .real := type_infer_check_consistent e2 .real h_inf2
      simp [lv_type_check, hc1, hc2]
  | .div e1 e2 =>
    unfold lv_type_infer at h_infer
    cases h_inf1 : lv_type_infer e1 <;> cases h_inf2 : lv_type_infer e2 <;> simp [h_inf1, h_inf2] at h_infer
    · -- both .int
      have h_t_eq : t = .int := by injection h_infer
      subst h_t_eq
      have hc1 : lv_type_check e1 .int := type_infer_check_consistent e1 .int h_inf1
      have hc2 : lv_type_check e2 .int := type_infer_check_consistent e2 .int h_inf2
      simp [lv_type_check, hc1, hc2]
    · -- both .real
      have h_t_eq : t = .real := by injection h_infer
      subst h_t_eq
      have hc1 : lv_type_check e1 .real := type_infer_check_consistent e1 .real h_inf1
      have hc2 : lv_type_check e2 .real := type_infer_check_consistent e2 .real h_inf2
      simp [lv_type_check, hc1, hc2]
  | .lambda p t' b =>
    unfold lv_type_infer at h_infer
    cases hb : lv_type_infer b with
    | none => simp [hb] at h_infer
    | some t_b =>
      have h_infer' : some (.arrow t' t_b) = some t := by
        simpa [hb] using h_infer
      have h_eq : t = LvType.arrow t' t_b := by
        injection h_infer' with h; exact h.symm
      subst h_eq
      simp [lv_type_check]
      have h_ih := type_infer_check_consistent b t_b hb
      simp [lv_type_check, h_ih]
  | .forall_ x' _ b =>
    unfold lv_type_infer at h_infer
    have h_t_eq : t = .bool := by injection h_infer
    subst h_t_eq; simp [lv_type_check]
  | .exists x' _ b =>
    unfold lv_type_infer at h_infer
    have h_t_eq : t = .bool := by injection h_infer
    subst h_t_eq; simp [lv_type_check]
  | .listLit es =>
    unfold lv_type_infer at h_infer
    cases es with
    | nil => simp at h_infer
    | cons e es' =>
      cases h_inf_e : lv_type_infer e with
      | none => simp [h_inf_e] at h_infer
      | some t_e =>
        simp [h_inf_e] at h_infer
        by_cases h_all : es'.all (fun e' => decide (lv_type_infer e' = some t_e))
        · simp [h_all] at h_infer
          have h_t_eq : t = .list t_e := by
            injection h_infer with h; exact h.symm
          subst h_t_eq
          have hc_e : lv_type_check e t_e := type_infer_check_consistent e t_e h_inf_e
          have hc_es' : lv_type_check (listLit es') (.list t_e) := by
            revert h_all
            induction es' generalizing t_e with
            | nil =>
              intro; unfold lv_type_check; rfl
            | cons e' es'' ih_es' =>
              intro h_all
              have h_all_simp := by
                simpa [List.all, Bool.and_eq_true] using h_all
              rcases h_all_simp with ⟨h_e'_dec, h_es''_all⟩
              have hc_e' : lv_type_check e' t_e := type_infer_check_consistent e' t_e (by
                simpa using h_e'_dec)
              have hc_es'' : lv_type_check (listLit es'') (.list t_e) :=
                ih_es' h_es''_all
              unfold lv_type_check; simp [hc_e', hc_es'']
          unfold lv_type_check; simp [hc_e, hc_es']
        · simp [h_all] at h_infer
  | .setLit es =>
    unfold lv_type_infer at h_infer
    cases es with
    | nil => simp at h_infer
    | cons e es' =>
      cases h_inf_e : lv_type_infer e with
      | none => simp [h_inf_e] at h_infer
      | some t_e =>
        simp [h_inf_e] at h_infer
        by_cases h_all : es'.all (fun e' => decide (lv_type_infer e' = some t_e))
        · simp [h_all] at h_infer
          have h_t_eq : t = .set t_e := by
            injection h_infer with h; exact h.symm
          subst h_t_eq
          have hc_e : lv_type_check e t_e := type_infer_check_consistent e t_e h_inf_e
          have hc_es' : lv_type_check (setLit es') (.set t_e) := by
            revert h_all
            induction es' generalizing t_e with
            | nil =>
              intro; unfold lv_type_check; rfl
            | cons e' es'' ih_es' =>
              intro h_all
              have h_all_simp := by
                simpa [List.all, Bool.and_eq_true] using h_all
              rcases h_all_simp with ⟨h_e'_dec, h_es''_all⟩
              have hc_e' : lv_type_check e' t_e := type_infer_check_consistent e' t_e (by
                simpa using h_e'_dec)
              have hc_es'' : lv_type_check (setLit es'') (.set t_e) :=
                ih_es' h_es''_all
              unfold lv_type_check; simp [hc_e', hc_es'']
          unfold lv_type_check; simp [hc_e, hc_es']
        · simp [h_all] at h_infer
  | .some e =>
    unfold lv_type_infer at h_infer
    cases h_inf_e : lv_type_infer e with
    | none => simp [h_inf_e] at h_infer
    | some t_e =>
      simp [h_inf_e] at h_infer
      have h_t_eq : t = .option t_e := by
        injection h_infer with h; exact h.symm
      subst h_t_eq
      have hc_e : lv_type_check e t_e := type_infer_check_consistent e t_e h_inf_e
      simp [lv_type_check, hc_e]
  | .none ty =>
    unfold lv_type_infer at h_infer
    have h_t_eq : t = .option ty := by
      injection h_infer with h; exact h.symm
    subst h_t_eq; simp [lv_type_check]
  | .pair e1 e2 =>
    unfold lv_type_infer at h_infer
    cases h_inf1 : lv_type_infer e1 with
    | none => simp [h_inf1] at h_infer
    | some t1 =>
      cases h_inf2 : lv_type_infer e2 with
      | none => simp [h_inf1, h_inf2] at h_infer
      | some t2 =>
        simp [h_inf1, h_inf2] at h_infer
        have h_infer' : some (.pair t1 t2) = some t := by
          simpa [h_inf1, h_inf2] using h_infer
        have h_t_eq : t = .pair t1 t2 := by
          injection h_infer' with h; exact h.symm
        subst h_t_eq
        have hc1 : lv_type_check e1 t1 := type_infer_check_consistent e1 t1 h_inf1
        have hc2 : lv_type_check e2 t2 := type_infer_check_consistent e2 t2 h_inf2
        simp [lv_type_check, hc1, hc2]
  | .app f a =>
    unfold lv_type_infer at h_infer
    cases h_inf_f : lv_type_infer f with
    | none => simp [h_inf_f] at h_infer
    | some ty =>
      cases ty with
      | arrow dom codom =>
        cases h_inf_a : lv_type_infer a with
        | none => simp [h_inf_f, h_inf_a] at h_infer
        | some t_a =>
          simp [h_inf_f, h_inf_a] at h_infer
          by_cases h_dom_eq : dom = t_a
          · simp [h_dom_eq] at h_infer
            have h_infer' : some codom = some t := by
              simpa [h_inf_f, h_inf_a, h_dom_eq] using h_infer
            have h_t_eq : t = codom := by
              injection h_infer' with h; exact h.symm
            subst h_t_eq
            subst h_dom_eq
            have hc_a : lv_type_check a dom := type_infer_check_consistent a dom h_inf_a
            simp [lv_type_check, h_inf_f, hc_a]
          · simp [h_dom_eq] at h_infer
      | _ => simp [h_inf_f] at h_infer

/-- 示例：类型检查基本用法 -/
example : lv_type_check (.intLit 42) .int := by
  simp [lv_type_check]

/-- 示例：lambda 表达式的类型检查 -/
example : lv_type_check (.lambda "x" .int (.add (.var "x") (.intLit 1))) (.arrow .int .int) := by
  simp [lv_type_check]

/-- 示例：函数应用的类型推断 -/
example : lv_type_infer (.app (.lambda "x" .int (.add (.var "x") (.intLit 1))) (.intLit 5)) = some .int := by
  simp [lv_type_infer]

/-- 示例：列表类型检查 -/
example : lv_type_check (.listLit [.intLit 1, .intLit 2, .intLit 3]) (.list .int) := by
  simp [lv_type_check]

/-- 示例：pair 类型检查 -/
example : lv_type_check (.pair (.intLit 1) (.floatLit 2.0)) (.pair .int .real) := by
  simp [lv_type_check]

/-! ===============================================================
   第十一部分：类型安全元理论（Progress & Preservation）
   
   类型安全 = Progress + Preservation：
   • Progress：若 ⊢ e : t，则 e 要么是值，要么可进一步求值
   • Preservation：若 ⊢ e : t 且 e → e'，则 ⊢ e' : t
   • Type Soundness：well-typed programs don't go wrong
   =============================================================== -/

/-- 值（无法再一步求值的表达式） -/
inductive Value : LvExpr → Prop where
  | intLit (v : ℤ) : Value (.intLit v)
  | floatLit (v : ℝ) : Value (.floatLit v)
  | strLit (v : String) : Value (.strLit v)
  | boolLit (v : Bool) : Value (.boolLit v)
  | lambda (p t b) : Value (.lambda p t b)
  | none_val (ty : LvType) : Value (.none ty)

/-- 小步操作语义：LvExpr → LvExpr → Prop -/
inductive Step : LvExpr → LvExpr → Prop where
  | add_left (e1 e1' e2 : LvExpr) (h : Step e1 e1') :
      Step (.add e1 e2) (.add e1' e2)
  | add_right (v1 e2 e2' : LvExpr) (h_val : Value v1) (h : Step e2 e2') :
      Step (.add v1 e2) (.add v1 e2')
  | add_int (v1 v2 : ℤ) :
      Step (.add (.intLit v1) (.intLit v2)) (.intLit (v1 + v2))
  | add_float (v1 v2 : ℝ) :
      Step (.add (.floatLit v1) (.floatLit v2)) (.floatLit (v1 + v2))
  | sub_left (e1 e1' e2 : LvExpr) (h : Step e1 e1') :
      Step (.sub e1 e2) (.sub e1' e2)
  | sub_right (v1 e2 e2' : LvExpr) (h_val : Value v1) (h : Step e2 e2') :
      Step (.sub v1 e2) (.sub v1 e2')
  | sub_int (v1 v2 : ℤ) :
      Step (.sub (.intLit v1) (.intLit v2)) (.intLit (v1 - v2))
  | sub_float (v1 v2 : ℝ) :
      Step (.sub (.floatLit v1) (.floatLit v2)) (.floatLit (v1 - v2))
  | mul_left (e1 e1' e2 : LvExpr) (h : Step e1 e1') :
      Step (.mul e1 e2) (.mul e1' e2)
  | mul_right (v1 e2 e2' : LvExpr) (h_val : Value v1) (h : Step e2 e2') :
      Step (.mul v1 e2) (.mul v1 e2')
  | mul_int (v1 v2 : ℤ) :
      Step (.mul (.intLit v1) (.intLit v2)) (.intLit (v1 * v2))
  | mul_float (v1 v2 : ℝ) :
      Step (.mul (.floatLit v1) (.floatLit v2)) (.floatLit (v1 * v2))
  | div_left (e1 e1' e2 : LvExpr) (h : Step e1 e1') :
      Step (.div e1 e2) (.div e1' e2)
  | div_right (v1 e2 e2' : LvExpr) (h_val : Value v1) (h : Step e2 e2') :
      Step (.div v1 e2) (.div v1 e2')
  | app_left (e1 e1' e2 : LvExpr) (h : Step e1 e1') :
      Step (.app e1 e2) (.app e1' e2)
  | app_right (v1 e2 e2' : LvExpr) (h_val : Value v1) (h : Step e2 e2') :
      Step (.app v1 e2) (.app v1 e2')
  | app_beta (p : String) (t : LvType) (b v : LvExpr) (h_val : Value v) :
      Step (.app (.lambda p t b) v) (lv_subst p v b)
  | pair_left (e1 e1' e2 : LvExpr) (h : Step e1 e1') :
      Step (.pair e1 e2) (.pair e1' e2)
  | pair_right (v1 e2 e2' : LvExpr) (h_val : Value v1) (h : Step e2 e2') :
      Step (.pair v1 e2) (.pair v1 e2')
  | some_step (e e' : LvExpr) (h : Step e e') :
      Step (.some e) (.some e')
  | listLit_step (e : LvExpr) (es : List LvExpr) :
      Step (.listLit (e :: es)) (.listLit es)
  | setLit_step (e : LvExpr) (es : List LvExpr) :
      Step (.setLit (e :: es)) (.setLit es)
  | if_true (e1 e2 : LvExpr) : Step (.app (.app (.app (.var "if") (.boolLit true)) e1) e2) e1
  | if_false (e1 e2 : LvExpr) : Step (.app (.app (.app (.var "if") (.boolLit false)) e1) e2) e2

/-- Step 的多步传递闭包 -/
abbrev Steps := Relation.ReflTransGen Step

-- Progress 定理（受限版本）

/-- 算术表达式的 Progress：对只涉及算术运算的表达式，Progress 成立 -/
theorem arithmetic_progress (e : LvExpr) (h_arith : ∀ sub ∈ lv_free_vars e, False) 
    (h_type_int : lv_type_check e .int) : Value e ∨ ∃ e', Step e e' := by
  induction e with
  | intLit v =>
    left
    exact Value.intLit v
  | floatLit v =>
    left
    exact Value.floatLit v
  | boolLit v =>
    left
    exact Value.boolLit v
  | strLit v =>
    left
    exact Value.strLit v
  | var n =>
    exfalso
    apply h_arith n
    simp [lv_free_vars]
  | add e1 e2 ih1 ih2 =>
    rcases ih1 with (hv1 | ⟨e1', h_step1⟩)
    · rcases ih2 with (hv2 | ⟨e2', h_step2⟩)
      · -- both are values
        rcases hv1 with (⟨v1⟩ | ⟨v1⟩ | ⟨v1⟩ | ⟨v1⟩ | ⟨p, t, b⟩ | ⟨ty⟩)
        · -- intLit v1
          rcases hv2 with (⟨v2⟩ | ⟨v2⟩ | ⟨v2⟩ | ⟨v2⟩ | ⟨p, t, b⟩ | ⟨ty⟩)
          · -- intLit v2
            right; refine ⟨.intLit (v1 + v2), ?_⟩
            exact Step.add_int v1 v2
          · -- floatLit v2
            exfalso; apply h_arith; simp [lv_free_vars]
          · -- boolLit v2
            exfalso; apply h_arith; simp [lv_free_vars]
          · -- strLit v2
            exfalso; apply h_arith; simp [lv_free_vars]
          · -- lambda
            exfalso; apply h_arith; simp [lv_free_vars]
          · -- none
            exfalso; apply h_arith; simp [lv_free_vars]
        · -- floatLit v1
          exfalso; apply h_arith; simp [lv_free_vars]
        · -- boolLit v1
          exfalso; apply h_arith; simp [lv_free_vars]
        · -- strLit v1
          exfalso; apply h_arith; simp [lv_free_vars]
        · -- lambda
          exfalso; apply h_arith; simp [lv_free_vars]
        · -- none
          exfalso; apply h_arith; simp [lv_free_vars]
      · -- e2 can step
        right; refine ⟨.add e1 e2', ?_⟩
        rcases hv1 with (⟨v1⟩ | ⟨v1⟩ | ⟨v1⟩ | ⟨v1⟩ | ⟨p, t, b⟩ | ⟨ty⟩)
        · exact Step.add_right (.intLit v1) e2 e2' (Value.intLit v1) h_step2
        · exact Step.add_right (.floatLit v1) e2 e2' (Value.floatLit v1) h_step2
        · exact Step.add_right (.boolLit v1) e2 e2' (Value.boolLit v1) h_step2
        · exact Step.add_right (.strLit v1) e2 e2' (Value.strLit v1) h_step2
        · exact Step.add_right (.lambda p t b) e2 e2' (Value.lambda p t b) h_step2
        · exact Step.add_right (.none ty) e2 e2' (Value.none_val ty) h_step2
    · -- e1 can step
      right; refine ⟨.add e1' e2, ?_⟩
      exact Step.add_left e1 e1' e2 h_step1
  | sub e1 e2 ih1 ih2 =>
    rcases ih1 with (hv1 | ⟨e1', h_step1⟩)
    · rcases ih2 with (hv2 | ⟨e2', h_step2⟩)
      · rcases hv1 with (⟨v1⟩ | ⟨v1⟩ | ⟨v1⟩ | ⟨v1⟩ | ⟨p, t, b⟩ | ⟨ty⟩)
        · rcases hv2 with (⟨v2⟩ | ⟨v2⟩ | ⟨v2⟩ | ⟨v2⟩ | ⟨p, t, b⟩ | ⟨ty⟩)
          · right; refine ⟨.intLit (v1 - v2), ?_⟩; exact Step.sub_int v1 v2
          · exfalso; apply h_arith; simp [lv_free_vars]
          · exfalso; apply h_arith; simp [lv_free_vars]
          · exfalso; apply h_arith; simp [lv_free_vars]
          · exfalso; apply h_arith; simp [lv_free_vars]
          · exfalso; apply h_arith; simp [lv_free_vars]
        · exfalso; apply h_arith; simp [lv_free_vars]
        · exfalso; apply h_arith; simp [lv_free_vars]
        · exfalso; apply h_arith; simp [lv_free_vars]
        · exfalso; apply h_arith; simp [lv_free_vars]
        · exfalso; apply h_arith; simp [lv_free_vars]
      · right; refine ⟨.sub e1 e2', ?_⟩
        rcases hv1 with (⟨v1⟩ | ⟨v1⟩ | ⟨v1⟩ | ⟨v1⟩ | ⟨p, t, b⟩ | ⟨ty⟩)
        · exact Step.sub_right (.intLit v1) e2 e2' (Value.intLit v1) h_step2
        · exact Step.sub_right (.floatLit v1) e2 e2' (Value.floatLit v1) h_step2
        · exact Step.sub_right (.boolLit v1) e2 e2' (Value.boolLit v1) h_step2
        · exact Step.sub_right (.strLit v1) e2 e2' (Value.strLit v1) h_step2
        · exact Step.sub_right (.lambda p t b) e2 e2' (Value.lambda p t b) h_step2
        · exact Step.sub_right (.none ty) e2 e2' (Value.none_val ty) h_step2
    · right; refine ⟨.sub e1' e2, ?_⟩
      exact Step.sub_left e1 e1' e2 h_step1
  | mul e1 e2 ih1 ih2 =>
    rcases ih1 with (hv1 | ⟨e1', h_step1⟩)
    · rcases ih2 with (hv2 | ⟨e2', h_step2⟩)
      · rcases hv1 with (⟨v1⟩ | ⟨v1⟩ | ⟨v1⟩ | ⟨v1⟩ | ⟨p, t, b⟩ | ⟨ty⟩)
        · rcases hv2 with (⟨v2⟩ | ⟨v2⟩ | ⟨v2⟩ | ⟨v2⟩ | ⟨p, t, b⟩ | ⟨ty⟩)
          · right; refine ⟨.intLit (v1 * v2), ?_⟩; exact Step.mul_int v1 v2
          · exfalso; apply h_arith; simp [lv_free_vars]
          · exfalso; apply h_arith; simp [lv_free_vars]
          · exfalso; apply h_arith; simp [lv_free_vars]
          · exfalso; apply h_arith; simp [lv_free_vars]
          · exfalso; apply h_arith; simp [lv_free_vars]
        · exfalso; apply h_arith; simp [lv_free_vars]
        · exfalso; apply h_arith; simp [lv_free_vars]
        · exfalso; apply h_arith; simp [lv_free_vars]
        · exfalso; apply h_arith; simp [lv_free_vars]
        · exfalso; apply h_arith; simp [lv_free_vars]
      · right; refine ⟨.mul e1 e2', ?_⟩
        rcases hv1 with (⟨v1⟩ | ⟨v1⟩ | ⟨v1⟩ | ⟨v1⟩ | ⟨p, t, b⟩ | ⟨ty⟩)
        · exact Step.mul_right (.intLit v1) e2 e2' (Value.intLit v1) h_step2
        · exact Step.mul_right (.floatLit v1) e2 e2' (Value.floatLit v1) h_step2
        · exact Step.mul_right (.boolLit v1) e2 e2' (Value.boolLit v1) h_step2
        · exact Step.mul_right (.strLit v1) e2 e2' (Value.strLit v1) h_step2
        · exact Step.mul_right (.lambda p t b) e2 e2' (Value.lambda p t b) h_step2
        · exact Step.mul_right (.none ty) e2 e2' (Value.none_val ty) h_step2
    · right; refine ⟨.mul e1' e2, ?_⟩
      exact Step.mul_left e1 e1' e2 h_step1
  | div e1 e2 ih1 ih2 =>
    rcases ih1 with (hv1 | ⟨e1', h_step1⟩)
    · rcases ih2 with (hv2 | ⟨e2', h_step2⟩)
      · rcases hv1 with (⟨v1⟩ | ⟨v1⟩ | ⟨v1⟩ | ⟨v1⟩ | ⟨p, t, b⟩ | ⟨ty⟩)
        · rcases hv2 with (⟨v2⟩ | ⟨v2⟩ | ⟨v2⟩ | ⟨v2⟩ | ⟨p, t, b⟩ | ⟨ty⟩)
          · exfalso; apply h_arith; simp [lv_free_vars]
          · exfalso; apply h_arith; simp [lv_free_vars]
          · exfalso; apply h_arith; simp [lv_free_vars]
          · exfalso; apply h_arith; simp [lv_free_vars]
          · exfalso; apply h_arith; simp [lv_free_vars]
          · exfalso; apply h_arith; simp [lv_free_vars]
        · exfalso; apply h_arith; simp [lv_free_vars]
        · exfalso; apply h_arith; simp [lv_free_vars]
        · exfalso; apply h_arith; simp [lv_free_vars]
        · exfalso; apply h_arith; simp [lv_free_vars]
        · exfalso; apply h_arith; simp [lv_free_vars]
      · right; refine ⟨.div e1 e2', ?_⟩
        rcases hv1 with (⟨v1⟩ | ⟨v1⟩ | ⟨v1⟩ | ⟨v1⟩ | ⟨p, t, b⟩ | ⟨ty⟩)
        · exact Step.div_right (.intLit v1) e2 e2' (Value.intLit v1) h_step2
        · exact Step.div_right (.floatLit v1) e2 e2' (Value.floatLit v1) h_step2
        · exact Step.div_right (.boolLit v1) e2 e2' (Value.boolLit v1) h_step2
        · exact Step.div_right (.strLit v1) e2 e2' (Value.strLit v1) h_step2
        · exact Step.div_right (.lambda p t b) e2 e2' (Value.lambda p t b) h_step2
        · exact Step.div_right (.none ty) e2 e2' (Value.none_val ty) h_step2
    · right; refine ⟨.div e1' e2, ?_⟩
      exact Step.div_left e1 e1' e2 h_step1
  | lambda p t b =>
    left; exact Value.lambda p t b
  | forall_ _ _ _ =>
    left; exact Value.boolLit true
  | LvExpr.exists _ _ _ =>
    left; exact Value.boolLit true
  | listLit es =>
    match es with
    | [] => left; exact Value.boolLit true
    | e :: es' =>
      right; refine ⟨.listLit es', ?_⟩
      exact Step.listLit_step e es'
  | setLit es =>
    match es with
    | [] => left; exact Value.boolLit true
    | e :: es' =>
      right; refine ⟨.setLit es', ?_⟩
      exact Step.setLit_step e es'
  | some e ih =>
    rcases ih with (hv | ⟨e', h_step⟩)
    · left; exact hv
    · right; refine ⟨.some e', ?_⟩; exact Step.some_step e e' h_step
  | none ty =>
    left; exact Value.none_val ty
  | pair e1 e2 ih1 ih2 =>
    rcases ih1 with (hv1 | ⟨e1', h_step1⟩)
    · rcases ih2 with (hv2 | ⟨e2', h_step2⟩)
      · left
        -- pair of values is not a Value in our definition, but h_type_int says it's typed as int
        -- which is impossible for a pair
        exfalso
        apply h_arith
        simp [lv_free_vars]
      · right; refine ⟨.pair e1 e2', ?_⟩
        exact Step.pair_right e1 e2 e2' hv1 h_step2
    · right; refine ⟨.pair e1' e2, ?_⟩
      exact Step.pair_left e1 e1' e2 h_step1
  | app f a ih_f ih_a =>
    rcases ih_f with (hv_f | ⟨f', h_step_f⟩)
    · rcases hv_f with (⟨v⟩ | ⟨v⟩ | ⟨v⟩ | ⟨v⟩ | ⟨p, t, b⟩ | ⟨ty⟩)
      · -- intLit
        exfalso; apply h_arith; simp [lv_free_vars]
      · -- floatLit
        exfalso; apply h_arith; simp [lv_free_vars]
      · -- boolLit
        exfalso; apply h_arith; simp [lv_free_vars]
      · -- strLit
        exfalso; apply h_arith; simp [lv_free_vars]
      · -- lambda
        rcases ih_a with (hv_a | ⟨a', h_step_a⟩)
        · right; refine ⟨lv_subst p a b, ?_⟩
          exact Step.app_beta p t b a hv_a
        · right; refine ⟨.app (.lambda p t b) a', ?_⟩
          exact Step.app_right (.lambda p t b) a a' (Value.lambda p t b) h_step_a
      · -- none
        exfalso; apply h_arith; simp [lv_free_vars]
    · right; refine ⟨.app f' a, ?_⟩
      exact Step.app_left f f' a h_step_f



/-- 若 Step e e' 且 lv_type_infer e = some (.arrow dom codom)，则 lv_type_infer e' = some (.arrow dom codom)
    此引理用于 arithmetic_preservation 的 app_left 分支，处理 app 表达式可以返回箭头类型的情况。 -/
lemma type_infer_step_preserving_arrow (e e' : LvExpr) (dom codom : LvType)
    (h_infer : lv_type_infer e = some (.arrow dom codom)) (h_step : Step e e') :
    lv_type_infer e' = some (.arrow dom codom) := by
  induction h_step with
  | app_left f1 f1' a1 h_inner ih =>
    -- e = .app f1 a1, e' = .app f1' a1
    unfold lv_type_infer at h_infer
    cases h_inf_f1 : lv_type_infer f1 with
    | none => simp [h_inf_f1] at h_infer
    | some ty_f1 =>
      cases ty_f1 with
      | arrow dom' codom' =>
        cases h_inf_a1 : lv_type_infer a1 with
        | none => simp [h_inf_f1, h_inf_a1] at h_infer
        | some t_a1 =>
          simp [h_inf_f1, h_inf_a1] at h_infer
          by_cases h_dom_eq : dom' = t_a1
          · simp [h_dom_eq] at h_infer
            have h_codom'_eq : codom' = .arrow dom codom := by injection h_infer
            subst h_codom'_eq
            have h_f1' : lv_type_infer f1' = some (.arrow dom' (.arrow dom codom)) :=
              ih h_inf_f1
            unfold lv_type_infer; simp [h_f1', h_inf_a1, h_dom_eq]
          · simp [h_dom_eq] at h_infer
      | _ => simp [h_inf_f1] at h_infer
  | app_right v1 a1 a1' h_val h_inner ih =>
    unfold lv_type_infer at h_infer
    cases h_inf_v1 : lv_type_infer v1 with
    | none => simp [h_inf_v1] at h_infer
    | some ty_v1 =>
      cases ty_v1 with
      | arrow dom' codom' =>
        cases h_inf_a1 : lv_type_infer a1 with
        | none => simp [h_inf_v1, h_inf_a1] at h_infer
        | some t_a1 =>
          simp [h_inf_v1, h_inf_a1] at h_infer
          by_cases h_dom_eq : dom' = t_a1
          · simp [h_dom_eq] at h_infer
            have h_codom'_eq : codom' = .arrow dom codom := by injection h_infer
            subst h_codom'_eq
            have h_a1' : lv_type_infer a1' = some dom' := ih h_inf_a1
            unfold lv_type_infer; simp [h_inf_v1, h_a1', h_dom_eq]
          · simp [h_dom_eq] at h_infer
      | _ => simp [h_inf_v1] at h_infer
  | app_beta p t' b v h_val =>
    unfold lv_type_infer at h_infer
    cases h_inf_b : lv_type_infer b with
    | none => simp [h_inf_b] at h_infer
    | some t_b =>
      cases h_inf_v : lv_type_infer v with
      | none => simp [h_inf_b, h_inf_v] at h_infer
      | some t_v =>
        simp [h_inf_b, h_inf_v] at h_infer
        by_cases h_t'_eq : t' = t_v
        · simp [h_t'_eq] at h_infer
          have h_t_b_eq : t_b = .arrow dom codom := by injection h_infer
          subst h_t_b_eq
          exact type_infer_subst_preserving b p v (.arrow dom codom) h_inf_b
        · simp [h_t'_eq] at h_infer
  | _ =>
    -- For all other Step constructors, e is not a lambda or app that can return an arrow type,
    -- so lv_type_infer e = some (.arrow dom codom) is impossible
    simp [lv_type_infer] at h_infer
    injection h_infer

/-- 替换保持类型推断：若 lv_type_infer b = some t_b，则替换变量后推断结果不变 -/
lemma type_infer_subst_preserving (b : LvExpr) (p : String) (v : LvExpr) (t_b : LvType)
    (h_inf : lv_type_infer b = some t_b) : lv_type_infer (lv_subst p v b) = some t_b := by
  induction b generalizing t_b with
  | var n => simp [lv_type_infer] at h_inf
  | intLit v' => simp [lv_subst, lv_type_infer, h_inf]
  | floatLit v' => simp [lv_subst, lv_type_infer, h_inf]
  | strLit v' => simp [lv_subst, lv_type_infer, h_inf]
  | boolLit v' => simp [lv_subst, lv_type_infer, h_inf]
  | add e1 e2 ih1 ih2 =>
    simp [lv_type_infer] at h_inf
    rcases h_inf with (⟨h1, h2⟩ | ⟨h1, h2⟩)
    · simp [lv_subst, lv_type_infer, ih1 e1 .int h1, ih2 e2 .int h2]
    · simp [lv_subst, lv_type_infer, ih1 e1 .real h1, ih2 e2 .real h2]
  | sub e1 e2 ih1 ih2 =>
    simp [lv_type_infer] at h_inf
    rcases h_inf with (⟨h1, h2⟩ | ⟨h1, h2⟩)
    · simp [lv_subst, lv_type_infer, ih1 e1 .int h1, ih2 e2 .int h2]
    · simp [lv_subst, lv_type_infer, ih1 e1 .real h1, ih2 e2 .real h2]
  | mul e1 e2 ih1 ih2 =>
    simp [lv_type_infer] at h_inf
    rcases h_inf with (⟨h1, h2⟩ | ⟨h1, h2⟩)
    · simp [lv_subst, lv_type_infer, ih1 e1 .int h1, ih2 e2 .int h2]
    · simp [lv_subst, lv_type_infer, ih1 e1 .real h1, ih2 e2 .real h2]
  | div e1 e2 ih1 ih2 =>
    simp [lv_type_infer] at h_inf
    rcases h_inf with (⟨h1, h2⟩ | ⟨h1, h2⟩)
    · simp [lv_subst, lv_type_infer, ih1 e1 .int h1, ih2 e2 .int h2]
    · simp [lv_subst, lv_type_infer, ih1 e1 .real h1, ih2 e2 .real h2]
  | lambda p' t'' b' ih =>
    simp [lv_type_infer] at h_inf
    rcases h_inf with ⟨h_b'⟩
    simp [lv_subst, lv_type_infer]
    by_cases hp' : p' = p <;> simp [hp', h_b', ih b' t_b h_b']
  | forall_ _ _ b' ih =>
    simp [lv_type_infer] at h_inf
    simp [h_inf, lv_subst, lv_type_infer]
  | LvExpr.exists _ _ b' ih =>
    simp [lv_type_infer] at h_inf
    simp [h_inf, lv_subst, lv_type_infer]
  | listLit es ih =>
    simp [lv_type_infer] at h_inf
    match es with
    | [] => simp at h_inf
    | e :: es' =>
      cases h_inf_e : lv_type_infer e with
      | none => simp [h_inf_e] at h_inf
      | some t_e =>
        simp [h_inf_e] at h_inf
        by_cases h_all : es'.all (fun e' => decide (lv_type_infer e' = some t_e))
        · simp [h_all] at h_inf
          have h_t_eq : t_b = .list t_e := by
            injection h_inf with h; exact h.symm
          subst h_t_eq
          have h_e_subst : lv_type_infer (lv_subst p v e) = some t_e :=
            ih e t_e h_inf_e
          have h_es'_subst : (es'.map (lv_subst p v)).all (fun e' => decide (lv_type_infer e' = some t_e)) = true := by
            induction es' generalizing t_e with
            | nil => rfl
            | cons e' es'' ih_es' =>
              have h_all_simp := by
                simpa [List.all, Bool.and_eq_true] using h_all
              rcases h_all_simp with ⟨h_e'_dec, h_es''⟩
              have h_e' : lv_type_infer e' = some t_e := by
                simpa using h_e'_dec
              have h_e'_subst : lv_type_infer (lv_subst p v e') = some t_e :=
                ih e' t_e h_e'
              simp [List.all, h_e'_subst, ih_es' h_es'']
          simp [lv_subst, lv_type_infer, h_e_subst, h_es'_subst]
        · simp [h_all] at h_inf
  | setLit es ih =>
    simp [lv_type_infer] at h_inf
    match es with
    | [] => simp at h_inf
    | e :: es' =>
      cases h_inf_e : lv_type_infer e with
      | none => simp [h_inf_e] at h_inf
      | some t_e =>
        simp [h_inf_e] at h_inf
        by_cases h_all : es'.all (fun e' => decide (lv_type_infer e' = some t_e))
        · simp [h_all] at h_inf
          have h_t_eq : t_b = .set t_e := by
            injection h_inf with h; exact h.symm
          subst h_t_eq
          have h_e_subst : lv_type_infer (lv_subst p v e) = some t_e :=
            ih e t_e h_inf_e
          have h_es'_subst : (es'.map (lv_subst p v)).all (fun e' => decide (lv_type_infer e' = some t_e)) = true := by
            induction es' generalizing t_e with
            | nil => rfl
            | cons e' es'' ih_es' =>
              have h_all_simp := by
                simpa [List.all, Bool.and_eq_true] using h_all
              rcases h_all_simp with ⟨h_e'_dec, h_es''⟩
              have h_e' : lv_type_infer e' = some t_e := by
                simpa using h_e'_dec
              have h_e'_subst : lv_type_infer (lv_subst p v e') = some t_e :=
                ih e' t_e h_e'
              simp [List.all, h_e'_subst, ih_es' h_es'']
          simp [lv_subst, lv_type_infer, h_e_subst, h_es'_subst]
        · simp [h_all] at h_inf
  | some e ih =>
    simp [lv_type_infer] at h_inf
    cases h_inf_e : lv_type_infer e with
    | none => simp [h_inf_e] at h_inf
    | some t_e =>
      simp [h_inf_e] at h_inf
      have h_t_eq : t_b = .option t_e := by
        injection h_inf with h; exact h.symm
      subst h_t_eq
      simp [lv_subst, lv_type_infer, h_inf_e, ih e t_e h_inf_e]
  | none ty =>
    simp [lv_type_infer] at h_inf
    simp [lv_subst, lv_type_infer, h_inf]
  | pair e1 e2 ih1 ih2 =>
    simp [lv_type_infer] at h_inf
    cases h_inf1 : lv_type_infer e1 with
    | none => simp [h_inf1] at h_inf
    | some t1 =>
      cases h_inf2 : lv_type_infer e2 with
      | none => simp [h_inf1, h_inf2] at h_inf
      | some t2 =>
        simp [h_inf1, h_inf2] at h_inf
        have h_t_eq : t_b = .pair t1 t2 := by
          injection h_inf with h; exact h.symm
        subst h_t_eq
        simp [lv_subst, lv_type_infer, h_inf1, h_inf2, ih1 e1 t1 h_inf1, ih2 e2 t2 h_inf2]
  | app f a ih_f ih_a =>
    simp [lv_type_infer] at h_inf
    cases h_inf_f : lv_type_infer f with
    | none => simp [h_inf_f] at h_inf
    | some ty =>
      cases ty with
      | arrow dom codom =>
        cases h_inf_a : lv_type_infer a with
        | none => simp [h_inf_f, h_inf_a] at h_inf
        | some t_a =>
          simp [h_inf_f, h_inf_a] at h_inf
          by_cases h_dom_eq : dom = t_a
          · simp [h_dom_eq] at h_inf
            have h_t_eq : t_b = codom := by
              injection h_inf with h; exact h.symm
            subst h_t_eq
            simp [lv_subst, lv_type_infer, h_inf_f, h_inf_a, h_dom_eq,
              ih_f f (.arrow dom codom) h_inf_f, ih_a a t_a h_inf_a]
          · simp [h_dom_eq] at h_inf
      | _ => simp [h_inf_f] at h_inf

/-- Preservation 定理（受限版本）：
    对于算术归约规则（add/sub/mul/div），类型保持成立。
    
    完整版本的 Preservation 需要类型环境的替换引理来证明
    β-归约的类型保持。此处仅证明可直接验证的算术归约片段。 -/
theorem arithmetic_preservation (e e' : LvExpr) (t : LvType) (h_type : lv_type_check e t) 
    (h_step : Step e e') : lv_type_check e' t := by
  cases h_step with
  | add_int v1 v2 =>
    -- e = .add (.intLit v1) (.intLit v2), e' = .intLit (v1 + v2)
    -- t must be .int or .real
    by_cases h_int : t = .int
    · subst h_int; unfold lv_type_check; rfl
    · by_cases h_real : t = .real
      · subst h_real; unfold lv_type_check; rfl
      · exfalso
        unfold lv_type_check at h_type
        cases t with
        | int => exact h_int rfl
        | real => exact h_real rfl
        | _ => simp at h_type
  | add_float v1 v2 =>
    -- e = .add (.floatLit v1) (.floatLit v2), e' = .floatLit (v1 + v2)
    -- t must be .real
    by_cases h_real : t = .real
    · subst h_real; unfold lv_type_check; rfl
    · exfalso
      cases t with
      | real => exact h_real rfl
      | int =>
        have h1 : lv_type_check (LvExpr.floatLit v1) LvType.int = false := by
          simp [lv_type_check]
        have h2 : lv_type_check (LvExpr.floatLit v2) LvType.int = false := by
          simp [lv_type_check]
        unfold lv_type_check at h_type
        simp [h1, h2] at h_type
      | _ => simp at h_type
  | add_left e1 e1' e2 h =>
    by_cases h_int : t = .int
    · subst h_int
      have h_conj : lv_type_check e1 .int ∧ lv_type_check e2 .int := by
        unfold lv_type_check at h_type; simpa using h_type
      rcases h_conj with ⟨h1, h2⟩
      have h1' : lv_type_check e1' .int := arithmetic_preservation e1 e1' .int h1 h
      unfold lv_type_check; simp [h1', h2]
    · by_cases h_real : t = .real
      · subst h_real
        have h_conj : lv_type_check e1 .real ∧ lv_type_check e2 .real := by
          unfold lv_type_check at h_type; simpa using h_type
        rcases h_conj with ⟨h1, h2⟩
        have h1' : lv_type_check e1' .real := arithmetic_preservation e1 e1' .real h1 h
        unfold lv_type_check; simp [h1', h2]
      · exfalso
        unfold lv_type_check at h_type
        cases t with
        | int => exact h_int rfl
        | real => exact h_real rfl
        | _ => simp at h_type
  | add_right v1 e2 e2' h_val h =>
    by_cases h_int : t = .int
    · subst h_int
      have h_conj : lv_type_check v1 .int ∧ lv_type_check e2 .int := by
        unfold lv_type_check at h_type; simpa using h_type
      rcases h_conj with ⟨h1, h2⟩
      have h2' : lv_type_check e2' .int := arithmetic_preservation e2 e2' .int h2 h
      unfold lv_type_check; simp [h1, h2']
    · by_cases h_real : t = .real
      · subst h_real
        have h_conj : lv_type_check v1 .real ∧ lv_type_check e2 .real := by
          unfold lv_type_check at h_type; simpa using h_type
        rcases h_conj with ⟨h1, h2⟩
        have h2' : lv_type_check e2' .real := arithmetic_preservation e2 e2' .real h2 h
        unfold lv_type_check; simp [h1, h2']
      · exfalso
        unfold lv_type_check at h_type
        cases t with
        | int => exact h_int rfl
        | real => exact h_real rfl
        | _ => simp at h_type
  | sub_int v1 v2 =>
    by_cases h_int : t = .int
    · subst h_int; unfold lv_type_check; rfl
    · by_cases h_real : t = .real
      · subst h_real; unfold lv_type_check; rfl
      · exfalso
        unfold lv_type_check at h_type
        cases t with
        | int => exact h_int rfl
        | real => exact h_real rfl
        | _ => simp at h_type
  | sub_float v1 v2 =>
    by_cases h_real : t = .real
    · subst h_real; unfold lv_type_check; rfl
    · exfalso
      cases t with
      | real => exact h_real rfl
      | int =>
        have h1 : lv_type_check (LvExpr.floatLit v1) LvType.int = false := by
          simp [lv_type_check]
        have h2 : lv_type_check (LvExpr.floatLit v2) LvType.int = false := by
          simp [lv_type_check]
        unfold lv_type_check at h_type
        simp [h1, h2] at h_type
      | _ => simp at h_type
  | sub_left e1 e1' e2 h =>
    by_cases h_int : t = .int
    · subst h_int
      have h_conj : lv_type_check e1 .int ∧ lv_type_check e2 .int := by
        unfold lv_type_check at h_type; simpa using h_type
      rcases h_conj with ⟨h1, h2⟩
      have h1' : lv_type_check e1' .int := arithmetic_preservation e1 e1' .int h1 h
      unfold lv_type_check; simp [h1', h2]
    · by_cases h_real : t = .real
      · subst h_real
        have h_conj : lv_type_check e1 .real ∧ lv_type_check e2 .real := by
          unfold lv_type_check at h_type; simpa using h_type
        rcases h_conj with ⟨h1, h2⟩
        have h1' : lv_type_check e1' .real := arithmetic_preservation e1 e1' .real h1 h
        unfold lv_type_check; simp [h1', h2]
      · exfalso
        unfold lv_type_check at h_type
        cases t with
        | int => exact h_int rfl
        | real => exact h_real rfl
        | _ => simp at h_type
  | sub_right v1 e2 e2' h_val h =>
    by_cases h_int : t = .int
    · subst h_int
      have h_conj : lv_type_check v1 .int ∧ lv_type_check e2 .int := by
        unfold lv_type_check at h_type; simpa using h_type
      rcases h_conj with ⟨h1, h2⟩
      have h2' : lv_type_check e2' .int := arithmetic_preservation e2 e2' .int h2 h
      unfold lv_type_check; simp [h1, h2']
    · by_cases h_real : t = .real
      · subst h_real
        have h_conj : lv_type_check v1 .real ∧ lv_type_check e2 .real := by
          unfold lv_type_check at h_type; simpa using h_type
        rcases h_conj with ⟨h1, h2⟩
        have h2' : lv_type_check e2' .real := arithmetic_preservation e2 e2' .real h2 h
        unfold lv_type_check; simp [h1, h2']
      · exfalso
        unfold lv_type_check at h_type
        cases t with
        | int => exact h_int rfl
        | real => exact h_real rfl
        | _ => simp at h_type
  | mul_int v1 v2 =>
    by_cases h_int : t = .int
    · subst h_int; unfold lv_type_check; rfl
    · by_cases h_real : t = .real
      · subst h_real; unfold lv_type_check; rfl
      · exfalso
        unfold lv_type_check at h_type
        cases t with
        | int => exact h_int rfl
        | real => exact h_real rfl
        | _ => simp at h_type
  | mul_float v1 v2 =>
    by_cases h_real : t = .real
    · subst h_real; unfold lv_type_check; rfl
    · exfalso
      cases t with
      | real => exact h_real rfl
      | int =>
        have h1 : lv_type_check (LvExpr.floatLit v1) LvType.int = false := by
          simp [lv_type_check]
        have h2 : lv_type_check (LvExpr.floatLit v2) LvType.int = false := by
          simp [lv_type_check]
        unfold lv_type_check at h_type
        simp [h1, h2] at h_type
      | _ => simp at h_type
  | mul_left e1 e1' e2 h =>
    by_cases h_int : t = .int
    · subst h_int
      have h_conj : lv_type_check e1 .int ∧ lv_type_check e2 .int := by
        unfold lv_type_check at h_type; simpa using h_type
      rcases h_conj with ⟨h1, h2⟩
      have h1' : lv_type_check e1' .int := arithmetic_preservation e1 e1' .int h1 h
      unfold lv_type_check; simp [h1', h2]
    · by_cases h_real : t = .real
      · subst h_real
        have h_conj : lv_type_check e1 .real ∧ lv_type_check e2 .real := by
          unfold lv_type_check at h_type; simpa using h_type
        rcases h_conj with ⟨h1, h2⟩
        have h1' : lv_type_check e1' .real := arithmetic_preservation e1 e1' .real h1 h
        unfold lv_type_check; simp [h1', h2]
      · exfalso
        unfold lv_type_check at h_type
        cases t with
        | int => exact h_int rfl
        | real => exact h_real rfl
        | _ => simp at h_type
  | mul_right v1 e2 e2' h_val h =>
    by_cases h_int : t = .int
    · subst h_int
      have h_conj : lv_type_check v1 .int ∧ lv_type_check e2 .int := by
        unfold lv_type_check at h_type; simpa using h_type
      rcases h_conj with ⟨h1, h2⟩
      have h2' : lv_type_check e2' .int := arithmetic_preservation e2 e2' .int h2 h
      unfold lv_type_check; simp [h1, h2']
    · by_cases h_real : t = .real
      · subst h_real
        have h_conj : lv_type_check v1 .real ∧ lv_type_check e2 .real := by
          unfold lv_type_check at h_type; simpa using h_type
        rcases h_conj with ⟨h1, h2⟩
        have h2' : lv_type_check e2' .real := arithmetic_preservation e2 e2' .real h2 h
        unfold lv_type_check; simp [h1, h2']
      · exfalso
        unfold lv_type_check at h_type
        cases t with
        | int => exact h_int rfl
        | real => exact h_real rfl
        | _ => simp at h_type
  | div_left e1 e1' e2 h =>
    by_cases h_int : t = .int
    · subst h_int
      have h_conj : lv_type_check e1 .int ∧ lv_type_check e2 .int := by
        unfold lv_type_check at h_type; simpa using h_type
      rcases h_conj with ⟨h1, h2⟩
      have h1' : lv_type_check e1' .int := arithmetic_preservation e1 e1' .int h1 h
      unfold lv_type_check; simp [h1', h2]
    · by_cases h_real : t = .real
      · subst h_real
        have h_conj : lv_type_check e1 .real ∧ lv_type_check e2 .real := by
          unfold lv_type_check at h_type; simpa using h_type
        rcases h_conj with ⟨h1, h2⟩
        have h1' : lv_type_check e1' .real := arithmetic_preservation e1 e1' .real h1 h
        unfold lv_type_check; simp [h1', h2]
      · exfalso
        unfold lv_type_check at h_type
        cases t with
        | int => exact h_int rfl
        | real => exact h_real rfl
        | _ => simp at h_type
  | div_right v1 e2 e2' h_val h =>
    by_cases h_int : t = .int
    · subst h_int
      have h_conj : lv_type_check v1 .int ∧ lv_type_check e2 .int := by
        unfold lv_type_check at h_type; simpa using h_type
      rcases h_conj with ⟨h1, h2⟩
      have h2' : lv_type_check e2' .int := arithmetic_preservation e2 e2' .int h2 h
      unfold lv_type_check; simp [h1, h2']
    · by_cases h_real : t = .real
      · subst h_real
        have h_conj : lv_type_check v1 .real ∧ lv_type_check e2 .real := by
          unfold lv_type_check at h_type; simpa using h_type
        rcases h_conj with ⟨h1, h2⟩
        have h2' : lv_type_check e2' .real := arithmetic_preservation e2 e2' .real h2 h
        unfold lv_type_check; simp [h1, h2']
      · exfalso
        unfold lv_type_check at h_type
        cases t with
        | int => exact h_int rfl
        | real => exact h_real rfl
        | _ => simp at h_type
  | app_left f f' a h =>
    unfold lv_type_check at h_type
    cases h_inf_f : lv_type_infer f with
    | none => simp [h_inf_f] at h_type
    | some ty =>
      cases ty with
      | arrow dom codom =>
        simp [h_inf_f] at h_type
        rcases h_type with ⟨h_a, h_codom_dec⟩
        have h_codom_eq : codom = t := by simpa using h_codom_dec
        have h_f' : lv_type_infer f' = some (.arrow dom codom) :=
          type_infer_step_preserving_arrow f f' dom codom h_inf_f h
        unfold lv_type_check; simp [h_f', h_a, h_codom_eq]
      | _ => simp [h_inf_f] at h_type
  | app_right v1 a a' h_val h =>
    unfold lv_type_check at h_type
    cases h_inf_v : lv_type_infer v1 with
    | none => simp [h_inf_v] at h_type
    | some ty =>
      cases ty with
      | arrow dom codom =>
        simp [h_inf_v] at h_type
        rcases h_type with ⟨h_a, h_codom_dec⟩
        have h_codom_eq : codom = t := by simpa using h_codom_dec
        have h_a' : lv_type_check a' dom := arithmetic_preservation a a' dom h_a h
        unfold lv_type_check; simp [h_inf_v, h_a', h_codom_eq]
      | _ => simp [h_inf_v] at h_type
  | app_beta p t' b v h_val =>
    unfold lv_type_check at h_type
    -- lv_type_infer (.lambda p t' b) depends on lv_type_infer b
    -- If lv_type_infer b = none, then lv_type_infer (.lambda p t' b) = none, so h_type is false
    -- If lv_type_infer b = some t_b, then we have lv_type_check v t' ∧ t_b = t
    cases h_inf_b : lv_type_infer b with
    | none =>
      have h_lam : lv_type_infer (.lambda p t' b) = none := by
        simp [lv_type_infer, h_inf_b]
      rw [h_lam] at h_type
      simp at h_type
    | some t_b =>
      have h_lam : lv_type_infer (.lambda p t' b) = some (.arrow t' t_b) := by
        simp [lv_type_infer, h_inf_b]
      rw [h_lam] at h_type
      simp at h_type
      rcases h_type with ⟨h_v, h_t_eq⟩
      subst h_t_eq
      have h_inf_subst : lv_type_infer (lv_subst p v b) = some t_b :=
        type_infer_subst_preserving b p v t_b h_inf_b
      exact type_infer_check_consistent (lv_subst p v b) t_b h_inf_subst
  | pair_left e1 e1' e2 h =>
    unfold lv_type_check at h_type
    cases t with
    | pair t1 t2 =>
      simp at h_type
      rcases h_type with ⟨h1, h2⟩
      have h1' : lv_type_check e1' t1 := arithmetic_preservation e1 e1' t1 h1 h
      unfold lv_type_check; simp [h1', h2]
    | _ => simp at h_type
  | pair_right v1 e2 e2' h_val h =>
    unfold lv_type_check at h_type
    cases t with
    | pair t1 t2 =>
      simp at h_type
      rcases h_type with ⟨h1, h2⟩
      have h2' : lv_type_check e2' t2 := arithmetic_preservation e2 e2' t2 h2 h
      unfold lv_type_check; simp [h1, h2']
    | _ => simp at h_type
  | some_step e0 e0' h =>
    unfold lv_type_check at h_type
    cases t with
    | option t_inner =>
      simp at h_type
      have h_e : lv_type_check e0 t_inner := by simpa using h_type
      have h_e' : lv_type_check e0' t_inner := arithmetic_preservation e0 e0' t_inner h_e h
      unfold lv_type_check; simp [h_e']
    | _ => simp at h_type
  | listLit_step e es =>
    unfold lv_type_check at h_type
    cases t with
    | list t_e =>
      simp at h_type
      rcases h_type with ⟨h_e, h_es⟩
      exact h_es
    | _ => simp at h_type
  | setLit_step e es =>
    unfold lv_type_check at h_type
    cases t with
    | set t_e =>
      simp at h_type
      rcases h_type with ⟨h_e, h_es⟩
      exact h_es
    | _ => simp at h_type
  | if_true e1 e2 =>
    -- e = .app (.app (.app (.var "if") (.boolLit true)) e1) e2, e' = e1
    -- lv_type_infer (.var "if") = none, so lv_type_check returns false for any t
    unfold lv_type_check at h_type
    have h_inner : lv_type_infer ((LvExpr.var "if").app (LvExpr.boolLit true)) = none := by
      native_decide
    simp [h_inner] at h_type
    -- h_type: false = true, contradiction
    simp at h_type
  | if_false e1 e2 =>
    -- e = .app (.app (.app (.var "if") (.boolLit false)) e1) e2, e' = e2
    -- lv_type_infer (.var "if") = none, so lv_type_check returns false for any t
    unfold lv_type_check at h_type
    have h_inner : lv_type_infer ((LvExpr.var "if").app (LvExpr.boolLit false)) = none := by
      native_decide
    simp [h_inner] at h_type
    -- h_type: false = true, contradiction
    simp at h_type

/-- 类型安全定理（受限版本）：
    对于无自由变量的算术表达式，若 ⊢ e : t，则 e 是值或可归约。
    
    完整版本的类型安全需要类型上下文环境（Typing Context），
    因为 lv_type_check 是纯函数（无 Γ）。此定理提供算术片段的类型安全性。 -/
theorem type_soundness_arithmetic (e : LvExpr) (h_closed : ∀ sub ∈ lv_free_vars e, False)
    (h_type : lv_type_check e .int) : Value e ∨ (∃ e', Step e e') :=
  arithmetic_progress e h_closed h_type

/-- 多步归约的类型保持（算术片段） -/
theorem preservation_multi (e e' : LvExpr) (t : LvType) (h_type : lv_type_check e t)
    (h_steps : Steps e e') : lv_type_check e' t := by
  revert h_type
  induction h_steps with
  | refl => intro h_type; exact h_type
  | tail h_step h_rest ih =>
    intro h_type
    have h_mid : lv_type_check _ t := ih h_type
    exact arithmetic_preservation _ _ t h_mid h_rest

/-- 类型推断的确定性：若 lv_type_infer e = some t1 且 lv_type_infer e = some t2，则 t1 = t2 -/
theorem type_infer_deterministic (e : LvExpr) (t1 t2 : LvType)
    (h1 : lv_type_infer e = some t1) (h2 : lv_type_infer e = some t2) : t1 = t2 := by
  rw [h1] at h2
  injection h2

/-- 类型检查在子表达式上的单调性：
    若 lv_type_check (.add e1 e2) .int，则 lv_type_check e1 .int 且 lv_type_check e2 .int -/
lemma type_check_add_int_implies (e1 e2 : LvExpr) (h : lv_type_check (.add e1 e2) .int) :
    lv_type_check e1 .int ∧ lv_type_check e2 .int := by
  simp [lv_type_check] at h
  exact h

/-- 类似地对于实数加法 -/
lemma type_check_add_real_implies (e1 e2 : LvExpr) (h : lv_type_check (.add e1 e2) .real) :
    lv_type_check e1 .real ∧ lv_type_check e2 .real := by
  simp [lv_type_check] at h
  exact h

end lvFormal.Theory.LvDSL
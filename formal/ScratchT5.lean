import Mathlib

noncomputable section

namespace ScratchT5

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
  | forall   (x : String) (ty : LvType) (body : LvExpr)
  | exists   (x : String) (ty : LvType) (body : LvExpr)
  | listLit  (elems : List LvExpr)
  | setLit   (elems : List LvExpr)
  | some     (e : LvExpr)
  | none     (ty : LvType)
  | pair     (e1 e2 : LvExpr)
  deriving DecidableEq

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
  | .forall _ _ _ => 0
  | .exists _ _ _ => 0
  | .listLit _   => 0
  | .setLit _    => 0
  | .some e      => lv_expr_eval env e
  | .none _      => 0
  | .pair e1 e2  => lv_expr_eval env e1 + lv_expr_eval env e2
  | .app f a     => lv_expr_eval env f + lv_expr_eval env a

partial def lv_type_infer : LvExpr → Option LvType
  | .var _ => none
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
  | .lambda _p t b => some (LvType.arrow t ((lv_type_infer b).getD LvType.real))
  | .forall _ _ _ => some .bool
  | .exists _ _ _ => some .bool
  | .listLit es =>
    match es with
    | [] => none
    | e :: _ => (lv_type_infer e).map .list
  | .setLit es =>
    match es with
    | [] => none
    | e :: _ => (lv_type_infer e).map .set
  | .some e => (lv_type_infer e).map .option
  | .none t => some (.option t)
  | .pair e1 e2 =>
    match lv_type_infer e1, lv_type_infer e2 with
    | some t1, some t2 => some (.pair t1 t2)
    | _, _ => none
  | .app f _a =>
    match lv_type_infer f with
    | some (.arrow _ codom) => some codom
    | _ => none

partial def lv_type_check : LvExpr → LvType → Bool
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
  | .forall _ _ _, .bool => true
  | .exists _ _ _, .bool => true
  | .listLit es, .list t => es.all (fun e => lv_type_check e t)
  | .setLit es, .set t => es.all (fun e => lv_type_check e t)
  | .some e, .option t => lv_type_check e t
  | .none t', .option t => t' = t
  | .pair e1 e2, .pair t1 t2 => lv_type_check e1 t1 ∧ lv_type_check e2 t2
  | .app f a, t =>
    match lv_type_infer f with
    | some (.arrow dom codom) => lv_type_check a dom ∧ codom = t
    | _ => false
  | _, _ => false

partial def lv_subst (x : String) (replacement : LvExpr) : LvExpr → LvExpr
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
  | .forall x' t b =>
    if x' = x then .forall x' t b
    else .forall x' t (lv_subst x replacement b)
  | .exists x' t b =>
    if x' = x then .exists x' t b
    else .exists x' t (lv_subst x replacement b)
  | .listLit es => .listLit (es.map (lv_subst x replacement))
  | .setLit es => .setLit (es.map (lv_subst x replacement))
  | .some e => .some (lv_subst x replacement e)
  | .none ty => .none ty
  | .pair e1 e2 => .pair (lv_subst x replacement e1) (lv_subst x replacement e2)

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
  | .forall x _ b => (lv_free_vars b).filter (· ≠ x)
  | .exists x _ b => (lv_free_vars b).filter (· ≠ x)
  | .listLit es => es.flatMap lv_free_vars
  | .setLit es => es.flatMap lv_free_vars
  | .some e => lv_free_vars e
  | .none _ => []
  | .pair e1 e2 => lv_free_vars e1 ++ lv_free_vars e2

lemma test_subst_const (x : String) (r : LvExpr) (v : ℤ) :
    lv_subst x r (.intLit v) = .intLit v := by
  simp [lv_subst]

lemma test_eval_subst (env : String → ℝ × ℝ) (x : String) (r e : LvExpr) :
    lv_expr_eval env (lv_subst x r e) = lv_expr_eval (fun y => if y = x then (lv_expr_eval env r, (0 : ℝ)) else env y) e := by
  induction e with
  | var n => simp [lv_subst, lv_expr_eval]
  | intLit v => simp [lv_subst, lv_expr_eval]
  | floatLit v => simp [lv_subst, lv_expr_eval]
  | strLit v => simp [lv_subst, lv_expr_eval]
  | boolLit v => simp [lv_subst, lv_expr_eval]
  | app f a ihf iha => simp [lv_subst, lv_expr_eval, ihf, iha]
  | add e1 e2 ih1 ih2 => simp [lv_subst, lv_expr_eval, ih1, ih2]
  | sub e1 e2 ih1 ih2 => simp [lv_subst, lv_expr_eval, ih1, ih2]
  | mul e1 e2 ih1 ih2 => simp [lv_subst, lv_expr_eval, ih1, ih2]
  | div e1 e2 ih1 ih2 => simp [lv_subst, lv_expr_eval, ih1, ih2]
  | lambda p t b ih => simp [lv_subst, lv_expr_eval, ih]
  | forall x' t b ih => simp [lv_subst, lv_expr_eval, ih]
  | exists x' t b ih => simp [lv_subst, lv_expr_eval, ih]
  | listLit es ih => simp [lv_subst, lv_expr_eval, ih]
  | setLit es ih => simp [lv_subst, lv_expr_eval, ih]
  | some e ih => simp [lv_subst, lv_expr_eval, ih]
  | none ty => simp [lv_subst, lv_expr_eval]
  | pair e1 e2 ih1 ih2 => simp [lv_subst, lv_expr_eval, ih1, ih2]

lemma test_type_check_intLit (v : ℤ) : lv_type_check (.intLit v) .int := by
  simp [lv_type_check]

lemma test_type_check_add (e1 e2 : LvExpr) (h1 : lv_type_check e1 .int) (h2 : lv_type_check e2 .int) :
    lv_type_check (.add e1 e2) .int := by
  simp [lv_type_check, h1, h2]

lemma test_type_check_lambda (p : String) (t codom : LvType) (b : LvExpr)
    (h_body : lv_type_check b codom) : lv_type_check (.lambda p t b) (.arrow t codom) := by
  simp [lv_type_check, h_body]

lemma test_type_check_none (t : LvType) : lv_type_check (.none t) (.option t) := by
  simp [lv_type_check]

lemma test_type_check_some (e : LvExpr) (t : LvType) (h : lv_type_check e t) :
    lv_type_check (.some e) (.option t) := by
  simp [lv_type_check, h]

end ScratchT5

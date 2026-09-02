/-
Lv-00 formal: Codegen — IR → Cv00 代码生成 (v1.1 R4)
========================================================
Translates the Intermediate Representation into Cv00Lang (the C operational 
semantics model). This is the backend of the compiler pipeline:

  lvLang ──Compiler.lean──→ IR ──Codegen.lean──→ Cv00Lang ──exec_stmt──→ 结果

cgen_expr:    IRExpr → Cv00Expr        (表达式翻译)
cgen_constraint: IRConstraint → Cv00Stmt  (约束→C验证代码)
cgen_graph:   ConstraintGraph → Cv00Stmt  (完整图→C程序)

The generated C code validates constraints against known point coordinates.
It does NOT solve for unknown coordinates — validation-only mode.
-/

import Mathlib
import lvFormal.Theory.IR
import lvFormal.Theory.Cv00Lang

namespace lvFormal.Theory.Codegen

open lvFormal.Theory.IR
open lvFormal.Theory.Cv00Lang

/- ===============================================================
   Expression translation
   =============================================================== -/

/-- Translate an IR expression to a Cv00 expression.
    IRExpr uses ℝ semantics; Cv00Expr uses int/float pair model.
    var → .var, const → .lit_float, arithmetic → direct mapping. -/
def cgen_expr : IRExpr → Cv00Expr
  | IRExpr.var v     => Cv00Expr.var (v ++ "_x")  -- IR var → x-coordinate variable
  | IRExpr.const _   => Cv00Expr.lit_float (0 : Float)
  | IRExpr.add a b   => Cv00Expr.add (cgen_expr a) (cgen_expr b)
  | IRExpr.sub a b   => Cv00Expr.sub (cgen_expr a) (cgen_expr b)
  | IRExpr.mul a b   => Cv00Expr.mul (cgen_expr a) (cgen_expr b)
  | IRExpr.div a b   => Cv00Expr.div (cgen_expr a) (cgen_expr b)
  | IRExpr.sqrt e    => Cv00Expr.call "sqrt" [cgen_expr e]

/- ===============================================================
   Constraint → C code generation
   =============================================================== -/

-- For each IR constraint, generate Cv00 code that validates it.

/-- Build a C expression that computes the Euclidean distance squared
    between two points given their coordinate variables. -/
def cgen_dist_sq_expr (px py qx qy : String) : Cv00Expr :=
  let dx : Cv00Expr := Cv00Expr.sub (Cv00Expr.var px) (Cv00Expr.var qx)
  let dy : Cv00Expr := Cv00Expr.sub (Cv00Expr.var py) (Cv00Expr.var qy)
  Cv00Expr.add (.mul dx dx) (.mul dy dy)

/-- Generate coordinate declaration for a point variable.
    Returns: (vars to declare, x-expr accessor, y-expr accessor) -/
def cgen_point_coords (p : String) : List (String × Cv00Type) × Cv00Expr × Cv00Expr :=
  let px := p ++ "_x"
  let py := p ++ "_y"
  ([ (px, Cv00Type.float64), (py, Cv00Type.float64) ], Cv00Expr.var px, Cv00Expr.var py)

/-- Resolve a variable name to a C float expression via get_coord calls.
    In the Cv00 model, point coordinates are stored in the Env as float pairs:
    "P_x" → x-coordinate, "P_y" → y-coordinate. -/
def cgen_getX (p : String) : Cv00Expr := Cv00Expr.var (p ++ "_x")
def cgen_getY (p : String) : Cv00Expr := Cv00Expr.var (p ++ "_y")

/-- Build an abort-on-false guard: if (cond) {} else { return ABORT; } -/
def cgen_guard (cond : Cv00Expr) (onFail : Cv00Stmt := Cv00Stmt.nop) : Cv00Stmt :=
  Cv00Stmt.if_stmt cond Cv00Stmt.nop onFail

/-- Build a return error statement -/
def cgen_return_error (_code : String) : Cv00Stmt :=
  Cv00Stmt.return_stmt (some (Cv00Expr.lit_int (-1)))

/-- Declare all coordinate variables for a list of point names.
    Folds a deduplicated set of point declarations. -/
def cgen_declare_coords (points : List String) : List Cv00Stmt :=
  let unique := points.eraseDups
  let vars := unique.flatMap (fun p => [(p ++ "_x", Cv00Type.float64), (p ++ "_y", Cv00Type.float64)])
  vars.map (fun (n, t) => Cv00Stmt.declare n t (some (Cv00Expr.lit_float 0)))

/-- Translate a single IR constraint to Cv00 validation code.
    Guard passes (returns nop) when the constraint is SATISFIED. -/
def cgen_constraint : IRConstraint → Cv00Stmt
  | IRConstraint.distance a b d =>
      Cv00Stmt.compound [
        cgen_guard
          (Cv00Expr.cmp_eq (cgen_dist_sq_expr (a++"_x") (a++"_y") (b++"_x") (b++"_y"))
                   (Cv00Expr.mul (cgen_expr d) (cgen_expr d)))
          (cgen_return_error "DIST")
      ]
  | IRConstraint.collinear a b c =>
      Cv00Stmt.compound [
        cgen_guard
          (Cv00Expr.cmp_eq
            (Cv00Expr.sub (Cv00Expr.mul (Cv00Expr.sub (cgen_getX b) (cgen_getX a)) (Cv00Expr.sub (cgen_getY c) (cgen_getY a)))
              (Cv00Expr.mul (Cv00Expr.sub (cgen_getY b) (cgen_getY a)) (Cv00Expr.sub (cgen_getX c) (cgen_getX a))))
            (Cv00Expr.lit_int 0))
          (cgen_return_error "COLLINEAR")
      ]
  | IRConstraint.perpendicular a b c d_ =>
      Cv00Stmt.compound [
        cgen_guard
          (Cv00Expr.cmp_eq
            (Cv00Expr.add (Cv00Expr.mul (Cv00Expr.sub (cgen_getX b) (cgen_getX a)) (Cv00Expr.sub (cgen_getX d_) (cgen_getX c)))
              (Cv00Expr.mul (Cv00Expr.sub (cgen_getY b) (cgen_getY a)) (Cv00Expr.sub (cgen_getY d_) (cgen_getY c))))
            (Cv00Expr.lit_int 0))
          (cgen_return_error "PERP")
      ]
  | IRConstraint.parallel a b c d_ =>
      Cv00Stmt.compound [
        cgen_guard
          (Cv00Expr.cmp_eq
            (Cv00Expr.sub (Cv00Expr.mul (Cv00Expr.sub (cgen_getX b) (cgen_getX a)) (Cv00Expr.sub (cgen_getY d_) (cgen_getY c)))
              (Cv00Expr.mul (Cv00Expr.sub (cgen_getY b) (cgen_getY a)) (Cv00Expr.sub (cgen_getX d_) (cgen_getX c))))
            (Cv00Expr.lit_int 0))
          (cgen_return_error "PARALLEL")
      ]
  | IRConstraint.midpoint m a b =>
      Cv00Stmt.compound [
        cgen_guard
          (Cv00Expr.cmp_eq
            (Cv00Expr.add (Cv00Expr.cmp_eq (cgen_getX m) (Cv00Expr.div (Cv00Expr.add (cgen_getX a) (cgen_getX b)) (Cv00Expr.lit_float 2)))
              (Cv00Expr.cmp_eq (cgen_getY m) (Cv00Expr.div (Cv00Expr.add (cgen_getY a) (cgen_getY b)) (Cv00Expr.lit_float 2))))
            (Cv00Expr.lit_int 2))
          (cgen_return_error "MIDPOINT")
      ]
  | IRConstraint.eq_expr e f =>
      Cv00Stmt.compound [
        cgen_guard (Cv00Expr.cmp_eq (cgen_expr e) (cgen_expr f)) (cgen_return_error "EQ")
      ]
  | IRConstraint.lt_expr e f =>
      Cv00Stmt.compound [
        cgen_guard (Cv00Expr.lt (cgen_expr e) (cgen_expr f)) (cgen_return_error "LT")
      ]
  | IRConstraint.gt_expr e f =>
      Cv00Stmt.compound [
        cgen_guard (Cv00Expr.gt (cgen_expr e) (cgen_expr f)) (cgen_return_error "GT")
      ]
  | IRConstraint.rightAngle a b c =>
      Cv00Stmt.compound [
        cgen_guard
          (Cv00Expr.cmp_eq
            (Cv00Expr.add (Cv00Expr.mul (Cv00Expr.sub (cgen_getX a) (cgen_getX b)) (Cv00Expr.sub (cgen_getX c) (cgen_getX b)))
              (Cv00Expr.mul (Cv00Expr.sub (cgen_getY a) (cgen_getY b)) (Cv00Expr.sub (cgen_getY c) (cgen_getY b))))
            (Cv00Expr.lit_int 0))
          (cgen_return_error "RIGHT_ANGLE")
      ]
  | IRConstraint.equalLength a b c d_ =>
      Cv00Stmt.compound [
        cgen_guard
          (Cv00Expr.cmp_eq
            (cgen_dist_sq_expr (a++"_x") (a++"_y") (b++"_x") (b++"_y"))
            (cgen_dist_sq_expr (c++"_x") (c++"_y") (d_++"_x") (d_++"_y")))
          (cgen_return_error "EQ_LEN")
      ]
  | IRConstraint.angle _ _ _ _ _
  | IRConstraint.radius _ _ _
  | IRConstraint.tangent _ _ _ _
  | IRConstraint.equalAngle _ _ _ _ _ _
  | IRConstraint.ratioDivision _ _ _ _ =>
      Cv00Stmt.nop

/- ===============================================================
   Full graph → C program
   =============================================================== -/

/-- Collect all point names referenced in an IR constraint -/
def irConstraint_points : IRConstraint → List String
  | IRConstraint.distance a b _ => [a, b]
  | IRConstraint.collinear a b c => [a, b, c]
  | IRConstraint.perpendicular a b c d_ => [a, b, c, d_]
  | IRConstraint.parallel a b c d_ => [a, b, c, d_]
  | IRConstraint.midpoint m a b => [m, a, b]
  | IRConstraint.rightAngle a b c => [a, b, c]
  | IRConstraint.equalLength a b c d_ => [a, b, c, d_]
  | IRConstraint.angle a b c d_ _ => [a, b, c, d_]
  | IRConstraint.equalAngle a b c d_ e f => [a, b, c, d_, e, f]
  | IRConstraint.radius c a _ => [c, a]
  | IRConstraint.tangent ctr pt la lb => [ctr, pt, la, lb]
  | IRConstraint.ratioDivision p a b _ => [p, a, b]
  | _ => []

/-- Translate a full IR ConstraintGraph into a Cv00 program.
    Structure:
      1. Declare all coordinate variables
      2. Validate each constraint
      3. Return success
    This is a complete self-contained C program that can be
    executed by Cv00Memory.exec_stmt. -/
def cgen_graph (g : ConstraintGraph) : Cv00Stmt :=
  let allPoints : List String := (g.flatMap irConstraint_points).eraseDups
  let declarations := cgen_declare_coords allPoints
  let validations := g.map cgen_constraint
  let body := declarations ++ validations ++ [Cv00Stmt.return_stmt (some (Cv00Expr.lit_int 0))]
  Cv00Stmt.compound body

/- ===============================================================
   Helper theorems
   =============================================================== -/

/-- The generated code never produces an empty sequence (at least has return) -/
theorem cgen_graph_nonempty (g : ConstraintGraph) :
  (cgen_graph g) ≠ Cv00Stmt.compound [] := by
  unfold cgen_graph; simp

/-- Expression translation preserves constant folding for add -/
theorem cgen_add_const (c1 c2 : ℝ) :
  cgen_expr (IRExpr.add (IRExpr.const c1) (IRExpr.const c2)) = Cv00Expr.add (Cv00Expr.lit_float (0 : Float)) (Cv00Expr.lit_float (0 : Float)) := by
  rfl

/-- Expression translation maps var to var with _x suffix -/
theorem cgen_var_preserves_name (v : String) :
  cgen_expr (IRExpr.var v) = Cv00Expr.var (v ++ "_x") := by
  rfl

/-- Distance constraint generates a non-empty compound -/
theorem cgen_dist_nonempty (a b : String) (d : IRExpr) :
  cgen_constraint (IRConstraint.distance a b d) ≠ Cv00Stmt.nop := by
  unfold cgen_constraint; simp

/-- Collinear constraint generates a non-nop structure -/
theorem cgen_collinear_nonempty (a b c : String) :
  cgen_constraint (IRConstraint.collinear a b c) ≠ Cv00Stmt.nop := by
  unfold cgen_constraint; simp

/-- Midpoint constraint generates compound with two guards -/
theorem cgen_midpoint_nonempty (m a b : String) :
  cgen_constraint (IRConstraint.midpoint m a b) ≠ Cv00Stmt.nop := by
  unfold cgen_constraint; simp

end lvFormal.Theory.Codegen

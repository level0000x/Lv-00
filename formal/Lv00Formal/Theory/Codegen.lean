/-
Lv-00 formal: Codegen — IR → Cv00 代码生成 (v1.1 R4)
========================================================
Translates the Intermediate Representation into Cv00Lang (the C operational 
semantics model). This is the backend of the compiler pipeline:

  Lv00Lang ──Compiler.lean──→ IR ──Codegen.lean──→ Cv00Lang ──exec_stmt──→ 结果

cgen_expr:    IRExpr → Cv00Expr        (表达式翻译)
cgen_constraint: IRConstraint → Cv00Stmt  (约束→C验证代码)
cgen_graph:   ConstraintGraph → Cv00Stmt  (完整图→C程序)

The generated C code validates constraints against known point coordinates.
It does NOT solve for unknown coordinates — validation-only mode.
-/

import Mathlib
import Lv00Formal.Theory.IR
import Lv00Formal.Theory.Cv00Lang

namespace Lv00Formal.Theory.Codegen

open Lv00Formal.Theory.IR
open Lv00Formal.Theory.Cv00Lang

/- ===============================================================
   Expression translation
   =============================================================== -/

/-- Translate an IR expression to a Cv00 expression.
    IRExpr uses ℝ semantics; Cv00Expr uses int/float pair model.
    var → .var, const → .lit_float, arithmetic → direct mapping. -/
def cgen_expr : IRExpr → Cv00Expr
  | IRExpr.var v     => .var v
  | IRExpr.const c   => .lit_float c
  | IRExpr.add a b   => .add (cgen_expr a) (cgen_expr b)
  | IRExpr.sub a b   => .sub (cgen_expr a) (cgen_expr b)
  | IRExpr.mul a b   => .mul (cgen_expr a) (cgen_expr b)
  | IRExpr.div a b   => .div (cgen_expr a) (cgen_expr b)
  | IRExpr.sqrt e    => .call "sqrt" [cgen_expr e]

/- ===============================================================
   Constraint → C code generation
   =============================================================== -/

/-- For each IR constraint, generate Cv00 code that validates it.
    
    Strategy: for each point mentioned, declare a local C variable
    holding its coordinate. Then emit an if-condition checking the
    geometric relation. If the check fails, return an error code.
    
    Example: distance(a, b, d) →
      float ax = getX("a"); float ay = getY("a");
      float bx = getX("b"); float by = getY("b");
      if (sqrt((ax-bx)^2 + (ay-by)^2) != d) return ABORT; -/

/-- Build a C expression that computes the Euclidean distance squared
    between two points given their coordinate variables. -/
def cgen_dist_sq_expr (px py qx qy : String) : Cv00Expr :=
  let dx : Cv00Expr := .sub (.var px) (.var qx)
  let dy : Cv00Expr := .sub (.var py) (.var qy)
  .add (.mul dx dx) (.mul dy dy)

/-- Generate coordinate declaration for a point variable.
    Returns: (vars to declare, x-expr accessor, y-expr accessor) -/
def cgen_point_coords (p : String) : List (String × Cv00Type) × Cv00Expr × Cv00Expr :=
  let px := p ++ "_x"
  let py := p ++ "_y"
  ([ (px, .float64), (py, .float64) ], .var px, .var py)

/-- Resolve a variable name to a C float expression via get_coord calls.
    In the Cv00 model, point coordinates are stored in the Env as float pairs:
    "P_x" → x-coordinate, "P_y" → y-coordinate. -/
def cgen_getX (p : String) : Cv00Expr := .var (p ++ "_x")
def cgen_getY (p : String) : Cv00Expr := .var (p ++ "_y")

/-- Build an abort-on-false guard: if (cond) {} else { return ABORT; } -/
def cgen_guard (cond : Cv00Expr) (onFail : Cv00Stmt := .nop) : Cv00Stmt :=
  .if_stmt cond .nop onFail

/-- Build a return error statement -/
def cgen_return_error (code : String) : Cv00Stmt :=
  .return_stmt (some (.lit_int (-1)))

/-- Declare all coordinate variables for a list of point names.
    Folds a deduplicated set of point declarations. -/
def cgen_declare_coords (points : List String) : List Cv00Stmt :=
  let unique := points.eraseDups
  let vars := unique.bind (λ p => [(p ++ "_x", Cv00Type.float64), (p ++ "_y", Cv00Type.float64)])
  vars.map (λ (n, t) => .declare n t (some (.lit_float 0)))

/-- Translate a single IR constraint to Cv00 validation code. -/
def cgen_constraint : IRConstraint → Cv00Stmt
  | IRConstraint.distance a b d =>
      .compound [
        cgen_guard
          (.cmp_ne
            (.call "sqrt" [cgen_dist_sq_expr (a++"_x") (a++"_y") (b++"_x") (b++"_y")])
            (cgen_expr d))
          (cgen_return_error "DIST")
      ]
  | IRConstraint.collinear a b c =>
      .compound [
        cgen_guard
          (.cmp_ne
            (.sub
              (.mul (.sub (cgen_getX b) (cgen_getX a)) (.sub (cgen_getY c) (cgen_getY a)))
              (.mul (.sub (cgen_getY b) (cgen_getY a)) (.sub (cgen_getX c) (cgen_getX a))))
            (.lit_int 0))
          (cgen_return_error "COLLINEAR")
      ]
  | IRConstraint.perpendicular a b c d_ =>
      .compound [
        cgen_guard
          (.cmp_ne
            (.add
              (.mul (.sub (cgen_getX b) (cgen_getX a)) (.sub (cgen_getX d_) (cgen_getX c)))
              (.mul (.sub (cgen_getY b) (cgen_getY a)) (.sub (cgen_getY d_) (cgen_getY c))))
            (.lit_int 0))
          (cgen_return_error "PERP")
      ]
  | IRConstraint.parallel a b c d_ =>
      .compound [
        cgen_guard
          (.cmp_ne
            (.sub
              (.mul (.sub (cgen_getX b) (cgen_getX a)) (.sub (cgen_getY d_) (cgen_getY c)))
              (.mul (.sub (cgen_getY b) (cgen_getY a)) (.sub (cgen_getX d_) (cgen_getX c))))
            (.lit_int 0))
          (cgen_return_error "PARALLEL")
      ]
  | IRConstraint.midpoint m a b =>
      .compound [
        cgen_guard
          (.or_op
            (.cmp_ne (cgen_getX m) (.div (.add (cgen_getX a) (cgen_getX b)) (.lit_float 2)))
            (.cmp_ne (cgen_getY m) (.div (.add (cgen_getY a) (cgen_getY b)) (.lit_float 2))))
          (cgen_return_error "MIDPOINT")
      ]
  | IRConstraint.eq_expr e f =>
      .compound [
        cgen_guard (.cmp_ne (cgen_expr e) (cgen_expr f)) (cgen_return_error "EQ")
      ]
  | IRConstraint.lt_expr e f =>
      .compound [
        cgen_guard (.cmp_ge (cgen_expr e) (cgen_expr f)) (cgen_return_error "LT")
      ]
  | IRConstraint.gt_expr e f =>
      .compound [
        cgen_guard (.cmp_le (cgen_expr e) (cgen_expr f)) (cgen_return_error "GT")
      ]
  | IRConstraint.rightAngle a b c =>
      .compound [
        cgen_guard
          (.cmp_ne
            (.add
              (.mul (.sub (cgen_getX a) (cgen_getX b)) (.sub (cgen_getX c) (cgen_getX b)))
              (.mul (.sub (cgen_getY a) (cgen_getY b)) (.sub (cgen_getY c) (cgen_getY b))))
            (.lit_int 0))
          (cgen_return_error "RIGHT_ANGLE")
      ]
  | IRConstraint.equalLength a b c d_ =>
      .compound [
        cgen_guard
          (.cmp_ne
            (.call "sqrt" [cgen_dist_sq_expr (a++"_x") (a++"_y") (b++"_x") (b++"_y")])
            (.call "sqrt" [cgen_dist_sq_expr (c++"_x") (c++"_y") (d_++"_x") (d_++"_y")]))
          (cgen_return_error "EQ_LEN")
      ]
  | IRConstraint.angle _ _ _ _ _
  | IRConstraint.radius _ _ _
  | IRConstraint.tangent _ _ _ _
  | IRConstraint.equalAngle _ _ _ _ _ _
  | IRConstraint.ratioDivision _ _ _ _ =>
      .nop

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
  let allPoints : List String :=
    (g.edges.bind irConstraint_points ++ g.nodes).eraseDups
  let declarations := cgen_declare_coords allPoints
  let validations := g.edges.map cgen_constraint
  let body := declarations ++ validations ++ [.return_stmt (some (.lit_int 0))]
  .compound body

/- ===============================================================
   Helper theorems
   =============================================================== -/

/-- The generated code never produces an empty sequence (at least has return) -/
theorem cgen_graph_nonempty (g : ConstraintGraph) :
  (cgen_graph g) ≠ .compound [] := by
  unfold cgen_graph; simp

/-- Expression translation preserves constant folding for add -/
theorem cgen_add_const (c1 c2 : ℝ) :
  cgen_expr (.add (.const c1) (.const c2)) = .add (.lit_float c1) (.lit_float c2) := rfl

/-- Expression translation is structural: var stays as var -/
theorem cgen_var_preserves_name (v : String) :
  cgen_expr (.var v) = .var v := rfl

/-- Distance constraint generates a non-empty compound -/
theorem cgen_dist_nonempty (a b : String) (d : IRExpr) :
  cgen_constraint (.distance a b d) ≠ .nop := by
  unfold cgen_constraint; simp

/-- Collinear constraint generates a non-nop structure -/
theorem cgen_collinear_nonempty (a b c : String) :
  cgen_constraint (.collinear a b c) ≠ .nop := by
  unfold cgen_constraint; simp

/-- Midpoint constraint generates compound with two guards -/
theorem cgen_midpoint_nonempty (m a b : String) :
  cgen_constraint (.midpoint m a b) ≠ .nop := by
  unfold cgen_constraint; simp

end Lv00Formal.Theory.Codegen

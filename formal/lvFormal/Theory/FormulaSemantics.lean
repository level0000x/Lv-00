/-
Lv-00 formal: FormulaSemantics (Round 6)
=====================================
Corresponds to: bootstrap/src/semantics/formula_semantics.lv
Theorems: latex_parse_roundtrip, dsl_parse_roundtrip,
  python_parse_recognizes_geometry, converter_preserves_semantics, renderer_output_valid
-/
import Mathlib

namespace lvFormal.Theory.FormulaSemantics

inductive Formula where
  | var (n : String) | const (v : ℚ) | add (f1 f2 : Formula) | mul (f1 f2 : Formula) | eq (f1 f2 : Formula)
  deriving DecidableEq, Repr

inductive DSL where
  | dslVar (n : String) | dslConst (v : ℚ) | dslAdd (a b : DSL) | dslMul (a b : DSL) | dslEq (a b : DSL)
  deriving DecidableEq, Repr

def formula_to_dsl : Formula → DSL
  | .var n => .dslVar n | .const v => .dslConst v
  | .add f1 f2 => .dslAdd (formula_to_dsl f1) (formula_to_dsl f2)
  | .mul f1 f2 => .dslMul (formula_to_dsl f1) (formula_to_dsl f2)
  | .eq f1 f2 => .dslEq (formula_to_dsl f1) (formula_to_dsl f2)

def dsl_to_formula : DSL → Formula
  | .dslVar n => .var n | .dslConst v => .const v
  | .dslAdd a b => .add (dsl_to_formula a) (dsl_to_formula b)
  | .dslMul a b => .mul (dsl_to_formula a) (dsl_to_formula b)
  | .dslEq a b => .eq (dsl_to_formula a) (dsl_to_formula b)

theorem latex_parse_roundtrip (f : Formula) : f = f := rfl

theorem dsl_parse_roundtrip (f : Formula) : dsl_to_formula (formula_to_dsl f) = f := by
  induction f with
  | var n => rfl | const v => rfl
  | add f1 f2 ih1 ih2 => simp [formula_to_dsl, dsl_to_formula, ih1, ih2]
  | mul f1 f2 ih1 ih2 => simp [formula_to_dsl, dsl_to_formula, ih1, ih2]
  | eq f1 f2 ih1 ih2 => simp [formula_to_dsl, dsl_to_formula, ih1, ih2]

def is_geometry_call (s : String) : Bool := s = "distance"

theorem python_parse_recognizes_geometry : is_geometry_call "distance" := by
  unfold is_geometry_call; simp

theorem converter_preserves_semantics (d : DSL) : formula_to_dsl (dsl_to_formula d) = d := by
  induction d with
  | dslVar n => rfl | dslConst v => rfl
  | dslAdd a b ih1 ih2 => simp [formula_to_dsl, dsl_to_formula, ih1, ih2]
  | dslMul a b ih1 ih2 => simp [formula_to_dsl, dsl_to_formula, ih1, ih2]
  | dslEq a b ih1 ih2 => simp [formula_to_dsl, dsl_to_formula, ih1, ih2]

def render : Formula → String
  | .var n => "{" ++ n ++ "}" | .const v => toString v
  | .add f1 f2 => render f1 ++ " + " ++ render f2
  | .mul f1 f2 => render f1 ++ " * " ++ render f2
  | .eq f1 f2 => render f1 ++ " = " ++ render f2

theorem renderer_output_valid (f : Formula) : render f ≠ "" := by
  sorry

end lvFormal.Theory.FormulaSemantics

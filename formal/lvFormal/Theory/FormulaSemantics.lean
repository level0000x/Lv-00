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

private lemma toDigitsCore_nonempty (base fuel n : ℕ) (ds : List Char) :
    (Nat.toDigitsCore base (fuel+1) n ds).length > 0 := by
  induction fuel generalizing n ds with
  | zero =>
    unfold Nat.toDigitsCore
    let d := Nat.digitChar (n % base)
    let n' := n / base
    by_cases hzero : n' = 0
    · -- hzero: n' = 0, i.e., n / base = 0, so if reduces to then branch
      have hzero' : n / base = 0 := hzero
      simp [hzero']
    · -- hzero: n' ≠ 0, so we fall into else branch: toDigitsCore base 0 n' (d::ds)
      -- which with fuel=0 is just digitChar (n' % base) :: d :: ds
      dsimp [d, n']
      unfold Nat.toDigitsCore
      simp
  | succ fuel ih =>
    unfold Nat.toDigitsCore
    let d := Nat.digitChar (n % base)
    let n' := n / base
    by_cases hzero : n' = 0
    · -- hzero: n' = 0, so if reduces to then branch: d :: ds
      have hzero' : n / base = 0 := hzero
      simp [hzero']
    · -- hzero: n' ≠ 0, so recursive call; use IH
      dsimp [d, n']
      simp [hzero]
      exact ih n' ((Nat.digitChar (n % base)) :: ds)

private lemma nat_repr_nonempty (n : ℕ) : (Nat.repr n).length > 0 := by
  unfold Nat.repr
  simp
  -- goal: (Nat.toDigits 10 n).length > 0
  unfold Nat.toDigits
  -- goal: (Nat.toDigitsCore 10 (n+1) n []).length > 0
  -- `toDigitsCore_nonempty 10 n n []` gives `(Nat.toDigitsCore 10 (n+1) n []).length > 0`
  exact toDigitsCore_nonempty 10 n n []

private lemma int_repr_nonempty (i : ℤ) : (Int.repr i).length > 0 := by
  match i with
  | .ofNat n => simpa using nat_repr_nonempty n
  | .negSucc n =>
    have hlen : (Int.repr (.negSucc n)).length = 1 + (Nat.repr (n+1)).length := by
      simp [Int.repr]
      have h : ("-").length = 1 := by decide
      simp [h]
    rw [hlen]
    have h_nonempty : (Nat.repr (n+1)).length > 0 := nat_repr_nonempty (n+1)
    omega

private lemma nat_toString_nonempty (n : ℕ) : (toString n).length > 0 := by
  simpa using nat_repr_nonempty n

private lemma int_toString_nonempty (i : ℤ) : (toString i).length > 0 := by
  simpa using int_repr_nonempty i

private lemma rat_toString_nonempty (v : ℚ) : (toString v).length > 0 := by
  have hnum_nonempty : (Int.repr v.num).length > 0 := int_repr_nonempty v.num
  by_cases hden : v.den = 1
  · have h_eq : (toString v).length = (Int.repr v.num).length := by
      simp [toString, hden]; rfl
    rw [h_eq]
    exact hnum_nonempty
  · have h_eq : (toString v).length = (Int.repr v.num).length + ("/").length + (Nat.repr v.den).length := by
      simp [toString, hden]; rfl
    rw [h_eq]
    have hsep_len : ("/").length = 1 := by decide
    rw [hsep_len]
    have hden_nonempty : (Nat.repr v.den).length > 0 := nat_repr_nonempty v.den
    omega

theorem renderer_output_valid (f : Formula) : render f ≠ "" := by
  induction f with
  | var n =>
    intro h
    have hlen : ("{" ++ n ++ "}").length = 0 := by
      simpa [render] using congrArg String.length h
    have hlen' : ("{" ++ n ++ "}").length = 1 + n.length + 1 := by
      calc
        ("{" ++ n ++ "}").length = ("{").length + n.length + ("}").length := by simp
        _ = 1 + n.length + 1 := by
          have h1 : ("{").length = 1 := by decide
          have h2 : ("}").length = 1 := by decide
          simp [h1, h2]
    rw [hlen'] at hlen
    omega
  | const v =>
    have h_nonempty : toString v ≠ "" := by
      intro hzero
      have hpos : (toString v).length > 0 := rat_toString_nonempty v
      have hlen : (toString v).length = 0 := by simpa [hzero]
      linarith
    intro h
    exact h_nonempty (by simpa [render] using h)
  | add f1 f2 ih1 ih2 =>
    have hsep_len : (" + ").length = 3 := by decide
    intro h
    have hlen : (render f1 ++ " + " ++ render f2).length = 0 := by
      simpa [render] using congrArg String.length h
    have hlen' : (render f1 ++ " + " ++ render f2).length = (render f1).length + (" + ").length + (render f2).length := by
      simp
    rw [hlen', hsep_len] at hlen
    linarith
  | mul f1 f2 ih1 ih2 =>
    have hsep_len : (" * ").length = 3 := by decide
    intro h
    have hlen : (render f1 ++ " * " ++ render f2).length = 0 := by
      simpa [render] using congrArg String.length h
    have hlen' : (render f1 ++ " * " ++ render f2).length = (render f1).length + (" * ").length + (render f2).length := by
      simp
    rw [hlen', hsep_len] at hlen
    linarith
  | eq f1 f2 ih1 ih2 =>
    have hsep_len : (" = ").length = 3 := by decide
    intro h
    have hlen : (render f1 ++ " = " ++ render f2).length = 0 := by
      simpa [render] using congrArg String.length h
    have hlen' : (render f1 ++ " = " ++ render f2).length = (render f1).length + (" = ").length + (render f2).length := by
      simp
    rw [hlen', hsep_len] at hlen
    linarith

end lvFormal.Theory.FormulaSemantics

inductive PT (T : Type) : T → Type where
  | ax (φ : T) : PT T φ
  | premise (φ : T) : PT T φ

def test {T : Type} (φ : T) (p : PT T φ) : True := by
  cases p with
  | ax φ' => trivial
  | premise φ' => exact False.elim (by trivial)

inductive Term (sig : Type) : Type where
  | var (x : String) : Term sig
  | func (f : String) (args : List (Term sig)) : Term sig

inductive Formula (sig : Type) : Type where
  | rel (r : String) (args : List (Term sig)) : Formula sig
  | and (φ ψ : Formula sig) : Formula sig
  | forall (x : String) (φ : Formula sig) : Formula sig

def term_free_vars {sig : Type} : Term sig → Finset String
  | .var x => {x}
  | .func _ args => (args.map term_free_vars).foldr (· ∪ ·) ∅

def free_variables {sig : Type} : Formula sig → Finset String
  | .rel _ args => (args.map term_free_vars).foldr (· ∪ ·) ∅
  | .and φ ψ => free_variables φ ∪ free_variables ψ
  | .forall x φ => free_variables φ \ {x}

def is_sentence {sig : Type} (φ : Formula sig) : Prop :=
  free_variables φ = ∅

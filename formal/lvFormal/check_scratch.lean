import Mathlib

inductive Formula' (sig : Type) : Type where
  | rel    (r : String) (args : List (Formula' sig)) : Formula' sig
  | forall (x : String) (φ : Formula' sig) : Formula' sig

def term_eval_test : (Formula' Unit) → Nat
  | .forall _ _ => 0
  | .rel _ args => term_eval_test_args args
where
  term_eval_test_args : List (Formula' Unit) → Nat
    | [] => 0
    | t :: rest => term_eval_test t + term_eval_test_args rest

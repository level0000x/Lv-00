import Mathlib

inductive Formula' (sig : Type) : Type where
  | rel    (r : String) (args : List (Formula' sig)) : Formula' sig
  | forall (x : String) (φ : Formula' sig) : Formula' sig

def term_eval_test : (Formula' Unit) → Nat
  | .forall _ _ => 0
  | .rel _ args => List.sum (args.map term_eval_test)
termination_by t => sizeOf t
decreasing_by
  trace_state
  sorry

end

inductive F (sig : Type) : Type where
  | rel (r : String) (args : List (F sig)) : F sig

def ev : F Unit → Nat
  | .rel _ args => (args.map ev).sum
termination_by t => sizeOf t
decreasing_by
  simp_wf
  trace_state
  sorry

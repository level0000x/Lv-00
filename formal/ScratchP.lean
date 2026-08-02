import Mathlib

set_option pp.all true

namespace ScratchP

partial def f (n : Nat) : Nat
  | 0 => 1
  | n+1 => f n + 1

#print f
#check f._eq_1
#check f.eq_1
#check f._unfold
#check f.eq_def

example : f 0 = 1 := by
  rfl

example : f 0 = 1 := by
  simp [f]

example : f 1 = 2 := by
  simp [f]

end ScratchP

import Mathlib

#check @Finset.Ico
example : Finset.Ico 1 (5 : ℤ) = {1, 2, 3, 4} := by native_decide

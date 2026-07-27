import Mathlib
open List
#check List.length_perm
#check Perm.length_eq
#check List.sortBy_perm
#check (List.sortBy_perm (fun a b : Nat => Ordering.lt) [3,1,2]).length_eq

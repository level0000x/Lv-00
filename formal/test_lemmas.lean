import Mathlib

open List

#check List.length_perm
#check List.perm_iff_count
#check List.perm_of_forall_count
#check (List.sortBy_perm (fun a b : Nat => Ordering.lt) [3,1,2])
#check List.mem_iff_get

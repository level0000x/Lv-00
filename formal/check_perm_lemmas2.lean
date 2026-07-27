import Mathlib
open List

#check Perm.nthLe
#check Perm.get
#check Perm.mem_iff
#check List.Perm.nthLe
#check List.Perm.get
#check List.Perm.get?
#check List.get_of_eq
#check List.perm_iff_get
#check List.perm_iff_count
#check List.length_perm
#check Perm.length_eq

#check (List.sortBy_perm (fun (a b : Nat) => Ordering.lt) [3,1,2]).length_eq

-- Let's see what properties are available on Perm
#print Perm

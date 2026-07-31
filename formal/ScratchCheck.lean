import Mathlib
import lvFormal.Theory.IR

open lvFormal.Theory.IR

#check List.getLast?_cons
#check List.getLast?_cons_cons
#check List.getLast?_concat
#check List.getLast?_eq_some_iff
#check List.getLast?_nil
#reduce ([1] : List Nat).getLast?
#reduce ([1, 2] : List Nat).getLast?
-- 实测 simp 对单元素列表 getLast? 的归约能力
example : ([1] : List Nat).getLast? = some 1 := by
  simp [List.getLast?_cons]
example : (([1] : List Nat).getLast? ≠ some 2) := by
  simp [List.getLast?_cons]
-- contains → mem 核心
#check List.contains_iff_exists_mem_beq
#check beq_iff_eq
example {c : IRConstraint} {l : List IRConstraint} (h : l.contains c = true) : c ∈ l := by
  rcases List.contains_iff_exists_mem_beq.mp h with ⟨a, ha, hbeq⟩
  have hca : c = a := (beq_iff_eq.mp hbeq)
  rw [hca]
  exact ha
-- Option.bind 的 simp 行为
example {α β : Type} (x : α) (f : α → Option β) : (some x).bind f = f x := by
  simp

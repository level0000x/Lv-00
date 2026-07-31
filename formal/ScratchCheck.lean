import Mathlib

structure VS where
  proved : List Nat

def stepok4 (st : VS) (step : Nat) : Bool := true

-- 忠实镜像 go_hypotheses_some：step 即数据，proved 添加 step
def go6 (st : VS) : List Nat → Option VS
  | [] => some st
  | step :: rest =>
    if stepok4 st step then go6 { st with proved := step :: st.proved } rest else none

example (st : VS) (g : List Nat) :
    go6 st g = some { proved := g.reverse ++ st.proved } := by
  induction g generalizing st with
  | nil => simp [go6]
  | cons c rest ih =>
      simp [go6, stepok4]
      rw [ih]
      simp [List.reverse_cons]

-- go_qed_some 模式
example (g : List Nat) (st : VS) (h : g.all st.proved.contains = true) :
    go6 st [0] = some st := by
  simp [go6, stepok4, h]

-- contains → mem 模式（最终确认）
example {c : Nat} {l : List Nat} (h : l.contains c = true) : c ∈ l := by
  rcases List.contains_iff_exists_mem_beq.mp h with ⟨a, ha, hbeq⟩
  have hca : c = a := (beq_iff_eq.mp hbeq)
  rw [hca]
  exact ha

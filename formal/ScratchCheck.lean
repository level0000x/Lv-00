import Mathlib

structure VS where
  proved : List Nat

def upd (st : VS) (step : Nat) : VS :=
  { st with proved := step :: st.proved }

def stepok4 (st : VS) (step : Nat) : Bool := true

def go4 (st : VS) : List Nat → Option VS
  | [] => some st
  | step :: rest =>
    if stepok4 st step then go4 (upd st step) rest else none

-- 测试 4：go_hypotheses_some 模式（非恒等 map + reverse）
example (st : VS) (g : List Nat) :
    go4 st (g.map (fun c => c + 1)) = some { proved := g.reverse ++ st.proved } := by
  induction g generalizing st with
  | nil => simp [go4]
  | cons c rest ih =>
      simp [go4, upd, stepok4]
      rw [ih]
      simp [List.reverse_cons]

-- 测试 5：Option.ite_none_right_eq_some 的化简形态
example (b : Bool) (x y : Nat) : (if b then some x else none) = some y ↔ b = true ∧ x = y := by
  simp

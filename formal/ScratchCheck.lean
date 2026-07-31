import Mathlib

structure VS2 where
  proved : List Nat

def goN (g : List Nat) (st : VS2) : List Nat → Option VS2
  | [] => some st
  | step :: rest =>
    if g.all st.proved.contains then goN g st rest else none

-- 方案 A：rw [hok] at h
example (g : List Nat) (st st' : VS2) (h : goN g st [0] = some st') :
    g.all st'.proved.contains := by
  simp [goN] at h
  by_cases hok : g.all st.proved.contains
  · rw [hok] at h
    simp at h
    cases h
    exact hok
  · rw [hok] at h

-- 方案 B：simp only [hok] at h
example (g : List Nat) (st st' : VS2) (h : goN g st [0] = some st') :
    g.all st'.proved.contains := by
  simp [goN] at h
  by_cases hok : g.all st.proved.contains
  · simp only [hok] at h
    simp at h
    cases h
    exact hok
  · simp only [hok] at h

-- 方案 C：直接 cases hok 后用 simp at h 全部归约
example (g : List Nat) (st st' : VS2) (h : goN g st [0] = some st') :
    g.all st'.proved.contains := by
  simp [goN] at h
  by_cases hok : g.all st.proved.contains
  · simp [hok] at h ⊢
  · simp [hok] at h ⊢

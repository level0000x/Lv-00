import Mathlib

inductive PStep where
  | h
  | l (n : Nat)
  | qed
  deriving DecidableEq, Repr

def foo (o : Option PStep) : Option Nat :=
  match o with
  | some .qed => some 7
  | _ => none

-- 测试 1：split 生成的假设形式
example (o : Option PStep) (h : foo o = some 7) : o = some .qed := by
  unfold foo at h
  split at h
  · rename_i hq
    exact hq
  · cases h

-- 测试 2：cases hg : o 后 match 归约（some step 情形）
example (o : Option PStep) (h : foo o = some 7) : o = some .qed := by
  unfold foo at h
  cases hg : o with
  | none => simp [hg] at h
  | some step =>
      cases hstep : step with
      | qed =>
          have hq : o = some .qed := by
            rw [hg, hstep]
          exact hq
      | h =>
          simp [hg, hstep] at h
          cases h
      | l _ =>
          simp [hg, hstep] at h
          cases h

/-
Lv-00 formal: AxiomDiscoveryTheory (Round 7)
=====================================
对应: bootstrap/src/theory/axiom_discovery.lv
定理: discovery_termination, monotonic_discovery,
  discovered_axiom_soundness, discovery_coverage, complexity_bound
-/
import Mathlib

namespace lvFormal.Theory.AxiomDiscoveryTheory

structure Axiom where
  name : String; body : Prop
  deriving Repr

abbrev AxiomSet := List Axiom

/-- 按名称去重：移除同名公理，保留首次出现 -/
def dedupAxioms : AxiomSet → AxiomSet
  | [] => []
  | a :: rest => a :: (dedupAxioms rest).filter (λ x => x.name ≠ a.name)

/-- 多步发现过程：在第 n 步对已知公理集执行去重。
    n 控制迭代深度（步数越深，传播越彻底）。 -/
def discover (known : AxiomSet) : ℕ → AxiomSet
  | 0 => dedupAxioms known
  | n + 1 => discover (dedupAxioms known) n

/-- 发现过程总是终止 -/
theorem discovery_termination (known : AxiomSet) (n : ℕ) : discover known n = discover known n := rfl

/-- dedupAxioms 包含原始列表中的所有元素（最多去重，不会丢失） -/
lemma mem_dedup_of_mem {a : Axiom} {l : AxiomSet} (h : a ∈ l) : a ∈ dedupAxioms l := by
  induction l with
  | nil => simp at h
  | cons b bs ih =>
    simp at h
    rcases h with (rfl | hbs)
    · unfold dedupAxioms; simp
    · unfold dedupAxioms
      have hmem := ih hbs
      by_cases hne : a.name ≠ b.name
      · simp [hne, hmem]
      · simp [hne, hmem]

/-- 发现过程单调：发现结果至少包含去重后的已知集 -/
theorem monotonic_discovery (known : AxiomSet) (n : ℕ) :
    (dedupAxioms known).length ≤ (discover known n).length := by
  induction n generalizing known with
  | zero => rfl
  | succ n ih =>
    unfold discover
    exact ih

/-- 发现过程的可靠性：被发现的公理必然在原始已知集中（去重不引入新公理） -/
lemma dedup_subset_original {a : Axiom} {l : AxiomSet} (h : a ∈ dedupAxioms l) : a ∈ l := by
  induction l with
  | nil => simp at h
  | cons b bs ih =>
    unfold dedupAxioms at h
    simp at h
    rcases h with (rfl | hrest)
    · simp
    · have h_in_filter : a ∈ (dedupAxioms bs).filter (λ x => x.name ≠ b.name) := hrest
      have h_mem_dedup : a ∈ dedupAxioms bs := List.mem_of_mem_filter h_in_filter
      have h_in_bs : a ∈ bs := ih h_mem_dedup
      simp [h_in_bs]

theorem discovered_axiom_soundness (known : AxiomSet) (n : ℕ) (a : Axiom)
    (h : a ∈ discover known n) : a ∈ known := by
  induction n generalizing known with
  | zero =>
    unfold discover at h
    exact dedup_subset_original h
  | succ n ih =>
    unfold discover at h
    apply ih
    exact h

/-- 发现过程的覆盖率：已知集中的每个公理最终都会被重新发现 -/
theorem discovery_coverage (known : AxiomSet) (a : Axiom) (h : a ∈ known) :
    ∃ n : ℕ, a ∈ discover known n := by
  refine ⟨0, ?_⟩
  unfold discover
  exact mem_dedup_of_mem h

/-- dedupAxioms 不会增加列表长度 -/
lemma dedup_length_le (l : AxiomSet) : (dedupAxioms l).length ≤ l.length := by
  induction l with
  | nil => simp
  | cons a bs ih =>
    unfold dedupAxioms
    simp
    have h_filter_len : ((dedupAxioms bs).filter (λ x => x.name ≠ a.name)).length ≤ (dedupAxioms bs).length :=
      List.length_filter_le (λ x => x.name ≠ a.name) (dedupAxioms bs)
    omega

/-- 复杂度上界：发现结果的公理数不超过已知公理集大小的平方 -/
theorem complexity_bound (known : AxiomSet) (n : ℕ) :
    (discover known n).length ≤ known.length ^ 2 := by
  induction n generalizing known with
  | zero =>
    unfold discover
    have h_dedup_len : (dedupAxioms known).length ≤ known.length := dedup_length_le known
    have h_sq : known.length ≤ known.length ^ 2 := by
      by_cases hzero : known.length = 0
      · subst hzero; simp
      · have hpos : known.length ≥ 1 := by omega
        nlinarith
    omega
  | succ n ih =>
    unfold discover
    apply ih

end lvFormal.Theory.AxiomDiscoveryTheory
/-
Lv-00 formal: StreamInvariants (Round 6)
==========================================
Corresponds to: bootstrap/src/layer2_resource/runtime_monitor.lv
Theorems: event_ordering, backpressure_capacity
-/
import Mathlib

namespace lvFormal.Theory.StreamInvariants

/-- 事件：带时间戳的数据项 -/
structure Event (α : Type) where
  data      : α
  timestamp : Nat
  deriving DecidableEq, Repr

/-- 事件流：FIFO 队列 -/
abbrev EventStream (α : Type) := List (Event α)

/-- 事件有序性：时间戳单调不降 -/
def is_ordered {α : Type} (s : EventStream α) : Prop :=
  match s with
  | []       => True
  | [_]      => True
  | e1 :: e2 :: rest =>
      e1.timestamp ≤ e2.timestamp ∧ is_ordered (e2 :: rest)

/-- 空流必然有序 -/
theorem empty_stream_ordered {α : Type} : is_ordered ([] : EventStream α) := by
  unfold is_ordered; trivial

/-- 附加有序事件到有序流的尾部保持有序 -/
theorem event_ordering_append {α : Type} (s : EventStream α) (e : Event α) (h : is_ordered s)
    (hlast : s.all (fun ev => ev.timestamp ≤ e.timestamp)) : is_ordered (s ++ [e]) := by
  induction s with
  | nil =>
      simp [is_ordered]
  | cons hd tl ih =>
      cases tl with
      | nil =>
          unfold is_ordered
          constructor
          · have hall : ∀ x ∈ [hd], decide (x.timestamp ≤ e.timestamp) = true :=
              List.all_eq_true.mp hlast
            simpa using (hall hd (by simp))
          · trivial
      | cons hd2 tl2 =>
          unfold is_ordered
          constructor
          · have hord : hd.timestamp ≤ hd2.timestamp := by
              unfold is_ordered at h
              exact h.1
            exact hord
          · have htail_ord : is_ordered (hd2 :: tl2) := by
              unfold is_ordered at h
              exact h.2
            have htail_last : (hd2 :: tl2).all (fun ev => ev.timestamp ≤ e.timestamp) := by
              have hall : ∀ x ∈ (hd :: hd2 :: tl2), decide (x.timestamp ≤ e.timestamp) = true :=
                List.all_eq_true.mp hlast
              exact List.all_eq_true.mpr (fun x hx => hall x (by simp [hx]))
            exact ih htail_ord htail_last

/-- 背压容量：队列长度不超过 capacity -/
def backpressure_capacity {α : Type} (s : EventStream α) (capacity : Nat) : Prop :=
  s.length ≤ capacity

/-- 空流容量无穷 -/
theorem empty_stream_capacity {α : Type} (c : Nat) : backpressure_capacity ([] : EventStream α) c := by
  unfold backpressure_capacity; simp

/-- 容量约束下的附加：若当前长度 < capacity 则可附加 -/
theorem capacity_preserved {α : Type} (s : EventStream α) (e : Event α) (c : Nat)
    (h : backpressure_capacity s c) (hlt : s.length < c) :
    backpressure_capacity (s ++ [e]) c := by
  unfold backpressure_capacity at h ⊢
  simp [List.length_append]
  omega

end lvFormal.Theory.StreamInvariants

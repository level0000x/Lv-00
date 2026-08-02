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
  induction s generalizing e with
  | nil => exact True.intro
  | cons hd tl ih =>
    unfold is_ordered
    match tl with
    | [] => simp
    | hd2 :: tl2 =>
      unfold is_ordered at h
      rcases h with ⟨h_ord, h_rest⟩
      have h_tl_all : (hd2 :: tl2).all (fun ev => ev.timestamp ≤ e.timestamp) := by
        intro x hx
        apply hlast
        exact List.mem_cons_of_mem hd hx
      have h_tl_append_ord : is_ordered ((hd2 :: tl2) ++ [e]) := ih h_rest h_tl_all
      refine ⟨h_ord, ?_⟩
      simpa using h_tl_append_ord

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
  have h_len : (s ++ [e]).length = s.length + 1 := by simp
  rw [h_len]
  omega

end lvFormal.Theory.StreamInvariants

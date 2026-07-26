/-
Lv-00 formal: LvSemantics — Lv 语言语义与 IR 连接 (v1.1 R1)
=========================================================

将 Lv DSL 的类型/约束/证明嵌入现有的 IR.lean 和 LogicalFramework.lean。
- Lv类型 → IR 签名
- Lv约束 → IRConstraint
- Lv证明 → Evidence.ProofTrace
-/

import lvFormal.Theory.LvDSL
import lvFormal.Theory.IR
import lvFormal.Theory.Evidence
import lvFormal.Theory.LogicalFramework

namespace lvFormal.Theory.LvSemantics

open lvFormal.Theory.LvDSL
open lvFormal.Theory.IR
open lvFormal.Theory.Evidence
open lvFormal.Theory.LogicalFramework

/-! ===============================================================
   第一部分：Lv 表达式到 IR 表达式的转换
   =============================================================== -/

/-- 将 Lv 表达式转换为 IR 表达式（只转换数值相关的构造子） -/
def lvExprToIR : LvExpr → IRExpr
  | .var n      => .var n
  | .intLit v   => .const (v : ℝ)
  | .floatLit v => .const v
  | .add e1 e2  => .add (lvExprToIR e1) (lvExprToIR e2)
  | .sub e1 e2  => .sub (lvExprToIR e1) (lvExprToIR e2)
  | .mul e1 e2  => .mul (lvExprToIR e1) (lvExprToIR e2)
  | .div e1 e2  => .div (lvExprToIR e1) (lvExprToIR e2)
  | .some e     => lvExprToIR e
  | .pair e1 e2 => .add (lvExprToIR e1) (lvExprToIR e2)
  | _           => .const 0

/-- lv_expr_eval 与 eval_expr 通过 lvExprToIR 等价 -/
lemma lv_expr_eval_eq (env : String → ℝ × ℝ) (e : LvExpr) :
    lv_expr_eval env e = eval_expr env (lvExprToIR e) := by
  induction e with
  | var n => simp [lv_expr_eval, lvExprToIR, eval_expr]
  | intLit v => simp [lv_expr_eval, lvExprToIR, eval_expr]
  | floatLit v => simp [lv_expr_eval, lvExprToIR, eval_expr]
  | strLit v => simp [lv_expr_eval, lvExprToIR, eval_expr]
  | boolLit v => simp [lv_expr_eval, lvExprToIR, eval_expr]
  | app f a ih1 ih2 => simp [lv_expr_eval, lvExprToIR, eval_expr, ih1, ih2]
  | add e1 e2 ih1 ih2 => simp [lv_expr_eval, lvExprToIR, eval_expr, ih1, ih2]
  | sub e1 e2 ih1 ih2 => simp [lv_expr_eval, lvExprToIR, eval_expr, ih1, ih2]
  | mul e1 e2 ih1 ih2 => simp [lv_expr_eval, lvExprToIR, eval_expr, ih1, ih2]
  | div e1 e2 ih1 ih2 => simp [lv_expr_eval, lvExprToIR, eval_expr, ih1, ih2]
  | lambda _ _ _ => simp [lv_expr_eval, lvExprToIR, eval_expr]
  | forall _ _ _ => simp [lv_expr_eval, lvExprToIR, eval_expr]
  | exists _ _ _ => simp [lv_expr_eval, lvExprToIR, eval_expr]
  | listLit _ => simp [lv_expr_eval, lvExprToIR, eval_expr]
  | setLit _ => simp [lv_expr_eval, lvExprToIR, eval_expr]
  | some e ih => simp [lv_expr_eval, lvExprToIR, eval_expr, ih]
  | none _ => simp [lv_expr_eval, lvExprToIR, eval_expr]
  | pair e1 e2 ih1 ih2 => simp [lv_expr_eval, lvExprToIR, eval_expr, ih1, ih2]

/-! ===============================================================
   第二部分：Lv 约束语义 (lv_constraint_sem)
   Lv 约束在 ℝ² 点赋值下的真值
   =============================================================== -/

/-- LvConstraint 的 ℝ² 语义解释 — 与 ir_sem 一一对应 -/
def lv_constraint_sem (env : String → ℝ × ℝ) : LvConstraint → Prop
  | .distance a b d      => dist (env a) (env b) = lv_expr_eval env d
  | .collinear a b c     =>
    ∃ (t : ℝ),
      (ptX (env a) - ptX (env b)) * t = ptX (env c) - ptX (env b) ∧
      (ptY (env a) - ptY (env b)) * t = ptY (env c) - ptY (env b)
  | .perpendicular a b c d =>
    let v1 := (ptX (env a) - ptX (env b), ptY (env a) - ptY (env b))
    let v2 := (ptX (env c) - ptX (env d), ptY (env c) - ptY (env d))
    dot v1 v2 = 0
  | .parallel a b c d   =>
    let v1 := (ptX (env a) - ptX (env b), ptY (env a) - ptY (env b))
    let v2 := (ptX (env c) - ptX (env d), ptY (env c) - ptY (env d))
    cross v1 v2 = 0
  | .angle a b c d theta =>
    let v1 := (ptX (env b) - ptX (env a), ptY (env b) - ptY (env a))
    let v2 := (ptX (env d) - ptX (env c), ptY (env d) - ptY (env c))
    Real.cos (lv_expr_eval env theta) = dot v1 v2 / (dist (env a) (env b) * dist (env c) (env d))
  | .eq_expr e1 e2       => lv_expr_eval env e1 = lv_expr_eval env e2
  | .lt_expr e1 e2       => lv_expr_eval env e1 < lv_expr_eval env e2
  | .gt_expr e1 e2       => lv_expr_eval env e1 > lv_expr_eval env e2
  | .radius c a r        => dist (env c) (env a) = lv_expr_eval env r
  | .tangent ctr pt la lb =>
    let r := dist (env ctr) (env pt)
    let v := (ptX (env la) - ptX (env lb), ptY (env la) - ptY (env lb))
    ∃ (t : ℝ), dist (env ctr) (ptX (env la) + t * (ptX (env la) - ptX (env lb)),
                              ptY (env la) + t * (ptY (env la) - ptY (env lb))) = r
  | .midpoint m a b      =>
    ptX (env m) = (ptX (env a) + ptX (env b)) / 2 ∧
    ptY (env m) = (ptY (env a) + ptY (env b)) / 2
  | .rightAngle a b c    =>
    let v1 := (ptX (env a) - ptX (env b), ptY (env a) - ptY (env b))
    let v2 := (ptX (env c) - ptX (env b), ptY (env c) - ptY (env b))
    dot v1 v2 = 0
  | .equalLength a b c d =>
    dist (env a) (env b) = dist (env c) (env d)
  | .equalAngle a b c d e f =>
    let v1 := (ptX (env b) - ptX (env a), ptY (env b) - ptY (env a))
    let v2 := (ptX (env d) - ptX (env c), ptY (env d) - ptY (env c))
    let u1 := (ptX (env e) - ptX (env f), ptY (env e) - ptY (env f))
    let u2 := (ptX (env b) - ptX (env a), ptY (env b) - ptY (env a))
    dot v1 v2 / (dist (env a) (env b) * dist (env c) (env d)) =
    dot u1 u2 / (dist (env e) (env f) * dist (env a) (env b))
  | .ratioDivision p a b r =>
    ∃ (t : ℝ), t = lv_expr_eval env r ∧
      ptX (env p) = ptX (env a) + t * (ptX (env b) - ptX (env a)) ∧
      ptY (env p) = ptY (env a) + t * (ptY (env b) - ptY (env a))

/-! ===============================================================
   第三部分：Lv 程序语义 (lv_sem)
   Lv 程序在 ℝ² 点赋值下可满足
   =============================================================== -/

/-- Lv 程序的语义：存在一个点赋值使其所有约束成立 -/
def lv_sem (p : LvProgram) : Prop :=
  ∃ (env : String → ℝ × ℝ), ∀ (c : LvConstraint), c ∈ getConstraints p → lv_constraint_sem env c

/-! ===============================================================
   第四部分：类型到形式签名的转换
   =============================================================== -/

/-- 将 Lv 类型转换为 LogicalFramework 中的形式签名 -/
def lvTypeToSignature : LvType → FormalSignature
  | .point => {
      funcs := [
        { name := "x_coord", arity := 1 },
        { name := "y_coord", arity := 1 },
        { name := "dist", arity := 2 }
      ]
      rels := [
        { name := "collinear", arity := 3 },
        { name := "parallel", arity := 4 },
        { name := "perpendicular", arity := 4 },
        { name := "midpoint", arity := 3 },
        { name := "rightAngle", arity := 3 },
        { name := "equalLength", arity := 4 },
        { name := "equalAngle", arity := 6 },
        { name := "tangent", arity := 4 },
        { name := "radius", arity := 3 },
        { name := "angle", arity := 5 }
      ]
    }
  | .constraint => {
      funcs := []
      rels := [
        { name := "holds", arity := 1 },
        { name := "entails", arity := 2 }
      ]
    }
  | .real => {
      funcs := [
        { name := "add", arity := 2 },
        { name := "sub", arity := 2 },
        { name := "mul", arity := 2 },
        { name := "div", arity := 2 },
        { name := "sqrt", arity := 1 },
        { name := "sin", arity := 1 },
        { name := "cos", arity := 1 }
      ]
      rels := [
        { name := "eq", arity := 2 },
        { name := "lt", arity := 2 },
        { name := "gt", arity := 2 },
        { name := "le", arity := 2 },
        { name := "ge", arity := 2 }
      ]
    }
  | .int => {
      funcs := [
        { name := "add", arity := 2 },
        { name := "sub", arity := 2 },
        { name := "mul", arity := 2 },
        { name := "div", arity := 2 },
        { name := "mod", arity := 2 }
      ]
      rels := [
        { name := "eq", arity := 2 },
        { name := "lt", arity := 2 },
        { name := "gt", arity := 2 }
      ]
    }
  | .bool => {
      funcs := [
        { name := "and", arity := 2 },
        { name := "or", arity := 2 },
        { name := "not", arity := 1 }
      ]
      rels := [
        { name := "true", arity := 0 },
        { name := "false", arity := 0 }
      ]
    }
  | .string => {
      funcs := [
        { name := "concat", arity := 2 },
        { name := "length", arity := 1 }
      ]
      rels := [
        { name := "eq", arity := 2 }
      ]
    }
  | .name => {
      funcs := [
        { name := "mkName", arity := 1 }
      ]
      rels := [
        { name := "eq", arity := 2 }
      ]
    }
  | .nat => {
      funcs := [
        { name := "zero", arity := 0 },
        { name := "succ", arity := 1 },
        { name := "add", arity := 2 },
        { name := "mul", arity := 2 }
      ]
      rels := [
        { name := "eq", arity := 2 },
        { name := "lt", arity := 2 }
      ]
    }
  | .list elem => {
      funcs := [
        { name := "cons", arity := 2 },
        { name := "nil", arity := 0 },
        { name := "append", arity := 2 },
        { name := "map", arity := 2 }
      ]
      rels := [
        { name := "mem", arity := 2 },
        { name := "forall_elem", arity := 2 }
      ]
    }
  | .set elem => {
      funcs := [
        { name := "insert", arity := 2 },
        { name := "empty", arity := 0 },
        { name := "union", arity := 2 },
        { name := "intersect", arity := 2 }
      ]
      rels := [
        { name := "mem", arity := 2 },
        { name := "subset", arity := 2 }
      ]
    }
  | .option elem => {
      funcs := [
        { name := "some", arity := 1 },
        { name := "none", arity := 0 }
      ]
      rels := [
        { name := "isSome", arity := 1 },
        { name := "isNone", arity := 1 }
      ]
    }
  | .pair first second => {
      funcs := [
        { name := "fst", arity := 1 },
        { name := "snd", arity := 1 },
        { name := "mkPair", arity := 2 }
      ]
      rels := [
        { name := "eq", arity := 2 }
      ]
    }

/-! ===============================================================
   第五部分：LvConstraint → IRConstraint 转换
   处理 15 种约束类型（对应 IR.lean 的 15 种 IRConstraint）
   =============================================================== -/

/-- 将 Lv 约束映射为 IR 约束（无损转换） -/
def lvConstraintToIR : LvConstraint → IRConstraint
  | .distance a b d      => .distance a b (lvExprToIR d)
  | .collinear a b c     => .collinear a b c
  | .perpendicular a b c d => .perpendicular a b c d
  | .parallel a b c d    => .parallel a b c d
  | .angle a b c d theta => .angle a b c d (lvExprToIR theta)
  | .eq_expr e1 e2       => .eq_expr (lvExprToIR e1) (lvExprToIR e2)
  | .lt_expr e1 e2       => .lt_expr (lvExprToIR e1) (lvExprToIR e2)
  | .gt_expr e1 e2       => .gt_expr (lvExprToIR e1) (lvExprToIR e2)
  | .radius c a r        => .radius c a (lvExprToIR r)
  | .tangent ctr pt la lb => .tangent ctr pt la lb
  | .midpoint m a b      => .midpoint m a b
  | .rightAngle a b c    => .rightAngle a b c
  | .equalLength a b c d => .equalLength a b c d
  | .equalAngle a b c d e f => .equalAngle a b c d e f
  | .ratioDivision p a b r => .ratioDivision p a b (lvExprToIR r)

/-! ===============================================================
   第六部分：LvProgram → ConstraintGraph 转换
   =============================================================== -/

/-- 将整个 Lv 程序转换为约束图（提取所有约束并映射为 IR 约束） -/
def lvProgramToConstraintGraph (p : LvProgram) : ConstraintGraph :=
  (getConstraints p).map lvConstraintToIR

/-! ===============================================================
   第七部分：Prove 语句 → ProofTrace 转换
   =============================================================== -/

/-- 将 Prove 语句转换为证据迹（返回仅含 qed 的迹作为占位）
    更完整的实现应使用 mkProgramProofTrace -/
def lvProveToEvidence (s : LvStmt) : Option ProofTrace :=
  match s with
  | .prove _ => some [.qed]
  | _ => none

/-- 为 Lv 程序构造平凡证明迹：将所有约束作为 hypothesis，最后 qed -/
def mkTrivialProofTrace (p : LvProgram) : ProofTrace :=
  (getConstraints p).map (fun c => ProofStep.hypothesis (lvConstraintToIR c)) ++ [.qed]

/-- 平凡证明迹以 qed 结尾 -/
lemma mkTrivialProofTrace_ends_qed (p : LvProgram) :
    (mkTrivialProofTrace p).getLast? = some .qed := by
  unfold mkTrivialProofTrace; simp

/-- 从程序构造完整证明迹：若存在 Prove 语句，返回包含所有约束假设的迹
    这比 lvProveToEvidence 更完整，因为它包含了实际约束作为 hypothesis 步骤 -/
def mkProgramProofTrace (p : LvProgram) : Option ProofTrace :=
  match findProveStmt p with
  | some (.prove _) => some (mkTrivialProofTrace p)
  | _ => none

/-- 若程序有 Prove 语句，则 mkProgramProofTrace 返回的迹能通过证据检查 -/
lemma mkProgramProofTrace_passes_check (p : LvProgram) (h_prove : findProveStmt p ≠ none) :
    evidence_check (lvProgramToConstraintGraph p) 
      (mkTrivialProofTrace p) = true := by
  have h_some : ∃ s, findProveStmt p = some s :=
    Option.ne_none_iff_exists.mp h_prove
  -- evidence_completeness 直接保证平凡迹通过检查
  rcases evidence_completeness (lvProgramToConstraintGraph p) with ⟨t, ht⟩
  -- evidence_completeness 构造的迹就是 trivial_proof_trace
  -- 验证它等价于 mkTrivialProofTrace
  have h_same : trivial_proof_trace (lvProgramToConstraintGraph p) = 
    mkTrivialProofTrace p := by
    unfold trivial_proof_trace mkTrivialProofTrace lvProgramToConstraintGraph
    simp
  rw [← h_same]
  exact ht

/-! ===============================================================
   第八部分：核心等价引理
   lv_constraint_sem ↔ ir_sem (via lvConstraintToIR)
   =============================================================== -/

/-- 核心引理：Lv 约束的语义与对应 IR 约束的语义完全等价 -/
lemma lv_constraint_sem_iff (env : String → ℝ × ℝ) (c : LvConstraint) :
    lv_constraint_sem env c ↔ ir_sem env (lvConstraintToIR c) := by
  induction c with
  | distance a b d =>
    simp [lv_constraint_sem, lvConstraintToIR, ir_sem, lv_expr_eval_eq env d]
  | collinear a b c =>
    simp [lv_constraint_sem, lvConstraintToIR, ir_sem]
  | perpendicular a b c d =>
    simp [lv_constraint_sem, lvConstraintToIR, ir_sem]
  | parallel a b c d =>
    simp [lv_constraint_sem, lvConstraintToIR, ir_sem]
  | angle a b c d theta =>
    simp [lv_constraint_sem, lvConstraintToIR, ir_sem, lv_expr_eval_eq env theta]
  | eq_expr e1 e2 =>
    simp [lv_constraint_sem, lvConstraintToIR, ir_sem, lv_expr_eval_eq env e1, lv_expr_eval_eq env e2]
  | lt_expr e1 e2 =>
    simp [lv_constraint_sem, lvConstraintToIR, ir_sem, lv_expr_eval_eq env e1, lv_expr_eval_eq env e2]
  | gt_expr e1 e2 =>
    simp [lv_constraint_sem, lvConstraintToIR, ir_sem, lv_expr_eval_eq env e1, lv_expr_eval_eq env e2]
  | radius c a r =>
    simp [lv_constraint_sem, lvConstraintToIR, ir_sem, lv_expr_eval_eq env r]
  | tangent ctr pt la lb =>
    simp [lv_constraint_sem, lvConstraintToIR, ir_sem]
  | midpoint m a b =>
    simp [lv_constraint_sem, lvConstraintToIR, ir_sem]
  | rightAngle a b c =>
    simp [lv_constraint_sem, lvConstraintToIR, ir_sem]
  | equalLength a b c d =>
    simp [lv_constraint_sem, lvConstraintToIR, ir_sem]
  | equalAngle a b c d e f =>
    simp [lv_constraint_sem, lvConstraintToIR, ir_sem]
  | ratioDivision p a b r =>
    simp [lv_constraint_sem, lvConstraintToIR, ir_sem, lv_expr_eval_eq env r]

/-! ===============================================================
   第九部分：正确性定理
   =============================================================== -/

/-- 正确性定理：Lv 程序转换后的约束图可满足 ⟺ 原 Lv 程序语义成立 -/
theorem lv_to_ir_correctness (p : LvProgram) :
    graph_satisfiable (lvProgramToConstraintGraph p) ↔ lv_sem p := by
  unfold lvProgramToConstraintGraph graph_satisfiable lv_sem
  constructor
  · intro ⟨env, h⟩
    refine ⟨env, λ c hc => ?_⟩
    have hc_mem : lvConstraintToIR c ∈ (getConstraints p).map lvConstraintToIR := by
      apply List.mem_map.mpr
      exact ⟨c, hc, rfl⟩
    have h_ir : ir_sem env (lvConstraintToIR c) := h _ hc_mem
    exact (lv_constraint_sem_iff env c).mpr h_ir
  · intro ⟨env, h⟩
    refine ⟨env, λ c_ir hc_ir => ?_⟩
    rcases List.mem_map.mp hc_ir with ⟨c, hc, rfl⟩
    have h_lv : lv_constraint_sem env c := h c hc
    exact (lv_constraint_sem_iff env c).mp h_lv

/-- 推论：空程序的约束图总是可满足的 -/
corollary empty_program_satisfiable :
    graph_satisfiable (lvProgramToConstraintGraph (⟨"", []⟩ : LvProgram)) := by
  rw [lvProgramToConstraintGraph, getConstraints, List.map_nil]
  exact empty_graph_satisfiable

/-- 推论：空程序在 Lv 语义下成立 -/
corollary empty_program_lv_sem : lv_sem (⟨"", []⟩ : LvProgram) :=
  (lv_to_ir_correctness (⟨"", []⟩ : LvProgram)).mpr empty_program_satisfiable

/-! ===============================================================
   第十部分：证明可靠性定理
   =============================================================== -/

/-- 证明可靠性：对 Lv 程序 p，若其 Prove 语句对应的证据迹 t 通过
    evidence_check 且迹语义可靠，则程序语义成立 -/
theorem lv_prove_soundness (p : LvProgram) (t : ProofTrace)
    (h_evidence : lvProveToEvidence (findProveStmt p) = some t)
    (h_check : evidence_check (lvProgramToConstraintGraph p) t = true)
    (h_sound : TraceSound (initVerifier (lvProgramToConstraintGraph p)) t) :
    lv_sem p :=
  (lv_to_ir_correctness p).mp
    (evidence_soundness (lvProgramToConstraintGraph p) t h_check h_sound)

/-- 通用引理：若 env 同时满足约束图 g 和当前状态 st 的所有已证约束，
    则 hypothesis 列表构成的证明迹片段从 st 开始是语义可靠的。 -/
lemma hypotheses_trace_sound_gen (g : ConstraintGraph) (env : String → ℝ × ℝ) (st : VerifierState)
    (h_env_g : ∀ c ∈ g, ir_sem env c)
    (h_env_st : ∀ c ∈ st.proved, ir_sem env c) :
    TraceSound st (g.map (fun c => .hypothesis c)) := by
  induction g generalizing st with
  | nil =>
    exact TraceSound.nil st
  | cons c g' ih =>
    have h_c : ir_sem env c := h_env_g c (by simp)
    have h_step_sound : step_sound st (.hypothesis c) := by
      unfold step_sound transition
      intro _
      exact ⟨env, λ c' h => by
        simp at h
        rcases h with (rfl | h')
        · exact h_c
        · exact h_env_st c' h'⟩
    have h_env_trans : ∀ c' ∈ (transition st (.hypothesis c)).proved, ir_sem env c' := by
      intro c' h
      unfold transition at h
      simp at h
      rcases h with (rfl | h')
      · exact h_c
      · exact h_env_st c' h'
    have h_env_g' : ∀ c' ∈ g', ir_sem env c' := by
      intro c' hc'
      exact h_env_g c' (List.mem_cons_of_mem c hc')
    have h_rest_sound : TraceSound (transition st (.hypothesis c)) (g'.map (fun c' => .hypothesis c')) :=
      ih (transition st (.hypothesis c)) env h_env_g' h_env_trans
    exact TraceSound.cons st (.hypothesis c) (g'.map (fun c' => .hypothesis c')) h_step_sound h_rest_sound

/-- 当约束图 g 可满足时，从初始状态开始的 hypothesis 列表迹是语义可靠的。 -/
lemma hypotheses_trace_sound_from_init (g : ConstraintGraph) (h_sat : graph_satisfiable g) :
    TraceSound (initVerifier g) (g.map (fun c => .hypothesis c)) := by
  rcases h_sat with ⟨env, h_env⟩
  apply hypotheses_trace_sound_gen g env (initVerifier g) h_env
  intro c' h
  simp at h

/-- 空约束图上的平凡证明迹 [qed] 是语义可靠的 -/
lemma qed_trace_sound (g : ConstraintGraph) : TraceSound (initVerifier g) [.qed] := by
  refine TraceSound.cons (initVerifier g) .qed [] ?_ (TraceSound.nil _)
  unfold step_sound transition initVerifier
  intro h_sat
  exact h_sat

/-- TraceSound 的组合性引理：若 t₁ 从状态 st 是语义可靠的，
    且 t₂ 从 trace_fold st t₁（即 t₁ 处理后的状态）也是语义可靠的，
    则 t₁ ++ t₂ 从状态 st 是语义可靠的。
    
    此引理允许我们将证明迹分段组合。
    证明：对 TraceSound st t₁ 的推导进行归纳。 -/
lemma trace_sound_append (st : VerifierState) (t1 t2 : ProofTrace)
    (h1 : TraceSound st t1) (h2 : TraceSound (trace_fold st t1) t2) :
    TraceSound st (t1 ++ t2) := by
  induction h1 with
  | nil =>
    -- t1 = []，trace_fold st [] = st，故 st ++ t2 = t2
    simpa [trace_fold] using h2
  | cons st' step rest h_step h_rest ih =>
    -- st' ⊢ step :: rest，需证 st' ⊢ (step :: rest) ++ t2 = step :: (rest ++ t2)
    -- h_rest: TraceSound (transition st' step) rest
    -- h2（调整后）: TraceSound (trace_fold st' (step :: rest)) t2
    --   = TraceSound (trace_fold (transition st' step) rest) t2
    -- 由 ih: 从 h_rest 和调整后的 h2 得 TraceSound (transition st' step) (rest ++ t2)
    apply TraceSound.cons st' step (rest ++ t2) h_step
    apply ih
    -- 展开 trace_fold 使 h2 的状态与 ih 的期望匹配
    unfold trace_fold at h2
    -- trace_fold st' (step :: rest) = trace_fold (transition st' step) rest
    simpa [trace_fold] using h2

/-- qed 步骤从任意状态都是可靠的（只要状态已满足原图约束）。
    transition st .qed = st，故 step_sound 退化为恒等映射。 -/
lemma qed_sound_at_state (g : ConstraintGraph) (st : VerifierState) :
    TraceSound st [.qed] := by
  refine TraceSound.cons st .qed [] ?_ (TraceSound.nil _)
  unfold step_sound transition
  intro h_sat
  exact h_sat

/-- 程序级证明迹的可靠性：若约束图可满足，则 mkTrivialProofTrace 对应的迹语义可靠。
    
    证明结构：
    1. hypothesis 片段：对每个约束 c ∈ g，hypothesis_c 步骤从空状态是可靠的
       （因为 h_sat 保证存在 env 满足所有约束）。
    2. 经过 hypothesis 片段后，状态为 { proved := g.reverse }。
    3. qed 步骤从该状态总是可靠的（qed_sound_at_state）。
    4. 由 trace_sound_append 组合二者。 -/
lemma program_trace_sound (p : LvProgram) (h_sat : graph_satisfiable (lvProgramToConstraintGraph p)) :
    TraceSound (initVerifier (lvProgramToConstraintGraph p)) (mkTrivialProofTrace p) := by
  let g := lvProgramToConstraintGraph p
  -- 将 mkTrivialProofTrace 拆分为 hypothesis 列表 + [qed]
  have h_mkTrace_eq : mkTrivialProofTrace p =
      ((getConstraints p).map (fun c => ProofStep.hypothesis (lvConstraintToIR c))) ++ [.qed] := by
    unfold mkTrivialProofTrace
    rfl
  rw [h_mkTrace_eq]
  -- 应用 trace_sound_append
  apply trace_sound_append (initVerifier g)
    ((getConstraints p).map (fun c => ProofStep.hypothesis (lvConstraintToIR c)))
    [.qed]
  · -- 第一部分：hypothesis 片段可靠
    rcases h_sat with ⟨env, h_env⟩
    apply hypotheses_trace_sound_gen g env (initVerifier g)
    · -- ∀ c ∈ g, ir_sem env c
      intro c hc
      exact h_env c hc
    · -- ∀ c ∈ (initVerifier g).proved, ir_sem env c（空状态）
      intro c hc
      simp [initVerifier] at hc
  · -- 第二部分：qed 在 hypothesis 处理后的状态上可靠
    -- 计算 hypothesis 处理后的状态
    -- trace_fold (initVerifier g) ((getConstraints p).map ...)
    --   = { proved := g.reverse }（由 go_hypotheses_some + go_some_eq_trace_fold）
    -- 但这里不需要展开，直接使用 qed_sound_at_state
    apply qed_sound_at_state g

/-- 简化版可靠性：对任意 Lv 程序 p，若其约束图为空，则平凡证明迹通过检查 -/
theorem lv_empty_trivial_prove_soundness (p : LvProgram)
    (h_empty : getConstraints p = []) :
    evidence_check (lvProgramToConstraintGraph p) (mkTrivialProofTrace p) = true := by
  unfold mkTrivialProofTrace
  rw [h_empty]
  simp [lvProgramToConstraintGraph, evidence_check, evidence_check_witness]

/-- 可靠性 Corollary：若 Lv 程序 p 的约束图为空，则 p 语义成立 -/
corollary lv_empty_program_sound (p : LvProgram)
    (h_empty : getConstraints p = []) :
    lv_sem p := by
  have h_sat : graph_satisfiable (lvProgramToConstraintGraph p) := by
    rw [lvProgramToConstraintGraph, h_empty, List.map_nil]
    exact empty_graph_satisfiable
  exact (lv_to_ir_correctness p).mp h_sat

/-! ===============================================================
   第十一部分：示例
   =============================================================== -/

/-- 示例1：构造一个包含距离约束的 Lv 程序并验证其转换正确性 -/
example : True := by
  let prog : LvProgram := {
    filename := "example.lv"
    stmts := [
      .constraintDecl "distAB" (.distance "A" "B" (.intLit 5)) true
    ]
  }
  have h_conv : lvProgramToConstraintGraph prog = [.distance "A" "B" (.const 5)] := by
    unfold lvProgramToConstraintGraph getConstraints prog
    simp [lvConstraintToIR, lvExprToIR]
  trivial

/-- 示例2：空程序的正确性（语义成立） -/
example : lv_sem (⟨"empty.lv", []⟩ : LvProgram) :=
  lv_empty_program_sound (⟨"empty.lv", []⟩ : LvProgram) (by rfl)

/-- 示例3：空程序的约束图可满足 -/
example : graph_satisfiable (lvProgramToConstraintGraph (⟨"empty.lv", []⟩ : LvProgram)) := by
  apply empty_program_satisfiable

/-- 示例4：对含距离约束的程序，构造证明迹并验证通过 -/
example : True := by
  let prog : LvProgram := {
    filename := "dist_example.lv"
    stmts := [
      .constraintDecl "d1" (.distance "A" "B" (.intLit 5)) true,
      .prove "d1"
    ]
  }
  let g := lvProgramToConstraintGraph prog
  let t := mkTrivialProofTrace prog
  have h_check : evidence_check g t = true := by
    -- evidence_completeness 保证平凡迹通过检查
    rcases evidence_completeness g with ⟨t', ht'⟩
    -- trivial_proof_trace g 与 mkTrivialProofTrace prog 一致
    have h_same : trivial_proof_trace g = t := by
      unfold trivial_proof_trace mkTrivialProofTrace lvProgramToConstraintGraph
      simp
    rw [← h_same]
    exact ht'
  trivial

/-- 示例5：lv_to_ir_correctness 的应用 — 对距离约束程序，语义等价性成立 -/
example : True := by
  let prog : LvProgram := {
    filename := "dist_example2.lv"
    stmts := [
      .constraintDecl "d1" (.distance "A" "B" (.intLit 3)) true
    ]
  }
  -- lv_to_ir_correctness 断言 graph_satisfiable ↔ lv_sem
  have h_equiv := lv_to_ir_correctness prog
  trivial

/-- 示例6：构造直角三角形约束并验证转换 -/
example : True := by
  let prog : LvProgram := {
    filename := "right_triangle.lv"
    stmts := [
      .constraintDecl "right" (.rightAngle "A" "B" "C") true,
      .constraintDecl "distAB" (.distance "A" "B" (.intLit 3)) true,
      .constraintDecl "distBC" (.distance "B" "C" (.intLit 4)) true,
      .prove "right"
    ]
  }
  let g := lvProgramToConstraintGraph prog
  -- 验证转换：约束图包含 rightAngle 和两个 distance 约束
  have h_conv : g = [
    .rightAngle "A" "B" "C",
    .distance "A" "B" (.const 3),
    .distance "B" "C" (.const 4)
  ] := by
    unfold g lvProgramToConstraintGraph getConstraints prog
    simp [lvConstraintToIR, lvExprToIR]
  trivial

end lvFormal.Theory.LvSemantics

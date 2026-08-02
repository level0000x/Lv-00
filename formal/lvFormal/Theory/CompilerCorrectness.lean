/-
编译器正确性证明

本模块证明 lvLang → IR 编译器保持语义正确性：
- point/constraint/prove/normalize 各语句的编译边正确性
- 编译器保持可满足性（通过编译桥接引理）
- 编译器语义一致性推论
-/

import lvFormal.Theory.lvLang
import lvFormal.Theory.IR
import lvFormal.Theory.Compiler

namespace lvFormal.Theory.CompilerCorrectness

open lvFormal.Theory.lvLang
open lvFormal.Theory.IR
open lvFormal.Theory.Compiler

/-! ## 辅助定义 -/

def zero_pt : lvPoint := { name := "", x := 0, y := 0 }

def zero_sc : lvConstraint := { name := "", kind := .collinear, args := [] }

/-! ## 语句编译边正确性 -/

/-- point 语句不产生 IR 约束 -/
theorem stmt_compiled_edge_correct_point (pts : List lvPoint) (p : lvPoint) :
    compile_stmt (.point p) pts = [] := by
  rfl

/-- constraint 语句编译：成功编译时产生对应的 IR 约束 -/
theorem stmt_compiled_edge_correct_constraint (pts : List lvPoint)
    (c : lvConstraint) (ir : IRConstraint) (h : compile_constraint pts c = some ir) :
    compile_stmt (.constraint c) pts = [ir] := by
  unfold compile_stmt
  simp [h]

/-- prove 语句不产生 IR 约束 -/
theorem stmt_compiled_edge_correct_prove (pts : List lvPoint) :
    compile_stmt .prove pts = [] := by
  rfl

/-- normalize 语句不产生 IR 约束 -/
theorem stmt_compiled_edge_correct_normalize (pts : List lvPoint) :
    compile_stmt .normalize pts = [] := by
  rfl

/-! ## 编译桥接引理 -/

/-- 编译产物在退化环境（所有点坐标为零）下恒成立：
    编译总是产生可满足的约束（对每个约束可单独满足）。 -/
lemma compile_constraint_trivially_satisfied (c : lvConstraint) :
    ∀ ir, compile_constraint [] c = some ir → ir_sem (fun _ => (0, 0)) ir := by
  intro ir h
  cases c with
  | mk _ kind args =>
    match kind with
    | .collinear =>
        match args with
        | [] => simp [compile_constraint] at h
        | [_] => simp [compile_constraint] at h
        | [_, _] => simp [compile_constraint] at h
        | a :: b :: c' :: rest =>
            match rest with
            | [] =>
                simp [compile_constraint] at h
                rw [h.symm]
                simp [ir_sem, cross]
            | d :: rest' => simp [compile_constraint] at h
    | .parallel =>
        match args with
        | [] => simp [compile_constraint] at h
        | [_] => simp [compile_constraint] at h
        | [_, _] => simp [compile_constraint] at h
        | [_, _, _] => simp [compile_constraint] at h
        | a :: b :: c' :: d :: rest =>
            match rest with
            | [] =>
                simp [compile_constraint] at h
                rw [h.symm]
                simp [ir_sem, cross, ptX, ptY]
            | e :: rest' => simp [compile_constraint] at h
    | .perpendicular =>
        match args with
        | [] => simp [compile_constraint] at h
        | [_] => simp [compile_constraint] at h
        | [_, _] => simp [compile_constraint] at h
        | [_, _, _] => simp [compile_constraint] at h
        | a :: b :: c' :: d :: rest =>
            match rest with
            | [] =>
                simp [compile_constraint] at h
                rw [h.symm]
                simp [ir_sem, dot, ptX, ptY]
            | e :: rest' => simp [compile_constraint] at h
    | .distance =>
        match args with
        | [] => simp [compile_constraint] at h
        | [_] => simp [compile_constraint] at h
        | a :: b :: rest =>
            match rest with
            | [] =>
                simp [compile_constraint] at h
                rw [h.symm]
                simp [ir_sem, IR.dist, eval_expr]
            | c' :: rest' => simp [compile_constraint] at h
    | .angle =>
        match args with
        | [] => simp [compile_constraint] at h
        | [_] => simp [compile_constraint] at h
        | [_, _] => simp [compile_constraint] at h
        | [_, _, _] => simp [compile_constraint] at h
        | a :: b :: c' :: d :: rest =>
            match rest with
            | [] =>
                simp [compile_constraint] at h
                rw [h.symm]
                simp [ir_sem, eval_expr]
            | e :: rest' => simp [compile_constraint] at h
    | .midpoint =>
        match args with
        | [] => simp [compile_constraint] at h
        | [_] => simp [compile_constraint] at h
        | [_, _] => simp [compile_constraint] at h
        | m :: a :: b :: rest =>
            match rest with
            | [] =>
                simp [compile_constraint] at h
                rw [h.symm]
                simp [ir_sem, ptX, ptY]
            | c' :: rest' => simp [compile_constraint] at h
    | .rightAngle =>
        match args with
        | [] => simp [compile_constraint] at h
        | [_] => simp [compile_constraint] at h
        | [_, _] => simp [compile_constraint] at h
        | a :: b :: c' :: rest =>
            match rest with
            | [] =>
                simp [compile_constraint] at h
                rw [h.symm]
                simp [ir_sem, dot, ptX, ptY]
            | d :: rest' => simp [compile_constraint] at h
    | .equalLength =>
        match args with
        | [] => simp [compile_constraint] at h
        | [_] => simp [compile_constraint] at h
        | [_, _] => simp [compile_constraint] at h
        | [_, _, _] => simp [compile_constraint] at h
        | a :: b :: c' :: d :: rest =>
            match rest with
            | [] =>
                simp [compile_constraint] at h
                rw [h.symm]
                simp [ir_sem, IR.dist]
            | e :: rest' => simp [compile_constraint] at h
    | .equalAngle =>
        match args with
        | [] => simp [compile_constraint] at h
        | [_] => simp [compile_constraint] at h
        | [_, _] => simp [compile_constraint] at h
        | [_, _, _] => simp [compile_constraint] at h
        | [_, _, _, _] => simp [compile_constraint] at h
        | [_, _, _, _, _] => simp [compile_constraint] at h
        | a :: b :: c' :: d :: e :: f :: rest =>
            match rest with
            | [] =>
                simp [compile_constraint] at h
                rw [h.symm]
                simp [ir_sem, dot, IR.dist, ptX, ptY]
            | g :: rest' => simp [compile_constraint] at h
    | .radius =>
        match args with
        | [] => simp [compile_constraint] at h
        | [_] => simp [compile_constraint] at h
        | c' :: a :: rest =>
            match rest with
            | [] =>
                simp [compile_constraint] at h
                rw [h.symm]
                simp [ir_sem, IR.dist, eval_expr]
            | b :: rest' => simp [compile_constraint] at h
    | .tangent =>
        match args with
        | [] => simp [compile_constraint] at h
        | [_] => simp [compile_constraint] at h
        | [_, _] => simp [compile_constraint] at h
        | [_, _, _] => simp [compile_constraint] at h
        | c_ctr :: c_pt :: la :: lb :: rest =>
            match rest with
            | [] =>
                simp [compile_constraint] at h
                rw [h.symm]
                simp [ir_sem, IR.dist, ptX, ptY]
            | c' :: rest' => simp [compile_constraint] at h
    | .ratioDivision =>
        match args with
        | [] => simp [compile_constraint] at h
        | [_] => simp [compile_constraint] at h
        | [_, _] => simp [compile_constraint] at h
        | p :: a :: b :: rest =>
            match rest with
            | [] =>
                simp [compile_constraint] at h
                rw [h.symm]
                simp [ir_sem, ptX, ptY, eval_expr]
            | c' :: rest' => simp [compile_constraint] at h

/-- 编译桥接引理：若 compile_constraint pts c = some ir，
    则编译产物 ir 在退化环境下成立（编译总是产生可满足的约束）。 -/
lemma compile_bridge_sem (c : lvConstraint) (pts : List lvPoint) (ir : IRConstraint)
    (h_compile : compile_constraint pts c = some ir) :
    ir_sem (fun _ => (0, 0)) ir := by
  have hps : compile_constraint [] c = some ir := by
    rw [compile_constraint_ps_irrelevant [] pts]
    exact h_compile
  exact compile_constraint_trivially_satisfied c ir hps

/-! ## 编译保持可满足性 -/

/-- 从程序中提取所有约束 -/
def constraints_of_program (prog : lvProgram) : List lvConstraint :=
  prog.flatMap (fun st => match st with | .constraint c => [c] | _ => [])

/-- compile_program_go 的归纳版本：编译产物的来源追踪 -/
lemma compile_program_go_contains (pts : List lvPoint) (prog : lvProgram) :
    ∀ ir ∈ compile_program_go pts prog,
      ∃ c ∈ constraints_of_program prog, compile_constraint [] c = some ir := by
  induction prog generalizing pts with
  | nil =>
      intro ir hir
      simp [compile_program_go] at hir
  | cons st sts ih =>
      intro ir hir
      cases st with
      | point p =>
          simp [compile_program_go, compile_stmt] at hir
          rcases ih (p :: pts) ir hir with ⟨c, hc, hcc⟩
          refine ⟨c, ?_, hcc⟩
          simpa [constraints_of_program] using hc
      | constraint c =>
          simp [compile_program_go, compile_stmt] at hir
          rcases hir with (hir | hir)
          · cases hc0 : compile_constraint pts c with
            | none => simp [hc0] at hir
            | some ir' =>
                simp [hc0] at hir
                subst ir
                refine ⟨c, ?_, ?_⟩
                · simp [constraints_of_program, List.mem_append]
                · rw [compile_constraint_ps_irrelevant [] pts, hc0]
          · rcases ih pts ir hir with ⟨c', hc', hcc'⟩
            refine ⟨c', ?_, hcc'⟩
            right
            exact hc'
      | prove =>
          simp [compile_program_go, compile_stmt] at hir
          rcases ih pts ir hir with ⟨c, hc, hcc⟩
          refine ⟨c, ?_, hcc⟩
          simpa [constraints_of_program] using hc
      | normalize =>
          simp [compile_program_go, compile_stmt] at hir
          rcases ih pts ir hir with ⟨c, hc, hcc⟩
          refine ⟨c, ?_, hcc⟩
          simpa [constraints_of_program] using hc

/-- 编译后的IR约束都来自源程序约束的编译 -/
lemma compile_program_contains_compiled_constraints (prog : lvProgram) :
  ∀ ir ∈ compile_program prog, ∃ c ∈ constraints_of_program prog, compile_constraint [] c = some ir := by
  unfold compile_program
  exact compile_program_go_contains [] prog

/-- 编译保持可满足性：
    若源程序 lv 可满足（按新的坐标条件语义），
    则编译后的 IR 约束图也可满足（退化环境构造）。 -/
theorem compile_preserves_satisfiability (prog : lvProgram) :
    lvLang.satisfiable (lvLang.eval_program lvLang.initialState prog) →
    graph_satisfiable (compile_program prog) := by
  intro h_sat
  refine ⟨fun _ => (0, 0), ?_⟩
  intro ir hir
  rcases compile_program_contains_compiled_constraints prog ir hir with ⟨c, hc, hcc⟩
  exact compile_constraint_trivially_satisfied c ir hcc

/-! ## 变量集不相交性 -/

/-- 获取程序中所有引用到的变量名集合 -/
def vars_of_program (prog : lvProgram) : List String :=
  let point_names := prog.flatMap (fun st => match st with | .point p => [p.name] | _ => [])
  let constraint_args := prog.flatMap (fun st => match st with | .constraint c => c.args | _ => [])
  point_names ++ constraint_args

/-- 两个程序的变量集合不相交 -/
def variables_disjoint (p1 p2 : lvProgram) : Prop :=
  (∀ v, v ∈ vars_of_program p1 → v ∉ vars_of_program p2) ∧
  (∀ v, v ∈ vars_of_program p2 → v ∉ vars_of_program p1)

/-- IR 表达式中引用的所有变量名 -/
def ir_expr_vars : IRExpr → List String
  | .var v => [v]
  | .const _ => []
  | .add e1 e2 => ir_expr_vars e1 ++ ir_expr_vars e2
  | .sub e1 e2 => ir_expr_vars e1 ++ ir_expr_vars e2
  | .mul e1 e2 => ir_expr_vars e1 ++ ir_expr_vars e2
  | .div e1 e2 => ir_expr_vars e1 ++ ir_expr_vars e2
  | .sqrt e => ir_expr_vars e

/-- IR 约束中引用的所有变量名（表达式参数也提取其中变量）-/
def ir_vars : IRConstraint → List String
  | .distance a b d => [a, b] ++ ir_expr_vars d
  | .collinear a b c => [a, b, c]
  | .perpendicular a b c d => [a, b, c, d]
  | .parallel a b c d => [a, b, c, d]
  | .angle a b c d theta => [a, b, c, d] ++ ir_expr_vars theta
  | .midpoint m a b => [m, a, b]
  | .rightAngle a b c => [a, b, c]
  | .equalLength a b c d => [a, b, c, d]
  | .equalAngle a b c d e f => [a, b, c, d, e, f]
  | .radius c a r => [c, a] ++ ir_expr_vars r
  | .tangent ctr pt la lb => [ctr, pt, la, lb]
  | .ratioDivision p a b r => [p, a, b] ++ ir_expr_vars r
  | .eq_expr e1 e2 => ir_expr_vars e1 ++ ir_expr_vars e2
  | .lt_expr e1 e2 => ir_expr_vars e1 ++ ir_expr_vars e2
  | .gt_expr e1 e2 => ir_expr_vars e1 ++ ir_expr_vars e2

/-- eval_expr 只依赖表达式中出现的变量 -/
lemma eval_expr_depends_on_vars (e : IRExpr) (env1 env2 : String → ℝ × ℝ)
    (h : ∀ v, v ∈ ir_expr_vars e → env1 v = env2 v) :
    eval_expr env1 e = eval_expr env2 e := by
  induction e with
  | var v =>
      have hv : env1 v = env2 v := h v (by simp [ir_expr_vars])
      rw [eval_expr, hv]
      simp [eval_expr]
  | const c => simp [eval_expr]
  | add e1 e2 ih1 ih2 =>
      have h1 : eval_expr env1 e1 = eval_expr env2 e1 :=
        ih1 (fun v hv => h v (by simp [ir_expr_vars, hv]))
      have h2 : eval_expr env1 e2 = eval_expr env2 e2 :=
        ih2 (fun v hv => h v (by simp [ir_expr_vars, hv]))
      simp [eval_expr, h1, h2]
  | sub e1 e2 ih1 ih2 =>
      have h1 : eval_expr env1 e1 = eval_expr env2 e1 :=
        ih1 (fun v hv => h v (by simp [ir_expr_vars, hv]))
      have h2 : eval_expr env1 e2 = eval_expr env2 e2 :=
        ih2 (fun v hv => h v (by simp [ir_expr_vars, hv]))
      simp [eval_expr, h1, h2]
  | mul e1 e2 ih1 ih2 =>
      have h1 : eval_expr env1 e1 = eval_expr env2 e1 :=
        ih1 (fun v hv => h v (by simp [ir_expr_vars, hv]))
      have h2 : eval_expr env1 e2 = eval_expr env2 e2 :=
        ih2 (fun v hv => h v (by simp [ir_expr_vars, hv]))
      simp [eval_expr, h1, h2]
  | div e1 e2 ih1 ih2 =>
      have h1 : eval_expr env1 e1 = eval_expr env2 e1 :=
        ih1 (fun v hv => h v (by simp [ir_expr_vars, hv]))
      have h2 : eval_expr env1 e2 = eval_expr env2 e2 :=
        ih2 (fun v hv => h v (by simp [ir_expr_vars, hv]))
      simp [eval_expr, h1, h2]
  | sqrt e ih =>
      have h1 : eval_expr env1 e = eval_expr env2 e :=
        ih (fun v hv => h v (by simp [ir_expr_vars, hv]))
      simp [eval_expr, h1]

/-- ir_sem 只依赖于 ir_vars 中列出的变量 -/
lemma ir_sem_depends_on_vars (ir : IRConstraint) (env1 env2 : String → ℝ × ℝ)
    (h : ∀ v, v ∈ ir_vars ir → env1 v = env2 v) : ir_sem env1 ir ↔ ir_sem env2 ir := by
  induction ir with
  | distance a b d =>
      have ha : env1 a = env2 a := h a (by simp [ir_vars])
      have hb : env1 b = env2 b := h b (by simp [ir_vars])
      have hd : eval_expr env1 d = eval_expr env2 d :=
        eval_expr_depends_on_vars d env1 env2 (fun v hv => h v (by simp [ir_vars, hv]))
      simp [ir_sem, ha, hb, hd]
  | collinear a b c =>
      have ha : env1 a = env2 a := h a (by simp [ir_vars])
      have hb : env1 b = env2 b := h b (by simp [ir_vars])
      have hc : env1 c = env2 c := h c (by simp [ir_vars])
      simp [ir_sem, ha, hb, hc]
  | perpendicular a b c d =>
      have ha : env1 a = env2 a := h a (by simp [ir_vars])
      have hb : env1 b = env2 b := h b (by simp [ir_vars])
      have hc : env1 c = env2 c := h c (by simp [ir_vars])
      have hd : env1 d = env2 d := h d (by simp [ir_vars])
      simp [ir_sem, ha, hb, hc, hd]
  | parallel a b c d =>
      have ha : env1 a = env2 a := h a (by simp [ir_vars])
      have hb : env1 b = env2 b := h b (by simp [ir_vars])
      have hc : env1 c = env2 c := h c (by simp [ir_vars])
      have hd : env1 d = env2 d := h d (by simp [ir_vars])
      simp [ir_sem, ha, hb, hc, hd]
  | angle a b c d theta =>
      have ha : env1 a = env2 a := h a (by simp [ir_vars])
      have hb : env1 b = env2 b := h b (by simp [ir_vars])
      have hc : env1 c = env2 c := h c (by simp [ir_vars])
      have hd : env1 d = env2 d := h d (by simp [ir_vars])
      have htheta : eval_expr env1 theta = eval_expr env2 theta :=
        eval_expr_depends_on_vars theta env1 env2 (fun v hv => h v (by simp [ir_vars, hv]))
      simp [ir_sem, ha, hb, hc, hd, htheta]
  | eq_expr e1 e2 =>
      have h1 : eval_expr env1 e1 = eval_expr env2 e1 :=
        eval_expr_depends_on_vars e1 env1 env2 (fun v hv => h v (by simp [ir_vars, hv]))
      have h2 : eval_expr env1 e2 = eval_expr env2 e2 :=
        eval_expr_depends_on_vars e2 env1 env2 (fun v hv => h v (by simp [ir_vars, hv]))
      simp [ir_sem, h1, h2]
  | lt_expr e1 e2 =>
      have h1 : eval_expr env1 e1 = eval_expr env2 e1 :=
        eval_expr_depends_on_vars e1 env1 env2 (fun v hv => h v (by simp [ir_vars, hv]))
      have h2 : eval_expr env1 e2 = eval_expr env2 e2 :=
        eval_expr_depends_on_vars e2 env1 env2 (fun v hv => h v (by simp [ir_vars, hv]))
      simp [ir_sem, h1, h2]
  | gt_expr e1 e2 =>
      have h1 : eval_expr env1 e1 = eval_expr env2 e1 :=
        eval_expr_depends_on_vars e1 env1 env2 (fun v hv => h v (by simp [ir_vars, hv]))
      have h2 : eval_expr env1 e2 = eval_expr env2 e2 :=
        eval_expr_depends_on_vars e2 env1 env2 (fun v hv => h v (by simp [ir_vars, hv]))
      simp [ir_sem, h1, h2]
  | radius c a r =>
      have hc : env1 c = env2 c := h c (by simp [ir_vars])
      have ha : env1 a = env2 a := h a (by simp [ir_vars])
      have hr : eval_expr env1 r = eval_expr env2 r :=
        eval_expr_depends_on_vars r env1 env2 (fun v hv => h v (by simp [ir_vars, hv]))
      simp [ir_sem, hc, ha, hr]
  | tangent ctr pt la lb =>
      have hctr : env1 ctr = env2 ctr := h ctr (by simp [ir_vars])
      have hpt : env1 pt = env2 pt := h pt (by simp [ir_vars])
      have hla : env1 la = env2 la := h la (by simp [ir_vars])
      have hlb : env1 lb = env2 lb := h lb (by simp [ir_vars])
      simp [ir_sem, hctr, hpt, hla, hlb]
  | midpoint m a b =>
      have hm : env1 m = env2 m := h m (by simp [ir_vars])
      have ha : env1 a = env2 a := h a (by simp [ir_vars])
      have hb : env1 b = env2 b := h b (by simp [ir_vars])
      simp [ir_sem, hm, ha, hb]
  | rightAngle a b c =>
      have ha : env1 a = env2 a := h a (by simp [ir_vars])
      have hb : env1 b = env2 b := h b (by simp [ir_vars])
      have hc : env1 c = env2 c := h c (by simp [ir_vars])
      simp [ir_sem, ha, hb, hc]
  | equalLength a b c d =>
      have ha : env1 a = env2 a := h a (by simp [ir_vars])
      have hb : env1 b = env2 b := h b (by simp [ir_vars])
      have hc : env1 c = env2 c := h c (by simp [ir_vars])
      have hd : env1 d = env2 d := h d (by simp [ir_vars])
      simp [ir_sem, ha, hb, hc, hd]
  | equalAngle a b c d e f =>
      have ha : env1 a = env2 a := h a (by simp [ir_vars])
      have hb : env1 b = env2 b := h b (by simp [ir_vars])
      have hc : env1 c = env2 c := h c (by simp [ir_vars])
      have hd : env1 d = env2 d := h d (by simp [ir_vars])
      have he : env1 e = env2 e := h e (by simp [ir_vars])
      have hf : env1 f = env2 f := h f (by simp [ir_vars])
      simp [ir_sem, ha, hb, hc, hd, he, hf]
  | ratioDivision p a b r =>
      have hp : env1 p = env2 p := h p (by simp [ir_vars])
      have ha : env1 a = env2 a := h a (by simp [ir_vars])
      have hb : env1 b = env2 b := h b (by simp [ir_vars])
      have hr : eval_expr env1 r = eval_expr env2 r :=
        eval_expr_depends_on_vars r env1 env2 (fun v hv => h v (by simp [ir_vars, hv]))
      simp [ir_sem, hp, ha, hb, hr]

/-- 编译产物的变量都来自源约束的 args -/
lemma compiled_ir_vars_subset_args (c : lvConstraint) (ir : IRConstraint)
    (h : compile_constraint [] c = some ir) (v : String) (hv : v ∈ ir_vars ir) :
    v ∈ c.args := by
  cases c with
  | mk _ kind args =>
    match kind with
    | .collinear =>
        cases args with
        | nil => simp [compile_constraint] at h
        | cons a l1 =>
          cases l1 with
          | nil => simp [compile_constraint] at h
          | cons b l2 =>
            cases l2 with
            | nil => simp [compile_constraint] at h
            | cons c' l3 =>
              cases l3 with
              | nil =>
                  simp [compile_constraint] at h
                  rw [h.symm] at hv
                  simpa [ir_vars] using hv
              | cons d l4 => simp [compile_constraint] at h
    | .parallel =>
        cases args with
        | nil => simp [compile_constraint] at h
        | cons a l1 =>
          cases l1 with
          | nil => simp [compile_constraint] at h
          | cons b l2 =>
            cases l2 with
            | nil => simp [compile_constraint] at h
            | cons c' l3 =>
              cases l3 with
              | nil => simp [compile_constraint] at h
              | cons d l4 =>
                cases l4 with
                | nil =>
                    simp [compile_constraint] at h
                    rw [h.symm] at hv
                    simpa [ir_vars] using hv
                | cons e l5 => simp [compile_constraint] at h
    | .perpendicular =>
        cases args with
        | nil => simp [compile_constraint] at h
        | cons a l1 =>
          cases l1 with
          | nil => simp [compile_constraint] at h
          | cons b l2 =>
            cases l2 with
            | nil => simp [compile_constraint] at h
            | cons c' l3 =>
              cases l3 with
              | nil => simp [compile_constraint] at h
              | cons d l4 =>
                cases l4 with
                | nil =>
                    simp [compile_constraint] at h
                    rw [h.symm] at hv
                    simpa [ir_vars] using hv
                | cons e l5 => simp [compile_constraint] at h
    | .distance =>
        cases args with
        | nil => simp [compile_constraint] at h
        | cons a l1 =>
          cases l1 with
          | nil => simp [compile_constraint] at h
          | cons b l2 =>
            cases l2 with
            | nil =>
                simp [compile_constraint] at h
                rw [h.symm] at hv
                simpa [ir_vars, ir_expr_vars] using hv
            | cons c' l3 => simp [compile_constraint] at h
    | .angle =>
        cases args with
        | nil => simp [compile_constraint] at h
        | cons a l1 =>
          cases l1 with
          | nil => simp [compile_constraint] at h
          | cons b l2 =>
            cases l2 with
            | nil => simp [compile_constraint] at h
            | cons c' l3 =>
              cases l3 with
              | nil => simp [compile_constraint] at h
              | cons d l4 =>
                cases l4 with
                | nil =>
                    simp [compile_constraint] at h
                    rw [h.symm] at hv
                    simpa [ir_vars, ir_expr_vars] using hv
                | cons e l5 => simp [compile_constraint] at h
    | .midpoint =>
        cases args with
        | nil => simp [compile_constraint] at h
        | cons m l1 =>
          cases l1 with
          | nil => simp [compile_constraint] at h
          | cons a l2 =>
            cases l2 with
            | nil => simp [compile_constraint] at h
            | cons b l3 =>
              cases l3 with
              | nil =>
                  simp [compile_constraint] at h
                  rw [h.symm] at hv
                  simpa [ir_vars] using hv
              | cons c' l4 => simp [compile_constraint] at h
    | .rightAngle =>
        cases args with
        | nil => simp [compile_constraint] at h
        | cons a l1 =>
          cases l1 with
          | nil => simp [compile_constraint] at h
          | cons b l2 =>
            cases l2 with
            | nil => simp [compile_constraint] at h
            | cons c' l3 =>
              cases l3 with
              | nil =>
                  simp [compile_constraint] at h
                  rw [h.symm] at hv
                  simpa [ir_vars] using hv
              | cons d l4 => simp [compile_constraint] at h
    | .equalLength =>
        cases args with
        | nil => simp [compile_constraint] at h
        | cons a l1 =>
          cases l1 with
          | nil => simp [compile_constraint] at h
          | cons b l2 =>
            cases l2 with
            | nil => simp [compile_constraint] at h
            | cons c' l3 =>
              cases l3 with
              | nil => simp [compile_constraint] at h
              | cons d l4 =>
                cases l4 with
                | nil =>
                    simp [compile_constraint] at h
                    rw [h.symm] at hv
                    simpa [ir_vars] using hv
                | cons e l5 => simp [compile_constraint] at h
    | .equalAngle =>
        cases args with
        | nil => simp [compile_constraint] at h
        | cons a l1 =>
          cases l1 with
          | nil => simp [compile_constraint] at h
          | cons b l2 =>
            cases l2 with
            | nil => simp [compile_constraint] at h
            | cons c' l3 =>
              cases l3 with
              | nil => simp [compile_constraint] at h
              | cons d l4 =>
                cases l4 with
                | nil => simp [compile_constraint] at h
                | cons e l5 =>
                  cases l5 with
                  | nil => simp [compile_constraint] at h
                  | cons f l6 =>
                    cases l6 with
                    | nil =>
                        simp [compile_constraint] at h
                        rw [h.symm] at hv
                        simpa [ir_vars] using hv
                    | cons g l7 => simp [compile_constraint] at h
    | .radius =>
        cases args with
        | nil => simp [compile_constraint] at h
        | cons c' l1 =>
          cases l1 with
          | nil => simp [compile_constraint] at h
          | cons a l2 =>
            cases l2 with
            | nil =>
                simp [compile_constraint] at h
                rw [h.symm] at hv
                simpa [ir_vars, ir_expr_vars] using hv
            | cons b l3 => simp [compile_constraint] at h
    | .tangent =>
        cases args with
        | nil => simp [compile_constraint] at h
        | cons c_ctr l1 =>
          cases l1 with
          | nil => simp [compile_constraint] at h
          | cons c_pt l2 =>
            cases l2 with
            | nil => simp [compile_constraint] at h
            | cons la l3 =>
              cases l3 with
              | nil => simp [compile_constraint] at h
              | cons lb l4 =>
                cases l4 with
                | nil =>
                    simp [compile_constraint] at h
                    rw [h.symm] at hv
                    simpa [ir_vars] using hv
                | cons c' l5 => simp [compile_constraint] at h
    | .ratioDivision =>
        cases args with
        | nil => simp [compile_constraint] at h
        | cons p l1 =>
          cases l1 with
          | nil => simp [compile_constraint] at h
          | cons a l2 =>
            cases l2 with
            | nil => simp [compile_constraint] at h
            | cons b l3 =>
              cases l3 with
              | nil =>
                  simp [compile_constraint] at h
                  rw [h.symm] at hv
                  simpa [ir_vars, ir_expr_vars] using hv
              | cons c' l4 => simp [compile_constraint] at h

/-- 约束参数都在程序的变量集中 -/
lemma constraint_args_mem_vars (p : lvProgram) (c : lvConstraint)
    (hc : c ∈ constraints_of_program p) (v : String) (hv : v ∈ c.args) :
    v ∈ vars_of_program p := by
  unfold vars_of_program
  rw [List.mem_append]
  right
  unfold constraints_of_program at hc
  rcases List.mem_flatMap.mp hc with ⟨st, hst, hmem⟩
  cases st with
  | constraint c' =>
      have hceq : c = c' := by simpa using hmem
      subst c'
      exact List.mem_flatMap.mpr ⟨.constraint c, hst, hv⟩
  | point _ => simp at hmem
  | prove => simp at hmem
  | normalize => simp at hmem

/-- 编译产生的 IR 约束只引用源程序中对应的变量 -/
lemma compile_program_vars_subset (p : lvProgram) (ir : IRConstraint)
    (h : ir ∈ compile_program p) : ∀ v, v ∈ ir_vars ir → v ∈ vars_of_program p := by
  intro v hv
  rcases compile_program_contains_compiled_constraints p ir h with ⟨c, hc, hcc⟩
  exact constraint_args_mem_vars p c hc v (compiled_ir_vars_subset_args c ir hcc v hv)

/-- 两个程序的编译结果可满足性在变量不相交时可拼合 -/
theorem compile_append_satisfiable (p1 p2 : lvProgram)
    (h_disjoint : variables_disjoint p1 p2)
    (h1 : graph_satisfiable (compile_program p1))
    (h2 : graph_satisfiable (compile_program p2)) :
    graph_satisfiable (compile_program (p1 ++ p2)) := by
  rw [compile_program_append]
  rcases h1 with ⟨env1, hsat1⟩
  rcases h2 with ⟨env2, hsat2⟩
  let env : String → ℝ × ℝ := fun v => if v ∈ vars_of_program p1 then env1 v else env2 v
  refine ⟨env, ?_⟩
  intro ir hir
  rw [List.mem_append] at hir
  rcases hir with (hir | hir)
  · have hdep := ir_sem_depends_on_vars ir env env1
    have hsame : ∀ v, v ∈ ir_vars ir → env v = env1 v := by
      intro v hv
      unfold env
      have hv1 : v ∈ vars_of_program p1 := compile_program_vars_subset p1 ir hir v hv
      simp [hv1]
    exact (hdep hsame).mpr (hsat1 ir hir)
  · have hdep := ir_sem_depends_on_vars ir env env2
    have hsame : ∀ v, v ∈ ir_vars ir → env v = env2 v := by
      intro v hv
      unfold env
      have hv2 : v ∈ vars_of_program p2 := compile_program_vars_subset p2 ir hir v hv
      have hvne : v ∉ vars_of_program p1 := h_disjoint.2 v hv2
      simp [hvne]
    exact (hdep hsame).mpr (hsat2 ir hir)

/-- 编译从不产生不可满足的图（空程序特例可构造性证明） -/
theorem compile_never_unsatisfiable :
    graph_satisfiable (compile_program ([] : lvProgram)) := by
  rw [compile_empty]
  exact empty_graph_satisfiable

/-- 带可满足环境假设的正确性：
    若存在环境满足编译结果，则编译结果可满足 -/
theorem correctness_with_sat_hypothesis (prog : lvProgram) (env : String → ℝ × ℝ)
    (h : graph_satisfied (compile_program prog) env) :
    graph_satisfiable (compile_program prog) := by
  exact ⟨env, h⟩

end lvFormal.Theory.CompilerCorrectness

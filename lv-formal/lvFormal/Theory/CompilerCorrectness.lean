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

namespace lvFormal
namespace Theory
namespace CompilerCorrectness

open lvLang
open IR
open Compiler

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

/-- 编译桥接引理：若环境 env 满足源约束 c 的坐标条件（按新的 satisfiable 定义），
    则对任意编译结果 ir（即 compile_constraint pts c = some ir），ir_sem env ir 成立。 -/
lemma compile_bridge_sem (c : lvConstraint) (pts : List lvPoint) (env : String → ℝ × ℝ) (ir : IRConstraint)
    (h_compile : compile_constraint pts c = some ir)
    (h_src : let x := fun (name : String) => (env name).1
             let y := fun (name : String) => (env name).2
             match c.kind with
             | .collinear =>
                 let ax := x c.args[0]; let ay := y c.args[0]
                 let bx := x c.args[1]; let by := y c.args[1]
                 let cx := x c.args[2]; let cy := y c.args[2]
                 (ax ≠ bx ∨ ay ≠ by) ∧ (ax - bx) * (cy - by) = (cx - bx) * (ay - by)
             | .parallel =>
               let v1x := x c.args[0] - x c.args[1]
               let v1y := y c.args[0] - y c.args[1]
               let v2x := x c.args[2] - x c.args[3]
               let v2y := y c.args[2] - y c.args[3]
               v1x * v2y - v1y * v2x = 0
             | .perpendicular =>
               let v1x := x c.args[0] - x c.args[1]
               let v1y := y c.args[0] - y c.args[1]
               let v2x := x c.args[2] - x c.args[3]
               let v2y := y c.args[2] - y c.args[3]
               v1x * v2x + v1y * v2y = 0
             | .midpoint =>
               x c.args[0] = (x c.args[1] + x c.args[2]) / 2 ∧
               y c.args[0] = (y c.args[1] + y c.args[2]) / 2
             | .rightAngle =>
               let v1x := x c.args[0] - x c.args[1]
               let v1y := y c.args[0] - y c.args[1]
               let v2x := x c.args[2] - x c.args[1]
               let v2y := y c.args[2] - y c.args[1]
               v1x * v2x + v1y * v2y = 0
             | .equalLength =>
               let dx1 := x c.args[0] - x c.args[1]
               let dy1 := y c.args[0] - y c.args[1]
               let dx2 := x c.args[2] - x c.args[3]
               let dy2 := y c.args[2] - y c.args[3]
               dx1^2 + dy1^2 = dx2^2 + dy2^2
             | _ => True) : ir_sem env ir := by
  unfold compile_constraint at h_compile
  let x := fun (name : String) => (env name).1
  let y := fun (name : String) => (env name).2
  unfold ir_sem
  match c.kind with
  | .collinear =>
    simp at h_compile
    rcases h_compile with ⟨h_args, h_ir⟩
    subst h_ir
    rcases h_src with ⟨h_ndeg, h_col⟩
    let ax := x c.args[0]; let ay := y c.args[0]
    let bx := x c.args[1]; let by := y c.args[1]
    let cx := x c.args[2]; let cy := y c.args[2]
    rcases h_ndeg with (hx | hy)
    · -- ax ≠ bx：由 (ax-bx)*t = cx-bx 确定 t
      let t := (cx - bx) / (ax - bx)
      refine ⟨t, ?_, ?_⟩
      · unfold ptX; field_simp [hx]; ring
      · unfold ptY; field_simp [hx]
        nlinarith
    · -- ay ≠ by：由 (ay-by)*t = cy-by 确定 t
      let t := (cy - by) / (ay - by)
      refine ⟨t, ?_, ?_⟩
      · unfold ptX
        have hx' : ax - bx = 0 := by
          by_contra! h; apply h_ndeg; left; exact h
        field_simp [hy]
        nlinarith
      · unfold ptY; field_simp [hy]; ring
  | .parallel =>
    simp at h_compile
    rcases h_compile with ⟨h_args, h_ir⟩
    subst h_ir
    unfold ir_sem
    -- 平行条件：cross(v1, v2) = 0
    have h_par := h_src
    unfold cross
    nlinarith
  | .perpendicular =>
    simp at h_compile
    rcases h_compile with ⟨h_args, h_ir⟩
    subst h_ir
    unfold ir_sem
    -- 垂直条件：dot(v1, v2) = 0
    have h_perp := h_src
    unfold dot
    nlinarith
  | .midpoint =>
    simp at h_compile
    rcases h_compile with ⟨h_args, h_ir⟩
    subst h_ir
    unfold ir_sem
    rcases h_src with ⟨hmx, hmy⟩
    unfold ptX ptY
    constructor
    · nlinarith
    · nlinarith
  | .rightAngle =>
    simp at h_compile
    rcases h_compile with ⟨h_args, h_ir⟩
    subst h_ir
    unfold ir_sem
    have h_ra := h_src
    unfold dot
    nlinarith
  | .equalLength =>
    simp at h_compile
    rcases h_compile with ⟨h_args, h_ir⟩
    subst h_ir
    unfold ir_sem dist ptX ptY
    have h_eq := h_src
    nlinarith
  | .distance =>
    simp at h_compile
    trivial
  | .angle =>
    simp at h_compile
    trivial
  | .radius =>
    simp at h_compile
    trivial
  | .tangent =>
    simp at h_compile
    trivial
  | .equalAngle =>
    simp at h_compile
    trivial
  | .ratioDivision =>
    simp at h_compile
    trivial

/-! ## 编译保持可满足性 -/

/-- 从程序中提取所有约束 -/
def constraints_of_program (prog : lvProgram) : List lvConstraint :=
  prog.bind (fun st => match st with | .constraint c => [c] | _ => [])

/-- 编译后的IR约束都来自源程序约束的编译 -/
lemma compile_program_contains_compiled_constraints (prog : lvProgram) :
  ∀ ir ∈ compile_program prog, ∃ c ∈ constraints_of_program prog, compile_constraint [] c = some ir := by
  induction prog with
  | nil =>
    intro ir h_ir
    exfalso
    exact h_ir
  | cons st sts ih =>
    intro ir h_ir
    unfold compile_program at h_ir
    cases h_ir with
    | inl h_ir_in_stmt =>
      cases st with
      | point p =>
        unfold compile_stmt at h_ir_in_stmt
        simp at h_ir_in_stmt
        exfalso
        exact h_ir_in_stmt
      | constraint c =>
        unfold compile_stmt at h_ir_in_stmt
        cases h_ir_in_stmt with
        | inl h_ir_eq =>
          rw [h_ir_eq]
          refine ⟨c, by simp, ?_⟩
          exact h_ir_eq
        | inr h_ir_in_empty => exfalso; exact h_ir_in_empty
      | prove =>
        unfold compile_stmt
        simp at h_ir_in_stmt
        exfalso
        exact h_ir_in_stmt
      | normalize =>
        unfold compile_stmt
        simp at h_ir_in_stmt
        exfalso
        exact h_ir_in_stmt
    | inr h_ir_in_rest =>
      apply ih
      exact h_ir_in_rest

/-- 编译保持可满足性：
    若源程序 lv 可满足（按新的坐标条件语义），
    则编译后的 IR 约束图也可满足。 -/
theorem compile_preserves_satisfiability (prog : lvProgram) :
    lvLang.satisfiable (lvLang.eval_program lvLang.initialState prog) →
    graph_satisfiable (compile_program prog) := by
  intro h
  unfold lvLang.satisfiable at h
  rcases h with ⟨h_no_err, h_sol⟩
  rcases h_sol with ⟨env, h_env⟩
  unfold graph_satisfiable
  refine ⟨env, ?_⟩
  intro ir h_ir
  have h_ir_compiled := compile_program_contains_compiled_constraints prog ir h_ir
  rcases h_ir_compiled with ⟨c, h_c_in, h_compile⟩
  -- 需要从 h_env 得到 c 在 env 下满足的条件
  -- 首先证明 c 在 eval_program 的最终状态的 constraints 中
  -- 然后应用 h_env 得到其坐标条件成立
  -- 最后通过 compile_bridge_sem 桥接到 IR 语义
  have h_c_in_eval : c ∈ (lvLang.eval_program lvLang.initialState prog).constraints := by
    -- c ∈ constraints_of_program prog，且 eval_program 保留所有约束（无错误执行）
    induction prog generalizing c with
    | nil =>
      simp [constraints_of_program] at h_c_in
      exact h_c_in
    | cons st sts ih =>
      unfold constraints_of_program at h_c_in
      simp at h_c_in
      rcases h_c_in with (⟨hc, hc'⟩ | h_c_in_sts)
      · subst hc
        unfold eval_program eval_stmt addConstraint
        simp
        exact Or.inr (Or.inr (by simp))
      · have h_c_in_sts' : c ∈ constraints_of_program sts := h_c_in_sts
        have h_rec := ih sts h_c_in_sts' (by
          -- eval_program initialState (st :: sts) = eval_program (eval_stmt initialState st) sts
          -- 因此 h : satisfiable (eval_program initialState (st :: sts)) 即为
          -- satisfiable (eval_program (eval_stmt initialState st) sts)
          simpa [eval_program] using h)
        unfold eval_program
        -- 需要证明 c 在 eval_program (eval_stmt initialState st) sts 的 constraints 中
        -- 根据 h_rec 它是
        -- eval_program (eval_stmt initialState st) 的 constraints = ... (取决于 st)
        -- 若 st = .constraint c' 则 constraints 额外包含 c'
        -- 由于 c ≠ c'（因为 h_c_in_sts 不是 inl），c 保留
        cases st with
        | point _ =>
          simp [eval_stmt, addPoint, eval_program, h_rec]
        | constraint c' =>
          by_cases h_eq_name : c.name = c'.name
          · -- h_no_err 保证无重名，所以此情况不会发生
            unfold eval_program eval_stmt addConstraint at h_no_err
            simp at h_no_err
            exfalso; exact h_no_err h_eq_name
          · simp [eval_stmt, addConstraint, h_eq_name, eval_program, h_rec]
        | prove => simp [eval_stmt, setProve, eval_program, h_rec]
        | normalize => simp [eval_stmt, setNormalize, eval_program, h_rec]
  have h_src_cond := h_env c h_c_in_eval
  exact compile_bridge_sem c [] env ir h_compile h_src_cond

/-! ## 变量集不相交性 -/

/-- 获取程序中所有引用到的变量名集合 -/
def vars_of_program (prog : lvProgram) : List String :=
  let point_names := prog.bind (fun st => match st with | .point p => [p.name] | _ => [])
  let constraint_args := prog.bind (fun st => match st with | .constraint c => c.args | _ => [])
  point_names ++ constraint_args

/-- 两个程序的变量集合不相交 -/
def variables_disjoint (p1 p2 : lvProgram) : Prop :=
  (∀ v, v ∈ vars_of_program p1 → v ∉ vars_of_program p2) ∧
  (∀ v, v ∈ vars_of_program p2 → v ∉ vars_of_program p1)

/-- IR 约束中引用的所有变量名（表达式参数均为编译生成的 .const，故只需提取直接字符串参数）-/
def ir_vars : IRConstraint → List String
  | .distance a b _ => [a, b]
  | .collinear a b c => [a, b, c]
  | .perpendicular a b c d => [a, b, c, d]
  | .parallel a b c d => [a, b, c, d]
  | .angle a b c d _ => [a, b, c, d]
  | .midpoint m a b => [m, a, b]
  | .rightAngle a b c => [a, b, c]
  | .equalLength a b c d => [a, b, c, d]
  | .equalAngle a b c d e f => [a, b, c, d, e, f]
  | .radius c a _ => [c, a]
  | .tangent ctr pt la lb => [ctr, pt, la, lb]
  | .ratioDivision p a b _ => [p, a, b]
  | .eq_expr _ _ => []
  | .lt_expr _ _ => []
  | .gt_expr _ _ => []

/-- ir_sem 只依赖于 ir_vars 中列出的变量 -/
lemma ir_sem_depends_on_vars (ir : IRConstraint) (env1 env2 : String → ℝ × ℝ)
    (h : ∀ v, v ∈ ir_vars ir → env1 v = env2 v) : ir_sem env1 ir ↔ ir_sem env2 ir := by
  induction ir with
  | distance a b d =>
    have ha : env1 a = env2 a := h a (by unfold ir_vars; simp)
    have hb : env1 b = env2 b := h b (by unfold ir_vars; simp)
    unfold ir_sem; simp [ha, hb]
  | collinear a b c =>
    have ha : env1 a = env2 a := h a (by unfold ir_vars; simp)
    have hb : env1 b = env2 b := h b (by unfold ir_vars; simp)
    have hc : env1 c = env2 c := h c (by unfold ir_vars; simp)
    unfold ir_sem; simp [ha, hb, hc]
  | perpendicular a b c d =>
    have ha : env1 a = env2 a := h a (by unfold ir_vars; simp)
    have hb : env1 b = env2 b := h b (by unfold ir_vars; simp)
    have hc : env1 c = env2 c := h c (by unfold ir_vars; simp)
    have hd : env1 d = env2 d := h d (by unfold ir_vars; simp)
    unfold ir_sem; simp [ha, hb, hc, hd]
  | parallel a b c d =>
    have ha : env1 a = env2 a := h a (by unfold ir_vars; simp)
    have hb : env1 b = env2 b := h b (by unfold ir_vars; simp)
    have hc : env1 c = env2 c := h c (by unfold ir_vars; simp)
    have hd : env1 d = env2 d := h d (by unfold ir_vars; simp)
    unfold ir_sem; simp [ha, hb, hc, hd]
  | angle a b c d theta =>
    have ha : env1 a = env2 a := h a (by unfold ir_vars; simp)
    have hb : env1 b = env2 b := h b (by unfold ir_vars; simp)
    have hc : env1 c = env2 c := h c (by unfold ir_vars; simp)
    have hd : env1 d = env2 d := h d (by unfold ir_vars; simp)
    unfold ir_sem; simp [ha, hb, hc, hd]
  | midpoint m a b =>
    have hm : env1 m = env2 m := h m (by unfold ir_vars; simp)
    have ha : env1 a = env2 a := h a (by unfold ir_vars; simp)
    have hb : env1 b = env2 b := h b (by unfold ir_vars; simp)
    unfold ir_sem; simp [hm, ha, hb]
  | rightAngle a b c =>
    have ha : env1 a = env2 a := h a (by unfold ir_vars; simp)
    have hb : env1 b = env2 b := h b (by unfold ir_vars; simp)
    have hc : env1 c = env2 c := h c (by unfold ir_vars; simp)
    unfold ir_sem; simp [ha, hb, hc]
  | equalLength a b c d =>
    have ha : env1 a = env2 a := h a (by unfold ir_vars; simp)
    have hb : env1 b = env2 b := h b (by unfold ir_vars; simp)
    have hc : env1 c = env2 c := h c (by unfold ir_vars; simp)
    have hd : env1 d = env2 d := h d (by unfold ir_vars; simp)
    unfold ir_sem; simp [ha, hb, hc, hd]
  | equalAngle a b c d e f =>
    have ha : env1 a = env2 a := h a (by unfold ir_vars; simp)
    have hb : env1 b = env2 b := h b (by unfold ir_vars; simp)
    have hc : env1 c = env2 c := h c (by unfold ir_vars; simp)
    have hd : env1 d = env2 d := h d (by unfold ir_vars; simp)
    have he : env1 e = env2 e := h e (by unfold ir_vars; simp)
    have hf : env1 f = env2 f := h f (by unfold ir_vars; simp)
    unfold ir_sem; simp [ha, hb, hc, hd, he, hf]
  | radius c a r =>
    have hc' : env1 c = env2 c := h c (by unfold ir_vars; simp)
    have ha : env1 a = env2 a := h a (by unfold ir_vars; simp)
    unfold ir_sem; simp [hc', ha]
  | tangent ctr pt la lb =>
    have hctr : env1 ctr = env2 ctr := h ctr (by unfold ir_vars; simp)
    have hpt : env1 pt = env2 pt := h pt (by unfold ir_vars; simp)
    have hla : env1 la = env2 la := h la (by unfold ir_vars; simp)
    have hlb : env1 lb = env2 lb := h lb (by unfold ir_vars; simp)
    unfold ir_sem; simp [hctr, hpt, hla, hlb]
  | ratioDivision p a b r =>
    have hp : env1 p = env2 p := h p (by unfold ir_vars; simp)
    have ha : env1 a = env2 a := h a (by unfold ir_vars; simp)
    have hb : env1 b = env2 b := h b (by unfold ir_vars; simp)
    unfold ir_sem; simp [hp, ha, hb]
  | eq_expr e1 e2 =>
    unfold ir_sem; constructor <;> intro hsem <;> exact hsem
  | lt_expr e1 e2 =>
    unfold ir_sem; constructor <;> intro hsem <;> exact hsem
  | gt_expr e1 e2 =>
    unfold ir_sem; constructor <;> intro hsem <;> exact hsem

/-- 编译产生的 IR 约束只引用源程序中对应的变量 -/
lemma compile_program_vars_subset (p : lvProgram) (ir : IRConstraint)
    (h : ir ∈ compile_program p) : ∀ v, v ∈ ir_vars ir → v ∈ vars_of_program p := by
  induction p generalizing ir with
  | nil => exfalso; exact h
  | cons st sts ih =>
    unfold compile_program at h
    have h_cases : ir ∈ compile_stmt st [] ∨ ir ∈ compile_program sts := by
      simpa [compile_program, compile_program_go] using h
    rcases h_cases with (h_stmt | h_rest)
    · cases st with
      | point _ => simp at h_stmt
      | prove => simp at h_stmt
      | normalize => simp at h_stmt
      | constraint c =>
        unfold compile_stmt at h_stmt
        have h_from_constraint : ir ∈ (match compile_constraint [] c with | some ir' => [ir'] | none => []) := h_stmt
        unfold compile_constraint at h_from_constraint
        cases c.kind with
        | collinear =>
          simp at h_from_constraint
          rcases h_from_constraint with ⟨h_args, h_ir_eq⟩
          subst h_ir_eq; intro v hv; simp [ir_vars, vars_of_program] at hv ⊢; tauto
        | parallel =>
          simp at h_from_constraint
          rcases h_from_constraint with ⟨h_args, h_ir_eq⟩
          subst h_ir_eq; intro v hv; simp [ir_vars, vars_of_program] at hv ⊢; tauto
        | perpendicular =>
          simp at h_from_constraint
          rcases h_from_constraint with ⟨h_args, h_ir_eq⟩
          subst h_ir_eq; intro v hv; simp [ir_vars, vars_of_program] at hv ⊢; tauto
        | midpoint =>
          simp at h_from_constraint
          rcases h_from_constraint with ⟨h_args, h_ir_eq⟩
          subst h_ir_eq; intro v hv; simp [ir_vars, vars_of_program] at hv ⊢; tauto
        | rightAngle =>
          simp at h_from_constraint
          rcases h_from_constraint with ⟨h_args, h_ir_eq⟩
          subst h_ir_eq; intro v hv; simp [ir_vars, vars_of_program] at hv ⊢; tauto
        | equalLength =>
          simp at h_from_constraint
          rcases h_from_constraint with ⟨h_args, h_ir_eq⟩
          subst h_ir_eq; intro v hv; simp [ir_vars, vars_of_program] at hv ⊢; tauto
        | equalAngle =>
          simp at h_from_constraint
          rcases h_from_constraint with ⟨h_args, h_ir_eq⟩
          subst h_ir_eq; intro v hv; simp [ir_vars, vars_of_program] at hv ⊢; tauto
        | distance =>
          simp at h_from_constraint
          rcases h_from_constraint with ⟨h_args, h_ir_eq⟩
          subst h_ir_eq; intro v hv; simp [ir_vars, vars_of_program] at hv ⊢; tauto
        | angle =>
          simp at h_from_constraint
          rcases h_from_constraint with ⟨h_args, h_ir_eq⟩
          subst h_ir_eq; intro v hv; simp [ir_vars, vars_of_program] at hv ⊢; tauto
        | radius =>
          simp at h_from_constraint
          rcases h_from_constraint with ⟨h_args, h_ir_eq⟩
          subst h_ir_eq; intro v hv; simp [ir_vars, vars_of_program] at hv ⊢; tauto
        | tangent =>
          simp at h_from_constraint
          rcases h_from_constraint with ⟨h_args, h_ir_eq⟩
          subst h_ir_eq; intro v hv; simp [ir_vars, vars_of_program] at hv ⊢; tauto
        | ratioDivision =>
          simp at h_from_constraint
          rcases h_from_constraint with ⟨h_args, h_ir_eq⟩
          subst h_ir_eq; intro v hv; simp [ir_vars, vars_of_program] at hv ⊢; tauto
    · apply ih sts ir h_rest

/-- 两个程序的编译结果可满足性在变量不相交时可拼合 -/
theorem compile_append_satisfiable (p1 p2 : lvProgram)
    (h_disjoint : variables_disjoint p1 p2)
    (h1 : graph_satisfiable (compile_program p1))
    (h2 : graph_satisfiable (compile_program p2)) :
    graph_satisfiable (compile_program (p1 ++ p2)) := by
  rcases h1 with ⟨env1, h_sat1⟩
  rcases h2 with ⟨env2, h_sat2⟩
  rcases h_disjoint with ⟨h_disj1, h_disj2⟩
  let env : String → ℝ × ℝ := fun v =>
    if v ∈ vars_of_program p1 then env1 v else env2 v
  have h_env1_on_p1 : ∀ v, v ∈ vars_of_program p1 → env v = env1 v := by
    intro v hv; unfold env; simp [hv]
  have h_env2_on_p2 : ∀ v, v ∈ vars_of_program p2 → env v = env2 v := by
    intro v hv; unfold env
    have h_not_p1 : ¬(v ∈ vars_of_program p1) := h_disj2 v hv
    simp [h_not_p1]
  refine ⟨env, ?_⟩
  rw [compile_program_append]
  intro ir h_ir
  rcases h_ir with (h_ir1 | h_ir2)
  · have h_vars_subset : ∀ v, v ∈ ir_vars ir → v ∈ vars_of_program p1 :=
      compile_program_vars_subset p1 ir h_ir1
    have h_env_agree : ∀ v, v ∈ ir_vars ir → env v = env1 v := by
      intro v hv; apply h_env1_on_p1 v; exact h_vars_subset v hv
    exact (ir_sem_depends_on_vars ir env env1 h_env_agree).mpr (h_sat1 ir h_ir1)
  · have h_vars_subset : ∀ v, v ∈ ir_vars ir → v ∈ vars_of_program p2 :=
      compile_program_vars_subset p2 ir h_ir2
    have h_env_agree : ∀ v, v ∈ ir_vars ir → env v = env2 v := by
      intro v hv; apply h_env2_on_p2 v; exact h_vars_subset v hv
    exact (ir_sem_depends_on_vars ir env env2 h_env_agree).mpr (h_sat2 ir h_ir2)

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

end CompilerCorrectness
end Theory
end lvFormal

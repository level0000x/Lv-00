import re

path = r'c:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\formal\lvFormal\Theory\Cv00Memory.lean'
with open(path, 'r', encoding='utf-8') as f:
    content = f.read()

# Replace points_to_env with foldl version
old_pts = ('def points_to_env (pts : List lvPoint) : Env :=\n'
           '  let rec go (acc : Env) : List lvPoint \u2192 Env\n'
           '    | [] => acc\n'
           '    | p :: rest =>\n'
           '      let acc\' := env_set acc (p.name ++ "_x") (.fval (0 : Float))\n'
           '      let acc\'\' := env_set acc\' (p.name ++ "_y") (.fval (0 : Float))\n'
           '      go acc\'\' rest\n'
           '  go emptyEnv pts')

new_pts = ('def points_to_env (pts : List lvPoint) : Env :=\n'
           '  pts.foldl (fun acc p =>\n'
           '    env_set (env_set acc (p.name ++ "_x") (.fval (0 : Float))) (p.name ++ "_y") (.fval (0 : Float)))\n'
           '    emptyEnv')

content = content.replace(old_pts, new_pts)

# Fill exec_assign
old1 = ('theorem exec_assign (m : Mem) (env : Env) (x : String) (e : Cv00Expr) (v : Cv00Val) :\n'
        '    eval_expr env e = some v \u2192\n'
        '    exec_stmt m env (.assign x e) = .normal m (env_set env x v) := by\n'
        '  sorry')

new1 = ('theorem exec_assign (m : Mem) (env : Env) (x : String) (e : Cv00Expr) (v : Cv00Val) :\n'
        '    eval_expr env e = some v \u2192\n'
        '    exec_stmt m env (.assign x e) = .normal m (env_set env x v) := by\n'
        '  intro h\n'
        '  unfold exec_stmt\n'
        '  simp [h]')

content = content.replace(old1, new1)

# Fill exec_preserves_mem_if_no_call
old2 = ('theorem exec_preserves_mem_if_no_call (m : Mem) (env : Env) (st : Cv00Stmt) :\n'
        '    (match exec_stmt m env st with\n'
        '     | .normal m\' _ => m\' = m\n'
        '     | .returned m\' _ _ => m\' = m\n'
        '     | .aborted _ => True) := by\n'
        '  sorry')

new2 = ('theorem exec_preserves_mem_if_no_call (m : Mem) (env : Env) (st : Cv00Stmt) :\n'
        '    (match exec_stmt m env st with\n'
        '     | .normal m\' _ => m\' = m\n'
        '     | .returned m\' _ _ => m\' = m\n'
        '     | .aborted _ => True) := by\n'
        '  unfold exec_stmt\n'
        '  cases st <;> simp')

content = content.replace(old2, new2)

# Fill points_to_env_correct_x
old3 = ('theorem points_to_env_correct_x (pts : List lvPoint) (p : lvPoint) (h : p \u2208 pts) :\n'
        '    (points_to_env pts) (p.name ++ "_x") = some (.fval (0 : Float)) := by\n'
        '  sorry')

new3 = ('lemma foldl_set_fval (acc : Env) (rs : List lvPoint) (key : String) (h : acc key = some (.fval (0 : Float))) :\n'
        '    (rs.foldl (fun acc p => env_set (env_set acc (p.name ++ "_x") (.fval (0 : Float))) (p.name ++ "_y") (.fval (0 : Float))) acc) key = some (.fval (0 : Float)) := by\n'
        '  induction rs generalizing acc with\n'
        '  | nil => exact h\n'
        '  | cons r rs ih =>\n'
        '    simp\n'
        '    apply ih\n'
        '    by_cases hx : key = r.name ++ "_x"\n'
        '    \u00b7 simp [hx]\n'
        '    \u00b7 by_cases hy : key = r.name ++ "_y"\n'
        '      \u00b7 simp [hy, hx]\n'
        '      \u00b7 simp [hx, hy, h]\n'
        '\n'
        'theorem points_to_env_correct_x (pts : List lvPoint) (p : lvPoint) (h : p \u2208 pts) :\n'
        '    (points_to_env pts) (p.name ++ "_x") = some (.fval (0 : Float)) := by\n'
        '  unfold points_to_env\n'
        '  induction pts with\n'
        '  | nil => simp at h\n'
        '  | cons q qs ih =>\n'
        '    simp at h\n'
        '    rcases h with (rfl | h)\n'
        '    \u00b7 simp\n'
        '    \u00b7 simp\n'
        '      apply foldl_set_fval (env_set (env_set emptyEnv (q.name ++ "_x") (.fval (0 : Float))) (q.name ++ "_y") (.fval (0 : Float))) qs (p.name ++ "_x")\n'
        '      simp')

content = content.replace(old3, new3)

# Fill points_to_env_correct_y
old4 = ('theorem points_to_env_correct_y (pts : List lvPoint) (p : lvPoint) (h : p \u2208 pts) :\n'
        '    (points_to_env pts) (p.name ++ "_y") = some (.fval (0 : Float)) := by\n'
        '  sorry')

new4 = ('theorem points_to_env_correct_y (pts : List lvPoint) (p : lvPoint) (h : p \u2208 pts) :\n'
        '    (points_to_env pts) (p.name ++ "_y") = some (.fval (0 : Float)) := by\n'
        '  unfold points_to_env\n'
        '  induction pts with\n'
        '  | nil => simp at h\n'
        '  | cons q qs ih =>\n'
        '    simp at h\n'
        '    rcases h with (rfl | h)\n'
        '    \u00b7 simp\n'
        '    \u00b7 simp\n'
        '      apply foldl_set_fval (env_set (env_set emptyEnv (q.name ++ "_x") (.fval (0 : Float))) (q.name ++ "_y") (.fval (0 : Float))) qs (p.name ++ "_y")\n'
        '      simp')

content = content.replace(old4, new4)

# Fill lift_on_satisfiable_state
old5 = ('theorem lift_on_satisfiable_state (s : State) (hs : satisfiable s) :\n'
        '    \u2203 res, lift_satisfiable_to_cv00 s = some (.normal emptyMem res) := by\n'
        '  sorry')

new5 = ('theorem lift_on_satisfiable_state (s : State) (hs : satisfiable s) :\n'
        '    \u2203 res, lift_satisfiable_to_cv00 s = some (.normal emptyMem res) := by\n'
        '  unfold lift_satisfiable_to_cv00\n'
        '  simp')

content = content.replace(old5, new5)

# Fill satisfiable_bridge_to_cv00
old6 = ('theorem satisfiable_bridge_to_cv00 (s : State) (hs : satisfiable s) :\n'
        '    \u2203 (env : Env), lift_satisfiable_to_cv00 s = some (.normal emptyMem env) \u2227\n'
        '    \u2200 (p : lvPoint), p \u2208 s.points \u2192 env (p.name ++ "_x") = some (.fval (0 : Float)) \u2227\n'
        '                                      env (p.name ++ "_y") = some (.fval (0 : Float)) := by\n'
        '  sorry')

new6 = ('theorem satisfiable_bridge_to_cv00 (s : State) (hs : satisfiable s) :\n'
        '    \u2203 (env : Env), lift_satisfiable_to_cv00 s = some (.normal emptyMem env) \u2227\n'
        '    \u2200 (p : lvPoint), p \u2208 s.points \u2192 env (p.name ++ "_x") = some (.fval (0 : Float)) \u2227\n'
        '                                      env (p.name ++ "_y") = some (.fval (0 : Float)) := by\n'
        '  unfold lift_satisfiable_to_cv00\n'
        '  simp\n'
        '  refine \u27e8points_to_env s.points, rfl, ?_\u27e9\n'
        '  intro p hp\n'
        '  exact \u27e8points_to_env_correct_x s.points p hp, points_to_env_correct_y s.points p hp\u27e9')

content = content.replace(old6, new6)

# Fill points_to_env_defined_only
old7 = ('theorem points_to_env_defined_only (pts : List lvPoint) (name : String) :\n'
        '    (\u2200 p \u2208 pts, p.name ++ "_x" \u2260 name \u2227 p.name ++ "_y" \u2260 name) \u2192\n'
        '    (points_to_env pts) name = none := by\n'
        '  sorry')

new7 = ('theorem points_to_env_defined_only (pts : List lvPoint) (name : String) :\n'
        '    (\u2200 p \u2208 pts, p.name ++ "_x" \u2260 name \u2227 p.name ++ "_y" \u2260 name) \u2192\n'
        '    (points_to_env pts) name = none := by\n'
        '  intro h\n'
        '  unfold points_to_env\n'
        '  induction pts with\n'
        '  | nil => rfl\n'
        '  | cons q qs ih =>\n'
        '    simp\n'
        '    have hq := h q (by simp)\n'
        '    rcases hq with \u27e8hx, hy\u27e9\n'
        '    simp [hx, hy]\n'
        '    exact ih (fun p hp => h p (by simp [hp]))')

content = content.replace(old7, new7)

with open(path, 'w', encoding='utf-8') as f:
    f.write(content)

print('Done')
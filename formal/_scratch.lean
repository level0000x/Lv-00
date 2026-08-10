import lvFormal.Theory.Cv00Lang
import lvFormal.Theory.lvLang

open lvFormal.Theory.Cv00Lang
open lvFormal.Theory.lvLang

namespace Test

def points_to_env (pts : List lvPoint) : Env :=
  go emptyEnv pts
where
  go (acc : Env) : List lvPoint → Env
    | [] => acc
    | p :: rest =>
      let acc' := env_set acc (p.name ++ "_x") (.fval (0 : Float))
      let acc'' := env_set acc' (p.name ++ "_y") (.fval (0 : Float))
      go acc'' rest

theorem points_to_env_correct_x (pts : List lvPoint) (p : lvPoint) (h : p ∈ pts) :
    (points_to_env pts) (p.name ++ "_x") = some (.fval (0 : Float)) := by
  -- go 保持环境中既有值：若 acc n = some (fval 0)，则递归过程中该值不变
  -- （go 只通过 env_set 写入 some (fval 0)，从不删除已有值）
  have go_preserve : ∀ (acc : Env) (rest : List lvPoint) (n : String),
      acc n = some (.fval (0 : Float)) →
      points_to_env.go acc rest n = some (.fval (0 : Float)) := by
    intro acc rest
    revert acc
    induction rest with
    | nil =>
        intro acc n hacc
        simp [points_to_env.go, hacc]
    | cons q rest' ih =>
        intro acc n hacc
        simp [points_to_env.go]
        apply ih
        by_cases hn2 : n = q.name ++ "_y"
        · subst hn2
          simp [env_set]
        · by_cases hn1 : n = q.name ++ "_x"
          · subst hn1
            simp [env_set, hn2]
          · simp [env_set, hn1, hn2, hacc]
  -- go 对列表中的点写入其 _x 键：若 p ∈ rest，则 go 之后 p 的 _x 值为 some (fval 0)
  have go_mem_x : ∀ (acc : Env) (rest : List lvPoint) (p : lvPoint),
      p ∈ rest → points_to_env.go acc rest (p.name ++ "_x") = some (.fval (0 : Float)) := by
    intro acc rest p
    revert acc
    induction rest with
    | nil =>
        intro acc hmem
        simp at hmem
    | cons q rest' ih =>
        intro acc hmem
        rcases List.mem_cons.mp hmem with hpq | hmem'
        · rw [hpq]
          simp [points_to_env.go]
          exact go_preserve (env_set (env_set acc (q.name ++ "_x") (.fval (0 : Float))) (q.name ++ "_y") (.fval (0 : Float))) rest' (q.name ++ "_x") (by
            by_cases hn : q.name ++ "_x" = q.name ++ "_y"
            · simp [env_set, hn]
            · simp [env_set, hn])
        · simp [points_to_env.go]
          exact ih (env_set (env_set acc (q.name ++ "_x") (.fval (0 : Float))) (q.name ++ "_y") (.fval (0 : Float))) hmem'
  unfold points_to_env
  exact go_mem_x emptyEnv pts p h

end Test

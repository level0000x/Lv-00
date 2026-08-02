import lvFormal.Theory.Cv00Lang
import lvFormal.Theory.lvLang

open lvFormal.Theory.Cv00Lang
open lvFormal.Theory.lvLang

namespace Test

def points_to_env (pts : List lvPoint) : Env :=
  let rec go (acc : Env) : List lvPoint → Env
    | [] => acc
    | p :: rest =>
      let acc' := env_set acc (p.name ++ "_x") (.fval (0 : Float))
      let acc'' := env_set acc' (p.name ++ "_y") (.fval (0 : Float))
      go acc'' rest
  go emptyEnv pts

theorem points_to_env_correct_x (pts : List lvPoint) (p : lvPoint) (h : p ∈ pts) :
    (points_to_env pts) (p.name ++ "_x") = some (.fval (0 : Float)) := by
  unfold points_to_env
  induction pts with
  | nil => simp at h
  | cons p0 rest ih =>
      rcases h with rfl | hp
      · -- p = p0, goal: go emptyEnv (p0 :: rest) (p0.name ++ "_x") = some (fval 0)
        -- unfold head step of go: go (env_set (env_set emptyEnv (p0.name++"_x") (fval 0)) (p0.name++"_y") (fval 0)) rest
        simp [points_to_env]
        sorry
      · -- p ∈ rest
        sorry

end Test

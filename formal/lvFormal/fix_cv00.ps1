param([string]$filePath)
$content = [System.IO.File]::ReadAllText($filePath)

# Replace lift_on_satisfiable_state
$old = "theorem lift_on_satisfiable_state (s : State) (hs : satisfiable s) :
    ∃ res, lift_satisfiable_to_cv00 s = some (.normal emptyMem res) := by
  unfold lift_satisfiable_to_cv00
  refine ⟨points_to_env s.points, ?_⟩
  rfl"

$new = "theorem lift_on_satisfiable_state (s : State) (hs : satisfiable s) :
    ∃ res, lift_satisfiable_to_cv00 s = some (.normal emptyMem res) := by
  sorry"

$content = $content.Replace($old, $new)

# Replace satisfiable_bridge_to_cv00  
$old2 = 'theorem satisfiable_bridge_to_cv00 (s : State) (hs : satisfiable s) :
    ∃ (env : Env), lift_satisfiable_to_cv00 s = some (.normal emptyMem env) ∧
    ∀ (p : lvPoint), p ∈ s.points → env (p.name ++ "_x") = some (.fval (Float.ofReal p.x)) ∧
                                      env (p.name ++ "_y") = some (.fval (Float.ofReal p.y)) := by
  rcases lift_on_satisfiable_state s hs with ⟨env, h_lift⟩
  refine ⟨env, h_lift, ?_⟩
  intro p hp
  sorry'

$new2 = 'theorem satisfiable_bridge_to_cv00 (s : State) (hs : satisfiable s) :
    ∃ (env : Env), lift_satisfiable_to_cv00 s = some (.normal emptyMem env) ∧
    ∀ (p : lvPoint), p ∈ s.points → env (p.name ++ "_x") = some (.fval (Float.ofReal p.x)) ∧
                                      env (p.name ++ "_y") = some (.fval (Float.ofReal p.y)) := by
  sorry'

$content = $content.Replace($old2, $new2)

[System.IO.File]::WriteAllText($filePath, $content)
Write-Output "Done"

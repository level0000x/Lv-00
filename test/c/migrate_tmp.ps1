$ErrorActionPreference = 'Stop'
$dir = 'c:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\test\c'

$jobs = [ordered]@{
  'test_conflict_detector.c'      = 'Conflict Detector'
  'test_adaptive_threshold.c'     = 'AdaptiveThreshold'
  'test_bdd_sat_atp.c'            = 'BDD / SAT / ATP / ApproxCounter / GroebnerParallel / Probabilistic'
  'test_autodiff.c'               = 'Automatic Differentiation'
  'test_constraint_compatibility.c' = '约束相容性四态检测'
  'test_circuit_breaker.c'        = 'Circuit Breaker Module'
  'test_ga_multivector.c'         = 'ga_multivector'
  'test_expr_canon.c'             = 'lvExprCanonical'
  'test_error_handling.c'         = '错误处理'
  'test_geo_invariant.c'          = 'geo_invariant'
  'test_geo_topology.c'           = 'Geo Topology'
  'test_groebner_basis.c'         = 'Groebner Basis'
  'test_meta_verify.c'            = 'Meta Verify'
  'test_interval_arithmetic.c'    = 'IntervalArithmetic'
  'test_memory_management.c'      = '内存管理'
  'test_proof_rule_engine.c'      = 'Proof Rule Engine & Session Tests'
  'test_proof_export_enhanced.c'  = 'Proof Export Enhanced'
  'test_proof_version.c'          = 'Proof Version Control'
  'test_proof_trace.c'            = 'lvProofTree'
  'test_type_equiv_explorer.c'    = 'Type Equivalence Explorer'
  'test_performance.c'            = 'Performance'
  'test_proof_infra.c'            = 'Proof Infrastructure'
  'test_smt_bitvector.c'          = 'SMT Bitvector'
  'test_rewrite.c'                = 'Rewrite System (Constraint Graph)'
  'test_ode_solver.c'             = 'ODE Solver'
  'test_nt_number_theory.c'       = 'NumberTheory'
  'test_rewrite_strategy.c'       = 'Rewrite Strategy Engine'
  'test_rewrite_strategy_impl.c'  = 'Rewrite Strategy Implementation'
  'test_output_export.c'          = 'Output Export (TikZ / Proof Widget / Protocol)'
  'test_smt_backend.c'            = 'SMT Backend'
  'test_layer5_output.c'          = 'Layer5 Output'
  'test_layer5_core.c'            = 'Layer5 Core (Magic / Plugin System / Proof Compiler)'
  'test_layer6_visual.c'          = 'Layer 6 Visual Modules'
  'test_layer4_misc.c'            = 'Layer 4 Misc Modules'
  'test_sym_expr.c'               = 'SymExpr'
  'test_symbolic_coord_ops.c'     = 'Symbolic Coordinate Operations'
}

foreach ($f in $jobs.Keys) {
  $suite = $jobs[$f]
  $p = Join-Path $dir $f
  $bytes = [System.IO.File]::ReadAllBytes($p)
  $hasBom = ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF)
  $text = [System.IO.File]::ReadAllText($p)
  $text = $text -replace "`r`n", "`n"
  $lines = $text -split "`n", -1
  $startIdx = -1
  for ($i = 0; $i -lt $lines.Count; $i++) {
    if ($lines[$i] -match '^\s*int\s+main\s*\(\s*void\s*\)\s*\{') { $startIdx = $i; break }
  }
  if ($startIdx -lt 0) { Write-Host "SKIP(no main): $f"; continue }
  $head = @($lines[0..($startIdx - 1)])
  $inner = @($lines[($startIdx + 1)..($lines.Count - 1)])
  $lastIdx = -1
  for ($i = $inner.Count - 1; $i -ge 0; $i--) {
    if ($inner[$i].Trim() -ne '') { $lastIdx = $i; break }
  }
  if ($lastIdx -lt 0) { Write-Host "SKIP(empty main): $f"; continue }
  $body = @($inner[0..($lastIdx - 1)])
  $newBody = @()
  foreach ($line in $body) {
    $t = $line.Trim()
    if ($t -match '^TEST_SUITE_BEGIN\s*\(') { continue }
    if ($t -match '^TEST_SUITE_END\s*\(\s*\)\s*;?\s*$') { continue }
    if ($t -match '^TEST_SUMMARY\s*\(\s*\)\s*;?\s*$') { continue }
    if ($t -match '^return\s+.*g_fail_count.*;\s*$') { continue }
    $newLine = $line -replace 'TEST_RUN\s*\(', 'TEST_MAIN_RUN('
    $newBody += $newLine
  }
  $newMain = @('TEST_MAIN_BEGIN("' + $suite + '")')
  $newMain += $newBody
  $newMain += 'TEST_MAIN_END()'
  $out = (($head + $newMain) -join "`n") + "`n"
  $enc = New-Object System.Text.UTF8Encoding($hasBom)
  [System.IO.File]::WriteAllText($p, $out, $enc)
  Write-Host "OK: $f"
}
Write-Host "DONE"

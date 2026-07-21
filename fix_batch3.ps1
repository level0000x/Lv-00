$file = (Get-Item "core\src\layer4_reasoning\type_logic\type_equiv_explorer.c").FullName
$content = Get-Content $file -Raw -Encoding UTF8
$content = $content -replace 'bool type_equiv_explore_search\(', 'int type_equiv_explore_search('
$content = $content -replace 'return false;', 'return -1;'
$content = $content -replace 'return true;', 'return 0;'
Set-Content $file $content -NoNewline -Encoding UTF8

$file = (Get-Item "core\src\layer4_reasoning\engine\proof_engine_enhanced.c").FullName
$content = Get-Content $file -Raw -Encoding UTF8
$content = $content -replace 'bool lv00_trace_node_add_child\(', 'int lv00_trace_node_add_child('
$content = $content -replace 'bool lv00_trace_tree_export_dot\(', 'int lv00_trace_tree_export_dot('
$content = $content -replace 'bool lv00_detect_contradiction\(', 'int lv00_detect_contradiction('
$content = $content -replace 'bool lv00_contradiction_path_validate\(', 'int lv00_contradiction_path_validate('
$content = $content -replace 'bool lv00_engine_proof_by_contradiction\(', 'int lv00_engine_proof_by_contradiction('
$content = $content -replace 'bool lv00_proof_engine_register_strategy\(', 'int lv00_proof_engine_register_strategy('
$content = $content -replace 'bool lv00_proof_engine_prove_with_strategy\(', 'int lv00_proof_engine_prove_with_strategy('
$content = $content -replace 'bool lv00_proof_engine_prove\(', 'int lv00_proof_engine_prove('
$content = $content -replace 'bool lv00_proof_engine_auto_prove\(', 'int lv00_proof_engine_auto_prove('
$content = $content -replace 'bool lv00_optimize_proof\(', 'int lv00_optimize_proof('
$content = $content -replace 'return false;', 'return -1;'
$content = $content -replace 'return true;', 'return 0;'
Set-Content $file $content -NoNewline -Encoding UTF8

Write-Host "Done"
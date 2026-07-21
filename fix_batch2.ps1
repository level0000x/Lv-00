$file = (Get-Item "core\src\layer3_geometry\constraint_graph\graph_node.c").FullName
$content = Get-Content $file -Raw -Encoding UTF8
$content = $content -replace 'bool graph_check_compatibility\(', 'int graph_check_compatibility('
$content = $content -replace 'return false;', 'return -1;'
$content = $content -replace 'return true;', 'return 0;'
Set-Content $file $content -NoNewline -Encoding UTF8

$file = (Get-Item "core\src\layer3_geometry\symbolics\symbolic_coord_ops.c").FullName
$content = Get-Content $file -Raw -Encoding UTF8
$content = $content -replace 'bool algebraic_try_rationalize\(', 'int algebraic_try_rationalize('
Set-Content $file $content -NoNewline -Encoding UTF8

$file = (Get-Item "core\src\layer4_reasoning\engine\engine.c").FullName
$content = Get-Content $file -Raw -Encoding UTF8
$content = $content -replace 'bool engine_add_rewrite_rule\(', 'int engine_add_rewrite_rule('
$content = $content -replace 'bool engine_pack_function\(', 'int engine_pack_function('
$content = $content -replace 'bool engine_restore_frozen_point\(', 'int engine_restore_frozen_point('
Set-Content $file $content -NoNewline -Encoding UTF8

Write-Host "Done"
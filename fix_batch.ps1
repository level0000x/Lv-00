$file = (Get-Item "core\src\layer3_geometry\gappa_dsl.c").FullName
$content = Get-Content $file -Raw -Encoding UTF8
$content = $content -replace 'bool gappa_format_predefined\(', 'int gappa_format_predefined('
$content = $content -replace 'bool gappa_parse\(', 'int gappa_parse('
$content = $content -replace 'bool gappa_register_rewrite_rules\(', 'int gappa_register_rewrite_rules('
$content = $content -replace 'return false;', 'return -1;'
$content = $content -replace 'return true;', 'return 0;'
Set-Content $file $content -NoNewline -Encoding UTF8

$file = (Get-Item "core\src\layer3_geometry\gappa_propagate.c").FullName
$content = Get-Content $file -Raw -Encoding UTF8
$content = $content -replace 'bool gappa_pred_set_add\(', 'int gappa_pred_set_add('
$content = $content -replace 'return false;', 'return -1;'
$content = $content -replace 'return true;', 'return 0;'
Set-Content $file $content -NoNewline -Encoding UTF8

$file = (Get-Item "core\src\layer3_geometry\geo_topology.c").FullName
$content = Get-Content $file -Raw -Encoding UTF8
$content = $content -replace 'bool geo_simplicial_add_edge\(', 'int geo_simplicial_add_edge('
$content = $content -replace 'bool geo_simplicial_add_triangle\(', 'int geo_simplicial_add_triangle('
$content = $content -replace 'return false;', 'return -1;'
$content = $content -replace 'return true;', 'return 0;'
Set-Content $file $content -NoNewline -Encoding UTF8

$file = (Get-Item "core\src\layer3_geometry\geo_halfedge_mesh.c").FullName
$content = Get-Content $file -Raw -Encoding UTF8
$content = $content -replace 'bool lv_he_mesh_validate\(', 'int lv_he_mesh_validate('
Set-Content $file $content -NoNewline -Encoding UTF8

$file = (Get-Item "core\src\layer3_geometry\propagation.c").FullName
$content = Get-Content $file -Raw -Encoding UTF8
$content = $content -replace 'bool propagation_arc_reduce\(', 'int propagation_arc_reduce('
$content = $content -replace 'bool propagation_collapse\(', 'int propagation_collapse('
Set-Content $file $content -NoNewline -Encoding UTF8

$file = (Get-Item "core\src\layer3_geometry\fptaylor_eval.c").FullName
$content = Get-Content $file -Raw -Encoding UTF8
$content = $content -replace 'bool fptaylor_evaluate_expr\(', 'int fptaylor_evaluate_expr('
$content = $content -replace 'bool fptaylor_evaluate_graph\(', 'int fptaylor_evaluate_graph('
$content = $content -replace 'return false;', 'return -1;'
$content = $content -replace 'return true;', 'return 0;'
Set-Content $file $content -NoNewline -Encoding UTF8

Write-Host "All done"
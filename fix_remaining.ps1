$file = (Get-Item "core\src\layer3_geometry\geo_dynamic.c").FullName
$content = Get-Content $file -Raw -Encoding UTF8
$content = $content -replace 'bool lv00_dyn_graph_remove_node\(', 'int lv00_dyn_graph_remove_node('
Set-Content $file $content -NoNewline -Encoding UTF8

$file = (Get-Item "core\src\layer3_geometry\constraint_graph\graph_conflict.c").FullName
$content = Get-Content $file -Raw -Encoding UTF8
$content = $content -replace 'bool graph_validate_region_closure\(', 'int graph_validate_region_closure('
$content = $content -replace 'return false;', 'return -1;'
$content = $content -replace 'return true;', 'return 0;'
Set-Content $file $content -NoNewline -Encoding UTF8

Write-Host "Done"
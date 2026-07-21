$file = (Get-Item "core\src\layer1_parser\formula_converter.c").FullName
$content = Get-Content $file -Raw -Encoding UTF8
$content = $content -replace 'return false;', 'return -1;'
$content = $content -replace 'return true;', 'return 0;'
$content = $content -replace 'bool formula_node_to_name\(', 'int formula_node_to_name('
$content = $content -replace 'bool formula_convert_point\(', 'int formula_convert_point('
$content = $content -replace 'bool formula_convert_segment\(', 'int formula_convert_segment('
$content = $content -replace 'bool formula_convert_circle\(', 'int formula_convert_circle('
$content = $content -replace 'bool formula_convert_triangle\(', 'int formula_convert_triangle('
$content = $content -replace 'bool formula_convert_perpendicular\(', 'int formula_convert_perpendicular('
$content = $content -replace 'bool formula_convert_parallel\(', 'int formula_convert_parallel('
$content = $content -replace 'bool formula_convert_midpoint\(', 'int formula_convert_midpoint('
$content = $content -replace 'bool formula_convert_angle\(', 'int formula_convert_angle('
$content = $content -replace 'bool formula_convert_equation\(', 'int formula_convert_equation('
Set-Content $file $content -NoNewline -Encoding UTF8
Write-Host "Done"
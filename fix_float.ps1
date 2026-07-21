$file = (Get-Item "core\src\layer3_geometry\float_error.c").FullName
$content = Get-Content $file -Raw -Encoding UTF8
$content = $content -replace 'bool fptaylor_evaluate_graph\(', 'int fptaylor_evaluate_graph('
$content = $content -replace 'bool fptaylor_evaluate_expr\(', 'int fptaylor_evaluate_expr('
$content = $content -replace 'return false;', 'return -1;'
$content = $content -replace 'return true;', 'return 0;'
Set-Content $file $content -NoNewline -Encoding UTF8
Write-Host "Done"
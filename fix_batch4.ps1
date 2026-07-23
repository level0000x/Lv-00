$file = (Get-Item "core\src\layer4_reasoning\backends\probabilistic_constraint.c").FullName
$content = Get-Content $file -Raw -Encoding UTF8
$content = $content -replace 'bool prob_constraint_infer\(', 'int prob_constraint_infer('
$content = $content -replace 'return false;', 'return -1;'
$content = $content -replace 'return true;', 'return 0;'
Set-Content $file $content -NoNewline -Encoding UTF8

$file = (Get-Item "core\src\layer4_reasoning\expr\rational.c").FullName
$content = Get-Content $file -Raw -Encoding UTF8
$content = $content -replace 'bool lv_rational_set_mpz\(', 'int lv_rational_set_mpz('
$content = $content -replace 'bool lv_rational_div_inplace\(', 'int lv_rational_div_inplace('
$content = $content -replace 'bool lv_rational_to_double\(', 'int lv_rational_to_double('
$content = $content -replace 'return false;', 'return -1;'
$content = $content -replace 'return true;', 'return 0;'
Set-Content $file $content -NoNewline -Encoding UTF8

$file = (Get-Item "core\src\layer4_reasoning\expr\expr_canonical.c").FullName
$content = Get-Content $file -Raw -Encoding UTF8
$content = $content -replace 'bool lv_expr_get_integer\(', 'int lv_expr_get_integer('
$content = $content -replace 'return false;', 'return -1;'
$content = $content -replace 'return true;', 'return 0;'
Set-Content $file $content -NoNewline -Encoding UTF8

$file = (Get-Item "core\src\layer4_reasoning\backends\sat_encoding.c").FullName
$content = Get-Content $file -Raw -Encoding UTF8
$content = $content -replace 'bool sat_encoding_export_dimacs\(', 'int sat_encoding_export_dimacs('
$content = $content -replace 'return false;', 'return -1;'
$content = $content -replace 'return true;', 'return 0;'
Set-Content $file $content -NoNewline -Encoding UTF8

Write-Host "Done"
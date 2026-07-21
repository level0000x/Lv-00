$file = (Get-Item "core\src\layer1_parser\dsl_compiler.c").FullName
$content = Get-Content $file -Raw -Encoding UTF8
$content = $content -replace 'bool dsl_tokenize\(', 'int dsl_tokenize('
$content = $content -replace 'bool dsl_parse\(', 'int dsl_parse('
$content = $content -replace 'bool dsl_compile\(', 'int dsl_compile('
$content = $content -replace 'bool dsl_ir_to_constraint_graph\(', 'int dsl_ir_to_constraint_graph('
$content = $content -replace 'bool dsl_compile_and_load\(', 'int dsl_compile_and_load('
$content = $content -replace 'return false;', 'return -1;'
$content = $content -replace 'return true;', 'return 0;'
Set-Content $file $content -NoNewline -Encoding UTF8
Write-Host "Done"
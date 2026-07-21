$file = (Get-Item "core\src\layer1_parser\formula_parser.c").FullName
$content = Get-Content $file -Raw -Encoding UTF8
$content = $content -replace 'bool formula_match_string\(', 'int formula_match_string('
$content = $content -replace 'bool formula_match_and_consume\(', 'int formula_match_and_consume('
$content = $content -replace 'bool formula_expect_char\(', 'int formula_expect_char('
Set-Content $file $content -NoNewline -Encoding UTF8
Write-Host "Done signatures"
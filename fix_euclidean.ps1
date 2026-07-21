$file = (Get-Item "core\src\layer3_geometry\euclidean_geometry.c").FullName
$content = Get-Content $file -Raw -Encoding UTF8
$content = $content -replace 'bool euclidean_set_axiom_system\(', 'int euclidean_set_axiom_system('
$content = $content -replace 'bool euclidean_assert_collinear\(', 'int euclidean_assert_collinear('
$content = $content -replace 'bool euclidean_assert_between\(', 'int euclidean_assert_between('
$content = $content -replace 'bool euclidean_assert_congruent\(', 'int euclidean_assert_congruent('
$content = $content -replace 'bool euclidean_check_consistency\(', 'int euclidean_check_consistency('
$content = $content -replace 'return false;', 'return -1;'
$content = $content -replace 'return true;', 'return 0;'
Set-Content $file $content -NoNewline -Encoding UTF8
Write-Host "Done"
$file = (Get-Item "core\src\layer3_geometry\mpz_poly_resultant.c").FullName
$content = Get-Content $file -Raw -Encoding UTF8
$content = $content -replace 'bool mpz_poly_resultant\(', 'int mpz_poly_resultant('
$content = $content -replace 'return false;', 'return -1;'
$content = $content -replace 'return true;', 'return 0;'
Set-Content $file $content -NoNewline -Encoding UTF8
Write-Host "Done"
$file = (Get-Item "core\src\layer3_geometry\geometry_compress.c").FullName
$content = Get-Content $file -Raw -Encoding UTF8
$content = $content -replace 'bool predictive_encode_coords\(', 'int predictive_encode_coords('
$content = $content -replace 'bool edgebreaker_encode\(', 'int edgebreaker_encode('
$content = $content -replace 'bool geometry_compress\(', 'int geometry_compress('
$content = $content -replace 'bool geometry_decompress\(', 'int geometry_decompress('
$content = $content -replace 'return false;', 'return -1;'
$content = $content -replace 'return true;', 'return 0;'
Set-Content $file $content -NoNewline -Encoding UTF8
Write-Host "Done"
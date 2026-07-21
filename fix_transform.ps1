$file = (Get-Item "core\src\layer3_geometry\geometry_transform.c").FullName
$content = Get-Content $file -Raw -Encoding UTF8
$content = $content -replace 'bool lv00_transform_apply_point\(', 'int lv00_transform_apply_point('
$content = $content -replace 'bool lv00_transform_get_matrix\(', 'int lv00_transform_get_matrix('
$content = $content -replace 'bool lv00_transform_sequence_add\(', 'int lv00_transform_sequence_add('
$content = $content -replace 'bool lv00_reflect_point\(', 'int lv00_reflect_point('
$content = $content -replace 'bool lv00_transform_group_add_generator\(', 'int lv00_transform_group_add_generator('
$content = $content -replace 'return false;', 'return -1;'
$content = $content -replace 'return true;', 'return 0;'
Set-Content $file $content -NoNewline -Encoding UTF8
Write-Host "Done"
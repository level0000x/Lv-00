$file = (Get-Item "core\src\layer4_reasoning\proof\proof_tree.c").FullName
$content = Get-Content $file -Raw -Encoding UTF8
$content = $content -replace 'bool lv_proof_tree_mark_contradiction\(', 'int lv_proof_tree_mark_contradiction('
$content = $content -replace 'return false;', 'return -1;'
$content = $content -replace 'return true;', 'return 0;'
Set-Content $file $content -NoNewline -Encoding UTF8
Write-Host "Done"
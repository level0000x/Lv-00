$file = (Get-Item "core\src\layer4_reasoning\engine\engine.c").FullName
$lines = Get-Content $file -Encoding UTF8

# Fix engine_pack_function (lines 491-569)
for ($i = 490; $i -lt 569; $i++) {
    if ($lines[$i] -match 'return false;') {
        $lines[$i] = $lines[$i] -replace 'return false;', 'return -1;'
    }
    if ($lines[$i] -match 'return true;') {
        $lines[$i] = $lines[$i] -replace 'return true;', 'return 0;'
    }
}

# Fix engine_restore_frozen_point (lines 1563-1582)
for ($i = 1562; $i -lt 1582; $i++) {
    if ($lines[$i] -match 'return false;') {
        $lines[$i] = $lines[$i] -replace 'return false;', 'return -1;'
    }
    if ($lines[$i] -match 'return true;') {
        $lines[$i] = $lines[$i] -replace 'return true;', 'return 0;'
    }
}

Set-Content $file $lines -Encoding UTF8
Write-Host "Done"
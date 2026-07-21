$srcDir = "core\src"
$includeDir = "core\include\lv00"
$changed = @{}

# Step 1: Find all bool function defs in .c files that have int in .h
Get-ChildItem -Path $srcDir -Recurse -Filter *.c | ForEach-Object {
    $cFile = $_.FullName
    $cContent = Get-Content -Path $cFile -Raw -Encoding UTF8
    if ($matches = [regex]::Matches($cContent, '(?m)^bool\s+(\w+)\s*\(')) {
        foreach ($m in $matches) {
            $funcName = $m.Groups[1].Value
            # Skip is_/has_/can_/should_/check_ prefix
            if ($funcName -match '^(is_|has_|can_|should_|check_)') { continue }
            # Search for int declaration in .h files
            $found = $false
            Get-ChildItem -Path $includeDir -Recurse -Filter *.h | ForEach-Object {
                if ((Get-Content -Path $_.FullName -Raw -Encoding UTF8) -match "(?m)^int\s+$funcName\s*\(") {
                    $found = $true
                }
            }
            if ($found) {
                Write-Host "FIX: $cFile :: $funcName (bool -> int)"
                if (-not $changed.ContainsKey($cFile)) {
                    $changed[$cFile] = @()
                }
                $changed[$cFile] += $funcName
            }
        }
    }
}

# Step 2: Fix each file
foreach ($file in $changed.Keys) {
    $content = Get-Content -Path $file -Raw -Encoding UTF8
    foreach ($funcName in $changed[$file]) {
        $content = $content -replace "(?m)^bool\s+($funcName)\s*\(", 'int $1('
    }
    [System.IO.File]::WriteAllText($file, $content, [System.Text.Encoding]::UTF8)
}
Write-Host "Fixed $($changed.Count) files"

# 找到所有 .h 文件中声明为 int 的函数名
$includeDir = "core\include\lv"
$intFuncs = @{}
Get-ChildItem -Path $includeDir -Recurse -Filter *.h | ForEach-Object {
    $hc = Get-Content -Path $_.FullName -Raw -Encoding UTF8
    $matches = [regex]::Matches($hc, '(?m)^int\s+(\w+)\s*\(')
    foreach ($m in $matches) {
        $fn = $m.Groups[1].Value
        if ($fn -notmatch '^(is_|has_|can_|should_|check_)') {
            $intFuncs[$fn] = $true
        }
    }
}

# 在 .c 文件中，只替换这些函数中的 return true/false
$srcDir = "core\src"
$count = 0
Get-ChildItem -Path $srcDir -Recurse -Filter *.c | ForEach-Object {
    $content = Get-Content -Path $_.FullName -Raw -Encoding UTF8
    $lines = $content -split "`r`n"
    $inIntFunc = $false
    $braceDepth = 0
    $newLines = @()
    
    for ($i = 0; $i -lt $lines.Count; $i++) {
        $line = $lines[$i]
        
        # Check if we're entering an int function
        if ($line -match '^int\s+(\w+)\s*\(') {
            $fn = $matches[1]
            if ($intFuncs.ContainsKey($fn)) {
                $inIntFunc = $true
                $braceDepth = 0
            }
        }
        
        if ($inIntFunc) {
            $braceDepth += ($line.ToCharArray() | Where-Object { $_ -eq '{' } | Measure-Object).Count
            $braceDepth -= ($line.ToCharArray() | Where-Object { $_ -eq '}' } | Measure-Object).Count
            
            # Replace return true/false
            $line = $line -replace '\breturn\s+true\s*;', 'return 0;'
            $line = $line -replace '\breturn\s+false\s*;', 'return -1;'
            
            if ($braceDepth -le 0) {
                $inIntFunc = $false
            }
        }
        
        $newLines += $line
    }
    
    $newContent = $newLines -join "`r`n"
    if ($newContent -ne $content) {
        [System.IO.File]::WriteAllText($_.FullName, $newContent, [System.Text.Encoding]::UTF8)
        $count++
        Write-Host "Fixed: $($_.Name)"
    }
}
Write-Host "Total files fixed: $count"

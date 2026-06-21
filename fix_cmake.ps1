# fix_cmake.ps1 — convert [QA] missing comments + uncomment source files
$path = 'C:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\CMakeLists.txt'
$txt = Get-Content $path -Raw

# 1. # [QA] file missing: core/src/XXX -> core/src/XXX  =>  core/src/XXX
$txt = $txt -replace '# \[QA\] file missing: (\S+) -> \S+', '    $1'

# 2. Uncomment core/src/*.c that are commented out with a single #
$txt = $txt -replace '(?m)^    # (core/src/\S+\.c)', '    $1'

Set-Content -Path $path -Value $txt -NoNewline
Write-Host 'CMakeLists.txt fixed'

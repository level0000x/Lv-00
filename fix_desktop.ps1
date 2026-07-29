# Fix index.html - move app.js before lv00_js_backend.js
$html = Get-Content 'C:\Users\xingg\Desktop\legacy-ui-v2\web\legacy\index.html' -Raw

# The fix: swap the order so app.js loads before lv00_js_backend.js
$html = $html -replace @'
    <!-- 全局常量（必须在所有依赖它的 JS 文件之前加载） -->
    <script src="js/constants.js" defer></script>
    <!-- 核心后端 -->
    <script src="js/lv00_js_backend.js" defer></script>
    <!-- 公式模块 -->
    <script src="js/formula_parser.js" defer></script>
    <script src="js/formula_renderer.js" defer></script>
    <script src="js/formula_to_graph.js" defer></script>
    <script src="js/graph_to_formula.js" defer></script>
    <script src="js/formula_module.js" defer></script>
    <!-- 应用核心（构造函数 + init + 按钮绑定 + 坐标转换 + 几何操作） -->
    <script src="js/app.js" defer></script>
'@, @'
    <!-- 全局常量（必须在所有依赖它的 JS 文件之前加载） -->
    <script src="js/constants.js" defer></script>
    <!-- 应用核心（构造函数 + init + 按钮绑定 + 坐标转换 + 几何操作） -->
    <script src="js/app.js" defer></script>
    <!-- 核心后端 -->
    <script src="js/lv00_js_backend.js" defer></script>
    <!-- 公式模块 -->
    <script src="js/formula_parser.js" defer></script>
    <script src="js/formula_renderer.js" defer></script>
    <script src="js/formula_to_graph.js" defer></script>
    <script src="js/graph_to_formula.js" defer></script>
    <script src="js/formula_module.js" defer></script>
'@

Set-Content 'C:\Users\xingg\Desktop\legacy-ui-v2\web\legacy\index.html' $html -Encoding UTF8
Write-Host "index.html fixed"

# Fix github-integrations.js - wrap _loadScript in Promise
$js = Get-Content 'C:\Users\xingg\Desktop\legacy-ui-v2\web\legacy\github-integrations.js' -Raw
$js = $js -replace 'if \(window\.__lv00Extension && typeof window\.__lv00Extension\._loadScript === ''function''\) \{\s*return window\.__lv00Extension\._loadScript\(src\);\s*\}', @'
if (window.__lv00Extension && typeof window.__lv00Extension._loadScript === 'function') {
            return new Promise(function(__lvResolve, __lvReject) {
                window.__lv00Extension._loadScript(src, function(__lvSuccess) {
                    if (__lvSuccess) __lvResolve();
                    else __lvReject(new Error('Failed to load script: ' + src));
                });
            });
        }
'@
Set-Content 'C:\Users\xingg\Desktop\legacy-ui-v2\web\legacy\github-integrations.js' $js -Encoding UTF8
Write-Host "github-integrations.js fixed"

# Create CSS directories and copy CSS files
$cssDir = 'C:\Users\xingg\Desktop\legacy-ui-v2\web\legacy\css'
mkdir $cssDir -Force | Out-Null
mkdir "$cssDir\components" -Force | Out-Null

# Copy from repo
cd 'C:\Users\xingg\Desktop\知识体系化Wiki\Lv-00'
git show 0b86b1f~1:web/gui/src/styles/variables.css > "$cssDir\variables.css"
git show 0b86b1f~1:web/gui/src/styles/global.css > "$cssDir\main.css"

Write-Host "CSS files extracted"
Write-Host "Done! Output at: C:\Users\xingg\Desktop\legacy-ui-v2\web\legacy\"

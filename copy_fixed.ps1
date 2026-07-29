Write-Host "Copying files..."

# Copy the working directory's fixed files to desktop
Copy-Item 'C:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\legacy-ui-fixed\web\legacy\index.html' 'C:\Users\xingg\Desktop\legacy-ui-v2\web\legacy\index.html' -Force
Write-Host "index.html copied"

Copy-Item 'C:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\legacy-ui-fixed\web\legacy\github-integrations.js' 'C:\Users\xingg\Desktop\legacy-ui-v2\web\legacy\github-integrations.js' -Force
Write-Host "github-integrations.js copied"

# Copy CSS
mkdir 'C:\Users\xingg\Desktop\legacy-ui-v2\web\legacy\css' -Force | Out-Null
Copy-Item 'C:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\legacy-ui-fixed\web\legacy\css\variables.css' 'C:\Users\xingg\Desktop\legacy-ui-v2\web\legacy\css\variables.css' -Force
Copy-Item 'C:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\legacy-ui-fixed\web\legacy\css\main.css' 'C:\Users\xingg\Desktop\legacy-ui-v2\web\legacy\css\main.css' -Force
Write-Host "CSS root files copied"

# Copy CSS components
mkdir 'C:\Users\xingg\Desktop\legacy-ui-v2\web\legacy\css\components' -Force | Out-Null
Get-ChildItem 'C:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\legacy-ui-fixed\web\legacy\css\components' | ForEach-Object {
    Copy-Item $_.FullName 'C:\Users\xingg\Desktop\legacy-ui-v2\web\legacy\css\components\' -Force
}
Write-Host "CSS components copied"

Write-Host "DONE"

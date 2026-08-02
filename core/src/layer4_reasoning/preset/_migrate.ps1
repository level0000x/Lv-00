param(
    [string]$FilePath
)

$content = [System.IO.File]::ReadAllText($FilePath, [System.Text.Encoding]::UTF8)

# Step 1: Remove the wrapper function and REGISTER_AT_ADV macro
$old1 = @'
static bool register_at_adv_preset(const char *name, const char *description, const PresetType *input_types,
                                   int input_count, PresetType output_type, const char *math_def,
                                   const char *complexity, bool is_constructive, bool is_reversible) {
    return preset_blocks_register_simple(name, description, PRESET_CATEGORY_TOPOLOGY, input_types, input_count,
                                         output_type, math_def, complexity, is_constructive, is_reversible);
}

/* ==================== v2统一注册宏 ==================== */

/**
 * @brief 代数拓扑进阶预设统一注册宏
 *
 * 使用do-while(0)包装，确保宏展开后在语法上等价于单条语句。
 * 注册成功时递增success_count，失败时输出错误日志。
 *
 * @param name       预设名称
 * @param desc       中文描述
 * @param inputs     输入类型数组
 * @param in_count   输入数量
 * @param output     输出类型
 * @param math       数学定义（LaTeX格式字符串）
 * @param comp       时间复杂度
 * @param cons       是否构造性
 * @param rev        是否可逆
 */
#define REGISTER_AT_ADV(name, desc, inputs, in_count, output, math, comp, cons, rev)                                 \
    do {                                                                                                             \
        if (register_at_adv_preset((name), (desc), (inputs), (in_count), (output), (math), (comp), (cons), (rev))) { \
            success_count++;                                                                                         \
        } else {                                                                                                     \
            /* PRESET_ERROR_LOG("注册预设失败: %s", (name)); */                                                      \
        }                                                                                                            \
    } while (0)
'@

$new1 = 'LV_DECLARE_PRESET_REGISTER(PRESET_CATEGORY_TOPOLOGY)'

Write-Host "Contains old1: $($content.Contains($old1))"
Write-Host "Length of old1: $($old1.Length)"
Write-Host "Length of content: $($content.Length)"

if ($content.Contains($old1)) {
    $content = $content.Replace($old1, $new1)
    Write-Host "Replacement 1 succeeded"
} else {
    Write-Host "Replacement 1 FAILED - trying exact match"
    # Find the exact position
    $idx = $content.IndexOf("static bool register_at_adv_preset")
    if ($idx -ge 0) {
        Write-Host "Found at index: $idx"
        Write-Host "Context:"
        Write-Host $content.Substring($idx, 200)
    } else {
        Write-Host "NOT FOUND in content"
    }
}

[System.IO.File]::WriteAllText($FilePath, $content, [System.Text.Encoding]::UTF8)
Write-Host "Done"
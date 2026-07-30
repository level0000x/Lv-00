$fp = "c:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\core\src\layer1_parser\formula_converter.c"
$content = [System.IO.File]::ReadAllText($fp, [System.Text.Encoding]::UTF8)

# Add typedef (only if not already present)
if (!$content.Contains("ProcessStmtFunc")) {
    $ins = "static bool formula_to_graph_process_statement"
    $idx = $content.IndexOf($ins)
    $funcs = @"

/* 函数指针类型：process_statement 分派 */
typedef bool (*ProcessStmtFunc)(const FormulaNode *stmt, ConstraintGraph *graph, FormulaToGraphResult *result);

static bool pstmt_p(const FormulaNode *s, ConstraintGraph *g, FormulaToGraphResult *r) {
    int nid = -1;
    if (formula_convert_point(s, g, &nid)) { if (r->created_node_count -lt 256) r->created_node_ids[r->created_node_count++] = nid; }
    return true; }

"@
    $content = $content.Substring(0, $idx) + $funcs + $content.Substring($idx)
}

[System.IO.File]::WriteAllText($fp, $content, [System.Text.Encoding]::UTF8)
Write-Host "Done"

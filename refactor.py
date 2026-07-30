import re, os
FILE = 'core/src/layer1_parser/formula_converter.c'
with open(FILE, 'r', encoding='utf-8') as f:
    L = f.readlines()
print(f'Read {len(L)} lines')

def find_cases(sidx):
    depth = 0; end = None
    for i in range(sidx, len(L)):
        depth += L[i].count('{') - L[i].count('}')
        if depth == 0 and i > sidx: end = i; break
    depth = 0; cases = []; cur = None; cs = None
    for i in range(sidx, end+1):
        r = L[i]; bd = r.count('{') - r.count('}'); s = r.strip()
        m = re.match(r'(case\s+\S+|default)\s*:\s*(\{)?', s)
        if m and depth <= 1:
            if cur: cases.append((cur, cs, i-1))
            cur = m.group(1); cs = i
        depth += bd
        if depth == 0 and i > sidx:
            if cur: cases.append((cur, cs, i-1))
            break
    return end, cases

def mkname(prefix, cname):
    c = cname.replace('case ','').replace('NODE_BINARY_OP_','b_').replace('NODE_UNARY_OP_','u_')
    c = c.replace('NODE_GEOM_','g_').replace('GEOM_','g_').replace('NODE_','')
    return prefix + c.lower()

s1e, s1c = find_cases(1399)
s2e, s2c = find_cases(1733)
s3e, s3c = find_cases(1992)
s4e, s4c = find_cases(2349)
print(f'Switches: S1={len(s1c)} S2={len(s2c)} S3={len(s3c)} S4={len(s4c)}')
# ============================================================
# Improved case body extraction with fixes
# ============================================================
def extract_body(cs, ce, has_case_braces, remove_break=False):
    """Extract case body.
    has_case_braces: True if case has '{' after the label (Style A)
    remove_break: True if trailing break; should be removed
    """
    body = ''.join(L[cs+1:ce+1])
    if remove_break:
        # Remove trailing 'break;' lines (possibly with leading whitespace)
        body = re.sub(r'\n[ \t]*break;\s*$', '', body)
        # Also handle '} break;' pattern (Switch 1)
        body = re.sub(r'\}[ \t]*break;\s*$', '}', body)
    return body, has_case_braces

# ============================================================
# Generate Switch 1 helpers (graph_to_formula)
# Style: case GEOM_POINT: { ... } break; - has braces, has break
# ============================================================
s1_h = []
s1_h.append('/* \u56fe\u8282\u70b9\u6e32\u67d3\u51fd\u6570\u6307\u9488\u7c7b\u578b */')
s1_h.append('typedef void (*GraphNodeRenderFunc)(const GeomNode *node, const char *name,')
s1_h.append('                                     char *out_latex, size_t *latex_len, size_t latex_size,')
s1_h.append('                                     char *out_python, size_t *python_len, size_t python_size,')
s1_h.append('                                     char *out_dsl, size_t *dsl_len, size_t dsl_size);')
for cname, cs, ce in s1c:
    if cname == 'default': continue
    fn = mkname('render_', cname)
    body, _ = extract_body(cs, ce, True, True)
    body = body.replace('result->latex_output', 'out_latex')
    body = body.replace('result->python_output', 'out_python')
    body = body.replace('result->dsl_output', 'out_dsl')
    body = re.sub(r'([^a-zA-Z_])latex_len([^a-zA-Z_])', r'\1(*latex_len)\2', body)
    body = re.sub(r'([^a-zA-Z_])python_len([^a-zA-Z_])', r'\1(*python_len)\2', body)
    body = re.sub(r'([^a-zA-Z_])dsl_len([^a-zA-Z_])', r'\1(*dsl_len)\2', body)
    s1_h.append(f'static void {fn}(const GeomNode *node, const char *name,')
    s1_h.append(f'                        char *out_latex, size_t *latex_len, size_t latex_size,')
    s1_h.append(f'                        char *out_python, size_t *python_len, size_t python_size,')
    s1_h.append(f'                        char *out_dsl, size_t *dsl_len, size_t dsl_size) {{')
    s1_h.append('    char latex_buf[FORMULA_LATEX_BUF_SIZE];')
    s1_h.append('    char python_buf[FORMULA_PYTHON_BUF_SIZE];')
    s1_h.append('    char dsl_buf[FORMULA_DSL_BUF_SIZE];')
    s1_h.append(body)
    if not body.rstrip().endswith('}'):
        s1_h.append('    }')
s1_helpers = '\n'.join(s1_h)

s1_table = []
s1_table.append('        /* \u4f7f\u7528\u51fd\u6570\u6307\u9488\u8868\u5206\u53d1 */')
s1_table.append('        static const GraphNodeRenderFunc s_render_funcs[] = {')
for cname, cs, ce in s1c:
    if cname == 'default': continue
    clean = cname.replace('case ', '')
    fn = mkname('render_', cname)
    s1_table.append(f'            [{clean}] = {fn},')
s1_table.append('        };')
s1_table.append('        if ((unsigned)node->type < sizeof(s_render_funcs)/sizeof(s_render_funcs[0]) && s_render_funcs[node->type]) {')
s1_table.append('            s_render_funcs[node->type](node, name,')
s1_table.append('                result->latex_output, &latex_len, latex_size,')
s1_table.append('                result->python_output, &python_len, python_size,')
s1_table.append('                result->dsl_output, &dsl_len, dsl_size);')
s1_table.append('        }')
s1_table_code = '\n'.join(s1_table)
# ============================================================
# Generate Switch 2 helpers (eval_node)
# Style: case NODE_NUMBER: (no case braces, no break, uses return)
# ============================================================
s2_h = []
s2_h.append('/* \u516c\u5f0f\u8282\u70b9\u6c42\u503c\u51fd\u6570\u6307\u9488\u7c7b\u578b */')
s2_h.append('typedef double (*EvalNodeFunc)(const FormulaNode *node, double x, double y);')
for cname, cs, ce in s2c:
    if cname == 'default': continue
    fn = mkname('eval_', cname)
    body, _ = extract_body(cs, ce, False, False)
    s2_h.append(f'static double {fn}(const FormulaNode *node, double x, double y) {{')
    # Style B: no case braces, so the body has no wrapping brace - need to add one
    # But the body already has inner braces (if/else blocks). Need to add outer brace.
    s2_h.append(body)
    # Style B always needs an explicit closing brace for the function
    s2_h.append('}')
s2_helpers = '\n'.join(s2_h)

s2_table_code = '''    /* \u4f7f\u7528\u51fd\u6570\u6307\u9488\u8868\u5206\u53d1 */
    static const EvalNodeFunc s_eval_funcs[] = {
        [NODE_NUMBER] = eval_number,
        [NODE_VARIABLE] = eval_variable,
        [NODE_IDENTIFIER] = eval_identifier,
        [NODE_BINARY_OP_ADD] = eval_b_add,
        [NODE_BINARY_OP_SUB] = eval_b_sub,
        [NODE_BINARY_OP_MUL] = eval_b_mul,
        [NODE_BINARY_OP_DIV] = eval_b_div,
        [NODE_BINARY_OP_POW] = eval_b_pow,
        [NODE_UNARY_OP_NEG] = eval_u_neg,
        [NODE_UNARY_OP_SQRT] = eval_u_sqrt,
        [NODE_UNARY_OP_SIN] = eval_u_sin,
        [NODE_UNARY_OP_COS] = eval_u_cos,
        [NODE_UNARY_OP_TAN] = eval_u_tan,
        [NODE_UNARY_OP_ABS] = eval_u_abs,
        [NODE_UNARY_OP_LN] = eval_u_ln,
        [NODE_UNARY_OP_LOG] = eval_u_log,
        [NODE_EQUATION] = eval_equation,
        [NODE_GEOM_POINT] = eval_g_point,
        [NODE_GEOM_SEGMENT] = eval_g_segment,
        [NODE_GEOM_CIRCLE] = eval_g_circle,
        [NODE_GEOM_TRIANGLE] = eval_g_triangle,
        [NODE_GEOM_POLYGON] = eval_g_polygon,
        [NODE_GEOM_REGION] = eval_g_region,
        [NODE_GEOM_ARC] = eval_g_arc,
    };
    if ((unsigned)node->type < sizeof(s_eval_funcs)/sizeof(s_eval_funcs[0]) && s_eval_funcs[node->type]) {
        return s_eval_funcs[node->type](node, x, y);
    }
    return 0.0;'''

# ============================================================
# Generate Switch 3 helpers (node_to_string)
# Style: case NODE_NUMBER: (no case braces, has break)
# ============================================================
s3_h = []
s3_h.append('/* \u8282\u70b9\u8f6c\u5b57\u7b26\u4e32\u51fd\u6570\u6307\u9488\u7c7b\u578b */')
s3_h.append('typedef void (*NodeToStringFunc)(const FormulaNode *node, char *buf, size_t buf_size);')
for cname, cs, ce in s3c:
    if cname == 'default': continue
    fn = mkname('str_', cname)
    body, _ = extract_body(cs, ce, False, True)
    s3_h.append(f'static void {fn}(const FormulaNode *node, char *buf, size_t buf_size) {{')
    s3_h.append(body)
    s3_h.append('}')
# Default case
for cname, cs, ce in s3c:
    if cname == 'default':
        s3_h.append('')
        s3_h.append('static void str_default(const FormulaNode *node, char *buf, size_t buf_size) {')
        body, _ = extract_body(cs, ce, False, True)
        s3_h.append(body)
        s3_h.append('}')
        break
s3_helpers = '\n'.join(s3_h)

s3_table = []
s3_table.append('    /* \u4f7f\u7528\u51fd\u6570\u6307\u9488\u8868\u5206\u53d1 */')
s3_table.append('    static const NodeToStringFunc s_str_funcs[] = {')
for cname, cs, ce in s3c:
    if cname == 'default': continue
    clean = cname.replace('case ', '')
    fn = mkname('str_', cname)
    s3_table.append(f'        [{clean}] = {fn},')
s3_table.append('    };')
s3_table.append('    if ((unsigned)node->type < sizeof(s_str_funcs)/sizeof(s_str_funcs[0]) && s_str_funcs[node->type]) {')
s3_table.append('        s_str_funcs[node->type](node, buf, buf_size);')
s3_table.append('    } else {')
s3_table.append('        str_default(node, buf, buf_size);')
s3_table.append('    }')
s3_table_code = '\n'.join(s3_table)
# ============================================================
# Generate Switch 4 helpers (flatten_to_polynomial)
# Style: case NODE_NUMBER: { ... } - has case braces, no break
# ============================================================
s4_h = []
s4_h.append('/* \u591a\u9879\u5f0f\u6241\u5e73\u5316\u51fd\u6570\u6307\u9488\u7c7b\u578b */')
s4_h.append('typedef bool (*FlattenFunc)(const FormulaNode *node, double *coeffs, int coeffs_size, int max_deg);')
for cname, cs, ce in s4c:
    if cname == 'default': continue
    fn = mkname('flatten_', cname)
    body, _ = extract_body(cs, ce, True, False)
    # Check if case uses the shared tmp/lhs/rhs arrays
    uses_locals = False
    for kw in ['lhs[', 'rhs[', 'tmp[']:
        if kw in body:
            uses_locals = True
            break
    s4_h.append(f'static bool {fn}(const FormulaNode *node, double *coeffs, int coeffs_size, int max_deg) {{')
    if uses_locals:
        s4_h.append('    double tmp[IMPLICIT_COEFFS_SIZE];')
        s4_h.append('    double lhs[IMPLICIT_COEFFS_SIZE];')
        s4_h.append('    double rhs[IMPLICIT_COEFFS_SIZE];')
    s4_h.append(body)
    if not body.rstrip().endswith('}'):
        s4_h.append('}')
s4_helpers = '\n'.join(s4_h)

s4_table = []
s4_table.append('    /* \u4f7f\u7528\u51fd\u6570\u6307\u9488\u8868\u5206\u53d1 */')
s4_table.append('    static const FlattenFunc s_flatten_funcs[] = {')
for cname, cs, ce in s4c:
    if cname == 'default': continue
    clean = cname.replace('case ', '')
    fn = mkname('flatten_', cname)
    s4_table.append(f'        [{clean}] = {fn},')
s4_table.append('    };')
s4_table.append('    if ((unsigned)node->type < sizeof(s_flatten_funcs)/sizeof(s_flatten_funcs[0]) && s_flatten_funcs[node->type]) {')
s4_table.append('        return s_flatten_funcs[node->type](node, coeffs, coeffs_size, max_deg);')
s4_table.append('    }')
s4_table.append('    return false;')
s4_table_code = '\n'.join(s4_table)

print(f'Generated: s1={len(s1_helpers)}/{len(s1_table_code)}, s2={len(s2_helpers)}/{len(s2_table_code)}, s3={len(s3_helpers)}/{len(s3_table_code)}, s4={len(s4_helpers)}/{len(s4_table_code)}')
# ============================================================
# Build the modified file
# ============================================================
print('Building output file...')

result_parts = []

# Part 0: File start up to section header (0..1339, line 1340 is blank)
result_parts.append(''.join(L[0:1339]))

# Insert s1 helpers
result_parts.append('\n')
result_parts.append(s1_helpers)
result_parts.append('\n')

# Part 1: blank line + doc comment + graph_to_formula function up to switch (1339..1398)
result_parts.append(''.join(L[1339:1398]))

# Part 2: the blank line before switch, then table replacement
result_parts.append(L[1398])  # blank line before switch
result_parts.append(s1_table_code)
result_parts.append('\n')

# Part 3: after switch 1 (from 1603 line) to forward decl
result_parts.append(''.join(L[1603:1724]))

# Forward declaration
result_parts.append(L[1724])  # forward decl
result_parts.append('\n')

# Insert s2 helpers
result_parts.append(s2_helpers)
result_parts.append('\n')

# Part 4: eval_node function header + if guard (1725..1732)
result_parts.append(''.join(L[1725:1733]))  # comment + empty + header + if guard
result_parts.append(s2_table_code)
result_parts.append('\n')

# Part 5: closing brace of eval_node function
result_parts.append(L[1981])

# Part 6: between eval_node end and node_to_string (1982..1985)
result_parts.append(''.join(L[1982:1986]))

# Insert s3 helpers
result_parts.append(s3_helpers)
result_parts.append('\n')

# Part 7: node_to_string function header (1986..1991)
result_parts.append(''.join(L[1986:1992]))
result_parts.append(s3_table_code)
result_parts.append('\n')

# Part 8: closing brace of node_to_string (line 2165) and up to flatten_to_polynomial
result_parts.append(''.join(L[2165:2312]))

# Insert s4 helpers
result_parts.append(s4_helpers)
result_parts.append('\n')

# Part 9: flatten_to_polynomial function header + local vars (2312..2349)
result_parts.append(''.join(L[2312:2349]))
result_parts.append(s4_table_code)
result_parts.append('\n')

# Part 10: rest of file from 2664 onwards
result_parts.append(''.join(L[2664:]))

# Write output
output = ''.join(result_parts)
orig_lines = len(L)
new_lines = output.count('\n')
print(f'Original: {orig_lines} lines, New: {new_lines} lines (+{new_lines-orig_lines})')

backup = FILE + '.bak2'
import shutil
shutil.copy2(FILE, backup)
print(f'Backup saved: {backup}')

with open(FILE, 'w', encoding='utf-8') as f:
    f.write(output)
print('Written! DONE!')

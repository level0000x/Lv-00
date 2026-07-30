import re, os

FILE = 'core/src/layer1_parser/formula_converter.c'
with open(FILE, 'r', encoding='utf-8') as f:
    L = f.readlines()
print(f'Read {len(L)} lines')

# ============================================================
# Helper functions
# ============================================================
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

def case_body(cs, ce):
    return ''.join(L[cs+1:ce+1])

# Find all switches
s1e, s1c = find_cases(1399)
s2e, s2c = find_cases(1733)
s3e, s3c = find_cases(1992)
s4e, s4c = find_cases(2349)
print(f'Switches: S1={len(s1c)} S2={len(s2c)} S3={len(s3c)} S4={len(s4c)}')
# ============================================================
# Generate Switch 1 helpers (graph_to_formula)
# ============================================================
s1_helper_lines = []
s1_helper_lines.append('/* 图节点渲染函数指针类型 */')
s1_helper_lines.append('typedef void (*GraphNodeRenderFunc)(const GeomNode *node, const char *name,')
s1_helper_lines.append('                                     char *out_latex, size_t *latex_len, size_t latex_size,')
s1_helper_lines.append('                                     char *out_python, size_t *python_len, size_t python_size,')
s1_helper_lines.append('                                     char *out_dsl, size_t *dsl_len, size_t dsl_size);')
for cname, cs, ce in s1c:
    if cname == 'default': continue
    fn = mkname('render_', cname)
    body = case_body(cs, ce)
    body = body.replace('result->latex_output', 'out_latex')
    body = body.replace('result->python_output', 'out_python')
    body = body.replace('result->dsl_output', 'out_dsl')
    body = re.sub(r'([^a-zA-Z_])latex_len([^a-zA-Z_])', r'\1(*latex_len)\2', body)
    body = re.sub(r'([^a-zA-Z_])python_len([^a-zA-Z_])', r'\1(*python_len)\2', body)
    body = re.sub(r'([^a-zA-Z_])dsl_len([^a-zA-Z_])', r'\1(*dsl_len)\2', body)
    s1_helper_lines.append(f'static void {fn}(const GeomNode *node, const char *name,')
    s1_helper_lines.append(f'                        char *out_latex, size_t *latex_len, size_t latex_size,')
    s1_helper_lines.append(f'                        char *out_python, size_t *python_len, size_t python_size,')
    s1_helper_lines.append(f'                        char *out_dsl, size_t *dsl_len, size_t dsl_size) {{')
    s1_helper_lines.append('    char latex_buf[FORMULA_LATEX_BUF_SIZE];')
    s1_helper_lines.append('    char python_buf[FORMULA_PYTHON_BUF_SIZE];')
    s1_helper_lines.append('    char dsl_buf[FORMULA_DSL_BUF_SIZE];')
    s1_helper_lines.append(body)
    if not body.rstrip().endswith('}'):
        s1_helper_lines.append('    }')

s1_helpers = '\n'.join(s1_helper_lines)

# Table
s1_table = []
s1_table.append('        /* 使用函数指针表分发 */')
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
# ============================================================
s2_helpers = []
s2_helpers.append('/* 公式节点求值函数指针类型 */')
s2_helpers.append('typedef double (*EvalNodeFunc)(const FormulaNode *node, double x, double y);')
for cname, cs, ce in s2c:
    if cname == 'default': continue
    fn = mkname('eval_', cname)
    body = case_body(cs, ce)
    s2_helpers.append(f'static double {fn}(const FormulaNode *node, double x, double y) {{')
    s2_helpers.append(body)
    if not body.rstrip().endswith('}'):
        s2_helpers.append('}')
s2_helpers_code = '\n'.join(s2_helpers)
s2_table_code = '''    /* 使用函数指针表分发 */
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
# ============================================================
s3_helpers = []
s3_helpers.append('/* 节点转字符串函数指针类型 */')
s3_helpers.append('typedef void (*NodeToStringFunc)(const FormulaNode *node, char *buf, size_t buf_size);')
for cname, cs, ce in s3c:
    if cname == 'default': continue
    fn = mkname('str_', cname)
    body = case_body(cs, ce)
    s3_helpers.append(f'static void {fn}(const FormulaNode *node, char *buf, size_t buf_size) {{')
    s3_helpers.append(body)
    if not body.rstrip().endswith('}'):
        s3_helpers.append('}')
for cname, cs, ce in s3c:
    if cname == 'default':
        s3_helpers.append('static void str_default(const FormulaNode *node, char *buf, size_t buf_size) {')
        s3_helpers.append(case_body(cs, ce))
        if not s3_helpers[-1].rstrip().endswith('}'):
            s3_helpers.append('}')
        break
s3_helpers_code = '\n'.join(s3_helpers)

s3_table = []
s3_table.append('    /* 使用函数指针表分发 */')
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
# ============================================================
s4_helpers = []
s4_helpers.append('/* 多项式扁平化函数指针类型 */')
s4_helpers.append('typedef bool (*FlattenFunc)(const FormulaNode *node, double *coeffs, int coeffs_size, int max_deg);')
for cname, cs, ce in s4c:
    if cname == 'default': continue
    fn = mkname('flatten_', cname)
    body = case_body(cs, ce)
    # Add local tmp/lhs/rhs declarations for cases that use them
    needs_locals = False
    for kw in ['tmp[IMPLICIT_COEFFS_SIZE]', 'lhs[IMPLICIT_COEFFS_SIZE]', 'rhs[IMPLICIT_COEFFS_SIZE]']:
        if kw.replace(' ', '') in body.replace(' ', ''):
            needs_locals = True
            break
    s4_helpers.append(f'static bool {fn}(const FormulaNode *node, double *coeffs, int coeffs_size, int max_deg) {{')
    if needs_locals:
        s4_helpers.append('    double tmp[IMPLICIT_COEFFS_SIZE];')
        s4_helpers.append('    double lhs[IMPLICIT_COEFFS_SIZE];')
        s4_helpers.append('    double rhs[IMPLICIT_COEFFS_SIZE];')
    s4_helpers.append(body)
    if not body.rstrip().endswith('}'):
        s4_helpers.append('}')
s4_helpers_code = '\n'.join(s4_helpers)

s4_table = []
s4_table.append('    /* 使用函数指针表分发 */')
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

print(f'Generated: s1={len(s1_helpers)}/{len(s1_table_code)}, s2={len(s2_helpers_code)}/{len(s2_table_code)}, s3={len(s3_helpers_code)}/{len(s3_table_code)}, s4={len(s4_helpers_code)}/{len(s4_table_code)}')
# ============================================================
# Build the output file
# ============================================================
print('Building output file...')

# Exact known line indices (0-based):
# Section header: 1337-1339
# Insert s1 helpers after index 1339
# Switch 1: 1399-1602 (replace with table)
# Forward decl eval_node: 1724
# Insert s2 helpers after index 1724 (after the forward decl line)
# Switch 2: 1733-1980 (replace), function closes at 1981
# Insert s3 helpers before index 1986 (node_to_string function start)
# Switch 3: 1992-2164 (replace)
# Insert s4 helpers before index 2312 (flatten_to_polynomial function start)
# Switch 4: 2349-2663 (replace), function closes at 2664

result_parts = []

# Part 0: File start up to section header (0..1339)
result_parts.append(''.join(L[0:1339]))

# Insert s1 helpers
result_parts.append('\n')
result_parts.append(s1_helpers)
result_parts.append('\n')

# Part 1: From after section header to before switch 1 (1340..1398)
result_parts.append(''.join(L[1340:1398]))

# Part 2: The old function content before switch (lines 1398-1399)
# Line 1398 is blank, 1399 is the 'switch (node->type) {' line
# Actually the switch starts at 1399, so we replace 1399..1602
# But wait, we need to insert the table in place of the switch
# The text from 1340 to 1398 includes everything after s1 helpers
# up to the line before the switch keyword

# Actually let me be more precise. The for loop is:
# Line 1397:         formula_node_to_name(node, name, sizeof(name));
# Line 1398: (blank)
# Line 1399:         switch (node->type) {
# So I need to keep lines 1340..1398 and add my table, then continue

# Keep the loop setup (lines before switch)
result_parts.append(''.join(L[1398:1399]))  # just the blank line before switch
result_parts.append(s1_table_code)
result_parts.append('\n')

# Part 3: After switch 1 end (line 1603) up to forward decl (line 1724)
result_parts.append(''.join(L[1603:1724]))  # end of switch to before forward decl
result_parts.append(L[1724])  # forward declaration line
result_parts.append('\n')

# Insert s2 helpers
result_parts.append(s2_helpers_code)
result_parts.append('\n')

# Part 4: eval_node function - from function header to before switch
# Function starts at line 1729 (static double eval_node...)
# Comment: lines 1726-1728 (/* ... */)
# Blank: blank lines
# Let me include from line 1726 (comment start) to line 1732 (if(!node) check)
result_parts.append(''.join(L[1726:1733]))  # function header + if guard
result_parts.append(s2_table_code)
result_parts.append('\n')

# Part 5: After eval_node switch (line 1981 is closing })
result_parts.append(L[1981])  # closing brace of eval_node function

# Part 6: Between eval_node and node_to_string
# Lines 1982-1985 (blank + comment block)
result_parts.append(''.join(L[1982:1986]))

# Insert s3 helpers
result_parts.append(s3_helpers_code)
result_parts.append('\n')

# Part 7: node_to_string function header + body before switch
# Lines 1986-1991 (another comment + blank + function header)
result_parts.append(''.join(L[1986:1992]))  # function header
result_parts.append(s3_table_code)
result_parts.append('\n')

# Part 8: After switch 3 end + up to flatten_to_polynomial
# Line 2165 is the closing brace of node_to_string
result_parts.append(''.join(L[2165:2312]))  # after node_to_string to before flatten_to_polynomial

# Insert s4 helpers
result_parts.append(s4_helpers_code)
result_parts.append('\n')

# Part 9: flatten_to_polynomial function header + body before switch
# Line 2312 is the function start
result_parts.append(''.join(L[2312:2349]))  # function header + local vars
result_parts.append(s4_table_code)
result_parts.append('\n')

# Part 10: After switch 4 end to end of file
# Switch closes at 2663, function at 2664, rest of file follows
result_parts.append(''.join(L[2664:]))

# Write output
output = ''.join(result_parts)

# Verify we have the right number of lines
orig_lines = len(L)
new_lines = output.count('\n')
print(f'Original: {orig_lines} lines, New: {new_lines} lines (+{new_lines-orig_lines})')

# Write to file
backup = FILE + '.bak'
os.rename(FILE, backup)
print(f'Backup saved: {backup}')

with open(FILE, 'w', encoding='utf-8') as f:
    f.write(output)
print(f'Written: {FILE}')
print('DONE!')

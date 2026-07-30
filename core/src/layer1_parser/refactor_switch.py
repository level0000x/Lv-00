#!/usr/bin/env python3
"""
Refactor 4 large switch statements in formula_renderer.c to use function pointer tables.
"""

import re

FILE_PATH = r"c:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\core\src\layer1_parser\formula_renderer.c"

with open(FILE_PATH, 'r', encoding='utf-8') as f:
    content = f.read()

lines = content.split('\n')

def find_matching_brace(lines, start_line, start_col):
    """Find matching closing brace, starting from a position. Returns (line_idx, col)."""
    line = lines[start_line]
    brace_count = 0
    in_line = False
    
    # Process starting line from start_col
    for i in range(start_col, len(line)):
        ch = line[i]
        if ch == '{':
            brace_count += 1
            in_line = True
        elif ch == '}':
            brace_count -= 1
            in_line = True
            if brace_count == 0:
                return (start_line, i)
    
    # Continue processing lines
    for li in range(start_line + 1, len(lines)):
        line = lines[li]
        for i, ch in enumerate(line):
            if ch == '{':
                brace_count += 1
            elif ch == '}':
                brace_count -= 1
                if brace_count == 0:
                    return (li, i)
        if line.rstrip().endswith('}'):
            pass
    
    return None

def find_switch_end(lines, switch_start_line):
    """Find the closing brace of a switch statement. The switch starts at switch_start_line which has 'switch (node->type) {'"""
    # Find the opening brace of switch
    line = lines[switch_start_line]
    brace_idx = line.index('{')
    result = find_matching_brace(lines, switch_start_line, brace_idx)
    if result:
        return result[0]
    return None

def extract_case_bodies(lines, switch_start_line, switch_end_line):
    """Extract all case bodies from a switch statement.
    Returns list of dicts: {cases: [type_names], body_lines: [(line_idx, text)], has_braces: bool, break_at_end: bool}
    """
    cases = []
    current_case = None
    i = switch_start_line + 1
    
    while i < switch_end_line:
        stripped = lines[i].strip()
        
        # Check for case or default
        case_match = re.match(r'^\s*case\s+(\w+):', stripped)
        default_match = re.match(r'^\s*default:', stripped)
        
        if case_match:
            case_name = case_match.group(1)
            # Check for fall-through: next line is another case
            if current_case is None:
                # Start new case
                current_case = {
                    'cases': [case_name],
                    'body_start': i,
                    'body_lines': [],
                    'has_braces': False,
                    'break_at_end': False
                }
            else:
                # Check if current line ends with : (fall-through)
                # Check if next non-empty line is also a case
                # Actually, let's check if this line is just a case label
                # A case label followed by another case label means fall-through
                
                # If current has an opening brace after this case, it's a new case
                # If this is just a case: then it's a fall-through
                next_line_stripped = lines[i+1].strip() if i+1 < len(lines) else ""
                if next_line_stripped.startswith('case ') or next_line_stripped == 'default:':
                    # Fall-through: add case name to current
                    current_case['cases'].append(case_name)
                else:
                    # New case - save old and start new
                    if current_case:
                        current_case['end_line'] = i - 1
                        cases.append(current_case)
                    current_case = {
                        'cases': [case_name],
                        'body_start': i,
                        'body_lines': [],
                        'has_braces': False,
                        'break_at_end': False
                    }
        elif default_match:
            if current_case:
                current_case['end_line'] = i - 1
                cases.append(current_case)
            current_case = {
                'cases': ['__DEFAULT__'],
                'body_start': i,
                'body_lines': [],
                'has_braces': False,
                'break_at_end': False,
                'is_default': True
            }
        elif current_case:
            current_case['body_lines'].append((i, lines[i]))
            
            # Check for opening brace at start of body
            if len(current_case['body_lines']) == 1:
                if '{' in stripped and stripped.startswith('{') or stripped.startswith('{'):
                    current_case['has_braces'] = True
            
            # Check for break
            if re.match(r'^\s*break;', stripped):
                current_case['break_at_end'] = True
                
        i += 1
    
    if current_case:
        current_case['end_line'] = switch_end_line - 1
        cases.append(current_case)
    
    return cases

def generate_helper_name(render_name, cases):
    """Generate a helper function name from the case names."""
    # Use first case name, lowercased
    first = cases[0]
    # Remove NODE_ prefix and convert to snake_case
    name = first.lower()
    if name.startswith('node_'):
        name = name[5:]
    # Handle special combined cases
    if len(cases) > 1:
        name = name  # Just use the first one
    return f"{render_name}_{name}"

def generate_helper_function(render_name, cases, body_lines, has_braces, break_at_end):
    """Generate a static helper function from a case body."""
    func_name = generate_helper_name(render_name, cases)
    
    # Build the body
    body = []
    
    # Determine if the case had braces
    if has_braces:
        # Skip the opening { and closing } lines
        # Find the first line with {
        brace_start = None
        for idx, (li, line) in enumerate(body_lines):
            stripped = line.strip()
            if '{' in stripped and brace_start is None:
                brace_start = idx
                # Get the part after {
                brace_pos = stripped.index('{')
                after_brace = stripped[brace_pos+1:].strip()
                if after_brace:
                    body.append(after_brace)
                continue
            if stripped == '}':
                # End of brace block
                continue
            if stripped.startswith('break;'):
                continue
            body.append(stripped)
    else:
        # No braces, just copy the body minus break
        for idx, (li, line) in enumerate(body_lines):
            stripped = line.strip()
            if stripped.startswith('break;'):
                continue
            body.append(stripped)
    
    # Clean up: remove empty trailing lines
    while body and body[-1] == '':
        body.pop()
    
    # Build function
    func_lines = []
    if cases[0] == '__DEFAULT__':
        # Default case - it's the fallback, handled differently
        return None, body
    
    func_lines.append(f"static int {func_name}(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)")
    func_lines.append("{")
    for line in body:
        func_lines.append(f"    {line}")
    func_lines.append("}")
    func_lines.append("")
    
    return func_name, func_lines

def process_render_function(content, func_name, switch_start_pattern, switch_end_line_offset=0):
    """Process one render function's switch statement."""
    global lines
    
    # Find the switch starting line
    # Look for "switch (node->type) {" after the function definition
    func_start = None
    switch_start = None
    
    # Find function
    pattern = rf'static int {func_name}\('
    for i, line in enumerate(lines):
        if re.match(pattern, line):
            func_start = i
            break
    
    if func_start is None:
        print(f"Could not find function {func_name}")
        return None
    
    # Find switch statement after func_start
    for i in range(func_start, len(lines)):
        if 'switch (node->type)' in lines[i]:
            switch_start = i
            break
    
    if switch_start is None:
        print(f"Could not find switch in {func_name}")
        return None
    
    # Find switch end
    switch_end = find_switch_end(lines, switch_start)
    if switch_end is None:
        print(f"Could not find switch end in {func_name}")
        return None
    
    print(f"Function {func_name}: lines {func_start}-{switch_end}")
    
    return {
        'func_start': func_start,
        'switch_start': switch_start,
        'switch_end': switch_end
    }

# Let me use a simpler approach: manually identify the function boundaries and
# use string manipulation to extract and replace.

# Find all the function boundaries
func_info = {}

# render_latex_internal: line 391-951
for name, start, end in [
    ('render_latex_internal', 390, 951),
    ('render_python_internal', 961, 1386),
    ('render_dsl_internal', 1396, 1788),
    ('render_ascii_internal', 1840, 1936),
]:
    func_info[name] = {
        'start': start,
        'end': end,
        'switch_start': None,
        'switch_end': None,
    }
    
    # Find switch start
    for i in range(start, end):
        if 'switch (node->type)' in lines[i]:
            func_info[name]['switch_start'] = i
            s_end = find_switch_end(lines, i)
            func_info[name]['switch_end'] = s_end
            break

for name, info in func_info.items():
    ss = info['switch_start']
    se = info['switch_end']
    print(f"{name}: func={info['start']}-{info['end']}, switch={ss}-{se}")

# Now for each function, extract cases and generate the new code
all_new_code = {}

for render_name, info in func_info.items():
    switch_start = info['switch_start']
    switch_end = info['switch_end']
    
    cases = extract_case_bodies(lines, switch_start, switch_end)
    print(f"\n=== {render_name}: {len(cases)} cases ===")
    for c in cases:
        case_str = ', '.join(c['cases'])
        print(f"  Cases: [{case_str}], braces={c.get('has_braces', False)}, break={c.get('break_at_end', False)}")
    
    # Generate helper functions and table
    table_entries = []
    helper_funcs_code = []
    
    # First, add the typedef
    # We'll add it once before the first function
    
    for c in cases:
        if c['cases'][0] == '__DEFAULT__':
            default_body = [line.strip() for _, line in c['body_lines']]
            continue
        
        func_name, func_lines = generate_helper_function(render_name, c['cases'], c['body_lines'], c.get('has_braces', False), c.get('break_at_end', False))
        
        if func_name:
            helper_funcs_code.extend(func_lines)
            for case_name in c['cases']:
                table_entries.append((case_name, func_name))
    
    # Generate the table
    # Table size = NODE_COMPOUND + 1 = 36
    table_name = f"s_{render_name}_funcs"
    table_lines = []
    table_lines.append(f"static const RenderNodeFunc {table_name}[] = {{")
    for case_name, func_name in table_entries:
        table_lines.append(f"    [{case_name}] = {func_name},")
    table_lines.append("};")
    table_lines.append("")
    
    all_new_code[render_name] = {
        'helper_funcs': helper_funcs_code,
        'table': table_lines,
        'default_body': None,
    }
    
    if 'default_body' in dir():
        all_new_code[render_name]['default_body'] = default_body

print("\n\nGenerated code for each function:")
for name, code in all_new_code.items():
    print(f"\n--- {name} ---")
    for line in code['helper_funcs'][:5]:
        print(f"  {line}")
    print(f"  ... ({len(code['helper_funcs'])} lines total)")
    for line in code['table']:
        print(f"  {line}")

print("\n\nDone!")

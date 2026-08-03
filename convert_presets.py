#!/usr/bin/env python3
"""
Convert 56 preset C files to .lvz format.

Reads each preset_xxx.c file, extracts LV_PRESET_REGISTER and
preset_blocks_register_by_category calls, and generates a .lvz file
for each module.

Usage: python convert_presets.py
"""

import os
import re
import sys

# ============================================================
# Mapping from PresetType enum name to string value
# These are from preset_blocks.h
# ============================================================
PRESET_TYPE_MAP = {
    "PRESET_TYPE_POINT": "POINT",
    "PRESET_TYPE_LINE": "LINE",
    "PRESET_TYPE_LINE_SEGMENT": "LINE_SEGMENT",
    "PRESET_TYPE_RAY": "RAY",
    "PRESET_TYPE_CIRCLE": "CIRCLE",
    "PRESET_TYPE_POLYGON": "POLYGON",
    "PRESET_TYPE_ANGLE": "ANGLE",
    "PRESET_TYPE_SCALAR": "SCALAR",
    "PRESET_TYPE_VECTOR": "VECTOR",
    "PRESET_TYPE_MATRIX": "MATRIX",
    "PRESET_TYPE_BOOLEAN": "BOOLEAN",
    "PRESET_TYPE_INTEGER": "INTEGER",
    "PRESET_TYPE_SET": "SET",
    "PRESET_TYPE_FUNCTION": "FUNCTION",
    "PRESET_TYPE_TUPLE": "TUPLE",
    "PRESET_TYPE_LIST": "LIST",
    "PRESET_TYPE_SEQUENCE": "SEQUENCE",
    "PRESET_TYPE_REGION": "REGION",
    "PRESET_TYPE_PATH": "PATH",
    "PRESET_TYPE_SURFACE": "SURFACE",
    "PRESET_TYPE_SPACE": "SPACE",
    "PRESET_TYPE_GROUP": "GROUP",
    "PRESET_TYPE_GROUP_ELEMENT": "GROUP_ELEMENT",
    "PRESET_TYPE_SUBGROUP": "SUBGROUP",
    "PRESET_TYPE_HOMOMORPHISM": "HOMOMORPHISM",
    "PRESET_TYPE_PRIME": "PRIME",
    "PRESET_TYPE_EQUATION": "EQUATION",
    "PRESET_TYPE_LIMIT": "LIMIT",
    "PRESET_TYPE_DERIVATIVE": "DERIVATIVE",
    "PRESET_TYPE_POLYNOMIAL": "POLYNOMIAL",
    "PRESET_TYPE_LIMIT_EXPRESSION": "LIMIT_EXPRESSION",
    "PRESET_TYPE_RING": "RING",
    "PRESET_TYPE_IDEAL": "IDEAL",
    "PRESET_TYPE_FIELD": "FIELD",
    "PRESET_TYPE_MODULE": "MODULE",
    "PRESET_TYPE_ALGEBRA": "ALGEBRA",
    "PRESET_TYPE_TOPOLOGY": "TOPOLOGY",
    "PRESET_TYPE_MANIFOLD": "MANIFOLD",
    "PRESET_TYPE_DISTRIBUTION": "DISTRIBUTION",
    "PRESET_TYPE_PROBABILITY": "PROBABILITY",
    "PRESET_TYPE_GRAPH": "GRAPH",
    "PRESET_TYPE_TREE": "TREE",
    "PRESET_TYPE_INTEGRAL": "INTEGRAL",
    "PRESET_TYPE_SERIES": "SERIES",
    "PRESET_TYPE_COMPLEX": "COMPLEX",
    "PRESET_TYPE_PERMUTATION": "PERMUTATION",
    "PRESET_TYPE_COSET": "COSET",
    "PRESET_TYPE_EXTENSION": "EXTENSION",
    "PRESET_TYPE_AUTOMORPHISM": "AUTOMORPHISM",
    "PRESET_TYPE_DISTANCE": "DISTANCE",
    "PRESET_TYPE_AREA": "AREA",
    "PRESET_TYPE_LENGTH": "LENGTH",
    "PRESET_TYPE_CURVATURE": "CURVATURE",
    "PRESET_TYPE_OPEN_SET": "OPEN_SET",
    "PRESET_TYPE_CLOSED_SET": "CLOSED_SET",
    "PRESET_TYPE_RESIDUE": "RESIDUE",
    "PRESET_TYPE_FORMULA": "FORMULA",
    "PRESET_TYPE_EXPRESSION": "EXPRESSION",
    "PRESET_TYPE_STRUCTURE": "STRUCTURE",
    "PRESET_TYPE_STRING": "STRING",
    "PRESET_TYPE_ANY": "ANY",
    "0": "ANY",
}

# ============================================================
# PresetCategory mapping (from func_block_registry.h)
# ============================================================
CATEGORY_MAP = {
    "PRESET_CATEGORY_CONSTRUCTION": "CONSTRUCTION",
    "PRESET_CATEGORY_MEASUREMENT": "MEASUREMENT",
    "PRESET_CATEGORY_TRANSFORMATION": "TRANSFORMATION",
    "PRESET_CATEGORY_ALGEBRAIC": "ALGEBRAIC",
    "PRESET_CATEGORY_LOGIC": "LOGIC",
    "PRESET_CATEGORY_ANALYSIS": "ANALYSIS",
    "PRESET_CATEGORY_NUMBER_THEORY": "NUMBER_THEORY",
    "PRESET_CATEGORY_GROUP_THEORY": "GROUP_THEORY",
    "PRESET_CATEGORY_RING_THEORY": "RING_THEORY",
    "PRESET_CATEGORY_FIELD_THEORY": "FIELD_THEORY",
    "PRESET_CATEGORY_TOPOLOGY": "TOPOLOGY",
    "PRESET_CATEGORY_LINEAR_ALGEBRA": "LINEAR_ALGEBRA",
    "PRESET_CATEGORY_COMBINATORICS": "COMBINATORICS",
    "PRESET_CATEGORY_COMPLEX_ANALYSIS": "COMPLEX_ANALYSIS",
    "PRESET_CATEGORY_PROBABILITY": "PROBABILITY",
    "PRESET_CATEGORY_GEOMETRY": "GEOMETRY",
    "PRESET_CATEGORY_ALGEBRA": "ALGEBRA",
    "PRESET_CATEGORY_CATEGORY_THEORY": "CATEGORY_THEORY",
    "PRESET_CATEGORY_SET_THEORY": "SET_THEORY",
    "PRESET_CATEGORY_GRAPH_THEORY": "GRAPH_THEORY",
    "PRESET_CATEGORY_DIFFERENTIAL_GEOMETRY": "DIFFERENTIAL_GEOMETRY",
    "PRESET_CATEGORY_NUMERICAL": "NUMERICAL",
    "PRESET_CATEGORY_OPTIMIZATION": "OPTIMIZATION",
    "PRESET_CATEGORY_MATH_LOGIC": "MATH_LOGIC",
    "PRESET_CATEGORY_CUSTOM": "CUSTOM",
}


# ============================================================
# Load preset_name_defs.h to get the name constant -> string mapping
# ============================================================
def load_name_defs(path):
    name_map = {}
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()
    for m in re.finditer(r'#define\s+(\w+)\s+"([^"]*)"', content):
        name_map[m.group(1)] = m.group(2)
    return name_map


# ============================================================
# Extract category from file content
# ============================================================
def extract_category(content):
    m = re.search(r'LV_DECLARE_PRESET_REGISTER\s*\(\s*(\w+)\s*\)', content)
    if m:
        cat = m.group(1)
        return CATEGORY_MAP.get(cat, "CUSTOM")
    m = re.search(r'@category\s+(\w+)', content)
    if m:
        cat = m.group(1)
        return CATEGORY_MAP.get(cat, "CUSTOM")
    return "CUSTOM"


# ============================================================
# Parse a single LV_PRESET_REGISTER / REGISTER_XXX call
# Handles both name as macro constant and as string literal
# ============================================================

# Pattern for LV_PRESET_REGISTER or custom REGISTER_XXX macros
# The name can be either a macro constant (\w+) or a string literal ("...")
# Format: MACRO(success_counter, name, "desc", input_count, OUTPUT_TYPE, "math_def", "complexity", cons, rev, INPUT_TYPES...)
def find_register_calls(content, name_map):
    """Find all preset register calls in content and return list of parsed presets."""
    presets = []

    # Pattern 1: LV_PRESET_REGISTER or custom REGISTER_XXX macros
    # Match: MACRO_NAME(success_var, NAME_OR_STRING, "desc", N, TYPE, "math", "complexity", bool, bool, ...)
    lines = content.split('\n')
    # First, join continuation lines (lines ending with backslash or with open parens)
    joined = content.replace('\\\n', ' ')

    # Find all macro calls that look like register patterns
    # We need to handle multi-line calls carefully
    # Strategy: find all occurrences of LV_PRESET_REGISTER( or REGISTER_XXX(
    # and extract the balanced parentheses

    # Simpler approach: find all lines with LV_PRESET_REGISTER or REGISTER_ or helper
    # and collect the full call

    # Collect all calls (including multi-line)
    calls = []
    i = 0
    while i < len(joined):
        # Look for register macro calls
        for pattern_prefix in ['LV_PRESET_REGISTER(', 'REGISTER_', 'lv_preset_register_helper(']:
            idx = joined.find(pattern_prefix, i)
            if idx >= 0:
                # Found a potential call, extract balanced parens
                start = joined.rfind('\n', 0, idx) + 1
                if start < 0:
                    start = 0
                # Find the opening paren after the macro name
                paren_start = joined.find('(', idx)
                if paren_start >= 0:
                    depth = 1
                    pos = paren_start + 1
                    while pos < len(joined) and depth > 0:
                        if joined[pos] == '(':
                            depth += 1
                        elif joined[pos] == ')':
                            depth -= 1
                        pos += 1
                    if depth == 0:
                        call_text = joined[paren_start:pos]
                        calls.append((pattern_prefix, call_text))
                        i = pos
                        break
        else:
            i += 1

    for prefix, call_text in calls:
        try:
            # Parse the call based on prefix
            if prefix == 'lv_preset_register_helper(':
                preset = parse_helper_call(call_text, name_map)
                if preset:
                    presets.append(preset)
            elif prefix == 'LV_PRESET_REGISTER(' or prefix.startswith('REGISTER_'):
                preset = parse_register_macro(call_text, name_map)
                if preset:
                    presets.append(preset)
        except Exception as e:
            print(f"    Warning: Parse error in {prefix}: {e}")

    # Also find preset_blocks_register_by_category calls using balanced-paren approach
    for prefix in ['preset_blocks_register_by_category(', 'preset_blocks_register_simple(']:
        i = 0
        while i < len(joined):
            idx = joined.find(prefix, i)
            if idx < 0:
                break
            paren_start = joined.find('(', idx)
            if paren_start < 0:
                i = idx + 1
                continue
            # Extract balanced parens respecting quotes
            depth = 1
            pos = paren_start + 1
            in_quote = False
            quote_char = None
            while pos < len(joined) and depth > 0:
                ch = joined[pos]
                if in_quote:
                    if ch == quote_char:
                        in_quote = False
                elif ch in ('"', "'"):
                    in_quote = True
                    quote_char = ch
                elif ch == '(':
                    depth += 1
                elif ch == ')':
                    depth -= 1
                pos += 1
            if depth == 0:
                call_text = joined[paren_start:pos]
                if prefix.startswith('preset_blocks_register_by_category'):
                    preset = parse_register_by_category(call_text)
                else:
                    preset = parse_register_simple(call_text, name_map)
                if preset:
                    presets.append(preset)
                i = pos
            else:
                i = idx + 1

    return presets


def parse_register_macro(call_text, name_map):
    """Parse LV_PRESET_REGISTER or custom REGISTER_XXX macro call."""
    # Remove the outermost parens
    inner = call_text.strip()
    if inner.startswith('(') and inner.endswith(')'):
        inner = inner[1:-1]

    # Split by top-level commas (not inside parens or quotes)
    args = split_by_comma(inner)
    if len(args) < 9:
        return None

    # Args: success_counter, name, "desc", input_count, OUTPUT_TYPE, "math_def", "complexity", cons, rev, INPUT_TYPES...
    # success_counter is arg[0] - skip it
    name_raw = args[1].strip()
    desc = unquote(args[2].strip())
    input_count_str = args[3].strip()
    output_type_raw = args[4].strip()
    math_def = unquote(args[5].strip())
    complexity = unquote(args[6].strip())
    cons_raw = args[7].strip()
    rev_raw = args[8].strip()

    # Resolve name
    name = resolve_name(name_raw, name_map)

    # Resolve output type
    output_type = PRESET_TYPE_MAP.get(output_type_raw, "ANY")

    # Parse input count
    try:
        input_count = int(input_count_str)
    except ValueError:
        input_count = 0

    # Parse input types (remaining args)
    input_types = []
    for i in range(9, len(args)):
        t = args[i].strip()
        if t:
            input_types.append(PRESET_TYPE_MAP.get(t, "ANY"))

    # Parse bools
    constructive = cons_raw.lower() in ('true', '1')
    reversible = rev_raw.lower() in ('true', '1')

    return {
        'name': name,
        'description': desc,
        'input_types': input_types[:input_count] if input_count > 0 else [],
        'input_count': input_count,
        'output_type': output_type,
        'math_def': math_def,
        'complexity': complexity,
        'constructive': constructive,
        'reversible': reversible,
    }


def parse_helper_call(call_text, name_map):
    """Parse lv_preset_register_helper call."""
    inner = call_text.strip()
    if inner.startswith('(') and inner.endswith(')'):
        inner = inner[1:-1]

    args = split_by_comma(inner)
    if len(args) < 8:
        return None

    # Args: name, "desc", types_array, input_count, OUTPUT_TYPE, "math_def", "complexity", cons, rev
    name_raw = args[0].strip()
    desc = unquote(args[1].strip())
    # args[2] is types_array - skip
    input_count_str = args[3].strip()
    output_type_raw = args[4].strip()
    math_def = unquote(args[5].strip())
    complexity = unquote(args[6].strip())
    cons_raw = args[7].strip()
    rev_raw = args[8].strip() if len(args) > 8 else 'false'

    name = resolve_name(name_raw, name_map)
    output_type = PRESET_TYPE_MAP.get(output_type_raw, "ANY")

    try:
        input_count = int(input_count_str)
    except ValueError:
        input_count = 0

    constructive = cons_raw.lower() in ('true', '1')
    reversible = rev_raw.lower() in ('true', '1')

    return {
        'name': name,
        'description': desc,
        'input_types': [],
        'input_count': input_count,
        'output_type': output_type,
        'math_def': math_def,
        'complexity': complexity,
        'constructive': constructive,
        'reversible': reversible,
    }


def parse_register_by_category(inner):
    """Parse preset_blocks_register_by_category call."""
    args = split_by_comma(inner)
    if len(args) < 5:
        return None

    name = unquote(args[0].strip())
    desc = unquote(args[1].strip())
    # args[2] is category - skip
    input_count = int(args[3].strip())
    output_count = int(args[4].strip())

    return {
        'name': name,
        'description': desc,
        'input_count': input_count,
        'output_count': output_count,
        'input_types': [],
        'output_type': 'ANY',
        'math_def': '',
        'complexity': '',
        'constructive': True,
        'reversible': False,
    }


def parse_register_simple(inner, name_map):
    """Parse direct preset_blocks_register_simple call."""
    args = split_by_comma(inner)
    if len(args) < 10:
        return None

    name_raw = args[0].strip()
    desc = unquote(args[1].strip())
    # args[2] is category_enum - skip
    # args[3] is input_types array - skip
    input_count = int(args[4].strip())
    output_type_raw = args[5].strip()
    math_def = unquote(args[6].strip())
    complexity = unquote(args[7].strip())
    cons_raw = args[8].strip()
    rev_raw = args[9].strip()

    name = resolve_name(name_raw, name_map)
    output_type = PRESET_TYPE_MAP.get(output_type_raw, "ANY")
    constructive = cons_raw.lower() in ('true', '1')
    reversible = rev_raw.lower() in ('true', '1')

    return {
        'name': name,
        'description': desc,
        'input_types': [],
        'input_count': input_count,
        'output_type': output_type,
        'math_def': math_def,
        'complexity': complexity,
        'constructive': constructive,
        'reversible': reversible,
    }


def resolve_name(raw, name_map):
    """Resolve a name that could be a macro constant or a string literal."""
    raw = raw.strip()
    if raw.startswith('"') and raw.endswith('"'):
        return raw[1:-1]
    # It's a macro constant
    return name_map.get(raw, raw.lower())


def unquote(s):
    """Remove surrounding quotes from a string."""
    if s.startswith('"') and s.endswith('"'):
        return s[1:-1]
    return s


def split_by_comma(text):
    """Split by top-level commas (not inside quotes or parens)."""
    result = []
    depth = 0
    current = []
    in_quote = False
    quote_char = None

    for ch in text:
        if in_quote:
            current.append(ch)
            if ch == quote_char:
                in_quote = False
        elif ch in ('"', "'"):
            current.append(ch)
            in_quote = True
            quote_char = ch
        elif ch in '({[':
            current.append(ch)
            depth += 1
        elif ch in ')}]':
            current.append(ch)
            depth -= 1
        elif ch == ',' and depth == 0:
            result.append(''.join(current).strip())
            current = []
        else:
            current.append(ch)

    remaining = ''.join(current).strip()
    if remaining:
        result.append(remaining)
    return result


# ============================================================
# Generate .lvz content for a set of presets
# ============================================================
def generate_lvz_content(module_name, category, presets):
    lines = []
    lines.append(f'# =============================================================================')
    lines.append(f'# Lv-00 Presets: {module_name}')
    lines.append(f'# =============================================================================')
    lines.append(f'#')
    lines.append(f'# Auto-generated from C preset files.')
    lines.append(f'#')
    lines.append(f'# =============================================================================')
    lines.append(f'')
    lines.append(f'lvz 1')
    lines.append(f'')
    lines.append(f'presets {{')
    lines.append(f'')

    for p in presets:
        lines.append(f'    preset "{p["name"]}" {{')
        # Escape quotes in description
        desc = p["description"].replace('"', '\\"')
        lines.append(f'        description "{desc}"')
        lines.append(f'        category "{category}"')
        if p['input_count'] > 0:
            types_str = ' '.join(f'"{t}"' for t in p['input_types'])
            lines.append(f'        inputs {p["input_count"]} {types_str}')
        lines.append(f'        output "{p["output_type"]}"')
        if p['math_def']:
            math_def = p['math_def'].replace('"', '\\"')
            lines.append(f'        math_def "{math_def}"')
        if p['complexity']:
            lines.append(f'        complexity "{p["complexity"]}"')
        lines.append(f'        constructive {"true" if p["constructive"] else "false"}')
        lines.append(f'        reversible {"true" if p["reversible"] else "false"}')
        lines.append(f'    }}')
        lines.append(f'')

    lines.append(f'}}')
    lines.append(f'')
    return '\n'.join(lines)


# ============================================================
# Main conversion
# ============================================================
def main():
    base_dir = r'c:\Users\xingg\Desktop\知识体系化Wiki\Lv-00'
    preset_dir = os.path.join(base_dir, 'core', 'src', 'layer4_reasoning', 'preset')
    output_dir = os.path.join(base_dir, 'module', 'presets')
    name_defs_path = os.path.join(base_dir, 'core', 'include', 'lv', 'preset_name_defs.h')

    # Load name definitions
    name_map = load_name_defs(name_defs_path)
    print(f"Loaded {len(name_map)} name definitions from preset_name_defs.h")

    # Create output directory
    os.makedirs(output_dir, exist_ok=True)

    # Process each C file
    c_files = sorted([f for f in os.listdir(preset_dir) if f.endswith('.c') and f.startswith('preset_')])
    print(f"Found {len(c_files)} preset C files to convert")

    total_presets = 0
    files_generated = 0

    for c_file in c_files:
        filepath = os.path.join(preset_dir, c_file)
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()

        # Extract category
        category = extract_category(content)

        # Find all register calls
        presets = find_register_calls(content, name_map)

        if not presets and 'preset_blocks_register' in content:
            # Debug: count regex matches
            import re as _re
            _m = list(_re.finditer(r'preset_blocks_register_by_category\s*\(([^)]+)\)', content.replace('\\\n', ' ')))
            print(f"  {c_file}: DEBUG - regex found {len(_m)} matches")

        if presets:
            # Deduplicate by name (keep last occurrence)
            seen = {}
            for p in presets:
                seen[p['name']] = p
            presets = list(seen.values())

            total_presets += len(presets)
            files_generated += 1
            print(f"  {c_file}: {len(presets)} presets, category={category}")

            # Generate .lvz file
            module_name = c_file.replace('.c', '').replace('preset_', '').replace('_', ' ').title()
            lvz_filename = c_file.replace('.c', '.lvz')
            lvz_filepath = os.path.join(output_dir, lvz_filename)

            lvz_content = generate_lvz_content(module_name, category, presets)
            with open(lvz_filepath, 'w', encoding='utf-8') as f:
                f.write(lvz_content)
        else:
            # Check if file has any preset registration at all
            if 'preset_blocks_register' in content or 'LV_PRESET_REGISTER' in content or 'REGISTER_' in content:
                print(f"  {c_file}: WARNING - found registration keywords but couldn't parse any presets")
            else:
                print(f"  {c_file}: No presets found (skipping)")

    print(f"\nDone! Generated {files_generated} .lvz files in {output_dir}")
    print(f"Total presets: {total_presets}")


if __name__ == '__main__':
    main()
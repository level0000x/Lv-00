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

# helper 布局自定义宏（展开为 lv_preset_register_helper，参数顺序：name, desc, inputs数组, in_count,
# output, math, comp, cons, rev —— 与 LV_PRESET_REGISTER 的 success_counter 布局不同）
HELPER_LAYOUT_MACROS = {
    'REGISTER_LATTICE',
    'REGISTER_LOGIC',
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
    # 预处理：剥离 C 注释（保护字符串字面量）→ 移除 #define 宏定义块（防占位符误解析）
    # → 连接反斜杠续行
    joined = strip_c_comments(content)
    joined = remove_define_blocks(joined)
    joined = joined.replace('\\\n', ' ')

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
                # Found a potential call, extract balanced parens (respecting quotes)
                start = joined.rfind('\n', 0, idx) + 1
                if start < 0:
                    start = 0
                # Find the opening paren after the macro name
                paren_start = joined.find('(', idx)
                if paren_start >= 0:
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
                        # 提取实际宏名（如 REGISTER_LATTICE），用于布局分发
                        macro_name = None
                        if pattern_prefix == 'REGISTER_':
                            m = re.match(r'(\w+)\s*\(', joined[idx:])
                            if m:
                                macro_name = m.group(1)
                        calls.append((pattern_prefix, call_text, idx, macro_name))
                        i = pos
                        break
        else:
            i += 1

    for prefix, call_text, idx, macro_name in calls:
        try:
            # Parse the call based on prefix
            if prefix == 'lv_preset_register_helper(':
                preset = parse_helper_call(call_text, name_map, joined, idx)
                if preset:
                    presets.append(preset)
            elif prefix == 'LV_PRESET_REGISTER(':
                preset = parse_register_macro(call_text, name_map)
                if preset:
                    presets.append(preset)
            elif prefix.startswith('REGISTER_'):
                # helper 布局宏（展开为 lv_preset_register_helper）：REGISTER_LATTICE / REGISTER_LOGIC
                if macro_name in HELPER_LAYOUT_MACROS:
                    preset = parse_helper_call(call_text, name_map, joined, idx)
                else:
                    # LV 布局（含 success_counter 或 REGISTER_RT 风格）：走标准宏解析
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
                    preset = parse_register_simple(call_text, name_map, joined, idx)
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
    args = merge_arg_strings(split_by_comma(inner))
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


def parse_helper_call(call_text, name_map, joined=None, idx=0):
    """Parse lv_preset_register_helper call.

    Args 布局: name, "desc", types_array, input_count, OUTPUT_TYPE, "math_def", "complexity", cons, rev
    types_array 为 `PresetType inputs[] = { ... }` 声明中的数组名（或内联初始化列表），
    通过 parse_input_array_arg 解析为类型名列表。
    """
    inner = call_text.strip()
    if inner.startswith('(') and inner.endswith(')'):
        inner = inner[1:-1]

    args = merge_arg_strings(split_by_comma(inner))
    if len(args) < 8:
        return None

    # Args: name, "desc", types_array, input_count, OUTPUT_TYPE, "math_def", "complexity", cons, rev
    name_raw = args[0].strip()
    desc = unquote(args[1].strip())
    # args[2] 是类型数组（内联 {..} 或向前查找 `PresetType <name>[] = {..}` 声明）
    input_types = parse_input_array_arg(args[2], joined, idx)
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
        'input_types': input_types,
        'input_count': input_count,
        'output_type': output_type,
        'math_def': math_def,
        'complexity': complexity,
        'constructive': constructive,
        'reversible': reversible,
    }


def parse_register_by_category(inner):
    """Parse preset_blocks_register_by_category call."""
    inner = inner.strip()
    if inner.startswith('(') and inner.endswith(')'):
        inner = inner[1:-1]

    args = merge_arg_strings(split_by_comma(inner))
    if len(args) < 5:
        return None

    name = unquote(args[0].strip())
    desc = unquote(args[1].strip())
    # args[2] is category - skip
    input_count = int(args[3].strip())
    output_count = int(args[4].strip())

    # 按 category 注册的调用不携带输入类型，用 ANY 填充以保持 .lvz 格式
    # 与 loader 兼容（module_lvz.c 的 preset_field_inputs 要求 count 个类型 token）
    input_types = ['ANY'] * input_count if input_count > 0 else []

    return {
        'name': name,
        'description': desc,
        'input_count': input_count,
        'output_count': output_count,
        'input_types': input_types,
        'output_type': 'ANY',
        'math_def': '',
        'complexity': '',
        'constructive': True,
        'reversible': False,
    }


def parse_register_simple(inner, name_map, joined=None, idx=0):
    """Parse direct preset_blocks_register_simple call."""
    inner = inner.strip()
    if inner.startswith('(') and inner.endswith(')'):
        inner = inner[1:-1]

    args = merge_arg_strings(split_by_comma(inner))
    if len(args) < 10:
        return None

    name_raw = args[0].strip()
    desc = unquote(args[1].strip())
    # args[2] is category_enum - skip
    # args[3] 是 input_types 数组（内联 {..} 或向前查找声明）
    input_types = parse_input_array_arg(args[3], joined, idx)
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
        'input_types': input_types,
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


# ============ C 源码预处理辅助 ============

def strip_c_comments(text):
    """剥离 C 注释（/* */ 与 //），保护字符串字面量内的文本。"""
    out = []
    i = 0
    n = len(text)
    while i < n:
        ch = text[i]
        if ch == '"':
            out.append(ch)
            i += 1
            while i < n:
                c = text[i]
                out.append(c)
                i += 1
                if c == '\\' and i < n:
                    out.append(text[i])
                    i += 1
                elif c == '"':
                    break
        elif ch == '/' and i + 1 < n and text[i + 1] == '*':
            i += 2
            while i + 1 < n and not (text[i] == '*' and text[i + 1] == '/'):
                i += 1
            i += 2
            out.append(' ')
        elif ch == '/' and i + 1 < n and text[i + 1] == '/':
            i += 2
            while i < n and text[i] != '\n':
                i += 1
            out.append(' ')
        else:
            out.append(ch)
            i += 1
    return ''.join(out)


def remove_define_blocks(text):
    """移除 #define 宏定义块（含 \\ 续行），避免宏占位符被当作真实调用。"""
    lines = text.split('\n')
    out = []
    in_block = False
    for ln in lines:
        if in_block:
            if ln.rstrip().endswith('\\'):
                continue
            in_block = False
            out.append(ln)
        else:
            if ln.strip().startswith('#define'):
                if not ln.rstrip().endswith('\\'):
                    pass
                else:
                    in_block = True
            else:
                out.append(ln)
    return '\n'.join(out)


def merge_c_strings(s):
    """合并 C 相邻字符串字面量（"a" "b" -> "ab"）。"""
    s = s.strip()
    if not s or s[0] != '"':
        return s
    parts = []
    i = 0
    n = len(s)
    while i < n:
        while i < n and s[i] in ' \t\r\n':
            i += 1
        if i >= n:
            break
        if s[i] != '"':
            return s
        j = i + 1
        while j < n and s[j] != '"':
            if s[j] == '\\' and j + 1 < n:
                j += 2
            else:
                j += 1
        if j >= n:
            return s
        parts.append(s[i + 1:j])
        i = j + 1
    if len(parts) < 2:
        return s
    return '"' + ''.join(parts) + '"'


def merge_arg_strings(args):
    return [merge_c_strings(a) for a in args]


def is_number_str(s):
    try:
        int(s.strip())
        return True
    except ValueError:
        return False


def find_type_array(joined, idx, var_name):
    """在 joined[0:idx] 向前找最近的 `PresetType <name>[] = {...}` 声明。"""
    if not var_name:
        return None
    region = joined[:idx]
    pattern = re.compile(r'\bPresetType\s+' + re.escape(var_name) +
                         r'\s*\[\s*\d*\s*\]\s*=\s*\{(.*?)\}', re.S)
    matches = list(pattern.finditer(region))
    if not matches:
        return None
    return matches[-1].group(1)


def extract_type_names_from_array(array_text):
    """从 {PRESET_TYPE_X, ...} 文本提取类型名列表（映射为字符串，支持 0=ANY）。"""
    tokens = re.findall(r'\bPRESET_TYPE_\w+|\b0\b', array_text or '')
    return [PRESET_TYPE_MAP.get(t, 'ANY') for t in tokens]


def parse_input_array_arg(raw_arg, joined, idx):
    """解析 helper 的类型数组参数：内联 {..} 或向前查 `PresetType name[] = {..}`。"""
    arg = raw_arg.strip()
    if '{' in arg:
        m = re.search(r'\{(.*)\}', arg, re.S)
        if m:
            return extract_type_names_from_array(m.group(1))
        return []
    var_name = arg.strip('() \t\r\n')
    if not re.fullmatch(r'[A-Za-z_]\w*', var_name or ''):
        return []
    array_text = find_type_array(joined, idx, var_name)
    if array_text is None:
        return []
    return extract_type_names_from_array(array_text)


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
            # 类型数量必须与 input_count 一致（loader 要求 N 个类型 token）：
            # 不足补 ANY，超出截断
            types = list(p.get('input_types') or [])
            if len(types) < p['input_count']:
                types += ['ANY'] * (p['input_count'] - len(types))
            types = types[:p['input_count']]
            types_str = ' '.join(f'"{t}"' for t in types)
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
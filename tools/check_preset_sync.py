#!/usr/bin/env python3
"""
check_preset_sync.py — 校验 module/presets/*.lvz 与 preset_xxx.c 注册数据的同步状态

数据流背景：
  - convert_presets.py 从 core/src/layer4_reasoning/preset/preset_*.c 中提取
    LV_PRESET_REGISTER / REGISTER_XXX / preset_blocks_register_by_category 等
    注册调用，生成 module/presets/*.lvz。
  - 运行时（preset_blocks.c 的 load_presets_from_lvz）以 .lvz 为唯一数据源，
    preset_xxx.c 不参与构建（历史遗留/死代码）。
  - 本脚本对每个模块比较（a）C 源中可提取的注册条目集合 与（b）.lvz 中
    实际条目集合，输出差异报告，支持 --fix 重新生成（先备份 .lvz.bak）。

用法：
  python tools/check_preset_sync.py            # dry-run：仅报告差异
  python tools/check_preset_sync.py --fix      # 修复可安全修复的差异（先备份 .lvz.bak）
  python tools/check_preset_sync.py --json     # 机器可读摘要（json 追加到 stdout 末尾）
"""

import argparse
import json
import os
import re
import shutil
import sys

# 允许 import 根目录下的 convert_presets.py
BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if BASE_DIR not in sys.path:
    sys.path.insert(0, BASE_DIR)

import convert_presets  # noqa: E402

PRESET_C_DIR = os.path.join(BASE_DIR, 'core', 'src', 'layer4_reasoning', 'preset')
LVZ_DIR = os.path.join(BASE_DIR, 'module', 'presets')
NAME_DEFS_PATH = os.path.join(BASE_DIR, 'core', 'include', 'lv', 'preset_name_defs.h')

# 状态分类
SYNC = 'SYNC'                # 两侧条目集合完全一致（含两侧皆空）
C_EMPTY = 'C_EMPTY'          # C 无条目、.lvz 有条目 → 预期（死 C vs 活数据）
EXTRA_ONLY = 'EXTRA_ONLY'    # .lvz 有 C 没有的条目 → 预期（活数据扩展）
MISSING_ONLY = 'MISSING_ONLY'  # C 有 .lvz 没有的条目（纯缺失）→ 可修复候选
BOTH = 'BOTH'                # 双向都有差异 → 保守不修

# 5 个 C_EMPTY 活数据模块：C 源为死代码（无注册、无类型信息），.lvz 为唯一数据源，
# 且 `inputs N` 后无类型 token（历史活数据格式）——loader（module_lvz.c 的
# preset_field_inputs）以 ANY 填充容错加载。这是预期状态（不误报），
# C 侧无类型来源，不可用生成器补类型。
TYPELESS_INPUTS_MODULES = {
    'preset_algebraic.lvz',
    'preset_basic_geometry.lvz',
    'preset_measurements.lvz',
    'preset_polygons.lvz',
    'preset_transformations.lvz',
}


def read_text_robust(path):
    """读取文本，按 utf-8 → gbk → latin-1 依次尝试（条目名均为 ASCII，不影响比较）。"""
    with open(path, 'rb') as f:
        data = f.read()
    for enc in ('utf-8', 'gbk', 'latin-1'):
        try:
            return data.decode(enc)
        except UnicodeDecodeError:
            continue
    return data.decode('utf-8', errors='replace')


def parse_lvz_names(path):
    """从 .lvz 提取 preset 条目名（按出现顺序）。"""
    content = read_text_robust(path)
    return re.findall(r'\bpreset\s+"([^"]+)"\s*\{', content)


def extract_c_presets(c_path, name_map):
    """从 C 文件提取注册条目（复用 convert_presets 解析逻辑，去重 keep-last 与生成器一致）。"""
    content = convert_presets.load_file_utf8(c_path) if hasattr(convert_presets, 'load_file_utf8') else read_text_robust(c_path)
    presets = convert_presets.find_register_calls(content, name_map)
    seen = {}
    for p in presets:
        seen[p['name']] = p
    return list(seen.values())


def compare_module(c_file, lvz_file, name_map):
    """返回 (c_presets, lvz_names, missing, extra, status)。"""
    c_presets = extract_c_presets(c_file, name_map)
    lvz_names = parse_lvz_names(lvz_file)

    c_names = [p['name'] for p in c_presets]
    c_set = set(c_names)
    lvz_set = set(lvz_names)

    missing = [n for n in c_names if n not in lvz_set]          # C 有、.lvz 无（保持 C 顺序）
    extra = [n for n in lvz_names if n not in c_set]            # .lvz 有、C 无（保持 .lvz 顺序）

    if not c_names and not lvz_names:
        status = SYNC
    elif missing or extra:
        if not c_names:
            status = C_EMPTY
        elif missing and not extra:
            status = MISSING_ONLY
        elif extra and not missing:
            status = EXTRA_ONLY
        else:
            status = BOTH
    else:
        status = SYNC

    return c_presets, lvz_names, missing, extra, status


def regen_lvz(c_file, lvz_file, c_presets, category):
    """用生成器逻辑重新生成 .lvz 内容。"""
    module_name = os.path.basename(c_file).replace('.c', '').replace('preset_', '').replace('_', ' ').title()
    return convert_presets.generate_lvz_content(module_name, category, c_presets)


# ============================================================
# --verify: 模拟 module_lvz.c 的 .lvz 词法/语法解析（load_presets_from_lvz 语义）
# 忠实复刻：'#' 行注释、字符串转义（\n \t \r \" \\）、数字、标识符、
# presets 节语法、字段表分发、inputs 类型缺失容错（ANY 填充，与修复后 C loader 一致）
# ============================================================

# token 类型（对齐 module_lvz.c 的 LvzTokenType）
T_EOF, T_IDENTIFIER, T_STRING, T_NUMBER, T_LBRACE, T_RBRACE = range(6)

PRESET_FIELDS = {'description', 'category', 'inputs', 'output', 'math_def', 'complexity', 'constructive', 'reversible'}


def lvz_tokenize(text):
    """复刻 lvz_lexer_next_token + lv_lexer_skip_whitespace_and_comments。返回 (tokens, error)。"""
    tokens = []
    i, n = 0, len(text)
    line = 1
    while i < n:
        ch = text[i]
        if ch in ' \t\r\n\f\v':
            if ch == '\n':
                line += 1
            i += 1
            continue
        if ch == '#':  # 注释到行尾
            while i < n and text[i] != '\n':
                i += 1
            continue
        if ch == '{':
            tokens.append((T_LBRACE, None, line))
            i += 1
            continue
        if ch == '}':
            tokens.append((T_RBRACE, None, line))
            i += 1
            continue
        if ch == '"':  # 字符串（含转义）
            i += 1
            s = []
            ok = False
            while i < n:
                c = text[i]
                if c == '\\':
                    if i + 1 >= n:
                        break
                    nxt = text[i + 1]
                    s.append({'n': '\n', 't': '\t', 'r': '\r', '"': '"', '\\': '\\'}.get(nxt, nxt))
                    i += 2
                elif c == '"':
                    ok = True
                    i += 1
                    break
                else:
                    s.append(c)
                    i += 1
            if not ok:
                return None, f'字符串字面量解析失败 (行 {line})'
            tokens.append((T_STRING, ''.join(s), line))
            continue
        if ch.isdigit() or (ch == '-' and i + 1 < n and text[i + 1].isdigit()):  # 数字
            start = i
            if ch == '-':
                i += 1
            while i < n and text[i].isdigit():
                i += 1
            if i < n and text[i] == '.':
                i += 1
                while i < n and text[i].isdigit():
                    i += 1
            tokens.append((T_NUMBER, float(text[start:i]), line))
            continue
        if ch.isalpha() or ch == '_':  # 标识符（可含 - .，与 C 一致）
            start = i
            while i < n and (text[i].isalnum() or text[i] in '_-.'):
                i += 1
            tokens.append((T_IDENTIFIER, text[start:i], line))
            continue
        return None, f'意外的字符 {ch!r} (行 {line})'
    tokens.append((T_EOF, None, line))
    return tokens, None


class _Tk:
    """轻量 token 游标，复刻 LvzParser 的 advance/expect 语义。"""

    def __init__(self, tokens):
        self.tokens = tokens
        self.pos = 0

    def cur(self):
        return self.tokens[self.pos] if self.pos < len(self.tokens) else (T_EOF, None, 0)

    def advance(self):
        if self.pos < len(self.tokens) - 1:
            self.pos += 1

    def expect(self, ttype):
        if self.cur()[0] != ttype:
            return False
        self.advance()
        return True


def _verify_preset_body(tk, name):
    """复刻 lvz_parse_preset_body + kPresetFieldTable 分发。返回错误消息或 None。"""
    if not tk.expect(T_LBRACE):
        return f"预设 '{name}': 期望 '{{'"
    while tk.cur()[0] not in (T_RBRACE, T_EOF):
        if tk.cur()[0] != T_IDENTIFIER:
            return f"预设 '{name}': 期望字段名"
        field = tk.cur()[1]
        if field not in PRESET_FIELDS:
            return f"预设 '{name}': 未知字段 '{field}'"
        tk.advance()
        if field == 'inputs':
            if tk.cur()[0] != T_NUMBER:
                return f"预设 '{name}': inputs 期望数量"
            count = int(tk.cur()[1])
            tk.advance()
            # 消费 count 个类型字符串；不足时 ANY 填充（与修复后 C loader 容错一致）
            got = 0
            while got < count and tk.cur()[0] == T_STRING:
                tk.advance()
                got += 1
        elif field in ('description', 'category', 'output', 'math_def', 'complexity'):
            if not tk.expect(T_STRING):
                return f"预设 '{name}': {field} 期望字符串"
        elif field in ('constructive', 'reversible'):
            if tk.cur()[0] == T_IDENTIFIER:
                tk.advance()
    if not tk.expect(T_RBRACE):
        return f"预设 '{name}': 期望 '}}' 结束预设体"
    return None


def verify_lvz_load(path):
    """模拟 lvz_load_presets_file。返回 (ok, 错误消息或 OK 摘要)。"""
    try:
        with open(path, 'rb') as f:
            data = f.read()
    except OSError as e:
        return False, f'无法读取: {e}'
    try:
        text = data.decode('utf-8')
    except UnicodeDecodeError:
        return False, '非 UTF-8 编码'
    tokens, err = lvz_tokenize(text)
    if err:
        return False, err
    tk = _Tk(tokens)
    # ---- 复刻 lvz_parse 主循环 ----
    if not (tk.cur()[0] == T_IDENTIFIER and tk.cur()[1] == 'lvz'):
        return False, "无效的 LVZ 文件: 缺少 'lvz' 头"
    tk.advance()
    if tk.cur()[0] != T_NUMBER:
        return False, '无效的 LVZ 文件: 缺少版本号'
    major = int(tk.cur()[1])
    tk.advance()
    # 可选的次版本号：仅数字分支消费（`1.0` 已被词法器解析为单个 NUMBER）；
    # 与修复后的 C loader 一致：不消费标识符，避免吞掉节名（如 presets）
    if tk.cur()[0] == T_NUMBER:
        tk.advance()
    if major > 1:
        return False, f'不支持的 LVZ 版本: {major}'
    # ---- 节循环 ----
    preset_count = 0
    while tk.cur()[0] != T_EOF:
        if tk.cur()[0] != T_IDENTIFIER:
            return False, f'解析错误 (行 {tk.cur()[2]}): 期望节名称'
        section = tk.cur()[1]
        if section != 'presets':
            if section == 'end':
                tk.advance()
                break
            return False, f'解析错误 (行 {tk.cur()[2]}): 未知的节 {section!r}'
        tk.advance()  # 跳过 'presets'
        # ---- 复刻 lvz_parse_presets_section ----
        if not tk.expect(T_LBRACE):
            return False, f'解析错误 (行 {tk.cur()[2]}): 期望 \'{{\' 开始预设节'
        while tk.cur()[0] == T_IDENTIFIER and tk.cur()[1] == 'preset':
            tk.advance()
            if not tk.expect(T_STRING):
                return False, '解析错误: 期望预设名称字符串'
            pname = tk.cur()[1]
            err = _verify_preset_body(tk, pname)
            if err:
                return False, err
            preset_count += 1
        if not tk.expect(T_RBRACE):
            return False, '解析错误: 期望 \'}\' 结束 presets 节'
    return True, f'OK: {preset_count} 个预设'


def run_verify(lvz_dir):
    """对所有 .lvz 执行模拟加载验证，返回 (results, summary)。"""
    results = {}
    summary = {'LOAD_OK': 0, 'LOAD_FAIL': 0}
    for name in sorted(f for f in os.listdir(lvz_dir) if f.endswith('.lvz')):
        path = os.path.join(lvz_dir, name)
        ok, msg = verify_lvz_load(path)
        results[name] = {'ok': ok, 'msg': msg}
        summary['LOAD_OK' if ok else 'LOAD_FAIL'] += 1
    return results, summary


def main():
    ap = argparse.ArgumentParser(description='校验 .lvz 与 C 预设注册数据的同步状态')
    ap.add_argument('--fix', action='store_true', help='修复可安全修复的差异（先备份 .lvz.bak）')
    ap.add_argument('--json', action='store_true', help='输出 JSON 摘要')
    ap.add_argument('--module', default=None, help='仅检查指定模块（不含前缀，如 information_theory）')
    ap.add_argument('--verify', action='store_true',
                    help='模拟 C loader（module_lvz.c）验证所有 .lvz 可被加载（Python 复刻词法/语法）')
    ap.add_argument('--ci', action='store_true',
                    help='CI 门禁模式：verify 有 LOAD_FAIL、或同步差异含 MISSING_ONLY/BOTH/NO_C_FILE 时 exit 1')
    args = ap.parse_args()

    # ---- --verify：模拟 loader 加载验证（不依赖同步状态） ----
    if args.verify:
        results, summary = run_verify(LVZ_DIR)
        for name in sorted(results):
            r = results[name]
            tag = 'LOAD_OK  ' if r['ok'] else 'LOAD_FAIL'
            print(f'[{tag}] {name:42s} {r["msg"]}')
        print()
        print(f'汇总: LOAD_OK={summary["LOAD_OK"]}, LOAD_FAIL={summary["LOAD_FAIL"]}')
        if args.json:
            print(json.dumps({'verify': results, 'summary': summary}, ensure_ascii=False))
        if args.ci and summary['LOAD_FAIL'] > 0:
            sys.exit(1)  # CI 门禁：存在无法加载的 .lvz
        sys.exit(0)

    name_map = convert_presets.load_name_defs(NAME_DEFS_PATH)
    if not os.path.isdir(LVZ_DIR):
        print(f'错误: .lvz 目录不存在: {LVZ_DIR}', file=sys.stderr)
        sys.exit(2)

    # 以 .lvz 文件为锚（运行时唯一数据源），C 文件一一对应
    lvz_files = sorted(f for f in os.listdir(LVZ_DIR) if f.endswith('.lvz'))
    if args.module:
        lvz_files = [f for f in lvz_files if f.startswith('preset_' + args.module + '.')]

    results = {}
    for lvz_name in lvz_files:
        c_name = lvz_name[:-4] + '.c'
        c_path = os.path.join(PRESET_C_DIR, c_name)
        lvz_path = os.path.join(LVZ_DIR, lvz_name)
        if not os.path.isfile(c_path):
            results[lvz_name] = {'status': 'NO_C_FILE', 'c_count': 0, 'lvz_count': len(parse_lvz_names(lvz_path)),
                                 'missing': [], 'extra': [], 'note': '.lvz 无对应 C 文件'}
            continue
        c_presets, lvz_names, missing, extra, status = compare_module(c_path, lvz_path, name_map)
        results[lvz_name] = {
            'status': status, 'c_count': len(c_presets), 'lvz_count': len(lvz_names),
            'missing': missing, 'extra': extra,
        }

    # ---- 输出报告 ----
    summary = {'SYNC': 0, 'MISSING_ONLY': 0, 'EXTRA_ONLY': 0, 'BOTH': 0, 'C_EMPTY': 0, 'NO_C_FILE': 0}
    for lvz_name in sorted(results):
        r = results[lvz_name]
        summary[r['status']] += 1
        if args.json:
            continue
        line = f"[{r['status']:<13}] {lvz_name:42s} C={r['c_count']:3d} lvz={r['lvz_count']:3d}"
        print(line)
        if lvz_name in TYPELESS_INPUTS_MODULES and r['status'] == 'C_EMPTY':
            print('      - 活数据格式：inputs N 无类型 token，依赖 loader ANY 容错加载（预期，不误报）')
        if r['missing']:
            print(f"      - 缺失 (C 有、.lvz 无): {', '.join(r['missing'])}")
        if r['extra']:
            print(f"      - 多出 (.lvz 有、C 无): {', '.join(r['extra'])}")

    if not args.json:
        print()
        print('状态说明:')
        print('  SYNC          : 两侧条目集合完全一致（含两侧皆空）')
        print('  MISSING_ONLY  : C 有 .lvz 没有的条目（纯缺失）→ --fix 可修复')
        print('  EXTRA_ONLY    : .lvz 有 C 没有的条目 → 预期差异（活数据扩展，不修）')
        print('  BOTH          : 双向都有差异 → 保守不修')
        print('  C_EMPTY       : C 无条目、.lvz 有条目 → 预期差异（死 C vs 活数据）')
        print('  NO_C_FILE     : .lvz 无对应 C 文件')
        print('  注：5 个 C_EMPTY 活数据模块（algebraic/basic_geometry/measurements/polygons/')
        print('      transformations）的 inputs 无类型 token，依赖 loader（module_lvz.c）ANY')
        print('      容错加载，属预期状态，不误报')
        print()
        print('汇总: ' + ', '.join(f'{k}={v}' for k, v in summary.items()))

    # ---- --fix 处理（仅纯缺失，且 C 侧非空；先备份） ----
    fixed, skipped = [], []
    if args.fix:
        for lvz_name in sorted(results):
            r = results[lvz_name]
            if r['status'] not in ('MISSING_ONLY',):
                skipped.append((lvz_name, r['status']))
                continue
            c_path = os.path.join(PRESET_C_DIR, lvz_name[:-4] + '.c')
            lvz_path = os.path.join(LVZ_DIR, lvz_name)
            c_presets, _, _, _, _ = compare_module(c_path, lvz_path, name_map)
            content = read_text_robust(c_path)
            category = convert_presets.extract_category(content)
            new_content = regen_lvz(c_path, lvz_path, c_presets, category)
            bak = lvz_path + '.bak'
            shutil.copy2(lvz_path, bak)
            with open(lvz_path, 'w', encoding='utf-8', newline='\n') as f:
                f.write(new_content)
            fixed.append((lvz_name, len(c_presets)))
            if not args.json:
                print(f'[FIX ] {lvz_name}: 备份 {os.path.basename(bak)}，重新生成 {len(c_presets)} 条')

        if not args.json:
            for name, st in skipped:
                print(f'[SKIP] {name}: 状态 {st}（非纯缺失，不修改）')
            print(f'\n--fix 完成: 修复 {len(fixed)} 个文件，跳过 {len(skipped)} 个文件')

    if args.json:
        print(json.dumps({'summary': summary, 'modules': results,
                          'fixed': [f[0] for f in fixed] if args.fix else None}, ensure_ascii=False))

    # ---- CI 门禁：同步差异含纯缺失/双向差异/缺 C 文件 → exit 1 ----
    if args.ci and not args.fix:
        problematic = summary.get('MISSING_ONLY', 0) + summary.get('BOTH', 0) + summary.get('NO_C_FILE', 0)
        if problematic > 0:
            sys.exit(1)


if __name__ == '__main__':
    main()

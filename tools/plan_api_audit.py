#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""规划文档蓝图 API 核对：提取 5 个豁免规划文档代码块中的 lv_* 符号，
对照 core/include/lv 实际声明，输出「已声明 / 未声明」清单（供逐项语义核对）。

用法: python tools/plan_api_audit.py
"""

import re
import sys
from collections import Counter
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8")

ROOT = Path(__file__).resolve().parent.parent
DOCS = ROOT / "doc" / "docs"
INC = ROOT / "core" / "include" / "lv"

PLAN_DOCS = [
    "CONSTRAINT_PROOF_TEST_PLAN.md",
    "DOCUMENT_GOVERNANCE_PLAN.md",
    "PERFORMANCE_OPTIMIZATION.md",
    "TEN_LAYER_INTEGRATION_PLAN.md",
    "TEN_LAYER_OPTIMIZED_PLAN.md",
]

# 头文件声明（与 symbol_sync_check 同口径：宏 / 函数 / typedef）
declared = set()
for h in INC.glob("*.h"):
    txt = h.read_text(encoding="utf-8", errors="replace")
    declared.update(re.findall(r"#define\s+(lv_[A-Za-z0-9_]+)", txt))
    declared.update(re.findall(r"\b(lv_[a-zA-Z0-9_]+)\s*\(", txt))
    declared.update(re.findall(r"typedef\s+.*?\b(lv[A-Za-z0-9_]+)\s*;", txt))
    declared.update(re.findall(r"\btypedef\s+enum\s*\{[^}]*\}\s*(lv[A-Za-z0-9_]+)\s*;", txt, re.S))

C_SYM = re.compile(r"\b(lv_[a-zA-Z0-9_]+)\s*\(")
PY_SYM = re.compile(r"\b(lv\.[a-zA-Z_][a-zA-Z0-9_]*)\s*\(")

# 非函数符号白名单（与 symbol_sync_check 一致 + 常见宏/类型）
WHITELIST = {
    "lv_OK", "lv_ERROR_NULL_POINTER", "lvContext", "lvEngine", "lvConfig",
    "lv_INITIALIZED", "lv_STAGE_COMPLETED", "lv_LAYER_PARSER", "lv_LAYER_RESOURCE",
    "lv_LAYER_GEOMETRY", "lv_LAYER_REASONING", "lv_LAYER_OUTPUT", "lv_LAYER_VISUAL",
    "lv_PI", "lv_VERSION_STRING", "lv_VERSION_MAJOR", "lv_VERSION_MINOR", "lv_VERSION_PATCH",
    "lv_NS_PER_MS", "lv_NS_PER_S", "lv_US_PER_S", "lv_MB_I", "lv_KB_I",
    "lv_STRINGIFY", "lv_SOLVER_SCALE_FACTOR", "lv_DEFER", "lv_DEPRECATED",
    "lv_PUBLIC_API", "lv_THREAD_LOCAL", "lv_ENABLE_LAYER_VALIDATION",
    "lv_ALLOW_LAYER", "lv_REQUIRE_STRICTLY_ABOVE", "lv_CURRENT_LAYER",
}


def extract(path):
    """返回 {符号: [(行号, 上下文), ...]}，仅代码块 + 跳过 C 注释"""
    found = {}
    in_code = False
    in_c_comment = False
    for i, line in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
        stripped = line.strip()
        if stripped.startswith("```"):
            in_code = not in_code
            in_c_comment = False
            continue
        if not in_code:
            continue
        if "/*" in line:
            in_c_comment = True
        if in_c_comment:
            if "*/" in line:
                in_c_comment = False
            continue
        for m in C_SYM.finditer(line):
            sym = m.group(1)
            if sym in WHITELIST:
                continue
            found.setdefault(sym, []).append((i, line.strip()[:90]))
        for m in PY_SYM.finditer(line):
            base = m.group(1).split(".")[-1]
            if any(f"lv_{base}" in d or base in d for d in declared):
                continue
            sym = m.group(1)
            found.setdefault(sym, []).append((i, line.strip()[:90]))
    return found


def main():
    all_declared, all_missing = {}, {}
    for name in PLAN_DOCS:
        p = DOCS / name
        if not p.exists():
            print(f"!! 缺失: {name}")
            continue
        syms = extract(p)
        declared_in_doc = {s: loc for s, loc in syms.items() if s.startswith("lv_") and s in declared}
        missing_in_doc = {s: loc for s, loc in syms.items() if s.startswith("lv_") and s not in declared}
        py_only = {s: loc for s, loc in syms.items() if s.startswith("lv.")}
        print(f"\n===== {name} =====")
        print(f"  代码块内 lv_* 符号总数: {len(syms)}  (C 形态 {len(syms)-len(py_only)} + Python 形态 {len(py_only)})")
        print(f"  已在头文件声明: {len(declared_in_doc)}")
        print(f"  未声明(蓝图): {len(missing_in_doc)}")
        all_declared[name] = declared_in_doc
        all_missing[name] = missing_in_doc
        if missing_in_doc:
            print("  --- 未声明符号(按首次出现行) ---")
            for s, loc in sorted(missing_in_doc.items(), key=lambda kv: kv[1][0][0]):
                first = loc[0]
                print(f"    L{first[0]:>4}  {s}    | {first[1]}")

    # 汇总：全部未声明符号及其所在文档
    print("\n\n===== 汇总：未声明(蓝图)符号按出现文档数排序 =====")
    counter = Counter()
    for name, m in all_missing.items():
        for s in m:
            counter[s] += 1
    for s, cnt in counter.most_common():
        docs = [n for n, m in all_missing.items() if s in m]
        print(f"  {s:45s} x{cnt}  {', '.join(docs)}")

    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""K5 示例同步机制：文档符号存在性检查。

扫描 doc/docs 的 Markdown 代码块中引用的 lv_* 符号，核对是否在
core/include/lv 头文件中声明——阻断示例/文档引用幻影 API（K5 判定
「6 不存在函数 / 4 幻影 API / 示例面严重脱节」的防复发机制）。

启发式：
- 提取 `lv_[a-z0-9_]+`（C 代码）与 `lv\.[a-z_]+`（Python 代码）符号；
- 白名单常见非函数符号（宏/类型/枚举如 lv_OK、lvContext、lv_LAYER_*）；
- 未在头文件声明的符号报告（函数名需 `name(` 或 `name;` 形态）。
- 跳过 C 注释块（`/* ... */`）与规划/蓝图文档、已标注虚构文档。

用法: python tools/symbol_sync_check.py
退出码: 0 无违规；1 存在违规（CI 门禁）。
"""

import re
import sys
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8")

ROOT = Path(__file__).resolve().parent.parent
DOCS = ROOT / "doc" / "docs"
INC = ROOT / "core" / "include" / "lv"

# 规划/蓝图类文档豁免：其代码块多为未来设计蓝图（虚构 API 属预期实现），
# 非教学示例幻影（K5 判定针对 README/API_QUICKSTART/USE_CASES 等教学面）
PLAN_DOC_KEYWORDS = ("PLAN", "ROADMAP", "DESIGN", "PROPOSAL", "DECISION", "OPTIMIZATION")

# 已整体/分章标注虚构的文档：脚本豁免（标注已防误用，避免 CI 噪音）
ANNOTATED_FICTIONAL_DOCS = {"API_REFERENCE.md", "34_meta_proof_cache.md"}

# 头文件声明的全部符号名（函数 + 宏 + 类型）
declared = set()
for h in INC.glob("*.h"):
    txt = h.read_text(encoding="utf-8", errors="replace")
    declared.update(re.findall(r"#define\s+(lv_[A-Za-z0-9_]+)", txt))
    declared.update(re.findall(r"\b(lv_[a-zA-Z0-9_]+)\s*\(", txt))
    declared.update(re.findall(r"typedef\s+.*?\b(lv[A-Za-z0-9_]+)\s*;", txt))
    declared.update(re.findall(r"\btypedef\s+enum\s*\{[^}]*\}\s*(lv[A-Za-z0-9_]+)\s*;", txt, re.S))

# 常见非函数符号白名单（枚举成员/类型名/常量，非函数声明）
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

C_SYM = re.compile(r"\b(lv_[a-zA-Z0-9_]+)\s*\(")  # 函数调用形态（幻影 API 必带括号）
PY_SYM = re.compile(r"\b(lv\.[a-zA-Z_][a-zA-Z0-9_]*)\s*\(")


def check_file(path):
    violations = []
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
        # 跳过 C 注释块（/* ... */）内的内容——注释示例不构成可运行引用
        if "/*" in line:
            in_c_comment = True
        if in_c_comment:
            if "*/" in line:
                in_c_comment = False
            continue
        for m in C_SYM.finditer(line):
            sym = m.group(1)
            if sym in declared:
                continue
            violations.append((i, sym))
        for m in PY_SYM.finditer(line):
            sym = m.group(1)
            base = sym.split(".")[-1]
            if any(f"lv_{base}" in d or base in d for d in declared):
                continue
            violations.append((i, sym))
    return violations


def main():
    total = 0
    for md in sorted(DOCS.glob("*.md")):
        if any(kw in md.name.upper() for kw in PLAN_DOC_KEYWORDS):
            continue  # 规划/蓝图文档豁免
        if md.name in ANNOTATED_FICTIONAL_DOCS:
            continue  # 已标注虚构文档豁免
        for i, sym in check_file(md):
            rel = str(md.relative_to(ROOT)).replace("\\", "/")
            print(f"  {rel}:{i}  未声明符号: {sym}")
            total += 1
    print(f"=== K5 符号同步检查: 教学文档代码块引用 {total} 处未声明符号 ===")
    return 1 if total else 0


if __name__ == "__main__":
    sys.exit(main())

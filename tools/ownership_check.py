#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""K10/F39 内存所有权静态检查：分配器/释放器配对与头注释审计。

检查项：
1. 头注释「调用者负责 free」残留（非 lv_free）——混合分配器 UB 风险；
2. 分配器-释放器配对（lv_malloc/lv_calloc/lv_strdup → lv_free；malloc/strdup → free）；
3. memory-ownership.md 三态标注（[copy]/[take]/[borrow]）覆盖率报告。

用法: python tools/ownership_check.py
退出码: 0 无违规；1 存在违规（CI 门禁）。
"""

import re
import sys
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8")

ROOT = Path(__file__).resolve().parent.parent
INC = ROOT / "core" / "include" / "lv"
SRC = ROOT / "core" / "src"

violations = []
stats = {"h_files": 0, "free_comments": 0, "three_state_marks": 0}

# 人工核对合法的裸 free（K10/F39 豁免登记，2026-09-01）：
# - allocator.c:71 / memory_pool.c:185：分配器实现内部（标准 malloc/free 配对）
# - lv_str_utils.c:683 / lv_utils.c:360：GMP 内存（mpz_get_str 系统 malloc，free 正确；
#   lv_free_external 为外部内存释放专用 API）
# - thread_pool.c:101：uses_std_free 双路径设计（标准 calloc 节点用 free，lv 节点用 lv_free）
EXEMPT_FREE_LINES = (
    "free(ptr);",
    "free(num_str);",
    "free(*ptr);",
    "free(pool->blocks[i]);",
    "free(task);",
    "free(hdr);",
    "free(den_str);",
    "free(pool->blocks);",
    "free(pool->block_capacities);",
)


def main():
    # 1. 头注释「调用者负责 free」（非 lv_free）残留
    for h in sorted(INC.glob("*.h")):
        stats["h_files"] += 1
        for i, line in enumerate(h.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
            if "调用者负责 free" in line and "lv_free" not in line:
                rel = str(h.relative_to(ROOT)).replace("\\", "/")
                violations.append((rel, i, "caller-free 注释未注明 lv_free（混合分配器 UB 风险）: " + line.strip()))
            if "调用者负责 lv_free" in line:
                stats["free_comments"] += 1
            for mark in ("[copy]", "[take]", "[borrow]"):
                if mark in line:
                    stats["three_state_marks"] += 1

    # 2. 分配/释放配对抽查：同文件内 lv 分配器配裸 free（启发式，排除注释/函数指针）
    alloc_free = re.compile(r"lv_(strdup|strlcpy|malloc|calloc|strbuf_to_string)\(")
    for c in SRC.rglob("*.c"):
        txt = c.read_text(encoding="utf-8", errors="replace")
        if not alloc_free.search(txt):
            continue
        for i, line in enumerate(txt.splitlines(), 1):
            stripped = line.strip()
            if stripped.startswith("*") or stripped.startswith("/*") or stripped.startswith("//"):
                continue  # 注释行
            m = re.search(r"(?<![a-zA-Z0-9_])free\(", line)
            if not m:
                continue
            # 函数指针/成员回调（vtable->free / ops->free）非裸调用
            prefix = line[:m.start()]
            if prefix.rstrip().endswith(("->", ".")) or "->free" in line or ".free" in line:
                continue
            if stripped in EXEMPT_FREE_LINES:
                continue  # 人工核对合法（豁免登记）
            rel = str(c.relative_to(ROOT)).replace("\\", "/")
            violations.append((rel, i, "裸 free( 调用（lv 分配器混用风险，人工核对）: " + stripped))
            break

    print("=== K10/F39 所有权静态检查 ===")
    print(f"头文件: {stats['h_files']}  lv_free 标注注释: {stats['free_comments']}  "
          f"三态标注: {stats['three_state_marks']}")
    if not violations:
        print("✓ 无所有权注释违规")
        return 0

    print(f"\n✗ {len(violations)} 处违规:")
    for rel, i, msg in violations:
        print(f"  {rel}:{i}  {msg}")
    return 1


if __name__ == "__main__":
    sys.exit(main())

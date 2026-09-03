#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""K40/B5 头清单漂移校验：CMakeLists lv_HEADERS 手写清单 vs 磁盘 core/include/lv。

lv_HEADERS 是 IDE 索引与构建追踪用的共享头清单（5 个 OBJECT 库共用）。
历史漂移 83-85 个 .h 未列（仅影响 IDE 索引，安装走 DIRECTORY 不受影响）；
本脚本确保清单与磁盘一致，新增头文件未登记即失败（防再漂移）。

用法:
  python tools/check_headers_sync.py            # 打印差异，退出码 0/1
  python tools/check_headers_sync.py --ci       # 与默认相同（差异即 exit 1）

退出码: 0 无差异；1 存在差异（CI 门禁）。
"""

import argparse
import re
import sys
from pathlib import Path

# Windows 控制台 GBK 无法编码部分字符：强制 UTF-8 输出
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")
    sys.stderr.reconfigure(encoding="utf-8")

ROOT = Path(__file__).resolve().parent.parent
CMAKE_FILE = ROOT / "CMakeLists.txt"
HEADER_DIR = ROOT / "core" / "include" / "lv"

BLOCK_RE = re.compile(r"set\(lv_HEADERS\n(.*?)\n\)\n", re.S)
HEADER_RE = re.compile(r"core/include/lv/([\w.]+\.h)")


def listed_headers(text: str):
    m = BLOCK_RE.search(text)
    if not m:
        raise SystemExit("CMakeLists.txt: lv_HEADERS 块未找到")
    return set(HEADER_RE.findall(m.group(1)))


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--ci", action="store_true", help="CI 门禁模式（差异即 exit 1，与默认相同）")
    ap.parse_args()

    cmake_text = CMAKE_FILE.read_text(encoding="utf-8")
    listed = listed_headers(cmake_text)
    disk = {p.name for p in HEADER_DIR.glob("*.h")}

    missing = sorted(disk - listed)  # 磁盘有、清单缺
    extra = sorted(listed - disk)    # 清单有、磁盘缺

    print(f"=== lv_HEADERS 清单同步检查 === 清单 {len(listed)} / 磁盘 {len(disk)}")
    ok = True
    if missing:
        ok = False
        print(f"✗ 磁盘有但清单缺 {len(missing)} 个（需加入 CMakeLists lv_HEADERS）:")
        for h in missing:
            print(f"    core/include/lv/{h}")
    if extra:
        ok = False
        print(f"✗ 清单有但磁盘缺 {len(extra)} 个（文件可能已删除）:")
        for h in extra:
            print(f"    core/include/lv/{h}")
    if ok:
        print("✓ 清单与磁盘一致，无漂移")
    else:
        print("修复: 新增头文件请在 CMakeLists.txt lv_HEADERS 块按字母序补一行")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

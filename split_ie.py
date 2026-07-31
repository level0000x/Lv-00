# -*- coding: utf-8 -*-
"""Split interop_export.c (1940 lines) into per-format sub-modules."""
import os

SRC = r"c:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\core\src\layer5_output\interop\interop_export.c"
DST = r"c:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\core\src\layer5_output\interop"

with open(SRC, "r", encoding="utf-8") as f:
    lines = f.readlines()

def block(a, b):
    """1-based inclusive line range -> string."""
    return "".join(lines[a-1:b])

FILE_HDR = """/**
 * @file %s
 * @brief 导出 —— %s
 */

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "lv/constraint_graph.h"
#include "lv/engine.h"
#include "lv/interop.h"
#include "lv/lv_json.h"

#include "debug.h"
#include "interop_export_internal.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_str_utils.h"

"""

def write_file(name, desc, content):
    path = os.path.join(DST, name)
    with open(path, "w", encoding="utf-8") as f:
        f.write(FILE_HDR % (name, desc))
        f.write("\n")
        f.write(content)
    print(name, "written,", len(content.splitlines()), "lines")

# ---------- 1. coq : 131-264 ----------
write_file("interop_export_coq.c", "Coq 定理证明导出", block(131, 264))

# ---------- 2. lean : 265-529 ----------
write_file("interop_export_lean.c", "Lean 定理证明导出", block(265, 529))

# ---------- 3. html : 530-690 ----------
write_file("interop_export_html.c", "HTML 导出", block(530, 690))

# ---------- 4. svg : svg_escape_string 104-130 + export_svg 691-1178 ----------
svg_body = block(104, 130) + "\n" + block(691, 1178)
write_file("interop_export_svg.c", "SVG 导出", svg_body)

# ---------- 5. tikz : 1179-1660 ----------
write_file("interop_export_tikz.c", "TikZ 导出（含 fragment）", block(1179, 1660))

# ---------- 6. canonical : 1661-1802 ----------
write_file("interop_export_canonical.c", "Canonical 导出", block(1661, 1802))

# ---------- 7. geojson : 1803-1923 ----------
write_file("interop_export_geojson.c", "GeoJSON 导出", block(1803, 1923))

# ---------- main : 1-103 + GGB 宏 1924-1940 ----------
main_body = block(1, 103) + "\n" + block(1924, 1940)
with open(SRC, "w", encoding="utf-8") as f:
    f.write(main_body)
print("main written,", len(main_body.splitlines()), "lines")

print("DONE")

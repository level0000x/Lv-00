#!/usr/bin/env python3
"""
拆分 euclidean_geometry.c (1569 lines) 为 8 个模块 + 1 容器文件。

分区边界（基于文件中 /* ====== */ 注释分隔符）：
  Part 1:  上下文生命周期管理               lines 121-186
  Part 2:  公理体系配置                     lines 188-298
  Part 3:  几何实体声明                     lines 300-449
  Part 4:  几何谓词断言                     lines 451-589
  Part 5:  定理验证与一致性检查              lines 591-690
  Part 6:  导出                             lines 692-764
  Part 7:  等价性证明框架                   lines 766-923
  Part 8:  内部辅助函数                     lines 925-1569
"""

import os
import shutil

SRC = os.path.join(os.path.dirname(__file__),
                   "core", "src", "layer3_geometry", "euclidean_geometry.c")
DST_DIR = os.path.dirname(SRC)

# 备份原文件
backup = SRC + ".bak"
if not os.path.exists(backup):
    shutil.copy2(SRC, backup)
    print(f"已备份原文件到 {backup}")

with open(SRC, "r", encoding="utf-8") as f:
    lines = f.readlines()

# ------------------------------------------------------------
# 切片定义: (输出文件名, 起始行号(1-based), 结束行号(含), 描述)
# ------------------------------------------------------------
SLICES = [
    ("euclidean_geometry_context.c",       121, 186, "上下文生命周期管理"),
    ("euclidean_geometry_axiom.c",         188, 298, "公理体系配置"),
    ("euclidean_geometry_declare.c",       300, 449, "几何实体声明"),
    ("euclidean_geometry_predicate.c",     451, 589, "几何谓词断言"),
    ("euclidean_geometry_consistency.c",   591, 690, "定理验证与一致性检查"),
    ("euclidean_geometry_export.c",        692, 764, "导出"),
    ("euclidean_geometry_equivalence.c",   766, 923, "等价性证明框架"),
    ("euclidean_geometry_helpers.c",       925, 1569, "内部辅助函数"),
]

# 每个模块需要包含的头文件
MODULE_INCLUDES = """#include "euclidean_geometry.h"
#include "euclidean_geometry_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/lv_check.h"

#include "debug.h"
#include "error_codes.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "symbolic_coord.h"
"""

# helpers 模块不需要 include internal.h（自己实现）
MODULE_INCLUDES_HELPERS = """#include "euclidean_geometry.h"
#include "euclidean_geometry_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/lv_check.h"

#include "debug.h"
#include "error_codes.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "symbolic_coord.h"
"""

# Part 6 (export) 不需要 symbolic_coord.h
MODULE_INCLUDES_EXPORT = """#include "euclidean_geometry.h"
#include "euclidean_geometry_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/lv_check.h"

#include "debug.h"
#include "error_codes.h"
#include "lv_internal.h"
#include "lv_utils.h"
"""


def write_module(filename, start, end, description, includes):
    """从 lines[start-1:end] 提取内容并写入模块文件"""
    outpath = os.path.join(DST_DIR, filename)
    content_lines = lines[start - 1:end]
    # 去除末尾空白行
    while content_lines and content_lines[-1].strip() == "":
        content_lines.pop()

    module_comment = f"""/**
 * @file {filename}
 * @brief 欧几里得几何公理体系实现 —— {description}
 *
 * @details 本文件由 euclidean_geometry.c 拆分而来，是 {description} 模块。
 *          原文件按功能域拆分为 8 个模块，通过容器文件 euclidean_geometry.c 聚合。
 *
 * @date 2026-08-02
 */

"""
    with open(outpath, "w", encoding="utf-8", newline="\n") as f:
        f.write(module_comment)
        f.write(includes)
        f.write("\n")
        for line in content_lines:
            f.write(line)
    print(f"  -> {filename}  ({len(content_lines)} 行)")


# ------------------------------------------------------------
# 1. 写模块文件
# ------------------------------------------------------------
print("=== 生成模块文件 ===")
for fname, s, e, desc in SLICES:
    if fname == "euclidean_geometry_export.c":
        inc = MODULE_INCLUDES_EXPORT
    elif fname == "euclidean_geometry_helpers.c":
        inc = MODULE_INCLUDES_HELPERS
    else:
        inc = MODULE_INCLUDES
    write_module(fname, s, e, desc, inc)

# 对 helpers 模块，移除 static 关键字（共享函数需要被其他模块调用）
helpers_path = os.path.join(DST_DIR, "euclidean_geometry_helpers.c")
with open(helpers_path, "r", encoding="utf-8") as f:
    content = f.read()

# 需要取消 static 的函数列表（按声明顺序）
non_static_funcs = [
    "euclidean_axiom_mask_offset",
    "euclidean_point_is_registered",
    "euclidean_line_is_registered",
    "euclidean_circle_is_registered",
    "euclidean_register_point_id",
    "euclidean_register_line_id",
    "euclidean_register_circle_id",
    "euclidean_verify_axiom_inconsistency",
    "euclidean_build_birkhoff_to_tarski_map",
    "euclidean_build_tarski_to_birkhoff_map",
    "euclidean_set_inconsistency",
    "euclidean_clear_inconsistency",
]

for func in non_static_funcs:
    # 匹配 "static <return_type> <func>(" 或 "static <return_type>\n<func>("
    import re
    # 匹配单独一行的 static 声明
    pattern = re.compile(r'^static\s+(.+\s+\*?' + re.escape(func) + r'\s*\()', re.MULTILINE)
    content = pattern.sub(r'\1', content)

with open(helpers_path, "w", encoding="utf-8", newline="\n") as f:
    f.write(content)
print("  已移除 helpers 中共享函数的 static 关键字")

# 检查 symbolic_check_collinear 等函数是否仍为 static（它们应该保持 static）
# 检查 graph_find_collinear_constraint 等
should_stay_static = [
    "graph_find_collinear_constraint",
    "graph_find_congruence_constraint",
    "symbolic_check_collinear",
    "symbolic_check_between",
    "symbolic_check_segment_congruent",
]
for func in should_stay_static:
    import re
    if re.search(r'^static\s+.+\b' + re.escape(func) + r'\s*\(', content, re.MULTILINE):
        print(f"  [OK] {func} 保持 static")
    else:
        print(f"  [WARN] {func} 可能丢失了 static 关键字")

# ------------------------------------------------------------
# 2. 重写容器文件 euclidean_geometry.c
# ------------------------------------------------------------
print("\n=== 重写容器文件 ===")

# 容器文件：保留文件头注释 + 包含头文件 + 常量 + 公理名称映射 + include 模块文件
container_header = lines[0:31]  # 文件头注释 (lines 1-31)
container_includes = lines[32:51]  # 包含头文件 (lines 33-51)
container_constants = lines[52:85]  # 常量定义 (lines 53-85)
container_axiom_names = lines[112:119]  # 公理体系名称字符串 (lines 113-119)

# 不包含前向声明部分 (lines 87-111)，改为 include internal.h
internal_header_include = '#include "euclidean_geometry_internal.h"\n'

# 模块 include 列表
module_includes = [
    '#include "euclidean_geometry_context.c"\n',
    '#include "euclidean_geometry_axiom.c"\n',
    '#include "euclidean_geometry_declare.c"\n',
    '#include "euclidean_geometry_predicate.c"\n',
    '#include "euclidean_geometry_consistency.c"\n',
    '#include "euclidean_geometry_export.c"\n',
    '#include "euclidean_geometry_equivalence.c"\n',
    '#include "euclidean_geometry_helpers.c"\n',
]

container_path = SRC
with open(container_path, "w", encoding="utf-8", newline="\n") as f:
    # 文件头
    for line in container_header:
        f.write(line)
    f.write("\n")

    # 包含头文件（在原 includes 基础上加上 internal.h）
    for line in container_includes:
        f.write(line)
    f.write(internal_header_include)
    f.write("\n")

    # 常量定义
    for line in container_constants:
        f.write(line)
    f.write("\n")

    # 公理名称映射
    for line in container_axiom_names:
        f.write(line)
    f.write("\n")

    # 模块 include
    for inc in module_includes:
        f.write(inc)

    # 末尾注释
    f.write("\n/* ========================================================================\n")
    f.write(" * 拆分说明\n")
    f.write(" * ========================================================================\n")
    f.write(" *\n")
    f.write(" * 本文件已按功能域拆分为以下模块：\n")
    for fname, s, e, desc in SLICES:
        f.write(f" * - {fname:<45s} {desc}\n")
    f.write(" * ======================================================================== */\n")

print(f"  -> 容器文件已重写: {container_path}")

print("\n=== 完成 ===")
# -*- coding: utf-8 -*-
"""Split PDF-export segment from interop_export.c into interop_export_pdf.c."""
import os

BASE = r'c:\Users\xingg\Desktop\知识体系化Wiki\Lv-00'
ENGINE = os.path.join(BASE, r'core\src\layer5_output\interop\interop_export.c')
PDF_C = os.path.join(BASE, r'core\src\layer5_output\interop\interop_export_pdf.c')
INTERNAL_H = os.path.join(BASE, r'core\src\layer5_output\interop\interop_export_internal.h')
CMAKE = os.path.join(BASE, 'CMakeLists.txt')

def read_file(p):
    with open(p, 'r', encoding='utf-8-sig') as f:
        return f.read()

def write_file(p, s):
    with open(p, 'w', encoding='utf-8', newline='') as f:
        f.write(s)

def find_first(lines, pred):
    for i, l in enumerate(lines):
        if pred(l):
            return i
    return None

lines = read_file(ENGINE).split('\n')

# PDF segment: doxygen comment (1922) .. end before GeoGebra section (2467)
anchor = find_first(lines, lambda l: '* @brief 将约束图导出为 PDF 文档' in l)
assert anchor is not None
seg_start = anchor
while not lines[seg_start].strip().startswith('/**'):
    seg_start -= 1

geogebra = find_first(lines, lambda l: 'GeoGebra 导入辅助' in l)
assert geogebra is not None
seg_end = geogebra
while seg_end > seg_start and not lines[seg_end - 1].strip():
    seg_end -= 1

seg = lines[seg_start:seg_end]
assert 'int interop_export_pdf(' in '\n'.join(seg)

pdf_c_content = (
    '/**\n'
    ' * @file interop_export_pdf.c\n'
    ' * @brief 约束图 PDF 导出实现（从 interop_export.c 拆分）\n'
    ' *\n'
    ' * @details 最小化纯 C 实现（无外部库依赖）：直接输出 PDF 1.4 文件结构，\n'
    ' *          包含图形流、xref 交叉引用表与文本/图形状态管理。\n'
    ' */\n'
    '\n'
    '#include "interop_export_internal.h"\n'
    '\n'
    '#include <float.h>\n'
    '#include <math.h>\n'
    '#include <stdint.h>\n'
    '#include <stdio.h>\n'
    '#include <stdlib.h>\n'
    '#include <string.h>\n'
    '\n'
    '#include "lv/constraint_graph.h"\n'
    '#include "lv/engine.h"\n'
    '#include "lv/interop.h"\n'
    '#include "lv/lv_json.h"\n'
    '\n'
    '#include "debug.h"\n'
    '#include "lv_internal.h"\n'
    '#include "lv_utils.h"\n'
    '#include "lv/lv_strbuf.h"\n'
    '#include "lv/lv_str_utils.h"\n'
    '\n'
) + '\n'.join(seg) + '\n'
write_file(PDF_C, pdf_c_content)
print('interop_export_pdf.c written,', len(seg), 'lines')

# internal.h
ih_content = (
    '/**\n'
    ' * @file interop_export_internal.h\n'
    ' * @brief 互操作导出内部共享声明（interop_export_pdf.c 与 interop_export.c 共用）\n'
    ' */\n'
    '\n'
    '#ifndef lv_INTEROP_EXPORT_INTERNAL_H\n'
    '#define lv_INTEROP_EXPORT_INTERNAL_H\n'
    '\n'
    '#include "lv/constraint_graph.h"\n'
    '\n'
    '#ifdef __cplusplus\n'
    'extern "C" {\n'
    '#endif\n'
    '\n'
    '/* 计算约束图的包围盒（SVG 与 PDF 导出共用） */\n'
    'void compute_bounding_box(const ConstraintGraph *graph, double *min_x, double *min_y, double *max_x,\n'
    '                          double *max_y);\n'
    '\n'
    '#ifdef __cplusplus\n'
    '}\n'
    '#endif\n'
    '\n'
    '#endif /* lv_INTEROP_EXPORT_INTERNAL_H */\n'
)
write_file(INTERNAL_H, ih_content)
print('interop_export_internal.h written')

# modify engine.c
new_lines = []
for i, l in enumerate(lines):
    if seg_start <= i < seg_end:
        continue
    if l.strip().startswith('static void compute_bounding_box('):
        l = l.replace('static void compute_bounding_box(', 'void compute_bounding_box(', 1)
    new_lines.append(l)

out = []
added = False
for l in new_lines:
    out.append(l)
    if l.strip() == '#include "debug.h"' and not added:
        out.append('#include "interop_export_internal.h"')
        added = True
assert added, 'include anchor not found'
write_file(ENGINE, '\n'.join(out))
print('interop_export.c updated,', len(out), 'lines')

cm = read_file(CMAKE)
old_entry = '    core/src/layer5_output/interop/interop_export.c\n'
assert cm.count(old_entry) == 1, 'CMake entry count: %d' % cm.count(old_entry)
cm = cm.replace(old_entry, old_entry + '    core/src/layer5_output/interop/interop_export_pdf.c\n')
write_file(CMAKE, cm)
print('CMakeLists.txt updated')
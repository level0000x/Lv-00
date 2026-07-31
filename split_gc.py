# -*- coding: utf-8 -*-
"""Split geometry_compress.c (2313 lines) into 12 files + internal header."""
import io
import os

SRC = r"c:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\core\src\layer3_geometry\geometry_compress.c"
DIR = os.path.dirname(SRC)

with io.open(SRC, "rb") as f:
    raw = f.read()
text = raw.decode("utf-8-sig")
lines = text.splitlines(keepends=True)
assert len(lines) == 2313, "line count mismatch: %d" % len(lines)

def seg(a, b):
    return lines[a - 1:b]

# 段边界（跳过段间空行）
head_parts = [seg(1, 45)]           # 头注释 + includes
# huff_heap_compare 定义 + config 段
main_extra = "".join(seg(121, 125)) + "\n" + "".join(seg(138, 151))
main_extra = main_extra.replace(
    "static int huff_heap_compare(const void *a, const void *b) {",
    "int huff_heap_compare(const void *a, const void *b) {", 1)
main_extra = main_extra.replace(
    "static CompressConfig compress_config_default(void) {",
    "CompressConfig compress_config_default(void) {", 1)

tri_parts = [seg(152, 304)]
pre_parts = [seg(305, 769)]
crd_parts = [seg(770, 796)]
edg_parts = [seg(797, 1067)]
bit_parts = [seg(1068, 1071), seg(1091, 1148)]   # 注释头 + 函数（跳过 BitWriter/BitReader 定义 1072-1090）
huf_parts = [seg(1149, 1490)]
rle_parts = [seg(1491, 1584)]
ent_parts = [seg(1585, 1718)]
clo_parts = [seg(1719, 1810)]
cpr_parts = [seg(1811, 2017)]
dcp_parts = [seg(2018, 2194)]
io_parts = [seg(2195, 2313)]

# 覆盖检查
covered = 45 + 5 + 14 + sum(len(p[0]) for p in
    [tri_parts, pre_parts, crd_parts, edg_parts, bit_parts, huf_parts,
     rle_parts, ent_parts, clo_parts, cpr_parts, dcp_parts, io_parts])
# 未覆盖：46-120（常量+结构体区，迁移到 internal.h）、126-137、152 之前空行等
uncovered = 120 - 45 + 4 + 14  # 46-120 常量结构体(75行) + 126-129(4) + 137(1) = 80
# 手动核对：总 2313 = covered + internal.h 迁移内容(46-120 75行 + 126-129 4行 + 137 前向声明 1行 = 80) + 段间空行跳过数
print("covered=%d, to-internal=106" % covered)
assert covered + 106 == 2313, "coverage check failed: %d" % covered

# ---- main ----
main_text = "".join(head_parts[0]) + "\n#include \"geometry_compress_internal.h\"\n\n" + main_extra

internal_h = '''/**
 * @file geometry_compress_internal.h
 * @brief Internal shared definitions for geometry compression engine.
 */

#ifndef lv_GEOMETRY_COMPRESS_INTERNAL_H
#define lv_GEOMETRY_COMPRESS_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "geometry_compress.h"

/* ---- constants ---- */
#define BOUNDARY_STACK_INITIAL 64
#define CLERS_SEQUENCE_INITIAL 256
#define LVZD_READ_BUFFER_INITIAL 4096

#ifndef COORD_DIM
#define COORD_DIM 2
#endif

#define LVZD_COMPRESS_MAGIC 0x4C564300 /* "LVZC" */
#define MAX_ADJACENT_FACES 16
#define HUFFMAN_MAX_NODES 511
#define HUFFMAN_MAX_CODE_LEN 256

/* ---- internal structures ---- */
typedef struct {
    int v0;
    int v1;
} Edge;

typedef struct {
    int verts[3];
    bool visited;
} TriangleFace;

typedef struct {
    int left;
    int right;
    int parent;
    uint32_t freq;
    uint8_t byte_val;
} HuffmanNode;

typedef struct {
    int node_index;
    uint32_t freq;
} HuffHeapElem;

typedef struct {
    uint32_t code;
    int length;
} HuffmanCode;

typedef struct {
    uint8_t *buf;
    size_t capacity;
    size_t byte_pos;
    int bit_pos;
} BitWriter;

typedef struct {
    const uint8_t *buf;
    size_t size;
    size_t byte_pos;
    int bit_pos;
} BitReader;

/* ---- cross-section helpers (defined in geometry_compress.c) ---- */
int huff_heap_compare(const void *a, const void *b);
CompressConfig compress_config_default(void);
void huffman_generate_codes(const HuffmanNode *hnodes, int root, HuffmanCode *codes);

#ifdef __cplusplus
}
#endif

#endif /* lv_GEOMETRY_COMPRESS_INTERNAL_H */
'''

def file_header(name, desc):
    return '/*\n' \
           ' * @file %(name)s\n' \
           ' * @brief Geometry compression engine - %(desc)s\n' \
           ' * @details Split from geometry_compress.c\n' \
           ' */\n\n' \
           '#include "geometry_compress.h"\n' \
           '#include "geometry_compress_internal.h"\n\n' \
           '#include "lv/lv_file.h"\n\n' \
           '#include <math.h>\n' \
           '#include <stdio.h>\n' \
           '#include <stdlib.h>\n' \
           '#include <string.h>\n\n' \
           '#include "lv/constraint_graph.h"\n' \
           '#include "lv/lv_heap.h"\n\n' \
           '#include "lv_internal.h"\n' \
           '#include "lv_utils.h"\n' \
           '#include "node_deep_copy.h"\n' \
           '#include "symbolic_coord.h"\n\n' % {"name": name, "desc": desc}


def join_parts(parts):
    out = []
    for i, p in enumerate(parts):
        out.append("".join(p))
        if i < len(parts) - 1:
            out.append("\n")
    return "".join(out)


files = {
    "geometry_compress_triangle.c": (file_header("geometry_compress_triangle.c", "triangle face extraction"), tri_parts),
    "geometry_compress_predict.c": (file_header("geometry_compress_predict.c", "predictive encoding"), pre_parts),
    "geometry_compress_coords.c": (file_header("geometry_compress_coords.c", "public predictive coords interface"), crd_parts),
    "geometry_compress_edgebreaker.c": (file_header("geometry_compress_edgebreaker.c", "edgebreaker CLERS encoding"), edg_parts),
    "geometry_compress_bitio.c": (file_header("geometry_compress_bitio.c", "bit-level I/O"), bit_parts),
    "geometry_compress_huffman.c": (file_header("geometry_compress_huffman.c", "huffman coding"), huf_parts),
    "geometry_compress_rle.c": (file_header("geometry_compress_rle.c", "run-length encoding"), rle_parts),
    "geometry_compress_entropy.c": (file_header("geometry_compress_entropy.c", "real entropy coding"), ent_parts),
    "geometry_compress_clone.c": (file_header("geometry_compress_clone.c", "graph deep copy"), clo_parts),
    "geometry_compress_main.c": (file_header("geometry_compress_main.c", "compress pipeline"), cpr_parts),
    "geometry_compress_decompress.c": (file_header("geometry_compress_decompress.c", "decompress pipeline"), dcp_parts),
    "geometry_compress_io.c": (file_header("geometry_compress_io.c", "lvzd file I/O"), io_parts),
}

for fname, (header, parts) in files.items():
    body = header + join_parts(parts)
    with io.open(os.path.join(DIR, fname), "w", encoding="utf-8", newline="\n") as f:
        f.write(body)
    print("written %s (%d lines)" % (fname, body.count("\n") + 1))

with io.open(SRC, "w", encoding="utf-8", newline="\n") as f:
    f.write(main_text)
print("rewritten geometry_compress.c (%d lines)" % (main_text.count("\n") + 1))

with io.open(os.path.join(DIR, "geometry_compress_internal.h"), "w", encoding="utf-8", newline="\n") as f:
    f.write(internal_h)
print("written geometry_compress_internal.h (%d lines)" % (internal_h.count("\n") + 1))

print("DONE")


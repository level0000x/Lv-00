# -*- coding: utf-8 -*-
"""Move CSGBSP types to internal.h; remove struct defs from main; fix mesh.c."""
import io, os
DIR = r"c:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\core\src\layer3_geometry"
def read(p):
    with io.open(p, "rb") as f:
        return f.read().decode("utf-8-sig").replace("\r\n", "\n")
def write(p, s):
    with io.open(p, "w", encoding="utf-8", newline="\n") as f:
        f.write(s)

# 1. mesh.c: 删除 CSGBSPClass/CSGBSPNode 定义（200-222 行区域）
p = os.path.join(DIR, "geometry_csg_mesh.c")
t = read(p)
start = t.find("typedef enum {\n    CSG_BSP_FRONT")
end = t.find("} CSGBSPNode;") + len("} CSGBSPNode;")
assert start >= 0 and end > start
removed = t[start:end]
t = t[:start] + t[end:]
write(p, t)
print("mesh.c removed CSGBSP types")

# 2. internal.h: 添加 CSGBSP 类型
p = os.path.join(DIR, "geometry_csg_internal.h")
t = read(p)
anchor = "/* ---- shared helpers (defined in geometry_csg.c) ---- */"
types = '''/* ---- BSP types (moved from geometry_csg_mesh.c) ---- */
typedef enum {
    CSG_BSP_FRONT = 0,
    CSG_BSP_BACK  = 1,
    CSG_BSP_ON    = 2,
    CSG_BSP_SPLIT = 3
} CSGBSPClass;

typedef struct CSGBSPNode {
    CSGVec3 plane_point;
    CSGVec3 plane_normal;
    struct CSGBSPNode *front;
    struct CSGBSPNode *back;
    CSGTriangle *tris;
    int tri_count;
    int tri_capacity;
} CSGBSPNode;

'''
assert anchor in t
t = t.replace(anchor, types + anchor)
write(p, t)
print("internal.h CSGBSP types added")

# 3. main: 删除 CSGVec3/CSGTriangle/CSGTriList/CSGMesh 结构体定义
p = os.path.join(DIR, "geometry_csg.c")
t = read(p)
# 从 "/**\n * @brief 3D 向量" 到 "} CSGMesh;" 删除
start = t.find("/**\n * @brief 3D \u5411\u91cf")
end = t.find("} CSGMesh;") + len("} CSGMesh;")
assert start >= 0 and end > start, (start, end)
t = t[:start] + t[end:]
write(p, t)
print("main.c removed struct defs")

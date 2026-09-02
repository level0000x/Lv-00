#ifndef lv_GEOMETRY_TYPES_H
#define lv_GEOMETRY_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */
#include "lv/lv_platform.h"
#include <stdbool.h>

/* ── CSG 图元参数数据结构 ── */
typedef struct {
    int type;         /* 图元类型：0=sphere, 1=box, 2=cylinder, 3=cone */
    double params[6]; /* 参数：radius, radius2, height, width, depth, unused */
} CSGNodePrimitive;

/* ── CSG 节点类型枚举 ── */
typedef enum {
    CSG_NODE_PRIMITIVE = 0,
    CSG_NODE_UNION = 1,
    CSG_NODE_DIFFERENCE = 2,
    CSG_NODE_INTERSECTION = 3,
    CSG_NODE_TRANSFORM = 4,
    CSG_NODE_HULL = 5,
    CSG_NODE_MINKOWSKI = 6,
    CSG_NODE_EXTRUDE_LINEAR = 7,
    CSG_NODE_EXTRUDE_ROTATE = 8
} CSGNodeKind;

/* ── CSG 构造树节点 ── */
typedef struct CSGNode CSGNode;
typedef struct CSGBBox {
    double min[3];
    double max[3];
} CSGBBox;

struct CSGNode {
    CSGNodeKind kind;
    CSGNode **children;
    int child_count;
    int child_capacity;
    int func_block_id;
    double transform[4][4];
    double bbox_min[3];
    double bbox_max[3];
    union {
        CSGNodePrimitive prim;
    } data;
    CSGBBox bbox; /* cached */
};

/* ── CSG 节点 API ── */
CSGNode *csg_node_create(CSGNodeKind kind);
lv_PUBLIC_API void csg_node_destroy(CSGNode *node);
lv_PUBLIC_API void csg_node_add_child(CSGNode *parent, CSGNode *child);
lv_PUBLIC_API void csg_node_init_bbox(CSGNode *node);

/* ── 基本图元 ── */
CSGNode *csg_sphere_create(double radius);
CSGNode *csg_box_create(double width, double height, double depth);
CSGNode *csg_cylinder_create(double radius, double height);
CSGNode *csg_cone_create(double radius1, double radius2, double height);

/* ── 布尔运算 ── */
CSGNode *geometry_csg_union(CSGNode *a, CSGNode *b);
CSGNode *geometry_csg_difference(CSGNode *a, CSGNode *b);
CSGNode *geometry_csg_intersection(CSGNode *a, CSGNode *b);

/* ── 导出 ── */
lv_PUBLIC_API char *geometry_csg_export_scad(const CSGNode *root, const char *name);

#ifdef __cplusplus
}
#endif
#endif

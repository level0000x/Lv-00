/*
 * @file geometry_csg_export.c
 * @brief CSG geometry module - openscad export and examples
 * @details Split from geometry_csg.c
 */

#include "lv/lv_platform.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "geometry_types.h"
#include "geometry_csg_internal.h"
#include "lv_internal.h"
#include "lv_utils.h"

void csg_evaluate(const CSGNode *node, CSGTriList *out) {
    if (!node || !out)
        return;
    if (node->kind >= 0 && node->kind < s_eval_func_count && s_eval_funcs[node->kind]) {
        s_eval_funcs[node->kind](node, out);
    }
}

/* ================================================================
 * OpenSCAD .scad 导出
 * ================================================================ */

/**
 * @brief 内部递归函数：将 CSG 子树转为 OpenSCAD 脚本片段
 *
 * @param node    当前节点
 * @param buf     输出缓冲区
 * @param buf_size 缓冲区容量
 * @param written 已写入的字符数
 * @param indent  缩进层级
 * @return 更新后的已写入字符数，出错返回 -1
 */
static int csg_export_node(const CSGNode *node, char *buf, int buf_size, int written, int indent) {
    if (!node || !buf || written < 0 || written >= buf_size)
        return written;

    /* 生成缩进空格 */
    char indent_str[33];
    int indent_len = indent * 2;
    if (indent_len > 32)
        indent_len = 32;
    memset(indent_str, ' ', (size_t) indent_len);
    indent_str[indent_len] = '\0';

    int n = 0;

    switch (node->kind) {
        case CSG_NODE_PRIMITIVE: {
            int ptype = node->data.prim.type;
            double *p = node->data.prim.params;

            switch (ptype) {
                case 0: /* 球体 */
                    n = snprintf(buf + written, (size_t) (buf_size - written), "%ssphere(r=%.10g);\n", indent_str,
                                 p[0]);
                    break;
                case 1: /* 立方体 */
                    n = snprintf(buf + written, (size_t) (buf_size - written),
                                 "%scube([%.10g, %.10g, %.10g], center=true);\n", indent_str, p[0], p[1], p[2]);
                    break;
                case 2: /* 圆柱体 */
                    n = snprintf(buf + written, (size_t) (buf_size - written),
                                 "%scylinder(r=%.10g, h=%.10g, center=true);\n", indent_str, p[0], p[1]);
                    break;
                default:
                    n = snprintf(buf + written, (size_t) (buf_size - written), "%s// unknown primitive type %d\n",
                                 indent_str, ptype);
                    break;
            }
            if (n > 0)
                written += n;
            if (n < 0)
                lv_RETURN_ERROR(lv_ERROR_INTERNAL, "csg_export_node: snprintf failed for primitive");
            break;
        }

        case CSG_NODE_UNION:
        case CSG_NODE_DIFFERENCE:
        case CSG_NODE_INTERSECTION: {
            const char *op_name = "union";
            if (node->kind == CSG_NODE_DIFFERENCE)
                op_name = "difference";
            if (node->kind == CSG_NODE_INTERSECTION)
                op_name = "intersection";

            n = snprintf(buf + written, (size_t) (buf_size - written), "%s%s() {\n", indent_str, op_name);
            if (n > 0)
                written += n;
            else if (n < 0)
                lv_RETURN_ERROR(lv_ERROR_INTERNAL, "csg_export_node: snprintf failed for boolean op");

            for (int i = 0; i < node->child_count; i++) {
                written = csg_export_node(node->children[i], buf, buf_size, written, indent + 1);
                if (written < 0)
                    lv_RETURN_ERROR(lv_ERROR_INTERNAL, "csg_export_node: child export failed");
            }

            n = snprintf(buf + written, (size_t) (buf_size - written), "%s}\n", indent_str);
            if (n > 0)
                written += n;
            else if (n < 0)
                lv_RETURN_ERROR(lv_ERROR_INTERNAL, "csg_export_node: snprintf failed for boolean close");
            break;
        }

        case CSG_NODE_TRANSFORM:
            n = snprintf(buf + written, (size_t) (buf_size - written), "%s// transform (TBI)\n", indent_str);
            if (n > 0)
                written += n;
            else if (n < 0)
                lv_RETURN_ERROR(lv_ERROR_INTERNAL, "csg_export_node: snprintf failed for transform");
            if (node->child_count > 0) {
                written = csg_export_node(node->children[0], buf, buf_size, written, indent);
                if (written < 0)
                    lv_RETURN_ERROR(lv_ERROR_INTERNAL, "csg_export_node: transform child export failed");
            }
            break;

        case CSG_NODE_HULL:
        case CSG_NODE_MINKOWSKI:
        case CSG_NODE_EXTRUDE_LINEAR:
        case CSG_NODE_EXTRUDE_ROTATE: {
            const char *op_name = "hull";
            if (node->kind == CSG_NODE_MINKOWSKI)
                op_name = "minkowski";
            if (node->kind == CSG_NODE_EXTRUDE_LINEAR)
                op_name = "linear_extrude";
            if (node->kind == CSG_NODE_EXTRUDE_ROTATE)
                op_name = "rotate_extrude";

            n = snprintf(buf + written, (size_t) (buf_size - written), "%s%s() {\n", indent_str, op_name);
            if (n > 0)
                written += n;
            else if (n < 0)
                lv_RETURN_ERROR(lv_ERROR_INTERNAL, "csg_export_node: snprintf failed for hull/minkowski/extrude");

            for (int i = 0; i < node->child_count; i++) {
                written = csg_export_node(node->children[i], buf, buf_size, written, indent + 1);
                if (written < 0)
                    lv_RETURN_ERROR(lv_ERROR_INTERNAL, "csg_export_node: child export failed");
            }

            n = snprintf(buf + written, (size_t) (buf_size - written), "%s}\n", indent_str);
            if (n > 0)
                written += n;
            else if (n < 0)
                lv_RETURN_ERROR(lv_ERROR_INTERNAL, "csg_export_node: snprintf failed for hull/minkowski/extrude close");
            break;
        }
    }

    return written;
}

/**
 * @brief 将 CSG 树导出为 OpenSCAD .scad 格式文本
 *
 * 递归遍历整棵 CSG 树，生成符合 OpenSCAD 语法的文本。
 * 调用者负责用 lv_free() 释放返回的字符串。
 *
 * @param root  CSG 树根节点
 * @return 以 '\0' 结尾的 OpenSCAD 脚本字符串，失败返回 NULL
 */
char *csg_export_to_openscad(const CSGNode *root) {
    if (!root)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "csg_export_to_openscad: root is NULL");

    int buf_size = CSG_EXPORT_BUF_INIT;
    char *buf = (char *) lv_calloc((size_t) buf_size, sizeof(char));
    if (!buf)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "csg_export_to_openscad: buffer allocation failed");

    /* 添加文件头 */
    int written = snprintf(buf, (size_t) buf_size,
                           "// Generated by Lv-00 CSG module\n"
                           "// Date: 2026-05-24\n"
                           "// Engine: geometry_csg.c (BSP-based)\n"
                           "$fn = 64;\n\n");
    if (written < 0) {
        lv_free((void **) &buf);
        return NULL;
    }

    written = csg_export_node(root, buf, buf_size, written, 0);
    if (written < 0) {
        lv_free((void **) &buf);
        return NULL;
    }

    return buf;
}

/* ================================================================
 * 内建示例：泰姬陵圆顶 CSG 组合
 * ================================================================ */

/**
 * @brief 构造泰姬陵圆顶的 CSG 描述
 *
 * 泰姬陵的中央圆顶由一个半球体和其下方的圆柱体基座组成。
 * 此函数构建如下 CSG 树：
 *
 *   union() {
 *       sphere(r=10);          // 半球体（用完整的球体近似）
 *       cylinder(r=10, h=4);   // 圆柱体基座
 *   }
 *
 * 圆顶的洋葱形状可通过后续 difference 操作削去多余部分来细化。
 *
 * @return 新 CSGNode 树根（调用者负责 csg_node_destroy）
 */
CSGNode *csg_example_taj_mahal_dome(void) {
    /* 创建半球体（近似为完整球体） */
    CSGNode *dome = csg_sphere_create(10.0);
    if (!dome)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "csg_example_taj_mahal_dome: dome allocation failed");

    /* 创建圆柱体基座 */
    CSGNode *base = csg_cylinder_create(10.0, 4.0);
    if (!base) {
        csg_node_destroy(dome);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "csg_example_taj_mahal_dome: base allocation failed");
    }

    /* 组合为并集 */
    CSGNode *taj_mahal = geometry_csg_union(dome, base);
    if (!taj_mahal) {
        csg_node_destroy(dome);
        csg_node_destroy(base);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "csg_example_taj_mahal_dome: union allocation failed");
    }

    return taj_mahal;
}

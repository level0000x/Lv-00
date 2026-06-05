#include "lv00/representation_converter.h"
#include "lv00/func_block.h"
#include "lv00/geometry_types.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* 几何编码内部结构：将 FuncBlock 编码为几何实体集合 */
typedef struct {
    /* 矩形区域列表（每个块对应一个矩形） */
    PolygonEntity **rects;
    int rect_count;

    /* 端口点列表（输入端口在左边缘，输出端口在右边缘） */
    PointEntity **port_points;
    int port_point_count;

    /* 连接线段列表（端口之间的连接） */
    LinearEntity **connections;
    int connection_count;
} GeometryEncoding;

/* 默认块布局参数 */
#define BLOCK_WIDTH  160.0
#define BLOCK_HEIGHT 80.0
#define BLOCK_GAP_X  40.0
#define BLOCK_GAP_Y  30.0
#define PORT_RADIUS  6.0

/* 将函数块转换为几何实体 */
/* 编码规则：
 *   Block → 矩形区域（PolygonEntity，4个顶点）
 *   输入端口 → 矩形左边缘上的点
 *   输出端口 → 矩形右边缘上的点
 *   连接 → 端口点之间的线段
 */
Lv00ConvertResult lv00_convert_block_to_geometry(void *block) {
    Lv00ConvertResult result = {0};
    if (!block) {
        result.success = 0;
        strncpy(result.error_msg, "NULL block", sizeof(result.error_msg));
        return result;
    }

    typedef struct {
        FuncBlock **blocks;
        int count;
    } BlockGraphView;

    BlockGraphView *bg = (BlockGraphView *)block;

    /* 创建几何编码结构 */
    GeometryEncoding *enc = calloc(1, sizeof(GeometryEncoding));
    if (!enc) {
        result.success = 0;
        strncpy(result.error_msg, "out of memory", sizeof(result.error_msg));
        return result;
    }

    /* 预分配空间 */
    int total_ports = 0;
    for (int i = 0; i < bg->count; i++) {
        if (bg->blocks[i]) {
            total_ports += func_block_get_input_count(bg->blocks[i]);
            total_ports += func_block_get_output_count(bg->blocks[i]);
        }
    }
    enc->rects = calloc(bg->count + 1, sizeof(PolygonEntity *));
    enc->port_points = calloc(total_ports + 1, sizeof(PointEntity *));
    enc->connections = calloc(total_ports + 1, sizeof(LinearEntity *));

    /* 为每个 FuncBlock 生成矩形和端口点 */
    for (int i = 0; i < bg->count; i++) {
        FuncBlock *fb = bg->blocks[i];
        if (!fb) continue;

        /* 计算块的位置（简单网格布局） */
        int col = i % 4;
        int row = i / 4;
        double x0 = col * (BLOCK_WIDTH + BLOCK_GAP_X);
        double y0 = row * (BLOCK_HEIGHT + BLOCK_GAP_Y);
        double x1 = x0 + BLOCK_WIDTH;
        double y1 = y0 + BLOCK_HEIGHT;

        /* 创建矩形区域的4个顶点 */
        SymbolicCoord *coords[8]; /* 4个点，每个点2个坐标 */
        coords[0] = symbolic_coord_from_double(x0);  /* 左下 x */
        coords[1] = symbolic_coord_from_double(y0);  /* 左下 y */
        coords[2] = symbolic_coord_from_double(x1);  /* 右下 x */
        coords[3] = symbolic_coord_from_double(y0);  /* 右下 y */
        coords[4] = symbolic_coord_from_double(x1);  /* 右上 x */
        coords[5] = symbolic_coord_from_double(y1);  /* 右上 y */
        coords[6] = symbolic_coord_from_double(x0);  /* 左上 x */
        coords[7] = symbolic_coord_from_double(y1);  /* 左上 y */

        PointEntity *corners[4];
        corners[0] = point_entity_create(coords[0], coords[1]); /* 左下 */
        corners[1] = point_entity_create(coords[2], coords[3]); /* 右下 */
        corners[2] = point_entity_create(coords[4], coords[5]); /* 右上 */
        corners[3] = point_entity_create(coords[6], coords[7]); /* 左上 */

        /* 创建矩形多边形 */
        PolygonEntity *rect = polygon_entity_create(corners, 4);
        if (rect) {
            const char *name = func_block_get_name(fb);
            if (name) {
                rect->base.name = lv00_strdup(name);
            }
            enc->rects[enc->rect_count++] = rect;
        }

        /* 生成输入端口点（左边缘均匀分布） */
        int in_count = func_block_get_input_count(fb);
        for (int j = 0; j < in_count; j++) {
            double py = y0 + (y1 - y0) * (j + 1) / (in_count + 1);
            SymbolicCoord *px_s = symbolic_coord_from_double(x0);
            SymbolicCoord *py_s = symbolic_coord_from_double(py);
            PointEntity *pt = point_entity_create(px_s, py_s);
            if (pt) {
                pt->base.name = lv00_strdup("input_port");
                enc->port_points[enc->port_point_count++] = pt;
            }
        }

        /* 生成输出端口点（右边缘均匀分布） */
        int out_count = func_block_get_output_count(fb);
        for (int j = 0; j < out_count; j++) {
            double py = y0 + (y1 - y0) * (j + 1) / (out_count + 1);
            SymbolicCoord *px_s = symbolic_coord_from_double(x1);
            SymbolicCoord *py_s = symbolic_coord_from_double(py);
            PointEntity *pt = point_entity_create(px_s, py_s);
            if (pt) {
                pt->base.name = lv00_strdup("output_port");
                enc->port_points[enc->port_point_count++] = pt;
            }
        }
    }

    /* 根据端口依赖生成连接线段 */
    int port_offset = 0;
    for (int i = 0; i < bg->count; i++) {
        FuncBlock *fb = bg->blocks[i];
        if (!fb) continue;

        int in_count = func_block_get_input_count(fb);
        int out_count = func_block_get_output_count(fb);

        /* 遍历端口依赖，创建连接线段 */
        if (fb->port_deps && fb->port_dep_count > 0) {
            for (int j = 0; j < fb->port_dep_count; j++) {
                PortDependency *dep = &fb->port_deps[j];

                /* 查找源块的输出端口点 */
                /* 通过块 ID 匹配查找源块 */
                int src_block_idx = -1;
                for (int k = 0; k < bg->count; k++) {
                    if (bg->blocks[k] && func_block_get_id(bg->blocks[k]) == dep->external_node_id) {
                        src_block_idx = k;
                        break;
                    }
                }

                if (src_block_idx >= 0) {
                    /* 创建连接线段（当前使用块中心点，完整版应使用端口坐标） */
                    int src_col = src_block_idx % 4;
                    int src_row = src_block_idx / 4;
                    int dst_col = i % 4;
                    int dst_row = i / 4;

                    double sx = src_col * (BLOCK_WIDTH + BLOCK_GAP_X) + BLOCK_WIDTH;
                    double sy = src_row * (BLOCK_HEIGHT + BLOCK_GAP_Y) + BLOCK_HEIGHT / 2.0;
                    double dx = dst_col * (BLOCK_WIDTH + BLOCK_GAP_X);
                    double dy = dst_row * (BLOCK_HEIGHT + BLOCK_GAP_Y) + BLOCK_HEIGHT / 2.0;

                    SymbolicCoord *sx_s = symbolic_coord_from_double(sx);
                    SymbolicCoord *sy_s = symbolic_coord_from_double(sy);
                    SymbolicCoord *dx_s = symbolic_coord_from_double(dx);
                    SymbolicCoord *dy_s = symbolic_coord_from_double(dy);

                    PointEntity *p1 = point_entity_create(sx_s, sy_s);
                    PointEntity *p2 = point_entity_create(dx_s, dy_s);
                    LinearEntity *seg = linear_entity_create_segment(p1, p2);
                    if (seg) {
                        enc->connections[enc->connection_count++] = seg;
                    }
                }
            }
        }

        port_offset += in_count + out_count;
    }

    result.output = enc;
    result.success = 1;
    return result;
}

/* 将几何实体反向解码为函数块结构 */
/* 解码规则：
 *   矩形 → FuncBlock
 *   左边缘上的点 → 输入端口
 *   右边缘上的点 → 输出端口
 *   线段 → 端口连接
 */
Lv00ConvertResult lv00_convert_geometry_to_block(void *entity) {
    Lv00ConvertResult result = {0};
    if (!entity) {
        result.success = 0;
        strncpy(result.error_msg, "NULL entity", sizeof(result.error_msg));
        return result;
    }

    GeometryEncoding *enc = (GeometryEncoding *)entity;

    typedef struct {
        FuncBlock **blocks;
        int count;
        int cap;
    } SimpleBlockGraph;

    SimpleBlockGraph *sg = calloc(1, sizeof(SimpleBlockGraph));
    if (!sg) {
        result.success = 0;
        strncpy(result.error_msg, "out of memory", sizeof(result.error_msg));
        return result;
    }
    sg->cap = enc->rect_count > 0 ? enc->rect_count : 8;
    sg->blocks = calloc(sg->cap, sizeof(FuncBlock *));

    /* 遍历矩形区域，每个矩形还原为一个 FuncBlock */
    for (int i = 0; i < enc->rect_count; i++) {
        PolygonEntity *rect = enc->rects[i];
        if (!rect) continue;

        FuncBlock *fb = func_block_create(i);
        if (!fb) continue;

        /* 使用矩形名称作为块名称 */
        if (rect->base.name) {
            func_block_set_name(fb, rect->base.name);
        }

        /* 统计该矩形左右边缘上的端口点 */
        /* 基于矩形包围盒判定端口归属（左边缘为输入，右边缘为输出） */
        double rx_min = rect->base.bounding_box.x_min;
        double rx_max = rect->base.bounding_box.x_max;

        int inputs[64], outputs[64];
        int in_cnt = 0, out_cnt = 0;

        for (int j = 0; j < enc->port_point_count && (in_cnt < 64 || out_cnt < 64); j++) {
            PointEntity *pt = enc->port_points[j];
            if (!pt) continue;

            double px = pt->base.bounding_box.x_min; /* 点的x坐标 */

            /* 判断点是否在该矩形的左/右边缘附近 */
            if (fabs(px - rx_min) < 1.0 && in_cnt < 64) {
                inputs[in_cnt++] = j;
            } else if (fabs(px - rx_max) < 1.0 && out_cnt < 64) {
                outputs[out_cnt++] = j;
            }
        }

        if (in_cnt > 0) func_block_set_input_ports(fb, inputs, in_cnt);
        if (out_cnt > 0) func_block_set_output_ports(fb, outputs, out_cnt);

        if (sg->count >= sg->cap) {
            int new_cap = sg->cap * 2;
            FuncBlock **tmp = realloc(sg->blocks, new_cap * sizeof(FuncBlock *));
            if (!tmp) {
                result.success = 0;
                strncpy(result.error_msg, "out of memory", sizeof(result.error_msg));
                /* 原始指针 sg->blocks 保持有效，可由调用者释放 */
                return result;
            }
            sg->blocks = tmp;
            sg->cap = new_cap;
        }
        sg->blocks[sg->count++] = fb;
    }

    result.output = sg;
    result.success = 1;
    return result;
}

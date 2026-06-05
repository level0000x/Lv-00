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

    /* 端口点附加信息：记录每个端口点的端口ID和所属块索引 */
    int *port_point_ids;      /* 端口ID数组，与 port_points 一一对应 */
    int *port_point_blocks;   /* 所属块索引数组，与 port_points 一一对应 */

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
    enc->port_point_ids = calloc(total_ports + 1, sizeof(int));
    enc->port_point_blocks = calloc(total_ports + 1, sizeof(int));
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
                rect->base.name = strdup(name);
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
                pt->base.name = strdup("input_port");
                int port_id = fb->input_port_ids ? fb->input_port_ids[j] : j;
                enc->port_point_ids[enc->port_point_count] = port_id;
                enc->port_point_blocks[enc->port_point_count] = i;
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
                pt->base.name = strdup("output_port");
                int port_id = fb->output_port_ids ? fb->output_port_ids[j] : j;
                enc->port_point_ids[enc->port_point_count] = port_id;
                enc->port_point_blocks[enc->port_point_count] = i;
                enc->port_points[enc->port_point_count++] = pt;
            }
        }
    }

    /* 根据端口依赖生成连接线段（使用端口ID匹配查找端口点） */
    for (int i = 0; i < bg->count; i++) {
        FuncBlock *fb = bg->blocks[i];
        if (!fb) continue;

        if (fb->port_deps && fb->port_dep_count > 0) {
            for (int j = 0; j < fb->port_dep_count; j++) {
                PortDependency *dep = &fb->port_deps[j];

                /* 通过端口ID匹配查找源块的输出端口点和目标块的输入端口点 */
                PointEntity *src_pt = NULL;
                PointEntity *dst_pt = NULL;

                for (int k = 0; k < enc->port_point_count; k++) {
                    if (enc->port_point_blocks[k] == i &&
                        enc->port_point_ids[k] == dep->port_id) {
                        dst_pt = enc->port_points[k];
                    }
                    /* 查找源块中匹配 external_node_id 的输出端口 */
                    if (enc->port_point_blocks[k] >= 0) {
                        FuncBlock *owner = bg->blocks[enc->port_point_blocks[k]];
                        if (owner && func_block_get_id(owner) == dep->external_node_id) {
                            /* 检查是否为输出端口 */
                            int out_count = func_block_get_output_count(owner);
                            for (int oi = 0; oi < out_count; oi++) {
                                int out_id = owner->output_port_ids ? owner->output_port_ids[oi] : oi;
                                if (out_id == dep->port_id) {
                                    src_pt = enc->port_points[k];
                                    break;
                                }
                            }
                        }
                    }
                    if (src_pt && dst_pt) break;
                }

                /* 如果未找到精确匹配的端口点，回退到块边缘中心点 */
                if (!src_pt || !dst_pt) {
                    int src_block_idx = -1;
                    for (int k = 0; k < bg->count; k++) {
                        if (bg->blocks[k] && func_block_get_id(bg->blocks[k]) == dep->external_node_id) {
                            src_block_idx = k;
                            break;
                        }
                    }
                    if (src_block_idx >= 0) {
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
                        if (!src_pt) src_pt = point_entity_create(sx_s, sy_s);
                        if (!dst_pt) dst_pt = point_entity_create(dx_s, dy_s);
                    }
                }

                if (src_pt && dst_pt) {
                    LinearEntity *seg = linear_entity_create_segment(src_pt, dst_pt);
                    if (seg) {
                        enc->connections[enc->connection_count++] = seg;
                    }
                }
            }
        }
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

/* 辅助：计算点 (px,py) 到线段 (ax,ay)-(bx,by) 的最短距离 */
static double point_to_segment_dist(double px, double py,
                                     double ax, double ay, double bx, double by) {
    double dx = bx - ax, dy = by - ay;
    double len_sq = dx * dx + dy * dy;
    if (len_sq < 1e-12) return sqrt((px - ax) * (px - ax) + (py - ay) * (py - ay));
    double t = ((px - ax) * dx + (py - ay) * dy) / len_sq;
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    double proj_x = ax + t * dx, proj_y = ay + t * dy;
    double ddx = px - proj_x, ddy = py - proj_y;
    return sqrt(ddx * ddx + ddy * ddy);
}

/* 辅助：从多边形实体提取顶点坐标（最多 max_verts 个） */
static int polygon_get_vertices(PolygonEntity *poly, double *xs, double *ys, int max_verts) {
    if (!poly || !poly->vertices || max_verts <= 0) return 0;
    int count = 0;
    for (int v = 0; v < max_verts; v++) {
        if (!poly->vertices[v]) break;
        xs[v] = symbolic_coord_to_double(poly->vertices[v]->x);
        ys[v] = symbolic_coord_to_double(poly->vertices[v]->y);
        count++;
    }
    return count;
}

/* 辅助：判断点是否在多边形内（射线法） */
static int point_in_polygon(double px, double py, double *xs, double *ys, int n) {
    int inside = 0;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        if (((ys[i] > py) != (ys[j] > py)) &&
            (px < (xs[j] - xs[i]) * (py - ys[i]) / (ys[j] - ys[i]) + xs[i])) {
            inside = !inside;
        }
    }
    return inside;
}

/* 辅助：判断点是否在多边形某条边的附近（距离 < threshold），
 * 返回最近边的索引，-1 表示不在任何边附近 */
static int point_near_polygon_edge(double px, double py, double *xs, double *ys, int n,
                                   double threshold) {
    int nearest_edge = -1;
    double min_dist = threshold;
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        double d = point_to_segment_dist(px, py, xs[i], ys[i], xs[j], ys[j]);
        if (d < min_dist) {
            min_dist = d;
            nearest_edge = i;
        }
    }
    return nearest_edge;
}

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

        /* 使用几何包含测试判定端口点归属 */
        /* 提取矩形的顶点坐标 */
        double vert_xs[64], vert_ys[64];
        int vert_count = polygon_get_vertices(rect, vert_xs, vert_ys, 64);

        int inputs[64], outputs[64];
        int in_cnt = 0, out_cnt = 0;

        for (int j = 0; j < enc->port_point_count && (in_cnt < 64 || out_cnt < 64); j++) {
            PointEntity *pt = enc->port_points[j];
            if (!pt) continue;

            /* 提取端口点的坐标 */
            double px = symbolic_coord_to_double(pt->x);
            double py = symbolic_coord_to_double(pt->y);

            /* 使用射线法判断点是否在多边形内 */
            if (vert_count < 3) continue;
            if (!point_in_polygon(px, py, vert_xs, vert_ys, vert_count)) continue;

            /* 点在矩形内，检查它靠近哪条边 */
            /* 对于标准矩形编码（4个顶点），边的索引对应：
             *   edge 0: vertex 0 -> vertex 1 (底边)
             *   edge 1: vertex 1 -> vertex 2 (右边)
             *   edge 2: vertex 2 -> vertex 3 (顶边)
             *   edge 3: vertex 3 -> vertex 0 (左边)
             */
            int nearest_edge = point_near_polygon_edge(px, py, vert_xs, vert_ys,
                                                        vert_count, PORT_RADIUS * 2.0);
            if (nearest_edge < 0) continue;

            /* 判断最近边是左边还是右边：
             * 计算边的方向向量，如果主要沿 Y 轴（垂直边），
             * 则检查其 X 坐标是较小（左边/输入）还是较大（右边/输出） */
            int e0 = nearest_edge;
            int e1 = (nearest_edge + 1) % vert_count;
            double edge_dx = fabs(vert_xs[e1] - vert_xs[e0]);
            double edge_dy = fabs(vert_ys[e1] - vert_ys[e0]);

            if (edge_dy > edge_dx) {
                /* 垂直边 */
                double edge_x = (vert_xs[e0] + vert_xs[e1]) / 2.0;
                double rect_center_x = 0.0;
                for (int v = 0; v < vert_count; v++) rect_center_x += vert_xs[v];
                rect_center_x /= vert_count;

                if (edge_x < rect_center_x && in_cnt < 64) {
                    inputs[in_cnt++] = j;
                } else if (edge_x >= rect_center_x && out_cnt < 64) {
                    outputs[out_cnt++] = j;
                }
            }
        }

        if (in_cnt > 0) func_block_set_input_ports(fb, inputs, in_cnt);
        if (out_cnt > 0) func_block_set_output_ports(fb, outputs, out_cnt);

        if (sg->count >= sg->cap) {
            sg->cap *= 2;
            sg->blocks = realloc(sg->blocks, sg->cap * sizeof(FuncBlock *));
        }
        sg->blocks[sg->count++] = fb;
    }

    result.output = sg;
    result.success = 1;
    return result;
}

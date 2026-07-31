/**
 * @file block_canvas.c
 * @brief 块画布视图实现
 *
 * @details 实现块画布视图，管理视觉块（含输入/输出端口）和块间连接。
 *          支持块的添加/删除、端口管理、块间连接以及 SVG 渲染输出。
 *          连接使用贝塞尔曲线绘制，块根据类型使用不同颜色标识。
 *
 * @author Lv-00 Project
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_check.h"
#include "lv/visual_editor.h"
#include "lv/lv_internal.h"

/* 块画布视图 - 完整实现 */

/** @brief 块类型枚举 */
typedef enum {
    lv_BLOCK_TYPE_INPUT,     /**< 输入块 */
    lv_BLOCK_TYPE_OUTPUT,    /**< 输出块 */
    lv_BLOCK_TYPE_PROCESS,   /**< 处理块 */
    lv_BLOCK_TYPE_FUNCTION,  /**< 函数块 */
    lv_BLOCK_TYPE_CONDITION, /**< 条件块 */
    lv_BLOCK_TYPE_LOOP       /**< 循环块 */
} lvBlockType;

/** @brief 端口结构 */
typedef struct lvBlockPort {
    int id;              /**< 端口唯一标识 */
    char name[64];       /**< 端口名称 */
    int is_input;        /**< 是否为输入端口（1=输入, 0=输出） */
    int index;           /**< 端口在块边缘的序号 */
    double rel_x, rel_y; /**< 相对于块的端口位置 */
} lvBlockPort;

/** @brief 视觉块结构 */
typedef struct lvVisualBlock {
    int id;               /**< 块唯一标识 */
    char label[128];      /**< 块标签 */
    double x, y;          /**< 块左上角位置 */
    double width, height; /**< 块宽高 */
    lvBlockType type;     /**< 块类型 */

    /** 端口数组 */
    lvBlockPort *ports;
    int port_count;        /**< 端口总数 */
    int input_port_count;  /**< 输入端口数 */
    int output_port_count; /**< 输出端口数 */
} lvVisualBlock;

/** @brief 块连接结构 */
typedef struct lvBlockConnection {
    int id;            /**< 连接唯一标识 */
    int from_block_id; /**< 源块ID */
    int from_port_id;  /**< 源端口ID */
    int to_block_id;   /**< 目标块ID */
    int to_port_id;    /**< 目标端口ID */
} lvBlockConnection;

/** @brief 块类型颜色映射表 */
static const char *block_type_colors[] = {
    "#4CAF50", /* INPUT - 绿色 */
    "#2196F3", /* OUTPUT - 蓝色 */
    "#FF9800", /* PROCESS - 橙色 */
    "#9C27B0", /* FUNCTION - 紫色 */
    "#F44336", /* CONDITION - 红色 */
    "#00BCD4"  /* LOOP - 青色 */
};

/** @brief 块画布内部结构 */
typedef struct lvBlockCanvasView {
    int view_type;                  /**< 视图类型标识 */
    void *library;                  /**< 库引用（保留扩展） */
    lvDArray blocks;                /**< lvVisualBlock 动态数组 */
    lvDArray connections;           /**< lvBlockConnection 动态数组 */
    int next_block_id;              /**< 下一个块ID */
    int next_port_id;               /**< 下一个端口ID */
    int next_connection_id;         /**< 下一个连接ID */
} lvBlockCanvasView;

/**
 * @brief 创建块画布视图
 *
 * 分配并初始化块画布，预分配块和连接数组的初始容量。
 *
 * @return 成功返回块画布指针，失败返回NULL
 */
lvBlockCanvasView *lv_block_canvas_create(void) {
    lvBlockCanvasView *canvas = lv_calloc(1, sizeof(lvBlockCanvasView));
    if (!canvas)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "failed to allocate block canvas");
    canvas->view_type = lv_VIEW_BLOCK_CANVAS;
    lv_darray_init(&canvas->blocks, sizeof(lvVisualBlock));
    lv_darray_init(&canvas->connections, sizeof(lvBlockConnection));
    if (!lv_darray_reserve(&canvas->blocks, 16) || !lv_darray_reserve(&canvas->connections, 16)) {
        lv_darray_free(&canvas->blocks);
        lv_darray_free(&canvas->connections);
        lv_free((void **) &canvas);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "failed to allocate block canvas arrays");
    }
    canvas->next_block_id = 1;
    canvas->next_port_id = 1;
    canvas->next_connection_id = 1;
    return canvas;
}

/**
 * @brief 销毁块画布视图
 *
 * 释放所有块的端口数组、块数组、连接数组和画布结构体。
 *
 * @param canvas 块画布指针
 */
void lv_block_canvas_destroy(lvBlockCanvasView *canvas) {
    if (!canvas)
        return;
    for (int i = 0; i < canvas->blocks.count; i++) {
        lvVisualBlock *b = (lvVisualBlock *) lv_darray_get(&canvas->blocks, i);
        lv_free((void **) &b->ports);
    }
    lv_darray_free(&canvas->blocks);
    lv_darray_free(&canvas->connections);
    lv_free((void **) &canvas);
}

/**
 * @brief 添加视觉块
 *
 * 向画布中添加一个视觉块，自动创建指定数量的输入和输出端口。
 * 如果块数组已满，自动扩容为当前容量的2倍。
 *
 * @param canvas       块画布指针
 * @param label        块标签
 * @param x            块X坐标
 * @param y            块Y坐标
 * @param width        块宽度（<=0时使用默认值120）
 * @param height       块高度（<=0时使用默认值60）
 * @param type         块类型
 * @param input_count  输入端口数
 * @param output_count 输出端口数
 * @return 成功返回块ID，失败返回-1
 */
int lv_block_canvas_add_block(lvBlockCanvasView *canvas, const char *label, double x, double y, double width,
                              double height, int type, int input_count, int output_count) {
    lv_CHECK_NOT_NULL(canvas);
    lv_CHECK_NOT_NULL(label);

    lvVisualBlock block;
    memset(&block, 0, sizeof(block));
    block.id = canvas->next_block_id++;
    strncpy(block.label, label, sizeof(block.label) - 1);
    block.label[sizeof(block.label) - 1] = '\0';
    block.x = x;
    block.y = y;
    block.width = (width > 0) ? width : 120.0;
    block.height = (height > 0) ? height : 60.0;
    block.type = (lvBlockType) type;

    /* 创建端口 */
    int total_ports = input_count + output_count;
    block.input_port_count = input_count;
    block.output_port_count = output_count;
    block.port_count = total_ports;
    if (total_ports > 0) {
        block.ports = lv_calloc(total_ports, sizeof(lvBlockPort));
        if (!block.ports)
            lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "failed to allocate block ports");

        /* 输入端口在左侧均匀分布 */
        for (int i = 0; i < input_count; i++) {
            lvBlockPort *p = &block.ports[i];
            p->id = canvas->next_port_id++;
            p->is_input = 1;
            p->index = i;
            snprintf(p->name, sizeof(p->name), "in_%d", i);
            p->rel_x = 0.0;
            p->rel_y = block.height * (i + 1) / (input_count + 1);
        }
        /* 输出端口在右侧均匀分布 */
        for (int i = 0; i < output_count; i++) {
            lvBlockPort *p = &block.ports[input_count + i];
            p->id = canvas->next_port_id++;
            p->is_input = 0;
            p->index = i;
            snprintf(p->name, sizeof(p->name), "out_%d", i);
            p->rel_x = block.width;
            p->rel_y = block.height * (i + 1) / (output_count + 1);
        }
    }

    int idx = lv_darray_push(&canvas->blocks, &block);
    if (idx < 0) {
        lv_free((void **) &block.ports);
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "failed to push block");
    }
    return block.id;
}

/**
 * @brief 移除块
 *
 * 删除指定ID的块，释放端口数组，同时移除所有关联的连接。
 * 用最后一个元素填充空位。
 *
 * @param canvas  块画布指针
 * @param block_id 要移除的块ID
 * @return 成功返回0，失败返回-1
 */
int lv_block_canvas_remove_block(lvBlockCanvasView *canvas, int block_id) {
    lv_CHECK_NOT_NULL(canvas);
    lv_CHECK_ARG(block_id > 0, lv_ERROR_INVALID_PARAM, "invalid block_id %d", block_id);
    int found = -1;
    for (int i = 0; i < canvas->blocks.count; i++) {
        lvVisualBlock *b = (lvVisualBlock *) lv_darray_get(&canvas->blocks, i);
        if (b->id == block_id) {
            found = i;
            break;
        }
    }
    if (found < 0)
        lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "block not found");

    /* 释放端口 */
    {
        lvVisualBlock *b = (lvVisualBlock *) lv_darray_get(&canvas->blocks, found);
        lv_free((void **) &b->ports);
    }

    /* 移除相关连接（原地压缩） */
    int new_c = 0;
    for (int i = 0; i < canvas->connections.count; i++) {
        lvBlockConnection *conn = (lvBlockConnection *) lv_darray_get(&canvas->connections, i);
        if (conn->from_block_id != block_id && conn->to_block_id != block_id) {
            if (new_c != i) {
                lvBlockConnection *dest = (lvBlockConnection *) lv_darray_get(&canvas->connections, new_c);
                *dest = *conn;
            }
            new_c++;
        }
    }
    canvas->connections.count = new_c;

    /* 用最后一个元素填充空位 */
    {
        lvVisualBlock *arr = (lvVisualBlock *) canvas->blocks.data;
        arr[found] = arr[canvas->blocks.count - 1];
    }
    lv_darray_pop(&canvas->blocks);
    return 0;
}

/**
 * @brief 查找块（内部）
 *
 * 根据块ID在画布中查找视觉块。
 *
 * @param canvas   块画布指针
 * @param block_id 块ID
 * @return 成功返回块指针，失败返回NULL
 */
static lvVisualBlock *find_block(lvBlockCanvasView *canvas, int block_id) {
    if (!canvas || block_id <= 0)
        return NULL;
    for (int i = 0; i < canvas->blocks.count; i++) {
        lvVisualBlock *b = (lvVisualBlock *) lv_darray_get(&canvas->blocks, i);
        if (b->id == block_id)
            return b;
    }
    return NULL;
}

/**
 * @brief 查找端口绝对坐标（内部）
 *
 * 根据块ID和端口ID计算端口的绝对画布坐标。
 *
 * @param canvas   块画布指针
 * @param block_id 块ID
 * @param port_id  端口ID
 * @param px       输出X坐标
 * @param py       输出Y坐标
 * @return 成功返回0，失败返回-1
 */
static int find_port_pos(lvBlockCanvasView *canvas, int block_id, int port_id, double *px, double *py) {
    lvVisualBlock *block = find_block(canvas, block_id);
    if (!block)
        return -1;
    for (int i = 0; i < block->port_count; i++) {
        if (block->ports[i].id == port_id) {
            *px = block->x + block->ports[i].rel_x;
            *py = block->y + block->ports[i].rel_y;
            return 0;
        }
    }
    return -1;
}

/**
 * @brief 连接两个块的端口
 *
 * 在两个块的指定端口之间建立逻辑连接。自动验证端口存在性，
 * 防止自连接。如果连接数组已满，自动扩容。
 *
 * @param canvas        块画布指针
 * @param from_block_id 源块ID
 * @param from_port_id  源端口ID
 * @param to_block_id   目标块ID
 * @param to_port_id    目标端口ID
 * @return 成功返回连接ID，失败返回-1
 */
int lv_block_canvas_connect_blocks(lvBlockCanvasView *canvas, int from_block_id, int from_port_id, int to_block_id,
                                   int to_port_id) {
    lv_CHECK_NOT_NULL(canvas);
    lv_CHECK_ARG(from_block_id > 0 && to_block_id > 0, lv_ERROR_INVALID_PARAM,
                 "invalid block id (from=%d, to=%d)", from_block_id, to_block_id);
    if (from_block_id == to_block_id)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "self-connection not allowed");

    /* 验证端口存在 */
    lvVisualBlock *from_block = find_block(canvas, from_block_id);
    lvVisualBlock *to_block = find_block(canvas, to_block_id);
    if (!from_block || !to_block)
        lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "source or target block not found");

    int from_found = 0, to_found = 0;
    for (int i = 0; i < from_block->port_count; i++) {
        if (from_block->ports[i].id == from_port_id) {
            from_found = 1;
            break;
        }
    }
    for (int i = 0; i < to_block->port_count; i++) {
        if (to_block->ports[i].id == to_port_id) {
            to_found = 1;
            break;
        }
    }
    if (!from_found || !to_found)
        lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "source or target port not found");

    lvBlockConnection conn;
    conn.id = canvas->next_connection_id++;
    conn.from_block_id = from_block_id;
    conn.from_port_id = from_port_id;
    conn.to_block_id = to_block_id;
    conn.to_port_id = to_port_id;

    int idx = lv_darray_push(&canvas->connections, &conn);
    if (idx < 0)
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "failed to push connection");
    return conn.id;
}

/**
 * @brief 安全写入 SVG 内容的辅助宏
 *
 * 检查缓冲区剩余空间后调用 snprintf，防止缓冲区溢出。
 * 自动将 pos 限制在 [0, buf_size-1] 范围内。
 */
#define SVG_SAFE_SNPRINTF(_buf, _pos, _size, ...)                                         \
    do {                                                                                  \
        if ((_pos) >= 0 && (_pos) < (_size)) {                                            \
            int _w = snprintf((_buf) + (_pos), (size_t) ((_size) - (_pos)), __VA_ARGS__); \
            if (_w > 0) {                                                                 \
                (_pos) += _w;                                                             \
                if ((_pos) >= (_size))                                                    \
                    (_pos) = (_size) - 1;                                                 \
            }                                                                             \
        }                                                                                 \
    } while (0)

/**
 * @brief 生成 SVG 输出
 *
 * 将块画布中的块和连接渲染为 SVG 格式的 XML 字符串。
 * 连接使用贝塞尔曲线绘制，块根据类型使用不同颜色。
 * 调用者负责释放返回的字符串。
 *
 * @param canvas 块画布指针
 * @return 成功返回分配的SVG字符串，失败返回NULL
 */
char *lv_block_canvas_render_svg(lvBlockCanvasView *canvas) {
    if (!canvas)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "NULL canvas");

    /* 计算包围盒 */
    double min_x = 1e18, min_y = 1e18, max_x = -1e18, max_y = -1e18;
    for (int i = 0; i < canvas->blocks.count; i++) {
        lvVisualBlock *b = (lvVisualBlock *) lv_darray_get(&canvas->blocks, i);
        if (b->x < min_x)
            min_x = b->x;
        if (b->y < min_y)
            min_y = b->y;
        if (b->x + b->width > max_x)
            max_x = b->x + b->width;
        if (b->y + b->height > max_y)
            max_y = b->y + b->height;
    }
    double margin = 40.0;
    min_x -= margin;
    min_y -= margin;
    max_x += margin;
    max_y += margin;
    if (max_x - min_x < 100) {
        min_x -= 50;
        max_x += 50;
    }
    if (max_y - min_y < 100) {
        min_y -= 50;
        max_y += 50;
    }

    /* [安全] 估算缓冲区大小，防止整数溢出 */
    size_t est_size = (size_t) canvas->blocks.count * 1024 + (size_t) canvas->connections.count * 512 + 4096;
    if (est_size > 1024 * 1024 * 16)
        est_size = 1024 * 1024 * 16;
    int buf_size = (int) est_size;
    char *buf = lv_calloc(buf_size, sizeof(char));
    if (!buf)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "failed to allocate SVG buffer");

    int pos = 0;

    SVG_SAFE_SNPRINTF(buf, pos, buf_size,
                      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                      "<svg xmlns=\"http://www.w3.org/2000/svg\" "
                      "viewBox=\"%g %g %g %g\" width=\"800\" height=\"600\">\n",
                      min_x, min_y, max_x - min_x, max_y - min_y);

    /* 绘制连接（贝塞尔曲线） */
    for (int i = 0; i < canvas->connections.count; i++) {
        lvBlockConnection *c = (lvBlockConnection *) lv_darray_get(&canvas->connections, i);
        double x1, y1, x2, y2;
        if (find_port_pos(canvas, c->from_block_id, c->from_port_id, &x1, &y1) != 0)
            continue;
        if (find_port_pos(canvas, c->to_block_id, c->to_port_id, &x2, &y2) != 0)
            continue;

        /* 控制点：水平方向偏移 */
        double dx = fabs(x2 - x1) * 0.5;
        if (dx < 30.0)
            dx = 30.0;
        double cx1 = x1 + dx;
        double cy1 = y1;
        double cx2 = x2 - dx;
        double cy2 = y2;

        SVG_SAFE_SNPRINTF(buf, pos, buf_size,
                          "  <path d=\"M %g,%g C %g,%g %g,%g %g,%g\" "
                          "fill=\"none\" stroke=\"#666666\" stroke-width=\"2\"/>\n",
                          x1, y1, cx1, cy1, cx2, cy2, x2, y2);
    }

    /* 绘制块 */
    for (int i = 0; i < canvas->blocks.count; i++) {
        lvVisualBlock *b = (lvVisualBlock *) lv_darray_get(&canvas->blocks, i);
        const char *color = (b->type >= 0 && b->type <= lv_BLOCK_TYPE_LOOP) ? block_type_colors[b->type] : "#999999";

        /* 圆角矩形 */
        double rx = 8.0;
        SVG_SAFE_SNPRINTF(buf, pos, buf_size,
                          "  <rect x=\"%g\" y=\"%g\" width=\"%g\" height=\"%g\" "
                          "rx=\"%g\" ry=\"%g\" fill=\"%s\" stroke=\"#333333\" "
                          "stroke-width=\"2\" opacity=\"0.9\"/>\n",
                          b->x, b->y, b->width, b->height, rx, rx, color);

        /* 标签 */
        SVG_SAFE_SNPRINTF(buf, pos, buf_size,
                          "  <text x=\"%g\" y=\"%g\" font-size=\"13\" fill=\"white\" "
                          "text-anchor=\"middle\" dominant-baseline=\"middle\" "
                          "font-weight=\"bold\">%s</text>\n",
                          b->x + b->width / 2.0, b->y + b->height / 2.0, b->label);

        /* 绘制端口（小圆圈） */
        for (int j = 0; j < b->port_count; j++) {
            lvBlockPort *p = &b->ports[j];
            double px = b->x + p->rel_x;
            double py = b->y + p->rel_y;
            const char *port_color = p->is_input ? "#E8F4FD" : "#FFF3E0";
            const char *port_stroke = p->is_input ? "#2196F3" : "#FF9800";
            SVG_SAFE_SNPRINTF(buf, pos, buf_size,
                              "  <circle cx=\"%g\" cy=\"%g\" r=\"5\" "
                              "fill=\"%s\" stroke=\"%s\" stroke-width=\"1.5\"/>\n",
                              px, py, port_color, port_stroke);
        }
    }

    SVG_SAFE_SNPRINTF(buf, pos, buf_size, "</svg>\n");
    return buf;
}

#undef SVG_SAFE_SNPRINTF

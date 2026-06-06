#include "lv00/visual_editor.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* 块画布视图 - 完整实现 */

/* 块类型 */
typedef enum {
    LV00_BLOCK_TYPE_INPUT,
    LV00_BLOCK_TYPE_OUTPUT,
    LV00_BLOCK_TYPE_PROCESS,
    LV00_BLOCK_TYPE_FUNCTION,
    LV00_BLOCK_TYPE_CONDITION,
    LV00_BLOCK_TYPE_LOOP
} Lv00BlockType;

/* 端口 */
typedef struct Lv00BlockPort {
    int id;
    char name[64];
    int is_input;     /* 1=输入端口, 0=输出端口 */
    int index;        /* 端口在块边缘的序号 */
    double rel_x, rel_y; /* 相对于块的端口位置 */
} Lv00BlockPort;

/* 视觉块 */
typedef struct Lv00VisualBlock {
    int id;
    char label[128];
    double x, y;      /* 块左上角位置 */
    double width, height;
    Lv00BlockType type;

    /* 端口 */
    Lv00BlockPort *ports;
    int port_count;
    int input_port_count;
    int output_port_count;
} Lv00VisualBlock;

/* 块连接 */
typedef struct Lv00BlockConnection {
    int id;
    int from_block_id;
    int from_port_id;
    int to_block_id;
    int to_port_id;
} Lv00BlockConnection;

/* 块类型颜色映射 */
static const char *block_type_colors[] = {
    "#4CAF50",  /* INPUT - 绿色 */
    "#2196F3",  /* OUTPUT - 蓝色 */
    "#FF9800",  /* PROCESS - 橙色 */
    "#9C27B0",  /* FUNCTION - 紫色 */
    "#F44336",  /* CONDITION - 红色 */
    "#00BCD4"   /* LOOP - 青色 */
};

typedef struct Lv00BlockCanvasView {
    int view_type;
    void *library;
    Lv00VisualBlock *blocks;
    int block_count;
    int block_capacity;
    Lv00BlockConnection *connections;
    int connection_count;
    int connection_capacity;
    int next_block_id;
    int next_port_id;
    int next_connection_id;
} Lv00BlockCanvasView;

Lv00BlockCanvasView *lv00_block_canvas_create(void) {
    Lv00BlockCanvasView *canvas = lv00_calloc(1, sizeof(Lv00BlockCanvasView));
    if (!canvas) return NULL;
    canvas->view_type = LV00_VIEW_BLOCK_CANVAS;
    canvas->block_capacity = 16;
    canvas->blocks = lv00_calloc(canvas->block_capacity, sizeof(Lv00VisualBlock));
    if (!canvas->blocks) { lv00_free((void **)&canvas); return NULL; }
    canvas->connection_capacity = 16;
    canvas->connections = lv00_calloc(canvas->connection_capacity, sizeof(Lv00BlockConnection));
    if (!canvas->connections) { lv00_free((void **)&canvas->blocks); lv00_free((void **)&canvas); return NULL; }
    canvas->next_block_id = 1;
    canvas->next_port_id = 1;
    canvas->next_connection_id = 1;
    return canvas;
}

void lv00_block_canvas_destroy(Lv00BlockCanvasView *canvas) {
    if (!canvas) return;
    for (int i = 0; i < canvas->block_count; i++) {
        lv00_free((void **)&canvas->blocks[i].ports);
    }
    lv00_free((void **)&canvas->blocks);
    lv00_free((void **)&canvas->connections);
    lv00_free((void **)&canvas);
}

/* 添加视觉块 */
int lv00_block_canvas_add_block(Lv00BlockCanvasView *canvas, const char *label,
                                 double x, double y, double width, double height,
                                 int type, int input_count, int output_count) {
    if (!canvas || !label) return -1;

    /* 自动扩容 */
    if (canvas->block_count >= canvas->block_capacity) {
        int new_cap = canvas->block_capacity * 2;
        Lv00VisualBlock *new_arr = lv00_realloc(canvas->blocks, new_cap * sizeof(Lv00VisualBlock));
        if (!new_arr) return -1;
        canvas->blocks = new_arr;
        canvas->block_capacity = new_cap;
    }

    Lv00VisualBlock *block = &canvas->blocks[canvas->block_count];
    block->id = canvas->next_block_id++;
    strncpy(block->label, label, sizeof(block->label) - 1);
    block->label[sizeof(block->label) - 1] = '\0';
    block->x = x;
    block->y = y;
    block->width = (width > 0) ? width : 120.0;
    block->height = (height > 0) ? height : 60.0;
    block->type = (Lv00BlockType)type;

    /* 创建端口 */
    int total_ports = input_count + output_count;
    block->input_port_count = input_count;
    block->output_port_count = output_count;
    block->port_count = total_ports;
    if (total_ports > 0) {
        block->ports = calloc(total_ports, sizeof(Lv00BlockPort));
        if (!block->ports) return -1;

        /* 输入端口在左侧均匀分布 */
        for (int i = 0; i < input_count; i++) {
            Lv00BlockPort *p = &block->ports[i];
            p->id = canvas->next_port_id++;
            p->is_input = 1;
            p->index = i;
            snprintf(p->name, sizeof(p->name), "in_%d", i);
            p->rel_x = 0.0;
            p->rel_y = block->height * (i + 1) / (input_count + 1);
        }
        /* 输出端口在右侧均匀分布 */
        for (int i = 0; i < output_count; i++) {
            Lv00BlockPort *p = &block->ports[input_count + i];
            p->id = canvas->next_port_id++;
            p->is_input = 0;
            p->index = i;
            snprintf(p->name, sizeof(p->name), "out_%d", i);
            p->rel_x = block->width;
            p->rel_y = block->height * (i + 1) / (output_count + 1);
        }
    }

    canvas->block_count++;
    return block->id;
}

/* 移除块 */
int lv00_block_canvas_remove_block(Lv00BlockCanvasView *canvas, int block_id) {
    if (!canvas || block_id <= 0) return -1;
    int found = -1;
    for (int i = 0; i < canvas->block_count; i++) {
        if (canvas->blocks[i].id == block_id) { found = i; break; }
    }
    if (found < 0) return -1;

    /* 释放端口 */
    lv00_free((void **)&canvas->blocks[found].ports);

    /* 移除相关连接 */
    int new_c = 0;
    for (int i = 0; i < canvas->connection_count; i++) {
        if (canvas->connections[i].from_block_id != block_id &&
            canvas->connections[i].to_block_id != block_id) {
            if (new_c != i) {
                canvas->connections[new_c] = canvas->connections[i];
            }
            new_c++;
        }
    }
    canvas->connection_count = new_c;

    /* 用最后一个元素填充空位 */
    canvas->blocks[found] = canvas->blocks[canvas->block_count - 1];
    canvas->block_count--;
    return 0;
}

/* 查找块 */
static Lv00VisualBlock *find_block(Lv00BlockCanvasView *canvas, int block_id) {
    if (!canvas || block_id <= 0) return NULL;
    for (int i = 0; i < canvas->block_count; i++) {
        if (canvas->blocks[i].id == block_id) return &canvas->blocks[i];
    }
    return NULL;
}

/* 查找端口绝对坐标 */
static int find_port_pos(Lv00BlockCanvasView *canvas, int block_id, int port_id,
                          double *px, double *py) {
    Lv00VisualBlock *block = find_block(canvas, block_id);
    if (!block) return -1;
    for (int i = 0; i < block->port_count; i++) {
        if (block->ports[i].id == port_id) {
            *px = block->x + block->ports[i].rel_x;
            *py = block->y + block->ports[i].rel_y;
            return 0;
        }
    }
    return -1;
}

/* 连接两个块的端口 */
int lv00_block_canvas_connect_blocks(Lv00BlockCanvasView *canvas,
                                      int from_block_id, int from_port_id,
                                      int to_block_id, int to_port_id) {
    if (!canvas || from_block_id <= 0 || to_block_id <= 0) return -1;
    if (from_block_id == to_block_id) return -1;

    /* 验证端口存在 */
    Lv00VisualBlock *from_block = find_block(canvas, from_block_id);
    Lv00VisualBlock *to_block = find_block(canvas, to_block_id);
    if (!from_block || !to_block) return -1;

    int from_found = 0, to_found = 0;
    for (int i = 0; i < from_block->port_count; i++) {
        if (from_block->ports[i].id == from_port_id) { from_found = 1; break; }
    }
    for (int i = 0; i < to_block->port_count; i++) {
        if (to_block->ports[i].id == to_port_id) { to_found = 1; break; }
    }
    if (!from_found || !to_found) return -1;

    /* 自动扩容 */
    if (canvas->connection_count >= canvas->connection_capacity) {
        int new_cap = canvas->connection_capacity * 2;
        Lv00BlockConnection *new_arr = lv00_realloc(canvas->connections,
                                                new_cap * sizeof(Lv00BlockConnection));
        if (!new_arr) return -1;
        canvas->connections = new_arr;
        canvas->connection_capacity = new_cap;
    }

    Lv00BlockConnection *conn = &canvas->connections[canvas->connection_count];
    conn->id = canvas->next_connection_id++;
    conn->from_block_id = from_block_id;
    conn->from_port_id = from_port_id;
    conn->to_block_id = to_block_id;
    conn->to_port_id = to_port_id;

    canvas->connection_count++;
    return conn->id;
}

/* 生成 SVG 输出 */
char *lv00_block_canvas_render_svg(Lv00BlockCanvasView *canvas) {
    if (!canvas) return NULL;

    /* 计算包围盒 */
    double min_x = 1e18, min_y = 1e18, max_x = -1e18, max_y = -1e18;
    for (int i = 0; i < canvas->block_count; i++) {
        Lv00VisualBlock *b = &canvas->blocks[i];
        if (b->x < min_x) min_x = b->x;
        if (b->y < min_y) min_y = b->y;
        if (b->x + b->width > max_x) max_x = b->x + b->width;
        if (b->y + b->height > max_y) max_y = b->y + b->height;
    }
    double margin = 40.0;
    min_x -= margin; min_y -= margin;
    max_x += margin; max_y += margin;
    if (max_x - min_x < 100) { min_x -= 50; max_x += 50; }
    if (max_y - min_y < 100) { min_y -= 50; max_y += 50; }

    /* 估算缓冲区 */
    int buf_size = 4096 + canvas->block_count * 1024 + canvas->connection_count * 512;
    char *buf = lv00_calloc(buf_size, sizeof(char));
    if (!buf) return NULL;

    int pos = 0;
    {
        int n = snprintf(buf + pos, buf_size - pos,
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<svg xmlns=\"http://www.w3.org/2000/svg\" "
            "viewBox=\"%g %g %g %g\" width=\"800\" height=\"600\">\n",
            min_x, min_y, max_x - min_x, max_y - min_y);
        if (n > 0) pos += (pos + n < buf_size) ? n : (buf_size - 1 - pos);
    }

    /* 绘制连接（贝塞尔曲线） */
    for (int i = 0; i < canvas->connection_count; i++) {
        Lv00BlockConnection *c = &canvas->connections[i];
        double x1, y1, x2, y2;
        if (find_port_pos(canvas, c->from_block_id, c->from_port_id, &x1, &y1) != 0) continue;
        if (find_port_pos(canvas, c->to_block_id, c->to_port_id, &x2, &y2) != 0) continue;

        /* 控制点：水平方向偏移 */
        double dx = fabs(x2 - x1) * 0.5;
        if (dx < 30.0) dx = 30.0;
        double cx1 = x1 + dx;
        double cy1 = y1;
        double cx2 = x2 - dx;
        double cy2 = y2;

        pos += snprintf(buf + pos, buf_size - pos,
            "  <path d=\"M %g,%g C %g,%g %g,%g %g,%g\" "
            "fill=\"none\" stroke=\"#666666\" stroke-width=\"2\"/>\n",
            x1, y1, cx1, cy1, cx2, cy2, x2, y2);
        if (pos >= buf_size) pos = buf_size - 1;
    }

    /* 绘制块 */
    for (int i = 0; i < canvas->block_count; i++) {
        Lv00VisualBlock *b = &canvas->blocks[i];
        const char *color = (b->type >= 0 && b->type <= LV00_BLOCK_TYPE_LOOP)
                              ? block_type_colors[b->type] : "#999999";

        /* 圆角矩形 */
        double rx = 8.0;
        pos += snprintf(buf + pos, buf_size - pos,
            "  <rect x=\"%g\" y=\"%g\" width=\"%g\" height=\"%g\" "
            "rx=\"%g\" ry=\"%g\" fill=\"%s\" stroke=\"#333333\" "
            "stroke-width=\"2\" opacity=\"0.9\"/>\n",
            b->x, b->y, b->width, b->height, rx, rx, color);
        if (pos >= buf_size) pos = buf_size - 1;

        /* 标签 */
        pos += snprintf(buf + pos, buf_size - pos,
            "  <text x=\"%g\" y=\"%g\" font-size=\"13\" fill=\"white\" "
            "text-anchor=\"middle\" dominant-baseline=\"middle\" "
            "font-weight=\"bold\">%s</text>\n",
            b->x + b->width / 2.0, b->y + b->height / 2.0, b->label);
        if (pos >= buf_size) pos = buf_size - 1;

        /* 绘制端口（小圆圈） */
        for (int j = 0; j < b->port_count; j++) {
            Lv00BlockPort *p = &b->ports[j];
            double px = b->x + p->rel_x;
            double py = b->y + p->rel_y;
            const char *port_color = p->is_input ? "#FFFFFF" : "#FFFFFF";
            const char *port_stroke = p->is_input ? "#333333" : "#333333";
            pos += snprintf(buf + pos, buf_size - pos,
                "  <circle cx=\"%g\" cy=\"%g\" r=\"5\" "
                "fill=\"%s\" stroke=\"%s\" stroke-width=\"1.5\"/>\n",
                px, py, port_color, port_stroke);
            if (pos >= buf_size) pos = buf_size - 1;
        }
    }

    pos += snprintf(buf + pos, buf_size - pos, "</svg>\n");
    if (pos >= buf_size) pos = buf_size - 1;
    return buf;
}

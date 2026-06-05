#include "lv00/representation_converter.h"
#include "lv00/func_block.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* 内部辅助：追加字符串到动态缓冲区 */
typedef struct {
    char *data;
    int len;
    int cap;
} TextBuf;

static void buf_init(TextBuf *b) {
    b->cap = 1024;
    b->data = calloc(b->cap, 1);
    b->len = 0;
}

static void buf_append(TextBuf *b, const char *s) {
    int slen = (int)strlen(s);
    while (b->len + slen + 1 > b->cap) {
        b->cap *= 2;
        b->data = realloc(b->data, b->cap);
    }
    memcpy(b->data + b->len, s, slen + 1);
    b->len += slen;
}

static void buf_appendf(TextBuf *b, const char *fmt, ...) {
    char tmp[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    buf_append(b, tmp);
}

static void buf_free(TextBuf *b) {
    free(b->data);
    b->data = NULL;
    b->len = b->cap = 0;
}

/* 内部辅助：生成缩进字符串 */
static void append_indent(TextBuf *buf, int level) {
    for (int i = 0; i < level; i++) {
        buf_append(buf, "  ");
    }
}

/* 将函数块图转换为 Lv-00 DSL 文本 */
/* 按拓扑序遍历块图，为每个 FuncBlock 生成 DSL 声明 */
Lv00ConvertResult lv00_convert_block_to_text(void *graph) {
    Lv00ConvertResult result = {0};
    if (!graph) {
        result.success = 0;
        strncpy(result.error_msg, "NULL graph", sizeof(result.error_msg));
        return result;
    }

    /* graph 指向 FuncBlock 数组（通过 block_graph 传递） */
    /* 假设 graph 是一个包含 FuncBlock** 指针和数量的结构体 */
    typedef struct {
        FuncBlock **blocks;
        int count;
    } BlockGraphView;

    BlockGraphView *bg = (BlockGraphView *)graph;
    TextBuf buf;
    buf_init(&buf);

    /* 遍历所有函数块（假设已按拓扑序排列） */
    for (int i = 0; i < bg->count; i++) {
        FuncBlock *fb = bg->blocks[i];
        if (!fb) continue;

        const char *name = func_block_get_name(fb);
        if (!name) name = "unnamed";

        /* 生成块声明头部 */
        buf_appendf(&buf, "block %s {\n", name);

        /* 生成输入端口声明 */
        int in_count = func_block_get_input_count(fb);
        for (int j = 0; j < in_count; j++) {
            append_indent(&buf, 1);
            buf_appendf(&buf, "input port%d\n", fb->input_port_ids ? fb->input_port_ids[j] : j);
        }

        /* 生成输出端口声明 */
        int out_count = func_block_get_output_count(fb);
        for (int j = 0; j < out_count; j++) {
            append_indent(&buf, 1);
            buf_appendf(&buf, "output port%d\n", fb->output_port_ids ? fb->output_port_ids[j] : j);
        }

        /* 生成块体占位 */
        append_indent(&buf, 1);
        buf_append(&buf, "// block body\n");

        buf_append(&buf, "}\n\n");
    }

    result.output = buf.data;
    result.success = 1;
    return result;
}

/* 解析 Lv-00 DSL 文本，构建函数块图 */
/* 逐行解析器：跟踪花括号深度，支持嵌套块声明 */
Lv00ConvertResult lv00_convert_text_to_block(const char *code) {
    Lv00ConvertResult result = {0};
    if (!code) {
        result.success = 0;
        strncpy(result.error_msg, "NULL code", sizeof(result.error_msg));
        return result;
    }

    /* 验证输入文本非空 */
    int len = (int)strlen(code);
    if (len == 0) {
        result.success = 0;
        strncpy(result.error_msg, "empty code input", sizeof(result.error_msg));
        return result;
    }

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
    sg->cap = 16;
    sg->blocks = calloc(sg->cap, sizeof(FuncBlock *));

    /* 逐行解析，跟踪花括号深度以支持嵌套块 */
    int block_id_counter = 0;
    int brace_depth = 0;          /* 当前花括号嵌套深度 */
    int block_brace_start = -1;   /* 当前块的花括号起始深度 */
    FuncBlock *cur_fb = NULL;     /* 当前正在解析的块 */
    int cur_inputs[64], cur_outputs[64];
    int cur_in_cnt = 0, cur_out_cnt = 0;

    const char *line_start = code;
    while (*line_start) {
        /* 找到行尾 */
        const char *line_end = line_start;
        while (*line_end && *line_end != '\n' && *line_end != '\r') line_end++;

        /* 计算行长度并跳过前导空白 */
        const char *p = line_start;
        while (p < line_end && (*p == ' ' || *p == '\t')) p++;
        int line_len = (int)(line_end - p);

        /* 处理非空行 */
        if (line_len > 0) {
            /* 统计本行花括号（排除字符串内和注释内的） */
            int open_braces = 0, close_braces = 0;
            for (const char *c = p; c < line_end; c++) {
                if (*c == '{') open_braces++;
                else if (*c == '}') close_braces++;
            }

            /* 检测顶层 "block <name> {" 声明 */
            if (brace_depth == 0 && strncmp(p, "block ", 6) == 0) {
                /* 提取块名称 */
                const char *name_start = p + 6;
                char name[256] = {0};
                int ni = 0;
                while (name_start < line_end && *name_start != ' ' &&
                       *name_start != '{' && *name_start != '\n' && ni < 255) {
                    name[ni++] = *name_start++;
                }
                /* 跳过名称后的空白到花括号 */
                while (name_start < line_end && *name_start != '{') name_start++;

                /* 创建函数块 */
                cur_fb = func_block_create(block_id_counter++);
                if (cur_fb) {
                    func_block_set_name(cur_fb, name);
                    cur_in_cnt = 0;
                    cur_out_cnt = 0;
                }
                block_brace_start = brace_depth;
            }
            /* 在块体内检测 input/output 端口声明 */
            else if (cur_fb && brace_depth > block_brace_start) {
                if (strncmp(p, "input ", 6) == 0) {
                    const char *port_str = p + 6;
                    int port_id = 0;
                    if (strncmp(port_str, "port", 4) == 0) {
                        port_str += 4;
                        port_id = atoi(port_str);
                    }
                    if (cur_in_cnt < 64) cur_inputs[cur_in_cnt++] = port_id;
                }
                else if (strncmp(p, "output ", 7) == 0) {
                    const char *port_str = p + 7;
                    int port_id = 0;
                    if (strncmp(port_str, "port", 4) == 0) {
                        port_str += 4;
                        port_id = atoi(port_str);
                    }
                    if (cur_out_cnt < 64) cur_outputs[cur_out_cnt++] = port_id;
                }
            }

            /* 先处理关闭花括号（在增加深度之前） */
            brace_depth -= close_braces;

            /* 当回到块的花括号起始深度时，块体结束 */
            if (cur_fb && brace_depth <= block_brace_start && close_braces > 0) {
                /* 设置端口 */
                if (cur_in_cnt > 0) func_block_set_input_ports(cur_fb, cur_inputs, cur_in_cnt);
                if (cur_out_cnt > 0) func_block_set_output_ports(cur_fb, cur_outputs, cur_out_cnt);

                /* 添加到块图 */
                if (sg->count >= sg->cap) {
                    sg->cap *= 2;
                    sg->blocks = realloc(sg->blocks, sg->cap * sizeof(FuncBlock *));
                }
                sg->blocks[sg->count++] = cur_fb;
                cur_fb = NULL;
                block_brace_start = -1;
            }

            /* 处理打开花括号 */
            brace_depth += open_braces;
        }
        else {
            /* 空行：仍需统计花括号 */
            for (const char *c = p; c < line_end; c++) {
                if (*c == '{') brace_depth++;
                else if (*c == '}') {
                    brace_depth--;
                    if (cur_fb && brace_depth <= block_brace_start) {
                        if (cur_in_cnt > 0) func_block_set_input_ports(cur_fb, cur_inputs, cur_in_cnt);
                        if (cur_out_cnt > 0) func_block_set_output_ports(cur_fb, cur_outputs, cur_out_cnt);
                        if (sg->count >= sg->cap) {
                            sg->cap *= 2;
                            sg->blocks = realloc(sg->blocks, sg->cap * sizeof(FuncBlock *));
                        }
                        sg->blocks[sg->count++] = cur_fb;
                        cur_fb = NULL;
                        block_brace_start = -1;
                    }
                }
            }
        }

        /* 跳到下一行 */
        if (*line_end == '\r') line_end++;
        if (*line_end == '\n') line_end++;
        line_start = line_end;
    }

    /* 将解析结果作为输出 */
    result.output = sg;
    result.success = 1;
    return result;
}

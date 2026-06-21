#include "lv00/representation_converter.h"
#include "lv00/func_block.h"
#include "lv00/lv00_utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* 内部辅助：追加字符串到动态缓冲区 */
typedef struct {
    char *data;
    int len;
    int cap;
} TextBuf;

static void buf_init(TextBuf *b) {
    b->cap = 1024;
    b->data = lv00_calloc(b->cap, 1);
    if (!b->data) {
        b->cap = 0;
        return;
    }
    b->len = 0;
}

static void buf_append(TextBuf *b, const char *s) {
    int slen = (int)strlen(s);
    while (b->len + slen + 1 > b->cap) {
        b->cap *= 2;
        char *tmp = lv00_realloc(b->data, b->cap);
        if (!tmp) {
            b->cap /= 2; /* restore old capacity */
            return;
        }
        b->data = tmp;
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
    lv00_free((void **)&b->data);
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
/* 逐行扫描 "block <name> {" 模式，为每个块创建 FuncBlock 并收集端口 */
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

    /* 逐行扫描 "block <name> {" 模式 */
    /* 为每个块创建 FuncBlock，收集输入/输出端口 */
    typedef struct {
        FuncBlock **blocks;
        int count;
        int cap;
    } SimpleBlockGraph;

    SimpleBlockGraph *sg = lv00_calloc(1, sizeof(SimpleBlockGraph));
    if (!sg) {
        result.success = 0;
        strncpy(result.error_msg, "out of memory", sizeof(result.error_msg));
        return result;
    }
    sg->cap = 16;
    sg->blocks = lv00_calloc(sg->cap, sizeof(FuncBlock *));
    if (!sg->blocks) {
        lv00_free((void **)&sg);
        result.success = 0;
        strncpy(result.error_msg, "out of memory", sizeof(result.error_msg));
        return result;
    }

    /* 逐行扫描 */
    const char *p = code;
    int block_id_counter = 0;
    while (*p) {
        /* 跳过空白行 */
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if (!*p) break;

        /* 检测 "block" 关键字 */
        if (strncmp(p, "block ", 6) == 0) {
            p += 6;
            /* 提取块名称 */
            char name[256] = {0};
            int ni = 0;
            while (*p && *p != ' ' && *p != '{' && *p != '\n' && ni < 255) {
                name[ni++] = *p++;
            }
            /* 跳到花括号 */
            while (*p && *p != '{') p++;
            if (*p == '{') p++;

            /* 创建函数块 */
            FuncBlock *fb = func_block_create(block_id_counter++);
            if (fb) {
                func_block_set_name(fb, name);

                /* 扫描块体中的 input/output 声明 */
                int inputs[MAX_BLOCK_PORTS], outputs[MAX_BLOCK_PORTS];
                int in_cnt = 0, out_cnt = 0;

                while (*p && *p != '}') {
                    /* 跳过空白 */
                    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
                    if (!*p || *p == '}') break;

                    /* 检测 input */
                    if (strncmp(p, "input ", 6) == 0) {
                        p += 6;
                        int port_id = 0;
                        if (strncmp(p, "port", 4) == 0) {
                            p += 4;
                            port_id = atoi(p);
                        }
                        if (in_cnt < 64) inputs[in_cnt++] = port_id;
                        /* 跳到行尾 */
                        while (*p && *p != '\n') p++;
                    }
                    /* 检测 output */
                    else if (strncmp(p, "output ", 7) == 0) {
                        p += 7;
                        int port_id = 0;
                        if (strncmp(p, "port", 4) == 0) {
                            p += 4;
                            port_id = atoi(p);
                        }
                        if (out_cnt < MAX_BLOCK_PORTS) outputs[out_cnt++] = port_id;
                        /* 跳到行尾 */
                        while (*p && *p != '\n') p++;
                    }
                    else {
                        /* 跳过其他行 */
                        while (*p && *p != '\n') p++;
                    }
                }

                /* 设置端口 */
                if (in_cnt > 0) func_block_set_input_ports(fb, inputs, in_cnt);
                if (out_cnt > 0) func_block_set_output_ports(fb, outputs, out_cnt);

                /* 添加到块图 */
                if (sg->count >= sg->cap) {
                    sg->cap *= 2;
                    FuncBlock **tmp = lv00_realloc(sg->blocks, sg->cap * sizeof(FuncBlock *));
                    if (!tmp) {
                        sg->cap /= 2; /* restore old capacity */
                        break;
                    }
                    sg->blocks = tmp;
                }
                sg->blocks[sg->count++] = fb;
            }

            if (*p == '}') p++;
        }
        else {
            /* 跳过非块行 */
            while (*p && *p != '\n') p++;
        }
    }

    /* 将解析结果作为输出 */
    result.output = sg;
    result.success = 1;
    return result;
}

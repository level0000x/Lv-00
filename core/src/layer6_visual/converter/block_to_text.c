#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "lv/func_block.h"
#include "lv/lv_internal.h"
#include "lv/lv_parse_utils.h"
#include "lv/representation_converter.h"
#include "lv/lv_strbuf.h"

/* layer6 新实现接管：屏蔽 layer2 旧桩的同名直接转换 API */
#define LV_HAS_LAYER6_CONVERTER

/* 内部辅助：统一使用 lvStrBuf 标准字符串构建器（原 TextBuf 手写 String Builder 已收敛至 lv/lv_strbuf.h） */

/* 将函数块图转换为 Lv-00 DSL 文本 */
/* 按拓扑序遍历块图，为每个 FuncBlock 生成 DSL 声明 */
lvConvertResult lv_convert_block_to_text(void *graph) {
    lvConvertResult result = {0};
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

    BlockGraphView *bg = (BlockGraphView *) graph;
    lvStrBuf buf = {0};

    /* 遍历所有函数块（假设已按拓扑序排列） */
    for (int i = 0; i < bg->count; i++) {
        FuncBlock *fb = bg->blocks[i];
        if (!fb)
            continue;

        const char *name = func_block_get_name(fb);
        if (!name)
            name = "unnamed";

        /* 生成块声明头部 */
        lv_strbuf_printf(&buf, "block %s {\n", name);

        /* 生成输入端口声明 */
        int in_count = func_block_get_input_count(fb);
        for (int j = 0; j < in_count; j++) {
            lv_strbuf_append_n(&buf, ' ', 2);
            lv_strbuf_printf(&buf, "input port%d\n", fb->input_port_ids ? fb->input_port_ids[j] : j);
        }

        /* 生成输出端口声明 */
        int out_count = func_block_get_output_count(fb);
        for (int j = 0; j < out_count; j++) {
            lv_strbuf_append_n(&buf, ' ', 2);
            lv_strbuf_printf(&buf, "output port%d\n", fb->output_port_ids ? fb->output_port_ids[j] : j);
        }

        /* 生成块体占位 */
        lv_strbuf_append_n(&buf, ' ', 2);
        lv_strbuf_printf(&buf, "// block body\n");

        lv_strbuf_printf(&buf, "}\n\n");
    }

    result.output = lv_strbuf_to_string(&buf);
    result.success = 1;
    return result;
}

/* 解析 Lv-00 DSL 文本，构建函数块图 */
/* 逐行扫描 "block <name> {" 模式，为每个块创建 FuncBlock 并收集端口 */
lvConvertResult lv_convert_text_to_block(const char *code) {
    lvConvertResult result = {0};
    if (!code) {
        result.success = 0;
        strncpy(result.error_msg, "NULL code", sizeof(result.error_msg));
        return result;
    }

    /* 验证输入文本非空 */
    int len = (int) strlen(code);
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

    SimpleBlockGraph *sg = lv_calloc(1, sizeof(SimpleBlockGraph));
    if (!sg) {
        result.success = 0;
        strncpy(result.error_msg, "out of memory", sizeof(result.error_msg));
        return result;
    }
    sg->cap = 16;
    sg->blocks = lv_calloc(sg->cap, sizeof(FuncBlock *));
    if (!sg->blocks) {
        lv_free((void **) &sg);
        result.success = 0;
        strncpy(result.error_msg, "out of memory", sizeof(result.error_msg));
        return result;
    }

    /* 逐行扫描 */
    const char *p = code;
    int block_id_counter = 0;
    while (*p) {
        /* 跳过空白行 */
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
            p++;
        if (!*p)
            break;

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
            while (*p && *p != '{')
                p++;
            if (*p == '{')
                p++;

            /* 创建函数块 */
            FuncBlock *fb = func_block_create(block_id_counter++);
            if (fb) {
                func_block_set_name(fb, name);

                /* 扫描块体中的 input/output 声明 */
                int inputs[MAX_BLOCK_PORTS], outputs[MAX_BLOCK_PORTS];
                int in_cnt = 0, out_cnt = 0;

                while (*p && *p != '}') {
                    /* 跳过空白 */
                    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
                        p++;
                    if (!*p || *p == '}')
                        break;

                    /* 检测 input */
                    if (strncmp(p, "input ", 6) == 0) {
                        p += 6;
                        int port_id = 0;
                        if (strncmp(p, "port", 4) == 0) {
                            p += 4;
                            port_id = 0;
                            lv_parse_int(p, &port_id);
                        }
                        if (in_cnt < 64)
                            inputs[in_cnt++] = port_id;
                        /* 跳到行尾 */
                        while (*p && *p != '\n')
                            p++;
                    }
                    /* 检测 output */
                    else if (strncmp(p, "output ", 7) == 0) {
                        p += 7;
                        int port_id = 0;
                        if (strncmp(p, "port", 4) == 0) {
                            p += 4;
                            port_id = 0;
                            lv_parse_int(p, &port_id);
                        }
                        if (out_cnt < MAX_BLOCK_PORTS)
                            outputs[out_cnt++] = port_id;
                        /* 跳到行尾 */
                        while (*p && *p != '\n')
                            p++;
                    } else {
                        /* 跳过其他行 */
                        while (*p && *p != '\n')
                            p++;
                    }
                }

                /* 设置端口 */
                if (in_cnt > 0)
                    func_block_set_input_ports(fb, inputs, in_cnt);
                if (out_cnt > 0)
                    func_block_set_output_ports(fb, outputs, out_cnt);

                /* 添加到块图 */
                if (!lv_ensure_capacity((void **) &sg->blocks, sg->count, &sg->cap, sizeof(FuncBlock *), 0))
                    break; /* lv_ensure_capacity 失败时指针/容量不变，无需回滚 */
                sg->blocks[sg->count++] = fb;
            }

            if (*p == '}')
                p++;
        } else {
            /* 跳过非块行 */
            while (*p && *p != '\n')
                p++;
        }
    }

    /* 将解析结果作为输出 */
    result.output = sg;
    result.success = 1;
    return result;
}

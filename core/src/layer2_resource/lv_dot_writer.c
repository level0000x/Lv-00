/**
 * @file lv_dot_writer.c
 * @brief 公共 DOT（Graphviz）写入器实现
 *
 * 统一 digraph 样板输出（lv_dot_begin/end）与节点/边语句（lv_dot_node/edge），
 * label 统一经 lv_str_json_escape 两遍法转义后写入（参照 proof_trace_tree.c
 * 的现成做法），消除各层重复且未转义的手写 DOT 生成器。
 *
 * 本文件仅依赖 Layer 2 基础设施（lvStrBuf / lv_str_utils / lv_file），
 * 因此置于 layer2_resource，供 L2~L5 各层 DOT 生成器复用。
 */

#include "lv/lv_dot_writer.h"

#include <string.h>

#include "lv/lv_file.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_utils.h"

/* ============== 内部辅助 ============== */

/**
 * @brief 将字符串经 JSON/DOT 转义后追加到 lvStrBuf（两遍法：先算长度再转义）
 * @param sb 目标 lvStrBuf（追加模式）
 * @param s  源字符串（NUL 结尾）
 */
static void dot_escape_append(lvStrBuf *sb, const char *s) {
    char *esc = lv_str_json_escape_alloc(s, strlen(s), NULL);
    if (esc) {
        lv_strbuf_printf(sb, "%s", esc);
        lv_free((void **) &esc);
    }
}

/* ============== 公共 API ============== */

void lv_dot_begin(lvStrBuf *sb, const char *graph_name, const char *rankdir,
                  const char *node_defaults, const char *edge_defaults) {
    if (!sb)
        return;
    if (graph_name) {
        lv_strbuf_printf(sb, "digraph %s {\n", graph_name);
    }
    if (rankdir) {
        lv_strbuf_printf(sb, "    rankdir=%s;\n", rankdir);
    }
    if (node_defaults) {
        lv_strbuf_printf(sb, "    node [%s];\n", node_defaults);
    }
    if (edge_defaults) {
        lv_strbuf_printf(sb, "    edge [%s];\n", edge_defaults);
    }
    lv_strbuf_printf(sb, "\n");
}

void lv_dot_node(lvStrBuf *sb, const char *id, const char *label, const char *extra_attrs) {
    if (!sb || !id)
        return;
    lv_strbuf_printf(sb, "    %s [", id);
    if (label) {
        lv_strbuf_printf(sb, "label=\"");
        dot_escape_append(sb, label);
        lv_strbuf_printf(sb, "\"");
    }
    if (extra_attrs && extra_attrs[0]) {
        if (label) {
            lv_strbuf_printf(sb, ", ");
        }
        lv_strbuf_printf(sb, "%s", extra_attrs);
    }
    lv_strbuf_printf(sb, "];\n");
}

void lv_dot_edge(lvStrBuf *sb, const char *from, const char *to, const char *label,
                 const char *extra_attrs) {
    if (!sb || !from || !to)
        return;
    lv_strbuf_printf(sb, "    %s -> %s", from, to);
    if (label || (extra_attrs && extra_attrs[0])) {
        lv_strbuf_printf(sb, " [");
        if (label) {
            lv_strbuf_printf(sb, "label=\"");
            dot_escape_append(sb, label);
            lv_strbuf_printf(sb, "\"");
        }
        if (extra_attrs && extra_attrs[0]) {
            if (label) {
                lv_strbuf_printf(sb, ", ");
            }
            lv_strbuf_printf(sb, "%s", extra_attrs);
        }
        lv_strbuf_printf(sb, "]");
    }
    lv_strbuf_printf(sb, ";\n");
}

void lv_dot_end(lvStrBuf *sb) {
    if (!sb)
        return;
    lv_strbuf_printf(sb, "}\n");
}

/* 判据 L/J 变体收敛：以整型 id 写节点/边（ID 内部格式化为 "前缀%d"），
 * 消除各 DOT 导出器散落的 idbuf/frombuf/tobuf + snprintf 三连样板。 */
void lv_dot_node_id(lvStrBuf *sb, const char *prefix, int id, const char *label, const char *extra_attrs) {
    char idbuf[32];
    snprintf(idbuf, sizeof(idbuf), "%s%d", prefix, id);
    lv_dot_node(sb, idbuf, label, extra_attrs);
}

void lv_dot_edge_id(lvStrBuf *sb, const char *prefix, int from_id, int to_id, const char *label,
                    const char *extra_attrs) {
    char frombuf[32], tobuf[32];
    snprintf(frombuf, sizeof(frombuf), "%s%d", prefix, from_id);
    snprintf(tobuf, sizeof(tobuf), "%s%d", prefix, to_id);
    lv_dot_edge(sb, frombuf, tobuf, label, extra_attrs);
}

bool lv_dot_write_file(const char *path, const char *content, size_t len) {
    if (!path || !content)
        return false;
    return lv_file_write_all(path, content, len) == 0;
}

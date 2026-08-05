#ifndef lv_DOT_WRITER_H
#define lv_DOT_WRITER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

#include "lv/lv_strbuf.h"

/**
 * @file lv_dot_writer.h
 * @brief 公共 DOT（Graphviz）写入器 —— 统一 digraph 样板与 label 转义
 *
 * 统一各层手写 DOT 生成器的样板：
 *   digraph 名 {
 *       rankdir=TB/LR;
 *       node [默认属性];
 *       edge [默认属性];
 *
 *       节点/边语句;
 *   }
 * 并将 label 统一经 lv_str_json_escape 转义（JSON 字符串字面量转义是
 * DOT 字符串字面量转义的子集，\\n / \\" / \\\\ 等转义结果 DOT 均接受），
 * 消除各调用点重复且未转义的 label 输出。
 *
 * 所有函数输出统一使用固定 4 空格缩进 + 换行的多行格式。
 */

/**
 * @brief 输出 digraph 头（图名、rankdir 与 node/edge 默认属性）
 * @param sb            目标 lvStrBuf（追加模式）
 * @param graph_name    图名（为 NULL 时跳过 digraph 声明行）
 * @param rankdir       布局方向，如 "TB"/"LR"（为 NULL 时跳过 rankdir 行）
 * @param node_defaults node 默认属性列表，如 "shape=box, fontsize=10"（为 NULL 跳过）
 * @param edge_defaults edge 默认属性列表（为 NULL 跳过）
 */
void lv_dot_begin(lvStrBuf *sb, const char *graph_name, const char *rankdir,
                  const char *node_defaults, const char *edge_defaults);

/**
 * @brief 输出一条节点语句
 *
 * 输出 `    id [label="转义后label", 额外属性];`。
 * label 经 lv_str_json_escape 转义；extra_attrs 原样输出（可为 NULL）。
 * 当 label 与 extra_attrs 同时存在时，二者间以 ", " 分隔保证 DOT 语法正确。
 *
 * @param sb          目标 lvStrBuf（追加模式）
 * @param id          节点名（原样输出）
 * @param label       节点显示文本（可为 NULL，省略 label 属性）
 * @param extra_attrs 附加属性列表，如 "shape=box, style=filled"（可为 NULL）
 */
void lv_dot_node(lvStrBuf *sb, const char *id, const char *label, const char *extra_attrs);

/**
 * @brief 输出一条边语句
 *
 * 输出 `    from -> to [label="转义后label", 额外属性];`。
 * label 可为 NULL（省略 [label] 属性），extra_attrs 原样输出（可为 NULL）。
 *
 * @param sb          目标 lvStrBuf（追加模式）
 * @param from        起点节点名（原样输出）
 * @param to          终点节点名（原样输出）
 * @param label       边显示文本（可为 NULL，省略 label 属性）
 * @param extra_attrs 附加属性列表（可为 NULL）
 */
void lv_dot_edge(lvStrBuf *sb, const char *from, const char *to, const char *label,
                 const char *extra_attrs);

/**
 * @brief 输出收尾大括号 `}\n`
 * @param sb 目标 lvStrBuf（追加模式）
 */
void lv_dot_end(lvStrBuf *sb);

/**
 * @brief 将 DOT 内容统一落盘
 *
 * 使用 lv_file_write_all（lv_file 抽象层）写入，失败返回 false。
 *
 * @param path    输出文件路径
 * @param content DOT 内容（NUL 结尾）
 * @param len     DOT 内容长度（字节，不含 NUL）
 * @return true 写入成功，false 失败
 */
bool lv_dot_write_file(const char *path, const char *content, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* lv_DOT_WRITER_H */

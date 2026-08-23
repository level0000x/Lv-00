/**
 * @file test_lv_dot_writer_ext.c
 * @brief 公共 DOT 写入器契约测试（批次 C-㊺续32：lv_dot_writer.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（8 个）：
 *   lv_dot_begin / node / node_id / edge / edge_id / end / append_escaped / write_file
 *
 * 契约要点（与 lv_dot_writer.c 核对）：
 *   - begin：graph_name/rankdir/node_defaults/edge_defaults 均可 NULL（跳过对应行），
 *     固定 4 空格缩进 + 结尾空行。
 *   - node：`    id [label="转义后label", 额外属性];`；label NULL 省略，extra 非空时 ", " 分隔。
 *   - edge：`    from -> to [label="...", ...];`；无 label 且无 extra 时无方括号。
 *   - node_id/edge_id：id 内部格式化为 "前缀%d"。
 *   - append_escaped：JSON/DOT 转义（" -> \"、\ -> \\）。
 *   - write_file：lv_file_write_all 落盘，成功 true。
 *
 * @author Lv-00 Project
 */

#include <stdio.h>
#include <string.h>

#include "lv/lv_dot_writer.h"
#include "lv/lv_strbuf.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：begin/end ============== */

static void test_begin_end(void) {
    lvStrBuf sb = {0};
    lv_strbuf_init(&sb);

    /* 全参数 */
    lv_dot_begin(&sb, "g", "TB", "shape=box, fontsize=10", "color=red");
    TEST_ASSERT_STR_EQ(lv_strbuf_cstr(&sb),
                       "digraph g {\n    rankdir=TB;\n    node [shape=box, fontsize=10];\n    edge [color=red];\n\n");
    lv_strbuf_reset(&sb);

    /* 部分 NULL：跳过对应行 */
    lv_dot_begin(&sb, NULL, NULL, "shape=box", NULL);
    TEST_ASSERT_STR_EQ(lv_strbuf_cstr(&sb), "    node [shape=box];\n\n");
    lv_strbuf_reset(&sb);

    /* 全部 NULL：仅空行 */
    lv_dot_begin(&sb, NULL, NULL, NULL, NULL);
    TEST_ASSERT_STR_EQ(lv_strbuf_cstr(&sb), "\n");
    lv_strbuf_reset(&sb);

    /* end */
    lv_dot_end(&sb);
    TEST_ASSERT_STR_EQ(lv_strbuf_cstr(&sb), "}\n");
    lv_strbuf_reset(&sb);

    /* NULL 安全 */
    lv_dot_begin(NULL, "g", "TB", NULL, NULL);
    lv_dot_end(NULL);

    lv_strbuf_destroy(&sb);
}

/* ============== 测试：node/edge ============== */

static void test_node_edge(void) {
    lvStrBuf sb = {0};
    lv_strbuf_init(&sb);

    /* 节点：label + extra */
    lv_dot_node(&sb, "n1", "Node 1", "shape=box");
    TEST_ASSERT_STR_EQ(lv_strbuf_cstr(&sb), "    n1 [label=\"Node 1\", shape=box];\n");
    lv_strbuf_reset(&sb);

    /* 节点：仅 label */
    lv_dot_node(&sb, "n1", "Node 1", NULL);
    TEST_ASSERT_STR_EQ(lv_strbuf_cstr(&sb), "    n1 [label=\"Node 1\"];\n");
    lv_strbuf_reset(&sb);

    /* 节点：仅 extra */
    lv_dot_node(&sb, "n1", NULL, "shape=box");
    TEST_ASSERT_STR_EQ(lv_strbuf_cstr(&sb), "    n1 [shape=box];\n");
    lv_strbuf_reset(&sb);

    /* 节点：无 label 无 extra */
    lv_dot_node(&sb, "n1", NULL, NULL);
    TEST_ASSERT_STR_EQ(lv_strbuf_cstr(&sb), "    n1 [];\n");
    lv_strbuf_reset(&sb);

    /* 边：label + extra */
    lv_dot_edge(&sb, "a", "b", "e", "color=blue");
    TEST_ASSERT_STR_EQ(lv_strbuf_cstr(&sb), "    a -> b [label=\"e\", color=blue];\n");
    lv_strbuf_reset(&sb);

    /* 边：仅 label */
    lv_dot_edge(&sb, "a", "b", "e", NULL);
    TEST_ASSERT_STR_EQ(lv_strbuf_cstr(&sb), "    a -> b [label=\"e\"];\n");
    lv_strbuf_reset(&sb);

    /* 边：无 label 无 extra */
    lv_dot_edge(&sb, "a", "b", NULL, NULL);
    TEST_ASSERT_STR_EQ(lv_strbuf_cstr(&sb), "    a -> b;\n");
    lv_strbuf_reset(&sb);

    /* NULL 安全 */
    lv_dot_node(NULL, "n1", NULL, NULL);
    lv_dot_node(&sb, NULL, NULL, NULL);
    lv_dot_edge(NULL, "a", "b", NULL, NULL);
    lv_dot_edge(&sb, NULL, "b", NULL, NULL);
    lv_dot_edge(&sb, "a", NULL, NULL, NULL);

    lv_strbuf_destroy(&sb);
}

/* ============== 测试：整型 id 变体 ============== */

static void test_node_edge_id(void) {
    lvStrBuf sb = {0};
    lv_strbuf_init(&sb);

    lv_dot_node_id(&sb, "node", 5, "N5", NULL);
    TEST_ASSERT_STR_EQ(lv_strbuf_cstr(&sb), "    node5 [label=\"N5\"];\n");
    lv_strbuf_reset(&sb);

    lv_dot_edge_id(&sb, "n", 1, 2, NULL, NULL);
    TEST_ASSERT_STR_EQ(lv_strbuf_cstr(&sb), "    n1 -> n2;\n");
    lv_strbuf_reset(&sb);

    lv_dot_edge_id(&sb, "step", 0, 7, "s", NULL);
    TEST_ASSERT_STR_EQ(lv_strbuf_cstr(&sb), "    step0 -> step7 [label=\"s\"];\n");

    lv_strbuf_destroy(&sb);
}

/* ============== 测试：转义 ============== */

static void test_append_escaped(void) {
    lvStrBuf sb = {0};
    lv_strbuf_init(&sb);

    lv_dot_append_escaped(&sb, "a\"b\\c");
    TEST_ASSERT_STR_EQ(lv_strbuf_cstr(&sb), "a\\\"b\\\\c");
    lv_strbuf_reset(&sb);

    lv_dot_append_escaped(&sb, "line1\nline2");
    TEST_ASSERT_STR_EQ(lv_strbuf_cstr(&sb), "line1\\nline2");
    lv_strbuf_reset(&sb);

    /* NULL 无操作 */
    lv_dot_append_escaped(&sb, NULL);
    TEST_ASSERT_STR_EQ(lv_strbuf_cstr(&sb), "");
    lv_dot_append_escaped(NULL, "x");

    lv_strbuf_destroy(&sb);
}

/* ============== 测试：落盘 ============== */

static void test_write_file(void) {
    const char *path = "dot_ext_test_tmp.gv";
    const char *content = "digraph g {\n    a -> b;\n}\n";

    /* NULL 契约 */
    TEST_ASSERT(!lv_dot_write_file(NULL, content, strlen(content)), "NULL path");
    TEST_ASSERT(!lv_dot_write_file(path, NULL, 10), "NULL content");

    /* 成功写入 */
    TEST_ASSERT(lv_dot_write_file(path, content, strlen(content)), "write ok");

    /* 读回验证 */
    FILE *f = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL(f);
    if (f) {
        char buf[256];
        size_t n = fread(buf, 1, sizeof(buf) - 1, f);
        buf[n] = '\0';
        fclose(f);
        TEST_ASSERT_STR_EQ(buf, content);
    }
    remove(path);
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("DotWriterExt")

    printf("\n--- lv_dot_writer (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_begin_end);
    TEST_MAIN_RUN(test_node_edge);
    TEST_MAIN_RUN(test_node_edge_id);
    TEST_MAIN_RUN(test_append_escaped);
    TEST_MAIN_RUN(test_write_file);

TEST_MAIN_END()

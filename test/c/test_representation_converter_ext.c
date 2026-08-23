/**
 * @file test_representation_converter_ext.c
 * @brief 表示转换器契约测试（批次 C-㊺续31：representation_converter.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（8 个）：
 *   lv_converter_create / destroy
 *   lv_convert_to_geometry / to_node_graph / to_text / from_text
 *   lv_converter_verify_roundtrip
 *   lv_simple_block_graph_guard_cleanup
 *
 * 契约要点（与 representation_converter.c 核对）：
 *   - create(graph)：graph 可为 NULL；conflict_count 初始 0。
 *   - destroy：NULL 安全；释放注册的转换器与结构。
 *   - convert_*：conv NULL / 输入 NULL / 转换器未注册均返回 success==0 的
 *     错误结果（error_msg 非空）；注册后调用 convert_forward/backward 回调。
 *   - verify_roundtrip：conv/original NULL 返回 0；lv_VIEW_TEXT_CODE 成功往返
 *     返回 1；其他视图无反向转换器返回 -1（lv_RETURN_ERROR）。
 *   - simple_block_graph_guard_cleanup：NULL 安全（本批修复：原实现无 NULL
 *     守卫，传入 NULL 会解引用崩溃）；销毁已建块并释放结构（堆分配）。
 *
 * @author Lv-00 Project
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "lv/block_graph_view.h"
#include "lv/lv_utils.h"
#include "lv/representation_converter.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试辅助：转换回调 ============== */

/** @brief 前向转换回调：透传输入为输出 */
static lvConvertResult test_fwd(void *input) {
    lvConvertResult r;
    r.success = 1;
    r.output = input;
    r.error_msg[0] = '\0';
    return r;
}

/** @brief 反向转换回调：透传输入为输出 */
static lvConvertResult test_bwd(void *input) {
    lvConvertResult r;
    r.success = 1;
    r.output = input;
    r.error_msg[0] = '\0';
    return r;
}

/** @brief 分配并注册一个转换器到指定槽位（前向/反向回调均指向同一函数） */
static void install_converter(lvConverter **slot, lvConvertResult (*fn)(void *)) {
    lvConverter *c = (lvConverter *) lv_malloc(sizeof(lvConverter));
    TEST_ASSERT_NOT_NULL(c);
    memset(c, 0, sizeof(lvConverter));
    c->convert_forward = fn;
    c->convert_backward = fn;
    *slot = c;
}

/* ============== 测试：生命周期 ============== */

static void test_lifecycle(void) {
    lvRepresentationConverter *conv = lv_converter_create((void *) (intptr_t) 0x1234);
    TEST_ASSERT_NOT_NULL(conv);
    TEST_ASSERT_EQ((intptr_t) conv->core_graph, (intptr_t) 0x1234);
    TEST_ASSERT_EQ(conv->conflict_count, 0);

    lvRepresentationConverter *conv2 = lv_converter_create(NULL);
    TEST_ASSERT_NOT_NULL(conv2);
    TEST_ASSERT_NULL(conv2->core_graph);

    lv_converter_destroy(conv);
    lv_converter_destroy(conv2);
    lv_converter_destroy(NULL);
}

/* ============== 测试：未注册转换路径 ============== */

static void test_unregistered_paths(void) {
    lvRepresentationConverter *conv = lv_converter_create(NULL);
    TEST_ASSERT_NOT_NULL(conv);

    /* conv NULL：错误结果 */
    lvConvertResult r = lv_convert_to_geometry(NULL, (void *) (intptr_t) 1);
    TEST_ASSERT_EQ(r.success, 0);
    TEST_ASSERT(r.error_msg[0] != '\0', "error msg set");

    /* 输入 NULL：错误结果 */
    r = lv_convert_to_geometry(conv, NULL);
    TEST_ASSERT_EQ(r.success, 0);

    /* 未注册转换器：错误结果 */
    r = lv_convert_to_geometry(conv, (void *) (intptr_t) 1);
    TEST_ASSERT_EQ(r.success, 0);
    TEST_ASSERT(r.error_msg[0] != '\0', "unregistered error msg");

    r = lv_convert_to_node_graph(conv, (void *) (intptr_t) 1);
    TEST_ASSERT_EQ(r.success, 0);

    r = lv_convert_to_text(conv, (void *) (intptr_t) 1);
    TEST_ASSERT_EQ(r.success, 0);

    /* from_text：code NULL / 未注册 */
    r = lv_convert_from_text(conv, NULL);
    TEST_ASSERT_EQ(r.success, 0);
    r = lv_convert_from_text(conv, "code");
    TEST_ASSERT_EQ(r.success, 0);

    lv_converter_destroy(conv);
}

/* ============== 测试：注册转换器成功路径 ============== */

static void test_registered_paths(void) {
    lvRepresentationConverter *conv = lv_converter_create(NULL);
    TEST_ASSERT_NOT_NULL(conv);

    /* 注册 forward.to_geometry（堆分配，destroy 释放） */
    install_converter(&conv->forward.to_geometry, test_fwd);
    lvConvertResult r = lv_convert_to_geometry(conv, (void *) (intptr_t) 0xAA);
    TEST_ASSERT_EQ(r.success, 1);
    TEST_ASSERT_EQ((intptr_t) r.output, (intptr_t) 0xAA);

    /* 注册 reverse.from_text */
    install_converter(&conv->reverse.from_text, test_bwd);
    r = lv_convert_from_text(conv, "code");
    TEST_ASSERT_EQ(r.success, 1);
    TEST_ASSERT_NOT_NULL(r.output);

    /* 未注册的槽位仍报错（互不影响） */
    r = lv_convert_to_node_graph(conv, (void *) (intptr_t) 1);
    TEST_ASSERT_EQ(r.success, 0);

    lv_converter_destroy(conv);
}

/* ============== 测试：往返验证 ============== */

static void test_roundtrip(void) {
    lvRepresentationConverter *conv = lv_converter_create(NULL);
    TEST_ASSERT_NOT_NULL(conv);

    /* NULL 契约：conv/original NULL 返回 0 */
    TEST_ASSERT_EQ(lv_converter_verify_roundtrip(NULL, (void *) (intptr_t) 1, lv_VIEW_TEXT_CODE), 0);
    TEST_ASSERT_EQ(lv_converter_verify_roundtrip(conv, NULL, lv_VIEW_TEXT_CODE), 0);

    /* 未注册转换器：to_text 失败返回 0 */
    TEST_ASSERT_EQ(lv_converter_verify_roundtrip(conv, (void *) (intptr_t) 1, lv_VIEW_TEXT_CODE), 0);

    /* 注册 text 往返转换器：成功返回 1 */
    install_converter(&conv->forward.to_text, test_fwd);
    install_converter(&conv->reverse.from_text, test_bwd);
    TEST_ASSERT_EQ(lv_converter_verify_roundtrip(conv, (void *) (intptr_t) 1, lv_VIEW_TEXT_CODE), 1);

    /* 其他视图无反向转换器：返回 -1（lv_RETURN_ERROR） */
    TEST_ASSERT_EQ(lv_converter_verify_roundtrip(conv, (void *) (intptr_t) 1, lv_VIEW_BLOCK_CANVAS), -1);
    TEST_ASSERT_EQ(lv_converter_verify_roundtrip(conv, (void *) (intptr_t) 1, lv_VIEW_NODE_GRAPH), -1);

    lv_converter_destroy(conv);
}

/* ============== 测试：SimpleBlockGraph 守卫 ============== */

static void test_guard_cleanup(void) {
    /* NULL 安全（本批修复：原实现解引用崩溃） */
    lv_simple_block_graph_guard_cleanup(NULL);

    /* 空结构（count=0）堆对象：释放结构不崩 */
    SimpleBlockGraph *sg = (SimpleBlockGraph *) lv_calloc(1, sizeof(SimpleBlockGraph));
    TEST_ASSERT_NOT_NULL(sg);
    lv_simple_block_graph_guard_cleanup(&sg);
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("RepresentationConverterExt")

    printf("\n--- representation_converter (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_lifecycle);
    TEST_MAIN_RUN(test_unregistered_paths);
    TEST_MAIN_RUN(test_registered_paths);
    TEST_MAIN_RUN(test_roundtrip);
    TEST_MAIN_RUN(test_guard_cleanup);

TEST_MAIN_END()

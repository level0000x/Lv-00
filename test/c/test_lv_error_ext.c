/**
 * @file test_lv_error_ext.c
 * @brief 错误处理增强层契约测试（批次 C-㊺续19：lv_error.h 17 个零覆盖 API）
 *
 * 覆盖零覆盖 API：
 *   上下文：context_current / context_init / context_cleanup
 *   设置：set / set_with_cause / set_at / push
 *   读取：code / message / cause / clear / has_error / format_chain /
 *     file / func / line
 *
 * 契约要点（与实现核对）：
 *   - set 系列返回 false（方便 return 链式）。
 *   - 无错误时 code=lv_OK、message=""、cause=NULL、has_error=false。
 *   - set_at 填充 file/func/line；set 不填（NULL/0）。
 *   - push 从旧式体系桥接，返回 true/false。
 *   - format_chain 从最旧帧开始（根因优先），无错误返回 NULL。
 *   - 栈满（>8 帧）丢弃最旧帧。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/lv_error.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：上下文 ============== */

static void test_error_context_api(void) {
    /* context_current：延迟初始化 */
    lvErrorContext *ctx = lv_error_context_current();
    TEST_ASSERT_NOT_NULL(ctx);
    TEST_ASSERT_EQ(ctx->frame_capacity, lv_ERROR_MAX_FRAMES);
    TEST_ASSERT_EQ(ctx->frame_count, 0);

    /* 再次调用返回同一实例 */
    lvErrorContext *ctx2 = lv_error_context_current();
    TEST_ASSERT_EQ(ctx, ctx2);

    /* context_init：手动初始化 */
    lvErrorContext manual;
    memset(&manual, 0xAB, sizeof(manual));
    lv_error_context_init(&manual);
    TEST_ASSERT_EQ(manual.frame_capacity, lv_ERROR_MAX_FRAMES);
    TEST_ASSERT_EQ(manual.frame_count, 0);
    lv_error_context_init(NULL);

    /* context_cleanup：重置后可重新 init */
    lv_error_set(&manual, 5, "msg");
    TEST_ASSERT(lv_error_has_error(&manual), "设置后有错误");
    lv_error_context_cleanup(&manual);
    TEST_ASSERT(!lv_error_has_error(&manual), "清理后无错误");
    TEST_ASSERT_EQ(manual.frame_capacity, 0);
    lv_error_context_cleanup(NULL);

    printf("  test_error_context_api: PASSED\n");
}

/* ============== 测试：设置与读取 ============== */

static void test_error_set_read_api(void) {
    lvErrorContext ctx;
    lv_error_context_init(&ctx);

    /* 初始无错误 */
    TEST_ASSERT(!lv_error_has_error(&ctx), "初始无错误");
    TEST_ASSERT_EQ(lv_error_code(&ctx), lv_OK);
    TEST_ASSERT(strcmp(lv_error_message(&ctx), "") == 0, "空消息");
    TEST_ASSERT_NULL(lv_error_cause(&ctx));
    TEST_ASSERT_NULL(lv_error_file(&ctx));
    TEST_ASSERT_NULL(lv_error_func(&ctx));
    TEST_ASSERT_EQ(lv_error_line(&ctx), 0);

    /* lv_error_set：返回 false + 格式化消息 */
    bool ret = lv_error_set(&ctx, 42, "error %d: %s", 7, "boom");
    TEST_ASSERT(!ret, "set 返回 false");
    TEST_ASSERT(lv_error_has_error(&ctx), "有错误");
    TEST_ASSERT_EQ(lv_error_code(&ctx), 42);
    TEST_ASSERT(strcmp(lv_error_message(&ctx), "error 7: boom") == 0, "格式化消息");

    /* set 不填 file/func/line */
    TEST_ASSERT_NULL(lv_error_file(&ctx));
    TEST_ASSERT_NULL(lv_error_func(&ctx));
    TEST_ASSERT_EQ(lv_error_line(&ctx), 0);

    /* lv_error_set_at：填充位置 */
    ret = lv_error_set_at(&ctx, 43, "file.c", 100, "my_func", "at %d", 9);
    TEST_ASSERT(!ret, "set_at 返回 false");
    TEST_ASSERT_EQ(lv_error_code(&ctx), 43);
    TEST_ASSERT(strcmp(lv_error_message(&ctx), "at 9") == 0, "set_at 消息");
    TEST_ASSERT(strcmp(lv_error_file(&ctx), "file.c") == 0, "文件");
    TEST_ASSERT(strcmp(lv_error_func(&ctx), "my_func") == 0, "函数");
    TEST_ASSERT_EQ(lv_error_line(&ctx), 100);

    /* set_with_cause：链式 */
    lvErrorFrame *cause = lv_error_cause(&ctx); /* 当前栈顶是 set_at 帧（cause=NULL） */
    TEST_ASSERT_NULL(cause);
    /* 显式 cause：取帧 0 */
    lvErrorFrame *f0 = &ctx.frames[0];
    ret = lv_error_set_with_cause(&ctx, 44, f0, "wrapped");
    TEST_ASSERT(!ret, "set_with_cause 返回 false");
    TEST_ASSERT_EQ(lv_error_code(&ctx), 44);
    cause = lv_error_cause(&ctx);
    TEST_ASSERT_NOT_NULL(cause);
    TEST_ASSERT_EQ(cause->code, 42);

    /* lv_error_push：桥接入口，推入 TLS 上下文（非局部 ctx），返回 true */
    lv_error_clear(&ctx);
    bool pr = lv_error_push(45, "push.c", 200, "push_fn", "pushed");
    TEST_ASSERT(pr, "push 成功");
    lvErrorContext *tls = lv_error_context_current();
    TEST_ASSERT_EQ(lv_error_code(tls), 45);
    TEST_ASSERT(strcmp(lv_error_message(tls), "pushed") == 0, "push 消息");
    TEST_ASSERT(strcmp(lv_error_file(tls), "push.c") == 0, "push 文件");
    TEST_ASSERT_EQ(lv_error_line(tls), 200);
    lv_error_clear(tls);

    /* lv_error_clear */
    lv_error_clear(&ctx);
    TEST_ASSERT(!lv_error_has_error(&ctx), "清除后无错误");
    TEST_ASSERT_EQ(lv_error_code(&ctx), lv_OK);

    /* NULL 契约 */
    lv_error_set(NULL, 1, "x");
    lv_error_set_at(NULL, 1, "f", 1, "g", "x");
    lv_error_set_with_cause(NULL, 1, NULL, "x");
    lv_error_clear(NULL);
    TEST_ASSERT(!lv_error_has_error(NULL), "NULL has_error false");
    TEST_ASSERT_EQ(lv_error_code(NULL), lv_OK);
    TEST_ASSERT(strcmp(lv_error_message(NULL), "") == 0, "NULL message 空");
    TEST_ASSERT_NULL(lv_error_cause(NULL));
    TEST_ASSERT_NULL(lv_error_file(NULL));
    TEST_ASSERT_NULL(lv_error_func(NULL));
    TEST_ASSERT_EQ(lv_error_line(NULL), 0);

    printf("  test_error_set_read_api: PASSED\n");
}

/* ============== 测试：format_chain ============== */

static void test_error_chain_api(void) {
    lvErrorContext ctx;
    lv_error_context_init(&ctx);

    /* 无错误 → NULL */
    TEST_ASSERT_NULL(lv_error_format_chain(&ctx));
    TEST_ASSERT_NULL(lv_error_format_chain(NULL));

    /* 根因优先：先 set（帧0），再 set_at（帧1） */
    lv_error_set_at(&ctx, 1, "a.c", 10, "fa", "root");
    lv_error_set_at(&ctx, 2, "b.c", 20, "fb", "wrapped");
    char *chain = lv_error_format_chain(&ctx);
    TEST_ASSERT_NOT_NULL(chain);
    TEST_ASSERT(strstr(chain, "root") != NULL, "含根因消息");
    TEST_ASSERT(strstr(chain, "wrapped") != NULL, "含包装消息");
    TEST_ASSERT(strstr(chain, "caused by") != NULL, "含 cause 链标记");
    TEST_ASSERT(strstr(chain, "a.c") != NULL, "含根因文件");
    TEST_ASSERT(strstr(chain, "b.c") != NULL, "含上层文件");
    lv_free((void **) &chain);

    /* 栈满丢弃最旧：13 帧后 count=8，丢弃 frame0..4，保留 frame5..12 */
    lv_error_context_init(&ctx);
    for (int i = 0; i < 13; i++) {
        lv_error_set(&ctx, 100 + i, "frame %d", i);
    }
    TEST_ASSERT_EQ(ctx.frame_count, lv_ERROR_MAX_FRAMES);
    TEST_ASSERT_EQ(lv_error_code(&ctx), 112);
    chain = lv_error_format_chain(&ctx);
    TEST_ASSERT_NOT_NULL(chain);
    TEST_ASSERT(strstr(chain, "frame 4") == NULL, "最旧帧 frame4 已丢弃");
    TEST_ASSERT(strstr(chain, "frame 5") != NULL, "frame5 为最旧保留帧");
    TEST_ASSERT(strstr(chain, "frame 12") != NULL, "最新帧保留");
    lv_free((void **) &chain);

    printf("  test_error_chain_api: PASSED\n");
}

/* ============== 测试：便捷宏 ============== */

static bool helper_ret_false(lvErrorContext *ctx) {
    lv_ERROR_CTX_RETURN(ctx, 99, "macro fail %d", 1);
}

static void *helper_ret_null(lvErrorContext *ctx) {
    lv_ERROR_CTX_RETURN_NULL(ctx, 98, "macro null");
}

static int helper_ret_neg1(lvErrorContext *ctx) {
    lv_ERROR_CTX_RETURN_NEG1(ctx, 97, "macro neg1");
}

static int helper_wrap(lvErrorContext *ctx) {
    int ret = 5;
    lv_ERROR_CTX_WRAP(ctx, ret, "outer ctx");
}

static void test_error_macro_api(void) {
    lvErrorContext ctx;
    lv_error_context_init(&ctx);

    /* lv_ERROR_CTX_RETURN：set_at + return false */
    TEST_ASSERT(!helper_ret_false(&ctx), "宏返回 false");
    TEST_ASSERT_EQ(lv_error_code(&ctx), 99);
    TEST_ASSERT(strcmp(lv_error_message(&ctx), "macro fail 1") == 0, "宏消息");
    TEST_ASSERT_NOT_NULL(lv_error_file(&ctx));
    TEST_ASSERT_NOT_NULL(lv_error_func(&ctx));
    TEST_ASSERT(lv_error_line(&ctx) > 0, "宏填 line");

    /* RETURN_NULL */
    TEST_ASSERT_NULL(helper_ret_null(&ctx));
    TEST_ASSERT_EQ(lv_error_code(&ctx), 98);

    /* RETURN_NEG1 */
    TEST_ASSERT_EQ(helper_ret_neg1(&ctx), -1);
    TEST_ASSERT_EQ(lv_error_code(&ctx), 97);

    /* lv_ERROR_CTX_WRAP：包装下层错误（宏内含 return） */
    lv_error_context_init(&ctx);
    lv_error_set(&ctx, 10, "inner");
    TEST_ASSERT_EQ(helper_wrap(&ctx), 5);
    TEST_ASSERT(lv_error_has_error(&ctx), "wrap 后有错误");
    lvErrorFrame *cause = lv_error_cause(&ctx);
    TEST_ASSERT_NOT_NULL(cause);
    TEST_ASSERT_EQ(cause->code, 10);

    printf("  test_error_macro_api: PASSED\n");
}

/* ============== 测试：K61 旧式读端接线帧栈 ============== */

static void test_legacy_reader_wiring(void) {
    /* 旧式 lv_set_error → 帧栈读端（lv_get_last_error_message/code）同源返回 */
    lv_clear_error();
    TEST_ASSERT_EQ((int) lv_get_last_error_code(), (int) lv_OK);

    lv_set_error(lv_ERROR_OUT_OF_MEMORY, "wired msg %d", 42);
    TEST_ASSERT_EQ((int) lv_get_last_error_code(), (int) lv_ERROR_OUT_OF_MEMORY);
    TEST_ASSERT(strcmp(lv_get_last_error_message(), "wired msg 42") == 0, "帧栈读端消息");

    /* 帧栈与 TLS 一致（写端桥接同源推入） */
    lvErrorContext *tls = lv_error_context_current();
    TEST_ASSERT(lv_error_has_error(tls), "帧栈有帧");
    TEST_ASSERT(strcmp(lv_error_message(tls), lv_get_last_error_message()) == 0, "帧栈/TLS 同文");

    /* 清错误 → 帧栈清空 + 读端回退 lv_OK */
    lv_clear_error();
    TEST_ASSERT(!lv_error_has_error(tls), "清错误后帧栈空");
    TEST_ASSERT_EQ((int) lv_get_last_error_code(), (int) lv_OK);

    /* 多层错误：读端取栈顶（最新） */
    lv_set_error(lv_ERROR_INVALID_PARAM, "first");
    lv_set_error(lv_ERROR_IO, "second");
    TEST_ASSERT_EQ((int) lv_get_last_error_code(), (int) lv_ERROR_IO);
    TEST_ASSERT(strcmp(lv_get_last_error_message(), "second") == 0, "栈顶最新消息");
    lv_clear_error();

    printf("  test_legacy_reader_wiring: PASSED\n");
}

/* ============== 测试入口 ============== */

TEST_MAIN_BEGIN("Lv-00 Error Ext Test Suite")
    printf("=== Lv-00 Error Ext Test Suite (batch C-㊺续19) ===\n\n");
    lv_init();

    TEST_MAIN_RUN(test_error_context_api);
    TEST_MAIN_RUN(test_error_set_read_api);
    TEST_MAIN_RUN(test_error_chain_api);
    TEST_MAIN_RUN(test_error_macro_api);
    TEST_MAIN_RUN(test_legacy_reader_wiring); /* K61 帧栈接线 */

    lv_cleanup();
TEST_MAIN_END()

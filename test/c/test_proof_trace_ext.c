/**
 * @file test_proof_trace_ext.c
 * @brief 证明追踪契约测试（批次 C-㊺续30：proof_trace.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（4 个）：
 *   lv_proof_trace_add_step / get_step_count / get_rule / is_complete
 * 附带覆盖：
 *   lv_proof_trace_alloc / free（本批修复：原 static 且无调用点，4 个公共 API
 *   无法从外部构造实例；现公开 alloc/free 使系统可测）
 *   lv_proof_trace_mark_complete / export（实现存在但头未声明，本批补齐声明）
 *
 * 契约要点（与 proof_trace.h / proof_trace.c 核对）：
 *   - alloc：分配 ProofTrace（lvDArray 步骤数组，初始 0 步，complete=false）。
 *   - add_step：追加步骤，rule 截断到 127 字符（MAX_RULE_NAME_LENGTH=128 含 NUL），
 *     state 可为 NULL（描述留空）；返回新步骤索引（失败 -1）。
 *   - get_step_count：NULL 返回 0。
 *   - get_rule：索引越界或 NULL 返回 NULL。
 *   - is_complete：初始 false；mark_complete 后 true。
 *   - export：返回调用者负责释放的字符串（lv_strbuf_to_string）。
 *
 * @author Lv-00 Project
 */

#include <stdio.h>
#include <string.h>

#include "lv/proof_trace.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：alloc 初始状态 ============== */

static void test_alloc_initial(void) {
    ProofTrace *t = lv_proof_trace_alloc();
    TEST_ASSERT_NOT_NULL(t);

    TEST_ASSERT_EQ(lv_proof_trace_get_step_count(t), 0);
    TEST_ASSERT(!lv_proof_trace_is_complete(t), "initially incomplete");
    TEST_ASSERT_NULL(lv_proof_trace_get_rule(t, 0));
    TEST_ASSERT_NULL(lv_proof_trace_get_rule(t, -1));

    lv_proof_trace_free(t);
    lv_proof_trace_free(NULL);
}

/* ============== 测试：add_step / get_rule / get_step_count ============== */

static void test_add_step_api(void) {
    ProofTrace *t = lv_proof_trace_alloc();
    TEST_ASSERT_NOT_NULL(t);

    /* 追加步骤，返回索引 */
    TEST_ASSERT_EQ(lv_proof_trace_add_step(t, "modus-ponens", NULL), 0);
    TEST_ASSERT_EQ(lv_proof_trace_add_step(t, "intro-conj", "premise a & b"), 1);
    TEST_ASSERT_EQ(lv_proof_trace_add_step(t, "elim-impl", NULL), 2);
    TEST_ASSERT_EQ(lv_proof_trace_get_step_count(t), 3);

    /* 按索引读取规则；state 参数（const void* 按字符串处理）不影响 rule */
    const char *r0 = lv_proof_trace_get_rule(t, 0);
    const char *r1 = lv_proof_trace_get_rule(t, 1);
    TEST_ASSERT_NOT_NULL(r0);
    TEST_ASSERT_NOT_NULL(r1);
    TEST_ASSERT_STR_EQ(r0, "modus-ponens");
    TEST_ASSERT_STR_EQ(r1, "intro-conj");

    /* 越界返回 NULL */
    TEST_ASSERT_NULL(lv_proof_trace_get_rule(t, 3));
    TEST_ASSERT_NULL(lv_proof_trace_get_rule(t, 100));

    /* NULL 安全 */
    TEST_ASSERT_EQ(lv_proof_trace_add_step(NULL, "rule", NULL), -1);
    TEST_ASSERT_EQ(lv_proof_trace_add_step(t, NULL, NULL), -1);
    TEST_ASSERT_EQ(lv_proof_trace_get_step_count(NULL), 0);
    TEST_ASSERT_NULL(lv_proof_trace_get_rule(NULL, 0));
    TEST_ASSERT(!lv_proof_trace_is_complete(NULL), "is_complete NULL false");

    lv_proof_trace_free(t);
}

/* ============== 测试：长规则名截断 ============== */

static void test_long_rule_truncation(void) {
    ProofTrace *t = lv_proof_trace_alloc();
    TEST_ASSERT_NOT_NULL(t);

    /* 200 字符规则名：截断到 127 字符（MAX_RULE_NAME_LENGTH=128 含 NUL） */
    char long_rule[256];
    memset(long_rule, 'R', sizeof(long_rule) - 1);
    long_rule[sizeof(long_rule) - 1] = '\0';
    TEST_ASSERT_EQ(lv_proof_trace_add_step(t, long_rule, NULL), 0);

    const char *got = lv_proof_trace_get_rule(t, 0);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT(strlen(got) <= 127, "rule truncated to 127");
    TEST_ASSERT(strncmp(got, long_rule, strlen(got)) == 0, "truncated prefix matches");

    lv_proof_trace_free(t);
}

/* ============== 测试：mark_complete / is_complete ============== */

static void test_complete_flag(void) {
    ProofTrace *t = lv_proof_trace_alloc();
    TEST_ASSERT_NOT_NULL(t);

    TEST_ASSERT(!lv_proof_trace_is_complete(t), "incomplete before mark");
    lv_proof_trace_mark_complete(t);
    TEST_ASSERT(lv_proof_trace_is_complete(t), "complete after mark");
    lv_proof_trace_mark_complete(NULL); /* NULL 安全 */

    lv_proof_trace_free(t);
}

/* ============== 测试：export 导出 ============== */

static void test_export_api(void) {
    ProofTrace *t = lv_proof_trace_alloc();
    TEST_ASSERT_NOT_NULL(t);

    TEST_ASSERT_EQ(lv_proof_trace_add_step(t, "rule-a", NULL), 0);
    TEST_ASSERT_EQ(lv_proof_trace_add_step(t, "rule-b", "state-b"), 1);
    lv_proof_trace_mark_complete(t);

    char *out = lv_proof_trace_export(t);
    TEST_ASSERT_NOT_NULL(out);
    TEST_ASSERT(strstr(out, "rule-a") != NULL, "export contains rule-a");
    TEST_ASSERT(strstr(out, "rule-b") != NULL, "export contains rule-b");

    lv_free((void **) &out);

    /* NULL 安全 */
    TEST_ASSERT_NULL(lv_proof_trace_export(NULL));

    lv_proof_trace_free(t);
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("ProofTraceExt")

    printf("\n--- proof_trace (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_alloc_initial);
    TEST_MAIN_RUN(test_add_step_api);
    TEST_MAIN_RUN(test_long_rule_truncation);
    TEST_MAIN_RUN(test_complete_flag);
    TEST_MAIN_RUN(test_export_api);

TEST_MAIN_END()

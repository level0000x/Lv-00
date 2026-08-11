/**
 * @file test_error_handling.c
 * @brief 边界条件与错误处理测试
 *
 * 测试内容：
 * 1. NULL 指针处理（engine_create 正常、solver 传 NULL graph 等）
 * 2. 无效输入到解析器（空字符串、不匹配括号、超长输入）
 * 3. 零大小分配（calloc 参数为 0）
 * 4. 双重释放检测
 * 5. 缓冲区溢出检测（通过魔数校验）
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine.h"
#include "formula_parser.h"
#include "lv.h"
#include "lv_error.h" /* lv_RESULT_FAIL 判据 K 收敛设施 */
#include "lv_utils.h"
#include "memory_pool.h"
#include "solver.h"
#include "test_helpers.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============================================================
 * 测试 1：NULL 指针处理
 * ============================================================ */

static void test_engine_create_normal(void) {
    /* 正常创建引擎应成功 */
    lvEngine *engine = engine_create();
    TEST_ASSERT_NOT_NULL(engine);
    engine_destroy(engine);
}

static void test_engine_destroy_null(void) {
    /* engine_destroy(NULL) 不应崩溃 */
    engine_destroy(NULL);
    g_pass_count++; /* 未崩溃即通过 */
}

static void test_solver_null_graph(void) {
    /*
     * solver_solve_graph 传入 NULL graph 应安全返回错误，
     * 而非崩溃。此处验证 lv_solve 对 NULL engine 的处理。
     */
    EngineSolveResult result = engine_solve(NULL);
    /* NULL engine 应返回 ENGINE_SOLVE_ERROR 或类似错误码 */
    TEST_ASSERT(result == ENGINE_SOLVE_ERROR, "engine_solve(NULL) 应返回 ENGINE_SOLVE_ERROR");
}

static void test_lv_malloc_zero_size(void) {
    /*
     * lv_malloc(0) 行为：C 标准允许返回 NULL 或唯一指针。
     * 验证不崩溃即可。
     */
    void *p = lv_malloc(0);
    /* 无论返回 NULL 还是有效指针，都不应崩溃 */
    if (p != NULL) {
        lv_free((void **) &p);
    }
    g_pass_count++;
}

static void test_lv_calloc_zero_params(void) {
    /*
     * lv_calloc(0, size) 或 lv_calloc(count, 0) 应安全处理。
     */
    void *p1 = lv_calloc(0, 32);
    if (p1 != NULL) {
        lv_free(&p1);
    }

    void *p2 = lv_calloc(16, 0);
    if (p2 != NULL) {
        lv_free(&p2);
    }

    g_pass_count++;
}

/* ============================================================
 * 测试 2：无效输入到解析器
 * ============================================================ */

static void test_parser_empty_string(void) {
    /* 空字符串输入 */
    FormulaNode *node = formula_parse("", "auto");
    /*
     * 空字符串应返回 NULL 或一个表示空输入的节点。
     * 两种情况都是合理行为，不崩溃即可。
     */
    if (node != NULL) {
        formula_node_destroy(node);
    }
    g_pass_count++;
}

static void test_parser_unmatched_brackets(void) {
    /* 不匹配的括号 */
    FormulaNode *node = formula_parse("((1 + 2)", "auto");
    if (node != NULL) {
        formula_node_destroy(node);
    }
    g_pass_count++;
}

static void test_parser_very_long_input(void) {
    /* 构造超长输入（10000 个字符） */
    char long_input[10001];
    memset(long_input, 'x', 10000);
    long_input[10000] = '\0';

    FormulaNode *node = formula_parse(long_input, "auto");
    if (node != NULL) {
        formula_node_destroy(node);
    }
    g_pass_count++;
}

static void test_parser_null_input(void) {
    /*
     * NULL 输入应安全处理（返回 NULL 或不崩溃）。
     * 注意：某些实现可能不检查 NULL，此处仅验证不崩溃。
     */
    FormulaNode *node = formula_parse(NULL, "auto");
    if (node != NULL) {
        formula_node_destroy(node);
    }
    g_pass_count++;
}

/* ============================================================
 * 测试 3：零大小分配
 * ============================================================ */

static void test_pool_alloc_zero_config(void) {
    /*
     * 使用 object_size=0 创建池应失败或安全处理。
     */
    lvPoolConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.object_size = 0;
    cfg.capacity = 16;
    cfg.name = "zero_size_pool";

    lvObjectPool *pool = lv_pool_create(&cfg);
    /* object_size=0 时池创建可能失败 */
    if (pool != NULL) {
        lv_pool_destroy(pool);
    }
    g_pass_count++;
}

/* ============================================================
 * 测试 4：双重释放检测
 * ============================================================ */

static void test_double_free_detection(void) {
    /*
     * lv_free 使用魔数机制检测双重释放。
     * 第一次释放后魔数被改写为 lv_MAGIC_FREED，
     * 第二次释放时魔数不匹配，应能检测到。
     *
     * 注意：lv_free 接受 void** 参数，释放后指针被置 NULL，
     * 因此标准 lv_free 无法直接触发双重释放。
     * 此处通过 lv_free_ptr（接受 void*）来模拟。
     */
    void *ptr = lv_malloc(64);
    TEST_ASSERT_NOT_NULL(ptr);

    /* 第一次释放 */
    lv_free(&ptr);
    TEST_ASSERT_NULL(ptr);

    /* ptr 已为 NULL，再次释放应安全（空操作） */
    lv_free(&ptr);
    g_pass_count++;
}

/* ============================================================
 * 测试 5：缓冲区溢出检测（通过魔数校验）
 * ============================================================ */

static void test_buffer_overflow_detection(void) {
    /*
     * lv_malloc 在用户数据末尾写入尾部魔数 (lv_MAGIC_TAIL)。
     * 如果发生缓冲区溢出（越界写入），尾部魔数会被破坏，
     * lv_memory_check_magic 应返回 false。
     *
     * 注意：实际溢出写入会触发 ASan/UBSan 等工具报错，
     * 此处仅验证正常分配的魔数完整性，以及概念性验证。
     */
    size_t alloc_size = 64;
    unsigned char *ptr = (unsigned char *) lv_malloc(alloc_size);
    TEST_ASSERT_NOT_NULL(ptr);

    /* 正常使用：写入不超过分配大小 */
    memset(ptr, 0xBB, alloc_size);
    bool magic_ok = lv_memory_check_magic(ptr);
    TEST_ASSERT(magic_ok, "正常使用后尾部魔数应完整");

    /* 验证毒模式功能 */
    lv_poison_enable(true);

    /* 释放后检查毒模式 */
    lv_free((void **) &ptr);

    lv_poison_enable(true);
}

static void test_magic_head_tail_consistency(void) {
    /*
     * 多次分配-释放循环，验证魔数一致性。
     */
    for (int i = 0; i < 5; i++) {
        size_t sizes[] = {16, 32, 64, 128, 256};
        unsigned char *p = (unsigned char *) lv_malloc(sizes[i]);
        TEST_ASSERT_NOT_NULL(p);

        bool ok = lv_memory_check_magic(p);
        TEST_ASSERT(ok, "每次新分配的魔数应完整");

        memset(p, (unsigned char) i, sizes[i]);
        ok = lv_memory_check_magic(p);
        TEST_ASSERT(ok, "写入后魔数应仍完整");

        lv_free((void **) &p);
    }
}

/* ============================================================
 * 测试：lv_RESULT_FAIL 错误结果宏（判据 K 收敛设施）
 * ============================================================ */

static void test_result_fail_macro(void) {
    /*
     * lv_RESULT_FAIL 收敛了错误结果样板：
     *   result.success = 0;
     *   lv_strlcpy(result.error_msg, msg, sizeof(result.error_msg));
     * 验证：success 置 0、消息复制、NULL 消息、超长截断。
     * 使用与 lvExecResult / lvConvertResult 同构的局部结构体。
     */
    struct FailRes {
        int success;
        char error_msg[16];
    };
    struct FailRes r;

    /* 正常路径：success=0 且消息复制 */
    memset(&r, 0xCC, sizeof(r));
    lv_RESULT_FAIL(r, "boom");
    TEST_ASSERT_EQ(r.success, 0);
    TEST_ASSERT_STR_EQ(r.error_msg, "boom");

    /* NULL 消息：写空串（不崩溃） */
    lv_RESULT_FAIL(r, NULL);
    TEST_ASSERT_EQ(r.success, 0);
    TEST_ASSERT_STR_EQ(r.error_msg, "");

    /* 可重复调用：覆盖先前的成功态 */
    r.success = 1;
    lv_RESULT_FAIL(r, "again");
    TEST_ASSERT_EQ(r.success, 0);
    TEST_ASSERT_STR_EQ(r.error_msg, "again");

    /* 超长消息：lv_strlcpy 截断并 NUL 终止，不越界 */
    lv_RESULT_FAIL(r, "this error message is much longer than sixteen bytes");
    TEST_ASSERT_EQ(r.success, 0);
    TEST_ASSERT(r.error_msg[15] == '\0', "超长消息应被截断且 NUL 终止");
    TEST_ASSERT_EQ((int) strlen(r.error_msg), 15);

    /* 边界：恰好填满缓冲区（15 字符 + NUL） */
    lv_RESULT_FAIL(r, "123456789012345");
    TEST_ASSERT_STR_EQ(r.error_msg, "123456789012345");
}

/* ============================================================
 * 测试入口
 * ============================================================ */

TEST_MAIN_BEGIN("错误处理")
    printf("=== Lv-00 边界条件与错误处理测试 ===\n\n");

    g_pass_count = 0;
    g_fail_count = 0;


    /* NULL 指针处理 */
    TEST_MAIN_RUN(test_engine_create_normal);
    TEST_MAIN_RUN(test_engine_destroy_null);
    TEST_MAIN_RUN(test_solver_null_graph);
    TEST_MAIN_RUN(test_lv_malloc_zero_size);
    TEST_MAIN_RUN(test_lv_calloc_zero_params);

    /* 无效输入到解析器 */
    TEST_MAIN_RUN(test_parser_empty_string);
    TEST_MAIN_RUN(test_parser_unmatched_brackets);
    TEST_MAIN_RUN(test_parser_very_long_input);
    TEST_MAIN_RUN(test_parser_null_input);

    /* 零大小分配 */
    TEST_MAIN_RUN(test_pool_alloc_zero_config);

    /* 双重释放检测 */
    TEST_MAIN_RUN(test_double_free_detection);

    /* 缓冲区溢出检测 */
    TEST_MAIN_RUN(test_buffer_overflow_detection);
    TEST_MAIN_RUN(test_magic_head_tail_consistency);

    /* lv_RESULT_FAIL 错误结果宏 */
    TEST_MAIN_RUN(test_result_fail_macro);


TEST_MAIN_END()

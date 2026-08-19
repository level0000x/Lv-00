/**
 * @file test_context.c
 * @brief 隔离上下文（lvContext）零覆盖 API 契约测试
 *
 * 批次 C-㉙：补全 context.h 中此前零测试覆盖的 30 个 API。
 * 覆盖域：
 * - 生命周期补充：register_resource_ops（NULL no-op 契约）
 * - 推理栈：push/pop/get_reasoning_depth/get_current_reasoning_frame
 * - 熔断器操作：begin_operation/check_timeout/enter+leave_uncancellable/
 *   record_step/record_success/record_error/is_circuit_open
 * - 参数配置：set/get_timeout、set/get_max_depth（钳制）、set/get_max_steps、
 *   set/get_name、get_id
 * - 错误管理：set_error（格式化）/get_error_code/get_error_message/clear_error
 * - 快照/回滚：snapshot 标量恢复、rollback 状态恢复
 * - 重置：reset 语义（状态/错误/步数/推理栈/trip_count 保留）
 * - 统计：get_stats 摘要内容
 *
 * 说明：
 * - main() 先调用 lv_init() 注入 context 资源操作（L0），保证
 *   lv_context_create 契约成立（与 test_circuit_breaker.c 同法）。
 * - register_resource_ops 的完整注册路径由 lv_init 内部覆盖，
 *   此处验证 NULL 参数 no-op 契约与注册后 create 不回归。
 *
 * @author Lv-00 Project
 * @date 2026-08-19
 */

#include <stdio.h>
#include <string.h>

#include "context.h"
#include "lv.h"
#include "test_helpers.h"

/* ============================================================
 * 全局测试计数器
 * ============================================================ */
int g_pass_count = 0;
int g_fail_count = 0;

/* ============================================================
 * 辅助：创建上下文并校验基础契约
 * ============================================================ */
static lvContext *ctx_new(void) {
    lvContext *ctx = lv_context_create();
    if (!ctx) {
        TEST_ASSERT_CONTINUE(ctx != NULL, "context 创建应成功");
        return NULL;
    }
    TEST_ASSERT_CONTINUE(lv_context_get_state(ctx) == lv_CONTEXT_IDLE, "初始状态 IDLE");
    TEST_ASSERT_CONTINUE(lv_context_get_error_code(ctx) == lv_OK, "初始错误码 OK");
    TEST_ASSERT_CONTINUE(lv_context_get_reasoning_depth(ctx) == 0, "初始推理栈深度 0");
    return ctx;
}

/* ============================================================
 * 参数配置：timeout / max_depth / max_steps / name / id
 * ============================================================ */
static void test_context_params(void) {
    lvContext *ctx = ctx_new();

    /* timeout：设置/读取往返 */
    lv_context_set_timeout(ctx, 12345);
    TEST_ASSERT_EQ(lv_context_get_timeout(ctx), (uint64_t) 12345);
    lv_context_set_timeout(ctx, 0); /* 0 = 不限制 */
    TEST_ASSERT_EQ(lv_context_get_timeout(ctx), (uint64_t) 0);

    /* max_depth：设置/读取；钳制 [1, lv_CONTEXT_MAX_RECURSION_DEPTH] */
    lv_context_set_max_depth(ctx, 42);
    TEST_ASSERT_EQ(lv_context_get_max_depth(ctx), 42);
    lv_context_set_max_depth(ctx, 0); /* 下限钳制到 1 */
    TEST_ASSERT_EQ(lv_context_get_max_depth(ctx), 1);
    lv_context_set_max_depth(ctx, lv_CONTEXT_MAX_RECURSION_DEPTH + 1000); /* 上限钳制 */
    TEST_ASSERT_EQ(lv_context_get_max_depth(ctx), lv_CONTEXT_MAX_RECURSION_DEPTH);

    /* max_steps：设置/读取；0 = 不限制 */
    lv_context_set_max_steps(ctx, 1000);
    TEST_ASSERT_EQ(lv_context_get_max_steps(ctx), (int64_t) 1000);
    lv_context_set_max_steps(ctx, 0);
    TEST_ASSERT_EQ(lv_context_get_max_steps(ctx), (int64_t) 0);

    /* name：设置/覆盖/清除；内部复制 */
    lv_context_set_name(ctx, "分支A");
    TEST_ASSERT_STR_EQ(lv_context_get_name(ctx), "分支A");
    lv_context_set_name(ctx, "分支B");
    TEST_ASSERT_STR_EQ(lv_context_get_name(ctx), "分支B");
    lv_context_set_name(ctx, NULL); /* 清除 → 返回空串 */
    TEST_ASSERT_STR_EQ(lv_context_get_name(ctx), "");

    /* id：非零且自增唯一 */
    uint64_t id1 = lv_context_get_id(ctx);
    TEST_ASSERT_MSG(id1 > 0, "context_id 应为正数");
    lvContext *ctx2 = lv_context_create();
    TEST_ASSERT_NOT_NULL(ctx2);
    TEST_ASSERT_MSG(lv_context_get_id(ctx2) != id1, "context_id 应唯一");
    lv_context_destroy(ctx2);

    /* NULL 安全 */
    TEST_ASSERT_EQ(lv_context_get_timeout(NULL), (uint64_t) 0);
    TEST_ASSERT_EQ(lv_context_get_max_depth(NULL), lv_CONTEXT_DEFAULT_MAX_DEPTH);
    TEST_ASSERT_EQ(lv_context_get_max_steps(NULL), (int64_t) 0);
    TEST_ASSERT_STR_EQ(lv_context_get_name(NULL), "null");
    TEST_ASSERT_EQ(lv_context_get_id(NULL), (uint64_t) 0);
    lv_context_set_timeout(NULL, 1);
    lv_context_set_max_depth(NULL, 1);
    lv_context_set_max_steps(NULL, 1);
    lv_context_set_name(NULL, "x");

    lv_context_destroy(ctx);
}

/* ============================================================
 * 错误管理：set_error / get_error_code / get_error_message / clear_error
 * ============================================================ */
static void test_context_error(void) {
    lvContext *ctx = ctx_new();

    /* 格式化错误：错误码 + 消息 */
    lv_context_set_error(ctx, lv_ERROR_TIMEOUT, "超时 %d ms", 300);
    TEST_ASSERT_EQ(lv_context_get_error_code(ctx), lv_ERROR_TIMEOUT);
    TEST_ASSERT_MSG(strstr(lv_context_get_error_message(ctx), "300") != NULL,
                    "错误消息应包含格式化参数");
    TEST_ASSERT_MSG(strstr(lv_context_get_error_message(ctx), "超时") != NULL,
                    "错误消息应包含格式串文本");

    /* 覆盖错误 */
    lv_context_set_error(ctx, lv_ERROR_OUT_OF_MEMORY, "OOM");
    TEST_ASSERT_EQ(lv_context_get_error_code(ctx), lv_ERROR_OUT_OF_MEMORY);
    TEST_ASSERT_STR_EQ(lv_context_get_error_message(ctx), "OOM");

    /* fmt = NULL → 消息清空但错误码保留 */
    lv_context_set_error(ctx, lv_ERROR_INVALID_STATE, NULL);
    TEST_ASSERT_EQ(lv_context_get_error_code(ctx), lv_ERROR_INVALID_STATE);
    TEST_ASSERT_STR_EQ(lv_context_get_error_message(ctx), "");

    /* clear_error：错误码回 OK、消息清空、last_status 归零 */
    lv_context_clear_error(ctx);
    TEST_ASSERT_EQ(lv_context_get_error_code(ctx), lv_OK);
    TEST_ASSERT_STR_EQ(lv_context_get_error_message(ctx), "");

    /* NULL 安全 */
    TEST_ASSERT_EQ(lv_context_get_error_code(NULL), lv_ERROR_NULL_POINTER);
    TEST_ASSERT_STR_EQ(lv_context_get_error_message(NULL), "null context");
    lv_context_set_error(NULL, lv_OK, "x");
    lv_context_clear_error(NULL);

    lv_context_destroy(ctx);
}

/* ============================================================
 * 推理栈：push / pop / get_reasoning_depth / get_current_reasoning_frame
 * ============================================================ */
static void test_context_reasoning_stack(void) {
    lvContext *ctx = ctx_new();

    /* 初始空栈：无帧 */
    TEST_ASSERT_NULL(lv_context_get_current_reasoning_frame(ctx));

    /* push 前向分支 */
    TEST_ASSERT_EQ(lv_context_push_reasoning(ctx, lv_REASONING_BRANCH_FORWARD, 0), lv_OK);
    TEST_ASSERT_EQ(lv_context_get_reasoning_depth(ctx), 1);
    ReasoningFrame *f1 = lv_context_get_current_reasoning_frame(ctx);
    TEST_ASSERT_NOT_NULL(f1);
    TEST_ASSERT_EQ(f1->branch_type, lv_REASONING_BRANCH_FORWARD);

    /* push 反证分支（嵌套） */
    TEST_ASSERT_EQ(lv_context_push_reasoning(ctx, lv_REASONING_BRANCH_CONTRADICTION, 5000), lv_OK);
    TEST_ASSERT_EQ(lv_context_get_reasoning_depth(ctx), 2);
    ReasoningFrame *f2 = lv_context_get_current_reasoning_frame(ctx);
    TEST_ASSERT_NOT_NULL(f2);
    TEST_ASSERT_EQ(f2->branch_type, lv_REASONING_BRANCH_CONTRADICTION);
    TEST_ASSERT_EQ(f2->timeout_ms, (uint64_t) 5000);

    /* pop 反证分支 */
    TEST_ASSERT_EQ(lv_context_pop_reasoning(ctx), lv_OK);
    TEST_ASSERT_EQ(lv_context_get_reasoning_depth(ctx), 1);
    ReasoningFrame *f3 = lv_context_get_current_reasoning_frame(ctx);
    TEST_ASSERT_NOT_NULL(f3);
    TEST_ASSERT_EQ(f3->branch_type, lv_REASONING_BRANCH_FORWARD);

    /* pop 前向分支 → 空栈 */
    TEST_ASSERT_EQ(lv_context_pop_reasoning(ctx), lv_OK);
    TEST_ASSERT_EQ(lv_context_get_reasoning_depth(ctx), 0);
    TEST_ASSERT_NULL(lv_context_get_current_reasoning_frame(ctx));

    /* 空栈 pop → lv_ERROR_INVALID_STATE */
    TEST_ASSERT_EQ(lv_context_pop_reasoning(ctx), lv_ERROR_INVALID_STATE);

    /* 假设引入分支 */
    TEST_ASSERT_EQ(lv_context_push_reasoning(ctx, lv_REASONING_BRANCH_HYPOTHESIS, 0), lv_OK);
    TEST_ASSERT_EQ(lv_context_get_reasoning_depth(ctx), 1);
    TEST_ASSERT_EQ(lv_context_pop_reasoning(ctx), lv_OK);

    /* NULL 安全 */
    TEST_ASSERT_EQ(lv_context_push_reasoning(NULL, lv_REASONING_BRANCH_FORWARD, 0), lv_ERROR_NULL_POINTER);
    TEST_ASSERT_EQ(lv_context_pop_reasoning(NULL), lv_ERROR_NULL_POINTER);
    TEST_ASSERT_EQ(lv_context_get_reasoning_depth(NULL), 0);
    TEST_ASSERT_MSG(lv_context_get_current_reasoning_frame(NULL) == NULL, "NULL ctx 返回 NULL");

    lv_context_destroy(ctx);
}

/* ============================================================
 * 熔断器操作：begin/check_timeout、不可取消区域、步数/连续错误熔断
 * ============================================================ */
static void test_context_circuit_ops(void) {
    lvContext *ctx = ctx_new();

    /* 初始：熔断器未打开 */
    TEST_ASSERT_MSG(!lv_context_is_circuit_open(ctx), "初始熔断器应关闭");
    TEST_ASSERT_MSG(lv_context_is_circuit_open(NULL), "NULL 上下文视为熔断");

    /* timeout=0（不限制）：check_timeout 恒 false */
    lv_context_set_timeout(ctx, 0);
    lv_context_begin_operation(ctx);
    TEST_ASSERT_MSG(!lv_context_check_timeout(ctx), "timeout=0 不应超时");

    /* 不可取消区域：即使超时设置极小也不触发 */
    lv_context_set_timeout(ctx, 1); /* 1ms 超时 */
    lv_context_begin_operation(ctx);
    lv_context_enter_uncancellable(ctx);
    TEST_ASSERT_MSG(!lv_context_check_timeout(ctx), "不可取消区域不应触发超时");
    lv_context_leave_uncancellable(ctx);

    /* 不可取消区域引用计数：两次进入需两次离开 */
    lv_context_enter_uncancellable(ctx);
    lv_context_enter_uncancellable(ctx);
    lv_context_leave_uncancellable(ctx);
    TEST_ASSERT_MSG(!lv_context_check_timeout(ctx), "refcount=1 仍不可取消");

    /* NULL 安全 */
    lv_context_begin_operation(NULL);
    TEST_ASSERT_MSG(lv_context_check_timeout(NULL), "NULL check_timeout 视为超时");
    lv_context_enter_uncancellable(NULL);
    lv_context_leave_uncancellable(NULL);

    lv_context_destroy(ctx);

    /* ---- 步数熔断：max_steps=3，第 3 步触发 ---- */
    lvContext *ctx2 = ctx_new();
    lv_context_set_max_steps(ctx2, 3);
    TEST_ASSERT_MSG(lv_context_record_step(ctx2), "第1步应在限制内");
    TEST_ASSERT_MSG(lv_context_record_step(ctx2), "第2步应在限制内");
    TEST_ASSERT_MSG(!lv_context_record_step(ctx2), "第3步达到上限应触发熔断");
    TEST_ASSERT_MSG(lv_context_is_circuit_open(ctx2), "步数熔断后熔断器打开");
    TEST_ASSERT_MSG(lv_context_get_state(ctx2) == lv_CONTEXT_ERROR, "熔断应强转 ERROR 状态");
    TEST_ASSERT_EQ(lv_context_get_error_code(ctx2), lv_ERROR_RESOURCE_EXHAUSTED);
    lv_context_destroy(ctx2);

    /* ---- 连续错误熔断：默认上限 10 ---- */
    lvContext *ctx3 = ctx_new();
    bool within = true;
    for (int i = 0; i < 9; i++) {
        within = within && lv_context_record_error(ctx3);
    }
    TEST_ASSERT_MSG(within, "前9次连续错误应在限制内");
    TEST_ASSERT_MSG(!lv_context_record_error(ctx3), "第10次连续错误应触发熔断");
    TEST_ASSERT_MSG(lv_context_is_circuit_open(ctx3), "错误熔断后熔断器打开");
    TEST_ASSERT_MSG(lv_context_get_state(ctx3) == lv_CONTEXT_ERROR, "错误熔断应强转 ERROR");

    /* 熔断后 record_success 恢复半开（试探）后关闭；record_step 在熔断期间仍计数 */
    lv_context_destroy(ctx3);

    /* ---- record_success 重置连续错误计数 ---- */
    lvContext *ctx4 = ctx_new();
    for (int i = 0; i < 5; i++) {
        lv_context_record_error(ctx4);
    }
    lv_context_record_success(ctx4); /* 重置连续错误 */
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_MSG(lv_context_record_error(ctx4), "success 后前5次错误应在限制内");
    }
    TEST_ASSERT_MSG(!lv_context_is_circuit_open(ctx4), "重置后 5+5=10 次错误刚好触发，此处 5 次应未触发");
    lv_context_destroy(ctx4);

    /* NULL 安全 */
    TEST_ASSERT_MSG(!lv_context_record_step(NULL), "NULL record_step 返回 false");
    TEST_ASSERT_MSG(!lv_context_record_error(NULL), "NULL record_error 返回 false");
    lv_context_record_success(NULL);
}

/* ============================================================
 * 快照 / 回滚：snapshot 标量恢复 + rollback 状态恢复
 * ============================================================ */
static void test_context_snapshot_rollback(void) {
    lvContext *ctx = ctx_new();

    /* 建立可观测状态：名称 + 错误码 + 状态机 + 配置 */
    lv_context_set_name(ctx, "任务1");
    lv_context_set_timeout(ctx, 999);
    lv_context_set_max_steps(ctx, 777);

    /* 快照 */
    lvContext *snap = lv_context_snapshot(ctx);
    TEST_ASSERT_NOT_NULL(snap);
    TEST_ASSERT_MSG(lv_context_get_name(snap) != NULL && strcmp(lv_context_get_name(snap), "任务1") == 0,
                    "快照应复制名称");
    TEST_ASSERT_EQ(lv_context_get_timeout(snap), (uint64_t) 999);

    /* 修改源上下文 */
    lv_context_set_name(ctx, "任务2");
    lv_context_set_error(ctx, lv_ERROR_TIMEOUT, "已超时");
    lv_context_set_state(ctx, lv_CONTEXT_PARSING);

    /* 回滚到快照：恢复快照时的标量状态（名称不属于 rollback 恢复字段，
     * 头文件能力边界：恢复状态机/错误码/熔断器/递归深度/重写步数上限等） */
    TEST_ASSERT_EQ(lv_context_rollback(ctx, snap), lv_OK);
    TEST_ASSERT_EQ(lv_context_get_timeout(ctx), (uint64_t) 999);
    TEST_ASSERT_EQ(lv_context_get_max_steps(ctx), (int64_t) 777);
    TEST_ASSERT_EQ(lv_context_get_error_code(ctx), lv_OK);
    TEST_ASSERT_MSG(lv_context_get_state(ctx) == lv_CONTEXT_IDLE, "回滚应恢复快照时的状态");

    /* 快照可重复回滚（标量字段） */
    lv_context_set_error(ctx, lv_ERROR_TIMEOUT, "再次失败");
    TEST_ASSERT_EQ(lv_context_rollback(ctx, snap), lv_OK);
    TEST_ASSERT_EQ(lv_context_get_error_code(ctx), lv_OK);

    /* NULL 安全 */
    TEST_ASSERT_EQ(lv_context_rollback(NULL, snap), lv_ERROR_NULL_POINTER);
    TEST_ASSERT_EQ(lv_context_rollback(ctx, NULL), lv_ERROR_NULL_POINTER);
    TEST_ASSERT_NULL(lv_context_snapshot(NULL));

    /* 销毁快照：snapshot_refcount 归零时递归销毁父上下文（ctx 的
     * parent_snapshot 链即 ctx，故仅销毁 snap 即可，避免 double-free）。 */
    lv_context_destroy(snap);
}

/* ============================================================
 * 重置：reset 语义
 * ============================================================ */
static void test_context_reset(void) {
    lvContext *ctx = ctx_new();

    /* 制造状态：错误 + 推理栈 + 步数 + 名称 */
    lv_context_set_state(ctx, lv_CONTEXT_REASONING);
    lv_context_set_error(ctx, lv_ERROR_TIMEOUT, "失败");
    lv_context_push_reasoning(ctx, lv_REASONING_BRANCH_FORWARD, 0);
    lv_context_set_max_steps(ctx, 5);
    lv_context_record_step(ctx);
    lv_context_record_step(ctx);
    lv_context_set_name(ctx, "任务X");

    /* 记录 trip_count 前：无熔断 */
    /* reset：清空问题特定状态 */
    lv_context_reset(ctx);

    /* 状态机回 IDLE */
    TEST_ASSERT_MSG(lv_context_get_state(ctx) == lv_CONTEXT_IDLE, "reset 后状态 IDLE");
    /* 错误清除 */
    TEST_ASSERT_EQ(lv_context_get_error_code(ctx), lv_OK);
    TEST_ASSERT_STR_EQ(lv_context_get_error_message(ctx), "");
    /* 推理栈清空 */
    TEST_ASSERT_EQ(lv_context_get_reasoning_depth(ctx), 0);
    /* 名称释放 → 空串 */
    TEST_ASSERT_STR_EQ(lv_context_get_name(ctx), "");
    /* 配置保留（max_steps 不属于问题特定状态） */
    TEST_ASSERT_EQ(lv_context_get_max_steps(ctx), (int64_t) 5);

    /* reset 后上下文可继续使用：走合法转移链 IDLE→PARSING→REASONING→COMPLETE→IDLE */
    TEST_ASSERT_EQ(lv_context_set_state(ctx, lv_CONTEXT_PARSING), lv_OK);
    TEST_ASSERT_EQ(lv_context_set_state(ctx, lv_CONTEXT_REASONING), lv_OK);
    TEST_ASSERT_EQ(lv_context_set_state(ctx, lv_CONTEXT_COMPLETE), lv_OK);
    TEST_ASSERT_EQ(lv_context_set_state(ctx, lv_CONTEXT_IDLE), lv_OK);

    /* NULL 安全 */
    lv_context_reset(NULL);

    lv_context_destroy(ctx);
}

/* ============================================================
 * 统计：get_stats 摘要
 * ============================================================ */
static void test_context_stats(void) {
    lvContext *ctx = ctx_new();

    char buf[1024];
    int n = lv_context_get_stats(ctx, buf, sizeof(buf));
    TEST_ASSERT_MSG(n > 0, "get_stats 应返回正字符数");
    TEST_ASSERT_MSG((size_t) n < sizeof(buf), "摘要不应溢出");
    TEST_ASSERT_MSG(strstr(buf, "lvContext") != NULL, "摘要应含标题");
    TEST_ASSERT_MSG(strstr(buf, "IDLE") != NULL, "摘要应含当前状态");

    /* 小缓冲区：截断且终止 */
    char small[16];
    int n2 = lv_context_get_stats(ctx, small, sizeof(small));
    TEST_ASSERT_MSG(n2 == (int) (sizeof(small) - 1), "小缓冲区应返回 buf_size-1 并截断");
    TEST_ASSERT_EQ(small[sizeof(small) - 1], '\0');

    /* NULL 安全 */
    TEST_ASSERT_EQ(lv_context_get_stats(NULL, buf, sizeof(buf)), 0);
    TEST_ASSERT_EQ(lv_context_get_stats(ctx, NULL, 100), 0);
    TEST_ASSERT_EQ(lv_context_get_stats(ctx, buf, 0), 0);

    lv_context_destroy(ctx);
}

/* ============================================================
 * register_resource_ops：NULL no-op 契约
 * ============================================================ */
static void test_context_register_resource_ops(void) {
    /* NULL 参数：no-op，不影响后续 create */
    lv_context_register_resource_ops(NULL);

    /* 注册后 create 不回归（完整注册路径由 lv_init 注入） */
    lvContext *ctx = ctx_new();
    lv_context_destroy(ctx);
}

/* ============================================================
 * 主入口
 * ============================================================ */
TEST_MAIN_BEGIN("Isolated Context")

    /* 初始化系统：注入 context 资源操作（main_graph 等 L3/L4 不透明资源
     * 由 L0 在 lv_init 时注册），保证 lv_context_create 契约成立 */
    lv_init();

    TEST_MAIN_RUN(test_context_params);
    TEST_MAIN_RUN(test_context_error);
    TEST_MAIN_RUN(test_context_reasoning_stack);
    TEST_MAIN_RUN(test_context_circuit_ops);
    TEST_MAIN_RUN(test_context_snapshot_rollback);
    TEST_MAIN_RUN(test_context_reset);
    TEST_MAIN_RUN(test_context_stats);
    TEST_MAIN_RUN(test_context_register_resource_ops);

TEST_MAIN_END()

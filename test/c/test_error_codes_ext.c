/**
 * @file test_error_codes_ext.c
 * @brief 错误码工具契约测试（批次 C-㊺续29：error_codes.h 4 个零覆盖 API）
 *
 * 覆盖：error_category / error_code_from_string / error_is_unknown /
 *   get_error_description
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/error_codes.h"
#include "lv/status_codes.h"

int g_pass_count = 0;
int g_fail_count = 0;

static void test_error_codes_api(void) {
    /* error_category：常见码有类别 */
    const char *cat = lv_error_category(lv_ERROR_UNKNOWN);
    TEST_ASSERT_NOT_NULL(cat);
    cat = lv_error_category(lv_ERROR_INVALID_PARAM);
    TEST_ASSERT_NOT_NULL(cat);
    TEST_ASSERT(strlen(cat) > 0, "类别非空");

    /* error_is_unknown */
    TEST_ASSERT(!lv_error_is_unknown(lv_ERROR_UNKNOWN), "已知码非未知");
    TEST_ASSERT(!lv_error_is_unknown(lv_ERROR_INVALID_PARAM), "已知码非未知");
    TEST_ASSERT(lv_error_is_unknown((lvErrorCode) 99999), "未知名码为未知");

    /* error_code_from_string：往返 */
    lvErrorCode code = lv_error_code_from_string("lv_ERROR_INVALID_PARAM");
    TEST_ASSERT_EQ((int) code, (int) lv_ERROR_INVALID_PARAM);
    /* NULL 或未知名 → 默认 */
    lvErrorCode unknown = lv_error_code_from_string("no_such_code");
    TEST_ASSERT_EQ((int) unknown, (int) lv_ERROR_UNKNOWN);

    /* get_error_description：缓冲写描述 */
    char buf[256];
    /* 先设置一个错误 */
    lv_set_error(lv_ERROR_INVALID_PARAM, "测试错误");
    int n = lv_get_error_description(buf, sizeof(buf));
    TEST_ASSERT(n >= 0, "描述长度非负");
    TEST_ASSERT(strlen(buf) > 0, "描述非空");
    TEST_ASSERT_EQ(lv_get_error_description(NULL, 10), -1);
    TEST_ASSERT_EQ(lv_get_error_description(buf, 0), -1);

    printf("  test_error_codes_api: PASSED\n");
}

static void test_set_error_ctx_api(void) {
    /* lv_set_error_ctx：设置线程局部错误上下文 */
    lv_set_error_ctx(lv_ERROR_NOT_FOUND, __FILE__, __LINE__, __func__, "ctx msg %d", 42);
    TEST_ASSERT_EQ((int) lv_get_last_error_code(), (int) lv_ERROR_NOT_FOUND);
    const char *msg = lv_get_last_error_message();
    TEST_ASSERT_NOT_NULL(msg);
    TEST_ASSERT(strstr(msg, "ctx msg 42") != NULL, "formatted message");
}

static void test_status_category_api(void) {
    /* lv_status_category（error_codes.h 声明，status_codes.c 实现） */
    const char *cat = lv_status_category(0);
    TEST_ASSERT_NOT_NULL(cat);
    TEST_ASSERT(strlen(cat) > 0, "category non-empty");

    /* 负码（警告区间） */
    cat = lv_status_category(-1);
    TEST_ASSERT_NOT_NULL(cat);

    /* 未知范围 */
    cat = lv_status_category(999999);
    TEST_ASSERT_NOT_NULL(cat);
    TEST_ASSERT(strcmp(cat, "未分类") == 0, "unknown category");
}

/* ========== 蓝图错误消息 API（TEN_LAYER_OPTIMIZED_PLAN §4.1.6 落地，批次 G1a） ========== */

static void test_blueprint_error_message_api(void) {
    /* lv_error_category_name / _cn：枚举 ↔ 名字 */
    TEST_ASSERT_NOT_NULL(lv_error_category_name(LV_CAT_OK));
    TEST_ASSERT(strcmp(lv_error_category_name(LV_CAT_OK), "OK") == 0, "英文类别名 OK");
    TEST_ASSERT(strcmp(lv_error_category_name(LV_CAT_SYSTEM), "SYSTEM") == 0, "英文类别名 SYSTEM");
    TEST_ASSERT_NOT_NULL(lv_error_category_name_cn(LV_CAT_SYSTEM));
    TEST_ASSERT(strcmp(lv_error_category_name_cn(LV_CAT_SYSTEM), "通用系统错误") == 0, "中文类别长名");
    /* 越界 → NULL */
    TEST_ASSERT_NULL(lv_error_category_name((lvErrorCategory) -1));
    TEST_ASSERT_NULL(lv_error_category_name((lvErrorCategory) lv_ERROR_CATEGORY_COUNT));
    TEST_ASSERT_NULL(lv_error_category_name_cn((lvErrorCategory) -1));

    /* lv_get_error_message：编译期表回退 */
    const lvErrorMessage *m = lv_get_error_message((int) lv_ERROR_INVALID_PARAM);
    TEST_ASSERT_NOT_NULL(m);
    TEST_ASSERT_EQ(m->code, (int) lv_ERROR_INVALID_PARAM);
    TEST_ASSERT_NOT_NULL(m->message_cn);
    TEST_ASSERT(strlen(m->message_cn) > 0, "中文消息非空");
    /* 未知码 → NULL */
    TEST_ASSERT_NULL(lv_get_error_message(99999));

    /* lv_format_error：格式化输出 */
    char buf[256];
    int n = lv_format_error(buf, sizeof(buf), (int) lv_ERROR_INVALID_PARAM, "test ctx");
    TEST_ASSERT(n > 0, "格式长度正");
    TEST_ASSERT(strstr(buf, "test ctx") != NULL, "包含上下文");
    TEST_ASSERT(strstr(buf, "INVALID_PARAM") != NULL, "包含错误名");
    TEST_ASSERT_EQ(lv_format_error(NULL, 0, (int) lv_ERROR_INVALID_PARAM, NULL), -1);

    /* lv_error_code_count：>= 编译期表大小（lv_error_table_size 声明于 lv_internal.h，此处以计数自检） */
    TEST_ASSERT(lv_error_code_count() > 0, "错误消息计数非零");
}

static void test_blueprint_error_register_api(void) {
    /* 动态注册 → 覆盖查询 → 注销 */
    lvErrorMessageRegistration reg;
    memset(&reg, 0, sizeof(reg));
    reg.code = 4242;
    reg.category = LV_CAT_SYSTEM;
    reg.message = "CUSTOM_EN";
    reg.message_cn = "自定义错误";
    reg.suggestion = "修复建议";
    TEST_ASSERT(lv_register_error_message(&reg), "注册成功");

    const lvErrorMessage *m = lv_get_error_message(4242);
    TEST_ASSERT_NOT_NULL(m);
    TEST_ASSERT_EQ(m->code, 4242);
    TEST_ASSERT(strcmp(m->message, "CUSTOM_EN") == 0, "英文消息");
    TEST_ASSERT(strcmp(m->message_cn, "自定义错误") == 0, "中文消息");
    TEST_ASSERT(strcmp(m->suggestion, "修复建议") == 0, "建议");
    TEST_ASSERT(lv_error_code_count() > 0, "计数非零");
    /* base_count 在注册后取（含 1 个动态项）；注销后应为 base_count - 1 */
    int base_count = lv_error_code_count();

    /* 同码覆盖 */
    reg.message = "CUSTOM_EN2";
    TEST_ASSERT(lv_register_error_message(&reg), "覆盖注册成功");
    m = lv_get_error_message(4242);
    TEST_ASSERT(strcmp(m->message, "CUSTOM_EN2") == 0, "覆盖后英文消息");

    /* 参数校验 */
    TEST_ASSERT(!lv_register_error_message(NULL), "NULL 注册拒绝");
    reg.message = NULL;
    TEST_ASSERT(!lv_register_error_message(&reg), "缺消息拒绝");

    /* 注销 */
    TEST_ASSERT(lv_unregister_error_message(4242), "注销成功");
    TEST_ASSERT_NULL(lv_get_error_message(4242));
    TEST_ASSERT(!lv_unregister_error_message(4242), "重复注销失败");
    TEST_ASSERT(!lv_unregister_error_message(99999), "未注册码注销失败");
    TEST_ASSERT(lv_error_code_count() == base_count - 1, "注销后计数复原");
}

TEST_MAIN_BEGIN("Lv-00 Error Codes Ext Test Suite")
    printf("=== Lv-00 Error Codes Ext Test Suite (batch C-㊺续29 + G1a 蓝图错误消息) ===\n\n");
    lv_init();
    TEST_MAIN_RUN(test_error_codes_api);
    TEST_MAIN_RUN(test_set_error_ctx_api);
    TEST_MAIN_RUN(test_status_category_api);
    TEST_MAIN_RUN(test_blueprint_error_message_api);
    TEST_MAIN_RUN(test_blueprint_error_register_api);
    lv_cleanup();
TEST_MAIN_END()

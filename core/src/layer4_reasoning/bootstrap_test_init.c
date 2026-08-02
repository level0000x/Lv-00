/**
 * @file bootstrap_test_init.c
 * @brief Lv-00 自举差分测试框架 —— 框架初始化与全局状态
 *
 * @details 由 bootstrap_test.c 按功能组件拆分而来。
 *          共享兼容定义与框架状态见 bootstrap_test_internal.h。
 *
 * @author Lv-00 Project
 * @version 1.0.0
 */

#include "lv/bootstrap_test.h"
#include "lv/lv_log.h"

#include "lv/lv_file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lv/constraint_graph.h"
#include "lv/cross_platform.h"
#include "lv/engine.h"
#include "lv/lv.h"
#include "lv/lv_utils.h"
#include "lv/proof_trace.h"
#include "lv/lv_internal.h"

#include "bootstrap_test_internal.h"

/* ============== 内部状态（typedef 见 bootstrap_test_internal.h） ============== */

/** 模块级唯一状态实例 */
BootstrapTestState s_test_state = {0};

/* ============== 框架初始化 ============== */

/**
 * @brief 初始化自举测试框架
 *
 * 初始化 Lv-00 核心系统和原语包装器。
 * 可重复调用（幂等），第二次调用直接返回 true。
 *
 * @return true 初始化成功，false 失败
 */
bool bootstrap_test_framework_init(void) {
    if (s_test_state.initialized) {
        return true;
    }

    /* 初始化 Lv-00 核心系统 */
    if (!lv_init()) {
        lv_ERROR("[BootstrapTest] Failed to initialize Lv-00 core");
        return false;
    }

    /* 初始化原语包装器 */
    if (!primitive_wrapper_init()) {
        lv_ERROR("[BootstrapTest] Failed to initialize primitive wrapper");
        lv_cleanup();
        return false;
    }

    s_test_state.initialized = true;
    s_test_state.test_count = 0;
    s_test_state.pass_count = 0;
    s_test_state.fail_count = 0;

    lv_INFO("[BootstrapTest] Framework initialized successfully");
    return true;
}

/**
 * @brief 清理自举测试框架
 *
 * 清理原语包装器和 Lv-00 核心系统。
 * 幂等函数，多次调用安全。
 */
void lv_bootstrap_test_framework_cleanup(void) {
    if (!s_test_state.initialized) {
        return;
    }

    lv_primitive_wrapper_cleanup();
    lv_cleanup();

    s_test_state.initialized = false;
    lv_INFO("[BootstrapTest] Framework cleaned up");
}

/**
 * @brief 检查框架是否已初始化
 *
 * @return true 已初始化，false 未初始化
 */
bool bootstrap_test_framework_is_initialized(void) {
    return s_test_state.initialized;
}


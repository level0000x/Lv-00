/**
 * @file smt_backend_impl_external.c
 * @brief 外部求解器调用
 *
 * @details 从 smt_backend_impl.c 拆分的子模块（Lv-00 项目 v3.3.0+）。
 *          通过统一外部进程执行器 lv_external_process_run 调用外部 SMT 求解器
 *          （z3 / cvc5 / singular），SMT-LIB2 输入经 stdin 管道传递，
 *          不再依赖临时文件路径。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_str_utils.h"

#include "lv/smt_backend.h"
#include "smt_backend_internal.h"
#include "lv/lv_process.h"
#include "lv/lv_registry.h"
#include "lv/lv_thread.h"

#include "lv/error_codes.h"
#include "lv/groebner_engine.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"

/* ============================================================
 * 外部求解器子进程辅助函数
 * ============================================================ */

/**
 * @brief 通过子进程调用外部 SMT 求解器
 *
 * 将 SMT-LIB2 输入经 stdin 管道传递给求解器，调用指定求解器可执行文件，
 * 读取其标准输出并解析 sat/unsat/unknown 结果。
 *
 * @param[in]  solver       求解器句柄（用于错误报告）
 * @param[in]  executable   求解器可执行文件名（如 "z3" 或 "cvc5"）
 * @param[in]  smt2_input   SMT-LIB2 格式的输入文本
 * @param[in]  smt2_len     输入文本长度
 * @param[out] result_buf   可选：存储求解器原始输出
 * @param[in]  result_size  result_buf 缓冲区大小
 * @return SMTSatResult 求解结果
 */
SMTSatResult smt_external_solver_check(SMTSolver *solver, const char *executable, const char *smt2_input, int smt2_len,
                                       char *result_buf, int result_size) {
    if (!smt2_input || smt2_len <= 0) {
        return SMT_RESULT_UNKNOWN;
    }

    /* 构造 argv；输入经 stdin 管道传递，不再依赖临时文件路径：
     * - z3:      -in 表示读取 stdin
     * - cvc5:    --lang smt2，无文件参数时读取 stdin
     * - 其他:    无文件参数时读取 stdin（如 singular） */
    char *exec_argv[4];
    int argc = 0;
    exec_argv[argc++] = (char *) executable;
    if (lv_str_eq(executable, "z3")) {
        exec_argv[argc++] = "-in";
    } else if (lv_str_eq(executable, "cvc5")) {
        exec_argv[argc++] = "--lang";
        exec_argv[argc++] = "smt2";
    }
    exec_argv[argc] = NULL;

    /* 超时优先使用 solver->config.timeout_ms（求解器配置，默认 5000ms），
     * 若未配置（<=0）则回退默认 30s，与 ATP 后端默认超时（ATP_DEFAULT_TIMEOUT=30s）一致；
     * 超时强杀时 lv_external_process_run 返回 lv_OK 且 exit_code == -1，
     * 与"退出码非 0 → UNKNOWN"的降级路径衔接。 */
    const int k_default_timeout_ms = 30000;
    int timeout_ms = (solver && solver->config.timeout_ms > 0) ? solver->config.timeout_ms : k_default_timeout_ms;

    char *output = NULL;
    size_t out_len = 0;
    int exit_code = -1;
    int rc = lv_external_process_run(executable, exec_argv, smt2_input, (size_t) smt2_len, timeout_ms, &output, &out_len,
                                     &exit_code);

    if (rc != (int) lv_OK) {
        lv_LOG_WARNING("外部求解器 %s: 子进程执行失败 (error=%d)，回退到 UNKNOWN", executable, rc);
        return SMT_RESULT_UNKNOWN;
    }

    /* 将原始输出复制到 result_buf（如果调用者需要） */
    if (result_buf && result_size > 0) {
        lv_strlcpy(result_buf, output ? output : "", result_size);
    }

    if (exit_code != 0) {
        lv_LOG_WARNING("外部求解器 %s: 进程退出码=%d，回退到 UNKNOWN", executable, exit_code);
        lv_free((void **) &output);
        return SMT_RESULT_UNKNOWN;
    }

    if (!output) {
        lv_LOG_WARNING("外部求解器 %s: 无输出，回退到 UNKNOWN", executable);
        return SMT_RESULT_UNKNOWN;
    }

    /* 解析求解器输出 */
    /* 去除首尾空白 */
    char *trimmed = lv_str_trim(output);

    SMTSatResult result;
    if (lv_str_startswith(trimmed, "sat")) {
        result = SMT_RESULT_SAT;
        lv_LOG_INFO("外部求解器 %s: 结果 = SAT", executable);
    } else if (lv_str_startswith(trimmed, "unsat")) {
        result = SMT_RESULT_UNSAT;
        lv_LOG_INFO("外部求解器 %s: 结果 = UNSAT", executable);
    } else if (lv_str_startswith(trimmed, "unknown")) {
        result = SMT_RESULT_UNKNOWN;
        lv_LOG_INFO("外部求解器 %s: 结果 = UNKNOWN", executable);
    } else {
        result = SMT_RESULT_UNKNOWN;
        lv_LOG_WARNING("外部求解器 %s: 无法解析输出 \"%s\"，回退到 UNKNOWN", executable, trimmed);
    }

    lv_free((void **) &output);
    return result;
}

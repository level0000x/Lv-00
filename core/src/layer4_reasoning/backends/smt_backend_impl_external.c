/**
 * @file smt_backend_impl_external.c
 * @brief 外部求解器调用
 *
 * @details 从 smt_backend_impl.c 拆分的子模块（Lv-00 项目 v3.3.0+）。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_file.h"
#include "lv/lv_str_utils.h"

#include "smt_backend.h"
#include "smt_backend_internal.h"
#include "lv/lv_registry.h"
#include "lv/lv_thread.h"

#include "error_codes.h"
#include "groebner_engine.h"
#include "lv_internal.h"
#include "lv_utils.h"

/* ============================================================
 * 外部求解器子进程辅助函数
 * ============================================================ */

/**
 * @brief 通过子进程调用外部 SMT 求解器
 *
 * 将 SMT-LIB2 输入写入临时文件，调用指定求解器可执行文件，
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

    /* 写入临时文件 */
    FILE *tmp = tmpfile();
    if (!tmp) {
        lv_LOG_WARNING("外部求解器 %s: 无法创建临时文件，回退到 UNKNOWN", executable);
        return SMT_RESULT_UNKNOWN;
    }
    fputs(smt2_input, tmp);
    fflush(tmp);

    /* 获取临时文件的文件描述符/句柄 */
#ifdef _WIN32
    long fd = _fileno(tmp);
    /* 在 Windows 上获取临时文件路径 */
    char tmp_path[MAX_PATH];
    if (_get_osfhandle(fd) == -1 || tmpnam_s(tmp_path, MAX_PATH) != 0) {
        lv_file_close(tmp);
        lv_LOG_WARNING("外部求解器 %s: 无法获取临时文件路径，回退到 UNKNOWN", executable);
        return SMT_RESULT_UNKNOWN;
    }
    /* 将 tmpfile 内容复制到命名临时文件 */
    FILE *named_tmp = lv_file_open(tmp_path, "w");
    if (!named_tmp) {
        lv_file_close(tmp);
        lv_LOG_WARNING("外部求解器 %s: 无法创建命名临时文件，回退到 UNKNOWN", executable);
        return SMT_RESULT_UNKNOWN;
    }
    rewind(tmp);
    char copy_buf[4096];
    size_t n;
    while ((n = fread(copy_buf, 1, sizeof(copy_buf), tmp)) > 0) {
        size_t written = fwrite(copy_buf, 1, n, named_tmp);
        if (written != n) {
            lv_LOG_WARNING("外部求解器 %s: 临时文件写入不完整（期望 %zu, 实际 %zu）", executable, n, written);
            break;
        }
    }
    lv_file_close(named_tmp);
    lv_file_close(tmp);

    /* 构造命令行 */
    char cmd[1024];
    if (strcmp(executable, "z3") == 0) {
        snprintf(cmd, sizeof(cmd), "z3 -in \"%s\" 2>NUL", tmp_path);
    } else if (strcmp(executable, "cvc5") == 0) {
        snprintf(cmd, sizeof(cmd), "cvc5 --lang smt2 \"%s\" 2>NUL", tmp_path);
    } else {
        snprintf(cmd, sizeof(cmd), "\"%s\" \"%s\" 2>NUL", executable, tmp_path);
    }
#else
    char tmp_path[64];
    snprintf(tmp_path, sizeof(tmp_path), "/dev/fd/%d", fileno(tmp));

    /* 构造命令行 */
    char cmd[1024];
    if (strcmp(executable, "z3") == 0) {
        snprintf(cmd, sizeof(cmd), "%s -in %s 2>/dev/null", executable, tmp_path);
    } else if (strcmp(executable, "cvc5") == 0) {
        snprintf(cmd, sizeof(cmd), "%s --lang smt2 %s 2>/dev/null", executable, tmp_path);
    } else {
        snprintf(cmd, sizeof(cmd), "%s %s 2>/dev/null", executable, tmp_path);
    }
#endif

    lv_LOG_INFO("外部求解器 %s: 启动子进程: %s", executable, cmd);

    /* 通过 popen 启动子进程 */
    FILE *pipe = lv_popen(cmd, "r");
    if (!pipe) {
        lv_LOG_WARNING("外部求解器 %s: popen 失败（求解器可能未安装），回退到 UNKNOWN", executable);
#ifdef _WIN32
        _unlink(tmp_path);
#else
        lv_file_close(tmp);
#endif
        return SMT_RESULT_UNKNOWN;
    }

    /* 读取求解器输出 */
    char output_buf[4096] = {0};
    size_t total_read = 0;
    size_t chunk;
    while ((chunk = fread(output_buf + total_read, 1, sizeof(output_buf) - total_read - 1, pipe)) > 0) {
        total_read += chunk;
    }
    output_buf[total_read] = '\0';

    int status = lv_pclose(pipe);

#ifdef _WIN32
    _unlink(tmp_path);
#else
    lv_file_close(tmp);
#endif

    /* 将原始输出复制到 result_buf（如果调用者需要） */
    if (result_buf && result_size > 0) {
        snprintf(result_buf, result_size, "%s", output_buf);
    }

    /* 检查进程退出状态 */
#ifndef _WIN32
    if (WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);
        if (exit_code != 0) {
            lv_LOG_WARNING("外部求解器 %s: 进程退出码=%d，回退到 UNKNOWN", executable, exit_code);
            return SMT_RESULT_UNKNOWN;
        }
    }
#else
    if (status != 0) {
        lv_LOG_WARNING("外部求解器 %s: 进程退出码=%d，回退到 UNKNOWN", executable, status);
        return SMT_RESULT_UNKNOWN;
    }
#endif

    /* 解析求解器输出 */
    /* 去除首尾空白 */
    char *trimmed = output_buf;
    while (*trimmed == ' ' || *trimmed == '\t' || *trimmed == '\r' || *trimmed == '\n')
        trimmed++;
    char *end = trimmed + strlen(trimmed) - 1;
    while (end > trimmed && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n'))
        *end-- = '\0';

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

    return result;
}


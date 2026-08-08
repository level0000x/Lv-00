/**
 * @file lv_process.h
 * @brief 统一外部进程执行器 —— 跨平台子进程调用基础设施
 *
 * @details 收敛 Layer 4 各后端（ATP/SMT）中重复的外部进程调用逻辑：
 *          - Windows: CreateProcessA + 匿名管道（stdin/stdout）
 *          - POSIX:   fork + execvp + pipe + dup2
 *          统一提供：stdin 管道输入、stdout 动态缓冲捕获、超时强杀、
 *          退出码跨平台归一、失败路径完整清理。
 *
 *          【后续收敛定位】调用方 atp_backend.c（atp_run_subprocess）与
 *          smt_backend_impl_external.c（smt_external_solver_check）仍各自实现
 *          "argv 构造（按可执行名硬编码 / 按 extra_args 空格切分）→ 超时来源
 *          （秒转毫秒 / solver->config.timeout_ms）→ 输出解析（SZS 状态 /
 *          sat-unsat 前缀）→ 失败降级（返回 rc / 降级 UNKNOWN）"骨架，
 *          与本执行器是调用关系而非统一入口。若未来抽象 lvExternalRunner
 *          （封装"构造 argv → 执行 → 捕获输出 → 超时 → 退出码降级"骨架），
 *          应以此为底座；收敛前保持各调用方现状。
 *
 *          实现位于 core/src/layer2_resource/lv_process.c。
 *
 * @author Lv-00 Project
 * @version 1.0.0
 */
#ifndef lv_PROCESS_H
#define lv_PROCESS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

/**
 * @brief 运行外部进程并捕获其标准输出（stderr 合并到 stdout）
 *
 * 统一 Windows（CreateProcessA + 管道）与 POSIX（fork/execvp + pipe/dup2）实现：
 * - 输入文本经 stdin 管道传递给子进程（不依赖临时文件路径）
 * - stdout+stderr 合并捕获到动态缓冲区
 * - 支持超时强杀：timeout_ms > 0 时超时后强制终止子进程
 * - 退出码跨平台归一：正常退出返回进程退出码；被超时强杀或异常终止返回 -1
 *
 * @param[in]  exe         可执行文件路径或名称（按系统 PATH 规则查找）
 * @param[in]  argv        参数列表（argv[0] 应为 exe，以 NULL 结尾）
 * @param[in]  input_text  通过 stdin 传递给子进程的输入文本（可为 NULL）
 * @param[in]  input_len   input_text 长度（input_text 为 NULL 时按 0 处理）
 * @param[in]  timeout_ms  超时毫秒数；<= 0 表示无超时
 * @param[out] out_stdout  捕获的输出（lv_malloc 分配、NUL 结尾；调用者 lv_free）
 * @param[out] out_len     输出字节数（不含结尾 NUL）
 * @param[out] exit_code   进程退出码（超时强杀/异常终止为 -1）
 * @return lv_OK 成功（含超时强杀：此时 *exit_code == -1）；
 *         其他 lv_ERROR_* 错误码（lv_ERROR_NULL_POINTER /
 *         lv_ERROR_INVALID_PARAM / lv_ERROR_IO / lv_ERROR_OUT_OF_MEMORY）
 */
int lv_external_process_run(const char *exe, char *const argv[], const char *input_text, size_t input_len,
                            int timeout_ms, char **out_stdout, size_t *out_len, int *exit_code);

/**
 * @brief 检测可执行文件是否可用（PATH 搜索，不实际执行目标程序）
 *
 * Windows: SearchPathA 查找（自动尝试 .exe 扩展名）；
 * POSIX:   遍历 PATH 检查 access(X_OK)。
 *
 * @param[in] name 可执行文件名（如 "z3"）或路径
 * @return true 可用，false 不可用（含 NULL/空字符串）
 */
bool lv_external_process_available(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* lv_PROCESS_H */

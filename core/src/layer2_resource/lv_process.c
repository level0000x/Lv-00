/**
 * @file lv_process.c
 * @brief 统一外部进程执行器实现
 *
 * @details 收敛 Layer 4 各后端（ATP/SMT）中重复的外部进程调用逻辑：
 *          - Windows: CreateProcessA + 匿名管道（stdin/stdout）
 *          - POSIX:   fork + execvp + pipe + dup2
 *          统一提供：stdin 管道输入、stdout 动态缓冲捕获、超时强杀、
 *          退出码跨平台归一、失败路径完整清理。
 *
 *          返回语义：
 *          - 进程成功启动并完成生命周期（含超时强杀）→ 返回 lv_OK，
 *            超时强杀/异常终止时 *exit_code == -1（与 atp_run_subprocess
 *            原语义一致，调用方据此区分超时与正常退出）
 *          - 启动/管道/内存失败 → 返回 lv_ERROR_*，*out_stdout == NULL
 *
 * @author Lv-00 Project
 * @version 1.0.0
 */

#include "lv/lv_platform.h"   /* 必须最先包含（功能测试宏） */

#include "lv/lv_process.h"
#include "lv/lv_strbuf.h"     /* lvStrBuf 动态输出缓冲（SSO+倍增，收敛 lv_out_buf） */
#include "lv_internal.h"      /* lv_LOG_* / lv_CHECK_* / 错误码 / lv_malloc 等 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef _WIN32
#include <signal.h>
#endif

/* ============================================================
 * 动态输出缓冲
 *
 * 直接复用 lvStrBuf（SSO+倍增扩容、始终 NUL 结尾），替代原手写
 * lv_out_buf（data/len/cap + 倍增 realloc + memcpy + '\0'）——两者
 * 扩容策略与输出字节语义一致。
 * ============================================================ */

/** @brief 成功收尾：保证 out_stdout 始终为非 NULL 的 NUL 结尾缓冲（lvStrBuf 恒 NUL 结尾） */
static int lv_process_finalize(lvStrBuf *ob, char **out_stdout, size_t *out_len, int *exit_code, int exit_code_v) {
    *out_stdout = ob->data;
    *out_len = ob->len;
    *exit_code = exit_code_v;
    return (int) lv_OK;
}

/* ============================================================
 * POSIX 实现：fork + execvp + pipe + dup2
 * ============================================================ */

#ifndef _WIN32

/** @brief 写入管道且不触发 SIGPIPE（子进程可能提前关闭 stdin 读端） */
static ssize_t lv_write_nosigpipe(int fd, const void *buf, size_t n) {
#ifdef SIGPIPE
    struct sigaction old_sa;
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, &old_sa);
    ssize_t r = write(fd, buf, n);
    sigaction(SIGPIPE, &old_sa, NULL);
    return r;
#else
    return write(fd, buf, n);
#endif
}

static int lv_run_posix(const char *exe, char *const argv[], const char *input_text, size_t input_len, int timeout_ms,
                        char **out_stdout, size_t *out_len, int *exit_code) {
    int stdin_pipe[2] = {-1, -1};
    int stdout_pipe[2] = {-1, -1};
    pid_t pid = -1;
    int rc = (int) lv_OK;
    bool has_input = (input_text != NULL && input_len > 0);
    bool timed_out = false;
    int exit_code_v = -1;
    size_t in_off = 0;
    lvStrBuf ob;
    lv_strbuf_init(&ob);

    if (pipe(stdin_pipe) != 0) {
        rc = (int) lv_ERROR_IO;
        goto cleanup;
    }
    if (pipe(stdout_pipe) != 0) {
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        stdin_pipe[0] = stdin_pipe[1] = -1;
        rc = (int) lv_ERROR_IO;
        goto cleanup;
    }

    pid = fork();
    if (pid < 0) {
        rc = (int) lv_ERROR_IO;
        goto cleanup;
    }

    if (pid == 0) {
        /* 子进程：stdin 读输入，stdout/stderr 合并写入管道 */
        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stdout_pipe[1], STDERR_FILENO);
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        execvp(exe, argv);
        /* execvp 返回说明失败（如可执行文件不存在），约定 127 */
        _exit(127);
    }

    /* 父进程：关闭子进程持有的端 */
    close(stdin_pipe[0]);
    stdin_pipe[0] = -1;
    close(stdout_pipe[1]);
    stdout_pipe[1] = -1;
    if (!has_input) {
        close(stdin_pipe[1]);
        stdin_pipe[1] = -1;
    }

    uint64_t deadline = 0;
    if (timeout_ms > 0) {
        deadline = lv_get_time_ms() + (uint64_t) timeout_ms;
    }

    for (;;) {
        /* 1) 超时检查：SIGTERM 优雅退出 1.5s，再 SIGKILL 强杀 */
        if (timeout_ms > 0 && !timed_out && lv_get_time_ms() >= deadline) {
            timed_out = true;
            kill(pid, SIGTERM);
            struct timespec ts = {.tv_sec = 1, .tv_nsec = 500000000L};
            nanosleep(&ts, NULL);
            kill(pid, SIGKILL);
            exit_code_v = -1;
        }

        /* 2) 非阻塞检查子进程退出 */
        int status = 0;
        pid_t wret = waitpid(pid, &status, WNOHANG);
        if (wret == pid) {
            if (!timed_out) {
                exit_code_v = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
            }
            break;
        }

        /* 3) poll：剩余输入可写 + stdout 可读 */
        struct pollfd pfds[2];
        memset(pfds, 0, sizeof(pfds));
        nfds_t nfds = 0;
        if (stdin_pipe[1] >= 0 && in_off < input_len) {
            pfds[nfds].fd = stdin_pipe[1];
            pfds[nfds].events = POLLOUT;
            nfds++;
        }
        pfds[nfds].fd = stdout_pipe[0];
        pfds[nfds].events = POLLIN;
        nfds++;

        int pr = poll(pfds, nfds, 100);
        if (pr < 0) {
            if (errno == EINTR) {
                continue;
            }
            rc = (int) lv_ERROR_IO;
            break;
        }

        for (nfds_t i = 0; i < nfds; i++) {
            short rev = pfds[i].revents;
            if (rev == 0) {
                continue;
            }
            if (pfds[i].fd == stdout_pipe[0]) {
                if (rev & POLLIN) {
                    char buf[lv_LARGE_BUF_SIZE];
                    ssize_t n = read(stdout_pipe[0], buf, sizeof(buf));
                    if (n > 0) {
                        lv_strbuf_append_raw(&ob, buf, (size_t) n);
                    }
                }
            } else {
                /* stdin 写端 */
                if (rev & POLLOUT) {
                    ssize_t w = lv_write_nosigpipe(stdin_pipe[1], input_text + in_off, input_len - in_off);
                    if (w > 0) {
                        in_off += (size_t) w;
                        if (in_off >= input_len) {
                            close(stdin_pipe[1]);
                            stdin_pipe[1] = -1;
                        }
                    } else if (w < 0 && errno != EINTR && errno != EAGAIN) {
                        /* 子进程已关闭 stdin 读端 */
                        close(stdin_pipe[1]);
                        stdin_pipe[1] = -1;
                    }
                } else if (rev & (POLLERR | POLLHUP | POLLNVAL)) {
                    close(stdin_pipe[1]);
                    stdin_pipe[1] = -1;
                }
            }
        }
        if (rc != 0) {
            break;
        }
    }

    if (rc != 0) {
        /* 失败路径：终止并回收仍在运行的子进程 */
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
    }

    /* 排空剩余输出（子进程已退出或被强杀，读到 EOF） */
    if (stdout_pipe[0] >= 0) {
        for (;;) {
            char buf[lv_LARGE_BUF_SIZE];
            ssize_t n = read(stdout_pipe[0], buf, sizeof(buf));
            if (n > 0) {
                lv_strbuf_append_raw(&ob, buf, (size_t) n);
            } else if (n == 0) {
                break;
            } else if (errno == EINTR) {
                continue;
            } else {
                break;
            }
        }
        close(stdout_pipe[0]);
        stdout_pipe[0] = -1;
    }
    if (stdin_pipe[1] >= 0) {
        close(stdin_pipe[1]);
        stdin_pipe[1] = -1;
    }

cleanup:
    if (stdin_pipe[0] >= 0) {
        close(stdin_pipe[0]);
    }
    if (stdin_pipe[1] >= 0) {
        close(stdin_pipe[1]);
    }
    if (stdout_pipe[0] >= 0) {
        close(stdout_pipe[0]);
    }
    if (stdout_pipe[1] >= 0) {
        close(stdout_pipe[1]);
    }

    if (rc != 0) {
        lv_strbuf_destroy(&ob);
        return rc;
    }
    return lv_process_finalize(&ob, out_stdout, out_len, exit_code, exit_code_v);
}

#endif /* !_WIN32 */

/* ============================================================
 * Windows 实现：CreateProcessA + 匿名管道（stdin/stdout）
 * ============================================================ */

#ifdef _WIN32

/** @brief 输出单个字符到命令行缓冲（超出容量则只计数不写入） */
#define LV_WIN_PUTC(c)          \
    do {                        \
        if (o < cap) {          \
            out[o] = (c);       \
        }                       \
        o++;                    \
    } while (0)

/**
 * @brief 将单个参数按 CommandLineToArgvW 规则写入带引号的命令行片段
 * @return 理论长度（含未写入部分），用于截断检测
 */
static size_t lv_win_quote_arg(char *out, size_t cap, const char *arg) {
    size_t o = 0;
    size_t bs = 0;
    bool need = (strchr(arg, ' ') != NULL || strchr(arg, '\t') != NULL || strchr(arg, '"') != NULL);

    if (need) {
        LV_WIN_PUTC('"');
    }
    for (const char *p = arg; *p; p++) {
        if (*p == '\\') {
            bs++;
            continue;
        }
        if (*p == '"') {
            /* 2n+1 反斜杠 + 引号 → n 反斜杠 + 字面引号 */
            while (bs > 0) {
                LV_WIN_PUTC('\\');
                LV_WIN_PUTC('\\');
                bs--;
            }
            LV_WIN_PUTC('\\');
            LV_WIN_PUTC('"');
        } else {
            while (bs > 0) {
                LV_WIN_PUTC('\\');
                bs--;
            }
            LV_WIN_PUTC(*p);
        }
    }
    if (need) {
        /* 2n 反斜杠 + 结束引号 → n 反斜杠 + 字符串结束 */
        while (bs > 0) {
            LV_WIN_PUTC('\\');
            LV_WIN_PUTC('\\');
            bs--;
        }
        LV_WIN_PUTC('"');
    } else {
        while (bs > 0) {
            LV_WIN_PUTC('\\');
            bs--;
        }
    }
    return o;
}

#undef LV_WIN_PUTC

/** @brief 构造 CreateProcessA 命令行（exe + argv，逐参数引号转义） */
static int lv_win_build_cmdline(char *out, size_t cap, const char *exe, char *const argv[]) {
    size_t o = lv_win_quote_arg(out, cap, exe);
    for (int i = 1; argv[i]; i++) {
        if (o + 1 < cap) {
            out[o] = ' ';
        }
        o++;
        o += lv_win_quote_arg(out + (o < cap ? o : cap), (o < cap) ? (cap - o) : 0, argv[i]);
    }
    if (o >= cap) {
        return -1; /* 命令行过长 */
    }
    out[o] = '\0';
    return 0;
}

static int lv_run_win(const char *exe, char *const argv[], const char *input_text, size_t input_len, int timeout_ms,
                      char **out_stdout, size_t *out_len, int *exit_code) {
    SECURITY_ATTRIBUTES sa;
    HANDLE hChildStdin = NULL, hParentStdinWrite = NULL;
    HANDLE hParentStdoutRead = NULL, hChildStdoutWrite = NULL;
    PROCESS_INFORMATION pi;
    char cmdline[8192];
    STARTUPINFOA si;
    lvStrBuf ob;
    lv_strbuf_init(&ob);
    int rc = (int) lv_OK;
    int exit_code_v = -1;
    bool timed_out = false;

    memset(&sa, 0, sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    if (!CreatePipe(&hChildStdin, &hParentStdinWrite, &sa, 65536)) {
        return (int) lv_ERROR_IO;
    }
    if (!CreatePipe(&hParentStdoutRead, &hChildStdoutWrite, &sa, 65536)) {
        CloseHandle(hChildStdin);
        CloseHandle(hParentStdinWrite);
        return (int) lv_ERROR_IO;
    }

    /* 父进程专用端不继承给子进程 */
    if (!SetHandleInformation(hParentStdinWrite, HANDLE_FLAG_INHERIT, 0) ||
        !SetHandleInformation(hParentStdoutRead, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(hChildStdin);
        CloseHandle(hParentStdinWrite);
        CloseHandle(hParentStdoutRead);
        CloseHandle(hChildStdoutWrite);
        return (int) lv_ERROR_IO;
    }

    if (lv_win_build_cmdline(cmdline, sizeof(cmdline), exe, argv) != 0) {
        CloseHandle(hChildStdin);
        CloseHandle(hParentStdinWrite);
        CloseHandle(hParentStdoutRead);
        CloseHandle(hChildStdoutWrite);
        return (int) lv_ERROR_IO;
    }

    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.hStdInput = hChildStdin;
    si.hStdOutput = hChildStdoutWrite;
    si.hStdError = hChildStdoutWrite;
    si.dwFlags |= STARTF_USESTDHANDLES;

    memset(&pi, 0, sizeof(pi));
    if (!CreateProcessA(NULL, cmdline, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(hChildStdin);
        CloseHandle(hParentStdinWrite);
        CloseHandle(hParentStdoutRead);
        CloseHandle(hChildStdoutWrite);
        return (int) lv_ERROR_IO;
    }

    CloseHandle(hChildStdin);
    CloseHandle(hChildStdoutWrite);

    /* 写入 stdin（阻塞写；子进程读取 stdin 到 EOF 后才会产出输出。
     * 输入较小（TPTP/SMT-LIB2 文本），管道缓冲 64KB 足以避免与
     * stdout 读取互相阻塞；子进程提前关闭 stdin 时 WriteFile 返回
     * ERROR_BROKEN_PIPE，停止写入即可。 */
    if (input_text && input_len > 0) {
        size_t off = 0;
        while (off < input_len) {
            DWORD written = 0;
            DWORD to_write = (DWORD) ((input_len - off) > 65536u ? 65536u : (input_len - off));
            if (!WriteFile(hParentStdinWrite, input_text + off, to_write, &written, NULL) || written == 0) {
                break;
            }
            off += (size_t) written;
        }
    }
    CloseHandle(hParentStdinWrite);

    /* 等待 + 读取（带超时；读取与等待交错，避免子进程输出超过
     * 管道缓冲时父进程仍在等待导致互锁） */
    DWORD timeout = (timeout_ms > 0) ? (DWORD) timeout_ms : INFINITE;
    uint64_t start_ms = lv_get_time_ms();
    bool proc_exited = false;

    for (;;) {
        DWORD wr = WaitForSingleObject(pi.hProcess, 100);
        if (wr == WAIT_OBJECT_0) {
            proc_exited = true;
        } else if (wr == WAIT_TIMEOUT) {
            if (timeout != INFINITE && (lv_get_time_ms() - start_ms) >= timeout) {
                timed_out = true;
                TerminateProcess(pi.hProcess, 1);
                WaitForSingleObject(pi.hProcess, 5000);
                proc_exited = true;
            }
        } else {
            proc_exited = true; /* WAIT_FAILED 等异常：按已退出处理 */
        }

        /* 排空当前可用的输出 */
        DWORD avail = 0;
        while (PeekNamedPipe(hParentStdoutRead, NULL, 0, NULL, &avail, NULL) && avail > 0) {
            char buf[lv_LARGE_BUF_SIZE];
            DWORD n = 0;
            DWORD want = (avail < (DWORD) sizeof(buf)) ? avail : (DWORD) sizeof(buf);
            if (!ReadFile(hParentStdoutRead, buf, want, &n, NULL) || n == 0) {
                break;
            }
            lv_strbuf_append_raw(&ob, buf, (size_t) n);
        }
        if (rc != 0 || proc_exited) {
            break;
        }
    }

    if (rc != 0) {
        /* 失败路径：终止仍在运行的子进程 */
        TerminateProcess(pi.hProcess, 1);
        WaitForSingleObject(pi.hProcess, 5000);
    } else {
        /* 排空剩余输出（子进程已退出，读到 EOF） */
        for (;;) {
            char buf[lv_LARGE_BUF_SIZE];
            DWORD n = 0;
            if (!ReadFile(hParentStdoutRead, buf, (DWORD) sizeof(buf), &n, NULL) || n == 0) {
                break;
            }
            lv_strbuf_append_raw(&ob, buf, (size_t) n);
        }
        if (!timed_out) {
            DWORD code = 0;
            if (GetExitCodeProcess(pi.hProcess, &code)) {
                exit_code_v = (int) code;
            }
        }
    }

    CloseHandle(hParentStdoutRead);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (rc != 0) {
        lv_strbuf_destroy(&ob);
        return rc;
    }
    return lv_process_finalize(&ob, out_stdout, out_len, exit_code, exit_code_v);
}

#endif /* _WIN32 */

/* ============================================================
 * 公共 API
 * ============================================================ */

int lv_external_process_run(const char *exe, char *const argv[], const char *input_text, size_t input_len,
                            int timeout_ms, char **out_stdout, size_t *out_len, int *exit_code) {
    if (!exe || !argv || !out_stdout || !out_len || !exit_code) {
        return (int) lv_ERROR_NULL_POINTER;
    }
    if (!argv[0]) {
        return (int) lv_ERROR_INVALID_PARAM;
    }
    if (!input_text) {
        input_len = 0;
    }

    *out_stdout = NULL;
    *out_len = 0;
    *exit_code = -1;

#ifdef _WIN32
    return lv_run_win(exe, argv, input_text, input_len, timeout_ms, out_stdout, out_len, exit_code);
#else
    return lv_run_posix(exe, argv, input_text, input_len, timeout_ms, out_stdout, out_len, exit_code);
#endif
}

bool lv_external_process_available(const char *name) {
    if (!name || name[0] == '\0') {
        return false;
    }

#ifdef _WIN32
    char buf[MAX_PATH];
    if (SearchPathA(NULL, name, ".exe", MAX_PATH, buf, NULL) > 0) {
        return true;
    }
    if (SearchPathA(NULL, name, NULL, MAX_PATH, buf, NULL) > 0) {
        return true;
    }
    return false;
#else
    /* 含路径的名称：直接检查可执行权限 */
    if (strchr(name, '/') != NULL) {
        return access(name, X_OK) == 0;
    }
    /* 遍历 PATH 查找 */
    const char *path = getenv("PATH");
    if (!path || path[0] == '\0') {
        path = "/usr/local/bin:/usr/bin:/bin";
    }
    char full[1024];
    const char *start = path;
    while (start && *start) {
        const char *end = strchr(start, ':');
        size_t len = end ? (size_t) (end - start) : strlen(start);
        if (len > 0 && len + 2 + strlen(name) < sizeof(full)) {
            memcpy(full, start, len);
            size_t w = len;
            if (full[w - 1] != '/') {
                full[w++] = '/';
            }
            memcpy(full + w, name, strlen(name) + 1);
            if (access(full, X_OK) == 0) {
                return true;
            }
        }
        if (!end) {
            break;
        }
        start = end + 1;
    }
    return false;
#endif
}

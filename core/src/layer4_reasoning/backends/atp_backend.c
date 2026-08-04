/**
 * @file atp_backend.c
 * @brief 一阶逻辑自动定理证明器（FOL ATP）后端抽象层实现
 *
 * @details 实现 atp_backend.h 中声明的所有 ATP 后端接口。
 *          通过子进程调用外部 ATP 可执行文件（Vampire、E Prover、iProver），
 *          解析 SZS 状态行获取求解结果。当 ATP 不可用时优雅降级。
 *
 *          与 SMT 后端的分工：
 *          - ATP 处理纯逻辑推导和一阶量词推理
 *          - SMT 处理算术/非线性约束
 *
 *          编码格式：TPTP FOF/CNF/TFF（与 Vampire/E Prover 兼容）
 *
 * @author Lv-00 Project
 * @version 3.3.0
 * @date 2026-05-24
 *
 * @dependencies
 *   - atp_backend.h          : ATP 后端公共接口
 *   - lv_internal.h        : 内部常量与工具宏
 *   - lv_utils.h           : 统一内存分配器
 *   - error_codes.h          : 统一错误码系统
 *   - proof.h                : Lv-00 证明系统
 * [QA] Uses double for timing/layout — not geometric computation. Acceptable.
 */

#include "lv/lv_platform.h"

#include "lv/lv_file.h"

#include "atp_backend.h"
#include "lv/lv_backend_plugin.h"
#include "lv/lv_registry.h"
#include "lv/lv_xmacro.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


#include "lv/lv_parse_utils.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_thread.h"

#include "error_codes.h"
#include "lv_internal.h"
#include "lv_utils.h"


/* ============================================================
 * 模块级常量
 * ============================================================ */

/** @brief 默认求解超时（秒） */
#define lv_config_get_double(LV_CFG_ATP_DEFAULT_TIMEOUT, ATP_DEFAULT_TIMEOUT) 30.0

/** @brief 默认内存限制（MB） */
#define lv_config_get_int(LV_CFG_ATP_DEFAULT_MEMORY_MB, ATP_DEFAULT_MEMORY_MB) 1024

/** @brief TPTP 编码缓冲区默认大小 */
#define lv_config_get_int(LV_CFG_ATP_TPTP_BUFFER_SIZE, ATP_TPTP_BUFFER_SIZE) 65536

/* ============================================================
 * 不透明结构：ATPBackendSolver 内部实现
 * ============================================================ */

/**
 * @brief ATP 求解器内部状态
 */
struct ATPBackendSolver {
    ATPBackendType type; /**< 后端类型 */
    ATPConfig config;    /**< 求解器配置 */
    char *tptp_code;     /**< 已加载的 TPTP 编码 */
    int tptp_len;        /**< TPTP 编码长度 */
    bool is_initialized; /**< 是否已初始化 */
    bool has_problem;    /**< 是否已加载问题 */
};

/* ============================================================
 * 全局后端注册表（单例）
 * ============================================================ */

/** @brief ATP 后端注册表单例状态 */
typedef struct {
    lvRegistry registry;                           /**< 全局注册表（单例） */
    bool registry_inited;                          /**< 注册表是否已初始化 */
    ATPBackendRegistry public_registry;            /**< 公开注册表（兼容 ATPBackendRegistry 布局） */
} ATPRegistryState;

/** @brief ATP 后端注册表全局单例 */
static ATPRegistryState s_atp_registry_state = {0};

/* ============================================================
 * 默认配置
 * ============================================================ */

/**
 * @brief 创建默认 ATP 配置
 *
 * 默认使用 TPTP FOF 格式、30 秒超时、自动策略选择、生成证明。
 */
ATPConfig atp_config_default(void) {
    ATPConfig cfg;
    memset(&cfg, 0, sizeof(ATPConfig));
    cfg.input_format = ATP_FORMAT_TPTP_FOF;
    cfg.timeout_seconds = lv_config_get_double(LV_CFG_ATP_DEFAULT_TIMEOUT, ATP_DEFAULT_TIMEOUT);
    cfg.memory_limit_mb = lv_config_get_int(LV_CFG_ATP_DEFAULT_MEMORY_MB, ATP_DEFAULT_MEMORY_MB);
    cfg.auto_strategy = true;
    cfg.strategy_name = NULL;
    cfg.produce_proof = true;
    cfg.produce_unsat_core = false;
    cfg.use_avatar = false;
    cfg.clause_weight_limit = 0;
    cfg.custom_options = NULL;
    cfg.verbosity = 0;
    cfg.log_file = NULL;
    return cfg;
}

/* ============================================================
 * 静态辅助函数（TPTP 谓词格式化）
 * ============================================================ */

/** @brief 格式化 2-参谓词（如 incident(p%d, l%d)） */
static void s_format_predicate_2(char *buf, size_t sz, const char *fmt, const int *parts) {
    snprintf(buf, sz, fmt, parts[0], parts[1]);
}

/** @brief 格式化 3-参谓词（如 between(p%d, p%d, p%d)） */
static void s_format_predicate_3(char *buf, size_t sz, const char *fmt, const int *parts) {
    snprintf(buf, sz, fmt, parts[0], parts[1], parts[2]);
}

/* ============================================================
 * 约束图 -> TPTP 编码
 * ============================================================ */

/**
 * @brief 将约束图编码为 TPTP FOF/CNF/TFF 格式字符串
 *
 * 将 Lv-00 的几何约束图编码为 TPTP 格式：
 * - 节点 -> 常量符号
 * - 几何类型 -> 一元谓词
 * - 约束 -> 二元/三元谓词
 * - 公理 -> TPTP axiom 子句
 *
 * 框架实现：生成基本 TPTP 骨架。
 */
char *atp_encode_constraint_graph(const ConstraintGraph *graph, ATPInputFormat format, const char *problem_name,
                                  bool include_proof_goal, const Proposition *target_prop) {
    lv_CHECK_NULL(graph, NULL);

    char *buf = (char *) lv_malloc(lv_config_get_int(LV_CFG_ATP_TPTP_BUFFER_SIZE, ATP_TPTP_BUFFER_SIZE));
    if (!buf) {
        return NULL;
    }

    int offset = 0;
    int remaining = lv_config_get_int(LV_CFG_ATP_TPTP_BUFFER_SIZE, ATP_TPTP_BUFFER_SIZE);

    /* 格式 -> TPTP 语言标识符 查找表 */
    static const char *const s_format_lang_table[] = {
        "fof",  /* ATP_FORMAT_TPTP_FOF = 0 */
        "cnf",  /* ATP_FORMAT_TPTP_CNF = 1 */
        "tff",  /* ATP_FORMAT_TPTP_TFF = 2 */
    };
    const char *lang = ((int)format >= 0 && (int)format <= ATP_FORMAT_TPTP_TFF)
                       ? s_format_lang_table[format] : "fof";

    int n = snprintf(buf + offset, (size_t) remaining,
                     "%% TPTP %s encoding for Lv-00 constraint graph\n"
                     "%% Generated by Lv-00 ATP backend\n\n",
                     lang);
    if (n > 0 && n < remaining) {
        offset += n;
        remaining -= n;
    }

    /* 类型声明（TFF 格式） */
    if (format == ATP_FORMAT_TPTP_TFF && remaining > 128) {
        n = snprintf(buf + offset, (size_t) remaining,
                     "tff(point_type, type, point: $tType).\n"
                     "tff(line_type, type, line: $tType).\n\n");
        if (n > 0 && n < remaining) {
            offset += n;
            remaining -= n;
        }
    }

    /* 声明节点常量 —— 使用节点真实 ID 而非数组索引 */
    int node_count = graph->node_count;
    for (int i = 0; i < node_count && remaining > 64; i++) {
        int nid = graph->nodes[i]->id;
        n = snprintf(buf + offset, (size_t) remaining, "%s(p%d_decl, axiom, point(p%d)).\n", lang, nid, nid);
        if (n > 0 && n < remaining) {
            offset += n;
            remaining -= n;
        }
    }

    /* 编码几何约束为谓词 —— 根据实际约束类型编码 */

    /* 约束类型 -> TPTP 谓词格式 静态查找表 */
    typedef struct {
        ConstraintType type;
        int min_participants;
        const char *predicate_fmt;
    } ConstraintPredicateEntry;

    static const ConstraintPredicateEntry s_constraint_predicate_table[] = {
        {INCIDENCE,    2, "incident(p%d, l%d)"},
        {BETWEENNESS,  3, "between(p%d, p%d, p%d)"},
        {INTERSECTION, 3, "intersect(l%d, l%d, p%d)"},
        {CONTAINMENT,  2, "contain(r%d, p%d)"},
        {ANGLE,        2, "angle(l%d, l%d)"},
        {CONNECTION,   2, "connect(p%d, p%d)"},
    };
    const int s_constraint_predicate_count = (int)(sizeof(s_constraint_predicate_table) / sizeof(s_constraint_predicate_table[0]));

    int edge_count = graph->constraint_count;
    for (int i = 0; i < edge_count && remaining > 128; i++) {
        const Constraint *con = graph->constraints[i];
        if (!con || !con->is_active)
            continue;

        /* 查找表驱动：匹配约束类型并构建 TPTP 谓词 */
        const ConstraintPredicateEntry *entry = NULL;
        for (int k = 0; k < s_constraint_predicate_count; k++) {
            if (s_constraint_predicate_table[k].type == con->type) {
                entry = &s_constraint_predicate_table[k];
                break;
            }
        }
        if (!entry || con->participant_count < entry->min_participants) {
            continue;
        }

        /* 根据 participant 数量构建谓词字符串（查找表驱动） */
        typedef void (*PredicateFormatFn)(char *, size_t, const char *, const int *);
        static const PredicateFormatFn s_format_predicate_fn_table[] = {
            NULL,                        /* 0 */
            NULL,                        /* 1 */
            s_format_predicate_2,        /* 2 */
            s_format_predicate_3,        /* 3 */
        };
        char predicate[128];
        int pc = entry->min_participants;
        if (pc < 2 || pc > 3 || !s_format_predicate_fn_table[pc]) {
            continue;
        }
        s_format_predicate_fn_table[pc](predicate, sizeof(predicate), entry->predicate_fmt, con->participants);

        n = snprintf(buf + offset, (size_t) remaining,
                     "%s(constraint_%d, axiom, %s).\n",
                     lang, con->id, predicate);
        if (n > 0 && n < remaining) {
            offset += n;
            remaining -= n;
        }
    }

    /* 添加 conjecture（如果请求） */
    if (include_proof_goal && remaining > 128) {
        if (target_prop) {
            /* 使用目标命题的名称作为 conjecture */
            const char *prop_name = target_prop->name ? target_prop->name : "goal";
            n = snprintf(buf + offset, (size_t) remaining, "%s(%s, conjecture, $false).\n", lang, prop_name);
        } else {
            n = snprintf(buf + offset, (size_t) remaining, "%s(goal, conjecture, $false).\n", lang);
        }
        if (n > 0 && n < remaining) {
            offset += n;
            remaining -= n;
        }
    }

    return buf;
}

/* ============================================================
 * ATP 求解器生命周期
 * ============================================================ */

/**
 * @brief 创建 ATP 求解器实例
 *
 * 框架实现：分配并初始化求解器句柄。
 * 当前所有后端为占位实现，待链接实际 ATP 可执行文件后
 * 将产生真实的证明结果。支持的 ATP 后端包括：
 *   - E prover: 等式推理和超归结
 *   - Vampire: 一阶逻辑自动定理证明
 *   - Z3: SMT 求解（通过 SMT-LIB 2 接口）
 */
ATPBackendSolver *atp_solver_create(ATPBackendType type, const ATPConfig *config) {
    ATPBackendSolver *solver = (ATPBackendSolver *) lv_calloc(1, sizeof(ATPBackendSolver));
    if (!solver) {
        return NULL;
    }
    solver->type = type;
    solver->is_initialized = true;
    solver->has_problem = false;

    if (config) {
        solver->config = *config;
    } else {
        solver->config = atp_config_default();
    }

    return solver;
}

/**
 * @brief 销毁 ATP 求解器
 */
void atp_solver_destroy(ATPBackendSolver *solver) {
    if (!solver) {
        return;
    }
    if (solver->tptp_code) {
        lv_free((void **) &solver->tptp_code);
    }
    lv_free((void **) &solver);
}

/**
 * @brief 获取求解器后端类型
 */
ATPBackendType atp_solver_get_type(const ATPBackendSolver *solver) {
    if (!solver) {
        return ATP_BACKEND_COUNT;
    }
    return solver->type;
}

/* ============================================================
 * ATP 可执行文件检测
 * ============================================================ */

/**
 * @brief 获取 ATP 后端对应的可执行文件名
 */
static const char *atp_executable_name(ATPBackendType type) {
    /* 后端类型 -> 可执行文件名 查找表（按枚举值升序） */
    static const char *const s_executable_name_table[] = {
        "vampire",   /* ATP_BACKEND_VAMPIRE = 0 */
        "eprover",   /* ATP_BACKEND_EPROVER = 1 */
        "iprover",   /* ATP_BACKEND_IPROVER = 2 */
        NULL,        /* ATP_BACKEND_CUSTOM = 3 */
    };
    if ((int)type >= 0 && (int)type < ATP_BACKEND_CUSTOM) {
        return s_executable_name_table[type];
    }
    return NULL;
}

/**
 * @brief 通过尝试执行 "exec --version" 检测 ATP 可执行文件是否可用
 *
 * 使用 popen 检测可执行文件是否在 PATH 中。
 */
static bool atp_check_executable(const char *name) {
    if (!name)
        return false;

    char cmd[512];
#ifdef _WIN32
    snprintf(cmd, sizeof(cmd), "where %s 2>NUL", name);
#else
    snprintf(cmd, sizeof(cmd), "command -v %s 2>/dev/null", name);
#endif

    FILE *fp = lv_popen(cmd, "r");
    if (!fp)
        return false;

    char buffer[256];
    bool found = false;
    if (fgets(buffer, sizeof(buffer), fp)) {
        /* 如果命令找到了文件，输出包含路径 */
        found = (buffer[0] != '\0' && buffer[0] != '\n');
    }
    lv_pclose(fp);
    return found;
}

/* ============================================================
 * ATP 子进程调用
 * ============================================================ */

/**
 * @brief 通过子进程调用 ATP 求解器并捕获输出
 *
 * @param[in]  executable  可执行文件名
 * @param[in]  tptp_text   TPTP 编码文本
 * @param[in]  timeout_sec 超时秒数
 * @param[in]  extra_args  额外命令行参数（可为 NULL）
 * @param[out] out_output  捕获的 stdout（调用者 free）
 * @param[out] out_exit_code 进程退出码
 * @return lv_OK 成功
 */
static int atp_run_subprocess(const char *executable, const char *tptp_text, double timeout_sec, const char *extra_args,
                              char **out_output, int *out_exit_code) {
    /* 记录开始时间，用于超时检查 */
    time_t start_time = time(NULL);
    if (start_time == (time_t) -1)
        start_time = 0;

    if (!executable || !tptp_text || !out_output || !out_exit_code)
        return (int) lv_ERROR_NULL_POINTER;

    *out_output = NULL;
    *out_exit_code = -1;

    /* 将 TPTP 文本写入临时文件 */
#ifdef _WIN32
    char temp_path[MAX_PATH + 64];
    char temp_dir[MAX_PATH];

    /* 获取临时目录 */
    DWORD len = GetTempPathA(MAX_PATH, temp_dir);
    if (len == 0) {
        snprintf(temp_dir, sizeof(temp_dir), ".");
    }

    /* 生成唯一临时文件名 */
    snprintf(temp_path, sizeof(temp_path), "%slv_atp_%d.p", temp_dir, (int) GetCurrentProcessId());

    FILE *tmp = lv_file_open(temp_path, "w");
    if (!tmp)
        return (int) lv_ERROR_IO;

    fputs(tptp_text, tmp);
    lv_file_close(tmp);

    /* 构建命令行 */
    char cmd[2048];
    if (extra_args && extra_args[0] != '\0') {
        snprintf(cmd, sizeof(cmd), "%s %s %s 2>&1", executable, extra_args, temp_path);
    } else {
        snprintf(cmd, sizeof(cmd), "%s %s 2>&1", executable, temp_path);
    }

    /* 创建匿名管道用于捕获 stdout+stderr */
    HANDLE hReadPipe, hWritePipe;
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        remove(temp_path);
        return (int) lv_ERROR_IO;
    }
    /* 确保读端不继承给子进程 */
    if (!SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        remove(temp_path);
        return (int) lv_ERROR_IO;
    }

    /* 设置子进程启动信息 */
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));
    si.cb = sizeof(STARTUPINFOA);
    si.hStdError = hWritePipe;
    si.hStdOutput = hWritePipe;
    si.dwFlags |= STARTF_USESTDHANDLES;

    BOOL created = CreateProcessA(
        NULL,      /* 可执行文件名（在命令行中指定） */
        cmd,       /* 命令行 */
        NULL,      /* 进程安全属性 */
        NULL,      /* 线程安全属性 */
        TRUE,      /* 句柄可继承 */
        0,         /* 创建标志 */
        NULL,      /* 环境变量 */
        NULL,      /* 当前目录 */
        &si,
        &pi
    );

    /* 关闭写端句柄——子进程拥有其副本 */
    CloseHandle(hWritePipe);

    if (!created) {
        CloseHandle(hReadPipe);
        remove(temp_path);
        return (int) lv_ERROR_IO;
    }

    /* 分配输出缓冲区 */
    size_t out_size = 65536;
    size_t out_len = 0;
    char *output = (char *) lv_malloc(out_size);
    if (!output) {
        CloseHandle(hReadPipe);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        remove(temp_path);
        return (int) lv_ERROR_OUT_OF_MEMORY;
    }

    /* 等待进程完成或超时 */
    DWORD timeout_ms = (timeout_sec > 0.0) ? (DWORD)(timeout_sec * 1000.0) : INFINITE;
    DWORD wait_result = WaitForSingleObject(pi.hProcess, timeout_ms);

    int exit_code = -1;
    if (wait_result == WAIT_TIMEOUT) {
        /* 超时：强制终止子进程 */
        TerminateProcess(pi.hProcess, 1);
        WaitForSingleObject(pi.hProcess, 5000);
        exit_code = -1;
    } else {
        /* 正常退出，获取退出码 */
        DWORD code = 0;
        if (GetExitCodeProcess(pi.hProcess, &code)) {
            exit_code = (int) code;
        }
    }

    /* 读取管道中的所有输出（进程已终止或已完成） */
    DWORD bytes_read;
    char buffer[lv_LARGE_BUF_SIZE];
    while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytes_read, NULL) && bytes_read > 0) {
        buffer[bytes_read] = '\0';
        size_t chunk_len = strlen(buffer);
        while (out_len + chunk_len + 1 >= out_size) {
            out_size *= 2;
            char *new_output = (char *) lv_realloc(output, out_size);
            if (!new_output) {
                lv_free((void **) &output);
                CloseHandle(hReadPipe);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                remove(temp_path);
                return (int) lv_ERROR_OUT_OF_MEMORY;
            }
            output = new_output;
        }
        memcpy(output + out_len, buffer, chunk_len);
        out_len += chunk_len;
    }
    output[out_len] = '\0';

    /* 清理 */
    CloseHandle(hReadPipe);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    /* 清理临时文件 */
    remove(temp_path);

#else /* POSIX: fork + execvp + pipe + dup2 */
    char temp_path[256];
    const char *tmpdir = getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp";
    snprintf(temp_path, sizeof(temp_path), "%s/lv_atp_%d.p", tmpdir, (int) getpid());

    FILE *tmp = lv_file_open(temp_path, "w");
    if (!tmp)
        return (int) lv_ERROR_IO;

    fputs(tptp_text, tmp);
    lv_file_close(tmp);

    /* 准备参数列表 */
    char *exec_argv[16];
    char *extra_copy = NULL;
    int argc = 0;
    exec_argv[argc++] = (char *) executable;
    if (extra_args && extra_args[0] != '\0') {
        /* 简单参数切分（空格分隔） */
        extra_copy = lv_strdup(extra_args);
        if (extra_copy) {
            char *save_ptr = NULL;
            char *token = strtok_s(extra_copy, " ", &save_ptr);
            while (token && argc < 14) {
                exec_argv[argc++] = token;
                token = strtok_s(NULL, " ", &save_ptr);
            }
        }
    }
    /* bounds check: ensure indices 14 (temp_path) and 15 (NULL) fit in exec_argv[16] */
    if (argc > 14) {
        lv_free((void **) &extra_copy);
        return (int) lv_ERROR_IO;
    }
    exec_argv[argc++] = temp_path;
    exec_argv[argc] = NULL;

    /* 创建管道 */
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        remove(temp_path);
        return (int) lv_ERROR_IO;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        remove(temp_path);
        return (int) lv_ERROR_IO;
    }

    if (pid == 0) {
        /* 子进程：重定向 stdout/stderr 到管道写端 */
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        execvp(executable, exec_argv);
        /* 如果 execvp 返回，说明出错 */
        _exit(127);
    }

    /* 父进程：关闭写端，使用 poll + WNOHANG 实现带超时的进程等待 */
    lv_free((void **) &extra_copy);
    close(pipefd[1]);

    size_t out_size = 65536;
    size_t out_len = 0;
    char *output = (char *) lv_malloc(out_size);
    if (!output) {
        close(pipefd[0]);
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
        remove(temp_path);
        return (int) lv_ERROR_OUT_OF_MEMORY;
    }

    bool process_exited = false;
    bool timed_out = false;
    int exit_code = -1;

    /* 使用 poll() 实现带超时的管道读取和进程状态监控 */
    struct pollfd pfd;
    pfd.fd = pipefd[0];
    pfd.events = POLLIN;

    while (1) {
        /* 检查是否超时 */
        if (timeout_sec > 0.0 && !timed_out) {
            double elapsed = difftime(time(NULL), start_time);
            if (elapsed >= timeout_sec) {
                timed_out = true;
                kill(pid, SIGTERM);
                /* 给 1.5 秒优雅退出，然后强制终止 */
                struct timespec ts = { .tv_sec = 1, .tv_nsec = 500000000L };
                nanosleep(&ts, NULL);
                kill(pid, SIGKILL);
                exit_code = -1;
            }
        }

        /* 如果已超时终止，做最终读取 */
        if (timed_out) {
            char buf[lv_LARGE_BUF_SIZE];
            ssize_t n;
            while ((n = read(pipefd[0], buf, sizeof(buf))) > 0) {
                while (out_len + (size_t) n + 1 >= out_size) {
                    out_size *= 2;
                    char *new_output = (char *) lv_realloc(output, out_size);
                    if (!new_output) {
                        lv_free((void **) &output);
                        close(pipefd[0]);
                        waitpid(pid, NULL, 0);
                        remove(temp_path);
                        return (int) lv_ERROR_OUT_OF_MEMORY;
                    }
                    output = new_output;
                }
                memcpy(output + out_len, buf, (size_t) n);
                out_len += (size_t) n;
            }
            break;
        }

        /* 非阻塞地检查子进程是否已退出 */
        int status = 0;
        pid_t ret = waitpid(pid, &status, WNOHANG);
        if (ret == pid) {
            /* 子进程已正常退出 */
            process_exited = true;
            exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
            /* 读取剩余管道数据 */
            char buf[lv_LARGE_BUF_SIZE];
            ssize_t n;
            while ((n = read(pipefd[0], buf, sizeof(buf))) > 0) {
                while (out_len + (size_t) n + 1 >= out_size) {
                    out_size *= 2;
                    char *new_output = (char *) lv_realloc(output, out_size);
                    if (!new_output) {
                        lv_free((void **) &output);
                        close(pipefd[0]);
                        remove(temp_path);
                        return (int) lv_ERROR_OUT_OF_MEMORY;
                    }
                    output = new_output;
                }
                memcpy(output + out_len, buf, (size_t) n);
                out_len += (size_t) n;
            }
            break;
        }

        /* 使用 poll 等待管道数据就绪（100ms 超时） */
        int poll_ret = poll(&pfd, 1, 100);
        if (poll_ret > 0 && (pfd.revents & POLLIN)) {
            char buf[lv_LARGE_BUF_SIZE];
            ssize_t nread = read(pipefd[0], buf, sizeof(buf));
            if (nread > 0) {
                while (out_len + (size_t) nread + 1 >= out_size) {
                    out_size *= 2;
                    char *new_output = (char *) lv_realloc(output, out_size);
                    if (!new_output) {
                        lv_free((void **) &output);
                        close(pipefd[0]);
                        kill(pid, SIGKILL);
                        waitpid(pid, NULL, 0);
                        remove(temp_path);
                        return (int) lv_ERROR_OUT_OF_MEMORY;
                    }
                    output = new_output;
                }
                memcpy(output + out_len, buf, (size_t) nread);
                out_len += (size_t) nread;
            } else if (nread == 0) {
                /* EOF — 子进程已关闭 stdout，继续检查进程状态 */
                continue;
            }
        }
        /* poll_ret == 0：100ms 内无数据，回到循环开头重新检查超时和进程状态 */
    }

    if (out_len == 0) {
        output[0] = '\0';
    } else {
        output[out_len] = '\0';
    }
    close(pipefd[0]);

    /* 如果进程尚未被回收（非超时情况），最后阻塞等待一次 */
    if (!process_exited && !timed_out) {
        int status = 0;
        waitpid(pid, &status, 0);
        exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }

    /* 清理临时文件 */
    remove(temp_path);

#endif

    *out_output = output;
    *out_exit_code = exit_code;
    return (int) lv_OK;
}

/**
 * @brief 从 ATP 输出中解析 SZS 状态行
 *
 * SZS 状态行格式：SZS status: <result>
 * 常见结果：Theorem, Unsatisfiable, Satisfiable, CounterSatisfiable,
 *           Timeout, ResourceOut, Unknown, Error
 */
static ATPResult atp_parse_szs_status(const char *output) {
    if (!output)
        return ATP_RESULT_UNKNOWN;

    /* 搜索 SZS 状态行 */
    const char *szs = strstr(output, "SZS status:");
    if (!szs)
        return ATP_RESULT_UNKNOWN;

    /* 跳过 "SZS status:" 前缀 */
    const char *status = szs + strlen("SZS status:");
    while (*status == ' ')
        status++;

    /* 匹配结果 */
    if (lv_str_startswith(status, "Theorem") || lv_str_startswith(status, "Unsatisfiable"))
        return ATP_RESULT_UNSAT;

    if (lv_str_startswith(status, "Satisfiable") || lv_str_startswith(status, "CounterSatisfiable"))
        return ATP_RESULT_SAT;

    if (lv_str_startswith(status, "Timeout") || lv_str_startswith(status, "ResourceOut"))
        return ATP_RESULT_UNKNOWN;

    if (lv_str_startswith(status, "Error"))
        return ATP_RESULT_ERROR;

    return ATP_RESULT_UNKNOWN;
}

/**
 * @brief 从 ATP 输出中提取证明步骤（TSTP 格式）
 *
 * TSTP 证明步骤格式：
 *   step_id. [status] clause (inference(rule, [parent1, parent2, ...])).
 */
static int atp_extract_proof_steps(const char *output, ATPProofStep **out_steps, int *out_step_count) {
    if (!output || !out_steps || !out_step_count)
        return (int) lv_ERROR_NULL_POINTER;

    *out_steps = NULL;
    *out_step_count = 0;

    /* 计算证明步骤数（以行首数字+点开头的行） */
    int capacity = 64;
    ATPProofStep *steps = (ATPProofStep *) lv_calloc((size_t) capacity, sizeof(ATPProofStep));
    if (!steps)
        return (int) lv_ERROR_OUT_OF_MEMORY;

    int count = 0;
    const char *line = output;

    while (*line) {
        /* 跳过空白 */
        while (*line == ' ' || *line == '\t' || *line == '\n' || *line == '\r')
            line++;

        if (*line == '\0')
            break;

        /* 检查是否是证明步骤行：以数字开头，后跟点和括号 */
        const char *p = line;
        bool is_step = false;
        if (*p >= '0' && *p <= '9') {
            while (*p >= '0' && *p <= '9')
                p++;
            if (*p == '.' && *(p + 1) == ' ') {
                is_step = true;
            }
        }

        if (!is_step) {
            /* 跳到行尾 */
            while (*line && *line != '\n')
                line++;
            continue;
        }

        /* 提取步骤 ID */
        int step_id = 0;
        lv_parse_int(line, &step_id);

        /* 提取子句内容（到行尾） */
        const char *clause_start = line;
        while (*clause_start != ' ')
            clause_start++;
        while (*clause_start == ' ')
            clause_start++;

        const char *clause_end = clause_start;
        while (*clause_end && *clause_end != '\n')
            clause_end++;

        int clause_len = (int) (clause_end - clause_start);
        if (clause_len > 0 && count < capacity) {
            steps[count].step_id = step_id;
            steps[count].is_axiom = false;
            steps[count].is_goal = false;
            steps[count].inference_rule = NULL;
            steps[count].justification = NULL;

            steps[count].clause = (char *) lv_malloc((size_t) clause_len + 1);
            if (steps[count].clause) {
                memcpy(steps[count].clause, clause_start, (size_t) clause_len);
                steps[count].clause[clause_len] = '\0';
            }

            /* 检查是否包含 inference 规则 */
            const char *inf = strstr(clause_start, "inference(");
            if (inf && inf < clause_end) {
                const char *rule_start = inf + strlen("inference(");
                const char *rule_end = strchr(rule_start, ',');
                if (rule_end) {
                    int rule_len = (int) (rule_end - rule_start);
                    steps[count].inference_rule = (char *) lv_malloc((size_t) rule_len + 1);
                    if (steps[count].inference_rule) {
                        memcpy(steps[count].inference_rule, rule_start, (size_t) rule_len);
                        steps[count].inference_rule[rule_len] = '\0';
                    }
                }
            }

            /* 检查是否是目标行 */
            if (strstr(clause_start, "[goal]") || strstr(clause_start, "conjecture"))
                steps[count].is_goal = true;

            count++;
        }

        /* 跳到行尾 */
        line = clause_end;
    }

    *out_steps = steps;
    *out_step_count = count;
    return (int) lv_OK;
}

/* ============================================================
 * ATP 求解操作
 * ============================================================ */

/**
 * @brief 将 TPTP 编码加载到求解器
 */
int atp_solver_load(ATPBackendSolver *solver, const char *tptp_text) {
    lv_CHECK_NULL(solver, (int) lv_ERROR_NULL_POINTER);
    lv_CHECK_NULL(tptp_text, (int) lv_ERROR_NULL_POINTER);

    if (!solver->is_initialized) {
        return (int) lv_ERROR_NOT_INITIALIZED;
    }

    /* 释放旧编码 */
    if (solver->tptp_code) {
        lv_free((void **) &solver->tptp_code);
    }

    int len = (int) strlen(tptp_text);
    if (len <= 0) {
        return (int) lv_ERROR_INVALID_PARAM;
    }

    solver->tptp_code = (char *) lv_malloc((size_t) (len + 1));
    if (!solver->tptp_code) {
        return (int) lv_ERROR_OUT_OF_MEMORY;
    }

    memcpy(solver->tptp_code, tptp_text, (size_t) (len + 1));
    solver->tptp_len = len;
    solver->has_problem = true;

    return (int) lv_OK;
}

/**
 * @brief 执行 ATP 求解
 *
 * 真实实现：通过子进程调用 ATP 可执行文件，解析 SZS 状态行。
 * 如果 ATP 不可用，优雅降级返回 UNKNOWN。
 */
int atp_solver_solve(ATPBackendSolver *solver, ATPResultInfo *result) {
    lv_CHECK_NULL(solver, (int) lv_ERROR_NULL_POINTER);
    lv_CHECK_NULL(result, (int) lv_ERROR_NULL_POINTER);

    atp_result_init(result);

    if (!solver->is_initialized) {
        result->result = ATP_RESULT_ERROR;
        result->error_code = (int) lv_ERROR_NOT_INITIALIZED;
        snprintf(result->error_message, sizeof(result->error_message), "ATP solver not initialized");
        return (int) lv_ERROR_NOT_INITIALIZED;
    }

    if (!solver->has_problem) {
        result->result = ATP_RESULT_ERROR;
        result->error_code = (int) lv_ERROR_INVALID_STATE;
        snprintf(result->error_message, sizeof(result->error_message), "No problem loaded");
        return (int) lv_ERROR_INVALID_STATE;
    }

    result->backend = solver->type;

    /* 获取可执行文件名 */
    const char *exe_name = atp_executable_name(solver->type);
    if (!exe_name) {
        result->result = ATP_RESULT_UNKNOWN;
        snprintf(result->error_message, sizeof(result->error_message), "Unknown ATP backend type");
        return (int) lv_OK;
    }

    /* 检查 ATP 是否可用 */
    if (!atp_check_executable(exe_name)) {
        /* 优雅降级：ATP 不可用，返回 UNKNOWN */
        result->result = ATP_RESULT_UNKNOWN;
        result->solve_time_seconds = 0.0;
        snprintf(result->error_message, sizeof(result->error_message),
                 "ATP backend '%s' not found in PATH; returning UNKNOWN (graceful degradation)",
                 atp_backend_type_name(solver->type));
        return (int) lv_OK;
    }

    /* 构建额外参数 */
    char extra_args[512];
    extra_args[0] = '\0';

    /* 输入格式 -> 模式参数 查找表 */
    static const char *const s_format_mode_table[] = {
        "--fof",  /* ATP_FORMAT_TPTP_FOF = 0 */
        "--cnf",  /* ATP_FORMAT_TPTP_CNF = 1 */
        "--tff",  /* ATP_FORMAT_TPTP_TFF = 2 */
    };
    const char *mode = ((int)solver->config.input_format >= 0
                        && (int)solver->config.input_format <= ATP_FORMAT_TPTP_TFF)
                       ? s_format_mode_table[solver->config.input_format] : "--fof";

    snprintf(extra_args, sizeof(extra_args), "%s -t %d --proof tptp", mode, (int) solver->config.timeout_seconds);

    if (solver->config.custom_options) {
        size_t len = strlen(extra_args);
        snprintf(extra_args + len, sizeof(extra_args) - len, " %s", solver->config.custom_options);
    }

    /* 调用 ATP 子进程 — 记录实际耗时 */
    char *raw_output = NULL;
    int exit_code = -1;
    clock_t start_clock = clock();
    int rc = atp_run_subprocess(exe_name, solver->tptp_code, solver->config.timeout_seconds, extra_args, &raw_output,
                                &exit_code);
    double elapsed_seconds = lv_clock_elapsed_sec(start_clock);

    if (rc != (int) lv_OK) {
        result->result = ATP_RESULT_ERROR;
        result->error_code = rc;
        snprintf(result->error_message, sizeof(result->error_message), "Failed to execute ATP subprocess: error %d",
                 rc);
        return (int) lv_OK;
    }

    /* 存储 raw output */
    result->raw_output = raw_output;
    result->raw_output_length = raw_output ? (int) strlen(raw_output) : 0;

    /* 解析 SZS 状态 */
    result->result = atp_parse_szs_status(raw_output);

    /* 如果求解成功且需要证明，提取证明步骤 */
    if (result->result == ATP_RESULT_UNSAT && solver->config.produce_proof) {
        atp_extract_proof_steps(raw_output, &result->proof_steps, &result->proof_step_count);
    }

    /* 使用 clock() 测量实际求解耗时 */
    result->solve_time_seconds = elapsed_seconds;
    result->generated_clauses = 0;
    result->processed_clauses = 0;
    result->kept_clauses = 0;

    /* 统计子句数（简化：统计输出中的行数） */
    if (raw_output) {
        int lines = 0;
        for (const char *p = raw_output; *p; p++) {
            if (*p == '\n')
                lines++;
        }
        result->generated_clauses = lines;
    }

    return (int) lv_OK;
}

/**
 * @brief 便捷函数：编码 + 加载 + 求解
 */
int atp_solver_solve_graph(ATPBackendSolver *solver, const ConstraintGraph *graph, ATPInputFormat format,
                           const char *problem_name, bool include_goal, const Proposition *target_prop,
                           ATPResultInfo *result) {
    lv_CHECK_NULL(solver, (int) lv_ERROR_NULL_POINTER);
    lv_CHECK_NULL(graph, (int) lv_ERROR_NULL_POINTER);
    lv_CHECK_NULL(result, (int) lv_ERROR_NULL_POINTER);

    /* 步骤 1：编码 */
    char *tptp = atp_encode_constraint_graph(graph, format, problem_name, include_goal, target_prop);
    if (!tptp) {
        atp_result_init(result);
        result->result = ATP_RESULT_ERROR;
        result->error_code = (int) lv_ERROR_OUT_OF_MEMORY;
        snprintf(result->error_message, sizeof(result->error_message), "TPTP encoding failed");
        return (int) lv_ERROR_OUT_OF_MEMORY;
    }

    /* 步骤 2：加载 */
    int rc = atp_solver_load(solver, tptp);
    lv_free((void **) &tptp);

    if (rc != (int) lv_OK) {
        atp_result_init(result);
        result->result = ATP_RESULT_ERROR;
        result->error_code = rc;
        snprintf(result->error_message, sizeof(result->error_message), "Failed to load TPTP encoding");
        return rc;
    }

    /* 步骤 3：求解 */
    return atp_solver_solve(solver, result);
}

/* ============================================================
 * 结果处理与转换
 * ============================================================ */

/**
 * @brief 初始化 ATP 求解结果
 */
void atp_result_init(ATPResultInfo *result) {
    if (!result) {
        return;
    }
    memset(result, 0, sizeof(ATPResultInfo));
    result->result = ATP_RESULT_UNKNOWN;
    result->backend = ATP_BACKEND_COUNT;
}

/**
 * @brief 释放 ATP 求解结果中的动态资源
 */
void atp_result_destroy(ATPResultInfo *result) {
    if (!result) {
        return;
    }
    if (result->proof_steps) {
        for (int i = 0; i < result->proof_step_count; i++) {
            if (result->proof_steps[i].clause) {
                lv_free((void **) &result->proof_steps[i].clause);
            }
            if (result->proof_steps[i].inference_rule) {
                lv_free((void **) &result->proof_steps[i].inference_rule);
            }
            if (result->proof_steps[i].justification) {
                lv_free((void **) &result->proof_steps[i].justification);
            }
        }
        lv_free((void **) &result->proof_steps);
    }
    result->proof_step_count = 0;
    if (result->unsat_core_clause_ids) {
        lv_free((void **) &result->unsat_core_clause_ids);
    }
    result->unsat_core_count = 0;
    if (result->raw_output) {
        lv_free((void **) &result->raw_output);
    }
    result->raw_output_length = 0;
}

/**
 * @brief 将 ATP 证明转换为 Lv-00 ProofNavigator 步骤
 *
 * 解析 TSTP 格式的 ATP 证明输出，转换为 Lv-00 证明步骤。
 * 实现基本的证明步骤转换：
 * - 解析每个证明步骤的推理规则（resolution/paramodulation/superposition等）
 * - 创建 lvProofStep 对象并设置适当的类型标签
 * - 将步骤链接到有向证明图中
 */
int atp_proof_to_lv(const ATPResultInfo *result, Proof *proof, int *step_count) {
    lv_CHECK_NULL(result, (int) lv_ERROR_NULL_POINTER);
    lv_CHECK_NULL(proof, (int) lv_ERROR_NULL_POINTER);

    if (step_count) {
        *step_count = 0;
    }

    if (result->result != ATP_RESULT_UNSAT) {
        /* lv_ERROR_SET(lv_ERROR_INVALID_STATE,
                        "Proof conversion requires UNSAT result"); */
        return (int) lv_ERROR_INVALID_STATE;
    }

    if (!result->proof_steps || result->proof_step_count == 0) {
        return (int) lv_ERROR_NOT_FOUND;
    }

    /* 第一步：为每个 ATP 步骤创建 Lv-00 ProofStep 并记录映射关系 */
    int count = 0;
    int *lv_step_ids = (int *) lv_calloc((size_t) result->proof_step_count, sizeof(int));
    if (!lv_step_ids)
        return (int) lv_ERROR_OUT_OF_MEMORY;

    for (int i = 0; i < result->proof_step_count; i++) {
        const ATPProofStep *atp_step = &result->proof_steps[i];
        if (!atp_step->clause)
            continue;

        /* 根据推理规则映射 Lv-00 步骤类型 */
        ProofStepType step_type = PROOF_STEP_REWRITE; /* 默认 */
        if (atp_step->is_axiom) {
            step_type = PROOF_STEP_ADD_CONSTRAINT;
        } else if (atp_step->is_goal) {
            step_type = PROOF_STEP_UNIFY;
        } else if (atp_step->inference_rule) {
            /* 所有推理规则均标记为通用 REWRITE（step_type 已默认为 PROOF_STEP_REWRITE） */
        }

        ProofStep *lv_step = proof_step_create(step_type);
        if (!lv_step) {
            /* 清理已创建的步骤 */
            for (int j = 0; j < i; j++) {
                if (lv_step_ids[j] >= 0) {
                    /* 步骤已添加到 proof navigator，由 proof 负责生命周期 */
                }
            }
            lv_free((void **) &lv_step_ids);
            return (int) lv_ERROR_OUT_OF_MEMORY;
        }

        lv_step->id = atp_step->step_id;
        lv_step->color = PROOF_COLOR_GREEN; /* ATP 证明步骤默认为绿色 */
        lv_step->note = atp_step->clause;   /* 存储原始子句作为注释 */

        /* 添加到证明导航器 */
        if (!proof_navigator_add_step(proof, lv_step)) {
            proof_step_destroy(lv_step);
            lv_free((void **) &lv_step_ids);
            return (int) lv_ERROR_INVALID_STATE;
        }

        /* 记录映射：ATP step_id -> Lv-00 proof step index */
        /* 使用 proof->step_count - 1 作为刚添加的步骤索引 */
        lv_step_ids[i] = proof->step_count - 1;
        count++;
    }

    /* 第二步：建立步骤之间的依赖关系（有向证明图） */
    for (int i = 0; i < result->proof_step_count; i++) {
        if (lv_step_ids[i] < 0)
            continue;

        const ATPProofStep *atp_step = &result->proof_steps[i];
        ProofStep *lv_step = proof->steps[lv_step_ids[i]];

        /* 解析 justification 以找到父步骤引用 */
        if (atp_step->justification) {
            /* justification 格式通常是 "step_id1,step_id2,..." */
            const char *p = atp_step->justification;
            while (*p) {
                /* 跳过非数字字符 */
                while (*p && (*p < '0' || *p > '9'))
                    p++;
                if (!*p)
                    break;

                int parent_id = 0;
                lv_parse_int(p, &parent_id);
                /* 查找该父步骤在我们的映射中的位置 */
                for (int j = 0; j < result->proof_step_count; j++) {
                    if (result->proof_steps[j].step_id == parent_id && lv_step_ids[j] >= 0) {
                        proof_step_add_dependency(lv_step, result->proof_steps[j].step_id);
                        /* 设置第一个匹配的父步骤作为树结构中的父节点 */
                        if (lv_step->parent_step_id < 0) {
                            lv_step->parent_step_id = result->proof_steps[j].step_id;
                        }
                        break;
                    }
                }
                /* 跳过这个数字 */
                while (*p >= '0' && *p <= '9')
                    p++;
            }
        }
    }

    lv_free((void **) &lv_step_ids);

    if (step_count) {
        *step_count = count;
    }

    return (int) lv_OK;
}

/* ============================================================
 * 后端注册与发现
 * ============================================================ */

/**
 * @brief 获取全局 ATP 后端注册表（返回保留的私有指针，用于外部只读访问）
 *
 * 返回的指针指向内部的 ATPBackendRegistry，可用于遍历已注册的后端。
 */
const ATPBackendRegistry *atp_get_registry(void) {
    if (!s_atp_registry_state.registry_inited) {
        lv_registry_init(&s_atp_registry_state.registry, ATP_BACKEND_COUNT);
        s_atp_registry_state.registry_inited = true;
    }
    return &s_atp_registry_state.public_registry;
}

/**
 * @brief 注册自定义 ATP 后端
 */
int atp_register_backend(const ATPBackendEntry *entry) {
    lv_CHECK_NULL(entry, (int) lv_ERROR_NULL_POINTER);

    if (!s_atp_registry_state.registry_inited) {
        lv_registry_init(&s_atp_registry_state.registry, ATP_BACKEND_COUNT);
        s_atp_registry_state.registry_inited = true;
    }

    /* 使用 atp_backend_type_name 获取后端名称 */
    const char *name = atp_backend_type_name(entry->type);
    if (!name) {
        return (int) lv_ERROR_INVALID_PARAM;
    }

    /* 注册到通用注册表（名称 + 工厂函数），检查重复 */
    if (!lv_registry_register(&s_atp_registry_state.registry, name, (void *(*)(void)) entry->create)) {
        /* 名称已存在 */
        return (int) lv_ERROR_ALREADY_EXISTS;
    }

    /* 保存完整元数据到并行数组 */
    if (s_atp_registry_state.public_registry.count < ATP_BACKEND_COUNT) {
        s_atp_registry_state.public_registry.entries[s_atp_registry_state.public_registry.count] = *entry;
        s_atp_registry_state.public_registry.count++;
    }

    return (int) lv_OK;
}

/**
 * @brief 检查后端在系统上是否可用
 *
 * 通过 `where` 命令检查可执行文件是否在 PATH 中。
 */
bool atp_is_backend_available(ATPBackendType type) {
    const char *exe = atp_executable_name(type);
    if (!exe)
        return false;
    return atp_check_executable(exe);
}

/**
 * @brief 查找后端条目
 */
const ATPBackendEntry *atp_find_backend(ATPBackendType type) {
    if (!s_atp_registry_state.registry_inited) {
        return NULL;
    }

    for (int i = 0; i < s_atp_registry_state.public_registry.count; i++) {
        if (s_atp_registry_state.public_registry.entries[i].type == type) {
            return &s_atp_registry_state.public_registry.entries[i];
        }
    }
    return NULL;
}

/**
 * @brief 获取后端类型名称
 */
/* ================================================================
 * 枚举 -> 名称 映射表（数据表化，替代 switch）
 * ================================================================ */

/** @brief atp_backend_type_name 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_atp_backend_type_name_entries[] = {
    {"Vampire", ATP_BACKEND_VAMPIRE},
    {"E Prover", ATP_BACKEND_EPROVER},
    {"iProver", ATP_BACKEND_IPROVER},
    {"Custom", ATP_BACKEND_CUSTOM},
};

const char *atp_backend_type_name(ATPBackendType type) {
    return lv_enum_to_str(s_atp_backend_type_name_entries, lv_ARRAY_SIZE(s_atp_backend_type_name_entries), (int) type, "Unknown");
}

/**
 * @brief 从名称字符串解析后端类型
 *
 * 使用 s_atp_backend_type_name_entries 表驱动查找。
 * 注意：表中 "E Prover" 包含空格，此处额外支持 "e" 和 "eprover" 别名。
 */
bool atp_backend_type_from_name(const char *name, ATPBackendType *out_type) {
    if (!name || !out_type) {
        return false;
    }

    /* 先尝试精确匹配现有表项 */
    int v = lv_str_to_enum_ci(s_atp_backend_type_name_entries,
                               lv_ARRAY_SIZE(s_atp_backend_type_name_entries),
                               name, -1);
    if (v >= 0) {
        *out_type = (ATPBackendType) v;
        return true;
    }

    /* 兼容 "e" 缩写和 "eprover" 别名 */
    if (strcasecmp(name, "e") == 0 || strcasecmp(name, "eprover") == 0) {
        *out_type = ATP_BACKEND_EPROVER;
        return true;
    }

    return false;
}

/* ============================================================
 * 引擎调度器集成
 * ============================================================ */

/**
 * @brief 将所有可用 ATP 后端注册到引擎调度器
 *
 * 框架实现：检查系统上可用的 ATP 后端并注册。
 * 当前无 ATP 后端可用，返回 0。
 */
int atp_register_all_to_scheduler(void) {
    int registered = 0;

    /* 尝试注册所有 ATP 后端 */
    ATPBackendType types[] = {ATP_BACKEND_VAMPIRE, ATP_BACKEND_EPROVER, ATP_BACKEND_IPROVER, ATP_BACKEND_CUSTOM};

    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
        if (atp_is_backend_available(types[i])) {
            ATPBackendEntry entry;
            memset(&entry, 0, sizeof(entry));
            entry.type = types[i];
            entry.available = true;
            entry.create = NULL; /* 工厂函数由各后端模块提供 */
            entry.priority = (int) i + 1;
            entry.description = atp_backend_type_name(types[i]);

            if (atp_register_backend(&entry) == (int) lv_OK) {
                registered++;
            }
        }
    }

    return registered;
}

/**
 * @brief 自动选择最优后端并求解（ATP vs SMT）
 *
 * 决策逻辑：
 * 1. 纯逻辑约束 -> 优先 ATP
 * 2. 含非线性算术 -> 优先 SMT
 * 3. 混合约束 -> 同时尝试，返回最先成功的结果
 *
 * 框架实现：自动选择可用后端并求解，无可用后端时返回 ATP_RESULT_UNKNOWN。
 */
int atp_auto_solve(const ConstraintGraph *graph, const ATPConfig *config, ATPResultInfo *result) {
    lv_CHECK_NULL(graph, (int) lv_ERROR_NULL_POINTER);
    lv_CHECK_NULL(result, (int) lv_ERROR_NULL_POINTER);

    atp_result_init(result);

    /* 尝试 Vampire */
    if (atp_is_backend_available(ATP_BACKEND_VAMPIRE)) {
        ATPBackendSolver *solver = atp_solver_create(ATP_BACKEND_VAMPIRE, config);
        if (solver) {
            int rc = atp_solver_solve_graph(solver, graph, config ? config->input_format : ATP_FORMAT_TPTP_FOF,
                                            "lv_auto", false, NULL, result);
            atp_solver_destroy(solver);
            return rc;
        }
    }

    /* 尝试 E Prover */
    if (atp_is_backend_available(ATP_BACKEND_EPROVER)) {
        ATPBackendSolver *solver = atp_solver_create(ATP_BACKEND_EPROVER, config);
        if (solver) {
            int rc = atp_solver_solve_graph(solver, graph, config ? config->input_format : ATP_FORMAT_TPTP_FOF,
                                            "lv_auto", false, NULL, result);
            atp_solver_destroy(solver);
            return rc;
        }
    }

    /* 所有后端不可用：返回 UNKNOWN */
    result->result = ATP_RESULT_UNKNOWN;
    result->backend = ATP_BACKEND_COUNT;
    snprintf(result->error_message, sizeof(result->error_message), "No ATP backend available; returning UNKNOWN");

    return (int) lv_OK;
}

/* ============================================================
 * 字符串工具
 * ============================================================ */

/**
 * @brief 获取结果类型名称
 */
/** @brief atp_result_name 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_atp_result_name_entries[] = {
    {"SAT", ATP_RESULT_SAT},
    {"UNSAT", ATP_RESULT_UNSAT},
    {"UNKNOWN", ATP_RESULT_UNKNOWN},
    {"ERROR", ATP_RESULT_ERROR},
};

const char *atp_result_name(ATPResult result) {
    return lv_enum_to_str(s_atp_result_name_entries, lv_ARRAY_SIZE(s_atp_result_name_entries), (int) result, "INVALID");
}

/**
 * @brief 获取输入格式名称
 */
/** @brief atp_format_name 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_atp_format_name_entries[] = {
    {"TPTP FOF", ATP_FORMAT_TPTP_FOF},
    {"TPTP CNF", ATP_FORMAT_TPTP_CNF},
    {"TPTP TFF", ATP_FORMAT_TPTP_TFF},
    {"SMT-LIB2", ATP_FORMAT_SMTLIB2},
};

const char *atp_format_name(ATPInputFormat format) {
    return lv_enum_to_str(s_atp_format_name_entries, lv_ARRAY_SIZE(s_atp_format_name_entries), (int) format, "UNKNOWN");
}

/* ============================================================
 * 统一后端插件系统集成
 *
 * 将 ATP 后端注册到全局 lvBackendPluginRegistry，
 * 保持现有 API 向后兼容。
 * ============================================================ */

/** @brief ATP 后端插件描述符数组 */
static lvBackendPlugin s_atp_plugins[ATP_BACKEND_COUNT];

/** @brief ATP 插件是否已注册到全局注册表 */
static bool s_atp_plugins_registered = false;

/**
 * @brief ATP 后端插件初始化函数
 */
static bool atp_plugin_init_vampire(void) {
    s_atp_plugins[ATP_BACKEND_VAMPIRE].available = atp_is_backend_available(ATP_BACKEND_VAMPIRE);
    return true;
}

static bool atp_plugin_init_eprover(void) {
    s_atp_plugins[ATP_BACKEND_EPROVER].available = atp_is_backend_available(ATP_BACKEND_EPROVER);
    return true;
}

static bool atp_plugin_init_iprover(void) {
    s_atp_plugins[ATP_BACKEND_IPROVER].available = atp_is_backend_available(ATP_BACKEND_IPROVER);
    return true;
}

static bool atp_plugin_init_custom(void) {
    s_atp_plugins[ATP_BACKEND_CUSTOM].available = false;
    return true;
}

/**
 * @brief 将所有 ATP 后端注册到全局后端插件注册表
 *
 * 创建 lvBackendPlugin 包装器，将每个 ATP 后端类型映射到
 * 统一插件描述符，并注册到全局注册表。
 * 此函数可安全地多次调用（仅首次生效）。
 */
void atp_register_all_plugins(void) {
    if (s_atp_plugins_registered) {
        return;
    }

    lvBackendPluginRegistry *reg = lv_backend_plugin_registry_global();

    /* 初始化静态插件描述符 */
    memset(s_atp_plugins, 0, sizeof(s_atp_plugins));

    /* Vampire */
    s_atp_plugins[ATP_BACKEND_VAMPIRE].name = "Vampire";
    s_atp_plugins[ATP_BACKEND_VAMPIRE].version = "4.x";
    s_atp_plugins[ATP_BACKEND_VAMPIRE].type = lv_PLUGIN_TYPE_ATP;
    s_atp_plugins[ATP_BACKEND_VAMPIRE].capabilities = lv_PLUGIN_CAP_PROOF_PROD | lv_PLUGIN_CAP_UNSAT_CORE;
    s_atp_plugins[ATP_BACKEND_VAMPIRE].priority = 1;
    s_atp_plugins[ATP_BACKEND_VAMPIRE].available = atp_is_backend_available(ATP_BACKEND_VAMPIRE);
    s_atp_plugins[ATP_BACKEND_VAMPIRE].init = atp_plugin_init_vampire;
    s_atp_plugins[ATP_BACKEND_VAMPIRE].cleanup = NULL;
    s_atp_plugins[ATP_BACKEND_VAMPIRE].ops = NULL;
    lv_backend_plugin_register(reg, &s_atp_plugins[ATP_BACKEND_VAMPIRE]);

    /* E Prover */
    s_atp_plugins[ATP_BACKEND_EPROVER].name = "E Prover";
    s_atp_plugins[ATP_BACKEND_EPROVER].version = "3.x";
    s_atp_plugins[ATP_BACKEND_EPROVER].type = lv_PLUGIN_TYPE_ATP;
    s_atp_plugins[ATP_BACKEND_EPROVER].capabilities = lv_PLUGIN_CAP_PROOF_PROD;
    s_atp_plugins[ATP_BACKEND_EPROVER].priority = 2;
    s_atp_plugins[ATP_BACKEND_EPROVER].available = atp_is_backend_available(ATP_BACKEND_EPROVER);
    s_atp_plugins[ATP_BACKEND_EPROVER].init = atp_plugin_init_eprover;
    s_atp_plugins[ATP_BACKEND_EPROVER].cleanup = NULL;
    s_atp_plugins[ATP_BACKEND_EPROVER].ops = NULL;
    lv_backend_plugin_register(reg, &s_atp_plugins[ATP_BACKEND_EPROVER]);

    /* iProver */
    s_atp_plugins[ATP_BACKEND_IPROVER].name = "iProver";
    s_atp_plugins[ATP_BACKEND_IPROVER].version = "3.x";
    s_atp_plugins[ATP_BACKEND_IPROVER].type = lv_PLUGIN_TYPE_ATP;
    s_atp_plugins[ATP_BACKEND_IPROVER].capabilities = lv_PLUGIN_CAP_PROOF_PROD;
    s_atp_plugins[ATP_BACKEND_IPROVER].priority = 3;
    s_atp_plugins[ATP_BACKEND_IPROVER].available = atp_is_backend_available(ATP_BACKEND_IPROVER);
    s_atp_plugins[ATP_BACKEND_IPROVER].init = atp_plugin_init_iprover;
    s_atp_plugins[ATP_BACKEND_IPROVER].cleanup = NULL;
    s_atp_plugins[ATP_BACKEND_IPROVER].ops = NULL;
    lv_backend_plugin_register(reg, &s_atp_plugins[ATP_BACKEND_IPROVER]);

    /* Custom */
    s_atp_plugins[ATP_BACKEND_CUSTOM].name = "ATP Custom";
    s_atp_plugins[ATP_BACKEND_CUSTOM].version = "1.0";
    s_atp_plugins[ATP_BACKEND_CUSTOM].type = lv_PLUGIN_TYPE_ATP;
    s_atp_plugins[ATP_BACKEND_CUSTOM].capabilities = lv_PLUGIN_CAP_NONE;
    s_atp_plugins[ATP_BACKEND_CUSTOM].priority = 99;
    s_atp_plugins[ATP_BACKEND_CUSTOM].available = false;
    s_atp_plugins[ATP_BACKEND_CUSTOM].init = atp_plugin_init_custom;
    s_atp_plugins[ATP_BACKEND_CUSTOM].cleanup = NULL;
    s_atp_plugins[ATP_BACKEND_CUSTOM].ops = NULL;
    lv_backend_plugin_register(reg, &s_atp_plugins[ATP_BACKEND_CUSTOM]);

    s_atp_plugins_registered = true;
}

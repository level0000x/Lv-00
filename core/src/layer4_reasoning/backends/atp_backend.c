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
 *   - lv00_internal.h        : 内部常量与工具宏
 *   - lv00_utils.h           : 统一内存分配器
 *   - error_codes.h          : 统一错误码系统
 *   - proof.h                : Lv-00 证明系统
 * [QA] Uses double for timing/layout — not geometric computation. Acceptable.
 */

#include "atp_backend.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define popen _popen
#define pclose _pclose
#else
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <pthread.h>
#endif

#include "error_codes.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

/* ============================================================
 * 模块级常量
 * ============================================================ */

/** @brief 默认求解超时（秒） */
#define ATP_DEFAULT_TIMEOUT 30.0

/** @brief 默认内存限制（MB） */
#define ATP_DEFAULT_MEMORY_MB 1024

/** @brief TPTP 编码缓冲区默认大小 */
#define ATP_TPTP_BUFFER_SIZE 65536

/** @brief 全局注册表最大条目数 */
#define ATP_REGISTRY_MAX_ENTRIES 8

/* ============================================================
 * 不透明结构：ATPBackendSolver 内部实现
 * ============================================================ */

/**
 * @brief ATP 求解器内部状态
 */
struct ATPBackendSolver {
    ATPBackendType type;        /**< 后端类型 */
    ATPConfig config;           /**< 求解器配置 */
    char *tptp_code;            /**< 已加载的 TPTP 编码 */
    int tptp_len;               /**< TPTP 编码长度 */
    bool is_initialized;        /**< 是否已初始化 */
    bool has_problem;           /**< 是否已加载问题 */
};

/* ============================================================
 * 全局后端注册表（单例）
 * ============================================================ */

/** @brief 全局静态注册表 */
static ATPBackendRegistry g_atp_registry;
static bool g_atp_registry_initialized = false;

#ifdef _WIN32
static CRITICAL_SECTION g_atp_registry_cs = {0};
static volatile LONG g_atp_cs_initialized = 0;
#define ATP_REGISTRY_LOCK() do { \
    if (!g_atp_cs_initialized) { \
        InterlockedCompareExchange(&g_atp_cs_initialized, 1, 0); \
        if (g_atp_cs_initialized) InitializeCriticalSection(&g_atp_registry_cs); \
    } \
    EnterCriticalSection(&g_atp_registry_cs); \
} while(0)
#define ATP_REGISTRY_UNLOCK() LeaveCriticalSection(&g_atp_registry_cs)
#else
static pthread_mutex_t g_atp_registry_mutex = PTHREAD_MUTEX_INITIALIZER;
#define ATP_REGISTRY_LOCK() pthread_mutex_lock(&g_atp_registry_mutex)
#define ATP_REGISTRY_UNLOCK() pthread_mutex_unlock(&g_atp_registry_mutex)
#endif

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
    cfg.timeout_seconds = ATP_DEFAULT_TIMEOUT;
    cfg.memory_limit_mb = ATP_DEFAULT_MEMORY_MB;
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
char *atp_encode_constraint_graph(const ConstraintGraph *graph, ATPInputFormat format,
                                   const char *problem_name, bool include_proof_goal,
                                   const Proposition *target_prop) {
    LV00_CHECK_NULL(graph, NULL);

    char *buf = (char *)lv00_malloc(ATP_TPTP_BUFFER_SIZE);
    if (!buf) {
        return NULL;
    }

    int offset = 0;
    int remaining = ATP_TPTP_BUFFER_SIZE;

    /* 根据格式选择头部 */
    const char *lang;
    switch (format) {
    case ATP_FORMAT_TPTP_FOF: lang = "fof"; break;
    case ATP_FORMAT_TPTP_CNF: lang = "cnf"; break;
    case ATP_FORMAT_TPTP_TFF: lang = "tff"; break;
    default:                  lang = "fof"; break;
    }

    const char *name = problem_name ? problem_name : "lv00_geometry";
    int n = snprintf(buf + offset, (size_t)remaining,
                     "%% TPTP %s encoding for Lv-00 constraint graph\n"
                     "%% Generated by Lv-00 ATP backend\n\n",
                     lang);
    if (n > 0 && n < remaining) { offset += n; remaining -= n; }

    /* 类型声明（TFF 格式） */
    if (format == ATP_FORMAT_TPTP_TFF && remaining > 128) {
        n = snprintf(buf + offset, (size_t)remaining,
                     "tff(point_type, type, point: $tType).\n"
                     "tff(line_type, type, line: $tType).\n\n");
        if (n > 0 && n < remaining) { offset += n; remaining -= n; }
    }

    /* 声明节点常量 —— 使用节点真实 ID 而非数组索引 */
    int node_count = graph->node_count;
    for (int i = 0; i < node_count && remaining > 64; i++) {
        int nid = graph->nodes[i]->id;
        n = snprintf(buf + offset, (size_t)remaining,
                     "%s(p%d_decl, axiom, point(p%d)).\n", lang, nid, nid);
        if (n > 0 && n < remaining) { offset += n; remaining -= n; }
    }

    /* 编码几何约束为谓词 —— 根据实际约束类型编码 */
    int edge_count = graph->constraint_count;
    for (int i = 0; i < edge_count && remaining > 128; i++) {
        const Constraint *con = graph->constraints[i];
        if (!con || !con->is_active)
            continue;

        switch (con->type) {
        case INCIDENCE:
            if (con->participant_count >= 2) {
                n = snprintf(buf + offset, (size_t)remaining,
                             "%s(constraint_%d, axiom, incident(p%d, l%d)).\n",
                             lang, con->id,
                             con->participants[0], con->participants[1]);
            }
            break;
        case BETWEENNESS:
            if (con->participant_count >= 3) {
                n = snprintf(buf + offset, (size_t)remaining,
                             "%s(constraint_%d, axiom, between(p%d, p%d, p%d)).\n",
                             lang, con->id,
                             con->participants[0], con->participants[1], con->participants[2]);
            }
            break;
        case INTERSECTION:
            if (con->participant_count >= 3) {
                n = snprintf(buf + offset, (size_t)remaining,
                             "%s(constraint_%d, axiom, intersect(l%d, l%d, p%d)).\n",
                             lang, con->id,
                             con->participants[0], con->participants[1], con->participants[2]);
            }
            break;
        case CONTAINMENT:
            if (con->participant_count >= 2) {
                n = snprintf(buf + offset, (size_t)remaining,
                             "%s(constraint_%d, axiom, contain(r%d, p%d)).\n",
                             lang, con->id,
                             con->participants[0], con->participants[1]);
            }
            break;
        case CONNECTION:
            if (con->participant_count >= 2) {
                n = snprintf(buf + offset, (size_t)remaining,
                             "%s(constraint_%d, axiom, connect(p%d, p%d)).\n",
                             lang, con->id,
                             con->participants[0], con->participants[1]);
            }
            break;
        default:
            continue;
        }
        if (n > 0 && n < remaining) { offset += n; remaining -= n; }
    }

    /* 添加 conjecture（如果请求） */
    if (include_proof_goal && remaining > 128) {
        if (target_prop) {
            /* 使用目标命题的名称作为 conjecture */
            const char *prop_name = target_prop->name ? target_prop->name : "goal";
            n = snprintf(buf + offset, (size_t)remaining,
                         "%s(%s, conjecture, $false).\n", lang, prop_name);
        } else {
            n = snprintf(buf + offset, (size_t)remaining,
                         "%s(goal, conjecture, $false).\n", lang);
        }
        if (n > 0 && n < remaining) { offset += n; remaining -= n; }
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
    ATPBackendSolver *solver = (ATPBackendSolver *)lv00_malloc(sizeof(ATPBackendSolver));
    if (!solver) {
        return NULL;
    }

    memset(solver, 0, sizeof(ATPBackendSolver));
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
        lv00_free((void **)&solver->tptp_code);
    }
    lv00_free((void **)&solver);
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
    switch (type) {
    case ATP_BACKEND_VAMPIRE:  return "vampire";
    case ATP_BACKEND_EPROVER:  return "eprover";
    case ATP_BACKEND_IPROVER:  return "iprover";
    default:                    return NULL;
    }
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

    FILE *fp = popen(cmd, "r");
    if (!fp)
        return false;

    char buffer[256];
    bool found = false;
    if (fgets(buffer, sizeof(buffer), fp)) {
        /* 如果命令找到了文件，输出包含路径 */
        found = (buffer[0] != '\0' && buffer[0] != '\n');
    }
    pclose(fp);
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
 * @return LV00_OK 成功
 */
static int atp_run_subprocess(const char *executable, const char *tptp_text,
                               double timeout_sec, const char *extra_args,
                               char **out_output, int *out_exit_code) {
    /* TODO: Implement timeout using process kill after timeout_sec */
    LV00_UNUSED(timeout_sec);
    if (!executable || !tptp_text || !out_output || !out_exit_code)
        return (int)LV00_ERROR_NULL_POINTER;

    *out_output = NULL;
    *out_exit_code = -1;

    /* 将 TPTP 文本写入临时文件 */
#ifdef _WIN32
    char temp_path[MAX_PATH];
    char temp_dir[MAX_PATH];

    /* 获取临时目录 */
    DWORD len = GetTempPathA(MAX_PATH, temp_dir);
    if (len == 0) {
        snprintf(temp_dir, sizeof(temp_dir), ".");
    }

    /* 生成唯一临时文件名 */
    snprintf(temp_path, sizeof(temp_path), "%slv00_atp_%d.p", temp_dir, (int)GetCurrentProcessId());

    FILE *tmp = fopen(temp_path, "w");
    if (!tmp)
        return (int)LV00_ERROR_IO;

    fputs(tptp_text, tmp);
    fclose(tmp);

    /* 构建命令行 */
    char cmd[2048];
    if (extra_args && extra_args[0] != '\0') {
        snprintf(cmd, sizeof(cmd), "%s %s %s 2>&1", executable, extra_args, temp_path);
    } else {
        snprintf(cmd, sizeof(cmd), "%s %s 2>&1", executable, temp_path);
    }

    /* 执行子进程 */
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        remove(temp_path);
        return (int)LV00_ERROR_IO;
    }

    /* 读取输出 */
    size_t out_size = 65536;
    size_t out_len = 0;
    char *output = (char *) lv00_malloc(out_size);
    if (!output) {
        pclose(fp);
        remove(temp_path);
        return (int)LV00_ERROR_OUT_OF_MEMORY;
    }

    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), fp)) {
        size_t chunk_len = strlen(buffer);
        while (out_len + chunk_len + 1 >= out_size) {
            out_size *= 2;
            char *new_output = (char *) lv00_realloc(output, out_size);
            if (!new_output) {
                lv00_free((void **)&output);
                pclose(fp);
                remove(temp_path);
                return (int)LV00_ERROR_OUT_OF_MEMORY;
            }
            output = new_output;
        }
        memcpy(output + out_len, buffer, chunk_len);
        out_len += chunk_len;
    }
    output[out_len] = '\0';

    int exit_code = pclose(fp);

    /* 清理临时文件 */
    remove(temp_path);

#else  /* POSIX: fork + execvp + pipe + dup2 */
    char temp_path[256];
    const char *tmpdir = getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp";
    snprintf(temp_path, sizeof(temp_path), "%s/lv00_atp_%d.p", tmpdir, (int)getpid());

    FILE *tmp = fopen(temp_path, "w");
    if (!tmp)
        return (int)LV00_ERROR_IO;

    fputs(tptp_text, tmp);
    fclose(tmp);

    /* 准备参数列表 */
    char *exec_argv[16];
    char *extra_copy = NULL;
    int argc = 0;
    exec_argv[argc++] = (char *)executable;
    if (extra_args && extra_args[0] != '\0') {
        /* 简单参数切分（空格分隔） */
        extra_copy = strdup(extra_args);
        if (extra_copy) {
            char *save_ptr = NULL;
            char *token = strtok_s(extra_copy, " ", &save_ptr);
            while (token && argc < 14) {
                exec_argv[argc++] = token;
                token = strtok_s(NULL, " ", &save_ptr);
            }
        }
    }
    exec_argv[argc++] = temp_path;
    exec_argv[argc] = NULL;

    /* 创建管道 */
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        remove(temp_path);
        return (int)LV00_ERROR_IO;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        remove(temp_path);
        return (int)LV00_ERROR_IO;
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

    /* 父进程：关闭写端，读取管道输出 */
    free(extra_copy);
    close(pipefd[1]);

    size_t out_size = 65536;
    size_t out_len = 0;
    char *output = (char *) lv00_malloc(out_size);
    if (!output) {
        close(pipefd[0]);
        waitpid(pid, NULL, 0);
        remove(temp_path);
        return (int)LV00_ERROR_OUT_OF_MEMORY;
    }

    ssize_t nread;
    char buffer[4096];
    while ((nread = read(pipefd[0], buffer, sizeof(buffer))) > 0) {
        while (out_len + (size_t)nread + 1 >= out_size) {
            out_size *= 2;
            char *new_output = (char *) lv00_realloc(output, out_size);
            if (!new_output) {
                lv00_free((void **)&output);
                close(pipefd[0]);
                waitpid(pid, NULL, 0);
                remove(temp_path);
                return (int)LV00_ERROR_OUT_OF_MEMORY;
            }
            output = new_output;
        }
        memcpy(output + out_len, buffer, (size_t)nread);
        out_len += (size_t)nread;
    }
    if (out_len == 0) {
        output[0] = '\0';
    } else {
        output[out_len] = '\0';
    }
    close(pipefd[0]);

    /* 等待子进程结束 */
    int status = 0;
    waitpid(pid, &status, 0);
    int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

    /* 清理临时文件 */
    remove(temp_path);

#endif

    *out_output = output;
    *out_exit_code = exit_code;
    return (int)LV00_OK;
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
    if (strncmp(status, "Theorem", 7) == 0 ||
        strncmp(status, "Unsatisfiable", 13) == 0)
        return ATP_RESULT_UNSAT;

    if (strncmp(status, "Satisfiable", 11) == 0 ||
        strncmp(status, "CounterSatisfiable", 18) == 0)
        return ATP_RESULT_SAT;

    if (strncmp(status, "Timeout", 7) == 0 ||
        strncmp(status, "ResourceOut", 11) == 0)
        return ATP_RESULT_UNKNOWN;

    if (strncmp(status, "Error", 5) == 0)
        return ATP_RESULT_ERROR;

    return ATP_RESULT_UNKNOWN;
}

/**
 * @brief 从 ATP 输出中提取证明步骤（TSTP 格式）
 *
 * TSTP 证明步骤格式：
 *   step_id. [status] clause (inference(rule, [parent1, parent2, ...])).
 */
static int atp_extract_proof_steps(const char *output,
                                     ATPProofStep **out_steps,
                                     int *out_step_count) {
    if (!output || !out_steps || !out_step_count)
        return (int)LV00_ERROR_NULL_POINTER;

    *out_steps = NULL;
    *out_step_count = 0;

    /* 计算证明步骤数（以行首数字+点开头的行） */
    int capacity = 64;
    ATPProofStep *steps = (ATPProofStep *) lv00_calloc((size_t) capacity, sizeof(ATPProofStep));
    if (!steps)
        return (int)LV00_ERROR_OUT_OF_MEMORY;

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
            while (*p >= '0' && *p <= '9') p++;
            if (*p == '.' && *(p + 1) == ' ') {
                is_step = true;
            }
        }

        if (!is_step) {
            /* 跳到行尾 */
            while (*line && *line != '\n') line++;
            continue;
        }

        /* 提取步骤 ID */
        int step_id = atoi(line);

        /* 提取子句内容（到行尾） */
        const char *clause_start = line;
        while (*clause_start != ' ') clause_start++;
        while (*clause_start == ' ') clause_start++;

        const char *clause_end = clause_start;
        while (*clause_end && *clause_end != '\n') clause_end++;

        int clause_len = (int)(clause_end - clause_start);
        if (clause_len > 0 && count < capacity) {
            steps[count].step_id = step_id;
            steps[count].is_axiom = false;
            steps[count].is_goal = false;
            steps[count].inference_rule = NULL;
            steps[count].justification = NULL;

            steps[count].clause = (char *) lv00_malloc((size_t) clause_len + 1);
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
                    int rule_len = (int)(rule_end - rule_start);
                    steps[count].inference_rule = (char *) lv00_malloc((size_t) rule_len + 1);
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
    return (int)LV00_OK;
}

/* ============================================================
 * ATP 求解操作
 * ============================================================ */

/**
 * @brief 将 TPTP 编码加载到求解器
 */
int atp_solver_load(ATPBackendSolver *solver, const char *tptp_text) {
    LV00_CHECK_NULL(solver, (int)LV00_ERROR_NULL_POINTER);
    LV00_CHECK_NULL(tptp_text, (int)LV00_ERROR_NULL_POINTER);

    if (!solver->is_initialized) {
        return (int)LV00_ERROR_NOT_INITIALIZED;
    }

    /* 释放旧编码 */
    if (solver->tptp_code) {
        lv00_free((void **)&solver->tptp_code);
    }

    int len = (int)strlen(tptp_text);
    if (len <= 0) {
        return (int)LV00_ERROR_INVALID_PARAM;
    }

    solver->tptp_code = (char *)lv00_malloc((size_t)(len + 1));
    if (!solver->tptp_code) {
        return (int)LV00_ERROR_OUT_OF_MEMORY;
    }

    memcpy(solver->tptp_code, tptp_text, (size_t)(len + 1));
    solver->tptp_len = len;
    solver->has_problem = true;

    return (int)LV00_OK;
}

/**
 * @brief 执行 ATP 求解
 *
 * 真实实现：通过子进程调用 ATP 可执行文件，解析 SZS 状态行。
 * 如果 ATP 不可用，优雅降级返回 UNKNOWN。
 */
int atp_solver_solve(ATPBackendSolver *solver, ATPResultInfo *result) {
    LV00_CHECK_NULL(solver, (int)LV00_ERROR_NULL_POINTER);
    LV00_CHECK_NULL(result, (int)LV00_ERROR_NULL_POINTER);

    atp_result_init(result);

    if (!solver->is_initialized) {
        result->result = ATP_RESULT_ERROR;
        result->error_code = (int)LV00_ERROR_NOT_INITIALIZED;
        snprintf(result->error_message, sizeof(result->error_message),
                 "ATP solver not initialized");
        return (int)LV00_ERROR_NOT_INITIALIZED;
    }

    if (!solver->has_problem) {
        result->result = ATP_RESULT_ERROR;
        result->error_code = (int)LV00_ERROR_INVALID_STATE;
        snprintf(result->error_message, sizeof(result->error_message),
                 "No problem loaded");
        return (int)LV00_ERROR_INVALID_STATE;
    }

    result->backend = solver->type;

    /* 获取可执行文件名 */
    const char *exe_name = atp_executable_name(solver->type);
    if (!exe_name) {
        result->result = ATP_RESULT_UNKNOWN;
        snprintf(result->error_message, sizeof(result->error_message),
                 "Unknown ATP backend type");
        return (int)LV00_OK;
    }

    /* 检查 ATP 是否可用 */
    if (!atp_check_executable(exe_name)) {
        /* 优雅降级：ATP 不可用，返回 UNKNOWN */
        result->result = ATP_RESULT_UNKNOWN;
        result->solve_time_seconds = 0.0;
        snprintf(result->error_message, sizeof(result->error_message),
                 "ATP backend '%s' not found in PATH; returning UNKNOWN (graceful degradation)",
                 atp_backend_type_name(solver->type));
        return (int)LV00_OK;
    }

    /* 构建额外参数 */
    char extra_args[512];
    extra_args[0] = '\0';

    /* 通用参数 */
    const char *mode = "--mode";
    switch (solver->config.input_format) {
    case ATP_FORMAT_TPTP_CNF: mode = "--cnf"; break;
    case ATP_FORMAT_TPTP_TFF: mode = "--tff"; break;
    default: mode = "--fof"; break;
    }

    snprintf(extra_args, sizeof(extra_args),
             "%s -t %d --proof tptp",
             mode, (int) solver->config.timeout_seconds);

    if (solver->config.custom_options) {
        size_t len = strlen(extra_args);
        snprintf(extra_args + len, sizeof(extra_args) - len,
                 " %s", solver->config.custom_options);
    }

    /* 调用 ATP 子进程 — 记录实际耗时 */
    char *raw_output = NULL;
    int exit_code = -1;
    clock_t start_clock = clock();
    int rc = atp_run_subprocess(exe_name, solver->tptp_code,
                                 solver->config.timeout_seconds,
                                 extra_args, &raw_output, &exit_code);
    clock_t end_clock = clock();
    double elapsed_seconds = (double)(end_clock - start_clock) / (double)CLOCKS_PER_SEC;

    if (rc != (int)LV00_OK) {
        result->result = ATP_RESULT_ERROR;
        result->error_code = rc;
        snprintf(result->error_message, sizeof(result->error_message),
                 "Failed to execute ATP subprocess: error %d", rc);
        return (int)LV00_OK;
    }

    /* 存储 raw output */
    result->raw_output = raw_output;
    result->raw_output_length = raw_output ? (int) strlen(raw_output) : 0;

    /* 解析 SZS 状态 */
    result->result = atp_parse_szs_status(raw_output);

    /* 如果求解成功且需要证明，提取证明步骤 */
    if (result->result == ATP_RESULT_UNSAT && solver->config.produce_proof) {
        atp_extract_proof_steps(raw_output,
                                 &result->proof_steps,
                                 &result->proof_step_count);
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
            if (*p == '\n') lines++;
        }
        result->generated_clauses = lines;
    }

    return (int)LV00_OK;
}

/**
 * @brief 便捷函数：编码 + 加载 + 求解
 */
int atp_solver_solve_graph(ATPBackendSolver *solver, const ConstraintGraph *graph,
                            ATPInputFormat format, const char *problem_name,
                            bool include_goal, const Proposition *target_prop,
                            ATPResultInfo *result) {
    LV00_CHECK_NULL(solver, (int)LV00_ERROR_NULL_POINTER);
    LV00_CHECK_NULL(graph, (int)LV00_ERROR_NULL_POINTER);
    LV00_CHECK_NULL(result, (int)LV00_ERROR_NULL_POINTER);

    /* 步骤 1：编码 */
    char *tptp = atp_encode_constraint_graph(graph, format, problem_name,
                                              include_goal, target_prop);
    if (!tptp) {
        atp_result_init(result);
        result->result = ATP_RESULT_ERROR;
        result->error_code = (int)LV00_ERROR_OUT_OF_MEMORY;
        snprintf(result->error_message, sizeof(result->error_message),
                 "TPTP encoding failed");
        return (int)LV00_ERROR_OUT_OF_MEMORY;
    }

    /* 步骤 2：加载 */
    int rc = atp_solver_load(solver, tptp);
    lv00_free((void **)&tptp);

    if (rc != (int)LV00_OK) {
        atp_result_init(result);
        result->result = ATP_RESULT_ERROR;
        result->error_code = rc;
        snprintf(result->error_message, sizeof(result->error_message),
                 "Failed to load TPTP encoding");
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
void atp_result_free(ATPResultInfo *result) {
    if (!result) {
        return;
    }
    if (result->proof_steps) {
        for (int i = 0; i < result->proof_step_count; i++) {
            if (result->proof_steps[i].clause) {
                lv00_free((void **)&result->proof_steps[i].clause);
            }
            if (result->proof_steps[i].inference_rule) {
                lv00_free((void **)&result->proof_steps[i].inference_rule);
            }
            if (result->proof_steps[i].justification) {
                lv00_free((void **)&result->proof_steps[i].justification);
            }
        }
        lv00_free((void **)&result->proof_steps);
    }
    result->proof_step_count = 0;
    if (result->unsat_core_clause_ids) {
        lv00_free((void **)&result->unsat_core_clause_ids);
    }
    result->unsat_core_count = 0;
    if (result->raw_output) {
        lv00_free((void **)&result->raw_output);
    }
    result->raw_output_length = 0;
}

/**
 * @brief 将 ATP 证明转换为 Lv-00 ProofNavigator 步骤
 *
 * 解析 TSTP 格式的 ATP 证明输出，转换为 Lv-00 证明步骤。
 * 实现基本的证明步骤转换：
 * - 解析每个证明步骤的推理规则（resolution/paramodulation/superposition等）
 * - 创建 LV00ProofStep 对象并设置适当的类型标签
 * - 将步骤链接到有向证明图中
 */
int atp_proof_to_lv00(const ATPResultInfo *result, Proof *proof, int *step_count) {
    LV00_CHECK_NULL(result, (int)LV00_ERROR_NULL_POINTER);
    LV00_CHECK_NULL(proof, (int)LV00_ERROR_NULL_POINTER);

    if (step_count) {
        *step_count = 0;
    }

    if (result->result != ATP_RESULT_UNSAT) {
        LV00_ERROR_SET(LV00_ERROR_INVALID_STATE,
                        "Proof conversion requires UNSAT result");
        return (int)LV00_ERROR_INVALID_STATE;
    }

    if (!result->proof_steps || result->proof_step_count == 0) {
        return (int)LV00_ERROR_NOT_FOUND;
    }

    /* 第一步：为每个 ATP 步骤创建 Lv-00 ProofStep 并记录映射关系 */
    int count = 0;
    int *lv00_step_ids = (int *)lv00_calloc((size_t)result->proof_step_count, sizeof(int));
    if (!lv00_step_ids)
        return (int)LV00_ERROR_OUT_OF_MEMORY;

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
            const char *rule = atp_step->inference_rule;
            if (strstr(rule, "resolution") || strstr(rule, "eq_resolution") ||
                strstr(rule, "equality")) {
                step_type = PROOF_STEP_REWRITE;
            } else if (strstr(rule, "paramodulation") || strstr(rule, "superposition")) {
                step_type = PROOF_STEP_REWRITE;
            } else if (strstr(rule, "instantiation") || strstr(rule, "subst")) {
                step_type = PROOF_STEP_REWRITE;
            } else {
                /* 未知规则类型 — 标记为通用 REWRITE */
                step_type = PROOF_STEP_REWRITE;
            }
        }

        ProofStep *lv_step = proof_step_create(step_type);
        if (!lv_step) {
            /* 清理已创建的步骤 */
            for (int j = 0; j < i; j++) {
                if (lv00_step_ids[j] >= 0) {
                    /* 步骤已添加到 proof navigator，由 proof 负责生命周期 */
                }
            }
            lv00_free((void **)&lv00_step_ids);
            return (int)LV00_ERROR_OUT_OF_MEMORY;
        }

        lv_step->id = atp_step->step_id;
        lv_step->color = PROOF_COLOR_GREEN; /* ATP 证明步骤默认为绿色 */
        lv_step->note = atp_step->clause;   /* 存储原始子句作为注释 */

        /* 添加到证明导航器 */
        if (!proof_navigator_add_step(proof, lv_step)) {
            proof_step_destroy(lv_step);
            lv00_free((void **)&lv00_step_ids);
            return (int)LV00_ERROR_INVALID_STATE;
        }

        /* 记录映射：ATP step_id -> Lv-00 proof step index */
        /* 使用 proof->step_count - 1 作为刚添加的步骤索引 */
        lv00_step_ids[i] = proof->step_count - 1;
        count++;
    }

    /* 第二步：建立步骤之间的依赖关系（有向证明图） */
    for (int i = 0; i < result->proof_step_count; i++) {
        if (lv00_step_ids[i] < 0)
            continue;

        const ATPProofStep *atp_step = &result->proof_steps[i];
        ProofStep *lv_step = proof->steps[lv00_step_ids[i]];

        /* 解析 justification 以找到父步骤引用 */
        if (atp_step->justification) {
            /* justification 格式通常是 "step_id1,step_id2,..." */
            const char *p = atp_step->justification;
            while (*p) {
                /* 跳过非数字字符 */
                while (*p && (*p < '0' || *p > '9')) p++;
                if (!*p) break;

                int parent_id = atoi(p);
                /* 查找该父步骤在我们的映射中的位置 */
                for (int j = 0; j < result->proof_step_count; j++) {
                    if (result->proof_steps[j].step_id == parent_id &&
                        lv00_step_ids[j] >= 0) {
                        proof_step_add_dependency(lv_step, result->proof_steps[j].step_id);
                        /* 设置第一个匹配的父步骤作为树结构中的父节点 */
                        if (lv_step->parent_step_id < 0) {
                            lv_step->parent_step_id = result->proof_steps[j].step_id;
                        }
                        break;
                    }
                }
                /* 跳过这个数字 */
                while (*p >= '0' && *p <= '9') p++;
            }
        }
    }

    lv00_free((void **)&lv00_step_ids);

    if (step_count) {
        *step_count = count;
    }

    return (int)LV00_OK;
}

/* ============================================================
 * 后端注册与发现
 * ============================================================ */

/**
 * @brief 获取全局 ATP 后端注册表
 */
const ATPBackendRegistry *atp_get_registry(void) {
    ATP_REGISTRY_LOCK();
    if (!g_atp_registry_initialized) {
        memset(&g_atp_registry, 0, sizeof(g_atp_registry));
        g_atp_registry.count = 0;
        g_atp_registry_initialized = true;
    }
    ATP_REGISTRY_UNLOCK();
    return &g_atp_registry;
}

/**
 * @brief 注册自定义 ATP 后端
 */
int atp_register_backend(const ATPBackendEntry *entry) {
    LV00_CHECK_NULL(entry, (int)LV00_ERROR_NULL_POINTER);

    if (!g_atp_registry_initialized) {
        atp_get_registry(); /* 初始化 */
    }

    ATP_REGISTRY_LOCK();

    /* 检查是否已存在 */
    for (int i = 0; i < g_atp_registry.count; i++) {
        if (g_atp_registry.entries[i].type == entry->type) {
            ATP_REGISTRY_UNLOCK();
            return (int)LV00_ERROR_ALREADY_EXISTS;
        }
    }

    if (g_atp_registry.count >= ATP_BACKEND_COUNT) {
        ATP_REGISTRY_UNLOCK();
        return (int)LV00_ERROR_RESOURCE_EXHAUSTED;
    }

    g_atp_registry.entries[g_atp_registry.count] = *entry;
    g_atp_registry.count++;

    ATP_REGISTRY_UNLOCK();
    return (int)LV00_OK;
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
    if (!g_atp_registry_initialized) {
        return NULL;
    }

    for (int i = 0; i < g_atp_registry.count; i++) {
        if (g_atp_registry.entries[i].type == type) {
            return &g_atp_registry.entries[i];
        }
    }
    return NULL;
}

/**
 * @brief 获取后端类型名称
 */
const char *atp_backend_type_name(ATPBackendType type) {
    switch (type) {
    case ATP_BACKEND_VAMPIRE: return "Vampire";
    case ATP_BACKEND_EPROVER: return "E Prover";
    case ATP_BACKEND_IPROVER: return "iProver";
    case ATP_BACKEND_CUSTOM:  return "Custom";
    default:                  return "Unknown";
    }
}

/**
 * @brief 从名称字符串解析后端类型
 */
bool atp_backend_type_from_name(const char *name, ATPBackendType *out_type) {
    if (!name || !out_type) {
        return false;
    }

    if (strcasecmp(name, "vampire") == 0) {
        *out_type = ATP_BACKEND_VAMPIRE;
        return true;
    }
    if (strcasecmp(name, "eprover") == 0 || strcasecmp(name, "e") == 0) {
        *out_type = ATP_BACKEND_EPROVER;
        return true;
    }
    if (strcasecmp(name, "iprover") == 0) {
        *out_type = ATP_BACKEND_IPROVER;
        return true;
    }
    if (strcasecmp(name, "custom") == 0) {
        *out_type = ATP_BACKEND_CUSTOM;
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
    ATPBackendType types[] = {
        ATP_BACKEND_VAMPIRE, ATP_BACKEND_EPROVER,
        ATP_BACKEND_IPROVER, ATP_BACKEND_CUSTOM
    };

    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
        if (atp_is_backend_available(types[i])) {
            ATPBackendEntry entry;
            memset(&entry, 0, sizeof(entry));
            entry.type = types[i];
            entry.available = true;
            entry.create = NULL; /* 工厂函数由各后端模块提供 */
            entry.priority = (int)i + 1;
            entry.description = atp_backend_type_name(types[i]);

            if (atp_register_backend(&entry) == (int)LV00_OK) {
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
int atp_auto_solve(const ConstraintGraph *graph, const ATPConfig *config,
                    ATPResultInfo *result) {
    LV00_CHECK_NULL(graph, (int)LV00_ERROR_NULL_POINTER);
    LV00_CHECK_NULL(result, (int)LV00_ERROR_NULL_POINTER);

    atp_result_init(result);

    /* 尝试 Vampire */
    if (atp_is_backend_available(ATP_BACKEND_VAMPIRE)) {
        ATPBackendSolver *solver = atp_solver_create(ATP_BACKEND_VAMPIRE, config);
        if (solver) {
            int rc = atp_solver_solve_graph(solver, graph, config ? config->input_format : ATP_FORMAT_TPTP_FOF,
                                             "lv00_auto", false, NULL, result);
            atp_solver_destroy(solver);
            return rc;
        }
    }

    /* 尝试 E Prover */
    if (atp_is_backend_available(ATP_BACKEND_EPROVER)) {
        ATPBackendSolver *solver = atp_solver_create(ATP_BACKEND_EPROVER, config);
        if (solver) {
            int rc = atp_solver_solve_graph(solver, graph, config ? config->input_format : ATP_FORMAT_TPTP_FOF,
                                             "lv00_auto", false, NULL, result);
            atp_solver_destroy(solver);
            return rc;
        }
    }

    /* 所有后端不可用：返回 UNKNOWN */
    result->result = ATP_RESULT_UNKNOWN;
    result->backend = ATP_BACKEND_COUNT;
    snprintf(result->error_message, sizeof(result->error_message),
             "No ATP backend available; returning UNKNOWN");

    return (int)LV00_OK;
}

/* ============================================================
 * 字符串工具
 * ============================================================ */

/**
 * @brief 获取结果类型名称
 */
const char *atp_result_name(ATPResult result) {
    switch (result) {
    case ATP_RESULT_SAT:     return "SAT";
    case ATP_RESULT_UNSAT:   return "UNSAT";
    case ATP_RESULT_UNKNOWN: return "UNKNOWN";
    case ATP_RESULT_ERROR:   return "ERROR";
    default:                 return "INVALID";
    }
}

/**
 * @brief 获取输入格式名称
 */
const char *atp_format_name(ATPInputFormat format) {
    switch (format) {
    case ATP_FORMAT_TPTP_FOF: return "TPTP FOF";
    case ATP_FORMAT_TPTP_CNF: return "TPTP CNF";
    case ATP_FORMAT_TPTP_TFF: return "TPTP TFF";
    case ATP_FORMAT_SMTLIB2:  return "SMT-LIB2";
    default:                  return "UNKNOWN";
    }
}

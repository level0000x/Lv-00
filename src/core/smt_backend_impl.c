/**
 * @file smt_backend_impl.c
 * @brief SMT 后端抽象层实现 —— 多引擎 SMT 求解器的框架与桩代码
 *
 * @details 本模块实现 smt_backend.h 中声明的所有 SMT 后端接口。
 *          设计参考 polymake 的多后端架构，提供与求解器无关的统一接口。
 *
 *          当前版本为框架实现（桩代码），所有后端创建函数返回未链接标记，
 *          求解操作返回 SMT_RESULT_UNKNOWN。当链接真正的 Z3/cvc5/Singular 库后，
 *          由各后端的工厂函数（smt_z3.c、smt_cvc5.c 等）接管实际计算。
 *
 *          编码管线：
 *          1. smtencode_constraint_graph_to_smtlib2()  约束图 -> SMT-LIB2
 *          2. smtsolver_encode()                       SMT-LIB2 -> 后端原生表示
 *          3. smtsolver_check()                        执行求解
 *          4. smtsolver_decode_result()                解析结果 -> SMTSolverResult
 *
 * @author Lv-00 Project
 * @version 3.2.0
 * @date 2026-05-24
 *
 * @dependencies
 *   - smt_backend.h          : SMT 后端公共接口
 *   - lv00_internal.h        : 内部常量与工具宏
 *   - lv00_utils.h           : 统一内存分配器
 *   - error_codes.h          : 统一错误码系统
 */

#include "smt_backend.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error_codes.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

/* ============================================================
 * 模块级常量
 * ============================================================ */

/** @brief SMT-LIB2 输出缓冲区默认大小 */
#define SMTLIB2_DEFAULT_BUFFER 65536

/** @brief 默认求解超时（毫秒） */
#define SMT_DEFAULT_TIMEOUT_MS 30000

/** @brief 默认内存限制（MB） */
#define SMT_DEFAULT_MEMORY_MB 1024

/* ============================================================
 * 不透明结构：SMTSolver 内部实现
 * ============================================================ */

/**
 * @brief SMT 求解器内部状态
 */
struct SMTSolver {
    SolverBackendType type;           /**< 后端类型 */
    SMTSolverConfig config;           /**< 求解器配置 */
    SMTErrorCode last_error;          /**< 最近错误码 */
    char last_error_msg[512];         /**< 最近错误消息 */
    char *encoded_formula;            /**< 已编码的 SMT-LIB2 脚本 */
    int encoded_len;                  /**< 编码长度 */
    bool is_initialized;              /**< 是否已初始化 */
    bool has_assertions;              /**< 是否有待求解的断言 */
};

/* ============================================================
 * 全局后端注册表（单例）
 * ============================================================ */

/** @brief 全局注册表实例 */
static SMTBackendRegistry g_smt_registry;
static bool g_smt_registry_initialized = false;

/* ============================================================
 * 默认配置
 * ============================================================ */

/**
 * @brief 创建并返回默认的求解器配置
 *
 * 基于后端类型选择适当的默认值。所有后端共享通用的基础配置，
 * 特定后端可通过 custom_config 传入私有参数。
 */
const SMTSolverConfig *smtsolver_default_config(SolverBackendType type) {
    static SMTSolverConfig defaults[COUNT];
    static bool initialized = false;

    if (!initialized) {
        for (int i = 0; i < COUNT; i++) {
            defaults[i].timeout_ms = SMT_DEFAULT_TIMEOUT_MS;
            defaults[i].memory_limit_mb = SMT_DEFAULT_MEMORY_MB;
            defaults[i].logic = SMT_LOGIC_AUTO;
            defaults[i].produce_models = true;
            defaults[i].produce_unsat_cores = false;
            defaults[i].produce_proofs = false;
            defaults[i].incremental = false;
            defaults[i].random_seed = -1;
            defaults[i].verbosity = 0;
            defaults[i].custom_config = NULL;
        }
        /* Groebner 后端使用线性算术 */
        defaults[GROEBNER].logic = SMT_LOGIC_QF_LRA;
        initialized = true;
    }

    if (type >= COUNT) {
        return &defaults[GROEBNER];
    }
    return &defaults[type];
}

/* ============================================================
 * 后端生命周期管理
 * ============================================================ */

/**
 * @brief 创建 SMT 求解器实例
 *
 * 根据后端类型创建求解器句柄。当前所有后端均为未链接桩，
 * 设置 SMT_ERROR_BACKEND_UNAVAILABLE 但不阻止创建句柄。
 * 仅 GROEBNER 后端标记为可用（使用内置实现）。
 */
SMTSolver *smtsolver_create(SolverBackendType type, const SMTSolverConfig *config) {
    SMTSolver *solver = (SMTSolver *)lv00_malloc(sizeof(SMTSolver));
    if (!solver) {
        return NULL;
    }

    memset(solver, 0, sizeof(SMTSolver));
    solver->type = type;
    solver->is_initialized = true;
    solver->has_assertions = false;
    solver->last_error = SMT_ERROR_NONE;
    solver->last_error_msg[0] = '\0';

    /* 使用提供的配置或默认配置 */
    if (config) {
        solver->config = *config;
    } else {
        const SMTSolverConfig *def = smtsolver_default_config(type);
        if (def) {
            solver->config = *def;
        }
    }

    /* 检查后端可用性 */
    if (!smtsolver_is_backend_available(type)) {
        solver->last_error = SMT_ERROR_BACKEND_UNAVAILABLE;
        snprintf(solver->last_error_msg, sizeof(solver->last_error_msg),
                 "Backend '%s' is not available (not linked)",
                 smtsolver_backend_type_name(type));
    }

    return solver;
}

/**
 * @brief 销毁 SMT 求解器实例
 */
void smtsolver_destroy(SMTSolver *solver) {
    if (!solver) {
        return;
    }
    if (solver->encoded_formula) {
        lv00_free((void **)&solver->encoded_formula);
    }
    lv00_free((void **)&solver);
}

/**
 * @brief 获取求解器后端类型
 */
SolverBackendType smtsolver_get_type(const SMTSolver *solver) {
    if (!solver) {
        return COUNT;
    }
    return solver->type;
}

/**
 * @brief 获取最近错误码
 */
SMTErrorCode smtsolver_get_last_error(const SMTSolver *solver) {
    if (!solver) {
        return SMT_ERROR_NONE;
    }
    return solver->last_error;
}

/**
 * @brief 获取最近错误消息
 */
const char *smtsolver_get_last_error_message(const SMTSolver *solver) {
    if (!solver) {
        return "null solver";
    }
    if (solver->last_error_msg[0] == '\0') {
        return "";
    }
    return solver->last_error_msg;
}

/* ============================================================
 * 约束图 -> SMT-LIB2 编码
 * ============================================================ */

/**
 * @brief 将约束图编码为 SMT-LIB2 格式字符串
 *
 * 框架实现：生成 SMT-LIB2 脚本的基本骨架（set-logic, declare-fun, assert, check-sat）。
 * 完整实现需遍历 ConstraintGraph 的所有节点和边，将几何约束翻译为多项式方程。
 */
int smtencode_constraint_graph_to_smtlib2(const ConstraintGraph *graph, SMTLogic logic,
                                           bool produce_unsat_cores, char *out_smtlib2, size_t buffer_size) {
    LV00_CHECK_NULL(graph, -1);
    LV00_CHECK_NULL(out_smtlib2, -1);

    if (buffer_size < 256) {
        return (int)buffer_size + 256; /* 返回所需大小 */
    }

    const char *logic_name = smtsolver_logic_name(logic);
    int written = 0;

    written = snprintf(out_smtlib2, buffer_size,
                       "; SMT-LIB2 encoding for Lv-00 constraint graph\n"
                       "(set-logic %s)\n"
                       "(set-info :source |Lv-00 geometric constraint solver|)\n",
                       logic_name);
    if (written < 0) return -1;

    /* 声明变量：遍历约束图节点 */
    int remaining = (int)buffer_size - written;
    if (remaining <= 0) return written;

    int node_count = (graph ? graph->node_count : 0);
    for (int i = 0; i < node_count && remaining > 64; i++) {
        int n = snprintf(out_smtlib2 + written, (size_t)remaining,
                         "(declare-fun x%d () Real)\n", i);
        if (n < 0 || n >= remaining) break;
        written += n;
        remaining -= n;
    }

    if (remaining <= 0) return written;

    /* 编码约束为断言 */
    /* 简化实现：生成占位断言 */
    int edge_count = (graph ? graph->constraint_count : 0);
    for (int i = 0; i < edge_count && remaining > 128; i++) {
        int n;
        if (produce_unsat_cores) {
            n = snprintf(out_smtlib2 + written, (size_t)remaining,
                         "(assert (! (= x%d 0) :named c%d))\n", i % node_count, i);
        } else {
            n = snprintf(out_smtlib2 + written, (size_t)remaining,
                         "(assert (= x%d 0))\n", i % node_count);
        }
        if (n < 0 || n >= remaining) break;
        written += n;
        remaining -= n;
    }

    if (remaining <= 0) return written;

    /* 求解命令 */
    int n = snprintf(out_smtlib2 + written, (size_t)remaining, "(check-sat)\n");
    if (n > 0 && n < remaining) {
        written += n;
    }

    return written;
}

/* ============================================================
 * 求解流程：编码 -> 检查 -> 解码
 * ============================================================ */

/**
 * @brief 设置求解器内部错误状态
 */
static void smtsolver_set_error(SMTSolver *solver, SMTErrorCode code, const char *msg) {
    if (!solver) return;
    solver->last_error = code;
    if (msg) {
        snprintf(solver->last_error_msg, sizeof(solver->last_error_msg), "%s", msg);
    }
}

/**
 * @brief 将 SMT-LIB2 脚本加载到求解器
 *
 * 框架实现：仅存储脚本副本，不进行实际解析。
 */
int smtsolver_encode(SMTSolver *solver, const char *smtlib2, int len) {
    LV00_CHECK_NULL(solver, (int)-SMT_ERROR_ENCODING_FAILED);
    LV00_CHECK_NULL(smtlib2, (int)-SMT_ERROR_ENCODING_FAILED);

    if (!solver->is_initialized) {
        smtsolver_set_error(solver, SMT_ERROR_ENCODING_FAILED, "Solver not initialized");
        return (int)-SMT_ERROR_ENCODING_FAILED;
    }

    if (solver->last_error == SMT_ERROR_BACKEND_UNAVAILABLE) {
        return (int)-SMT_ERROR_BACKEND_UNAVAILABLE;
    }

    /* 释放旧编码 */
    if (solver->encoded_formula) {
        lv00_free((void **)&solver->encoded_formula);
    }

    int actual_len = (len <= 0) ? (int)strlen(smtlib2) : len;
    if (actual_len <= 0) {
        smtsolver_set_error(solver, SMT_ERROR_ENCODING_FAILED, "Empty SMT-LIB2 input");
        return (int)-SMT_ERROR_ENCODING_FAILED;
    }

    solver->encoded_formula = (char *)lv00_malloc((size_t)(actual_len + 1));
    if (!solver->encoded_formula) {
        smtsolver_set_error(solver, SMT_ERROR_MEMORY_EXHAUSTED, "Failed to allocate encoding buffer");
        return (int)-SMT_ERROR_MEMORY_EXHAUSTED;
    }

    memcpy(solver->encoded_formula, smtlib2, (size_t)actual_len);
    solver->encoded_formula[actual_len] = '\0';
    solver->encoded_len = actual_len;
    solver->has_assertions = true;
    solver->last_error = SMT_ERROR_NONE;

    return 0;
}

/**
 * @brief 执行可满足性检查
 *
 * 框架实现：桩代码，返回 SMT_RESULT_UNKNOWN。
 * 实际后端链接后将替换为真实求解器调用。
 */
SMTSatResult smtsolver_check(SMTSolver *solver) {
    LV00_CHECK_NULL(solver, SMT_RESULT_ERROR);

    if (!solver->is_initialized) {
        smtsolver_set_error(solver, SMT_ERROR_SOLVER_CRASHED, "Solver not initialized");
        return SMT_RESULT_ERROR;
    }

    if (!solver->has_assertions) {
        smtsolver_set_error(solver, SMT_ERROR_ENCODING_FAILED, "No assertions loaded");
        return SMT_RESULT_ERROR;
    }

    if (solver->last_error == SMT_ERROR_BACKEND_UNAVAILABLE) {
        return SMT_RESULT_UNKNOWN;
    }

    /* 桩：仅 GROEBNER 后端可进行基本求解 */
    if (solver->type == GROEBNER) {
        /* 内置 Gr?bner 后端：桩返回 UNKNOWN */
        return SMT_RESULT_UNKNOWN;
    }

    /* 所有外部后端均为桩：返回 UNKNOWN */
    smtsolver_set_error(solver, SMT_ERROR_BACKEND_UNAVAILABLE,
                        "Backend solver not linked; returning UNKNOWN");
    return SMT_RESULT_UNKNOWN;
}

/**
 * @brief 从求解器输出中解码结果
 *
 * 框架实现：填充基本的 SMTSolverResult 结构。
 */
int smtsolver_decode_result(SMTSolver *solver, SMTSatResult sat_result,
                             SMTSolverResult *out_result) {
    LV00_CHECK_NULL(solver, (int)-SMT_ERROR_PARSE_FAILED);

    if (!out_result) {
        return 0; /* 允许跳过结果构造 */
    }

    smtsolver_result_init(out_result);
    out_result->sat_result = sat_result;
    out_result->backend_used = solver->type;
    out_result->solve_time_ms = 0;

    if (sat_result == SMT_RESULT_ERROR) {
        out_result->error_code = solver->last_error;
        if (solver->last_error_msg[0]) {
            snprintf(out_result->error_message, sizeof(out_result->error_message),
                     "%s", solver->last_error_msg);
        }
    }

    return 0;
}

/**
 * @brief 完整求解管线：编码 -> 加载 -> 求解 -> 解码
 */
int smtsolver_solve(SMTSolver *solver, const ConstraintGraph *graph,
                     SMTSolverResult *out_result) {
    LV00_CHECK_NULL(solver, -1);
    LV00_CHECK_NULL(graph, -1);

    if (!out_result) {
        return -1;
    }

    smtsolver_result_init(out_result);

    /* 步骤 1：编码约束图为 SMT-LIB2 */
    char *smtlib2_buf = (char *)lv00_malloc(SMTLIB2_DEFAULT_BUFFER);
    if (!smtlib2_buf) {
        out_result->sat_result = SMT_RESULT_ERROR;
        out_result->error_code = SMT_ERROR_MEMORY_EXHAUSTED;
        snprintf(out_result->error_message, sizeof(out_result->error_message),
                 "Failed to allocate SMT-LIB2 buffer");
        return -1;
    }

    int enc_len = smtencode_constraint_graph_to_smtlib2(graph, solver->config.logic,
                                                          solver->config.produce_unsat_cores,
                                                          smtlib2_buf, SMTLIB2_DEFAULT_BUFFER);
    if (enc_len < 0) {
        lv00_free((void **)&smtlib2_buf);
        out_result->sat_result = SMT_RESULT_ERROR;
        out_result->error_code = SMT_ERROR_ENCODING_FAILED;
        snprintf(out_result->error_message, sizeof(out_result->error_message),
                 "SMT-LIB2 encoding failed");
        return -1;
    }

    /* 步骤 2：加载到求解器 */
    int rc = smtsolver_encode(solver, smtlib2_buf, enc_len);
    lv00_free((void **)&smtlib2_buf);

    if (rc < 0) {
        out_result->sat_result = SMT_RESULT_ERROR;
        out_result->error_code = (SMTErrorCode)(-rc);
        snprintf(out_result->error_message, sizeof(out_result->error_message),
                 "Solver encoding failed");
        return -1;
    }

    /* 步骤 3：执行求解 */
    SMTSatResult sat_res = smtsolver_check(solver);
    out_result->sat_result = sat_res;
    out_result->backend_used = solver->type;

    /* 步骤 4：解码结果 */
    smtsolver_decode_result(solver, sat_res, out_result);

    return (sat_res == SMT_RESULT_SAT) ? 0 : ((sat_res == SMT_RESULT_ERROR) ? -1 : 1);
}

/* ============================================================
 * 结果管理
 * ============================================================ */

/**
 * @brief 初始化空的求解结果
 */
void smtsolver_result_init(SMTSolverResult *result) {
    if (!result) {
        return;
    }
    memset(result, 0, sizeof(SMTSolverResult));
    result->sat_result = SMT_RESULT_UNKNOWN;
    result->backend_used = GROEBNER;
}

/**
 * @brief 释放求解结果中的动态资源
 */
void smtsolver_result_free(SMTSolverResult *result) {
    if (!result) {
        return;
    }
    if (result->assignments) {
        lv00_free((void **)&result->assignments);
    }
    result->assignment_count = 0;
    if (result->unsat_core_ids) {
        lv00_free((void **)&result->unsat_core_ids);
    }
    result->unsat_core_size = 0;
}

/**
 * @brief 在结果中按变量节点 ID 查找赋值
 */
const SMTVariableAssignment *smtsolver_result_find_assignment(const SMTSolverResult *result,
                                                                int var_node_id) {
    if (!result || !result->assignments || result->assignment_count <= 0) {
        return NULL;
    }

    for (int i = 0; i < result->assignment_count; i++) {
        if (result->assignments[i].var_node_id == var_node_id) {
            return &result->assignments[i];
        }
    }
    return NULL;
}

/**
 * @brief 检查结果是否为有效解
 */
bool smtsolver_result_is_valid(const SMTSolverResult *result) {
    if (!result) {
        return false;
    }
    return (result->sat_result == SMT_RESULT_SAT) && (result->assignment_count > 0);
}

/* ============================================================
 * 后端可用性查询
 * ============================================================ */

/**
 * @brief 检查指定后端是否可用
 *
 * 当前仅 GROEBNER 内置后端标记为可用。
 * Z3、cvc5、Singular 需要对应的编译单元被链接后才可用。
 */
bool smtsolver_is_backend_available(SolverBackendType type) {
    switch (type) {
    case GROEBNER:
        return true;  /* 内置实现，始终可用 */
    case SMT_Z3:
    case SMT_CVC5:
    case SMT_SINGULAR:
        return false; /* 未链接 */
    default:
        return false;
    }
}

/**
 * @brief 获取后端名称字符串
 */
const char *smtsolver_backend_type_name(SolverBackendType type) {
    switch (type) {
    case GROEBNER:    return "Groebner";
    case SMT_Z3:      return "Z3";
    case SMT_CVC5:    return "cvc5";
    case SMT_SINGULAR: return "Singular";
    default:          return "Unknown";
    }
}

/**
 * @brief 从名称字符串解析后端类型（大小写不敏感）
 */
SolverBackendType smtsolver_backend_type_from_name(const char *name) {
    if (!name) {
        return COUNT;
    }

    /* 简单的大小写不敏感比较 */
    if (strcasecmp(name, "groebner") == 0 || strcasecmp(name, "grobner") == 0) {
        return GROEBNER;
    }
    if (strcasecmp(name, "z3") == 0) {
        return SMT_Z3;
    }
    if (strcasecmp(name, "cvc5") == 0) {
        return SMT_CVC5;
    }
    if (strcasecmp(name, "singular") == 0) {
        return SMT_SINGULAR;
    }
    return COUNT;
}

/**
 * @brief 获取 SMT 逻辑的名称字符串
 */
const char *smtsolver_logic_name(SMTLogic logic) {
    switch (logic) {
    case SMT_LOGIC_QF_NRA:   return "QF_NRA";
    case SMT_LOGIC_QF_LRA:   return "QF_LRA";
    case SMT_LOGIC_QF_NIA:   return "QF_NIA";
    case SMT_LOGIC_QF_LIA:   return "QF_LIA";
    case SMT_LOGIC_QF_UFLRA:  return "QF_UFLRA";
    case SMT_LOGIC_QF_UFNRA:  return "QF_UFNRA";
    case SMT_LOGIC_QF_BV:    return "QF_BV";
    case SMT_LOGIC_AUTO:     return "AUTO";
    default:                 return "UNKNOWN";
    }
}

/**
 * @brief 获取 SMT 可满足性结果的名称字符串
 */
const char *smtsolver_sat_result_name(SMTSatResult result) {
    switch (result) {
    case SMT_RESULT_SAT:     return "SAT";
    case SMT_RESULT_UNSAT:   return "UNSAT";
    case SMT_RESULT_UNKNOWN: return "UNKNOWN";
    case SMT_RESULT_ERROR:   return "ERROR";
    default:                 return "INVALID";
    }
}

/**
 * @brief 获取 SMT 错误码描述字符串
 */
const char *smtsolver_error_string(SMTErrorCode code) {
    switch (code) {
    case SMT_ERROR_NONE:                return "No error";
    case SMT_ERROR_BACKEND_UNAVAILABLE: return "Backend unavailable";
    case SMT_ERROR_ENCODING_FAILED:     return "Encoding failed";
    case SMT_ERROR_PARSE_FAILED:        return "Parse failed";
    case SMT_ERROR_SOLVER_CRASHED:      return "Solver crashed";
    case SMT_ERROR_MEMORY_EXHAUSTED:    return "Memory exhausted";
    case SMT_ERROR_TIMEOUT_REACHED:     return "Timeout reached";
    case SMT_ERROR_UNSUPPORTED_THEORY:  return "Unsupported theory";
    case SMT_ERROR_INVALID_MODEL:       return "Invalid model";
    default:                            return "Unknown error";
    }
}

/* ============================================================
 * 后端注册表管理
 * ============================================================ */

/**
 * @brief 获取全局后端注册表（惰性初始化）
 */
SMTBackendRegistry *smtsolver_get_registry(void) {
    if (!g_smt_registry_initialized) {
        memset(&g_smt_registry, 0, sizeof(g_smt_registry));
        g_smt_registry.count = 0;
        g_smt_registry_initialized = true;
    }
    return &g_smt_registry;
}

/**
 * @brief 向后端注册表注册一个后端
 */
int smtsolver_register_backend(SMTBackendRegistry *registry, const SMTBackendEntry *entry) {
    LV00_CHECK_NULL(registry, -1);
    LV00_CHECK_NULL(entry, -1);

    if (registry->count >= SMT_BACKEND_REGISTRY_CAPACITY) {
        return -1;
    }

    registry->entries[registry->count] = *entry;
    registry->count++;
    return 0;
}

/**
 * @brief 在注册表中查找指定类型的后端
 */
const SMTBackendEntry *smtsolver_find_backend(const SMTBackendRegistry *registry,
                                               SolverBackendType type) {
    if (!registry) {
        return NULL;
    }

    for (int i = 0; i < registry->count; i++) {
        if (registry->entries[i].type == type) {
            return &registry->entries[i];
        }
    }
    return NULL;
}

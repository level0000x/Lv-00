/**
 * @file smt_backend_impl.c
 * @brief SMT 后端抽象层实现 —— 多引擎 SMT 求解器框架（含 Groebner 基真实求解）
 *
 * @details 本模块实现 smt_backend.h 中声明的所有 SMT 后端接口。
 *          设计参考 polymake 的多后端架构，提供与求解器无关的统一接口。
 *
 *          后端实现状态：
 *          - GROEBNER：已集成，通过 constraint_graph_to_ideal() 将约束图
 *            转换为多项式理想，调用 Buchberger 算法计算 Groebner 基，
 *            通过理想成员关系判定可满足性，并通过代数簇求解获取具体坐标。
 *          - Z3 / cvc5 / Singular：通过子进程调用外部求解器，
 *            Z3 和 cvc5 使用 SMT-LIB2 格式，Singular 使用自有脚本格式，
 *            未安装时返回 SMT_RESULT_UNKNOWN 并可回退到 Groebner 后端。
 *
 *          编码管线：
 *          1. smtencode_constraint_graph_to_smtlib2()  约束图 -> SMT-LIB2
 *          2. smtsolver_encode()                       SMT-LIB2 -> 后端原生表示
 *          3. smtsolver_check()                        执行求解（Groebner 后端真实求解）
 *          4. smtsolver_decode_result()                解析结果 -> SMTSolverResult
 *
 * @author Lv-00 Project
 * @version 3.4.0
 * @date 2026-05-25
 *
 * @dependencies
 *   - smt_backend.h          : SMT 后端公共接口
 *   - groebner_engine.h      : Groebner 基计算引擎（内置求解核心）
 *   - lv_internal.h        : 内部常量与工具宏
 *   - lv_utils.h           : 统一内存分配器
 *   - error_codes.h          : 统一错误码系统
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv.h"
#include "lv/lv_error.h"
#include "lv/lv_file.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_xmacro.h"

#include "lv/smt_backend.h"
#include "smt_backend_internal.h"
#include "lv/lv_backend_plugin.h"
#include "lv/lv_registry.h"
#include "lv/lv_thread.h"

#include "lv/error_codes.h"
#include "lv/groebner_engine.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"



/* ============================================================
 * 全局后端注册表（单例）
 * ============================================================ */

/** @brief SMT 后端注册表单例状态 */
typedef struct {
    lv_lazy_lock lock;                /**< 注册表访问锁（lv_once 惰性初始化，消除锁初始化竞态） */
    SMTBackendRegistry registry;      /**< 后端完整元数据（兼容 SMTBackendRegistry 结构） */
    bool registry_inited;             /**< 注册表数据是否已初始化 */
} SMTRegistryState;

/** @brief SMT 后端注册表全局单例 */
static SMTRegistryState s_smt_registry_state = {0};

/** @brief 注册表互斥锁的一次性初始化回调（由 lv_lazy_lock 触发，线程安全） */
static void smtsolver_registry_lock_init_once(void) {
    lv_mutex_init(&s_smt_registry_state.lock.mutex);
}


/* ============================================================
 * 默认配置
 * ============================================================ */

static SMTSolverConfig g_default_configs[COUNT];
static lv_once_t g_default_config_once = lv_ONCE_INIT;

/** @brief 初始化全部后端的默认求解器配置（仅执行一次，由 lv_once 保证线程安全） */
static void smtsolver_default_config_init(void) {
    for (int i = 0; i < COUNT; i++) {
        g_default_configs[i].timeout_ms = lv_DEFAULT_TIMEOUT_MS;
        g_default_configs[i].memory_limit_mb = lv_config_get_int(LV_CFG_SMT_DEFAULT_MEMORY_MB, SMT_DEFAULT_MEMORY_MB);
        g_default_configs[i].logic = SMT_LOGIC_AUTO;
        g_default_configs[i].produce_models = true;
        g_default_configs[i].produce_unsat_cores = false;
        g_default_configs[i].produce_proofs = false;
        g_default_configs[i].incremental = false;
        g_default_configs[i].random_seed = -1;
        g_default_configs[i].verbosity = 0;
        g_default_configs[i].custom_config = NULL;
    }
    /* Groebner 后端使用非线性实数算术（几何约束含距离平方等二次项） */
    g_default_configs[GROEBNER].logic = SMT_LOGIC_QF_NRA;
}

/**
 * @brief 创建并返回默认的求解器配置
 *
 * 基于后端类型选择适当的默认值。所有后端共享通用的基础配置，
 * 特定后端可通过 custom_config 传入私有参数。
 * Groebner 后端默认使用非线性实数算术（QF_NRA），因为几何约束
 * 通常涉及距离平方等二次多项式。
 */
const SMTSolverConfig *smtsolver_default_config(SolverBackendType type) {
    lv_once(&g_default_config_once, smtsolver_default_config_init);

    if (type >= COUNT) {
        return &g_default_configs[GROEBNER];
    }
    return &g_default_configs[type];
}

/* ============================================================
 * 后端生命周期管理
 * ============================================================ */

/**
 * @brief 创建 SMT 求解器实例
 *
 * 根据后端类型创建求解器句柄。GROEBNER 后端使用内置实现，
 * Z3/cvc5/Singular 通过子进程调用外部求解器（运行时可用性取决于是否安装）。
 * 未链接的后端设置 SMT_ERROR_BACKEND_UNAVAILABLE 但不阻止创建句柄。
 */
SMTSolver *smtsolver_create(SolverBackendType type, const SMTSolverConfig *config) {
    SMTSolver *solver = (SMTSolver *) lv_calloc(1, sizeof(SMTSolver));
    if (!solver) {
        return NULL;
    }
    solver->type = type;
    solver->is_initialized = true;
    solver->has_assertions = false;
    solver->last_error = SMT_ERROR_NONE;
    solver->last_error_msg[0] = '\0';

    /* Groebner 后端专用字段初始化为无效值 */
    solver->groebner_registry = NULL;
    solver->groebner_ring_id = -1;
    solver->groebner_ideal_id = -1;
    solver->groebner_var_count = 0;
    solver->groebner_node_var_map = NULL;
    solver->groebner_node_var_map_size = 0;
    solver->groebner_variety_id = -1;

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
        char msg[512];
        snprintf(msg, sizeof(msg), "Backend '%s' is not available (not linked)", smtsolver_backend_type_name(type));
        smtsolver_set_error(solver, SMT_ERROR_BACKEND_UNAVAILABLE, msg);
    }

    return solver;
}

/**
 * @brief 销毁 SMT 求解器实例
 *
 * 释放求解器占用的所有资源，包括 SMT-LIB2 编码缓冲区和
 * Groebner 后端的环注册表、理想、代数簇等。
 */
void smtsolver_destroy(SMTSolver *solver) {
    if (!solver) {
        return;
    }
    if (solver->encoded_formula) {
        lv_free((void **) &solver->encoded_formula);
    }

    /* 清理 Groebner 后端专用资源 */
    groebner_backend_cleanup(solver);

    lv_free((void **) &solver);
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
    if (lv_str_is_empty(solver->last_error_msg)) {
        return "";
    }
    return solver->last_error_msg;
}


/* ============================================================
 * 求解流程：编码 -> 检查 -> 解码
 * ============================================================ */

/**
 * @brief 设置求解器内部错误状态
 */
void smtsolver_set_error(SMTSolver *solver, SMTErrorCode code, const char *msg) {
    if (!solver)
        return;
    solver->last_error = code;
    if (msg) {
        /* 公共写入口：模块级 last_error 通道统一走 lv_error.h 的 lv_ERROR_SLOT_COPY */
        lv_ERROR_SLOT_COPY(solver->last_error_msg, sizeof(solver->last_error_msg), msg);
    }
}

/**
 * @brief 将 SMT-LIB2 脚本加载到求解器
 *
 * 框架实现：仅存储脚本副本，不进行实际解析。
 * 对于 Groebner 后端，SMT-LIB2 脚本仅作为调试输出，
 * 实际求解通过直接操作约束图的多项式理想完成。
 */
int smtsolver_encode(SMTSolver *solver, const char *smtlib2, int len) {
    lv_CHECK_NULL(solver, (int) -SMT_ERROR_ENCODING_FAILED);
    lv_CHECK_NULL(smtlib2, (int) -SMT_ERROR_ENCODING_FAILED);

    if (!solver->is_initialized) {
        smtsolver_set_error(solver, SMT_ERROR_ENCODING_FAILED, "Solver not initialized");
        return (int) -SMT_ERROR_ENCODING_FAILED;
    }

    if (solver->last_error == SMT_ERROR_BACKEND_UNAVAILABLE) {
        return (int) -SMT_ERROR_BACKEND_UNAVAILABLE;
    }

    /* 释放旧编码 */
    if (solver->encoded_formula) {
        lv_free((void **) &solver->encoded_formula);
    }

    int actual_len = (len <= 0) ? (int) strlen(smtlib2) : len;
    if (actual_len <= 0) {
        smtsolver_set_error(solver, SMT_ERROR_ENCODING_FAILED, "Empty SMT-LIB2 input");
        return (int) -SMT_ERROR_ENCODING_FAILED;
    }

    solver->encoded_formula = (char *) lv_malloc((size_t) (actual_len + 1));
    if (!solver->encoded_formula) {
        smtsolver_set_error(solver, SMT_ERROR_MEMORY_EXHAUSTED, "Failed to allocate encoding buffer");
        return (int) -SMT_ERROR_MEMORY_EXHAUSTED;
    }

    lv_strlcpy_n(solver->encoded_formula, (size_t) actual_len + 1, smtlib2, (size_t) actual_len);
    solver->encoded_len = actual_len;
    solver->has_assertions = true;
    solver->last_error = SMT_ERROR_NONE;

    return 0;
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
        lv_free((void **) &result->assignments);
    }
    result->assignment_count = 0;
    if (result->unsat_core_ids) {
        lv_free((void **) &result->unsat_core_ids);
    }
    result->unsat_core_size = 0;
}

/**
 * @brief 清除求解结果（释放动态资源并重置）
 *
 * 调用 smtsolver_result_free 释放赋值和 unsat core 后，
 * 将整个结果结构体清零并初始化为 UNKNOWN 状态。
 */
void smtsolver_result_clear(SMTSolverResult *result) {
    if (!result)
        return;
    smtsolver_result_free(result);
    result->sat_result = SMT_RESULT_UNKNOWN;
    result->backend_used = GROEBNER;
}

/**
 * @brief 在结果中按变量节点 ID 查找赋值
 */
const SMTVariableAssignment *smtsolver_result_find_assignment(const SMTSolverResult *result, int var_node_id) {
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
 * 当前仅 GROEBNER 内置后端标记为可用（已集成 Groebner 基引擎）。
 * Z3、cvc5、Singular 需要对应的编译单元被链接后才可用。
 */
bool smtsolver_is_backend_available(SolverBackendType type) {
    static const bool s_available[COUNT] = {
        [GROEBNER] = true,
        [SMT_Z3] = false,
        [SMT_CVC5] = false,
        [SMT_SINGULAR] = false,
    };
    if (type < 0 || type >= COUNT)
        return false;
    return s_available[type];
}

/**
 * @brief 获取后端名称字符串
 */
/* ================================================================
 * 枚举 -> 名称 映射表（数据表化，替代 switch）
 * ================================================================ */

/** @brief smtsolver_backend_type_name 名称表（由 LV_SMT_BACKEND_ENTRY 单一事实源生成） */
static const lvStrToEnumEntry s_smtsolver_backend_type_name_entries[] = {
    lv_XMACRO_TO_ENUM_TABLE(LV_SMT_BACKEND_ENTRY)
};

const char *smtsolver_backend_type_name(SolverBackendType type) {
    return lv_enum_to_str(s_smtsolver_backend_type_name_entries, lv_ARRAY_SIZE(s_smtsolver_backend_type_name_entries), (int) type, "Unknown");
}

/**
 * @brief 从名称字符串解析后端类型（大小写不敏感）
 *
 * 使用 s_smtsolver_backend_type_name_entries 表驱动查找。
 * 注意：表中 "Groebner" 首字母大写，此处做大小写不敏感比较。
 */
SolverBackendType smtsolver_backend_type_from_name(const char *name) {
    if (!name) {
        return COUNT;
    }

    /* 先尝试精确匹配现有表项 */
    int v = lv_str_to_enum_ci(s_smtsolver_backend_type_name_entries,
                               lv_ARRAY_SIZE(s_smtsolver_backend_type_name_entries),
                               name, -1);
    if (v >= 0)
        return (SolverBackendType) v;

    /* 兼容 "grobner" 拼写错误 */
    if (lv_str_icmp(name, "grobner") == 0)
        return GROEBNER;

    return COUNT;
}

/**
 * @brief 获取 SMT 逻辑的名称字符串
 */
/** @brief smtsolver_logic_name 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_smtsolver_logic_name_entries[] = {
    {"QF_NRA", SMT_LOGIC_QF_NRA},
    {"QF_LRA", SMT_LOGIC_QF_LRA},
    {"QF_NIA", SMT_LOGIC_QF_NIA},
    {"QF_LIA", SMT_LOGIC_QF_LIA},
    {"QF_UFLRA", SMT_LOGIC_QF_UFLRA},
    {"QF_UFNRA", SMT_LOGIC_QF_UFNRA},
    {"QF_BV", SMT_LOGIC_QF_BV},
    {"AUTO", SMT_LOGIC_AUTO},
};

const char *smtsolver_logic_name(SMTLogic logic) {
    return lv_enum_to_str(s_smtsolver_logic_name_entries, lv_ARRAY_SIZE(s_smtsolver_logic_name_entries), (int) logic, "UNKNOWN");
}

/**
 * @brief 获取 SMT 可满足性结果的名称字符串
 */
/** @brief smtsolver_sat_result_name 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_smtsolver_sat_result_name_entries[] = {
    {"SAT", SMT_RESULT_SAT},
    {"UNSAT", SMT_RESULT_UNSAT},
    {"UNKNOWN", SMT_RESULT_UNKNOWN},
    {"ERROR", SMT_RESULT_ERROR},
};

const char *smtsolver_sat_result_name(SMTSatResult result) {
    return lv_enum_to_str(s_smtsolver_sat_result_name_entries, lv_ARRAY_SIZE(s_smtsolver_sat_result_name_entries), (int) result, "INVALID");
}

/**
 * @brief 获取 SMT 错误码描述字符串
 */
/** @brief smtsolver_error_string 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_smtsolver_error_string_entries[] = {
    {"No error", SMT_ERROR_NONE},
    {"Backend unavailable", SMT_ERROR_BACKEND_UNAVAILABLE},
    {"Encoding failed", SMT_ERROR_ENCODING_FAILED},
    {"Parse failed", SMT_ERROR_PARSE_FAILED},
    {"Solver crashed", SMT_ERROR_SOLVER_CRASHED},
    {"Memory exhausted", SMT_ERROR_MEMORY_EXHAUSTED},
    {"Timeout reached", SMT_ERROR_TIMEOUT_REACHED},
    {"Unsupported theory", SMT_ERROR_UNSUPPORTED_THEORY},
    {"Invalid model", SMT_ERROR_INVALID_MODEL},
};

const char *smtsolver_error_string(SMTErrorCode code) {
    return lv_enum_to_str(s_smtsolver_error_string_entries, lv_ARRAY_SIZE(s_smtsolver_error_string_entries), (int) code, "Unknown error");
}

/* ============================================================
 * 后端注册表管理
 * ============================================================ */

/**
 * @brief 获取全局后端注册表（惰性初始化）
 *
 * 惰性锁保证首次初始化仅执行一次且 happens-before 后续访问，
 * 消除原先无锁标志检查-设置造成的锁初始化竞态。
 */
SMTBackendRegistry *smtsolver_get_registry(void) {
    lv_lazy_lock_lock(&s_smt_registry_state.lock, smtsolver_registry_lock_init_once);
    if (!s_smt_registry_state.registry_inited) {
        memset(&s_smt_registry_state.registry, 0, sizeof(s_smt_registry_state.registry));
        s_smt_registry_state.registry.count = 0;

        /* 自动注册内置后端（与 smtsolver_is_backend_available / 名称表保持一致），
         * 避免注册表单例长期为空导致 smtsolver_find_backend 永远查不到后端。 */
        static const struct {
            SolverBackendType type;
            const char *version;
            int priority;
        } kBuiltinBackends[] = {
            {GROEBNER, "3.x", 1},
            {SMT_Z3, "4.x", 2},
            {SMT_CVC5, "1.x", 3},
            {SMT_SINGULAR, "4.x", 4},
        };
        for (size_t i = 0;
             i < sizeof(kBuiltinBackends) / sizeof(kBuiltinBackends[0]) &&
             s_smt_registry_state.registry.count < SMT_BACKEND_REGISTRY_CAPACITY;
             i++) {
            SMTBackendEntry *entry = &s_smt_registry_state.registry.entries[s_smt_registry_state.registry.count];
            memset(entry, 0, sizeof(*entry));
            entry->type = kBuiltinBackends[i].type;
            lv_strlcpy(entry->name, smtsolver_backend_type_name(kBuiltinBackends[i].type), sizeof(entry->name));
            lv_strlcpy(entry->version, kBuiltinBackends[i].version, sizeof(entry->version));
            entry->available = smtsolver_is_backend_available(kBuiltinBackends[i].type);
            entry->priority = kBuiltinBackends[i].priority;
            s_smt_registry_state.registry.count++;
        }

        s_smt_registry_state.registry_inited = true;
    }
    lv_lazy_lock_unlock(&s_smt_registry_state.lock);
    return &s_smt_registry_state.registry;
}

/**
 * @brief 向后端注册表注册一个后端
 */
int smtsolver_register_backend(SMTBackendRegistry *registry, const SMTBackendEntry *entry) {
    lv_CHECK_NULL(registry, -1);
    lv_CHECK_NULL(entry, -1);

    /* 惰性锁首次使用时自动完成互斥锁初始化（消除对未初始化锁的加锁） */
    lv_lazy_lock_lock(&s_smt_registry_state.lock, smtsolver_registry_lock_init_once);

    if (registry->count >= SMT_BACKEND_REGISTRY_CAPACITY) {
        lv_lazy_lock_unlock(&s_smt_registry_state.lock);
        lv_RETURN_ERROR(lv_ERROR_RESOURCE_EXHAUSTED, "SMT 后端注册表容量已满");
    }

    registry->entries[registry->count] = *entry;
    registry->count++;

    lv_lazy_lock_unlock(&s_smt_registry_state.lock);
    return 0;
}

/**
 * @brief 在注册表中查找指定类型的后端
 */
const SMTBackendEntry *smtsolver_find_backend(const SMTBackendRegistry *registry, SolverBackendType type) {
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

/* ============================================================
 * 统一后端插件系统集成
 *
 * 将 SMT 后端注册到全局 lvBackendPluginRegistry，
 * 保持现有 API 向后兼容。
 * ============================================================ */

/** @brief SMT 后端插件描述符数组 */
static lvBackendPlugin s_smt_plugins[COUNT];

/** @brief SMT 插件懒注册一次性守卫（lv_once 保证线程安全） */
static lv_once_t s_smt_plugins_once = lv_ONCE_INIT;

/**
 * @brief SMT 后端插件初始化函数
 */
static bool smt_plugin_init_groebner(void) {
    s_smt_plugins[GROEBNER].available = smtsolver_is_backend_available(GROEBNER);
    return true;
}

static bool smt_plugin_init_z3(void) {
    s_smt_plugins[SMT_Z3].available = smtsolver_is_backend_available(SMT_Z3);
    return true;
}

static bool smt_plugin_init_cvc5(void) {
    s_smt_plugins[SMT_CVC5].available = smtsolver_is_backend_available(SMT_CVC5);
    return true;
}

static bool smt_plugin_init_singular(void) {
    s_smt_plugins[SMT_SINGULAR].available = smtsolver_is_backend_available(SMT_SINGULAR);
    return true;
}

/**
 * @brief SMT 后端插件一次性注册回调（仅由 lv_once 调用一次）
 *
 * 创建 lvBackendPlugin 包装器，将每个 SMT 后端类型映射到
 * 统一插件描述符，并注册到全局注册表。
 */
static void smt_plugins_register_once(void) {
    lvBackendPluginRegistry *reg = lv_backend_plugin_registry_global();

    /* 初始化静态插件描述符 */
    memset(s_smt_plugins, 0, sizeof(s_smt_plugins));

    /* Groebner */
    s_smt_plugins[GROEBNER].name = smtsolver_backend_type_name(GROEBNER);
    s_smt_plugins[GROEBNER].version = "1.0";
    s_smt_plugins[GROEBNER].type = lv_PLUGIN_TYPE_GROEBNER;
    s_smt_plugins[GROEBNER].capabilities = lv_PLUGIN_CAP_EXACT;
    s_smt_plugins[GROEBNER].priority = 1;
    s_smt_plugins[GROEBNER].available = smtsolver_is_backend_available(GROEBNER);
    s_smt_plugins[GROEBNER].init = smt_plugin_init_groebner;
    s_smt_plugins[GROEBNER].cleanup = NULL;
    s_smt_plugins[GROEBNER].ops = NULL;
    lv_backend_plugin_register(reg, &s_smt_plugins[GROEBNER]);

    /* Z3 */
    s_smt_plugins[SMT_Z3].name = smtsolver_backend_type_name(SMT_Z3);
    s_smt_plugins[SMT_Z3].version = "4.x";
    s_smt_plugins[SMT_Z3].type = lv_PLUGIN_TYPE_SMT;
    s_smt_plugins[SMT_Z3].capabilities = lv_PLUGIN_CAP_INCREMENTAL | lv_PLUGIN_CAP_PROOF_PROD
                                         | lv_PLUGIN_CAP_UNSAT_CORE | lv_PLUGIN_CAP_FLOAT;
    s_smt_plugins[SMT_Z3].priority = 2;
    s_smt_plugins[SMT_Z3].available = smtsolver_is_backend_available(SMT_Z3);
    s_smt_plugins[SMT_Z3].init = smt_plugin_init_z3;
    s_smt_plugins[SMT_Z3].cleanup = NULL;
    s_smt_plugins[SMT_Z3].ops = NULL;
    lv_backend_plugin_register(reg, &s_smt_plugins[SMT_Z3]);

    /* cvc5 */
    s_smt_plugins[SMT_CVC5].name = smtsolver_backend_type_name(SMT_CVC5);
    s_smt_plugins[SMT_CVC5].version = "1.x";
    s_smt_plugins[SMT_CVC5].type = lv_PLUGIN_TYPE_SMT;
    s_smt_plugins[SMT_CVC5].capabilities = lv_PLUGIN_CAP_INCREMENTAL | lv_PLUGIN_CAP_PROOF_PROD
                                           | lv_PLUGIN_CAP_UNSAT_CORE | lv_PLUGIN_CAP_FLOAT;
    s_smt_plugins[SMT_CVC5].priority = 3;
    s_smt_plugins[SMT_CVC5].available = smtsolver_is_backend_available(SMT_CVC5);
    s_smt_plugins[SMT_CVC5].init = smt_plugin_init_cvc5;
    s_smt_plugins[SMT_CVC5].cleanup = NULL;
    s_smt_plugins[SMT_CVC5].ops = NULL;
    lv_backend_plugin_register(reg, &s_smt_plugins[SMT_CVC5]);

    /* Singular */
    s_smt_plugins[SMT_SINGULAR].name = smtsolver_backend_type_name(SMT_SINGULAR);
    s_smt_plugins[SMT_SINGULAR].version = "4.x";
    s_smt_plugins[SMT_SINGULAR].type = lv_PLUGIN_TYPE_GROEBNER;
    s_smt_plugins[SMT_SINGULAR].capabilities = lv_PLUGIN_CAP_EXACT;
    s_smt_plugins[SMT_SINGULAR].priority = 4;
    s_smt_plugins[SMT_SINGULAR].available = smtsolver_is_backend_available(SMT_SINGULAR);
    s_smt_plugins[SMT_SINGULAR].init = smt_plugin_init_singular;
    s_smt_plugins[SMT_SINGULAR].cleanup = NULL;
    s_smt_plugins[SMT_SINGULAR].ops = NULL;
    lv_backend_plugin_register(reg, &s_smt_plugins[SMT_SINGULAR]);
}

/**
 * @brief 将所有 SMT 后端注册到全局后端插件注册表
 *
 * 创建 lvBackendPlugin 包装器，将每个 SMT 后端类型映射到
 * 统一插件描述符，并注册到全局注册表。
 * 此函数可安全地多次调用（仅首次生效），
 * 由 lv_once 保证懒注册的线程安全（消除无锁标志的检查-设置竞态）。
 */
void smt_register_all_plugins(void) {
    lv_once(&s_smt_plugins_once, smt_plugins_register_once);
}

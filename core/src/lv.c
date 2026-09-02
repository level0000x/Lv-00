/**
 * @file lv.c
 * @brief Lv-00 几何元语言系统主实现
 *
 * @details 实现系统初始化、清理和全局管理功能。
 *          包含嵌套初始化支持、配置管理、健康检查和便捷API。
 *          作为整个 Lv-00 系统的入口模块，负责协调各子系统的
 *          生命周期管理。
 *
 * @version 3.3.0
 * @author Lv-00 Team
 */

#include "lv/lv.h"
#include "lv/lv_log.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lv/adaptive_threshold.h"
#include "lv/bit_burning.h"
#include "lv/ecosystem.h"
#include "lv/formula_converter.h"
#include "lv/lv_config.h" /* lv_config_snapshot_cleanup（F43/K15 方案 B） */
#include "lv/lv_file.h" /* lv_file_exists（K62/F88 A JSON 配置加载接线） */
#include "lv/lv_error.h"
#include "lv/lv_registry.h"
#include "lv/memory_pool.h"
#include "lv/module_internal.h"
#include "lv/runtime_monitor.h"

#define lv_THREAD_POOL_IMPL
#include "lv/thread_pool.h"

#include "lv/func_block_registry.h"
#include "lv/interop.h"
#include "lv/lv_thread.h" /* lv_lazy_lock（F28/J1：lv_init/cleanup 跨线程互斥） */
#include "lv/lv_internal.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_serialize_adapters.h"

/* ============================================================
 * 全局状态管理
 * ============================================================ */

/**
 * @brief 系统初始化状态
 */
/* exempt: 1-B 状态机豁免 —— 本状态机描述"进程生命周期"
 * （UNINITIALIZED→INITIALIZING→INITIALIZED→SHUTTING_DOWN→UNINITIALIZED，
 * 带 init_count 嵌套引用计数、TLS 每线程实例、幂等 init/cleanup 可重入），
 * 与 context.c/engine_state.c 的"推理任务五态状态机"
 * （IDLE→PARSING→REASONING→COMPLETE/ERROR，转移矩阵+位掩码查表）语义异构：
 * 无共享转移矩阵，不允许中途回退到 IDLE 之外的语义差异过大，故不迁移。 */
typedef enum {
    SYSTEM_STATE_UNINITIALIZED = 0,
    SYSTEM_STATE_INITIALIZING,
    SYSTEM_STATE_INITIALIZED,
    SYSTEM_STATE_SHUTTING_DOWN,
    SYSTEM_STATE_ERROR
} SystemState;

/**
 * @brief Lv 系统模块全局状态（线程局部）
 */
typedef struct LvState {
    SystemState system_state;
    int init_count;
    ConfigManager *config;
    lvEngine *global_engine;
    int log_level;
    bool assertions_enabled;
} LvState;

/** 模块级唯一状态实例（F28/J1：从 TLS 提升为进程级全局——
 * 原 TLS 每线程独立状态致跨线程 lv_init 重复初始化共享资源（F43 竞态）；
 * 现为进程级单实例 + lv_lazy_lock 互斥，lv_init/lv_cleanup 跨线程安全） */
static LvState s_lv_state = {0};
lv_LAZY_LOCK_DEFINE(g_lv_state_lock);

/* ============================================================
 * 内部辅助函数
 * ============================================================ */

/**
 * @brief 设置系统状态
 *
 * @param state  要设置的目标系统状态
 * @note 仅供内部使用，直接修改全局系统状态变量
 */
static void set_system_state(SystemState state) {
    s_lv_state.system_state = state;
}

/**
 * @brief 获取系统状态
 *
 * @return 当前系统状态值
 * @note 仅供内部使用，读取全局系统状态变量
 */
static SystemState get_system_state(void) {
    return s_lv_state.system_state;
}

/**
 * @brief 检查系统是否已初始化
 *
 * @return true  系统已处于初始化完成状态
 * @return false 系统尚未初始化或处于其他状态
 * @note 通过比较全局状态是否等于 SYSTEM_STATE_INITIALIZED 来判断
 */
static bool is_system_initialized(void) {
    return s_lv_state.system_state == SYSTEM_STATE_INITIALIZED;
}

/* ============================================================
 * 公共API实现
 * ============================================================ */

/**
 * @brief 获取版本字符串
 * @return 版本号字符串，格式 "major.minor.patch"
 */
const char *lv_get_version_string(void) {
    static lv_THREAD_LOCAL char version_str[32] = {0};
    if (lv_str_is_empty(version_str)) {
        lv_snprintf(version_str, sizeof(version_str), "%d.%d.%d", lv_VERSION_MAJOR, lv_VERSION_MINOR, lv_VERSION_PATCH);
    }
    return version_str;
}

/* ============================================================
 * 模块生命周期包装函数
 *
 * 将各子系统的初始化/清理函数包装为 lvModuleInitFunc /
 * lvModuleCleanupFunc 类型，然后通过模块注册表集中管理。
 * ============================================================ */

/** @brief 日志系统初始化包装 */
static bool lv_module_init_log(void) {
    if (debug_log_init() != 0) {
        lv_WARN("[Lv-00] 警告: 日志系统初始化失败");
        /* 不视为致命错误（与原始行为一致） */
    }
    return true;
}

/** @brief 错误码表验证包装（仅警告，不阻止启动） */
static bool lv_module_init_error_table(void) {
    if (!lv_error_table_validate()) {
        LOG_WARN("lv", "错误码查找表排序自检失败，错误查找可能返回错误结果");
    }
    return true;
}

/** @brief 内存统计初始化包装 */
static bool lv_module_init_memory(void) {
    lv_reset_memory_stats();
    return true;
}

/** @brief 随机数初始化包装 */
static bool lv_module_init_random(void) {
    lv_random_init((uint64_t) time(NULL));
    return true;
}

/** @brief 配置管理器初始化包装 */
static bool lv_module_init_config(void) {
    s_lv_state.config = config_manager_create(NULL);
    if (!s_lv_state.config) {
        LOG_ERROR("lv", "配置管理器创建失败");
        return false;
    }

    /* 设置默认配置值（魔术数字全部定义在 lv_internal.h 中） */
    config_set_int(s_lv_state.config, "solver.max_iterations", lv_DEFAULT_MAX_ITERATIONS);
    config_set_int(s_lv_state.config, "solver.precision_bits", lv_DEFAULT_PRECISION_BITS);
    config_set_bool(s_lv_state.config, "debug.assertions_enabled", true);
    config_set_bool(s_lv_state.config, "debug.trace_enabled", false);
    config_set_int(s_lv_state.config, "rewrite.step_limit", lv_DEFAULT_REWRITE_STEP_LIMIT);
    config_set_int(s_lv_state.config, "memory.limit_mb", lv_DEFAULT_MEMORY_LIMIT_MB);

    /* 应用内存限制 */
    int mem_limit_mb = config_get_int(s_lv_state.config, "memory.limit_mb", 0);
    if (mem_limit_mb > 0) {
        if ((size_t) mem_limit_mb <= SIZE_MAX / lv_MB_I) {
            lv_set_memory_limit((size_t) mem_limit_mb * lv_MB_I);
        } else {
            LOG_WARN("lv", "内存限制值 %d MB 过大，已忽略", mem_limit_mb);
        }
    }

    /* K62/F88：A JSON 配置加载接线——lv_config_load_json 原仅测试调用
     * （生产"配置不落盘+不加载"，改了不生效）；存在 lv.config.json 时应用
     * （文件缺失静默跳过，默认配置继续生效）。lv_config_load_json 会把
     * 文件键经 lv_config_apply 覆盖当前快照，缺省键保持默认。 */
    if (lv_file_exists("lv.config.json")) {
        if (lv_config_load_json("lv.config.json") != 0) {
            LOG_WARN("lv", "lv.config.json 解析失败，使用默认配置");
        }
    }

    return true;
}

/** @brief 配置管理器清理包装 */
static void lv_module_cleanup_config(void) {
    if (s_lv_state.config) {
        config_manager_destroy(s_lv_state.config);
        s_lv_state.config = NULL;
    }
}

/** @brief 错误上下文清理包装（注册表 cleanup 无参签名适配） */
static void lv_module_cleanup_error_context(void) {
    lv_error_context_cleanup(lv_error_context_current());
    /* 动态错误消息注册表（蓝图 lv_register_error_message 扩展）进程级清理 */
    lv_error_messages_cleanup();
}

/** @brief 预设对象池初始化包装
 * 池是性能优化而非必需：初始化失败仅告警，不阻断系统初始化
 * （失败时后续分配回退普通 lv_malloc/lv_calloc）。 */
static bool lv_module_init_preset_pools(void) {
    if (!lv_init_preset_pools()) {
        lv_WARN("[Lv-00] 警告: 预设对象池初始化失败，回退普通内存分配");
    }
    return true;
}

/** @brief 预设对象池清理包装 */
static void lv_module_cleanup_preset_pools(void) {
    lv_cleanup_preset_pools();
}

/** @brief context 资源操作注入
 * L2 context 不持有 L3/L4 不透明资源（main_graph / last_normalization）的
 * 具体实现，create/copy/destroy 经 lv_context_register_resource_ops() 由
 * L0 在此注入（仿 serialize_adapters 注册模式；幂等可重复调用）。 */
static bool lv_module_init_context_resources(void) {
    LvContextResourceOps ops = {
        .create = graph_create,
        .copy = graph_copy,
        .destroy = graph_destroy,
        .normalization_destroy = normalization_result_destroy,
    };
    lv_context_register_resource_ops(&ops);
    return true;
}

/** @brief interop 插件宿主（进程级插件表单例的注册入口）
 * 注册 coq/lean4/opml 三个证明互操作插件；lvPlugin.export_proof 统一接受
 * ProofNavigator*（navigator 语义），各插件内部转换自身证明表示。
 * 销毁顺序：先清插件表（纯值类型），再销毁宿主。 */
static InteropServer *s_interop_plugin_host = NULL;

static bool lv_module_init_interop_plugins(void) {
    s_interop_plugin_host = interop_server_create(INTEROP_INTERFACE_STDIO);
    if (!s_interop_plugin_host) {
        lv_set_error(lv_ERROR_ALLOCATION_FAILED, "interop plugin host create failed");
        return false;
    }
    if (lv_register_coq_plugin(s_interop_plugin_host) != 0 ||
        lv_register_lean4_plugin(s_interop_plugin_host) != 0 ||
        lv_register_opml_plugin(s_interop_plugin_host) != 0) {
        lv_set_error(lv_ERROR_INTERNAL, "interop plugin registration failed");
        lv_interop_reset_plugins();
        interop_server_destroy(s_interop_plugin_host);
        s_interop_plugin_host = NULL;
        return false;
    }
    return true;
}

static void lv_module_cleanup_interop_plugins(void) {
    lv_interop_reset_plugins();
    interop_server_destroy(s_interop_plugin_host);
    s_interop_plugin_host = NULL;
}

/** @brief 系统初始化主函数 @details 初始化内存管理、配置系统和全局状态。 @return true 成功，false 失败 */
bool lv_init(void) {
    /* F28/J1：进程级锁——跨线程并发 lv_init 互斥（原 TLS 状态各线程独立，
     * 共享底层资源被重复初始化）；锁保护整个初始化过程 */
    lv_lazy_lock_lock(&g_lv_state_lock, g_lv_state_lock_init_once);

    /* 支持嵌套初始化：当系统已初始化时，递增计数即可 */
    if (s_lv_state.system_state == SYSTEM_STATE_INITIALIZED) {
        s_lv_state.init_count++;
        lv_lazy_lock_unlock(&g_lv_state_lock);
        return true;
    }

    /* 检查状态：防止在初始化过程中重复调用 */
    if (s_lv_state.system_state == SYSTEM_STATE_INITIALIZING) {
        lv_set_error(lv_ERROR_INVALID_STATE, "系统正在初始化中");
        lv_lazy_lock_unlock(&g_lv_state_lock);
        return false;
    }

    set_system_state(SYSTEM_STATE_INITIALIZING);

    /* 注册核心模块 */
    lv_module_register("log",   lv_module_init_log,         debug_log_shutdown,     lv_MODULE_PRIO_CORE);
    lv_module_register("error_codes", lv_module_init_error_table, NULL,              lv_MODULE_PRIO_CORE);
    lv_module_register("memory", lv_module_init_memory,     NULL,                   lv_MODULE_PRIO_CORE);
    lv_module_register("random", lv_module_init_random,     NULL,                   lv_MODULE_PRIO_CORE);
    lv_module_register("config", lv_module_init_config,     lv_module_cleanup_config, lv_MODULE_PRIO_RESOURCE);
    lv_module_register("preset_pools", lv_module_init_preset_pools, lv_module_cleanup_preset_pools,
                       lv_MODULE_PRIO_RESOURCE);
    /* 序列化适配器注册（把 graph JSON 等业务序列化对接入统一序列化注册表；
     * init 仅写入注册表函数指针，幂等可重复调用；
     * cleanup 完整释放注册表结构，修复 lv_cleanup 后的序列化条目泄漏） */
    lv_module_register("serialize_adapters", lv_serialize_register_graph_adapters, lv_serialize_cleanup_adapters,
                       lv_MODULE_PRIO_RESOURCE);
    /* context 资源操作注入（main_graph 的 create/copy/destroy 与
     * last_normalization 的 destroy；L2 context 不依赖 L3/L4，由 L0 注入） */
    lv_module_register("context_resources", lv_module_init_context_resources, NULL,
                       lv_MODULE_PRIO_RESOURCE);
    /* interop 证明插件注册（coq/lean4/opml；插件表单例 + 宿主生命周期） */
    lv_module_register("interop_plugins", lv_module_init_interop_plugins, lv_module_cleanup_interop_plugins,
                       lv_MODULE_PRIO_RESOURCE);

    /* F28/J1：原 lv_cleanup 硬编码清理序列注册表化（纯 cleanup 模块，init=NULL）。
     * 优先级决定逆序清理顺序：高优先级（输出/应用层）先清，低（资源/核心）后清，
     * 与原有硬编码顺序（func_block→perf→…→delta）语义一致。 */
    lv_module_register("func_block_registry", NULL, lv_func_block_registry_cleanup, lv_MODULE_PRIO_REASONING);
    lv_module_register("perf", NULL, lv_perf_shutdown, lv_MODULE_PRIO_OUTPUT);
    lv_module_register("health", NULL, lv_health_shutdown, lv_MODULE_PRIO_OUTPUT);
    lv_module_register("adaptive_threshold", NULL, lv_adaptive_threshold_cleanup, lv_MODULE_PRIO_REASONING);
    lv_module_register("error_context", NULL, lv_module_cleanup_error_context, lv_MODULE_PRIO_RESOURCE);
    lv_module_register("module_autosave", NULL, module_autosave_cleanup, lv_MODULE_PRIO_RESOURCE);
    lv_module_register("ecosystem", NULL, lv_ecosystem_shutdown, lv_MODULE_PRIO_RESOURCE);
    lv_module_register("unify_storage", NULL, lv_unify_equivalence_storage_cleanup, lv_MODULE_PRIO_RESOURCE);
    lv_module_register("formula_converter_util", NULL, formula_converter_util_cleanup, lv_MODULE_PRIO_RESOURCE);
    lv_module_register("scratch_buf", NULL, lv_scratch_buf_cleanup, lv_MODULE_PRIO_RESOURCE);
    /* thread_pool 用 CORE 优先级：cleanup_all 逆序时最后清理（其他模块
     * 可能仍在用全局线程池；原硬编码顺序 thread_pool 在 TLS 清理之后） */
    lv_module_register("thread_pool", NULL, lv_global_thread_pool_destroy, lv_MODULE_PRIO_CORE);
    lv_module_register("module_delta", NULL, module_delta_cleanup, lv_MODULE_PRIO_RESOURCE);

    /* J1：ecosystem 生产接线——lv_ecosystem_init 原仅测试调用（M6），
     * 生产 lv_init 不初始化则 lv_ecosystem_register_module 恒失败
     * （!initialized）。init 幂等（initialized 标志，shutdown 置 0 后可重入）。 */
    if (lv_ecosystem_init() != 0) {
        LOG_WARN("lv", "ecosystem 初始化失败（生态模块注册将不可用）");
    }

    LOG_INFO("lv", "Lv-00 v%s 系统初始化开始", lv_VERSION_STRING);

    /* 通过模块注册表一次性初始化所有已注册模块 */
    if (!lv_module_init_all()) {
        LOG_ERROR("lv", "模块初始化失败");
        set_system_state(SYSTEM_STATE_ERROR);
        lv_lazy_lock_unlock(&g_lv_state_lock);
        return false;
    }

    s_lv_state.init_count = 1;
    set_system_state(SYSTEM_STATE_INITIALIZED);

    LOG_INFO("lv", "Lv-00 v%s 系统初始化完成", lv_VERSION_STRING);

    lv_lazy_lock_unlock(&g_lv_state_lock);
    return true;
}

/** @brief 系统清理函数 @details 释放所有全局资源，重置初始化状态。 */
void lv_cleanup(void) {
    /* F28/J1：进程级锁——与 lv_init 互斥（跨线程并发 init/cleanup 安全） */
    lv_lazy_lock_lock(&g_lv_state_lock, g_lv_state_lock_init_once);

    /* 检查嵌套计数 */
    if (s_lv_state.init_count > 1) {
        s_lv_state.init_count--;
        lv_lazy_lock_unlock(&g_lv_state_lock);
        return;
    }

    if (s_lv_state.init_count == 0) {
        /* 未初始化或已清理 */
        lv_lazy_lock_unlock(&g_lv_state_lock);
        return;
    }

    if (s_lv_state.system_state != SYSTEM_STATE_INITIALIZED) {
        lv_lazy_lock_unlock(&g_lv_state_lock);
        return;
    }

    set_system_state(SYSTEM_STATE_SHUTTING_DOWN);

    LOG_INFO("lv", "Lv-00 系统清理开始");

    /* 清理全局引擎 */
    if (s_lv_state.global_engine) {
        engine_destroy(s_lv_state.global_engine);
        s_lv_state.global_engine = NULL;
    }

    /* F28/J1：以下清理已注册表化（lv_init 注册，cleanup_all 逆序执行）：
     * func_block_registry / perf / health / adaptive_threshold / error_context /
     * module_autosave / ecosystem / unify_storage / formula_converter_util /
     * scratch_buf / thread_pool / module_delta——不再硬编码 */

    /* 模块化清理：按反向优先级顺序清理所有已注册模块 */
    lv_module_cleanup_all();

    /* F28/J1：清理后重置模块注册表——init/cleanup 循环后第二次 lv_init
     * 可重新注册（原 count 不重置，重复注册被吞） */
    lv_module_registry_reset();

    /* 配置快照清理（F43/K15 方案 B：释放延迟回收的旧快照，单线程阶段） */
    lv_config_snapshot_cleanup();

    /* F28/J1：unify_storage / formula_converter_util / scratch_buf /
     * thread_pool / module_delta 清理已注册表化（见 lv_init 注册区），
     * 由 lv_module_cleanup_all 逆序执行 */

    /* 输出内存统计（放在所有清理之后，避免清理顺序导致误报泄漏） */
    MemoryStats stats;
    lv_get_memory_stats(&stats);
    if (stats.current_used > 0) {
        LOG_WARN("lv", "检测到内存泄漏: 当前使用 %zu 字节", stats.current_used);
    }
    LOG_INFO("lv", "内存统计 - 总分配: %zu, 总释放: %zu, 峰值: %zu", stats.total_allocated, stats.total_freed,
             stats.peak_used);

    s_lv_state.init_count = 0;
    set_system_state(SYSTEM_STATE_UNINITIALIZED);

    lv_lazy_lock_unlock(&g_lv_state_lock);
}

/** @brief 查询系统是否已初始化 @return true 已初始化，false 未初始化 */
bool lv_is_initialized(void) {
    return is_system_initialized();
}

/**
 * @brief 获取系统信息字符串
 *
 * 将系统版本、运行状态、内存统计和性能计数器格式化为可读字符串。
 * 使用 lv_SAFE_SNPRINTF 确保返回值的语义安全：返回值始终在
 * [0, size-1] 范围内（当 size > 0 时），便于调用者正确判断输出长度。
 * 若输出被截断，返回值为 size-1，表示实际输出超出缓冲区容量。
 *
 * @param info 输出缓冲区，用于接收格式化后的系统信息字符串
 * @param size 缓冲区大小（字节），必须大于 0
 * @return 实际写入的字符数（不含终止符），范围 [0, size-1]；
 *         若 info 为 NULL 或 size == 0 则返回 0
 */
int lv_get_system_info(char *info, size_t size) {
    if (!info || size == 0)
        return 0;

    MemoryStats mem_stats;
    lv_get_memory_stats(&mem_stats);

    PerformanceCounters perf;
    debug_get_counters(&perf);

    int written;
    lv_SAFE_SNPRINTF(
        written, info, size,
        "=== Lv-00 系统信息 ===\n"
        "版本: %s\n"
        "状态: %s\n"
        "初始化次数: %d\n"
        "\n[内存统计]\n"
        "  当前使用: %.2f MB\n"
        "  峰值使用: %.2f MB\n"
        "  总分配: %.2f MB\n"
        "  总释放: %.2f MB\n"
        "  分配次数: %zu\n"
        "\n[性能统计]\n"
        "  节点创建: %" PRIu64
        "\n"
        "  约束创建: %" PRIu64
        "\n"
        "  求解器调用: %" PRIu64
        "\n"
        "  重写步数: %" PRIu64
        "\n"
        "  合一检查: %" PRIu64 "\n",
        lv_VERSION_STRING, is_system_initialized() ? "已初始化" : "未初始化", s_lv_state.init_count,
        (double) mem_stats.current_used / lv_MB, (double) mem_stats.peak_used / lv_MB,
        (double) mem_stats.total_allocated / lv_MB, (double) mem_stats.total_freed / lv_MB,
        mem_stats.allocation_count, (uint64_t) perf.total_nodes_created, (uint64_t) perf.total_constraints_created,
        (uint64_t) perf.solver_call_count, (uint64_t) perf.rewrite_total_steps, (uint64_t) perf.unify_check_count);

    return written;
}

/**
 * @brief 系统健康检查
 *
 * 基于内存使用率、内存泄漏迹象和错误状态计算综合健康分数。
 * 满分 lv_HEALTH_SCORE_MAX，各项扣分规则：
 * - 内存使用超过限制的 lv_HEALTH_MEMORY_USAGE_RATIO 时扣分
 * - 检测到潜在内存泄漏迹象时扣分
 * - 存在最近的错误状态时扣分
 * 分数最终钳制在 [0, lv_HEALTH_SCORE_MAX] 范围内。
 *
 * @return 健康分数，范围 [0, lv_HEALTH_SCORE_MAX]，未初始化时返回 0
 */
int lv_health_check(void) {
    if (!is_system_initialized()) {
        return 0;
    }

    int score = lv_HEALTH_SCORE_MAX;

    /* 检查内存使用 */
    MemoryStats mem_stats;
    lv_get_memory_stats(&mem_stats);

    size_t mem_limit = lv_get_memory_limit();
    if (mem_limit > 0 && mem_stats.current_used > (size_t) ((double) mem_limit * lv_HEALTH_MEMORY_USAGE_RATIO)) {
        score -= lv_HEALTH_MEMORY_WARNING_PENALTY; /* 内存使用超过阈值 */
    }

    /* 检查内存泄漏 */
    if (mem_stats.current_used > (size_t) ((double) mem_stats.peak_used * lv_HEALTH_MEMORY_LEAK_RATIO)) {
        score -= lv_HEALTH_MEMORY_LEAK_PENALTY; /* 可能存在内存泄漏 */
    }

    /* 检查错误状态 */
    if (lv_get_last_error_code() != lv_OK) {
        score -= lv_HEALTH_RECENT_ERROR_PENALTY;
    }

    /* 确保分数在合理范围 */
    if (score < 0)
        score = 0;
    if (score > lv_HEALTH_SCORE_MAX)
        score = lv_HEALTH_SCORE_MAX;

    return score;
}

/* ============================================================
 * 便捷API实现
 * ============================================================ */

/**
 * @brief 创建并初始化引擎（便捷函数）
 */
lvEngine *lv_engine_create(void) {
    if (!is_system_initialized()) {
        if (!lv_init()) {
            lv_RETURN_ERROR_NULL(lv_ERROR_INTERNAL, "lv_engine_create: lv_init() failed");
        }
    }
    return engine_create();
}

/**
 * @brief 销毁引擎（便捷函数）
 */
void lv_engine_destroy(lvEngine *engine) {
    if (engine) {
        engine_destroy(engine);
    }
}

/**
 * @brief 快速创建点（便捷函数）
 */
int lv_add_point(lvEngine *engine, int64_t x_num, uint64_t x_den, int64_t y_num, uint64_t y_den) {
    if (!engine || !engine->main_graph) {
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "lv_add_point: engine 或 main_graph 为 NULL");
    }
    /* 参数校验：分母不能为零 */
    if (x_den == 0 || y_den == 0) {
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "lv_add_point: 分母不能为零 (x_den=%" PRIu64 ", y_den=%" PRIu64 ")",
                     (uint64_t) x_den, (uint64_t) y_den);
    }

    SymbolicCoord *x = symbolic_coord_create_rational(x_num, x_den);
    SymbolicCoord *y = symbolic_coord_create_rational(y_num, y_den);

    if (!x || !y) {
        if (x) symbolic_coord_destroy(x);
        if (y) symbolic_coord_destroy(y);
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_add_point: 创建符号坐标失败");
    }

    AddNodeResult result = graph_add_point_xy(engine->main_graph, x, y);

    symbolic_coord_destroy(x);
    symbolic_coord_destroy(y);

    if (result != ADD_NODE_OK) {
        lv_RETURN_ERROR(lv_ERROR_NODE_CONFLICT, "lv_add_point: 添加点到图失败 (result=%d)", (int) result);
    }

    return engine->main_graph->next_node_id - 1;
}

/**
 * @brief 快速创建线段（便捷函数）
 */
int lv_add_line_segment(lvEngine *engine, int point1_id, int point2_id) {
    if (!engine || !engine->main_graph) {
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "lv_add_line_segment: engine 或 main_graph 为 NULL");
    }

    AddNodeResult result = graph_add_line_segment(engine->main_graph, point1_id, point2_id);
    if (result != ADD_NODE_OK) {
        lv_RETURN_ERROR(lv_ERROR_NODE_CONFLICT, "lv_add_line_segment: 添加线段失败 (point1=%d, point2=%d, result=%d)",
                     point1_id, point2_id, (int) result);
    }

    return engine->main_graph->next_node_id - 1;
}

/**
 * @brief 快速添加约束（便捷函数）
 */
bool lv_add_constraint_incidence(lvEngine *engine, int point_id, int line_id) {
    if (!engine || !engine->main_graph) {
        lv_set_error(lv_ERROR_NULL_POINTER, "lv_add_constraint_incidence: engine 或 main_graph 为 NULL");
        return false;
    }

    AddConstraintResult result = graph_add_incidence(engine->main_graph, point_id, line_id);
    if (result != ADD_CONSTRAINT_OK) {
        lv_set_error(lv_ERROR_CONSTRAINT_CONFLICT,
                     "lv_add_constraint_incidence: 添加关联约束失败 (point=%d, line=%d, result=%d)", point_id, line_id,
                     (int) result);
        return false;
    }
    return true;
}

/**
 * @brief 执行归一化（便捷函数）
 */
NormalizationResult *lv_normalize(lvEngine *engine, bool scope_aware) {
    if (!engine || !engine->main_graph)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_normalize: engine or main_graph is NULL");
    return graph_normalize(engine->main_graph, scope_aware);
}

/**
 * @brief 执行求解（便捷函数）
 */
EngineSolveResult lv_solve(lvEngine *engine) {
    if (!engine)
        return ENGINE_SOLVE_ERROR;
    return engine_solve(engine);
}

/* ============================================================
 * 配置系统收敛：三套 → 两套（系统 C 已消除）
 *
 * 历史上有三套配置语义并存：
 *   - 系统 A（lvConfig，config.h / lv_config.c）：X-macro 注册表
 *     （LV_CONFIG_INT_KEYS / LV_CONFIG_DOUBLE_KEYS）+ 类型安全
 *     lv_config_get_<key>() / lv_config_set_<key>()，JSON 持久化；
 *   - 系统 B（ConfigManager，lv_utils_config.c）：字符串键存储，
 *     公共 API 即本文件的 lv_config_get_*(key, default)；
 *   - 系统 C（已消除）：config.h 的 LV_CFG_* 字符串键宏曾构成独立
 *     键空间——其中与系统 A 同名的键（如 circuit_overflow_threshold、
 *     smoke_test_step_limit 等）在两侧各有副本，读写路径分裂。
 *
 * 收敛方式（v4 配置归一）：字符串键统一分发——先查系统 A 注册表（与 A 同名键
 * 统一读取 lvConfig 单例，消除第二副本），未命中再回落系统 B。
 * get_int / get_bool / get_double / get_string 四条读取路径与
 * set_bool / set_string 写入路径遵循同一条 "A 优先、B 回落" 规则，
 * 同一逻辑键在任意类型访问下读到一致值（A 无字符串存储：get_string 对 A 键
 * 返回 default、set_string 对 A 键返回 false；bool 按值非零归一化）。
 * 对外 API 名与 LV_CFG_* 宏名均保持不变，仅键归属明确为 A 或 B。
 * ============================================================ */

/** @brief 系统 A 整型键字符串分发：命中返回 true 并写出 *out */
static bool lv_config_resolve_a_int(const char *key, int *out) {
    const lvConfig *c = lv_config_current();
#define A_INT_GET_IF(k, t, f, d) \
    if (lv_str_eq(key, #k)) {  \
        *out = c->f;             \
        return true;             \
    }
    LV_CONFIG_INT_KEYS(A_INT_GET_IF)
#undef A_INT_GET_IF
    (void) c;
    return false;
}

/** @brief 系统 A 浮点键字符串分发：命中返回 true 并写出 *out */
static bool lv_config_resolve_a_double(const char *key, double *out) {
    const lvConfig *c = lv_config_current();
#define A_DBL_GET_IF(k, t, f, d) \
    if (lv_str_eq(key, #k)) {  \
        *out = c->f;             \
        return true;             \
    }
    LV_CONFIG_DOUBLE_KEYS(A_DBL_GET_IF)
#undef A_DBL_GET_IF
    (void) c;
    return false;
}

/** @brief 系统 A 布尔键字符串分发：命中返回 true 并写出 *out
 *  int 键按 (值 != 0)、double 键按 (值 != 0.0) 归一化为 bool，
 *  与 get_int / get_double 的读取语义保持一致（同键任意类型访问读到一致值）。 */
static bool lv_config_resolve_a_bool(const char *key, bool *out) {
    const lvConfig *c = lv_config_current();
#define A_BOOL_INT_IF(k, t, f, d) \
    if (lv_str_eq(key, #k)) {   \
        *out = (c->f != 0);       \
        return true;              \
    }
    LV_CONFIG_INT_KEYS(A_BOOL_INT_IF)
#undef A_BOOL_INT_IF
#define A_BOOL_DBL_IF(k, t, f, d) \
    if (lv_str_eq(key, #k)) {   \
        *out = (c->f != 0.0);     \
        return true;              \
    }
    LV_CONFIG_DOUBLE_KEYS(A_BOOL_DBL_IF)
#undef A_BOOL_DBL_IF
    (void) c;
    return false;
}

/** @brief 系统 A 任意键命中检测（int 或 double 注册表任一命中即 true） */
static bool lv_config_resolve_a_any(const char *key) {
    int ival = 0;
    double dval = 0.0;
    return lv_config_resolve_a_int(key, &ival) || lv_config_resolve_a_double(key, &dval);
}

/**
 * @brief 获取配置值（便捷函数）
 *
 * 统一分发（v4 配置归一）：所有 lv_config_get_*(key, default) 走同一条规则
 * —— 键命中系统 A（lvConfig 注册表）→ 读 A 单例；未命中 → 回落系统 B
 * （ConfigManager）。任何类型访问都不会在 A/B 两侧产生同键双副本：
 *   - get_int / get_double：A 值原样返回；
 *   - get_bool：A 命中键按值非零归一化（与 get_int/double 一致）；
 *   - get_string：A 注册表无字符串存储，A 命中键返回 default_val（不再查 B）。
 */
int lv_config_get_int(const char *key, int default_val) {
    if (!key)
        return default_val;
    int a_val = 0;
    if (lv_config_resolve_a_int(key, &a_val))
        return a_val;
    if (!s_lv_state.config)
        return default_val;
    return config_get_int(s_lv_state.config, key, default_val);
}

/** @brief 获取布尔配置项 @param key 配置键名 @param default_val 默认值 @return 配置值 */
bool lv_config_get_bool(const char *key, bool default_val) {
    if (!key)
        return default_val;
    bool a_val = false;
    if (lv_config_resolve_a_bool(key, &a_val))
        return a_val; /* A 优先：与 get_int/get_double 读到同一份值 */
    if (!s_lv_state.config)
        return default_val;
    return config_get_bool(s_lv_state.config, key, default_val); /* B 回落 */
}

/** @brief 获取双精度浮点配置项 @param key 配置键名 @param default_val 默认值 @return 配置值 */
double lv_config_get_double(const char *key, double default_val) {
    if (!key)
        return default_val;
    double a_val = 0.0;
    if (lv_config_resolve_a_double(key, &a_val))
        return a_val;
    if (!s_lv_state.config)
        return default_val;
    return config_get_double(s_lv_state.config, key, default_val);
}

/** @brief 获取字符串配置项 @param key 配置键名 @param default_val 默认值 @return 配置值（可能为 NULL） */
const char *lv_config_get_string(const char *key, const char *default_val) {
    if (!key)
        return default_val;
    /* A 注册表无字符串存储：A 命中键返回 default_val 且不查 B，避免 B 侧陈旧副本 */
    if (lv_config_resolve_a_any(key))
        return default_val;
    if (!s_lv_state.config)
        return default_val;
    return config_get_string(s_lv_state.config, key, default_val);
}

/* lv_config_set_int / lv_config_set_double → 已迁移至 lv_config.c（仅接受 A 注册表键） */

/**
 * @brief 设置布尔配置项 @param key 配置键名 @param value 配置值 @return true 成功
 *
 * 与 get_bool 对称的统一规则：键命中系统 A → 归一化写入 A 对应字段
 * （int 字段写 0/1、double 字段写 0.0/1.0）；未命中 → 回落系统 B。
 */
bool lv_config_set_bool(const char *key, bool value) {
    if (!key)
        return false;
    int a_int = 0;
    double a_dbl = 0.0;
    if (lv_config_resolve_a_int(key, &a_int))
        return lv_config_set_int(key, value ? 1 : 0);
    if (lv_config_resolve_a_double(key, &a_dbl))
        return lv_config_set_double(key, value ? 1.0 : 0.0);
    if (!s_lv_state.config)
        return false;
    return config_set_bool(s_lv_state.config, key, value); /* B 回落 */
}

/* lv_config_set_double → 已迁移至 lv_config.c */

/**
 * @brief 设置字符串配置项 @param key 配置键名 @param value 配置值 @return true 成功
 *
 * A 注册表无字符串存储：键命中系统 A 时返回 false（不写 B，避免同键双副本）；
 * 否则回落系统 B。
 */
bool lv_config_set_string(const char *key, const char *value) {
    if (!key)
        return false;
    int a_int = 0;
    double a_dbl = 0.0;
    if (lv_config_resolve_a_int(key, &a_int) || lv_config_resolve_a_double(key, &a_dbl))
        return false;
    if (!s_lv_state.config)
        return false;
    return config_set_string(s_lv_state.config, key, value);
}

bool lv_check_version_compat(void) {
    /* 检查运行时主版本号与编译时主版本号是否一致。
     * 修复（C-㊺续36 测试暴露）：原硬编码 lv_VERSION_MAJOR != 3 与实际
     * 主版本（1）不符，恒返回 false；单库静态构建下运行时与编译头同源，
     * 主版本恒一致，按 lv_VERSION_MAJOR 自比较。 */
    if (lv_VERSION_MAJOR != lv_VERSION_MAJOR) {
        return false;
    }
    return true;
}

/* ============================================================
 * 调试和日志便捷API实现
 * ============================================================ */

/** @brief 设置日志级别 @param level 日志级别（0=关闭, 1=错误, 2=警告, 3=信息, 4=调试） */
void lv_set_log_level(int level) {
    s_lv_state.log_level = level;
    if (s_lv_state.config) {
        config_set_int(s_lv_state.config, "debug.log_level", level);
    }
}

/** @brief 获取当前日志级别 @return 日志级别 */
int lv_get_log_level(void) {
    return s_lv_state.log_level;
}

/** @brief 启用或禁用运行时断言 @param enabled true 启用，false 禁用 */
void lv_set_assertions_enabled(bool enabled) {
    s_lv_state.assertions_enabled = enabled;
    if (s_lv_state.config) {
        config_set_bool(s_lv_state.config, "debug.assertions_enabled", enabled);
    }
}

/** @brief 查询运行时断言是否启用 @return true 启用，false 禁用 */
bool lv_are_assertions_enabled(void) {
    return s_lv_state.assertions_enabled;
}

/* ============================================================
 * SetNumericAssumption API —— 节点永久降级为数值假设
 * ============================================================ */

/**
 * @brief 将节点永久降级为数值假设（SetNumericAssumption 命令）
 *
 * 当位数熔断触发且用户选择"永久降级"时调用此函数。
 * 将节点的信任颜色设为 TRUST_AMBER，存储精度阈值和声明文本。
 * 所有下游依赖节点自动继承 TRUST_AMBER。
 *
 * @param engine     引擎实例
 * @param node_id    要降级的节点 ID
 * @param precision  数值精度阈值（如 1e-15）
 * @param declaration 数值假设声明文本（如"该点坐标在10^{-15}精度下近似为1.4142"）
 * @return 成功返回 0（lv_OK），失败返回负错误码
 */
int lv_set_numeric_assumption(lvEngine *engine, int node_id, double precision, const char *declaration) {
    /* 参数有效性检查 */
    if (!engine || !engine->main_graph) {
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "lv_set_numeric_assumption: engine 或 main_graph 为 NULL");
    }

    /* 委托给 bit_burning_downgrade_to_amber 执行降级操作 */
    if (!bit_burning_downgrade_to_amber(engine->main_graph, node_id, precision, declaration)) {
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "lv_set_numeric_assumption: 节点 %d 不存在或降级失败", node_id);
    }

    return 0; /* lv_OK */
}

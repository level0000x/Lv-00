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

#include "lv.h"
#include "lv/lv_log.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lv/bit_burning.h"
#include "lv/lv_registry.h"
#include "lv/memory_pool.h"

#include "func_block_registry.h"
#include "lv_internal.h"
#include "lv/lv_str_utils.h"

/* ============================================================
 * 全局状态管理
 * ============================================================ */

/**
 * @brief 系统初始化状态
 */
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

/** 模块级唯一状态实例（线程局部） */
static lv_THREAD_LOCAL LvState s_lv_state = {0};

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
        snprintf(version_str, sizeof(version_str), "%d.%d.%d", lv_VERSION_MAJOR, lv_VERSION_MINOR, lv_VERSION_PATCH);
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
        if ((size_t) mem_limit_mb <= SIZE_MAX / (1024 * 1024)) {
            lv_set_memory_limit((size_t) mem_limit_mb * 1024 * 1024);
        } else {
            LOG_WARN("lv", "内存限制值 %d MB 过大，已忽略", mem_limit_mb);
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

/** @brief 系统初始化主函数 @details 初始化内存管理、配置系统和全局状态。 @return true 成功，false 失败 */
bool lv_init(void) {
    /* 支持嵌套初始化：当系统已初始化时，递增计数即可 */
    if (s_lv_state.system_state == SYSTEM_STATE_INITIALIZED) {
        s_lv_state.init_count++;
        return true;
    }

    /* 检查状态：防止在初始化过程中重复调用 */
    if (s_lv_state.system_state == SYSTEM_STATE_INITIALIZING) {
        lv_set_error(lv_ERROR_INVALID_STATE, "系统正在初始化中");
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

    LOG_INFO("lv", "Lv-00 v%s 系统初始化开始", lv_VERSION_STRING);

    /* 通过模块注册表一次性初始化所有已注册模块 */
    if (!lv_module_init_all()) {
        LOG_ERROR("lv", "模块初始化失败");
        set_system_state(SYSTEM_STATE_ERROR);
        return false;
    }

    s_lv_state.init_count = 1;
    set_system_state(SYSTEM_STATE_INITIALIZED);

    LOG_INFO("lv", "Lv-00 v%s 系统初始化完成", lv_VERSION_STRING);

    return true;
}

/** @brief 系统清理函数 @details 释放所有全局资源，重置初始化状态。 */
void lv_cleanup(void) {
    /* 检查嵌套计数 */
    if (s_lv_state.init_count > 1) {
        s_lv_state.init_count--;
        return;
    }

    if (s_lv_state.init_count == 0) {
        /* 未初始化或已清理 */
        return;
    }

    if (s_lv_state.system_state != SYSTEM_STATE_INITIALIZED) {
        return;
    }

    set_system_state(SYSTEM_STATE_SHUTTING_DOWN);

    LOG_INFO("lv", "Lv-00 系统清理开始");

    /* 清理全局引擎 */
    if (s_lv_state.global_engine) {
        engine_destroy(s_lv_state.global_engine);
        s_lv_state.global_engine = NULL;
    }

    /* 清理函数块注册表 */
    lv_func_block_registry_cleanup();

    /* 输出内存统计 */
    MemoryStats stats;
    lv_get_memory_stats(&stats);
    if (stats.current_used > 0) {
        LOG_WARN("lv", "检测到内存泄漏: 当前使用 %zu 字节", stats.current_used);
    }
    LOG_INFO("lv", "内存统计 - 总分配: %zu, 总释放: %zu, 峰值: %zu", stats.total_allocated, stats.total_freed,
             stats.peak_used);

    /* 模块化清理：按反向优先级顺序清理所有已注册模块 */
    lv_module_cleanup_all();

    s_lv_state.init_count = 0;
    set_system_state(SYSTEM_STATE_UNINITIALIZED);
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
        (double) mem_stats.current_used / (1024.0 * 1024.0), (double) mem_stats.peak_used / (1024.0 * 1024.0),
        (double) mem_stats.total_allocated / (1024.0 * 1024.0), (double) mem_stats.total_freed / (1024.0 * 1024.0),
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

    /* 二维坐标维度常量 */
    const int coord_dim = 2;
    SymbolicCoord *coords[] = {x, y};
    AddNodeResult result = graph_add_point(engine->main_graph, coords, coord_dim);

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
 * 收敛方式：字符串键统一分发——先查系统 A 注册表（与 A 同名键
 * 统一读取 lvConfig 单例，消除第二副本），未命中再回落系统 B。
 * 对外 API 名与 LV_CFG_* 宏名均保持不变，仅键归属明确为 A 或 B。
 * ============================================================ */

/** @brief 系统 A 整型键字符串分发：命中返回 true 并写出 *out */
static bool lv_config_resolve_a_int(const char *key, int *out) {
    const lvConfig *c = lv_config_current();
#define A_INT_GET_IF(k, t, f, d) \
    if (strcmp(key, #k) == 0) {  \
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
    if (strcmp(key, #k) == 0) {  \
        *out = c->f;             \
        return true;             \
    }
    LV_CONFIG_DOUBLE_KEYS(A_DBL_GET_IF)
#undef A_DBL_GET_IF
    (void) c;
    return false;
}

/**
 * @brief 获取配置值（便捷函数）
 *
 * 统一分发：与系统 A（lvConfig 注册表）同名的键读 A 单例；
 * 其余键回落系统 B（ConfigManager）。
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
    if (!s_lv_state.config)
        return default_val;
    return config_get_bool(s_lv_state.config, key, default_val);
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
    if (!s_lv_state.config)
        return default_val;
    return config_get_string(s_lv_state.config, key, default_val);
}

/* lv_config_set_int / lv_config_set_double → 已迁移至 lv_config.c */

/** @brief 设置布尔配置项 @param key 配置键名 @param value 配置值 @return true 成功 */
bool lv_config_set_bool(const char *key, bool value) {
    if (!s_lv_state.config)
        return false;
    return config_set_bool(s_lv_state.config, key, value);
}

/* lv_config_set_double → 已迁移至 lv_config.c */

/** @brief 设置字符串配置项 @param key 配置键名 @param value 配置值 @return true 成功 */
bool lv_config_set_string(const char *key, const char *value) {
    if (!s_lv_state.config)
        return false;
    return config_set_string(s_lv_state.config, key, value);
}

/* ============================================================
 * 版本信息 API
 * ============================================================ */

bool lv_get_version_info(lvVersionInfo *info) {
    if (!info)
        return false;

    info->major = lv_VERSION_MAJOR;
    info->minor = lv_VERSION_MINOR;
    info->patch = lv_VERSION_PATCH;
    info->version_string = lv_get_version_string();

#if defined(_WIN32) || defined(_WIN64)
    info->platform = "Windows";
#elif defined(__APPLE__)
    info->platform = "macOS";
#elif defined(__linux__)
    info->platform = "Linux";
#elif defined(__FreeBSD__)
    info->platform = "FreeBSD";
#else
    info->platform = "Unknown";
#endif

#if defined(_MSC_VER)
    info->compiler = "MSVC";
#elif defined(__GNUC__)
    info->compiler = "GCC";
#elif defined(__clang__)
    info->compiler = "Clang";
#else
    info->compiler = "Unknown";
#endif

#if defined(_WIN64) || defined(__x86_64__)
    info->arch = "x86_64";
#elif defined(__aarch64__)
    info->arch = "ARM64";
#elif defined(_WIN32) || defined(__i386__)
    info->arch = "x86";
#elif defined(__arm__)
    info->arch = "ARM";
#else
    info->arch = "Unknown";
#endif

    info->build_date = __DATE__;
    info->build_time = __TIME__;

    return true;
}

bool lv_check_version_compat(void) {
    /* 检查运行时主版本号与编译时主版本号是否一致 */
    if (lv_VERSION_MAJOR != 3) {
        return false;
    }
    return true;
}

/* ============================================================
 * 内存管理便捷API实现
 * ============================================================ */

/* ===== 向后兼容保留的别名（已弃用）=====
 * 本函数纯做 1:1 转发到 lv_get_memory_stats()，仅为旧版调用方提供兼容。
 * 新代码请直接使用 lv_get_memory_stats()。
 * 该函数计划在后续主版本中移除。
 */
lv_DEPRECATED("use lv_get_memory_stats instead")
    /**
 * @brief 获取扩展内存统计信息（便捷封装）
 *
 * @param stats  输出参数，用于接收内存统计信息
 * @return true  成功获取并写入统计信息
 * @return false stats 为 NULL 指针，未执行任何操作
 * @note 内部委托 lv_get_memory_stats() 完成实际统计
 */
    bool lv_get_memory_stats_ex(MemoryStats *stats) {
    if (!stats)
        return false;
    lv_get_memory_stats(stats);
    return true;
}

/* ===== 向后兼容保留的别名（已弃用）=====
 * 本函数纯做 1:1 转发到 lv_set_memory_limit()，仅为旧版调用方提供兼容。
 * 新代码请直接使用 lv_set_memory_limit()。
 * 该函数计划在后续主版本中移除。
 */
lv_DEPRECATED("use lv_set_memory_limit instead") void lv_set_memory_limit_ex(size_t limit_bytes) {
    lv_set_memory_limit(limit_bytes);
}

/* ===== 向后兼容保留的别名（已弃用）=====
 * 本函数纯做 1:1 转发到 lv_get_memory_limit()，仅为旧版调用方提供兼容。
 * 新代码请直接使用 lv_get_memory_limit()。
 * 该函数计划在后续主版本中移除。
 */
lv_DEPRECATED("use lv_get_memory_limit instead") size_t lv_get_memory_limit_ex(void) {
    return lv_get_memory_limit();
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

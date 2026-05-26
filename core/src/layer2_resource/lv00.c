/**
 * @file lv00.c
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

#include "lv00.h"
#include "memory_pool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>

#include "func_block_registry.h"
#include "lv00_internal.h"

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
 * @brief 全局系统状态（线程局部）
 *
 * 【线程安全设计说明】
 * 使用 LV00_THREAD_LOCAL 宏将系统状态声明为线程局部存储（TLS），
 * 意味着每个线程拥有独立的 g_system_state 副本，互不干扰。
 *
 * 设计意图：
 * - 支持多线程并行求解：每个线程可独立初始化引擎、执行求解，无需加锁
 * - 避免全局状态的竞态条件：不同线程的初始化/清理不会相互影响
 * - 嵌套调用安全：同一线程内的嵌套初始化通过 g_init_count 计数器管理
 *
 * 注意事项：
 * - 跨线程传递几何对象时，需确保对象本身是线程安全的（如引用计数正确）
 * - TLS 变量在主线程退出时自动销毁，但建议显式调用 lv00_cleanup()
 */
static LV00_THREAD_LOCAL SystemState g_system_state = SYSTEM_STATE_UNINITIALIZED;

/**
 * @brief 初始化计数器（支持嵌套初始化和清理）
 */
static LV00_THREAD_LOCAL int g_init_count = 0;

/**
 * @brief 系统配置
 */
static LV00_THREAD_LOCAL ConfigManager *g_config = NULL;

/**
 * @brief 全局引擎实例（可选的单例模式）
 */
static LV00_THREAD_LOCAL LV00Engine *g_global_engine = NULL;

/**
 * @brief 日志级别（默认：信息级别）
 */
static LV00_THREAD_LOCAL int g_log_level = LV00_LOG_LEVEL_INFO;

/**
 * @brief 断言启用状态
 *
 * 【线程安全性说明】
 *   g_assertions_enabled 使用 LV00_THREAD_LOCAL 宏声明为线程局部存储。
 *   每个操作系统线程拥有独立的副本，因此：
 *   - 不同线程可以独立设置和读取自己的断言启用状态，不会产生数据竞争。
 *   - 同一线程内的读写是顺序一致的。
 *   - 如果需要全局统一的断言控制（所有线程共享同一开关），
 *     应考虑使用 C11 的 atomic_bool 或互斥锁保护的全局变量。
 *     当前设计选择线程局部存储，是因为断言检查通常与特定线程的
 *     调试上下文相关，不同线程可能有不同的调试需求。
 */
static LV00_THREAD_LOCAL bool g_assertions_enabled = true;

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
    g_system_state = state;
}

/**
 * @brief 获取系统状态
 *
 * @return 当前系统状态值
 * @note 仅供内部使用，读取全局系统状态变量
 */
static SystemState get_system_state(void) {
    return g_system_state;
}

/**
 * @brief 检查系统是否已初始化
 *
 * @return true  系统已处于初始化完成状态
 * @return false 系统尚未初始化或处于其他状态
 * @note 通过比较全局状态是否等于 SYSTEM_STATE_INITIALIZED 来判断
 */
static bool is_system_initialized(void) {
    return g_system_state == SYSTEM_STATE_INITIALIZED;
}

/* ============================================================
 * 公共API实现
 * ============================================================ */

/**
 * @brief 获取版本字符串
 * @return 版本号字符串，格式 "major.minor.patch"
 */
const char *lv00_get_version_string(void) {
    static LV00_THREAD_LOCAL char version_str[32] = {0};
    if (version_str[0] == '\0') {
        snprintf(version_str, sizeof(version_str), "%d.%d.%d",
                 LV00_VERSION_MAJOR, LV00_VERSION_MINOR, LV00_VERSION_PATCH);
    }
    return version_str;
}

/**
 * @brief 系统初始化主函数
 * @details 初始化内存管理、配置系统和全局状态。
 *          支持嵌套初始化（递增引用计数），需与 lv00_cleanup 配对使用。
 * @return true 初始化成功，false 初始化失败
 */
bool lv00_init(void) {
    /* 支持嵌套初始化：当系统已初始化时，递增计数即可 */
    if (g_system_state == SYSTEM_STATE_INITIALIZED) {
        g_init_count++;
        return true;
    }

    /* 检查状态：防止在初始化过程中重复调用 */
    if (g_system_state == SYSTEM_STATE_INITIALIZING) {
        lv00_set_error(LV00_ERROR_INVALID_STATE, "系统正在初始化中");
        return false;
    }

    set_system_state(SYSTEM_STATE_INITIALIZING);

    /* 初始化日志系统 */
    if (debug_log_init() != 0) {
        fprintf(stderr, "[Lv-00] 警告: 日志系统初始化失败\n");
        /* 继续，不视为致命错误 */
    }

    LOG_INFO("lv00", "Lv-00 v%s 系统初始化开始", LV00_VERSION_STRING);

    /* 运行时验证错误码查找表排序正确性 */
    if (!lv00_error_table_validate()) {
        LOG_WARN("lv00", "错误码查找表排序自检失败，错误查找可能返回错误结果");
        /* 这是警告而非致命错误——系统仍可运行，但错误信息可能不准确 */
    }

    /* 初始化内存统计 */
    lv00_reset_memory_stats();

    /* 初始化随机数生成器 */
    lv00_random_init((uint64_t) time(NULL));

    /* 加载默认配置 */
    g_config = config_manager_create(NULL);
    if (!g_config) {
        LOG_ERROR("lv00", "配置管理器创建失败");
        set_system_state(SYSTEM_STATE_ERROR);
        return false;
    }

    /* 设置默认配置值（魔术数字全部定义在 lv00_internal.h 中） */
    config_set_int(g_config, "solver.max_iterations", LV00_DEFAULT_MAX_ITERATIONS);
    config_set_int(g_config, "solver.precision_bits", LV00_DEFAULT_PRECISION_BITS);
    config_set_bool(g_config, "debug.assertions_enabled", true);
    config_set_bool(g_config, "debug.trace_enabled", false);
    config_set_int(g_config, "rewrite.step_limit", LV00_DEFAULT_REWRITE_STEP_LIMIT);
    config_set_int(g_config, "memory.limit_mb", LV00_DEFAULT_MEMORY_LIMIT_MB); /* 0 = 无限制 */

    /* 应用内存限制 */
    int mem_limit_mb = config_get_int(g_config, "memory.limit_mb", 0);
    if (mem_limit_mb > 0) {
        /* 添加溢出检查：确保内存限制值不会溢出 size_t */
        if ((size_t) mem_limit_mb <= SIZE_MAX / (1024 * 1024)) {
            lv00_set_memory_limit((size_t) mem_limit_mb * 1024 * 1024);
        } else {
            LOG_WARN("lv00", "内存限制值 %d MB 过大，已忽略", mem_limit_mb);
        }
    }

    g_init_count = 1;
    set_system_state(SYSTEM_STATE_INITIALIZED);

    LOG_INFO("lv00", "Lv-00 v%s 系统初始化完成", LV00_VERSION_STRING);

    return true;
}

/**
 * @brief 系统清理函数
 * @details 释放所有全局资源，重置初始化状态。
 *          支持嵌套清理（递减引用计数），需与 lv00_init 配对使用。
 */
void lv00_cleanup(void) {
    /* 检查嵌套计数 */
    if (g_init_count > 1) {
        g_init_count--;
        return;
    }

    if (g_init_count == 0) {
        /* 未初始化或已清理 */
        return;
    }

    if (g_system_state != SYSTEM_STATE_INITIALIZED) {
        return;
    }

    set_system_state(SYSTEM_STATE_SHUTTING_DOWN);

    LOG_INFO("lv00", "Lv-00 系统清理开始");

    /* 清理全局引擎 */
    if (g_global_engine) {
        engine_destroy(g_global_engine);
        g_global_engine = NULL;
    }

    /* 清理配置管理器 */
    if (g_config) {
        config_manager_destroy(g_config);
        g_config = NULL;
    }

    /* 清理函数块注册表 */
    func_block_registry_cleanup();

    /* 关闭日志系统会释放其内部缓冲区；先关闭再统计，避免将日志系统自身计入泄漏 */
    debug_log_shutdown();

    /* 输出内存统计 */
    MemoryStats stats;
    lv00_get_memory_stats(&stats);
    if (stats.current_used > 0) {
        fprintf(stderr, "[Lv-00] 警告: 检测到内存泄漏: 当前使用 %zu 字节\n", stats.current_used);
    }
    fprintf(stderr, "[Lv-00] 内存统计 - 总分配: %zu, 总释放: %zu, 峰值: %zu\n",
            stats.total_allocated, stats.total_freed, stats.peak_used);

    g_init_count = 0;
    set_system_state(SYSTEM_STATE_UNINITIALIZED);
}

/**
 * @brief 查询系统是否已初始化
 * @return true 已初始化，false 未初始化
 */
bool lv00_is_initialized(void) {
    return is_system_initialized();
}

/**
 * @brief 获取系统信息字符串
 *
 * 将系统版本、运行状态、内存统计和性能计数器格式化为可读字符串。
 * 使用 LV00_SAFE_SNPRINTF 确保返回值的语义安全：返回值始终在
 * [0, size-1] 范围内（当 size > 0 时），便于调用者正确判断输出长度。
 * 若输出被截断，返回值为 size-1，表示实际输出超出缓冲区容量。
 *
 * @param info 输出缓冲区，用于接收格式化后的系统信息字符串
 * @param size 缓冲区大小（字节），必须大于 0
 * @return 实际写入的字符数（不含终止符），范围 [0, size-1]；
 *         若 info 为 NULL 或 size == 0 则返回 0
 */
int lv00_get_system_info(char *info, size_t size) {
    if (!info || size == 0)
        return 0;

    MemoryStats mem_stats;
    lv00_get_memory_stats(&mem_stats);

    PerformanceCounters perf;
    debug_get_counters(&perf);

    int written;
    LV00_SAFE_SNPRINTF(
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
        "  节点创建: %" PRIu64 "\n"
        "  约束创建: %" PRIu64 "\n"
        "  求解器调用: %" PRIu64 "\n"
        "  重写步数: %" PRIu64 "\n"
        "  合一检查: %" PRIu64 "\n",
        LV00_VERSION_STRING, is_system_initialized() ? "已初始化" : "未初始化", g_init_count,
        (double) mem_stats.current_used / (1024.0 * 1024.0), (double) mem_stats.peak_used / (1024.0 * 1024.0),
        (double) mem_stats.total_allocated / (1024.0 * 1024.0), (double) mem_stats.total_freed / (1024.0 * 1024.0),
        mem_stats.allocation_count, (uint64_t) perf.total_nodes_created,
        (uint64_t) perf.total_constraints_created, (uint64_t) perf.solver_call_count,
        (uint64_t) perf.rewrite_total_steps, (uint64_t) perf.unify_check_count);

    return written;
}

/**
 * @brief 系统健康检查
 *
 * 基于内存使用率、内存泄漏迹象和错误状态计算综合健康分数。
 * 满分 LV00_HEALTH_SCORE_MAX，各项扣分规则：
 * - 内存使用超过限制的 LV00_HEALTH_MEMORY_USAGE_RATIO 时扣分
 * - 检测到潜在内存泄漏迹象时扣分
 * - 存在最近的错误状态时扣分
 * 分数最终钳制在 [0, LV00_HEALTH_SCORE_MAX] 范围内。
 *
 * @return 健康分数，范围 [0, LV00_HEALTH_SCORE_MAX]，未初始化时返回 0
 */
int lv00_health_check(void) {
    if (!is_system_initialized()) {
        return 0;
    }

    int score = LV00_HEALTH_SCORE_MAX;

    /* 检查内存使用 */
    MemoryStats mem_stats;
    lv00_get_memory_stats(&mem_stats);

    size_t mem_limit = lv00_get_memory_limit();
    if (mem_limit > 0 && mem_stats.current_used > (size_t) ((double) mem_limit * LV00_HEALTH_MEMORY_USAGE_RATIO)) {
        score -= LV00_HEALTH_MEMORY_WARNING_PENALTY; /* 内存使用超过阈值 */
    }

    /* 检查内存泄漏 */
    if (mem_stats.current_used > (size_t) ((double) mem_stats.peak_used * LV00_HEALTH_MEMORY_LEAK_RATIO)) {
        score -= LV00_HEALTH_MEMORY_LEAK_PENALTY; /* 可能存在内存泄漏 */
    }

    /* 检查错误状态 */
    if (lv00_get_last_error_code() != LV00_OK) {
        score -= LV00_HEALTH_RECENT_ERROR_PENALTY;
    }

    /* 确保分数在合理范围 */
    if (score < 0)
        score = 0;
    if (score > LV00_HEALTH_SCORE_MAX)
        score = LV00_HEALTH_SCORE_MAX;

    return score;
}

/* ============================================================
 * 便捷API实现
 * ============================================================ */

/**
 * @brief 创建并初始化引擎（便捷函数）
 */
LV00Engine *lv00_engine_create(void) {
    if (!is_system_initialized()) {
        if (!lv00_init()) {
            return NULL;
        }
    }
    return engine_create();
}

/**
 * @brief 销毁引擎（便捷函数）
 */
void lv00_engine_destroy(LV00Engine *engine) {
    if (engine) {
        engine_destroy(engine);
    }
}

/**
 * @brief 快速创建点（便捷函数）
 */
int lv00_add_point(LV00Engine *engine, int64_t x_num, uint64_t x_den, int64_t y_num, uint64_t y_den) {
    if (!engine || !engine->main_graph) {
        lv00_set_error(LV00_ERROR_NULL_POINTER, "lv00_add_point: engine 或 main_graph 为 NULL");
        return -1;
    }
    /* 参数校验：分母不能为零 */
    if (x_den == 0 || y_den == 0) {
        lv00_set_error(LV00_ERROR_INVALID_PARAM, "lv00_add_point: 分母不能为零 (x_den=%" PRIu64 ", y_den=%" PRIu64 ")",
                       (uint64_t) x_den, (uint64_t) y_den);
        return -1;
    }

    SymbolicCoord *x = symbolic_coord_create_rational(x_num, x_den);
    SymbolicCoord *y = symbolic_coord_create_rational(y_num, y_den);

    if (!x || !y) {
        lv00_set_error(LV00_ERROR_OUT_OF_MEMORY, "lv00_add_point: 创建符号坐标失败");
        if (x)
            symbolic_coord_destroy(x);
        if (y)
            symbolic_coord_destroy(y);
        return -1;
    }

    /* 二维坐标维度常量 */
    const int coord_dim = 2;
    SymbolicCoord *coords[] = {x, y};
    AddNodeResult result = graph_add_point(engine->main_graph, coords, coord_dim);

    symbolic_coord_destroy(x);
    symbolic_coord_destroy(y);

    if (result != ADD_NODE_OK) {
        lv00_set_error(LV00_ERROR_NODE_CONFLICT, "lv00_add_point: 添加点到图失败 (result=%d)", (int) result);
        return -1;
    }

    return engine->main_graph->next_node_id - 1;
}

/**
 * @brief 快速创建线段（便捷函数）
 */
int lv00_add_line_segment(LV00Engine *engine, int point1_id, int point2_id) {
    if (!engine || !engine->main_graph) {
        lv00_set_error(LV00_ERROR_NULL_POINTER, "lv00_add_line_segment: engine 或 main_graph 为 NULL");
        return -1;
    }

    AddNodeResult result = graph_add_line_segment(engine->main_graph, point1_id, point2_id);
    if (result != ADD_NODE_OK) {
        lv00_set_error(LV00_ERROR_NODE_CONFLICT, "lv00_add_line_segment: 添加线段失败 (point1=%d, point2=%d, result=%d)",
                       point1_id, point2_id, (int) result);
        return -1;
    }

    return engine->main_graph->next_node_id - 1;
}

/**
 * @brief 快速添加约束（便捷函数）
 */
bool lv00_add_constraint_incidence(LV00Engine *engine, int point_id, int line_id) {
    if (!engine || !engine->main_graph) {
        lv00_set_error(LV00_ERROR_NULL_POINTER, "lv00_add_constraint_incidence: engine 或 main_graph 为 NULL");
        return false;
    }

    AddConstraintResult result = graph_add_incidence(engine->main_graph, point_id, line_id);
    if (result != ADD_CONSTRAINT_OK) {
        lv00_set_error(LV00_ERROR_CONSTRAINT_CONFLICT,
                       "lv00_add_constraint_incidence: 添加关联约束失败 (point=%d, line=%d, result=%d)",
                       point_id, line_id, (int) result);
        return false;
    }
    return true;
}

/**
 * @brief 执行归一化（便捷函数）
 */
NormalizationResult *lv00_normalize(LV00Engine *engine, bool scope_aware) {
    if (!engine || !engine->main_graph)
        return NULL;
    return graph_normalize(engine->main_graph, scope_aware);
}

/**
 * @brief 执行求解（便捷函数）
 */
EngineSolveResult lv00_solve(LV00Engine *engine) {
    if (!engine)
        return ENGINE_SOLVE_ERROR;
    return engine_solve(engine);
}

/**
 * @brief 获取配置值（便捷函数）
 */
int lv00_config_get_int(const char *key, int default_val) {
    if (!g_config)
        return default_val;
    return config_get_int(g_config, key, default_val);
}

/** @brief 获取布尔配置项 @param key 配置键名 @param default_val 默认值 @return 配置值 */
bool lv00_config_get_bool(const char *key, bool default_val) {
    if (!g_config)
        return default_val;
    return config_get_bool(g_config, key, default_val);
}

/** @brief 获取双精度浮点配置项 @param key 配置键名 @param default_val 默认值 @return 配置值 */
double lv00_config_get_double(const char *key, double default_val) {
    if (!g_config)
        return default_val;
    return config_get_double(g_config, key, default_val);
}

/** @brief 获取字符串配置项 @param key 配置键名 @param default_val 默认值 @return 配置值（可能为 NULL） */
const char *lv00_config_get_string(const char *key, const char *default_val) {
    if (!g_config)
        return default_val;
    return config_get_string(g_config, key, default_val);
}

/**
 * @brief 设置配置值（便捷函数）
 */
bool lv00_config_set_int(const char *key, int value) {
    if (!g_config)
        return false;
    return config_set_int(g_config, key, value);
}

/** @brief 设置布尔配置项 @param key 配置键名 @param value 配置值 @return true 成功 */
bool lv00_config_set_bool(const char *key, bool value) {
    if (!g_config)
        return false;
    return config_set_bool(g_config, key, value);
}

/** @brief 设置双精度浮点配置项 @param key 配置键名 @param value 配置值 @return true 成功 */
bool lv00_config_set_double(const char *key, double value) {
    if (!g_config)
        return false;
    return config_set_double(g_config, key, value);
}

/** @brief 设置字符串配置项 @param key 配置键名 @param value 配置值 @return true 成功 */
bool lv00_config_set_string(const char *key, const char *value) {
    if (!g_config)
        return false;
    return config_set_string(g_config, key, value);
}

/* ============================================================
 * 版本信息 API
 * ============================================================ */

bool lv00_get_version_info(LV00VersionInfo *info) {
    if (!info)
        return false;

    info->major = LV00_VERSION_MAJOR;
    info->minor = LV00_VERSION_MINOR;
    info->patch = LV00_VERSION_PATCH;
    info->version_string = lv00_get_version_string();

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

#if defined(__aarch64__)
    info->arch = "aarch64";
#elif defined(_WIN64) || defined(__x86_64__)
    info->arch = "x86_64";
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

bool lv00_check_version_compat(void) {
    /* 检查编译时主版本号是否为预期值
     * 预期主版本号：3（与 CMakeLists.txt 中的 PROJECT_VERSION_MAJOR 保持同步）
     * 当项目升级到 v4.x 时，此检查将帮助识别未重新编译的旧代码 */
    const int expected_major = 3;
    if (LV00_VERSION_MAJOR != expected_major) {
        return false;
    }
    return true;
}

/* ============================================================
 * 内存管理便捷API实现
 * ============================================================ */

/* ===== 向后兼容保留的别名（已弃用）=====
 * 本函数纯做 1:1 转发到 lv00_get_memory_stats()，仅为旧版调用方提供兼容。
 * 新代码请直接使用 lv00_get_memory_stats()。
 * 该函数计划在后续主版本中移除。
 */
LV00_DEPRECATED("use lv00_get_memory_stats instead")
/**
 * @brief 获取扩展内存统计信息（便捷封装）
 *
 * @param stats  输出参数，用于接收内存统计信息
 * @return true  成功获取并写入统计信息
 * @return false stats 为 NULL 指针，未执行任何操作
 * @note 内部委托 lv00_get_memory_stats() 完成实际统计
 */
bool lv00_get_memory_stats_ex(Lv00MemoryStats *stats) {
    if (!stats)
        return false;
    /* MemoryStats 和 Lv00MemoryStats 是不同的结构体，需要手动转换 */
    MemoryStats mem_stats;
    lv00_get_memory_stats(&mem_stats);
    /* 将 MemoryStats 的字段映射到 Lv00MemoryStats */
    stats->total_bytes = mem_stats.current_used;
    stats->peak_bytes = mem_stats.peak_used;
    /* types 和 type_count 保持为零（此函数不追踪类型细分） */
    stats->type_count = 0;
    memset(stats->types, 0, sizeof(stats->types));
    return true;
}

/* ===== 向后兼容保留的别名（已弃用）=====
 * 本函数纯做 1:1 转发到 lv00_set_memory_limit()，仅为旧版调用方提供兼容。
 * 新代码请直接使用 lv00_set_memory_limit()。
 * 该函数计划在后续主版本中移除。
 */
LV00_DEPRECATED("use lv00_set_memory_limit instead")
void lv00_set_memory_limit_ex(size_t limit_bytes) {
    lv00_set_memory_limit(limit_bytes);
}

/* ===== 向后兼容保留的别名（已弃用）=====
 * 本函数纯做 1:1 转发到 lv00_get_memory_limit()，仅为旧版调用方提供兼容。
 * 新代码请直接使用 lv00_get_memory_limit()。
 * 该函数计划在后续主版本中移除。
 */
LV00_DEPRECATED("use lv00_get_memory_limit instead")
size_t lv00_get_memory_limit_ex(void) {
    return lv00_get_memory_limit();
}

/* ============================================================
 * 调试和日志便捷API实现
 * ============================================================ */

/** @brief 设置日志级别 @param level 日志级别（0=关闭, 1=错误, 2=警告, 3=信息, 4=调试） */
void lv00_set_log_level(int level) {
    g_log_level = level;
    if (g_config) {
        config_set_int(g_config, "debug.log_level", level);
    }
}

/** @brief 获取当前日志级别 @return 日志级别 */
int lv00_get_log_level(void) {
    return g_log_level;
}

/** @brief 启用或禁用运行时断言 @param enabled true 启用，false 禁用 */
void lv00_set_assertions_enabled(bool enabled) {
    g_assertions_enabled = enabled;
    if (g_config) {
        config_set_bool(g_config, "debug.assertions_enabled", enabled);
    }
}

/** @brief 查询运行时断言是否启用 @return true 启用，false 禁用 */
bool lv00_are_assertions_enabled(void) {
    return g_assertions_enabled;
}

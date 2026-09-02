/**
 * @file lv_circuit_breaker.h
 * @brief 熔断器（Circuit Breaker）独立模块 —— 防止连续错误导致系统雪崩
 *
 * @details 从 lvContext God Object 中提取的独立熔断器子系统。
 *          提供三态状态机（CLOSED / HALF_OPEN / OPEN）和多维熔断保护：
 *          - 时间熔断：单次操作超时
 *          - 深度熔断：递归/推理深度超限
 *          - 次数熔断：推理步数超限
 *          - 错误熔断：连续错误次数超限
 *          - 内存熔断：内存使用超限
 *
 *          本模块是 lvContext 中 CircuitBreaker 的独立版本，
 *          不依赖 lvContext 或任何上层结构体。
 *
 * @author Lv-00 Project
 * @version 1.0.0
 * @date   2026-07-31
 */
#ifndef lv_LV_CIRCUIT_BREAKER_H
#define lv_LV_CIRCUIT_BREAKER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ============================================================
 * 默认配置（如果未在 config.h 中定义，使用此处硬编码默认值）
 * ============================================================ */

#ifndef lv_CB_DEFAULT_TIMEOUT_MS
#define lv_CB_DEFAULT_TIMEOUT_MS      30000   /**< 默认单次操作超时（毫秒） */
#endif

#ifndef lv_CB_DEFAULT_MAX_DEPTH
#define lv_CB_DEFAULT_MAX_DEPTH       100     /**< 默认最大递归/推理深度 */
#endif

#ifndef lv_CB_DEFAULT_MAX_STEPS
#define lv_CB_DEFAULT_MAX_STEPS       1000000 /**< 默认最大推理步数 */
#endif

#ifndef lv_CB_DEFAULT_MAX_ERRORS
#define lv_CB_DEFAULT_MAX_ERRORS      10      /**< 默认最大连续错误次数 */
#endif

#ifndef lv_CB_DEFAULT_COOLDOWN_MS
#define lv_CB_DEFAULT_COOLDOWN_MS     5000    /**< 默认熔断冷却时间（毫秒） */
#endif

/* ============================================================
 * 熔断器状态枚举
 * ============================================================ */

/**
 * @brief 熔断器状态
 */
typedef enum {
    lv_CB_CLOSED,     /**< 关闭态：正常工作 */
    lv_CB_HALF_OPEN,  /**< 半开态：冷却后试探性恢复 */
    lv_CB_OPEN        /**< 打开态：熔断，拒绝执行 */
} lvCircuitBreakerState;

/** @brief 向后兼容别名 */
#define CIRCUIT_BREAKER_CLOSED    lv_CB_CLOSED
#define CIRCUIT_BREAKER_HALF_OPEN lv_CB_HALF_OPEN
#define CIRCUIT_BREAKER_OPEN      lv_CB_OPEN

/* ============================================================
 * 熔断器结构体
 * ============================================================ */

/**
 * @brief 熔断器（Circuit Breaker）—— 多维熔断保护
 *
 * 监控关键资源指标。当任一指标超过阈值时，熔断器打开。
 * 熔断前可通过检查各维度预先判断是否即将超限。
 */
typedef struct lvCircuitBreaker {
    /** 熔断器当前状态 */
    lvCircuitBreakerState state;

    /* ── 时间熔断 ── */

    /** 单次操作超时时间（毫秒）。0 表示不限制。 */
    uint64_t timeout_ms;

    /** 上下文创建至今的总运行时间（毫秒）。超时后熔断。 */
    uint64_t total_timeout_ms;

    /** 不可取消区域计数（> 0 表示在关键路径中，超时不触发熔断） */
    int uncancellable_refcount;

    /* ── 深度熔断 ── */

    /** 当前递归/重写/推理的嵌套深度 */
    int current_depth;

    /** 最大允许深度 */
    int max_depth;

    /* ── 次数熔断 ── */

    /** 已执行的推理步骤总数 */
    int64_t total_steps;

    /** 最大推理步骤数（0 表示不限制） */
    int64_t max_steps;

    /* ── 错误熔断 ── */

    /** 连续错误计数（一次成功操作会清零） */
    int consecutive_errors;

    /** 连续错误上限（超过则熔断） */
    int max_consecutive_errors;

    /* ── 内存熔断 ── */

    /** 内存使用上限（字节，0 表示不限制） */
    size_t max_memory_bytes;

    /* ── 时间测量 ── */

    /** 创建/最近一次 reset 的时间戳（微秒） */
    uint64_t start_time_us;

    /** 当前操作的开始时间（微秒，用于单次超时判断） */
    uint64_t operation_start_us;

    /** 冷却时间（毫秒，熔断后必须等待的最小时间） */
    uint64_t cooldown_ms;

    /** 熔断发生的时间戳（微秒） */
    uint64_t tripped_at_us;

    /** 上次熔断的触发原因（可读字符串，动态分配） */
    char *trip_reason;

    /** 熔断次数（生命周期内累计） */
    int trip_count;
} lvCircuitBreaker;

/** @brief 向后兼容别名 */
typedef lvCircuitBreaker CircuitBreaker;
typedef lvCircuitBreakerState CircuitBreakerState;

/* ============================================================
 * 核心 API —— 直接操作 lvCircuitBreaker 结构体
 *
 * 这些函数不依赖 lvContext，是纯粹的熔断器操作。
 * ============================================================ */

/**
 * @brief 初始化熔断器为默认值
 *
 * 将所有字段设置为安全默认值：
 *   - 状态：CLOSED
 *   - 超时：lv_CB_DEFAULT_TIMEOUT_MS
 *   - 深度上限：lv_CB_DEFAULT_MAX_DEPTH
 *   - 步数上限：lv_CB_DEFAULT_MAX_STEPS
 *   - 错误上限：lv_CB_DEFAULT_MAX_ERRORS
 *   - 冷却时间：lv_CB_DEFAULT_COOLDOWN_MS
 *   - start_time_us 设为当前时间
 *
 * @param cb 熔断器指针（非 NULL）
 */
lv_PUBLIC_API void lv_circuit_breaker_init(lvCircuitBreaker *cb);

/**
 * @brief 检查熔断器是否已跳闸
 *
 * 综合检查各维度和状态机：
 *   1. 状态为 OPEN 且冷却未完成 → 已跳闸
 *   2. 总运行时间超限 → 已跳闸
 *   3. 深度超限 → 已跳闸
 *   4. 步数超限 → 已跳闸
 *
 * @param cb 熔断器指针（非 NULL）
 * @return true  熔断器已跳闸，拒绝执行
 *         false 可以继续操作
 */
lv_PUBLIC_API bool lv_circuit_breaker_is_tripped(const lvCircuitBreaker *cb);

/**
 * @brief 记录一次成功操作
 *
 * 重置连续错误计数。
 * 在半开态（HALF_OPEN）下，将状态恢复为 CLOSED。
 *
 * @param cb 熔断器指针（非 NULL）
 */
lv_PUBLIC_API void lv_circuit_breaker_record_success(lvCircuitBreaker *cb);

/**
 * @brief 记录一次错误操作
 *
 * 递增连续错误计数，如果超过上限则触发熔断（状态设为 OPEN）。
 * 在半开态（HALF_OPEN）下，直接触发熔断。
 *
 * @param cb 熔断器指针（非 NULL）
 * @return true  熔断器仍在 CLOSED 态（正常）
 *         false 熔断器已跳闸（错误次数超限或半开态失败）
 */
lv_PUBLIC_API bool lv_circuit_breaker_record_error(lvCircuitBreaker *cb);

/**
 * @brief 重置熔断器到初始状态
 *
 * 清除所有错误计数、跳闸原因和冷却计时。
 * 保留 trip_count（累计熔断次数不清零）。
 * start_time_us 重新设为当前时间。
 *
 * @param cb 熔断器指针（非 NULL）
 */
lv_PUBLIC_API void lv_circuit_breaker_reset(lvCircuitBreaker *cb);

/**
 * @brief 获取熔断器当前状态
 *
 * @param cb 熔断器指针（非 NULL）
 * @return 当前状态枚举值
 */
lvCircuitBreakerState lv_circuit_breaker_state(const lvCircuitBreaker *cb);

/**
 * @brief 检查熔断器并执行状态迁移，维度超限时自动跳闸
 *
 * 与 lv_circuit_breaker_is_tripped() 的区别：
 *  - 各维度超限（总运行时间/深度/步数/连续错误）时直接触发跳闸并记录原因
 *  - 冷却结束后自动将状态从 OPEN 迁移到 HALF_OPEN（允许一次试探）
 *  - 总运行时间检查尊重 uncancellable_refcount（不可取消区域不触发超时熔断）
 *
 * @param cb 熔断器指针（非 NULL）
 * @return true  可以继续操作
 *         false 熔断器打开（已跳闸），拒绝执行
 */
lv_PUBLIC_API bool lv_circuit_breaker_check_guarded(lvCircuitBreaker *cb);

/**
 * @brief 触发熔断器跳闸（状态置为 OPEN）
 *
 * 记录跳闸时间、累计熔断次数与跳闸原因（内部复制）。
 * 等价于内部跳闸动作，供外部在检测到超时/步数超限等场景直接调用。
 *
 * @param cb     熔断器指针（非 NULL）
 * @param reason 跳闸原因的可读描述（可为 NULL，此时使用 "未知原因"）
 */
lv_PUBLIC_API void lv_circuit_breaker_do_trip(lvCircuitBreaker *cb, const char *reason);

/**
 * @brief 获取熔断器状态的可读名称（枚举所在层 L2 的规范实现）
 *
 * 状态名表与 lvCircuitBreakerState 枚举同属 L2（枚举单一事实来源），
 * L4 的上下文级兼容包装 lv_circuit_breaker_state_name(lvContext*)
 * 委托本函数，避免跨层重复维护状态名。
 *
 * @param cb 熔断器实例（可为 NULL，返回"无熔断器"）
 * @return 状态名称字符串（静态存储，不需释放）
 */
lv_PUBLIC_API const char *lv_circuit_breaker_state_name_cb(const lvCircuitBreaker *cb);

#ifdef __cplusplus
}
#endif

#endif /* lv_CIRCUIT_BREAKER_H */

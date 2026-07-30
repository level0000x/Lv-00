/**
 * @file circuit_breaker.h
 * @brief 熔断器模块 —— 上下文级熔断器操作函数（向后兼容层）
 *
 * @details 本文件是上下文级熔断器操作的向后兼容层。
 *          CircuitBreaker 结构体已移至 lv/lv_circuit_breaker.h，
 *          此文件提供使用 lvContext* 的便利包装函数。
 *
 *          熔断器状态机：
 *
 *            CLOSED ── 错误次数超阈值 ──→ OPEN
 *              ↑                            │
 *              │                            │ 冷却时间过后
 *              │                            ↓
 *              └── 试探成功 ←── HALF_OPEN ←─┘
 *
 *          借鉴来源：
 *          - Netflix Hystrix 熔断器模式
 *          - Resilience4j CircuitBreaker
 *          - 分布式系统中的 bulkhead 模式
 *
 * @author Lv-00 Project
 * @version 2.0.0
 * @date   2026-07-31
 */
#ifndef lv_CIRCUIT_BREAKER_H
#define lv_CIRCUIT_BREAKER_H
#ifdef __cplusplus
extern "C" {
#endif
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* CircuitBreaker 结构体及独立 API 定义在此 */
#include "lv/lv_circuit_breaker.h"

/* lvContext 前向声明 */
struct lvContext;

/* ============================================================
 * 上下文级熔断器 API
 *
 * 这些函数接受 lvContext*，通过 ctx->circuit_breaker
 * 字段间接操作熔断器。与 lv_circuit_breaker.h 中的独立 API
 * 功能等价但设计为与现有调用代码兼容。
 * ============================================================ */

/**
 * @brief 检查熔断器状态，判断是否可以执行操作
 *
 * 逻辑：
 *   - CLOSED 态：正常执行，返回 true
 *   - OPEN 态：检查冷却时间
 *       - 冷却未完成：返回 false（拒绝执行）
 *       - 冷却已完成：自动进入 HALF_OPEN 态（仅允许一次试探）
 *   - HALF_OPEN 态：允许试探性调用，返回 true。
 *     但此次调用结果将决定是恢复 CLOSED 还是重新 OPEN。
 *
 * @param ctx 上下文（非 NULL，ctx->circuit_breaker 为熔断器实例）
 * @return true  可以继续操作
 *         false 熔断器打开，拒绝执行
 */
bool lv_circuit_breaker_check(struct lvContext *ctx);

/**
 * @brief 触发熔断器跳闸
 *
 * 将熔断器状态设置为 OPEN，并记录跳闸原因和时间。
 * 之后所有 lv_circuit_breaker_check() 调用将返回 false，
 * 直到冷却时间过去。
 *
 * @param ctx    上下文（非 NULL）
 * @param reason 跳闸原因的可读描述（内部复制，调用者可释放原字符串）
 */
void lv_circuit_breaker_trip(struct lvContext *ctx, const char *reason);

/**
 * @brief 记录一次失败操作
 *
 * 在 HALF_OPEN 态下的一次失败会立即将熔断器重新设为 OPEN。
 * 在 CLOSED 态下，递增连续错误计数，超过阈值则触发跳闸。
 *
 * @param ctx 上下文（非 NULL）
 * @return true  熔断器仍在 CLOSED 态（正常）
 *         false 熔断器已跳闸（错误次数超限）
 */
bool lv_circuit_breaker_record_failure(struct lvContext *ctx);

/**
 * @brief 获取熔断器当前状态的可读名称
 *
 * @param ctx 上下文（可为 NULL）
 * @return 状态的中文名称字符串（静态存储，无需释放）
 */
const char *lv_circuit_breaker_state_name(struct lvContext *ctx);

/**
 * @brief 获取熔断器的健康摘要
 *
 * 返回一个包含状态、错误计数、跳闸次数等信息的可读字符串。
 *
 * @param ctx      上下文（非 NULL）
 * @param buf      输出缓冲区
 * @param buf_size 缓冲区大小（建议至少 256 字节）
 * @return 实际写入的字符数（不含终止符）
 */
int lv_circuit_breaker_summary(struct lvContext *ctx, char *buf, size_t buf_size);

/**
 * @brief 获取从创建/重置以来的运行时间（微秒）
 *
 * @param cb 熔断器指针（非 NULL）
 * @return 微秒数
 */
uint64_t lv_circuit_breaker_uptime_us(const lvCircuitBreaker *cb);

/**
 * @brief 获取当前时间戳（微秒级）
 *
 * 平台无关的高精度时间获取函数。
 *
 * @return 单调递增的微秒时间戳
 */
uint64_t lv_circuit_breaker_now_us(void);

#ifdef __cplusplus
}
#endif
#endif /* lv_CIRCUIT_BREAKER_H */

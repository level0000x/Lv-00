/**
 * @file circuit_breaker.h
 * @brief 熔断器模块 —— 独立于上下文的熔断器操作函数
 *
 * @details 提供熔断器的核心操作：检查、跳闸、重置，以及时间驱动的
 *          自动恢复机制。CircuitBreaker 结构体在 context.h 中定义，
 *          本模块提供其操作实现。
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
 * @version 1.1.0
 * @date   2026-05-24
 */
#ifndef LV00_CIRCUIT_BREAKER_H
#define LV00_CIRCUIT_BREAKER_H
#ifdef __cplusplus
extern "C" {
#endif
#include <stdbool.h>
#include <stdint.h>
/* 前向声明 —— CircuitBreaker 结构体在 context.h 中定义 */
struct CircuitBreaker;
struct Lv00Context;
/* ============================================================
 * 核心熔断器 API
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
bool lv00_circuit_breaker_check(struct Lv00Context *ctx);
/**
 * @brief 触发熔断器跳闸
 *
 * 将熔断器状态设置为 OPEN，并记录跳闸原因和时间。
 * 之后所有 lv00_circuit_breaker_check() 调用将返回 false，
 * 直到冷却时间过去。
 *
 * @param ctx    上下文（非 NULL）
 * @param reason 跳闸原因的可读描述（内部复制，调用者可释放原字符串）
 */
void lv00_circuit_breaker_trip(struct Lv00Context *ctx, const char *reason);
/**
 * @brief 重置熔断器到 CLOSED 状态
 *
 * 清除所有错误计数、跳闸原因和冷却计时。
 * 仅在以下场景谨慎使用：
 *   - 上下文完全重置时（lv00_context_reset）
 *   - 确认问题已修复后的手动恢复
 *
 * @param ctx 上下文（非 NULL）
 */
void lv00_circuit_breaker_reset(struct Lv00Context *ctx);
/**
 * @brief 记录一次成功操作
 *
 * 在 HALF_OPEN 态下的一次成功调用会将熔断器恢复到 CLOSED 态。
 * 在 CLOSED 态下，重置连续错误计数。
 *
 * @param ctx 上下文（非 NULL）
 */
void lv00_circuit_breaker_record_success(struct Lv00Context *ctx);
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
bool lv00_circuit_breaker_record_failure(struct Lv00Context *ctx);
/**
 * @brief 获取熔断器当前状态的可读名称
 *
 * @param ctx 上下文（可为 NULL）
 * @return 状态的中文名称字符串（静态存储，无需释放）
 */
const char *lv00_circuit_breaker_state_name(struct Lv00Context *ctx);
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
int lv00_circuit_breaker_summary(struct Lv00Context *ctx, char *buf, size_t buf_size);
/**
 * @brief 获取从创建/重置以来的运行时间（微秒）
 *
 * @param cb 熔断器指针（非 NULL）
 * @return 微秒数
 */
uint64_t lv00_circuit_breaker_uptime_us(const struct CircuitBreaker *cb);
/**
 * @brief 获取当前时间戳（微秒级）
 *
 * 平台无关的高精度时间获取函数。
 *
 * @return 单调递增的微秒时间戳
 */
uint64_t lv00_circuit_breaker_now_us(void);
#ifdef __cplusplus
}
#endif
#endif /* LV00_CIRCUIT_BREAKER_H */

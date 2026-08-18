/**
 * @file lv_internal.h
 * @brief Lv-00 项目内部头文件 —— 内部工具宏与常量
 *
 * @details 提供项目内部使用的通用工具宏、常量和辅助函数声明，
 * 仅供 .c 源文件内部使用，不作为公共 API 暴露。
 *
 * 包含内容：
 * - lv_ARRAY_GROWTH_FACTOR：动态数组增长因子
 * - lv_SOLVER_SCALE_FACTOR：SAT 赋值到坐标的缩放因子
 * - lv_CHECK_NULL / lv_CHECK_NULL_VOID / lv_CHECK_ALLOC：空指针和分配检查宏
 * - lv_UNUSED：抑制未使用变量警告
 * - lv_ensure_capacity：动态数组容量确保函数
 * - lv_set_error_ctx：带上下文的错误设置函数
 * - lv_ERROR_*：错误码（通过包含 error_codes.h 获取）
 *
 * @note 本头文件会自动包含 error_codes.h 和 lv_utils.h，
 *       因此使用本头文件的 .c 文件无需再单独包含它们。
 *
 * @version 1.1.0
 */

#ifndef lv_INTERNAL_H
#define lv_INTERNAL_H

#include "lv/lv_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*
 * 错误码定义和错误处理宏（lv_CHECK_NULL, lv_CHECK_NULL_VOID,
 * lv_CHECK_ALLOC, lv_set_error_ctx, lv_ERROR_* 等）
 */
#include "config.h"
#include "error_codes.h"

/*
 * 工具函数（lv_malloc, lv_free, lv_realloc, lv_calloc,
 * lv_strdup, lv_ensure_capacity 等）
 */
#include "lv_utils.h"

/*
 * lv_UNUSED 宏（抑制未使用变量/参数的编译器警告）
 * 定义在 cross_platform.h 中
 */
#include "cross_platform.h"

#ifndef lv_PUBLIC_API
#define lv_PUBLIC_API
#endif

/* ================================================================
 * 内部常量
 * ================================================================ */

/**
 * @brief 动态数组增长因子
 *
 * 当数组需要扩容时，新容量 = 当前容量 * lv_ARRAY_GROWTH_FACTOR。
 * 值为 2（倍增策略），兼顾内存使用和性能。
 */
#ifndef lv_ARRAY_GROWTH_FACTOR
#define lv_ARRAY_GROWTH_FACTOR 2
#endif

/**
 * @brief SAT 赋值到坐标的缩放因子
 *
 * 将 SAT 求解器的布尔赋值映射为有理数坐标时的缩放系数。
 * 例如：赋值为真 -> +SCALE_FACTOR，赋值为假 -> -SCALE_FACTOR。
 */
#ifndef lv_SOLVER_SCALE_FACTOR
#define lv_SOLVER_SCALE_FACTOR 1000
#endif

/* ================================================================
 * 全局默认常量（仅本头内部使用）
 * 注：与 config.h 重复的编译期默认值（如 lv_DEFAULT_TIMEOUT_MS、
 *     lv_DEFAULT_MAX_ITERATIONS）以 config.h 为唯一来源，勿在此重定义。
 * ================================================================ */
#ifndef MAX_BLOCK_PORTS
#define MAX_BLOCK_PORTS 32
#endif
#ifndef lv_DEFAULT_MAX_DEPTH
#define lv_DEFAULT_MAX_DEPTH 64
#endif
/* 【版本权威源统一】fallback 值与 lv.h 的 lv_VERSION_MAJOR/MINOR/PATCH（1.1.0）保持一致，
 * 避免 include 顺序不同导致版本字符串漂移（原 3.5.0 为历史遗留硬编码） */
#ifndef lv_VERSION_STRING
#define lv_VERSION_STRING "1.1.0"
#endif

/* ================================================================
 * 日志级别与日志宏
 * ================================================================ */

#ifndef lv_LOG_LEVEL_OFF
#define lv_LOG_LEVEL_OFF 0
#define lv_LOG_LEVEL_ERROR 1
#define lv_LOG_LEVEL_WARNING 2
#define lv_LOG_LEVEL_INFO 3
#define lv_LOG_LEVEL_DEBUG 4
#endif

/**
 * @brief 日志消息分发函数（需在使用日志宏的 .c 文件中提供实现）
 *
 * @param level   日志级别（lv_LOG_LEVEL_*）
 * @param file    源文件名（由宏自动传入 __FILE__）
 * @param line    行号（由宏自动传入 __LINE__）
 * @param fmt     格式化字符串
 * @param ...     可变参数
 */
extern void lv_log_message(int level, const char *file, int line, const char *fmt, ...);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#ifndef lv_LOG_WARNING
#define lv_LOG_WARNING(fmt, ...) lv_log_message(lv_LOG_LEVEL_WARNING, __FILE__, __LINE__, (fmt), ##__VA_ARGS__)
#endif
#ifndef lv_LOG_WARN
#define lv_LOG_WARN(fmt, ...) lv_log_message(lv_LOG_LEVEL_WARNING, __FILE__, __LINE__, (fmt), ##__VA_ARGS__)
#endif
#ifndef lv_LOG_ERROR
#define lv_LOG_ERROR(fmt, ...) lv_log_message(lv_LOG_LEVEL_ERROR, __FILE__, __LINE__, (fmt), ##__VA_ARGS__)
#endif
#ifndef lv_LOG_INFO
#define lv_LOG_INFO(fmt, ...) lv_log_message(lv_LOG_LEVEL_INFO, __FILE__, __LINE__, (fmt), ##__VA_ARGS__)
#endif
#ifndef lv_LOG_DEBUG
#define lv_LOG_DEBUG(fmt, ...) lv_log_message(lv_LOG_LEVEL_DEBUG, __FILE__, __LINE__, (fmt), ##__VA_ARGS__)
#endif
#pragma GCC diagnostic pop

/* ================================================================
 * 安全字符串格式化宏
 * ================================================================ */

/**
 * @brief 安全 snprintf 包装宏
 *
 * 将 snprintf 的返回值规范化为实际写入的字符数（不含终止符），
 * 即使输出被截断也不会返回负值或超过缓冲区大小的值。
 *
 * @param retvar  接收规范化返回值的 int 变量名
 * @param buf     目标缓冲区
 * @param size    缓冲区大小
 * @param fmt     格式化字符串
 * @param ...     可变参数
 */
#define lv_SAFE_SNPRINTF(retvar, buf, size, fmt, ...)                                                    \
    do {                                                                                                 \
        int _sn_rc = snprintf((buf), (size), (fmt), ##__VA_ARGS__);                                      \
        (retvar) = ((_sn_rc) < 0) ? 0 : (((size_t) (_sn_rc) >= (size)) ? (int) ((size) - 1) : (_sn_rc)); \
    } while (0)

/**
 * @brief 安全增量写入宏（SVG 渲染等流式拼接场景）
 *
 * 在 _pos 处调用 snprintf 追加格式化内容并推进 _pos；_pos 始终钳位在
 * [0, _size-1]，且仅当 _pos 处于有效范围时才写入，防止越界写入。
 *
 * 与 lv_SAFE_SNPRINTF 的区别：本宏面向"逐步拼接"场景，调用后 _pos 被
 * 推进（_pos 必须为 int 左值，支持变量或 *int 指针解引用）；
 * lv_SAFE_SNPRINTF 仅将规范化写入字符数存入 retvar，不推进位置。
 * 本宏统一了 block_canvas.c / geometry_canvas.c 中曾各自定义的
 * SVG_SAFE_SNPRINTF / SVG_WRITE 两份本地副本（语义取两者之并集：
 * 含 _pos >= 0 防御检查），保证两处渲染输出完全一致。
 *
 * @param _pos   写入位置（int 左值，可为变量或 *int 指针）
 * @param _buf   目标缓冲区
 * @param _size  缓冲区大小
 * @param ...    格式化字符串及参数
 */
#define lv_SVG_WRITE(_buf, _pos, _size, ...)                                                      \
    do {                                                                                          \
        if ((_pos) >= 0 && (_pos) < (_size)) {                                                    \
            int _lv_w = snprintf((_buf) + (_pos), (size_t) ((_size) - (_pos)), __VA_ARGS__);      \
            if (_lv_w > 0) {                                                                      \
                (_pos) += _lv_w;                                                                  \
                if ((_pos) >= (_size))                                                            \
                    (_pos) = (_size) - 1;                                                         \
            }                                                                                     \
        }                                                                                         \
    } while (0)

/* ================================================================
 * 流上下文声明宏
 * ================================================================ */

/**
 * @brief 声明模块级线程局部流上下文变量
 *
 * 在每个使用流式输出的模块 .c 文件中使用，声明一个线程局部的
 * StreamContext 指针，供 stream_emit_simple 等函数使用。
 *
 * @param module  模块名称（用于构造变量名）
 */
#ifdef __GNUC__
#define lv_UNUSED_ATTR __attribute__((unused))
#else
#define lv_UNUSED_ATTR
#endif
#define lv_DECLARE_STREAM_CTX(module) lv_UNUSED_ATTR static lv_THREAD_LOCAL StreamContext *module##_stream_ctx = NULL

/* ================================================================
 * 错误返回宏
 *
 * lv_ERROR_RETURN / lv_RETURN_ERROR / lv_RETURN_ERROR_NULL /
 * lv_RETURN_ERROR_BOOL / lv_RETURN_ERROR_VAL 唯一定义于 error_codes.h
 * （本头文件顶部已 #include "error_codes.h"），此处不再重复定义。
 * ================================================================ */

/* ================================================================
 * 安全整数加法宏
 * ================================================================ */

/**
 * @brief 带溢出检测的整数加法
 *
 * 执行有符号 int 加法，若发生溢出则返回 overflow_val。
 *
 * @param a             第一个操作数
 * @param b             第二个操作数
 * @param overflow_val  溢出时的返回值
 */
#define lv_SAFE_ADD(a, b, overflow_val)                             \
    (((a) >= 0 && (b) > 0 && (a) > INT_MAX - (b))  ? (overflow_val) \
     : ((a) < 0 && (b) < 0 && (a) < INT_MIN - (b)) ? (overflow_val) \
                                                   : (int) ((a) + (b)))

/* ================================================================
 * 内存对齐工具
 * ================================================================ */

/**
 * @brief 将 size 向上对齐到 alignment（alignment 必须是 2 的幂）
 *
 * 内存池、竞技场分配器等底层分配模块共用此实现，
 * 避免各模块重复定义同义的静态内联函数。
 */
#ifndef lv_ALIGN_UP_DEFINED
#define lv_ALIGN_UP_DEFINED
static inline size_t align_up(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}
#endif

/* ================================================================
 * 规范错误表查询（内部）
 * ================================================================ */

/**
 * @brief 获取规范错误信息表（error_codes.c 中的 g_error_table）的条目数量
 *
 * 供同库内需要报告错误码映射规模的模块（如 status_codes、error_messages_cn）
 * 使用，避免维护重复的错误码 → 文本映射表。
 *
 * @return 错误信息表条目数量
 */
int lv_error_table_size(void);

/* ================================================================
 * 全局 ID 计数器 reset（测试进程内隔离）
 *
 * 各模块的 static 全局 ID 计数器在测试进程内会持续漂移，导致跨用例
 * 的 ID 断言不可重复。以下 reset 函数将对应计数器恢复为初始值，
 * 仅供测试在用例间调用；正常路径行为完全不变。
 * ================================================================ */

/** @brief 重置原生实现全局 ID 计数器（lv_impl_native.c，恢复初始值 2000000） */
void lv_native_reset_id_counter(void);

/** @brief 重置 .lv 加载器名称映射表（lv_loader.c，清空 s_loader_names） */
void lv_loader_reset(void);

/** @brief 重置代数模式全局 ID 计数器（algebra_mode.c，恢复 0） */
void lv_algebra_reset_id_counter(void);

/** @brief 重置溯源节点 ID 计数器（proof_trace_tree.c，恢复初始值 1） */
void lv_trace_reset_id_counter(void);

#ifdef __cplusplus
}
#endif

#endif /* lv_INTERNAL_H */

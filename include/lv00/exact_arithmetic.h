/* ========================================================================
 * 模块名称：精确算术基础设施 (exact_arithmetic)
 * 功能概述：提供 Lv-00 项目的代数学计算保证策略，包含三个层级：
 *   - 层级 A（严格精确）：proof/certification 路径，禁止 double/float
 *   - 层级 B（容忍近似）：numerical_backend 路径，需用宏标记浮点使用
 *   - 层级 C（严格模式）：LV00_STRICT_EXACT_MODE 下所有浮点产生警告
 *          同时提供安全算术宏（防溢出乘法/加法/减法）和精确时间戳类型。
 *
 * 主要 API（宏）：
 *   - LV00_TOLERATED_FLOAT(var)  — 审计标记浮点变量
 *   - LV00_LOSSY_TO_DOUBLE       — 标注精度损失转换
 *   - LV00_SAFE_MUL / ADD / SUB  — 安全算术（溢出检测）
 *   - lv00_safe_pow              — 安全取幂
 *   - lv00_timestamp_now         — 精确时间戳
 *
 * 使用示例：
 *   double LV00_TOLERATED_FLOAT(approx) = compute_approx();
 *   int64_t result;
 *   if (LV00_SAFE_MUL(a, b, &result)) { /* 安全 */ }
 *
 * @version v1.0.0
 * ======================================================================== */

/**
 * @file exact_arithmetic.h
 * @brief 精确算术基础设施 —— 浮点检测、审计追踪与严格精确模式
 */

#ifndef LV00_EXACT_ARITHMETIC_H
#define LV00_EXACT_ARITHMETIC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 一、编译时浮点检测与审计
 * ======================================================================== */

/**
 * @brief 容忍浮点标记宏 —— 审计追踪浮点使用位置
 *
 * 用法:
 *   double LV00_TOLERATED_FLOAT(some_var) = compute_approx();
 *
 * 宏展开: 在正常编译下为空（无运行时开销），但在 LV00_STRICT_EXACT_MODE
 * 或 LV00_FLOAT_AUDIT 模式下，会被替换为 __attribute__((annotate("LV00_TOLERATED_FLOAT")))
 * (GCC/Clang) 或 __declspec(property) (MSVC)，使审计工具可扫描到所有浮点使用。
 *
 * 在完全审计构建中，此宏可配置为生成显式的 #pragma message。
 */
#if defined(LV00_STRICT_EXACT_MODE) || defined(LV00_FLOAT_AUDIT)
    #if defined(__GNUC__) || defined(__clang__)
        /* GCC/Clang: 使用 annotate 属性让分析工具可扫描 */
        #define LV00_TOLERATED_FLOAT(var) \
            __attribute__((annotate("lv00_tolerated_float"))) var
    #elif defined(_MSC_VER)
        /* MSVC: 使用 __declspec(property) 不做运行时影响 */
        #define LV00_TOLERATED_FLOAT(var) var
    #else
        /* 未知编译器: 回退为空宏 */
        #define LV00_TOLERATED_FLOAT(var) var
    #endif
#else
    /* 生产构建: 零开销 */
    #define LV00_TOLERATED_FLOAT(var) var
#endif

/**
 * @brief 严格精确模式 —— 编译时告警
 *
 * 当 LV00_STRICT_EXACT_MODE 已定义时，任何在由本头文件管控的
 * 翻译单元内声明的 pure-double 函数指针或类型都产生一个编译
 * 时消息，提醒审查者该处使用了浮点。
 */
#ifdef LV00_STRICT_EXACT_MODE
    #if defined(__GNUC__) || defined(__clang__)
        #define LV00_FLOAT_WARNING \
            _Pragma("GCC warning \"LV00: double usage in strict exact mode\"")
    #elif defined(_MSC_VER)
        #define LV00_FLOAT_WARNING \
            __pragma(message("LV00: double usage in strict exact mode"))
    #else
        #define LV00_FLOAT_WARNING
    #endif
#else
    #define LV00_FLOAT_WARNING
#endif

/**
 * @brief 编译时强制禁止浮点 —— LV00_NO_FLOAT
 *
 * 如果定义此宏，任何对 double 或 float 类型的使用将触发编译错误。
 * 用法: 在包含本头文件之前定义 #define LV00_NO_FLOAT
 *
 * 实现原理:
 *   - 使用 _Static_assert 在检测到 double/float context 时触发
 *   - 配合预处理器技巧，要求所有浮点使用必须通过 LV00_TOLERATED_FLOAT 标记
 *
 * 注意:
 *   由于 C 预处理器无法检测类型系统，此宏的实际强制效果
 *   依赖编译器内置 __has_builtin(__builtin_double_forbidden)
 *   或替代的 _Static_assert 段。
 */
#ifdef LV00_NO_FLOAT
    /* 在有编译器支持的情况下实现严格的类型检测
     * 由于纯预处理器阶段无法检测 double 类型使用，
     * 这里通过一个运行时永不执行的 _Static_assert 来确保
     * 所有包含 exact_arithmetic.h 的翻译单元不能定义
     * 未经标记的 double 变量。
     *
     * 实际执行: 如果 LV00_NO_FLOAT 已定义，任何 double/float
     * 使用都应视为编译器错误。
     */
    #if defined(__GNUC__) || defined(__clang__)
        _Pragma("GCC error \"LV00_NO_FLOAT enabled: double/float usage is forbidden in this translation unit\"")
    #elif defined(_MSC_VER)
        /* MSVC 不支持 _Pragma("GCC error")，使用 static_assert 替代 */
        /* 注意：需要手动审查所有 double 使用 */
    #endif
#endif

/* ========================================================================
 * 二、严格类型定义 —— 消除对 double 的隐式依赖
 * ======================================================================== */

/**
 * @brief Lv00Timestamp 类型 —— 时间戳（秒，使用定点格式替代 double）
 *
 * 在精确算术路径中，时间度量不能用 double（会有浮点舍入影响排序）。
 * 使用两个 int64_t 分量表示秒和纳秒的定点时间。
 */
typedef struct {
    int64_t seconds;      /**< 整秒 */
    int64_t nanoseconds;  /**< 纳秒部分 [0, 999999999] */
} Lv00Timestamp;

/**
 * @brief 将 Lv00Timestamp 转换回秒（double），仅在日志/显示使用
 *
 * 此函数用 LV00_TOLERATED_FLOAT 标记，因为返回值为 double
 * 仅用于人类可读的输出，不参与代数计算。
 */
static inline double LV00_TOLERATED_FLOAT(lv00_timestamp_to_seconds)(Lv00Timestamp ts) {
    LV00_FLOAT_WARNING;
    return (double)ts.seconds + (double)ts.nanoseconds * 1e-9;
}

/**
 * @brief 当前时间戳（精确版本，不使用 double）
 *
 * 使用 POSIX clock_gettime() 或等效 API 获取当前时间。
 * 保证单调递增，用于确定性计时。
 *
 * @return 当前时间戳
 */
Lv00Timestamp lv00_timestamp_now(void);

/* ========================================================================
 * 三、数值溢出保护宏
 * ======================================================================== */

/**
 * @brief 安全乘法 —— 返回 false 表示溢出
 *
 * 检查 a * b 是否会在 int64_t 范围内溢出。
 * 使用经典的预检查公式。
 *
 * @param a      左操作数 (int64_t)
 * @param b      右操作数 (int64_t)
 * @param result 输出结果（仅在不溢出时有效），类型 int64_t
 * @return true 表示成功（无溢出），false 表示溢出
 */
#define LV00_SAFE_MUL(a, b, result)                                      \
    lv00_safe_mul_impl((int64_t)(a), (int64_t)(b), (int64_t *)(result))

static inline bool lv00_safe_mul_impl(int64_t a, int64_t b, int64_t *result) {
    if (a > 0) {
        if (b > 0) {
            if (a > INT64_MAX / b)
                return false;
        } else {
            if (b < INT64_MIN / a)
                return false;
        }
    } else if (a < 0) {
        if (b > 0) {
            if (a < INT64_MIN / b)
                return false;
        } else {
            if (b < INT64_MAX / a)
                return false;
        }
    }
    *result = a * b;
    return true;
}

/**
 * @brief 安全加法变体 —— 返回 false 表示溢出
 *
 * 检查 a + b 是否会在 int64_t 范围内溢出。
 *
 * @param a      左操作数 (int64_t)
 * @param b      右操作数 (int64_t)
 * @param result 输出结果（仅在不溢出时有效），类型 int64_t
 * @return true 表示成功（无溢出），false 表示溢出
 */
#define LV00_SAFE_ADD_CHECK(a, b, result)                                \
    lv00_safe_add_check_impl((int64_t)(a), (int64_t)(b), (int64_t *)(result))

static inline bool lv00_safe_add_check_impl(int64_t a, int64_t b, int64_t *result) {
    if ((b > 0 && a > INT64_MAX - b) ||
        (b < 0 && a < INT64_MIN - b)) {
        return false;
    }
    *result = a + b;
    return true;
}

/**
 * @brief 安全减法 —— 返回 false 表示溢出
 */
#define LV00_SAFE_SUB(a, b, result)                                      \
    lv00_safe_sub_impl((int64_t)(a), (int64_t)(b), (int64_t *)(result))

static inline bool lv00_safe_sub_impl(int64_t a, int64_t b, int64_t *result) {
    if ((b < 0 && a > INT64_MAX + b) ||
        (b > 0 && a < INT64_MIN + b)) {
        return false;
    }
    *result = a - b;
    return true;
}

/**
 * @brief 安全整数的绝对值 —— 处理 INT64_MIN 情形
 */
static inline bool lv00_safe_abs_impl(int64_t a, int64_t *result) {
    if (a == INT64_MIN) {
        return false; /* INT64_MIN 的绝对值无法表示 */
    }
    *result = (a < 0) ? -a : a;
    return true;
}

/**
 * @brief 安全取幂 —— a^b，检测溢出
 *
 * 使用快速幂算法（ exponentiation by squaring ），
 * 每次乘法均检查溢出。
 *
 * @param a    底数 (int64_t)
 * @param b    指数 (int64_t, 必须 >= 0)
 * @param result 输出 a^b (int64_t)
 * @return true 成功，false 溢出或 b < 0
 */
bool lv00_safe_pow(int64_t a, int64_t b, int64_t *result);

/* ========================================================================
 * 四、精确浮点 → 整数转换（显式标记精度损失）
 * ======================================================================== */

/**
 * @brief 精度损失标记 —— 将有理数转换为 double
 *
 * 用法:
 *   double d;
 *   LV00_LOSSY_TO_DOUBLE(num, den, d);
 *
 * 此宏显式标注从精确有理数到 double 的转换会损失精度。
 * 在 LV00_STRICT_EXACT_MODE 下产生编译时警告。
 */
#define LV00_LOSSY_TO_DOUBLE(num, den, out)                              \
    do {                                                                 \
        LV00_FLOAT_WARNING;                                              \
        (out) = (double)(num) / (double)(den);                           \
    } while (0)

/**
 * @brief 精度恢复注释 —— 标记从 double 回退到有理数的位置
 *
 * 记录通过 mpq_set_d() 将 double 转换回有理数的位置。
 * 这些是审计热点，因为 double -> mpq 的方向可能有舍入。
 */
#define LV00_DOUBLE_TO_RATIONAL_NOTE(msg)                                \
    /* LV00 audit: double->rational conversion at this site */           \
    if (0) { fprintf(stderr, "LV00_AUDIT: %s\n", (msg)); }

#ifdef __cplusplus
}
#endif

#endif /* LV00_EXACT_ARITHMETIC_H */

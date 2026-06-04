/**
 * @file cross_platform.h
 * @brief Lv-00 跨平台类型系统 —— 可移植的固定宽度类型、平台/编译器/架构检测
 *
 * @details 此文件集中定义所有跨平台相关的类型别名、检测宏和可移植工具。
 *          所有 Lv-00 模块应通过此头文件获取平台信息，禁止自行编写
 *          #ifdef _WIN32 等分散的平台检测代码。
 *
 * 设计原则:
 *   - 单一事实来源：所有平台差异在此文件中一次性处理
 *   - 可移植优先：在保证可移植性的前提下，允许平台特定优化
 *   - 零运行时开销：所有检测在编译期完成，不产生运行时分支
 *
 * 使用方式:
 *   #include "lv00/cross_platform.h"
 *   lv00_i32 value = 42;           // 固定宽度有符号 32 位整数
 *   #if LV00_PLATFORM_WINDOWS      // 编译期平台分支
 *       // Windows 特定代码
 *   #endif
 *
 * @version 3.3.0
 * @date   2026-05-24
 */

#ifndef LV00_CROSS_PLATFORM_H
#define LV00_CROSS_PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>  /* size_t, NULL */
#include <stdint.h>  /* int32_t, uint32_t, int64_t, uint64_t */
#include <limits.h>  /* CHAR_BIT, UINT_MAX, ULONG_MAX */

/* ====================================================================
 * 1. 固定宽度整数类型别名（Fixed-width Integer Types）
 *
 * 使用 stdint.h 作为底层实现，通过 typedef 统一命名为 lv00_* 前缀，
 * 确保在整个代码库中类型名称一致、不受平台差异影响。
 *
 * 约定:
 *   - lv00_i32: 恰好 32 位有符号整数（等于 int32_t）
 *   - lv00_u32: 恰好 32 位无符号整数（等于 uint32_t）
 *   - lv00_i64: 恰好 64 位有符号整数（等于 int64_t）
 *   - lv00_u64: 恰好 64 位无符号整数（等于 uint64_t）
 *   - lv00_iptr: 指针宽度有符号整数（用于指针运算）
 *   - lv00_uptr: 指针宽度无符号整数（用于指针运算/哈希）
 *   - lv00_byte: 恰好 8 位无符号整数（字节操作）
 *   - lv00_bool: 布尔类型（依据 C 标准版本自动选择）
 * ==================================================================== */

/** @brief 固定宽度 32 位有符号整数 */
typedef int32_t  lv00_i32;

/** @brief 固定宽度 32 位无符号整数 */
typedef uint32_t lv00_u32;

/** @brief 固定宽度 64 位有符号整数 */
typedef int64_t  lv00_i64;

/** @brief 固定宽度 64 位无符号整数 */
typedef uint64_t lv00_u64;

/** @brief 指针宽度有符号整数（intptr_t 别名） */
typedef intptr_t  lv00_iptr;

/** @brief 指针宽度无符号整数（uintptr_t 别名，常用于哈希） */
typedef uintptr_t lv00_uptr;

/** @brief 恰好 8 位无符号整数（字节操作） */
typedef uint8_t   lv00_byte;

/** @brief 布尔类型：C99+ 使用 _Bool，否则回退到 int */
#if __STDC_VERSION__ >= 199901L || defined(__cplusplus)
#include <stdbool.h>
typedef bool lv00_bool;
#else
typedef int  lv00_bool;
#define true  1
#define false 0
#endif

/* ====================================================================
 * 2. 平台检测宏（Platform Detection）
 *
 * 检测目标操作系统，设置对应宏为 1。
 * 优先级：Windows > Linux > macOS > 其他 Unix
 *
 * 命名:
 *   - LV00_PLATFORM_WINDOWS: 目标为 Windows（含 MinGW/Cygwin）
 *   - LV00_PLATFORM_LINUX:   目标为 Linux 内核
 *   - LV00_PLATFORM_MACOS:   目标为 macOS / Darwin
 *   - LV00_PLATFORM_UNIX:    目标为任意 Unix-like 系统（POSIX）
 *   - LV00_PLATFORM_NAME:    目标平台的人类可读名称字符串
 * ==================================================================== */

/* ── Windows 检测 ──
 * _WIN32: 所有 Windows 平台（32 位和 64 位），包括 MinGW
 * _WIN64: 仅 64 位 Windows */
#if defined(_WIN32) || defined(_WIN64)
  #define LV00_PLATFORM_WINDOWS 1
#else
  #define LV00_PLATFORM_WINDOWS 0
#endif

/* ── Linux 检测 ──
 * __linux__: Linux 内核（GCC/Clang 定义）
 * 注意: Android 也定义 __linux__，通过 __ANDROID__ 排除 */
#if defined(__linux__) && !defined(__ANDROID__)
  #define LV00_PLATFORM_LINUX 1
#else
  #define LV00_PLATFORM_LINUX 0
#endif

/* ── macOS 检测 ──
 * __APPLE__ && __MACH__: Apple 平台（macOS, iOS 等）
 * TARGET_OS_MAC: macOS 桌面系统（排除 iOS/tvOS/watchOS） */
#if defined(__APPLE__) && defined(__MACH__)
  #include <TargetConditionals.h>
  #if TARGET_OS_MAC && !TARGET_OS_IPHONE
    #define LV00_PLATFORM_MACOS 1
  #else
    #define LV00_PLATFORM_MACOS 0
  #endif
#else
  #define LV00_PLATFORM_MACOS 0
#endif

/* ── 通用 Unix 检测 ──
 * 涵盖所有 POSIX 兼容系统: Linux, macOS, BSD, Solaris 等 */
#if LV00_PLATFORM_LINUX || LV00_PLATFORM_MACOS || \
    defined(__unix__) || defined(__unix) || \
    (defined(__APPLE__) && defined(__MACH__))
  #define LV00_PLATFORM_UNIX 1
#elif LV00_PLATFORM_WINDOWS
  #define LV00_PLATFORM_UNIX 0
#else
  #define LV00_PLATFORM_UNIX 0
#endif

/* ── 平台名称字符串 ── */
#if LV00_PLATFORM_WINDOWS
  #define LV00_PLATFORM_NAME "Windows"
#elif LV00_PLATFORM_LINUX
  #define LV00_PLATFORM_NAME "Linux"
#elif LV00_PLATFORM_MACOS
  #define LV00_PLATFORM_NAME "macOS"
#else
  #define LV00_PLATFORM_NAME "Unknown"
#endif

/* ====================================================================
 * 3. 编译器检测宏（Compiler Detection）
 *
 * 检测编译器类型和版本，允许针对不同编译器启用特定优化或变通方案。
 *
 * 命名:
 *   - LV00_CC_GCC:   GNU Compiler Collection
 *   - LV00_CC_CLANG: LLVM Clang / Apple Clang
 *   - LV00_CC_MSVC:  Microsoft Visual C++
 *   - LV00_CC_NAME:  编译器的人类可读名称字符串
 * ==================================================================== */

/* ── GCC ── */
#if defined(__GNUC__) && !defined(__clang__) && !defined(__INTEL_COMPILER)
  #define LV00_CC_GCC   1
  #define LV00_CC_CLANG 0
  #define LV00_CC_MSVC  0
  #define LV00_CC_NAME  "GCC"
  #define LV00_CC_VERSION_MAJOR __GNUC__
  #define LV00_CC_VERSION_MINOR __GNUC_MINOR__
  #define LV00_CC_VERSION_PATCH __GNUC_PATCHLEVEL__

/* ── Clang ──
 * Clang 也定义 __GNUC__ 宏，因此必须在 GCC 之后检测 */
#elif defined(__clang__)
  #define LV00_CC_GCC   0
  #define LV00_CC_CLANG 1
  #define LV00_CC_MSVC  0
  #define LV00_CC_NAME  "Clang"
  #define LV00_CC_VERSION_MAJOR __clang_major__
  #define LV00_CC_VERSION_MINOR __clang_minor__
  #define LV00_CC_VERSION_PATCH __clang_patchlevel__

/* ── MSVC ── */
#elif defined(_MSC_VER)
  #define LV00_CC_GCC   0
  #define LV00_CC_CLANG 0
  #define LV00_CC_MSVC  1
  #define LV00_CC_NAME  "MSVC"

  /* _MSC_VER 编码: MMmmpp (主版本+次版本+补丁) */
  #define LV00_CC_VERSION_MAJOR (_MSC_VER / 100)
  #define LV00_CC_VERSION_MINOR ((_MSC_VER % 100) / 10)
  #define LV00_CC_VERSION_PATCH (_MSC_VER % 10)

/* ── 未知编译器 ── */
#else
  #define LV00_CC_GCC   0
  #define LV00_CC_CLANG 0
  #define LV00_CC_MSVC  0
  #define LV00_CC_NAME  "Unknown"
  #define LV00_CC_VERSION_MAJOR 0
  #define LV00_CC_VERSION_MINOR 0
  #define LV00_CC_VERSION_PATCH 0
#endif

/* ====================================================================
 * 4. 架构检测宏（Architecture Detection）
 *
 * 检测目标 CPU 架构的字宽。
 * 32 位和 64 位是互斥的（一个为 1，另一个必须为 0）。
 *
 * 命名:
 *   - LV00_ARCH_32BIT: 32 位架构（ILP32）
 *   - LV00_ARCH_64BIT: 64 位架构（LP64 / LLP64）
 *   - LV00_ARCH_NAME:  架构的人类可读名称字符串
 * ==================================================================== */

/* 64 位检测:
 *   - __x86_64__ / __amd64__: x86-64 (GCC/Clang/MSVC)
 *   - __aarch64__ / __arm64__: ARM64
 *   - __ia64__: Intel Itanium
 *   - __ppc64__ / __powerpc64__: PowerPC 64
 *   - _WIN64: 64 位 Windows (MSVC)
 *   - UINTPTR_MAX: 标准方法（最后手段） */
#if defined(__x86_64__) || defined(__amd64__) || defined(_M_X64) || \
    defined(__aarch64__) || defined(__arm64__) || \
    defined(__ia64__) || defined(__ppc64__) || defined(__powerpc64__) || \
    defined(_WIN64)
  #define LV00_ARCH_64BIT 1
  #define LV00_ARCH_32BIT 0

#elif defined(__i386__) || defined(_M_IX86) || \
      defined(__arm__) || defined(_M_ARM) || \
      defined(__mips__) || defined(__ppc__) || defined(__powerpc__)
  #define LV00_ARCH_64BIT 0
  #define LV00_ARCH_32BIT 1

/* 回退: 根据指针大小推断 */
#elif UINTPTR_MAX == 0xFFFFFFFFFFFFFFFFULL
  #define LV00_ARCH_64BIT 1
  #define LV00_ARCH_32BIT 0
#elif UINTPTR_MAX == 0xFFFFFFFFUL
  #define LV00_ARCH_64BIT 0
  #define LV00_ARCH_32BIT 1
#else
  #error "Lv-00: Unable to determine architecture bit width"
#endif

/* ── 架构名称 ── */
#if LV00_ARCH_64BIT
  #define LV00_ARCH_NAME "64-bit"
#else
  #define LV00_ARCH_NAME "32-bit"
#endif

/* ====================================================================
 * 5. 栈大小感知宏（Stack Size Awareness）
 *
 * 不同平台的默认栈大小差异很大:
 *   - Windows (MSVC): 默认 1 MB 线程栈
 *   - Linux (pthread): 默认 8 MB 线程栈
 *   - macOS (pthread): 默认 512 KB 辅助线程栈
 *
 * 这些宏允许代码根据平台调整栈分配策略，避免栈溢出。
 *
 * 使用示例:
 *   // 大数组改为堆分配以避免 Windows 上栈溢出
 *   #if LV00_STACK_SIZE_KB < 2048
 *       char *buf = lv00_malloc(LV00_CONFIG_STREAM_JSON_BUFFER_SIZE);
 *   #else
 *       char buf[LV00_CONFIG_STREAM_JSON_BUFFER_SIZE];
 *   #endif
 * ==================================================================== */

/* ── 估计的默认线程栈大小（KB）──
 * 注意: 主线程栈可能不同；这些值是典型平台的 pthread/CreateThread 默认值 */
#if LV00_PLATFORM_WINDOWS
  #define LV00_STACK_SIZE_KB           1024   /* MSVC 默认 1 MB */
  #define LV00_STACK_SIZE_BYTES        1048576
#elif LV00_PLATFORM_MACOS
  #define LV00_STACK_SIZE_KB           512    /* macOS 辅助线程 512 KB */
  #define LV00_STACK_SIZE_BYTES        524288
#else
  /* Linux 及其他 Unix: 通常 8 MB */
  #define LV00_STACK_SIZE_KB           8192
  #define LV00_STACK_SIZE_BYTES        8388608
#endif

/* ── 安全栈分配阈值 ──
 * 如果局部变量总计超过此值，应使用堆分配
 * 保守策略: 不超过栈大小的 1/8 */
#define LV00_SAFE_STACK_ALLOC_BYTES   (LV00_STACK_SIZE_BYTES / 8)

/* ====================================================================
 * 6. 字节序检测（Endianness Detection）
 *
 * 检测目标平台的字节序。大多数现代平台是小端序。
 *
 * 命名:
 *   - LV00_ENDIAN_LITTLE: 小端序（LSB 在前）
 *   - LV00_ENDIAN_BIG:    大端序（MSB 在前）
 * ==================================================================== */

/* ── 编译期字节序检测 ──
 * 策略: 优先使用编译器内置宏，回退到标准检测 */
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && defined(__ORDER_BIG_ENDIAN__)
  /* GCC / Clang 内置 */
  #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    #define LV00_ENDIAN_LITTLE 1
    #define LV00_ENDIAN_BIG    0
  #elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    #define LV00_ENDIAN_LITTLE 0
    #define LV00_ENDIAN_BIG    1
  #else
    #error "Lv-00: Unknown byte order (PDP-endian not supported)"
  #endif

#elif defined(_WIN32)
  /* Windows: 总是小端序 (x86/x64/ARM64) */
  #define LV00_ENDIAN_LITTLE 1
  #define LV00_ENDIAN_BIG    0

#elif defined(__LITTLE_ENDIAN__)
  #define LV00_ENDIAN_LITTLE 1
  #define LV00_ENDIAN_BIG    0

#elif defined(__BIG_ENDIAN__)
  #define LV00_ENDIAN_LITTLE 0
  #define LV00_ENDIAN_BIG    1

#else
  /* 回退: 编译时无法确定，运行时检测 */
  #define LV00_ENDIAN_UNKNOWN 1
  #define LV00_ENDIAN_LITTLE  0
  #define LV00_ENDIAN_BIG     0

  /** @brief 运行时字节序检测函数
   *  @return 1 = 小端序, 0 = 大端序 */
  static inline int lv00_is_little_endian(void) {
      const uint16_t val = 0x0001;
      return (int)(*(const uint8_t *)&val);
  }
#endif

/* ── 字节交换宏（可移植）── */
#if defined(__GNUC__) || defined(__clang__)
  /** @brief 16 位字节交换（GCC/Clang 内置） */
  #define LV00_BSWAP16(x) __builtin_bswap16(x)
  /** @brief 32 位字节交换（GCC/Clang 内置） */
  #define LV00_BSWAP32(x) __builtin_bswap32(x)
  /** @brief 64 位字节交换（GCC/Clang 内置） */
  #define LV00_BSWAP64(x) __builtin_bswap64(x)
#elif defined(_MSC_VER)
  #include <stdlib.h>
  #define LV00_BSWAP16(x) _byteswap_ushort(x)
  #define LV00_BSWAP32(x) _byteswap_ulong(x)
  #define LV00_BSWAP64(x) _byteswap_uint64(x)
#else
  /* 通用实现（较慢，但保证正确） */
  #define LV00_BSWAP16(x) ((uint16_t)(((x) >> 8) | ((x) << 8)))
  #define LV00_BSWAP32(x) ((uint32_t)( \
      (((x) & 0xFF000000u) >> 24) | \
      (((x) & 0x00FF0000u) >> 8)  | \
      (((x) & 0x0000FF00u) << 8)  | \
      (((x) & 0x000000FFu) << 24)))
  #define LV00_BSWAP64(x) ((uint64_t)( \
      (((x) & 0xFF00000000000000ull) >> 56) | \
      (((x) & 0x00FF000000000000ull) >> 40) | \
      (((x) & 0x0000FF0000000000ull) >> 24) | \
      (((x) & 0x000000FF00000000ull) >> 8)  | \
      (((x) & 0x00000000FF000000ull) << 8)  | \
      (((x) & 0x0000000000FF0000ull) << 24) | \
      (((x) & 0x000000000000FF00ull) << 40) | \
      (((x) & 0x00000000000000FFull) << 56)))
#endif

/* ====================================================================
 * 7. 可移植对齐宏（Portable Alignment Macros）
 *
 * 提供跨编译器的对齐声明和计算宏。
 * ==================================================================== */

/* ── 对齐声明 ──
 * LV00_ALIGNAS(n): 将变量/类型对齐到 n 字节边界
 * 示例: LV00_ALIGNAS(16) char buf[64]; */
#if __STDC_VERSION__ >= 201112L
  /* C11: 使用标准 _Alignas */
  #define LV00_ALIGNAS(n) _Alignas(n)
#elif defined(__GNUC__) || defined(__clang__)
  #define LV00_ALIGNAS(n) __attribute__((aligned(n)))
#elif defined(_MSC_VER)
  #define LV00_ALIGNAS(n) __declspec(align(n))
#else
  #define LV00_ALIGNAS(n)
  #warning "Lv-00: LV00_ALIGNAS not supported on this compiler"
#endif

/* ── 对齐计算 ──
 * LV00_ALIGN_UP(x, n): 将 x 向上对齐到 n 的倍数
 * LV00_ALIGN_DOWN(x, n): 将 x 向下对齐到 n 的倍数
 * 要求: n 必须是 2 的幂 */
#define LV00_ALIGN_UP(x, n)   (((x) + ((n) - 1)) & ~((n) - 1))
#define LV00_ALIGN_DOWN(x, n) ((x) & ~((n) - 1))

/* ── 缓存行大小估计 ──
 * 大多数 x86/ARM 架构: 64 字节
 * Apple M 系列: 128 字节 */
#if defined(__APPLE__) && defined(__aarch64__)
  #define LV00_CACHE_LINE_SIZE 128
#else
  #define LV00_CACHE_LINE_SIZE 64
#endif

/* ── 标准对齐常量 ── */
#define LV00_ALIGNOF_POINTER   (sizeof(void *))        /**< 指针对齐 */
#define LV00_ALIGNOF_MAX       (sizeof(void *) * 2)    /**< 最大标量对齐（通常 8/16） */
#define LV00_ALIGNOF_SIMD      16                       /**< SIMD 向量对齐（SSE/NEON） */
#define LV00_ALIGNOF_PAGE      4096                     /**< 内存页面对齐 */

/* ====================================================================
 * 8. 属性与注解宏（Attributes and Annotations）
 *
 * 跨编译器统一的函数/变量属性。
 * ==================================================================== */

/* ── 函数内联提示 ── */
#if __STDC_VERSION__ >= 199901L || defined(__cplusplus)
  #define LV00_INLINE inline
#else
  #define LV00_INLINE /* 不支持 */
#endif

/* 强制内联（编译器自行决定是否采纳） */
#if defined(__GNUC__) || defined(__clang__)
  #define LV00_FORCE_INLINE __attribute__((always_inline)) inline
#elif defined(_MSC_VER)
  #define LV00_FORCE_INLINE __forceinline
#else
  #define LV00_FORCE_INLINE LV00_INLINE
#endif

/* ── 函数不可返回标记 ── */
#if defined(__GNUC__) || defined(__clang__)
  #define LV00_NORETURN __attribute__((noreturn))
#elif defined(_MSC_VER)
  #define LV00_NORETURN __declspec(noreturn)
#else
  #define LV00_NORETURN
#endif

/* ── 未使用变量/参数抑制警告 ── */
#if defined(__GNUC__) || defined(__clang__)
  #define LV00_UNUSED(x) (void)(x)
#else
  #define LV00_UNUSED(x) (void)(x)
#endif

/* ── const 返回值提示（帮助编译器优化） ── */
#if defined(__GNUC__) || defined(__clang__)
  #define LV00_CONST_FUNC __attribute__((const))
  #define LV00_PURE_FUNC  __attribute__((pure))
#else
  #define LV00_CONST_FUNC
  #define LV00_PURE_FUNC
#endif

/* ── 分支预测提示 ── */
#if defined(__GNUC__) || defined(__clang__)
  #define LV00_LIKELY(x)   __builtin_expect(!!(x), 1)
  #define LV00_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
  #define LV00_LIKELY(x)   (x)
  #define LV00_UNLIKELY(x) (x)
#endif

/* ── 格式字符串检查（printf/scanf 风格） ── */
#if defined(__GNUC__) || defined(__clang__)
  #define LV00_FORMAT_PRINTF(fmt_idx, args_idx) \
      __attribute__((format(printf, fmt_idx, args_idx)))
  #define LV00_FORMAT_SCANF(fmt_idx, args_idx) \
      __attribute__((format(scanf, fmt_idx, args_idx)))
#else
  #define LV00_FORMAT_PRINTF(fmt_idx, args_idx)
  #define LV00_FORMAT_SCANF(fmt_idx, args_idx)
#endif

/* ====================================================================
 * 9. 导出/导入宏（用于共享库/DLL）
 *
 * 这些宏在 lv00.h 的 LV00_PUBLIC_API 中组合使用，
 * 此处提供底层构建块。
 * ==================================================================== */

/* ── 符号可见性（GCC/Clang）── */
#if defined(__GNUC__) || defined(__clang__)
  #if __GNUC__ >= 4 || defined(__clang__)
    #define LV00_EXPORT __attribute__((visibility("default")))
    #define LV00_HIDDEN __attribute__((visibility("hidden")))
  #else
    #define LV00_EXPORT
    #define LV00_HIDDEN
  #endif
#else
  #define LV00_EXPORT
  #define LV00_HIDDEN
#endif

/* ====================================================================
 * 10. 编译期静态断言（Compile-time Static Assertion）
 *
 * 跨 C 标准版本的统一静态断言。
 * C11 之前使用负数组大小技巧。
 * ==================================================================== */

#if __STDC_VERSION__ >= 201112L
  #define LV00_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#else
  /* 非 C11: 使用经典的数组大小技巧 */
  #define LV00_STATIC_ASSERT_CONCAT_IMPL(a, b) a##b
  #define LV00_STATIC_ASSERT_CONCAT(a, b) LV00_STATIC_ASSERT_CONCAT_IMPL(a, b)
  #define LV00_STATIC_ASSERT(cond, msg) \
      typedef char LV00_STATIC_ASSERT_CONCAT(lv00_static_assertion_, __LINE__) \
          [(cond) ? 1 : -1]
#endif

/* ── 编译期验证固定宽度类型 ── */
LV00_STATIC_ASSERT(sizeof(lv00_i32) == 4, "lv00_i32 must be exactly 4 bytes");
LV00_STATIC_ASSERT(sizeof(lv00_u32) == 4, "lv00_u32 must be exactly 4 bytes");
LV00_STATIC_ASSERT(sizeof(lv00_i64) == 8, "lv00_i64 must be exactly 8 bytes");
LV00_STATIC_ASSERT(sizeof(lv00_u64) == 8, "lv00_u64 must be exactly 8 bytes");
LV00_STATIC_ASSERT(sizeof(lv00_byte) == 1, "lv00_byte must be exactly 1 byte");

/* ====================================================================
 * 11. 平台信息查询函数（运行时 API）
 *
 * 提供编译期平台信息的运行时查询接口，方便日志和调试。
 * ==================================================================== */

/**
 * @brief 获取编译期检测到的平台名称
 * @return 平台名称字符串（如 "Windows", "Linux", "macOS"）
 */
static inline const char *lv00_platform_name(void) {
    return LV00_PLATFORM_NAME;
}

/**
 * @brief 获取编译期检测到的编译器名称
 * @return 编译器名称字符串（如 "GCC", "Clang", "MSVC"）
 */
static inline const char *lv00_compiler_name(void) {
    return LV00_CC_NAME;
}

/**
 * @brief 获取编译期检测到的架构名称
 * @return 架构名称字符串（如 "64-bit", "32-bit"）
 */
static inline const char *lv00_arch_name(void) {
    return LV00_ARCH_NAME;
}

/**
 * @brief 获取编译期检测到的字节序
 * @return "little-endian", "big-endian", 或 "unknown"
 */
static inline const char *lv00_endian_name(void) {
#if LV00_ENDIAN_LITTLE
    return "little-endian";
#elif LV00_ENDIAN_BIG
    return "big-endian";
#else
    /* 运行时检测 */
    return lv00_is_little_endian() ? "little-endian" : "big-endian";
#endif
}

/**
 * @brief 获取估计的默认线程栈大小（字节）
 * @return 栈大小（字节）
 */
static inline size_t lv00_stack_size(void) {
    return LV00_STACK_SIZE_BYTES;
}

/**
 * @brief 获取缓存行大小（字节）
 * @return 缓存行大小（字节）
 */
static inline int lv00_cache_line_size(void) {
    return LV00_CACHE_LINE_SIZE;
}

#ifdef __cplusplus
}
#endif

#endif /* LV00_CROSS_PLATFORM_H */

/**
 * @file lv_platform.h
 * @brief 跨平台编译兼容层 —— POSIX 特性宏、系统头文件、函数映射、I/O 抽象
 *
 * @details 集中处理所有平台相关的功能测试宏、缺失头文件和函数映射，
 *          避免在每个源文件中重复分散的 #ifdef 兼容代码。
 *
 *          解决的问题：
 *          - macOS 上 _POSIX_C_SOURCE 会隐藏 BSD 类型 (u_int 等)、_SC_* 宏、strdup
 *          - Linux 上 clock_gettime/strtok_r/strdup/snprintf 需要正确的 _POSIX_C_SOURCE
 *          - Windows 上 M_PI 需要 _USE_MATH_DEFINES
 *          - strtok_s (MSVC) → strtok_r (POSIX) 映射
 *          - 动态库加载 (LoadLibrary/dlopen)、文件存在性检查、目录创建
 *
 * 使用方式：
 *   在需要 POSIX 功能的 .c / .h 文件最开头添加：
 *   @code
 *   #include "lv/lv_platform.h"
 *   @endcode
 *   替代原来分散的 _POSIX_C_SOURCE / _DARWIN_C_SOURCE / _USE_MATH_DEFINES 宏定义。
 *
 * @attention
 *   必须在所有系统头文件 #include 之前包含此文件，否则功能测试宏不生效。
 *   此文件不依赖任何 Lv-00 项目头文件，可安全地作为第一个 #include。
 *
 *   不负责的内容（见其他模块）：
 *   - 固定宽度整数类型 → cross_platform.h (lv_i32 / lv_u64 等)
 *   - 原子操作          → runtime_guard.h (lv_ATOMIC_STORE / lv_ATOMIC_INC 等)
 *   - 平台/编译器/架构检测 → cross_platform.h (lv_PLATFORM_WINDOWS 等)
 *   - 高精度计时器      → lv_utils.h (lv_get_time_ns/us/ms)
 *   - 线程同步          → lv_thread.h (lv_mutex_t 等)
 *
 * @version 2.0.0
 * @date   2026-07-30
 */

#ifndef lv_PLATFORM_COMPAT_H
#define lv_PLATFORM_COMPAT_H

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════
 * 第 1 节：平台功能测试宏
 *
 * 必须在所有系统头文件 #include 之前定义。
 * ───────────────────────────────────────────────────────────────────
 * macOS:
 *   _POSIX_C_SOURCE 会隐藏 BSD 类型（u_int、u_char 等）、SC_PHYS_PAGES 等
 *   sysconf 常量和 strdup。必须改用 _DARWIN_C_SOURCE。
 * Linux / 其他 Unix:
 *   _POSIX_C_SOURCE 200809L 涵盖 clock_gettime、strtok_r、strdup、snprintf。
 * Windows:
 *   不需要 POSIX 宏（但可能需要 _USE_MATH_DEFINES，见第 2 节）。
 * ═══════════════════════════════════════════════════════════════════ */

#if defined(__APPLE__)
  /* macOS: 取消可能已设置的 _POSIX_C_SOURCE，改用 _DARWIN_C_SOURCE */
  #ifdef _POSIX_C_SOURCE
    #undef _POSIX_C_SOURCE
  #endif
  #ifndef _DARWIN_C_SOURCE
    #define _DARWIN_C_SOURCE
  #endif

#elif !defined(_WIN32)
  /* Linux / BSD / 其他 Unix: POSIX.1-2008 */
  #if !defined(_POSIX_C_SOURCE) || _POSIX_C_SOURCE < 200809L
    #define _POSIX_C_SOURCE 200809L
  #endif
#endif

/* ═══════════════════════════════════════════════════════════════════
 * 第 2 节：数学头文件与常量
 *
 * Windows: _USE_MATH_DEFINES 必须在 <math.h> 之前定义才能获得 M_PI。
 * POSIX:  即使有 _POSIX_C_SOURCE，某些实现也可能不定义 M_PI。
 *         提供回退定义确保 M_PI / M_E 始终可用。
 * ═══════════════════════════════════════════════════════════════════ */

#ifndef _USE_MATH_DEFINES
  #define _USE_MATH_DEFINES
#endif
#include <math.h>

#ifndef M_PI
  #define M_PI   3.14159265358979323846
#endif
#ifndef M_PI_2
  #define M_PI_2 1.57079632679489661923
#endif
#ifndef M_E
  #define M_E   2.71828182845904523536
#endif

/* ═══════════════════════════════════════════════════════════════════
 * 第 3 节：标准 C 头文件（最常缺失的）
 *
 * 这些是跨平台构建中经常因为功能测试宏设置不当而缺失的头文件。
 * 包含顺序无关紧要，它们彼此独立。
 * ═══════════════════════════════════════════════════════════════════ */

#include <errno.h>   /* errno, EINVAL, ... */
#include <limits.h>  /* INT_MAX, LONG_MAX, ... */
#include <stddef.h>  /* size_t, NULL, offsetof */

/* ═══════════════════════════════════════════════════════════════════
 * 第 4 节：函数映射
 *
 * 将平台特定函数映射为可移植的等价函数。
 * ═══════════════════════════════════════════════════════════════════ */

/* strtok_s（MSVC 安全版本）→ strtok_r（POSIX 可重入版本）
 *
 * strtok_s 的签名在 MSVC 和 C11 Annex K 之间不同：
 *   MSVC:      strtok_s(str, delim, context)
 *   Annex K:   strtok_s(str, delim, context)  ← 参数顺序相同
 * strtok_r 的签名与 MSVC strtok_s 兼容：
 *   POSIX:     strtok_r(str, delim, saveptr)
 *
 * 因此直接映射即可。 */
#ifndef _WIN32
  #define strtok_s  strtok_r
#endif

/* ═══════════════════════════════════════════════════════════════════
 * 第 5 节：文件系统操作抽象
 * ═══════════════════════════════════════════════════════════════════ */

#include <sys/stat.h>

#ifdef _WIN32
  #include <direct.h>
  #include <io.h>
  #define lv_mkdir(path)     _mkdir(path)
  #define lv_access(path, m) _access(path, 0)
#else
  #include <unistd.h>
  #define lv_mkdir(path)     mkdir(path, 0755)
  #define lv_access(path, m) access(path, F_OK)
#endif

/** 检查文件是否存在。返回非零值表示存在，0 表示不存在。 */
#define lv_file_exists(path) (lv_access(path, 0) == 0)

/* ═══════════════════════════════════════════════════════════════════
 * 第 6 节：动态库加载抽象
 *
 * 将 LoadLibrary / dlopen 等平台 API 统一为 lv_dlopen 系列函数。
 * 使用 static inline 实现零开销的编译期内联。
 * ═══════════════════════════════════════════════════════════════════ */

#ifdef _WIN32
  #include <windows.h>
  static inline void *lv_dlopen(const char *path) {
      return (void *)LoadLibraryA(path);
  }
  static inline void *lv_dlsym(void *handle, const char *name) {
      return (void *)GetProcAddress((HMODULE)handle, name);
  }
  static inline void lv_dlclose(void *handle) {
      if (handle) FreeLibrary((HMODULE)handle);
  }
#else
  #include <dlfcn.h>
  static inline void *lv_dlopen(const char *path) {
      return dlopen(path, RTLD_LAZY);
  }
  static inline void *lv_dlsym(void *handle, const char *name) {
      return dlsym(handle, name);
  }
  static inline void lv_dlclose(void *handle) {
      if (handle) dlclose(handle);
  }
#endif

#ifdef __cplusplus
}
#endif

#endif /* lv_PLATFORM_COMPAT_H */

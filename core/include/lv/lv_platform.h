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

/* ═══════════════════════════════════════════════════════════════════
 * 第 7 节：原子操作抽象（Atomic Operations）
 *
 * 将 Windows Interlocked* 和 GCC/Clang __atomic* 内置函数
 * 统一为 lv_ATOMIC_* 宏，消除分散的 #ifdef _WIN32 分支。
 *
 * 支持的操���：
 *   lv_ATOMIC_INC(ptr)         —— 原子自增 1（int）
 *   lv_ATOMIC_DEC(ptr)         —— 原子自减 1（int）
 *   lv_ATOMIC_ADD(ptr, val)    —— 原子加法
 *   lv_ATOMIC_SUB(ptr, val)    —— 原子减法
 *   lv_ATOMIC_INC64(ptr)       —— 原子自增 1（int64_t）
 *   lv_ATOMIC_DEC64(ptr)       —— 原子自减 1（int64_t）
 *   lv_ATOMIC_ADD64(ptr, val)  —— 原子加法（int64_t）
 *   lv_ATOMIC_EXCHANGE(ptr, v) —— 原子交换，返回旧值
 *   lv_ATOMIC_STORE(ptr, val)  —— 原子写入
 *   lv_ATOMIC_CAS_BOOL(ptr,d,e)—— 比较并交换，返回是否成功
 *   lv_ATOMIC_FENCE_ACQUIRE()  —— 获取内存屏障
 *   lv_ATOMIC_FENCE_RELEASE()  —— 释放内存屏障
 *   lv_ATOMIC_FENCE_SEQ_CST()  —— 全序内存屏障
 * ═══════════════════════════════════════════════════════════════════ */

#ifdef _WIN32
  #include <windows.h>
  /* int 原子操作 */
  #define lv_ATOMIC_INC(ptr)           InterlockedIncrement((ptr))
  #define lv_ATOMIC_DEC(ptr)           InterlockedDecrement((ptr))
  #define lv_ATOMIC_ADD(ptr, val)      InterlockedExchangeAdd((ptr), (val))
  #define lv_ATOMIC_SUB(ptr, val)      InterlockedExchangeAdd((ptr), -(val))
  /* 64 位原子操作 */
  #define lv_ATOMIC_INC64(ptr)         InterlockedIncrement64((volatile LONG64 *)(ptr))
  #define lv_ATOMIC_DEC64(ptr)         InterlockedDecrement64((volatile LONG64 *)(ptr))
  #define lv_ATOMIC_ADD64(ptr, val)    InterlockedExchangeAdd64((volatile LONG64 *)(ptr), (LONG64)(val))
  /* 通用原子操作 */
   #define lv_ATOMIC_EXCHANGE(ptr, v)   InterlockedExchange((ptr), (v))
   #define lv_ATOMIC_STORE(ptr, val)    InterlockedExchange((ptr), (val))
   /* lv_ATOMIC_CAS_BOOL: 比较并交换，返回是否成功
    * (e) 为期望值的左值指针（如 &expected），会被写入实际值
    * (d) 为 desired 新值 */
   #define lv_ATOMIC_CAS_BOOL(ptr,d,e)  (InterlockedCompareExchange((ptr), (d), *(e)) == *(e))
  /* 内存屏障 */
  #define lv_ATOMIC_FENCE_ACQUIRE()    MemoryBarrier()
  #define lv_ATOMIC_FENCE_RELEASE()    MemoryBarrier()
  #define lv_ATOMIC_FENCE_SEQ_CST()    MemoryBarrier()
#else
  /* int 原子操作 */
  #define lv_ATOMIC_INC(ptr)           __atomic_add_fetch((ptr), 1, __ATOMIC_SEQ_CST)
  #define lv_ATOMIC_DEC(ptr)           __atomic_sub_fetch((ptr), 1, __ATOMIC_SEQ_CST)
  #define lv_ATOMIC_ADD(ptr, val)      __atomic_fetch_add((ptr), (val), __ATOMIC_SEQ_CST)
  #define lv_ATOMIC_SUB(ptr, val)      __atomic_fetch_sub((ptr), (val), __ATOMIC_SEQ_CST)
  /* 64 位原子操作 */
  #define lv_ATOMIC_INC64(ptr)         __atomic_fetch_add((ptr), 1, __ATOMIC_RELAXED)
  #define lv_ATOMIC_DEC64(ptr)         __atomic_fetch_sub((ptr), 1, __ATOMIC_RELAXED)
  #define lv_ATOMIC_ADD64(ptr, val)    __atomic_fetch_add((ptr), (val), __ATOMIC_RELAXED)
  /* 通用原子操作 */
   #define lv_ATOMIC_EXCHANGE(ptr, v)   __atomic_exchange_n((ptr), (v), __ATOMIC_SEQ_CST)
   #define lv_ATOMIC_STORE(ptr, val)    __atomic_store_n((ptr), (val), __ATOMIC_RELAXED)
   /* lv_ATOMIC_CAS_BOOL: 比较并交换，返回是否成功
    * (e) 为期望值的左值指针（如 &expected），会被写入实际值
    * (d) 为 desired 新值 */
   #define lv_ATOMIC_CAS_BOOL(ptr,d,e)  __atomic_compare_exchange_n((ptr), (e), (d), 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)
  /* 内存屏障 */
  #define lv_ATOMIC_FENCE_ACQUIRE()    __atomic_thread_fence(__ATOMIC_ACQUIRE)
  #define lv_ATOMIC_FENCE_RELEASE()    __atomic_thread_fence(__ATOMIC_RELEASE)
  #define lv_ATOMIC_FENCE_SEQ_CST()    __atomic_thread_fence(__ATOMIC_SEQ_CST)
#endif

/* ═══════════════════════════════════════════════════════════════════
 * 第 8 节：互斥锁抽象（Mutex Abstraction）
 *
 * 将 Windows CRITICAL_SECTION 和 POSIX pthread_mutex_t
 * 统一为 lv_MUTEX_* 宏，消除分散的 #ifdef _WIN32 分支。
 *
 * 定义的类：
 *   lvMutex               —— 互斥锁类型（联合体，保证足够大）
 *   lv_MUTEX_INIT(m)      —— 初始化互斥锁
 *   lv_MUTEX_LOCK(m)      —— 加锁
 *   lv_MUTEX_UNLOCK(m)    —— 解锁
 *   lv_MUTEX_DESTROY(m)   —— 销毁互斥锁
 *
 * 使用方式：
 *   lvMutex my_mutex;
 *   lv_MUTEX_INIT(&my_mutex);
 *   lv_MUTEX_LOCK(&my_mutex);
 *   // ... 临界区代码 ...
 *   lv_MUTEX_UNLOCK(&my_mutex);
 *   lv_MUTEX_DESTROY(&my_mutex);
 * ═══════════════════════════════════════════════════════════════════ */

#ifdef _WIN32
  typedef CRITICAL_SECTION lvMutex;
  #define lv_MUTEX_INIT(m)      InitializeCriticalSection((m))
  #define lv_MUTEX_LOCK(m)      EnterCriticalSection((m))
  #define lv_MUTEX_UNLOCK(m)    LeaveCriticalSection((m))
  #define lv_MUTEX_DESTROY(m)   DeleteCriticalSection((m))
#else
  #include <pthread.h>
  typedef pthread_mutex_t lvMutex;
  #define lv_MUTEX_INIT(m)      pthread_mutex_init((m), NULL)
  #define lv_MUTEX_LOCK(m)      pthread_mutex_lock((m))
  #define lv_MUTEX_UNLOCK(m)    pthread_mutex_unlock((m))
  #define lv_MUTEX_DESTROY(m)   pthread_mutex_destroy((m))
#endif

#ifdef __cplusplus
}
#endif

#endif /* lv_PLATFORM_COMPAT_H */

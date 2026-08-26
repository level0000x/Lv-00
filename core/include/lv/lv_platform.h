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
#else
  #include <unistd.h>
  #define lv_mkdir(path)     mkdir(path, 0755)
#endif


/* ═══════════════════════════════════════════════════════════════════
 * 第 6 节：动态库加载抽象
 *
 * 将 LoadLibrary / dlopen 等平台 API 统一为 lv_dlopen 系列函数。
 * 使用 static inline 实现零开销的编译期内联。
 * ═══════════════════════════════════════════════════════════════════ */

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #ifndef NOIME
    #define NOIME
  #endif
  #ifndef NOSERVICE
    #define NOSERVICE
  #endif
  #ifndef NOMCX
    #define NOMCX
  #endif
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
 * 支持的操作：
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

/* 全项目唯一权威定义：指针语义（参数为指向目标变量的指针）。
 * 兼容历史左值语义宏：若使用点需按左值调用，可在调用处取址（&var）。 */
#ifndef lv_ATOMIC_INC
#ifdef _WIN32
  #include <windows.h>
  /* int 原子操作
   * 注：与 POSIX 分支的 __atomic_* 内建（接受任意指针类型）对齐，
   * 32 位宏统一将 (ptr) cast 为 volatile LONG *，调用点（int * 与
   * _Atomic int * 等）无需关心类型，保证全平台调用约定一致。 */
  #define lv_ATOMIC_INC(ptr)           InterlockedIncrement((volatile LONG *)(ptr))
  #define lv_ATOMIC_DEC(ptr)           InterlockedDecrement((volatile LONG *)(ptr))
  #define lv_ATOMIC_ADD(ptr, val)      InterlockedExchangeAdd((volatile LONG *)(ptr), (LONG)(val))
  #define lv_ATOMIC_SUB(ptr, val)      InterlockedExchangeAdd((volatile LONG *)(ptr), -(LONG)(val))
  /* 64 位原子操作 */
  #define lv_ATOMIC_INC64(ptr)         InterlockedIncrement64((volatile LONG64 *)(ptr))
  #define lv_ATOMIC_DEC64(ptr)         InterlockedDecrement64((volatile LONG64 *)(ptr))
  #define lv_ATOMIC_ADD64(ptr, val)    InterlockedExchangeAdd64((volatile LONG64 *)(ptr), (LONG64)(val))
  /* 通用原子操作 */
   #define lv_ATOMIC_EXCHANGE(ptr, v)   InterlockedExchange((volatile LONG *)(ptr), (LONG)(v))
   #define lv_ATOMIC_STORE(ptr, val)    InterlockedExchange((volatile LONG *)(ptr), (LONG)(val))
   #define lv_ATOMIC_LOAD(ptr)          InterlockedCompareExchange((volatile LONG *)(ptr), 0, 0)
   #define lv_ATOMIC_CAS(ptr, exp, des) (InterlockedCompareExchange((volatile LONG *)(ptr), (LONG)(des), *(exp)) == *(exp))
   /* lv_ATOMIC_CAS_BOOL: 比较并交换，返回是否成功
    * (e) 为期望值的左值指针（如 &expected），会被写入实际值
    * (d) 为 desired 新值
    * 注：Windows 分支 InterlockedCompareExchange 不更新 (e) 指向的期望值，
    *      POSIX 分支 __atomic_compare_exchange_n 失败时会更新 (e)；
    *      调用点若依赖失败后重试，须自行重新读取实际值（如 lv_ATOMIC_LOAD）。 */
   #define lv_ATOMIC_CAS_BOOL(ptr,d,e)  (InterlockedCompareExchange((volatile LONG *)(ptr), (LONG)(d), *(e)) == *(e))
  /* 内存屏障 */
  #define lv_ATOMIC_FENCE_ACQUIRE()    MemoryBarrier()
  #define lv_ATOMIC_FENCE_RELEASE()    MemoryBarrier()
  #define lv_ATOMIC_FENCE_SEQ_CST()    MemoryBarrier()
#else
  /* POSIX: GCC/Clang __atomic_* 内建。
   * 注意：调用点可能传 _Atomic(int) *（如 constraint_graph.h 的
   * GRAPH_ATOMIC_NODE_ID_INCREMENT、simd_ops_internal.h 的 _Atomic uint64_t
   * 计数器），__atomic_* 内建不接受 _Atomic 限定指针（Clang 报 "address
   * argument to atomic operation must be a pointer to integer or pointer"）。
   * C11 _Generic 控制表达式会剥离限定符（含 _Atomic），据此把指针 cast 回
   * 无 _Atomic 限定的整数指针；default 分支覆盖未列出的标量类型。 */
  #define lv_ATOMIC_UNQUAL_PTR(ptr) \
      _Generic((*(ptr)), \
          int:             (int *)(ptr), \
          unsigned int:    (unsigned int *)(ptr), \
          long:            (long *)(ptr), \
          unsigned long:   (unsigned long *)(ptr), \
          long long:       (long long *)(ptr), \
          unsigned long long: (unsigned long long *)(ptr), \
          default:         (__typeof__(*(ptr)) *)(ptr))
  #define lv_ATOMIC_INC(ptr)           __atomic_add_fetch(lv_ATOMIC_UNQUAL_PTR(ptr), 1, __ATOMIC_SEQ_CST)
  #define lv_ATOMIC_DEC(ptr)           __atomic_sub_fetch(lv_ATOMIC_UNQUAL_PTR(ptr), 1, __ATOMIC_SEQ_CST)
  #define lv_ATOMIC_ADD(ptr, val)      __atomic_fetch_add(lv_ATOMIC_UNQUAL_PTR(ptr), (val), __ATOMIC_SEQ_CST)
  #define lv_ATOMIC_SUB(ptr, val)      __atomic_fetch_sub(lv_ATOMIC_UNQUAL_PTR(ptr), (val), __ATOMIC_SEQ_CST)
  /* 64 位原子操作 */
  #define lv_ATOMIC_INC64(ptr)         __atomic_fetch_add(lv_ATOMIC_UNQUAL_PTR(ptr), 1, __ATOMIC_RELAXED)
  #define lv_ATOMIC_DEC64(ptr)         __atomic_fetch_sub(lv_ATOMIC_UNQUAL_PTR(ptr), 1, __ATOMIC_RELAXED)
  #define lv_ATOMIC_ADD64(ptr, val)    __atomic_fetch_add(lv_ATOMIC_UNQUAL_PTR(ptr), (val), __ATOMIC_RELAXED)
  /* 通用原子操作 */
   #define lv_ATOMIC_EXCHANGE(ptr, v)   __atomic_exchange_n(lv_ATOMIC_UNQUAL_PTR(ptr), (v), __ATOMIC_SEQ_CST)
   #define lv_ATOMIC_STORE(ptr, val)    __atomic_store_n(lv_ATOMIC_UNQUAL_PTR(ptr), (val), __ATOMIC_RELAXED)
   #define lv_ATOMIC_LOAD(ptr)          __atomic_load_n(lv_ATOMIC_UNQUAL_PTR(ptr), __ATOMIC_RELAXED)
   #define lv_ATOMIC_CAS(ptr, exp, des) __atomic_compare_exchange_n(lv_ATOMIC_UNQUAL_PTR(ptr), (exp), (des), 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)
   /* lv_ATOMIC_CAS_BOOL: 比较并交换，返回是否成功
    * (e) 为期望值的左值指针（如 &expected），会被写入实际值
    * (d) 为 desired 新值 */
   #define lv_ATOMIC_CAS_BOOL(ptr,d,e)  __atomic_compare_exchange_n(lv_ATOMIC_UNQUAL_PTR(ptr), (e), (d), 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)
  /* 内存屏障 */
  #define lv_ATOMIC_FENCE_ACQUIRE()    __atomic_thread_fence(__ATOMIC_ACQUIRE)
  #define lv_ATOMIC_FENCE_RELEASE()    __atomic_thread_fence(__ATOMIC_RELEASE)
  #define lv_ATOMIC_FENCE_SEQ_CST()    __atomic_thread_fence(__ATOMIC_SEQ_CST)
#endif
#endif /* lv_ATOMIC_INC */

/* ═══════════════════════════════════════════════════════════════════
 * 第 8 节：互斥锁抽象（Mutex Abstraction）—— 兼容别名层
 *
 * 历史 API lvMutex / lv_MUTEX_* 与 lv_thread.h 的新 API
 * lv_mutex_t / lv_mutex_init/lock/unlock/destroy 底层原语完全一致
 * （同一 CRITICAL_SECTION / pthread_mutex_t 的两套薄包装）。
 *
 * 统一方案：lv_thread.h 的新 API 为唯一实现；此处保留旧名作为
 * 兼容别名（类型别名 + 宏别名），调用点零改动即获得统一语义。
 *
 * 定义的类：
 *   lv_mutex_t            —— 互斥锁类型（唯一实现，本头定义）
 *   lvMutex               —— 旧名类型别名（== lv_mutex_t）
 *   lv_MUTEX_INIT(m)      —— == lv_mutex_init((m))
 *   lv_MUTEX_LOCK(m)      —— == lv_mutex_lock((m))
 *   lv_MUTEX_UNLOCK(m)    —— == lv_mutex_unlock((m))
 *   lv_MUTEX_DESTROY(m)   —— == lv_mutex_destroy((m))
 *
 * 注意：使用 lv_MUTEX_* 宏的编译单元必须包含 lv/lv_thread.h
 * （lv_mutex_* 为 static inline 函数，需要其声明可见）。
 *
 * 使用方式（旧 API，语义不变）：
 *   lvMutex my_mutex;
 *   lv_MUTEX_INIT(&my_mutex);
 *   lv_MUTEX_LOCK(&my_mutex);
 *   // ... 临界区代码 ...
 *   lv_MUTEX_UNLOCK(&my_mutex);
 *   lv_MUTEX_DESTROY(&my_mutex);
 * ═══════════════════════════════════════════════════════════════════ */

#ifdef _WIN32
  typedef CRITICAL_SECTION lv_mutex_t;  /* 与 lv_thread.h 统一的新 API 类型 */
  typedef lv_mutex_t lvMutex;           /* 旧名兼容别名 */
#else
  #include <pthread.h>
  typedef pthread_mutex_t lv_mutex_t;
  typedef lv_mutex_t lvMutex;
#endif

/* 兼容别名宏：统一映射到 lv_thread.h 的 lv_mutex_*（唯一实现） */
#define lv_MUTEX_INIT(m)      lv_mutex_init((m))
#define lv_MUTEX_LOCK(m)      lv_mutex_lock((m))
#define lv_MUTEX_UNLOCK(m)    lv_mutex_unlock((m))
#define lv_MUTEX_DESTROY(m)   lv_mutex_destroy((m))

#ifdef __cplusplus
}
#endif

#endif /* lv_PLATFORM_COMPAT_H */

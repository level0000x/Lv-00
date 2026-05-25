/**
 * @file lv00_internal.h (src/)
 * @brief 内部宏定义统一入口（src/ 目录的桥接文件）
 *
 * 【设计意图】编译器对 #include "lv00_internal.h" 优先在当前目录（src/）搜索。
 * 本文件将 lv00.h 中的核心宏和本文件中定义的
 * 辅助宏合并加载，作为所有 .c 源文件的统一宏入口。
 *
 * 【三层架构说明】
 *   第 1 层 - lv00.h：对外公开的核心平台宏，供库用户和内部代码共同使用。
 *      包含线程局部存储、版本号、废弃标记等跨平台兼容性定义。
 *   第 2 层 - 内部辅助宏：不便暴露给库用户的编译器/工具宏。
 *      包含未使用变量抑制、数组计数、安全格式化输出、数学常量等。
 *      这些宏在此文件中定义，作为内部辅助宏的权威来源。
 *   第 3 层 - 项目级公共常量：消除 src/ 目录下所有 .c 文件中的魔术数字。
 *      包含引擎配置默认值、哈希参数、容差阈值、迭代上限等。
 *      所有常量集中在此定义，确保修改一处、全项目生效。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#ifndef LV00_LV00_INTERNAL_H
#define LV00_LV00_INTERNAL_H

/* [P1 修复] 添加 extern "C" 保护，确保 C++ 编译器能正确链接此头文件 */
#ifdef __cplusplus
extern "C" {
#endif

/* ====================================================================
 * 第 1 层：lv00.h 提供的核心平台宏
 *   - LV00_THREAD_LOCAL  (线程局部存储)
 *   - LV00_LOCALTIME     (线程安全 localtime)
 *   - LV00_DEPRECATED(msg) (函数废弃标记)
 *   - LV00_VERSION_STRING  等
 * ==================================================================== */
#include "lv00.h"

/* ====================================================================
 * 第 2 层：内部辅助宏（不便在 lv00.h 中暴露的）
 *   - LV00_UNUSED(x)        抑制未使用变量警告
 *   - LV00_ARRAY_COUNT(arr) 获取数组元素个数
 *   - LV00_SAFE_SNPRINTF    安全格式化输出到定长缓冲区
 *   - M_PI                  圆周率常量
 *
 * 这些宏在此文件中直接定义，作为内部辅助宏的权威来源。
 * ==================================================================== */

/* ── 数学常量 ── */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ── 自然对数底数 e ── */
#ifndef M_E
#define M_E 2.71828182845904523536
#endif

/* ── 抑制"未使用变量"警告 ── */
#ifndef LV00_UNUSED
#define LV00_UNUSED(x) ((void) (x))
#endif

/* ── 数组元素计数 ──
 * 注意：此宏仅适用于编译期数组，传入指针会产生错误结果。
 * 编译时可通过 _Generic 或 static_assert 进行基本检查。 */
#ifndef LV00_ARRAY_COUNT
#define LV00_ARRAY_COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

/* ── 安全加法（防溢出） ──
 * 注意：GCC/Clang 分支下参数 a、b 在宏中仅求值一次，可安全传入含副作用的表达式。
 * 若 a + b > limit，返回 limit；否则返回 a + b。
 * 提供 GCC/Clang 和 MSVC 两种实现，自动根据编译器选择。 */
#ifndef LV00_SAFE_ADD
#ifdef _MSC_VER
/* MSVC 不支持 __typeof__ 和语句表达式，使用 __int64 作为通用类型。
 * 注意：此分支下参数 a、b、limit 会被求值多次，请避免传入含副作用的表达式。 */
#define LV00_SAFE_ADD(a, b, limit) \
    ((__int64) (a) > (__int64) (limit) - (__int64) (b) ? (__int64) (limit) : ((__int64) (a) + (__int64) (b)))
#else
/* GCC/Clang 实现：使用 __typeof__ 推导参数类型 */
#define LV00_SAFE_ADD(a, b, limit)                         \
    (__extension__({                                       \
        __typeof__(a) _sa_a = (a);                         \
        __typeof__(b) _sa_b = (b);                         \
        __typeof__(limit) _sa_l = (limit);                 \
        (_sa_a > _sa_l - _sa_b) ? _sa_l : (_sa_a + _sa_b); \
    }))
#endif
#endif

/* ── 安全 snprintf：确保返回值非负且不超过 buf_size-1 ── */
#ifndef LV00_SAFE_SNPRINTF
#define LV00_SAFE_SNPRINTF(written, buf, buf_size, ...)                                            \
    do {                                                                                           \
        int _sn = snprintf((buf), (buf_size), __VA_ARGS__);                                        \
        (written) = (_sn < 0) ? 0 : ((size_t) (_sn) >= (buf_size) ? (int) ((buf_size) - 1) : _sn); \
    } while (0)
#endif

/* ====================================================================
 * 第 3 层：项目级公共常量
 *
 * 消除魔术数字，全部在此定义，供所有 .c 源文件引用。
 * ==================================================================== */

/* ── 引擎配置默认值 ── */

/** @brief 引擎最大迭代次数默认值（求解器/重写循环的迭代上限） */
#define LV00_DEFAULT_MAX_ITERATIONS 1000
/** @brief 默认符号精度位数（symbolic_coord 精度，单位：比特） */
#define LV00_DEFAULT_PRECISION_BITS 64
/** @brief 默认重写步数上限（每次 solve 调用中的重写步数限制）
 *  与 config.h 中 LV00_CONFIG_DEFAULT_REWRITE_LIMIT 保持一致 */
#define LV00_DEFAULT_REWRITE_STEP_LIMIT 1000
/** @brief 默认内存使用上限（单位：MB，0 表示无限制） */
#define LV00_DEFAULT_MEMORY_LIMIT_MB 0

/* ── 动态数组初始容量与增长因子 ── */
/** @brief 动态数组的初始容量（统一使用 config.h 中的定义） */
#ifndef LV00_CONFIG_INITIAL_ARRAY_CAPACITY
#define LV00_INITIAL_ARRAY_CAPACITY 4
#else
#define LV00_INITIAL_ARRAY_CAPACITY LV00_CONFIG_INITIAL_ARRAY_CAPACITY
#endif
/** @brief 动态数组增长因子（容量不足时乘以该因子进行扩容） */
#define LV00_ARRAY_GROWTH_FACTOR 2

/* ── 哈希索引参数 ── */
/** @brief 节点哈希索引初始桶数量（约束图中按节点 ID 快速查找） */
#define LV00_NODE_INDEX_INITIAL_SIZE 64
/** @brief 约束哈希索引初始桶数量（约束图中按约束 ID 快速查找） */
#define LV00_CONSTRAINT_INDEX_INITIAL_SIZE 64
/** @brief 哈希索引负载因子上限（超过此值触发 rehash 扩容） */
#define LV00_INDEX_LOAD_FACTOR 0.75

/* ── FNV-1a 哈希常量 ── */
/** FNV-1a 64 位哈希参数（项目统一使用 64 位） */
#define LV00_FNV64_OFFSET_BASIS 0xcbf29ce484222325ULL
#define LV00_FNV64_PRIME 0x100000001b3ULL
/** FNV-1a 32 位哈希参数（仅在需要 32 位哈希时使用） */
#define LV00_FNV32_OFFSET_BASIS 2166136261u
#define LV00_FNV32_PRIME 16777619u
/* 简化版乘法器：2654435769 = 0x9E3779B9（黄金比例 phi 的 2^32 倍），
   用于快速位混合，非标准 FNV 参数，仅在特定路径使用 */
#define LV00_FNV_HASH_MULTIPLIER 2654435769u

/* ── 位数熔断阈值 ── */
/** @brief 位数熔断阈值：当坐标值的位数超过此值，触发降级或熔断保护 */
#define LV00_BIT_CUTOFF_THRESHOLD 1000000
/** @brief 符号坐标系统支持的最大精度位数（超过此值的操作将被拒绝） */
#define LV00_MAX_PRECISION_BITS 100

/* ── 电路溢出阈值 ── */
/** @brief 位电路溢出容错次数：连续溢出超过此次数触发回滚/降级策略 */
#define LV00_CIRCUIT_OVERFLOW_THRESHOLD 3

/* ── 健康检查阈值 ── */
/** @brief 健康检查满分值（lv00_health_check 返回的最高分） */
#define LV00_HEALTH_SCORE_MAX 100
/** @brief 内存使用率告警阈值（占总内存上限的比率，超过触发扣分） */
#define LV00_HEALTH_MEMORY_USAGE_RATIO 0.9
/** @brief 内存泄漏率阈值（泄漏内存占总分配的比率，超过触发扣分） */
#define LV00_HEALTH_MEMORY_LEAK_RATIO 0.5
/** @brief 近期错误惩罚分值（每次近期错误从满分中扣除的分数） */
#define LV00_HEALTH_RECENT_ERROR_PENALTY 15
/** @brief 内存使用告警惩罚分值（内存使用率超阈值的扣分） */
#define LV00_HEALTH_MEMORY_WARNING_PENALTY 20
/** @brief 内存泄漏惩罚分值（检测到泄漏时的扣分） */
#define LV00_HEALTH_MEMORY_LEAK_PENALTY 10

/* ── 数值计算容差 ── */
/** @brief 双精度浮点通用容差（用于一般性的浮点比较） */
#define LV00_EPSILON_DOUBLE 1e-12
/** @brief 数值比较容差（用于坐标和几何量的相等比较） */
#define LV00_EPSILON_NUMERIC_COMPARE 1e-10
/** @brief 牛顿迭代法收敛容差（用于根细化求解的终止条件） */
#define LV00_EPSILON_NEWTON 1e-15
/** @brief 线段内部点判定容差（判断点是否在线段内部的区间容差） */
#define LV00_EPSILON_SEGMENT_INTERIOR 1e-9
/** @brief 分数零值判定容差（分母接近零时的下限阈值） */
#define LV00_EPSILON_FRACTION_ZERO 1e-300

/* ── 连分数迭代上限 ── */
/** @brief 连分数展开的最大迭代次数（超限后使用最后的近似值） */
#define LV00_CONTINUED_FRACTION_MAX_ITER 1000

/* ── 根隔离参数 ── */
/** @brief 根隔离最大子区间数（Sturm 序列/二分法划分子区间的上限） */
#define LV00_MAX_SUBINTERVALS 256
/** @brief 根隔离容差（子区间宽度小于此值时认为包含单根） */
#define LV00_ROOT_EPSILON 1e-12

/* ── 牛顿/代数数细化 ── */
/** @brief 牛顿法细化迭代次数（对近似数值根进行高精度修正的迭代上限） */
#define LV00_NEWTON_REFINE_ITERATIONS 10

/* ── 代数数比较/细化迭代 ── */
/** @brief 代数数比较/细化迭代上限（代数数化简与比较的最大轮次） */
#define LV00_ALGEBRAIC_REFINE_ITERATIONS 100

/* ── 降级近似分母基数 ── */
/** @brief 降级近似分母基数（将符号坐标以有理数近似时的分母上限） */
#define LV00_DOWNGRADE_DENOMINATOR 1000000000

/* ── 坐标值过大阈值（用于降级判断） ── */
/** @brief 坐标值过大阈值：当坐标绝对值超过此值时触发降级策略 */
#define LV00_VALUE_TOO_LARGE 9.2e9

/* ── 二次根式化简循环 ── */
/** @brief 二次根式化简最大尝试次数（移除外层平方因子的循环上限） */
#define LV00_SQRT_REMOVE_MAX_TRIES 100000

/* ── 求解器 (solver.c) 模块级常量 ── */
/** @brief 缩放因子 —— 提供约6位十进制精度，用于有理数转换，未来应使用mpq直接运算 */
#define LV00_SOLVER_SCALE_FACTOR 1000000
/** @brief 质数搜索上限 —— 平方因子分解时的最大质数搜索范围 */
#define LV00_SOLVER_PRIME_LIMIT 100000
/** @brief Buchberger算法步数限制 —— Groebner基计算的迭代上限 */
#define LV00_SOLVER_BUCHBERGER_STEP_LIMIT 10000

/* ── 重写引擎 (rewrite.c) 模块级常量 ── */
/** @brief Weisfeiler-Lehman 图哈希乘法器（用于重写规则匹配时的图结构哈希计算） */
#define LV00_REWRITE_WL_HASH_MULTIPLIER 65599

/* ── 函数块 (func_block.c) 模块级常量 ── */
/** @brief 函数块ID起始偏移量 —— 避免与普通节点ID冲突 */
#define LV00_FUNC_BLOCK_ID_OFFSET 10000
/** @brief 距离平方的默认值，当无法计算有效距离时返回（func_block.c 和 func_block_selector.c 共用） */
#define LV00_DEFAULT_DISTANCE_SQUARED 1e30

/* ── 递归 (recursion.c) 模块级常量 ── */
/** @brief 递归深度硬上限 —— 防止栈溢出 */
#define LV00_MAX_RECURSION_DEPTH_LIMIT 100000

/* ── 预设函数块 (func_block_preset.c) 模块级常量 ── */
/** @brief 预设函数块库版本主版本号 */
#define LV00_PRESET_LIBRARY_VERSION_MAJOR 5
/** @brief 预设函数块库版本次版本号 */
#define LV00_PRESET_LIBRARY_VERSION_MINOR 0
/** @brief 预设函数块库版本补丁版本号 */
#define LV00_PRESET_LIBRARY_VERSION_PATCH 0
/** @brief 预设函数块最大数量（注册表中可注册的预设上限） */
#define LV00_PRESET_MAX_COUNT 1024
/** @brief 预设函数块最大参数数量（单个预设的最大形参个数） */
#define LV00_PRESET_MAX_PARAMS 16
/** @brief 预设ID起始偏移量（预设函数块的节点ID从此值开始分配） */
#define LV00_PRESET_ID_OFFSET 60000

/* ── 编译期日志级别过滤 ──
 * 通过定义 LV00_LOG_LEVEL_COMPILE 来控制编译期日志级别：
 *   0 = 关闭所有日志
 *   1 = 仅错误 (ERROR)
 *   2 = 错误 + 警告 (WARN)
 *   3 = 错误 + 警告 + 信息 (INFO)
 *   4 = 全部日志 (DEBUG)
 * 未定义时默认为 4（全部启用），保持向后兼容
 */
#ifndef LV00_LOG_LEVEL_COMPILE
#define LV00_LOG_LEVEL_COMPILE 4 /* 默认编译全部日志级别 */
#endif

#define LV00_COMPILE_LOG_LEVEL_ERROR 1   /**< 编译期日志级别：仅错误 */
#define LV00_COMPILE_LOG_LEVEL_WARN 2    /**< 编译期日志级别：错误 + 警告 */
#define LV00_COMPILE_LOG_LEVEL_INFO 3    /**< 编译期日志级别：错误 + 警告 + 信息 */
#define LV00_COMPILE_LOG_LEVEL_DEBUG 4   /**< 编译期日志级别：全部日志（包括调试信息） */

/* ── 运行时日志级别 ── */
#define LV00_LOG_LEVEL_ERROR 1    /**< 运行时日志级别：错误 */
#define LV00_LOG_LEVEL_WARNING 2  /**< 运行时日志级别：警告 */
#define LV00_LOG_LEVEL_INFO 3     /**< 运行时日志级别：信息 */
#define LV00_LOG_LEVEL_DEBUG 4    /**< 运行时日志级别：调试 */

/* ── 日志宏（带编译期过滤） ── */
#if LV00_LOG_LEVEL_COMPILE >= LV00_COMPILE_LOG_LEVEL_ERROR
/** @brief 错误日志宏：输出错误级别日志，附带文件名和行号 */
#define LV00_LOG_ERROR(fmt, ...) lv00_log_message(LV00_LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define LV00_LOG_ERROR(fmt, ...) ((void) 0)
#endif

#if LV00_LOG_LEVEL_COMPILE >= LV00_COMPILE_LOG_LEVEL_WARN
/** @brief 警告日志宏：输出警告级别日志，附带文件名和行号 */
#define LV00_LOG_WARNING(fmt, ...) lv00_log_message(LV00_LOG_LEVEL_WARNING, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define LV00_LOG_WARNING(fmt, ...) ((void) 0)
#endif

#if LV00_LOG_LEVEL_COMPILE >= LV00_COMPILE_LOG_LEVEL_INFO
/** @brief 信息日志宏：输出信息级别日志，附带文件名和行号 */
#define LV00_LOG_INFO(fmt, ...) lv00_log_message(LV00_LOG_LEVEL_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define LV00_LOG_INFO(fmt, ...) ((void) 0)
#endif

#if LV00_LOG_LEVEL_COMPILE >= LV00_COMPILE_LOG_LEVEL_DEBUG
/** @brief 调试日志宏：输出调试级别日志，附带文件名和行号 */
#define LV00_LOG_DEBUG(fmt, ...) lv00_log_message(LV00_LOG_LEVEL_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define LV00_LOG_DEBUG(fmt, ...) ((void) 0)
#endif

/** @brief 统一日志函数声明（在 lv00_utils.c 中实现）
 *  @param level 日志级别（LV00_LOG_LEVEL_ERROR ~ LV00_LOG_LEVEL_DEBUG）
 *  @param file  源文件名（通常传入 __FILE__）
 *  @param line  源代码行号（通常传入 __LINE__）
 *  @param fmt   printf 风格的格式化字符串
 *  @param ...   可变参数列表 */
void lv00_log_message(int level, const char *file, int line, const char *fmt, ...);

/* ====================================================================
 * 统一内联函数宏
 * ==================================================================== */
#ifndef LV00_INLINE
#if defined(__GNUC__) || defined(__clang__)
#define LV00_INLINE static inline __attribute__((unused))
#elif defined(_MSC_VER)
#define LV00_INLINE static __inline
#else
#define LV00_INLINE static
#endif
#endif

/* ====================================================================
 * 统一平台抽象层 —— 互斥锁与条件变量
 * ====================================================================
 * 集中管理跨平台线程同步原语，避免各模块各自重复定义。
 * 使用方式：在需要线程同步的 .c 文件中 #include "lv00_internal.h"
 *
 * 现有模块适配说明：
 *   - memory_pool.c   使用 LV00_MUTEX_INIT/DESTROY/LOCK/UNLOCK（带 & 取地址）
 *   - runtime_monitor.c 使用 MUTEX_INIT/DESTROY/LOCK/UNLOCK（带 & 取地址）
 *   - stream.c        使用 lv00_mutex_create/destroy/lock/unlock（函数形式，堆分配）
 * 各模块可逐步迁移到统一的 lv00_mutex_* / lv00_condvar_* 命名。
 * ==================================================================== */

#ifdef LV00_THREAD_SAFE

/* 互斥锁类型 */
#ifdef _WIN32
typedef CRITICAL_SECTION lv00_mutex_t;
typedef CONDITION_VARIABLE lv00_condvar_t;
#else
typedef pthread_mutex_t lv00_mutex_t;
typedef pthread_cond_t lv00_condvar_t;
#endif

/* 互斥锁操作宏 */
#define LV00_MUTEX_INIT(m)    \
    do { \
        _Static_assert(sizeof(m) >= sizeof(lv00_mutex_t), "lv00_mutex_t size mismatch"); \
        memset(&(m), 0, sizeof(m)); \
        /* 实际初始化由下面的平台特定代码完成 */ \
    } while(0)

/* 注意：由于 C 宏无法在 do-while 内使用平台特定的初始化函数，
 * 这里采用内联函数方式。各模块可继续使用各自的宏定义，
 * 但应逐步迁移到统一的 lv00_mutex_* 命名。 */

/* 互斥锁初始化/销毁/加锁/解锁 —— Windows 实现 */
#ifdef _WIN32
#define LV00_MUTEX_INIT_PTR(m)    InitializeCriticalSection((m))
#define LV00_MUTEX_DESTROY_PTR(m) DeleteCriticalSection((m))
#define LV00_MUTEX_LOCK_PTR(m)    EnterCriticalSection((m))
#define LV00_MUTEX_UNLOCK_PTR(m)  LeaveCriticalSection((m))

/* 条件变量初始化/销毁/信号/等待 */
#define LV00_CONDVAR_INIT_PTR(cv)     InitializeConditionVariable((cv))
#define LV00_CONDVAR_DESTROY_PTR(cv)  ((void)0) /* Windows 不需要销毁 */
#define LV00_CONDVAR_SIGNAL_PTR(cv)   WakeConditionVariable((cv))
#define LV00_CONDVAR_BROADCAST_PTR(cv) WakeAllConditionVariable((cv))
#define LV00_CONDVAR_WAIT_PTR(cv, m)  SleepConditionVariableCS((cv), (m), INFINITE)

#else /* POSIX 实现 */
#define LV00_MUTEX_INIT_PTR(m)    pthread_mutex_init((m), NULL)
#define LV00_MUTEX_DESTROY_PTR(m) pthread_mutex_destroy((m))
#define LV00_MUTEX_LOCK_PTR(m)    pthread_mutex_lock((m))
#define LV00_MUTEX_UNLOCK_PTR(m)  pthread_mutex_unlock((m))

#define LV00_CONDVAR_INIT_PTR(cv)     pthread_cond_init((cv), NULL)
#define LV00_CONDVAR_DESTROY_PTR(cv)  pthread_cond_destroy((cv))
#define LV00_CONDVAR_SIGNAL_PTR(cv)   pthread_cond_signal((cv))
#define LV00_CONDVAR_BROADCAST_PTR(cv) pthread_cond_broadcast((cv))
#define LV00_CONDVAR_WAIT_PTR(cv, m)  pthread_cond_wait((cv), (m))

#endif /* _WIN32 */

#else /* LV00_THREAD_SAFE 未定义时为空操作 */
#define LV00_MUTEX_INIT_PTR(m)    ((void)0)
#define LV00_MUTEX_DESTROY_PTR(m) ((void)0)
#define LV00_MUTEX_LOCK_PTR(m)    ((void)0)
#define LV00_MUTEX_UNLOCK_PTR(m)  ((void)0)
#define LV00_CONDVAR_INIT_PTR(cv)     ((void)0)
#define LV00_CONDVAR_DESTROY_PTR(cv)  ((void)0)
#define LV00_CONDVAR_SIGNAL_PTR(cv)   ((void)0)
#define LV00_CONDVAR_BROADCAST_PTR(cv) ((void)0)
#define LV00_CONDVAR_WAIT_PTR(cv, m)  ((void)0)
#endif /* LV00_THREAD_SAFE */

/* ====================================================================
 * 统一 FNV-1a 哈希函数
 * ====================================================================
 * 消除 rewrite.c、normalization.c、memory_pool.c 中的重复实现。
 * ==================================================================== */

/**
 * @brief FNV-1a 32位哈希 —— 对字节数组计算哈希值
 * @param data 输入数据指针
 * @param len  数据长度（字节）
 * @return 32位哈希值
 */
LV00_INLINE uint32_t lv00_fnv1a_32(const void *data, size_t len) {
    const unsigned char *bytes = (const unsigned char *)data;
    uint32_t hash = 2166136261u; /* FNV offset basis */
    for (size_t i = 0; i < len; i++) {
        hash ^= bytes[i];
        hash *= 16777619u; /* FNV prime */
    }
    return hash;
}

/**
 * @brief FNV-1a 32位哈希 —— 对以 null 结尾的字符串计算哈希值
 * @param str 输入字符串
 * @return 32位哈希值
 */
LV00_INLINE uint32_t lv00_fnv1a_str(const char *str) {
    if (!str) return 0;
    uint32_t hash = 2166136261u;
    while (*str) {
        hash ^= (unsigned char)*str++;
        hash *= 16777619u;
    }
    return hash;
}

#ifdef __cplusplus
}
#endif

#endif /* LV00_LV00_INTERNAL_H */

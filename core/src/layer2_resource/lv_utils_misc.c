/**
 * @file lv_utils_misc.c
 * @brief 版本/时间/随机/哈希/日志等杂项工具
 *
 * @details 从 lv_utils.c 拆分的子模块（Lv-00 项目 v3.3.0+）。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv_utils.h"
#include "lv_utils_internal.h"

#include "lv/allocator.h"
#include "lv/lv_file.h"
#include "lv/lv_path.h"

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "error_codes.h"
#include "lv.h"
#include "debug.h"
#include "lv_internal.h"

/* ============================================================
 * 版本管理
 * ============================================================ */

lvVersion *version_parse(const char *version_str) {
    if (!version_str)
        return NULL; /* NULL 字符串无法解析，合法哨兵 */

    lvVersion *ver = lv_calloc(1, sizeof(lvVersion));
    if (!ver)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "version_parse calloc 失败");

    /* 解析主版本.次版本.修订版本 */
    int parsed = sscanf(version_str, "%d.%d.%d", &ver->major, &ver->minor, &ver->patch);
    if (parsed < 2) {
        lv_free((void **) &ver);
        lv_RETURN_ERROR_NULL(lv_ERROR_PARSE, "version_parse 解析版本号失败");
    }
    if (parsed == 2)
        ver->patch = 0;

    /* 解析预发布标识 */
    char *dash = strchr(version_str, '-');
    if (dash) {
        char *plus = strchr(dash, '+');
        if (plus) {
            /* 添加 plus > dash 边界条件检查，防止指针运算溢出 */
            if (plus > dash && (size_t) (plus - dash) > 1) {
                ver->prerelease = lv_malloc((size_t) (plus - dash));
                if (ver->prerelease) {
                    /* 使用 memcpy 进行精确长度复制（已分配精确内存，手动零终止） */
                    memcpy(ver->prerelease, dash + 1, (size_t) (plus - dash - 1));
                    ver->prerelease[plus - dash - 1] = '\0';
                }
            } else {
                /* prerelease 部分为空（如 "1.0.0-+build"），prerelease 设为 NULL */
                ver->prerelease = NULL;
            }
            ver->build = lv_strdup_safe(plus + 1);
        } else {
            ver->prerelease = lv_strdup_safe(dash + 1);
        }
    }

    return ver;
}

void version_destroy(lvVersion *ver) {
    if (!ver)
        return;
    lv_free((void **) &ver->prerelease);
    lv_free((void **) &ver->build);
    lv_free((void **) &ver);
}

char *version_to_string(const lvVersion *ver) {
    if (!ver)
        return NULL;

    if (ver->prerelease && ver->build) {
        return lv_asprintf("%d.%d.%d-%s+%s", ver->major, ver->minor, ver->patch, ver->prerelease, ver->build);
    } else if (ver->prerelease) {
        return lv_asprintf("%d.%d.%d-%s", ver->major, ver->minor, ver->patch, ver->prerelease);
    } else if (ver->build) {
        return lv_asprintf("%d.%d.%d+%s", ver->major, ver->minor, ver->patch, ver->build);
    } else {
        return lv_asprintf("%d.%d.%d", ver->major, ver->minor, ver->patch);
    }
}

int version_compare(const lvVersion *v1, const lvVersion *v2) {
    if (!v1 || !v2)
        return 0;

    if (v1->major != v2->major)
        return (v1->major > v2->major) ? 1 : -1;
    if (v1->minor != v2->minor)
        return (v1->minor > v2->minor) ? 1 : -1;
    if (v1->patch != v2->patch)
        return (v1->patch > v2->patch) ? 1 : -1;

    /* 预发布版本小于正式版本 */
    if (v1->prerelease && !v2->prerelease)
        return -1;
    if (!v1->prerelease && v2->prerelease)
        return 1;
    if (v1->prerelease && v2->prerelease) {
        int cmp = strcmp(v1->prerelease, v2->prerelease);
        if (cmp != 0)
            return (cmp > 0) ? 1 : -1;
    }

    return 0;
}

bool version_compatible(const lvVersion *required, const lvVersion *actual) {
    if (!required || !actual)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "version_compatible 参数为空");

    /* 主版本必须相同 */
    if (required->major != actual->major)
        return false;

    /* 实际版本必须大于等于要求版本 */
    return version_compare(actual, required) >= 0;
}

bool lv_check_version(const char *min_version) {
    lvVersion *min = version_parse(min_version);
    if (!min)
        lv_RETURN_ERROR_BOOL(lv_ERROR_PARSE, "lv_check_version 解析版本号失败");

    lvVersion current;
    current.major = lv_VERSION_MAJOR;
    current.minor = lv_VERSION_MINOR;
    current.patch = lv_VERSION_PATCH;
    current.prerelease = NULL;
    current.build = NULL;

    bool compatible = version_compatible(min, &current);
    version_destroy(min);
    return compatible;
}

/* ============================================================
 * 时间工具
 * ============================================================ */

/* 时间单位转换常量 */
#define lv_US_PER_MS 1000   /**< 微秒转毫秒 */
#define lv_MS_PER_S 1000    /**< 毫秒转秒 */
#define lv_US_PER_S 1000000 /**< 微秒转秒 */
#define lv_NS_PER_S 1000000000ULL /**< 纳秒转秒 */

#ifdef _WIN32

uint64_t lv_get_time_ns(void) {
    static double ns_per_count = 0.0;
    if (ns_per_count == 0.0) {
        LARGE_INTEGER freq;
        QueryPerformanceFrequency(&freq);
        ns_per_count = 1e9 / (double)freq.QuadPart;
    }
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (uint64_t)((double)counter.QuadPart * ns_per_count);
}

uint64_t lv_get_time_us(void) {
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (uint64_t) (count.QuadPart * (LONGLONG) lv_US_PER_S / freq.QuadPart);
}

uint64_t lv_get_wallclock_ns(void) {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER li;
    li.LowPart = ft.dwLowDateTime;
    li.HighPart = ft.dwHighDateTime;
    /* FILETIME epoch is 1601-01-01, Unix epoch is 1970-01-01 */
    const uint64_t EPOCH_DIFF = 116444736000000000ULL;
    return (li.QuadPart - EPOCH_DIFF) * 100;
}

#else
#include <sys/time.h>


uint64_t lv_get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * lv_NS_PER_S + (uint64_t)ts.tv_nsec;
}

uint64_t lv_get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t) tv.tv_sec * lv_US_PER_S + (uint64_t) tv.tv_usec;
}

uint64_t lv_get_wallclock_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * lv_NS_PER_S + (uint64_t)ts.tv_nsec;
}
#endif

uint64_t lv_get_wallclock_ms(void) {
    return lv_get_wallclock_ns() / 1000000ULL;
}

uint64_t lv_get_time_ms(void) {
    return lv_get_time_us() / lv_US_PER_MS;
}

const char *lv_format_time(uint64_t timestamp_us, char *buf, size_t buf_size) {
    if (!buf || buf_size == 0)
        return NULL;

    time_t sec = (time_t) (timestamp_us / lv_US_PER_S);
    /* 修复：使用线程安全的 lv_LOCALTIME 宏替代非线程安全的 localtime */
    struct tm tm_buf;
    lv_LOCALTIME(&sec, &tm_buf);

    strftime(buf, buf_size, "%Y-%m-%d %H:%M:%S", &tm_buf);
    return buf;
}

/* ============================================================
 * 随机数生成
 * ============================================================ */

/* xorshift64* 伪随机数生成器参数 */
#define lv_XORSHIFT_SHIFT_A 12                       /**< 第一段右移位数 */
#define lv_XORSHIFT_SHIFT_B 25                       /**< 左移位数 */
#define lv_XORSHIFT_SHIFT_C 27                       /**< 第二段右移位数 */
#define lv_XORSHIFT_MULTIPLIER 0x2545F4914F6CDD1DULL /**< 乘法常数（来自 Marsaglia 论文） */

/* 双精度随机数生成参数 */
#define lv_DOUBLE_RAND_HI_BITS 53                  /**< 高位位数（double 尾数精度） */
#define lv_DOUBLE_RAND_LO_BITS 11                  /**< 低位位数（附加精度） */
#define lv_DOUBLE_RAND_MAX_SAFE 0.9999999999999999 /**< [0,1) 区间安全上界 */



void lv_random_init(uint64_t seed) {
    s_utils_state.random_state = seed ? seed : (uint64_t) time(NULL);
}

/* xorshift64* 伪随机数生成器（Marsaglia, 2003） */
static uint64_t xorshift64star(void) {
    s_utils_state.random_state ^= s_utils_state.random_state >> lv_XORSHIFT_SHIFT_A;
    s_utils_state.random_state ^= s_utils_state.random_state << lv_XORSHIFT_SHIFT_B;
    s_utils_state.random_state ^= s_utils_state.random_state >> lv_XORSHIFT_SHIFT_C;
    return s_utils_state.random_state * lv_XORSHIFT_MULTIPLIER;
}

int lv_random_int(int min, int max) {
    if (min >= max)
        return min;
    uint64_t range = (uint64_t) max - (uint64_t) min;
    /* 拒绝采样法：消除模偏差。
     * 当 range 不是 2^64 的约数时，xorshift64star() % range 会使较小值
     * 的出现概率略高于较大值。通过计算阈值并拒绝超出范围的采样值来保证均匀性。 */
    uint64_t threshold = UINT64_MAX - (UINT64_MAX % range);
    uint64_t r;
    do {
        r = xorshift64star();
    } while (r >= threshold);
    return min + (int) (r % range);
}

double lv_random_double(double min, double max) {
    if (min >= max)
        return min;
    uint64_t r = xorshift64star();
    /* 修复：使用双精度拆分法生成 [0.0, 1.0) 区间内的均匀随机数。
     *
     * 原实现使用 (double)UINT64_MAX + 1.0 作为除数，但 UINT64_MAX (2^64-1)
     * 转为 double 后精度丢失约 9 位，加 1.0 后这些位全部被吸收，除数实际等于
     * (double)UINT64_MAX ≈ 1.8446744e19，导致：
     *   - r==UINT64_MAX 时 normalized == 1.0，结果可能等于 max
     *   - 低 11 位的变化对 normalized 无影响，分布不均匀
     *
     * 修复方案：将 64 位随机数拆分为高 53 位（提供 double 的完整尾数精度）
     * 和低 11 位（作为附加精度），避免浮点转换时的精度丢失。 */
    uint64_t hi53 = r >> lv_DOUBLE_RAND_LO_BITS;              /* 高 53 位作为主尾数 */
    uint64_t lo11 = r & ((1u << lv_DOUBLE_RAND_LO_BITS) - 1); /* 低 11 位作为补充精度 */
    /* 构造 [0.0, 1.0) 的均匀随机数：
     *   normalized = hi53/2^53 + lo11/2^64
     * 使用 2^53 作为主除数（double 的 53 位尾数可精确表示），
     * 低 11 位作为微小扰动，确保所有 64 位都对结果有贡献。 */
    double normalized = (double) hi53 / 9007199254740992.0        /* 2^53 */
                        + (double) lo11 / 18446744073709551616.0; /* 2^64 */
    /* 钳制到 [0.0, 1.0) 以确保安全（理论上 normalized < 1.0，但浮点运算
     * 的舍入可能导致极微小的超出） */
    if (normalized >= 1.0)
        normalized = lv_DOUBLE_RAND_MAX_SAFE;
    return min + normalized * (max - min);
}

/* ============================================================
 * 哈希函数
 * ============================================================ */

uint64_t lv_hash_string(const char *str) {
    if (!str)
        return 0;

    /* FNV-1a 哈希算法（使用 lv_internal.h 中的统一定义） */
    uint64_t hash = lv_FNV64_OFFSET_BASIS;
    while (*str) {
        hash ^= (uint64_t) (unsigned char) *str++;
        hash *= lv_FNV64_PRIME;
    }
    return hash;
}

/* ============================================================
 * 日志函数（lv_internal.h 中宏调用的底层实现）
 * ============================================================ */

/* ============================================================
 * 日志系统（统一委托给 debug.h 的 debug_log()）
 *
 * lv_LOG_* 宏（定义在 lv_internal.h）通过 lv_log_message()
 * 委托到 debug.h 的 debug_log()，实现统一的日志管道。
 * 所有日志级别过滤、格式化、文件输出、环形缓冲区等
 * 均由 debug.c 中的 debug_log() 统一处理。
 * ============================================================ */

/**
 * @brief 将 lv_LOG_LEVEL_* 映射为 debug.h 的 LogLevel 枚举
 *
 * 注意：两套级别的语义是相反的：
 *   - lv_LOG_LEVEL_*：数值越大越详细（DEBUG=4 > ERROR=1）
 *   - LogLevel：数值越大越严重（FATAL=4 > TRACE=-1）
 */
static const LogLevel kLogLevelMap[] = {
    [lv_LOG_LEVEL_ERROR]   = LOG_LEVEL_ERROR,
    [lv_LOG_LEVEL_WARNING] = LOG_LEVEL_WARN,
    [lv_LOG_LEVEL_INFO]    = LOG_LEVEL_INFO,
    [lv_LOG_LEVEL_DEBUG]   = LOG_LEVEL_DEBUG,
};

static LogLevel lv_log_map_level(int level) {
    if (level >= 0 && level < (int)(sizeof(kLogLevelMap)/sizeof(kLogLevelMap[0])))
        return kLogLevelMap[level];
    return LOG_LEVEL_INFO;
}

/**
 * @brief 输出日志消息 —— 委托给 debug_log()
 *
 * 由 lv_LOG_INFO / lv_LOG_WARNING / lv_LOG_ERROR / lv_LOG_DEBUG
 * 系列宏间接调用。所有日志最终通过 debug_log() 统一管道输出，
 * 享受线程安全、日志轮转、环形缓冲区和编译期过滤等功能。
 *
 * @param level 日志级别（lv_LOG_LEVEL_*）
 * @param file  源文件名（__FILE__）
 * @param line  源文件行号（__LINE__）
 * @param fmt   printf 风格格式字符串
 * @param ...   可变参数
 */
void lv_log_message(int level, const char *file, int line, const char *fmt, ...) {
    /* 提取模块名（文件名不含路径作为模块标识） */
    const char *module = "unknown";
    if (file) {
        const char *base = strrchr(file, '/');
        if (!base) base = strrchr(file, '\\');
        module = base ? base + 1 : file;
    }

    /* 格式化消息并附加 [file:line] 前缀以保留调用位置信息 */
    char buf[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    debug_log(lv_log_map_level(level), module, "[%s:%d] %s", file ? file : "?", line, buf);
}

uint64_t lv_hash_bytes(const void *data, size_t len) {
    if (!data || len == 0)
        return 0;

    const uint8_t *bytes = (const uint8_t *) data;
    uint64_t hash = lv_FNV64_OFFSET_BASIS;

    for (size_t i = 0; i < len; i++) {
        hash ^= (uint64_t) bytes[i];
        hash *= lv_FNV64_PRIME;
    }
    return hash;
}

uint64_t lv_hash_int(int value) {
    /* 使用 FNV-1a 哈希（使用 lv_internal.h 中的统一定义） */
    uint64_t hash = lv_FNV64_OFFSET_BASIS;
    /* 逐字节哈希 int 值（sizeof(int) 通常为 4） */
    for (size_t i = 0; i < sizeof(int); i++) {
        hash ^= (uint64_t) ((value >> (i * 8)) & 0xFF);
        hash *= lv_FNV64_PRIME;
    }
    return hash;
}

/* ============================================================
 * 统一数组扩容函数
 * ============================================================ */

/**
 * @brief 确保动态数组有足够的容量
 * @param arr 当前数组指针（可能被 realloc）
 * @param count 当前元素数量
 * @param capacity 当前容量指针（会被更新）
 * @param elem_size 每个元素的大小
 * @param min_growth 最小增长量
 * @return 成功返回 true，失败返回 false
 * @note 使用 lv_ARRAY_GROWTH_FACTOR 倍增策略
 */
bool lv_ensure_capacity(void **arr, int count, int *capacity, size_t elem_size, int min_growth) {
    if (!arr || !capacity || elem_size == 0)
        lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_PARAM, "ensure_capacity 参数无效");

    /* 无需扩容 */
    if (count < *capacity)
        return true;

    /* 溢出检查 */
    if (count < 0 || *capacity < 0)
        lv_RETURN_ERROR_BOOL(lv_ERROR_OVERFLOW, "ensure_capacity count/capacity 为负");

    /* 计算最小需求容量 */
    int min_required = count + min_growth;
    if (min_required < count) /* 溢出检测 */
        lv_RETURN_ERROR_BOOL(lv_ERROR_OVERFLOW, "ensure_capacity min_required 溢出");

    /* 计算新容量 */
    if (*capacity > INT_MAX / lv_ARRAY_GROWTH_FACTOR)
        lv_RETURN_ERROR_BOOL(lv_ERROR_OVERFLOW, "ensure_capacity 容量溢出");
    int new_cap = (*capacity == 0) ? lv_INITIAL_ARRAY_CAPACITY : *capacity * lv_ARRAY_GROWTH_FACTOR;
    if (new_cap < min_required) {
        if (min_required > INT_MAX / lv_ARRAY_GROWTH_FACTOR)
            lv_RETURN_ERROR_BOOL(lv_ERROR_OVERFLOW, "ensure_capacity min_required 容量溢出");
        new_cap = min_required * lv_ARRAY_GROWTH_FACTOR;
    }

    /* 分配前检查 size_t 溢出 */
    if ((size_t) new_cap > SIZE_MAX / elem_size)
        lv_RETURN_ERROR_BOOL(lv_ERROR_OVERFLOW, "ensure_capacity size_t 溢出");

    void *new_arr = lv_realloc(*arr, (size_t) new_cap * elem_size);
    if (!new_arr)
        lv_RETURN_ERROR_BOOL(lv_ERROR_ALLOCATION_FAILED, "ensure_capacity realloc 失败");

    *arr = new_arr;
    *capacity = new_cap;
    return true;
}

/* ============================================================
 * 统一 FNV-1a 哈希函数
 * ============================================================ */

/**
 * @brief FNV-1a 哈希函数
 * @param data 输入数据
 * @param len 数据长度
 * @return 64位哈希值
 */
uint64_t lv_fnv1a_hash(const void *data, size_t len) {
    if (!data || len == 0)
        return 0;
    const uint8_t *p = (const uint8_t *) data;
    uint64_t hash = lv_FNV64_OFFSET_BASIS;
    for (size_t i = 0; i < len; i++) {
        hash ^= p[i];
        hash *= lv_FNV64_PRIME;
    }
    return hash;
}

uint64_t lv_fnv1a_update(uint64_t hash, const void *data, size_t len) {
    if (!data || len == 0)
        return hash;
    const uint8_t *p = (const uint8_t *) data;
    for (size_t i = 0; i < len; i++) {
        hash ^= p[i];
        hash *= lv_FNV64_PRIME;
    }
    return hash;
}

uint64_t lv_fnv1a_hash_str(const char *s) {
    if (!s)
        return lv_FNV64_OFFSET_BASIS;
    return lv_fnv1a_update(lv_FNV64_OFFSET_BASIS, s, strlen(s));
}

uint64_t lv_fnv1a_hash_int(uint64_t hash, uint64_t v) {
    return lv_fnv1a_update(hash, &v, sizeof(v));
}

/* ============================================================
 * 线程局部临时缓冲区（scratch）
 * ============================================================ */

/** 线程局部 scratch 缓冲区（按需增长，最小分配 256 字节） */
static lv_THREAD_LOCAL char *s_lv_scratch_buf = NULL;
static lv_THREAD_LOCAL size_t s_lv_scratch_cap = 0;

char *lv_scratch_buf(size_t min_size) {
    if (min_size < 256)
        min_size = 256;
    if (s_lv_scratch_cap < min_size) {
        char *nb = (char *) lv_realloc(s_lv_scratch_buf, min_size);
        if (!nb)
            return s_lv_scratch_buf; /* 分配失败：返回旧缓冲区（尽力而为） */
        s_lv_scratch_buf = nb;
        s_lv_scratch_cap = min_size;
    }
    return s_lv_scratch_buf;
}

char *lv_fmt_tmp(const char *fmt, ...) {
    if (!fmt)
        return NULL;

    va_list args;
    va_start(args, fmt);
    va_list copy;
    va_copy(copy, args);
    int need = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (need < 0) {
        va_end(args);
        return NULL;
    }

    char *buf = lv_scratch_buf((size_t) need + 1);
    if (s_lv_scratch_cap < (size_t) need + 1) {
        va_end(args);
        return NULL; /* 扩容失败，无法容纳结果 */
    }
    vsnprintf(buf, (size_t) need + 1, fmt, args);
    va_end(args);
    return buf;
}

/* ============================================================
 * 资源追踪器实现
 * ============================================================ */

/**
 * @brief 被追踪的资源节点
 *
 * 双向链表节点，存储资源指针、名称和销毁回调函数。
 * 后进先出（LIFO）顺序销毁，确保依赖关系正确（后分配的先释放）。
 */
typedef struct TrackedResource {
    void *resource;                /**< 资源指针（文件句柄、内存、锁等） */
    lvResourceDestroyFunc destroy; /**< 资源销毁回调 */
    char *name;                    /**< 资源名称（用于调试），可为 NULL */
    struct TrackedResource *prev;  /**< 前驱节点 */
    struct TrackedResource *next;  /**< 后继节点 */
} TrackedResource;

/**
 * @brief 资源追踪器
 */
struct ResourceTracker {
    TrackedResource *head; /**< 链表头（最早注册的资源） */
    TrackedResource *tail; /**< 链表尾（最近注册的资源） */
    int count;             /**< 当前追踪的资源数量 */
};

ResourceTracker *lv_resource_tracker_create(void) {
    ResourceTracker *rt = (ResourceTracker *) lv_calloc(1, sizeof(ResourceTracker));
    return rt; /* calloc 已将 head/tail/count 置零 */
}

void lv_resource_tracker_destroy(ResourceTracker **rt) {
    if (!rt || !*rt)
        return;

    /* 仅释放追踪器自身和节点，不调用销毁回调 */
    /* 注意：先调用 cleanup 再调用此函数才安全 */
    TrackedResource *node = (*rt)->head;
    while (node) {
        TrackedResource *next = node->next;
        lv_free((void **) &node->name);
        lv_free((void **) &node);
        node = next;
    }

    lv_free((void **) &*rt);
    *rt = NULL;
}

bool lv_resource_track(ResourceTracker *rt, void *resource, lvResourceDestroyFunc destroy, const char *name) {
    if (!rt || !resource || !destroy)
        return false;

    TrackedResource *node = (TrackedResource *) lv_calloc(1, sizeof(TrackedResource));
    if (!node)
        return false;

    node->resource = resource;
    node->destroy = destroy;

    /* 复制名称（若有） */
    if (name) {
        node->name = (char *) lv_malloc(strlen(name) + 1);
        if (node->name) {
            /* [Bug修复] strcpy → lv_strlcpy 防止缓冲区溢出 */
            lv_strlcpy(node->name, name, strlen(name) + 1);
        }
    }

    /* 追加到双向链表尾部 */
    if (rt->tail) {
        rt->tail->next = node;
        node->prev = rt->tail;
        rt->tail = node;
    } else {
        rt->head = rt->tail = node;
    }

    rt->count++;
    return true;
}

bool lv_resource_untrack(ResourceTracker *rt, void *resource) {
    if (!rt || !resource)
        lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_PARAM, "resource_untrack 参数无效");

    TrackedResource *node = rt->head;
    while (node) {
        if (node->resource == resource) {
            /* 从双向链表中移除 */
            if (node->prev)
                node->prev->next = node->next;
            else
                rt->head = node->next;

            if (node->next)
                node->next->prev = node->prev;
            else
                rt->tail = node->prev;

            lv_free((void **) &node->name);
            lv_free((void **) &node);
            rt->count--;
            return true;
        }
        node = node->next;
    }

    lv_RETURN_ERROR_BOOL(lv_ERROR_NOT_FOUND, "resource_untrack 未找到 resource");
}

void lv_resource_tracker_cleanup(ResourceTracker *rt) {
    if (!rt)
        return;

    /* 从链表尾部开始逆序销毁（后注册的先销毁） */
    TrackedResource *node = rt->tail;
    while (node) {
        TrackedResource *prev = node->prev;

        /* 调用销毁回调 */
        if (node->resource && node->destroy) {
            lv_LOG_DEBUG("资源追踪器清理: %s (0x%p)", node->name ? node->name : "<未命名>", node->resource);
            node->destroy(node->resource);
        }

        lv_free((void **) &node->name);
        lv_free((void **) &node);
        node = prev;
    }

    rt->head = NULL;
    rt->tail = NULL;
    rt->count = 0;
}

int lv_resource_tracker_count(const ResourceTracker *rt) {
    if (!rt)
        return 0;
    return rt->count;
}

/* ============================================================
 * FFI 兼容接口
 * ============================================================ */

/**
 * @brief FFI 兼容的内存释放函数
 *
 * 专为外部函数接口（Python ctypes、JNI 等）设计。
 * 与 lv_free((void **) &void**) 不同，此函数接受标准的 void* 参数，
 * 语义与标准 C 的 lv_free() 一致，但不执行指针置 NULL 操作。
 *
 * 适用场景：
 *   - Python ctypes 调用：ctypes 无法方便地传递双重指针
 *   - 其他 FFI 绑定：需要标准 lv_free((void **) &void*) 语义的语言绑定
 *
 * @param ptr 要释放的内存指针，允许为 NULL（安全无操作）
 *
 * @note 对于 C 内部代码，应继续使用 lv_free((void **) &void**) 以获得
 *       自动置 NULL 的安全保证。此函数仅用于 FFI 边界。
 */
void lv_free_ptr(void *ptr) {
    if (!ptr)
        return;

    lv_allocator_get()->free(ptr);
}

/* ============================================================
 * 动态字符串（lv_dstr）
 * ============================================================ */

/**
 * @brief 初始化动态字符串构建器
 * @param d   字符串构建器指针
 * @param cap 初始容量
 * @return 0 成功，-1 内存分配失败
 */
int lv_dstr_init(lvDStr *d, size_t cap) {
    d->data = (char *) lv_malloc(cap);
    if (!d->data)
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "dstr_init malloc 失败");
    d->data[0] = '\0';
    d->len = 0;
    d->cap = cap;
    return 0;
}

/**
 * @brief 确保缓冲区有足够的空间容纳额外内容
 * @param d     字符串构建器指针
 * @param extra 需要的额外字节数
 * @return 0 成功，-1 内存分配失败
 */
int lv_dstr_grow(lvDStr *d, size_t extra) {
    size_t needed = d->len + extra + 1;
    if (needed <= d->cap)
        return 0;
    size_t new_cap = d->cap * 2;
    while (new_cap < needed)
        new_cap *= 2;
    char *nd = (char *) lv_realloc(d->data, new_cap);
    if (!nd)
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "dstr_grow realloc 失败");
    d->data = nd;
    d->cap = new_cap;
    return 0;
}

/**
 * @brief 向动态字符串追加格式化内容（printf 风格）
 * @param d   字符串构建器指针
 * @param fmt printf 格式字符串
 * @param ... 格式化参数
 * @return 0 成功，-1 失败
 */
int lv_dstr_append_fmt(lvDStr *d, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int needed = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (needed < 0)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "dstr_append_fmt vsnprintf 失败");
    if (lv_dstr_grow(d, (size_t) needed) != 0)
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "dstr_append_fmt grow 失败");
    va_start(ap, fmt);
    vsnprintf(d->data + d->len, d->cap - d->len, fmt, ap);
    va_end(ap);
    d->len += (size_t) needed;
    return 0;
}

/**
 * @brief 向动态字符串追加原始字节数据
 * @param d 字符串构建器指针
 * @param s 数据源指针
 * @param n 字节数
 * @return 0 成功，-1 失败
 */
int lv_dstr_append_raw(lvDStr *d, const char *s, size_t n) {
    if (!s || n == 0)
        return 0;
    if (lv_dstr_grow(d, n) != 0)
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "dstr_append_raw grow 失败");
    memcpy(d->data + d->len, s, n);
    d->len += n;
    d->data[d->len] = '\0';
    return 0;
}

/**
 * @brief 向动态字符串追加 C 字符串
 * @param d 字符串构建器指针
 * @param s 要追加的字符串（可为 NULL）
 * @return 0 成功，-1 失败
 */
int lv_dstr_append_str(lvDStr *d, const char *s) {
    if (!s)
        return 0;
    return lv_dstr_append_raw(d, s, strlen(s));
}

/**
 * @brief 释放动态字符串构建器的内部缓冲区
 * @param d 字符串构建器指针
 */
void lv_dstr_free(lvDStr *d) {
    if (d->data) {
        lv_free((void **) &(d->data));
        d->data = NULL;
    }
    d->len = 0;
    d->cap = 0;
}

/* ============================================================
 * lvDArray —— 泛型动态数组容器
 * ============================================================ */

void lv_darray_init(lvDArray *arr, size_t elem_size) {
    arr->data = NULL;
    arr->count = 0;
    arr->capacity = 0;
    arr->elem_size = elem_size;
}

void lv_darray_free(lvDArray *arr) {
    if (arr->data) {
        lv_free((void **) &(arr->data));
        arr->data = NULL;
    }
    arr->count = 0;
    arr->capacity = 0;
    arr->elem_size = 0;
}

bool lv_darray_reserve(lvDArray *arr, int count) {
    if (count <= arr->capacity)
        return true;
    int target = count;
    return lv_ensure_capacity(&arr->data, target, &arr->capacity, arr->elem_size, 0);
}

int lv_darray_push(lvDArray *arr, const void *elem) {
    if (!lv_darray_reserve(arr, arr->count + 1))
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "darray_push reserve 失败");
    char *ptr = (char *)arr->data + (size_t)arr->count * arr->elem_size;
    memcpy(ptr, elem, arr->elem_size);
    return arr->count++;
}

void lv_darray_pop(lvDArray *arr) {
    if (arr->count > 0)
        arr->count--;
}

void *lv_darray_get(const lvDArray *arr, int index) {
    if (index < 0 || index >= arr->count || !arr->data)
        return NULL;
    return (char *)arr->data + (size_t)index * arr->elem_size;
}

void lv_darray_clear(lvDArray *arr) {
    arr->count = 0;
}

/* ============================================================
 * 数值数组聚合
 * ============================================================ */

double lv_max_abs(const double *arr, int64_t n) {
    if (!arr || n <= 0)
        return 0.0;
    double max_val = fabs(arr[0]);
    for (int64_t i = 1; i < n; ++i) {
        double abs_val = fabs(arr[i]);
        if (abs_val > max_val)
            max_val = abs_val;
    }
    return max_val;
}

double lv_max_d(const double *arr, int64_t n) {
    if (!arr || n <= 0)
        return 0.0;
    double max_val = arr[0];
    for (int64_t i = 1; i < n; ++i) {
        if (arr[i] > max_val)
            max_val = arr[i];
    }
    return max_val;
}

/* ============================================================
 * 字节序工具（显式大端/小端读写）
 * ============================================================ */

void lv_store_be16(uint8_t *dst, uint16_t v) {
    dst[0] = (uint8_t) ((v >> 8) & 0xFF);
    dst[1] = (uint8_t) (v & 0xFF);
}

void lv_store_be32(uint8_t *dst, uint32_t v) {
    dst[0] = (uint8_t) ((v >> 24) & 0xFF);
    dst[1] = (uint8_t) ((v >> 16) & 0xFF);
    dst[2] = (uint8_t) ((v >> 8) & 0xFF);
    dst[3] = (uint8_t) (v & 0xFF);
}

uint16_t lv_load_be16(const uint8_t *src) {
    return (uint16_t) ((((uint16_t) src[0]) << 8) | (uint16_t) src[1]);
}

uint32_t lv_load_be32(const uint8_t *src) {
    return ((uint32_t) src[0] << 24) | ((uint32_t) src[1] << 16) | ((uint32_t) src[2] << 8) | (uint32_t) src[3];
}

void lv_store_le16(uint8_t *dst, uint16_t v) {
    dst[0] = (uint8_t) (v & 0xFF);
    dst[1] = (uint8_t) ((v >> 8) & 0xFF);
}

void lv_store_le32(uint8_t *dst, uint32_t v) {
    dst[0] = (uint8_t) (v & 0xFF);
    dst[1] = (uint8_t) ((v >> 8) & 0xFF);
    dst[2] = (uint8_t) ((v >> 16) & 0xFF);
    dst[3] = (uint8_t) ((v >> 24) & 0xFF);
}

uint16_t lv_load_le16(const uint8_t *src) {
    return (uint16_t) ((uint16_t) src[0] | (((uint16_t) src[1]) << 8));
}

uint32_t lv_load_le32(const uint8_t *src) {
    return ((uint32_t) src[0]) | ((uint32_t) src[1] << 8) | ((uint32_t) src[2] << 16) | ((uint32_t) src[3] << 24);
}

void lv_store_be64(uint8_t *dst, uint64_t v) {
    for (int i = 0; i < 8; i++) {
        dst[i] = (uint8_t) ((v >> (56 - i * 8)) & 0xFF);
    }
}

uint64_t lv_load_be64(const uint8_t *src) {
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) {
        v = (v << 8) | src[i];
    }
    return v;
}

void lv_store_le64(uint8_t *dst, uint64_t v) {
    for (int i = 0; i < 8; i++) {
        dst[i] = (uint8_t) ((v >> (i * 8)) & 0xFF);
    }
}

uint64_t lv_load_le64(const uint8_t *src) {
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) {
        v |= ((uint64_t) src[i]) << (i * 8);
    }
    return v;
}

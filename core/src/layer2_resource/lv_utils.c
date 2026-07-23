/**
 * @file lv_utils.c
 * @brief Lv-00 工具函数库实现
 *
 * 提供内存管理、字符串处理、数组操作、配置管理等通用功能。
 *
 * ================================================================
 * 内存管理规范 (Memory Management Conventions)
 * ================================================================
 *
 * 本项目所有动态内存分配必须使用本模块提供的封装函数：
 *   - lv_malloc / lv_calloc / lv_realloc / lv_free
 *
 * **关键安全规则：**
 *
 * 1. lv_realloc 与标准 realloc 的关键差异：
 *    - 当 size==0 时，lv_realloc 返回 NULL 但**不释放原内存**。
 *      这是与标准 C 库 lv_realloc(p, 0) 的重要区别（C11 标准中 lv_realloc(p, 0)
 *      行为由实现定义）。调用者必须显式使用 lv_free((void **) &ptr) 来释放内存，
 *      不应依赖 lv_realloc(ptr, 0) 来释放。
 *    - **调用者必须将返回值赋给原指针变量**，否则在 realloc 移动内存块后
 *      原指针将成为悬空指针。
 *
 * 2. lv_free 使用 void** 参数：
 *    - lv_free 接受 void** 而非 void*，释放后自动将调用者的指针置为 NULL，
 *      有效防止 use-after-free 和 double-free。
 *    - 必须传递指针的地址：lv_free((void **)&ptr)，必须写作 lv_free((void **) &ptr)。
 *
 * 3. 内存所有权规则：
 *    - 创建函数（如 rune_create_*）返回新分配的内存，调用者拥有所有权。
 *    - 当对象被添加到容器（如 RuneSequence）时，所有权转移给容器。
 *    - 销毁容器时会递归释放所有包含的元素。
 *
 * 4. 线程安全：
 *    - 内存统计 (MemoryStats) 使用 lv_THREAD_LOCAL 存储，每个线程独立统计。
 *    - 内存限制 (g_memory_limit) 同样是线程局部变量。
 *    - 多线程环境下的跨线程内存操作需调用者自行同步。
 */

#include "lv_utils.h"

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* [Bug修复] 为 realloc 非本分配器路径获取旧大小所需的平台头文件 */
#ifdef _WIN32
#include <malloc.h>  /* _msize */
#elif defined(__linux__)
#include <malloc.h>  /* malloc_usable_size */
#elif defined(__APPLE__)
#include <malloc/malloc.h>  /* malloc_size on macOS */
#endif

#include "error_codes.h"
#include "lv.h"
#include "lv_internal.h"

/* ============================================================
 * 内存统计跟踪
 * ============================================================ */

static lv_THREAD_LOCAL MemoryStats g_memory_stats = {0};
static lv_THREAD_LOCAL size_t g_memory_limit = 0;
static lv_THREAD_LOCAL bool g_poison_enabled = false; /**< 毒模式填充开关，默认关闭（v3.3.0调试阶段） */

/**
 * @brief 内部分配头 —— 存储每次分配的元数据
 *
 * 每个通过 lv_malloc / lv_calloc 分配的内存块前附加此头部，
 * 使得 lv_free 和 lv_realloc 可以精确获取原始分配大小，
 * 从而正确维护 current_used 等内存统计指标。
 *
 * 内存布局（实际分配 = 头部 + 用户数据 + 尾部魔数）：
 *   [AllocHeader | 用户数据 (size 字节) | 尾部魔数 (4 字节)]
 *
 * 头部魔数 (head_magic) 用于检测 double-free 和内存损坏；
 * 尾部魔数 (写于 data[size] 位置) 用于检测缓冲区溢出。
 *
 * 对外返回的指针指向 data[] 起始位置，调用者不可感知此头部。
 * 此设计不会与外部 lv_free() 兼容（外部调用者必须使用 lv_free /
 * lv_free_ptr），但项目规范要求所有动态内存通过本模块管理，
 * 因此这是安全的。
 */
typedef struct AllocHeader {
    uint32_t head_magic;            /**< 头部魔数，检测内存损坏和 double-free */
    uint32_t tail_offset;           /**< 尾部魔数相对于 data 的偏移（字节）= size */
    size_t size;                    /**< 用户请求的分配大小（字节） */
    const char *file;               /**< 分配源文件名（NULL 表示未记录） */
    int line;                       /**< 分配源行号（0 表示未记录） */
    struct AllocHeader *track_next; /**< 全局追踪链表中的下一个节点 */
    char data[];                    /**< 柔性数组成员：实际数据起始位置 */
} AllocHeader;

#define ALLOC_HEAD_MAGIC  0xADBEEF01  /**< 头部魔数（存活标记） */
#define ALLOC_TAIL_MAGIC  0xADBEEF02  /**< 尾部魔数（缓冲区溢出检测） */
#define ALLOC_POISON      0xDEADBEEF  /**< 毒模式（use-after-free 检测） */
#define ALLOC_MAGIC_FREED 0xDEADDEAD  /**< 已释放标记（double-free 检测） */
#define ALLOC_HEADER_SIZE offsetof(AllocHeader, data) /**< 头部大小（不含 data） */

#define ALLOC_TAIL_MAGIC_SIZE sizeof(uint32_t)        /**< 尾部魔数大小 */

/** 全局追踪链表头 —— 记录所有未释放的分配 */
static lv_THREAD_LOCAL AllocHeader *g_tracked_allocs = NULL;

/**
 * @brief 从用户指针获取分配头
 * @param ptr 用户指针（即 lv_malloc 等返回的 data[] 地址）
 * @return 对应的 AllocHeader 指针；若头部或尾部魔数不匹配则返回 NULL
 */
static AllocHeader *get_header(void *ptr) {
    if (!ptr)
        return NULL;
    AllocHeader *hdr = (AllocHeader *)((char *)ptr - ALLOC_HEADER_SIZE);
    /* 检查头部魔数 */
    if (hdr->head_magic != ALLOC_HEAD_MAGIC)
        return NULL;
    /* 检查尾部魔数（缓冲区溢出检测）—— 使用 memcpy 避免未对齐访问 */
    if (hdr->tail_offset > 0) {
        uint32_t tail_value;
        memcpy(&tail_value, (char *)ptr + hdr->tail_offset, sizeof(uint32_t));
        if (tail_value != ALLOC_TAIL_MAGIC) {
            /* 尾部魔数不匹配 —— 可能发生了缓冲区溢出 */
            lv_LOG_ERROR("内存损坏检测: 尾部魔数不匹配，指针=0x%p, 期望=0x%08X, 实际=0x%08X",
                           ptr, ALLOC_TAIL_MAGIC, tail_value);
            return NULL;
        }
    }
    return hdr;
}

/**
 * @brief 将分配加入全局追踪链表
 * @param hdr 分配头指针
 */
static void track_allocation(AllocHeader *hdr) {
    hdr->track_next = g_tracked_allocs;
    g_tracked_allocs = hdr;
}

/**
 * @brief 从全局追踪链表中移除分配
 * @param hdr 分配头指针
 * @return true 成功移除，false 未找到
 */
static bool untrack_allocation(AllocHeader *hdr) {
    AllocHeader **curr = &g_tracked_allocs;
    while (*curr) {
        if (*curr == hdr) {
            *curr = hdr->track_next;
            hdr->track_next = NULL;
            return true;
        }
        curr = &(*curr)->track_next;
    }
    return false;
}

/**
 * @brief 用毒模式填充已释放的内存区域
 *
 * 将指定内存区域的每个 32 位字填充为 ALLOC_POISON (0xDEADBEEF)。
 * 之后任何对该区域的读操作都会发现毒模式值，从而检测 use-after-free。
 *
 * @param data 数据区域起始地址
 * @param size 数据区域大小（字节）
 */
static void fill_poison(void *data, size_t size) {
    if (!g_poison_enabled || !data || size == 0)
        return;
    /* 使用 memcpy 避免未对齐访问 —— data 可能非 4 字节对齐 */
    uint32_t poison_val = ALLOC_POISON;
    uint8_t *p = (uint8_t *)data;
    size_t count = size / sizeof(uint32_t);
    for (size_t i = 0; i < count; i++) {
        memcpy(p + i * sizeof(uint32_t), &poison_val, sizeof(uint32_t));
    }
    /* 填充剩余不足 4 字节的尾部 */
    size_t remaining = size % sizeof(uint32_t);
    if (remaining > 0) {
        uint8_t *tail = (uint8_t *)(p + count);
        memset(tail, 0xBE, remaining);
    }
}

void *lv_malloc(size_t size) {
    /* 向后兼容：委托给 tracked 版本，file/line 为 NULL/0 */
    return lv_malloc_tracked(size, NULL, 0);
}

void *lv_malloc_tracked(size_t size, const char *file, int line) {
    /* 零大小请求：分配最小块（1 字节数据 + 尾魔数），保持 lv_malloc(0) 语义 */
    size_t alloc_size = size ? size : 1;

    if (g_memory_limit > 0 && g_memory_stats.current_used > g_memory_limit - alloc_size) {
        lv_set_error(lv_ERROR_OUT_OF_MEMORY, "内存限制超出: 请求%zu", alloc_size);
        return NULL;
    }

    /* 检查溢出：头部大小 + 用户请求大小 + 尾部魔数大小 */
    AllocHeader *hdr = NULL;
    size_t total = ALLOC_HEADER_SIZE;
    if (total > SIZE_MAX - alloc_size ||
        total + alloc_size > SIZE_MAX - ALLOC_TAIL_MAGIC_SIZE) {
        lv_set_error(lv_ERROR_OVERFLOW, "malloc 溢出: header=%zu + size=%zu + tail=%zu",
                       (size_t)ALLOC_HEADER_SIZE, alloc_size, (size_t)ALLOC_TAIL_MAGIC_SIZE);
        return NULL;
    }
    total += alloc_size;
    total += ALLOC_TAIL_MAGIC_SIZE;

    hdr = (AllocHeader *)malloc(total);
    if (!hdr)
        return NULL;

    hdr->head_magic = ALLOC_HEAD_MAGIC;
    hdr->tail_offset = (uint32_t)alloc_size;
    hdr->size = alloc_size;
    hdr->file = file;
    hdr->line = line;

    /* 设置尾部魔数（使用 memcpy 避免未对齐访问） */
    uint32_t tail_magic = ALLOC_TAIL_MAGIC;
    memcpy(hdr->data + alloc_size, &tail_magic, sizeof(uint32_t));

    /* 加入全局追踪链表 */
    track_allocation(hdr);

    g_memory_stats.total_allocated += alloc_size;
    g_memory_stats.current_used += alloc_size;
    g_memory_stats.allocation_count++;
    if (g_memory_stats.current_used > g_memory_stats.peak_used)
        g_memory_stats.peak_used = g_memory_stats.current_used;

    return hdr->data;
}

void *lv_calloc(size_t nmemb, size_t size) {
    return lv_calloc_tracked(nmemb, size, NULL, 0);
}

void *lv_calloc_tracked(size_t nmemb, size_t size, const char *file, int line) {
    /* 零大小请求：分配最小块（1 字节数据），保持与 lv_malloc(0) 一致的语义 */
    if (nmemb == 0 || size == 0) {
        nmemb = 1;
        size = 1;
    }

    /* 检查溢出 */
    if (nmemb > SIZE_MAX / size) {
        lv_set_error(lv_ERROR_OVERFLOW, "calloc 溢出: %zu * %zu", nmemb, size);
        return NULL;
    }

    size_t total = nmemb * size;

    /* 检查头部和尾部大小溢出 */
    {
        size_t full = ALLOC_HEADER_SIZE;
        if (full > SIZE_MAX - total) goto overflow;
        full += total;
        if (full > SIZE_MAX - ALLOC_TAIL_MAGIC_SIZE) goto overflow;
        full += ALLOC_TAIL_MAGIC_SIZE;

        AllocHeader *hdr = (AllocHeader *)malloc(full);
        if (!hdr)
            return NULL;
        memset(hdr, 0, full);

        hdr->head_magic = ALLOC_HEAD_MAGIC;
        hdr->tail_offset = (uint32_t)total;
        hdr->size = total;
        hdr->file = file;
        hdr->line = line;

        /* 设置尾部魔数（使用 memcpy 避免未对齐访问） */
        uint32_t ctail_magic = ALLOC_TAIL_MAGIC;
        memcpy(hdr->data + total, &ctail_magic, sizeof(uint32_t));

        track_allocation(hdr);

        g_memory_stats.total_allocated += total;
        g_memory_stats.current_used += total;
        g_memory_stats.allocation_count++;
        if (g_memory_stats.current_used > g_memory_stats.peak_used)
            g_memory_stats.peak_used = g_memory_stats.current_used;

        return hdr->data;
    }

overflow:
    lv_set_error(lv_ERROR_OVERFLOW, "calloc 溢出: header=%zu + total=%zu + tail=%zu",
                   (size_t)ALLOC_HEADER_SIZE, total, (size_t)ALLOC_TAIL_MAGIC_SIZE);
    return NULL;
}

/**
 * @brief 重新分配内存（统一内存追踪版本）
 *
 * 行为与标准 realloc 的关键差异：
 * - 当 size 为 0 时，返回 NULL 但不释放原内存。
 *   调用者应先 lv_free((void **) &ptr) 再处理 size=0 的情况。
 * - 自动维护 current_used 统计：减去旧大小，加上新大小。
 * - 若原指针不由 lv_malloc/lv_calloc 分配（魔数不匹配），
 *   则委托给 lv_malloc（保守处理：无法获取旧大小）。
 */
void *lv_realloc(void *ptr, size_t size) {
    if (!ptr)
        return lv_malloc(size);
    if (size == 0)
        return NULL;

    size_t alloc_size = size;
    AllocHeader *old_hdr = get_header(ptr);

    if (old_hdr) {
        /* 正规路径：由本分配器分配的指针，可以精确追踪 */
        size_t old_size = old_hdr->size;

        /* 计算新总大小 */
        size_t new_total = ALLOC_HEADER_SIZE;
        if (new_total > SIZE_MAX - alloc_size) goto realloc_overflow;
        new_total += alloc_size;
        if (new_total > SIZE_MAX - ALLOC_TAIL_MAGIC_SIZE) goto realloc_overflow;
        new_total += ALLOC_TAIL_MAGIC_SIZE;

        /* 从追踪链表中移除旧节点 */
        untrack_allocation(old_hdr);

        AllocHeader *new_hdr = (AllocHeader *)realloc(old_hdr, new_total);
        if (!new_hdr) {
            /* realloc 失败：旧分配仍然有效，重新加入追踪链表 */
            track_allocation(old_hdr);
            return NULL;
        }

        new_hdr->head_magic = ALLOC_HEAD_MAGIC;
        new_hdr->tail_offset = (uint32_t)alloc_size;
        new_hdr->size = alloc_size;

        /* 设置尾部魔数 */
        uint32_t tail_magic = ALLOC_TAIL_MAGIC;
        memcpy(new_hdr->data + alloc_size, &tail_magic, sizeof(uint32_t));

        /* 重新加入追踪链表 */
        track_allocation(new_hdr);

        /* 更新统计：减去旧大小，加上新大小 */
        g_memory_stats.total_allocated += alloc_size;
        if (old_size <= g_memory_stats.current_used) {
            g_memory_stats.current_used = g_memory_stats.current_used - old_size + alloc_size;
        } else {
            g_memory_stats.current_used += alloc_size;
        }
        if (g_memory_stats.current_used > g_memory_stats.peak_used)
            g_memory_stats.peak_used = g_memory_stats.current_used;

        return new_hdr->data;
    } else {
        /* 非本分配器分配的指针（魔数不匹配）：
         * 尝试获取旧分配大小并复制数据，以避免数据丢失。
         * 使用平台特定 API 获取旧块大小：_msize (Windows) / malloc_usable_size (Linux/macOS)。
         * [Bug修复] 原代码未复制旧数据，导致 realloc 语义不正确。 */
        void *new_ptr = lv_malloc(alloc_size);
        if (!new_ptr)
            return NULL;

        /* 尝试获取旧分配的实际可用大小 */
        size_t old_usable_size = 0;
#ifdef _WIN32
        old_usable_size = (size_t)_msize(ptr);
#elif defined(__APPLE__)
        old_usable_size = (size_t)malloc_size(ptr);
#elif defined(__linux__)
        old_usable_size = (size_t)malloc_usable_size(ptr);
#endif
        if (old_usable_size > 0) {
            /* 复制 min(alloc_size, old_usable_size) 字节，确保不越界 */
            size_t copy_size = (alloc_size < old_usable_size) ? alloc_size : old_usable_size;
            memcpy(new_ptr, ptr, copy_size);
        }
        /* 注意：若平台不支持获取旧大小（old_usable_size == 0），
         * 则不复制旧数据。调用者应尽量使用 lv_malloc/lv_free 配对。 */

        /* 释放旧指针（由 raw malloc/calloc 分配，用 raw free 释放） */
        free(ptr);

        return new_ptr;
    }

realloc_overflow:
    lv_set_error(lv_ERROR_OVERFLOW, "realloc 溢出");
    return NULL;
}

void lv_free(void **ptr) {
    if (!ptr || !*ptr)
        return;

    AllocHeader *hdr = get_header(*ptr);
    if (hdr) {
        /* 正规路径：由本分配器分配的指针 */
        size_t freed_size = hdr->size;

        /* 从追踪链表中移除 */
        untrack_allocation(hdr);

        /* 用毒模式填充用户数据区（检测 use-after-free） */
        fill_poison(hdr->data, hdr->tail_offset);

        /* 标记头部魔数为已释放（防止 double-free） */
        hdr->head_magic = ALLOC_MAGIC_FREED;

        /* 更新统计 */
        if (freed_size <= g_memory_stats.current_used) {
            g_memory_stats.current_used -= freed_size;
        } else {
            /* 防御：统计不一致时将 current_used 归零 */
            g_memory_stats.current_used = 0;
        }
        g_memory_stats.total_freed += freed_size;
        g_memory_stats.free_count++;

        free(hdr);
    } else {
        /* 非本分配器指针或已释放（魔数不匹配）：
         * 仍然释放内存以防泄漏，但不更新统计。 */
        free(*ptr);
    }

    *ptr = NULL;
}

void lv_free_many(void **first, ...) {
    va_list args;
    va_start(args, first);

    void **ptr = first;
    while (ptr) {
        lv_free(ptr);
        ptr = va_arg(args, void **);
    }

    va_end(args);
}

void lv_free_external(void **ptr) {
    if (!ptr || !*ptr)
        return;

    /* 直接调用系统free释放外部库（如GMP）分配的内存
     * GMP的mpz_get_str等函数使用系统malloc分配内存
     * 不能用lv_free释放，因为lv_free期望AllocHeader头部 */
    free(*ptr);
    *ptr = NULL;
}

/**
 * @brief 自动释放包装函数（用于 GCC/Clang cleanup 属性）
 *
 * 此函数设计为与 __attribute__((cleanup)) 配合使用，在变量离开作用域时
 * 自动调用 lv_free 释放内存，避免手动管理资源导致泄漏。
 *
 * 使用示例：
 * @code
 *   char *buf __attribute__((cleanup(lv_auto_free))) = lv_malloc(100);
 *   // buf 在离开作用域时自动释放
 * @endcode
 *
 * @param p 指向指针变量的指针（cleanup 属性传入的是变量的地址）。
 *          内部会将其转换为 void** 并调用 lv_free。
 * @note 此函数不应被直接调用，仅供编译器 cleanup 机制间接使用。
 */
void lv_auto_free(void *p) {
    void **ptr = (void **) p;
    lv_free(ptr);
}

void lv_get_memory_stats(MemoryStats *stats) {
    if (!stats)
        return;
    *stats = g_memory_stats;
}

void lv_reset_memory_stats(void) {
    memset(&g_memory_stats, 0, sizeof(g_memory_stats));
}

void lv_set_memory_limit(size_t limit) {
    g_memory_limit = limit;
}

size_t lv_get_memory_limit(void) {
    return g_memory_limit;
}

bool lv_memory_limit_exceeded(void) {
    if (g_memory_limit == 0)
        return false;
    return g_memory_stats.current_used > g_memory_limit;
}

/* ============================================================
 * POISON/MAGIC 检测实现
 * ============================================================ */

bool lv_memory_check_poison(const void *ptr, size_t size) {
    if (!ptr || size == 0)
        return true; /* 空区域视为安全 */

    const uint8_t *p = (const uint8_t *)ptr;
    size_t count = size / sizeof(uint32_t);
    uint32_t val;

    for (size_t i = 0; i < count; i++) {
        memcpy(&val, p + i * sizeof(uint32_t), sizeof(uint32_t));  /* 避免未对齐访问 */
        if (val == ALLOC_POISON) {
            lv_LOG_WARNING("Poison 标记检测: 地址 0x%p 偏移 %zu 处发现毒模式 0x%08X（可能 use-after-free）",
                             ptr, i * sizeof(uint32_t), ALLOC_POISON);
            return false;
        }
    }

    /* 检查尾部不完整的字节 */
    size_t remaining = size % sizeof(uint32_t);
    if (remaining > 0) {
        const uint8_t *tail = (const uint8_t *)(p + count);
        for (size_t i = 0; i < remaining; i++) {
            if (tail[i] == 0xBE) {
                return false;
            }
        }
    }

    return true;
}

bool lv_memory_check_magic(const void *ptr) {
    if (!ptr)
        return true; /* NULL 视为安全 */

    AllocHeader *hdr = (AllocHeader *)((char *)ptr - ALLOC_HEADER_SIZE);

    /* 检查头部魔数 */
    if (hdr->head_magic != ALLOC_HEAD_MAGIC) {
        if (hdr->head_magic == ALLOC_MAGIC_FREED) {
            lv_LOG_ERROR("魔数检测失败: 指针 0x%p 的内存已被释放（double-free?）", ptr);
        } else {
            lv_LOG_ERROR("魔数检测失败: 指针 0x%p 的头部魔数异常 0x%08X", ptr, hdr->head_magic);
        }
        return false;
    }

    /* 检查尾部魔数 */
    if (hdr->tail_offset > 0) {
        const uint32_t *tail = (const uint32_t *)((const char *)ptr + hdr->tail_offset);
        if (*tail != ALLOC_TAIL_MAGIC) {
            lv_LOG_ERROR("尾部魔数检测失败: 指针 0x%p 可能发生缓冲区溢出, 期望 0x%08X, 实际 0x%08X",
                           ptr, ALLOC_TAIL_MAGIC, *tail);
            return false;
        }
    }

    return true;
}

void lv_poison_enable(bool enable) {
    g_poison_enabled = enable;
}

bool lv_poison_is_enabled(void) {
    return g_poison_enabled;
}

/* ============================================================
 * 边界检查分配与泄漏报告实现
 * ============================================================ */

void *lv_malloc_bounded(size_t size, size_t max_size) {
    if (size > max_size) {
        lv_set_error(lv_ERROR_OVERFLOW,
                       "malloc_bounded: 请求大小 %zu 超过上限 %zu", size, max_size);
        return NULL;
    }
    return lv_malloc(size);
}

int lv_memory_leak_report(FILE *output) {
    if (!output)
        output = stderr;

    int leak_count = 0;
    size_t leak_bytes = 0;
    AllocHeader *curr = g_tracked_allocs;

    fprintf(output, "\n========== Lv-00 内存泄漏报告 ==========\n");

    if (!curr) {
        fprintf(output, "无内存泄漏 —— 所有分配均已正确释放。\n");
        fprintf(output, "==========================================\n\n");
        return 0;
    }

    fprintf(output, "%-20s %-12s %-30s %s\n", "地址", "大小(字节)", "源文件:行号", "状态");
    fprintf(output, "----------------------------------------------------------------------------\n");

    while (curr) {
        leak_count++;
        leak_bytes += curr->size;

        /* 格式化源位置信息 */
        char location[64];
        if (curr->file && curr->line > 0) {
            /* 只取文件名部分（去除路径前缀） */
            const char *filename = strrchr(curr->file, '\\');
            if (!filename)
                filename = strrchr(curr->file, '/');
            if (filename)
                filename++; /* 跳过路径分隔符 */
            else
                filename = curr->file;
            snprintf(location, sizeof(location), "%s:%d", filename, curr->line);
        } else {
            snprintf(location, sizeof(location), "<未记录>");
        }

        fprintf(output, "0x%p %-12zu %-30s [泄漏]\n",
                (const void *)curr->data, curr->size, location);

        curr = curr->track_next;
    }

    fprintf(output, "----------------------------------------------------------------------------\n");
    fprintf(output, "总计: %d 个泄漏块, %zu 字节\n", leak_count, leak_bytes);
    fprintf(output, "==========================================\n\n");

    return leak_count;
}

/* ============================================================
 * 字符串处理
 * ============================================================ */

size_t lv_strlcpy(char *dest, const char *src, size_t dest_size) {
    if (!dest || !src || dest_size == 0)
        return 0;

    size_t src_len = strlen(src);
    if (src_len < dest_size) {
        memcpy(dest, src, src_len + 1);
    } else {
        memcpy(dest, src, dest_size - 1);
        dest[dest_size - 1] = '\0';
    }
    return src_len;
}

size_t lv_strlcat(char *dest, const char *src, size_t dest_size) {
    if (!dest || !src || dest_size == 0)
        return 0;

    size_t dest_len = strlen(dest);
    if (dest_len >= dest_size)
        return dest_len + strlen(src);

    size_t remaining = dest_size - dest_len - 1;
    size_t src_len = strlen(src);

    if (src_len < remaining) {
        memcpy(dest + dest_len, src, src_len + 1);
    } else {
        memcpy(dest + dest_len, src, remaining);
        dest[dest_size - 1] = '\0';
    }
    return dest_len + src_len;
}

char *lv_strdup_safe(const char *str) {
    if (!str)
        return NULL;
    size_t len = strlen(str);
    char *copy = lv_malloc(len + 1);
    if (copy) {
        memcpy(copy, str, len + 1);
    }
    return copy;
}

char *lv_asprintf(const char *fmt, ...) {
    if (!fmt)
        return NULL;

    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    if (len < 0)
        return NULL;

    char *buf = lv_malloc((size_t) len + 1);
    if (!buf)
        return NULL;

    va_start(args, fmt);
    vsnprintf(buf, (size_t) len + 1, fmt, args);
    va_end(args);

    return buf;
}

/**
 * @brief 判断字符串是否为空白或空
 *
 * 检查给定字符串是否为 NULL、空字符串或仅包含空白字符（空格、制表符、
 * 换行符等）。
 *
 * @param str 待检查的字符串指针，允许为 NULL。
 * @return true  字符串为 NULL、空字符串或全部由空白字符组成；
 *         false 字符串包含至少一个非空白字符。
 */
bool lv_str_is_blank(const char *str) {
    if (!str)
        return true;
    while (*str) {
        if (!isspace((unsigned char) *str))
            return false;
        str++;
    }
    return true;
}

/**
 * @brief 原地去除字符串首尾空白字符
 *
 * 修改传入的字符串，去除其前导和尾部的空白字符（空格、制表符、换行符等）。
 * 通过在尾部空白处写入 '\0' 来截断字符串，并返回指向去除前导空白后
 * 第一个非空白字符的指针。
 *
 * @param str 待修剪的字符串指针，允许为 NULL。
 * @return 指向去除前导空白后的字符串起始位置的指针。
 *         若 str 为 NULL，返回 NULL。
 * @note 返回值可能与传入的 str 不同（当字符串有前导空白时）。
 *       此函数会原地修改字符串内容，调用者应使用返回值而非原始指针。
 *       若字符串全部为空白字符，返回指向末尾 '\0' 的指针。
 */
char *lv_str_trim(char *str) {
    if (!str)
        return NULL;

    /* 去除前导空白 */
    while (isspace((unsigned char) *str))
        str++;

    if (*str == '\0')
        return str;

    /* 去除尾部空白 */
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char) *end))
        end--;
    end[1] = '\0';

    return str;
}

/**
 * @brief 安全字符串复制 —— 保证 \0 终止并全面检查参数有效性
 *
 * 与 lv_strlcpy 不同：
 * - 参数为 NULL 时安全返回 NULL
 * - dest_size 为 0 时返回 NULL
 * - 仅复制 dest_size - 1 个字符并确保以 \0 结尾
 *
 * @param dest 目标缓冲区
 * @param src  源字符串（可为 NULL）
 * @param dest_size 目标缓冲区大小（字节）
 * @return 成功时返回 dest，失败时返回 NULL
 */
char *lv_strncpy(char *dest, const char *src, size_t dest_size) {
    if (!dest || !src || dest_size == 0)
        return NULL;

    size_t i;
    for (i = 0; i < dest_size - 1 && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    dest[i] = '\0';
    return dest;
}

/**
 * @brief 安全字符串连接 —— 保证 \0 终止并全面检查参数有效性
 *
 * 查找 dest 中现有字符串的末尾，然后追加 src。
 * 若 dest 已经完全填满（无 \0 终止符），则仅保证 dest[dest_size-1] = '\0'。
 *
 * @param dest 目标缓冲区（必须已包含一个有效的 \0 终止字符串）
 * @param src  源字符串（可为 NULL）
 * @param dest_size 目标缓冲区总大小（字节）
 * @return 成功时返回 dest，失败时返回 NULL
 */
char *lv_strncat(char *dest, const char *src, size_t dest_size) {
    if (!dest || !src || dest_size == 0)
        return NULL;

    /* 查找 dest 当前字符串的末尾 */
    size_t dest_len = 0;
    while (dest_len < dest_size && dest[dest_len] != '\0') {
        dest_len++;
    }

    /* 若 dest 已满（没有 \0），则保证末尾为 \0 */
    if (dest_len >= dest_size) {
        dest[dest_size - 1] = '\0';
        return dest;
    }

    /* 追加 src */
    size_t remaining = dest_size - dest_len - 1; /* -1 保留 \0 空间 */
    size_t i;
    for (i = 0; i < remaining && src[i] != '\0'; i++) {
        dest[dest_len + i] = src[i];
    }
    dest[dest_len + i] = '\0';
    return dest;
}

/**
 * @brief 安全格式化输出到定长缓冲区
 *
 * 包装 vsnprintf，添加参数有效性检查并确保 \0 终止。
 *
 * @param buf  输出缓冲区
 * @param size 缓冲区大小
 * @param fmt  格式字符串
 * @param ...  可变参数
 * @return 成功时返回写入的字符数（不含 \0），失败返回 -1
 */
int lv_snprintf(char *buf, size_t size, const char *fmt, ...) {
    if (!buf || size == 0 || !fmt)
        return -1;

    va_list args;
    va_start(args, fmt);
    int written = vsnprintf(buf, size, fmt, args);
    va_end(args);

    /* 确保 \0 终止（防御 vsnprintf 的某些非标准实现） */
    if (written < 0) {
        buf[0] = '\0';
        return -1;
    }
    if ((size_t)written >= size) {
        buf[size - 1] = '\0';
    }

    return written;
}

/* ============================================================
 * 动态数组
 * ============================================================ */

lvArray *lv_array_create(size_t initial_capacity, size_t elem_size) {
    /* 修复：验证 elem_size，避免后续操作中出现除零或无意义的零大小元素 */
    if (elem_size == 0)
        return NULL;

    lvArray *arr = lv_calloc(1, sizeof(lvArray));
    if (!arr)
        return NULL;

    arr->count = 0;
    arr->capacity = initial_capacity > 0 ? initial_capacity : lv_INITIAL_ARRAY_CAPACITY;
    arr->elem_size = elem_size;
    arr->store_pointers = false; /* 修复：elem_size 已验证非零，不再需要 store_pointers 回退逻辑 */

    arr->data = lv_calloc(arr->capacity, sizeof(void *));
    if (!arr->data) {
        /* 修复：lv_calloc 失败时释放已分配的 arr，防止资源泄漏 */
        lv_free((void **) &arr);
        return NULL;
    }

    return arr;
}

void lv_array_destroy(lvArray *arr, bool free_elements) {
    if (!arr)
        return;

    if (free_elements && arr->data) {
        for (size_t i = 0; i < arr->count; i++) {
            if (arr->data[i]) {
                lv_free((void **) &arr->data[i]);
            }
        }
    }

    lv_free((void **) &arr->data);
    lv_free((void **) &arr);
}

static bool lv_array_ensure_capacity(lvArray *arr, size_t min_capacity) {
    if (!arr)
        return false;
    /* 输入验证：容量为0时按最小默认容量处理，避免死循环 */
    if (min_capacity == 0)
        min_capacity = 1;
    if (arr->capacity >= min_capacity)
        return true;

    size_t new_capacity = arr->capacity;
    while (new_capacity < min_capacity) {
        /* 修复：检查两步溢出
         * 1. new_capacity * lv_ARRAY_GROWTH_FACTOR 不能超过 SIZE_MAX
         * 2. new_capacity * sizeof(void*) 不能超过 SIZE_MAX（分配时使用） */
        if (new_capacity > SIZE_MAX / lv_ARRAY_GROWTH_FACTOR)
            return false;
        new_capacity *= lv_ARRAY_GROWTH_FACTOR;
    }

    /* 修复：检查 new_capacity * sizeof(void*) 是否溢出 */
    if (new_capacity > SIZE_MAX / sizeof(void *))
        return false;
    size_t alloc_size = new_capacity * sizeof(void *);

    void **new_data = lv_realloc(arr->data, alloc_size);
    if (!new_data)
        return false;

    /* 清零新分配的部分 */
    memset(new_data + arr->capacity, 0, (new_capacity - arr->capacity) * sizeof(void *));

    arr->data = new_data;
    arr->capacity = new_capacity;
    return true;
}

bool lv_array_push(lvArray *arr, void *elem) {
    if (!arr)
        return false;

    if (!lv_array_ensure_capacity(arr, arr->count + 1)) {
        return false;
    }

    arr->data[arr->count++] = elem;
    return true;
}

bool lv_array_remove(lvArray *arr, size_t index, bool free_elem) {
    if (!arr || index >= arr->count)
        return false;

    if (free_elem && arr->data[index]) {
        lv_free((void **) &arr->data[index]);
    }

    /* 移动后续元素 */
    for (size_t i = index; i < arr->count - 1; i++) {
        arr->data[i] = arr->data[i + 1];
    }
    arr->count--;
    arr->data[arr->count] = NULL;

    return true;
}

void *lv_array_get(const lvArray *arr, size_t index) {
    if (!arr || index >= arr->count)
        return NULL;
    return arr->data[index];
}

bool lv_array_set(lvArray *arr, size_t index, void *elem) {
    if (!arr || index >= arr->count)
        return false;
    arr->data[index] = elem;
    return true;
}

void lv_array_clear(lvArray *arr, bool free_elements) {
    if (!arr)
        return;

    if (free_elements) {
        for (size_t i = 0; i < arr->count; i++) {
            if (arr->data[i]) {
                lv_free((void **) &arr->data[i]);
            }
        }
    }

    memset(arr->data, 0, arr->capacity * sizeof(void *));
    arr->count = 0;
}

void lv_array_sort(lvArray *arr, int (*cmp)(const void *, const void *)) {
    if (!arr || !cmp || arr->count < 2)
        return;
    qsort(arr->data, arr->count, sizeof(void *), cmp);
}

int lv_array_find(const lvArray *arr, const void *elem) {
    if (!arr)
        return -1;
    for (size_t i = 0; i < arr->count; i++) {
        if (arr->data[i] == elem)
            return (int) i;
    }
    return -1;
}

/* ============================================================
 * 整数数组
 * ============================================================ */

IntArray *int_array_create(size_t initial_capacity) {
    IntArray *arr = lv_calloc(1, sizeof(IntArray));
    if (!arr)
        return NULL;

    arr->count = 0;
    arr->capacity = initial_capacity > 0 ? initial_capacity : lv_INITIAL_ARRAY_CAPACITY;
    arr->data = lv_calloc(arr->capacity, sizeof(int));

    if (!arr->data) {
        lv_free((void **) &arr);
        return NULL;
    }

    return arr;
}

void int_array_destroy(IntArray *arr) {
    if (!arr)
        return;
    lv_free((void **) &arr->data);
    lv_free((void **) &arr);
}

static bool int_array_ensure_capacity(IntArray *arr, size_t min_capacity) {
    if (!arr)
        return false;
    /* 输入验证：容量为0时按最小默认容量处理，避免死循环 */
    if (min_capacity == 0)
        min_capacity = 1;
    if (arr->capacity >= min_capacity)
        return true;

    size_t new_capacity = arr->capacity;
    while (new_capacity < min_capacity) {
        /* 修复：检查两步溢出（与 lv_array_ensure_capacity 相同） */
        if (new_capacity > SIZE_MAX / lv_ARRAY_GROWTH_FACTOR)
            return false;
        new_capacity *= lv_ARRAY_GROWTH_FACTOR;
    }

    /* 修复：检查 new_capacity * sizeof(int) 是否溢出 */
    if (new_capacity > SIZE_MAX / sizeof(int))
        return false;
    size_t alloc_size = new_capacity * sizeof(int);

    int *new_data = lv_realloc(arr->data, alloc_size);
    if (!new_data)
        return false;

    arr->data = new_data;
    arr->capacity = new_capacity;
    return true;
}

bool int_array_push(IntArray *arr, int value) {
    if (!arr)
        return false;
    if (!int_array_ensure_capacity(arr, arr->count + 1))
        return false;

    arr->data[arr->count++] = value;
    return true;
}

/**
 * @brief 批量向整数数组末尾追加多个元素
 *
 * 将 values 数组中的 count 个整数依次追加到 arr 的末尾。
 * 若空间不足，会自动扩容。
 *
 * @param arr    目标整数数组指针，不允许为 NULL。
 * @param values 源数据数组指针，不允许为 NULL。
 * @param count  要追加的元素个数。
 * @return true  追加成功；
 *         false 参数无效或内存扩容失败。
 */
bool int_array_push_many(IntArray *arr, const int *values, size_t count) {
    if (!arr || !values)
        return false;
    if (!int_array_ensure_capacity(arr, arr->count + count))
        return false;

    memcpy(arr->data + arr->count, values, count * sizeof(int));
    arr->count += count;
    return true;
}

/**
 * @brief 判断整数数组是否包含指定值
 *
 * 线性遍历数组，检查是否存在与 value 相等的元素。
 *
 * @param arr   整数数组指针，允许为 NULL。
 * @param value 要查找的值。
 * @return true  数组中存在该值；
 *         false arr 为 NULL 或数组中不存在该值。
 * @note 时间复杂度为 O(n)，不适用于对性能敏感的频繁查找场景。
 */
bool int_array_contains(const IntArray *arr, int value) {
    if (!arr)
        return false;
    for (size_t i = 0; i < arr->count; i++) {
        if (arr->data[i] == value)
            return true;
    }
    return false;
}

/**
 * @brief 查找指定值在整数数组中首次出现的索引
 *
 * 线性遍历数组，返回第一个与 value 相等的元素的下标。
 *
 * @param arr   整数数组指针，允许为 NULL。
 * @param value 要查找的值。
 * @return >=0  值在数组中的索引（从 0 开始）；
 *         -1   arr 为 NULL 或数组中不存在该值。
 * @note 时间复杂度为 O(n)。若数组中存在多个匹配项，仅返回第一个的索引。
 */
int int_array_index_of(const IntArray *arr, int value) {
    if (!arr)
        return -1;
    for (size_t i = 0; i < arr->count; i++) {
        if (arr->data[i] == value)
            return (int) i;
    }
    return -1;
}

/**
 * @brief 从整数数组中移除指定值的第一个匹配项
 *
 * 查找并移除数组中第一个与 value 相等的元素，后续元素前移以保持连续性。
 *
 * @param arr   整数数组指针，不允许为 NULL。
 * @param value 要移除的值。
 * @return true  成功找到并移除了该值；
 *         false arr 为 NULL 或数组中不存在该值。
 * @note 仅移除第一个匹配项，若存在多个相同值需多次调用。
 *       移除操作的时间复杂度为 O(n)（含查找和元素前移）。
 */
bool int_array_remove(IntArray *arr, int value) {
    if (!arr)
        return false;
    int idx = int_array_index_of(arr, value);
    if (idx < 0)
        return false;

    /* 移动后续元素 */
    for (size_t i = (size_t) idx; i < arr->count - 1; i++) {
        arr->data[i] = arr->data[i + 1];
    }
    arr->count--;
    return true;
}

/**
 * @brief 整数三向比较函数（用于 qsort 排序）
 *
 * 避免 (ia > ib) - (ia < ib) 写法在极端值情况下可能触发的
 * 未定义行为（INT_MIN 与 INT_MAX 相减导致有符号整数溢出）。
 *
 * @param a 指向第一个 int 的指针
 * @param b 指向第二个 int 的指针
 * @return 负数（a < b）、零（a == b）、正数（a > b）
 */
static int compare_int(const void *a, const void *b) {
    int ia = *(const int *) a;
    int ib = *(const int *) b;
    /* 使用分支而非算术运算，避免有符号整数溢出风险 */
    if (ia < ib)
        return -1;
    if (ia > ib)
        return 1;
    return 0;
}

void int_array_sort(IntArray *arr) {
    if (!arr || arr->count < 2)
        return;
    qsort(arr->data, arr->count, sizeof(int), compare_int);
}

IntArray *int_array_copy(const IntArray *arr) {
    if (!arr)
        return NULL;
    IntArray *copy = int_array_create(arr->capacity);
    if (!copy)
        return NULL;

    memcpy(copy->data, arr->data, arr->count * sizeof(int));
    copy->count = arr->count;
    return copy;
}

IntArray *int_array_from_carray(const int *data, size_t count) {
    if (!data)
        return NULL;
    IntArray *arr = int_array_create(count);
    if (!arr)
        return NULL;

    memcpy(arr->data, data, count * sizeof(int));
    arr->count = count;
    return arr;
}

/* ============================================================
 * 配置管理
 * ============================================================ */

/* 消除魔术数字，用宏定义替代字面量 */
#define CONFIG_LINE_BUFFER_SIZE 1024 /**< 配置文件每行读取缓冲区大小 */

static ConfigItem *config_item_create(const char *key, ConfigType type) {
    ConfigItem *item = lv_calloc(1, sizeof(ConfigItem));
    if (!item)
        return NULL;

    item->key = lv_strdup_safe(key);
    if (!item->key) {
        lv_free((void **) &item);
        return NULL;
    }
    item->type = type;
    return item;
}

static void config_item_destroy(ConfigItem *item) {
    if (!item)
        return;

    lv_free((void **) &item->key);

    switch (item->type) {
        case CONFIG_TYPE_STRING:
            lv_free((void **) &item->value.string_val);
            break;
        case CONFIG_TYPE_ARRAY:
            for (size_t i = 0; i < item->array_count; i++) {
                config_item_destroy(item->value.array_val[i]);
            }
            lv_free((void **) &item->value.array_val);
            break;
        default:
            break;
    }

    lv_free((void **) &item);
}

ConfigManager *config_manager_create(const char *config_file) {
    ConfigManager *mgr = lv_calloc(1, sizeof(ConfigManager));
    if (!mgr)
        return NULL;

    if (config_file) {
        mgr->config_file = lv_strdup_safe(config_file);
    }
    mgr->auto_save = false;

    return mgr;
}

void config_manager_destroy(ConfigManager *mgr) {
    if (!mgr)
        return;

    ConfigItem *item = mgr->items;
    while (item) {
        ConfigItem *next = item->next;
        config_item_destroy(item);
        item = next;
    }

    lv_free((void **) &mgr->config_file);
    lv_free((void **) &mgr);
}

/**
 * @brief 在配置管理器中查找指定键对应的配置项
 *
 * 遍历配置管理器的链表，通过字符串比较查找与 key 匹配的配置项。
 *
 * @param mgr 配置管理器指针，允许为 NULL。
 * @param key 要查找的配置键名，允许为 NULL。
 * @return 找到的配置项指针；若 mgr 或 key 为 NULL，或未找到匹配项，返回 NULL。
 * @note 此为内部静态函数，仅供配置管理模块内部使用。
 */
static ConfigItem *config_find_item(const ConfigManager *mgr, const char *key) {
    if (!mgr || !key)
        return NULL;

    ConfigItem *item = mgr->items;
    while (item) {
        if (strcmp(item->key, key) == 0)
            return item;
        item = item->next;
    }
    return NULL;
}

/**
 * @brief 生成标量类型配置设置函数的宏
 *
 * 用于 int、bool、double 等标量类型的 config_set_* 函数，
 * 避免重复编写"查找已有项 → 更新或创建 → 自动保存"的通用逻辑。
 *
 * 参数说明：
 *   func_name  - 要生成的函数名（如 config_set_int）
 *   cfg_type   - 对应的 ConfigType 枚举值（如 CONFIG_TYPE_INT）
 *   val_type   - 值参数的 C 类型（如 int）
 *   val_member - ConfigItem.value 联合体中的成员名（如 int_val）
 *
 * 注意：config_set_string 不使用此宏，因为字符串类型需要额外的
 * 内存管理（释放旧值、strdup 新值），逻辑与标量类型有本质区别。
 */
#define DEFINE_CONFIG_SET_SCALAR(func_name, cfg_type, val_type, val_member) \
    bool func_name(ConfigManager *mgr, const char *key, val_type value) {   \
        if (!mgr || !key)                                                   \
            return false;                                                   \
                                                                            \
        ConfigItem *item = config_find_item(mgr, key);                      \
        if (item) {                                                         \
            item->type = cfg_type;                                          \
            item->value.val_member = value;                                 \
        } else {                                                            \
            item = config_item_create(key, cfg_type);                       \
            if (!item)                                                      \
                return false;                                               \
            item->value.val_member = value;                                 \
            item->next = mgr->items;                                        \
            mgr->items = item;                                              \
        }                                                                   \
                                                                            \
        if (mgr->auto_save)                                                 \
            config_save(mgr);                                               \
        return true;                                                        \
    }

/* 使用宏生成 int、bool、double 三种标量类型的配置设置函数 */
DEFINE_CONFIG_SET_SCALAR(config_set_int, CONFIG_TYPE_INT, int, int_val)
DEFINE_CONFIG_SET_SCALAR(config_set_bool, CONFIG_TYPE_BOOL, bool, bool_val)
DEFINE_CONFIG_SET_SCALAR(config_set_double, CONFIG_TYPE_DOUBLE, double, double_val)

bool config_set_string(ConfigManager *mgr, const char *key, const char *value) {
    if (!mgr || !key)
        return false;

    ConfigItem *item = config_find_item(mgr, key);
    if (item) {
        if (item->type == CONFIG_TYPE_STRING) {
            lv_free((void **) &item->value.string_val);
        }
        item->type = CONFIG_TYPE_STRING;
        item->value.string_val = lv_strdup_safe(value);
    } else {
        item = config_item_create(key, CONFIG_TYPE_STRING);
        if (!item)
            return false;
        item->value.string_val = lv_strdup_safe(value);
        item->next = mgr->items;
        mgr->items = item;
    }

    if (mgr->auto_save)
        config_save(mgr);
    return true;
}

int config_get_int(const ConfigManager *mgr, const char *key, int default_val) {
    ConfigItem *item = config_find_item(mgr, key);
    if (item && item->type == CONFIG_TYPE_INT) {
        return item->value.int_val;
    }
    return default_val;
}

bool config_get_bool(const ConfigManager *mgr, const char *key, bool default_val) {
    ConfigItem *item = config_find_item(mgr, key);
    if (item && item->type == CONFIG_TYPE_BOOL) {
        return item->value.bool_val;
    }
    return default_val;
}

double config_get_double(const ConfigManager *mgr, const char *key, double default_val) {
    ConfigItem *item = config_find_item(mgr, key);
    if (item && item->type == CONFIG_TYPE_DOUBLE) {
        return item->value.double_val;
    }
    return default_val;
}

const char *config_get_string(const ConfigManager *mgr, const char *key, const char *default_val) {
    ConfigItem *item = config_find_item(mgr, key);
    if (item && item->type == CONFIG_TYPE_STRING) {
        return item->value.string_val;
    }
    return default_val;
}

/**
 * @brief 检查配置管理器中是否存在指定键
 *
 * @param mgr 配置管理器指针，允许为 NULL。
 * @param key 要检查的配置键名，允许为 NULL。
 * @return true  配置中存在该键；
 *         false mgr 或 key 为 NULL，或配置中不存在该键。
 */
bool config_has_key(const ConfigManager *mgr, const char *key) {
    return config_find_item(mgr, key) != NULL;
}

bool config_remove(ConfigManager *mgr, const char *key) {
    if (!mgr || !key)
        return false;

    ConfigItem **current = &mgr->items;
    while (*current) {
        if (strcmp((*current)->key, key) == 0) {
            ConfigItem *to_remove = *current;
            *current = to_remove->next;
            config_item_destroy(to_remove);
            if (mgr->auto_save)
                config_save(mgr);
            return true;
        }
        current = &(*current)->next;
    }
    return false;
}

/* 配置文件格式支持：
 *   - 注释行：以 '#' 开头
 *   - 节头：[section_name]   后续键自动加上 "section_name." 前缀
 *   - 键值对：key = value     在节内时存储为 "section.key"
 *   - 空行：忽略
 * 支持通过 dotted notation (如 "section.key") 查找配置项。
 */
bool config_load(ConfigManager *mgr) {
    if (!mgr || !mgr->config_file)
        return false;

    FILE *f = fopen(mgr->config_file, "r");
    if (!f)
        return false;

    char current_section[256];
    current_section[0] = '\0';

    char line[CONFIG_LINE_BUFFER_SIZE];
    while (fgets(line, sizeof(line), f)) {
        char *trimmed = lv_str_trim(line);
        if (*trimmed == '\0' || *trimmed == '#')
            continue;

        /* 解析节头 [section_name] */
        if (*trimmed == '[') {
            char *close_bracket = strchr(trimmed, ']');
            if (close_bracket) {
                *close_bracket = '\0';
                char *section_name = lv_str_trim(trimmed + 1);
                snprintf(current_section, sizeof(current_section), "%s", section_name);
            }
            continue;
        }

        char *eq = strchr(trimmed, '=');
        if (!eq)
            continue;

        *eq = '\0';
        char *raw_key = lv_str_trim(trimmed);
        char *value = lv_str_trim(eq + 1);

        /* 构建带节前缀的完整键名：section.key 或直接 key */
        char full_key[512];
        if (current_section[0] != '\0') {
            snprintf(full_key, sizeof(full_key), "%s.%s", current_section, raw_key);
        } else {
            snprintf(full_key, sizeof(full_key), "%s", raw_key);
        }

        /* 尝试解析为整数 */
        char *endptr;
        long int_val = strtol(value, &endptr, 10);
        if (*endptr == '\0') {
            config_set_int(mgr, full_key, (int) int_val);
            continue;
        }

        /* 尝试解析为布尔值 */
        if (strcmp(value, "true") == 0 || strcmp(value, "yes") == 0) {
            config_set_bool(mgr, full_key, true);
            continue;
        }
        if (strcmp(value, "false") == 0 || strcmp(value, "no") == 0) {
            config_set_bool(mgr, full_key, false);
            continue;
        }

        /* 尝试解析为浮点数 */
        double double_val = strtod(value, &endptr);
        if (*endptr == '\0') {
            config_set_double(mgr, full_key, double_val);
            continue;
        }

        /* 否则作为字符串 */
        config_set_string(mgr, full_key, value);
    }

    fclose(f);
    return true;
}

bool config_save(const ConfigManager *mgr) {
    if (!mgr || !mgr->config_file)
        return false;

    FILE *f = fopen(mgr->config_file, "w");
    if (!f)
        return false;

    fprintf(f, "# Lv-00 Configuration File\n");
    fprintf(f, "# Auto-generated\n\n");

    char last_section[256];
    last_section[0] = '\0';

    ConfigItem *item = mgr->items;
    while (item) {
        /* 检测节前缀：如果键包含 '.'，提取节名并在变化时输出节头 */
        const char *dot = strchr(item->key, '.');
        if (dot) {
            char section[256];
            size_t section_len = (size_t)(dot - item->key);
            if (section_len >= sizeof(section))
                section_len = sizeof(section) - 1;
            memcpy(section, item->key, section_len);
            section[section_len] = '\0';

            if (strcmp(section, last_section) != 0) {
                fprintf(f, "\n[%s]\n", section);
                snprintf(last_section, sizeof(last_section), "%s", section);
            }
        } else {
            /* 无节前缀的键：如果之前在某个节内，先输出空行退出节 */
            if (last_section[0] != '\0') {
                fprintf(f, "\n");
                last_section[0] = '\0';
            }
        }

        switch (item->type) {
            case CONFIG_TYPE_INT:
                fprintf(f, "%s = %d\n", item->key, item->value.int_val);
                break;
            case CONFIG_TYPE_BOOL:
                fprintf(f, "%s = %s\n", item->key, item->value.bool_val ? "true" : "false");
                break;
            case CONFIG_TYPE_DOUBLE:
                fprintf(f, "%s = %.6f\n", item->key, item->value.double_val);
                break;
            case CONFIG_TYPE_STRING:
                fprintf(f, "%s = %s\n", item->key, item->value.string_val);
                break;
            case CONFIG_TYPE_ARRAY:
                /* 数组类型：逐元素序列化 */
                fprintf(f, "%s = [", item->key);
                if (item->value.array_val && item->array_count > 0) {
                    for (size_t ai = 0; ai < item->array_count; ai++) {
                        if (ai > 0)
                            fprintf(f, ", ");
                        ConfigItem *elem_item = item->value.array_val[ai];
                        if (elem_item && elem_item->key) {
                            fprintf(f, "\"%s\"", elem_item->key);
                        } else {
                            fprintf(f, "\"\"");
                        }
                    }
                }
                fprintf(f, "]\n");
                break;
            default:
                break;
        }
        item = item->next;
    }

    fclose(f);
    return true;
}

/* ============================================================
 * 版本管理
 * ============================================================ */

lvVersion *version_parse(const char *version_str) {
    if (!version_str)
        return NULL;

    lvVersion *ver = lv_calloc(1, sizeof(lvVersion));
    if (!ver)
        return NULL;

    /* 解析主版本.次版本.修订版本 */
    int parsed = sscanf(version_str, "%d.%d.%d", &ver->major, &ver->minor, &ver->patch);
    if (parsed < 2) {
        lv_free((void **) &ver);
        return NULL;
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
        return false;

    /* 主版本必须相同 */
    if (required->major != actual->major)
        return false;

    /* 实际版本必须大于等于要求版本 */
    return version_compare(actual, required) >= 0;
}

bool lv_check_version(const char *min_version) {
    lvVersion *min = version_parse(min_version);
    if (!min)
        return false;

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

#ifdef _WIN32
#include <windows.h>

uint64_t lv_get_time_us(void) {
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (uint64_t) (count.QuadPart * (LONGLONG) lv_US_PER_S / freq.QuadPart);
}

#else
#include <sys/time.h>

uint64_t lv_get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t) tv.tv_sec * lv_US_PER_S + (uint64_t) tv.tv_usec;
}
#endif

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

static lv_THREAD_LOCAL uint64_t g_random_state = 0;

void lv_random_init(uint64_t seed) {
    g_random_state = seed ? seed : (uint64_t) time(NULL);
}

/* xorshift64* 伪随机数生成器（Marsaglia, 2003） */
static uint64_t xorshift64star(void) {
    g_random_state ^= g_random_state >> lv_XORSHIFT_SHIFT_A;
    g_random_state ^= g_random_state << lv_XORSHIFT_SHIFT_B;
    g_random_state ^= g_random_state >> lv_XORSHIFT_SHIFT_C;
    return g_random_state * lv_XORSHIFT_MULTIPLIER;
}

int lv_random_int(int min, int max) {
    if (min >= max)
        return min;
    uint64_t range = (uint64_t)max - (uint64_t)min;
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
 * 日志系统（运行时级别过滤 + 时间戳 + 可选文件输出）
 * ============================================================ */

/** 当前运行时日志级别（默认 INFO，即 3） */
static int g_log_level = lv_LOG_LEVEL_INFO;

/** 可选日志文件句柄（NULL 表示仅输出到 stderr） */
static FILE *g_log_file = NULL;

/**
 * @brief 日志级别名称映射
 */
static const char *log_level_name(int level) {
    switch (level) {
    case lv_LOG_LEVEL_ERROR:   return "ERROR";
    case lv_LOG_LEVEL_WARNING: return "WARN ";
    case lv_LOG_LEVEL_INFO:    return "INFO ";
    case lv_LOG_LEVEL_DEBUG:   return "DEBUG";
    default:                     return "TRACE";
    }
}

/**
 * @brief 输出日志消息（带级别过滤、时间戳、可选文件输出）
 *
 * 由 lv_LOG_INFO / lv_LOG_WARNING / lv_LOG_ERROR / lv_LOG_DEBUG
 * 系列宏间接调用。实现功能：
 * - 运行时日志级别过滤（低于 g_log_level 的消息被丢弃）
 * - 自动添加时间戳 [YYYY-MM-DD HH:MM:SS]
 * - 格式：[TIMESTAMP] [LEVEL] [file:line] message
 * - 默认输出到 stderr，可配置同时写入日志文件
 *
 * @param level 日志级别（lv_LOG_LEVEL_DEBUG / INFO / WARNING / ERROR）
 * @param file  源文件名（__FILE__）
 * @param line  源文件行号（__LINE__）
 * @param fmt   printf 风格格式字符串
 * @param ...   可变参数
 */
void lv_log_message(int level, const char *file, int line, const char *fmt, ...) {
    /* 运行时级别过滤：低于当前级别的日志直接丢弃 */
    if (level > g_log_level) return;

    /* 生成时间戳 */
    time_t now = time(NULL);
    struct tm tm_buf;
    lv_LOCALTIME(&now, &tm_buf);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm_buf);

    /* 格式化级别名称 */
    const char *level_str = log_level_name(level);

    /* 输出到 stderr */
    fprintf(stderr, "[%s] [%s] [%s:%d] ", timestamp, level_str,
            file ? file : "?", line);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
    fflush(stderr);

    /* 可选：同时写入日志文件 */
    if (g_log_file) {
        fprintf(g_log_file, "[%s] [%s] [%s:%d] ", timestamp, level_str,
                file ? file : "?", line);
        va_list args2;
        va_start(args2, fmt);
        vfprintf(g_log_file, fmt, args2);
        va_end(args2);
        fprintf(g_log_file, "\n");
        fflush(g_log_file);
    }
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
        return false;

    /* 无需扩容 */
    if (count < *capacity)
        return true;

    /* 溢出检查 */
    if (count < 0 || *capacity < 0)
        return false;

    /* 计算最小需求容量 */
    int min_required = count + min_growth;
    if (min_required < count)  /* 溢出检测 */
        return false;

    /* 计算新容量 */
    if (*capacity > INT_MAX / lv_ARRAY_GROWTH_FACTOR)
        return false;
    int new_cap = (*capacity == 0) ? lv_INITIAL_ARRAY_CAPACITY : *capacity * lv_ARRAY_GROWTH_FACTOR;
    if (new_cap < min_required) {
        if (min_required > INT_MAX / lv_ARRAY_GROWTH_FACTOR)
            return false;
        new_cap = min_required * lv_ARRAY_GROWTH_FACTOR;
    }

    /* 分配前检查 size_t 溢出 */
    if ((size_t)new_cap > SIZE_MAX / elem_size)
        return false;

    void *new_arr = lv_realloc(*arr, (size_t)new_cap * elem_size);
    if (!new_arr)
        return false;

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
    const uint8_t *p = (const uint8_t *)data;
    uint64_t hash = lv_FNV64_OFFSET_BASIS;
    for (size_t i = 0; i < len; i++) {
        hash ^= p[i];
        hash *= lv_FNV64_PRIME;
    }
    return hash;
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
    void *resource;                  /**< 资源指针（文件句柄、内存、锁等） */
    lvResourceDestroyFunc destroy; /**< 资源销毁回调 */
    char *name;                      /**< 资源名称（用于调试），可为 NULL */
    struct TrackedResource *prev;    /**< 前驱节点 */
    struct TrackedResource *next;    /**< 后继节点 */
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
    ResourceTracker *rt = (ResourceTracker *)lv_calloc(1, sizeof(ResourceTracker));
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

bool lv_resource_track(ResourceTracker *rt, void *resource,
                          lvResourceDestroyFunc destroy, const char *name) {
    if (!rt || !resource || !destroy)
        return false;

    TrackedResource *node = (TrackedResource *)lv_calloc(1, sizeof(TrackedResource));
    if (!node)
        return false;

    node->resource = resource;
    node->destroy = destroy;

    /* 复制名称（若有） */
    if (name) {
        node->name = (char *)lv_malloc(strlen(name) + 1);
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
        return false;

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

    return false;
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
            lv_LOG_DEBUG("资源追踪器清理: %s (0x%p)",
                           node->name ? node->name : "<未命名>", node->resource);
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

    AllocHeader *hdr = get_header(ptr);
    if (hdr) {
        size_t freed_size = hdr->size;

        untrack_allocation(hdr);
        fill_poison(hdr->data, hdr->tail_offset);
        hdr->head_magic = ALLOC_MAGIC_FREED;

        if (freed_size <= g_memory_stats.current_used) {
            g_memory_stats.current_used -= freed_size;
        } else {
            g_memory_stats.current_used = 0;
        }
        g_memory_stats.total_freed += freed_size;
        g_memory_stats.free_count++;
        free(hdr);
    } else {
        lv_free((void **) &ptr);
    }
}

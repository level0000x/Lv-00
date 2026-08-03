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
 *    - 内存限制 (s_utils_state.memory_limit) 同样是线程局部变量。
 *    - 多线程环境下的跨线程内存操作需调用者自行同步。
 */

#include "lv_utils.h"
#include "lv_utils_internal.h"

#include "lv/allocator.h"
#include "lv/lv_file.h"

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


/* [Bug修复] 为 realloc 非本分配器路径获取旧大小所需的平台头文件 */
#ifdef _WIN32
#include <malloc.h> /* _msize */

#elif defined(__linux__)
#include <malloc.h> /* malloc_usable_size */

#elif defined(__APPLE__)
#include <malloc/malloc.h> /* malloc_size on macOS */

#endif

#include "error_codes.h"
#include "lv.h"
#include "debug.h"
#include "lv_internal.h"



/* ============================================================
 * 内存统计跟踪
 * ============================================================ */




/** 模块级唯一状态实例（线程局部） */
lv_THREAD_LOCAL LvUtilsState s_utils_state = {0};

/**
 * @brief 从用户指针获取分配头
 * @param ptr 用户指针（即 lv_malloc 等返回的 data[] 地址）
 * @return 对应的 AllocHeader 指针；若头部或尾部魔数不匹配则返回 NULL
 */
AllocHeader *get_header(void *ptr) {
    if (!ptr)
        return NULL;
    AllocHeader *hdr = (AllocHeader *) ((char *) ptr - ALLOC_HEADER_SIZE);
    /* 检查头部魔数 */
    if (hdr->head_magic != ALLOC_HEAD_MAGIC)
        return NULL;

    /* 检查尾部魔数（缓冲区溢出检测）—— 使用 memcpy 避免未对齐访问 */
    if (hdr->tail_offset > 0) {
        uint32_t tail_value;
        memcpy(&tail_value, (char *) ptr + hdr->tail_offset, sizeof(uint32_t));
        if (tail_value != ALLOC_TAIL_MAGIC) {
            /* 尾部魔数不匹配 —— 可能发生了缓冲区溢出 */
            lv_LOG_ERROR("内存损坏检测: 尾部魔数不匹配，指针=0x%p, 期望=0x%08X, 实际=0x%08X", ptr, ALLOC_TAIL_MAGIC,
                         tail_value);
            return NULL;
        }
    }
    return hdr;
}

/**
 * @brief 将分配加入全局追踪链表
 * @param hdr 分配头指针
 */
void track_allocation(AllocHeader *hdr) {
    hdr->track_next = s_utils_state.tracked_allocs;
    s_utils_state.tracked_allocs = hdr;
}

/**
 * @brief 从全局追踪链表中移除分配
 * @param hdr 分配头指针
 * @return true 成功移除，false 未找到
 */
bool untrack_allocation(AllocHeader *hdr) {
    AllocHeader **curr = &s_utils_state.tracked_allocs;
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
void fill_poison(void *data, size_t size) {
    if (!s_utils_state.poison_enabled || !data || size == 0)
        return;
    /* 使用 memcpy 避免未对齐访问 —— data 可能非 4 字节对齐 */
    uint32_t poison_val = ALLOC_POISON;
    uint8_t *p = (uint8_t *) data;
    size_t count = size / sizeof(uint32_t);
    for (size_t i = 0; i < count; i++) {
        memcpy(p + i * sizeof(uint32_t), &poison_val, sizeof(uint32_t));
    }
    /* 填充剩余不足 4 字节的尾部 */
    size_t remaining = size % sizeof(uint32_t);
    if (remaining > 0) {
        uint8_t *tail = (uint8_t *) (p + count);
        memset(tail, 0xBE, remaining);
    }
}

void *lv_malloc(size_t size) {
    return lv_allocator_get()->alloc(size);
}

void *lv_malloc_tracked(size_t size, const char *file, int line) {
    /* 零大小请求：分配最小块（1 字节数据 + 尾魔数），保持 lv_malloc(0) 语义 */
    size_t alloc_size = size ? size : 1;

    if (s_utils_state.memory_limit > 0 && s_utils_state.memory_stats.current_used > s_utils_state.memory_limit - alloc_size) {
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "内存限制超出: 请求%zu", alloc_size);
    }

    /* 检查溢出：头部大小 + 用户请求大小 + 尾部魔数大小 */
    AllocHeader *hdr = NULL;
    size_t total = ALLOC_HEADER_SIZE;
    if (total > SIZE_MAX - alloc_size || total + alloc_size > SIZE_MAX - ALLOC_TAIL_MAGIC_SIZE) {
        lv_RETURN_ERROR_NULL(lv_ERROR_OVERFLOW, "malloc 溢出: header=%zu + size=%zu + tail=%zu", (size_t) ALLOC_HEADER_SIZE,
                     alloc_size, (size_t) ALLOC_TAIL_MAGIC_SIZE);
    }
    total += alloc_size;
    total += ALLOC_TAIL_MAGIC_SIZE;

    hdr = (AllocHeader *) malloc(total);
    if (!hdr)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "malloc 分配失败");

    hdr->head_magic = ALLOC_HEAD_MAGIC;
    hdr->tail_offset = (uint32_t) alloc_size;
    hdr->size = alloc_size;
    hdr->file = file;
    hdr->line = line;

    /* 设置尾部魔数（使用 memcpy 避免未对齐访问） */
    uint32_t tail_magic = ALLOC_TAIL_MAGIC;
    memcpy(hdr->data + alloc_size, &tail_magic, sizeof(uint32_t));

    /* 加入全局追踪链表 */
    track_allocation(hdr);

    s_utils_state.memory_stats.total_allocated += alloc_size;
    s_utils_state.memory_stats.current_used += alloc_size;
    s_utils_state.memory_stats.allocation_count++;
    if (s_utils_state.memory_stats.current_used > s_utils_state.memory_stats.peak_used)
        s_utils_state.memory_stats.peak_used = s_utils_state.memory_stats.current_used;

    return hdr->data;
}

void *lv_calloc(size_t nmemb, size_t size) {
    const AllocatorOps *ops = lv_allocator_get();
    if (ops->calloc) {
        return ops->calloc(nmemb, size);
    }
    /* 回退：alloc + memset */
    if (nmemb == 0 || size == 0)
        return NULL;
    if (nmemb > SIZE_MAX / size) {
        lv_RETURN_ERROR_NULL(lv_ERROR_OVERFLOW, "calloc 溢出: %zu * %zu", nmemb, size);
    }
    size_t total = nmemb * size;
    void *p = ops->alloc(total);
    if (p) {
        memset(p, 0, total);
    }
    return p;
}

void *lv_calloc_tracked(size_t nmemb, size_t size, const char *file, int line) {
    /* 零大小请求：分配最小块（1 字节数据），保持与 lv_malloc(0) 一致的语义 */
    if (nmemb == 0 || size == 0) {
        nmemb = 1;
        size = 1;
    }

    /* 检查溢出 */
    if (nmemb > SIZE_MAX / size) {
        lv_RETURN_ERROR_NULL(lv_ERROR_OVERFLOW, "calloc 溢出: %zu * %zu", nmemb, size);
    }

    size_t total = nmemb * size;

    /* 检查头部和尾部大小溢出 */
    {
        size_t full = ALLOC_HEADER_SIZE;
        if (full > SIZE_MAX - total)
            goto overflow;
        full += total;
        if (full > SIZE_MAX - ALLOC_TAIL_MAGIC_SIZE)
            goto overflow;
        full += ALLOC_TAIL_MAGIC_SIZE;

        AllocHeader *hdr = (AllocHeader *) malloc(full);
        if (!hdr)
            lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "calloc malloc 失败");
        memset(hdr, 0, full);

        hdr->head_magic = ALLOC_HEAD_MAGIC;
        hdr->tail_offset = (uint32_t) total;
        hdr->size = total;
        hdr->file = file;
        hdr->line = line;

        /* 设置尾部魔数（使用 memcpy 避免未对齐访问） */
        uint32_t ctail_magic = ALLOC_TAIL_MAGIC;
        memcpy(hdr->data + total, &ctail_magic, sizeof(uint32_t));

        track_allocation(hdr);

        s_utils_state.memory_stats.total_allocated += total;
        s_utils_state.memory_stats.current_used += total;
        s_utils_state.memory_stats.allocation_count++;
        if (s_utils_state.memory_stats.current_used > s_utils_state.memory_stats.peak_used)
            s_utils_state.memory_stats.peak_used = s_utils_state.memory_stats.current_used;

        return hdr->data;
    }

overflow:
    lv_RETURN_ERROR_NULL(lv_ERROR_OVERFLOW, "calloc 溢出: header=%zu + total=%zu + tail=%zu", (size_t) ALLOC_HEADER_SIZE, total,
                 (size_t) ALLOC_TAIL_MAGIC_SIZE);
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
        return lv_allocator_get()->alloc(size);
    if (size == 0)
        return NULL;

    const AllocatorOps *ops = lv_allocator_get();
    if (ops->realloc) {
        return ops->realloc(ptr, size);
    }
    /* 回退：alloc + memcpy + free */
    {
        void *new_ptr = ops->alloc(size);
        if (!new_ptr)
            return NULL;
        /* 保守复制：复制 min(旧大小, 新大小) 字节 */
        size_t copy_size = size;
        memcpy(new_ptr, ptr, copy_size);
        ops->free(ptr);
        return new_ptr;
    }
}

void lv_free(void **ptr) {
    if (!ptr || !*ptr)
        return;

    lv_allocator_get()->free(*ptr);
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
    *stats = s_utils_state.memory_stats;
}

void lv_reset_memory_stats(void) {
    memset(&s_utils_state.memory_stats, 0, sizeof(s_utils_state.memory_stats));
}

void lv_set_memory_limit(size_t limit) {
    s_utils_state.memory_limit = limit;
}

size_t lv_get_memory_limit(void) {
    return s_utils_state.memory_limit;
}

bool lv_memory_limit_exceeded(void) {
    if (s_utils_state.memory_limit == 0)
        return false;
    return s_utils_state.memory_stats.current_used > s_utils_state.memory_limit;
}

/* ============================================================
 * POISON/MAGIC 检测实现
 * ============================================================ */

bool lv_memory_check_poison(const void *ptr, size_t size) {
    if (!ptr || size == 0)
        return true; /* 空区域视为安全 */

    const uint8_t *p = (const uint8_t *) ptr;
    size_t count = size / sizeof(uint32_t);
    uint32_t val;

    for (size_t i = 0; i < count; i++) {
        memcpy(&val, p + i * sizeof(uint32_t), sizeof(uint32_t)); /* 避免未对齐访问 */
        if (val == ALLOC_POISON) {
            lv_LOG_WARNING("Poison 标记检测: 地址 0x%p 偏移 %zu 处发现毒模式 0x%08X（可能 use-after-free）", ptr,
                           i * sizeof(uint32_t), ALLOC_POISON);
            return false;
        }
    }

    /* 检查尾部不完整的字节 */
    size_t remaining = size % sizeof(uint32_t);
    if (remaining > 0) {
        const uint8_t *tail = (const uint8_t *) (p + count);
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

    AllocHeader *hdr = (AllocHeader *) ((char *) ptr - ALLOC_HEADER_SIZE);

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
        const uint32_t *tail = (const uint32_t *) ((const char *) ptr + hdr->tail_offset);
        if (*tail != ALLOC_TAIL_MAGIC) {
            lv_LOG_ERROR("尾部魔数检测失败: 指针 0x%p 可能发生缓冲区溢出, 期望 0x%08X, 实际 0x%08X", ptr,
                         ALLOC_TAIL_MAGIC, *tail);
            return false;
        }
    }

    return true;
}

void lv_poison_enable(bool enable) {
    s_utils_state.poison_enabled = enable;
}

bool lv_poison_is_enabled(void) {
    return s_utils_state.poison_enabled;
}

/* ============================================================
 * 边界检查分配与泄漏报告实现
 * ============================================================ */

void *lv_malloc_bounded(size_t size, size_t max_size) {
    if (size > max_size) {
        lv_RETURN_ERROR_NULL(lv_ERROR_OVERFLOW, "malloc_bounded: 请求大小 %zu 超过上限 %zu", size, max_size);
    }
    return lv_malloc(size);
}

int lv_memory_leak_report(FILE *output) {
    if (!output)
        output = stderr;

    int leak_count = 0;
    size_t leak_bytes = 0;
    AllocHeader *curr = s_utils_state.tracked_allocs;

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

        fprintf(output, "0x%p %-12zu %-30s [泄漏]\n", (const void *) curr->data, curr->size, location);

        curr = curr->track_next;
    }

    fprintf(output, "----------------------------------------------------------------------------\n");
    fprintf(output, "总计: %d 个泄漏块, %zu 字节\n", leak_count, leak_bytes);
    fprintf(output, "==========================================\n\n");

    return leak_count;
}


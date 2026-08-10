/**
 * @file debug_refcount.c
 * @brief reference counting and GC
 * @details Split from debug.c
 */

#include "lv/lv_file.h"
#include "lv/lv_platform.h"
#include "lv/lv_thread.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "lv/engine.h"
#include "lv/lv_json.h"

#include "context.h"
#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "stream.h"
#include "stream_context_util.h"
#include "type_system.h"
#include "lv/lv_xmacro.h"
#include "lv/lv_strbuf.h"
#include "debug_internal.h"

/* ================================================================== */
/*  引用计数与垃圾回收实现                                              */
/* ================================================================== */

/**
 * @brief 增加对象的引用计数
 * @param obj 指向引用计数对象的指针（其第一个成员必须为 RefCounted）
 * @note 此函数是线程安全的，内部使用互斥锁保护引用计数操作
 *
 * [线程安全注意] 本函数使用全局互斥锁 debug_lock_refcount/debug_unlock_refcount
 * 保护 ref_count 的递增操作。由于该锁是全局的（复用 counter_mutex），在高并发场景下
 * 可能成为瓶颈。建议调用者：
 *   - 如果对同一对象的 inc/dec 操作频繁且集中在少数线程，当前实现已足够安全
 *   - 如果需要在多线程环境下高频操作大量不同对象，考虑改为 per-object 锁或使用
 *     C11 atomic_int / InterlockedIncrement 等原子操作以提升性能
 *   - 务必确保 RefCounted 是对象的第一个成员，否则强制类型转换将导致未定义行为
 */
void ref_count_inc(void *obj) {
    if (!obj)
        return;
    RefCounted *rc = (RefCounted *) obj;
    debug_lock_refcount();
    rc->ref_count++;
    debug_unlock_refcount();
}

/**
 * 【引用计数递减 —— 线程安全详细文档】[线程安全注意]
 *
 * 减少引用计数，到0时自动销毁对象。
 *
 * 操作语义：
 *   此函数对 RefCounted 结构体的 ref_count 字段执行三个逻辑操作：
 *     1. READ:  读取当前 ref_count 值
 *     2. MODIFY: 将值减 1
 *     3. WRITE: 写回修改后的值
 *   但在当前实现中，这三个操作是通过 C 语言的 `rc->ref_count--` 完成的，
 *   这是一个非原子的复合操作（在绝大多数平台上对应 3 条机器指令）。
 *
 * 竞态条件场景（多线程环境）：
 *   假设两个线程 T1 和 T2 同时对同一对象调用 ref_count_dec：
 *
 *   时间线:
 *     T1: 读取 ref_count = 2
 *     T2: 读取 ref_count = 2          ← 两个线程都读到 2
 *     T1: 递减 → 1，写回
 *     T2: 递减 → 1，写回              ← 应该是 1（预期）→ 实际也是 1（侥幸正确）
 *
 *   危险时间线（ref_count = 1 时）：
 *     T1: 读取 ref_count = 1
 *     T2: 读取 ref_count = 1          ← 两个线程都读到 1
 *     T1: 递减 → 0，调用 destructor，对象被释放
 *     T2: 递减 → 0，调用 destructor   ← DOUBLE FREE! use-after-free!
 *
 * 调用者防护要求：
 *   - 首选方案：使用外部互斥锁保护对同一对象的所有 ref_count_inc/ref_count_dec 调用
 *   - 次选方案：确保同一对象的引用计数操作仅发生在单一线程中
 *   - 妥协方案：确认该对象在线程间传递时使用"转移语义"（发送方 dec，接收方 inc），
 *     而非"共享语义"（多线程同时持有引用）
 *   - 如果需要在多线程环境下安全地共享引用计数，应在 RefCounted 中改用
 *     C11 atomic_int 或平台特定的原子操作（Windows: InterlockedDecrement，
 *     POSIX: __atomic_sub_fetch）
 *
 * 注意：此函数的参数类型为 void*，内部通过强制类型转换访问 RefCounted 字段。
 * 这要求所有使用引用计数的结构体必须以 RefCounted 作为第一个成员，
 * 且 destructor 字段必须指向正确的销毁函数。
 *
 * @param obj   指向引用计数对象的指针（其第一个成员必须为 RefCounted）
 * @return true  表示对象已被销毁（ref_count 降为 0 并调用了 destructor）
 * @return false 表示对象仍然存活（ref_count > 0），或 obj 为 NULL，
 *               或 ref_count 已经为 0（无效调用，不执行任何操作）
 */
bool ref_count_dec(void *obj) {
    if (!obj)
        return false;
    RefCounted *rc = (RefCounted *) obj;

    /* 使用互斥锁保护引用计数操作，防止多线程竞态条件。
     * 修复：将析构函数调用也放在锁内执行，避免以下竞态场景：
     *   T1: 读取 ref_count=1 → 递减为0 → 解锁
     *   T2: 读取 ref_count=0 → 解锁（返回 false）
     *   T1: 调用 destructor → 释放对象
     *   T2: 此时对象已被释放，若 T2 在解锁前已缓存了 obj 指针，则可能 use-after-free。
     * 将 destructor 调用保持在锁内，确保同一时刻只有一个线程能触发销毁。 */
    debug_lock_refcount();
    if (rc->ref_count <= 0) {
        debug_unlock_refcount();
        return false;
    }
    rc->ref_count--;
    if (rc->ref_count <= 0) {
        /* 引用计数降为零，在锁内调用析构函数并销毁对象，
         * 防止多线程同时触发双重释放（TOCTOU 窗口已消除）。
         * 前置条件：destructor 不得调用 ref_count_inc/ref_count_dec/ref_count_get
         * —— debug_refcount_lock 是全局互斥锁且不可重入，destructor 内重入
         * 上述 API 将造成死锁。 */
        void (*destructor)(void *) = rc->destructor;
        rc->destructor = NULL; /* 置空防止重复调用 */
        if (destructor) {
            destructor(obj);
        }
        debug_unlock_refcount();
        return true; /* 对象已销毁 */
    }
    debug_unlock_refcount();
    return false;
}

/**
 * @brief 获取对象的当前引用计数
 * @param obj 指向引用计数对象的指针（其第一个成员必须为 RefCounted）
 * @return 当前引用计数值，obj 为 NULL 时返回 0
 * @note 此函数未加锁，返回值仅供参考，在多线程环境下可能已过时
 */
int ref_count_get(const void *obj) {
    if (!obj)
        return 0;
    const RefCounted *rc = (const RefCounted *) obj;
    return rc->ref_count;
}

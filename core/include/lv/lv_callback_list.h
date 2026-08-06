/**
 * @file lv_callback_list.h
 * @brief 泛型回调列表 —— 注册/注销/分发公共设施
 *
 * @details 从 stream（CallbackEntry 数组，stream_context.c）与 event_bus
 *          （lvEventSubscription 数组，lv_event_bus.c）提取的公共回调列表设施，
 *          消除各模块手写的「回调数组 + 注册/注销/分发」样板。
 *
 *          设计要点（对齐 stream_context.c 蓝本）：
 *          - 条目：回调函数指针 + 用户数据 + 自增 ID + 过滤值
 *          - 注册返回自增 ID（>= 1），注销按 ID 或按函数指针
 *          - 分发迭代安全：快照遍历时的 count + 每次迭代越界检查，
 *            遍历中注册/注销均安全（注销采用前移紧凑，不导致崩溃）
 *          - 注销按注册顺序前移紧凑（与 stream 蓝本一致）
 *          - 无锁：线程安全策略由调用方决定（与蓝本一致，
 *            涉及线程安全的消费点保持其现有锁语义）
 *
 *          泛型方式（与项目现有风格一致：void* + 转换）：
 *          条目中的回调以 lvCallbackFn（void (*)(void)）形式存储。
 *          C11 6.3.2.3p8 保证函数指针可转换为其他函数指针类型并转回原类型，
 *          分发时由调用方提供 invoke 函数将条目回调转回真实签名后调用。
 *
 * @author Lv-00 Project
 * @version 1.0.0
 * @date   2026-08-06
 */
#ifndef lv_LV_CALLBACK_LIST_H
#define lv_LV_CALLBACK_LIST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* lv_PUBLIC_API —— 若未定义则提供默认实现 */
#ifndef lv_PUBLIC_API
#if defined(_WIN32) || defined(_MSC_VER)
#ifdef lv_BUILD_SHARED
#define lv_PUBLIC_API __declspec(dllexport)
#else
#define lv_PUBLIC_API
#endif
#elif defined(__GNUC__) || defined(__clang__)
#ifdef lv_BUILD_SHARED
#define lv_PUBLIC_API __attribute__((visibility("default")))
#else
#define lv_PUBLIC_API
#endif
#else
#define lv_PUBLIC_API
#endif
#endif

/** @brief 回调列表默认初始容量 */
#define lv_CALLBACK_LIST_DEFAULT_CAPACITY 16

/**
 * @brief 泛型回调函数指针存储类型
 *
 * 存储时由调用方将真实签名的函数指针强转为 lvCallbackFn，
 * 分发时由调用方在 invoke 函数中转回真实签名后调用。
 * C11 6.3.2.3p8 保证该往返转换安全且结果与原指针相等。
 */
typedef void (*lvCallbackFn)(void);

/**
 * @brief 回调条目
 */
typedef struct lvCallbackEntry {
    lvCallbackFn callback; /**< 回调函数指针（泛型存储，分发时转回真实签名） */
    void *user_data;       /**< 回调透传数据 */
    int id;                /**< 自增回调 ID（>= 1），用于按 ID 注销/更新过滤值 */
    uint64_t filter;       /**< 调用方定义的过滤值（如事件类型位掩码或事件类型 ID） */
} lvCallbackEntry;

/**
 * @brief 回调列表
 *
 * 回调数组支持动态扩容，最多扩容到 max_entries 硬上限。
 * 超过硬上限后注册失败。
 */
typedef struct lvCallbackList {
    lvCallbackEntry *entries; /**< 回调条目数组（堆分配，支持动态扩容） */
    int count;                /**< 当前回调数量 */
    int capacity;             /**< 当前数组容量 */
    int next_id;              /**< 下一个回调 ID（自增，从 1 开始） */
    int max_entries;          /**< 硬上限（0 = 无限制），超出后注册失败 */
} lvCallbackList;

/* ==================== 生命周期 ==================== */

/**
 * @brief 初始化回调列表
 *
 * 预分配 initial_capacity 容量的条目数组（分配失败不致命，
 * 后续注册时惰性扩容兜底）。
 *
 * @param list             回调列表指针
 * @param initial_capacity 初始容量（<= 0 时使用默认值 16）
 * @param max_entries      硬上限（0 = 无限制）
 */
lv_PUBLIC_API void lv_callback_list_init(lvCallbackList *list, int initial_capacity, int max_entries);

/**
 * @brief 释放回调列表资源
 *
 * 释放条目数组内存并清零列表。已注册的回调不再被调用。
 *
 * @param list 回调列表指针（可为 NULL）
 */
lv_PUBLIC_API void lv_callback_list_cleanup(lvCallbackList *list);

/**
 * @brief 清空回调列表（保留容量，不释放数组内存）
 *
 * @param list 回调列表指针
 */
lv_PUBLIC_API void lv_callback_list_clear(lvCallbackList *list);

/* ==================== 注册 / 注销 ==================== */

/**
 * @brief 注册回调
 *
 * @param list      回调列表
 * @param callback  回调函数指针（非 NULL）
 * @param user_data 用户数据（透传给回调）
 * @param filter    过滤值（调用方定义，分发时传给过滤函数）
 * @return 回调 ID（>= 1），失败返回 -1（参数无效、回调已满或内存不足）
 */
lv_PUBLIC_API int lv_callback_list_add(lvCallbackList *list, lvCallbackFn callback, void *user_data,
                                       uint64_t filter);

/**
 * @brief 按回调 ID 注销
 *
 * 将后续回调前移一位以保持数组紧凑与注册顺序。
 *
 * @param list 回调列表
 * @param id   注册时返回的回调 ID
 * @return true 成功，false 未找到或参数无效
 */
lv_PUBLIC_API bool lv_callback_list_remove_by_id(lvCallbackList *list, int id);

/**
 * @brief 按函数指针注销
 *
 * 将后续回调前移一位以保持数组紧凑与注册顺序。
 *
 * @param list     回调列表
 * @param callback 要注销的回调函数指针
 * @return true 成功，false 未找到或参数无效
 */
lv_PUBLIC_API bool lv_callback_list_remove_by_fn(lvCallbackList *list, lvCallbackFn callback);

/**
 * @brief 更新回调的过滤值
 *
 * @param list   回调列表
 * @param id     注册时返回的回调 ID
 * @param filter 新的过滤值
 * @return true 成功，false 未找到对应回调
 */
lv_PUBLIC_API bool lv_callback_list_set_filter(lvCallbackList *list, int id, uint64_t filter);

/**
 * @brief 获取回调的过滤值
 *
 * @param list 回调列表
 * @param id   注册时返回的回调 ID
 * @return 过滤值，未找到时返回 0
 */
lv_PUBLIC_API uint64_t lv_callback_list_get_filter(const lvCallbackList *list, int id);

/* ==================== 查询 ==================== */

/**
 * @brief 获取当前回调数量
 *
 * @param list 回调列表（可为 NULL）
 * @return 回调数量，list 为 NULL 时返回 0
 */
lv_PUBLIC_API int lv_callback_list_count(const lvCallbackList *list);

/* ==================== 分发 ==================== */

/**
 * @brief 分发过滤函数类型
 *
 * @param entry        回调条目（含 filter 过滤值）
 * @param dispatch_arg 分发参数（调用方定义，如事件数据指针）
 * @return true 调用该回调，false 跳过
 */
typedef bool (*lvCallbackFilterFn)(const lvCallbackEntry *entry, const void *dispatch_arg);

/**
 * @brief 分发调用函数类型
 *
 * 负责将条目中的泛型回调（lvCallbackFn）转回调用方的真实签名并调用。
 *
 * @param entry        回调条目（回调以 lvCallbackFn 存储）
 * @param dispatch_arg 分发参数（调用方定义）
 */
typedef void (*lvCallbackInvokeFn)(const lvCallbackEntry *entry, const void *dispatch_arg);

/**
 * @brief 分发：遍历回调列表，按注册顺序调用过滤匹配的回调
 *
 * 迭代安全：快照遍历开始时的 count；每次迭代检查索引是否越界
 * （回调可能在 invoke 中注册/注销导致数组变化）。注销采用前移紧凑，
 * 遍历中注销不会导致越界崩溃（与 stream 蓝本 stream_dispatch 行为一致）。
 *
 * @param list         回调列表
 * @param dispatch_arg 分发参数（透传给 filter/invoke）
 * @param filter       过滤函数（可为 NULL，表示全部调用）
 * @param invoke       调用函数（可为 NULL，表示只遍历不调用）
 */
lv_PUBLIC_API void lv_callback_list_dispatch(lvCallbackList *list, const void *dispatch_arg,
                                             lvCallbackFilterFn filter, lvCallbackInvokeFn invoke);

#ifdef __cplusplus
}
#endif

#endif /* lv_LV_CALLBACK_LIST_H */

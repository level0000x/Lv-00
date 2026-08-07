/**
 * @file lv_lifecycle.h
 * @brief 复合对象生命周期管理与作用域守卫
 *
 * @details 为消除各模块中同构的逐字段销毁样板与手写 goto cleanup 提供公共基元：
 *
 *          1. 字段销毁描述表（lvFieldDesc / lv_obj_destroy_fields）：
 *             以"描述表 + 通用实现"收敛 graph_destroy / engine_destroy /
 *             axiom_package_destroy / lv_context_destroy 等 40~70 行的
 *             同构销毁函数。描述表按声明顺序逐字段释放，全部置 NULL 安全，
 *             支持：指针字段直接释放（PLAIN_FREE）、lvDArray 释放
 *             （DARRAY_FREE / 逐元素回调后释放 DARRAY_ELEMS）、对象回调释放
 *             （OBJECT）、指针数组逐元素回调后释放（ARRAY_ELEMS）、
 *             自定义清理（CUSTOM，如引用计数递减）。
 *
 *          2. 作用域守卫宏族（lv_DEFER / lv_SCOPE_EXIT / lv_DEFER_FREE /
 *             lv_DEFER_FREE_MANY）：
 *             基于 GCC/Clang __attribute__((cleanup)) 实现，注册的清理在
 *             所在作用域退出（含任意 return / goto）时逆序（LIFO）自动执行，
 *             用于收敛 goto cleanup 模式的单出口样板。
 *
 *          实现选择与代码库既有风格一致：
 *          - 复用 lv_utils.h 已有的 lvDeferSlot / lv_defer_slot_cleanup /
 *            lv_defer_free_ptr（GCC cleanup 属性 + 槽位记录）；
 *          - lv_DEFER 变量名用 __COUNTER__ 唯一化，突破 lv_utils.h 中
 *            LV_DEFER 宏"同一作用域仅可注册一个"的限制；
 *          - MSVC 等不支持 cleanup 属性的编译器下展开为空操作
 *            （与 lv_utils.h 的 LV_DEFER 行为一致），需回退 goto cleanup 模式。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#ifndef lv_LV_LIFECYCLE_H
#define lv_LV_LIFECYCLE_H

#include <stddef.h>

#include "lv/lv_utils.h" /* lvDeferSlot / lvDeferFn / lv_defer_slot_cleanup /
                          * lv_defer_free_ptr / lv_free / lvDArray / lv_PUBLIC_API */

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 一、作用域守卫（defer）宏族
 * ============================================================ */

/** @brief 宏内联拼接辅助（供唯一变量名生成使用） */
#define lv_LIFECYCLE_CAT_(a, b) a##b
#define lv_LIFECYCLE_CAT(a, b) lv_LIFECYCLE_CAT_(a, b)

/**
 * @brief 注册一个作用域退出清理（类似 C 的 defer）
 *
 * 在所在作用域退出（含任意 return / goto）时，按注册逆序（后注册先执行）
 * 自动调用 cleanup_fn(arg)。同一作用域内可注册多个（变量名 __COUNTER__ 唯一化）。
 *
 * 用法：
 * @code
 *   int *buf = lv_malloc(100);
 *   lv_DEFER(lv_defer_free_ptr, &buf);
 *   if (cond)
 *       return -1;   // buf 在返回时自动释放
 * @endcode
 *
 * @param cleanup_fn 清理回调（void (*)(void *arg)）
 * @param arg        传给回调的参数
 */
#if defined(__GNUC__) || defined(__clang__)
#define lv_DEFER(cleanup_fn, arg)                                                                    \
    lvDeferSlot lv_LIFECYCLE_CAT(_lv_lifecycle_defer_, __COUNTER__)                                  \
        __attribute__((cleanup(lv_defer_slot_cleanup))) = {(lvDeferFn) (cleanup_fn), (void *) (arg)}
#else
#define lv_DEFER(cleanup_fn, arg) ((void) (cleanup_fn), (void) (arg))
#endif

/**
 * @brief 作用域退出清理注册（lv_DEFER 的语义化别名）
 *
 * 与 lv_DEFER 完全等价：cleanup_fn(arg) 在所在作用域退出（含任意 return / goto）
 * 时逆序自动执行。命名上用于强调"在作用域出口执行清理"的语义。
 *
 * @param cleanup_fn 清理回调（void (*)(void *arg)）
 * @param arg        传给回调的参数
 */
#define lv_SCOPE_EXIT(cleanup_fn, arg) lv_DEFER(cleanup_fn, arg)

/**
 * @brief 便捷宏：注册单个指针变量在作用域退出时的自动释放
 *
 * 用法：lv_DEFER_FREE(buf);   （buf 为指针变量，此处自动取地址）
 */
#define lv_DEFER_FREE(ptr) lv_DEFER(lv_defer_free_ptr, &(ptr))

/** @brief lv_DEFER_FREE 的语义化别名（作用域出口释放） */
#define lv_SCOPE_EXIT_FREE(ptr) lv_DEFER_FREE(ptr)

/**
 * @brief 便捷宏：批量注册多个指针变量在作用域退出时的自动释放
 *
 * 通过 NULL 哨兵指针数组（复合字面量）将多个指针变量的地址一次注册，
 * 作用域退出时按数组顺序逐个 lv_free 并置 NULL。
 *
 * 用法：lv_DEFER_FREE_MANY(&buf1, &buf2, &buf3);
 */
#define lv_DEFER_FREE_MANY(...) lv_DEFER(lv_free_many_deferred, &((void *[]){__VA_ARGS__, NULL}))

/**
 * @brief 批量释放清理回调（供 lv_DEFER_FREE_MANY 使用）
 *
 * @param arg 指向 NULL 哨兵结尾的 void** 数组（元素为各指针变量的地址）
 * @note 也可直接配合 lv_DEFER(lv_free_many_deferred, &(void *[]){...}) 使用。
 */
static inline void lv_free_many_deferred(void *arg) {
    void **list = (void **) arg;
    if (!list)
        return;
    while (*list) {
        lv_free((void **) *list);
        list++;
    }
}

/* ============================================================
 * 二、复合对象字段销毁描述表
 *
 * 用法：为对象的每个需释放字段声明一条 lvFieldDesc 条目，然后调用
 *       lv_obj_destroy_fields(obj, descs, 条目数) 统一执行。
 *       条目按声明顺序逐字段释放（指针字段置 NULL，lvDArray 归零）。
 * ============================================================ */

/** @brief 字段释放方式 */
typedef enum lvFieldKind {
    LV_FIELD_PLAIN_FREE,    /**< 指针字段：lv_free((void **)field)（NULL 安全，释放后置 NULL） */
    LV_FIELD_DARRAY_FREE,   /**< lvDArray 字段：lv_darray_free（元素为平凡值，无需逐元素销毁） */
    LV_FIELD_DARRAY_ELEMS,  /**< lvDArray 字段：逐元素调用 elem_destroy 后 lv_darray_free */
    LV_FIELD_OBJECT,        /**< 对象指针字段：调用 object_destroy(字段值) 后置 NULL */
    LV_FIELD_ARRAY_ELEMS,   /**< 指针数组字段：逐元素调用 elem_destroy 后 lv_free 数组本身
                                 （元素个数由 count_offset 指向的 int 字段给出） */
    LV_FIELD_CUSTOM         /**< 自定义清理：custom_fn(obj, field_ptr)（如引用计数递减） */
} lvFieldKind;

/** @brief 字段销毁描述条目 */
typedef struct lvFieldDesc {
    const char *name;   /**< 字段名（诊断/日志用） */
    lvFieldKind kind;   /**< 释放方式 */
    size_t offset;      /**< 字段在对象中的字节偏移（offsetof，可复合 offsetof 计算嵌套字段） */
    size_t count_offset; /**< 仅 LV_FIELD_ARRAY_ELEMS：元素数字段（int）的字节偏移，其余填 0 */
    union {
        void (*object_destroy)(void *obj);        /**< LV_FIELD_OBJECT：销毁字段指向的对象 */
        void (*elem_destroy)(void *elem);         /**< LV_FIELD_DARRAY_ELEMS / ARRAY_ELEMS：销毁单个元素 */
        void (*custom_fn)(void *obj, void *field_ptr); /**< LV_FIELD_CUSTOM：自定义清理 */
    } u;
} lvFieldDesc;

/** @brief 便捷构造宏：指针字段直接释放 */
#define lv_FIELD_PLAIN(obj_ty, field) \
    {#field, LV_FIELD_PLAIN_FREE, offsetof(obj_ty, field), 0, {NULL}}

/** @brief 便捷构造宏：lvDArray 字段整体释放（元素为平凡值） */
#define lv_FIELD_DARRAY(obj_ty, field) \
    {#field, LV_FIELD_DARRAY_FREE, offsetof(obj_ty, field), 0, {NULL}}

/** @brief 便捷构造宏：lvDArray 字段逐元素销毁后整体释放 */
#define lv_FIELD_DARRAY_ELEMS(obj_ty, field, elem_destroy_fn) \
    {#field, LV_FIELD_DARRAY_ELEMS, offsetof(obj_ty, field), 0, {.elem_destroy = (elem_destroy_fn)}}

/** @brief 便捷构造宏：对象指针字段调用其 destroy 回调后置 NULL */
#define lv_FIELD_OBJECT(obj_ty, field, object_destroy_fn) \
    {#field, LV_FIELD_OBJECT, offsetof(obj_ty, field), 0, {.object_destroy = (object_destroy_fn)}}

/** @brief 便捷构造宏：指针数组字段逐元素销毁后释放数组本身 */
#define lv_FIELD_ARRAY(obj_ty, field, count_field, elem_destroy_fn) \
    {#field, LV_FIELD_ARRAY_ELEMS, offsetof(obj_ty, field), offsetof(obj_ty, count_field), {.elem_destroy = (elem_destroy_fn)}}

/** @brief 便捷构造宏：自定义清理（custom_fn(void *obj, void *field_ptr)） */
#define lv_FIELD_CUSTOM(obj_ty, field, cleanup_fn) \
    {#field, LV_FIELD_CUSTOM, offsetof(obj_ty, field), 0, {.custom_fn = (cleanup_fn)}}

/**
 * @brief 按字段描述表统一销毁复合对象的全部资源
 *
 * 按描述表声明顺序逐字段释放：PLAIN_FREE 释放指针并置 NULL；
 * DARRAY_FREE / DARRAY_ELEMS 释放 lvDArray（元素非平凡时先逐元素回调）；
 * OBJECT 调用对象自身 destroy 回调后置 NULL；ARRAY_ELEMS 逐元素回调后
 * 释放数组指针；CUSTOM 交由调用方提供的回调处理（如引用计数递减）。
 * 所有指针字段释放后均置 NULL，重复调用安全。
 *
 * @param obj    对象指针（NULL 安全）
 * @param fields 字段描述表
 * @param n      描述表条目数
 */
void lv_obj_destroy_fields(void *obj, const lvFieldDesc *fields, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* lv_LV_LIFECYCLE_H */

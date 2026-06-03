/**
 * @file stream_context_util.h
 * @brief 流式上下文工具 —— 注册/分发机制 + 消除各模块中的重复样板代码
 *
 * @details 各模块需要独立的线程局部 StreamContext 指针和 setter 函数。
 *          此文件提供注册/分发机制，使引擎无需硬编码每个模块的 setter 调用，
 *          同时提供统一宏，确保所有模块使用一致的命名规范和线程安全模式。
 *
 *          新增模块时只需在其 .c 文件中调用 LV00_DECLARE_STREAM_CTX(prefix)，
 *          并在该文件末尾调用 LV00_REGISTER_STREAM_CTX(prefix) 完成自注册，
 *          无需修改引擎初始化代码。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */
#ifndef LV00_STREAM_CONTEXT_UTIL_H
#define LV00_STREAM_CONTEXT_UTIL_H

/* 精确依赖：
 *   - LV00_THREAD_LOCAL 宏来自 lv00.h 的第零层定义，但不导入整个 lv00.h
 *   - lv00.h 已在 include 链中通过其他头文件间接包含，无需重复引入
 *   - 仅引入 stream.h 获取 StreamContext 类型
 */
#include <stddef.h>

#include "lv00.h"
#include "stream.h"

/* [P1 修复] 添加 extern "C" 保护，确保 C++ 编译器能正确链接此头文件 */
#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 流式上下文 Setter 回调类型
 * ================================================================ */

/**
 * @brief 流式上下文设置回调函数指针类型
 *
 * 每个模块提供一个符合此签名的函数，用于接收引擎分配的
 * StreamContext 指针。注册到分发系统后，引擎可通过一次
 * stream_context_dispatch_all() 调用同步所有模块。
 *
 * 此签名与现有的 xxx_set_stream_context(StreamContext *ctx)
 * 函数完全兼容，无需额外包装或类型转换。
 */
typedef void (*StreamContextSetter)(StreamContext *ctx);

/* ================================================================
 * 注册/分发 API
 * ================================================================ */

/**
 * @brief 注册一个流式上下文 setter 到全局注册表
 *
 * 将模块提供的 setter 回调注册到分发系统中，供后续
 * stream_context_dispatch_all() 遍历调用。重复注册同一个
 * setter 函数指针是安全的（内部使用静态数组，不会重复添加）。
 *
 * 此函数在初始化期一次性调用，不需要线程安全保护。
 *
 * @param setter  模块的流式上下文设置函数指针（不可为 NULL）
 */
LV00_PUBLIC_API void stream_context_register_setter(StreamContextSetter setter);

/**
 * @brief 向所有已注册模块分发流式上下文
 *
 * 遍历全局注册表中的所有 setter 回调，依次调用 setter(ctx)，
 * 将流式上下文同步到各个模块。如果尚无任何模块注册，此调用
 * 为空操作。
 *
 * @param ctx  要分发的 StreamContext 指针（可为 NULL 以清空所有模块）
 */
LV00_PUBLIC_API void stream_context_dispatch_all(void *ctx);

/**
 * @brief 注册所有内置模块的流式上下文 setter（一次性初始化）
 *
 * 在引擎首次创建时调用。此函数将 solver、rewrite、unify、
 * func_block、type_system、proof、recursion、normalization、
 * prop_verifier、graph 等核心模块的 setter 注册到全局分发系统。
 *
 * 使用 static 标志确保多次调用只执行一次注册。
 * 新增模块时在此函数的实现中追加对应的注册行即可。
 */
LV00_PUBLIC_API void stream_context_register_builtins(void);

/* ================================================================
 * 模块级流式上下文宏
 * ================================================================ */

/**
 * @brief 声明模块级流式上下文变量和 setter 函数
 *
 * 此宏展开后生成：
 *   1. 一个线程局部 static StreamContext 指针
 *   2. 一个 public setter 函数（void prefix_set_stream_context(StreamContext *ctx)）
 *
 * 新增模块后，在其 .c 文件中调用此宏即可获得流式上下文支持。
 *
 * @warning **static 变量多副本警告**:
 *   此宏展开后会生成一个 static 变量和函数定义。如果在多个 .c 文件中
 *   使用相同的 prefix 调用此宏，每个编译单元会有自己独立的 static
 *   副本，导致 setter 设置的指针在跨编译单元时不可见。每当需要跨
 *   编译单元共享 StreamContext 时，必须在唯一的 .c 文件中调用此宏，
 *   并在头文件中仅声明 extern 版本。
 *
 *   **推荐做法**：只在 .c 文件中使用此宏，不要在头文件中调用。
 *   若模块需要在多个 .c 文件间共享上下文，应在模块的公共 .c 文件
 *   （如 module_main.c）中调用一次，然后在模块头文件中手动声明
 *   extern StreamContext *prefix##_stream_ctx;
 *   void prefix##_set_stream_context(StreamContext *ctx);
 *
 * @param prefix 模块前缀（如 solver, rewrite, proof）
 */
#define LV00_DECLARE_STREAM_CTX(prefix)                                 \
    static LV00_THREAD_LOCAL StreamContext *prefix##_stream_ctx = NULL; \
    void prefix##_set_stream_context(StreamContext *ctx) {              \
        prefix##_stream_ctx = ctx;                                      \
    }

/**
 * @brief 将模块的流式上下文 setter 注册到全局分发系统
 *
 * 使用方式：在模块 .c 文件的 LV00_DECLARE_STREAM_CTX(prefix) 之后，
 * 在文件末尾调用此宏。由于 C11 标准限制，此宏需放置在函数体内部
 * （推荐在某个模块初始化函数中调用），或在支持编译器扩展的环境下
 * 辅助自动注册。
 *
 * 实际注册通过调用 stream_context_register_setter() 完成。
 *
 * @param prefix 模块前缀（必须与 LV00_DECLARE_STREAM_CTX 的 prefix 一致）
 */
#define LV00_REGISTER_STREAM_CTX(prefix) stream_context_register_setter(prefix##_set_stream_context)

/**
 * @brief 在头文件中声明 extern 版本的流式上下文变量
 * 
 * 与 LV00_DECLARE_STREAM_CTX 配套使用。在头文件中使用此宏声明 extern 变量，
 * 在一个 .c 文件中使用 LV00_DECLARE_STREAM_CTX 定义实际变量。
 *
 * @param type  上下文类型（如 StreamContext*）
 * @param name  变量名
 */
#define LV00_DECLARE_STREAM_CTX_EXTERN(type, name) extern type name

#ifdef __cplusplus
}
#endif

#endif /* LV00_STREAM_CONTEXT_UTIL_H */

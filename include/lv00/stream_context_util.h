/**
 * @file stream_context_util.h
 * @brief 流式上下文工具宏 —— 消除各模块中的重复样板代码
 *
 * @details 各模块需要独立的线程局部StreamContext指针和setter函数。
 *          此文件提供统一宏，确保所有模块使用一致的命名规范和线程安全模式。
 *
 * @author Lv-00 Project
 * @version 3.0.1
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

/**
 * @brief 声明模块级流式上下文变量和setter函数
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
#define LV00_DECLARE_STREAM_CTX(prefix) \
    static LV00_THREAD_LOCAL StreamContext *prefix##_stream_ctx = NULL; \
    void prefix##_set_stream_context(StreamContext *ctx) { \
        prefix##_stream_ctx = ctx; \
    }

#endif /* LV00_STREAM_CONTEXT_UTIL_H */

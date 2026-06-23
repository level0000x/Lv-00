/**
 * @file stream_context_util.c
 * @brief 流式上下文注册/分发机制的实现
 *
 * @details 提供全局注册表，各模块通过 stream_context_register_setter()
 *          将其 setter 函数注册进来，引擎则通过 stream_context_dispatch_all()
 *          一次性同步流式上下文到所有已注册模块。
 *
 *          注册表使用静态数组实现，在初始化期一次性填充，无需线程安全保护。
 *          内置模块的注册通过 stream_context_register_builtins() 集中完成，
 *          该函数在引擎初始化时被调用一次。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "stream_context_util.h"

#include <stddef.h>

/* ---- 依赖前向声明：内置模块的 setter 函数 ---- */

/* 各模块已在对应头文件中声明了 void xxx_set_stream_context(StreamContext *ctx)。
 * 由于 StreamContextSetter 的签名就是 void (*)(StreamContext *)，各 setter
 * 函数与此类型完全兼容，无需强制转换。 */

#include "lv00/constraint_graph.h" /* graph_set_stream_context   */
#include "func_block.h"       /* func_block_set_stream_context */
#include "normalization.h"    /* normalization_set_stream_context*/
#include "lv00/proof.h"            /* proof_set_stream_context      */
#include "prop_verifier.h"    /* prop_verifier_set_stream_context*/
#include "recursion.h"        /* recursion_set_stream_context  */
#include "rewrite.h"          /* rewrite_set_stream_context    */
#include "lv00/solver.h"           /* solver_set_stream_context     */
#include "type_system.h"      /* type_system_set_stream_context*/
#include "unify.h"            /* unify_set_stream_context      */

/* ================================================================
 * 全局注册表（编译期初始化，运行期只读追加）
 * ================================================================ */

/** @brief 注册表最大容量，预留足够空间供未来扩展 */
#define MAX_REGISTERED_SETTERS 32

/** @brief 已注册的 setter 回调数组 */
static StreamContextSetter registered_setters[MAX_REGISTERED_SETTERS];

/** @brief 当前已注册的 setter 数量 */
static int registered_count = 0;

/* ================================================================
 * 公开 API 实现
 * ================================================================ */

void stream_context_register_setter(StreamContextSetter setter) {
    /* 参数校验：setter 不能为空 */
    if (!setter)
        return;

    /* 去重检查：避免重复注册同一个 setter */
    for (int i = 0; i < registered_count; i++) {
        if (registered_setters[i] == setter) {
            return; /* 已注册，跳过 */
        }
    }

    /* 容量检查：防止越界 */
    if (registered_count >= MAX_REGISTERED_SETTERS) {
        return; /* 注册表已满，静默丢弃（实际场景不会达到上限） */
    }

    /* 追加到注册表末尾 */
    registered_setters[registered_count++] = setter;
}

void stream_context_dispatch_all(void *ctx) {
    /* 将 void * 转为 StreamContext *，与 setter 签名匹配 */
    StreamContext *sc = (StreamContext *) ctx;
    /* 遍历已注册的 setter 数组，依次调用 */
    for (int i = 0; i < registered_count; i++) {
        if (registered_setters[i]) {
            registered_setters[i](sc);
        }
    }
}

/* ================================================================
 * 内置模块批量注册
 *
 * 集中注册所有内置模块的流式上下文 setter。此函数在引擎初始化
 * 时被调用一次，之后新增模块只需在此处添加一行注册调用，
 * 无需修改引擎初始化代码（engine.c）。
 *
 * 各模块 setter 的声明来自各自头文件，参数类型为 StreamContext *。
 * 通过 StreamContextSetter 强制转换统一签名，因为 StreamContext *
 * 与 void * 可以安全地互相转换（C 标准保证对象指针与 void* 的
 * 往返转换是安全的）。
 * ================================================================ */

/**
 * @brief 注册所有内置模块的流式上下文 setter
 *
 * 在引擎首次创建时调用。此函数将各个核心模块的 setter 注册到
 * 全局分发系统中，之后 stream_context_dispatch_all() 即可一次性
 * 将流式上下文同步到所有模块。
 *
 * 新增模块时，在此函数末尾追加一行 stream_context_register_setter()
 * 调用即可，无需修改 engine.c。
 */
void stream_context_register_builtins(void) {
    /* 使用 static 标志确保只注册一次 */
    static int builtins_registered = 0;
    if (builtins_registered)
        return;
    builtins_registered = 1;

    /* ---- 核心求解/变换模块 ---- */
    stream_context_register_setter(solver_set_stream_context);
    stream_context_register_setter(rewrite_set_stream_context);
    stream_context_register_setter(unify_set_stream_context);

    /* ---- 函数块系统 ---- */
    stream_context_register_setter(func_block_set_stream_context);

    /* ---- 类型系统 ---- */
    stream_context_register_setter(type_system_set_stream_context);

    /* ---- 证明系统 ---- */
    stream_context_register_setter(proof_set_stream_context);

    /* ---- 递归 ---- */
    stream_context_register_setter(recursion_set_stream_context);

    /* ---- 规范化 ---- */
    stream_context_register_setter(normalization_set_stream_context);

    /* ---- 命题验证器 (信任颜色桥接) ---- */
    stream_context_register_setter(prop_verifier_set_stream_context);

    /* ---- 约束图模块 ---- */
    stream_context_register_setter(graph_set_stream_context);
}

/**
 * @file stream_context_util.c
 * @brief 流式上下文注册/分发机制的实现
 *
 * @details 提供全局注册表，各模块通过 stream_context_register_setter()
 *          将其 setter 函数注册进来，引擎则通过 stream_context_dispatch_all()
 *          一次性同步流式上下文到所有已注册模块。
 *
 *          注册表基于公共 lv_callback_list 回调列表设施实现（lv_once 惰性
 *          初始化），在初始化期一次性填充，无需线程安全保护。
 *          内置模块的注册通过 stream_context_register_builtins() 集中完成，
 *          该函数在引擎初始化时被调用一次。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv/stream.h"

#include <stddef.h>

#include <stdarg.h>

#include "lv/lv_callback_list.h" /* 公共回调列表设施（setter 注册表委托实现） */
#include "lv/lv_thread.h"

#include "lv/lv_strbuf.h"
#include "lv/runtime_monitor.h"   /* lv_event_trace_set_stream_context（事件总线→Stream 桥接 setter） */

/* ---- 依赖前向声明：内置模块的 setter 函数 ---- */

/* 各模块已在对应头文件中声明了 void xxx_set_stream_context(StreamContext *ctx)。
 * 由于 StreamContextSetter 的签名就是 void (*)(StreamContext *)，各 setter
 * 函数与此类型完全兼容，无需强制转换。 */

#include "lv/constraint_graph.h" /* graph_set_stream_context   */
#include "lv/proof.h"            /* proof_set_stream_context      */
#include "lv/runtime_monitor.h"  /* lv_event_trace_set_stream_context（事件总线 → Stream 桥接） */
#include "lv/solver.h"           /* solver_set_stream_context     */

/* interop_set_stream_context 定义于 L5（interop.h），本注册器驻 L4 不允许依赖 L5。
 * 仅需函数符号（void (*)(StreamContext*)），以本地前向声明代替 include，
 * 链接由 lv_static 聚合解析。 */
void interop_set_stream_context(StreamContext *ctx);

#include "lv/func_block.h"    /* func_block_set_stream_context */
#include "lv/normalization.h" /* normalization_set_stream_context*/
#include "lv/prop_verifier.h" /* prop_verifier_set_stream_context*/
#include "lv/recursion.h"     /* recursion_set_stream_context  */
#include "lv/rewrite.h"       /* rewrite_set_stream_context    */
#include "lv/type_system.h"   /* type_system_set_stream_context*/
#include "lv/unify.h"         /* unify_set_stream_context      */

/* ================================================================
 * 全局注册表（委托公共 lv_callback_list 回调列表设施）
 * ================================================================ */

/** @brief 注册表初始容量与硬上限，预留足够空间供未来扩展 */
#define MAX_REGISTERED_SETTERS 32

/** @brief setter 注册表全局单例（lv_once 惰性初始化，运行期只读追加） */
static lvCallbackList s_setter_registry;

/** @brief 注册表单次初始化标记 */
static lv_once_t s_setter_registry_once = lv_ONCE_INIT;

/** @brief 注册表惰性初始化（首次 register/dispatch/clear 时完成） */
static void setter_registry_init_once(void) {
    lv_callback_list_init(&s_setter_registry, MAX_REGISTERED_SETTERS, MAX_REGISTERED_SETTERS);
}

/** @brief 分发调用：将条目中泛型回调转回 StreamContextSetter 签名后调用 */
static void setter_registry_invoke(const lvCallbackEntry *entry, const void *dispatch_arg) {
    StreamContextSetter setter = (StreamContextSetter) entry->callback;
    setter((StreamContext *) dispatch_arg);
}

/* ================================================================
 * 公开 API 实现
 * ================================================================ */

void stream_context_register_setter(StreamContextSetter setter) {
    /* 参数校验：setter 不能为空 */
    if (!setter)
        return;

    lv_once(&s_setter_registry_once, setter_registry_init_once);

    /* 去重检查：避免重复注册同一个 setter */
    for (int i = 0; i < lv_callback_list_count(&s_setter_registry); i++) {
        if (s_setter_registry.entries[i].callback == (lvCallbackFn) setter) {
            return; /* 已注册，跳过 */
        }
    }

    /* 追加到注册表末尾；满时 lv_callback_list_add 返回 -1，静默丢弃（与原行为一致） */
    lv_callback_list_add(&s_setter_registry, (lvCallbackFn) setter, NULL, 0);
}

void stream_context_dispatch_all(StreamContext *ctx) {
    /* 遍历已注册的 setter 回调列表，依次调用（迭代安全，与 stream_dispatch 行为一致） */
    lv_once(&s_setter_registry_once, setter_registry_init_once);
    lv_callback_list_dispatch(&s_setter_registry, ctx, NULL, setter_registry_invoke);
}

/**
 * @brief 清除所有已注册 setter 的全局流式上下文指针为 NULL
 *
 * 在引擎销毁其流式上下文前调用，防止各模块持有悬挂指针。
 * 如果不调用此函数，各模块的 StreamContext 全局变量 (如
 * type_system_stream_ctx、rewrite_stream_ctx 等) 仍指向
 * 已释放的内存，后续使用会导致堆损坏或访问违例。
 */
void stream_context_clear_all(void) {
    /* 以 NULL 参数调用所有已注册 setter（遍历/调用阶段与 dispatch_all 共用分发路径） */
    lv_once(&s_setter_registry_once, setter_registry_init_once);
    lv_callback_list_dispatch(&s_setter_registry, NULL, NULL, setter_registry_invoke);
}

/**
 * @brief 便捷函数: 格式化文本并发射简单流式事件
 *
 * 内部完成「lvStrBuf 组装 + stream_emit_simple + lvStrBuf 销毁」。
 *
 * @param ctx         流式上下文（可为 NULL，此时为空操作）
 * @param type        事件类型
 * @param step_number 步骤编号（各模块可将其视为阶段号 phase）
 * @param fmt         printf 风格格式串
 * @param ...         格式化参数
 */
void stream_emit_fmt(StreamContext *ctx, StreamEventType type, int step_number, const char *fmt, ...) {
    if (!ctx || !fmt)
        return;

    va_list args;
    va_start(args, fmt);

    lvStrBuf sb = {0};
    lv_strbuf_vprintf(&sb, fmt, args);
    va_end(args);

    stream_emit_simple(ctx, type, sb.data, step_number);
    lv_strbuf_destroy(&sb);
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
/** @brief 内置模块 setter 一次性注册回调（lv_once 保证仅执行一次且同步完成） */
static void register_builtins_once(void) {
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

    /* ---- 运行时事件总线（runtime_monitor）→ Stream 桥接 ----
     * engine_create 分发 engine->stream_ctx 时即激活 lv_event_bus 的
     * stream 投射（STREAM_EVENT_BUS_EVENT）；engine 销毁时由
     * stream_context_clear_all 以 NULL 解除，避免悬垂指针。 */
    stream_context_register_setter(lv_event_trace_set_stream_context);

    /* ---- 互操作模块 ---- */
    stream_context_register_setter(interop_set_stream_context);
}

void stream_context_register_builtins(StreamContext *ctx) {
    (void) ctx;
    /* 使用 lv_once 确保只注册一次（线程安全） */
    static lv_once_t s_builtins_once = lv_ONCE_INIT;
    lv_once(&s_builtins_once, register_builtins_once);
}

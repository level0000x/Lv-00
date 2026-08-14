/**
 * @file debug.c
 * @brief 调试工具实现
 * @details 实现日志系统、性能计数器、内存池、引用计数/GC、
 *          紧急保存和追踪会话等调试功能。
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

#include "lv/context.h" /* v3.3.0: 结构化日志需要 lvContext */
#include "lv/debug.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/stream.h"
#include "lv/stream_context_util.h"
#include "lv/type_system.h"
#include "lv/lv_xmacro.h"
#include "lv/lv_strbuf.h"

/* 注：debug 模块无 setter 函数（debug_stream_ctx 由 debug_emergency.c 等直接 extern 引用），
 * 不适用 LV_STREAM_CTX_DEFINE 宏，保留手写。 */
lv_THREAD_LOCAL StreamContext *debug_stream_ctx = NULL;

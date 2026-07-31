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

#include "context.h" /* v3.3.0: 结构化日志需要 lvContext */
#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "stream.h"
#include "stream_context_util.h"
#include "type_system.h"
#include "lv/lv_xmacro.h"
#include "lv/lv_strbuf.h"

lv_THREAD_LOCAL StreamContext *debug_stream_ctx = NULL;

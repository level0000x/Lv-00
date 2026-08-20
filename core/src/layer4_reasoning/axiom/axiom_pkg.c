/**
 * @file axiom_pkg.c
 * @brief 公理系统包实现
 * @details 实现公理包的加载、验证和展开功能。支持约束模板、
 *          不可构造问题检测、双层测试和 SHA-256 依赖追踪。
 */

#include "lv/axiom_pkg.h"
#include "axiom_pkg_internal.h"

#include "lv/lv_file.h"

#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "lv/sha256.h"


#include "lv/debug.h"
#include "lv/error_codes.h"
#include "lv/lexer_shared.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/stream.h"


/* 线程局部存储用于错误消息（使用lv_internal.h中定义的lv_THREAD_LOCAL） */
/* 注：修复（C-㊴ 测试暴露）：setter 名须与头文件公开 API axiom_set_stream_context
 * 一致——原实现名 axiom_pkg_set_stream_context 与 axiom_pkg.h 声明脱节（M5），
 * 导致头文件声明的符号链接失败。变量名 axiom_stream_ctx 保留手写。 */

lv_THREAD_LOCAL StreamContext *axiom_stream_ctx = NULL;

void axiom_set_stream_context(StreamContext *ctx) {
    axiom_stream_ctx = ctx;
}

/* ============== 辅助函数 ============== */

const char *axiom_package_get_last_error(void) {
    return lv_get_last_error_message();
}


